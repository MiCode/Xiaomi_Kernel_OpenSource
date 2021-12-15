// SPDX-License-Identifier: GPL-2.0
//
// Copyright (C) 2018 MediaTek Inc.

/* linux include path*/
#if defined(__linux__)
#include <linux/uaccess.h>
#include <sound/pcm.h>
#include <sound/core.h>

#include "mtk-dsp-mem-control.h"
#include "mtk-dsp-common.h"
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

#ifdef CFG_AUDIO_SUPPORT
#include <audio_type.h>
#include <audio_ringbuf.h>
#include <dma.h>
#include "audio_hw_reg.h"
#include <dvfs.h>
#endif

#ifdef CONFIG_MTK_AUDIODSP_SUPPORT
#include <adsp_ipi.h>
#include <audio_ipi_dma.h>
#else
#include <scp_ipi.h>
#endif

#define AUDIO_BUF_ALIGNEMNT (8)
/* #define RINGBUF_COUNT_CHECK */

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

/**
 * copy count number bytes from ring buffer to buf
 * @param buf buffer copy from
 * @param RingBuf1 buffer copy to
 * @param count number of bytes need to copy
 */
void RingBuf_copyToLinear(char *buf, struct RingBuf *RingBuf1,
			  unsigned int count)
{
	if (count == 0)
		return;

	if (RingBuf_getDataCount(RingBuf1) < count) {
		AUD_LOG_D("RingBuf_getDataCount(RingBuf1) %d < count %d\n",
			  RingBuf_getDataCount(RingBuf1), count);
		return;
	}

	if (RingBuf1->pRead <= RingBuf1->pWrite) {
		memcpy(buf, RingBuf1->pRead, count);
		RingBuf1->pRead += count;
		if (RingBuf1->pRead >= RingBuf1->pBufEnd)
			RingBuf1->pRead -= RingBuf1->bufLen;
	} else {
		unsigned int r2e = RingBuf1->pBufEnd - RingBuf1->pRead;

		if (count <= r2e) {
			memcpy(buf, RingBuf1->pRead, count);
			RingBuf1->pRead += count;
			if (RingBuf1->pRead >= RingBuf1->pBufEnd)
				RingBuf1->pRead -= RingBuf1->bufLen;
		} else {
			memcpy(buf, RingBuf1->pRead, r2e);
			memcpy(buf + r2e, RingBuf1->pBufBase, count - r2e);
			RingBuf1->pRead = RingBuf1->pBufBase + count - r2e;
		}
	}
	RingBuf1->datacount -= count;
	Ringbuf_Check(RingBuf1);
}

/**
 * copy count number bytes from buf to RingBuf1
 * @param RingBuf1 ring buffer copy from
 * @param buf copy to
 * @param count number of bytes need to copy
 */

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

/**
 * copy ring buffer from RingBufs(source) to RingBuft(target)
 * @param RingBuft ring buffer copy to
 * @param RingBufs copy from copy from
 */
void RingBuf_copyFromRingBufAll(struct RingBuf *RingBuft,
				struct RingBuf *RingBufs)
{
	/* if not enough, assert */
	/* ASSERT(RingBuf_getFreeSpace(RingBuft) >= */
	/* RingBuf_getDataCount(RingBufs)); */
	if (RingBuf_getFreeSpace(RingBuft) < RingBuf_getDataCount(RingBufs))
		AUD_LOG_D(
			"RingBuf_getFreeSpace(RingBuft) %d < RingBuf_getDataCount(RingBufs) %d\n",
			RingBuf_getFreeSpace(RingBuft),
			RingBuf_getDataCount(RingBufs));

	if (RingBufs->pRead <= RingBufs->pWrite) {
		RingBuf_copyFromLinear(RingBuft, RingBufs->pRead,
				       RingBufs->pWrite - RingBufs->pRead);
	} else {
		RingBuf_copyFromLinear(RingBuft, RingBufs->pRead,
				       RingBufs->pBufEnd - RingBufs->pRead);
		RingBuf_copyFromLinear(RingBuft, RingBufs->pBufBase,
				       RingBufs->pWrite - RingBufs->pBufBase);
	}
	RingBufs->pRead = RingBufs->pWrite;
	Ringbuf_Check(RingBuft);
	Ringbuf_Check(RingBufs);
}

/**
 * copy ring buffer from RingBufs(source) to RingBuft(target) with count
 * @param RingBuft ring buffer copy to
 * @param RingBufs copy from copy from
 */
int RingBuf_copyFromRingBuf(struct RingBuf *RingBuft, struct RingBuf *RingBufs,
			    unsigned int count)
{
	if (count == 0)
		return 0;

	/* if not enough, assert */
	/* ASSERT(RingBuf_getDataCount(RingBufs) >= count && */
	/* RingBuf_getFreeSpace(RingBuft) >= count); */
	if ((RingBuf_getDataCount(RingBufs) < count) ||
	    (RingBuf_getFreeSpace(RingBuft) < count)) {
		AUD_LOG_D("Space RingBuft %d || Data RingBufs %d < count %d\n",
			  RingBuf_getFreeSpace(RingBuft),
			  RingBuf_getDataCount(RingBufs), count);
	}
	if (RingBufs->pRead <= RingBufs->pWrite) {
		RingBuf_copyFromLinear(RingBuft, RingBufs->pRead, count);
		RingBufs->pRead += count;
		if (RingBufs->pRead >= RingBufs->pBufEnd)
			RingBufs->pRead -= RingBufs->bufLen;
	} else {
		unsigned int r2e = RingBufs->pBufEnd - RingBufs->pRead;

		if (r2e >= count) {
			RingBuf_copyFromLinear(RingBuft, RingBufs->pRead,
					       count);
			RingBufs->pRead += count;
			if (RingBufs->pRead >= RingBufs->pBufEnd)
				RingBufs->pRead -= RingBufs->bufLen;
		} else {
			RingBuf_copyFromLinear(RingBuft, RingBufs->pRead, r2e);
			RingBuf_copyFromLinear(RingBuft, RingBufs->pBufBase,
					       count - r2e);
			RingBufs->pRead = RingBufs->pBufBase + count - r2e;
		}
	}
	Ringbuf_Check(RingBuft);
	Ringbuf_Check(RingBufs);
	return count;
}

