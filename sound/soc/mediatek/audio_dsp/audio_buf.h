/* SPDX-License-Identifier: GPL-2.0 */
/*
 * audio-buf-h --  Mediatek audio buffer
 *
 * Copyright (c) 2018 MediaTek Inc.
 * Author: Chipeng <Chipeng.chang@mediatek.com>
 */

#ifndef AUDIO_RINGBUF_H
#define AUDIO_RINGBUF_H

#if defined(__linux__)
#include <linux/kernel.h>
#include <linux/string.h>
#include <sound/memalloc.h>
#include <sound/pcm.h>
#else
#include <audio_type.h>
#include <stdio.h>
#endif

struct buf_attr {
	unsigned int format;
	unsigned int rate;
	unsigned int channel;
	unsigned int direction;
};

/*
 * real data operation with ringbuffer
 * when moving memory.
 */
struct RingBuf {
	char *pBufBase;
	char *pBufEnd;
	char *pRead;
	char *pWrite;
	int bufLen;
	int datacount;
};

struct audio_dsp_dram {
	unsigned long long phy_addr;
	unsigned long long va_addr;
	unsigned long long size;
	unsigned char *vir_addr;
};

/*
 * data infiormation with ring buffer
 * because of char* data width differnt with AP <==> dsp
 * using ringbuf_bridge provide index with ring buffer.
 */
struct ringbuf_bridge {
	unsigned long long pBufBase;
	unsigned long long pBufEnd;
	unsigned long long pRead;
	unsigned long long pWrite;
	unsigned long long bufLen;
	unsigned long long datacount;
};

struct audio_buffer {
	struct ringbuf_bridge buf_bridge;
	struct buf_attr buffer_attr;
	unsigned int start_threshold;
	unsigned int stop_threshold;
	int period_size;  /* preoid size with this hw buffer */
	int period_count; /* preoid size with this hw buffer */
};

struct audio_hw_buffer {
	struct audio_buffer aud_buffer;
	char hw_buffer;
	char audio_memiftype;   /*DL 1,2,3 */
	char irq_num; /* irq with this hw buffer */
	char memory_type; /* sram,dram */
	int counter;
	char ignore_irq;
};

struct audiohw_buffer_ops {
	unsigned int (*hwbuf_getcur)(struct audio_hw_buffer *audio_hw_buffer);
	unsigned int (*hwbuf_getlen)(struct audio_hw_buffer *audio_hw_buffer);
	unsigned int (*hwbuf_open)(struct audio_hw_buffer *audio_hw_buffer);
	unsigned int (*hwbuf_close)(struct audio_hw_buffer *audio_hw_buffer);
	unsigned int (*hwbuf_write)(struct audio_hw_buffer *audio_hw_buffer);
};

struct audio_buffer_ops {
	unsigned int (*buf_getcur)(struct audio_buffer *audio_buffer);
	unsigned int (*buf_getlen)(struct audio_buffer *audio_buffer);
	unsigned int (*buf_open)(struct audio_buffer *audio_buffer);
	unsigned int (*buf_close)(struct audio_buffer *audio_buffer);
	unsigned int (*buf_write)(struct audio_buffer *audio_buffer);
};

#ifdef CFG_AUDIO_SUPPORT
void RingBuf_copyToLinear_dma(char *buf, struct RingBuf *RingBuf1,
			      unsigned int count, uint8_t IsDram, int mem_id);
void RingBuf_copyFromLinear_dma(struct RingBuf *RingBuf1, const char *buf,
				unsigned int count, uint8_t IsDram, int mem_id);
int RingBuf_copyFromRingBuf_dma(struct RingBuf *RingBuft,
				struct RingBuf *RingBufs, unsigned int count,
				uint8_t IsDram, int mem_id);
void RingBuf_copyFromRingBufAll_dma(struct RingBuf *RingBuft,
				    struct RingBuf *RingBufs, uint8_t IsDram,
				    int mem_id);
int Get_dma_channel_memid(unsigned int mem_id);

/* using ringbuf_bridge and map to ringbuf */
int RingBuf_Map_RingBuf_bridge(struct ringbuf_bridge *buf_bridge,
			       struct RingBuf *pRingBuf);

#endif

void dump_ring_bufinfo(struct RingBuf *buf);
int init_ring_buf(struct RingBuf *buf, char *vaaddr, int size);
int init_ring_buf_bridge(struct ringbuf_bridge *buf_bridge,
			 unsigned long long paaddr, int size);

/**
 *  audio ring buffer implememation
 *  get data count
 *  get free sapce
 *  copy buffer from linear buffer/ring buffer
 */
unsigned int RingBuf_getDataCount(const struct RingBuf *RingBuf1);
unsigned int RingBuf_getFreeSpace(const struct RingBuf *RingBuf1);
void RingBuf_copyToLinear(char *buf, struct RingBuf *RingBuf1,
			  unsigned int count);
void RingBuf_copyFromLinear(struct RingBuf *RingBuf1, const char *buf,
			    unsigned int count);

int RingBuf_copyFromRingBuf(struct RingBuf *RingBuft, struct RingBuf *RingBufs,
			    unsigned int count);

/* direct set value with buffer */
void RingBuf_writeDataValue(struct RingBuf *RingBuf1, const char value,
			    const unsigned int count);
