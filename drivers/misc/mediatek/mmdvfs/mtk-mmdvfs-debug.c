// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

#include <dt-bindings/clock/mmdvfs-clk.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <linux/regulator/consumer.h>
#include <linux/soc/mediatek/mtk_mmdvfs.h>
#include <soc/mediatek/mmdvfs_v3.h>
#include <mt-plat/mrdump.h>

#include "mtk-mmdvfs-v3-memory.h"
#include "mtk-mmdvfs-ftrace.h"

#define MMDVFS_DBG_VER1	BIT(0)
#define MMDVFS_DBG_VER3	BIT(1)

#define MMDVFS_DBG(fmt, args...) \
	pr_notice("[mmdvfs_dbg][dbg]%s: "fmt"\n", __func__, ##args)

struct mmdvfs_record {
	u64 sec;
	u64 nsec;
	u8 opp;
};

struct mmdvfs_debug {
	struct device *dev;
	struct proc_dir_entry *proc;
	u32 debug_version;

	/* MMDVFS_DBG_VER1 */
	struct regulator *reg;
	u32 reg_cnt_vol;
	u32 force_step0;
	u32 release_step0;

	spinlock_t lock;
	u8 rec_cnt;
	struct mmdvfs_record rec[MEM_REC_CNT_MAX];

	/* MMDVFS_DBG_VER3 */
	u32 use_v3_pwr;
};

static struct mmdvfs_debug *g_mmdvfs;
static bool ftrace_v1_ena, ftrace_v3_ena;
static bool mmdvfs_v3_debug_init_done;

void mtk_mmdvfs_debug_release_step0(void)
{
	if (!g_mmdvfs || !g_mmdvfs->release_step0)
		return;

	if (!IS_ERR_OR_NULL(g_mmdvfs->reg))
		regulator_set_voltage(g_mmdvfs->reg, 0, INT_MAX);
}
EXPORT_SYMBOL_GPL(mtk_mmdvfs_debug_release_step0);

static int mmdvfs_debug_set_force_step(const char *val,
	const struct kernel_param *kp)
{
	u8 idx = 0, opp = 0;
	int ret;

	ret = sscanf(val, "%hhu %hhu", &idx, &opp);
	if (ret != 2 || idx >= PWR_MMDVFS_NUM) {
		MMDVFS_DBG("failed:%d idx:%hhu opp:%hhu", ret, idx, opp);
		return -EINVAL;
	}

	if (idx == PWR_MMDVFS_VCORE && (!g_mmdvfs->debug_version ||
		g_mmdvfs->debug_version & MMDVFS_DBG_VER1))
		mmdvfs_set_force_step(opp);

	if (g_mmdvfs->debug_version & MMDVFS_DBG_VER3)
		mtk_mmdvfs_v3_set_force_step(idx, opp);

	return 0;
}

static struct kernel_param_ops mmdvfs_debug_set_force_step_ops = {
	.set = mmdvfs_debug_set_force_step,
};
module_param_cb(force_step, &mmdvfs_debug_set_force_step_ops, NULL, 0644);
MODULE_PARM_DESC(force_step, "force mmdvfs to specified step");

static int mmdvfs_debug_set_vote_step(const char *val,
	const struct kernel_param *kp)
{
	u8 idx = 0, opp = 0;
	int ret;

	ret = sscanf(val, "%hhu %hhu", &idx, &opp);
	if (ret != 2 || idx >= PWR_MMDVFS_NUM) {
		MMDVFS_DBG("failed:%d idx:%hhu opp:%hhu", ret, idx, opp);
		return -EINVAL;
	}

	if (idx == PWR_MMDVFS_VCORE && (!g_mmdvfs->debug_version ||
		g_mmdvfs->debug_version & MMDVFS_DBG_VER1))
		mmdvfs_set_vote_step(opp);

	if (g_mmdvfs->debug_version & MMDVFS_DBG_VER3)
		mtk_mmdvfs_v3_set_vote_step(idx, opp);

	return 0;
}

static struct kernel_param_ops mmdvfs_debug_set_vote_step_ops = {
	.set = mmdvfs_debug_set_vote_step,
};
module_param_cb(vote_step, &mmdvfs_debug_set_vote_step_ops, NULL, 0644);
MODULE_PARM_DESC(vote_step, "vote mmdvfs to specified step");

static void mmdvfs_debug_record_opp(const u8 opp)
{
	struct mmdvfs_record *rec;
	unsigned long flags;

	if (!g_mmdvfs)
		return;

	spin_lock_irqsave(&g_mmdvfs->lock, flags);

	rec = &g_mmdvfs->rec[g_mmdvfs->rec_cnt];
	rec->sec = sched_clock();
	rec->nsec = do_div(rec->sec, 1000000000);
	rec->opp = opp;

	g_mmdvfs->rec_cnt =
		(g_mmdvfs->rec_cnt + 1) % ARRAY_SIZE(g_mmdvfs->rec);

	spin_unlock_irqrestore(&g_mmdvfs->lock, flags);
}

static int mmdvfs_debug_opp_show(struct seq_file *file, void *data)
{
	unsigned long flags;
	u32 i, j;

	/* MMDVFS_DBG_VER1 */
	seq_puts(file, "VER1: mux controlled by vcore regulator:\n");

	spin_lock_irqsave(&g_mmdvfs->lock, flags);

	if (g_mmdvfs->rec[g_mmdvfs->rec_cnt].sec)
		for (i = g_mmdvfs->rec_cnt; i < ARRAY_SIZE(g_mmdvfs->rec); i++)
			seq_printf(file, "[%5llu.%06llu] opp:%u\n",
				g_mmdvfs->rec[i].sec, g_mmdvfs->rec[i].nsec,
				g_mmdvfs->rec[i].opp);

	for (i = 0; i < g_mmdvfs->rec_cnt; i++)
		seq_printf(file, "[%5llu.%06llu] opp:%u\n",
			g_mmdvfs->rec[i].sec, g_mmdvfs->rec[i].nsec,
			g_mmdvfs->rec[i].opp);

	spin_unlock_irqrestore(&g_mmdvfs->lock, flags);

	/* MMDVFS_DBG_VER3 */
	if (!MEM_BASE)
		return 0;

	seq_puts(file, "VER3: mux controlled by vcp:\n");

	// power opp
	i = readl(MEM_REC_PWR_CNT) % MEM_REC_CNT_MAX;
	if (readl(MEM_REC_PWR_SEC(i)))
		for (j = i; j < MEM_REC_CNT_MAX; j++)
			seq_printf(file, "[%5u.%3u] pwr:%u opp:%u\n",
				readl(MEM_REC_PWR_SEC(j)), readl(MEM_REC_PWR_NSEC(j)),
				readl(MEM_REC_PWR_ID(j)), readl(MEM_REC_PWR_OPP(j)));

	for (j = 0; j < i; j++)
		seq_printf(file, "[%5u.%3u] pwr:%u opp:%u\n",
			readl(MEM_REC_PWR_SEC(j)), readl(MEM_REC_PWR_NSEC(j)),
			readl(MEM_REC_PWR_ID(j)), readl(MEM_REC_PWR_OPP(j)));

	// user opp
	i = readl(MEM_REC_USR_CNT) % MEM_REC_CNT_MAX;
	if (readl(MEM_REC_USR_SEC(i)))
		for (j = i; j < MEM_REC_CNT_MAX; j++)
			seq_printf(file, "[%5u.%3u] pwr:%u usr:%u opp:%u\n",
				readl(MEM_REC_USR_SEC(j)), readl(MEM_REC_USR_NSEC(j)),
				readl(MEM_REC_USR_PWR(j)),
				readl(MEM_REC_USR_ID(j)), readl(MEM_REC_USR_OPP(j)));

	for (j = 0; j < i; j++)
		seq_printf(file, "[%5u.%3u] pwr:%u usr:%u opp:%u\n",
			readl(MEM_REC_USR_SEC(j)), readl(MEM_REC_USR_NSEC(j)),
			readl(MEM_REC_USR_PWR(j)),
			readl(MEM_REC_USR_ID(j)), readl(MEM_REC_USR_OPP(j)));

	return 0;
}

static int mmdvfs_debug_opp_open(struct inode *inode, struct file *file)
{
	return single_open(file, mmdvfs_debug_opp_show, inode->i_private);
}

static const struct proc_ops mmdvfs_debug_opp_fops = {
	.proc_open = mmdvfs_debug_opp_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

bool mtk_is_mmdvfs_v3_debug_init_done(void)
{
	return mmdvfs_v3_debug_init_done;
}
EXPORT_SYMBOL_GPL(mtk_is_mmdvfs_v3_debug_init_done);

static int mmdvfs_v3_debug_thread(void *data)
{
	phys_addr_t pa = 0ULL;
	unsigned long va;
	int ret = 0, retry = 0;

	while (!mtk_is_mmdvfs_init_done()) {
		if (++retry > 100) {
			MMDVFS_DBG("mmdvfs_v3 init not ready");
			goto init_done;
		}
		ssleep(2);
	}

	va = (unsigned long)(unsigned long *)mtk_mmdvfs_vcp_get_base(&pa);
	if (va && pa) {
		ret = mrdump_mini_add_extra_file(va, pa, PAGE_SIZE, "MMDVFS_OPP");
		if (ret)
			MMDVFS_DBG("failed:%d va:%#lx pa:pa", ret, va, &pa);
	}

	if (g_mmdvfs->use_v3_pwr & (1 << PWR_MMDVFS_VCORE))
		mtk_mmdvfs_v3_set_vote_step(PWR_MMDVFS_VCORE, g_mmdvfs->force_step0);

	if (g_mmdvfs->use_v3_pwr & (1 << PWR_MMDVFS_VMM))
		mtk_mmdvfs_v3_set_vote_step(PWR_MMDVFS_VMM, g_mmdvfs->force_step0);

	if (!g_mmdvfs->release_step0)
		goto init_done;

	if (g_mmdvfs->use_v3_pwr & (1 << PWR_MMDVFS_VCORE))
		mtk_mmdvfs_v3_set_vote_step(PWR_MMDVFS_VCORE, -1);

	if (g_mmdvfs->use_v3_pwr & (1 << PWR_MMDVFS_VMM))
		mtk_mmdvfs_v3_set_vote_step(PWR_MMDVFS_VMM, -1);

init_done:
	mmdvfs_v3_debug_init_done = true;
	return 0;
}

static int mmdvfs_v1_dbg_ftrace_thread(void *data)
{
	static u8 old_cnt;
	unsigned long flags;
	int ret = 0;
	s32 i;

	while (!kthread_should_stop()) {

		spin_lock_irqsave(&g_mmdvfs->lock, flags);

		if (g_mmdvfs->rec_cnt != old_cnt) {
			if (g_mmdvfs->rec_cnt > old_cnt) {
				for (i = old_cnt; i < g_mmdvfs->rec_cnt; i++)
					ftrace_record_opp_v1(1, g_mmdvfs->rec[i].opp);
			} else {
				for (i = old_cnt; i < ARRAY_SIZE(g_mmdvfs->rec); i++)
					ftrace_record_opp_v1(1, g_mmdvfs->rec[i].opp);

				for (i = 0; i < g_mmdvfs->rec_cnt; i++)
					ftrace_record_opp_v1(1, g_mmdvfs->rec[i].opp);
			}

			old_cnt = g_mmdvfs->rec_cnt;
		}

		spin_unlock_irqrestore(&g_mmdvfs->lock, flags);

		msleep(20);
	}

	ftrace_v1_ena = false;
	MMDVFS_DBG("kthread mmdvfs-dbg-ftrace-v1 end");
	return ret;
}

static int mmdvfs_v3_dbg_ftrace_thread(void *data)
{
	static u32 pwr_cnt, usr_cnt;
	int retry = 0;
	s32 i, j;

	while (!mtk_is_mmdvfs_init_done()) {
		if (++retry > 100) {
			ftrace_v3_ena = false;
			MMDVFS_DBG("mmdvfs_v3 init not ready");
			return 0;
		}
		ssleep(2);
	}

	if (!MEM_BASE) {
		ftrace_v3_ena = false;
		return 0;
	}

	while (!kthread_should_stop()) {
		// power opp
		i = readl(MEM_REC_PWR_CNT) % MEM_REC_CNT_MAX;
		if (pwr_cnt <= i) {
			for (j = pwr_cnt; j < i; j++)
				ftrace_pwr_opp_v3(readl(MEM_REC_PWR_ID(j)),
					readl(MEM_REC_PWR_OPP(j)));
		} else {
			for (j = pwr_cnt; j < MEM_REC_CNT_MAX; j++)
				ftrace_pwr_opp_v3(readl(MEM_REC_PWR_ID(j)),
					readl(MEM_REC_PWR_OPP(j)));
			for (j = 0; j < i; j++)
				ftrace_pwr_opp_v3(readl(MEM_REC_PWR_ID(j)),
					readl(MEM_REC_PWR_OPP(j)));
		}
		pwr_cnt = i;

		// user opp
		i = readl(MEM_REC_USR_CNT) % MEM_REC_CNT_MAX;
		if (usr_cnt <= i) {
			for (j = usr_cnt; j < i; j++) {
				if (readl(MEM_REC_USR_PWR(j)) == PWR_MMDVFS_VCORE)
					ftrace_user_opp_v3_vcore(readl(MEM_REC_USR_ID(j)),
						readl(MEM_REC_USR_OPP(j)));
				if (readl(MEM_REC_USR_PWR(j)) == PWR_MMDVFS_VMM)
					ftrace_user_opp_v3_vmm(readl(MEM_REC_USR_ID(j)),
						readl(MEM_REC_USR_OPP(j)));
			}
		} else {
			for (j = usr_cnt; j < MEM_REC_CNT_MAX; j++) {
				if (readl(MEM_REC_USR_PWR(j)) == PWR_MMDVFS_VCORE)
					ftrace_user_opp_v3_vcore(readl(MEM_REC_USR_ID(j)),
						readl(MEM_REC_USR_OPP(j)));
				if (readl(MEM_REC_USR_PWR(j)) == PWR_MMDVFS_VMM)
					ftrace_user_opp_v3_vmm(readl(MEM_REC_USR_ID(j)),
						readl(MEM_REC_USR_OPP(j)));
			}
			for (j = 0; j < i; j++) {
				if (readl(MEM_REC_USR_PWR(j)) == PWR_MMDVFS_VCORE)
					ftrace_user_opp_v3_vcore(readl(MEM_REC_USR_ID(j)),
						readl(MEM_REC_USR_OPP(j)));
				if (readl(MEM_REC_USR_PWR(j)) == PWR_MMDVFS_VMM)
					ftrace_user_opp_v3_vmm(readl(MEM_REC_USR_ID(j)),
						readl(MEM_REC_USR_OPP(j)));
			}
		}
		usr_cnt = i;
		msleep(20);
	}

	ftrace_v3_ena = false;
	MMDVFS_DBG("kthread mmdvfs-dbg-ftrace-v3 end");
	return 0;
}

static int mmdvfs_debug_set_ftrace(const char *val,
	const struct kernel_param *kp)
{
	static struct task_struct *kthr_v1, *kthr_v3;
	u32 ver = 0, ena = 0;
	int ret;

	ret = sscanf(val, "%hhu %hhu", &ver, &ena);
	if (ret != 2) {
		MMDVFS_DBG("failed:%d ver:%hhu ena:%hhu", ret, ver, ena);
		return -EINVAL;
	}

	if (ver & MMDVFS_DBG_VER1) {
		if (ena) {
			if (ftrace_v1_ena)
				MMDVFS_DBG("kthread mmdvfs-dbg-ftrace-v1 already created");
			else {
				kthr_v1 = kthread_run(
					mmdvfs_v1_dbg_ftrace_thread, NULL, "mmdvfs-dbg-ftrace-v1");
				if (IS_ERR(kthr_v1))
					MMDVFS_DBG("create kthread mmdvfs-dbg-ftrace-v1 failed");
				else
					ftrace_v1_ena = true;
			}
		} else {
			if (ftrace_v1_ena) {
				ret = kthread_stop(kthr_v1);
				if (!ret) {
					MMDVFS_DBG("stop kthread mmdvfs-dbg-ftrace-v1");
					ftrace_v1_ena = false;
				}
			}
		}
	}

	if (ver & MMDVFS_DBG_VER3) {
		if (ena) {
			if (ftrace_v3_ena)
				MMDVFS_DBG("kthread mmdvfs-dbg-ftrace-v3 already created");
			else {
				kthr_v3 = kthread_run(
					mmdvfs_v3_dbg_ftrace_thread, NULL, "mmdvfs-dbg-ftrace-v3");
				if (IS_ERR(kthr_v3))
					MMDVFS_DBG("create kthread mmdvfs-dbg-ftrace-v3 failed");
				else
					ftrace_v3_ena = true;
			}
		} else {
			if (ftrace_v3_ena) {
				ret = kthread_stop(kthr_v3);
				if (!ret) {
					MMDVFS_DBG("stop kthread mmdvfs-dbg-ftrace-v3");
					ftrace_v3_ena = false;
				}
			}
		}
	}

	return 0;
}

static struct kernel_param_ops mmdvfs_debug_set_ftrace_ops = {
	.set = mmdvfs_debug_set_ftrace,
};
module_param_cb(ftrace, &mmdvfs_debug_set_ftrace_ops, NULL, 0644);
MODULE_PARM_DESC(ftrace, "mmdvfs ftrace log");

static int mmdvfs_debug_probe(struct platform_device *pdev)
{
	struct proc_dir_entry *dir, *proc;
	struct task_struct *kthr;
	struct regulator *reg;
	int ret;

	g_mmdvfs = kzalloc(sizeof(*g_mmdvfs), GFP_KERNEL);
	if (!g_mmdvfs) {
		MMDVFS_DBG("kzalloc: g_mmdvfs no memory");
		return -ENOMEM;
	}
	g_mmdvfs->dev = &pdev->dev;

	dir = proc_mkdir("mmdvfs", NULL);
	if (IS_ERR_OR_NULL(dir))
		MMDVFS_DBG("proc_mkdir failed:%ld", PTR_ERR(dir));

	proc = proc_create("mmdvfs_opp", 0444, dir, &mmdvfs_debug_opp_fops);
	if (IS_ERR_OR_NULL(proc))
		MMDVFS_DBG("proc_create failed:%ld", PTR_ERR(proc));
	else
		g_mmdvfs->proc = proc;

	ret = of_property_read_u32(g_mmdvfs->dev->of_node,
		"debug-version", &g_mmdvfs->debug_version);
	if (ret)
		MMDVFS_DBG("debug_version:%u failed:%d",
			g_mmdvfs->debug_version, ret);

	/* MMDVFS_DBG_VER1 */
	reg = devm_regulator_get(g_mmdvfs->dev, "dvfsrc-vcore");
	if (IS_ERR_OR_NULL(reg)) {
		MMDVFS_DBG("devm_regulator_get failed:%d", PTR_ERR(reg));
		return PTR_ERR(reg);
	}
	g_mmdvfs->reg = reg;

	ret = regulator_count_voltages(reg);
	if (ret < 0) {
		MMDVFS_DBG("regulator_count_voltages failed:%d", ret);
		return ret;
	}
	g_mmdvfs->reg_cnt_vol = (u32)ret;

	ret = of_property_read_u32(g_mmdvfs->dev->of_node,
		"force-step0", &g_mmdvfs->force_step0);
	if (ret) {
		MMDVFS_DBG("force_step0:%u failed:%d",
			g_mmdvfs->force_step0, ret);
		return ret;
	}

	if (g_mmdvfs->force_step0 >= g_mmdvfs->reg_cnt_vol) {
		MMDVFS_DBG("force_step0:%u cannot larger reg_cnt_vol:%u",
			g_mmdvfs->force_step0, g_mmdvfs->reg_cnt_vol);
		return -EINVAL;
	}

	spin_lock_init(&g_mmdvfs->lock);
	mmdvfs_debug_record_opp_set_fp(mmdvfs_debug_record_opp);

	ret = regulator_list_voltage(
		reg, g_mmdvfs->reg_cnt_vol - 1 - g_mmdvfs->force_step0);
	regulator_set_voltage(reg, ret, INT_MAX);

	ret = of_property_read_u32(g_mmdvfs->dev->of_node,
		"release-step0", &g_mmdvfs->release_step0);
	if (ret)
		MMDVFS_DBG("release_step0:%u failed:%d",
			g_mmdvfs->release_step0, ret);

	/* MMDVFS_DBG_VER3 */
	ret = of_property_read_u32(g_mmdvfs->dev->of_node,
		"use-v3-pwr", &g_mmdvfs->use_v3_pwr);

	if (g_mmdvfs->use_v3_pwr)
		kthr = kthread_run(
			mmdvfs_v3_debug_thread, NULL, "mmdvfs-dbg-vcp");

	return 0;
}

static int mmdvfs_debug_remove(struct platform_device *pdev)
{
	devm_regulator_put(g_mmdvfs->reg);
	kfree(g_mmdvfs);
	return 0;
}

static const struct of_device_id of_mmdvfs_debug_match_tbl[] = {
	{
		.compatible = "mediatek,mmdvfs-debug",
	},
	{}
};

static struct platform_driver mmdvfs_debug_drv = {
	.probe = mmdvfs_debug_probe,
	.remove = mmdvfs_debug_remove,
	.driver = {
		.name = "mtk-mmdvfs-debug",
		.of_match_table = of_mmdvfs_debug_match_tbl,
	},
};

static int __init mmdvfs_debug_init(void)
{
	int ret;

	ret = platform_driver_register(&mmdvfs_debug_drv);
	if (ret)
		MMDVFS_DBG("failed:%d", ret);

	return ret;
}

static void __exit mmdvfs_debug_exit(void)
{
	platform_driver_unregister(&mmdvfs_debug_drv);
}

module_init(mmdvfs_debug_init);
module_exit(mmdvfs_debug_exit);
MODULE_DESCRIPTION("MMDVFS Debug Driver");
MODULE_AUTHOR("Anthony Huang<anthony.huang@mediatek.com>");
MODULE_LICENSE("GPL");

