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
#include "saralgo.h"
#include "situation.h"
#include <hwmsen_helper.h>
#include <SCP_sensorHub.h>
#include <linux/notifier.h>
#include "include/scp.h"
/*N19a code for HQ-348009 by huweifeng at 2023/12/23 start*/
#include "saralgo_factory.h"
static uint32_t sardata;
struct saralgo_ipi_data {
	bool factory_enable;
	int32_t cali_data[3];
	int8_t cali_status;
};
static struct saralgo_ipi_data *obj_ipi_data;
static int saralgo_factory_enable_sensor(bool enabledisable,
					 int64_t sample_periods_ms)
{
	int err = 0;
	struct saralgo_ipi_data *obj = obj_ipi_data;
	if (enabledisable == true)
		WRITE_ONCE(obj->factory_enable, true);
	else
		WRITE_ONCE(obj->factory_enable, false);
	if (enabledisable == true) {
		err = sensor_set_delay_to_hub(ID_SAR_ALGO,
					      sample_periods_ms);
		if (err) {
			pr_err("sensor_set_delay_to_hub failed!\n");
			return -1;
		}
	}
	err = sensor_enable_to_hub(ID_SAR_ALGO, enabledisable);
	if (err) {
		pr_err("sensor_enable_to_hub failed!\n");
		return -1;
	}
	return 0;
}

static int saralgo_factory_get_data(int32_t sensor_data[3])
{
	int err = 0;
	struct data_unit_t data;
	err = sensor_get_data_from_hub(ID_SAR_ALGO, &data);
	if (err < 0) {
		pr_err_ratelimited("saralgo_get_data_from_hub fail!!\n");
		return -1;
	}
	sensor_data[0] = data.data[0];
	sensor_data[2] = data.time_stamp;
	err = sensor_get_data_from_hub(ID_SAR_ALGO_TOP, &data);
	if (err < 0) {
		pr_err_ratelimited("saralgo_get_data_from_hub fail!!\n");
		return -1;
	}
	sensor_data[1] = data.data[0];
	pr_err_ratelimited("saralgo_get_data %d   %d  \n",sensor_data[0],sensor_data[1]);
	return err;
}
static int saralgo_factory_set_step(int32_t* step_en)
{
	uint8_t count;
	int err =0;
	int32_t cfg_data[2];
	if (*step_en==1){
		cfg_data[0] = 80;
		cfg_data[1] = 1;
	}else{
		cfg_data[0] = 80;
		cfg_data[1] = 0;
	}
	count = sizeof(cfg_data);
	err = sensor_cfg_to_hub(ID_SAR_ALGO,(uint8_t *)cfg_data,count);
	if (err < 0){
	pr_err_ratelimited("saralgo_get_data_from_hub fail!!\n");
	return -1;
	}
	return err;
}
/*N19a code for HQ-348009 by huweifeng at 2023/12/23 end*/

