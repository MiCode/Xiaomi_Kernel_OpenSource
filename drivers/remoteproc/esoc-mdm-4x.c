// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2014-2015, 2017-2021, The Linux Foundation. All rights reserved.
 */

#include <linux/coresight.h>
#include <linux/coresight-cti.h>
#include <linux/workqueue.h>
#include <linux/sched/clock.h>
#include <linux/regulator/consumer.h>
#include <soc/qcom/sysmon.h>
#include "esoc-mdm.h"

enum gpio_update_config {
	GPIO_UPDATE_BOOTING_CONFIG = 1,
	GPIO_UPDATE_RUNNING_CONFIG,
};

enum irq_mask {
	IRQ_ERRFATAL = 0x1,
	IRQ_STATUS = 0x2,
	IRQ_PBLRDY = 0x4,
};


static struct gpio_map {
	const char *name;
	int index;
} gpio_map[] = {
	{"qcom,mdm2ap-errfatal-gpio",    MDM2AP_ERRFATAL},
	{"qcom,ap2mdm-errfatal-gpio",    AP2MDM_ERRFATAL},
	{"qcom,mdm2ap-status-gpio",      MDM2AP_STATUS},
	{"qcom,ap2mdm-status-gpio",      AP2MDM_STATUS},
	{"qcom,mdm2ap-pblrdy-gpio",      MDM2AP_PBLRDY},
	{"qcom,ap2mdm-wakeup-gpio",      AP2MDM_WAKEUP},
	{"qcom,ap2mdm-chnlrdy-gpio",     AP2MDM_CHNLRDY},
	{"qcom,mdm2ap-wakeup-gpio",      MDM2AP_WAKEUP},
	{"qcom,ap2mdm-vddmin-gpio",      AP2MDM_VDDMIN},
	{"qcom,mdm2ap-vddmin-gpio",      MDM2AP_VDDMIN},
	{"qcom,ap2mdm-pmic-pwr-en-gpio", AP2MDM_PMIC_PWR_EN},
	{"qcom,mdm-link-detect-gpio",	 MDM_LINK_DETECT},
};

/* Required gpios */
static const int required_gpios[] = {
	MDM2AP_ERRFATAL,
	AP2MDM_ERRFATAL,
	MDM2AP_STATUS,
	AP2MDM_STATUS,
};

void *ipc_log;

static void mdm_debug_gpio_show(struct mdm_ctrl *mdm)
{
	struct device *dev = mdm->dev;

	dev_dbg(dev, "%s: MDM2AP_ERRFATAL gpio = %d\n", __func__, MDM_GPIO(mdm, MDM2AP_ERRFATAL));
	dev_dbg(dev, "%s: AP2MDM_ERRFATAL gpio = %d\n", __func__, MDM_GPIO(mdm, AP2MDM_ERRFATAL));
	dev_dbg(dev, "%s: MDM2AP_STATUS gpio = %d\n", __func__, MDM_GPIO(mdm, MDM2AP_STATUS));
	dev_dbg(dev, "%s: AP2MDM_STATUS gpio = %d\n", __func__, MDM_GPIO(mdm, AP2MDM_STATUS));
	dev_dbg(dev, "%s: AP2MDM_SOFT_RESET gpio = %d\n",
		__func__, MDM_GPIO(mdm, AP2MDM_SOFT_RESET));
	dev_dbg(dev, "%s: MDM2AP_WAKEUP gpio = %d\n", __func__, MDM_GPIO(mdm, MDM2AP_WAKEUP));
	dev_dbg(dev, "%s: AP2MDM_WAKEUP gpio = %d\n", __func__, MDM_GPIO(mdm, AP2MDM_WAKEUP));
	dev_dbg(dev, "%s: AP2MDM_PMIC_PWR_EN gpio = %d\n",
		__func__, MDM_GPIO(mdm, AP2MDM_PMIC_PWR_EN));
	dev_dbg(dev, "%s: MDM2AP_PBLRDY gpio = %d\n", __func__, MDM_GPIO(mdm, MDM2AP_PBLRDY));
	dev_dbg(dev, "%s: AP2MDM_VDDMIN gpio = %d\n", __func__, MDM_GPIO(mdm, AP2MDM_VDDMIN));
	dev_dbg(dev, "%s: MDM2AP_VDDMIN gpio = %d\n", __func__, MDM_GPIO(mdm, MDM2AP_VDDMIN));
}

static void mdm_debug_gpio_ipc_log(struct mdm_ctrl *mdm)
{
	esoc_mdm_log("MDM2AP_ERRFATAL gpio = %d\n", MDM_GPIO(mdm, MDM2AP_ERRFATAL));
	esoc_mdm_log("AP2MDM_ERRFATAL gpio = %d\n", MDM_GPIO(mdm, AP2MDM_ERRFATAL));
	esoc_mdm_log("MDM2AP_STATUS gpio = %d\n", MDM_GPIO(mdm, MDM2AP_STATUS));
	esoc_mdm_log("AP2MDM_STATUS gpio = %d\n", MDM_GPIO(mdm, AP2MDM_STATUS));
	esoc_mdm_log("AP2MDM_SOFT_RESET gpio = %d\n", MDM_GPIO(mdm, AP2MDM_SOFT_RESET));
}

static void mdm_enable_irqs(struct mdm_ctrl *mdm)
{
	if (!mdm)
		return;
	esoc_mdm_log("Enabling the interrupts\n");
	if (mdm->irq_mask & IRQ_ERRFATAL) {
		enable_irq(mdm->errfatal_irq);
		mdm->irq_mask &= ~IRQ_ERRFATAL;
	}
	if (mdm->irq_mask & IRQ_STATUS) {
		enable_irq(mdm->status_irq);
		mdm->irq_mask &= ~IRQ_STATUS;
	}
	if (mdm->irq_mask & IRQ_PBLRDY) {
		enable_irq(mdm->pblrdy_irq);
		mdm->irq_mask &= ~IRQ_PBLRDY;
	}
}

void mdm_disable_irqs(struct mdm_ctrl *mdm)
{
	if (!mdm)
		return;
	esoc_mdm_log("Disabling the interrupts\n");
	if (!(mdm->irq_mask & IRQ_ERRFATAL)) {
		disable_irq_nosync(mdm->errfatal_irq);
		mdm->irq_mask |= IRQ_ERRFATAL;
	}
	if (!(mdm->irq_mask & IRQ_STATUS)) {
		disable_irq_nosync(mdm->status_irq);
		mdm->irq_mask |= IRQ_STATUS;
	}
	if (!(mdm->irq_mask & IRQ_PBLRDY)) {
		disable_irq_nosync(mdm->pblrdy_irq);
		mdm->irq_mask |= IRQ_PBLRDY;
	}
}

