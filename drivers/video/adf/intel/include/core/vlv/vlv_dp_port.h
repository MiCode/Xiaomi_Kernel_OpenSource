/*
 * Copyright (C) 2014, Intel Corporation.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * Author: Sivakumar Thulasimani <sivakumar.thulasimani@intel.com>
 */

#ifndef _VLV_DP_PORT_H_
#define _VLV_DP_PORT_H_

#include <core/vlv/vlv_dc_config.h>
#include <core/vlv/vlv_dc_regs.h>

struct edp_pps_delays {
	u16 t1_t3;
	u16 t8;
	u16 t9;
	u16 t10;
	u16 t11_t12;
};

struct vlv_dp_port {
	u32 offset;
	u32 pipe_select_val;
	u32 aux_ctl_offset;
	u32 pp_ctl_offset;
	u32 pp_stat_offset;
	u32 pp_on_delay_offset;
	u32 pp_off_delay_offset;
	u32 pp_divisor_offset;
	u32 pwm_ctl_offset;
	u32 pwm_duty_cycle_offset;
	u32 duty_cycle_delay;
	u32 hist_guard_offset;
	u32 hist_ctl_offset;

	enum port port_id;
	bool is_edp;
	struct edp_pps_delays pps_delays;
	const char *name;
	struct device *dev;
	struct mutex hw_mutex;
	/* for reading edid */
	struct i2c_adapter ddc;
};

bool vlv_dp_port_init(struct vlv_dp_port *port, enum port port_id,
	enum pipe pipe_id, enum intel_pipe_type type, struct device *dev);
void vlv_dp_port_destroy(struct vlv_dp_port *port);

u32 vlv_dp_port_set_link_pattern(struct vlv_dp_port *port,
		u8 train_pattern);
void vlv_dp_port_get_adjust_train(struct vlv_dp_port *port,
	struct link_params *params);
u32 vlv_dp_port_set_signal_levels(struct vlv_dp_port *port,
	struct link_params *params, u32 *deemp, u32 *margin);
u32 vlv_dp_port_aux_transfer(struct vlv_dp_port *port,
		struct dp_aux_msg *msg);
bool vlv_dp_port_is_screen_connected(struct vlv_dp_port *port);
u32 vlv_dp_port_disable(struct vlv_dp_port *port);
u32 vlv_dp_port_enable(struct vlv_dp_port *port, u32 flags,
		union encoder_params *params);
u32 vlv_dp_port_backlight_seq(struct vlv_dp_port *port, bool enable);
u32 vlv_dp_port_pwm_seq(struct vlv_dp_port *port, bool enable);
u32 vlv_dp_port_panel_power_seq(struct vlv_dp_port *port, bool enable);
void vlv_dp_port_get_max_vswing_preemp(struct vlv_dp_port *port,
	enum vswing_level *max_v, enum preemp_level *max_p);
struct i2c_adapter *vlv_dp_port_get_i2c_adapter(struct vlv_dp_port *port);
u32 vlv_dp_port_set_brightness(struct vlv_dp_port *port, int level);
u32 vlv_dp_port_get_brightness(struct vlv_dp_port *port);
#endif /* _VLV_DP_PORT_H_ */
