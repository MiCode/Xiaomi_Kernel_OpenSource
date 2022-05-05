// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Anthony Huang <anthony.huang@mediatek.com>
 */

#include <linux/clk.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/pm_domain.h>
#include <linux/pm_runtime.h>
#include <linux/scmi_protocol.h>
#include <linux/slab.h>
#include "cmdq-util.h"
#include "mtk-smi-dbg.h"
#include "tinysys-scmi.h"
#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
#include <mt-plat/aee.h>
#endif
#include <soc/mediatek/smi.h>

#if IS_ENABLED(CONFIG_MTK_DEVAPC)
#include <linux/soc/mediatek/devapc_public.h>
#endif

#define MMINFRA_MAX_CLK_NUM	(4)

struct mminfra_dbg {
	void __iomem *ctrl_base;
	void __iomem *mminfra_base;
	ssize_t ctrl_size;
	struct device *comm_dev;
	struct notifier_block nb;
};

static struct notifier_block mtk_pd_notifier;
static struct scmi_tinysys_info_st *tinfo;
static int feature_id;
static struct clk *mminfra_clk[MMINFRA_MAX_CLK_NUM];
static atomic_t clk_ref_cnt = ATOMIC_INIT(0);
static struct device *dev;
static struct mminfra_dbg *dbg;

#define MMINFRA_BASE		0x1e800000

#define MMINFRA_CG_CON0		0x100
#define GCED_CG_BIT			BIT(0)
#define GCEM_CG_BIT			BIT(1)
#define SMI_CG_BIT			BIT(2)

#define MMINFRA_CG_CON1		0x110
#define GCE26M_CG_BIT		BIT(17)

#define GCED				0
#define GCEM				1

static bool mminfra_check_scmi_status(void)
{
	if (tinfo)
		return true;

	tinfo = get_scmi_tinysys_info();

	if (IS_ERR_OR_NULL(tinfo)) {
		pr_notice("%s: tinfo is wrong!!\n", __func__);
		tinfo = NULL;
		return false;
	}

	if (IS_ERR_OR_NULL(tinfo->ph)) {
		pr_notice("%s: tinfo->ph is wrong!!\n", __func__);
		tinfo = NULL;
		return false;
	}

	of_property_read_u32(tinfo->sdev->dev.of_node, "scmi_mminfra", &feature_id);
	pr_notice("%s: get scmi_smi succeed id=%d!!\n", __func__, feature_id);
	return true;
}

static void do_mminfra_bkrs(bool is_restore)
{
	int err;

	if (mminfra_check_scmi_status()) {
		err = scmi_tinysys_common_set(tinfo->ph, feature_id,
				2, (is_restore)?0:1, 0, 0, 0);
		if (err)
			pr_notice("%s: call scmi_tinysys_common_set(%d) err=%d\n",
				__func__, is_restore, err);
	}
}

static void mminfra_clk_set(bool is_enable)
{
	int err = 0;
	int i, j;

	if (is_enable) {
		for (i = 0; i < MMINFRA_MAX_CLK_NUM; i++) {
			if (mminfra_clk[i])
				err = clk_prepare_enable(mminfra_clk[i]);
			else
				break;

			if (err) {
				pr_notice("mminfra clk(%d) enable fail:%d\n", i, err);
				for (j = i - 1; j >= 0; j--)
					clk_disable_unprepare(mminfra_clk[j]);
				return;
			}
		}
	} else {
		for (i = MMINFRA_MAX_CLK_NUM - 1; i >= 0; i--) {
			if (mminfra_clk[i])
				clk_disable_unprepare(mminfra_clk[i]);
			else
				break;
		}
	}
}

static bool is_mminfra_power_on(void)
{
	return (atomic_read(&clk_ref_cnt) > 0);
}

static bool is_gce_cg_on(u32 hw_id)
{
	u32 con0_val;

	con0_val = readl_relaxed(dbg->mminfra_base + MMINFRA_CG_CON0);

	if (con0_val & (hw_id == GCED ? GCED_CG_BIT : GCEM_CG_BIT))
		return false;

	return true;
}

