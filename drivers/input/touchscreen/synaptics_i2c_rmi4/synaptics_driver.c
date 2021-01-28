// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2012 Synaptics Incorporated.
 */

#include <linux/interrupt.h>

#include <linux/i2c.h>
#include <uapi/linux/sched/types.h>
#include <linux/kthread.h>
/*#include <linux/rtpm_prio.h>*/
#include <linux/wait.h>
#include <linux/time.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include "tpd.h"
#include "include/synaptics_dsx_rmi4_i2c.h"
#include "SynaImage.h"
#include <linux/gpio.h>

/*#include <mach/mt_pm_ldo.h>*/
/* #include <mach/mt_typedefs.h>*/
#ifdef CONFIG_MTK_BOOT
#include "mtk_boot_common.h"
#endif

#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>



/* #define HAVE_TOUCH_KEY */
#define CONFIG_ID 0x30303033




/* DTS2012031404176 linghai 20120314 end */
/* add by huxin */
#ifdef HAVE_TOUCH_KEY
const u16 touch_key_array[] = { KEY_MENU, KEY_HOMEPAGE, KEY_BACK };

#define MAX_KEY_NUM (ARRAY_SIZE(touch_key_array)/sizeof(touch_key_array[0]))
#endif


#if (defined(TPD_WARP_START) && defined(TPD_WARP_END))
static int tpd_wb_start_local[TPD_WARP_CNT] = TPD_WARP_START;
static int tpd_wb_end_local[TPD_WARP_CNT] = TPD_WARP_END;
#endif
#if (defined(TPD_HAVE_CALIBRATION) && !defined(TPD_CUSTOM_CALIBRATION))
static int tpd_calmat_local[8] = TPD_CALIBRATION_MATRIX;
static int tpd_def_calmat_local[8] = TPD_CALIBRATION_MATRIX;
#endif


unsigned int touch_irq;


struct point {
	int x;
	int raw_x;
	int y;
	int raw_y;
	int z;
	int status;
};

struct function_descriptor {
	u16 query_base;
	u16 cmd_base;
	u16 ctrl_base;
	u16 data_base;
	u8 intSrc;
#define FUNCTION_VERSION(x) ((x >> 5) & 3)
#define INTERRUPT_SOURCE_COUNT(x) (x & 7)

	u8 functionNumber;
};

struct tpd_data {
	struct i2c_client *client;
	struct function_descriptor f01;
	struct function_descriptor f11;
	struct function_descriptor f1a;
	u8 fn11_mask;
	u8 fn1a_mask;
	struct point *cur_points;
	struct point *pre_points;
	struct mutex io_ctrl_mutex;
	struct work_struct work;
	int f11_max_x, f11_max_y;
	u8 points_supported;
	u8 data_length;
	u8 current_page;
};

struct tpd_debug {
	u8 button_0d_enabled;
};

static DECLARE_WAIT_QUEUE_HEAD(waiter);
static struct tpd_data *ts;
static struct tpd_debug *td;
#ifdef CONFIG_MTK_BOOT
static u8 boot_mode;
#endif
static int tpd_flag;
static int tpd_halt;
static DEFINE_MUTEX(i2c_access);

/* Function extern */
static int tpd_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int tpd_detect(struct i2c_client *client, struct i2c_board_info *info);
static int tpd_remove(struct i2c_client *client);
/* static void tpd_work_func(struct work_struct *work); */
static void tpd_down(int x, int y, int p, int id);
static void tpd_up(int x, int y);
/* static int tpd_sw_power(struct i2c_client *client, int on); */
static int tpd_clear_interrupt(struct i2c_client *client);
/* static u8 get_config_version(void); */


static const struct i2c_device_id tpd_id[] = { {TPD_DEVICE, 0}, {} };

/* DTS2012040603460 gkf61766 20120406 begin */
static unsigned short force[] = { 0, 0x40, I2C_CLIENT_END, I2C_CLIENT_END };

/* DTS2012040603460 gkf61766 20120406 end */
static const unsigned short *const forces[] = { force, NULL };

/* static struct i2c_client_address_data addr_data = { .forces = forces, }; */

static const struct of_device_id synaptics_dt_match[] = {
	{.compatible = "mediatek,cap_touch"},
	{},
};

MODULE_DEVICE_TABLE(of, synaptics_dt_match);

static struct i2c_driver tpd_i2c_driver = {
	.driver = {
		   .name = TPD_DEVICE,
		   .owner = THIS_MODULE,
		   .of_match_table = of_match_ptr(synaptics_dt_match),
		   },
	.probe = tpd_probe,
	.remove = tpd_remove,
	.id_table = tpd_id,
	.detect = tpd_detect,
	.address_list = (const unsigned short *)forces,
	/* .address_data = &addr_data, */
};

unsigned int tpd_rst_gpio_number;
unsigned int tpd_int_gpio_number;

#ifdef CONFIG_OF
static int of_get_synaptic_platform_data(struct device *dev)
{
	/*int ret, num;*/

	if (dev->of_node) {
		const struct of_device_id *match;

		match = of_match_device(of_match_ptr(synaptics_dt_match), dev);
		if (!match) {
			TPD_DEBUG("Error: No device match found\n");
			return -ENODEV;
		}
	}
	tpd_rst_gpio_number = of_get_named_gpio(dev->of_node, "rst-gpio", 0);
	tpd_int_gpio_number = of_get_named_gpio(dev->of_node, "int-gpio", 0);

	TPD_DEBUG("g_vproc_en_gpio_number %d\n", tpd_rst_gpio_number);
	TPD_DEBUG("g_vproc_vsel_gpio_number %d\n", tpd_int_gpio_number);
	return 0;
}
#else
static int of_get_synaptic_platform_data(struct device *dev)
{
	return 0;
}
#endif

