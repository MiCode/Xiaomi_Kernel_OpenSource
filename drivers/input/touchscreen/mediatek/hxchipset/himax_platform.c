/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#include "himax_ic.h"

int i2c_error_count;
int irq_enable_count;

DEFINE_MUTEX(hx_wr_access);

MODULE_DEVICE_TABLE(of, himax_match_table);
const struct of_device_id himax_match_table[] = {
	{.compatible = "mediatek,cap_touch"}, {},
};

static int himax_tpd_int_gpio = 5;
unsigned int himax_touch_irq;
unsigned int himax_tpd_rst_gpio_number = -1;
unsigned int himax_tpd_int_gpio_number = -1;

u8 *gpDMABuf_va;
u8 *gpDMABuf_pa;

/* Custom set some config */
static int hx_panel_coords[4] = {0, 1080, 0,
				 1920}; /* [1]=X resolution, [3]=Y resolution */
static int hx_display_coords[4] = {0, 1080, 0, 1920};
static int report_type = PROTOCOL_TYPE_B;

struct i2c_client *i2c_client_point;

#if defined(HX_PLATFOME_DEFINE_KEY)
/*In MT6797 need to set 1 into use-tpd-button in dts */
/* kernel-3.18\arch\arm64\boot\dts\amt6797_evb_m.dts*/
/*key_range : [keyindex][key_data] {..{x,y}..}*/
static int key_range[3][2] = {{180, 2400}, {360, 2400}, {540, 2400} };
#endif

int himax_dev_set(struct himax_ts_data *ts)
{
	ts->input_dev = tpd->dev;

	return NO_ERR;
}
int himax_input_register_device(struct input_dev *input_dev)
{
	return NO_ERR;
}

#if defined(HX_PLATFOME_DEFINE_KEY)
void himax_platform_key(void)
{
	int idx = 0;

	if (tpd_dts_data.use_tpd_button) {
		for (idx = 0; idx < tpd_dts_data.tpd_key_num; idx++) {
			input_set_capability(tpd->dev, EV_KEY,
					     tpd_dts_data.tpd_key_local[idx]);
			I("[%d]key:%d\n", idx, tpd_dts_data.tpd_key_local[idx]);
		}
	}
}
/* report coordinates to system and system will transfer it into Key */
static void himax_vk_parser(struct himax_i2c_platform_data *pdata, int key_num)
{
	int i = 0;
	struct himax_virtual_key *vk;
	uint8_t key_index = 0;

	vk = kzalloc(key_num * (sizeof(*vk)), GFP_KERNEL);
	for (key_index = 0; key_index < key_num; key_index++) {
		/* index: def in our driver */
		vk[key_index].index = key_index + 1;
		/* key size */
		vk[key_index].x_range_min = key_range[key_index][0],
		vk[key_index].x_range_max = key_range[key_index][0];
		vk[key_index].y_range_min = key_range[key_index][1],
		vk[key_index].y_range_max = key_range[key_index][1];
	}
	pdata->virtual_key = vk;

	for (i = 0; i < key_num; i++) {
		I(" vk[%d] idx:%d x_min:%d, y_max:%d", i,
		  pdata->virtual_key[i].index,
		  pdata->virtual_key[i].x_range_min,
		  pdata->virtual_key[i].y_range_max);
	}
}
#else
void himax_vk_parser(struct device_node *dt,
		     struct himax_i2c_platform_data *pdata)
{
	u32 data = 0;
	uint8_t cnt = 0, i = 0;
	uint32_t coords[4] = {0};
	struct device_node *node, *pp = NULL;
	struct himax_virtual_key *vk;

	node = of_parse_phandle(dt, "virtualkey", 0);
	if (node == NULL) {
		I(" DT-No vk info in DT");
		return;
	}
	while ((pp = of_get_next_child(node, pp)))
		cnt++;
	if (!cnt)
		return;

