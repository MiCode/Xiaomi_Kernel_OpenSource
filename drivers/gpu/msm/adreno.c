/* Copyright (c) 2002,2007-2014, The Linux Foundation. All rights reserved.
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
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/ioctl.h>
#include <linux/sched.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/delay.h>
#include <linux/of_coresight.h>
#include <linux/input.h>

#include <linux/msm-bus-board.h>
#include <linux/msm-bus.h>

#include "kgsl.h"
#include "kgsl_pwrscale.h"
#include "kgsl_cffdump.h"
#include "kgsl_sharedmem.h"
#include "kgsl_iommu.h"
#include "kgsl_compat.h"
#include "kgsl_trace.h"

#include "adreno.h"
#include "adreno_pm4types.h"
#include "adreno_trace.h"

#include "a3xx_reg.h"
#include "adreno_a3xx_snapshot.h"

/* Include the master list of GPU cores that are supported */
#include "adreno-gpulist.h"

#define DRIVER_VERSION_MAJOR   3
#define DRIVER_VERSION_MINOR   1

/* Number of times to try hard reset */
#define NUM_TIMES_RESET_RETRY 5

#define KGSL_LOG_LEVEL_DEFAULT 3

static void adreno_input_work(struct work_struct *work);

static struct devfreq_msm_adreno_tz_data adreno_tz_data = {
	.bus = {
		.max = 350,
	},
	.device_id = KGSL_DEVICE_3D0,
};

static const struct kgsl_functable adreno_functable;

static struct adreno_device device_3d0 = {
	.dev = {
		KGSL_DEVICE_COMMON_INIT(device_3d0.dev),
		.pwrscale = KGSL_PWRSCALE_INIT(&adreno_tz_data),
		.name = DEVICE_3D0_NAME,
		.id = KGSL_DEVICE_3D0,
		.pwrctrl = {
			.irq_name = KGSL_3D0_IRQ,
		},
		.iomemname = KGSL_3D0_REG_MEMORY,
		.shadermemname = KGSL_3D0_SHADER_MEMORY,
		.ftbl = &adreno_functable,
		.cmd_log = KGSL_LOG_LEVEL_DEFAULT,
		.ctxt_log = KGSL_LOG_LEVEL_DEFAULT,
		.drv_log = KGSL_LOG_LEVEL_DEFAULT,
		.mem_log = KGSL_LOG_LEVEL_DEFAULT,
		.pwr_log = KGSL_LOG_LEVEL_DEFAULT,
	},
	.gmem_size = SZ_256K,
	.pfp_fw = NULL,
	.pm4_fw = NULL,
	.wait_timeout = 0, /* in milliseconds, 0 means disabled */
	.ib_check_level = 0,
	.ft_policy = KGSL_FT_DEFAULT_POLICY,
	.ft_pf_policy = KGSL_FT_PAGEFAULT_DEFAULT_POLICY,
	.fast_hang_detect = 1,
	.long_ib_detect = 1,
	.input_work = __WORK_INITIALIZER(device_3d0.input_work,
		adreno_input_work),
	.pwrctrl_flag = BIT(ADRENO_SPTP_PC_CTRL),
};

/* Ptr to array for the current set of fault detect registers */
unsigned int *adreno_ft_regs;
/* Total number of fault detect registers */
unsigned int adreno_ft_regs_num;
/* Ptr to array for the current fault detect registers values */
unsigned int *adreno_ft_regs_val;
/* Array of default fault detect registers */
static unsigned int adreno_ft_regs_default[] = {
	ADRENO_REG_RBBM_STATUS,
	ADRENO_REG_CP_RB_RPTR,
	ADRENO_REG_CP_IB1_BASE,
	ADRENO_REG_CP_IB1_BUFSZ,
	ADRENO_REG_CP_IB2_BASE,
	ADRENO_REG_CP_IB2_BUFSZ
};

/* Nice level for the higher priority GPU start thread */
static int _wake_nice = -7;

/* Number of milliseconds to stay active active after a wake on touch */
static unsigned int _wake_timeout = 100;


static int _get_counter(struct adreno_device *adreno_dev,
		int group, int countable, unsigned int *lo,
		unsigned int *hi)
{
	int ret = 0;

	if (*lo == 0) {

		ret = adreno_perfcounter_get(adreno_dev, group, countable,
			lo, hi, PERFCOUNTER_FLAG_KERNEL);

		if (ret) {
			struct kgsl_device *device = &adreno_dev->dev;

			KGSL_DRV_ERR(device,
				"Unable to allocate fault detect performance counter %d/%d\n",
				group, countable);
			KGSL_DRV_ERR(device,
				"GPU fault detect will be less reliable\n");
		}
	}

	return ret;
}

static inline void _put_counter(struct adreno_device *adreno_dev,
		int group, int countable, unsigned int *lo,
		unsigned int *hi)
{
	if (*lo != 0)
		adreno_perfcounter_put(adreno_dev, group, countable,
			PERFCOUNTER_FLAG_KERNEL);

	*lo = 0;
	*hi = 0;
}

/**
 * adreno_fault_detect_start() - Allocate performance counters
 * used for fast fault detection
 * @adreno_dev: Pointer to an adreno_device structure
 *
 * Allocate the series of performance counters that should be periodically
 * checked to verify that the GPU is still moving
 */
void adreno_fault_detect_start(struct adreno_device *adreno_dev)
{
	struct adreno_gpudev *gpudev = ADRENO_GPU_DEVICE(adreno_dev);
	unsigned int i, j = ARRAY_SIZE(adreno_ft_regs_default);

	for (i = 0; i < gpudev->ft_perf_counters_count; i++) {
		_get_counter(adreno_dev, gpudev->ft_perf_counters[i].counter,
			 gpudev->ft_perf_counters[i].countable,
			 &adreno_ft_regs[j + (i * 2)],
			 &adreno_ft_regs[j + ((i * 2) + 1)]);
	}
}

/**
 * adreno_fault_detect_stop() - Release performance counters
 * used for fast fault detection
 * @adreno_dev: Pointer to an adreno_device structure
 *
 * Release the counters allocated in adreno_fault_detect_start
 */
void adreno_fault_detect_stop(struct adreno_device *adreno_dev)
{
	struct adreno_gpudev *gpudev = ADRENO_GPU_DEVICE(adreno_dev);
	unsigned int i, j = ARRAY_SIZE(adreno_ft_regs_default);

	for (i = 0; i < gpudev->ft_perf_counters_count; i++) {
		_put_counter(adreno_dev, gpudev->ft_perf_counters[i].counter,
			 gpudev->ft_perf_counters[i].countable,
			 &adreno_ft_regs[j + (i * 2)],
			 &adreno_ft_regs[j + ((i * 2) + 1)]);

	}
}

/*
 * A workqueue callback responsible for actually turning on the GPU after a
 * touch event. kgsl_pwrctrl_change_state(ACTIVE) is used without any
 * active_count protection to avoid the need to maintain state.  Either
 * somebody will start using the GPU or the idle timer will fire and put the
 * GPU back into slumber.
 */
static void adreno_input_work(struct work_struct *work)
{
	struct adreno_device *adreno_dev = container_of(work,
			struct adreno_device, input_work);
	struct kgsl_device *device = &adreno_dev->dev;

	mutex_lock(&device->mutex);

	device->flags |= KGSL_FLAG_WAKE_ON_TOUCH;

	/*
	 * Don't schedule adreno_start in a high priority workqueue, we are
	 * already in a workqueue which should be sufficient
	 */
	kgsl_pwrctrl_change_state(device, KGSL_STATE_ACTIVE);

	/*
	 * When waking up from a touch event we want to stay active long enough
	 * for the user to send a draw command.  The default idle timer timeout
	 * is shorter than we want so go ahead and push the idle timer out
	 * further for this special case
	 */
	mod_timer(&device->idle_timer,
		jiffies + msecs_to_jiffies(_wake_timeout));
	mutex_unlock(&device->mutex);
}

/*
 * Process input events and schedule work if needed.  At this point we are only
 * interested in groking EV_ABS touchscreen events
 */
static void adreno_input_event(struct input_handle *handle, unsigned int type,
		unsigned int code, int value)
{
	struct kgsl_device *device = handle->handler->private;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);

	/* Only consider EV_ABS (touch) events */
	if (type != EV_ABS)
		return;

	/*
	 * Don't do anything if anything hasn't been rendered since we've been
	 * here before
	 */

	if (device->flags & KGSL_FLAG_WAKE_ON_TOUCH)
		return;

	/*
	 * If the device is in nap, kick the idle timer to make sure that we
	 * don't go into slumber before the first render. If the device is
	 * already in slumber schedule the wake.
	 */

	if (device->state == KGSL_STATE_NAP) {
		/*
		 * Set the wake on touch bit to keep from coming back here and
		 * keeping the device in nap without rendering
		 */

		device->flags |= KGSL_FLAG_WAKE_ON_TOUCH;

		mod_timer(&device->idle_timer,
			jiffies + device->pwrctrl.interval_timeout);
	} else if (device->state == KGSL_STATE_SLUMBER) {
		schedule_work(&adreno_dev->input_work);
	}
}

#ifdef CONFIG_INPUT
static int adreno_input_connect(struct input_handler *handler,
		struct input_dev *dev, const struct input_device_id *id)
{
	struct input_handle *handle;
	int ret;

	handle = kzalloc(sizeof(*handle), GFP_KERNEL);
	if (handle == NULL)
		return -ENOMEM;

	handle->dev = dev;
	handle->handler = handler;
	handle->name = handler->name;

	ret = input_register_handle(handle);
	if (ret) {
		kfree(handle);
		return ret;
	}

	ret = input_open_device(handle);
	if (ret) {
		input_unregister_handle(handle);
		kfree(handle);
	}

	return ret;
}

static void adreno_input_disconnect(struct input_handle *handle)
{
	input_close_device(handle);
	input_unregister_handle(handle);
	kfree(handle);
}
#else
static int adreno_input_connect(struct input_handler *handler,
		struct input_dev *dev, const struct input_device_id *id)
{
	return 0;
}
static void adreno_input_disconnect(struct input_handle *handle) {}
#endif

/*
 * We are only interested in EV_ABS events so only register handlers for those
 * input devices that have EV_ABS events
 */
static const struct input_device_id adreno_input_ids[] = {
	{
		.flags = INPUT_DEVICE_ID_MATCH_EVBIT,
		.evbit = { BIT_MASK(EV_ABS) },
		/* assumption: MT_.._X & MT_.._Y are in the same long */
		.absbit = { [BIT_WORD(ABS_MT_POSITION_X)] =
				BIT_MASK(ABS_MT_POSITION_X) |
				BIT_MASK(ABS_MT_POSITION_Y) },
	},
	{ },
};

static struct input_handler adreno_input_handler = {
	.event = adreno_input_event,
	.connect = adreno_input_connect,
	.disconnect = adreno_input_disconnect,
	.name = "kgsl",
	.id_table = adreno_input_ids,
};

static int adreno_init_sysfs(struct kgsl_device *device);
static void adreno_uninit_sysfs(struct kgsl_device *device);
static int adreno_soft_reset(struct kgsl_device *device);

/*
 * _soft_reset() - Soft reset GPU
 * @adreno_dev: Pointer to adreno device
 *
 * Soft reset the GPU by doing a AHB write of value 1 to RBBM_SW_RESET
 * register. This is used when we want to reset the GPU without
 * turning off GFX power rail. The reset when asserted resets
 * all the HW logic, restores GPU registers to default state and
 * flushes out pending VBIF transactions.
 */
