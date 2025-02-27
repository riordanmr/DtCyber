/*--------------------------------------------------------------------------
**
**  Copyright (c) 2003-2011, Tom Hunter
**
**  Name: charset.c
**
**  Description:
**      CDC 6600 character set conversions.
**
**  This program is free software: you can redistribute it and/or modify
**  it under the terms of the GNU General Public License version 3 as
**  published by the Free Software Foundation.
**
**  This program is distributed in the hope that it will be useful,
**  but WITHOUT ANY WARRANTY; without even the implied warranty of
**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**  GNU General Public License version 3 for more details.
**
**  You should have received a copy of the GNU General Public License
**  version 3 along with this program in file "license-gpl-3.0.txt".
**  If not, see <http://www.gnu.org/licenses/gpl-3.0.txt>.
**
**--------------------------------------------------------------------------
*/

/*
**  -------------
**  Include Files
**  -------------
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "const.h"
#include "types.h"
#include "proto.h"

/*
**  --------------------------------------
**  Public character set conversion tables
**  --------------------------------------
*/
const u8 asciiToCdc[256] =
    {
    /* 000- */
    0,                0,   0,   0,   0,   0,   0,   0,
    /* 010- */ 0,     0,   0,   0,   0,   0,   0,   0,
    /* 020- */ 0,     0,   0,   0,   0,   0,   0,   0,
    /* 030- */ 0,     0,   0,   0,   0,   0,   0,   0,
    /* 040- */ 055, 066, 064, 060, 053, 063, 067, 070,
    /* 050- */ 051, 052, 047, 045, 056, 046, 057, 050,
    /* 060- */ 033, 034, 035, 036, 037, 040, 041, 042,
    /* 070- */ 043, 044,   0, 077, 072, 054, 073, 071,
    /* 100- */ 074, 001, 002, 003, 004, 005, 006, 007,
    /* 110- */ 010, 011, 012, 013, 014, 015, 016, 017,
    /* 120- */ 020, 021, 022, 023, 024, 025, 026, 027,
    /* 130- */ 030, 031, 032, 061, 075, 062, 076, 065,
    /* 140- */ 0,   001, 002, 003, 004, 005, 006, 007,
    /* 150- */ 010, 011, 012, 013, 014, 015, 016, 017,
    /* 160- */ 020, 021, 022, 023, 024, 025, 026, 027,
    /* 170- */ 030, 031, 032, 061,   0,   0,   0, 0
    };

const char cdcToAscii[64] =
    {
    /* 00-07 */
    ':',              'A', 'B', 'C', 'D', 'E',  'F', 'G',
    /* 10-17 */ 'H',  'I', 'J', 'K', 'L', 'M',  'N', 'O',
    /* 20-27 */ 'P',  'Q', 'R', 'S', 'T', 'U',  'V', 'W',
    /* 30-37 */ 'X',  'Y', 'Z', '0', '1', '2',  '3', '4',
    /* 40-47 */ '5',  '6', '7', '8', '9', '+',  '-', '*',
    /* 50-57 */ '/',  '(', ')', '$', '=', ' ',  ',', '.',
    /* 60-67 */ '#',  '[', ']', '%', '"', '_',  '!', '&',
    /* 70-77 */ '\'', '?', '<', '>', '@', '\\', '^', ';'
    };

const u8 asciiToConsole[256] =
    {
    /* 00-07 */
    0,                 0,   0,   0,   0,   0,   0,   0,
    /* 08-0F */ 061,   0, 060,   0,   0, 060,   0,   0,
    /* 10-17 */ 0,     0,   0,   0,   0,   0,   0,   0,
    /* 18-1F */ 0,     0,   0,   0,   0,   0,   0,   0,
    /* 20-27 */ 062,   0,   0,   0,   0,   0,   0,   0,
    /* 28-2F */ 051, 052, 047, 045, 056, 046, 057, 050,
    /* 30-37 */ 033, 034, 035, 036, 037, 040, 041, 042,
    /* 38-3F */ 043, 044,   0,   0,   0, 054,   0,   0,
    /* 40-47 */ 0,    01,  02,  03,  04,  05,  06,  07,
    /* 48-4F */ 010, 011, 012, 013, 014, 015, 016, 017,
    /* 50-57 */ 020, 021, 022, 023, 024, 025, 026, 027,
    /* 58-5F */ 030, 031, 032, 053,   0, 055,   0,   0,
    /* 60-67 */ 0,    01,  02,  03,  04,  05,  06,  07,
    /* 68-6F */ 010, 011, 012, 013, 014, 015, 016, 017,
    /* 70-77 */ 020, 021, 022, 023, 024, 025, 026, 027,
    /* 78-7F */ 030, 031, 032,   0,   0,   0,   0, 0
    };