static int sar_algo_get_data(int *probability, int *status)
{
	int err = 0;
	struct data_unit_t data;
	uint64_t time_stamp = 0;
	err = sensor_get_data_from_hub(ID_SAR_ALGO, &data);
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

static int saralgo_set_step(uint8_t *data, uint8_t count)
{
	int32_t *buf = (int32_t *)data;
	uint8_t count_size;
	int err =0;
	int32_t cfg_data[2];
	if (buf[1]==1){
		cfg_data[0] = 80;
		cfg_data[1] = 1;
	}else{
		cfg_data[0] = 80;
		cfg_data[1] = 0;
	}
	count_size = sizeof(cfg_data);
	err = sensor_cfg_to_hub(ID_SAR_ALGO,(uint8_t *)cfg_data,count_size);
	if (err < 0){
	pr_err_ratelimited("saralgo_get_data_from_hub fail!!\n");
	return -1;
	}
	return err;
}

static int sar_algo_open_report_data(int open)
{
	int ret = 0;
	pr_err("%s : enable=%d HTP\n", __func__, open);
#if defined CONFIG_MTK_SCP_SENSORHUB_V1
	if (open == 1)
		ret = sensor_set_delay_to_hub(ID_SAR_ALGO, 120);
#elif defined CONFIG_NANOHUB
#else
#endif
	ret = sensor_enable_to_hub(ID_SAR_ALGO, open);
	return ret;
}
static int sar_algo_batch(int flag, int64_t samplingPeriodNs,
		int64_t maxBatchReportLatencyNs)
{
	pr_err("%s : flag=%d HTP\n", __func__, flag);
	return sensor_batch_to_hub(ID_SAR_ALGO, flag, samplingPeriodNs,
			maxBatchReportLatencyNs);
}

/*N19a code for HQ-353506 by huweifeng at 2024/01/29 start*/
static int sar_algo_flush(void)
{
	return sensor_flush_to_hub(ID_SAR_ALGO);
}
/*N19a code for HQ-353506 by huweifeng at 2024/01/29 end*/

/*N19a code for HQ-348009 by huweifeng at 2023/12/23 start*/
static struct saralgo_factory_fops saralgo_factory_fops = {
	.enable_sensor = saralgo_factory_enable_sensor,
	.get_data = saralgo_factory_get_data,
	.step_set_cfg = saralgo_factory_set_step,
};
static struct saralgo_factory_public saralgo_factory_device = {
	.gain = 1,
	.sensitivity = 1,
	.fops = &saralgo_factory_fops,
};

static int sar_algo_recv_data(struct data_unit_t *event, void *reserved)
{
	int err = 0;
	if (event->flush_action == FLUSH_ACTION)
		err = situation_flush_report(ID_SAR_ALGO);
	else if (event->flush_action == DATA_ACTION){
		sardata = (uint32_t)event->data[0];
		err = situation_data_report_t(ID_SAR_ALGO, (uint32_t)event->data[0], (int64_t)event->time_stamp);
	}
	return err;
}
/*N19a code for HQ-348009 by huweifeng at 2023/12/23 end*/

static int sar_algo_local_init(void)
{
	struct situation_control_path ctl = {0};
	struct situation_data_path data = {0};
	int err = 0;
	/*N19a code for HQ-348009 by huweifeng at 2023/12/23 start*/
	struct saralgo_ipi_data *obj;
	pr_debug("%s\n", __func__);
	obj = kzalloc(sizeof(*obj), GFP_KERNEL);
	if (!obj) {
		err = -ENOMEM;
		goto exit;
	}
	memset(obj, 0, sizeof(*obj));
	obj_ipi_data = obj;
	WRITE_ONCE(obj->factory_enable, false);
	/*N19a code for HQ-348009 by huweifeng at 2023/12/23 end*/
	pr_err("%s : sar_algo init HTP\n", __func__);
	ctl.open_report_data = sar_algo_open_report_data;
	ctl.batch = sar_algo_batch;
	ctl.flush = sar_algo_flush;
	ctl.is_support_wake_lock = true;
	ctl.set_cali = saralgo_set_step;
	err = situation_register_control_path(&ctl, ID_SAR_ALGO);
	if (err) {
		pr_err("register sar_algo control path err\n");
		goto exit_create_attr_failed;
	}
	data.get_data = sar_algo_get_data;
	err = situation_register_data_path(&data, ID_SAR_ALGO);
	if (err) {
		pr_err("register sar_algo data path err\n");
		goto exit_create_attr_failed;
	}
	err = scp_sensorHub_data_registration(ID_SAR_ALGO, sar_algo_recv_data);
	if (err) {
		pr_err("SCP_sensorHub_data_registration fail!!\n");
		goto exit_create_attr_failed;
	}
	/*N19a code for HQ-348009 by huweifeng at 2023/12/23 start*/
	err = saralgo_factory_device_register(&saralgo_factory_device);
	if (err) {
		pr_err("saralgo_factory_device_register register failed\n");
		goto exit_create_attr_failed;
	}
	/*N19a code for HQ-348009 by huweifeng at 2023/12/23 end*/
	return 0;
exit_create_attr_failed:
kfree(obj);
exit:
	return -1;
}
static int sar_algo_local_uninit(void)
{       if (obj_ipi_data) {
        kfree(obj_ipi_data);
        obj_ipi_data = NULL;
	}
	return 0;
}
static struct situation_init_info sar_algo_init_info = {
	.name = "sar_algo_hub",
	.init = sar_algo_local_init,
	.uninit = sar_algo_local_uninit,
};
static int __init sar_algo_init(void)
{
	situation_driver_add(&sar_algo_init_info, ID_SAR_ALGO);
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
MODULE_AUTHOR("huweifeng@huaqin.com");
