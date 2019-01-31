/*
 * Copyright (C) 2017 MediaTek Inc.

 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef __LINUX_MTK_AUXADC_INTF_H
#define __LINUX_MTK_AUXADC_INTF_H
#include <mach/upmu_hw.h>

#define VOLTAGE_FULL_RANGE	(1800)

/* =========== User Layer ================== */
/* pmic_get_auxadc_value
 * if return value < 0 -> means get data fail
 */
extern int pmic_get_auxadc_value(u8 list);
extern const char *pmic_get_auxadc_name(u8 list);

/* Move to include/mt-plat/<mtxxxx>/include/mach/mtk_pmic.h */
#if 0
enum {
#ifdef CONFIG_MTK_PMIC_CHIP_MT6355
	/* mt6355 */
	AUXADC_LIST_BATADC,
	AUXADC_LIST_MT6355_START = AUXADC_LIST_BATADC,
	AUXADC_LIST_VCDT,
	AUXADC_LIST_BATTEMP,
	AUXADC_LIST_BATID,
	AUXADC_LIST_VBIF,
	AUXADC_LIST_MT6355_CHIP_TEMP,
	AUXADC_LIST_DCXO,
	AUXADC_LIST_ACCDET,
	AUXADC_LIST_TSX,
	AUXADC_LIST_HPOFS_CAL,
	AUXADC_LIST_MT6355_END = AUXADC_LIST_HPOFS_CAL,
#endif
#ifdef CONFIG_MTK_PMIC_CHIP_MT6356
	/* mt6356 */
	AUXADC_LIST_BATADC,
	AUXADC_LIST_MT6356_START = AUXADC_LIST_BATADC,
	AUXADC_LIST_VCDT,
	AUXADC_LIST_BATTEMP,
	AUXADC_LIST_BATID,
	AUXADC_LIST_VBIF,
	AUXADC_LIST_MT6356_CHIP_TEMP,
	AUXADC_LIST_DCXO,
	AUXADC_LIST_ACCDET,
	AUXADC_LIST_TSX,
	AUXADC_LIST_HPOFS_CAL,
	AUXADC_LIST_ISENSE,
	AUXADC_LIST_MT6356_BUCK1_TEMP,
	AUXADC_LIST_MT6356_BUCK2_TEMP,
	AUXADC_LIST_MT6356_END = AUXADC_LIST_MT6356_BUCK2_TEMP,
#endif
#ifdef CONFIG_MTK_PMIC_CHIP_MT6357
	/* mt6357 */
	AUXADC_LIST_BATADC,
	AUXADC_LIST_MT6357_START = AUXADC_LIST_BATADC,
	AUXADC_LIST_VCDT,
	AUXADC_LIST_BATTEMP,
	AUXADC_LIST_BATID,
	AUXADC_LIST_VBIF,
	AUXADC_LIST_MT6357_CHIP_TEMP,
	AUXADC_LIST_DCXO,
	AUXADC_LIST_ACCDET,
	AUXADC_LIST_TSX,
	AUXADC_LIST_HPOFS_CAL,
	AUXADC_LIST_ISENSE,
	AUXADC_LIST_MT6357_BUCK1_TEMP,
	AUXADC_LIST_MT6357_BUCK2_TEMP,
	AUXADC_LIST_MT6357_END = AUXADC_LIST_MT6357_BUCK2_TEMP,
#endif
	AUXADC_LIST_MAX,
};
#endif

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

/* Move to include/mt-plat/<mtxxxx>/include/mach/mtk_pmic.h */
#if 0
extern void mt6355_auxadc_init(void);
extern void mt6355_auxadc_dump_regs(char *buf);
extern int mt6355_get_auxadc_value(u8 channel);
extern void mt6355_auxadc_lock(void);
extern void mt6355_auxadc_unlock(void);
extern int mt6355_auxadc_recv_batmp(void);
#endif /* CONFIG_MTK_PMIC_CHIP_MT6355 */

#if 0
extern void mt6356_auxadc_init(void);
extern void mt6356_auxadc_dump_regs(char *buf);
extern int mt6356_get_auxadc_value(u8 channel);
extern void pmic_auxadc_lock(void);
extern void pmic_auxadc_unlock(void);
#endif /* CONFIG_MTK_PMIC_CHIP_MT6356 */

#if 0
extern void mt6357_auxadc_init(void);
extern void mt6357_auxadc_dump_regs(char *buf);
extern int mt6357_get_auxadc_value(u8 channel);
extern void pmic_auxadc_lock(void);
extern void pmic_auxadc_unlock(void);
#endif /* CONFIG_MTK_PMIC_CHIP_MT6357 */

/* ============ kernel Layer =================== */
extern void mtk_auxadc_init(void);
extern int register_mtk_auxadc_intf(struct mtk_auxadc_intf *intf);

#endif /* __LINUX_MTK_AUXADC_INTF_H */
