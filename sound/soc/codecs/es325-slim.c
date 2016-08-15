/*
 * es325.c  --  Audience eS325 ALSA SoC Audio driver
 *
 * Copyright 2011 Audience, Inc.
 * Copyright (C) 2016 XiaoMi, Inc.
 *
 * Author: Greg Clemson <gclemson@audience.com>
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/firmware.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/completion.h>
#include <linux/i2c.h>
#include <linux/slimbus/slimbus.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dai.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>
#include <sound/tlv.h>
#include <linux/kthread.h>
#include <linux/device.h>
#include <linux/pm_runtime.h>
#include <linux/mutex.h>
#include <linux/slimbus/slimbus.h>
#include <linux/i2c/esxxx.h> /* TODO: common location for i2c and slimbus */
#include "es325.h"

#define ES325_SLIM_1_PB_MAX_CHANS	2
#define ES325_SLIM_1_CAP_MAX_CHANS	2
#define ES325_SLIM_2_PB_MAX_CHANS	2
#define ES325_SLIM_2_CAP_MAX_CHANS	2
#define ES325_SLIM_3_PB_MAX_CHANS	2
#define ES325_SLIM_3_CAP_MAX_CHANS	2

#define ES325_SLIM_1_PB_OFFSET	0
#define ES325_SLIM_2_PB_OFFSET	2
#define ES325_SLIM_3_PB_OFFSET	4
#define ES325_SLIM_1_CAP_OFFSET	0
#define ES325_SLIM_2_CAP_OFFSET	2
#define ES325_SLIM_3_CAP_OFFSET	4

static int es325_slim_rx_port_to_ch[ES325_SLIM_RX_PORTS] = {
	152, 153, 154, 155, 134, 135
};

static int es325_slim_tx_port_to_ch[ES325_SLIM_TX_PORTS] = {
	156, 157, 138, 139, 143, 144
};

static int es325_slim_be_id[ES325_NUM_CODEC_SLIM_DAIS] = {
	ES325_SLIM_2_CAP, /* for ES325_SLIM_1_PB tx from es325 */
	ES325_SLIM_3_PB, /* for ES325_SLIM_1_CAP rx to es325 */
	ES325_SLIM_3_CAP, /* for ES325_SLIM_2_PB tx from es325 */
	-1, /* for ES325_SLIM_2_CAP */
	-1, /* for ES325_SLIM_3_PB */
	-1, /* for ES325_SLIM_3_CAP */
};

static void es325_alloc_slim_rx_chan(struct slim_device *sbdev);
static void es325_alloc_slim_tx_chan(struct slim_device *sbdev);
static int es325_cfg_slim_rx(struct slim_device *sbdev, unsigned int *ch_num,
			     unsigned int ch_cnt, unsigned int rate);
static int es325_cfg_slim_tx(struct slim_device *sbdev, unsigned int *ch_num,
			     unsigned int ch_cnt, unsigned int rate);
static int es325_close_slim_rx(struct slim_device *sbdev, unsigned int *ch_num,
			       unsigned int ch_cnt);
static int es325_close_slim_tx(struct slim_device *sbdev, unsigned int *ch_num,
			       unsigned int ch_cnt);
static int es325_rx_ch_num_to_idx(int ch_num);
static int es325_tx_ch_num_to_idx(int ch_num);

static int es325_rx_ch_num_to_idx(int ch_num)
{
	int i;
	int idx = -1;

	pr_debug("%s(ch_num = %d)\n", __func__, ch_num);
	/* for (i = 0; i < ES325_SLIM_RX_PORTS; i++) { */
	for (i = 0; i < 6; i++) {
		if (ch_num == es325_slim_rx_port_to_ch[i]) {
			idx = i;
			break;
		}
	}

	return idx;
}

static int es325_tx_ch_num_to_idx(int ch_num)
{
	int i;
	int idx = -1;

	pr_debug("%s(ch_num = %d)\n", __func__, ch_num);
	for (i = 0; i < ES325_SLIM_TX_PORTS; i++) {
		if (ch_num == es325_slim_tx_port_to_ch[i]) {
			idx = i;
			break;
		}
	}

	return idx;
}

/* es325 -> codec - alsa playback function */
static int es325_codec_cfg_slim_tx(struct es325_priv *es325, int dai_id)
{
	struct slim_device *sbdev = es325->gen0_client;
	int rc;

	dev_dbg(&sbdev->dev, "%s(dai_id = %d)\n", __func__, dai_id);
	/* start slim channels associated with id */
	rc = es325_cfg_slim_tx(es325->gen0_client,
			       es325->dai[dai_id - 1].ch_num,
			       es325->dai[dai_id - 1].ch_tot,
			       es325->dai[dai_id - 1].rate);

	return rc;
}

