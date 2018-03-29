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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/mm.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/irq.h>
#include <linux/sched.h>
#include <linux/list.h>
#ifdef CONFIG_MTK_AEE_FEATURE
#include <mt-plat/aee.h>
#endif
#include <linux/timer.h>
#include <linux/workqueue.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>

#include "mach/emi_mpu.h"
#include <mt-plat/mt_io.h>
#include <mt-plat/mt_device_apc.h>
#include <mt-plat/sync_write.h>

#ifdef CONFIG_MTK_IN_HOUSE_TEE_SUPPORT
#include "trustzone/kree/system.h"
#include "trustzone/tz_cross/trustzone.h"
#include "trustzone/tz_cross/ta_emi.h"
#endif

#define ENABLE_EMI_CHKER

#define NR_REGION_ABORT 8
#define MAX_EMI_MPU_STORE_CMD_LEN 128
#define TIMEOUT 100
#define AXI_VIO_MONITOR_TIME    (1 * HZ)

static struct work_struct emi_mpu_work;
static struct workqueue_struct *emi_mpu_workqueue;

static unsigned int vio_addr;

struct mst_tbl_entry {
	u32 master;
	u32 port;
	u32 id_mask;
	u32 id_val;
	char *name;
};

struct emi_mpu_notifier_block {
	struct list_head list;
	emi_mpu_notifier notifier;
};

static const struct mst_tbl_entry mst_tbl[] = {
	/* apmcu */
	{.master = MST_ID_APMCU_0, .port = 0x0, .id_mask = 0x000e,
	.id_val = 0x0008, .name = "APMCU: CA7"},

	{.master = MST_ID_APMCU_1, .port = 0x0, .id_mask = 0x000e,
	.id_val = 0x0006, .name = "APMCU: CA15"},

	{.master = MST_ID_APMCU_2, .port = 0x0, .id_mask = 0x1ffe,
	.id_val = 0x0004, .name = "MSDC0"},

	{.master = MST_ID_APMCU_3, .port = 0x0, .id_mask = 0x1ffe,
	.id_val = 0x0084, .name = "NFI"},

	{.master = MST_ID_APMCU_4, .port = 0x0, .id_mask = 0x1ffe,
	.id_val = 0x0284, .name = "Audio"},

	{.master = MST_ID_APMCU_5, .port = 0x0, .id_mask = 0x1ffe,
	.id_val = 0x0484, .name = "MSDC3"},

	{.master = MST_ID_APMCU_6, .port = 0x0, .id_mask = 0x1ffe,
	.id_val = 0x0684, .name = "USB20"},

	{.master = MST_ID_APMCU_7, .port = 0x0, .id_mask = 0x1ffe,
	.id_val = 0x0104, .name = "MD"},

	{.master = MST_ID_APMCU_8, .port = 0x0, .id_mask = 0x1ffe,
	.id_val = 0x0304, .name = "SPM"},

	{.master = MST_ID_APMCU_9, .port = 0x0, .id_mask = 0x1ffe,
	.id_val = 0x0504, .name = "MD32"},

	{.master = MST_ID_APMCU_10, .port = 0x0, .id_mask = 0x1ffe,
	.id_val = 0x0704, .name = "THERM"},

	{.master = MST_ID_APMCU_11, .port = 0x0, .id_mask = 0x1ffe,
	.id_val = 0x0184, .name = "DMA"},

	{.master = MST_ID_APMCU_12, .port = 0x0, .id_mask = 0x1ffe,
	.id_val = 0x0014, .name = "PWM"},

	{.master = MST_ID_APMCU_13, .port = 0x0, .id_mask = 0x1ffe,
	.id_val = 0x0114, .name = "MSDC1"},

	{.master = MST_ID_APMCU_14, .port = 0x0, .id_mask = 0x1ffe,
	.id_val = 0x0214, .name = "MSCD2"},

	{.master = MST_ID_APMCU_15, .port = 0x0, .id_mask = 0x1ffe,
	.id_val = 0x0314, .name = "SPIO"},

	{.master = MST_ID_APMCU_16, .port = 0x0, .id_mask = 0x1ffe,
	.id_val = 0x0094, .name = "USB30"},

	{.master = MST_ID_APMCU_17, .port = 0x0, .id_mask = 0x1ffe,
	.id_val = 0x0024, .name = "GCPU"},

	{.master = MST_ID_APMCU_18, .port = 0x0, .id_mask = 0x1f7e,
	.id_val = 0x0034, .name = "CQ_DMA"},

	{.master = MST_ID_APMCU_19, .port = 0x0, .id_mask = 0x1f7e,
	.id_val = 0x0044, .name = "DebugTop"},

	{.master = MST_ID_APMCU_20, .port = 0x0, .id_mask = 0x1ffe,
	.id_val = 0x07c4, .name = "Perisys IOMMU"},

	{.master = MST_ID_APMCU_21, .port = 0x0, .id_mask = 0x1ffe,
	.id_val = 0x07d4, .name = "Perisys IOMMU"},

	{.master = MST_ID_APMCU_22, .port = 0x0, .id_mask = 0x1f0e,
	.id_val = 0x0002, .name = "GPU"},

	/* MM */
	{.master = MST_ID_MM_0, .port = 0x2, .id_mask = 0x1f80,
	.id_val = 0x0000, .name = "Larb0"},

	{.master = MST_ID_MM_1, .port = 0x2, .id_mask = 0x1ffe,
	.id_val = 0x0080, .name = "Larb1"},

	{.master = MST_ID_MM_2, .port = 0x2, .id_mask = 0x1ffe,
	.id_val = 0x0100, .name = "Larb2"},

	{.master = MST_ID_MM_3, .port = 0x2, .id_mask = 0x1ffe,
	.id_val = 0x0180, .name = "Larb3"},

	{.master = MST_ID_MM_4, .port = 0x2, .id_mask = 0x1ffe,
	.id_val = 0x0200, .name = "Larb4"},

	{.master = MST_ID_MM_5, .port = 0x2, .id_mask = 0x1ffe,
	.id_val = 0x03fc, .name = "M4U"},

	/* Modem */
	{.master = MST_ID_MDMCU_0, .port = 0x3, .id_mask = 0x0000,
	.id_val = 0x0000, .name = "MDMCU"},

	/* Modem HW (2G/3G) */
	{.master = MST_ID_MDHW_0, .port = 0x4, .id_mask = 0x0000,
	.id_val = 0x0000, .name = "MDHW"},

	/* Periperal */
	{.master = MST_ID_PERI_0, .port = 0x6, .id_mask = 0x1fff,
	.id_val = 0x0001, .name = "MSDC0"},

	{.master = MST_ID_PERI_1, .port = 0x6, .id_mask = 0x1fff,
	.id_val = 0x0011, .name = "NFI"},

	{.master = MST_ID_PERI_2, .port = 0x6, .id_mask = 0x1fff,
	.id_val = 0x0051, .name = "Audio"},

	{.master = MST_ID_PERI_3, .port = 0x6, .id_mask = 0x1fff,
	.id_val = 0x0091, .name = "MSDC3"},

	{.master = MST_ID_PERI_4, .port = 0x6, .id_mask = 0x1fff,
	.id_val = 0x00d1, .name = "USB20"},

	{.master = MST_ID_PERI_5, .port = 0x6, .id_mask = 0x1fff,
	.id_val = 0x0021, .name = "MD"},

	{.master = MST_ID_PERI_6, .port = 0x6, .id_mask = 0x1fff,
	.id_val = 0x0061, .name = "SPM"},

	{.master = MST_ID_PERI_7, .port = 0x6, .id_mask = 0x1fff,
	.id_val = 0x00a1, .name = "MD32"},

	{.master = MST_ID_PERI_8, .port = 0x6, .id_mask = 0x1fff,
	.id_val = 0x00e1, .name = "THERM"},

	{.master = MST_ID_PERI_9, .port = 0x6, .id_mask = 0x1fff,
	.id_val = 0x0031, .name = "DMA"},

	{.master = MST_ID_PERI_10, .port = 0x6, .id_mask = 0x1fff,
	.id_val = 0x0003, .name = "PWM"},

	{.master = MST_ID_PERI_11, .port = 0x6, .id_mask = 0x1fff,
	.id_val = 0x0023, .name = "MSDC1"},

	{.master = MST_ID_PERI_12, .port = 0x6, .id_mask = 0x1fff,
	.id_val = 0x0043, .name = "MSCD2"},

	{.master = MST_ID_PERI_13, .port = 0x6, .id_mask = 0x1fff,
	.id_val = 0x0063, .name = "SPIO"},

	{.master = MST_ID_PERI_14, .port = 0x6, .id_mask = 0x1fff,
	.id_val = 0x0013, .name = "USB30"},

	{.master = MST_ID_PERI_15, .port = 0x6, .id_mask = 0x1fff,
	.id_val = 0x0005, .name = "GCPU"},

	{.master = MST_ID_PERI_16, .port = 0x6, .id_mask = 0x1fef,
	.id_val = 0x0007, .name = "CQ_DMA"},

	{.master = MST_ID_PERI_17, .port = 0x6, .id_mask = 0x1fff,
	.id_val = 0x0009, .name = "DebugTop"},

	{.master = MST_ID_PERI_18, .port = 0x6, .id_mask = 0x1fff,
	.id_val = 0x00f9, .name = "Perisys IOMMU"},

	{.master = MST_ID_PERI_19, .port = 0x6, .id_mask = 0x1fff,
	.id_val = 0x00fb, .name = "Perisys IOMMU"},

	{.master = MST_ID_PERI_20, .port = 0x6, .id_mask = 0x1f01,
	.id_val = 0x0000, .name = "emi_m6 : MFG"},

	/* GPU */
	{.master = MST_ID_GPU_0, .port = 0x7, .id_mask = 0x1f80,
	.id_val = 0x0000, .name = "emi_m7 : MFG"},
};

