/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <asm/uaccess.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#ifdef CONFIG_MTK_CLKMGR
#include <mach/mt_clkmgr.h>
#else
#if defined(CONFIG_ARCH_MT6755) || defined(CONFIG_ARCH_MT6797) || \
	defined(CONFIG_ARCH_MT6757) || defined(CONFIG_ARCH_ELBRUS)
#include <ddp_clkmgr.h>
#endif
#endif
#include <cmdq_record.h>
#include <ddp_drv.h>
#include <ddp_reg.h>
#include <ddp_path.h>
#include <ddp_gamma.h>
#include <disp_drv_platform.h>
#if defined(CONFIG_ARCH_MT6755) || defined(CONFIG_ARCH_MT6797) || defined(CONFIG_ARCH_MT6757)
#include <disp_helper.h>
#endif

/* To enable debug log: */
/* # echo corr_dbg:1 > /sys/kernel/debug/dispsys */
int corr_dbg_en = 0;
int ccorr_scenario = 0;
#define GAMMA_ERR(fmt, arg...) pr_err("[GAMMA] " fmt "\n", ##arg)
#define GAMMA_NOTICE(fmt, arg...) do { if (corr_dbg_en) pr_debug("[GAMMA] " fmt "\n", ##arg); } while (0)
#define GAMMA_DBG(fmt, arg...) do { if (corr_dbg_en) pr_debug("[GAMMA] " fmt "\n", ##arg); } while (0)
#define CCORR_ERR(fmt, arg...) pr_err("[CCORR] " fmt "\n", ##arg)
#define CCORR_NOTICE(fmt, arg...) do { if (corr_dbg_en) pr_debug("[CCORR] " fmt "\n", ##arg); } while (0)
#define CCORR_DBG(fmt, arg...) do { if (corr_dbg_en) pr_debug("[CCORR] " fmt "\n", ##arg); } while (0)

static DEFINE_MUTEX(g_gamma_global_lock);


/* ======================================================================== */
/*  GAMMA                                                                   */
/* ======================================================================== */

#if defined(CONFIG_ARCH_ELBRUS) || defined(CONFIG_ARCH_MT6757)
#define GAMMA0_MODULE_NAMING (DISP_MODULE_GAMMA0)
#else
#define GAMMA0_MODULE_NAMING (DISP_MODULE_GAMMA)
#endif

#if defined(CONFIG_ARCH_MT6797) || defined(CONFIG_ARCH_MT6757)
#define GAMMA_SUPPORT_PARTIAL_UPDATE
#endif

static DISP_GAMMA_LUT_T *g_disp_gamma_lut[DISP_GAMMA_TOTAL] = { NULL };

static ddp_module_notify g_gamma_ddp_notify;


static int disp_gamma_write_lut_reg(cmdqRecHandle cmdq, disp_gamma_id_t id, int lock);

static int disp_gamma_start(DISP_MODULE_ENUM module, void *cmdq)
{
	disp_gamma_write_lut_reg(cmdq, DISP_GAMMA0, 1);

	return 0;
}

static void disp_gamma_init(disp_gamma_id_t id, unsigned int width, unsigned int height, void *cmdq)
{
	DISP_REG_SET(cmdq, DISP_REG_GAMMA_SIZE, (width << 16) | height);
#if defined(CONFIG_ARCH_MT6797) || defined(CONFIG_ARCH_MT6757) /* disable stall cg for avoid display path hang */
	DISP_REG_MASK(cmdq, DISP_REG_GAMMA_CFG, 0x0 << 8, 0x1 << 8);
#endif

#ifdef DISP_PLATFORM_HAS_SHADOW_REG
	if (disp_helper_get_option(DISP_OPT_SHADOW_REGISTER)) {
		if (disp_helper_get_option(DISP_OPT_SHADOW_MODE) == 0) {
			/* full shadow mode*/
			DISP_REG_MASK(cmdq, DISP_REG_GAMMA_DEBUG, 0x0, 0x7);
		} else if (disp_helper_get_option(DISP_OPT_SHADOW_MODE) == 1) {
			/* force commit */
			DISP_REG_MASK(cmdq, DISP_REG_GAMMA_DEBUG, 0x1 << 1, 0x7);
		} else if (disp_helper_get_option(DISP_OPT_SHADOW_MODE) == 2) {
			/* bypass shadow */
			DISP_REG_MASK(cmdq, DISP_REG_GAMMA_DEBUG, 0x1, 0x7);
		}
	}
#endif

}

