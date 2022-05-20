// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/iopoll.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>

#include "apusys_power_user.h"
#include "apu_devfreq.h"
#include "apu_io.h"
#include "apu_rpc.h"
#include "apu_log.h"
#include "apu_common.h"
#include "apu_dbg.h"

void __iomem *rpc_reg;
void __iomem *spm_reg;
void __iomem *tclk_reg;
struct mutex rpc_lock;

const int _apu_rpc_engid(enum DVFS_USER user)
{
	static const int ids[] = {
		[VPU0] = RPC_VPU0_WAKE_ID,
		[VPU1] = RPC_VPU1_WAKE_ID,
		[VPU2] = RPC_VPU2_WAKE_ID,
		[MDLA0] = RPC_MDLA0_WAKE_ID,
		[MDLA1] = RPC_MDLA1_WAKE_ID,
		[APUCONN] = RPC_TOP_WAKE_ID,
		[APUCORE] = RPC_TOP_WAKE_ID,
	};

	if (user >= ARRAY_SIZE(ids))
		return -EINVAL;
	return ids[user];
}

static void _dump_topclk_info(struct apu_dev *ad)
{
	print_hex_dump(KERN_ERR, "CLK_CFG_3: ", DUMP_PREFIX_OFFSET,
		       16, 4, (tclk_reg + CLK_CFG_3), 0x10, 1);
	print_hex_dump(KERN_ERR, "CLK_CFG_4: ", DUMP_PREFIX_OFFSET,
		       16, 4, (tclk_reg + CLK_CFG_4), 0x10, 1);
}

static void _dump_spm_status(struct apu_dev *ad)
{
	int val = 0;

	val = readl(spm_reg + SPM_OTHER_PWR_STATUS);
	arpc_warn(ad->dev, "SPM OTHER_PWR_STATUS = 0x%x\n", val);
	val = readl(spm_reg + SPM_BUCK_ISOLATION);
	arpc_warn(ad->dev, "SPM BUCK_ISOLATION = 0x%x\n", val);
	val = readl(spm_reg + SPM_CROSS_WAKE_M01_REQ);
	arpc_warn(ad->dev, "SPM SPM_CROSS_WAKE_M01_REQ = 0x%x\n", val);
}

static int _check_if_rpc_alive(struct apu_dev *ad)
{
	uint val_b = 0, val_a = 0;
	int ret = 0;
	int bit_offset = 26;

	/* #1 [31:26] is dummy register area for testing read/write */
	bit_offset = 26;
	val_a = apu_readl(rpc_reg + RPC_TOP_SEL);

	/* #2 purposely write 0x3a << 26 */
	val_b = val_a | (0x3a << bit_offset);
	apu_writel(val_b, (rpc_reg + RPC_TOP_SEL));

	/* #3 read back TOP_SEL */
	val_b = apu_readl(rpc_reg + RPC_TOP_SEL);

	/* #4 clear bit[31:26] as 0 */
	apu_clearl(0x3F << bit_offset, (rpc_reg + RPC_TOP_SEL));
	ret = ((val_b >> bit_offset) & 0x3f) == 0x3a ? 1 : 0;

	arpc_warn(ad->dev, "[%s] before TOP_SEL 0x%x, after TOP_SEL 0x%x\n",
			  __func__, val_a, val_b);

	return ret;
}


static int _check_rpc_status(struct apu_dev *ad, int engid, bool enable)
{
	int ret = 0, alive = 0;
	uint val = 0;

	/* except top, other engines's ready bit is 1 << engid */
	if (engid != RPC_TOP_WAKE_ID)
		engid = 1 << engid;

	ret = readl_relaxed_poll_timeout_atomic(rpc_reg + RPC_INTF_PWR_RDY,
						val,
						enable ? (val & engid) : !(val & engid),
						POLL_INTERVAL, POLL_TIMEOUT);
	if (ret) {
		alive = _check_if_rpc_alive(ad);
		_dump_topclk_info(ad);
		_dump_spm_status(ad);
		arpc_err(ad->dev, "RPC_RDY timeout, RPC_RDY = 0x%x, alive %s\n",
				 val, alive ? "yes" : "no");
	}

	return ret;
}

static int _rpc_power_switch(struct apu_dev *ad, int engid, bool enable)
{
	int ret = 0;
	uint val = 0, alive = 0;

	ret = readl_relaxed_poll_timeout_atomic(rpc_reg + RPC_TOP_CON, val,
						(val & RPC_FIFO_FULL) == 0,
						POLL_INTERVAL, POLL_TIMEOUT);
	if (ret) {
		alive = _check_if_rpc_alive(ad);
		_dump_topclk_info(ad);
		_dump_spm_status(ad);
		arpc_err(ad->dev, "[RPC fifo full] rpc_top = 0x%x\n", val);
		goto out;
	}

	if (enable)
		apu_writel((engid | 1 << 4), (rpc_reg + RPC_SW_FIFO_WE));
	else
		apu_writel(engid, (rpc_reg + RPC_SW_FIFO_WE));

out:

	return ret;
}


ulong apu_rpc_rdy_value(void)
{
	if (rpc_reg)
		return apu_readl(rpc_reg + RPC_INTF_PWR_RDY);
	pr_info("%s not ready\n", __func__);
	return 0;
}

ulong apu_spm_wakeup_value(void)
{
	if (spm_reg)
		return apu_readl(spm_reg + SPM_CROSS_WAKE_M01_REQ);
	pr_info("%s not ready\n", __func__);
	return 0;
}

void apu_buckiso(struct apu_dev *ad, bool enable)
{
	if (enable) /* enable buck isolation */
		apu_setl(0x21, (spm_reg + SPM_BUCK_ISOLATION));
	else /* release buck isolation */
		apu_clearl(0x21, (spm_reg + SPM_BUCK_ISOLATION));
}

