/* Copyright (C) 2012, Chris Morrison <chris-morrison@homemail.com>
 *
 * This file is part of xema.
 *
 * Xema is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Xema is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Xema.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <strings.h>
#include <unistr.h>
#include <popt.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <libpst/libpst.h>
#include <dirent.h>
#include <unistdio.h>
#include <ctype.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <zlib.h>
#include <libolmec.h>
#include "xema_defs.h"
#include "xema.h"
#include "base64.h"
#include "utils.h"
#include "config.h"

static int quiet_mode = false;
static int prefix_mode = PREFIX_NONE;
static int conflict_mode = CONFLICT_RENAME;
static int print_summary = false;
static char dest_dir[BUFF_SIZE];
static char source_path[BUFF_SIZE];
static char current_address[BUFF_SIZE];
static char current_subject[BUFF_SIZE];
static off_t current_file_size;
static struct ctr counter = { 0, 0, 0, 0, 0 ,0, 0, 0, 0 };

int main(int argc, const char **argv)
{
    //FILE *fileptr = NULL;
    int rc = 0;
    int print_ver = 0;
    char *prefix = NULL;
    char *conflict_val = NULL;
    char buffer[BUFF_SIZE];
    const char **arguments = NULL;
    DIR *pDir = NULL;
    struct dirent *dent = NULL;
    char temp_source_path[BUFF_SIZE];
    struct poptOption opts[] =
    {
        {"prefix", 'p', POPT_ARG_STRING, &prefix, 0, "Specifies the information to prepend to the extracted filename", "PREFIX"},
        {"conflict-action", 'c', POPT_ARG_STRING, &conflict_val, 0, "Specifies the action that will be taken if the file being extracted already exist in the destination directory.", "MODE"},
        {"quiet", 'q', POPT_ARG_NONE, &quiet_mode, 0, "Puts xema in quiet mode, no output will be sent to stdout. Errors will still be sent to stderr.", NULL},
	{"version", 'v', POPT_ARG_NONE, &print_ver, 0, "Prints version information and exits.", NULL},
        {"print-summary", 's', POPT_ARG_NONE, &print_summary, 0, "Prints a summary of what has been done once extraction is complete. This option ignores the \'quiet\' option.", NULL},
        POPT_AUTOHELP POPT_TABLEEND
    };
    struct stat st;
        
    poptContext context = poptGetContext(NULL, argc, argv, opts, POPT_CONTEXT_POSIXMEHARDER);
    poptSetOtherOptionHelp(context, "[OPTIONS] FILE DESTINATION");
    
    while ((rc = poptGetNextOpt(context)) > 0)
    {
        if (rc < -1)
        {
            /* an error occurred during option processing */
            fprintf(stderr, "%s: %s\n", poptBadOption(context, POPT_BADOPTION_NOALIAS), poptStrerror(rc));
            return X_FATAL;
        }
    }

    if (print_ver == true)
    {
        print_version();
	return 0;
    }
    
    if (prefix != NULL)
    {
        strncpy(buffer, prefix, BUFF_SIZE);
        dequote_and_tidy_string(buffer);
        if (strcasecmp(buffer, "none") == 0)
        {
            prefix_mode = PREFIX_NONE;
        }
        else if (strcasecmp(buffer, "subject") == 0)
        {
            prefix_mode = PREFIX_SUBJECT;
        }
        else if (strcasecmp(buffer, "sender") == 0)
        {
            prefix_mode = PREFIX_ADDRESS;
        }
        else
        {
            fprintf(stderr, "prefix: Invalid prefix value.\n");
            return X_FATAL;
        }
    }
    else
    {
        prefix_mode = PREFIX_NONE;
    }
    
    if (conflict_val != NULL)
    {
        strncpy(buffer, conflict_val, BUFF_SIZE);
        dequote_and_tidy_string(buffer);
        if (strcasecmp(buffer, "overwrite") == 0)
        {
            conflict_mode = CONFLICT_OVERWRITE;
        }
        else if (strcasecmp(buffer, "rename") == 0)
        {
            conflict_mode = CONFLICT_RENAME;
        }
        else if (strcasecmp(buffer, "discard") == 0)
        {
            conflict_mode = CONFLICT_DISCARD;
        }
        else
        {
            fprintf(stderr, "prefix: Invalid conflict mode value.\n");
            return X_FATAL;
        }
    }
    else
    {
        conflict_mode = CONFLICT_RENAME;
    }
    
    /* Get the remaining arguments */
    arguments = poptGetArgs(context);
    if (arguments == NULL) goto badUsage;
    if (arguments[0] == NULL) goto badUsage;
    if (arguments[1] == NULL) goto badUsage;
    
    /* stat the directory. */
    strncpy(dest_dir, arguments[1], BUFF_SIZE);
    dequote_and_tidy_string(dest_dir);
    if (stat(dest_dir, &st) == -1)
    {
        fprintf(stderr, "ERROR: Could not stat %s: %s.\n", dest_dir, strerror(errno));
        return X_FATAL;
    }
    /* Make sure it is a directory. */
    if (S_ISDIR(st.st_mode) == 0)
    {
        fprintf(stderr, "ERROR: %s is not a directory.\n", dest_dir);
        return X_FATAL;   
    }
    
    /* Now see what the source is. */
    strncpy(source_path, arguments[0], BUFF_SIZE);
    dequote_and_tidy_string(source_path);
    if (stat(source_path, &st) == -1)
    {
        fprintf(stderr, "ERROR: Could not stat %s: %s.\n", source_path, strerror(errno));
        return X_FATAL;
    }
    
    if (S_ISREG(st.st_mode) == true)
    {
        current_file_size = st.st_size;
        rc = do_extraction();   
    }
    else if (S_ISDIR(st.st_mode) == true)
    {
        strncpy(temp_source_path, source_path, BUFF_SIZE);
        pDir = opendir(source_path);
        while ((dent = readdir(pDir)) != NULL)
        {
            if (dent->d_type != DT_REG) continue; /* Ignore anything other than files. */
            snprintf(source_path, BUFF_SIZE, "%s/%s", temp_source_path, dent->d_name);
            if (stat(source_path, &st) == -1)
            {
                fprintf(stderr, "ERROR: Could not stat %s: %s.\n", source_path, strerror(errno));
                return X_FATAL;
            }
            current_file_size = st.st_size;
            rc = do_extraction();
        }
        closedir(pDir);
    }
    else
    {
        fprintf(stderr, "The file %s is not a file that xema can work with.\n", source_path);
        rc = X_FATAL;
    }

    poptFreeContext(context);
    
    /* Print the summary of what was done. */
    if ((counter.message_total > 0) && (print_summary == true)) print_summary_to_screen();
    
    return rc;
    
