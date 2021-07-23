

/* elevator_detect sensor driver */


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
#include <sensors_io.h>
#include "elevator_detect.h"
#include "situation.h"

#include <hwmsen_helper.h>

#include <SCP_sensorHub.h>
#include <linux/notifier.h>
#include "scp_helper.h"


static int elevator_detect_get_data(int *probability, int *status)
{
	int err = 0;
	struct data_unit_t data;
	uint64_t time_stamp = 0;

	err = sensor_get_data_from_hub(ID_ELEVATOR_DETECT, &data);
	if (err < 0) {
		pr_err("sensor_get_data_from_hub fail!!\n");
		return -1;
	}
	time_stamp = data.time_stamp;
	*probability = data.gesture_data_t.probability;
	pr_err("recv ipi: timestamp: %lld, probability: %d!\n", time_stamp,
		*probability);
	return 0;
}

static int elevator_detect_open_report_data(int open)
{
	int ret = 0;

	pr_info("%s : enable=%d\n", __func__, open);
#if defined CONFIG_MTK_SCP_SENSORHUB_V1
	if (open == 1)
		ret = sensor_set_delay_to_hub(ID_ELEVATOR_DETECT, 120);
#elif defined CONFIG_NANOHUB

#else

#endif

	ret = sensor_enable_to_hub(ID_ELEVATOR_DETECT, open);
	return ret;
}

static int elevator_detect_batch(int flag, int64_t samplingPeriodNs,
		int64_t maxBatchReportLatencyNs)
{
	pr_info("%s : flag=%d\n", __func__, flag);

	return sensor_batch_to_hub(ID_ELEVATOR_DETECT, flag, samplingPeriodNs,
			maxBatchReportLatencyNs);
}

static int elevator_detect_recv_data(struct data_unit_t *event, void *reserved)
{
	int err = 0;
	int32_t value[1] = {0};

	if (event->flush_action == FLUSH_ACTION)
		pr_err("elevator_detect do not support flush\n");
	else if (event->flush_action == DATA_ACTION)
		//err = situation_notify_t(ID_ELEVATOR_DETECT , (int64_t)event->time_stamp);
		value[0] = event->elevator_data_t.status;
	err = elevator_data_report_t(value, (int64_t)event->time_stamp);
	return err;
}

static int elevator_detect_local_init(void)
{
	struct situation_control_path ctl = {0};
	struct situation_data_path data = {0};
	int err = 0;

	ctl.open_report_data = elevator_detect_open_report_data;
	ctl.batch = elevator_detect_batch;
	ctl.is_support_wake_lock = true;
	err = situation_register_control_path(&ctl, ID_ELEVATOR_DETECT);
	if (err) {
		pr_err("register elevator_detect control path err\n");
		goto exit;
	}

	data.get_data = elevator_detect_get_data;
	err = situation_register_data_path(&data, ID_ELEVATOR_DETECT);
	if (err) {
		pr_err("register ps_strm data path err\n");
		goto exit;
	}
	err = scp_sensorHub_data_registration(ID_ELEVATOR_DETECT, elevator_detect_recv_data);
	if (err) {
		pr_err("SCP_sensorHub_data_registration fail!!\n");
		goto exit_create_attr_failed;
	}
	return 0;
exit:
exit_create_attr_failed:
	return -1;
}
static int elevator_detect_local_uninit(void)
{
	return 0;
}

static struct situation_init_info elevator_detect_init_info = {
	.name = "elevator_detect_hub",
	.init = elevator_detect_local_init,
	.uninit = elevator_detect_local_uninit,
};

static int __init elevator_detect_init(void)
{
	situation_driver_add(&elevator_detect_init_info, ID_ELEVATOR_DETECT);
	return 0;
}

static void __exit elevator_detect_exit(void)
{
	pr_info("elevator_detect exit\n");
}

module_init(elevator_detect_init);
module_exit(elevator_detect_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("ELEVATOR_DETECT_HUB driver");
MODULE_AUTHOR("xxx");
