/* Copyright (c) 2014-2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#define pr_fmt(fmt) "%s:%s " fmt, KBUILD_MODNAME, __func__

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/mutex.h>
#include "lmh_interface.h"
#include <linux/slab.h>
#include <asm/cacheflush.h>
#include <soc/qcom/scm.h>
#include <linux/dma-mapping.h>
#include <linux/regulator/consumer.h>

#define CREATE_TRACE_POINTS
#define TRACE_MSM_LMH
#include <trace/trace_thermal.h>

#define LMH_DRIVER_NAME			"lmh-lite-driver"
#define LMH_INTERRUPT			"lmh-interrupt"
#define LMH_DEVICE			"lmh-profile"
#define LMH_MAX_SENSOR			10
#define LMH_GET_PROFILE_SIZE		10
#define LMH_SCM_PAYLOAD_SIZE		10
#define LMH_DEFAULT_PROFILE		0
#define LMH_DEBUG_READ_TYPE		0x0
#define LMH_DEBUG_CONFIG_TYPE		0x1
#define LMH_CHANGE_PROFILE		0x01
#define LMH_GET_PROFILES		0x02
#define LMH_CTRL_QPMDA			0x03
#define LMH_TRIM_ERROR			0x04
#define LMH_GET_INTENSITY		0x06
#define LMH_GET_SENSORS			0x07
#define LMH_DEBUG_SET			0x08
#define LMH_DEBUG_READ_BUF_SIZE		0x09
#define LMH_DEBUG_READ			0x0A
#define LMH_DEBUG_GET_TYPE		0x0B
#define MAX_TRACE_EVENT_MSG_LEN		50
#define APCS_DPM_VOLTAGE_SCALE		0x09950804
#define LMH_ODCM_MAX_COUNT		6

#define LMH_CHECK_SCM_CMD(_cmd) \
	do { \
		if (!scm_is_call_available(SCM_SVC_LMH, _cmd)) { \
			pr_err("SCM cmd:%d not available\n", _cmd); \
			return -ENODEV; \
		} \
	} while (0)

#define LMH_GET_RECURSSIVE_DATA(desc_arg, cmd_idx, cmd_buf, payload, next, \
	size, cmd_id, dest_buf, ret)					\
	do {								\
		int idx = 0;						\
		desc_arg.args[cmd_idx] = cmd_buf.list_start = next;	\
		trace_lmh_event_call("GET_TYPE enter");			\
		dmac_flush_range(payload, (void *)payload +             \
				sizeof(uint32_t) * LMH_SCM_PAYLOAD_SIZE);\
		if (!is_scm_armv8()) {					\
			ret = scm_call(SCM_SVC_LMH, cmd_id,		\
				(void *) &cmd_buf, SCM_BUFFER_SIZE(cmd_buf), \
				&size, SCM_BUFFER_SIZE(size));		\
		} else {						\
			ret = scm_call2(SCM_SIP_FNID(SCM_SVC_LMH,	\
				cmd_id), &desc_arg);			\
			size = desc_arg.ret[0];				\
		}							\
		/* Have barrier before reading from TZ data */		\
		mb();							\
		dmac_inv_range(payload, (void *)payload +               \
				sizeof(uint32_t) * LMH_SCM_PAYLOAD_SIZE);\
		trace_lmh_event_call("GET_TYPE exit");			\
		if (ret) {						\
			pr_err("Error in SCM v%d get type. cmd:%x err:%d\n", \
				(is_scm_armv8()) ? 8 : 7, cmd_id, ret);	\
			break;						\
		}							\
		if (!size) {						\
			pr_err("No LMH device supported.\n");		\
			ret = -ENODEV;					\
			break;						\
		}							\
		if (!dest_buf) {					\
			dest_buf = devm_kzalloc(lmh_data->dev,		\
				sizeof(uint32_t) * size, GFP_KERNEL);	\
			if (!dest_buf) {				\
				ret = -ENOMEM;				\
				break;					\
			}						\
		}							\
		for (idx = next;					\
			idx < min((next + LMH_SCM_PAYLOAD_SIZE), size); \
			idx++)						\
			dest_buf[idx] = payload[idx - next];		\
		next += LMH_SCM_PAYLOAD_SIZE;				\
	} while (next < size)						\

struct __attribute__((__packed__)) lmh_sensor_info {
	uint32_t			name;
	uint32_t			node_id;
	uint32_t			intensity;
	uint32_t			max_intensity;
	uint32_t			type;
};

struct __attribute__((__packed__)) lmh_sensor_packet {
	uint32_t			count;
	struct lmh_sensor_info		sensor[LMH_MAX_SENSOR];
};

struct lmh_profile {
	struct lmh_device_ops		dev_ops;
	uint32_t			level_ct;
	uint32_t			curr_level;
	uint32_t			*levels;
	uint32_t			read_type_count;
	uint32_t			config_type_count;
};

struct lmh_debug {
	struct lmh_debug_ops		debug_ops;
	uint32_t			*read_type;
	uint32_t			*config_type;
	uint32_t			read_type_count;
	uint32_t			config_type_count;
};

struct lmh_driver_data {
	struct device			*dev;
	struct workqueue_struct		*poll_wq;
	struct delayed_work		poll_work;
	uint32_t			log_enabled;
	uint32_t			log_delay;
	enum lmh_monitor_state		intr_state;
	uint32_t			intr_reg_val;
	uint32_t			intr_status_val;
	uint32_t			trim_err_offset;
	bool				trim_err_disable;
	void				*intr_addr;
	int				irq_num;
	int				max_sensor_count;
	struct lmh_profile		dev_info;
	struct lmh_debug		debug_info;
	struct regulator		*regulator;
	struct notifier_block		dpm_notifier_blk;
	void __iomem			*dpm_voltage_scale_reg;
	uint32_t			odcm_thresh_mV;
	void __iomem			*odcm_reg[LMH_ODCM_MAX_COUNT];
	bool				odcm_enabled;
};

struct lmh_sensor_data {
	char				sensor_name[LMH_NAME_MAX];
	uint32_t			sensor_hw_name;
	uint32_t			sensor_hw_node_id;
	int				sensor_sw_id;
	struct lmh_sensor_ops		ops;
	int				last_read_value;
	struct list_head		list_ptr;
};

struct lmh_default_data {
	uint32_t			default_profile;
	uint32_t			odcm_reg_addr[LMH_ODCM_MAX_COUNT];
};

