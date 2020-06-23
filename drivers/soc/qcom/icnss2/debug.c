// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2015-2020, The Linux Foundation. All rights reserved.
 */
#include <linux/err.h>
#include <linux/seq_file.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include "main.h"
#include "debug.h"
#include "qmi.h"
#include "power.h"

void *icnss_ipc_log_context;
void *icnss_ipc_log_long_context;

static ssize_t icnss_regwrite_write(struct file *fp,
				    const char __user *user_buf,
				    size_t count, loff_t *off)
{
	struct icnss_priv *priv =
		((struct seq_file *)fp->private_data)->private;
	char buf[64];
	char *sptr, *token;
	unsigned int len = 0;
	uint32_t reg_offset, mem_type, reg_val;
	const char *delim = " ";
	int ret = 0;

	if (!test_bit(ICNSS_FW_READY, &priv->state) ||
	    !test_bit(ICNSS_POWER_ON, &priv->state))
		return -EINVAL;

	len = min(count, sizeof(buf) - 1);
	if (copy_from_user(buf, user_buf, len))
		return -EFAULT;

	buf[len] = '\0';
	sptr = buf;

	token = strsep(&sptr, delim);
	if (!token)
		return -EINVAL;

	if (!sptr)
		return -EINVAL;

	if (kstrtou32(token, 0, &mem_type))
		return -EINVAL;

	token = strsep(&sptr, delim);
	if (!token)
		return -EINVAL;

	if (!sptr)
		return -EINVAL;

	if (kstrtou32(token, 0, &reg_offset))
		return -EINVAL;

	token = strsep(&sptr, delim);
	if (!token)
		return -EINVAL;

	if (kstrtou32(token, 0, &reg_val))
		return -EINVAL;

	ret = wlfw_athdiag_write_send_sync_msg(priv, reg_offset, mem_type,
					       sizeof(uint32_t),
					       (uint8_t *)&reg_val);
	if (ret)
		return ret;

	return count;
}

static int icnss_regwrite_show(struct seq_file *s, void *data)
{
	struct icnss_priv *priv = s->private;

	seq_puts(s, "Usage: echo <mem_type> <offset> <reg_val> > <debugfs>/icnss/reg_write\n");

	if (!test_bit(ICNSS_FW_READY, &priv->state))
		seq_puts(s, "Firmware is not ready yet!, wait for FW READY\n");

	return 0;
}

static int icnss_regwrite_open(struct inode *inode, struct file *file)
{
	return single_open(file, icnss_regwrite_show, inode->i_private);
}

static const struct file_operations icnss_regwrite_fops = {
	.read		= seq_read,
	.write          = icnss_regwrite_write,
	.open           = icnss_regwrite_open,
	.owner          = THIS_MODULE,
	.llseek		= seq_lseek,
};

static int icnss_regread_show(struct seq_file *s, void *data)
{
	struct icnss_priv *priv = s->private;

	mutex_lock(&priv->dev_lock);
	if (!priv->diag_reg_read_buf) {
		seq_puts(s, "Usage: echo <mem_type> <offset> <data_len> > <debugfs>/icnss/reg_read\n");

		if (!test_bit(ICNSS_FW_READY, &priv->state))
			seq_puts(s, "Firmware is not ready yet!, wait for FW READY\n");

		mutex_unlock(&priv->dev_lock);
		return 0;
	}

	seq_printf(s, "REGREAD: Addr 0x%x Type 0x%x Length 0x%x\n",
		   priv->diag_reg_read_addr, priv->diag_reg_read_mem_type,
		   priv->diag_reg_read_len);

	seq_hex_dump(s, "", DUMP_PREFIX_OFFSET, 32, 4, priv->diag_reg_read_buf,
		     priv->diag_reg_read_len, false);

	priv->diag_reg_read_len = 0;
	kfree(priv->diag_reg_read_buf);
	priv->diag_reg_read_buf = NULL;
	mutex_unlock(&priv->dev_lock);

	return 0;
}

static int icnss_regread_open(struct inode *inode, struct file *file)
{
	return single_open(file, icnss_regread_show, inode->i_private);
}

static ssize_t icnss_reg_parse(const char __user *user_buf, size_t count,
			       struct icnss_reg_info *reg_info_ptr)
{
	char buf[64] = {0};
	char *sptr = NULL, *token = NULL;
	const char *delim = " ";
	unsigned int len = 0;

	if (user_buf == NULL)
		return -EFAULT;

