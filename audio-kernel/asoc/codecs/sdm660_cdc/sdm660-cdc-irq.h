/* Copyright (c) 2015-2018, The Linux Foundation. All rights reserved.
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
#ifndef __WCD9XXX_SPMI_IRQ_H__
#define __WCD9XXX_SPMI_IRQ_H__

#include <sound/soc.h>
#include <linux/spmi.h>
#include <linux/platform_device.h>
#include <linux/irq.h>
#include <linux/pm_qos.h>

extern void wcd9xxx_spmi_enable_irq(int irq);
extern void wcd9xxx_spmi_disable_irq(int irq);
extern int wcd9xxx_spmi_request_irq(int irq, irq_handler_t handler,
				const char *name, void *priv);
extern int wcd9xxx_spmi_free_irq(int irq, void *priv);
extern void wcd9xxx_spmi_set_codec(struct snd_soc_codec *codec);
extern void wcd9xxx_spmi_set_dev(struct platform_device *spmi, int i);
extern int wcd9xxx_spmi_irq_init(void);
extern void wcd9xxx_spmi_irq_exit(void);
extern int wcd9xxx_spmi_suspend(pm_message_t pmesg);
extern int wcd9xxx_spmi_resume(void);
bool wcd9xxx_spmi_lock_sleep(void);
void wcd9xxx_spmi_unlock_sleep(void);

#endif