static void mdm_deconfigure_ipc(struct mdm_ctrl *mdm)
{
	int i;

	for (i = 0; i < NUM_GPIOS; ++i) {
		if (gpio_is_valid(MDM_GPIO(mdm, i)))
			gpio_free(MDM_GPIO(mdm, i));
	}
	if (mdm->mdm_queue) {
		destroy_workqueue(mdm->mdm_queue);
		mdm->mdm_queue = NULL;
	}
}

static void mdm_update_gpio_configs(struct mdm_ctrl *mdm, enum gpio_update_config gpio_config)
{
	struct pinctrl_state *pins_state = NULL;
	/* Some gpio configuration may need updating after modem bootup.*/
	switch (gpio_config) {
	case GPIO_UPDATE_RUNNING_CONFIG:
		pins_state = mdm->gpio_state_running;
		break;
	case GPIO_UPDATE_BOOTING_CONFIG:
		pins_state = mdm->gpio_state_booting;
		break;
	default:
		pins_state = NULL;
		dev_err(mdm->dev, "%s: called with no config\n", __func__);
		break;
	}
	if (pins_state != NULL) {
		if (pinctrl_select_state(mdm->pinctrl, pins_state))
			dev_err(mdm->dev, "switching gpio config failed\n");
	}
}

static void mdm_trigger_dbg(struct mdm_ctrl *mdm)
{
	int ret;

	if (mdm->dbg_mode && !mdm->trig_cnt) {
		ret = coresight_cti_pulse_trig(mdm->cti, MDM_CTI_CH);
		mdm->trig_cnt++;
		if (ret)
			dev_err(mdm->dev, "unable to trigger cti pulse on\n");
	}
}

static int mdm_cmd_exe(enum esoc_cmd cmd, struct esoc_clink *esoc)
{
	struct mdm_ctrl *mdm = get_esoc_clink_data(esoc);
	bool graceful_shutdown = false;
	u32 status, err_fatal;

	switch (cmd) {
	case ESOC_PWR_ON:
		if (esoc->auto_boot) {
			/*
			 * If esoc has already booted, we would have missed
			 * status change interrupt. Read status and err_fatal
			 * signals to arrive at the state of esoc.
			 */
			esoc->clink_ops->get_status(&status, esoc);
			esoc->clink_ops->get_err_fatal(&err_fatal, esoc);
			if (err_fatal)
				return -EIO;
			if (status && !mdm->ready) {
				mdm->ready = true;
				esoc->clink_ops->notify(ESOC_BOOT_DONE, esoc);
			}
		}
		esoc_mdm_log("ESOC_PWR_ON: Setting AP2MDM_ERRFATAL = 0\n");
		gpio_set_value(MDM_GPIO(mdm, AP2MDM_ERRFATAL), 0);
		mdm->init = 1;
		mdm_do_first_power_on(mdm);
		mdm_enable_irqs(mdm);
		break;
	case ESOC_PWR_OFF:
		mdm_disable_irqs(mdm);
		mdm->debug = 0;
		mdm->ready = false;
		mdm->trig_cnt = 0;
		if (esoc->primary)
			break;
		graceful_shutdown = true;
		/* as a part of the sysmon rproc subdevice we send
		 * the power off request so nothing to do here
		 */
		if (esoc->userspace_handle_shutdown)
			esoc_clink_queue_request(ESOC_REQ_SEND_SHUTDOWN, esoc);
		break;
	case ESOC_FORCE_PWR_OFF:
		if (!qcom_sysmon_shutdown_acked(esoc->rproc_sysmon)) {
			mdm_disable_irqs(mdm);
			mdm->debug = 0;
			mdm->ready = false;
			mdm->trig_cnt = 0;

			dev_err(mdm->dev, "Graceful shutdown fail\n");
		}

		if (esoc->primary)
			break;
		/*
		 * Force a shutdown of the mdm. This is required in order
		 * to prevent the mdm from immediately powering back on
		 * after the shutdown. Avoid setting status to 0, if line is
		 * monitored by multiple mdms(might be wrongly interpreted as
		 * a primary crash).
		 */
		if (!esoc->statusline_not_a_powersource) {
			esoc_mdm_log("ESOC_FORCE_PWR_OFF: setting AP2MDM_STATUS = 0\n");
			gpio_set_value(MDM_GPIO(mdm, AP2MDM_STATUS), 0);
		}
		esoc_mdm_log("ESOC_FORCE_PWR_OFF: Queueing request: ESOC_REQ_SHUTDOWN\n");
		esoc_clink_queue_request(ESOC_REQ_SHUTDOWN, esoc);
		mdm_power_down(mdm);
		mdm_update_gpio_configs(mdm, GPIO_UPDATE_BOOTING_CONFIG);
		break;
	case ESOC_RESET:
		esoc_mdm_log("ESOC_RESET: Resetting the modem\n");
		mdm_toggle_soft_reset(mdm, false);
		break;
	case ESOC_PREPARE_DEBUG:
		/*
		 * disable all irqs except request irq (pblrdy)
		 * force a reset of the mdm by signaling
		 * an APQ crash, wait till mdm is ready for ramdumps.
		 */
		mdm->ready = false;
		esoc_mdm_log("ESOC_PREPARE_DEBUG: Cancelling the status check work\n");
		cancel_delayed_work(&mdm->mdm2ap_status_check_work);
		if (!mdm->esoc->auto_boot) {
			esoc_mdm_log("ESOC_PREPARE_DEBUG: setting AP2MDM_ERRFATAL = 1\n");
			gpio_set_value(MDM_GPIO(mdm, AP2MDM_ERRFATAL), 1);
			dev_dbg(mdm->dev, "set ap2mdm errfatal to force reset\n");
			msleep(mdm->ramdump_delay_ms);
		}
		break;
	case ESOC_EXE_DEBUG:
		mdm->trig_cnt = 0;

		if (mdm->skip_restart_for_mdm_crash)
			break;

		esoc_mdm_log("ESOC_EXE_DEBUG: Resetting the modem\n");
		mdm->debug = 1;
		mdm_toggle_soft_reset(mdm, false);
		/*
		 * wait for ramdumps to be collected
		 * then power down the mdm and switch gpios to booting
		 * config
		 */
		esoc_mdm_log("ESOC_EXE_DEBUG: Waiting for ramdumps to be collected\n");
		wait_for_completion(&mdm->debug_done);
		if (mdm->debug_fail) {
			esoc_mdm_log("ESOC_EXE_DEBUG: Failed to collect the ramdumps\n");
			dev_err(mdm->dev, "unable to collect ramdumps\n");
			mdm->debug = 0;
			return -EIO;
		}
		dev_dbg(mdm->dev, "ramdump collection done\n");
		mdm->debug = 0;
		init_completion(&mdm->debug_done);
		break;
	case ESOC_EXIT_DEBUG:
		/*
		 * Deassert APQ to mdm err fatal
		 * Power on the mdm
		 */
		esoc_mdm_log("ESOC_EXIT_DEBUG: Setting AP2MDM_ERRFATAL = 0\n");
		gpio_set_value(MDM_GPIO(mdm, AP2MDM_ERRFATAL), 0);
		dev_dbg(mdm->dev, "exiting debug state after power on\n");
		mdm->get_restart_reason = true;
		break;
	default:
		esoc_mdm_log("Invalid command\n");
		return -EINVAL;
	}
	return 0;
}

