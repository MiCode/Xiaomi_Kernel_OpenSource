// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/thermal.h>
#include <linux/types.h>
#include <mtk_ccci_common.h>
#include "../thermal_core.h"
#include "md_cooling.h"
#include "thermal_trace.h"


/*==================================================
 * Global Variables
 *==================================================
 */
static struct md_cooling_data *mdc;
static unsigned int default_tx_pwr[MAX_NUM_TX_PWR_STATE] = {
	DEFAULT_THROTTLE_TX_PWR_LV1,
	DEFAULT_THROTTLE_TX_PWR_LV2,
	DEFAULT_THROTTLE_TX_PWR_LV3,
};
static DEFINE_MUTEX(md_cdev_list_lock);
static DEFINE_MUTEX(mdc_data_lock);
static DEFINE_MUTEX(send_tmc_msg_lock);
static LIST_HEAD(md_cdev_list);

/*==================================================
 * Global functions
 *==================================================
 */
enum md_cooling_status get_md_cooling_status(void)
{
	enum md_cooling_status cur_status;
	int md_state;

	if (!mdc)
		return MD_OFF;

	md_state = exec_ccci_kern_func_by_md_id(0, ID_GET_MD_STATE, NULL, 0);
	if (md_state == MD_STATE_INVALID || md_state == MD_STATE_EXCEPTION) {
		pr_warn("Invalid MD state(%d)!\n", md_state);
		cur_status = MD_OFF;
	} else {
		mutex_lock(&mdc_data_lock);
		cur_status = mdc->status;
		mutex_unlock(&mdc_data_lock);
	}

	return cur_status;
}

int send_throttle_msg(unsigned int msg)
{
	int ret;

	mutex_lock(&send_tmc_msg_lock);
	ret = exec_ccci_kern_func_by_md_id(MD_SYS1, ID_THROTTLING_CFG, (char *)&msg, 4);
	mutex_unlock(&send_tmc_msg_lock);

	if (ret)
		pr_err("send tmc msg 0x%x failed, ret:%d\n", msg, ret);
	else
		pr_debug("send tmc msg 0x%x done\n", msg);

	return ret;
}

void update_throttle_power(unsigned int pa_id, unsigned int *pwr)
{
	struct md_cooling_device *md_cdev;

	mutex_lock(&md_cdev_list_lock);
	list_for_each_entry(md_cdev, &md_cdev_list, node) {
		if (md_cdev->type == MD_COOLING_TYPE_TX_PWR && md_cdev->pa_id == pa_id) {
			memcpy(md_cdev->throttle_tx_power, pwr,
				sizeof(md_cdev->throttle_tx_power));
			mutex_unlock(&md_cdev_list_lock);
			return;
		}
	}
	mutex_unlock(&md_cdev_list_lock);
}

struct md_cooling_device *get_md_cdev(enum md_cooling_type type, unsigned int pa_id)
{
	struct md_cooling_device *md_cdev;

	mutex_lock(&md_cdev_list_lock);
	list_for_each_entry(md_cdev, &md_cdev_list, node) {
		if (md_cdev->type == type && md_cdev->pa_id == pa_id) {
			mutex_unlock(&md_cdev_list_lock);
			return md_cdev;
		}
	}
	mutex_unlock(&md_cdev_list_lock);

	return NULL;
}

unsigned int get_pa_num(void)
{
	return (mdc) ? mdc->pa_num : 0;
}

/*==================================================
 * Local functions
 *==================================================
 */
static unsigned long find_max_mutt_state(void)
{
	struct md_cooling_device *md_cdev;
	unsigned long final_state = MD_COOLING_UNLIMITED_STATE;
	int i;

	for (i = 0; i < mdc->pa_num; i++) {
		md_cdev = get_md_cdev(MD_COOLING_TYPE_MUTT, i);
		if (md_cdev)
			final_state = max(final_state, md_cdev->target_state);
	}

	return final_state;
}