	vk = kzalloc(cnt * (sizeof(*vk)), GFP_KERNEL);
	pp = NULL;
	while ((pp = of_get_next_child(node, pp))) {
		if (of_property_read_u32(pp, "idx", &data) == 0)
			vk[i].index = data;
		if (of_property_read_u32_array(pp, "range", coords, 4) == 0) {
			vk[i].x_range_min = coords[0],
			vk[i].x_range_max = coords[1];
			vk[i].y_range_min = coords[2],
			vk[i].y_range_max = coords[3];
		} else
			I(" range faile");
		i++;
	}
	pdata->virtual_key = vk;
	for (i = 0; i < cnt; i++)
		I(" vk[%d] idx:%d x_min:%d, y_max:%d", i,
		  pdata->virtual_key[i].index,
		  pdata->virtual_key[i].x_range_min,
		  pdata->virtual_key[i].y_range_max);
}
#endif
int himax_parse_dt(struct himax_ts_data *ts,
		   struct himax_i2c_platform_data *pdata)
{
	struct device_node *dt = ts->client->dev.of_node;
	struct i2c_client *client = ts->client;

	if (dt) {
		const struct of_device_id *match;

		match = of_match_device(of_match_ptr(himax_match_table),
					&client->dev);
		if (!match) {
			TPD_DMESG("[Himax]Error: No device match found\n");
			return -ENODEV;
		}
	}

	himax_tpd_rst_gpio_number = GTP_RST_PORT;
	himax_tpd_int_gpio_number = GTP_INT_PORT;

	pdata->gpio_reset = himax_tpd_rst_gpio_number;
	pdata->gpio_irq = himax_tpd_int_gpio_number;
	I("%s: int : %2.2x\n", __func__, pdata->gpio_irq);
	I("%s: rst : %2.2x\n", __func__, pdata->gpio_reset);

#if defined(HX_PLATFOME_DEFINE_KEY)

	/* now 3 keys */
	himax_vk_parser(pdata, 3);
#else
	himax_vk_parser(dt, pdata);

#endif

	/* Set device tree data */
	/* Set panel coordinates */
	pdata->abs_x_min = hx_panel_coords[0],
	pdata->abs_x_max = hx_panel_coords[1];
	pdata->abs_y_min = hx_panel_coords[2],
	pdata->abs_y_max = hx_panel_coords[3];
	I(" %s:panel-coords = %d, %d, %d, %d\n", __func__, pdata->abs_x_min,
	  pdata->abs_x_max, pdata->abs_y_min, pdata->abs_y_max);

	/* Set display coordinates */
	pdata->screenWidth = hx_display_coords[1];
	pdata->screenHeight = hx_display_coords[3];
	I(" %s:display-coords = (%d, %d)", __func__, pdata->screenWidth,
	  pdata->screenHeight);
	/* report type */
	pdata->protocol_type = report_type;
	return 0;
}

#ifdef MTK_I2C_DMA
int i2c_himax_read(struct i2c_client *client, uint8_t command, uint8_t *data,
		   uint8_t length, uint8_t toRetry)
{
	int ret = 0;
	s32 retry = 0;
	u8 buffer[1];

	struct i2c_msg msg[] = {
		{.addr = (client->addr & I2C_MASK_FLAG),
		 .flags = 0,
		 .buf = buffer,
		 .len = 1,
		 .timing = 400},
		{.addr = (client->addr & I2C_MASK_FLAG),
		 .ext_flag = (client->ext_flag | I2C_ENEXT_FLAG | I2C_DMA_FLAG),
		 .flags = I2C_M_RD,
		 .buf = gpDMABuf_pa,
		 .len = length,
		 .timing = 400},
	};
	mutex_lock(&hx_wr_access);
	buffer[0] = command;

	if (data == NULL) {
		mutex_unlock(&hx_wr_access);
		return -1;
	}
	for (retry = 0; retry < toRetry; ++retry) {
		ret = i2c_transfer(client->adapter, &msg[0], 2);
		if (ret < 0)
			continue;

		memcpy(data, gpDMABuf_va, length);
		mutex_unlock(&hx_wr_access);
		return 0;
	}
	E("Dma I2C Read Error: %d byte(s), err-code: %d", length, ret);
	i2c_error_count = toRetry;
	mutex_unlock(&hx_wr_access);
	return ret;
}