	len = min(count, sizeof(buf) - 1);
	if (copy_from_user(buf, user_buf, len))
		return -EFAULT;

	buf[len] = '\0';
	sptr = buf;

	token = strsep(&sptr, delim);
	if (!token)
		return -EINVAL;

	if (!sptr)
		return -EINVAL;

	if (kstrtou32(token, 0, &reg_info_ptr->mem_type))
		return -EINVAL;

	token = strsep(&sptr, delim);
	if (!token)
		return -EINVAL;

	if (!sptr)
		return -EINVAL;

	if (kstrtou32(token, 0, &reg_info_ptr->reg_offset))
		return -EINVAL;

	token = strsep(&sptr, delim);
	if (!token)
		return -EINVAL;

	if (kstrtou32(token, 0, &reg_info_ptr->data_len))
		return -EINVAL;

	if (reg_info_ptr->data_len == 0 ||
	    reg_info_ptr->data_len > WLFW_MAX_DATA_SIZE)
		return -EINVAL;

	return 0;
}

static ssize_t icnss_regread_write(struct file *fp, const char __user *user_buf,
				   size_t count, loff_t *off)
{
	struct icnss_priv *priv =
		((struct seq_file *)fp->private_data)->private;
	uint8_t *reg_buf = NULL;
	int ret = 0;
	struct icnss_reg_info reg_info;

	if (!test_bit(ICNSS_FW_READY, &priv->state) ||
	    !test_bit(ICNSS_POWER_ON, &priv->state))
		return -EINVAL;

	ret = icnss_reg_parse(user_buf, count, &reg_info);
	if (ret)
		return ret;

	mutex_lock(&priv->dev_lock);
	kfree(priv->diag_reg_read_buf);
	priv->diag_reg_read_buf = NULL;

	reg_buf = kzalloc(reg_info.data_len, GFP_KERNEL);
	if (!reg_buf) {
		mutex_unlock(&priv->dev_lock);
		return -ENOMEM;
	}

	ret = wlfw_athdiag_read_send_sync_msg(priv, reg_info.reg_offset,
					      reg_info.mem_type,
					      reg_info.data_len,
					      reg_buf);
	if (ret) {
		kfree(reg_buf);
		mutex_unlock(&priv->dev_lock);
		return ret;
	}

	priv->diag_reg_read_addr = reg_info.reg_offset;
	priv->diag_reg_read_mem_type = reg_info.mem_type;
	priv->diag_reg_read_len = reg_info.data_len;
	priv->diag_reg_read_buf = reg_buf;
	mutex_unlock(&priv->dev_lock);

	return count;
}

static const struct file_operations icnss_regread_fops = {
	.read           = seq_read,
	.write          = icnss_regread_write,
	.open           = icnss_regread_open,
	.owner          = THIS_MODULE,
	.llseek         = seq_lseek,
};

static ssize_t icnss_stats_write(struct file *fp, const char __user *buf,
				size_t count, loff_t *off)
{
	struct icnss_priv *priv =
		((struct seq_file *)fp->private_data)->private;
	int ret;
	u32 val;

	ret = kstrtou32_from_user(buf, count, 0, &val);
	if (ret)
		return ret;

	if (ret == 0)
		memset(&priv->stats, 0, sizeof(priv->stats));

	return count;
}

static int icnss_stats_show_rejuvenate_info(struct seq_file *s,
					    struct icnss_priv *priv)
{
	if (priv->stats.rejuvenate_ind)  {
		seq_puts(s, "\n<---------------- Rejuvenate Info ----------------->\n");
		seq_printf(s, "Number of Rejuvenations: %u\n",
			   priv->stats.rejuvenate_ind);
		seq_printf(s, "Cause for Rejuvenation: 0x%x\n",
			   priv->cause_for_rejuvenation);
		seq_printf(s, "Requesting Sub-System: 0x%x\n",
			   priv->requesting_sub_system);
		seq_printf(s, "Line Number: %u\n",
			   priv->line_number);
		seq_printf(s, "Function Name: %s\n",
			   priv->function_name);
	}

	return 0;
}

static int icnss_stats_show_irqs(struct seq_file *s, struct icnss_priv *priv)
{
	int i;

	seq_puts(s, "\n<------------------ IRQ stats ------------------->\n");
	seq_printf(s, "%4s %4s %8s %8s %8s %8s\n", "CE_ID", "IRQ", "Request",
		   "Free", "Enable", "Disable");
	for (i = 0; i < ICNSS_MAX_IRQ_REGISTRATIONS; i++)
		seq_printf(s, "%4d: %4u %8u %8u %8u %8u\n", i,
			   priv->ce_irqs[i], priv->stats.ce_irqs[i].request,
			   priv->stats.ce_irqs[i].free,
			   priv->stats.ce_irqs[i].enable,
			   priv->stats.ce_irqs[i].disable);

	return 0;
}

