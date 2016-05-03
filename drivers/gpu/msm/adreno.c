/* Copyright (c) 2002,2007-2016, The Linux Foundation. All rights reserved.
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
#include <linux/sched.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <soc/qcom/scm.h>

#include <linux/msm-bus-board.h>
#include <linux/msm-bus.h>

#include "kgsl.h"
#include "kgsl_pwrscale.h"
#include "kgsl_cffdump.h"
#include "kgsl_sharedmem.h"
#include "kgsl_iommu.h"
#include "kgsl_trace.h"

#include "adreno.h"
#include "adreno_iommu.h"
#include "adreno_compat.h"
#include "adreno_pm4types.h"
#include "adreno_trace.h"

#include "a3xx_reg.h"
#include "adreno_snapshot.h"

/* Include the master list of GPU cores that are supported */
#include "adreno-gpulist.h"

#undef MODULE_PARAM_PREFIX
#define MODULE_PARAM_PREFIX "adreno."

static bool nopreempt;
module_param(nopreempt, bool, 0444);
MODULE_PARM_DESC(nopreempt, "Disable GPU preemption");

#define DRIVER_VERSION_MAJOR   3
#define DRIVER_VERSION_MINOR   1

/* Number of times to try hard reset */
#define NUM_TIMES_RESET_RETRY 5

#define KGSL_LOG_LEVEL_DEFAULT 3

static void adreno_input_work(struct work_struct *work);
static unsigned int counter_delta(struct kgsl_device *device,
	unsigned int reg, unsigned int *counter);

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
			.irq_name = "kgsl_3d0_irq",
		},
		.iomemname = "kgsl_3d0_reg_memory",
		.shadermemname = "kgsl_3d0_shader_memory",
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
	.ft_policy = KGSL_FT_DEFAULT_POLICY,
	.ft_pf_policy = KGSL_FT_PAGEFAULT_DEFAULT_POLICY,
	.fast_hang_detect = 1,
	.long_ib_detect = 1,
	.input_work = __WORK_INITIALIZER(device_3d0.input_work,
		adreno_input_work),
	.pwrctrl_flag = BIT(ADRENO_SPTP_PC_CTRL) | BIT(ADRENO_PPD_CTRL) |
		BIT(ADRENO_LM_CTRL) | BIT(ADRENO_HWCG_CTRL) |
		BIT(ADRENO_THROTTLING_CTRL),
	.profile.enabled = false,
	.active_list = LIST_HEAD_INIT(device_3d0.active_list),
	.active_list_lock = __SPIN_LOCK_UNLOCKED(device_3d0.active_list_lock),
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
int adreno_wake_nice = -7;

/* Number of milliseconds to stay active active after a wake on touch */
unsigned int adreno_wake_timeout = 100;

/**
 * adreno_readreg64() - Read a 64bit register by getting its offset from the
 * offset array defined in gpudev node
 * @adreno_dev:		Pointer to the the adreno device
 * @lo:	lower 32bit register enum that is to be read
 * @hi:	higher 32bit register enum that is to be read
 * @val: 64 bit Register value read is placed here
 */
void adreno_readreg64(struct adreno_device *adreno_dev,
		enum adreno_regs lo, enum adreno_regs hi, uint64_t *val)
{
	struct adreno_gpudev *gpudev = ADRENO_GPU_DEVICE(adreno_dev);
	unsigned int val_lo = 0, val_hi = 0;
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);

	if (adreno_checkreg_off(adreno_dev, lo))
		kgsl_regread(device, gpudev->reg_offsets->offsets[lo], &val_lo);
	if (adreno_checkreg_off(adreno_dev, hi))
		kgsl_regread(device, gpudev->reg_offsets->offsets[hi], &val_hi);

	*val = (val_lo | ((uint64_t)val_hi << 32));
}

/**
 * adreno_writereg64() - Write a 64bit register by getting its offset from the
 * offset array defined in gpudev node
 * @adreno_dev:	Pointer to the the adreno device
 * @lo:	lower 32bit register enum that is to be written
 * @hi:	higher 32bit register enum that is to be written
 * @val: 64 bit value to write
 */
void adreno_writereg64(struct adreno_device *adreno_dev,
		enum adreno_regs lo, enum adreno_regs hi, uint64_t val)
{
	struct adreno_gpudev *gpudev = ADRENO_GPU_DEVICE(adreno_dev);

	if (adreno_checkreg_off(adreno_dev, lo))
		kgsl_regwrite(KGSL_DEVICE(adreno_dev),
			gpudev->reg_offsets->offsets[lo], lower_32_bits(val));
	if (adreno_checkreg_off(adreno_dev, hi))
		kgsl_regwrite(KGSL_DEVICE(adreno_dev),
			gpudev->reg_offsets->offsets[hi], upper_32_bits(val));
}

/**
 * adreno_get_rptr() - Get the current ringbuffer read pointer
 * @rb: Pointer the ringbuffer to query
 *
 * Get the latest rptr
 */
unsigned int adreno_get_rptr(struct adreno_ringbuffer *rb)
{
	struct adreno_device *adreno_dev = ADRENO_RB_DEVICE(rb);
	unsigned int rptr = 0;

	if (adreno_is_a3xx(adreno_dev))
		adreno_readreg(adreno_dev, ADRENO_REG_CP_RB_RPTR,
				&rptr);
	else {
		struct kgsl_device *device = KGSL_DEVICE(adreno_dev);

		kgsl_sharedmem_readl(&device->scratch, &rptr,
				SCRATCH_RPTR_OFFSET(rb->id));
	}

	return rptr;
}

/**
 * adreno_of_read_property() - Adreno read property
 * @node: Device node
 *
 * Read a u32 property.
 */
static inline int adreno_of_read_property(struct device_node *node,
	const char *prop, unsigned int *ptr)
{
	int ret = of_property_read_u32(node, prop, ptr);
	if (ret)
		KGSL_CORE_ERR("Unable to read '%s'\n", prop);
	return ret;
}

static void __iomem *efuse_base;
static size_t efuse_len;

int adreno_efuse_map(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct resource *res;

	if (efuse_base != NULL)
		return 0;

	res = platform_get_resource_byname(device->pdev, IORESOURCE_MEM,
		"qfprom_memory");

	if (res == NULL)
		return -ENODEV;

	efuse_base = ioremap(res->start, resource_size(res));
	if (efuse_base == NULL)
		return -ENODEV;

	efuse_len = resource_size(res);
	return 0;
}

void adreno_efuse_unmap(struct adreno_device *adreno_dev)
{
	if (efuse_base != NULL) {
		iounmap(efuse_base);
		efuse_base = NULL;
		efuse_len = 0;
	}
}

int adreno_efuse_read_u32(struct adreno_device *adreno_dev, unsigned int offset,
		unsigned int *val)
{
	if (efuse_base == NULL)
		return -ENODEV;

	if (offset >= efuse_len)
		return -ERANGE;

	if (val != NULL) {
		*val = readl_relaxed(efuse_base + offset);
		/* Make sure memory is updated before returning */
		rmb();
	}

	return 0;
}