static void mdm2ap_status_check(struct work_struct *work)
{
	struct mdm_ctrl *mdm = container_of(work, struct mdm_ctrl, mdm2ap_status_check_work.work);
	struct device *dev = mdm->dev;
	struct esoc_clink *esoc = mdm->esoc;

	esoc_mdm_log("Powerup timer expired after images are transferred to modem\n");

	if (gpio_get_value(MDM_GPIO(mdm, MDM2AP_STATUS)) == 0) {
		esoc_mdm_log("MDM2AP_STATUS did not go high\n");
		dev_dbg(dev, "MDM2AP_STATUS did not go high\n");
		esoc_clink_evt_notify(ESOC_UNEXPECTED_RESET, esoc);
	}
}

static void mdm_status_fn(struct work_struct *work)
{
	struct mdm_ctrl *mdm = container_of(work, struct mdm_ctrl, mdm_status_work);
	struct device *dev = mdm->dev;
	int value = gpio_get_value(MDM_GPIO(mdm, MDM2AP_STATUS));

	dev_dbg(dev, "%s: status:%d\n", __func__, value);
	/* Update gpio configuration to "running" config. */
	mdm_update_gpio_configs(mdm, GPIO_UPDATE_RUNNING_CONFIG);
}

static void mdm_get_restart_reason(struct work_struct *work)
{

	int ret = 0;
	int ntries = 0;
	char sfr_buf[RD_BUF_SIZE];
	struct mdm_ctrl *mdm = container_of(work, struct mdm_ctrl, restart_reason_work);
	struct device *dev = mdm->dev;

	do {
		qcom_sysmon_get_reason(mdm->esoc->rproc_sysmon, sfr_buf, sizeof(sfr_buf));
		if (!ret) {
			esoc_mdm_log("restart reason is %s\n", sfr_buf);
			dev_err(dev, "mdm restart reason is %s\n", sfr_buf);
			break;
		}
		msleep(SFR_RETRY_INTERVAL);
	} while (++ntries < SFR_MAX_RETRIES);
	if (ntries == SFR_MAX_RETRIES) {
		esoc_mdm_log("restart reason not obtained. err:\n");
		dev_dbg(dev, "%s: Error retrieving restart reason:\n",
						__func__);
	}
	mdm->get_restart_reason = false;

}

void mdm_wait_for_status_low(struct mdm_ctrl *mdm, bool atomic)
{
	uint64_t timeout;
	uint64_t now;

	esoc_mdm_log("Waiting for MDM2AP_STATUS to go LOW\n");
	timeout = local_clock();
	do_div(timeout, NSEC_PER_MSEC);
	timeout += MDM_MODEM_TIMEOUT;
	do {
		if (gpio_get_value(MDM_GPIO(mdm, MDM2AP_STATUS)) == 0) {
			esoc_mdm_log("MDM2AP_STATUS went LOW\n");
			return;
		}
		now = local_clock();
		do_div(now, NSEC_PER_MSEC);
	} while (!time_after64(now, timeout));

	esoc_mdm_log("MDM2AP_STATUS didn't go LOW. Warm-resetting modem\n");
	dev_err(mdm->dev, "MDM2AP status did not go low\n");

	mdm_toggle_soft_reset(mdm, atomic);
}

static void mdm_notify(enum esoc_notify notify, struct esoc_clink *esoc)
{
	struct mdm_ctrl *mdm = get_esoc_clink_data(esoc);
	struct device *dev = mdm->dev;

	esoc_mdm_log("Notification: %d\n", notify);

	switch (notify) {
	case ESOC_IMG_XFER_DONE:
		if (gpio_get_value(MDM_GPIO(mdm, MDM2AP_STATUS)) ==  0) {
			esoc_mdm_log(
			"ESOC_IMG_XFER_DONE: Begin timeout of %lu ms for modem_status\n",
			MDM2AP_STATUS_TIMEOUT_MS);
			schedule_delayed_work(&mdm->mdm2ap_status_check_work,
				msecs_to_jiffies(MDM2AP_STATUS_TIMEOUT_MS));
		}
		break;
	case ESOC_BOOT_DONE:
		esoc_mdm_log("ESOC_BOOT_DONE: Sending the notification: ESOC_RUN_STATE\n");
		esoc_clink_evt_notify(ESOC_RUN_STATE, esoc);
		break;
	case ESOC_IMG_XFER_RETRY:
		esoc_mdm_log("ESOC_IMG_XFER_RETRY: Resetting the device\n");
		mdm->init = 1;
		mdm_toggle_soft_reset(mdm, false);
		break;
	case ESOC_IMG_XFER_FAIL:
		esoc_mdm_log("ESOC_IMG_XFER_FAIL: Send notification: ESOC_INVALID_STATE\n");
		esoc_clink_evt_notify(ESOC_INVALID_STATE, esoc);
		break;
	case ESOC_BOOT_FAIL:
		esoc_mdm_log("ESOC_BOOT_FAIL: Send notification: ESOC_INVALID_STATE\n");
		esoc_clink_evt_notify(ESOC_INVALID_STATE, esoc);
		break;
	case ESOC_PON_RETRY:
		esoc_mdm_log("ESOC_PON_RETRY: Send notification: ESOC_RETRY_PON_EVT\n");
		esoc_clink_evt_notify(ESOC_RETRY_PON_EVT, esoc);
		break;
	case ESOC_UPGRADE_AVAILABLE:
		break;
	case ESOC_DEBUG_DONE:
		esoc_mdm_log("ESOC_DEBUG_DONE: Ramdumps collection done\n");
		mdm->debug_fail = false;
		mdm_update_gpio_configs(mdm, GPIO_UPDATE_BOOTING_CONFIG);
		complete(&mdm->debug_done);
		break;
	case ESOC_DEBUG_FAIL:
		esoc_mdm_log("ESOC_DEBUG_FAIL: Ramdumps collection failed\n");
		mdm->debug_fail = true;
		complete(&mdm->debug_done);
		break;
	case ESOC_PRIMARY_CRASH:
		mdm_disable_irqs(mdm);
		dev_dbg(dev, "signal apq err fatal for graceful restart\n");
		esoc_mdm_log("ESOC_PRIMARY_CRASH: Setting AP2MDM_ERRFATAL = 1\n");
		gpio_set_value(MDM_GPIO(mdm, AP2MDM_ERRFATAL), 1);
		if (esoc->primary)
			break;
		mdm_wait_for_status_low(mdm, true);
		break;
	case ESOC_PRIMARY_REBOOT:
		mdm_disable_irqs(mdm);
		mdm->debug = 0;
		mdm->ready = false;
		esoc_mdm_log("ESOC_PRIMARY_REBOOT: Powering down the modem\n");
		mdm_power_down(mdm);
		break;
	}
}

