// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/slab.h>

#include "apu_top.h"
#include "aputop_rpmsg.h"
#include "mt6895_apupwr.h"
#include "mt6895_apupwr_prot.h"

#define LOCAL_DBG	(1)

static void __iomem *spare_reg_base;

// for saving data after sync with remote site
static struct tiny_dvfs_opp_tbl opp_tbl;
static struct apu_pwr_curr_info curr_info;
static const char * const pll_name[] = {
				"PLL_CONN", /* "PLL_RV33", */"PLL_MVPU", "PLL_MDLA"};
static const char * const buck_name[] = {
				"BUCK_VAPU", "BUCK_VSRAM", "BUCK_VCORE"};
static const char * const cluster_name[] = {
				"ACX0", "ACX1", "RCX"};

#define _OPP_LMT_TBL(_opp_lmt_reg) {    \
	.opp_lmt_reg = _opp_lmt_reg,    \
}
static struct cluster_dev_opp_info opp_limit_tbl[CLUSTER_NUM] = {
	_OPP_LMT_TBL(ACX0_LIMIT_OPP_REG),
};

static inline int over_range_check(int opp)
{
	// we treat opp -1 as a special hint regard to unlimit opp !
	if (opp == -1)
		return -1;
	else if (opp < USER_MAX_OPP_VAL)
		return USER_MAX_OPP_VAL;
	else if (opp > USER_MIN_OPP_VAL)
		return USER_MIN_OPP_VAL;
	else
		return opp;
}

static void _opp_limiter(int vpu_max, int vpu_min, int dla_max, int dla_min,
		enum apu_opp_limit_type type)
{
	int i;
	unsigned int reg_data;
	unsigned int reg_offset;

	vpu_max = over_range_check(vpu_max);
	vpu_min = over_range_check(vpu_min);
	dla_max = over_range_check(dla_max);
	dla_min = over_range_check(dla_min);

#if LOCAL_DBG
	pr_info("%s type:%d, %d/%d/%d/%d\n", __func__, type,
			vpu_max, vpu_min, dla_max, dla_min);
#endif
	for (i = 0 ; i < CLUSTER_NUM ; i++) {
		opp_limit_tbl[i].dev_opp_lmt.vpu_max = vpu_max & 0x3f;
		opp_limit_tbl[i].dev_opp_lmt.vpu_min = vpu_min & 0x3f;
		opp_limit_tbl[i].dev_opp_lmt.dla_max = dla_max & 0x3f;
		opp_limit_tbl[i].dev_opp_lmt.dla_min = dla_min & 0x3f;
		opp_limit_tbl[i].dev_opp_lmt.lmt_type = type & 0xff;

		reg_data = 0x0;
		reg_data = (
			((vpu_max & 0x3f) << 0) |	// [5:0]
			((vpu_min & 0x3f) << 6) |	// [11:6]
			((dla_max & 0x3f) << 12) |	// [17:12]
			((dla_min & 0x3f) << 18) |	// [23:18]
			((type & 0xff) << 24));		// dedicate 1 byte

		reg_offset = opp_limit_tbl[i].opp_lmt_reg;

		apu_writel(reg_data, spare_reg_base + reg_offset);
#if LOCAL_DBG
		pr_info("%s cluster%d write:0x%08x, readback:0x%08x\n",
				__func__, i, reg_data,
				apu_readl(spare_reg_base + reg_offset));
#endif
	}

}

static void limit_opp_to_all_devices(int opp)
{
	int c_id, d_id;

	for (c_id = 0 ; c_id < CLUSTER_NUM ; c_id++)
		for (d_id = 0 ; d_id < DEVICE_NUM ; d_id++)
			_opp_limiter(opp, opp, opp, opp, OPP_LIMIT_DEBUG);
}

void mt6895_aputop_opp_limit(struct aputop_func_param *aputop,
		enum apu_opp_limit_type type)
{
	int vpu_max, vpu_min, dla_max, dla_min;

	vpu_max = aputop->param1;
	vpu_min = aputop->param2;
	dla_max = aputop->param3;
	dla_min = aputop->param4;

	_opp_limiter(vpu_max, vpu_min, dla_max, dla_min, type);
}

