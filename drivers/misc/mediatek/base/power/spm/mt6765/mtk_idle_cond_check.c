// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2017 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>

#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/io.h>

#include <trace/events/mtk_idle_event.h>

#include <mtk_idle.h> /* IDLE_TYPE_xxx */
#include <mtk_idle_internal.h>

#include "mtk_spm_internal.h"

/***********************************************************
 * Local definitions
 ***********************************************************/

static void __iomem *infrasys_base;    /* INFRA_REG, INFRA_SW_CG_x_STA */
static void __iomem *mmsys_base;       /* MM_REG, DISP_CG_CON_x */
static void __iomem *imgsys_base;      /* IMGSYS_REG, IMG_CG_CON */
static void __iomem *mfgsys_base;      /* MFGSYS_REG, MFG_CG_CON */
static void __iomem *vencsys_base;     /* VENCSYS_REG, VENCSYS_CG_CON */
static void __iomem *sleepsys_base;    /* SPM_REG */
static void __iomem *topck_base;       /* TOPCK_REG */
static void __iomem *apmixedsys_base;  /* APMIXEDSYS */

#define idle_readl(addr)    __raw_readl(addr)

#define INFRA_REG(ofs)      (infrasys_base + ofs)
#define MM_REG(ofs)         (mmsys_base + ofs)
#define IMGSYS_REG(ofs)     (imgsys_base + ofs)
#define MFGSYS_REG(ofs)     (mfgsys_base + ofs)
#define VENCSYS_REG(ofs)    (vencsys_base + ofs)
#define SPM_REG(ofs)        (sleepsys_base + ofs)
#define TOPCK_REG(ofs)      (topck_base + ofs)
#define APMIXEDSYS(ofs)     (apmixedsys_base + ofs)

#undef SPM_PWR_STATUS
#define SPM_PWR_STATUS      SPM_REG(0x0180)
#define	INFRA_SW_CG_0_STA   INFRA_REG(0x0094)
#define	INFRA_SW_CG_1_STA   INFRA_REG(0x0090)
#define	INFRA_SW_CG_2_STA   INFRA_REG(0x00AC)
#define DISP_CG_CON_0       MM_REG(0x100)
#define DISP_CG_CON_1       MM_REG(0x110)

/* SPM_PWR_STATUS bit definition */
#define PWRSTA_BIT_MD       (1U << 0)
#define PWRSTA_BIT_CONN     (1U << 1)
#define PWRSTA_BIT_DISP     (1U << 3)
#define PWRSTA_BIT_MFG      (1U << 4)
#define PWRSTA_BIT_INFRA    (1U << 6)
#define PWRSTA_BIT_ALL		(0xffffffff)

/***********************************************************
 * Functions for external modules
 ***********************************************************/

/***********************************************************
 * Check clkmux registers
 ***********************************************************/
#define CLK_CFG(id) TOPCK_REG(0x40+id*0x10)

enum {
	/* CLK_CFG_0 */
	CK_AXI = 0,
	CK_MEM,
	CK_MM,
	CK_SCP,
	/* CLK_CFG_1 */
	CK_MFG,
	CK_ATB,
	CK_CAMTG,
	CK_CAMTG1,
	/* CLK_CFG_2 */
	CK_CAMTG2,
	CK_CAMTG3,
	CK_UART,
	CK_SPI,
	/* CLK_CFG_3 */
	CK_MSDC50_0_HCLK,
	CK_MSDC50_0,
	CK_MSDC30_1,
	CK_AUDIO,
	/* CLK_CFG_4 */
	CK_AUD_INTBUS,
	CK_AUD_1,
	CK_AUD_ENGINE1,
	CK_DISP_PWM,
	/* CLK_CFG_5 */
	CK_SSPM,
	CK_DXCC,
	CK_USB_TOP,
	CK_SPM,
	/* CLK_CFG_6 */
	CK_I2C,
	CK_PWM,
	CK_SENINF,
	CK_FAES_UFSDE,
	/* CLK_CFG_7 */
	CK_FPWRAP_ULPOSC,
	CK_CAMTM,
};

#define CLK_CHECK	(1 << 31)

static bool check_clkmux_pdn(unsigned int clkmux_id)
{
	unsigned int reg, val, idx;

	if (clkmux_id & CLK_CHECK) {
		clkmux_id = (clkmux_id & ~CLK_CHECK);
		reg = clkmux_id / 4;
		val = idle_readl(CLK_CFG(reg));
		idx = clkmux_id % 4;
		val = (val >> (idx * 8)) & 0x80;
		return val ? true : false;
	}

	return false;
}