/* struct list_head emi_mpu_notifier_list[NR_MST]; */
static const char *UNKNOWN_MASTER = "unknown";
static spinlock_t emi_mpu_lock;

#ifdef ENABLE_EMI_CHKER
struct timer_list emi_axi_vio_timer;
#endif

char *smi_larb0_port[14] = {"disp_ovl_0", "disp_rdma_0", "disp_rdma_1", "disp_wdma_0",
		"disp_ovl_1", "disp_rdma_2", "disp_wdma_1", "disp_od_r",
		"disp_od_w", "mdp_rdma_0", "mdp_rdma_1", "mdp_wdma",
		"mdp_wrot_0", "mdp_wrot_1" };

char *smi_larb1_port[9] = {"hw_vdec_mc_ext", "hw_vdec_pp_ext", "hw_vdec_ufo_ext",
		"hw_vdec_vid_ext", "hw_vdec_vid2_ext", "hw_vdec_avc_ext",
		"hw_vdec_pred_rd_ext", "hw_vdec_pred_wr_ext", "hw_vdec_ppwarp_ext" };

char *smi_larb2_port[21] = {"cam_imgo", "cam_rrzo", "cam_aao", "cam_icso", "cam_esfko",
		"cam_imgo_d", "cam_isci", "cam_isci_d", "cam_bpci", "cam_bpci_d",
		"cam_ufdi", "cam_imgi", "cam_img2o", "cam_img3o", "cam_vipi",
		"cam_vip2i", "cam_vip3i", "cam_icei", "cam_rb", "cam_rp", "cam_wr"
};

char *smi_larb3_port[19] = {"venc_rcpu", "venc_rec", "venc_bsdma", "venc_sv_comv",
	"vend_rd_comv", "jpgenc_bsdma", "remdc_sdma", "remdc_bsdma", "jpgenc_rdma",
	"jpgenc_sdma", "jpgenc_wdma", "jpgdec_bsdma", "venc_cur_luma", "venc_cur_chroma",
	"venc_ref_luma", "vend_ref_chroma", "redmc_wdma", "venc_nbm_rdma", "venc_nbm_wdma"
};

char *smi_larb4_port[4] = {"mjc_mv_rd", "mjc_mv_wr", "mjc_dma_rd", "mjc_dma_wr"};

