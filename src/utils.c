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
 * Quoted printable decoding support derived from code by 
 * Henrik Storner <storner@image.dk>
 */

#include "xema_defs.h"
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>
#include <wordexp.h>
#if defined (__linux__)
#include <bsd/string.h>
#endif
#include <uuid/uuid.h>
#include <errno.h>
#include <unistr.h>
#include <unictype.h>
#include <unicase.h>
#include "base64.h"
#include "utils.h"

/* This function is identical to fgets() provided by the standard C library except that it will stop reading when it
 * encounters:-
 * 
 * A newline (\n).
 * A form feed (\r).
 * A null terminator (\0).
 * A semi-colon
 *
 * It will also remove any leading or trailing spaces, tabs, newlines or form feeds from the returned string.
 */
char *read_token_from_fp(char *buff, size_t buff_len, FILE *fp)
{
    if (buff_len == 0) return NULL;
    if (buff == NULL) return NULL;
    if (fp == NULL) return NULL;
    memset(buff, 0, buff_len);
    char ch = 0;
    size_t end = buff_len - 1; /* Read one less character than the length of the buffer so that there will always be a terminating null. */
    
    for (size_t idx = 0; idx < end; ++idx)
    {
        if (feof(fp) != 0) break;
        if (ferror(fp) != 0) return NULL;
        ch = (char)getc(fp);
        if ((ch == '\r') || (ch == '\n') || (ch == '\0') || (ch == ';'))
        {
            /* If ch == \r then it is possible that there could be a \n still left in the stream (if the file uses Micr$oft Windoze line endings), this will trip up the next read from the stream. */
            if (ch == '\r')
            {
                ch = (char)getc(fp); /* Get the next character. */
                if (ch != '\n') ungetc((int)ch, fp); /* If it is not a \n then put it back. */
            }
            /* If ch == ';' then there could be a \r and or \n left in the stream that will have to be consumed to prevent the next read from failing. */
            if (ch == ';')
            {
                ch = (char)getc(fp); /* Get the next character. */
                if ((ch != '\r') && (ch != '\n')) ungetc((int)ch, fp); /* If it is not a \n then put it back. */
                if (ch == '\r') /* If the next character is \r then there could be a \n after it. */
                {
                    ch = (char)getc(fp); /* Get the next character. */
                    if (ch != '\n') ungetc((int)ch, fp); /* If it is not a \n then put it back. */
                }
            }
            buff[idx] = '\0';
            break;
        }
        else
        {
            buff[idx] = ch;
        }
    }
    
    if (buff[0] != '\0')
    {
        /* Remove all leading spaces and tabs and newlines etc. */
        while ((buff[0] == '\t') || (buff[0] == ' '))
        {
            memmove(buff, (buff + 1), (buff_len - 1));
        }
        
        /* Remove all trailing spaces and tabs and newlines etc. */
        end = strlen(buff) - 1;
        while ((buff[end] == '\t') || (buff[end] == ' ') || (buff[end] == '\r') || (buff[end] == '\n'))
        {
            buff[end] = '\0';
            --end;
        }
    }
    
    return buff;
}

/* This function is identical to fgets() provided by the standard C library except that it will stop reading when it
 * encounters:-
 * 
 * A newline (\n).
 * A form feed (\r).
 * A null terminator (\0).
 *
 * It will also remove any leading or trailing spaces, tabs, newlines or form feeds from the returned string.
 */
char *read_string_from_fp(char *buff, size_t buff_len, FILE *fp)
{
    if (buff_len == 0) return NULL;
    if (buff == NULL) return NULL;
    if (fp == NULL) return NULL;
    memset(buff, 0, buff_len);
    char ch = 0;
    size_t end = buff_len - 1; /* Read one less character than the length of the buffer so that there will always be a terminating null. */
    
    for (size_t idx = 0; idx < end; ++idx)
    {
        if (feof(fp) != 0) break;
        if (ferror(fp) != 0) return NULL;
        ch = (char)getc(fp);
        if ((ch == '\r') || (ch == '\n') || (ch == '\0'))
        {
            /* If ch == \r then it is possible that there could be a \n still left in the stream (if the file uses Micr$oft Windoze line endings), this will trip up the next read from the stream. */
            if (ch == '\r')
            {
                ch = (char)getc(fp); /* Get the next character. */
                if (ch != '\n') ungetc((int)ch, fp); /* If it is not a \n then put it back. */
            }
            /* If ch == ';' then there could be a \r and or \n left in the stream that will have to be consumed to prevent the next read from failing. */
            if (ch == ';')
            {
                ch = (char)getc(fp); /* Get the next character. */
                if ((ch != '\r') && (ch != '\n')) ungetc((int)ch, fp); /* If it is not a \n then put it back. */
                if (ch == '\r') /* If the next character is \r then there could be a \n after it. */
                {
                    ch = (char)getc(fp); /* Get the next character. */
                    if (ch != '\n') ungetc((int)ch, fp); /* If it is not a \n then put it back. */
                }
            }
            buff[idx] = '\0';
            break;
        }
        else
        {
            buff[idx] = ch;
        }
    }
    
    if (buff[0] != '\0')
    {
        /* Remove all leading spaces and tabs and newlines etc. */
        while ((buff[0] == '\t') || (buff[0] == ' '))
        {
            memmove(buff, (buff + 1), (buff_len - 1));
        }
        
        /* Remove all trailing spaces and tabs and newlines etc. */
        end = strlen(buff) - 1;
        while ((buff[end] == '\t') || (buff[end] == ' ') || (buff[end] == '\r') || (buff[end] == '\n'))
        {
            buff[end] = '\0';
            --end;
        }
    }
    
    return buff;
}

