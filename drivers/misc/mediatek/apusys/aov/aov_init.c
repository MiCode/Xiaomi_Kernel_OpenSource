// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/cdev.h>
#include <linux/of_device.h>
#include <linux/types.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/debugfs.h>
#include <apusys_core.h>
#include <apusys_device.h>
#include <scp.h>

#include "slbc_ops.h"

#include "npu_scp_ipi.h"
#include "aov_rpmsg.h"
#include "aov_recovery.h"

#define MAX_DEBUG_CMD_SIZE (1024)

#define APUSYS_AOV_KVERSION "v1.1.0.1-ko"

#define STATE_TIMEOUT_MS (20)

struct apusys_aov_ctx {
	atomic_t aov_enabled;
	struct device *dev;
	struct completion comp;
	struct npu_scp_ipi_param npu_scp_msg;
	struct mutex scp_ipi_lock;
	atomic_t response_arrived;
	struct dentry *dbg_dir;
	struct dentry *dbg_node;
};

static struct apusys_aov_ctx *aov_ctx;
static struct dentry *apusys_dbg_root;

static int apusys_aov_test_show(struct seq_file *s, void *unused)
{
	// struct apusys_aov_ctx *ctx = (struct apusys_aov_ctx *)s->private;
	struct npu_scp_ipi_param send_msg = { 0, 0, 0, 0 };
	struct npu_scp_ipi_param recv_msg = { 0, 0, 0, 0 };
	int ret, major, minor, subminor, dev;

	send_msg.cmd = NPU_SCP_SYSTEM;
	send_msg.act = NPU_SCP_SYSTEM_GET_VERSION;
	ret = npu_scp_ipi_send(&send_msg, &recv_msg, SCP_IPI_TIMEOUT_MS);
	if (ret) {
		seq_printf(s, "%s failed to send scp ipi, ret %d\n", __func__, ret);
		return 0;
	}

	major = (recv_msg.ret >> 24) & 0xff;
	minor = (recv_msg.ret >> 16) & 0xff;
	subminor = (recv_msg.ret >> 8) & 0xff;
	dev = recv_msg.ret & 0xff;
	seq_printf(s, "AOV APU KO Version: %s\n", APUSYS_AOV_KVERSION);
	seq_printf(s, "AOV APU Version: %d.%d.%d.%d\n", major, minor, subminor, dev);

	return 0;
}

static int apusys_aov_test_open(struct inode *inode, struct file *flip)
{
	return single_open(flip, apusys_aov_test_show, inode->i_private);
}

static inline bool args_compare(char *lstr, char *sstr, char *var)
{
	if (!var)
		return false;

	if (lstr && sstr) {
		size_t llens = min(strlen(lstr), strlen(var));
		size_t slens = min(strlen(sstr), strlen(var));

		return ((strncmp((lstr), (var), llens)) == 0) ||
			((strncmp((sstr), (var), slens)) == 0);
	} else if (lstr) {
		size_t llens = min(strlen(lstr), strlen(var));

		return ((strncmp((lstr), (var), llens)) == 0);
	} else
		return false;
}

static inline char *args_get_next_str(char **str)
{
	static char *empty_str = "";
	char *token = NULL;

	if (str)
		token = strsep(str, " ");

	if (token == NULL)
		return empty_str;

	return token;
}

static int apusys_aov_start_testcase(struct seq_file *s, struct apusys_aov_ctx *ctx,
				     unsigned int testcase)
{
	struct npu_scp_ipi_param send_msg = { 0, 0, 0, 0 };
	struct npu_scp_ipi_param recv_msg = { 0, 0, 0, 0 };
	int ret;

	dev_info(ctx->dev, "%s Start testcase %u\n", __func__, testcase);

	send_msg.cmd = NPU_SCP_TEST;
	send_msg.act = NPU_SCP_TEST_START;
	send_msg.arg = testcase;
	ret = npu_scp_ipi_send(&send_msg, &recv_msg, TESTCASE_TIMEOUT_MS);
	if (ret) {
		dev_info(ctx->dev, "%s failed to send scp ipi, ret %d\n", __func__, ret);
		return ret;
	}

	if (recv_msg.ret == NPU_SCP_RET_TEST_START_ERR) {
		dev_info(ctx->dev, "%s failed to start testcase %u, ret %d\n", __func__, testcase,
			 recv_msg.ret);
		return recv_msg.ret;
	}

	dev_info(ctx->dev, "%s testcase %u is running\n", __func__, testcase);

	return recv_msg.ret;
}