static irqreturn_t mdm_errfatal(int irq, void *dev_id)
{
	struct mdm_ctrl *mdm = (struct mdm_ctrl *)dev_id;
	struct esoc_clink *esoc;
	struct device *dev;

	if (!mdm)
		goto no_mdm_irq;
	dev = mdm->dev;
	if (!mdm->ready)
		goto mdm_pwroff_irq;
	esoc = mdm->esoc;
	esoc_mdm_log("MDM2AP_ERRFATAL IRQ received!\n");
	dev_err(dev, "%s: mdm sent errfatal interrupt\n",
					__func__);
	/* disable irq ?*/
	esoc_clink_evt_notify(ESOC_ERR_FATAL, esoc);
	return IRQ_HANDLED;
mdm_pwroff_irq:
	esoc_mdm_log("MDM2AP_ERRFATAL IRQ received before modem booted. Ignoring.\n");
	dev_info(dev, "errfatal irq when in pwroff\n");
no_mdm_irq:
	return IRQ_HANDLED;
}

static irqreturn_t mdm_status_change(int irq, void *dev_id)
{
	int value;
	struct esoc_clink *esoc;
	struct device *dev;
	struct mdm_ctrl *mdm = (struct mdm_ctrl *)dev_id;

	if (!mdm)
		return IRQ_HANDLED;
	dev = mdm->dev;
	esoc = mdm->esoc;
	esoc_mdm_log("MDM2AP_STATUS IRQ received!\n");
	/*
	 * On auto boot devices, there is a possibility of receiving
	 * status change interrupt before esoc_clink structure is
	 * initialized. Ignore them.
	 */
	if (!esoc) {
		esoc_mdm_log("Unexpected IRQ. Ignoring.\n");
		return IRQ_HANDLED;
	}
	value = gpio_get_value(MDM_GPIO(mdm, MDM2AP_STATUS));
	if (value == 0 && mdm->ready) {
		esoc_mdm_log("Unexpected reset of external modem\n");
		dev_err(dev, "unexpected reset external modem\n");
		esoc_clink_evt_notify(ESOC_UNEXPECTED_RESET, esoc);
	} else if (value == 1) {
		/*
		 * In auto_boot cases, bailout early if mdm
		 * is up already.
		 */
		if (esoc->auto_boot && mdm->ready)
			return IRQ_HANDLED;

		esoc_mdm_log("Modem ready. Cancelling the status_check work\n");
		cancel_delayed_work(&mdm->mdm2ap_status_check_work);
		dev_dbg(dev, "status = 1: mdm is now ready\n");
		mdm->ready = true;
		esoc_clink_evt_notify(ESOC_BOOT_STATE, esoc);
		mdm_trigger_dbg(mdm);
		queue_work(mdm->mdm_queue, &mdm->mdm_status_work);
		if (mdm->get_restart_reason)
			queue_work(mdm->mdm_queue, &mdm->restart_reason_work);
		if (esoc->auto_boot)
			esoc->clink_ops->notify(ESOC_BOOT_DONE, esoc);
	}
	return IRQ_HANDLED;
}

static irqreturn_t mdm_pblrdy_change(int irq, void *dev_id)
{
	struct mdm_ctrl *mdm;
	struct device *dev;
	struct esoc_clink *esoc;

	mdm = (struct mdm_ctrl *)dev_id;
	if (!mdm)
		return IRQ_HANDLED;
	esoc = mdm->esoc;
	dev = mdm->dev;
	dev_dbg(dev, "pbl ready %d:\n", gpio_get_value(MDM_GPIO(mdm, MDM2AP_PBLRDY)));
	if (mdm->init) {
		mdm->init = 0;
		mdm_trigger_dbg(mdm);
		esoc_clink_queue_request(ESOC_REQ_IMG, esoc);
		return IRQ_HANDLED;
	}
	if (mdm->debug)
		esoc_clink_queue_request(ESOC_REQ_DEBUG, esoc);
	return IRQ_HANDLED;
}

static void mdm_get_status(u32 *status, struct esoc_clink *esoc)
{
	struct mdm_ctrl *mdm = get_esoc_clink_data(esoc);

	if (gpio_get_value(MDM_GPIO(mdm, MDM2AP_STATUS)) == 0)
		*status = 0;
	else
		*status = 1;
}

static void mdm_get_err_fatal(u32 *status, struct esoc_clink *esoc)
{
	struct mdm_ctrl *mdm = get_esoc_clink_data(esoc);

	if (gpio_get_value(MDM_GPIO(mdm, MDM2AP_ERRFATAL)) == 0)
		*status = 0;
	else
		*status = 1;
}

/* Fail if any of the required gpios is absent. */
static int mdm_dt_parse_gpios(struct mdm_ctrl *mdm)
{
	int i, val, rc = 0;
	struct device_node *node = mdm->dev->of_node;

	for (i = 0; i < NUM_GPIOS; i++)
		mdm->gpios[i] = INVALID_GPIO;

	for (i = 0; i < ARRAY_SIZE(gpio_map); i++) {
		val = of_get_named_gpio(node, gpio_map[i].name, 0);
		if (val >= 0)
			MDM_GPIO(mdm, gpio_map[i].index) = val;
	}
	/* These two are special because they can be inverted. */
	/* Verify that the required gpios have valid values */
	for (i = 0; i < ARRAY_SIZE(required_gpios); i++) {
		if (MDM_GPIO(mdm, required_gpios[i]) == INVALID_GPIO) {
			rc = -ENXIO;
			break;
		}
	}
	mdm_debug_gpio_show(mdm);
	return rc;
}

