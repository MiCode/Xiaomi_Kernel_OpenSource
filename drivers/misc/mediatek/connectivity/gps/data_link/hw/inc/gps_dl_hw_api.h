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
#ifndef _GPS_DL_HW_API_H
#define _GPS_DL_HW_API_H

#include "gps_dl_config.h"
#include "gps_dl_dma_buf.h"
#include "gps_dl_hal_api.h"
#include "gps_dl_hal_type.h"

enum GDL_HW_RET {
	HW_OKAY,        /* hw CRs access okay */
	E_INV_PARAMS,   /* invalid parameters */
	E_RESETTING,    /* whole chip reset is on-going */
	E_POLL_TIMEOUT, /* timeout when waiting CR change to excepted value */
	E_MAX
};

#if GPS_DL_ON_LINUX
#define GDL_HW_STATUS_POLL_INTERVAL_USEC	(1) /* 1us */
#else
#define GDL_HW_STATUS_POLL_INTERVAL_USEC	(2*1000) /* 2ms */
#endif

enum dsp_ctrl_enum {
	GPS_L1_DSP_ON,
	GPS_L1_DSP_OFF,
	GPS_L5_DSP_ON,
	GPS_L5_DSP_OFF,
	GPS_L1_DSP_ENTER_DSLEEP,
	GPS_L1_DSP_EXIT_DSLEEP,
	GPS_L1_DSP_ENTER_DSTOP,
	GPS_L1_DSP_EXIT_DSTOP,
	GPS_L5_DSP_ENTER_DSLEEP,
	GPS_L5_DSP_EXIT_DSLEEP,
	GPS_L5_DSP_ENTER_DSTOP,
	GPS_L5_DSP_EXIT_DSTOP,
	GPS_L1_DSP_CLEAR_PWR_STAT,
	GPS_L5_DSP_CLEAR_PWR_STAT,
	GPS_DSP_CTRL_MAX
};
int gps_dl_hw_gps_dsp_ctrl(enum dsp_ctrl_enum ctrl);
bool gps_dl_hw_gps_dsp_is_off_done(enum gps_dl_link_id_enum link_id);
void gps_dl_hw_gps_adie_force_off(void);
void gps_dl_hw_gps_dump_top_rf_cr(void);

int gps_dl_hw_gps_common_on(void);
int gps_dl_hw_gps_common_off(void);
bool gps_dl_hw_gps_force_wakeup_conninfra_top_off(bool enable);
void gps_dl_hw_gps_sw_request_emi_usage(bool request);

enum GDL_HW_RET gps_dl_hw_get_mcub_info(
	enum gps_dl_link_id_enum link_id, struct gps_dl_hal_mcub_info *p);

void gps_dl_hw_clear_mcub_d2a_flag(
	enum gps_dl_link_id_enum link_id, unsigned int d2a_flag);

unsigned int gps_dl_hw_get_mcub_a2d_flag(enum gps_dl_link_id_enum link_id);

enum GDL_RET_STATUS gps_dl_hw_mcub_dsp_read_request(
	enum gps_dl_link_id_enum link_id, u16 dsp_addr);

void gps_dl_hw_set_gps_emi_remapping(unsigned int _20msb_of_36bit_phy_addr);
unsigned int gps_dl_hw_get_gps_emi_remapping(void);

void gps_dl_hw_set_dma_start(enum gps_dl_hal_dma_ch_index channel,
	struct gdl_hw_dma_transfer *p_transfer);

void gps_dl_hw_set_dma_stop(enum gps_dl_hal_dma_ch_index channel);

bool gps_dl_hw_get_dma_int_status(enum gps_dl_hal_dma_ch_index channel);

unsigned int gps_dl_hw_get_dma_left_len(enum gps_dl_hal_dma_ch_index channel);

struct gps_dl_hw_dma_status_struct {
	unsigned int wrap_count;
	unsigned int wrap_to_addr;
	unsigned int total_count;
	unsigned int config;
	unsigned int start_flag;
	unsigned int intr_flag;
	unsigned int left_count;
	unsigned int curr_addr;
	unsigned int state;
};

