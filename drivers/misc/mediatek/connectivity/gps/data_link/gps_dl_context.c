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
#include "gps_dl_hw_api.h"

#if GPS_DL_ON_LINUX
#define GPS_DL_ISR_DATA0    gps_dl_linux_irq_dispatcher
#define GPS_DL_ISR_NODATA0  gps_dl_linux_irq_dispatcher
#define GPS_DL_ISR_MCUB0    gps_dl_linux_irq_dispatcher
#define GPS_DL_ISR_DATA1    gps_dl_linux_irq_dispatcher
#define GPS_DL_ISR_NODATA1  gps_dl_linux_irq_dispatcher
#define GPS_DL_ISR_MCUB1    gps_dl_linux_irq_dispatcher
#define GPS_DL_ISR_DMA      gps_dl_linux_irq_dispatcher
#elif GPS_DL_ON_CTP
#define GPS_DL_ISR_DATA0    gps_dl_isr_dl0_has_data
#define GPS_DL_ISR_NODATA0  gps_dl_isr_dl0_has_nodata
#define GPS_DL_ISR_MCUB0    gps_dl_isr_dl0_mcub
#define GPS_DL_ISR_DATA1    gps_dl_isr_dl1_has_data
#define GPS_DL_ISR_NODATA1  gps_dl_isr_dl1_has_nodata
#define GPS_DL_ISR_MCUB1    gps_dl_isr_dl1_mcub
#define GPS_DL_ISR_DMA      gps_dl_isr_dma_done
#endif

static struct gps_dl_ctx s_gps_dl_ctx = {
	.links = {
		/* for /dev/gpsdl0 */
		{.cfg = {.tx_buf_size = GPS_DL_TX_BUF_SIZE, .rx_buf_size = GPS_DL_RX_BUF_SIZE} },

		/* for /dev/gpsdl1 */
		{.cfg = {.tx_buf_size = GPS_DL_TX_BUF_SIZE, .rx_buf_size = GPS_DL_RX_BUF_SIZE} },
	},
	.irqs = {
		{.cfg = {.index = GPS_DL_IRQ_LINK0_DATA,   .name = "gps_da0",
			.trig_type = GPS_DL_IRQ_TRIG_LEVEL_HIGH,
			.isr = (void *)GPS_DL_ISR_DATA0} },
		{.cfg = {.index = GPS_DL_IRQ_LINK0_NODATA, .name = "gps_nd0",
			.trig_type = GPS_DL_IRQ_TRIG_LEVEL_HIGH,
			.isr = (void *)GPS_DL_ISR_NODATA0} },
		{.cfg = {.index = GPS_DL_IRQ_LINK0_MCUB,   .name = "gps_mb0",
			.trig_type = GPS_DL_IRQ_TRIG_LEVEL_HIGH,
			.isr = (void *)GPS_DL_ISR_MCUB0} },
		{.cfg = {.index = GPS_DL_IRQ_LINK1_DATA,   .name = "gps_da1",
			.trig_type = GPS_DL_IRQ_TRIG_LEVEL_HIGH,
			.isr = (void *)GPS_DL_ISR_DATA1} },
		{.cfg = {.index = GPS_DL_IRQ_LINK1_NODATA, .name = "gps_nd1",
			.trig_type = GPS_DL_IRQ_TRIG_LEVEL_HIGH,
			.isr = (void *)GPS_DL_ISR_NODATA1} },
		{.cfg = {.index = GPS_DL_IRQ_LINK1_MCUB,   .name = "gps_mb1",
			.trig_type = GPS_DL_IRQ_TRIG_LEVEL_HIGH,
			.isr = (void *)GPS_DL_ISR_MCUB1} },
		{.cfg = {.index = GPS_DL_IRQ_DMA,          .name = "gps_dma",
			.trig_type = GPS_DL_IRQ_TRIG_LEVEL_HIGH,
			.isr = (void *)GPS_DL_ISR_DMA} },
	},
#if GPS_DL_ON_LINUX
	.devices = {
		/* for /dev/gpsdl0 */
		{.cfg = {.dev_name = "gpsdl0", .index = 0} },

		/* for /dev/gpsdl1 */
		{.cfg = {.dev_name = "gpsdl1", .index = 1} },
	},
#endif /* GPS_DL_ON_LINUX */
};

static struct gps_dl_runtime_cfg s_gps_rt_cfg = {
	.dma_is_1byte_mode = false,
	.dma_is_enabled = true,
	.show_reg_rw_log = false,
	.show_reg_wait_log = true,
	.only_show_wait_done_log = true,
	.log_level = GPS_DL_LOG_DEF_SETTING_LEVEL,
	.log_mod_bitmask = GPS_DL_LOG_DEF_SETTING_MODULES,
	.log_reg_rw_bitmask = GPS_DL_LOG_REG_RW_BITMASK,
};

struct gps_each_link *gps_dl_link_get(enum gps_dl_link_id_enum link_id)
{
	if (link_id >= 0 && link_id < GPS_DATA_LINK_NUM)
		return &s_gps_dl_ctx.links[link_id];