static int _get_counter(struct adreno_device *adreno_dev,
		int group, int countable, unsigned int *lo,
		unsigned int *hi)
{
	int ret = 0;

	if (*lo == 0) {

		ret = adreno_perfcounter_get(adreno_dev, group, countable,
			lo, hi, PERFCOUNTER_FLAG_KERNEL);

		if (ret) {
			KGSL_DRV_ERR(KGSL_DEVICE(adreno_dev),
				"Unable to allocate fault detect performance counter %d/%d\n",
				group, countable);
			KGSL_DRV_ERR(KGSL_DEVICE(adreno_dev),
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

	if (!test_bit(ADRENO_DEVICE_SOFT_FAULT_DETECT, &adreno_dev->priv))
		return;

	if (adreno_dev->fast_hang_detect == 1)
		return;

	for (i = 0; i < gpudev->ft_perf_counters_count; i++) {
		_get_counter(adreno_dev, gpudev->ft_perf_counters[i].counter,
			 gpudev->ft_perf_counters[i].countable,
			 &adreno_ft_regs[j + (i * 2)],
			 &adreno_ft_regs[j + ((i * 2) + 1)]);
	}

	adreno_dev->fast_hang_detect = 1;
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

	if (!test_bit(ADRENO_DEVICE_SOFT_FAULT_DETECT, &adreno_dev->priv))
		return;

	if (!adreno_dev->fast_hang_detect)
		return;

	for (i = 0; i < gpudev->ft_perf_counters_count; i++) {
		_put_counter(adreno_dev, gpudev->ft_perf_counters[i].counter,
			 gpudev->ft_perf_counters[i].countable,
			 &adreno_ft_regs[j + (i * 2)],
			 &adreno_ft_regs[j + ((i * 2) + 1)]);

	}

	adreno_dev->fast_hang_detect = 0;
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
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);

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
		jiffies + msecs_to_jiffies(adreno_wake_timeout));
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
static void _soft_reset(struct adreno_device *adreno_dev)
{
	struct adreno_gpudev *gpudev  = ADRENO_GPU_DEVICE(adreno_dev);
	unsigned int reg;

	/*
	 * On a530 v1 RBBM cannot be reset in soft reset.
	 * Reset all blocks except RBBM for a530v1.
	 */
	if (adreno_is_a530v1(adreno_dev)) {
		adreno_writereg(adreno_dev, ADRENO_REG_RBBM_BLOCK_SW_RESET_CMD,
						 0xFFDFFC0);
		adreno_writereg(adreno_dev, ADRENO_REG_RBBM_BLOCK_SW_RESET_CMD2,
						0x1FFFFFFF);
	} else {

		adreno_writereg(adreno_dev, ADRENO_REG_RBBM_SW_RESET_CMD, 1);
		/*
		 * Do a dummy read to get a brief read cycle delay for the
		 * reset to take effect
		 */
		adreno_readreg(adreno_dev, ADRENO_REG_RBBM_SW_RESET_CMD, &reg);
		adreno_writereg(adreno_dev, ADRENO_REG_RBBM_SW_RESET_CMD, 0);
	}

	/* The SP/TP regulator gets turned off after a soft reset */

	if (gpudev->regulator_enable)
		gpudev->regulator_enable(adreno_dev);
}


void adreno_irqctrl(struct adreno_device *adreno_dev, int state)
{
	struct adreno_gpudev *gpudev = ADRENO_GPU_DEVICE(adreno_dev);
	unsigned int mask = state ? gpudev->irq->mask : 0;

	adreno_writereg(adreno_dev, ADRENO_REG_RBBM_INT_0_MASK, mask);
}

 /*
 * adreno_hang_int_callback() - Isr for fatal interrupts that hang GPU
 * @adreno_dev: Pointer to device
 * @bit: Interrupt bit
 */
void adreno_hang_int_callback(struct adreno_device *adreno_dev, int bit)
{
	KGSL_DRV_CRIT_RATELIMIT(KGSL_DEVICE(adreno_dev),
			"MISC: GPU hang detected\n");
	adreno_irqctrl(adreno_dev, 0);

	/* Trigger a fault in the dispatcher - this will effect a restart */
	adreno_set_gpu_fault(adreno_dev, ADRENO_HARD_FAULT);
	adreno_dispatcher_schedule(KGSL_DEVICE(adreno_dev));
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
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);

	kgsl_schedule_work(&device->event_work);
	adreno_dispatcher_schedule(device);
}

static irqreturn_t adreno_irq_handler(struct kgsl_device *device)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct adreno_gpudev *gpudev = ADRENO_GPU_DEVICE(adreno_dev);
	struct adreno_irq *irq_params = gpudev->irq;
	irqreturn_t ret = IRQ_NONE;
	unsigned int status = 0, tmp;
	int i;

	adreno_readreg(adreno_dev, ADRENO_REG_RBBM_INT_0_STATUS, &status);

	/* Loop through all set interrupts and call respective handlers */
	for (tmp = status; tmp != 0;) {
		i = fls(tmp) - 1;

		if (irq_params->funcs[i].func != NULL) {
			if (irq_params->mask & BIT(i))
				irq_params->funcs[i].func(adreno_dev, i);
		} else
			KGSL_DRV_CRIT_RATELIMIT(device,
					"Unhandled interrupt bit %x\n", i);

		ret = IRQ_HANDLED;

		tmp &= ~BIT(i);
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

	if (kgsl_property_read_u32(KGSL_DEVICE(adreno_dev), "qcom,chipid",
		&adreno_dev->chipid))
		KGSL_DRV_FATAL(KGSL_DEVICE(adreno_dev),
			"No GPU chip ID was specified\n");

	adreno_dev->gpucore = _get_gpu_core(adreno_dev->chipid);

	if (adreno_dev->gpucore == NULL)
		KGSL_DRV_FATAL(KGSL_DEVICE(adreno_dev),
			"Unknown GPU chip ID %8.8X\n", adreno_dev->chipid);

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

	/* Do target specific identification */
	if (gpudev->platform_setup != NULL)
		gpudev->platform_setup(adreno_dev);
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

static int adreno_of_parse_pwrlevels(struct adreno_device *adreno_dev,
		struct device_node *node)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;
	struct device_node *child;

	pwr->num_pwrlevels = 0;

	for_each_child_of_node(node, child) {
		unsigned int index;
		struct kgsl_pwrlevel *level;

		if (adreno_of_read_property(child, "reg", &index))
			return -EINVAL;

		if (index >= KGSL_MAX_PWRLEVELS) {
			KGSL_CORE_ERR("Pwrlevel index %d is out of range\n",
				index);
			continue;
		}

		if (index >= pwr->num_pwrlevels)
			pwr->num_pwrlevels = index + 1;

		level = &pwr->pwrlevels[index];

		if (adreno_of_read_property(child, "qcom,gpu-freq",
			&level->gpu_freq))
			return -EINVAL;

		if (adreno_of_read_property(child, "qcom,bus-freq",
			&level->bus_freq))
			return -EINVAL;

		if (of_property_read_u32(child, "qcom,bus-min",
			&level->bus_min))
			level->bus_min = level->bus_freq;

		if (of_property_read_u32(child, "qcom,bus-max",
			&level->bus_max))
			level->bus_max = level->bus_freq;
	}

	return 0;
}


static void adreno_of_get_initial_pwrlevel(struct adreno_device *adreno_dev,
		struct device_node *node)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;
	int init_level = 1;

	of_property_read_u32(node, "qcom,initial-pwrlevel", &init_level);

	if (init_level < 0 || init_level > pwr->num_pwrlevels)
		init_level = 1;

	pwr->active_pwrlevel = init_level;
	pwr->default_pwrlevel = init_level;
}

static int adreno_of_get_legacy_pwrlevels(struct adreno_device *adreno_dev,
		struct device_node *parent)
{
	struct device_node *node;
	int ret;

	node = of_find_node_by_name(parent, "qcom,gpu-pwrlevels");

