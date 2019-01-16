/*
 * MUSB OTG driver - support for Mentor's DMA controller
 *
 * Copyright 2005 Mentor Graphics Corporation
 * Copyright (C) 2005-2007 by Texas Instruments
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
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

#define MUSBFSH_HSDMA_BASE		0x200
#define MUSBFSH_HSDMA_INTR		(MUSBFSH_HSDMA_BASE + 0)
#define MUSBFSH_HSDMA_DMA_INTR_UNMASK MUSBFSH_HSDMA_INTR+1
#define MUSBFSH_HSDMA_DMA_INTR_UNMASK_CLEAR MUSBFSH_HSDMA_INTR+2
#define MUSBFSH_HSDMA_DMA_INTR_UNMASK_SET MUSBFSH_HSDMA_INTR+3

#define MUSBFSH_HSDMA_CONTROL		0x4
#define MUSBFSH_HSDMA_ADDRESS		0x8
#define MUSBFSH_HSDMA_COUNT		0xc

#define MUSBFSH_HSDMA_CHANNEL_OFFSET(_bchannel, _offset)		\
		(MUSBFSH_HSDMA_BASE + (_bchannel << 4) + _offset)//_bchannel starts from 0

#define musbfsh_read_hsdma_addr(mbase, bchannel)	\
	musbfsh_readl(mbase,	\
		   MUSBFSH_HSDMA_CHANNEL_OFFSET(bchannel, MUSBFSH_HSDMA_ADDRESS))

#define musbfsh_write_hsdma_addr(mbase, bchannel, addr) \
	musbfsh_writel(mbase, \
		    MUSBFSH_HSDMA_CHANNEL_OFFSET(bchannel, MUSBFSH_HSDMA_ADDRESS), \
		    addr)

#define musbfsh_read_hsdma_count(mbase, bchannel)	\
	musbfsh_readl(mbase,	\
		   MUSBFSH_HSDMA_CHANNEL_OFFSET(bchannel, MUSBFSH_HSDMA_COUNT))

#define musbfsh_write_hsdma_count(mbase, bchannel, len) \
	musbfsh_writel(mbase, \
		    MUSBFSH_HSDMA_CHANNEL_OFFSET(bchannel, MUSBFSH_HSDMA_COUNT), \
		    len)

/* control register (16-bit): */
#define MUSBFSH_HSDMA_ENABLE_SHIFT		0
#define MUSBFSH_HSDMA_TRANSMIT_SHIFT	1
#define MUSBFSH_HSDMA_MODE1_SHIFT		2
#define MUSBFSH_HSDMA_IRQENABLE_SHIFT	3
#define MUSBFSH_HSDMA_ENDPOINT_SHIFT	4
#define MUSBFSH_HSDMA_BUSERROR_SHIFT	8
#define MUSBFSH_HSDMA_BURSTMODE_SHIFT	9
#define MUSBFSH_HSDMA_BURSTMODE		(3 << MUSBFSH_HSDMA_BURSTMODE_SHIFT)
#define MUSBFSH_HSDMA_BURSTMODE_UNSPEC	0
#define MUSBFSH_HSDMA_BURSTMODE_INCR4	1
#define MUSBFSH_HSDMA_BURSTMODE_INCR8	2
#define MUSBFSH_HSDMA_BURSTMODE_INCR16	3

#ifndef MUSBFSH_HSDMA_CHANNELS
#define MUSBFSH_HSDMA_CHANNELS		8 
#endif

struct musbfsh_dma_controller;

struct musbfsh_dma_channel {
	struct dma_channel		channel;
	struct musbfsh_dma_controller	*controller;
	u32				start_addr;
	u32				len;
	u16				max_packet_sz;
	u8				idx;
	u8				epnum;
	u8				transmit;
};

struct musbfsh_dma_controller {
	struct dma_controller		controller;
	struct musbfsh_dma_channel		channel[MUSBFSH_HSDMA_CHANNELS];
	void				*private_data;
	void __iomem			*base;
	u8				channel_count;
	u8				used_channels;
	u8				irq;
};