static struct lmh_default_data		lmh_lite_data = {
	.default_profile = 0,
};
static struct lmh_default_data		lmh_v1_data = {
	.default_profile = 1,
	.odcm_reg_addr = {	0x09981030, /* CPU0 */
				0x09991030, /* CPU1 */
				0x099A1028, /* APC0_L2 */
				0x099B1030, /* CPU2 */
				0x099C1030, /* CPU3 */
				0x099D1028, /* APC1_l2 */
	},
};
static struct lmh_default_data		*lmh_hw_data;
static struct lmh_driver_data		*lmh_data;
static DECLARE_RWSEM(lmh_sensor_access);
static DEFINE_MUTEX(lmh_sensor_read);
static DEFINE_MUTEX(lmh_odcm_access);
static LIST_HEAD(lmh_sensor_list);

static int lmh_read(struct lmh_sensor_ops *ops, int *val)
{
	struct lmh_sensor_data *lmh_sensor = container_of(ops,
		       struct lmh_sensor_data, ops);

	mutex_lock(&lmh_sensor_read);
	*val = lmh_sensor->last_read_value;
	mutex_unlock(&lmh_sensor_read);

	return 0;
}

static int lmh_ctrl_qpmda(uint32_t enable)
{
	int ret = 0;
	struct scm_desc desc_arg;
	struct {
		uint32_t enable;
		uint32_t rate;
	} cmd_buf;

	desc_arg.args[0] = cmd_buf.enable = enable;
	desc_arg.args[1] = cmd_buf.rate = lmh_data->log_delay;
	desc_arg.arginfo = SCM_ARGS(2, SCM_VAL, SCM_VAL);
	trace_lmh_event_call("CTRL_QPMDA enter");
	if (!is_scm_armv8())
		ret = scm_call(SCM_SVC_LMH, LMH_CTRL_QPMDA,
			(void *) &cmd_buf, SCM_BUFFER_SIZE(cmd_buf), NULL, 0);
	else
		ret = scm_call2(SCM_SIP_FNID(SCM_SVC_LMH,
			LMH_CTRL_QPMDA), &desc_arg);
	trace_lmh_event_call("CTRL_QPMDA exit");
	if (ret) {
		pr_err("Error in SCM v%d %s QPMDA call. err:%d\n",
			(is_scm_armv8()) ? 8 : 7, (enable) ? "enable" :
			"disable", ret);
		goto ctrl_exit;
	}

ctrl_exit:
	return ret;
}

static int lmh_disable_log(void)
{
	int ret = 0;

	if (!lmh_data->log_enabled)
		return ret;
	ret = lmh_ctrl_qpmda(0);
	if (ret)
		goto disable_exit;
	pr_debug("LMH hardware log disabled.\n");
	lmh_data->log_enabled = 0;

disable_exit:
	return ret;
}

static int lmh_enable_log(uint32_t delay, uint32_t reg_val)
{
	int ret = 0;

	if (lmh_data->log_enabled == reg_val && lmh_data->log_delay == delay)
		return ret;

	lmh_data->log_delay = delay;
	ret = lmh_ctrl_qpmda(reg_val);
	if (ret)
		goto enable_exit;
	pr_debug("LMH hardware log enabled[%u]. delay:%u\n", reg_val, delay);
	lmh_data->log_enabled = reg_val;

enable_exit:
	return ret;
}

static void lmh_update(struct lmh_driver_data *lmh_dat,
	struct lmh_sensor_data *lmh_sensor)
{
	if (lmh_sensor->last_read_value > 0 && !(lmh_dat->intr_status_val
		& BIT(lmh_sensor->sensor_sw_id))) {
		pr_debug("Sensor:[%s] interrupt triggered\n",
			lmh_sensor->sensor_name);
		trace_lmh_sensor_interrupt(lmh_sensor->sensor_name,
			lmh_sensor->last_read_value);
		lmh_dat->intr_status_val |= BIT(lmh_sensor->sensor_sw_id);
	} else if (lmh_sensor->last_read_value == 0 && (lmh_dat->intr_status_val
		& BIT(lmh_sensor->sensor_sw_id))) {
		pr_debug("Sensor:[%s] interrupt clear\n",
			lmh_sensor->sensor_name);
		trace_lmh_sensor_interrupt(lmh_sensor->sensor_name,
			lmh_sensor->last_read_value);

		lmh_data->intr_status_val ^= BIT(lmh_sensor->sensor_sw_id);
	}
	lmh_sensor->ops.new_value_notify(&lmh_sensor->ops,
		lmh_sensor->last_read_value);
}

static void lmh_read_and_update(struct lmh_driver_data *lmh_dat)
{
	int ret = 0, idx = 0;
	struct lmh_sensor_data *lmh_sensor = NULL;
	static struct lmh_sensor_packet payload;
	struct scm_desc desc_arg;
	struct {
		/* TZ is 32-bit right now */
		uint32_t addr;
		uint32_t size;
	} cmd_buf;

	mutex_lock(&lmh_sensor_read);
	list_for_each_entry(lmh_sensor, &lmh_sensor_list, list_ptr)
		lmh_sensor->last_read_value = 0;
	payload.count = 0;
	cmd_buf.addr = SCM_BUFFER_PHYS(&payload);
	/* &payload may be a physical address > 4 GB */
	desc_arg.args[0] = SCM_BUFFER_PHYS(&payload);
	desc_arg.args[1] = cmd_buf.size
			= SCM_BUFFER_SIZE(struct lmh_sensor_packet);
	desc_arg.arginfo = SCM_ARGS(2, SCM_RW, SCM_VAL);
	trace_lmh_event_call("GET_INTENSITY enter");
	dmac_flush_range(&payload, (void *)&payload +
			sizeof(struct lmh_sensor_packet));
	if (!is_scm_armv8())
		ret = scm_call(SCM_SVC_LMH, LMH_GET_INTENSITY,
			(void *) &cmd_buf, SCM_BUFFER_SIZE(cmd_buf), NULL, 0);
	else
		ret = scm_call2(SCM_SIP_FNID(SCM_SVC_LMH,
			LMH_GET_INTENSITY), &desc_arg);
	/* Have memory barrier before we access the TZ data */
	mb();
	trace_lmh_event_call("GET_INTENSITY exit");
	dmac_inv_range(&payload, (void *)&payload +
			sizeof(struct lmh_sensor_packet));
	if (ret) {
		pr_err("Error in SCM v%d read call. err:%d\n",
				(is_scm_armv8()) ? 8 : 7, ret);
		goto read_exit;
	}

	for (idx = 0; idx < payload.count; idx++) {
		list_for_each_entry(lmh_sensor, &lmh_sensor_list, list_ptr) {

			if (payload.sensor[idx].name
				== lmh_sensor->sensor_hw_name
				&& (payload.sensor[idx].node_id
				== lmh_sensor->sensor_hw_node_id)) {

				lmh_sensor->last_read_value =
					(payload.sensor[idx].max_intensity) ?
					((payload.sensor[idx].intensity * 100)
					/ payload.sensor[idx].max_intensity)
					: payload.sensor[idx].intensity;
				trace_lmh_sensor_reading(
					lmh_sensor->sensor_name,
					lmh_sensor->last_read_value);
				break;
			}
		}
	}

read_exit:
	mutex_unlock(&lmh_sensor_read);
	list_for_each_entry(lmh_sensor, &lmh_sensor_list, list_ptr)
		lmh_update(lmh_dat, lmh_sensor);

	return;
}