static int __match_id(u32 axi_id, int tbl_idx, u32 port_ID)
{
	u32 mm_larb;
	u32 smi_port;

	if (((axi_id & mst_tbl[tbl_idx].id_mask) == mst_tbl[tbl_idx].id_val)
	    && (port_ID == mst_tbl[tbl_idx].port)) {
		switch (port_ID) {
		case 0:	/* ARM */
		case 3:	/* MD */
		case 4:	/* MD HW (2G/3G) */
		case 6:	/* Peripheral */
		case 7:	/* GPU */
			pr_err("Violation master name is %s.\n", mst_tbl[tbl_idx].name);
			break;
		case 2:	/* MM */
			mm_larb = axi_id >> 7;
			smi_port = (axi_id & 0x7F) >> 2;
			if (mm_larb == 0x0) {
				if (smi_port >= ARRAY_SIZE(smi_larb0_port)) {
					pr_err("[EMI MPU ERROR] Invalidate master ID! lookup smi table failed!\n");
					return 0;
				}
				pr_err("Violation master name is %s (%s).\n",
				       mst_tbl[tbl_idx].name, smi_larb0_port[smi_port]);
			} else if (mm_larb == 0x1) {
				if (smi_port >= ARRAY_SIZE(smi_larb1_port)) {
					pr_err("[EMI MPU ERROR] Invalidate master ID! lookup smi table failed!\n");
					return 0;
				}
				pr_err("Violation master name is %s (%s).\n",
				       mst_tbl[tbl_idx].name, smi_larb1_port[smi_port]);
			} else if (mm_larb == 0x2) {
				if (smi_port >= ARRAY_SIZE(smi_larb2_port)) {
					pr_err("[EMI MPU ERROR] Invalidate master ID! lookup smi table failed!\n");
					return 0;
				}
				pr_err("Violation master name is %s (%s).\n",
				       mst_tbl[tbl_idx].name, smi_larb2_port[smi_port]);
			} else if (mm_larb == 0x3) {
				if (smi_port >= ARRAY_SIZE(smi_larb3_port)) {
					pr_err("[EMI MPU ERROR] Invalidate master ID! lookup smi table failed!\n");
					return 0;
				}
				pr_err("Violation master name is %s (%s).\n",
				       mst_tbl[tbl_idx].name, smi_larb3_port[smi_port]);
			} else if (mm_larb == 0x4) {
				if (smi_port >= ARRAY_SIZE(smi_larb4_port)) {
					pr_err("[EMI MPU ERROR] Invalidate master ID! lookup smi table failed!\n");
					return 0;
				}
				pr_err("Violation master name is %s (%s).\n",
				       mst_tbl[tbl_idx].name, smi_larb4_port[smi_port]);
			} else {	/*M4U */
				pr_err("Violation master name is %s.\n",
				       mst_tbl[tbl_idx].name);
			}
			break;
		default:
			pr_err("[EMI MPU ERROR] Invalidate port ID! lookup bus ID table failed!\n");
			break;
		}
		return 1;
	} else {
		return 0;
	}
}

static char *__id2name(u32 id)
{
	int i;
	u32 axi_ID;
	u32 port_ID;

	axi_ID = (id >> 3) & 0x00001FFF;
	port_ID = id & 0x00000007;

	pr_err("[EMI MPU] axi_id = %x, port_id = %x\n", axi_ID, port_ID);

	/* Both M0 & M1 are APMCU */
	if (port_ID == 1)
		port_ID = 0;

	/* Both M2 & M5 are MM */
	if (port_ID == 5)
		port_ID = 2;

	for (i = 0; i < ARRAY_SIZE(mst_tbl); i++) {
		if (__match_id(axi_ID, i, port_ID))
			return mst_tbl[i].name;
	}

	return (char *)UNKNOWN_MASTER;
}


#ifdef CONFIG_MTK_IN_HOUSE_TEE_SUPPORT
#ifdef CONFIG_MTK_AEE_FEATURE

static u32 __id2mst(u32 id)
{
	int i;
	u32 axi_ID;
	u32 port_ID;

	axi_ID = (id >> 3) & 0x000001FFF;
	port_ID = id & 0x00000007;

	pr_err("[EMI MPU] axi_id = %x, port_id = %x\n", axi_ID, port_ID);

	/* Both M0 & M1 are APMCU */
	if (port_ID == 1)
		port_ID = 0;
	/* Both M2 & M5 are MM */
	if (port_ID == 5)
		port_ID = 2;

	for (i = 0; i < ARRAY_SIZE(mst_tbl); i++) {
		if (__match_id(axi_ID, i, port_ID))
			return mst_tbl[i].master;
	}
	return MST_INVALID;
}
#endif

/*
static void __clear_emi_mpu_vio(void)
{

}
*/

/*EMI MPU violation handler*/
static irqreturn_t mpu_violation_irq(int irq, void *dev_id)
{
	u32 dbg_s, dbg_t, dbg_pqry, i;
	u32 master_ID, domain_ID, wr_vio;
	s32 region;
	char *master_name;

	KREE_SESSION_HANDLE emi_session;
	MTEEC_PARAM param[4];
	TZ_RESULT ret;


	pr_err("[EMI MPU] Violation information from TA.\n");

	ret = KREE_CreateSession(TZ_TA_EMI_UUID, &emi_session);
	if (ret != TZ_RESULT_SUCCESS)
		return -1;

	ret = KREE_TeeServiceCall(emi_session, TZCMD_EMI_REG,
			TZ_ParamTypes4(TZPT_VALUE_OUTPUT, TZPT_VALUE_OUTPUT,
			TZPT_VALUE_OUTPUT, TZPT_VALUE_OUTPUT), param);

	dbg_s	  = (uint32_t)(param[0].value.a);
	dbg_t	  = (uint32_t)(param[0].value.b);
	master_ID = (uint32_t)(param[1].value.a);
	domain_ID = (uint32_t)(param[1].value.b);
	region	  = (uint32_t)(param[2].value.a);
	dbg_pqry  = (uint32_t)(param[2].value.b);
	wr_vio = (dbg_s >> 28) & 0x00000003;

	ret = KREE_CloseSession(emi_session);
	if (ret != TZ_RESULT_SUCCESS)
		return -1;

	if (dbg_s == 0) {
		pr_err("It's not a MPU violation.\n");
		return IRQ_NONE;
	}

	for (i = 0; i < NR_REGION_ABORT; i++) {
		if ((region >> i) & 1)
			break;
	}
	region = (i >= NR_REGION_ABORT) ? -1 : i;

	/*TBD: print the abort region */
	pr_err("[EMI MPU] Debug info start ----------------------------------------\n");
	pr_err("EMI_MPUS = %x, EMI_MPUT = %x.\n", dbg_s, dbg_t);
	pr_err("Current process is \"%s \" (pid: %i).\n", current->comm, current->pid);
	pr_err("Violation address is 0x%x.\n", dbg_t + EMI_PHY_OFFSET);
	pr_err("Violation master ID is 0x%x.\n", master_ID);

	/*print out the murderer name */
	master_name = __id2name(master_ID);
	pr_err("Violation domain ID is 0x%x.\n", domain_ID);
	pr_err("%s violation.\n", (wr_vio == 1) ? "Write" : "Read");
	pr_err("Corrupted region is %d\n\r", region);
	if (dbg_pqry & OOR_VIO)
		pr_err("Out of range violation.\n");
	pr_err("[EMI MPU] Debug info end------------------------------------------\n");

#ifdef CONFIG_MTK_AEE_FEATURE
	/*FIXME: skip ca53 violation to trigger root-cause KE*/
	if ((0 != dbg_s) && (__id2mst(master_ID) != MST_ID_APMCU_0)
		&& (__id2mst(master_ID) != MST_ID_APMCU_1)) {
		aee_kernel_exception("EMI MPU",
			"EMI MPU violation.\nEMI_MPUS = 0x%x, EMI_MPUT = 0x%x, module is %s.\n",
			dbg_s, dbg_t + EMI_PHY_OFFSET, master_name);
	}
#endif

	vio_addr = dbg_t + EMI_PHY_OFFSET;

	return IRQ_HANDLED;
}
#endif


