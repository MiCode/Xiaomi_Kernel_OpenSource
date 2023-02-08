/**
 * Copyright (C) Fourier Semiconductor Inc. 2016-2020. All rights reserved.
 * 2020-04-29 File created.
 */

#ifndef __FS15XX_H__
#define __FS15XX_H__

#include <sound/core.h>
#include <sound/soc.h>
#include <linux/gpio.h>
#include <linux/hrtimer.h>
#include <linux/version.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/power_supply.h>

#define FS15XX_PINCTRL_SUPPORT
#define FSM_CHECK_PLUSE_TIME
// #define FS15XX_USE_HRTIMER
#define FS15XX_MULTI_TYPE_CHECK

#define FS15XX_GPIO_LOW     0
#define FS15XX_GPIO_HIGH    1
#define FS15XX_OFF_MODE     0
#define FS15XX_MIN_MODE     0
#define FS15XX_MAX_MODE     6
#define SPC1910_DEFAULT_MODE (4)
#define FS15XX_DEFAULT_MODE  (4)
#define X102_DEFAULT_MODE    (9)
#define XX318_DEFAULT_MODE   (6)
#define FS15XX_DRV_VERSION  "V1.1.2"
#define FS15XX_CODE_DATE    "20220809"
#define FS15XX_DRV_NAME     "fs15xx"

#define SPC1910_HDR_PULSES    (15)
#define FS15XX_PULSE_DELAY_US (10)
#define FS15XX_RETRY          (10)
#define FS15XX_MONITOR_PEROID (500) // ms

#define FS15XX_IOC_MAGIC      (0x7D)
#define FS15XX_IOC_GET_VBAT       _IOR(FS15XX_IOC_MAGIC, 1, int)

enum fs15xx_dev_type {
	FS15XX_DEV_1910 = 0,
	FS15XX_DEV_15XX,
/* Add fs1512 & x102 & xx318/xx358 pa type check logic : B2 & C3 */
#ifdef FS15XX_MULTI_TYPE_CHECK
	FS15XX_DEV_X102,
	FS15XX_DEV_XX318,
#endif
	FS15XX_DEV_MAX
};

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 19, 0))
#define snd_soc_codec              snd_soc_component
#define snd_soc_add_codec_controls snd_soc_add_component_controls
#endif

struct fs15xx_dev {
#ifdef FS15XX_USE_HRTIMER
	struct hrtimer timer;
	struct work_struct monitor_work;
	atomic_t running;
#endif
	spinlock_t fs15xx_lock;
	ktime_t last_time;
	ktime_t cur_time;
	bool fs15xx_timeout;
	int dev_type;
	int fs15xx_mode;
#ifdef FS15XX_PINCTRL_SUPPORT
	struct pinctrl *pinctrl;
	struct pinctrl_state *fs15xx_id_default;
	struct pinctrl_state *fs15xx_cmd_default;
	struct pinctrl_state *fs15xx_mod_default;
#endif
	u32 gpio_id;
	u32 gpio_cmd;
	u32 gpio_mod;
};
typedef struct fs15xx_dev fs15xx_dev_t;

int fs15xx_set_timer(bool enable);
int fs15xx_ext_amp_set(int enable);
int fs15xx_set_mode_simple(int mode);
int spc19xx_set_mode_simple(int mode);
int fs15xx_set_ext_amp(struct snd_soc_codec *codec, int enable);
void fs15xx_add_card_kcontrol(struct snd_soc_card *card);
void fs15xx_add_codec_kcontrol(struct snd_soc_codec *codec);
#ifdef FSM_UNUSED_CODE
void fs15xx_add_platform_kcontrol(struct snd_soc_platform *platform);
#endif

#endif