void _soft_reset(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = &adreno_dev->dev;
	unsigned int reg;

	adreno_writereg(adreno_dev, ADRENO_REG_RBBM_SW_RESET_CMD, 1);
	/*
	 * Do a dummy read to get a brief read cycle delay for the reset to take
	 * effect
	 */
	adreno_readreg(adreno_dev, ADRENO_REG_RBBM_SW_RESET_CMD, &reg);
	adreno_writereg(adreno_dev, ADRENO_REG_RBBM_SW_RESET_CMD, 0);

	/* The SP/TP regulator gets turned off after a soft reset */
	if (device->ftbl->regulator_enable)
		device->ftbl->regulator_enable(device);
}


static inline void adreno_irqctrl(struct adreno_device *adreno_dev, int state)
{
	struct adreno_gpudev *gpudev = ADRENO_GPU_DEVICE(adreno_dev);

	if (state)
		adreno_writereg(adreno_dev, ADRENO_REG_RBBM_INT_0_MASK,
			      gpudev->irq->mask);
	else
		adreno_writereg(adreno_dev, ADRENO_REG_RBBM_INT_0_MASK, 0);
}

 /*
 * adreno_hang_int_callback() - Isr for fatal interrupts that hang GPU
 * @adreno_dev: Pointer to device
 * @bit: Interrupt bit
 */
void adreno_hang_int_callback(struct adreno_device *adreno_dev, int bit)
{
	struct kgsl_device *device = &adreno_dev->dev;

	KGSL_DRV_CRIT(device, "MISC: GPU hang detected\n");
	kgsl_pwrctrl_irq(device, KGSL_PWRFLAGS_OFF);

	/* Trigger a fault in the dispatcher - this will effect a restart */
	adreno_dispatcher_irq_fault(device);
}

 /*
 * adreno_cp_callback() - CP interrupt handler
 * @adreno_dev: Adreno device pointer
 * @irq: irq number
 *
 * Handle the cp interrupt generated by GPU.
 */
void adreno_cp_callback(struct adreno_device *adreno_dev, int bit)
{
	struct kgsl_device *device = &adreno_dev->dev;

	queue_work(device->work_queue, &device->event_work);
	adreno_dispatcher_schedule(device);
}

static irqreturn_t adreno_irq_handler(struct kgsl_device *device)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct adreno_gpudev *gpudev = ADRENO_GPU_DEVICE(adreno_dev);
	struct adreno_irq *irq_params = gpudev->irq;
	irqreturn_t ret = IRQ_NONE;
	unsigned int status = 0, tmp = 0;
	int i;

	adreno_readreg(adreno_dev, ADRENO_REG_RBBM_INT_0_STATUS, &status);

	/* Loop through all set interrupts and call respective handlers */
	for (tmp = status, i = 0; tmp &&
		 i < irq_params->funcs_count; i++) {
		if (tmp & 1) {
			if (irq_params->funcs[i].func != NULL) {
				irq_params->funcs[i].func(adreno_dev, i);
				ret = IRQ_HANDLED;
			} else
			KGSL_DRV_CRIT(device,
					"Unhandled interrupt bit %x\n", i);
		}
		tmp >>= 1;
	}

	gpudev->irq_trace(adreno_dev, status);

	if (status)
		adreno_writereg(adreno_dev, ADRENO_REG_RBBM_INT_CLEAR_CMD,
				status);
	return ret;

}

static inline bool _rev_match(unsigned int id, unsigned int entry)
{
	return (entry == ANY_ID || entry == id);
}

static inline const struct adreno_gpu_core *_get_gpu_core(unsigned int chipid)
{
	unsigned int core = ADRENO_CHIPID_CORE(chipid);
	unsigned int major = ADRENO_CHIPID_MAJOR(chipid);
	unsigned int minor = ADRENO_CHIPID_MINOR(chipid);
	unsigned int patchid = ADRENO_CHIPID_PATCH(chipid);
	int i;

	for (i = 0; i < ARRAY_SIZE(adreno_gpulist); i++) {
		if (core == adreno_gpulist[i].core &&
		    _rev_match(major, adreno_gpulist[i].major) &&
		    _rev_match(minor, adreno_gpulist[i].minor) &&
		    _rev_match(patchid, adreno_gpulist[i].patchid))
			return &adreno_gpulist[i];
	}

	return NULL;
}

static void
adreno_identify_gpu(struct adreno_device *adreno_dev)
{
	const struct adreno_reg_offsets *reg_offsets;
	struct adreno_gpudev *gpudev;
	int i;

	if (kgsl_property_read_u32(&adreno_dev->dev, "qcom,chipid",
		&adreno_dev->chipid))
		KGSL_DRV_FATAL(&adreno_dev->dev,
			"No GPU chip ID was specified\n");

	adreno_dev->gpucore = _get_gpu_core(adreno_dev->chipid);

	if (adreno_dev->gpucore == NULL)
		KGSL_DRV_FATAL(&adreno_dev->dev, "Unknown GPU chip ID %8.8X\n",
			adreno_dev->chipid);

	/*
	 * The gmem size might be dynamic when ocmem is involved so copy it out
	 * of the gpu device
	 */

	adreno_dev->gmem_size = adreno_dev->gpucore->gmem_size;

	/*
	 * Initialize uninitialzed gpu registers, only needs to be done once
	 * Make all offsets that are not initialized to ADRENO_REG_UNUSED
	 */

	gpudev = ADRENO_GPU_DEVICE(adreno_dev);
	reg_offsets = gpudev->reg_offsets;

	for (i = 0; i < ADRENO_REG_REGISTER_MAX; i++) {
		if (reg_offsets->offset_0 != i && !reg_offsets->offsets[i])
			reg_offsets->offsets[i] = ADRENO_REG_UNUSED;
	}
}

static const struct platform_device_id adreno_id_table[] = {
	{ DEVICE_3D0_NAME, (unsigned long) &device_3d0, },
	{},
};

MODULE_DEVICE_TABLE(platform, adreno_id_table);

static const struct of_device_id adreno_match_table[] = {
	{ .compatible = "qcom,kgsl-3d0", .data = &device_3d0 },
	{}
};

static inline int adreno_of_read_property(struct device_node *node,
	const char *prop, unsigned int *ptr)
{
	int ret = of_property_read_u32(node, prop, ptr);
	if (ret)
		KGSL_CORE_ERR("Unable to read '%s'\n", prop);
	return ret;
}

static struct device_node *adreno_of_find_subnode(struct device_node *parent,
	const char *name)
{
	struct device_node *child;

	for_each_child_of_node(parent, child) {
		if (of_device_is_compatible(child, name))
			return child;
	}

	return NULL;
}

static int adreno_of_get_pwrlevels(struct device_node *parent,
	struct kgsl_device_platform_data *pdata)
{
	struct device_node *node, *child;
	int ret = -EINVAL;

	node = adreno_of_find_subnode(parent, "qcom,gpu-pwrlevels");

	if (node == NULL) {
		KGSL_CORE_ERR("Unable to find 'qcom,gpu-pwrlevels'\n");
		return -EINVAL;
	}

	pdata->num_levels = 0;

	for_each_child_of_node(node, child) {
		unsigned int index;
		struct kgsl_pwrlevel *level;

		if (adreno_of_read_property(child, "reg", &index))
			goto done;

		if (index >= KGSL_MAX_PWRLEVELS) {
			KGSL_CORE_ERR("Pwrlevel index %d is out of range\n",
				index);
			continue;
		}

		if (index >= pdata->num_levels)
			pdata->num_levels = index + 1;

		level = &pdata->pwrlevel[index];

		if (adreno_of_read_property(child, "qcom,gpu-freq",
			&level->gpu_freq))
			goto done;

		if (adreno_of_read_property(child, "qcom,bus-freq",
			&level->bus_freq))
			goto done;

		if (of_property_read_u32(child, "qcom,bus-min",
			&level->bus_min))
			level->bus_min = level->bus_freq;

		if (of_property_read_u32(child, "qcom,bus-max",
			&level->bus_max))
			level->bus_max = level->bus_freq;
	}

	if (of_property_read_u32(parent, "qcom,initial-pwrlevel",
		&pdata->init_level))
		pdata->init_level = 1;

	if (pdata->init_level < 0 || pdata->init_level > pdata->num_levels) {
		KGSL_CORE_ERR("Initial power level out of range\n");
		pdata->init_level = 1;
	}

	ret = 0;
done:
	return ret;

}

static inline struct adreno_device *adreno_get_dev(struct platform_device *pdev)
{
	const struct of_device_id *of_id =
		of_match_device(adreno_match_table, &pdev->dev);

	return of_id ? (struct adreno_device *) of_id->data : NULL;
}

static int adreno_of_get_iommu(struct platform_device *pdev,
	struct kgsl_device_platform_data *pdata)
{
	struct device_node *parent = pdev->dev.of_node;
	struct adreno_device *adreno_dev;
	int result = -EINVAL;
	struct device_node *node, *child;
	struct kgsl_device_iommu_data *data = NULL;
	struct kgsl_iommu_ctx *ctxs = NULL;
	u32 reg_val[2];
	u32 secure_id;
	int ctx_index = 0;

	node = of_parse_phandle(parent, "iommu", 0);
	if (node == NULL)
		return -EINVAL;

	adreno_dev = adreno_get_dev(pdev);
	if (adreno_dev == NULL)
		return -EINVAL;

	adreno_dev->dev.mmu.secured =
		(of_property_read_u32(node, "qcom,iommu-secure-id",
			&secure_id) == 0) ?  true : false;

	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (data == NULL) {
		result = -ENOMEM;
		goto err;
	}

	if (of_property_read_u32_array(node, "reg", reg_val, 2))
		goto err;

	data->physstart = reg_val[0];
	data->physend = data->physstart + reg_val[1] - 1;

	data->iommu_ctx_count = 0;

	for_each_child_of_node(node, child)
		data->iommu_ctx_count++;

	ctxs = kzalloc(data->iommu_ctx_count * sizeof(struct kgsl_iommu_ctx),
		GFP_KERNEL);

	if (ctxs == NULL) {
		result = -ENOMEM;
		goto err;
	}

	for_each_child_of_node(node, child) {
		int ret = of_property_read_string(child, "label",
				&ctxs[ctx_index].iommu_ctx_name);

		if (ret) {
			KGSL_CORE_ERR("Unable to read KGSL IOMMU 'label'\n");
			goto err;
		}

		if (!strcmp("gfx3d_user", ctxs[ctx_index].iommu_ctx_name)) {
			ctxs[ctx_index].ctx_id = 0;
		} else if (!strcmp("gfx3d_priv",
					ctxs[ctx_index].iommu_ctx_name)) {
			ctxs[ctx_index].ctx_id = 1;
		} else if (!strcmp("gfx3d_spare",
					ctxs[ctx_index].iommu_ctx_name)) {
			ctxs[ctx_index].ctx_id = 2;
		/*
		 * Context bank 2 is secure context bank if content protection
		 * is supported
		 */
		} else if (!strcmp("gfx3d_secure",
					ctxs[ctx_index].iommu_ctx_name)) {
			ctxs[ctx_index].ctx_id = 2;
		} else {
			KGSL_CORE_ERR("dt: IOMMU context %s is invalid\n",
				ctxs[ctx_index].iommu_ctx_name);
			goto err;
		}

		ctx_index++;
	}