const char consoleToAscii[64] =
    {
    /* 00-07 */
    0,               'A', 'B', 'C', 'D', 'E', 'F', 'G',
    /* 10-17 */ 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O',
    /* 20-27 */ 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W',
    /* 30-37 */ 'X', 'Y', 'Z', '0', '1', '2', '3', '4',
    /* 40-47 */ '5', '6', '7', '8', '9', '+', '-', '*',
    /* 50-57 */ '/', '(', ')', ' ', '=', ' ', ',', '.',
    /* 60-67 */ 0,     0,   0,   0,   0,   0,   0,   0,
    /* 70-77 */ 0,     0,   0,   0,   0,   0,   0, 0
    };

const PpWord asciiTo026[256] =
    {
    /*                                                                         */
    /* 000- */
    0,                    0,     0,     0,     0,     0,     0,     0,
    /*                                                                         */
    /* 010- */ 0,         0,     0,     0,     0,     0,     0,     0,
    /*                                                                         */
    /* 020- */ 0,         0,     0,     0,     0,     0,     0,     0,
    /*                                                                         */
    /* 030- */ 0,         0,     0,     0,     0,     0,     0,     0,
    /*          space   !       "       #       $       %       &       '      */
    /* 040- */ 0,     03000, 00042, 01012, 02102, 00012, 01006, 02022,
    /*          (       )       *       +       ,       -       .       /      */
    /* 050- */ 01042, 04042, 02042, 04000, 01102, 02000, 04102, 01400,
    /*          0       1       2       3       4       5       6       7      */
    /* 060- */ 01000, 00400, 00200, 00100, 00040, 00020, 00010, 00004,
    /*          8       9       :       ;       <       =       >       ?      */
    /* 070- */ 00002, 00001, 00202, 04006, 05000, 00102, 02006, 02012,
    /*          @       A       B       C       D       E       F       G      */
    /* 100- */ 00022, 04400, 04200, 04100, 04040, 04020, 04010, 04004,
    /*          H       I       J       K       L       M       N       O      */
    /* 110- */ 04002, 04001, 02400, 02200, 02100, 02040, 02020, 02010,
    /*          P       Q       R       S       T       U       V       W      */
    /* 120- */ 02004, 02002, 02001, 01200, 01100, 01040, 01020, 01010,
    /*          X       Y       Z       [       \       ]       ^       _      */
    /* 130- */ 01004, 01002, 01001, 00006, 04022, 01202, 04012, 01022,
    /*          `       a       b       c       d       e       f       g      */
    /* 140- */ 0,     04400, 04200, 04100, 04040, 04020, 04010, 04004,
    /*          h       i       j       k       l       m       n       o      */
    /* 150- */ 04002, 04001, 02400, 02200, 02100, 02040, 02020, 02010,
    /*          p       q       r       s       t       u       v       w      */
    /* 160- */ 02004, 02002, 02001, 01200, 01100, 01040, 01020, 01010,
    /*          x       y       z       {       |       }       ~              */
    /* 170- */ 01004, 01002, 01001, 00015,     0, 00017, 00007, 0
    };