static int disp_gamma_config(DISP_MODULE_ENUM module, disp_ddp_path_config *pConfig, void *cmdq)
{
	if (pConfig->dst_dirty)
		disp_gamma_init(DISP_GAMMA0, pConfig->dst_w, pConfig->dst_h, cmdq);
	return 0;
}


static void disp_gamma_trigger_refresh(disp_gamma_id_t id)
{
	if (g_gamma_ddp_notify != NULL)
		g_gamma_ddp_notify(GAMMA0_MODULE_NAMING, DISP_PATH_EVENT_TRIGGER);
}


static int disp_gamma_write_lut_reg(cmdqRecHandle cmdq, disp_gamma_id_t id, int lock)
{
	unsigned long lut_base = 0;
	DISP_GAMMA_LUT_T *gamma_lut;
	int i;
	int ret = 0;

	if (id >= DISP_GAMMA_TOTAL) {
		GAMMA_ERR("disp_gamma_write_lut_reg: invalid ID = %d\n", id);
		return -EFAULT;
	}

	if (lock)
		mutex_lock(&g_gamma_global_lock);

	gamma_lut = g_disp_gamma_lut[id];
	if (gamma_lut == NULL) {
		GAMMA_ERR(
		       "disp_gamma_write_lut_reg: gamma table [%d] not initialized\n", id);
		ret = -EFAULT;
		goto gamma_write_lut_unlock;
	}

	if (id == DISP_GAMMA0) {
		DISP_REG_MASK(cmdq, DISP_REG_GAMMA_EN, 0x1, 0x1);
		DISP_REG_MASK(cmdq, DISP_REG_GAMMA_CFG, 0x2, 0x2);
		lut_base = DISP_REG_GAMMA_LUT;
	} else {
		ret = -EFAULT;
		goto gamma_write_lut_unlock;
	}

	for (i = 0; i < DISP_GAMMA_LUT_SIZE; i++) {
		DISP_REG_MASK(cmdq, (lut_base + i * 4), gamma_lut->lut[i], ~0);

		if ((i & 0x3f) == 0) {
			GAMMA_DBG("[0x%08lx](%d) = 0x%x\n", (lut_base + i * 4), i,
			       gamma_lut->lut[i]);
		}
	}
	i--;
	GAMMA_DBG("[0x%08lx](%d) = 0x%x\n", (lut_base + i * 4), i,
	       gamma_lut->lut[i]);

gamma_write_lut_unlock:

	if (lock)
		mutex_unlock(&g_gamma_global_lock);

	return ret;
}


static int disp_gamma_set_lut(const DISP_GAMMA_LUT_T __user *user_gamma_lut, void *cmdq)
{
	int ret = 0;
	disp_gamma_id_t id;
	DISP_GAMMA_LUT_T *gamma_lut, *old_lut;

	GAMMA_DBG("disp_gamma_set_lut(cmdq = %d)", (cmdq != NULL ? 1 : 0));

	gamma_lut = kmalloc(sizeof(DISP_GAMMA_LUT_T), GFP_KERNEL);
	if (gamma_lut == NULL) {
		GAMMA_ERR("disp_gamma_set_lut: no memory\n");
		return -EFAULT;
	}

	if (copy_from_user(gamma_lut, user_gamma_lut, sizeof(DISP_GAMMA_LUT_T)) != 0) {
		ret = -EFAULT;
		kfree(gamma_lut);
	} else {
		id = gamma_lut->hw_id;
		if (0 <= id && id < DISP_GAMMA_TOTAL) {
			mutex_lock(&g_gamma_global_lock);

			old_lut = g_disp_gamma_lut[id];
			g_disp_gamma_lut[id] = gamma_lut;

			ret = disp_gamma_write_lut_reg(cmdq, id, 0);

			mutex_unlock(&g_gamma_global_lock);

			if (old_lut != NULL)
				kfree(old_lut);

			disp_gamma_trigger_refresh(id);
		} else {
			GAMMA_ERR("disp_gamma_set_lut: invalid ID = %d\n", id);
			ret = -EFAULT;
		}
	}

	return ret;
}

#ifdef GAMMA_SUPPORT_PARTIAL_UPDATE
static int _gamma_partial_update(DISP_MODULE_ENUM module, void *arg, void *cmdq)
{
	struct disp_rect *roi = (struct disp_rect *) arg;
	int width = roi->width;
	int height = roi->height;

	DISP_REG_SET(cmdq, DISP_REG_GAMMA_SIZE, (width << 16) | height);
	return 0;
}

