/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __ASM_ARCH_DMA_H
#define __ASM_ARCH_DMA_H

#define MAX_DMA_ADDRESS     (0xFFFFFFFF)
#define MAX_DMA_CHANNELS    (0)

#endif				/* !__ASM_ARCH_DMA_H */

#ifndef __MT_DMA_H__
#define __MT_DMA_H__

/* define DMA channels */
enum {
	G_DMA_1 = 0, G_DMA_2,
	P_DMA_AP_HIF, P_DMA_MD_HIF,
	P_DMA_SIM1, P_DMA_SIM2,
	P_DMA_IRDA,
	P_DMA_UART1_TX, P_DMA_UART1_RX,
	P_DMA_UART2_TX, P_DMA_UART2_RX,
	P_DMA_UART3_TX, P_DMA_UART3_RX,
};

/* define DMA error code */
enum {
	DMA_ERR_CH_BUSY = 1,
	DMA_ERR_INVALID_CH = 2,
	DMA_ERR_CH_FREE = 3,
	DMA_ERR_NO_FREE_CH = 4,
	DMA_ERR_INV_CONFIG = 5,
};

/* define DMA ISR callback function's prototype */
typedef void (*DMA_ISR_CALLBACK) (void *);

/*
 * NoteXXX: Implementation below is obsolete and deprecated.
 */

#include <linux/types.h>

typedef u32 INFO;

enum {
	DMA_FALSE = 0,
	DMA_TRUE
};

enum {
	DMA_OK = 0,
	DMA_FAIL
};

enum {
	REMAINING_LENGTH = 0,	/* not valid for virtual FIFO */
	VF_READPTR,		/* only valid for virtual FIFO */
	VF_WRITEPTR,		/* only valid for virtual FIFO */
	VF_FFCNT,		/* only valid for virtual FIFO */
	VF_ALERT,		/* only valid for virtual FIFO */
	VF_EMPTY,		/* only valid for virtual FIFO */
	VF_FULL,		/* only valid for virtual FIFO */
	VF_PORT
};

enum {
	GDMA_1 = 0,
	GDMA_2,
	GDMA_ANY
};

enum {
	ALL = 0,
	SRC,
	DST,
	SRC_AND_DST
};

/* define GDMA configurations */
struct mt_gdma_conf {
	unsigned int count;
	int iten;
	unsigned int burst;
	int dfix;
	int sfix;
	unsigned int limiter;
	dma_addr_t src;
	dma_addr_t dst;
	dma_addr_t jump;
	int wpen;
	int wpsd;
	unsigned int wplen;
	unsigned int wpto;
	/* unsigned int cohen; */
	unsigned int domain;
	void (*isr_cb)(void *);
	void *data;
};

/* burst */
#define DMA_CON_BURST_SINGLE    (0x00000000)
#define DMA_CON_BURST_2BEAT     (0x00010000)
#define DMA_CON_BURST_3BEAT     (0x00020000)
#define DMA_CON_BURST_4BEAT     (0x00030000)
#define DMA_CON_BURST_5BEAT     (0x00040000)
#define DMA_CON_BURST_6BEAT     (0x00050000)
#define DMA_CON_BURST_7BEAT     (0x00060000)
#define DMA_CON_BURST_8BEAT     (0x00070000)

/* size */
/* keep for backward compatibility only */
#define DMA_CON_SIZE_BYTE   (0x00000000)
#define DMA_CON_SIZE_SHORT  (0x00000001)
#define DMA_CON_SIZE_LONG   (0x00000002)

extern int mt65xx_free_gdma(int channel);
extern int mt65xx_req_gdma(DMA_ISR_CALLBACK cb, void *data);
extern int mt65xx_start_gdma(int channel);
extern int mt65xx_stop_gdma(int channel);
extern void mt_reset_dma(const unsigned int iChannel);
extern void mt65xx_dma_running_status(void);
extern void mt_reset_gdma_conf(const unsigned int iChannel);

extern int mt_config_gdma(int channel, struct mt_gdma_conf *config, int flag);
extern int mt_free_gdma(int channel);
extern int mt_req_gdma(int chan);
extern int mt_start_gdma(int channel);
extern int mt_polling_gdma(int channel, unsigned long timeout);
extern int mt_stop_gdma(int channel);
extern int mt_dump_gdma(int channel);
extern int mt_warm_reset_gdma(int channel);
extern int mt_hard_reset_gdma(int channel);
extern int mt_reset_gdma(int channel);
extern void mt_dma_running_status(void);
/* This channel is used for APDMA Dummy READ.
 * in MT6592 this channel will be used by Frequency hopping all the time
 * .Owner: Chieh-Jay Liu
 */
#define DFS_APDMA_CHANNEL 0

#endif				/* !__MT_DMA_H__ */