/* This function is identical to fgets() provided by the standard C library except that it will stop reading when it
 * encounters:-
 * 
 * A newline (\n).
 * A form feed (\r).
 * A null terminator (\0).
 *
 * It will also remove any trailing spaces, tabs, newlines or form feeds from the returned string. Leading tabs and
 * spaces will be preserved.
 */
char *read_raw_string_from_fp(char *buff, size_t buff_len, FILE *fp)
{
    if (buff_len == 0) return NULL;
    if (buff == NULL) return NULL;
    if (fp == NULL) return NULL;
    memset(buff, 0, buff_len);
    char ch = 0;
    size_t end = buff_len - 1; /* Read one less character than the length of the buffer so that there will always be a terminating null. */
    
    for (size_t idx = 0; idx < end; ++idx)
    {
        if (feof(fp) != 0) break;
        if (ferror(fp) != 0) return NULL;
        ch = (char)getc(fp);
        if ((ch == '\r') || (ch == '\n') || (ch == '\0'))
        {
            /* If ch == \r then it is possible that there could be a \n still left in the stream (if the file uses Micr$oft Windoze line endings), this will trip up the next read from the stream. */
            if (ch == '\r')
            {
                ch = (char)getc(fp); /* Get the next character. */
                if (ch != '\n') ungetc((int)ch, fp); /* If it is not a \n then put it back. */
            }
            /* If ch == ';' then there could be a \r and or \n left in the stream that will have to be consumed to prevent the next read from failing. */
            if (ch == ';')
            {
                ch = (char)getc(fp); /* Get the next character. */
                if ((ch != '\r') && (ch != '\n')) ungetc((int)ch, fp); /* If it is not a \n then put it back. */
                if (ch == '\r') /* If the next character is \r then there could be a \n after it. */
                {
                    ch = (char)getc(fp); /* Get the next character. */
                    if (ch != '\n') ungetc((int)ch, fp); /* If it is not a \n then put it back. */
                }
            }
            buff[idx] = '\0';
            break;
        }
        else
        {
            buff[idx] = ch;
        }
    }
    
    if (buff[0] != '\0')
    {
        /* Remove all trailing spaces and tabs and newlines etc. */
        end = strlen(buff) - 1;
        while ((buff[end] == '\t') || (buff[end] == ' ') || (buff[end] == '\r') || (buff[end] == '\n'))
        {
            buff[end] = '\0';
            --end;
        }
    }
    
    return buff;
}

/* This function is identical to fgets() provided by the standard C library except that it will stop reading when it
 * encounters:-
 * 
 * A newline (\n).
 * A form feed (\r).
 * A null terminator (\0).
 *
 * UNLESS the subsequent line begins with a tab or whitespace, in that case reading will continue until a line that does
 * not begin with a tab or whitespace is encountered.
 */
char *read_folded_line_from_fp(char *buff, size_t buff_len, FILE *fp)
{
    if (buff_len == 0) return NULL;
    if (buff == NULL) return NULL;
    if (fp == NULL) return NULL;
    memset(buff, 0, buff_len);
    char buffer2[BUFF_SIZE];
    off_t oldpos = 0;
    
    /* Get a string from the stream. */
    if (read_string_from_fp(buff, BUFF_SIZE, fp) == NULL) return NULL;
    
    /* Now look for any more parts to this line. */
    for (;;)
    {
        if (feof(fp) != 0) break;
        if (ferror(fp) != 0) return NULL;
        oldpos = ftello(fp);
        if (read_raw_string_from_fp(buffer2, BUFF_SIZE, fp) == NULL) return NULL;
        if ((buffer2[0] != ' ') && (buffer2[0] != '\t'))
        {
            /* If the line read does not begin with a tab or space then we have reached the end of the folded string (if it was one) so put everything back and break out of the loop. */
            fseeko(fp, oldpos, SEEK_SET);
            break;
        }
        
        /* Remove the leading spaces and tabs from the read string. */
        while ((buffer2[0] == '\t') || (buffer2[0] == ' '))
        {
            memmove(buffer2, (buffer2 + 1), (buff_len - 1));
        }
        
        strlcat(buff, buffer2, BUFF_SIZE);
    }
    
    return buff;
}