/*
 * update for write or read pointer , can use by hardware conusme data.
 */
void RingBuf_update_writeptr(struct RingBuf *RingBuf1,
			     unsigned int count);
void RingBuf_update_readptr(struct RingBuf *RingBuf1, unsigned int count);

void RingBuf_Bridge_update_writeptr(struct ringbuf_bridge *RingBuf1,
				    const unsigned int count);
void RingBuf_Bridge_update_readptr(struct ringbuf_bridge *RingBuf1,
				   const unsigned int count);

/* clear ring buffer state , including read/write pointer.*/
void RingBuf_Reset(struct RingBuf *RingBuf1);
void RingBuf_Bridge_Reset(struct ringbuf_bridge *RingBuf1);

/* clear ring buffer state , including read/write pointer.*/
int RingBuf_Clear(struct RingBuf *RingBuf1);
int RingBuf_Bridge_Clear(struct ringbuf_bridge *RingBuf1);

bool is_ringbuf_clear(struct RingBuf *ring_buf);
bool is_ringbuf_bridge_clear(struct ringbuf_bridge *ring_buf);

/* check if ringbur read write pointer */
void Ringbuf_Bridge_Check(struct ringbuf_bridge *buf_bridge);
/* check if ringbur read write pointer */
void Ringbuf_Check(struct RingBuf *RingBuf1);

int clear_audiobuffer_hw(struct audio_hw_buffer *audio_hwbuf);

/* set audio_hw_buffer attribute */
int set_audiobuffer_hw(struct audio_hw_buffer *audio_hwbuf, int hw_buffer);
int set_audiobuffer_memorytype(struct audio_hw_buffer *audio_hwbuf,
				       int memory_type);
int set_audiobuffer_audio_memiftype(struct audio_hw_buffer *audio_hwbuf,
				    int audio_memtype);
int set_audiobuffer_audio_irq_num(struct audio_hw_buffer *audio_hwbuf,
				  int irq_num);

int sync_ringbuf_readidx(struct RingBuf *task_ring_buf,
		    struct ringbuf_bridge *buf_bridge);
int sync_ringbuf_writeidx(struct RingBuf *task_ring_buf,
		     struct ringbuf_bridge *buf_bridge);
int sync_bridge_ringbuf_readidx(struct ringbuf_bridge *buf_bridge,
			  struct RingBuf *task_ring_buf);
int sync_bridge_ringbuf_writeidx(struct ringbuf_bridge *buf_bridge,
			   struct RingBuf *task_ring_buf);

/* clear audiobuffer related API */
int reset_audiobuffer(struct audio_buffer *audio_buf);
int reset_audiobuffer_hw(struct audio_hw_buffer *audio_hwbuf);

/* dump buffer realted API */
void dump_rbuf_bridge(struct ringbuf_bridge *ring_buffer_bridge);
void dump_rbuf_bridge_s(const char *appendingstring,
			struct ringbuf_bridge *ring_buffer_bridge);
void dump_rbuf_s(const char *appendingstring,
		 struct RingBuf *ring_buffer);
void dump_rbuf(struct RingBuf *ring_buffer);
void dump_buf_attr(struct buf_attr bufattr);
void dump_audio_buffer(struct audio_buffer *audio_buf);
void dump_audio_hwbuffer(struct audio_hw_buffer *audio_hwbuf);

void dump_audio_dsp_dram(struct audio_dsp_dram *dsp_dram);

/* linux OS API related */
#if defined(__linux__)

struct snd_pcm_substream;
struct snd_pcm_hw_params;
struct audio_dsp_dram;
struct snd_dma_buffer;

int snd_dmabuffer_to_audio_ring_buffer_bridge(
	struct snd_dma_buffer *dma_buffer,
	struct ringbuf_bridge *audio_ring_buf_brideg);

/* mapping audio reserved fram to snd_dma_buffer */
int release_snd_dmabuffer(struct snd_dma_buffer *dma_buffer);
int dsp_dram_to_snd_dmabuffer(struct audio_dsp_dram *dsp_dram,
			      struct snd_dma_buffer *dma_buffer);

/* mapping snd_dma_buffer audio buffer fram */
int snd_dmabuffer_to_audio_ring_buffer(struct snd_dma_buffer *dma_buffer,
				       struct RingBuf *audio_ring_buf);

int set_audiobuffer_threshold(struct audio_hw_buffer *audio_hwbuf,
			      struct snd_pcm_substream *substream);

/* using afe substream to set audio_hw_buffer */
int set_afe_audio_pcmbuf(struct audio_hw_buffer *audio_hwbuf,
			 struct snd_pcm_substream *substream);

/* set audio buffer by substream and snd_pcm_params */
int set_audiobuffer_attribute(struct audio_hw_buffer *audio_buf,
			      struct snd_pcm_substream *substream,
			      struct snd_pcm_hw_params *params,
			      int direction);

void RingBuf_copyFromUserLinear(struct RingBuf *RingBuf1, void __user *buf,
				unsigned int count);

void ringbuf_copyto_user_linear(void __user *buf, struct RingBuf *RingBuf1,
			  unsigned int count);
#endif

/* todo: change ringbuffer using full data count */

#endif /* end of AUDIO_RINGBUF_H */
