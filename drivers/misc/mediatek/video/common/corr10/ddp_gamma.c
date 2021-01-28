// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#ifdef CONFIG_MTK_CLKMGR
#include <mach/mt_clkmgr.h>
#else
#if defined(CONFIG_MACH_MT6755) || defined(CONFIG_MACH_MT6797) || \
	defined(CONFIG_MACH_MT6757) || defined(CONFIG_MACH_KIBOPLUS) || \
	defined(CONFIG_MACH_ELBRUS) || defined(CONFIG_MACH_MT6799) || \
	defined(CONFIG_MACH_MT6759) || defined(CONFIG_MACH_MT6763) || \
	defined(CONFIG_MACH_MT6758) || defined(CONFIG_MACH_MT6739) || \
	defined(CONFIG_MACH_MT6765) || defined(CONFIG_MACH_MT6761) || \
	defined(CONFIG_MACH_MT3967) || defined(CONFIG_MACH_MT6779)
#include <ddp_clkmgr.h>
#endif
#endif
#include <cmdq_record.h>
#include <ddp_drv.h>
#include <ddp_reg.h>
#include <ddp_path.h>
#include <ddp_gamma.h>
#include <ddp_pq.h>
#include <disp_drv_platform.h>
#if defined(CONFIG_MACH_MT6755) || defined(CONFIG_MACH_MT6797) || \
	defined(CONFIG_MACH_MT6757) || defined(CONFIG_MACH_KIBOPLUS) || \
	defined(CONFIG_MACH_MT6799) || defined(CONFIG_MACH_MT6739) || \
	defined(CONFIG_MACH_MT6765) || defined(CONFIG_MACH_MT6761) || \
	defined(CONFIG_MACH_MT3967) || defined(CONFIG_MACH_MT6779)
#include <disp_helper.h>
#endif
#include <primary_display.h>

