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

#ifndef xema_utils_h
#define xema_utils_h

#define _FILE_OFFSET_BITS 64
#define _DARWIN_USE_64_BIT_INODE

char *read_token_from_fp(char *buff, size_t buff_len, FILE *fp);
char *read_string_from_fp(char *buff, size_t buff_len, FILE *fp);
char *read_raw_string_from_fp(char *buff, size_t buff_len, FILE *fp);
char *read_folded_line_from_fp(char *buff, size_t buff_len, FILE *fp);
void append_char(char* s, char c);
char *dequote_and_tidy_string(char *str);
char *extract_address_from_sender(char *sender);
int decode_base64_subject_lines(char *buff);
int decode_quoted_printable_subject_lines(char *buff);
ssize_t first_index_of(const char *str, char c);
ssize_t last_index_of(const char *str, char c);
uint32_t *canonicalize_filename(uint32_t *filename);
int allowed_punct(uint32_t c);
int file_exists(const char *filename);
char *generate_uuid(char *buffer, unsigned int opts);
uint32_t *generate_uuid_utf32(uint32_t *buffer, unsigned int opts);
int _ruuidutf8(char *uuid);
int _ruuidutf32(uint32_t *uuid);
int replace_uuid(char *uuid);
int replace_uuid_utf32(uint32_t *uuid);
int qp_char(char c1, char c2, char *c_out);
char unhex(char c);
char *decode_quoted_printable_line(char *buff);
char *_strrstr(const char *str1, const char *str2);

#endif