void gps_dl_hw_save_dma_status_struct(
	enum gps_dl_hal_dma_ch_index ch, struct gps_dl_hw_dma_status_struct *p);

void gps_dl_hw_print_dma_status_struct(
	enum gps_dl_hal_dma_ch_index ch, struct gps_dl_hw_dma_status_struct *p);

enum GDL_RET_STATUS gps_dl_hw_wait_until_dma_complete_and_stop_it(
	enum gps_dl_hal_dma_ch_index ch, int timeout_usec, bool return_if_not_start);


struct gps_dl_hw_usrt_status_struct {
	unsigned int ctrl_setting;
	unsigned int intr_enable;
	unsigned int state;
	unsigned int mcub_d2a_flag;
	unsigned int mcub_d2a_d0;
	unsigned int mcub_d2a_d1;
	unsigned int mcub_a2d_flag;
	unsigned int mcub_a2d_d0;
	unsigned int mcub_a2d_d1;
};

/* TODO: replace gps_dl_hw_usrt_status_struct */
struct gps_dl_hw_link_status_struct {
	bool usrt_has_data;
	bool usrt_has_nodata;
	bool rx_dma_done;
	bool tx_dma_done;
};

void gps_dl_hw_get_link_status(
	enum gps_dl_link_id_enum link_id, struct gps_dl_hw_link_status_struct *p);

void gps_dl_hw_save_usrt_status_struct(
	enum gps_dl_link_id_enum link_id, struct gps_dl_hw_usrt_status_struct *p);

void gps_dl_hw_print_usrt_status_struct(
	enum gps_dl_link_id_enum link_id, struct gps_dl_hw_usrt_status_struct *p);

void gps_dl_hw_dump_sleep_prot_status(void);
void gps_dl_hw_dump_host_csr_gps_info(bool force_show_log);
void gps_dl_hw_dump_host_csr_conninfra_info(bool force_show_log);
void gps_dl_hw_print_hw_status(enum gps_dl_link_id_enum link_id, bool dump_rf_cr);
void gps_dl_hw_do_gps_a2z_dump(void);
void gps_dl_hw_print_usrt_status(enum gps_dl_link_id_enum link_id);
bool gps_dl_hw_poll_usrt_dsp_rx_empty(enum gps_dl_link_id_enum link_id);
void gps_dl_hw_gps_dump_gps_rf_cr(void);
void gps_dl_hw_print_ms_counter_status(void);

void gps_dl_hw_switch_dsp_jtag(void);

void gps_dl_hw_usrt_rx_irq_enable(
	enum gps_dl_link_id_enum link_id, bool enable);
void gps_dl_hw_usrt_ctrl(enum gps_dl_link_id_enum link_id,
	bool is_on, bool is_dma_mode, bool is_1byte_mode);
void gps_dl_hw_usrt_clear_nodata_irq(enum gps_dl_link_id_enum link_id);
bool gps_dl_hw_usrt_has_set_nodata_flag(enum gps_dl_link_id_enum link_id);


bool gps_dl_hw_is_pta_clk_cfg_ready(void);
void gps_dl_hw_set_ptk_clk_cfg(void);

bool gps_dl_hw_is_pta_uart_init_done(void);
bool gps_dl_hw_init_pta_uart(void);
void gps_dl_hw_deinit_pta_uart(void);

bool gps_dl_hw_is_pta_init_done(void);
void gps_dl_hw_init_pta(void);
void gps_dl_hw_deinit_pta(void);

void gps_dl_hw_claim_pta_used_by_gps(void);
void gps_dl_hw_disclaim_pta_used_by_gps(void);
void gps_dl_hw_set_pta_blanking_parameter(bool use_direct_path);

bool gps_dl_hw_take_conn_coex_hw_sema(unsigned int timeout_ms);
void gps_dl_hw_give_conn_coex_hw_sema(void);
bool gps_dl_hw_take_conn_rfspi_hw_sema(unsigned int timeout_ms);
void gps_dl_hw_give_conn_rfspi_hw_sema(void);

unsigned int gps_dl_hw_get_mcub_a2d1_cfg(enum gps_dl_link_id_enum link_id, bool is_1byte_mode);

#endif /* _GPS_DL_HW_API_H */

