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
#include <linux/of_address.h>

#include <aee.h>
#include <mtk_smi.h>
#include <soc/mediatek/smi.h>
#include "smi_public.h"
#include "mmdvfs_mgr.h"

#if IS_ENABLED(CONFIG_COMPAT)
#include <linux/compat.h>
#endif

#if IS_ENABLED(CONFIG_MACH_MT6758)
#include <clk-mt6758-pg.h>
#elif IS_ENABLED(CONFIG_MACH_MT6765)
#include <clk-mt6765-pg.h>
#include "smi_config_mt6765.h"
#else
#include "smi_config_default.h"
#endif

#if IS_ENABLED(CONFIG_MTK_M4U)
#include <m4u.h>
#endif

#define SMI_TAG "SMI"
#undef pr_fmt
#define pr_fmt(fmt) "[" SMI_TAG "]%s: " fmt, __func__

#define SMIDBG(string, args...) pr_debug(string, ##args)

#if IS_ENABLED(CONFIG_MTK_CMDQ)
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

static unsigned int smi_bwc_config_disable;

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

static struct smi_device *smi_dev;
static struct smi_driver *smi_drv;
static struct mmsys_config *smi_mmsys;

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

/* ********************************************************
 * prepare and enable CG/MTCMOS of COMMON and specific LARB
 * reg_indx: select specific LARB or COMMON
 * user_name: caller's module name, used for debug
 * mtcmos: wish to manipulate power with mtcmos = 1
 * *******************************************************/
int smi_bus_prepare_enable(const unsigned int reg_indx,
	const char *user_name, const bool mtcmos)
{
	int ref_cnt = -1, ret = 0;

	if (reg_indx > SMI_LARB_NUM) {
		SMIDBG("Invalid reg_indx=%u, SMI_LARB_NUM=%u\n",
			reg_indx, SMI_LARB_NUM);
		return -EINVAL;
	}
	/* COMMON */
	if (mtcmos)
		ret = clk_prepare_enable(common->clks[0]);
	if (ret) {
		SMIERR("COMMON MTCMOS enable failed=%d\n", ret);
		return ret;
	}
	ret = mtk_smi_dev_enable(common);
	if (ret)
		return ret;
	ref_cnt = mtk_smi_clk_ref_cnts_read(common);
	if (ref_cnt <= 0)
		SMIWRN(0, "(user_name=%s, mtcmos=%d): ref_cnt=%d\n",
			user_name, mtcmos, ref_cnt);
	/* LARB */
	if (reg_indx < SMI_LARB_NUM) {
		struct mtk_smi_dev *larb = larbs[reg_indx];

		if (mtcmos)
			ret = clk_prepare_enable(larb->clks[0]);
		if (ret) {
			SMIERR("LARB%u MTCMOS enable failed: %d\n",
				reg_indx, ret);
			return ret;
		}
		ret = mtk_smi_dev_enable(larb);
		if (ret)
			return ret;
		ref_cnt = mtk_smi_clk_ref_cnts_read(larb);
		if (ref_cnt <= 0)
			SMIWRN(0,
				"(user_name=%s, mtcmos=%d): LARB%u ref_cnt=%d\n",
				user_name, mtcmos, reg_indx, ref_cnt);
	}
	/* DBG
	 * SMIDBG("(user_name=%s, mtcmos=%d): LARB%u ref_cnt=%d\n",
	 * user_name, mtcmos, reg_indx, ref_cnt);
	 */
	return ret;
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
	int ref_cnt = -1, ret = 0;

	if (reg_indx > SMI_LARB_NUM) {
		SMIDBG("Invalid reg_indx=%u, SMI_LARB_NUM=%u\n",
			reg_indx, SMI_LARB_NUM);
		return -EINVAL;
	}
	/* LARB */
	if (reg_indx < SMI_LARB_NUM) {
		struct mtk_smi_dev *larb = larbs[reg_indx];

		ret = mtk_smi_dev_disable(larb);
		if (ret)
			return ret;
		ref_cnt = mtk_smi_clk_ref_cnts_read(larb);
		if (ref_cnt < 0)
			SMIWRN(0,
				"(user_name=%s, mtcmos=%d): LARB%u ref_cnt=%d\n",
				user_name, mtcmos, reg_indx, ref_cnt);
		if (mtcmos)
			clk_disable_unprepare(larb->clks[0]);
	}
	/* COMMON */
	ret = mtk_smi_dev_disable(common);
	if (ret)
		return ret;
	ref_cnt = mtk_smi_clk_ref_cnts_read(common);
	if (ref_cnt < 0)
		SMIWRN(0, "(user_name=%s, mtcmos=%d): ref_cnt=%d\n",
			user_name, mtcmos, ref_cnt);
	if (mtcmos)
		clk_disable_unprepare(common->clks[0]);
	/* DBG
	 * SMIDBG("(reg_indx=%u, user_name=%s, mtcmos=%d): COMM ref_cnt=%d\n",
	 * reg_indx, user_name, mtcmos, ref_cnt);
	 */
	return ret;
}
EXPORT_SYMBOL_GPL(smi_bus_disable_unprepare);