/**
 * write bytes size of count with value
 * @param RingBuf1 ring buffer copy to
 * @value value put into buffer
 * @count bytes ned to put.
 */

void RingBuf_writeDataValue(struct RingBuf *RingBuf1, const char value,
			    const unsigned int count)
{
	if (count == 0)
		return;

	/* if not enough, assert */
	if (RingBuf_getFreeSpace(RingBuf1) < count) {
		AUD_LOG_D("RingBuf_getFreeSpace(RingBuf1) %d < count %d\n",
			  RingBuf_getFreeSpace(RingBuf1), count);
	}
	if (RingBuf1->pRead <= RingBuf1->pWrite) {
		unsigned int w2e = RingBuf1->pBufEnd - RingBuf1->pWrite;

		if (count <= w2e) {
			memset(RingBuf1->pWrite, value, count);
			RingBuf1->pWrite += count;
			if (RingBuf1->pWrite >= RingBuf1->pBufEnd)
				RingBuf1->pWrite -= RingBuf1->bufLen;
		} else {
			memset(RingBuf1->pWrite, value, w2e);
			memset(RingBuf1->pBufBase, value, count - w2e);
			RingBuf1->pWrite = RingBuf1->pBufBase + count - w2e;
		}
	} else {
		memset(RingBuf1->pWrite, value, count);
		RingBuf1->pWrite += count;
		if (RingBuf1->pWrite >= RingBuf1->pBufEnd)
			RingBuf1->pWrite -= RingBuf1->bufLen;
	}
	RingBuf1->datacount += count;
}

void RingBuf_update_writeptr(struct RingBuf *RingBuf1, unsigned int count)
{
	if (count == 0 || count > RingBuf1->bufLen) {
#ifdef RINGBUF_COUNT_CHECK
		AUD_LOG_W("%s count[%u] datacount[%d] Len[%d]\n",
			  __func__, count,
			  RingBuf1->datacount, RingBuf1->bufLen);
#endif
		return;
	}

	if (RingBuf1->pRead <= RingBuf1->pWrite) {
		unsigned int w2e = RingBuf1->pBufEnd - RingBuf1->pWrite;

		if (count <= w2e) {
			RingBuf1->pWrite += count;
			if (RingBuf1->pWrite >= RingBuf1->pBufEnd)
				RingBuf1->pWrite -= RingBuf1->bufLen;
		} else {
			RingBuf1->pWrite = RingBuf1->pBufBase + count - w2e;
			if (RingBuf1->pWrite > RingBuf1->pRead)
				RingBuf1->pWrite = RingBuf1->pBufBase;
		}
	} else {
		RingBuf1->pWrite += count;
		if (RingBuf1->pWrite >= RingBuf1->pBufEnd)
			RingBuf1->pWrite -= RingBuf1->bufLen;
	}
	/* handle buffer overflow*/
	if (RingBuf1->datacount + count > RingBuf1->bufLen) {
		AUD_LOG_W("%s overflow count[%u] datacount[%d] Len[%d]\n",
			  __func__, count,
			  RingBuf1->datacount, RingBuf1->bufLen);

		if (RingBuf1->pRead >= RingBuf1->pWrite)
			RingBuf1->datacount =
			RingBuf1->pRead - RingBuf1->pWrite;
		else
			RingBuf1->datacount =
			RingBuf1->pWrite + RingBuf1->bufLen - RingBuf1->pRead;
	} else
		RingBuf1->datacount += count;

	Ringbuf_Check(RingBuf1);
}