	data->iommu_ctxs = ctxs;

	pdata->iommu_data = data;
	pdata->iommu_count = 1;

	return 0;

err:
	kfree(ctxs);
	kfree(data);

	return result;
}

static int adreno_of_get_pdata(struct platform_device *pdev)
{
	struct kgsl_device_platform_data *pdata = NULL;
	int ret = -EINVAL;

	if (of_property_read_string(pdev->dev.of_node, "label", &pdev->name)) {
		KGSL_CORE_ERR("Unable to read 'label'\n");
		goto err;
	}

	if (adreno_of_read_property(pdev->dev.of_node, "qcom,id", &pdev->id))
		goto err;

	pdata = kzalloc(sizeof(*pdata), GFP_KERNEL);
	if (pdata == NULL) {
		ret = -ENOMEM;
		goto err;
	}

	/* pwrlevel Data */
	ret = adreno_of_get_pwrlevels(pdev->dev.of_node, pdata);
	if (ret)
		goto err;

	/* get pm-qos-active-latency, set it to default if not found */
	if (of_property_read_u32(pdev->dev.of_node,
		"qcom,pm-qos-active-latency",
		&pdata->pm_qos_active_latency))
		pdata->pm_qos_active_latency = 501;

	/* get pm-qos-wakeup-latency, set it to default if not found */
	if (of_property_read_u32(pdev->dev.of_node,
		"qcom,pm-qos-wakeup-latency",
		&pdata->pm_qos_wakeup_latency))
		pdata->pm_qos_wakeup_latency = 101;

	if (of_property_read_u32(pdev->dev.of_node, "qcom,idle-timeout",
		&pdata->idle_timeout))
		pdata->idle_timeout = HZ/12;

	pdata->strtstp_sleepwake = of_property_read_bool(pdev->dev.of_node,
						"qcom,strtstp-sleepwake");

	pdata->bus_control = of_property_read_bool(pdev->dev.of_node,
						"qcom,bus-control");

	if (adreno_of_read_property(pdev->dev.of_node, "qcom,clk-map",
		&pdata->clk_map))
		goto err;

	/* Bus Scale Data */

	pdata->bus_scale_table = msm_bus_cl_get_pdata(pdev);
	if (IS_ERR_OR_NULL(pdata->bus_scale_table)) {
		ret = PTR_ERR(pdata->bus_scale_table);
		if (!ret)
			ret = -EINVAL;
		goto err;
	}

	ret = adreno_of_get_iommu(pdev, pdata);
	if (ret)
		goto err;

	pdata->coresight_pdata = of_get_coresight_platform_data(&pdev->dev,
			pdev->dev.of_node);

	pdev->dev.platform_data = pdata;
	return 0;

err:
	if (pdata) {
		if (pdata->iommu_data)
			kfree(pdata->iommu_data->iommu_ctxs);

		kfree(pdata->iommu_data);
	}

	kfree(pdata);

	return ret;
}

#ifdef CONFIG_MSM_OCMEM
static int
adreno_ocmem_malloc(struct adreno_device *adreno_dev)
{
	if (!ADRENO_FEATURE(adreno_dev, ADRENO_USES_OCMEM))
		return 0;

	if (adreno_dev->ocmem_hdl == NULL) {
		adreno_dev->ocmem_hdl =
			ocmem_allocate(OCMEM_GRAPHICS, adreno_dev->gmem_size);
		if (IS_ERR_OR_NULL(adreno_dev->ocmem_hdl)) {
			adreno_dev->ocmem_hdl = NULL;
			return -ENOMEM;
		}

		adreno_dev->gmem_size = adreno_dev->ocmem_hdl->len;
		adreno_dev->gmem_base = adreno_dev->ocmem_hdl->addr;
	}

	return 0;
}

static void
adreno_ocmem_free(struct adreno_device *adreno_dev)
{
	if (adreno_dev->ocmem_hdl != NULL) {
		ocmem_free(OCMEM_GRAPHICS, adreno_dev->ocmem_hdl);
		adreno_dev->ocmem_hdl = NULL;
	}
}
#else
static int
adreno_ocmem_malloc(struct adreno_device *adreno_dev)
{
	return 0;
}

static void
adreno_ocmem_free(struct adreno_device *adreno_dev)
{
}
#endif

int adreno_probe(struct platform_device *pdev)
{
	struct kgsl_device *device;
	struct adreno_device *adreno_dev;
	int status;

	adreno_dev = adreno_get_dev(pdev);

	if (adreno_dev == NULL) {
		pr_err("adreno: qcom,kgsl-3d0 does not exist in the device tree");
		return -ENODEV;
	}

	device = &adreno_dev->dev;
	device->pdev = pdev;

	status = adreno_of_get_pdata(pdev);
	if (status) {
		device->pdev = NULL;
		return status;
	}

	/* Identify the specific GPU */
	adreno_identify_gpu(adreno_dev);

	/*
	 * qcom,iommu-secure-id is used to identify MMUs that can handle secure
	 * content but that is only part of the story - the GPU also has to be
	 * able to handle secure content.  Unfortunately in a classic catch-22
	 * we cannot identify the GPU until after the DT is parsed. tl;dr -
	 * check the GPU capabilities here and modify mmu->secured accordingly
	 */

	if (!ADRENO_FEATURE(adreno_dev, ADRENO_CONTENT_PROTECTION))
		device->mmu.secured = false;

	status = kgsl_device_platform_probe(device);
	if (status) {
		device->pdev = NULL;
		return status;
	}

	status = adreno_ringbuffer_init(device);
	if (status)
		goto out;

	status = adreno_dispatcher_init(adreno_dev);
	if (status)
		goto out;

	adreno_debugfs_init(adreno_dev);
	adreno_profile_init(adreno_dev);

	adreno_init_sysfs(device);

	kgsl_pwrscale_init(&pdev->dev, CONFIG_MSM_ADRENO_DEFAULT_GOVERNOR);

	adreno_input_handler.private = device;

#ifdef CONFIG_INPUT
	/*
	 * It isn't fatal if we cannot register the input handler.  Sad,
	 * perhaps, but not fatal
	 */
	if (input_register_handler(&adreno_input_handler))
		KGSL_DRV_ERR(device, "Unable to register the input handler\n");
#endif
out:
	if (status) {
		adreno_ringbuffer_close(adreno_dev);
		kgsl_device_platform_remove(device);
		device->pdev = NULL;
	}

	return status;
}

static int adreno_remove(struct platform_device *pdev)
{
	struct adreno_device *adreno_dev = adreno_get_dev(pdev);
	struct kgsl_device *device;

	if (adreno_dev == NULL)
		return 0;

	device = &adreno_dev->dev;

	if (test_bit(ADRENO_DEVICE_CMDBATCH_PROFILE, &adreno_dev->priv))
		kgsl_free_global(&adreno_dev->cmdbatch_profile_buffer);

#ifdef CONFIG_INPUT
	input_unregister_handler(&adreno_input_handler);
#endif
	adreno_uninit_sysfs(device);

	adreno_coresight_remove(adreno_dev);
	adreno_profile_close(adreno_dev);

	kgsl_pwrscale_close(device);

	adreno_dispatcher_close(adreno_dev);
	adreno_ringbuffer_close(adreno_dev);
	adreno_perfcounter_close(adreno_dev);
	kgsl_device_platform_remove(device);

	if (test_bit(ADRENO_DEVICE_PWRON_FIXUP, &adreno_dev->priv)) {
		kgsl_free_global(&adreno_dev->pwron_fixup);
		clear_bit(ADRENO_DEVICE_PWRON_FIXUP, &adreno_dev->priv);
	}
	clear_bit(ADRENO_DEVICE_INITIALIZED, &adreno_dev->priv);

	return 0;
}

static int adreno_init(struct kgsl_device *device)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct adreno_gpudev *gpudev = ADRENO_GPU_DEVICE(adreno_dev);
	int i;
	int ret;

	kgsl_pwrctrl_change_state(device, KGSL_STATE_INIT);
	/*
	 * initialization only needs to be done once initially until
	 * device is shutdown
	 */
	if (test_bit(ADRENO_DEVICE_INITIALIZED, &adreno_dev->priv))
		return 0;

	/* Power up the device */
	ret = kgsl_pwrctrl_enable(device);
	if (ret)
		return ret;

	/* Initialize coresight for the target */
	adreno_coresight_init(adreno_dev);

	adreno_ringbuffer_read_pm4_ucode(device);
	adreno_ringbuffer_read_pfp_ucode(device);

	kgsl_pwrctrl_change_state(device, KGSL_STATE_INIT);
	/*
	 * Check if firmware supports the sync lock PM4 packets needed
	 * for IOMMUv1
	 */

	if (ADRENO_FEATURE(adreno_dev, ADRENO_HAS_IOMMU_SYNC_LOCK) &&
		(adreno_compare_pm4_version(adreno_dev,
			adreno_dev->gpucore->sync_lock_pm4_ver) >= 0) &&
		(adreno_compare_pfp_version(adreno_dev,
			adreno_dev->gpucore->sync_lock_pfp_ver) >= 0))
		device->mmu.flags |= KGSL_MMU_FLAGS_IOMMU_SYNC;

	adreno_ft_regs_num = (ARRAY_SIZE(adreno_ft_regs_default) +
				   gpudev->ft_perf_counters_count*2);

	adreno_ft_regs = kzalloc(adreno_ft_regs_num, GFP_KERNEL);
	if (!adreno_ft_regs)
		return -ENOMEM;

	adreno_ft_regs_val = kzalloc(adreno_ft_regs_num, GFP_KERNEL);
	if (!adreno_ft_regs_val)
		return -ENOMEM;

	for (i = 0; i < ARRAY_SIZE(adreno_ft_regs_default); i++)
		adreno_ft_regs[i] = adreno_getreg(adreno_dev,
					adreno_ft_regs_default[i]);

	ret = adreno_perfcounter_init(adreno_dev);

	/* Power down the device */
	kgsl_pwrctrl_disable(device);

	if (ret)
		goto done;

	/*
	 * Enable the power on shader corruption fix
	 * This is only applicable for 28nm targets
	 */
	if (adreno_is_a3xx(adreno_dev))
		adreno_a3xx_pwron_fixup_init(adreno_dev);
	else if ((adreno_is_a405(adreno_dev)) || (adreno_is_a420(adreno_dev)))
		adreno_a4xx_pwron_fixup_init(adreno_dev);

	set_bit(ADRENO_DEVICE_INITIALIZED, &adreno_dev->priv);

	/* Use shader offset and length defined in gpudev */
	if (adreno_dev->gpucore->shader_offset &&
					adreno_dev->gpucore->shader_size) {

		if (device->shader_mem_phys || device->shader_mem_virt)
			KGSL_DRV_ERR(device,
			"Shader memory already specified in device tree\n");
		else {
			device->shader_mem_phys = device->reg_phys +
					adreno_dev->gpucore->shader_offset;
			device->shader_mem_virt = device->reg_virt +
					adreno_dev->gpucore->shader_offset;
			device->shader_mem_len =
					adreno_dev->gpucore->shader_size;
		}
	}

	/* Adjust snapshot section sizes according to core */
	if ((adreno_is_a330(adreno_dev) || adreno_is_a305b(adreno_dev))) {
		gpudev->snapshot_data->sect_sizes->cp_state_deb =
					A320_SNAPSHOT_CP_STATE_SECTION_SIZE;
		gpudev->snapshot_data->sect_sizes->roq =
					A320_SNAPSHOT_ROQ_SECTION_SIZE;
		gpudev->snapshot_data->sect_sizes->cp_merciu =
					A320_SNAPSHOT_CP_MERCIU_SECTION_SIZE;
	}

	/*
	 * Allocate a small chunk of memory for precise cmdbatch profiling for
	 * those targets that have the always on timer
	 */

	if (gpudev->alwayson_counter_read) {
		int r = kgsl_allocate_global(&adreno_dev->dev,
			&adreno_dev->cmdbatch_profile_buffer, PAGE_SIZE, 0, 0);

		adreno_dev->cmdbatch_profile_index = 0;

		if (r == 0)
			set_bit(ADRENO_DEVICE_CMDBATCH_PROFILE,
				&adreno_dev->priv);
	}