static int smi_larb_cmd_grp_enable(void)
{
	int i, j;

	for (i = 0; i < SMI_LARB_CMD_GP_EN_LARB_NUM; i++) {
		struct mtk_smi_dev *smi = larbs[i];

		if (!smi) {
			SMIDBG("No such device or address\n");
			return -ENXIO;
		} else if (!smi->base) {
			SMIDBG("LARB%u no such device or address\n", i);
			return -ENXIO;
		}

		for (j = 0; j < SMI_LARB_CMD_GP_EN_PORT_NUM; j++)
			writel(readl(smi->base + SMI_LARB_NON_SEC_CON(j)) | 0x2,
				smi->base + SMI_LARB_NON_SEC_CON(j));
	}
	SMIDBG("CMD_GP_EN: LARB_NUM=%u, PORT_NUM=%u\n",
		SMI_LARB_CMD_GP_EN_LARB_NUM, SMI_LARB_CMD_GP_EN_PORT_NUM);
	return 0;
}

static int smi_larb_bw_thrt_enable(const unsigned int reg_indx)
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
	}

	for (i = smi_larb_bw_thrt_en_port[reg_indx][0];
		i < smi_larb_bw_thrt_en_port[reg_indx][1]; i++)
		writel(readl(smi->base + SMI_LARB_NON_SEC_CON(i)) | 0x8,
			smi->base + SMI_LARB_NON_SEC_CON(i));
	SMIDBG("BW_THRT_EN: reg_indx=%u, PORT=(%u, %u)\n", reg_indx,
		smi_larb_bw_thrt_en_port[reg_indx][0],
		smi_larb_bw_thrt_en_port[reg_indx][1]);
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
#endif
	return 0;
}

static void smi_clk_subsys_after_on(enum subsys_id sys)
{
	unsigned int subsys = smi_clk_subsys_larbs(sys);
	int i, smi_scen = smi_scen_map[smi_drv->scen];
	/* COMMON */
	if (subsys & 1) { /* COMMON and LARB0 in SYS_MM0 or SYS_DIS */
		mtk_smi_config_set(common, SMI_SCEN_NUM, false);
		mtk_smi_config_set(common, smi_scen, false);
	}
	/* LARBs */
	for (i = 0; i < SMI_LARB_NUM; i++)
		if (subsys & (1 << i)) {
			mtk_smi_config_set(larbs[i], SMI_SCEN_NUM, false);
			mtk_smi_config_set(larbs[i], smi_scen, false);
			smi_larb_bw_thrt_enable(i);
			if (!i) /* DISP */
				smi_larb_cmd_grp_enable();
		}
}

