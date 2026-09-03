// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (C) 2019 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "sipa_dele: " fmt

#include <linux/delay.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/sipa.h>
#include <linux/sipc.h>

#include "sipa_dele_priv.h"

static struct cp_delegator *s_cp_delegator;

static void cp_dele_on_commad(void *priv, u16 flag, u32 data)
{
	struct sipa_delegator *delegator = priv;
	int ret = 0;

	pr_info("prod_id:%d, on_cmd, flag = %d\n", delegator->prod_id, flag);

	switch (flag) {
	case SMSG_FLG_DELE_ENABLE:

		if (!delegator->cfg->sipa_sys_eb) {
			pm_stay_awake(delegator->pdev);
			pr_info("sipa_dele pm stay awake\n");
		}
		delegator->pd_eb_flag = true;

		if (!s_cp_delegator->delegator.cfg->sipa_sys_eb)
			goto sipa_eb;
check_again:
		ret = pm_runtime_get_sync(delegator->pdev);
		if (ret) {
			pm_runtime_put(delegator->pdev);
			pr_warn("sipa_dele get pd fail ret = %d\n", ret);
			mdelay(1);
			goto check_again;
		} else {
sipa_eb:
			delegator->pd_get_flag = true;
			sipa_set_enabled(true);
			sipa_dele_start_done_work(delegator,
						  SMSG_FLG_DELE_ENABLE,
						  SMSG_VAL_DELE_REQ_SUCCESS);
			pr_info("sipa_dele get pd success ret = %d\n", ret);
		}
		break;
	case SMSG_FLG_DELE_DISABLE:
		delegator->pd_eb_flag = false;
		sipa_set_enabled(false);
		if (s_cp_delegator->delegator.cfg->sipa_sys_eb)
			pm_runtime_put(delegator->pdev);
		delegator->pd_get_flag = false;

		if (!delegator->cfg->sipa_sys_eb) {
			pm_relax(delegator->pdev);
			pr_info("sipa_dele pm relax\n");
		}
		break;
	default:
		break;
	}

	pr_info("sipa pd_eb_flag = %d, pd_get_flag = %d\n",
		delegator->pd_eb_flag, delegator->pd_get_flag);

	/* do default operations */
	sipa_dele_on_commad(priv, flag, data);
}

static void cp_dele_on_event(void *priv, u16 flag, u32 data)
{
	struct sipa_delegator *delegator = priv;

	pr_info("prod_id:%d, on_evt, flag = %d\n", delegator->prod_id, flag);

	switch (flag) {
	case SMSG_FLG_DELE_ENTER_FLOWCTRL:
		sipa_irq_affinity_change(true);
		break;
	case SMSG_FLG_DELE_EXIT_FLOWCTRL:
		break;
	default:
		break;
	}
}

static int cp_dele_local_req_r_prod(void *user_data)
{
	/* do enable ipa  operation */

	return sipa_dele_local_req_r_prod(user_data);
}

static void cp_dele_restart_handler(struct work_struct *work)
{
	struct delayed_work *restart_delayed_work = to_delayed_work(work);
	struct sipa_delegator *delegator = container_of(restart_delayed_work,
							struct sipa_delegator,
							restart_work);
	unsigned long flags;

	spin_lock_irqsave(&delegator->lock, flags);
	if (delegator->pd_eb_flag && delegator->pd_get_flag) {
		pr_info("sipa will power off\n");
		sipa_set_enabled(false);
		if (s_cp_delegator->delegator.cfg->sipa_sys_eb)
			pm_runtime_put(delegator->pdev);
		delegator->pd_eb_flag = false;
		delegator->pd_get_flag = false;
	}

	if (delegator->cons_ref_cnt) {
		pr_info("sipa will release resource\n");
		delegator->cons_ref_cnt--;
		sipa_rm_release_resource(delegator->cons_user);
	}

	if (!delegator->cfg->sipa_sys_eb) {
		pm_relax(delegator->pdev);
		pr_info("sipa_dele pm relax\n");
	}

	spin_unlock_irqrestore(&delegator->lock, flags);
}