void RingBuf_update_readptr(struct RingBuf *RingBuf1, unsigned int count)
{
	if (count == 0 || count > RingBuf1->bufLen) {
		AUD_LOG_W("%s count[%u] datacount[%d] Len[%d]\n",
			  __func__, count,
			  RingBuf1->datacount, RingBuf1->bufLen);
		return;
	}

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
#ifdef RINGBUF_COUNT_CHECK
		AUD_LOG_W("%s underflow count %u datacount %d Len %d\n",
			   __func__, count,
			   RingBuf1->datacount, RingBuf1->bufLen);
#endif
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

void RingBuf_Bridge_update_writeptr(struct ringbuf_bridge *RingBuf1,
				    const unsigned int count)
{
	if (count == 0)
		return;

#ifdef RINGBUF_COUNT_CHECK
	if (RingBuf1->datacount + count > RingBuf1->bufLen) {
		AUD_LOG_W("datacount %llu count %u bufLen %llu\n",
			  RingBuf1->datacount, count,
			  RingBuf1->bufLen);
		AUD_ASSERT(0);
	}
#endif
	if (RingBuf1->pRead <= RingBuf1->pWrite) {
		unsigned int w2e = RingBuf1->pBufEnd - RingBuf1->pWrite;

		if (count <= w2e) {
			RingBuf1->pWrite += count;
			if (RingBuf1->pWrite >= RingBuf1->pBufEnd)
				RingBuf1->pWrite -= RingBuf1->bufLen;
		} else {
			RingBuf1->pWrite = RingBuf1->pBufBase + count - w2e;
			if (RingBuf1->pWrite > RingBuf1->pRead)
				RingBuf1->pWrite = RingBuf1->pBufBase;
		}
	} else {
		RingBuf1->pWrite += count;
		if (RingBuf1->pWrite >= RingBuf1->pBufEnd)
			RingBuf1->pWrite -= RingBuf1->bufLen;
	}
	Ringbuf_Bridge_Check(RingBuf1);
}

void RingBuf_Bridge_update_readptr(struct ringbuf_bridge *RingBuf1,
				   const unsigned int count)
{
	if (count == 0)
		return;

	if (RingBuf1->pRead <= RingBuf1->pWrite) {
		RingBuf1->pRead += count;
		if (RingBuf1->pRead >= RingBuf1->pBufEnd)
			RingBuf1->pRead -= RingBuf1->pBufBase;
	} else {
		unsigned int r2e = RingBuf1->pBufEnd - RingBuf1->pRead;

		if (count <= r2e) {
			RingBuf1->pRead += count;
			if (RingBuf1->pRead >= RingBuf1->pBufEnd)
				RingBuf1->pRead -= RingBuf1->pBufBase;
		} else {
			RingBuf1->pRead = RingBuf1->pBufBase + count - r2e;
			if (RingBuf1->pRead >= RingBuf1->pBufEnd)
				RingBuf1->pRead -= RingBuf1->bufLen;
		}
	}
	Ringbuf_Bridge_Check(RingBuf1);
}

void dump_audio_dsp_dram(struct audio_dsp_dram *dsp_dram)
{
	AUD_LOG_D(
		"%s dsp_dram vir_addr = %p va_addr = 0x%llx phy_addr =0x%llx size=%llu\n",
		__func__, dsp_dram->vir_addr, dsp_dram->va_addr,
		dsp_dram->phy_addr, dsp_dram->size);
}

#if defined(__linux__)

int release_snd_dmabuffer(struct snd_dma_buffer *dma_buffer)
{
	dma_buffer->area = NULL;
	dma_buffer->addr = 0;
	dma_buffer->bytes = 0;
	return 0;
}

/* wrap dsp dram to dma_buffer */
int dsp_dram_to_snd_dmabuffer(struct audio_dsp_dram *dsp_dram,
			      struct snd_dma_buffer *dma_buffer)
{
	if (dsp_dram == NULL)
		return -1;
	if (dma_buffer == NULL)
		return -1;

	dma_buffer->area = dsp_dram->vir_addr;
	dma_buffer->addr = dsp_dram->phy_addr;
	dma_buffer->bytes = dsp_dram->size;
	return 0;
}

/* wrap snd_dma_buffer to audio_hw_buffer */
int snd_dmabuffer_to_audio_ring_buffer(struct snd_dma_buffer *dma_buffer,
				       struct RingBuf *audio_ring_buf)
{
	int ret = 0;

	if (dma_buffer == NULL || audio_ring_buf == NULL)
		return -1;

	if (dma_buffer->area == NULL) {
		AUD_LOG_D("%s ma_buffer->area == NULL", __func__);
		return -1;
	}

	if (dma_buffer->bytes == 0) {
		AUD_LOG_D("%s ma_buffer->bytes == 0", __func__);
		return -1;
	}

	audio_ring_buf->bufLen = dma_buffer->bytes;
	audio_ring_buf->pBufBase = dma_buffer->area;
	audio_ring_buf->pBufEnd = dma_buffer->area + dma_buffer->bytes - 1;
	audio_ring_buf->pWrite = audio_ring_buf->pBufBase;
	audio_ring_buf->pRead = audio_ring_buf->pBufBase;

	return ret;
}

int snd_dmabuffer_to_audio_ring_buffer_bridge(
	struct snd_dma_buffer *dma_buffer,
	struct ringbuf_bridge *audio_ring_buf_brideg)
{
	int ret = 0;

	if (dma_buffer == NULL || audio_ring_buf_brideg == NULL)
		return -1;

	if (dma_buffer->area == NULL) {
		AUD_LOG_D("%s ma_buffer->area == NULL", __func__);
		return -1;
	}

	if (dma_buffer->bytes == 0) {
		AUD_LOG_D("%s ma_buffer->bytes == 0", __func__);
		return -1;
	}

	audio_ring_buf_brideg->bufLen = dma_buffer->bytes;
	audio_ring_buf_brideg->pBufBase = dma_buffer->addr;
	audio_ring_buf_brideg->pBufEnd =
		dma_buffer->addr + dma_buffer->bytes - 1;
	audio_ring_buf_brideg->pWrite = audio_ring_buf_brideg->pBufBase;
	audio_ring_buf_brideg->pRead = audio_ring_buf_brideg->pBufBase;

	return ret;
}

int set_audiobuffer_threshold(struct audio_hw_buffer *audio_hwbuf,
			      struct snd_pcm_substream *substream)
{
	int ret = 0;

	if (audio_hwbuf == NULL) {
		AUD_LOG_D("%s audio_hwbuf == NULL", __func__);
		return -1;
	}

	audio_hwbuf->aud_buffer.start_threshold =
		substream->runtime->start_threshold;
	audio_hwbuf->aud_buffer.stop_threshold =
		substream->runtime->stop_threshold;
	audio_hwbuf->aud_buffer.period_size = substream->runtime->period_size;
	audio_hwbuf->aud_buffer.period_count = substream->runtime->periods;

	return ret;
}

int set_afe_audio_pcmbuf(struct audio_hw_buffer *audio_hwbuf,
			 struct snd_pcm_substream *substream)
{
	int ret = 0;

	ret = init_ring_buf_bridge(
		&audio_hwbuf->aud_buffer.buf_bridge,
		(unsigned long long)substream->runtime->dma_addr,
		substream->runtime->dma_bytes);

	return ret;
}

int set_audiobuffer_attribute(struct audio_hw_buffer *audio_hwbuf,
			      struct snd_pcm_substream *substream,
			      struct snd_pcm_hw_params *params,
			      int direction)
{
	int ret = 0;

	if (audio_hwbuf == NULL) {
		AUD_LOG_D("%s audio_hwbuf == NULL", __func__);
		return -1;
	}

	audio_hwbuf->aud_buffer.buffer_attr.direction = direction;

	if (params == NULL)
		return 0;

	audio_hwbuf->aud_buffer.buffer_attr.channel = params_channels(params);
	audio_hwbuf->aud_buffer.buffer_attr.format = params_format(params);
	audio_hwbuf->aud_buffer.buffer_attr.rate = params_rate(params);

	return ret;
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


void ringbuf_copyto_user_linear(void __user *buf, struct RingBuf *RingBuf1,
			  unsigned int count)
{
	int ret = 0;

	if (count == 0)
		return;

	if (RingBuf_getDataCount(RingBuf1) < count) {
		AUD_LOG_D("RingBuf_getDataCount(RingBuf1) %d < count %d\n",
			  RingBuf_getDataCount(RingBuf1), count);
		return;
	}

	if (RingBuf1->pRead <= RingBuf1->pWrite) {
		ret = copy_to_user(buf, RingBuf1->pRead, count);
		if (ret)
			AUD_LOG_D("%s copy_to_user fail line %d\n",
				  __func__, __LINE__);
		RingBuf1->pRead += count;
		if (RingBuf1->pRead >= RingBuf1->pBufEnd)
			RingBuf1->pRead -= RingBuf1->bufLen;
	} else {
		unsigned int r2e = RingBuf1->pBufEnd - RingBuf1->pRead;

		if (count <= r2e) {
			ret = copy_to_user(buf, RingBuf1->pRead, count);
			if (ret)
				AUD_LOG_D("%s copy_to_user fail line %d\n",
					  __func__, __LINE__);
			RingBuf1->pRead += count;
			if (RingBuf1->pRead >= RingBuf1->pBufEnd)
				RingBuf1->pRead -= RingBuf1->bufLen;
		} else {
			ret = copy_to_user(buf, RingBuf1->pRead, r2e);
			if (ret)
				AUD_LOG_D("%s copy_to_user fail line %d\n",
					  __func__, __LINE__);
			ret = copy_to_user(buf + r2e,
					   RingBuf1->pBufBase,
					   count - r2e);
			if (ret)
				AUD_LOG_D("%s copy_to_user fail line %d\n",
					  __func__, __LINE__);
			RingBuf1->pRead = RingBuf1->pBufBase + count - r2e;
		}
	}
	RingBuf1->datacount -= count;
	Ringbuf_Check(RingBuf1);
}

#endif

int init_ring_buf(struct RingBuf *buf, char *vaaddr, int size)
{
	if (buf == NULL) {
		AUD_LOG_D("%s buf == NULL\n", __func__);
		return -1;
	} else if (vaaddr == NULL) {
		AUD_LOG_D("%s vaaddr == NULL\n", __func__);
		return -1;
	} else if (size == 0) {
		AUD_LOG_D("%s size == 0\n", __func__);
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

int init_ring_buf_bridge(struct ringbuf_bridge *buf_bridge,
			 unsigned long long paaddr, int size)
{
	if (buf_bridge == NULL) {
		AUD_LOG_D("%s buf_bridge == NULL\n", __func__);
		return -1;
	} else if (paaddr == 0) {
		AUD_LOG_D("%s paaddr == NULL\n", __func__);
		return -1;
	} else if (size == 0) {
		AUD_LOG_D("%s size == 0\n", __func__);
		return -1;
	}

	buf_bridge->pBufBase = paaddr;
	buf_bridge->pBufEnd = buf_bridge->pBufBase + size;
	buf_bridge->pRead = paaddr;
	buf_bridge->pWrite = paaddr;
	buf_bridge->bufLen = size;
	buf_bridge->datacount = 0;
	return 0;
}

void RingBuf_Bridge_Reset(struct ringbuf_bridge *RingBuf1)
{
	if (RingBuf1 == NULL)
		return;

	RingBuf1->pRead = RingBuf1->pBufBase;
	RingBuf1->pWrite = RingBuf1->pBufBase;
	RingBuf1->datacount = 0;
}

int RingBuf_Bridge_Clear(struct ringbuf_bridge *RingBuf1)
{
	if (RingBuf1 == NULL)
		return -1;


	if (RingBuf1->pBufBase != 0 && RingBuf1->pBufEnd != 0) {
		RingBuf1->pBufBase = 0;
		RingBuf1->pBufEnd = 0;
		RingBuf1->pRead = 0;
		RingBuf1->pWrite = 0;
		RingBuf1->bufLen = 0;
		RingBuf1->datacount = 0;
	}
	return 0;
}

/* check if ringbur read write pointer*/
void Ringbuf_Bridge_Check(struct ringbuf_bridge *buf_bridge)
{
	if (buf_bridge->pRead > buf_bridge->pBufBase + buf_bridge->bufLen) {
		dump_rbuf_bridge(buf_bridge);
#if defined(__linux__)
		dump_stack();
#endif
		AUD_ASSERT(0);
	}
	if (buf_bridge->pWrite > buf_bridge->pBufBase + buf_bridge->bufLen) {
		dump_rbuf_bridge(buf_bridge);
#if defined(__linux__)
		dump_stack();
#endif
		AUD_ASSERT(0);
	}
	if (buf_bridge->pWrite < buf_bridge->pBufBase) {
		dump_rbuf_bridge(buf_bridge);
#if defined(__linux__)
		dump_stack();
#endif
		AUD_ASSERT(0);
	}
	if (buf_bridge->pRead < buf_bridge->pBufBase) {
		dump_rbuf_bridge(buf_bridge);
#if defined(__linux__)
		dump_stack();
#endif
		AUD_ASSERT(0);
	}
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

int clear_audiobuffer_hw(struct audio_hw_buffer *audio_hwbuf)
{

	int ret = 0;

	if (audio_hwbuf == NULL)
		return -1;

	RingBuf_Bridge_Reset(&audio_hwbuf->aud_buffer.buf_bridge);
	audio_hwbuf->counter = 0;

	return ret;
}

int reset_audiobuffer(struct audio_buffer *audio_buf)
{
	int ret = 0;

	if (audio_buf == NULL)
		return -1;

	audio_buf->period_size = 0;
	audio_buf->start_threshold = 0;
	audio_buf->stop_threshold = 0;

	RingBuf_Bridge_Reset(&audio_buf->buf_bridge);

	return ret;
}

int reset_audiobuffer_hw(struct audio_hw_buffer *audio_hwbuf)
{
	int ret = 0;
	struct audio_buffer *audio_buf = &audio_hwbuf->aud_buffer;

	if (audio_hwbuf == NULL) {
		AUD_LOG_D("%s audio_hwbuf == NULL", __func__);
		return -1;
	}

	audio_hwbuf->hw_buffer = 0;
	audio_hwbuf->audio_memiftype = 0;
	audio_hwbuf->irq_num = 0;

	ret = reset_audiobuffer(audio_buf);

	return ret;
}

int set_audiobuffer_hw(struct audio_hw_buffer *audio_hwbuf, int hw_buffer)
{
	int ret = 0;

	if (audio_hwbuf == NULL) {
		AUD_LOG_D("%s audio_hwbuf == NULL", __func__);
		return -1;
	}

	audio_hwbuf->hw_buffer = hw_buffer;
	return ret;
}

int set_audiobuffer_memorytype(struct audio_hw_buffer *audio_hwbuf,
				       int memory_type)
{
	int ret = 0;

	if (audio_hwbuf == NULL) {
		AUD_LOG_D("%s audio_hwbuf == NULL", __func__);
		return -1;
	}

	audio_hwbuf->memory_type = memory_type;
	return ret;
}

int set_audiobuffer_audio_memiftype(struct audio_hw_buffer *audio_hwbuf,
				    int audio_memiftype)
{
	int ret = 0;

	if (audio_hwbuf == NULL) {
		AUD_LOG_D("%s audio_hwbuf == NULL", __func__);
		return -1;
	}

	audio_hwbuf->audio_memiftype = audio_memiftype;
	return ret;
}

int set_audiobuffer_audio_irq_num(struct audio_hw_buffer *audio_hwbuf,
				  int irq_num)
{
	int ret = 0;

	if (audio_hwbuf == NULL) {
		AUD_LOG_D("%s audio_hwbuf == NULL", __func__);
		return -1;
	}

	audio_hwbuf->irq_num = irq_num;
	return ret;
}

int sync_ringbuf_readidx(struct RingBuf *task_ring_buf,
			 struct ringbuf_bridge *buf_bridge)
{
	unsigned int datacount = 0;
	char *readidx = NULL;

	if (task_ring_buf == NULL) {
		AUD_LOG_W("%s task_ring_buf == NULL", __func__);
		return -1;
	} else if (buf_bridge == NULL) {
		AUD_LOG_W("%s buf_bridge == NULL", __func__);
		return -1;
	}

	/* buffer empty */
	if (task_ring_buf->pRead == task_ring_buf->pWrite &&
	    task_ring_buf->datacount == 0) {
		AUD_LOG_W("%s task_ring_buf empty", __func__);
		return -1;
	}

	readidx = task_ring_buf->pBufBase +
		  (buf_bridge->pRead - buf_bridge->pBufBase);

	if (readidx >= task_ring_buf->pRead)
		datacount = readidx - task_ring_buf->pRead;
	else
		datacount = task_ring_buf->bufLen -
			    (task_ring_buf->pRead - readidx);

#ifdef RINGBUF_COUNT_CHECK
	if (datacount == 0 || datacount == task_ring_buf->bufLen) {
		dump_rbuf_s(__func__, task_ring_buf);
		dump_rbuf_bridge_s(__func__, buf_bridge);
	}
#endif

	RingBuf_update_readptr(task_ring_buf, datacount);

	Ringbuf_Check(task_ring_buf);
	Ringbuf_Bridge_Check(buf_bridge);
	return 0;
}

int sync_ringbuf_writeidx(struct RingBuf *task_ring_buf,
			  struct ringbuf_bridge *buf_bridge)
{
	unsigned int datacount = 0;
	char *writeidx = NULL;

	if (task_ring_buf == NULL) {
		AUD_LOG_D("%s task_ring_buf == NULL", __func__);
		return -1;
	} else if (buf_bridge == NULL) {
		AUD_LOG_D("%s buf_bridge == NULL", __func__);
		return -1;
	}

	/* buffer full */
	if (task_ring_buf->pRead == task_ring_buf->pWrite &&
		task_ring_buf->datacount == task_ring_buf->bufLen) {
		AUD_LOG_D("%s task_ring_buf full", __func__);
	}

	writeidx = task_ring_buf->pBufBase +
		   (buf_bridge->pWrite - buf_bridge->pBufBase);

	if (writeidx >= task_ring_buf->pWrite)
		datacount = writeidx - task_ring_buf->pWrite;
	else
		datacount = task_ring_buf->bufLen -
				(task_ring_buf->pWrite - writeidx);

	RingBuf_update_writeptr(task_ring_buf, datacount);

	Ringbuf_Check(task_ring_buf);
	Ringbuf_Bridge_Check(buf_bridge);
	return 0;
}

int sync_bridge_ringbuf_readidx(struct ringbuf_bridge *buf_bridge,
				struct RingBuf *task_ring_buf)
{
	if (task_ring_buf == NULL) {
		AUD_LOG_D("%s task_ring_buf == NULL\n", __func__);
		return -1;
	} else if (buf_bridge == NULL) {
		AUD_LOG_D("%s buf_bridge == NULL\n", __func__);
		return -1;
	}

	buf_bridge->pRead = buf_bridge->pBufBase +
	(task_ring_buf->pRead - task_ring_buf->pBufBase);

	Ringbuf_Check(task_ring_buf);
	Ringbuf_Bridge_Check(buf_bridge);
	return 0;
}

int sync_bridge_ringbuf_writeidx(struct ringbuf_bridge *buf_bridge,
				 struct RingBuf *task_ring_buf)
{
	if (task_ring_buf == NULL) {
		AUD_LOG_D("%s task_ring_buf == NULL\n", __func__);
		return -1;
	} else if (buf_bridge == NULL) {
		AUD_LOG_D("%s buf_bridge == NULL\n", __func__);
		return -1;
	}

	/* buffer full */
	if (buf_bridge->pRead == buf_bridge->pWrite &&
	   buf_bridge->datacount == buf_bridge->bufLen) {
		AUD_LOG_D("%s buffer full\n", __func__);
	}

	buf_bridge->pWrite = buf_bridge->pBufBase +
		(task_ring_buf->pWrite - task_ring_buf->pBufBase);

	Ringbuf_Check(task_ring_buf);
	Ringbuf_Bridge_Check(buf_bridge);
	return 0;
}


void dump_rbuf_bridge(struct ringbuf_bridge *ring_buffer_bridge)
{
	if (ring_buffer_bridge == NULL)
		return;
#if defined(__linux__)
	pr_info("%s pBufBase = 0x%llx pBufEnd = 0x%llx pRead = 0x%llx pWrite = 0x%llx bufLen=%llu readidx = 0x%llx writeidx = 0x%llx\n",
		 __func__, ring_buffer_bridge->pBufBase,
		 ring_buffer_bridge->pBufEnd, ring_buffer_bridge->pRead,
		 ring_buffer_bridge->pWrite, ring_buffer_bridge->bufLen,
		 (ring_buffer_bridge->pRead - ring_buffer_bridge->pBufBase),
		 (ring_buffer_bridge->pWrite - ring_buffer_bridge->pBufBase)
		 );

#else
	AUD_LOG_D("%s Base = 0x%llx End = 0x%llx bufLen=%llu\n",
		  __func__, ring_buffer_bridge->pBufBase,
		  ring_buffer_bridge->pBufEnd,
		  ring_buffer_bridge->bufLen
		  );
	AUD_LOG_D("R= 0x%llx W= 0x%llx ridx = %llu widx = %llu\n",
		  ring_buffer_bridge->pRead,
		  ring_buffer_bridge->pWrite,
		  (ring_buffer_bridge->pRead - ring_buffer_bridge->pBufBase),
		  (ring_buffer_bridge->pWrite - ring_buffer_bridge->pBufBase)
		  );

#endif
}

void dump_rbuf_bridge_s(const char *appendingstring,
				    struct ringbuf_bridge *ring_buffer_bridge)
{
	if (ring_buffer_bridge == NULL)
		return;
#if defined(__linux__)
	pr_info("%s %s pBufBase = 0x%llx pBufEnd = 0x%llx pRead = 0x%llx pWrite = 0x%llx bufLen=%llu readidx = 0x%llx writeidx = 0x%llx\n",
		appendingstring, __func__, ring_buffer_bridge->pBufBase,
		ring_buffer_bridge->pBufEnd, ring_buffer_bridge->pRead,
		ring_buffer_bridge->pWrite, ring_buffer_bridge->bufLen,
		(ring_buffer_bridge->pRead - ring_buffer_bridge->pBufBase),
		(ring_buffer_bridge->pWrite - ring_buffer_bridge->pBufBase)
		);

#else
	AUD_LOG_D("%s %s Base = 0x%llx End = 0x%llx bufLen=%llu\n",
		  appendingstring, __func__, ring_buffer_bridge->pBufBase,
		  ring_buffer_bridge->pBufEnd,
		  ring_buffer_bridge->bufLen
		  );
	AUD_LOG_D("R= 0x%x W= 0x%x ridx = 0x%x widx = 0x%x\n",
		  ring_buffer_bridge->pRead,
		  ring_buffer_bridge->pWrite,
		  (ring_buffer_bridge->pRead - ring_buffer_bridge->pBufBase),
		  (ring_buffer_bridge->pWrite - ring_buffer_bridge->pBufBase)
		  );
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


void dump_buf_attr(struct buf_attr bufattr)
{
	AUD_LOG_D("%s format = %d rate = %d channel = %d\n", __func__,
		  bufattr.format, bufattr.rate, bufattr.channel);
}

void dump_audio_buffer(struct audio_buffer *audio_buf)
{
	if (audio_buf == NULL)
		return;

	AUD_LOG_D(
		"%s period_size = %d start_threshold = %d stop_threshold = %d\n",
		__func__,
		audio_buf->period_size,
		audio_buf->start_threshold,
		audio_buf->stop_threshold);

	dump_rbuf_bridge(&audio_buf->buf_bridge);
	dump_buf_attr(audio_buf->buffer_attr);
}

void dump_audio_hwbuffer(struct audio_hw_buffer *audio_hwbuf)
{
	if (audio_hwbuf == NULL)
		return;
#if defined(__linux__)
	pr_info(
		"%s hw_buffer = %d audio_memiftype = %d irq_num = %d memory_type = %d counter = %d",
		__func__, audio_hwbuf->hw_buffer,
		audio_hwbuf->audio_memiftype,
		audio_hwbuf->irq_num,
		audio_hwbuf->memory_type,
		audio_hwbuf->counter);
#else
	AUD_LOG_D(
		"%s hw_buffer = %d audio_memiftype = %d irq_num = %d memory_type = %d counter = %d",
		__func__, audio_hwbuf->hw_buffer,
		audio_hwbuf->audio_memiftype,
		audio_hwbuf->irq_num,
		audio_hwbuf->memory_type,
		audio_hwbuf->counter);

#endif
	dump_audio_buffer(&audio_hwbuf->aud_buffer);
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

#ifdef CFG_AUDIO_SUPPORT
int Get_dma_channel_memid(unsigned int mem_id)
{
	int dma_id;

	switch (mem_id) {
	case MP3_MEM_ID:
		dma_id = MP3_DMA_ID;
		break;
	case OPENDSP_MEM_ID:
		dma_id = OPENDSP_DMA_ID;
		break;
	case SMART_PA_MEM_ID:
		dma_id = SMART_PA_DMA_ID;
		break;
	case PLAYBACK_MEM_ID:
		dma_id = PLAYBACK_DMA_ID;
		break;
	case PRIMARY_MEM_ID:
		dma_id = PRIMARY_DMA_ID;
		break;
	case CAPTURE_UL1_MEM_ID:
		dma_id = CAPTURE_UL1_DMA_ID;
		break;
	case DEEPBUFFER_MEM_ID:
		dma_id = DEEPBUFFER_DMA_ID;
		break;
	case VOIP_MEM_ID:
		dma_id = VOIP_DMA_ID;
		break;
	case USB_AUDIO_MEM_ID:
		dma_id = USB_AUDIO_DMA_ID;
		break;
	case EFFECT_MEM_ID:
	dma_id = EFFECT_DMA_ID;
		break;
	default:
		AUD_LOG_D("%s not suppoer mem_id %d\n", __func__, mem_id);
		dma_id = -1;
		break;
	}
	return dma_id;
}

static int dma_length_fourbyte(unsigned int len, unsigned int dst_addr,
			       unsigned int src_addr)
{
	if (len % 4 || dst_addr % 4 || src_addr % 4)
		return 0; /* config can't four byte */
	else
		return 1;

}

int aud_dma_transaction_wrap(unsigned int dst_addr, unsigned int src_addr,
			     unsigned int len, uint8_t IsDram, int mem_id)
{
	DMA_RESULT ret = 0;
	int dma_channel = Get_dma_channel_memid(mem_id);

#ifdef CFG_VCORE_DVFS_SUPPORT
	if (IsDram)
		dvfs_enable_DRAM_resource(mem_id);
#endif
	if (dma_length_fourbyte(len, dst_addr, src_addr) == 0) {
		unsigned int need_add = 4 - (dst_addr % 4);

		ret = scp_dma_transaction(dst_addr + need_add, src_addr, len,
					  dma_channel, NO_RESERVED);
		memmove((void *)dst_addr, (void *)(dst_addr + need_add), len);
	} else
		ret = scp_dma_transaction(dst_addr,
					  src_addr,
					  len,
					  dma_channel,
					  NO_RESERVED);
#ifdef CFG_VCORE_DVFS_SUPPORT
	if (IsDram)
		dvfs_disable_DRAM_resource(mem_id);
#endif
	return ret;
}

static void dma_memcpy(const char *target, const char *source, size_t count,
		       uint8_t Dramplaystate, int mem_id)
{
	if (count == 0 || target == source) { /* nothing to do */
		return;
	}
	if (Dramplaystate == 0) { /* sram */
		aud_dma_transaction_wrap((unsigned int)target,
					 (unsigned int)source, count, false,
					 mem_id);
	} else { /* dram */
		aud_dma_transaction_wrap((unsigned int)target,
					 (unsigned int)source, count, true,
					 mem_id);
	}
}

/**
 * copy ring buffer from RingBufs(source) to RingBuft(target) with count
 * @param RingBuft ring buffer copy to
 * @param RingBufs copy from copy from
 */
int RingBuf_copyFromRingBuf_dma(struct RingBuf *RingBuft,
				struct RingBuf *RingBufs, unsigned int count,
				uint8_t IsDram, int mem_id)
{
	if (count == 0)
		return 0;

	if ((RingBuf_getDataCount(RingBufs) < count) ||
	    (RingBuf_getFreeSpace(RingBuft) < count)) {
		AUD_LOG_D("Space RingBuft %d || Data RingBufs %d < count %d\n",
			  RingBuf_getFreeSpace(RingBuft),
			  RingBuf_getDataCount(RingBufs), count);
		return 0;
	}
	if (RingBufs->pRead <= RingBufs->pWrite) {
		RingBuf_copyFromLinear_dma(RingBuft, RingBufs->pRead, count,
					   IsDram, mem_id);
		RingBufs->pRead += count;
		if (RingBufs->pRead >= RingBufs->pBufEnd)
			RingBufs->pRead -= RingBufs->bufLen;
	} else {
		unsigned int r2e = RingBufs->pBufEnd - RingBufs->pRead;

		if (r2e >= count) {
			RingBuf_copyFromLinear_dma(RingBuft, RingBufs->pRead,
						   count, IsDram, mem_id);
			RingBufs->pRead += count;
			if (RingBufs->pRead >= RingBufs->pBufEnd)
				RingBufs->pRead -= RingBufs->bufLen;
		} else {
			RingBuf_copyFromLinear_dma(RingBuft, RingBufs->pRead,
						   r2e, IsDram, mem_id);
			RingBuf_copyFromLinear_dma(RingBuft, RingBufs->pBufBase,
						   count - r2e, IsDram, mem_id);
			RingBufs->pRead = RingBufs->pBufBase + count - r2e;
		}
	}
	RingBufs->datacount += count;
	Ringbuf_Check(RingBuft);
	Ringbuf_Check(RingBufs);
	return count;
}

void RingBuf_copyToLinear_dma(char *buf, struct RingBuf *RingBuf1,
			      unsigned int count, uint8_t IsDram, int mem_id)
{
	if (count == 0)
		return;

	/* if not enough, assert */
	/* ASSERT(RingBuf_getDataCount(RingBuf1) >= count); */

	if (RingBuf_getDataCount(RingBuf1) < count)
		AUD_LOG_D("datacount(RingBuf1) %d < count %d\n",
			  RingBuf_getDataCount(RingBuf1), count);

	if (RingBuf1->pRead <= RingBuf1->pWrite) {
		dma_memcpy(buf, RingBuf1->pRead, count, IsDram, mem_id);
		RingBuf1->pRead += count;
	} else {
		unsigned int r2e = RingBuf1->pBufEnd - RingBuf1->pRead;

		if (count <= r2e) {
			dma_memcpy(buf, RingBuf1->pRead, count, IsDram, mem_id);
			RingBuf1->pRead += count;
			if (RingBuf1->pRead >= RingBuf1->pBufEnd)
				RingBuf1->pRead -= RingBuf1->bufLen;
		} else {
			dma_memcpy(buf, RingBuf1->pRead, r2e, IsDram, mem_id);
			dma_memcpy(buf + r2e, RingBuf1->pBufBase, count - r2e,
				   IsDram, mem_id);
			RingBuf1->pRead = RingBuf1->pBufBase + count - r2e;
		}
	}
	RingBuf1->datacount -= count;
	Ringbuf_Check(RingBuf1);
}

/**
 * copy ring buffer from RingBufs(source) to RingBuft(target)
 * @param RingBuft ring buffer copy to
 * @param RingBufs copy from copy from
 */
void RingBuf_copyFromRingBufAll_dma(struct RingBuf *RingBuft,
				    struct RingBuf *RingBufs, uint8_t IsDram,
				    int mem_id)
{
	/* if not enough, assert */
	/* ASSERT(RingBuf_getFreeSpace(RingBuft) >= */
	/* RingBuf_getDataCount(RingBufs)); */
	if (RingBuf_getFreeSpace(RingBuft) < RingBuf_getDataCount(RingBufs))
		AUD_LOG_D(
			"RingBuf_getFreeSpace(RingBuft) %d < RingBuf_getDataCount(RingBufs) %d\n",
			RingBuf_getFreeSpace(RingBuft),
			RingBuf_getDataCount(RingBufs));

	if (RingBufs->pRead <= RingBufs->pWrite) {
		RingBuf_copyFromLinear_dma(RingBuft, RingBufs->pRead,
					   RingBufs->pWrite - RingBufs->pRead,
					   IsDram, mem_id);
	} else {
		RingBuf_copyFromLinear_dma(RingBuft, RingBufs->pRead,
					   RingBufs->pBufEnd - RingBufs->pRead,
					   IsDram, mem_id);
		RingBuf_copyFromLinear_dma(
			RingBuft, RingBufs->pBufBase,
			RingBufs->pWrite - RingBufs->pBufBase, IsDram, mem_id);
	}
	RingBufs->pRead = RingBufs->pWrite;
	Ringbuf_Check(RingBuft);
	Ringbuf_Check(RingBufs);
}

void RingBuf_copyFromLinear_dma(struct RingBuf *RingBuf1, const char *buf,
				unsigned int count, uint8_t IsDram, int mem_id)
{
	int spaceIHave;
	char *end = RingBuf1->pBufBase + RingBuf1->bufLen;
	/* count buffer data I have */
	spaceIHave = RingBuf_getFreeSpace(RingBuf1);

	/* if not enough, assert */
	/* ASSERT(spaceIHave >= count); */
	if (spaceIHave < count) {
		AUD_LOG_D("spaceIHave %d < count %d\n", spaceIHave, count);
		return;
	}

	if (RingBuf1->pRead <= RingBuf1->pWrite) {
		int w2e = end - RingBuf1->pWrite;

		if (count <= w2e) {
			dma_memcpy(RingBuf1->pWrite, buf, count, IsDram,
				   mem_id);
			RingBuf1->pWrite += count;
			if (RingBuf1->pWrite >= end)
				RingBuf1->pWrite -= RingBuf1->bufLen;
		} else {
			dma_memcpy(RingBuf1->pWrite, buf, w2e, IsDram, mem_id);
			dma_memcpy(RingBuf1->pBufBase, buf + w2e, count - w2e,
				   IsDram, mem_id);
			RingBuf1->pWrite = RingBuf1->pBufBase + count - w2e;
		}
	} else {
		dma_memcpy(RingBuf1->pWrite, buf, count, IsDram, mem_id);
		RingBuf1->pWrite += count;
		if (RingBuf1->pWrite >= RingBuf1->pBufEnd)
			RingBuf1->pWrite -= RingBuf1->bufLen;
	}
	RingBuf1->datacount += count;
	Ringbuf_Check(RingBuf1);
}


static inline unsigned int get_sram_addr(unsigned int phyAdd)
{
	return (phyAdd -
		AFE_INTERNAL_SRAM_PHY_BASE +
		AFE_INTERNAL_SRAM_MAP_BASE);
}

/* using ringbuf_bridge and map to ringbuf */
int RingBuf_Map_RingBuf_bridge(struct ringbuf_bridge *buf_bridge,
			       struct RingBuf *pRingBuf)
{
	unsigned long phyaddr;

	if (buf_bridge == NULL) {
		AUD_LOG_D("%s buf_bridge == NULL\n", __func__);
		return -1;
	}
	if (pRingBuf == NULL) {
		AUD_LOG_D("%s RingBuf == NULL\n", __func__);
		return -1;
	}
	phyaddr = (unsigned long)buf_bridge->pBufBase;

	if (phyaddr > 0x20000000)
		pRingBuf->pBufBase = (char *)ap_to_scp(phyaddr);
	else
		pRingBuf->pBufBase = (char *)get_sram_addr(phyaddr);

	pRingBuf->bufLen = buf_bridge->bufLen;
	pRingBuf->pBufEnd = pRingBuf->pBufBase + pRingBuf->bufLen;
	pRingBuf->pRead = pRingBuf->pBufBase;
	pRingBuf->pWrite = pRingBuf->pBufBase;
	pRingBuf->datacount = buf_bridge->datacount;
	return 0;
}

#endif
