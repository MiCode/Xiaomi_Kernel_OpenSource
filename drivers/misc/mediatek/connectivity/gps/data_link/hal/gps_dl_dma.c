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
#include "gps_dl_context.h"

#include "gps_dl_hal.h"
#include "gps_dl_hal_api.h"
#include "gps_dl_hal_util.h"
#include "gps_each_link.h"

#if GPS_DL_MOCK_HAL
#include "gps_mock_hal.h"
#endif
#include "gps_dl_hw_api.h"

void gps_dl_hal_dma_init(void)
{
}

void gps_dl_hal_dma_config(enum gps_dl_hal_dma_ch_index ch)
{
}


void gps_dl_hal_a2d_tx_dma_start(enum gps_dl_link_id_enum link_id,
	struct gdl_dma_buf_entry *p_entry)
{
	struct gps_each_link *p_link = gps_dl_link_get(link_id);

	p_link->tx_dma_buf.dma_working_entry = *p_entry;

	GDL_LOGXD(link_id, "");

	if (link_id == GPS_DATA_LINK_ID0)
		gps_dl_hal_dma_start(GPS_DL_DMA_LINK0_A2D, p_entry);
	else if (link_id == GPS_DATA_LINK_ID1)
		gps_dl_hal_dma_start(GPS_DL_DMA_LINK1_A2D, p_entry);
}

void gps_dl_hal_a2d_tx_dma_stop(enum gps_dl_link_id_enum link_id)
{
	if (link_id == GPS_DATA_LINK_ID0)
		gps_dl_hal_dma_stop(GPS_DL_DMA_LINK0_A2D);
	else if (link_id == GPS_DATA_LINK_ID1)
		gps_dl_hal_dma_stop(GPS_DL_DMA_LINK1_A2D);
}

enum GDL_RET_STATUS gps_dl_hal_a2d_tx_dma_wait_until_done_and_stop_it(
	enum gps_dl_link_id_enum link_id, int timeout_usec, bool return_if_not_start)
{
	GDL_LOGXD(link_id, "");
	if (link_id == GPS_DATA_LINK_ID0) {
		return gps_dl_hw_wait_until_dma_complete_and_stop_it(
			GPS_DL_DMA_LINK0_A2D, timeout_usec, return_if_not_start);
	} else if (link_id == GPS_DATA_LINK_ID1) {
		return gps_dl_hw_wait_until_dma_complete_and_stop_it(
			GPS_DL_DMA_LINK1_A2D, timeout_usec, return_if_not_start);
	}

	return GDL_FAIL;
}

void gps_dl_hal_d2a_rx_dma_start(enum gps_dl_link_id_enum link_id,
	struct gdl_dma_buf_entry *p_entry)
{
	struct gps_each_link *p_link = gps_dl_link_get(link_id);

	ASSERT_LINK_ID(link_id, GDL_VOIDF());
	ASSERT_NOT_NULL(p_entry, GDL_VOIDF());

	p_link->rx_dma_buf.dma_working_entry = *p_entry;

	if (link_id == GPS_DATA_LINK_ID0)
		gps_dl_hal_dma_start(GPS_DL_DMA_LINK0_D2A, p_entry);
	else if (link_id == GPS_DATA_LINK_ID1)
		gps_dl_hal_dma_start(GPS_DL_DMA_LINK1_D2A, p_entry);
}

void gps_dl_hal_d2a_rx_dma_stop(enum gps_dl_link_id_enum link_id)
{
	if (link_id == GPS_DATA_LINK_ID0)
		gps_dl_hal_dma_stop(GPS_DL_DMA_LINK0_D2A);
	else if (link_id == GPS_DATA_LINK_ID1)
		gps_dl_hal_dma_stop(GPS_DL_DMA_LINK1_D2A);
}

enum GDL_RET_STATUS gps_dl_hal_d2a_rx_dma_wait_until_done(
	enum gps_dl_link_id_enum link_id, int timeout_usec)
{
	if (link_id == GPS_DATA_LINK_ID0) {
		return gps_dl_hw_wait_until_dma_complete_and_stop_it(
			GPS_DL_DMA_LINK0_D2A, timeout_usec, false);
	} else if (link_id == GPS_DATA_LINK_ID1) {
		return gps_dl_hw_wait_until_dma_complete_and_stop_it(
			GPS_DL_DMA_LINK1_D2A, timeout_usec, false);
	}

