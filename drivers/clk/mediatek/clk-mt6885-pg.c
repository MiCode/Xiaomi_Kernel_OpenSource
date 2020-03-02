/*
 * Copyright (c) 2019 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/clk.h>
#include <linux/of.h>
#include <linux/of_address.h>

#include <linux/io.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/clkdev.h>

#include <linux/clk-provider.h>
#include <linux/clk.h>
#include "clk-mtk-v1.h"
#include "clk-mt6885-pg.h"

#include <dt-bindings/clock/mt6885-clk.h>


#define MT_CCF_DEBUG	0
#define MT_CCF_BRINGUP  0
#define CONTROL_LIMIT 1

#define	CHECK_PWR_ST	1

#define CONN_TIMEOUT_RECOVERY	6
#define CONN_TIMEOUT_STEP1	5

#ifndef GENMASK
#define GENMASK(h, l)	(((U32_C(1) << ((h) - (l) + 1)) - 1) << (l))
#endif

#ifdef CONFIG_ARM64
#define IOMEM(a)	((void __force __iomem *)((a)))
#endif

#define mt_reg_sync_writel(v, a) \
	do { \
		__raw_writel((v), IOMEM(a)); \
		/* sync up */ \
		mb(); } \
while (0)
#define spm_read(addr)			__raw_readl(IOMEM(addr))
#define spm_write(addr, val)		mt_reg_sync_writel(val, addr)

#define clk_writel(addr, val)   \
	mt_reg_sync_writel(val, addr)

#define clk_readl(addr)			__raw_readl(IOMEM(addr))

void __attribute__((weak)) mtk_wcn_cmb_stub_clock_fail_dump(void) {}

/*MM Bus*/
#if 1
#ifdef CONFIG_OF
void __iomem *clk_mmsys_config_base;
void __iomem *clk_imgsys_base;
void __iomem *clk_ipesys_base;
void __iomem *clk_vdec_gcon_base;
void __iomem *clk_venc_gcon_base;
void __iomem *clk_camsys_base;
void __iomem *clk_apu_vcore_base;
void __iomem *clk_apu_conn_base;
#endif
#endif

/*
 * MTCMOS
 */

#define STA_POWER_DOWN	0
#define STA_POWER_ON	1
#define SUBSYS_PWR_DOWN		0
#define SUBSYS_PWR_ON		1

struct subsys;

struct subsys_ops {
	int (*enable)(struct subsys *sys);
	int (*disable)(struct subsys *sys);
	int (*get_state)(struct subsys *sys);
};

struct subsys {
	const char *name;
	uint32_t sta_mask;
	void __iomem *ctl_addr;
	uint32_t sram_pdn_bits;
	uint32_t sram_pdn_ack_bits;
	uint32_t bus_prot_mask;
	struct subsys_ops *ops;
};

/*static struct subsys_ops general_sys_ops;*/
static struct subsys_ops MD1_sys_ops;

static void __iomem *infracfg_base;/*infracfg_ao*/
static void __iomem *spm_base;
static void __iomem *infra_base;/*infracfg*/
static void __iomem *ckgen_base;/*ckgen*/
static void __iomem *smi_common_base;

#define INFRACFG_REG(offset)		(infracfg_base + offset)
#define SPM_REG(offset)			(spm_base + offset)
#define INFRA_REG(offset)	(infra_base + offset)
#define CKGEN_REG(offset)	(ckgen_base + offset)
#define SMI_COMMON_REG(offset)	(smi_common_base + offset)

#if 1
#define POWERON_CONFIG_EN	SPM_REG(0x0000)
#define PWR_STATUS		SPM_REG(0x0160)
#define PWR_STATUS_2ND		SPM_REG(0x0164)

#define MD1_PWR_CON	SPM_REG(0x318)

#define MD_EXT_BUCK_ISO_CON	SPM_REG(0x3B0)
#define EXT_BUCK_ISO		SPM_REG(0x3B4)

#define INFRA_TOPAXI_SI0_CTL		INFRACFG_REG(0x0200)
#define INFRA_TOPAXI_SI0_CTL_SET	INFRACFG_REG(0x03B8)
#define INFRA_TOPAXI_SI0_CTL_CLR	INFRACFG_REG(0x03BC)

#define INFRA_TOPAXI_SI2_CTL	INFRACFG_REG(0x0234)
#define INFRA_TOPAXI_PROTECTEN		INFRACFG_REG(0x0220)
#define INFRA_TOPAXI_PROTECTEN_STA0	INFRACFG_REG(0x0224)
#define INFRA_TOPAXI_PROTECTEN_STA1	INFRACFG_REG(0x0228)
#define INFRA_TOPAXI_PROTECTEN_1   INFRACFG_REG(0x0250)
#define INFRA_TOPAXI_PROTECTEN_STA0_1 INFRACFG_REG(0x0254)
#define INFRA_TOPAXI_PROTECTEN_STA1_1 INFRACFG_REG(0x0258)

#define INFRA_TOPAXI_PROTECTEN_2_SET	INFRACFG_REG(0x0714)
#define INFRA_TOPAXI_PROTECTEN_2_CLR	INFRACFG_REG(0x0718)
#define INFRA_TOPAXI_PROTECTEN_STA1_2	INFRACFG_REG(0x0724)


#define INFRA_TOPAXI_PROTECTEN_SET	INFRACFG_REG(0x02A0)
#define INFRA_TOPAXI_PROTECTEN_CLR	INFRACFG_REG(0x02A4)
#define INFRA_TOPAXI_PROTECTEN_1_SET	INFRACFG_REG(0x02A8)
#define INFRA_TOPAXI_PROTECTEN_1_CLR	INFRACFG_REG(0x02AC)

