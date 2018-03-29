/*
 *drivers/mmc/card/cbp_sdio.c
 *
 *VIA CBP SDIO driver for Linux
 *
 *Copyright (C) 2009 VIA TELECOM Corporation, Inc.
 *Author: VIA TELECOM Corporation, Inc.
 *
 *This package is free software; you can redistribute it and/or modify
 *it under the terms of the GNU General Public License version 2 as
 *published by the Free Software Foundation.
 *
 *THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 *IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 *WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <linux/module.h>
#include <linux/mod_devicetable.h>
#include <linux/mmc/core.h>
#include <linux/mmc/card.h>
#include <linux/mmc/sdio_func.h>
#include <linux/mmc/sdio_ids.h>
#include <linux/platform_device.h>
#include <linux/irq.h>
#include <linux/circ_buf.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/kfifo.h>
#include <linux/slab.h>

#include <linux/delay.h>
#include <linux/timer.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/mutex.h>
#include <linux/mmc/host.h>
#include <linux/gpio.h>
#include <linux/wait.h>
#include <linux/suspend.h>

#include "modem_sdio.h"

static int modem_detect_host(const char *host_id)
{
	/*HACK!!!
	 *Rely on mmc->class_dev.class set in mmc_alloc_host
	 *Tricky part: a new mmc hook is being (temporary) created
	 *to discover mmc_host class.
	 *Do you know more elegant way how to enumerate mmc_hosts?
	 */
	struct mmc_host *mmc = NULL;
	struct mmc_host *host = NULL;
	struct class_dev_iter iter;
	struct device *dev;
	int ret = -1;
#if 1
	pr_debug("[C2K] before alloc host\n");
	mmc = mmc_alloc_host(0, NULL);
	if (!mmc) {
		pr_debug("[C2K] mmc_aloc_host error\n");
		ret = -ENOMEM;
		goto out;
	}

	pr_debug("[C2K] mmc_aloc_host success\n");
	BUG_ON(!mmc->class_dev.class);
	class_dev_iter_init(&iter, mmc->class_dev.class, NULL, NULL);
	for (;;) {
		dev = class_dev_iter_next(&iter);
		if (!dev) {
			pr_debug("[C2K] class dev iter next failed\n");
			LOGPRT(LOG_ERR, "%s: %d\n", __func__, __LINE__);
			break;
		}
		host = container_of(dev, struct mmc_host, class_dev);
		if (dev_name(&host->class_dev)
		    && strcmp(dev_name(&host->class_dev), host_id))
			continue;
		ret = 0;
		break;
	}
	mmc_free_host(mmc);
#endif
	/*ret = 0; */
 out:
	return ret;
}

static struct cbp_platform_data cbp_data = {
	.bus = "sdio",
	.host_id = MDM_MMC_ID,
	.ipc_enable = false,
	.rst_ind_enable = false,
	.data_ack_enable = false,
	.flow_ctrl_enable = false,
	.tx_disable_irq = true,

	.gpio_ap_wkup_cp = GPIO_C2K_SDIO_AP_WAKE_MDM,
	.gpio_cp_ready = GPIO_C2K_SDIO_MDM_RDY,
	.gpio_cp_wkup_ap = GPIO_C2K_SDIO_MDM_WAKE_AP,
	.gpio_ap_ready = GPIO_C2K_SDIO_AP_RDY,
	.gpio_sync_polar = GPIO_C2K_SDIO_SYNC_POLAR,

#ifndef CONFIG_EVDO_DT_VIA_SUPPORT
	.gpio_cp_exception = GPIO_C2K_EXCEPTION,
	.c2k_wdt_irq_id = 0,
#endif

	.gpio_rst_ind = GPIO_C2K_MDM_RST_IND,
	.gpio_rst_ind_polar = GPIO_C2K_MDM_RST_IND_POLAR,

	.gpio_data_ack = GPIO_C2K_SDIO_DATA_ACK,
	.gpio_data_ack_polar = GPIO_C2K_SDIO_DATA_ACK_POLAR,

	.gpio_flow_ctrl = GPIO_C2K_SDIO_FLOW_CTRL,
	.gpio_flow_ctrl_polar = GPIO_C2K_SDIO_FLOW_CTRL_POLAR,

	.gpio_pwr_on = GPIO_C2K_MDM_PWR_EN,
	.gpio_rst = GPIO_C2K_MDM_RST,
	/*for the level transfor chip fssd06 */
	.gpio_sd_select = GPIO_C2K_SD_SEL_N,
	.gpio_mc3_enable = GPIO_C2K_MC3_EN_N,

	.modem = NULL,

	.detect_host = modem_detect_host,
	.cbp_setup = modem_sdio_init,
	.cbp_destroy = modem_sdio_exit,
};

/*----------------------data_ack functions-------------------*/
static struct cbp_wait_event *cbp_data_ack;

static irqreturn_t gpio_irq_data_ack(int irq, void *data)
{
	struct cbp_wait_event *cbp_data_ack = (struct cbp_wait_event *)data;
	int level;
	/*unsigned long long hr_t1,hr_t2; */
	/*hr_t1 = sched_clock(); */

	level = !!c2k_gpio_get_value(cbp_data_ack->wait_gpio);
	/*LOGPRT(LOG_NOTICE,  "%s enter, level = %d!\n", __func__, level); */

	if (level == cbp_data_ack->wait_polar) {
		atomic_set(&cbp_data_ack->state, MODEM_ST_READY);
		wake_up(&cbp_data_ack->wait_q);
	}
#if defined(CONFIG_MTK_LEGACY)
	c2k_gpio_irq_unmask(cbp_data_ack->wait_gpio);
#endif
	/*hr_t2 = sched_clock(); */
	/*pr_debug("[sdio]ack: t1=%llu,t2 =%llu,delta=%llu\n",hr_t1, hr_t2, hr_t2-hr_t1); */
	return IRQ_HANDLED;
}

