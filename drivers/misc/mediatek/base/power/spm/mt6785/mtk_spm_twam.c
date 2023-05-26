/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>

#include <mtk_spm_internal.h>
#include "mtk_sspm.h"

static twam_handler_t spm_twam_handler;
static unsigned int idle_sel;
static struct twam_cfg twamcfg;
static unsigned int twam_speed_mode;
static unsigned int twam_window_len;
static unsigned int twam_running;
static unsigned int twam_speed_mode;
static unsigned int twam_window_len;
void spm_twam_set_idle_select(unsigned int sel)
{
	idle_sel = sel & 0x3;
}
EXPORT_SYMBOL(spm_twam_set_idle_select);

static unsigned int window_len;
void spm_twam_set_window_length(unsigned int len)
{
	window_len = len;
	twam_window_len = len;
}
EXPORT_SYMBOL(spm_twam_set_window_length);

void spm_twam_set_mon_type(struct twam_cfg *mon)
{
	int index = 0;

	if (mon)
		for (index = 0; index < 4; index++)
			twamcfg.byte[index].monitor_type =
				mon->byte[index].monitor_type;
}
EXPORT_SYMBOL(spm_twam_set_mon_type);

void spm_twam_register_handler(twam_handler_t handler)
{
	spm_twam_handler = handler;
}
EXPORT_SYMBOL(spm_twam_register_handler);

twam_handler_t spm_twam_handler_get(void)
{
	return spm_twam_handler;
}
EXPORT_SYMBOL(spm_twam_handler_get);

void spm_twam_config_channel(struct twam_cfg *cfg, bool speed_mode,
	unsigned int window_len_hz)
{
	memcpy(&twamcfg, cfg, sizeof(struct twam_cfg));
	twam_speed_mode = speed_mode;
	twam_window_len = window_len_hz;
}
EXPORT_SYMBOL(spm_twam_config_channel);

bool spm_twam_met_enable(void)
{
	return twam_running;
}
EXPORT_SYMBOL(spm_twam_met_enable);

void spm_twam_enable_monitor(bool en_monitor,
	bool debug_signal, twam_handler_t cb_handler)
{
	unsigned long flags;

	struct spm_data spm_d;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
	int ret;
#endif

	if (en_monitor == true) {
		if (twam_running == true)
			return;
	} else {
		if (twam_running == false)
			return;
		spm_twam_handler = NULL;
		spm_d.u.args.args1 = 0;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		spm_to_sspm_command(SPM_TWAM_ENABLE, &spm_d);
#endif
		twam_running = false;
			return;
	}

	spin_lock_irqsave(&__spm_lock, flags);

	spm_twam_handler = cb_handler;
	spm_d.u.args.args1 = 1;
	spm_d.u.args.args2 = ~ISRM_TWAM;
	spm_d.u.args.args3 = (((twamcfg.byte[0].monitor_type & 0x3) << 4) |
			   ((twamcfg.byte[1].monitor_type & 0x3) << 6) |
			   ((twamcfg.byte[2].monitor_type & 0x3) << 8) |
			   ((twamcfg.byte[3].monitor_type & 0x3) << 10) |
			   (twam_speed_mode ? REG_TWAM_SPEED_MODE_EN_LSB : 0) |
			   REG_TWAM_ENABLE_LSB);
	spm_d.u.args.args4 = (((twamcfg.byte[0].id & 0x1f) |
			   (twamcfg.byte[0].signal & 0x3) << 5) << 0) |
			   (((twamcfg.byte[1].id & 0x1f) |
			   (twamcfg.byte[1].signal & 0x3) << 5) << 8) |
			   (((twamcfg.byte[2].id & 0x1f) |
			   (twamcfg.byte[2].signal & 0x3) << 5) << 16) |
			   (((twamcfg.byte[3].id & 0x1f) |
			   (twamcfg.byte[3].signal & 0x3) << 5) << 24);
	spm_d.u.args.args5 = twam_window_len;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
	ret = spm_to_sspm_command(SPM_TWAM_ENABLE, &spm_d);
#endif
	twam_running = true;

	spin_unlock_irqrestore(&__spm_lock, flags);

	printk_deferred("[name:spm&][SPM] enable TWAM for signal %u, %u, %u, %u (%u)\n",
		  twamcfg.byte[0].id, twamcfg.byte[1].id, twamcfg.byte[2].id,
		  twamcfg.byte[3].id, twam_speed_mode);
}
EXPORT_SYMBOL(spm_twam_enable_monitor);

void spm_twam_disable_monitor(void)
{
	unsigned long flags;

	spin_lock_irqsave(&__spm_lock, flags);

	spm_write(SPM_TWAM_CON, spm_read(SPM_TWAM_CON) & ~REG_TWAM_ENABLE_LSB);
	spm_write(SPM_IRQ_MASK, spm_read(SPM_IRQ_MASK) | ISRM_TWAM);
	spm_write(SPM_IRQ_STA, ISRC_TWAM);
	spin_unlock_irqrestore(&__spm_lock, flags);

	printk_deferred("[name:spm&]disable TWAM\n");
}
EXPORT_SYMBOL(spm_twam_disable_monitor);

