/* Copyright (c) 2011-2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "mt_afe_reg.h"
#include "mt_afe_digital_type.h"
#include <linux/printk.h>

/**
* here define conenction table for input and output
*/
static const char connection_table[INTER_CONN_INPUT_NUM][INTER_CONN_OUTPUT_NUM] = {
	/* 0   1   2   3   4   5   6   7   8   9  10  11  12  13  14  15
	    16  17  18  19  20  21  22 */
	{3, 3, 3, 3, 3, 3, -1, 1, 1, 1, 1, -1, -1, 1, 1, 3,
	 3, -1, -1, 3, 3, 1, -1},	/* I00 */
	{3, 3, 3, 3, 3, -1, 3, 1, 1, 1, 1, -1, -1, 1, 1, 3,
	 3, -1, -1, 3, 3, -1, 1},	/* I01 */
	{1, 1, 1, 1, 1, 1, -1, 1, 1, -1, -1, 1, -1, 1, 1, 1,
	 1, -1, -1, -1, -1, -1, -1},	/* I02 */
	{1, 1, 3, 1, 1, 1, -1, 1, -1, 1, -1, -1, -1, 1, 1, 1,
	 1, -1, -1, 1, 1, 1, -1},	/* I03 */
	{1, 1, 3, 1, 1, -1, 1, -1, 1, -1, 1, -1, -1, 1, 1, 1,
	 1, -1, -1, 1, 1, -1, 1},	/* I04 */
	{3, 3, 3, 3, 3, 1, -1, 1, -1, 1, -1, 1, -1, 1, 1, 1,
	 1, -1, -1, 3, 3, 1, -1},	/* I05 */
	{3, 3, 3, 3, 3, -1, 1, -1, 1, -1, 1, -1, 1, 1, 1, 1,
	 1, -1, -1, 3, 3, -1, 1},	/* I06 */
	{3, 3, 3, 3, 3, 1, -1, 1, -1, 1, -1, 1, -1, 1, 1, 1,
	 1, -1, -1, 3, 3, 1, -1},	/* I07 */
	{3, 3, 3, 3, 3, -1, 1, -1, 1, -1, 1, -1, 1, 1, 1, 1,
	 1, -1, -1, 3, 3, -1, 1},	/* I08 */
	{1, 1, 1, 1, 1, 1, -1, -1, -1, 1, -1, -1, 1, 1, 1,
	 1, 1, -1, -1, 1, 1, -1, -1},	/* I09 */
	{3, -1, 3, 3, -1, 1, -1, 1, -1, 1, 1, 1, 1, -1, -1, -1,
	 -1, -1, -1, 1, -1, 1, 1},	/* I10 */
	{-1, 3, 3, -1, 3, -1, 1, -1, 1, 1, 1, 1, 1, -1, -1, -1,
	 -1, -1, -1, -1, 1, 1, 1},	/* I11 */
	{3, -1, 3, 3, -1, 1, -1, 1, -1, 1, 1, 1, 1, -1, -1, -1,
	 -1, -1, -1, 1, -1, 1, 1},	/* I12 */
	{-1, 3, 3, -1, 3, -1, 1, -1, 1, 1, 1, 1, 1, -1, -1, -1,
	 -1, -1, -1, -1, 1, 1, 1},	/* I13 */
	{-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	 -1, -1, -1, -1, -1, -1, -1},	/* I14 */
	{3, 3, 3, 3, 3, 3, -1, -1, -1, 1, -1, -1, -1, 1, 1, 3,
	 1, -1, -1, -1, -1, 1, -1},	/* I15 */
	{3, 3, 3, 3, 3, -1, 3, -1, -1, -1, 1, -1, -1, 1, 1, 1,
	 3, -1, -1, -1, -1, -1, 1},	/* I16 */
	{1, -1, 3, 1, -1, 1, -1, 1, -1, 1, -1, 1, -1, 1, 1, 1,
	 1, -1, -1, 1, 1, 1, -1},	/* I17 */
	{-1, 1, 3, -1, 1, -1, 1, -1, 1, -1, 1, -1, 1, 1, 1, 1,
	 1, -1, -1, 1, 1, -1, 1},	/* I18 */
	{3, -1, 3, 3, 3, 1, -1, 1, -1, 1, -1, 1, -1, 1, 1, 1,
	 1, -1, -1, 1, 1, 1, -1},	/* I19 */
	{-1, 3, 3, 3, 3, -1, 1, -1, 1, -1, 1, -1, 1, 1, 1, 1,
	 1, -1, -1, 1, 1, -1, 1},	/* I20 */
};

