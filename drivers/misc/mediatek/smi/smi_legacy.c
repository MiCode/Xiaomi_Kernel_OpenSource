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

#include <linux/fs.h>
#include <linux/clk.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/debugfs.h>
#include <linux/of_address.h>

#include <aee.h>
#include <mtk_smi.h>
#include <soc/mediatek/smi.h>
#include "smi_public.h"
#include "mmdvfs_mgr.h"
#include "mmdvfs_pmqos.h"

#if defined(CONFIG_MACH_MT6757)
#if defined(SMI_D1) || defined(SMI_D2) || defined(SMI_D3) \
	|| defined(SMI_J) ||  defined(SMI_EV) || defined(SMI_OLY)
#define MMDVFS_HOOK
#endif
#endif

#if IS_ENABLED(CONFIG_COMPAT)
#include <linux/compat.h>
#endif

#if IS_ENABLED(CONFIG_MACH_MT6758)
#include <clk-mt6758-pg.h>
#include "smi_config_default.h"
#elif IS_ENABLED(CONFIG_MACH_MT6763)
#include <clk-mt6763-pg.h>
#include "smi_config_mt6763.h"
#elif IS_ENABLED(CONFIG_MACH_MT6757)
#include <clk-mt6757-pg.h>
#include "smi_config_mt6757.h"
#elif IS_ENABLED(CONFIG_MACH_MT6765)
#include <clk-mt6765-pg.h>
#include "smi_config_mt6765.h"
#elif IS_ENABLED(CONFIG_MACH_MT6761)
#include <clk-mt6761-pg.h>
#include "smi_config_mt6761.h"
#elif IS_ENABLED(CONFIG_MACH_MT3967)
#include <clk-mt3967-pg.h>
#include "smi_config_mt3967.h"
#elif IS_ENABLED(CONFIG_MACH_MT6779)
#include <clk-mt6779-pg.h>
#include "smi_config_mt6779.h"
#include <mtk_qos_ipi.h>
#include <mtk_qos_sram.h>
#include <plat_debug_api.h>
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SSPM_SUPPORT)
#include <sspm_reservedmem_define.h>
#endif
#else
#include "smi_config_default.h"
#endif

#if IS_ENABLED(CONFIG_MTK_M4U)
#include <m4u.h>
#elif IS_ENABLED(CONFIG_MTK_PSEUDO_M4U)
#include <mach/pseudo_m4u.h>
#endif

#if IS_ENABLED(CONFIG_MMPROFILE)
#include <mmprofile.h>
#include <mmprofile_function.h>

struct smi_mmp_event_t {
	mmp_event smi;
	mmp_event mtcmos_dis;
	mmp_event mtcmos_vde;
	mmp_event mtcmos_ven;
	mmp_event mtcmos_isp;
	mmp_event mtcmos_ipe;
	mmp_event mtcmos_cam;
};

static struct smi_mmp_event_t smi_mmp_event;
#endif

#define DEV_NAME "MTK_SMI"
#undef pr_fmt
#define pr_fmt(fmt) "[" DEV_NAME "]" fmt

#define SMIDBG(string, args...) pr_debug(string, ##args)