static enum md_cooling_status state_to_md_cooling_status(unsigned long state)
{
	enum md_cooling_status status;

	if (state == mdc->pdata->max_lv)
		status = MD_NO_IMS;
	else if (state == mdc->pdata->max_lv - 1)
		status = MD_IMS_ONLY;
	else if (state == MD_COOLING_UNLIMITED_STATE)
		status = MD_LV_THROTTLE_DISABLED;
	else
		status = MD_LV_THROTTLE_ENABLED;

	return status;
}

/*==================================================
 * cooler callback functions
 *==================================================
 */
static int mutt_throttle(struct md_cooling_device *md_cdev, unsigned long state)
{
	struct device *dev = md_cdev->dev;
	enum md_cooling_status status, new_status;
	unsigned int msg;
	unsigned long final_state;
	int ret;

	status = get_md_cooling_status();
	/*
	 * Ignore when SCG off is activated
	 */
	if (is_scg_off_enabled(status)) {
		dev_info(dev, "skip MUTT due to SCG off is enabled\n");
		return 0;
	}

	mutex_lock(&mdc_data_lock);

	if (is_md_off(status)) {
		md_cdev->target_state = MD_COOLING_UNLIMITED_STATE;
		mdc->mutt_state = MD_COOLING_UNLIMITED_STATE;
		mutex_unlock(&mdc_data_lock);
		trace_md_mutt_limit(md_cdev, status);
		return 0;
	}

	md_cdev->target_state = state;
	final_state = find_max_mutt_state();
	if (final_state == mdc->mutt_state) {
		mutex_unlock(&mdc_data_lock);
		dev_info(dev, "%s: ignore set state %ld due to final_state(%ld) is not changed\n",
				md_cdev->name, state, final_state);
		return 0;
	}

	msg = (final_state == MD_COOLING_UNLIMITED_STATE)
		? TMC_COOLER_LV_DISABLE_MSG
		: mutt_lv_to_tmc_msg(md_cdev->pa_id, mdc->pdata->state_to_mutt_lv(final_state));
	ret = send_throttle_msg(msg);
	if (ret) {
		mutex_unlock(&mdc_data_lock);
		return ret;
	}

	new_status = state_to_md_cooling_status(final_state);
	mdc->status = new_status;
	mdc->mutt_state = final_state;

	mutex_unlock(&mdc_data_lock);

	dev_info(dev, "%s: set lv = %ld done\n", md_cdev->name, state);
	trace_md_mutt_limit(md_cdev, new_status);

	return 0;
}

static int tx_pwr_throttle(struct md_cooling_device *md_cdev, unsigned long state)
{
	struct device *dev = md_cdev->dev;
	enum md_cooling_status status;
	unsigned int msg, pwr;
	int ret;

	status = get_md_cooling_status();
	if (is_md_off(status)) {
		dev_info(dev, "skip tx pwr control due to MD is off\n");
		/*
		 * TX power throttle will be cleared when MD is reset. Clear target
		 * LV to avoid sending unnecessary command to MD
		 */
		md_cdev->target_state = MD_COOLING_UNLIMITED_STATE;
		trace_md_tx_pwr_limit(md_cdev, status);
		return 0;
	}

	if (state == MD_COOLING_UNLIMITED_STATE) {
		pwr = 0;
	} else {
		if (!md_cdev->throttle_tx_power[state - 1])
			md_cdev->throttle_tx_power[state - 1] = default_tx_pwr[state - 1];
		pwr = md_cdev->throttle_tx_power[state - 1];
	}

	msg = reduce_tx_pwr_to_tmc_msg(md_cdev->pa_id, pwr);
	ret = send_throttle_msg(msg);
	if (!ret)
		md_cdev->target_state = state;

	dev_info(dev, "%s: set lv = %ld(tx_pwr=%d) done\n", md_cdev->name, state, pwr);
	trace_md_tx_pwr_limit(md_cdev, status);

	return ret;
}