static void data_ack_wait_event(struct cbp_wait_event *pdata_ack)
{
	struct sdio_modem *modem = c2k_modem;
	struct cbp_wait_event *cbp_data_ack =
	    (struct cbp_wait_event *)pdata_ack;

	wait_event(cbp_data_ack->wait_q,
		   (MODEM_ST_READY == atomic_read(&cbp_data_ack->state))
		   || (modem->status == MD_OFF));
}

/*----------------------flow control functions-------------------*/
unsigned long long hr_t1 = 0;
unsigned long long hr_t2 = 0;

static struct cbp_wait_event *cbp_flow_ctrl;

static irqreturn_t gpio_irq_flow_ctrl(int irq, void *data)
{
	struct cbp_wait_event *cbp_flow_ctrl = (struct cbp_wait_event *)data;
	int level;
	/*hr_t1 = sched_clock(); */
	level = !!c2k_gpio_get_value(cbp_flow_ctrl->wait_gpio);

	/*c2k_gpio_set_irq_type(cbp_flow_ctrl->wait_gpio, IRQF_TRIGGER_FALLING |IRQF_TRIGGER_RISING); */
	/*c2k_gpio_set_irq_type(cbp_flow_ctrl->wait_gpio, IRQF_TRIGGER_FALLING ); */
	/*c2k_gpio_set_irq_type(cbp_flow_ctrl->wait_gpio, IRQ_TYPE_LEVEL_LOW |IRQ_TYPE_LEVEL_HIGH); */
#if defined(CONFIG_MTK_LEGACY)
	c2k_gpio_irq_unmask(cbp_flow_ctrl->wait_gpio);
#endif

	if (level == cbp_flow_ctrl->wait_polar) {
		atomic_set(&cbp_flow_ctrl->state, FLOW_CTRL_ENABLE);
		/*LOGPRT(LOG_DEBUG,  "%s: flow control is enable, please write later!\n", __func__); */
	} else {
		atomic_set(&cbp_flow_ctrl->state, FLOW_CTRL_DISABLE);
		/*LOGPRT(LOG_DEBUG,  "%s: %d flow control is disable, can write now!\n", __func__,flw_count); */
		wake_up(&cbp_flow_ctrl->wait_q);
	}

	/*hr_t2 = sched_clock(); */
	/*pr_debug("[sdio] t1=%llu,t2 =%llu,delta=%llu\n",hr_t1, hr_t2, hr_t2-hr_t1); */
	return IRQ_HANDLED;
}

static void flow_ctrl_wait_event(struct cbp_wait_event *pflow_ctrl)
{
	struct cbp_wait_event *cbp_flow_ctrl =
	    (struct cbp_wait_event *)pflow_ctrl;
	struct sdio_modem *modem = c2k_modem;
	/*wait_event(cbp_flow_ctrl->wait_q, FLOW_CTRL_DISABLE == atomic_read(&cbp_flow_ctrl->state)); */
	wait_event_timeout(cbp_flow_ctrl->wait_q,
			   (FLOW_CTRL_DISABLE ==
			    atomic_read(&cbp_flow_ctrl->state)
			    || (modem->status == MD_OFF)),
			   msecs_to_jiffies(20));
}

/*----------------------IPC functions-------------------*/
static int modem_sdio_tx_notifier(int event, void *data);
static int modem_sdio_rx_notifier(int event, void *data);

static struct asc_config sdio_tx_handle = {
	.name = CBP_TX_HD_NAME,
};

static struct asc_infor sdio_tx_user = {
	.name = CBP_TX_USER_NAME,
	.data = &sdio_tx_handle,
	.notifier = modem_sdio_tx_notifier,
};

static struct asc_config sdio_rx_handle = {
	.name = SDIO_RX_HD_NAME,
};

static struct asc_infor sdio_rx_user = {
	.name = SDIO_RX_USER_NAME,
	.data = &sdio_rx_handle,
	.notifier = modem_sdio_rx_notifier,
};

static int modem_sdio_tx_notifier(int event, void *data)
{
	return 0;
}

static int modem_sdio_rx_notifier(int event, void *data)
{
	struct asc_config *rx_config = (struct asc_config *)data;
	struct sdio_modem *modem = c2k_modem;
	int ret = 0;

	LOGPRT(LOG_NOTICE, "%s event=%d\n", __func__, event);
	switch (event) {
	case ASC_NTF_RX_PREPARE:
#ifdef WAKE_HOST_BY_SYNC	/*wake up sdio host by four wire sync mechanis */
		if (modem->status != MD_OFF)
			SRC_trigger_signal(1);
		else
			LOGPRT(LOG_ERR,
			       "ignor asc event to resume sdio host\n");
#endif
		asc_rx_confirm_ready(rx_config->name, 1);
		break;
	case ASC_NTF_RX_POST:
#ifdef WAKE_HOST_BY_SYNC	/*wake up sdio host by four wire sync mechanis */
		if (modem->status != MD_OFF)
			SRC_trigger_signal(0);
		else
			LOGPRT(LOG_ERR,
			       "ignor asc event to suspend sdio host\n");
#endif
		/*asc_rx_confirm_ready(rx_config->name, 0); */
		break;
	default:
		LOGPRT(LOG_ERR, "%s: ignor unknown evernt!!\n", __func__);
		break;
	}
	return ret;
}

static struct cbp_exception *cbp_excp_ind;
/*----------------------reset indication functions-------------------*/
static struct cbp_reset *cbp_rst_ind;

