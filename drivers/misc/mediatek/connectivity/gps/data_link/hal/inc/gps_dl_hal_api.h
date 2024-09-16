/*
 * Copyright (C) 2019 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */
#ifndef _GPS_DL_HAL_API_H
#define _GPS_DL_HAL_API_H

#include "gps_dl_config.h"
#include "gps_dl_dma_buf.h"

/* provide function declaration for gps_dl_hal.c */
/* the functions are implemented in hal/gps_dl*.c */
enum gps_dl_hal_event_id {
	GPS_DL_HAL_EVT_A2D_TX_DMA_DONE,
	GPS_DL_HAL_EVT_D2A_RX_HAS_DATA,
	GPS_DL_HAL_EVT_D2A_RX_DMA_DONE,
	GPS_DL_HAL_EVT_D2A_RX_HAS_NODATA,
	GPS_DL_HAL_EVT_MCUB_HAS_IRQ,
	GPS_DL_HAL_EVT_DSP_ROM_START,
	GPS_DL_HAL_EVT_DSP_RAM_START,
	GPS_DL_HAL_EVT_A2D_TX_DMA_TIMEOUT, /* exception */
	GPS_DL_HAL_EVT_A2D_RX_DMA_TIMEOUT,
	GPS_DL_HAL_EVT_DMA_ISR_PENDING,
	GPD_DL_HAL_EVT_NUM
};

enum gps_dl_hal_poll_id {
	GPS_DL_POLL_TX_DMA_DONE,
	GPS_DL_POLL_RX_DMA_DONE,
	GPS_DL_POLL_USRT_HAS_DATA,
	GPS_DL_POLL_USRT_HAS_NODATA,
	GPS_DL_POLL_MCUB_D2A_FLAG_SET,
	GPS_DL_POLL_MAX
};

void gps_dl_hal_event_send(enum gps_dl_hal_event_id evt,
	enum gps_dl_link_id_enum link_id);

void gps_dl_hal_event_proc(enum gps_dl_hal_event_id evt,
	enum gps_dl_link_id_enum link_id, int sid_on_evt);

/* DMA operations */
enum gps_dl_hal_dma_ch_index {
	GPS_DL_DMA_LINK0_A2D,
	GPS_DL_DMA_LINK0_D2A,
	GPS_DL_DMA_LINK1_A2D,
	GPS_DL_DMA_LINK1_D2A,
	GPS_DL_DMA_CH_NUM,
};

enum gps_dl_hal_emi_user {
	GPS_DL_EMI_USER_TX_DMA0 = GPS_DL_DMA_LINK0_A2D,
	GPS_DL_EMI_USER_RX_DMA0 = GPS_DL_DMA_LINK0_D2A,
	GPS_DL_EMI_USER_TX_DMA1 = GPS_DL_DMA_LINK1_A2D,
	GPS_DL_EMI_USER_RX_DMA1 = GPS_DL_DMA_LINK1_D2A,
	GPS_DL_EMI_USER_ICAP,   /* reserved, driver not use it */
	GPS_DL_EMI_USER_GPS_ON, /* may remove this user later */
	GPS_DL_EMI_USER_NUM
};

#define DMA_CH_TO_LINK_ID(ch) (\
	((ch) == GPS_DL_DMA_LINK0_A2D || (ch) == GPS_DL_DMA_LINK0_D2A) ? GPS_DATA_LINK_ID0 : (\
	((ch) == GPS_DL_DMA_LINK1_A2D || (ch) == GPS_DL_DMA_LINK1_D2A) ? GPS_DATA_LINK_ID1 : (\
	0xFF)))

#define DMA_CH_IS_TX(ch) \
	((ch) == GPS_DL_DMA_LINK0_A2D || (ch) == GPS_DL_DMA_LINK1_A2D)

#define DMA_CH_IS_RX(ch) \
	((ch) == GPS_DL_DMA_LINK0_D2A || (ch) == GPS_DL_DMA_LINK1_D2A)

#define GET_TX_DMA_CH_OF(link_id) \
	CHOOSE_BY_LINK_ID(link_id, GPS_DL_DMA_LINK0_A2D, GPS_DL_DMA_LINK1_A2D, 0xFF)

#define GET_RX_DMA_CH_OF(link_id) \
	CHOOSE_BY_LINK_ID(link_id, GPS_DL_DMA_LINK0_D2A, GPS_DL_DMA_LINK1_D2A, 0xFF)

void gps_dl_hal_dma_init(void);

void gps_dl_hal_dma_config(enum gps_dl_hal_dma_ch_index ch);
void gps_dl_hal_dma_start(enum gps_dl_hal_dma_ch_index ch,
	struct gdl_dma_buf_entry *p_entry);
void gps_dl_hal_dma_stop(enum gps_dl_hal_dma_ch_index ch);

void gps_dl_emi_remap_calc_and_set(void);
enum GDL_RET_STATUS gps_dl_emi_remap_phy_to_bus_addr(unsigned int phy_addr, unsigned int *bus_addr);

void gps_dl_hal_emi_usage_init(void);
void gps_dl_hal_emi_usage_deinit(void);
void gps_dl_hal_emi_usage_claim(enum gps_dl_hal_emi_user user, bool use_emi);
void gps_dl_hal_a2d_tx_dma_claim_emi_usage(enum gps_dl_link_id_enum link_id, bool use_emi);
void gps_dl_hal_d2a_rx_dma_claim_emi_usage(enum gps_dl_link_id_enum link_id, bool use_emi);


#endif /* _GPS_DL_HAL_API_H */