/* Appends a character to the end of a string. */
void append_char(char* s, char c)
{
    size_t len = strlen(s);
    s[len] = c;
    s[len + 1] = '\0';
}

/* Extracts the e-mail address from a sender string in the format of
 * sender name <sender_address>
 */
char *extract_address_from_sender(char *sender)
{
    ssize_t first_angbrack = first_index_of(sender, '<');
    ssize_t last_angbrack = first_index_of(sender, '>');
    size_t addr_len = 0;
    
    if ((first_angbrack != -1) && (last_angbrack != -1))
    {
        sender[last_angbrack] = '\0';
        addr_len = ((size_t)last_angbrack - (size_t)first_angbrack);
        memmove(sender, (sender + first_angbrack + 1), addr_len);
    }
    
    return  sender;
}

/* Decodes a collection of base64 encoded lines as read using fgettds. The string
 * must begin with =?utf-8?B? and end with ?= and can contain one or more embedded
 * ?==?utf-8?B? character sequences.
 *
 * The contents of buff is undefined if DECODE_ERROR is returned.
 */
int decode_base64_subject_lines(char *buff)
{
    if (buff == NULL) return DECODE_ERROR;
    if (strlen(buff) == 0) return DECODE_ERROR;
    char *enc_term = NULL;
    char *next_str = NULL;
    
    /* Position the buffer addres to the start of the encoded string (it should be at the beginning of the buffer but you never know). */
    buff = strstr(buff, "=?utf-8?B?");
    if (buff == NULL)
        return NOT_B64_ENCODED;
    else
        memset(buff, '\n', 10); /* If the magic string was not found then this is most probably not an encoded subject line so return the buffer unchanged. */
    
    enc_term = _strrstr(buff, "?=");
    if (enc_term != NULL)
    {
        enc_term[0] = '\0';
        enc_term[1] = '\0';
    }
    else
    {
        /* If a magic start string was found but the encoding terminator was not then we have a corrupt or damaged string. */
        return DECODE_ERROR;
    }
    
    /* Now we must work though the string and find any instances of '?==?utf-8?B?' and replace them with sequences of \n, which will be ignored by the decoder. */
    for (;;)
    {
        next_str = strstr(buff, "?==?utf-8?B?");
        if (next_str == NULL) break;
        memset(next_str, '\n', 12);
    }
    
    /* Now decode the string. */
    if (decode_base64((unsigned char *)buff, buff) == 0) return DECODE_ERROR;
    
    return DECODED_OK;
}

/* Decodes a collection of base64 encoded lines as read using fgettds. The string
 * must begin with =?utf-8?Q? and end with ?= and could contain one or more embedded
 * ?==?utf-8?Q?
 *
 * The contents of buff is undefined if DECODE_ERROR is returned.
 */

int decode_quoted_printable_subject_lines(char *buff)
{
    if (buff == NULL) return DECODE_ERROR;
    if (strlen(buff) == 0) return DECODE_ERROR;
    char *enc_term = NULL;
    char *next_str = NULL;
    
    /* Position the buffer addres to the start of the encoded string (it should be at the beginning of the buffer but you never know). */
    buff = strstr(buff, "=?utf-8?Q?");
    if (buff == NULL)
        return NOT_B64_ENCODED;
    else
        memset(buff, '\n', 10); /* If the magic string was not found then this is most probably not an encoded subject line so return the buffer unchanged. */
    
    enc_term = _strrstr(buff, "?=");
    if (enc_term != NULL)
    {
        enc_term[0] = '\0';
        enc_term[1] = '\0';
    }
    else
    {
        /* If a magic start string was found but the encoding terminator was not then we have a corrupt or damaged string. */
        return DECODE_ERROR;
    }
    
    /* Now we must work though the string and find any instances of '?==?utf-8?Q?' and replace them with sequences of \n, which will be ignored by the decoder. */
    for (;;)
    {
        next_str = strstr(buff, "?==?utf-8?Q?");
        if (next_str == NULL) break;
        memset(next_str, '\n', 12);
    }
    
    /* Now decode the string. */
    decode_quoted_printable_line(buff);

    return DECODED_OK;
}