	if (node == NULL) {
		KGSL_CORE_ERR("Unable to find 'qcom,gpu-pwrlevels'\n");
		return -EINVAL;
	}

	ret = adreno_of_parse_pwrlevels(adreno_dev, node);
	if (ret == 0)
		adreno_of_get_initial_pwrlevel(adreno_dev, parent);
	return ret;
}

static int adreno_of_get_pwrlevels(struct adreno_device *adreno_dev,
		struct device_node *parent)
{
	struct device_node *node, *child;

	node = of_find_node_by_name(parent, "qcom,gpu-pwrlevel-bins");
	if (node == NULL)
		return adreno_of_get_legacy_pwrlevels(adreno_dev, parent);

	for_each_child_of_node(node, child) {
		unsigned int bin;

		if (of_property_read_u32(child, "qcom,speed-bin", &bin))
			continue;

		if (bin == adreno_dev->speed_bin) {
			int ret;

			ret = adreno_of_parse_pwrlevels(adreno_dev, child);
			if (ret == 0)
				adreno_of_get_initial_pwrlevel(adreno_dev,
								child);
			return ret;
		}
	}

	return -ENODEV;
}

static inline struct adreno_device *adreno_get_dev(struct platform_device *pdev)
{
	const struct of_device_id *of_id =
		of_match_device(adreno_match_table, &pdev->dev);

	return of_id ? (struct adreno_device *) of_id->data : NULL;
}

static struct {
	unsigned int quirk;
	const char *prop;
} adreno_quirks[] = {
	 { ADRENO_QUIRK_TWO_PASS_USE_WFI, "qcom,gpu-quirk-two-pass-use-wfi" },
	 { ADRENO_QUIRK_IOMMU_SYNC, "qcom,gpu-quirk-iommu-sync" },
	 { ADRENO_QUIRK_CRITICAL_PACKETS, "qcom,gpu-quirk-critical-packets" },
	 { ADRENO_QUIRK_FAULT_DETECT_MASK, "qcom,gpu-quirk-fault-detect-mask" },
};

static int adreno_of_get_power(struct adreno_device *adreno_dev,
		struct platform_device *pdev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct device_node *node = pdev->dev.of_node;
	int i;
	unsigned int timeout;

	if (of_property_read_string(node, "label", &pdev->name)) {
		KGSL_CORE_ERR("Unable to read 'label'\n");
		return -EINVAL;
	}

	if (adreno_of_read_property(node, "qcom,id", &pdev->id))
		return -EINVAL;

	/* Set up quirks and other boolean options */
	for (i = 0; i < ARRAY_SIZE(adreno_quirks); i++) {
		if (of_property_read_bool(node, adreno_quirks[i].prop))
			adreno_dev->quirks |= adreno_quirks[i].quirk;
	}

	if (adreno_of_get_pwrlevels(adreno_dev, node))
		return -EINVAL;

	/* get pm-qos-active-latency, set it to default if not found */
	if (of_property_read_u32(node, "qcom,pm-qos-active-latency",
		&device->pwrctrl.pm_qos_active_latency))
		device->pwrctrl.pm_qos_active_latency = 501;

	/* get pm-qos-cpu-mask-latency, set it to default if not found */
	if (of_property_read_u32(node, "qcom,l2pc-cpu-mask-latency",
		&device->pwrctrl.pm_qos_cpu_mask_latency))
		device->pwrctrl.pm_qos_cpu_mask_latency = 501;

	/* get pm-qos-wakeup-latency, set it to default if not found */
	if (of_property_read_u32(node, "qcom,pm-qos-wakeup-latency",
		&device->pwrctrl.pm_qos_wakeup_latency))
		device->pwrctrl.pm_qos_wakeup_latency = 101;

	if (of_property_read_u32(node, "qcom,idle-timeout", &timeout))
		timeout = 80;

	device->pwrctrl.interval_timeout = msecs_to_jiffies(timeout);

	device->pwrctrl.strtstp_sleepwake =
		of_property_read_bool(node, "qcom,strtstp-sleepwake");

	device->pwrctrl.bus_control = of_property_read_bool(node,
		"qcom,bus-control");

	return 0;
}

#ifdef CONFIG_QCOM_OCMEM
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

static int adreno_probe(struct platform_device *pdev)
{
	struct kgsl_device *device;
	struct adreno_device *adreno_dev;
	int status;

	adreno_dev = adreno_get_dev(pdev);

	if (adreno_dev == NULL) {
		pr_err("adreno: qcom,kgsl-3d0 does not exist in the device tree");
		return -ENODEV;
	}

	device = KGSL_DEVICE(adreno_dev);
	device->pdev = pdev;

	/* Get the chip ID from the DT and set up target specific parameters */
	adreno_identify_gpu(adreno_dev);

	status = adreno_of_get_power(adreno_dev, pdev);
	if (status) {
		device->pdev = NULL;
		return status;
	}

	/*
	* The SMMU APIs use unsigned long for virtual addresses which means
	* that we cannot use 64 bit virtual addresses on a 32 bit kernel even
	* though the hardware and the rest of the KGSL driver supports it.
	*/
	if (adreno_support_64bit(adreno_dev))
		device->mmu.features |= KGSL_MMU_64BIT;

	status = kgsl_device_platform_probe(device);
	if (status) {
		device->pdev = NULL;
		return status;
	}

	/*
	 * qcom,iommu-secure-id is used to identify MMUs that can handle secure
	 * content but that is only part of the story - the GPU also has to be
	 * able to handle secure content.  Unfortunately in a classic catch-22
	 * we cannot identify the GPU until after the DT is parsed. tl;dr -
	 * check the GPU capabilities here and modify mmu->secured accordingly
	 */

	if (!ADRENO_FEATURE(adreno_dev, ADRENO_CONTENT_PROTECTION))
		device->mmu.secured = false;

	status = adreno_ringbuffer_probe(adreno_dev, nopreempt);
	if (status)
		goto out;

	status = adreno_dispatcher_init(adreno_dev);
	if (status)
		goto out;

	adreno_debugfs_init(adreno_dev);
	adreno_profile_init(adreno_dev);

	adreno_sysfs_init(adreno_dev);

	kgsl_pwrscale_init(&pdev->dev, CONFIG_QCOM_ADRENO_DEFAULT_GOVERNOR);

	/* Initialize coresight for the target */
	adreno_coresight_init(adreno_dev);

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

static void _adreno_free_memories(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);

	if (test_bit(ADRENO_DEVICE_CMDBATCH_PROFILE, &adreno_dev->priv))
		kgsl_free_global(device, &adreno_dev->cmdbatch_profile_buffer);

	/* Free local copies of firmware and other command streams */
	kfree(adreno_dev->pfp_fw);
	adreno_dev->pfp_fw = NULL;

	kfree(adreno_dev->pm4_fw);
	adreno_dev->pm4_fw = NULL;

	kfree(adreno_dev->gpmu_cmds);
	adreno_dev->gpmu_cmds = NULL;

	kgsl_free_global(device, &adreno_dev->pm4);
	kgsl_free_global(device, &adreno_dev->pfp);
}