static int apusys_aov_stop_testcase(struct seq_file *s, struct apusys_aov_ctx *ctx,
				     unsigned int testcase)
{
	struct npu_scp_ipi_param send_msg = { 0, 0, 0, 0 };
	struct npu_scp_ipi_param recv_msg = { 0, 0, 0, 0 };
	int ret;

	dev_info(ctx->dev, "%s Stop testcase %u\n", __func__, testcase);

	send_msg.cmd = NPU_SCP_TEST;
	send_msg.act = NPU_SCP_TEST_STOP;
	send_msg.arg = testcase;
	ret = npu_scp_ipi_send(&send_msg, &recv_msg, TESTCASE_TIMEOUT_MS);
	if (ret) {
		dev_info(ctx->dev, "%s failed to send scp ipi, ret %d\n", __func__, ret);
		return ret;
	}

	if (recv_msg.ret == NPU_SCP_RET_TEST_STOP_ERR) {
		dev_info(ctx->dev, "%s failed to stop testcase %u\n", __func__, testcase);
		return recv_msg.ret;
	}

	if (recv_msg.ret)
		dev_info(ctx->dev, "%s ERROR! testcase %u return error %d\n", __func__, testcase,
			 recv_msg.ret, recv_msg.ret);
	else
		dev_info(ctx->dev, "%s testcase %u done successfully, run %d times, ret %d\n",
			 __func__, testcase, recv_msg.arg, recv_msg.ret);

	return recv_msg.ret;
}

static int npu_system_cmd_to_scp(struct apusys_aov_ctx *ctx, struct npu_scp_ipi_param *param)
{
	struct npu_scp_ipi_param send_msg = { 0, 0, 0, 0 };
	int ret = 0;

	switch (param->act) {
	case NPU_SCP_SYSTEM_FORCE_TO_SUSPEND:
		send_msg.cmd = NPU_SCP_SYSTEM;
		send_msg.act = NPU_SCP_SYSTEM_FORCE_TO_SUSPEND;
		ret = npu_scp_ipi_send(&send_msg, NULL, SCP_IPI_TIMEOUT_MS);
		if (ret)
			dev_info(ctx->dev, "%s failed to send scp ipi, ret %d\n", __func__, ret);
		break;
	case NPU_SCP_SYSTEM_FORCE_TO_RESUME:
		send_msg.cmd = NPU_SCP_SYSTEM;
		send_msg.act = NPU_SCP_SYSTEM_FORCE_TO_RESUME;
		ret = npu_scp_ipi_send(&send_msg, NULL, SCP_IPI_TIMEOUT_MS);
		if (ret)
			dev_info(ctx->dev, "%s failed to send scp ipi, ret %d\n", __func__, ret);
		break;
	default:
		dev_info(ctx->dev, "%s Not supported act %d\n", __func__, param->act);
		break;
	}

	return ret;
}

