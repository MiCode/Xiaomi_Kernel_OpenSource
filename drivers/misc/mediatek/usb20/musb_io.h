/*
 * MUSB OTG driver register I/O
 *
 * Copyright 2005 Mentor Graphics Corporation
 * Copyright (C) 2005-2006 by Texas Instruments
 * Copyright (C) 2006-2007 Nokia Corporation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 * NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef __MUSB_LINUX_PLATFORM_ARCH_H__
#define __MUSB_LINUX_PLATFORM_ARCH_H__

#include <linux/io.h>
#include <linux/spinlock.h>

extern bool mtk_usb_power;
#ifdef CONFIG_MTK_MUSB_PORT0_LOWPOWER_MODE
extern void mt_usb_clock_prepare(void);
extern void mt_usb_clock_unprepare(void);
#endif
extern bool usb_enable_clock(bool enable);
extern spinlock_t usb_io_lock;

static inline u16 musb_readw(const void __iomem *addr, unsigned int offset)
{
	u16 rc = 0;

	if (likely(mtk_usb_power)) {
		rc = readw(addr + offset);
	} else {
		unsigned long flags = 0;

		#ifdef CONFIG_MTK_MUSB_PORT0_LOWPOWER_MODE
		mt_usb_clock_prepare();
		#endif
		spin_lock_irqsave(&usb_io_lock, flags);
		usb_enable_clock(true);
		DBG(1, "[MUSB]:access %s function when usb clock is off 0x%X\n",
			__func__, offset);
		rc = readw(addr + offset);
		usb_enable_clock(false);
		spin_unlock_irqrestore(&usb_io_lock, flags);
		#ifdef CONFIG_MTK_MUSB_PORT0_LOWPOWER_MODE
		mt_usb_clock_unprepare();
		#endif
	}
	return rc;
}

static inline u32
	musb_readl(const void __iomem *addr, unsigned int offset)
{
	u32 rc = 0;

	if (likely(mtk_usb_power)) {
		rc = readl(addr + offset);
	} else {
		unsigned long flags = 0;

		#ifdef CONFIG_MTK_MUSB_PORT0_LOWPOWER_MODE
		mt_usb_clock_prepare();
		#endif
		spin_lock_irqsave(&usb_io_lock, flags);
		usb_enable_clock(true);
		DBG(1, "[MUSB]:access %s function when usb clock is off 0x%X\n",
			__func__, offset);
		rc = readl(addr + offset);
		usb_enable_clock(false);
		spin_unlock_irqrestore(&usb_io_lock, flags);
		#ifdef CONFIG_MTK_MUSB_PORT0_LOWPOWER_MODE
		mt_usb_clock_unprepare();
		#endif
	}
	return rc;
}


static inline void
	musb_writew(void __iomem *addr, unsigned int offset, u16 data)
{
	if (likely(mtk_usb_power)) {
		writew(data, addr + offset);
	} else {
		unsigned long flags = 0;

		#ifdef CONFIG_MTK_MUSB_PORT0_LOWPOWER_MODE
		mt_usb_clock_prepare();
		#endif
		spin_lock_irqsave(&usb_io_lock, flags);
		usb_enable_clock(true);
		DBG(1, "[MUSB]:access %s function when usb clock is off 0x%X\n",
			__func__, offset);
		writew(data, addr + offset);
		usb_enable_clock(false);
		spin_unlock_irqrestore(&usb_io_lock, flags);
		#ifdef CONFIG_MTK_MUSB_PORT0_LOWPOWER_MODE
		mt_usb_clock_unprepare();
		#endif
	}
}

static inline void
	musb_writel(void __iomem *addr, unsigned int offset, u32 data)
{
	if (likely(mtk_usb_power)) {
		writel(data, addr + offset);
	} else {
		unsigned long flags = 0;

		#ifdef CONFIG_MTK_MUSB_PORT0_LOWPOWER_MODE
		mt_usb_clock_prepare();
		#endif
		spin_lock_irqsave(&usb_io_lock, flags);
		usb_enable_clock(true);
		DBG(1, "[MUSB]:access %s function when usb clock is off 0x%X\n",
			__func__, offset);
		writel(data, addr + offset);
		usb_enable_clock(false);
		spin_unlock_irqrestore(&usb_io_lock, flags);
		#ifdef CONFIG_MTK_MUSB_PORT0_LOWPOWER_MODE
		mt_usb_clock_unprepare();
		#endif
	}
}

static inline u8 musb_readb(const void __iomem *addr, unsigned int offset)
{
	u8 rc = 0;

	if (likely(mtk_usb_power)) {
		rc = readb(addr + offset);
	} else {
		unsigned long flags = 0;

		#ifdef CONFIG_MTK_MUSB_PORT0_LOWPOWER_MODE
		mt_usb_clock_prepare();
		#endif
		spin_lock_irqsave(&usb_io_lock, flags);
		usb_enable_clock(true);
		DBG(1, "[MUSB]:access %s function when usb clock is off 0x%X\n",
			__func__, offset);
		rc = readb(addr + offset);
		usb_enable_clock(false);
		spin_unlock_irqrestore(&usb_io_lock, flags);
		#ifdef CONFIG_MTK_MUSB_PORT0_LOWPOWER_MODE
		mt_usb_clock_unprepare();
		#endif
	}
	return rc;
}

static inline void musb_writeb
	(void __iomem *addr, unsigned int offset, u8 data)
{
	if (likely(mtk_usb_power)) {
		writeb(data, addr + offset);
	} else {
		unsigned long flags = 0;

		#ifdef CONFIG_MTK_MUSB_PORT0_LOWPOWER_MODE
		mt_usb_clock_prepare();
		#endif
		spin_lock_irqsave(&usb_io_lock, flags);
		usb_enable_clock(true);
		DBG(1, "[MUSB]:access %s function when usb clock is off 0x%X\n",
			__func__, offset);
		writeb(data, addr + offset);
		usb_enable_clock(false);
		spin_unlock_irqrestore(&usb_io_lock, flags);
		#ifdef CONFIG_MTK_MUSB_PORT0_LOWPOWER_MODE
		mt_usb_clock_unprepare();
		#endif
	}
}

#endif
