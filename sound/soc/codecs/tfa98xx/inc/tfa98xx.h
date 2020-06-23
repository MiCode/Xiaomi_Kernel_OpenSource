/*
 * Copyright (C) 2014 NXP Semiconductors, All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#ifndef __TFA98XX_INC__
#define __TFA98XX_INC__

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/list.h>

#include "tfa_device.h"
#include "tfa_container.h"
#include "config.h"

/* max. length of a alsa mixer control name */
#define MAX_CONTROL_NAME 48

#define TFA98XX_MAX_REGISTER 0xff

#define TFA98XX_FLAG_SKIP_INTERRUPTS (1 << 0)
#define TFA98XX_FLAG_SAAM_AVAILABLE (1 << 1)
#define TFA98XX_FLAG_STEREO_DEVICE (1 << 2)
#define TFA98XX_FLAG_MULTI_MIC_INPUTS (1 << 3)
#define TFA98XX_FLAG_TAPDET_AVAILABLE (1 << 4)
#define TFA98XX_FLAG_CALIBRATION_CTL (1 << 5)
#define TFA98XX_FLAG_REMOVE_PLOP_NOISE (1 << 6)
#define TFA98XX_FLAG_LP_MODES (1 << 7)
#define TFA98XX_FLAG_TDM_DEVICE (1 << 8)

#define TFA98XX_NUM_RATES 9

/* DSP init status */
enum tfa98xx_dsp_init_state {
	TFA98XX_DSP_INIT_STOPPED,     /* DSP not running */
	TFA98XX_DSP_INIT_RECOVER,     /* DSP error detected at runtime */
	TFA98XX_DSP_INIT_FAIL,	/* DSP init failed */
	TFA98XX_DSP_INIT_PENDING,     /* DSP start requested */
	TFA98XX_DSP_INIT_DONE,	/* DSP running */
	TFA98XX_DSP_INIT_INVALIDATED, /* DSP was running, requires re-init */
};

enum tfa98xx_dsp_fw_state {
	TFA98XX_DSP_FW_NONE = 0,
	TFA98XX_DSP_FW_PENDING,
	TFA98XX_DSP_FW_FAIL,
	TFA98XX_DSP_FW_OK,
};

enum TFA_DEVICE_TYPE {
	TFA_DEVICE_TYPE_9894,
	TFA_DEVICE_TYPE_9874_PRIMARY,
	TFA_DEVICE_TYPE_9874_SECONDARY,
	TFA_DEVICE_TYPE_MAX
};

enum TFA_DEVICE_MUTE {
	TFA98XX_DEVICE_MUTE_OFF = 0,
	TFA98XX_DEVICE_MUTE_ON,
};

enum {
	IOCTL_CMD_GET_MEMTRACK_DATA = 0,
	IOCTL_CMD_GET_CNT_VERSION,
};

enum {
	MEMTRACK_ITEM_SPEAKER_F0 = 0,
	MEMTRACK_ITEM_SPEAKER_TEMPERATURE,
	MEMTRACK_ITEM_SPEAKER_IMPEDANCE,
	MEMTRACK_ITEM_MAX
};

struct livedata_cfg {
	int address;
	int track;
	int scaler;
};

struct tfa98xx_firmware {
	void *base;
	struct tfa98xx_device *dev;
	char name[9]; // TODO get length from tfa parameter defs
};

struct tfa98xx_baseprofile {
	char basename[MAX_CONTROL_NAME];    /* profile basename */
	int len;			    /* profile length */
	int item_id;			    /* profile id */
	int sr_rate_sup[TFA98XX_NUM_RATES]; // sample rates supported by this
					    // profile
	struct list_head list; /* list of all profiles */
};

struct tfa98xx {
	struct regmap *regmap;
	struct i2c_client *i2c;
	struct regulator *vdd;
	struct snd_soc_component *codec;
	struct workqueue_struct *tfa98xx_wq;
	struct delayed_work init_work;
	struct delayed_work monitor_work;
	struct delayed_work interrupt_work;
	struct delayed_work tapdet_work;
	struct mutex dsp_lock;
	int dsp_init;
	int dsp_fw_state;
	int sysclk;
	int rst_gpio;
	u16 rev;
	int audio_mode;
	struct tfa98xx_firmware fw;
	char *fw_name;
	int rate;
	wait_queue_head_t wq;
	struct device *dev;
	unsigned int init_count;
	int pstream;
	int cstream;
	struct input_dev *input;
	bool tapdet_enabled;	  /* service enabled */
	bool tapdet_open;	     /* device file opened */
	unsigned int tapdet_profiles; /* tapdet profile bitfield */
	bool tapdet_poll;	     /* tapdet running on polling mode */

	unsigned int rate_constraint_list[TFA98XX_NUM_RATES];
	struct snd_pcm_hw_constraint_list rate_constraint;

	int reset_gpio;
	int power_gpio;
	int irq_gpio;

	struct list_head list;
	struct tfa_device *tfa;
	int vstep;
	int profile;
	int prof_vsteps[TFACONT_MAXPROFS]; // store vstep per profile (single
					   // device)

#ifdef CONFIG_DEBUG_FS
	struct dentry *dbg_dir;
#endif
	u8 reg;
	unsigned int flags;
	bool set_mtp_cal;
	uint16_t cal_data;
	enum TFA_DEVICE_MUTE tfa_mute_mode;
};

#endif /* __TFA98XX_INC__ */

