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
#include "gps_dl_config.h"
#include "gps_dl_isr.h"
#include "gps_dl_hal.h"
#include "gps_dl_hw_api.h"
#include "gps_dl_hal_util.h"
#include "gps_dsp_fsm.h"
#include "gps_each_link.h"
#include "gps/gps_usrt_apb.h"
#include "gps/gps_l5_usrt_apb.h"

/* TODO: IRQ hwirq, irq_id, gdl_irq_index */
/* On CTP: hwirq == irq_id */
/* On Linux: hwirq != irq_id */
#if GPS_DL_ON_CTP
/* x_define_irq.h 198 -> 415 */
#define GPS_DL_IRQ_BASE_ID			(GPS_L1_IRQ_BUS_BIT0_ID)
#else
#define GPS_DL_IRQ_BASE_ID          (383) /* (415 - 32) */
#endif

#define GPS_IRQ_ID_DL0_HAS_DATA     (GPS_DL_IRQ_BASE_ID + 0)
#define GPS_IRQ_ID_DL0_HAS_NODATA   (GPS_DL_IRQ_BASE_ID + 1)
#define GPS_IRQ_ID_DL0_MCUB         (GPS_DL_IRQ_BASE_ID + 2)
#define GPS_IRQ_ID_DL1_HAS_DATA     (GPS_DL_IRQ_BASE_ID + 3)
#define GPS_IRQ_ID_DL1_HAS_NODATA   (GPS_DL_IRQ_BASE_ID + 4)
#define GPS_IRQ_ID_DL1_MCUB         (GPS_DL_IRQ_BASE_ID + 5)
#define GPS_IRQ_ID_DMA_DONE         (GPS_DL_IRQ_BASE_ID + 6)

#if GPS_DL_ON_LINUX
/* TODO: hwirq and linux irq id */
int g_gps_irq_index_to_id_table[GPS_DL_IRQ_NUM];
void gps_dl_irq_set_id(enum gps_dl_irq_index_enum irq_idx, int irq_id)
{
	g_gps_irq_index_to_id_table[irq_idx] = irq_id;
}
#else
int g_gps_irq_index_to_id_table[GPS_DL_IRQ_NUM] = {
	[GPS_DL_IRQ_LINK0_DATA]   = GPS_IRQ_ID_DL0_HAS_DATA,
	[GPS_DL_IRQ_LINK0_NODATA] = GPS_IRQ_ID_DL0_HAS_NODATA,
	[GPS_DL_IRQ_LINK0_MCUB]   = GPS_IRQ_ID_DL0_MCUB,
	[GPS_DL_IRQ_LINK1_DATA]   = GPS_IRQ_ID_DL1_HAS_DATA,
	[GPS_DL_IRQ_LINK1_NODATA] = GPS_IRQ_ID_DL1_HAS_NODATA,
	[GPS_DL_IRQ_LINK1_MCUB]   = GPS_IRQ_ID_DL1_MCUB,
	[GPS_DL_IRQ_DMA]          = GPS_IRQ_ID_DMA_DONE,
};
#endif

int gps_dl_irq_index_to_id(enum gps_dl_irq_index_enum irq_idx)
{
	ASSERT_IRQ_IDX(irq_idx, -1);
	return g_gps_irq_index_to_id_table[irq_idx];
}

int g_gps_irq_type_to_hwirq_table[GPS_DATA_LINK_NUM][GPS_DL_IRQ_TYPE_NUM] = {
	[GPS_DATA_LINK_ID0][GPS_DL_IRQ_TYPE_HAS_DATA] =
		GPS_IRQ_ID_DL0_HAS_DATA,
	[GPS_DATA_LINK_ID0][GPS_DL_IRQ_TYPE_HAS_NODATA] =
		GPS_IRQ_ID_DL0_HAS_NODATA,
	[GPS_DATA_LINK_ID0][GPS_DL_IRQ_TYPE_MCUB] =
		GPS_IRQ_ID_DL0_MCUB,

	[GPS_DATA_LINK_ID1][GPS_DL_IRQ_TYPE_HAS_DATA] =
		GPS_IRQ_ID_DL1_HAS_DATA,
	[GPS_DATA_LINK_ID1][GPS_DL_IRQ_TYPE_HAS_NODATA] =
		GPS_IRQ_ID_DL1_HAS_NODATA,
	[GPS_DATA_LINK_ID1][GPS_DL_IRQ_TYPE_MCUB] =
		GPS_IRQ_ID_DL1_MCUB,
};