static void lmh_poll(struct work_struct *work)
{
	struct lmh_driver_data *lmh_dat = container_of(work,
		       struct lmh_driver_data, poll_work.work);

	down_write(&lmh_sensor_access);
	if (lmh_dat->intr_state != LMH_ISR_POLLING)
		goto poll_exit;
	lmh_read_and_update(lmh_dat);
	if (!lmh_data->intr_status_val) {
		lmh_data->intr_state = LMH_ISR_MONITOR;
		pr_debug("Zero throttling. Re-enabling interrupt\n");
		trace_lmh_event_call("Lmh Interrupt Clear");
		enable_irq(lmh_data->irq_num);
		goto poll_exit;
	} else {
		queue_delayed_work(lmh_dat->poll_wq, &lmh_dat->poll_work,
			msecs_to_jiffies(lmh_get_poll_interval()));
	}

poll_exit:
	up_write(&lmh_sensor_access);
	return;
}

static void lmh_trim_error(void)
{
	struct scm_desc desc_arg;
	int ret = 0;

	WARN_ON(1);
	pr_err("LMH hardware trim error\n");
	desc_arg.arginfo = SCM_ARGS(0);
	trace_lmh_event_call("TRIM_ERROR enter");
	if (!is_scm_armv8())
		ret = scm_call(SCM_SVC_LMH, LMH_TRIM_ERROR, NULL, 0, NULL, 0);
	else
		ret = scm_call2(SCM_SIP_FNID(SCM_SVC_LMH,
			LMH_TRIM_ERROR), &desc_arg);
	trace_lmh_event_call("TRIM_ERROR exit");
	if (ret)
		pr_err("Error in SCM v%d trim error call. err:%d\n",
					(is_scm_armv8()) ? 8 : 7, ret);

	return;
}

static irqreturn_t lmh_isr_thread(int irq, void *data)
{
	struct lmh_driver_data *lmh_dat = data;

	pr_debug("LMH Interrupt triggered\n");
	trace_lmh_event_call("Lmh Interrupt");

	disable_irq_nosync(irq);
	down_write(&lmh_sensor_access);
	if (lmh_dat->intr_state != LMH_ISR_MONITOR) {
		pr_err("Invalid software state\n");
		trace_lmh_event_call("Invalid software state");
		WARN_ON(1);
		goto isr_unlock_exit;
	}
	lmh_dat->intr_state = LMH_ISR_POLLING;
	if (!lmh_data->trim_err_disable) {
		lmh_dat->intr_reg_val = readl_relaxed(lmh_dat->intr_addr);
		pr_debug("Lmh hw interrupt:%d\n", lmh_dat->intr_reg_val);
		if (lmh_dat->intr_reg_val & BIT(lmh_dat->trim_err_offset)) {
			trace_lmh_event_call("Lmh trim error");
			lmh_trim_error();
			lmh_dat->intr_state = LMH_ISR_MONITOR;
			goto decide_next_action;
		}
	}
	lmh_read_and_update(lmh_dat);
	if (!lmh_dat->intr_status_val) {
		pr_debug("LMH not throttling. Enabling interrupt\n");
		lmh_dat->intr_state = LMH_ISR_MONITOR;
		trace_lmh_event_call("Lmh Zero throttle Interrupt Clear");
		goto decide_next_action;
	}

decide_next_action:
	if (lmh_dat->intr_state == LMH_ISR_POLLING)
		queue_delayed_work(lmh_dat->poll_wq, &lmh_dat->poll_work,
			msecs_to_jiffies(lmh_get_poll_interval()));
	else
		enable_irq(lmh_dat->irq_num);

isr_unlock_exit:
	up_write(&lmh_sensor_access);
	return IRQ_HANDLED;
}

static int lmh_get_sensor_devicetree(struct platform_device *pdev)
{
	int ret = 0, idx = 0;
	char *key = NULL;
	struct device_node *node = pdev->dev.of_node;
	struct resource *lmh_intr_base = NULL;

	lmh_data->trim_err_disable = false;
	key = "qcom,lmh-trim-err-offset";
	ret = of_property_read_u32(node, key,
			&lmh_data->trim_err_offset);
	if (ret) {
		if (ret == -EINVAL) {
			lmh_data->trim_err_disable = true;
			ret = 0;
		} else {
			pr_err("Error reading:%s. err:%d\n", key, ret);
			goto dev_exit;
		}
	}

	lmh_data->regulator = devm_regulator_get(lmh_data->dev, "vdd-apss");
	if (IS_ERR(lmh_data->regulator)) {
		pr_err("unable to get vdd-apss regulator. err:%ld\n",
			PTR_ERR(lmh_data->regulator));
		lmh_data->regulator = NULL;
	} else {
		key = "qcom,lmh-odcm-disable-threshold-mA";
		ret = of_property_read_u32(node, key,
			&lmh_data->odcm_thresh_mV);
		if (ret) {
			pr_err("Error getting ODCM thresh. err:%d\n", ret);
			ret = 0;
		} else {
			lmh_data->odcm_enabled = true;
			for (; idx < LMH_ODCM_MAX_COUNT; idx++) {
				lmh_data->odcm_reg[idx] =
					devm_ioremap(&pdev->dev,
					lmh_hw_data->odcm_reg_addr[idx], 4);
				if (!lmh_data->odcm_reg[idx]) {
					pr_err("Err mapping ODCM memory 0x%x\n",
					lmh_hw_data->odcm_reg_addr[idx]);
					lmh_data->odcm_enabled = false;
					lmh_data->odcm_reg[0] = NULL;
					break;
				}
			}
		}
	}

	lmh_data->irq_num = platform_get_irq(pdev, 0);
	if (lmh_data->irq_num < 0) {
		ret = lmh_data->irq_num;
		pr_err("Error getting IRQ number. err:%d\n", ret);
		goto dev_exit;
	}

	ret = request_threaded_irq(lmh_data->irq_num, NULL,
		lmh_isr_thread, IRQF_TRIGGER_HIGH | IRQF_ONESHOT,
		LMH_INTERRUPT, lmh_data);
	if (ret) {
		pr_err("Error getting irq for LMH. err:%d\n", ret);
		goto dev_exit;
	}

	if (!lmh_data->trim_err_disable) {
		lmh_intr_base = platform_get_resource(pdev, IORESOURCE_MEM, 0);
		if (!lmh_intr_base) {
			ret = -EINVAL;
			pr_err("Error getting reg MEM for LMH.\n");
			goto dev_exit;
		}
		lmh_data->intr_addr =
			devm_ioremap(&pdev->dev, lmh_intr_base->start,
			resource_size(lmh_intr_base));
		if (!lmh_data->intr_addr) {
			ret = -ENODEV;
			pr_err("Error Mapping LMH memory address\n");
			goto dev_exit;
		}
	}

dev_exit:
	return ret;
}

