// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 * Author: Yuan Jung Kuo <yuan-jung.kuo@mediatek.com>
 */

#include <linux/io.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/notifier.h>
#include <linux/timekeeping.h>
#include <linux/remoteproc/mtk_ccu.h>
#include <linux/pm_domain.h>
#include <linux/pm_runtime.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <soc/mediatek/mmdvfs_v3.h>

#include <mt-plat/mtk-vmm-notifier.h>


#define ISP_LOGI(fmt, args...) \
	pr_notice("%s(): " fmt "\n", \
		__func__, ##args)
#define ISP_LOGE(fmt, args...) \
	pr_notice("error %s(),%d: " fmt "\n", \
		__func__, __LINE__, ##args)
#define ISP_LOGF(fmt, args...) \
	pr_notice("fatal %s(),%d: " fmt "\n", \
		__func__, __LINE__, ##args)

#define AVS_WAIT_CONFIG_WINDOW_DONE_US 450
/* bits[2]: group 0, bits[1]: age 0, bits[0]: age 1 */
#define AVS_RO_CNT_STATUS_CHECK_MASK 0x6

#define AVS_RO_EN0 0
#define AVS_RO_EN1 0x4
#define AVS_PWR_CTRL_ADDR 0x8
#define AVS_REG_CNT_STATUS_ADDR 0xC
#define AVS_REG_COUNTER0_ADDR 0x10
#define AVS_REG_COUNTER7_ADDR 0x2C
#define AVS_REG_COUNTER8_ADDR 0x30
#define AVS_RO_SEL 0x34
#define AVS_WIN_CYC 0x38
#define AVS_REG_TEST_ADDR 0x3C

#define AVS_UPDATE_PERIOD_MS 86400000

enum POWER_DOMAIN_ID {
	PD_CAM_MAIN,
	PD_ISP_MAIN, /* 1 */
	PD_CAM_VCORE, /* 2 */
	PD_ISP_VCORE, /* 3 */
	PD_VDE0, /* 4 */
	PD_VDE1, /* 5 */
	PD_NUM
};

struct vmm_notifier_data {
	struct notifier_block notifier;
	u32 pd_id;
	struct clk *clk_avs;
	const char *avs_name;
	void __iomem *base;
	unsigned long timestamp;
};

static struct vmm_notifier_data global_data[PD_NUM];

/* VDE user coount */
struct mutex vde_mutex;
static int vde_user_counter;

/* Camera user count */
struct mutex ctrl_mutex;
static int vmm_genpd_user_counter;
static int vmm_user_counter;
static int vmm_locked_isp_open(bool genpd_update)
{
	if (genpd_update) {
		vmm_genpd_user_counter++;
		if (vmm_genpd_user_counter == 1)
			mtk_mmdvfs_camera_notify(true, true);
	}

	vmm_user_counter++;
	if (vmm_user_counter == 1)
		mtk_mmdvfs_camera_notify(false, true);

	return 0;
}

static int vmm_locked_isp_close(bool genpd_update)
{
	if (genpd_update) {
		if (vmm_genpd_user_counter == 0)
			return 0;
		vmm_genpd_user_counter--;
		if (vmm_genpd_user_counter == 0)
			mtk_mmdvfs_camera_notify(true, false);
	}

	/* no need to counter down at probe stage */
	if (vmm_user_counter == 0)
		return 0;
	vmm_user_counter--;
	if (vmm_user_counter == 0)
		mtk_mmdvfs_camera_notify(false, false);

	return 0;
}

static int vmm_locked_vde_open(void)
{
	vde_user_counter++;
	if (vde_user_counter == 1)
		mtk_mmdvfs_vdec_notify(1);

	return 0;
}

static int vmm_locked_vde_close(void)
{
	/* no need to counter down at probe stage */
	if (vde_user_counter == 0)
		return 0;

	vde_user_counter--;
	if (vde_user_counter == 0)
		mtk_mmdvfs_vdec_notify(0);

	return 0;
}

#ifdef AVS_RO_READY
static int mtk_avs_detect(struct vmm_notifier_data *data)
{
	int ret = 0;
	u32 aging_cnt = 0, fresh_cnt = 0;
	unsigned long curr;

	curr = jiffies;
	if (data->timestamp /* Make sure we have updated */
		&& ((curr - data->timestamp) < AVS_UPDATE_PERIOD_MS))
		goto out;

	/* Enable SUBSYS clock before power on avs macro's mtcmos */
	ret = clk_prepare_enable(data->clk_avs);
	if (ret) {
		ISP_LOGE("avs clock(%s) enable fail\n", data->avs_name);
		goto out;
	}

	/* AVS sw release */
	writel_relaxed(0xC7, data->base + AVS_REG_TEST_ADDR);
	writel_relaxed(0x1C7, data->base + AVS_REG_TEST_ADDR);

	/* Power on */
	writel_relaxed(0x15, data->base + AVS_PWR_CTRL_ADDR);
	writel_relaxed(0x1F, data->base + AVS_PWR_CTRL_ADDR);
	writel_relaxed(0x5F, data->base + AVS_PWR_CTRL_ADDR);
	writel_relaxed(0x4F, data->base + AVS_PWR_CTRL_ADDR);
	writel_relaxed(0x6F, data->base + AVS_PWR_CTRL_ADDR);

	/* Enable active age0 (number: #52)*/
	writel_relaxed((0x1 << 20), data->base + AVS_RO_EN1);

	/* Enable group0 (number: #2)*/
	writel_relaxed((0x1 << 2), data->base + AVS_RO_EN0);
	writel_relaxed(0x92492, data->base + AVS_RO_SEL);

	/* Config timing window */
	writel_relaxed(0x22A00, data->base + AVS_WIN_CYC);
	writel_relaxed(0x12A00, data->base + AVS_WIN_CYC);
	udelay(AVS_WAIT_CONFIG_WINDOW_DONE_US);
	writel_relaxed(0x2A00, data->base + AVS_WIN_CYC);

	if (readl_relaxed(data->base + AVS_REG_CNT_STATUS_ADDR)
			!= AVS_RO_CNT_STATUS_CHECK_MASK) {
		ISP_LOGE("Reg cnt status not correct(0x%x)\n",
				readl_relaxed(data->base + AVS_REG_CNT_STATUS_ADDR));
		//ret = -EINVAL;
		goto power_off;
	}

	aging_cnt = readl_relaxed(data->base + AVS_REG_COUNTER7_ADDR);
	/* RO number: #2 which resides in mtcmos */
	fresh_cnt = readl_relaxed(data->base + AVS_REG_COUNTER0_ADDR);
	ret = mtk_mmdvfs_set_avs((u16)data->pd_id, aging_cnt, fresh_cnt);
	if (!ret)
		data->timestamp = curr;

	ISP_LOGI("SUBSYS(%s) age(0x%x) fresh(0x%x)\n",
			data->avs_name,
			aging_cnt,
			fresh_cnt);

power_off:
	writel_relaxed(0, data->base + AVS_RO_EN1);
	writel_relaxed(0, data->base + AVS_RO_EN0);

	/* Power off */
	writel_relaxed(0x6F, data->base + AVS_PWR_CTRL_ADDR);
	writel_relaxed(0x7F, data->base + AVS_PWR_CTRL_ADDR);
	writel_relaxed(0x3F, data->base + AVS_PWR_CTRL_ADDR);
	writel_relaxed(0x1F, data->base + AVS_PWR_CTRL_ADDR);
	writel_relaxed(0x1E, data->base + AVS_PWR_CTRL_ADDR);
	writel_relaxed(0x1C, data->base + AVS_PWR_CTRL_ADDR);

	/* Disable SUBSYS clock after power off avs macro's mtcmos */
	clk_disable_unprepare(data->clk_avs);
out:
	return ret;
}
#endif

static int mtk_camera_pd_callback(struct notifier_block *nb,
		unsigned long flags, void *data)
{
	int ret = 0;
	struct vmm_notifier_data *priv;

	priv = container_of(nb, struct vmm_notifier_data, notifier);
	switch (priv->pd_id) {
	#ifdef AVS_RO_READY
	case PD_CAM_MAIN:
	case PD_ISP_MAIN:
		if (flags == GENPD_NOTIFY_ON)
			return mtk_avs_detect(priv);
		break;
	#endif
	case PD_CAM_VCORE:
	case PD_ISP_VCORE:
		mutex_lock(&ctrl_mutex);

		if (flags == GENPD_NOTIFY_PRE_ON)
			ret = vmm_locked_isp_open(true);
		else if (flags == GENPD_NOTIFY_OFF)
			ret = vmm_locked_isp_close(true);

		mutex_unlock(&ctrl_mutex);
		break;
	case PD_VDE0:
	case PD_VDE1:
		mutex_lock(&vde_mutex);

		if (flags == GENPD_NOTIFY_PRE_ON)
			ret = vmm_locked_vde_open();
		else if (flags == GENPD_NOTIFY_OFF)
			ret = vmm_locked_vde_close();

		mutex_unlock(&vde_mutex);
		break;
	default:
	#ifdef AVS_RO_READY
		ISP_LOGE("invalid pd_id(%u)", priv->pd_id);
	#endif
		break;
	}

	return ret;
}

int vmm_isp_ctrl_notify(int openIsp)
{
	int ret;

	mutex_lock(&ctrl_mutex);

	if (openIsp)
		ret = vmm_locked_isp_open(false);
	else
		ret = vmm_locked_isp_close(false);

	mutex_unlock(&ctrl_mutex);

	return ret;
}
EXPORT_SYMBOL_GPL(vmm_isp_ctrl_notify);

static int vmm_notifier_probe(struct platform_device *pdev)
{
	int ret;
	struct device *dev = &pdev->dev;
	u32 pd_id;
	const char *reg_name;
	struct vmm_notifier_data *data;
	struct resource *res;

	mutex_init(&ctrl_mutex);

	ret = of_property_read_u32(dev->of_node, "pd-id", &pd_id);
	if (ret) {
		ISP_LOGE("property read fail(%d)", ret);
		return -ENODEV;
	}

	if (pd_id >= PD_NUM) {
		ISP_LOGE("invalid pd_id in dts(%u)", pd_id);
		return -ENODEV;
	}
	data = &global_data[pd_id];

	if (pd_id == PD_CAM_MAIN || pd_id == PD_ISP_MAIN) {
		/* Get avs base register address */
		ret = of_property_read_string(dev->of_node, "reg-names", &reg_name);
		if (ret) {
			ISP_LOGE("get register name fail");
			return -ENODEV;
		}
		res = platform_get_resource_byname(pdev, IORESOURCE_MEM, reg_name);
		if (!res) {
			ISP_LOGE("get platform resource fail(%s)", reg_name);
			return -ENODEV;
		}

		ret = of_property_read_string(dev->of_node,
				"clock-names", &data->avs_name);
		data->clk_avs = devm_clk_get(dev, data->avs_name);
		if (IS_ERR(data->clk_avs)) {
			ISP_LOGE("Get clk:%s fail", data->avs_name);
			return -ENODEV;
		}

		data->base = ioremap(res->start, resource_size(res));
		if (IS_ERR(data->base)) {
			ISP_LOGE("ioremap fail");
			return PTR_ERR(data->base);
		}
	}

	pm_runtime_enable(dev);
	data->notifier.notifier_call = mtk_camera_pd_callback;
	data->pd_id = pd_id;
	ret = dev_pm_genpd_add_notifier(dev, &data->notifier);
	if (ret)
		ISP_LOGE("gen pd add notifier fail(%d)", ret);

	return 0;
}

static const struct of_device_id of_vmm_notifier_match_tbl[] = {
	{
		.compatible = "mediatek,vmm_notifier",
	},
	{}
};

static struct platform_driver drv_vmm_notifier = {
	.probe = vmm_notifier_probe,
	.driver = {
		.name = "mtk-vmm-notifier",
		.of_match_table = of_vmm_notifier_match_tbl,
	},
};

static int __init mtk_vmm_notifier_init(void)
{
	s32 status;

	status = platform_driver_register(&drv_vmm_notifier);
	if (status) {
		pr_notice("Failed to register VMM dbg driver(%d)\n", status);
		return -ENODEV;
	}

	return 0;
}

static void __exit mtk_vmm_notifier_exit(void)
{
	platform_driver_unregister(&drv_vmm_notifier);
}

module_init(mtk_vmm_notifier_init);
module_exit(mtk_vmm_notifier_exit);
MODULE_DESCRIPTION("MTK VMM notifier driver");
MODULE_AUTHOR("Yuan Jung Kuo <yuan-jung.kuo@mediatek.com>");
MODULE_LICENSE("GPL v2");