/* Does what says on the tin. */
ssize_t first_index_of(const char *str, char c)
{
    if (str == NULL) return -1;
    
    size_t s = strlen(str);
    
    for (size_t i = 0; i < s; i++)
    {
        if (str[i] == c) return (ssize_t)i;
    }
    
    return -1;
}

ssize_t last_index_of(const char *str, char c)
{
    if (str == NULL) return -1;
    
    size_t s = strlen(str);
    ssize_t idx = -1;
    
    for (size_t i = 0; i < s; i++)
    {
        if (str[i] == c) idx = (ssize_t)i;
    }
    
    return idx;
}

/* A function that is missing from string.h, returns a pointer to the LAST occurence of str2 within str1,
 * if there is one; otherwise NULL.
 */
char *_strrstr(const char* str1, const char* str2)
{
    char *strp;
    size_t len1 = 0;
    size_t len2 = 0;
    
    len2 = strlen(str2);
    if (len2 == 0) return (char *)str1;
    
    len1 = strlen(str1);
    if (len1 - len2 <= 0) return NULL;
    
    strp = (char *)(str1 + len1 - len2);
    while(strp != str1)
    {
        if(*strp == *str2)
        {
            if (strncmp(strp, str2, len2) == 0) return strp;
        }
        strp--;
    }
    
    return NULL;
}

/* Tidy's up an existing filename string. Also used to tidy up options from command line parameters.
 * str is assumed to be at least BUFF_SIZE in length, cannot be NULL and will be modified.
 */
char *dequote_and_tidy_string(char *str)
{
    char string1[BUFF_SIZE];
    char string2[BUFF_SIZE];
    char *ptr = NULL;
    if (str == NULL) return NULL;
    size_t len = strlen(str);
    wordexp_t exp_result;
    char c = 0;
    size_t idx2 = 0;
    
    /* Remove only enclosing single quotes as apostrophes are permitted in filenames. Also remove leading or trailing spaces.*/
    if ((str[len - 1] == '\'') || (str[len - 1] == ' '))
    {
        str[len - 1] = '\0';
        --len;
    }
    if ((str[0] == '\'') || (str[0] == ' '))
    {
        memmove(str, (str + 1), len);
        --len;
    }
    
    memset(string1, 0, BUFF_SIZE);
    memset(string2, 0, BUFF_SIZE);
    idx2 = 0;
    for (size_t idx1 = 0; idx1 < len; idx1++)
    {
        c = str[idx1];
        if ((c != '\"') && (c != '\t') && (c != '\r') && (c != '\n'))
        {
            string1[idx2] = c;
            ++idx2;
        }
    }
    
    /* De-tilde the path */
    if (strncmp(string1, "~/", 2) == 0)
    {
        wordexp(string1, &exp_result, 0);
        strncpy(string1, exp_result.we_wordv[0], BUFF_SIZE);
        wordfree(&exp_result);
    }
    
    if ((strncmp(string1, "./", 2) == 0) || (strncmp(string1, "../", 3) == 0))
    {
        ptr = realpath(string1, string2);
        if (ptr == NULL) ptr = string1;
    }
    else
    {
        ptr = string1;
    }
    
    /* Remove any trailing strokes */
    size_t ldx = strlen(ptr) - 1;
    if (ptr[ldx] == '/') ptr[ldx] = '\0';
    
    strncpy(str, ptr, BUFF_SIZE);
    
    return str;
}

/* Canonicalizes the given string so that it can be used for a filename or part thereof.
 * filename is assumed to be BUFF_SIZE and containa a zero terminated string.
 * If filename is NULL or empty then a UUID will be returned.
 */