static struct pg_callbacks smi_clk_subsys_handle = {
	.after_on = smi_clk_subsys_after_on
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
	unsigned int i, scen;

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
	mtk_smi_config_set(common, smi_scen_map[smi_drv->scen], true);
	for (i = 0; i < SMI_LARB_NUM; i++)
		mtk_smi_config_set(larbs[i], smi_scen_map[smi_drv->scen], true);

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

static ssize_t smi_bwc_scen_show(struct device_driver *driver, char *buf)
{
	ssize_t ret = 0;
	int i, max_len = 1 << 7;

	smi_debug_dump_status(-1);
	ret += snprintf(buf + ret, max_len, "%s(%d, %d)=%d\n",
		smi_bwc_scen_name_get(smi_drv->scen), smi_drv->scen,
		smi_scen_map[smi_drv->scen], smi_drv->table[smi_drv->scen]);

	ret += snprintf(buf + ret, max_len, "table[%d]:", SMI_BWC_SCEN_CNT);
	for (i = 0; i < SMI_BWC_SCEN_CNT; i++)
		ret += snprintf(buf + ret, max_len, " %d,", smi_drv->table[i]);
	ret += snprintf(buf + ret, max_len, "\n");
	return strlen(buf);
}

static ssize_t smi_bwc_scen_store(struct device_driver *driver, const char *buf,
	size_t count)
{
	char *name;
	int smi_scen, i, j;

	for (i = 0; i < SMI_BWC_SCEN_CNT; i++) {
		name = smi_bwc_scen_name_get(i);
		if (!strncmp(buf, name, strlen(name))) {
			smi_drv->scen = (enum MTK_SMI_BWC_SCEN)i;
			smi_scen = smi_scen_map[smi_drv->scen];
			mtk_smi_config_set(common, smi_scen, true);
			for (j = 0; j < SMI_LARB_NUM; j++)
				mtk_smi_config_set(larbs[j], smi_scen, true);
			break;
		}
	}
	if (i < SMI_BWC_SCEN_CNT)
		SMIDBG("%s(%d, %d)\n", smi_bwc_scen_name_get(i), i, smi_scen);
	else
		SMIDBG("unknown operation: %s\n", buf);
	return count;
}
DRIVER_ATTR(smi_bwc_scen, 0644, smi_bwc_scen_show, smi_bwc_scen_store);

static int smi_debug_dumpper(struct mtk_smi_dev *smi,
	struct mmsys_config *mmsys, const bool gce, const bool offset)
{
	void __iomem *base;
	unsigned int nr_debugs, *debugs, val, length, size, max_size = 1 << 7;
	char *name, buffer[max_size + 1];
	int i, j, ref_cnt;

	if (!smi) {
		SMIWRN(gce, "No such device or address\n");
		return -ENXIO;
	} else if (!smi->base || !smi->debugs) {
		SMIWRN(gce, "%s%u no such device or address\n",
			smi->index == SMI_LARB_NUM ? "COMMON" : "LARB",
			smi->index);
		return -ENXIO;
	}

	base = (mmsys ? mmsys->base : smi->base);
	nr_debugs = (mmsys ? mmsys->nr_debugs : smi->nr_debugs);
	debugs = (mmsys ? mmsys->debugs : smi->debugs);
	name = (mmsys ? "MMSYS" :
		(smi->index == SMI_LARB_NUM ? "COMMON" : "LARB"));
	ref_cnt = mtk_smi_clk_ref_cnts_read(smi);

	/* offset or value */
	if (offset)
		SMIWRN(gce, "========== %s offset ==========\n", name);
	else if (ref_cnt || !smi->index || smi->index == SMI_LARB_NUM
#if IS_ENABLED(CONFIG_MACH_MT6758)
		|| smi->index == 1
#endif /* SYS_MM0 or SYS_DIS */
		)
		SMIWRN(gce,
			"========== %s%u non-zero value, clock=%d ==========\n",
			name, smi->index, ref_cnt);
	else { /* !ref_cnt */
		SMIWRN(gce, "========== %s%u without clock=%d ==========\n",
			name, smi->index, ref_cnt);
		return 0;
	}

