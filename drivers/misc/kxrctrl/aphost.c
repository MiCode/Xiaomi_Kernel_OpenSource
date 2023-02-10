/*
 * SPI controller driver for the nordic52832 SoCs
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */
 
#include "aphost.h"

static struct js_spi_client *gspi_client;
static cp_buffer_t *u_packet;

static void d_packet_set_instance(cp_buffer_t *in)
{

	if (gspi_client == NULL)
		pr_err("js %s: drv init err\n", __func__);

	spin_lock(&gspi_client->smem_lock);

	if (in == NULL)
		u_packet = NULL;
	else {
		u_packet = in;
		u_packet->c_head = -1;
		u_packet->p_head = -1;
	}

	spin_unlock(&gspi_client->smem_lock);

	if (in == NULL)
		pr_debug("js %s:  release mem\n", __func__);
	else
		pr_debug("js %s:  alloc mem\n", __func__);

}

void js_irq_enable(struct js_spi_client   *spi_client, bool enable)
{
	if (spi_client->irqstate == enable) {
		pr_debug("js irq already =%d\n", enable);
		return;
	}

	pr_debug("js irq en =%d\n", enable);
	if (enable)
		enable_irq(spi_client->js_irq);
	else
		disable_irq(spi_client->js_irq);

	spi_client->irqstate = enable;
}

static void js_set_power(int jspower)
{
	int ret = 0;

	if (gspi_client && gspi_client->v1p8) {
		if (gspi_client->powerstate != jspower) {
			if (jspower == 0) { /* off */
				ret = regulator_disable(gspi_client->v1p8);
				if (ret)
					pr_err("reg_disable v1p8 failed\n");
				gspi_client->powerstate = 0;
				js_irq_enable(gspi_client, false);
			} else if (jspower == 1) { /* normal on */
				regulator_set_load(gspi_client->v1p8, 600000);
				ret = regulator_enable(gspi_client->v1p8);
				if (ret)
					pr_err("reg_enable failed for v1p8\n");
				gspi_client->powerstate = 1;
				js_irq_enable(gspi_client, true);
			}
		}
	}
}

static ssize_t jsmem_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n",
					gspi_client->memfd);
}

static ssize_t jsmem_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int ret;
	cp_buffer_t * inbuf;

	ret = kstrtoint(buf, 10, &gspi_client->memfd);
	if (ret < 0)
		return ret;

	mutex_lock(&gspi_client->js_sm_mutex);

	if (gspi_client->memfd == -1) {
		if (IS_ERR_OR_NULL(gspi_client->vaddr))
			goto __end;

		d_packet_set_instance(NULL);
		dma_buf_vunmap(gspi_client->js_buf, gspi_client->vaddr);
		dma_buf_end_cpu_access(gspi_client->js_buf, DMA_BIDIRECTIONAL);
		dma_buf_put(gspi_client->js_buf);
		gspi_client->vaddr = NULL;
		gspi_client->js_buf = NULL;
	} else {
		gspi_client->js_buf = dma_buf_get(gspi_client->memfd);
		if (IS_ERR_OR_NULL(gspi_client->js_buf)) {
			ret = -ENOMEM;
			pr_err("[%s]dma_buf_get failed for fd: %d\n", __func__,
							gspi_client->memfd);
			goto __end;
		}

		ret = dma_buf_begin_cpu_access(gspi_client->js_buf,
							DMA_BIDIRECTIONAL);
		if (ret) {
			pr_err("[%s]: dma_buf_begin_cpu_access failed\n",
							 __func__);
			dma_buf_put(gspi_client->js_buf);
			gspi_client->js_buf = NULL;
			goto __end;
		}

		gspi_client->vsize = gspi_client->js_buf->size;
		gspi_client->vaddr = dma_buf_vmap(gspi_client->js_buf);

		if (IS_ERR_OR_NULL(gspi_client->vaddr)) {
			dma_buf_end_cpu_access(gspi_client->js_buf,
						DMA_BIDIRECTIONAL);
			dma_buf_put(gspi_client->js_buf);
			gspi_client->js_buf = NULL;
			pr_err("[%s]dma_buf_vmap failed for fd: %d\n", __func__,
							  gspi_client->memfd);
			goto __end;
		}

		inbuf = (cp_buffer_t *)gspi_client->vaddr;
		d_packet_set_instance(inbuf);
	}