static void mminfra_cg_check(bool on)
{
	u32 con0_val;
	u32 con1_val;

	con0_val = readl_relaxed(dbg->mminfra_base + MMINFRA_CG_CON0);
	con1_val = readl_relaxed(dbg->mminfra_base + MMINFRA_CG_CON1);

	if (on) {
		/* SMI CG still off */
		if ((con0_val & (SMI_CG_BIT)) || (con0_val & GCEM_CG_BIT) ||
			(con0_val & GCED_CG_BIT) || (con1_val & GCE26M_CG_BIT)) {
			pr_notice("%s cg still off, CG_CON0:0x%x CG_CON1:0x%x\n",
						__func__, con0_val, con1_val);
			mtk_smi_dbg_cg_status();
			cmdq_dump_usage();
		}
	} else {
		/* SMI CG still on */
		if (!(con0_val & (SMI_CG_BIT)) || !(con0_val & GCEM_CG_BIT)
			|| !(con0_val & GCED_CG_BIT) || !(con1_val & GCE26M_CG_BIT)) {
			pr_notice("%s Scg still on, CG_CON0:0x%x CG_CON1:0x%x\n",
						__func__, con0_val, con1_val);
			mtk_smi_dbg_cg_status();
			cmdq_dump_usage();
		}
	}
}


static int mtk_mminfra_pd_callback(struct notifier_block *nb,
			unsigned long flags, void *data)
{
	int count;
	void __iomem *test_base;
	u32 val;

	if (flags == GENPD_NOTIFY_ON) {
		mminfra_clk_set(true);
		mminfra_cg_check(true);
		count = atomic_inc_return(&clk_ref_cnt);
		cmdq_util_mminfra_cmd(0);
		cmdq_util_mminfra_cmd(3); //mminfra rfifo init
		do_mminfra_bkrs(true);
		test_base = ioremap(0x1e800280, 4);
		val = readl_relaxed(test_base);
		if (val != 0xfff) {
			pr_notice("%s: HRE restore failed 0x1e800280=%x\n",
				__func__, val);
#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
			aee_kernel_warning("mminfra",
				"HRE restore failed 0x1e800280=%x\n", val);
#endif
			BUG_ON(1);
		}
		iounmap(test_base);
		pr_notice("%s: enable clk ref_cnt=%d\n", __func__, count);
	} else if (flags == GENPD_NOTIFY_PRE_OFF) {
		do_mminfra_bkrs(false);
		count = atomic_read(&clk_ref_cnt);
		if (count != 1) {
			pr_notice("%s: wrong clk ref_cnt=%d in PRE_OFF\n",
				__func__, count);
			return NOTIFY_OK;
		}
		mminfra_clk_set(false);
		mminfra_cg_check(false);
		count = atomic_dec_return(&clk_ref_cnt);
		pr_notice("%s: disable clk ref_cnt=%d\n", __func__, count);
	}

	return NOTIFY_OK;
}

int mminfra_scmi_test(const char *val, const struct kernel_param *kp)
{
	int ret, arg0;
	unsigned int test_case;
	void __iomem *test_base = ioremap(0x1e800280, 4);

	ret = sscanf(val, "%u %d", &test_case, &arg0);
	if (ret != 2) {
		pr_notice("%s: invalid input: %s, result(%d)\n", __func__, val, ret);
		return -EINVAL;
	}
	if (mminfra_check_scmi_status()) {
		if (test_case == 2 && arg0 == 0) {
			writel(0x123, test_base);
			pr_notice("%s: before BKRS read 0x1e800280 = 0x%x\n",
				__func__, readl_relaxed(test_base));
		}
		pr_notice("%s: feature_id=%d test_case=%d arg0=%d\n",
			__func__, feature_id, test_case, arg0);
		ret = scmi_tinysys_common_set(tinfo->ph, feature_id, test_case, arg0, 0, 0, 0);
		pr_notice("%s: scmi return %d\n", __func__, ret);
		if (test_case == 2 && arg0 == 1)
			pr_notice("%s after BKRS read 0x1e800280 = 0x%x\n",
				__func__, readl_relaxed(test_base));
	}

	iounmap(test_base);

	return 0;
}

static struct kernel_param_ops scmi_test_ops = {
	.set = mminfra_scmi_test,
};
module_param_cb(scmi_test, &scmi_test_ops, NULL, 0644);
MODULE_PARM_DESC(scmi_test, "scmi test");


