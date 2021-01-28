/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef MT_SYNC_WRITE_H
#define MT_SYNC_WRITE_H

#if defined(__KERNEL__)

#include <linux/io.h>
#include <asm/cacheflush.h>

/*
 * Define macros.
 */
#define mt_reg_sync_writel(v, a) \
	do {    \
		__raw_writel((v), (void __force __iomem *)((a)));   \
		dsb();  \
	} while (0)
#endif

#define mt_reg_sync_writew(v, a) \
	do {    \
		__raw_writew((v), (void __force __iomem *)((a)));   \
		dsb();  \
	} while (0)
#endif

#define mt_reg_sync_writeb(v, a) \
	do {    \
		__raw_writeb((v), (void __force __iomem *)((a)));   \
		dsb();  \
	} while (0)
#endif

#ifdef CONFIG_64BIT
#define mt_reg_sync_writeq(v, a) \
	do {    \
		__raw_writeq((v), (void __force __iomem *)((a)));   \
		dsb();  \
	} while (0)
#endif

#else				/* __KERNEL__ */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#define mt_reg_sync_writel(v, a)        mt65xx_reg_sync_writel(v, a)
#define mt_reg_sync_writew(v, a)        mt65xx_reg_sync_writew(v, a)
#define mt_reg_sync_writeb(v, a)        mt65xx_reg_sync_writeb(v, a)

#define dsb() \
	{    \
		__asm__ __volatile__("dsb sy" : : : "memory"); \
	}

#define mt65xx_reg_sync_writel(v, a) \
	do {    \
		*(volatile unsigned int *)(a) = (v);    \
		dsb(); \
	} while (0)

#define mt65xx_reg_sync_writew(v, a) \
	do {    \
		*(volatile unsigned short *)(a) = (v);    \
		dsb(); \
	} while (0)

#define mt65xx_reg_sync_writeb(v, a) \
	do {    \
		*(volatile unsigned char *)(a) = (v);    \
		dsb(); \
	} while (0)

#endif				/* __KERNEL__ */

#endif				/* !MT_SYNC_WRITE_H */