	return NULL;
}

struct gps_each_irq *gps_dl_irq_get(enum gps_dl_irq_index_enum irq_idx)
{
	if (irq_idx >= 0 && irq_idx < GPS_DL_IRQ_NUM)
		return &s_gps_dl_ctx.irqs[irq_idx];

	return NULL;
}

#if GPS_DL_ON_LINUX
struct gps_each_device *gps_dl_device_get(enum gps_dl_link_id_enum link_id)
{
	if (link_id >= 0 && link_id < GPS_DATA_LINK_NUM)
		return &s_gps_dl_ctx.devices[link_id];

	return NULL;
}
#endif

struct gps_dl_remap_ctx *gps_dl_remap_ctx_get(void)
{
	return &s_gps_dl_ctx.remap_ctx;
}

bool gps_dl_is_1byte_mode(void)
{
	return s_gps_rt_cfg.dma_is_1byte_mode;
}

bool gps_dl_set_1byte_mode(bool is_1byte_mode)
{
	bool old = s_gps_rt_cfg.dma_is_1byte_mode;

	s_gps_rt_cfg.dma_is_1byte_mode = is_1byte_mode;
	return old;
}

bool gps_dl_is_dma_enabled(void)
{
	return s_gps_rt_cfg.dma_is_enabled;
}

bool gps_dl_set_dma_enabled(bool to_enable)
{
	bool old = s_gps_rt_cfg.dma_is_enabled;

	s_gps_rt_cfg.dma_is_enabled = to_enable;
	return old;
}

bool gps_dl_show_reg_rw_log(void)
{
	return s_gps_rt_cfg.show_reg_rw_log;
}

bool gps_dl_show_reg_wait_log(void)
{
	return s_gps_rt_cfg.show_reg_wait_log;
}

bool gps_dl_only_show_wait_done_log(void)
{
	return s_gps_rt_cfg.only_show_wait_done_log;
}

bool gps_dl_set_show_reg_rw_log(bool on)
{
	bool last_on = s_gps_rt_cfg.show_reg_rw_log;

	s_gps_rt_cfg.show_reg_rw_log = on;
	return last_on;
}

void gps_dl_set_show_reg_wait_log(bool on)
{
	s_gps_rt_cfg.show_reg_wait_log = on;
}

int gps_dl_set_rx_transfer_max(enum gps_dl_link_id_enum link_id, int max)
{
	struct gps_each_link *p_link;
	int old_max = 0;

	p_link = gps_dl_link_get(link_id);

	if (p_link) {
		old_max = p_link->rx_dma_buf.transfer_max;
		p_link->rx_dma_buf.transfer_max = max;
		GDL_LOGXD(link_id, "old_max = %d, new_max = %d", old_max, max);
	}

	return old_max;
}

int gps_dl_set_tx_transfer_max(enum gps_dl_link_id_enum link_id, int max)
{
	struct gps_each_link *p_link;
	int old_max = 0;

	p_link = gps_dl_link_get(link_id);

	if (p_link) {
		old_max = p_link->tx_dma_buf.transfer_max;
		p_link->tx_dma_buf.transfer_max = max;
		GDL_LOGXD(link_id, "old_max = %d, new_max = %d", old_max, max);
	}

	return old_max;
}

enum gps_dl_log_level_enum gps_dl_log_level_get(void)
{
	return s_gps_rt_cfg.log_level;
}

void gps_dl_log_level_set(enum gps_dl_log_level_enum level)
{
	s_gps_rt_cfg.log_level = level;
}

bool gps_dl_log_mod_is_on(enum gps_dl_log_module_enum mod)
{
	return (bool)(s_gps_rt_cfg.log_mod_bitmask & (1UL << mod));
}

void gps_dl_log_mod_on(enum gps_dl_log_module_enum mod)
{
	s_gps_rt_cfg.log_mod_bitmask |= (1UL << mod);
}

void gps_dl_log_mod_off(enum gps_dl_log_module_enum mod)
{
	s_gps_rt_cfg.log_mod_bitmask &= ~(1UL << mod);
}

void gps_dl_log_mod_bitmask_set(unsigned int bitmask)
{
	s_gps_rt_cfg.log_mod_bitmask = bitmask;
}

unsigned int gps_dl_log_mod_bitmask_get(void)
{
	return s_gps_rt_cfg.log_mod_bitmask;
}

bool gps_dl_log_reg_rw_is_on(enum gps_dl_log_reg_rw_ctrl_enum log_reg_rw)
{
	return (bool)(s_gps_rt_cfg.log_reg_rw_bitmask & (1UL << log_reg_rw));
}

void gps_dl_log_info_show(void)
{
	bool show_reg_rw_log = gps_dl_show_reg_rw_log();

	GDL_LOGE("level = %d, bitmask = 0x%08x, rrw = %d",
		s_gps_rt_cfg.log_level, s_gps_rt_cfg.log_mod_bitmask, show_reg_rw_log);
}

