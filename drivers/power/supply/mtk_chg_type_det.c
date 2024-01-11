// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#include <dt-bindings/power/mtk-charger.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <tcpm.h>
#include "mtk_battery.h"

#include "mtk_charger.h"
#if IS_ENABLED(CONFIG_MTK_CHARGER)
#include <charger_class.h>
#endif /* CONFIG_MTK_CHARGER */

#define MTK_CTD_DRV_VERSION	"1.0.2_MTK"

#define FAST_CHG_WATT		7500000 /* uW */

/* Add for typec_mode */
int typec_mode = -1;
EXPORT_SYMBOL(typec_mode);

struct mci_notifier_block {
	struct notifier_block nb;
	struct mtk_ctd_info *mci;
};

struct mtk_ctd_info {
	struct device *dev;
	/* device tree */
	u32 nr_port;
#if IS_ENABLED(CONFIG_MTK_CHARGER)
	struct charger_device **chg_dev;
	bool *chg_shared;
#endif /* CONFIG_MTK_CHARGER */
	u32 *bc12_sel;
	struct power_supply **bc12_psy;
	/* typec notify */
	struct tcpc_device **tcpc;
	struct mci_notifier_block *pd_nb;
	/* chg det */
	int *typec_attach;
	unsigned long chrdet;
	wait_queue_head_t attach_wq;
	struct mutex attach_lock;
	struct task_struct *attach_task;
};

static int typec_attach_thread(void *data)
{
	struct mtk_ctd_info *mci = data;
	int ret = 0, i = 0, attach = 0;
	union power_supply_propval val = {0,};

	dev_info(mci->dev, "%s ++\n", __func__);
wait:
	ret = wait_event_interruptible(mci->attach_wq, mci->chrdet ||
				       kthread_should_stop());
	if (kthread_should_stop() || ret) {
		dev_notice(mci->dev, "%s exits(%d)\n", __func__, ret);
		goto out;
	}

	for (i = 0; i < mci->nr_port; i++) {
		mutex_lock(&mci->attach_lock);
		if (test_and_clear_bit(i, &mci->chrdet))
			attach = mci->typec_attach[i];
		else
			attach = ATTACH_TYPE_MAX;
		mutex_unlock(&mci->attach_lock);
		if (attach == ATTACH_TYPE_MAX)
			continue;

		dev_info(mci->dev, "%s port%d attach = %d\n", __func__,
				   i, attach);

		if (mci->bc12_sel[i] == MTK_CTD_BY_SUBPMIC_PWR_RDY)
			continue;

		val.intval = ONLINE(i, attach);
		ret = power_supply_set_property(mci->bc12_psy[i],
						POWER_SUPPLY_PROP_ONLINE, &val);
		if (ret < 0)
			dev_notice(mci->dev, "Failed to set online(%d)\n", ret);
	}
	goto wait;
out:
	dev_info(mci->dev, "%s --\n", __func__);

	return ret;
}

static void handle_typec_pd_attach(struct mtk_ctd_info *mci, int idx,
				   int attach)
{
	mutex_lock(&mci->attach_lock);
	dev_info(mci->dev, "%s port%d attach = %d --> %d\n", __func__,
			   idx, mci->typec_attach[idx], attach);
	if (mci->typec_attach[idx] == attach)
		goto skip;
	mci->typec_attach[idx] = attach;
	set_bit(idx, &mci->chrdet);
	wake_up_interruptible(&mci->attach_wq);
skip:
	mutex_unlock(&mci->attach_lock);
}

static void handle_pd_rdy_attach(struct mtk_ctd_info *mci, int idx,
				 struct tcp_notify *noti)
{
	int attach = 0, watt = 0;
	bool usb_comm = false;
	struct tcpm_remote_power_cap cap;

	switch (noti->pd_state.connected) {
	case PD_CONNECT_PE_READY_SNK:
	case PD_CONNECT_PE_READY_SNK_PD30:
	case PD_CONNECT_PE_READY_SNK_APDO:
		break;
	default:
		return;
	}

	usb_comm = tcpm_inquire_usb_comm(mci->tcpc[idx]);
	memset(&cap, 0, sizeof(cap));
	tcpm_get_remote_power_cap(mci->tcpc[idx], &cap);
	watt = cap.max_mv[0] * cap.ma[0];
	dev_info(mci->dev, "%s %dmV %dmA %duW\n", __func__,
			    cap.max_mv[0], cap.ma[0], watt);