/* es325 <- codec - alsa capture function */
static int es325_codec_cfg_slim_rx(struct es325_priv *es325, int dai_id)
{
	struct slim_device *sbdev = es325->gen0_client;
	int rc;

	dev_dbg(&sbdev->dev, "%s(dai_id = %d)\n", __func__, dai_id);
	/* start slim channels associated with id */
	rc = es325_cfg_slim_rx(es325->gen0_client,
			       es325->dai[dai_id - 1].ch_num,
			       es325->dai[dai_id - 1].ch_tot,
			       es325->dai[dai_id - 1].rate);

	return rc;
}

/* es325 -> codec - alsa playback function */
static int es325_codec_close_slim_tx(struct es325_priv *es325, int dai_id)
{
	struct slim_device *sbdev = es325->gen0_client;
	int rc;

	dev_dbg(&sbdev->dev, "%s(dai_id = %d)\n", __func__, dai_id);
	/* close slim channels associated with id */
	rc = es325_close_slim_tx(es325->gen0_client,
				 es325->dai[dai_id - 1].ch_num,
				 es325->dai[dai_id - 1].ch_tot);

	return rc;
}

/* es325 <- codec - alsa capture function */
static int es325_codec_close_slim_rx(struct es325_priv *es325, int dai_id)
{
	struct slim_device *sbdev = es325->gen0_client;
	int rc;

	dev_dbg(&sbdev->dev, "%s(dai_id = %d)\n", __func__, dai_id);
	/* close slim channels associated with id */
	rc = es325_close_slim_rx(es325->gen0_client,
				 es325->dai[dai_id - 1].ch_num,
				 es325->dai[dai_id - 1].ch_tot);

	return rc;
}

static void es325_alloc_slim_rx_chan(struct slim_device *sbdev)
{
	struct es325_priv *es325_priv = slim_get_devicedata(sbdev);
	struct es325_slim_ch *rx = es325_priv->slim_rx;
	int i;
	int port_id;

	dev_dbg(&sbdev->dev, "%s()\n", __func__);

	/* for (i = 0; i < ES325_SLIM_RX_PORTS; i++) { */
	for (i = 0; i < 6; i++) {
		port_id = i;
		rx[i].ch_num = es325_slim_rx_port_to_ch[i];
		slim_get_slaveport(sbdev->laddr, port_id, &rx[i].sph,
				   SLIM_SINK);
		slim_query_ch(sbdev, rx[i].ch_num, &rx[i].ch_h);
	}
}

static void es325_alloc_slim_tx_chan(struct slim_device *sbdev)
{
	struct es325_priv *es325_priv = slim_get_devicedata(sbdev);
	struct es325_slim_ch *tx = es325_priv->slim_tx;
	int i;
	int port_id;

	dev_dbg(&sbdev->dev, "%s()\n", __func__);

	for (i = 0; i < ES325_SLIM_TX_PORTS; i++) {
		port_id = i + 10; /* ES325_SLIM_RX_PORTS; */
		tx[i].ch_num = es325_slim_tx_port_to_ch[i];
		slim_get_slaveport(sbdev->laddr, port_id, &tx[i].sph,
				   SLIM_SRC);
		slim_query_ch(sbdev, tx[i].ch_num, &tx[i].ch_h);
	}
}

static int es325_cfg_slim_rx(struct slim_device *sbdev, unsigned int *ch_num,
			     unsigned int ch_cnt, unsigned int rate)
{
	struct es325_priv *es325_priv = slim_get_devicedata(sbdev);
	struct es325_slim_ch *rx = es325_priv->slim_rx;
	u16 grph;
	u32 sph[ES325_SLIM_RX_PORTS] = {0};
	u16 ch_h[ES325_SLIM_RX_PORTS] = {0};
	struct slim_ch prop;
	int i;
	int idx;
	int rc = 0;

	dev_dbg(&sbdev->dev, "%s(ch_cnt = %d, rate = %d)\n", __func__,
		ch_cnt, rate);

	for (i = 0; i < ch_cnt; i++) {
		idx = es325_rx_ch_num_to_idx(ch_num[i]);
		ch_h[i] = rx[idx].ch_h;
		sph[i] = rx[idx].sph;
	}

	prop.prot = SLIM_AUTO_ISO;
	prop.baser = SLIM_RATE_4000HZ;
	prop.dataf = SLIM_CH_DATAF_NOT_DEFINED;
	prop.auxf = SLIM_CH_AUXF_NOT_APPLICABLE;
	prop.ratem = (rate/4000);
	prop.sampleszbits = 16;

	rc = slim_define_ch(sbdev, &prop, ch_h, ch_cnt, true, &grph);
	if (rc < 0) {
		dev_err(&sbdev->dev, "%s(): slim_define_ch() failed: %d\n",
			__func__, rc);
		goto slim_define_ch_error;
	}
	for (i = 0; i < ch_cnt; i++) {
		rc = slim_connect_sink(sbdev, &sph[i], 1, ch_h[i]);
		if (rc < 0) {
			dev_err(&sbdev->dev,
				"%s(): slim_connect_sink() failed: %d\n",
				__func__, rc);
			goto slim_connect_sink_error;
		}
	}
	rc = slim_control_ch(sbdev, grph, SLIM_CH_ACTIVATE, true);
	if (rc < 0) {
		dev_err(&sbdev->dev,
			"%s(): slim_control_ch() failed: %d\n",
			__func__, rc);
		goto slim_control_ch_error;
	}
	for (i = 0; i < ch_cnt; i++) {
		idx = es325_rx_ch_num_to_idx(ch_num[i]);
		rx[idx].grph = grph;
	}
	return rc;
slim_control_ch_error:
slim_connect_sink_error:
	es325_close_slim_rx(sbdev, ch_num, ch_cnt);
slim_define_ch_error:
	return rc;
}

