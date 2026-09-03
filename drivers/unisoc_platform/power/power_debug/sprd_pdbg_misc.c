// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2017 Spreadtrum Communications Inc.
 */
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include "sprd_pdbg_comm.h"
#include "sprd_pdbg_misc.h"

#define WS_CHECK_INTERNAL_MS (msecs_to_jiffies(500))

static bool sprd_pdbg_bootmode_check(const char *str)
{
	struct device_node *mode;
	const char *cmd_line;
	int rc;

	mode = of_find_node_by_path("/chosen");
	rc = of_property_read_string(mode, "bootargs", &cmd_line);
	if (rc)
		return false;

	if (!strstr(cmd_line, str))
		return false;

	return true;
}

static void pdbg_engpc_ws_work(struct work_struct *work)
{
	struct misc_data *data =
		container_of(work, struct misc_data, engpc_ws_check_work.work);

	if (!data->engpc_boot_mode)
		return;

	sprd_pdbg_kernel_active_ws_show();
	queue_delayed_work(system_highpri_wq, &data->engpc_ws_check_work, WS_CHECK_INTERNAL_MS);
}

static ssize_t engpc_read(struct file *file, char __user *in_buf, size_t count, loff_t *ppos)
{
	char out_buf[5];
	size_t len;
	struct misc_data *data = PDE_DATA(file_inode(file));

	len = sprintf(out_buf, "%d\n", data->engpc_deep_en);
	return simple_read_from_buffer(in_buf, count, ppos, out_buf, len);
}

static ssize_t engpc_write(struct file *file, const char __user *user_buf, size_t count,
			     loff_t *ppos)
{
	int ret;
	bool enable;
	struct misc_data *data = PDE_DATA(file_inode(file));

	if (*ppos < 0)
		return -EINVAL;

	if (count == 0)
		return 0;

	if (*ppos != 0)
		return 0;

	ret = kstrtobool_from_user(user_buf, count, &enable);
	if (ret)
		return -EINVAL;

	data->engpc_deep_en = enable;
	if (data->engpc_deep_en)
		queue_delayed_work(system_highpri_wq, &data->engpc_ws_check_work, 0);
	else
		cancel_delayed_work_sync(&data->engpc_ws_check_work);

	return count;
}


static const struct proc_ops misc_engpc_fops = {
	.proc_open	= simple_open,
	.proc_read	= engpc_read,
	.proc_write	= engpc_write,
	.proc_lseek	= default_llseek,
};

static ssize_t apsys_pd_disable_read(struct file *file, char *in_buf, size_t count, loff_t *ppos)
{
	char out_buf[5];
	size_t len;
	u32 apsys_pd_disable = 0;
	struct arm_smccc_res ret_vals = {0};

	if (sprd_pdbg_misc_trans(PDBG_MISC_APSYS_PD_GET, 0, &ret_vals)) {
		SPRD_PDBG_ERR("apsys_pd_disable proc get failed\n");
		return -EINVAL;
	}

	apsys_pd_disable = !!ret_vals.a0;
	len = sprintf(out_buf, "%u\n", apsys_pd_disable);
	return simple_read_from_buffer(in_buf, count, ppos, out_buf, len);
}

static ssize_t apsys_pd_disable_write(struct file *file, const char *user_buf, size_t count,
					     loff_t *ppos)
{
	int ret;
	bool enable;

	if (*ppos < 0)
		return -EINVAL;

	if (count == 0)
		return 0;

	if (*ppos != 0)
		return 0;

	ret = kstrtobool_from_user(user_buf, count, &enable);
	if (ret)
		return -EINVAL;

	if (sprd_pdbg_misc_trans(PDBG_MISC_APSYS_PD_SET, enable, NULL)) {
		SPRD_PDBG_ERR("apsys_pd_disable proc set failed\n");
		return -EINVAL;
	}

	return count;
}


static const struct proc_ops apsys_pd_fops = {
	.proc_open	= simple_open,
	.proc_read	= apsys_pd_disable_read,
	.proc_write	= apsys_pd_disable_write,
	.proc_lseek	= default_llseek,
};

static int pdbg_misc_proc_init(struct misc_data *data, struct proc_dir_entry *dir)
{
	struct proc_dir_entry *fle;

	fle = proc_create_data("apsys_pd_disable", 0644, dir, &apsys_pd_fops, data);
	if (!fle) {
		SPRD_PDBG_ERR("Proc apsys_pd fops  file create failed\n");
		return -EINVAL;
	}

	if (data->engpc_boot_mode) {
		fle = proc_create_data("engpc_deep_en", 0644, dir, &misc_engpc_fops, data);
		if (!fle) {
			SPRD_PDBG_ERR("Proc engpc_deep_en fops  file create failed\n");
			return -EINVAL;
		}
	}

	return 0;
}

static void misc_engpc_devm_action(void *_data)
{
	struct misc_data *data = _data;

	cancel_delayed_work_sync(&data->engpc_ws_check_work);
}

static int pdbg_engpc_init(struct device *dev, struct misc_data *data)
{
	int ret = -1;

	data->engpc_boot_mode = (sprd_pdbg_bootmode_check("sprdboot.mode=cali")
		|| sprd_pdbg_bootmode_check("sprdboot.mode=autotest"));

	if (!data->engpc_boot_mode)
		return 0;

	INIT_DELAYED_WORK(&data->engpc_ws_check_work, pdbg_engpc_ws_work);
	sprd_pdbg_log_force = 1;

	ret = devm_add_action(dev, misc_engpc_devm_action, data);
	if (ret) {
		misc_engpc_devm_action(data);
		SPRD_PDBG_ERR("failed to add misc_engpc_devm_action\n");
		return ret;
	}

	return 0;
}

int sprd_pdbg_misc_init(struct device *dev, struct proc_dir_entry *dir, struct misc_data **_data)
{
	struct misc_data *data;
	int ret;

	data = devm_kzalloc(dev, sizeof(struct misc_data), GFP_KERNEL);
	if (!data) {
		SPRD_PDBG_ERR("%s: ws_data alloc error\n", __func__);
		return -ENOMEM;
	}

	ret = pdbg_engpc_init(dev, data);
	if (ret) {
		SPRD_PDBG_ERR("%s: pdbg_engpc_init error\n", __func__);
		return ret;
	}

	ret = pdbg_misc_proc_init(data, dir);
	if (ret) {
		SPRD_PDBG_ERR("%s: pdbg_misc_proc_init error\n", __func__);
		return ret;
	}

	*_data = data;

	return 0;
}