static int mdm_configure_ipc(struct mdm_ctrl *mdm, struct platform_device *pdev)
{
	int ret = -1;
	int irq;
	struct device *dev = mdm->dev;
	struct device_node *node = pdev->dev.of_node;

	ret = of_property_read_u32(node, "qcom,ramdump-timeout-ms", &mdm->dump_timeout_ms);
	if (ret)
		mdm->dump_timeout_ms = DEF_RAMDUMP_TIMEOUT;
	ret = of_property_read_u32(node, "qcom,ramdump-delay-ms", &mdm->ramdump_delay_ms);
	if (ret)
		mdm->ramdump_delay_ms = DEF_RAMDUMP_DELAY;
	/*
	 * In certain scenarios, multiple esoc devices are monitoring
	 * same AP2MDM_STATUS line. But only one of them will have a
	 * successful gpio_request call. Initialize gpio only if request
	 * succeeds.
	 */
	if (gpio_request(MDM_GPIO(mdm, AP2MDM_STATUS), "AP2MDM_STATUS"))
		dev_err(dev, "Failed to configure AP2MDM_STATUS gpio\n");
	else
		gpio_direction_output(MDM_GPIO(mdm, AP2MDM_STATUS), 0);
	if (gpio_request(MDM_GPIO(mdm, AP2MDM_ERRFATAL), "AP2MDM_ERRFATAL"))
		dev_err(dev, "%s Failed to configure AP2MDM_ERRFATAL gpio\n", __func__);
	else
		gpio_direction_output(MDM_GPIO(mdm, AP2MDM_ERRFATAL), 0);
	if (gpio_request(MDM_GPIO(mdm, MDM2AP_STATUS), "MDM2AP_STATUS")) {
		dev_err(dev, "%s Failed to configure MDM2AP_STATUS gpio\n", __func__);
		goto fatal_err;
	}
	if (gpio_request(MDM_GPIO(mdm, MDM2AP_ERRFATAL), "MDM2AP_ERRFATAL")) {
		dev_err(dev, "%s Failed to configure MDM2AP_ERRFATAL gpio\n", __func__);
		goto fatal_err;
	}
	if (gpio_is_valid(MDM_GPIO(mdm, MDM2AP_PBLRDY))) {
		if (gpio_request(MDM_GPIO(mdm, MDM2AP_PBLRDY), "MDM2AP_PBLRDY")) {
			dev_err(dev, "Cannot configure MDM2AP_PBLRDY gpio\n");
			goto fatal_err;
		}
	}
	if (gpio_is_valid(MDM_GPIO(mdm, AP2MDM_WAKEUP))) {
		if (gpio_request(MDM_GPIO(mdm, AP2MDM_WAKEUP), "AP2MDM_WAKEUP")) {
			dev_err(dev, "Cannot configure AP2MDM_WAKEUP gpio\n");
			goto fatal_err;
		}
	}
	if (gpio_is_valid(MDM_GPIO(mdm, AP2MDM_CHNLRDY))) {
		if (gpio_request(MDM_GPIO(mdm, AP2MDM_CHNLRDY), "AP2MDM_CHNLRDY")) {
			dev_err(dev, "Cannot configure AP2MDM_CHNLRDY gpio\n");
			goto fatal_err;
		}
	}

	if (gpio_is_valid(MDM_GPIO(mdm, AP2MDM_CHNLRDY)))
		gpio_direction_output(MDM_GPIO(mdm, AP2MDM_CHNLRDY), 0);

	gpio_direction_input(MDM_GPIO(mdm, MDM2AP_STATUS));
	gpio_direction_input(MDM_GPIO(mdm, MDM2AP_ERRFATAL));

	/* ERR_FATAL irq. */
	irq = gpio_to_irq(MDM_GPIO(mdm, MDM2AP_ERRFATAL));
	if (irq < 0) {
		dev_err(dev, "bad MDM2AP_ERRFATAL IRQ resource\n");
		goto errfatal_err;

	}
	ret = request_irq(irq, mdm_errfatal, IRQF_TRIGGER_RISING, "mdm errfatal", mdm);

	if (ret < 0) {
		dev_err(dev, "%s: MDM2AP_ERRFATAL IRQ#%d request failed,\n", __func__, irq);
		goto errfatal_err;
	}
	mdm->errfatal_irq = irq;
	irq_set_irq_wake(mdm->errfatal_irq, 1);

errfatal_err:
	 /* status irq */
	irq = gpio_to_irq(MDM_GPIO(mdm, MDM2AP_STATUS));
	if (irq < 0) {
		dev_err(dev, "%s: bad MDM2AP_STATUS IRQ resource, err = %d\n", __func__, irq);
		goto status_err;
	}
	ret = request_threaded_irq(irq, NULL, mdm_status_change,
		IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING | IRQF_ONESHOT, "mdm status", mdm);
	if (ret < 0) {
		dev_err(dev, "%s: MDM2AP_STATUS IRQ#%d request failed, err=%d\n",
			__func__, irq, ret);
		goto status_err;
	}
	mdm->status_irq = irq;
	irq_set_irq_wake(mdm->status_irq, 1);
status_err:
	if (gpio_is_valid(MDM_GPIO(mdm, MDM2AP_PBLRDY))) {
		irq =  platform_get_irq_byname(pdev, "plbrdy_irq");
		if (irq < 0) {
			dev_err(dev, "%s: MDM2AP_PBLRDY IRQ request failed\n", __func__);
			goto pblrdy_err;
		}

		ret = request_threaded_irq(irq, NULL, mdm_pblrdy_change,
				IRQF_TRIGGER_RISING | IRQF_ONESHOT,
				"mdm pbl ready", mdm);
		if (ret < 0) {
			dev_err(dev, "MDM2AP_PBL IRQ#%d request failed %d\n", irq, ret);
			goto pblrdy_err;
		}
		mdm->pblrdy_irq = irq;
	}
	mdm_disable_irqs(mdm);
pblrdy_err:
	return 0;
fatal_err:
	mdm_deconfigure_ipc(mdm);
	return ret;

}

static int mdm_pinctrl_init(struct mdm_ctrl *mdm)
{
	int retval = 0;

	mdm->pinctrl = devm_pinctrl_get(mdm->dev);
	if (IS_ERR_OR_NULL(mdm->pinctrl)) {
		retval = PTR_ERR(mdm->pinctrl);
		goto err_state_suspend;
	}
	mdm->gpio_state_booting = pinctrl_lookup_state(mdm->pinctrl, "mdm_booting");
	if (IS_ERR_OR_NULL(mdm->gpio_state_booting)) {
		mdm->gpio_state_running = NULL;
		mdm->gpio_state_booting = NULL;
	} else {
		mdm->gpio_state_running = pinctrl_lookup_state(mdm->pinctrl, "mdm_running");
		if (IS_ERR_OR_NULL(mdm->gpio_state_running)) {
			mdm->gpio_state_booting = NULL;
			mdm->gpio_state_running = NULL;
		}
	}
	mdm->gpio_state_active = pinctrl_lookup_state(mdm->pinctrl, "mdm_active");
	if (IS_ERR_OR_NULL(mdm->gpio_state_active)) {
		retval = PTR_ERR(mdm->gpio_state_active);
		goto err_state_active;
	}
	mdm->gpio_state_suspend = pinctrl_lookup_state(mdm->pinctrl, "mdm_suspend");
	if (IS_ERR_OR_NULL(mdm->gpio_state_suspend)) {
		retval = PTR_ERR(mdm->gpio_state_suspend);
		goto err_state_suspend;
	}
	retval = pinctrl_select_state(mdm->pinctrl, mdm->gpio_state_active);
	return retval;

err_state_suspend:
	mdm->gpio_state_active = NULL;
err_state_active:
	mdm->gpio_state_suspend = NULL;
	mdm->gpio_state_booting = NULL;
	mdm->gpio_state_running = NULL;
	return retval;
}