/**
* connection bits of certain bits
*/
static const char connection_bits[INTER_CONN_INPUT_NUM][INTER_CONN_OUTPUT_NUM] = {
	/* 0   1   2   3   4   5   6   7   8   9  10  11  12  13  14  15
	    16  17  18 19 20 21 22 */
	{0, 16, 0, 16, 0, 16, -1, 2, 5, 8, 12, -1, -1, 2, 15, 16,
	 22, -1, -1, 8, 24, 30, -1},	/* I00 */
	{1, 17, 1, 17, 1, -1, 22, 3, 6, 9, 13, -1, -1, 3, 16, 17,
	 23, -1, -1, 10, 26, -1, 2},	/* I01 */
	{2, 18, 2, 18, 2, 17, -1, 21, 22, -1, -1, 6, -1, 4, 17, -1,
	 24, -1, -1, -1, -1, -1, -1},	/* I02 */
	{3, 19, 3, 19, 3, 18, -1, 26, -1, 0, -1, -1, -1, 5, 18, 19,
	 25, -1, -1, 12, 28, 31, -1},	/* I03 */
	{4, 20, 4, 20, 4, -1, 23, -1, 29, -1, 3, -1, -1, 6, 19, 20,
	 26, -1, -1, 13, 29, -1, 3},	/* I04 */
	{5, 21, 5, 21, 5, 19, -1, 27, -1, 1, -1, 7, -1, 16, 20, 17,
	 26, -1, -1, 14, 31, 8, -1},	/* I05 */
	{6, 22, 6, 22, 6, -1, 24, -1, 30, -1, 4, -1, 9, 17, 21, 18,
	 27, -1, -1, 16, 0, -1, 11},	/* I06 */
	{7, 23, 7, 23, 7, 20, -1, 28, -1, 2, -1, 8, -1, 18, 22, 19,
	 28, -1, -1, 18, 2, 9, -1},	/* I07 */
	{8, 24, 8, 24, 8, -1, 25, -1, 31, -1, 5, -1, 10, 19, 23, 20,
	 29, -1, -1, 20, 4, -1, 12},	/* I08 */
	{9, 25, 9, 25, 9, 21, 27, -1, -1, 10, -1, -1, 11, 7, 20, 21,
	 27, -1, -1, 22, 6, -1, -1},	/* I09 */
	{0, -1, 4, 8, -1, 12, -1, 14, -1, 26, 28, 30, 0, -1, -1, -1,
	 -1, -1, -1, 28, -1, 0, -1},	/* I10 */
	{-1, 2, 6, -1, 10, -1, 13, -1, 15, 27, 29, 31, 1, -1, -1, -1,
	 -1, -1, -1, -1, 29, 31, 1},	/* I11 */
	{0, -1, 4, 8, -1, 12, -1, 14, -1, 8, 10, 12, 14, -1, -1, -1,
	 -1, -1, -1, 2, -1, -1, 6},	/* I12 */
	{-1, 2, 6, -1, 10, -1, 13, -1, 15, 9, 11, 13, 15, -1, -1, -1,
	 -1, -1, -1, -1, 3, 4, 7},	/* I13 */
	{-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	 -1, -1, -1, -1, -1, -1, -1},	/* I14 */
	{13, 18, 23, 28, 1, 6, -1, -1, -1, 10, -1, -1, -1, 9, 22, 29,
	 2, -1, -1, 23, -1, 10, -1},	/* I15 */
	{14, 19, 24, 29, 2, -1, 8, -1, -1, -1, 11, -1, -1, 10, 23, 30,
	 3, -1, -1, -1, -1, -1, 13},	/* I16 */
	{0, 19, 27, 0, 2, 22, 8, 26, -1, 30, 11, 2, -1, 11, 24, 21,
	 30, -1, -1, 22, 6, 0, -1},	/* I17 */
	{14, 3, 28, -1, 1, -1, 24, -1, 28, -1, 0, -1, 4, 12, 25, 22,
	 31, -1, -1, 23, 7, -1, 4},	/* I18 */
	{1, 19, 10, 14, 18, 23, 8, 27, -1, 31, -1, 3, -1, 13, 26, 23,
	 0, -1, -1, 24, 28, 1, -1},	/* I19 */
	{14, 4, 12, 16, 20, -1, 25, -1, 29, -1, 1, -1, 5, 14, 27, 24,
	 1, -1, -1, 25, 29, -1, 5},	/* I20 */
};


