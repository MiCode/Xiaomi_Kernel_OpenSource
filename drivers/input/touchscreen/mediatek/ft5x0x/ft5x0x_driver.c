/*
 * Copyright (C) 2010 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/wait.h>
#include <linux/time.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/gpio.h>

#include "tpd_ft5x0x_common.h"
/* #include "ft5x06_ex_fun.h" */

#include "tpd.h"

/* #define TIMER_DEBUG */

#ifdef TIMER_DEBUG
#include <linux/timer.h>
#include <linux/jiffies.h>
#include <linux/module.h>
#endif

#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>

#ifdef CONFIG_MTK_SENSOR_HUB_SUPPORT
#include <mach/md32_ipi.h>
#include <mach/md32_helper.h>
#include <linux/irqchip/mtk-gic-extend.h>
#endif

#ifdef CONFIG_MTK_SENSOR_HUB_SUPPORT
enum DOZE_T {
	DOZE_DISABLED = 0,
	DOZE_ENABLED = 1,
	DOZE_WAKEUP = 2,
};
static enum DOZE_T doze_status = DOZE_DISABLED;
#endif

#ifdef CONFIG_MTK_SENSOR_HUB_SUPPORT
static s8 ftp_enter_doze(struct i2c_client *client);
static s8 ftp_exit_doze(struct i2c_client *client);

enum TOUCH_IPI_CMD_T {
	/* SCP->AP */
	IPI_COMMAND_SA_GESTURE_TYPE,
	/* AP->SCP */
	IPI_COMMAND_AS_CUST_PARAMETER,
	IPI_COMMAND_AS_ENTER_DOZEMODE,
	IPI_COMMAND_AS_ENABLE_GESTURE,
	IPI_COMMAND_AS_GESTURE_SWITCH,
};

struct Touch_Cust_Setting {
	u32 i2c_num;
	u32 int_num;
	u32 io_int;
	u32 io_rst;
};

struct Touch_IPI_Packet {
	u32 cmd;
	union {
		u32 data;
		struct Touch_Cust_Setting tcs;
	} param;
};

static bool tpd_scp_doze_en;
/*static bool tpd_scp_doze_en = true;*/
DEFINE_MUTEX(i2c_access);
#endif

#define TPD_SUPPORT_POINTS	5


struct i2c_client *i2c_client = NULL;
struct task_struct *thread = NULL;

#ifdef CONFIG_FT_AUTO_UPGRADE_SUPPORT

#ifdef CONFIG_MTK_I2C_EXTENSION
u8 *tpd_i2c_dma_va = NULL;
dma_addr_t tpd_i2c_dma_pa = 0;
#endif				/* CONFIG_MTK_I2C_EXTENSION */
#endif
static DECLARE_WAIT_QUEUE_HEAD(waiter);

static irqreturn_t tpd_eint_interrupt_handler(int irq, void *dev_id);


static int tpd_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int tpd_i2c_detect(struct i2c_client *client, struct i2c_board_info *info);
static int tpd_remove(struct i2c_client *client);
static int touch_event_handler(void *unused);
static void tpd_resume(struct device *h);
static void tpd_suspend(struct device *h);

static int tpd_flag;
/*static int point_num = 0;
static int p_point_num = 0;*/

unsigned int tpd_rst_gpio_number = 0;
unsigned int tpd_int_gpio_number = 0;
unsigned int touch_irq = 0;
#define TPD_OK 0

/* Register define */
#define DEVICE_MODE	0x00
#define GEST_ID		0x01
#define TD_STATUS	0x02

#define TOUCH1_XH	0x03
#define TOUCH1_XL	0x04
#define TOUCH1_YH	0x05
#define TOUCH1_YL	0x06

#define TOUCH2_XH	0x09
#define TOUCH2_XL	0x0A
#define TOUCH2_YH	0x0B
#define TOUCH2_YL	0x0C

#define TOUCH3_XH	0x0F
#define TOUCH3_XL	0x10
#define TOUCH3_YH	0x11
#define TOUCH3_YL	0x12

#define TPD_RESET_ISSUE_WORKAROUND
#define TPD_MAX_RESET_COUNT	3

#ifdef TIMER_DEBUG

static struct timer_list test_timer;

static void timer_func(unsigned long data)
{
	tpd_flag = 1;
	wake_up_interruptible(&waiter);

	mod_timer(&test_timer, jiffies + 100*(1000/HZ));
}

static int init_test_timer(void)
{
	memset((void *)&test_timer, 0, sizeof(test_timer));
	test_timer.expires  = jiffies + 100*(1000/HZ);
	test_timer.function = timer_func;
	test_timer.data     = 0;
	init_timer(&test_timer);
	add_timer(&test_timer);
	return 0;
}
#endif


#if defined(CONFIG_TPD_ROTATE_90) || defined(CONFIG_TPD_ROTATE_270) || defined(CONFIG_TPD_ROTATE_180)
static void tpd_swap_xy(int *x, int *y)
{
	int temp = 0;

	temp = *x;
	*x = *y;
	*y = temp;
}