static int scg_off_throttle(struct md_cooling_device *md_cdev, unsigned long state)
{
	struct device *dev = md_cdev->dev;
	enum md_cooling_status status;
	unsigned int msg;
	int ret;

	status = get_md_cooling_status();
	/*
	 * SCG will be turned on again when MD is reset. Clear target
	 * LV to avoid sending unnecessary SCG on command to MD
	 */
	if (is_md_off(status)) {
		md_cdev->target_state = MD_COOLING_UNLIMITED_STATE;
		trace_md_scg_off(md_cdev, status);
		return 0;
	}

	/*
	 * Ignore when MUTT is activated because SCG off is the one of MUTT cooling levels
	 */
	if (is_mutt_enabled(status)) {
		dev_info(dev, "skip SCG control due to MUTT is enabled\n");
		return 0;
	}

	msg = (state == MD_COOLING_UNLIMITED_STATE)
		? scg_off_to_tmc_msg(0) : scg_off_to_tmc_msg(1);
	ret = send_throttle_msg(msg);
	if (!ret) {
		md_cdev->target_state = state;
		mutex_lock(&mdc_data_lock);
		mdc->status = (state) ? MD_SCG_OFF : MD_LV_THROTTLE_DISABLED;
		mutex_unlock(&mdc_data_lock);
	}

	dev_info(dev, "%s: set lv = %ld done\n", md_cdev->name, state);
	trace_md_scg_off(md_cdev, status);

	return ret;
}

static int md_cooling_get_max_state(struct thermal_cooling_device *cdev, unsigned long *state)
{
	struct md_cooling_device *md_cdev = cdev->devdata;

	*state = md_cdev->max_state;

	return 0;
}

static int md_cooling_get_cur_state(struct thermal_cooling_device *cdev, unsigned long *state)
{
	struct md_cooling_device *md_cdev = cdev->devdata;

	*state = md_cdev->target_state;

	return 0;
}

static int md_cooling_set_cur_state(struct thermal_cooling_device *cdev, unsigned long state)
{
	struct md_cooling_device *md_cdev = cdev->devdata;
	int ret;

	/* Request state should be less than max_state */
	if (state > md_cdev->max_state || !md_cdev->throttle)
		return -EINVAL;

	if (md_cdev->target_state == state)
		return 0;

	ret = md_cdev->throttle(md_cdev, state);

	return ret;
}

/*==================================================
 * platform data and platform driver callbacks
 *==================================================
 */
static unsigned long md_cooling_state_to_mutt_lv(unsigned long state)
{
	return (state > 0) ? (state - 1) : 0;
}

static const struct md_cooling_platform_data md_cooling_pdata = {
	.state_to_mutt_lv = md_cooling_state_to_mutt_lv,
	.max_lv = 9,
};

static const struct of_device_id md_cooling_of_match[] = {
	{ .compatible = "mediatek,mt6295-md-cooler", },
	{ .compatible = "mediatek,mt6297-md-cooler", },
	{ .compatible = "mediatek,mt6298-md-cooler", },
	{},
};
MODULE_DEVICE_TABLE(of, md_cooling_of_match);

static struct thermal_cooling_device_ops md_cooling_ops = {
	.get_max_state		= md_cooling_get_max_state,
	.get_cur_state		= md_cooling_get_cur_state,
	.set_cur_state		= md_cooling_set_cur_state,
};