/**
* connection shift bits of certain bits
*/
static const char shift_connection_bits[INTER_CONN_INPUT_NUM][INTER_CONN_OUTPUT_NUM] = {
	/* 0   1   2   3   4   5   6    7  8   9  10  11  12  13  14  15
	    16  17  18  19  20  21  22 */
	{10, 26, 10, 26, 10, 19, -1, -1, -1, -1, -1, -1, -1, -1, -1, 31,
	 25, -1, -1, 9, 25, -1, -1},	/* I00 */
	{11, 27, 11, 27, 11, -1, 20, -1, -1, -1, -1, -1, -1, -1, -1, 16,
	 4, -1, -1, 11, 27, -1, -1},	/* I01 */
	{-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	 -1, -1, -1, -1, -1, -1, -1},	/* I02 */
	{-1, -1, 25, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	 -1, -1, -1, -1, -1, -1, -1},	/* I03 */
	{-1, -1, 26, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	 -1, -1, -1, -1, -1, -1, -1},	/* I04 */
	{12, 28, 12, 28, 12, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	 -1, -1, -1, 15, 30, -1, -1},	/* I05 */
	{13, 29, 13, 29, 13, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	 -1, -1, -1, 17, 1, -1, -1},	/* I06 */
	{14, 30, 14, 30, 14, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	 -1, -1, -1, 19, 3, -1, -1},	/* I07 */
	{15, 31, 15, 31, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	 -1, -1, -1, 21, 5, -1, -1},	/* I08 */
	{-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	 -1, -1, -1, -1, -1, -1, -1},	/* I09 */
	{1, -1, 5, 9, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	 -1, -1, -1, -1, -1, -1, -1},	/* I10 */
	{-1, 3, 7, -1, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	 -1, -1, -1, -1, -1, -1, -1},	/* I11 */
	{1, -1, 5, 9, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	 -1, -1, -1, -1, -1, -1, -1},	/* I12 */
	{-1, 3, 7, -1, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	 -1, -1, -1, -1, -1, -1, -1},	/* I13 */
	{-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	 -1, -1, -1, -1, -1, -1, -1},	/* I14 */
	{15, 20, 25, 30, 3, 7, -1, -1, -1, 10, -1, -1, -1, -1, -1,
	 0, 2, -1, -1, -1, -1, -1, -1},	/* I15 */
	{16, 21, 26, 31, 4, -1, 9, -1, -1, -1, 11, -1, -1, -1, -1, 30,
	 5, -1, -1, -1, -1, -1, -1},	/* I16 */
	{14, 19, 30, 29, 2, -1, 8, -1, -1, -1, 11, -1, -1, -1, -1, 30,
	 3, -1, -1, -1, -1, -1, -1},	/* I17 */
	{14, 19, 31, 29, 2, -1, 8, -1, -1, -1, 11, -1, -1, -1, -1, 30,
	 3, -1, -1, -1, -1, -1, -1},	/* I18 */
	{2, 19, 11, 16, 19, -1, 8, -1, -1, -1, 11, -1, -1, -1, -1, 30,
	 3, -1, -1, -1, -1, -1, -1},	/* I19 */
	{14, 5, 13, 17, 21, -1, 8, -1, -1, -1, 11, -1, -1, -1, -1, 30,
	 3, -1, -1, -1, -1, -1, -1},	/* I20 */
};

