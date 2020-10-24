// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2002,2007-2020, The Linux Foundation. All rights reserved.
 */
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_fdt.h>
#include <linux/module.h>
#include <linux/msm_kgsl.h>
#include <linux/msm-bus.h>
#include <linux/regulator/consumer.h>
#include <linux/nvmem-consumer.h>
#include <soc/qcom/scm.h>

#include "adreno.h"
#include "adreno_a3xx.h"
#include "adreno_a5xx.h"
#include "adreno_a6xx.h"
#include "adreno_compat.h"
#include "adreno_iommu.h"
#include "adreno_llc.h"
#include "adreno_trace.h"
#include "kgsl_trace.h"

/* Include the master list of GPU cores that are supported */
#include "adreno-gpulist.h"

static void adreno_input_work(struct work_struct *work);
static unsigned int counter_delta(struct kgsl_device *device,
	unsigned int reg, unsigned int *counter);

static struct devfreq_msm_adreno_tz_data adreno_tz_data = {
	.bus = {
		.max = 350,
		.floating = true,
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
		.ftbl = &adreno_functable,
	},
	.ft_policy = KGSL_FT_DEFAULT_POLICY,
	.ft_pf_policy = KGSL_FT_PAGEFAULT_DEFAULT_POLICY,
	.long_ib_detect = 1,
	.input_work = __WORK_INITIALIZER(device_3d0.input_work,
		adreno_input_work),
	.pwrctrl_flag = BIT(ADRENO_THROTTLING_CTRL) | BIT(ADRENO_HWCG_CTRL),
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
	ADRENO_REG_CP_IB2_BUFSZ,
};

/* Nice level for the higher priority GPU start thread */
int adreno_wake_nice = -7;

/* Number of milliseconds to stay active active after a wake on touch */
unsigned int adreno_wake_timeout = 100;

void adreno_reglist_write(struct adreno_device *adreno_dev,
		const struct adreno_reglist *list, u32 count)
{
	int i;

	for (i = 0; list && i < count; i++)
		kgsl_regwrite(KGSL_DEVICE(adreno_dev),
			list[i].offset, list[i].value);
}

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
static inline int adreno_of_read_property(struct device *dev,
		struct device_node *node, const char *prop, unsigned int *ptr)
{
	int ret = of_property_read_u32(node, prop, ptr);