static int init_md_cooling_device(struct device *dev, struct device_node *np, int id)
{
	struct thermal_cooling_device *cdev;
	struct md_cooling_device *md_cdev;

	md_cdev = devm_kzalloc(dev, sizeof(*md_cdev), GFP_KERNEL);
	if (!md_cdev)
		return -ENOMEM;

	strncpy(md_cdev->name, np->name, strlen(np->name));
	md_cdev->pa_id = id;
	md_cdev->target_state = MD_COOLING_UNLIMITED_STATE;
	md_cdev->dev = dev;

	if (strstr(np->name, "mutt") != NULL) {
		md_cdev->type = MD_COOLING_TYPE_MUTT;
		md_cdev->max_state = mdc->pdata->max_lv;
		md_cdev->throttle = mutt_throttle;
	} else if (strstr(np->name, "tx-pwr") != NULL) {
		md_cdev->type = MD_COOLING_TYPE_TX_PWR;
		md_cdev->max_state = MAX_NUM_TX_PWR_STATE;
		md_cdev->throttle = tx_pwr_throttle;
	} else if (strstr(np->name, "scg-off") != NULL) {
		md_cdev->type = MD_COOLING_TYPE_SCG_OFF;
		md_cdev->max_state = MAX_NUM_SCG_OFF_STATE;
		md_cdev->throttle = scg_off_throttle;
	} else {
		goto init_fail;
	}

	cdev = thermal_of_cooling_device_register(np, md_cdev->name, md_cdev, &md_cooling_ops);
	if (IS_ERR(cdev))
		goto init_fail;
	md_cdev->cdev = cdev;

	mutex_lock(&md_cdev_list_lock);
	list_add(&md_cdev->node, &md_cdev_list);
	mutex_unlock(&md_cdev_list_lock);

	dev_info(dev, "register %s done, id=%d\n", md_cdev->name, md_cdev->pa_id);

	return 0;

init_fail:
	kfree(md_cdev);
	return -EINVAL;
}

static int parse_dt(struct platform_device *pdev)
{
	struct device_node *child, *gchild;
	struct device_node *np = pdev->dev.of_node;
	struct device *dev = &pdev->dev;
	int id = 0, count, ret;

	count = of_get_child_count(np);
	if (!count) {
		dev_err(dev, "missing child node for PA info\n");
		return -EINVAL;
	}

	mdc->pa_num = count;

	for_each_child_of_node(np, child) {
		for_each_child_of_node(child, gchild) {
			ret = init_md_cooling_device(dev, gchild, id);
			of_node_put(gchild);
			if (ret) {
				of_node_put(child);
				return ret;
			}
		}
		of_node_put(child);
		id++;
	}

	return 0;
}

static int md_cooling_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	int ret = 0;

	mdc = devm_kzalloc(dev, sizeof(*mdc), GFP_KERNEL);
	if (!mdc)
		return -ENOMEM;

	mdc->mutt_state = MD_COOLING_UNLIMITED_STATE;
	mdc->pdata = &md_cooling_pdata;

	ret = parse_dt(pdev);
	if (ret) {
		dev_err(dev, "failed to parse cooler nodes from DT!\n");
		return ret;
	}

	platform_set_drvdata(pdev, mdc);

#if IS_ENABLED(CONFIG_DEBUG_FS)
	md_cooling_debugfs_init();
#endif

	return 0;
}

static int md_cooling_remove(struct platform_device *pdev)
{
	struct list_head *pos, *next;
	struct md_cooling_device *md_cdev;

#if IS_ENABLED(CONFIG_DEBUG_FS)
	md_cooling_debugfs_exit();
#endif

	mutex_lock(&md_cdev_list_lock);
	list_for_each_safe(pos, next, &md_cdev_list) {
		md_cdev = list_entry(pos, struct md_cooling_device, node);
		thermal_cooling_device_unregister(md_cdev->cdev);
		list_del(&md_cdev->node);
	}
	mutex_unlock(&md_cdev_list_lock);

	platform_set_drvdata(pdev, NULL);

	return 0;
}

static struct platform_driver md_cooling_driver = {
	.probe = md_cooling_probe,
	.remove = md_cooling_remove,
	.driver = {
		.name = "mtk-md-cooling",
		.of_match_table = md_cooling_of_match,
	},
};
module_platform_driver(md_cooling_driver);

MODULE_AUTHOR("Shun-Yao Yang <brian-sy.yang@mediatek.com>");
MODULE_DESCRIPTION("Mediatek modem cooling driver");
MODULE_LICENSE("GPL v2");