static int es325_cfg_slim_tx(struct slim_device *sbdev, unsigned int *ch_num,
			     unsigned int ch_cnt, unsigned int rate)
{
	struct es325_priv *es325_priv = slim_get_devicedata(sbdev);
	struct es325_slim_ch *tx = es325_priv->slim_tx;
	u16 grph;
	u32 sph[ES325_SLIM_TX_PORTS] = {0};
	u16 ch_h[ES325_SLIM_TX_PORTS] = {0};
	struct slim_ch prop;
	int i;
	int idx;
	int rc;

	dev_dbg(&sbdev->dev, "%s(ch_cnt = %d, rate = %d)\n", __func__,
		ch_cnt, rate);

	for (i = 0; i < ch_cnt; i++) {
		idx = es325_tx_ch_num_to_idx(ch_num[i]);
		ch_h[i] = tx[idx].ch_h;
		sph[i] = tx[idx].sph;
	}

	prop.prot = SLIM_AUTO_ISO;
	prop.baser = SLIM_RATE_4000HZ;
	prop.dataf = SLIM_CH_DATAF_NOT_DEFINED;
	prop.auxf = SLIM_CH_AUXF_NOT_APPLICABLE;
	prop.ratem = (rate/4000);
	prop.sampleszbits = 16;

	rc = slim_define_ch(sbdev, &prop, ch_h, ch_cnt, true, &grph);
	if (rc < 0) {
		dev_err(&sbdev->dev, "%s(): slim_define_ch() failed: %d\n",
			__func__, rc);
		goto slim_define_ch_error;
	}
	for (i = 0; i < ch_cnt; i++) {
		rc = slim_connect_src(sbdev, sph[i], ch_h[i]);
		if (rc < 0) {
			dev_err(&sbdev->dev,
				"%s(): slim_connect_src() failed: %d\n",
				__func__, rc);
			dev_err(&sbdev->dev,
				"%s(): ch_num[0] = %d\n",
				__func__, ch_num[0]);
			goto slim_connect_src_error;
		}
	}
	rc = slim_control_ch(sbdev, grph, SLIM_CH_ACTIVATE, true);
	if (rc < 0) {
		dev_err(&sbdev->dev,
			"%s(): slim_control_ch() failed: %d\n",
			__func__, rc);
		goto slim_control_ch_error;
	}
	for (i = 0; i < ch_cnt; i++) {
		idx = es325_tx_ch_num_to_idx(ch_num[i]);
		tx[idx].grph = grph;
	}
	return rc;
slim_control_ch_error:
slim_connect_src_error:
	es325_close_slim_tx(sbdev, ch_num, ch_cnt);
slim_define_ch_error:
	return rc;
}

static int es325_close_slim_rx(struct slim_device *sbdev, unsigned int *ch_num,
			       unsigned int ch_cnt)
{
	struct es325_priv *es325_priv = slim_get_devicedata(sbdev);
	struct es325_slim_ch *rx = es325_priv->slim_rx;
	u16 grph = 0;
	u32 sph[ES325_SLIM_RX_PORTS] = {0};
	int i;
	int idx;
	int rc;

	dev_dbg(&sbdev->dev, "%s(ch_cnt = %d)\n", __func__, ch_cnt);

	for (i = 0; i < ch_cnt; i++) {
		idx = es325_rx_ch_num_to_idx(ch_num[i]);
		sph[i] = rx[idx].sph;
		grph = rx[idx].grph;
	}

	rc = slim_control_ch(sbdev, grph, SLIM_CH_REMOVE, true);
	if (rc < 0) {
		dev_err(&sbdev->dev,
			"%s(): slim_control_ch() failed: %d\n",
			__func__, rc);
		goto slim_control_ch_error;
	}
	for (i = 0; i < ch_cnt; i++) {
		idx = es325_rx_ch_num_to_idx(ch_num[i]);
		rx[idx].grph = 0;
	}
	rc = slim_disconnect_ports(sbdev, sph, ch_cnt);
	if (rc < 0) {
		dev_err(&sbdev->dev,
			"%s(): slim_disconnect_ports() failed: %d\n",
			__func__, rc);
	}
slim_control_ch_error:
	return rc;
}