int i2c_himax_write(struct i2c_client *client, uint8_t command, uint8_t *buf,
		    uint8_t len, uint8_t toRetry)
{
	int rc = 0, retry = 0;
	u8 *pWriteData = gpDMABuf_va;

	struct i2c_msg msg[] = {
		{.addr = (client->addr & I2C_MASK_FLAG),
		 .ext_flag = (client->ext_flag | I2C_ENEXT_FLAG | I2C_DMA_FLAG),
		 .flags = 0,
		 .buf = gpDMABuf_pa,
		 .len = len + 1,
		 .timing = 400},
	};

	mutex_lock(&hx_wr_access);
	if (!pWriteData) {
		E("dma_alloc_coherent failed!\n");
		mutex_unlock(&hx_wr_access);
		return -1;
	}

	gpDMABuf_va[0] = command;

	memcpy(gpDMABuf_va + 1, buf, len);

	for (retry = 0; retry < toRetry; ++retry) {
		rc = i2c_transfer(client->adapter, &msg[0], 1);
		if (rc < 0)
			continue;

		mutex_unlock(&hx_wr_access);
		return 0;
	}

	E("Dma I2C master write Error: %d byte(s), err-code: %d", len, rc);
	i2c_error_count = toRetry;
	mutex_unlock(&hx_wr_access);
	return rc;
}

int i2c_himax_write_command(struct i2c_client *client, uint8_t command,
			    uint8_t toRetry)
{
	return i2c_himax_write(client, command, NULL, 0, toRetry);
}

int i2c_himax_master_write(struct i2c_client *client, uint8_t *buf, uint8_t len,
			   uint8_t toRetry)
{
	int rc = 0, retry = 0;
	u8 *pWriteData = gpDMABuf_va;

	struct i2c_msg msg[] = {
		{.addr = (client->addr & I2C_MASK_FLAG),
		 .ext_flag = (client->ext_flag | I2C_ENEXT_FLAG | I2C_DMA_FLAG),
		 .flags = 0,
		 .buf = gpDMABuf_pa,
		 .len = len,
		 .timing = 400},
	};

	mutex_lock(&hx_wr_access);
	if (!pWriteData) {
		E("dma_alloc_coherent failed!\n");
		mutex_unlock(&hx_wr_access);
		return -1;
	}

	memcpy(gpDMABuf_va, buf, len);
	for (retry = 0; retry < toRetry; ++retry) {
		rc = i2c_transfer(client->adapter, &msg[0], 1);
		if (rc < 0)
			continue;

		mutex_unlock(&hx_wr_access);
		return 0;
	}
	E("Dma I2C master write Error: %d byte(s), err-code: %d", len, rc);
	i2c_error_count = toRetry;
	mutex_unlock(&hx_wr_access);
	return rc;
}

#else
int i2c_himax_read(struct i2c_client *client, uint8_t command, uint8_t *data,
		   uint8_t length, uint8_t toRetry)
{
	int retry;
	struct i2c_msg msg[] = {{
					.addr = client->addr,
					.flags = 0,
					.len = 1,
					.buf = &command,
				},
				{
					.addr = client->addr,
					.flags = I2C_M_RD,
					.len = length,
					.buf = data,
				} };

	for (retry = 0; retry < toRetry; retry++) {
		if (i2c_transfer(client->adapter, msg, 2) == 2)
			break;
		msleep(20);
	}
	if (retry == toRetry) {
		E("%s: i2c_read_block retry over %d\n", __func__, toRetry);
		i2c_error_count = toRetry;
		return -EIO;
	}
	return 0;
}

int i2c_himax_write(struct i2c_client *client, uint8_t command, uint8_t *data,
		    uint8_t length, uint8_t toRetry)
{
	int retry /*, loop_i*/;
	uint8_t buf[length + 1];

	struct i2c_msg msg[] = {{
		.addr = client->addr, .flags = 0, .len = length + 1, .buf = buf,
	} };

	buf[0] = command;
	memcpy(buf + 1, data, length);

	for (retry = 0; retry < toRetry; retry++) {
		if (i2c_transfer(client->adapter, msg, 1) == 1)
			break;
		msleep(20);
	}

	if (retry == toRetry) {
		E("%s: i2c_write_block retry over %d\n", __func__, toRetry);
		i2c_error_count = toRetry;
		return -EIO;
	}
	return 0;
}

