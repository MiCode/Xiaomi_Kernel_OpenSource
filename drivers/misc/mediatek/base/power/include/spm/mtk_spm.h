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

#ifndef __MTK_SPM_H__
#define __MTK_SPM_H__

#include <linux/kernel.h>



/************************************************************
 * FIXME: To be refined !!!
 ************************************************************/

#include <linux/io.h>
/* SUBSYS Power Status */
extern void __iomem *spm_base;
#define SPM_BASE spm_base
#define PCM_REG13_DATA  (SPM_BASE + 0x134)
#define PWR_STATUS      (SPM_BASE + 0x180)
#define PWR_STATUS_2ND  (SPM_BASE + 0x184)
#define MD1_PWR_CON     (SPM_BASE + 0x320)
#define SPM_SW_RSV_0    (SPM_BASE + 0x608)
#define SPARE_ACK_MASK  (SPM_BASE + 0x6F4)

/********************************************************************
 * FIXME: To be refined !!!
 *******************************************************************/
void *mt_spm_base_get(void);
extern int spm_load_firmware_status(void);

enum {
	WR_NONE = 0,
	WR_UART_BUSY = 1,
	WR_PCM_ASSERT = 2,
	WR_PCM_TIMER = 3,
	WR_WAKE_SRC = 4,
	WR_UNKNOWN = 5,
};

/********************************************************************
 * sspm lock spm scenario
 *******************************************************************/

#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
extern bool is_sspm_ipi_lock_spm(void);
extern void sspm_ipi_lock_spm_scenario(int start, int id, int opt,
	const char *name);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */


/********************************************************************
 * TWAM definitions for MET use only.
 *******************************************************************/

struct twam_sig {
	u32 sig0;       /* signal 0: config or status */
	u32 sig1;       /* signal 1: config or status */
	u32 sig2;       /* signal 2: config or status */
	u32 sig3;       /* signal 3: config or status */
};

/* for TWAM in MET */
typedef void (*twam_handler_t) (struct twam_sig *twamsig);
extern void spm_twam_register_handler(twam_handler_t handler);
extern twam_handler_t spm_twam_handler_get(void);
extern void spm_twam_enable_monitor(const struct twam_sig *twamsig,
	bool speed_mode);
extern void spm_twam_disable_monitor(void);
extern void spm_twam_set_idle_select(unsigned int sel);
extern void spm_twam_set_window_length(unsigned int len);
extern void spm_twam_set_mon_type(struct twam_sig *mon);


#endif /* __MTK_SPM_H__ */