	if (usb_comm)
		attach = ATTACH_TYPE_PD_SDP;
	else
		attach = watt >= FAST_CHG_WATT ? ATTACH_TYPE_PD_DCP :
						 ATTACH_TYPE_PD_NONSTD;
	handle_typec_pd_attach(mci, idx, attach);
}

static void handle_audio_attach(struct mtk_ctd_info *mci, int idx,
				struct tcp_notify *noti)
{
	if (tcpm_inquire_typec_attach_state(mci->tcpc[idx]) !=
					    TYPEC_ATTACHED_AUDIO)
		return;

	handle_typec_pd_attach(mci, idx,
			       noti->vbus_state.mv ? ATTACH_TYPE_PD_NONSTD :
						     ATTACH_TYPE_NONE);
}

static int get_source_mode(struct tcp_notify *noti)
{
	if (noti->typec_state.new_state == TYPEC_ATTACHED_CUSTOM_SRC || noti->typec_state.new_state == TYPEC_ATTACHED_NORP_SRC)
		return PSY_TYPEC_SOURCE_DEFAULT;

	switch (noti->typec_state.rp_level) {
	case TYPEC_CC_VOLT_SNK_1_5:
		return PSY_TYPEC_SOURCE_MEDIUM;
	case TYPEC_CC_VOLT_SNK_3_0:
		return PSY_TYPEC_SOURCE_HIGH;
	case TYPEC_CC_VOLT_SNK_DFT:
		return PSY_TYPEC_SOURCE_DEFAULT;
	default:
		break;
	}

	return PSY_TYPEC_NONE;
}

static int pd_tcp_notifier_call(struct notifier_block *nb,
				unsigned long event, void *data)
{
	struct mci_notifier_block *pd_nb =
		container_of(nb, struct mci_notifier_block, nb);
	struct mtk_ctd_info *mci = pd_nb->mci;
	int idx = pd_nb - mci->pd_nb;
	struct tcp_notify *noti = data;
	uint8_t old_state = TYPEC_UNATTACHED, new_state = TYPEC_UNATTACHED;

	switch (event) {
	case TCP_NOTIFY_SINK_VBUS:
		handle_audio_attach(mci, idx, noti);
		break;
	case TCP_NOTIFY_PD_STATE:
		handle_pd_rdy_attach(mci, idx, noti);
		break;
	case TCP_NOTIFY_TYPEC_STATE:
		old_state = noti->typec_state.old_state;
		new_state = noti->typec_state.new_state;

		if (old_state == TYPEC_UNATTACHED &&
		    (new_state == TYPEC_ATTACHED_SNK ||
		     new_state == TYPEC_ATTACHED_NORP_SRC ||
		     new_state == TYPEC_ATTACHED_CUSTOM_SRC ||
		     new_state == TYPEC_ATTACHED_DBGACC_SNK)) {
			dev_info(mci->dev,
				 "%s Charger plug in, polarity = %d\n",
				 __func__, noti->typec_state.polarity);
			handle_typec_pd_attach(mci, idx, ATTACH_TYPE_TYPEC);
			typec_mode = get_source_mode(noti);
		} else if ((old_state == TYPEC_ATTACHED_SNK ||
			    old_state == TYPEC_ATTACHED_NORP_SRC ||
			    old_state == TYPEC_ATTACHED_CUSTOM_SRC ||
			    old_state == TYPEC_ATTACHED_DBGACC_SNK ||
			    old_state == TYPEC_ATTACHED_AUDIO) &&
			    new_state == TYPEC_UNATTACHED) {
			dev_info(mci->dev, "%s Charger plug out\n", __func__);
			typec_mode = PSY_TYPEC_NONE;
			handle_typec_pd_attach(mci, idx, ATTACH_TYPE_NONE);
		}else if (old_state == TYPEC_ATTACHED_SRC &&
				new_state == TYPEC_ATTACHED_SNK) {
			dev_info(mci->dev, "%s Source_to_Sink\n", __func__);
			typec_mode = PSY_TYPEC_SINK;
		}else if (old_state == TYPEC_ATTACHED_SNK &&
				new_state == TYPEC_ATTACHED_SRC) {
			dev_info(mci->dev, "%s Sink_to_Source\n", __func__);
			typec_mode = get_source_mode(noti);
		}else if (old_state == TYPEC_UNATTACHED &&
		    	new_state == TYPEC_ATTACHED_SRC) {
			dev_info(mci->dev, "%s None_to_Sink\n", __func__);
			typec_mode = PSY_TYPEC_SINK;
		} else if (old_state == TYPEC_UNATTACHED &&
		    	new_state == TYPEC_ATTACHED_AUDIO) {
			dev_info(mci->dev, "%s None_to_AUDIO\n", __func__);
			typec_mode = PSY_TYPEC_SINK_AUDIO_ADAPTER;
		} else if ((old_state == TYPEC_ATTACHED_SRC ||
				old_state == TYPEC_ATTACHED_AUDIO) &&
		    	new_state == TYPEC_UNATTACHED) {
			dev_info(mci->dev, "%s otg plug out\n", __func__);
			typec_mode = PSY_TYPEC_NONE;
		}
		break;
	case TCP_NOTIFY_PR_SWAP:
		if (noti->swap_state.new_role == PD_ROLE_SOURCE)
			handle_typec_pd_attach(mci, idx, ATTACH_TYPE_NONE);
		break;
	case TCP_NOTIFY_EXT_DISCHARGE:
		dev_info(mci->dev, "%s ext discharge = %d\n",
				    __func__, noti->en_state.en);
#if IS_ENABLED(CONFIG_MTK_CHARGER)
		if (!mci->chg_shared[idx])
			charger_dev_enable_discharge(mci->chg_dev[idx],
						     noti->en_state.en);
#endif /* CONFIG_MTK_CHARGER */
		break;
	default:
		break;
	};

	return NOTIFY_OK;
}

