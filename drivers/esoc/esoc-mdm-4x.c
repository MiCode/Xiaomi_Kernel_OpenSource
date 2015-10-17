/* Copyright (c) 2014-2015, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/coresight.h>
#include <linux/coresight-cti.h>
#include <linux/workqueue.h>
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
	{"qcom,mdm2ap-errfatal-gpio",   MDM2AP_ERRFATAL},
	{"qcom,ap2mdm-errfatal-gpio",   AP2MDM_ERRFATAL},
	{"qcom,mdm2ap-status-gpio",     MDM2AP_STATUS},
	{"qcom,ap2mdm-status-gpio",     AP2MDM_STATUS},
	{"qcom,mdm2ap-pblrdy-gpio",     MDM2AP_PBLRDY},
	{"qcom,ap2mdm-wakeup-gpio",     AP2MDM_WAKEUP},
	{"qcom,ap2mdm-chnlrdy-gpio",    AP2MDM_CHNLRDY},
	{"qcom,mdm2ap-wakeup-gpio",     MDM2AP_WAKEUP},
	{"qcom,ap2mdm-vddmin-gpio",     AP2MDM_VDDMIN},
	{"qcom,mdm2ap-vddmin-gpio",     MDM2AP_VDDMIN},
	{"qcom,ap2mdm-pmic-pwr-en-gpio", AP2MDM_PMIC_PWR_EN},
	{"qcom,mdm-link-detect-gpio", MDM_LINK_DETECT},
};

/* Required gpios */
static const int required_gpios[] = {
	MDM2AP_ERRFATAL,
	AP2MDM_ERRFATAL,
	MDM2AP_STATUS,
	AP2MDM_STATUS,
};

static void mdm_debug_gpio_show(struct mdm_ctrl *mdm)
{
	struct device *dev = mdm->dev;

	dev_dbg(dev, "%s: MDM2AP_ERRFATAL gpio = %d\n",
			__func__, MDM_GPIO(mdm, MDM2AP_ERRFATAL));
	dev_dbg(dev, "%s: AP2MDM_ERRFATAL gpio = %d\n",
			__func__, MDM_GPIO(mdm, AP2MDM_ERRFATAL));
	dev_dbg(dev, "%s: MDM2AP_STATUS gpio = %d\n",
			__func__, MDM_GPIO(mdm, MDM2AP_STATUS));
	dev_dbg(dev, "%s: AP2MDM_STATUS gpio = %d\n",
			__func__, MDM_GPIO(mdm, AP2MDM_STATUS));
	dev_dbg(dev, "%s: AP2MDM_SOFT_RESET gpio = %d\n",
			__func__, MDM_GPIO(mdm, AP2MDM_SOFT_RESET));
	dev_dbg(dev, "%s: MDM2AP_WAKEUP gpio = %d\n",
			__func__, MDM_GPIO(mdm, MDM2AP_WAKEUP));
	dev_dbg(dev, "%s: AP2MDM_WAKEUP gpio = %d\n",
			 __func__, MDM_GPIO(mdm, AP2MDM_WAKEUP));
	dev_dbg(dev, "%s: AP2MDM_PMIC_PWR_EN gpio = %d\n",
			 __func__, MDM_GPIO(mdm, AP2MDM_PMIC_PWR_EN));
	dev_dbg(dev, "%s: MDM2AP_PBLRDY gpio = %d\n",
			 __func__, MDM_GPIO(mdm, MDM2AP_PBLRDY));
	dev_dbg(dev, "%s: AP2MDM_VDDMIN gpio = %d\n",
			 __func__, MDM_GPIO(mdm, AP2MDM_VDDMIN));
	dev_dbg(dev, "%s: MDM2AP_VDDMIN gpio = %d\n",
			 __func__, MDM_GPIO(mdm, MDM2AP_VDDMIN));
}

