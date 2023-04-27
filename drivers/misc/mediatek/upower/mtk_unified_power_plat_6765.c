// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2015 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/spinlock.h>
#include <linux/rcupdate.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/export.h>
#include <linux/module.h>
#include <linux/ktime.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/nvmem-consumer.h>
#include <linux/of_platform.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/sched.h>
#include <linux/types.h>

/* local include */
#include "inc/mtk_cpufreq_api.h"
#include "mtk_cpufreq_config.h"
#include "mtk_unified_power.h"
#include "mtk_unified_power_data.h"
#include "mtk_devinfo.h"
#include "mtk_static_power.h"

#ifndef EARLY_PORTING_SPOWER
#include "mtk_common_static_power.h"
#endif

#undef  BIT
#define BIT(bit)	(1U << (bit))

#define MSB(range)	(1 ? range)
#define LSB(range)	(0 ? range)
/**
 * Genearte a mask wher MSB to LSB are all 0b1
 * @r:	Range in the form of MSB:LSB
 */
#define BITMASK(r)	\
	(((unsigned int) -1 >> (31 - MSB(r))) & ~((1U << LSB(r)) - 1))

/**
 * Set value at MSB:LSB. For example, BITS(7:3, 0x5A)
 * will return a value where bit 3 to bit 7 is 0x5A
 * @r:	Range in the form of MSB:LSB
 */
/* BITS(MSB:LSB, value) => Set value at MSB:LSB  */
#define BITS(r, val)	((val << LSB(r)) & BITMASK(r))

#define GET_BITS_VAL(_bits_, _val_)   \
	(((_val_) & (BITMASK(_bits_))) >> ((0) ? _bits_))
/* #if (NR_UPOWER_TBL_LIST <= 1) */
struct upower_tbl final_upower_tbl[NR_UPOWER_BANK] = {};
/* #endif */

int degree_set[NR_UPOWER_DEGREE] = {
		UPOWER_DEGREE_0,
		UPOWER_DEGREE_1,
		UPOWER_DEGREE_2,
		UPOWER_DEGREE_3,
		UPOWER_DEGREE_4,
		UPOWER_DEGREE_5,
};

