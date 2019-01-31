/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.
 * If not, see <http://www.gnu.org/licenses/>.
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
	MTK_SPK_MEDIATEK_MT6660,
	MTK_SPK_TYPE_NUM
};

int mtk_spk_get_type(void);
int mtk_spk_update_dai_link(struct snd_soc_dai_link *mtk_spk_dai_link,
			    struct platform_device *pdev);

#endif