#if PROXIMITY
static ssize_t synaptics_rmi4_f51_enables_show(struct device *dev,
	struct device_attribute *attr, char *buf);

static ssize_t synaptics_rmi4_f51_enables_store(struct device *dev,
	struct device_attribute *attr, const char *buf,
						size_t count);
#endif

static irqreturn_t tpd_eint_handler(int irq, void *dev_id);

struct kobject *properties_kobj_synap;
struct kobject *properties_kobj_driver;


static struct device_attribute attrs[] = {

#if PROXIMITY
	__ATTR(proximity_enables, (0666),
	       synaptics_rmi4_f51_enables_show,
	       synaptics_rmi4_f51_enables_store),
#endif

};

static int tpd_irq_registration(void)
{
	struct device_node *node = NULL;
	int ret = 0;

	TPD_DEBUG("Device Tree Tpd_irq_registration!");

	node = of_find_compatible_node(NULL, NULL, "mediatek,cap_touch");

	if (node) {
		/*touch_irq = gpio_to_irq(tpd_int_gpio_number);*/
		touch_irq = irq_of_parse_and_map(node, 0);
		TPD_DEBUG("touch_irq number %d\n", touch_irq);

		ret = request_irq(touch_irq, tpd_eint_handler,
		IRQF_TRIGGER_FALLING,
					TPD_DEVICE, NULL);
			if (ret > 0)
				TPD_DMESG("tpd IRQ LINE NOT AVAILABLE!.");
	} else {
		TPD_DMESG("tpd can not find touch eint device node!.");
	}

	return ret;
}


static bool exp_fn_inited;
static struct mutex exp_fn_list_mutex;
static struct list_head exp_fn_list;

#if PROXIMITY
static struct synaptics_rmi4_f51_handle *f51;
#endif

#if PROXIMITY
static ssize_t synaptics_rmi4_f51_enables_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int retval;
	unsigned char proximity_enables;

	if (!f51)
		return -ENODEV;

	retval = synaptics_rmi4_i2c_read(f51->rmi4_data,
			f51->proximity_enables_addr,
		&proximity_enables, sizeof(proximity_enables));
	if (retval < 0) {
		dev_info(dev,
			"%s: Failed to read proximity enables, error = %d\n",
			__func__, retval);
		return retval;
	}

	return snprintf(buf, PAGE_SIZE, "0x%02x\n", proximity_enables);
}

static ssize_t synaptics_rmi4_f51_enables_store(struct device *dev,
	struct device_attribute *attr, const char *buf,
						size_t count)
{
	int retval;
	unsigned int input;
	unsigned char proximity_enables;

	if (!f51)
		return -ENODEV;

	if (kstrtoint(buf, 10, &input))
		return -EINVAL;

	proximity_enables = input;

	retval = synaptics_rmi4_i2c_write(f51->rmi4_data,
			f51->proximity_enables_addr,
			&proximity_enables, sizeof(proximity_enables));
	if (retval < 0) {
		dev_info(dev,
			"%s: Failed to write proximity enables, error = %d\n",
			__func__, retval);
		return retval;
	}

	return count;
}
#endif

static int tpd_set_page(struct i2c_client *client,
	unsigned int address)
{
	int retval = 0;
	unsigned char retry;
	unsigned char buf[PAGE_SELECT_LEN];
	unsigned char page;

	page = ((address >> 8) & MASK_8BIT);
	if (page != ts->current_page) {
		buf[0] = MASK_8BIT;
		buf[1] = page;
		for (retry = 0; retry < SYN_I2C_RETRY_TIMES; retry++) {
			retval = i2c_master_send(client,
			buf, PAGE_SELECT_LEN);

			if (retval != PAGE_SELECT_LEN) {
				dev_info(&client->dev, "%s: I2C retry %d\n",
				__func__, retry + 1);
				msleep(20);
			} else {
				ts->current_page = page;
				break;
			}
		}
	} else {
		retval = PAGE_SELECT_LEN;
	}

	return retval;
}

int tpd_i2c_read_data(struct i2c_client *client,
	unsigned short addr, unsigned char *data,
	unsigned short length)
{
	u8 retval = 0;
	u8 retry = 0;
	u8 *pData = data;
	int tmp_addr = addr;
	int left_len = length;
	/* u16 old_flag; */
	mutex_lock(&(ts->io_ctrl_mutex));

	retval = tpd_set_page(client, addr);
	if (retval != PAGE_SELECT_LEN)
		goto exit;

	/* old_flag = client->ext_flag; */
	/* client->addr = client->addr & I2C_MASK_FLAG ; */

	while (left_len > 0) {
		pData[0] = tmp_addr;

		for (retry = 0; retry < SYN_I2C_RETRY_TIMES; retry++) {
			retval = i2c_master_send(client, pData, 1);

			if (retval <= 0) {
				dev_info(&client->dev, "%s: I2C retry %d\n",
				__func__, retry + 1);
				msleep(20);
				continue;
			}

			if (left_len > 8)
				retval = i2c_master_recv(client, pData, 8);
			else
				retval = i2c_master_recv(client,
			pData, left_len);

			if (retval <= 0) {
				dev_info(&client->dev, "%s: I2C retry %d\n",
				__func__, retry + 1);
				msleep(20);
				continue;
			} else
				break;
		}

		left_len -= 8;
		pData += 8;
		tmp_addr += 8;
	}

	/* client->ext_flag = old_flag; */

 exit:
	mutex_unlock(&(ts->io_ctrl_mutex));

	return retval;
}
EXPORT_SYMBOL(tpd_i2c_read_data);