#define INFRA_TOPAXI_PROTECTEN_MM	INFRACFG_REG(0x02D0)
#define INFRA_TOPAXI_PROTECTEN_MM_SET	INFRACFG_REG(0x02D4)
#define INFRA_TOPAXI_PROTECTEN_MM_CLR	INFRACFG_REG(0x02D8)
#define INFRA_TOPAXI_PROTECTEN_MM_STA0	INFRACFG_REG(0x02E8)
#define INFRA_TOPAXI_PROTECTEN_MM_STA1	INFRACFG_REG(0x02EC)

#define INFRA_TOPAXI_PROTECTEN_MCU_SET	INFRACFG_REG(0x02C4)
#define INFRA_TOPAXI_PROTECTEN_MCU_CLR	INFRACFG_REG(0x02C8)
#define INFRA_TOPAXI_PROTECTEN_MCU_STA0	INFRACFG_REG(0x02E0)
#define INFRA_TOPAXI_PROTECTEN_MCU_STA1	INFRACFG_REG(0x02E4)
/* INFRACFG */
#define INFRA_TOPAXI_SI0_STA		INFRA_REG(0x0000)
#define INFRA_TOPAXI_SI2_STA		INFRA_REG(0x0028)
/* SMI LARB */

/* SMI COMMON */
#define SMI_COMMON_SMI_CLAMP	SMI_COMMON_REG(0x03C0)
#define SMI_COMMON_SMI_CLAMP_SET	SMI_COMMON_REG(0x03C4)
#define SMI_COMMON_SMI_CLAMP_CLR	SMI_COMMON_REG(0x03C8)


#endif

#define  SPM_PROJECT_CODE    0xB16

/* Define MBIST EFUSE control */

/* Define MTCMOS power control */
#define PWR_RST_B                        (0x1 << 0)
#define PWR_ISO                          (0x1 << 1)
#define PWR_ON                           (0x1 << 2)
#define PWR_ON_2ND                       (0x1 << 3)
#define PWR_CLK_DIS                      (0x1 << 4)
#define SRAM_CKISO                       (0x1 << 5)
#define SRAM_ISOINT_B                    (0x1 << 6)
#define DORMANT_ENABLE                   (0x1 << 6)
#define SLPB_CLAMP                       (0x1 << 7)
#define VPROC_EXT_OFF                    (0x1 << 7)

/* Define MTCMOS Bus Protect Mask */
#define MD1_PROT_STEP1_0_MASK            ((0x1 << 7))
#define MD1_PROT_STEP1_0_ACK_MASK        ((0x1 << 7))
#define MD1_PROT_STEP2_0_MASK            ((0x1 << 3) \
					  |(0x1 << 4))
#define MD1_PROT_STEP2_0_ACK_MASK        ((0x1 << 3) \
					  |(0x1 << 4))
#define MD1_PROT_STEP2_1_MASK            ((0x1 << 6))
#define MD1_PROT_STEP2_1_ACK_MASK        ((0x1 << 6))

/* Define MTCMOS Power Status Mask */

#define MD1_PWR_STA_MASK                 (0x1 << 0)

/* Define CPU SRAM Mask */

/* Define Non-CPU SRAM Mask */


static struct subsys syss[] =	/* NR_SYSS *//* FIXME: set correct value */
{
	[SYS_MD1] = {
		     .name = __stringify(SYS_MD1),
		     .sta_mask = PWR_ON,/*MD1_PWR_STA_MASK,*/
		     /* .ctl_addr = NULL,  */
		     .sram_pdn_bits = 0,
		     .sram_pdn_ack_bits = 0,
		     .bus_prot_mask = 0,
		     .ops = &MD1_sys_ops,
		     },

};

LIST_HEAD(pgcb_list);

struct pg_callbacks *register_pg_callback(struct pg_callbacks *pgcb)
{
	INIT_LIST_HEAD(&pgcb->list);

	list_add(&pgcb->list, &pgcb_list);

	return pgcb;
}

static struct subsys *id_to_sys(unsigned int id)
{
	return id < NR_SYSS ? &syss[id] : NULL;
}

/* sync from mtcmos_ctrl.c  */
#ifdef CONFIG_MTK_RAM_CONSOLE
#if 0 /*add after early porting*/
static void aee_clk_data_rest(void)
{
	aee_rr_rec_clk(0, 0);
	aee_rr_rec_clk(1, 0);
	aee_rr_rec_clk(2, 0);
	aee_rr_rec_clk(3, 0);
}
#endif
#endif

#define DBG_ID_MD1 0

#define ID_MADK   0xFF000000
#define STA_MASK  0x00F00000
#define STEP_MASK 0x000000FF

#define INCREASE_STEPS \
	do { DBG_STEP++; } while (0)

static int DBG_ID;
static int DBG_STA;
static int DBG_STEP;
/*
 * ram console data0 define
 * [31:24] : DBG_ID
 * [23:20] : DBG_STA
 * [7:0] : DBG_STEP
 */
