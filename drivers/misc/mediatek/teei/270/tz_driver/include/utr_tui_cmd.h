/*
 * Copyright (c) 2015-2017 MICROTRUST Incorporated
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __UT_TUI_H_
#define __UT_TUI_H_

enum disp_pwm_id_t {
	DISP_PWM0 = 0x1,
	DISP_PWM1 = 0x2,
	DISP_PWM_ALL = (DISP_PWM0 | DISP_PWM1)
};
#define disp_pwm_id_t enum disp_pwm_id_t

extern int mtkfb_set_backlight_level(unsigned int level);
extern int disp_pwm_set_backlight(disp_pwm_id_t id, int level_1024);
extern void disp_aal_notify_backlight_changed(int bl_1024);
extern void ut_down_low(struct semaphore *sema);

extern int enter_tui_flag;
extern int power_down_flag;
extern struct mutex pm_mutex;
extern unsigned long tui_display_message_buff;
extern unsigned long tui_notice_message_buff;
extern struct semaphore tui_notify_sema;

int try_send_tui_command(void);
int send_tui_display_command(unsigned long share_memory_size);
int send_tui_notice_command(unsigned long share_memory_size);
unsigned long create_tui_buff(int buff_size, unsigned int fdrv_type);
int wait_for_power_down(void);
int tui_notify_reboot(struct notifier_block *this, unsigned long code, void *x);
int __send_tui_display_command(unsigned long share_memory_size);
int __send_tui_notice_command(unsigned long share_memory_size);

#endif /* end of __UT_TUI_H_ */