static ssize_t apusys_aov_test_write(struct file *flip, const char __user *buffer,
				     size_t count, loff_t *f_pos)
{
	struct seq_file *s = (struct seq_file *)flip->private_data;
	struct apusys_aov_ctx *ctx = (struct apusys_aov_ctx *)s->private;
	char *user_input, *cmd_str;
	unsigned int testcase = 0;
	bool trigger_start = false;
	bool trigger_stop = false;

	if (count > MAX_DEBUG_CMD_SIZE)
		return -EINVAL;

	user_input = kzalloc(count + 1, GFP_KERNEL);
	if (!user_input)
		return -ENOMEM;

	if (!access_ok(buffer, count)) {
		dev_info(ctx->dev, "%s unreadable user buffer\n", __func__);
		goto out;
	}

	if (copy_from_user(user_input, buffer, count)) {
		dev_info(ctx->dev, "%s Failed to copy_from_user\n", __func__);
		goto out;
	}

	user_input[count] = '\0';

	cmd_str = user_input;

	dev_info(ctx->dev, "%s Get testing cmd %s\n", __func__, cmd_str);

	while (cmd_str != NULL) {
		char *token = args_get_next_str(&cmd_str);

		if (args_compare("--testcase", "-t", token)) {
			unsigned int number;

			token = args_get_next_str(&cmd_str);
			if (kstrtouint(token, 10, &number)) {
				dev_info(ctx->dev, "%s Failed to get %s number\n", __func__, token);
				break;
			}

			testcase = number;
		} else if (args_compare("--system", "-s", token)) {
			struct npu_scp_ipi_param param = { 0, 0, 0, 0 };
			unsigned int act;

			token = args_get_next_str(&cmd_str);
			if (kstrtouint(token, 10, &act))
				break;

			param.cmd = NPU_SCP_SYSTEM;
			param.act = act;
			npu_system_cmd_to_scp(ctx, &param);
		} else if (args_compare("--start", NULL, token)) {

			trigger_start = true;
		} else if (args_compare("--stop", NULL, token)) {

			trigger_stop = true;
		} else {
			dev_info(ctx->dev, "%s Unrecognized args: %s\n", __func__, token);
			goto out;
		}
	}

	if (trigger_stop)
		apusys_aov_stop_testcase(s, ctx, testcase);

	if (trigger_start)
		apusys_aov_start_testcase(s, ctx, testcase);

out:
	kfree(user_input);

	return count;
}

