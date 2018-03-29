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

#ifndef __MUSB_IOM_H__
#define __MUSB_IOM_H__

#include <linux/io.h>

static inline void mu3d_writeb(void __iomem *base, unsigned int offset, unsigned char data)
{
	writeb(data, base + offset);
}

static inline void mu3d_writew(void __iomem *base, unsigned int offset, unsigned short data)
{
	writew(data, base + offset);
}

static inline void mu3d_writel(void __iomem *base, unsigned int offset, unsigned int data)
{
	writel(data, base + offset);
}

static inline u32 mu3d_readl(void __iomem *base, unsigned int offset)
{
	return readl(base + offset);
}

static inline void mu3d_writelmsk(void __iomem *base, unsigned int offset, unsigned int msk,
				  unsigned int data)
{
	void __iomem *addr = base + offset;
	unsigned int tmp = readl(addr);

	mb();
	writel(((tmp & ~(msk)) | ((data) & (msk))), addr);
}

static inline void mu3d_setmsk(void __iomem *base, unsigned int offset, unsigned int msk)
{
	void __iomem *addr = base + offset;
	unsigned int tmp = readl(addr);

	mb();
	writel((tmp | (msk)), addr);
}

static inline void mu3d_clrmsk(void __iomem *base, unsigned int offset, unsigned int msk)
{
	void __iomem *addr = base + offset;
	unsigned int tmp = readl(addr);

	mb();
	writel((tmp & ~(msk)), addr);

}

/*msk the data first, then umsk with the umsk.*/
#if 0
static inline void mu3d_writelmskumsk(void __iomem *base, unsigned int offset, unsigned int data,
				      unsigned int msk, unsigned int umsk)
{
	void __iomem *addr = /*base + */ (void __iomem *)offset;
	unsigned int tmp = readl(addr);

	mb();
	writel(((tmp & ~(msk)) | ((data) & (msk))) & (umsk), addr);
}

static inline unsigned int mu3d_dir_readl(void __iomem *regs)
{
	return readl(regs);
}

static inline void mu3d_dir_writeb(void __iomem *regs, unsigned char val)
{
	writeb(val, regs);
}

static inline void mu3d_dir_writew(void __iomem *regs, unsigned short val)
{
	writew(val, regs);
}

static inline void mu3d_dir_writel(void __iomem *regs, unsigned int val)
{
	writel(val, regs);
}
#endif


/* ep_num should be >1 & < max ep-number */
static inline u32 mu3d_xcsr_readl(void __iomem *base, unsigned int ep1_csrx, unsigned int ep_num)
{
	return readl(base + (ep1_csrx + (0x10 * (ep_num - 1))));
}

/* ep_num should be >1 & < max ep-number */
static inline void mu3d_xcsr_writel(void __iomem *base, unsigned int ep1_csrx, unsigned int ep_num,
				    unsigned int data)
{
	writel(data, base + (ep1_csrx + (0x10 * (ep_num - 1))));
}

#endif