#if 0
static int modem_detect_card(struct cbp_reset *cbp_rst_ind)
{
	/*HACK!!!
	 *Rely on mmc->class_dev.class set in mmc_alloc_host
	 *Tricky part: a new mmc hook is being (temporary) created
	 *to discover mmc_host class.
	 *Do you know more elegant way how to enumerate mmc_hosts?
	 */
	struct mmc_host *mmc = NULL;
	struct class_dev_iter iter;
	struct device *dev;
	int ret = -1;

	mmc = mmc_alloc_host(0, NULL);
	if (!mmc) {
		ret = -ENOMEM;
		goto out;
	}

	BUG_ON(!mmc->class_dev.class);
	class_dev_iter_init(&iter, mmc->class_dev.class, NULL, NULL);
	for (;;) {
		dev = class_dev_iter_next(&iter);
		if (!dev)
			break;
		struct mmc_host *host = container_of(dev,
						     struct mmc_host,
						     class_dev);
		if (dev_name(&host->class_dev)
		    && strcmp(dev_name(&host->class_dev), MDM_MMC_ID)) {
			pr_debug
			    ("[MODEM SDIO] detect card not match\n");
			continue;
		}
		pr_debug("[MODEM SDIO] detect card matched\n");
		cbp_rst_ind->host = host;
		mmc_detect_change(host, 0);
		ret = 0;
		break;
	}
	mmc_free_host(mmc);
 out:
	return ret;
}
#endif

#ifdef WAKE_HOST_BY_SYNC	/*wake up sdio host by four wire sync mechanis */

void c2k_wake_host(int wake)
{
#ifndef CONFIG_EVDO_DT_VIA_SUPPORT
	static int host_status = 1;	/*we should set host off for the first time, so set status to be 1 */

	if (wake && !host_status) {	/*wake sdio host to enum device for sth host */
		host_status = 1;
		LOGPRT(LOG_NOTICE, "%s %d host on.\n", __func__, __LINE__);
		via_sdio_on(3);
	} else if (host_status) {
		host_status = 0;
		LOGPRT(LOG_NOTICE, "%s %d host off.\n", __func__, __LINE__);
		via_sdio_off(3);
	}
#endif
}

static void modem_detect(struct work_struct *work)
{
	struct cbp_reset *cbp_rst_ind = NULL;

	int level = 0;

	LOGPRT(LOG_NOTICE, "%s %d .\n", __func__, __LINE__);
	cbp_rst_ind = container_of(work, struct cbp_reset, reset_work);
#ifdef CONFIG_EVDO_DT_VIA_SUPPORT
	if (cbp_rst_ind->host == NULL) {	/*for first detection and ipoh */
		LOGPRT(LOG_NOTICE, "%s %d modem_detect_card.\n", __func__,
		       __LINE__);
		ret = modem_detect_card(cbp_rst_ind);
		if (ret)
			LOGPRT(LOG_ERR, "%s: modem detect failed.\n", __func__);
	} else {		/*for device reset */
		level = !!c2k_gpio_get_value(cbp_rst_ind->rst_ind_gpio);
		if (level == cbp_rst_ind->rst_ind_polar) {
			LOGPRT(LOG_NOTICE, "%s %d power on sdio host\n",
			       __func__, __LINE__);
			c2k_wake_host(0);
			c2k_wake_host(1);
		} else {
			LOGPRT(LOG_NOTICE, "%s %d power off sdio host\n",
			       __func__, __LINE__);
			/*c2k_gpio_direction_output(GPIO_C2K_MDM_PWR_EN, 1); */
			modem_reset_handler();
			c2k_wake_host(0);
		}
	}
#else

	level = !!c2k_gpio_get_value(cbp_rst_ind->rst_ind_gpio);
	if (level == cbp_rst_ind->rst_ind_polar) {
		LOGPRT(LOG_NOTICE, "%s %d power on sdio host\n", __func__,
		       __LINE__);
		c2k_wake_host(0);
		c2k_wake_host(1);
	} else {
		LOGPRT(LOG_NOTICE, "%s %d power off sdio host\n", __func__,
		       __LINE__);
		/*c2k_gpio_direction_output(GPIO_C2K_MDM_PWR_EN, 1); */
		modem_reset_handler();
		c2k_wake_host(0);
	}

#endif

}

static void modem_detect_for_excp(struct work_struct *work)
{
	pr_debug("[MODEM SDIO] excp work sched!!!\n");
	modem_reset_handler();
	c2k_wake_host(0);
	msleep(1000);
	c2k_wake_host(1);

}

#else
static void modem_detect(struct work_struct *work)
{
	struct cbp_reset *cbp_rst_ind = NULL;
	int ret;
	int level = 0;

	LOGPRT(LOG_NOTICE, "%s %d.\n", __func__, __LINE__);
	cbp_rst_ind = container_of(work, struct cbp_reset, reset_work);
	ret = modem_detect_card(cbp_rst_ind);
	if (ret)
		LOGPRT(LOG_ERR, "%s: modem detect failed.\n", __func__);
}

#endif

void gpio_irq_cbp_rst_ind(void)
{
	int level = 0;
	unsigned long flags;
	struct cbp_platform_data *cdata = &cbp_data;

	level = !!c2k_gpio_get_value(cbp_rst_ind->rst_ind_gpio);
	if (level != cbp_rst_ind->rst_ind_polar) {	/*1:cbp reset happened */
		LOGPRT(LOG_INFO, "%s: set md off.\n", __func__);
		spin_lock_irqsave(&cdata->modem->status_lock, flags);
		cdata->modem->status = MD_OFF;
		spin_unlock_irqrestore(&cdata->modem->status_lock,
				       flags);
#ifdef CONFIG_EVDO_DT_VIA_SUPPORT
		wake_up(&cbp_flow_ctrl->wait_q);
		wake_up(&cbp_data_ack->wait_q);
#else
		atomic_set(&cdata->modem->tx_fifo_cnt, TX_FIFO_SZ);
		wake_up(&cdata->modem->wait_tx_done_q);
#endif
	}
	queue_work(cbp_rst_ind->reset_wq, &cbp_rst_ind->reset_work);
}