int mminfra_ut(const char *val, const struct kernel_param *kp)
{
	int ret, arg0;
	unsigned int test_case, value;
	void __iomem *test_base;

	ret = sscanf(val, "%u %i", &test_case, &arg0);
	if (ret != 2) {
		pr_notice("%s: invalid input: %s, result(%d)\n", __func__, val, ret);
		return -EINVAL;
	}
	pr_notice("%s: input: %s\n", __func__, val);
	switch (test_case) {
	case 0:
		ret = pm_runtime_get_sync(dev);
		test_base = ioremap(arg0, 4);
		value = readl_relaxed(test_base);
		do_mminfra_bkrs(false); // backup
		writel(0x123, test_base);
		do_mminfra_bkrs(true); // restore
		if (value == readl_relaxed(test_base))
			pr_notice("%s: test_case(%d) pass\n",
				__func__, test_case);
		else
			pr_notice("%s: test_case(%d) fail value=%d\n",
				__func__, test_case, value);
		iounmap(test_base);
		pm_runtime_put_sync(dev);
		break;
	default:
		pr_notice("%s: wrong test_case(%d)\n", __func__, test_case);
		break;
	}

	return 0;
}

static struct kernel_param_ops mminfra_ut_ops = {
	.set = mminfra_ut,
};
module_param_cb(mminfra_ut, &mminfra_ut_ops, NULL, 0644);
MODULE_PARM_DESC(mminfra_ut, "mminfra ut");


int mtk_mminfra_dbg_hang_detect(const char *user)
{
	s32 offset, ret, len = 0;
	u32 val;
	char buf[LINK_MAX + 1] = {0};

	if (!dev || !dbg || !dbg->comm_dev)
		return -ENODEV;

	pr_info("%s: check caller:%s\n", __func__, user);
	ret = pm_runtime_get_if_in_use(dbg->comm_dev);
	if (ret <= 0) {
		pr_notice("%s: mminfra is power off(%d)\n", __func__, ret);
		return -EFAULT;
	}

	for (offset = 0; offset <= dbg->ctrl_size; offset += 4) {
		val = readl_relaxed(dbg->ctrl_base + offset);
		ret = snprintf(buf + len, LINK_MAX - len, " %#x=%#x,",
			offset, val);
		if (ret < 0 || ret >= LINK_MAX - len) {
			ret = snprintf(buf + len, LINK_MAX - len, "%c", '\0');
			if (ret < 0) {
				pr_notice("%s: snprintf failed:%d\n", __func__, ret);
				continue;
			}
			dev_info(dev, "%s\n", buf);

			len = 0;
			memset(buf, '\0', sizeof(char) * ARRAY_SIZE(buf));
			ret = snprintf(buf + len, LINK_MAX - len, " %#x=%#x,",
				offset, val);
			if (ret < 0) {
				pr_notice("%s: snprintf failed:%d\n", __func__, ret);
				continue;
			}
		}
		len += ret;
	}
	ret = snprintf(buf + len, LINK_MAX - len, "%c", '\0');
	if (ret < 0)
		pr_notice("%s: snprintf failed:%d\n", __func__, ret);
	dev_info(dev, "%s\n", buf);

	pm_runtime_put(dbg->comm_dev);
	return 0;
}

static int mminfra_smi_dbg_cb(struct notifier_block *nb,
		unsigned long value, void *v)
{
	mtk_mminfra_dbg_hang_detect("smi_dbg");
	return 0;
}

static bool aee_dump;
static irqreturn_t mminfra_irq_handler(int irq, void *data)
{
	//char buf[LINK_MAX + 1] = {0};

	pr_notice("handle mminfra irq!\n");
	if (!dev || !dbg || !dbg->comm_dev)
		return IRQ_NONE;

	cmdq_util_mminfra_cmd(1);

	if (!aee_dump) {
#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
		aee_kernel_warning("mminfra", "MMInfra bus timeout\n");
#endif
		aee_dump = true;
	}

	cmdq_util_mminfra_cmd(0);

	return IRQ_HANDLED;
}

#if IS_ENABLED(CONFIG_MTK_DEVAPC)
static bool mminfra_devapc_power_cb(void)
{
	return is_mminfra_power_on();
}

static struct devapc_power_callbacks devapc_power_handle = {
	.type = DEVAPC_TYPE_MMINFRA,
	.query_power = mminfra_devapc_power_cb,
};
#endif

