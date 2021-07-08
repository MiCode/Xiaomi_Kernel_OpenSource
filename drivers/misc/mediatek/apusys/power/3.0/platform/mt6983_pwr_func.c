// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/slab.h>

#include "apu_top.h"
#include "aputop_rpmsg.h"
#include "mt6983_apupwr.h"
#include "mt6983_apupwr_prot.h"

#define LOCAL_DBG	(1)

static void __iomem *g_reg_base;

// for saving data after sync with remote site
static struct tiny_dvfs_opp_tbl opp_tbl;
static struct apu_pwr_curr_info curr_info;
static const char * const pll_name[] = {
				"PLL_CONN", "PLL_UP", "PLL_VPU", "PLL_DLA"};
static const char * const buck_name[] = {
				"BUCK_VAPU", "BUCK_VSRAM", "BUCK_VCORE"};
static const char * const cluster_name[] = {
				"ACX0", "ACX1", "RCX"};

#define _OPP_LMT_TBL(_opp_lmt_reg) {    \
	.opp_lmt_reg = _opp_lmt_reg,    \
}
static struct cluster_dev_opp_info opp_limit_tbl[CLUSTER_NUM] = {
	_OPP_LMT_TBL(ACX0_LIMIT_OPP_REG),
	_OPP_LMT_TBL(ACX1_LIMIT_OPP_REG),
};

static void _opp_limiter(int vpu_max, int vpu_min, int dla_max, int dla_min,
		enum apu_opp_limit_type type)
{
	int i;
	unsigned int reg_data;
	unsigned int reg_offset;

#if LOCAL_DBG
	pr_info("%s type:%d, %d/%d/%d/%d\n", __func__, type,
			vpu_max, vpu_min, dla_max, dla_min);
#endif
	for (i = 0 ; i < CLUSTER_NUM ; i++) {
		opp_limit_tbl[i].dev_opp_lmt.vpu_max = vpu_max & 0xf;
		opp_limit_tbl[i].dev_opp_lmt.vpu_min = vpu_min & 0xf;
		opp_limit_tbl[i].dev_opp_lmt.dla_max = dla_max & 0xf;
		opp_limit_tbl[i].dev_opp_lmt.dla_min = dla_min & 0xf;
		opp_limit_tbl[i].dev_opp_lmt.lmt_type = type & 0xff;

		reg_data = 0x0;
		reg_data = ((vpu_max & 0xf) |		// [3:0]
			((vpu_min & 0xf) << 4) |	// [7:4]
			((dla_max & 0xf) << 8) |	// [b:8]
			((dla_min & 0xf) << 12) |	// [f:c]
			((type & 0xff) << 16));		// dedicate 1 byte

		reg_offset = opp_limit_tbl[i].opp_lmt_reg;

		apu_writel(reg_data, g_reg_base + reg_offset);
#if LOCAL_DBG
		pr_info("%s cluster%d write:0x%08x, readback:0x%08x\n",
				__func__, i, reg_data,
				apu_readl(g_reg_base + reg_offset));
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

void aputop_opp_limit(struct aputop_func_param *aputop,
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
		if (args[i] < 0 || args[i] > 255) {
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
	default:
		pr_info("%s unsupport the pwr param:%d\n", __func__, param);
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int aputop_show_opp_tbl(struct seq_file *s, void *unused)
{
	struct tiny_dvfs_opp_tbl tbl;
	int size, i, j;

	memcpy(&tbl, &opp_tbl, sizeof(struct tiny_dvfs_opp_tbl));
	size = tbl.tbl_size;

	// first line
	seq_printf(s, "\n|#|%*s|", "BUCK_VAPU");
	for (i = 0 ; i < PLL_NUM ; i++)
		seq_printf(s, "%*s|", pll_name[i]);

	seq_puts(s, "\n");
	for (i = 0 ; i < size ; i++) {
		seq_printf(s, "|%d|%*d|", i, tbl.opp[i].vapu);

		for (j = 0 ; j < PLL_NUM ; j++)
			seq_printf(s, "|%*d|", tbl.opp[i].pll_freq[j]);

		seq_puts(s, "\n");
	}

	return 0;
}

static int aputop_show_curr_status(struct seq_file *s, void *unused)
{
	struct apu_pwr_curr_info info;
	struct rpc_status_dump cluster_dump[CLUSTER_NUM + 1];
	int i;

	memcpy(&info, &curr_info, sizeof(struct apu_pwr_curr_info));

	seq_puts(s, "\n");

	for (i = 0 ; i < PLL_NUM ; i++) {
		seq_printf(s, "%*s : opp %*d , %*d(kHz)",
				pll_name[i],
				info.pll_opp[i],
				info.pll_freq[i]);
	}

	for (i = 0 ; i < BUCK_NUM ; i++) {
		seq_printf(s, "%*s : opp %*d , %*d(mV)\n",
				buck_name[i],
				info.buck_opp[i],
				info.buck_volt[i]);
	}

	for (i = 0 ; i < CLUSTER_NUM ; i++) {
		apu_dump_rpc_status(i, &cluster_dump[i]);
		seq_printf(s, "%*s : rpc_status 0x%08x , conn_cg 0x%08x\n",
				cluster_name[i],
				cluster_dump[i].rpc_reg_status,
				cluster_dump[i].conn_reg_status);
	}

	// for RCX
	apu_dump_rpc_status(RCX, &cluster_dump[CLUSTER_NUM]);
	seq_printf(s,
		"%*s : rpc_status 0x%08x , conn_cg 0x%08x vcore_cg 0x%08x\n",
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

int mt6983_apu_top_dbg_open(struct inode *inode, struct file *file)
{
	return single_open(file, apu_top_dbg_show, inode->i_private);
}

#define MAX_ARG 4
ssize_t mt6983_apu_top_dbg_write(
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

int mt6983_apu_top_rpmsg_cb(int cmd, void *data, int len, void *priv, u32 src)
{
	int ret = 0;

	switch ((enum aputop_rpmsg_cmd)cmd) {
	case APUTOP_DEV_CTL:
	case APUTOP_DEV_SET_OPP:
		// do nothing
		break;
	case APUTOP_DUMP_OPP_TBL:
		if (len == sizeof(opp_tbl)) {
			memcpy(&opp_tbl,
				(struct tiny_dvfs_opp_tbl *)data, len);
		} else {
			pr_info("%s invalid size : %d/%d\n",
					__func__, len, sizeof(opp_tbl));
			ret = -EINVAL;
		}
		break;
	case APUTOP_CURR_STATUS:
		if (len == sizeof(curr_info)) {
			memcpy(&curr_info,
				(struct apu_pwr_curr_info *)data, len);
		} else {
			pr_info("%s invalid size : %d/%d\n",
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

int aputop_opp_limiter_init(void __iomem *reg_base)
{
	int i;
	uint32_t reg_offset = 0x0;

	g_reg_base = reg_base;

	for (i = 0 ; i < CLUSTER_NUM ; i++) {
		// 0xffff_ffff means no limit
		memset(&opp_limit_tbl[i].dev_opp_lmt, -1,
				sizeof(struct device_opp_limit));
		reg_offset = opp_limit_tbl[i].opp_lmt_reg;
#if LOCAL_DBG
		pr_info("%s g_reg_base:0x%08x, offset:0x%08x\n",
				__func__, g_reg_base, reg_offset);
#endif
		apu_writel(0xffffffff, g_reg_base + reg_offset);
	}

	return 0;
}