badUsage:
    
    poptSetOtherOptionHelp(context, "FILE DESTINATION");
    poptPrintUsage(context, stderr, 0);
    poptFreeContext(context);
    
    return X_FATAL;
}

void print_version(void)
{
    printf("%s version %s\nCopyright (C) 2014 Chris Morrison\n", PACKAGE, VERSION);
    printf("License GPLv3+: GNU GPL version 3 or later <https://www.gnu.org/licenses/gpl-3.0.html>\nThis is free software: you are free to change and redistribute it.\nThere is NO WARRANTY, to the extent permitted by law.");
    printf("\n\nWritten by Chris Morrison <chris-morrison@cyberservices.com>\n");
}

int do_extraction(void)
{
    int ret_code = 0;
    
    if (quiet_mode == false) printf("Processing file %s\n", source_path);
    
    counter.files_total++;
    
    if (extract_from_mbox_style_files(&ret_code) == true) return ret_code;
    if (extract_from_pst_file(&ret_code) == true) return ret_code;
    if (extract_from_olm_archive(&ret_code) == true) return ret_code;
    
    fprintf(stderr, "ERROR: The file %s is an unknown or invalid mailbox file.\n", source_path);
    counter.error_total++;
    
    return X_FATAL;
}

/* Extracts the attachments from e-mails on an olm (Entourage) archive.
 * result cannot be NULL.
 */
int extract_from_olm_archive(int *result)
{
    int error = 0;
    olm_file_t *olm;
    olm_mail_message_t *message = NULL;
    olm_attachment_t *attachment = NULL;
    uint64_t message_count = 0;
    uint64_t errors = 0;
    char test[1024];
    
    /* Open the file. */
    olm = olm_open_file(source_path, OLM_OPT_IGNORE_ERRORS, &error);
    if (olm == INVALID_OLM_FILE)
    {
        *result = X_FATAL;
        return false;
    }
    /* Get the message count. */
    message_count = olm_mail_message_count(olm);
    
    for (uint64_t idx1 = 0; idx1 < message_count; idx1++)
    {
        message = olm_get_message_at(olm, idx1, &error);
        if (message != NULL)
        {
            printf("Message ID:     %s\n", message->message_id);
            printf("To:             %s\n", message->to);
            printf("Reply-to:       %s\n", message->reply_to);
            printf("From:           %s\n", message->from);
            printf("Subject:        %s\n", message->subject);
            printf("Attachments:    %lu\n", message->attachment_count);
            printf("Sent date:      %s", ctime(&message->sent_time));
            printf("Modified date:  %s", ctime(&message->modified_time));
            printf("Received date:  %s", ctime(&message->received_time));
            printf("Has HTML:       %d\n", message->has_html);
            printf("Has Rich Test:  %d\n", message->has_rich_text);
            printf("Priority:       %d\n", message->message_priority);
            printf("-----------------------------------------------------\n");
            printf("-----------------------------------------------------\n");
            
            if (message->attachment_count > 0)
            {
                for (uint64_t idx2 = 0; idx2 < message->attachment_count; idx2++)
                {
                    attachment = message->attachment_list[idx2];
                    printf("    Attachment name:          %s\n", attachment->filename);
                    printf("    Attachment extension:     %s\n", attachment->extension);
                    printf("    Attachment size:          %llu\n", attachment->file_size);
                    printf("    Attachment internal name: %s\n", attachment->__private);
                    printf("    -------------------------------------------------\n");
                    printf("    Extracting attachment . . .\n");
                    sprintf(test, "/Users/Chris/OLM_TEST/%s", attachment->filename);
                    if (olm_extract_and_save_attachment(olm, attachment, test) != OLM_ERROR_SUCCESS) exit(1);
                }
            }
            
            olm_message_free(message);
        }
        else
        {
            switch (error)
            {
                case OLM_ERROR_NOT_OLM_FILE:
                    fprintf(stderr, "ERROR: Not an OLM file.");
                    break;
                case OLM_ERROR_FILE_IO_ERROR:
                    fprintf(stderr, "ERROR: An IO error occurred.");
                    break;
                case OLM_ERROR_FILE_CORRUPTED:
                    fprintf(stderr, "ERROR: The file is invalid, corrupted or damaged.");
                    break;
                case OLM_ERROR_NO_MEMORY:
                    fprintf(stderr, "ERROR: There is not enough free memory to carry out this operation.");
                    break;
                case OLM_ERROR_INVALID_FILE_HANDLE:
                    fprintf(stderr, "ERROR: The archive handle is not valid.");
                    break;
                case OLM_ERROR_MESSAGE_CORRUPTED:
                    fprintf(stderr, "ERROR: The message is invalid, corrupted or damaged.");
                    break;
                case OLM_ERROR_ATTACHMENT_CORRUPTED:
                    fprintf(stderr, "ERROR: The attachment is invalid, corrupted or damaged..");
                    break;
                default:
                    break;
            }
            fprintf(stderr, " In message at index %llu\n", idx1);
            ++errors;
        }
    }
    
    printf("Completed with %llu errors and warnings.\n", errors);
    
    olm_close_file(olm);
    
    return true;
}