const PpWord asciiTo029[256] =
    {
    /*                                                                         */
    /* 000- */
    0,                    0,     0,     0,     0,     0,     0,     0,
    /*                                                                         */
    /* 010- */ 0,         0,     0,     0,     0,     0,     0,     0,
    /*                                                                         */
    /* 020- */ 0,         0,     0,     0,     0,     0,     0,     0,
    /*                                                                         */
    /* 030- */ 0,         0,     0,     0,     0,     0,     0,     0,
    /*          space   !       "       #       $       %       &       '      */
    /* 040- */ 0,     04006, 00006, 00102, 02102, 01042, 04000, 00022,
    /*          (       )       *       +       ,       -       .       /      */
    /* 050- */ 04022, 02022, 02042, 04012, 01102, 02000, 04102, 01400,
    /*          0       1       2       3       4       5       6       7      */
    /* 060- */ 01000, 00400, 00200, 00100, 00040, 00020, 00010, 00004,
    /*          8       9       :       ;       <       =       >       ?      */
    /* 070- */ 00002, 00001, 00202, 02012, 04042, 00012, 01012, 01006,
    /*          @       A       B       C       D       E       F       G      */
    /* 100- */ 00042, 04400, 04200, 04100, 04040, 04020, 04010, 04004,
    /*          H       I       J       K       L       M       N       O      */
    /* 110- */ 04002, 04001, 02400, 02200, 02100, 02040, 02020, 02010,
    /*          P       Q       R       S       T       U       V       W      */
    /* 120- */ 02004, 02002, 02001, 01200, 01100, 01040, 01020, 01010,
    /*          X       Y       Z       [       \       ]       ^       _      */
    /* 130- */ 01004, 01002, 01001, 04202, 01202, 02202, 02006, 01022,
    /*          `       a       b       c       d       e       f       g      */
    /* 140- */ 0,     04400, 04200, 04100, 04040, 04020, 04010, 04004,
    /*          h       i       j       k       l       m       n       o      */
    /* 150- */ 04002, 04001, 02400, 02200, 02100, 02040, 02020, 02010,
    /*          p       q       r       s       t       u       v       w      */
    /* 160- */ 02004, 02002, 02001, 01200, 01100, 01040, 01020, 01010,
    /*          x       y       z       {       |       }       ~              */
    /* 170- */ 01004, 01002, 01001, 00015,     0, 00017, 00007, 0
    };

/*
**  077 is & but is also used as filler for unused codes
*/
const u8 asciiToBcd[256] =
    {
    /*                                                                         */
    /* 000- */
    077,            077, 077, 077, 077, 077, 077, 077,
    /*                                                                         */
    /* 010- */ 077, 077, 077, 077, 077, 077, 077, 077,
    /*                                                                         */
    /* 020- */ 077, 077, 077, 077, 077, 077, 077, 077,
    /*                                                                         */
    /* 030- */ 077, 077, 077, 077, 077, 077, 077, 077,
    /*          space   !       "       #       $       %       &       '      */
    /* 040- */ 060, 052, 014, 076, 053, 016, 077, 055,
    /*          (       )       *       +       ,       -       .       /      */
    /* 050- */ 074, 034, 054, 020, 073, 040, 033, 061,
    /*          0       1       2       3       4       5       6       7      */
    /* 060- */ 000, 001, 002, 003, 004, 005, 006, 007,
    /*          8       9       :       ;       <       =       >       ?      */
    /* 070- */ 010, 011, 012, 037, 032, 013, 057, 056,
    /*          @       A       B       C       D       E       F       G      */
    /* 100- */ 015, 021, 022, 023, 024, 025, 026, 027,
    /*          H       I       J       K       L       M       N       O      */
    /* 110- */ 030, 031, 041, 042, 043, 044, 045, 046,
    /*          P       Q       R       S       T       U       V       W      */
    /* 120- */ 047, 050, 051, 062, 063, 064, 065, 066,
    /*          X       Y       Z       [       \       ]       ^       _      */
    /* 130- */ 067, 070, 071, 017, 035, 072, 036, 075,
    /*          `       a       b       c       d       e       f       g      */
    /* 140- */ 077, 021, 022, 023, 024, 025, 026, 027,
    /*          h       i       j       k       l       m       n       o      */
    /* 150- */ 030, 031, 041, 042, 043, 044, 045, 046,
    /*          p       q       r       s       t       u       v       w      */
    /* 160- */ 047, 050, 051, 062, 063, 064, 065, 066,
    /*          x       y       z       {       |       }       ~              */
    /* 170- */ 067, 070, 071, 077, 077, 077, 077, 077
    };

