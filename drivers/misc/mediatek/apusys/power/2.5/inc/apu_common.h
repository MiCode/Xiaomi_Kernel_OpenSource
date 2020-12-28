/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __APU_POWER_H__
#define __APU_POWER_H__
#include "apu_devfreq.h"
#include "apu_gov.h"

#define	KHZ	(1000)
#define	MHZ	(1000*KHZ)

#define	TOMHZ(x)	(x / MHZ)
#define	TOKHZ(x)	(x / KHZ)

#define	MV	(1000)
#define	TOMV(x)	(x / MV)

/* round_khz, and input unit is hz */
static inline bool round_khz(unsigned long x, unsigned long y)
{
	return (abs((x) - (y)) < KHZ);
}

/* round_Mhz, and input unit is hz */
static inline bool round_Mhz(unsigned long x, unsigned long y)
{
	return (abs((x) - (y)) < MHZ);
}

static inline const char *apu_dev_name(struct device *dev)
{
	return ((struct apu_dev *)dev_get_drvdata(dev))->name;
}

const char *apu_dev_string(enum DVFS_USER user);
enum DVFS_USER apu_dev_user(const char *name);
struct apu_dev *apu_find_device(enum DVFS_USER user);
int apu_add_devfreq(struct apu_dev *ad);
int apu_del_devfreq(struct apu_dev *ad);
int apu_boost2opp(struct apu_dev *ad, int boost);
int apu_boost2freq(struct apu_dev *ad, int boost);
int apu_opp2freq(struct apu_dev *ad, int opp);
int apu_opp2boost(struct apu_dev *ad, int opp);
int apu_freq2opp(struct apu_dev *ad, unsigned long freq);
int apu_freq2boost(struct apu_dev *ad, unsigned long freq);
int apu_volt2opp(struct apu_dev *ad, int volt);
int apu_volt2boost(struct apu_dev *ad, int volt);
int apu_get_recommend_freq_volt(struct device *dev, unsigned long *freq,
				unsigned long *volt, int flag);
#endif