static void lmh_remove_sensors(void)
{
	struct lmh_sensor_data *curr_sensor = NULL, *prev_sensor = NULL;

	down_write(&lmh_sensor_access);
	list_for_each_entry_safe(prev_sensor, curr_sensor, &lmh_sensor_list,
		list_ptr) {
		list_del(&prev_sensor->list_ptr);
		pr_debug("Deregistering Sensor:[%s]\n",
			prev_sensor->sensor_name);
		lmh_sensor_deregister(&prev_sensor->ops);
		devm_kfree(lmh_data->dev, prev_sensor);
	}
	up_write(&lmh_sensor_access);
}

static int lmh_check_tz_debug_cmds(void)
{
	LMH_CHECK_SCM_CMD(LMH_DEBUG_SET);
	LMH_CHECK_SCM_CMD(LMH_DEBUG_READ_BUF_SIZE);
	LMH_CHECK_SCM_CMD(LMH_DEBUG_READ);
	LMH_CHECK_SCM_CMD(LMH_DEBUG_GET_TYPE);

	return 0;
}

static int lmh_check_tz_dev_cmds(void)
{
	LMH_CHECK_SCM_CMD(LMH_CHANGE_PROFILE);
	LMH_CHECK_SCM_CMD(LMH_GET_PROFILES);

	return 0;
}

static int lmh_check_tz_sensor_cmds(void)
{
	LMH_CHECK_SCM_CMD(LMH_CTRL_QPMDA);
	if (!lmh_data->trim_err_disable)
		LMH_CHECK_SCM_CMD(LMH_TRIM_ERROR);
	LMH_CHECK_SCM_CMD(LMH_GET_INTENSITY);
	LMH_CHECK_SCM_CMD(LMH_GET_SENSORS);

	return 0;
}

static int lmh_parse_sensor(struct lmh_sensor_info *sens_info)
{
	int ret = 0, idx = 0, size = 0;
	struct lmh_sensor_data *lmh_sensor = NULL;

	lmh_sensor = devm_kzalloc(lmh_data->dev, sizeof(struct lmh_sensor_data),
			GFP_KERNEL);
	if (!lmh_sensor) {
		pr_err("No payload\n");
		return -ENOMEM;
	}
	size = sizeof(sens_info->name);
	size = min(size, LMH_NAME_MAX);
	memset(lmh_sensor->sensor_name, '\0', LMH_NAME_MAX);
	while (size--)
		lmh_sensor->sensor_name[idx++] = ((sens_info->name
				       & (0xFF << (size	* 8))) >> (size * 8));
	if (lmh_sensor->sensor_name[idx - 1] == '\0')
		idx--;
	lmh_sensor->sensor_name[idx++] = '_';
	size = sizeof(sens_info->node_id);
	if ((idx + size) > LMH_NAME_MAX)
		size -= LMH_NAME_MAX - idx - size - 1;
	while (size--)
		lmh_sensor->sensor_name[idx++] = ((sens_info->node_id
				       & (0xFF << (size * 8))) >> (size * 8));
	pr_info("Registering sensor:[%s]\n", lmh_sensor->sensor_name);
	lmh_sensor->ops.read = lmh_read;
	lmh_sensor->ops.disable_hw_log = lmh_disable_log;
	lmh_sensor->ops.enable_hw_log = lmh_enable_log;
	lmh_sensor->sensor_sw_id = lmh_data->max_sensor_count++;
	lmh_sensor->sensor_hw_name = sens_info->name;
	lmh_sensor->sensor_hw_node_id = sens_info->node_id;
	ret = lmh_sensor_register(lmh_sensor->sensor_name, &lmh_sensor->ops);
	if (ret) {
		pr_err("Sensor:[%s] registration failed. err:%d\n",
			lmh_sensor->sensor_name, ret);
		goto sens_exit;
	}
	list_add_tail(&lmh_sensor->list_ptr, &lmh_sensor_list);
	pr_debug("Registered sensor:[%s] driver\n", lmh_sensor->sensor_name);

sens_exit:
	if (ret)
		devm_kfree(lmh_data->dev, lmh_sensor);
	return ret;
}

static int lmh_get_sensor_list(void)
{
	int ret = 0, buf_size = 0;
	uint32_t size = 0, next = 0, idx = 0, count = 0;
	struct scm_desc desc_arg;
	struct lmh_sensor_packet *payload = NULL;
	struct {
		uint32_t addr;
		uint32_t size;
	} cmd_buf;

	buf_size = PAGE_ALIGN(sizeof(*payload));
	payload = kzalloc(buf_size, GFP_KERNEL);
	if (!payload)
		return -ENOMEM;

	do {
		memset(payload, 0, buf_size);
		payload->count = next;
		cmd_buf.addr = SCM_BUFFER_PHYS(payload);
		/* payload_phys may be a physical address > 4 GB */
		desc_arg.args[0] = SCM_BUFFER_PHYS(payload);
		desc_arg.args[1] = cmd_buf.size = SCM_BUFFER_SIZE(struct
				lmh_sensor_packet);
		desc_arg.arginfo = SCM_ARGS(2, SCM_RW, SCM_VAL);
		trace_lmh_event_call("GET_SENSORS enter");
		dmac_flush_range(payload, (void *)payload + buf_size);
		if (!is_scm_armv8())
			ret = scm_call(SCM_SVC_LMH, LMH_GET_SENSORS,
				(void *) &cmd_buf,
				SCM_BUFFER_SIZE(cmd_buf),
				NULL, 0);
		else
			ret = scm_call2(SCM_SIP_FNID(SCM_SVC_LMH,
				LMH_GET_SENSORS), &desc_arg);
		/* Have memory barrier before we access the TZ data */
		mb();
		trace_lmh_event_call("GET_SENSORS exit");
		dmac_inv_range(payload, (void *)payload + buf_size);
		if (ret < 0) {
			pr_err("Error in SCM v%d call. err:%d\n",
					(is_scm_armv8()) ? 8 : 7, ret);
			goto get_exit;
		}
		size = payload->count;
		if (!size) {
			pr_err("No LMH sensor supported\n");
			ret = -ENODEV;
			goto get_exit;
		}
		count = ((size - next) > LMH_MAX_SENSOR) ? LMH_MAX_SENSOR :
				(size - next);
		next += LMH_MAX_SENSOR;
		for (idx = 0; idx < count; idx++) {
			ret = lmh_parse_sensor(&payload->sensor[idx]);
			if (ret)
				goto get_exit;
		}
	} while (next < size);

get_exit:
	kfree(payload);
	return ret;
}

