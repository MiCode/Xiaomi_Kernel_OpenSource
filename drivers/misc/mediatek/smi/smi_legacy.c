/*
 * Copyright (C) 2018 MediaTek Inc.
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
#include <linux/sched/clock.h>

#include <aee.h>
#include <mtk_smi.h>
#include <soc/mediatek/smi.h>
#include "smi_public.h"

#if IS_ENABLED(CONFIG_MACH_MT6771)
#include <clk-mt6771-pg.h>
#include "smi_conf_mt6771.h"
#else
#include "smi_conf_dft.h"
#endif

#if IS_ENABLED(CONFIG_MTK_M4U)
#include <m4u.h>
#endif

#define DEV_NAME "MTK_SMI"
#undef pr_fmt
#define pr_fmt(fmt) "[" DEV_NAME "]%s: " fmt, __func__

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
		aee_kernel_warning(DEV_NAME, string, ##args); \
	} while (0)

struct smi_driver {
	spinlock_t		lock;
	enum MTK_SMI_BWC_SCEN	scen;
	s32			table[SMI_BWC_SCEN_CNT];
};

struct smi_mtcmos {
	u64 sec;
	u32 nsec;
	char user[NAME_MAX];
};

static struct smi_driver smi_drv;
static struct smi_mtcmos smi_record[SMI_LARB_NUM][2];

static u32 nr_larbs;
static struct mtk_smi_dev *smi_dev[SMI_LARB_NUM + 1];

static bool smi_reg_first, smi_bwc_conf_dis;
static u32 smi_mm_first;

static void smi_mtcmos_record(const u32 id, const char *user, const bool enable)
{
	struct smi_mtcmos *record;

	if (id >= nr_larbs) {
		SMIDBG("Invalid id:%2u, nr_larbs=%u, user=%s\n",
			id, nr_larbs, user);
		return;
	}
	record = &smi_record[id][enable];
	record->sec = sched_clock();
	record->nsec = do_div(record->sec, 1000000000) / 1000;
	strncpy(record->user, user, NAME_MAX);
	record->user[sizeof(record->user) - 1] = '\0';
}

bool smi_mm_first_check(void)
{
	return smi_mm_first ? true : false;
}
EXPORT_SYMBOL_GPL(smi_mm_first_check);

s32 smi_bus_prepare_enable(const u32 id, const char *user, const bool mtcmos)
{
	s32 cnts, ret;

	if (id > nr_larbs) {
		SMIDBG("Invalid id:%2u, nr_larbs=%u, user=%s\n",
			id, nr_larbs, user);
		return -EINVAL;
	}

	/* COMM */
	if (mtcmos) {
		ret = clk_prepare_enable(smi_dev[nr_larbs]->clks[0]);
		if (ret) {
			SMIERR("COMMON MTCMOS enable failed: %d\n", ret);
			return ret;
		}
	}
	ret = mtk_smi_clk_enable(smi_dev[nr_larbs]);
	if (ret)
		return ret;
	atomic_inc(&(smi_dev[nr_larbs]->clk_cnts));

	if (id == nr_larbs)
		return ret;
	/* LARB */
	if (mtcmos) {
		ret = clk_prepare_enable(smi_dev[id]->clks[0]);
		if (ret) {
			SMIERR("LARB%u MTCMOS enable failed: %d\n", id, ret);
			return ret;
		}
	}
	ret = mtk_smi_clk_enable(smi_dev[id]);
	if (ret)
		return ret;
	atomic_inc(&(smi_dev[id]->clk_cnts));

	/* record */
	if (mtcmos)
		smi_mtcmos_record(id, user, true);
	cnts = atomic_read(&(smi_dev[id]->clk_cnts));
	SMIDBG("LARB%2u ON(%d) by%16s\n", id, cnts, user);
	return ret;
}
EXPORT_SYMBOL_GPL(smi_bus_prepare_enable);

