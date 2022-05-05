// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/device.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/of_device.h>
#include <linux/kthread.h>
#include <linux/io.h>

#include "mtk_qos_ipi.h"
#include "mtk_qos_sram.h"
#include "mtk_qos_bound.h"
#include "mtk_qos_sysfs.h"
#include "mtk_qos_share.h"
#include "mtk_qos_common.h"

struct mtk_qos *m_qos;
static void __iomem *qos_sram_base;
static unsigned int qos_sram_bound;

unsigned int mtk_qos_enable = 1;

unsigned int is_mtk_qos_enable(void)
{
	return mtk_qos_enable;
}

int qos_get_ipi_cmd(int idx)
{
	int cmd;

	if (!m_qos)
		return -EACCES;
	if (m_qos->soc->ipi_pin[idx].valid == false)
		return -EPERM;

	cmd = m_qos->soc->ipi_pin[idx].id;

	return cmd;
}
EXPORT_SYMBOL_GPL(qos_get_ipi_cmd);

u32 qos_sram_read(u32 id)
{
	u32 offset;

	if (!m_qos)
		return 0;
	if (m_qos->soc->sram_pin[id].valid == false)
		return 0;
	if (id >= QOS_SRAM_ID_MAX)
		return 0;

	offset = m_qos->soc->sram_pin[id].offset;
	if (!qos_sram_base || offset >= qos_sram_bound)
		return 0;

	return readl(qos_sram_base + offset);
}
EXPORT_SYMBOL_GPL(qos_sram_read);

void qos_sram_write(u32 id, u32 val)
{
	u32 offset;

	if (!m_qos)
		return;
	if (m_qos->soc->sram_pin[id].valid == false)
		return;
	if (id >= QOS_SRAM_ID_MAX)
		return;

	offset = m_qos->soc->sram_pin[id].offset;
	if (!qos_sram_base || offset >= qos_sram_bound)
		return;

	writel(val, qos_sram_base + offset);
}
EXPORT_SYMBOL_GPL(qos_sram_write);

void qos_sram_init(void __iomem *regs, unsigned int bound)
{
	int i;

	qos_sram_base = regs;
	qos_sram_bound = bound;
	pr_info("qos_sram addr:0x%p len:%d\n",
		qos_sram_base, qos_sram_bound);

	for (i = 0; i < bound; i += 4)
		writel(0x0, qos_sram_base+i);
}


int mtk_qos_probe(struct platform_device *pdev,
			const struct mtk_qos_soc *soc)
{
	struct resource *res;
	struct mtk_qos *qos;
	struct device_node *node = pdev->dev.of_node;
	int ret;

	qos = devm_kzalloc(&pdev->dev, sizeof(*qos), GFP_KERNEL);
	if (!qos)
		return -ENOMEM;

	qos->soc = of_device_get_match_data(&pdev->dev);
	if (!qos->soc)
		return -EINVAL;

	ret = of_property_read_u32(node,
			"mediatek,qos_enable", &mtk_qos_enable);
	if (!ret)
		pr_info("mtkqos: dts qos_enable = %d\n", mtk_qos_enable);
	else
		mtk_qos_enable = 1;

	qos->soc = soc;
	qos->dev = &pdev->dev;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	qos->regs = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(qos->regs))
		return PTR_ERR(qos->regs);
	qos_add_interface(&pdev->dev);
	if (mtk_qos_enable) {
		qos->regsize = (unsigned int) resource_size(res);
		m_qos = qos;
		qos_sram_init(qos->regs, qos->regsize);

		qos_ipi_init(qos);

		if (qos->soc->ipi_pin[QOS_IPI_QOS_BOUND].valid == true)
			qos_bound_init();

		qos_ipi_recv_init(qos);
		qos_init_rec_share();
	} else {
		m_qos = NULL;
	}

	platform_set_drvdata(pdev, qos);

	pr_info("mtkqos:%s done (enable=%d)\n", __func__, mtk_qos_enable);

	return 0;
}

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MediaTek QoS driver");
