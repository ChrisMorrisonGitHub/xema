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

#ifndef xema_xema_defs_h
#define xema_xema_defs_h

#define ARRAYSIZE(a) (sizeof(a) / sizeof(a[0]))

#define BUFF_SIZE               4096

#define NO_ADDRESS      "NO_ADDRESS"
#define NO_SUBJECT      "NO_SUBJECT"
#define NO_MEM_FOR_DATA "ERROR"

#define PREFIX_NONE             0
#define PREFIX_SUBJECT          1
#define PREFIX_ADDRESS          2

#define CONFLICT_OVERWRITE      0
#define CONFLICT_RENAME         1
#define CONFLICT_DISCARD        2

#define X_SUCCESS               0      /* Normal (no errors or warnings detected) */
#define X_NON_FATAL             1      /* Warning (Non fatal error(s)). */
#define X_FATAL                 2      /* Fatal error. */
#define X_BAD_PARAMS            7      /* Bad command line parameters. */
#define X_NO_MEMORY             8      /* Not enough memory for operation. */
#define X_CANCELLED             255    /* User stopped the process with control-C (or similar) */

#define UUID_OPTS_DEFAULT       0x00
#define UUID_OPTS_BARE          0x01
#define UUID_OPTS_MS_REGISTRY   0x02
#define UUID_OPTS_UNDERSCORES   0x04
#define UUID_OPTS_USE_RANDOM    0x08
#define UUID_OPTS_UPPERCASE     0x80000000

#define NOT_B64_ENCODED         0x00
#define NOT_QP_ENCODED          0x01
#define DECODED_OK              0x02
#define DECODE_ERROR            0x03

#endif