/***********************************************************
 * Check cg idle condition for dp/sodi/sodi3
 ***********************************************************/
/* Local definitions */
struct idle_cond_info {
	/* check SPM_PWR_STATUS for bit definition */
	unsigned int    subsys_mask;
	/* cg name */
	const char      *name;
	/* cg address */
	void __iomem    *addr;
	/* bitflip value from *addr ? */
	bool            bBitflip;
	/* check clkmux if bit 31 = 1, id is bit[30:0] */
	unsigned int    clkmux_id;
};

/* NOTE: null address will be updated in mtk_idle_cond_check_init() */
static struct idle_cond_info idle_cg_info[] = {
	{ 0xffffffff, "MTCMOS", NULL, false, 0 },
	{ 0x00000040, "INFRA0", NULL, true,  0 },
	{ 0x00000040, "INFRA1", NULL, true,  0 },
	{ 0x00000040, "INFRA2", NULL, true,  0 },
	{ 0x00000008, "MMSYS0", NULL, true,  (CK_MM | CLK_CHECK) },
	{ 0x00000008, "MMSYS1", NULL, true,  (CK_MM | CLK_CHECK) },
};

#define NR_CG_GRPS \
	(sizeof(idle_cg_info)/sizeof(struct idle_cond_info))

static unsigned int idle_cond_mask[NR_IDLE_TYPES][NR_CG_GRPS] = {
	[IDLE_TYPE_DP] = {
		0x04000038, /* MTCMOS, 26:VCODEC,5:ISP,4:MFG,3:DIS */
		0x08040802,	/* INFRA0, 27:dxcc_sec_core_cg_sta */
		0x00BFF800,	/* INFRA1, 8:icusb_cg_sta (removed) */
		0x060406C5,	/* INFRA2 */
		0x3FFFFFFF,	/* MMSYS0 */
		0x00000000,	/* MMSYS1 */
	},
	[IDLE_TYPE_SO3] = {
		0x04000030, /* MTCMOS, 26:VCODEC,5:ISP,4:MFG */
		0x0A040802,	/* INFRA0, 27:dxcc_sec_core_cg_sta */
		0x00BFF800,	/* INFRA1, 8:icusb_cg_sta (removed) */
		0x060406D1,	/* INFRA2 */
		0x3FFFFFFF,	/* MMSYS0 */
		0x00000000,	/* MMSYS1 */
	},
	[IDLE_TYPE_SO] = {
		0x04000030, /* MTCMOS, 26:VCODEC,5:ISP,4:MFG */
		0x08040802,	/* INFRA0, 27:dxcc_sec_core_cg_sta */
		0x00BFF800,	/* INFRA1, 8:icusb_cg_sta (removed) */
		0x060406C1,	/* INFRA2 */
		0x0F84005F,	/* MMSYS0 */
		0x00000000,	/* MMSYS1 */
	},
	[IDLE_TYPE_RG] = {
		0, 0, 0, 0, 0, 0},
};

static unsigned int idle_block_mask[NR_IDLE_TYPES][NR_CG_GRPS+1];
static unsigned int idle_value[NR_CG_GRPS];

/***********************************************************
 * Check pll idle condition
 ***********************************************************/

#define PLL_MFGPLL  APMIXEDSYS(0x24C)
#define PLL_MMPLL   APMIXEDSYS(0x25C)
#define PLL_UNIVPLL APMIXEDSYS(0x26C)
#define PLL_MSDCPLL APMIXEDSYS(0x27C)

#define PLL_BIT_MFGPLL  (1 << 0)
#define PLL_BIT_MMPLL   (1 << 1)
#define PLL_BIT_UNIVPLL (1 << 2)
#define PLL_BIT_MSDCPLL (1 << 3)

static unsigned int idle_pll_cond_mask[NR_IDLE_TYPES] = {
	[IDLE_TYPE_DP] = 0,
	[IDLE_TYPE_SO3] = PLL_BIT_UNIVPLL | PLL_BIT_MSDCPLL,
	[IDLE_TYPE_SO] = 0,
	};
static unsigned int idle_pll_block_mask[NR_IDLE_TYPES];
static unsigned int idle_pll_value;

