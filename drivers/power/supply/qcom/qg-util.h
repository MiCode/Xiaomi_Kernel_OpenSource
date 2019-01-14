/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2018-2019 The Linux Foundation. All rights reserved.
 */

#ifndef __QG_UTIL_H__
#define __QG_UTIL_H__

int qg_read(struct qpnp_qg *chip, u32 addr, u8 *val, int len);
int qg_write(struct qpnp_qg *chip, u32 addr, u8 *val, int len);
int qg_masked_write(struct qpnp_qg *chip, int addr, u32 mask, u32 val);
int qg_read_raw_data(struct qpnp_qg *chip, int addr, u32 *data);
int get_fifo_length(struct qpnp_qg *chip, u32 *fifo_length, bool rt);
int get_sample_count(struct qpnp_qg *chip, u32 *sample_count);
int get_sample_interval(struct qpnp_qg *chip, u32 *sample_interval);
int get_fifo_done_time(struct qpnp_qg *chip, bool rt, int *time_ms);
int get_rtc_time(unsigned long *rtc_time);
bool is_usb_present(struct qpnp_qg *chip);
bool is_dc_present(struct qpnp_qg *chip);
bool is_input_present(struct qpnp_qg *chip);
bool is_parallel_enabled(struct qpnp_qg *chip);
int qg_write_monotonic_soc(struct qpnp_qg *chip, int msoc);
int qg_get_battery_temp(struct qpnp_qg *chip, int *batt_temp);
int qg_get_battery_current(struct qpnp_qg *chip, int *ibat_ua);
int qg_get_battery_voltage(struct qpnp_qg *chip, int *vbat_uv);
int qg_get_vbat_avg(struct qpnp_qg *chip, int *vbat_uv);
s64 qg_iraw_to_ua(struct qpnp_qg *chip, int iraw);

#endif