int apu_mtcmos_on(struct apu_dev *ad)
{
	int engid = _apu_rpc_engid(ad->user);
	int ret = 0, val = 0;

	if (engid <= 0)
		return engid;

	mutex_lock(&rpc_lock);
	if (engid == RPC_TOP_WAKE_ID) {
		apu_setl(WAKEUP_APU, (spm_reg + SPM_CROSS_WAKE_M01_REQ));
		ret = readl_relaxed_poll_timeout_atomic((spm_reg + SPM_OTHER_PWR_STATUS),
							val, (val & (0x1UL << 5)),
							POLL_INTERVAL, POLL_TIMEOUT);
		if (ret) {
			_dump_spm_status(ad);
			_dump_topclk_info(ad);
			aspm_err(ad->dev, "%s spm wake up fail, ret %d\n", __func__, ret);
			goto out;
		}
	} else {
		ret = _rpc_power_switch(ad, engid, 1);
		if (ret)
			goto out;
	}
	ret = _check_rpc_status(ad, engid, 1);
out:
	mutex_unlock(&rpc_lock);
	return ret;
}

int apu_mtcmos_off(struct apu_dev *ad)
{
	int engid = _apu_rpc_engid(ad->user);
	int ret = 0, val = 0;

	if (engid <= 0)
		return engid;

	mutex_lock(&rpc_lock);
	if (engid == RPC_TOP_WAKE_ID) {
		apu_clearl(WAKEUP_APU, (spm_reg + SPM_CROSS_WAKE_M01_REQ));
		ret = readl_relaxed_poll_timeout_atomic((spm_reg + SPM_CROSS_WAKE_M01_REQ),
							val, !(val & WAKEUP_APU),
							POLL_INTERVAL, POLL_TIMEOUT);
		if (ret) {
			_dump_spm_status(ad);
			_dump_topclk_info(ad);
			aspm_err(ad->dev, "[%s] spm suspend fail, ret %d\n", __func__, ret);
			goto out;
		}

		/*
		 * sleep request enable
		 * CAUTION!! do NOT request sleep twice in succession
		 * or system may crash (comments from DE)
		 */
		apu_setl(0x1, (rpc_reg + RPC_TOP_CON));
		ret = _check_rpc_status(ad, engid, 0);
		if (ret)
			goto out;
		ret = readl_relaxed_poll_timeout_atomic(spm_reg + SPM_OTHER_PWR_STATUS, val,
							(val & (0x1UL << 5)) == 0x0,
							POLL_INTERVAL, POLL_TIMEOUT);
		if (ret) {
			_dump_spm_status(ad);
			_dump_topclk_info(ad);
			arpc_err(ad->dev, "[%s] sleep request fail, ret %d\n",
					__func__, ret);
			goto out;
		}
	} else {
		ret = _rpc_power_switch(ad, engid, 0);
		if (ret)
			goto out;
	}
	ret = _check_rpc_status(ad, engid, 0);

out:
	mutex_unlock(&rpc_lock);
	return ret;
}

int apu_rpc_init_done(struct apu_dev *ad)
{
	uint val = 0;

	if (!rpc_reg || !spm_reg)
		return -EPROBE_DEFER;

	/*
	 * RPC stay on alway on domain, no need to enable clk src and pmic
	 * while setting memory type to PD or sleep group
	 * sw_type register for each memory group, set to PD mode default
	 */
	apu_writel(0xFF, rpc_reg + RPC_SW_TYPE0); /* APUTOP */
	apu_writel(0x07, rpc_reg + RPC_SW_TYPE2); /* VPU0 */
	apu_writel(0x07, rpc_reg + RPC_SW_TYPE3); /* VPU1 */
	apu_writel(0x07, rpc_reg + RPC_SW_TYPE4); /* VPU2 */
	apu_writel(0x03, rpc_reg + RPC_SW_TYPE6); /* MDLA0 */
	apu_writel(0x03, rpc_reg + RPC_SW_TYPE7); /* MDLA1 */

	/* mask RPC IRQ and bypass WFI */
	val = apu_readl(rpc_reg + RPC_TOP_SEL);
	val |= (0x9E | (1 << 10));
	apu_writel(val, rpc_reg + RPC_TOP_SEL);

	return 0;
}

static int apu_rpc_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	mutex_init(&rpc_lock);
	/* rpc register initialization */
	rpc_reg = of_iomap(pdev->dev.of_node, 0);
	if (IS_ERR_OR_NULL(rpc_reg)) {
		arpc_err(dev, "Unable to iomap rpc\n");
		return -ENODEV;
	}

	tclk_reg = of_iomap(pdev->dev.of_node, 1);
	if (!tclk_reg) {
		arpc_err(dev, "Unable to iomap topclk register\n");
		return -ENODEV;
	}

	spm_reg = of_iomap(pdev->dev.of_node, 2);
	if (!spm_reg) {
		arpc_err(dev, "Unable to iomap spm register\n");
		return -ENODEV;
	}

	return apupw_dbg_register_nodes(dev);
}

static int apu_rpc_remove(struct platform_device *pdev)
{
	dev_info(&pdev->dev, "%s\n", __func__);
	iounmap(spm_reg);
	apupw_dbg_release_nodes();
	return 0;
}

static const struct of_device_id of_match[] = {
	{ .compatible = "mtk68xx,apurpc" },
	{ },
};

MODULE_DEVICE_TABLE(of, of_match);

struct platform_driver apu_rpc_driver = {
	.probe	= apu_rpc_probe,
	.remove	= apu_rpc_remove,
	.driver = {
		.name = "mtk68xx,apurpc",
		.of_match_table = of_match,
	},
};