static int adreno_remove(struct platform_device *pdev)
{
	struct adreno_device *adreno_dev = adreno_get_dev(pdev);
	struct adreno_gpudev *gpudev;
	struct kgsl_device *device;

	if (adreno_dev == NULL)
		return 0;

	device = KGSL_DEVICE(adreno_dev);
	gpudev = ADRENO_GPU_DEVICE(adreno_dev);

	if (gpudev->remove != NULL)
		gpudev->remove(adreno_dev);

	/* The memory is fading */
	_adreno_free_memories(adreno_dev);

#ifdef CONFIG_INPUT
	input_unregister_handler(&adreno_input_handler);
#endif
	adreno_sysfs_close(adreno_dev);

	adreno_coresight_remove(adreno_dev);
	adreno_profile_close(adreno_dev);

	kgsl_pwrscale_close(device);

	adreno_dispatcher_close(adreno_dev);
	adreno_ringbuffer_close(adreno_dev);

	adreno_fault_detect_stop(adreno_dev);

	kfree(adreno_ft_regs);
	adreno_ft_regs = NULL;

	kfree(adreno_ft_regs_val);
	adreno_ft_regs_val = NULL;

	if (efuse_base != NULL)
		iounmap(efuse_base);

	adreno_perfcounter_close(adreno_dev);
	kgsl_device_platform_remove(device);

	if (test_bit(ADRENO_DEVICE_PWRON_FIXUP, &adreno_dev->priv)) {
		kgsl_free_global(device, &adreno_dev->pwron_fixup);
		clear_bit(ADRENO_DEVICE_PWRON_FIXUP, &adreno_dev->priv);
	}
	clear_bit(ADRENO_DEVICE_INITIALIZED, &adreno_dev->priv);

	return 0;
}

static void adreno_fault_detect_init(struct adreno_device *adreno_dev)
{
	struct adreno_gpudev *gpudev = ADRENO_GPU_DEVICE(adreno_dev);
	int i, val = adreno_dev->fast_hang_detect;

	/* Disable the fast hang detect bit until we know its a go */
	adreno_dev->fast_hang_detect = 0;

	adreno_ft_regs_num = (ARRAY_SIZE(adreno_ft_regs_default) +
		gpudev->ft_perf_counters_count*2);

	adreno_ft_regs = kzalloc(adreno_ft_regs_num * sizeof(unsigned int),
		GFP_KERNEL);
	adreno_ft_regs_val = kzalloc(adreno_ft_regs_num * sizeof(unsigned int),
		GFP_KERNEL);

	if (adreno_ft_regs == NULL || adreno_ft_regs_val == NULL) {
		kfree(adreno_ft_regs);
		kfree(adreno_ft_regs_val);

		adreno_ft_regs = NULL;
		adreno_ft_regs_val = NULL;

		return;
	}

	for (i = 0; i < ARRAY_SIZE(adreno_ft_regs_default); i++)
		adreno_ft_regs[i] = adreno_getreg(adreno_dev,
			adreno_ft_regs_default[i]);

	set_bit(ADRENO_DEVICE_SOFT_FAULT_DETECT, &adreno_dev->priv);

	if (val)
		adreno_fault_detect_start(adreno_dev);
}

static int adreno_init(struct kgsl_device *device)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct adreno_gpudev *gpudev = ADRENO_GPU_DEVICE(adreno_dev);
	int ret;

	kgsl_pwrctrl_change_state(device, KGSL_STATE_INIT);
	/*
	 * initialization only needs to be done once initially until
	 * device is shutdown
	 */
	if (test_bit(ADRENO_DEVICE_INITIALIZED, &adreno_dev->priv))
		return 0;

	/*
	 * Either the microcode read failed because the usermodehelper isn't
	 * available or the microcode was corrupted. Fail the init and force
	 * the user to try the open() again
	 */

	ret = gpudev->microcode_read(adreno_dev);
	if (ret)
		return ret;

	/* Put the GPU in a responsive state */
	ret = kgsl_pwrctrl_change_state(device, KGSL_STATE_AWARE);
	if (ret)
		return ret;

	ret = adreno_iommu_init(adreno_dev);
	if (ret)
		return ret;

	adreno_perfcounter_init(adreno_dev);
	adreno_fault_detect_init(adreno_dev);

	/* Power down the device */
	kgsl_pwrctrl_change_state(device, KGSL_STATE_INIT);

	if (gpudev->init != NULL)
		gpudev->init(adreno_dev);

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

	/*
	 * Allocate a small chunk of memory for precise cmdbatch profiling for
	 * those targets that have the always on timer
	 */

	if (!adreno_is_a3xx(adreno_dev)) {
		int r = kgsl_allocate_global(device,
			&adreno_dev->cmdbatch_profile_buffer, PAGE_SIZE, 0, 0);

		adreno_dev->cmdbatch_profile_index = 0;

		if (r == 0) {
			set_bit(ADRENO_DEVICE_CMDBATCH_PROFILE,
				&adreno_dev->priv);
			kgsl_sharedmem_set(device,
				&adreno_dev->cmdbatch_profile_buffer, 0, 0,
				PAGE_SIZE);
		}

	}

	if (nopreempt == false &&
		ADRENO_FEATURE(adreno_dev, ADRENO_PREEMPTION)) {
		int r = 0;

		if (gpudev->preemption_init)
			r = gpudev->preemption_init(adreno_dev);

		if (r == 0)
			set_bit(ADRENO_DEVICE_PREEMPTION, &adreno_dev->priv);
		else
			WARN(1, "adreno: GPU preemption is disabled\n");
	}

	return 0;
}

static bool regulators_left_on(struct kgsl_device *device)
{
	int i;

	for (i = 0; i < KGSL_MAX_REGULATORS; i++) {
		struct kgsl_regulator *regulator =
			&device->pwrctrl.regulators[i];

		if (IS_ERR_OR_NULL(regulator->reg))
			break;

		if (regulator_is_enabled(regulator->reg))
			return true;
	}

	return false;
}

static void _setup_throttling_counters(struct adreno_device *adreno_dev)
{
	int i, ret;

	if (!adreno_is_a540(adreno_dev))
		return;

	if (!ADRENO_FEATURE(adreno_dev, ADRENO_GPMU))
		return;

	for (i = 0; i < ADRENO_GPMU_THROTTLE_COUNTERS; i++) {
		/* reset throttled cycles ivalue */
		adreno_dev->busy_data.throttle_cycles[i] = 0;

		if (adreno_dev->gpmu_throttle_counters[i] != 0)
			continue;
		ret = adreno_perfcounter_get(adreno_dev,
			KGSL_PERFCOUNTER_GROUP_GPMU_PWR,
			ADRENO_GPMU_THROTTLE_COUNTERS_BASE_REG + i,
			&adreno_dev->gpmu_throttle_counters[i],
			NULL,
			PERFCOUNTER_FLAG_KERNEL);
		WARN_ONCE(ret,  "Unable to get clock throttling counter %x\n",
			ADRENO_GPMU_THROTTLE_COUNTERS_BASE_REG + i);
	}
}

/* FW driven idle 10% throttle */
#define IDLE_10PCT 0
/* number of cycles when clock is throttled by 50% (CRC) */
#define CRC_50PCT  1
/* number of cycles when clock is throttled by more than 50% (CRC) */
#define CRC_MORE50PCT 2
/* number of cycles when clock is throttle by less than 50% (CRC) */
#define CRC_LESS50PCT 3

