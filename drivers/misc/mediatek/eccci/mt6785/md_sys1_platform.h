/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2016 MediaTek Inc.
 */


#ifndef __MD_SYS1_PLATFORM_H__
#define __MD_SYS1_PLATFORM_H__

#include <linux/skbuff.h>
#include "ccif_hif_platform.h"
//#include <mt-plat/mtk_secure_api.h>
#include "ccci_platform.h"

struct ccci_clk_node {
	struct clk *clk_ref;
	unsigned char *clk_name;
};

struct md_pll_reg {
	void __iomem *md_top_clkSW;

	void __iomem *md_boot_stats_select;
	void __iomem *md_boot_stats;
#ifdef CCCI_PLATFORM_MT6781
	void __iomem *md_l2sram_base;
#endif
};

struct ccci_plat_ops {
	void (*init)(struct ccci_modem *md);
	void (*md_dump_reg)(unsigned int md_index);
	void (*cldma_hw_rst)(unsigned char md_id);
	void (*set_clk_cg)(struct ccci_modem *md, unsigned int on);
	int (*remap_md_reg)(struct ccci_modem *md);
	void (*lock_cldma_clock_src)(int locked);
	void (*lock_modem_clock_src)(int locked);
	void (*dump_md_bootup_status)(struct ccci_modem *md);
	void (*get_md_bootup_status)(
	struct ccci_modem *md, unsigned int *buff, int length);
	void (*debug_reg)(struct ccci_modem *md);
	int (*pccif_send)(struct ccci_modem *md, int channel_id);
	void (*check_emi_state)(struct ccci_modem *md, int polling);
	int (*soft_power_off)(struct ccci_modem *md, unsigned int mode);
	int (*soft_power_on)(struct ccci_modem *md, unsigned int mode);
	int (*start_platform)(struct ccci_modem *md);
	int (*power_on)(struct ccci_modem *md);
	int (*let_md_go)(struct ccci_modem *md);
	int (*power_off)(struct ccci_modem *md, unsigned int timeout);
	int (*vcore_config)(unsigned int md_id, unsigned int hold_req);
};

struct md_hw_info {
	/* HW info - Register Address */
	unsigned long l1_rgu_base;
	unsigned long md_boot_slave_Vector;
	unsigned long md_boot_slave_Key;
	unsigned long md_boot_slave_En;
	unsigned long ap_ccif_base;
	unsigned long md_ccif_base;
	unsigned int sram_size;
	void __iomem *md_ccif4_base;
#ifdef CCCI_PLATFORM_MT6781
	unsigned long spm_sleep_base;
#endif
	/* HW info - Interrutpt ID */
	unsigned int ap_ccif_irq0_id;
	unsigned int ap_ccif_irq1_id;
	unsigned int md_wdt_irq_id;
	unsigned int ap2md_bus_timeout_irq_id;
	void __iomem *md_pcore_pccif_base;

	/* HW info - Interrupt flags */
	unsigned long ap_ccif_irq0_flags;
	unsigned long ap_ccif_irq1_flags;
	unsigned long md_wdt_irq_flags;
	void *hif_hw_info;
	/*HW info - plat*/
	struct ccci_plat_ops *plat_ptr;
	struct ccci_plat_val *plat_val;
	unsigned int md_gen;
};


int ccci_modem_remove(struct platform_device *dev);
void ccci_modem_shutdown(struct platform_device *dev);
int ccci_modem_suspend(struct platform_device *dev, pm_message_t state);
int ccci_modem_resume(struct platform_device *dev);
int ccci_modem_pm_suspend(struct device *device);
int ccci_modem_pm_resume(struct device *device);
int ccci_modem_pm_restore_noirq(struct device *device);
void md_cd_lock_modem_clock_src(int locked);
int md_cd_vcore_config(unsigned int md_id, unsigned int hold_req);

int md_cd_low_power_notify(struct ccci_modem *md,
	enum LOW_POEWR_NOTIFY_TYPE type, int level);
int md_cd_get_modem_hw_info(struct platform_device *dev_ptr,
	struct ccci_dev_cfg *dev_cfg, struct md_hw_info *hw_info);
int md_cd_pccif_send(struct ccci_modem *md, int channel_id);
void md_cd_dump_pccif_reg(struct ccci_modem *md);

/* ADD_SYS_CORE */
extern void clk_buf_set_by_flightmode(bool is_flightmode_on);

extern unsigned int devapc_check_flag;
extern void ccci_mem_dump(int md_id, void *start_addr, int len);
extern void dump_emi_outstanding(void);
extern void ccif_set_irq_on_poweron(unsigned char hif_id);
extern void ccif_set_irq_on_poweroff(unsigned char hif_id);

#endif				/* __MD_SYS1_PLATFORM_H__ */
