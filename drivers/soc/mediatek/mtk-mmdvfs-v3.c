// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#include <linux/clk.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/sched/clock.h>
#include <linux/slab.h>

#include "clk-mtk.h"
//#include "clk-mux.h"
#include "mtk-mmdvfs-v3.h"
#include "vcp_status.h"

static u8 mmdvfs_clk_num;
static struct mtk_mmdvfs_clk *mtk_mmdvfs_clks;

static u8 mmdvfs_pwr_opp[PWR_MMDVFS_NUM];
static struct clk *mmdvfs_pwr_clk[PWR_MMDVFS_NUM];

static u32 mmdvfs_vcp_ipi_data;
static DEFINE_MUTEX(mmdvfs_vcp_ipi_mutex);

static log_level;
static bool mmdvfs_init_done;

enum mmdvfs_log_level {
	log_ipi,
	log_ccf_cb,
	log_vcp,
};

static inline struct mtk_mmdvfs_clk *to_mtk_mmdvfs_clk(struct clk_hw *hw)
{
	return container_of(hw, struct mtk_mmdvfs_clk, clk_hw);
}

static int mmdvfs_vcp_is_ready(void)
{
	int ret = 0;

	while (!is_vcp_ready_ex(VCP_A_ID)) {
		ret += 1;
		if (log_level & 1 << log_vcp)
			MMDVFS_DBG("retry:%d VCP_A_ID:%d not ready", ret, VCP_A_ID);
		if (ret > 100)
			return 0;
		msleep(50);
	}
	return 1;
}

static int mmdvfs_vcp_ipi_send(const u8 func, const u8 idx, const u8 opp,
	const u8 ack) // ap > vcp
{
	struct mmdvfs_ipi_data slot = {func, idx, opp, ack};
	int ret;

	if (!mmdvfs_vcp_is_ready()) {
		MMDVFS_ERR("vcp is not ready");
		return -ETIMEDOUT;
	}

	mutex_lock(&mmdvfs_vcp_ipi_mutex);
	mmdvfs_vcp_ipi_data =
		(func << 0) | (idx << 8) | (opp << 16) | (ack << 24);

	ret = mtk_ipi_send(vcp_get_ipidev(), IPI_OUT_MMDVFS, IPI_SEND_WAIT,
		&slot, PIN_OUT_SIZE_MMDVFS, IPI_TIMEOUT_MS);

	if (ret != IPI_ACTION_DONE) {
		MMDVFS_ERR("mtk_ipi_send failed:%d slot:%#x data:%#x",
			ret, slot, mmdvfs_vcp_ipi_data);

		mutex_unlock(&mmdvfs_vcp_ipi_mutex);
		return ret;
	}

	while ((mmdvfs_vcp_ipi_data & 0xff) == func)
		udelay(100);

	ret = mmdvfs_vcp_ipi_data;
	mutex_unlock(&mmdvfs_vcp_ipi_mutex);
	return ret;
}

static int mmdvfs_vcp_ipi_cb(unsigned int ipi_id, void *prdata, void *data,
	unsigned int len) // vcp > ap
{
	struct mmdvfs_ipi_data slot;

	if (ipi_id != IPI_IN_MMDVFS || !data)
		return 0;

	slot = *(struct mmdvfs_ipi_data *)data;

	if (log_level & 1 << log_ipi)
		MMDVFS_DBG("ipi_id:%u slot:%#x func:%hhu idx:%hhu opp:%hhu ack:%hhu",
			ipi_id, slot, slot.func, slot.idx, slot.opp, slot.ack);

	return 0;
}