static int icnss_stats_show_capability(struct seq_file *s,
				       struct icnss_priv *priv)
{
	if (test_bit(ICNSS_FW_READY, &priv->state)) {
		seq_puts(s, "\n<---------------- FW Capability ----------------->\n");
		seq_printf(s, "Chip ID: 0x%x\n", priv->chip_info.chip_id);
		seq_printf(s, "Chip family: 0x%x\n",
			  priv->chip_info.chip_family);
		seq_printf(s, "Board ID: 0x%x\n", priv->board_id);
		seq_printf(s, "SOC Info: 0x%x\n", priv->soc_id);
		seq_printf(s, "Firmware Version: 0x%x\n",
			   priv->fw_version_info.fw_version);
		seq_printf(s, "Firmware Build Timestamp: %s\n",
			   priv->fw_version_info.fw_build_timestamp);
		seq_printf(s, "Firmware Build ID: %s\n",
			   priv->fw_build_id);
	}

	return 0;
}

static int icnss_stats_show_events(struct seq_file *s, struct icnss_priv *priv)
{
	int i;

	seq_puts(s, "\n<----------------- Events stats ------------------->\n");
	seq_printf(s, "%24s %16s %16s\n", "Events", "Posted", "Processed");
	for (i = 0; i < ICNSS_DRIVER_EVENT_MAX; i++)
		seq_printf(s, "%24s %16u %16u\n",
			   icnss_driver_event_to_str(i),
			   priv->stats.events[i].posted,
			   priv->stats.events[i].processed);

	return 0;
}

static int icnss_stats_show_state(struct seq_file *s, struct icnss_priv *priv)
{
	enum icnss_driver_state i;
	int skip = 0;
	unsigned long state;

	seq_printf(s, "\nState: 0x%lx(", priv->state);
	for (i = 0, state = priv->state; state != 0; state >>= 1, i++) {

		if (!(state & 0x1))
			continue;

		if (skip++)
			seq_puts(s, " | ");

		switch (i) {
		case ICNSS_WLFW_CONNECTED:
			seq_puts(s, "FW CONN");
			continue;
		case ICNSS_POWER_ON:
			seq_puts(s, "POWER ON");
			continue;
		case ICNSS_FW_READY:
			seq_puts(s, "FW READY");
			continue;
		case ICNSS_DRIVER_PROBED:
			seq_puts(s, "DRIVER PROBED");
			continue;
		case ICNSS_FW_TEST_MODE:
			seq_puts(s, "FW TEST MODE");
			continue;
		case ICNSS_PM_SUSPEND:
			seq_puts(s, "PM SUSPEND");
			continue;
		case ICNSS_PM_SUSPEND_NOIRQ:
			seq_puts(s, "PM SUSPEND NOIRQ");
			continue;
		case ICNSS_SSR_REGISTERED:
			seq_puts(s, "SSR REGISTERED");
			continue;
		case ICNSS_PDR_REGISTERED:
			seq_puts(s, "PDR REGISTERED");
			continue;
		case ICNSS_PD_RESTART:
			seq_puts(s, "PD RESTART");
			continue;
		case ICNSS_WLFW_EXISTS:
			seq_puts(s, "WLAN FW EXISTS");
			continue;
		case ICNSS_SHUTDOWN_DONE:
			seq_puts(s, "SHUTDOWN DONE");
			continue;
		case ICNSS_HOST_TRIGGERED_PDR:
			seq_puts(s, "HOST TRIGGERED PDR");
			continue;
		case ICNSS_FW_DOWN:
			seq_puts(s, "FW DOWN");
			continue;
		case ICNSS_DRIVER_UNLOADING:
			seq_puts(s, "DRIVER UNLOADING");
			continue;
		case ICNSS_REJUVENATE:
			seq_puts(s, "FW REJUVENATE");
			continue;
		case ICNSS_MODE_ON:
			seq_puts(s, "MODE ON DONE");
			continue;
		case ICNSS_BLOCK_SHUTDOWN:
			seq_puts(s, "BLOCK SHUTDOWN");
			continue;
		case ICNSS_PDR:
			seq_puts(s, "PDR TRIGGERED");
			continue;
		case ICNSS_DEL_SERVER:
			seq_puts(s, "DEL SERVER");
		}

		seq_printf(s, "UNKNOWN-%d", i);
		}
	seq_puts(s, ")\n");

	return 0;
}

