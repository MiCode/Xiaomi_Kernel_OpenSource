/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

/*
 * implement your own function "function_name" for the type:
 * void function_name(unsigned long *vaddr, unsigned long *size)
 * then add one line here just like the sample code below
 * EXTRA_MISC(function_name, "DBFILENAME", MAX_SIZE)
 * a file "SYS_EXTRA_DBFILENAME_RAW" will be generated
 * ensure size return by the function is <= MAX_SIZE
 * otherwise this file will not be generated i.e. ignored
 * MAX_SIZE should be as less as possible and dozens of kb is preferred
 * NOTE: end-of-line DON'T add comma
 */
/* e.g. */
/* EXTRA_MISC(test_func, "TESTNAME", (4 * 1024)) */
EXTRA_MISC(get_msdc_aee_buffer, "MSDC", (512 * 1024))
EXTRA_MISC(get_ufs_aee_buffer, "UFS", (100 * 1024))
EXTRA_MISC(get_blockio_aee_buffer, "BLOCKIO", (300 * 1024))
EXTRA_MISC(get_ccci_aee_buffer, "CCCI", (300 * 1024))
