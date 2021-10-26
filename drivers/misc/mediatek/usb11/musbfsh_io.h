/*
 * MUSB OTG driver register I/O
 *
 * Copyright 2005 Mentor Graphics Corporation
 * Copyright (C) 2005-2006 by Texas Instruments
 * Copyright (C) 2006-2007 Nokia Corporation
 *
 * Copyright 2015 Mediatek Inc.
 *	Marvin Lin <marvin.lin@mediatek.com>
 *	Arvin Wang <arvin.wang@mediatek.com>
 *	Vincent Fan <vincent.fan@mediatek.com>
 *	Bryant Lu <bryant.lu@mediatek.com>
 *	Yu-Chang Wang <yu-chang.wang@mediatek.com>
 *	Macpaul Lin <macpaul.lin@mediatek.com>
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
 * You should have received a copy of the GNU General Public License
 * along with this program.
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

#ifndef __MUSBFSH_LINUX_PLATFORM_ARCH_H__
#define __MUSBFSH_LINUX_PLATFORM_ARCH_H__

#include <linux/io.h>
#include <linux/spinlock.h>
#include "musbfsh_core.h"
extern spinlock_t musbfs_io_lock;
extern void mt65xx_usb11_clock_enable(bool enable);
extern bool musbfsh_power;
#ifdef CONFIG_MUSBFSH_IDDIG
extern int mt_usb11_clock_prepare(void);
extern void mt_usb11_clock_unprepare(void);
#endif
/* NOTE:  these offsets are all in bytes */

static inline u16 musbfsh_readw(const void __iomem *addr, unsigned int offset)
{
	u16 rc = 0;

	if (musbfsh_power) {
		rc = readw(addr + offset);
	} else {
		unsigned long flags = 0;

		#ifdef CONFIG_MUSBFSH_IDDIG
		mt_usb11_clock_prepare();
		#endif
		spin_lock_irqsave(&musbfs_io_lock, flags);
		mt65xx_usb11_clock_enable(true);
		/*
		 * DBG(0, "[MUSBfsh]: access %s FUNC: USB CLK is off 0x%X\n",
		 * __func__, offset);
		 */
		rc = readw(addr + offset);
		mt65xx_usb11_clock_enable(false);
		spin_unlock_irqrestore(&musbfs_io_lock, flags);
		#ifdef CONFIG_MUSBFSH_IDDIG
		mt_usb11_clock_unprepare();
		#endif
	}
	return rc;
}

static inline u32 musbfsh_readl(const void __iomem *addr, unsigned int offset)
{
	u32 rc = 0;

	if (musbfsh_power) {
		rc = readl(addr + offset);
	} else {
		unsigned long flags = 0;

		#ifdef CONFIG_MUSBFSH_IDDIG
		mt_usb11_clock_prepare();
		#endif
		spin_lock_irqsave(&musbfs_io_lock, flags);
		mt65xx_usb11_clock_enable(true);
		/*
		 * DBG(0, "[MUSBfsh]: access %s FUNC: USB CLK is off 0x%X\n",
		 * __func__, offset);
		 */
		rc = readl(addr + offset);
		mt65xx_usb11_clock_enable(false);
		spin_unlock_irqrestore(&musbfs_io_lock, flags);
		#ifdef CONFIG_MUSBFSH_IDDIG
		mt_usb11_clock_unprepare();
		#endif
	}
	return rc;
}


static inline void
musbfsh_writew(void __iomem *addr, unsigned int offset, u16 data)
{
	if (musbfsh_power) {
		writew(data, addr + offset);
	} else {
		unsigned long flags = 0;

		#ifdef CONFIG_MUSBFSH_IDDIG
		mt_usb11_clock_prepare();
		#endif
		spin_lock_irqsave(&musbfs_io_lock, flags);
		mt65xx_usb11_clock_enable(true);
		/*
		 * DBG(0, "[MUSBfsh]: access %s FUNC: USB CLK is off 0x%X\n",
		 * __func__, offset);
		 */
		writew(data, addr + offset);
		mt65xx_usb11_clock_enable(false);
		spin_unlock_irqrestore(&musbfs_io_lock, flags);
		#ifdef CONFIG_MUSBFSH_IDDIG
		mt_usb11_clock_unprepare();
		#endif
	}
}

static inline void
musbfsh_writel(void __iomem *addr, unsigned int offset, u32 data)
{
	if (musbfsh_power) {
		writel(data, addr + offset);
	} else {
		unsigned long flags = 0;

		#ifdef CONFIG_MUSBFSH_IDDIG
		mt_usb11_clock_prepare();
		#endif
		spin_lock_irqsave(&musbfs_io_lock, flags);
		mt65xx_usb11_clock_enable(true);
		/*
		 * DBG(0, "[MUSBfsh]: access %s FUNC: USB CLK is off 0x%X\n",
		 * __func__, offset);
		 */
		writel(data, addr + offset);
		mt65xx_usb11_clock_enable(false);
		spin_unlock_irqrestore(&musbfs_io_lock, flags);
		#ifdef CONFIG_MUSBFSH_IDDIG
		mt_usb11_clock_unprepare();
		#endif
	}
}


static inline u8 musbfsh_readb(const void __iomem *addr, unsigned int offset)
{
	u8 rc = 0;

	if (musbfsh_power) {
		rc = readb(addr + offset);
	} else {
		unsigned long flags = 0;

		#ifdef CONFIG_MUSBFSH_IDDIG
		mt_usb11_clock_prepare();
		#endif
		spin_lock_irqsave(&musbfs_io_lock, flags);
		mt65xx_usb11_clock_enable(true);
		/*
		 * DBG(0, "[MUSBfsh]: access %s FUNC: USB CLK is off 0x%X\n",
		 * __func__, offset);
		 */
		rc = readb(addr + offset);
		mt65xx_usb11_clock_enable(false);
		spin_unlock_irqrestore(&musbfs_io_lock, flags);
		#ifdef CONFIG_MUSBFSH_IDDIG
		mt_usb11_clock_unprepare();
		#endif
	}
	return rc;
}

static inline void
musbfsh_writeb(void __iomem *addr, unsigned int offset, u8 data)
{
	if (musbfsh_power) {
		writeb(data, addr + offset);
	} else {
		unsigned long flags = 0;

		#ifdef CONFIG_MUSBFSH_IDDIG
		mt_usb11_clock_prepare();
		#endif
		spin_lock_irqsave(&musbfs_io_lock, flags);
		mt65xx_usb11_clock_enable(true);
		/*
		 * DBG(0, "[MUSBfsh]: access %s FUNC: USB CLK is off 0x%X\n",
		 * __func__, offset);
		 */
		writeb(data, addr + offset);
		mt65xx_usb11_clock_enable(false);
		spin_unlock_irqrestore(&musbfs_io_lock, flags);
		#ifdef CONFIG_MUSBFSH_IDDIG
		mt_usb11_clock_unprepare();
		#endif
	}
}

/* NOTE:  these offsets are all in bytes */
#endif
