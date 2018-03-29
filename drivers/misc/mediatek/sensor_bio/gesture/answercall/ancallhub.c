/* ancallhub motion sensor driver
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
#include "ancallhub.h"
#include <gesture.h>
#include <hwmsen_helper.h>

#include <SCP_sensorHub.h>
#include <linux/notifier.h>
#include "scp_helper.h"


#define ANCALLHUB_TAG                  "[ancallhub] "
#define ANCALLHUB_FUN(f)               printk(ANCALLHUB_TAG"%s\n", __func__)
#define ANCALLHUB_ERR(fmt, args...)    printk(ANCALLHUB_TAG"%s %d : "fmt, __func__, __LINE__, ##args)
#define ANCALLHUB_LOG(fmt, args...)    printk(ANCALLHUB_TAG fmt, ##args)

typedef enum {
	ANCALLHUBH_TRC_INFO = 0X10,
} ANCALLHUB_TRC;

static struct ges_init_info ancallhub_init_info;

static int answer_call_get_data(int *probability, int *status)
{
	int err = 0;
	struct data_unit_t data;
	uint64_t time_stamp = 0;
	uint64_t time_stamp_gpt = 0;

	err = sensor_get_data_from_hub(ID_ANSWER_CALL, &data);
	if (err < 0) {
		ANCALLHUB_ERR("sensor_get_data_from_hub fail!!\n");
		return -1;
	}
	time_stamp		= data.time_stamp;
	time_stamp_gpt	= data.time_stamp_gpt;
	*probability	= data.gesture_data_t.probability;
	ANCALLHUB_LOG("recv ipi: timestamp: %lld, timestamp_gpt: %lld, probability: %d!\n", time_stamp, time_stamp_gpt,
		*probability);
	return 0;
}
static int answer_call_open_report_data(int open)
{
	int ret = 0;

#if defined CONFIG_MTK_SCP_SENSORHUB_V1
	if (open == 1)
		ret = sensor_set_delay_to_hub(ID_ANSWER_CALL, 66);
#elif defined CONFIG_NANOHUB

#else

#endif
	pr_warn("%s : type=%d, open=%d\n", __func__, ID_ANSWER_CALL, open);
	ret = sensor_enable_to_hub(ID_ANSWER_CALL, open);
	return ret;
}
static int answer_call_gesture_batch(int flag, int64_t samplingPeriodNs, int64_t maxBatchReportLatencyNs)
{
	return sensor_batch_to_hub(ID_ANSWER_CALL, flag, samplingPeriodNs, maxBatchReportLatencyNs);
}
static int answer_call_recv_data(struct data_unit_t *event, void *reserved)
{
	if (event->flush_action == FLUSH_ACTION)
		ANCALLHUB_ERR("answer_call do not support flush\n");
	else if (event->flush_action == DATA_ACTION)
		ges_notify(ID_ANSWER_CALL);
	return 0;
}

static int ancallhub_local_init(void)
{
	struct ges_control_path ctl = {0};
	struct ges_data_path data = {0};
	int err = 0;

	ctl.open_report_data = answer_call_open_report_data;
	ctl.batch = answer_call_gesture_batch;
	err = ges_register_control_path(&ctl, ID_ANSWER_CALL);
	if (err) {
		ANCALLHUB_ERR("register answer_call control path err\n");
		goto exit;
	}

	data.get_data = answer_call_get_data;
	err = ges_register_data_path(&data, ID_ANSWER_CALL);
	if (err) {
		ANCALLHUB_ERR("register answer_call data path err\n");
		goto exit;
	}
	err = SCP_sensorHub_data_registration(ID_ANSWER_CALL, answer_call_recv_data);
	if (err) {
		ANCALLHUB_ERR("SCP_sensorHub_data_registration fail!!\n");
		goto exit_create_attr_failed;
	}
	return 0;
exit:
exit_create_attr_failed:
	return -1;
}
static int ancallhub_local_uninit(void)
{
	return 0;
}

static struct ges_init_info ancallhub_init_info = {
	.name = "answer_call_hub",
	.init = ancallhub_local_init,
	.uninit = ancallhub_local_uninit,
};

static int __init ancallhub_init(void)
{
	ges_driver_add(&ancallhub_init_info, ID_ANSWER_CALL);
	return 0;
}

static void __exit ancallhub_exit(void)
{
	ANCALLHUB_FUN();
}

module_init(ancallhub_init);
module_exit(ancallhub_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("ANSWER_CALL_HUB driver");
MODULE_AUTHOR("hongxu.zhao@mediatek.com");