int gps_dl_irq_type_to_hwirq(enum gps_dl_link_id_enum link_id,
	enum gps_dl_each_link_irq_type irq_type)
{
	ASSERT_LINK_ID(link_id, -1);
	ASSERT_IRQ_TYPE(irq_type, -1);
	return g_gps_irq_type_to_hwirq_table[link_id][irq_type];
}

int g_gps_irq_type_to_index_table[GPS_DATA_LINK_NUM][GPS_DL_IRQ_TYPE_NUM] = {
	[GPS_DATA_LINK_ID0][GPS_DL_IRQ_TYPE_HAS_DATA] =
		GPS_DL_IRQ_LINK0_DATA,
	[GPS_DATA_LINK_ID0][GPS_DL_IRQ_TYPE_HAS_NODATA] =
		GPS_DL_IRQ_LINK0_NODATA,
	[GPS_DATA_LINK_ID0][GPS_DL_IRQ_TYPE_MCUB] =
		GPS_DL_IRQ_LINK0_MCUB,

	[GPS_DATA_LINK_ID1][GPS_DL_IRQ_TYPE_HAS_DATA] =
		GPS_DL_IRQ_LINK1_DATA,
	[GPS_DATA_LINK_ID1][GPS_DL_IRQ_TYPE_HAS_NODATA] =
		GPS_DL_IRQ_LINK1_NODATA,
	[GPS_DATA_LINK_ID1][GPS_DL_IRQ_TYPE_MCUB] =
		GPS_DL_IRQ_LINK1_MCUB,

};

int gps_dl_irq_type_to_id(enum gps_dl_link_id_enum link_id,
	enum gps_dl_each_link_irq_type irq_type)
{
	int irq_index;
	int irq_id;

	ASSERT_LINK_ID(link_id, -1);
	ASSERT_IRQ_TYPE(irq_type, -1);

	irq_index = g_gps_irq_type_to_index_table[link_id][irq_type];
	irq_id = g_gps_irq_index_to_id_table[irq_index];

	return irq_id;
}

void gps_dl_irq_each_link_control(enum gps_dl_link_id_enum link_id,
	enum gps_dl_each_link_irq_type irq_type, bool mask_it,
	enum gps_dl_irq_ctrl_from from)
{
	int irq_id;

	ASSERT_LINK_ID(link_id, GDL_VOIDF());

	irq_id = gps_dl_irq_type_to_id(link_id, irq_type);

	GDL_LOGXD(link_id, "irq ctrl: from = %d, mask = %d, type = %d, irq_id = %d",
		from, mask_it, irq_type, irq_id);
#if !GPS_DL_HW_IS_MOCK
	if (mask_it)
		gps_dl_irq_mask(irq_id, from);
	else
		gps_dl_irq_unmask(irq_id, from);
#endif
}

void gps_dl_irq_each_link_mask(enum gps_dl_link_id_enum link_id,
	enum gps_dl_each_link_irq_type irq_type, enum gps_dl_irq_ctrl_from from)
{
#if (GPS_DL_ON_CTP && !GPS_DL_NO_USE_IRQ)
	if (irq_type == GPS_DL_IRQ_TYPE_HAS_DATA &&
		gps_dl_link_is_ready_to_write(link_id))
		/* Note: CTP isr main always unmask ARM IRQ when return. */
		/* we need irq not go for some cases, so musk it form GPS side. */
		gps_dl_hw_usrt_rx_irq_enable(link_id, false);
#endif
	gps_dl_irq_each_link_control(link_id, irq_type, true, from);
}