static const struct file_operations apusys_aov_test_fops = {
	.open		= apusys_aov_test_open,
	.write		= apusys_aov_test_write,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int npu_system_handler(struct apusys_aov_ctx *ctx, struct npu_scp_ipi_param *param)
{
	switch (param->act) {
	case NPU_SCP_SYSTEM_FUNCTION_ENABLE:
		atomic_set(&ctx->aov_enabled, 1);
		dev_info(ctx->dev, "%s Turn on apu aov\n", __func__);
		break;
	case NPU_SCP_SYSTEM_FUNCTION_DISABLE:
		atomic_set(&ctx->aov_enabled, 0);
		dev_info(ctx->dev, "%s Turn off apu aov\n", __func__);
		break;
	default:
		dev_info(ctx->dev, "%s Not supported act %d\n", __func__, param->act);
		break;
	}

	return 0;
}

int npu_scp_ipi_send(struct npu_scp_ipi_param *send_msg, struct npu_scp_ipi_param *ret_msg,
		     uint32_t timeout_ms)
{
	struct apusys_aov_ctx *ctx = aov_ctx;
	int ret = 0;

	if (!ctx) {
		pr_info("%s apusys aov context is not available\n", __func__);
		return -ENODEV;
	}

	mutex_lock(&ctx->scp_ipi_lock);

	atomic_set(&ctx->response_arrived, 0);

	do {
		ret = mtk_ipi_send(&scp_ipidev, IPI_OUT_NPU_SCP, IPI_SEND_WAIT,
				   send_msg, 4, 0);
		if (ret < 0 && ret != IPI_PIN_BUSY) {
			dev_info(ctx->dev, "%s scp busy, ret %d\n", __func__, ret);
			break;
		}
	} while (ret == IPI_PIN_BUSY);

	if (ret != IPI_ACTION_DONE) {
		dev_info(ctx->dev, "%s failed to send scp ipi, ret %d\n", __func__, ret);
		goto out;
	}

	if (ret_msg) {
		// wait npu_scp_ipi_callback
		reinit_completion(&ctx->comp);
		ret = wait_for_completion_timeout(&ctx->comp, msecs_to_jiffies(timeout_ms));
		if (!ret) {
			dev_info(ctx->dev, "%s wait for scp ipi timeout\n", __func__);
			ret = -EFAULT;
			goto out;
		}

		if (!atomic_read(&ctx->response_arrived)) {
			dev_info(ctx->dev, "%s No response\n", __func__);
			ret = -ENODATA;
			goto out;
		}

		ret_msg->cmd = ctx->npu_scp_msg.cmd;
		ret_msg->act = ctx->npu_scp_msg.act;
		ret_msg->arg = ctx->npu_scp_msg.arg;
		ret_msg->ret = ctx->npu_scp_msg.ret;
	}

	ret = 0;

out:
	mutex_unlock(&ctx->scp_ipi_lock);

	return ret;
}

static int npu_scp_ipi_callback(unsigned int id, void *prdata, void *data, unsigned int len)
{
	int ret = 0;
	struct apusys_aov_ctx *ctx = (struct apusys_aov_ctx *)prdata;
	struct npu_scp_ipi_param *recv_msg = (struct npu_scp_ipi_param *)data;

	if (!ctx || !recv_msg) {
		pr_info("%s failed to get context or mssage\n", __func__);
		return -EINVAL;
	}

	ctx->npu_scp_msg.cmd = recv_msg->cmd;
	ctx->npu_scp_msg.act = recv_msg->act;
	ctx->npu_scp_msg.arg = recv_msg->arg;
	ctx->npu_scp_msg.ret = recv_msg->ret;

	switch (recv_msg->cmd) {
	case NPU_SCP_RESPONSE:
		atomic_set(&ctx->response_arrived, 1);
		complete(&ctx->comp);
		ret = 0;
		break;
	case NPU_SCP_SYSTEM:
		ret = npu_system_handler(ctx, recv_msg);
		break;
	case NPU_SCP_NP_MDW:
		ret = scp_mdw_handler(recv_msg);
		break;
	case NPU_SCP_RECOVERY:
		ret = aov_recovery_handler(recv_msg);
		break;
	default:
		dev_info(ctx->dev, "%s get undefined cmd %d\n", __func__, recv_msg->cmd);
		ret = -EINVAL;
		break;
	}

	if (ret)
		dev_info(ctx->dev, "%s ERROR, cmd %d ret %d\n", __func__, recv_msg->cmd, ret);

	return 0;
}

static int apusys_aov_probe(struct platform_device *pdev)
{
	struct apusys_aov_ctx *ctx;
	int ret;

	dev_info(&pdev->dev, "%s version %s\n", __func__, APUSYS_AOV_KVERSION);

	ctx = devm_kzalloc(&pdev->dev, sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	atomic_set(&ctx->aov_enabled, 0);
	atomic_set(&ctx->response_arrived, 0);
	mutex_init(&ctx->scp_ipi_lock);

	init_completion(&ctx->comp);
	ret = mtk_ipi_register(&scp_ipidev, IPI_IN_SCP_NPU, npu_scp_ipi_callback, ctx,
			       &ctx->npu_scp_msg);
	if (ret != SCP_IPI_DONE)
		dev_info(&pdev->dev, "%s Failed to register scp ipi, ret %d\n", __func__, ret);

	ctx->dbg_dir = debugfs_create_dir("aov", apusys_dbg_root);

	ctx->dbg_node = debugfs_create_file("test", 0444, ctx->dbg_dir, ctx, &apusys_aov_test_fops);
	if (IS_ERR_OR_NULL(ctx->dbg_node)) {
		dev_info(&pdev->dev, "%s Failed to create apusys aov test node\n", __func__);
		return -ENODEV;
	}

	ctx->dev = &pdev->dev;
	platform_set_drvdata(pdev, ctx);
	aov_ctx = ctx;

	dev_info(&pdev->dev, "%s Successfully\n", __func__);

	return ret;
}

static int apusys_aov_suspend_late(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct apusys_aov_ctx *ctx = platform_get_drvdata(pdev);
	struct npu_scp_ipi_param send_msg = { 0, 0, 0, 0 };
	int ret, retry_cnt = 10;

	if (!atomic_read(&ctx->aov_enabled)) {
		dev_dbg(ctx->dev, "%s aov is disabled\n", __func__);
		return 0;
	}

	do {
		send_msg.cmd = NPU_SCP_STATE_CHANGE;
		send_msg.act = NPU_SCP_STATE_CHANGE_TO_SUSPEND;
		ret = npu_scp_ipi_send(&send_msg, NULL, STATE_TIMEOUT_MS);
		if (ret)
			dev_info(ctx->dev, "%s failed to send scp ipi, ret %d\n", __func__, ret);
	} while (ret != 0 && retry_cnt-- > 0);

	dev_info(ctx->dev, "%s send suspend done\n", __func__);

	return 0;
}

static int apusys_aov_resume_early(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct apusys_aov_ctx *ctx = platform_get_drvdata(pdev);
	struct npu_scp_ipi_param send_msg = { 0, 0, 0, 0 };
	int ret, retry_cnt = 10;

	if (!atomic_read(&ctx->aov_enabled)) {
		dev_dbg(ctx->dev, "%s aov is disabled\n", __func__);
		return 0;
	}

	do {
		send_msg.cmd = NPU_SCP_STATE_CHANGE;
		send_msg.act = NPU_SCP_STATE_CHANGE_TO_RESUME;
		ret = npu_scp_ipi_send(&send_msg, NULL, STATE_TIMEOUT_MS);
		if (ret)
			dev_info(ctx->dev, "%s failed to send scp ipi, ret %d\n", __func__, ret);
	} while (ret != 0 && retry_cnt-- > 0);

	if (ret) {
		// If SCP is not responding, then release SLB anyway.
		int slb_ref = 0;
		struct slbc_data slb_data = {
			.uid = UID_AOV_APU,
			.type = TP_BUFFER,
			.timeout = 10,
		};

		slb_ref = slbc_status(&slb_data);
		if (slb_ref > 0) {
			dev_info(ctx->dev, "%s forcibly release AOV_APU slb\n", __func__);
			ret = slbc_release(&slb_data);
			if (ret) {
				dev_info(ctx->dev, "%s Failed to release AOV_APU slb, ret %d\n",
					 __func__, ret);

			}
		}
	}

	dev_info(ctx->dev, "%s send resume done\n", __func__);

	return 0;
}

static int apusys_aov_remove(struct platform_device *pdev)
{
	struct apusys_aov_ctx *ctx = platform_get_drvdata(pdev);

	dev_info(ctx->dev, "%s +++\n", __func__);

	complete_all(&ctx->comp);

	debugfs_remove_recursive(ctx->dbg_dir);

	aov_ctx = NULL;

	dev_info(ctx->dev, "%s ---\n", __func__);

	return 0;
}

static const struct dev_pm_ops apusys_aov_pm_cb = {
	.suspend_late = apusys_aov_suspend_late,
	.resume_early = apusys_aov_resume_early,
};

static const struct of_device_id apusys_aov_of_match[] = {
	{ .compatible = "mediatek,apusys_aov", },
	{},
};
MODULE_DEVICE_TABLE(of, apusys_aov_of_match);

static struct platform_driver apusys_aov_driver = {
	.probe = apusys_aov_probe,
	.remove = apusys_aov_remove,
	.driver	= {
		.name = "apusys_aov",
		.owner = THIS_MODULE,
		.of_match_table = apusys_aov_of_match,
		.pm = &apusys_aov_pm_cb,
	},
};

int aov_init(struct apusys_core_info *info)
{
	int ret;

	if (info)
		apusys_dbg_root = info->dbg_root;

	ret = platform_driver_register(&apusys_aov_driver);
	if (ret)
		pr_info("%s driver register fail\n", __func__);

	aov_recovery_init(info);

	aov_rpmsg_init(info);

	return ret;
}

void aov_exit(void)
{
	aov_rpmsg_exit();

	aov_recovery_exit();

	platform_driver_unregister(&apusys_aov_driver);
}
