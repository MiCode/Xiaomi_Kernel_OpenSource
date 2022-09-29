/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2015 MediaTek Inc.
 */

#ifndef __MD_SYS1_PLATFORM_H__
#define __MD_SYS1_PLATFORM_H__

#include <linux/io.h>
#include <linux/skbuff.h>

#define ccci_write32(b, a, v)  \
do { \
	writel(v, (b) + (a)); \
	mb(); /* make sure register access in order */ \
} while (0)


#define ccci_write16(b, a, v)  \
do { \
	writew(v, (b) + (a)); \
	mb(); /* make sure register access in order */ \
} while (0)


#define ccci_write8(b, a, v)  \
do { \
	writeb(v, (b) + (a)); \
	mb(); /* make sure register access in order */ \
} while (0)


#define ccci_read32(b, a)               ioread32((void __iomem *)((b)+(a)))
#define ccci_read16(b, a)               ioread16((void __iomem *)((b)+(a)))
#define ccci_read8(b, a)                ioread8((void __iomem *)((b)+(a)))

extern struct regmap *syscon_regmap_lookup_by_phandle(
		struct device_node *np, const char *property);
extern int regmap_write(struct regmap *map,
		unsigned int reg, unsigned int val);
extern int regmap_read(struct regmap *map,
		unsigned int reg, unsigned int *val);

struct ccci_modem;

struct  ccci_plat_val {
	struct regmap *infra_ao_base;
	struct regmap *topckgen_clk_base;
	struct regmap *spm_sleep_base;
	unsigned int md_gen;
	unsigned int md_sub_ver;
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

struct ccci_plat_ops {
	void (*md_dump_reg)(void);
	//void (*cldma_hw_rst)(void);
	//void (*set_clk_cg)(struct ccci_modem *md, unsigned int on);
	int (*remap_md_reg)(struct ccci_modem *md);
	void (*lock_modem_clock_src)(int locked);
	void (*get_md_bootup_status)(unsigned int *buff, int length);
	void (*debug_reg)(struct ccci_modem *md, bool isr_skip_dump);
	int (*pccif_send)(struct ccci_modem *md, int channel_id);
	void (*check_emi_state)(struct ccci_modem *md, int polling);
	int (*soft_power_off)(struct ccci_modem *md, unsigned int mode);
	int (*soft_power_on)(struct ccci_modem *md, unsigned int mode);
	int (*start_platform)(struct ccci_modem *md);
	int (*power_on)(struct ccci_modem *md);
	int (*let_md_go)(struct ccci_modem *md);
	int (*power_off)(struct ccci_modem *md, unsigned int timeout);
	int (*vcore_config)(unsigned int hold_req);
};

struct md_hw_info {
	/* HW info - Register Address */
	unsigned int sram_size;

	/* HW info - Interrutpt ID */
	unsigned int ap_ccif_irq1_id;
	unsigned int md_wdt_irq_id;
	unsigned int md_epon_offset;
	void __iomem *md_l2sram_base;
	unsigned int md_l2sram_size;

	/* HW info - Interrupt flags */
	unsigned long ap_ccif_irq1_flags;
	unsigned long md_wdt_irq_flags;
	void *hif_hw_info;
	/*HW info - plat*/
	struct ccci_plat_ops *plat_ptr;
	struct ccci_plat_val *plat_val;
};

enum {
	SRCCLKENA_SETTING_BIT,
	SRCLKEN_O1_BIT,
	REVERT_SEQUENCER_BIT,
	MD_PLL_SETTING,
};


/* ADD_SYS_CORE */
int ccci_modem_syssuspend(void);
void ccci_modem_sysresume(void);
void md_dump_register_6873(void);

#if IS_ENABLED(CONFIG_MTK_EMI)
extern void mtk_emidbg_dump(void);
#endif
int Is_MD_EMI_voilation(void);

#define MD_IN_DEBUG(md) (0)

extern unsigned int ccci_get_hs2_done_status(void);
extern void reset_modem_hs2_status(void);

#endif				/* __MD_SYS1_PLATFORM_H__ */
