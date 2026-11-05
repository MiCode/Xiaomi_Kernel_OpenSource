// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */

#define pr_fmt(fmt)	"CHARGER_ULOG: %s: " fmt, __func__

#include <linux/debugfs.h>
#include <linux/device.h>
#include <linux/ipc_logging.h>
#include <linux/ktime.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/rpmsg.h>
#include <linux/slab.h>
#include <linux/soc/qcom/qti_pmic_glink.h>
#include <linux/mca/common/mca_log.h>

#define MSG_OWNER_CHG_ULOG		32778
#define MSG_TYPE_REQ_RESP		1
#define GET_CHG_ULOG_REQ		0x18
#define SET_CHG_ULOG_PROP_REQ		0x19
#define GET_CHG_INIT_ULOG_REQ		0x23
#define GET_MCA_LOG_REQ				0x52
#define GET_MCA_REALTIME_LOG_REQ	0x53

#define LOG_CATEGORY_INIT		(1ULL << 32)
#define LOG_MIN_TIME_MS			500
#define LOG_DEFAULT_TIME_MS		1000

#define MAX_ULOG_SIZE			8192
#define NUM_LOG_PAGES			10
#define NUM_INIT_LOG_PAGES		8
#define MAX_ADSP_LOG_SIZE		7680
#define SINGLE_LOG_SIZE			256
#define MAX_LOG_PR_SIZE			512

#ifndef MCA_LOG_TAG
#define MCA_LOG_TAG "charger_ulog_glink"
#endif

struct set_ulog_prop_req_msg {
	struct pmic_glink_hdr		hdr;
	u64				log_category;
	u32				log_level;
};

struct get_ulog_req_msg {
	struct pmic_glink_hdr		hdr;
	u32				log_size;
};

struct get_ulog_resp_msg {
	struct pmic_glink_hdr		hdr;
	u8				buf[MAX_ULOG_SIZE];
};

struct get_adsp_log_msg {
	struct pmic_glink_hdr hdr;
	char log_info[MAX_ADSP_LOG_SIZE];
	int cur_pos;
	u64 cur_time;
};

struct adsp_realtime_log_msg {
	struct pmic_glink_hdr hdr;
	char log_info[SINGLE_LOG_SIZE];
};

struct chg_ulog_glink_dev {
	struct device			*dev;
	struct pmic_glink_client	*client;
	struct dentry			*debugfs_dir;
	void				*ipc_log;
	void				*ipc_init_log;
	struct mutex			lock;
	struct completion		ack;
	struct delayed_work		ulog_work;
	struct work_struct		adsp_dump_log_work;
	u8				ulog_buf[MAX_ULOG_SIZE];
	u64				log_category;
	u32				log_level;
	u32				log_time_ms;
	bool				log_enable;
	bool				init_log_enable;
};

#define WAIT_TIME_MS			1000
static int chg_ulog_write(struct chg_ulog_glink_dev *cd, void *data,
				size_t len)
{
	int rc;

	mutex_lock(&cd->lock);
	reinit_completion(&cd->ack);
	rc = pmic_glink_write(cd->client, data, len);
	if (!rc) {
		rc = wait_for_completion_timeout(&cd->ack,
					msecs_to_jiffies(WAIT_TIME_MS));
		if (!rc) {
			pr_err("Error, timed out sending message\n");
			mutex_unlock(&cd->lock);
			return -ETIMEDOUT;
		}

		rc = 0;
	}
	mutex_unlock(&cd->lock);

	return rc;
}

static int chg_ulog_request(struct chg_ulog_glink_dev *cd, bool init)
{
	struct get_ulog_req_msg req_msg = { { 0 } };

	req_msg.hdr.owner = MSG_OWNER_CHG_ULOG;
	req_msg.hdr.type = MSG_TYPE_REQ_RESP;
	req_msg.hdr.opcode = init ? GET_CHG_INIT_ULOG_REQ : GET_CHG_ULOG_REQ;
	req_msg.log_size = MAX_ULOG_SIZE;

	return chg_ulog_write(cd, &req_msg, sizeof(req_msg));
}

static int chg_ulog_set_log_type(struct chg_ulog_glink_dev *cd, u64 category,
				u32 level)
{
	struct set_ulog_prop_req_msg req_msg = { { 0 } };
	int rc;

	req_msg.hdr.owner = MSG_OWNER_CHG_ULOG;
	req_msg.hdr.type = MSG_TYPE_REQ_RESP;
	req_msg.hdr.opcode = SET_CHG_ULOG_PROP_REQ;
	req_msg.log_category = category;
	req_msg.log_level = level;

	rc = chg_ulog_write(cd, &req_msg, sizeof(req_msg));
	if (!rc)
		pr_debug("Set log category %llu log level %u\n", category,
			level);

	return rc;
}

