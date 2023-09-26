/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#endif
#include <mt-plat/mtk_ccci_common.h>
#include "ccci_config.h"
#include "ccci_modem.h"
#include "ccci_bm.h"
#include "ccci_platform.h"
#include "modem_secure_base.h"

#ifdef FEATURE_USING_4G_MEMORY_API
#include <mt-plat/mtk_lpae.h>
#endif
#ifdef FEATURE_LOW_BATTERY_SUPPORT
#include <mach/upmu_sw.h>
#endif

#ifdef FEATURE_LOW_BATTERY_SUPPORT
#define TMC_CTRL_CMD_TX_POWER 10
#endif

#define TAG "plat"

int Is_MD_EMI_voilation(void)
{
	return 1;
}

unsigned long pericfg_base;
unsigned long infra_ao_base;
unsigned long infra_ao_mem_base;


size_t mt_secure_call(
		size_t arg0, size_t arg1, size_t arg2,
		size_t arg3, size_t r1, size_t r2, size_t r3)
{
	struct arm_smccc_res res;

	arm_smccc_smc(MTK_SIP_KERNEL_CCCI_CONTROL, arg0, arg1,
			arg2, arg3, r1, r2, r3, &res);

	return res.a0;
}

/*
 * when MD attached its codeviser for debuging, this bit will be set.
 * so CCCI should disable some checkings and
 * operations as MD may not respond to us.
 */
unsigned int ccci_get_md_debug_mode(struct ccci_modem *md)
{
	return 0;
}
EXPORT_SYMBOL(ccci_get_md_debug_mode);

#ifdef FEATURE_LOW_BATTERY_SUPPORT
static int ccci_md_low_power_notify(
	struct ccci_modem *md, enum LOW_POEWR_NOTIFY_TYPE type, int level)
{
	unsigned int md_throttle_cmd = 0;
	int ret = 0;

	CCCI_NORMAL_LOG(md->index, TAG,
		"low power notification type=%d, level=%d\n", type, level);

	switch (type) {
	case LOW_BATTERY:
		if (level <= LOW_BATTERY_LEVEL_2 &&
			level >= LOW_BATTERY_LEVEL_0) {
			md_throttle_cmd = TMC_CTRL_CMD_TX_POWER |
			LOW_BATTERY << 8 | level << 16;
		}
		break;
	case OVER_CURRENT:
		if (level <= BATTERY_OC_LEVEL_1 &&
			level >= BATTERY_OC_LEVEL_0) {
			md_throttle_cmd = TMC_CTRL_CMD_TX_POWER |
			OVER_CURRENT << 8 | level << 16;
		}
		break;
	default:
		break;
	};

	if (md_throttle_cmd)
		ret = exec_ccci_kern_func_by_md_id(MD_SYS1,
				ID_THROTTLING_CFG,
				(char *) &md_throttle_cmd, 4);

	if (ret || !md_throttle_cmd)
		CCCI_ERROR_LOG(md->index, TAG,
			"%s: error, ret=%d, t=%d l=%d\n",
			__func__, ret, type, level);
	return ret;
}

static void ccci_md_low_battery_cb(LOW_BATTERY_LEVEL level)
{
	int idx = 0;
	struct ccci_modem *md = NULL;

	for (idx = 0; idx < MAX_MD_NUM; idx++) {
		md = ccci_md_get_modem_by_id(idx);
		if (md != NULL)
			ccci_md_low_power_notify(md, LOW_BATTERY, level);
	}
}

static void ccci_md_over_current_cb(BATTERY_OC_LEVEL level)
{
	int idx = 0;
	struct ccci_modem *md = NULL;

	for (idx = 0; idx < MAX_MD_NUM; idx++) {
		md = ccci_md_get_modem_by_id(idx);
		if (md != NULL)
			ccci_md_low_power_notify(md, OVER_CURRENT, level);
	}
}
#endif

#define PCCIF_BUSY (0x4)
#define PCCIF_TCHNUM (0xC)
#define PCCIF_ACK (0x14)
#define PCCIF_CHDATA (0x100)
#define PCCIF_SRAM_SIZE (512)