/* collect all the raw tables */
#define INIT_UPOWER_TBL_INFOS(name, tbl) {__stringify(name), &tbl}
struct upower_tbl_info upower_tbl_list[NR_UPOWER_TBL_LIST][NR_UPOWER_BANK] = {
	/* FY */
	[0] = {
	INIT_UPOWER_TBL_INFOS(UPOWER_BANK_L, upower_tbl_l_FY),
	INIT_UPOWER_TBL_INFOS(UPOWER_BANK_LL, upower_tbl_ll_FY),
	INIT_UPOWER_TBL_INFOS(UPOWER_BANK_CLS_L, upower_tbl_cluster_l_FY),
	INIT_UPOWER_TBL_INFOS(UPOWER_BANK_CLS_LL, upower_tbl_cluster_ll_FY),
	INIT_UPOWER_TBL_INFOS(UPOWER_BANK_CCI, upower_tbl_cci_FY),
	},
	/* SB */
	[1] = {
	INIT_UPOWER_TBL_INFOS(UPOWER_BANK_L, upower_tbl_l_SB),
	INIT_UPOWER_TBL_INFOS(UPOWER_BANK_LL, upower_tbl_ll_SB),
	INIT_UPOWER_TBL_INFOS(UPOWER_BANK_CLS_L, upower_tbl_cluster_l_SB),
	INIT_UPOWER_TBL_INFOS(UPOWER_BANK_CLS_LL, upower_tbl_cluster_ll_SB),
	INIT_UPOWER_TBL_INFOS(UPOWER_BANK_CCI, upower_tbl_cci_SB),
	},
	/* C65T */
	[2] = {
	INIT_UPOWER_TBL_INFOS(UPOWER_BANK_L, upower_tbl_l_C65T),
	INIT_UPOWER_TBL_INFOS(UPOWER_BANK_LL, upower_tbl_ll_C65T),
	INIT_UPOWER_TBL_INFOS(UPOWER_BANK_CLS_L, upower_tbl_cluster_l_C65T),
	INIT_UPOWER_TBL_INFOS(UPOWER_BANK_CLS_LL, upower_tbl_cluster_ll_C65T),
	INIT_UPOWER_TBL_INFOS(UPOWER_BANK_CCI, upower_tbl_cci_C65T),
	},
	/* C65 */
	[3] = {
	INIT_UPOWER_TBL_INFOS(UPOWER_BANK_L, upower_tbl_l_C65),
	INIT_UPOWER_TBL_INFOS(UPOWER_BANK_LL, upower_tbl_ll_C65),
	INIT_UPOWER_TBL_INFOS(UPOWER_BANK_CLS_L, upower_tbl_cluster_l_C65),
	INIT_UPOWER_TBL_INFOS(UPOWER_BANK_CLS_LL, upower_tbl_cluster_ll_C65),
	INIT_UPOWER_TBL_INFOS(UPOWER_BANK_CCI, upower_tbl_cci_C65),
	},
	/* C62 */
	[4] = {
	INIT_UPOWER_TBL_INFOS(UPOWER_BANK_L, upower_tbl_l_C62),
	INIT_UPOWER_TBL_INFOS(UPOWER_BANK_LL, upower_tbl_ll_C62),
	INIT_UPOWER_TBL_INFOS(UPOWER_BANK_CLS_L, upower_tbl_cluster_l_C62),
	INIT_UPOWER_TBL_INFOS(UPOWER_BANK_CLS_LL, upower_tbl_cluster_ll_C62),
	INIT_UPOWER_TBL_INFOS(UPOWER_BANK_CCI, upower_tbl_cci_C62),
	},
	/* C62LY */
	[5] = {
	INIT_UPOWER_TBL_INFOS(UPOWER_BANK_L, upower_tbl_l_C62),
	INIT_UPOWER_TBL_INFOS(UPOWER_BANK_LL, upower_tbl_ll_C62),
	INIT_UPOWER_TBL_INFOS(UPOWER_BANK_CLS_L, upower_tbl_cluster_l_C62),
	INIT_UPOWER_TBL_INFOS(UPOWER_BANK_CLS_LL, upower_tbl_cluster_ll_C62),
	INIT_UPOWER_TBL_INFOS(UPOWER_BANK_CCI, upower_tbl_cci_C62),
	},
	/* C65R */
	[6] = {
	INIT_UPOWER_TBL_INFOS(UPOWER_BANK_L, upower_tbl_l_C65R),
	INIT_UPOWER_TBL_INFOS(UPOWER_BANK_LL, upower_tbl_ll_C65R),
	INIT_UPOWER_TBL_INFOS(UPOWER_BANK_CLS_L, upower_tbl_cluster_l_C65R),
	INIT_UPOWER_TBL_INFOS(UPOWER_BANK_CLS_LL, upower_tbl_cluster_ll_C65R),
	INIT_UPOWER_TBL_INFOS(UPOWER_BANK_CCI, upower_tbl_cci_C65R),
	},
	/* C62D */
	[7] = {
	INIT_UPOWER_TBL_INFOS(UPOWER_BANK_L, upower_tbl_l_C62D),
	INIT_UPOWER_TBL_INFOS(UPOWER_BANK_LL, upower_tbl_ll_C62D),
	INIT_UPOWER_TBL_INFOS(UPOWER_BANK_CLS_L, upower_tbl_cluster_l_C62D),
	INIT_UPOWER_TBL_INFOS(UPOWER_BANK_CLS_LL, upower_tbl_cluster_ll_C62D),
	INIT_UPOWER_TBL_INFOS(UPOWER_BANK_CCI, upower_tbl_cci_C62D),
	},
	/* C62DLY */
	[8] = {
	INIT_UPOWER_TBL_INFOS(UPOWER_BANK_L, upower_tbl_l_C62DLY),
	INIT_UPOWER_TBL_INFOS(UPOWER_BANK_LL, upower_tbl_ll_C62DLY),
	INIT_UPOWER_TBL_INFOS(UPOWER_BANK_CLS_L, upower_tbl_cluster_l_C62DLY),
	INIT_UPOWER_TBL_INFOS(UPOWER_BANK_CLS_LL, upower_tbl_cluster_ll_C62DLY),
	INIT_UPOWER_TBL_INFOS(UPOWER_BANK_CCI, upower_tbl_cci_C62DLY),
	},
	/* C65OD */
	[9] = {
	INIT_UPOWER_TBL_INFOS(UPOWER_BANK_L, upower_tbl_l_C65OD),
	INIT_UPOWER_TBL_INFOS(UPOWER_BANK_LL, upower_tbl_ll_C65OD),
	INIT_UPOWER_TBL_INFOS(UPOWER_BANK_CLS_L, upower_tbl_cluster_l_C65OD),
	INIT_UPOWER_TBL_INFOS(UPOWER_BANK_CLS_LL, upower_tbl_cluster_ll_C65OD),
	INIT_UPOWER_TBL_INFOS(UPOWER_BANK_CCI, upower_tbl_cci_C65OD),
	},
	/* C65X */
	[10] = {
	INIT_UPOWER_TBL_INFOS(UPOWER_BANK_L, upower_tbl_l_C65X),
	INIT_UPOWER_TBL_INFOS(UPOWER_BANK_LL, upower_tbl_ll_C65X),
	INIT_UPOWER_TBL_INFOS(UPOWER_BANK_CLS_L, upower_tbl_cluster_l_C65X),
	INIT_UPOWER_TBL_INFOS(UPOWER_BANK_CLS_LL, upower_tbl_cluster_ll_C65X),
	INIT_UPOWER_TBL_INFOS(UPOWER_BANK_CCI, upower_tbl_cci_C65X),
	},
};
/* Upower will know how to apply voltage that comes from EEM */
unsigned char upower_recognize_by_eem[NR_UPOWER_BANK] = {
	UPOWER_BANK_L, /* L EEM apply voltage to L upower bank */
	UPOWER_BANK_LL, /* LL EEM apply voltage to LL upower bank */
	UPOWER_BANK_L, /* L EEM apply voltage to CLS_L upower bank */
	UPOWER_BANK_LL, /* LL EEM apply voltage to CLS_LL upower bank */
	UPOWER_BANK_CCI, /* CCI EEM apply voltage to CCI upower bank */
};