#if IS_ENABLED(CONFIG_DEBUG_FS)
static int aputop_dbg_set_parameter(int param, int argc, int *args)
{
	int ret = 0, i;
	struct aputop_rpmsg_data rpmsg_data;

	for (i = 0 ; i < argc ; i++) {
		if (args[i] < -1 || args[i] >= INT_MAX) {
			pr_info("%s invalid args[%d]\n", __func__, i);
			return -EINVAL;
		}
	}

	memset(&rpmsg_data, 0, sizeof(struct aputop_rpmsg_data));

	switch (param) {
	case APUPWR_DBG_DEV_CTL:
		if (argc == 3) {
			rpmsg_data.cmd = APUTOP_DEV_CTL;
			rpmsg_data.data0 = args[0]; // cluster_id
			rpmsg_data.data1 = args[1]; // device_id
			rpmsg_data.data2 = args[2]; // POWER_ON/POWER_OFF
			aputop_send_rpmsg(&rpmsg_data, 100);
		} else {
			pr_info("%s invalid param num:%d\n", __func__, argc);
			ret = -EINVAL;
		}
		break;
	case APUPWR_DBG_DEV_SET_OPP:
		if (argc == 3) {
			rpmsg_data.cmd = APUTOP_DEV_SET_OPP;
			rpmsg_data.data0 = args[0]; // cluster_id
			rpmsg_data.data1 = args[1]; // device_id
			rpmsg_data.data2 = args[2]; // opp
			aputop_send_rpmsg(&rpmsg_data, 100);
		} else {
			pr_info("%s invalid param num:%d\n", __func__, argc);
			ret = -EINVAL;
		}
		break;
	case APUPWR_DBG_DVFS_DEBUG:
		if (argc == 1) {
			limit_opp_to_all_devices(args[0]);
		} else {
			pr_info("%s invalid param num:%d\n", __func__, argc);
			ret = -EINVAL;
		}
		break;
	case APUPWR_DBG_DUMP_OPP_TBL:
		if (argc == 1) {
			rpmsg_data.cmd = APUTOP_DUMP_OPP_TBL;
			rpmsg_data.data0 = args[0]; // pseudo data
			aputop_send_rpmsg(&rpmsg_data, 100);
		} else {
			pr_info("%s invalid param num:%d\n", __func__, argc);
			ret = -EINVAL;
		}
		break;
	case APUPWR_DBG_CURR_STATUS:
		if (argc == 1) {
			rpmsg_data.cmd = APUTOP_CURR_STATUS;
			rpmsg_data.data0 = args[0]; // pseudo data
			aputop_send_rpmsg(&rpmsg_data, 100);
		} else {
			pr_info("%s invalid param num:%d\n", __func__, argc);
			ret = -EINVAL;
		}
		break;
	case APUPWR_DBG_PROFILING:
		if (argc == 2) {
			rpmsg_data.cmd = APUTOP_PWR_PROFILING;
			// 0:clean, 1:result, 2:allow bit, 3:allow bitmask
			rpmsg_data.data0 = args[0];
			// value of allow bit/bitmask
			rpmsg_data.data1 = args[1]; // allow bitmask
			aputop_send_rpmsg(&rpmsg_data, 100);
		} else {
			pr_info("%s invalid param num:%d\n", __func__, argc);
			ret = -EINVAL;
		}
		break;
	default:
		pr_info("%s unsupport the pwr param:%d\n", __func__, param);
		ret = -EINVAL;
		break;
	}

	return ret;
}

// boost range : 100 ~ 0 (from fast to slow)
// opp range : 1 ~ USER_MIN_OPP_VAL (from fast to slow) , opp0 is turbo boost
static int _apu_boost_to_opp(int boost)
{
	int opp, sec;
	int opp_min_num = USER_MIN_OPP_VAL;

	if (boost > 100)
		return TURBO_BOOST_OPP;

	if (boost < 0)
		boost = 0;

	// not include opp 0, adjust here to handle the part of not divisible
	sec = 100 / opp_min_num;
	opp = opp_min_num - (boost / sec);

	if (opp > opp_min_num)
		opp = opp_min_num;

	if (opp <= TURBO_BOOST_OPP)
		opp = TURBO_BOOST_OPP + 1;

	return opp;
}

static void plat_dump_boost_mapping(struct seq_file *s)
{
	int boost, opp, i;
	int opp_cnt[USER_MIN_OPP_VAL + 1] = {};
	int begin, end;

	for (boost = TURBO_BOOST_VAL ; boost >= 0 ; boost--) {
		opp = _apu_boost_to_opp(boost);
		opp_cnt[opp]++;
	}

	begin = TURBO_BOOST_VAL;

	for (i = 0 ; i < USER_MIN_OPP_VAL + 1; i++) {
		end = begin - opp_cnt[i] + 1;
		seq_printf(s, "opp%d : boost %d ~ %d (%d)\n",
					i, begin, end, opp_cnt[i]);
		begin -= opp_cnt[i];
	}
}