done:
	return ret;
}

/**
 * _adreno_start - Power up the GPU and prepare to accept commands
 * @adreno_dev: Pointer to an adreno_device structure
 *
 * The core function that powers up and initalizes the GPU.  This function is
 * called at init and after coming out of SLUMBER
 */
static int _adreno_start(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = &adreno_dev->dev;
	struct adreno_gpudev *gpudev = ADRENO_GPU_DEVICE(adreno_dev);
	int status = -EINVAL;
	unsigned int state = device->state;
	unsigned int regulator_left_on = 0;
	unsigned int pmqos_wakeup_vote = device->pwrctrl.pm_qos_wakeup_latency;
	unsigned int pmqos_active_vote = device->pwrctrl.pm_qos_active_latency;

	pm_qos_update_request(&device->pwrctrl.pm_qos_req_dma,
			pmqos_wakeup_vote);

	kgsl_cffdump_open(device);

	kgsl_pwrctrl_change_state(device, KGSL_STATE_INIT);

	regulator_left_on = (regulator_is_enabled(device->pwrctrl.gpu_reg) ||
				(device->pwrctrl.gpu_cx &&
				regulator_is_enabled(device->pwrctrl.gpu_cx)));

	/* Clear any GPU faults that might have been left over */
	adreno_clear_gpu_fault(adreno_dev);

	/* Power up the device */
	status = kgsl_pwrctrl_enable(device);
	if (status)
		goto error_rail_off;

	/* Set the bit to indicate that we've just powered on */
	set_bit(ADRENO_DEVICE_PWRON, &adreno_dev->priv);

	status = kgsl_mmu_start(device);
	if (status)
		goto error_clk_off;

	status = adreno_ocmem_malloc(adreno_dev);
	if (status) {
		KGSL_DRV_ERR(device, "OCMEM malloc failed\n");
		goto error_mmu_off;
	}

	/* Soft reset GPU if regulator is stuck on*/
	if (regulator_left_on)
		_soft_reset(adreno_dev);

	if (device->pwrctrl.bus_control) {
		/* VBIF waiting for RAM */
		if (!adreno_dev->starved_ram_lo) {
			status |= adreno_perfcounter_get(adreno_dev,
					KGSL_PERFCOUNTER_GROUP_VBIF_PWR, 0,
					&adreno_dev->starved_ram_lo, NULL,
					PERFCOUNTER_FLAG_KERNEL);
		}

		/* VBIF DDR cycles */
		if (!adreno_dev->ram_cycles_lo) {
			status |= adreno_perfcounter_get(adreno_dev,
					KGSL_PERFCOUNTER_GROUP_VBIF,
					VBIF_AXI_TOTAL_BEATS,
					&adreno_dev->ram_cycles_lo, NULL,
					PERFCOUNTER_FLAG_KERNEL);
		}
	}
	if (status) {
		KGSL_DRV_ERR(device,
			"Unable to get perf counters for Bus DCVS\n");
		goto error_mmu_off;
	}

	/* Restore performance counter registers with saved values */
	adreno_perfcounter_restore(adreno_dev);

	/* Start the GPU */
	gpudev->start(adreno_dev);

	/* Re-initialize the coresight registers if applicable */
	adreno_coresight_start(adreno_dev);

	kgsl_pwrctrl_irq(device, KGSL_PWRFLAGS_ON);
	adreno_irqctrl(adreno_dev, 1);

	adreno_perfcounter_start(adreno_dev);

	status = adreno_ringbuffer_cold_start(adreno_dev);

	if (status)
		goto error_irq_off;

	/* Enable h/w power collapse feature */
	if (gpudev->enable_pc)
		gpudev->enable_pc(adreno_dev);

	/* Enable peak power detect feature */
	if (gpudev->enable_ppd)
		gpudev->enable_ppd(adreno_dev);

	/* Start the dispatcher */
	adreno_dispatcher_start(device);

	device->reset_counter++;

	set_bit(ADRENO_DEVICE_STARTED, &adreno_dev->priv);

	if (pmqos_active_vote != pmqos_wakeup_vote)
		pm_qos_update_request(&device->pwrctrl.pm_qos_req_dma,
				pmqos_active_vote);

	return 0;

error_irq_off:
	kgsl_pwrctrl_irq(device, KGSL_PWRFLAGS_OFF);

error_mmu_off:
	kgsl_mmu_stop(&device->mmu);

error_clk_off:
	kgsl_pwrctrl_disable(device);
error_rail_off:
	/* set the state back to original state */
	kgsl_pwrctrl_change_state(device, state);

	if (pmqos_active_vote != pmqos_wakeup_vote)
		pm_qos_update_request(&device->pwrctrl.pm_qos_req_dma,
				pmqos_active_vote);

	return status;
}

/**
 * adreno_start() - Power up and initialize the GPU
 * @device: Pointer to the KGSL device to power up
 * @priority:  Boolean flag to specify of the start should be scheduled in a low
 * latency work queue
 *
 * Power up the GPU and initialize it.  If priority is specified then elevate
 * the thread priority for the duration of the start operation
 */
static int adreno_start(struct kgsl_device *device, int priority)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	int nice = task_nice(current);
	int ret;

	if (priority && (_wake_nice < nice))
		set_user_nice(current, _wake_nice);

	ret = _adreno_start(adreno_dev);

	if (priority)
		set_user_nice(current, nice);

	return ret;
}

static int adreno_stop(struct kgsl_device *device)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);

	if (!test_bit(ADRENO_DEVICE_STARTED, &adreno_dev->priv))
		return 0;

	kgsl_pwrctrl_enable(device);

	adreno_set_active_ctx_null(adreno_dev);

	adreno_dispatcher_stop(adreno_dev);

	adreno_ringbuffer_stop(adreno_dev);

	adreno_irqctrl(adreno_dev, 0);
	kgsl_pwrctrl_irq(device, KGSL_PWRFLAGS_OFF);
	del_timer_sync(&device->idle_timer);

	adreno_ocmem_free(adreno_dev);

	/* Save active coresight registers if applicable */
	adreno_coresight_stop(adreno_dev);

	/* Save physical performance counter values before GPU power down*/
	adreno_perfcounter_save(adreno_dev);

	kgsl_mmu_stop(&device->mmu);
	/* Power down the device */
	kgsl_pwrctrl_disable(device);

	kgsl_cffdump_close(device);

	clear_bit(ADRENO_DEVICE_STARTED, &adreno_dev->priv);

	return 0;
}

/**
 * adreno_reset() - Helper function to reset the GPU
 * @device: Pointer to the KGSL device structure for the GPU
 *
 * Try to reset the GPU to recover from a fault.  First, try to do a low latency
 * soft reset.  If the soft reset fails for some reason, then bring out the big
 * guns and toggle the footswitch.
 */
int adreno_reset(struct kgsl_device *device)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	int ret = -EINVAL;
	struct kgsl_mmu *mmu = &device->mmu;
	int i = 0;

	/* Try soft reset first, for non mmu fault case only */
	if (!atomic_read(&mmu->fault)) {
		ret = adreno_soft_reset(device);
		if (ret)
			KGSL_DEV_ERR_ONCE(device, "Device soft reset failed\n");
	}
	if (ret) {
		/* If soft reset failed/skipped, then pull the power */
		adreno_stop(device);

		/* Keep trying to start the device until it works */
		for (i = 0; i < NUM_TIMES_RESET_RETRY; i++) {
			ret = adreno_start(device, 0);
			if (!ret)
				break;

			msleep(20);
		}
	}
	if (ret)
		return ret;

	if (0 != i)
		KGSL_DRV_WARN(device, "Device hard reset tried %d tries\n", i);

	/*
	 * If active_cnt is non-zero then the system was active before
	 * going into a reset - put it back in that state
	 */

	if (atomic_read(&device->active_cnt))
		kgsl_pwrctrl_change_state(device, KGSL_STATE_ACTIVE);

	/* Set the page table back to the default page table */
	kgsl_mmu_set_pt(&device->mmu, device->mmu.defaultpagetable);
	kgsl_sharedmem_writel(device,
		&adreno_dev->ringbuffers[0].pagetable_desc,
		offsetof(struct adreno_ringbuffer_pagetable_info,
			current_global_ptname), 0);

	return ret;
}

/**
 * _get_adreno_dev() -  Routine to get a pointer to adreno dev
 * @dev: device ptr
 * @attr: Device attribute
 * @buf: value to write
 * @count: size of the value to write
 */
static struct adreno_device *_get_adreno_dev(struct device *dev)
{
	struct kgsl_device *device = kgsl_device_from_dev(dev);
	return device ? ADRENO_DEVICE(device) : NULL;
}

/**
 * _ft_policy_store() -  Routine to configure FT policy
 * @dev: device ptr
 * @attr: Device attribute
 * @buf: value to write
 * @count: size of the value to write
 *
 * FT policy can be set to any of the options below.
 * KGSL_FT_DISABLE -> BIT(0) Set to disable FT
 * KGSL_FT_REPLAY  -> BIT(1) Set to enable replay
 * KGSL_FT_SKIPIB  -> BIT(2) Set to skip IB
 * KGSL_FT_SKIPFRAME -> BIT(3) Set to skip frame
 * by default set FT policy to KGSL_FT_DEFAULT_POLICY
 */
static ssize_t _ft_policy_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct adreno_device *adreno_dev = _get_adreno_dev(dev);
	int ret;
	unsigned int temp;
	if (adreno_dev == NULL)
		return 0;

	mutex_lock(&adreno_dev->dev.mutex);
	ret = kgsl_sysfs_store(buf, &temp);
	mutex_unlock(&adreno_dev->dev.mutex);
	if (!ret) {
		temp &= KGSL_FT_POLICY_MASK;
		adreno_dev->ft_policy = temp;
	}

	return ret < 0 ? ret : count;
}

/**
 * _ft_policy_show() -  Routine to read FT policy
 * @dev: device ptr
 * @attr: Device attribute
 * @buf: value read
 *
 * This is a routine to read current FT policy
 */
static ssize_t _ft_policy_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct adreno_device *adreno_dev = _get_adreno_dev(dev);
	if (adreno_dev == NULL)
		return 0;
	return snprintf(buf, PAGE_SIZE, "0x%X\n", adreno_dev->ft_policy);
}