const char bcdToAscii[64] =
    {
    /* 00-07 */
    '0',             '1', '2', '3', '4', '5',  '6', '7',
    /* 10-17 */ '8', '9', ':', '=', '"', '@',  '%', '[',
    /* 20-27 */ '+', 'A', 'B', 'C', 'D', 'E',  'F', 'G',
    /* 30-37 */ 'H', 'I', '<', '.', ')', '\\', '^', ';',
    /* 40-47 */ '-', 'J', 'K', 'L', 'M', 'N',  'O', 'P',
    /* 50-57 */ 'Q', 'R', '!', '$', '*', 0x27, '?', '>',
    /* 60-67 */ ' ', '/', 'S', 'T', 'U', 'V',  'W', 'X',
    /* 70-77 */ 'Y', 'Z', ']', ',', '(', '_',  '#', '&'
    };

const char extBcdToAscii[64] =
    {
    /* 00-07 */
    ':',             '1', '2', '3', '4', '5',  '6', '7',
    /* 10-17 */ '8', '9', '0', '=', '"', '@',  '%', '[',
    /* 20-27 */ ' ', '/', 'S', 'T', 'U', 'V',  'W', 'X',
    /* 30-37 */ 'Y', 'Z', ']', ',', '(', '_',  '#', '&',
    /* 40-47 */ '-', 'J', 'K', 'L', 'M', 'N',  'O', 'P',
    /* 50-57 */ 'Q', 'R', '!', '$', '*', '\'', '?', '>',
    /* 60-67 */ '+', 'A', 'B', 'C', 'D', 'E',  'F', 'G',
    /* 70-77 */ 'H', 'I', '<', '.', ')', '\\', '^', ';'
    };

/* Plato keycodes translation */

/* Note that we have valid entries (entry != -1) here only for those keys
** where we do not need to do anything unusual for Shift.  For example,
** "space" is not decoded here because Shift-Space gets a different code.
*/
const i8 asciiToPlato[128] =
    {
    /*                                                                         */
    /* 000- */
    -1,                -1,   -1,   -1,   -1,   -1,   -1,   -1,
    /*                                                                         */
    /* 010- */ -1,     -1,   -1,   -1,   -1,   -1,   -1,   -1,
    /*                                                                         */
    /* 020- */ -1,     -1,   -1,   -1,   -1,   -1,   -1,   -1,
    /*                                                                         */
    /* 030- */ -1,     -1,   -1,   -1,   -1,   -1,   -1,   -1,
    /*          space   !       "       #       $       %       &       '      */
    /* 040- */ -1,   0176, 0177,   -1, 0044, 0045,   -1, 0047,
    /*          (       )       *       +       ,       -       .       /      */
    /* 050- */ 0051, 0173, 0050, 0016, 0137, 0017, 0136, 0135,
    /*          0       1       2       3       4       5       6       7      */
    /* 060- */ 0000, 0001, 0002, 0003, 0004, 0005, 0006, 0007,
    /*          8       9       :       ;       <       =       >       ?      */
    /* 070- */ 0010, 0011, 0174, 0134, 0040, 0133, 0041, 0175,
    /*          @       A       B       C       D       E       F       G      */
    /* 100- */ -1,   0141, 0142, 0143, 0144, 0145, 0146, 0147,
    /*          H       I       J       K       L       M       N       O      */
    /* 110- */ 0150, 0151, 0152, 0153, 0154, 0155, 0156, 0157,
    /*          P       Q       R       S       T       U       V       W      */
    /* 120- */ 0160, 0161, 0162, 0163, 0164, 0165, 0166, 0167,
    /*          X       Y       Z       [       \       ]       ^       _      */
    /* 130- */ 0170, 0171, 0172, 0042,   -1, 0043,   -1, 0046,
    /*          `       a       b       c       d       e       f       g      */
    /* 140- */ -1,   0101, 0102, 0103, 0104, 0105, 0106, 0107,
    /*          h       i       j       k       l       m       n       o      */
    /* 150- */ 0110, 0111, 0112, 0113, 0114, 0115, 0116, 0117,
    /*          p       q       r       s       t       u       v       w      */
    /* 160- */ 0120, 0121, 0122, 0123, 0124, 0125, 0126, 0127,
    /*          x       y       z       {       |       }       ~              */
    /* 170- */ 0130, 0131, 0132,   -1,   -1,   -1,   -1, -1
    };