/* Extracts the attachments from an email message file (e.g. eml, emlx, mbox etc.)
 * result cannot be NULL.
 */
int extract_from_mbox_style_files(int *result)
{
    FILE *fp = NULL;
    //int header_found = false;
    char buffer[BUFF_SIZE];
    memset(current_address, 0, BUFF_SIZE);
    memset(current_subject, 0, BUFF_SIZE);
    int trans_encoding_found = false;
    int filename_found = false;
    int entry_is_attachment = false;
    char attachment_filename[BUFF_SIZE];
    char encoding[100];
    char save_file_path[BUFF_SIZE];
    FILE *cfp = NULL;
    unsigned long atts_this_file = 0;
    unsigned int header_found_flag = 0;
    memset(buffer, 0, BUFF_SIZE);
    int ret_val = 0;
    char type_hint[1024];
    
    /* Open the file. */
    fp = fopen(source_path, "r");
    /* Be prepared for race conditions and errors. */
    if (fp == NULL)
    {
        fprintf(stderr, "ERROR: Could not open the source file: %s\n", strerror(errno));
        counter.error_total++;
        *result = X_FATAL;
        return false;
    }
    
    /* Sniff for a header to make sure that this is a valid e-mail message or box. */
    if (sniff_for_eml_header(fp, type_hint) == false)
    {
        *result = X_FATAL;
        return false;
    }
    
    /* If we got here a header was found. */
    if (quiet_mode == false) printf("---> File identified as an %s...\n", type_hint);
    
    /* Now go through the file looking for headers and attachments. */
    while ((feof(fp) == 0) && (ferror(fp) == 0))
    {
        filename_found = false;
        trans_encoding_found = false;
        entry_is_attachment = false;
        
        /***********************************************************************************************
         * Look for a header.
         ***********************************************************************************************/
        
        if (read_folded_line_from_fp(buffer, BUFF_SIZE, fp) == NULL)
        {
            fprintf(stderr, "ERROR: An unknown I/O error occurred.\n");
            counter.error_total++;
            *result = X_FATAL;
            return false;
        }
        if (strncasecmp(buffer, "from: ", 6) == 0)
        {
            header_found_flag |= 1;
            strncpy(current_address, (buffer + 6), BUFF_SIZE);
            if (current_address[0] == '\0')
            {
                strcpy(current_address, NO_ADDRESS);
            }
            else
            {
                dequote_and_tidy_string(current_address);
                extract_address_from_sender(current_address);
            }
        }
        if (strncasecmp(buffer, "date: ", 6) == 0)
        {
            header_found_flag |= 2;
        }
        if (strncasecmp(buffer, "message-id: ", 12) == 0)
        {
            header_found_flag |= 4;
        }
        if (strncasecmp(buffer, "to: ", 4) == 0)
        {
            header_found_flag |= 8;
        }
        if (strncasecmp(buffer, "subject: ", 9) == 0)
        {
            header_found_flag |= 16;
            strncpy(current_subject, (buffer + 9), BUFF_SIZE);
            if (current_subject[0] == '\0')
            {
                strcpy(current_subject, NO_SUBJECT);
            }
            else
            {
                ret_val = decode_base64_subject_lines(current_subject);
                if (ret_val == NOT_B64_ENCODED) ret_val = decode_quoted_printable_subject_lines(current_subject);
                if (ret_val == DECODE_ERROR) strcpy(current_subject, NO_SUBJECT);
            }
        }

        /* Another possible source of an address. */
        if ((strncasecmp(buffer, "return-path: ", 13) == 0) && (strcmp(current_address, NO_ADDRESS) == 0))
        {
            strncpy(current_address, (buffer + 13), BUFF_SIZE);
            if (current_address[0] == '\0')
            {
                dequote_and_tidy_string(current_address);
                extract_address_from_sender(current_address);
            }
        }
        if ((strncasecmp(buffer, "reply-to: ", 10) == 0) && (strcmp(current_address, NO_ADDRESS) == 0))
        {
            strncpy(current_address, (buffer + 10), BUFF_SIZE);
            if (current_address[0] == '\0')
            {
                dequote_and_tidy_string(current_address);
                extract_address_from_sender(current_address);
            }
        }
        if ((strncasecmp(buffer, "x-original-to: ", 15) == 0) && (strcmp(current_address, NO_ADDRESS) == 0))
        {
            strncpy(current_address, (buffer + 15), BUFF_SIZE);
            if (current_address[0] == '\0')
            {
                dequote_and_tidy_string(current_address);
                extract_address_from_sender(current_address);
            }
        }
        if ((strncasecmp(buffer, "sender: ", 8) == 0) && (strcmp(current_address, NO_ADDRESS) == 0))
        {
            strncpy(current_address, (buffer + 8), BUFF_SIZE);
            if (current_address[0] == '\0')
            {
                dequote_and_tidy_string(current_address);
                extract_address_from_sender(current_address);
            }
        }

        if (header_found_flag >= 0x07)
        {
            ++counter.message_total;
            header_found_flag = 0;
        }
       
        /***********************************************************************************************
         * Look for an attachments.
         ***********************************************************************************************/
        
        /* Apple Mail attachments will take the form of:
         *
         * Content-Disposition: inline;filename=filename
         * Content-Type: image/jpeg;name=filename
         * Content-Transfer-Encoding: base64
         *
         * -or-
         *
         * Content-type: image/jpeg; name="iPhone1.jpg";x-mac-type="4A504547"
         * Content-disposition: attachment; filename="iPhone1.jpg"
         * Content-transfer-encoding: base64
         *
         * -or-
         *
         * Content-Type: image/jpeg;name="unknown.jpg"
         * Content-Transfer-Encoding: base64
         * Content-ID: XXXXX
         *
         * Thunderbird takes the form of:
         *
         * Content-type: application/vnd.openxmlformats-officedocument.wordprocessingml.document; name="Pay and Olympics March 2012.docx"
         * Content-disposition: attachment; filename="Pay and Olympics March 2012.docx"
         * Content-transfer-encoding: base64
         *
         * Outlook message source:
         *
         * Content-Type: application/zip; name="Visual Studio.zip"
         * Content-Transfer-Encoding: base64
         * Content-Disposition: attachment; filename="Visual Studio.zip"
         *
         * Windows Mail
         *
         * Content-Type: image/jpeg; name="logo.jpg"
         * Content-Transfer-Encoding: base64
         * Content-ID: <image_0001>
         * Content-Disposition: inline
         *
         */
        
        if ((strncasecmp(buffer, "content-disposition: ", 21) == 0) || (strncasecmp(buffer, "content-type: ", 14) == 0))
        {
            char mime_type[265];
            char *namestart = NULL;
            ssize_t semicolon = 0;
            ssize_t whitespace = 0;
            if (strncasecmp(buffer, "content-type: ", 14) == 0)
            {
                strcpy(mime_type, (buffer + 14));
                semicolon = first_index_of(mime_type, ';');
                if (semicolon != -1) mime_type[semicolon] = '\0';
                whitespace = first_index_of(mime_type, ' ');
                if (whitespace != -1) mime_type[whitespace] = '\0';
                if (strcasecmp(mime_type, "application/applefile") == 0)
                {
                    continue;
                }
            }
            if ((namestart = strcasestr(buffer, "filename=")) != NULL)
            {
                /* Chop any other tokens off the end of the string they will very unlikley be any good to us. */
                semicolon = first_index_of(namestart, ';');
                if (semicolon != -1) namestart[semicolon] = '\0';
                filename_found = true;
                strcpy(attachment_filename, (namestart + 9));
                dequote_and_tidy_string(attachment_filename);
            }
            else if ((namestart = strcasestr(buffer, "name=")) != NULL)
            {
                /* Chop any other tokens off the end of the string they will very unlikley be any good to us. */
                semicolon = first_index_of(namestart, ';');
                if (semicolon != -1) namestart[semicolon] = '\0';
                filename_found = true;
                strcpy(attachment_filename, (namestart + 5));
                dequote_and_tidy_string(attachment_filename);
            }
            else
            {
                continue;
            }
            
            entry_is_attachment = true;
            
            while ((feof(fp) == 0) && (ferror(fp) == 0))
            {
                if (read_string_from_fp(buffer, BUFF_SIZE, fp) == NULL)
                {
                    fprintf(stderr, "ERROR: An unknown I/O error occurred.\n");
                    counter.error_total++;
                    *result = X_FATAL;
                    return false;
                }
                if (buffer[0] == '\0') break;
                if (strncasecmp(buffer, "content-transfer-encoding: ", 27) == 0)
                {
                    trans_encoding_found = true;
                    strcpy(encoding, (buffer + 27));
                }
            }
        }
        
        /**********************************************************************************************************************************************
         * By this point if there is an attachment we should have all the information about it and the file pointer should be pointing to the first   *
         * line of the encoded attachment data.                                                                                                       *
         **********************************************************************************************************************************************/
        
        if ((trans_encoding_found == true) && (filename_found == true) && (entry_is_attachment == true))
        {
            ++counter.attachment_total;
            ++atts_this_file;
            
            /* Build the pathname. */
            if (build_output_path(attachment_filename, BUFF_SIZE, save_file_path, false) == NULL)
            {
                fprintf(stderr, "ERROR: Failed to build output path.\n");
                counter.error_total++;
                *result = X_FATAL;
                return false;
            }
            
            if (quiet_mode == false) printf("---> Extracting %s from %s:%s\n", save_file_path, current_address, current_subject);
            
            /* Check if the file exists. */
            if (file_exists(save_file_path) == true)
            {
                if (quiet_mode == false) printf("WARNING: The file %s already exists and was ", save_file_path);
                counter.warning_total++;
                switch (conflict_mode)
                {
                    case CONFLICT_OVERWRITE:
                        if (quiet_mode == false) printf("overwritten\n");
                        /* Do nothing go straight to writing the file. */
                        break;
                    case CONFLICT_DISCARD:
                        if (quiet_mode == false) printf("discarded\n");
                        /* Increment to the next item in the linked list and continue the loop. */
                        ++counter.discardrd_total;
                        continue;
                        break;
                    case CONFLICT_RENAME:
                        /* Rename the destination file before writing. */
                        if (build_output_path(attachment_filename, BUFF_SIZE, save_file_path, true) == NULL)
                        {
                            fprintf(stderr, "\rERROR: Failed to build output path.\n");
                            counter.error_total++;
                            *result = X_FATAL;
                            return false;
                        }
                        if (quiet_mode == false) printf("renamed to %s\n", save_file_path);
                        ++counter.renamed_total;
                        break;
                }
            }
            
            /* Now try to extract the file. */
            cfp = fopen(save_file_path, "w+");
            if (cfp == NULL)
            {
                if (quiet_mode == false) printf("WARNING: Could not create %s: %s\n", save_file_path, strerror(errno));
                ++counter.warning_total;
            }
            else
            {
                /* Now extract the data. */
                if (strcasecmp(encoding, "base64") == 0)
                {
                    if (decode_and_write_base64_data(fp, cfp, NULL) == false)
                    {
                        /* An error message will already have been displayed ny the function. */
                        fclose(cfp);
                        unlink(save_file_path);
                        continue;
                    }
                }
                else
                {
                    if (quiet_mode == false) printf("WARNING: Could not extract %s: unsupported content transfer encoding\n", attachment_filename);
                    ++counter.warning_total;
                    fclose(cfp);
                    unlink(save_file_path);
                    continue;
                }
                ++counter.extracted_total;
                if (conflict_mode == CONFLICT_OVERWRITE) ++counter.overwritten_total;
                fclose(cfp);
            }
        }
    }
    
    if (ferror(fp) != 0)
    {
        fprintf(stderr, "An unknown error occurred while reading data from the file.\n");
        counter.error_total++;
        *result = X_FATAL;
        return false;
    }
    
    if ((atts_this_file == 0) && (quiet_mode == false)) printf("---> No attachments found.\n");
    
    fclose(fp);
    *result = X_SUCCESS;
    
    return true;
}

