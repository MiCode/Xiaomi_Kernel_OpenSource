// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#include <linux/clk.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/platform_device.h>
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

static inline struct mtk_mmdvfs_clk *to_mtk_mmdvfs_clk(struct clk_hw *hw)
{
	return container_of(hw, struct mtk_mmdvfs_clk, clk_hw);
}

static int mtk_mmdvfs_set_rate(struct clk_hw *hw, unsigned long rate,
		unsigned long parent_rate)
{
	struct mtk_mmdvfs_clk *mmdvfs_clk = to_mtk_mmdvfs_clk(hw);
	u8 i, opp, level, pwr_opp = MAX_OPP;

	for (i = 0; i < mmdvfs_clk->freq_num; i++)
		if (rate <= mmdvfs_clk->freqs[i])
			break;

	level = (i == mmdvfs_clk->freq_num) ? (i-1) : i;
	opp = (mmdvfs_clk->freq_num - level - 1);
	MMDVFS_DBG("%s: rate=%lu parent_rate=%lu freq=%lu old_opp=%d new_opp=%d\n",
		__func__, rate, parent_rate, mmdvfs_clk->freqs[level], mmdvfs_clk->opp, opp);
	if (mmdvfs_clk->opp == opp)
		return 0;
	mmdvfs_clk->opp = opp;

	for (i = 0; i < max_mmdvfs_num; i++) {
		if (mmdvfs_clk->pwr_id == mtk_mmdvfs_clks[i].pwr_id
			&& mtk_mmdvfs_clks[i].opp < pwr_opp)
			pwr_opp = mtk_mmdvfs_clks[i].opp;
	}

	if (mmdvfs_clk->special_type != SPECIAL_INDEPENDENCE
		&& pwr_opp ==  mmdvfs_clk->opp)
		return 0;

	mmdvfs_pwr_opp[mmdvfs_clk->pwr_id] = pwr_opp;

	/* Do IPI to MMuP */
	return 0;
}

static long mtk_mmdvfs_round_rate(struct clk_hw *hw, unsigned long rate,
			       unsigned long *parent_rate)
{
	MMDVFS_DBG("%s: rate=%lu parent_rate=%lu\n", __func__, rate, *parent_rate);

	return rate;
}

static unsigned long mtk_mmdvfs_recalc_rate(struct clk_hw *hw,
					 unsigned long parent_rate)
{
	MMDVFS_DBG("%s: parent_rate=%lu\n", __func__, parent_rate);
	return parent_rate;
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
				MMDVFS_DBG("%s: failed to register clk %s: %ld\n",
				       __func__, mtk_mmdvfs_clks[i].name, PTR_ERR(clk));
				continue;
			}

			clk_data->clks[i] = clk;
		}
	}
	return 0;
}
//EXPORT_SYMBOL(mtk_clk_register_mmdvfs);

static int mmdvfs_vcp_is_ready(void)
{
	int ret = 0;

	while (!is_vcp_ready_ex(VCP_A_ID)) {
		ret += 1;
		MMDVFS_DBG("retry:%d VCP_A_ID:%d not ready", ret, VCP_A_ID);
		if (ret > 100)
			return 0;
		msleep(50);
	}
	return 1;
}

static int mmdvfs_vcp_ipi_send(const u8 func_id, const u8 user_id, const u8 opp)
{
	struct mmdvfs_ipi_data slot = {func_id, user_id, opp, 0};
	int ret = 0;
	//ktime_t time;

	if (!mmdvfs_vcp_is_ready())
		return -ETIMEDOUT;
	MMDVFS_DBG(
		"ipi_id:%d func_id:%hhu user_id:%hhu opp:%hhu slot:%#x size:%d ret:%d",
		IPI_OUT_MMDVFS, func_id, user_id, opp, slot,
		PIN_OUT_SIZE_MMDVFS, ret);

	//time = ktime_get();
	ret = mtk_ipi_send(vcp_get_ipidev(), IPI_OUT_MMDVFS, IPI_SEND_WAIT,
		&slot, PIN_OUT_SIZE_MMDVFS, IPI_TIMEOUT_MS);
	//MMDVFS_DBG("ipi time=%u\n", ktime_us_delta(ktime_get(), time));
	return ret;
}

static int mmdvfs_vcp_ipi_cb(unsigned int ipi_id, void *prdata, void *data,
	unsigned int len) // vcp > ap
{
	struct mmdvfs_ipi_data slot;

	if (ipi_id != IPI_IN_MMDVFS || !data)
		return 0;

	slot = *(struct mmdvfs_ipi_data *)data;

	MMDVFS_DBG(
		"ipi_id:%u slot:%#x func_id:%hhu user_id:%hhu freq_opp:%hhu data_ack:%hhu",
		ipi_id, slot,
		slot.func_id, slot.user_id, slot.freq_opp, slot.data_ack);

	return 0;
}