/* Keycode translation for ALT-keypress */
const i8 altKeyToPlato[128] =
    {
    /*                                                                         */
    /* 000- */
    -1,                -1,   -1,   -1,   -1,   -1,   -1, -1,
    /*                                                                         */
    /* 010- */ -1,     -1,   -1,   -1,   -1,   -1,   -1, -1,
    /*                                                                         */
    /* 020- */ -1,     -1,   -1,   -1,   -1,   -1,   -1, -1,
    /*                                                                         */
    /* 040- */ -1,     -1,   -1,   -1,   -1,   -1,   -1, -1,
    /*          space   !       "       #       $       %       &       '      */
    /* 040- */ -1,     -1,   -1,   -1,   -1,   -1,   -1, -1,
    /*          (       )       *       +       ,       -       .       /      */
    /* 050- */ -1,     -1,   -1,   -1,   -1,   -1,   -1, -1,
    /*          0       1       2       3       4       5       6       7      */
    /* 060- */ -1,     -1,   -1,   -1,   -1,   -1,   -1, -1,
    /*          8       9       :       ;       <       =       >       ?      */
    /* 070- */ -1,     -1,   -1,   -1,   -1, 0015,   -1, -1,
    /*          @       A       B       C       D       E       F       G      */
    /* 100- */ -1,   0062, 0070, 0073, 0071, 0067, 0064, -1,
    /*          H       I       J       K       L       M       N       O      */
    /* 110- */ 0065,   -1,   -1,   -1, 0075, 0064, 0066, -1,
    /*          P       Q       R       S       T       U       V       W      */
    /* 120- */ -1,   0074, 0063, 0072, 0062,   -1,   -1, -1,
    /*          X       Y       Z       [       \       ]       ^       _      */
    /* 130- */ -1,     -1,   -1,   -1,   -1,   -1,   -1, -1,
    /*          `       a       b       c       d       e       f       g      */
    /* 140- */ -1,   0022, 0030, 0033, 0031, 0027, 0064, -1,
    /*          h       i       j       k       l       m       n       o      */
    /* 150- */ 0025,   -1,   -1,   -1, 0035, 0024, 0026, -1,
    /*          p       q       r       s       t       u       v       w      */
    /* 160- */ -1,   0034, 0023, 0032, 0062,   -1,   -1, -1,
    /*          x       y       z       {       |       }       ~              */
    /* 170- */ -1,     -1,   -1,   -1,   -1,   -1,   -1, -1
    };