int tpd_i2c_write_data(struct i2c_client *client,
	unsigned short addr, unsigned char *data,
	unsigned short length)
{
	u8 retval = 0;
	u8 retry = 0;
	u8 *pData = data;
	u8 buf[5] = { 0 };
	int tmp_addr = addr;
	int left_len = length;

	mutex_lock(&(ts->io_ctrl_mutex));

	retval = tpd_set_page(client, addr);
	if (retval != PAGE_SELECT_LEN) {
		TPD_DMESG("tpd_set_page fail, retval = %d\n", retval);
		retval = -EIO;
		goto exit;
	}

	while (left_len > 0) {
		buf[0] = tmp_addr;
		for (retry = 0; retry < SYN_I2C_RETRY_TIMES; retry++) {
			if (left_len > 4) {
				memcpy(buf + 1, pData, 4);
				retval = i2c_master_send(client, buf, 5);
			} else {
				memcpy(buf + 1, pData, left_len);
				retval = i2c_master_send(client,
				buf, left_len + 1);
			}

			if (retval > 0)
				break;
			TPD_DMESG("%s: I2C retry %d\n", __func__, retry + 1);
			msleep(20);
		}

		left_len -= 4;
		pData += 4;
		tmp_addr += 4;
	}

 exit:
	mutex_unlock(&(ts->io_ctrl_mutex));

	return retval;
}
EXPORT_SYMBOL(tpd_i2c_write_data);

#if PROXIMITY
static int synaptics_rmi4_f51_report(
	struct synaptics_rmi4_data *rmi4_data,
	struct synaptics_rmi4_fn *fhandler)
{
	int retval;
	unsigned char touch_count = 0;	/* number of touch points */
	unsigned short data_base_addr;
	int x;
	int y;
	int z;
	struct synaptics_rmi4_f51_data *data_reg;

	data_base_addr = fhandler->full_addr.data_base;
	data_reg = (struct synaptics_rmi4_f51_data *)fhandler->data;

	retval = tpd_i2c_read(rmi4_data, data_base_addr,
	data_reg->data, sizeof(data_reg->data));
	if (retval < 0)
		return 0;

	if (data_reg->data[0] == 0x00)
		return 0;

#if sdfsdfadf
	if (data_reg->finger_hover_det) {
		if (data_reg->hover_finger_z > 0) {
			x = (data_reg->hover_finger_x_4__11 << 4) |
			    (data_reg->hover_finger_xy_0__3 & 0x0f);
			y = (data_reg->hover_finger_y_4__11 << 4) |
			    (data_reg->hover_finger_xy_0__3 >> 4);
			z = HOVER_Z_MAX - data_reg->hover_finger_z;

			dev_dbg(&rmi4_data->i2c_client->dev,
		"%s: Hover finger: x = %d, y = %d, z = %d\n",
				__func__, x, y, z);

			input_report_abs(tpd->dev, ABS_MT_POSITION_X, x);
			input_report_abs(tpd->dev, ABS_MT_POSITION_Y, y);
#ifdef INPUT_MULTITOUCH
			input_report_abs(tpd->dev, ABS_MT_DISTANCE, z);
#endif
			input_mt_sync(tpd->dev);

			touch_count++;
		}
	}

	if (data_reg->air_swipe_det) {
		dev_dbg(&rmi4_data->i2c_client->dev,
			"%s: Swipe direction 0 = %d\n", __func__,
			data_reg->air_swipe_dir_0);
		dev_dbg(&rmi4_data->i2c_client->dev,
			"%s: Swipe direction 1 = %d\n", __func__,
			data_reg->air_swipe_dir_1);
	}

	if (data_reg->large_obj_det) {
		dev_dbg(&rmi4_data->i2c_client->dev,
			"%s: Large object activity = %d\n", __func__,
			data_reg->large_obj_act);
	}

	if (data_reg->hover_pinch_det) {
		dev_dbg(&rmi4_data->i2c_client->dev,
			"%s: Hover pinch direction = %d\n", __func__,
			data_reg->hover_pinch_dir);
	}
#endif

	if (!touch_count)
		input_mt_sync(tpd->dev);

	input_sync(tpd->dev);

	return touch_count;
}
#endif

#if PROXIMITY
static int synaptics_rmi4_f51_init(
	struct synaptics_rmi4_data *rmi4_data,
	struct synaptics_rmi4_fn *fhandler,
	struct synaptics_rmi4_fn_desc *fd, unsigned int intr_count)
{
	int retval;
	unsigned char ii;
	unsigned short intr_offset;
	unsigned char proximity_enable_mask = PROXIMITY_ENABLE;
	struct synaptics_rmi4_f51_query query_register;
	struct synaptics_rmi4_f51_data *data_register;

	fhandler->fn_number = fd->fn_number;
	fhandler->num_of_data_sources = fd->intr_src_count;

	fhandler->intr_reg_num = (intr_count + 7) / 8;
	if (fhandler->intr_reg_num != 0)
		fhandler->intr_reg_num -= 1;

	/* Set an enable bit for each data source */
	intr_offset = intr_count % 8;
	fhandler->intr_mask = 0;
	for (ii = intr_offset; ii <
	((fd->intr_src_count & MASK_3BIT) +
	intr_offset); ii++)
		fhandler->intr_mask |= 1 << ii;

