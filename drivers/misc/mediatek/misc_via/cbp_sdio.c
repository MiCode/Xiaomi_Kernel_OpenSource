/*
 * drivers/mmc/card/cbp_sdio.c
 *
 * VIA CBP SDIO driver for Linux
 *
 * Copyright (C) 2009 VIA TELECOM Corporation, Inc.
 * Author: VIA TELECOM Corporation, Inc.
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
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
#include "modem_sdio.h"

#define DRIVER_NAME "cbp"
unsigned char cbp_power_state = 0;
extern void modem_reset_handler(void);

static int modem_detect_host(const char *host_id)
{
	/* HACK!!!
	 * Rely on mmc->class_dev.class set in mmc_alloc_host
	 * Tricky part: a new mmc hook is being (temporary) created
	 * to discover mmc_host class.
	 * Do you know more elegant way how to enumerate mmc_hosts?
	 */
	struct mmc_host *mmc = NULL;
	struct class_dev_iter iter;
	struct device *dev;
	int ret = -1;

	mmc = mmc_alloc_host(0, NULL);
	if (!mmc){
		ret =  -ENOMEM;
		goto out;
	}

	BUG_ON(!mmc->class_dev.class);
	class_dev_iter_init(&iter, mmc->class_dev.class, NULL, NULL);
	for (;;) {
		dev = class_dev_iter_next(&iter);
		if (!dev) {
			LOGPRT(LOG_ERR,  "%s: %d\n", __func__, __LINE__);
			break;
		} else {
			struct mmc_host *host = container_of(dev,
				struct mmc_host, class_dev);
			if (dev_name(&host->class_dev) &&
				strcmp(dev_name(&host->class_dev),
					host_id))
				continue;
			ret = 0;
			break;
		}
	}
	mmc_free_host(mmc);
out:
	return ret;
}

/*----------------------data_ack functions-------------------*/
static struct cbp_wait_event *cbp_data_ack = NULL;

static irqreturn_t gpio_irq_data_ack(int irq, void *data)
{
	struct cbp_wait_event *cbp_data_ack = (struct cbp_wait_event *)data;
	int level;
	//unsigned long long hr_t1,hr_t2;
	//hr_t1 = sched_clock();

	level = !!oem_gpio_get_value(cbp_data_ack->wait_gpio);
	//LOGPRT(LOG_NOTICE,  "%s enter, level = %d!\n", __func__, level);

	if(level == cbp_data_ack->wait_polar){
		atomic_set(&cbp_data_ack->state, MODEM_ST_READY);
		wake_up(&cbp_data_ack->wait_q);
	}
	oem_gpio_irq_unmask(cbp_data_ack->wait_gpio);
	//hr_t2 = sched_clock();
	//printk("[sdio]ack: t1=%llu,t2 =%llu,delta=%llu \n",hr_t1, hr_t2, hr_t2-hr_t1);
	return IRQ_HANDLED;
}

static void data_ack_wait_event(struct cbp_wait_event *pdata_ack)
{
	struct cbp_wait_event *cbp_data_ack = (struct cbp_wait_event *)pdata_ack;

	wait_event(cbp_data_ack->wait_q, (MODEM_ST_READY == atomic_read(&cbp_data_ack->state))||(cbp_power_state==0));
}

/*----------------------flow control functions-------------------*/
unsigned long long hr_t1=0;
unsigned long long hr_t2=0;

static struct cbp_wait_event *cbp_flow_ctrl = NULL;

static irqreturn_t gpio_irq_flow_ctrl(int irq, void *data)
{
	struct cbp_wait_event *cbp_flow_ctrl = (struct cbp_wait_event *)data;
	int level;
    //hr_t1 = sched_clock();
	level = !!oem_gpio_get_value(cbp_flow_ctrl->wait_gpio);
	
	//oem_gpio_set_irq_type(cbp_flow_ctrl->wait_gpio, IRQF_TRIGGER_FALLING |IRQF_TRIGGER_RISING);
	//oem_gpio_set_irq_type(cbp_flow_ctrl->wait_gpio, IRQF_TRIGGER_FALLING );
	//oem_gpio_set_irq_type(cbp_flow_ctrl->wait_gpio, IRQ_TYPE_LEVEL_LOW |IRQ_TYPE_LEVEL_HIGH);
	oem_gpio_irq_unmask(cbp_flow_ctrl->wait_gpio);

	if(level == cbp_flow_ctrl->wait_polar){
		atomic_set(&cbp_flow_ctrl->state, FLOW_CTRL_ENABLE);
		//LOGPRT(LOG_DEBUG,  "%s: flow control is enable, please write later!\n", __func__);
	}
	else{
		atomic_set(&cbp_flow_ctrl->state, FLOW_CTRL_DISABLE);
		//LOGPRT(LOG_DEBUG,  "%s: %d flow control is disable, can write now!\n", __func__,flw_count);
		wake_up(&cbp_flow_ctrl->wait_q);
	}
	
	//hr_t2 = sched_clock();
	//printk("[sdio] t1=%llu,t2 =%llu,delta=%llu \n",hr_t1, hr_t2, hr_t2-hr_t1);
	return IRQ_HANDLED;
}