/* Used for rcu lock, points to all the raw tables list*/
struct upower_tbl_info *p_upower_tbl_infos = &upower_tbl_list[0][0];

#ifndef EARLY_PORTING_SPOWER
int upower_bank_to_spower_bank(int upower_bank)
{
	int ret;

	switch (upower_bank) {
	case UPOWER_BANK_L:
		ret = MTK_SPOWER_CPUL;
		break;
	case UPOWER_BANK_LL:
		ret = MTK_SPOWER_CPULL;
		break;
	case UPOWER_BANK_CLS_L:
		ret = MTK_SPOWER_CPUL_CLUSTER;
		break;
	case UPOWER_BANK_CLS_LL:
		ret = MTK_SPOWER_CPULL_CLUSTER;
		break;
	case UPOWER_BANK_CCI:
		ret = MTK_SPOWER_CCI;
		break;
	default:
		ret = -1;
		break;
	}
	return ret;
}
#endif

/****************************************************
 * According to chip version get the raw upower tbl *
 * and let upower_tbl_infos points to it.           *
 * Choose a non used upower tbl location and let    *
 * upower_tbl_ref points to it to store target      *
 * power tbl.                                       *
 ***************************************************/

int cpu_cluster_mapping(unsigned int cpu)
{
	enum upower_bank bank = UPOWER_BANK_LL;

	if (cpu < 4) /* cpu 0-3 */
		bank = UPOWER_BANK_L;
	else if (cpu < 8) /* cpu 4-7 */
		bank = UPOWER_BANK_LL;

	return bank;
}

