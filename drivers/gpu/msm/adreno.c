/* Copyright (c) 2002,2007-2019, The Linux Foundation. All rights reserved.
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
#include <linux/io.h>
#include <soc/qcom/scm.h>

#include <linux/msm-bus-board.h>
#include <linux/msm-bus.h>

#include "kgsl.h"
#include "kgsl_gmu_core.h"
#include "kgsl_pwrscale.h"
#include "kgsl_sharedmem.h"
#include "kgsl_iommu.h"
#include "kgsl_trace.h"
#include "adreno_llc.h"

#include "adreno.h"
#include "adreno_iommu.h"
#include "adreno_compat.h"
#include "adreno_pm4types.h"
#include "adreno_trace.h"

#include "a3xx_reg.h"
#include "a6xx_reg.h"
#include "adreno_snapshot.h"

/* Include the master list of GPU cores that are supported */
#include "adreno-gpulist.h"
#include "adreno_dispatch.h"

#undef MODULE_PARAM_PREFIX
#define MODULE_PARAM_PREFIX "adreno."

static bool nopreempt;
module_param(nopreempt, bool, 0444);
MODULE_PARM_DESC(nopreempt, "Disable GPU preemption");

static bool swfdetect;
module_param(swfdetect, bool, 0444);
MODULE_PARM_DESC(swfdetect, "Enable soft fault detection");

#define DRIVER_VERSION_MAJOR   3
#define DRIVER_VERSION_MINOR   1

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
	.fw[0] = {
		.fwvirt = NULL
	},
	.fw[1] = {
		.fwvirt = NULL
	},
	.gmem_size = SZ_256K,
	.ft_policy = KGSL_FT_DEFAULT_POLICY,
	.ft_pf_policy = KGSL_FT_PAGEFAULT_DEFAULT_POLICY,
	.long_ib_detect = 1,
	.input_work = __WORK_INITIALIZER(device_3d0.input_work,
		adreno_input_work),
	.pwrctrl_flag = BIT(ADRENO_SPTP_PC_CTRL) | BIT(ADRENO_PPD_CTRL) |
		BIT(ADRENO_LM_CTRL) | BIT(ADRENO_HWCG_CTRL) |
		BIT(ADRENO_THROTTLING_CTRL),
	.profile.enabled = false,
	.active_list = LIST_HEAD_INIT(device_3d0.active_list),
	.active_list_lock = __SPIN_LOCK_UNLOCKED(device_3d0.active_list_lock),
	.gpu_llc_slice_enable = true,
	.gpuhtw_llc_slice_enable = true,
	.preempt = {
		.preempt_level = 1,
		.skipsaverestore = 1,
		.usesgmem = 1,
	},
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
static int _soft_reset(struct adreno_device *adreno_dev)
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

	return 0;
}

/**
 * adreno_irqctrl() - Enables/disables the RBBM interrupt mask
 * @adreno_dev: Pointer to an adreno_device
 * @state: 1 for masked or 0 for unmasked
 * Power: The caller of this function must make sure to use OOBs
 * so that we know that the GPU is powered on
 */
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

	adreno_dispatcher_schedule(device);
}

static irqreturn_t adreno_irq_handler(struct kgsl_device *device)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct adreno_gpudev *gpudev = ADRENO_GPU_DEVICE(adreno_dev);
	struct adreno_irq *irq_params = gpudev->irq;
	irqreturn_t ret = IRQ_NONE;
	unsigned int status = 0, fence = 0, fence_retries = 0, tmp, int_bit;
	unsigned int shadow_status = 0;
	int i;

	atomic_inc(&adreno_dev->pending_irq_refcnt);
	/* Ensure this increment is done before the IRQ status is updated */
	smp_mb__after_atomic();

	/*
	 * On A6xx, the GPU can power down once the INT_0_STATUS is read
	 * below. But there still might be some register reads required
	 * so force the GMU/GPU into KEEPALIVE mode until done with the ISR.
	 */
	if (gpudev->gpu_keepalive)
		gpudev->gpu_keepalive(adreno_dev, true);

	/*
	 * If the AHB fence is not in ALLOW mode when we receive an RBBM
	 * interrupt, something went wrong. This means that we cannot proceed
	 * since the IRQ status and clear registers are not accessible.
	 * This is usually harmless because the GMU will abort power collapse
	 * and change the fence back to ALLOW. Poll so that this can happen.
	 */
	if (gmu_core_isenabled(device)) {
		adreno_readreg(adreno_dev,
				ADRENO_REG_GMU_AO_AHB_FENCE_CTRL,
				&fence);

		while (fence != 0) {
			/* Wait for small time before trying again */
			udelay(1);
			adreno_readreg(adreno_dev,
					ADRENO_REG_GMU_AO_AHB_FENCE_CTRL,
					&fence);

			if (fence_retries == FENCE_RETRY_MAX && fence != 0) {
				adreno_readreg(adreno_dev,
					ADRENO_REG_GMU_RBBM_INT_UNMASKED_STATUS,
					&shadow_status);

				KGSL_DRV_CRIT_RATELIMIT(device,
					"Status=0x%x Unmasked status=0x%x Mask=0x%x\n",
					shadow_status & irq_params->mask,
					shadow_status, irq_params->mask);
				adreno_set_gpu_fault(adreno_dev,
						ADRENO_GMU_FAULT);
				adreno_dispatcher_schedule(KGSL_DEVICE
						(adreno_dev));
				goto done;
			}
			fence_retries++;
		}
	}

	adreno_readreg(adreno_dev, ADRENO_REG_RBBM_INT_0_STATUS, &status);

	/*
	 * Clear all the interrupt bits but ADRENO_INT_RBBM_AHB_ERROR. Because
	 * even if we clear it here, it will stay high until it is cleared
	 * in its respective handler. Otherwise, the interrupt handler will
	 * fire again.
	 */
	int_bit = ADRENO_INT_BIT(adreno_dev, ADRENO_INT_RBBM_AHB_ERROR);
	adreno_writereg(adreno_dev, ADRENO_REG_RBBM_INT_CLEAR_CMD,
				status & ~int_bit);

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

	/*
	 * Clear ADRENO_INT_RBBM_AHB_ERROR bit after this interrupt has been
	 * cleared in its respective handler
	 */
	if (status & int_bit)
		adreno_writereg(adreno_dev, ADRENO_REG_RBBM_INT_CLEAR_CMD,
				int_bit);

done:
	/* Turn off the KEEPALIVE vote from earlier unless hard fault set */
	if (gpudev->gpu_keepalive) {
		/* If hard fault, then let snapshot turn off the keepalive */
		if (!(adreno_gpu_fault(adreno_dev) & ADRENO_HARD_FAULT))
			gpudev->gpu_keepalive(adreno_dev, false);
	}

	/* Make sure the regwrites are done before the decrement */
	smp_mb__before_atomic();
	atomic_dec(&adreno_dev->pending_irq_refcnt);
	/* Ensure other CPUs see the decrement */
	smp_mb__after_atomic();

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

static struct {
	unsigned int quirk;
	const char *prop;
} adreno_quirks[] = {
	 { ADRENO_QUIRK_TWO_PASS_USE_WFI, "qcom,gpu-quirk-two-pass-use-wfi" },
	 { ADRENO_QUIRK_IOMMU_SYNC, "qcom,gpu-quirk-iommu-sync" },
	 { ADRENO_QUIRK_CRITICAL_PACKETS, "qcom,gpu-quirk-critical-packets" },
	 { ADRENO_QUIRK_FAULT_DETECT_MASK, "qcom,gpu-quirk-fault-detect-mask" },
	 { ADRENO_QUIRK_DISABLE_RB_DP2CLOCKGATING,
			"qcom,gpu-quirk-dp2clockgating-disable" },
	 { ADRENO_QUIRK_DISABLE_LMLOADKILL,
			"qcom,gpu-quirk-lmloadkill-disable" },
	{ ADRENO_QUIRK_HFI_USE_REG, "qcom,gpu-quirk-hfi-use-reg" },
	{ ADRENO_QUIRK_SECVID_SET_ONCE, "qcom,gpu-quirk-secvid-set-once" },
	{ ADRENO_QUIRK_LIMIT_UCHE_GBIF_RW,
			"qcom,gpu-quirk-limit-uche-gbif-rw" },
	{ ADRENO_QUIRK_MMU_SECURE_CB_ALT, "qcom,gpu-quirk-mmu-secure-cb-alt" },
	{ ADRENO_QUIRK_CX_GDSC, "qcom,gpu-quirk-cx-gdsc" },
};

static struct device_node *
adreno_get_soc_hw_revision_node(struct adreno_device *adreno_dev,
	struct platform_device *pdev)
{
	struct device_node *node, *child;
	unsigned int rev;

	node = of_find_node_by_name(pdev->dev.of_node, "qcom,soc-hw-revisions");
	if (node == NULL)
		return NULL;

	for_each_child_of_node(node, child) {
		if (of_property_read_u32(child, "qcom,soc-hw-revision", &rev))
			continue;

		if (rev == adreno_dev->soc_hw_rev)
			return child;
	}