int i2c_himax_write_command(struct i2c_client *client, uint8_t command,
			    uint8_t toRetry)
{
	return i2c_himax_write(client, command, NULL, 0, toRetry);
}

int i2c_himax_master_write(struct i2c_client *client, uint8_t *data,
			   uint8_t length, uint8_t toRetry)
{
	int retry /*, loop_i*/;
	uint8_t buf[length];

	struct i2c_msg msg[] = {{
		.addr = client->addr, .flags = 0, .len = length, .buf = buf,
	} };

	memcpy(buf, data, length);

	for (retry = 0; retry < toRetry; retry++) {
		if (i2c_transfer(client->adapter, msg, 1) == 1)
			break;
		msleep(20);
	}

	if (retry == toRetry) {
		E("%s: i2c_write_block retry over %d\n", __func__, toRetry);
		i2c_error_count = toRetry;
		return -EIO;
	}
	return 0;
}
#endif

uint8_t himax_int_gpio_read(int pinnum)
{
	return gpio_get_value(himax_tpd_int_gpio);
}

void himax_int_enable(int irqnum, int enable)
{
	I("%s: Entering!\n", __func__);
	if (enable == 1 && irq_enable_count == 0) {
		enable_irq(irqnum);
		irq_enable_count++;
		private_ts->irq_enabled = 1;
	} else if (enable == 0 && irq_enable_count == 1) {
		disable_irq_nosync(irqnum);
		irq_enable_count--;
		private_ts->irq_enabled = 0;
	}
	I("irq_enable_count = %d\n", irq_enable_count);
}

#ifdef HX_RST_PIN_FUNC
void himax_rst_gpio_set(int pinnum, uint8_t value)
{
	if (value)
		tpd_gpio_output(himax_tpd_rst_gpio_number, 1);
	else
		tpd_gpio_output(himax_tpd_rst_gpio_number, 0);
}
#endif

int himax_gpio_power_config(struct i2c_client *client,
			    struct himax_i2c_platform_data *pdata)
{
	int error = 0;

	error = regulator_enable(tpd->reg);
	if (error != 0)
		TPD_DMESG("Failed to enable reg-vgp6: %d\n", error);
	msleep(100);

#ifdef HX_RST_PIN_FUNC
	tpd_gpio_output(himax_tpd_rst_gpio_number, 1);
	msleep(20);
	tpd_gpio_output(himax_tpd_rst_gpio_number, 0);
	msleep(20);
	tpd_gpio_output(himax_tpd_rst_gpio_number, 1);
#endif

	TPD_DMESG("mtk_tpd: himax reset over\n");

	/* set INT mode */

	tpd_gpio_as_int(himax_tpd_int_gpio_number);
	return 0;
}

static void himax_ts_isr_func(struct himax_ts_data *ts)
{
	himax_ts_work(ts);
}

irqreturn_t himax_ts_thread(int irq, void *ptr)
{
	struct himax_ts_data *ts = ptr;

	if (ts->debug_log_level & BIT(2))
		himax_log_touch_int_devation(HX_FINGER_ON);

	himax_ts_isr_func((struct himax_ts_data *)ptr);

	if (ts->debug_log_level & BIT(2))
		himax_log_touch_int_devation(HX_FINGER_LEAVE);

	return IRQ_HANDLED;
}

static void himax_ts_work_func(struct work_struct *work)
{
	struct himax_ts_data *ts =
		container_of(work, struct himax_ts_data, work);
	himax_ts_work(ts);
}

int himax_int_register_trigger(struct i2c_client *client)
{
	int ret = NO_ERR;
	struct himax_ts_data *ts = i2c_get_clientdata(client);

	if (ic_data->HX_INT_IS_EDGE) {
		ret = request_threaded_irq(client->irq, NULL, himax_ts_thread,
					   IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
					   client->name, ts);
	} else {
		ret = request_threaded_irq(client->irq, NULL, himax_ts_thread,
					   IRQF_TRIGGER_LOW | IRQF_ONESHOT,
					   client->name, ts);
	}

	return ret;
}