static void mdm_release_ipc_gpio(struct mdm_ctrl *mdm)
{
	int i;

	if (!mdm)
		return;

	for (i = 0; i < NUM_GPIOS; ++i)
		if (gpio_is_valid(MDM_GPIO(mdm, i)))
			gpio_free(MDM_GPIO(mdm, i));
}

static void mdm_free_irq(struct mdm_ctrl *mdm)
{
	if (!mdm)
		return;

	free_irq(mdm->errfatal_irq, mdm);
	free_irq(mdm->status_irq, mdm);
}

static int mdm9x55_setup_hw(struct mdm_ctrl *mdm,
					const struct mdm_ops *ops,
					struct platform_device *pdev)
{
	int ret;
	struct device_node *node;
	struct esoc_clink *esoc;
	const struct esoc_clink_ops *const clink_ops = ops->clink_ops;
	const struct mdm_pon_ops *pon_ops = ops->pon_ops;

	mdm->dev = &pdev->dev;
	mdm->pon_ops = pon_ops;
	node = pdev->dev.of_node;
	esoc = devm_kzalloc(mdm->dev, sizeof(*esoc), GFP_KERNEL);
	if (IS_ERR_OR_NULL(esoc)) {
		dev_err(mdm->dev, "cannot allocate esoc device\n");
		return PTR_ERR(esoc);
	}
	esoc->pdev = pdev;
	mdm->mdm_queue = alloc_workqueue("mdm_queue", 0, 0);
	if (!mdm->mdm_queue) {
		dev_err(mdm->dev, "could not create mdm_queue\n");
		return -ENOMEM;
	}
	mdm->irq_mask = 0;
	mdm->ready = false;
	ret = mdm_dt_parse_gpios(mdm);
	if (ret)
		return ret;
	dev_dbg(mdm->dev, "parsing gpio done\n");
	ret = mdm_pon_dt_init(mdm);
	if (ret)
		return ret;
	dev_dbg(mdm->dev, "pon dt init done\n");
	ret = mdm_pinctrl_init(mdm);
	if (ret)
		return ret;
	dev_dbg(mdm->dev, "pinctrl init done\n");
	ret = mdm_pon_setup(mdm);
	if (ret)
		return ret;
	dev_dbg(mdm->dev, "pon setup done\n");
	ret = mdm_configure_ipc(mdm, pdev);
	if (ret)
		return ret;
	dev_dbg(mdm->dev, "ipc configure done\n");
	esoc->name = MDM9x55_LABEL;
	mdm->dual_interface = of_property_read_bool(node, "qcom,mdm-dual-link");
	esoc->link_name = MDM9x55_PCIE;
	ret = of_property_read_string(node, "qcom,mdm-link-info", &esoc->link_info);
	if (ret)
		dev_info(mdm->dev, "esoc link info missing\n");

	ret = of_property_read_u32(node, "qcom,shutdown-timeout-ms", &mdm->shutdown_timeout_ms);
	if (ret)
		mdm->shutdown_timeout_ms = DEF_SHUTDOWN_TIMEOUT;

	ret = of_property_read_u32(node, "qcom,ssctl-instance-id", &esoc->ssctl_id);
	if (ret)
		dev_info(mdm->dev, "esoc ssctl id missing\n");

	esoc->sysmon_name = MDM9x55_LABEL;
	esoc->clink_ops = clink_ops;
	esoc->parent = mdm->dev;
	esoc->owner = THIS_MODULE;
	esoc->np = pdev->dev.of_node;

	esoc->auto_boot = of_property_read_bool(esoc->np, "qcom,mdm-auto-boot");
	esoc->statusline_not_a_powersource = of_property_read_bool(esoc->np,
				"qcom,mdm-statusline-not-a-powersource");
	esoc->userspace_handle_shutdown = of_property_read_bool(esoc->np,
				"qcom,mdm-userspace-handle-shutdown");
	set_esoc_clink_data(esoc, mdm);
	ret = esoc_clink_register(esoc);
	if (ret) {
		dev_err(mdm->dev, "esoc registration failed\n");
		return ret;
	}
	dev_dbg(mdm->dev, "esoc registration done\n");
	init_completion(&mdm->debug_done);
	INIT_WORK(&mdm->mdm_status_work, mdm_status_fn);
	INIT_WORK(&mdm->restart_reason_work, mdm_get_restart_reason);
	INIT_DELAYED_WORK(&mdm->mdm2ap_status_check_work, mdm2ap_status_check);
	mdm->get_restart_reason = false;
	mdm->debug_fail = false;
	mdm->esoc = esoc;
	mdm->init = 0;
	if (esoc->auto_boot)
		gpio_direction_output(MDM_GPIO(mdm, AP2MDM_STATUS), 1);
	return 0;
}

static int sdx50m_setup_hw(struct mdm_ctrl *mdm,
					const struct mdm_ops *ops,
					struct platform_device *pdev)
{
	int ret;
	struct device_node *node;
	struct esoc_clink *esoc;
	const struct esoc_clink_ops *const clink_ops = ops->clink_ops;
	const struct mdm_pon_ops *pon_ops = ops->pon_ops;

	mdm->dev = &pdev->dev;
	mdm->pon_ops = pon_ops;
	node = pdev->dev.of_node;

	esoc = devm_kzalloc(mdm->dev, sizeof(*esoc), GFP_KERNEL);
	if (IS_ERR_OR_NULL(esoc)) {
		dev_err(mdm->dev, "cannot allocate esoc device\n");
		return PTR_ERR(esoc);
	}
	esoc->pdev = pdev;

	mdm->mdm_queue = alloc_workqueue("mdm_queue", 0, 0);
	if (!mdm->mdm_queue) {
		dev_err(mdm->dev, "could not create mdm_queue\n");
		return -ENOMEM;
	}

	mdm->irq_mask = 0;
	mdm->ready = false;

	ret = mdm_dt_parse_gpios(mdm);
	if (ret) {
		esoc_mdm_log("Failed to parse DT gpios\n");
		dev_err(mdm->dev, "Failed to parse DT gpios\n");
		goto err_destroy_wrkq;
	}