uint32_t *canonicalize_filename(uint32_t *filename)
{
    uint32_t buffer[BUFF_SIZE];
    uint32_t cur_char = 0;
    uint32_t prev_char = 0;
    size_t buf_len = 0;
    size_t idx2 = 0;
    uint32_t *extension = NULL;
    uint32_t uch = 0;
    uc_general_category_t alnum = uc_general_category_or(UC_LETTER, UC_NUMBER);
    
    if ((filename == NULL) || (u32_strlen(filename) == 0))
    {
        generate_uuid_utf32(filename, UUID_OPTS_BARE);
        return filename;
    }
    
    /* We have now got the filename in UTF-32 encoding so we can parse out any forbidden characters, etc. */
    memset(buffer, 0, sizeof(buffer));
    idx2 = 0;
    buf_len = u32_strlen(filename) + 1;
    for (size_t idx1 = 0; idx1 < buf_len; ++idx1)
    {
        if (idx2 > 0) prev_char = buffer[idx2 - 1];
        cur_char = filename[idx1];
        if (cur_char == '\0') /* If the NULL terminator is hit, copy it into the dest buffer and exit the loop. */
        {
            buffer[idx2] = '\0';
            break;
        }
        if (cur_char == ' ') cur_char = '_';
        if ((allowed_punct(cur_char) == true) && (allowed_punct(prev_char) == false))
        {
            buffer[idx2] = cur_char;
            ++idx2;
            continue;
        }
        if ((allowed_punct(cur_char) == true) && (uc_is_general_category(prev_char, alnum) == true))
        {
            buffer[idx2] = cur_char;
            ++idx2;
            continue;
        }
        if (uc_is_general_category(cur_char, alnum) == true)
        {
            buffer[idx2] = cur_char;
            ++idx2;
        }
    }
    
    /* Make sure the filename does start with a dot and make it an underscore if it does */
    if (buffer[0] == '.') buffer[0] = '_';
    
    /* Make sure that the filename does not end with a non-alphanumeric characters and lop it off if it does. */
    idx2 = u32_strlen(buffer) - 1;
    if (uc_is_general_category(buffer[idx2], alnum) == false) buffer[idx2] = '\0';
    
    /* Lastly tidy up, the file's extension, if it has one. */
    extension = u32_strrchr(buffer, '.');
    idx2 = 0;
    if (extension != NULL)
    {
        ++extension; /* Skip the dot. */
        while ((uch = extension[idx2]) != '\0')
        {
            extension[idx2] = uc_tolower(uch);
            ++idx2;
        }
    }
    
    u32_strncpy(filename, buffer, BUFF_SIZE);
    return filename;             
}

int allowed_punct(uint32_t c)
{
    if (c == '-') return true;
    if (c == '_') return true;
    if (c == '.') return true;
    if (c == '@') return true;
    if (c == ' ') return true;
    
    return false;
}

/* This function performs test to determine if the given file exists and can be overwritten if the users desires.
 * It will only return false if it can definatley prove the file does not exist, if there were any other errors, 
 * such as access/permissions issues they will be encoutered and dealt with when the times comes to actually write the file.
 */
int file_exists(const char *filename)
{
    struct stat s;
    
    if ((filename == NULL) || (strlen(filename) == 0)) return false;
    
    if (stat(filename, &s) == 0)
    {
        if ((s.st_mode & S_IFLNK) == S_IFLNK) return false;
        return true;
    }
    
    if (errno == ENOENT) return false;
    
    return true;
}

/* Generates a UUID (GUID) and formats it as specified.
 * buffer must (and is assumed to be) at least 40 bytes.
 */
char *generate_uuid(char *buffer, unsigned int opts)
{
    char buff[40];
    uuid_t uu;
    if ((opts & UUID_OPTS_USE_RANDOM) == UUID_OPTS_USE_RANDOM)
        uuid_generate_random(uu);
    else
        uuid_generate_time(uu);
    
    if ((opts & UUID_OPTS_UPPERCASE) == UUID_OPTS_UPPERCASE)
        uuid_unparse_upper(uu, buffer);
    else
        uuid_unparse_lower(uu, buffer);
    
    /* Now focus on the option bits. */
    opts &= 0x07;
    
    switch (opts)
    {
        case UUID_OPTS_BARE:
            memmove((buffer + 8), (buffer + 9), 4);
            memmove((buffer + 12), (buffer + 14), 4);
            memmove((buffer + 16), (buffer + 19), 4);
            memmove((buffer + 20), (buffer + 24), 12);
            buffer[32] = '\0';
            break;
        case UUID_OPTS_MS_REGISTRY:
            snprintf(buff, 40, "{%s}", buffer);
            strncpy(buffer, buff, 40);
            break;
        case UUID_OPTS_UNDERSCORES:
            buffer[8] = '_';
            buffer[13] = '_';
            buffer[18] = '_';
            buffer[23] = '_';
            break;
    }
    
    return buffer;
}

/* Scans through a string looking for a UUID, moving forward one character at a time until a UUID is found
 * or there is insufficient space left for a UUID. If one is found it is replaced by a new one.
 */
int replace_uuid(char *uuid)
{
    size_t diff = 0;
    size_t len = strlen(uuid);
    char *start = NULL;
    
    if (len > 32)
    {
        diff = len - 32;
        for (size_t offset = 0; offset <= diff; offset++)
        {
            start = uuid + offset;
            if (_ruuidutf8(start) == true) return true;
        }
        return false;
    }
    
    return _ruuidutf8(uuid);
}

/* Scans through a string looking for a UUID, moving forward one character at a time until a UUID is found
 * or there is insufficient space left for a UUID. If one is found it is replaced by a new one.
 */