	/* print */
	for (i = 0; i < nr_debugs; i += j) {
		length = 0;
		for (j = 0; i + j < nr_debugs; j++) {
			val = readl(base + debugs[i + j]);
			if (offset)
				size = snprintf(buffer + length,
					max_size - length,
					" %#x,", debugs[i + j]);
			else if (val)
				size = snprintf(buffer + length,
					max_size - length,
					" %#x=%#x,", debugs[i + j], val);
			else
				continue;
			if (size < 0 || (max_size <= length + size)) {
				snprintf(buffer + length, max_size - length,
					"%c", '\0');
				break;
			}
			length = length + size;
		}
		SMIWRN(gce, "%s\n", buffer);
	}
	return 0;
}

/* *******************************************************
 * dump smi debug status for COMMON and/or LARB(s)
 * reg_indx = [0, SMI_LARB_NUM]: specific LARB or COMMON
 * otherwise: COMMON and LARBs
 * ******************************************************/
void smi_debug_dump_status(const unsigned int reg_indx)
{
	bool gce = false, offset = false;
	struct mtk_smi_dev *smi;

	if (reg_indx <= SMI_LARB_NUM)
		smi_debug_dumpper(
			(reg_indx == SMI_LARB_NUM) ? common : larbs[reg_indx],
			NULL, gce, offset);
	else {
		int i;

		for (i = 0; i <= SMI_LARB_NUM; i++) {
			smi = (i == SMI_LARB_NUM) ? common : larbs[i];
			if (!smi) {
				SMIWRN(gce, "%s%u no such device or address\n",
					i == SMI_LARB_NUM ? "COMMON" : "LARB",
					i);
				continue;
			}
			smi_debug_dumpper(smi, NULL, gce, offset);
		}
	}
	smi_debug_dumpper(common, smi_mmsys, gce, offset);
	SMIWRN(gce, "%s(%d, %d)=%d\n",
		smi_bwc_scen_name_get(smi_drv->scen), smi_drv->scen,
		smi_scen_map[smi_drv->scen], smi_drv->table[smi_drv->scen]);
}
EXPORT_SYMBOL_GPL(smi_debug_dump_status);

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
	unsigned int dump_time = 5, val;
	int i, j, ret = 0, ref_cnt;
	bool offset = false;

	if (!common) {
		SMIDBG("COMMON no such device or address\n");
		return -ENXIO;
	} else if (!larbs || !larbs[0]) {
		SMIDBG("LARBs no such device or address\n");
		return -ENXIO;
	}
	if (dump) {
		smi_debug_dumpper(common, smi_mmsys, gce, !offset);
		smi_debug_dumpper(common, NULL, gce, !offset);
		smi_debug_dumpper(larbs[0], NULL, gce, !offset);
	}

	for (i = 0; i < dump_time; i++) {
		if (dump)
			smi_debug_dumpper(common, smi_mmsys, gce, offset);
		/* COMMON */
		if (reg_indx & 1) { /* COMMON and LARB0 in SYS_MM0 or SYS_DIS */
			if (!(readl(common->base + 0x440) & 0x1))
				common->busy_cnts += 1;
			if (dump)
				smi_debug_dumpper(common, NULL, gce, offset);
		}
		/* LARBs */
		for (j = 0; j < SMI_LARB_NUM; j++) {
			if (!(reg_indx & (1 << j)))
				continue;
			ref_cnt = mtk_smi_clk_ref_cnts_read(larbs[j]);
			if (ref_cnt > 0 || !j || j == SMI_LARB_NUM
#if IS_ENABLED(CONFIG_MACH_MT6758)
				|| j == 1
#endif /* SYS_MM0 or SYS_DIS */
				)
				val = readl(larbs[j]->base + 0x0);
			else
				val = 0x0;
			if (val & 0x1)
				larbs[j]->busy_cnts += 1;
			if (dump)
				smi_debug_dumpper(larbs[j], NULL, gce, offset);
		}
	}
	/* busy_cnts */
	for (j = 0; j < SMI_LARB_NUM; j++) {
		SMIWRN(gce,
			"LARB%u=%u/%u busy with clock=%d, COMM=%u/%u busy with clock=%d\n",
			j, larbs[j]->busy_cnts, dump_time,
			mtk_smi_clk_ref_cnts_read(larbs[j]),
			common->busy_cnts, dump_time,
			mtk_smi_clk_ref_cnts_read(common));
		larbs[j]->busy_cnts = 0;
	}
	common->busy_cnts = 0;
	SMIWRN(gce, "%s(%d, %d)=%d\n",
		smi_bwc_scen_name_get(smi_drv->scen), smi_drv->scen,
		smi_scen_map[smi_drv->scen], smi_drv->table[smi_drv->scen]);
#if IS_ENABLED(CONFIG_MTK_M4U)
	if (dump) /* m4u */
		m4u_dump_reg_for_smi_hang_issue();
#endif
	return ret;
}
EXPORT_SYMBOL_GPL(smi_debug_bus_hang_detect);