s32 smi_bus_disable_unprepare(const u32 id, const char *user, const bool mtcmos)
{
	s32 cnts, ret;

	if (id > nr_larbs) {
		SMIDBG("Invalid id:%2u, nr_larbs=%u, user=%s\n",
			id, nr_larbs, user);
		return -EINVAL;
	} else if (id == nr_larbs)
		goto smi_comm_disable_unprepare;

	/* check */
	cnts = atomic_read(&(smi_dev[id]->clk_cnts));
	if (cnts == 1 && readl(smi_dev[id]->base + SMI_LARB_STAT))
		SMIWRN(1, "LARB%2u OFF(%d) by%16s, but still busy\n",
			id, cnts, user);
	else
		SMIDBG("LARB%2u OFF(%d) by%16s\n", id, cnts, user);
	if (mtcmos)
		smi_mtcmos_record(id, user, false);

	/* LARB */
	atomic_dec(&(smi_dev[id]->clk_cnts));
	mtk_smi_clk_disable(smi_dev[id]);
	if (mtcmos)
		clk_disable_unprepare(smi_dev[id]->clks[0]);

	/* COMM */
smi_comm_disable_unprepare:
	atomic_dec(&(smi_dev[nr_larbs]->clk_cnts));
	mtk_smi_clk_disable(smi_dev[nr_larbs]);
	if (mtcmos)
		clk_disable_unprepare(smi_dev[nr_larbs]->clks[0]);
	return ret;
}
EXPORT_SYMBOL_GPL(smi_bus_disable_unprepare);

static s32 smi_debug_dumper(u32 id, const bool off, const bool gce)
{
	char *name, buffer[NAME_MAX + 1];
	void __iomem *base;
	u32 nr_debugs, *debugs, len, temp[NAME_MAX];
	s32 i, j, ret;

	if (id > nr_larbs + 1) {
		SMIDBG("Invalid id:%2u, nr_larbs=%u\n", id, nr_larbs);
		return -EINVAL;
	} else if (id < nr_larbs) {
		name = "LARB";
		base = smi_dev[id]->base;
		nr_debugs = SMI_LARB_DEBUG_NUM;
		debugs = smi_larb_debug_offset;
	} else if (id == nr_larbs) {
		name = "COMM";
		base = smi_dev[id]->base;
		nr_debugs = SMI_COMM_DEBUG_NUM;
		debugs = smi_comm_debug_offset;
	} else {
		name = "MMSYS";
		base = smi_mmsys_base;
		nr_debugs = SMI_MMSYS_DEBUG_NUM;
		debugs = smi_mmsys_debug_offset;
		id = nr_larbs;
	}
	if (!base || !nr_debugs || !debugs) {
		SMIDBG("Invalid base, nr_debugs or debugs from conf header\n");
		return -ENXIO;
	}

	if (off) {
		SMIWRN(gce, "======== %s%d offset ========\n", name, id);
		for (i = 0; i < nr_debugs; i += j) {
			len = 0;
			for (j = 0; i + j < nr_debugs; j++) {
				ret = snprintf(buffer + len, NAME_MAX - len,
					" %#x,", debugs[i + j]);
				if (ret < 0 || len + ret >= NAME_MAX) {
					snprintf(buffer + len, NAME_MAX - len,
						"%c", '\0');
					break;
				}
				len += ret;
			}
			SMIWRN(gce, "%s\n", buffer);
		}
		return 0;
	}
	/* read */
	for (i = 0; i < nr_debugs &&
		atomic_read(&(smi_dev[id]->clk_cnts)) > 0; i++)
		temp[i] = readl(base + debugs[i]);
	if (i < nr_debugs) {
		SMIWRN(gce, "======== %s%d MTCMOS OFF ========\n", name, id);
		return 0;
	}
	/* print */
	SMIWRN(gce, "======== %s%d non-zero value, clk:%d ========\n",
		name, id, atomic_read(&(smi_dev[id]->clk_cnts)));
	for (i = 0; i < nr_debugs; i += j) {
		len = 0;
		for (j = 0; i + j < nr_debugs; j++) {
			if (temp[i + j])
				ret = snprintf(buffer + len, NAME_MAX - len,
					" %#x=%#x,",
					debugs[i + j], temp[i + j]);
			else
				continue;
			if (ret < 0 || len + ret >= NAME_MAX) {
				snprintf(buffer + len, NAME_MAX - len,
					"%c", '\0');
				break;
			}
			len += ret;
		}
		SMIWRN(gce, "%s\n", buffer);
	}
	return 0;
}