static void chg_ulog_work(struct work_struct *work)
{
	struct chg_ulog_glink_dev *cd = container_of(work,
					struct chg_ulog_glink_dev,
					ulog_work.work);
	int rc;

	rc = chg_ulog_request(cd, cd->init_log_enable);
	if (rc)
		pr_err("Error requesting ulog, rc=%d\n", rc);
	else if (cd->log_enable || cd->init_log_enable)
		schedule_delayed_work(&cd->ulog_work,
					msecs_to_jiffies(cd->log_time_ms));
}

static void adsp_dump_log_work_func(struct work_struct *work)
{
	struct chg_ulog_glink_dev *cd = container_of(work,
					struct chg_ulog_glink_dev,
					adsp_dump_log_work);
	struct get_ulog_req_msg req_msg = { { 0 } };

	req_msg.hdr.owner = MSG_OWNER_CHG_ULOG;
	req_msg.hdr.type = MSG_TYPE_REQ_RESP;
	req_msg.hdr.opcode = GET_MCA_LOG_REQ;

	mutex_lock(&cd->lock);
	(void)pmic_glink_write(cd->client, &req_msg, sizeof(req_msg));
	mutex_unlock(&cd->lock);
}

static void ulog_store(struct chg_ulog_glink_dev *cd, void *ipc_ctxt,
			size_t len)
{
	char *buf = cd->ulog_buf, *token = NULL;

	if (buf[0] == '\0') {
		pr_debug("buffer is NULL\n");
		if (cd->init_log_enable)
			cd->init_log_enable = false;
		return;
	}

	buf[len - 1] = '\0';
	if (len >= MAX_MSG_SIZE) {
		do {
			token = strsep((char **)&buf, "\n");
			if (token)
				ipc_log_string(ipc_ctxt, "%s", token);
		} while (token);
	} else {
		ipc_log_string(ipc_ctxt, "%s", buf);
	}
}

static void handle_ulog_message(struct chg_ulog_glink_dev *cd,
				struct get_ulog_resp_msg *resp_msg,
				size_t len)
{
	void *ipc_ctxt;

	if (len != sizeof(*resp_msg)) {
		pr_err("Expected data length: %zu, received: %zu\n",
				sizeof(*resp_msg), len);
		return;
	}

	memcpy(cd->ulog_buf, resp_msg->buf, sizeof(cd->ulog_buf));
	ipc_ctxt = (resp_msg->hdr.opcode == GET_CHG_INIT_ULOG_REQ)
			? cd->ipc_init_log : cd->ipc_log;

	ulog_store(cd, ipc_ctxt, len - sizeof(resp_msg->hdr));
}

static void handle_adsp_log(struct chg_ulog_glink_dev *cd,
				void *data,	size_t len)
{
	struct get_adsp_log_msg *log_msg = data;
	int totol_len = log_msg->cur_pos;
	int cur_pos = 0;
	char buf[MAX_LOG_PR_SIZE + 1] = { 0 };
	char *temp = NULL;
	int count = SINGLE_LOG_SIZE;

	pr_info("adsp_cur_time=%llu\n", log_msg->cur_time);
	pr_info("totol_len=%d\n", totol_len);
	while (totol_len) {
		if (totol_len < SINGLE_LOG_SIZE) {
			memcpy(buf, log_msg->log_info + cur_pos, totol_len);
			buf[totol_len] = '\0';
			pr_info("%s", buf);
			break;
		}

		temp = log_msg->log_info + cur_pos + SINGLE_LOG_SIZE;
		while(*temp != '\n' && *temp != '\0' && count < MAX_LOG_PR_SIZE - 1) {
			temp++;
			count++;
		}
		if (count < MAX_LOG_PR_SIZE - 1)
			count++;
		memcpy(buf, log_msg->log_info + cur_pos, count);
		buf[count] = '\0';
		totol_len -= count;
		cur_pos += count;
		count = SINGLE_LOG_SIZE;
		mca_log_info("%s", buf);
	}
}

static int chg_ulog_callback(void *priv, void *data, size_t len)
{
	struct pmic_glink_hdr *hdr = data;
	struct chg_ulog_glink_dev *cd = priv;
	struct adsp_realtime_log_msg *rt_log = data;

	pr_debug("owner: %u type: %u opcode: %#x len: %zu\n", hdr->owner,
		hdr->type, hdr->opcode, len);

	switch (hdr->opcode) {
	case SET_CHG_ULOG_PROP_REQ:
		complete(&cd->ack);
		break;
	case GET_CHG_ULOG_REQ:
	case GET_CHG_INIT_ULOG_REQ:
		handle_ulog_message(cd, data, len);
		complete(&cd->ack);
		break;
	case GET_MCA_LOG_REQ:
		handle_adsp_log(cd, data, len);
		break;
	case GET_MCA_REALTIME_LOG_REQ:
		mca_log_info("%s", rt_log->log_info);
		break;
	default:
		pr_err("Unknown opcode %u\n", hdr->opcode);
		break;
	}

	return 0;
}