static void ram_console_update(void)
{
#ifdef CONFIG_MTK_RAM_CONSOLE
	struct pg_callbacks *pgcb;
	u32 data[8] = {0x0};
	u32 i = 0, j = 0;
	static u32 pre_data;
	static int k;
	static bool print_once = true;

	data[i] = ((DBG_ID << 24) & ID_MADK)
		| ((DBG_STA << 20) & STA_MASK)
		| (DBG_STEP & STEP_MASK);

	data[++i] = clk_readl(INFRA_TOPAXI_PROTECTEN);
	data[++i] = clk_readl(INFRA_TOPAXI_PROTECTEN_1);
	data[++i] = clk_readl(INFRA_TOPAXI_PROTECTEN_STA1);
	/*data[++i] = clk_readl(INFRA_TOPAXI_PROTECTEN_STA1_1);*/
	/*data[++i] = clk_readl(INFRA_TOPAXI_PROTECTEN_MM);*/
	data[++i] = clk_readl(INFRA_TOPAXI_PROTECTEN_MM_STA1);
	/*data[++i] = clk_readl(SMI_COMMON_SMI_CLAMP);*/
	data[++i] = clk_readl(PWR_STATUS);
	data[++i] = clk_readl(PWR_STATUS_2ND);
	data[++i] = clk_readl(CAM_PWR_CON);


	if (pre_data == data[0])
		k++;
	else if (pre_data != data[0]) {
		k = 0;
		pre_data = data[0];
		print_once = true;
	}

	if (k > 10000 && print_once) {
		print_once = false;
		k = 0;

		print_enabled_clks_once();

		pr_notice("%s: clk = 0x%08x\n", __func__, data[0]);
		pr_notice("%s: INFRA_TOPAXI_PROTECTEN = 0x%08x\n",
			__func__, clk_readl(INFRA_TOPAXI_PROTECTEN));
		pr_notice("%s: INFRA_TOPAXI_PROTECTEN_STA0 = 0x%08x\n",
			__func__, clk_readl(INFRA_TOPAXI_PROTECTEN_STA0));
		pr_notice("%s: INFRA_TOPAXI_PROTECTEN_STA1 = 0x%08x\n",
			__func__, clk_readl(INFRA_TOPAXI_PROTECTEN_STA1));

		pr_notice("%s: INFRA_TOPAXI_PROTECTEN_1 = 0x%08x\n",
			__func__, clk_readl(INFRA_TOPAXI_PROTECTEN_1));
		pr_notice("%s: INFRA_TOPAXI_PROTECTEN_STA0_1 = 0x%08x\n",
			__func__, clk_readl(INFRA_TOPAXI_PROTECTEN_STA0_1));
		pr_notice("%s: INFRA_TOPAXI_PROTECTEN_STA1_1 = 0x%08x\n",
			__func__, clk_readl(INFRA_TOPAXI_PROTECTEN_STA1_1));

		pr_notice("%s: INFRA_TOPAXI_PROTECTEN_MM = 0x%08x\n",
			__func__, clk_readl(INFRA_TOPAXI_PROTECTEN_MM));
		pr_notice("%s: INFRA_TOPAXI_PROTECTEN_MM_STA1 = 0x%08x\n",
			__func__, clk_readl(INFRA_TOPAXI_PROTECTEN_MM_STA1));
		pr_notice("%s: SMI_COMMON_SMI_CLAMP = 0x%08x\n",
			__func__, clk_readl(SMI_COMMON_SMI_CLAMP));


		list_for_each_entry_reverse(pgcb, &pgcb_list, list) {
			if (pgcb->debug_dump)
				pgcb->debug_dump(DBG_ID);
		}

	}
	for (j = 0; j <= i; j++)
		aee_rr_rec_clk(j, data[j]);
	/*todo: add each domain's debug register to ram console*/
#endif
}

/* auto-gen begin*/
int spm_mtcmos_ctrl_md1(int state)
{
	int err = 0;

	DBG_ID = DBG_ID_MD1;
	DBG_STA = state;
	DBG_STEP = 0;

	/* TINFO="enable SPM register control" */

	if (state == STA_POWER_DOWN) {
		/* TINFO="Start to turn off MD1" */
		/* TINFO="Set bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_SET, MD1_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1)
			& MD1_PROT_STEP1_0_ACK_MASK)
			!= MD1_PROT_STEP1_0_ACK_MASK)
			ram_console_update();
		INCREASE_STEPS;
#endif
		/* TINFO="Set bus protect - step2 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_SET, MD1_PROT_STEP2_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1)
			& MD1_PROT_STEP2_0_ACK_MASK)
			!= MD1_PROT_STEP2_0_ACK_MASK)
			ram_console_update();
		INCREASE_STEPS;
#endif
		/* TINFO="Set bus protect - step2 : 1" */
		spm_write(INFRA_TOPAXI_PROTECTEN_1_SET, MD1_PROT_STEP2_1_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1_1)
			& MD1_PROT_STEP2_1_ACK_MASK)
			!= MD1_PROT_STEP2_1_ACK_MASK)
			ram_console_update();
		INCREASE_STEPS;
#endif
		/* TINFO="Set PWR_ON = 0" */
		spm_write(MD1_PWR_CON, spm_read(MD1_PWR_CON) & ~PWR_ON);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until MD1_PWR_STA_MASK = 0" */
		while (spm_read(PWR_STATUS) & MD1_PWR_STA_MASK)
			ram_console_update();
		INCREASE_STEPS;