__end:
	mutex_unlock(&gspi_client->js_sm_mutex);

	return count;
}
static DEVICE_ATTR_RW(jsmem);

static ssize_t jsrequest_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned int input = 0;
	acknowledge_t nordicAck;
	int size = 0;

	if (gspi_client == NULL) {
		pr_err("invalid gspi_client\n");
		return size;
	}

	mutex_lock(&gspi_client->js_mutex);
	memset(&nordicAck, 0, sizeof(acknowledge_t));
	input = atomic_read(&gspi_client->nordicAcknowledge);
	atomic_set(&gspi_client->nordicAcknowledge, 0);
	nordicAck.acknowledgeHead.requestType =
			((input & 0x7f000000) >> 24);
	nordicAck.acknowledgeHead.ack =
			((input & 0x80000000) >> 31);
	nordicAck.acknowledgeData[0] =
			(input & 0x000000ff);
	nordicAck.acknowledgeData[1] =
			((input & 0x0000ff00) >> 8);
	nordicAck.acknowledgeData[2] =
			((input & 0x00ff0000) >> 16);

	if (nordicAck.acknowledgeHead.ack == 1)	{
		switch (nordicAck.acknowledgeHead.requestType) {
		case getMasterNordicVersionRequest:
				size = scnprintf(buf, PAGE_SIZE,
					"masterNordic fwVersion:%d.%d\n",
					nordicAck.acknowledgeData[1],
					nordicAck.acknowledgeData[0]);
				break;
		case bondJoyStickRequest:
		case disconnectJoyStickRequest:
		case setVibStateRequest:
		case hostEnterDfuStateRequest:
				size = scnprintf(buf, PAGE_SIZE,
					"requestType:%d ack:%d\n",
					nordicAck.acknowledgeHead.requestType,
					nordicAck.acknowledgeHead.ack);
				break;
		case getJoyStickBondStateRequest:
				gspi_client->JoyStickBondState =
					(nordicAck.acknowledgeData[0] & 0x03);
				size = scnprintf(buf, PAGE_SIZE,
				"left/right joyStick bond state:%d:%d\n",
				 (gspi_client->JoyStickBondState & 0x01),
				((gspi_client->JoyStickBondState & 0x02) >> 1));
				break;
		case getLeftJoyStickProductNameRequest:
				size = scnprintf(buf, PAGE_SIZE,
					"leftJoyStick productNameID:%d\n",
					nordicAck.acknowledgeData[0]);
				break;
		case getRightJoyStickProductNameRequest:
				size = scnprintf(buf, PAGE_SIZE,
					"rightJoyStick productNameID:%d\n",
					nordicAck.acknowledgeData[0]);
				break;
		case getLeftJoyStickFwVersionRequest:
				size = scnprintf(buf, PAGE_SIZE,
					"leftJoyStick fwVersion:%d.%d\n",
					 nordicAck.acknowledgeData[1],
					 nordicAck.acknowledgeData[0]);
				break;
		case getRightJoyStickFwVersionRequest:
				size = scnprintf(buf, PAGE_SIZE,
					"rightJoyStick fwVersion:%d.%d\n",
					nordicAck.acknowledgeData[1],
					nordicAck.acknowledgeData[0]);
				break;
		default:
				size = scnprintf(buf, PAGE_SIZE,
					"invalid requestType\n");
				break;
		}
	} else {
		size = scnprintf(buf, PAGE_SIZE, "no need to ack\n");
	}
	pinctrl_select_state(
		gspi_client->pinctrl_info.pinctrl,
		gspi_client->pinctrl_info.active);
	gspi_client->js_ledl_state = 0;
	mutex_unlock(&gspi_client->js_mutex);

	return size;
}

