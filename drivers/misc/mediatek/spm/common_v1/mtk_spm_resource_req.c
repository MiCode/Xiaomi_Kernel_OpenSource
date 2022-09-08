// SPDX-License-Identifier: GPL-2.0

/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/spinlock.h>
#include <linux/uaccess.h>
#include <linux/module.h>
#include <mtk_spm_resource_req.h>
#include <mtk_spm_resource_req_internal.h>
#include <mtk_idle_fs/mtk_idle_sysfs.h>
#include <mtk_lp_dts.h>

#define NF_SPM_USER_USAGE_STRUCT	2

DEFINE_SPINLOCK(spm_resource_desc_update_lock);

struct spm_resource_desc {
	int id;
	unsigned int user_usage[NF_SPM_USER_USAGE_STRUCT];	/* 64 bits */
	unsigned int user_usage_mask[NF_SPM_USER_USAGE_STRUCT];   /* 64 bits */
};

static struct spm_resource_desc resc_desc[NF_SPM_RESOURCE];
static unsigned int curr_res_usage;

static const char * const spm_resource_name[] = {
	"mainpll",
	"dram   ",
	"26m    ",
	"axi_bus",
	"cpu    "
};

static struct mtk_idle_sysfs_handle spm_resource_req_file;
static unsigned int spm_console_req_status;

static int spm_resource_in_use(int resource)
{
	int i;
	int in_use = 0;

	if ((resource >= NF_SPM_RESOURCE) || (resource < 0))
		return false;

	for (i = 0; i < NF_SPM_USER_USAGE_STRUCT; i++)
		in_use |= resc_desc[resource].user_usage[i] &
			resc_desc[resource].user_usage_mask[i];

	return in_use;
}

bool spm_resource_req(unsigned int user, unsigned int req_mask)
{
	int i;
	int value = 0;
	unsigned int field = 0;
	unsigned int offset = 0;
	unsigned long flags;

	if (user >= NF_SPM_RESOURCE_USER)
		return false;

	spin_lock_irqsave(&spm_resource_desc_update_lock, flags);

	curr_res_usage = 0;

	for (i = 0; i < NF_SPM_RESOURCE; i++) {

		value = !!(req_mask & (1 << i));
		field = user / 32;
		offset = user % 32;

		if (value)
			resc_desc[i].user_usage[field] |= (1 << offset);
		else
			resc_desc[i].user_usage[field] &= ~(1 << offset);

		if (spm_resource_in_use(i))
			curr_res_usage |= (1 << i);
	}

	spin_unlock_irqrestore(&spm_resource_desc_update_lock, flags);

	return true;
}
EXPORT_SYMBOL(spm_resource_req);

unsigned int spm_get_resource_usage(void)
{
	unsigned int resource_usage;
	unsigned long flags;

	spin_lock_irqsave(&spm_resource_desc_update_lock, flags);
	resource_usage = curr_res_usage;
	spin_unlock_irqrestore(&spm_resource_desc_update_lock, flags);

	return resource_usage;
}
EXPORT_SYMBOL(spm_get_resource_usage);

unsigned int spm_get_resource_usage_by_user(unsigned int user)
{
	int i;
	unsigned long flags;
	unsigned int field = 0;
	unsigned int curr_res_usage_by_user = 0;

	spin_lock_irqsave(&spm_resource_desc_update_lock, flags);

	field = user / 32;
	for (i = 0; i < NF_SPM_RESOURCE; i++) {
		if ((resc_desc[i].user_usage_mask[field] & (1 << user)) &&
			(resc_desc[i].user_usage[field] & (1 << user))) {
			curr_res_usage_by_user |= (1 << i);
		}
	}
	spin_unlock_irqrestore(&spm_resource_desc_update_lock, flags);

	return curr_res_usage_by_user;
}
EXPORT_SYMBOL(spm_get_resource_usage_by_user);

static void spm_update_curr_resource_usage(void)
{
	int res;
	unsigned long flags;

	spin_lock_irqsave(&spm_resource_desc_update_lock, flags);

	curr_res_usage = 0;

	for (res = 0; res < NF_SPM_RESOURCE; res++) {
		if (spm_resource_in_use(res))
			curr_res_usage |= (1 << res);
	}

	spin_unlock_irqrestore(&spm_resource_desc_update_lock, flags);
}

/*
 * debugfs
 */
#define NF_CMD_BUF		128