#if IS_ENABLED(CONFIG_MTK_CMDQ) && !defined(CONFIG_MACH_MT6757)
#include <cmdq_core.h>
#define SMIWRN(cmdq, string, args...) \
	do { \
		if (cmdq != 0) \
			cmdq_core_save_first_dump(string, ##args); \
		pr_info(string, ##args); \
	} while (0)
#else
#define SMIWRN(cmdq, string, args...) pr_info(string, ##args)
#endif

#define SMIERR(string, args...) \
	do { \
		pr_notice(string, ##args); \
		aee_kernel_warning("%s:" string, __func__, ##args); \
	} while (0)

#ifndef ATOMR_CLK
#define ATOMR_CLK(i) \
	((i) < (SMI_LARB_NUM) ? atomic_read(&(larbs[(i)]->clk_ref_cnts)) : \
		atomic_read(&(common->clk_ref_cnts)))
#endif

static unsigned int smi_bwc_config_disable;
static unsigned int smi_mm_clk_first;

#define SF_HWC_PIXEL_MAX_NORMAL  (1920 * 1080 * 7)
struct MTK_SMI_BWC_MM_INFO g_smi_bwc_mm_info = {
	0, 0, {0, 0}, {0, 0}, {0, 0}, {0, 0}, 0, 0, 0,
	SF_HWC_PIXEL_MAX_NORMAL
};

struct smi_device {
	char		*name;
	dev_t		dev_no;
	struct cdev	*cdev;
	struct class	*class;
	struct device	*device;
};

struct smi_driver {
	spinlock_t		lock;
	int			table[SMI_BWC_SCEN_CNT];
	enum MTK_SMI_BWC_SCEN	scen;
};

struct mmsys_config {
	void __iomem	*base;
	unsigned int	nr_debugs;
	unsigned int	*debugs;
};

struct smi_record_t {
	/* clk from api */
	char user[NAME_MAX];
	u64 clk_sec;
	u32 clk_nsec;
	/* mtcmos cb from ccf */
	u64 sec;
	u32 nsec;
};

static struct smi_device *smi_dev;
static struct smi_driver *smi_drv;
static struct mmsys_config *smi_mmsys;
static struct smi_record_t smi_record[SMI_LARB_NUM][2];

#if IS_ENABLED(CONFIG_MACH_MT6779)
static u32 smi_dram_dump;
const u32 SMI_L1ARB_LARB[SMI_LARB_MMDVFS_NR] = {

	0, 1, 2, 3, SMI_COMM_MASTER_NR, 4, SMI_COMM_MASTER_NR,
	5, 5, 7, 6, SMI_COMM_MASTER_NR};

const bool SMI_BUS_SEL_MASTER[SMI_COMM_MASTER_NR] = {0, 1, 1, 0, 1, 1, 1, 1};

void smi_ostd_update(struct plist_head *head)
{
	struct mm_qos_request *req;
	u32 curr, port, off, ostd;

	if (plist_head_empty(head))
		return;

	plist_for_each_entry(req, head, owner_node) {
		if (!req->updated)
			continue;
		req->updated = false;
		curr = SMI_LARB_ID_GET(req->master_id);
		if (curr >= SMI_LARB_NUM)
			break;

		port = SMI_PORT_ID_GET(req->master_id);
		off = SMI_LARB_OSTDL_PORT(port);
		if (!req->ostd)
			ostd = 0x1;
		else if (curr == 1 && (port == 9 || port == 11) &&
			req->ostd > 0x7) /* workaround */
			ostd = 0x7;
		else if (req->ostd > 0x28)
			ostd = 0x28;
		else
			ostd = req->ostd;
		smi_scen_pair[curr][SMI_ESL_INIT][port].value = ostd;

		if (mtk_smi_clk_ref_cnts_read(larbs[curr])) {
			smi_bus_prepare_enable(curr, "MMDVFS", 1);
			writel(ostd, larbs[curr]->base + off);
			smi_bus_disable_unprepare(curr, "MMDVFS", 1);
		}
	}
}

void smi_bwl_update(const u32 comm_port, const u32 bwl, const bool soft)
{
	u32 val;

	if (comm_port >= SMI_COMM_MASTER_NR) {
		SMIDBG("Invalid common port=%u, SMI_COMM_MASTER_NR=%u\n",
			comm_port, SMI_COMM_MASTER_NR);
		return;
	}
	val = (soft ? 0x1000 : 0x3000) | (bwl & 0xFFF);
	smi_scen_pair[SMI_LARB_NUM][SMI_ESL_INIT][comm_port].value = val;

	if (mtk_smi_clk_ref_cnts_read(common)) {
		smi_bus_prepare_enable(common->index, "MMDVFS", 1);
		writel(val, common->base +
		smi_scen_pair[SMI_LARB_NUM][SMI_ESL_INIT][comm_port].offset);
		smi_bus_disable_unprepare(common->index, "MMDVFS", 1);
	}
}
#endif

/* ***********************************************
 * get smi base address of COMMON or specific LARB
 * reg_indx: select specific LARB or COMMON
 * **********************************************/
void __iomem *smi_base_addr_get(const unsigned int reg_indx)
{
	if (reg_indx > SMI_LARB_NUM) {
		SMIDBG("Invalid reg_indx=%u, SMI_LARB_NUM=%u\n",
			reg_indx, SMI_LARB_NUM);
		return NULL;
	}
	return (reg_indx == SMI_LARB_NUM) ?
		common->base : larbs[reg_indx]->base;
}

void __iomem *smi_mmsys_base_addr_get(void)
{
	if (!smi_mmsys || !smi_mmsys->base) {
		SMIDBG("MMSYS no device or address\n");
		return NULL;
	}
	return smi_mmsys->base;
}

bool smi_mm_clk_first_get(void)
{
	return smi_mm_clk_first ? true : false;
}
EXPORT_SYMBOL_GPL(smi_mm_clk_first_get);

static void smi_clk_record(const u32 id, const bool en, const char *user)
{
	struct smi_record_t *record;

	if (id >= SMI_LARB_NUM) {
		SMIDBG("Invalid id:%u, LARB_NUM=%d, user=%s\n",
			id, SMI_LARB_NUM, user);
		return;
	}

	record = &smi_record[id][en ? 1 : 0];
	if (user) {
		record->clk_sec = sched_clock();
		record->clk_nsec = do_div(record->clk_sec, 1000000000) / 1000;
		strncpy(record->user, user, NAME_MAX);
		record->user[sizeof(record->user) - 1] = '\0';
	} else {
	record->sec = sched_clock();
	record->nsec = do_div(record->sec, 1000000000) / 1000;
	}
}

static inline s32 smi_unit_prepare_enable(const u32 id)
{
	struct mtk_smi_dev *smi = (id < SMI_LARB_NUM ? larbs[id] : common);
	s32 ret;

	ret = clk_prepare_enable(smi->clks[0]);
	if (ret) {
		SMIERR("SMI%u MTCMOS enable failed: %d\n", id, ret);
		return ret;
	}
	ret = mtk_smi_dev_enable(smi);
	if (ret)
		return ret;
	return ret;
}

static s32 smi_bus_prepare_enable_k414(const u32 id, const char *user)
{
	s32 ret;

	if (id > SMI_LARB_NUM) {
		SMIDBG("Invalid id:%u, LARB_NUM=%d, user=%s\n",
			id, SMI_LARB_NUM, user);
		return -EINVAL;
	} else if (id < SMI_LARB_NUM && ATOMR_CLK(id) == 0)
		smi_clk_record(id, true, user);

	ret = smi_unit_prepare_enable(SMI_LARB_NUM);
	if (ret || id == SMI_LARB_NUM)
		return ret;
	return smi_unit_prepare_enable(id);
}

static inline void smi_unit_disable_unprepare(const u32 id)
{
	struct mtk_smi_dev *smi = (id < SMI_LARB_NUM ? larbs[id] : common);

	mtk_smi_dev_disable(smi);
	clk_disable_unprepare(smi->clks[0]);
}

static s32 smi_bus_disable_unprepare_k414(const u32 id, const char *user)
{
	if (id > SMI_LARB_NUM) {
		SMIDBG("Invalid id:%u, LARB_NUM=%d, user=%s\n",
			id, SMI_LARB_NUM, user);
		return -EINVAL;
	} else if (id == SMI_LARB_NUM) {
		smi_unit_disable_unprepare(id);
		return 0;
	} else if (ATOMR_CLK(id) == 1) {
		smi_clk_record(id, false, user);
		if (readl(larbs[id]->base + SMI_LARB_STAT))
			SMIWRN(1, "LARB%u OFF by%16s but busy\n", id, user);
		}

	smi_unit_disable_unprepare(id);
	smi_unit_disable_unprepare(SMI_LARB_NUM);
	return 0;
}

/* ********************************************************
 * prepare and enable CG/MTCMOS of COMMON and specific LARB
 * reg_indx: select specific LARB or COMMON
 * user_name: caller's module name, used for debug
 * mtcmos: wish to manipulate power with mtcmos = 1
 * *******************************************************/
int smi_bus_prepare_enable(const unsigned int reg_indx,
	const char *user_name, const bool mtcmos)
{
	return smi_bus_prepare_enable_k414(reg_indx, user_name);
}
EXPORT_SYMBOL_GPL(smi_bus_prepare_enable);

/* ***********************************************************
 * disable and unprepare CG/MTCMOS of specific LARB and COMMON
 * reg_indx: select specific LARB or COMMON
 * user_name: caller's module name, used for debug
 * mtcmos: wish to manipulate power with mtcmos = 1
 * **********************************************************/
int smi_bus_disable_unprepare(const unsigned int reg_indx,
	const char *user_name, const bool mtcmos)
{
	return smi_bus_disable_unprepare_k414(reg_indx, user_name);
}
EXPORT_SYMBOL_GPL(smi_bus_disable_unprepare);

static int smi_larb_non_sec_con_set(const unsigned int reg_indx)
{
	struct mtk_smi_dev *smi = larbs[reg_indx];
	int i;

	if (reg_indx >= SMI_LARB_NUM) {
		SMIDBG("Invalid reg_indx=%u, SMI_LARB_NUM=%u\n",
			reg_indx, SMI_LARB_NUM);
		return -EINVAL;
	} else if (!smi) {
		SMIDBG("No such device or address\n");
		return -ENXIO;
	} else if (!smi->base) {
		SMIDBG("LARB%u no such device or address\n", smi->index);
		return -ENXIO;
	} else if (!mtk_smi_clk_ref_cnts_read(smi)) {
		SMIDBG("LARB%u without mtcmos\n", smi->index);
		return 0;
	}

	for (i = smi_larb_cmd_gr_en_port[reg_indx][0];
		i < smi_larb_cmd_gr_en_port[reg_indx][1]; i++) /* CMD_GR */
		writel(readl(smi->base + SMI_LARB_NON_SEC_CON(i)) | 0x2,
			smi->base + SMI_LARB_NON_SEC_CON(i));
	for (i = smi_larb_bw_thrt_en_port[reg_indx][0];
		i < smi_larb_bw_thrt_en_port[reg_indx][1]; i++) /* BW_THRT */
		writel(readl(smi->base + SMI_LARB_NON_SEC_CON(i)) | 0x8,
			smi->base + SMI_LARB_NON_SEC_CON(i));
	return 0;
}

static unsigned int smi_clk_subsys_larbs(enum subsys_id sys)
{
#if IS_ENABLED(CONFIG_MACH_MT6758)
	switch (sys) {
	case SYS_MM0:
		return 0x3; /* larb 0 & larb 1 */
	case SYS_CAM:
		return 0x48; /* larb 3 & larb 6 */
	case SYS_ISP:
		return 0x24; /* larb 2 & larb 5 */
	case SYS_VEN:
		return 0x80; /* larb 7 */
	case SYS_VDE:
		return 0x10; /* larb 4 */
	default:
		return 0x0;
	}
#elif IS_ENABLED(CONFIG_MACH_MT6763)
	switch (sys) {
	case SYS_DIS:
		return 0x1; /* larb 0 */
	case SYS_CAM:
		return 0x4; /* larb 2 */
	case SYS_ISP:
		return 0x2; /* larb 1 */
	case SYS_VEN:
		return 0x8; /* larb 3 */
	default:
		return 0x0;
	}
#elif IS_ENABLED(CONFIG_MACH_MT6765)
	switch (sys) {
	case SYS_DIS:
		return 0x1; /* larb 0 */
	case SYS_CAM:
		return 0x8; /* larb 3 */
	case SYS_ISP:
		return 0x4; /* larb 2 */
	case SYS_VCODEC:
		return 0x2; /* larb 1 */
	default:
		return 0x0;
	}
#elif IS_ENABLED(CONFIG_MACH_MT6761)
	switch (sys) {
	case SYS_DIS:
		return 0x1; /* larb 0 */
	case SYS_CAM:
		return 0x4; /* larb 2 */
	case SYS_VCODEC:
		return 0x2; /* larb 1 */
	default:
		return 0x0;
	}
#elif IS_ENABLED(CONFIG_MACH_MT3967)
	switch (sys) {
	case SYS_DIS:
		return 0x1; /* larb 0 */
	case SYS_VEN:
		return 0x10; /* larb 4 */
	case SYS_VDE:
		return 0x2; /* larb 1 */
	case SYS_CAM:
		return 0x48; /* larb 3 & larb 6 */
	case SYS_ISP:
		return 0x24; /* larb 2 & larb 5 */
	default:
		return 0x0;
	}
#elif IS_ENABLED(CONFIG_MACH_MT6779)
	switch (sys) {
	case SYS_DIS:
		return ((1 << 0) | (1 << 1) | (1 << SMI_LARB_NUM));
	case SYS_VDE:
		return (1 << 2);
	case SYS_VEN:
		return (1 << 3);
	case SYS_ISP:
		return ((1 << 5) | (1 << 6));
	case SYS_IPE:
		return ((1 << 7) | (1 << 8));
	case SYS_CAM:
		return ((1 << 9) | (1 << 10) | (1 << 11));
	default:
		return 0;
	}
#elif IS_ENABLED(CONFIG_MACH_MT6757)
	switch (sys) {
	case SYS_DIS:
		return (1 << 0) | (1 << 4) | (1 << SMI_LARB_NUM);
	case SYS_VDE:
		return (1 << 1);
	case SYS_CAM:
		return (1 << 2);
	case SYS_VEN:
		return (1 << 3);
	case SYS_ISP:
		return (1 << 5);
	default:
		return 0x0;
	}
#endif
	return 0;
}

static void smi_mmp_event_log(enum subsys_id sys, const bool en)
{
#if IS_ENABLED(CONFIG_MMPROFILE)
	mmp_event mtcmos = 0;

#if IS_ENABLED(CONFIG_MACH_MT6779)
	switch (sys) {
	case SYS_DIS:
		mtcmos = smi_mmp_event.mtcmos_dis;
		break;
	case SYS_VDE:
		mtcmos = smi_mmp_event.mtcmos_vde;
		break;
	case SYS_VEN:
		mtcmos = smi_mmp_event.mtcmos_ven;
		break;
	case SYS_ISP:
		mtcmos = smi_mmp_event.mtcmos_isp;
		break;
	case SYS_IPE:
		mtcmos = smi_mmp_event.mtcmos_ipe;
		break;
	case SYS_CAM:
		mtcmos = smi_mmp_event.mtcmos_cam;
		break;
	default:
		break;
	}
#endif
	mmprofile_log_ex(
		mtcmos, (en ? MMPROFILE_FLAG_START : MMPROFILE_FLAG_END), 0, 0);
#endif
}

static void smi_clk_subsys_after_on(enum subsys_id sys)
{
	unsigned int subsys = smi_clk_subsys_larbs(sys);
	int i, smi_scen = smi_scen_map[smi_drv->scen];

	/* COMMON */
	if (subsys & 1) { /* COMMON and LARB0 in SYS_MM0 or SYS_DIS */
		mtk_smi_dev_enable(common);
		mtk_smi_config_set(common, SMI_SCEN_NUM);
		mtk_smi_config_set(common, smi_scen);
		mtk_smi_dev_disable(common);
	}
	/* LARBs */
	for (i = 0; i < SMI_LARB_NUM; i++)
		if (subsys & (1 << i)) {
			smi_clk_record(i, true, NULL);
			mtk_smi_dev_enable(larbs[i]);
			mtk_smi_config_set(larbs[i], SMI_SCEN_NUM);
			mtk_smi_config_set(larbs[i], smi_scen);
			smi_larb_non_sec_con_set(i);
			mtk_smi_dev_disable(larbs[i]);
		}
	smi_mmp_event_log(sys, true);
#if IS_ENABLED(CONFIG_MACH_MT6779)
	for (i = 0; i < SMI_LARB_NUM; i++)
		if ((subsys & (1 << i)) && (sys != SYS_DIS))
			mtk_smi_dev_enable(larbs[i]);
	spin_lock(&(smi_drv->lock));
	qos_sram_write(MM_SMI_CLK, qos_sram_read(MM_SMI_CLK) | subsys);
	spin_unlock(&(smi_drv->lock));
	qos_sram_write(MM_SMI_CLR, subsys);
	while (qos_sram_read(MM_SMI_EXE) && qos_sram_read(MM_SMI_CLR))
		;
#endif
}

static void smi_clk_subsys_before_off(enum subsys_id sys)
{
	u32 subsys = smi_clk_subsys_larbs(sys);
	s32 i;

	for (i = 0; i < SMI_LARB_NUM; i++)
		if (subsys & (1 << i))
			smi_clk_record(i, false, NULL);
	smi_mmp_event_log(sys, false);
#if IS_ENABLED(CONFIG_MACH_MT6779)
	spin_lock(&(smi_drv->lock));
	qos_sram_write(MM_SMI_CLK, qos_sram_read(MM_SMI_CLK) & ~subsys);
	spin_unlock(&(smi_drv->lock));
	qos_sram_write(MM_SMI_CLR, subsys);
	while (qos_sram_read(MM_SMI_EXE) && qos_sram_read(MM_SMI_CLR))
		;
	for (i = 0; i < SMI_LARB_NUM; i++)
		if ((subsys & (1 << i)) && (sys != SYS_DIS))
			mtk_smi_dev_disable(larbs[i]);
#endif
}

#if IS_ENABLED(CONFIG_MACH_MT6779)
static void smi_clk_subsys_debug_dump(enum subsys_id sys)
{
	u32 subsys = smi_clk_subsys_larbs(sys);

	if (subsys)
		smi_debug_bus_hang_detect(
			SMI_PARAM_BUS_OPTIMIZATION, true, false, true);
}
#endif

static struct pg_callbacks smi_clk_subsys_handle = {
	.after_on = smi_clk_subsys_after_on,
	.before_off = smi_clk_subsys_before_off,
#if IS_ENABLED(CONFIG_MACH_MT6779)
	.debug_dump = smi_clk_subsys_debug_dump,
#endif
};

LIST_HEAD(cb_list);
/* ********************************************
 * register callback function:
 * callback only when esl golden setting change
 * cb: callback structure from callee
 * *******************************************/
struct smi_bwc_scen_cb *smi_bwc_scen_cb_register(struct smi_bwc_scen_cb *cb)
{
	INIT_LIST_HEAD(&cb->list);
	list_add(&cb->list, &cb_list);
	return cb;
}
EXPORT_SYMBOL_GPL(smi_bwc_scen_cb_register);

static char *smi_bwc_scen_name_get(enum MTK_SMI_BWC_SCEN scen)
{
	switch (scen) {
	/* mtkcam */
	case SMI_BWC_SCEN_NORMAL: /* NONE */
		return "SMI_BWC_SCEN_NORMAL";
	case SMI_BWC_SCEN_CAM_PV: /* CAMERA_PREVIEW */
		return "SMI_BWC_SCEN_CAM_PV";
	case SMI_BWC_SCEN_CAM_CP: /* CAMERA_CAPTURE */
		return "SMI_BWC_SCEN_CAM_CP";
	case SMI_BWC_SCEN_ICFP: /* CAMERA_ICFP */
		return "SMI_BWC_SCEN_ICFP";
	/* CAMERA_ZSD, VIDEO_TELEPHONY, VIDEO_RECORD_CAMERA, VIDEO_NORMAL */
	case SMI_BWC_SCEN_VR:
		return "SMI_BWC_SCEN_VR";
	case SMI_BWC_SCEN_VR_SLOW: /* VIDEO_RECORD_SLOWMOTION */
		return "SMI_BWC_SCEN_VR_SLOW";
	case SMI_BWC_SCEN_VSS: /* VIDEO_SNAPSHOT */
		return "SMI_BWC_SCEN_VSS";
	case SMI_BWC_SCEN_FORCE_MMDVFS: /* FORCE_MMDVFS */
		return "SMI_BWC_SCEN_FORCE_MMDVFS";

	/* libvcodec */
	case SMI_BWC_SCEN_VP: /* VIDEO_PLAYBACK */
		return "SMI_BWC_SCEN_VP";
	case SMI_BWC_SCEN_VP_HIGH_FPS: /* VIDEO_PLAYBACK_HIGH_FPS */
		return "SMI_BWC_SCEN_VP_HIGH_FPS";
	case SMI_BWC_SCEN_VP_HIGH_RESOLUTION: /* VIDEO_PLAYBACK_HIGH_RESOL */
		return "SMI_BWC_SCEN_VP_HIGH_RESOLUTION";
	case SMI_BWC_SCEN_VENC: /* VIDEO_RECORD: without CAM */
		return "SMI_BWC_SCEN_VENC";
	case SMI_BWC_SCEN_MM_GPU: /* VIDEO_LIVE_PHOTO */
		return "SMI_BWC_SCEN_MM_GPU";

	/* libstagefright/wifi-display, crossmountlib */
	case SMI_BWC_SCEN_WFD: /* VIDEO_WIFI_DISPLAY */
		return "SMI_BWC_SCEN_WFD";

	/* unuse so far */
	case SMI_BWC_SCEN_SWDEC_VP: /* VIDEO_SWDEC_PLAYBACK */
		return "SMI_BWC_SCEN_SWDEC_VP";
	case SMI_BWC_SCEN_UI_IDLE: /* video */
		return "SMI_BWC_SCEN_UI_IDLE";
	case SMI_BWC_SCEN_VPMJC: /* mjc */
		return "SMI_BWC_SCEN_VPMJC";
	case SMI_BWC_SCEN_HDMI:
		return "SMI_BWC_SCEN_HDMI";
	case SMI_BWC_SCEN_HDMI4K:
		return "SMI_BWC_SCEN_HDMI4K";
	default:
		return "SMI_BWC_SCEN_UNKNOWN";
	}
}

static int smi_bwc_config(struct MTK_SMI_BWC_CONFIG *config)
{
	struct smi_bwc_scen_cb *cb;
	bool flag = true;
	int i, scen;

	if (smi_bwc_config_disable) {
		SMIDBG("Disable configure SMI BWC profile\n");
		return 0;
	}
	if (!config) {
		SMIDBG("struct MTK_SMI_BWC_CONFIG config no such address\n");
		return -ENXIO;
	}
	if (config->scenario < 0 || config->scenario >= SMI_BWC_SCEN_CNT) {
		SMIDBG("Invalid config scnenario=%d, SMI_BWC_SCEN_CNT=%u\n",
			config->scenario, SMI_BWC_SCEN_CNT);
		return -EINVAL;
	}
	/* table and concurrency of scenario */
	spin_lock(&(smi_drv->lock));
	scen = config->scenario;
	if (!config->b_on_off) {
		if (smi_drv->table[scen] <= 0)
			SMIDBG("%s(%d, %d) OFF not in pairs=%d\n",
				smi_bwc_scen_name_get(scen), scen,
				smi_scen_map[scen], smi_drv->table[scen]);
		else
			smi_drv->table[scen] -= 1;
	} else
		smi_drv->table[scen] += 1;

	for (i = SMI_BWC_SCEN_CNT - 1; i >= 0; i--)
		if (smi_drv->table[i])
			break;
	if (smi_scen_map[i] == smi_scen_map[smi_drv->scen])
		SMIDBG("prev=%s(%d, %d), ioctl=%s(%d, %d)%s, curr=%s(%d, %d)\n",
			smi_bwc_scen_name_get(smi_drv->scen),
			smi_drv->scen, smi_scen_map[smi_drv->scen],
			smi_bwc_scen_name_get(scen), scen, smi_scen_map[scen],
			config->b_on_off ? "ON" : "OFF",
			smi_bwc_scen_name_get(i), i, smi_scen_map[i]);
	else
		flag = false;

	smi_drv->scen = (i > 0 ? (enum MTK_SMI_BWC_SCEN)i : 0);
	spin_unlock(&(smi_drv->lock));
#ifdef MMDVFS_HOOK
	if (!SMI_PARAM_DISABLE_MMDVFS) {
		unsigned int concurrency = 0;

		if (config->b_on_off)
			mmdvfs_notify_scenario_enter(scen);
		else
			mmdvfs_notify_scenario_exit(scen);

		for (i = 0; i < SMI_BWC_SCEN_CNT; i++)
			concurrency |= (smi_drv->table[i] ? 1 : 0) << i;
		mmdvfs_notify_scenario_concurrency(concurrency);
	}
#endif
	if (flag)
		return 0;
	/* set config and callback */
	mtk_smi_config_set(common, smi_scen_map[smi_drv->scen]);
	for (i = 0; i < SMI_LARB_NUM; i++)
		mtk_smi_config_set(larbs[i], smi_scen_map[smi_drv->scen]);

	list_for_each_entry(cb, &cb_list, list)
		if (cb->smi_bwc_scen_cb_handle)
			cb->smi_bwc_scen_cb_handle(smi_drv->scen);

	SMIWRN(0, "ioctl=%s(%d, %d) %s, curr=%s(%d, %d)\n",
		smi_bwc_scen_name_get(scen), scen, smi_scen_map[scen],
		config->b_on_off ? "ON" : "OFF",
		smi_bwc_scen_name_get(smi_drv->scen),
		smi_drv->scen, smi_scen_map[smi_drv->scen]);
	return 0;
}

static int smi_bwc_info_set(struct MTK_SMI_BWC_INFO_SET *config)
{
	if (!config) {
		SMIDBG("struct MTK_SMI_BWC_INFO_SET config no such address\n");
		return -ENXIO;
	}

	switch (config->property) {
	case SMI_BWC_INFO_CON_PROFILE:
		g_smi_bwc_mm_info.concurrent_profile = config->value1;
		break;
	case SMI_BWC_INFO_SENSOR_SIZE:
		g_smi_bwc_mm_info.sensor_size[0] = config->value1;
		g_smi_bwc_mm_info.sensor_size[1] = config->value2;
		break;
	case SMI_BWC_INFO_VIDEO_RECORD_SIZE:
		g_smi_bwc_mm_info.video_record_size[0] = config->value1;
		g_smi_bwc_mm_info.video_record_size[1] = config->value2;
		break;
	case SMI_BWC_INFO_DISP_SIZE:
		g_smi_bwc_mm_info.display_size[0] = config->value1;
		g_smi_bwc_mm_info.display_size[1] = config->value2;
		break;
	case SMI_BWC_INFO_TV_OUT_SIZE:
		g_smi_bwc_mm_info.tv_out_size[0] = config->value1;
		g_smi_bwc_mm_info.tv_out_size[1] = config->value2;
		break;
	case SMI_BWC_INFO_FPS:
		g_smi_bwc_mm_info.fps = config->value1;
		break;
	case SMI_BWC_INFO_VIDEO_ENCODE_CODEC:
		g_smi_bwc_mm_info.video_encode_codec = config->value1;
		break;
	case SMI_BWC_INFO_VIDEO_DECODE_CODEC:
		g_smi_bwc_mm_info.video_decode_codec = config->value1;
		break;
	default:
		SMIDBG("struct MTK_SMI_BWC_INFO_SET config property unknown\n");
		break;
	}
	return 0;
}

static inline void smi_debug_print(const bool gce, const bool off_en,
	const u32 num, const u32 *off, const u32 *val)
{
	char buf[LINK_MAX + 1];
	s32 len, i, j, ret;

	for (i = 0; i < num; i += j) {
		len = 0;
		for (j = 0; j + i < num; j++) {
			if (off_en)
				ret = snprintf(buf + len, LINK_MAX - len,
					" %#x,", off[i + j]);
			else if (val[i + j])
				ret = snprintf(buf + len, LINK_MAX - len,
					" %#x=%#x,", off[i + j], val[i + j]);
			else
				ret = 0;

			if (ret < 0 || len + ret >= LINK_MAX) {
				snprintf(buf + len, LINK_MAX - len, "%c", '\0');
				break;
			}
			len += ret;
		}
		SMIWRN(gce, "%s\n", buf);
	}
}

static s32 smi_debug_dumper(const bool gce, const bool off, const u32 id)
{
	char *name;
	void __iomem *base;
	u32 nr_debugs, *debugs, temp[MAX_INPUT];
	s32 i, j;

	if (id > SMI_LARB_NUM + 1) {
		SMIDBG("Invalid id:%u, LARB_NUM=%d\n", id, SMI_LARB_NUM);
		return -EINVAL;
	}

	j = (id > SMI_LARB_NUM ? SMI_LARB_NUM : id);
	name = (id > SMI_LARB_NUM ? "MMSYS" :
		(id < SMI_LARB_NUM ? "LARB" : "COMM"));
	base = (id > SMI_LARB_NUM ? smi_mmsys->base :
		(id < SMI_LARB_NUM ? larbs[id]->base : common->base));
	nr_debugs = (id > SMI_LARB_NUM ? smi_mmsys->nr_debugs :
		(id < SMI_LARB_NUM ? larbs[id]->nr_debugs : common->nr_debugs));
	debugs = (id > SMI_LARB_NUM ? smi_mmsys->debugs :
		(id < SMI_LARB_NUM ? larbs[id]->debugs : common->debugs));
	if (!base || !nr_debugs || !debugs) {
		SMIDBG("Invalid base, nr_debugs, debugs of %s%u\n", name, id);
		return -ENXIO;
	}

	if (off) {
		SMIWRN(gce, "========== %s%u offset ==========\n", name, id);
		smi_debug_print(gce, off, nr_debugs, debugs, NULL);
		return 0;
	}

	for (i = 0; i < nr_debugs && ATOMR_CLK(j) > 0; i++)
		temp[i] = readl(base + debugs[i]);
	if (i < nr_debugs) {
		SMIWRN(gce, "========== %s%u OFF ==========\n", name, id);
		return 0;
	}

	SMIWRN(gce, "========== %s%u non-zero value, clk:%d ==========\n",
		name, id, ATOMR_CLK(j));
	smi_debug_print(gce, off, nr_debugs, debugs, temp);
	return 0;
}

static void smi_debug_dump_status(const bool gce)
{
	s32 i;

	for (i = 0; i <= SMI_LARB_NUM + 1; i++)
		smi_debug_dumper(gce, false, i);

	SMIWRN(gce, "SCEN=%s(%d), SMI_SCEN=%d\n",
		smi_bwc_scen_name_get(smi_drv->scen), smi_drv->scen,
		smi_scen_map[smi_drv->scen]);
}

static s32 smi_debug_bus_hang_detect_k414(const bool gce)
{
	u32 time = 5, busy[SMI_LARB_NUM + 1] = {0};
	s32 i, j, ret = 0;

#if IS_ENABLED(CONFIG_MACH_MT6779)
	dump_emi_outstanding();
#endif
#if IS_ENABLED(CONFIG_MTK_M4U)
	m4u_dump_reg_for_smi_hang_issue();
#endif
	for (i = 0; i < time; i++) {
		for (j = 0; j < SMI_LARB_NUM; j++)
			busy[j] += ((ATOMR_CLK(j) > 0 &&
				readl(larbs[j]->base + SMI_LARB_STAT)) ? 1 : 0);
		busy[j] += ((ATOMR_CLK(j) > 0 &&
			!(readl(common->base + SMI_DEBUG_MISC) & 0x1)) ? 1 : 0);
	}

	for (i = 0; i < SMI_LARB_NUM && !ret; i++)
		ret = (busy[i] == time ? i : ret);
	if (!ret || busy[SMI_LARB_NUM] < time) {
		SMIWRN(gce, "%s:SMI MM bus NOT hang\n", __func__);
		smi_debug_dump_status(gce);
		return 0;
		}

	SMIWRN(gce, "%s:SMI MM bus may hang by M4U/EMI/DVFS\n", __func__);
	for (i = 0; i < time; i++)
		for (j = 0; j <= SMI_LARB_NUM + 1; j++) {
			if (!i && j && j < SMI_LARB_NUM) /* offset */
				continue;
			smi_debug_dumper(gce, !i, j);
	}
	smi_debug_dump_status(gce);

	for (i = 0; i < SMI_LARB_NUM; i++)
		SMIWRN(gce,
			"LARB%u=%u/%u busy with clk:%d, COMMON=%u/%u busy with clk:%d\n",
			i, busy[i], time, ATOMR_CLK(i),
			busy[SMI_LARB_NUM], time, ATOMR_CLK(SMI_LARB_NUM));

	for (i = 0; i < SMI_LARB_NUM; i++)
		SMIWRN(gce,
			"LARB%u:[OFF]%16s[%5llu.%6u],CCF[%5llu.%6u];[ON]%16s[%5llu.%6u],CCF[%5llu.%6u]\n",
			i, smi_record[i][0].user,
			smi_record[i][0].clk_sec, smi_record[i][0].clk_nsec,
			smi_record[i][0].sec, smi_record[i][0].nsec,
			smi_record[i][1].user,
			smi_record[i][1].clk_sec, smi_record[i][1].clk_nsec,
			smi_record[i][1].sec, smi_record[i][1].nsec);
	return 0;
}

/* ********************************************
 * bus hang detect for debug smi status
 * reg_indx: check for specific LARBs
 * dump: dump complete log to kernel log or not
 * gce: write log to gce buffer or not
 * m4u: call m4u debug dump api or not
 * *******************************************/
int smi_debug_bus_hang_detect(unsigned int reg_indx, const bool dump,
	const bool gce, const bool m4u)
{
	return smi_debug_bus_hang_detect_k414(gce);
}
EXPORT_SYMBOL_GPL(smi_debug_bus_hang_detect);

static int smi_open(struct inode *inode, struct file *file)
{
	file->private_data = kcalloc(SMI_BWC_SCEN_CNT, sizeof(unsigned int),
		GFP_ATOMIC);

	if (!file->private_data) {
		SMIERR("file private data allocate failed\n");
		return -ENOMEM;
	}
	return 0;
}

static int smi_release(struct inode *inode, struct file *file)
{
	kfree(file->private_data);
	file->private_data = NULL;
	return 0;
}

static long smi_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret = 0;

	if (!file->f_op || !file->f_op->unlocked_ioctl)
		return -ENOTTY;

	switch (cmd) {
	case MTK_IOC_SMI_BWC_CONFIG:
	{
		struct MTK_SMI_BWC_CONFIG config;

		ret = copy_from_user(&config, (void *)arg,
			sizeof(struct MTK_SMI_BWC_CONFIG));
		if (ret)
			SMIWRN(0, "cmd %u copy_from_user fail: %d\n", cmd, ret);
		else
			ret = smi_bwc_config(&config);
		break;
	}

	case MTK_IOC_SMI_BWC_INFO_SET:
	{
		struct MTK_SMI_BWC_INFO_SET config;

		ret = copy_from_user(&config, (void *)arg,
			sizeof(struct MTK_SMI_BWC_INFO_SET));
		if (ret)
			SMIWRN(0, "cmd %u copy_from_user fail: %d\n", cmd, ret);
		else
			ret = smi_bwc_info_set(&config);
		break;
	}

	case MTK_IOC_SMI_BWC_INFO_GET:
	{
		ret = copy_to_user((void *)arg, (void *)&g_smi_bwc_mm_info,
			sizeof(struct MTK_SMI_BWC_MM_INFO));
		if (ret)
			SMIWRN(0, "cmd %u copy_to_user failed: %d\n", cmd, ret);
		break;
	}

	case MTK_IOC_SMI_DUMP_COMMON:
	case MTK_IOC_SMI_DUMP_LARB:
		break;
#ifdef MMDVFS_HOOK
	case MTK_IOC_MMDVFS_CMD:
	{
		struct MTK_MMDVFS_CMD config;

		if (SMI_PARAM_DISABLE_MMDVFS)
			return -EACCES;

		ret = copy_from_user(&config, (void *)arg,
			sizeof(struct MTK_MMDVFS_CMD));
		if (ret)
			SMIWRN(0, "cmd %u copy_from_user fail: %d\n", cmd, ret);
		else {
			mmdvfs_handle_cmd(&config);
			ret = copy_to_user((void *)arg, (void *)&config,
				sizeof(struct MTK_MMDVFS_CMD));
			if (ret)
				SMIWRN(0, "cmd %u copy_to_user failed: %d\n",
					cmd, ret);
		}
		break;
	}
	case MTK_IOC_MMDVFS_QOS_CMD:
	{
		struct MTK_MMDVFS_QOS_CMD config;

		if (SMI_PARAM_DISABLE_MMDVFS)
			return -EACCES;

		ret = copy_from_user(&config, (void *)arg,
			sizeof(struct MTK_MMDVFS_QOS_CMD));
		if (ret)
			SMIWRN(0, "cmd %u copy_from_user fail: %d\n", cmd, ret);
		else {
			switch (config.type) {
			case MTK_MMDVFS_QOS_CMD_TYPE_SET:
#if !defined(CONFIG_MACH_MT6757)
				mmdvfs_set_max_camera_hrt_bw(config.max_cam_bw);
				config.ret = 0;
#else
				SMIWRN(0, "Not Support mmdvfs QOS cmd\n");
				return -EINVAL;
#endif
				break;
			default:
				SMIWRN(0, "invalid mmdvfs QOS cmd\n");
				return -EINVAL;
			}
		}
		break;
	}
#endif
	default:
		ret = -ENOIOCTLCMD;
		break;
	}
	return ret;
}

#if IS_ENABLED(CONFIG_COMPAT)
struct MTK_SMI_COMPAT_BWC_CONFIG {
	compat_int_t scenario;
	compat_int_t b_on_off;
};

struct MTK_SMI_COMPAT_BWC_INFO_SET {
	compat_int_t property;
	compat_int_t value1;
	compat_int_t value2;
};

struct MTK_SMI_COMPAT_BWC_MM_INFO {
	compat_uint_t flag; /* reserved */
	compat_int_t concurrent_profile;
	compat_int_t sensor_size[2];
	compat_int_t video_record_size[2];
	compat_int_t display_size[2];
	compat_int_t tv_out_size[2];
	compat_int_t fps;
	compat_int_t video_encode_codec;
	compat_int_t video_decode_codec;
	compat_int_t hw_ovl_limit;
};

#define COMPAT_MTK_IOC_SMI_BWC_CONFIG \
	MTK_IOW(24, struct MTK_SMI_COMPAT_BWC_CONFIG)
static int smi_bwc_config_compat_get(
	struct MTK_SMI_BWC_CONFIG __user *data,
	struct MTK_SMI_COMPAT_BWC_CONFIG __user *data32)
{
	compat_int_t i;
	int ret;

	ret = get_user(i, &(data32->scenario));
	ret |= put_user(i, &(data->scenario));
	ret |= get_user(i, &(data32->b_on_off));
	ret |= put_user(i, &(data->b_on_off));
	return ret;
}

#define COMPAT_MTK_IOC_SMI_BWC_INFO_SET \
	MTK_IOWR(28, struct MTK_SMI_COMPAT_BWC_INFO_SET)
static int smi_bwc_info_compat_set(
	struct MTK_SMI_BWC_INFO_SET __user *data,
	struct MTK_SMI_COMPAT_BWC_INFO_SET __user *data32)
{
	compat_int_t i;
	int ret;

	ret = get_user(i, &(data32->property));
	ret |= put_user(i, &(data->property));
	ret |= get_user(i, &(data32->value1));
	ret |= put_user(i, &(data->value1));
	ret |= get_user(i, &(data32->value2));
	ret |= put_user(i, &(data->value2));
	return ret;
}

#define COMPAT_MTK_IOC_SMI_BWC_INFO_GET \
	MTK_IOWR(29, struct MTK_SMI_COMPAT_BWC_MM_INFO)
static int smi_bwc_info_compat_get(
	struct MTK_SMI_BWC_MM_INFO __user *data,
	struct MTK_SMI_COMPAT_BWC_MM_INFO __user *data32)
{
	compat_uint_t u;
	compat_int_t p[2];
	compat_int_t i;
	int ret;

	ret = get_user(u, &(data32->flag));
	ret |= put_user(u, &(data->flag));

	ret |= copy_from_user(p, &(data32->sensor_size), sizeof(p));
	ret |= copy_to_user(&(data->sensor_size), p, sizeof(p));
	ret |= copy_from_user(p, &(data32->video_record_size), sizeof(p));
	ret |= copy_to_user(&(data->video_record_size), p, sizeof(p));
	ret |= copy_from_user(p, &(data32->display_size), sizeof(p));
	ret |= copy_to_user(&(data->display_size), p, sizeof(p));
	ret |= copy_from_user(p, &(data32->tv_out_size), sizeof(p));
	ret |= copy_to_user(&(data->tv_out_size), p, sizeof(p));

	ret |= get_user(i, &(data32->concurrent_profile));
	ret |= put_user(i, &(data->concurrent_profile));
	ret |= get_user(i, &(data32->fps));
	ret |= put_user(i, &(data->fps));
	ret |= get_user(i, &(data32->video_encode_codec));
	ret |= put_user(i, &(data->video_encode_codec));
	ret |= get_user(i, &(data32->video_decode_codec));
	ret |= put_user(i, &(data->video_decode_codec));
	ret |= get_user(i, &(data32->hw_ovl_limit));
	ret |= put_user(i, &(data->hw_ovl_limit));
	return ret;
}

static int smi_bwc_info_compat_put(
	struct MTK_SMI_BWC_MM_INFO __user *data,
	struct MTK_SMI_COMPAT_BWC_MM_INFO __user *data32)
{
	compat_uint_t u;
	compat_int_t p[2];
	compat_int_t i;
	int ret;

	ret = get_user(u, &(data->flag));
	ret |= put_user(u, &(data32->flag));

	ret |= copy_from_user(p, &(data->sensor_size), sizeof(p));
	ret |= copy_to_user(&(data32->sensor_size), p, sizeof(p));
	ret |= copy_from_user(p, &(data->video_record_size), sizeof(p));
	ret |= copy_to_user(&(data32->video_record_size), p, sizeof(p));
	ret |= copy_from_user(p, &(data->display_size), sizeof(p));
	ret |= copy_to_user(&(data32->display_size), p, sizeof(p));
	ret |= copy_from_user(p, &(data->tv_out_size), sizeof(p));
	ret |= copy_to_user(&(data32->tv_out_size), p, sizeof(p));

	ret |= get_user(i, &(data->concurrent_profile));
	ret |= put_user(i, &(data32->concurrent_profile));
	ret |= get_user(i, &(data->fps));
	ret |= put_user(i, &(data32->fps));
	ret |= get_user(i, &(data->video_encode_codec));
	ret |= put_user(i, &(data32->video_encode_codec));
	ret |= get_user(i, &(data->video_decode_codec));
	ret |= put_user(i, &(data32->video_decode_codec));
	ret |= get_user(i, &(data->hw_ovl_limit));
	ret |= put_user(i, &(data32->hw_ovl_limit));
	return ret;
}

static long smi_compat_ioctl(struct file *file, unsigned int cmd,
	unsigned long arg)
{
	int ret = 0;

	if (!file->f_op || !file->f_op->unlocked_ioctl)
		return -ENOTTY;

	switch (cmd) {
	case COMPAT_MTK_IOC_SMI_BWC_CONFIG:
	{
		struct MTK_SMI_BWC_CONFIG __user *data;
		struct MTK_SMI_COMPAT_BWC_CONFIG __user *data32;

		data32 = compat_ptr(arg);

		if (cmd == MTK_IOC_SMI_BWC_CONFIG)
			return file->f_op->unlocked_ioctl(file, cmd,
				(unsigned long)data32);

		data = compat_alloc_user_space(
			sizeof(struct MTK_SMI_BWC_CONFIG));
		if (!data)
			return -EFAULT;

		ret = smi_bwc_config_compat_get(data, data32);
		if (ret)
			return ret;

		return file->f_op->unlocked_ioctl(file,
			MTK_IOC_SMI_BWC_CONFIG, (unsigned long)data);
	}

	case COMPAT_MTK_IOC_SMI_BWC_INFO_SET:
	{
		struct MTK_SMI_BWC_INFO_SET __user *data;
		struct MTK_SMI_COMPAT_BWC_INFO_SET __user *data32;

		data32 = compat_ptr(arg);

		if (cmd == MTK_IOC_SMI_BWC_INFO_SET)
			return file->f_op->unlocked_ioctl(file, cmd,
				(unsigned long)data32);

		data = compat_alloc_user_space(
			sizeof(struct MTK_SMI_BWC_INFO_SET));
		if (!data)
			return -EFAULT;

		ret = smi_bwc_info_compat_set(data, data32);
		if (ret)
			return ret;

		return file->f_op->unlocked_ioctl(file,
			MTK_IOC_SMI_BWC_INFO_SET, (unsigned long)data);
	}

	case COMPAT_MTK_IOC_SMI_BWC_INFO_GET:
	{
		struct MTK_SMI_BWC_MM_INFO __user *data;
		struct MTK_SMI_COMPAT_BWC_MM_INFO __user *data32;

		data32 = compat_ptr(arg);

		if (cmd == MTK_IOC_SMI_BWC_INFO_GET)
			return file->f_op->unlocked_ioctl(file, cmd,
				(unsigned long)data32);

		data = compat_alloc_user_space(
			sizeof(struct MTK_SMI_BWC_MM_INFO));
		if (!data)
			return -EFAULT;

		ret = smi_bwc_info_compat_get(data, data32);
		if (ret)
			return ret;

		ret = file->f_op->unlocked_ioctl(file,
			MTK_IOC_SMI_BWC_INFO_GET, (unsigned long)data);

		return smi_bwc_info_compat_put(data, data32);
	}

	case MTK_IOC_SMI_DUMP_COMMON:
	case MTK_IOC_SMI_DUMP_LARB:
	case MTK_IOC_MMDVFS_CMD:
	case MTK_IOC_MMDVFS_QOS_CMD:
		return file->f_op->unlocked_ioctl(file, cmd, (unsigned long)
			compat_ptr(arg));
	default:
		return -ENOIOCTLCMD;
	}
}
#else /* #if !IS_ENABLED(CONFIG_COMPAT) */
#define smi_compat_ioctl NULL
#endif

static const struct file_operations smi_file_opers = {
	.owner = THIS_MODULE,
	.open = smi_open,
	.release = smi_release,
	.unlocked_ioctl = smi_ioctl,
	.compat_ioctl = smi_compat_ioctl,
};

#if IS_ENABLED(CONFIG_MACH_MT6779)
static phys_addr_t smi_dram_size;
static void __iomem *smi_dram_virt;
static struct dentry *smi_dram_node;

static int smi_dram_open(struct inode *inode, struct file *file)
{
	return nonseekable_open(inode, file);
}

static ssize_t
smi_dram_read(struct file *filp, char __user *buf, size_t len, loff_t *ppos)
{
	if (!smi_dram_virt)
		return 0;

	return simple_read_from_buffer(buf, len, ppos,
		smi_dram_virt, smi_dram_size);
}

static const struct file_operations smi_dram_file_opers = {
	.owner = THIS_MODULE,
	.open = smi_dram_open,
	.read = smi_dram_read,
};

int smi_dram_init(void)
{
	s32 ret = 0;
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SSPM_SUPPORT)
	phys_addr_t phys;
	struct qos_ipi_data qos_ipi_d;

	phys = sspm_reserve_mem_get_phys(SMI_MEM_ID);
	smi_dram_size = sspm_reserve_mem_get_size(SMI_MEM_ID);
	smi_dram_virt = ioremap_wc(phys, smi_dram_size);
	if (IS_ERR(smi_dram_virt)) {
		ret = PTR_ERR(smi_dram_virt);
		SMIERR("ioremap_wc phys=%#llx failed: %d\n", phys, ret);
		return ret;
	}

	qos_ipi_d.cmd = QOS_IPI_SMI_MET_MON;
	qos_ipi_d.u.smi_met_mon.ena = ~0;
	qos_ipi_d.u.smi_met_mon.enc[0] = phys;
	qos_ipi_d.u.smi_met_mon.enc[1] = smi_dram_size;
	qos_ipi_to_sspm_command(&qos_ipi_d, 4);
#endif
	smi_dram_node = debugfs_create_file(
		"smi_mon", 0444, NULL, (void *)0, &smi_dram_file_opers);
	if (IS_ERR(smi_dram_node)) {
		ret = PTR_ERR(smi_dram_node);
		SMIERR("debugfs_create_file failed: %d\n", ret);
		return ret;
	}
	return ret;
}
#endif

static int smi_mmsys_offset_get(void)
{
	struct device_node *of_node;

	smi_mmsys = kzalloc(sizeof(*smi_mmsys), GFP_KERNEL);
	if (!smi_mmsys)
		return -ENOMEM;

	of_node = of_parse_phandle(common->dev->of_node, "mmsys_config", 0);
	smi_mmsys->base = (void *)of_iomap(of_node, 0);
	of_node_put(of_node);
	if (!smi_mmsys->base) {
		SMIERR("Unable to parse or iomap mmsys_config\n");
		return -ENOMEM;
	}

	smi_mmsys->nr_debugs = SMI_MMSYS_DEBUG_NUM;
	smi_mmsys->debugs = smi_mmsys_debug_offset;

	SMIDBG("MMSYS nr_debugs=%d\n", smi_mmsys->nr_debugs);
	return 0;
}

static int smi_debug_offset_get(struct mtk_smi_dev *smi)
{
	if (!smi) {
		SMIDBG("No such device or address\n");
		return -ENXIO;
	} else if (!smi->dev) {
		SMIDBG("%s%u no such device or address\n",
			smi->index == SMI_LARB_NUM ? "COMMON" : "LARB",
			smi->index);
		return -ENXIO;
	}

	smi->nr_debugs = (smi->index == SMI_LARB_NUM) ?
		SMI_COMM_DEBUG_NUM : SMI_LARB_DEBUG_NUM;
	smi->debugs = (smi->index == SMI_LARB_NUM) ?
		smi_comm_debug_offset : smi_larb_debug_offset;

	SMIDBG("%s%u nr_debugs=%d\n", smi->index == SMI_LARB_NUM ?
		"COMMON" : "LARB", smi->index, smi->nr_debugs);
	return 0;
}

static int smi_scen_config_get(struct mtk_smi_dev *smi)
{
	if (!smi) {
		SMIDBG("No such device or address\n");
		return -ENXIO;
	} else if (!smi->dev) {
		SMIDBG("%s%u no such device or address\n",
			smi->index == SMI_LARB_NUM ? "COMMON" : "LARB",
			smi->index);
		return -ENXIO;
	}

	smi->nr_scens = SMI_SCEN_NUM;
	smi->nr_scen_pairs = smi_scen_pair_num[smi->index];
	smi->scen_pairs = smi_scen_pair[smi->index];

	SMIDBG("%s%u nr_scens=%d, nr_scen_pairs=%d\n",
		smi->index == SMI_LARB_NUM ? "COMMON" : "LARB", smi->index,
		smi->nr_scens, smi->nr_scen_pairs);
	return 0;
}

static int smi_basic_config_get(struct mtk_smi_dev *smi)
{
	if (!smi) {
		SMIDBG("No such device or address\n");
		return -ENXIO;
	} else if (!smi->dev) {
		SMIDBG("%s%u no such device or address\n",
			smi->index == SMI_LARB_NUM ? "COMMON" : "LARB",
			smi->index);
		return -ENXIO;
	}

	smi->nr_config_pairs = smi_config_pair_num[smi->index];
	smi->config_pairs = smi_config_pair[smi->index];

	SMIDBG("%s%u nr_config_pairs=%d\n", smi->index == SMI_LARB_NUM ?
		"COMMON" : "LARB", smi->index, smi->nr_config_pairs);
	return 0;
}

static void smi_mmp_event_init(void)
{
#if IS_ENABLED(CONFIG_MMPROFILE)
	mmprofile_enable(1);
	if (!smi_mmp_event.smi) {
		smi_mmp_event.smi =
			mmprofile_register_event(MMP_ROOT_EVENT, DEV_NAME);
		smi_mmp_event.mtcmos_dis =
			mmprofile_register_event(smi_mmp_event.smi, "DIS");
		smi_mmp_event.mtcmos_vde =
			mmprofile_register_event(smi_mmp_event.smi, "VDE");
		smi_mmp_event.mtcmos_ven =
			mmprofile_register_event(smi_mmp_event.smi, "VEN");
		smi_mmp_event.mtcmos_isp =
			mmprofile_register_event(smi_mmp_event.smi, "ISP");
		smi_mmp_event.mtcmos_ipe =
			mmprofile_register_event(smi_mmp_event.smi, "IPE");
		smi_mmp_event.mtcmos_cam =
			mmprofile_register_event(smi_mmp_event.smi, "CAM");
		mmprofile_enable_event_recursive(smi_mmp_event.smi, 1);
	}
	mmprofile_start(1);
#endif
}

int smi_register(struct platform_driver *drv)
{
	int i, ret;
	/* smi driver */
	smi_drv = kzalloc(sizeof(*smi_drv), GFP_KERNEL);
	if (!smi_drv)
		return -ENOMEM;

	spin_lock_init(&(smi_drv->lock));
	smi_drv->scen = SMI_BWC_SCEN_NORMAL;
	smi_drv->table[smi_drv->scen] += 1;

#if IS_ENABLED(CONFIG_MACH_MT6758)
	smi_mm_clk_first = smi_clk_subsys_larbs(SYS_MM0) | (1 << SMI_LARB_NUM);
#else
	smi_mm_clk_first = smi_clk_subsys_larbs(SYS_DIS) | (1 << SMI_LARB_NUM);
#endif
	ret = smi_mmsys_offset_get();
	if (ret)
		return ret;
	/* COMMON and LARBs */
	for (i = 0; i <= SMI_LARB_NUM; i++) {
		struct mtk_smi_dev *smi =
			(i == SMI_LARB_NUM) ? common : larbs[i];

		if (!smi) {
			SMIDBG("%s%u no such device or address\n",
				i == SMI_LARB_NUM ? "COMMON" : "LARB", i);
			return -ENXIO;
		}
		/* GET */
		ret = smi_basic_config_get(smi);
		if (ret)
			return ret;
		ret = smi_scen_config_get(smi);
		if (ret)
			return ret;
		ret = smi_debug_offset_get(smi);
		if (ret)
			return ret;
		/* SET */
		if (smi_mm_clk_first & (1 << i)) {
			smi_bus_prepare_enable(i, "SMI_MM", true);
			mtk_smi_config_set(smi, SMI_SCEN_NUM);
			mtk_smi_config_set(smi, smi_scen_map[smi_drv->scen]);
			smi_larb_non_sec_con_set(i);
		}
	}
	smi_debug_dump_status(-1);

	/* smi device */
	smi_dev = kzalloc(sizeof(*smi_dev), GFP_KERNEL);
	if (!smi_dev)
		return -ENOMEM;

	smi_dev->name = DEV_NAME;
	smi_dev->dev_no = MKDEV(MTK_SMI_MAJOR_NUMBER, 0);

	ret = alloc_chrdev_region(&(smi_dev->dev_no), 0, 1, smi_dev->name);
	if (ret) {
		SMIERR("alloc_chrdev_region failed: %d\n", ret);
		return ret;
	}

	smi_dev->cdev = cdev_alloc();
	if (!smi_dev->cdev) {
		SMIERR("cdev_alloc failed\n");
		unregister_chrdev_region(smi_dev->dev_no, 1);
		return -ENOMEM;
	}

	cdev_init(smi_dev->cdev, &smi_file_opers);
	smi_dev->cdev->owner = THIS_MODULE;
	smi_dev->cdev->dev = smi_dev->dev_no;
	ret = cdev_add(smi_dev->cdev, smi_dev->dev_no, 1);
	if (ret) {
		SMIERR("cdev_add failed: %d\n", ret);
		unregister_chrdev_region(smi_dev->dev_no, 1);
		return ret;
	}

	smi_dev->class = class_create(THIS_MODULE, smi_dev->name);
	if (IS_ERR(smi_dev->class)) {
		ret = PTR_ERR(smi_dev->class);
		SMIERR("class_create failed: %d\n", ret);
		return ret;
	}

	smi_dev->device = device_create(smi_dev->class, NULL, smi_dev->dev_no,
		NULL, smi_dev->name);
	if (IS_ERR(smi_dev->device)) {
		ret = PTR_ERR(smi_dev->device);
		SMIERR("device_create failed: %d\n", ret);
		return ret;
	}

	register_pg_callback(&smi_clk_subsys_handle);
#ifdef MMDVFS_HOOK
	mmdvfs_init(&g_smi_bwc_mm_info);
#endif
	return ret;
}
EXPORT_SYMBOL_GPL(smi_register);

int smi_unregister(struct platform_driver *drv)
{
	device_destroy(smi_dev->class, smi_dev->dev_no);
	class_destroy(smi_dev->class);

	cdev_del(smi_dev->cdev);
	unregister_chrdev_region(smi_dev->dev_no, 1);

	kfree(smi_dev);
	kfree(smi_drv);
	return 0;
}
EXPORT_SYMBOL_GPL(smi_unregister);

static int __init smi_late_init(void)
{
	int i;

#if IS_ENABLED(CONFIG_MACH_MT6779)
	qos_sram_write(MM_SMI_CLK, smi_mm_clk_first);
	qos_sram_write(MM_SMI_CLR, 0x0);
#if IS_ENABLED(CONFIG_MTK_ENG_BUILD)
	smi_dram_dump = 0x1;
	qos_sram_write(MM_SMI_DUMP, smi_dram_dump);
#endif
	smi_dram_init();
#endif
	for (i = 0; i <= SMI_LARB_NUM; i++)
		if (smi_mm_clk_first & (1 << i))
			smi_bus_disable_unprepare(i, "SMI_MM", true);
	smi_mm_clk_first = 0;
	smi_mmp_event_init();
	return 0;
}
late_initcall(smi_late_init);

#if IS_ENABLED(CONFIG_MACH_MT6779)
int set_smi_dram_dump(const char *val, const struct kernel_param *kp)
{
	s32 arg, ret;

	ret = kstrtoint(val, 0, &arg);
	if (ret) {
		SMIDBG("Invalid val: %s, ret=%d\n", val, ret);
		return ret;
	} else if (smi_dram_dump || !arg) {
		SMIDBG("arg=%d, smi_dram_dump=%d\n", arg, smi_dram_dump);
		return 0;
	}
	ret = smi_dram_dump;
	smi_dram_dump = arg;
	qos_sram_write(MM_SMI_DUMP, smi_dram_dump);
	SMIDBG("smi_dram_dump: prev=%d, curr=%d\n", ret, smi_dram_dump);
	return 0;
}

int get_smi_dram_dump(char *buf, const struct kernel_param *kp)
{
	s32 pos = 0;

	pos += snprintf(buf + pos, PAGE_SIZE - pos,
		"smi_dram_dump=%d\n", smi_dram_dump);
	SMIDBG("smi_dram_dump=%d", smi_dram_dump);
	return pos;
}

static struct kernel_param_ops smi_dram_dump_ops = {
	.set = set_smi_dram_dump,
	.get = get_smi_dram_dump,
};

module_param_cb(smi_dram_dump, &smi_dram_dump_ops, NULL, 0644);
#endif
module_param(smi_bwc_config_disable, uint, 0644);

#ifdef MMDVFS_HOOK
static unsigned int mmdvfs_scen_log_mask = 1 << MMDVFS_SCEN_COUNT;
int mmdvfs_scen_log_mask_get(void)
{
	return mmdvfs_scen_log_mask;
}

static unsigned int mmdvfs_debug_level;
int mmdvfs_debug_level_get(void)
{
	return mmdvfs_debug_level;
}

#ifdef SMI_PARAM_DISABLE_MMDVFS
static unsigned int disable_mmdvfs = SMI_PARAM_DISABLE_MMDVFS;
#else
static unsigned int disable_mmdvfs;
#endif
int is_mmdvfs_disabled(void)
{
	return disable_mmdvfs;
}

#ifdef SMI_PARAM_DISABLE_FREQ_MUX
static unsigned int disable_freq_mux = SMI_PARAM_DISABLE_FREQ_MUX;
#else
static unsigned int disable_freq_mux = 1;
#endif
int is_mmdvfs_freq_mux_disabled(void)
{
	return disable_freq_mux;
}

#ifdef SMI_PARAM_DISABLE_FREQ_HOPPING
static unsigned int disable_freq_hopping = SMI_PARAM_DISABLE_FREQ_HOPPING;
#else
static unsigned int disable_freq_hopping = 1;
#endif
int is_mmdvfs_freq_hopping_disabled(void)
{
	return disable_freq_hopping;
}

#ifdef SMI_PARAM_DISABLE_FORCE_MMSYS_MAX_CLK
static unsigned int force_max_mmsys_clk =
	!(SMI_PARAM_DISABLE_FORCE_MMSYS_MAX_CLK);
#else
static unsigned int force_max_mmsys_clk;
#endif
int is_force_max_mmsys_clk(void)
{
	return force_max_mmsys_clk;
}

#ifdef SMI_PARAM_FORCE_MMSYS_CLKS_ALWAYS_ON
static unsigned int force_always_on_mm_clks_mask =
	SMI_PARAM_FORCE_MMSYS_CLKS_ALWAYS_ON;
#else
static unsigned int force_always_on_mm_clks_mask = 1;
#endif
int force_always_on_mm_clks(void)
{
	return force_always_on_mm_clks_mask;
}

static unsigned int clk_mux_mask = 0xFFFF;
int get_mmdvfs_clk_mux_mask(void)
{
	return clk_mux_mask;
}

module_param(mmdvfs_scen_log_mask, uint, 0644);
module_param(mmdvfs_debug_level, uint, 0644);
module_param(disable_mmdvfs, uint, 0644);
module_param(disable_freq_mux, uint, 0644);
module_param(disable_freq_hopping, uint, 0644);
module_param(force_max_mmsys_clk, uint, 0644);
module_param(clk_mux_mask, uint, 0644);

#ifdef MMDVFS_QOS_SUPPORT
static struct kernel_param_ops qos_scenario_ops = {
	.set = set_qos_scenario,
	.get = get_qos_scenario,
};
module_param_cb(qos_scenario, &qos_scenario_ops, NULL, 0644);
#endif

#endif /* MMDVFS_HOOK */