static irqreturn_t gpio_irq_cbp_excp_ind(int irq, void *data)
{
	unsigned long flags;

	struct cbp_platform_data *cdata = &cbp_data;

	LOGPRT(LOG_ERR, "%s: receive c2k exception interrupt...\n", __func__);
	spin_lock_irqsave(&cdata->modem->status_lock, flags);
	if (cdata->modem->status != MD_OFF && cdata->modem->status != MD_EXCEPTION) {
		cdata->modem->status = MD_EXCEPTION;
		modem_notify_event(MDM_EVT_NOTIFY_EXCP);
		queue_work(cbp_excp_ind->excp_wq, &cbp_excp_ind->excp_work);
	} else {
		LOGPRT(LOG_ERR, "%s: md status is %u now, ignore this EE\n", __func__, cdata->modem->status);
	}
	spin_unlock_irqrestore(&cdata->modem->status_lock, flags);
#if defined(CONFIG_MTK_LEGACY)
	c2k_gpio_irq_unmask(cbp_excp_ind->excp_ind_gpio);
#endif
	return IRQ_HANDLED;
}

#ifndef CONFIG_EVDO_DT_VIA_SUPPORT

static irqreturn_t c2k_wdt_isr(int irq, void *data)
{
	struct cbp_platform_data *cdata = &cbp_data;

	LOGPRT(LOG_ERR,
	       "%s: receive c2k wdt interrupt, prepare to reset c2k...!\n",
	       __func__);
	dump_c2k_iram();
	/*wake_lock_timeout(&cmdata->wlock, MDM_RST_LOCK_TIME *HZ); */
	modem_notify_event(MDM_EVT_NOTIFY_WDT);

	atomic_set(&cdata->modem->tx_fifo_cnt, TX_FIFO_SZ);
	wake_up(&cdata->modem->wait_tx_done_q);

	return IRQ_HANDLED;
}
#endif

#if 0
/*----------------------cbp sys interface --------------------------*/
static void sys_power_on_cbp(void)
{
	c2k_gpio_direction_output(GPIO_C2K_MDM_RST, 0);
	c2k_gpio_direction_output(GPIO_C2K_MDM_PWR_EN, 0);

	c2k_gpio_direction_output(GPIO_C2K_MDM_RST, 1);
	c2k_gpio_direction_output(GPIO_C2K_MDM_PWR_EN, 1);
	msleep(400);

	c2k_gpio_direction_output(GPIO_C2K_MDM_RST, 0);	/*MDM_RST */
}

static void sys_power_off_cbp(void)
{
	c2k_gpio_direction_output(GPIO_C2K_MDM_RST, 0);
	c2k_gpio_direction_output(GPIO_C2K_MDM_PWR_EN, 0);
	c2k_gpio_direction_output(GPIO_C2K_MDM_RST, 1);
	msleep(500);
	msleep(600);
	c2k_gpio_direction_output(GPIO_C2K_MDM_RST, 0);
}

static void sys_reset_cbp(void)
{
	c2k_gpio_direction_output(GPIO_C2K_MDM_PWR_EN, 1);
	msleep(20);
	c2k_gpio_direction_output(GPIO_C2K_MDM_RST, 1);
	msleep(100);
	msleep(300);
	c2k_gpio_direction_output(GPIO_C2K_MDM_RST, 0);	/*MDM_RST */
}
#endif

static ssize_t cbp_power_show(struct kobject *kobj, struct kobj_attribute *attr,
			      char *buf)
{
	struct cbp_platform_data *cdata = &cbp_data;
	char *s = buf;

	if (cdata->modem)
		s += sprintf(s, "%d\n", cdata->modem->status);

	return s - buf;
}

static ssize_t cbp_power_store(struct kobject *kobj,
			       struct kobj_attribute *attr, const char *buf,
			       size_t n)
{
	unsigned long val;
	struct cbp_platform_data *cdata = &cbp_data;
	unsigned long flags;

	if (kstrtoul(buf, 10, &val))
		return -EINVAL;

	if (val < 0)
		return -EINVAL;

	if (val) {
		if (cdata->modem) {
			if (cdata->modem->status == MD_OFF) {
				/*sys_power_on_cbp(); */
				/*
				   spin_lock_irqsave(cdata->modem->status_lock, flags);
				   cdata.modem->status = MD_OFF;
				   spin_unlock_irqrestore(cdata->modem->status_lock, flags);
				 */
				LOGPRT(LOG_INFO, "AP power on CBP.\n");
			} else {
				LOGPRT(LOG_ERR,
				       "%s: CBP is already power on.\n",
				       __func__);
			}

		}

	} else {
		if (cdata->modem) {
			if (cdata->modem->status != MD_OFF) {
				/*sys_power_off_cbp(); */
				LOGPRT(LOG_INFO, "AP power off CBP.\n");
				spin_lock_irqsave(&cdata->modem->status_lock,
						  flags);
				cdata->modem->status = MD_OFF;
				spin_unlock_irqrestore(&cdata->
						       modem->status_lock,
						       flags);
			} else {
				LOGPRT(LOG_ERR,
				       "%s: CBP is already power off.\n",
				       __func__);
			}
		}
	}

	return n;
}

static ssize_t cbp_reset_show(struct kobject *kobj, struct kobj_attribute *attr,
			      char *buf)
{
	return 0;
}