void ccci_reset_ccif_hw(unsigned char md_id,
			int ccif_id, void __iomem *baseA, void __iomem *baseB)
{
	int i;
	struct ccci_smem_region *region = NULL;

	{
		int ccif0_reset_bit = 8;
		int ccif1_reset_bit = 7;

		if (ccif_id != AP_MD1_CCIF)
			return;

		/*
		 *this reset bit will clear
		 *CCIF's busy/wch/irq, but not SRAM
		 */
		/*set reset bit*/
		ccci_write32(infra_ao_base, 0x150,
						(1 << ccif0_reset_bit) |
						(1 << ccif1_reset_bit));

		/*clear reset bit*/
		ccci_write32(infra_ao_base, 0x154,
						(1 << ccif0_reset_bit) |
						(1 << ccif1_reset_bit));
	}

	/* clear SRAM */
	for (i = 0; i < PCCIF_SRAM_SIZE/sizeof(unsigned int); i++) {
		ccif_write32(baseA, PCCIF_CHDATA+i*sizeof(unsigned int), 0);
		ccif_write32(baseB, PCCIF_CHDATA+i*sizeof(unsigned int), 0);
	}

	/* extend from 36bytes to 72bytes in CCIF SRAM */
	/* 0~60bytes for bootup trace,
	 *last 12bytes for magic pattern,smem address and size
	 */
	region = ccci_md_get_smem_by_user_id(md_id,
		SMEM_USER_RAW_MDSS_DBG);
	ccif_write32(baseA,
		PCCIF_CHDATA + PCCIF_SRAM_SIZE - 3 * sizeof(u32),
		0x7274626E);
	ccif_write32(baseA,
		PCCIF_CHDATA + PCCIF_SRAM_SIZE - 2 * sizeof(u32),
		region->base_md_view_phy);
	ccif_write32(baseA,
		PCCIF_CHDATA + PCCIF_SRAM_SIZE - sizeof(u32),
		region->size);
}

int ccci_platform_init(struct ccci_modem *md)
{
	struct device_node *node;
	/* Get infra cfg ao base */
	node = of_find_compatible_node(NULL, NULL, "mediatek,infracfg_ao");
	infra_ao_base = (unsigned long)of_iomap(node, 0);
	if (!infra_ao_base) {
		CCCI_ERROR_LOG(md->index, TAG,
			"%s: infra_ao_base of_iomap failed\n", node->full_name);
		return -1;
	}
	CCCI_INIT_LOG(-1, TAG, "infra_ao_base:0x%p\n", (void *)infra_ao_base);
	/*Get pericfg base(0x1000 3000) for ccif5*/
	node = of_find_compatible_node(NULL, NULL, "mediatek,pericfg");
	pericfg_base = (unsigned long)of_iomap(node, 0);
	if (!pericfg_base) {
		CCCI_ERROR_LOG(md->index, TAG,
			"%s: pericfg_base of_iomap failed\n", node->full_name);
		return -1;
	}
	CCCI_INIT_LOG(-1, TAG, "pericfg_base:0x%p\n", (void *)pericfg_base);

	node = of_find_compatible_node(NULL, NULL, "mediatek,infracfg_ao_mem");
	infra_ao_mem_base = (unsigned long)of_iomap(node, 0);
	if (!infra_ao_mem_base) {
		CCCI_ERROR_LOG(md->index, TAG,
			"%s: infra_ao_mem_base of_iomap failed\n",
			node->full_name);
		return -1;
	}
	CCCI_INIT_LOG(-1, TAG, "infra_ao_mem_base:0x%p\n",
			(void *)infra_ao_mem_base);

	ccci_md_devapc_register_cb();
	CCCI_INIT_LOG(-1, TAG, "ccci_md_devapc_register_callback success\n");

#ifdef FEATURE_LOW_BATTERY_SUPPORT
	register_low_battery_notify_ext(
		&ccci_md_low_battery_cb, LOW_BATTERY_PRIO_MD);
	register_battery_oc_notify(
		&ccci_md_over_current_cb, BATTERY_OC_PRIO_MD);
#endif
	return 0;
}

#define DUMMY_PAGE_SIZE (128)
#define DUMMY_PADDING_CNT (5)

#define CTRL_PAGE_SIZE (1024)
#define CTRL_PAGE_NUM (32)

#define MD_EX_PAGE_SIZE (20*1024)
#define MD_EX_PAGE_NUM  (6)


/*
 *  Note : Moidy this size will affect dhl frame size in this page
 *  Minimum : 352B to reserve 256B for header frame
 */
#define MD_HW_PAGE_SIZE (512)

/* replace with HW page */
#define MD_BUF1_PAGE_SIZE (MD_HW_PAGE_SIZE)
#define MD_BUF1_PAGE_NUM  (72)
#define AP_BUF1_PAGE_SIZE (1024)
#define AP_BUF1_PAGE_NUM  (32)

#define MD_BUF2_0_PAGE_SIZE (MD_HW_PAGE_SIZE)
#define MD_BUF2_1_PAGE_SIZE (MD_HW_PAGE_SIZE)
#define MD_BUF2_2_PAGE_SIZE (MD_HW_PAGE_SIZE)