static int ulog_cat_get(void *data, u64 *val)
{
	struct chg_ulog_glink_dev *cd = data;

	*val = cd->log_category;

	return 0;
}

static int ulog_cat_set(void *data, u64 val)
{
	int rc;
	struct chg_ulog_glink_dev *cd = data;

	if (cd->log_enable) {
		pr_err("Disable ulog before changing log category\n");
		return -EINVAL;
	}

	if (val == cd->log_category)
		return 0;

	rc = chg_ulog_set_log_type(cd, val, cd->log_level);
	if (rc)
		pr_err("Couldn't set log_category rc=%d\n", rc);
	else
		cd->log_category = val;

	return rc;
}
DEFINE_DEBUGFS_ATTRIBUTE(ulog_cat_fops, ulog_cat_get, ulog_cat_set,
			"%llu\n");

static int ulog_level_get(void *data, u64 *val)
{
	struct chg_ulog_glink_dev *cd = data;

	*val = cd->log_level;

	return 0;
}

static int ulog_level_set(void *data, u64 val)
{
	int rc;
	struct chg_ulog_glink_dev *cd = data;
	u32 level = val;

	if (cd->log_enable) {
		pr_err("Disable ulog before changing log level\n");
		return -EINVAL;
	}

	if (level == cd->log_level)
		return 0;

	rc = chg_ulog_set_log_type(cd, cd->log_category, level);
	if (rc)
		pr_err("Couldn't set log_level rc=%d\n", rc);
	else
		cd->log_level = level;

	return rc;
}
DEFINE_DEBUGFS_ATTRIBUTE(ulog_level_fops, ulog_level_get, ulog_level_set,
			"%llu\n");

static int ulog_en_get(void *data, u64 *val)
{
	struct chg_ulog_glink_dev *cd = data;

	*val = cd->log_enable;

	return 0;
}

static int ulog_en_set(void *data, u64 val)
{
	struct chg_ulog_glink_dev *cd = data;
	bool en = val;

	if (en == cd->log_enable)
		return 0;

	if (cd->log_category == LOG_CATEGORY_INIT)
		cd->init_log_enable = en;
	else
		cd->log_enable = en;

	if (en)
		schedule_delayed_work(&cd->ulog_work,
					msecs_to_jiffies(cd->log_time_ms));
	else
		cancel_delayed_work_sync(&cd->ulog_work);

	return 0;
}
DEFINE_DEBUGFS_ATTRIBUTE(ulog_en_fops, ulog_en_get, ulog_en_set, "%llu\n");

static int ulog_time_get(void *data, u64 *val)
{
	struct chg_ulog_glink_dev *cd = data;

	*val = cd->log_time_ms;

	return 0;
}

static int ulog_time_set(void *data, u64 val)
{
	struct chg_ulog_glink_dev *cd = data;

	if (val == cd->log_time_ms)
		return 0;

	if (val < LOG_MIN_TIME_MS)
		return -EINVAL;

	cd->log_time_ms = val;

	return 0;
}
DEFINE_DEBUGFS_ATTRIBUTE(ulog_time_fops, ulog_time_get, ulog_time_set,
			"%llu\n");

static int dump_adsp_log_get(void *data, u64 *val)
{
	struct chg_ulog_glink_dev *cd = data;

	schedule_work(&cd->adsp_dump_log_work);

	return 0;
}

static int dump_adsp_log_set(void *data, u64 val)
{
	struct chg_ulog_glink_dev *cd = data;

	schedule_work(&cd->adsp_dump_log_work);

	return 0;
}
DEFINE_DEBUGFS_ATTRIBUTE(dump_adsp_log_fops, dump_adsp_log_get, dump_adsp_log_set,
			"%llu\n");