#endif
		/* TINFO="MD_EXT_BUCK_ISO_CON[0]=1"*/
		spm_write(MD_EXT_BUCK_ISO_CON,
			spm_read(MD_EXT_BUCK_ISO_CON) | (0x1 << 0));
		/* TINFO="MD_EXT_BUCK_ISO_CON[1]=1"*/
		spm_write(MD_EXT_BUCK_ISO_CON,
			spm_read(MD_EXT_BUCK_ISO_CON) | (0x1 << 1));
		/* TINFO="Set PWR_RST_B = 0" */
		spm_write(MD1_PWR_CON, spm_read(MD1_PWR_CON) & ~PWR_RST_B);
		/* TINFO="Finish to turn off MD1" */
	} else {    /* STA_POWER_ON */
		/* TINFO="Start to turn on MD1" */
		/* TINFO="MD_EXT_BUCK_ISO_CON[0]=0"*/
		spm_write(MD_EXT_BUCK_ISO_CON,
			spm_read(MD_EXT_BUCK_ISO_CON) & ~(0x1 << 0));
		/* TINFO="MD_EXT_BUCK_ISO_CON[1]=0"*/
		spm_write(MD_EXT_BUCK_ISO_CON,
			spm_read(MD_EXT_BUCK_ISO_CON) & ~(0x1 << 1));
		/* TINFO="Set PWR_RST_B = 1" */
		spm_write(MD1_PWR_CON, spm_read(MD1_PWR_CON) | PWR_RST_B);
		/* TINFO="Set PWR_ON = 1" */
		spm_write(MD1_PWR_CON, spm_read(MD1_PWR_CON) | PWR_ON);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until MD1_PWR_STA_MASK = 1" */
		while ((spm_read(PWR_STATUS) & MD1_PWR_STA_MASK)
			!= MD1_PWR_STA_MASK)
			ram_console_update();
		INCREASE_STEPS;
#endif
		/* TINFO="Release bus protect - step2 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_CLR, MD1_PROT_STEP2_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
#endif
		/* TINFO="Release bus protect - step2 : 1" */
		spm_write(INFRA_TOPAXI_PROTECTEN_1_CLR, MD1_PROT_STEP2_1_MASK);
#ifndef IGNORE_MTCMOS_CHECK
#endif
		/* TINFO="Release bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_CLR, MD1_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
#endif
		/* TINFO="Finish to turn on MD1" */
	}
	return err;
}


/* auto-gen end*/

/* enable op*/
/*
 *static int general_sys_enable_op(struct subsys *sys)
 *{
 *	return spm_mtcmos_power_on_general_locked(sys, 1, 0);
 *}
 */
static int MD1_sys_enable_op(struct subsys *sys)
{
	return spm_mtcmos_ctrl_md1(STA_POWER_ON);
}

static int MD1_sys_disable_op(struct subsys *sys)
{
	return spm_mtcmos_ctrl_md1(STA_POWER_DOWN);
}

/*
 * static int sys_get_state_op(struct subsys *sys)
 *{
 *	unsigned int sta = clk_readl(PWR_STATUS);
 *	unsigned int sta_s = clk_readl(PWR_STATUS_2ND);
 *
 *	return (sta & sys->sta_mask) && (sta_s & sys->sta_mask);
 *}
 */

static int sys_get_md1_state_op(struct subsys *sys)
{
	unsigned int sta = clk_readl(MD1_PWR_CON);

	return (sta & sys->sta_mask);
}

static struct subsys_ops MD1_sys_ops = {
	.enable = MD1_sys_enable_op,
	.disable = MD1_sys_disable_op,
	.get_state = sys_get_md1_state_op,
};

static int subsys_is_on(enum subsys_id id)
{
	int r;
	struct subsys *sys = id_to_sys(id);

	WARN_ON(!sys);

	r = sys->ops->get_state(sys);

#if MT_CCF_DEBUG
	pr_debug("[CCF] %s:%d, sys=%s, id=%d\n", __func__, r, sys->name, id);
#endif				/* MT_CCF_DEBUG */

	return r;
}

#if CONTROL_LIMIT
int allow[NR_SYSS] = {
1,	/*SYS_MD1 = 0,*/
1,	/*SYS_CONN = 1,*/
1,	/*SYS_DIS = 2,*/
1,	/*SYS_VEN = 3,*/
1,	/*SYS_VDE = 4,*/
1,	/*SYS_CAM = 5,*/
1,	/*SYS_ISP = 6,*/
1,	/*SYS_AUDIO = 7,*/
1,	/*SYS_MFG0 = 8,*/
1,	/*SYS_MFG1 = 9,*/
1,	/*SYS_MFG2 = 10,*/
1,	/*SYS_MFG3 = 11,*/
0,	/*SYS_MFG4 = 12,*/
1,	/*SYS_VPU_VCORE_DORMANT = 13,*/
1,	/*SYS_VPU_VCORE_SHUTDOWNT = 14,*/
1,	/*SYS_VPU_CONN_DORMANT = 15,*/
1,	/*SYS_VPU_CONN_SHUTDOWNT = 16,*/
1,	/*SYS_VPU_CORE0_DORMANT = 17,*/
1,	/*SYS_VPU_CORE0_SHUTDOWN = 18,*/
1,	/*SYS_VPU_CORE1_DORMANT = 19,*/
1,	/*SYS_VPU_CORE1_SHUTDOWN = 20,*/
1,	/*SYS_VPU_CORE2_DORMANT = 21,*/
1,	/*SYS_VPU_CORE2_SHUTDOWN = 22,*/
1,	/*SYS_IPE = 23,*/
};
#endif
static int enable_subsys(enum subsys_id id)
{
	int r;
	unsigned long flags;
	struct subsys *sys = id_to_sys(id);
	struct pg_callbacks *pgcb;

	WARN_ON(!sys);

#if MT_CCF_BRINGUP
	/*pr_debug("[CCF] %s: sys=%s, id=%d\n", __func__, sys->name, id);*/
	if (sys->ops->get_state(sys) == SUBSYS_PWR_DOWN) {
		switch (id) {
		case SYS_MD1:
			spm_mtcmos_ctrl_md1(STA_POWER_ON);
			break;
/*		case SYS_CONN:
 *			spm_mtcmos_ctrl_conn(STA_POWER_ON);
 *			break;
 */
		default:
			break;
		}
	}
	return 0;
#endif				/* MT_CCF_BRINGUP */

#if CONTROL_LIMIT
	#if MT_CCF_DEBUG
	pr_debug("[CCF] %s: sys=%s, id=%d\n", __func__, sys->name, id);
	#endif
	if (allow[id] == 0) {
		#if MT_CCF_DEBUG
		pr_debug("[CCF] %s: do nothing return\n", __func__);
		#endif
		return 0;
	}
#endif


	mtk_clk_lock(flags);

#if CHECK_PWR_ST
	if (sys->ops->get_state(sys) == SUBSYS_PWR_ON) {
		mtk_clk_unlock(flags);
		return 0;
	}
#endif				/* CHECK_PWR_ST */

	r = sys->ops->enable(sys);
	WARN_ON(r);

	mtk_clk_unlock(flags);

	list_for_each_entry(pgcb, &pgcb_list, list) {
		if (pgcb->after_on)
			pgcb->after_on(id);
	}
	return r;
}

