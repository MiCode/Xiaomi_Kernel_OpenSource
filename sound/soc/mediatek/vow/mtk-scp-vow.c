// SPDX-License-Identifier: GPL-2.0
/*
 *  MediaTek SCP VoW
 *
 *  Copyright (c) 2020 MediaTek Inc.
 *  Author: Michael Hsiao <michael.hsiao@mediatek.com>
 */

#include <linux/module.h>
#include "mtk-base-afe.h"
#include "mtk-sram-manager.h"

#include "mtk-scp-vow.h"
#include "mtk-afe-external.h"

#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCP_SUPPORT)
#include "scp.h"
#endif
#include "vow.h"

int mtk_scp_vow_barge_in_allocate_mem(struct snd_pcm_substream *substream,
			     dma_addr_t *phys_addr,
			     unsigned char **virt_addr,
			     unsigned int size)
{
	struct snd_dma_buffer *dma_buf = &substream->dma_buffer;

	dma_buf->dev.type = SNDRV_DMA_TYPE_DEV;
	dma_buf->dev.dev = substream->pcm->card->dev;
	dma_buf->private_data = NULL;
	dma_buf->bytes = size;

	pr_debug("%s(), use DRAM\n", __func__);
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCP_SUPPORT)
	dma_buf->addr = scp_get_reserve_mem_phys(VOW_BARGEIN_MEM_ID);
	dma_buf->area =
		(uint8_t *)scp_get_reserve_mem_virt(VOW_BARGEIN_MEM_ID);

	*phys_addr = dma_buf->addr;
	*virt_addr = dma_buf->area;
#else
	pr_debug("%s(), SCP not support\n", __func__);
	return -EINVAL;
#endif
	return 0;
}

int notify_vow_ipi_send_event(struct notifier_block *nb, unsigned long event, void *v)
{
	int status = NOTIFY_STOP;//NOTIFY_DONE; //default don't care it.

	if (event == NOTIFIER_VOW_IPI_SEND) {
		struct vow_sound_soc_ipi_send_info *vow_ipi_info;

		vow_ipi_info = (struct vow_sound_soc_ipi_send_info *)v;
		pr_debug("%s(), vow received notify ipi send event.\n", __func__);
		if (vow_ipi_send(vow_ipi_info->msg_id,
				 vow_ipi_info->payload_len,
				 vow_ipi_info->payload,
				 vow_ipi_info->need_ack))
			status = NOTIFY_STOP;
		else
			status = NOTIFY_BAD;
	}
	return status;
}

int notify_vow_init_event(struct notifier_block *nb, unsigned long event, void *v)
{
	int status = NOTIFY_DONE; //default don't care it.

	if (event == NOTIFIER_VOW_ALLOCATE_MEM) {
		struct snd_pcm_substream *substream;

		substream = (struct snd_pcm_substream *)v;
		pr_debug("%s(), vow received afe notify init event.\n", __func__);
		if (mtk_scp_vow_barge_in_allocate_mem(substream,
				&substream->runtime->dma_addr,
				&substream->runtime->dma_area,
				substream->runtime->dma_bytes) == 0)
			status = NOTIFY_STOP;
		else
			status = NOTIFY_BAD;

	}
	return status;
}

/* define a notifier_block */
static struct notifier_block vow_scp_init_notifier = {
	.notifier_call = notify_vow_init_event,
};

static struct notifier_block vow_ipi_send_notifier = {
	.notifier_call = notify_vow_ipi_send_event,
};

static int __init mtk_scp_vow_init(void)
{
	register_afe_allocate_mem_notifier(&vow_scp_init_notifier);
	register_vow_ipi_send_notifier(&vow_ipi_send_notifier);
	return 0;
}

static void __exit mtk_scp_vow_exit(void)
{
	unregister_afe_allocate_mem_notifier(&vow_scp_init_notifier);
	unregister_vow_ipi_send_notifier(&vow_ipi_send_notifier);
}

module_init(mtk_scp_vow_init);
module_exit(mtk_scp_vow_exit);

MODULE_DESCRIPTION("Mediatek SCP VoW Common Driver");
MODULE_AUTHOR("Michael Hsiao <michael.hsiao@mediatek.com>");
MODULE_LICENSE("GPL v2");

