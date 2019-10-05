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
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/of_address.h>
#include <linux/sched/clock.h>

#include <soc/mediatek/smi.h>
#include <mtk_smi.h>
#include <aee.h>

#include <smi_conf.h>
#include <smi_public.h>
#include <smi_pmqos.h>
#if IS_ENABLED(CONFIG_MTK_MMDVFS)
#include <mmdvfs_pmqos.h>
#endif
#if IS_ENABLED(CONFIG_MTK_M4U)
#include <m4u.h>
#endif
#ifdef MMDVFS_HOOK
#include <mmdvfs_mgr.h>
#endif

#if IS_ENABLED(CONFIG_MTK_TINYSYS_SSPM_SUPPORT)
#include <sspm_ipi.h>
#include <sspm_define.h>
#include <sspm_reservedmem_define.h>
#endif

#define DEV_NAME "MTK_SMI"
#undef pr_fmt
#define pr_fmt(fmt) "[" DEV_NAME "]" fmt

#define SMIDBG(string, args...) pr_debug(string, ##args)

#if IS_ENABLED(CONFIG_MTK_CMDQ)
#include <cmdq_helper_ext.h>
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

#ifndef ATOMR_CLK
#define ATOMR_CLK(i) atomic_read(&(smi_dev[(i)]->clk_cnts))
#endif

struct smi_driver_t {
	spinlock_t		lock;
	enum MTK_SMI_BWC_SCEN	scen;
	s32			table[SMI_BWC_SCEN_CNT];
};

struct smi_record_t {
	/* clk from api */
	char user[NAME_MAX];
	u64 clk_sec;
	u32 clk_nsec;
	/* mtcmos cb from ccf */
	atomic_t on;
	u64 sec;
	u32 nsec;
};

struct smi_dram_t {
	u8		dump;
	phys_addr_t	size;
	void __iomem	*virt;
	struct dentry	*node;
};

static struct smi_driver_t	smi_drv;
static struct smi_record_t	smi_record[SMI_LARB_NUM][2];
static struct smi_dram_t	smi_dram;

static struct mtk_smi_dev	*smi_dev[SMI_LARB_NUM + 1];

static bool smi_reg_first, smi_bwc_conf_dis;
static u32 smi_subsys_on, smi_mm_first;

bool smi_mm_first_get(void)
{
	return smi_mm_first ? true : false;
}
EXPORT_SYMBOL_GPL(smi_mm_first_get);

static void smi_clk_record(const u32 id, const bool en, const char *user)
{
	struct smi_record_t *record;
	u64 sec;
	u32 nsec;

	if (id >= SMI_LARB_NUM && user) {
		SMIDBG("Invalid id:%u, SMI_LARB_NUM=%u, user=%s\n",
			id, SMI_LARB_NUM, user);
		return;
	} else if (id == SMI_LARB_NUM)
		return;

	record = &smi_record[id][en ? 1 : 0];
	sec = sched_clock();
	nsec = do_div(sec, 1000000000) / 1000;

	if (user) {
		record->clk_sec = sec;
		record->clk_nsec = nsec;
		strncpy(record->user, user, NAME_MAX);
		record->user[sizeof(record->user) - 1] = '\0';
	} else {
		record->sec = sec;
		record->nsec = nsec;
		atomic_set(&(record->on), en ? 1 : 0);
		atomic_set(&(smi_record[id][en ? 0 : 1].on), en ? 1 : 0);
	}
}

static inline s32 smi_unit_prepare_enable(const u32 id)
{
	s32 ret = 0;

	ret = clk_prepare_enable(smi_dev[id]->clks[0]);
	if (ret) {
		SMIERR("SMI%u MTCMOS enable failed: %d\n", id, ret);
		return ret;
	}
	return mtk_smi_clk_enable(smi_dev[id]);
}

s32 smi_bus_prepare_enable(const u32 id, const char *user)
{
	s32 ret;

	if (id > SMI_LARB_NUM) {
		SMIDBG("Invalid id:%u, SMI_LARB_NUM=%u, user=%s\n",
			id, SMI_LARB_NUM, user);
		return -EINVAL;
	} else if (id < SMI_LARB_NUM)
		smi_clk_record(id, true, user);

	ret = smi_unit_prepare_enable(SMI_LARB_NUM);
	if (ret || id == SMI_LARB_NUM)
		return ret;
	return smi_unit_prepare_enable(id);
}
EXPORT_SYMBOL_GPL(smi_bus_prepare_enable);

