/*
 * Copyright (C) 2017 MediaTek Inc.
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

#include <linux/module.h>
#include <linux/kernel.h>

#include <mtk_spm.h>	/* SPM TWAM API */
#include <mtk_idle_internal.h>

/* SPM TAWM */
#define TRIGGER_TYPE                (2) /* b'10: high */
#define TWAM_PERIOD_MS              (1000)
#define WINDOW_LEN_SPEED            (TWAM_PERIOD_MS * 0x65B8)
#define WINDOW_LEN_NORMAL           (TWAM_PERIOD_MS * 0xD)
#define GET_EVENT_RATIO_SPEED(x)    ((x)/(WINDOW_LEN_SPEED/1000))
#define GET_EVENT_RATIO_NORMAL(x)   ((x)/(WINDOW_LEN_NORMAL/1000))


/********************************************************************
 * Weak functions for chip dependent flow.
 *******************************************************************/

/* [ByChip] Internal weak functions: implemented in mtk_spm_irq.c */
void __attribute__((weak)) spm_twam_register_handler(twam_handler_t handler) {}
void __attribute__((weak)) spm_twam_enable_monitor(
	const struct twam_sig *twamsig, bool speed_mode) {}
void __attribute__((weak)) spm_twam_disable_monitor(void) {}
void __attribute__((weak)) spm_twam_set_idle_select(unsigned int sel) {}
void __attribute__((weak)) spm_twam_set_window_length(unsigned int len) {}
void __attribute__((weak)) spm_twam_set_mon_type(struct twam_sig *mon) {}

static struct mtk_idle_twam idle_twam = {
	.running = false,
	.speed_mode = true,
	.sel = 0,
	.event = 29,
};

struct mtk_idle_twam *mtk_idle_get_twam(void)
{
	return &idle_twam;
}

static void mtk_idle_twam_callback(struct twam_sig *ts)
{
	pr_notice("Power/swap spm twam (sel%d: %d) ratio: %5u/1000\n",
		idle_twam.sel, idle_twam.event,
		(idle_twam.speed_mode) ? GET_EVENT_RATIO_SPEED(ts->sig0) :
			GET_EVENT_RATIO_NORMAL(ts->sig0));
}

void mtk_idle_twam_disable(void)
{
	if (idle_twam.running == false)
		return;
	spm_twam_register_handler(NULL);
	spm_twam_disable_monitor();
	idle_twam.running = false;
}

void mtk_idle_twam_enable(unsigned int event)
{
	struct twam_sig montype = {0};
	struct twam_sig twamsig = {0};

	if (idle_twam.event != event)
		mtk_idle_twam_disable();

	if (idle_twam.running == true)
		return;

	idle_twam.event = (event < 32) ? event : 29;
	twamsig.sig0 = idle_twam.event;
	montype.sig0 = TRIGGER_TYPE;
	spm_twam_set_mon_type(&montype);
	spm_twam_set_window_length(
		(idle_twam.speed_mode) ? WINDOW_LEN_SPEED : WINDOW_LEN_NORMAL);
	spm_twam_register_handler(mtk_idle_twam_callback);
	spm_twam_set_idle_select(idle_twam.sel);
	spm_twam_enable_monitor(&twamsig, idle_twam.speed_mode);
	idle_twam.running = true;
}