static void mtk_ctd_driver_remove_helper(struct mtk_ctd_info *mci)
{
	int i = 0, ret = 0;

	for (i = 0; i < mci->nr_port; i++) {
		if (!mci->tcpc[i])
			break;
		ret = unregister_tcp_dev_notifier(mci->tcpc[i],
						  &mci->pd_nb[i].nb,
						  TCP_NOTIFY_TYPE_ALL);
		if (ret < 0)
			break;
	}
	mutex_destroy(&mci->attach_lock);
}

#define MCI_DEVM_KCALLOC(member)					\
	(mci->member = devm_kcalloc(mci->dev, mci->nr_port,		\
				    sizeof(*mci->member), GFP_KERNEL))	\

static int mtk_ctd_probe(struct platform_device *pdev)
{
	struct mtk_ctd_info *mci = NULL;
	int ret = 0, i = 0;
	char name[16];
	struct device_node *np = pdev->dev.of_node;
#if IS_ENABLED(CONFIG_MTK_CHARGER)
	int j = 0;
	const char *str = NULL;
#endif /* CONFIG_MTK_CHARGER */

	dev_dbg(&pdev->dev, "%s ++\n", __func__);

	mci = devm_kzalloc(&pdev->dev, sizeof(*mci), GFP_KERNEL);
	if (!mci)
		return -ENOMEM;

	mci->dev = &pdev->dev;
	ret = of_property_read_u32(np, "nr_port", &mci->nr_port);
	if (ret < 0) {
		dev_notice(mci->dev, "Failed to read nr_port property(%d)\n",
				     ret);
		mci->nr_port = 1;
	}
#if IS_ENABLED(CONFIG_MTK_CHARGER)
	MCI_DEVM_KCALLOC(chg_dev);
	MCI_DEVM_KCALLOC(chg_shared);
	if (!mci->chg_dev || !mci->chg_shared)
		return -ENOMEM;
#endif /* CONFIG_MTK_CHARGER */
	MCI_DEVM_KCALLOC(bc12_sel);
	MCI_DEVM_KCALLOC(bc12_psy);
	MCI_DEVM_KCALLOC(tcpc);
	MCI_DEVM_KCALLOC(pd_nb);
	MCI_DEVM_KCALLOC(typec_attach);
	if (!mci->bc12_sel || !mci->bc12_psy ||
	    !mci->tcpc || !mci->pd_nb || !mci->typec_attach)
		return -ENOMEM;
	init_waitqueue_head(&mci->attach_wq);
	mutex_init(&mci->attach_lock);
	platform_set_drvdata(pdev, mci);

	for (i = 0; i < mci->nr_port; i++) {
#if IS_ENABLED(CONFIG_MTK_CHARGER)
		ret = snprintf(name, sizeof(name), "chg_name_port%d", i);
		if (ret >= sizeof(name))
			dev_notice(mci->dev, "chg_name name is truncated\n");

		ret = of_property_read_string(np, name, &str);
		if (ret < 0) {
			dev_notice(mci->dev, "Failed to read %s property(%d)\n",
					     name, ret);
			str = "primary_chg";
		}

		mci->chg_dev[i] = get_charger_by_name(str);
		if (!mci->chg_dev[i]) {
			dev_notice(mci->dev, "Failed to get %s chg_dev\n", str);
			ret = -ENODEV;
			goto out;
		}

		for (j = 0; j < i; j++) {
			if (mci->chg_dev[j] == mci->chg_dev[i]) {
				mci->chg_shared[j] = true;
				mci->chg_shared[i] = true;
				break;
			}
		}
#endif /* CONFIG_MTK_CHARGER */

		ret = snprintf(name, sizeof(name), "bc12_sel_port%d", i);
		if (ret >= sizeof(name))
			dev_notice(mci->dev, "bc12_sel name is truncated\n");

		ret = of_property_read_u32(np, name, &mci->bc12_sel[i]);
		if (ret < 0) {
			dev_notice(mci->dev, "Failed to read %s property(%d)\n",
					     name, ret);
			mci->bc12_sel[i] = MTK_CTD_BY_SUBPMIC;
		}

		if (mci->bc12_sel[i] == MTK_CTD_BY_SUBPMIC_PWR_RDY)
			goto skip_get_psy;

		ret = snprintf(name, sizeof(name), "bc12_psy_port%d", i);
		if (ret >= sizeof(name))
			dev_notice(mci->dev, "bc12_psy name is truncated\n");

		mci->bc12_psy[i] = devm_power_supply_get_by_phandle(mci->dev,
								    name);
		if (IS_ERR_OR_NULL(mci->bc12_psy[i])) {
			dev_notice(mci->dev, "Failed to get %s\n", name);
			ret = -ENODEV;
			goto out;
		}
skip_get_psy:
		ret = snprintf(name, sizeof(name), "type_c_port%d", i);
		if (ret >= sizeof(name))
			dev_notice(mci->dev, "type_c name is truncated\n");

		mci->tcpc[i] = tcpc_dev_get_by_name(name);
		if (!mci->tcpc[i]) {
			dev_notice(mci->dev, "Failed to get %s\n", name);
			ret = -ENODEV;
			goto out;
		}

		mci->pd_nb[i].nb.notifier_call = pd_tcp_notifier_call;
		mci->pd_nb[i].mci = mci;
		ret = register_tcp_dev_notifier(mci->tcpc[i], &mci->pd_nb[i].nb,
						TCP_NOTIFY_TYPE_ALL);
		if (ret < 0) {
			dev_notice(mci->dev,
				   "Failed to register %s notifier(%d)",
				   name, ret);
			goto out;
		}
	}

	mci->attach_task = kthread_run(typec_attach_thread, mci,
				       "attach_thread");
	if (IS_ERR(mci->attach_task)) {
		ret = PTR_ERR(mci->attach_task);
		dev_notice(mci->dev, "Failed to run attach kthread(%d)\n", ret);
		goto out;
	}
	dev_info(mci->dev, "%s successfully\n", __func__);

	return 0;
out:
	mtk_ctd_driver_remove_helper(mci);

	return ret;
}