#define ICNSS_STATS_DUMP(_s, _priv, _x) \
	seq_printf(_s, "%24s: %u\n", #_x, _priv->stats._x)

static int icnss_stats_show(struct seq_file *s, void *data)
{

	struct icnss_priv *priv = s->private;

	ICNSS_STATS_DUMP(s, priv, ind_register_req);
	ICNSS_STATS_DUMP(s, priv, ind_register_resp);
	ICNSS_STATS_DUMP(s, priv, ind_register_err);
	ICNSS_STATS_DUMP(s, priv, cap_req);
	ICNSS_STATS_DUMP(s, priv, cap_resp);
	ICNSS_STATS_DUMP(s, priv, cap_err);
	ICNSS_STATS_DUMP(s, priv, pin_connect_result);
	ICNSS_STATS_DUMP(s, priv, cfg_req);
	ICNSS_STATS_DUMP(s, priv, cfg_resp);
	ICNSS_STATS_DUMP(s, priv, cfg_req_err);
	ICNSS_STATS_DUMP(s, priv, mode_req);
	ICNSS_STATS_DUMP(s, priv, mode_resp);
	ICNSS_STATS_DUMP(s, priv, mode_req_err);
	ICNSS_STATS_DUMP(s, priv, ini_req);
	ICNSS_STATS_DUMP(s, priv, ini_resp);
	ICNSS_STATS_DUMP(s, priv, ini_req_err);
	ICNSS_STATS_DUMP(s, priv, recovery.pdr_fw_crash);
	ICNSS_STATS_DUMP(s, priv, recovery.pdr_host_error);
	ICNSS_STATS_DUMP(s, priv, recovery.root_pd_crash);
	ICNSS_STATS_DUMP(s, priv, recovery.root_pd_shutdown);

	seq_puts(s, "\n<------------------ PM stats ------------------->\n");
	ICNSS_STATS_DUMP(s, priv, pm_suspend);
	ICNSS_STATS_DUMP(s, priv, pm_suspend_err);
	ICNSS_STATS_DUMP(s, priv, pm_resume);
	ICNSS_STATS_DUMP(s, priv, pm_resume_err);
	ICNSS_STATS_DUMP(s, priv, pm_suspend_noirq);
	ICNSS_STATS_DUMP(s, priv, pm_suspend_noirq_err);
	ICNSS_STATS_DUMP(s, priv, pm_resume_noirq);
	ICNSS_STATS_DUMP(s, priv, pm_resume_noirq_err);
	ICNSS_STATS_DUMP(s, priv, pm_stay_awake);
	ICNSS_STATS_DUMP(s, priv, pm_relax);

	if (priv->device_id != WCN6750_DEVICE_ID) {
		seq_puts(s, "\n<------------------ MSA stats ------------------->\n");
		ICNSS_STATS_DUMP(s, priv, msa_info_req);
		ICNSS_STATS_DUMP(s, priv, msa_info_resp);
		ICNSS_STATS_DUMP(s, priv, msa_info_err);
		ICNSS_STATS_DUMP(s, priv, msa_ready_req);
		ICNSS_STATS_DUMP(s, priv, msa_ready_resp);
		ICNSS_STATS_DUMP(s, priv, msa_ready_err);
		ICNSS_STATS_DUMP(s, priv, msa_ready_ind);

		seq_puts(s, "\n<------------------ Rejuvenate stats ------------------->\n");
		ICNSS_STATS_DUMP(s, priv, rejuvenate_ind);
		ICNSS_STATS_DUMP(s, priv, rejuvenate_ack_req);
		ICNSS_STATS_DUMP(s, priv, rejuvenate_ack_resp);
		ICNSS_STATS_DUMP(s, priv, rejuvenate_ack_err);
		icnss_stats_show_rejuvenate_info(s, priv);

	}

	icnss_stats_show_irqs(s, priv);

	icnss_stats_show_capability(s, priv);

	icnss_stats_show_events(s, priv);

	icnss_stats_show_state(s, priv);

	return 0;
}

static int icnss_stats_open(struct inode *inode, struct file *file)
{
	return single_open(file, icnss_stats_show, inode->i_private);
}

static const struct file_operations icnss_stats_fops = {
	.read		= seq_read,
	.write		= icnss_stats_write,
	.release	= single_release,
	.open		= icnss_stats_open,
	.owner		= THIS_MODULE,
	.llseek		= seq_lseek,
};

static int icnss_fw_debug_show(struct seq_file *s, void *data)
{
	struct icnss_priv *priv = s->private;

	seq_puts(s, "\nUsage: echo <CMD> <VAL> > <DEBUGFS>/icnss/fw_debug\n");

	seq_puts(s, "\nCMD: test_mode\n");
	seq_puts(s, "  VAL: 0 (Test mode disable)\n");
	seq_puts(s, "  VAL: 1 (WLAN FW test)\n");
	seq_puts(s, "  VAL: 2 (CCPM test)\n");
	seq_puts(s, "  VAL: 3 (Trigger Recovery)\n");
	seq_puts(s, "  VAL: 4 (allow recursive recovery)\n");
	seq_puts(s, "  VAL: 3 (Disallow recursive recovery)\n");

	seq_puts(s, "\nCMD: dynamic_feature_mask\n");
	seq_puts(s, "  VAL: (64 bit feature mask)\n");

	if (!test_bit(ICNSS_FW_READY, &priv->state)) {
		seq_puts(s, "Firmware is not ready yet, can't run test_mode!\n");
		goto out;
	}

	if (test_bit(ICNSS_DRIVER_PROBED, &priv->state)) {
		seq_puts(s, "Machine mode is running, can't run test_mode!\n");
		goto out;
	}

	if (test_bit(ICNSS_FW_TEST_MODE, &priv->state)) {
		seq_puts(s, "test_mode is running, can't run test_mode!\n");
		goto out;
	}

out:
	seq_puts(s, "\n");
	return 0;
}

static int icnss_test_mode_fw_test_off(struct icnss_priv *priv)
{
	int ret;

	if (!test_bit(ICNSS_FW_READY, &priv->state)) {
		icnss_pr_err("Firmware is not ready yet!, wait for FW READY: state: 0x%lx\n",
			     priv->state);
			ret = -ENODEV;
			goto out;
	}

	if (test_bit(ICNSS_DRIVER_PROBED, &priv->state)) {
		icnss_pr_err("Machine mode is running, can't run test mode: state: 0x%lx\n",
			     priv->state);
			ret = -EINVAL;
			goto out;
	}

	if (!test_bit(ICNSS_FW_TEST_MODE, &priv->state)) {
		icnss_pr_err("Test mode not started, state: 0x%lx\n",
			     priv->state);
		ret = -EINVAL;
		goto out;
	}

	icnss_wlan_disable(&priv->pdev->dev, ICNSS_OFF);

	ret = icnss_hw_power_off(priv);

	clear_bit(ICNSS_FW_TEST_MODE, &priv->state);

out:
	return ret;
}

static int icnss_test_mode_fw_test(struct icnss_priv *priv,
				   enum icnss_driver_mode mode)
{
	int ret;

	if (!test_bit(ICNSS_FW_READY, &priv->state)) {
		icnss_pr_err("Firmware is not ready yet!, wait for FW READY, state: 0x%lx\n",
			     priv->state);
			ret = -ENODEV;
			goto out;
	}

	if (test_bit(ICNSS_DRIVER_PROBED, &priv->state)) {
		icnss_pr_err("Machine mode is running, can't run test mode, state: 0x%lx\n",
			     priv->state);
		ret = -EINVAL;
		goto out;
	}

	if (test_bit(ICNSS_FW_TEST_MODE, &priv->state)) {
		icnss_pr_err("Test mode already started, state: 0x%lx\n",
			     priv->state);
		ret = -EBUSY;
		goto out;
	}

	ret = icnss_hw_power_on(priv);
	if (ret)
		goto out;

	set_bit(ICNSS_FW_TEST_MODE, &priv->state);

	ret = icnss_wlan_enable(&priv->pdev->dev, NULL, mode, NULL);
	if (ret)
		goto power_off;

	return 0;

power_off:
	icnss_hw_power_off(priv);
	clear_bit(ICNSS_FW_TEST_MODE, &priv->state);

out:
	return ret;
}


static ssize_t icnss_fw_debug_write(struct file *fp,
				    const char __user *user_buf,
				    size_t count, loff_t *off)
{
	struct icnss_priv *priv =
		((struct seq_file *)fp->private_data)->private;
	char buf[64];
	char *sptr, *token;
	unsigned int len = 0;
	char *cmd;
	uint64_t val;
	const char *delim = " ";
	int ret = 0;

	len = min(count, sizeof(buf) - 1);
	if (copy_from_user(buf, user_buf, len))
		return -EINVAL;

	buf[len] = '\0';
	sptr = buf;

	token = strsep(&sptr, delim);
	if (!token)
		return -EINVAL;
	if (!sptr)
		return -EINVAL;
	cmd = token;

	token = strsep(&sptr, delim);
	if (!token)
		return -EINVAL;
	if (kstrtou64(token, 0, &val))
		return -EINVAL;

	if (strcmp(cmd, "test_mode") == 0) {
		switch (val) {
		case 0:
			ret = icnss_test_mode_fw_test_off(priv);
			break;
		case 1:
			ret = icnss_test_mode_fw_test(priv, ICNSS_WALTEST);
			break;
		case 2:
			ret = icnss_test_mode_fw_test(priv, ICNSS_CCPM);
			break;
		case 3:
			ret = icnss_trigger_recovery(&priv->pdev->dev);
			break;
		case 4:
			icnss_allow_recursive_recovery(&priv->pdev->dev);
			break;
		case 5:
			icnss_disallow_recursive_recovery(&priv->pdev->dev);
			break;
		default:
			return -EINVAL;
		}
	} else if (strcmp(cmd, "dynamic_feature_mask") == 0) {
		ret = wlfw_dynamic_feature_mask_send_sync_msg(priv, val);
	} else {
		return -EINVAL;
	}

	if (ret)
		return ret;

	return count;
}

static int icnss_fw_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, icnss_fw_debug_show, inode->i_private);
}