static int gamma_ioctl(DISP_MODULE_ENUM module, void *handle,
		DDP_IOCTL_NAME ioctl_cmd, void *params)
{
	int ret = -1;

	if (ioctl_cmd == DDP_PARTIAL_UPDATE) {
		_gamma_partial_update(module, params, handle);
		ret = 0;
	}
	return ret;
}
#endif

static int disp_gamma_io(DISP_MODULE_ENUM module, int msg, unsigned long arg, void *cmdq)
{
	switch (msg) {
	case DISP_IOCTL_SET_GAMMALUT:
		if (disp_gamma_set_lut((DISP_GAMMA_LUT_T *) arg, cmdq) < 0) {
			GAMMA_ERR("DISP_IOCTL_SET_GAMMALUT: failed\n");
			return -EFAULT;
		}
		break;
	}

	return 0;
}


static int disp_gamma_set_listener(DISP_MODULE_ENUM module, ddp_module_notify notify)
{
	g_gamma_ddp_notify = notify;
	return 0;
}


static int disp_gamma_bypass(DISP_MODULE_ENUM module, int bypass)
{
	int relay = 0;

	if (bypass)
		relay = 1;

	DISP_REG_MASK(NULL, DISP_REG_GAMMA_CFG, relay, 0x1);

	GAMMA_DBG("disp_gamma_bypass(bypass = %d)\n", bypass);

	return 0;
}


static int disp_gamma_power_on(DISP_MODULE_ENUM module, void *handle)
{
#if defined(CONFIG_ARCH_MT6755) || defined(CONFIG_ARCH_ELBRUS) || defined(CONFIG_ARCH_MT6757)
	/* gamma is DCM , do nothing */
#else
#ifdef ENABLE_CLK_MGR
	if (module == DISP_MODULE_GAMMA) {
#ifdef CONFIG_MTK_CLKMGR
		enable_clock(MT_CG_DISP0_DISP_GAMMA, "GAMMA");
#else
		ddp_clk_enable(DISP0_DISP_GAMMA);
#endif
	}
#endif
#endif
	return 0;
}

static int disp_gamma_power_off(DISP_MODULE_ENUM module, void *handle)
{
#if defined(CONFIG_ARCH_MT6755) || defined(CONFIG_ARCH_ELBRUS) || defined(CONFIG_ARCH_MT6757)
	/* gamma is DCM , do nothing */
#else
#ifdef ENABLE_CLK_MGR
	if (module == DISP_MODULE_GAMMA) {
#ifdef CONFIG_MTK_CLKMGR
		disable_clock(MT_CG_DISP0_DISP_GAMMA, "GAMMA");
#else
		ddp_clk_disable(DISP0_DISP_GAMMA);
#endif
	}
#endif
#endif
	return 0;
}


DDP_MODULE_DRIVER ddp_driver_gamma = {
	.start = disp_gamma_start,
	.config = disp_gamma_config,
	.bypass = disp_gamma_bypass,
	.set_listener = disp_gamma_set_listener,
	.cmd = disp_gamma_io,
	.init = disp_gamma_power_on,
	.deinit = disp_gamma_power_off,
	.power_on = disp_gamma_power_on,
	.power_off = disp_gamma_power_off,
#ifdef GAMMA_SUPPORT_PARTIAL_UPDATE
	.ioctl = gamma_ioctl,
#endif
};



/* ======================================================================== */
/*  COLOR CORRECTION                                                        */
/* ======================================================================== */

#if defined(CONFIG_ARCH_ELBRUS) || defined(CONFIG_ARCH_MT6757)
#define CCORR0_BASE_NAMING (DISPSYS_CCORR0_BASE)
#define CCORR0_MODULE_NAMING (DISP_MODULE_CCORR0)
#else
#define CCORR0_BASE_NAMING (DISPSYS_CCORR_BASE)
#define CCORR0_MODULE_NAMING (DISP_MODULE_CCORR)
#endif

#if defined(CONFIG_ARCH_MT6797) || defined(CONFIG_ARCH_MT6757)
#define CCORR_SUPPORT_PARTIAL_UPDATE
#endif

static DISP_CCORR_COEF_T *g_disp_ccorr_coef[DISP_CCORR_TOTAL] = { NULL };