/* EBCDIC to ASCII and ASCII to EBCDIC translations */
const u8 asciiToEbcdic[256] =
    {
    /* 00-07 */
    0x00,             0x01, 0x02, 0x03, 0x37, 0x2d, 0x2e, 0x2f,
    /* 08-0f */ 0x16, 0x05, 0x25, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
    /* 10-17 */ 0x10, 0x11, 0x12, 0x13, 0x3c, 0x3d, 0x32, 0x26,
    /* 18-1f */ 0x18, 0x19, 0x3f, 0x27, 0x1c, 0x1d, 0x1e, 0x1f,
    /* 20-27 */ 0x40, 0x5a, 0x7f, 0x7b, 0x5b, 0x6c, 0x50, 0x7d,
    /* 28-2f */ 0x4d, 0x5d, 0x5c, 0x4e, 0x6b, 0x60, 0x4b, 0x61,
    /* 30-37 */ 0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7,
    /* 38-3f */ 0xf8, 0xf9, 0x7a, 0x5e, 0x4c, 0x7e, 0x6e, 0x6f,
    /* 40-47 */ 0x7c, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7,
    /* 48-4f */ 0xc8, 0xc9, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6,
    /* 50-57 */ 0xd7, 0xd8, 0xd9, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6,
    /* 58-5f */ 0xe7, 0xe8, 0xe9, 0xad, 0xe0, 0xbd, 0x5f, 0x6d,
    /* 60-67 */ 0x79, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
    /* 68-6f */ 0x88, 0x89, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96,
    /* 70-77 */ 0x97, 0x98, 0x99, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6,
    /* 78-7f */ 0xa7, 0xa8, 0xa9, 0xc0, 0x4f, 0xd0, 0xa1, 0x07,

    /* 80-87 */ 0x20, 0x01, 0x02, 0x03, 0x37, 0x2d, 0x2e, 0x2f,
    /* 88-8f */ 0x16, 0x05, 0x25, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
    /* 90-97 */ 0x10, 0x11, 0x12, 0x13, 0x3c, 0x3d, 0x32, 0x26,
    /* 98-9f */ 0x18, 0x19, 0x3f, 0x27, 0x1c, 0x1d, 0x1e, 0x1f,
    /* a0-a7 */ 0x40, 0x5a, 0x4a, 0xb1, 0x5b, 0xb2, 0x6a, 0x7d,
    /* a8-af */ 0x4d, 0x5d, 0x5c, 0x4e, 0x6b, 0x60, 0x4b, 0x61,
    /* b0-b7 */ 0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7,
    /* b8-bf */ 0xf8, 0xf9, 0x7a, 0x5e, 0x4c, 0x7e, 0x6e, 0x6f,
    /* c0-c7 */ 0x7c, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7,
    /* c8-cf */ 0xc8, 0xc9, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6,
    /* d0-d7 */ 0xd7, 0xd8, 0xd9, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6,
    /* d8-df */ 0xe7, 0xe8, 0xe9, 0xad, 0xe0, 0xbd, 0x5f, 0x6d,
    /* e0-e7 */ 0x79, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
    /* e8-ef */ 0x88, 0x89, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96,
    /* f0-f7 */ 0x97, 0x98, 0x99, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6,
    /* f8-ff */ 0xa7, 0xa8, 0xa9, 0xc0, 0x4f, 0xd0, 0xa1, 0x07
    };

const u8 ebcdicToAscii[256] =
    {
    /* 00-07 */
    0x00,             0x01, 0x02, 0x03, 0x1a, 0x09, 0x1a, 0x7f,
    /* 08-0f */ 0x1a, 0x1a, 0x1a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
    /* 10-17 */ 0x10, 0x11, 0x12, 0x13, 0x1a, 0x1a, 0x08, 0x1a,
    /* 18-1f */ 0x18, 0x19, 0x1a, 0x1a, 0x1c, 0x1d, 0x1e, 0x1f,
    /* 20-27 */ 0x80, 0x1a, 0x1a, 0x1a, 0x1a, 0x0a, 0x17, 0x1b,
    /* 28-2f */ 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x05, 0x06, 0x07,
    /* 30-37 */ 0x1a, 0x1a, 0x16, 0x1a, 0x1a, 0x1a, 0x1a, 0x04,
    /* 38-3f */ 0x1a, 0x1a, 0x1a, 0x1a, 0x14, 0x15, 0x1a, 0x1a,
    /* 40-47 */ 0x20, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a,
    /* 48-4f */ 0x1a, 0x1a, 0xa2, 0x2e, 0x3c, 0x28, 0x2b, 0x7c,
    /* 50-57 */ 0x26, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a,
    /* 58-5f */ 0x1a, 0x1a, 0x21, 0x24, 0x2a, 0x29, 0x3b, 0x5e,
    /* 60-67 */ 0x2d, 0x2f, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a,
    /* 68-6f */ 0x1a, 0x1a, 0xa6, 0x2c, 0x25, 0x5f, 0x3e, 0x3f,
    /* 70-77 */ 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a,
    /* 78-7f */ 0x1a, 0x60, 0x3a, 0x23, 0x40, 0x27, 0x3d, 0x22,

    /* 80-87 */ 0x1a, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67,
    /* 88-8f */ 0x68, 0x69, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a,
    /* 90-97 */ 0x1a, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f, 0x70,
    /* 98-9f */ 0x71, 0x72, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a,
    /* a0-a7 */ 0x1a, 0x7e, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78,
    /* a8-af */ 0x79, 0x7a, 0x1a, 0x1a, 0x1a, 0x5b, 0x1a, 0x1a,
    /* b0-b7 */ 0x5e, 0xa3, 0xa5, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a,
    /* b8-bf */ 0x1a, 0x1a, 0x5b, 0x5d, 0x1a, 0x5d, 0x1a, 0x1a,
    /* c0-c7 */ 0x7b, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,
    /* c8-cf */ 0x48, 0x49, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a,
    /* d0-d7 */ 0x7d, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x50,
    /* d8-df */ 0x51, 0x52, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a,
    /* e0-e7 */ 0x5c, 0x1a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58,
    /* e8-ef */ 0x59, 0x5a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a,
    /* f0-f7 */ 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
    /* f8-fF */ 0x38, 0x39, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a
    };