static int mtk_mmdvfs_set_rate(struct clk_hw *hw, unsigned long rate,
		unsigned long parent_rate)
{
	struct mtk_mmdvfs_clk *mmdvfs_clk = to_mtk_mmdvfs_clk(hw);
	u8 i, opp, level, pwr_opp = MAX_OPP, user_opp = MAX_OPP;
	struct mmdvfs_ipi_data slot;
	int ret;

	for (i = 0; i < mmdvfs_clk->freq_num; i++)
		if (rate <= mmdvfs_clk->freqs[i])
			break;

	level = (i == mmdvfs_clk->freq_num) ? (i-1) : i;
	opp = (mmdvfs_clk->freq_num - level - 1);
	if (log_level & 1 << log_ccf_cb)
		MMDVFS_DBG("user:%u freq=%lu old_opp=%d new_opp=%d\n",
			mmdvfs_clk->user_id, rate, mmdvfs_clk->opp, opp);
	if (mmdvfs_clk->opp == opp)
		return 0;
	mmdvfs_clk->opp = opp;

	for (i = 0; i < mmdvfs_clk_num; i++) {
		if (mmdvfs_clk->pwr_id == mtk_mmdvfs_clks[i].pwr_id
			&& mtk_mmdvfs_clks[i].opp < pwr_opp)
			pwr_opp = mtk_mmdvfs_clks[i].opp;
	}

	/* Choose max step among all users of special independence */
	if (mmdvfs_clk->spec_type == SPEC_MMDVFS_ALONE) {
		for (i = 0; i < mmdvfs_clk_num; i++) {
			if (mmdvfs_clk->user_id == mtk_mmdvfs_clks[i].user_id
				&& mtk_mmdvfs_clks[i].opp < user_opp)
				user_opp = mtk_mmdvfs_clks[i].opp;
		}
		ret = mmdvfs_vcp_ipi_send(FUNC_SET_OPP, mmdvfs_clk->user_id, user_opp, MAX_OPP);
	} else {
		if (pwr_opp == mmdvfs_pwr_opp[mmdvfs_clk->pwr_id])
			return 0;
		ret = mmdvfs_vcp_ipi_send(FUNC_SET_OPP, mmdvfs_clk->user_id, pwr_opp, MAX_OPP);
	}

	mmdvfs_pwr_opp[mmdvfs_clk->pwr_id] = pwr_opp;

	slot = *(struct mmdvfs_ipi_data *)(u32 *)&ret;
	if (log_level & 1 << log_ipi)
		MMDVFS_DBG("ipi:%d slot:%#x idx:%hhu opp:%hhu", ret, slot, slot.idx, slot.ack);
	return 0;
}

static long mtk_mmdvfs_round_rate(struct clk_hw *hw, unsigned long rate,
			       unsigned long *parent_rate)
{
	if (log_level & 1 << log_ccf_cb)
		MMDVFS_DBG("rate=%lu parent_rate=%lu\n", rate, *parent_rate);

	return rate;
}

static unsigned long mtk_mmdvfs_recalc_rate(struct clk_hw *hw,
					 unsigned long parent_rate)
{
	struct mtk_mmdvfs_clk *mmdvfs_clk = to_mtk_mmdvfs_clk(hw);
	unsigned long ret;
	u8 level;

	level = (mmdvfs_clk->opp == MAX_OPP) ? 0 : (mmdvfs_clk->freq_num - mmdvfs_clk->opp - 1);
	ret = mmdvfs_clk->freqs[level];
	if (log_level & 1 << log_ccf_cb)
		MMDVFS_DBG("parent_rate=%lu freq=%lu\n", parent_rate, ret);
	return ret;
}

static const struct clk_ops mtk_mmdvfs_req_ops = {
	.set_rate = mtk_mmdvfs_set_rate,
	.round_rate	= mtk_mmdvfs_round_rate,
	.recalc_rate	= mtk_mmdvfs_recalc_rate,
};

int mmdvfs_camera_notify(const bool enable)
{
	struct mmdvfs_ipi_data slot;
	int ret;

	ret = mmdvfs_vcp_ipi_send(FUNC_CAMERA_ON, enable, MAX_OPP, MAX_OPP);

	slot = *(struct mmdvfs_ipi_data *)(u32 *)&ret;
	MMDVFS_DBG("ipi:%d slot:%#x ena:%hhu", ret, slot, slot.ack);

	return 0;
}
EXPORT_SYMBOL(mmdvfs_camera_notify);

bool mtk_is_mmdvfs_init_done(void)
{
	return mmdvfs_init_done;
}
EXPORT_SYMBOL(mtk_is_mmdvfs_init_done);