	retval = synaptics_rmi4_i2c_read(rmi4_data,
					 fhandler->full_addr.query_base,
					 query_register.data,
					 sizeof(query_register.data));
	if (retval < 0)
		return retval;

	fhandler->data_size = sizeof(data_register->data);
	data_register = kmalloc(fhandler->data_size, GFP_KERNEL);
	fhandler->data = (void *)data_register;

	retval = synaptics_rmi4_i2c_write(rmi4_data,
			fhandler->full_addr.ctrl_base +
			query_register.control_register_count - 1,
			&proximity_enable_mask,
			sizeof(proximity_enable_mask));
	if (retval < 0)
		return retval;

	f51 = kmalloc(sizeof(*f51), GFP_KERNEL);
	f51->rmi4_data = rmi4_data;
	f51->proximity_enables_addr = fhandler->full_addr.ctrl_base +
	    query_register.control_register_count - 1;

	return 0;
}

int synaptics_rmi4_proximity_enables(unsigned char enables)
{
	int retval;
	unsigned char proximity_enables = enables;

	if (!f51)
		return -ENODEV;

	retval = synaptics_rmi4_i2c_write(f51->rmi4_data,
		f51->proximity_enables_addr,
		&proximity_enables, sizeof(proximity_enables));
	if (retval < 0)
		return retval;

	return 0;
}
EXPORT_SYMBOL(synaptics_rmi4_proximity_enables);
#endif

static int tpd_rmi4_read_pdt(struct tpd_data *ts)
{
	int retval;
	unsigned char ii;
	unsigned char offset = 0;
	unsigned char page_number;
	unsigned char intr_count = 0;
	/* unsigned char data_sources = 0; */
	/* unsigned char f01_query[F01_STD_QUERY_LEN]; */
	unsigned char f11_query[F11_STD_QUERY_LEN];
	unsigned int f11_max_xy;
	/* u8  point_length; */
	unsigned short pdt_entry_addr;
	/* unsigned short intr_addr; */
	static unsigned char intsrc = 1;
	/* struct synaptics_rmi4_f01_device_status status; */
	struct synaptics_rmi4_fn_desc rmi_fd;

	/* Scan the page description tables of the pages to service */
	for (page_number = 0; page_number <
	PAGES_TO_SERVICE; page_number++) {
		for (pdt_entry_addr = PDT_START; pdt_entry_addr >
		PDT_END;
		     pdt_entry_addr -= PDT_ENTRY_SIZE) {
			pdt_entry_addr |= (page_number << 8);

			retval = tpd_i2c_read_data(ts->client,
				pdt_entry_addr,
				(unsigned char *)&rmi_fd, sizeof(rmi_fd));
			if (retval < 0)
				return retval;

			if (rmi_fd.fn_number == 0) {
				dev_dbg(&ts->client->dev,
				"%s: Reached end of PDT\n", __func__);
				break;
			}

			dev_dbg(&ts->client->dev,
				"%s: F%02x found (page %d)\n",
				__func__, rmi_fd.fn_number, page_number);

			switch (rmi_fd.fn_number) {
			case SYNAPTICS_RMI4_F01:

				ts->f01.query_base = rmi_fd.query_base_addr;
				ts->f01.ctrl_base = rmi_fd.ctrl_base_addr;
				ts->f01.cmd_base = rmi_fd.cmd_base_addr;
				ts->f01.data_base = rmi_fd.data_base_addr;
				ts->f01.intSrc = intsrc++;
				ts->f01.functionNumber = rmi_fd.fn_number;

				break;

			case SYNAPTICS_RMI4_F11:
				if (rmi_fd.intr_src_count == 0)
					break;

				ts->f11.query_base = rmi_fd.query_base_addr;
				ts->f11.ctrl_base = rmi_fd.ctrl_base_addr;
				ts->f11.cmd_base = rmi_fd.cmd_base_addr;
				ts->f11.data_base = rmi_fd.data_base_addr;
				ts->f11.intSrc = intsrc++;
				ts->f11.functionNumber = rmi_fd.fn_number;

				ts->fn11_mask = 0;
				offset = intr_count % 8;
				for (ii = offset; ii < (rmi_fd.intr_src_count +
				offset); ii++)
					ts->fn11_mask |= 1 << ii;

				retval =
		tpd_i2c_read_data(ts->client, ts->f11.query_base,
		f11_query,
						      sizeof(f11_query));
				if (retval < 0)
					return retval;
				TPD_DEBUG("f11 query base=%d\n",
				ts->f11.query_base);
				/* Maximum number of fingers supported */
				if ((f11_query[1] & MASK_3BIT) <= 4) {
					ts->points_supported =
					(f11_query[1] & MASK_3BIT) + 1;
					TPD_DEBUG("points_supported=%d\n",
					ts->points_supported);
				} else if ((f11_query[1] & MASK_3BIT) == 5) {
					ts->points_supported = 10;
					TPD_DEBUG("points_supported=%d\n",
					ts->points_supported);
				}
				retval =
				    tpd_i2c_read_data(ts->client,
					ts->f11.ctrl_base + 6,
						      (char *)(&f11_max_xy),
							  sizeof(f11_max_xy));
				if (retval < 0)
					return retval;

				/* Maximum x and y */
				ts->f11_max_x = f11_max_xy & 0xFFF;
				ts->f11_max_y = (f11_max_xy >> 16) & 0xFFF;


				ts->pre_points =
				    kzalloc(ts->points_supported *
					sizeof(struct point),
					    GFP_KERNEL);
				if (ts->pre_points == NULL) {
					TPD_DMESG("Error zalloc failed!\n");
					retval = -ENOMEM;
					return retval;
				}

				ts->cur_points =
				    kzalloc(ts->points_supported *
					sizeof(struct point),
					    GFP_KERNEL);
				if (ts->cur_points == NULL) {
					TPD_DMESG("Error zalloc failed!\n");
					retval = -ENOMEM;
					return retval;
				}

				ts->data_length =
				    3 + (2 * ((f11_query[5] &
					MASK_2BIT) == 0 ? 1 : 0));
				break;

			case SYNAPTICS_RMI4_F12:

				break;
			case SYNAPTICS_RMI4_F1A:
				if (rmi_fd.intr_src_count == 0)
					break;

				ts->f1a.query_base = rmi_fd.query_base_addr;
				ts->f1a.ctrl_base = rmi_fd.ctrl_base_addr;
				ts->f1a.cmd_base = rmi_fd.cmd_base_addr;
				ts->f1a.data_base = rmi_fd.data_base_addr;
				ts->f01.intSrc = intsrc++;
				ts->f01.functionNumber = rmi_fd.fn_number;

				td->button_0d_enabled = 1;

				ts->fn1a_mask = 0;
				offset = intr_count % 8;
				for (ii = offset; ii < (rmi_fd.intr_src_count +
				offset); ii++)
					ts->fn1a_mask |= 1 << ii;

				break;

#if PROXIMITY
			case SYNAPTICS_RMI4_F51:
				if (rmi_fd.intr_src_count == 0)
					break;

				break;
#endif
			}
			if (rmi_fd.intr_src_count & 0x03)
				intr_count += rmi_fd.intr_src_count & 0x03;

		}
	}

	return 0;
}