/* This translates ASCII codes to PLATO text string codes.  0 means end of line,
 * which is encoded as 0000 at end of word. 76 and/or 70 are prefix codes (access
 * and shift respectively).
 */
const int asciiToPlatoString[256] =
    {
    /*                                                                         */
    /* 000- */
    -1,                          -1,        -1,        -1,        -1,        -1,        -1,        -1,
    /*                                                                         */
    /* 010- */ 0,                -1,        -1,        -1,        -1,        -1,        -1,        -1,
    /*                                                                         */
    /* 020- */ -1,               -1,        -1,        -1,        -1,        -1,        -1,        -1,
    /*                                                                         */
    /* 030- */ -1,               -1,        -1,        -1,        -1,        -1,        -1,        -1,
    /*          space   !       "       #       $       %       &       '      */
    /* 040- */ 055,           07057,     07056,     07653,       053,       063,     07645,     07042,
    /*          (       )       *       +       ,       -       .       /      */
    /* 050- */ 051,             052,       047,       045,       056,       046,       057,       050,
    /*          0       1       2       3       4       5       6       7      */
    /* 060- */ 033,             034,       035,       036,       037,       040,       041,       042,
    /*          8       9       :       ;       <       =       >       ?      */
    /* 070- */ 043,             044,     07077,       077,       072,       054,       073,     07050,
    /*          @       A       B       C       D       E       F       G      */
    /* 100- */ 07640,         07001,     07002,     07003,     07004,     07005,     07006,     07007,
    /*          H       I       J       K       L       M       N       O      */
    /* 110- */ 07010,         07011,     07012,     07013,     07014,     07015,     07016,     07017,
    /*          P       Q       R       S       T       U       V       W      */
    /* 120- */ 07020,         07021,     07022,     07023,     07024,     07025,     07026,     07027,
    /*          X       Y       Z       [       \       ]       ^       _      */
    /* 130- */ 07030,         07031,     07032,       061,     07650,       062,     07630,     07041,
    /*          `       a       b       c       d       e       f       g      */
    /* 140- */ 07621,           001,       002,       003,       004,       005,       006,       007,
    /*          h       i       j       k       l       m       n       o      */
    /* 150- */ 010,             011,       012,       013,       014,       015,       016,       017,
    /*          p       q       r       s       t       u       v       w      */
    /* 160- */ 020,             021,       022,       023,       024,       025,       026,       027,
    /*          x       y       z       {       |       }       ~              */
    /* 170- */ 030,             031,       032,     07661,   0767011,     07662,     07677,        -1,
    /*                                                                         */
    /* 200- */ -1,               -1,        -1,        -1,        -1,        -1,        -1,        -1,
    /*                                                                         */
    /* 210- */ -1,               -1,        -1,        -1,        -1,        -1,        -1,        -1,
    /*                                                                         */
    /* 220- */ -1,               -1,        -1,        -1,        -1,        -1,        -1,        -1,
    /*                                                                         */
    /* 230- */ -1,               -1,        -1,        -1,        -1,        -1,        -1,        -1,
    /*                  ¡       ¢       £       ¤       ¥       ¦       §      */
    /* 240- */ -1,               -1,        -1,        -1,        -1,        -1,   0767011,        -1,
    /*          ¨       ©       ª       «       ¬       ­       ®       ¯      */
    /* 250- */ -1,          0767003,        -1,        -1,        -1,        -1,        -1,        -1,
    /*          °       ±       ²       ³       ´       µ       ¶       ·      */
    /* 260- */ -1,               -1,        -1,        -1,        -1,   0767015,        -1,        -1,
    /*          ¸       ¹       º       »       ¼       ½       ¾       ¿      */
    /* 270- */ -1,               -1,        -1,        -1,        -1,        -1,        -1,        -1,
    /*          À         Á         Â         Ã         Ä         Å         Æ         Ç      */
    /* 300- */ 070017621, 070017605, 070017630, 070017616, 070017625,        -1,        -1, 070037603,
    /*          È         É         Ê         Ë         Ì         Í         Î         Ï      */
    /* 310- */ 070057621, 070057605, 070057630, 070057625, 070117621, 070117605, 070117630, 070117625,
    /*          Ð         Ñ         Ò         Ó         Ô         Õ         Ö         ×      */
    /* 320- */ -1,        070167616, 070177621, 070177605, 070177630,        -1, 070177625,        -1,
    /*          Ø         Ù         Ú         Û         Ü         Ý         Þ         ß      */
    /* 330- */ -1,        070257621, 070257605, 070257630, 070257625, 070317605,        -1,     07402,
    /*          à         á         â         ã         ä         å         æ         ç      */
    /* 340- */ 0017621,     0017605,   0017630,   0017616,   0017625,        -1,        -1,   0037603,
    /*          è         é         ê         ë         ì         í         î         ï      */
    /* 350- */ 0057621,     0057604,   0057630,   0057625,   0117621,   0117605,   0117630,   0117625,
    /*          ð         ñ         ò         ó         ô         õ         ö         ÷      */
    /* 360- */ -1,          0167616,   0177621,   0177605,   0177630,        -1,   0177625,        -1,
    /*          ø         ù         ú         û         ü         ý         þ         ÿ      */
    /* 370- */ -1,          0257621,   0257605,   0257630,   0257625,   0317605,        -1,   0317625,
    };

