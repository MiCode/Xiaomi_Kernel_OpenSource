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
#include "saralgo_top.h"
#include "situation.h"
#include <hwmsen_helper.h>
#include <SCP_sensorHub.h>
#include <linux/notifier.h>
#include "include/scp.h"
static int sar_algo_get_data(int *probability, int *status)
{
	int err = 0;
	struct data_unit_t data;
	uint64_t time_stamp = 0;
	err = sensor_get_data_from_hub(ID_SAR_ALGO_TOP, &data);
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
static int sar_algo_open_report_data(int open)
{
	int ret = 0;
	pr_err("%s : enable=%d HTP\n", __func__, open);
#if defined CONFIG_MTK_SCP_SENSORHUB_V1
	if (open == 1)
		ret = sensor_set_delay_to_hub(ID_SAR_ALGO_TOP, 120);
#elif defined CONFIG_NANOHUB
#else
#endif
	ret = sensor_enable_to_hub(ID_SAR_ALGO_TOP, open);
	return ret;
}
static int sar_algo_batch(int flag, int64_t samplingPeriodNs,
		int64_t maxBatchReportLatencyNs)
{
	pr_err("%s : flag=%d HTP\n", __func__, flag);
	return sensor_batch_to_hub(ID_SAR_ALGO_TOP, flag, samplingPeriodNs,
			maxBatchReportLatencyNs);
}

static int sar_algo_top_flush(void)
{
	return sensor_flush_to_hub(ID_SAR_ALGO_TOP);
}

static int sar_algo_recv_data(struct data_unit_t *event, void *reserved)
{
	int err = 0;
	if (event->flush_action == FLUSH_ACTION)
		err = situation_flush_report(ID_SAR_ALGO_TOP);
	else if (event->flush_action == DATA_ACTION)
		err = situation_data_report_t(ID_SAR_ALGO_TOP, (uint32_t)event->data[0], (int64_t)event->time_stamp);
	return err;
}
static int sar_algo_local_init(void)
{
	struct situation_control_path ctl = {0};
	struct situation_data_path data = {0};
	int err = 0;
	pr_err("%s : sar_algo init HTP\n", __func__);
	ctl.open_report_data = sar_algo_open_report_data;
	ctl.batch = sar_algo_batch;
	ctl.flush = sar_algo_top_flush;
	ctl.is_support_wake_lock = true;
	err = situation_register_control_path(&ctl, ID_SAR_ALGO_TOP);
	if (err) {
		pr_err("register sar_algo control path err\n");
		goto exit;
	}
	data.get_data = sar_algo_get_data;
	err = situation_register_data_path(&data, ID_SAR_ALGO_TOP);
	if (err) {
		pr_err("register sar_algo data path err\n");
		goto exit;
	}
	err = scp_sensorHub_data_registration(ID_SAR_ALGO_TOP, sar_algo_recv_data);
	if (err) {
		pr_err("SCP_sensorHub_data_registration fail!!\n");
		goto exit_create_attr_failed;
	}
	return 0;
exit:
exit_create_attr_failed:
	return -1;
}
static int sar_algo_local_uninit(void)
{
	return 0;
}
static struct situation_init_info sar_algo_init_info = {
	.name = "sar_algo_hub",
	.init = sar_algo_local_init,
	.uninit = sar_algo_local_uninit,
};
static int __init sar_algo_init(void)
{
	situation_driver_add(&sar_algo_init_info, ID_SAR_ALGO_TOP);
	return 0;
}
static void __exit sar_algo_exit(void)
{
	pr_info("sar_algo exit\n");
}
module_init(sar_algo_init);
module_exit(sar_algo_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("SAR_ALGO_HUB driver");
MODULE_AUTHOR("huitianpu@huaqin.com");