#define MD_BUF2_0_PAGE_NUM (64)
#define MD_BUF2_1_PAGE_NUM (64)
#define MD_BUF2_2_PAGE_NUM (256)

#define MD_MDM_PAGE_SIZE (MD_HW_PAGE_SIZE)
#define MD_MDM_PAGE_NUM  (32)

#define AP_MDM_PAGE_SIZE (1024)
#define AP_MDM_PAGE_NUM  (16)

#define MD_META_PAGE_SIZE (65*1024)
#define MD_META_PAGE_NUM (8)

#define AP_META_PAGE_SIZE (63*1024)
#define AP_META_PAGE_NUM (8)

struct ccci_ccb_config ccb_configs[] = {
	{SMEM_USER_CCB_DHL, P_CORE, CTRL_PAGE_SIZE,
			CTRL_PAGE_SIZE, CTRL_PAGE_SIZE*CTRL_PAGE_NUM,
			CTRL_PAGE_SIZE*CTRL_PAGE_NUM}, /* Ctrl */
	{SMEM_USER_CCB_DHL, P_CORE, MD_EX_PAGE_SIZE,
		 DUMMY_PAGE_SIZE, MD_EX_PAGE_SIZE*MD_EX_PAGE_NUM,
			DUMMY_PAGE_SIZE},			/* exception */
	{SMEM_USER_CCB_DHL, P_CORE, MD_BUF1_PAGE_SIZE,
	 AP_BUF1_PAGE_SIZE, (MD_BUF1_PAGE_SIZE*MD_BUF1_PAGE_NUM),
			AP_BUF1_PAGE_SIZE*AP_BUF1_PAGE_NUM},/* PS */
	{SMEM_USER_CCB_DHL, P_CORE, MD_BUF2_0_PAGE_SIZE,
		 DUMMY_PAGE_SIZE, MD_BUF2_0_PAGE_SIZE*MD_BUF2_0_PAGE_NUM,
			DUMMY_PAGE_SIZE},     /* HWLOGGER1 */
	{SMEM_USER_CCB_DHL, P_CORE, MD_BUF2_1_PAGE_SIZE,
		 DUMMY_PAGE_SIZE, MD_BUF2_1_PAGE_SIZE*MD_BUF2_1_PAGE_NUM,
			DUMMY_PAGE_SIZE},     /* HWLOGGER2  */
	{SMEM_USER_CCB_DHL, P_CORE, MD_BUF2_2_PAGE_SIZE,
		 DUMMY_PAGE_SIZE, MD_BUF2_2_PAGE_SIZE*MD_BUF2_2_PAGE_NUM,
			DUMMY_PAGE_SIZE},     /* HWLOGGER3 */
	{SMEM_USER_CCB_DHL, P_CORE, DUMMY_PAGE_SIZE,
		 DUMMY_PAGE_SIZE, DUMMY_PAGE_SIZE, DUMMY_PAGE_SIZE},
	{SMEM_USER_CCB_DHL, P_CORE, DUMMY_PAGE_SIZE,
		 DUMMY_PAGE_SIZE, DUMMY_PAGE_SIZE, DUMMY_PAGE_SIZE},
	{SMEM_USER_CCB_DHL, P_CORE, DUMMY_PAGE_SIZE,
		 DUMMY_PAGE_SIZE, DUMMY_PAGE_SIZE, DUMMY_PAGE_SIZE},
	{SMEM_USER_CCB_DHL, P_CORE, DUMMY_PAGE_SIZE,
		 DUMMY_PAGE_SIZE, DUMMY_PAGE_SIZE, DUMMY_PAGE_SIZE},
	{SMEM_USER_CCB_DHL, P_CORE, DUMMY_PAGE_SIZE,
		 DUMMY_PAGE_SIZE, DUMMY_PAGE_SIZE, DUMMY_PAGE_SIZE},
	{SMEM_USER_CCB_DHL, P_CORE, DUMMY_PAGE_SIZE,
		 DUMMY_PAGE_SIZE, DUMMY_PAGE_SIZE, DUMMY_PAGE_SIZE},
	{SMEM_USER_CCB_DHL, P_CORE, DUMMY_PAGE_SIZE,
		 DUMMY_PAGE_SIZE, DUMMY_PAGE_SIZE, DUMMY_PAGE_SIZE},
	{SMEM_USER_CCB_DHL, P_CORE, DUMMY_PAGE_SIZE,
		 DUMMY_PAGE_SIZE, DUMMY_PAGE_SIZE, DUMMY_PAGE_SIZE},
	{SMEM_USER_CCB_DHL, P_CORE, DUMMY_PAGE_SIZE,
		 DUMMY_PAGE_SIZE, DUMMY_PAGE_SIZE, DUMMY_PAGE_SIZE},
	{SMEM_USER_CCB_DHL, P_CORE, DUMMY_PAGE_SIZE,
		 DUMMY_PAGE_SIZE, DUMMY_PAGE_SIZE, DUMMY_PAGE_SIZE},
	{SMEM_USER_CCB_DHL, P_CORE, DUMMY_PAGE_SIZE,
		 DUMMY_PAGE_SIZE, DUMMY_PAGE_SIZE, DUMMY_PAGE_SIZE},
	{SMEM_USER_CCB_DHL, P_CORE, DUMMY_PAGE_SIZE,
		 DUMMY_PAGE_SIZE, DUMMY_PAGE_SIZE*DUMMY_PADDING_CNT,
		DUMMY_PAGE_SIZE},
	{SMEM_USER_CCB_MD_MONITOR, P_CORE, MD_MDM_PAGE_SIZE,
		 AP_MDM_PAGE_SIZE, MD_MDM_PAGE_SIZE*MD_MDM_PAGE_NUM,
		AP_MDM_PAGE_SIZE*AP_MDM_PAGE_NUM},     /* MDM */
	{SMEM_USER_CCB_META, P_CORE, MD_META_PAGE_SIZE,
		AP_META_PAGE_SIZE, MD_META_PAGE_SIZE*MD_META_PAGE_NUM,
		AP_META_PAGE_SIZE*AP_META_PAGE_NUM},   /* META */
};
unsigned int ccb_configs_len =
			sizeof(ccb_configs)/sizeof(struct ccci_ccb_config);


