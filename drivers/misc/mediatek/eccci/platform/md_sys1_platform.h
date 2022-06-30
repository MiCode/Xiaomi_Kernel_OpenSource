/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2015 MediaTek Inc.
 */

#ifndef __MD_SYS1_PLATFORM_H__
#define __MD_SYS1_PLATFORM_H__

#include <linux/skbuff.h>

struct  ccci_plat_val {
	struct regmap *infra_ao_base;
	struct regmap *topckgen_clk_base;
	struct regmap *spm_sleep_base;
	unsigned int md_gen;
	unsigned long offset_epof_md1;
	void __iomem *md_plat_info;
	unsigned int power_flow_config;
	int srclken_o1_bit;
	unsigned int md_first_power_on;
};

struct ccci_clk_node {
	struct clk *clk_ref;
	unsigned char *clk_name;
};

struct md_pll_reg {
	void __iomem *md_top_clkSW;

	void __iomem *md_boot_stats_select;
	void __iomem *md_boot_stats;
};
struct ccci_plat_ops {
	void (*init)(struct ccci_modem *md);
	void (*md_dump_reg)(unsigned int md_index);
	//void (*cldma_hw_rst)(unsigned char md_id);
	void (*set_clk_cg)(struct ccci_modem *md, unsigned int on);
	int (*remap_md_reg)(struct ccci_modem *md);
	void (*lock_modem_clock_src)(int locked);
	void (*get_md_bootup_status)(unsigned int *buff, int length);
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
	unsigned long md_rgu_base;
	unsigned long md_boot_slave_Vector;
	unsigned long md_boot_slave_Key;
	unsigned int sram_size;

	/* HW info - Interrutpt ID */
	unsigned int ap_ccif_irq1_id;
	unsigned int md_wdt_irq_id;
	unsigned int md_epon_offset;
	void __iomem *md_l2sram_base;

	/* HW info - Interrupt flags */
	unsigned long ap_ccif_irq1_flags;
	unsigned long md_wdt_irq_flags;
	void *hif_hw_info;
	/*HW info - plat*/
	struct ccci_plat_ops *plat_ptr;
	struct ccci_plat_val *plat_val;
};

struct cldma_hw_info {
	unsigned long cldma_ap_ao_base;
	unsigned long cldma_ap_pdn_base;
	unsigned int cldma_irq_id;
	unsigned long cldma_irq_flags;
};

enum {
	SRCCLKENA_SETTING_BIT,
	SRCLKEN_O1_BIT,
	REVERT_SEQUENCER_BIT,
	MD_PLL_SETTING,
	SKIP_TOPCLK_BIT,
};

extern unsigned int ap_plat_info;

/* ADD_SYS_CORE */
int ccci_modem_syssuspend(void);
void ccci_modem_sysresume(void);
void md_dump_register_6873(unsigned int md_index);

extern void ccci_mem_dump(int md_id, void *start_addr, int len);
extern void dump_emi_outstanding(void);
extern void mtk_emidbg_dump(void);
extern unsigned int ccci_get_hs2_done_status(void);
extern void reset_modem_hs2_status(void);


#endif				/* __MD_SYS1_PLATFORM_H__ */
