/*
 * kernel/power/tuxonice_power_off.h
 *
 * Copyright (C) 2006-2010 Nigel Cunningham (nigel at tuxonice net)
 *
 * This file is released under the GPLv2.
 *
 * Support for the powering down.
 */

int toi_pm_state_finish(void);
void toi_power_down(void);
extern unsigned long toi_poweroff_method;
int toi_poweroff_init(void);
void toi_poweroff_exit(void);
void toi_check_resleep(void);

extern int platform_begin(int platform_mode);
extern int platform_pre_snapshot(int platform_mode);
extern void platform_leave(int platform_mode);
extern void platform_end(int platform_mode);
extern void platform_finish(int platform_mode);
extern int platform_pre_restore(int platform_mode);
extern void platform_restore_cleanup(int platform_mode);