	if (ret)
		dev_err(dev, "%pOF: Unable to read '%s'\n", node, prop);
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
			dev_err(KGSL_DEVICE(adreno_dev)->dev,
				     "Unable to allocate fault detect performance counter %d/%d\n",
				     group, countable);
			dev_err(KGSL_DEVICE(adreno_dev)->dev,
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

#define GMU_CM3_CFG_NONMASKINTR_SHIFT	9

/* Send an NMI to the GMU */
void adreno_gmu_send_nmi(struct adreno_device *adreno_dev)
{
	u32 val;
	/* Mask so there's no interrupt caused by NMI */
	adreno_write_gmureg(adreno_dev,
			ADRENO_REG_GMU_GMU2HOST_INTR_MASK, 0xFFFFFFFF);

	/* Make sure the interrupt is masked before causing it */
	wmb();
	if (ADRENO_QUIRK(adreno_dev, ADRENO_QUIRK_HFI_USE_REG))
		adreno_write_gmureg(adreno_dev,
				ADRENO_REG_GMU_NMI_CONTROL_STATUS, 0);

	adreno_read_gmureg(adreno_dev, ADRENO_REG_GMU_CM3_CFG, &val);
	val |= 1 << GMU_CM3_CFG_NONMASKINTR_SHIFT;
	adreno_write_gmureg(adreno_dev, ADRENO_REG_GMU_CM3_CFG, val);

	/* Make sure the NMI is invoked before we proceed*/
	wmb();
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

	adreno_writereg(adreno_dev, ADRENO_REG_RBBM_SW_RESET_CMD, 1);
	/*
	 * Do a dummy read to get a brief read cycle delay for the
	 * reset to take effect
	 */
	adreno_readreg(adreno_dev, ADRENO_REG_RBBM_SW_RESET_CMD, &reg);
	adreno_writereg(adreno_dev, ADRENO_REG_RBBM_SW_RESET_CMD, 0);

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
	dev_crit_ratelimited(KGSL_DEVICE(adreno_dev)->dev,
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
	u64 ts, ts1, ts2;

	ts = gmu_core_dev_read_ao_counter(device);

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
			ts1 =  gmu_core_dev_read_ao_counter(device);

			/* Wait for small time before trying again */
			udelay(1);
			adreno_readreg(adreno_dev,
					ADRENO_REG_GMU_AO_AHB_FENCE_CTRL,
					&fence);

			if (fence_retries == FENCE_RETRY_MAX && fence != 0) {
				ts2 =  gmu_core_dev_read_ao_counter(device);

				adreno_readreg(adreno_dev,
					ADRENO_REG_GMU_RBBM_INT_UNMASKED_STATUS,
					&shadow_status);

				dev_crit_ratelimited(device->dev,
					"Status=0x%x Unmasked status=0x%x Timestamps:%llx %llx %llx\n",
					shadow_status & irq_params->mask,
					shadow_status, ts, ts1, ts2);

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
			dev_crit_ratelimited(device->dev,
						"Unhandled interrupt bit %x\n",
						i);

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
		if (core == adreno_gpulist[i]->core &&
		    _rev_match(major, adreno_gpulist[i]->major) &&
		    _rev_match(minor, adreno_gpulist[i]->minor) &&
		    _rev_match(patchid, adreno_gpulist[i]->patchid))
			return adreno_gpulist[i];
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

	dev_warn(KGSL_DEVICE(adreno_dev)->dev,
		      "No matching SOC HW revision found for efused HW rev=%u\n",
		      adreno_dev->soc_hw_rev);
	return NULL;
}

static void adreno_update_soc_hw_revision_quirks(
		struct adreno_device *adreno_dev, struct platform_device *pdev)
{
	struct device_node *node;
	int i;

	node = adreno_get_soc_hw_revision_node(adreno_dev, pdev);
	if (node == NULL)
		node = pdev->dev.of_node;

	/* get chip id, fall back to parent if revision node does not have it */
	if (of_property_read_u32(node, "qcom,chipid", &adreno_dev->chipid)) {
		if (of_property_read_u32(pdev->dev.of_node,
				"qcom,chipid", &adreno_dev->chipid)) {
			dev_crit(KGSL_DEVICE(adreno_dev)->dev,
				"No GPU chip ID was specified\n");
			BUG();
			return;
		}
	}

	/* update quirk */
	for (i = 0; i < ARRAY_SIZE(adreno_quirks); i++) {
		if (of_property_read_bool(node, adreno_quirks[i].prop))
			adreno_dev->quirks |= adreno_quirks[i].quirk;
	}
}

static int adreno_identify_gpu(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	const struct adreno_reg_offsets *reg_offsets;
	struct adreno_gpudev *gpudev;
	int i;

	adreno_dev->gpucore = _get_gpu_core(adreno_dev->chipid);

	if (adreno_dev->gpucore == NULL) {
		dev_crit(&device->pdev->dev,
			"Unknown GPU chip ID %8.8X\n", adreno_dev->chipid);
		return -ENODEV;
	}

	/*
	 * Identify non-longer supported targets and spins and print a helpful
	 * message
	 */
	if (adreno_dev->gpucore->features & ADRENO_DEPRECATED) {
		dev_err(&device->pdev->dev,
			"Support for GPU %d.%d.%d.%d has been deprecated\n",
			adreno_dev->gpucore->core,
			adreno_dev->gpucore->major,
			adreno_dev->gpucore->minor,
			adreno_dev->gpucore->patchid);
		return -ENODEV;
	}

	/*
	 * Some GPUs needs UCHE GMEM base address to be minimum 0x100000
	 * and 1MB aligned. Configure UCHE GMEM base based on GMEM size
	 * and align it one 1MB. This needs to be done based on GMEM size
	 * because setting it to minimum value 0x100000 will result in RB
	 * and UCHE GMEM range overlap for GPUs with GMEM size >1MB.
	 */
	if (!adreno_is_a650_family(adreno_dev))
		adreno_dev->uche_gmem_base =
			ALIGN(adreno_dev->gpucore->gmem_size, SZ_1M);

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

	return 0;
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
			dev_err(device->dev,
				"Unable to set the GPU OPP table: %d\n", ret);
			return ret;
		}
	}

	pwr->num_pwrlevels = 0;

	for_each_child_of_node(node, child) {
		unsigned int index;
		struct kgsl_pwrlevel *level;

		if (adreno_of_read_property(device->dev, child, "reg", &index))
			return -EINVAL;

		if (index >= KGSL_MAX_PWRLEVELS) {
			dev_err(device->dev,
				"%pOF: Pwrlevel index %d is out of range\n",
					child, index);
			continue;
		}

		if (index >= pwr->num_pwrlevels)
			pwr->num_pwrlevels = index + 1;

		level = &pwr->pwrlevels[index];

		if (adreno_of_read_property(device->dev, child, "qcom,gpu-freq",
			&level->gpu_freq))
			return -EINVAL;

		of_property_read_u32(child, "qcom,acd-level",
			&level->acd_level);

		ret = kgsl_of_property_read_ddrtype(child,
			"qcom,bus-freq", &level->bus_freq);
		if (ret) {
			dev_err(device->dev,
				"%pOF: Couldn't read the bus frequency for power level %d\n",
				child, index);
			return ret;
		}

		level->bus_min = level->bus_freq;
		kgsl_of_property_read_ddrtype(child,
			"qcom,bus-min", &level->bus_min);

		level->bus_max = level->bus_freq;
		kgsl_of_property_read_ddrtype(child,
			"qcom,bus-max", &level->bus_max);
	}

	return 0;
}

static void adreno_of_get_bimc_iface_clk(struct adreno_device *adreno_dev,
		struct device_node *node)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;

	/* Getting gfx-bimc-interface-clk frequency */
	if (!of_property_read_u32(node, "qcom,gpu-bimc-interface-clk-freq",
				&pwr->gpu_bimc_int_clk_freq)) {
		pwr->gpu_bimc_int_clk = devm_clk_get(&device->pdev->dev,
				"bimc_gpu_clk");
		if (IS_ERR_OR_NULL(pwr->gpu_bimc_int_clk)) {
			dev_err(&device->pdev->dev,
					"dt: Couldn't get bimc_gpu_clk (%d)\n",
					PTR_ERR(pwr->gpu_bimc_int_clk));
			pwr->gpu_bimc_int_clk = NULL;
		}
	}
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

static void adreno_of_get_limits(struct adreno_device *adreno_dev,
		struct device_node *node)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct kgsl_pwrctrl *pwrctrl = &device->pwrctrl;
	unsigned int throttle_level;

	if (!ADRENO_FEATURE(adreno_dev, ADRENO_LM) || of_property_read_u32(node,
				"qcom,throttle-pwrlevel", &throttle_level))
		return;

	throttle_level = min(throttle_level, pwrctrl->num_pwrlevels - 1);

	pwrctrl->throttle_mask = GENMASK(pwrctrl->num_pwrlevels - 1,
			pwrctrl->num_pwrlevels - 1 - throttle_level);

	set_bit(ADRENO_LM_CTRL, &adreno_dev->pwrctrl_flag);
}

static int adreno_of_get_legacy_pwrlevels(struct adreno_device *adreno_dev,
		struct device_node *parent)
{
	struct device_node *node;
	int ret;

	node = of_find_node_by_name(parent, "qcom,gpu-pwrlevels");

	if (node == NULL) {
		dev_err(KGSL_DEVICE(adreno_dev)->dev,
			"Unable to find 'qcom,gpu-pwrlevels'\n");
		return -EINVAL;
	}

	ret = adreno_of_parse_pwrlevels(adreno_dev, node);
	if (ret)
		return ret;

	adreno_of_get_initial_pwrlevel(adreno_dev, parent);

	adreno_of_get_limits(adreno_dev, parent);

	adreno_of_get_bimc_iface_clk(adreno_dev, parent);

	return 0;
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
			if (ret)
				return ret;

			adreno_of_get_initial_pwrlevel(adreno_dev, child);

			adreno_of_get_bimc_iface_clk(adreno_dev, child);

			/*
			 * Check for global throttle-pwrlevel first and override
			 * with speedbin specific one if found.
			 */
			adreno_of_get_limits(adreno_dev, parent);
			adreno_of_get_limits(adreno_dev, child);