static int es325_close_slim_tx(struct slim_device *sbdev, unsigned int *ch_num,
			       unsigned int ch_cnt)
{
	struct es325_priv *es325_priv = slim_get_devicedata(sbdev);
	struct es325_slim_ch *tx = es325_priv->slim_tx;
	u16 grph = 0;
	u32 sph[ES325_SLIM_TX_PORTS] = {0};
	int i;
	int idx;
	int rc;

	dev_dbg(&sbdev->dev, "%s(ch_cnt = %d)\n", __func__, ch_cnt);

	for (i = 0; i < ch_cnt; i++) {
		idx = es325_tx_ch_num_to_idx(ch_num[i]);
		sph[i] = tx[idx].sph;
		grph = tx[idx].grph;
	}

	rc = slim_control_ch(sbdev, grph, SLIM_CH_REMOVE, true);
	if (rc < 0) {
		dev_err(&sbdev->dev,
			"%s(): slim_connect_sink() failed: %d\n",
			__func__, rc);
		goto slim_control_ch_error;
	}
	for (i = 0; i < ch_cnt; i++) {
		idx = es325_tx_ch_num_to_idx(ch_num[i]);
		tx[idx].grph = 0;
	}
	rc = slim_disconnect_ports(sbdev, sph, ch_cnt);
	if (rc < 0) {
		dev_err(&sbdev->dev,
			"%s(): slim_disconnect_ports() failed: %d\n",
			__func__, rc);
	}
slim_control_ch_error:
	return rc;
}

int es325_remote_cfg_slim_rx(int dai_id)
{
	struct es325_priv *es325 = &es325_priv;
	struct slim_device *sbdev = es325->gen0_client;
	int be_id;
	int rc = 0;

	dev_dbg(&sbdev->dev, "%s(dai_id = %d)\n", __func__, dai_id);

	if (dai_id != ES325_SLIM_1_PB
	    && dai_id != ES325_SLIM_2_PB)
		return rc;

	if (es325->dai[dai_id - 1].ch_tot != 0) {
		/* start slim channels associated with id */
		rc = es325_cfg_slim_rx(es325->gen0_client,
				       es325->dai[dai_id - 1].ch_num,
				       es325->dai[dai_id - 1].ch_tot,
				       es325->dai[dai_id - 1].rate);

		be_id = es325_slim_be_id[dai_id - 1];
		es325->dai[be_id - 1].ch_tot = es325->dai[dai_id - 1].ch_tot;
		es325->dai[be_id - 1].rate = es325->dai[dai_id - 1].rate;
		if (be_id == ES325_SLIM_2_CAP) {
			es325->dai[be_id - 1].ch_num[0] = 138;
			es325->dai[be_id - 1].ch_num[1] = 139;
		} else if (be_id == ES325_SLIM_3_CAP) {
			es325->dai[be_id - 1].ch_num[0] = 143;
			es325->dai[be_id - 1].ch_num[1] = 144;
		}
		rc = es325_codec_cfg_slim_tx(es325, be_id);
	}

	return rc;
}

EXPORT_SYMBOL_GPL(es325_remote_cfg_slim_rx);

int es325_remote_cfg_slim_tx(int dai_id)
{
	struct es325_priv *es325 = &es325_priv;
	struct slim_device *sbdev = es325->gen0_client;
	int be_id;
	int ch_cnt;
	int rc = 0;

	dev_dbg(&sbdev->dev, "%s(dai_id = %d)\n", __func__, dai_id);

	if (dai_id != ES325_SLIM_1_CAP)
		return rc;

	if (es325->dai[dai_id - 1].ch_tot != 0) {
		/* start slim channels associated with id */
		if (dai_id == ES325_SLIM_1_CAP) {
			ch_cnt = es325->ap_tx1_ch_cnt;
		}
		rc = es325_cfg_slim_tx(es325->gen0_client,
				       es325->dai[dai_id - 1].ch_num,
				       ch_cnt,
				       es325->dai[dai_id - 1].rate);

		be_id = es325_slim_be_id[dai_id - 1];
		es325->dai[be_id - 1].ch_tot = es325->dai[dai_id - 1].ch_tot;
		es325->dai[be_id - 1].rate = es325->dai[dai_id - 1].rate;
		if (be_id == ES325_SLIM_3_PB) {
			es325->dai[be_id - 1].ch_num[0] = 134;
			es325->dai[be_id - 1].ch_num[1] = 135;
		}
		rc = es325_codec_cfg_slim_rx(es325, be_id);
	}

	return rc;
}
EXPORT_SYMBOL_GPL(es325_remote_cfg_slim_tx);

