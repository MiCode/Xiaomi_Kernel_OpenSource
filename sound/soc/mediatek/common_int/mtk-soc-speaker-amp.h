/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Michael Hsiao <michael.hsiao@mediatek.com>
 */

#ifndef _MTK_SP_SPK_AMP_H
#define _MTK_SP_SPK_AMP_H

struct mtk_spk_i2c_ctrl {
	int (*i2c_probe)(struct i2c_client *,
			 const struct i2c_device_id *);
	int (*i2c_remove)(struct i2c_client *);
	void (*i2c_shutdown)(struct i2c_client *);
	const char *codec_dai_name;
	const char *codec_name;
};

enum mtk_spk_type {
	MTK_SPK_NOT_SMARTPA = 0,
	MTK_SPK_RICHTEK_RT5509,
#if defined(CONFIG_SND_SOC_TAS5782M)
	MTK_SPK_TI_TAS5782M,
#endif
	MTK_SPK_MEDIATEK_MT6660,
	MTK_SPK_TYPE_NUM
};

int mtk_spk_get_type(void);
int mtk_spk_update_dai_link(struct snd_soc_dai_link *mtk_spk_dai_link,
			    struct platform_device *pdev);

#endif

