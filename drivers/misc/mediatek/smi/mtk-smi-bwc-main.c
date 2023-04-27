// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2023 MediaTek Inc.
 */
#include <linux/fs.h>
#include <linux/clk.h>
#include <linux/cdev.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <soc/mediatek/smi.h>
#if IS_ENABLED(CONFIG_COMPAT)
#include <linux/compat.h>
#endif
#ifdef MMDVFS_HOOK
#include <mmdvfs_mgr.h>
#endif

#include "mtk-smi-bwc.h"

#define DEV_NAME "MTK_SMI"
#undef pr_fmt
#define pr_fmt(fmt) "[" DEV_NAME "]" fmt
#define SMIDBG(string, args...) pr_debug(string, ##args)
#define SMIWRN(cmdq, string, args...) pr_info(string, ##args)
#define SMIERR(string, args...) pr_notice(string, ##args)

#define MAX_SMI_SCEN_NUM		(3)
#define MTK_LARB_NR_MAX			(6)
#define SMI_LARB_PORT_NR_MAX	(32)
#define SMI_COMMON_LARB_NR_MAX	(8)
#define SMI_COMMON_MISC_NR		(1)

#define SMI_BUS_SEL				(0x220)

struct mtk_smi_reg_pair {
	u16	offset;
	u32	value;
};

struct smi_bwc_info {
	u32 *scen_map;
	u8 *larb_bwl;
	u16 *comm_bwl;
	struct mtk_smi_reg_pair *comm_misc;
};

// begin bwc config for mt6768
u8 mtk_smi_larb_mt6768_bwl[MAX_SMI_SCEN_NUM][MTK_LARB_NR_MAX][SMI_LARB_PORT_NR_MAX] = {
	{
	{0x1f, 0x1f, 0x1f, 0x7, 0x7, 0x4, 0x4, 0x1f}, /* LARB0 */
	{0x9, 0x9, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x6}, /* LARB1 */
	{0xc, 0x1, 0x4, 0x4, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1}, /* LARB2 */
	{0x16, 0x14, 0x2, 0x2, 0x2, 0x2, 0x4, 0x2, 0x2, 0x2, 0x2, 0x2, 0x4, 0x4, 0x4,
	 0x2, 0x2, 0x2, 0x2, 0x2, 0x2}, /* LARB3 */
	{0x3, 0x1, 0x1, 0x1, 0x1, 0x5, 0x3, 0x1, 0x1, 0x1, 0x6}, /* LARB4 */
	},	// init, vpwfd, vr4k
	{
	{0x1f, 0x1f, 0x1f, 0x7, 0x7, 0x1, 0xb, 0x1f}, /* LARB0 */
	{0x3, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x6}, /* LARB1 */
	{0x6, 0x1, 0x2, 0x3, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1}, /* LARB2 */
	{0x14, 0x6, 0x2, 0x2, 0x2, 0x2, 0x4, 0x2, 0x2, 0x2, 0x2, 0x2, 0x4, 0x4, 0x4,
	 0x2, 0x2, 0x2, 0x2, 0x2, 0x2}, /* LARB3 */
	{0x3, 0x1, 0x1, 0x1, 0x1, 0x5, 0x3, 0x1, 0x1, 0x1, 0x6}, /* LARB4 */
	},	// icfp
};

static u16 mtk_smi_common_mt6768_bwl[MAX_SMI_SCEN_NUM][SMI_COMMON_LARB_NR_MAX] = {
	{0x1ba5, 0x1000, 0x15d3, 0x1000, 0x1000, 0x0, 0x0, 0x0},	// init, vpwfd, vr4k
	{0x1327, 0x119e, 0x1241, 0x12e6, 0x119e, 0x0, 0x0, 0x0},	// icfp
};

static struct mtk_smi_reg_pair
mtk_smi_common_mt6768_misc[MAX_SMI_SCEN_NUM][SMI_COMMON_MISC_NR] = {
	{{SMI_BUS_SEL, 0x104},},	// init, vpwfd, vr4k
	{{SMI_BUS_SEL, 0x144},},	// icfp
};