/**
* connection of register
*/
static const short connection_reg[INTER_CONN_INPUT_NUM][INTER_CONN_OUTPUT_NUM] = {
	/* 0    1    2    3    4    5    6    7    8    9    10    11    12    13    14    15
	   16   17   18   19   20   21   22 */
	{0x20, 0x20, 0x24, 0x24, 0x28, 0x28, -1, 0x5c, -1, 0x5c, 0x5c, -1, -1, 0x448, 0x448, 0x438,
	 0x438, 0x5c, 0x5c, 0x464, 0x464, -1, -1},	/* I00 */
	{0x20, 0x20, 0x24, 0x24, 0x28, -1, 0x28, 0x5c, 0x5c, 0x5c, 0x5c, -1, -1, 0x448, 0x448,
	 0x438, 0x438, 0x5c, 0x5c, 0x464, 0x464, -1},	/* I01 */
	{-1, 0x20, 0x24, 0x24, -1, -1, 0x30, 0x30, -1, -1, -1, 0x2c, -1, 0x448, 0x448, -1,
	 -1, 0x30, 0x30, -1, -1, -1, -1},	/* I02 */
	{0x20, 0x20, 0x24, 0x24, 0x28, 0x28, -1, 0x28, -1, 0x2C, -1, -1, -1, 0x448, 0x448, 0x438,
	 0x438, 0x30, -1, 0x464, 0x464, 0x5c, -1},	/* I03 */
	{0x20, 0x20, 0x24, 0x24, 0x28, -1, 0x28, -1, 0x28, -1, 0x2C, -1, -1, 0x448, 0x448, 0x438,
	 0x438, -1, 0x30, 0x464, 0x464, -1, 0xbc},	/* I04 */
	{0x20, 0x20, 0x24, 0x24, 0x28, 0x28, -1, 0x28, -1, 0x2C, -1, -1, -1, 0x420, 0x420, -1,
	 -1, 0x30, -1, 0x464, 0x464, -1, -1},	/* I05 */
	{0x20, 0x20, 0x24, 0x24, 0x28, -1, 0x28, -1, 0x28, -1, 0x2C, -1, 0x2C, 0x420, 0x420, -1,
	 -1, -1, 0x30, 0x464, 0x468, -1, -1},	/* I06 */
	{0x20, 0x20, 0x24, 0x24, 0x28, 0x28, -1, 0x28, -1, 0x2C, -1, -1, -1, 0x420, 0x420, -1,
	 -1, 0x30, -1, 0x464, 0x468, -1, -1},	/* I07 */
	{0x20, 0x20, 0x24, 0x24, 0x28, -1, 0x28, -1, 0x28, -1, 0x2C, -1, 0x2C, 0x420, 0x420, -1,
	 -1, -1, 0x30, 0x464, 0x468, -1, -1},	/* I08 */
	{0x20, 0x20, 0x24, 0x24, 0x28, 0x28, -1, -1, -1, 0x5c, -1, -1, 0x2C, 0x448, 0x448, 0x438,
	 0x438, 0x5c, 0x5c, 0x5c, 0x5c, -1, -1},	/* I09 */
	{0x420, -1, -1, 0x420, -1, 0x420, -1, 0x420, -1, -1, -1, -1, 0x448, -1, -1, -1,
	 -1, 0x420, -1, 0x448, -1, 0x448, 0x44c},	/* I10 */
	{-1, 0x420, -1, -1, 0x420, -1, 0x420, -1, 0x420, -1, -1, -1, 0x448, -1, -1, -1,
	 -1, -1, 0x420, -1, 0x448, 0x448, 0x44c},	/* I11 */
	{0x438, 0x438, -1, 0x438, -1, 0x438, -1, 0x438, -1, -1, -1, -1, -1, -1, -1, -1,
	 -1, 0x440, -1, 0x444, -1, 0x444, 0x444},	/* I12 */
	{-1, 0x438, -1, -1, 0x438, -1, 0x438, -1, 0x438, -1, -1, -1, -1, -1, -1, -1,
	 -1, -1, 0x440, -1, 0x444, 0x444, 0x444},	/* I13 */
	{0x2C, 0x2C, 0x2C, 0x2C, 0x30, 0x30, -1, 0x5c, 0x5c, 0x5c, -1, -1, 0x30, 0x448, 0x448,
	 0x438,
	 0x440, -1, -1, 0x5c, 0x5c, -1, -1},	/* I14 */
	{0x2C, 0x2C, 0x2C, 0x2C, 0x30, 0x30, -1, -1, -1, 0x30, -1, -1, -1, 0x448, 0x448, 0x438,
	 0x440, -1, -1, -1, -1, -1, -1},	/* I15 */
	{0x2C, 0x2C, 0x2C, 0x2C, 0x30, -1, 0x30, -1, -1, -1, 0x30, -1, -1, 0x448, 0x448, 0x438,
	 0x440, -1, -1, -1, -1, -1, -1},	/* I16 */
	{0x460, 0x2C, 0x30, 0x5c, 0x30, 0x460, 0x30, 0x460, -1, 0x460, 0x30, 0x464, -1, 0x448,
	 0x448, 0x438,
	 0x440, 0x5c, -1, 0x464, -1, 0xbc, -1},	/* I17 */
	{0x2C, 0x460, 0x30, 0x2C, 0x5c, -1, 0x460, -1, 0x460, -1, 0x464, -1, 0x464, 0x448, 0x448,
	 0x438,
	 0x440, -1, 0x5c, 0x464, -1, -1, 0xbc},	/* I18 */
	{0x460, 0x2C, 0x460, 0x460, 0x460, 0x460, 0x30, 0x460, -1, 0x460, 0x30, 0x464, -1, 0x448,
	 0x448, 0x438,
	 0x444, 0x464, -1, 0x5c, 0x5c, 0xbc, -1},	/* I19 */
	{0x2C, 0x460, 0x460, 0x460, 0x460, -1, 0x460, -1, 0x460, -1, 0x464, -1, 0x464, 0x448, 0x448,
	 0x438,
	 0x444, -1, 0x464, 0x5c, 0x5c, -1, 0xbc},	/* I20 */
};