/* Read Base64 encoded lines from source_fileptr decodes them and writes them to dest_fileptr.
 *
 * Reading will stop when a blank line on an e-mail boundary is encountered.
 *
 * If bytes_read is not NULL it will contain the total size of the decoded data upon return.
 * The contents of bytes_read will have no meaning if this function return false.
 */
int decode_and_write_base64_data(FILE *source_fileptr, FILE *dest_fileptr, size_t *bytes_read)
{
    char line_read[BUFF_SIZE];
    size_t decoded_data_len = 0;
    size_t encoded_data_len = 0;
    size_t br = 0;
    
    if ((source_fileptr == NULL) || (dest_fileptr == NULL)) return false;
    
    for (;;)
    {
        if (ferror(source_fileptr) != 0)
        {
            if (quiet_mode == false) printf("WARNING: Could not read attachment from message file: %s.\n", strerror(errno));
            counter.warning_total++;
            return false;
        }
        read_string_from_fp(line_read, BUFF_SIZE, source_fileptr);
        if (line_read[0] == '\0') break;
        if (strncmp(line_read, "--", 2) == 0) break; /* Ignore boundaries. */
        encoded_data_len = strlen(line_read);
        decoded_data_len = decode_base64((unsigned char *)line_read, line_read);
        br += decoded_data_len;
        if ((decoded_data_len == 0) && (encoded_data_len != 0))
        {
            if (quiet_mode == false) printf("WARNING: Could not extract the attacment data: an unknown decoding error occurred.\n");
            counter.warning_total++;
            return false;
        }
        fwrite(line_read, decoded_data_len, 1, dest_fileptr);
        if (ferror(dest_fileptr) != 0)
        {
            if (quiet_mode == false) printf("WARNING: Could not write to attachment file: %s.\n", strerror(errno));
            counter.warning_total++;
            return false;
        }
    }
    
    if (br == 0)
    {
        if (quiet_mode == false) printf("WARNING: An attachment was detected but no data was found.\n");
        counter.warning_total++;
        return false;
    }
    
    if (bytes_read != NULL) *bytes_read = br;
    
    return true;
}