static ssize_t cbp_reset_store(struct kobject *kobj,
			       struct kobj_attribute *attr, const char *buf,
			       size_t n)
{
	unsigned long val;

	if (kstrtoul(buf, 10, &val))
		return -EINVAL;

	if (val < 0)
		return -EINVAL;

	if (val) {
		/*sys_reset_cbp(); */
		/*c2k_modem_reset_platform(); */
		c2k_reset_modem();

		LOGPRT(LOG_INFO, "AP reset CBP.\n");
	} else
		LOGPRT(LOG_ERR, "%s: reset cbp use value 1.\n", __func__);

	return n;
}

static int jtag_mode;
static ssize_t cbp_jtag_show(struct kobject *kobj, struct kobj_attribute *attr,
			     char *buf)
{
	char *s = buf;

	s += sprintf(s, "%d\n", jtag_mode);

	return s - buf;
}

static ssize_t cbp_jtag_store(struct kobject *kobj, struct kobj_attribute *attr,
			      const char *buf, size_t n)
{
	unsigned long val;

	if (kstrtoul(buf, 10, &val))
		return -EINVAL;

	if (val < 0)
		return -EINVAL;

	if (val) {
		/*sys_reset_cbp(); */
		/*c2k_modem_reset_platform(); */
		jtag_mode = val;
		enable_c2k_jtag(val);

		LOGPRT(LOG_INFO, "set cbp jtag to mode %d.\n", jtag_mode);
	}

	return n;
}

#define cbp_attr(_name) \
static struct kobj_attribute _name##_attr = {	\
	.attr	= {				\
		.name = __stringify(_name),	\
		.mode = 0660,			\
	},					\
	.show	= cbp_##_name##_show,			\
	.store	= cbp_##_name##_store,		\
}
cbp_attr(power);
cbp_attr(reset);
cbp_attr(jtag);

static struct attribute *cbp_power_attr[] = {
	&power_attr.attr,
	&reset_attr.attr,
	&jtag_attr.attr,
	NULL,
};

static struct kobject *cbp_power_kobj;
static struct attribute_group g_power_attr_group = {
	.attrs = cbp_power_attr,
};

/*----------------------- cbp platform driver ------------------------*/

