/* tiltdetecthub motion sensor driver
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

#include <hwmsensor.h>
#include "tiltdetecthub.h"
#include <tilt_detector.h>
#include <SCP_sensorHub.h>
#include <linux/notifier.h>
#include "scp_helper.h"


#define TILTDETHUB_TAG                  "[tiltdetecthub] "
#define TILTDETHUB_FUN(f)               printk(TILTDETHUB_TAG"%s\n", __func__)
#define TILTDETHUB_ERR(fmt, args...)    printk(TILTDETHUB_TAG"%s %d : "fmt, __func__, __LINE__, ##args)
#define TILTDETHUB_LOG(fmt, args...)    printk(TILTDETHUB_TAG fmt, ##args)

typedef enum {
	TILTDETHUB_TRC_INFO = 0X10,
} TILTDETHUB_TRC;

static struct tilt_init_info tiltdetecthub_init_info;

struct tiltdetecthub_ipi_data {
	atomic_t trace;
	atomic_t suspend;
};

static struct tiltdetecthub_ipi_data obj_ipi_data;
static ssize_t show_tilt_detect_value(struct device_driver *ddri, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", buf);
}
static ssize_t store_trace_value(struct device_driver *ddri, const char *buf, size_t count)
{
	struct tiltdetecthub_ipi_data *obj = &obj_ipi_data;
	int trace = 0;

	if (obj == NULL) {
		TILTDETHUB_ERR("obj is null!!\n");
		return 0;
	}

	if (1 == sscanf(buf, "0x%x", &trace)) {
		atomic_set(&obj->trace, trace);
	} else {
		TILTDETHUB_ERR("invalid content: '%s', length = %zu\n", buf, count);
		return 0;
	}
	return count;
}

static DRIVER_ATTR(tilt_detect, S_IRUGO, show_tilt_detect_value, NULL);
static DRIVER_ATTR(trace, S_IWUSR | S_IRUGO, NULL, store_trace_value);

static struct driver_attribute *tiltdetecthub_attr_list[] = {
	&driver_attr_tilt_detect,
	&driver_attr_trace,
};

static int tiltdetecthub_create_attr(struct device_driver *driver)
{
	int idx, err = 0;
	int num = (int)(sizeof(tiltdetecthub_attr_list) / sizeof(tiltdetecthub_attr_list[0]));


	if (driver == NULL)
		return -EINVAL;

	for (idx = 0; idx < num; idx++) {
		err = driver_create_file(driver, tiltdetecthub_attr_list[idx]);
		if (0 != err) {
			TILTDETHUB_ERR("driver_create_file (%s) = %d\n", tiltdetecthub_attr_list[idx]->attr.name, err);
			break;
		}
	}
	return err;
}

static int tiltdetecthub_delete_attr(struct device_driver *driver)
{
	int idx, err = 0;
	int num = (int)(sizeof(tiltdetecthub_attr_list) / sizeof(tiltdetecthub_attr_list[0]));

	if (driver == NULL)
		return -EINVAL;

	for (idx = 0; idx < num; idx++)
		driver_remove_file(driver, tiltdetecthub_attr_list[idx]);

	return err;
}

static int tilt_detect_get_data(int *probability, int *status)
{
	int err = 0;
	struct data_unit_t data;
	uint64_t time_stamp = 0;
	uint64_t time_stamp_gpt = 0;

	err = sensor_get_data_from_hub(ID_TILT_DETECTOR, &data);
	if (err < 0) {
		TILTDETHUB_ERR("sensor_get_data_from_hub fail!!\n");
		return -1;
	}
	time_stamp		= data.time_stamp;
	time_stamp_gpt	= data.time_stamp_gpt;
	*probability	= data.gesture_data_t.probability;
	TILTDETHUB_LOG("recv ipi: timestamp: %lld, timestamp_gpt: %lld, probability: %d!\n", time_stamp, time_stamp_gpt,
		*probability);
	return 0;
}
static int tilt_detect_open_report_data(int open)
{
	int ret = 0;

	ret = sensor_enable_to_hub(ID_TILT_DETECTOR, open);
	return ret;
}
static int tilt_detect_set_delay(uint64_t delay)
{
	unsigned int ret = 0;

#if defined CONFIG_MTK_SCP_SENSORHUB_V1
	unsigned int delayms = 0;

	delayms = delay / 1000 / 1000;
	ret = sensor_set_delay_to_hub(ID_TILT_DETECTOR, delayms);
#elif defined CONFIG_NANOHUB

#else

#endif
	return ret;
}
static int tilt_detect_batch(int flag, int64_t samplingPeriodNs, int64_t maxBatchReportLatencyNs)
{
	return sensor_batch_to_hub(ID_TILT_DETECTOR, flag, samplingPeriodNs, maxBatchReportLatencyNs);
}

static int tilt_detect_flush(void)
{
	return sensor_flush_to_hub(ID_TILT_DETECTOR);
}

static int tilt_detect_recv_data(struct data_unit_t *event, void *reserved)
{
	if (event->flush_action == FLUSH_ACTION)
		tilt_flush_report();
	else if (event->flush_action == DATA_ACTION)
		tilt_notify();
	return 0;
}

static int tiltdetecthub_local_init(void)
{
	struct tilt_control_path ctl = {0};
	struct tilt_data_path data = {0};
	int err = 0;

	err = tiltdetecthub_create_attr(&tiltdetecthub_init_info.platform_diver_addr->driver);
	if (err) {
		TILTDETHUB_ERR("create attribute err = %d\n", err);
		goto exit_create_attr_failed;
	}
	ctl.open_report_data = tilt_detect_open_report_data;
	ctl.set_delay = tilt_detect_set_delay;
	ctl.batch = tilt_detect_batch;
	ctl.flush = tilt_detect_flush;
	ctl.is_report_input_direct = false;
	ctl.is_support_batch = false;
	err = tilt_register_control_path(&ctl);
	if (err) {
		TILTDETHUB_ERR("register tilt_detect control path err\n");
		goto exit;
	}

	data.get_data = tilt_detect_get_data;
	err = tilt_register_data_path(&data);
	if (err) {
		TILTDETHUB_ERR("register tilt_detect data path err\n");
		goto exit;
	}
	err = SCP_sensorHub_data_registration(ID_TILT_DETECTOR, tilt_detect_recv_data);
	if (err) {
		TILTDETHUB_ERR("SCP_sensorHub_data_registration fail!!\n");
		goto exit_create_attr_failed;
	}
	return 0;
exit:
	tiltdetecthub_delete_attr(&(tiltdetecthub_init_info.platform_diver_addr->driver));
exit_create_attr_failed:
	return -1;
}
static int tiltdetecthub_local_uninit(void)
{
	return 0;
}

static struct tilt_init_info tiltdetecthub_init_info = {
	.name = "tilt_detect_hub",
	.init = tiltdetecthub_local_init,
	.uninit = tiltdetecthub_local_uninit,
};

static int __init tiltdetecthub_init(void)
{
	tilt_driver_add(&tiltdetecthub_init_info);
	return 0;
}

static void __exit tiltdetecthub_exit(void)
{
	TILTDETHUB_FUN();
}

module_init(tiltdetecthub_init);
module_exit(tiltdetecthub_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("GLANCE_GESTURE_HUB driver");
MODULE_AUTHOR("hongxu.zhao@mediatek.com");