static int lmh_set_level(struct lmh_device_ops *ops, int level)
{
	int ret = 0, idx = 0;
	struct scm_desc desc_arg;
	struct lmh_profile *lmh_dev;

	if (level < 0 || !ops) {
		pr_err("Invalid Input\n");
		return -EINVAL;
	}
	lmh_dev = container_of(ops, struct lmh_profile, dev_ops);
	for (idx = 0; idx < lmh_dev->level_ct; idx++) {
		if (level != lmh_dev->levels[idx])
			continue;
		break;
	}
	if (idx == lmh_dev->level_ct) {
		pr_err("Invalid profile:[%d]\n", level);
		return -EINVAL;
	}
	desc_arg.args[0] = level;
	desc_arg.arginfo = SCM_ARGS(1, SCM_VAL);
	if (!is_scm_armv8())
		ret = scm_call_atomic1(SCM_SVC_LMH, LMH_CHANGE_PROFILE,
			level);
	else
		ret = scm_call2(SCM_SIP_FNID(SCM_SVC_LMH,
			LMH_CHANGE_PROFILE), &desc_arg);
	if (ret) {
		pr_err("Error in SCM v%d switching profile:[%d]. err:%d\n",
			(is_scm_armv8()) ? 8 : 7, level, ret);
		return ret;
	}
	pr_debug("Device:[%s] Current level:%d\n", LMH_DEVICE, level);
	lmh_dev->curr_level = level;

	return ret;

}

static int lmh_get_all_level(struct lmh_device_ops *ops, int *level)
{
	struct lmh_profile *lmh_dev;

	if (!ops) {
		pr_err("Invalid input\n");
		return -EINVAL;
	}
	lmh_dev = container_of(ops, struct lmh_profile, dev_ops);
	if (!level)
		return lmh_dev->level_ct;
	memcpy(level, lmh_dev->levels, lmh_dev->level_ct * sizeof(uint32_t));

	return 0;
}


static int lmh_get_level(struct lmh_device_ops *ops, int *level)
{
	struct lmh_profile *lmh_dev;

	if (!level || !ops) {
		pr_err("Invalid input\n");
		return -EINVAL;
	}
	lmh_dev = container_of(ops, struct lmh_profile, dev_ops);

	*level = lmh_dev->curr_level;

	return 0;
}

static int lmh_get_dev_info(void)
{
	int ret = 0;
	uint32_t size = 0, next = 0;
	struct scm_desc desc_arg;
	uint32_t *payload = NULL;
	struct {
		uint32_t list_addr;
		uint32_t list_size;
		uint32_t list_start;
	} cmd_buf;

	payload = devm_kzalloc(lmh_data->dev, sizeof(uint32_t) *
		LMH_GET_PROFILE_SIZE, GFP_KERNEL);
	if (!payload) {
		pr_err("No payload\n");
		ret = -ENOMEM;
		goto get_dev_exit;
	}

	cmd_buf.list_addr = SCM_BUFFER_PHYS(payload);
	/* &payload may be a physical address > 4 GB */
	desc_arg.args[0] = SCM_BUFFER_PHYS(payload);
	desc_arg.args[1] = cmd_buf.list_size =
		SCM_BUFFER_SIZE(uint32_t) * LMH_GET_PROFILE_SIZE;
	desc_arg.arginfo = SCM_ARGS(3, SCM_RW, SCM_VAL, SCM_VAL);
	LMH_GET_RECURSSIVE_DATA(desc_arg, 2, cmd_buf, payload, next, size,
		LMH_GET_PROFILES, lmh_data->dev_info.levels, ret);
	if (ret)
		goto get_dev_exit;
	lmh_data->dev_info.level_ct = size;
	lmh_data->dev_info.curr_level = LMH_DEFAULT_PROFILE;
	ret = lmh_set_level(&lmh_data->dev_info.dev_ops,
		lmh_hw_data->default_profile);
	if (ret) {
		pr_err("Error switching to default profile%d, err:%d\n",
			lmh_data->dev_info.curr_level, ret);
		goto get_dev_exit;
	}

get_dev_exit:
	if (ret)
		devm_kfree(lmh_data->dev, lmh_data->dev_info.levels);
	devm_kfree(lmh_data->dev, payload);
	return ret;
}

static int lmh_device_init(void)
{
	int ret = 0;

	if (lmh_check_tz_dev_cmds())
		return -ENODEV;

	ret = lmh_get_dev_info();
	if (ret)
		goto dev_init_exit;

	lmh_data->dev_info.dev_ops.get_available_levels = lmh_get_all_level;
	lmh_data->dev_info.dev_ops.get_curr_level = lmh_get_level;
	lmh_data->dev_info.dev_ops.set_level = lmh_set_level;
	ret = lmh_device_register(LMH_DEVICE, &lmh_data->dev_info.dev_ops);
	if (ret) {
		pr_err("Error registering device:[%s]. err:%d", LMH_DEVICE,
			ret);
		goto dev_init_exit;
	}

dev_init_exit:
	return ret;
}