static inline void smi_unit_disable_unprepare(const u32 id)
{
	mtk_smi_clk_disable(smi_dev[id]);
	clk_disable_unprepare(smi_dev[id]->clks[0]);
}

s32 smi_bus_disable_unprepare(const u32 id, const char *user)
{
	if (id > SMI_LARB_NUM) {
		SMIDBG("Invalid id:%u, SMI_LARB_NUM=%u, user=%s\n",
			id, SMI_LARB_NUM, user);
		return -EINVAL;
	} else if (id == SMI_LARB_NUM) {
		smi_unit_disable_unprepare(SMI_LARB_NUM);
		return 0;
	} else if (id < SMI_LARB_NUM)
		smi_clk_record(id, false, user);

	if (ATOMR_CLK(id) == 1 && readl(smi_dev[id]->base + SMI_LARB_STAT))
		SMIWRN(1, "LARB%u OFF by %s but busy\n", id, user);
	smi_unit_disable_unprepare(id);
	smi_unit_disable_unprepare(SMI_LARB_NUM);
	return 0;
}
EXPORT_SYMBOL_GPL(smi_bus_disable_unprepare);

void
smi_bwl_update(const u32 id, const u32 bwl, const bool soft, const char *user)
{
	u32 val;

	if (id >= SMI_LARB_NUM) {
		SMIDBG("Invalid id:%u, SMI_LARB_NUM=%u\n", id, SMI_LARB_NUM);
		return;
	}

	val = (soft ? 0x1000 : 0x3000) | SMI_PMQOS_BWL_MASK(bwl);
	smi_scen_pair[SMI_LARB_NUM][SMI_ESL_INIT][id].val = val;

	if (ATOMR_CLK(SMI_LARB_NUM)) {
		smi_bus_prepare_enable(SMI_LARB_NUM, user);
		writel(val, smi_dev[SMI_LARB_NUM]->base +
			smi_scen_pair[SMI_LARB_NUM][SMI_ESL_INIT][id].off);
		smi_bus_disable_unprepare(SMI_LARB_NUM, user);
	}
}
EXPORT_SYMBOL_GPL(smi_bwl_update);

void smi_ostd_update(const struct plist_head *head, const char *user)
{
#if IS_ENABLED(CONFIG_MTK_MMDVFS)
	struct mm_qos_request *req;
	u32 larb, port, ostd;

	if (plist_head_empty(head))
		return;

	plist_for_each_entry(req, head, owner_node) {
		larb = SMI_PMQOS_LARB_DEC(req->master_id);
		if (!req->updated || larb >= SMI_LARB_NUM)
			continue;

		port = SMI_PMQOS_PORT_MASK(req->master_id);
		if (!req->ostd)
			ostd = 0x1;
		else if (req->ostd > SMI_OSTD_MAX)
			ostd = SMI_OSTD_MAX;
		else
			ostd = req->ostd;
		smi_scen_pair[larb][SMI_ESL_INIT][port].val = ostd;

		if (ATOMR_CLK(larb)) {
			smi_bus_prepare_enable(larb, user);
			writel(ostd, smi_dev[larb]->base +
				smi_scen_pair[larb][SMI_ESL_INIT][port].off);
			smi_bus_disable_unprepare(larb, user);
		}
		req->updated = false;
	}
#endif
}
EXPORT_SYMBOL_GPL(smi_ostd_update);