static u32 mtk_smi_mt6768_scen_map[SMI_BWC_SCEN_CNT] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 1, 1, 1, 0
};

static struct smi_bwc_info bwc_info_mt6768 = {
	.scen_map = (u32 *)mtk_smi_mt6768_scen_map,
	.larb_bwl = (u8 *)mtk_smi_larb_mt6768_bwl,
	.comm_bwl = (u16 *)mtk_smi_common_mt6768_bwl,
	.comm_misc = (struct mtk_smi_reg_pair *)mtk_smi_common_mt6768_misc,
};
// end bwc config for mt6768

// begin bwc config for mt6761
u8 mtk_smi_larb_mt6761_bwl[MAX_SMI_SCEN_NUM][MTK_LARB_NR_MAX][SMI_LARB_PORT_NR_MAX] = {
	{
	{0x1f, 0x1f, 0xe, 0x7, 0x7, 0x4, 0x4, 0x1f}, /* LARB0 */
	{0x3, 0x1, 0x1, 0x1, 0x1, 0x5, 0x3, 0x1, 0x1, 0x1, 0x6}, /* LARB1 */
	{0x16, 0x14, 0x2, 0x2, 0x2, 0x4, 0x4, 0x2, 0x2, 0x4, 0x2, 0x2, 0x4, 0x4, 0x4,
	 0x4, 0x4, 0x2, 0x2, 0x2, 0x2, 0x4, 0x4, 0x4}, /* LARB2 */
	},	// init, vpwfd, icfp, vr4k
};

static u16 mtk_smi_common_mt6761_bwl[MAX_SMI_SCEN_NUM][SMI_COMMON_LARB_NR_MAX] = {
	{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},	// init, vpwfd, icfp, vr4k
};

static u32 mtk_smi_mt6761_scen_map[SMI_BWC_SCEN_CNT] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0
};

static struct smi_bwc_info bwc_info_mt6761 = {
	.scen_map = (u32 *)mtk_smi_mt6761_scen_map,
	.larb_bwl = (u8 *)mtk_smi_larb_mt6761_bwl,
	.comm_bwl = (u16 *)mtk_smi_common_mt6761_bwl,
	.comm_misc = NULL,
};
// end bwc config for mt6761

// begin bwc config for mt6739
u8 mtk_smi_larb_mt6739_bwl[MAX_SMI_SCEN_NUM][MTK_LARB_NR_MAX][SMI_LARB_PORT_NR_MAX] = {
	{
	{0x1f, 0x1f, 0x1f, 0x2, 0x1, 0x1, 0x5}, /* LARB0 */
	{0x3, 0x1, 0x2, 0x1, 0x1, 0x3, 0x2, 0x1, 0x1, 0x1, 0x5}, /* LARB1 */
	{0xc, 0x6, 0x2, 0x2, 0x2, 0x2, 0x2, 0x2, 0x4, 0x3, 0x1}, /* LARB2 */
	},	// init, vpwfd
	{
	{0x1f, 0x1f, 0x1f, 0xe, 0x1, 0x1, 0x7}, /* LARB0 */
	{0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x4}, /* LARB1 */
	{0xa, 0x4, 0x2, 0x2, 0x2, 0x2, 0x2, 0x2, 0x4, 0x2, 0x1}, /* LARB2 */
	},	// icfp, vr4k
	{
	{0x1, 0x1f, 0x1f, 0x2, 0x1, 0x1, 0x5}, /* LARB0 */
	{0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x4}, /* LARB1 */
	{0xa, 0x4, 0x2, 0x2, 0x2, 0x2, 0x2, 0x2, 0x4, 0x2, 0x1}, /* LARB2 */
	},	// wfd
};

