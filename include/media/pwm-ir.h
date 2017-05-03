/* Copyright (C) 2013 by Xiang Xiao <xiaoxiang@xiaomi.com>
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

#ifndef __PWM_IR_H__
#define __PWM_IR_H__

#define PWM_IR_NAME	"pwm-ir"

struct pwm_ir_data {
	const char  *reg_id;
	int          pwm_id;
	bool         low_active;
	bool         use_timer;
};

#endif /* __PWM_IR_H__ */