/* Sniffs the first 10K of the given file for an e-mail header and returns true if one is found.
 */
int sniff_for_eml_header(FILE *fp, char *hint)
{
    char buffer[BUFF_SIZE];
    unsigned int header_found_flag = 0;
    memset(buffer, 0, BUFF_SIZE);
    int ret_val = 0;
    olkheader olk_header = {0, 0, 0, 0 ,0};
    size_t olk_header_size = sizeof(olk_header);
    size_t olk_data_size = ((size_t)current_file_size - olk_header_size);
    uint32_t crc = (uint32_t)crc32(0L, Z_NULL, 0);
    size_t bytes_read = 0;
    
    /* Rewind the file pointer. */
    fseeko(fp, 0, SEEK_SET);
    
    /* First try to read out a Microsoft Outlook message source file header. */
    fread(&olk_header, olk_header_size, 1, fp);
    olk_header.magic = ntohl(olk_header.magic);
    olk_header.size1 = ntohl(olk_header.size1);
    olk_header.size2 = ntohl(olk_header.size2);
    olk_header.crc32 = ntohl(olk_header.crc32);
    if (olk_header.magic == 0x4D537263)
    {
        strcpy(hint, "Outlook message source file");
        if (olk_data_size != olk_header.size1)
        {
            fprintf(stderr, "ERROR: This e-mail message is corrupted or damaged.\n");
            counter.error_total++;
            return false;
        }
        /* CRC check the file. */
        while (feof(fp) == 0)
        {
            bytes_read = fread(buffer, 1, BUFF_SIZE, fp);
            crc = (uint32_t)crc32(crc, (unsigned char *)buffer, bytes_read);
        }
        if (crc != olk_header.crc32)
        {
            fprintf(stderr, "ERROR: This e-mail message is corrupted or damaged.\n");
            counter.error_total++;
            return false;
        }
        
        fseeko(fp, (off_t)olk_header_size, SEEK_SET);
    }
    else
    {
        strcpy(hint, "RFC 5322 type message or mailbox");
    }
    
    /* Initialize the global variables. */
    strcpy(current_address, NO_ADDRESS);
    strcpy(current_subject, NO_SUBJECT);
    
    while ((feof(fp) == 0) && (ferror(fp) == 0) && (header_found_flag != 0x1F) && (ftello(fp) < 10240))
    {       
        if (read_folded_line_from_fp(buffer, BUFF_SIZE, fp) == NULL)
        {
            fprintf(stderr, "ERROR: An unknown I/O error occurred.\n");
            counter.error_total++;
            return false;
        }
        
        /* A blank line should indicate that we have reached the end of the header. */
        if (buffer[0] == '\0') break;
        
        if (strncasecmp(buffer, "from: ", 6) == 0)
        {
            header_found_flag |= 1;
            strncpy(current_address, (buffer + 6), BUFF_SIZE);
            if (current_address[0] == '\0')
            {
                strcpy(current_address, NO_ADDRESS);
            }
            else
            {
                dequote_and_tidy_string(current_address);
                extract_address_from_sender(current_address);
            }
        }
        if (strncasecmp(buffer, "date: ", 6) == 0)
        {
            header_found_flag |= 2;
        }
        if (strncasecmp(buffer, "message-id: ", 12) == 0)
        {
            header_found_flag |= 4;
        }
        if (strncasecmp(buffer, "to: ", 4) == 0)
        {
            header_found_flag |= 8;
        }
        if (strncasecmp(buffer, "subject: ", 9) == 0)
        {
            header_found_flag |= 16;
            strncpy(current_subject, (buffer + 9), BUFF_SIZE);
            if (current_subject[0] == '\0')
            {
                strcpy(current_subject, NO_SUBJECT);
            }
            else
            {
                ret_val = decode_base64_subject_lines(current_subject);
                if (ret_val == NOT_B64_ENCODED) ret_val = decode_quoted_printable_subject_lines(current_subject);
                if (ret_val == DECODE_ERROR) strcpy(current_subject, NO_SUBJECT);
            }
        }
        /* Another possible source of an address. */
        if ((strncasecmp(buffer, "return-path: ", 13) == 0) && (strcmp(current_address, NO_ADDRESS) == 0))
        {
            strncpy(current_address, (buffer + 13), BUFF_SIZE);
            if (current_address[0] == '\0')
            {
                dequote_and_tidy_string(current_address);
                extract_address_from_sender(current_address);
            }
        }
        if ((strncasecmp(buffer, "reply-to: ", 10) == 0) && (strcmp(current_address, NO_ADDRESS) == 0))
        {
            strncpy(current_address, (buffer + 10), BUFF_SIZE);
            if (current_address[0] == '\0')
            {
                dequote_and_tidy_string(current_address);
                extract_address_from_sender(current_address);
            }
        }
        if ((strncasecmp(buffer, "x-original-to: ", 15) == 0) && (strcmp(current_address, NO_ADDRESS) == 0))
        {
            strncpy(current_address, (buffer + 15), BUFF_SIZE);
            if (current_address[0] == '\0')
            {
                dequote_and_tidy_string(current_address);
                extract_address_from_sender(current_address);
            }
        }
        if ((strncasecmp(buffer, "sender: ", 8) == 0) && (strcmp(current_address, NO_ADDRESS) == 0))
        {
            strncpy(current_address, (buffer + 8), BUFF_SIZE);
            if (current_address[0] == '\0')
            {
                dequote_and_tidy_string(current_address);
                extract_address_from_sender(current_address);
            }
        }
    }
    
    /* Rewind the file pointer. */
    fseeko(fp, 0, SEEK_SET);
    
    if (header_found_flag >= 0x07)
    {
        return true;
    }
    
    return false;
}