#ifdef TPD_UPDATE_FIRMWARE
static void synaptics_rmi4_detection_work(struct work_struct *work)
{
	struct synaptics_rmi4_exp_fn *exp_fhandler, *next_list_entry;



	mutex_lock(&exp_fn_list_mutex);
	if (!list_empty(&exp_fn_list)) {
		list_for_each_entry_safe(exp_fhandler,
		next_list_entry, &exp_fn_list, link) {
			if ((exp_fhandler->func_init != NULL) &&
			(exp_fhandler->inserted == false)) {
				exp_fhandler->func_init(ts->client);
				exp_fhandler->inserted = true;
			} else if ((exp_fhandler->func_init == NULL) &&
				   (exp_fhandler->inserted == true)) {
				exp_fhandler->func_remove(ts->client);
				list_del(&exp_fhandler->link);
				kfree(exp_fhandler);
			}
		}
	}
	mutex_unlock(&exp_fn_list_mutex);
}

static int touch_update_handler(void *unused)
{
	int retval = 0;

	msleep(10000);

	TPD_DEBUG("start to touch update_handler\n");

	mutex_lock(&i2c_access);

	if (tpd_halt) {
		mutex_unlock(&i2c_access);
		TPD_DMESG("the touch has been suspend\n");
		return 0;
	}

	disable_irq(touch_irq);


	synaptics_rmi4_detection_work(NULL);
	/* synaptics_fw_updater(synaImage); */
	synaptics_fw_updater(NULL);	/* auto detect by sensor id */
	retval = tpd_rmi4_read_pdt(ts);
	if (retval < 0)
		TPD_DMESG("Failed to query device\n");

	tpd_clear_interrupt(ts->client);

	enable_irq(touch_irq);

	mutex_unlock(&i2c_access);
	return 0;
}
#endif

void synaptics_rmi4_new_function(enum exp_fn fn_type,
	bool insert,
	int (*func_init)(struct i2c_client *client),
	void (*func_remove)(struct i2c_client *client),
	void (*func_attn)(struct i2c_client *client,
	unsigned char intr_mask))
{
	struct synaptics_rmi4_exp_fn *exp_fhandler;

	if (!exp_fn_inited) {
		mutex_init(&exp_fn_list_mutex);
		INIT_LIST_HEAD(&exp_fn_list);
		exp_fn_inited = 1;
	}

	mutex_lock(&exp_fn_list_mutex);
	if (insert) {
		exp_fhandler = kzalloc(sizeof(*exp_fhandler),
		GFP_KERNEL);

		exp_fhandler->fn_type = fn_type;
		exp_fhandler->func_init = func_init;
		exp_fhandler->func_attn = func_attn;
		exp_fhandler->func_remove = func_remove;
		exp_fhandler->inserted = false;
		list_add_tail(&exp_fhandler->link, &exp_fn_list);
	} else {
		list_for_each_entry(exp_fhandler, &exp_fn_list, link) {
			if (exp_fhandler->func_init == func_init) {
				exp_fhandler->inserted = false;
				exp_fhandler->func_init = NULL;
				exp_fhandler->func_attn = NULL;
				goto exit;
			}
		}
	}

 exit:
	mutex_unlock(&exp_fn_list_mutex);
}
EXPORT_SYMBOL(synaptics_rmi4_new_function);

static void tpd_down(int x, int y, int p, int id)
{
	input_report_key(tpd->dev, BTN_TOUCH, 1);
	input_report_abs(tpd->dev, ABS_MT_TRACKING_ID, id);
	/* input_report_abs(tpd->dev, ABS_PRESSURE, p); */
	input_report_abs(tpd->dev, ABS_MT_TOUCH_MAJOR, p);
	input_report_abs(tpd->dev, ABS_MT_POSITION_X, x);
	input_report_abs(tpd->dev, ABS_MT_POSITION_Y, y);
	input_mt_sync(tpd->dev);
	TPD_DEBUG_SET_TIME;
	TPD_EM_PRINT(x, y, x, y, 0, 1);

#ifdef CONFIG_MTK_BOOT
	if (tpd_dts_data.use_tpd_button) {
		if (boot_mode == FACTORY_BOOT ||
		boot_mode == RECOVERY_BOOT)
			tpd_button(x, y, 1);
	}
#endif
}

