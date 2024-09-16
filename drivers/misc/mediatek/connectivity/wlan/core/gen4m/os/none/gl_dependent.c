/****************************************************************************
 *
 * This file is provided under a dual license.  When you use or
 * distribute this software, you may choose to be licensed under
 * version 2 of the GNU General Public License ("GPLv2 License")
 * or BSD License.
 *
 * GPLv2 License
 *
 * Copyright(C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 *
 * BSD LICENSE
 *
 * Copyright(C) 2016 MediaTek Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  * Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 ***************************************************************************/
/****************************************************************************
 *[File]             gl_dependency.c
 *[Version]          v1.0
 *[Revision Date]    2019/01/01
 *[Author]
 *[Description]
 *    The implementation for os-related API provided to common part.
 *    mainly Linux related code block in common part
 *[Copyright]
 *    Copyright (C) 2010 MediaTek Incorporation. All Rights Reserved.
 ****************************************************************************/


/*****************************************************************************
 *                         C O M P I L E R   F L A G S
 *****************************************************************************
 */

/*****************************************************************************
 *                    E X T E R N A L   R E F E R E N C E S
 *****************************************************************************
 */

#include "gl_os.h"

#include "precomp.h"

#include "stdio.h"
/*****************************************************************************
 *                              C O N S T A N T S
 *****************************************************************************
 */

/*****************************************************************************
 *                             D A T A   T Y P E S
 *****************************************************************************
 */

/*****************************************************************************
 *                            P U B L I C   D A T A
 *****************************************************************************
 */

/*****************************************************************************
 *                           P R I V A T E   D A T A
 *****************************************************************************
 */


/*****************************************************************************
 *                                 M A C R O S
 *****************************************************************************
 */

/*****************************************************************************
 *                   F U N C T I O N   D E C L A R A T I O N S
 *****************************************************************************
 */


/*****************************************************************************
 *                              F U N C T I O N S
 *****************************************************************************
 */
long KAL_NEED_IMPLEMENT(const char *f, const char *func, int line, ...)
{
	/* please refer to the implementation on other os. eg. Linux */
	DBGLOG(INIT, ERROR, "%s not supported here, %s@%d\n",
		func, f, line);
	return 0;
}

int test(struct net_device *a)
{
	return 0;
}
/*
 * for GCC to build under user space, please remove/keep it
 * dependent to the OS
 */
int main(int argc, char *argv[])
{
	wlanSetDriverDbgLevel(DBG_ALL_MODULE_IDX,
			      DBG_LOG_LEVEL_DEFAULT);
	DBGLOG(INIT, ERROR, "test run\n");
	return 0;
}
#if 0
int kal_dbg_print(const char *s, ...)
{
	printf((s), ...);
	return 0;
}
#endif
int kal_hex_dump_to_buffer(const void *buf, size_t len, int rowsize,
	int groupsize, char *linebuf, size_t linebuflen, bool ascii)
{
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__);
	return 0;
}

void kal_warn_on(uint8_t condition)
{
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__);
}

void kal_do_gettimeofday(struct timeval *tv)
{
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__);
}

void kal_get_monotonic_boottime(struct timespec *ts)
{
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__);
}

int kal_mod_timer(struct timer_list *timer, unsigned long expires)
{
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__);
	return 0;
}

int kal_strtoint(const char *s, unsigned int base, int *res)
{
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__);
	return 0;
}

int kal_strtou8(const char *s, unsigned int base, uint8_t *res)
{
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__);
	return 0;
}

int kal_strtou16(const char *s, unsigned int base, uint16_t *res)
{
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__);
	return 0;
}

int kal_strtou32(const char *s, unsigned int base, uint32_t *res)
{
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__);
	return 0;
}

int kal_strtos32(const char *s, unsigned int base, int32_t *res)
{
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__);
	return 0;
}

int kal_strtoul(const char *s, unsigned int base, unsigned long *res)
{
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__);
	return 0;
}

int kal_scnprintf(char *buf, size_t size, const char *fmt, ...)
{
	va_list args;
	int i = 0;

	va_start(args, fmt);
	/* i = vscnprintf(buf, size, fmt, args); */
	va_end(args);

	return i;
}

void *kal_kmalloc(size_t size, enum gfp_t type)
{
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__);
	return NULL;
}

void *kal_vmalloc(size_t size)
{
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__);
	return NULL;
}

void kal_kfree(void *addr)
{
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__);
}

void kal_vfree(void *addr)
{
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__);
}

void kal_spin_lock_bh(spinlock_t *lock)
{
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__);
}

void kal_spin_unlock_bh(spinlock_t *lock)
{
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__);
}

void kal_spin_lock_irqsave(spinlock_t *lock, unsigned long flags)
{
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__);
}

void kal_spin_unlock_irqrestore(spinlock_t *lock, unsigned long flags)
{
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__);
}

uint32_t kal_skb_queue_len(const struct sk_buff_head *list)
{
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__);
	return list->qlen;
}

void kal_skb_queue_tail(struct sk_buff_head *list, struct sk_buff *newsk)
{
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__);
}

void *kal_skb_push(struct sk_buff *skb, unsigned int len)
{
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__);
	return NULL;
}

unsigned char *kal_skb_put(struct sk_buff *skb, unsigned int len)
{
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__);
	return NULL;
}

struct sk_buff *kal_skb_dequeue_tail(struct sk_buff_head *list)
{
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__);
	return NULL;
}

void kal_skb_queue_head(struct sk_buff_head *list, struct sk_buff *newsk)
{
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__);
}

void kal_skb_reset_tail_pointer(struct sk_buff *skb)
{
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__);
}

void kal_skb_trim(struct sk_buff *skb, unsigned int len)
{
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__);
}

struct sk_buff *kal_dev_alloc_skb(unsigned int length)
{
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__);
	return NULL;
}

void kal_kfree_skb(struct sk_buff *skb)
{
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__);
}

int kal_test_and_clear_bit(unsigned long bit, unsigned long *p)
{
	unsigned int res;
	unsigned long mask = 1UL << (bit & 31);

	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__);
	p += bit >> 5;
	/* TODO: disable interrupt */
	res = *p;
	*p = res & ~mask;
	/* TODO: restore interrupt */

	return (res & mask) != 0;
}

void kal_clear_bit(unsigned long bit, unsigned long *p)
{
	unsigned long mask = 1UL << (bit & 31);

	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__);
	p += bit >> 5;
	/* TODO: disable interrupt */
	*p &= ~mask;
	/* TOD: restore interrupt */
}

void kal_set_bit(unsigned long nr, unsigned long *addr)
{
	unsigned long mask = BIT(nr % 32);
	unsigned long *p = ((unsigned long *)addr) + (nr / 32);

	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__);
	/* TODO: disable interrupt */
	*p |= mask;
	/* TODO: restore interrupt */
}

int kal_test_bit(unsigned long nr, unsigned long *addr)
{
	unsigned long mask = BIT(nr % 32);
	unsigned long *p = ((unsigned long *)addr) + (nr / 32);
	int res = 0;

	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__);
	/* TODO: disable interrupt */

	res = mask & *p;
	/* TODO: restore interrupt */

	return res;
}