/**
 * _ft_pagefault_policy_store() -  Routine to configure FT
 * pagefault policy
 * @dev: device ptr
 * @attr: Device attribute
 * @buf: value to write
 * @count: size of the value to write
 *
 * FT pagefault policy can be set to any of the options below.
 * KGSL_FT_PAGEFAULT_INT_ENABLE -> BIT(0) set to enable pagefault INT
 * KGSL_FT_PAGEFAULT_GPUHALT_ENABLE  -> BIT(1) Set to enable GPU HALT on
 * pagefaults. This stalls the GPU on a pagefault on IOMMU v1 HW.
 * KGSL_FT_PAGEFAULT_LOG_ONE_PER_PAGE  -> BIT(2) Set to log only one
 * pagefault per page.
 * KGSL_FT_PAGEFAULT_LOG_ONE_PER_INT -> BIT(3) Set to log only one
 * pagefault per INT.
 */
static ssize_t _ft_pagefault_policy_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct adreno_device *adreno_dev = _get_adreno_dev(dev);
	int ret = 0;
	unsigned int policy = 0;
	if (adreno_dev == NULL)
		return 0;

	mutex_lock(&adreno_dev->dev.mutex);

	ret = kgsl_sysfs_store(buf, &policy);
	if (ret)
		goto out;

	policy &= (KGSL_FT_PAGEFAULT_INT_ENABLE |
			KGSL_FT_PAGEFAULT_GPUHALT_ENABLE |
			KGSL_FT_PAGEFAULT_LOG_ONE_PER_PAGE |
			KGSL_FT_PAGEFAULT_LOG_ONE_PER_INT);
	ret = kgsl_mmu_set_pagefault_policy(&(adreno_dev->dev.mmu),
			adreno_dev->ft_pf_policy);
	if (!ret)
		adreno_dev->ft_pf_policy = policy;

out:
	mutex_unlock(&adreno_dev->dev.mutex);
	return ret < 0 ? ret : count;
}

/**
 * _ft_pagefault_policy_show() -  Routine to read FT pagefault
 * policy
 * @dev: device ptr
 * @attr: Device attribute
 * @buf: value read
 *
 * This is a routine to read current FT pagefault policy
 */
static ssize_t _ft_pagefault_policy_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct adreno_device *adreno_dev = _get_adreno_dev(dev);
	if (adreno_dev == NULL)
		return 0;
	return snprintf(buf, PAGE_SIZE, "0x%X\n", adreno_dev->ft_pf_policy);
}

/**
 * _ft_fast_hang_detect_store() -  Routine to configure FT fast
 * hang detect policy
 * @dev: device ptr
 * @attr: Device attribute
 * @buf: value to write
 * @count: size of the value to write
 *
 * 0x1 - Enable fast hang detection
 * 0x0 - Disable fast hang detection
 */
static ssize_t _ft_fast_hang_detect_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct adreno_device *adreno_dev = _get_adreno_dev(dev);
	struct adreno_gpudev *gpudev;
	int ret, tmp;

	if (adreno_dev == NULL)
		return 0;

	gpudev = ADRENO_GPU_DEVICE(adreno_dev);

	mutex_lock(&adreno_dev->dev.mutex);

	tmp = adreno_dev->fast_hang_detect;

	ret = kgsl_sysfs_store(buf, &adreno_dev->fast_hang_detect);

	if (tmp != adreno_dev->fast_hang_detect) {
		if (adreno_dev->fast_hang_detect) {
			if (kgsl_active_count_get(&adreno_dev->dev)) {
				adreno_fault_detect_start(adreno_dev);
				kgsl_active_count_put(&adreno_dev->dev);
			}
		} else
			adreno_fault_detect_stop(adreno_dev);
	}

	mutex_unlock(&adreno_dev->dev.mutex);

	return ret < 0 ? ret : count;

}

/**
 * _ft_fast_hang_detect_show() -  Routine to read FT fast
 * hang detect policy
 * @dev: device ptr
 * @attr: Device attribute
 * @buf: value read
 */
static ssize_t _ft_fast_hang_detect_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct adreno_device *adreno_dev = _get_adreno_dev(dev);
	if (adreno_dev == NULL)
		return 0;
	return snprintf(buf, PAGE_SIZE, "%d\n",
				(adreno_dev->fast_hang_detect ? 1 : 0));
}

/**
 * _ft_long_ib_detect_store() -  Routine to configure FT long IB
 * detect policy
 * @dev: device ptr
 * @attr: Device attribute
 * @buf: value to write
 * @count: size of the value to write
 *
 * 0x0 - Enable long IB detection
 * 0x1 - Disable long IB detection
 */
static ssize_t _ft_long_ib_detect_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct adreno_device *adreno_dev = _get_adreno_dev(dev);
	int ret;
	if (adreno_dev == NULL)
		return 0;

	mutex_lock(&adreno_dev->dev.mutex);
	ret = kgsl_sysfs_store(buf, &adreno_dev->long_ib_detect);
	mutex_unlock(&adreno_dev->dev.mutex);

	return ret < 0 ? ret : count;

}

/**
 * _ft_long_ib_detect_show() -  Routine to read FT long IB
 * detect policy
 * @dev: device ptr
 * @attr: Device attribute
 * @buf: value read
 */
static ssize_t _ft_long_ib_detect_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct adreno_device *adreno_dev = _get_adreno_dev(dev);
	if (adreno_dev == NULL)
		return 0;
	return snprintf(buf, PAGE_SIZE, "%d\n",
				(adreno_dev->long_ib_detect ? 1 : 0));
}


/**
 * _ft_hang_intr_status_store -  Routine to enable/disable h/w hang interrupt
 * @dev: device ptr
 * @attr: Device attribute
 * @buf: value to write
 * @count: size of the value to write
 */
static ssize_t _ft_hang_intr_status_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	unsigned int new_setting = 0, old_setting;
	struct kgsl_device *device = kgsl_device_from_dev(dev);
	struct adreno_device *adreno_dev;
	int ret;
	if (device == NULL)
		return 0;
	adreno_dev = ADRENO_DEVICE(device);

	mutex_lock(&device->mutex);
	ret = kgsl_sysfs_store(buf, &new_setting);
	if (ret)
		goto done;
	if (new_setting)
		new_setting = 1;
	old_setting =
		(test_bit(ADRENO_DEVICE_HANG_INTR, &adreno_dev->priv) ? 1 : 0);
	if (new_setting != old_setting) {
		if (new_setting)
			set_bit(ADRENO_DEVICE_HANG_INTR, &adreno_dev->priv);
		else
			clear_bit(ADRENO_DEVICE_HANG_INTR, &adreno_dev->priv);
		/* Set the new setting based on device state */
		switch (device->state) {
		case KGSL_STATE_NAP:
		case KGSL_STATE_SLEEP:
			kgsl_pwrctrl_change_state(device, KGSL_STATE_ACTIVE);
		case KGSL_STATE_ACTIVE:
			adreno_irqctrl(adreno_dev, 1);
		/*
		 * For following states setting will be picked up on device
		 * start. Still need them in switch statement to differentiate
		 * from default
		 */
		case KGSL_STATE_SLUMBER:
		case KGSL_STATE_SUSPEND:
			break;
		default:
			ret = -EACCES;
			/* reset back to old setting on error */
			if (new_setting)
				clear_bit(ADRENO_DEVICE_HANG_INTR,
					&adreno_dev->priv);
			else
				set_bit(ADRENO_DEVICE_HANG_INTR,
					&adreno_dev->priv);
			goto done;
		}
	}
done:
	mutex_unlock(&device->mutex);
	return ret < 0 ? ret : count;
}

/**
 * _ft_hang_intr_status_show() -  Routine to read hardware hang interrupt
 * enablement
 * @dev: device ptr
 * @attr: Device attribute
 * @buf: value read
 */
static ssize_t _ft_hang_intr_status_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct adreno_device *adreno_dev = _get_adreno_dev(dev);
	if (adreno_dev == NULL)
		return 0;
	return snprintf(buf, PAGE_SIZE, "%d\n",
		test_bit(ADRENO_DEVICE_HANG_INTR, &adreno_dev->priv) ? 1 : 0);
}

/**
 * _wake_timeout_store() - Store the amount of time to extend idle check after
 * wake on touch
 * @dev: device ptr
 * @attr: Device attribute
 * @buf: value to write
 * @count: size of the value to write
 *
 */
static ssize_t _wake_timeout_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	int ret = kgsl_sysfs_store(buf, &_wake_timeout);
	return ret < 0 ? ret : count;
}

/**
 * _wake_timeout_show() -  Show the amount of time idle check gets extended
 * after wake on touch
 * detect policy
 * @dev: device ptr
 * @attr: Device attribute
 * @buf: value read
 */
static ssize_t _wake_timeout_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u\n", _wake_timeout);
}

/**
 * _sptp_pc_store() - Enable or disable SP/TP power collapse
 * @dev: device ptr
 * @attr: Device attribute
 * @buf: value to write
 * @count: size of the value to write
 *
 */
static ssize_t _sptp_pc_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct adreno_device *adreno_dev = _get_adreno_dev(dev);
	struct kgsl_device *device = kgsl_device_from_dev(dev);
	struct adreno_gpudev *gpudev;
	int ret, t = 0;

	if ((adreno_dev == NULL) || (device == NULL))
		return -ENODEV;

	gpudev = ADRENO_GPU_DEVICE(adreno_dev);
	ret = kgsl_sysfs_store(buf, &t);
	if (ret < 0)
		return ret;

	mutex_lock(&device->mutex);

	kgsl_pwrctrl_change_state(device, KGSL_STATE_SUSPEND);
	if (t)
		set_bit(ADRENO_SPTP_PC_CTRL, &adreno_dev->pwrctrl_flag);
	else
		clear_bit(ADRENO_SPTP_PC_CTRL, &adreno_dev->pwrctrl_flag);
	kgsl_pwrctrl_change_state(device, KGSL_STATE_SLUMBER);

	mutex_unlock(&device->mutex);
	return count;
}

/**
 * _sptp_pc_show() -  Show whether SP/TP power collapse is enabled
 * @dev: device ptr
 * @attr: Device attribute
 * @buf: value read
 */
static ssize_t _sptp_pc_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct adreno_device *adreno_dev = _get_adreno_dev(dev);
	return snprintf(buf, PAGE_SIZE, "%u\n", test_bit(ADRENO_SPTP_PC_CTRL,
					&adreno_dev->pwrctrl_flag));
}

#define ADRENO_DEVICE_ATTR(name) \
	DEVICE_ATTR(name, 0644,	_ ## name ## _show, _ ## name ## _store);

static ADRENO_DEVICE_ATTR(ft_policy);
static ADRENO_DEVICE_ATTR(ft_pagefault_policy);
static ADRENO_DEVICE_ATTR(ft_fast_hang_detect);
static ADRENO_DEVICE_ATTR(ft_long_ib_detect);
static ADRENO_DEVICE_ATTR(ft_hang_intr_status);

static DEVICE_INT_ATTR(wake_nice, 0644, _wake_nice);
static ADRENO_DEVICE_ATTR(wake_timeout);
static ADRENO_DEVICE_ATTR(sptp_pc);

static const struct device_attribute *_attr_list[] = {
	&dev_attr_ft_policy,
	&dev_attr_ft_pagefault_policy,
	&dev_attr_ft_fast_hang_detect,
	&dev_attr_ft_long_ib_detect,
	&dev_attr_ft_hang_intr_status,
	&dev_attr_wake_nice.attr,
	&dev_attr_wake_timeout,
	&dev_attr_sptp_pc,
	NULL,
};

/* Add a ppd directory for controlling different knobs from sysfs */
struct adreno_ppd_attribute {
	struct attribute attr;
	ssize_t (*show)(struct kgsl_device *device, char *buf);
	ssize_t (*store)(struct kgsl_device *device, const char *buf,
		size_t count);
};

#define PPD_ATTR(_name, _mode, _show, _store) \
struct adreno_ppd_attribute attr_##_name = { \
	.attr = { .name = __stringify(_name), .mode = _mode }, \
	.show = _show, \
	.store = _store, \
}