static int aputop_show_opp_tbl(struct seq_file *s, void *unused)
{
	struct tiny_dvfs_opp_tbl tbl;
	int size, i, j;

	pr_info("%s ++\n", __func__);
	memcpy(&tbl, &opp_tbl, sizeof(struct tiny_dvfs_opp_tbl));
	size = tbl.tbl_size;

	// first line
	seq_printf(s, "\n| # | %s |", buck_name[0]);
	for (i = 0 ; i < PLL_NUM ; i++)
		seq_printf(s, " %s |", pll_name[i]);

	seq_puts(s, "\n");
	for (i = 0 ; i < size ; i++) {
		seq_printf(s, "| %d |   %d  |", i, tbl.opp[i].vapu);

		for (j = 0 ; j < PLL_NUM ; j++)
			seq_printf(s, "  %07d |", tbl.opp[i].pll_freq[j]);

		seq_puts(s, "\n");
	}

	seq_puts(s, "\n");
	plat_dump_boost_mapping(s);
	seq_puts(s, "\n");

	return 0;
}

static int aputop_show_curr_status(struct seq_file *s, void *unused)
{
	struct apu_pwr_curr_info info;
	struct rpc_status_dump cluster_dump[CLUSTER_NUM + 1];
	int i;

	pr_info("%s ++\n", __func__);

	memset(&cluster_dump, 0, sizeof(struct rpc_status_dump));
	memcpy(&info, &curr_info, sizeof(struct apu_pwr_curr_info));

	seq_puts(s, "\n");

	for (i = 0 ; i < PLL_NUM ; i++) {
		seq_printf(s, "%s : opp %d , %d(kHz)\n",
				pll_name[i],
				info.pll_opp[i],
				info.pll_freq[i]);
	}

	for (i = 0 ; i < BUCK_NUM ; i++) {
		seq_printf(s, "%s : opp %d , %d(mV)\n",
				buck_name[i],
				info.buck_opp[i],
				info.buck_volt[i]);
	}

	for (i = 0 ; i < CLUSTER_NUM ; i++) {
		mt6895_apu_dump_rpc_status(i, &cluster_dump[i]);
		seq_printf(s, "%s : rpc_status 0x%08x , conn_cg 0x%08x\n",
				cluster_name[i],
				cluster_dump[i].rpc_reg_status,
				cluster_dump[i].conn_reg_status);
	}

	// for RCX
	mt6895_apu_dump_rpc_status(RCX, &cluster_dump[CLUSTER_NUM]);
	seq_printf(s,
		"%s : rpc_status 0x%08x , conn_cg 0x%08x vcore_cg 0x%08x\n",
			cluster_name[CLUSTER_NUM],
			cluster_dump[CLUSTER_NUM].rpc_reg_status,
			cluster_dump[CLUSTER_NUM].conn_reg_status,
			cluster_dump[CLUSTER_NUM].vcore_reg_status);

	seq_puts(s, "\n");

	return 0;
}

static int apu_top_dbg_show(struct seq_file *s, void *unused)
{
	int ret = 0;
	enum aputop_rpmsg_cmd cmd = get_curr_rpmsg_cmd();

	pr_info("%s for aputop_rpmsg_cmd : %d\n", __func__, cmd);

	if (cmd == APUTOP_DUMP_OPP_TBL)
		ret = aputop_show_opp_tbl(s, unused);
	else if (cmd == APUTOP_CURR_STATUS)
		ret = aputop_show_curr_status(s, unused);
	else
		pr_info("%s not support this cmd\n", __func__, cmd);

	return ret;
}

int mt6895_apu_top_dbg_open(struct inode *inode, struct file *file)
{
	pr_info("%s ++\n", __func__);

	return single_open(file, apu_top_dbg_show, inode->i_private);
}

#define MAX_ARG 4
ssize_t mt6895_apu_top_dbg_write(
		struct file *flip, const char __user *buffer,
		size_t count, loff_t *f_pos)
{
	char *tmp, *token, *cursor;
	int ret, i, param;
	unsigned int args[MAX_ARG];

	pr_info("%s\n", __func__);

	tmp = kzalloc(count + 1, GFP_KERNEL);
	if (!tmp)
		return -ENOMEM;

	ret = copy_from_user(tmp, buffer, count);
	if (ret) {
		pr_info("[%s] copy_from_user failed, ret=%d\n", __func__, ret);
		goto out;
	}

	tmp[count] = '\0';
	cursor = tmp;
	/* parse a command */
	token = strsep(&cursor, " ");
	if (!strcmp(token, "device_ctl"))
		param = APUPWR_DBG_DEV_CTL;
	else if (!strcmp(token, "device_set_opp"))
		param = APUPWR_DBG_DEV_SET_OPP;
	else if (!strcmp(token, "dvfs_debug"))
		param = APUPWR_DBG_DVFS_DEBUG;
	else if (!strcmp(token, "dump_opp_tbl"))
		param = APUPWR_DBG_DUMP_OPP_TBL;
	else if (!strcmp(token, "curr_status"))
		param = APUPWR_DBG_CURR_STATUS;
	else if (!strcmp(token, "pwr_profiling"))
		param = APUPWR_DBG_PROFILING;
	else {
		ret = -EINVAL;
		pr_info("no power param[%s]!\n", token);
		goto out;
	}

	/* parse arguments */
	for (i = 0; i < MAX_ARG && (token = strsep(&cursor, " ")); i++) {
		ret = kstrtoint(token, 10, &args[i]);
		if (ret) {
			pr_info("fail to parse args[%d](%s)", i, token);
			goto out;
		}
	}

	aputop_dbg_set_parameter(param, i, args);
	ret = count;
out:
	kfree(tmp);
	return ret;
}
#endif