/* Acquire DRAM Setting for PASR/DPD */
void acquire_dram_setting(struct basic_dram_setting *pasrdpd)
{
	int ch_nr = MAX_CHANNELS;
	unsigned int emi_cona, col_bit, row_bit;

	pasrdpd->channel_nr = ch_nr;

	emi_cona = readl(IOMEM(EMI_CONA));

	/* channel 0 */
	{
		/* rank 0 */
		col_bit = ((emi_cona >> 4) & 0x03) + 9;
		row_bit = ((emi_cona >> 12) & 0x03) + 13;
		pasrdpd->channel[0].rank[0].valid_rank = true;
		/* 32 bits * 8 banks, unit Gb */
		pasrdpd->channel[0].rank[0].rank_size = (1 << (row_bit + col_bit)) >> 22;
		pasrdpd->channel[0].rank[0].segment_nr = 8;
		if (0 != (emi_cona & (1 << 17))) {	/* rank 1 exist */
			col_bit = ((emi_cona >> 6) & 0x03) + 9;
			row_bit = ((emi_cona >> 14) & 0x03) + 13;
			pasrdpd->channel[0].rank[1].valid_rank = true;
			/* 32 bits * 8 banks, unit Gb */
			pasrdpd->channel[0].rank[1].rank_size = (1 << (row_bit + col_bit)) >> 22;
			pasrdpd->channel[0].rank[1].segment_nr = 8;
		} else {
			pasrdpd->channel[0].rank[1].valid_rank = false;
			pasrdpd->channel[0].rank[1].rank_size = 0;
			pasrdpd->channel[0].rank[1].segment_nr = 0;
		}
	}

	if (0 != (emi_cona & 0x01)) {	/* channel 1 exist */
		/* rank0 setting */
		col_bit = ((emi_cona >> 20) & 0x03) + 9;
		row_bit = ((emi_cona >> 28) & 0x03) + 13;
		pasrdpd->channel[1].rank[0].valid_rank = true;
		/* 32 bits * 8 banks, unit Gb */
		pasrdpd->channel[1].rank[0].rank_size = (1 << (row_bit + col_bit)) >> 22;
		pasrdpd->channel[1].rank[0].segment_nr = 8;

		if (0 != (emi_cona & (1 << 16))) {	/* rank 1 exist */
			col_bit = ((emi_cona >> 22) & 0x03) + 9;
			row_bit = ((emi_cona >> 30) & 0x03) + 13;
			pasrdpd->channel[1].rank[1].valid_rank = true;
			/* 32 bits * 8 banks, unit Gb */
			pasrdpd->channel[1].rank[1].rank_size = (1 << (row_bit + col_bit)) >> 22;
			pasrdpd->channel[1].rank[1].segment_nr = 8;
		} else {
			pasrdpd->channel[1].rank[1].valid_rank = false;
			pasrdpd->channel[1].rank[1].rank_size = 0;
			pasrdpd->channel[1].rank[1].segment_nr = 0;
		}
	} else {	/* channel 2 does not exist */

		pasrdpd->channel[1].rank[0].valid_rank = false;
		pasrdpd->channel[1].rank[0].rank_size = 0;
		pasrdpd->channel[1].rank[0].segment_nr = 0;

		pasrdpd->channel[1].rank[1].valid_rank = false;
		pasrdpd->channel[1].rank[1].rank_size = 0;
		pasrdpd->channel[1].rank[1].segment_nr = 0;
	}
}

/*
 * emi_mpu_set_region_protection: protect a region.
 * @start: start address of the region
 * @end: end address of the region
 * @region: EMI MPU region id
 * @access_permission: EMI MPU access permission
 * Return 0 for success, otherwise negative status code.
 */
int emi_mpu_set_region_protection(unsigned int start, unsigned int end, int region,
				  unsigned int access_permission)
{
	int ret = 0;
	unsigned int tmp;
	unsigned long flags;

	if ((end != 0) || (start != 0)) {
		/*Address 64KB alignment */
		start -= EMI_PHY_OFFSET;
		end -= EMI_PHY_OFFSET;
		start = start >> 16;
		end = end >> 16;

		if (end <= start)
			return -EINVAL;
	}

	spin_lock_irqsave(&emi_mpu_lock, flags);

	switch (region) {
	case 0:
		/* Marcos: Clear access right before setting MPU address (Mt6582 design) */
		tmp = readl(IOMEM(EMI_MPUI)) & 0xFFFF0000;
		mt_reg_sync_writel(0, EMI_MPUI);
		mt_reg_sync_writel((start << 16) | end, EMI_MPUA);
		mt_reg_sync_writel(tmp | access_permission, EMI_MPUI);
		break;

	case 1:
		/* Marcos: Clear access right before setting MPU address (Mt6582 design) */
		tmp = readl(IOMEM(EMI_MPUI)) & 0x0000FFFF;
		mt_reg_sync_writel(0, EMI_MPUI);
		mt_reg_sync_writel((start << 16) | end, EMI_MPUB);
		mt_reg_sync_writel(tmp | (access_permission << 16), EMI_MPUI);
		break;

	case 2:
		/* Marcos: Clear access right before setting MPU address (Mt6582 design) */
		tmp = readl(IOMEM(EMI_MPUJ)) & 0xFFFF0000;
		mt_reg_sync_writel(0, EMI_MPUJ);
		mt_reg_sync_writel((start << 16) | end, EMI_MPUC);
		mt_reg_sync_writel(tmp | access_permission, EMI_MPUJ);
		break;

	case 3:
		/* Marcos: Clear access right before setting MPU address (Mt6582 design) */
		tmp = readl(IOMEM(EMI_MPUJ)) & 0x0000FFFF;
		mt_reg_sync_writel(0, EMI_MPUJ);
		mt_reg_sync_writel((start << 16) | end, EMI_MPUD);
		mt_reg_sync_writel(tmp | (access_permission << 16), EMI_MPUJ);
		break;

	case 4:
		/* Marcos: Clear access right before setting MPU address (Mt6582 design) */
		tmp = readl(IOMEM(EMI_MPUK)) & 0xFFFF0000;
		mt_reg_sync_writel(0, EMI_MPUK);
		mt_reg_sync_writel((start << 16) | end, EMI_MPUE);
		mt_reg_sync_writel(tmp | access_permission, EMI_MPUK);
		break;

	case 5:
		/* Marcos: Clear access right before setting MPU address (Mt6582 design) */
		tmp = readl(IOMEM(EMI_MPUK)) & 0x0000FFFF;
		mt_reg_sync_writel(0, EMI_MPUK);
		mt_reg_sync_writel((start << 16) | end, EMI_MPUF);
		mt_reg_sync_writel(tmp | (access_permission << 16), EMI_MPUK);
		break;

	case 6:
		/* Marcos: Clear access right before setting MPU address (Mt6582 design) */
		tmp = readl(IOMEM(EMI_MPUL)) & 0xFFFF0000;
		mt_reg_sync_writel(0, EMI_MPUL);
		mt_reg_sync_writel((start << 16) | end, EMI_MPUG);
		mt_reg_sync_writel(tmp | access_permission, EMI_MPUL);
		break;

	case 7:
		/* Marcos: Clear access right before setting MPU address (Mt6582 design) */
		tmp = readl(IOMEM(EMI_MPUL)) & 0x0000FFFF;
		mt_reg_sync_writel(0, EMI_MPUL);
		mt_reg_sync_writel((start << 16) | end, EMI_MPUH);
		mt_reg_sync_writel(tmp | (access_permission << 16), EMI_MPUL);
		break;

	default:
		ret = -EINVAL;
		break;
	}

	spin_unlock_irqrestore(&emi_mpu_lock, flags);

	return ret;
}
EXPORT_SYMBOL(emi_mpu_set_region_protection);