static u16 mtk_smi_common_mt6739_bwl[MAX_SMI_SCEN_NUM][SMI_COMMON_LARB_NR_MAX] = {
	{0x12f6, 0x1000, 0x1000, 0x0, 0x0, 0x0, 0x0, 0x0},	// init, vpwfd
	{0x119a, 0x118f, 0x1250, 0x0, 0x0, 0x0, 0x0, 0x0},	// icfp, vr4k
	{0x119a, 0x118f, 0x1250, 0x0, 0x0, 0x0, 0x0, 0x0},	// wfd
};

static u32 mtk_smi_mt6739_scen_map[SMI_BWC_SCEN_CNT] = {
	0, 0, 0, 0, 0, 0, 2, 0, 0, 0,
	0, 1, 1, 1, 1, 1, 1, 1, 0
};

static struct smi_bwc_info bwc_info_mt6739 = {
	.scen_map = (u32 *)mtk_smi_mt6739_scen_map,
	.larb_bwl = (u8 *)mtk_smi_larb_mt6739_bwl,
	.comm_bwl = (u16 *)mtk_smi_common_mt6739_bwl,
	.comm_misc = NULL,
};
// end bwc config for mt6739

// begin bwc config for mt6765
u8 mtk_smi_larb_mt6765_bwl[MAX_SMI_SCEN_NUM][MTK_LARB_NR_MAX][SMI_LARB_PORT_NR_MAX] = {
	{
	{0x1f, 0x1f, 0x1f, 0x7, 0x7, 0x4, 0x4, 0x1f},                     /* LARB0 */
	{0x3, 0x1, 0x1, 0x1, 0x1, 0x5, 0x3, 0x1, 0x1, 0x1, 0x6},          /* LARB1 */
	{0xc, 0x1, 0x4, 0x4, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1},     /* LARB2 */
	{0x16, 0x14, 0x2, 0x2, 0x2, 0x2, 0x4, 0x2, 0x2, 0x2, 0x2, 0x2, 0x4, 0x4, 0x4,
	 0x2, 0x2, 0x2, 0x2, 0x2, 0x2},                                   /* LARB3 */
	},       // init, vpwfd, vr4k smi_larb<X>_init_pair()
	{
	{0x1f, 0x1f, 0x1f, 0x7, 0x7, 0x1, 0xb, 0x1f},                    /* LARB0 */
	{0x3, 0x1, 0x1, 0x1, 0x1, 0x5, 0x3, 0x1, 0x1, 0x1, 0x6},         /* LARB1 */
	{0x6, 0x1, 0x2, 0x3, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1},    /* LARB2 */
	{0x14, 0x6, 0x2, 0x2, 0x2, 0x2, 0x4, 0x2, 0x2, 0x2, 0x2, 0x2, 0x4, 0x4, 0x4,
	 0x2, 0x2, 0x2, 0x2, 0x2, 0x2},                                  /* LARB3 */
	},      // icfp smi_larb<X>_icfp_pair()
};

//From smi_comm_init_pair() & smi_comm_icfp_pair()
static u16 mtk_smi_common_mt6765_bwl[MAX_SMI_SCEN_NUM][SMI_COMMON_LARB_NR_MAX] = {
	{0x1ba5, 0x1000, 0x15d3, 0x1000, 0x0, 0x0, 0x0, 0x0}, // init, vpwfd, vr4k
	{0x1327, 0x119e, 0x1241, 0x12e6, 0x0, 0x0, 0x0, 0x0}, // icfp
};

static struct mtk_smi_reg_pair
mtk_smi_common_mt6765_misc[MAX_SMI_SCEN_NUM][SMI_COMMON_MISC_NR] = {
	{{SMI_BUS_SEL, 0x4},},	// init, vpwfd, vr4k smi_comm_init_pair()
	{{SMI_BUS_SEL, 0x44},},	// icfp smi_comm_icfp_pair()
};

static u32 mtk_smi_mt6765_scen_map[SMI_BWC_SCEN_CNT] = {
	0, 0, 0, 0, 0,
	0, 0, 0, 0, 0,
	0, 0, 0, 0, 0,
	1, 1, 1, 0
};

