/*
 *  mtk_auxadc_intf.h
 *  Include header file to MTK AUXADC Interface
 *
 *  Copyright (C) 2015 Richtek Technology Corp.
 *  Jeff Chang <jeff_chang@richtek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#ifndef __LINUX_MTK_AUXADC_INTF_H
#define __LINUX_MTK_AUXADC_INTF_H
#include <mach/upmu_hw.h>

#define VOLTAGE_FULL_RANGE	(1800)

/* =========== User Layer ================== */
/* pmic_get_auxadc_value
 * if return value < 0 -> means get data fail */
extern int pmic_get_auxadc_value(u8 list);
extern const char *pmic_get_auxadc_name(u8 list);

enum {
	/* mt6335 */
	AUXADC_LIST_BATADC,
	AUXADC_LIST_MT6335_START = AUXADC_LIST_BATADC,
	AUXADC_LIST_VCDT,
	AUXADC_LIST_BATTEMP_35,
	AUXADC_LIST_BATID_35,
	AUXADC_LIST_VBIF,
	AUXADC_LIST_MT6335_CHIP_TEMP,
	AUXADC_LIST_DCXO,
	AUXADC_LIST_TSX,
	AUXADC_LIST_MT6335_END = AUXADC_LIST_TSX,
	/* mt6336*/
	AUXADC_LIST_BATSNS,
	AUXADC_LIST_MT6336_START = AUXADC_LIST_BATSNS,
	AUXADC_LIST_VBUS,
	AUXADC_LIST_BATTEMP_36,
	AUXADC_LIST_MT6336_CHIP_TEMP,
	AUXADC_LIST_TYPEC_CCDETECT,
	AUXADC_LIST_BATID_36,
	AUXADC_LIST_VLED1,
	AUXADC_LIST_VLED2,
	AUXADC_LIST_MT6336_END = AUXADC_LIST_VLED2,
	/* mt6337 */
	AUXADC_LIST_BATSNS_37,
	AUXADC_LIST_MT6337_START = AUXADC_LIST_BATSNS_37,
	AUXADC_LIST_MT6337_CHIP_TEMP,
	AUXADC_LIST_ACCDET,
	AUXADC_LIST_HPOFS_CAL,
	AUXADC_LIST_MT6337_END = AUXADC_LIST_HPOFS_CAL,
};

/* channel_rdy : AUXADC ready bit
 * channel_out : AUXCAD out data
 * resolution : AUXADC resolution
 * r_val : AUXADC channel R value
 */
struct pmic_auxadc_channel {
	u8 resolution;
	u8 r_val;
	unsigned int channel_rqst;
	unsigned int channel_rdy;
	unsigned int channel_out;
};

enum {
	AUXADC_DUMP,
	AUXADC_CHANNEL,
	AUXADC_VALUE,
	AUXADC_REGS,
};

struct mtk_auxadc_ops {
	void (*lock)(void);
	void (*unlock)(void);
	void (*dump_regs)(char *buf);
	int (*get_channel_value)(u8 channel);
};

struct mtk_auxadc_intf {
	struct mtk_auxadc_ops *ops;
	char *name;
	int channel_num;
	const char **channel_name;
	int dbg_chl;
	int dbg_md_chl;
	int reg;
	int data;
};

#ifdef CONFIG_MTK_PMIC_CHIP_MT6335
extern void mt6335_auxadc_init(void);
extern void mt6335_auxadc_dump_regs(char *buf);
extern int mt6335_get_auxadc_value(u8 channel);
#endif /* CONFIG_MTK_PMIC_CHIP_MT6335 */

#ifdef CONFIG_MTK_PMIC_CHIP_MT6336
extern void mt6336_auxadc_init(void);
extern void mt6336_auxadc_dump_regs(char *buf);
extern int mt6336_get_auxadc_value(u8 channel);
#endif /* CONFIG_MTK_PMIC_CHIP_MT6336 */

#ifdef CONFIG_MTK_PMIC_CHIP_MT6337
extern void mt6337_auxadc_init(void);
extern void mt6337_auxadc_dump_regs(char *buf);
extern int mt6337_get_auxadc_value(u8 channel);
#endif /* CONFIG_MTK_PMIC_CHIP_MT6337 */

/* ============ kernel Layer =================== */
extern void mtk_auxadc_init(void);
extern int register_mtk_auxadc_intf(struct mtk_auxadc_intf *intf);

#endif /* __LINUX_MTK_AUXADC_INTF_H */