static int mtk_ctd_remove(struct platform_device *pdev)
{
	struct mtk_ctd_info *mci = platform_get_drvdata(pdev);

	dev_dbg(mci->dev, "%s\n", __func__);
	kthread_stop(mci->attach_task);
	mtk_ctd_driver_remove_helper(mci);
	return 0;
}

static const struct of_device_id __maybe_unused mtk_ctd_of_id[] = {
	{ .compatible = "mediatek,mtk_ctd", },
	{}
};
MODULE_DEVICE_TABLE(of, mtk_ctd_of_id);

static struct platform_driver mtk_ctd_driver = {
	.driver = {
		.name = "mtk_ctd",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(mtk_ctd_of_id),
	},
	.probe = mtk_ctd_probe,
	.remove = mtk_ctd_remove,
};

static int __init mtk_ctd_init(void)
{
	return platform_driver_register(&mtk_ctd_driver);
}
device_initcall_sync(mtk_ctd_init);

static void __exit mtk_ctd_exit(void)
{
	platform_driver_unregister(&mtk_ctd_driver);
}
module_exit(mtk_ctd_exit);

MODULE_AUTHOR("Gene Chen <gene_chen@richtek.com>");
MODULE_DESCRIPTION("MTK CHARGER TYPE DETECT Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(MTK_CTD_DRV_VERSION);