/* Extracts the attachments from an Outlook pst file.
 * result cannot be NULL.
 */
int extract_from_pst_file(int *result)
{
    pst_file pstf;
    pst_item *root = NULL;
    pst_desc_tree *top_of_folders;
    
    /* Try to open the file as a pst file. */
    if (pst_open(&pstf, source_path, NULL) == -1)
    {
        *result = X_FATAL;
        return false;
    }
    if (pst_load_index(&pstf) == -1)
    {
        pst_close(&pstf);
        *result = X_FATAL;
        return false;
    }
    if (pst_load_extended_attributes(&pstf) == -1)
    {
        pst_close(&pstf);
        *result = X_FATAL;
        return false;
    }
    root = pst_parse_item(&pstf, pstf.d_head, NULL);
    if ((root == NULL) || (root->message_store == NULL))
    {
        pst_close(&pstf);
        *result = X_FATAL;
        return false;
    }
    top_of_folders = pst_getTopOfFolders(&pstf, root);
    if (top_of_folders == NULL)
    {
        pst_close(&pstf);
        *result = X_FATAL;
        return false;
    }
    
    if (quiet_mode == false) printf("---> File identified as Outlook PST file...\n");
    
    
    /* Process the topmost item. */
    *result = process_pst_item(&pstf, root, top_of_folders->child);
    
    pst_freeItem(root);
    pst_close(&pstf);
    
    if ((counter.attachment_total == 0) && (quiet_mode == false)) printf("---> No attachments found.\n");
    
    if (*result == X_FATAL) return false;

    return true;
}

void print_summary_to_screen(void)
{
    printf("\n        Total files processed: %'llu\n", counter.files_total);
    printf("         Total messages found: %'llu\n", counter.message_total);
    printf("      Total attachments found: %'llu\n", counter.attachment_total);
    printf("  Total attachments extracted: %'llu\n", counter.extracted_total);
    printf("  Total attachments discarded: %'llu\n", counter.discardrd_total);
    printf("    Total attachments renamed: %'llu\n", counter.renamed_total);
    printf("Total attachments overwritten: %'llu\n", counter.overwritten_total);
    printf("               Total warnings: %'llu\n", counter.warning_total);
    printf("                 Total errors: %'llu\n", counter.error_total);
}

/* Walks a pst_desc_tree and extracts any attachments from any e-mails it may contain. 
 * This function is recursivley called on any folders found. All other items are ignored.
 */