int mt6895_apu_top_rpmsg_cb(int cmd, void *data, int len, void *priv, u32 src)
{
	int ret = 0;

	switch ((enum aputop_rpmsg_cmd)cmd) {
	case APUTOP_DEV_CTL:
	case APUTOP_DEV_SET_OPP:
	case APUTOP_PWR_PROFILING:
		// do nothing
		break;
	case APUTOP_DUMP_OPP_TBL:
		if (len == sizeof(opp_tbl)) {
			memcpy(&opp_tbl,
				(struct tiny_dvfs_opp_tbl *)data, len);
		} else {
			pr_info("%s invalid size : %d/%lu\n",
					__func__, len, sizeof(opp_tbl));
			ret = -EINVAL;
		}
		break;
	case APUTOP_CURR_STATUS:
		if (len == sizeof(curr_info)) {
			memcpy(&curr_info,
				(struct apu_pwr_curr_info *)data, len);
		} else {
			pr_info("%s invalid size : %d/%lu\n",
					__func__, len, sizeof(curr_info));
			ret = -EINVAL;
		}
		break;
	default:
		pr_info("%s invalid cmd : %d\n", __func__, cmd);
		ret = -EINVAL;
	}

	return ret;
}

int mt6895_pwr_flow_remote_sync(uint32_t cfg)
{
	uint32_t reg_data = 0x0;

	reg_data = (cfg & 0x1);

	apu_writel(reg_data, spare_reg_base + PWR_FLOW_SYNC_REG);

	pr_info("%s write 0x%08x, readback:0x%08x\n",
			__func__, reg_data,
			apu_readl(spare_reg_base + PWR_FLOW_SYNC_REG));

	return 0;
}

int mt6895_drv_cfg_remote_sync(struct aputop_func_param *aputop)
{
	struct drv_cfg_data cfg;
	uint32_t reg_data = 0x0;

	cfg.log_level = aputop->param1 & 0xf;
	cfg.dvfs_debounce = aputop->param2 & 0xf;
	cfg.disable_hw_meter = aputop->param3 & 0xf;

	reg_data = cfg.log_level |
		(cfg.dvfs_debounce << 8) |
		(cfg.disable_hw_meter << 16);

	pr_info("%s 0x%08x\n", __func__, reg_data);
	apu_writel(reg_data, spare_reg_base + DRV_CFG_SYNC_REG);

	return 0;
}

int mt6895_chip_data_remote_sync(struct plat_cfg_data *plat_cfg)
{
	uint32_t reg_data = 0x0;

	reg_data = (plat_cfg->aging_flag & 0xf)
		| ((plat_cfg->hw_id & 0xf) << 4)
		| ((plat_cfg->seg_efuse & 0xf) << 8);

	pr_info("%s 0x%08x\n", __func__, reg_data);
	apu_writel(reg_data, spare_reg_base + PLAT_CFG_SYNC_REG);

	return 0;
}

int mt6895_init_remote_data_sync(void __iomem *reg_base)
{
	int i;
	uint32_t reg_offset = 0x0;

	spare_reg_base = reg_base;

	for (i = 0 ; i < CLUSTER_NUM ; i++) {
		// 0xffff_ffff means no limit
		memset(&opp_limit_tbl[i].dev_opp_lmt, -1,
				sizeof(struct device_opp_limit));
		reg_offset = opp_limit_tbl[i].opp_lmt_reg;
		apu_writel(0xffffffff, spare_reg_base + reg_offset);
	}

	apu_writel(0x0, spare_reg_base + DRV_CFG_SYNC_REG);
	apu_writel(0x0, spare_reg_base + PLAT_CFG_SYNC_REG);

	return 0;
}