/*static int __devinit cbp_probe(struct platform_device *pdev)*/
static int cbp_probe(struct platform_device *pdev)
{
	struct cbp_platform_data *plat = pdev->dev.platform_data;
	int ret = -1;

#ifndef CONFIG_OF
	/*must have platform data */
	if (!plat) {
		LOGPRT(LOG_ERR, "%s: no platform data!\n", __func__);
		ret = -EINVAL;
		goto out;
	}
#else
	pdev->dev.platform_data = &cbp_data;
	plat = &cbp_data;
#endif

#if 0
	if (plat->bus && !strcmp(plat->bus, "sdio")) {
		if (plat->detect_host) {
			ret = plat->detect_host(plat->host_id);
			if (ret) {
				LOGPRT(LOG_ERR, "%s: host %s dectect failed!\n",
				       __func__, plat->host_id);
				goto out;
			}
		} else {
			LOGPRT(LOG_ERR,
			       "%s: bus %s have no dectect function!\n",
			       __func__, plat->bus);
			goto out;
		}
	} else {
		LOGPRT(LOG_ERR, "%s: unknown bus!\n", __func__);
		goto out;
	}
#endif

	if (GPIO_C2K_VALID(plat->gpio_data_ack)) {
		cbp_data_ack =
		    kzalloc(sizeof(struct cbp_wait_event), GFP_KERNEL);
		if (!cbp_data_ack) {
			ret = -ENOMEM;
			LOGPRT(LOG_ERR, "%s %d kzalloc cbp_data_ack failed\n",
			       __func__, __LINE__);
			goto err_kzalloc_cbp_data_ack;
		}

		init_waitqueue_head(&cbp_data_ack->wait_q);
		atomic_set(&cbp_data_ack->state, MODEM_ST_UNKNOWN);
		cbp_data_ack->wait_gpio = plat->gpio_data_ack;
		cbp_data_ack->wait_polar = plat->gpio_data_ack_polar;
		LOGPRT(LOG_ERR, "cbp_data_ack->wait_gpio=%d\n",
		       cbp_data_ack->wait_gpio);
		LOGPRT(LOG_ERR, "cbp_data_ack->wait_polar=%d\n",
		       cbp_data_ack->wait_polar);
#if defined(CONFIG_MTK_LEGACY)
		c2k_gpio_irq_mask(plat->gpio_data_ack);
#endif
		c2k_gpio_direction_input_for_irq(plat->gpio_data_ack);
		c2k_gpio_set_irq_type(plat->gpio_data_ack,
				      IRQF_TRIGGER_FALLING);
		ret =
		    c2k_gpio_request_irq(plat->gpio_data_ack, gpio_irq_data_ack,
					 IRQF_SHARED | IRQF_TRIGGER_FALLING,
					 DRIVER_NAME "(data_ack)",
					 cbp_data_ack);
#if defined(CONFIG_MTK_LEGACY)
		c2k_gpio_irq_unmask(plat->gpio_data_ack);
#endif
		if (ret < 0) {
			LOGPRT(LOG_ERR,
			       "%s: %d fail to request irq for data_ack!!\n",
			       __func__, __LINE__);
			goto err_req_irq_data_ack;
		}
		plat->cbp_data_ack = cbp_data_ack;
		plat->data_ack_wait_event = data_ack_wait_event;
		plat->data_ack_enable = true;
	}

	if (GPIO_C2K_VALID(plat->gpio_flow_ctrl)) {
		cbp_flow_ctrl =
		    kzalloc(sizeof(struct cbp_wait_event), GFP_KERNEL);
		if (!cbp_flow_ctrl) {
			ret = -ENOMEM;
			LOGPRT(LOG_ERR, "%s %d kzalloc cbp_flow_ctrl failed\n",
			       __func__, __LINE__);
			goto err_kzalloc_cbp_flow_ctrl;
		}

		init_waitqueue_head(&cbp_flow_ctrl->wait_q);
		atomic_set(&cbp_flow_ctrl->state, FLOW_CTRL_DISABLE);
		cbp_flow_ctrl->wait_gpio = plat->gpio_flow_ctrl;
		cbp_flow_ctrl->wait_polar = plat->gpio_flow_ctrl_polar;
		LOGPRT(LOG_ERR, "cbp_flow_ctrl->wait_gpio=%d\n",
		       cbp_flow_ctrl->wait_gpio);
		LOGPRT(LOG_ERR, "cbp_flow_ctrl->wait_polar=%d\n",
		       cbp_flow_ctrl->wait_polar);

#if defined(CONFIG_MTK_LEGACY)
		c2k_gpio_irq_mask(plat->gpio_flow_ctrl);
#endif
		c2k_gpio_direction_input_for_irq(plat->gpio_flow_ctrl);
		/*c2k_gpio_set_irq_type(plat->gpio_flow_ctrl, IRQ_TYPE_LEVEL_LOW |IRQ_TYPE_LEVEL_HIGH); */
		c2k_gpio_set_irq_type(plat->gpio_flow_ctrl,
				      IRQF_TRIGGER_FALLING);
		ret =
		    c2k_gpio_request_irq(plat->gpio_flow_ctrl,
					 gpio_irq_flow_ctrl,
					 IRQF_SHARED | IRQF_TRIGGER_RISING |
					 IRQF_TRIGGER_FALLING,
					 DRIVER_NAME "(flow_ctrl)",
					 cbp_flow_ctrl);
#if defined(CONFIG_MTK_LEGACY)
		c2k_gpio_irq_unmask(plat->gpio_flow_ctrl);
#endif
		if (ret < 0) {
			LOGPRT(LOG_ERR,
			       "%s: %d fail to request irq for flow_ctrl!!\n",
			       __func__, __LINE__);
			goto err_req_irq_flow_ctrl;
		}

		plat->cbp_flow_ctrl = cbp_flow_ctrl;
		plat->flow_ctrl_wait_event = flow_ctrl_wait_event;
		plat->flow_ctrl_enable = true;
	}

	if (GPIO_C2K_VALID(plat->gpio_rst_ind)) {
		cbp_rst_ind = kzalloc(sizeof(struct cbp_reset), GFP_KERNEL);
		if (!cbp_rst_ind) {
			ret = -ENOMEM;
			LOGPRT(LOG_ERR, "%s %d kzalloc cbp_rst_ind failed\n",
			       __func__, __LINE__);
			goto err_kzalloc_cbp_rst_ind;
		}

		cbp_rst_ind->name = "cbp_rst_ind_wq";
		cbp_rst_ind->reset_wq =
		    create_singlethread_workqueue(cbp_rst_ind->name);
		if (cbp_rst_ind->reset_wq == NULL) {
			ret = -ENOMEM;
			LOGPRT(LOG_ERR, "%s %d error creat rst_ind_workqueue\n",
			       __func__, __LINE__);
			goto err_create_work_queue;
		}
		INIT_WORK(&cbp_rst_ind->reset_work, modem_detect);
		cbp_rst_ind->rst_ind_gpio = plat->gpio_rst_ind;
		cbp_rst_ind->rst_ind_polar = plat->gpio_rst_ind_polar;
		cbp_rst_ind->host = NULL;
#if 0
		/*c2k_gpio_irq_mask(plat->gpio_rst_ind); */
		c2k_gpio_direction_input_for_irq(plat->gpio_rst_ind);
		c2k_gpio_set_irq_type(plat->gpio_rst_ind,
				      IRQF_TRIGGER_FALLING |
				      IRQF_TRIGGER_RISING);
		ret =
		    c2k_gpio_request_irq(plat->gpio_rst_ind,
					 gpio_irq_cbp_rst_ind,
					 IRQF_SHARED | IRQF_TRIGGER_FALLING |
					 IRQF_TRIGGER_RISING,
					 DRIVER_NAME "(rst_ind)", cbp_rst_ind);
		/*c2k_gpio_irq_unmask(plat->gpio_rst_ind); */
		if (ret < 0) {
			LOGPRT(LOG_ERR,
			       "%s: %d fail to request irq for rst_ind!!\n",
			       __func__, __LINE__);
			goto err_req_irq_rst_ind;
		}
#endif
		plat->rst_ind_enable = true;
	}

	cbp_excp_ind = kzalloc(sizeof(struct cbp_exception), GFP_KERNEL);
	if (!cbp_excp_ind) {
		ret = -ENOMEM;
		LOGPRT(LOG_ERR, "%s %d kzalloc cbp_rst_ind failed\n", __func__,
		       __LINE__);
		goto err_kzalloc_cbp_excp_ind;
	}

	cbp_excp_ind->name = "cbp_excp_ind_wq";
	cbp_excp_ind->excp_wq =
	    create_singlethread_workqueue(cbp_excp_ind->name);
	if (cbp_excp_ind->excp_wq == NULL) {
		ret = -ENOMEM;
		LOGPRT(LOG_ERR, "%s %d error creat rst_ind_workqueue\n",
		       __func__, __LINE__);
		goto err_create_excp_work_queue;
	}
	/*Todo: workqueue function need to be implemented */
	/*INIT_WORK(&cbp_excp_ind->excp_work, modem_detect); */
	INIT_WORK(&cbp_excp_ind->excp_work, modem_detect_for_excp);
	cbp_excp_ind->host = NULL;

#ifndef CONFIG_EVDO_DT_VIA_SUPPORT
	if (GPIO_C2K_VALID(plat->gpio_cp_exception)) {
		cbp_excp_ind->excp_ind_gpio = plat->gpio_cp_exception;

#if defined(CONFIG_MTK_LEGACY)
		c2k_gpio_irq_mask(plat->gpio_cp_exception);
#endif
		c2k_gpio_set_irq_type(plat->gpio_cp_exception,
				      IRQF_TRIGGER_FALLING);
		ret =
		    c2k_gpio_request_irq(plat->gpio_cp_exception,
					 gpio_irq_cbp_excp_ind,
					 IRQF_TRIGGER_FALLING,
					 DRIVER_NAME "(c2k EE)", cbp_excp_ind);
#if defined(CONFIG_MTK_LEGACY)
		c2k_gpio_irq_unmask(plat->gpio_cp_exception);
#endif
		if (ret < 0) {
			LOGPRT(LOG_ERR,
			       "%s: %d fail to request irq for flow_ctrl!!\n",
			       __func__, __LINE__);
			goto err_req_irq_excp;
		}
	}
	plat->c2k_wdt_irq_id = get_c2k_wdt_irq_id();
	LOGPRT(LOG_INFO, "get c2k wdt irq id %d\n", plat->c2k_wdt_irq_id);
#if 1
	if (plat->c2k_wdt_irq_id)
		ret =
		    request_irq(plat->c2k_wdt_irq_id, c2k_wdt_isr,
				IRQF_TRIGGER_NONE, "C2K_CCCI", plat);
	else
		LOGPRT(LOG_ERR, "%s: %d fail to get wdt irq id!!\n", __func__,
		       __LINE__);
#endif
#endif

	if ((GPIO_C2K_VALID(plat->gpio_ap_wkup_cp))
	    && (GPIO_C2K_VALID(plat->gpio_cp_ready))
	    && (GPIO_C2K_VALID(plat->gpio_cp_wkup_ap))
	    && (GPIO_C2K_VALID(plat->gpio_ap_ready))) {
		sdio_tx_handle.gpio_wake = plat->gpio_ap_wkup_cp;
		sdio_tx_handle.gpio_ready = plat->gpio_cp_ready;
		sdio_tx_handle.polar = plat->gpio_sync_polar;
		ret = asc_tx_register_handle(&sdio_tx_handle);
		if (ret) {
			LOGPRT(LOG_ERR,
			       "%s %d asc_tx_register_handle failed.\n",
			       __func__, __LINE__);
			goto err_ipc;
		}
		ret = asc_tx_add_user(sdio_tx_handle.name, &sdio_tx_user);
		if (ret) {
			LOGPRT(LOG_ERR, "%s %d asc_tx_add_user failed.\n",
			       __func__, __LINE__);
			goto err_ipc;
		}

		sdio_rx_handle.gpio_wake = plat->gpio_cp_wkup_ap;
		sdio_rx_handle.gpio_ready = plat->gpio_ap_ready;
		sdio_rx_handle.polar = plat->gpio_sync_polar;
		ret = asc_rx_register_handle(&sdio_rx_handle);
		if (ret) {
			LOGPRT(LOG_ERR,
			       "%s %d asc_rx_register_handle failed.\n",
			       __func__, __LINE__);
			goto err_ipc;
		}
		ret = asc_rx_add_user(sdio_rx_handle.name, &sdio_rx_user);
		if (ret) {
			LOGPRT(LOG_ERR, "%s %d asc_rx_add_user failed.\n",
			       __func__, __LINE__);
			goto err_ipc;
		}
		plat->ipc_enable = true;
		plat->tx_handle = &sdio_tx_handle;
	}

	ret = plat->cbp_setup(plat);
	if (ret) {
		LOGPRT(LOG_ERR, "%s: host %s setup failed!\n", __func__,
		       plat->host_id);
		goto err_ipc;
	}
	cbp_power_kobj = c2k_kobject_add("power");
	if (!cbp_power_kobj) {
		LOGPRT(LOG_ERR, "error c2k_kobject_add!\n");
		ret = -ENOMEM;
		goto err_create_kobj;
	}
#if !defined(CONFIG_MTK_CLKMGR)
	clk_scp_sys_md2_main = devm_clk_get(&pdev->dev, "scp-sys-md2-main");
	if (IS_ERR(clk_scp_sys_md2_main))
		LOGPRT(LOG_ERR, "[C2K] get scp-sys-md2-main failed\n");
#endif
	LOGPRT(LOG_INFO,
	       " cbp initialized on host %s successfully, bus is %s !\n",
	       plat->host_id, plat->bus);
	return sysfs_create_group(cbp_power_kobj, &g_power_attr_group);

 err_create_kobj:
	plat->cbp_destroy();
 err_ipc:
#if 0
	if (GPIO_C2K_VALID(plat->gpio_rst_ind))
		free_irq(c2k_gpio_to_irq(plat->gpio_rst_ind), cbp_rst_ind);
 err_req_irq_rst_ind:

#endif

#ifndef CONFIG_EVDO_DT_VIA_SUPPORT
 err_req_irq_excp:
	if (GPIO_C2K_VALID(plat->gpio_cp_exception))
		destroy_workqueue(cbp_excp_ind->excp_wq);
#endif
 err_create_excp_work_queue:
	kfree(cbp_excp_ind);
 err_kzalloc_cbp_excp_ind:

	if (GPIO_C2K_VALID(plat->gpio_rst_ind))
		destroy_workqueue(cbp_rst_ind->reset_wq);
 err_create_work_queue:
	if (GPIO_C2K_VALID(plat->gpio_rst_ind))
		kfree(cbp_rst_ind);

 err_kzalloc_cbp_rst_ind:
	if (GPIO_C2K_VALID(plat->gpio_flow_ctrl))
		free_irq(c2k_gpio_to_irq(plat->gpio_flow_ctrl), cbp_flow_ctrl);

 err_req_irq_flow_ctrl:
	if (GPIO_C2K_VALID(plat->gpio_flow_ctrl))
		kfree(cbp_flow_ctrl);

 err_kzalloc_cbp_flow_ctrl:
	if (GPIO_C2K_VALID(plat->gpio_data_ack))
		free_irq(c2k_gpio_to_irq(plat->gpio_data_ack), cbp_data_ack);

 err_req_irq_data_ack:
	if (GPIO_C2K_VALID(plat->gpio_data_ack))
		kfree(cbp_data_ack);

 err_kzalloc_cbp_data_ack:
	return ret;
}

