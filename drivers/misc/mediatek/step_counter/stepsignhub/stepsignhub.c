/* step_chub motion sensor driver
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
#include "stepsignhub.h"
#include <step_counter.h>
#include <SCP_sensorHub.h>
#include <linux/notifier.h>
#include "scp_helper.h"



#define STEP_CDS_TAG                  "[stepsignhub] "
#define STEP_CDS_FUN(f)               pr_err(STEP_CDS_TAG"%s\n", __func__)
#define STEP_CDS_ERR(fmt, args...)    pr_err(STEP_CDS_TAG"%s %d : "fmt, __func__, __LINE__, ##args)
#define STEP_CDS_LOG(fmt, args...)    pr_err(STEP_CDS_TAG fmt, ##args)

typedef enum {
	STEP_CDSH_TRC_INFO = 0X10,
} STEP_CDS_TRC;

static struct step_c_init_info step_cdshub_init_info;

struct step_chub_ipi_data {
	atomic_t trace;
	atomic_t suspend;
	struct work_struct step_d_work;
	struct work_struct step_s_work;
};

static struct step_chub_ipi_data obj_ipi_data;
static void step_detect_work(struct work_struct *work)
{
	STEP_CDS_FUN();
	step_notify(TYPE_STEP_DETECTOR);
}

static void step_sign_work(struct work_struct *work)
{
	STEP_CDS_FUN();
	step_notify(TYPE_SIGNIFICANT);
}

static ssize_t show_step_cds_value(struct device_driver *ddri, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", buf);
}

static ssize_t store_trace_value(struct device_driver *ddri, const char *buf, size_t count)
{
	struct step_chub_ipi_data *obj = &obj_ipi_data;
	int trace = 0;

	if (obj == NULL) {
		STEP_CDS_ERR("obj is null!!\n");
		return 0;
	}

	if (1 == sscanf(buf, "0x%x", &trace)) {
		atomic_set(&obj->trace, trace);
	} else {
		STEP_CDS_ERR("invalid content: '%s', length = %zu\n", buf, count);
		return 0;
	}
	return count;
}

static DRIVER_ATTR(step_cds, S_IRUGO, show_step_cds_value, NULL);
static DRIVER_ATTR(trace, S_IWUSR | S_IRUGO, NULL, store_trace_value);

static struct driver_attribute *step_chub_attr_list[] = {
	&driver_attr_step_cds,
	&driver_attr_trace,
};

static int step_chub_create_attr(struct device_driver *driver)
{
	int idx, err = 0;
	int num = (int)(sizeof(step_chub_attr_list) / sizeof(step_chub_attr_list[0]));


	if (driver == NULL)
		return -EINVAL;

	for (idx = 0; idx < num; idx++) {
		err = driver_create_file(driver, step_chub_attr_list[idx]);
		if (0 != err) {
			STEP_CDS_ERR("driver_create_file (%s) = %d\n",
				     step_chub_attr_list[idx]->attr.name, err);
			break;
		}
	}
	return err;
}

static int step_chub_delete_attr(struct device_driver *driver)
{
	int idx, err = 0;
	int num = (int)(sizeof(step_chub_attr_list) / sizeof(step_chub_attr_list[0]));

	if (driver == NULL)
		return -EINVAL;

	for (idx = 0; idx < num; idx++)
		driver_remove_file(driver, step_chub_attr_list[idx]);

	return err;
}

static int step_c_enable_nodata(int en)
{
	int ret = 0;

	/* if (en == 1)
		ret = sensor_set_delay_to_hub(ID_STEP_COUNTER, 66); */
	ret = sensor_enable_to_hub(ID_STEP_COUNTER, en);
	return ret;
}

static int step_d_enable_nodata(int en)
{
	int ret = 0;

	/* if (en == 1)
		ret = sensor_set_delay_to_hub(ID_STEP_DETECTOR, 66); */
	ret = sensor_enable_to_hub(ID_STEP_DETECTOR, en);
	return ret;
}

static int step_s_enable_nodata(int en)
{
	int ret = 0;

	if (en == 1)
		ret = sensor_set_delay_to_hub(ID_SIGNIFICANT_MOTION, 66);
	ret = sensor_enable_to_hub(ID_SIGNIFICANT_MOTION, en);
	return ret;
}

static int step_c_set_delay(u64 delay)
{
	unsigned int delayms = 0;

	delayms = delay / 1000 / 1000;
	return sensor_set_delay_to_hub(ID_STEP_COUNTER, delayms);
}

static int step_d_set_delay(u64 delay)
{
	unsigned int delayms = 0;

	delayms = delay / 1000 / 1000;
	return sensor_set_delay_to_hub(ID_STEP_DETECTOR, delayms);
}
static int step_counter_get_data(uint32_t *counter, int *status)
{
	int err = 0;
	struct data_unit_t data;
	uint64_t time_stamp = 0;
	uint64_t time_stamp_gpt = 0;

	err = sensor_get_data_from_hub(ID_STEP_COUNTER, &data);
	if (err < 0) {
		STEP_CDS_ERR("sensor_get_data_from_hub fail!!\n");
		return -1;
	}
	time_stamp = data.time_stamp;
	time_stamp_gpt = data.time_stamp_gpt;
	*counter = data.step_counter_t.accumulated_step_count;
	STEP_CDS_LOG("recv ipi: timestamp: %lld, timestamp_gpt: %lld, counter: %d!\n", time_stamp,
		     time_stamp_gpt, *counter);
	return 0;
}
static int step_detector_get_data(uint32_t *counter, int *status)
{
	return 0;
}
static int significant_get_data(uint32_t *counter, int *status)
{
	return 0;
}

