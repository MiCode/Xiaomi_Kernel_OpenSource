/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2018 MediaTek Inc.
 */

#ifndef AUDIO_RINGBUF_H
#define AUDIO_RINGBUF_H

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

void reset_audio_dma_buf(struct snd_dma_buffer *dma_buf);
void dump_rbuf(struct RingBuf *ring_buffer);
void dump_rbuf_s(const char *appendingstring, struct RingBuf *ring_buffer);
int init_ring_buf(struct RingBuf *buf, char *vaaddr, int size);
void RingBuf_update_readptr(struct RingBuf *RingBuf1, unsigned int count);
unsigned int RingBuf_getFreeSpace(const struct RingBuf *RingBuf1);
void Ringbuf_Check(struct RingBuf *RingBuf1);
void RingBuf_copyFromUserLinear(struct RingBuf *RingBuf1, void __user *buf,
				unsigned int count);
void RingBuf_copyFromLinear(struct RingBuf *RingBuf1, const char *buf,
			    unsigned int count);
void RingBuf_Reset(struct RingBuf *RingBuf1);
int RingBuf_Clear(struct RingBuf *RingBuf1);


#endif /* end of AUDIO_RINGBUF_H */