/* To enable debug log: */
/* # echo corr_dbg:1 > /sys/kernel/debug/dispsys */
int corr_dbg_en;
int ccorr_scenario;
#define GAMMA_ERR(fmt, arg...) \
	pr_notice("[GAMMA] %s: " fmt "\n", __func__, ##arg)
#define GAMMA_NOTICE(fmt, arg...) \
	do { if (corr_dbg_en) \
		pr_debug("[GAMMA] %s: " fmt "\n", __func__, ##arg); } while (0)
#define GAMMA_DBG(fmt, arg...) \
	do { if (corr_dbg_en) \
		pr_debug("[GAMMA] %s: " fmt "\n", __func__, ##arg); } while (0)
#define CCORR_ERR(fmt, arg...) \
	pr_notice("[CCORR] %s: " fmt "\n", __func__, ##arg)
#define CCORR_NOTICE(fmt, arg...) \
	do { if (corr_dbg_en) \
		pr_debug("[CCORR] %s: " fmt "\n", __func__, ##arg); } while (0)
#define CCORR_DBG(fmt, arg...) \
	do { if (corr_dbg_en) \
		pr_debug("[CCORR] %s: " fmt "\n", __func__, ##arg); } while (0)

static DEFINE_MUTEX(g_gamma_global_lock);


/* ======================================================================== */
/*  GAMMA								    */
/* ======================================================================== */

#if defined(CONFIG_MACH_ELBRUS) || defined(CONFIG_MACH_MT6757) || \
	defined(CONFIG_MACH_KIBOPLUS) || defined(CONFIG_MACH_MT6799) || \
	defined(CONFIG_MACH_MT6763) || defined(CONFIG_MACH_MT6758) || \
	defined(CONFIG_MACH_MT6739) || defined(CONFIG_MACH_MT6765) || \
	defined(CONFIG_MACH_MT6761) || defined(CONFIG_MACH_MT3967) || \
	defined(CONFIG_MACH_MT6779)
#define GAMMA0_MODULE_NAMING (DISP_MODULE_GAMMA0)
#else
#define GAMMA0_MODULE_NAMING (DISP_MODULE_GAMMA)
#endif

#if defined(CONFIG_MACH_MT6799) || defined(CONFIG_MACH_MT6763) || \
	defined(CONFIG_MACH_MT6758) || defined(CONFIG_MACH_MT6739) || \
	defined(CONFIG_MACH_MT6765) || defined(CONFIG_MACH_MT6761) || \
	defined(CONFIG_MACH_MT3967) || defined(CONFIG_MACH_MT6779)
#define GAMMA0_CLK_NAMING (DISP0_DISP_GAMMA0)
#else
#define GAMMA0_CLK_NAMING (DISP0_DISP_GAMMA)
#endif


#if defined(CONFIG_MACH_MT6797) || defined(CONFIG_MACH_MT6757) || \
	defined(CONFIG_MACH_KIBOPLUS) || defined(CONFIG_MACH_MT6799) || \
	defined(CONFIG_MACH_MT6763) || defined(CONFIG_MACH_MT6758) || \
	defined(CONFIG_MACH_MT6739) || defined(CONFIG_MACH_MT6765) || \
	defined(CONFIG_MACH_MT6761) || defined(CONFIG_MACH_MT3967) || \
	defined(CONFIG_MACH_MT6779)
#define GAMMA_SUPPORT_PARTIAL_UPDATE
#endif

#define GAMMA0_OFFSET (0)
#if defined(CONFIG_MACH_MT6799)
#define GAMMA_TOTAL_MODULE_NUM (2)
#define GAMMA1_OFFSET (DISPSYS_GAMMA1_BASE - DISPSYS_GAMMA0_BASE)

#define gamma_get_offset(module) ((module == GAMMA0_MODULE_NAMING) ? \
	GAMMA0_OFFSET : GAMMA1_OFFSET)
#define index_of_gamma(module) ((module == GAMMA0_MODULE_NAMING) ? 0 : 1)
#else
#define GAMMA_TOTAL_MODULE_NUM (1)

#define gamma_get_offset(module) (GAMMA0_OFFSET)
#define index_of_gamma(module) (0)
#endif

static unsigned int g_gamma_relay_value[GAMMA_TOTAL_MODULE_NUM];

static struct DISP_GAMMA_LUT_T *g_disp_gamma_lut[DISP_GAMMA_TOTAL] = { NULL };

static ddp_module_notify g_gamma_ddp_notify;


static int disp_gamma_write_lut_reg(struct cmdqRecStruct *cmdq,
	enum DISP_MODULE_ENUM module,
	enum disp_gamma_id_t id, int lock);

static int disp_gamma_start(enum DISP_MODULE_ENUM module, void *cmdq)
{
	disp_gamma_write_lut_reg(cmdq, module, DISP_GAMMA0, 1);

	return 0;
}

static void disp_gamma_init(enum DISP_MODULE_ENUM module, unsigned int width,
	unsigned int height, void *cmdq)
{
	const int offset = gamma_get_offset(module);

	DISP_REG_SET(cmdq, DISP_REG_GAMMA_SIZE + offset,
		(width << 16) | height);
#if defined(CONFIG_MACH_MT6797) || defined(CONFIG_MACH_MT6757) || \
	defined(CONFIG_MACH_KIBOPLUS)
	/* disable stall cg for avoid display path hang */
	DISP_REG_MASK(cmdq, DISP_REG_GAMMA_CFG + offset, 0x0 << 8, 0x1 << 8);
#endif

#ifdef DISP_PLATFORM_HAS_SHADOW_REG
	if (disp_helper_get_option(DISP_OPT_SHADOW_REGISTER)) {
		if (disp_helper_get_option(DISP_OPT_SHADOW_MODE) == 0) {
			/* full shadow mode*/
			DISP_REG_MASK(cmdq, DISP_REG_GAMMA_DEBUG + offset,
			0x0, 0x7);
		} else if (disp_helper_get_option(DISP_OPT_SHADOW_MODE) == 1) {
			/* force commit */
			DISP_REG_MASK(cmdq, DISP_REG_GAMMA_DEBUG + offset,
			0x1, 0x7);
		} else if (disp_helper_get_option(DISP_OPT_SHADOW_MODE) == 2) {
			/* bypass shadow */
			DISP_REG_MASK(cmdq, DISP_REG_GAMMA_DEBUG + offset,
			0x1 << 1, 0x7);
		}
	}
#endif

#ifdef GAMMA_ROUND_CORNER
	/* disable mask config */
	DISP_REG_SET(cmdq, DISP_REG_GAMMA_DEBUG + offset, 0x0);
#endif

	/* enable with relay bit set for bypass gamma engine */
	DISP_REG_MASK(cmdq, DISP_REG_GAMMA_EN + offset, 0x1, 0x1);
}

static int disp_gamma_config(enum DISP_MODULE_ENUM module,
	struct disp_ddp_path_config *pConfig, void *cmdq)
{
	if (pConfig->dst_dirty)
		disp_gamma_init(module, pConfig->dst_w, pConfig->dst_h, cmdq);
	return 0;
}


static void disp_gamma_trigger_refresh(enum disp_gamma_id_t id)
{
	if (g_gamma_ddp_notify != NULL)
		g_gamma_ddp_notify(GAMMA0_MODULE_NAMING,
		DISP_PATH_EVENT_TRIGGER);
}


static int disp_gamma_write_lut_reg(struct cmdqRecStruct *cmdq,
	enum DISP_MODULE_ENUM module,
	enum disp_gamma_id_t id, int lock)
{
	const int offset = gamma_get_offset(module);
	unsigned long lut_base = 0;
	struct DISP_GAMMA_LUT_T *gamma_lut;
	int i;
	int ret = 0;

	if (module < GAMMA0_MODULE_NAMING ||
		module >= GAMMA0_MODULE_NAMING + GAMMA_TOTAL_MODULE_NUM) {
		GAMMA_ERR("invalid module = %d\n", module);
		return -EFAULT;
	}

	if (lock)
		mutex_lock(&g_gamma_global_lock);

	gamma_lut = g_disp_gamma_lut[id];
	if (gamma_lut == NULL) {
		GAMMA_ERR("table [%d] not initialized\n", id);
		ret = -EFAULT;
		goto gamma_write_lut_unlock;
	}

	DISP_REG_MASK(cmdq, DISP_REG_GAMMA_EN + offset, 0x1, 0x1);

	lut_base = DISP_REG_GAMMA_LUT + offset;

	for (i = 0; i < DISP_GAMMA_LUT_SIZE; i++) {
		DISP_REG_SET(cmdq, (lut_base + i * 4), gamma_lut->lut[i]);

		if ((i & 0x3f) == 0) {
			GAMMA_DBG("[0x%08lx](%d) = 0x%x\n",
				(lut_base + i * 4), i, gamma_lut->lut[i]);
		}
	}
	i--;
	GAMMA_DBG("[0x%08lx](%d) = 0x%x\n", (lut_base + i * 4), i,
	       gamma_lut->lut[i]);
#ifdef GAMMA_LIGHT
	if ((int)(gamma_lut->lut[0] & 0x3FF) -
		(int)(gamma_lut->lut[510] & 0x3FF) > 0) {
		DISP_REG_MASK(cmdq, DISP_REG_GAMMA_CFG + offset, 0x1 << 2, 0x4);
		GAMMA_DBG("decreasing LUT\n");
	} else {
		DISP_REG_MASK(cmdq, DISP_REG_GAMMA_CFG + offset, 0x0 << 2, 0x4);
		GAMMA_DBG("Incremental LUT\n");
	}
#endif
	DISP_REG_MASK(cmdq, DISP_REG_GAMMA_CFG + offset,
		0x2|g_gamma_relay_value[index_of_gamma(module)], 0x3);

gamma_write_lut_unlock:

	if (lock)
		mutex_unlock(&g_gamma_global_lock);

	return ret;
}


static int disp_gamma_set_lut
	(const struct DISP_GAMMA_LUT_T __user *user_gamma_lut,
		enum DISP_MODULE_ENUM module, void *cmdq)
{
	int ret = 0;
	enum disp_gamma_id_t id;
	struct DISP_GAMMA_LUT_T *gamma_lut, *old_lut;

	GAMMA_DBG("(cmdq = %d)", (cmdq != NULL ? 1 : 0));

	gamma_lut = kmalloc(sizeof(struct DISP_GAMMA_LUT_T), GFP_KERNEL);
	if (gamma_lut == NULL) {
		GAMMA_ERR("no memory\n");
		return -EFAULT;
	}

	if (copy_from_user(gamma_lut, user_gamma_lut,
		sizeof(struct DISP_GAMMA_LUT_T)) != 0) {
		ret = -EFAULT;
		kfree(gamma_lut);
	} else {
		id = gamma_lut->hw_id;
		if (id >= 0 && id < DISP_GAMMA_TOTAL) {
			mutex_lock(&g_gamma_global_lock);

			old_lut = g_disp_gamma_lut[id];
			g_disp_gamma_lut[id] = gamma_lut;

			GAMMA_DBG("Set module(%d) lut", module);
			ret = disp_gamma_write_lut_reg(cmdq, module, id, 0);

			mutex_unlock(&g_gamma_global_lock);

			if (old_lut != NULL)
				kfree(old_lut);

			disp_gamma_trigger_refresh(id);
		} else {
			GAMMA_ERR("invalid ID = %d\n", id);
			ret = -EFAULT;
		}
	}

	return ret;
}

#ifdef GAMMA_SUPPORT_PARTIAL_UPDATE
static int _gamma_partial_update(enum DISP_MODULE_ENUM module, void *arg,
void *cmdq)
{
	struct disp_rect *roi = (struct disp_rect *) arg;
	int width = roi->width;
	int height = roi->height;

	DISP_REG_SET(cmdq, DISP_REG_GAMMA_SIZE + gamma_get_offset(module),
		(width << 16) | height);
	return 0;
}

static int gamma_ioctl(enum DISP_MODULE_ENUM module, void *handle,
		enum DDP_IOCTL_NAME ioctl_cmd, void *params)
{
	int ret = -1;

	if (ioctl_cmd == DDP_PARTIAL_UPDATE) {
		_gamma_partial_update(module, params, handle);
		ret = 0;
	}
	return ret;
}
#endif

static int disp_gamma_io(enum DISP_MODULE_ENUM module, unsigned int msg,
	unsigned long arg, void *cmdq)
{
	switch (msg) {
	case DISP_IOCTL_SET_GAMMALUT:
		if (disp_gamma_set_lut((struct DISP_GAMMA_LUT_T *) arg,
			module, cmdq) < 0) {
			GAMMA_ERR("DISP_IOCTL_SET_GAMMALUT: failed\n");
			return -EFAULT;
		}
		break;
	}

	return 0;
}


static int disp_gamma_set_listener(enum DISP_MODULE_ENUM module,
	ddp_module_notify notify)
{
	g_gamma_ddp_notify = notify;
	return 0;
}


static int disp_gamma_bypass(enum DISP_MODULE_ENUM module, int bypass)
{
	int relay = 0;

	if (bypass) {
		relay = 1;
		g_gamma_relay_value[index_of_gamma(module)] = 0x1;
	} else {
		g_gamma_relay_value[index_of_gamma(module)] = 0x0;
	}

	DISP_REG_MASK(NULL, DISP_REG_GAMMA_CFG + gamma_get_offset(module),
		relay, 0x1);

	GAMMA_DBG("Module(%d) (bypass = %d)\n", module, bypass);

	return 0;
}


static int disp_gamma_power_on(enum DISP_MODULE_ENUM module, void *handle)
{
#if defined(CONFIG_MACH_MT6755) || defined(CONFIG_MACH_ELBRUS) || \
	defined(CONFIG_MACH_MT6757) || defined(CONFIG_MACH_KIBOPLUS)
	/* gamma is DCM , do nothing */
#elif defined(CONFIG_MACH_MT6759) || defined(CONFIG_MACH_MT6758) || \
	defined(CONFIG_MACH_MT6763) || defined(CONFIG_MACH_MT6739) || \
	defined(CONFIG_MACH_MT6765) || defined(CONFIG_MACH_MT6761) || \
	defined(CONFIG_MACH_MT3967) || defined(CONFIG_MACH_MT6779)

	ddp_clk_prepare_enable(ddp_get_module_clk_id(module));
#else
#ifdef ENABLE_CLK_MGR
	if (module == GAMMA0_MODULE_NAMING) {
#ifdef CONFIG_MTK_CLKMGR
		enable_clock(MT_CG_DISP0_DISP_GAMMA, "GAMMA");
#else
		ddp_clk_enable(GAMMA0_CLK_NAMING);
#endif		/* CONFIG_MTK_CLKMGR */
	}
#if defined(CONFIG_MACH_MT6799)
	else if (module == DISP_MODULE_GAMMA1) {
#ifndef CONFIG_MTK_CLKMGR
		ddp_clk_enable(DISP0_DISP_GAMMA1);
#endif		/* not define CONFIG_MTK_CLKMGR */
	}
#endif
#endif		/* ENABLE_CLK_MGR */
#endif
	return 0;
}

static int disp_gamma_power_off(enum DISP_MODULE_ENUM module, void *handle)
{
#if defined(CONFIG_MACH_MT6755) || defined(CONFIG_MACH_ELBRUS) || \
	defined(CONFIG_MACH_MT6757) || defined(CONFIG_MACH_KIBOPLUS)
	/* gamma is DCM , do nothing */
#elif defined(CONFIG_MACH_MT6759) || defined(CONFIG_MACH_MT6758) || \
	defined(CONFIG_MACH_MT6763) || defined(CONFIG_MACH_MT6739) || \
	defined(CONFIG_MACH_MT6765) || defined(CONFIG_MACH_MT6761) || \
	defined(CONFIG_MACH_MT3967) || defined(CONFIG_MACH_MT6779)
	ddp_clk_disable_unprepare(ddp_get_module_clk_id(module));
#else
#ifdef ENABLE_CLK_MGR
	if (module == GAMMA0_MODULE_NAMING) {
#ifdef CONFIG_MTK_CLKMGR
		disable_clock(MT_CG_DISP0_DISP_GAMMA, "GAMMA");
#else
		ddp_clk_disable(GAMMA0_CLK_NAMING);
#endif		/* CONFIG_MTK_CLKMGR */
	}
#if defined(CONFIG_MACH_MT6799)
	else if (module == DISP_MODULE_GAMMA1) {
#ifndef CONFIG_MTK_CLKMGR
		ddp_clk_disable(DISP0_DISP_GAMMA1);
#endif		/* not define CONFIG_MTK_CLKMGR */
	}
#endif
#endif		/* ENABLE_CLK_MGR */
#endif
	return 0;
}


struct DDP_MODULE_DRIVER ddp_driver_gamma = {
	.start = disp_gamma_start,
	.config = disp_gamma_config,
	.bypass = disp_gamma_bypass,
	.set_listener = disp_gamma_set_listener,
	.cmd = disp_gamma_io,
#if !defined(CONFIG_MACH_MT6759) && !defined(CONFIG_MACH_MT6739)
	.init = disp_gamma_power_on,
	.deinit = disp_gamma_power_off,
#endif
	.power_on = disp_gamma_power_on,
	.power_off = disp_gamma_power_off,
#ifdef GAMMA_SUPPORT_PARTIAL_UPDATE
	.ioctl = gamma_ioctl,
#endif
};



/* ======================================================================== */
/*  COLOR CORRECTION							    */
/* ======================================================================== */

#if defined(CONFIG_MACH_ELBRUS) || defined(CONFIG_MACH_MT6757) || \
	defined(CONFIG_MACH_KIBOPLUS) || defined(CONFIG_MACH_MT6799) || \
	defined(CONFIG_MACH_MT6763) || defined(CONFIG_MACH_MT6758) || \
	defined(CONFIG_MACH_MT6739) || defined(CONFIG_MACH_MT6765) || \
	defined(CONFIG_MACH_MT6761) || defined(CONFIG_MACH_MT3967) || \
	defined(CONFIG_MACH_MT6779)
#define CCORR0_BASE_NAMING (DISPSYS_CCORR0_BASE)
#define CCORR0_MODULE_NAMING (DISP_MODULE_CCORR0)
#else
#define CCORR0_BASE_NAMING (DISPSYS_CCORR_BASE)
#define CCORR0_MODULE_NAMING (DISP_MODULE_CCORR)
#endif

#if defined(CONFIG_MACH_MT6799) || defined(CONFIG_MACH_MT6763) || \
	defined(CONFIG_MACH_MT6739) || defined(CONFIG_MACH_MT6765) || \
	defined(CONFIG_MACH_MT6761) || defined(CONFIG_MACH_MT3967) || \
	defined(CONFIG_MACH_MT6779)
#define CCORR0_CLK_NAMING (DISP0_DISP_CCORR0)
#else
#define CCORR0_CLK_NAMING (DISP0_DISP_CCORR)
#endif


#if defined(CONFIG_MACH_MT6797) || defined(CONFIG_MACH_MT6757) || \
	defined(CONFIG_MACH_KIBOPLUS) || defined(CONFIG_MACH_MT6799) || \
	defined(CONFIG_MACH_MT6763) || defined(CONFIG_MACH_MT6758) || \
	defined(CONFIG_MACH_MT6739) || defined(CONFIG_MACH_MT6765) || \
	defined(CONFIG_MACH_MT6761) || defined(CONFIG_MACH_MT3967) || \
	defined(CONFIG_MACH_MT6779)
#define CCORR_SUPPORT_PARTIAL_UPDATE
#endif

#define CCORR0_OFFSET (0)
#if defined(CONFIG_MACH_MT6799)
#define CCORR_TOTAL_MODULE_NUM (2)
#define CCORR1_OFFSET (DISPSYS_CCORR1_BASE - DISPSYS_CCORR0_BASE)

#define ccorr_get_offset(module) ((module == CCORR0_MODULE_NAMING) ? \
	CCORR0_OFFSET : CCORR1_OFFSET)
#define index_of_ccorr(module) ((module == CCORR0_MODULE_NAMING) ? 0 : 1)

static atomic_t g_ccorr_is_clock_on[CCORR_TOTAL_MODULE_NUM] = {
	ATOMIC_INIT(0), ATOMIC_INIT(0) };
#else
#define CCORR_TOTAL_MODULE_NUM (1)

#define ccorr_get_offset(module) (CCORR0_OFFSET)
#define index_of_ccorr(module) (0)

static atomic_t g_ccorr_is_clock_on[CCORR_TOTAL_MODULE_NUM] = {
	ATOMIC_INIT(0) };
#endif

#define CCORR_CLIP(val, min, max) ((val >= max) ? \
	max : ((val <= min) ? min : val))

static unsigned int g_ccorr_relay_value[CCORR_TOTAL_MODULE_NUM];

static struct DISP_CCORR_COEF_T *g_disp_ccorr_coef[DISP_CCORR_TOTAL] = { NULL };
static int g_ccorr_color_matrix[3][3] = {
	{1024, 0, 0},
	{0, 1024, 0},
	{0, 0, 1024} };
static int g_ccorr_prev_matrix[3][3] = {
	{1024, 0, 0},
	{0, 1024, 0},
	{0, 0, 1024} };
static int g_rgb_matrix[3][3] = {
	{1024, 0, 0},
	{0, 1024, 0},
	{0, 0, 1024} };
static struct DISP_CCORR_COEF_T g_multiply_matrix_coef;
static int g_disp_ccorr_without_gamma;

static DECLARE_WAIT_QUEUE_HEAD(g_ccorr_get_irq_wq);
static DEFINE_SPINLOCK(g_ccorr_get_irq_lock);
static atomic_t g_ccorr_get_irq = ATOMIC_INIT(0);

static ddp_module_notify g_ccorr_ddp_notify;

static int disp_ccorr_write_coef_reg(struct cmdqRecStruct *cmdq,
	enum DISP_MODULE_ENUM module,
		enum disp_ccorr_id_t id, int lock);
static void ccorr_dump_reg(void);

static void disp_ccorr_init(enum DISP_MODULE_ENUM module, unsigned int width,
	unsigned int height, void *cmdq)
{
	const int base_offset = ccorr_get_offset(module);

	DISP_REG_SET(cmdq, DISP_REG_CCORR_SIZE + base_offset,
		(width << 16) | height);
#if defined(CONFIG_MACH_MT6797) || defined(CONFIG_MACH_MT6757) || \
	defined(CONFIG_MACH_KIBOPLUS)
	/* disable stall cg for avoid display path hang */
	DISP_REG_MASK(cmdq, DISP_REG_CCORR_CFG + base_offset,
	0x0 << 8, 0x1 << 8);
#endif

#ifdef DISP_PLATFORM_HAS_SHADOW_REG
	if (disp_helper_get_option(DISP_OPT_SHADOW_REGISTER)) {
		if (disp_helper_get_option(DISP_OPT_SHADOW_MODE) == 0) {
			/* full shadow mode*/
			DISP_REG_MASK(cmdq, DISP_REG_CCORR_SHADOW + base_offset,
			0x0, 0x7);
		} else if (disp_helper_get_option(DISP_OPT_SHADOW_MODE) == 1) {
			/* force commit */
			DISP_REG_MASK(cmdq, DISP_REG_CCORR_SHADOW + base_offset,
			0x1 << 1, 0x7);
		} else if (disp_helper_get_option(DISP_OPT_SHADOW_MODE) == 2) {
			/* bypass shadow */
			DISP_REG_MASK(cmdq, DISP_REG_CCORR_SHADOW + base_offset,
			0x1 << 2, 0x7);
		}
	}
#endif

	/* enable with relay bit set for bypass ccorr engine */
	DISP_REG_SET(NULL, DISP_REG_CCORR_EN + base_offset, 0x1);
}

static int disp_ccorr_start(enum DISP_MODULE_ENUM module, void *cmdq)
{
#ifndef CONFIG_FPGA_EARLY_PORTING
		disp_ccorr_write_coef_reg(cmdq, module, DISP_CCORR0, 1);
#else
		DISP_REG_SET(cmdq,
		DISP_REG_CCORR_EN + ccorr_get_offset(module), 1);
		CCORR_DBG("FPGA_EARLY_PORTING");
#endif
	return 0;
}

static void disp_ccorr_multiply_3x3(unsigned int ccorrCoef[3][3],
		int color_matrix[3][3], unsigned int resultCoef[3][3])
{
	int temp_Result;
	int signedCcorrCoef[3][3];
	int i, j;

	/* convert unsigned 12 bit ccorr coefficient to signed 12 bit format */
	for (i = 0; i < 3; i += 1) {
		for (j = 0; j < 3; j += 1) {
			if (ccorrCoef[i][j] > 2047) {
				signedCcorrCoef[i][j] =
					(int)ccorrCoef[i][j] - 4096;
			} else {
				signedCcorrCoef[i][j] =
					(int)ccorrCoef[i][j];
			}
		}
	}

	for (i = 0; i < 3; i += 1) {
		CCORR_DBG("signedCcorrCoef[%d][0-2] = {%d, %d, %d}\n", i,
			signedCcorrCoef[i][0],
			signedCcorrCoef[i][1],
			signedCcorrCoef[i][2]);
	}

	temp_Result = (int)((signedCcorrCoef[0][0]*color_matrix[0][0] +
		signedCcorrCoef[0][1]*color_matrix[1][0] +
		signedCcorrCoef[0][2]*color_matrix[2][0]) / 1024);
	resultCoef[0][0] = CCORR_CLIP(temp_Result, -2048, 2047) & 0xFFF;

	temp_Result = (int)((signedCcorrCoef[0][0]*color_matrix[0][1] +
		signedCcorrCoef[0][1]*color_matrix[1][1] +
		signedCcorrCoef[0][2]*color_matrix[2][1]) / 1024);
	resultCoef[0][1] = CCORR_CLIP(temp_Result, -2048, 2047) & 0xFFF;

	temp_Result = (int)((signedCcorrCoef[0][0]*color_matrix[0][2] +
		signedCcorrCoef[0][1]*color_matrix[1][2] +
		signedCcorrCoef[0][2]*color_matrix[2][2]) / 1024);
	resultCoef[0][2] = CCORR_CLIP(temp_Result, -2048, 2047) & 0xFFF;


	temp_Result = (int)((signedCcorrCoef[1][0]*color_matrix[0][0] +
		signedCcorrCoef[1][1]*color_matrix[1][0] +
		signedCcorrCoef[1][2]*color_matrix[2][0]) / 1024);
	resultCoef[1][0] = CCORR_CLIP(temp_Result, -2048, 2047) & 0xFFF;

	temp_Result = (int)((signedCcorrCoef[1][0]*color_matrix[0][1] +
		signedCcorrCoef[1][1]*color_matrix[1][1] +
		signedCcorrCoef[1][2]*color_matrix[2][1]) / 1024);
	resultCoef[1][1] = CCORR_CLIP(temp_Result, -2048, 2047) & 0xFFF;

	temp_Result = (int)((signedCcorrCoef[1][0]*color_matrix[0][2] +
		signedCcorrCoef[1][1]*color_matrix[1][2] +
		signedCcorrCoef[1][2]*color_matrix[2][2]) / 1024);
	resultCoef[1][2] = CCORR_CLIP(temp_Result, -2048, 2047) & 0xFFF;


	temp_Result = (int)((signedCcorrCoef[2][0]*color_matrix[0][0] +
		signedCcorrCoef[2][1]*color_matrix[1][0] +
		signedCcorrCoef[2][2]*color_matrix[2][0]) / 1024);
	resultCoef[2][0] = CCORR_CLIP(temp_Result, -2048, 2047) & 0xFFF;

	temp_Result = (int)((signedCcorrCoef[2][0]*color_matrix[0][1] +
		signedCcorrCoef[2][1]*color_matrix[1][1] +
		signedCcorrCoef[2][2]*color_matrix[2][1]) / 1024);
	resultCoef[2][1] = CCORR_CLIP(temp_Result, -2048, 2047) & 0xFFF;

	temp_Result = (int)((signedCcorrCoef[2][0]*color_matrix[0][2] +
		signedCcorrCoef[2][1]*color_matrix[1][2] +
		signedCcorrCoef[2][2]*color_matrix[2][2]) / 1024);
	resultCoef[2][2] = CCORR_CLIP(temp_Result, -2048, 2047) & 0xFFF;

	for (i = 0; i < 3; i += 1) {
		CCORR_DBG("resultCoef[%d][0-2] = {0x%x, 0x%x, 0x%x}\n", i,
			resultCoef[i][0],
			resultCoef[i][1],
			resultCoef[i][2]);
	}
}

#define CCORR_REG(base, idx) (base + (idx) * 4 + 0x80)

static int disp_ccorr_write_coef_reg(struct cmdqRecStruct *cmdq,
	enum DISP_MODULE_ENUM module, enum disp_ccorr_id_t id, int lock)
{
	const int base_offset = ccorr_get_offset(module);
	const unsigned long ccorr_base = CCORR0_BASE_NAMING + base_offset;
	int ret = 0;
	struct DISP_CCORR_COEF_T *ccorr, *multiply_matrix;
	unsigned int cfg_val;
	unsigned int temp_matrix[3][3];

	if (module < CCORR0_MODULE_NAMING ||
		module >= CCORR0_MODULE_NAMING + CCORR_TOTAL_MODULE_NUM) {
		CCORR_ERR("invalid module = %d\n", module);
		return -EFAULT;
	}

	if (lock)
		mutex_lock(&g_gamma_global_lock);

	ccorr = g_disp_ccorr_coef[id];
	if (ccorr == NULL) {
		CCORR_DBG("[%d] not initialized\n", id);
		ret = -EFAULT;
		goto ccorr_write_coef_unlock;
	}

	if (id == 0) {
		multiply_matrix = &g_multiply_matrix_coef;
		disp_ccorr_multiply_3x3(ccorr->coef, g_ccorr_color_matrix,
			temp_matrix);
		disp_ccorr_multiply_3x3(temp_matrix, g_rgb_matrix,
			multiply_matrix->coef);
		ccorr = multiply_matrix;
	}

	DISP_REG_SET(cmdq, DISP_REG_CCORR_EN + base_offset, 1);

	cfg_val = 0x2 | g_ccorr_relay_value[index_of_ccorr(module)] |
		     (g_disp_ccorr_without_gamma << 2);
	DISP_REG_MASK(cmdq, DISP_REG_CCORR_CFG + base_offset, cfg_val, 0x7);

	DISP_REG_SET(cmdq, CCORR_REG(ccorr_base, 0),
		     ((ccorr->coef[0][0] << 16) | (ccorr->coef[0][1])));
	DISP_REG_SET(cmdq, CCORR_REG(ccorr_base, 1),
		     ((ccorr->coef[0][2] << 16) | (ccorr->coef[1][0])));
	DISP_REG_SET(cmdq, CCORR_REG(ccorr_base, 2),
		     ((ccorr->coef[1][1] << 16) | (ccorr->coef[1][2])));
	DISP_REG_SET(cmdq, CCORR_REG(ccorr_base, 3),
		     ((ccorr->coef[2][0] << 16) | (ccorr->coef[2][1])));
	DISP_REG_SET(cmdq, CCORR_REG(ccorr_base, 4), (ccorr->coef[2][2] << 16));

	CCORR_DBG("write coef register finish");
ccorr_write_coef_unlock:

	if (lock)
		mutex_unlock(&g_gamma_global_lock);

	return ret;
}

static void disp_ccorr_trigger_refresh(enum disp_ccorr_id_t id)
{
	if (g_ccorr_ddp_notify != NULL)
		g_ccorr_ddp_notify(CCORR0_MODULE_NAMING,
		DISP_PATH_EVENT_TRIGGER);
}

void disp_ccorr_on_end_of_frame(void)
{
	unsigned int intsta;
	unsigned long flags;

	intsta = DISP_REG_GET(DISP_REG_CCORR_INTSTA);

	CCORR_DBG("intsta: 0x%x", intsta);
	if (intsta & 0x2) {	/* End of frame */
		if (spin_trylock_irqsave(&g_ccorr_get_irq_lock, flags)) {
			DISP_CPU_REG_SET(DISP_REG_CCORR_INTSTA,
				(intsta & ~0x3));

			atomic_set(&g_ccorr_get_irq, 1);

			spin_unlock_irqrestore(&g_ccorr_get_irq_lock, flags);

			wake_up_interruptible(&g_ccorr_get_irq_wq);
		}
	}
}

#ifdef CCORR_TRANSITION
static DEFINE_SPINLOCK(g_pq_bl_change_lock);
static int g_pq_backlight;
static int g_pq_backlight_db;

static atomic_t g_ccorr_is_init_valid = ATOMIC_INIT(0);

static void disp_ccorr_set_interrupt(int enabled)
{
	unsigned int index = index_of_ccorr(CCORR0_MODULE_NAMING);

	if (atomic_read(&g_ccorr_is_clock_on[index]) != 1) {
		CCORR_DBG("clock is off");
		return;
	}

	if (enabled) {
		if (DISP_REG_GET(DISP_REG_CCORR_EN) == 0) {
			/* Print error message */
			CCORR_DBG("[WARNING] DISP_REG_CCORR_EN not enabled!");
		}

		/* Enable output frame end interrupt */
		DISP_CPU_REG_SET(DISP_REG_CCORR_INTEN, 0x2);
		CCORR_DBG("Interrupt enabled");
	} else {
		/* Disable output frame end interrupt */
		DISP_CPU_REG_SET(DISP_REG_CCORR_INTEN, 0x0);
		CCORR_DBG("Interrupt disabled");
	}
}

static void disp_ccorr_clear_irq_only(void)
{
	unsigned int intsta;
	unsigned long flags;

	intsta = DISP_REG_GET(DISP_REG_CCORR_INTSTA);

	CCORR_DBG("intsta: 0x%x", intsta);
	if (intsta & 0x2) { /* End of frame */
		if (spin_trylock_irqsave(&g_ccorr_get_irq_lock, flags)) {
			DISP_CPU_REG_SET(DISP_REG_CCORR_INTSTA,
				(intsta & ~0x3));

			spin_unlock_irqrestore(&g_ccorr_get_irq_lock, flags);
		}
	}

	disp_ccorr_set_interrupt(0);
}

static int disp_ccorr_wait_irq(unsigned long timeout)
{
	unsigned long flags;
	int ret = 0;

	if (atomic_read(&g_ccorr_get_irq) == 0) {
		ret = wait_event_interruptible(g_ccorr_get_irq_wq,
			atomic_read(&g_ccorr_get_irq) == 1);
		CCORR_DBG("get_irq = 1, waken up");
		CCORR_DBG("get_irq = 1, ret = %d", ret);
	} else {
		/* If g_ccorr_get_irq is already set, */
		/* means PQService was delayed */
		CCORR_DBG("get_irq = 0");
	}

	spin_lock_irqsave(&g_ccorr_get_irq_lock, flags);
	atomic_set(&g_ccorr_get_irq, 0);
	spin_unlock_irqrestore(&g_ccorr_get_irq_lock, flags);

	return ret;
}

static int disp_ccorr_exit_idle(int need_kick)
{
#if defined(CONFIG_MACH_MT6757) || defined(CONFIG_MACH_KIBOPLUS) || \
	defined(CONFIG_MACH_MT6799)
	if (need_kick == 1)
		if (disp_helper_get_option(DISP_OPT_IDLEMGR_ENTER_ULPS))
			primary_display_idlemgr_kick(__func__, 1);
#endif
	return 0;
}

static int disp_pq_copy_backlight_to_user(int __user *backlight)
{
	unsigned long flags;
	int ret = -EFAULT;

	disp_ccorr_exit_idle(1);

	/* We assume only one thread will call this function */
	spin_lock_irqsave(&g_pq_bl_change_lock, flags);
	g_pq_backlight_db = g_pq_backlight;
	spin_unlock_irqrestore(&g_pq_bl_change_lock, flags);

	if (copy_to_user(backlight, &g_pq_backlight_db, sizeof(int)) == 0)
		ret = 0;

	CCORR_DBG("%d", ret);

	return ret;
}
#endif

void disp_pq_notify_backlight_changed(int bl_1024)
{
#ifdef CCORR_TRANSITION
	unsigned long flags;
	int old_bl;

	spin_lock_irqsave(&g_pq_bl_change_lock, flags);
	old_bl = g_pq_backlight;
	g_pq_backlight = bl_1024;
	spin_unlock_irqrestore(&g_pq_bl_change_lock, flags);

	if (atomic_read(&g_ccorr_is_init_valid) != 1)
		return;

	CCORR_DBG("%d", bl_1024);

	if (old_bl == 0 || bl_1024 == 0) {
		disp_ccorr_set_interrupt(1);
		disp_ccorr_trigger_refresh(DISP_CCORR0);
		CCORR_DBG("trigger refresh when backlight ON/Off");
	}
#endif
}

static int disp_ccorr_set_coef
	(const struct DISP_CCORR_COEF_T __user *user_color_corr,
		enum DISP_MODULE_ENUM module, void *cmdq)
{
	int ret = 0;
	struct DISP_CCORR_COEF_T *ccorr, *old_ccorr;
	enum disp_ccorr_id_t id;

	ccorr = kmalloc(sizeof(struct DISP_CCORR_COEF_T), GFP_KERNEL);
	if (ccorr == NULL) {
		CCORR_ERR("no memory\n");
		return -EFAULT;
	}

	if (copy_from_user(ccorr, user_color_corr,
		sizeof(struct DISP_CCORR_COEF_T)) != 0) {
		ret = -EFAULT;
		kfree(ccorr);
	} else {
		id = ccorr->hw_id;
		if (id >= 0 && id < DISP_CCORR_TOTAL) {
			mutex_lock(&g_gamma_global_lock);

			old_ccorr = g_disp_ccorr_coef[id];
			g_disp_ccorr_coef[id] = ccorr;

			CCORR_DBG("Set module(%d) coef", module);
			ret = disp_ccorr_write_coef_reg(cmdq, module, id, 0);

			mutex_unlock(&g_gamma_global_lock);

			if (old_ccorr != NULL)
				kfree(old_ccorr);

			disp_ccorr_trigger_refresh(id);
		} else {
			CCORR_ERR("invalid ID = %d\n", id);
			ret = -EFAULT;
			kfree(ccorr);
		}
	}

	return ret;
}

static int disp_ccorr_config(enum DISP_MODULE_ENUM module,
	struct disp_ddp_path_config *pConfig, void *cmdq)
{
	if (pConfig->dst_dirty)
		disp_ccorr_init(module, pConfig->dst_w, pConfig->dst_h, cmdq);

	return 0;
}

int disp_ccorr_set_color_matrix(void *cmdq, int32_t matrix[16], int32_t hint)
{
	int ret = 0;
	int i, j;
	int ccorr_without_gamma = 0;
	bool need_refresh = false;

	if (cmdq == NULL) {
		CCORR_ERR("cmdq can not be NULL\n");
		return -EFAULT;
	}

	mutex_lock(&g_gamma_global_lock);

	for (i = 0; i < 3; i++) {
		for (j = 0; j < 3; j++) {
			/* Copy Color Matrix */
			g_ccorr_color_matrix[i][j] = matrix[j*4 + i];

			/* early jump out */
			if (ccorr_without_gamma == 1)
				continue;

			if (i == j && g_ccorr_color_matrix[i][j] != 1024)
				ccorr_without_gamma = 1;
			else if (i != j && g_ccorr_color_matrix[i][j] != 0)
				ccorr_without_gamma = 1;
		}
	}

	g_disp_ccorr_without_gamma = ccorr_without_gamma;

	disp_ccorr_write_coef_reg(cmdq, CCORR0_MODULE_NAMING, 0, 0);

	for (i = 0; i < 3; i++) {
		for (j = 0; j < 3; j++) {
			if (g_ccorr_prev_matrix[i][j]
				!= g_ccorr_color_matrix[i][j]) {
				/* refresh when matrix changed */
				need_refresh = true;
			}
			/* Copy Color Matrix */
			g_ccorr_prev_matrix[i][j] = g_ccorr_color_matrix[i][j];
		}
	}

	for (i = 0; i < 3; i += 1) {
		CCORR_DBG("g_ccorr_color_matrix[%d][0-2] = {%d, %d, %d}\n",
				i,
				g_ccorr_color_matrix[i][0],
				g_ccorr_color_matrix[i][1],
				g_ccorr_color_matrix[i][2]);
	}

	CCORR_DBG("g_disp_ccorr_without_gamma: [%d], need_refresh: [%d]\n",
		g_disp_ccorr_without_gamma, need_refresh);

	mutex_unlock(&g_gamma_global_lock);

	if (need_refresh == true)
		disp_ccorr_trigger_refresh(DISP_CCORR0);

	return ret;
}

int disp_ccorr_set_RGB_Gain(int r, int g, int b)
{
	int ret;

	mutex_lock(&g_gamma_global_lock);
	g_rgb_matrix[0][0] = r;
	g_rgb_matrix[1][1] = g;
	g_rgb_matrix[2][2] = b;

	CCORR_DBG("r[%d], g[%d], b[%d]", r, g, b);
	ret = disp_ccorr_write_coef_reg(NULL, CCORR0_MODULE_NAMING, 0, 0);
	mutex_unlock(&g_gamma_global_lock);
	return ret;
}


#ifdef CCORR_SUPPORT_PARTIAL_UPDATE
static int _ccorr_partial_update(enum DISP_MODULE_ENUM module,
	void *arg, void *cmdq)
{
	struct disp_rect *roi = (struct disp_rect *) arg;
	int width = roi->width;
	int height = roi->height;

	DISP_REG_SET(cmdq, DISP_REG_CCORR_SIZE + ccorr_get_offset(module),
		(width << 16) | height);
	return 0;
}

static int ccorr_ioctl(enum DISP_MODULE_ENUM module, void *handle,
		enum DDP_IOCTL_NAME ioctl_cmd, void *params)
{
	int ret = -1;

	if (ioctl_cmd == DDP_PARTIAL_UPDATE) {
		_ccorr_partial_update(module, params, handle);
		ret = 0;
	}
	return ret;
}
#endif

static int disp_ccorr_io(enum DISP_MODULE_ENUM module, unsigned int msg,
	unsigned long arg, void *cmdq)
{
	struct DISP_COLOR_TRANSFORM color_transform;
	unsigned int i;
#ifdef CCORR_TRANSITION
	int enabled;
#endif

	switch (msg) {
	case DISP_IOCTL_SET_CCORR:
		if (disp_ccorr_set_coef((struct DISP_CCORR_COEF_T *) arg,
			module, cmdq) < 0) {
			CCORR_ERR("DISP_IOCTL_SET_CCORR: failed\n");
			return -EFAULT;
		}
		break;
#ifdef CCORR_TRANSITION
	case DISP_IOCTL_CCORR_EVENTCTL:
		if (copy_from_user(&enabled, (void *)arg,
			sizeof(enabled))) {
			CCORR_ERR("DISP_IOCTL_CCORR_EVENTCTL: failed");
			return -EFAULT;
		}

		disp_ccorr_set_interrupt(enabled);

		if (enabled)
			disp_ccorr_trigger_refresh(DISP_CCORR0);

		break;
	case DISP_IOCTL_CCORR_GET_IRQ:
		atomic_set(&g_ccorr_is_init_valid, 1);
		disp_ccorr_wait_irq(60);
		if (disp_pq_copy_backlight_to_user((int *) arg) < 0) {
			CCORR_ERR("DISP_IOCTL_CCORR_GET_IRQ: failed");
			return -EFAULT;
		}

		break;
#endif
	case DISP_IOCTL_SUPPORT_COLOR_TRANSFORM:
		if (copy_from_user(&color_transform, (void *)arg,
			sizeof(struct DISP_COLOR_TRANSFORM))) {
			CCORR_ERR("DISP_IOCTL_SUPPORT_COLOR_TRANSFORM: failed");
			return -EFAULT;
		}

		for (i = 0 ; i < 3; i++) {
			if (color_transform.matrix[3][i] != 0 ||
				color_transform.matrix[i][3] != 0) {
				CCORR_DBG("unsupported matrix");
				return -EFAULT;
			}
		}
		break;
	}

	return 0;
}


static int disp_ccorr_set_listener(enum DISP_MODULE_ENUM module,
	ddp_module_notify notify)
{
	g_ccorr_ddp_notify = notify;
	return 0;
}


static int disp_ccorr_bypass(enum DISP_MODULE_ENUM module, int bypass)
{
	const int base_offset = ccorr_get_offset(module);
	int relay = 0;

	if (bypass) {
		relay = 1;
		g_ccorr_relay_value[index_of_ccorr(module)] = 0x1;
	} else {
		g_ccorr_relay_value[index_of_ccorr(module)] = 0x0;
	}

	DISP_REG_MASK(NULL, DISP_REG_CCORR_CFG + base_offset, 1, 1);

	CCORR_DBG("Module(%d) (bypass = %d)", module, bypass);

	return 0;
}

static int disp_ccorr_power_on(enum DISP_MODULE_ENUM module, void *handle)
{
#if defined(CONFIG_MACH_MT6759) || defined(CONFIG_MACH_MT6758) || \
	defined(CONFIG_MACH_MT6763) || defined(CONFIG_MACH_MT6739) || \
	defined(CONFIG_MACH_MT6765) || defined(CONFIG_MACH_MT6761) || \
	defined(CONFIG_MACH_MT3967) || defined(CONFIG_MACH_MT6779)
	ddp_clk_prepare_enable(ddp_get_module_clk_id(module));
#else
#ifdef ENABLE_CLK_MGR
	if (module == CCORR0_MODULE_NAMING) {
#ifdef CONFIG_MTK_CLKMGR
#if !defined(CONFIG_MACH_MT6580)
		enable_clock(MT_CG_DISP0_DISP_CCORR, "CCORR");
#endif
#else
		ddp_clk_enable(CCORR0_CLK_NAMING);
#endif		/* CONFIG_MTK_CLKMGR */
	}
#if defined(CONFIG_MACH_MT6799)
	else if (module == DISP_MODULE_CCORR1) {
#ifndef CONFIG_MTK_CLKMGR
		ddp_clk_enable(DISP0_DISP_CCORR1);
#endif		/* not define CONFIG_MTK_CLKMGR */
	}
#endif
#endif		/* ENABLE_CLK_MGR */
#endif

	atomic_set(&g_ccorr_is_clock_on[index_of_ccorr(module)], 1);

	return 0;
}

static int disp_ccorr_power_off(enum DISP_MODULE_ENUM module, void *handle)
{
#ifdef CCORR_TRANSITION
	disp_ccorr_clear_irq_only();
#endif

	atomic_set(&g_ccorr_is_clock_on[index_of_ccorr(module)], 0);

#if defined(CONFIG_MACH_MT6759) || defined(CONFIG_MACH_MT6758) || \
	defined(CONFIG_MACH_MT6763) || defined(CONFIG_MACH_MT6739) || \
	defined(CONFIG_MACH_MT6765) || defined(CONFIG_MACH_MT6761) || \
	defined(CONFIG_MACH_MT3967) || defined(CONFIG_MACH_MT6779)
	ddp_clk_disable_unprepare(ddp_get_module_clk_id(module));
#else

#ifdef ENABLE_CLK_MGR
	if (module == CCORR0_MODULE_NAMING) {
#ifdef CONFIG_MTK_CLKMGR
#if !defined(CONFIG_MACH_MT6580)
		disable_clock(MT_CG_DISP0_DISP_CCORR, "CCORR");
#endif
#else
		ddp_clk_disable(CCORR0_CLK_NAMING);
#endif		/* CONFIG_MTK_CLKMGR */

#ifdef CCORR_TRANSITION
		disp_ccorr_set_interrupt(0);
#endif
	}
#if defined(CONFIG_MACH_MT6799)
	else if (module == DISP_MODULE_CCORR1) {
#ifndef CONFIG_MTK_CLKMGR
		ddp_clk_disable(DISP0_DISP_CCORR1);
#endif		/* not define CONFIG_MTK_CLKMGR */
	}
#endif
#endif		/* ENABLE_CLK_MGR */
#endif

	return 0;
}


struct DDP_MODULE_DRIVER ddp_driver_ccorr = {
	.config = disp_ccorr_config,
	.start = disp_ccorr_start,
	.bypass = disp_ccorr_bypass,
	.set_listener = disp_ccorr_set_listener,
	.cmd = disp_ccorr_io,
#if !defined(CONFIG_MACH_MT6759) && !defined(CONFIG_MACH_MT6739)
	.init = disp_ccorr_power_on,
	.deinit = disp_ccorr_power_off,
#endif
	.power_on = disp_ccorr_power_on,
	.power_off = disp_ccorr_power_off,
#ifdef CCORR_SUPPORT_PARTIAL_UPDATE
	.ioctl = ccorr_ioctl,
#endif
};

int ccorr_coef_interface(enum DISP_MODULE_ENUM module,
	unsigned int ccorr_coef_ref[3][3], void *handle)
{
	const enum disp_ccorr_id_t id = DISP_CCORR0;
	int y, x;
	struct DISP_CCORR_COEF_T *ccorr;

	if (g_disp_ccorr_coef[id] == NULL) {
		g_disp_ccorr_coef[id] =
			kmalloc(sizeof(struct DISP_CCORR_COEF_T), GFP_KERNEL);
		if (g_disp_ccorr_coef[id] == NULL) {
			CCORR_ERR("disp_ccorr_set_coef: no memory\n");
			return -EFAULT;
		}
		CCORR_DBG("ccorr_interface_for_color:allocate coef buffer");
		ccorr = g_disp_ccorr_coef[id];
	} else {
		ccorr = g_disp_ccorr_coef[id];
	}

	for (y = 0; y < 3; y += 1)
		for (x = 0; x < 3; x += 1)
			ccorr->coef[y][x] = ccorr_coef_ref[y][x];

	CCORR_DBG("== CCORR[%d] Coefficient ==", id);
	CCORR_DBG("%4d %4d %4d", ccorr->coef[0][0], ccorr->coef[0][1],
		ccorr->coef[0][2]);
	CCORR_DBG("%4d %4d %4d", ccorr->coef[1][0], ccorr->coef[1][1],
		ccorr->coef[1][2]);
	CCORR_DBG("%4d %4d %4d", ccorr->coef[2][0], ccorr->coef[2][1],
		ccorr->coef[2][2]);

	disp_ccorr_write_coef_reg(handle, module, id, 1);

	return 0;

}

static int ddp_simple_strtoul(char *ptr, unsigned long *res)
{
	int i;
	char buffer[20];
	int end = 0;
	int ret = 0;

	for (i = 0; i < 20; i += 1) {
		end = i;
		CCORR_DBG("%c\n", ptr[i]);
		if (ptr[i] < '0' || ptr[i] > '9')
			break;
	}

	if (end > 0) {
		strncpy(buffer, ptr, end);
		buffer[end] = '\0';
		ret = kstrtoul(buffer, 0, res);

	}
	return end;

}

static int ccorr_parse_coef(const char *cmd, enum DISP_MODULE_ENUM module,
	void *handle)
{
	int i, j, end;
	bool stop = false;
	int count = 0;
	unsigned long temp;
	unsigned int ccorr_coef[3][3];
	char *next = (char *)cmd;

	for (i = 0; i < 3; i++) {
		for (j = 0; j < 3; j++) {
			end = ddp_simple_strtoul(next,
				(unsigned long *)(&temp));
			next += end;

			ccorr_coef[i][j] = (unsigned int)temp;
			count++;

			CCORR_DBG("ccorr coef(%3d,%3d)=%d\n",
				i, j, ccorr_coef[i][j]);

			if (*next == ',')
				next++;
			else if (*next == '\0' || *next == '\n') {
				stop = true;
				break;
			}
		}
		if (stop == true)
			break;
	}

	if (count != 9) {
		CCORR_DBG("ccorr coef# not correct\n");

	} else {
		ccorr_coef_interface(module, ccorr_coef, handle);
		CCORR_DBG("ccorr coef config done\n");
	}
	return 0;
}

static int ccorr_parse_triple(const char *cmd, unsigned long *offset,
	unsigned long *value, unsigned long *mask)
{
	int count = 0;
	char *next = (char *)cmd;
	int end;

	*value = 0;
	*mask = 0;
	end = ddp_simple_strtoul(next, offset);
	next += end;
	if (*offset > 0x1000UL || (*offset & 0x3UL) != 0)  {
		*offset = 0UL;
		return 0;
	}

	count++;

	if (*next == ',')
		next++;

	end = ddp_simple_strtoul(next, value);
	next += end;
	count++;

	if (*next == ',')
		next++;

	end = ddp_simple_strtoul(next, mask);
	next += end;
	count++;

	return count;
}

static void ccorr_dump_reg(void)
{
	const unsigned long reg_base = CCORR0_BASE_NAMING;
	int offset;

	CCORR_DBG("[DUMP] Base = 0x%lx", reg_base);
	CCORR_DBG("Basic Setting");
	for (offset = 0; offset <= 0x30; offset += 4) {
		unsigned int val = DISP_REG_GET(reg_base + offset);

		CCORR_DBG("[+0x%02x] = 0x%08x", offset, val);
	}
	CCORR_DBG("Coefficient");
	for (offset = 0x80; offset <= 0x90; offset += 4) {
		unsigned int val = DISP_REG_GET(reg_base + offset);

		CCORR_DBG("[+0x%02x] = 0x%08x", offset, val);
	}
}

void ccorr_test(const char *cmd, char *debug_output)
{
	unsigned long offset;
	unsigned long value, mask;
	enum DISP_MODULE_ENUM module = CCORR0_MODULE_NAMING;
	int i;
	int config_module_num = 1;

#if defined(CONFIG_MACH_MT6799)
	if (primary_display_get_pipe_status() == DUAL_PIPE)
		config_module_num = CCORR_TOTAL_MODULE_NUM;
#endif

	CCORR_DBG("(%s)", cmd);

	debug_output[0] = '\0';

	if (strncmp(cmd, "set:", 4) == 0) {
		int count = ccorr_parse_triple(cmd + 4, &offset, &value, &mask);

		for (i = 0; i < config_module_num; i++) {
			module += i;
			if (count == 3) {
				DISP_REG_MASK(NULL, CCORR0_BASE_NAMING +
					ccorr_get_offset(module) + offset,
					value, mask);
			} else if (count == 2) {
				DISP_REG_SET(NULL, CCORR0_BASE_NAMING +
					ccorr_get_offset(module) + offset,
					value);
				mask = 0xffffffff;
			}
		}

		if (count >= 2) {
			CCORR_DBG("[+0x%031lx] = 0x%08lx(%d) & 0x%08lx",
				offset, value, (int)value, mask);
		}
	} else if (strncmp(cmd, "coef:", 5) == 0) {
		for (i = 0; i < config_module_num; i++) {
			module += i;
			ccorr_parse_coef(cmd+5, module, NULL);
		}
	} else if (strncmp(cmd, "dump", 4) == 0) {
		ccorr_dump_reg();
	} else if (strncmp(cmd, "en:", 3) == 0) {
		int enabled = (cmd[3] == '1' ? 1 : 0);
		int bypass = (enabled == 0 ? 1 : 0);

		for (i = 0; i < config_module_num; i++) {
			module += i;
			disp_ccorr_bypass(module, bypass);
		}
	} else if (strncmp(cmd, "dbg:", 4) == 0) {
		corr_dbg_en = cmd[4] - '0';
		corr_dbg_en = (corr_dbg_en > 1) ? 1 : corr_dbg_en;
		CCORR_DBG("debug log status:%d", corr_dbg_en);

	} else {

	}
	disp_ccorr_trigger_refresh(DISP_CCORR0);
}