	KGSL_DRV_WARN(KGSL_DEVICE(adreno_dev),
		"No matching SOC HW revision found for efused HW rev=%u\n",
		adreno_dev->soc_hw_rev);
	return NULL;
}

static int adreno_update_soc_hw_revision_quirks(
		struct adreno_device *adreno_dev, struct platform_device *pdev)
{
	struct device_node *node;
	int i;

	node = adreno_get_soc_hw_revision_node(adreno_dev, pdev);
	if (node == NULL)
		node = pdev->dev.of_node;

	/* get chip id, fall back to parent if revision node does not have it */
	if (of_property_read_u32(node, "qcom,chipid", &adreno_dev->chipid))
		if (of_property_read_u32(pdev->dev.of_node,
				"qcom,chipid", &adreno_dev->chipid))
			KGSL_DRV_FATAL(KGSL_DEVICE(adreno_dev),
			"No GPU chip ID was specified\n");

	/* update quirk */
	for (i = 0; i < ARRAY_SIZE(adreno_quirks); i++) {
		if (of_property_read_bool(node, adreno_quirks[i].prop))
			adreno_dev->quirks |= adreno_quirks[i].quirk;
	}

	return 0;
}

static void
adreno_identify_gpu(struct adreno_device *adreno_dev)
{
	const struct adreno_reg_offsets *reg_offsets;
	struct adreno_gpudev *gpudev;
	int i;

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

static void adreno_of_get_ca_target_pwrlevel(struct adreno_device *adreno_dev,
		struct device_node *node)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	unsigned int ca_target_pwrlevel = 1;

	of_property_read_u32(node, "qcom,ca-target-pwrlevel",
		&ca_target_pwrlevel);

	if (ca_target_pwrlevel > device->pwrctrl.num_pwrlevels - 2)
		ca_target_pwrlevel = 1;

	device->pwrscale.ctxt_aware_target_pwrlevel = ca_target_pwrlevel;
}

static void adreno_of_get_ca_aware_properties(struct adreno_device *adreno_dev,
		struct device_node *parent)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct kgsl_pwrscale *pwrscale = &device->pwrscale;
	struct device_node *node, *child;
	unsigned int bin = 0;

	pwrscale->ctxt_aware_enable =
		of_property_read_bool(parent, "qcom,enable-ca-jump");

	if (pwrscale->ctxt_aware_enable) {
		if (of_property_read_u32(parent, "qcom,ca-busy-penalty",
			&pwrscale->ctxt_aware_busy_penalty))
			pwrscale->ctxt_aware_busy_penalty = 12000;

		node = of_find_node_by_name(parent, "qcom,gpu-pwrlevel-bins");
		if (node == NULL) {
			adreno_of_get_ca_target_pwrlevel(adreno_dev, parent);
			return;
		}

		for_each_child_of_node(node, child) {
			if (of_property_read_u32(child, "qcom,speed-bin", &bin))
				continue;

			if (bin == adreno_dev->speed_bin) {
				adreno_of_get_ca_target_pwrlevel(adreno_dev,
					child);
				return;
			}
		}

		pwrscale->ctxt_aware_target_pwrlevel = 1;
	}
}

static int adreno_of_parse_pwrlevels(struct adreno_device *adreno_dev,
		struct device_node *node)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;
	struct device_node *child;
	int ret;

	/* ADD the GPU OPP table if we define it */
	if (of_find_property(device->pdev->dev.of_node,
			"operating-points-v2", NULL)) {
		ret = dev_pm_opp_of_add_table(&device->pdev->dev);
		if (ret) {
			KGSL_CORE_ERR("Unable to set the GPU OPP table: %d\n",
					ret);
			return ret;
		}
	}

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
	unsigned int bin = 0;

	node = of_find_node_by_name(parent, "qcom,gpu-pwrlevel-bins");
	if (node == NULL)
		return adreno_of_get_legacy_pwrlevels(adreno_dev, parent);

	for_each_child_of_node(node, child) {

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

	KGSL_CORE_ERR("GPU speed_bin:%d mismatch for efused bin:%d\n",
			adreno_dev->speed_bin, bin);
	return -ENODEV;
}

static void
l3_pwrlevel_probe(struct kgsl_device *device, struct device_node *node)
{
	struct device_node *pwrlevel_node, *child;

	pwrlevel_node = of_find_node_by_name(node, "qcom,l3-pwrlevels");

	if (pwrlevel_node == NULL)
		return;

	for_each_available_child_of_node(pwrlevel_node, child) {
		unsigned int index;

		if (of_property_read_u32(child, "reg", &index))
			return;
		if (index >= MAX_L3_LEVELS)
			continue;

		if (index >= device->num_l3_pwrlevels)
			device->num_l3_pwrlevels = index + 1;

		if (of_property_read_u32(child, "qcom,l3-freq",
				&device->l3_freq[index]))
			continue;
	}

	device->l3_clk = devm_clk_get(&device->pdev->dev, "l3_vote");

	if (IS_ERR_OR_NULL(device->l3_clk)) {
		dev_err(&device->pdev->dev,
			"Unable to get the l3_vote clock\n");
		device->l3_clk = NULL;
	}
}

static inline struct adreno_device *adreno_get_dev(struct platform_device *pdev)
{
	const struct of_device_id *of_id =
		of_match_device(adreno_match_table, &pdev->dev);

	return of_id ? (struct adreno_device *) of_id->data : NULL;
}

static int adreno_of_get_power(struct adreno_device *adreno_dev,
		struct platform_device *pdev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct device_node *node = pdev->dev.of_node;
	struct resource *res;
	unsigned int timeout;
	unsigned int throt = 4;

	if (of_property_read_string(node, "label", &pdev->name)) {
		KGSL_CORE_ERR("Unable to read 'label'\n");
		return -EINVAL;
	}

	if (adreno_of_read_property(node, "qcom,id", &pdev->id))
		return -EINVAL;

	/* Get starting physical address of device registers */
	res = platform_get_resource_byname(device->pdev, IORESOURCE_MEM,
					   device->iomemname);
	if (res == NULL) {
		KGSL_DRV_ERR(device, "platform_get_resource_byname failed\n");
		return -EINVAL;
	}
	if (res->start == 0 || resource_size(res) == 0) {
		KGSL_DRV_ERR(device, "dev %d invalid register region\n",
			device->id);
		return -EINVAL;
	}

	device->reg_phys = res->start;
	device->reg_len = resource_size(res);

	if (adreno_of_get_pwrlevels(adreno_dev, node))
		return -EINVAL;

	/* Get throttle power level */
	of_property_read_u32(node, "qcom,throttle-pwrlevel", &throt);

	if (throt < device->pwrctrl.num_pwrlevels)
		device->pwrctrl.throttle_mask =
			GENMASK(device->pwrctrl.num_pwrlevels - 1,
				device->pwrctrl.num_pwrlevels - 1 - throt);

	/* Get context aware DCVS properties */
	adreno_of_get_ca_aware_properties(adreno_dev, node);

	l3_pwrlevel_probe(device, node);

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

	device->pwrctrl.bus_control = of_property_read_bool(node,
		"qcom,bus-control");

	device->pwrctrl.input_disable = of_property_read_bool(node,
		"qcom,disable-wake-on-touch");

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

static void adreno_cx_dbgc_probe(struct kgsl_device *device)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct resource *res;

	res = platform_get_resource_byname(device->pdev, IORESOURCE_MEM,
					   "cx_dbgc");

	if (res == NULL)
		return;

	adreno_dev->cx_dbgc_base = res->start - device->reg_phys;
	adreno_dev->cx_dbgc_len = resource_size(res);
	adreno_dev->cx_dbgc_virt = devm_ioremap(device->dev,
					device->reg_phys +
						adreno_dev->cx_dbgc_base,
					adreno_dev->cx_dbgc_len);

	if (adreno_dev->cx_dbgc_virt == NULL)
		KGSL_DRV_WARN(device, "cx_dbgc ioremap failed\n");
}

static void adreno_cx_misc_probe(struct kgsl_device *device)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct resource *res;

	res = platform_get_resource_byname(device->pdev, IORESOURCE_MEM,
					   "cx_misc");

	if (res == NULL)
		return;

	adreno_dev->cx_misc_len = resource_size(res);
	adreno_dev->cx_misc_virt = devm_ioremap(device->dev,
					res->start, adreno_dev->cx_misc_len);
}

static void adreno_efuse_read_soc_hw_rev(struct adreno_device *adreno_dev)
{
	unsigned int val;
	unsigned int soc_hw_rev[3];
	int ret;

	if (of_property_read_u32_array(
		KGSL_DEVICE(adreno_dev)->pdev->dev.of_node,
		"qcom,soc-hw-rev-efuse", soc_hw_rev, 3))
		return;

	ret = adreno_efuse_map(adreno_dev);
	if (ret) {
		KGSL_CORE_ERR(
			"Unable to map hardware revision fuse: ret=%d\n", ret);
		return;
	}

	ret = adreno_efuse_read_u32(adreno_dev, soc_hw_rev[0], &val);
	adreno_efuse_unmap(adreno_dev);

	if (ret) {
		KGSL_CORE_ERR(
			"Unable to read hardware revision fuse: ret=%d\n", ret);
		return;
	}

	adreno_dev->soc_hw_rev = (val >> soc_hw_rev[1]) & soc_hw_rev[2];
}