static int disable_subsys(enum subsys_id id)
{
	int r;
	unsigned long flags;
	struct subsys *sys = id_to_sys(id);
	struct pg_callbacks *pgcb;

	WARN_ON(!sys);

#if MT_CCF_BRINGUP
	/*pr_debug("[CCF] %s: sys=%s, id=%d\n", __func__, sys->name, id);*/
	if (sys->ops->get_state(sys) == SUBSYS_PWR_ON) {
		switch (id) {
		case SYS_MD1:
			spm_mtcmos_ctrl_md1(STA_POWER_DOWN);
			break;
/*		case SYS_CONN:
 *			spm_mtcmos_ctrl_conn(STA_POWER_DOWN);
 *			break;
 */
		default:
			break;
		}
	}
	return 0;
#endif				/* MT_CCF_BRINGUP */
#if CONTROL_LIMIT
	#if MT_CCF_DEBUG
	pr_debug("[CCF] %s: sys=%s, id=%d\n", __func__, sys->name, id);
	#endif
	if (allow[id] == 0) {
		#if MT_CCF_DEBUG
		pr_debug("[CCF] %s: do nothing return\n", __func__);
		#endif
		return 0;
	}
#endif



	/* TODO: check all clocks related to this subsys are off */
	/* could be power off or not */
	list_for_each_entry_reverse(pgcb, &pgcb_list, list) {
		if (pgcb->before_off)
			pgcb->before_off(id);
	}

	mtk_clk_lock(flags);

#if CHECK_PWR_ST
	if (sys->ops->get_state(sys) == SUBSYS_PWR_DOWN) {
		mtk_clk_unlock(flags);
		return 0;
	}
#endif				/* CHECK_PWR_ST */

	r = sys->ops->disable(sys);
	WARN_ON(r);

	mtk_clk_unlock(flags);

	return r;
}

/*
 * power_gate
 */

struct mt_power_gate {
	struct clk_hw hw;
	struct clk *pre_clk;
	enum subsys_id pd_id;
};

#define to_power_gate(_hw) container_of(_hw, struct mt_power_gate, hw)

static int pg_enable(struct clk_hw *hw)
{
	struct mt_power_gate *pg = to_power_gate(hw);

#if MT_CCF_DEBUG
	pr_debug("[CCF] %s: sys=%s, pd_id=%u\n", __func__,
		 __clk_get_name(hw->clk), pg->pd_id);
#endif				/* MT_CCF_DEBUG */

	return enable_subsys(pg->pd_id);
}

static void pg_disable(struct clk_hw *hw)
{
	struct mt_power_gate *pg = to_power_gate(hw);

#if MT_CCF_DEBUG
	pr_debug("[CCF] %s: sys=%s, pd_id=%u\n", __func__,
		 __clk_get_name(hw->clk), pg->pd_id);
#endif				/* MT_CCF_DEBUG */

	disable_subsys(pg->pd_id);
}

static int pg_is_enabled(struct clk_hw *hw)
{
	struct mt_power_gate *pg = to_power_gate(hw);

	return subsys_is_on(pg->pd_id);
}

int pg_prepare(struct clk_hw *hw)
{
	int r;
	struct mt_power_gate *pg = to_power_gate(hw);

#if MT_CCF_DEBUG
	pr_debug("[CCF] %s: sys=%s, pre_sys=%s\n", __func__,
		 __clk_get_name(hw->clk),
		 pg->pre_clk ? __clk_get_name(pg->pre_clk) : "");
#endif				/* MT_CCF_DEBUG */

	if (pg->pre_clk) {
		r = clk_prepare_enable(pg->pre_clk);
		if (r)
			return r;
	}

	return pg_enable(hw);

}

void pg_unprepare(struct clk_hw *hw)
{
	struct mt_power_gate *pg = to_power_gate(hw);

#if MT_CCF_DEBUG
	pr_debug("[CCF] %s: clk=%s, pre_clk=%s\n", __func__,
		 __clk_get_name(hw->clk),
		 pg->pre_clk ? __clk_get_name(pg->pre_clk) : "");
#endif				/* MT_CCF_DEBUG */

	pg_disable(hw);

	if (pg->pre_clk)
		clk_disable_unprepare(pg->pre_clk);
}

static const struct clk_ops mt_power_gate_ops = {
	.prepare = pg_prepare,
	.unprepare = pg_unprepare,
	.is_enabled = pg_is_enabled,
};