int es325_remote_close_slim_rx(int dai_id)
{
	struct es325_priv *es325 = &es325_priv;
	struct slim_device *sbdev = es325->gen0_client;
	int be_id;
	int rc = 0;

	dev_dbg(&sbdev->dev, "%s(dai_id = %d)\n", __func__, dai_id);

	if (dai_id != ES325_SLIM_1_PB
	    && dai_id != ES325_SLIM_2_PB)
		return rc;

	if (es325->dai[dai_id - 1].ch_tot != 0) {
		es325_close_slim_rx(es325->gen0_client,
				    es325->dai[dai_id - 1].ch_num,
				    es325->dai[dai_id - 1].ch_tot);

		be_id = es325_slim_be_id[dai_id - 1];
		rc = es325_codec_close_slim_tx(es325, be_id);

		es325->dai[dai_id - 1].ch_tot = 0;
	}

	return rc;
}

EXPORT_SYMBOL_GPL(es325_remote_close_slim_rx);

int es325_remote_close_slim_tx(int dai_id)
{
	struct es325_priv *es325 = &es325_priv;
	struct slim_device *sbdev = es325->gen0_client;
	int be_id;
	int ch_cnt;
	int rc = 0;

	dev_dbg(&sbdev->dev, "%s(dai_id = %d)\n", __func__, dai_id);

	if (dai_id != ES325_SLIM_1_CAP)
		return rc;

	if (es325->dai[dai_id - 1].ch_tot != 0) {
		if (dai_id == ES325_SLIM_1_CAP)
			ch_cnt = es325->ap_tx1_ch_cnt;
		es325_close_slim_tx(es325->gen0_client,
				    es325->dai[dai_id - 1].ch_num,
				    ch_cnt);

		be_id = es325_slim_be_id[dai_id - 1];
		rc = es325_codec_close_slim_rx(es325, be_id);

		es325->dai[dai_id - 1].ch_tot = 0;
	}

	return rc;
}

EXPORT_SYMBOL_GPL(es325_remote_close_slim_tx);

void es325_init_slim_slave(struct slim_device *sbdev)
{
	dev_dbg(&sbdev->dev, "%s()\n", __func__);

	es325_alloc_slim_rx_chan(sbdev);
	es325_alloc_slim_tx_chan(sbdev);
}

void es325_slim_ch_num(struct snd_soc_dai_driver *es325_dai)
{
	int ch_cnt = 0;
	/* allocate ch_num array for each DAI */
	for (i = 0; i < ES325_NUM_CODEC_SLIM_DAIS; i++) {
		switch (es325_dai[i].id) {
		case ES325_SLIM_1_PB:
		case ES325_SLIM_2_PB:
		case ES325_SLIM_3_PB:
			ch_cnt = es325_dai[i].playback.channels_max;
			break;
		case ES325_SLIM_1_CAP:
		case ES325_SLIM_2_CAP:
		case ES325_SLIM_3_CAP:
			ch_cnt = es325_dai[i].capture.channels_max;
			break;
		default:
			continue;
		}
		es325_priv.dai[i].ch_num =
			kzalloc((ch_cnt * sizeof(unsigned int)), GFP_KERNEL);
	}

}

int es325_slim_read(struct es325_priv *es325, char *buf, int len)
{
	struct slim_device *sbdev = es325->gen0_client;
	DECLARE_COMPLETION_ONSTACK(read_done);
	struct slim_ele_access msg = {
		.start_offset = ES325_READ_VE_OFFSET,
		.num_bytes = ES325_READ_VE_WIDTH,
		.comp = NULL,
	};
	int rc;

	rc = slim_request_val_element(sbdev, &msg, buf, len);
	if (rc != 0)
		dev_err(&sbdev->dev, "%s: read failed rc=%d\n",
			__func__, rc);

	return rc;
}

int es325_slim_write(struct es325_priv *es325, char *buf, int len)
{
	struct slim_device *sbdev = es325->gen0_client;
	struct slim_ele_access msg = {
		.start_offset = ES325_WRITE_VE_OFFSET,
		.num_bytes = ES325_WRITE_VE_WIDTH,
		.comp = NULL,
	};
	int rc;

	rc = slim_change_val_element(sbdev, &msg, buf, len);
	if (rc != 0)
		dev_err(&sbdev->dev, "%s: slim_write failed rc=%d\n",
			__func__, rc);

	return rc;
}

static int es325_slim_set_dai_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	return 0;
}