int replace_uuid_utf32(uint32_t *uuid)
{
    size_t diff = 0;
    size_t len = u32_strlen(uuid);
    uint32_t *start = NULL;
    
    if (len > 32)
    {
        diff = len - 32;
        for (size_t offset = 0; offset <= diff; offset++)
        {
            start = uuid + offset;
            if (_ruuidutf32(start) == true) return true;
        }
        return false;
    }
    
    return _ruuidutf32(uuid);
}

/* Scans the given string to determine if it is (or starts with) a valid UUID, if it is replaces it like-for-like with a new one and
 * returns true. Returns false otherwise.
 */
int _ruuidutf8(char *uuid)
{
    char c = 0;
    unsigned int opts = 0;
    int lower_case_seen = false;
    int upper_case_seen = false;
    char buffer[40];
    
    if (strlen(uuid) < 32) return false;
    
    for (int i = 0; i < 32; i++)
    {
        c = uuid[i];
        if ((c >= 'A') && (c <= 'F'))
        {
            if (lower_case_seen == true) goto defaultUUID; /* A properly formed UUID should not contain a mix of upper and lower case hexadecimal digits. */
            opts = UUID_OPTS_UPPERCASE;
            upper_case_seen = true;
            continue;
        }
        if ((c >= 'a') && (c <= 'f'))
        {
            if (upper_case_seen == true) goto defaultUUID; /* A properly formed UUID should not contain a mix of upper and lower case hexadecimal digits. */
            lower_case_seen = true;
            continue;
        }
        if ((c >= '0') && (c <= '9'))
        {
            continue;
        }
        else
        {
            goto defaultUUID;
        }
    }
    /* If we get this far then it was a valid UUID, so we can replace it. */
    opts |= UUID_OPTS_BARE;
    generate_uuid(buffer, opts);
    /* Copy just the UUID (no null terminators) over the top of the old UUID. */
    memcpy(uuid, buffer, 32);
    return true;
    
defaultUUID:
    
    if (strlen(uuid) < 36) return false;
    
    for (int i = 0; i < 36; i++)
    {
        c = uuid[i];
        if ((c == '-') && (i == 8)) continue;
        if ((c == '-') && (i == 13)) continue;
        if ((c == '-') && (i == 18)) continue;
        if ((c == '-') && (i == 23)) continue;
        if ((c >= 'A') && (c <= 'F'))
        {
            if (lower_case_seen == true) goto underscoredUUID; /* A properly formed UUID should not contain a mix of upper and lower case hexadecimal digits. */
            opts = UUID_OPTS_UPPERCASE;
            upper_case_seen = true;
            continue;
        }
        if ((c >= 'a') && (c <= 'f'))
        {
            if (upper_case_seen == true) goto underscoredUUID; /* A properly formed UUID should not contain a mix of upper and lower case hexadecimal digits. */
            lower_case_seen = true;
            continue;
        }
        if ((c >= '0') && (c <= '9'))
        {
            continue;
        }
        else
        {
            goto registryUUID;
        }
    }
    /* If we get this far then it was a valid UUID, so we can replace it. */
    opts |= UUID_OPTS_DEFAULT;
    generate_uuid(buffer, opts);
    /* Copy just the UUID (no null terminators) over the top of the old UUID. */
    memcpy(uuid, buffer, 36);
    return true;
    
underscoredUUID:
    
    for (int i = 0; i < 36; i++)
    {
        c = uuid[i];
        if ((c == '_') && (i == 8)) continue;
        if ((c == '_') && (i == 13)) continue;
        if ((c == '_') && (i == 18)) continue;
        if ((c == '_') && (i == 23)) continue;
        if ((c >= 'A') && (c <= 'F'))
        {
            if (lower_case_seen == true) goto registryUUID; /* A properly formed UUID should not contain a mix of upper and lower case hexadecimal digits. */
            opts = UUID_OPTS_UPPERCASE;
            upper_case_seen = true;
            continue;
        }
        if ((c >= 'a') && (c <= 'f'))
        {
            if (upper_case_seen == true) goto registryUUID; /* A properly formed UUID should not contain a mix of upper and lower case hexadecimal digits. */
            lower_case_seen = true;
            continue;
        }
        if ((c >= '0') && (c <= '9'))
        {
            continue;
        }
        else
        {
            goto registryUUID;
        }
    }
    /* If we get this far then it was a valid UUID, so we can replace it. */
    opts |= UUID_OPTS_UNDERSCORES;
    generate_uuid(buffer, opts);
    /* Copy just the UUID (no null terminators) over the top of the old UUID. */
    memcpy(uuid, buffer, 36);
    return true;    
    
registryUUID:
    
    if (strlen(uuid) < 38) return false;
    
    for (int i = 0; i < 38; i++)
    {
        c = uuid[i];
        if ((c == '{') && (i == 0)) continue;
        if ((c == '}') && (i == 37)) continue;
        if ((c == '-') && (i == 9)) continue;
        if ((c == '-') && (i == 14)) continue;
        if ((c == '-') && (i == 19)) continue;
        if ((c == '-') && (i == 24)) continue;
        if ((c >= 'A') && (c <= 'F'))
        {
            if (lower_case_seen == true) return false; /* A properly formed UUID should not contain a mix of upper and lower case hexadecimal digits. */
            opts = UUID_OPTS_UPPERCASE;
            upper_case_seen = true;
            continue;
        }
        if ((c >= 'a') && (c <= 'f'))
        {
            if (upper_case_seen == true) return false; /* A properly formed UUID should not contain a mix of upper and lower case hexadecimal digits. */
            lower_case_seen = true;
            continue;
        }
        if ((c >= '0') && (c <= '9'))
        {
            continue;
        }
        else
        {
            return false;
        }
    }
    /* If we get this far then it was a valid UUID, so we can replace it. */
    opts |= UUID_OPTS_MS_REGISTRY;
    generate_uuid(buffer, opts);
    /* Copy just the UUID (no null terminators) over the top of the old UUID. */
    memcpy(uuid, buffer, 38);
    return true;
}