static ssize_t jsrequest_store(struct device *dev,
		 struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned int input = 0;
	request_t request;
	int vibState = 0;
	int err = 0;

	if (gspi_client == NULL) {
		pr_err("invalid gspi_client\n");
		return size;
	}

	pinctrl_select_state(
		gspi_client->pinctrl_info.pinctrl,
		gspi_client->pinctrl_info.suspend);
	gspi_client->js_ledl_state = 1;

	mutex_lock(&gspi_client->js_mutex);
	err = kstrtouint(buf, 16, &input);
	if (err) {
		pr_err("invalid param\n");
	} else {
		memset(&request, 0, sizeof(request_t));
		request.requestHead.requestType =
				((input & 0x7f000000) >> 24);
		if (request.requestHead.requestType == 0xc) {
			request.requestData[0] =
					(input & 0x000000ff);
			request.requestData[1] =
					((input & 0x0000ff00) >> 8);
			request.requestData[2] =
					((input & 0x00ff0000) >> 16);
		} else {
			request.requestData[0] =
					(input & 0x000000ff);
			request.requestData[1] =
					(input & 0x0000ff00);
			request.requestData[2] =
					(input & 0x00ff0000);
		}

		switch (request.requestHead.requestType) {
		case setVibStateRequest:
				vibState =
					((request.requestData[1] << 8) |
					request.requestData[0]);
				if (vibState >= 0 &&
					vibState <= 0xffff) {
					atomic_set(
						&gspi_client->userRequest,
						input);
					atomic_inc(
						&gspi_client->dataflag);
					wake_up_interruptible(
						&gspi_client->wait_queue);
				} else {
					pr_err("invalid vibState\n");
					memset(&gspi_client->userRequest, 0,
					sizeof(gspi_client->userRequest));
				}
				break;
		case getMasterNordicVersionRequest:
		case bondJoyStickRequest:
		case disconnectJoyStickRequest:
		case getJoyStickBondStateRequest:
		case hostEnterDfuStateRequest:
		case getLeftJoyStickProductNameRequest:
		case getRightJoyStickProductNameRequest:
		case getLeftJoyStickFwVersionRequest:
		case getRightJoyStickFwVersionRequest:
		case setControllerSleepMode:
				atomic_set(&gspi_client->userRequest,
					input);
				atomic_inc(&gspi_client->dataflag);
				wake_up_interruptible(
					&gspi_client->wait_queue);
				break;
		default:
				pr_err("invalid requestType\n");
				memset(&gspi_client->userRequest, 0,
					sizeof(gspi_client->userRequest));
		}
	}
	mutex_unlock(&gspi_client->js_mutex);

	return size;
}
static DEVICE_ATTR_RW(jsrequest);

#ifdef MANUL_CONTROL_JOYSTICK_RLED
static void js_rled_enable(u8 mask)
{
	if (gspi_client) {
		mask = (3 - mask);
		switch (mask) {
		case 0:
			if ((gspi_client->js_ledr_state == 0) ||
				(gspi_client->js_ledl_state == 0)) {
				pr_debug("enable\n");
				pinctrl_select_state(
					gspi_client->pinctrl_info.pinctrl,
					gspi_client->pinctrl_info.active);
				gspi_client->js_ledr_state = 1;
				gspi_client->js_ledl_state = 0;
			}
			break;
		case 3:
			if ((gspi_client->js_ledr_state == 1) ||
				(gspi_client->js_ledl_state == 1)) {
				pr_debug("disable\n");
				pinctrl_select_state(
					gspi_client->pinctrl_info.pinctrl,
					gspi_client->pinctrl_info.suspend);
				gspi_client->js_ledr_state = 0;
				gspi_client->js_ledl_state = 0;
			}
			break;
		}
	}
}

static ssize_t jsrled_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n",
		((gspi_client->js_ledr_state << 1) |
		(gspi_client->js_ledl_state)));
}

static ssize_t jsrled_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int  rledflag = 0;
	int err = 0;

	mutex_lock(&gspi_client->js_rled_mutex);
	err = kstrtouint(buf, 16, &rledflag);
	if (err) {
		pr_err("invalid param\n");
		mutex_unlock(&gspi_client->js_rled_mutex);
		return err;
	}

	js_rled_enable(rledflag);
	mutex_unlock(&gspi_client->js_rled_mutex);
	return count;
}

static DEVICE_ATTR_RW(jsrled);
#endif

static int js_spi_txfr(struct spi_device *spi, char *txbuf,
				char *rxbuf, int num_byte, uint64_t *tts)
{
	int ret = 0;
	struct spi_transfer txfr;
	struct spi_message msg;

	memset(&txfr, 0, sizeof(txfr));
	txfr.tx_buf = txbuf;
	txfr.rx_buf = rxbuf;
	txfr.len = num_byte;
	spi_message_init(&msg);
	spi_message_add_tail(&txfr, &msg);

	*tts = ktime_to_ns(ktime_get_boottime());
	ret = spi_sync(spi, &msg);

	if (ret < 0)
		pr_err("%s err=%d\n", __func__, ret);

	return ret;
}