int es325_slim_set_channel_map(struct snd_soc_dai *dai,
			       unsigned int tx_num, unsigned int *tx_slot,
			       unsigned int rx_num, unsigned int *rx_slot)
{
	struct snd_soc_codec *codec = dai->codec;
	/* local codec access */
	/* struct es325_priv *es325 = snd_soc_codec_get_drvdata(codec); */
	/* remote codec access */
	struct es325_priv *es325 = &es325_priv;
	int id = dai->id;
	int i;
	int rc = 0;

	dev_dbg(codec->dev, "%s() dai->name = %s, dai->id = %d\n", __func__,
		dai->name, dai->id);

	if (id == ES325_SLIM_1_PB ||
	    id == ES325_SLIM_2_PB ||
	    id == ES325_SLIM_3_PB) {
		es325->dai[id - 1].ch_tot = rx_num;
		es325->dai[id - 1].ch_act = 0;
		for (i = 0; i < rx_num; i++)
			es325->dai[id - 1].ch_num[i] = rx_slot[i];
	} else if (id == ES325_SLIM_1_CAP ||
		 id == ES325_SLIM_2_CAP ||
		 id == ES325_SLIM_3_CAP) {
		es325->dai[id - 1].ch_tot = tx_num;
		es325->dai[id - 1].ch_act = 0;
		for (i = 0; i < tx_num; i++) {
			es325->dai[id - 1].ch_num[i] = tx_slot[i];
		}
	}

	return rc;
}

EXPORT_SYMBOL_GPL(es325_slim_set_channel_map);

int es325_slim_get_channel_map(struct snd_soc_dai *dai,
			       unsigned int *tx_num, unsigned int *tx_slot,
			       unsigned int *rx_num, unsigned int *rx_slot)
{
	struct snd_soc_codec *codec = dai->codec;
	/* local codec access */
	/* struct es325_priv *es325 = snd_soc_codec_get_drvdata(codec); */
	/* remote codec access */
	struct es325_priv *es325 = &es325_priv;
	struct es325_slim_ch *rx = es325->slim_rx;
	struct es325_slim_ch *tx = es325->slim_tx;
	int id = dai->id;
	int i;
	int rc = 0;

	dev_dbg(codec->dev, "%s() dai->name = %s, dai->id = %d\n", __func__,
		dai->name, dai->id);

	if (id == ES325_SLIM_1_PB) {
		*rx_num = es325_dai[id - 1].playback.channels_max;
		for (i = 0; i < *rx_num; i++) {
			rx_slot[i] = rx[ES325_SLIM_1_PB_OFFSET + i].ch_num;
		}
	} else if (id == ES325_SLIM_2_PB) {
		*rx_num = es325_dai[id - 1].playback.channels_max;
		for (i = 0; i < *rx_num; i++) {
			rx_slot[i] = rx[ES325_SLIM_2_PB_OFFSET + i].ch_num;
		}
	} else if (id == ES325_SLIM_3_PB) {
		*rx_num = es325_dai[id - 1].playback.channels_max;
		for (i = 0; i < *rx_num; i++) {
			rx_slot[i] = rx[ES325_SLIM_3_PB_OFFSET + i].ch_num;
		}
	} else if (id == ES325_SLIM_1_CAP) {
		*tx_num = es325_dai[id - 1].capture.channels_max;
		for (i = 0; i < *tx_num; i++) {
			tx_slot[i] = tx[ES325_SLIM_1_CAP_OFFSET + i].ch_num;
		}
	} else if (id == ES325_SLIM_2_CAP) {
		*tx_num = es325_dai[id - 1].capture.channels_max;
		for (i = 0; i < *tx_num; i++) {
			tx_slot[i] = tx[ES325_SLIM_2_CAP_OFFSET + i].ch_num;
		}
	} else if (id == ES325_SLIM_3_CAP) {
		*tx_num = es325_dai[id - 1].capture.channels_max;
		for (i = 0; i < *tx_num; i++) {
			tx_slot[i] = tx[ES325_SLIM_3_CAP_OFFSET + i].ch_num;
		}
	}

	return rc;
}

EXPORT_SYMBOL_GPL(es325_slim_get_channel_map);

int es325_remote_route_enable(struct snd_soc_dai *dai)
{
	pr_debug("%s():dai->name = %s dai->id = %d\n", __func__,
		 dai->name, dai->id);
	if (es325_priv.intf == ES325_I2C_INTF)
	  return 0;

	switch (dai->id) {
	case ES325_SLIM_1_PB:
		return es325_priv.rx1_route_enable;
	case ES325_SLIM_1_CAP:
		return es325_priv.tx1_route_enable;
	case ES325_SLIM_2_PB:
		return es325_priv.rx2_route_enable;
	default:
		return 0;
	}
}

EXPORT_SYMBOL_GPL(es325_remote_route_enable);

static int es325_slim_set_tristate(struct snd_soc_dai *dai, int tristate)
{
	return 0;
}

static int es325_slim_port_mute(struct snd_soc_dai *dai, int mute)
{
	return 0;
}