static struct smi_bwc_info bwc_info_mt6765 = {
	.scen_map = (u32 *)mtk_smi_mt6765_scen_map,
	.larb_bwl = (u8 *)mtk_smi_larb_mt6765_bwl,
	.comm_bwl = (u16 *)mtk_smi_common_mt6765_bwl,
	.comm_misc = (struct mtk_smi_reg_pair *)mtk_smi_common_mt6765_misc,
};
// end bwc config for mt6765

static const struct of_device_id of_smi_bwc_match_tbl[] = {
	{
		.compatible = "mediatek,mt6768-smi-bwc",
		.data = &bwc_info_mt6768,
	},
	{
		.compatible = "mediatek,mt6761-smi-bwc",
		.data = &bwc_info_mt6761,
	},
	{
		.compatible = "mediatek,mt6739-smi-bwc",
		.data = &bwc_info_mt6739,
	},
	{
		.compatible = "mediatek,mt6765-smi-bwc",
		.data = &bwc_info_mt6765,
	},
	{}
};

struct smi_driver_t {
	spinlock_t		lock;
	enum MTK_SMI_BWC_SCEN	scen;
	s32			table[SMI_BWC_SCEN_CNT];
};

static struct smi_driver_t smi_drv;
static struct smi_bwc_info *g_bwc_info;

struct smi_node {
	struct device	*dev;
	u32	id;
	void __iomem	*va;
};

struct smi_dev_info {
	bool			probe;
	struct smi_node	larb[MTK_LARB_NR_MAX];
	struct smi_node	comm;
};
static struct smi_dev_info	*g_dev_info;

static s32 smi_dev_probe(void)
{
	struct device_node	*node = NULL;
	struct platform_device	*pdev;
	s32			larb_nr = 0, comm_nr = 0, id;
	struct smi_dev_info	*smi = g_dev_info;
	struct resource	*res;
	void __iomem	*va;

	for_each_compatible_node(node, NULL, "mediatek,smi-larb") {

		if (of_property_read_u32(node, "mediatek,larb-id", &id))
			id = larb_nr;
		larb_nr += 1;

		pdev = of_find_device_by_node(node);
		of_node_put(node);
		if (!pdev)
			return -EINVAL;
		smi->larb[id].dev = &pdev->dev;
		smi->larb[id].id = id;
	}

	for_each_compatible_node(node, NULL, "mediatek,smi-common") {

		if (of_property_read_u32(node, "mediatek,common-id", &id))
			id = comm_nr;
		comm_nr += 1;

		pdev = of_find_device_by_node(node);
		of_node_put(node);
		if (!pdev)
			return -EINVAL;
		smi->comm.dev = &pdev->dev;
		smi->comm.id = id;

		res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
		if (!res)
			return -EINVAL;

		va = devm_ioremap(&pdev->dev, res->start, 0x1000);
		if (IS_ERR(va))
			return PTR_ERR(va);
		smi->comm.va = va;
	}

	return 0;
}

static void do_config_by_scen(s32 smi_scen)
{
	s32 i, j, index, ret;
	struct mtk_smi_reg_pair *comm_misc;
	struct smi_dev_info	*smi = g_dev_info;

	for (i = 0; i < ARRAY_SIZE(smi->larb); i++) {
		if (!smi->larb[i].dev)
			continue;
		ret = mtk_smi_larb_get(smi->larb[i].dev);

		for (j = 0; j < SMI_LARB_PORT_NR_MAX; j++) {
			index = SMI_LARB_PORT_NR_MAX * MTK_LARB_NR_MAX * smi_scen
					+ SMI_LARB_PORT_NR_MAX * i + j;
			if (g_bwc_info->larb_bwl[index] > 0) {
				mtk_smi_larb_bw_set(smi->larb[i].dev, j,
					g_bwc_info->larb_bwl[index]);
			}
		}

		if (i == 0) {
			for (j = 0; j < SMI_COMMON_LARB_NR_MAX; j++) {
				index = SMI_COMMON_LARB_NR_MAX * smi_scen + j;
				if (g_bwc_info->comm_bwl[index] > 0) {
					mtk_smi_common_bw_set(smi->larb[i].dev, j,
						g_bwc_info->comm_bwl[index]);
				}
			}
			if (!ret && g_bwc_info->comm_misc) {
				for (j = 0; j < SMI_COMMON_MISC_NR; j++) {
					index = SMI_COMMON_MISC_NR * smi_scen + j;
					comm_misc = &(g_bwc_info->comm_misc[index]);
					writel(comm_misc->value, smi->comm.va + comm_misc->offset);
				}
			}
		}

		mtk_smi_larb_put(smi->larb[i].dev);
	}
}

