// SPDX-License-Identifier: GPL-2.0
//
// mtk-sram-manager.c  --  Mediatek afe sram manager
//
// Copyright (c) 2017 MediaTek Inc.
// Author: Kai Chieh Chuang <kaichieh.chuang@mediatek.com>

#include <linux/module.h>
#include <linux/dma-mapping.h>
#include <linux/of.h>
#include <linux/of_address.h>

#include "mtk-sram-manager.h"

static void mtk_audio_sram_update_block_valid(struct mtk_audio_sram *sram,
					      enum mtk_audio_sram_mode mode)
{
	int i;

	for (i = 0; i < sram->block_num; i++) {
		if ((i + 1) * sram->block_size > sram->mode_size[mode])
			sram->blocks[i].valid = false;
		else
			sram->blocks[i].valid = true;
	}
}

static bool mtk_audio_sram_avail(struct mtk_audio_sram *sram,
				 unsigned int size,
				 unsigned int *blk_idx,
				 unsigned int *blk_num)
{
	unsigned int max_avail_size = 0;
	bool start_record = false;
	struct mtk_audio_sram_block *sram_blk = NULL;
	int i = 0;

	*blk_idx = 0;

	for (i = 0; i < sram->block_num; i++) {
		sram_blk = &sram->blocks[i];
		if ((sram_blk->user == NULL) && sram_blk->valid) {
			max_avail_size += sram->block_size;
			if (start_record == false) {
				start_record = true;
				*blk_idx = i;
			}
			(*blk_num)++;

			/* can callocate sram */
			if (max_avail_size >= size)
				break;
		}

		/* when reach allocate buffer , reset condition*/
		if ((sram_blk->user != NULL) && sram_blk->valid) {
			max_avail_size = 0;
			*blk_num = 0;
			*blk_idx = 0;
			start_record = false;
		}

		if (sram_blk->valid == 0) {
			dev_warn(sram->dev, "%s(), sram_blk->valid == 0, i = %d\n",
				 __func__, i);
			break;
		}
	}

	dev_info(sram->dev, "%s(), max_avail_size = %d, size = %d, blk_idx = %d, blk_num = %d\n",
		 __func__, max_avail_size, size, *blk_idx, *blk_num);

	if (max_avail_size >= size)
		return true;
	else
		return false;
}

int mtk_audio_sram_init(struct device *dev,
			struct mtk_audio_sram *sram,
			const struct mtk_audio_sram_ops *ops)
{
	struct device_node *sram_node = NULL;
	const __be32 *regaddr_p;
	u64 regaddr64, size64;
	int i = 0;
	int ret;

	if (!ops->set_sram_mode) {
		dev_err(sram->dev, "%s(), ops->set_sram_mode == NULL\n",
			__func__);
		return -EINVAL;
	}
	sram->ops.set_sram_mode = ops->set_sram_mode;

	sram->dev = dev;
	spin_lock_init(&sram->lock);

	/* parse info from device tree */
	sram_node = of_find_compatible_node(NULL, NULL,
					    "mediatek,audio_sram");

	if (sram_node == NULL) {
		dev_err(sram->dev, "%s(), sram_node == NULL\n", __func__);
		goto of_error;
	}

	/* iomap sram address */
	sram->virt_addr = of_iomap(sram_node, 0);
	if (sram->virt_addr == NULL) {
		dev_err(sram->dev, "%s(), sram->virt_addr == NULL\n", __func__);
		goto of_error;
	}

	/* get physical address, size */
	regaddr_p = of_get_address(sram_node, 0, &size64, NULL);
	if (!regaddr_p) {
		dev_err(sram->dev, "%s(), get sram address fail\n", __func__);
		goto of_error;
	}

	regaddr64 = of_translate_address(sram_node, regaddr_p);

	sram->phys_addr = (dma_addr_t)regaddr64;
	sram->size = (unsigned int)size64;

	/* get prefer sram mode, mode size */
	ret = of_property_read_u32(sram_node,
				   "prefer_mode", &sram->prefer_mode);
	if (ret) {
		dev_err(sram->dev, "%s(), get prefer_mode fail\n", __func__);
		goto of_error;
	}

	ret = of_property_read_u32_array(sram_node, "mode_size",
					 sram->mode_size,
					 MTK_AUDIO_SRAM_MODE_NUM);
	if (ret) {
		dev_err(sram->dev, "%s(), get mode_size fail, ret %d\n",
			__func__, ret);
		goto of_error;
	}


	/* get block size */
	ret = of_property_read_u32(sram_node,
				   "block_size", &sram->block_size);
	if (ret) {
		dev_err(sram->dev, "%s(), get block_size fail\n", __func__);
		goto of_error;
	}

	sram->block_num = (sram->size / sram->block_size);

	of_node_put(sram_node);

	dev_info(sram->dev, "%s(), size %d, block_size %d, block_num %d, virt_addr %p, phys_addr %pad\n",
		 __func__,
		 sram->size, sram->block_size, sram->block_num,
		 sram->virt_addr, &sram->phys_addr);
	dev_info(sram->dev, "%s(), prefer_mode %d, mode_size[0] 0x%x, mode_size[1] 0x%x\n",
		 __func__,
		 sram->prefer_mode, sram->mode_size[0], sram->mode_size[1]);

	/* Dynamic allocate sram blocks according to block_num */
	sram->blocks = devm_kcalloc(sram->dev,
				    sram->block_num,
				    sizeof(struct mtk_audio_sram_block),
				    GFP_KERNEL);

	for (i = 0; i < sram->block_num ; i++) {
		sram->blocks[i].valid = true;
		sram->blocks[i].size = sram->block_size;
		sram->blocks[i].user = NULL;
		sram->blocks[i].phys_addr = sram->phys_addr +
						(sram->block_size *
						 (dma_addr_t)i);
		sram->blocks[i].virt_addr = (void *)((char *)sram->virt_addr +
						     (sram->block_size * i));
	}

	/* init for normal mode or compact mode */
	sram->sram_mode = sram->prefer_mode;
	mtk_audio_sram_update_block_valid(sram, sram->sram_mode);

	return 0;
of_error:
	of_node_put(sram_node);
	return -ENODEV;
}

