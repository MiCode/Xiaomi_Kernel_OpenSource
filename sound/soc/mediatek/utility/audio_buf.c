// SPDX-License-Identifier: GPL-2.0
//
// Copyright (C) 2018 MediaTek Inc.

/* linux include path*/
#if defined(__linux__)
#include <linux/uaccess.h>
#include <sound/pcm.h>
#include <sound/core.h>

//#include "mtk-scp-spk-common.h"
#include "audio_assert.h"
#include "audio_buf.h"
#include <linux/kprobes.h>
#include <asm/traps.h>

#define AUD_LOG_W(format, args...) snd_printk(format, ##args)
#ifdef CONFIG_SND_VERBOSE_PRINTK
#define AUD_LOG_D(format, args...) snd_printk(format, ##args)
#else
#define AUD_LOG_D(format, args...)
#endif

#endif

void reset_audio_dma_buf(struct snd_dma_buffer *dma_buf)
{
	dma_buf->addr = 0;
	dma_buf->area = NULL;
	dma_buf->bytes = 0;
}

void RingBuf_update_readptr(struct RingBuf *RingBuf1, unsigned int count)
{
	if (count == 0)
		return;

	if (RingBuf1->pRead <= RingBuf1->pWrite) {
		RingBuf1->pRead += count;
		if (RingBuf1->pRead >= RingBuf1->pBufEnd)
			RingBuf1->pRead -= RingBuf1->bufLen;
	} else {
		unsigned int r2e = RingBuf1->pBufEnd - RingBuf1->pRead;

		if (count <= r2e) {
			RingBuf1->pRead += count;
			if (RingBuf1->pRead >= RingBuf1->pBufEnd)
				RingBuf1->pRead -= RingBuf1->bufLen;
		} else {
			RingBuf1->pRead = RingBuf1->pBufBase + count - r2e;
			if (RingBuf1->pRead >= RingBuf1->pBufEnd)
				RingBuf1->pRead -= RingBuf1->bufLen;
		}
	}

	/* handle buffer underflow*/
	if (count > RingBuf1->datacount) {
		AUD_LOG_W("%s underflow count %u datacount %d Len %d\n",
			   __func__, count,
			   RingBuf1->datacount, RingBuf1->bufLen);

		if (RingBuf1->pWrite >= RingBuf1->pRead)
			RingBuf1->datacount =
			RingBuf1->pWrite - RingBuf1->pRead;
		else
			RingBuf1->datacount =
			RingBuf1->pRead + RingBuf1->bufLen - RingBuf1->pWrite;
	} else
		RingBuf1->datacount -= count;
	Ringbuf_Check(RingBuf1);
}

/*
 * function for get how many data is available
 * @return how many data exist
 */
unsigned int RingBuf_getDataCount(const struct RingBuf *RingBuf1)
{
	return RingBuf1->datacount;
}

/*
 *
 * function for get how free space available
 * @return how free sapce
 */
unsigned int RingBuf_getFreeSpace(const struct RingBuf *RingBuf1)
{
	return RingBuf1->bufLen - RingBuf1->datacount;
}

void dump_ring_bufinfo(struct RingBuf *buf)
{
#if defined(__linux__)
	pr_info(
		"pBufBase = %p pBufEnd = %p  pread = %p p write = %p DataCount = %u freespace = %u\n",
		buf->pBufBase, buf->pBufEnd, buf->pRead, buf->pWrite,
		RingBuf_getDataCount(buf), RingBuf_getFreeSpace(buf));
#else
	AUD_LOG_D(
		"pBufBase = %p pBufEnd = %p  pread = %p p write = %p DataCount = %u freespace = %u\n",
		buf->pBufBase, buf->pBufEnd, buf->pRead, buf->pWrite,
		RingBuf_getDataCount(buf), RingBuf_getFreeSpace(buf));
#endif
}

void dump_rbuf(struct RingBuf *ring_buffer)
{
	if (ring_buffer == NULL)
		return;
#if defined(__linux__)
	pr_info("%s Base[%p] End[%p] R[%p] w[%p] Len[%d] count[%d]\n",
		__func__,
		ring_buffer->pBufBase,
		ring_buffer->pBufEnd,
		ring_buffer->pRead,
		ring_buffer->pWrite,
		ring_buffer->bufLen,
		ring_buffer->datacount);
#else
	AUD_LOG_D("%s Base[%p] End[%p] R[%p] w[%p] Len[%d] count[%d]\n",
		  __func__,
		  ring_buffer->pBufBase,
		  ring_buffer->pBufEnd,
		  ring_buffer->pRead,
		  ring_buffer->pWrite,
		  ring_buffer->bufLen,
		  ring_buffer->datacount);
#endif
}