/*
 * emi_mpu_notifier_register: register a notifier.
 * master: MST_ID_xxx
 * notifier: the callback function
 * Return 0 for success, otherwise negative error code.
 */
#if 0
int emi_mpu_notifier_register(int master, emi_mpu_notifier notifier)
{
	struct emi_mpu_notifier_block *block;
	static int emi_mpu_notifier_init;
	int i;

	if (master >= MST_INVALID)
		return -EINVAL;

	block = kmalloc(sizeof(struct emi_mpu_notifier_block), GFP_KERNEL);
	if (!block)
		return -ENOMEM;

	if (!emi_mpu_notifier_init) {
		for (i = 0; i < NR_MST; i++)
			INIT_LIST_HEAD(&(emi_mpu_notifier_list[i]));
		emi_mpu_notifier_init = 1;
	}

	block->notifier = notifier;
	list_add(&(block->list), &(emi_mpu_notifier_list[master]));

	return 0;
}
#endif

static ssize_t emi_mpu_show(struct device_driver *driver, char *buf)
{
	char *ptr = buf;
	unsigned int start, end;
	unsigned int reg_value;
	unsigned int d0, d1, d2, d3;
	static const char *permission[6] = {
		"No protect",
		"Only R/W for secure access",
		"Only R/W for secure access, and non-secure read access",
		"Only R/W for secure access, and non-secure write access",
		"Only R for secure/non-secure",
		"Both R/W are forbidden" "Only secure W is forbidden"
	};

	reg_value = readl(IOMEM(EMI_MPUA));
	start = ((reg_value >> 16) << 16) + EMI_PHY_OFFSET;
	end = ((reg_value & 0xFFFF) << 16) + EMI_PHY_OFFSET;
	ptr += sprintf(ptr, "Region 0 --> 0x%x to 0x%x\n", start, end);

	reg_value = readl(IOMEM(EMI_MPUB));
	start = ((reg_value >> 16) << 16) + EMI_PHY_OFFSET;
	end = ((reg_value & 0xFFFF) << 16) + EMI_PHY_OFFSET;
	ptr += sprintf(ptr, "Region 1 --> 0x%x to 0x%x\n", start, end);

	reg_value = readl(IOMEM(EMI_MPUC));
	start = ((reg_value >> 16) << 16) + EMI_PHY_OFFSET;
	end = ((reg_value & 0xFFFF) << 16) + EMI_PHY_OFFSET;
	ptr += sprintf(ptr, "Region 2 --> 0x%x to 0x%x\n", start, end);

	reg_value = readl(IOMEM(EMI_MPUD));
	start = ((reg_value >> 16) << 16) + EMI_PHY_OFFSET;
	end = ((reg_value & 0xFFFF) << 16) + EMI_PHY_OFFSET;
	ptr += sprintf(ptr, "Region 3 --> 0x%x to 0x%x\n", start, end);

	reg_value = readl(IOMEM(EMI_MPUE));
	start = ((reg_value >> 16) << 16) + EMI_PHY_OFFSET;
	end = ((reg_value & 0xFFFF) << 16) + EMI_PHY_OFFSET;
	ptr += sprintf(ptr, "Region 4 --> 0x%x to 0x%x\n", start, end);

	reg_value = readl(IOMEM(EMI_MPUF));
	start = ((reg_value >> 16) << 16) + EMI_PHY_OFFSET;
	end = ((reg_value & 0xFFFF) << 16) + EMI_PHY_OFFSET;
	ptr += sprintf(ptr, "Region 5 --> 0x%x to 0x%x\n", start, end);

	reg_value = readl(IOMEM(EMI_MPUG));
	start = ((reg_value >> 16) << 16) + EMI_PHY_OFFSET;
	end = ((reg_value & 0xFFFF) << 16) + EMI_PHY_OFFSET;
	ptr += sprintf(ptr, "Region 6 --> 0x%x to 0x%x\n", start, end);

	reg_value = readl(IOMEM(EMI_MPUH));
	start = ((reg_value >> 16) << 16) + EMI_PHY_OFFSET;
	end = ((reg_value & 0xFFFF) << 16) + EMI_PHY_OFFSET;
	ptr += sprintf(ptr, "Region 7 --> 0x%x to 0x%x\n", start, end);

	ptr += sprintf(ptr, "\n");

	reg_value = readl(IOMEM(EMI_MPUI));
	d0 = (reg_value & 0x7);
	d1 = (reg_value >> 3) & 0x7;
	d2 = (reg_value >> 6) & 0x7;
	d3 = (reg_value >> 9) & 0x7;
	ptr +=
	    sprintf(ptr, "Region 0 --> d0 = %s, d1 = %s, d2 = %s, d3 = %s\n", permission[d0],
		    permission[d1], permission[d2], permission[d3]);

	d0 = ((reg_value >> 16) & 0x7);
	d1 = ((reg_value >> 16) >> 3) & 0x7;
	d2 = ((reg_value >> 16) >> 6) & 0x7;
	d3 = ((reg_value >> 16) >> 9) & 0x7;
	ptr +=
	    sprintf(ptr, "Region 1 --> d0 = %s, d1 = %s, d2 = %s, d3 = %s\n", permission[d0],
		    permission[d1], permission[d2], permission[d3]);

	reg_value = readl(IOMEM(EMI_MPUJ));
	d0 = (reg_value & 0x7);
	d1 = (reg_value >> 3) & 0x7;
	d2 = (reg_value >> 6) & 0x7;
	d3 = (reg_value >> 9) & 0x7;
	ptr +=
	    sprintf(ptr, "Region 2 --> d0 = %s, d1 = %s, d2 = %s, d3 = %s\n", permission[d0],
		    permission[d1], permission[d2], permission[d3]);

	d0 = ((reg_value >> 16) & 0x7);
	d1 = ((reg_value >> 16) >> 3) & 0x7;
	d2 = ((reg_value >> 16) >> 6) & 0x7;
	d3 = ((reg_value >> 16) >> 9) & 0x7;
	ptr +=
	    sprintf(ptr, "Region 3 --> d0 = %s, d1 = %s, d2 = %s, d3 = %s\n", permission[d0],
		    permission[d1], permission[d2], permission[d3]);

	reg_value = readl(IOMEM(EMI_MPUK));
	d0 = (reg_value & 0x7);
	d1 = (reg_value >> 3) & 0x7;
	d2 = (reg_value >> 6) & 0x7;
	d3 = (reg_value >> 9) & 0x7;
	ptr +=
	    sprintf(ptr, "Region 4 --> d0 = %s, d1 = %s, d2 = %s, d3 = %s\n", permission[d0],
		    permission[d1], permission[d2], permission[d3]);

	d0 = ((reg_value >> 16) & 0x7);
	d1 = ((reg_value >> 16) >> 3) & 0x7;
	d2 = ((reg_value >> 16) >> 6) & 0x7;
	d3 = ((reg_value >> 16) >> 9) & 0x7;
	ptr +=
	    sprintf(ptr, "Region 5 --> d0 = %s, d1 = %s, d2 = %s, d3 = %s\n", permission[d0],
		    permission[d1], permission[d2], permission[d3]);

	reg_value = readl(IOMEM(EMI_MPUL));
	d0 = (reg_value & 0x7);
	d1 = (reg_value >> 3) & 0x7;
	d2 = (reg_value >> 6) & 0x7;
	d3 = (reg_value >> 9) & 0x7;
	ptr +=
	    sprintf(ptr, "Region 6 --> d0 = %s, d1 = %s, d2 = %s, d3 = %s\n", permission[d0],
		    permission[d1], permission[d2], permission[d3]);

	d0 = ((reg_value >> 16) & 0x7);
	d1 = ((reg_value >> 16) >> 3) & 0x7;
	d2 = ((reg_value >> 16) >> 6) & 0x7;
	d3 = ((reg_value >> 16) >> 9) & 0x7;
	ptr +=
	    sprintf(ptr, "Region 7 --> d0 = %s, d1 = %s, d2 = %s, d3 = %s\n", permission[d0],
		    permission[d1], permission[d2], permission[d3]);

	return strlen(buf);
}