int es325_slim_startup(struct snd_pcm_substream *substream,
			      struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;

	dev_dbg(codec->dev, "%s() dai->name = %s, dai->id = %d\n", __func__,
		dai->name, dai->id);

	return 0;
}

EXPORT_SYMBOL_GPL(es325_slim_startup);

void es325_slim_shutdown(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;

	dev_dbg(codec->dev, "%s() dai->name = %s, dai->id = %d\n", __func__,
		dai->name, dai->id);
}

EXPORT_SYMBOL_GPL(es325_slim_shutdown);

int es325_slim_hw_params(struct snd_pcm_substream *substream,
			 struct snd_pcm_hw_params *params,
			 struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	/* local codec access */
	/* struct es325_priv *es325 = snd_soc_codec_get_drvdata(codec); */
	/* remote codec access */
	struct es325_priv *es325 = &es325_priv;
	int id = dai->id;
	int channels;
	int rate;
	int rc = 0;

	dev_dbg(codec->dev, "%s() dai->name = %s, dai->id = %d\n", __func__,
		dai->name, dai->id);

	channels = params_channels(params);
	switch (channels) {
	case 1:
	case 2:
	case 3:
	case 4:
	case 5:
	case 6:
		es325->dai[id - 1].ch_tot = channels;
		break;
	default:
		dev_err(codec->dev,
			"%s(): unsupported number of channels, %d\n",
			__func__, channels);
		return -EINVAL;
	}
	rate = params_rate(params);
	switch (rate) {
	case 8000:
	case 16000:
	case 32000:
	case 48000:
		es325->dai[id - 1].rate = rate;
		break;
	default:
		dev_err(codec->dev,
			"%s(): unsupported rate, %d\n",
			__func__, rate);
		return -EINVAL;
	}

	return rc;
}
EXPORT_SYMBOL_GPL(es325_slim_hw_params);

static int es325_slim_hw_free(struct snd_pcm_substream *substream,
			      struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	int rc = 0;

	dev_dbg(codec->dev, "%s() dai->name = %s, dai->id = %d\n", __func__,
		dai->name, dai->id);

	return rc;
}

static int es325_slim_prepare(struct snd_pcm_substream *substream,
			      struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	int rc = 0;

	dev_dbg(codec->dev, "%s() dai->name = %s, dai->id = %d\n", __func__,
		dai->name, dai->id);

	return rc;
}

int es325_slim_trigger(struct snd_pcm_substream *substream,
		       int cmd, struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	/* local codec access */
	/* struct es325_priv *es325 = snd_soc_codec_get_drvdata(codec); */
	/* remote codec access */
	/* struct es325_priv *es325 = &es325_priv; */
	int rc = 0;

	dev_dbg(codec->dev, "%s() dai->name = %s, dai->id = %d\n", __func__,
		dai->name, dai->id);

	return rc;
}

EXPORT_SYMBOL_GPL(es325_slim_trigger);

struct snd_soc_dai_ops es325_slim_port_dai_ops = {
	.set_fmt	= es325_slim_set_dai_fmt,
	.set_channel_map	= es325_slim_set_channel_map,
	.get_channel_map	= es325_slim_get_channel_map,
	.set_tristate	= es325_slim_set_tristate,
	.digital_mute	= es325_slim_port_mute,
	.startup	= es325_slim_startup,
	.shutdown	= es325_slim_shutdown,
	.hw_params	= es325_slim_hw_params,
	.hw_free	= es325_slim_hw_free,
	.prepare	= es325_slim_prepare,
	.trigger	= es325_slim_trigger,
};

static int es325_slim_device_up(struct slim_device *sbdev);
static int es325_fw_thread(void *priv)
{
	struct es325_priv *es325 = (struct es325_priv  *)priv;

	do {
		slim_get_logical_addr(es325->gen0_client,
				      es325->gen0_client->e_addr,
				      6, &(es325->gen0_client->laddr));
		usleep_range(1000, 2000);
	} while (es325->gen0_client->laddr == 0xf0);
	dev_dbg(&es325->gen0_client->dev, "%s(): gen0_client LA = %d\n",
		__func__, es325->gen0_client->laddr);
	do {
		slim_get_logical_addr(es325->intf_client,
				      es325->intf_client->e_addr,
				      6, &(es325->intf_client->laddr));
		usleep_range(1000, 2000);
	} while (es325->intf_client->laddr == 0xf0);
	dev_dbg(&es325->intf_client->dev, "%s(): intf_client LA = %d\n",
		__func__, es325->intf_client->laddr);

	es325_slim_device_up(es325->gen0_client);
	return 0;
}

