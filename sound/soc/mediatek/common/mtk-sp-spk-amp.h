/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2018 MediaTek Inc.
 */

#ifndef _MTK_SP_SPK_AMP_H
#define _MTK_SP_SPK_AMP_H

struct mtk_spk_i2c_ctrl {
	int (*i2c_probe)(struct i2c_client *client,
			 const struct i2c_device_id *id);
	int (*i2c_remove)(struct i2c_client *client);
	void (*i2c_shutdown)(struct i2c_client *client);
	const char *stream_name;
	const char *codec_dai_name;
	const char *codec_name;
};

#define MTK_SPK_NOT_SMARTPA_STR "MTK_SPK_NOT_SMARTPA"
#define MTK_SPK_RICHTEK_RT5509_STR "MTK_SPK_RICHTEK_RT5509"
#define MTK_SPK_MEDIATEK_MT6660_STR "MTK_SPK_MEDIATEK_MT6660"
#define MTK_SPK_I2S_0_STR "MTK_SPK_I2S_0"
#define MTK_SPK_I2S_1_STR "MTK_SPK_I2S_1"
#define MTK_SPK_I2S_2_STR "MTK_SPK_I2S_2"
#define MTK_SPK_I2S_3_STR "MTK_SPK_I2S_3"
#define MTK_SPK_I2S_5_STR "MTK_SPK_I2S_5"

enum mtk_spk_type {
	MTK_SPK_NOT_SMARTPA = 0,
	MTK_SPK_RICHTEK_RT5509,
	MTK_SPK_MEDIATEK_MT6660,
	MTK_SPK_TYPE_NUM
};

enum mtk_spk_i2s_type {
	MTK_SPK_I2S_TYPE_INVALID = -1,
	MTK_SPK_I2S_0,
	MTK_SPK_I2S_1,
	MTK_SPK_I2S_2,
	MTK_SPK_I2S_3,
	MTK_SPK_I2S_5,
	MTK_SPK_I2S_TYPE_NUM
};

int mtk_spk_get_type(void);
int mtk_spk_get_i2s_out_type(void);
int mtk_spk_get_i2s_in_type(void);
int mtk_spk_update_dai_link(struct snd_soc_card *card,
			    struct platform_device *pdev,
			    const struct snd_soc_ops *i2s_ops);
int mtk_spk_send_ipi_buf_to_dsp(void *data_buffer, uint32_t data_size);
int mtk_spk_recv_ipi_buf_from_dsp(int8_t *buffer,
				  int16_t size,
				  uint32_t *buf_len);

#endif

