/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2016 MediaTek Inc.
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

/********************************************************************
 * APIs for external modules
 *******************************************************************/
bool mtk_spm_drv_ready(void);
bool mtk_spm_base_ready(void);
unsigned int mtk_spm_read_register(int register_index);

enum {
	SPM_PWRSTA = 0,
	SPM_MD1_PWR_CON,
	SPM_REG13,
	SPM_SPARE_ACK_MASK,
};

/********************************************************************
 * FIXME: To be refined !!!
 *******************************************************************/
int mtk_spm_init(void);
void *mt_spm_base_get(void);
extern int spm_load_firmware_status(void);

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
struct twam_byte {
	u32 signal;
	u32 id;
	u32 monitor_type;
};

struct twam_cfg {
	struct twam_byte byte[4];   /* Channel 0~3 config */
};

struct twam_select {
	u32 signal[4];
	u32 id[4];
};

/* for TWAM in MET */
typedef void (*twam_handler_t) (struct twam_cfg *twamsig,
	struct twam_select *twam_sel);
extern void spm_twam_register_handler(twam_handler_t handler);
extern twam_handler_t spm_twam_handler_get(void);
extern void spm_twam_enable_monitor(bool en_monitor,
	bool debug_signal, twam_handler_t cb_handler);
extern void spm_twam_disable_monitor(void);
extern void spm_twam_set_idle_select(unsigned int sel);
extern void spm_twam_set_window_length(unsigned int len);
extern void spm_twam_set_mon_type(struct twam_cfg *mon);
extern void spm_twam_config_channel(struct twam_cfg *cfg,
	bool speed_mode, unsigned int window_len_hz);
extern bool spm_twam_met_enable(void);


#endif /* __MTK_SPM_H__ */

