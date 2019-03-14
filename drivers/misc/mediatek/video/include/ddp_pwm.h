/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Joey Pan <joey.pan@mediatek.com>
 */

#ifndef __DDP_PWM_H__
#define __DDP_PWM_H__


enum disp_pwm_id_t {
	DISP_PWM0 = 0x1,
	DISP_PWM1 = 0x2,
	DISP_PWM_ALL = (DISP_PWM0 | DISP_PWM1)
};

void disp_pwm_set_main(enum disp_pwm_id_t main);
enum disp_pwm_id_t disp_pwm_get_main(void);

int disp_pwm_is_enabled(enum disp_pwm_id_t id);

int disp_pwm_set_backlight(enum disp_pwm_id_t id, int level_1024);
int disp_pwm_set_backlight_cmdq(enum disp_pwm_id_t id,
	int level_1024, void *cmdq);

int disp_pwm_set_max_backlight(enum disp_pwm_id_t id, unsigned int level_1024);
int disp_pwm_get_max_backlight(enum disp_pwm_id_t id);

/* For backward compatible */
int disp_bls_set_max_backlight(unsigned int level_1024);
#ifdef CONFIG_MTK_FB_DUMMY
int disp_bls_set_backlight(int level_1024) { return 0; }
#else
int disp_bls_set_backlight(int level_1024);
#endif
bool disp_pwm_is_osc(void);
void disp_pwm_test(const char *cmd, char *debug_output);

#endif