static int js_thread(void *data)
{
	int ret;
	unsigned char *pbuf;
	uint64_t tts;
	uint32_t tth[8];
	uint64_t tto[8];
	int num = 0;
	int pksz = 0;
	int index = 0;
	uint32_t hosttime;
	bool skiprport = false;
	unsigned int input = 0;
	request_t currentRequest;
	static request_t lastRequest;
	acknowledge_t nordicAck;
	uint8_t val = 0;
	struct js_spi_client  *spi_client = (struct js_spi_client *)data;

	struct sched_param param = {
		.sched_priority = 88
	};

	sched_setscheduler(current, SCHED_FIFO, &param);
	pr_debug("%s: start\n", __func__);

	do {
		skiprport = false;
		ret = wait_event_interruptible(spi_client->wait_queue,
		atomic_read(&spi_client->dataflag) || kthread_should_stop());

		if ((ret < 0) || kthread_should_stop()) {
			pr_err("%s: exit\n", __func__);
			break;
		}
		atomic_set(&spi_client->dataflag, 0);

		if (spi_client->powerstate != 1) {
			msleep(100);
			continue;
		}

		input = (unsigned int)atomic_read(&gspi_client->userRequest);

		val = gpio_get_value(spi_client->js_irq_gpio);

		/* Filter out the exception trigger */
		if (val == 0 && input == 0)
			continue;

		memset(&currentRequest, 0, sizeof(request_t));
		currentRequest.requestHead.needAck =
				((input & 0x80000000) >> 31);
		currentRequest.requestHead.requestType =
					((input & 0x7f000000) >> 24);
		currentRequest.requestData[0] = (input & 0x000000ff);
		currentRequest.requestData[1] = ((input & 0x0000ff00) >> 8);
		currentRequest.requestData[2] = ((input & 0x00ff0000) >> 16);

		memset(spi_client->txbuffer, 0, sizeof(spi_client->txbuffer));
		memset(spi_client->rxbuffer, 0, sizeof(spi_client->rxbuffer));
		spi_client->txbuffer[0] = CMD_REQUEST_TAG;
		spi_client->txbuffer[1] =
			((currentRequest.requestHead.needAck << 7)
				| currentRequest.requestHead.requestType);

		switch (currentRequest.requestHead.requestType) {
		case setVibStateRequest:
				spi_client->txbuffer[2] =
					currentRequest.requestData[0];
				spi_client->txbuffer[3] =
					 currentRequest.requestData[1];
				break;
		case bondJoyStickRequest:
		case disconnectJoyStickRequest:
				spi_client->txbuffer[2] =
					 (currentRequest.requestData[0]&0x01);
				break;
		case setControllerSleepMode:
				spi_client->txbuffer[2] =
					currentRequest.requestData[0];
				spi_client->txbuffer[3] =
					currentRequest.requestData[1];
				break;
		default:
				break;
		}
		if (spi_client->powerstate == 1) {
			ret = js_spi_txfr(spi_client->spi_client,
			spi_client->txbuffer, spi_client->rxbuffer,
						XFR_SIZE, &tts);
			if (ret != 0)
				continue;
		} else
			continue;

		/* Filtering dirty Data */
		if (spi_client->rxbuffer[4] == 0xff)
			continue;

		if (lastRequest.requestHead.needAck == 1) {
			memset(&nordicAck, 0, sizeof(acknowledge_t));
			nordicAck.acknowledgeHead.ack =
				((spi_client->rxbuffer[0] & 0x80) >> 7);
			nordicAck.acknowledgeHead.requestType =
					(spi_client->rxbuffer[0] & 0x7f);
			nordicAck.acknowledgeData[0] = spi_client->rxbuffer[1];
			nordicAck.acknowledgeData[1] = spi_client->rxbuffer[2];
			nordicAck.acknowledgeData[2] = spi_client->rxbuffer[3];
			if (lastRequest.requestHead.requestType
				== nordicAck.acknowledgeHead.requestType) {
				unsigned int input = 0;

				input = ((spi_client->rxbuffer[0] << 24)
				| (spi_client->rxbuffer[3] << 16)
				| (spi_client->rxbuffer[2] << 8)
				| spi_client->rxbuffer[1]);
				atomic_set(&spi_client->nordicAcknowledge,
							 input);
			}
			memset(&lastRequest, 0, sizeof(lastRequest));
		}

		/* left or right joyStick are bound */
		if ((gspi_client->JoyStickBondState & 0x3) != 0 && input == 0) {

			pksz = spi_client->rxbuffer[4];
			num = spi_client->rxbuffer[5];

			if (num == 0 || pksz != 30)
				skiprport = true;
			memcpy(&hosttime, &spi_client->rxbuffer[6], 4);
			tts = spi_client->tsHost;

			pbuf = &spi_client->rxbuffer[10];
			if (!skiprport) {
				spin_lock(&gspi_client->smem_lock);
				for (index = 0; index < num; index++) {
					memcpy(&tth[index], pbuf, 4);
					tto[index] =
					tts - (hosttime-tth[index]) * 100000;
					if ((u_packet) && (spi_client->vaddr)) {
						int8_t p_head;
						d_packet_t *pdata;

						p_head =
					(u_packet->p_head + 1) % MAX_PACK_SIZE;
						pdata = &u_packet->data[p_head];
						pdata->ts = tto[index];
						pdata->size = pksz - 4;
						memcpy((void *)pdata->data,
						 (void *)(pbuf+4), pksz-4);
						u_packet->p_head = p_head;
					}
					pbuf += pksz;
				}
				spin_unlock(&gspi_client->smem_lock);
			}
		}

		if (currentRequest.requestHead.requestType != 0)
			atomic_set(&gspi_client->userRequest, 0);

		memcpy(&lastRequest, &currentRequest, sizeof(currentRequest));
	} while (1);

	return 0;
}

