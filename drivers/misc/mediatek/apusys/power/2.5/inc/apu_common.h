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

const char *apu_dev_string(enum DVFS_USER user);
enum DVFS_USER apu_dev_user(const char *name);
struct apu_dev *apu_find_device(enum DVFS_USER user);
int apu_add_devfreq(struct apu_dev *add_dev);
int apu_del_devfreq(struct apu_dev *add_dev);
int apu_boost2opp(struct apu_dev *adev, int boost);
ulong apu_boost2freq(struct apu_dev *adev, int boost);
int apu_opp2freq(struct apu_dev *adev, int opp);
int apu_freq2opp(struct apu_dev *adev, unsigned long freq);
int apu_create_child_array(struct apu_gov_data *pgov_data);
void apu_release_child_array(struct apu_gov_data *pgov_data);

#endif
