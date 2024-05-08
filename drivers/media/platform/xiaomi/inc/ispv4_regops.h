// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2022 Xiaomi, Inc.
 */

#ifndef _LINUX_XM_REGOPS_H
#define _LINUX_XM_REGOPS_H

#include <linux/types.h>
int ispv4_regops_read(u32 addr, u32 *valp);
int ispv4_regops_dread(u32 addr, u32 *valp);
int ispv4_regops_write(u32 addr, u32 data);
int ispv4_regops_w8dw(u32 addr, u32 *data);
int ispv4_regops_w64dw(u32 addr, u32 *data);

int ispv4_regops_burst_write(u32 addr, u8 *data, u32 len);
int ispv4_regops_long_burst_write(u32 addr, u8 *data, u32 len);
int ispv4_regops_set(u32 addr, u32 data, u32 len);
int ispv4_regops_clear_and_set(u32 addr, u32 mask, u32 value);

int ispv4_regops_inner_read(u8 offset, u32 *valp);

int ispv4_regops_get_speed(void);
int ispv4_regops_set_speed(u32 sp);

#define ISPV4_SPI_LOW_SP 1000000
#define ISPV4_SPI_HIGH_SP 32000000

#define ispv4_set_lowsp() ispv4_regops_set_speed(ISPV4_SPI_LOW_SP)
#define ispv4_set_highsp() ispv4_regops_set_speed(ISPV4_SPI_HIGH_SP)

#include <linux/printk.h>

/**
 * REGOPS_NO_DUMP - Prevent regops dump debug message.
 *
 * Example
 * #define REGOPS_NO_DUMP
 * #include <linux/xm_regops.h>
 * ......
 */
#ifdef REGOPS_NO_DUMP
#undef pr_info
#undef pr_err
#define pr_info(...)
#define pr_err(...)
#endif

/**
 * REGOPS_ERROR - identify a golbal var to record error-num.
 *
 * Example
 * #define REGOPS_ERROR g_xxx_regops_err
 * #include <linux/xm_regops.h>
 * ......
 * static int g_xxx_regoops_err = 0;
 */
#ifdef REGOPS_ERROR
#define REGOPS_RECORD_ERROR(x)                                                 \
	({                                                                     \
		do {                                                           \
			REGOPS_ERROR = x;                                      \
		} while (0);                                                   \
	})
#else
#define REGOPS_RECORD_ERROR(x)
#endif

#define putreg32(val, address)                                                  \
	({                                                                      \
		u32 addr = address;                                             \
		do {                                                            \
			int ret;                                                \
			ret = ispv4_regops_write(addr, val);                    \
			if (ret != 0) {                                         \
				pr_err("Regops write 0x%x failed(%d)! %s:%d\n", \
				       addr, ret, __FUNCTION__, __LINE__);      \
				REGOPS_RECORD_ERROR(ret);                       \
			}                                                       \
		} while (0);                                                    \
	})

#define getreg32(address)                                                      \
	({                                                                     \
		u32 val = 0;                                                   \
		u32 addr = address;                                            \
		do {                                                           \
			int ret;                                               \
			ret = ispv4_regops_read(addr, &val);                   \
			if (ret != 0) {                                        \
				pr_err("Regops read 0x%x failed(%d)! %s:%d\n", \
				       addr, ret, __FUNCTION__, __LINE__);     \
				REGOPS_RECORD_ERROR(ret);                      \
			}                                                      \
		} while (0);                                                   \
		val;                                                           \
	})

#define clear_set_reg32(value, mask, address)                                  \
	({                                                                     \
		u32 val = 0;                                                   \
		u32 ad = address;                                              \
		do {                                                           \
			val = getreg32(ad);                                    \
			val &= ~mask;                                          \
			val |= (value & mask);                                 \
			putreg32(val, ad);                                     \
		} while (0);                                                   \
	})

#define polling_reg32(value, mask, address)                                    \
	({                                                                     \
		u32 val = 0;                                                   \
		u32 ad = address;                                              \
		u32 ret = -1;                                                  \
		u32 timeout = 1000;                                            \
		do {                                                           \
			val = getreg32(ad);                                    \
			if ((val & mask) == value) {                           \
				ret = 0;                                       \
				pr_info("Regops polling 0x%x done! %s:%d\n",   \
				       ad,  __FUNCTION__, __LINE__);           \
				break;                                         \
			}                                                      \
		} while (timeout--);                                           \
		if (ret) {                                                     \
			pr_err("Regops polling 0x%x failed! %s:%d\n",          \
				ad,  __FUNCTION__, __LINE__);                  \
		}                                                              \
		ret;                                                           \
	})
#endif