static uint64_t _read_throttling_counters(struct adreno_device *adreno_dev)
{
	int i;
	uint32_t th[ADRENO_GPMU_THROTTLE_COUNTERS];
	struct adreno_busy_data *busy = &adreno_dev->busy_data;

	if (!adreno_is_a540(adreno_dev))
		return 0;

	if (!ADRENO_FEATURE(adreno_dev, ADRENO_GPMU))
		return 0;

	if (!test_bit(ADRENO_THROTTLING_CTRL, &adreno_dev->pwrctrl_flag))
		return 0;

	for (i = 0; i < ADRENO_GPMU_THROTTLE_COUNTERS; i++) {
		if (!adreno_dev->gpmu_throttle_counters[i])
			return 0;

		th[i] = counter_delta(KGSL_DEVICE(adreno_dev),
			adreno_dev->gpmu_throttle_counters[i],
			&busy->throttle_cycles[i]);
	}
	i = th[CRC_MORE50PCT] - th[IDLE_10PCT];
	return th[CRC_50PCT] + th[CRC_LESS50PCT] / 3 + (i < 0 ? 0 : i) * 3;
}

static void _update_threshold_count(struct adreno_device *adreno_dev,
	uint64_t adj)
{
	if (adreno_is_a530(adreno_dev))
		kgsl_regread(KGSL_DEVICE(adreno_dev),
			adreno_dev->lm_threshold_count,
			&adreno_dev->lm_threshold_cross);
	else if (adreno_is_a540(adreno_dev))
		adreno_dev->lm_threshold_cross = adj;
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
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct adreno_gpudev *gpudev = ADRENO_GPU_DEVICE(adreno_dev);
	int status = -EINVAL, ret;
	unsigned int state = device->state;
	bool regulator_left_on;
	unsigned int pmqos_wakeup_vote = device->pwrctrl.pm_qos_wakeup_latency;
	unsigned int pmqos_active_vote = device->pwrctrl.pm_qos_active_latency;

	/* make sure ADRENO_DEVICE_STARTED is not set here */
	BUG_ON(test_bit(ADRENO_DEVICE_STARTED, &adreno_dev->priv));

	pm_qos_update_request(&device->pwrctrl.pm_qos_req_dma,
			pmqos_wakeup_vote);

	kgsl_cffdump_open(device);

	regulator_left_on = regulators_left_on(device);

	/* Clear any GPU faults that might have been left over */
	adreno_clear_gpu_fault(adreno_dev);

	/* Put the GPU in a responsive state */
	status = kgsl_pwrctrl_change_state(device, KGSL_STATE_AWARE);
	if (status)
		goto error_pwr_off;

	/* Set the bit to indicate that we've just powered on */
	set_bit(ADRENO_DEVICE_PWRON, &adreno_dev->priv);

	/* Soft reset the GPU if a regulator is stuck on*/
	if (regulator_left_on)
		_soft_reset(adreno_dev);

	status = kgsl_mmu_start(device);
	if (status)
		goto error_pwr_off;

	/* Program GPU contect protection init values */
	if (device->mmu.secured) {
		if (adreno_is_a4xx(adreno_dev))
			adreno_writereg(adreno_dev,
				ADRENO_REG_RBBM_SECVID_TRUST_CONFIG, 0x2);
		adreno_writereg(adreno_dev,
				ADRENO_REG_RBBM_SECVID_TSB_CONTROL, 0x0);

		adreno_writereg64(adreno_dev,
			ADRENO_REG_RBBM_SECVID_TSB_TRUSTED_BASE,
			ADRENO_REG_RBBM_SECVID_TSB_TRUSTED_BASE_HI,
			KGSL_IOMMU_SECURE_BASE);
		adreno_writereg(adreno_dev,
			ADRENO_REG_RBBM_SECVID_TSB_TRUSTED_SIZE,
			KGSL_IOMMU_SECURE_SIZE);
	}

	status = adreno_ocmem_malloc(adreno_dev);
	if (status) {
		KGSL_DRV_ERR(device, "OCMEM malloc failed\n");
		goto error_mmu_off;
	}

	/* Enable 64 bit gpu addr if feature is set */
	if (gpudev->enable_64bit &&
			adreno_support_64bit(adreno_dev))
		gpudev->enable_64bit(adreno_dev);

	if (adreno_dev->perfctr_pwr_lo == 0) {
		ret = adreno_perfcounter_get(adreno_dev,
			KGSL_PERFCOUNTER_GROUP_PWR, 1,
			&adreno_dev->perfctr_pwr_lo, NULL,
			PERFCOUNTER_FLAG_KERNEL);

		if (ret) {
			KGSL_DRV_ERR(device,
				"Unable to get the perf counters for DCVS\n");
			adreno_dev->perfctr_pwr_lo = 0;
		}
	}

	if (device->pwrctrl.bus_control) {

		/* VBIF waiting for RAM */
		if (adreno_dev->starved_ram_lo == 0) {
			ret = adreno_perfcounter_get(adreno_dev,
				KGSL_PERFCOUNTER_GROUP_VBIF_PWR, 0,
				&adreno_dev->starved_ram_lo, NULL,
				PERFCOUNTER_FLAG_KERNEL);

			if (ret) {
				KGSL_DRV_ERR(device,
					"Unable to get perf counters for bus DCVS\n");
				adreno_dev->starved_ram_lo = 0;
			}
		}

		/* VBIF DDR cycles */
		if (adreno_dev->ram_cycles_lo == 0) {
			ret = adreno_perfcounter_get(adreno_dev,
				KGSL_PERFCOUNTER_GROUP_VBIF,
				VBIF_AXI_TOTAL_BEATS,
				&adreno_dev->ram_cycles_lo, NULL,
				PERFCOUNTER_FLAG_KERNEL);

			if (ret) {
				KGSL_DRV_ERR(device,
					"Unable to get perf counters for bus DCVS\n");
				adreno_dev->ram_cycles_lo = 0;
			}
		}
	}

	/* Clear the busy_data stats - we're starting over from scratch */
	adreno_dev->busy_data.gpu_busy = 0;
	adreno_dev->busy_data.vbif_ram_cycles = 0;
	adreno_dev->busy_data.vbif_starved_ram = 0;

	if (adreno_is_a530(adreno_dev) && ADRENO_FEATURE(adreno_dev, ADRENO_LM)
		&& adreno_dev->lm_threshold_count == 0) {

		ret = adreno_perfcounter_get(adreno_dev,
			KGSL_PERFCOUNTER_GROUP_GPMU_PWR, 27,
			&adreno_dev->lm_threshold_count, NULL,
			PERFCOUNTER_FLAG_KERNEL);
		/* Ignore noncritical ret - used for debugfs */
		if (ret)
			adreno_dev->lm_threshold_count = 0;
	}

	_setup_throttling_counters(adreno_dev);

	/* Restore performance counter registers with saved values */
	adreno_perfcounter_restore(adreno_dev);

	/* Start the GPU */
	gpudev->start(adreno_dev);

	/* Re-initialize the coresight registers if applicable */
	adreno_coresight_start(adreno_dev);

	adreno_irqctrl(adreno_dev, 1);

	adreno_perfcounter_start(adreno_dev);

	/* Clear FSR here in case it is set from a previous pagefault */
	kgsl_mmu_clear_fsr(&device->mmu);

	status = adreno_ringbuffer_start(adreno_dev, ADRENO_START_COLD);
	if (status)
		goto error_mmu_off;

	/* Start the dispatcher */
	adreno_dispatcher_start(device);

	device->reset_counter++;

	set_bit(ADRENO_DEVICE_STARTED, &adreno_dev->priv);

	if (pmqos_active_vote != pmqos_wakeup_vote)
		pm_qos_update_request(&device->pwrctrl.pm_qos_req_dma,
				pmqos_active_vote);

	return 0;

error_mmu_off:
	kgsl_mmu_stop(&device->mmu);

error_pwr_off:
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

	if (priority && (adreno_wake_nice < nice))
		set_user_nice(current, adreno_wake_nice);

	ret = _adreno_start(adreno_dev);

	if (priority)
		set_user_nice(current, nice);

	return ret;
}