static int lmh_debug_read(struct lmh_debug_ops *ops, uint32_t **buf)
{
	int ret = 0, size = 0, tz_ret = 0;
	static uint32_t curr_size;
	struct scm_desc desc_arg;
	static uint32_t *payload;
	struct {
		uint32_t buf_addr;
		uint32_t buf_size;
	} cmd_buf;

	desc_arg.arginfo = SCM_ARGS(0);
	trace_lmh_event_call("GET_DEBUG_READ_SIZE enter");
	if (!is_scm_armv8()) {
		ret = scm_call(SCM_SVC_LMH, LMH_DEBUG_READ_BUF_SIZE,
			NULL, 0, &size, SCM_BUFFER_SIZE(size));
	} else {
		ret = scm_call2(SCM_SIP_FNID(SCM_SVC_LMH,
			LMH_DEBUG_READ_BUF_SIZE), &desc_arg);
		size = desc_arg.ret[0];
	}
	trace_lmh_event_call("GET_DEBUG_READ_SIZE exit");
	if (ret) {
		pr_err("Error in SCM v%d get debug buffer size call. err:%d\n",
				(is_scm_armv8()) ? 8 : 7, ret);
		goto get_dbg_exit;
	}
	if (!size) {
		pr_err("No Debug data to read.\n");
		ret = -ENODEV;
		goto get_dbg_exit;
	}
	size = SCM_BUFFER_SIZE(uint32_t) * size * LMH_READ_LINE_LENGTH;
	if (curr_size != size) {
		if (payload)
			devm_kfree(lmh_data->dev, payload);
		payload = devm_kzalloc(lmh_data->dev, PAGE_ALIGN(size),
				       GFP_KERNEL);
		if (!payload) {
			pr_err("payload buffer alloc failed\n");
			ret = -ENOMEM;
			goto get_dbg_exit;
		}
		curr_size = size;
	}

	cmd_buf.buf_addr = SCM_BUFFER_PHYS(payload);
	/* &payload may be a physical address > 4 GB */
	desc_arg.args[0] = SCM_BUFFER_PHYS(payload);
	desc_arg.args[1] = cmd_buf.buf_size = curr_size;
	desc_arg.arginfo = SCM_ARGS(2, SCM_RW, SCM_VAL);
	trace_lmh_event_call("GET_DEBUG_READ enter");
	dmac_flush_range(payload, (void *)payload + curr_size);
	if (!is_scm_armv8()) {
		ret = scm_call(SCM_SVC_LMH, LMH_DEBUG_READ,
			(void *) &cmd_buf, SCM_BUFFER_SIZE(cmd_buf),
			&tz_ret, SCM_BUFFER_SIZE(tz_ret));
	} else {
		ret = scm_call2(SCM_SIP_FNID(SCM_SVC_LMH,
			LMH_DEBUG_READ), &desc_arg);
		tz_ret = desc_arg.ret[0];
	}
	/* Have memory barrier before we access the TZ data */
	mb();
	dmac_inv_range(payload, (void *)payload + curr_size);
	trace_lmh_event_call("GET_DEBUG_READ exit");
	if (ret) {
		pr_err("Error in SCM v%d get debug read. err:%d\n",
				(is_scm_armv8()) ? 8 : 7, ret);
		goto get_dbg_exit;
	}
	if (tz_ret) {
		pr_err("TZ API returned error. err:%d\n", tz_ret);
		ret = tz_ret;
		goto get_dbg_exit;
	}
	trace_lmh_debug_data("Debug read", payload,
		curr_size / sizeof(uint32_t));

get_dbg_exit:
	if (ret && payload) {
		devm_kfree(lmh_data->dev, payload);
		payload = NULL;
		curr_size = 0;
	}
	*buf = payload;

	return (ret < 0) ? ret : curr_size;
}

static int lmh_debug_config_write(uint32_t cmd_id, uint32_t *buf, int size)
{
	int ret = 0, size_bytes = 0;
	struct scm_desc desc_arg;
	uint32_t *payload = NULL;
	struct {
		uint32_t buf_addr;
		uint32_t buf_size;
		uint32_t node;
		uint32_t node_id;
		uint32_t read_type;
	} cmd_buf;

	trace_lmh_debug_data("Config LMH", buf, size);
	size_bytes = (size - 3) * sizeof(uint32_t);
	payload = devm_kzalloc(lmh_data->dev, PAGE_ALIGN(size_bytes),
			       GFP_KERNEL);
	if (!payload) {
		ret = -ENOMEM;
		goto set_cfg_exit;
	}
	memcpy(payload, &buf[3], size_bytes);

	cmd_buf.buf_addr = SCM_BUFFER_PHYS(payload);
	/* &payload may be a physical address > 4 GB */
	desc_arg.args[0] = SCM_BUFFER_PHYS(payload);
	desc_arg.args[1] = cmd_buf.buf_size = size_bytes;
	desc_arg.args[2] = cmd_buf.node = buf[0];
	desc_arg.args[3] = cmd_buf.node_id = buf[1];
	desc_arg.args[4] = cmd_buf.read_type = buf[2];
	desc_arg.arginfo = SCM_ARGS(5, SCM_RO, SCM_VAL, SCM_VAL, SCM_VAL,
					SCM_VAL);
	trace_lmh_event_call("CONFIG_DEBUG_WRITE enter");
	dmac_flush_range(payload, (void *)payload + size_bytes);
	if (!is_scm_armv8())
		ret = scm_call(SCM_SVC_LMH, cmd_id, (void *) &cmd_buf,
			SCM_BUFFER_SIZE(cmd_buf), NULL, 0);
	else
		ret = scm_call2(SCM_SIP_FNID(SCM_SVC_LMH, cmd_id), &desc_arg);
	/* Have memory barrier before we access the TZ data */
	mb();
	dmac_inv_range(payload, (void *)payload + size_bytes);
	trace_lmh_event_call("CONFIG_DEBUG_WRITE exit");
	if (ret) {
		pr_err("Error in SCM v%d config debug read. err:%d\n",
				(is_scm_armv8()) ? 8 : 7, ret);
		goto set_cfg_exit;
	}

set_cfg_exit:
	return ret;
}

static int lmh_debug_config_read(struct lmh_debug_ops *ops, uint32_t *buf,
	int size)
{
	return lmh_debug_config_write(LMH_DEBUG_SET, buf, size);
}