	return GDL_FAIL;
}

unsigned int gps_dl_hal_d2a_rx_dma_get_rx_len(enum gps_dl_link_id_enum link_id)
{
	return 0;
}

enum GDL_RET_STATUS gps_dl_real_dma_get_rx_write_index(
	enum gps_dl_link_id_enum link_id, unsigned int *p_write_index)
{
	enum GDL_RET_STATUS gdl_ret;
	unsigned int left_len;
	enum gps_dl_hal_dma_ch_index ch;
	struct gps_each_link *p_link = gps_dl_link_get(link_id);

	ASSERT_LINK_ID(link_id, GDL_FAIL_ASSERT);

	if (link_id == GPS_DATA_LINK_ID0)
		ch = GPS_DL_DMA_LINK0_D2A;
	else
		ch = GPS_DL_DMA_LINK1_D2A;

	left_len = gps_dl_hw_get_dma_left_len(ch);
	if (!gps_dl_is_1byte_mode())
		left_len *= 4;

	gdl_ret = gdl_dma_buf_entry_transfer_left_to_write_index(
		&p_link->rx_dma_buf.dma_working_entry, left_len, p_write_index);

	return gdl_ret;
}

enum GDL_RET_STATUS gps_dl_hal_d2a_rx_dma_get_write_index(
	enum gps_dl_link_id_enum link_id, unsigned int *p_write_index)
{
#if GPS_DL_MOCK_HAL
	unsigned int write_index_from_mock;
	enum GDL_RET_STATUS gdl_from_mock;
#endif
	enum GDL_RET_STATUS gdl_ret;

	ASSERT_NOT_NULL(p_write_index, GDL_FAIL_ASSERT);
#if GPS_DL_MOCK_HAL
	gdl_from_mock = gps_dl_mock_dma_get_rx_write_index(link_id, &write_index_from_mock);
	if (gdl_from_mock != GDL_OKAY)
		return gdl_from_mock;
#endif
	gdl_ret = gps_dl_real_dma_get_rx_write_index(link_id, p_write_index);
	GDL_LOGD("real gdl_ret = %s", gdl_ret_to_name(gdl_ret));

#if GPS_DL_MOCK_HAL
	*p_write_index = write_index_from_mock;
	return gdl_from_mock;
#else
	return gdl_ret;
#endif
}

void gps_dl_real_dma_start(enum gps_dl_hal_dma_ch_index ch,
	struct gdl_dma_buf_entry *p_entry)
{
	enum GDL_RET_STATUS gdl_ret;
	enum gps_dl_link_id_enum link_id;
	struct gdl_hw_dma_transfer dma_transfer;

	ASSERT_NOT_NULL(p_entry, GDL_VOIDF());

	link_id = DMA_CH_TO_LINK_ID(ch);
	ASSERT_LINK_ID(link_id, GDL_VOIDF());

	GDL_LOGXD(link_id, "ch = %d, r = %u, w = %u, addr = 0x%p",
		ch, p_entry->read_index, p_entry->write_index,
		p_entry->vir_addr);

	if (DMA_CH_IS_TX(ch))
		gdl_ret = gdl_dma_buf_entry_to_transfer(p_entry, &dma_transfer, true);
	else if (DMA_CH_IS_RX(ch))
		gdl_ret = gdl_dma_buf_entry_to_transfer(p_entry, &dma_transfer, false);
	else
		GDL_ASSERT(false, GDL_VOIDF(), "");
	GDL_ASSERT(gdl_ret == GDL_OKAY, GDL_VOIDF(), "gdl_ret = %d", gdl_ret);
	gps_dl_hw_set_dma_start(ch, &dma_transfer);
}

void gps_dl_hal_dma_start(enum gps_dl_hal_dma_ch_index ch,
	struct gdl_dma_buf_entry *p_entry)
{
	gps_dl_real_dma_start(ch, p_entry);
#if GPS_DL_MOCK_HAL
	gps_dl_mock_dma_start(ch, p_entry);
#endif
}

void gps_dl_hal_dma_stop(enum gps_dl_hal_dma_ch_index ch)
{
#if GPS_DL_MOCK_HAL
	gps_dl_mock_dma_stop(ch);
#endif
	gps_dl_hw_set_dma_stop(ch);
}