static ssize_t emi_mpu_store(struct device_driver *driver, const char *buf, size_t count)
{
	int i;
	unsigned int start_addr;
	unsigned int end_addr;
	unsigned int region;
	unsigned int access_permission;
	char *command;
	char *ptr;
	char *token[5];

	if ((strlen(buf) + 1) > MAX_EMI_MPU_STORE_CMD_LEN) {
		pr_err("emi_mpu_store command overflow.");
		return count;
	}
	pr_err("emi_mpu_store: %s\n", buf);

	command = kmalloc((size_t) MAX_EMI_MPU_STORE_CMD_LEN, GFP_KERNEL);
	if (!command)
		return count;

	strcpy(command, buf);
	ptr = (char *)buf;

	if (!strncmp(buf, EN_MPU_STR, strlen(EN_MPU_STR))) {
		i = 0;
		while (ptr != NULL) {
			ptr = strsep(&command, " ");
			token[i] = ptr;
			pr_err("token[%d] = %s\n", i, token[i]);
			i++;
		}

		for (i = 0; i < 5; i++)
			pr_err("token[%d] = %s\n", i, token[i]);

		start_addr = kstrtoul(token[1], 16, (unsigned long *)&token[1]);
		end_addr = kstrtoul(token[2], 16, (unsigned long *)&token[2]);
		region = kstrtoul(token[3], 16, (unsigned long *)&token[3]);
		access_permission = kstrtoul(token[4], 16, (unsigned long *)&token[4]);
		emi_mpu_set_region_protection(start_addr, end_addr, region, access_permission);
		pr_err(
		       "Set EMI_MPU: start: 0x%x, end: 0x%x, region: %d, permission: 0x%x.\n",
		       start_addr, end_addr, region, access_permission);
	} else if (!strncmp(buf, DIS_MPU_STR, strlen(DIS_MPU_STR))) {
		i = 0;
		while (ptr != NULL) {
			ptr = strsep(&command, " ");
			token[i] = ptr;
			pr_err("token[%d] = %s\n", i, token[i]);
			i++;
		}

		for (i = 0; i < 5; i++)
			pr_err("token[%d] = %s\n", i, token[i]);

		start_addr = kstrtoul(token[1], 16, (unsigned long *)&token[1]);
		end_addr = kstrtoul(token[2], 16, (unsigned long *)&token[2]);
		region = kstrtoul(token[3], 16, (unsigned long *)&token[3]);
		emi_mpu_set_region_protection(0x0, 0x0, region,
					      SET_ACCESS_PERMISSON(NO_PROTECTION, NO_PROTECTION,
								   NO_PROTECTION, NO_PROTECTION));
		pr_err("set EMI MPU: start: 0x%x, end: 0x%x, region: %d, permission: 0x%x\n", 0, 0,
		       region, SET_ACCESS_PERMISSON(NO_PROTECTION, NO_PROTECTION, NO_PROTECTION,
						    NO_PROTECTION));
	} else {
		pr_err("Unknown emi_mpu command.\n");
	}

	kfree(command);

	return count;
}