static void tpd_up(int x, int y)
{
	input_report_key(tpd->dev, BTN_TOUCH, 0);
	TPD_DEBUG_SET_TIME;
	TPD_EM_PRINT(x, y, x, y, 0, 0);

#ifdef CONFIG_MTK_BOOT
	if (tpd_dts_data.use_tpd_button) {
		if (boot_mode == FACTORY_BOOT ||
		boot_mode == RECOVERY_BOOT)
			tpd_button(x, y, 0);
	}
#endif
}


static int touch_event_handler(void *unused)
{
	struct sched_param param = {.sched_priority = 4 };
	u16 temp = 0;
	u8 i = 0;
	u8 status = 0;
	u8 retval = 0;
	u8 finger_num = 0;
	u8 finger_status = 0;
	u8 finger_status_reg[3];
	u8 data[F11_STD_DATA_LEN];
	u8 num_of_finger_status_regs = 0;
	u8 button = 0;
	struct point *ppt = NULL;
#ifdef HAVE_TOUCH_KEY
	struct point ppt_v = { 0 };
#endif

	sched_setscheduler(current, SCHED_RR, &param);
	do {
		set_current_state(TASK_INTERRUPTIBLE);

		wait_event_interruptible(waiter, tpd_flag != 0);
		tpd_flag = 0;
		set_current_state(TASK_RUNNING);
		mutex_lock(&i2c_access);

		if (tpd_halt) {
			mutex_unlock(&i2c_access);
			TPD_DEBUG("return for interrupt after suspend...");
			continue;
		}
		/* clear interrupt bit */
		retval = tpd_i2c_read_data(ts->client,
		ts->f01.data_base + 1, &status, 1);

		if (retval < 0) {
			/* disable_irq_nosync(touch_irq);*/
			mutex_unlock(&i2c_access);
			/* enable_irq(touch_irq);*/
			continue;
		}

		if (status & ts->fn11_mask) {
			tpd_i2c_read_data(ts->client, ts->f11.data_base,
			finger_status_reg,
					  (ts->points_supported + 3) / 4);
			num_of_finger_status_regs =
			(ts->points_supported + 3) / 4;
			finger_num = ts->points_supported;

			for (i = 0; i < ts->points_supported; i++) {
				finger_status = finger_status_reg[i / 4];
				finger_status =
				(finger_status >> ((i % 4) * 2)) & 3;

				ppt = &ts->cur_points[i];
				ppt->status = finger_status;

				if (finger_status == 0x01 ||
				finger_status == 0x02) {
					temp =
					    ts->f11.data_base +
						num_of_finger_status_regs +
					    i * ts->data_length;

					tpd_i2c_read_data(ts->client,
							  ts->f11.data_base +
					num_of_finger_status_regs +
					i * ts->data_length, data,
							  ts->data_length);

					ppt->raw_x = ppt->x =
					    (((u16) (data[0]) << 4) |
						(data[2] & 0x0F));
					ppt->raw_y = ppt->y =
					    (((u16) (data[1]) << 4) |
						((data[2] >> 4) & 0x0F));
					ppt->z = data[4];
					pr_debug("[TPD]point %d: [X:%04d, Y:%04d]",
						  i, ppt->x, ppt->y);
					tpd_down(ppt->x, ppt->y, ppt->z, i);
				} else {
					finger_num--;
				}
			}
		}
#ifdef HAVE_TOUCH_KEY
		if (status & ts->fn1a_mask)
			retval = tpd_i2c_read_data(ts->client,
		0x200, &button, 1);

		if (button) {
			for (i = 0; i < MAX_KEY_NUM; i++) {
				if (button & (0x01 << i)) {
					ppt_v.x = tpd_keys_dim_local_wvga[i][0];
					ppt_v.y = tpd_keys_dim_local_wvga[i][1];
					tpd_down(ppt_v.x, ppt_v.y, 20, 10 + i);
				}
			}
		}
#endif
		mutex_unlock(&i2c_access);
		if (!finger_num && !button)
			tpd_up(0, 0);
		input_sync(tpd->dev);
		/* ts->pre_points = ts->cur_points; */
/*        disable_irq_nosync(touch_irq);*/
/*		enable_irq(touch_irq);*/

	} while (!kthread_should_stop());

	return 0;
}



static irqreturn_t tpd_eint_handler(int irq, void *dev_id)
{
	TPD_DEBUG_PRINT_INT;

	tpd_flag = 1;

	wake_up_interruptible(&waiter);

	return IRQ_HANDLED;
}

static int tpd_detect(struct i2c_client *client,
	struct i2c_board_info *info)
{
	strcpy(info->type, TPD_DEVICE);
	return 0;
}

static int tpd_remove(struct i2c_client *client)
{
	TPD_DEBUG("TPD removed\n");
	return 0;
}

static int tpd_clear_interrupt(struct i2c_client *client)
{
	int retval = 0;
	u8 status = 0;

	retval = tpd_i2c_read_data(client, ts->f01.data_base + 1,
	&status, 1);
	if (retval < 0)
		dev_info(&client->dev,
	"%s: Failed to enable attention interrupt\n", __func__);

	return retval;
}

