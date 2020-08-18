/*
 * Copyright (C) 2015 MediaTek Inc.
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

#ifndef _DRV_COMM_H
#define _DRV_COMM_H
#include "mu3d_hal_hw.h"
#include <linux/io.h>

#undef EXTERN

#ifdef _DRV_COMM_H
#define EXTERN
#else
#define EXTERN \
extern
#endif


/* CONSTANTS */

#ifndef FALSE
#define FALSE   0
#endif

#ifndef TRUE
#define TRUE    1
#endif

/* TYPES */

#define DEV_UINT32 unsigned int
#define DEV_INT32 int
#define DEV_UINT16 unsigned short
#define DEV_INT16 short
#define DEV_UINT8 unsigned char
#define DEV_INT8 char

enum USB_RESULT {
	RET_SUCCESS = 0,
	RET_FAIL,
};

#ifdef NEVER
#define os_writelmsk(addr, data, msk) \
	{ os_writel(addr, ((os_readl(addr) & ~(msk)) | ((data) & (msk)))); \
	}
#define os_setmsk(addr, msk) \
	{ os_writel(addr, os_readl(addr) | msk); \
	}
#define os_clrmsk(addr, msk) \
	{ os_writel(addr, os_readl(addr) & ~msk); \
	}
/*msk the data first, then umsk with the umsk.*/
#define os_writelmskumsk(addr, data, msk, umsk) \
{\
	os_writel(addr, ((os_readl(addr) & ~(msk)) | ((data) & (msk))) & (umsk));\
}

#define USB_END_OFFSET(_bEnd, _bOffset)	((0x10*(_bEnd-1)) + _bOffset)
#define USB_ReadCsr32(_bOffset, _bEnd) os_readl(USB_END_OFFSET(_bEnd, _bOffset))
#define USB_WriteCsr32(_bOffset, _bEnd, _bData) os_writel(USB_END_OFFSET(_bEnd, _bOffset), _bData)
#else
static inline void os_writeb(void __iomem *addr, unsigned char data)
{
	writeb(data, (void __iomem *)addr);
	if (0)
		pr_debug("%s writeb [%p] = 0x%08x\n", __func__, (void *)addr, data);
}

static inline void os_writew(void __iomem *addr, unsigned short data)
{
	writew(data, (void __iomem *)addr);
	if (0)
		pr_debug("%s writew [%p] = 0x%08x\n", __func__, (void *)addr, data);
}

static inline void os_writel(void __iomem *addr, unsigned int data)
{
	writel(data, (void __iomem *)addr);
	if (0)
		pr_debug("%s writel [%p] = 0x%08x\n", __func__, (void *)addr, data);
}

#define os_readl(addr)  readl((void __iomem *)((unsigned long)addr))

static inline void os_writelmsk(void __iomem *addr, unsigned int data, unsigned int msk)
{
	unsigned int tmp = readl((void __iomem *)addr);

	mb();	/* avoid context switch */
	writel(((tmp & ~(msk)) | ((data) & (msk))), (void __iomem *)addr);
}

static inline void os_setmsk(void __iomem *addr, unsigned int msk)
{
	unsigned int tmp = readl((void __iomem *)addr);

	if (0)
		pr_debug("%s setmsk [%p] = 0x%08x\n", __func__, (void *)addr, tmp);
	mb();	/* avoid context switch */
	writel((tmp | msk), (void __iomem *)addr);
	if (0)
		pr_debug("%s setmsk [%p] = 0x%08x\n", __func__, (void *)addr,
			 readl((void __iomem *)addr));
}

static inline void os_clrmsk(void __iomem *addr, unsigned int msk)
{
	unsigned int tmp = readl((void __iomem *)addr);

	if (0)
		pr_debug("%s clrmsk [%p] = 0x%08x\n", __func__, (void *)addr, tmp);
	mb();	/* avoid context switch */
	writel((tmp & ~(msk)), (void __iomem *)addr);
	if (0)
		pr_debug("%s clrmsk [%p] = 0x%08x\n", __func__, (void *)addr,
			 readl((void __iomem *)addr));
}

/*msk the data first, then umsk with the umsk.*/
static inline void os_writelmskumsk(void __iomem *addr, unsigned int data,
				    unsigned int msk, unsigned int umsk)
{
	unsigned int tmp = readl((void __iomem *)addr);

	mb();	/* avoid context switch */
	writel(((tmp & ~(msk)) | ((data) & (msk))) & (umsk), (void __iomem *)addr);
}

static inline int wait_for_value(void __iomem *addr, unsigned int msk,
				 unsigned int value, unsigned int ms_intvl, unsigned int count)
{
	u32 i;

	for (i = 0; i < count; i++) {
		if ((os_readl(addr) & msk) == value)
			return RET_SUCCESS;
		mb();	/* avoid context switch */
		mdelay(ms_intvl);
	}
	return RET_FAIL;
}

static inline int wait_for_value_us(void __iomem *addr, unsigned int msk,
				    unsigned int value, unsigned int us_intvl, unsigned int count)
{
	u32 i;

	for (i = 0; i < count; i++) {
		if ((os_readl(addr) & msk) == value)
			return RET_SUCCESS;
		mb();	/* avoid context switch */
		udelay(us_intvl);
	}
	return RET_FAIL;
}

#define USB_END_OFFSET(_bEnd, _bOffset)	((0x10*(_bEnd-1)) + _bOffset)

#define USB_ReadCsr32(_bOffset, _bEnd) \
			readl((void __iomem *)(uintptr_t)(USB_END_OFFSET(_bEnd, _bOffset)))

#define USB_WriteCsr32(_bOffset, _bEnd, _bData) \
			do {\
				writel(_bData, (void __iomem *)(uintptr_t)(USB_END_OFFSET(_bEnd, _bOffset)));\
				mb(); /* avoid context switch */ \
			} while (0)

#endif

#define div_and_rnd_up(x, y) (((x) + (y) - 1) / (y))

#endif	 /*_DRV_COMM_H*/