static int step_cds_open_report_data(int open)
{
	return 0;
}

static int SCP_sensorHub_detect_notify_handler(void *data, unsigned int len)
{
	SCP_SENSOR_HUB_DATA_P rsp = (SCP_SENSOR_HUB_DATA_P) data;
	struct step_chub_ipi_data *obj = &obj_ipi_data;

	switch (rsp->rsp.action) {
	case SENSOR_HUB_NOTIFY:
		switch (rsp->notify_rsp.event) {
		case SCP_NOTIFY:
				schedule_work(&(obj->step_d_work));
			break;
		default:
			STEP_CDS_ERR("Error sensor hub notify");
			break;
		}
		break;
	default:
		STEP_CDS_ERR("Error sensor hub action");
		break;
	}

	return 0;
}

static int SCP_sensorHub_sign_notify_handler(void *data, unsigned int len)
{
	SCP_SENSOR_HUB_DATA_P rsp = (SCP_SENSOR_HUB_DATA_P) data;
	struct step_chub_ipi_data *obj = &obj_ipi_data;

	switch (rsp->rsp.action) {
	case SENSOR_HUB_NOTIFY:
		switch (rsp->notify_rsp.event) {
		case SCP_NOTIFY:
				schedule_work(&(obj->step_s_work));
			break;
		default:
			STEP_CDS_ERR("Error sensor hub notify");
			break;
		}
		break;
	default:
		STEP_CDS_ERR("Error sensor hub action");
		break;
	}

	return 0;
}

static int step_chub_local_init(void)
{
	struct step_c_control_path ctl = { 0 };
	struct step_c_data_path data = { 0 };
	struct step_chub_ipi_data *obj = &obj_ipi_data;
	int err = 0;

	err = step_chub_create_attr(&step_cdshub_init_info.platform_diver_addr->driver);
	if (err) {
		STEP_CDS_ERR("create attribute err = %d\n", err);
		goto exit_create_attr_failed;
	}
	ctl.open_report_data = step_cds_open_report_data;
	ctl.enable_nodata = step_c_enable_nodata;
	ctl.enable_step_detect = step_d_enable_nodata;
	ctl.enable_significant = step_s_enable_nodata;
	ctl.step_c_set_delay = step_c_set_delay;
	ctl.step_d_set_delay = step_d_set_delay;
	ctl.is_report_input_direct = false;
	ctl.is_support_batch = false;
	err = step_c_register_control_path(&ctl);
	if (err) {
		STEP_CDS_ERR("register step_cds control path err\n");
		goto exit;
	}

	data.get_data = step_counter_get_data;
	data.get_data_step_d = step_detector_get_data;
	data.get_data_significant = significant_get_data;
	err = step_c_register_data_path(&data);
	if (err) {
		STEP_CDS_ERR("register step_cds data path err\n");
		goto exit;
	}
	INIT_WORK(&obj->step_d_work, step_detect_work);
	INIT_WORK(&obj->step_s_work, step_sign_work);
	err = SCP_sensorHub_rsp_registration(ID_STEP_DETECTOR, SCP_sensorHub_detect_notify_handler);
	if (err) {
		STEP_CDS_ERR("SCP_sensorHub_rsp_registration fail!!\n");
		goto exit_create_attr_failed;
	}
	err =
	    SCP_sensorHub_rsp_registration(ID_SIGNIFICANT_MOTION,
					   SCP_sensorHub_sign_notify_handler);
	if (err) {
		STEP_CDS_ERR("SCP_sensorHub_rsp_registration fail!!\n");
		goto exit_create_attr_failed;
	}
	err = batch_register_support_info(ID_STEP_DETECTOR, ctl.is_support_batch, 1, 1);
	if (err) {
		STEP_CDS_ERR("register hmdy batch support err = %d\n", err);
		goto exit_create_attr_failed;
	}
	err = batch_register_support_info(ID_STEP_COUNTER, ctl.is_support_batch, 1, 1);
	if (err) {
		STEP_CDS_ERR("register hmdy batch support err = %d\n", err);
		goto exit_create_attr_failed;
	}

	return 0;
exit:
	step_chub_delete_attr(&(step_cdshub_init_info.platform_diver_addr->driver));
exit_create_attr_failed:
	return -1;
}

static int step_chub_local_uninit(void)
{
	return 0;
}

static struct step_c_init_info step_cdshub_init_info = {
	.name = "step_cds_hub",
	.init = step_chub_local_init,
	.uninit = step_chub_local_uninit,
};

static int __init step_chub_init(void)
{
	step_c_driver_add(&step_cdshub_init_info);
	return 0;
}

static void __exit step_chub_exit(void)
{
	STEP_CDS_FUN();
}

module_init(step_chub_init);
module_exit(step_chub_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("GLANCE_GESTURE_HUB driver");
MODULE_AUTHOR("hongxu.zhao@mediatek.com");