int mtk_audio_sram_allocate(struct mtk_audio_sram *sram,
			    dma_addr_t *phys_addr, unsigned char **virt_addr,
			    unsigned int size, void *user,
			    snd_pcm_format_t format, bool force_normal)
{
	unsigned int block_num = 0;
	unsigned int block_idx = 0;
	struct mtk_audio_sram_block *blocks = NULL;
	enum mtk_audio_sram_mode request_sram_mode;
	bool has_user = false;
	int ret = 0;
	int i;

	dev_info(sram->dev, "%s(), size %d, user %p, format %d, force_normal %d\n",
		 __func__, size, user, format, force_normal);

	spin_lock(&sram->lock);

	/* check if sram has user */
	for (i = 0; i < sram->block_num; i++) {
		blocks = &sram->blocks[i];
		if (blocks->valid == true && blocks->user != NULL) {
			has_user = true;
			break;
		}
	}

	/* get sram mode for this request */
	if (force_normal) {
		request_sram_mode = MTK_AUDIO_SRAM_NORMAL_MODE;
	} else {
		if (format == SNDRV_PCM_FORMAT_S24 ||
		    format == SNDRV_PCM_FORMAT_U24) {
			request_sram_mode = has_user ?
					    sram->sram_mode :
					    sram->prefer_mode;
		} else {
			request_sram_mode = MTK_AUDIO_SRAM_NORMAL_MODE;
		}
	}

	/* change sram mode if needed */
	if (sram->sram_mode != request_sram_mode) {
		if (has_user) {
			dev_info(sram->dev, "%s(), cannot change mode to %d\n",
				 __func__,
				 request_sram_mode);
			spin_unlock(&sram->lock);
			return -ENOMEM;
		}

		sram->sram_mode = request_sram_mode;
		mtk_audio_sram_update_block_valid(sram, sram->sram_mode);
	}

	if (sram->ops.set_sram_mode)
		sram->ops.set_sram_mode(sram->dev, sram->sram_mode);
	else
		dev_warn(sram->dev, "%s(), set_sram_mode == NULL\n",
			 __func__);

	if (mtk_audio_sram_avail(sram, size, &block_idx, &block_num) == true) {
		*phys_addr = sram->blocks[block_idx].phys_addr;
		*virt_addr = (char *)sram->blocks[block_idx].virt_addr;

		/* set aud sram with user*/
		while (block_num) {
			sram->blocks[block_idx].user = user;
			block_num--;
			block_idx++;
		}
	} else {
		ret = -ENOMEM;
	}

	spin_unlock(&sram->lock);
	return ret;
}

int mtk_audio_sram_free(struct mtk_audio_sram *sram, void *user)
{
	unsigned int i = 0;
	struct mtk_audio_sram_block *sram_blk = NULL;

	dev_info(sram->dev, "%s(), user %p\n", __func__, user);

	spin_lock(&sram->lock);
	for (i = 0; i < sram->block_num ; i++) {
		sram_blk = &sram->blocks[i];
		if (sram_blk->user == user)
			sram_blk->user = NULL;
	}
	spin_unlock(&sram->lock);
	return 0;
}

unsigned int mtk_audio_sram_get_size(struct mtk_audio_sram *sram, int mode)
{
	return sram->mode_size[mode];
}


MODULE_DESCRIPTION("Mediatek sram manager");
MODULE_AUTHOR("Kai Chieh Chuang <kaichieh.chuang@mediatek.com>");
MODULE_LICENSE("GPL v2");