static void mdm_enable_irqs(struct mdm_ctrl *mdm)
{
	if (!mdm)
		return;
	if (mdm->irq_mask & IRQ_ERRFATAL) {
		enable_irq(mdm->errfatal_irq);
		irq_set_irq_wake(mdm->errfatal_irq, 1);
		mdm->irq_mask &= ~IRQ_ERRFATAL;
	}
	if (mdm->irq_mask & IRQ_STATUS) {
		enable_irq(mdm->status_irq);
		irq_set_irq_wake(mdm->status_irq, 1);
		mdm->irq_mask &= ~IRQ_STATUS;
	}
	if (mdm->irq_mask & IRQ_PBLRDY) {
		enable_irq(mdm->pblrdy_irq);
		mdm->irq_mask &= ~IRQ_PBLRDY;
	}
}

static void mdm_disable_irqs(struct mdm_ctrl *mdm)
{
	if (!mdm)
		return;
	if (!(mdm->irq_mask & IRQ_ERRFATAL)) {
		irq_set_irq_wake(mdm->errfatal_irq, 0);
		disable_irq_nosync(mdm->errfatal_irq);
		mdm->irq_mask |= IRQ_ERRFATAL;
	}
	if (!(mdm->irq_mask & IRQ_STATUS)) {
		irq_set_irq_wake(mdm->status_irq, 0);
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

static void mdm_update_gpio_configs(struct mdm_ctrl *mdm,
				enum gpio_update_config gpio_config)
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
	unsigned long end_time;
	bool status_down = false;
	struct mdm_ctrl *mdm = get_esoc_clink_data(esoc);
	struct device *dev = mdm->dev;
	int ret;
	bool graceful_shutdown = false;

	switch (cmd) {
	case ESOC_PWR_ON:
		gpio_set_value(MDM_GPIO(mdm, AP2MDM_ERRFATAL), 0);
		mdm_enable_irqs(mdm);
		mdm->init = 1;
		mdm_do_first_power_on(mdm);
		break;
	case ESOC_PWR_OFF:
		mdm_disable_irqs(mdm);
		mdm->debug = 0;
		mdm->ready = false;
		mdm->trig_cnt = 0;
		graceful_shutdown = true;
		ret = sysmon_send_shutdown(&esoc->subsys);
		if (ret) {
			dev_err(mdm->dev, "sysmon shutdown fail, ret = %d\n",
									ret);
			graceful_shutdown = false;
			goto force_poff;
		}
		dev_dbg(mdm->dev, "Waiting for status gpio go low\n");
		status_down = false;
		end_time = jiffies + msecs_to_jiffies(10000);
		while (time_before(jiffies, end_time)) {
			if (gpio_get_value(MDM_GPIO(mdm, MDM2AP_STATUS))
									== 0) {
				dev_dbg(dev, "Status went low\n");
				status_down = true;
				break;
			}
			msleep(100);
		}
		if (status_down)
			dev_dbg(dev, "shutdown successful\n");
		else
			dev_err(mdm->dev, "graceful poff ipc fail\n");
force_poff:
	case ESOC_FORCE_PWR_OFF:
		if (!graceful_shutdown) {
			mdm_disable_irqs(mdm);
			mdm->debug = 0;
			mdm->ready = false;
			mdm->trig_cnt = 0;

			dev_err(mdm->dev, "Graceful shutdown fail, ret = %d\n",
				esoc->subsys.sysmon_shutdown_ret);
		}

		/*
		 * Force a shutdown of the mdm. This is required in order
		 * to prevent the mdm from immediately powering back on
		 * after the shutdown
		 */
		gpio_set_value(MDM_GPIO(mdm, AP2MDM_STATUS), 0);
		esoc_clink_queue_request(ESOC_REQ_SHUTDOWN, esoc);
		mdm_power_down(mdm);
		mdm_update_gpio_configs(mdm, GPIO_UPDATE_BOOTING_CONFIG);
		break;
	case ESOC_RESET:
		mdm_toggle_soft_reset(mdm, false);
		break;
	case ESOC_PREPARE_DEBUG:
		/*
		 * disable all irqs except request irq (pblrdy)
		 * force a reset of the mdm by signaling
		 * an APQ crash, wait till mdm is ready for ramdumps.
		 */
		mdm->ready = false;
		cancel_delayed_work(&mdm->mdm2ap_status_check_work);
		gpio_set_value(MDM_GPIO(mdm, AP2MDM_ERRFATAL), 1);
		dev_dbg(mdm->dev, "set ap2mdm errfatal to force reset\n");
		msleep(mdm->ramdump_delay_ms);
		break;
	case ESOC_EXE_DEBUG:
		mdm->debug = 1;
		mdm->trig_cnt = 0;
		mdm_toggle_soft_reset(mdm, false);
		/*
		 * wait for ramdumps to be collected
		 * then power down the mdm and switch gpios to booting
		 * config
		 */
		wait_for_completion(&mdm->debug_done);
		if (mdm->debug_fail) {
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
		gpio_set_value(MDM_GPIO(mdm, AP2MDM_ERRFATAL), 0);
		dev_dbg(mdm->dev, "exiting debug state after power on\n");
		mdm->get_restart_reason = true;
	      break;
	default:
	      return -EINVAL;
	};
	return 0;
}

static void mdm2ap_status_check(struct work_struct *work)
{
	struct mdm_ctrl *mdm =
		container_of(work, struct mdm_ctrl,
					 mdm2ap_status_check_work.work);
	struct device *dev = mdm->dev;
	struct esoc_clink *esoc = mdm->esoc;
	if (gpio_get_value(MDM_GPIO(mdm, MDM2AP_STATUS)) == 0) {
		dev_dbg(dev, "MDM2AP_STATUS did not go high\n");
		esoc_clink_evt_notify(ESOC_UNEXPECTED_RESET, esoc);
	}
}

static void mdm_status_fn(struct work_struct *work)
{
	struct mdm_ctrl *mdm =
		container_of(work, struct mdm_ctrl, mdm_status_work);
	struct device *dev = mdm->dev;
	int value = gpio_get_value(MDM_GPIO(mdm, MDM2AP_STATUS));

	dev_dbg(dev, "%s: status:%d\n", __func__, value);
	/* Update gpio configuration to "running" config. */
	mdm_update_gpio_configs(mdm, GPIO_UPDATE_RUNNING_CONFIG);
}

static void mdm_get_restart_reason(struct work_struct *work)
{
	int ret, ntries = 0;
	char sfr_buf[RD_BUF_SIZE];
	struct mdm_ctrl *mdm =
		container_of(work, struct mdm_ctrl, restart_reason_work);
	struct device *dev = mdm->dev;

	do {
		ret = sysmon_get_reason(&mdm->esoc->subsys, sfr_buf,
							sizeof(sfr_buf));
		if (!ret) {
			dev_err(dev, "mdm restart reason is %s\n", sfr_buf);
			break;
		}
		msleep(SFR_RETRY_INTERVAL);
	} while (++ntries < SFR_MAX_RETRIES);
	if (ntries == SFR_MAX_RETRIES)
		dev_dbg(dev, "%s: Error retrieving restart reason: %d\n",
						__func__, ret);
	mdm->get_restart_reason = false;
}

static void mdm_notify(enum esoc_notify notify, struct esoc_clink *esoc)
{
	bool status_down;
	uint64_t timeout;
	uint64_t now;
	struct mdm_ctrl *mdm = get_esoc_clink_data(esoc);
	struct device *dev = mdm->dev;

	switch (notify) {
	case ESOC_IMG_XFER_DONE:
		if (gpio_get_value(MDM_GPIO(mdm, MDM2AP_STATUS)) ==  0)
			schedule_delayed_work(&mdm->mdm2ap_status_check_work,
				msecs_to_jiffies(MDM2AP_STATUS_TIMEOUT_MS));
		break;
	case ESOC_BOOT_DONE:
		esoc_clink_evt_notify(ESOC_RUN_STATE, esoc);
		break;
	case ESOC_IMG_XFER_RETRY:
		mdm->init = 1;
		mdm_toggle_soft_reset(mdm, false);
		break;
	case ESOC_IMG_XFER_FAIL:
		esoc_clink_evt_notify(ESOC_INVALID_STATE, esoc);
		break;
	case ESOC_BOOT_FAIL:
		esoc_clink_evt_notify(ESOC_INVALID_STATE, esoc);
		break;
	case ESOC_UPGRADE_AVAILABLE:
		break;
	case ESOC_DEBUG_DONE:
		mdm->debug_fail = false;
		mdm_update_gpio_configs(mdm, GPIO_UPDATE_BOOTING_CONFIG);
		complete(&mdm->debug_done);
		break;
	case ESOC_DEBUG_FAIL:
		mdm->debug_fail = true;
		complete(&mdm->debug_done);
		break;
	case ESOC_PRIMARY_CRASH:
		mdm_disable_irqs(mdm);
		status_down = false;
		dev_dbg(dev, "signal apq err fatal for graceful restart\n");
		gpio_set_value(MDM_GPIO(mdm, AP2MDM_ERRFATAL), 1);
		timeout = local_clock();
		do_div(timeout, NSEC_PER_MSEC);
		timeout += MDM_MODEM_TIMEOUT;
		do {
			if (gpio_get_value(MDM_GPIO(mdm,
						MDM2AP_STATUS)) == 0) {
				status_down = true;
				break;
			}
			now = local_clock();
			do_div(now, NSEC_PER_MSEC);
		} while (!time_after64(now, timeout));

		if (!status_down) {
			dev_err(mdm->dev, "%s MDM2AP status did not go low\n",
								__func__);
			mdm_toggle_soft_reset(mdm, true);
		}
		break;
	case ESOC_PRIMARY_REBOOT:
		mdm_disable_irqs(mdm);
		mdm->debug = 0;
		mdm->ready = false;
		mdm_cold_reset(mdm);
		break;
	};
	return;
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
	dev_err(dev, "%s: mdm sent errfatal interrupt\n",
					 __func__);
	/* disable irq ?*/
	esoc_clink_evt_notify(ESOC_ERR_FATAL, esoc);
	return IRQ_HANDLED;
mdm_pwroff_irq:
	dev_info(dev, "errfatal irq when in pwroff\n");
no_mdm_irq:
	return IRQ_HANDLED;
}

static irqreturn_t mdm_status_change(int irq, void *dev_id)
{
	int value;
	struct esoc_clink *esoc;
	struct mdm_ctrl *mdm = (struct mdm_ctrl *)dev_id;
	struct device *dev = mdm->dev;

	if (!mdm)
		return IRQ_HANDLED;
	esoc = mdm->esoc;
	value = gpio_get_value(MDM_GPIO(mdm, MDM2AP_STATUS));
	if (value == 0 && mdm->ready) {
		dev_err(dev, "unexpected reset external modem\n");
		esoc_clink_evt_notify(ESOC_UNEXPECTED_RESET, esoc);
	} else if (value == 1) {
		cancel_delayed_work(&mdm->mdm2ap_status_check_work);
		dev_dbg(dev, "status = 1: mdm is now ready\n");
		mdm->ready = true;
		mdm_trigger_dbg(mdm);
		queue_work(mdm->mdm_queue, &mdm->mdm_status_work);
		if (mdm->get_restart_reason)
			queue_work(mdm->mdm_queue, &mdm->restart_reason_work);
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
	dev_dbg(dev, "pbl ready %d:\n",
			gpio_get_value(MDM_GPIO(mdm, MDM2AP_PBLRDY)));
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

static int mdm_get_status(u32 *status, struct esoc_clink *esoc)
{
	struct mdm_ctrl *mdm = get_esoc_clink_data(esoc);

	if (gpio_get_value(MDM_GPIO(mdm, MDM2AP_STATUS)) == 0)
		*status = 0;
	else
		*status = 1;
	return 0;
}

static void mdm_configure_debug(struct mdm_ctrl *mdm)
{
	void __iomem *addr;
	unsigned val;
	int ret;
	struct device_node *node = mdm->dev->of_node;

	addr = of_iomap(node, 0);
	if (IS_ERR(addr)) {
		dev_err(mdm->dev, "failed to get debug base addres\n");
		return;
	}
	mdm->dbg_addr = addr + MDM_DBG_OFFSET;
	val = readl_relaxed(mdm->dbg_addr);
	if (val == MDM_DBG_MODE) {
		mdm->dbg_mode = true;
		mdm->cti = coresight_cti_get(MDM_CTI_NAME);
		if (IS_ERR(mdm->cti)) {
			dev_err(mdm->dev, "unable to get cti handle\n");
			goto cti_get_err;
		}
		ret = coresight_cti_map_trigout(mdm->cti, MDM_CTI_TRIG,
								MDM_CTI_CH);
		if (ret) {
			dev_err(mdm->dev, "unable to map trig to channel\n");
			goto cti_map_err;
		}
		mdm->trig_cnt = 0;
	} else {
		dev_dbg(mdm->dev, "Not in debug mode. debug mode = %u\n", val);
		mdm->dbg_mode = false;
	}
	return;
cti_map_err:
	coresight_cti_put(mdm->cti);
cti_get_err:
	mdm->dbg_mode = false;
	return;
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

	ret = of_property_read_u32(node, "qcom,ramdump-timeout-ms",
						&mdm->dump_timeout_ms);
	if (ret)
		mdm->dump_timeout_ms = DEF_RAMDUMP_TIMEOUT;
	ret = of_property_read_u32(node, "qcom,ramdump-delay-ms",
						&mdm->ramdump_delay_ms);
	if (ret)
		mdm->ramdump_delay_ms = DEF_RAMDUMP_DELAY;
	/* Multilple gpio_request calls are allowed */
	if (gpio_request(MDM_GPIO(mdm, AP2MDM_STATUS), "AP2MDM_STATUS"))
		dev_err(dev, "Failed to configure AP2MDM_STATUS gpio\n");
	/* Multilple gpio_request calls are allowed */
	if (gpio_request(MDM_GPIO(mdm, AP2MDM_ERRFATAL), "AP2MDM_ERRFATAL"))
		dev_err(dev, "%s Failed to configure AP2MDM_ERRFATAL gpio\n",
			   __func__);
	if (gpio_request(MDM_GPIO(mdm, MDM2AP_STATUS), "MDM2AP_STATUS")) {
		dev_err(dev, "%s Failed to configure MDM2AP_STATUS gpio\n",
			   __func__);
		goto fatal_err;
	}
	if (gpio_request(MDM_GPIO(mdm, MDM2AP_ERRFATAL), "MDM2AP_ERRFATAL")) {
		dev_err(dev, "%s Failed to configure MDM2AP_ERRFATAL gpio\n",
			   __func__);
		goto fatal_err;
	}
	if (gpio_is_valid(MDM_GPIO(mdm, MDM2AP_PBLRDY))) {
		if (gpio_request(MDM_GPIO(mdm, MDM2AP_PBLRDY),
						"MDM2AP_PBLRDY")) {
			dev_err(dev, "Cannot configure MDM2AP_PBLRDY gpio\n");
			goto fatal_err;
		}
	}
	if (gpio_is_valid(MDM_GPIO(mdm, AP2MDM_WAKEUP))) {
		if (gpio_request(MDM_GPIO(mdm, AP2MDM_WAKEUP),
					"AP2MDM_WAKEUP")) {
			dev_err(dev, "Cannot configure AP2MDM_WAKEUP gpio\n");
			goto fatal_err;
		}
	}
	if (gpio_is_valid(MDM_GPIO(mdm, AP2MDM_CHNLRDY))) {
		if (gpio_request(MDM_GPIO(mdm, AP2MDM_CHNLRDY),
						"AP2MDM_CHNLRDY")) {
			dev_err(dev, "Cannot configure AP2MDM_CHNLRDY gpio\n");
			goto fatal_err;
		}
	}

	gpio_direction_output(MDM_GPIO(mdm, AP2MDM_STATUS), 0);
	gpio_direction_output(MDM_GPIO(mdm, AP2MDM_ERRFATAL), 0);

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
	ret = request_irq(irq, mdm_errfatal,
			IRQF_TRIGGER_RISING , "mdm errfatal", mdm);

	if (ret < 0) {
		dev_err(dev, "%s: MDM2AP_ERRFATAL IRQ#%d request failed,\n",
					__func__, irq);
		goto errfatal_err;
	}
	mdm->errfatal_irq = irq;

errfatal_err:
	 /* status irq */
	irq = gpio_to_irq(MDM_GPIO(mdm, MDM2AP_STATUS));
	if (irq < 0) {
		dev_err(dev, "%s: bad MDM2AP_STATUS IRQ resource, err = %d\n",
				__func__, irq);
		goto status_err;
	}
	ret = request_threaded_irq(irq, NULL, mdm_status_change,
		IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
		"mdm status", mdm);
	if (ret < 0) {
		dev_err(dev, "%s: MDM2AP_STATUS IRQ#%d request failed, err=%d",
			 __func__, irq, ret);
		goto status_err;
	}
	mdm->status_irq = irq;
status_err:
	if (gpio_is_valid(MDM_GPIO(mdm, MDM2AP_PBLRDY))) {
		irq =  platform_get_irq_byname(pdev, "plbrdy_irq");
		if (irq < 0) {
			dev_err(dev, "%s: MDM2AP_PBLRDY IRQ request failed\n",
				 __func__);
			goto pblrdy_err;
		}

		ret = request_threaded_irq(irq, NULL, mdm_pblrdy_change,
				IRQF_TRIGGER_RISING | IRQF_ONESHOT,
				"mdm pbl ready", mdm);
		if (ret < 0) {
			dev_err(dev, "MDM2AP_PBL IRQ#%d request failed %d\n",
								irq, ret);
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
	mdm->gpio_state_booting =
		pinctrl_lookup_state(mdm->pinctrl,
				"mdm_booting");
	if (IS_ERR_OR_NULL(mdm->gpio_state_booting)) {
		mdm->gpio_state_running = NULL;
		mdm->gpio_state_booting = NULL;
	} else {
		mdm->gpio_state_running =
			pinctrl_lookup_state(mdm->pinctrl,
				"mdm_running");
		if (IS_ERR_OR_NULL(mdm->gpio_state_running)) {
			mdm->gpio_state_booting = NULL;
			mdm->gpio_state_running = NULL;
		}
	}
	mdm->gpio_state_active =
		pinctrl_lookup_state(mdm->pinctrl,
				"mdm_active");
	if (IS_ERR_OR_NULL(mdm->gpio_state_active)) {
		retval = PTR_ERR(mdm->gpio_state_active);
		goto err_state_active;
	}
	mdm->gpio_state_suspend =
		pinctrl_lookup_state(mdm->pinctrl,
				"mdm_suspend");
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
static int mdm9x25_setup_hw(struct mdm_ctrl *mdm,
					const struct mdm_ops *ops,
					struct platform_device *pdev)
{
	int ret;
	struct esoc_clink *esoc;
	const struct esoc_clink_ops *const clink_ops = ops->clink_ops;
	const struct mdm_pon_ops *pon_ops = ops->pon_ops;

	mdm->dev = &pdev->dev;
	mdm->pon_ops = pon_ops;
	esoc = devm_kzalloc(mdm->dev, sizeof(*esoc), GFP_KERNEL);
	if (IS_ERR(esoc)) {
		dev_err(mdm->dev, "cannot allocate esoc device\n");
		return PTR_ERR(esoc);
	}
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
	dev_err(mdm->dev, "parsing gpio done\n");
	ret = mdm_pon_dt_init(mdm);
	if (ret)
		return ret;
	dev_dbg(mdm->dev, "pon dt init done\n");
	ret = mdm_pinctrl_init(mdm);
	if (ret)
		return ret;
	dev_err(mdm->dev, "pinctrl init done\n");
	ret = mdm_pon_setup(mdm);
	if (ret)
		return ret;
	dev_dbg(mdm->dev, "pon setup done\n");
	ret = mdm_configure_ipc(mdm, pdev);
	if (ret)
		return ret;
	mdm_configure_debug(mdm);
	dev_err(mdm->dev, "ipc configure done\n");
	esoc->name = MDM9x25_LABEL;
	esoc->link_name = MDM9x25_HSIC;
	esoc->clink_ops = clink_ops;
	esoc->parent = mdm->dev;
	esoc->owner = THIS_MODULE;
	esoc->np = pdev->dev.of_node;
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
	return 0;
}

static int mdm9x35_setup_hw(struct mdm_ctrl *mdm,
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
	if (IS_ERR(esoc)) {
		dev_err(mdm->dev, "cannot allocate esoc device\n");
		return PTR_ERR(esoc);
	}
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
	mdm_configure_debug(mdm);
	dev_dbg(mdm->dev, "ipc configure done\n");
	esoc->name = MDM9x35_LABEL;
	mdm->dual_interface = of_property_read_bool(node,
						"qcom,mdm-dual-link");
	/* Check if link gpio is available */
	if (gpio_is_valid(MDM_GPIO(mdm, MDM_LINK_DETECT))) {
		if (mdm->dual_interface) {
			if (gpio_get_value(MDM_GPIO(mdm, MDM_LINK_DETECT)))
				esoc->link_name = MDM9x35_DUAL_LINK;
			else
				esoc->link_name = MDM9x35_PCIE;
		} else {
			if (gpio_get_value(MDM_GPIO(mdm, MDM_LINK_DETECT)))
				esoc->link_name = MDM9x35_HSIC;
			else
				esoc->link_name = MDM9x35_PCIE;
		}
	} else if (mdm->dual_interface)
		esoc->link_name = MDM9x35_DUAL_LINK;
	else
		esoc->link_name = MDM9x35_HSIC;
	esoc->clink_ops = clink_ops;
	esoc->parent = mdm->dev;
	esoc->owner = THIS_MODULE;
	esoc->np = pdev->dev.of_node;
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
	return 0;
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
	if (IS_ERR(esoc)) {
		dev_err(mdm->dev, "cannot allocate esoc device\n");
		return PTR_ERR(esoc);
	}
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
	mdm->dual_interface = of_property_read_bool(node,
						"qcom,mdm-dual-link");
	esoc->link_name = MDM9x55_PCIE;
	esoc->clink_ops = clink_ops;
	esoc->parent = mdm->dev;
	esoc->owner = THIS_MODULE;
	esoc->np = pdev->dev.of_node;
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
	return 0;
}

static struct esoc_clink_ops mdm_cops = {
	.cmd_exe = mdm_cmd_exe,
	.get_status = mdm_get_status,
	.notify = mdm_notify,
};

static struct mdm_ops mdm9x25_ops = {
	.clink_ops = &mdm_cops,
	.config_hw = mdm9x25_setup_hw,
	.pon_ops = &mdm9x25_pon_ops,
};

static struct mdm_ops mdm9x35_ops = {
	.clink_ops = &mdm_cops,
	.config_hw = mdm9x35_setup_hw,
	.pon_ops = &mdm9x35_pon_ops,
};

static struct mdm_ops mdm9x55_ops = {
	.clink_ops = &mdm_cops,
	.config_hw = mdm9x55_setup_hw,
	.pon_ops = &mdm9x55_pon_ops,
};

static const struct of_device_id mdm_dt_match[] = {
	{ .compatible = "qcom,ext-mdm9x25",
		.data = &mdm9x25_ops, },
	{ .compatible = "qcom,ext-mdm9x35",
		.data = &mdm9x35_ops, },
	{ .compatible = "qcom,ext-mdm9x55",
		.data = &mdm9x55_ops, },
	{},
};
MODULE_DEVICE_TABLE(of, mdm_dt_match);

static int mdm_probe(struct platform_device *pdev)
{
	const struct of_device_id *match;
	const struct mdm_ops *mdm_ops;
	struct device_node *node = pdev->dev.of_node;
	struct mdm_ctrl *mdm;

	match = of_match_node(mdm_dt_match, node);
	if (IS_ERR(match))
		return PTR_ERR(match);
	mdm_ops = match->data;
	mdm = devm_kzalloc(&pdev->dev, sizeof(*mdm), GFP_KERNEL);
	if (IS_ERR(mdm))
		return PTR_ERR(mdm);
	return mdm_ops->config_hw(mdm, mdm_ops, pdev);
}

static struct platform_driver mdm_driver = {
	.probe		= mdm_probe,
	.driver = {
		.name	= "ext-mdm",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(mdm_dt_match),
	},
};

static int __init mdm_register(void)
{
	return platform_driver_register(&mdm_driver);
}
module_init(mdm_register);

static void __exit mdm_unregister(void)
{
	platform_driver_unregister(&mdm_driver);
}
module_exit(mdm_unregister);
MODULE_LICENSE("GPL v2");