/* This translates PLATO string codes (as opposed to key codes) into ASCII, or
 * more precisely, ISO 8859-1 ("Latin-1").
 * \001 marks codes that don't translate to Latin-1 or are unused altogether,
 * and will be ignored during translation.
 * The four lines correspond to unshifted characters, shifted (preceded by 070),
 * access (preceded by 076) and shifted access characters.
 */
const unsigned char platoStringToAscii[4][65] =
    {
    "\001abcdefghijklmnopqrstuvwxyz0123456789+-*/()$= ,.\xf7[]%\xd7\xab\001\001\001\001<>\001\001\001;",
    "\001ABCDEFGHIJKLMNOPQRSTUVWXYZ\001\001\001\001\001\001_'\001\001\001\001\001\001{}&\001\001\"!\001\001\001\001\001\001\001\001\001\001\001\001\001\001\001:",
    "\001\001\001\001\001\001\001\xe6\xf8\001\xe5\xe4\001\xb5\001\xb0\001\001\001\001\001\001\001\001\001\xf6\001\xab\xbb\001\001\001@\xbb\001\001\001&\001\001\\\001\001#\001\001\001\001\001{}\001\xba\001\001\001\001\001\001\001\001\001\001~",
    "\001\001\001\xa9\001\001\xc6\xd8|\xc5\xc4\001\001\001\001\001\001\001\001\001\001\001\001\001\xd6\001\001\001\001\001\001\001\001\001\001\001\001\001\001\001\001\001\001\001\001\001\001\001\001\001\001\001\001\001\001\001\001\001\001\001\001\001\001"
    };

/*---------------------------  End Of File  ------------------------------*/
