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
#include "sarunify.h"
#include "situation.h"
#include <hwmsen_helper.h>
#include <SCP_sensorHub.h>
#include <linux/notifier.h>
#include "include/scp.h"
struct sarhub_ipi_data {
	bool factory_enable;
	atomic_t  trace ;
	int32_t cali_data[3];
	int8_t cali_status;
};

static struct sarhub_ipi_data *obj_ipi_data;
static int sar_unify_get_data(int *probability, int *status)
{
	int err = 0;
	struct data_unit_t data;
	uint64_t time_stamp = 0;
	err = sensor_get_data_from_hub(ID_SAR_UNIFY, &data);
	if (err < 0) {
		pr_err("sensor_get_data_from_hub fail!!\n");
		return -1;
	}
	time_stamp = data.time_stamp;
	*probability = data.data[0];//data.gesture_data_t.probability;
	pr_err("sarunify recv ipi: timestamp: %lld, probability: %d\n", time_stamp,
		*probability, data.data[0]);
	return 0;
}
static int sar_unify_open_report_data(int open)
{
	int ret = 0;
	pr_err("%s : enable=%d HTP\n", __func__, open);
#if defined CONFIG_MTK_SCP_SENSORHUB_V1
	if (open == 1)
		ret = sensor_set_delay_to_hub(ID_SAR_UNIFY, 120);
#elif defined CONFIG_NANOHUB
#else
#endif
	ret = sensor_enable_to_hub(ID_SAR_UNIFY, open);
	return ret;
}
static int sar_unify_batch(int flag, int64_t samplingPeriodNs,
		int64_t maxBatchReportLatencyNs)
{
	pr_err("%s : flag=%d HTP\n", __func__, flag);
	return sensor_batch_to_hub(ID_SAR_UNIFY, flag, samplingPeriodNs,
			maxBatchReportLatencyNs);
}

static int sarunify_flush(void)
{
	return sensor_flush_to_hub(ID_SAR_UNIFY);
}

static int sar_unify_recv_data(struct data_unit_t *event, void *reserved)
{
	int err = 0;
	int32_t value[3] = {0};
	if (event->flush_action == FLUSH_ACTION) {
                err = situation_flush_report(ID_SAR_UNIFY);
		pr_err("sar_unify do not support flush\n");
        }
	else if (event->flush_action == DATA_ACTION) {
	value[0] = event->data[0];
	err = sarunify_data_report_t(value, (int64_t)event->time_stamp);
	pr_err("sarunify, value[0]: %d\n", value[0]);
	//situation_data_report_t(ID_SAR_UNIFY, (uint32_t)event->data[0], (int64_t)event->time_stamp);
	}
	return err;
}

static ssize_t sarunify_trace_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t res = 0;
	struct sarhub_ipi_data *obj = obj_ipi_data;
	if (!obj_ipi_data) {
		pr_err("sarunify obj_ipi_data is null!!\n");
		return 0;
	}
	res = snprintf(buf, PAGE_SIZE, "%d\n", atomic_read(&obj->trace));
	return res;
}
//EXPORT_SYMBOL(sar_data_report_t);
static ssize_t sarunify_trace_store(struct device *dev,
	struct device_attribute *attr, const char *buf,size_t count)
{
	int trace = 0;
	struct sarhub_ipi_data *obj = obj_ipi_data;
	int res = 0;
	int ret = 0;
	if (!obj) {
		pr_err("sarunify obj_ipi_data is null!!\n");
		return 0;
	}
	ret = sscanf(buf, "%d", &trace);
	if (ret != 1) {
		pr_err("sarunify invalid content: '%s', length = %zu\n", buf, count);
		return count;
	}
	atomic_set(&obj->trace, trace);
	res = sensor_set_cmd_to_hub(ID_SAR_UNIFY,
				CUST_ACTION_SET_TRACE, &trace);
	if (res < 0) {
		pr_err("sarunify sensor_set_cmd_to_hub fail,(ID: %d),(action: %d)\n",
				ID_SAR_UNIFY, CUST_ACTION_SET_TRACE);
		return count;
	}
	return count;
}

static DEVICE_ATTR_RW(sarunify_trace);

static int sarunifyhub_probe(struct platform_device *pdev)
{
	int ret;
	struct device *dev;
	pr_err("sarunify_probe\n");
	dev = &pdev->dev;
	ret = device_create_file(dev, &dev_attr_sarunify_trace);
	if(ret != 0) {
		pr_err("device create fail\n");
		return ret;
	}
	pr_err("sarunify_probe ok\n");
	return 0;
}

static int sarunifyhub_remove(struct platform_device *pdev)
{
	device_remove_file(&(pdev->dev), &dev_attr_sarunify_trace);
	return 0;
}

static struct platform_device sarunifyhub_device = {
	.name = "sarunify_hub_s",
	.id = -1,
};

static struct platform_driver sarunifyhub_driver = {
        .probe = sarunifyhub_probe,
        .remove = sarunifyhub_remove,
        .driver = {
                .name = "sarunify_hub_s",
        },
};

static int sar_unify_local_init(void)
{
	struct situation_control_path ctl = {0};
	struct situation_data_path data = {0};
	int err = 0;
	struct sarhub_ipi_data *obj;

        pr_err("%s\n", __func__);
        obj = kzalloc(sizeof(*obj), GFP_KERNEL);
        if (!obj) {
                return -ENOMEM;
                //goto exit;
        }

        memset(obj, 0, sizeof(*obj));
        obj_ipi_data = obj;

	pr_err("%s : sar_unify init HTP\n", __func__);
	ctl.open_report_data = sar_unify_open_report_data;
	ctl.batch = sar_unify_batch;
        ctl.flush = sarunify_flush;
	ctl.is_support_wake_lock = true;
        ctl.is_support_batch = false;
	err = situation_register_control_path(&ctl, ID_SAR_UNIFY);
	if (err) {
		pr_err("register sar_unify control path err\n");
		goto exit;
	}
	data.get_data = sar_unify_get_data;
	err = situation_register_data_path(&data, ID_SAR_UNIFY);
	if (err) {
		pr_err("register sar_unify data path err\n");
		goto exit;
	}
	err = scp_sensorHub_data_registration(ID_SAR_UNIFY, sar_unify_recv_data);
	if (err) {
		pr_err("SCP_sensorHub_data_registration fail!!\n");
		goto exit_create_attr_failed;
	}
	err = platform_driver_register(&sarunifyhub_driver);
	if (err) {
		pr_err("sar_unify init add driver error\n");
		goto exit;
	}
	return 0;
exit:
exit_create_attr_failed:
	kfree(obj);
        obj_ipi_data = NULL;
	return -1;
}
static int sar_unify_local_uninit(void)
{
	return 0;
}
static struct situation_init_info sar_unify_init_info = {
	.name = "sar_unify_hub",
	.init = sar_unify_local_init,
	.uninit = sar_unify_local_uninit,
};
int __init sar_unify_init(void)
{	
		pr_err("sar_unify hub initl!!\n");
	if (platform_device_register(&sarunifyhub_device)) {
		pr_err("sar_unify  platform device error\n");
		return -1;
	}
	situation_driver_add(&sar_unify_init_info, ID_SAR_UNIFY);
	return 0;
}
void __exit sar_unify_exit(void)
{
	pr_info("sar_unify exit\n");
}
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("SAR_UNIFY_HUB driver");