static void tpd_rotate_90(int *x, int *y)
{
	int temp;

	*x = TPD_RES_X + 1 - *x;

	*x = (*x * TPD_RES_Y) / TPD_RES_X;
	*y = (*y * TPD_RES_X) / TPD_RES_Y;

	tpd_swap_xy(x, y);
}
static void tpd_rotate_180(int *x, int *y)
{
	*y = TPD_RES_Y + 1 - *y;
	*x = TPD_RES_X + 1 - *x;
}
static void tpd_rotate_270(int *x, int *y)
{
	int temp;

	*y = TPD_RES_Y + 1 - *y;

	*x = (*x * TPD_RES_Y) / TPD_RES_X;
	*y = (*y * TPD_RES_X) / TPD_RES_Y;

	tpd_swap_xy(x, y);
}
#endif
struct touch_info {
	int y[TPD_SUPPORT_POINTS];
	int x[TPD_SUPPORT_POINTS];
	int p[TPD_SUPPORT_POINTS];
	int count;
};


#if (defined(CONFIG_TPD_HAVE_CALIBRATION) && !defined(CONFIG_TPD_CUSTOM_CALIBRATION))
/* static int tpd_calmat_local[8]     = TPD_CALIBRATION_MATRIX; */
/* static int tpd_def_calmat_local[8] = TPD_CALIBRATION_MATRIX; */
static int tpd_def_calmat_local_normal[8]  = TPD_CALIBRATION_MATRIX_ROTATION_NORMAL;
static int tpd_def_calmat_local_factory[8] = TPD_CALIBRATION_MATRIX_ROTATION_FACTORY;
#endif

static const struct i2c_device_id ft5x0x_tpd_id[] = {{"ft5x0x", 0}, {} };
static const struct of_device_id ft5x0x_dt_match[] = {
	{.compatible = "mediatek,cap_touch"},
	{},
};
MODULE_DEVICE_TABLE(of, ft5x0x_dt_match);

static struct i2c_driver tpd_i2c_driver = {
	.driver = {
		.of_match_table = of_match_ptr(ft5x0x_dt_match),
		.name = "ft5x0x",
	},
	.probe = tpd_probe,
	.remove = tpd_remove,
	.id_table = ft5x0x_tpd_id,
	.detect = tpd_i2c_detect,
};

static int of_get_ft5x0x_platform_data(struct device *dev)
{
	/*int ret, num;*/

	if (dev->of_node) {
		const struct of_device_id *match;

		match = of_match_device(of_match_ptr(ft5x0x_dt_match), dev);
		if (!match) {
			TPD_DMESG("Error: No device match found\n");
			return -ENODEV;
		}
	}
	tpd_rst_gpio_number = of_get_named_gpio(dev->of_node, "rst-gpio", 0);
	tpd_int_gpio_number = of_get_named_gpio(dev->of_node, "int-gpio", 0);
	/*ret = of_property_read_u32(dev->of_node, "rst-gpio", &num);
	if (!ret)
		tpd_rst_gpio_number = num;
	ret = of_property_read_u32(dev->of_node, "int-gpio", &num);
	if (!ret)
		tpd_int_gpio_number = num;
  */
	TPD_DMESG("g_vproc_en_gpio_number %d\n", tpd_rst_gpio_number);
	TPD_DMESG("g_vproc_vsel_gpio_number %d\n", tpd_int_gpio_number);
	return 0;
}

#ifdef CONFIG_MTK_SENSOR_HUB_SUPPORT

void tpd_scp_wakeup_enable(bool en)
{
	tpd_scp_doze_en = en;
}