	ret = mdm_pon_dt_init(mdm);
	if (ret) {
		esoc_mdm_log("Failed to parse PON DT gpios\n");
		dev_err(mdm->dev, "Failed to parse PON DT gpio\n");
		goto err_destroy_wrkq;
	}

	ret = mdm_pinctrl_init(mdm);
	if (ret) {
		esoc_mdm_log("Failed to init pinctrl\n");
		dev_err(mdm->dev, "Failed to init pinctrl\n");
		goto err_destroy_wrkq;
	}

	ret = mdm_pon_setup(mdm);
	if (ret) {
		esoc_mdm_log("Failed to setup PON\n");
		dev_err(mdm->dev, "Failed to setup PON\n");
		goto err_destroy_wrkq;
	}

	ret = mdm_configure_ipc(mdm, pdev);
	if (ret) {
		esoc_mdm_log("Failed to configure the ipc\n");
		dev_err(mdm->dev, "Failed to configure the ipc\n");
		goto err_release_ipc;
	}

	esoc->name = SDX50M_LABEL;
	mdm->dual_interface = of_property_read_bool(node, "qcom,mdm-dual-link");
	esoc->link_name = SDX50M_PCIE;
	ret = of_property_read_string(node, "qcom,mdm-link-info", &esoc->link_info);
	if (ret)
		dev_info(mdm->dev, "esoc link info missing\n");

	mdm->skip_restart_for_mdm_crash = of_property_read_bool(node,
				"qcom,esoc-skip-restart-for-mdm-crash");

	ret = of_property_read_u32(node, "qcom,ssctl-instance-id", &esoc->ssctl_id);
	if (ret)
		dev_info(mdm->dev, "esoc ssctl id missing\n");

	esoc->sysmon_name = SDX50M_LABEL;

	esoc->clink_ops = clink_ops;
	esoc->parent = mdm->dev;
	esoc->owner = THIS_MODULE;
	esoc->np = pdev->dev.of_node;
	set_esoc_clink_data(esoc, mdm);

	ret = esoc_clink_register(esoc);
	if (ret) {
		esoc_mdm_log("esoc registration failed\n");
		dev_err(mdm->dev, "esoc registration failed\n");
		goto err_free_irq;
	}
	dev_dbg(mdm->dev, "esoc registration done\n");
	esoc_mdm_log("Done configuring the GPIOs and esoc registration\n");

	init_completion(&mdm->debug_done);
	INIT_WORK(&mdm->mdm_status_work, mdm_status_fn);
	INIT_WORK(&mdm->restart_reason_work, mdm_get_restart_reason);
	INIT_DELAYED_WORK(&mdm->mdm2ap_status_check_work, mdm2ap_status_check);
	mdm->get_restart_reason = false;
	mdm->debug_fail = false;
	mdm->esoc = esoc;
	mdm->init = 0;

	mdm_debug_gpio_ipc_log(mdm);

	return 0;

err_free_irq:
	mdm_free_irq(mdm);
err_release_ipc:
	mdm_release_ipc_gpio(mdm);
err_destroy_wrkq:
	destroy_workqueue(mdm->mdm_queue);
	return ret;
}

static int sdx55m_setup_hw(struct mdm_ctrl *mdm, const struct mdm_ops *ops,
			   struct platform_device *pdev)
{
	int ret;
	struct device_node *node;
	struct esoc_clink *esoc;
	const struct esoc_clink_ops *const clink_ops = ops->clink_ops;
	const struct mdm_pon_ops *pon_ops = ops->pon_ops;

	mdm->dev = &pdev->dev;
	mdm->pon_ops = pon_ops;
	node = pdev->dev.of_node;

	esoc = devm_kzalloc(mdm->dev, sizeof(*esoc), GFP_KERNEL);
	if (IS_ERR_OR_NULL(esoc)) {
		dev_err(mdm->dev, "cannot allocate esoc device\n");
		return PTR_ERR(esoc);
	}
	esoc->pdev = pdev;

	mdm->mdm_queue = alloc_workqueue("mdm_queue", 0, 0);
	if (!mdm->mdm_queue) {
		dev_err(mdm->dev, "could not create mdm_queue\n");
		return -ENOMEM;
	}

	mdm->irq_mask = 0;
	mdm->ready = false;

	ret = mdm_dt_parse_gpios(mdm);
	if (ret) {
		esoc_mdm_log("Failed to parse DT gpios\n");
		dev_err(mdm->dev, "Failed to parse DT gpios\n");
		goto err_destroy_wrkq;
	}

	ret = mdm_pinctrl_init(mdm);
	if (ret) {
		esoc_mdm_log("Failed to init pinctrl\n");
		dev_err(mdm->dev, "Failed to init pinctrl\n");
		goto err_destroy_wrkq;
	}

	ret = mdm_configure_ipc(mdm, pdev);
	if (ret) {
		esoc_mdm_log("Failed to configure the ipc\n");
		dev_err(mdm->dev, "Failed to configure the ipc\n");
		goto err_release_ipc;
	}

	esoc->name = SDX55M_LABEL;
	mdm->dual_interface = of_property_read_bool(node, "qcom,mdm-dual-link");
	esoc->link_name = SDX55M_PCIE;
	ret = of_property_read_string(node, "qcom,mdm-link-info", &esoc->link_info);
	if (ret)
		dev_info(mdm->dev, "esoc link info missing\n");

	mdm->skip_restart_for_mdm_crash = of_property_read_bool(node,
				"qcom,esoc-skip-restart-for-mdm-crash");

	ret = of_property_read_u32(node, "qcom,ssctl-instance-id", &esoc->ssctl_id);
	if (ret)
		dev_info(mdm->dev, "esoc ssctl id missing\n");

	esoc->sysmon_name = SDX55M_LABEL;
	esoc->clink_ops = clink_ops;
	esoc->parent = mdm->dev;
	esoc->owner = THIS_MODULE;
	esoc->np = pdev->dev.of_node;
	set_esoc_clink_data(esoc, mdm);

	ret = esoc_clink_register(esoc);
	if (ret) {
		esoc_mdm_log("esoc registration failed\n");
		dev_err(mdm->dev, "esoc registration failed\n");
		goto err_free_irq;
	}
	dev_dbg(mdm->dev, "esoc registration done\n");
	esoc_mdm_log("Done configuring the GPIOs and esoc registration\n");

	init_completion(&mdm->debug_done);
	INIT_WORK(&mdm->mdm_status_work, mdm_status_fn);
	INIT_WORK(&mdm->restart_reason_work, mdm_get_restart_reason);
	INIT_DELAYED_WORK(&mdm->mdm2ap_status_check_work, mdm2ap_status_check);
	mdm->get_restart_reason = false;
	mdm->debug_fail = false;
	mdm->esoc = esoc;
	mdm->init = 0;

