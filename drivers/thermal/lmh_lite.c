/* Copyright (c) 2014, The Linux Foundation. All rights reserved.
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

#define LMH_DRIVER_NAME			"lmh-lite-driver"
#define LMH_INTERRUPT			"lmh-interrupt"
#define LMH_DEVICE			"lmh-profile"
#define LMH_MAX_SENSOR			10
#define LMH_GET_PROFILE_SIZE		10
#define LMH_DEFAULT_PROFILE		0
#define LMH_CHANGE_PROFILE		0x01
#define LMH_GET_PROFILES		0x02
#define LMH_CTRL_QPMDA			0x03
#define LMH_TRIM_ERROR			0x04
#define LMH_GET_INTENSITY		0x06
#define LMH_GET_SENSORS			0x07

#define LMH_CHECK_SCM_CMD(_cmd) \
	do { \
		if (!scm_is_call_available(SCM_SVC_LMH, _cmd)) { \
			pr_err("SCM cmd:%d not available\n", _cmd); \
			return -ENODEV; \
		} \
	} while (0)

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
};

struct lmh_driver_data {
	struct device			*dev;
	struct workqueue_struct		*isr_wq;
	struct work_struct		isr_work;
	struct delayed_work		poll_work;
	uint32_t			log_enabled;
	uint32_t			log_delay;
	enum lmh_monitor_state		intr_state;
	uint32_t			intr_reg_val;
	uint32_t			intr_status_val;
	uint32_t			trim_err_offset;
	void				*intr_addr;
	int				irq_num;
	int				max_sensor_count;
	struct lmh_profile		dev_info;
};

struct lmh_sensor_data {
	char				sensor_name[LMH_NAME_MAX];
	uint32_t			sensor_hw_name;
	uint32_t			sensor_hw_node_id;
	int				sensor_sw_id;
	struct lmh_sensor_ops		ops;
	enum lmh_monitor_state		state;
	long				last_read_value;
	struct list_head		list_ptr;
};

static struct lmh_driver_data		*lmh_data;
static DECLARE_RWSEM(lmh_sensor_access);
static DEFINE_MUTEX(lmh_sensor_read);
static LIST_HEAD(lmh_sensor_list);

static int lmh_read(struct lmh_sensor_ops *ops, long *val)
{
	struct lmh_sensor_data *lmh_sensor = container_of(ops,
		       struct lmh_sensor_data, ops);

	*val = lmh_sensor->last_read_value;
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
	if (!is_scm_armv8())
		ret = scm_call(SCM_SVC_LMH, LMH_CTRL_QPMDA,
			(void *) &cmd_buf, SCM_BUFFER_SIZE(cmd_buf), NULL, 0);
	else
		ret = scm_call2(SCM_SIP_FNID(SCM_SVC_LMH,
			LMH_CTRL_QPMDA), &desc_arg);
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

static int lmh_reset(struct lmh_sensor_ops *ops)
{
	int ret = 0;
	struct lmh_sensor_data *lmh_sensor = container_of(ops,
		       struct lmh_sensor_data, ops);

	down_write(&lmh_sensor_access);
	if (lmh_data->intr_status_val & BIT(lmh_sensor->sensor_sw_id)) {
		lmh_data->intr_status_val ^= BIT(lmh_sensor->sensor_sw_id);
		lmh_sensor->state = LMH_ISR_MONITOR;
		pr_debug("Sensor:[%s] not throttling. Switch to monitor mode\n",
			       lmh_sensor->sensor_name);
	} else {
		goto reset_exit;
	}

	if (!lmh_data->intr_status_val)
		lmh_data->intr_state = LMH_ISR_MONITOR;

reset_exit:
	up_write(&lmh_sensor_access);
	if (!lmh_data->intr_status_val) {
		/* cancel the poll work after releasing the lock to avoid
		** deadlock situation */
		pr_debug("Zero throttling. Re-enabling interrupt\n");
		cancel_delayed_work_sync(&lmh_data->poll_work);
		enable_irq(lmh_data->irq_num);
	}
	return ret;
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
	payload.count = 0;
	desc_arg.args[0] = cmd_buf.addr = SCM_BUFFER_PHYS(&payload);
	desc_arg.args[1] = cmd_buf.size
			= SCM_BUFFER_SIZE(struct lmh_sensor_packet);
	desc_arg.arginfo = SCM_ARGS(2, SCM_RW, SCM_VAL);
	dmac_flush_range(&payload, &payload + sizeof(struct lmh_sensor_packet));
	if (!is_scm_armv8())
		ret = scm_call(SCM_SVC_LMH, LMH_GET_INTENSITY,
			(void *) &cmd_buf, SCM_BUFFER_SIZE(cmd_buf), NULL, 0);
	else
		ret = scm_call2(SCM_SIP_FNID(SCM_SVC_LMH,
			LMH_GET_INTENSITY), &desc_arg);
	if (ret) {
		pr_err("Error in SCM v%d read call. err:%d\n",
				(is_scm_armv8()) ? 8 : 7, ret);
		goto read_exit;
	}
	dmac_inv_range(&payload, &payload + sizeof(struct lmh_sensor_packet));

	list_for_each_entry(lmh_sensor, &lmh_sensor_list, list_ptr)
		lmh_sensor->last_read_value = 0;
	for (idx = 0; idx < payload.count; idx++) {
		list_for_each_entry(lmh_sensor, &lmh_sensor_list, list_ptr) {

			if (payload.sensor[idx].name
				== lmh_sensor->sensor_hw_name
				&& (payload.sensor[idx].node_id
				== lmh_sensor->sensor_hw_node_id)) {

				lmh_sensor->last_read_value =
					payload.sensor[idx].intensity;
				break;
			}
		}
	}

