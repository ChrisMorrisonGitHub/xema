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

#ifndef xema_xema_h
#define xema_xema_h

#define _FILE_OFFSET_BITS 64
#define _DARWIN_USE_64_BIT_INODE

struct ctr
{
    uint64_t files_total;
    uint64_t message_total;
    uint64_t attachment_total;
    uint64_t extracted_total;
    uint64_t discardrd_total;
    uint64_t renamed_total;
    uint64_t overwritten_total;
    uint64_t warning_total;
    uint64_t error_total;
};

/* Microsoft Outlook message source binary header.
 * The members of the following structure are stored as BIG ENDIAN, unsigned 32-bit integers.
 */
typedef struct olkheader
{
    uint32_t magic;           /* Should always be 0x4D537263 (MSrc) */
    uint32_t size1;           /* The size of the message data that follows (filesize - 20) */
    uint32_t size2;           /* The same as size1 - presumably these are compressed/uncompressed size fields to support some kind of compression. */
    uint32_t crc32;           /* The CRC32 of the message data that follows. */
    uint32_t message_number;  /* The number of this message, contained in the original filename, not used by us. */
} __attribute__((__packed__)) olkheader;

/* Core logic. */
int do_extraction(void);
void print_summary_to_screen(void);
int extract_from_mbox_style_files(int *result);
int extract_from_pst_file(int *result);
int extract_from_olm_archive(int *result);
int process_pst_item(pst_file *pf, pst_item *outeritem, pst_desc_tree *d_ptr);
int sniff_for_eml_header(FILE *fp, char *hint);
char *build_output_path(const char *filename, size_t filename_length, char *buffer, int do_rename);
int decode_and_write_base64_data(FILE *source_fileptr, FILE *dest_fileptr, size_t *bytes_read);

#endif