void tpd_enter_doze(void)
{

}
static ssize_t show_scp_ctrl(struct device *dev, struct device_attribute *attr, char *buf)
{
	return 0;
}
static ssize_t store_scp_ctrl(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned long cmd;
	/*struct Touch_IPI_Packet ipi_pkt;*/

	if (kstrtoul(buf, 10, &cmd)) {
		TPD_DEBUG("[SCP_CTRL]: Invalid values\n");
		return -EINVAL;
	}

	TPD_DEBUG("SCP_CTRL: Command=%lu", cmd);
	switch (cmd) {
	case 1:
	    /* make touch in doze mode */
	    tpd_scp_wakeup_enable(true);
	    tpd_suspend(NULL);
	    break;
	case 2:
	    tpd_resume(NULL);
	    break;
		/*case 3:
	    // emulate in-pocket on
	    ipi_pkt.cmd = IPI_COMMAND_AS_GESTURE_SWITCH,
	    ipi_pkt.param.data = 1;
		md32_ipi_send(IPI_TOUCH, &ipi_pkt, sizeof(ipi_pkt), 0);
	    break;
	case 4:
	    // emulate in-pocket off
	    ipi_pkt.cmd = IPI_COMMAND_AS_GESTURE_SWITCH,
	    ipi_pkt.param.data = 0;
		md32_ipi_send(IPI_TOUCH, &ipi_pkt, sizeof(ipi_pkt), 0);
	    break;*/
	case 5:
		{
				struct Touch_IPI_Packet ipi_pkt;

				ipi_pkt.cmd = IPI_COMMAND_AS_CUST_PARAMETER;
			    ipi_pkt.param.tcs.i2c_num = 0;/* shuold be modify according your hardware design*/
			ipi_pkt.param.tcs.int_num = get_hardware_irq(touch_irq);
				ipi_pkt.param.tcs.io_int = tpd_int_gpio_number;
			ipi_pkt.param.tcs.io_rst = tpd_rst_gpio_number;
			if (md32_ipi_send(IPI_TOUCH, &ipi_pkt, sizeof(ipi_pkt), 0) < 0)
				TPD_DEBUG("[TOUCH] IPI cmd failed (%d)\n", ipi_pkt.cmd);

			break;
		}
	default:
	    TPD_DEBUG("[SCP_CTRL] Unknown command");
	    break;
	}

	return size;
}
static DEVICE_ATTR(tpd_scp_ctrl, 0664, show_scp_ctrl, store_scp_ctrl);
#endif

static struct device_attribute *ft5x0x_attrs[] = {
#ifdef CONFIG_MTK_SENSOR_HUB_SUPPORT
	&dev_attr_tpd_scp_ctrl,
#endif
};