static void smi_debug_dump_status(const bool gce)
{
	s32 i;

	for (i = 0; i <= nr_larbs + 1; i++)
		smi_debug_dumper(i, false, gce);

	SMIWRN(gce, "SCEN=%s(%d), SMI=%d\n",
		smi_bwc_scen_name_get(smi_drv.scen),
		smi_drv.scen, smi_scen_map[smi_drv.scen]);
}

s32 smi_debug_bus_hang_detect(const bool gce)
{
	u32 time = 5, busy[SMI_LARB_NUM + 1] = {0};
	s32 i, j, ret = 0;

	for (i = 0; i < time; i++) {
		for (j = 0; j < nr_larbs; j++)
			busy[j] += ((atomic_read(&(smi_dev[j]->clk_cnts)) > 0
			&& readl(smi_dev[j]->base + SMI_LARB_STAT)) ? 1 : 0);
		busy[j] += ((atomic_read(&(smi_dev[j]->clk_cnts)) > 0
		&& !(readl(smi_dev[j]->base + SMI_DEBUG_MISC) & 0x1)) ? 1 : 0);
	}
	for (i = 0; i < nr_larbs; i++)
		ret += (busy[i] < time ? 0 : 1);
	if (!ret) {
		SMIWRN(gce, "SMI bus NOT hang\n");
		smi_debug_dump_status(gce);
		return ret;
	}

	SMIWRN(gce, "SMI bus may hang by M4U/EMI/DRAMC, full dump as follow\n");
	for (i = 0; i < time; i++)
		for (j = 0; j <= nr_larbs + 1; j++)
			smi_debug_dumper(j, !i, gce);
	smi_debug_dump_status(gce);

	for (i = 0; i < nr_larbs; i++)
		SMIWRN(gce,
			"LARB%2u=%u/%u busy with clk:%d, COMM%2u=%u/%u busy with clk:%2d\n",
			i, busy[i], time, atomic_read(&(smi_dev[i]->clk_cnts)),
			nr_larbs, busy[nr_larbs], time,
			atomic_read(&(smi_dev[nr_larbs]->clk_cnts)));

	for (i = 0; i < nr_larbs; i++)
		SMIWRN(gce,
			"LARB%2u OFF:%5llu.%6lu by%16s, ON:%5llu.%6lu by%16s\n",
			i, smi_record[i][0].sec, smi_record[i][0].nsec,
			smi_record[i][0].user, smi_record[i][1].sec,
			smi_record[i][1].nsec, smi_record[i][1].user);

#if IS_ENABLED(CONFIG_MTK_M4U)
	m4u_dump_reg_for_smi_hang_issue();
#endif
	return ret;
}

