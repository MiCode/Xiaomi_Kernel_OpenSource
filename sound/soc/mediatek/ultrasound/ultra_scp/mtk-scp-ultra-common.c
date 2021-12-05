// SPDX-License-Identifier: GPL-2.0
//
// Copyright (C) 2018 MediaTek Inc.
#include "mtk-scp-ultra-common.h"
#include "mtk-base-scp-ultra.h"
#include <sound/soc.h>
#include <linux/device.h>
#include <linux/compat.h>
//#include "audio_ultra_msg_id.h"
#include "ultra_ipi.h"
#include "mtk-base-afe.h"
#include "mtk-scp-ultra.h"
#include "mtk-afe-external.h"

#define AFE_AGENT_SET_OFFSET 4
#define AFE_AGENT_CLR_OFFSET 8

/* don't use this directly if not necessary */
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
EXPORT_SYMBOL_GPL(ultra_set_dsp_afe);
struct mtk_base_afe *get_afe_base(void)
{
	if (!local_scp_ultra_afe)
		pr_err("%s(), local_scp_ultra_afe is NULL", __func__);

	return local_scp_ultra_afe;
}
void set_ipi_recv_private(void *priv)
{
	pr_debug("%s\n", __func__);

	if (priv != NULL)
		ipi_recv_private = priv;
	else
		pr_debug("%s ipi_recv_private has been set\n", __func__);
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
				0x1 << irq_data->irq_scp_en_shift,
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

int ultra_memif_set_enable(struct mtk_base_afe *afe, int afe_id)
{
	struct mtk_base_afe_memif *memif = &afe->memif[afe_id];
	int reg = 0;

	if (memif->data->enable_shift < 0) {
		pr_info("%s(), error, id %d, enable_shift < 0\n",
			 __func__, afe_id);
		return 0;
	}

	if (memif->data->enable_reg < 0) {
		pr_info("%s(), error, id %d, enable_reg < 0\n",
			 __func__, afe_id);
		return 0;
	}

	if (afe->is_memif_bit_banding)
		reg = memif->data->enable_reg + AFE_AGENT_SET_OFFSET;
	else
		reg = memif->data->enable_reg;

	return regmap_update_bits(afe->regmap,
				  reg,
				  1 << memif->data->enable_shift,
				  1 << memif->data->enable_shift);
}

int ultra_memif_set_disable(struct mtk_base_afe *afe, int afe_id)
{
	struct mtk_base_afe_memif *memif = &afe->memif[afe_id];
	int reg = 0;
	int val = 0;

	if (memif->data->enable_shift < 0) {
		pr_info("%s(), error, id %d, enable_shift < 0\n",
			 __func__, afe_id);
		return 0;
	}

	if (memif->data->enable_reg < 0) {
		pr_info("%s(), error, id %d, enable_reg < 0\n",
			 __func__, afe_id);
		return 0;
	}

	if (afe->is_memif_bit_banding) {
		reg = memif->data->enable_reg + AFE_AGENT_CLR_OFFSET;
		val = 1;
	} else {
		reg = memif->data->enable_reg;
		val = 0;
	}

	return regmap_update_bits(afe->regmap,
				  reg,
				  1 << memif->data->enable_shift,
				  val << memif->data->enable_shift);
}

int ultra_memif_set_enable_hw_sema(struct mtk_base_afe *afe, int afe_id)
{
	int ret = 0;
	int scp_sem_ret = NOTIFY_STOP;

	if (!afe)
		return -EPERM;

	if (!afe->is_scp_sema_support)
		return ultra_memif_set_enable(afe, afe_id);

	if (afe->is_memif_bit_banding == 0) {
		scp_sem_ret = notify_3way_semaphore_control(
			NOTIFIER_SCP_3WAY_SEMAPHORE_GET, NULL);
		if (scp_sem_ret != NOTIFY_STOP) {
			pr_info("%s error, adsp_sem_ret[%d]\n", __func__, ret);
			return -EBUSY;
		}
	}

	ret = ultra_memif_set_enable(afe, afe_id);

	if (afe->is_memif_bit_banding == 0) {
		notify_3way_semaphore_control(
			NOTIFIER_SCP_3WAY_SEMAPHORE_RELEASE, NULL);
	}

	return ret;
}

int ultra_memif_set_disable_hw_sema(struct mtk_base_afe *afe, int afe_id)
{
	int ret = 0;
	int scp_sem_ret = NOTIFY_STOP;

	if (!afe)
		return -EPERM;

	if (!afe->is_scp_sema_support)
		return ultra_memif_set_disable(afe, afe_id);

	if (afe->is_memif_bit_banding == 0) {
		scp_sem_ret = notify_3way_semaphore_control(
				NOTIFIER_SCP_3WAY_SEMAPHORE_GET, NULL);
		if (scp_sem_ret != NOTIFY_STOP) {
			pr_info("%s error, adsp_sem_ret[%d]\n", __func__, ret);
			return -EBUSY;
		}
	}

	ret = ultra_memif_set_disable(afe, afe_id);

	if (afe->is_memif_bit_banding == 0) {
		notify_3way_semaphore_control(
			NOTIFIER_SCP_3WAY_SEMAPHORE_RELEASE, NULL);
	}

	return ret;
}

int ultra_irq_set_enable_hw_sema(struct mtk_base_afe *afe,
				 const struct mtk_base_irq_data *irq_data,
				 int afe_id)
{
	int ret = 0;
	int scp_sem_ret = NOTIFY_STOP;

	if (!afe)
		return -ENODEV;
	if (!irq_data)
		return -ENODEV;

	if (!afe->is_scp_sema_support)
		return regmap_update_bits(afe->regmap, irq_data->irq_en_reg,
					  1 << irq_data->irq_en_shift,
					  1 << irq_data->irq_en_shift);

	scp_sem_ret =
		notify_3way_semaphore_control(NOTIFIER_SCP_3WAY_SEMAPHORE_GET,
					      NULL);
	if (scp_sem_ret != NOTIFY_STOP) {
		pr_info("%s error, scp_sem_ret[%d]\n", __func__, scp_sem_ret);
		return -EBUSY;
	}

	regmap_update_bits(afe->regmap, irq_data->irq_en_reg,
			   1 << irq_data->irq_en_shift,
			   1 << irq_data->irq_en_shift);
	notify_3way_semaphore_control(NOTIFIER_SCP_3WAY_SEMAPHORE_RELEASE,
				      NULL);

	return ret;
}

int ultra_irq_set_disable_hw_sema(struct mtk_base_afe *afe,
				  const struct mtk_base_irq_data *irq_data,
				  int afe_id)
{
	int ret = 0;
	int scp_sem_ret = NOTIFY_STOP;

	if (!afe)
		return -ENODEV;
	if (!irq_data)
		return -ENODEV;

	if (!afe->is_scp_sema_support)
		return regmap_update_bits(afe->regmap, irq_data->irq_en_reg,
					  1 << irq_data->irq_en_shift,
					  0 << irq_data->irq_en_shift);

	scp_sem_ret =
		notify_3way_semaphore_control(NOTIFIER_SCP_3WAY_SEMAPHORE_GET,
					      NULL);
	if (scp_sem_ret != NOTIFY_STOP) {
		pr_info("%s error, scp_sem_ret[%d]\n", __func__, scp_sem_ret);
		return -EBUSY;
	}

	regmap_update_bits(afe->regmap, irq_data->irq_en_reg,
			   1 << irq_data->irq_en_shift,
			   0 << irq_data->irq_en_shift);
	notify_3way_semaphore_control(NOTIFIER_SCP_3WAY_SEMAPHORE_RELEASE,
				      NULL);

	return ret;
}

