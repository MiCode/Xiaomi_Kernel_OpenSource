/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */
#ifndef __APU_TOP_H__
#define __APU_TOP_H__

#include <linux/clk.h>
#include <linux/device.h>
#if IS_ENABLED(CONFIG_DEBUG_FS)
#include <linux/debugfs.h>
#endif
#include <linux/io.h>
#include <linux/platform_device.h>
#if IS_ENABLED(CONFIG_PM_SLEEP)
#include <linux/pm_wakeup.h>
#endif
#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
#include <mt-plat/aee.h>
#endif
#include "apu_top_entry.h"

extern const struct apupwr_plat_data *pwr_data;

enum aputop_func_id {
	APUTOP_FUNC_PWR_OFF = 0,
	APUTOP_FUNC_PWR_ON,
	APUTOP_FUNC_OPP_LIMIT_HAL,
	APUTOP_FUNC_OPP_LIMIT_DBG,
	APUTOP_FUNC_DUMP_REG,
	APUTOP_FUNC_DRV_CFG,
	APUTOP_FUNC_IPI_TEST,	// test for wakeup apu through ipi flow
	APUTOP_FUNC_ARE_DUMP1,
	APUTOP_FUNC_ARE_DUMP2,
	APUTOP_FUNC_DBG_VSRAM_VB,
	APUTOP_FUNC_MAX_ID,
};

struct aputop_func_param {
	enum aputop_func_id func_id; //param0
	int param1;
	int param2;
	int param3;
	int param4;
};

struct apupwr_plat_data {
	const char *plat_name;
	int ( *plat_aputop_on)(struct device *dev);
	int ( *plat_aputop_off)(struct device *dev);
	int ( *plat_aputop_pb)(struct platform_device *pdev);
	int ( *plat_aputop_rm)(struct platform_device *pdev);
	int (*plat_aputop_suspend)(struct device *dev);
	int (*plat_aputop_resume)(struct device *dev);
	int ( *plat_aputop_func)(struct platform_device *pdev,
			enum aputop_func_id func_id,
			struct aputop_func_param *aputop);
#if IS_ENABLED(CONFIG_DEBUG_FS)
	int ( *plat_aputop_dbg_open)(struct inode *inode,
			struct file *file);
	ssize_t ( *plat_aputop_dbg_write)(struct file *flip,
			const char __user *buffer,
			size_t count, loff_t *f_pos);
#endif
	int (*plat_rpmsg_callback)(int cmd, void *data, int len,
			void *priv, u32 src);
	int bypass_pwr_on;
	int bypass_pwr_off;
};

struct apupwr_dbg {
#if IS_ENABLED(CONFIG_DEBUG_FS)
	/* below used for debugfs */
	struct dentry *file;
#endif
};

extern int fpga_type;
extern const struct apupwr_plat_data mt6983_plat_data;
extern const struct apupwr_plat_data mt6879_plat_data;
extern const struct apupwr_plat_data mt6895_plat_data;

static inline void apu_writel(const unsigned int val,
		void __force __iomem *regs)
{
	writel(val, regs);
	/* make sure all the write instructions are done */
	wmb();
}

static inline u32 apu_readl(void __force __iomem *regs)
{
	return readl(regs);
}

static inline void apu_setl(const unsigned int val,
		void __force __iomem *regs)
{
	apu_writel((readl(regs) | val), regs);
}

static inline void apu_clearl(const unsigned int val,
		void __force __iomem *regs)
{
	apu_writel((readl(regs) & ~val), regs);
}

/*
 * BIT Operation
 */
#undef  BIT
#define BIT(_bit_) (unsigned int)(1 << (_bit_))
#define BITS(_bits_, _val_) ((((unsigned int) -1 >> (31 - ((1) ? _bits_))) \
& ~((1U << ((0) ? _bits_)) - 1)) & ((_val_)<<((0) ? _bits_)))
#define BITMASK(_bits_) (((unsigned int) -1 >> (31 - ((1) ? _bits_))) \
& ~((1U << ((0) ? _bits_)) - 1))
#define GET_BITS_VAL(_bits_, _val_) (((_val_) & \
(BITMASK(_bits_))) >> ((0) ? _bits_))

/*
 * Clock Operation
 */
#define PREPARE_CLK(clk) \
	{ \
		clk = devm_clk_get(&pdev->dev, #clk); \
		if (IS_ERR(clk)) { \
			ret = -ENOENT; \
			pr_notice("can not find clk: %s\n", #clk); \
		} \
		ret_clk |= ret; \
	}

#define UNPREPARE_CLK(clk) \
	{ \
		if (clk != NULL) \
			clk = NULL; \
	}

#define ENABLE_CLK(clk) \
	{ \
		ret = clk_prepare_enable(clk); \
		if (ret) { \
			pr_notice("fail to prepare & enable clk:%s\n", #clk); \
			clk_disable_unprepare(clk); \
		} \
		ret_clk |= ret; \
}

#define DISABLE_CLK(clk) \
	{ \
		clk_disable_unprepare(clk); \
}

#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
#define apupw_aee_warn(module, reason) \
	do { \
		char mod_name[150];\
		if (snprintf(mod_name, 150, "%s_%s", reason, module) > 0) { \
			dev_info(dev, "%s: %s\n", reason, module); \
			aee_kernel_exception(mod_name, \
					"\nCRDISPATCH_KEY:%s\n", module); \
		} else { \
			dev_info(dev, "%s: snprintf fail(%d)\n", __func__, __LINE__); \
		} \
	} while (0)
#else
#define apupw_aee_warn(module, reason)
#endif

#endif