static s32 smi_bwc_conf(const struct MTK_SMI_BWC_CONF *conf)
{
	s32 same = 0, smi_scen, i;
	u32 *smi_scen_map = g_bwc_info->scen_map;

	if (!conf) {
		SMIDBG("MTK_SMI_BWC_CONF no such device or address\n");
		return -ENXIO;
	} else if (conf->scen >= SMI_BWC_SCEN_CNT) {
		SMIDBG("Invalid conf scenario:%u, SMI_BWC_SCEN_CNT=%u\n",
			conf->scen, SMI_BWC_SCEN_CNT);
		return -EINVAL;
	}

	spin_lock(&(smi_drv.lock));

	if (!conf->b_on) {
		if (smi_drv.table[conf->scen] <= 0)
			SMIWRN(0, "ioctl=%s:OFF not in pairs\n",
				smi_bwc_scen_name_get(conf->scen));
		else
			smi_drv.table[conf->scen] -= 1;
	} else
		smi_drv.table[conf->scen] += 1;

	for (i = SMI_BWC_SCEN_CNT - 1; i >= 0; i--)
		if (smi_drv.table[i])
			break;
	i = (i < 0 ? 0 : i);
	if (smi_scen_map[i] == smi_scen_map[smi_drv.scen])
		same += 1;
	smi_drv.scen = (i > 0 ? i : 0);
	spin_unlock(&(smi_drv.lock));

#ifdef MMDVFS_HOOK
	{
		unsigned int concurrency = 0;

		if (conf->b_on)
			mmdvfs_notify_scenario_enter(conf->scen);
		else
			mmdvfs_notify_scenario_exit(conf->scen);

		for (i = 0; i < SMI_BWC_SCEN_CNT; i++)
			concurrency |= (smi_drv.table[i] ? 1 : 0) << i;
		mmdvfs_notify_scenario_concurrency(concurrency);
	}
#endif

	smi_scen = smi_scen_map[smi_drv.scen];
	if (same) {
		pr_debug("ioctl=%s:%s, curr=%s(%d), SMI_SCEN=%d [same as prev]\n",
			smi_bwc_scen_name_get(conf->scen),
			conf->b_on ? "ON" : "OFF",
			smi_bwc_scen_name_get(smi_drv.scen),
			smi_drv.scen, smi_scen);
		return 0;
	}

	do_config_by_scen(smi_scen);

	pr_debug("ioctl=%s:%s, curr=%s(%d), SMI_SCEN=%d\n",
		smi_bwc_scen_name_get(conf->scen), conf->b_on ? "ON" : "OFF",
		smi_bwc_scen_name_get(smi_drv.scen), smi_drv.scen, smi_scen);
	return 0;
}

static s32 smi_open(struct inode *inode, struct file *file)
{
	file->private_data = kcalloc(SMI_BWC_SCEN_CNT, sizeof(u32), GFP_ATOMIC);
	if (!file->private_data) {
		SMIERR("Allocate file private data failed\n");
		return -ENOMEM;
	}
	return 0;
}

static s32 smi_release(struct inode *inode, struct file *file)
{
	kfree(file->private_data);
	file->private_data = NULL;
	return 0;
}

