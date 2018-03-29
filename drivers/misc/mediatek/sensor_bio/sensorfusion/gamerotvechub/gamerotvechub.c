/* gamerotvechub motion sensor driver
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
#include "gamerotvechub.h"
#include <fusion.h>
#include <SCP_sensorHub.h>
#include <linux/notifier.h>
#include "scp_helper.h"

#define GROTVEC_TAG                  "[gamerotvechub] "
#define GROTVEC_FUN(f)               pr_debug(GROTVEC_TAG"%s\n", __func__)
#define GROTVEC_ERR(fmt, args...)    pr_err(GROTVEC_TAG"%s %d : "fmt, __func__, __LINE__, ##args)
#define GROTVEC_LOG(fmt, args...)    pr_debug(GROTVEC_TAG fmt, ##args)

static struct fusion_init_info gamerotvechub_init_info;

static int gamerotvec_get_data(int *x, int *y, int *z, int *scalar, int *status)
{
	int err = 0;
	struct data_unit_t data;
	uint64_t time_stamp = 0;
	uint64_t time_stamp_gpt = 0;

	err = sensor_get_data_from_hub(ID_GAME_ROTATION_VECTOR, &data);
	if (err < 0) {
		GROTVEC_ERR("sensor_get_data_from_hub fail!!\n");
		return -1;
	}
	time_stamp		= data.time_stamp;
	time_stamp_gpt	= data.time_stamp_gpt;
	*x				= data.orientation_t.azimuth;
	*y				= data.orientation_t.pitch;
	*z				= data.orientation_t.roll;
	*scalar				= data.orientation_t.scalar;
	*status			= data.orientation_t.status;
	GROTVEC_LOG("recv ipi: timestamp: %lld, timestamp_gpt: %lld, x: %d, y: %d, z: %d!\n",
		time_stamp, time_stamp_gpt, *x, *y, *z);
	return 0;
}
static int gamerotvec_open_report_data(int open)
{
	return 0;
}
static int gamerotvec_enable_nodata(int en)
{
	return sensor_enable_to_hub(ID_GAME_ROTATION_VECTOR, en);
}
static int gamerotvec_set_delay(u64 delay)
{
#if defined CONFIG_MTK_SCP_SENSORHUB_V1
	unsigned int delayms = 0;

	delayms = delay / 1000 / 1000;
	return sensor_set_delay_to_hub(ID_GAME_ROTATION_VECTOR, delayms);
#elif defined CONFIG_NANOHUB
	return 0;
#else
	return 0;
#endif
}
static int gamerotvec_batch(int flag, int64_t samplingPeriodNs, int64_t maxBatchReportLatencyNs)
{
	return sensor_batch_to_hub(ID_GAME_ROTATION_VECTOR, flag, samplingPeriodNs, maxBatchReportLatencyNs);
}

static int gamerotvec_flush(void)
{
	return sensor_flush_to_hub(ID_GAME_ROTATION_VECTOR);
}

static int gamerotvec_recv_data(struct data_unit_t *event, void *reserved)
{
	int err = 0;

	if (event->flush_action == FLUSH_ACTION)
		err = grv_flush_report();
	else if (event->flush_action == DATA_ACTION)
		err = grv_data_report(event->orientation_t.azimuth, event->orientation_t.pitch,
			event->orientation_t.roll, event->orientation_t.scalar,
			event->orientation_t.status, (int64_t)(event->time_stamp + event->time_stamp_gpt));

	return err;
}
static int gamerotvechub_local_init(void)
{
	struct fusion_control_path ctl = {0};
	struct fusion_data_path data = {0};
	int err = 0;

	ctl.open_report_data = gamerotvec_open_report_data;
	ctl.enable_nodata = gamerotvec_enable_nodata;
	ctl.set_delay = gamerotvec_set_delay;
	ctl.batch = gamerotvec_batch;
	ctl.flush = gamerotvec_flush;
#if defined CONFIG_MTK_SCP_SENSORHUB_V1
	ctl.is_report_input_direct = true;
	ctl.is_support_batch = false;
#elif defined CONFIG_NANOHUB
	ctl.is_report_input_direct = true;
	ctl.is_support_batch = false;
#else
#endif
	err = fusion_register_control_path(&ctl, ID_GAME_ROTATION_VECTOR);
	if (err) {
		GROTVEC_ERR("register gamerotvec control path err\n");
		goto exit;
	}

	data.get_data = gamerotvec_get_data;
	data.vender_div = 1000000;
	err = fusion_register_data_path(&data, ID_GAME_ROTATION_VECTOR);
	if (err) {
		GROTVEC_ERR("register gamerotvec data path err\n");
		goto exit;
	}
	err = SCP_sensorHub_data_registration(ID_GAME_ROTATION_VECTOR, gamerotvec_recv_data);
	if (err < 0) {
		GROTVEC_ERR("SCP_sensorHub_data_registration failed\n");
		goto exit;
	}
	return 0;
exit:
	return -1;
}
static int gamerotvechub_local_uninit(void)
{
	return 0;
}

static struct fusion_init_info gamerotvechub_init_info = {
	.name = "gamerotvec_hub",
	.init = gamerotvechub_local_init,
	.uninit = gamerotvechub_local_uninit,
};

static int __init gamerotvechub_init(void)
{
	fusion_driver_add(&gamerotvechub_init_info, ID_GAME_ROTATION_VECTOR);
	return 0;
}

static void __exit gamerotvechub_exit(void)
{
	GROTVEC_FUN();
}

module_init(gamerotvechub_init);
module_exit(gamerotvechub_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("gamerotvec driver");
MODULE_AUTHOR("hongxu.zhao@mediatek.com");