/**
* shift connection of register
*/
static const short shift_connection_reg[INTER_CONN_INPUT_NUM][INTER_CONN_OUTPUT_NUM] = {
	/* 0    1    2    3    4    5    6    7    8    9    10    11    12    13    14    15
	   16    17    18   19   20   21   22 */
	{0x20, 0x20, 0x24, 0x24, 0x28, 0x30, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0x438,
	 -1, -1, -1, 0x464, 0x464, -1, -1},	/* I00 */
	{0x20, 0x20, 0x24, 0x24, 0x28, -1, 0x30, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	 0x440, -1, -1, 0x464, 0x464, -1, -1},	/* I01 */
	{-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	 -1, -1, -1, -1, -1, -1, -1},	/* I02 */
	{-1, -1, 0x30, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	 -1, -1, -1, -1, -1, -1, -1},	/* I03 */
	{-1, -1, 0x30, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	 -1, -1, -1, -1, -1, -1, -1},	/* I04 */
	{0x20, 0x20, 0x24, 0x24, 0x28, -1, -1, -1, -1, -1, -1, 0x2C, -1, -1, -1, -1,
	 -1, -1, -1, 0x464, 0x464, -1, -1},	/* I05 */
	{0x20, 0x20, 0x24, 0x24, 0x28, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	 -1, -1, -1, 0x464, -1, -1, -1},	/* I06 */
	{0x20, 0x20, 0x24, 0x24, 0x28, -1, -1, -1, -1, -1, -1, 0x2C, -1, -1, -1, -1,
	 -1, -1, -1, 0x464, -1, -1, -1},	/* I07 */
	{0x20, 0x20, 0x24, 0x24, 0x28, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	 -1, -1, -1, 0x464, -1, -1, -1},	/* I08 */
	{-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	 -1, -1, -1, -1, -1, -1, -1},	/* I09 */
	{0x420, -1, -1, 0x420, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	 -1, -1, -1, -1, -1, -1, -1},	/* I10 */
	{0x420, 0x420, -1, -1, 0x420, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	 -1, -1, -1, -1, -1, -1, -1},	/* I11 */
	{0x438, 0x438, -1, 0x438, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	 -1, -1, -1, -1, -1, -1, -1},	/* I12 */
	{-1, 0x438, -1, -1, 0x438, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	 -1, -1, -1, -1, -1, -1, -1},	/* I13 */
	{-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	 -1, -1, -1, -1, -1, -1, -1},	/* I14 */
	{0x2C, 0x2C, -1, 0x2C, 0x30, 0x30, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0x440,
	 -1, -1, -1, -1, -1, -1, -1},	/* I15 */
	{0x2C, 0x2C, -1, 0x2C, 0x30, -1, 0x30, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	 0x440, -1, -1, -1, -1, -1, -1},	/* I16 */
	{0x2C, 0x2C, 0xbc, 0x2C, 0x30, -1, 0x30, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	 0x440, -1, -1, -1, -1, -1, -1},	/* I17 */
	{0x2C, 0x2C, 0xbc, 0x2C, 0x30, -1, 0x30, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	 0x440, -1, -1, -1, -1, -1, -1},	/* I18 */
	{0x460, 0x2C, 0x460, 0x460, 0x460, -1, 0x30, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	 0x440, -1, -1, -1, -1, -1, -1},	/* I19 */
	{0x2C, 0x460, 0x460, 0x460, 0x460, -1, 0x30, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	 0x440, -1, -1, -1, -1, -1, -1},	/* I20 */
};