static int lmh_debug_get_types(struct lmh_debug_ops *ops, bool is_read,
	uint32_t **buf)
{
	int ret = 0;
	uint32_t size = 0, next = 0;
	struct scm_desc desc_arg;
	uint32_t *payload = NULL, *dest_buf = NULL;
	struct {
		uint32_t list_addr;
		uint32_t list_size;
		uint32_t cmd_type;
		uint32_t list_start;
	} cmd_buf;

	if (is_read && lmh_data->debug_info.read_type) {
		*buf = lmh_data->debug_info.read_type;
		trace_lmh_debug_data("Data type",
			lmh_data->debug_info.read_type,
			lmh_data->debug_info.read_type_count);
		return lmh_data->debug_info.read_type_count;
	} else if (!is_read && lmh_data->debug_info.config_type) {
		*buf = lmh_data->debug_info.config_type;
		trace_lmh_debug_data("Config type",
			lmh_data->debug_info.config_type,
			lmh_data->debug_info.config_type_count);
		return lmh_data->debug_info.config_type_count;
	}
	payload = devm_kzalloc(lmh_data->dev, sizeof(uint32_t) *
		LMH_SCM_PAYLOAD_SIZE, GFP_KERNEL);
	if (!payload) {
		ret = -ENOMEM;
		goto get_type_exit;
	}
	cmd_buf.list_addr = SCM_BUFFER_PHYS(payload);
	/* &payload may be a physical address > 4 GB */
	desc_arg.args[0] = SCM_BUFFER_PHYS(payload);
	desc_arg.args[1] = cmd_buf.list_size =
		SCM_BUFFER_SIZE(uint32_t) * LMH_SCM_PAYLOAD_SIZE;
	desc_arg.args[2] = cmd_buf.cmd_type = (is_read) ?
			LMH_DEBUG_READ_TYPE : LMH_DEBUG_CONFIG_TYPE;
	desc_arg.arginfo = SCM_ARGS(4, SCM_RW, SCM_VAL, SCM_VAL, SCM_VAL);
	LMH_GET_RECURSSIVE_DATA(desc_arg, 3, cmd_buf, payload, next, size,
		LMH_DEBUG_GET_TYPE, dest_buf, ret);
	if (ret)
		goto get_type_exit;
	pr_debug("Total %s types:%d\n", (is_read) ? "read" : "config", size);
	if (is_read) {
		lmh_data->debug_info.read_type = *buf = dest_buf;
		lmh_data->debug_info.read_type_count = size;
		trace_lmh_debug_data("Data type", dest_buf, size);
	} else {
		lmh_data->debug_info.config_type = *buf = dest_buf;
		lmh_data->debug_info.config_type_count = size;
		trace_lmh_debug_data("Config type", dest_buf, size);
	}

get_type_exit:
	if (ret) {
		devm_kfree(lmh_data->dev, lmh_data->debug_info.read_type);
		devm_kfree(lmh_data->dev, lmh_data->debug_info.config_type);
		lmh_data->debug_info.config_type_count = 0;
		lmh_data->debug_info.read_type_count = 0;
	}
	devm_kfree(lmh_data->dev, payload);
	return (ret) ? ret : size;
}

static int lmh_debug_lmh_config(struct lmh_debug_ops *ops, uint32_t *buf,
	int size)
{
	return lmh_debug_config_write(LMH_DEBUG_SET, buf, size);
}

static void lmh_voltage_scale_set(uint32_t voltage)
{
	char trace_buf[MAX_TRACE_EVENT_MSG_LEN] = "";

	mutex_lock(&scm_lmh_lock);
	writel_relaxed(voltage, lmh_data->dpm_voltage_scale_reg);
	mutex_unlock(&scm_lmh_lock);
	snprintf(trace_buf, MAX_TRACE_EVENT_MSG_LEN,
		"DPM voltage scale %d mV", voltage);
	pr_debug("%s\n", trace_buf);
	trace_lmh_event_call(trace_buf);
}

static void write_to_odcm(bool enable)
{
	uint32_t idx = 0, data = enable ? 1 : 0;

	for (; idx < LMH_ODCM_MAX_COUNT; idx++)
		writel_relaxed(data, lmh_data->odcm_reg[idx]);
}

static void evaluate_and_config_odcm(uint32_t rail_uV, unsigned long state)
{
	uint32_t rail_mV = rail_uV / 1000;
	static bool prev_state, disable_odcm;

	mutex_lock(&lmh_odcm_access);
	switch (state) {
	case REGULATOR_EVENT_VOLTAGE_CHANGE:
		if (!disable_odcm)
			break;
		pr_debug("Disable ODCM\n");
		write_to_odcm(false);
		lmh_data->odcm_enabled = false;
		disable_odcm = false;
		break;
	case REGULATOR_EVENT_PRE_VOLTAGE_CHANGE:
		disable_odcm = false;
		prev_state = lmh_data->odcm_enabled;
		if (rail_mV > lmh_data->odcm_thresh_mV) {
			if (lmh_data->odcm_enabled)
				break;
			/* Enable ODCM before the voltage increases */
			pr_debug("Enable ODCM for voltage %u mV\n", rail_mV);
			write_to_odcm(true);
			lmh_data->odcm_enabled = true;
		} else {
			if (!lmh_data->odcm_enabled)
				break;
			/* Disable ODCM after the voltage decreases */
			pr_debug("Disable ODCM for voltage %u mV\n", rail_mV);
			disable_odcm = true;
		}
		break;
	case REGULATOR_EVENT_ABORT_VOLTAGE_CHANGE:
		disable_odcm = false;
		if (prev_state == lmh_data->odcm_enabled)
			break;
		pr_debug("Reverting ODCM state to %s\n",
			prev_state ? "enabled" : "disabled");
		write_to_odcm(prev_state);
		lmh_data->odcm_enabled = prev_state;
		break;
	default:
		break;
	}
	mutex_unlock(&lmh_odcm_access);
}

static int lmh_voltage_change_notifier(struct notifier_block *nb_data,
	unsigned long event, void *data)
{
	uint32_t voltage = 0;
	static uint32_t last_voltage;
	static bool change_needed;

	if (event == REGULATOR_EVENT_VOLTAGE_CHANGE) {
		/* Convert from uV to mV */
		pr_debug("Received event POST_VOLTAGE_CHANGE\n");
		voltage = ((unsigned long)data) / 1000;
		if (change_needed == 1 &&
			(last_voltage == voltage)) {
			lmh_voltage_scale_set(voltage);
			change_needed = 0;
		}
		if (lmh_data->odcm_reg[0])
			evaluate_and_config_odcm(0, event);
	} else if (event == REGULATOR_EVENT_PRE_VOLTAGE_CHANGE) {
		struct pre_voltage_change_data *change_data =
			(struct pre_voltage_change_data *)data;
		last_voltage = change_data->min_uV / 1000;
		if (change_data->min_uV > change_data->old_uV)
			/* Going from low to high apply change first */
			lmh_voltage_scale_set(last_voltage);
		else
			/* Going from high to low apply change after */
			change_needed = 1;
		pr_debug("Received event PRE_VOLTAGE_CHANGE\n");
		pr_debug("max = %lu mV min = %lu mV previous = %lu mV\n",
			change_data->max_uV / 1000, change_data->min_uV / 1000,
			change_data->old_uV / 1000);

		if (lmh_data->odcm_reg[0])
			evaluate_and_config_odcm(change_data->max_uV, event);
	} else if (event == REGULATOR_EVENT_ABORT_VOLTAGE_CHANGE) {
		pr_debug("Received event ABORT_VOLTAGE_CHANGE\n");
		if (lmh_data->odcm_reg[0])
			evaluate_and_config_odcm(0, event);
	}

	return NOTIFY_OK;
}

