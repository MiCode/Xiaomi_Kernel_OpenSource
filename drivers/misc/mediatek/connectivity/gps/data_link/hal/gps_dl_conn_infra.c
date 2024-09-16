/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#include "gps_dl_config.h"

#include "gps_dl_context.h"
#include "gps_dl_hw_ver.h"
#include "gps_dl_hw_api.h"
#include "gps_dl_hal_api.h"
#if GPS_DL_HAS_PLAT_DRV
#include "gps_dl_linux_reserved_mem.h"
#endif

#define GPS_EMI_REMAP_BASE_MASK (0xFFFFF0000) /* start from 64KB boundary, get msb20 of 36bit */
#define GPS_EMI_REMAP_LENGTH    (64 * 1024 * 1024UL)
#define GPS_EMI_BUS_BASE        (0x78000000)

void gps_dl_emi_remap_set(unsigned int min_addr, unsigned int max_addr)
{
	unsigned int aligned_addr = 0;
	unsigned int _20msb_of_36bit_phy_addr;

	/* TODO: addr may not use uint, due to addr might be 36bit and uint might be only 32bit */
	aligned_addr = (min_addr & GPS_EMI_REMAP_BASE_MASK);


	if (max_addr - aligned_addr > GPS_EMI_REMAP_LENGTH) {
		GDL_LOGE("min = 0x%09x, max = 0x%09x, base = 0x%09x, over range",
			min_addr, max_addr, aligned_addr);
	} else {
		GDL_LOGD("min = 0x%09x, max = 0x%09x, base = 0x%09x",
			min_addr, max_addr, aligned_addr);
	}

	_20msb_of_36bit_phy_addr = aligned_addr >> 16;
	GDL_LOGD("icap_buf: remap setting = 0x%08x", _20msb_of_36bit_phy_addr);
	gps_dl_hw_set_gps_emi_remapping(_20msb_of_36bit_phy_addr);
	gps_dl_remap_ctx_get()->gps_emi_phy_high20 = aligned_addr;
}

enum GDL_RET_STATUS gps_dl_emi_remap_phy_to_bus_addr(unsigned int phy_addr, unsigned int *bus_addr)
{
	unsigned int remap_setting = gps_dl_remap_ctx_get()->gps_emi_phy_high20;

	if ((phy_addr >= remap_setting) &&
		(phy_addr < (remap_setting + GPS_EMI_REMAP_LENGTH))) {
		*bus_addr = GPS_EMI_BUS_BASE + (phy_addr - remap_setting);
		return GDL_OKAY;
	}

	*bus_addr = 0;
	return GDL_FAIL;
}

void gps_dl_emi_remap_calc_and_set(void)
{
	enum gps_dl_link_id_enum  i;
	struct gps_each_link *p_link;

	unsigned int min_addr = 0xFFFFFFFF;
	unsigned int max_addr = 0;
	unsigned int tx_end;
	unsigned int rx_end;

#if GPS_DL_HAS_PLAT_DRV
	if (gps_dl_reserved_mem_is_ready()) {
		gps_dl_reserved_mem_show_info();
		gps_dl_reserved_mem_get_range(&min_addr, &max_addr);
		gps_dl_emi_remap_set(min_addr, max_addr);
		return;
	}
#endif

	for (i = 0; i < GPS_DATA_LINK_NUM; i++) {
		p_link = gps_dl_link_get(i);

		min_addr = (p_link->rx_dma_buf.phy_addr < min_addr) ? p_link->rx_dma_buf.phy_addr : min_addr;
		min_addr = (p_link->tx_dma_buf.phy_addr < min_addr) ? p_link->tx_dma_buf.phy_addr : min_addr;

		rx_end = p_link->rx_dma_buf.phy_addr + p_link->rx_dma_buf.len;
		tx_end = p_link->tx_dma_buf.phy_addr + p_link->tx_dma_buf.len;

		max_addr = (rx_end > min_addr) ? rx_end : max_addr;
		max_addr = (tx_end > min_addr) ? tx_end : max_addr;
	}
	GDL_LOGD("cal from dma buffers: min = 0x%x, max = 0x%x", min_addr, max_addr);
	gps_dl_emi_remap_set(min_addr, max_addr);
}


unsigned int g_gps_dl_hal_conn_infra_poll_ok_ver;

void gps_dl_hal_set_conn_infra_ver(unsigned int ver)
{
	g_gps_dl_hal_conn_infra_poll_ok_ver = ver;
}

unsigned int gps_dl_hal_get_conn_infra_ver(void)
{
	return g_gps_dl_hal_conn_infra_poll_ok_ver;
}

bool gps_dl_hal_conn_infra_ver_is_mt6885(void)
{
	/* is_mt6885 valid after gps_dl_hw_gps_common_on */
	return (gps_dl_hal_get_conn_infra_ver() == GDL_HW_CONN_INFRA_VER_MT6885);
}