static ddp_module_notify g_ccorr_ddp_notify;

static int disp_ccorr_write_coef_reg(cmdqRecHandle cmdq, disp_ccorr_id_t id, int lock);
static void ccorr_dump_reg(void);


static void disp_ccorr_init(disp_ccorr_id_t id, unsigned int width, unsigned int height, void *cmdq)
{
	DISP_REG_SET(cmdq, DISP_REG_CCORR_SIZE, (width << 16) | height);
#if defined(CONFIG_ARCH_MT6797) || defined(CONFIG_ARCH_MT6757) /* disable stall cg for avoid display path hang */
	DISP_REG_MASK(cmdq, DISP_REG_CCORR_CFG, 0x0 << 8, 0x1 << 8);
#endif
}

static int disp_ccorr_start(DISP_MODULE_ENUM module, void *cmdq)
{
#ifndef CONFIG_FPGA_EARLY_PORTING
		disp_ccorr_write_coef_reg(cmdq, 0, 1);
#else
		DISP_REG_SET(cmdq, DISP_REG_CCORR_EN, 1);
		CCORR_DBG("FPGA_EARLY_PORTING");
#endif
	return 0;
}

#define CCORR_REG(base, idx) (base + (idx) * 4 + 0x80)

static int disp_ccorr_write_coef_reg(cmdqRecHandle cmdq, disp_ccorr_id_t id, int lock)
{
	const unsigned long ccorr_base = CCORR0_BASE_NAMING;
	int ret = 0;
	DISP_CCORR_COEF_T *ccorr;

	if (lock)
		mutex_lock(&g_gamma_global_lock);

	ccorr = g_disp_ccorr_coef[id];
	if (ccorr == NULL) {
		CCORR_DBG("disp_ccorr_write_coef_reg: [%d] not initialized\n", id);
		ret = -EFAULT;
		goto ccorr_write_coef_unlock;
	}

	DISP_REG_SET(cmdq, DISP_REG_CCORR_EN, 1);
	DISP_REG_MASK(cmdq, DISP_REG_CCORR_CFG, 0x2, 0x3);

	DISP_REG_SET(cmdq, CCORR_REG(ccorr_base, 0),
		     ((ccorr->coef[0][0] << 16) | (ccorr->coef[0][1])));
	DISP_REG_SET(cmdq, CCORR_REG(ccorr_base, 1),
		     ((ccorr->coef[0][2] << 16) | (ccorr->coef[1][0])));
	DISP_REG_SET(cmdq, CCORR_REG(ccorr_base, 2),
		     ((ccorr->coef[1][1] << 16) | (ccorr->coef[1][2])));
	DISP_REG_SET(cmdq, CCORR_REG(ccorr_base, 3),
		     ((ccorr->coef[2][0] << 16) | (ccorr->coef[2][1])));
	DISP_REG_SET(cmdq, CCORR_REG(ccorr_base, 4), (ccorr->coef[2][2] << 16));

	CCORR_DBG("disp_ccorr_write_coef_reg");
ccorr_write_coef_unlock:

	if (lock)
		mutex_unlock(&g_gamma_global_lock);

	return ret;
}


static void disp_ccorr_trigger_refresh(disp_ccorr_id_t id)
{
	if (g_ccorr_ddp_notify != NULL)
		g_ccorr_ddp_notify(CCORR0_MODULE_NAMING, DISP_PATH_EVENT_TRIGGER);
}


static int disp_ccorr_set_coef(const DISP_CCORR_COEF_T __user *user_color_corr, void *cmdq)
{
	int ret = 0;
	DISP_CCORR_COEF_T *ccorr, *old_ccorr;
	disp_ccorr_id_t id;

	ccorr = kmalloc(sizeof(DISP_CCORR_COEF_T), GFP_KERNEL);
	if (ccorr == NULL) {
		CCORR_ERR("disp_ccorr_set_coef: no memory\n");
		return -EFAULT;
	}

	if (copy_from_user(ccorr, user_color_corr, sizeof(DISP_CCORR_COEF_T)) != 0) {
		ret = -EFAULT;
		kfree(ccorr);
	} else {
		id = ccorr->hw_id;
		if (0 <= id && id < DISP_CCORR_TOTAL) {
			mutex_lock(&g_gamma_global_lock);

			old_ccorr = g_disp_ccorr_coef[id];
			g_disp_ccorr_coef[id] = ccorr;

			ret = disp_ccorr_write_coef_reg(cmdq, id, 0);

			mutex_unlock(&g_gamma_global_lock);

			if (old_ccorr != NULL)
				kfree(old_ccorr);

			disp_ccorr_trigger_refresh(id);
		} else {
			CCORR_ERR("disp_ccorr_set_coef: invalid ID = %d\n", id);
			ret = -EFAULT;
		}
	}

	return ret;
}