int process_pst_item(pst_file *pf, pst_item *outeritem, pst_desc_tree *d_ptr)
{
    pst_item *item = NULL;
    pst_item_attach *attachment = NULL;
    memset(current_address, 0, BUFF_SIZE);
    memset(current_subject, 0, BUFF_SIZE);
    char attachment_filename[BUFF_SIZE];
    char save_file_path[BUFF_SIZE];
    FILE *cfp = NULL;
    
    while (d_ptr)
    {
        /* Skip NULL items. */
        if (d_ptr->desc != NULL)
        {
            item = pst_parse_item(pf, d_ptr, NULL);
            
            if ((item->folder != NULL) && (d_ptr->child != NULL))
            {
                // If this is a folder, we want to recurse into it
                pst_convert_utf8(item, &item->file_as);
                if (quiet_mode == false) printf("Processing mail folder %s...\n", item->file_as.str);
                process_pst_item(pf, item, d_ptr->child);
            }
            
            /* Increment the e-mail counter. */
            if (item->email != NULL) ++counter.message_total;
            
            if ((item->email != NULL) && (item->attach != NULL) && ((item->flags & PST_FLAG_HAS_ATTACHMENT) == PST_FLAG_HAS_ATTACHMENT))
            {
                /* Get an e-mail address for the sender. */
                if (item->email->sender_address.str != NULL)
                {
                    pst_convert_utf8(item, &item->email->sender_address);
                    strncpy(current_address, item->email->sender_address.str, BUFF_SIZE);
                }
                else if (item->email->reply_to.str != NULL)
                {
                    pst_convert_utf8(item, &item->email->reply_to);
                    strncpy(current_address, item->email->reply_to.str, BUFF_SIZE);
                }
                else
                {
                    strcpy(current_address, NO_ADDRESS);
                }
                
                /* Get the m-mail subject. */
                if (item->subject.str != NULL)
                {
                    pst_convert_utf8(item, &item->subject);
                    strncpy(current_subject, item->subject.str, BUFF_SIZE);
                }
                else
                {
                    strcpy(current_subject, NO_SUBJECT);
                }
                
                /* Now get the attachments. */
                attachment = item->attach;
                while (attachment)
                {
                    /* Make sure that threre is an attachment. */
                    if (attachment->method != PST_ATTACH_NONE)
                    {
                        /* Increments the attachments counter. */
                        ++counter.attachment_total;
                        
                        /* Create the filename to which this attachment will be saved. */
                        if (attachment->filename2.str != NULL)
                        {
                            pst_convert_utf8(item, &attachment->filename2);
                            strncpy(attachment_filename, attachment->filename2.str, BUFF_SIZE);
                        } 
                        else if (attachment->filename1.str != NULL)
                        {
                            pst_convert_utf8(item, &attachment->filename1);
                            strncpy(attachment_filename, attachment->filename1.str, BUFF_SIZE);
                        } 
                        else
                        {
                            /* If no filename could be found (unlikley) use a UUID. */
                            generate_uuid(attachment_filename, UUID_OPTS_BARE);
                        }
                        
                        /* Build the pathname. */
                        if (build_output_path(attachment_filename, BUFF_SIZE, save_file_path, false) == NULL)
                        {
                            fprintf(stderr, "ERROR: Failed to build output path.\n");
                            counter.error_total++;
                            return X_FATAL;
                        }
                        
                        if (quiet_mode == false) printf("---> Extracting %s from %s:%s\n", save_file_path, current_address, current_subject);
                        
                        /* Check if the file exists. */
                        if (file_exists(save_file_path) == true)
                        {
                            if (quiet_mode == false) printf("WARNING: The file %s already exists and was ", save_file_path);
                            counter.warning_total++;
                            switch (conflict_mode)
                            {
                                case CONFLICT_OVERWRITE:
                                    if (quiet_mode == false) printf("overwritten\n");
                                    /* Do nothing go straight to writing the file. */
                                    break;
                                case CONFLICT_DISCARD:
                                    if (quiet_mode == false) printf("discarded\n");
                                    /* Increment to the next item in the linked list and continue the loop. */
                                    ++counter.discardrd_total;
                                    attachment = attachment->next;
                                    continue;
                                    break;
                                case CONFLICT_RENAME:
                                    /* Rename the destination file before writing. */
                                    if (build_output_path(attachment_filename, BUFF_SIZE, save_file_path, true) == NULL)
                                    {
                                        fprintf(stderr, "\rERROR: Failed to build output path.\n");
                                        counter.error_total++;
                                        return X_FATAL;
                                    }
                                    if (quiet_mode == false) printf("renamed to %s\n", save_file_path);
                                    ++counter.renamed_total;
                                    break;
                            }
                        }
                        
                        /* Now try to extract the file. */
                        cfp = fopen(save_file_path, "w+");
                        if (cfp == NULL)
                        {
                            if (quiet_mode == false) printf("WARNING: Could not create %s: %s\n", save_file_path, strerror(errno));
                            counter.warning_total++;
                        }
                        else
                        {
                            pst_attach_to_file(pf, attachment, cfp);
                            fclose(cfp);
                            ++counter.extracted_total;
                            if (conflict_mode == CONFLICT_OVERWRITE) ++counter.overwritten_total;
                        }
                    }
                    
                    attachment = attachment->next;
                }
            }
            
            /* Clean up. */
            pst_freeItem(item);
        }
        
        d_ptr = d_ptr->next;
    }
    
    if ((counter.error_total > 0) && (counter.extracted_total > 0)) return X_NON_FATAL;
    if ((counter.error_total > 0) && (counter.extracted_total == 0)) return X_FATAL;
    
    return X_SUCCESS;
}

