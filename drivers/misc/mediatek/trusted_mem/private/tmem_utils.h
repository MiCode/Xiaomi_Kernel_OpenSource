/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef TMEM_UTILS_H
#define TMEM_UTILS_H

#define REGMGR_REGION_DEFER_OFF_DELAY_MS (1000)
#define REGMGR_REGION_DEFER_OFF_OPERATION_LATENCY_MS (500)
#define REGMGR_REGION_DEFER_OFF_DONE_DELAY_MS                                  \
	(REGMGR_REGION_DEFER_OFF_DELAY_MS                                      \
	 + REGMGR_REGION_DEFER_OFF_OPERATION_LATENCY_MS)

#define UNUSED(x) ((void)x)
#define VALID(ptr) (ptr != NULL)
#define INVALID(ptr) (ptr == NULL)
#define IS_ZERO(val) (val == 0)
#define INVALID_ADDR(addr) (addr == 0)
#define INVALID_SIZE(size) (size == 0)

#define COMPILE_ASSERT(condition) ((void)sizeof(char[1 - 2 * !!!(condition)]))

/* clang-format off */
#define SIZE_512B     0x00000200    /* 512B  */
#define SIZE_1K       0x00000400    /* 1K    */
#define SIZE_2K       0x00000800    /* 2K    */
#define SIZE_4K       0x00001000    /* 4K    */
#define SIZE_8K       0x00002000    /* 8K    */
#define SIZE_16K      0x00004000    /* 16K   */
#define SIZE_32K      0x00008000    /* 32K   */
#define SIZE_64K      0x00010000    /* 64K   */
#define SIZE_128K     0x00020000    /* 128K  */
#define SIZE_256K     0x00040000    /* 256K  */
#define SIZE_512K     0x00080000    /* 512K  */
#define SIZE_1M       0x00100000    /* 1M    */
#define SIZE_2M       0x00200000    /* 2M    */
#define SIZE_4M       0x00400000    /* 4M    */
#define SIZE_8M       0x00800000    /* 8M    */
#define SIZE_16M      0x01000000    /* 16M   */
#define SIZE_32M      0x02000000    /* 32M   */
#define SIZE_64M      0x04000000    /* 64M   */
#define SIZE_96M      0x06000000    /* 96M   */
#define SIZE_128M     0x08000000    /* 128M  */
#define SIZE_256M     0x10000000    /* 256M  */
#define SIZE_320M     0x14000000    /* 320M  */
#define SIZE_512M     0x20000000    /* 512M  */
/* clang-format on */

#define GET_TIME_DIFF_SEC_P(start, end)                                        \
	(int)((end->tv_usec > start->tv_usec)                                  \
		      ? (end->tv_sec - start->tv_sec)                          \
		      : (end->tv_sec - start->tv_sec - 1))

#define GET_TIME_DIFF_USEC_P(start, end)                                       \
	(int)((end->tv_usec > start->tv_usec)                                  \
		      ? (end->tv_usec - start->tv_usec)                        \
		      : (1000000 + end->tv_usec - start->tv_usec))

#endif /* end of TMEM_UTILS_H */