static inline void
smi_debug_print(const bool gce, const u32 num, const u32 *pos, const u32 *val)
{
	char buf[LINK_MAX + 1];
	s32 len, i, j, ret;

	for (i = 0; i < num; i += j) {
		len = 0;
		for (j = 0; i + j < num; j++) {
			if (!val)
				ret = snprintf(buf + len, LINK_MAX - len,
					" %#x,", pos[i + j]);
			else if (val[i + j])
				ret = snprintf(buf + len, LINK_MAX - len,
					" %#x=%#x,", pos[i + j], val[i + j]);
			else
				continue;

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
		SMIDBG("Invalid id:%u, SMI_LARB_NUM=%u\n", id, SMI_LARB_NUM);
		return -EINVAL;
	}
	j = (id > SMI_LARB_NUM ? SMI_LARB_NUM : id);
	name = (id > SMI_LARB_NUM ? "MMSYS" :
		(id < SMI_LARB_NUM ? "LARB" : "COMM"));
	base = (id > SMI_LARB_NUM ? smi_mmsys_base : smi_dev[id]->base);
	nr_debugs = (id > SMI_LARB_NUM ? SMI_MMSYS_DEBUG_NUM :
		(id < SMI_LARB_NUM ? SMI_LARB_DEBUG_NUM : SMI_COMM_DEBUG_NUM));
	debugs = (id > SMI_LARB_NUM ?
		smi_mmsys_debug_offset : (id < SMI_LARB_NUM ?
		smi_larb_debug_offset : smi_comm_debug_offset));
	if (!base || !nr_debugs || !debugs) {
		SMIDBG("Invalid base, nr_debugs, debugs of %s%u\n", name, id);
		return -ENXIO;
	}

	if (off) {
		SMIWRN(gce, "======== %s offset ========\n", name);
		smi_debug_print(gce, nr_debugs, debugs, NULL);
		return 0;
	}

	for (i = 0; i < nr_debugs && ATOMR_CLK(j) > 0; i++)
		temp[i] = readl(base + debugs[i]);
	if (i < nr_debugs) {
		SMIWRN(gce, "======== %s%u OFF ========\n", name, id);
		return 0;
	}

	SMIWRN(gce, "======== %s%u non-zero value, clk:%d ========\n",
		name, id, ATOMR_CLK(j));
	smi_debug_print(gce, nr_debugs, debugs, temp);
	return 0;
}

static void smi_debug_dump_status(const bool gce)
{
	s32 on, i;

	for (i = 0; i <= SMI_LARB_NUM + 1; i++)
		smi_debug_dumper(gce, false, i);

	SMIWRN(gce, "SCEN=%s(%d), SMI_SCEN=%d\n",
		smi_bwc_scen_name_get(smi_drv.scen),
		smi_drv.scen, smi_scen_map[smi_drv.scen]);

	for (i = 0; i < SMI_LARB_NUM; i++) {
		on = atomic_read(&(smi_record[i][0].on));
		SMIWRN(gce,
			"LARB%u:[%cON][%5llu.%6u][%16s]/[%cOFF][%5llu.%6u][%16s]\n",
			i, on ? '*' : ' ', smi_record[i][1].sec,
			smi_record[i][1].nsec, smi_record[i][1].user,
			on ? ' ' : '*', smi_record[i][0].sec,
			smi_record[i][0].nsec, smi_record[i][0].user);
	}
}

s32 smi_debug_bus_hang_detect(const bool gce, const char *user)
{
	u32 time = 5, busy[SMI_LARB_NUM + 1] = {0};
	s32 i, j, ret = 0;

	/* dump_emi_outstanding(); */
#if IS_ENABLED(CONFIG_MTK_M4U)
	m4u_dump_reg_for_smi_hang_issue();
#endif
	for (i = 0; i < time; i++) {
		for (j = 0; j < SMI_LARB_NUM; j++)
			busy[j] += ((ATOMR_CLK(j) > 0 &&
			readl(smi_dev[j]->base + SMI_LARB_STAT)) ? 1 : 0);
		/* COMM */
		busy[j] += ((ATOMR_CLK(j) > 0 &&
		!(readl(smi_dev[j]->base + SMI_DEBUG_MISC) & 0x1)) ? 1 : 0);
	}

	for (i = 0; i < SMI_LARB_NUM && !ret; i++)
		ret = (busy[i] == time ? i : ret);
	if (!ret || busy[SMI_LARB_NUM] < time) {
		SMIWRN(gce, "SMI MM bus NOT hang, check master %s\n", user);
		smi_debug_dump_status(gce);
		return 0;
	}

	SMIWRN(gce, "SMI MM bus may hang by %s/M4U/EMI/DVFS\n", user);
	for (i = 0; i < time; i++)
		for (j = 0; j <= SMI_LARB_NUM + 1; j++) {
			if (!i && j && j < SMI_LARB_NUM) /* larb offset once */
				continue;
			smi_debug_dumper(gce, !i, j);
		}
	smi_debug_dump_status(gce);

	for (i = 0; i < SMI_LARB_NUM; i++)
		SMIWRN(gce,
			"LARB%u=%u/%u busy with clk:%d, COMMON=%u/%u busy with clk:%d\n",
			i, busy[i], time, ATOMR_CLK(i),
			busy[SMI_LARB_NUM], time, ATOMR_CLK(SMI_LARB_NUM));
	return 0;
}
EXPORT_SYMBOL_GPL(smi_debug_bus_hang_detect);

static inline void smi_larb_port_set(const struct mtk_smi_dev *smi)
{
	s32 i;

	if (!smi || !smi->dev || smi->id >= SMI_LARB_NUM)
		return;

	for (i = smi_larb_cmd_gp_en_port[smi->id][0];
		i < smi_larb_cmd_gp_en_port[smi->id][1]; i++)
		writel(readl(smi->base + SMI_LARB_NON_SEC_CON(i)) | 0x2,
			smi->base + SMI_LARB_NON_SEC_CON(i));

	for (i = smi_larb_bw_thrt_en_port[smi->id][0];
		i < smi_larb_bw_thrt_en_port[smi->id][1]; i++)
		writel(readl(smi->base + SMI_LARB_NON_SEC_CON(i)) | 0x8,
			smi->base + SMI_LARB_NON_SEC_CON(i));
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
		SMIDBG("ioctl=%s:%s, curr=%s(%d), SMI_SCEN=%d [same as prev]\n",
			smi_bwc_scen_name_get(conf->scen),
			conf->b_on ? "ON" : "OFF",
			smi_bwc_scen_name_get(smi_drv.scen),
			smi_drv.scen, smi_scen);
		return 0;
	}
	for (i = 0; i <= SMI_LARB_NUM; i++) {
		mtk_smi_conf_set(smi_dev[i], smi_scen);
		smi_larb_port_set(smi_dev[i]);
	}
	SMIDBG("ioctl=%s:%s, curr=%s(%d), SMI_SCEN=%d\n",
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
#ifdef MMDVFS_HOOK
	case MTK_IOC_SMI_BWC_INFO_SET:
	{
		ret = set_mm_info_ioctl_wrapper(file, cmd, arg);
		break;
	}
	case MTK_IOC_SMI_BWC_INFO_GET:
	{
		ret = get_mm_info_ioctl_wrapper(file, cmd, arg);
		break;
	}
	case MTK_IOC_MMDVFS_CMD:
	{
		struct MTK_MMDVFS_CMD mmdvfs_cmd;

		if (copy_from_user(&mmdvfs_cmd,
			(void *)arg, sizeof(struct MTK_MMDVFS_CMD)))
			return -EFAULT;


		mmdvfs_handle_cmd(&mmdvfs_cmd);

		if (copy_to_user((void *)arg,
			(void *)&mmdvfs_cmd, sizeof(struct MTK_MMDVFS_CMD)))
			return -EFAULT;
		break;
	}
#endif
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

static inline void smi_subsys_sspm_ipi(const bool ena, const u32 subsys)
{
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SSPM_SUPPORT) && IS_ENABLED(SMI_SSPM)
	struct smi_ipi_data_s ipi_data;
	s32 ackdata;

	spin_lock(&(smi_drv.lock));
	smi_subsys_on = ena ?
		(smi_subsys_on | subsys) : (smi_subsys_on & ~subsys);
	spin_unlock(&(smi_drv.lock));

	ipi_data.cmd = SMI_IPI_ENABLE;
	ipi_data.u.logger.enable = smi_subsys_on;
	do {
		sspm_ipi_send_sync(IPI_ID_SMI, IPI_OPT_POLLING, &ipi_data,
			sizeof(ipi_data) / MBOX_SLOT_SIZE, &ackdata, 1);
	} while (ackdata);
#endif
}

static void smi_subsys_after_on(enum subsys_id sys)
{
	u32 subsys = smi_subsys_to_larbs[sys];
	u32 smi_scen = smi_scen_map[smi_drv.scen];
	s32 i;

	if (!subsys)
		return;
#if IS_ENABLED(CONFIG_MMPROFILE)
	mmprofile_log(smi_mmp_event[sys], MMPROFILE_FLAG_START);
#endif
	for (i = SMI_LARB_NUM; i >= 0; i--)
		if (subsys & (1 << i)) {
			smi_clk_record(i, true, NULL);
			mtk_smi_clk_enable(smi_dev[i]);
			mtk_smi_conf_set(smi_dev[i], smi_scen);
			smi_larb_port_set(smi_dev[i]);
		}
	smi_subsys_sspm_ipi(true, subsys);
}

static void smi_subsys_before_off(enum subsys_id sys)
{
	u32 subsys = smi_subsys_to_larbs[sys];
	s32 i;

	if (!subsys)
		return;

	smi_subsys_sspm_ipi(false, subsys);
	for (i = 0; i <= SMI_LARB_NUM; i++)
		if (subsys & (1 << i)) {
			if (sys != SYS_DIS || !(smi_mm_first & subsys))
				mtk_smi_clk_disable(smi_dev[i]);
			smi_clk_record(i, false, NULL);
		}
	if (sys == SYS_DIS && (smi_mm_first & subsys))
		smi_mm_first &= ~subsys;
#if IS_ENABLED(CONFIG_MMPROFILE)
	mmprofile_log(smi_mmp_event[sys], MMPROFILE_FLAG_END);
#endif
}

static struct pg_callbacks smi_clk_subsys_handle = {
	.after_on = smi_subsys_after_on,
	.before_off = smi_subsys_before_off,
};

static s32 smi_conf_get(const u32 id)
{
	if (id > SMI_LARB_NUM) {
		SMIDBG("Invalid id:%u, SMI_LARB_NUM=%u\n", id, SMI_LARB_NUM);
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
	struct resource		res;
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

	/* driver */
	spin_lock_init(&(smi_drv.lock));
	smi_drv.scen = SMI_BWC_SCEN_NORMAL;
	memset(&smi_drv.table, 0, sizeof(smi_drv.table));
	memset(&smi_record, 0, sizeof(smi_record));
	smi_drv.table[smi_drv.scen] += 1;

	/* init */
	spin_lock(&(smi_drv.lock));
	smi_subsys_on = smi_subsys_to_larbs[SYS_DIS];
	spin_unlock(&(smi_drv.lock));
	for (i = SMI_LARB_NUM; i >= 0; i--) {
		smi_conf_get(i);
		if (smi_subsys_on & (1 << i)) {
			smi_bus_prepare_enable(i, DEV_NAME);
			mtk_smi_conf_set(smi_dev[i], smi_drv.scen);
			smi_larb_port_set(smi_dev[i]);
		}
	}
	smi_mm_first = ~0;

	/* mmsys */
	of_node = of_parse_phandle(
		smi_dev[SMI_LARB_NUM]->dev->of_node, "mmsys_config", 0);
#ifdef MMDVFS_HOOK
	mmdvfs_init();
	mmdvfs_clks_init(smi_dev[SMI_LARB_NUM]->dev->of_node);
#endif
	smi_mmsys_base = (void *)of_iomap(of_node, 0);
	if (!smi_mmsys_base) {
		SMIERR("Unable to parse or iomap mmsys_config\n");
		return -ENOMEM;
	}
	if (of_address_to_resource(of_node, 0, &res))
		return -EINVAL;
	SMIWRN(0, "MMSYS base: VA=%p, PA=%pa\n", smi_mmsys_base, &res.start);
	of_node_put(of_node);

	smi_debug_dump_status(false);
	register_pg_callback(&smi_clk_subsys_handle);
	return 0;
}
EXPORT_SYMBOL_GPL(smi_register);

static int smi_dram_open(struct inode *node, struct file *filp)
{
	return nonseekable_open(node, filp);
}

static ssize_t
smi_dram_read(struct file *filp, char __user *buf, size_t len, loff_t *pos)
{
	return !smi_dram.virt ? 0 : simple_read_from_buffer(buf, len, pos,
		smi_dram.virt, smi_dram.size);
}

static const struct file_operations smi_dram_file_opers = {
	.owner = THIS_MODULE,
	.open = smi_dram_open,
	.read = smi_dram_read,
};

static inline void smi_mmp_init(void)
{
#if IS_ENABLED(CONFIG_MMPROFILE)
	s32 i;

	mmp_event smi_mmp;

	mmprofile_enable(1);
	smi_mmp = mmprofile_register_event(MMP_ROOT_EVENT, DEV_NAME);
	for (i = 0; i < NR_SYSS; i++)
		if (smi_mmp_name[i])
			smi_mmp_event[i] =
			mmprofile_register_event(smi_mmp, smi_mmp_name[i]);
	mmprofile_enable_event_recursive(smi_mmp, 1);
	mmprofile_start(1);
#endif
}

static inline void smi_dram_init(void)
{
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SSPM_SUPPORT) && IS_ENABLED(SMI_SSPM)
	phys_addr_t phys = sspm_reserve_mem_get_phys(SMI_MEM_ID);
	struct smi_ipi_data_s ipi_data;
	s32 ackdata;

	smi_dram.size = sspm_reserve_mem_get_size(SMI_MEM_ID);
	smi_dram.virt = ioremap_wc(phys, smi_dram.size);
#if 0
	if (IS_ERR(smi_dram.virt))
		SMIERR("ioremap_wc phys=%pa failed: %ld\n",
			&phys, PTR_ERR(smi_dram.virt));
#endif
	ipi_data.cmd = SMI_IPI_INIT;
	ipi_data.u.ctrl.phys = phys;
	ipi_data.u.ctrl.size = smi_dram.size;
	sspm_ipi_send_sync(IPI_ID_SMI, IPI_OPT_POLLING,
		&ipi_data, sizeof(ipi_data) / MBOX_SLOT_SIZE, &ackdata, 1);

#if IS_ENABLED(CONFIG_MTK_ENG_BUILD)
	smi_dram.dump = 1;
#endif
	ipi_data.cmd = SMI_IPI_ENABLE;
	ipi_data.u.logger.enable = (smi_dram.dump << 31) | smi_subsys_on;
	sspm_ipi_send_sync(IPI_ID_SMI, IPI_OPT_POLLING,
		&ipi_data, sizeof(ipi_data) / MBOX_SLOT_SIZE, &ackdata, 1);
#endif
	smi_dram.node = debugfs_create_file(
		"smi_mon", 0444, NULL, (void *)0, &smi_dram_file_opers);
	if (IS_ERR(smi_dram.node))
		SMIERR("debugfs_create_file failed: %ld\n",
			PTR_ERR(smi_dram.node));
}

static s32 __init smi_late_init(void)
{
	s32 i;

	smi_mmp_init();
	smi_dram_init();
	for (i = 0; i <= SMI_LARB_NUM; i++)
		if (smi_subsys_on & (1 << i))
			smi_bus_disable_unprepare(i, DEV_NAME);
	smi_reg_first = true;
	return 0;
}
late_initcall(smi_late_init);

int smi_dram_dump_get(char *buf, const struct kernel_param *kp)
{
	s32 pos = 0;

	pos += snprintf(buf + pos, PAGE_SIZE - pos, "dump=%u\n", smi_dram.dump);
	SMIDBG("dump=%u\n", smi_dram.dump);
	return pos;
}

int smi_dram_dump_set(const char *val, const struct kernel_param *kp)
{
	s32 arg, ret;

	ret = kstrtoint(val, 0, &arg);
	if (ret)
		SMIDBG("Invalid val: %s, ret=%d\n", val, ret);
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SSPM_SUPPORT) && IS_ENABLED(SMI_SSPM)
	else if (arg && !smi_dram.dump) {
		struct smi_ipi_data_s ipi_data;

		smi_dram.dump = 1;
		ipi_data.cmd = SMI_IPI_ENABLE;
		ipi_data.u.logger.enable =
			(smi_dram.dump << 31) | smi_subsys_on;
		sspm_ipi_send_sync(IPI_ID_SMI, IPI_OPT_WAIT,
			&ipi_data, sizeof(ipi_data) / MBOX_SLOT_SIZE, &ret, 1);
	}
#endif
	SMIDBG("arg=%d, dump=%u\n", arg, smi_dram.dump);
	return ret;
}

static struct kernel_param_ops smi_dram_dump_ops = {
	.get = smi_dram_dump_get,
	.set = smi_dram_dump_set,
};

module_param_cb(smi_dram_dump, &smi_dram_dump_ops, NULL, 0644);