DRIVER_ATTR(mpu_config, 0644, emi_mpu_show, emi_mpu_store);

void mtk_search_full_pgtab(void)
{
	pgd_t *pgd;
	pud_t *pud;
	pmd_t *pmd;
	pte_t *pte;
	unsigned long addr;
	unsigned long addr_2nd, addr_2nd_end;
	unsigned int v_addr = vio_addr;

	/*FIXME: testing */
	/* vio_addr = 0x9DE0D000; */

	for (addr = 0xC0000000; addr < 0xFFF00000; addr += 0x100000) {
		pgd = pgd_offset(&init_mm, addr);
		if (pgd_none(*pgd) || !pgd_present(*pgd))
			continue;

		pud = pud_offset(pgd, addr);
		if (pud_none(*pud) || !pud_present(*pud))
			continue;

		pmd = pmd_offset(pud, addr);
		if (pmd_none(*pmd) || !pmd_present(*pmd))
			continue;

#ifndef CONFIG_ARM_LPAE
		if ((pmd_val(*pmd) & PMD_TYPE_MASK) == PMD_TYPE_TABLE) {
			/* Page table entry */
			addr_2nd = addr;
			addr_2nd_end = addr_2nd + 0x100000;
			for (; addr_2nd < (addr_2nd_end); addr_2nd += 0x1000) {
				pte = pte_offset_map(pmd, addr_2nd);
				if (((unsigned long)v_addr & PAGE_MASK) ==
				    ((unsigned long)pte_val(*(pte)) & PAGE_MASK)) {
					pr_err
						("[EMI MPU] page entry section at pte: %lx. violation address = 0x%x\n",
					     (unsigned long)(pte), v_addr);
					return;
				}
			}
		} else {
			/* printk("[EMI MPU] Section pmd: %x, addr = 0x%x\n", (unsigned long)(pmd), addr); */
			/* Section */
			/* if(v_addr>>20 == (unsigned long)pmd_val(*(pmd))>>20) */
			if (((unsigned long)pmd_val(*(pmd)) & SECTION_MASK) ==
			    ((unsigned long)v_addr & SECTION_MASK)) {
				pr_err
				    ("[EMI MPU] page entry section at pmd: %lx. violation address = 0x%x\n",
				     (unsigned long)(pmd), v_addr);
				return;
			}
		}
#else
		/* TBD */
#endif
	}
	pr_err("[EMI MPU] ** Can not find page table entry! violation address = 0x%x ******\n",
	       v_addr);

}

void emi_mpu_work_callback(struct work_struct *work)
{
	pr_err("[EMI MPU] Enter EMI MPU workqueue!\n");
	mtk_search_full_pgtab();
	pr_err("[EMI MPU] Exit EMI MPU workqueue!\n");
}

static ssize_t pgt_scan_show(struct device_driver *driver, char *buf)
{
	return 0;
}

static ssize_t pgt_scan_store(struct device_driver *driver, const char *buf, size_t count)
{
	unsigned int value;
	unsigned int ret;

	if (unlikely(kstrtoint(buf, 0, &value) != 1))
		return -EINVAL;

	if (value == 1) {
		ret = queue_work(emi_mpu_workqueue, &emi_mpu_work);
		if (!ret)
			pr_err("[EMI MPU] submit workqueue failed, ret = %d\n", ret);
	}

	return count;
}

DRIVER_ATTR(pgt_scan, 0644, pgt_scan_show, pgt_scan_store);

#ifdef ENABLE_EMI_CHKER
static void emi_axi_set_chker(const unsigned int setting)
{
	int value;

	value = readl(IOMEM(EMI_CHKER));
	value &= ~(0x7 << 16);
	value |= (setting);

	mt_reg_sync_writel(value, EMI_CHKER);
}

static void emi_axi_set_master(const unsigned int setting)
{
	int value;

	value = readl(IOMEM(EMI_CHKER));
	value &= ~(0x0F << AXI_NON_ALIGN_CHK_MST);
	value |= (setting & 0xF) << AXI_NON_ALIGN_CHK_MST;

	mt_reg_sync_writel(value, EMI_CHKER);
}

static void emi_axi_dump_info(int aee_ke_en)
{
	int value, master_ID;
	char *master_name;

	value = readl(IOMEM(EMI_CHKER));
	master_ID = (value & 0x0000FFFF);

	if (value & 0x0000FFFF) {
		pr_err("AXI violation.\n");
		pr_err(
		       "[EMI MPU AXI] Debug info start ----------------------------------------\n");

		pr_err("EMI_CHKER = %x.\n", value);
		pr_err("Violation address is 0x%x.\n", readl(IOMEM(EMI_CHKER_ADR)));
		pr_err("Violation master ID is 0x%x.\n", master_ID);
		pr_err(
		       "Violation type is: AXI_ADR_CHK_EN(%d), AXI_LOCK_CHK_EN(%d), AXI_NON_ALIGN_CHK_EN(%d).\n",
		       (value & (1 << AXI_ADR_VIO)) ? 1 : 0,
		       (value & (1 << AXI_LOCK_ISSUE)) ? 1 : 0,
		       (value & (1 << AXI_NON_ALIGN_ISSUE)) ? 1 : 0);
		pr_err("%s violation.\n", (value & (1 << AXI_VIO_WR)) ? "Write" : "Read");

		pr_err(
		       "[EMI MPU AXI] Debug info end ----------------------------------------\n");

		master_name = __id2name(master_ID);
#ifdef CONFIG_MTK_AEE_FEATURE
		if (aee_ke_en)
			aee_kernel_exception("EMI MPU AXI",
					     "AXI violation.\EMI_CHKER = 0x%x, module is %s.\n",
					     value, master_name);
#endif
		/* clear AXI checker status */
		mt_reg_sync_writel((1 << AXI_VIO_CLR) | readl(IOMEM(EMI_CHKER)), EMI_CHKER);
	}
}

static void emi_axi_vio_timer_func(unsigned long a)
{
	emi_axi_dump_info(1);

	mod_timer(&emi_axi_vio_timer, jiffies + AXI_VIO_MONITOR_TIME);
}

static ssize_t emi_axi_vio_show(struct device_driver *driver, char *buf)
{
	int value;

	value = readl(IOMEM(EMI_CHKER));

	emi_axi_dump_info(0);

	return snprintf(buf, PAGE_SIZE,
			"AXI vio setting is: ADR_CHK_EN %s, LOCK_CHK_EN %s, NON_ALIGN_CHK_EN %s\n",
			(value & (1 << AXI_ADR_CHK_EN)) ? "ON" : "OFF",
			(value & (1 << AXI_LOCK_CHK_EN)) ? "ON" : "OFF",
			(value & (1 << AXI_NON_ALIGN_CHK_EN)) ? "ON" : "OFF");
}

