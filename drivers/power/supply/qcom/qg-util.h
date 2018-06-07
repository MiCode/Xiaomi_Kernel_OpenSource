/* Copyright (c) 2018 The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
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
bool is_parallel_enabled(struct qpnp_qg *chip);
int qg_write_monotonic_soc(struct qpnp_qg *chip, int msoc);
int qg_get_battery_temp(struct qpnp_qg *chip, int *batt_temp);

#endif