			return 0;
		}
	}

	dev_err(KGSL_DEVICE(adreno_dev)->dev,
		"GPU speed_bin:%d mismatch for efused bin:%d\n",
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

static int adreno_of_get_power(struct adreno_device *adreno_dev,
		struct platform_device *pdev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct device_node *node = pdev->dev.of_node;
	struct resource *res;
	unsigned int timeout;

	if (of_property_read_string(node, "label", &pdev->name)) {
		dev_err(device->dev, "Unable to read 'label'\n");
		return -EINVAL;
	}

	if (adreno_of_read_property(device->dev, node, "qcom,id", &pdev->id))
		return -EINVAL;

	/* Get starting physical address of device registers */
	res = platform_get_resource_byname(device->pdev, IORESOURCE_MEM,
					   device->iomemname);
	if (res == NULL) {
		dev_err(device->dev,
			     "platform_get_resource_byname failed\n");
		return -EINVAL;
	}
	if (res->start == 0 || resource_size(res) == 0) {
		dev_err(device->dev, "dev %d invalid register region\n",
			     device->id);
		return -EINVAL;
	}

	device->reg_phys = res->start;
	device->reg_len = resource_size(res);

	if (adreno_of_get_pwrlevels(adreno_dev, node))
		return -EINVAL;

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
		dev_warn(device->dev, "cx_dbgc ioremap failed\n");
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

static void adreno_rscc_probe(struct kgsl_device *device)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct resource *res;

	res = platform_get_resource_byname(device->pdev, IORESOURCE_MEM,
						"rscc");

	if (res == NULL)
		return;

	adreno_dev->rscc_base = res->start - device->reg_phys;
	adreno_dev->rscc_len = resource_size(res);
	adreno_dev->rscc_virt = devm_ioremap(device->dev, res->start,
						adreno_dev->rscc_len);
	if (adreno_dev->rscc_virt == NULL)
		dev_warn(device->dev, "rscc ioremap failed\n");
}

static void adreno_isense_probe(struct kgsl_device *device)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct resource *res;

	res = platform_get_resource_byname(device->pdev, IORESOURCE_MEM,
			"isense_cntl");
	if (res == NULL)
		return;

	adreno_dev->isense_base = res->start - device->reg_phys;
	adreno_dev->isense_len = resource_size(res);
	adreno_dev->isense_virt = devm_ioremap(device->dev, res->start,
					adreno_dev->isense_len);
	if (adreno_dev->isense_virt == NULL)
		dev_warn(device->dev, "isense ioremap failed\n");
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
		dev_err(KGSL_DEVICE(adreno_dev)->dev,
			"Unable to map hardware revision fuse: ret=%d\n", ret);
		return;
	}

	ret = adreno_efuse_read_u32(adreno_dev, soc_hw_rev[0], &val);
	adreno_efuse_unmap(adreno_dev);

	if (ret) {
		dev_err(KGSL_DEVICE(adreno_dev)->dev,
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

static int adreno_read_speed_bin(struct platform_device *pdev,
		struct adreno_device *adreno_dev)
{
	struct nvmem_cell *cell = nvmem_cell_get(&pdev->dev, "speed_bin");
	int ret = PTR_ERR_OR_ZERO(cell);
	void *buf;
	size_t len;

	if (ret) {
		/*
		 * If the cell isn't defined enabled, then revert to
		 * using the default bin
		 */
		if (ret == -ENOENT)
			return 0;

		return ret;
	}

	buf = nvmem_cell_read(cell, &len);
	nvmem_cell_put(cell);

	if (!IS_ERR(buf)) {
		memcpy(&adreno_dev->speed_bin, buf,
				min(len, sizeof(adreno_dev->speed_bin)));
		kfree(buf);
	}

	return PTR_ERR_OR_ZERO(buf);
}

static int adreno_probe_efuse(struct platform_device *pdev,
					struct adreno_device *adreno_dev)
{
	int ret;

	ret = adreno_read_speed_bin(pdev, adreno_dev);
	if (ret)
		return ret;

	ret = nvmem_cell_read_u32(&pdev->dev, "isense_slope",
					&adreno_dev->lm_slope);
	if (ret && ret != -ENOENT)
		return ret;

	return 0;
}

static int adreno_probe(struct platform_device *pdev)
{
	const struct of_device_id *of_id;
	struct adreno_device *adreno_dev;
	struct kgsl_device *device;
	int status;
	unsigned int priv;

	of_id = of_match_device(adreno_match_table, &pdev->dev);
	if (!of_id)
		return -EINVAL;

	adreno_dev = (struct adreno_device *) of_id->data;
	device = KGSL_DEVICE(adreno_dev);

	device->pdev = pdev;

	if (adreno_is_gpu_disabled(adreno_dev)) {
		dev_err(&pdev->dev, "adreno: GPU is disabled on this device\n");
		return -ENODEV;
	}

	/* Identify SOC hardware revision to be used */
	adreno_efuse_read_soc_hw_rev(adreno_dev);

	adreno_update_soc_hw_revision_quirks(adreno_dev, pdev);

	status = adreno_probe_efuse(pdev, adreno_dev);
	if (status)
		return status;

	/* Get the chip ID from the DT and set up target specific parameters */
	if (adreno_identify_gpu(adreno_dev))
		return -ENODEV;

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

	if (adreno_is_a6xx(adreno_dev))
		device->mmu.features |= KGSL_MMU_SMMU_APERTURE;

	if (ADRENO_FEATURE(adreno_dev, ADRENO_USE_SHMEM))
		device->flags |= KGSL_FLAG_USE_SHMEM;

	if (ADRENO_FEATURE(adreno_dev, ADRENO_PROCESS_RECLAIM)) {
		device->flags |= KGSL_FLAG_USE_SHMEM;
		device->flags |= KGSL_FLAG_PROCESS_RECLAIM;
	}

	device->pwrctrl.bus_width = adreno_dev->gpucore->bus_width;

	status = kgsl_device_platform_probe(device);
	if (status) {
		device->pdev = NULL;
		return status;
	}

	/* Probe for the optional CX_DBGC block */
	adreno_cx_dbgc_probe(device);

	/* Probe for the optional CX_MISC block */
	adreno_cx_misc_probe(device);

	adreno_rscc_probe(device);

	adreno_isense_probe(device);
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

	/* Allocate the memstore for storing timestamps and other useful info */
	priv = KGSL_MEMDESC_CONTIG;

	if (ADRENO_FEATURE(adreno_dev, ADRENO_APRIV))
		priv |= KGSL_MEMDESC_PRIVILEGED;

	status = kgsl_allocate_global(device, &device->memstore,
		KGSL_MEMSTORE_SIZE, 0, priv, "memstore");

	if (status)
		goto out;

	status = adreno_ringbuffer_probe(adreno_dev);
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
	adreno_dev->gpu_llc_slice = adreno_llc_getd(LLCC_GPU);
	if (IS_ERR(adreno_dev->gpu_llc_slice) &&
			PTR_ERR(adreno_dev->gpu_llc_slice) != -ENOENT)
		dev_warn(device->dev,
			"Failed to get GPU LLC slice descriptor %ld\n",
			PTR_ERR(adreno_dev->gpu_llc_slice));

	/* Get the system cache slice descriptor for GPU pagetables */
	adreno_dev->gpuhtw_llc_slice = adreno_llc_getd(LLCC_GPUHTW);
	if (IS_ERR(adreno_dev->gpuhtw_llc_slice) &&
			PTR_ERR(adreno_dev->gpuhtw_llc_slice) != -ENOENT)
		dev_warn(device->dev,
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
			dev_err(device->dev,
				     "Unable to register the input handler\n");
		}
	}
#endif
out:
	if (status) {
		adreno_ringbuffer_close(adreno_dev);
		kgsl_free_global(device, &device->memstore);
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
	const struct of_device_id *of_id;
	struct adreno_device *adreno_dev;
	struct kgsl_device *device;
	struct adreno_gpudev *gpudev;

	of_id = of_match_device(adreno_match_table, &pdev->dev);
	if (!of_id)
		return -EINVAL;

	adreno_dev = (struct adreno_device *) of_id->data;
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

	kgsl_free_global(device, &device->memstore);

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

	if (!ADRENO_FEATURE(adreno_dev, ADRENO_SOFT_FAULT_DETECT))
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

static void do_gbif_halt(struct adreno_device *adreno_dev,
	u32 halt_reg, u32 ack_reg, u32 mask, const char *client)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	unsigned long t;
	u32 val;

	adreno_writereg(adreno_dev, halt_reg, mask);

	t = jiffies + msecs_to_jiffies(VBIF_RESET_ACK_TIMEOUT);
	do {
		adreno_readreg(adreno_dev, ack_reg, &val);
		if ((val & mask) == mask)
			return;

		/*
		 * If we are attempting GBIF halt in case of stall-on-fault
		 * then the halt sequence will not complete as long as SMMU
		 * is stalled.
		 */
		kgsl_mmu_pagefault_resume(&device->mmu);
		usleep_range(10, 100);
	} while (!time_after(jiffies, t));

	/* Check one last time */
	kgsl_mmu_pagefault_resume(&device->mmu);

	adreno_readreg(adreno_dev, ack_reg, &val);
	if ((val & mask) == mask)
		return;

	dev_err(device->dev, "%s GBIF Halt ack timed out\n", client);
}

/**
 * adreno_smmu_resume - Clears stalled/pending transactions in GBIF pipe
 * and resumes stalled SMMU
 * @adreno_dev: Pointer to the the adreno device
 */
void adreno_smmu_resume(struct adreno_device *adreno_dev)
{
	struct adreno_gpudev *gpudev = ADRENO_GPU_DEVICE(adreno_dev);

	/* Halt GBIF GX traffic */
	if (gmu_core_dev_gx_is_on(KGSL_DEVICE(adreno_dev)))
		do_gbif_halt(adreno_dev, ADRENO_REG_RBBM_GBIF_HALT,
			ADRENO_REG_RBBM_GBIF_HALT_ACK,
			gpudev->gbif_gx_halt_mask, "GX");

	/* Halt all CX traffic */
	do_gbif_halt(adreno_dev, ADRENO_REG_GBIF_HALT,
		ADRENO_REG_GBIF_HALT_ACK, gpudev->gbif_arb_halt_mask, "CX");
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

		/* This is taken care by GMU firmware if GMU is enabled */
		if (!gmu_core_gpmu_isenabled(device)) {
			/* Halt GBIF GX traffic and poll for halt ack */
			if (adreno_is_a615_family(adreno_dev)) {
				adreno_writereg(adreno_dev,
					ADRENO_REG_RBBM_GPR0_CNTL,
					GBIF_HALT_REQUEST);
				ret = adreno_wait_for_halt_ack(device,
					A6XX_RBBM_VBIF_GX_RESET_STATUS,
					VBIF_RESET_ACK_MASK);
			} else {
				adreno_writereg(adreno_dev,
					ADRENO_REG_RBBM_GBIF_HALT,
					gpudev->gbif_gx_halt_mask);
				ret = adreno_wait_for_halt_ack(device,
					ADRENO_REG_RBBM_GBIF_HALT_ACK,
					gpudev->gbif_gx_halt_mask);
			}
			if (ret)
				return ret;
		}

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

	ret = gmu_core_init(device);
	if (ret)
		return ret;

	/* Put the GPU in a responsive state */
	if (ADRENO_GPUREV(adreno_dev) < 600) {
		/* No need for newer generation architectures */
		ret = kgsl_pwrctrl_change_state(device, KGSL_STATE_AWARE);
		if (ret)
			return ret;
	}

	 adreno_iommu_init(adreno_dev);

	adreno_perfcounter_init(adreno_dev);
	adreno_fault_detect_init(adreno_dev);

	adreno_dev->cooperative_reset = ADRENO_FEATURE(adreno_dev,
							ADRENO_COOP_RESET);

	/* Power down the device */
	if (ADRENO_GPUREV(adreno_dev) < 600)
		kgsl_pwrctrl_change_state(device, KGSL_STATE_SLUMBER);

	if (gpudev->init != NULL)
		gpudev->init(adreno_dev);

	set_bit(ADRENO_DEVICE_INITIALIZED, &adreno_dev->priv);

	/*
	 * Allocate a small chunk of memory for precise drawobj profiling for
	 * those targets that have the always on timer
	 */

	if (!adreno_is_a3xx(adreno_dev)) {
		unsigned int priv = 0;
		int r;

		if (ADRENO_FEATURE(adreno_dev, ADRENO_APRIV))
			priv |= KGSL_MEMDESC_PRIVILEGED;

		r = kgsl_allocate_global(device,
			&adreno_dev->profile_buffer, PAGE_SIZE,
			0, priv, "alwayson");

		adreno_dev->profile_index = 0;

		if (r == 0) {
			set_bit(ADRENO_DEVICE_DRAWOBJ_PROFILE,
				&adreno_dev->priv);
			kgsl_sharedmem_set(device,
				&adreno_dev->profile_buffer, 0, 0,
				PAGE_SIZE);
		}

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

	/*
	 * Keep high bus vote to reduce AHB latency
	 * during FW loading and wakeup.
	 */
	if (device->pwrctrl.gpu_cfg)
		msm_bus_scale_client_update_request(device->pwrctrl.gpu_cfg,
			KGSL_GPU_CFG_PATH_HIGH);

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

	/*
	 * During adreno_stop, GBIF halt is asserted to ensure
	 * no further transaction can go through GPU before GPU
	 * headswitch is turned off.
	 *
	 * This halt is deasserted once headswitch goes off but
	 * incase headswitch doesn't goes off clear GBIF halt
	 * here to ensure GPU wake-up doesn't fail because of
	 * halted GPU transactions.
	 */
	adreno_deassert_gbif_halt(adreno_dev);

	/*
	 * Observed race between timeout fault (long IB detection) and
	 * MISC hang (hard fault). MISC hang can be set while in recovery from
	 * timeout fault. If fault flag is set in start path CP init fails.
	 * Clear gpu fault to avoid such race.
	 */
	adreno_clear_gpu_fault(adreno_dev);

	adreno_ringbuffer_set_global(adreno_dev, 0);

	status = kgsl_mmu_start(device);
	if (status)
		goto error_boot_oob_clear;

	/* Send OOB request to turn on the GX */
	status = gmu_core_dev_oob_set(device, oob_gpu);
	if (status) {
		gmu_core_snapshot(device);
		goto error_mmu_off;
	}

	status = gmu_core_dev_hfi_start_msg(device);
	if (status) {
		gmu_core_snapshot(device);
		goto error_oob_clear;
	}

	_set_secvid(device);

	/* Enable 64 bit gpu addr if feature is set */
	if (gpudev->enable_64bit &&
			adreno_support_64bit(adreno_dev))
		gpudev->enable_64bit(adreno_dev);

	if (device->pwrctrl.bus_control) {
		/* VBIF waiting for RAM */
		if (adreno_dev->starved_ram_lo == 0) {
			ret = adreno_perfcounter_get(adreno_dev,
				KGSL_PERFCOUNTER_GROUP_VBIF_PWR, 0,
				&adreno_dev->starved_ram_lo, NULL,
				PERFCOUNTER_FLAG_KERNEL);

			if (ret) {
				dev_err(device->dev,
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
					dev_err(device->dev,
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
					dev_err(device->dev,
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
					dev_err(device->dev,
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
					dev_err(device->dev,
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
					dev_err(device->dev,
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
					dev_err(device->dev,
						     "Unable to get perf counters for bus DCVS\n");
					adreno_dev->ram_cycles_lo = 0;
				}
			}
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

	status = adreno_ringbuffer_start(adreno_dev);
	if (status)
		goto error_oob_clear;

	/*
	 * At this point it is safe to assume that we recovered. Setting
	 * this field allows us to take a new snapshot for the next failure
	 * if we are prioritizing the first unrecoverable snapshot.
	 */
	if (device->snapshot)
		device->snapshot->recovered = true;

	/* Start the dispatcher */
	adreno_dispatcher_start(device);

	device->reset_counter++;

	set_bit(ADRENO_DEVICE_STARTED, &adreno_dev->priv);

	if (pmqos_active_vote != pmqos_wakeup_vote)
		pm_qos_update_request(&device->pwrctrl.pm_qos_req_dma,
				pmqos_active_vote);

	/* Send OOB request to allow IFPC */
	gmu_core_dev_oob_clear(device, oob_gpu);

	/* If we made it this far, the BOOT OOB was sent to the GMU */
	if (ADRENO_QUIRK(adreno_dev, ADRENO_QUIRK_HFI_USE_REG))
		gmu_core_dev_oob_clear(device, oob_boot_slumber);

	/*
	 * Low vote is enough after wakeup completes, this will make
	 * sure CPU to GPU AHB infrastructure clocks are running at-least
	 * at minimum frequency.
	 */
	if (device->pwrctrl.gpu_cfg)
		msm_bus_scale_client_update_request(device->pwrctrl.gpu_cfg,
			KGSL_GPU_CFG_PATH_LOW);

	return 0;

error_oob_clear:
	gmu_core_dev_oob_clear(device, oob_gpu);

error_mmu_off:
	kgsl_mmu_stop(&device->mmu);

error_boot_oob_clear:
	if (ADRENO_QUIRK(adreno_dev, ADRENO_QUIRK_HFI_USE_REG))
		gmu_core_dev_oob_clear(device, oob_boot_slumber);

error_pwr_off:
	/* set the state back to original state */
	kgsl_pwrctrl_change_state(device, state);

	if (pmqos_active_vote != pmqos_wakeup_vote)
		pm_qos_update_request(&device->pwrctrl.pm_qos_req_dma,
				pmqos_active_vote);

	if (device->pwrctrl.gpu_cfg)
		msm_bus_scale_client_update_request(device->pwrctrl.gpu_cfg,
			KGSL_GPU_CFG_PATH_OFF);
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
	int error = 0;

	if (!test_bit(ADRENO_DEVICE_STARTED, &adreno_dev->priv))
		return 0;

	error = gmu_core_dev_oob_set(device, oob_gpu);
	if (error) {
		gmu_core_dev_oob_clear(device, oob_gpu);
			gmu_core_snapshot(device);
			error = -EINVAL;
			goto no_gx_power;
	}

	kgsl_pwrscale_update_stats(device);

	adreno_irqctrl(adreno_dev, 0);

	/* Save active coresight registers if applicable */
	adreno_coresight_stop(adreno_dev);

	/* Save physical performance counter values before GPU power down*/
	adreno_perfcounter_save(adreno_dev);

	gmu_core_dev_prepare_stop(device);
	gmu_core_dev_oob_clear(device, oob_gpu);

	/*
	 * Saving perfcounters will use an OOB to put the GMU into
	 * active state. Before continuing, we should wait for the
	 * GMU to return to the lowest idle level. This is
	 * because some idle level transitions require VBIF and MMU.
	 */

	if (!error && gmu_core_dev_wait_for_lowest_idle(device)) {
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

no_gx_power:
	adreno_dispatcher_stop(adreno_dev);

	adreno_ringbuffer_stop(adreno_dev);

	if (!IS_ERR_OR_NULL(adreno_dev->gpu_llc_slice))
		llcc_slice_deactivate(adreno_dev->gpu_llc_slice);

	if (!IS_ERR_OR_NULL(adreno_dev->gpuhtw_llc_slice))
		llcc_slice_deactivate(adreno_dev->gpuhtw_llc_slice);

	/*
	 * The halt is not cleared in the above function if we have GBIF.
	 * Clear it here if GMU is enabled as GMU stop needs access to
	 * system memory to stop. For non-GMU targets, we don't need to
	 * clear it as it will get cleared automatically once headswitch
	 * goes OFF immediately after adreno_stop.
	 */
	if (gmu_core_gpmu_isenabled(device))
		adreno_deassert_gbif_halt(adreno_dev);

	kgsl_mmu_stop(&device->mmu);

	/*
	 * At this point, MMU is turned off so we can safely
	 * destroy any pending contexts and their pagetables
	 */
	adreno_set_active_ctxs_null(adreno_dev);

	if (device->pwrctrl.gpu_cfg)
		msm_bus_scale_client_update_request(device->pwrctrl.gpu_cfg,
			KGSL_GPU_CFG_PATH_OFF);

	clear_bit(ADRENO_DEVICE_STARTED, &adreno_dev->priv);

	return error;
}

static inline bool adreno_try_soft_reset(struct kgsl_device *device, int fault)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);

	/*
	 * Do not do soft reset for a IOMMU fault (because the IOMMU hardware
	 * needs a reset too) and also for below gpu
	 * A304: It can't do SMMU programming of any kind after a soft reset
	 * A612: IPC protocol between RGMU and CP will not restart after reset
	 * A610: An across chip issue with reset line in all 11nm chips,
	 * resulting in recommendation to not use soft reset
	 */

	if ((fault & ADRENO_IOMMU_PAGE_FAULT) || adreno_is_a304(adreno_dev) ||
			adreno_is_a612(adreno_dev) ||
			adreno_is_a610(adreno_dev))
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
				dev_err(device->dev,
					"Device soft reset failed: ret=%d\n",
					ret);
		}
	}
	if (ret) {
		unsigned long flags = device->pwrctrl.ctrl_flags;

		/*
		 * Clear ctrl_flags to ensure clocks and regulators are
		 * turned off
		 */
		device->pwrctrl.ctrl_flags = 0;

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

		device->pwrctrl.ctrl_flags = flags;
	}
	if (ret)
		return ret;

	if (i != 0)
		dev_warn(device->dev,
			      "Device hard reset tried %d tries\n", i);

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

static int copy_prop(struct kgsl_device_getproperty *param,
		void *src, size_t size)
{
	if (copy_to_user(param->value, src,
		min_t(u32, size, param->sizebytes)))
		return -EFAULT;

	return 0;
}

static int adreno_prop_device_info(struct kgsl_device *device,
		struct kgsl_device_getproperty *param)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct kgsl_devinfo devinfo = {
		.device_id = device->id + 1,
		.chip_id = adreno_dev->chipid,
		.mmu_enabled = MMU_FEATURE(&device->mmu, KGSL_MMU_PAGED),
		.gmem_gpubaseaddr = 0,
		.gmem_sizebytes = adreno_dev->gpucore->gmem_size,
	};

	return copy_prop(param, &devinfo, sizeof(devinfo));
}

static int adreno_prop_device_shadow(struct kgsl_device *device,
		struct kgsl_device_getproperty *param)
{
	struct kgsl_shadowprop shadowprop = { 0 };

	if (device->memstore.hostptr) {
		/*
		 * NOTE: with mmu enabled, gpuaddr doesn't mean
		 * anything to mmap().
		 */

		shadowprop.gpuaddr =  (unsigned long)device->memstore.gpuaddr;
		shadowprop.size = device->memstore.size;

		shadowprop.flags = KGSL_FLAGS_INITIALIZED |
			KGSL_FLAGS_PER_CONTEXT_TIMESTAMPS;
	}

	return copy_prop(param, &shadowprop, sizeof(shadowprop));
}

static int adreno_prop_device_qdss_stm(struct kgsl_device *device,
		struct kgsl_device_getproperty *param)
{
	struct kgsl_qdss_stm_prop qdssprop = {0};
	struct kgsl_memdesc *qdss_desc = kgsl_mmu_get_qdss_global_entry(device);

	if (qdss_desc) {
		qdssprop.gpuaddr = qdss_desc->gpuaddr;
		qdssprop.size = qdss_desc->size;
	}

	return copy_prop(param, &qdssprop, sizeof(qdssprop));
}

static int adreno_prop_device_qtimer(struct kgsl_device *device,
		struct kgsl_device_getproperty *param)
{
	struct kgsl_qtimer_prop qtimerprop = {0};
	struct kgsl_memdesc *qtimer_desc =
		kgsl_mmu_get_qtimer_global_entry(device);

	if (qtimer_desc) {
		qtimerprop.gpuaddr = qtimer_desc->gpuaddr;
		qtimerprop.size = qtimer_desc->size;
	}

	return copy_prop(param, &qtimerprop, sizeof(qtimerprop));
}

static int adreno_prop_s32(struct kgsl_device *device,
		struct kgsl_device_getproperty *param)
{
	int val = 0;

	if (param->type == KGSL_PROP_MMU_ENABLE)
		val = MMU_FEATURE(&device->mmu, KGSL_MMU_PAGED);
	else if (param->type == KGSL_PROP_INTERRUPT_WAITS)
		val = 1;

	return copy_prop(param, &val, sizeof(val));
}

static int adreno_prop_uche_gmem_addr(struct kgsl_device *device,
		struct kgsl_device_getproperty *param)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);

	return copy_prop(param, &adreno_dev->uche_gmem_base,
		sizeof(adreno_dev->uche_gmem_base));
}

static int adreno_prop_ucode_version(struct kgsl_device *device,
		struct kgsl_device_getproperty *param)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct kgsl_ucode_version ucode = {
		.pfp = adreno_dev->fw[ADRENO_FW_PFP].version,
		.pm4 = adreno_dev->fw[ADRENO_FW_PM4].version,
	};

	return copy_prop(param, &ucode, sizeof(ucode));
}

static int adreno_prop_gaming_bin(struct kgsl_device *device,
		struct kgsl_device_getproperty *param)
{
	void *buf;
	size_t len;
	int ret;
	struct nvmem_cell *cell;

	cell = nvmem_cell_get(&device->pdev->dev, "gaming_bin");
	if (IS_ERR(cell))
		return -EINVAL;

	buf = nvmem_cell_read(cell, &len);
	nvmem_cell_put(cell);

	if (!IS_ERR(buf)) {
		ret = copy_prop(param, buf, len);
		kfree(buf);
		return ret;
	}

	dev_err(device->dev, "failed to read gaming_bin nvmem cell\n");
	return -EINVAL;
}

static int adreno_prop_u32(struct kgsl_device *device,
		struct kgsl_device_getproperty *param)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	u32 val = 0;

	if (param->type == KGSL_PROP_HIGHEST_BANK_BIT) {
		val = adreno_dev->highest_bank_bit;
	} else if (param->type == KGSL_PROP_MIN_ACCESS_LENGTH)
		of_property_read_u32(device->pdev->dev.of_node,
			"qcom,min-access-length", &val);
	else if (param->type == KGSL_PROP_UBWC_MODE)
		of_property_read_u32(device->pdev->dev.of_node,
			"qcom,ubwc-mode", &val);
	else if (param->type == KGSL_PROP_DEVICE_BITNESS)
		val = adreno_support_64bit(adreno_dev) ? 48 : 32;
	else if (param->type == KGSL_PROP_SPEED_BIN)
		val = adreno_dev->speed_bin;

	return copy_prop(param, &val, sizeof(val));
}

static const struct {
	int type;
	int (*func)(struct kgsl_device *device,
		struct kgsl_device_getproperty *param);
} adreno_property_funcs[] = {
	{ KGSL_PROP_DEVICE_INFO, adreno_prop_device_info },
	{ KGSL_PROP_DEVICE_SHADOW, adreno_prop_device_shadow },
	{ KGSL_PROP_DEVICE_QDSS_STM, adreno_prop_device_qdss_stm },
	{ KGSL_PROP_DEVICE_QTIMER, adreno_prop_device_qtimer },
	{ KGSL_PROP_MMU_ENABLE, adreno_prop_s32 },
	{ KGSL_PROP_INTERRUPT_WAITS, adreno_prop_s32 },
	{ KGSL_PROP_UCHE_GMEM_VADDR, adreno_prop_uche_gmem_addr },
	{ KGSL_PROP_UCODE_VERSION, adreno_prop_ucode_version },
	{ KGSL_PROP_HIGHEST_BANK_BIT, adreno_prop_u32 },
	{ KGSL_PROP_MIN_ACCESS_LENGTH, adreno_prop_u32 },
	{ KGSL_PROP_UBWC_MODE, adreno_prop_u32 },
	{ KGSL_PROP_DEVICE_BITNESS, adreno_prop_u32 },
	{ KGSL_PROP_SPEED_BIN, adreno_prop_u32 },
	{ KGSL_PROP_GAMING_BIN, adreno_prop_gaming_bin },
};

static int adreno_getproperty(struct kgsl_device *device,
		struct kgsl_device_getproperty *param)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(adreno_property_funcs); i++) {
		if (param->type == adreno_property_funcs[i].type)
			return adreno_property_funcs[i].func(device, param);
	}

	return -ENODEV;
}

static int adreno_query_property_list(struct kgsl_device *device, u32 *list,
		u32 count)
{
	int i;

	if (!list)
		return ARRAY_SIZE(adreno_property_funcs);

	for (i = 0; i < count && i < ARRAY_SIZE(adreno_property_funcs); i++)
		list[i] = adreno_property_funcs[i].type;

	return i;
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
		status = -ENODEV;
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
	int ret;

	ret = gmu_core_dev_oob_set(device, oob_gpu);
	if (ret)
		return ret;

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
		gmu_core_dev_oob_clear(device, oob_gpu);
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

	ret = adreno_ringbuffer_start(adreno_dev);
	if (ret == 0) {
		device->reset_counter++;
		set_bit(ADRENO_DEVICE_STARTED, &adreno_dev->priv);
	}

	/* Restore physical performance counter values after soft reset */
	adreno_perfcounter_restore(adreno_dev);

	gmu_core_dev_oob_clear(device, oob_gpu);

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
	unsigned int hwfault, cx_status;

	dev_err(device->dev, str);

	adreno_readreg(adreno_dev, ADRENO_REG_CP_RB_RPTR, &rptr);
	adreno_readreg(adreno_dev, ADRENO_REG_CP_RB_WPTR, &wptr);

	adreno_readreg(adreno_dev, ADRENO_REG_RBBM_STATUS, &status);
	adreno_readreg(adreno_dev, ADRENO_REG_RBBM_STATUS3, &status3);
	adreno_readreg(adreno_dev, ADRENO_REG_RBBM_INT_0_STATUS, &intstatus);
	adreno_readreg(adreno_dev, ADRENO_REG_CP_HW_FAULT, &hwfault);


	/*
	 * If CP is stuck, gmu may not perform as expected. So force a gmu
	 * snapshot which captures entire state as well as sets the gmu fault
	 * because things need to be reset anyway.
	 */
	if (gmu_core_isenabled(device)) {
		gmu_core_regread(device,
				A6XX_GPU_GMU_AO_GPU_CX_BUSY_STATUS, &cx_status);
		dev_err(device->dev,
				"rb=%d pos=%X/%X rbbm_status=%8.8X/%8.8X int_0_status=%8.8X cx_busy_status:%8.8X\n",
				adreno_dev->cur_rb->id, rptr, wptr, status,
				status3, intstatus, cx_status);

		dev_err(device->dev, " hwfault=%8.8X\n", hwfault);
		gmu_core_snapshot(device);
	} else {
		dev_err(device->dev,
				"rb=%d pos=%X/%X rbbm_status=%8.8X/%8.8X int_0_status=%8.8X\n",
				adreno_dev->cur_rb->id, rptr, wptr, status,
				status3, intstatus);

		dev_err(device->dev, " hwfault=%8.8X\n", hwfault);
		kgsl_device_snapshot(device, NULL, false);
	}
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

static void adreno_retry_rbbm_read(struct kgsl_device *device,
		unsigned int offsetwords, unsigned int *value)
{
	int i;

	/*
	 * If 0xdeafbead was transient, second read is expected to return the
	 * actual register value. However, if a register value is indeed
	 * 0xdeafbead, read it enough times to guarantee that.
	 */
	for (i = 0; i < 16; i++) {
		*value = readl_relaxed(device->reg_virt + (offsetwords << 2));
		/*
		 * Read barrier needed so that register is read from hardware
		 * every iteration
		 */
		rmb();

		if (*value != 0xdeafbead)
			return;
	}
}

static bool adreno_is_rbbm_batch_reg(struct adreno_device *adreno_dev,
	unsigned int offsetwords)
{
	if ((adreno_is_a650(adreno_dev) &&
		ADRENO_CHIPID_PATCH(adreno_dev->chipid) < 2) ||
		adreno_is_a620v1(adreno_dev)) {
		if (((offsetwords >= 0x0) && (offsetwords <= 0x3FF)) ||
		((offsetwords >= 0x4FA) && (offsetwords <= 0x53F)) ||
		((offsetwords >= 0x556) && (offsetwords <= 0x5FF)) ||
		((offsetwords >= 0xF400) && (offsetwords <= 0xFFFF)))
			return  true;
	}

	return false;
}

/**
 * adreno_regread - Used to read adreno device registers
 * @offsetwords - Word (4 Bytes) offset to the register to be read
 * @value - Value read from device register
 */
static void adreno_regread(struct kgsl_device *device, unsigned int offsetwords,
	unsigned int *value)
{
	/* Make sure we're not reading from invalid memory */
	if (WARN(offsetwords * sizeof(uint32_t) >= device->reg_len,
		"Out of bounds register read: 0x%x/0x%x\n",
			offsetwords, device->reg_len >> 2))
		return;

	if (!in_interrupt())
		kgsl_pre_hwaccess(device);

	*value = readl_relaxed(device->reg_virt + (offsetwords << 2));
	/* Order this read with respect to the following memory accesses */
	rmb();

	if ((*value == 0xdeafbead) &&
		adreno_is_rbbm_batch_reg(ADRENO_DEVICE(device), offsetwords))
		adreno_retry_rbbm_read(device, offsetwords, value);
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
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	u64 ts1, ts2;

	adreno_writereg(adreno_dev, offset, val);

	if (!gmu_core_isenabled(KGSL_DEVICE(adreno_dev)))
		return 0;

	ts1 = gmu_core_dev_read_ao_counter(device);
	for (i = 0; i < GMU_CORE_LONG_WAKEUP_RETRY_LIMIT; i++) {
		/*
		 * Make sure the previous register write is posted before
		 * checking the fence status
		 */
		mb();

		adreno_read_gmureg(adreno_dev, ADRENO_REG_GMU_AHB_FENCE_STATUS,
			&status);

		/*
		 * If !writedropped0/1, then the write to fenced register
		 * was successful
		 */
		if (!(status & fence_mask))
			break;

		/* Wait a small amount of time before trying again */
		udelay(GMU_CORE_WAKEUP_DELAY_US);

		/* Try to write the fenced register again */
		adreno_writereg(adreno_dev, offset, val);
	}

	if (i < GMU_CORE_SHORT_WAKEUP_RETRY_LIMIT)
		return 0;

	ts2 = gmu_core_dev_read_ao_counter(device);

	if (i == GMU_CORE_LONG_WAKEUP_RETRY_LIMIT) {
		dev_err(device->dev,
			"Timed out waiting %d usecs to write fenced register 0x%x, timestamps %llu %llu, status 0x%x\n",
			i * GMU_CORE_WAKEUP_DELAY_US,
			reg_offset, ts1, ts2, status);

		return -ETIMEDOUT;
	}

	dev_err(device->dev,
		"Waited %d usecs to write fenced register 0x%x. status 0x%x\n",
		i * GMU_CORE_WAKEUP_DELAY_US, reg_offset, status);

	return 0;
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
void adreno_rscc_regread(struct adreno_device *adreno_dev,
	unsigned int offsetwords, unsigned int *value)
{
	unsigned int rscc_offset;

	rscc_offset = (offsetwords << 2);
	if (!adreno_dev->rscc_virt ||
		(rscc_offset >= adreno_dev->rscc_len))
		return;

	*value =  __raw_readl(adreno_dev->rscc_virt + rscc_offset);

	/*
	 * ensure this read finishes before the next one.
	 * i.e. act like normal readl()
	 */
	rmb();
}

void adreno_isense_regread(struct adreno_device *adreno_dev,
	unsigned int offsetwords, unsigned int *value)
{
	unsigned int isense_offset;

	isense_offset = (offsetwords << 2);
	if (!adreno_dev->isense_virt ||
		(isense_offset >= adreno_dev->isense_len))
		return;

	*value =  __raw_readl(adreno_dev->isense_virt + isense_offset);

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
	u64 gpu_busy;

	memset(stats, 0, sizeof(*stats));

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

	if (!ADRENO_QUIRK(ADRENO_DEVICE(device), ADRENO_QUIRK_IOMMU_SYNC))
		return;

	if (sync) {
		mutex_lock(&kgsl_mmu_sync);
		desc.args[0] = true;
		desc.arginfo = SCM_ARGS(1);
		ret = scm_call2_atomic(SCM_SIP_FNID(SCM_SVC_PWR, 0x8), &desc);
		if (ret)
			dev_err(device->dev,
				     "MMU sync with Hypervisor off %x\n", ret);
	} else {
		desc.args[0] = false;
		desc.arginfo = SCM_ARGS(1);
		scm_call2_atomic(SCM_SIP_FNID(SCM_SVC_PWR, 0x8), &desc);
		mutex_unlock(&kgsl_mmu_sync);
	}
}

static void _regulator_disable(struct device *dev,
		struct kgsl_regulator *regulator, unsigned int timeout)
{
	unsigned long wait_time;

	if (IS_ERR_OR_NULL(regulator->reg))
		return;

	regulator_disable(regulator->reg);

	wait_time = jiffies + msecs_to_jiffies(timeout);

	/* Poll for regulator status to ensure it's OFF */
	while (!time_after(jiffies, wait_time)) {
		if (!regulator_is_enabled(regulator->reg))
			return;
		usleep_range(10, 100);
	}

	if (!regulator_is_enabled(regulator->reg))
		return;

	dev_err(dev, "regulator '%s' disable timed out\n", regulator->name);
}

static void adreno_regulator_disable_poll(struct kgsl_device *device)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;
	int i;
	unsigned int timeout =
		ADRENO_QUIRK(adreno_dev, ADRENO_QUIRK_IOMMU_SYNC) ? 200 : 5000;

	adreno_iommu_sync(device, true);

	for (i = KGSL_MAX_REGULATORS - 1; i >= 0; i--)
		_regulator_disable(device->dev, &pwr->regulators[i], timeout);

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

static bool adreno_is_hwcg_on(struct kgsl_device *device)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);

	return test_bit(ADRENO_HWCG_CTRL, &adreno_dev->pwrctrl_flag);
}

u32 adreno_get_ucode_version(const u32 *data)
{
	u32 version;

	version = data[1];

	if ((version & 0xf) != 0xa)
		return version;

	version &= ~0xfff;
	return  version | ((data[3] & 0xfff000) >> 12);
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
	.query_property_list = adreno_query_property_list,
	.is_hwcg_on = adreno_is_hwcg_on,
};

static struct platform_driver adreno_platform_driver = {
	.probe = adreno_probe,
	.remove = adreno_remove,
	.suspend = kgsl_suspend_driver,
	.resume = kgsl_resume_driver,
	.id_table = adreno_id_table,
	.driver = {
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