static int mminfra_debug_probe(struct platform_device *pdev)
{
	struct device_node *node;
	struct platform_device *comm_pdev;
	struct property *prop;
	struct resource *res;
	const char *name;
	struct clk *clk;
	u32 mminfra_bkrs = 0, comm_id;
	int ret = 0, i = 0, irq;

	dbg = kzalloc(sizeof(*dbg), GFP_KERNEL);
	if (!dbg)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_notice(&pdev->dev, "could not get resource for ctrl\n");
		return -EINVAL;
	}

	dbg->ctrl_size = resource_size(res);
	dbg->ctrl_base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(dbg->ctrl_base)) {
		dev_notice(&pdev->dev, "could not ioremap resource for ctrl\n");
		return PTR_ERR(dbg->ctrl_base);
	}

	for_each_compatible_node(node, NULL, "mediatek,smi-common") {
		if (!node || of_property_read_u32(node, "mediatek,common-id", &comm_id))
			continue;

		if (comm_id == 0) {
			comm_pdev = of_find_device_by_node(node);
			of_node_put(node);
			if (!comm_pdev)
				return -EINVAL;
			dbg->comm_dev = &comm_pdev->dev;
			break;
		}
	}
	dbg->nb.notifier_call = mminfra_smi_dbg_cb;
	mtk_smi_dbg_register_notifier(&dbg->nb);

	node = pdev->dev.of_node;
	of_property_read_u32(node, "mminfra-bkrs", &mminfra_bkrs);

	mminfra_check_scmi_status();

	dev = &pdev->dev;
	pm_runtime_enable(dev);

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_notice(&pdev->dev, "failed to get irq (%d)\n", irq);
	} else {
		ret = devm_request_irq(&pdev->dev, irq, mminfra_irq_handler, IRQF_SHARED,
				"mtk_mminfra_debug", dbg);
		if (ret) {
			dev_notice(&pdev->dev,
				"failed to register ISR %d (%d)", irq, ret);
			return ret;
		}
		cmdq_util_mminfra_cmd(0);
		cmdq_util_mminfra_cmd(3); //mminfra rfifo init
	}

	dbg->mminfra_base = ioremap(MMINFRA_BASE, 0x8f4);

	cmdq_get_mminfra_cb(is_mminfra_power_on);
	cmdq_get_mminfra_gce_cg_cb(is_gce_cg_on);

	if (mminfra_bkrs == 1) {
		mtk_pd_notifier.notifier_call = mtk_mminfra_pd_callback;
		ret = dev_pm_genpd_add_notifier(dev, &mtk_pd_notifier);

		of_property_for_each_string(node, "clock-names", prop, name) {
			clk = devm_clk_get(dev, name);
			if (IS_ERR(clk)) {
				dev_notice(dev, "%s: clks of %s init failed\n",
					__func__, name);
				ret = PTR_ERR(clk);
				break;
			}
			if (i == MMINFRA_MAX_CLK_NUM) {
				dev_notice(dev, "%s: clk num is wrong\n", __func__);
				ret = -EINVAL;
				break;
			}
			mminfra_clk[i++] = clk;
		}
		if (of_property_read_bool(node, "init-clk-on")) {
			atomic_inc(&clk_ref_cnt);
			mminfra_clk_set(true);
			pr_notice("%s: init-clk-on enable clk\n", __func__);
		}
	}

#if IS_ENABLED(CONFIG_MTK_DEVAPC)
	register_devapc_power_callback(&devapc_power_handle);
#endif

	return ret;
}

static int mminfra_pm_suspend(struct device *dev)
{
	mtk_smi_dbg_cg_status();
	return 0;
}

static const struct dev_pm_ops mminfra_debug_pm_ops = {
	.suspend = mminfra_pm_suspend,
};

static const struct of_device_id of_mminfra_debug_match_tbl[] = {
	{
		.compatible = "mediatek,mminfra-debug",
	},
	{}
};

static struct platform_driver mminfra_debug_drv = {
	.probe = mminfra_debug_probe,
	.driver = {
		.name = "mtk-mminfra-debug",
		.of_match_table = of_mminfra_debug_match_tbl,
		.pm = &mminfra_debug_pm_ops,
	},
};

static int __init mtk_mminfra_debug_init(void)
{
	s32 status;

	status = platform_driver_register(&mminfra_debug_drv);
	if (status) {
		pr_notice("Failed to register MMInfra debug driver(%d)\n", status);
		return -ENODEV;
	}
	return 0;
}

static void __exit mtk_mminfra_debug_exit(void)
{
	platform_driver_unregister(&mminfra_debug_drv);
}

module_init(mtk_mminfra_debug_init);
module_exit(mtk_mminfra_debug_exit);
MODULE_DESCRIPTION("MTK MMInfra Debug driver");
MODULE_AUTHOR("Anthony Huang<anthony.huang@mediatek.com>");
MODULE_LICENSE("GPL v2");