read_exit:
	mutex_unlock(&lmh_sensor_read);
	return;
}

static void lmh_read_and_notify(struct lmh_driver_data *lmh_dat)
{
	struct lmh_sensor_data *lmh_sensor = NULL;
	long val;

	lmh_read_and_update(lmh_dat);
	list_for_each_entry(lmh_sensor, &lmh_sensor_list, list_ptr) {
		val = lmh_sensor->last_read_value;
		if (val > 0 && !(lmh_dat->intr_status_val
			& BIT(lmh_sensor->sensor_sw_id))) {
			pr_debug("Sensor:[%s] interrupt triggered\n",
				lmh_sensor->sensor_name);
			lmh_dat->intr_status_val
			       |= BIT(lmh_sensor->sensor_sw_id);
			lmh_sensor->state = LMH_ISR_POLLING;
			lmh_sensor->ops.interrupt_notify(&lmh_sensor->ops, val);
		}
	}

}

static void lmh_poll(struct work_struct *work)
{
	struct lmh_driver_data *lmh_dat = container_of(work,
		       struct lmh_driver_data, poll_work.work);

	down_write(&lmh_sensor_access);
	if (lmh_dat->intr_state != LMH_ISR_POLLING)
		goto poll_exit;
	lmh_read_and_notify(lmh_dat);
	schedule_delayed_work(&lmh_dat->poll_work,
			msecs_to_jiffies(lmh_poll_interval));

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
	if (!is_scm_armv8())
		ret = scm_call(SCM_SVC_LMH, LMH_TRIM_ERROR, NULL, 0, NULL, 0);
	else
		ret = scm_call2(SCM_SIP_FNID(SCM_SVC_LMH,
			LMH_TRIM_ERROR), &desc_arg);
	if (ret)
		pr_err("Error in SCM v%d trim error call. err:%d\n",
					(is_scm_armv8()) ? 8 : 7, ret);

	return;
}

static void lmh_notify(struct work_struct *work)
{
	struct lmh_driver_data *lmh_dat;

	down_write(&lmh_sensor_access);
	lmh_dat = container_of(work, struct lmh_driver_data, isr_work);
	lmh_dat->intr_reg_val = readl_relaxed(lmh_dat->intr_addr);
	pr_debug("Lmh hw interrupt:%d\n", lmh_dat->intr_reg_val);
	if (lmh_dat->intr_reg_val & BIT(lmh_dat->trim_err_offset)) {
		lmh_trim_error();
		lmh_dat->intr_state = LMH_ISR_MONITOR;
		goto notify_exit;
	}
	lmh_read_and_notify(lmh_dat);
	if (!lmh_dat->intr_status_val) {
		pr_debug("LMH not throttling. Enabling interrupt\n");
		lmh_dat->intr_state = LMH_ISR_MONITOR;
		goto notify_exit;
	}

notify_exit:
	if (lmh_dat->intr_state == LMH_ISR_POLLING)
		schedule_delayed_work(&lmh_dat->poll_work,
			msecs_to_jiffies(lmh_poll_interval));
	else
		enable_irq(lmh_dat->irq_num);
	up_write(&lmh_sensor_access);
	return;
}