static int js_pinctrl_init(struct js_spi_client   *spi_client)
{
	int rc = 0;

	spi_client->pinctrl_info.pinctrl =
		devm_pinctrl_get(&spi_client->spi_client->dev);

	if (IS_ERR_OR_NULL(spi_client->pinctrl_info.pinctrl)) {
		rc = PTR_ERR(spi_client->pinctrl_info.pinctrl);
		pr_err("failed  pinctrl, rc=%d\n", rc);
		goto error;
	}

	spi_client->pinctrl_info.active =
	 pinctrl_lookup_state(spi_client->pinctrl_info.pinctrl,
						 "nordic_default");
	if (IS_ERR_OR_NULL(spi_client->pinctrl_info.active)) {
		rc = PTR_ERR(spi_client->pinctrl_info.active);
		pr_err("failed  pinctrl active state, rc=%d\n", rc);
		goto error;
	}

	spi_client->pinctrl_info.suspend =
	 pinctrl_lookup_state(spi_client->pinctrl_info.pinctrl, "nordic_sleep");

	if (IS_ERR_OR_NULL(spi_client->pinctrl_info.suspend)) {
		rc = PTR_ERR(spi_client->pinctrl_info.suspend);
		pr_err("failed  pinctrl suspend state, rc=%d\n", rc);
		goto error;
	}
	pr_debug("%s ok\n", __func__);

	rc = pinctrl_select_state(spi_client->pinctrl_info.pinctrl,
			spi_client->pinctrl_info.active);
	if (rc) {
		pr_err("pinctrl_select_state failed:%d\n", rc);
	} else {
		spi_client->js_ledl_state = 1;
		spi_client->js_ledr_state = 1;
	}
	pr_debug("%s init successfully\n", __func__);
error:
	return rc;
}

static int js_parse_gpios(struct js_spi_client   *spi_client)
{
	int rc = 0;
	struct device_node *of_node = spi_client->spi_client->dev.of_node;
	struct device *dev = &spi_client->spi_client->dev;

	spi_client->v1p8 = devm_regulator_get_optional(dev, "v1p8");
	if (IS_ERR(spi_client->v1p8)) {
		pr_err("failed to get regulator v1p8\n");
		spi_client->v1p8 = NULL;
		rc = -EINVAL;
		goto error;
	}

	spi_client->js_irq_gpio = of_get_named_gpio(of_node,
				 "nordic,irq-gpio", 0);
	if (!gpio_is_valid(spi_client->js_irq_gpio)) {
		pr_err("failed get   js_irq_gpio gpio, rc=%d\n", rc);
		rc = -EINVAL;
		goto error;
	}

	spi_client->js_ledl_gpio = of_get_named_gpio(of_node,
			"nordic,ledl-gpio", 0);
	if (!gpio_is_valid(spi_client->js_ledl_gpio))
		pr_err("failed get   js_ledl_gpio gpio, rc=%d\n", rc);

	spi_client->js_ledr_gpio = of_get_named_gpio(of_node,
			"nordic,ledr-gpio", 0);
	if (!gpio_is_valid(spi_client->js_ledr_gpio))
		pr_err("failed get   js_ledr_gpio gpio, rc=%d\n", rc);

error:
	return rc;
}