static long smi_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	s32 ret = 0;
	struct smi_dev_info	*smi = g_dev_info;

	if (!g_bwc_info) {
		SMIWRN(0, "not support legacy bwc function\n", cmd, ret);
		return -EINVAL;
	}
	if (!file->f_op || !file->f_op->unlocked_ioctl)
		return -ENOTTY;

	spin_lock(&(smi_drv.lock));
	if (!smi->probe) {
		ret = smi_dev_probe();
		if (ret) {
			spin_unlock(&(smi_drv.lock));
			SMIDBG("cannot probe smi dev info.\n");
			return -ENXIO;
		}

		smi->probe = true;
	}
	spin_unlock(&(smi_drv.lock));

	switch (cmd) {
	case MTK_IOC_SMI_BWC_CONF:
	{
		struct MTK_SMI_BWC_CONF conf;

		ret = copy_from_user(&conf, (void *)arg, sizeof(conf));
		if (ret)
			SMIWRN(0, "CMD%u copy from user failed:%d\n", cmd, ret);
		else
			ret = smi_bwc_conf(&conf);
		break;
	}
	case MTK_IOC_MMDVFS_QOS_CMD:
	{
		struct MTK_MMDVFS_QOS_CMD config;

		ret = copy_from_user(&config, (void *)arg,
			sizeof(struct MTK_MMDVFS_QOS_CMD));
		if (ret) {
			SMIWRN(0, "cmd %u copy_from_user fail: %d\n",
				cmd, ret);
		} else {
			switch (config.type) {
			case MTK_MMDVFS_QOS_CMD_TYPE_SET:
#ifdef HRT_MECHANISM
				mmdvfs_set_max_camera_hrt_bw(
						config.max_cam_bw);
#endif
				config.ret = 0;
				break;
			default:
				SMIWRN(0, "invalid mmdvfs QOS cmd\n");
				ret = -EINVAL;
				break;
			}
		}
		break;
	}
#ifdef MMDVFS_HOOK
	case MTK_IOC_SMI_BWC_INFO_SET:
		ret = set_mm_info_ioctl_wrapper(file, cmd, arg);
		break;
	case MTK_IOC_SMI_BWC_INFO_GET:
		ret = get_mm_info_ioctl_wrapper(file, cmd, arg);
		break;
	case MTK_IOC_MMDVFS_CMD:
	{
		struct MTK_MMDVFS_CMD mmdvfs_cmd;

		if (copy_from_user(&mmdvfs_cmd,
			(void *)arg, sizeof(struct MTK_MMDVFS_CMD))) {
			ret = -EFAULT;
			break;
		}

		SMIERR("bwc mmdvfs_handle_cmd.\n");
		mmdvfs_handle_cmd(&mmdvfs_cmd);

		if (copy_to_user((void *)arg,
			(void *)&mmdvfs_cmd, sizeof(struct MTK_MMDVFS_CMD))) {
			ret = -EFAULT;
			break;
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
	compat_int_t scen;
	compat_int_t b_on;
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
	_IOW('O', 24, struct MTK_SMI_COMPAT_BWC_CONFIG)
static int smi_bwc_config_compat_get(
	struct MTK_SMI_BWC_CONF __user *data,
	struct MTK_SMI_COMPAT_BWC_CONFIG __user *data32)
{
	compat_int_t i;
	int ret;

	ret = get_user(i, &(data32->scen));
	ret |= put_user(i, &(data->scen));
	ret |= get_user(i, &(data32->b_on));
	ret |= put_user(i, &(data->b_on));
	return ret;
}

#define COMPAT_MTK_IOC_SMI_BWC_INFO_SET \
	_IOWR('O', 28, struct MTK_SMI_COMPAT_BWC_INFO_SET)
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
	_IOWR('O', 29, struct MTK_SMI_COMPAT_BWC_MM_INFO)
static int smi_bwc_info_compat_get(
	struct MTK_SMI_BWC_MM_INFO __user *data,
	struct MTK_SMI_COMPAT_BWC_MM_INFO __user *data32)
{
	compat_uint_t u;
	compat_int_t p[2] = {0};
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
	compat_int_t p[2] = {0};
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
		struct MTK_SMI_BWC_CONF __user *data;
		struct MTK_SMI_COMPAT_BWC_CONFIG __user *data32;

		data32 = compat_ptr(arg);

		if (cmd == MTK_IOC_SMI_BWC_CONF)
			return file->f_op->unlocked_ioctl(file, cmd,
				(unsigned long)data32);

		data = compat_alloc_user_space(
			sizeof(struct MTK_SMI_BWC_CONF));
		if (!data)
			return -EFAULT;

		ret = smi_bwc_config_compat_get(data, data32);
		if (ret)
			return ret;

		return file->f_op->unlocked_ioctl(file,
			MTK_IOC_SMI_BWC_CONF, (unsigned long)data);
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

static int smi_bwc_probe(struct platform_device *pdev)
{
	int ret = 0;
	dev_t			dev_no;
	struct cdev		*cdev;
	struct class		*class;
	struct device		*device;

	struct smi_dev_info	*dev_info;

#ifdef MMDVFS_HOOK
	mmdvfs_init();
	mmdvfs_clks_init((&pdev->dev)->of_node);
#endif

	dev_info = kzalloc(sizeof(*dev_info), GFP_KERNEL);
	if (!dev_info)
		return -ENOMEM;
	g_dev_info = dev_info;
	g_bwc_info =
		(struct smi_bwc_info *)of_device_get_match_data(&pdev->dev);
	dev_no = MKDEV(MTK_SMI_MAJOR_NUMBER, 0);
	if (alloc_chrdev_region(&dev_no, 0, 1, DEV_NAME))
		SMIERR("Allocate chrdev region failed\n");
	cdev = cdev_alloc();
	if (!cdev)
		SMIERR("Allocate cdev failed\n");
	else {
		cdev_init(cdev, &smi_file_opers);
		cdev->owner = THIS_MODULE;
		cdev->dev = dev_no;
		if (cdev_add(cdev, dev_no, 1))
			SMIERR("Add cdev failed\n");
	}
	class = class_create(THIS_MODULE, DEV_NAME);
	if (IS_ERR(class))
		SMIERR("Create class failed: %ld\n", PTR_ERR(class));
	device = device_create(class, NULL, dev_no, NULL, DEV_NAME);
	if (IS_ERR(device))
		SMIERR("Create device failed: %ld\n", PTR_ERR(device));

	spin_lock_init(&(smi_drv.lock));
	smi_drv.scen = SMI_BWC_SCEN_NORMAL;
	memset(&smi_drv.table, 0, sizeof(smi_drv.table));
	smi_drv.table[smi_drv.scen] += 1;
	SMIDBG("%s done!!\n", __func__);
	return ret;
}

int smi_bwc_remove(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver smi_bwc_drv = {
	.probe = smi_bwc_probe,
	.remove = smi_bwc_remove,
	.driver = {
		.name = "mtk-smi-bwc",
		.owner = THIS_MODULE,
		.of_match_table = of_smi_bwc_match_tbl,
	},
};
static int __init mtk_smi_bwc_init(void)
{
	s32 status;

	status = platform_driver_register(&smi_bwc_drv);
	if (status) {
		pr_notice(
			"Failed to register SMI bwc driver(%d)\n",
			status);
		return -ENODEV;
	}

	return 0;
}

static void __exit mtk_smi_bwc_exit(void)
{
	platform_driver_unregister(&smi_bwc_drv);
}

module_init(mtk_smi_bwc_init);
module_exit(mtk_smi_bwc_exit);
MODULE_DESCRIPTION("MTK SMI bwc driver for legacy chips");
MODULE_AUTHOR("Zhiyin Luo<zhiyin.luo@mediatek.com>");
MODULE_LICENSE("GPL");