static ssize_t resource_req_read(char *ToUserBuf, size_t sz, void *priv)
{
	int i, len = 0;
	char *p = ToUserBuf;

	p[0] = '\0';

	p += scnprintf(p, sz - strlen(ToUserBuf),
		"resource conole require = 0x%x\n", spm_console_req_status);

	for (i = 0; i < NF_SPM_RESOURCE; i++) {
		p += scnprintf(p, sz - strlen(ToUserBuf),
					"resource_req_bypass_stat[%s] = %x %x, usage %x %x\n",
					spm_resource_name[i],
					~resc_desc[i].user_usage_mask[0],
					~resc_desc[i].user_usage_mask[1],
					resc_desc[i].user_usage[0],
					resc_desc[i].user_usage[1]);
	}

	p += scnprintf(p, sz - strlen(ToUserBuf), "enable:\n");
	p += scnprintf(p, sz - strlen(ToUserBuf),
			"echo enable [bit] > /d/cpuidle/spm_resource_req\n");
	p += scnprintf(p, sz - strlen(ToUserBuf),
			"bypass:\n");
	p += scnprintf(p, sz - strlen(ToUserBuf),
			"echo bypass [bit] > /d/cpuidle/spm_resource_req\n");
	p += scnprintf(p, sz - strlen(ToUserBuf), "\n");
	p += scnprintf(p, sz - strlen(ToUserBuf),
			"[0]SPM, [1] UFS, [2] SSUSB, [3] AUDIO, [4] UART, ");
	p += scnprintf(p, sz - strlen(ToUserBuf),
			"[5] CONN, [6] MSDC, [7] SCP\n");

	len = p - ToUserBuf;

	return len;
}

static ssize_t resource_req_write(char *FromUserBuf, size_t sz, void *priv)
{
	char cmd[NF_CMD_BUF];
	int i;
	int param;
	unsigned int field = 0;
	unsigned int offset = 0;


	if (sscanf(FromUserBuf, "%127s %x", cmd, &param) == 2) {
		if (!strcmp(cmd, "enable")) {

			field = param / 32;
			offset = param % 32;

			for (i = 0; i < NF_SPM_RESOURCE; i++)
				resc_desc[i].user_usage_mask[field]
					|= (1 << offset);
			spm_update_curr_resource_usage();
		} else if (!strcmp(cmd, "bypass")) {

			field = param / 32;
			offset = param % 32;

			for (i = 0; i < NF_SPM_RESOURCE; i++)
				resc_desc[i].user_usage_mask[field]
					&= ~(1 << offset);
			spm_update_curr_resource_usage();
		}
		return sz;
	}

	return -EINVAL;
}

static const struct mtk_idle_sysfs_op resource_req_fops = {
	.fs_read = resource_req_read,
	.fs_write = resource_req_write,
};

void spm_resource_req_debugfs_init(void)
{
	mtk_idle_sysfs_entry_node_add("spm_resource_req", 0444
			, &resource_req_fops, &spm_resource_req_file);
}

bool spm_resource_req_init(void)
{
	int i, k;

	for (i = 0; i < NF_SPM_RESOURCE; i++) {
		resc_desc[i].id = i;

		for (k = 0; k < NF_SPM_USER_USAGE_STRUCT; k++) {
			resc_desc[i].user_usage[k] = 0;
			resc_desc[i].user_usage_mask[k] = 0xFFFFFFFF;
		}
	}

	return true;
}

/* Debug only */
void spm_resource_req_dump(void)
{
	int i;
	unsigned long flags;

	printk_deferred("[name:spm&]resource_req:\n");

	spin_lock_irqsave(&spm_resource_desc_update_lock, flags);

	for (i = 0; i < NF_SPM_RESOURCE; i++)
		printk_deferred("[name:spm&][%s]: 0x%x, 0x%x, mask = 0x%x, 0x%x\n",
				spm_resource_name[i],
				resc_desc[i].user_usage[0],
				resc_desc[i].user_usage[1],
				resc_desc[i].user_usage_mask[0],
				resc_desc[i].user_usage_mask[1]);

	spin_unlock_irqrestore(&spm_resource_desc_update_lock, flags);
}

void spm_resource_req_block_dump(void)
{
	unsigned long flags;

	spin_lock_irqsave(&spm_resource_desc_update_lock, flags);

	if (curr_res_usage == SPM_RESOURCE_ALL) {
		printk_deferred("[name:spm&][resource_req_block] user: 0x%x, 0x%x\n",
				resc_desc[0].user_usage[0],
				resc_desc[0].user_usage[1]);
	}

	spin_unlock_irqrestore(&spm_resource_desc_update_lock, flags);
}

unsigned int spm_resource_console_dts_required(
	struct spm_resource_console_req *req, int count)
{
	struct device_node *spm_node = NULL;
	int res_req_bits = 0;

	do {
		if (!req || (count <= 0))
			break;

		spm_node = GET_MTK_SPM_DTS_NODE();

		if (spm_node) {
			u32 PropValue;
			int prop_result;
			int index = 0;

			for (index = 0; (index < count) && req
				&& req[index].name; index++) {

				prop_result = of_property_read_u32(
					spm_node, req[index].name, &PropValue);

				if ((prop_result == 0) && (PropValue > 0))
					res_req_bits |=
						(1<<req[index].bit_number);
			}

			of_node_put(spm_node);
		}
		spm_console_req_status = res_req_bits;
	} while (0);

	return res_req_bits;
}
EXPORT_SYMBOL(spm_resource_console_dts_required);
MODULE_LICENSE("GPL");

