// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/completion.h>
#include <linux/of_platform.h>
#include <linux/pm_runtime.h>
#include <linux/sched/clock.h>

#include <linux/delay.h>

#include "apu.h"
#include "apu_config.h"
#include "mdw_export.h"
#include "mtk_apu_rpmsg.h"

#include "apu_hw.h"

#if IS_ENABLED(CONFIG_DEBUG_FS)
int apu_keep_awake;
#endif

enum {
	APUTOP_POWER_OFF = 0,
	APUTOP_POWER_ON = 1,
};

enum {
	DPIDLE_OK = 0,
	DPIDLE_IDLE_PROCEEDING,
	DPIDLE_SUSPEND_PROCEEDING,
	DPIDLE_MDW_LOCK_FAIL,
	DPIDLE_RPM_PUT_FAIL,
	DPIDLE_IPI_LOCK_FAIL,
	DPIDLE_KEEP_AWAKE,
};

struct dpidle_msg {
	uint32_t cmd;
	uint32_t ack;
};

void apu_deepidle_power_on_aputop(struct mtk_apu *apu)
{
	struct device *dev = apu->dev;
	int i;

	if (!pm_runtime_active(apu->dev)) {
		dev_info(apu->dev, "apu boot: pc=%08x, sp=%08x\n",
			ioread32(apu->md32_sysctrl + 0x838),
			ioread32(apu->md32_sysctrl+0x840));

		dev_info(dev, "%s: UP_NORMAL_DOMAIN_NS = 0x%x\n",
			__func__,
			ioread32(apu->apu_sctrl_reviser + UP_NORMAL_DOMAIN_NS));
		dev_info(dev, "%s: UP_PRI_DOMAIN_NS = 0x%x\n",
			__func__,
			ioread32(apu->apu_sctrl_reviser + UP_PRI_DOMAIN_NS));
		dev_info(dev, "%s: USERFW_CTXT = 0x%x\n",
			__func__,
			ioread32(apu->apu_sctrl_reviser + USERFW_CTXT));
		dev_info(dev, "%s: SECUREFW_CTXT = 0x%x\n",
			__func__,
			ioread32(apu->apu_sctrl_reviser + SECUREFW_CTXT));

		dev_info(dev, "%s: MD32_SYS_CTRL = 0x%x\n",
			__func__, ioread32(apu->md32_sysctrl + MD32_SYS_CTRL));

		dev_info(dev, "%s: MD32_CLK_EN = 0x%x\n",
			__func__, ioread32(apu->md32_sysctrl + MD32_CLK_EN));
		dev_info(dev, "%s: UP_WAKE_HOST_MASK0 = 0x%x\n",
			__func__, ioread32(apu->md32_sysctrl + UP_WAKE_HOST_MASK0));

		dev_info(dev, "%s: MD32_BOOT_CTRL = 0x%x\n",
			__func__, ioread32(apu->apu_ao_ctl + MD32_BOOT_CTRL));

		dev_info(dev, "%s: MD32_PRE_DEFINE = 0x%x\n",
			__func__, ioread32(apu->apu_ao_ctl + MD32_PRE_DEFINE));

		dev_info(apu->dev, "%s: power on apu top\n", __func__);
		apu->conf_buf->time_offset = sched_clock();
		pm_runtime_get(apu->dev);

		dev_info(dev, "mbox dummy= 0x%08x 0x%08x 0x%08x 0x%08x\n",
			ioread32(apu->apu_mbox + 0x40),
			ioread32(apu->apu_mbox + 0x44),
			ioread32(apu->apu_mbox + 0x48),
			ioread32(apu->apu_mbox + 0x4c));

		usleep_range(0, 1000);
		for (i = 0; i < 20; i++) {
			dev_info(apu->dev, "apu boot: pc=%08x, sp=%08x\n",
			ioread32(apu->md32_sysctrl + 0x838),
					ioread32(apu->md32_sysctrl+0x840));
			usleep_range(0, 1000);
		}

		dev_info(dev, "%s: UP_NORMAL_DOMAIN_NS = 0x%x\n",
			__func__,
			ioread32(apu->apu_sctrl_reviser + UP_NORMAL_DOMAIN_NS));
		dev_info(dev, "%s: UP_PRI_DOMAIN_NS = 0x%x\n",
			__func__,
			ioread32(apu->apu_sctrl_reviser + UP_PRI_DOMAIN_NS));
		dev_info(dev, "%s: USERFW_CTXT = 0x%x\n",
			__func__,
			ioread32(apu->apu_sctrl_reviser + USERFW_CTXT));
		dev_info(dev, "%s: SECUREFW_CTXT = 0x%x\n",
			__func__,
			ioread32(apu->apu_sctrl_reviser + SECUREFW_CTXT));

		dev_info(dev, "%s: MD32_SYS_CTRL = 0x%x\n",
			__func__, ioread32(apu->md32_sysctrl + MD32_SYS_CTRL));

		dev_info(dev, "%s: MD32_CLK_EN = 0x%x\n",
			__func__, ioread32(apu->md32_sysctrl + MD32_CLK_EN));
		dev_info(dev, "%s: UP_WAKE_HOST_MASK0 = 0x%x\n",
			__func__, ioread32(apu->md32_sysctrl + UP_WAKE_HOST_MASK0));

		dev_info(dev, "%s: MD32_BOOT_CTRL = 0x%x\n",
			__func__, ioread32(apu->apu_ao_ctl + MD32_BOOT_CTRL));

		dev_info(dev, "%s: MD32_PRE_DEFINE = 0x%x\n",
			__func__, ioread32(apu->apu_ao_ctl + MD32_PRE_DEFINE));

	}
}