static void tpd_down(int x, int y, int p)
{
#if defined(CONFIG_TPD_ROTATE_90)
	tpd_rotate_90(&x, &y);
#elif defined(CONFIG_TPD_ROTATE_270)
	tpd_rotate_270(&x, &y);
#elif defined(CONFIG_TPD_ROTATE_180)
	tpd_rotate_180(&x, &y);
#endif

#ifdef TPD_SOLVE_CHARGING_ISSUE
	if (0 != x) {
#else
	{
#endif
		TPD_DEBUG("%s x:%d y:%d p:%d\n", __func__, x, y, p);
		input_report_key(tpd->dev, BTN_TOUCH, 1);
		input_report_abs(tpd->dev, ABS_MT_TOUCH_MAJOR, 1);
		input_report_abs(tpd->dev, ABS_MT_POSITION_X, x);
		input_report_abs(tpd->dev, ABS_MT_POSITION_Y, y);
		input_mt_sync(tpd->dev);
	}
}

static void tpd_up(int x, int y)
{
#if defined(CONFIG_TPD_ROTATE_90)
	tpd_rotate_90(&x, &y);
#elif defined(CONFIG_TPD_ROTATE_270)
	tpd_rotate_270(&x, &y);
#elif defined(CONFIG_TPD_ROTATE_180)
	tpd_rotate_180(&x, &y);
#endif

#ifdef TPD_SOLVE_CHARGING_ISSUE
	if (0 != x) {
#else
	{
#endif
		TPD_DEBUG("%s x:%d y:%d\n", __func__, x, y);
		input_report_key(tpd->dev, BTN_TOUCH, 0);
		input_mt_sync(tpd->dev);
	}
}

/*Coordination mapping*/
static void tpd_calibrate_driver(int *x, int *y)
{
	int tx;

	tx = ((tpd_def_calmat[0] * (*x)) + (tpd_def_calmat[1] * (*y)) + (tpd_def_calmat[2])) >> 12;
	*y = ((tpd_def_calmat[3] * (*x)) + (tpd_def_calmat[4] * (*y)) + (tpd_def_calmat[5])) >> 12;
	*x = tx;
}

static int tpd_touchinfo(struct touch_info *cinfo, struct touch_info *pinfo)
{
	int i = 0;
	char data[40] = {0};
	u8 report_rate = 0;
	u16 high_byte, low_byte;

	i2c_smbus_read_i2c_block_data(i2c_client, 0x00, 8, &(data[0]));
	i2c_smbus_read_i2c_block_data(i2c_client, 0x08, 8, &(data[8]));
	i2c_smbus_read_i2c_block_data(i2c_client, 0x10, 8, &(data[16]));
	i2c_smbus_read_i2c_block_data(i2c_client, 0x18, 8, &(data[24]));
	i2c_smbus_read_i2c_block_data(i2c_client, 0xa6, 1, &(data[32]));
	i2c_smbus_read_i2c_block_data(i2c_client, 0x88, 1, &report_rate);

	TPD_DEBUG("FW version=%x]\n", data[32]);

#if 0
	TPD_DEBUG("received raw data from touch panel as following:\n");
	for (i = 0; i < 8; i++)
		TPD_DEBUG("data[%d] = 0x%02X ", i, data[i]);
	TPD_DEBUG("\n");
	for (i = 8; i < 16; i++)
		TPD_DEBUG("data[%d] = 0x%02X ", i, data[i]);
	TPD_DEBUG("\n");
	for (i = 16; i < 24; i++)
		TPD_DEBUG("data[%d] = 0x%02X ", i, data[i]);
	TPD_DEBUG("\n");
	for (i = 24; i < 32; i++)
		TPD_DEBUG("data[%d] = 0x%02X ", i, data[i]);
	TPD_DEBUG("\n");
#endif
	if (report_rate < 8) {
		report_rate = 0x8;
		if ((i2c_smbus_write_i2c_block_data(i2c_client, 0x88, 1, &report_rate)) < 0)
			TPD_DMESG("I2C write report rate error, line: %d\n", __LINE__);
	}

	/* Device Mode[2:0] == 0 :Normal operating Mode*/
	if ((data[0] & 0x70) != 0)
		return false;

	memcpy(pinfo, cinfo, sizeof(struct touch_info));
	memset(cinfo, 0, sizeof(struct touch_info));
	for (i = 0; i < TPD_SUPPORT_POINTS; i++)
		cinfo->p[i] = 1;	/* Put up */

	/*get the number of the touch points*/
	cinfo->count = data[2] & 0x0f;

	TPD_DEBUG("Number of touch points = %d\n", cinfo->count);

	TPD_DEBUG("Procss raw data...\n");

	for (i = 0; i < cinfo->count; i++) {
		cinfo->p[i] = (data[3 + 6 * i] >> 6) & 0x0003; /* event flag */

		/*get the X coordinate, 2 bytes*/
		high_byte = data[3 + 6 * i];
		high_byte <<= 8;
		high_byte &= 0x0F00;

		low_byte = data[3 + 6 * i + 1];
		low_byte &= 0x00FF;
		cinfo->x[i] = high_byte | low_byte;

		/*get the Y coordinate, 2 bytes*/
		high_byte = data[3 + 6 * i + 2];
		high_byte <<= 8;
		high_byte &= 0x0F00;

		low_byte = data[3 + 6 * i + 3];
		low_byte &= 0x00FF;
		cinfo->y[i] = high_byte | low_byte;

		TPD_DEBUG(" cinfo->x[%d] = %d, cinfo->y[%d] = %d, cinfo->p[%d] = %d\n", i,
		cinfo->x[i], i, cinfo->y[i], i, cinfo->p[i]);
	}




#ifdef CONFIG_TPD_HAVE_CALIBRATION
	for (i = 0; i < cinfo->count; i++) {
		tpd_calibrate_driver(&(cinfo->x[i]), &(cinfo->y[i]));
		TPD_DEBUG(" cinfo->x[%d] = %d, cinfo->y[%d] = %d, cinfo->p[%d] = %d\n", i,
		cinfo->x[i], i, cinfo->y[i], i, cinfo->p[i]);
	}
#endif

	return true;

};

static int touch_event_handler(void *unused)
{
	int i = 0;
	struct touch_info cinfo, pinfo, finfo;
	struct sched_param param = { .sched_priority = 4 };

	if (tpd_dts_data.use_tpd_button) {
		memset(&finfo, 0, sizeof(struct touch_info));
		for (i = 0; i < TPD_SUPPORT_POINTS; i++)
			finfo.p[i] = 1;
	}

	sched_setscheduler(current, SCHED_RR, &param);

	do {
		/*enable_irq(touch_irq);*/
		set_current_state(TASK_INTERRUPTIBLE);
		wait_event_interruptible(waiter, tpd_flag != 0);

		tpd_flag = 0;

		set_current_state(TASK_RUNNING);

		TPD_DEBUG("touch_event_handler start\n");

		if (tpd_touchinfo(&cinfo, &pinfo)) {
			if (tpd_dts_data.use_tpd_button) {
				if (cinfo.p[0] == 0)
					memcpy(&finfo, &cinfo, sizeof(struct touch_info));
			}

			if ((cinfo.y[0] >= TPD_RES_Y) && (pinfo.y[0] < TPD_RES_Y)
			&& ((pinfo.p[0] == 0) || (pinfo.p[0] == 2))) {
				TPD_DEBUG("Dummy release --->\n");
				tpd_up(pinfo.x[0], pinfo.y[0]);
				input_sync(tpd->dev);
				continue;
			}

			if (tpd_dts_data.use_tpd_button) {
				if ((cinfo.y[0] <= TPD_RES_Y && cinfo.y[0] != 0) && (pinfo.y[0] > TPD_RES_Y)
				&& ((pinfo.p[0] == 0) || (pinfo.p[0] == 2))) {
					TPD_DEBUG("Dummy key release --->\n");
					tpd_button(pinfo.x[0], pinfo.y[0], 0);
					input_sync(tpd->dev);
					continue;
				}

			if ((cinfo.y[0] > TPD_RES_Y) || (pinfo.y[0] > TPD_RES_Y)) {
				if (finfo.y[0] > TPD_RES_Y) {
					if ((cinfo.p[0] == 0) || (cinfo.p[0] == 2)) {
							TPD_DEBUG("Key press --->\n");
							tpd_button(pinfo.x[0], pinfo.y[0], 1);
					} else if ((cinfo.p[0] == 1) &&
						((pinfo.p[0] == 0) || (pinfo.p[0] == 2))) {
							TPD_DEBUG("Key release --->\n");
							tpd_button(pinfo.x[0], pinfo.y[0], 0);
					}
					input_sync(tpd->dev);
				}
				continue;
			}
			}

			if (cinfo.count > 0) {
				for (i = 0; i < cinfo.count; i++)
					tpd_down(cinfo.x[i], cinfo.y[i], i + 1);
			} else {
#ifdef TPD_SOLVE_CHARGING_ISSUE
				tpd_up(1, 48);
#else
				tpd_up(cinfo.x[0], cinfo.y[0]);
#endif

			}
			input_sync(tpd->dev);

		}
	} while (!kthread_should_stop());

	TPD_DEBUG("touch_event_handler exit\n");

	return 0;
}

static int tpd_i2c_detect(struct i2c_client *client, struct i2c_board_info *info)
{
	strcpy(info->type, TPD_DEVICE);

	return 0;
}

static irqreturn_t tpd_eint_interrupt_handler(int irq, void *dev_id)
{
	TPD_DEBUG("TPD interrupt has been triggered\n");
	tpd_flag = 1;
	wake_up_interruptible(&waiter);
	return IRQ_HANDLED;
}
static int tpd_irq_registration(void)
{
	struct device_node *node = NULL;
	int ret = 0;

	node = of_find_compatible_node(NULL, NULL, "mediatek,cap_touch");
	if (node) {
		/*touch_irq = gpio_to_irq(tpd_int_gpio_number);*/
		touch_irq = irq_of_parse_and_map(node, 0);
		ret = request_irq(touch_irq, tpd_eint_interrupt_handler,
					IRQF_TRIGGER_FALLING, TPD_DEVICE, NULL);
			if (ret > 0)
				TPD_DMESG("tpd request_irq IRQ LINE NOT AVAILABLE!.");
	} else {
		TPD_DMESG("[%s] tpd request_irq can not find touch eint device node!.", __func__);
	}
	return 0;
}
static int tpd_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int retval = TPD_OK;
	u8 report_rate = 0;
	int reset_count = 0;
	char data;

	i2c_client = client;

	of_get_ft5x0x_platform_data(&client->dev);
	/* configure the gpio pins */
	retval = gpio_request_one(tpd_rst_gpio_number, GPIOF_OUT_INIT_LOW,
				 "touchp_reset");
	if (retval < 0) {
		TPD_DMESG("Unable to request gpio reset_pin\n");
		return -1;
	}
	retval = gpio_request_one(tpd_int_gpio_number, GPIOF_IN,
				 "tpd_int");
	if (retval < 0) {
		TPD_DMESG("Unable to request gpio int_pin\n");
		gpio_free(tpd_rst_gpio_number);
		return -1;
	}
	gpio_direction_output(tpd_rst_gpio_number, 0);
	msleep(20);
	gpio_direction_output(tpd_rst_gpio_number, 1);
	msleep(50);

	TPD_DMESG("mtk_tpd: tpd_probe ft5x0x\n");


	retval = regulator_enable(tpd->reg);
	if (retval != 0)
		TPD_DMESG("Failed to enable reg-vgp6: %d\n", retval);

	/* set INT mode */

	gpio_direction_input(tpd_int_gpio_number);

	tpd_irq_registration();
	msleep(100);

#ifdef CONFIG_FT_AUTO_UPGRADE_SUPPORT
#ifdef CONFIG_MTK_I2C_EXTENSION
	tpd_i2c_dma_va = (u8 *)dma_alloc_coherent(&client->dev, 4096, &tpd_i2c_dma_pa, GFP_KERNEL);
	if (!tpd_i2c_dma_va)
		TPD_DMESG("TPD dma_alloc_coherent error!\n");
	else
		TPD_DMESG("TPD dma_alloc_coherent success!\n");
#endif				/* CONFIG_MTK_I2C_EXTENSION */
#endif

reset_proc:
	if ((i2c_smbus_read_i2c_block_data(i2c_client, 0x00, 1, &data)) < 0) {
		TPD_DMESG("I2C transfer error, line: %d\n", __LINE__);
#ifdef TPD_RESET_ISSUE_WORKAROUND
		if (reset_count < TPD_MAX_RESET_COUNT) {
			reset_count++;
			goto reset_proc;
		}
#endif
		gpio_free(tpd_rst_gpio_number);
		gpio_free(tpd_int_gpio_number);
		return -1;
	}
	tpd_load_status = 1;
#ifdef CONFIG_CUST_FTS_APK_DEBUG
	ft_rw_iic_drv_init(client);

	ft5x0x_create_sysfs(client);

	ft5x0x_create_apk_debug_channel(client);
#endif
#ifdef CONFIG_FT_AUTO_UPGRADE_SUPPORT
	/* msleep(200); */
	msleep(20);/* liuhuan */
	tpd_auto_upgrade(client);
	msleep(200);/* liuhuan */
	/* Reset CTP */
	gpio_direction_output(tpd_rst_gpio_number, 0);
	msleep(20);/* liuhuan */
	gpio_direction_output(tpd_rst_gpio_number, 1);
	/* msleep(50);//liuhuan */
#endif
	msleep(200);/* liuhuan */
	/* Set report rate 80Hz */
	report_rate = 0x8;
	if ((i2c_smbus_write_i2c_block_data(i2c_client, 0x88, 1, &report_rate)) < 0) {
		if ((i2c_smbus_write_i2c_block_data(i2c_client, 0x88, 1, &report_rate)) < 0)
			TPD_DMESG("I2C write report rate error, line: %d\n", __LINE__);
	}

	/* tpd_load_status = 1; */

	thread = kthread_run(touch_event_handler, 0, TPD_DEVICE);
	if (IS_ERR(thread)) {
		retval = PTR_ERR(thread);
		TPD_DMESG(TPD_DEVICE " failed to create kernel thread: %d\n", retval);
	}

	TPD_DMESG("Touch Panel Device Probe %s\n", (retval < TPD_OK) ? "FAIL" : "PASS");

#ifdef TIMER_DEBUG
	init_test_timer();
#endif

	{
		u8 ver;

		i2c_smbus_read_i2c_block_data(client, 0xA6, 1, &ver);

		TPD_DMESG(TPD_DEVICE " i2c_smbus_read_i2c_block_data version : %d\n", ver);
	}

#ifdef CONFIG_MTK_SENSOR_HUB_SUPPORT
	retval = get_md32_semaphore(SEMAPHORE_TOUCH);
	if (retval < 0)
		pr_err("[TOUCH] HW semaphore reqiure timeout\n");
#endif

	return 0;
}

static int tpd_remove(struct i2c_client *client)
{
	TPD_DEBUG("TPD removed\n");
#ifdef CONFIG_CUST_FTS_APK_DEBUG
	ft_rw_iic_drv_exit();
#endif

#ifdef CONFIG_FT_AUTO_UPGRADE_SUPPORT
#ifdef CONFIG_MTK_I2C_EXTENSION
	if (tpd_i2c_dma_va) {
		dma_free_coherent(NULL, 4096, tpd_i2c_dma_va, tpd_i2c_dma_pa);
		tpd_i2c_dma_va = NULL;
		tpd_i2c_dma_pa = 0;
	}
#endif				/* CONFIG_MTK_I2C_EXTENSION */
#endif
	gpio_free(tpd_rst_gpio_number);
	gpio_free(tpd_int_gpio_number);

	return 0;
}

static int tpd_local_init(void)
{
	int retval;

	TPD_DMESG("Focaltech FT5x0x I2C Touchscreen Driver...\n");
	tpd->reg = regulator_get(tpd->tpd_dev, "vtouch");
	retval = regulator_set_voltage(tpd->reg, 2800000, 2800000);
	if (retval != 0) {
		TPD_DMESG("Failed to set reg-vgp6 voltage: %d\n", retval);
		return -1;
	}
	if (i2c_add_driver(&tpd_i2c_driver) != 0) {
		TPD_DMESG("unable to add i2c driver.\n");
		return -1;
	}
     /* tpd_load_status = 1; */
	if (tpd_dts_data.use_tpd_button) {
		tpd_button_setting(tpd_dts_data.tpd_key_num, tpd_dts_data.tpd_key_local,
		tpd_dts_data.tpd_key_dim_local);
	}

#if (defined(TPD_WARP_START) && defined(TPD_WARP_END))
	TPD_DO_WARP = 1;
	memcpy(tpd_wb_start, tpd_wb_start_local, TPD_WARP_CNT * 4);
	memcpy(tpd_wb_end, tpd_wb_start_local, TPD_WARP_CNT * 4);
#endif

#if (defined(CONFIG_TPD_HAVE_CALIBRATION) && !defined(CONFIG_TPD_CUSTOM_CALIBRATION))

	memcpy(tpd_calmat, tpd_def_calmat_local_factory, 8 * 4);
	memcpy(tpd_def_calmat, tpd_def_calmat_local_factory, 8 * 4);

	memcpy(tpd_calmat, tpd_def_calmat_local_normal, 8 * 4);
	memcpy(tpd_def_calmat, tpd_def_calmat_local_normal, 8 * 4);

#endif

	TPD_DMESG("end %s, %d\n", __func__, __LINE__);
	tpd_type_cap = 1;

	return 0;
}

#ifdef CONFIG_MTK_SENSOR_HUB_SUPPORT
static s8 ftp_enter_doze(struct i2c_client *client)
{
	s8 ret = -1;
	s8 retry = 0;
	char gestrue_on = 0x01;
	char gestrue_data;
	int i;

	pr_alert("[gesture] Entering doze mode...");

	/* Enter gestrue recognition mode */
	ret = i2c_smbus_write_i2c_block_data(i2c_client, FT_GESTRUE_MODE_SWITCH_REG, 1, &gestrue_on);
	if (ret < 0) {
		pr_err("[gesture] Failed to enter Doze %d", retry);
		return ret;
	}
	msleep(30);

	for (i = 0; i < 10; i++) {
		i2c_smbus_read_i2c_block_data(i2c_client, FT_GESTRUE_MODE_SWITCH_REG, 1, &gestrue_data);
		if (gestrue_data == 0x01) {
			pr_alert("[gesture] FTP has been working in doze mode!");
			return 0;
		}
		msleep(20);
		ret = i2c_smbus_write_i2c_block_data(i2c_client, FT_GESTRUE_MODE_SWITCH_REG, 1, &gestrue_on);

	}

	return ret;
}
static s8 ftp_exit_doze(struct i2c_client *client)
{
	s8 ret = -1;
	s8 retry = 0;
	char gestrue_off = 0x00;
	char gestrue_data;
	int i;

	pr_alert("[gesture] exiting doze mode...");

	/* Enter gestrue recognition mode*/
	ret = i2c_smbus_write_i2c_block_data(i2c_client, FT_GESTRUE_MODE_SWITCH_REG, 1, &gestrue_off);
	if (ret < 0) {
		pr_err("[gesture] Failed to exit Doze %d", retry);
		return ret;
	}
	msleep(30);

	for (i = 0; i < 10; i++) {
		i2c_smbus_read_i2c_block_data(i2c_client, FT_GESTRUE_MODE_SWITCH_REG, 1, &gestrue_data);
		if (gestrue_data == 0x00) {
			pr_alert("[gesture] FTP has been working in normal mode!");
			return 0;
		}
		msleep(20);
		ret = i2c_smbus_write_i2c_block_data(i2c_client, FT_GESTRUE_MODE_SWITCH_REG, 1, &gestrue_off);
	}

	return ret;
}
#endif

static void tpd_resume(struct device *h)
{
	int retval = TPD_OK;

	TPD_DEBUG("TPD wake up\n");

#ifdef CONFIG_MTK_SENSOR_HUB_SUPPORT
	if (tpd_scp_doze_en) {
		pr_alert("[gesture] tpd_resume: need enable sensor hub, and exit doze mode...");
		retval = get_md32_semaphore(SEMAPHORE_TOUCH);
		if (retval < 0) {
			pr_err("[gesture] HW semaphore reqiure timeout\n");
		} else {
			struct Touch_IPI_Packet ipi_pkt = {.cmd = IPI_COMMAND_AS_ENABLE_GESTURE, .param.data = 0};

			retval = md32_ipi_send(IPI_TOUCH, &ipi_pkt, sizeof(ipi_pkt), 0);
			if (retval < 0)
				pr_err("[gesture] IPI cmd IPI_COMMAND_AS_ENABLE_GESTURE,param.data = 0  failed");
			retval = ftp_exit_doze(i2c_client);
			if (retval < 0)
				pr_err("[gesture] FTP exit Doze mode failed\n");
			else
				doze_status = DOZE_DISABLED;
		}
		if (retval < 0) {
			pr_alert("[gesture]TPD exit doze mode abnormally, so normal resume mode");
			retval = regulator_enable(tpd->reg);
			if (retval != 0)
				pr_alert("[gesture] Failed to enable reg-vgp6: %d\n", retval);

			msleep(100);
			gpio_direction_output(tpd_rst_gpio_number, 0);
			msleep(20);
			gpio_direction_output(tpd_rst_gpio_number, 1);
			msleep(20);
			enable_irq(touch_irq);
		}
	}	else {
		pr_alert("[gesture] tpd_scp_doze_en false or doze_status disabled");
#endif

		retval = regulator_enable(tpd->reg);
		if (retval != 0)
			TPD_DMESG("Failed to enable reg-vgp6: %d\n", retval);

		msleep(100);

		gpio_direction_output(tpd_rst_gpio_number, 0);
		msleep(20);
		gpio_direction_output(tpd_rst_gpio_number, 1);
		msleep(20);
		enable_irq(touch_irq);

#ifdef CONFIG_MTK_SENSOR_HUB_SUPPORT
	}
#endif
}

static void tpd_suspend(struct device *h)
{
	int retval = TPD_OK;
	static char data = 0x3;


#ifdef CONFIG_MTK_SENSOR_HUB_SUPPORT
	if (tpd_scp_doze_en) {
		char gestrue_data;
		/*char gestrue_cmd = 0x03;*/
		static int scp_init_flag;

		pr_alert("[gesture] tpd_suspend: need enable sensor hub, and enter doze mode...");
		tpd_enter_doze();


		/* pr_alert("[tpd_scp_doze]:init=%d en=%d", scp_init_flag, tpd_scp_doze_en); */

		mutex_lock(&i2c_access);

		retval = ftp_enter_doze(i2c_client);
		if (retval < 0) {
				pr_err("[gesture] FTP Enter Doze mode failed\n");
		} else {
			doze_status = DOZE_ENABLED;
			if (scp_init_flag == 0) {
				struct Touch_IPI_Packet ipi_pkt;

				ipi_pkt.cmd = IPI_COMMAND_AS_CUST_PARAMETER;
				ipi_pkt.param.tcs.i2c_num = 0;/* shuold be modify according your hardware design*/
				ipi_pkt.param.tcs.int_num = get_hardware_irq(touch_irq);
				ipi_pkt.param.tcs.io_int = tpd_int_gpio_number;
				ipi_pkt.param.tcs.io_rst = tpd_rst_gpio_number;

				pr_alert("[gesture]SEND CUST command :%d ", IPI_COMMAND_AS_CUST_PARAMETER);

				retval = md32_ipi_send(IPI_TOUCH, &ipi_pkt, sizeof(ipi_pkt), 0);
				if (retval < 0)
					pr_err("[gesture] IPI cmd IPI_COMMAND_AS_CUST_PARAMETER failed");

				msleep(20); /* delay added between continuous command */
				/* Workaround if suffer MD32 reset */
				/* scp_init_flag = 1; */
			}

			{
				int retry = 5;
				struct Touch_IPI_Packet ipi_pkt = {
					.cmd = IPI_COMMAND_AS_ENABLE_GESTURE,
					.param.data = 1
				};

				pr_alert("[gesture] SEND ENABLE GES command :%d ", IPI_COMMAND_AS_ENABLE_GESTURE);
				/* check doze mode */
				i2c_smbus_read_i2c_block_data(i2c_client, FT_GESTRUE_MODE_SWITCH_REG, 1, &gestrue_data);
				pr_alert("========================>0x%x", gestrue_data);
				msleep(20);
				do {
					if (md32_ipi_send(IPI_TOUCH, &ipi_pkt, sizeof(ipi_pkt), 1) == DONE)
						break;
					msleep(20);
					pr_alert("[gesture] ==>retry=%d", retry);
				} while (retry--);
				if (retry <= 0) {
					retval  = -1;
					pr_err("[gesture]md32_ipi_send IPI_COMMAND_AS_ENABLE_GESTURE failed retry=%d",
					retry);
				}
				/*while(release_md32_semaphore(SEMAPHORE_TOUCH) <= 0) {
					//TPD_DEBUG("GTP release md32 sem failed\n");
					pr_err("GTP release md32 sem failed\n");
				}*/
			}
			retval = release_md32_semaphore(SEMAPHORE_TOUCH);
			if (retval < 0)
				pr_err("[gesture] HW semaphore release timeout\n");
		}
		if (retval < 0) {
			pr_alert("[gesture]TPD enter doze mode abnormally, so enter sleep mode");

			disable_irq(touch_irq);
			i2c_smbus_write_i2c_block_data(i2c_client, 0xA5, 1, &data);  /* TP enter sleep mode */

			retval = regulator_disable(tpd->reg);
			if (retval != 0)
				pr_alert("[gesture] Failed to disable reg-vgp6: %d\n", retval);
		}

		mutex_unlock(&i2c_access);
	} else {
		pr_alert("[gesture] tpd_scp_doze_en false\n");
#endif
		TPD_DEBUG("TPD enter sleep\n");

		disable_irq(touch_irq);
		i2c_smbus_write_i2c_block_data(i2c_client, 0xA5, 1, &data);  /* TP enter sleep mode */

		retval = regulator_disable(tpd->reg);
		if (retval != 0)
			TPD_DMESG("Failed to disable reg-vgp6: %d\n", retval);

#ifdef CONFIG_MTK_SENSOR_HUB_SUPPORT
	}
#endif

}

static struct tpd_driver_t tpd_device_driver = {
	.tpd_device_name = "FT5x0x",
	.tpd_local_init = tpd_local_init,
	.suspend = tpd_suspend,
	.resume = tpd_resume,
	.attrs = {
		.attr = ft5x0x_attrs,
		.num  = ARRAY_SIZE(ft5x0x_attrs),
	},
};

/* called when loaded into kernel */
static int __init tpd_driver_init(void)
{
	TPD_DMESG("MediaTek FT5x0x touch panel driver init\n");
	tpd_get_dts_info();
	if (tpd_driver_add(&tpd_device_driver) < 0)
		TPD_DMESG("add FT5x0x driver failed\n");

	return 0;
}

/* should never be called */
static void __exit tpd_driver_exit(void)
{
	TPD_DMESG("MediaTek FT5x0x touch panel driver exit\n");
	tpd_driver_remove(&tpd_device_driver);
}

module_init(tpd_driver_init);
module_exit(tpd_driver_exit);