struct clk *mt_clk_register_power_gate(const char *name,
					const char *parent_name,
					struct clk *pre_clk,
					enum subsys_id pd_id)
{
	struct mt_power_gate *pg;
	struct clk *clk;
	struct clk_init_data init;

	pg = kzalloc(sizeof(*pg), GFP_KERNEL);
	if (!pg)
		return ERR_PTR(-ENOMEM);

	init.name = name;
	init.flags = CLK_IGNORE_UNUSED;
	init.parent_names = parent_name ? &parent_name : NULL;
	init.num_parents = parent_name ? 1 : 0;
	init.ops = &mt_power_gate_ops;

	pg->pre_clk = pre_clk;
	pg->pd_id = pd_id;
	pg->hw.init = &init;

	clk = clk_register(NULL, &pg->hw);
	if (IS_ERR(clk))
		kfree(pg);

	return clk;
}

#define pg_md1	"pg_md1"

/* FIXME: set correct value: E */

struct mtk_power_gate {
	int id;
	const char *name;
	const char *parent_name;
	const char *pre_clk_name;
	enum subsys_id pd_id;
};

#define PGATE(_id, _name, _parent, _pre_clk, _pd_id) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.pre_clk_name = _pre_clk,		\
		.pd_id = _pd_id,			\
	}

/* FIXME: all values needed to be verified */
struct mtk_power_gate scp_clks[] __initdata = {
	PGATE(SCP_SYS_MD1, pg_md1, NULL, NULL, SYS_MD1),
};

static void __init init_clk_scpsys(void __iomem *infracfg_reg,
				   void __iomem *spm_reg,
				   void __iomem *infra_reg,
				   void __iomem *smi_common_reg,
				   struct clk_onecell_data *clk_data)
{
	int i;
	struct clk *clk;
	struct clk *pre_clk;

	infracfg_base = infracfg_reg;
	spm_base = spm_reg;
	infra_base = infra_reg;
	smi_common_base = smi_common_reg;

#if 0
	syss[SYS_MD1].ctl_addr = MD1_PWR_CON;
	syss[SYS_CONN].ctl_addr = CONN_PWR_CON;
	syss[SYS_DIS].ctl_addr = DIS_PWR_CON;
	syss[SYS_MFG].ctl_addr = MFG_PWR_CON;
	syss[SYS_ISP].ctl_addr = ISP_PWR_CON;
	syss[SYS_VEN].ctl_addr = VEN_PWR_CON;
	syss[SYS_MFG_ASYNC].ctl_addr = MFG_ASYNC_PWR_CON;
	syss[SYS_AUDIO].ctl_addr = AUDIO_PWR_CON;
	syss[SYS_CAM].ctl_addr = CAM_PWR_CON;
	syss[SYS_MFG_CORE1].ctl_addr = MFG_CORE1_PWR_CON;
	syss[SYS_MFG_CORE0].ctl_addr = MFG_CORE0_PWR_CON;
#endif

	for (i = 0; i < ARRAY_SIZE(scp_clks); i++) {
		struct mtk_power_gate *pg = &scp_clks[i];

		pre_clk = pg->pre_clk_name ?
			__clk_lookup(pg->pre_clk_name) : NULL;

		clk = mt_clk_register_power_gate(pg->name, pg->parent_name,
			pre_clk, pg->pd_id);

		if (IS_ERR(clk)) {
			pr_notice("[CCF] %s: Failed to register clk %s: %ld\n",
			       __func__, pg->name, PTR_ERR(clk));
			continue;
		}

		if (clk_data)
			clk_data->clks[pg->id] = clk;

#if MT_CCF_DEBUG
		pr_debug("[CCF] %s: pgate %3d: %s\n", __func__, i, pg->name);
#endif				/* MT_CCF_DEBUG */
	}
}

/*
 * device tree support
 */

/* TODO: remove this function */
static struct clk_onecell_data *alloc_clk_data(unsigned int clk_num)
{
	int i;
	struct clk_onecell_data *clk_data;

	clk_data = kzalloc(sizeof(*clk_data), GFP_KERNEL);
	if (!clk_data)
		return NULL;

	clk_data->clks = kcalloc(clk_num, sizeof(struct clk *), GFP_KERNEL);
	if (!clk_data->clks) {
		kfree(clk_data);
		return NULL;
	}

	clk_data->clk_num = clk_num;

	for (i = 0; i < clk_num; ++i)
		clk_data->clks[i] = ERR_PTR(-ENOENT);

	return clk_data;
}

/* TODO: remove this function */
static void __iomem *get_reg(struct device_node *np, int index)
{
	return of_iomap(np, index);
}