/* Builds the full path to which an attachment, whose filename is given, will be saved.
 * if filename is NULL a UUID will be used, buffer cannot be NULL.
 * buffer is assumed to be BUFF_SIZE bytes.
 */
char *build_output_path(const char *filename, size_t filename_length, char *buffer, int do_rename)
{
    uint32_t utf32_filename[BUFF_SIZE];
    uint32_t utf32_subject[BUFF_SIZE];
    uint32_t utf32_sender[BUFF_SIZE];
    uint32_t wtemp_buffer[BUFF_SIZE];
    uint32_t wtemp_buffer1[BUFF_SIZE];
    uint32_t utf32_dest_dir[BUFF_SIZE];
    size_t buff_len = 0;
    uint32_t ext_cpy[100];
    uint32_t *extension = NULL;
    size_t flen = 0;
    
    if (buffer == NULL) return NULL;
    
    /* Convert the filename to UTF-32. */
    memset(utf32_filename, 0, sizeof(utf32_filename));
    buff_len = ARRAYSIZE(utf32_filename);
    if (u8_to_u32((const uint8_t *)filename, filename_length, utf32_filename, &buff_len) == NULL)
    {
        /* If we could not convert the filename (unlikley) then generate a UUID. */
        generate_uuid_utf32(utf32_filename, UUID_OPTS_BARE);
    }
    
    /* Convert the sender to UTF-32. */
    memset(utf32_sender, 0, sizeof(utf32_sender));
    buff_len = ARRAYSIZE(utf32_sender);
    if (u8_to_u32((uint8_t *)current_address, sizeof(current_address), utf32_sender, &buff_len) == NULL)
    {
        /* If we could not convert the filename (unlikley) then generate a UUID. */
        generate_uuid_utf32(utf32_sender, UUID_OPTS_BARE);
    }

    /* Convert the subject to UTF-32. */
    memset(utf32_subject, 0, sizeof(utf32_subject));
    buff_len = ARRAYSIZE(utf32_subject);
    if (u8_to_u32((uint8_t *)current_subject, sizeof(current_subject), utf32_subject, &buff_len) == NULL)
    {
        /* If we could not convert the filename (unlikley) then generate a UUID. */
        generate_uuid_utf32(utf32_subject, UUID_OPTS_BARE);
    }
    
    /* Convert the dest diriectory to UTF-32 for consistency and to perform further checks in its validity. */
    memset(utf32_dest_dir, 0, sizeof(utf32_dest_dir));
    buff_len = ARRAYSIZE(utf32_dest_dir);
    if (u8_to_u32((uint8_t *)dest_dir, sizeof(dest_dir), utf32_dest_dir, &buff_len) == NULL) return NULL;

    canonicalize_filename(utf32_filename);
    canonicalize_filename(utf32_sender);
    canonicalize_filename(utf32_subject);
    
    /* Check if we are doing a rename. */
    if (do_rename == true)
    {
        memset(ext_cpy, 0, sizeof(ext_cpy));
        memset(wtemp_buffer, 0, sizeof(wtemp_buffer));
        memset(wtemp_buffer1, 0, sizeof(wtemp_buffer1));
        /* Check if the fileame is a UUID, if it is replace it and carry on. */
        if (replace_uuid_utf32(utf32_filename) == true)
        {
            u32_strncpy(wtemp_buffer, utf32_filename, BUFF_SIZE);
            goto convertBack;
        }
        
        /* Otherwise lop of the extension and tumble the base filename. */
        extension = u32_strrchr(utf32_filename, '.'); /* Get a pointer to the extension. */
        if (extension != NULL)
        {
            u32_strncpy(ext_cpy, extension, ARRAYSIZE(ext_cpy)); /* make a copy of the extension string. */
            extension[0] = '\0'; /* Lop the extension off the end of the filename. */
        }
        
        /* Now generate a UUID and 'mix it' with the filename. */
        generate_uuid_utf32(wtemp_buffer, UUID_OPTS_UNDERSCORES);
        generate_uuid_utf32(wtemp_buffer1, UUID_OPTS_BARE | UUID_OPTS_USE_RANDOM);
        u32_cpy((wtemp_buffer + 9), wtemp_buffer1, 32);
        flen = u32_strlen(utf32_filename);
        /* Do a bit of tidying up on the randomized filename. */
        if (flen == 31) wtemp_buffer[41] = '\0'; /* No point in having a odd digit at the end of the filename. */
        if ((flen >= 1) && (flen <= 30)) wtemp_buffer[flen + 9] = '_'; /* put an underscore after the actual name to seperate it from the last bit of the UUID. */
        u32_cpy((wtemp_buffer + 9), utf32_filename, flen);
        
    convertBack:
        u32_snprintf(utf32_filename, BUFF_SIZE, "%llU%llU", wtemp_buffer, ext_cpy);
    }
    
    memset(buffer, 0, BUFF_SIZE);
    memset(wtemp_buffer, 0, sizeof(wtemp_buffer));
    
    switch (prefix_mode)
    {
        case PREFIX_SUBJECT:
            u32_snprintf(wtemp_buffer, BUFF_SIZE, "%llU/%llU_%llU", utf32_dest_dir, utf32_subject, utf32_filename);
            break;
        case PREFIX_ADDRESS:
            u32_snprintf(wtemp_buffer, BUFF_SIZE, "%llU/%llU_%llU", utf32_dest_dir, utf32_sender, utf32_filename);
            break;
        default:
            u32_snprintf(wtemp_buffer, BUFF_SIZE, "%llU/%llU", utf32_dest_dir, utf32_filename);
            break;
    }
    
    buff_len = BUFF_SIZE;
    u32_to_u8(wtemp_buffer, BUFF_SIZE, (uint8_t *)buffer, &buff_len);
    
    return buffer;
}