/**
 * adreno_vbif_clear_pending_transactions() - Clear transactions in VBIF pipe
 * @device: Pointer to the device whose VBIF pipe is to be cleared
 */
static int adreno_vbif_clear_pending_transactions(struct kgsl_device *device)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct adreno_gpudev *gpudev = ADRENO_GPU_DEVICE(adreno_dev);
	unsigned int mask = gpudev->vbif_xin_halt_ctrl0_mask;
	unsigned int val;
	unsigned long wait_for_vbif;
	int ret = 0;

	adreno_writereg(adreno_dev, ADRENO_REG_VBIF_XIN_HALT_CTRL0, mask);
	/* wait for the transactions to clear */
	wait_for_vbif = jiffies + msecs_to_jiffies(100);
	while (1) {
		adreno_readreg(adreno_dev,
			ADRENO_REG_VBIF_XIN_HALT_CTRL1, &val);
		if ((val & mask) == mask)
			break;
		if (time_after(jiffies, wait_for_vbif)) {
			KGSL_DRV_ERR(device,
				"Wait limit reached for VBIF XIN Halt\n");
			ret = -ETIMEDOUT;
			break;
		}
	}
	adreno_writereg(adreno_dev, ADRENO_REG_VBIF_XIN_HALT_CTRL0, 0);
	return ret;
}

static int adreno_stop(struct kgsl_device *device)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);

	if (!test_bit(ADRENO_DEVICE_STARTED, &adreno_dev->priv))
		return 0;

	adreno_set_active_ctxs_null(adreno_dev);

	adreno_dispatcher_stop(adreno_dev);

	adreno_ringbuffer_stop(adreno_dev);

	adreno_irqctrl(adreno_dev, 0);

	adreno_ocmem_free(adreno_dev);

	/* Save active coresight registers if applicable */
	adreno_coresight_stop(adreno_dev);

	/* Save physical performance counter values before GPU power down*/
	adreno_perfcounter_save(adreno_dev);

	adreno_vbif_clear_pending_transactions(device);

	kgsl_mmu_stop(&device->mmu);
	kgsl_cffdump_close(device);

	clear_bit(ADRENO_DEVICE_STARTED, &adreno_dev->priv);

	return 0;
}

static inline bool adreno_try_soft_reset(struct kgsl_device *device, int fault)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);

	/*
	 * Do not do soft reset for a IOMMU fault (because the IOMMU hardware
	 * needs a reset too) or for the A304 because it can't do SMMU
	 * programming of any kind after a soft reset
	 */

	if ((fault & ADRENO_IOMMU_PAGE_FAULT) || adreno_is_a304(adreno_dev))
		return false;

	return true;
}

/**
 * adreno_reset() - Helper function to reset the GPU
 * @device: Pointer to the KGSL device structure for the GPU
 * @fault: Type of fault. Needed to skip soft reset for MMU fault
 *
 * Try to reset the GPU to recover from a fault.  First, try to do a low latency
 * soft reset.  If the soft reset fails for some reason, then bring out the big
 * guns and toggle the footswitch.
 */
int adreno_reset(struct kgsl_device *device, int fault)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct adreno_gpudev *gpudev = ADRENO_GPU_DEVICE(adreno_dev);
	int ret = -EINVAL;
	int i = 0;

	/* broadcast to HW - reset is coming */
	if (gpudev->pre_reset)
		gpudev->pre_reset(adreno_dev);

	/* Try soft reset first */
	if (adreno_try_soft_reset(device, fault)) {
		/* Make sure VBIF is cleared before resetting */
		ret = adreno_vbif_clear_pending_transactions(device);

		if (ret == 0) {
			ret = adreno_soft_reset(device);
			if (ret)
				KGSL_DEV_ERR_ONCE(device,
					"Device soft reset failed\n");
		}
	}
	if (ret) {
		/* If soft reset failed/skipped, then pull the power */
		kgsl_pwrctrl_change_state(device, KGSL_STATE_INIT);
		/* since device is officially off now clear start bit */
		clear_bit(ADRENO_DEVICE_STARTED, &adreno_dev->priv);

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
	else
		kgsl_pwrctrl_change_state(device, KGSL_STATE_NAP);

	/* Set the page table back to the default page table */
	kgsl_mmu_set_pt(&device->mmu, device->mmu.defaultpagetable);
	kgsl_sharedmem_writel(device,
		&adreno_dev->ringbuffers[0].pagetable_desc,
		offsetof(struct adreno_ringbuffer_pagetable_info,
			current_global_ptname), 0);

	return ret;
}