void gps_dl_irq_each_link_unmask(enum gps_dl_link_id_enum link_id,
	enum gps_dl_each_link_irq_type irq_type, enum gps_dl_irq_ctrl_from from)
{
#if (GPS_DL_ON_CTP && !GPS_DL_NO_USE_IRQ)
	if (irq_type == GPS_DL_IRQ_TYPE_HAS_DATA &&
		gps_dl_link_is_ready_to_write(link_id))
		gps_dl_hw_usrt_rx_irq_enable(link_id, true);
#endif
	gps_dl_irq_each_link_control(link_id, irq_type, false, from);
}

void gps_dl_irq_mask_dma_intr(enum gps_dl_irq_ctrl_from from)
{
	GDL_LOGD("from = %d", from);
#if !GPS_DL_HW_IS_MOCK
	gps_dl_irq_mask(gps_dl_irq_index_to_id(GPS_DL_IRQ_DMA), from);
#endif
}

void gps_dl_irq_unmask_dma_intr(enum gps_dl_irq_ctrl_from from)
{
	GDL_LOGD("from = %d", from);
#if !GPS_DL_HW_IS_MOCK
	gps_dl_irq_unmask(gps_dl_irq_index_to_id(GPS_DL_IRQ_DMA), from);
#endif
}

void gps_dl_isr_usrt_has_data(enum gps_dl_link_id_enum link_id)
{
	GDL_LOGXD(link_id, "start");
	gps_dl_irq_each_link_mask(link_id, GPS_DL_IRQ_TYPE_HAS_DATA, GPS_DL_IRQ_CTRL_POSSIBLE_FROM_ISR);

	if (gps_each_link_is_active(link_id))
		gps_dl_hal_event_send(GPS_DL_HAL_EVT_D2A_RX_HAS_DATA, link_id);
	else {
		/* NOTE: ctrld has already unmask it, still unmask here to keep balance */
		gps_dl_irq_each_link_unmask(link_id, GPS_DL_IRQ_TYPE_HAS_DATA,
			GPS_DL_IRQ_CTRL_POSSIBLE_FROM_ISR);
		GDL_LOGXD(link_id, "bypass due to link not active");
	}
}

void gps_dl_isr_usrt_has_nodata(enum gps_dl_link_id_enum link_id)
{
	GDL_LOGXD(link_id, "ch = %d", GET_RX_DMA_CH_OF(link_id));

	gps_dl_irq_each_link_mask(link_id, GPS_DL_IRQ_TYPE_HAS_NODATA, GPS_DL_IRQ_CTRL_POSSIBLE_FROM_ISR);

	if (gps_each_link_is_active(link_id))
		gps_dl_hal_event_send(GPS_DL_HAL_EVT_D2A_RX_HAS_NODATA, link_id);
	else {
		gps_dl_irq_each_link_unmask(link_id, GPS_DL_IRQ_TYPE_HAS_NODATA,
			GPS_DL_IRQ_CTRL_POSSIBLE_FROM_ISR);
		GDL_LOGXD(link_id, "bypass due to link not active");
	}
}

void gps_dl_isr_d2a_rx_dma_done(enum gps_dl_link_id_enum link_id)
{
	GDL_LOGXD(link_id, "ch = %d", GET_RX_DMA_CH_OF(link_id));

	/* gps_dl_irq_each_link_mask(link_id, GPS_DL_IRQ_TYPE_MCUB); */
	gps_dl_hal_d2a_rx_dma_stop(link_id);

	gps_dl_hal_event_send(GPS_DL_HAL_EVT_D2A_RX_DMA_DONE, link_id);
}

void gps_dl_isr_a2d_tx_dma_done(enum gps_dl_link_id_enum link_id)
{
	GDL_LOGXD(link_id, "ch = %d", GET_TX_DMA_CH_OF(link_id));

	/* gps_dl_irq_mask_dma_intr(); */
	gps_dl_hal_a2d_tx_dma_stop(link_id);

	/* update dma buf write pointor */
	/* notify controller thread */
	gps_dl_hal_event_send(GPS_DL_HAL_EVT_A2D_TX_DMA_DONE, link_id);

	/* by ctrld thread determine whether start next dma session */
}