static irqreturn_t lmh_handle_isr(int irq, void *data)
{
	struct lmh_driver_data *lmh_dat = (struct lmh_driver_data *)data;

	pr_debug("LMH Interrupt triggered\n");
	if (lmh_dat->intr_state == LMH_ISR_MONITOR) {
		disable_irq_nosync(lmh_dat->irq_num);
		lmh_dat->intr_state = LMH_ISR_POLLING;
		queue_work(lmh_dat->isr_wq, &lmh_dat->isr_work);
	}

	return IRQ_HANDLED;
}

static int lmh_get_sensor_devicetree(struct platform_device *pdev)
{
	int ret = 0;
	char *key = NULL;
	struct device_node *node = pdev->dev.of_node;
	struct resource *lmh_intr_base = NULL;

	key = "qcom,lmh-trim-err-offset";
	ret = of_property_read_u32(node, key,
			&lmh_data->trim_err_offset);
	if (ret) {
		pr_err("Error reading:%s. err:%d\n", key, ret);
		goto dev_exit;
	}

	lmh_data->irq_num = platform_get_irq(pdev, 0);
	if (lmh_data->irq_num < 0) {
		ret = lmh_data->irq_num;
		pr_err("Error getting IRQ number. err:%d\n", ret);
		goto dev_exit;
	}

	ret = request_irq(lmh_data->irq_num, lmh_handle_isr,
				IRQF_TRIGGER_HIGH, LMH_INTERRUPT, lmh_data);
	if (ret) {
		pr_err("Error getting irq for LMH. err:%d\n", ret);
		goto dev_exit;
	}

	lmh_intr_base = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!lmh_intr_base) {
		ret = -EINVAL;
		pr_err("Error getting reg MEM for LMH.\n");
		goto dev_exit;
	}
	lmh_data->intr_addr = devm_ioremap(&pdev->dev, lmh_intr_base->start,
					resource_size(lmh_intr_base));
	if (!lmh_data->intr_addr) {
		ret = -ENODEV;
		pr_err("Error Mapping LMH memory address\n");
		goto dev_exit;
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

static int lmh_check_tz_dev_cmds(void)
{
	LMH_CHECK_SCM_CMD(LMH_CHANGE_PROFILE);
	LMH_CHECK_SCM_CMD(LMH_GET_PROFILES);

	return 0;
}

static int lmh_check_tz_sensor_cmds(void)
{
	LMH_CHECK_SCM_CMD(LMH_CTRL_QPMDA);
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
	lmh_sensor->ops.reset_interrupt = lmh_reset;
	lmh_sensor->state = LMH_ISR_MONITOR;
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
	int ret = 0;
	uint32_t size = 0, next = 0, idx = 0, count = 0;
	struct scm_desc desc_arg;
	struct lmh_sensor_packet *payload = NULL;
	struct {
		uint32_t addr;
		uint32_t size;
	} cmd_buf;

	payload = devm_kzalloc(lmh_data->dev, sizeof(struct lmh_sensor_packet),
		GFP_KERNEL);
	if (!payload) {
		pr_err("No payload\n");
		ret = -ENOMEM;
		goto get_exit;
	}

	do {
		payload->count = next;
		desc_arg.args[0] = cmd_buf.addr = SCM_BUFFER_PHYS(payload);
		desc_arg.args[1] = cmd_buf.size = SCM_BUFFER_SIZE(struct
				lmh_sensor_packet);
		desc_arg.arginfo = SCM_ARGS(2, SCM_RW, SCM_VAL);
		dmac_flush_range(payload, payload
			       + sizeof(struct lmh_sensor_packet));
		if (!is_scm_armv8())
			ret = scm_call(SCM_SVC_LMH, LMH_GET_SENSORS,
				(void *) &cmd_buf,
				SCM_BUFFER_SIZE(cmd_buf),
				NULL, 0);
		else
			ret = scm_call2(SCM_SIP_FNID(SCM_SVC_LMH,
				LMH_GET_SENSORS), &desc_arg);
		if (ret < 0) {
			pr_err("Error in SCM v%d call. err:%d\n",
					(is_scm_armv8()) ? 8 : 7, ret);
			goto get_exit;
		}
		dmac_inv_range(payload, payload
			       + sizeof(struct lmh_sensor_packet));
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
	devm_kfree(lmh_data->dev, payload);
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
	uint32_t size = 0, next = 0, idx = 0;
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

	do {
		desc_arg.args[0] = cmd_buf.list_addr = SCM_BUFFER_PHYS(payload);
		desc_arg.args[1] = cmd_buf.list_size =
			SCM_BUFFER_SIZE(uint32_t) * LMH_GET_PROFILE_SIZE;
		desc_arg.args[2] = cmd_buf.list_start = next;
		desc_arg.arginfo = SCM_ARGS(3, SCM_RW, SCM_VAL, SCM_VAL);
		dmac_flush_range(payload, payload + sizeof(uint32_t) *
			LMH_GET_PROFILE_SIZE);
		if (!is_scm_armv8()) {
			ret = scm_call(SCM_SVC_LMH, LMH_GET_PROFILES,
				(void *) &cmd_buf, SCM_BUFFER_SIZE(cmd_buf),
				&size, SCM_BUFFER_SIZE(size));
		} else {
			ret = scm_call2(SCM_SIP_FNID(SCM_SVC_LMH,
				LMH_GET_PROFILES), &desc_arg);
			size = desc_arg.ret[0];
		}
		if (ret) {
			pr_err("Error in SCM v%d get Profile call. err:%d\n",
					(is_scm_armv8()) ? 8 : 7, ret);
			goto get_dev_exit;
		}
		if (!size) {
			pr_err("No LMH device supported.\n");
			ret = -ENODEV;
			goto get_dev_exit;
		}
		dmac_inv_range(payload, payload + sizeof(uint32_t) *
				LMH_GET_PROFILE_SIZE);
		if (!lmh_data->dev_info.levels) {
			lmh_data->dev_info.levels = devm_kzalloc(lmh_data->dev,
				sizeof(uint32_t) * size, GFP_KERNEL);
			if (!lmh_data->dev_info.levels) {
				pr_err("No Memory\n");
				ret = -ENOMEM;
				goto get_dev_exit;
			}
			lmh_data->dev_info.level_ct = size;
			lmh_data->dev_info.curr_level = LMH_DEFAULT_PROFILE;
		}
		for (idx = next; idx < min((next + LMH_GET_PROFILE_SIZE), size);
			idx++)
			lmh_data->dev_info.levels[idx] = payload[idx - next];
		next += LMH_GET_PROFILE_SIZE;
	} while (next < size);

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
	if (ret)
		lmh_remove_sensors();
	up_write(&lmh_sensor_access);

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

	lmh_data->isr_wq = alloc_workqueue("lmh_isr_wq", WQ_HIGHPRI, 0);
	if (!lmh_data->isr_wq) {
		pr_err("Error allocating workqueue\n");
		ret = -ENOMEM;
		goto probe_exit;
	}
	INIT_WORK(&lmh_data->isr_work, lmh_notify);
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
	platform_set_drvdata(pdev, lmh_data);

	return ret;

probe_exit:
	if (lmh_data->isr_wq)
		destroy_workqueue(lmh_data->isr_wq);
	lmh_data = NULL;
	return ret;
}

static int lmh_remove(struct platform_device *pdev)
{
	struct lmh_driver_data *lmh_dat = platform_get_drvdata(pdev);

	cancel_delayed_work_sync(&lmh_dat->poll_work);
	destroy_workqueue(lmh_dat->isr_wq);
	free_irq(lmh_dat->irq_num, lmh_dat);
	lmh_remove_sensors();
	lmh_device_deregister(&lmh_dat->dev_info.dev_ops);

	return 0;
}

static struct of_device_id lmh_match[] = {
	{
		.compatible = "qcom,lmh",
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