/* Iperf setting */
/* static const struct dvfs_ref s_dvfs_tbl[] = { */
/*	{1700000000LL, 1181000, 138300, 0, 0x02, 0xF0, 0xF0}, */
/*	{1350000000LL, 1500000, -1, -1, 0x02, 0xF0, 0xF0}, */
/*	{1000000000LL, 900000, -1, -1, 0x02, 0xF0, 0xF0}, */
/*	{210000000LL, 900000, -1, -1, 0xFF, 0xFF, 0x0D}, */
/*	{0LL, -1, -1, -1, 0xFF, 0xFF, 0x0D}, */
/* }; */

/* APK setting */
static  struct dvfs_ref s_dl_dvfs_tbl[] = {
	/*speed, cluster0, cluster1, cluster2, cluster3, dram, isr, push, rps*/
	{1700000000LL, 1530000, 1526000, -1, -1, 0, 0x02, 0xF0, 0xF0},
	{1350000000LL, 1530000, 1526000, -1, -1, 1, 0x02, 0xF0, 0xF0},
	{1000000000LL, 1300000, 1406000, -1, -1, 1, 0x02, 0xF0, 0xF0},
	{450000000LL, 1200000, 1406000, -1, -1, 1, 0x02, 0xF0, 0xF0},
	{230000000LL, 1181000, -1, -1, -1, 1, 0xFF, 0xFF, 0x0D},
	{5000000LL, -1, -1, -1, -1, 1, 0xFF, 0xFF, 0x0D},
	/* normal */
	{0LL, -1, -1, -1, -1, -1, 0xFF, 0xFF, 0x0D},
};

static  struct dvfs_ref s_ul_dvfs_tbl[] = {
	/*speed, cluster0, cluster1, cluster2, cluster3, dram, isr, push, rps*/
	{600000000LL, 2700000, 2706000, -1, -1, 0, 0x02, 0xF0, 0xF0},
	{500000000LL, 1700000, 1706000, -1, -1, 0, 0x02, 0xF0, 0xF0},
	{300000000LL, 1500000, 1500000, -1, -1, 1, 0xFF, 0xFF, 0x0D},
	{250000000LL, -1, -1, -1, -1, -1, 0xFF, 0xFF, 0x0D},
	/* normal */
	{0LL, -1, -1, -1, -1, -1, 0xFF, 0xFF, 0x0D},
};

struct dvfs_ref *mtk_ccci_get_dvfs_table(int is_ul, int *tbl_num)
{
	if (!tbl_num)
		return NULL;

	/* Query UL settings */
	if (is_ul) {
		*tbl_num = (int)ARRAY_SIZE(s_ul_dvfs_tbl);
		return s_ul_dvfs_tbl;
	}
	/* DL settings */
	*tbl_num = (int)ARRAY_SIZE(s_dl_dvfs_tbl);
	return s_dl_dvfs_tbl;
}