#define to_ppd_attr(a) \
container_of(a, struct adreno_ppd_attribute, attr)

#define kobj_to_device(a) \
container_of(a, struct kgsl_device, ppd_kobj)

/**
 * ppd_enable_store() - Enable or disable peak power detection
 * @attr: Device attribute
 * @buf: value to write
 * @count: size of the value to write
 *
 */
static ssize_t ppd_enable_store(struct kgsl_device *device,
				const char *buf, size_t count)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct adreno_gpudev *gpudev;
	unsigned int ret;
	unsigned int ppd_on = 1;

	if ((adreno_dev == NULL) || (device == NULL))
		return -ENODEV;

	gpudev = ADRENO_GPU_DEVICE(adreno_dev);
	ret = kgsl_sysfs_store(buf, &ppd_on);
	if (ret < 0)
		return ret;

	mutex_lock(&device->mutex);

	kgsl_pwrctrl_change_state(device, KGSL_STATE_SUSPEND);
	if (ppd_on)
		set_bit(ADRENO_PPD_CTRL, &adreno_dev->pwrctrl_flag);
	else
		clear_bit(ADRENO_PPD_CTRL, &adreno_dev->pwrctrl_flag);
	kgsl_pwrctrl_change_state(device, KGSL_STATE_SLUMBER);

	mutex_unlock(&device->mutex);
	return count;
}

/**
 * ppd_enable_show() -  Show whether ppd is enabled
 * @dev: device ptr
 * @buf: value read
 */
static ssize_t ppd_enable_show(struct kgsl_device *device,
					char *buf)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	return snprintf(buf, PAGE_SIZE, "%u\n", test_bit(ADRENO_PPD_CTRL,
					&adreno_dev->pwrctrl_flag));
}
/* Add individual ppd attributes here */
static PPD_ATTR(enable, 0644, ppd_enable_show, ppd_enable_store);

static void ppd_sysfs_release(struct kobject *kobj)
{
}

static ssize_t ppd_sysfs_show(struct kobject *kobj,
	struct attribute *attr, char *buf)
{
	struct adreno_ppd_attribute *pattr = to_ppd_attr(attr);
	struct kgsl_device *device = kobj_to_device(kobj);
	ssize_t ret;

	if (device && pattr->show)
		ret = pattr->show(device, buf);
	else
		ret = -EIO;

	return ret;
}

static ssize_t ppd_sysfs_store(struct kobject *kobj,
	struct attribute *attr, const char *buf, size_t count)
{
	struct adreno_ppd_attribute *pattr = to_ppd_attr(attr);
	struct kgsl_device *device = kobj_to_device(kobj);
	ssize_t ret;

	if (device && pattr->store)
		ret = pattr->store(device, buf, count);
	else
		ret = -EIO;

	return ret;
}

static const struct sysfs_ops ppd_sysfs_ops = {
	.show = ppd_sysfs_show,
	.store = ppd_sysfs_store,
};

static struct kobj_type ktype_ppd = {
	.sysfs_ops = &ppd_sysfs_ops,
	.default_attrs = NULL,
	.release = ppd_sysfs_release,
};

static int adreno_init_sysfs(struct kgsl_device *device)
{
	int ret;
	ret =  kgsl_create_device_sysfs_files(device->dev, _attr_list);
	if (ret)
		goto done;
	ret = kobject_init_and_add(&device->ppd_kobj, &ktype_ppd,
		&device->dev->kobj, "ppd");
	if (ret)
		goto done;

	ret  = sysfs_create_file(&device->ppd_kobj, &attr_enable.attr);

done:
	return ret;
}

static void adreno_uninit_sysfs(struct kgsl_device *device)
{
	sysfs_remove_file(&device->ppd_kobj, &attr_enable.attr);

	kobject_put(&device->ppd_kobj);

	kgsl_remove_device_sysfs_files(device->dev, _attr_list);
}

static int adreno_getproperty(struct kgsl_device *device,
				enum kgsl_property_type type,
				void __user *value,
				size_t sizebytes)
{
	int status = -EINVAL;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);

	switch (type) {
	case KGSL_PROP_DEVICE_INFO:
		{
			struct kgsl_devinfo devinfo;

			if (sizebytes != sizeof(devinfo)) {
				status = -EINVAL;
				break;
			}

			memset(&devinfo, 0, sizeof(devinfo));
			devinfo.device_id = device->id+1;
			devinfo.chip_id = adreno_dev->chipid;
			devinfo.mmu_enabled = kgsl_mmu_enabled();
			devinfo.gmem_gpubaseaddr = adreno_dev->gmem_base;
			devinfo.gmem_sizebytes = adreno_dev->gmem_size;

			if (copy_to_user(value, &devinfo, sizeof(devinfo)) !=
					0) {
				status = -EFAULT;
				break;
			}
			status = 0;
		}
		break;
	case KGSL_PROP_DEVICE_SHADOW:
		{
			struct kgsl_shadowprop shadowprop;

			if (sizebytes != sizeof(shadowprop)) {
				status = -EINVAL;
				break;
			}
			memset(&shadowprop, 0, sizeof(shadowprop));
			if (device->memstore.hostptr) {
				/*NOTE: with mmu enabled, gpuaddr doesn't mean
				 * anything to mmap().
				 */
				shadowprop.gpuaddr = device->memstore.gpuaddr;
				shadowprop.size = device->memstore.size;
				/* GSL needs this to be set, even if it
				   appears to be meaningless */
				shadowprop.flags = KGSL_FLAGS_INITIALIZED |
					KGSL_FLAGS_PER_CONTEXT_TIMESTAMPS;
			}
			if (copy_to_user(value, &shadowprop,
				sizeof(shadowprop))) {
				status = -EFAULT;
				break;
			}
			status = 0;
		}
		break;
	case KGSL_PROP_MMU_ENABLE:
		{
			int mmu_prop = kgsl_mmu_enabled();

			if (sizebytes != sizeof(int)) {
				status = -EINVAL;
				break;
			}
			if (copy_to_user(value, &mmu_prop, sizeof(mmu_prop))) {
				status = -EFAULT;
				break;
			}
			status = 0;
		}
		break;
	case KGSL_PROP_INTERRUPT_WAITS:
		{
			int int_waits = 1;
			if (sizebytes != sizeof(int)) {
				status = -EINVAL;
				break;
			}
			if (copy_to_user(value, &int_waits, sizeof(int))) {
				status = -EFAULT;
				break;
			}
			status = 0;
		}
		break;
	default:
		status = -EINVAL;
	}

	return status;
}

int adreno_set_constraint(struct kgsl_device *device,
				struct kgsl_context *context,
				struct kgsl_device_constraint *constraint)
{
	int status = 0;

	switch (constraint->type) {
	case KGSL_CONSTRAINT_PWRLEVEL: {
		struct kgsl_device_constraint_pwrlevel pwr;

		if (constraint->size != sizeof(pwr)) {
			status = -EINVAL;
			break;
		}

		if (copy_from_user(&pwr,
				(void __user *)constraint->data,
				sizeof(pwr))) {
			status = -EFAULT;
			break;
		}
		if (pwr.level >= KGSL_CONSTRAINT_PWR_MAXLEVELS) {
			status = -EINVAL;
			break;
		}

		context->pwr_constraint.type =
				KGSL_CONSTRAINT_PWRLEVEL;
		context->pwr_constraint.sub_type = pwr.level;
		trace_kgsl_user_pwrlevel_constraint(device,
			context->id,
			context->pwr_constraint.type,
			context->pwr_constraint.sub_type);
		}
		break;
	case KGSL_CONSTRAINT_NONE:
		if (context->pwr_constraint.type == KGSL_CONSTRAINT_PWRLEVEL)
			trace_kgsl_user_pwrlevel_constraint(device,
				context->id,
				KGSL_CONSTRAINT_NONE,
				context->pwr_constraint.sub_type);
		context->pwr_constraint.type = KGSL_CONSTRAINT_NONE;
		break;

	default:
		status = -EINVAL;
		break;
	}

	/* If a new constraint has been set for a context, cancel the old one */
	if ((status == 0) &&
		(context->id == device->pwrctrl.constraint.owner_id)) {
		trace_kgsl_constraint(device, device->pwrctrl.constraint.type,
					device->pwrctrl.active_pwrlevel, 0);
		device->pwrctrl.constraint.type = KGSL_CONSTRAINT_NONE;
	}

	return status;
}

static int adreno_setproperty(struct kgsl_device_private *dev_priv,
				enum kgsl_property_type type,
				void __user *value,
				unsigned int sizebytes)
{
	int status = -EINVAL;
	struct kgsl_device *device = dev_priv->device;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);

	switch (type) {
	case KGSL_PROP_PWRCTRL: {
			unsigned int enable;

			if (sizebytes != sizeof(enable))
				break;

			if (copy_from_user(&enable, value, sizeof(enable))) {
				status = -EFAULT;
				break;
			}

			mutex_lock(&device->mutex);

			if (enable) {
				device->pwrctrl.ctrl_flags = 0;
				adreno_dev->fast_hang_detect = 1;

				if (!kgsl_active_count_get(&adreno_dev->dev)) {
					adreno_fault_detect_start(adreno_dev);
					kgsl_active_count_put(&adreno_dev->dev);
				}

				kgsl_pwrscale_enable(device);
			} else {
				kgsl_pwrctrl_change_state(device,
							KGSL_STATE_ACTIVE);
				device->pwrctrl.ctrl_flags = KGSL_PWR_ON;
				adreno_dev->fast_hang_detect = 0;
				adreno_fault_detect_stop(adreno_dev);
				kgsl_pwrscale_disable(device);
			}

			mutex_unlock(&device->mutex);
			status = 0;
		}
		break;
	case KGSL_PROP_PWR_CONSTRAINT: {
			struct kgsl_device_constraint constraint;
			struct kgsl_context *context;

			if (sizebytes != sizeof(constraint))
				break;

			if (copy_from_user(&constraint, value,
				sizeof(constraint))) {
				status = -EFAULT;
				break;
			}

			context = kgsl_context_get_owner(dev_priv,
							constraint.context_id);

			if (context == NULL)
				break;

			status = adreno_set_constraint(device, context,
								&constraint);

			kgsl_context_put(context);
		}
		break;
	default:
		break;
	}

	return status;
}

/*
 * adreno_irq_pending() - Checks if interrupt is generated by h/w
 * @adreno_dev: Pointer to device whose interrupts are checked
 *
 * Returns true if interrupts are pending from device else 0.
 */
inline unsigned int adreno_irq_pending(struct adreno_device *adreno_dev)
{
	struct adreno_gpudev *gpudev = ADRENO_GPU_DEVICE(adreno_dev);
	unsigned int status;

	adreno_readreg(adreno_dev, ADRENO_REG_RBBM_INT_0_STATUS, &status);

	return (status & gpudev->irq->mask) ? 1 : 0;
}


