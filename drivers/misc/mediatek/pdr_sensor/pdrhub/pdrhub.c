/* pdrhub motion sensor driver
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/miscdevice.h>
#include <asm/uaccess.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/kobject.h>

#include <linux/platform_device.h>
#include <asm/atomic.h>

#include <hwmsensor.h>
#include <hwmsen_dev.h>
#include <sensors_io.h>
#include <pdr_sensor.h>
#include "pdrhub.h"
#include <hwmsen_helper.h>

#include <batch.h>
#include <SCP_sensorHub.h>
#include <linux/notifier.h>
#include "scp_helper.h"



#define PDRHUB_TAG                  "[pdrhub] "
#define PDRHUB_FUN(f)               pr_err(PDRHUB_TAG"%s\n", __func__)
#define PDRHUB_ERR(fmt, args...)    pr_err(PDRHUB_TAG"%s %d : "fmt, __func__, __LINE__, ##args)
#define PDRHUB_LOG(fmt, args...)    pr_err(PDRHUB_TAG fmt, ##args)

#define PDR_AXIS_X                  0
#define PDR_AXIS_Y                  1
#define PDR_AXIS_Z                  2
#define PDR_AXIS_NUM                3


typedef enum {
	PDRHUB_TRC_INFO = 0X10,
} PDRHHUB_TRC;

static struct pdr_init_info pdrhub_init_info;

struct pdrhub_ipi_data {
	atomic_t trace;
	atomic_t suspend;
};

static struct pdrhub_ipi_data obj_ipi_data;

static ssize_t show_pdrsensor_value(struct device_driver *ddri, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", buf);
}

static ssize_t store_trace_value(struct device_driver *ddri, const char *buf, size_t count)
{
	struct pdrhub_ipi_data *obj = &obj_ipi_data;
	int trace = 0;

	if (obj == NULL) {
		PDRHUB_ERR("obj is null!!\n");
		return 0;
	}

	if (1 == sscanf(buf, "0x%x", &trace)) {
		atomic_set(&obj->trace, trace);
	} else {
		PDRHUB_ERR("invalid content: '%s', length = %zu\n", buf, count);
		return 0;
	}
	return count;
}

static DRIVER_ATTR(pdrsensor, S_IRUGO, show_pdrsensor_value, NULL);
static DRIVER_ATTR(trace, S_IWUSR | S_IRUGO, NULL, store_trace_value);

static struct driver_attribute *pdrhub_attr_list[] = {
	&driver_attr_pdrsensor,
	&driver_attr_trace,
};

static int pdrhub_create_attr(struct device_driver *driver)
{
	int idx = 0, err = 0;
	int num = (int)(sizeof(pdrhub_attr_list) / sizeof(pdrhub_attr_list[0]));


	if (driver == NULL)
		return -EINVAL;

	for (idx = 0; idx < num; idx++) {
		err = driver_create_file(driver, pdrhub_attr_list[idx]);
		if (0 != err) {
			PDRHUB_ERR("driver_create_file (%s) = %d\n",
				    pdrhub_attr_list[idx]->attr.name, err);
			break;
		}
	}
	return err;
}

static int pdrhub_delete_attr(struct device_driver *driver)
{
	int idx = 0, err = 0;
	int num = (int)(sizeof(pdrhub_attr_list) / sizeof(pdrhub_attr_list[0]));

	if (driver == NULL)
		return -EINVAL;

	for (idx = 0; idx < num; idx++)
		driver_remove_file(driver, pdrhub_attr_list[idx]);

	return err;
}

static int pdrsensor_get_data(uint32_t *sensor_value, int *status)
{
	int err = 0;
	uint64_t time_stamp = 0;
	uint64_t time_stamp_gpt = 0;
	struct data_unit_t data = {0};

	err = sensor_get_data_from_hub(ID_PDR, &data);
	if (err < 0) {
		PDRHUB_ERR("sensor_get_data_from_hub fail!!\n");
		return -1;
	}
	time_stamp = data.time_stamp;
	time_stamp_gpt = data.time_stamp_gpt;
	sensor_value[PDR_AXIS_X] = data.pdr_event.x;/*x axis value*/
	sensor_value[PDR_AXIS_Y] = data.pdr_event.y;/*y axis value*/
	sensor_value[PDR_AXIS_Z] = data.pdr_event.z;/*z axis value*/
	*status = data.pdr_event.status;/*status*/
	PDRHUB_LOG
	    ("recv ipi: timestamp: %lld, timestamp_gpt: %lld, x: %d, y: %d, z: %d, status: %d!\n",
	     time_stamp, time_stamp_gpt, sensor_value[0], sensor_value[1], sensor_value[2], *status);
	return err;
}

static int pdrsensor_open_report_data(int open)
{
	return 0;
}

static int pdrsensor_enable_nodata(int en)
{
	/*PDRHUB_ERR("pdrsensor_enable_nodata:%d\n", en);*/
	return sensor_enable_to_hub(ID_PDR, en);
}

static int pdrsensor_set_delay(u64 delay)
{
	unsigned int delayms = 0;

	delayms = delay / 1000 / 1000;
	return sensor_set_delay_to_hub(ID_PDR, delayms);
}

static int pdrhub_local_init(void)
{
	int err = 0;
	struct pdr_control_path ctl = { 0 };
	struct pdr_data_path data = { 0 };

	err = pdrhub_create_attr(&pdrhub_init_info.platform_diver_addr->driver);
	if (err) {
		PDRHUB_ERR("create attribute err = %d\n", err);
		goto exit_create_attr_failed;
	}
	ctl.open_report_data = pdrsensor_open_report_data;
	ctl.enable_nodata = pdrsensor_enable_nodata;
	ctl.set_delay = pdrsensor_set_delay;
	ctl.is_report_input_direct = false;
	ctl.is_support_batch = true;
	err = pdr_register_control_path(&ctl);
	if (err) {
		PDRHUB_ERR("register pdrsensor control path err\n");
		goto exit;
	}

	data.get_data = pdrsensor_get_data;
	err = pdr_register_data_path(&data);
	if (err) {
		PDRHUB_ERR("register pdrsensor data path err\n");
		goto exit;
	}
	return 0;
exit:
	pdrhub_delete_attr(&(pdrhub_init_info.platform_diver_addr->driver));
exit_create_attr_failed:
	return -1;
}

static int pdrhub_local_uninit(void)
{
	return 0;
}

static struct pdr_init_info pdrhub_init_info = {
	.name = "pdr_hub",
	.init = pdrhub_local_init,
	.uninit = pdrhub_local_uninit,
};

static int __init pdrhub_init(void)
{
	pdr_driver_add(&pdrhub_init_info);
	return 0;
}

static void __exit pdrhub_exit(void)
{
	PDRHUB_FUN();
}

module_init(pdrhub_init);
module_exit(pdrhub_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("PDRHUB driver");
MODULE_AUTHOR("xuexi.bai@mediatek.com");