static int tpd_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	/* u8 ii; */
	u8 attr_count;
	/* u8 status = 0; */
	int retval;
	/* int i; */
	u8 data;
	int reset_count = 3;
	/* unsigned char config_id[4]; */
	/* unsigned char tp_id[8]; */
	/* unsigned int config_id_no = 0; */
	/* u16 tp_x_for_lcd=0; */
	/* u16 tp_y_for_lcd=0; */
	/* struct synaptics_rmi4_fn *fhandler; */
	/* struct synaptics_rmi4_data *rmi4_data; */
	/* struct synaptics_rmi4_device_info *rmi; */
	struct task_struct *thread = NULL;

	TPD_DMESG("%s:enter\n", __func__);
 TPD_RESET_PROBE:

	ts = kzalloc(sizeof(struct tpd_data), GFP_KERNEL);
	if (!ts) {
		TPD_DMESG("Failed to alloc mem for tpd_data\n");
		return -ENOMEM;
	}

	td = kzalloc(sizeof(struct tpd_debug), GFP_KERNEL);
	if (!td)
		TPD_DMESG("Failed to alloc mem for tpd_debug\n");

	ts->client = client;
	mutex_init(&(ts->io_ctrl_mutex));


	of_get_synaptic_platform_data(&client->dev);
	/* configure the gpio pins */
	retval = gpio_request_one(tpd_rst_gpio_number,
	GPIOF_OUT_INIT_LOW,
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

	retval = regulator_enable(tpd->reg);
	if (retval != 0) {
		dev_info(&client->dev,
		"Failed to enable reg-vgp6: %d\n", retval);
		goto err_query_device;
	}
	retval = regulator_enable(tpd->io_reg);
	if (retval != 0) {
		dev_info(&client->dev,
		"Failed to enable reg-vgp4: %d\n", retval);
		goto err_query_device;
	}
	msleep(20);
	/*tpd_gpio_output(GTP_RST_PORT, 0);*/
	gpio_direction_output(tpd_rst_gpio_number, 0);
	msleep(50);
	/*tpd_gpio_output(GTP_RST_PORT, 1);*/
	gpio_direction_output(tpd_rst_gpio_number, 1);
	msleep(50);


	if ((tpd_i2c_read_data(ts->client,
	0xEE, &data, 1)) < 0) {
		if (reset_count-- > 0)
			goto TPD_RESET_PROBE;
		dev_info(&client->dev,
		"Can't connect touch panel.\n");
		return -1;
	}

	retval = tpd_rmi4_read_pdt(ts);
	if (retval < 0) {
		dev_info(&client->dev, "Failed to query device\n");
		goto err_query_device;
	}

	if (!exp_fn_inited) {
		mutex_init(&exp_fn_list_mutex);
		INIT_LIST_HEAD(&exp_fn_list);
		exp_fn_inited = 1;
	}
	tpd_clear_interrupt(client);

	properties_kobj_synap =
	kobject_create_and_add("synapics", NULL);

#ifdef HAVE_TOUCH_KEY
	set_bit(EV_KEY, tpd->dev->evbit);
	for (i = 0; i < MAX_KEY_NUM; i++)
		__set_bit(touch_key_array[i],
	tpd->dev->keybit);
#endif

#ifdef VELOCITY_CUSTOM
	tpd_v_magnify_x = TPD_VELOCITY_CUSTOM_X;
	tpd_v_magnify_y = TPD_VELOCITY_CUSTOM_Y;
#endif

#ifdef TPD_UPDATE_FIRMWARE
	thread = kthread_run(touch_update_handler,
	0, TPD_DEVICE);
	if (IS_ERR(thread)) {
		retval = PTR_ERR(thread);
		TPD_DMESG("failed to create kernel thread: %d\n",
		retval);
		goto error_kthread_creat_failed;
	}
#endif

	properties_kobj_driver = kobject_create_and_add("driver",
	properties_kobj_synap);

	for (attr_count = 0; attr_count <
	ARRAY_SIZE(attrs); attr_count++) {
		retval = sysfs_create_file(properties_kobj_driver,
		&attrs[attr_count].attr);

		if (retval < 0) {
			dev_info(&client->dev,
			"%s: Failed to create sysfs attributes\n", __func__);
			goto err_sysfs;
		}
	}

	thread = kthread_run(touch_event_handler, 0, TPD_DEVICE);
	if (IS_ERR(thread)) {
		retval = PTR_ERR(thread);
		TPD_DMESG("failed to create kernel thread: %d\n",
		retval);
		goto error_kthread_creat_failed;
	}

	tpd_irq_registration();

/*   disable_irq_nosync(touch_irq);*/
/*	enable_irq(touch_irq);*/

	tpd_load_status = 1;
	TPD_DMESG("%s: TouchPanel Device Probe %s\n",
	__func__, (retval < 0) ? "FAIL" : "PASS");
	return 0;

 err_sysfs:
	for (attr_count--; attr_count >= 0; attr_count--)
		sysfs_remove_file(properties_kobj_driver,
	&attrs[attr_count].attr);

 error_kthread_creat_failed:
 err_query_device:
	kfree(td);
	kfree(ts);
	/* hwPowerDown(MT65XX_POWER_LDO_VGP2, "TP"); */
	return retval;
}