static int js_gpio_request(struct js_spi_client   *spi_client)
{
	int rc = 0;

	if (gpio_is_valid(spi_client->js_irq_gpio)) {
		pr_debug("request for js_irq_gpio  =%d\n",
				 spi_client->js_irq_gpio);
		rc = gpio_request(spi_client->js_irq_gpio, "nordic_irq_gpio");
		if (rc) {
			pr_err("request for js_irq_gpio failed, rc=%d\n", rc);
			goto error;
		}
	}

	pr_debug("%s ok\n", __func__);

error:
	return rc;
}

static irqreturn_t  js_irq_handler(int irq, void *dev_id)
{
	int val = 0;
	struct js_spi_client *spi_client = (struct js_spi_client *)dev_id;

	if (spi_client->powerstate == 1) {
		val = gpio_get_value(spi_client->js_irq_gpio);
		if (val == 1) {
			spi_client->tsHost = ktime_to_ns(ktime_get_boottime());
			atomic_inc(&spi_client->dataflag);
			wake_up_interruptible(&spi_client->wait_queue);
		}
	}
	return IRQ_HANDLED;
}

static int js_io_init(struct js_spi_client   *spi_client)
{
	int ret;
	int rc = 0;

	rc = pinctrl_select_state(spi_client->pinctrl_info.pinctrl,
					spi_client->pinctrl_info.active);
	if (rc)
		pr_err("js failed to set pin state, rc=%d\n", rc);

	gpio_direction_input(spi_client->js_irq_gpio);
	spi_client->powerstate = 0;
	spi_client->js_irq = gpio_to_irq(spi_client->js_irq_gpio);

	if (spi_client->js_irq < 0) {
		spi_client->js_irq = -1;
		ret = -EINVAL;
		pr_err(" js  gpio_to_irq err\n");
		goto failed;
	} else {
		/* IRQF_TRIGGER_FALLING */
		ret = request_irq(spi_client->js_irq,
		js_irq_handler, IRQF_TRIGGER_RISING, "nordic", spi_client);
		disable_irq_nosync(spi_client->js_irq);
		if (ret < 0)
			pr_err("js request_irq err=%d\n", spi_client->js_irq);
	}
	pr_debug("%s ok\n", __func__);
	return 0;

failed:
	return ret;
}

static int js_spi_setup(struct spi_device *spi)
{
	struct js_spi_client   *spi_client;
	int rc = 0;

	if ((spi->dev.of_node) == NULL) {
		pr_err("js failed to check of_node\n");
		return -ENOMEM;
	}

	spi_client = kzalloc(sizeof(*spi_client), GFP_KERNEL);
	if (!spi_client) {
		pr_err("js failed to malloc\n");
		return -ENOMEM;
	}

	spi_client->spi_client = spi;
	rc = js_parse_gpios(spi_client);
	if (rc) {
		pr_err("js failed to parse gpio, rc=%d\n", rc);
		goto spi_free;
	}

	rc = js_pinctrl_init(spi_client);
	if (rc) {
		pr_err("js failed to init pinctrl, rc=%d\n", rc);
		goto spi_free;
	}

	rc = js_gpio_request(spi_client);
	if (rc) {
		pr_err("js failed to request gpios, rc=%d\n", rc);
		goto spi_free;
	}

	atomic_set(&spi_client->dataflag, 0);
	atomic_set(&spi_client->userRequest, 0);
	atomic_set(&spi_client->nordicAcknowledge, 0);
	mutex_init(&(spi_client->js_mutex));
	mutex_init(&(spi_client->js_sm_mutex));
#ifdef MANUL_CONTROL_JOYSTICK_RLED
	mutex_init(&(spi_client->js_rled_mutex));
#endif
	spin_lock_init(&spi_client->smem_lock);
	init_waitqueue_head(&spi_client->wait_queue);
	dev_set_drvdata(&spi->dev, spi_client);

	device_create_file(&spi->dev, &dev_attr_jsmem);
	device_create_file(&spi->dev, &dev_attr_jsrequest);
#ifdef MANUL_CONTROL_JOYSTICK_RLED
	device_create_file(&spi->dev, &dev_attr_jsrled);
#endif
	spi_client->suspend = false;
	spi_client->vaddr = NULL;

	gspi_client = spi_client;
	spi_client->kthread = kthread_run(js_thread, spi_client,
			"nordicthread");
	if (IS_ERR(spi_client->kthread))
		pr_err("js kernel_thread failed\n");

	js_io_init(spi_client);
	js_set_power(1);
	return rc;

spi_free:
	kfree(spi_client);
	return rc;
}