/*static int __devexit cbp_remove(struct platform_device *pdev)*/
static int cbp_remove(struct platform_device *pdev)
{
	struct cbp_platform_data *plat = pdev->dev.platform_data;
/*
	if ((GPIO_C2K_VALID(plat->gpio_sd_select))
	    && (GPIO_C2K_VALID(plat->gpio_mc3_enable))) {
	}
*/
	if (plat->data_ack_enable && (GPIO_C2K_VALID(plat->gpio_data_ack))) {
		free_irq(c2k_gpio_to_irq(plat->gpio_data_ack), cbp_data_ack);
		kfree(cbp_data_ack);
	}

	if (plat->flow_ctrl_enable && (GPIO_C2K_VALID(plat->gpio_flow_ctrl))) {
		free_irq(c2k_gpio_to_irq(plat->gpio_flow_ctrl), cbp_flow_ctrl);
		kfree(cbp_flow_ctrl);
	}

	if (plat->rst_ind_enable && (GPIO_C2K_VALID(plat->gpio_rst_ind))) {
		free_irq(c2k_gpio_to_irq(plat->gpio_rst_ind), cbp_rst_ind);
		destroy_workqueue(cbp_rst_ind->reset_wq);
		kfree(cbp_rst_ind);
	}

	destroy_workqueue(cbp_excp_ind->excp_wq);
	kfree(cbp_excp_ind);

	plat->cbp_destroy();
	sysfs_remove_group(cbp_power_kobj, &g_power_attr_group);
	kobject_put(cbp_power_kobj);
	LOGPRT(LOG_INFO, " cbp removed on host %s, bus is %s!\n", plat->host_id,
	       plat->bus);
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id c2k_of_ids[] = {
	{.compatible = "mediatek,MDC2K", },
	{}
};
#endif

static struct platform_device cbp_device = {
	.name = "cbp",
	.dev = {
		.platform_data = &cbp_data,
		},
};

static struct platform_driver cbp_driver = {
	.driver = {
		   .name = "cbp",
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = c2k_of_ids,
#endif
		   },
	.probe = cbp_probe,
	.remove = cbp_remove,
};

static int cbp_pm_event(struct notifier_block *notifier, unsigned long pm_event,
			void *unused)
{
	struct cbp_platform_data *cdata = &cbp_data;
	unsigned long flags;

	LOGPRT(LOG_NOTICE, "%s pm_event=%ld\n", __func__, pm_event);
	switch (pm_event) {
	case PM_HIBERNATION_PREPARE:
		/*This event is received when system is preparing to hibernation. */
		/*i.e., IPO-H power off in kernel space, where user/kernel space processes are not freezed yet. */
		return NOTIFY_DONE;
	case PM_RESTORE_PREPARE:
		/*This event is received before system is preparing to restore, */
		/*i.e., IPO-H power on, where kernel is on the way to late_initcall() in normal boot. */
		return NOTIFY_DONE;
	case PM_POST_HIBERNATION:
		/*
		   This event is received, after system is restored,
		   and user/kernel processes are unfreezed and can operate on.
		 */
		if (cdata->modem) {
			LOGPRT(LOG_INFO, "%s: set md off.\n", __func__);
			spin_lock_irqsave(&cdata->modem->status_lock, flags);
			cdata->modem->status = MD_OFF;
			spin_unlock_irqrestore(&cdata->modem->status_lock,
					       flags);
		}
		LOGPRT(LOG_NOTICE, "[%s] ipoh occurred\n", __func__);
		modem_reset_handler();
		c2k_platform_restore_first_init();
		LOGPRT(LOG_NOTICE, "%s %d power off sdio host\n", __func__,
		       __LINE__);
		c2k_wake_host(0);
		LOGPRT(LOG_NOTICE, "%s %d notify user space ipoh\n", __func__,
		       __LINE__);
		modem_ipoh_indication_usr();
		return NOTIFY_DONE;
	}
	return NOTIFY_OK;
}

static struct notifier_block cbp_pm_notifier_block = {
	.notifier_call = cbp_pm_event,
	.priority = 0,
};

static int __init cbp_init(void)
{
	int ret;

#ifndef CONFIG_OF
	ret = platform_device_register(&cbp_device);
	if (ret) {
		LOGPRT(LOG_ERR, "platform_device_register failed\n");
		goto err_platform_device_register;
	}
#endif

	ret = platform_driver_register(&cbp_driver);
	if (ret) {
		LOGPRT(LOG_ERR, "platform_driver_register failed\n");
		goto err_platform_driver_register;
	}

	ret = register_pm_notifier(&cbp_pm_notifier_block);
	if (ret) {
		LOGPRT(LOG_ERR, "%s failed to register PM notifier\n",
		       __func__);
		goto err_platform_driver_register;
	} else {
		LOGPRT(LOG_ERR, "%s success to register PM notifier\n",
		       __func__);
	}

	return ret;
 err_platform_driver_register:
	platform_device_unregister(&cbp_device);
	return ret;
}

late_initcall(cbp_init);