int set_vote_step(const char *val, const struct kernel_param *kp)
{
	int ret;
	int vote_step;

	ret = kstrtoint(val, 0, &vote_step);
	if (ret) {
		MMDVFS_DBG("mmdvfs set vote step failed: %d\n", ret);
		return ret;
	}

	mmdvfs_vcp_ipi_send(FUNC_SET_OPP, USER_DISP0_AP, vote_step);

	return 0;
}

static struct kernel_param_ops set_vote_step_ops = {
	.set = set_vote_step,
};
module_param_cb(vote_step, &set_vote_step_ops, NULL, 0644);
MODULE_PARM_DESC(vote_step, "vote mmdvfs to specified step");

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

	while (!mmdvfs_vcp_is_ready())
		msleep(2000);
	while (!(vcp_ipi_dev = vcp_get_ipidev())) {
		MMDVFS_DBG("cannot get vcp ipidev");
		msleep(1000);
	}

	ret = mtk_ipi_register(vcp_ipi_dev, IPI_IN_MMDVFS,
		mmdvfs_vcp_ipi_cb, NULL, &mmdvfs_vcp_ipi_data);
	if (ret)
		MMDVFS_DBG("mtk_ipi_register failed:%d ipi_id:%d", ret, IPI_IN_MMDVFS);

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

	if (!elem_num) {
		MMDVFS_DBG("%s: no clk definition found!\n", __func__);
		return 0;
	}

	mtk_mmdvfs_clks = kcalloc(elem_num, sizeof(*mtk_mmdvfs_clks), GFP_KERNEL);
	if (!mtk_mmdvfs_clks) {
		MMDVFS_DBG("%s: no memory for mtk_mmdvfs_clks\n", __func__);
		return -ENOMEM;
	}

	for (idx = 0; idx < elem_num; idx++) {
		of_property_read_string_index(node, MMDVFS_CLK_NAMES, idx,
			&mtk_mmdvfs_clks[max_mmdvfs_num].name);

		mtk_mmdvfs_clks[max_mmdvfs_num].clk_id = max_mmdvfs_num;

		ret = of_property_read_u32_index(node, MMDVFS_CLKS,
				idx * ARG_NUM + ARG_PWR_ID, &value);
		if (ret) {
			MMDVFS_DBG("%s: idx(%u) cannot get pwr_id\n", __func__, idx);
			return -EINVAL;
		}
		mtk_mmdvfs_clks[max_mmdvfs_num].pwr_id = value;

		ret = of_property_read_u32_index(node, MMDVFS_CLKS,
				idx * ARG_NUM + ARG_USER_ID, &value);
		if (ret) {
			MMDVFS_DBG("%s: idx(%u) cannot get user_id\n", __func__, idx);
			return -EINVAL;
		}
		mtk_mmdvfs_clks[max_mmdvfs_num].user_id = value;

		ret = of_property_read_u32_index(node, MMDVFS_CLKS,
				idx * ARG_NUM + ARG_SPECIAL_TYPE, &value);
		if (ret) {
			MMDVFS_DBG("%s: idx(%u) cannot get special_type\n", __func__, idx);
			return -EINVAL;
		}
		mtk_mmdvfs_clks[max_mmdvfs_num].special_type = value;

		ret = of_property_read_u32_index(node, MMDVFS_CLKS,
				idx * ARG_NUM + ARG_IPI_TYPE, &value);
		if (ret) {
			MMDVFS_DBG("%s: idx(%u) cannot get ipi type\n", __func__, idx);
			return -EINVAL;
		}
		mtk_mmdvfs_clks[max_mmdvfs_num].ipi_type = value;

		ret = of_property_read_u32_index(node, MMDVFS_CLKS,
				idx * ARG_NUM + ARG_OPP_TABLE, &opp_table_ph);
		if (ret) {
			MMDVFS_DBG("%s: idx(%u) cannot get opp_table\n", __func__, idx);
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
			MMDVFS_DBG("%s i=%d freq=%llu\n", __func__, i,
				mtk_mmdvfs_clks[max_mmdvfs_num].freqs[i]);
		max_mmdvfs_num++;
	}

	for (idx = 0; idx < MAX_PWR_NUM; idx++)
		mmdvfs_pwr_opp[idx] = MAX_OPP;

	clk_data = mtk_alloc_clk_data(max_mmdvfs_num);
	mtk_mmdvfs_register_clks(max_mmdvfs_num, clk_data);

	ret = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	kthr = kthread_run(mmdvfs_vcp_init_thread, NULL, "mmdvfs-vcp");

	MMDVFS_DBG("%s is called!\n", __func__);

	if (ret)
		MMDVFS_DBG("%s(): could not register clock provider: %d\n",
				__func__, ret);

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