static bool adreno_is_gpu_disabled(struct adreno_device *adreno_dev)
{
	unsigned int row0;
	unsigned int pte_row0_msb[3];
	int ret;
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);

	if (of_property_read_u32_array(device->pdev->dev.of_node,
		"qcom,gpu-disable-fuse", pte_row0_msb, 3))
		return false;
	/*
	 * Read the fuse value to disable GPU driver if fuse
	 * is blown. By default(fuse value is 0) GPU is enabled.
	 */
	if (adreno_efuse_map(adreno_dev))
		return false;

	ret = adreno_efuse_read_u32(adreno_dev, pte_row0_msb[0], &row0);
	adreno_efuse_unmap(adreno_dev);

	if (ret)
		return false;

	return (row0 >> pte_row0_msb[2]) &
			pte_row0_msb[1] ? true : false;
}

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

	if (adreno_is_gpu_disabled(adreno_dev)) {
		pr_err("adreno: GPU is disabled on this device\n");
		return -ENODEV;
	}

	/* Identify SOC hardware revision to be used */
	adreno_efuse_read_soc_hw_rev(adreno_dev);

	adreno_update_soc_hw_revision_quirks(adreno_dev, pdev);

	/* Get the chip ID from the DT and set up target specific parameters */
	adreno_identify_gpu(adreno_dev);

	status = adreno_of_get_power(adreno_dev, pdev);
	if (status) {
		device->pdev = NULL;
		return status;
	}

	/*
	 * Probe/init GMU after initial gpu power probe
	 * Another part of GPU power probe in platform_probe
	 * needs GMU initialized.
	 */
	status = gmu_core_probe(device);
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

	/* Default to 4K alignment (in other words, no additional padding) */
	device->mmu.va_padding = PAGE_SIZE;

	if (adreno_dev->gpucore->va_padding) {
		device->mmu.features |= KGSL_MMU_PAD_VA;
		device->mmu.va_padding = adreno_dev->gpucore->va_padding;
	}

	if (adreno_dev->gpucore->cx_ipeak_gpu_freq)
		device->pwrctrl.cx_ipeak_gpu_freq =
				adreno_dev->gpucore->cx_ipeak_gpu_freq;

	status = kgsl_device_platform_probe(device);
	if (status) {
		device->pdev = NULL;
		return status;
	}

	/* Probe for the optional CX_DBGC block */
	adreno_cx_dbgc_probe(device);

	/* Probe for the optional CX_MISC block */
	adreno_cx_misc_probe(device);

	/*
	 * qcom,iommu-secure-id is used to identify MMUs that can handle secure
	 * content but that is only part of the story - the GPU also has to be
	 * able to handle secure content.  Unfortunately in a classic catch-22
	 * we cannot identify the GPU until after the DT is parsed. tl;dr -
	 * check the GPU capabilities here and modify mmu->secured accordingly
	 */

	if (!ADRENO_FEATURE(adreno_dev, ADRENO_CONTENT_PROTECTION))
		device->mmu.secured = false;

	if (ADRENO_FEATURE(adreno_dev, ADRENO_IOCOHERENT))
		device->mmu.features |= KGSL_MMU_IO_COHERENT;

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

	/* Get the system cache slice descriptor for GPU */
	adreno_dev->gpu_llc_slice = adreno_llc_getd(&pdev->dev, "gpu");
	if (IS_ERR(adreno_dev->gpu_llc_slice) &&
			PTR_ERR(adreno_dev->gpu_llc_slice) != -ENOENT)
		KGSL_DRV_WARN(device,
			"Failed to get GPU LLC slice descriptor %ld\n",
			PTR_ERR(adreno_dev->gpu_llc_slice));

	/* Get the system cache slice descriptor for GPU pagetables */
	adreno_dev->gpuhtw_llc_slice = adreno_llc_getd(&pdev->dev, "gpuhtw");
	if (IS_ERR(adreno_dev->gpuhtw_llc_slice) &&
			PTR_ERR(adreno_dev->gpuhtw_llc_slice) != -ENOENT)
		KGSL_DRV_WARN(device,
			"Failed to get gpuhtw LLC slice descriptor %ld\n",
			PTR_ERR(adreno_dev->gpuhtw_llc_slice));

#ifdef CONFIG_INPUT
	if (!device->pwrctrl.input_disable) {
		adreno_input_handler.private = device;
		/*
		 * It isn't fatal if we cannot register the input handler.  Sad,
		 * perhaps, but not fatal
		 */
		if (input_register_handler(&adreno_input_handler)) {
			adreno_input_handler.private = NULL;
			KGSL_DRV_ERR(device,
				"Unable to register the input handler\n");
		}
	}
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
	struct adreno_firmware *pfp_fw = ADRENO_FW(adreno_dev, ADRENO_FW_PFP);
	struct adreno_firmware *pm4_fw = ADRENO_FW(adreno_dev, ADRENO_FW_PM4);

	if (test_bit(ADRENO_DEVICE_DRAWOBJ_PROFILE, &adreno_dev->priv))
		kgsl_free_global(device, &adreno_dev->profile_buffer);

	/* Free local copies of firmware and other command streams */
	kfree(pfp_fw->fwvirt);
	pfp_fw->fwvirt = NULL;

	kfree(pm4_fw->fwvirt);
	pm4_fw->fwvirt = NULL;

	kfree(adreno_dev->gpmu_cmds);
	adreno_dev->gpmu_cmds = NULL;

	kgsl_free_global(device, &pfp_fw->memdesc);
	kgsl_free_global(device, &pm4_fw->memdesc);
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
	if (adreno_input_handler.private)
		input_unregister_handler(&adreno_input_handler);
#endif
	adreno_sysfs_close(adreno_dev);

	adreno_coresight_remove(adreno_dev);
	adreno_profile_close(adreno_dev);

	/* Release the system cache slice descriptor */
	adreno_llc_putd(adreno_dev->gpu_llc_slice);
	adreno_llc_putd(adreno_dev->gpuhtw_llc_slice);

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

	gmu_core_remove(device);

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
	int i;

	if (!(swfdetect ||
			ADRENO_FEATURE(adreno_dev, ADRENO_SOFT_FAULT_DETECT)))
		return;

	/* Disable the fast hang detect bit until we know its a go */
	adreno_dev->fast_hang_detect = 0;

	adreno_ft_regs_num = (ARRAY_SIZE(adreno_ft_regs_default) +
		gpudev->ft_perf_counters_count*2);

	adreno_ft_regs = kcalloc(adreno_ft_regs_num, sizeof(unsigned int),
		GFP_KERNEL);
	adreno_ft_regs_val = kcalloc(adreno_ft_regs_num, sizeof(unsigned int),
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

	adreno_fault_detect_start(adreno_dev);
}

/**
 * adreno_clear_pending_transactions() - Clear transactions in GBIF/VBIF pipe
 * @device: Pointer to the device whose GBIF/VBIF pipe is to be cleared
 */
int adreno_clear_pending_transactions(struct kgsl_device *device)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct adreno_gpudev *gpudev = ADRENO_GPU_DEVICE(adreno_dev);
	int ret = 0;

	if (adreno_has_gbif(adreno_dev)) {

		/* Halt new client requests */
		adreno_writereg(adreno_dev, ADRENO_REG_GBIF_HALT,
				gpudev->gbif_client_halt_mask);
		ret = adreno_wait_for_halt_ack(device,
				ADRENO_REG_GBIF_HALT_ACK,
				gpudev->gbif_client_halt_mask);

		/* Halt all AXI requests */
		adreno_writereg(adreno_dev, ADRENO_REG_GBIF_HALT,
				gpudev->gbif_arb_halt_mask);
		ret = adreno_wait_for_halt_ack(device,
				ADRENO_REG_GBIF_HALT_ACK,
				gpudev->gbif_arb_halt_mask);
	} else {
		unsigned int mask = gpudev->vbif_xin_halt_ctrl0_mask;

		adreno_writereg(adreno_dev, ADRENO_REG_VBIF_XIN_HALT_CTRL0,
			mask);
		ret = adreno_wait_for_halt_ack(device,
				ADRENO_REG_VBIF_XIN_HALT_CTRL1, mask);
		adreno_writereg(adreno_dev, ADRENO_REG_VBIF_XIN_HALT_CTRL0, 0);
	}
	return ret;
}