static int es325_slim_probe(struct slim_device *sbdev)
{
	struct esxxx_platform_data *pdata = sbdev->dev.platform_data;
	const char *filename = "audience-es325-fw.bin";
	int rc;
	struct task_struct *thread = NULL;

	dev_dbg(&sbdev->dev, "%s(): sbdev->name = %s\n", __func__, sbdev->name);

	if (strcmp(sbdev->name, "es325-codec-intf") == 0) {
		dev_dbg(&sbdev->dev, "%s(): interface device probe\n",
			__func__);
		es325_priv.intf_client = sbdev;
	}
	if (strcmp(sbdev->name, "es325-codec-gen0") == 0) {
		dev_dbg(&sbdev->dev, "%s(): generic device probe\n",
			__func__);
		es325_priv.gen0_client = sbdev;
	}

	if (es325_priv.intf_client == NULL ||
	    es325_priv.gen0_client == NULL) {
		dev_dbg(&sbdev->dev, "%s() incomplete initialization\n",
			__func__);
		return 0;
	}
	if (pdata == NULL) {
		dev_err(&sbdev->dev, "%s(): pdata is NULL", __func__);
		rc = -EIO;
		goto pdata_error;
	}

	slim_set_clientdata(sbdev, &es325_priv);

	es325_priv.intf = ES325_SLIM_INTF;
	es325_priv.dev_read = es325_slim_read;
	es325_priv.dev_write = es325_slim_write;
	es325_priv.dev = &es325_priv.gen0_client->dev;

#if !defined(ES325_DEVICE_UP)
	thread = kthread_run(es325_fw_thread, &es325_priv, "audience thread");
	if (IS_ERR(thread)) {
		dev_err(&sbdev->dev,
			"%s(): can't create es325 firmware thread = %p\n",
			__func__, thread);
		return -EPERM;
	}
#endif

	return 0;

pdata_error:
	dev_dbg(&sbdev->dev, "%s(): exit with error\n", __func__);
	return rc;
}

static int es325_slim_remove(struct slim_device *sbdev)
{
	struct esxxx_platform_data *pdata = sbdev->dev.platform_data;

	dev_dbg(&sbdev->dev, "%s(): sbdev->name = %s\n", __func__, sbdev->name);

	if (es325_priv.gen0_client != sbdev)
		return 0;

	gpio_free(pdata->reset_gpio);
	gpio_free(pdata->gpioa_gpio);
	gpio_free(pdata->gpiob_gpio);

	snd_soc_unregister_codec(&sbdev->dev);

	return 0;
}

static int es325_slim_device_up(struct slim_device *sbdev)
{
	struct es325_priv *priv;
	int rc;

	dev_dbg(&sbdev->dev, "%s: name=%s\n", __func__, sbdev->name);
	dev_dbg(&sbdev->dev, "%s: laddr=%d\n", __func__, sbdev->laddr);
	/* Start the firmware download in the workqueue context. */
	priv = slim_get_devicedata(sbdev);
	if (strcmp(sbdev->name, "es325-codec-intf") == 0)
		return 0;

	rc = register_snd_soc(priv);
	BUG_ON(rc != 0);

	return rc;
}

static int es325_slim_suspend(struct device *dev)
{
	dev_dbg(dev, "%s()\n", __func__);
	return 0;
}

static int es325_slim_resume(struct device *dev)
{
	dev_dbg(dev, "%s()\n", __func__);
	return 0;
}

static int es325_slim_runtime_suspend(struct device *dev)
{
	dev_dbg(dev, "%s()\n", __func__);
	return 0;
}

static int es325_slim_runtime_resume(struct device *dev)
{
	dev_dbg(dev, "%s()\n", __func__);
	return 0;
}

static int es325_slim_runtime_idle(struct device *dev)
{
	dev_dbg(dev, "%s()\n", __func__);
	return 0;
}

static const struct dev_pm_ops es325_slim_dev_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(
		es325_slim_suspend,
		es325_slim_resume
	)
	SET_RUNTIME_PM_OPS(
		es325_slim_runtime_suspend,
		es325_slim_runtime_resume,
		es325_slim_runtime_idle
	)
};

static const struct slim_device_id es325_slim_id[] = {
	{ "es325-codec", 0 },
	{ "es325-codec-intf", 0 },
	{ "es325-codec-gen0", 0 },
	{  }
};

MODULE_DEVICE_TABLE(slim, es325_slim_id);

struct slim_driver es325_slim_driver = {
	.driver = {
		.name = "es325-codec",
		.owner = THIS_MODULE,
		.pm = &es325_slim_dev_pm_ops,
	},
	.probe = es325_slim_probe,
	.remove = es325_slim_remove,
#if defined(ES325_DEVICE_UP)
	.device_up = es325_slim_device_up,
#endif
	.id_table = es325_slim_id,
};

MODULE_DESCRIPTION("ASoC ES325 driver");
MODULE_AUTHOR("Greg Clemson <gclemson@audience.com>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:es325-codec");