static int js_spi_suspend(struct device *dev)
{
	struct js_spi_client *spi_client = NULL;

	if (!dev)
		return -EINVAL;

	spi_client = dev_get_drvdata(dev);
	if (!spi_client)
		return -EINVAL;

	js_set_power(0);
	spi_client->suspend = true;

	pr_debug("%s exit\n", __func__);
	return 0;
}

static int js_spi_resume(struct device *dev)
{
	struct js_spi_client *spi_client = NULL;
	if (!dev)
		return -EINVAL;

	spi_client = dev_get_drvdata(dev);
	if (!spi_client)
		return -EINVAL;

	js_set_power(1);
	spi_client->suspend = false;
	pr_debug("[%s] exit\n", __func__);
	return 0;
}

static int js_spi_driver_probe(struct spi_device *spi)
{
	int ret = 0;

	pr_debug("%s start\n", __func__);
	spi->bits_per_word = 8;
	spi->mode = SPI_MODE_0;

	spi->max_speed_hz = 8 * 1000 * 1000;
	ret = spi_setup(spi);

	if (ret < 0) {
		pr_err("js spi_setup failed ret=%d\n", ret);
		return ret;
	}
	pr_debug("%s ok\n", __func__);

	return js_spi_setup(spi);
}

static int js_spi_driver_remove(struct spi_device *spi)
{
	struct js_spi_client *spi_client = NULL;

	js_set_power(0);
	d_packet_set_instance(NULL);

	spi_client = dev_get_drvdata(&spi->dev);
	if (!IS_ERR_OR_NULL(spi_client->vaddr)) {
		dma_buf_end_cpu_access(spi_client->js_buf,
				DMA_BIDIRECTIONAL);
		dma_buf_put(spi_client->js_buf);
		spi_client->js_buf = NULL;
	}

	if (gspi_client->v1p8)
		regulator_disable(gspi_client->v1p8);

	if (gpio_is_valid(spi_client->js_irq_gpio))
		gpio_free(spi_client->js_irq_gpio);

	mutex_destroy(&(spi_client->js_mutex));
	mutex_destroy(&(spi_client->js_sm_mutex));
#ifdef MANUL_CONTROL_JOYSTICK_RLED
	mutex_destroy(&(spi_client->js_rled_mutex));
#endif

	device_remove_file(&spi->dev, &dev_attr_jsmem);
	device_remove_file(&spi->dev, &dev_attr_jsrequest);
#ifdef MANUL_CONTROL_JOYSTICK_RLED
	device_remove_file(&spi->dev, &dev_attr_jsrled);
#endif

	kfree(spi_client);
	gspi_client = NULL;
	return 0;
}

static const struct of_device_id js_dt_match[] = {
	{ .compatible = "nordic,spicontroller" },
	{ }
};

static const struct dev_pm_ops js_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(js_spi_suspend, js_spi_resume)
};

static struct spi_driver js_spi_driver = {
	.driver = {
		.name = "nordic,spicontroller",
		.owner = THIS_MODULE,
		.of_match_table = js_dt_match,
		.pm     = &js_pm_ops,
	},
	.probe = js_spi_driver_probe,
	.remove = js_spi_driver_remove,
	//.suspend			= js_spi_suspend,
	//.resume 			= js_spi_resume,
};

static int __init js_driver_init(void)
{
	int rc = 0;

	pr_debug("%s start\n", __func__);

	rc = spi_register_driver(&js_spi_driver);
	if (rc < 0) {
		pr_err("spi_register_driver failed rc = %d\n", rc);
		return rc;
	}

	return rc;
}

static void __exit js_driver_exit(void)
{
	spi_unregister_driver(&js_spi_driver);
}

module_init(js_driver_init);
module_exit(js_driver_exit);
MODULE_DESCRIPTION("kinetics nordic52832 driver");
MODULE_LICENSE("GPL v2");