static int adreno_init(struct kgsl_device *device)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct adreno_gpudev *gpudev = ADRENO_GPU_DEVICE(adreno_dev);
	int ret;

	if (!adreno_is_a3xx(adreno_dev))
		kgsl_sharedmem_set(device, &device->scratch, 0, 0,
				device->scratch.size);

	ret = kgsl_pwrctrl_change_state(device, KGSL_STATE_INIT);
	if (ret)
		return ret;

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
	if (ADRENO_GPUREV(adreno_dev) < 600) {
		/* No need for newer generation architectures */
		ret = kgsl_pwrctrl_change_state(device, KGSL_STATE_AWARE);
		if (ret)
			return ret;
	}

	ret = adreno_iommu_init(adreno_dev);
	if (ret)
		return ret;

	adreno_perfcounter_init(adreno_dev);
	adreno_fault_detect_init(adreno_dev);

	/* Power down the device */
	if (ADRENO_GPUREV(adreno_dev) < 600)
		kgsl_pwrctrl_change_state(device, KGSL_STATE_SLUMBER);

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
	 * Allocate a small chunk of memory for precise drawobj profiling for
	 * those targets that have the always on timer
	 */

	if (!adreno_is_a3xx(adreno_dev)) {
		int r = kgsl_allocate_global(device,
			&adreno_dev->profile_buffer, PAGE_SIZE,
			0, 0, "alwayson");

		adreno_dev->profile_index = 0;

		if (r == 0) {
			set_bit(ADRENO_DEVICE_DRAWOBJ_PROFILE,
				&adreno_dev->priv);
			kgsl_sharedmem_set(device,
				&adreno_dev->profile_buffer, 0, 0,
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

	if (gmu_core_gpmu_isenabled(device))
		return false;

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

static void _set_secvid(struct kgsl_device *device)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	static bool set;

	/* Program GPU contect protection init values */
	if (device->mmu.secured && !set) {
		if (adreno_is_a4xx(adreno_dev))
			adreno_writereg(adreno_dev,
				ADRENO_REG_RBBM_SECVID_TRUST_CONFIG, 0x2);
		adreno_writereg(adreno_dev,
				ADRENO_REG_RBBM_SECVID_TSB_CONTROL, 0x0);

		adreno_writereg64(adreno_dev,
			ADRENO_REG_RBBM_SECVID_TSB_TRUSTED_BASE,
			ADRENO_REG_RBBM_SECVID_TSB_TRUSTED_BASE_HI,
			KGSL_IOMMU_SECURE_BASE(&device->mmu));
		adreno_writereg(adreno_dev,
			ADRENO_REG_RBBM_SECVID_TSB_TRUSTED_SIZE,
			KGSL_IOMMU_SECURE_SIZE);
		if (ADRENO_QUIRK(adreno_dev, ADRENO_QUIRK_SECVID_SET_ONCE))
			set = true;
	}
}

static int adreno_switch_to_unsecure_mode(struct adreno_device *adreno_dev,
				struct adreno_ringbuffer *rb)
{
	unsigned int *cmds;
	int ret;

	cmds = adreno_ringbuffer_allocspace(rb, 2);
	if (IS_ERR(cmds))
		return PTR_ERR(cmds);
	if (cmds == NULL)
		return -ENOSPC;

	cmds += cp_secure_mode(adreno_dev, cmds, 0);

	ret = adreno_ringbuffer_submit_spin(rb, NULL, 2000);
	if (ret)
		adreno_spin_idle_debug(adreno_dev,
				"Switch to unsecure failed to idle\n");

	return ret;
}

int adreno_set_unsecured_mode(struct adreno_device *adreno_dev,
		struct adreno_ringbuffer *rb)
{
	int ret = 0;

	if (!adreno_is_a5xx(adreno_dev) && !adreno_is_a6xx(adreno_dev))
		return -EINVAL;

	if (ADRENO_QUIRK(adreno_dev, ADRENO_QUIRK_CRITICAL_PACKETS) &&
			adreno_is_a5xx(adreno_dev)) {
		ret = a5xx_critical_packet_submit(adreno_dev, rb);
		if (ret)
			return ret;
	}

	/* GPU comes up in secured mode, make it unsecured by default */
	if (adreno_dev->zap_loaded)
		ret = adreno_switch_to_unsecure_mode(adreno_dev, rb);
	else
		adreno_writereg(adreno_dev,
				ADRENO_REG_RBBM_SECVID_TRUST_CONTROL, 0x0);

	return ret;
}

static void adreno_set_active_ctxs_null(struct adreno_device *adreno_dev)
{
	int i;
	struct adreno_ringbuffer *rb;

	FOR_EACH_RINGBUFFER(adreno_dev, rb, i) {
		if (rb->drawctxt_active)
			kgsl_context_put(&(rb->drawctxt_active->base));
		rb->drawctxt_active = NULL;

		kgsl_sharedmem_writel(KGSL_DEVICE(adreno_dev),
			&rb->pagetable_desc, PT_INFO_OFFSET(current_rb_ptname),
			0);
	}
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
	struct gmu_dev_ops *gmu_dev_ops = GMU_DEVICE_OPS(device);
	int status = -EINVAL, ret;
	unsigned int state = device->state;
	bool regulator_left_on;
	unsigned int pmqos_wakeup_vote = device->pwrctrl.pm_qos_wakeup_latency;
	unsigned int pmqos_active_vote = device->pwrctrl.pm_qos_active_latency;

	/* make sure ADRENO_DEVICE_STARTED is not set here */
	WARN_ON(test_bit(ADRENO_DEVICE_STARTED, &adreno_dev->priv));

	/* disallow l2pc during wake up to improve GPU wake up time */
	kgsl_pwrctrl_update_l2pc(&adreno_dev->dev,
			KGSL_L2PC_WAKEUP_TIMEOUT);

	pm_qos_update_request(&device->pwrctrl.pm_qos_req_dma,
			pmqos_wakeup_vote);

	regulator_left_on = regulators_left_on(device);

	/* Clear any GPU faults that might have been left over */
	adreno_clear_gpu_fault(adreno_dev);

	/* Put the GPU in a responsive state */
	status = kgsl_pwrctrl_change_state(device, KGSL_STATE_AWARE);
	if (status)
		goto error_pwr_off;

	/* Set any stale active contexts to NULL */
	adreno_set_active_ctxs_null(adreno_dev);

	/* Set the bit to indicate that we've just powered on */
	set_bit(ADRENO_DEVICE_PWRON, &adreno_dev->priv);

	/* Soft reset the GPU if a regulator is stuck on*/
	if (regulator_left_on)
		_soft_reset(adreno_dev);


	if (adreno_is_a640v1(adreno_dev)) {
		unsigned long start = jiffies;

		if (scm_is_call_available(SCM_SVC_MP, CP_SMMU_APERTURE_ID)) {
			ret = kgsl_program_smmu_aperture();
			/* Log it if it takes more than 2 seconds */
			if (((jiffies - start) / HZ) > 2)
				dev_err(device->dev, "scm call took too long to finish on a640v1: %lu seconds\n",
					((jiffies - start) / HZ));
			if (ret) {
				dev_err(device->dev, "SMMU aperture programming call failed with error %d\n",
					ret);
				goto error_pwr_off;
			}
		}
	}

	adreno_ringbuffer_set_global(adreno_dev, 0);

	status = kgsl_mmu_start(device);
	if (status)
		goto error_boot_oob_clear;

	status = adreno_ocmem_malloc(adreno_dev);
	if (status) {
		KGSL_DRV_ERR(device, "OCMEM malloc failed\n");
		goto error_mmu_off;
	}

	/* Send OOB request to turn on the GX */
	if (GMU_DEV_OP_VALID(gmu_dev_ops, oob_set)) {
		status = gmu_dev_ops->oob_set(adreno_dev, oob_gpu);
		if (status)
			goto error_mmu_off;
	}

	if (GMU_DEV_OP_VALID(gmu_dev_ops, hfi_start_msg)) {
		status = gmu_dev_ops->hfi_start_msg(adreno_dev);
		if (status)
			goto error_oob_clear;
	}

	_set_secvid(device);

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
			WARN_ONCE(1, "Unable to get perf counters for DCVS\n");
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

		if (adreno_has_gbif(adreno_dev)) {
			if (adreno_dev->starved_ram_lo_ch1 == 0) {
				ret = adreno_perfcounter_get(adreno_dev,
					KGSL_PERFCOUNTER_GROUP_VBIF_PWR, 1,
					&adreno_dev->starved_ram_lo_ch1, NULL,
					PERFCOUNTER_FLAG_KERNEL);

				if (ret) {
					KGSL_DRV_ERR(device,
						"Unable to get perf counters for bus DCVS\n");
					adreno_dev->starved_ram_lo_ch1 = 0;
				}
			}

			if (adreno_dev->ram_cycles_lo == 0) {
				ret = adreno_perfcounter_get(adreno_dev,
					KGSL_PERFCOUNTER_GROUP_VBIF,
					GBIF_AXI0_READ_DATA_TOTAL_BEATS,
					&adreno_dev->ram_cycles_lo, NULL,
					PERFCOUNTER_FLAG_KERNEL);

				if (ret) {
					KGSL_DRV_ERR(device,
						"Unable to get perf counters for bus DCVS\n");
					adreno_dev->ram_cycles_lo = 0;
				}
			}

			if (adreno_dev->ram_cycles_lo_ch1_read == 0) {
				ret = adreno_perfcounter_get(adreno_dev,
					KGSL_PERFCOUNTER_GROUP_VBIF,
					GBIF_AXI1_READ_DATA_TOTAL_BEATS,
					&adreno_dev->ram_cycles_lo_ch1_read,
					NULL,
					PERFCOUNTER_FLAG_KERNEL);

				if (ret) {
					KGSL_DRV_ERR(device,
						"Unable to get perf counters for bus DCVS\n");
					adreno_dev->ram_cycles_lo_ch1_read = 0;
				}
			}

			if (adreno_dev->ram_cycles_lo_ch0_write == 0) {
				ret = adreno_perfcounter_get(adreno_dev,
					KGSL_PERFCOUNTER_GROUP_VBIF,
					GBIF_AXI0_WRITE_DATA_TOTAL_BEATS,
					&adreno_dev->ram_cycles_lo_ch0_write,
					NULL,
					PERFCOUNTER_FLAG_KERNEL);

				if (ret) {
					KGSL_DRV_ERR(device,
						"Unable to get perf counters for bus DCVS\n");
					adreno_dev->ram_cycles_lo_ch0_write = 0;
				}
			}

			if (adreno_dev->ram_cycles_lo_ch1_write == 0) {
				ret = adreno_perfcounter_get(adreno_dev,
					KGSL_PERFCOUNTER_GROUP_VBIF,
					GBIF_AXI1_WRITE_DATA_TOTAL_BEATS,
					&adreno_dev->ram_cycles_lo_ch1_write,
					NULL,
					PERFCOUNTER_FLAG_KERNEL);

				if (ret) {
					KGSL_DRV_ERR(device,
						"Unable to get perf counters for bus DCVS\n");
					adreno_dev->ram_cycles_lo_ch1_write = 0;
				}
			}
		} else {
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
	}

	if (gmu_core_isenabled(device) && adreno_dev->perfctr_ifpc_lo == 0) {
		ret = adreno_perfcounter_get(adreno_dev,
				KGSL_PERFCOUNTER_GROUP_GPMU_PWR, 4,
				&adreno_dev->perfctr_ifpc_lo, NULL,
				PERFCOUNTER_FLAG_KERNEL);
		if (ret) {
			WARN_ONCE(1, "Unable to get perf counter for IFPC\n");
			adreno_dev->perfctr_ifpc_lo = 0;
		}
	}

	/* Clear the busy_data stats - we're starting over from scratch */
	adreno_dev->busy_data.gpu_busy = 0;
	adreno_dev->busy_data.bif_ram_cycles = 0;
	adreno_dev->busy_data.bif_ram_cycles_read_ch1 = 0;
	adreno_dev->busy_data.bif_ram_cycles_write_ch0 = 0;
	adreno_dev->busy_data.bif_ram_cycles_write_ch1 = 0;
	adreno_dev->busy_data.bif_starved_ram = 0;
	adreno_dev->busy_data.bif_starved_ram_ch1 = 0;
	adreno_dev->busy_data.num_ifpc = 0;

	/* Restore performance counter registers with saved values */
	adreno_perfcounter_restore(adreno_dev);

	/* Start the GPU */
	gpudev->start(adreno_dev);

	/*
	 * The system cache control registers
	 * live on the CX/GX rail. Hence need
	 * reprogramming everytime the GPU
	 * comes out of power collapse.
	 */
	adreno_llc_setup(device);

	/* Re-initialize the coresight registers if applicable */
	adreno_coresight_start(adreno_dev);

	adreno_irqctrl(adreno_dev, 1);

	adreno_perfcounter_start(adreno_dev);

	/* Clear FSR here in case it is set from a previous pagefault */
	kgsl_mmu_clear_fsr(&device->mmu);

	status = adreno_ringbuffer_start(adreno_dev, ADRENO_START_COLD);
	if (status)
		goto error_oob_clear;

	/* Start the dispatcher */
	adreno_dispatcher_start(device);

	device->reset_counter++;

	set_bit(ADRENO_DEVICE_STARTED, &adreno_dev->priv);

	if (pmqos_active_vote != pmqos_wakeup_vote)
		pm_qos_update_request(&device->pwrctrl.pm_qos_req_dma,
				pmqos_active_vote);

	/* Send OOB request to allow IFPC */
	if (GMU_DEV_OP_VALID(gmu_dev_ops, oob_clear)) {
		gmu_dev_ops->oob_clear(adreno_dev, oob_gpu);

		/* If we made it this far, the BOOT OOB was sent to the GMU */
		if (ADRENO_QUIRK(adreno_dev, ADRENO_QUIRK_HFI_USE_REG))
			gmu_dev_ops->oob_clear(adreno_dev, oob_boot_slumber);
	}

	return 0;

error_oob_clear:
	if (GMU_DEV_OP_VALID(gmu_dev_ops, oob_clear))
		gmu_dev_ops->oob_clear(adreno_dev, oob_gpu);

error_mmu_off:
	kgsl_mmu_stop(&device->mmu);

error_boot_oob_clear:
	if (GMU_DEV_OP_VALID(gmu_dev_ops, oob_clear) &&
		ADRENO_QUIRK(adreno_dev, ADRENO_QUIRK_HFI_USE_REG))
		gmu_dev_ops->oob_clear(adreno_dev, oob_boot_slumber);

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
int adreno_start(struct kgsl_device *device, int priority)
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

static int adreno_stop(struct kgsl_device *device)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct gmu_dev_ops *gmu_dev_ops = GMU_DEVICE_OPS(device);
	int error = 0;

	if (!test_bit(ADRENO_DEVICE_STARTED, &adreno_dev->priv))
		return 0;

	/* Turn the power on one last time before stopping */
	if (GMU_DEV_OP_VALID(gmu_dev_ops, oob_set)) {
		error = gmu_dev_ops->oob_set(adreno_dev, oob_gpu);
		if (error && GMU_DEV_OP_VALID(gmu_dev_ops, oob_clear)) {
			gmu_dev_ops->oob_clear(adreno_dev, oob_gpu);
			if (gmu_core_regulator_isenabled(device)) {
				/* GPU is on. Try recovery */
				set_bit(GMU_FAULT, &device->gmu_core.flags);
				gmu_core_snapshot(device);
				error = -EINVAL;
			} else {
				return error;
			}
		}
	}

	adreno_dispatcher_stop(adreno_dev);

	adreno_ringbuffer_stop(adreno_dev);

	kgsl_pwrscale_update_stats(device);

	adreno_irqctrl(adreno_dev, 0);

	adreno_ocmem_free(adreno_dev);

	adreno_llc_deactivate_slice(adreno_dev->gpu_llc_slice);
	adreno_llc_deactivate_slice(adreno_dev->gpuhtw_llc_slice);

	/* Save active coresight registers if applicable */
	adreno_coresight_stop(adreno_dev);

	/* Save physical performance counter values before GPU power down*/
	adreno_perfcounter_save(adreno_dev);

	if (GMU_DEV_OP_VALID(gmu_dev_ops, prepare_stop))
		gmu_dev_ops->prepare_stop(adreno_dev);

	if (GMU_DEV_OP_VALID(gmu_dev_ops, oob_clear))
		gmu_dev_ops->oob_clear(adreno_dev, oob_gpu);

	/*
	 * Saving perfcounters will use an OOB to put the GMU into
	 * active state. Before continuing, we should wait for the
	 * GMU to return to the lowest idle level. This is
	 * because some idle level transitions require VBIF and MMU.
	 */
	if (!error && GMU_DEV_OP_VALID(gmu_dev_ops, wait_for_lowest_idle) &&
			gmu_dev_ops->wait_for_lowest_idle(adreno_dev)) {

		set_bit(GMU_FAULT, &device->gmu_core.flags);
		gmu_core_snapshot(device);
		/*
		 * Assume GMU hang after 10ms without responding.
		 * It shall be relative safe to clear vbif and stop
		 * MMU later. Early return in adreno_stop function
		 * will result in kernel panic in adreno_start
		 */
		error = -EINVAL;
	}

	adreno_clear_pending_transactions(device);

	/* The halt is not cleared in the above function if we have GBIF */
	adreno_deassert_gbif_halt(adreno_dev);

	kgsl_mmu_stop(&device->mmu);

	/*
	 * At this point, MMU is turned off so we can safely
	 * destroy any pending contexts and their pagetables
	 */
	adreno_set_active_ctxs_null(adreno_dev);

	clear_bit(ADRENO_DEVICE_STARTED, &adreno_dev->priv);

	return error;
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
	int ret = -EINVAL;
	int i = 0;

	/* Try soft reset first */
	if (adreno_try_soft_reset(device, fault)) {
		/* Make sure VBIF is cleared before resetting */
		ret = adreno_clear_pending_transactions(device);

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

		/* Try to reset the device */
		ret = adreno_start(device, 0);

		/* On some GPUS, keep trying until it works */
		if (ret && ADRENO_GPUREV(adreno_dev) < 600) {
			for (i = 0; i < NUM_TIMES_RESET_RETRY; i++) {
				msleep(20);
				ret = adreno_start(device, 0);
				if (!ret)
					break;
			}
		}
	}
	if (ret)
		return ret;

	if (i != 0)
		KGSL_DRV_WARN(device, "Device hard reset tried %d tries\n", i);

	/*
	 * If active_cnt is non-zero then the system was active before
	 * going into a reset - put it back in that state
	 */

	if (atomic_read(&device->active_cnt))
		kgsl_pwrctrl_change_state(device, KGSL_STATE_ACTIVE);
	else
		kgsl_pwrctrl_change_state(device, KGSL_STATE_NAP);

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
					(unsigned long)device->memstore.gpuaddr;
				shadowprop.size = device->memstore.size;
				/* GSL needs this to be set, even if it
				 * appears to be meaningless
				 */
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
	case KGSL_PROP_DEVICE_QDSS_STM:
		{
			struct kgsl_qdss_stm_prop qdssprop = {0};
			struct kgsl_memdesc *qdss_desc =
				kgsl_mmu_get_qdss_global_entry(device);

			if (sizebytes != sizeof(qdssprop)) {
				status = -EINVAL;
				break;
			}

			if (qdss_desc) {
				qdssprop.gpuaddr = qdss_desc->gpuaddr;
				qdssprop.size = qdss_desc->size;
			}

			if (copy_to_user(value, &qdssprop,
						sizeof(qdssprop))) {
				status = -EFAULT;
				break;
			}
			status = 0;
		}
		break;
	case KGSL_PROP_DEVICE_QTIMER:
		{
			struct kgsl_qtimer_prop qtimerprop = {0};
			struct kgsl_memdesc *qtimer_desc =
				kgsl_mmu_get_qtimer_global_entry(device);

			if (sizebytes != sizeof(qtimerprop)) {
				status = -EINVAL;
				break;
			}

			if (qtimer_desc) {
				qtimerprop.gpuaddr = qtimer_desc->gpuaddr;
				qtimerprop.size = qtimer_desc->size;
			}

			if (copy_to_user(value, &qtimerprop,
						sizeof(qtimerprop))) {
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

			if (adreno_is_a5xx(adreno_dev) ||
					adreno_is_a6xx(adreno_dev))
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

			ucode.pfp = adreno_dev->fw[ADRENO_FW_PFP].version;
			ucode.pm4 = adreno_dev->fw[ADRENO_FW_PM4].version;

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
	case KGSL_PROP_MIN_ACCESS_LENGTH:
		{
			unsigned int mal;

			if (sizebytes < sizeof(unsigned int)) {
				status = -EINVAL;
				break;
			}

			if (of_property_read_u32(device->pdev->dev.of_node,
				"qcom,min-access-length", &mal)) {
				mal = 0;
			}

			if (copy_to_user(value, &mal, sizeof(mal))) {
				status = -EFAULT;
				break;
			}
		}
		status = 0;
		break;
	case KGSL_PROP_UBWC_MODE:
		{
			unsigned int mode;

			if (sizebytes < sizeof(unsigned int)) {
				status = -EINVAL;
				break;
			}

			if (of_property_read_u32(device->pdev->dev.of_node,
				"qcom,ubwc-mode", &mode))
				mode = 0;

			if (copy_to_user(value, &mode, sizeof(mode))) {
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

	case KGSL_PROP_SPEED_BIN:
		{
			unsigned int speed_bin;

			if (sizebytes != sizeof(unsigned int)) {
				status = -EINVAL;
				break;
			}

			speed_bin = adreno_dev->speed_bin;

			if (copy_to_user(value, &speed_bin,
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
	case KGSL_CONSTRAINT_L3_PWRLEVEL: {
		struct kgsl_device_constraint_pwrlevel pwr;

		if (constraint->size != sizeof(pwr)) {
			status = -EINVAL;
			break;
		}

		if (copy_from_user(&pwr, constraint->data, sizeof(pwr))) {
			status = -EFAULT;
			break;
		}
		if (pwr.level >= KGSL_CONSTRAINT_PWR_MAXLEVELS)
			pwr.level = KGSL_CONSTRAINT_PWR_MAXLEVELS - 1;

		context->l3_pwr_constraint.type = KGSL_CONSTRAINT_L3_PWRLEVEL;
		context->l3_pwr_constraint.sub_type = pwr.level;
		trace_kgsl_user_pwrlevel_constraint(device, context->id,
			context->l3_pwr_constraint.type,
			context->l3_pwr_constraint.sub_type);
		}
		break;
	case KGSL_CONSTRAINT_L3_NONE: {
		unsigned int type = context->l3_pwr_constraint.type;

		if (type == KGSL_CONSTRAINT_L3_PWRLEVEL)
			trace_kgsl_user_pwrlevel_constraint(device, context->id,
				KGSL_CONSTRAINT_L3_NONE,
				context->l3_pwr_constraint.sub_type);
		context->l3_pwr_constraint.type = KGSL_CONSTRAINT_L3_NONE;
		}
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
				kgsl_pwrscale_disable(device, true);
			}

			mutex_unlock(&device->mutex);
			status = 0;
		}
		break;
	case KGSL_PROP_PWR_CONSTRAINT:
	case KGSL_PROP_L3_PWR_CONSTRAINT: {
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

	/*
	 * IRQ handler clears the RBBM INT0 status register immediately
	 * entering the ISR before actually serving the interrupt because
	 * of this we can't rely only on RBBM INT0 status only.
	 * Use pending_irq_refcnt along with RBBM INT0 to correctly
	 * determine whether any IRQ is pending or not.
	 */
	if ((status & gpudev->irq->mask) ||
		atomic_read(&adreno_dev->pending_irq_refcnt))
		return 1;
	else
		return 0;
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
	struct adreno_gpudev *gpudev  = ADRENO_GPU_DEVICE(adreno_dev);

	/* if hw driver implements idle check - use it */
	if (gpudev->hw_isidle)
		return gpudev->hw_isidle(adreno_dev);

	if (adreno_is_a540(adreno_dev))
		/**
		 * Due to CRC idle throttling GPU
		 * idle hysteresys can take up to
		 * 3usec for expire - account for it
		 */
		udelay(5);

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
int adreno_soft_reset(struct kgsl_device *device)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct adreno_gpudev *gpudev = ADRENO_GPU_DEVICE(adreno_dev);
	struct gmu_dev_ops *gmu_dev_ops = GMU_DEVICE_OPS(device);
	int ret;

	if (GMU_DEV_OP_VALID(gmu_dev_ops, oob_set)) {
		ret = gmu_dev_ops->oob_set(adreno_dev, oob_gpu);
		if (ret)
			return ret;
	}

	kgsl_pwrctrl_change_state(device, KGSL_STATE_AWARE);
	adreno_set_active_ctxs_null(adreno_dev);

	adreno_irqctrl(adreno_dev, 0);

	adreno_clear_gpu_fault(adreno_dev);
	/* since device is oficially off now clear start bit */
	clear_bit(ADRENO_DEVICE_STARTED, &adreno_dev->priv);

	/* save physical performance counter values before GPU soft reset */
	adreno_perfcounter_save(adreno_dev);

	/* Reset the GPU */
	if (gpudev->soft_reset)
		ret = gpudev->soft_reset(adreno_dev);
	else
		ret = _soft_reset(adreno_dev);
	if (ret) {
		if (GMU_DEV_OP_VALID(gmu_dev_ops, oob_clear))
			gmu_dev_ops->oob_clear(adreno_dev, oob_gpu);
		return ret;
	}

	/* Clear the busy_data stats - we're starting over from scratch */
	adreno_dev->busy_data.gpu_busy = 0;
	adreno_dev->busy_data.bif_ram_cycles = 0;
	adreno_dev->busy_data.bif_ram_cycles_read_ch1 = 0;
	adreno_dev->busy_data.bif_ram_cycles_write_ch0 = 0;
	adreno_dev->busy_data.bif_ram_cycles_write_ch1 = 0;
	adreno_dev->busy_data.bif_starved_ram = 0;
	adreno_dev->busy_data.bif_starved_ram_ch1 = 0;

	/* Set the page table back to the default page table */
	adreno_ringbuffer_set_global(adreno_dev, 0);
	kgsl_mmu_set_pt(&device->mmu, device->mmu.defaultpagetable);

	_set_secvid(device);

	/* Enable 64 bit gpu addr if feature is set */
	if (gpudev->enable_64bit &&
			adreno_support_64bit(adreno_dev))
		gpudev->enable_64bit(adreno_dev);


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

	/* Restore physical performance counter values after soft reset */
	adreno_perfcounter_restore(adreno_dev);

	if (GMU_DEV_OP_VALID(gmu_dev_ops, oob_clear))
		gmu_dev_ops->oob_clear(adreno_dev, oob_gpu);

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

/* Print some key registers if a spin-for-idle times out */
void adreno_spin_idle_debug(struct adreno_device *adreno_dev,
		const char *str)
{
	struct kgsl_device *device = &adreno_dev->dev;
	unsigned int rptr, wptr;
	unsigned int status, status3, intstatus;
	unsigned int hwfault;

	dev_err(device->dev, str);

	adreno_readreg(adreno_dev, ADRENO_REG_CP_RB_RPTR, &rptr);
	adreno_readreg(adreno_dev, ADRENO_REG_CP_RB_WPTR, &wptr);

	adreno_readreg(adreno_dev, ADRENO_REG_RBBM_STATUS, &status);
	adreno_readreg(adreno_dev, ADRENO_REG_RBBM_STATUS3, &status3);
	adreno_readreg(adreno_dev, ADRENO_REG_RBBM_INT_0_STATUS, &intstatus);
	adreno_readreg(adreno_dev, ADRENO_REG_CP_HW_FAULT, &hwfault);

	dev_err(device->dev,
		"rb=%d pos=%X/%X rbbm_status=%8.8X/%8.8X int_0_status=%8.8X\n",
		adreno_dev->cur_rb->id, rptr, wptr, status, status3, intstatus);

	dev_err(device->dev, " hwfault=%8.8X\n", hwfault);

	kgsl_device_snapshot(device, NULL, adreno_gmu_gpu_fault(adreno_dev));
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

		/* relax tight loop */
		cond_resched();

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
 * Caller must hold the device mutex, and must not hold the dispatcher mutex.
 */

int adreno_idle(struct kgsl_device *device)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	int ret;

	/*
	 * Make sure the device mutex is held so the dispatcher can't send any
	 * more commands to the hardware
	 */

	if (WARN_ON(!mutex_is_locked(&device->mutex)))
		return -EDEADLK;

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
 * Drain the dispatcher of existing drawobjs.  This halts
 * additional commands from being issued until the gate is completed.
 */
static int adreno_drain(struct kgsl_device *device)
{
	reinit_completion(&device->halt_gate);

	return 0;
}

/* Caller must hold the device mutex. */
static int adreno_suspend_context(struct kgsl_device *device)
{
	/* process any profiling results that are available */
	adreno_profile_process_results(ADRENO_DEVICE(device));

	/* Wait for the device to go idle */
	return adreno_idle(device);
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

	void __iomem *reg;

	/* Make sure we're not reading from invalid memory */
	if (WARN(offsetwords * sizeof(uint32_t) >= mem_len,
		"Out of bounds register read: 0x%x/0x%x\n",
			offsetwords, mem_len >> 2))
		return;

	reg = (base + (offsetwords << 2));

	if (!in_interrupt())
		kgsl_pre_hwaccess(device);

	*value = __raw_readl(reg);
	/*
	 * ensure this read finishes before the next one.
	 * i.e. act like normal readl()
	 */
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
	void __iomem *reg;

	/* Make sure we're not writing to an invalid register */
	if (WARN(offsetwords * sizeof(uint32_t) >= device->reg_len,
		"Out of bounds register write: 0x%x/0x%x\n",
			offsetwords, device->reg_len >> 2))
		return;

	if (!in_interrupt())
		kgsl_pre_hwaccess(device);

	trace_kgsl_regwrite(device, offsetwords, value);

	reg = (device->reg_virt + (offsetwords << 2));

	/*
	 * ensure previous writes post before this one,
	 * i.e. act like normal writel()
	 */
	wmb();
	__raw_writel(value, reg);
}

/**
 * adreno_gmu_clear_and_unmask_irqs() - Clear pending IRQs and Unmask IRQs
 * @adreno_dev: Pointer to the Adreno device that owns the GMU
 */
void adreno_gmu_clear_and_unmask_irqs(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct gmu_dev_ops *gmu_dev_ops = GMU_DEVICE_OPS(device);

	/* Clear any pending IRQs before unmasking on GMU */
	adreno_write_gmureg(adreno_dev, ADRENO_REG_GMU_GMU2HOST_INTR_CLR,
			0xFFFFFFFF);
	adreno_write_gmureg(adreno_dev, ADRENO_REG_GMU_AO_HOST_INTERRUPT_CLR,
			0xFFFFFFFF);

	/* Unmask needed IRQs on GMU */
	adreno_write_gmureg(adreno_dev, ADRENO_REG_GMU_GMU2HOST_INTR_MASK,
			(unsigned int) ~(gmu_dev_ops->gmu2host_intr_mask));
	adreno_write_gmureg(adreno_dev, ADRENO_REG_GMU_AO_HOST_INTERRUPT_MASK,
			(unsigned int) ~(gmu_dev_ops->gmu_ao_intr_mask));
}

/**
 * adreno_gmu_mask_and_clear_irqs() - Mask all IRQs and clear pending IRQs
 * @adreno_dev: Pointer to the Adreno device that owns the GMU
 */
void adreno_gmu_mask_and_clear_irqs(struct adreno_device *adreno_dev)
{
	/* Mask all IRQs on GMU */
	adreno_write_gmureg(adreno_dev, ADRENO_REG_GMU_AO_HOST_INTERRUPT_MASK,
			0xFFFFFFFF);
	adreno_write_gmureg(adreno_dev, ADRENO_REG_GMU_GMU2HOST_INTR_MASK,
			0xFFFFFFFF);

	/* Clear any pending IRQs before disabling */
	adreno_write_gmureg(adreno_dev, ADRENO_REG_GMU_AO_HOST_INTERRUPT_CLR,
			0xFFFFFFFF);
	adreno_write_gmureg(adreno_dev, ADRENO_REG_GMU_GMU2HOST_INTR_CLR,
			0xFFFFFFFF);
}

/*
 * adreno_gmu_fenced_write() - Check if there is a GMU and it is enabled
 * @adreno_dev: Pointer to the Adreno device that owns the GMU
 * @offset: 32bit register enum that is to be written
 * @val: The value to be written to the register
 * @fence_mask: The value to poll the fence status register
 *
 * Check the WRITEDROPPED0/1 bit in the FENCE_STATUS register to check if
 * the write to the fenced register went through. If it didn't then we retry
 * the write until it goes through or we time out.
 */
int adreno_gmu_fenced_write(struct adreno_device *adreno_dev,
		enum adreno_regs offset, unsigned int val,
		unsigned int fence_mask)
{
	unsigned int status, i;
	struct adreno_gpudev *gpudev = ADRENO_GPU_DEVICE(adreno_dev);
	unsigned int reg_offset = gpudev->reg_offsets->offsets[offset];

	adreno_writereg(adreno_dev, offset, val);

	if (!gmu_core_isenabled(KGSL_DEVICE(adreno_dev)))
		return 0;

	for (i = 0; i < GMU_CORE_LONG_WAKEUP_RETRY_LIMIT; i++) {
		adreno_read_gmureg(adreno_dev, ADRENO_REG_GMU_AHB_FENCE_STATUS,
			&status);

		/*
		 * If !writedropped0/1, then the write to fenced register
		 * was successful
		 */
		if (!(status & fence_mask))
			return 0;
		/* Wait a small amount of time before trying again */
		udelay(GMU_CORE_WAKEUP_DELAY_US);

		/* Try to write the fenced register again */
		adreno_writereg(adreno_dev, offset, val);

		if (i == GMU_CORE_SHORT_WAKEUP_RETRY_LIMIT)
			dev_err(adreno_dev->dev.dev,
				"Waited %d usecs to write fenced register 0x%x. Continuing to wait...\n",
				(GMU_CORE_SHORT_WAKEUP_RETRY_LIMIT *
				GMU_CORE_WAKEUP_DELAY_US),
				reg_offset);
	}

	dev_err(adreno_dev->dev.dev,
		"Timed out waiting %d usecs to write fenced register 0x%x\n",
		GMU_CORE_LONG_WAKEUP_RETRY_LIMIT * GMU_CORE_WAKEUP_DELAY_US,
		reg_offset);
	return -ETIMEDOUT;
}

unsigned int adreno_gmu_ifpc_show(struct adreno_device *adreno_dev)
{
	struct gmu_dev_ops *gmu_dev_ops = GMU_DEVICE_OPS(
			KGSL_DEVICE(adreno_dev));

	if (GMU_DEV_OP_VALID(gmu_dev_ops, ifpc_show))
		return gmu_dev_ops->ifpc_show(adreno_dev);

	return 0;
}

int adreno_gmu_ifpc_store(struct adreno_device *adreno_dev, unsigned int val)
{
	struct gmu_dev_ops *gmu_dev_ops = GMU_DEVICE_OPS(
			KGSL_DEVICE(adreno_dev));

	if (GMU_DEV_OP_VALID(gmu_dev_ops, ifpc_store))
		return gmu_dev_ops->ifpc_store(adreno_dev, val);

	return -EINVAL;
}

bool adreno_is_cx_dbgc_register(struct kgsl_device *device,
		unsigned int offsetwords)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);

	return adreno_dev->cx_dbgc_virt &&
		(offsetwords >= (adreno_dev->cx_dbgc_base >> 2)) &&
		(offsetwords < (adreno_dev->cx_dbgc_base +
				adreno_dev->cx_dbgc_len) >> 2);
}

void adreno_cx_dbgc_regread(struct kgsl_device *device,
	unsigned int offsetwords, unsigned int *value)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	unsigned int cx_dbgc_offset;

	if (!adreno_is_cx_dbgc_register(device, offsetwords))
		return;

	cx_dbgc_offset = (offsetwords << 2) - adreno_dev->cx_dbgc_base;
	*value = __raw_readl(adreno_dev->cx_dbgc_virt + cx_dbgc_offset);

	/*
	 * ensure this read finishes before the next one.
	 * i.e. act like normal readl()
	 */
	rmb();
}

void adreno_cx_dbgc_regwrite(struct kgsl_device *device,
	unsigned int offsetwords, unsigned int value)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	unsigned int cx_dbgc_offset;

	if (!adreno_is_cx_dbgc_register(device, offsetwords))
		return;

	cx_dbgc_offset = (offsetwords << 2) - adreno_dev->cx_dbgc_base;
	trace_kgsl_regwrite(device, offsetwords, value);

	/*
	 * ensure previous writes post before this one,
	 * i.e. act like normal writel()
	 */
	wmb();
	__raw_writel(value, adreno_dev->cx_dbgc_virt + cx_dbgc_offset);
}

void adreno_cx_misc_regread(struct adreno_device *adreno_dev,
	unsigned int offsetwords, unsigned int *value)
{
	unsigned int cx_misc_offset;

	cx_misc_offset = (offsetwords << 2);
	if (!adreno_dev->cx_misc_virt ||
		(cx_misc_offset >= adreno_dev->cx_misc_len))
		return;

	*value = __raw_readl(adreno_dev->cx_misc_virt + cx_misc_offset);

	/*
	 * ensure this read finishes before the next one.
	 * i.e. act like normal readl()
	 */
	rmb();
}

void adreno_cx_misc_regwrite(struct adreno_device *adreno_dev,
	unsigned int offsetwords, unsigned int value)
{
	unsigned int cx_misc_offset;

	cx_misc_offset = (offsetwords << 2);
	if (!adreno_dev->cx_misc_virt ||
		(cx_misc_offset >= adreno_dev->cx_misc_len))
		return;

	/*
	 * ensure previous writes post before this one,
	 * i.e. act like normal writel()
	 */
	wmb();
	__raw_writel(value, adreno_dev->cx_misc_virt + cx_misc_offset);
}

void adreno_cx_misc_regrmw(struct adreno_device *adreno_dev,
		unsigned int offsetwords,
		unsigned int mask, unsigned int bits)
{
	unsigned int val = 0;

	adreno_cx_misc_regread(adreno_dev, offsetwords, &val);
	val &= ~mask;
	adreno_cx_misc_regwrite(adreno_dev, offsetwords, val | bits);
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

	if (type == KGSL_TIMESTAMP_QUEUED)
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

/**
 * adreno_device_private_create(): Allocate an adreno_device_private structure
 */
struct kgsl_device_private *adreno_device_private_create(void)
{
	struct adreno_device_private *adreno_priv =
			kzalloc(sizeof(*adreno_priv), GFP_KERNEL);

	if (adreno_priv) {
		INIT_LIST_HEAD(&adreno_priv->perfcounter_list);
		return &adreno_priv->dev_priv;
	}
	return NULL;
}

/**
 * adreno_device_private_destroy(): Destroy an adreno_device_private structure
 * and release the perfcounters held by the kgsl fd.
 * @dev_priv: The kgsl device private structure
 */
void adreno_device_private_destroy(struct kgsl_device_private *dev_priv)
{
	struct kgsl_device *device = dev_priv->device;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct adreno_device_private *adreno_priv =
		container_of(dev_priv, struct adreno_device_private,
		dev_priv);
	struct adreno_perfcounter_list_node *p, *tmp;

	mutex_lock(&device->mutex);
	list_for_each_entry_safe(p, tmp, &adreno_priv->perfcounter_list, node) {
		adreno_perfcounter_put(adreno_dev, p->groupid,
					p->countable, PERFCOUNTER_FLAG_NONE);
		list_del(&p->node);
		kfree(p);
	}
	mutex_unlock(&device->mutex);

	kfree(adreno_priv);
}

static inline s64 adreno_ticks_to_us(u32 ticks, u32 freq)
{
	freq /= 1000000;
	return ticks / freq;
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
	struct adreno_gpudev *gpudev  = ADRENO_GPU_DEVICE(adreno_dev);
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;
	struct adreno_busy_data *busy = &adreno_dev->busy_data;
	int64_t adj = 0;

	memset(stats, 0, sizeof(*stats));

	/* Get the busy cycles counted since the counter was last reset */
	if (adreno_dev->perfctr_pwr_lo != 0) {
		uint64_t gpu_busy;

		gpu_busy = counter_delta(device, adreno_dev->perfctr_pwr_lo,
			&busy->gpu_busy);

		if (gpudev->read_throttling_counters) {
			adj = gpudev->read_throttling_counters(adreno_dev);
			if (adj < 0 && -adj > gpu_busy)
				adj = 0;

			gpu_busy += adj;
		}

		if (adreno_is_a6xx(adreno_dev)) {
			/* clock sourced from XO */
			stats->busy_time = gpu_busy * 10;
			do_div(stats->busy_time, 192);
		} else {
			/* clock sourced from GFX3D */
			stats->busy_time = adreno_ticks_to_us(gpu_busy,
				kgsl_pwrctrl_active_freq(pwr));
		}
	}

	if (device->pwrctrl.bus_control) {
		uint64_t ram_cycles = 0, starved_ram = 0;

		if (adreno_dev->ram_cycles_lo != 0)
			ram_cycles = counter_delta(device,
				adreno_dev->ram_cycles_lo,
				&busy->bif_ram_cycles);

		if (adreno_has_gbif(adreno_dev)) {
			if (adreno_dev->ram_cycles_lo_ch1_read != 0)
				ram_cycles += counter_delta(device,
					adreno_dev->ram_cycles_lo_ch1_read,
					&busy->bif_ram_cycles_read_ch1);

			if (adreno_dev->ram_cycles_lo_ch0_write != 0)
				ram_cycles += counter_delta(device,
					adreno_dev->ram_cycles_lo_ch0_write,
					&busy->bif_ram_cycles_write_ch0);

			if (adreno_dev->ram_cycles_lo_ch1_write != 0)
				ram_cycles += counter_delta(device,
					adreno_dev->ram_cycles_lo_ch1_write,
					&busy->bif_ram_cycles_write_ch1);
		}

		if (adreno_dev->starved_ram_lo != 0)
			starved_ram = counter_delta(device,
				adreno_dev->starved_ram_lo,
				&busy->bif_starved_ram);

		if (adreno_has_gbif(adreno_dev)) {
			if (adreno_dev->starved_ram_lo_ch1 != 0)
				starved_ram += counter_delta(device,
					adreno_dev->starved_ram_lo_ch1,
					&busy->bif_starved_ram_ch1);
		}

		stats->ram_time = ram_cycles;
		stats->ram_wait = starved_ram;
	}

	if (adreno_dev->perfctr_ifpc_lo != 0) {
		uint32_t num_ifpc;

		num_ifpc = counter_delta(device, adreno_dev->perfctr_ifpc_lo,
				&busy->num_ifpc);
		adreno_dev->ifpc_count += num_ifpc;
		if (num_ifpc > 0)
			trace_adreno_ifpc_count(adreno_dev->ifpc_count);
	}

	if (adreno_dev->lm_threshold_count &&
			gpudev->count_throttles)
		gpudev->count_throttles(adreno_dev, adj);
}

static unsigned int adreno_gpuid(struct kgsl_device *device,
	unsigned int *chipid)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);

	/*
	 * Some applications need to know the chip ID too, so pass
	 * that as a parameter
	 */

	if (chipid != NULL)
		*chipid = adreno_dev->chipid;

	/*
	 * Standard KGSL gpuid format:
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

static void adreno_clk_set_options(struct kgsl_device *device, const char *name,
	struct clk *clk, bool on)
{
	if (ADRENO_GPU_DEVICE(ADRENO_DEVICE(device))->clk_set_options)
		ADRENO_GPU_DEVICE(ADRENO_DEVICE(device))->clk_set_options(
			ADRENO_DEVICE(device), name, clk, on);
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

static void adreno_gpu_model(struct kgsl_device *device, char *str,
				size_t bufsz)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);

	snprintf(str, bufsz, "Adreno%d%d%dv%d",
			ADRENO_CHIPID_CORE(adreno_dev->chipid),
			 ADRENO_CHIPID_MAJOR(adreno_dev->chipid),
			 ADRENO_CHIPID_MINOR(adreno_dev->chipid),
			 ADRENO_CHIPID_PATCH(adreno_dev->chipid) + 1);
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
	.queue_cmds = adreno_dispatcher_queue_cmds,
	.ioctl = adreno_ioctl,
	.compat_ioctl = adreno_compat_ioctl,
	.power_stats = adreno_power_stats,
	.gpuid = adreno_gpuid,
	.snapshot = adreno_snapshot,
	.irq_handler = adreno_irq_handler,
	.drain = adreno_drain,
	.device_private_create = adreno_device_private_create,
	.device_private_destroy = adreno_device_private_destroy,
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
	.clk_set_options = adreno_clk_set_options,
	.gpu_model = adreno_gpu_model,
	.stop_fault_timer = adreno_dispatcher_stop_fault_timer,
	.dispatcher_halt = adreno_dispatcher_halt,
	.dispatcher_unhalt = adreno_dispatcher_unhalt,
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

static const struct of_device_id busmon_match_table[] = {
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
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:kgsl_3d");