static int apu_deepidle_send_ack(struct mtk_apu *apu, uint32_t ack)
{
	struct dpidle_msg msg;
	int ret;

	msg.cmd = 0;
	msg.ack = ack;

	ret = apu_ipi_send(apu, APU_IPI_DEEP_IDLE, &msg, sizeof(msg), 0);
	if (ret)
		dev_info(apu->dev,
			 "%s: failed to send ack msg, ack=%d, ret=%d\n",
			 __func__, ack, ret);

	dev_info(apu->dev, "%s: deepidle ack=%d\n", __func__, ack);

	return ret;
}

static void apu_deepidle_ipi_handler(void *data, unsigned int len, void *priv)
{
	struct mtk_apu *apu = (struct mtk_apu *)priv;
	int ret;

	dev_info(apu->dev, "%s: entering deep idle\n", __func__);

#if IS_ENABLED(CONFIG_DEBUG_FS)
	if (apu_keep_awake) {
		dev_info(apu->dev, "keep_awake set (%d), not powering down apu\n",
			 apu_keep_awake);
		apu_deepidle_send_ack(apu, DPIDLE_KEEP_AWAKE);
		return;
	}
#endif

	ret = mdw_dev_lock();
	if (ret && ret != -ENODEV) {
		dev_info(apu->dev, "failed to lock middleware, ret=%d\n", ret);
		apu_deepidle_send_ack(apu, DPIDLE_MDW_LOCK_FAIL);
		return;
	}

	ret = apu_ipi_lock(apu);
	if (ret) {
		dev_info(apu->dev, "failed to lock inbound IPI, ret=%d\n", ret);
		apu_deepidle_send_ack(apu, DPIDLE_IPI_LOCK_FAIL);
		goto unlock_mdw;
	}

	dev_info(apu->dev, "%s: take mdw lock done\n", __func__);
	ret = apu_deepidle_send_ack(apu, DPIDLE_OK);
	if (ret)
		goto unlock_ipi;

	dev_info(apu->dev, "%s: trigger power off\n", __func__);
	ret = pm_runtime_put_sync(apu->dev);
	if (ret) {
		dev_info(apu->dev, "failed to power off apu top, ret=%d\n", ret);
		goto unlock_ipi;
	}

	dev_info(apu->dev, "%s: power off done\n", __func__);

unlock_ipi:
	apu_ipi_unlock(apu);

unlock_mdw:
	mdw_dev_unlock();
}

int apu_deepidle_init(struct mtk_apu *apu)
{
	struct device *dev = apu->dev;
	struct device_node *np;
	struct platform_device *pdev;
	int ret;

	np = of_parse_phandle(dev->of_node, "mediatek,apusys_power", 0);
	if (!np) {
		dev_info(dev, "failed to parse apusys_power node\n");
		return -EINVAL;
	}

	if (!of_device_is_available(np)) {
		dev_info(dev, "unable to find apusys_power node\n");
		of_node_put(np);
		return -ENODEV;
	}

	pdev = of_find_device_by_node(np);
	if (!pdev) {
		dev_info(dev, "apusys_power is not ready yet\n");
		of_node_put(np);
		return -EPROBE_DEFER;
	}

	dev_info(dev, "%s: get power_dev, name=%s\n", __func__, pdev->name);

	apu->power_dev = &pdev->dev;
	of_node_put(np);


	ret = apu_ipi_register(apu, APU_IPI_DEEP_IDLE,
			       apu_deepidle_ipi_handler, apu);
	if (ret) {
		dev_info(dev,
			 "%s: failed to register deepidle ipi, ret=%d\n",
			 __func__, ret);
	}

	return ret;
}

void apu_deepidle_exit(struct mtk_apu *apu)
{
	/* module can be removed only after APU TOP is shutdown properly */

	apu_ipi_unregister(apu, APU_IPI_DEEP_IDLE);
}