int mmdvfs_set_force_step(const char *val, const struct kernel_param *kp)
{
	struct mmdvfs_ipi_data slot;
	u16 idx = 0, opp = 0;
	int ret;

	ret = sscanf(val, "%hu %hu", &idx, &opp);
	if (ret != 2 || idx >= PWR_MMDVFS_NUM || opp >= MAX_OPP) {
		MMDVFS_ERR("failed:%d idx:%hu opp:%hu", ret, idx, opp);
		return ret;
	}

	ret = mmdvfs_vcp_ipi_send(FUNC_FORCE_OPP, idx, opp, MAX_OPP);

	slot = *(struct mmdvfs_ipi_data *)(u32 *)&ret;
	MMDVFS_DBG("ipi:%d slot:%#x idx:%hhu opp:%hhu",
		ret, slot, slot.idx, slot.ack);

	return 0;
}

static struct kernel_param_ops mmdvfs_set_force_step_ops = {
	.set = mmdvfs_set_force_step,
};
module_param_cb(force_step, &mmdvfs_set_force_step_ops, NULL, 0644);
MODULE_PARM_DESC(force_step, "force mmdvfs to specified step");

int mmdvfs_set_vote_step(const char *val, const struct kernel_param *kp)
{
	u16 idx = 0, opp = 0;
	u32 freq = 0;
	int ret, i;

	ret = sscanf(val, "%hu %hu", &idx, &opp);
	if (ret != 2 || idx >= PWR_MMDVFS_NUM || opp >= MAX_OPP) {
		MMDVFS_ERR("failed:%d idx:%hu opp:%hu", ret, idx, opp);
		return ret;
	}

	for (i = mmdvfs_clk_num - 1; i >= 0; i--)
		if (idx == mtk_mmdvfs_clks[i].pwr_id) {
			if (opp >= mtk_mmdvfs_clks[i].freq_num) {
				MMDVFS_ERR("invalid opp:%hu freq_num:%hhu",
					opp, mtk_mmdvfs_clks[i].freq_num);
				return opp;
			}

			freq = mtk_mmdvfs_clks[i].freqs[
				mtk_mmdvfs_clks[i].freq_num - 1 - opp];
			clk_set_rate(mmdvfs_pwr_clk[idx], freq);
			break;
		}

	MMDVFS_DBG("idx:%hu clk:%p opp:%hu i:%d freq_num:%hhu freq:%u", idx,
		mmdvfs_pwr_clk[idx], opp, i, mtk_mmdvfs_clks[i].freq_num, freq);

	return 0;
}

static struct kernel_param_ops mmdvfs_set_vote_step_ops = {
	.set = mmdvfs_set_vote_step,
};
module_param_cb(vote_step, &mmdvfs_set_vote_step_ops, NULL, 0644);
MODULE_PARM_DESC(vote_step, "vote mmdvfs to specified step");

int mmdvfs_dump_setting(char *buf, const struct kernel_param *kp)
{
	struct mmdvfs_ipi_data slot;
	int i, len = 0, ret;

	len += snprintf(buf + len, PAGE_SIZE - len, "user request opp");
	for (i = 0; i < USER_NUM; i += 3) {
		ret = mmdvfs_vcp_ipi_send(FUNC_GET_OPP, i, i + 1, i + 2);

		slot = *(struct mmdvfs_ipi_data *)(u32 *)&ret;
		MMDVFS_DBG("slot:%#x i:%d opp:%hhu %hhu %hhu",
			slot, i, slot.idx, slot.opp, slot.ack);

		if (i + 1 >= USER_NUM) {
			len += snprintf(buf + len, PAGE_SIZE - len,
				", %hhu", slot.idx);
			break;
		}

		if (i + 2 >= USER_NUM) {
			len += snprintf(buf + len, PAGE_SIZE - len,
				", %hhu, %hhu", slot.idx, slot.opp);
			break;
		}

		len += snprintf(buf + len, PAGE_SIZE - len,
			", %hhu, %hhu, %hhu", slot.idx, slot.opp, slot.ack);
	}
	len += snprintf(buf + len, PAGE_SIZE - len, "\n");

	if (len >= PAGE_SIZE) {
		len = PAGE_SIZE - 1;
		buf[len] = '\n';
	}

	return len;
}

static struct kernel_param_ops mmdvfs_dump_setting_ops = {
	.get = mmdvfs_dump_setting,
};
module_param_cb(dump_setting, &mmdvfs_dump_setting_ops, NULL, 0444);
MODULE_PARM_DESC(dump_setting, "dump mmdvfs current setting");

