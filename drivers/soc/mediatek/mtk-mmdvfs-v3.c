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
//#include <dt-bindings/clock/mt6983-mmdvfs-clk.h>
#include "mtk-mmdvfs-v3.h"
#include "vcp_status.h"

static const char *MMDVFS_CLKS = "mediatek,mmdvfs_clocks";
static const char *MMDVFS_CLK_NAMES = "mediatek,mmdvfs_clk_names";

static struct mtk_mmdvfs_clk *mtk_mmdvfs_clks;
static u8 max_mmdvfs_num;
static u8 mmdvfs_pwr_opp[MAX_PWR_NUM];
static struct mtk_ipi_device *vcp_ipi_dev;
static u32 mmdvfs_vcp_ipi_data;
static DEFINE_MUTEX(mmdvfs_vcp_ipi_mutex);
static struct clk *vote_clk[POWER_NUM];
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

	for (i = 0; i < max_mmdvfs_num; i++) {
		if (mmdvfs_clk->pwr_id == mtk_mmdvfs_clks[i].pwr_id
			&& mtk_mmdvfs_clks[i].opp < pwr_opp)
			pwr_opp = mtk_mmdvfs_clks[i].opp;
	}

	/* Choose max step among all users of special independence */
	if (mmdvfs_clk->special_type == SPECIAL_INDEPENDENCE) {
		for (i = 0; i < max_mmdvfs_num; i++) {
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

static struct clk *mtk_mmdvfs_register_clk(u8 id, struct mtk_mmdvfs_clk *mmdvfs_clk)
{
	struct clk_init_data init = {};
	struct clk *clk;

	init.name = mmdvfs_clk->name;
	//init.flags = mux->flags | CLK_SET_RATE_PARENT;
	//init.parent_names = mux->parent_names;
	//init.num_parents = mux->num_parents;
	init.ops = &mtk_mmdvfs_req_ops;
	mmdvfs_clk->clk_hw.init = &init;
	//mmdvfs_clk->clk_id = id;

	clk = clk_register(NULL, &mmdvfs_clk->clk_hw);

	return clk;
}
//EXPORT_SYMBOL(mtk_clk_register_mux);

static int mtk_mmdvfs_register_clks(int num_clks,
			   struct clk_onecell_data *clk_data)
{
	struct clk *clk;
	int i;

	for (i = 0; i < num_clks; i++) {
		if (IS_ERR_OR_NULL(clk_data->clks[i])) {
			clk = mtk_mmdvfs_register_clk(i, &mtk_mmdvfs_clks[i]);
			if (IS_ERR(clk)) {
				MMDVFS_DBG("failed to register clk %s: %ld\n",
				       mtk_mmdvfs_clks[i].name, PTR_ERR(clk));
				continue;
			}

			clk_data->clks[i] = clk;
		}
	}
	return 0;
}
//EXPORT_SYMBOL(mtk_clk_register_mmdvfs);

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

	ret = sscanf(val, "%d %d", &idx, &opp);
	if (ret != 2 || idx >= POWER_NUM || opp >= MAX_OPP) {
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

	ret = sscanf(val, "%d %d", &idx, &opp);
	if (ret != 2 || idx >= POWER_NUM || opp >= MAX_OPP) {
		MMDVFS_ERR("failed:%d idx:%hu opp:%hu", ret, idx, opp);
		return ret;
	}

	for (i = max_mmdvfs_num; i >= 0; i--)
		if (idx == mtk_mmdvfs_clks[i].pwr_id) {
			if (opp >= mtk_mmdvfs_clks[i].freq_num) {
				MMDVFS_ERR("invalid opp:%hu freq_num:%hhu",
					opp, mtk_mmdvfs_clks[i].freq_num);
				return opp;
			}

			freq = mtk_mmdvfs_clks[i].freqs[
				mtk_mmdvfs_clks[i].freq_num - 1 - opp];
			clk_set_rate(vote_clk[idx], freq);
			break;
		}

	MMDVFS_DBG("idx:%hu clk:%p opp:%hu i:%d freq_num:%hhu freq:%u",
		idx, vote_clk[idx], opp, i, mtk_mmdvfs_clks[i].freq_num, freq);

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
	struct clk_onecell_data *clk_data;
	int ret = 0;
	struct device_node *node = pdev->dev.of_node;
	struct device_node *opp_table, *opp_node;
	struct task_struct *kthr;
	u32 opp_table_ph, value;
	u64 freq;
	u8 idx, opp_idx;
	int elem_num, i;

	elem_num = of_property_count_strings(node, MMDVFS_CLK_NAMES);

	if (elem_num <= 0) {
		MMDVFS_DBG("no clk definition found!\n");
		return 0;
	}

	mtk_mmdvfs_clks = kcalloc(elem_num, sizeof(*mtk_mmdvfs_clks), GFP_KERNEL);
	if (!mtk_mmdvfs_clks) {
		MMDVFS_DBG("no memory for mtk_mmdvfs_clks\n");
		return -ENOMEM;
	}

	for (idx = 0; idx < elem_num; idx++) {
		of_property_read_string_index(node, MMDVFS_CLK_NAMES, idx,
			&mtk_mmdvfs_clks[max_mmdvfs_num].name);

		mtk_mmdvfs_clks[max_mmdvfs_num].clk_id = max_mmdvfs_num;

		ret = of_property_read_u32_index(node, MMDVFS_CLKS,
				idx * ARG_NUM + ARG_PWR_ID, &value);
		if (ret) {
			MMDVFS_DBG("idx(%u) cannot get pwr_id\n", idx);
			return -EINVAL;
		}
		mtk_mmdvfs_clks[max_mmdvfs_num].pwr_id = value;

		ret = of_property_read_u32_index(node, MMDVFS_CLKS,
				idx * ARG_NUM + ARG_USER_ID, &value);
		if (ret) {
			MMDVFS_DBG("idx(%u) cannot get user_id\n", idx);
			return -EINVAL;
		}
		mtk_mmdvfs_clks[max_mmdvfs_num].user_id = value;

		ret = of_property_read_u32_index(node, MMDVFS_CLKS,
				idx * ARG_NUM + ARG_SPECIAL_TYPE, &value);
		if (ret) {
			MMDVFS_DBG("idx(%u) cannot get special_type\n", idx);
			return -EINVAL;
		}
		mtk_mmdvfs_clks[max_mmdvfs_num].special_type = value;

		ret = of_property_read_u32_index(node, MMDVFS_CLKS,
				idx * ARG_NUM + ARG_IPI_TYPE, &value);
		if (ret) {
			MMDVFS_DBG("idx(%u) cannot get ipi type\n", idx);
			return -EINVAL;
		}
		mtk_mmdvfs_clks[max_mmdvfs_num].ipi_type = value;

		ret = of_property_read_u32_index(node, MMDVFS_CLKS,
				idx * ARG_NUM + ARG_OPP_TABLE, &opp_table_ph);
		if (ret) {
			MMDVFS_DBG("idx(%u) cannot get opp_table\n", idx);
			return -EINVAL;
		}

		opp_table = of_find_node_by_phandle(opp_table_ph);
		opp_idx = 0;
		do {
			opp_node = of_get_next_available_child(opp_table, opp_node);
			if (opp_node) {
				of_property_read_u64(opp_node, "opp-hz", &freq);
				mtk_mmdvfs_clks[max_mmdvfs_num].freqs[opp_idx] = freq;
				opp_idx++;
			}
		} while (opp_node);
		mtk_mmdvfs_clks[max_mmdvfs_num].freq_num = opp_idx;

		mtk_mmdvfs_clks[max_mmdvfs_num].opp = MAX_OPP;

		MMDVFS_DBG("name=%s clk=%u pwr=%u usr=%u special=%u ipi=%u freq_num=%u opp=%u\n",
			mtk_mmdvfs_clks[max_mmdvfs_num].name,
			mtk_mmdvfs_clks[max_mmdvfs_num].clk_id,
			mtk_mmdvfs_clks[max_mmdvfs_num].pwr_id,
			mtk_mmdvfs_clks[max_mmdvfs_num].user_id,
			mtk_mmdvfs_clks[max_mmdvfs_num].special_type,
			mtk_mmdvfs_clks[max_mmdvfs_num].ipi_type,
			mtk_mmdvfs_clks[max_mmdvfs_num].freq_num,
			mtk_mmdvfs_clks[max_mmdvfs_num].opp);

		for (i = 0; i < mtk_mmdvfs_clks[max_mmdvfs_num].freq_num; i++)
			MMDVFS_DBG("i=%d freq=%llu\n", i,
				mtk_mmdvfs_clks[max_mmdvfs_num].freqs[i]);
		max_mmdvfs_num++;
	}

	for (idx = 0; idx < MAX_PWR_NUM; idx++)
		mmdvfs_pwr_opp[idx] = MAX_OPP;

	clk_data = mtk_alloc_clk_data(max_mmdvfs_num);
	mtk_mmdvfs_register_clks(max_mmdvfs_num, clk_data);

	ret = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (ret)
		MMDVFS_DBG("could not register clock provider: %d\n", ret);

	kthr = kthread_run(mmdvfs_vcp_init_thread, NULL, "mmdvfs-vcp");

	for (i = 0; i < POWER_NUM; i++) {
		vote_clk[i] = of_clk_get(node, i);
		if (IS_ERR_OR_NULL(vote_clk[i])) {
			MMDVFS_DBG("pwr(%u) could not get vote clk:%d\n", i, ret);
			vote_clk[i] = NULL;
		}
	}

	MMDVFS_DBG("probe done\n");

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