unsigned int cpufreq_get_cpu_level_upower(void)
{
	unsigned int lv = CPU_LEVEL_3;
	unsigned int efuse_seg;
	unsigned int efuse_ly;
	struct platform_device *pdev;
	struct device_node *node;
	struct nvmem_cell *efuse_cell;
	size_t efuse_len;
	unsigned int *efuse_buf;
	unsigned int *efuse_ly_buf;
	int val = 0;
	int val_ly = 0;
	unsigned int fabinfo2;

	node = of_find_compatible_node(NULL, NULL, "mediatek,mt6765-dvfsp");

	if (!node) {
		upower_error("%s fail to get device node\n", __func__);
		return 0;
	}
	pdev = of_device_alloc(node, NULL, NULL);
	if (!pdev) {
		upower_error("%s fail to create device node\n", __func__);
		return 0;
	}
	efuse_cell = nvmem_cell_get(&pdev->dev, "efuse_segment_cell");
	if (IS_ERR(efuse_cell)) {
		upower_error("@%s: cannot get efuse_cell, errno %ld\n",
			__func__, PTR_ERR(efuse_cell));
		return 0;
	}

	efuse_buf = (unsigned int *)nvmem_cell_read(efuse_cell, &efuse_len);
	nvmem_cell_put(efuse_cell);
	efuse_seg = *efuse_buf;
	val = efuse_seg;
	kfree(efuse_buf);

	/* get efuse ly */
	efuse_cell = nvmem_cell_get(&pdev->dev, "efuse_ly_cell");
	if (IS_ERR(efuse_cell)) {
		upower_error("@%s: cannot get efuse_ly_cell, errno %ld\n",
			__func__, PTR_ERR(efuse_cell));
		return 0;
	}

	efuse_ly_buf = (unsigned int *)nvmem_cell_read(efuse_cell, &efuse_len);
	nvmem_cell_put(efuse_cell);
	efuse_ly = *efuse_ly_buf;
	val_ly = (efuse_ly >> 1) & 0x1;
	fabinfo2 = (efuse_ly >> 3) & 0x1;
	kfree(efuse_ly_buf);

	if ((val == 0x2) || (val == 0x5))
		lv = CPU_LEVEL_2;
	else if ((val == 0x4) || (val == 0x3))
		lv = CPU_LEVEL_3;
	else if ((val == 0x1) || (val == 0x7) || (val == 0x19))
		lv = CPU_LEVEL_4;
	else if ((val == 0x8) || (val == 0x9) || (val == 0xF))
		lv = CPU_LEVEL_4;
	else if (val == 0x14)
		lv = CPU_LEVEL_6;
	else if (val == 0x20)
		lv = CPU_LEVEL_7;
	else
		lv = CPU_LEVEL_3;

	if (val_ly == 0x1) {
		if (val == 0x20)
			lv = CPU_LEVEL_8;
		else
			lv = CPU_LEVEL_5;
	}

	/* for MT6765X */
	if (val == 0x24)
		lv = CPU_LEVEL_10;


	/* for improve yield MT6765OD */
	if ((val == 0x3) || (val == 0x4) || (val == 0x12)) {
		if (fabinfo2 == 1)
			lv = CPU_LEVEL_9;
	}

	/* free pdev */
	if (pdev != NULL) {
		of_platform_device_destroy(&pdev->dev, NULL);
		of_dev_put(pdev);
	}

	upower_info("CPU level: %d\n", lv);
	return lv;
}

void get_original_table(void)
{
	/* unsigned int bin = 0; */
	unsigned short idx = 0; /* default use FY table */
	unsigned int i, j;

	idx = cpufreq_get_cpu_level_upower();

	if (idx >= NR_UPOWER_TBL_LIST)
		idx = 0;

	/* get location of reference table */
	upower_tbl_infos = &upower_tbl_list[idx][0];

	/* get location of target table */
	upower_tbl_ref = &final_upower_tbl[0];

	upower_debug("idx %d dest:%p, src:%p\n",
			(idx+1)%NR_UPOWER_TBL_LIST, upower_tbl_ref, upower_tbl_infos);

	/* If disable upower, ptr will point to original upower table */
	p_upower_tbl_infos = upower_tbl_infos;

	/*
	 *  Clear volt fields before eem run.
	 *  If eem is enabled, it will apply volt into it. If eem is disabled,
	 *  the values of volt are 0 , and upower will apply orig volt into it
	 */
	for (i = 0; i < NR_UPOWER_BANK; i++) {
		for (j = 0; j < UPOWER_OPP_NUM; j++)
			upower_tbl_ref[i].row[j].volt = 0;
	}
	for (i = 0; i < NR_UPOWER_BANK; i++)
		upower_debug("bank[%d] dest:%p dyn_pwr:%u, volt[0]%u\n",
			i, &upower_tbl_ref[i],
			upower_tbl_ref[i].row[0].dyn_pwr,
			upower_tbl_ref[i].row[0].volt);

}

MODULE_DESCRIPTION("MediaTek Unified Power Driver v0.0");
MODULE_LICENSE("GPL");