int mmdvfs_get_vcp_log(char *buf, const struct kernel_param *kp)
{
	struct mmdvfs_ipi_data slot;
	int len = 0, ret;

	ret = mmdvfs_vcp_ipi_send(FUNC_LOG, LOG_NUM, MAX_OPP, MAX_OPP);

	slot = *(struct mmdvfs_ipi_data *)(u32 *)&ret;
	len += snprintf(
		buf + len, PAGE_SIZE - len, "mmdvfs vcp log:%#x", slot.ack);

	return len;
}

int mmdvfs_set_vcp_log(const char *val, const struct kernel_param *kp)
{
	struct mmdvfs_ipi_data slot;
	u16 log = 0;
	int ret;

	ret = kstrtou16(val, 0, &log);
	if (ret || log >= LOG_NUM) {
		MMDVFS_ERR("failed:%d log:%hu", ret, log);
		return ret;
	}

	ret = mmdvfs_vcp_ipi_send(FUNC_LOG, log, MAX_OPP, MAX_OPP);

	slot = *(struct mmdvfs_ipi_data *)(u32 *)&ret;
	MMDVFS_DBG("ipi:%d slot:%#x log:%hu log:%#x", ret, slot, log, slot.ack);

	return 0;
}

static struct kernel_param_ops mmdvfs_set_vcp_log_ops = {
	.get = mmdvfs_get_vcp_log,
	.set = mmdvfs_set_vcp_log,
};
module_param_cb(vcp_log, &mmdvfs_set_vcp_log_ops, NULL, 0644);
MODULE_PARM_DESC(vcp_log, "mmdvfs vcp log");

module_param(log_level, uint, 0644);
MODULE_PARM_DESC(log_level, "mmdvfs log level");

static const struct of_device_id of_match_mmdvfs_v3[] = {
	{
		.compatible = "mediatek,mtk-mmdvfs-v3",
	}, {}
};

static int mmdvfs_vcp_init_thread(void *data)
{
	static struct mtk_ipi_device *vcp_ipi_dev;
	int ret;

	while (!mmup_enable_count()) {
		msleep(1000);
		MMDVFS_DBG("vcp is not powered on yet");
		vcp_register_feature_ex(MMDVFS_FEATURE_ID);
	}

	while (!mmdvfs_vcp_is_ready()) {
		MMDVFS_DBG("vcp is not ready yet");
		msleep(2000);
	}
	while (!(vcp_ipi_dev = vcp_get_ipidev())) {
		MMDVFS_DBG("cannot get vcp ipidev");
		msleep(1000);
	}

	ret = mtk_ipi_register(vcp_ipi_dev, IPI_IN_MMDVFS,
		mmdvfs_vcp_ipi_cb, NULL, &mmdvfs_vcp_ipi_data);
	if (ret)
		MMDVFS_DBG("mtk_ipi_register failed:%d ipi_id:%d", ret, IPI_IN_MMDVFS);

	mmdvfs_init_done = true;

	return 0;
}