static s32 smi_bwc_conf(const struct MTK_SMI_BWC_CONF *conf)
{
	s32 same = 0, smi_scen, i;

	if (smi_bwc_conf_dis) {
		SMIDBG("SMI BWC configuration disable: %u\n", smi_bwc_conf_dis);
		return 0;
	} else if (!conf) {
		SMIDBG("MTK_SMI_BWC_CONF no such device or address\n");
		return -ENXIO;
	} else if (conf->scen >= SMI_BWC_SCEN_CNT) {
		SMIDBG("Invalid conf scenario:%2u, SMI_BWC_SCEN_CNT=%u\n",
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
	if (smi_scen_map[i] == smi_scen_map[smi_drv.scen])
		same += 1;
	smi_drv.scen = (i > 0 ? i : 0);
	spin_unlock(&(smi_drv.lock));

	smi_scen = smi_scen_map[smi_drv.scen];
	if (same) {
		SMIDBG("ioctl=%s:%s, curr=%s(%d), SMI=%d: [same as prev]\n",
			smi_bwc_scen_name_get(conf->scen),
			conf->b_on ? "ON" : "OFF",
			smi_bwc_scen_name_get(smi_drv.scen),
			smi_drv.scen, smi_scen);
		return 0;
	}
	for (i = 0; i <= nr_larbs; i++)
		mtk_smi_conf_set(smi_dev[i], smi_scen);
	SMIDBG("ioctl=%s:%s, curr=%s(%d), SMI=%d\n",
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
	s32 ret;

	if (!file->f_op || !file->f_op->unlocked_ioctl)
		return -ENOTTY;

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
	default:
		ret = -ENOIOCTLCMD;
		break;
	}
	return ret;
}

static const struct file_operations smi_file_opers = {
	.owner = THIS_MODULE,
	.open = smi_open,
	.release = smi_release,
	.unlocked_ioctl = smi_ioctl,
};

static s32 smi_larb_non_sec_con_set(const u32 id)
{
	s32 i;

	if (id >= nr_larbs) {
		SMIDBG("Invalid id:%2u, nr_larbs=%u\n", id, nr_larbs);
		return -EINVAL;
	} else if (!smi_dev[id] || !smi_dev[id]->base) {
		SMIDBG("LARB%u no such device or address\n", id);
		return -ENXIO;
	}

	for (i = smi_larb_cmd_gr_en_port[id][0];
		i < smi_larb_cmd_gr_en_port[id][1]; i++) /* CMD_GR */
		writel(readl(smi_dev[id]->base + SMI_LARB_NON_SEC_CON(i)) | 0x2,
			smi_dev[id]->base + SMI_LARB_NON_SEC_CON(i));
	for (i = smi_larb_bw_thrt_en_port[id][0];
		i < smi_larb_bw_thrt_en_port[id][1]; i++) /* BW_THRT */
		writel(readl(smi_dev[id]->base + SMI_LARB_NON_SEC_CON(i)) | 0x8,
			smi_dev[id]->base + SMI_LARB_NON_SEC_CON(i));
	return 0;
}

static u32 smi_subsys_larbs(enum subsys_id sys)
{
#if IS_ENABLED(CONFIG_MACH_MT6771)
	switch (sys) {
	case SYS_DIS:
		return (1 << 0);
	case SYS_VDE:
		return (1 << 1);
	case SYS_ISP:
		return ((1 << 2) | (1 << 5));
	case SYS_VEN:
		return (1 << 4);
	case SYS_CAM:
		return ((1 << 3) | (1 << 6));
	default:
		return 0;
	}
#endif
	return 0;
}

static void smi_subsys_after_on(enum subsys_id sys)
{
	u32 subsys = smi_subsys_larbs(sys);
	u32 smi_scen = smi_scen_map[smi_drv.scen];
	s32 i;

	if (smi_scen >= SMI_SCEN_NUM) {
		SMIDBG("Invalid scen_id:%u, SMI_SCEN_NUM=%u\n",
			smi_scen, SMI_SCEN_NUM);
		return;
	}
	/* COMM */
	if (subsys & 1) {
		smi_bus_prepare_enable(nr_larbs, DEV_NAME, false);
		mtk_smi_conf_set(smi_dev[nr_larbs], smi_scen);
		smi_bus_disable_unprepare(nr_larbs, DEV_NAME, false);
	}
	/* LARB */
	for (i = 0; i < nr_larbs; i++)
		if (subsys & (1 << i)) {
			smi_bus_prepare_enable(i, DEV_NAME, false);
			mtk_smi_conf_set(smi_dev[i], smi_scen);
			smi_larb_non_sec_con_set(i);
			smi_bus_disable_unprepare(i, DEV_NAME, false);
		}
}

static struct pg_callbacks smi_clk_subsys_handle = {
	.after_on = smi_subsys_after_on
};

static s32 smi_conf_get(const u32 id)
{
	if (id > nr_larbs) {
		SMIDBG("Invalid id:%2u, nr_larbs=%u\n", id, nr_larbs);
		return -EINVAL;
	}

	smi_dev[id] = mtk_smi_dev_get(id);
	if (!smi_dev[id]) {
		SMIDBG("SMI%u no such device or address\n", id);
		return -ENXIO;
	}

	smi_dev[id]->nr_conf_pairs = smi_conf_pair_num[id];
	smi_dev[id]->conf_pairs = smi_conf_pair[id];
	smi_dev[id]->nr_scen_pairs = smi_scen_pair_num[id];
	smi_dev[id]->scen_pairs = smi_scen_pair[id];
	return 0;
}

s32 smi_register(void)
{
	dev_t			dev_no;
	struct cdev		*cdev;
	struct class		*class;
	struct device		*device;
	struct device_node	*of_node;
	s32 i;

	if (smi_reg_first)
		return 0;
	/* device */
	dev_no = MKDEV(MTK_SMI_MAJOR_NUMBER, 0);
	if (alloc_chrdev_region(&dev_no, 0, 1, DEV_NAME))
		SMIERR("Allocate chrdev region failed\n");

	cdev = cdev_alloc();
	if (!cdev)
		SMIERR("Allocate cdev failed\n");
	cdev_init(cdev, &smi_file_opers);
	cdev->owner = THIS_MODULE;
	cdev->dev = dev_no;
	if (cdev_add(cdev, dev_no, 1))
		SMIERR("Add cdev failed\n");

	class = class_create(THIS_MODULE, DEV_NAME);
	if (IS_ERR(class))
		SMIERR("Create class failed: %d\n", PTR_ERR(class));
	device = device_create(class, NULL, dev_no, NULL, DEV_NAME);
	if (IS_ERR(device))
		SMIERR("Create device failed: %d\n", PTR_ERR(device));

	/* driver */
	spin_lock_init(&(smi_drv.lock));
	memset(smi_drv.table, 0, sizeof(smi_drv.table));
	smi_drv.scen = SMI_BWC_SCEN_NORMAL;
	smi_drv.table[smi_drv.scen] += 1;

	/* init */
	nr_larbs = SMI_LARB_NUM;
	smi_mm_first = smi_subsys_larbs(SYS_DIS) | (1 << nr_larbs);
	for (i = nr_larbs; i >= 0; i--) {
		smi_conf_get(i);
		if (smi_mm_first & (1 << i)) {
			smi_bus_prepare_enable(i, "SMI_MM", true);
			mtk_smi_conf_set(smi_dev[i], smi_drv.scen);
			smi_larb_non_sec_con_set(i);
		}
	}

	/* mmsys */
	of_node = of_parse_phandle(
		smi_dev[nr_larbs]->dev->of_node, "mmsys_config", 0);
	smi_mmsys_base = (void *)of_iomap(of_node, 0);
	of_node_put(of_node);
	if (!smi_mmsys_base) {
		SMIERR("Unable to parse or iomap mmsys_config\n");
		return -ENOMEM;
	}
	register_pg_callback(&smi_clk_subsys_handle);
	return 0;
}
EXPORT_SYMBOL_GPL(smi_register);

static s32 __init smi_late_init(void)
{
	s32 i;

	for (i = 0; i <= nr_larbs; i++)
		if (smi_mm_first & (1 << i))
			smi_bus_disable_unprepare(i, "SMI_MM", true);
	smi_mm_first = 0;
	smi_reg_first = true;
	return 0;
}
late_initcall(smi_late_init);