/**
* connection state of register
*/
static char connection_state[INTER_CONN_INPUT_NUM][INTER_CONN_OUTPUT_NUM] = {
	/* 0  1  2  3  4  5  6  7  8  9  10 11 12 13 14 15 16 17 18 19 20 21 22 */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},	/* I00 */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},	/* I01 */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},	/* I02 */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},	/* I03 */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},	/* I04 */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},	/* I05 */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},	/* I06 */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},	/* I07 */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},	/* I08 */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},	/* I09 */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},	/* I10 */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},	/* I11 */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},	/* I12 */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},	/* I13 */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},	/* I14 */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},	/* I15 */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},	/* I16 */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},	/* I17 */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},	/* I18 */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},	/* I19 */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},	/* I20 */
};

/**
* value of connection register to set for certain input
*/
const uint32_t hdmi_connection_value[HDMI_INTER_CONN_INPUT_NUM] = {
	/* I_30 I_31 I_32 I_33 I_34 I_35 I_36 I_37 */
	0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7
};

/**
* mask of connection register to set for certain output
*/
const uint32_t hdmi_connection_mask[HDMI_INTER_CONN_OUTPUT_NUM] = {
	/* O_30 O_31 O_32 O_33 O_34 O_35 O_36 O_37 O_38 O_39 O_40 O_41 */
	0x7, 0x7, 0x7, 0x7, 0x7, 0x7, 0x7, 0x7, 0x7, 0x7, 0x7, 0x7
};

/**
* shift bits of connection register to set for certain output
*/
const char hdmi_connection_shift_bits[HDMI_INTER_CONN_OUTPUT_NUM] = {
	/* O_30 O_31 O_32 O_33 O_34 O_35 O_36 O_37 O_38 O_39 O_40 O_41 */
	0, 3, 6, 9, 12, 15, 18, 21, 24, 27, 0, 3
};

/**
* connection state of HDMI
*/
char hdmi_connection_state[HDMI_INTER_CONN_INPUT_NUM][HDMI_INTER_CONN_OUTPUT_NUM] = {
	/* O_30 O_31 O_32 O_33 O_34 O_35 O_36 O_37 O_38 O_39 O_40 O_41 */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},	/* I_30 */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},	/* I_31 */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},	/* I_32 */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},	/* I_33 */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},	/* I_34 */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},	/* I_35 */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},	/* I_36 */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}	/* I_37 */
};

/**
* connection state of HDMI
*/
const uint32_t hdmi_connection_reg[HDMI_INTER_CONN_OUTPUT_NUM] = {
	/* O_30 O_31 O_32 O_33 */
	AFE_HDMI_CONN0, AFE_HDMI_CONN0, AFE_HDMI_CONN0, AFE_HDMI_CONN0,
	/* O_34 O_35 O_36 O_37 */
	AFE_HDMI_CONN0, AFE_HDMI_CONN0, AFE_HDMI_CONN0, AFE_HDMI_CONN0,
	/* O_38 O_39 O_40 O_41 */
	AFE_HDMI_CONN0, AFE_HDMI_CONN0, AFE_HDMI_CONN1, AFE_HDMI_CONN1
};

static bool check_bits_and_reg(short reg_addr, char bits)
{
	if (reg_addr <= 0 || bits < 0) {
		pr_warn("reg_addr = %x bits = %d\n", reg_addr, bits);
		return false;
	}
	return true;
}