static void update_pll_state(void)
{
	idle_pll_value = 0;
	if (idle_readl(PLL_MFGPLL) & 0x1)
		idle_pll_value |= PLL_BIT_MFGPLL;
	if (idle_readl(PLL_MMPLL) & 0x1)
		idle_pll_value |= PLL_BIT_MMPLL;
	if (idle_readl(PLL_UNIVPLL) & 0x1)
		idle_pll_value |= PLL_BIT_UNIVPLL;
	if (idle_readl(PLL_MSDCPLL) & 0x1)
		idle_pll_value |= PLL_BIT_MSDCPLL;

	idle_pll_block_mask[IDLE_TYPE_DP] =
		idle_pll_value & idle_pll_cond_mask[IDLE_TYPE_DP];
	idle_pll_block_mask[IDLE_TYPE_SO3] =
		idle_pll_value & idle_pll_cond_mask[IDLE_TYPE_SO3];
	idle_pll_block_mask[IDLE_TYPE_SO] =
		idle_pll_value & idle_pll_cond_mask[IDLE_TYPE_SO];
}

/* dp/so3/so print blocking cond mask in debugfs */
int mtk_idle_cond_append_info(
	bool short_log, int idle_type, char *logptr, unsigned int logsize)
{
	int i;
	char *p = logptr;
	unsigned int s = logsize;

	#undef log
	#define log(fmt, args...) \
	do { \
		int l = scnprintf(p, s, fmt, ##args); \
		p += l; \
		s -= l; \
	} while (0)

	if (unlikely(idle_type < 0 || idle_type >= NR_IDLE_TYPES))
		return 0;

	if (short_log) {
		for (i = 0; i < NR_CG_GRPS; i++)
			log("0x%08x, ", idle_block_mask[idle_type][i]);
		log("idle_pll_block_mask: 0x%08x\n"
			, idle_pll_block_mask[idle_type]);
	} else {
		for (i = 0; i < NR_CG_GRPS; i++) {
			log("[%02d %s] value/cond/block = 0x%08x "
				, i, idle_cg_info[i].name, idle_value[i]);

			log("0x%08x 0x%08x\n", idle_cond_mask[idle_type][i]
				, idle_block_mask[idle_type][i]);
		}
		log("[%02d PLLCHK] value/cond/block = 0x%08x "
			, i, idle_pll_value);
		log("0x%08x 0x%08x\n", idle_pll_cond_mask[idle_type]
			, idle_pll_block_mask[idle_type]);
	}

	return p - logptr;
}

/* dp/so3/so may update idle_cond_mask by debugfs */
void mtk_idle_cond_update_mask(
	int idle_type, unsigned int reg, unsigned int mask)
{
	if (unlikely(idle_type < 0 || idle_type >= NR_IDLE_TYPES))
		return;

	if (reg < NR_CG_GRPS)
		idle_cond_mask[idle_type][reg] = mask;
	/* special case for sodi3 pll check */
	if (reg == NR_CG_GRPS)
		idle_pll_cond_mask[idle_type] = mask;
}

static int cgmon_sel = -1;
static unsigned int cgmon_sta[NR_CG_GRPS + 1];
static DEFINE_SPINLOCK(cgmon_spin_lock);

/* dp/so3/so print cg change state to ftrace log */
void mtk_idle_cg_monitor(int sel)
{
	unsigned long flags;

	spin_lock_irqsave(&cgmon_spin_lock, flags);
	cgmon_sel = sel;
	memset(cgmon_sta, 0, sizeof(cgmon_sta));
	spin_unlock_irqrestore(&cgmon_spin_lock, flags);
}


#define TRACE_CGMON(_g, _n, _cond)\
	trace_idle_cg(_g * 32 + _n, ((1 << _n) & _cond) ? 1 : 0)

static void mtk_idle_cgmon_trace_log(void)
{
	/* Note: trace tag is defined at trace/events/mtk_idle_event.h */
	#if MTK_IDLE_TRACE_TAG_ENABLE
	unsigned int diff, block, g, n;

	if (cgmon_sel == IDLE_TYPE_DP ||
		cgmon_sel == IDLE_TYPE_SO3 ||
		cgmon_sel == IDLE_TYPE_SO) {

		for (g = 0; g < NR_CG_GRPS + 1; g++) {
			block = (g < NR_CG_GRPS) ?
				idle_block_mask[cgmon_sel][g] :
				idle_pll_block_mask[cgmon_sel];
			diff = cgmon_sta[g] ^ block;
			if (diff) {
				cgmon_sta[g] = block;
				for (n = 0; n < 32; n++)
					if (diff & (1U << n))
						TRACE_CGMON(g, n, cgmon_sta[g]);
			}
		}
	}
	#endif
}

/* update secure cg state by secure call */
static void update_secure_cg_state(unsigned int clk[NR_CG_GRPS])
{
	/* Update INFRA0 bit 27 */
	#define INFRA0_BIT27	(1 << 27)

	clk[1] = clk[1] & ~INFRA0_BIT27;

	if (mt_secure_call(MTK_SIP_KERNEL_CHECK_SECURE_CG, 0, 0, 0, 0))
		clk[1] |= INFRA0_BIT27;
}

/* update all idle condition state: mtcmos/pll/cg/secure_cg */
void mtk_idle_cond_update_state(void)
{
	int i, j;
	unsigned int clk[NR_CG_GRPS];

	/* read all cg state (not including secure cg) */
	for (i = 0; i < NR_CG_GRPS; i++) {
		idle_value[i] = clk[i] = 0;

		/* check mtcmos, if off set idle_value and clk to 0 disable */
		if (!(idle_readl(SPM_PWR_STATUS) & idle_cg_info[i].subsys_mask))
			continue;
		/* check clkmux */
		if (check_clkmux_pdn(idle_cg_info[i].clkmux_id))
			continue;
		idle_value[i] = clk[i] = idle_cg_info[i].bBitflip ?
			~idle_readl(idle_cg_info[i].addr) :
			idle_readl(idle_cg_info[i].addr);
	}

	/* update secure cg state */
	update_secure_cg_state(clk);

	/* update pll state */
	update_pll_state();

	/* update block mask for dp/so/so3 */
	for (i = 0; i < NR_IDLE_TYPES; i++) {
		if (i == IDLE_TYPE_RG)
			continue;
		idle_block_mask[i][NR_CG_GRPS] = 0;
		for (j = 0; j < NR_CG_GRPS; j++) {
			idle_block_mask[i][j] = idle_cond_mask[i][j] & clk[j];
			idle_block_mask[i][NR_CG_GRPS] |= idle_block_mask[i][j];
		}
	}

	/* cg monitor: print cg change info to ftrace log */
	mtk_idle_cgmon_trace_log();
}

/* return final idle condition check result for each idle type */
bool mtk_idle_cond_check(int idle_type)
{
	bool ret = false;

	if (unlikely(idle_type < 0 || idle_type >= NR_IDLE_TYPES))
		return false;

	/* check cg state */
	ret = !(idle_block_mask[idle_type][NR_CG_GRPS]);

	/* check pll state */
	ret = (ret && !idle_pll_block_mask[idle_type]);

	return ret;
}

/***********************************************************
 * Clock mux check for vcore low power mode
 ***********************************************************/
bool mtk_idle_cond_vcore_lp_mode(int idle_type)
{
	return true;
}

/***********************************************************
 * Fundamental build up functions
 ***********************************************************/
static void get_cg_addrs(void)
{
	/* Assign cg address to idle_cg_info */
	idle_cg_info[0].addr = SPM_PWR_STATUS;
	idle_cg_info[1].addr = INFRA_SW_CG_0_STA;
	idle_cg_info[2].addr = INFRA_SW_CG_1_STA;
	idle_cg_info[3].addr = INFRA_SW_CG_2_STA;
	idle_cg_info[4].addr = DISP_CG_CON_0;
	idle_cg_info[5].addr = DISP_CG_CON_1;
}

static int get_base_from_node(
	const char *cmp, void __iomem **pbase, int idx)
{
	struct device_node *node;

	node = of_find_compatible_node(NULL, NULL, cmp);
	if (!node)
		pr_info("[IDLE] node '%s' not found!\n", cmp);

	*pbase = of_iomap(node, idx);
	if (!(*pbase))
		pr_info("[IDLE] node '%s' cannot iomap!\n", cmp);

	return 0;
}

int __init mtk_idle_cond_check_init(void)
{
	get_base_from_node("mediatek,mt6765-infracfg", &infrasys_base, 0);
	get_base_from_node("mediatek,mt6765-mmsys_config", &mmsys_base, 0);
	get_base_from_node("mediatek,mt6765-imgsys", &imgsys_base, 0);
	get_base_from_node("mediatek,mfgcfg", &mfgsys_base, 0);
	get_base_from_node("mediatek,venc_gcon", &vencsys_base, 0);
	get_base_from_node("mediatek,mt6765-apmixedsys", &apmixedsys_base, 0);
	get_base_from_node("mediatek,sleep", &sleepsys_base, 0);
	get_base_from_node("mediatek,mt6765-topckgen", &topck_base, 0);
	/* update cg address in idle_cg_info */
	get_cg_addrs();

	return 0;
}

late_initcall(mtk_idle_cond_check_init);