void dump_rbuf_s(const char *appendingstring, struct RingBuf *ring_buffer)
{
	if (ring_buffer == NULL)
		return;
#if defined(__linux__)
	pr_info("%s %s Base[%p] End[%p] R[%p] w[%p] Len[%d] count[%d]\n",
		appendingstring, __func__,
		ring_buffer->pBufBase,
		ring_buffer->pBufEnd,
		ring_buffer->pRead,
		ring_buffer->pWrite,
		ring_buffer->bufLen,
		ring_buffer->datacount);
#else
	AUD_LOG_D("%s %s Base[%p] End[%p] R[%p] w[%p] Len[%d] count[%d]\n",
		  appendingstring, __func__,
		  ring_buffer->pBufBase,
		  ring_buffer->pBufEnd,
		  ring_buffer->pRead,
		  ring_buffer->pWrite,
		  ring_buffer->bufLen,
		  ring_buffer->datacount);
#endif
}

int init_ring_buf(struct RingBuf *buf, char *vaaddr, int size)
{
	if (buf == NULL) {
		pr_err("%s buf == NULL\n", __func__);
		return -1;
	} else if (vaaddr == NULL) {
		pr_err("%s vaaddr == NULL\n", __func__);
		return -1;
	} else if (size == 0) {
		pr_err("%s size == 0\n", __func__);
		return -1;
	}

	buf->pBufBase = vaaddr;
	buf->pBufEnd = buf->pBufBase + size;
	buf->pRead = vaaddr;
	buf->pWrite = vaaddr;
	buf->bufLen = size;
	buf->datacount = 0;

	return 0;
}

/* check if ringbur read write pointer */
void Ringbuf_Check(struct RingBuf *RingBuf1)
{
	if (RingBuf1->pRead  ==  RingBuf1->pWrite) {
#ifdef RINGBUF_COUNT_CHECK
		if (RingBuf1->datacount != 0 && RingBuf1->datacount
		    != RingBuf1->bufLen) {
			dump_ring_bufinfo(RingBuf1);
#if defined(__linux__)
			dump_stack();
#endif
			AUD_ASSERT(0);
		}
#endif
	} else if (RingBuf1->pWrite > RingBuf1->pRead) {
#ifdef RINGBUF_COUNT_CHECK
		if ((RingBuf1->pWrite - RingBuf1->pRead)
		     != RingBuf1->datacount) {
			dump_ring_bufinfo(RingBuf1);
#if defined(__linux__)
			dump_stack();
#endif
			AUD_ASSERT(0);
		}
#endif
	} else if (RingBuf1->pRead > RingBuf1->pWrite) {
#ifdef RINGBUF_COUNT_CHECK
		if ((RingBuf1->bufLen - (RingBuf1->pRead - RingBuf1->pWrite))
		     != RingBuf1->datacount) {
			dump_ring_bufinfo(RingBuf1);
#if defined(__linux__)
			dump_stack();
#endif
			AUD_ASSERT(0);
		}
#endif
	}
	if (RingBuf1->pWrite < RingBuf1->pBufBase ||
	    RingBuf1->pWrite > RingBuf1->pBufEnd) {
		dump_rbuf(RingBuf1);
#if defined(__linux__)
		dump_stack();
#endif
		AUD_ASSERT(0);
	}
	if (RingBuf1->pRead < RingBuf1->pBufBase ||
	    RingBuf1->pRead > RingBuf1->pBufEnd) {
		dump_rbuf(RingBuf1);
#if defined(__linux__)
		dump_stack();
#endif
		AUD_ASSERT(0);
	}
	if (RingBuf1->datacount < 0) {
		dump_ring_bufinfo(RingBuf1);
#if defined(__linux__)
		dump_stack();
#endif
		AUD_ASSERT(0);
	}
}