static int mmdvfs_v3_probe(struct platform_device *pdev)
{
	const char *MMDVFS_CLK_NAMES = "mediatek,mmdvfs_clock_names";
	const char *MMDVFS_CLKS = "mediatek,mmdvfs_clocks";
	struct device_node *node = pdev->dev.of_node;
	struct clk_onecell_data *clk_data;
	struct task_struct *kthr;
	struct clk *clk;
	int i, ret;

	ret = of_property_count_strings(node, MMDVFS_CLK_NAMES);
	if (ret <= 0) {
		MMDVFS_ERR("%s invalid:%d", MMDVFS_CLK_NAMES, ret);
		return ret;
	}
	mmdvfs_clk_num = ret;

	mtk_mmdvfs_clks =
		kcalloc(mmdvfs_clk_num, sizeof(*mtk_mmdvfs_clks), GFP_KERNEL);
	if (!mtk_mmdvfs_clks) {
		MMDVFS_ERR("mtk_mmdvfs_clks without memory");
		return -ENOMEM;
	}

	clk_data = mtk_alloc_clk_data(mmdvfs_clk_num);
	if (!clk_data) {
		MMDVFS_ERR("allocate clk_data failed num:%hhu", mmdvfs_clk_num);
		return -ENOMEM;
	}

	for (i = 0; i < mmdvfs_clk_num; i++) {
		struct device_node *table, *opp = NULL;
		struct of_phandle_args spec;
		struct clk_init_data init = {};
		u8 idx = 0;

		of_property_read_string_index(
			node, MMDVFS_CLK_NAMES, i, &mtk_mmdvfs_clks[i].name);

		ret = of_parse_phandle_with_args(
			node, MMDVFS_CLKS, "#mmdvfs,clock-cells", i, &spec);
		if (ret) {
			MMDVFS_ERR("parse %s i:%d failed:%d",
				MMDVFS_CLKS, i, ret);
			return ret;
		}

		mtk_mmdvfs_clks[i].clk_id = spec.args[0];
		mtk_mmdvfs_clks[i].pwr_id = spec.args[1];
		mtk_mmdvfs_clks[i].user_id = spec.args[2];
		mtk_mmdvfs_clks[i].spec_type = spec.args[3];
		mtk_mmdvfs_clks[i].ipi_type = spec.args[4];
		table = of_find_node_by_phandle(spec.args[5]);
		of_node_put(spec.np);

		do {
			u64 freq;

			opp = of_get_next_available_child(table, opp);
			if (opp) {
				of_property_read_u64(opp, "opp-hz", &freq);
				mtk_mmdvfs_clks[i].freqs[idx] = freq;
				idx += 1;
			}
		} while (opp);
		of_node_put(table);

		mtk_mmdvfs_clks[i].opp = MAX_OPP;
		mtk_mmdvfs_clks[i].freq_num = idx;

		MMDVFS_DBG(
			"i:%d name:%s clk:%hhu pwr:%hhu user:%hhu ipi:%hhu spec:%hhu opp:%hhu freq:%hhu",
			i, mtk_mmdvfs_clks[i].name, mtk_mmdvfs_clks[i].clk_id,
			mtk_mmdvfs_clks[i].pwr_id, mtk_mmdvfs_clks[i].user_id,
			mtk_mmdvfs_clks[i].ipi_type,
			mtk_mmdvfs_clks[i].spec_type,
			mtk_mmdvfs_clks[i].opp, mtk_mmdvfs_clks[i].freq_num);

		if (!IS_ERR_OR_NULL(clk_data->clks[i]))
			continue;

		init.name = mtk_mmdvfs_clks[i].name;
		init.ops = &mtk_mmdvfs_req_ops;
		mtk_mmdvfs_clks[i].clk_hw.init = &init;

		clk = clk_register(NULL, &mtk_mmdvfs_clks[i].clk_hw);
		if (IS_ERR_OR_NULL(clk))
			MMDVFS_ERR("i:%d clk:%s register failed:%d",
				i, mtk_mmdvfs_clks[idx].name, PTR_ERR(clk));
		else
			clk_data->clks[i] = clk;
	}

	ret = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);
	if (ret) {
		MMDVFS_ERR("add clk provider failed:%d", ret);
		return ret;
	}

	for (i = 0; i < ARRAY_SIZE(mmdvfs_pwr_opp); i++) {
		mmdvfs_pwr_opp[i] = MAX_OPP;

		clk = of_clk_get(node, i);
		if (IS_ERR_OR_NULL(clk))
			MMDVFS_DBG("i:%d clk get failed:%d", i, PTR_ERR(clk));
		else
			mmdvfs_pwr_clk[i] = clk;
	}

	kthr = kthread_run(mmdvfs_vcp_init_thread, NULL, "mmdvfs-vcp");
	return ret;
}

static struct platform_driver clk_mmdvfs_drv = {
	.probe = mmdvfs_v3_probe,
	.driver = {
		.name = "mtk-mmdvfs-v3",
		.owner = THIS_MODULE,
		.of_match_table = of_match_mmdvfs_v3,
	},
};

static int __init clk_mmdvfs_init(void)
{
	return platform_driver_register(&clk_mmdvfs_drv);
}

static void __exit clk_mmdvfs_exit(void)
{
	platform_driver_unregister(&clk_mmdvfs_drv);
}

module_init(clk_mmdvfs_init);
module_exit(clk_mmdvfs_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek MMDVFS");
MODULE_AUTHOR("MediaTek Inc.");