int _ruuidutf32(uint32_t *uuid)
{
    uint32_t c = 0;
    unsigned int opts = 0;
    int lower_case_seen = false;
    int upper_case_seen = false;
    uint32_t buffer[40];
    
    if (u32_strlen(uuid) < 32) return false;
    
    for (int i = 0; i < 32; i++)
    {
        c = uuid[i];
        if ((c >= 'A') && (c <= 'F'))
        {
            if (lower_case_seen == true) goto defaultUUID; /* A properly formed UUID should not contain a mix of upper and lower case hexadecimal digits. */
            opts = UUID_OPTS_UPPERCASE;
            upper_case_seen = true;
            continue;
        }
        if ((c >= 'a') && (c <= 'f'))
        {
            if (upper_case_seen == true) goto defaultUUID; /* A properly formed UUID should not contain a mix of upper and lower case hexadecimal digits. */
            lower_case_seen = true;
            continue;
        }
        if ((c >= '0') && (c <= '9'))
        {
            continue;
        }
        else
        {
            goto defaultUUID;
        }
    }
    /* If we get this far then it was a valid UUID, so we can replace it. */
    opts |= UUID_OPTS_BARE;
    generate_uuid_utf32(buffer, opts);
    /* Copy just the UUID (no null terminators) over the top of the old UUID. */
    memcpy(uuid, buffer, 128);
    return true;
    
defaultUUID:
    
    if (u32_strlen(uuid) < 36) return false;
    
    for (int i = 0; i < 36; i++)
    {
        c = uuid[i];
        if ((c == '-') && (i == 8)) continue;
        if ((c == '-') && (i == 13)) continue;
        if ((c == '-') && (i == 18)) continue;
        if ((c == '-') && (i == 23)) continue;
        if ((c >= 'A') && (c <= 'F'))
        {
            if (lower_case_seen == true) goto registryUUID; /* A properly formed UUID should not contain a mix of upper and lower case hexadecimal digits. */
            opts = UUID_OPTS_UPPERCASE;
            upper_case_seen = true;
            continue;
        }
        if ((c >= 'a') && (c <= 'f'))
        {
            if (upper_case_seen == true) goto registryUUID; /* A properly formed UUID should not contain a mix of upper and lower case hexadecimal digits. */
            lower_case_seen = true;
            continue;
        }
        if ((c >= '0') && (c <= '9'))
        {
            continue;
        }
        else
        {
            goto registryUUID;
        }
    }
    /* If we get this far then it was a valid UUID, so we can replace it. */
    opts |= UUID_OPTS_DEFAULT;
    generate_uuid_utf32(buffer, opts);
    /* Copy just the UUID (no null terminators) over the top of the old UUID. */
    memcpy(uuid, buffer, 144);
    return true;
    
registryUUID:
    
    if (u32_strlen(uuid) < 38) return false;
    
    for (int i = 0; i < 38; i++)
    {
        c = uuid[i];
        if ((c == '{') && (i == 0)) continue;
        if ((c == '}') && (i == 37)) continue;
        if ((c == '-') && (i == 9)) continue;
        if ((c == '-') && (i == 14)) continue;
        if ((c == '-') && (i == 19)) continue;
        if ((c == '-') && (i == 24)) continue;
        if ((c >= 'A') && (c <= 'F'))
        {
            if (lower_case_seen == true) return false; /* A properly formed UUID should not contain a mix of upper and lower case hexadecimal digits. */
            opts = UUID_OPTS_UPPERCASE;
            upper_case_seen = true;
            continue;
        }
        if ((c >= 'a') && (c <= 'f'))
        {
            if (upper_case_seen == true) return false; /* A properly formed UUID should not contain a mix of upper and lower case hexadecimal digits. */
            lower_case_seen = true;
            continue;
        }
        if ((c >= '0') && (c <= '9'))
        {
            continue;
        }
        else
        {
            return false;
        }
    }
    /* If we get this far then it was a valid UUID, so we can replace it. */
    opts |= UUID_OPTS_MS_REGISTRY;
    generate_uuid_utf32(buffer, opts);
    /* Copy just the UUID (no null terminators) over the top of the old UUID. */
    memcpy(uuid, buffer, 152);
    return true;
}

