/*
 * MUSB OTG driver debug defines
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

#ifndef __MUSB_LINUX_DEBUG_H__
#define __MUSB_LINUX_DEBUG_H__

#define yprintk(facility, format, args...) \
		printk(facility "%s %d: " format, \
		__func__, __LINE__, ## args)

/* workaroud for redefine warning in usb_dump.c */
#ifdef WARNING
#undef WARNING
#endif

#ifdef INFO
#undef INFO
#endif

#ifdef ERR
#undef ERR
#endif

#define WARNING(fmt, args...) yprintk(KERN_WARNING, fmt, ## args)
#define INFO(fmt, args...) yprintk(KERN_INFO, fmt, ## args)
#define ERR(fmt, args...) yprintk(KERN_ERR, fmt, ## args)

#define xprintk(level,  format, args...) do { \
	if (_dbg_level(level)) { \
		if (musb_uart_debug) {\
			pr_notice("[MUSB]%s %d: " format, \
				__func__, __LINE__, ## args); \
		} \
		else{\
			pr_debug("[MUSB]%s %d: " format, \
				__func__, __LINE__, ## args); \
		} \
	} } while (0)

extern unsigned int musb_debug;
extern unsigned int musb_debug_limit;
extern unsigned int musb_uart_debug;
extern unsigned int musb_speed;

static inline int _dbg_level(unsigned int level)
{
	return level <= musb_debug;
}

#ifdef DBG
#undef DBG
#endif
#define DBG(level, fmt, args...) xprintk(level, fmt, ## args)

#define DBG_LIMIT(FREQ, fmt, args...) do {\
	static DEFINE_RATELIMIT_STATE(ratelimit, HZ, FREQ);\
	static int skip_cnt;\
	\
	if (unlikely(!musb_debug_limit))\
		DBG(0, fmt "<unlimit>\n", ## args);\
	else { \
		if (__ratelimit(&ratelimit)) {\
			DBG(0, fmt ", skip_cnt<%d>\n", ## args, skip_cnt);\
			skip_cnt = 0;\
		} else\
			skip_cnt++;\
	} \
} while (0)\

/* extern const char *otg_state_string(struct musb *); */
extern int musb_init_debugfs(struct musb *musb)  __attribute__((weak));
extern void musb_exit_debugfs(struct musb *musb) __attribute__((weak));

#endif				/*  __MUSB_LINUX_DEBUG_H__ */
