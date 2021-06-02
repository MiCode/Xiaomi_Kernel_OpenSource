// SPDX-License-Identifier: GPL-2.0
//
// Copyright (C) 2018 MediaTek Inc.
// Copyright (C) 2021 XiaoMi, Inc.

#include "mtk-scp-ultra-common.h"
#include "mtk-base-scp-ultra.h"
#include <sound/soc.h>
#include <linux/device.h>
#include <linux/compat.h>
#include "scp_helper.h"
#include "scp_ipi.h"
#include "audio_ipi_platform.h"
//#include "audio_ultra_msg_id.h"
#include "ultra_ipi.h"
#include "audio_task_manager.h"
#include "mtk-base-afe.h"
#include "mtk-scp-ultra-platform-mem-control.h"


/* don't use this directly if not necessary */
static struct mtk_base_scp_ultra *local_base_scp_ultra;
static struct mtk_base_afe *local_scp_ultra_afe;
static void *ipi_recv_private;

int ultra_set_dsp_afe(struct mtk_base_afe *afe)
{
	if (!afe) {
		pr_err("%s(), afe is NULL", __func__);
		return -1;
	}

	local_scp_ultra_afe = afe;
	return 0;
}

struct mtk_base_afe *ultra_get_afe_base(void)
{
	if (!local_scp_ultra_afe)
		pr_err("%s(), local_scp_ultra_afe is NULL", __func__);

	return local_scp_ultra_afe;
}

int set_scp_ultra_base(struct mtk_base_scp_ultra *scp_ultra)
{
	if (!scp_ultra) {
		pr_err("%s(), scp_ultra is NULL", __func__);
		return -1;
	}

	local_base_scp_ultra = scp_ultra;
	return 0;

}

void *get_scp_ultra_base(void)
{
	if (!local_base_scp_ultra)
		pr_err("%s(), local_base_scp_ultra is NULL", __func__);

	return local_base_scp_ultra;
}

void *ultra_get_ipi_recv_private(void)
{
	if (!ipi_recv_private)
		pr_info("%s(), ipi_recv_private is NULL", __func__);

	return ipi_recv_private;
}

void ultra_set_ipi_recv_private(void *priv)
{
	pr_debug("%s()\n", __func__);

	if (!ipi_recv_private)
		ipi_recv_private = priv;
	else
		pr_info("%s() has been set\n", __func__);
}

void set_afe_dl_irq_target(int scp_enable)
{
	struct mtk_base_afe *afe = get_afe_base();
	struct mtk_base_scp_ultra *scp_ultra = get_scp_ultra_base();
	struct mtk_base_afe_memif *memif =
		&afe->memif[scp_ultra->ultra_mem.ultra_dl_memif_id];
	struct mtk_base_afe_irq *irqs = &afe->irqs[memif->irq_usage];
	const struct mtk_base_irq_data *irq_data = irqs->irq_data;

	if (scp_enable) {
		regmap_update_bits(afe->regmap,
				irq_data->irq_ap_en_reg,
				0x1 << irq_data->irq_ap_en_shift,
				0x0);
		regmap_update_bits(afe->regmap,
				irq_data->irq_scp_en_reg,
				0x1 << irq_data->irq_scp_en_shift,
				0x1 << irq_data->irq_scp_en_shift);
	} else {
		regmap_update_bits(afe->regmap,
				irq_data->irq_scp_en_reg,
				0x1 << irq_data->irq_ap_en_shift,
				0);
		regmap_update_bits(afe->regmap,
				   irq_data->irq_ap_en_reg,
				   0x1 << irq_data->irq_ap_en_shift,
				   0x1 << irq_data->irq_ap_en_shift);
	}
	pr_debug("%s(), scp_en=%d,memif=%d,ap_en_reg:0x%x,scp_en_reg:0x%x\n",
		 __func__,
		 scp_enable,
		 scp_ultra->ultra_mem.ultra_dl_memif_id,
		 irq_data->irq_ap_en_reg,
		 irq_data->irq_scp_en_reg);
}
void set_afe_ul_irq_target(int scp_enable)
{
	struct mtk_base_afe *afe = get_afe_base();
	struct mtk_base_scp_ultra *scp_ultra = get_scp_ultra_base();
	struct mtk_base_afe_memif *memif =
		&afe->memif[scp_ultra->ultra_mem.ultra_ul_memif_id];
	struct mtk_base_afe_irq *irqs = &afe->irqs[memif->irq_usage];
	const struct mtk_base_irq_data *irq_data = irqs->irq_data;

	if (scp_enable) {
		regmap_update_bits(afe->regmap,
				   irq_data->irq_ap_en_reg,
				   0x1 << irq_data->irq_ap_en_shift,
				   0x0);

		regmap_update_bits(afe->regmap,
				   irq_data->irq_scp_en_reg,
				   0x1 << irq_data->irq_scp_en_shift,
				   0x1 << irq_data->irq_scp_en_shift);
	} else {
		regmap_update_bits(afe->regmap,
				   irq_data->irq_scp_en_reg,
				   0x1 << irq_data->irq_scp_en_shift,
				   0);

		regmap_update_bits(afe->regmap,
				   irq_data->irq_ap_en_reg,
				   0x1 << irq_data->irq_ap_en_shift,
				   0x1 << irq_data->irq_ap_en_shift);
	}
	pr_debug("%s(),scp_en=%d,memif=%d,ap_en_reg:0x%x,scp_en_reg:0x%x\n",
		 __func__,
		 scp_enable,
		 scp_ultra->ultra_mem.ultra_ul_memif_id,
		 irq_data->irq_ap_en_reg,
		 irq_data->irq_scp_en_reg);
}