ssize_t emi_axi_vio_store(struct device_driver *driver, const char *buf, size_t count)
{
	int value;
	int cpu = 0;		/* assign timer to CPU0 to avoid CPU plug-out and timer will be unavailable */

	value = readl(IOMEM(EMI_CHKER));

	if (!strncmp(buf, "ADR_CHK_ON", strlen("ADR_CHK_ON"))) {
		emi_axi_set_chker(1 << AXI_ADR_CHK_EN);
		add_timer_on(&emi_axi_vio_timer, cpu);
	} else if (!strncmp(buf, "LOCK_CHK_ON", strlen("LOCK_CHK_ON"))) {
		emi_axi_set_chker(1 << AXI_LOCK_CHK_EN);
		add_timer_on(&emi_axi_vio_timer, cpu);
	} else if (!strncmp(buf, "NON_ALIGN_CHK_ON", strlen("NON_ALIGN_CHK_ON"))) {
		emi_axi_set_chker(1 << AXI_NON_ALIGN_CHK_EN);
		add_timer_on(&emi_axi_vio_timer, cpu);
	} else if (!strncmp(buf, "OFF", strlen("OFF"))) {
		emi_axi_set_chker(0);
		del_timer(&emi_axi_vio_timer);
	} else
		pr_err("invalid setting\n");

	return count;
}

DRIVER_ATTR(emi_axi_vio, 0644, emi_axi_vio_show, emi_axi_vio_store);

#endif				/* #ifdef ENABLE_EMI_CHKER */

static struct platform_driver emi_mpu_ctrl_platform_drv = {
	.driver = {
		.name = "emi_mpu_ctrl",
		.bus = &platform_bus_type,
		.owner = THIS_MODULE,
	}
};
static int __init emi_mpu_mod_init(void)
{
	int ret;
	/*struct basic_dram_setting DRAM_setting;*/
	struct device_node *node;
	unsigned int mpu_irq = 0;

	/* DTS version */
	node = of_find_compatible_node(NULL, NULL, "mediatek,mt8173-emi");
	if (node) {
		/*emi irq is triggered by the device APC*/
		mpu_irq = irq_of_parse_and_map(node, 0);
		if (EMI_BASE_ADDR == NULL)
			EMI_BASE_ADDR = of_iomap(node, 0);
	} else
		return -1;

	spin_lock_init(&emi_mpu_lock);


#ifdef CONFIG_MTK_IN_HOUSE_TEE_SUPPORT
	/*pr_err("EMI MPU request in kernel.\n");*/

	ret = request_irq(mpu_irq, (irq_handler_t) mpu_violation_irq,
			IRQF_TRIGGER_LOW | IRQF_SHARED, "mt_emi_mpu",
			&emi_mpu_ctrl_platform_drv);

	if (ret != 0) {
		pr_err("Fail to request EMI_MPU interrupt. Error = %d.\n", ret);
		return ret;
	}
#endif

#if 0
	acquire_dram_setting(&DRAM_setting);
	pr_err("[EMI] EMI_CONA =  0x%x\n", readl(IOMEM(EMI_CONA)));
	pr_err("[EMI] Support channel number %d\n", DRAM_setting.channel_nr);
	pr_err("[EMI] Channel 0 : rank 0 : %d Gb, segment no : %d\n",
	       DRAM_setting.channel[0].rank[0].rank_size,
	       DRAM_setting.channel[0].rank[0].segment_nr);
	pr_err("[EMI] Channel 0 : rank 1 : %d Gb, segment no : %d\n",
	       DRAM_setting.channel[0].rank[1].rank_size,
	       DRAM_setting.channel[0].rank[1].segment_nr);
	pr_err("[EMI] Channel 1 : rank 0 : %d Gb, segment no : %d\n",
	       DRAM_setting.channel[1].rank[0].rank_size,
	       DRAM_setting.channel[1].rank[0].segment_nr);
	pr_err("[EMI] Channel 1 : rank 1 : %d Gb, segment no : %d\n",
	       DRAM_setting.channel[1].rank[1].rank_size,
	       DRAM_setting.channel[1].rank[1].segment_nr);
#endif

#ifdef ENABLE_EMI_CHKER
	/* AXI violation monitor setting and timer function create */
	mt_reg_sync_writel((1 << AXI_VIO_CLR) | readl(IOMEM(EMI_CHKER)), EMI_CHKER);
	emi_axi_set_master(MASTER_ALL);
	init_timer(&emi_axi_vio_timer);
	emi_axi_vio_timer.expires = jiffies + AXI_VIO_MONITOR_TIME;
	emi_axi_vio_timer.function = &emi_axi_vio_timer_func;
	emi_axi_vio_timer.data = ((unsigned long)0);
#endif				/* #ifdef ENABLE_EMI_CHKER */

#if !defined(USER_BUILD_KERNEL)
#ifdef ENABLE_EMI_CHKER
	/* Enable AXI 4KB boundary violation monitor timer */
	/* emi_axi_set_chker(1 << AXI_ADR_CHK_EN); */
	/* add_timer_on(&emi_axi_vio_timer, 0); */
#endif

	/* register driver and create sysfs files */
	ret = platform_driver_register(&emi_mpu_ctrl_platform_drv);
	if (ret)
		pr_err("Fail to register EMI_MPU driver.\n");

	ret = driver_create_file(&emi_mpu_ctrl_platform_drv.driver, &driver_attr_mpu_config);
	if (ret)
		pr_err("Fail to create MPU config sysfs file.\n");

#ifdef ENABLE_EMI_CHKER
	ret = driver_create_file(&emi_mpu_ctrl_platform_drv.driver, &driver_attr_emi_axi_vio);
	if (ret)
		pr_err("Fail to create AXI violation monitor sysfs file.\n");
#endif

	ret = driver_create_file(&emi_mpu_ctrl_platform_drv.driver, &driver_attr_pgt_scan);
	if (ret)
		pr_err("Fail to create pgt scan sysfs file.\n");
#endif


	/* Create a workqueue to search pagetable entry */
	emi_mpu_workqueue = create_singlethread_workqueue("emi_mpu");
	INIT_WORK(&emi_mpu_work, emi_mpu_work_callback);

	return 0;
}

static void __exit emi_mpu_mod_exit(void)
{
}


late_initcall(emi_mpu_mod_init);
module_exit(emi_mpu_mod_exit);