/**
 * adreno_hw_isidle() - Check if the GPU core is idle
 * @adreno_dev: Pointer to the Adreno device structure for the GPU
 *
 * Return true if the RBBM status register for the GPU type indicates that the
 * hardware is idle
 */
bool adreno_hw_isidle(struct adreno_device *adreno_dev)
{
	unsigned int reg_rbbm_status;

	adreno_readreg(adreno_dev, ADRENO_REG_RBBM_STATUS,
		&reg_rbbm_status);

	if (reg_rbbm_status & ~0x80000001)
		return false;

	/* Don't consider ourselves idle if there is an IRQ pending */
	if (adreno_irq_pending(adreno_dev))
		return false;

	return true;
}

/**
 * adreno_soft_reset() -  Do a soft reset of the GPU hardware
 * @device: KGSL device to soft reset
 *
 * "soft reset" the GPU hardware - this is a fast path GPU reset
 * The GPU hardware is reset but we never pull power so we can skip
 * a lot of the standard adreno_stop/adreno_start sequence
 */
static int adreno_soft_reset(struct kgsl_device *device)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct adreno_gpudev *gpudev = ADRENO_GPU_DEVICE(adreno_dev);
	int ret;

	adreno_set_active_ctx_null(adreno_dev);

	if (kgsl_pwrctrl_isenabled(device))
		adreno_irqctrl(adreno_dev, 0);

	kgsl_pwrctrl_irq(device, KGSL_PWRFLAGS_OFF);

	adreno_clear_gpu_fault(adreno_dev);
	/* since device is oficially off now clear start bit */
	clear_bit(ADRENO_DEVICE_STARTED, &adreno_dev->priv);

	/* Delete the idle timer */
	del_timer_sync(&device->idle_timer);

	/* Make sure we are totally awake */
	kgsl_pwrctrl_enable(device);

	/* save physical performance counter values before GPU soft reset */
	adreno_perfcounter_save(adreno_dev);

	kgsl_cffdump_close(device);
	/* Reset the GPU */
	_soft_reset(adreno_dev);

	/* start of new CFF after reset */
	kgsl_cffdump_open(device);

	/* Restore physical performance counter values after soft reset */
	adreno_perfcounter_restore(adreno_dev);

	/* Reinitialize the GPU */
	gpudev->start(adreno_dev);

	/* Re-initialize the coresight registers if applicable */
	adreno_coresight_start(adreno_dev);

	/* Enable IRQ */
	kgsl_pwrctrl_irq(device, KGSL_PWRFLAGS_ON);
	adreno_irqctrl(adreno_dev, 1);

	/* stop all ringbuffers to cancel RB events */
	adreno_ringbuffer_stop(adreno_dev);
	/*
	 * If we have offsets for the jump tables we can try to do a warm start,
	 * otherwise do a full ringbuffer restart
	 */

	if (ADRENO_FEATURE(adreno_dev, ADRENO_WARM_START))
		ret = adreno_ringbuffer_warm_start(adreno_dev);
	else
		ret = adreno_ringbuffer_cold_start(adreno_dev);

	if (!ret) {
		device->reset_counter++;
		/* device is back online */
		set_bit(ADRENO_DEVICE_STARTED, &adreno_dev->priv);
	}

	return ret;
}

/*
 * adreno_isidle() - return true if the GPU hardware is idle
 * @device: Pointer to the KGSL device structure for the GPU
 *
 * Return true if the GPU hardware is idle and there are no commands pending in
 * the ringbuffer
 */
bool adreno_isidle(struct kgsl_device *device)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct adreno_ringbuffer *rb;
	int i;

	if (!kgsl_pwrctrl_isenabled(device))
		return true;

	adreno_get_rptr(ADRENO_CURRENT_RINGBUFFER(adreno_dev));

	/*
	 * wptr is updated when we add commands to ringbuffer, add a barrier
	 * to make sure updated wptr is compared to rptr
	 */
	smp_mb();

	/*
	 * ringbuffer is truly idle when all ringbuffers read and write
	 * pointers are equal
	 */
	FOR_EACH_RINGBUFFER(adreno_dev, rb, i) {
		if (rb->rptr != rb->wptr)
			break;
	}

	if (i == adreno_dev->num_ringbuffers)
		return adreno_hw_isidle(adreno_dev);

	return false;
}

/**
 * adreno_spin_idle() - Spin wait for the GPU to idle
 * @device: Pointer to the KGSL device
 *
 * Spin the CPU waiting for the RBBM status to return idle
 */
int adreno_spin_idle(struct kgsl_device *device)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	unsigned long wait = jiffies + msecs_to_jiffies(ADRENO_IDLE_TIMEOUT);

	kgsl_cffdump_regpoll(device,
		adreno_getreg(adreno_dev, ADRENO_REG_RBBM_STATUS) << 2,
		0x00000000, 0x80000000);

	while (time_before(jiffies, wait)) {
		/*
		 * If we fault, stop waiting and return an error. The dispatcher
		 * will clean up the fault from the work queue, but we need to
		 * make sure we don't block it by waiting for an idle that
		 * will never come.
		 */

		if (adreno_gpu_fault(adreno_dev) != 0)
			return -EDEADLK;

		if (adreno_isidle(device))
			return 0;
	}

	return -ETIMEDOUT;
}

/**
 * adreno_idle() - wait for the GPU hardware to go idle
 * @device: Pointer to the KGSL device structure for the GPU
 *
 * Wait up to ADRENO_IDLE_TIMEOUT milliseconds for the GPU hardware to go quiet.
 */

int adreno_idle(struct kgsl_device *device)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	int ret;

	/*
	 * Make sure the device mutex is held so the dispatcher can't send any
	 * more commands to the hardware
	 */

	BUG_ON(!mutex_is_locked(&device->mutex));

	/* Check if we are already idle before idling dispatcher */
	if (adreno_isidle(device))
		return 0;
	/*
	 * Wait for dispatcher to finish completing commands
	 * already submitted
	 */
	ret = adreno_dispatcher_idle(adreno_dev);
	if (ret)
		return ret;

	return adreno_spin_idle(device);
}

/**
 * adreno_drain() - Drain the dispatch queue
 * @device: Pointer to the KGSL device structure for the GPU
 *
 * Drain the dispatcher of existing command batches.  This halts
 * additional commands from being issued until the gate is completed.
 */
static int adreno_drain(struct kgsl_device *device)
{
	INIT_COMPLETION(device->cmdbatch_gate);

	return 0;
}

/* Caller must hold the device mutex. */
static int adreno_suspend_context(struct kgsl_device *device)
{
	int status = 0;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);

	/* process any profiling results that are available */
	adreno_profile_process_results(adreno_dev);

	status = adreno_idle(device);
	if (status)
		return status;
	/* set the device to default pagetable */
	kgsl_mmu_set_pt(&device->mmu, device->mmu.defaultpagetable);
	kgsl_sharedmem_writel(device,
		&adreno_dev->ringbuffers[0].pagetable_desc,
		offsetof(struct adreno_ringbuffer_pagetable_info,
			current_global_ptname), 0);
	/* set ringbuffers to NULL ctxt */
	adreno_set_active_ctx_null(adreno_dev);

	return status;
}

/**
 * adreno_read - General read function to read adreno device memory
 * @device - Pointer to the GPU device struct (for adreno device)
 * @base - Base address (kernel virtual) where the device memory is mapped
 * @offsetwords - Offset in words from the base address, of the memory that
 * is to be read
 * @value - Value read from the device memory
 * @mem_len - Length of the device memory mapped to the kernel
 */
static void adreno_read(struct kgsl_device *device, void *base,
		unsigned int offsetwords, unsigned int *value,
		unsigned int mem_len)
{

	unsigned int *reg;
	BUG_ON(offsetwords*sizeof(uint32_t) >= mem_len);
	reg = (unsigned int *)(base + (offsetwords << 2));

	if (!in_interrupt())
		kgsl_pre_hwaccess(device);

	/*ensure this read finishes before the next one.
	 * i.e. act like normal readl() */
	*value = __raw_readl(reg);
	rmb();
}

/**
 * adreno_regread - Used to read adreno device registers
 * @offsetwords - Word (4 Bytes) offset to the register to be read
 * @value - Value read from device register
 */
static void adreno_regread(struct kgsl_device *device, unsigned int offsetwords,
	unsigned int *value)
{
	adreno_read(device, device->reg_virt, offsetwords, value,
						device->reg_len);
}

/**
 * adreno_shadermem_regread - Used to read GPU (adreno) shader memory
 * @device - GPU device whose shader memory is to be read
 * @offsetwords - Offset in words, of the shader memory address to be read
 * @value - Pointer to where the read shader mem value is to be stored
 */
void adreno_shadermem_regread(struct kgsl_device *device,
	unsigned int offsetwords, unsigned int *value)
{
	adreno_read(device, device->shader_mem_virt, offsetwords, value,
					device->shader_mem_len);
}

static void adreno_regwrite(struct kgsl_device *device,
				unsigned int offsetwords,
				unsigned int value)
{
	unsigned int *reg;

	BUG_ON(offsetwords*sizeof(uint32_t) >= device->reg_len);

	if (!in_interrupt())
		kgsl_pre_hwaccess(device);

	trace_kgsl_regwrite(device, offsetwords, value);

	kgsl_cffdump_regwrite(device, offsetwords << 2, value);
	reg = (unsigned int *)(device->reg_virt + (offsetwords << 2));

	/*ensure previous writes post before this one,
	 * i.e. act like normal writel() */
	wmb();
	__raw_writel(value, reg);
}

/**
 * adreno_waittimestamp - sleep while waiting for the specified timestamp
 * @device - pointer to a KGSL device structure
 * @context - pointer to the active kgsl context
 * @timestamp - GPU timestamp to wait for
 * @msecs - amount of time to wait (in milliseconds)
 *
 * Wait up to 'msecs' milliseconds for the specified timestamp to expire.
 */
static int adreno_waittimestamp(struct kgsl_device *device,
		struct kgsl_context *context,
		unsigned int timestamp,
		unsigned int msecs)
{
	int ret;

	if (context == NULL) {
		/* If they are doing then complain once */
		dev_WARN_ONCE(device->dev, 1,
			"IOCTL_KGSL_DEVICE_WAITTIMESTAMP is deprecated\n");
		return -ENOTTY;
	}

	/* Return -EINVAL if the context has been detached */
	if (kgsl_context_detached(context))
		return -EINVAL;

	ret = adreno_drawctxt_wait(ADRENO_DEVICE(device), context,
		timestamp, msecs);

	/* If the context got invalidated then return a specific error */
	if (kgsl_context_invalid(context))
		ret = -EDEADLK;

	/*
	 * Return -EPROTO if the device has faulted since the last time we
	 * checked.  Userspace uses this as a marker for performing post
	 * fault activities
	 */

	if (!ret && test_and_clear_bit(ADRENO_CONTEXT_FAULT, &context->priv))
		ret = -EPROTO;

	return ret;
}

/**
 * __adreno_readtimestamp() - Reads the timestamp from memstore memory
 * @device: Pointer to device whose memstore is read
 * @index: Index into the memstore memory
 * @type: Type of timestamp to read
 * @timestamp: The out parameter where the timestamp is read
 */