#if 1
#ifdef CONFIG_OF
void iomap_mm(void)
{
	struct device_node *node;

/*mmsys_config*/
	node = of_find_compatible_node(NULL, NULL, "mediatek,mmsys_config");
	if (!node)
		pr_debug("[CLK_MMSYS] find node failed\n");
	clk_mmsys_config_base = of_iomap(node, 0);
	if (!clk_mmsys_config_base)
		pr_debug("[CLK_MMSYS] base failed\n");
/*imgsys*/
	node = of_find_compatible_node(NULL, NULL, "mediatek,imgsys");
	if (!node)
		pr_debug("[CLK_IMGSYS_CONFIG] find node failed\n");
	clk_imgsys_base = of_iomap(node, 0);
	if (!clk_imgsys_base)
		pr_debug("[CLK_IMGSYS_CONFIG] base failed\n");
/*ipesys*/
	node = of_find_compatible_node(NULL, NULL, "mediatek,ipesys_config");
	if (!node)
		pr_debug("[CLK_IPESYS_CONFIG] find node failed\n");
	clk_ipesys_base = of_iomap(node, 0);
	if (!clk_ipesys_base)
		pr_debug("[CLK_IPESYS_CONFIG] base failed\n");
/*vdec_gcon*/
	node = of_find_compatible_node(NULL, NULL, "mediatek,vdec_gcon");
	if (!node)
		pr_debug("[CLK_VDEC_GCON] find node failed\n");
	clk_vdec_gcon_base = of_iomap(node, 0);
	if (!clk_vdec_gcon_base)
		pr_debug("[CLK_VDEC_GCON] base failed\n");
/*venc_gcon*/
	node = of_find_compatible_node(NULL, NULL, "mediatek,venc_gcon");
	if (!node)
		pr_debug("[CLK_VENC_GCON] find node failed\n");
	clk_venc_gcon_base = of_iomap(node, 0);
	if (!clk_venc_gcon_base)
		pr_debug("[CLK_VENC_GCON] base failed\n");

/*cam*/
	node = of_find_compatible_node(NULL, NULL, "mediatek,camsys");
	if (!node)
		pr_debug("[CLK_CAM] find node failed\n");
	clk_camsys_base = of_iomap(node, 0);
	if (!clk_camsys_base)
		pr_debug("[CLK_CAM] base failed\n");
/*apu vcore*/
	node = of_find_compatible_node(NULL, NULL, "mediatek,apu_vcore");
	if (!node)
		pr_debug("[CLK_APU_VCORE] find node failed\n");
	clk_apu_vcore_base = of_iomap(node, 0);
	if (!clk_apu_vcore_base)
		pr_debug("[CLK_APU_VCORE] base failed\n");
/*apu conn*/
	node = of_find_compatible_node(NULL, NULL, "mediatek,apu_conn");
	if (!node)
		pr_debug("[CLK_APU_CONN] find node failed\n");
	clk_apu_conn_base = of_iomap(node, 0);
	if (!clk_apu_conn_base)
		pr_debug("[CLK_APU_VCORE] base failed\n");
}
#endif
#endif

static void __init mt_scpsys_init(struct device_node *node)
{
	struct clk_onecell_data *clk_data;
	void __iomem *infracfg_reg;
	void __iomem *spm_reg;
	void __iomem *infra_reg;
	void __iomem *ckgen_reg;
	void __iomem *smi_common_reg;
	int r;

	infracfg_reg = get_reg(node, 0);
	spm_reg = get_reg(node, 1);
	infra_reg = get_reg(node, 2);
	ckgen_reg = get_reg(node, 3);
	smi_common_reg = get_reg(node, 4);



	if (!infracfg_reg || !spm_reg || !infra_reg  ||
		!ckgen_reg || !smi_common_reg) {
		pr_notice("clk-pg-mt6885: missing reg\n");
		return;
	}

	clk_data = alloc_clk_data(SCP_NR_SYSS);

	init_clk_scpsys(infracfg_reg, spm_reg, infra_reg,
		smi_common_reg, clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);
	if (r)
		pr_notice("[CCF] %s:could not register clock provide\n",
			__func__);

	ckgen_base = ckgen_reg;
	/*MM Bus*/
	iomap_mm();
#if 0/*!MT_CCF_BRINGUP*/
	/* subsys init: per modem owner request, disable modem power first */
	disable_subsys(SYS_MD1);
#else				/*power on all subsys for bring up */
#ifndef CONFIG_FPGA_EARLY_PORTING
/*
 *	spm_mtcmos_ctrl_mfg0(STA_POWER_ON);
 *	spm_mtcmos_ctrl_mfg1(STA_POWER_ON);
 *	spm_mtcmos_ctrl_mfg2(STA_POWER_ON);
 *	spm_mtcmos_ctrl_mfg3(STA_POWER_ON);
 *#if 0
 *	spm_mtcmos_ctrl_dis(STA_POWER_ON);
 *	spm_mtcmos_ctrl_cam(STA_POWER_ON);
 *	spm_mtcmos_ctrl_ven(STA_POWER_ON);
 *	spm_mtcmos_ctrl_vde(STA_POWER_ON);
 *	spm_mtcmos_ctrl_isp(STA_POWER_ON);
 *	spm_mtcmos_ctrl_ipe(STA_POWER_ON);
 *#endif
 */
#if 0 /*avoid hang in bring up*/
	spm_mtcmos_ctrl_vpu_vcore_shut_down(STA_POWER_ON);
	spm_mtcmos_ctrl_vpu_conn_shut_down(STA_POWER_ON);
	spm_mtcmos_ctrl_vpu_core0_shut_down(STA_POWER_ON);
	spm_mtcmos_ctrl_vpu_core1_shut_down(STA_POWER_ON);
	spm_mtcmos_ctrl_vpu_core2_shut_down(STA_POWER_ON);
#endif
	/*spm_mtcmos_ctrl_md1(STA_POWER_ON);*/
	/*spm_mtcmos_ctrl_md1(STA_POWER_DOWN);*/
	/*spm_mtcmos_ctrl_audio(STA_POWER_ON);*/
#endif
#endif				/* !MT_CCF_BRINGUP */
}

CLK_OF_DECLARE_DRIVER(mtk_pg_regs, "mediatek,scpsys", mt_scpsys_init);