static void lmh_dpm_remove(void)
{
	if (!IS_ERR_OR_NULL(lmh_data->regulator) &&
		lmh_data->dpm_notifier_blk.notifier_call != NULL) {
		regulator_unregister_notifier(lmh_data->regulator,
			&(lmh_data->dpm_notifier_blk));
		lmh_data->regulator = NULL;
	}
}

static void lmh_dpm_init(void)
{
	int ret = 0;

	lmh_data->dpm_voltage_scale_reg = devm_ioremap(lmh_data->dev,
			(phys_addr_t)APCS_DPM_VOLTAGE_SCALE, 4);
	if (!lmh_data->dpm_voltage_scale_reg) {
		ret = -ENODEV;
		pr_err("Error mapping LMH DPM voltage scale register\n");
		goto dpm_init_exit;
	}

	lmh_data->dpm_notifier_blk.notifier_call = lmh_voltage_change_notifier;
	ret = regulator_register_notifier(lmh_data->regulator,
		&(lmh_data->dpm_notifier_blk));
	if (ret) {
		pr_err("DPM regulator notification registration failed. err:%d\n",
			ret);
		goto dpm_init_exit;
	}

dpm_init_exit:
	if (ret) {
		if (lmh_data->dpm_notifier_blk.notifier_call)
			regulator_unregister_notifier(lmh_data->regulator,
				&(lmh_data->dpm_notifier_blk));
		devm_regulator_put(lmh_data->regulator);
		lmh_data->dpm_notifier_blk.notifier_call = NULL;
		lmh_data->regulator = NULL;
	}
}


static int lmh_debug_init(void)
{
	int ret = 0;

	if (lmh_check_tz_debug_cmds()) {
		pr_debug("Debug commands not available.\n");
		return -ENODEV;
	}

	lmh_data->debug_info.debug_ops.debug_read = lmh_debug_read;
	lmh_data->debug_info.debug_ops.debug_config_read
		= lmh_debug_config_read;
	lmh_data->debug_info.debug_ops.debug_config_lmh
		= lmh_debug_lmh_config;
	lmh_data->debug_info.debug_ops.debug_get_types
		= lmh_debug_get_types;
	ret = lmh_debug_register(&lmh_data->debug_info.debug_ops);
	if (ret) {
		pr_err("Error registering debug ops. err:%d\n", ret);
		goto debug_init_exit;
	}

debug_init_exit:
	return ret;
}
static int lmh_sensor_init(struct platform_device *pdev)
{
	int ret = 0;

	if (lmh_check_tz_sensor_cmds())
		return -ENODEV;

	down_write(&lmh_sensor_access);
	ret = lmh_get_sensor_list();
	if (ret)
		goto init_exit;

	lmh_data->intr_state = LMH_ISR_MONITOR;

	ret = lmh_get_sensor_devicetree(pdev);
	if (ret) {
		pr_err("Error getting device tree data. err:%d\n", ret);
		goto init_exit;
	}
	pr_debug("LMH Sensor Init complete\n");

init_exit:
	up_write(&lmh_sensor_access);
	if (ret)
		lmh_remove_sensors();

	return ret;
}

static int lmh_probe(struct platform_device *pdev)
{
	int ret = 0;

	if (lmh_data) {
		pr_err("Reinitializing lmh hardware driver\n");
		return -EEXIST;
	}
	lmh_data = devm_kzalloc(&pdev->dev, sizeof(struct lmh_driver_data),
					GFP_KERNEL);
	if (!lmh_data) {
		pr_err("kzalloc failed\n");
		return -ENOMEM;
	}
	lmh_data->dev = &pdev->dev;

	lmh_data->poll_wq = alloc_workqueue("lmh_poll_wq", WQ_HIGHPRI, 0);
	if (!lmh_data->poll_wq) {
		pr_err("Error allocating workqueue\n");
		ret = -ENOMEM;
		goto probe_exit;
	}
	INIT_DEFERRABLE_WORK(&lmh_data->poll_work, lmh_poll);

	ret = lmh_sensor_init(pdev);
	if (ret) {
		pr_err("Sensor Init failed. err:%d\n", ret);
		goto probe_exit;
	}
	ret = lmh_device_init();
	if (ret) {
		pr_err("WARNING: Device Init failed. err:%d. LMH continues\n",
			ret);
		ret = 0;
	}

	if (lmh_data->regulator)
		lmh_dpm_init();

	ret = lmh_debug_init();
	if (ret) {
		pr_err("LMH debug init failed. err:%d\n", ret);
		ret = 0;
	}
	platform_set_drvdata(pdev, lmh_data);

	return ret;

probe_exit:
	if (lmh_data->poll_wq)
		destroy_workqueue(lmh_data->poll_wq);
	lmh_data = NULL;
	return ret;
}

static int lmh_remove(struct platform_device *pdev)
{
	struct lmh_driver_data *lmh_dat = platform_get_drvdata(pdev);

	destroy_workqueue(lmh_dat->poll_wq);
	free_irq(lmh_dat->irq_num, lmh_dat);
	lmh_remove_sensors();
	lmh_device_deregister(&lmh_dat->dev_info.dev_ops);
	lmh_dpm_remove();

	return 0;
}

static struct of_device_id lmh_match[] = {
	{
		.compatible = "qcom,lmh",
		.data = (void *)&lmh_lite_data,
	},
	{
		.compatible = "qcom,lmh_v1",
		.data = (void *)&lmh_v1_data,
	},
	{},
};

static struct platform_driver lmh_driver = {
	.probe  = lmh_probe,
	.remove = lmh_remove,
	.driver = {
		.name           = LMH_DRIVER_NAME,
		.owner          = THIS_MODULE,
		.of_match_table = lmh_match,
	},
};

int __init lmh_init_driver(void)
{
	struct device_node *comp_node;

	comp_node = of_find_matching_node(NULL, lmh_match);
	if (comp_node) {
		const struct of_device_id *match = of_match_node(lmh_match,
							comp_node);
		if (!match) {
			pr_err("Couldnt find a match\n");
			goto plt_register;
		}
		lmh_hw_data = (struct lmh_default_data *)match->data;
		of_node_put(comp_node);
	}

plt_register:
	return platform_driver_register(&lmh_driver);
}

static void __exit lmh_exit(void)
{
	platform_driver_unregister(&lmh_driver);
}

late_initcall(lmh_init_driver);
module_exit(lmh_exit);

MODULE_DESCRIPTION("LMH hardware interface");
MODULE_ALIAS("platform:" LMH_DRIVER_NAME);
MODULE_LICENSE("GPL v2");