static int disp_ccorr_config(DISP_MODULE_ENUM module, disp_ddp_path_config *pConfig, void *cmdq)
{
	if (pConfig->dst_dirty)
		disp_ccorr_init(DISP_CCORR0, pConfig->dst_w, pConfig->dst_h, cmdq);

	return 0;
}

#ifdef CCORR_SUPPORT_PARTIAL_UPDATE
static int _ccorr_partial_update(DISP_MODULE_ENUM module, void *arg, void *cmdq)
{
	struct disp_rect *roi = (struct disp_rect *) arg;
	int width = roi->width;
	int height = roi->height;

	DISP_REG_SET(cmdq, DISP_REG_CCORR_SIZE, (width << 16) | height);
	return 0;
}

static int ccorr_ioctl(DISP_MODULE_ENUM module, void *handle,
		DDP_IOCTL_NAME ioctl_cmd, void *params)
{
	int ret = -1;

	if (ioctl_cmd == DDP_PARTIAL_UPDATE) {
		_ccorr_partial_update(module, params, handle);
		ret = 0;
	}
	return ret;
}
#endif

static int disp_ccorr_io(DISP_MODULE_ENUM module, int msg, unsigned long arg, void *cmdq)
{
	switch (msg) {
	case DISP_IOCTL_SET_CCORR:
		if (disp_ccorr_set_coef((DISP_CCORR_COEF_T *) arg, cmdq) < 0) {
			CCORR_ERR("DISP_IOCTL_SET_CCORR: failed\n");
			return -EFAULT;
		}
		break;
	}

	return 0;
}


static int disp_ccorr_set_listener(DISP_MODULE_ENUM module, ddp_module_notify notify)
{
	g_ccorr_ddp_notify = notify;
	return 0;
}


static int disp_ccorr_bypass(DISP_MODULE_ENUM module, int bypass)
{
	int relay = 0;

	if (bypass) {
		relay = 1;
		DISP_REG_SET(NULL, DISP_REG_CCORR_EN, 0x0);
	}

	CCORR_DBG("disp_ccorr_bypass(bypass = %d)", bypass);

	return 0;
}

static int disp_ccorr_power_on(DISP_MODULE_ENUM module, void *handle)
{
#ifdef ENABLE_CLK_MGR
	if (module == CCORR0_MODULE_NAMING) {
#ifdef CONFIG_MTK_CLKMGR
#if !defined(CONFIG_ARCH_MT6570) && !defined(CONFIG_ARCH_MT6580)
		enable_clock(MT_CG_DISP0_DISP_CCORR, "CCORR");
#endif
#else
		ddp_clk_enable(DISP0_DISP_CCORR);
#endif
	}
#endif
	return 0;
}

static int disp_ccorr_power_off(DISP_MODULE_ENUM module, void *handle)
{
#ifdef ENABLE_CLK_MGR
	if (module == CCORR0_MODULE_NAMING) {
#ifdef CONFIG_MTK_CLKMGR
#if !defined(CONFIG_ARCH_MT6570) && !defined(CONFIG_ARCH_MT6580)
		disable_clock(MT_CG_DISP0_DISP_CCORR, "CCORR");
#endif
#else
		ddp_clk_disable(DISP0_DISP_CCORR);
#endif
	}
#endif
	return 0;
}


DDP_MODULE_DRIVER ddp_driver_ccorr = {
	.config = disp_ccorr_config,
	.start = disp_ccorr_start,
	.bypass = disp_ccorr_bypass,
	.set_listener = disp_ccorr_set_listener,
	.cmd = disp_ccorr_io,
	.init = disp_ccorr_power_on,
	.deinit = disp_ccorr_power_off,
	.power_on = disp_ccorr_power_on,
	.power_off = disp_ccorr_power_off,
#ifdef CCORR_SUPPORT_PARTIAL_UPDATE
	.ioctl = ccorr_ioctl,
#endif
};