/* Generates a UUID (GUID) and formats it as specified.
 * buffer must (and is assumed to be) at least 40 UTF-32 characters.
 */
uint32_t *generate_uuid_utf32(uint32_t *buffer, unsigned int opts)
{
    char utf8_buffer[40];
    
    memset(buffer, 0, 160); /* Bad things could happen if the passed buffer is not at least 40 UTF-32 chars. */
    
    generate_uuid(utf8_buffer, opts);
    
    /* Since a UUID will only ever contain ASCII characters we can safely just cast each character to a uint32_t. */
    for (size_t i = 0; i < 38; i++)
        buffer[i] = (uint32_t)utf8_buffer[i];
    
    return buffer;
}

/* Uses libmagic to sniff the mime type and extension of the given file and return it as a Unicode UTF32 string.
 * This function assumes that buffer is at least 50 utf32 chars in size.
 uint32_t *get_file_extension_utf32(const char *filename, uint32_t *buffer)
 {
 // TODO NULL check for buffer.
 static unsigned int counter = 1;
 magic_t myt;
 const char *mime_type = NULL;
 
 myt = magic_open(MAGIC_ERROR | MAGIC_MIME_TYPE);
 magic_load(myt, NULL);
 mime_type = magic_file(myt, filename);
 if (mime_type != NULL)
 {
 printf("MIME: %s\n", mime_type);
 }
 else
 {
 u32_snprintf(buffer, 50, "%04d", counter);
 ++counter;
 ulc_fprintf(stdout, "MIME: %llU\n", buffer);
 }
 
 magic_close(myt);
 
 return buffer;
 } */

/*
 * Decode one line of data containing QP data.
 * Return flag set if this line ends with a soft line-break.
 * 'bufp' is modified to point to the end of the output buffer.
 */
char *decode_quoted_printable_line(char *buff)
{
    char *p_in = buff;
    char *p_out = NULL;
    char *p = NULL;
    size_t n;
    
    for (p_out = buff; (*p_in); )
    {
        /* Skip leading or embedded CRs and LFs. */
        if (*p_in == '\r')
        {
            p_in++;
            continue;
        }
        if (*p_in == '\n')
        {
            p_in++;
            continue;
        }
        
        p = strchr(p_in, '=');
        if (p == NULL)
        {
            /* No more QP data, just move remainder into place */
            n = strlen(p_in);
            memmove(p_out, p_in, n);
            p_in += n;
            p_out += n;
        }
        else
        {
            if (p > p_in)
            {
                /* There are some uncoded chars at the beginning. */
                n = (size_t)(p - p_in);
                memmove(p_out, p_in, n);
                p_out += n;
            }
            
            switch (*(p + 1))
            {
                case '\0':
                case '\r':
                case '\n':
                    /* Soft line break, skip '=' */
                    p_in = p + 1; 
                    if (*p_in == '\r') p_in++;
                    if (*p_in == '\n') p_in++;
                    break;
                default:
                    /* There is a QP encoded byte */
                    if (qp_char(*(p + 1), *(p + 2), p_out) == 0)
                    {
                        p_in = p + 3;
                    }
                    else
                    {
                        /* Invalid QP data - pass through unchanged. */
                        *p_out = '=';
                        p_in = p + 1;
                    }
                    p_out++;
                    break;
            }
        }
    }
    
    *p_out = '\0';
    
    return buff;
}

int qp_char(char c1, char c2, char *c_out)
{
    c1 = unhex(c1);
    c2 = unhex(c2);
    
    if ((c1 > 15) || (c2 > 15)) 
        return 1;
    else
    {
        *c_out = 16 * c1 + c2;
        return 0;
    }
}

char unhex(char c)
{
    if ((c >= '0') && (c <= '9'))
        return (c - '0');
    else if ((c >= 'A') && (c <= 'F'))
        return (c - 'A' + 10);
    else if ((c >= 'a') && (c <= 'f'))
        return (c - 'a' + 10);
    else
        return c;
}