/* smi ioctl */
static unsigned int smi_ioctl_disable;
module_param(smi_ioctl_disable, uint, 0644);

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

	if (smi_ioctl_disable) {
		SMIDBG("Disable SMI IOCTL, cmd=%u, arg=%lu\n", cmd, arg);
		return -EACCES;
	}
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
	{
		if (!common) {
			SMIDBG("COMMON no such device or address\n");
			ret = -ENXIO;
		} else
			ret = smi_debug_dumpper(common, NULL, false, false);
		break;
	}

	case MTK_IOC_SMI_DUMP_LARB:
	{
		unsigned int index;

		ret = copy_from_user(&index, (void *)arg, sizeof(unsigned int));
		if (ret)
			SMIWRN(0, "cmd %u copy_from_user fail: %d\n", cmd, ret);
		else if (index >= SMI_LARB_NUM || !larbs[index])
			SMIDBG("LARB%u no such device or address\n", index);
		else
			ret = smi_debug_dumpper(
				larbs[index], NULL, false, false);
		break;
	}
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

	if (smi_ioctl_disable) {
		SMIDBG("Disable SMI IOCTL, cmd=%u, arg=%lu\n", cmd, arg);
		return -EACCES;
	}
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
		ret = mtk_smi_config_set(smi, SMI_SCEN_NUM, true);
		if (ret)
			return ret;
		ret = mtk_smi_config_set(
			smi, smi_scen_map[smi_drv->scen], true);
		if (ret)
			return ret;
		if (!i) /* DISP */
			ret = smi_larb_cmd_grp_enable();
		if (ret)
			return ret;
		if (i < SMI_LARB_NUM)
			ret = smi_larb_bw_thrt_enable(i);
		if (ret)
			return ret;
	}
	ret = smi_mmsys_offset_get();
	if (ret)
		return ret;
	smi_debug_dump_status(-1);

	/* smi device */
	smi_dev = kzalloc(sizeof(*smi_dev), GFP_KERNEL);
	if (!smi_dev)
		return -ENOMEM;

	smi_dev->name = "MTK_SMI";
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

	ret = driver_create_file(&drv->driver, &driver_attr_smi_bwc_scen);
	if (ret) {
		SMIDBG("driver_create_file failed: %d\n", ret);
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
	driver_remove_file(&drv->driver, &driver_attr_smi_bwc_scen);

	device_destroy(smi_dev->class, smi_dev->dev_no);
	class_destroy(smi_dev->class);

	cdev_del(smi_dev->cdev);
	unregister_chrdev_region(smi_dev->dev_no, 1);

	kfree(smi_dev);
	kfree(smi_drv);
	return 0;
}
EXPORT_SYMBOL_GPL(smi_unregister);

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
#endif /* MMDVFS_HOOK */
module_param(smi_bwc_config_disable, uint, 0644);