int ccorr_coef_interface(unsigned int ccorr_coef_ref[3][3], void *handle)
{
	int y, x;
	DISP_CCORR_COEF_T *ccorr;

	if (g_disp_ccorr_coef[DISP_CCORR0] == NULL) {
		g_disp_ccorr_coef[DISP_CCORR0] = kmalloc(sizeof(DISP_CCORR_COEF_T), GFP_KERNEL);
		if (g_disp_ccorr_coef[DISP_CCORR0] == NULL) {
			CCORR_ERR("disp_ccorr_set_coef: no memory\n");
			return -EFAULT;
		}
		CCORR_DBG("ccorr_interface_for_color:allocate coef buffer");
		ccorr = g_disp_ccorr_coef[DISP_CCORR0];
	} else {
		ccorr = g_disp_ccorr_coef[DISP_CCORR0];
	}

	for (y = 0; y < 3; y += 1)
		for (x = 0; x < 3; x += 1)
			ccorr->coef[y][x] = ccorr_coef_ref[y][x];

	CCORR_DBG("== CCORR Coefficient ==");
	CCORR_DBG("%4d %4d %4d", ccorr->coef[0][0], ccorr->coef[0][1], ccorr->coef[0][2]);
	CCORR_DBG("%4d %4d %4d", ccorr->coef[1][0], ccorr->coef[1][1], ccorr->coef[1][2]);
	CCORR_DBG("%4d %4d %4d", ccorr->coef[2][0], ccorr->coef[2][1], ccorr->coef[2][2]);

	disp_ccorr_write_coef_reg(handle, DISP_CCORR0, 1);

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

static int ccorr_parse_coef(const char *cmd, void *handle)
{
	int i, j, end;
	bool stop = false;
	int count = 0;
	unsigned long temp;
	unsigned int ccorr_coef[3][3];
	char *next = (char *)cmd;

	for (i = 0; i < 3; i++) {
		for (j = 0; j < 3; j++) {
			end = ddp_simple_strtoul(next, (unsigned long *)(&temp));
			next += end;

			ccorr_coef[i][j] = (unsigned int)temp;
			count++;

			CCORR_DBG("ccorr coef(%3d,%3d)=%d\n", i, j, ccorr_coef[i][j]);

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
		ccorr_coef_interface(ccorr_coef, handle);
		CCORR_DBG("ccorr coef config done\n");
	}
	return 0;
}

static int ccorr_parse_triple(const char *cmd, unsigned long *offset, unsigned long *value, unsigned long *mask)
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

	CCORR_DBG("ccorr_test(%s)", cmd);

	debug_output[0] = '\0';

	if (strncmp(cmd, "set:", 4) == 0) {
		int count = ccorr_parse_triple(cmd + 4, &offset, &value, &mask);

		if (count == 3) {
			DISP_REG_MASK(NULL, CCORR0_BASE_NAMING + offset, value, mask);
		} else if (count == 2) {
			DISP_REG_SET(NULL, CCORR0_BASE_NAMING + offset, value);
			mask = 0xffffffff;
		}

		if (count >= 2) {
			CCORR_DBG("[+0x%031lx] = 0x%08lx(%d) & 0x%08lx",
				offset, value, (int)value, mask);
		}

	} else if (strncmp(cmd, "coef:", 5) == 0) {
		ccorr_parse_coef(cmd+5, NULL);

	} else if (strncmp(cmd, "dump", 4) == 0) {
		ccorr_dump_reg();

	} else if (strncmp(cmd, "en:", 3) == 0) {
		int enabled = (cmd[3] == '1' ? 1 : 0);

		if (enabled == 1) {
			DISP_REG_MASK(NULL, CCORR0_BASE_NAMING, 0x1, 0x1);
			DISP_REG_MASK(NULL, CCORR0_BASE_NAMING + 0x20, 0x2, 0x3);
		} else {
			DISP_REG_MASK(NULL, CCORR0_BASE_NAMING, 0x0, 0x1);
			DISP_REG_MASK(NULL, CCORR0_BASE_NAMING + 0x20, 0x1, 0x3);
		}

	} else if (strncmp(cmd, "dbg:", 4) == 0) {
		corr_dbg_en = cmd[4] - '0';
		corr_dbg_en = (corr_dbg_en > 1) ? 1 : corr_dbg_en;
		CCORR_DBG("debug log status:%d", corr_dbg_en);

	} else {

	}
	disp_ccorr_trigger_refresh(DISP_CCORR0);
}