static ssize_t sipa_dele_reset_show(struct device *dev,
				    struct device_attribute *attr,
				    char *buf)
{
	char *a = "modem reset\n";

	return sprintf(buf, "\n%s\n", a);
}

static ssize_t sipa_dele_reset_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf,
				     size_t count)
{
	struct sipa_delegator *delegator = &s_cp_delegator->delegator;

	if (strstr(buf, "Modem Assert") &&
	    !strstr(buf, "P-ARM Modem Assert")) {
		if (!delegator->cfg->sipa_sys_eb) {
			dev_info(delegator->pdev, "Modem assert\n");
			cancel_delayed_work(&delegator->restart_work);
		}
	} else if (strstr(buf, "Modem Reset") ||
		   strstr(buf, "Modem Blocked")) {
		dev_info(delegator->pdev, "reset sipa_dele for modem\n");
		cancel_delayed_work(&delegator->restart_work);
		queue_delayed_work(delegator->restart_wq,
				   &delegator->restart_work,
				   0);
	}

	dev_info(delegator->pdev, "modem cmd %s\n", buf);

	return count;
}

static DEVICE_ATTR_RW(sipa_dele_reset);

static struct attribute *sipa_dele_attrs[] = {
	&dev_attr_sipa_dele_reset.attr,
	NULL,
};
ATTRIBUTE_GROUPS(sipa_dele);

static int sipa_dele_init_sysfs(struct cp_delegator *cp_delegator)
{
	int ret;
	struct sipa_delegator *delegator = &cp_delegator->delegator;

	ret = sysfs_create_groups(&delegator->pdev->kobj,
				  sipa_dele_groups);
	if (ret) {
		dev_err(delegator->pdev, "sipa_dele fail to create sysfs\n");
		sysfs_remove_groups(&delegator->pdev->kobj,
				    sipa_dele_groups);
		return ret;
	}

	return 0;
}

int cp_delegator_init(struct sipa_delegator_create_params *params)
{
	int ret;

	s_cp_delegator = devm_kzalloc(params->pdev,
				      sizeof(*s_cp_delegator),
				      GFP_KERNEL);
	if (!s_cp_delegator)
		return -ENOMEM;
	ret = sipa_delegator_init(&s_cp_delegator->delegator,
				  params);
	if (ret)
		return ret;

	s_cp_delegator->delegator.on_cmd = cp_dele_on_commad;
	s_cp_delegator->delegator.on_evt = cp_dele_on_event;
	s_cp_delegator->delegator.local_request_prod = cp_dele_local_req_r_prod;
	s_cp_delegator->delegator.pd_eb_flag = false;
	s_cp_delegator->delegator.pd_get_flag = false;
	s_cp_delegator->delegator.smsg_cnt = 0;
	sipa_delegator_start(&s_cp_delegator->delegator);

	if (s_cp_delegator->delegator.cfg->sipa_sys_eb)
		pm_runtime_enable(s_cp_delegator->delegator.pdev);
	s_cp_delegator->delegator.restart_wq = create_workqueue("sipa_dele_restart_wq");
	if (!s_cp_delegator->delegator.restart_wq) {
		dev_err(s_cp_delegator->delegator.pdev,
			"sipa_dele_restart_wq create failed\n");
		return -ENOMEM;
	}
	INIT_DELAYED_WORK(&s_cp_delegator->delegator.restart_work,
			  cp_dele_restart_handler);

	ret = sipa_rm_add_dependency(SIPA_RM_RES_CONS_WIFI_UL,
				     s_cp_delegator->delegator.prod_id);
	if (ret)
		return ret;

	ret = sipa_rm_add_dependency(SIPA_RM_RES_CONS_WIFI_DL,
				     s_cp_delegator->delegator.prod_id);
	if (ret)
		return ret;

	sipa_dele_init_sysfs(s_cp_delegator);

	return 0;
}