static int tpd_local_init(void)
{
	int retval;

	TPD_DEBUG("Synaptics I2C Touchscreen Driver load\n");

	tpd->reg = regulator_get(tpd->tpd_dev, "vtouch");
	tpd->io_reg = regulator_get(tpd->tpd_dev, "vtouchio");
	retval = regulator_set_voltage(tpd->reg,
	3300000, 3300000);
	if (retval != 0) {
		TPD_DMESG("Failed to set reg-vgp6 voltage: %d\n",
		retval);
		return -1;
	}
	retval = regulator_set_voltage(tpd->io_reg,
	1800000, 1800000);
	if (retval != 0) {
		TPD_DMESG("Failed to set reg-vgp4 voltage: %d\n",
		retval);
		return -1;
	}

	if (i2c_add_driver(&tpd_i2c_driver) != 0) {
		TPD_DMESG("Error unable to add i2c driver.\n");
		return -1;
	}

	if (tpd_load_status == 0) {
		TPD_DMESG("Synaptics add error touch panel driver.\n");
		i2c_del_driver(&tpd_i2c_driver);
		return -1;
	}
	if (tpd_dts_data.use_tpd_button) {
		/*initialize tpd button data*/
		tpd_button_setting(tpd_dts_data.tpd_key_num,
		tpd_dts_data.tpd_key_local,
		tpd_dts_data.tpd_key_dim_local);
	}

#if (defined(TPD_WARP_START) && defined(TPD_WARP_END))
	TPD_DO_WARP = 1;
	memcpy(tpd_wb_start, tpd_wb_start_local, TPD_WARP_CNT * 4);
	memcpy(tpd_wb_end, tpd_wb_start_local, TPD_WARP_CNT * 4);
#endif

#if (defined(TPD_HAVE_CALIBRATION) && !defined(TPD_CUSTOM_CALIBRATION))
	memcpy(tpd_calmat, tpd_def_calmat_local, 8 * 4);
	memcpy(tpd_def_calmat, tpd_def_calmat_local, 8 * 4);
#endif

#ifdef CONFIG_MTK_BOOT
	boot_mode = get_boot_mode();
	/* if (boot_mode == 3) boot_mode = NORMAL_BOOT; */
#endif
	input_set_abs_params(tpd->dev, ABS_MT_TRACKING_ID,
	0, (10 - 1), 0, 0);
	TPD_DEBUG("end %s, %d\n", __func__, __LINE__);
	tpd_type_cap = 1;
	return 0;
}

static void tpd_resume(struct device *h)
{
	/* u16 tp_x_for_lcd=0; */
	/* u16 tp_y_for_lcd=0; */
	u8 data;
	int reset_count = 5;

	int retval;

 TPD_RESET_RESUME:
	retval = regulator_enable(tpd->reg);
	if (retval != 0)
		TPD_DMESG("Failed to enable reg-vgp6: %d\n", retval);

	retval = regulator_enable(tpd->io_reg);
	if (retval != 0)
		TPD_DMESG("Failed to enable reg-vgp4: %d\n", retval);
	/* hwPowerOn(MT6323_POWER_LDO_VGP2,  VOL_1800, "TP"); */
	msleep(20);

	/*tpd_gpio_output(GTP_RST_PORT, 0);*/
	gpio_direction_output(tpd_rst_gpio_number, 0);
	msleep(50);
	/*tpd_gpio_output(GTP_RST_PORT, 1);*/
	gpio_direction_output(tpd_rst_gpio_number, 1);
	msleep(50);
	/* Recovery EINT Mode */
	/*tpd_gpio_as_int(GTP_INT_PORT);*/
	gpio_direction_input(tpd_int_gpio_number);

	if ((tpd_i2c_read_data(ts->client, 0xEE, &data, 1)) < 0) {
		if (reset_count-- > 0)
			goto TPD_RESET_RESUME;
		TPD_DMESG("Can't connect touch panel.\n");
	}

	tpd_clear_interrupt(ts->client);
	mutex_lock(&i2c_access);
	tpd_halt = 0;

	enable_irq(touch_irq);

	mutex_unlock(&i2c_access);
}

static void tpd_suspend(struct device *h)
{
	int retval;

	mutex_lock(&i2c_access);

	disable_irq(touch_irq);

	tpd_halt = 1;
	mutex_unlock(&i2c_access);

	/* Set EINT PIN to low */
	/*tpd_gpio_output(GTP_INT_PORT, 0);*/
	gpio_direction_output(tpd_int_gpio_number, 0);
	/* Set RST PIN to low */
	/*tpd_gpio_output(GTP_RST_PORT, 0);*/
	gpio_direction_output(tpd_rst_gpio_number, 0);

	retval = regulator_disable(tpd->io_reg);
	if (retval != 0)
		TPD_DMESG("Failed to disable reg-vgp4: %d\n", retval);

	retval = regulator_disable(tpd->reg);
	if (retval != 0)
		TPD_DMESG("Failed to disable reg-vgp6: %d\n", retval);

	TPD_DEBUG("TPD enter sleep\n");
}


static struct tpd_driver_t tpd_device_driver = {
	.tpd_device_name = "synaptics",
	.tpd_local_init = tpd_local_init,
	.suspend = tpd_suspend,
	.resume = tpd_resume,
};

static int __init tpd_driver_init(void)
{
	TPD_DEBUG("Synaptics touch panel driver init\n");

	tpd_get_dts_info();
	if (tpd_driver_add(&tpd_device_driver) < 0)
		TPD_DMESG("Error Add Synaptics driver failed\n");
	return 0;
}

static void __exit tpd_driver_exit(void)
{
	TPD_DEBUG("Synaptics touch panel driver exit\n");
	tpd_driver_remove(&tpd_device_driver);
}
module_init(tpd_driver_init);
module_exit(tpd_driver_exit);

MODULE_DESCRIPTION("Mediatek Synaptics Driver");