	mdm_debug_gpio_ipc_log(mdm);

	return 0;

err_free_irq:
	mdm_free_irq(mdm);
err_release_ipc:
	mdm_release_ipc_gpio(mdm);
err_destroy_wrkq:
	destroy_workqueue(mdm->mdm_queue);
	return ret;
}

static int lemur_setup_regulators(struct mdm_ctrl *mdm)
{
	int len;
	int i, rc;
	char uv_ua[50];
	u32 uv_ua_vals[2];
	const char *reg_name;

	mdm->reg_cnt = of_property_count_strings(mdm->dev->of_node, "reg-names");
	if (mdm->reg_cnt <= 0) {
		dev_err(mdm->dev, "No regulators for this device\n");
		esoc_mdm_log("No regulators for %s device\n", mdm->esoc->name);
		return 0;
	}

	mdm->regs = devm_kzalloc(mdm->dev, sizeof(struct reg_info) * mdm->reg_cnt, GFP_KERNEL);
	if (!mdm->regs)
		return -ENOMEM;

	for (i = 0; i < mdm->reg_cnt; i++) {
		of_property_read_string_index(mdm->dev->of_node, "reg-names", i, &reg_name);

		mdm->regs[i].reg = devm_regulator_get(mdm->dev, reg_name);
		if (IS_ERR(mdm->regs[i].reg)) {
			dev_err(mdm->dev, "failed to get %s reg\n", reg_name);
			return PTR_ERR(mdm->regs[i].reg);
		}

		/* Read current(uA) and voltage(uV) value */
		snprintf(uv_ua, sizeof(uv_ua), "%s-uV-uA", reg_name);
		if (!of_find_property(mdm->dev->of_node, uv_ua, &len))
			continue;

		rc = of_property_read_u32_array(mdm->dev->of_node, uv_ua, uv_ua_vals,
						ARRAY_SIZE(uv_ua_vals));
		if (rc) {
			dev_err(mdm->dev, "Failed to read uVuA value(rc:%d)\n", rc);
			return rc;
		}

		if (uv_ua_vals[0] > 0)
			mdm->regs[i].uV = uv_ua_vals[0];
		if (uv_ua_vals[1] > 0)
			mdm->regs[i].uA = uv_ua_vals[1];
	}
	return 0;
}

static int lemur_setup_hw(struct mdm_ctrl *mdm, const struct mdm_ops *ops,
			  struct platform_device *pdev)
{
	int ret;

	/* Same configuration as that of sdx50, except for the name */
	ret = sdx50m_setup_hw(mdm, ops, pdev);
	if (ret) {
		dev_err(mdm->dev, "Hardware setup failed for lemur\n");
		esoc_mdm_log("Hardware setup failed for lemur\n");
		return ret;
	}

	ret = lemur_setup_regulators(mdm);
	if (ret) {
		dev_err(mdm->dev, "Failed to setup regulators: %d\n", ret);
		esoc_mdm_log("Failed to setup regulators: %d\n", ret);
	}

	mdm->esoc->name = LEMUR_LABEL;
	esoc_mdm_log("Hardware setup done for lemur\n");

	return ret;
}

static struct esoc_clink_ops mdm_cops = {
	.cmd_exe = mdm_cmd_exe,
	.get_status = mdm_get_status,
	.get_err_fatal = mdm_get_err_fatal,
	.notify = mdm_notify,
};

static struct mdm_ops mdm9x55_ops = {
	.clink_ops = &mdm_cops,
	.config_hw = mdm9x55_setup_hw,
	.pon_ops = &mdm9x55_pon_ops,
};

static struct mdm_ops sdx50m_ops = {
	.clink_ops = &mdm_cops,
	.config_hw = sdx50m_setup_hw,
	.pon_ops = &sdx50m_pon_ops,
};

static struct mdm_ops sdx55m_ops = {
	.clink_ops = &mdm_cops,
	.config_hw = sdx55m_setup_hw,
	.pon_ops = &sdx55m_pon_ops,
};

static struct mdm_ops lemur_ops = {
	.clink_ops = &mdm_cops,
	.config_hw = lemur_setup_hw,
	.pon_ops = &sdx50m_pon_ops,
};

static const struct of_device_id mdm_dt_match[] = {
	{ .compatible = "qcom,ext-mdm9x55",
		.data = &mdm9x55_ops, },
	{ .compatible = "qcom,ext-sdx50m",
		.data = &sdx50m_ops, },
	{ .compatible = "qcom,ext-sdx55m",
		.data = &sdx55m_ops, },
	{ .compatible = "qcom,ext-lemur",
		.data = &lemur_ops, },
	{},
};
MODULE_DEVICE_TABLE(of, mdm_dt_match);

static int mdm_probe(struct platform_device *pdev)
{
	const struct of_device_id *match;
	const struct mdm_ops *mdm_ops;
	struct device_node *node = pdev->dev.of_node;
	struct mdm_ctrl *mdm;
	int ret;

	match = of_match_node(mdm_dt_match, node);
	if (IS_ERR_OR_NULL(match))
		return PTR_ERR(match);
	mdm_ops = match->data;
	mdm = devm_kzalloc(&pdev->dev, sizeof(*mdm), GFP_KERNEL);
	if (IS_ERR_OR_NULL(mdm))
		return PTR_ERR(mdm);

	ret = mdm_ops->config_hw(mdm, mdm_ops, pdev);

	platform_set_drvdata(pdev, mdm);

	return ret;
}

static int mdm_remove(struct platform_device *pdev)
{
	struct mdm_ctrl *mdm = platform_get_drvdata(pdev);

	if (mdm->mdm_queue)
		destroy_workqueue(mdm->mdm_queue);

	esoc_clink_unregister(mdm->esoc);

	return 0;
}

static struct platform_driver mdm_driver = {
	.probe		= mdm_probe,
	.remove		= mdm_remove,
	.driver = {
		.name	= "ext-mdm",
		.of_match_table = of_match_ptr(mdm_dt_match),
	},
};

static int __init mdm_register(void)
{
	int ret;

	ipc_log = ipc_log_context_create(ESOC_MDM_IPC_PAGES, "esoc-mdm", 0);
	if (!ipc_log)
		pr_err("Failed to setup esoc-mdm IPC logging\n");

	ret = esoc_bus_init();
	if (ret) {
		pr_err("Failed to initialize esoc bus\n");
		return ret;
	}

	return platform_driver_register(&mdm_driver);
}
module_init(mdm_register);

static void __exit mdm_unregister(void)
{
	platform_driver_unregister(&mdm_driver);
	esoc_bus_exit();
	ipc_log_context_destroy(ipc_log);
}
module_exit(mdm_unregister);
MODULE_LICENSE("GPL v2");
