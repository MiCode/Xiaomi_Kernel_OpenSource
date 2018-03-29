/* inpocket motion sensor driver
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
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/kobject.h>
#include <linux/platform_device.h>
#include <linux/atomic.h>

#include <hwmsensor.h>
#include <hwmsen_dev.h>
#include <sensors_io.h>
#include "inpocket.h"
#include "gesture.h"

#include <hwmsen_helper.h>

#include <batch.h>
#include <SCP_sensorHub.h>
#include <linux/notifier.h>
#include "scp_helper.h"


#define INPOCKET_TAG                  "[inpocket] "
#define INPOCKET_FUN(f)               pr_warn(INPOCKET_TAG"%s\n", __func__)
#define INPOCKET_ERR(fmt, args...)    pr_err(INPOCKET_TAG"%s %d : "fmt, __func__, __LINE__, ##args)
#define INPOCKET_LOG(fmt, args...)    pr_warn(INPOCKET_TAG fmt, ##args)


static struct ges_init_info inpocket_init_info;

struct inpk_ipi_data {
	atomic_t trace;
	atomic_t suspend;
	struct work_struct	 inpk_work;
};

static struct inpk_ipi_data obj_ipi_data;
static void inpk_work(struct work_struct *work)
{
	INPOCKET_FUN();
	ges_notify(ID_IN_POCKET);
}

static int inpocket_get_data(int *probability, int *status)
{
	int err = 0;
	struct data_unit_t data;
	uint64_t time_stamp = 0;
	uint64_t time_stamp_gpt = 0;

	err = sensor_get_data_from_hub(ID_IN_POCKET, &data);
	if (err < 0) {
		INPOCKET_ERR("sensor_get_data_from_hub fail!!\n");
		return -1;
	}
	time_stamp		= data.time_stamp;
	time_stamp_gpt	= data.time_stamp_gpt;
	*probability	= data.gesture_data_t.probability;
	INPOCKET_LOG("recv ipi: timestamp: %lld, timestamp_gpt: %lld, probability: %d!\n", time_stamp, time_stamp_gpt,
		*probability);
	return 0;
}
static int inpocket_open_report_data(int open)
{
	int ret = 0;

	if (open == 1)
		ret = sensor_set_delay_to_hub(ID_IN_POCKET, 60);
	INPOCKET_LOG("%s : type=%d, open=%d\n", __func__, ID_IN_POCKET, open);
	ret = sensor_enable_to_hub(ID_IN_POCKET, open);
	return ret;
}
static int SCP_sensorHub_notify_handler(void *data, unsigned int len)
{
	SCP_SENSOR_HUB_DATA_P rsp = (SCP_SENSOR_HUB_DATA_P)data;
	struct inpk_ipi_data *obj = &obj_ipi_data;

	if (SENSOR_HUB_NOTIFY == rsp->rsp.action) {
		INPOCKET_LOG("sensorId = %d\n", rsp->notify_rsp.sensorType);
		switch (rsp->notify_rsp.event) {
		case SCP_NOTIFY:
		    if (ID_IN_POCKET == rsp->notify_rsp.sensorType)
			schedule_work(&(obj->inpk_work));
			break;
		default:
		    INPOCKET_ERR("Error sensor hub notify");
			break;
	    }
	} else
		INPOCKET_ERR("Error sensor hub action");

	return 0;
}

static int inpocket_local_init(void)
{
	struct ges_control_path ctl = {0};
	struct ges_data_path data = {0};
	struct inpk_ipi_data *obj = &obj_ipi_data;
	int err = 0;

	INPOCKET_FUN();

	ctl.open_report_data = inpocket_open_report_data;
	err = ges_register_control_path(&ctl, ID_IN_POCKET);
	if (err) {
		INPOCKET_ERR("register in_pocket control path err\n");
		goto exit;
	}

	data.get_data = inpocket_get_data;
	err = ges_register_data_path(&data, ID_IN_POCKET);
	if (err) {
		INPOCKET_ERR("register in_pocket data path err\n");
		goto exit;
	}
	INIT_WORK(&obj->inpk_work, inpk_work);
	err = SCP_sensorHub_rsp_registration(ID_IN_POCKET, SCP_sensorHub_notify_handler);
	if (err) {
		INPOCKET_ERR("SCP_sensorHub_rsp_registration fail!!\n");
		goto exit_create_attr_failed;
	}
	return 0;
exit:
exit_create_attr_failed:
	return -1;
}
static int inpocket_local_uninit(void)
{
	return 0;
}

static struct ges_init_info inpocket_init_info = {
	.name = "in_pocket_hub",
	.init = inpocket_local_init,
	.uninit = inpocket_local_uninit,
};

static int __init inpocket_init(void)
{
	ges_driver_add(&inpocket_init_info, ID_IN_POCKET);
	return 0;
}

static void __exit inpocket_exit(void)
{
	INPOCKET_FUN();
}

module_init(inpocket_init);
module_exit(inpocket_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("InPocket driver");
MODULE_AUTHOR("qiangming.xia@mediatek.com");