int set_hdmi_interconnection(uint32_t connection_state, uint32_t input, uint32_t output)
{
	uint32_t input_index;
	uint32_t output_index;

	/* check if connection request is valid */
	if (input < HDMI_INTER_CONN_INPUT_BASE || input > HDMI_INTER_CONN_INPUT_MAX ||
	    output < HDMI_INTER_CONN_OUTPUT_BASE || output > HDMI_INTER_CONN_OUTPUT_MAX) {
		return -1;
	}

	input_index = input - HDMI_INTER_CONN_INPUT_BASE;
	output_index = output - HDMI_INTER_CONN_OUTPUT_BASE;

	/* do connection */
	switch (connection_state) {
	case INTER_CONNECT:
		{
			uint32_t reg_value = hdmi_connection_value[input_index];
			uint32_t reg_mask = hdmi_connection_mask[output_index];
			uint32_t reg_shift_bits = hdmi_connection_shift_bits[output_index];
			uint32_t reg_conn = hdmi_connection_reg[output_index];

			mt_afe_set_reg(reg_conn, reg_value << reg_shift_bits,
				    reg_mask << reg_shift_bits);
			hdmi_connection_state[input_index][output_index] = INTER_CONNECT;
			break;
		}
	case INTER_DISCONNECT:
		{
			uint32_t reg_value = hdmi_connection_value[INTER_CONN_I37 -
								   HDMI_INTER_CONN_INPUT_BASE];
			uint32_t reg_mask = hdmi_connection_mask[output_index];
			uint32_t reg_shift_bits = hdmi_connection_shift_bits[output_index];
			uint32_t reg_conn = hdmi_connection_reg[output_index];

			mt_afe_set_reg(reg_conn, reg_value << reg_shift_bits,
				    reg_mask << reg_shift_bits);
			hdmi_connection_state[input_index][output_index] = INTER_DISCONNECT;
			break;
		}
	default:
		break;
	}

	return 0;
}


int mt_afe_set_connection(uint32_t state, uint32_t input, uint32_t output)
{
	int connect_bit = 0;
	int connect_reg = 0;

	pr_debug("%s state = %d input = %d output = %d\n", __func__, state, input, output);

	if (input >= HDMI_INTER_CONN_INPUT_BASE || output >= HDMI_INTER_CONN_OUTPUT_BASE)
		return set_hdmi_interconnection(state, input, output);

	if (unlikely(input >= INTER_CONN_INPUT_NUM || output >= INTER_CONN_OUTPUT_NUM)) {
		pr_warn("%s out of bound connection connection_table[%d][%d]\n", __func__, input,
			output);
		return -1;
	}

	if (unlikely(connection_table[input][output] < 0 || connection_table[input][output] == 0)) {
		pr_warn("%s no connection connection_table[%d][%d] = %d\n",
			__func__, input, output, connection_table[input][output]);
		return -1;
	}

	switch (state) {
	case INTER_DISCONNECT:
		if ((connection_state[input][output] &
		     INTER_CONNECT) == INTER_CONNECT) {
			/* here to disconnect connect bits */
			connect_bit = connection_bits[input][output];
			connect_reg = connection_reg[input][output];
			if (check_bits_and_reg(connect_reg, connect_bit)) {
				mt_afe_set_reg(connect_reg, 0 << connect_bit, 1 << connect_bit);
				connection_state[input][output] &= ~(INTER_CONNECT);
			}
		}
		if ((connection_state[input][output] &
		     INTER_CONNECT_SHIFT) == INTER_CONNECT_SHIFT) {
			/* here to disconnect connect shift bits */
			connect_bit = shift_connection_bits[input][output];
			connect_reg = shift_connection_reg[input][output];
			if (check_bits_and_reg(connect_reg, connect_bit)) {
				mt_afe_set_reg(connect_reg, 0 << connect_bit, 1 << connect_bit);
				connection_state[input][output] &= ~(INTER_CONNECT_SHIFT);
			}
		}
		break;
	case INTER_CONNECT:
		connect_bit = connection_bits[input][output];
		connect_reg = connection_reg[input][output];
		if (check_bits_and_reg(connect_reg, connect_bit)) {
			mt_afe_set_reg(connect_reg, 1 << connect_bit, 1 << connect_bit);
			pr_debug("%s set register(0x%x)[%d] to I%u->O%u connection\n",
				 __func__, connect_reg, connect_bit, input, output);
			connection_state[input][output] |= INTER_CONNECT;
		}
		break;
	case INTER_CONNECT_SHIFT:
		if ((connection_table[input][output] &
		     INTER_CONNECT_SHIFT) != INTER_CONNECT_SHIFT) {
			pr_warn("%s don't support shift opeartion\n", __func__);
			break;
		}
		connect_bit = shift_connection_bits[input][output];
		connect_reg = shift_connection_reg[input][output];
		if (check_bits_and_reg(connect_reg, connect_bit)) {
			mt_afe_set_reg(connect_reg, 1 << connect_bit, 1 << connect_bit);
			connection_state[input][output] |= INTER_CONNECT_SHIFT;
		}
		break;
	default:
		pr_warn("%s no this state state = %d\n", __func__, state);
		break;
	}
	return 0;
}