#if 0
static const char * const *get_all_clk_names(size_t *num)
{
	static const char * const clks[] = {

		/* CAM */
		"camsys_larb6",
		"camsys_dfp_vad",
		"camsys_larb3",
		"camsys_cam",
		"camsys_camtg",
		"camsys_seninf",
		"camsys_camsv0",
		"camsys_camsv1",
		"camsys_camsv2",
		"camsys_ccu",
		/* IMG */
		"imgsys_larb5",
		"imgsys_larb2",
		"imgsys_dip",
		"imgsys_fdvt",
		"imgsys_dpe",
		"imgsys_rsc",
		"imgsys_mfb",
		"imgsys_wpe_a",
		"imgsys_wpe_b",
		"imgsys_owe",
		/* MM */
		"mm_smi_common",
		"mm_smi_larb0",
		"mm_smi_larb1",
		"mm_gals_comm0",
		"mm_gals_comm1",
		"mm_gals_ccu2mm",
		"mm_gals_ipu12mm",
		"mm_gals_img2mm",
		"mm_gals_cam2mm",
		"mm_gals_ipu2mm",
		"mm_mdp_dl_txck",
		"mm_ipu_dl_txck",
		"mm_mdp_rdma0",
		"mm_mdp_rdma1",
		"mm_mdp_rsz0",
		"mm_mdp_rsz1",
		"mm_mdp_tdshp",
		"mm_mdp_wrot0",
		"mm_mdp_wdma0",
		"mm_fake_eng",
		"mm_disp_ovl0",
		"mm_disp_ovl0_2l",
		"mm_disp_ovl1_2l",
		"mm_disp_rdma0",
		"mm_disp_rdma1",
		"mm_disp_wdma0",
		"mm_disp_color0",
		"mm_disp_ccorr0",
		"mm_disp_aal0",
		"mm_disp_gamma0",
		"mm_disp_dither0",
		"mm_disp_split",
		"mm_dsi0_mmck",
		"mm_dsi0_ifck",
		"mm_dpi_mmck",
		"mm_dpi_ifck",
		"mm_fake_eng2",
		"mm_mdp_dl_rxck",
		"mm_ipu_dl_rxck",
		"mm_26m",
		"mm_mmsys_r2y",
		"mm_disp_rsz",
		"mm_mdp_aal",
		"mm_mdp_hdr",
		"mm_dbi_mmck",
		"mm_dbi_ifck",
		/* VENC */
		"venc_larb",
		"venc_venc",
		"venc_jpgenc",
		/* VDE */
		"vdec_cken",
		"vdec_larb1_cken",
	};
	*num = ARRAY_SIZE(clks);
	return clks;
}
#endif

/*
 *static void dump_cg_state(const char *clkname)
 *{
 *	struct clk *c = __clk_lookup(clkname);
 *
 *	if (IS_ERR_OR_NULL(c)) {
 *		pr_notice("[%17s: NULL]\n", clkname);
 *		return;
 *	}
 *
 *	pr_notice("[%-17s: %3d]\n",
 *		__clk_get_name(c),
 *		__clk_get_enable_count(c));
 *}
 */

void subsys_if_on(void)
{
	/* unsigned int sta = spm_read(PWR_STATUS); */
	/* unsigned int sta_s = spm_read(PWR_STATUS_2ND); */
	unsigned int sta_md1 = spm_read(MD1_PWR_CON);
	int ret = 0;
	/* size_t cam_num, img_num, ipe_num, mm_num, venc_num, vdec_num = 0; */
	/* size_t num, cam_num, img_num, mm_num, venc_num, vdec_num = 0;*/

	/* const char * const *clks = get_all_clk_names(&num);*/
	/* const char * const *cam_clks = get_cam_clk_names(&cam_num); */

	if (sta_md1 & PWR_ON)
		pr_notice("suspend warning: SYS_MD1 is on!!!\n");

	if (ret > 0)
		WARN_ON(1); /* BUG_ON(1); */

#if 0
	for (i = 0; i < num; i++)
		dump_cg_state(clks[i]);
#endif
}

#if 1 /*only use for suspend test*/
void mtcmos_force_off(void)
{
#if 0
	spm_mtcmos_ctrl_mfg3(STA_POWER_DOWN);
	spm_mtcmos_ctrl_mfg2(STA_POWER_DOWN);
	spm_mtcmos_ctrl_mfg1(STA_POWER_DOWN);
	spm_mtcmos_ctrl_mfg0(STA_POWER_DOWN);

	spm_mtcmos_ctrl_vpu_core2_shut_down(STA_POWER_DOWN);
	spm_mtcmos_ctrl_vpu_core1_shut_down(STA_POWER_DOWN);
	spm_mtcmos_ctrl_vpu_core0_shut_down(STA_POWER_DOWN);
	spm_mtcmos_ctrl_vpu_conn_shut_down(STA_POWER_DOWN);
	spm_mtcmos_ctrl_vpu_vcore_shut_down(STA_POWER_DOWN);

	spm_mtcmos_ctrl_cam(STA_POWER_DOWN);
	spm_mtcmos_ctrl_ven(STA_POWER_DOWN);
	spm_mtcmos_ctrl_vde(STA_POWER_DOWN);
	spm_mtcmos_ctrl_ipe(STA_POWER_DOWN);
	spm_mtcmos_ctrl_isp(STA_POWER_DOWN);
	spm_mtcmos_ctrl_dis(STA_POWER_DOWN);
	spm_mtcmos_ctrl_audio(STA_POWER_DOWN);
#endif
	/* spm_mtcmos_ctrl_conn(STA_POWER_DOWN); */
	spm_mtcmos_ctrl_md1(STA_POWER_DOWN);
}
#endif