void RingBuf_copyFromUserLinear(struct RingBuf *RingBuf1, void __user *buf,
				unsigned int count)
{
	int spaceIHave, ret;
	char *end = RingBuf1->pBufBase + RingBuf1->bufLen;
	/* count buffer data I have */
	spaceIHave = RingBuf_getFreeSpace(RingBuf1);

	/* if not enough, assert */
	/* ASSERT(spaceIHave >= count); */
	if (spaceIHave < count) {
		AUD_LOG_W("spaceIHave %d < count %d\n", spaceIHave, count);
		return;
	}

	if (RingBuf1->pRead <= RingBuf1->pWrite) {
		int w2e = end - RingBuf1->pWrite;

		if (count <= w2e) {
			ret = copy_from_user(RingBuf1->pWrite, buf, count);
			if (ret)
				AUD_LOG_D("%s copy_from_user fail line %d\n",
					  __func__, __LINE__);
			RingBuf1->pWrite += count;
			if (RingBuf1->pWrite >= end)
				RingBuf1->pWrite -= RingBuf1->bufLen;
		} else {
			ret = copy_from_user(RingBuf1->pWrite, buf, w2e);
			if (ret)
				AUD_LOG_D("%s copy_from_user fail line %d\n",
					  __func__, __LINE__);
			ret = copy_from_user(RingBuf1->pBufBase, buf + w2e,
					     count - w2e);
			if (ret)
				AUD_LOG_D("%s copy_from_user fail line %d\n",
					  __func__, __LINE__);
			RingBuf1->pWrite = RingBuf1->pBufBase + count - w2e;
		}
	} else {
		ret = copy_from_user(RingBuf1->pWrite, buf, count);
		if (ret)
			AUD_LOG_D("%s copy_from_user fail line %d\n",
				  __func__, __LINE__);
		RingBuf1->pWrite += count;
		if (RingBuf1->pWrite >= RingBuf1->pBufEnd)
			RingBuf1->pWrite -= RingBuf1->bufLen;
	}
	RingBuf1->datacount += count;
}

void RingBuf_copyFromLinear(struct RingBuf *RingBuf1, const char *buf,
			    unsigned int count)
{
	int spaceIHave;
	char *end = RingBuf1->pBufBase + RingBuf1->bufLen;
	/* count buffer data I have */
	spaceIHave = RingBuf_getFreeSpace(RingBuf1);

	/* if not enough, assert */
	/* ASSERT(spaceIHave >= count); */
	if (spaceIHave < count) {
		AUD_LOG_W("spaceIHave %d < count %d\n", spaceIHave, count);
		return;
	}

	if (RingBuf1->pRead <= RingBuf1->pWrite) {
		int w2e = end - RingBuf1->pWrite;

		if (count <= w2e) {
			memcpy(RingBuf1->pWrite, buf, count);
			RingBuf1->pWrite += count;
			if (RingBuf1->pWrite >= end)
				RingBuf1->pWrite -= RingBuf1->bufLen;
		} else {
			memcpy(RingBuf1->pWrite, buf, w2e);
			memcpy(RingBuf1->pBufBase, buf + w2e, count - w2e);
			RingBuf1->pWrite = RingBuf1->pBufBase + count - w2e;
		}
	} else {
		memcpy(RingBuf1->pWrite, buf, count);
		RingBuf1->pWrite += count;
		if (RingBuf1->pWrite >= RingBuf1->pBufEnd)
			RingBuf1->pWrite -= RingBuf1->bufLen;
	}
	RingBuf1->datacount += count;
	Ringbuf_Check(RingBuf1);
}

void RingBuf_Reset(struct RingBuf *RingBuf1)
{
	if (RingBuf1 == NULL)
		return;

	/* clear ringbuufer data*/
	memset(RingBuf1->pBufBase, 0, RingBuf1->bufLen);
	RingBuf1->pRead = RingBuf1->pBufBase;
	RingBuf1->pWrite = RingBuf1->pBufBase;
	RingBuf1->datacount = 0;
}

int RingBuf_Clear(struct RingBuf *RingBuf1)
{
	if (RingBuf1 == NULL)
		return -1;


	if (RingBuf1->pBufBase != NULL && RingBuf1->pBufEnd != NULL) {
		/* clear ringbuufer data*/
		memset(RingBuf1->pBufBase, 0, RingBuf1->bufLen);

		RingBuf1->pBufBase = NULL;
		RingBuf1->pBufEnd = NULL;
		RingBuf1->pRead = NULL;
		RingBuf1->pWrite = NULL;
		RingBuf1->bufLen = 0;
	}
	return 0;
}