void gps_dl_isr_dma_done(void)
{
	enum gps_dl_hal_dma_ch_index i;

	gps_dl_irq_mask_dma_intr(GPS_DL_IRQ_CTRL_POSSIBLE_FROM_ISR);

	if (!gps_dl_conninfra_is_readable()) {
		/* set it for gps_ctrld to check, avoiding twice mask */
		gps_dl_hal_set_dma_irq_en_flag(false);
		gps_dl_hal_event_send(GPS_DL_HAL_EVT_DMA_ISR_PENDING, GPS_DATA_LINK_ID0);
		gps_dl_hal_event_send(GPS_DL_HAL_EVT_DMA_ISR_PENDING, GPS_DATA_LINK_ID1);
		GDL_LOGE("pending due to readable check fail");
		return;
	}

	/* dma isr must copy the data and restore the intr flag
	 * no need to copy data, the data is copied in ctrld thread
	 */

	/* TODO: not always starts on i = 0 to make it's fair for each DMA ch */
	for (i = 0; i < GPS_DL_DMA_CH_NUM; i++) {
		/* TODO: is the dma ch is active */

		if (gps_dl_hw_get_dma_int_status(i)) {
#if 0
			DMA_Stop((kal_uint8)(index));
			set_dma_acki(index);
			while (DMA_CheckRunStat(index))
				;
			DMA_Clock_Disable(index);
#endif
			switch (i) {
			case GPS_DL_DMA_LINK0_A2D:
				gps_dl_isr_a2d_tx_dma_done(GPS_DATA_LINK_ID0);
				break;
			case GPS_DL_DMA_LINK0_D2A:
				gps_dl_isr_d2a_rx_dma_done(GPS_DATA_LINK_ID0);
				break;
			case GPS_DL_DMA_LINK1_A2D:
				gps_dl_isr_a2d_tx_dma_done(GPS_DATA_LINK_ID1);
				break;
			case GPS_DL_DMA_LINK1_D2A:
				gps_dl_isr_d2a_rx_dma_done(GPS_DATA_LINK_ID1);
				break;
			default:
				break;
			}
		}
	}
	/* TODO: end for-loop until all DMA is stopped */

	gps_dl_irq_unmask_dma_intr(GPS_DL_IRQ_CTRL_POSSIBLE_FROM_ISR);
}

void gps_dl_isr_mcub(enum gps_dl_link_id_enum link_id)
{
	GDL_LOGXD(link_id, "start");
	gps_dl_irq_each_link_mask(link_id, GPS_DL_IRQ_TYPE_MCUB, GPS_DL_IRQ_CTRL_FROM_ISR);

	if (gps_each_link_is_active(link_id))
		gps_dl_hal_event_send(GPS_DL_HAL_EVT_MCUB_HAS_IRQ, link_id);
	else {
		gps_dl_irq_each_link_unmask(link_id, GPS_DL_IRQ_TYPE_MCUB,
			GPS_DL_IRQ_CTRL_POSSIBLE_FROM_ISR);
		GDL_LOGXD(link_id, "bypass due to link not active");
	}
}

#if GPS_DL_ON_CTP
void gps_dl_isr_dl0_has_data(void)
{
	gps_dl_isr_usrt_has_data(GPS_DATA_LINK_ID0);
}

void gps_dl_isr_dl0_has_nodata(void)
{
	gps_dl_isr_usrt_has_nodata(GPS_DATA_LINK_ID0);
}

void gps_dl_isr_dl0_mcub(void)
{
	gps_dl_isr_mcub(GPS_DATA_LINK_ID0);
}

void gps_dl_isr_dl1_has_data(void)
{
	gps_dl_isr_usrt_has_data(GPS_DATA_LINK_ID1);
}

void gps_dl_isr_dl1_has_nodata(void)
{
	gps_dl_isr_usrt_has_nodata(GPS_DATA_LINK_ID1);
}

void gps_dl_isr_dl1_mcub(void)
{
	gps_dl_isr_mcub(GPS_DATA_LINK_ID1);
}
#endif

