/*
 * Copyright (C) 2017 MediaTek Inc.
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

#ifndef __MUSBFSH_LINUX_PLATFORM_ARCH_H__
#define __MUSBFSH_LINUX_PLATFORM_ARCH_H__

#include <linux/io.h>
#include <linux/spinlock.h>
extern spinlock_t musbfs_io_lock;
extern void mt65xx_usb11_clock_enable(bool enable);
extern bool musbfsh_power;
/* NOTE:  these offsets are all in bytes */

static inline u16 musbfsh_readw(const void __iomem *addr, unsigned int offset)
{
	u16 rc = 0;

	if (musbfsh_power)
		rc = readw(addr + offset);
	else {
		unsigned long flags = 0;

		spin_lock_irqsave(&musbfs_io_lock, flags);
		mt65xx_usb11_clock_enable(true);
		rc = readw(addr + offset);
		mt65xx_usb11_clock_enable(false);
		spin_unlock_irqrestore(&musbfs_io_lock, flags);
	}
	return rc;
}

static inline u32 musbfsh_readl(const void __iomem *addr, unsigned int offset)
{
	u32 rc = 0;

	if (musbfsh_power)
		rc = readl(addr + offset);
	else {
		unsigned long flags = 0;

		spin_lock_irqsave(&musbfs_io_lock, flags);
		mt65xx_usb11_clock_enable(true);
		rc = readl(addr + offset);
		mt65xx_usb11_clock_enable(false);
		spin_unlock_irqrestore(&musbfs_io_lock, flags);
	}
	return rc;
}


static inline void musbfsh_writew(void __iomem *addr, unsigned int offset,
				  u16 data)
{
	if (musbfsh_power)
		writew(data, addr + offset);
	else {
		unsigned long flags = 0;

		spin_lock_irqsave(&musbfs_io_lock, flags);
		mt65xx_usb11_clock_enable(true);
		writew(data, addr + offset);
		mt65xx_usb11_clock_enable(false);
		spin_unlock_irqrestore(&musbfs_io_lock, flags);
	}
}

static inline void musbfsh_writel(void __iomem *addr, unsigned int offset,
				  u32 data)
{
	if (musbfsh_power)
		writel(data, addr + offset);
	else {
		unsigned long flags = 0;

		spin_lock_irqsave(&musbfs_io_lock, flags);
		mt65xx_usb11_clock_enable(true);
		writel(data, addr + offset);
		mt65xx_usb11_clock_enable(false);
		spin_unlock_irqrestore(&musbfs_io_lock, flags);
	}
}

static inline u8 musbfsh_readb(const void __iomem *addr, unsigned int offset)
{
	u8 rc = 0;

	if (musbfsh_power)
		rc = readb(addr + offset);
	else {
		unsigned long flags = 0;

		spin_lock_irqsave(&musbfs_io_lock, flags);
		mt65xx_usb11_clock_enable(true);
		rc = readb(addr + offset);
		mt65xx_usb11_clock_enable(false);
		spin_unlock_irqrestore(&musbfs_io_lock, flags);
	}
	return rc;
}

static inline void musbfsh_writeb(void __iomem *addr, unsigned int offset,
				  u8 data)
{
	if (musbfsh_power)
		writeb(data, addr + offset);
	else {
		unsigned long flags = 0;

		spin_lock_irqsave(&musbfs_io_lock, flags);
		mt65xx_usb11_clock_enable(true);
		writeb(data, addr + offset);
		mt65xx_usb11_clock_enable(false);
		spin_unlock_irqrestore(&musbfs_io_lock, flags);
	}
}

/* NOTE:  these offsets are all in bytes */

#if 0
static inline u16 musbfsh_readw(const void __iomem *addr, unsigned int offset)
{
	return readw(addr + offset);
}

static inline u32 musbfsh_readl(const void __iomem *addr, unsigned int offset)
{
	return readl(addr + offset);
}

static inline void musbfsh_writew(void __iomem *addr, unsigned int offset,
				  u16 data)
{
	writew(data, addr + offset);
}

static inline void musbfsh_writel(void __iomem *addr, unsigned int offset,
				  u32 data)
{
	writel(data, addr + offset);
}

static inline u8 musbfsh_readb(const void __iomem *addr, unsigned int offset)
{
	return readb(addr + offset);
}

static inline void musbfsh_writeb(void __iomem *addr, unsigned int offset,
				  u8 data)
{
	writeb(data, addr + offset);
}
#endif

#endif