static void flow_ctrl_wait_event(struct cbp_wait_event *pflow_ctrl)
{
	struct cbp_wait_event *cbp_flow_ctrl = (struct cbp_wait_event *)pflow_ctrl;

	//wait_event(cbp_flow_ctrl->wait_q, FLOW_CTRL_DISABLE == atomic_read(&cbp_flow_ctrl->state));
	wait_event_timeout(cbp_flow_ctrl->wait_q, (FLOW_CTRL_DISABLE == atomic_read(&cbp_flow_ctrl->state)||(cbp_power_state==0)),msecs_to_jiffies(20));
}

/*----------------------IPC functions-------------------*/
static int modem_sdio_tx_notifier(int event, void *data);
static int modem_sdio_rx_notifier(int event, void *data);

static struct asc_config sdio_tx_handle ={
	.name = CBP_TX_HD_NAME,
};
static struct asc_infor sdio_tx_user ={
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
#ifdef WAKE_HOST_BY_SYNC/*wake up sdio host by four wire sync mechanis*/
//extern void VIA_trigger_signal(int i_on);
extern void SRC_trigger_signal(int i_on);
#endif

static int modem_sdio_rx_notifier(int event, void *data)
{
	struct asc_config *rx_config  = (struct asc_config *)data;
	int ret = 0;
	LOGPRT(LOG_NOTICE,  "%s event=%d\n", __func__,event);
	switch(event){
		case ASC_NTF_RX_PREPARE:
#ifdef WAKE_HOST_BY_SYNC/*wake up sdio host by four wire sync mechanis*/
			if(cbp_power_state)
				SRC_trigger_signal(1);
			else
				LOGPRT(LOG_ERR,  "ignor asc event to resume sdio host\n");
#endif
			asc_rx_confirm_ready(rx_config->name, 1);
			break;
		case ASC_NTF_RX_POST:
#ifdef WAKE_HOST_BY_SYNC/*wake up sdio host by four wire sync mechanis*/
			if(cbp_power_state)
				SRC_trigger_signal(0);
			else
				LOGPRT(LOG_ERR,  "ignor asc event to suspend sdio host\n");
#endif
			//asc_rx_confirm_ready(rx_config->name, 0);
			break;
		default:
			LOGPRT(LOG_ERR,  "%s: ignor unknow evernt!!\n", __func__);
			break;
	}
	return ret;
}

/*----------------------reset indication functions-------------------*/
static struct cbp_reset *cbp_rst_ind = NULL;
static int modem_detect_card(struct cbp_reset *cbp_rst_ind)
{
	/* HACK!!!
	 * Rely on mmc->class_dev.class set in mmc_alloc_host
	 * Tricky part: a new mmc hook is being (temporary) created
	 * to discover mmc_host class.
	 * Do you know more elegant way how to enumerate mmc_hosts?
	 */
	struct mmc_host *mmc = NULL;
	struct class_dev_iter iter;
	struct device *dev;
	int ret = -1;

	mmc = mmc_alloc_host(0, NULL);
	if (!mmc){
		ret =  -ENOMEM;
		goto out;
	}

	BUG_ON(!mmc->class_dev.class);
	class_dev_iter_init(&iter, mmc->class_dev.class, NULL, NULL);
	for (;;) {
		dev = class_dev_iter_next(&iter);
		if (!dev) {
			break;
		} else {
			struct mmc_host *host = container_of(dev,
				struct mmc_host, class_dev);
			if (dev_name(&host->class_dev) &&
				strcmp(dev_name(&host->class_dev),
					MDM_MMC_ID))
				continue;
			cbp_rst_ind->host = host;
			mmc_detect_change(host, 0);
			ret = 0;
			break;
		}
	}
	mmc_free_host(mmc);
out:
	return ret;
}

#ifdef WAKE_HOST_BY_SYNC/*wake up sdio host by four wire sync mechanis*/
// extern void via_sdio_on (int sdio_port_num);
// extern void via_sdio_off (int sdio_port_num);

static void oem_wake_host(int wake)
{
#if 0
	if(wake)/*wake sdio host to enum device for sth host*/
		via_sdio_on(2);
	else
		via_sdio_off(2);
#endif
}

static void modem_detect(struct work_struct *work)
{
	struct cbp_reset *cbp_rst_ind = NULL;
	int ret;
	int level = 0;

	LOGPRT(LOG_NOTICE,  "%s %d.\n",__func__,__LINE__);
	cbp_rst_ind = container_of(work, struct cbp_reset, reset_work);
	if(cbp_rst_ind->host == NULL){/*for first detection*/
		ret = modem_detect_card(cbp_rst_ind);
		if (ret){
			LOGPRT(LOG_ERR,  "%s: modem detect failed.\n", __func__);
		}
	}
	else{/*for device reset*/
		level = !!oem_gpio_get_value(cbp_rst_ind->rst_ind_gpio);
		if(level == cbp_rst_ind->rst_ind_polar){
			LOGPRT(LOG_NOTICE,  "%s %d power on sdio host level=%d\n", __func__, __LINE__,level);
			oem_wake_host(1);
			cbp_power_state = 1;
		}
		else{
			LOGPRT(LOG_NOTICE,  "%s %d power off sdio host level=%d\n", __func__, __LINE__,level);
			//oem_gpio_direction_output(GPIO_VIATEL_MDM_PWR_EN, 1);
			modem_reset_handler();
			oem_wake_host(0);
		}
	}
}

#else
static void modem_detect(struct work_struct *work)
{
	struct cbp_reset *cbp_rst_ind = NULL;
	int ret;
	int level = 0;

	LOGPRT(LOG_NOTICE,  "%s %d.\n",__func__,__LINE__);
	cbp_rst_ind = container_of(work, struct cbp_reset, reset_work);
	ret = modem_detect_card(cbp_rst_ind);
	if (ret){
		LOGPRT(LOG_ERR,  "%s: modem detect failed.\n", __func__);
	}
}

#endif

void gpio_irq_cbp_rst_ind(void)
{
	int level = 0;

	level = !!oem_gpio_get_value(cbp_rst_ind->rst_ind_gpio);
	if(level != cbp_rst_ind->rst_ind_polar){/*1:cbp reset happened*/
		cbp_power_state = 0;
		wake_up(&cbp_flow_ctrl->wait_q);
		wake_up(&cbp_data_ack->wait_q);
	}
	queue_work(cbp_rst_ind->reset_wq, &cbp_rst_ind->reset_work);
}


EXPORT_SYMBOL(gpio_irq_cbp_rst_ind);

/*----------------------cbp sys interface --------------------------*/
static void sys_power_on_cbp(void)
{
	oem_gpio_direction_output(GPIO_VIATEL_MDM_RST, 0);
	oem_gpio_direction_output(GPIO_VIATEL_MDM_PWR_EN, 0); 
	
	oem_gpio_direction_output(GPIO_VIATEL_MDM_RST, 1);
	oem_gpio_direction_output(GPIO_VIATEL_MDM_PWR_EN, 1);
	msleep(400);
	
	oem_gpio_direction_output(GPIO_VIATEL_MDM_RST, 0); //MDM_RST
}

static void sys_power_off_cbp(void)
{
	oem_gpio_direction_output(GPIO_VIATEL_MDM_RST, 0);
	oem_gpio_direction_output(GPIO_VIATEL_MDM_PWR_EN, 0);
	oem_gpio_direction_output(GPIO_VIATEL_MDM_RST, 1);
	msleep(500);
	msleep(600);
	oem_gpio_direction_output(GPIO_VIATEL_MDM_RST, 0);
}

static void sys_reset_cbp(void)
{
	oem_gpio_direction_output(GPIO_VIATEL_MDM_PWR_EN, 1);
	msleep(10);
	oem_gpio_direction_output(GPIO_VIATEL_MDM_RST, 1);
	msleep(100);
	msleep(300);
	oem_gpio_direction_output(GPIO_VIATEL_MDM_RST, 0); //MDM_RST
}

static ssize_t cbp_power_show(struct kobject *kobj, struct kobj_attribute *attr,
			     char *buf)
{
	char *s = buf;
	s += sprintf(s, "%d\n", cbp_power_state);

	return (s - buf);
}

static ssize_t cbp_power_store(struct kobject *kobj, struct kobj_attribute *attr,
			   const char *buf, size_t n)
{
	unsigned long val;

	if (strict_strtoul(buf, 10, &val))
		return -EINVAL;

	if (val < 0)
		return -EINVAL;

	if (val){
		if(cbp_power_state == 0){
			//sys_power_on_cbp();
			LOGPRT(LOG_INFO,  "AP power on CBP.\n");
		}
		else{
			LOGPRT(LOG_ERR,  "%s: CBP is already power on.\n", __func__);
		}
	}
	else{
		if(cbp_power_state == 1){
			//sys_power_off_cbp();
			LOGPRT(LOG_INFO,  "AP power off CBP.\n");
			cbp_power_state = 0;
		}
		else{
			LOGPRT(LOG_ERR,  "%s: CBP is already power off.\n", __func__);
		}
	}

	return n;
}

static ssize_t cbp_reset_show(struct kobject *kobj, struct kobj_attribute *attr,
			     char *buf)
{
	return 0;
}

static ssize_t cbp_reset_store(struct kobject *kobj, struct kobj_attribute *attr,
			   const char *buf, size_t n)
{
	unsigned long val;

	if (strict_strtoul(buf, 10, &val))
		return -EINVAL;

	if (val < 0)
		return -EINVAL;

	if (val){
		//sys_reset_cbp();
		LOGPRT(LOG_INFO,  "AP reset CBP.\n");
	}
	else{
		LOGPRT(LOG_ERR,  "%s: reset cbp use value 1.\n", __func__);
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

static struct attribute * cbp_power_attr[] = {
	&power_attr.attr,
	&reset_attr.attr,
	NULL,
};

static struct kobject *cbp_power_kobj;
static struct attribute_group g_power_attr_group = {
	.attrs = cbp_power_attr,
};

/*----------------------- cbp platform driver ------------------------*/
static struct cbp_platform_data cbp_data = {
	.bus					=	"sdio",
	.host_id				=	MDM_MMC_ID,
	.ipc_enable			=	false,
	.rst_ind_enable		=	false,
	.data_ack_enable		=	false,
	.flow_ctrl_enable		=	false,
	.tx_disable_irq		=	true,
	
	.gpio_ap_wkup_cp		=	GPIO_VIATEL_SDIO_AP_WAKE_MDM,
	.gpio_cp_ready		=	GPIO_VIATEL_SDIO_MDM_RDY,
	.gpio_cp_wkup_ap		=	GPIO_VIATEL_SDIO_MDM_WAKE_AP,
	.gpio_ap_ready		=	GPIO_VIATEL_SDIO_AP_RDY,
	.gpio_sync_polar		=	GPIO_VIATEL_SDIO_SYNC_POLAR,

	.gpio_rst_ind			=	GPIO_VIATEL_MDM_RST_IND,
	.gpio_rst_ind_polar		=	GPIO_VIATEL_MDM_RST_IND_POLAR,
	
	.gpio_data_ack		=	GPIO_VIATEL_SDIO_DATA_ACK,
	.gpio_data_ack_polar 	=	GPIO_VIATEL_SDIO_DATA_ACK_POLAR,

	.gpio_flow_ctrl		=	GPIO_VIATEL_SDIO_FLOW_CTRL,
	.gpio_flow_ctrl_polar	=	GPIO_VIATEL_SDIO_FLOW_CTRL_POLAR,
	
	.gpio_pwr_on			=	GPIO_VIATEL_MDM_PWR_EN,
	.gpio_rst				=	GPIO_VIATEL_MDM_RST,
	//for the level transfor chip fssd06
	.gpio_sd_select		=	GPIO_VIATEL_SD_SEL_N,
	.gpio_mc3_enable		=	GPIO_VIATEL_MC3_EN_N,

	.detect_host			=	modem_detect_host,
	.cbp_setup			=	modem_sdio_init,
	.cbp_destroy			=	modem_sdio_exit,
};

//static int __devinit cbp_probe(struct platform_device *pdev)
static int cbp_probe(struct platform_device *pdev)
{
	struct cbp_platform_data *plat = pdev->dev.platform_data;
	int ret = -1;
	
	/* must have platform data */
	if (!plat) {
		LOGPRT(LOG_ERR,  "%s: no platform data!\n", __func__);
		ret = -EINVAL;
		goto out;
	}
	
	if(plat->bus && !strcmp(plat->bus,"sdio")){
		if(plat->detect_host){
			ret = plat->detect_host(plat->host_id);
			if(ret){
				LOGPRT(LOG_ERR,  "%s: host %s dectect failed!\n", __func__, plat->host_id);
				goto out;
			}
		}
		else{
			LOGPRT(LOG_ERR,  "%s: bus %s have no dectect function!\n", __func__, plat->bus);
			goto out;
		}
	}
	else{
		LOGPRT(LOG_ERR,  "%s: unknow bus!\n", __func__);
		goto out;
	}

	if((GPIO_OEM_VALID(plat->gpio_sd_select)) && (GPIO_OEM_VALID(plat->gpio_mc3_enable))){
		ret = oem_gpio_request(plat->gpio_mc3_enable, DRIVER_NAME"(MC3_EN_N)");
		if(ret < 0){
			LOGPRT(LOG_ERR,  "%s: %d fail to requset MC3_EN_N gpio %d ret =%d!!\n",
				__func__, __LINE__, plat->gpio_mc3_enable,ret);
			goto out;
		}
		ret = oem_gpio_request(plat->gpio_sd_select, DRIVER_NAME"(SD_SEL_N)");
		if(ret < 0){
			LOGPRT(LOG_ERR,  "%s: %d fail to requset SD_SEL_N gpio %d ret =%d!!\n", 
				__func__, __LINE__, plat->gpio_sd_select,ret);
			goto err_req_sd_sel;
		}
		oem_gpio_direction_output(GPIO_VIATEL_MC3_EN_N, 0); //MC3_EN_N
		oem_gpio_direction_output(GPIO_VIATEL_SD_SEL_N, 0); //SD_SEL_N
	}

	if(GPIO_OEM_VALID(plat->gpio_data_ack)){
		cbp_data_ack = kzalloc(sizeof(struct cbp_wait_event), GFP_KERNEL);
		if (!cbp_data_ack){
			ret = -ENOMEM;
			LOGPRT(LOG_ERR,  "%s %d kzalloc cbp_data_ack failed \n",__func__, __LINE__);
			goto err_kzalloc_cbp_data_ack;    
		}

		init_waitqueue_head(&cbp_data_ack->wait_q);
		atomic_set(&cbp_data_ack->state, MODEM_ST_UNKNOW);
		cbp_data_ack->wait_gpio = plat->gpio_data_ack;
		cbp_data_ack->wait_polar = plat->gpio_data_ack_polar;
		LOGPRT(LOG_ERR,  "cbp_data_ack->wait_gpio=%d\n",cbp_data_ack->wait_gpio);
		LOGPRT(LOG_ERR,  "cbp_data_ack->wait_polar=%d\n",cbp_data_ack->wait_polar);
		ret = oem_gpio_request(plat->gpio_data_ack, DRIVER_NAME "(data_ack)");
		if(ret < 0){
			LOGPRT(LOG_ERR,  "%s: %d fail to requset data_ack gpio %d ret =%d!!\n", 
				__func__, __LINE__, plat->gpio_data_ack, ret);
			goto err_req_data_ack;
		}
		oem_gpio_irq_mask(plat->gpio_data_ack);
		oem_gpio_direction_input_for_irq(plat->gpio_data_ack);
		oem_gpio_set_irq_type(plat->gpio_data_ack, IRQF_TRIGGER_FALLING);
		ret = oem_gpio_request_irq(plat->gpio_data_ack, gpio_irq_data_ack, 
				IRQF_SHARED | IRQF_TRIGGER_FALLING, DRIVER_NAME "(data_ack)", cbp_data_ack);
		oem_gpio_irq_unmask(plat->gpio_data_ack);
		if (ret < 0) {
			LOGPRT(LOG_ERR,  "%s: %d fail to request irq for data_ack!!\n", __func__, __LINE__);
			goto err_req_irq_data_ack;
		}
		plat->cbp_data_ack = cbp_data_ack;
		plat->data_ack_wait_event = data_ack_wait_event;
		plat->data_ack_enable =true;
	}

	if(GPIO_OEM_VALID(plat->gpio_flow_ctrl)){
		cbp_flow_ctrl = kzalloc(sizeof(struct cbp_wait_event), GFP_KERNEL);
		if (!cbp_flow_ctrl){
			ret = -ENOMEM;
			LOGPRT(LOG_ERR,  "%s %d kzalloc cbp_flow_ctrl failed \n",__func__, __LINE__);
			goto err_kzalloc_cbp_flow_ctrl;    
		}

		init_waitqueue_head(&cbp_flow_ctrl->wait_q);
		atomic_set(&cbp_flow_ctrl->state, FLOW_CTRL_DISABLE);
		cbp_flow_ctrl->wait_gpio = plat->gpio_flow_ctrl;
		cbp_flow_ctrl->wait_polar = plat->gpio_flow_ctrl_polar;
		LOGPRT(LOG_ERR,  "cbp_flow_ctrl->wait_gpio=%d\n",cbp_flow_ctrl->wait_gpio);
		LOGPRT(LOG_ERR,  "cbp_flow_ctrl->wait_polar=%d\n",cbp_flow_ctrl->wait_polar);
		ret = oem_gpio_request(plat->gpio_flow_ctrl, DRIVER_NAME "(flow_ctrl)");
		if(ret < 0){
			LOGPRT(LOG_ERR,  "%s: %d fail to requset flow_ctrl gpio %d ret =%d!!\n", 
				__func__, __LINE__, plat->gpio_flow_ctrl, ret);
			goto err_req_flow_ctrl;
		}
		oem_gpio_irq_mask(plat->gpio_flow_ctrl);
		oem_gpio_direction_input_for_irq(plat->gpio_flow_ctrl);
		//oem_gpio_set_irq_type(plat->gpio_flow_ctrl, IRQ_TYPE_LEVEL_LOW |IRQ_TYPE_LEVEL_HIGH);
		oem_gpio_set_irq_type(plat->gpio_flow_ctrl, IRQF_TRIGGER_FALLING);
		ret = oem_gpio_request_irq(plat->gpio_flow_ctrl, gpio_irq_flow_ctrl, 
				IRQF_SHARED | IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING, 
				DRIVER_NAME "(flow_ctrl)", cbp_flow_ctrl);
		oem_gpio_irq_unmask(plat->gpio_flow_ctrl);
		if (ret < 0) {
			LOGPRT(LOG_ERR,  "%s: %d fail to request irq for flow_ctrl!!\n", __func__, __LINE__);
			goto err_req_irq_flow_ctrl;
		}
		
		plat->cbp_flow_ctrl= cbp_flow_ctrl;
		plat->flow_ctrl_wait_event = flow_ctrl_wait_event;
		plat->flow_ctrl_enable = true;
	}
	
	
	if(GPIO_OEM_VALID(plat->gpio_rst_ind)){
		cbp_rst_ind = kzalloc(sizeof(struct cbp_reset), GFP_KERNEL);
		if (!cbp_rst_ind){
			ret = -ENOMEM;
			LOGPRT(LOG_ERR,  "%s %d kzalloc cbp_rst_ind failed \n",__func__, __LINE__);
			goto err_kzalloc_cbp_rst_ind;    
		}

		cbp_rst_ind->name = "cbp_rst_ind_wq";
		cbp_rst_ind->reset_wq = create_singlethread_workqueue(cbp_rst_ind->name);
		if (cbp_rst_ind->reset_wq == NULL) {
			ret = -ENOMEM;
			LOGPRT(LOG_ERR,  "%s %d error creat rst_ind_workqueue \n",__func__, __LINE__);
			goto err_create_work_queue;
		}
		INIT_WORK(&cbp_rst_ind->reset_work, modem_detect);
		cbp_rst_ind->rst_ind_gpio = plat->gpio_rst_ind;
		cbp_rst_ind->rst_ind_polar = plat->gpio_rst_ind_polar;
		cbp_rst_ind->host = NULL;
#if 0		
		ret = oem_gpio_request(plat->gpio_rst_ind, DRIVER_NAME "(rst_ind)");
		if(ret < 0){
			LOGPRT(LOG_ERR,  "%s: %d fail to requset rst_ind gpio %d ret =%d!!\n", 
				__func__, __LINE__, plat->gpio_rst_ind, ret);
			goto err_req_rst_ind;
		}

		oem_gpio_irq_mask(plat->gpio_rst_ind);
		oem_gpio_direction_input_for_irq(plat->gpio_rst_ind);
		oem_gpio_set_irq_type(plat->gpio_rst_ind, IRQF_TRIGGER_FALLING |IRQF_TRIGGER_RISING);
		ret = oem_gpio_request_irq(plat->gpio_rst_ind, gpio_irq_cbp_rst_ind, 
			IRQF_SHARED |IRQF_TRIGGER_FALLING |IRQF_TRIGGER_RISING, 
			DRIVER_NAME "(rst_ind)", cbp_rst_ind);
		oem_gpio_irq_unmask(plat->gpio_rst_ind);
		if (ret < 0) {
			LOGPRT(LOG_ERR,  "%s: %d fail to request irq for rst_ind!!\n", __func__, __LINE__);
			goto err_req_irq_rst_ind;
		}
#endif
		plat->rst_ind_enable = true;
	}

	if((GPIO_OEM_VALID(plat->gpio_ap_wkup_cp)) && (GPIO_OEM_VALID(plat->gpio_cp_ready)) &&
		(GPIO_OEM_VALID(plat->gpio_cp_wkup_ap))  && (GPIO_OEM_VALID(plat->gpio_ap_ready))){
		sdio_tx_handle.gpio_wake = plat->gpio_ap_wkup_cp;
		sdio_tx_handle.gpio_ready = plat->gpio_cp_ready;
		sdio_tx_handle.polar = plat->gpio_sync_polar;
		ret = asc_tx_register_handle(&sdio_tx_handle);
		if(ret){
			LOGPRT(LOG_ERR,  "%s %d asc_tx_register_handle failed.\n",__FUNCTION__,__LINE__);
			goto err_ipc;
		}
		ret = asc_tx_add_user(sdio_tx_handle.name, &sdio_tx_user);
		if(ret){
			LOGPRT(LOG_ERR,  "%s %d asc_tx_add_user failed.\n",__FUNCTION__,__LINE__);
			goto err_ipc;
		}

		sdio_rx_handle.gpio_wake = plat->gpio_cp_wkup_ap;
		sdio_rx_handle.gpio_ready = plat->gpio_ap_ready;
		sdio_rx_handle.polar = plat->gpio_sync_polar;
		ret = asc_rx_register_handle(&sdio_rx_handle);
		if(ret){
			LOGPRT(LOG_ERR,  "%s %d asc_rx_register_handle failed.\n",__FUNCTION__,__LINE__);
			goto err_ipc;
		}
		ret = asc_rx_add_user(sdio_rx_handle.name, &sdio_rx_user);
		if(ret){
			LOGPRT(LOG_ERR,  "%s %d asc_rx_add_user failed.\n",__FUNCTION__,__LINE__);
			goto err_ipc;
		}
		plat->ipc_enable = true;
		plat->tx_handle = &sdio_tx_handle;
	}
	
	ret = plat->cbp_setup(plat);
	if(ret){
		LOGPRT(LOG_ERR,  "%s: host %s setup failed!\n", __func__, plat->host_id);
		goto err_ipc;
	}

	cbp_power_kobj = viatel_kobject_add("power");
	if (!cbp_power_kobj){
		LOGPRT(LOG_ERR,  "error viatel_kobject_add!\n");
		ret = -ENOMEM;
		goto err_create_kobj;
	}
	LOGPRT(LOG_INFO,  " cbp initialized on host %s successfully, bus is %s !\n", plat->host_id, plat->bus);
	return sysfs_create_group(cbp_power_kobj, &g_power_attr_group);
	
err_create_kobj:
	plat->cbp_destroy();
err_ipc:
#if 0
	if(GPIO_OEM_VALID(plat->gpio_rst_ind)){
		free_irq(oem_gpio_to_irq(plat->gpio_rst_ind), cbp_rst_ind);
	}
err_req_irq_rst_ind:
	if(GPIO_OEM_VALID(plat->gpio_rst_ind)){
		oem_gpio_free(plat->gpio_rst_ind);
	}
#endif
err_req_rst_ind:
	if(GPIO_OEM_VALID(plat->gpio_rst_ind)){
		destroy_workqueue(cbp_rst_ind->reset_wq);
	}
err_create_work_queue:
	if(GPIO_OEM_VALID(plat->gpio_rst_ind)){
		kfree(cbp_rst_ind);
	}
err_kzalloc_cbp_rst_ind:
	if(GPIO_OEM_VALID(plat->gpio_flow_ctrl)){
		free_irq(oem_gpio_to_irq(plat->gpio_flow_ctrl), cbp_flow_ctrl);
	}
err_req_irq_flow_ctrl:
	if(GPIO_OEM_VALID(plat->gpio_flow_ctrl)){
		oem_gpio_free(plat->gpio_flow_ctrl);
	}
err_req_flow_ctrl:
	if(GPIO_OEM_VALID(plat->gpio_flow_ctrl)){
		kfree(cbp_flow_ctrl);
	}
err_kzalloc_cbp_flow_ctrl:
	if(GPIO_OEM_VALID(plat->gpio_data_ack)){
		free_irq(oem_gpio_to_irq(plat->gpio_data_ack), cbp_data_ack);
	}
err_req_irq_data_ack:
	if(GPIO_OEM_VALID(plat->gpio_data_ack)){
		oem_gpio_free(plat->gpio_data_ack);
	}
err_req_data_ack:
	if(GPIO_OEM_VALID(plat->gpio_data_ack)){
		kfree(cbp_data_ack);
	}
err_kzalloc_cbp_data_ack:
	if((GPIO_OEM_VALID(plat->gpio_sd_select)) && (GPIO_OEM_VALID(plat->gpio_mc3_enable))){
		oem_gpio_free(plat->gpio_sd_select);
	}
err_req_sd_sel:
	if((GPIO_OEM_VALID(plat->gpio_sd_select)) && (GPIO_OEM_VALID(plat->gpio_mc3_enable))){
		oem_gpio_free(plat->gpio_mc3_enable);
	}
out:
	return ret;
}

//static int __devexit cbp_remove(struct platform_device *pdev)
static int cbp_remove(struct platform_device *pdev)
{
	struct cbp_platform_data *plat = pdev->dev.platform_data;
	
	if((GPIO_OEM_VALID(plat->gpio_sd_select)) && (GPIO_OEM_VALID(plat->gpio_mc3_enable))){
		oem_gpio_free(plat->gpio_sd_select);
		oem_gpio_free(plat->gpio_mc3_enable);
	}
	
	if(plat->data_ack_enable && (GPIO_OEM_VALID(plat->gpio_data_ack))){
		free_irq(oem_gpio_to_irq(plat->gpio_data_ack), cbp_data_ack);
		oem_gpio_free(plat->gpio_data_ack);
		kfree(cbp_data_ack);
	}

	if(plat->flow_ctrl_enable && (GPIO_OEM_VALID(plat->gpio_flow_ctrl))){
		free_irq(oem_gpio_to_irq(plat->gpio_flow_ctrl), cbp_flow_ctrl);
		oem_gpio_free(plat->gpio_flow_ctrl);
		kfree(cbp_flow_ctrl);
	}

	if(plat->rst_ind_enable && (GPIO_OEM_VALID(plat->gpio_rst_ind))){
		free_irq(oem_gpio_to_irq(plat->gpio_rst_ind), cbp_rst_ind);
		oem_gpio_free(plat->gpio_rst_ind);
		destroy_workqueue(cbp_rst_ind->reset_wq);
		kfree(cbp_rst_ind);
	}
	plat->cbp_destroy();
	sysfs_remove_group(cbp_power_kobj, &g_power_attr_group);
	kobject_put(cbp_power_kobj);
	LOGPRT(LOG_INFO,  " cbp removed on host %s, bus is %s!\n", plat->host_id, plat->bus);
	return 0;
}

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
	},
	.probe		= cbp_probe,
	//.remove		= __devexit_p(cbp_remove),
	.remove		= cbp_remove,
};

static int __init mop500_cbp_init(void)
{
	int ret;

	ret = platform_device_register(&cbp_device);
	if (ret) {
		LOGPRT(LOG_ERR,  "platform_device_register failed\n");
		goto err_platform_device_register;
	}

	ret = platform_driver_register(&cbp_driver);
	if (ret) {
		LOGPRT(LOG_ERR,  "platform_driver_register failed\n");
		goto err_platform_driver_register;
	}
	return ret;
err_platform_driver_register:
	platform_device_unregister(&cbp_device);
err_platform_device_register:
	return ret;
}

late_initcall(mop500_cbp_init);
#if 0
static void __exit mop500_cbp_exit(void)
{
	platform_driver_unregister(&cbp_driver);
	platform_device_unregister(&cbp_device);
}

module_init(mop500_cbp_init);
module_exit(mop500_cbp_exit);

MODULE_DESCRIPTION("MOP500 CBP SDIO driver");
#endif