static int adreno_getproperty(struct kgsl_device *device,
				unsigned int type,
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
			devinfo.mmu_enabled =
				MMU_FEATURE(&device->mmu, KGSL_MMU_PAGED);
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
				shadowprop.gpuaddr =
					(unsigned int) device->memstore.gpuaddr;
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
			/* Report MMU only if we can handle paged memory */
			int mmu_prop = MMU_FEATURE(&device->mmu,
				KGSL_MMU_PAGED);

			if (sizebytes < sizeof(mmu_prop)) {
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
	case KGSL_PROP_UCHE_GMEM_VADDR:
		{
			uint64_t gmem_vaddr = 0;
			if (adreno_is_a5xx(adreno_dev))
				gmem_vaddr = ADRENO_UCHE_GMEM_BASE;
			if (sizebytes != sizeof(uint64_t)) {
				status = -EINVAL;
				break;
			}
			if (copy_to_user(value, &gmem_vaddr,
					sizeof(uint64_t))) {
				status = -EFAULT;
				break;
			}
			status = 0;
		}
		break;
	case KGSL_PROP_SP_GENERIC_MEM:
		{
			struct kgsl_sp_generic_mem sp_mem;
			if (sizebytes != sizeof(sp_mem)) {
				status = -EINVAL;
				break;
			}
			memset(&sp_mem, 0, sizeof(sp_mem));

			sp_mem.local = adreno_dev->sp_local_gpuaddr;
			sp_mem.pvt = adreno_dev->sp_pvt_gpuaddr;

			if (copy_to_user(value, &sp_mem, sizeof(sp_mem))) {
				status = -EFAULT;
				break;
			}
			status = 0;
		}
		break;
	case KGSL_PROP_UCODE_VERSION:
		{
			struct kgsl_ucode_version ucode;

			if (sizebytes != sizeof(ucode)) {
				status = -EINVAL;
				break;
			}
			memset(&ucode, 0, sizeof(ucode));

			ucode.pfp = adreno_dev->pfp_fw_version;
			ucode.pm4 = adreno_dev->pm4_fw_version;

			if (copy_to_user(value, &ucode, sizeof(ucode))) {
				status = -EFAULT;
				break;
			}
			status = 0;
		}
		break;
	case KGSL_PROP_GPMU_VERSION:
		{
			struct kgsl_gpmu_version gpmu;

			if (adreno_dev->gpucore == NULL) {
				status = -EINVAL;
				break;
			}

			if (!ADRENO_FEATURE(adreno_dev, ADRENO_GPMU)) {
				status = -EOPNOTSUPP;
				break;
			}

			if (sizebytes != sizeof(gpmu)) {
				status = -EINVAL;
				break;
			}
			memset(&gpmu, 0, sizeof(gpmu));

			gpmu.major = adreno_dev->gpucore->gpmu_major;
			gpmu.minor = adreno_dev->gpucore->gpmu_minor;
			gpmu.features = adreno_dev->gpucore->gpmu_features;

			if (copy_to_user(value, &gpmu, sizeof(gpmu))) {
				status = -EFAULT;
				break;
			}
			status = 0;
		}
		break;
	case KGSL_PROP_HIGHEST_BANK_BIT:
		{
			unsigned int bit;

			if (sizebytes < sizeof(unsigned int)) {
				status = -EINVAL;
				break;
			}

			if (of_property_read_u32(device->pdev->dev.of_node,
				"qcom,highest-bank-bit", &bit)) {
					status = -EINVAL;
					break;
			}

			if (copy_to_user(value, &bit, sizeof(bit))) {
				status = -EFAULT;
				break;
			}
		}
		status = 0;
		break;
	case KGSL_PROP_DEVICE_BITNESS:
	{
		unsigned int bitness = 32;

		if (sizebytes != sizeof(unsigned int)) {
			status = -EINVAL;
			break;
		}
		/* No of bits used by the GPU */
		if (adreno_support_64bit(adreno_dev))
			bitness = 48;

		if (copy_to_user(value, &bitness,
				sizeof(unsigned int))) {
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
				unsigned int type,
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

				if (!kgsl_active_count_get(device)) {
					adreno_fault_detect_start(adreno_dev);
					kgsl_active_count_put(device);
				}

				kgsl_pwrscale_enable(device);
			} else {
				kgsl_pwrctrl_change_state(device,
							KGSL_STATE_ACTIVE);
				device->pwrctrl.ctrl_flags = KGSL_PWR_ON;
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
	const struct adreno_gpu_core *gpucore = adreno_dev->gpucore;
	unsigned int reg_rbbm_status;

	adreno_readreg(adreno_dev, ADRENO_REG_RBBM_STATUS,
		&reg_rbbm_status);

	if (reg_rbbm_status & gpucore->busy_mask)
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

	kgsl_pwrctrl_change_state(device, KGSL_STATE_AWARE);
	adreno_set_active_ctxs_null(adreno_dev);

	adreno_irqctrl(adreno_dev, 0);

	adreno_clear_gpu_fault(adreno_dev);
	/* since device is oficially off now clear start bit */
	clear_bit(ADRENO_DEVICE_STARTED, &adreno_dev->priv);

	/* save physical performance counter values before GPU soft reset */
	adreno_perfcounter_save(adreno_dev);

	kgsl_cffdump_close(device);
	/* Reset the GPU */
	_soft_reset(adreno_dev);

	/* start of new CFF after reset */
	kgsl_cffdump_open(device);

	/* Enable 64 bit gpu addr if feature is set */
	if (gpudev->enable_64bit &&
			adreno_support_64bit(adreno_dev))
		gpudev->enable_64bit(adreno_dev);

	/* Restore physical performance counter values after soft reset */
	adreno_perfcounter_restore(adreno_dev);

	/* Reinitialize the GPU */
	gpudev->start(adreno_dev);

	/* Re-initialize the coresight registers if applicable */
	adreno_coresight_start(adreno_dev);

	/* Enable IRQ */
	adreno_irqctrl(adreno_dev, 1);

	/* stop all ringbuffers to cancel RB events */
	adreno_ringbuffer_stop(adreno_dev);
	/*
	 * If we have offsets for the jump tables we can try to do a warm start,
	 * otherwise do a full ringbuffer restart
	 */

	if (ADRENO_FEATURE(adreno_dev, ADRENO_WARM_START))
		ret = adreno_ringbuffer_start(adreno_dev, ADRENO_START_WARM);
	else
		ret = adreno_ringbuffer_start(adreno_dev, ADRENO_START_COLD);
	if (ret == 0) {
		device->reset_counter++;
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

	if (!kgsl_state_is_awake(device))
		return true;

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
		if (!adreno_rb_empty(rb))
			return false;
	}

	return adreno_hw_isidle(adreno_dev);
}

/**
 * adreno_spin_idle() - Spin wait for the GPU to idle
 * @adreno_dev: Pointer to an adreno device
 * @timeout: milliseconds to wait before returning error
 *
 * Spin the CPU waiting for the RBBM status to return idle
 */
int adreno_spin_idle(struct adreno_device *adreno_dev, unsigned int timeout)
{
	unsigned long wait = jiffies + msecs_to_jiffies(timeout);

	kgsl_cffdump_regpoll(KGSL_DEVICE(adreno_dev),
		adreno_getreg(adreno_dev, ADRENO_REG_RBBM_STATUS) << 2,
		0x00000000, 0x80000000);

	do {
		/*
		 * If we fault, stop waiting and return an error. The dispatcher
		 * will clean up the fault from the work queue, but we need to
		 * make sure we don't block it by waiting for an idle that
		 * will never come.
		 */

		if (adreno_gpu_fault(adreno_dev) != 0)
			return -EDEADLK;

		if (adreno_isidle(KGSL_DEVICE(adreno_dev)))
			return 0;

	} while (time_before(jiffies, wait));

	/*
	 * Under rare conditions, preemption can cause the while loop to exit
	 * without checking if the gpu is idle. check one last time before we
	 * return failure.
	 */
	if (adreno_gpu_fault(adreno_dev) != 0)
		return -EDEADLK;

	if (adreno_isidle(KGSL_DEVICE(adreno_dev)))
		return 0;

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

	return adreno_spin_idle(adreno_dev, ADRENO_IDLE_TIMEOUT);
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
	reinit_completion(&device->cmdbatch_gate);

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
	adreno_set_active_ctxs_null(adreno_dev);

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
static void adreno_read(struct kgsl_device *device, void __iomem *base,
		unsigned int offsetwords, unsigned int *value,
		unsigned int mem_len)
{

	unsigned int __iomem *reg;
	BUG_ON(offsetwords*sizeof(uint32_t) >= mem_len);
	reg = (unsigned int __iomem *)(base + (offsetwords << 2));

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
	unsigned int __iomem *reg;

	BUG_ON(offsetwords*sizeof(uint32_t) >= device->reg_len);

	if (!in_interrupt())
		kgsl_pre_hwaccess(device);

	trace_kgsl_regwrite(device, offsetwords, value);

	kgsl_cffdump_regwrite(device, offsetwords << 2, value);
	reg = (unsigned int __iomem *)(device->reg_virt + (offsetwords << 2));

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

	/* Return -ENOENT if the context has been detached */
	if (kgsl_context_detached(context))
		return -ENOENT;

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
 * @adreno_dev: Pointer to an adreno device
 * @index: Index into the memstore memory
 * @type: Type of timestamp to read
 * @timestamp: The out parameter where the timestamp is read
 */
static int __adreno_readtimestamp(struct adreno_device *adreno_dev, int index,
				int type, unsigned int *timestamp)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
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
 * @adreno_dev: adreno device whose timestamp values are being queried
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
int adreno_rb_readtimestamp(struct adreno_device *adreno_dev,
		void *priv, enum kgsl_timestamp_type type,
		unsigned int *timestamp)
{
	int status = 0;
	struct adreno_ringbuffer *rb = priv;

	if (KGSL_TIMESTAMP_QUEUED == type)
		*timestamp = rb->timestamp;
	else
		status = __adreno_readtimestamp(adreno_dev,
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

	if (type == KGSL_TIMESTAMP_QUEUED) {
		struct adreno_context *ctxt = ADRENO_CONTEXT(context);

		*timestamp = ctxt->timestamp;
	} else
		status = __adreno_readtimestamp(ADRENO_DEVICE(device),
				context->id, type, timestamp);

	return status;
}

static inline s64 adreno_ticks_to_us(u32 ticks, u32 freq)
{
	freq /= 1000000;
	return ticks / freq;
}

static unsigned int counter_delta(struct kgsl_device *device,
			unsigned int reg, unsigned int *counter)
{
	unsigned int val;
	unsigned int ret = 0;

	/* Read the value */
	kgsl_regread(device, reg, &val);

	/* Return 0 for the first read */
	if (*counter != 0) {
		if (val < *counter)
			ret = (0xFFFFFFFF - *counter) + val;
		else
			ret = val - *counter;
	}

	*counter = val;
	return ret;
}

/**
 * adreno_power_stats() - Reads the counters needed for freq decisions
 * @device: Pointer to device whose counters are read
 * @stats: Pointer to stats set that needs updating
 * Power: The caller is expected to be in a clock enabled state as this
 * function does reg reads
 */
static void adreno_power_stats(struct kgsl_device *device,
				struct kgsl_power_stats *stats)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;
	struct adreno_busy_data *busy = &adreno_dev->busy_data;
	uint64_t adj = 0;

	memset(stats, 0, sizeof(*stats));

	/* Get the busy cycles counted since the counter was last reset */
	if (adreno_dev->perfctr_pwr_lo != 0) {
		uint64_t gpu_busy;

		gpu_busy = counter_delta(device, adreno_dev->perfctr_pwr_lo,
			&busy->gpu_busy);

		adj = _read_throttling_counters(adreno_dev);
		gpu_busy += adj;
		stats->busy_time = adreno_ticks_to_us(gpu_busy,
			kgsl_pwrctrl_active_freq(pwr));
	}

	if (device->pwrctrl.bus_control) {
		uint64_t ram_cycles = 0, starved_ram = 0;

		if (adreno_dev->ram_cycles_lo != 0)
			ram_cycles = counter_delta(device,
				adreno_dev->ram_cycles_lo,
				&busy->vbif_ram_cycles);

		if (adreno_dev->starved_ram_lo != 0)
			starved_ram = counter_delta(device,
				adreno_dev->starved_ram_lo,
				&busy->vbif_starved_ram);

		stats->ram_time = ram_cycles;
		stats->ram_wait = starved_ram;
	}
	if (adreno_dev->lm_threshold_count)
		_update_threshold_count(adreno_dev, adj);
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

static int adreno_regulator_enable(struct kgsl_device *device)
{
	int ret = 0;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct adreno_gpudev *gpudev  = ADRENO_GPU_DEVICE(adreno_dev);
	if (gpudev->regulator_enable &&
		!test_bit(ADRENO_DEVICE_GPU_REGULATOR_ENABLED,
			&adreno_dev->priv)) {
		ret = gpudev->regulator_enable(adreno_dev);
		if (!ret)
			set_bit(ADRENO_DEVICE_GPU_REGULATOR_ENABLED,
				&adreno_dev->priv);
	}
	return ret;
}

static bool adreno_is_hw_collapsible(struct kgsl_device *device)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct adreno_gpudev *gpudev  = ADRENO_GPU_DEVICE(adreno_dev);

	/*
	 * Skip power collapse for A304, if power ctrl flag is set to
	 * non zero. As A304 soft_reset will not work, power collapse
	 * needs to disable to avoid soft_reset.
	 */
	if (adreno_is_a304(adreno_dev) &&
			device->pwrctrl.ctrl_flags)
		return false;

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

static void adreno_pwrlevel_change_settings(struct kgsl_device *device,
		unsigned int prelevel, unsigned int postlevel, bool post)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct adreno_gpudev *gpudev  = ADRENO_GPU_DEVICE(adreno_dev);

	if (gpudev->pwrlevel_change_settings)
		gpudev->pwrlevel_change_settings(adreno_dev, prelevel,
					postlevel, post);
}

static void adreno_iommu_sync(struct kgsl_device *device, bool sync)
{
	struct scm_desc desc = {0};
	int ret;

	if (sync == true) {
		mutex_lock(&kgsl_mmu_sync);
		desc.args[0] = true;
		desc.arginfo = SCM_ARGS(1);
		ret = scm_call2_atomic(SCM_SIP_FNID(SCM_SVC_PWR, 0x8), &desc);
		if (ret)
			KGSL_DRV_ERR(device,
				"MMU sync with Hypervisor off %x\n", ret);
	} else {
		desc.args[0] = false;
		desc.arginfo = SCM_ARGS(1);
		scm_call2_atomic(SCM_SIP_FNID(SCM_SVC_PWR, 0x8), &desc);
		mutex_unlock(&kgsl_mmu_sync);
	}
}

static void _regulator_disable(struct kgsl_regulator *regulator, bool poll)
{
	unsigned long wait_time = jiffies + msecs_to_jiffies(200);

	if (IS_ERR_OR_NULL(regulator->reg))
		return;

	regulator_disable(regulator->reg);

	if (poll == false)
		return;

	while (!time_after(jiffies, wait_time)) {
		if (!regulator_is_enabled(regulator->reg))
			return;
		cpu_relax();
	}

	KGSL_CORE_ERR("regulator '%s' still on after 200ms\n", regulator->name);
}

static void adreno_regulator_disable_poll(struct kgsl_device *device)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;
	int i;

	/* Fast path - hopefully we don't need this quirk */
	if (!ADRENO_QUIRK(adreno_dev, ADRENO_QUIRK_IOMMU_SYNC)) {
		for (i = KGSL_MAX_REGULATORS - 1; i >= 0; i--)
			_regulator_disable(&pwr->regulators[i], false);
		return;
	}

	adreno_iommu_sync(device, true);

	for (i = 0; i < KGSL_MAX_REGULATORS; i++)
		_regulator_disable(&pwr->regulators[i], true);

	adreno_iommu_sync(device, false);
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
	.pwrlevel_change_settings = adreno_pwrlevel_change_settings,
	.regulator_disable_poll = adreno_regulator_disable_poll,
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

static struct of_device_id busmon_match_table[] = {
	{ .compatible = "qcom,kgsl-busmon", .data = &device_3d0 },
	{}
};

static int adreno_busmon_probe(struct platform_device *pdev)
{
	struct kgsl_device *device;
	const struct of_device_id *pdid =
			of_match_device(busmon_match_table, &pdev->dev);

	if (pdid == NULL)
		return -ENXIO;

	device = (struct kgsl_device *)pdid->data;
	device->busmondev = &pdev->dev;
	dev_set_drvdata(device->busmondev, device);

	return 0;
}

static struct platform_driver kgsl_bus_platform_driver = {
	.probe = adreno_busmon_probe,
	.driver = {
		.owner = THIS_MODULE,
		.name = "kgsl-busmon",
		.of_match_table = busmon_match_table,
	}
};

static int __init kgsl_3d_init(void)
{
	int ret;

	ret = platform_driver_register(&kgsl_bus_platform_driver);
	if (ret)
		return ret;

	ret = platform_driver_register(&adreno_platform_driver);
	if (ret)
		platform_driver_unregister(&kgsl_bus_platform_driver);

	return ret;
}

static void __exit kgsl_3d_exit(void)
{
	platform_driver_unregister(&adreno_platform_driver);
	platform_driver_unregister(&kgsl_bus_platform_driver);
}

module_init(kgsl_3d_init);
module_exit(kgsl_3d_exit);

MODULE_DESCRIPTION("3D Graphics driver");
MODULE_VERSION("1.2");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:kgsl_3d");