static int chg_ulog_add_debugfs(struct chg_ulog_glink_dev *cd)
{
	struct dentry *dir, *file;
	int rc;

	dir = debugfs_create_dir("charger_ulog", NULL);
	if (IS_ERR(dir)) {
		rc = PTR_ERR(dir);
		pr_err("Failed to create charger_ulog debugfs directory: %d\n",
			rc);
		return rc;
	}

	file = debugfs_create_file_unsafe("category", 0600, dir, cd,
					&ulog_cat_fops);
	if (IS_ERR(file)) {
		rc = PTR_ERR(file);
		pr_err("Failed to create category %d\n", rc);
		goto out;
	}

	file = debugfs_create_file_unsafe("level", 0600, dir, cd,
					&ulog_level_fops);
	if (IS_ERR(file)) {
		rc = PTR_ERR(file);
		pr_err("Failed to create level %d\n", rc);
		goto out;
	}

	file = debugfs_create_file_unsafe("enable", 0600, dir, cd,
					&ulog_en_fops);
	if (IS_ERR(file)) {
		rc = PTR_ERR(file);
		pr_err("Failed to create enable %d\n", rc);
		goto out;
	}

	file = debugfs_create_file_unsafe("time_ms", 0600, dir, cd,
					&ulog_time_fops);
	if (IS_ERR(file)) {
		rc = PTR_ERR(file);
		pr_err("Failed to create time_ms %d\n", rc);
		goto out;
	}

	file = debugfs_create_file_unsafe("dump_adsp_log", 0600, dir, cd,
					&dump_adsp_log_fops);
	if (IS_ERR(file)) {
		rc = PTR_ERR(file);
		pr_err("Failed to create time_ms %d\n", rc);
		goto out;
	}
	cd->debugfs_dir = dir;

	return 0;
out:
	debugfs_remove_recursive(dir);
	return rc;
}

static int chg_ulog_probe(struct platform_device *pdev)
{
	struct chg_ulog_glink_dev *cd;
	struct pmic_glink_client_data client_data = { };
	int rc;

	cd = devm_kzalloc(&pdev->dev, sizeof(*cd), GFP_KERNEL);
	if (!cd)
		return -ENOMEM;

	mutex_init(&cd->lock);
	init_completion(&cd->ack);
	INIT_DELAYED_WORK(&cd->ulog_work, chg_ulog_work);
	INIT_WORK(&cd->adsp_dump_log_work, adsp_dump_log_work_func);
	cd->log_time_ms = LOG_DEFAULT_TIME_MS;
	platform_set_drvdata(pdev, cd);

	cd->dev = &pdev->dev;
	client_data.id = MSG_OWNER_CHG_ULOG;
	client_data.name = "chg_ulog";
	client_data.msg_cb = chg_ulog_callback;
	client_data.priv = cd;

	cd->client = pmic_glink_register_client(cd->dev, &client_data);
	if (IS_ERR(cd->client))
		return dev_err_probe(cd->dev, PTR_ERR(cd->client),
				"Error in registering with pmic_glink\n");

	rc = chg_ulog_add_debugfs(cd);
	if (rc) {
		pmic_glink_unregister_client(cd->client);
		return dev_err_probe(cd->dev, -EINVAL, "Error in creating debugfs\n");
	}

	cd->ipc_log = ipc_log_context_create(NUM_LOG_PAGES, "charger_ulog", 0);
	if (!cd->ipc_log) {
		pmic_glink_unregister_client(cd->client);
		debugfs_remove_recursive(cd->debugfs_dir);
		return dev_err_probe(cd->dev, -ENODEV, "Error in creating charger_ulog\n");
	}

	cd->ipc_init_log = ipc_log_context_create(NUM_INIT_LOG_PAGES,
						"charger_ulog_init", 0);
	if (!cd->ipc_init_log) {
		pmic_glink_unregister_client(cd->client);
		ipc_log_context_destroy(cd->ipc_log);
		debugfs_remove_recursive(cd->debugfs_dir);
		return dev_err_probe(cd->dev, -ENODEV, "Error in creating charger_ulog_init\n");
	}

	return 0;
}

static int chg_ulog_remove(struct platform_device *pdev)
{
	struct chg_ulog_glink_dev *cd = platform_get_drvdata(pdev);
	int rc;

	debugfs_remove_recursive(cd->debugfs_dir);
	cancel_delayed_work_sync(&cd->ulog_work);

	rc = pmic_glink_unregister_client(cd->client);
	if (rc < 0)
		pr_err("Error unregistering from pmic_glink, rc=%d\n", rc);

	ipc_log_context_destroy(cd->ipc_log);
	ipc_log_context_destroy(cd->ipc_init_log);

	return 0;
}

static const struct of_device_id chg_ulog_match_table[] = {
	{ .compatible = "qcom,charger-ulog-glink" },
	{},
};

static struct platform_driver chg_ulog_driver = {
	.driver	= {
		.name = "charger_ulog_glink",
		.of_match_table = chg_ulog_match_table,
	},
	.probe	= chg_ulog_probe,
	.remove	= chg_ulog_remove,
};
module_platform_driver(chg_ulog_driver);

MODULE_DESCRIPTION("QTI charger ulog glink driver");
MODULE_LICENSE("GPL");
