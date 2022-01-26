/*
 * MUSB OTG driver debug defines
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
 */

#ifndef __MUSBFSH_LINUX_DEBUG_H__
#define __MUSBFSH_LINUX_DEBUG_H__

extern void musbfsh_bug(void);

/* for normal log, very detail, impact performance a lot */
extern int musbfsh_debug;

#define xprintk(level,  format, args...)  \
	do { \
		if (_dbg_level(level)) { \
			pr_info("[MUSB]%s %d: " format, \
				__func__, __LINE__, ## args); \
		} \
	} while (0)

#define yprintk(facility, format, args...) \
	do { \
		if (musbfsh_debug) { \
			printk(facility "[MUSBFSH] %s %d: " format, \
					__func__, __LINE__, ## args); \
		} \
	} while (0)

static inline int _dbg_level(unsigned int level)
{
	return level <= musbfsh_debug;
}

#ifdef DBG
#undef DBG
#endif
#define DBG(level, fmt, args...) xprintk(level, fmt, ## args)

#define INFO(fmt, args...) yprintk(KERN_NOTICE, fmt, ## args)

/* for critical log */
#define zprintk(facility, format, args...) \
		printk(facility "[MUSBFSH] %s %d: " \
		       format, __func__, __LINE__, ## args)

#define WARNING(fmt, args...) zprintk(KERN_WARNING, fmt, ## args)
#define ERR(fmt, args...) zprintk(KERN_ERR, fmt, ## args)

#endif