int __adreno_readtimestamp(struct kgsl_device *device, int index, int type,
		unsigned int *timestamp)
{
	int status = 0;

	switch (type) {
	case KGSL_TIMESTAMP_CONSUMED:
		kgsl_sharedmem_readl(&device->memstore, timestamp,
			KGSL_MEMSTORE_OFFSET(index, soptimestamp));
		break;
	case KGSL_TIMESTAMP_RETIRED:
		kgsl_sharedmem_readl(&device->memstore, timestamp,
			KGSL_MEMSTORE_OFFSET(index, eoptimestamp));
		break;
	default:
		status = -EINVAL;
		*timestamp = 0;
		break;
	}
	return status;
}

/**
 * adreno_rb_readtimestamp(): Return the value of given type of timestamp
 * for a RB
 * @device: GPU device whose timestamp values are being queried
 * @priv: The object being queried for a timestamp (expected to be a rb pointer)
 * @type: The type of timestamp (one of 3) to be read
 * @timestamp: Pointer to where the read timestamp is to be written to
 *
 * CONSUMED and RETIRED type timestamps are sorted by id and are constantly
 * updated by the GPU through shared memstore memory. QUEUED type timestamps
 * are read directly from context struct.

 * The function returns 0 on success and timestamp value at the *timestamp
 * address and returns -EINVAL on any read error/invalid type and timestamp = 0.
 */
int adreno_rb_readtimestamp(struct kgsl_device *device,
		void *priv, enum kgsl_timestamp_type type,
		unsigned int *timestamp)
{
	int status = 0;
	struct adreno_ringbuffer *rb = priv;

	/*
	 * If user passed in a NULL pointer for timestamp, return without
	 * doing anything.
	 */
	if (!timestamp)
		return status;

	if (KGSL_TIMESTAMP_QUEUED == type)
		*timestamp = rb->timestamp;
	else
		status = __adreno_readtimestamp(device,
				rb->id + KGSL_MEMSTORE_MAX,
				type, timestamp);

	return status;
}

/**
 * adreno_readtimestamp(): Return the value of given type of timestamp
 * @device: GPU device whose timestamp values are being queried
 * @priv: The object being queried for a timestamp (expected to be a context)
 * @type: The type of timestamp (one of 3) to be read
 * @timestamp: Pointer to where the read timestamp is to be written to
 *
 * CONSUMED and RETIRED type timestamps are sorted by id and are constantly
 * updated by the GPU through shared memstore memory. QUEUED type timestamps
 * are read directly from context struct.

 * The function returns 0 on success and timestamp value at the *timestamp
 * address and returns -EINVAL on any read error/invalid type and timestamp = 0.
 */
static int adreno_readtimestamp(struct kgsl_device *device,
		void *priv, enum kgsl_timestamp_type type,
		unsigned int *timestamp)
{
	int status = 0;
	struct kgsl_context *context = priv;
	unsigned int id = KGSL_CONTEXT_ID(context);

	BUG_ON(NULL == context || id >= KGSL_MEMSTORE_MAX);
	/*
	 * If user passed in a NULL pointer for timestamp, return without
	 * doing anything.
	 */
	if (!timestamp)
		return status;

	if (KGSL_TIMESTAMP_QUEUED == type)
		*timestamp = adreno_context_timestamp(context);
	else
		status = __adreno_readtimestamp(device,
				context->id, type, timestamp);

	return status;
}

static long adreno_ioctl(struct kgsl_device_private *dev_priv,
			      unsigned int cmd, void *data)
{
	struct kgsl_device *device = dev_priv->device;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	int result = 0;

	switch (cmd) {
	case IOCTL_KGSL_PERFCOUNTER_GET: {
		struct kgsl_perfcounter_get *get = data;
		mutex_lock(&device->mutex);
		/*
		 * adreno_perfcounter_get() is called by kernel clients
		 * during start(), so it is not safe to take an
		 * active count inside this function.
		 */
		result = kgsl_active_count_get(device);
		if (result == 0) {
			result = adreno_perfcounter_get(adreno_dev,
				get->groupid, get->countable, &get->offset,
				&get->offset_hi, PERFCOUNTER_FLAG_NONE);
			kgsl_active_count_put(device);
		}
		mutex_unlock(&device->mutex);
		break;
	}
	case IOCTL_KGSL_PERFCOUNTER_PUT: {
		struct kgsl_perfcounter_put *put = data;
		mutex_lock(&device->mutex);
		result = adreno_perfcounter_put(adreno_dev, put->groupid,
			put->countable, PERFCOUNTER_FLAG_NONE);
		mutex_unlock(&device->mutex);
		break;
	}
	case IOCTL_KGSL_PERFCOUNTER_QUERY: {
		struct kgsl_perfcounter_query *query = data;
		result = adreno_perfcounter_query_group(adreno_dev,
			query->groupid, query->countables,
			query->count, &query->max_counters);
		break;
	}
	case IOCTL_KGSL_PERFCOUNTER_READ: {
		struct kgsl_perfcounter_read *read = data;
		result = adreno_perfcounter_read_group(adreno_dev,
			read->reads, read->count);
		break;
	}
	default:
		KGSL_DRV_INFO(dev_priv->device,
			"invalid ioctl code %08x\n", cmd);
		result = -ENOIOCTLCMD;
		break;
	}
	return result;

}

static inline s64 adreno_ticks_to_us(u32 ticks, u32 freq)
{
	freq /= 1000000;
	return ticks / freq;
}

static void adreno_power_stats(struct kgsl_device *device,
				struct kgsl_power_stats *stats)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct adreno_gpudev *gpudev  = ADRENO_GPU_DEVICE(adreno_dev);
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;
	struct adreno_busy_data busy_data;

	memset(stats, 0, sizeof(*stats));

	/*
	 * If we're not currently active, there shouldn't have been
	 * any cycles since the last time this function was called.
	 */

	if (device->state != KGSL_STATE_ACTIVE)
		return;

	/* Get the busy cycles counted since the counter was last reset */
	gpudev->busy_cycles(adreno_dev, &busy_data);

	stats->busy_time = adreno_ticks_to_us(busy_data.gpu_busy,
					      kgsl_pwrctrl_active_freq(pwr));
	stats->ram_time = busy_data.vbif_ram_cycles;
	stats->ram_wait = busy_data.vbif_starved_ram;
}

static unsigned int adreno_gpuid(struct kgsl_device *device,
	unsigned int *chipid)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);

	/* Some applications need to know the chip ID too, so pass
	 * that as a parameter */

	if (chipid != NULL)
		*chipid = adreno_dev->chipid;

	/* Standard KGSL gpuid format:
	 * top word is 0x0002 for 2D or 0x0003 for 3D
	 * Bottom word is core specific identifer
	 */

	return (0x0003 << 16) | ADRENO_GPUREV(adreno_dev);
}

static void adreno_regulator_enable(struct kgsl_device *device)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct adreno_gpudev *gpudev  = ADRENO_GPU_DEVICE(adreno_dev);
	if (gpudev->regulator_enable &&
		!test_bit(ADRENO_DEVICE_GPU_REGULATOR_ENABLED,
			&adreno_dev->priv)) {
		gpudev->regulator_enable(adreno_dev);
		set_bit(ADRENO_DEVICE_GPU_REGULATOR_ENABLED,
			&adreno_dev->priv);
	}
}

static bool adreno_is_hw_collapsible(struct kgsl_device *device)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct adreno_gpudev *gpudev  = ADRENO_GPU_DEVICE(adreno_dev);

	return adreno_isidle(device) && (gpudev->is_sptp_idle ?
				gpudev->is_sptp_idle(adreno_dev) : true);
}

static void adreno_regulator_disable(struct kgsl_device *device)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct adreno_gpudev *gpudev  = ADRENO_GPU_DEVICE(adreno_dev);
	if (gpudev->regulator_disable &&
		test_bit(ADRENO_DEVICE_GPU_REGULATOR_ENABLED,
			&adreno_dev->priv)) {
		gpudev->regulator_disable(adreno_dev);
		clear_bit(ADRENO_DEVICE_GPU_REGULATOR_ENABLED,
			&adreno_dev->priv);
	}
}

static const struct kgsl_functable adreno_functable = {
	/* Mandatory functions */
	.regread = adreno_regread,
	.regwrite = adreno_regwrite,
	.idle = adreno_idle,
	.isidle = adreno_isidle,
	.suspend_context = adreno_suspend_context,
	.init = adreno_init,
	.start = adreno_start,
	.stop = adreno_stop,
	.getproperty = adreno_getproperty,
	.getproperty_compat = adreno_getproperty_compat,
	.waittimestamp = adreno_waittimestamp,
	.readtimestamp = adreno_readtimestamp,
	.issueibcmds = adreno_ringbuffer_issueibcmds,
	.ioctl = adreno_ioctl,
	.compat_ioctl = adreno_compat_ioctl,
	.power_stats = adreno_power_stats,
	.gpuid = adreno_gpuid,
	.snapshot = adreno_snapshot,
	.irq_handler = adreno_irq_handler,
	.drain = adreno_drain,
	/* Optional functions */
	.drawctxt_create = adreno_drawctxt_create,
	.drawctxt_detach = adreno_drawctxt_detach,
	.drawctxt_destroy = adreno_drawctxt_destroy,
	.drawctxt_dump = adreno_drawctxt_dump,
	.setproperty = adreno_setproperty,
	.setproperty_compat = adreno_setproperty_compat,
	.drawctxt_sched = adreno_drawctxt_sched,
	.resume = adreno_dispatcher_start,
	.regulator_enable = adreno_regulator_enable,
	.is_hw_collapsible = adreno_is_hw_collapsible,
	.regulator_disable = adreno_regulator_disable,

};

static struct platform_driver adreno_platform_driver = {
	.probe = adreno_probe,
	.remove = adreno_remove,
	.suspend = kgsl_suspend_driver,
	.resume = kgsl_resume_driver,
	.id_table = adreno_id_table,
	.driver = {
		.owner = THIS_MODULE,
		.name = DEVICE_3D_NAME,
		.pm = &kgsl_pm_ops,
		.of_match_table = adreno_match_table,
	}
};

static int __init kgsl_3d_init(void)
{
	return platform_driver_register(&adreno_platform_driver);
}

static void __exit kgsl_3d_exit(void)
{
	platform_driver_unregister(&adreno_platform_driver);
}

module_init(kgsl_3d_init);
module_exit(kgsl_3d_exit);


static struct of_device_id busmon_match_table[] = {
	{ .compatible = "qcom,kgsl-busmon", .data = &device_3d0 },
	{}
};

static int kgsl_busmon_probe(struct platform_device *pdev)
{
	struct kgsl_device *device;
	const struct of_device_id *pdid =
			of_match_device(busmon_match_table, &pdev->dev);

	device = (struct kgsl_device *)pdid->data;
	device->busmondev = &pdev->dev;
	dev_set_drvdata(device->busmondev, device);

	return 0;
}

static struct platform_driver kgsl_bus_platform_driver = {
	.probe = kgsl_busmon_probe,
	.driver = {
		.owner = THIS_MODULE,
		.name = "kgsl-busmon",
	.of_match_table = busmon_match_table,
	}
};

static int __init kgsl_busmon_init(void)
{
	return platform_driver_register(&kgsl_bus_platform_driver);
}

static void __exit kgsl_busmon_exit(void)
{
	platform_driver_unregister(&kgsl_bus_platform_driver);
}

module_init(kgsl_busmon_init);
module_exit(kgsl_busmon_exit);

MODULE_DESCRIPTION("3D Graphics driver");
MODULE_VERSION("1.2");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:kgsl_3d");