static const struct file_operations icnss_fw_debug_fops = {
	.read		= seq_read,
	.write		= icnss_fw_debug_write,
	.release	= single_release,
	.open		= icnss_fw_debug_open,
	.owner		= THIS_MODULE,
	.llseek		= seq_lseek,
};
#ifdef CONFIG_ICNSS2_DEBUG
int icnss_debugfs_create(struct icnss_priv *priv)
{
	int ret = 0;
	struct dentry *root_dentry;

	root_dentry = debugfs_create_dir("icnss", NULL);

	if (IS_ERR(root_dentry)) {
		ret = PTR_ERR(root_dentry);
		icnss_pr_err("Unable to create debugfs %d\n", ret);
		goto out;
		}

		priv->root_dentry = root_dentry;

		debugfs_create_file("fw_debug", 0600, root_dentry, priv,
					&icnss_fw_debug_fops);
		debugfs_create_file("stats", 0600, root_dentry, priv,
						&icnss_stats_fops);
		debugfs_create_file("reg_read", 0600, root_dentry, priv,
						&icnss_regread_fops);
		debugfs_create_file("reg_write", 0600, root_dentry, priv,
						&icnss_regwrite_fops);
out:
		return ret;
}
#else
int icnss_debugfs_create(struct icnss_priv *priv)
{
	int ret = 0;
	struct dentry *root_dentry;

	root_dentry = debugfs_create_dir("icnss", NULL);

	if (IS_ERR(root_dentry)) {
		ret = PTR_ERR(root_dentry);
		icnss_pr_err("Unable to create debugfs %d\n", ret);
		return ret;
	}

	priv->root_dentry = root_dentry;

	debugfs_create_file("stats", 0600, root_dentry, priv,
							     &icnss_stats_fops);
	return 0;
}
#endif

void icnss_debugfs_destroy(struct icnss_priv *priv)
{
	debugfs_remove_recursive(priv->root_dentry);
}

void icnss_debug_init(void)
{
	icnss_ipc_log_context = ipc_log_context_create(NUM_LOG_PAGES,
						       "icnss", 0);
	if (!icnss_ipc_log_context)
		icnss_pr_err("Unable to create log context\n");

	icnss_ipc_log_long_context = ipc_log_context_create(NUM_LOG_LONG_PAGES,
						       "icnss_long", 0);
	if (!icnss_ipc_log_long_context)
		icnss_pr_err("Unable to create log long context\n");
}

void icnss_debug_deinit(void)
{
	if (icnss_ipc_log_context) {
		ipc_log_context_destroy(icnss_ipc_log_context);
		icnss_ipc_log_context = NULL;
	}

	if (icnss_ipc_log_long_context) {
		ipc_log_context_destroy(icnss_ipc_log_long_context);
		icnss_ipc_log_long_context = NULL;
	}
}