int himax_int_en_set(struct i2c_client *client)
{
	int ret = NO_ERR;

	ret = himax_int_register_trigger(client);

	return ret;
}

int himax_ts_register_interrupt(struct i2c_client *client)
{
	struct himax_ts_data *ts = i2c_get_clientdata(client);
	struct device_node *node = NULL;
	u32 ints[2] = {0, 0};
	int ret = 0;

	node = of_find_matching_node(node, touch_of_match);
	if (node) {
		of_property_read_u32_array(node, "debounce", ints,
					   ARRAY_SIZE(ints));
		gpio_set_debounce(ints[0], ints[1]);
		himax_touch_irq = irq_of_parse_and_map(node, 0);
		I("himax_touch_irq=%ud\n", himax_touch_irq);
		client->irq = himax_touch_irq;
		ts->client->irq = himax_touch_irq;
	} else {
		I("[%s] tpd request_irq can not find touch eint device node!.",
		  __func__);
	}

	ts->irq_enabled = 0;
	ts->use_irq = 0;

	/* Work functon */
	if (client->irq) { /*INT mode*/

		ts->use_irq = 1;
		ret = himax_int_register_trigger(client);
		if (ret == 0) {
			ts->irq_enabled = 1;
			irq_enable_count = 1;
			I("%s: irq enabled at qpio: %d\n", __func__,
			  client->irq);
#ifdef HX_SMART_WAKEUP
			irq_set_irq_wake(client->irq, 1);
#endif
		} else {
			ts->use_irq = 0;
			E("%s: request_irq failed\n", __func__);
		}
	} else {
		I("%s: client->irq is empty, use polling mode.\n", __func__);
	}

	if (!ts->use_irq) /*if use polling mode need to disable */
			  /* HX_ESD_RECOVERY function*/
	{
		ts->himax_wq = create_singlethread_workqueue("himax_touch");

		INIT_WORK(&ts->work, himax_ts_work_func);

		hrtimer_init(&ts->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
		ts->timer.function = himax_ts_timer_func;
		hrtimer_start(&ts->timer, ktime_set(1, 0), HRTIMER_MODE_REL);
		I("%s: polling mode enabled\n", __func__);
	}
	return ret;
}

int himax_common_probe(struct i2c_client *client,
		       const struct i2c_device_id *id)
{
	int ret = 0;
#if defined(MTK_I2C_DMA)
	client->dev.coherent_dma_mask = DMA_BIT_MASK(32);
	gpDMABuf_va = (u8 *)dma_alloc_coherent(
		&client->dev, 4096, (dma_addr_t *)&gpDMABuf_pa, GFP_KERNEL);
	if (!gpDMABuf_va) {
		E("Allocate DMA I2C Buffer failed\n");
		ret = -ENODEV;
		goto err_alloc_MTK_DMA_failed;
	}
	memset(gpDMABuf_va, 0, 4096);
#endif

	i2c_client_point = client;
	client->addr = 0x48;
	ret = himax_chip_common_probe(client, id);

#if defined(MTK_I2C_DMA)
	if (ret) {
		if (gpDMABuf_va) {
			dma_free_coherent(&client->dev, 4096, gpDMABuf_va,
					  (dma_addr_t)gpDMABuf_pa);
			gpDMABuf_va = NULL;
			gpDMABuf_pa = NULL;
		}
	}
err_alloc_MTK_DMA_failed:
#endif
	return ret;
}

int himax_common_remove(struct i2c_client *client)
{
	int ret = 0;

	himax_chip_common_remove(client);

	if (gpDMABuf_va) {
		dma_free_coherent(&client->dev, 4096, gpDMABuf_va,
				  (dma_addr_t)gpDMABuf_pa);
		gpDMABuf_va = NULL;
		gpDMABuf_pa = NULL;
	}
	return ret;
}

static void himax_common_suspend(struct device *dev)
{
	struct himax_ts_data *ts = dev_get_drvdata(&i2c_client_point->dev);

	I("%s: enter\n", __func__);

	himax_chip_common_suspend(ts);
	I("%s: END\n", __func__);
}
static void himax_common_resume(struct device *dev)
{
	struct himax_ts_data *ts = dev_get_drvdata(&i2c_client_point->dev);

	I("%s: enter\n", __func__);

	himax_chip_common_resume(ts);

	I("%s: END\n", __func__);
}

#if defined(CONFIG_FB)
int fb_notifier_callback(struct notifier_block *self, unsigned long event,
			 void *data)
{
	struct fb_event *evdata = data;
	int *blank;
	struct himax_ts_data *ts =
		container_of(self, struct himax_ts_data, fb_notif);

	I(" %s\n", __func__);
	if (evdata && evdata->data && event == FB_EVENT_BLANK && ts &&
	    ts->client) {
		blank = evdata->data;
		switch (*blank) {
		case FB_BLANK_UNBLANK:
			himax_common_resume(&ts->client->dev);
			break;

		case FB_BLANK_POWERDOWN:
		case FB_BLANK_HSYNC_SUSPEND:
		case FB_BLANK_VSYNC_SUSPEND:
		case FB_BLANK_NORMAL:
			himax_common_suspend(&ts->client->dev);
			break;
		}
	}

	return 0;
}
#endif

static int himax_common_detect(struct i2c_client *client,
			       struct i2c_board_info *info)
{
	strcpy(info->type, TPD_DEVICE);
	return 0;
}

static const struct i2c_device_id himax_common_ts_id[] = {
	{HIMAX_common_NAME, 0}, {} };

static struct i2c_driver tpd_i2c_driver = {
	.probe = himax_common_probe,
	.remove = himax_common_remove,
	.detect = himax_common_detect,
	.driver = {


			.name = HIMAX_common_NAME,
			.of_match_table = of_match_ptr(himax_match_table),
		},
	.id_table = himax_common_ts_id,
	.address_list = (const unsigned short *)forces,
};

static int himax_common_local_init(void)
{
	int retval;

	I("[Himax] Himax_ts I2C Touchscreen Driver local init\n");

	tpd->reg = regulator_get(tpd->tpd_dev, "vtouch");
	retval = regulator_set_voltage(tpd->reg, 2800000, 2800000);
	if (retval != 0)
		E("Failed to set voltage 2V8: %d\n", retval);

	if (i2c_add_driver(&tpd_i2c_driver) != 0) {
		I("unable to add i2c driver.\n");
		return -1;
	}

/* input_set_abs_params(tpd->input_dev, ABS_MT_TRACKING_ID, 0, */
/* (HIMAX_MAX_TOUCH-1), 0, 0); */

/* set vendor string */
/* client->input_devid.vendor = 0x00; */
/* client->input_dev->id.product = tpd_info.pid; */
/* client-->input_dev->id.version = tpd_info.vid; */
#if defined(HX_PLATFOME_DEFINE_KEY)
	if (tpd_dts_data.use_tpd_button) {
		I("tpd_dts_data.use_tpd_button %d\n",
		  tpd_dts_data.use_tpd_button);
		tpd_button_setting(tpd_dts_data.tpd_key_num,
				   tpd_dts_data.tpd_key_local,
				   tpd_dts_data.tpd_key_dim_local);
	}
#endif

	I("end %s, %d\n", __func__, __LINE__);
	tpd_type_cap = 1;

	return 0;
}

static struct tpd_driver_t tpd_device_driver = {
	.tpd_device_name = HIMAX_common_NAME,
	.tpd_local_init = himax_common_local_init,
	.suspend = himax_common_suspend,
	.resume = himax_common_resume,
#ifdef TPD_HAVE_BUTTON
	.tpd_have_button = 1,
#else
	.tpd_have_button = 0,
#endif
};

static int __init himax_common_init(void)
{
	I("Himax_common touch panel driver init\n");
	tpd_get_dts_info();
	if (tpd_driver_add(&tpd_device_driver) < 0)
		E("Failed to add Driver!\n");

	return 0;
}

static void __exit himax_common_exit(void)
{
	tpd_driver_remove(&tpd_device_driver);
}
module_init(himax_common_init);
module_exit(himax_common_exit);

MODULE_DESCRIPTION("Himax_common driver");
MODULE_LICENSE("GPL");
