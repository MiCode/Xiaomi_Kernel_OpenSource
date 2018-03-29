/* gmagrotvechub motion sensor driver
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
#include "gmagrotvechub.h"
#include <fusion.h>
#include <SCP_sensorHub.h>
#include <linux/notifier.h>
#include "scp_helper.h"


#define GMAGROTVEC_TAG                  "[gmagrotvechub] "
#define GMAGROTVEC_FUN(f)               printk(GMAGROTVEC_TAG"%s\n", __func__)
#define GMAGROTVEC_ERR(fmt, args...)    printk(GMAGROTVEC_TAG"%s %d : "fmt, __func__, __LINE__, ##args)
#define GMAGROTVEC_LOG(fmt, args...)    printk(GMAGROTVEC_TAG fmt, ##args)

static struct fusion_init_info gmagrotvechub_init_info;

static int gmagrotvec_get_data(int *x, int *y, int *z, int *scalar, int *status)
{
	int err = 0;
	struct data_unit_t data;
	uint64_t time_stamp = 0;
	uint64_t time_stamp_gpt = 0;

	err = sensor_get_data_from_hub(ID_GEOMAGNETIC_ROTATION_VECTOR, &data);
	if (err < 0) {
		GMAGROTVEC_ERR("sensor_get_data_from_hub fail!!\n");
		return -1;
	}
	time_stamp		= data.time_stamp;
	time_stamp_gpt	= data.time_stamp_gpt;
	*x				= data.magnetic_t.azimuth;
	*y				= data.magnetic_t.pitch;
	*z				= data.magnetic_t.roll;
	*scalar				= data.magnetic_t.scalar;
	*status		= data.magnetic_t.status;
	/* GMAGROTVEC_LOG("recv ipi: timestamp: %lld, timestamp_gpt: %lld, x: %d, y: %d, z: %d!\n",
		time_stamp, time_stamp_gpt, *x, *y, *z); */
	return 0;
}
static int gmagrotvec_open_report_data(int open)
{
	return 0;
}
static int gmagrotvec_enable_nodata(int en)
{
	return sensor_enable_to_hub(ID_GEOMAGNETIC_ROTATION_VECTOR, en);
}
static int gmagrotvec_set_delay(u64 delay)
{
#if defined CONFIG_MTK_SCP_SENSORHUB_V1
	unsigned int delayms = 0;

	delayms = delay / 1000 / 1000;
	return sensor_set_delay_to_hub(ID_GEOMAGNETIC_ROTATION_VECTOR, delayms);
#elif defined CONFIG_NANOHUB
	return 0;
#else
	return 0;
#endif
}
static int gmagrotvec_batch(int flag, int64_t samplingPeriodNs, int64_t maxBatchReportLatencyNs)
{
	return sensor_batch_to_hub(ID_GEOMAGNETIC_ROTATION_VECTOR, flag, samplingPeriodNs, maxBatchReportLatencyNs);
}

static int gmagrotvec_flush(void)
{
	return sensor_flush_to_hub(ID_GEOMAGNETIC_ROTATION_VECTOR);
}

static int gmagrotvec_recv_data(struct data_unit_t *event, void *reserved)
{
	int err = 0;

	if (event->flush_action == FLUSH_ACTION)
		err = gmrv_flush_report();
	else if (event->flush_action == DATA_ACTION)
		err = gmrv_data_report(event->magnetic_t.x, event->magnetic_t.y, event->magnetic_t.z,
			event->magnetic_t.scalar, event->magnetic_t.status,
			(int64_t)(event->time_stamp + event->time_stamp_gpt));

	return err;
}
static int gmagrotvechub_local_init(void)
{
	struct fusion_control_path ctl = {0};
	struct fusion_data_path data = {0};
	int err = 0;

	ctl.open_report_data = gmagrotvec_open_report_data;
	ctl.enable_nodata = gmagrotvec_enable_nodata;
	ctl.set_delay = gmagrotvec_set_delay;
	ctl.batch = gmagrotvec_batch;
	ctl.flush = gmagrotvec_flush;
#if defined CONFIG_MTK_SCP_SENSORHUB_V1
	ctl.is_report_input_direct = true;
	ctl.is_support_batch = false;
#elif defined CONFIG_NANOHUB
	ctl.is_report_input_direct = true;
	ctl.is_support_batch = false;
#else
#endif
	err = fusion_register_control_path(&ctl, ID_GEOMAGNETIC_ROTATION_VECTOR);
	if (err) {
		GMAGROTVEC_ERR("register gmagrotvec control path err\n");
		goto exit;
	}

	data.get_data = gmagrotvec_get_data;
	data.vender_div = 1000000;
	err = fusion_register_data_path(&data, ID_GEOMAGNETIC_ROTATION_VECTOR);
	if (err) {
		GMAGROTVEC_ERR("register gmagrotvec data path err\n");
		goto exit;
	}
	err = SCP_sensorHub_data_registration(ID_GEOMAGNETIC_ROTATION_VECTOR, gmagrotvec_recv_data);
	if (err < 0) {
		GMAGROTVEC_ERR("SCP_sensorHub_data_registration failed\n");
		goto exit;
	}
	return 0;
exit:
	return -1;
}
static int gmagrotvechub_local_uninit(void)
{
	return 0;
}

static struct fusion_init_info gmagrotvechub_init_info = {
	.name = "gmagrotvec_hub",
	.init = gmagrotvechub_local_init,
	.uninit = gmagrotvechub_local_uninit,
};

static int __init gmagrotvechub_init(void)
{
	fusion_driver_add(&gmagrotvechub_init_info, ID_GEOMAGNETIC_ROTATION_VECTOR);
	return 0;
}

static void __exit gmagrotvechub_exit(void)
{
	GMAGROTVEC_FUN();
}

module_init(gmagrotvechub_init);
module_exit(gmagrotvechub_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("gmagrotvec driver");
MODULE_AUTHOR("hongxu.zhao@mediatek.com");
