// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2002,2007-2021, The Linux Foundation. All rights reserved.
 */
#include <linux/component.h>
#include <linux/delay.h>
#include <linux/firmware.h>
#include <linux/input.h>
#include <linux/interconnect.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_fdt.h>
#include <linux/module.h>
#include <linux/regulator/consumer.h>
#include <linux/nvmem-consumer.h>
#include <linux/soc/qcom/llcc-qcom.h>
#include <soc/qcom/subsystem_restart.h>
#include <soc/qcom/boot_stats.h>

#include "adreno.h"
#include "adreno_a3xx.h"
#include "adreno_a5xx.h"
#include "adreno_a6xx.h"
#include "adreno_compat.h"
#include "adreno_hwsched.h"
#include "adreno_iommu.h"
#include "adreno_trace.h"
#include "kgsl_bus.h"
#include "kgsl_trace.h"
#include "kgsl_util.h"

/* Include the master list of GPU cores that are supported */
#include "adreno-gpulist.h"

static void adreno_input_work(struct work_struct *work);
static int adreno_soft_reset(struct kgsl_device *device);
static unsigned int counter_delta(struct kgsl_device *device,
	unsigned int reg, unsigned int *counter);
static struct device_node *
	adreno_get_gpu_model_node(struct platform_device *pdev);

static struct adreno_device device_3d0;

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

static u32 get_ucode_version(const u32 *data)
{
	u32 version;

	version = data[1];

	if ((version & 0xf) != 0xa)
		return version;

	version &= ~0xfff;
	return  version | ((data[3] & 0xfff000) >> 12);
}

int adreno_get_firmware(struct adreno_device *adreno_dev,
		const char *fwfile, struct adreno_firmware *firmware)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	const struct firmware *fw = NULL;
	int ret;

	if (!IS_ERR_OR_NULL(firmware->memdesc))
		return 0;

	ret = request_firmware(&fw, fwfile, &device->pdev->dev);

	if (ret) {
		dev_err(device->dev, "request_firmware(%s) failed: %d\n",
				fwfile, ret);
		return ret;
	}

	firmware->memdesc = kgsl_allocate_global(device, fw->size - 4, 0,
				KGSL_MEMFLAGS_GPUREADONLY, KGSL_MEMDESC_UCODE,
				"ucode");

	ret = PTR_ERR_OR_ZERO(firmware->memdesc);
	if (!ret) {
		memcpy(firmware->memdesc->hostptr, &fw->data[4], fw->size - 4);
		firmware->size = (fw->size - 4) / sizeof(u32);
		firmware->version = get_ucode_version((u32 *)fw->data);
	}

	release_firmware(fw);
	return ret;
}


int adreno_zap_shader_load(struct adreno_device *adreno_dev,
		const char *name)
{
	void *ptr;

	if (!name || adreno_dev->zap_loaded)
		return 0;

	ptr = subsystem_get(name);

	if (!IS_ERR(ptr))
		adreno_dev->zap_loaded = true;

	return PTR_ERR_OR_ZERO(ptr);
}

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
	const struct adreno_gpudev *gpudev = ADRENO_GPU_DEVICE(adreno_dev);
	unsigned int val_lo = 0, val_hi = 0;
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);

	if (adreno_checkreg_off(adreno_dev, lo))
		kgsl_regread(device, gpudev->reg_offsets[lo], &val_lo);
	if (adreno_checkreg_off(adreno_dev, hi))
		kgsl_regread(device, gpudev->reg_offsets[hi], &val_hi);

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
	const struct adreno_gpudev *gpudev = ADRENO_GPU_DEVICE(adreno_dev);

	if (adreno_checkreg_off(adreno_dev, lo))
		kgsl_regwrite(KGSL_DEVICE(adreno_dev),
			gpudev->reg_offsets[lo], lower_32_bits(val));
	if (adreno_checkreg_off(adreno_dev, hi))
		kgsl_regwrite(KGSL_DEVICE(adreno_dev),
			gpudev->reg_offsets[hi], upper_32_bits(val));
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

		kgsl_sharedmem_readl(device->scratch, &rptr,
				SCRATCH_RPTR_OFFSET(rb->id));
	}

	return rptr;
}

static void __iomem *efuse_base;
static size_t efuse_len;

int adreno_efuse_map(struct platform_device *pdev)
{
	struct resource *res;

	if (efuse_base != NULL)
		return 0;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
		"qfprom_memory");

	if (res == NULL)
		return -ENODEV;

	efuse_base = ioremap(res->start, resource_size(res));
	if (efuse_base == NULL)
		return -ENODEV;

	efuse_len = resource_size(res);
	return 0;
}

void adreno_efuse_unmap(void)
{
	if (efuse_base != NULL) {
		iounmap(efuse_base);
		efuse_base = NULL;
		efuse_len = 0;
	}
}

int adreno_efuse_read_u32(unsigned int offset, unsigned int *val)
{
	if (efuse_base == NULL)
		return -ENODEV;

	if (offset >= efuse_len)
		return -ERANGE;

	*val = readl_relaxed(efuse_base + offset);
	/* Make sure memory is updated before returning */
	rmb();

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
	const struct adreno_gpudev *gpudev = ADRENO_GPU_DEVICE(adreno_dev);
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
	const struct adreno_gpudev *gpudev = ADRENO_GPU_DEVICE(adreno_dev);
	unsigned int i, j = ARRAY_SIZE(adreno_ft_regs_default);

	if (!test_bit(ADRENO_DEVICE_SOFT_FAULT_DETECT, &adreno_dev->priv))
		return;

	if (!adreno_dev->fast_hang_detect)
		return;

	for (i = 0; i < gpudev->ft_perf_counters_count; i++) {
		if (!adreno_ft_regs[j + (i * 2)])
			continue;

		adreno_perfcounter_put(adreno_dev,
			gpudev->ft_perf_counters[i].counter,
			gpudev->ft_perf_counters[i].countable,
			PERFCOUNTER_FLAG_KERNEL);

		adreno_ft_regs[j + (i * 2)] = 0;
		adreno_ft_regs[(j + (i * 2)) + 1] = 0;
	}

	adreno_dev->fast_hang_detect = 0;
}

static void adreno_touch_wakeup(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);

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
	const struct adreno_power_ops *ops = ADRENO_POWER_OPS(adreno_dev);

	mutex_lock(&device->mutex);

	device->flags |= KGSL_FLAG_WAKE_ON_TOUCH;

	ops->touch_wakeup(adreno_dev);

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

	if (gmu_core_isenabled(device)) {
		schedule_work(&adreno_dev->input_work);
		return;
	}

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
		kgsl_start_idle_timer(device);

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
static void _soft_reset(struct adreno_device *adreno_dev)
{
	const struct adreno_gpudev *gpudev  = ADRENO_GPU_DEVICE(adreno_dev);
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
	adreno_writereg(adreno_dev, ADRENO_REG_RBBM_INT_0_MASK,
		state ? adreno_dev->irq_mask : 0);
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
	const struct adreno_gpudev *gpudev = ADRENO_GPU_DEVICE(adreno_dev);
	irqreturn_t ret;

	atomic_inc(&adreno_dev->pending_irq_refcnt);
	/* Ensure this increment is done before the IRQ status is updated */
	smp_mb__after_atomic();

	ret = gpudev->irq_handler(adreno_dev);

	/* Make sure the regwrites are done before the decrement */
	smp_mb__before_atomic();
	atomic_dec(&adreno_dev->pending_irq_refcnt);
	/* Ensure other CPUs see the decrement */
	smp_mb__after_atomic();

	return ret;
}

irqreturn_t adreno_irq_callbacks(struct adreno_device *adreno_dev,
		const struct adreno_irq_funcs *funcs, u32 status)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	irqreturn_t ret = IRQ_NONE;

	/* Loop through all set interrupts and call respective handlers */
	while (status) {
		int i = fls(status) - 1;

		if (funcs[i].func) {
			if (adreno_dev->irq_mask & BIT(i))
				funcs[i].func(adreno_dev, i);
		} else
			dev_crit_ratelimited(device->dev,
				"Unhandled interrupt bit %x\n", i);

		ret = IRQ_HANDLED;

		status &= ~BIT(i);
	}

	return ret;
}


static inline bool _rev_match(unsigned int id, unsigned int entry)
{
	return (entry == ANY_ID || entry == id);
}

static const struct adreno_gpu_core *
_get_gpu_core(struct platform_device *pdev, unsigned int chipid)
{
	unsigned int core = ADRENO_CHIPID_CORE(chipid);
	unsigned int major = ADRENO_CHIPID_MAJOR(chipid);
	unsigned int minor = ADRENO_CHIPID_MINOR(chipid);
	unsigned int patchid = ADRENO_CHIPID_PATCH(chipid);
	int i;
	struct device_node *node;

	/*
	 * When "qcom,gpu-models" is defined, use gpu model node to match
	 * on a compatible string, otherwise match using legacy way.
	 */
	node = adreno_get_gpu_model_node(pdev);
	if (!node || !of_find_property(node, "compatible", NULL))
		node = pdev->dev.of_node;

	/* Check to see if any of the entries match on a compatible string */
	for (i = 0; i < ARRAY_SIZE(adreno_gpulist); i++) {
		if (adreno_gpulist[i]->compatible &&
			of_device_is_compatible(node,
				adreno_gpulist[i]->compatible))
			return adreno_gpulist[i];
	}

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
	{ ADRENO_QUIRK_CX_GDSC, "qcom,gpu-quirk-cx-gdsc" },
};

static struct device_node *
adreno_get_soc_hw_revision_node(struct platform_device *pdev, u32 hwrev)
{
	struct device_node *node, *child;

	node = of_find_node_by_name(pdev->dev.of_node, "qcom,soc-hw-revisions");
	if (node == NULL)
		return NULL;

	for_each_child_of_node(node, child) {
		u32 rev;

		if (of_property_read_u32(child, "qcom,soc-hw-revision", &rev))
			continue;

		if (rev == hwrev) {
			of_node_put(node);
			return child;
		}
	}

	of_node_put(node);

	dev_warn(&pdev->dev, "No matching SOC HW revision found for efused HW rev=%u\n",
		hwrev);

	return NULL;
}

static u32 adreno_efuse_read_soc_hw_rev(struct platform_device *pdev)
{
	unsigned int val;
	unsigned int soc_hw_rev[3];
	int ret;

	if (of_property_read_u32_array(pdev->dev.of_node,
		"qcom,soc-hw-rev-efuse", soc_hw_rev, 3))
		return 0;

	ret = adreno_efuse_map(pdev);
	if (ret) {
		dev_err(&pdev->dev,
			"Unable to map hardware revision fuse: ret=%d\n", ret);
		return 0;
	}

	ret = adreno_efuse_read_u32(soc_hw_rev[0], &val);
	adreno_efuse_unmap();

	if (ret) {
		dev_err(&pdev->dev,
			"Unable to read hardware revision fuse: ret=%d\n", ret);
		return 0;
	}

	return (val >> soc_hw_rev[1]) & soc_hw_rev[2];
}

static int adreno_get_chipid(struct platform_device *pdev, u32 *chipid)
{
	u32 hwrev = adreno_efuse_read_soc_hw_rev(pdev);
	struct device_node *node;
	int ret;

	node = adreno_get_soc_hw_revision_node(pdev, hwrev);
	if (!node)
		node = of_node_get(pdev->dev.of_node);

	ret = of_property_read_u32(node, "qcom,chipid", chipid);

	of_node_put(node);
	return ret;
}

static void
adreno_update_soc_hw_revision_quirks(struct adreno_device *adreno_dev,
		struct platform_device *pdev)
{
	struct device_node *node;
	int i;
	u32 hwrev;

	hwrev = adreno_efuse_read_soc_hw_rev(pdev);

	node = adreno_get_soc_hw_revision_node(pdev, hwrev);
	if (node == NULL)
		node = of_node_get(pdev->dev.of_node);

	/* update quirk */
	for (i = 0; i < ARRAY_SIZE(adreno_quirks); i++) {
		if (of_property_read_bool(node, adreno_quirks[i].prop))
			adreno_dev->quirks |= adreno_quirks[i].quirk;
	}

	/* Set a quirk in the MMU */
	if (of_property_read_bool(node, "qcom,gpu-quirk-mmu-secure-cb-alt"))
		kgsl_mmu_set_feature(KGSL_DEVICE(adreno_dev),
			KGSL_MMU_SECURE_CB_ALT);

	of_node_put(node);
}

static const struct adreno_gpu_core *
adreno_identify_gpu(struct platform_device *pdev, u32 *chipid)
{
	const struct adreno_gpu_core *gpucore;

	if (adreno_get_chipid(pdev, chipid)) {
		dev_crit(&pdev->dev, "Unable to get the GPU chip ID\n");
		return ERR_PTR(-ENODEV);
	}

	gpucore = _get_gpu_core(pdev, *chipid);
	if (!gpucore) {
		dev_crit(&pdev->dev, "Unknown GPU chip ID %8.8x\n", *chipid);
		return ERR_PTR(-ENODEV);
	}

	/*
	 * Identify non-longer supported targets and spins and print a helpful
	 * message
	 */
	if (gpucore->features & ADRENO_DEPRECATED) {
		dev_err(&pdev->dev,
			"Support for GPU %d.%d.%d.%d has been deprecated\n",
			gpucore->core,
			gpucore->major,
			gpucore->minor,
			gpucore->patchid);
		return ERR_PTR(-ENODEV);
	}

	return gpucore;
}

static const struct of_device_id adreno_match_table[] = {
	{ .compatible = "qcom,kgsl-3d0", .data = &device_3d0 },
	{ },
};

MODULE_DEVICE_TABLE(of, adreno_match_table);

/* Dynamically build the OPP table for the GPU device */
static void adreno_build_opp_table(struct device *dev, struct kgsl_pwrctrl *pwr)
{
	int i;

	/* Skip if the table has already been populated */
	if (dev_pm_opp_get_opp_count(dev) > 0)
		return;

	/* Add all the supported frequencies into the tree */
	for (i = 0; i < pwr->num_pwrlevels; i++)
		dev_pm_opp_add(dev, pwr->pwrlevels[i].gpu_freq, 0);
}

static int adreno_of_parse_pwrlevels(struct adreno_device *adreno_dev,
		struct device_node *node)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;
	struct device_node *child;
	int ret;

	pwr->num_pwrlevels = 0;

	for_each_child_of_node(node, child) {
		u32 index, freq = 0, voltage, bus;
		struct kgsl_pwrlevel *level;

		ret = of_property_read_u32(child, "reg", &index);
		if (ret) {
			dev_err(device->dev, "%pOF: powerlevel index not found\n",
				child);
			goto out;
		}

		ret = of_property_read_u32(child, "qcom,gpu-freq", &freq);
		if (ret) {
			dev_err(device->dev, "%pOF: Unable to read qcom,gpu-freq\n",
				child);
			goto out;
		}

		/* Ignore "zero" powerlevels */
		if (!freq)
			continue;

		ret = of_property_read_u32(child, "qcom,level", &voltage);
		if (ret) {
			dev_err(device->dev, "%pOF: Unable to read qcom,level\n",
				child);
			goto out;
		}

		ret = kgsl_of_property_read_ddrtype(child, "qcom,bus-freq",
			&bus);
		if (ret) {
			dev_err(device->dev, "%pOF:Unable to read qcom,bus-freq\n",
				child);
			goto out;
		}


		if (index >= KGSL_MAX_PWRLEVELS) {
			dev_err(device->dev, "%pOF: Pwrlevel index %d is out of range\n",
				child, index);
			continue;
		}

		if (index >= pwr->num_pwrlevels)
			pwr->num_pwrlevels = index + 1;

		level = &pwr->pwrlevels[index];

		level->gpu_freq = freq;
		level->bus_freq = bus;
		level->voltage_level = voltage;

		of_property_read_u32(child, "qcom,acd-level",
			&level->acd_level);

		level->bus_min = level->bus_freq;
		kgsl_of_property_read_ddrtype(child,
			"qcom,bus-min", &level->bus_min);

		level->bus_max = level->bus_freq;
		kgsl_of_property_read_ddrtype(child,
			"qcom,bus-max", &level->bus_max);
	}

	adreno_build_opp_table(&device->pdev->dev, pwr);
	return 0;
out:
	of_node_put(child);
	return ret;
}

static void adreno_of_get_initial_pwrlevel(struct kgsl_pwrctrl *pwr,
		struct device_node *node)
{
	int init_level = 1;

	of_property_read_u32(node, "qcom,initial-pwrlevel", &init_level);

	if (init_level < 0 || init_level >= pwr->num_pwrlevels)
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

	adreno_dev->lm_enabled = true;
}

static int adreno_of_get_legacy_pwrlevels(struct adreno_device *adreno_dev,
		struct device_node *parent)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct device_node *node;
	int ret;

	node = of_find_node_by_name(parent, "qcom,gpu-pwrlevels");

	if (node == NULL) {
		dev_err(&device->pdev->dev,
			"Unable to find 'qcom,gpu-pwrlevels'\n");
		return -EINVAL;
	}

	ret = adreno_of_parse_pwrlevels(adreno_dev, node);

	if (!ret) {
		adreno_of_get_initial_pwrlevel(&device->pwrctrl, parent);
		adreno_of_get_limits(adreno_dev, parent);
	}

	of_node_put(node);
	return ret;
}

static int adreno_of_get_pwrlevels(struct adreno_device *adreno_dev,
		struct device_node *parent)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct device_node *node, *child;
	unsigned int bin = 0;

	node = of_find_node_by_name(parent, "qcom,gpu-pwrlevel-bins");
	if (node == NULL)
		return adreno_of_get_legacy_pwrlevels(adreno_dev, parent);

	for_each_child_of_node(node, child) {

		if (of_property_read_u32(child, "qcom,speed-bin", &bin))
			continue;

		if (bin == device->speed_bin) {
			int ret;

			ret = adreno_of_parse_pwrlevels(adreno_dev, child);
			if (ret) {
				of_node_put(child);
				return ret;
			}

			adreno_of_get_initial_pwrlevel(&device->pwrctrl, child);

			/*
			 * Check for global throttle-pwrlevel first and override
			 * with speedbin specific one if found.
			 */
			adreno_of_get_limits(adreno_dev, parent);
			adreno_of_get_limits(adreno_dev, child);

			of_node_put(child);
			return 0;
		}
	}

	dev_err(&device->pdev->dev,
		"GPU speed_bin:%d mismatch for efused bin:%d\n",
		device->speed_bin, bin);
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

		of_property_read_u32(child, "qcom,l3-freq",
				&device->l3_freq[index]);
	}

	device->l3_icc = of_icc_get(&device->pdev->dev, "l3_path");

	if (IS_ERR(device->l3_icc))
		dev_err(&device->pdev->dev,
			"Unable to get the l3 icc path\n");
}

static int adreno_of_get_power(struct adreno_device *adreno_dev,
		struct platform_device *pdev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct resource *res;
	int ret;

	/* Get starting physical address of device registers */
	res = platform_get_resource_byname(device->pdev, IORESOURCE_MEM,
		"kgsl_3d0_reg_memory");

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

	ret = adreno_of_get_pwrlevels(adreno_dev, pdev->dev.of_node);
	if (ret)
		return ret;

	l3_pwrlevel_probe(device, pdev->dev.of_node);

	device->pwrctrl.interval_timeout = CONFIG_QCOM_KGSL_IDLE_TIMEOUT;

	device->pwrctrl.minbw_timeout = 10;

	/* Set default bus control to true on all targets */
	device->pwrctrl.bus_control = true;

	device->pwrctrl.input_disable =
		of_property_read_bool(pdev->dev.of_node,
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
	adreno_dev->cx_dbgc_virt = devm_ioremap(&device->pdev->dev,
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
	adreno_dev->cx_misc_virt = devm_ioremap(&device->pdev->dev,
					res->start, adreno_dev->cx_misc_len);
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
	adreno_dev->isense_virt = devm_ioremap(&device->pdev->dev, res->start,
					adreno_dev->isense_len);
	if (adreno_dev->isense_virt == NULL)
		dev_warn(device->dev, "isense ioremap failed\n");
}

static bool adreno_is_gpu_disabled(struct platform_device *pdev)
{
	unsigned int row0;
	unsigned int pte_row0_msb[3];
	int ret;

	if (of_property_read_u32_array(pdev->dev.of_node,
		"qcom,gpu-disable-fuse", pte_row0_msb, 3))
		return false;
	/*
	 * Read the fuse value to disable GPU driver if fuse
	 * is blown. By default(fuse value is 0) GPU is enabled.
	 */
	if (adreno_efuse_map(pdev))
		return false;

	ret = adreno_efuse_read_u32(pte_row0_msb[0], &row0);
	adreno_efuse_unmap();

	if (ret)
		return false;

	return (row0 >> pte_row0_msb[2]) &
			pte_row0_msb[1] ? true : false;
}

/* Read the efuse through the legacy method */
static int adreno_read_speed_bin_legacy(struct platform_device *pdev)
{
	u32 bin[3];
	u32 val;

	if (of_property_read_u32_array(pdev->dev.of_node,
		"qcom,gpu-speed-bin", bin, 3))
		return 0;

	if (adreno_efuse_map(pdev))
		return 0;

	adreno_efuse_read_u32(bin[0], &val);
	adreno_efuse_unmap();

	return (val & bin[1]) >> bin[2];
}

/* Read the efuse through the new and fancy nvmem method */
static int adreno_read_speed_bin(struct platform_device *pdev)
{
	struct nvmem_cell *cell = nvmem_cell_get(&pdev->dev, "speed_bin");
	int ret = PTR_ERR_OR_ZERO(cell);
	void *buf;
	int val = 0;
	size_t len;

	if (ret) {
		/* If the nvmem node isn't there, try legacy */
		if (ret == -ENOENT)
			return adreno_read_speed_bin_legacy(pdev);

		return ret;
	}

	buf = nvmem_cell_read(cell, &len);
	nvmem_cell_put(cell);

	if (IS_ERR(buf))
		return PTR_ERR(buf);

	memcpy(&val, buf, min(len, sizeof(val)));
	kfree(buf);

	return val;
}

static int adreno_read_gpu_model_fuse(struct platform_device *pdev)
{
	struct nvmem_cell *cell = nvmem_cell_get(&pdev->dev, "gpu_model");
	int ret = PTR_ERR_OR_ZERO(cell);
	void *buf;
	int val = 0;
	size_t len;

	if (ret)
		return ret;

	buf = nvmem_cell_read(cell, &len);
	nvmem_cell_put(cell);

	if (IS_ERR(buf))
		return PTR_ERR(buf);

	memcpy(&val, buf, min(len, sizeof(val)));
	kfree(buf);

	return val;
}

static struct device_node *
adreno_get_gpu_model_node(struct platform_device *pdev)
{
	struct device_node *node, *child;
	int fuse_model = adreno_read_gpu_model_fuse(pdev);

	if (fuse_model < 0)
		return NULL;

	node = of_find_node_by_name(pdev->dev.of_node, "qcom,gpu-models");
	if (node == NULL)
		return NULL;

	for_each_child_of_node(node, child) {
		u32 model;

		if (of_property_read_u32(child, "qcom,gpu-model-id", &model))
			continue;

		if (model == fuse_model) {
			of_node_put(node);
			return child;
		}
	}

	of_node_put(node);

	return NULL;
}

static const char *adreno_get_gpu_model(struct kgsl_device *device)
{
	struct device_node *node;
	static char gpu_model[32];
	const char *model;
	int ret;

	if (strlen(gpu_model))
		return gpu_model;

	node = adreno_get_gpu_model_node(device->pdev);
	if (!node)
		node = of_node_get(device->pdev->dev.of_node);

	ret = of_property_read_string(node, "qcom,gpu-model", &model);
	of_node_put(node);

	if (!ret)
		strlcpy(gpu_model, model, sizeof(gpu_model));
	else
		scnprintf(gpu_model, sizeof(gpu_model), "Adreno%d%d%dv%d",
			ADRENO_CHIPID_CORE(ADRENO_DEVICE(device)->chipid),
			ADRENO_CHIPID_MAJOR(ADRENO_DEVICE(device)->chipid),
			ADRENO_CHIPID_MINOR(ADRENO_DEVICE(device)->chipid),
			ADRENO_CHIPID_PATCH(ADRENO_DEVICE(device)->chipid) + 1);

	return gpu_model;
}

static u32 adreno_get_vk_device_id(struct kgsl_device *device)
{
	struct device_node *node;
	static u32 device_id;

	if (device_id)
		return device_id;

	node = adreno_get_gpu_model_node(device->pdev);
	if (!node)
		node = of_node_get(device->pdev->dev.of_node);

	if (of_property_read_u32(node, "qcom,vk-device-id", &device_id))
		device_id = ADRENO_DEVICE(device)->chipid;

	of_node_put(node);

	return device_id;
}

#if IS_ENABLED(CONFIG_QCOM_LLCC)
static int adreno_probe_llcc(struct adreno_device *adreno_dev,
		struct platform_device *pdev)
{
	int ret;

	/* Get the system cache slice descriptor for GPU */
	adreno_dev->gpu_llc_slice = llcc_slice_getd(LLCC_GPU);
	ret = PTR_ERR_OR_ZERO(adreno_dev->gpu_llc_slice);

	if (ret) {
		/* Propagate EPROBE_DEFER back to the probe function */
		if (ret == -EPROBE_DEFER)
			return ret;

		if (ret != -ENOENT)
			dev_warn(&pdev->dev,
				"Unable to get the GPU LLC slice: %d\n", ret);
	} else
		adreno_dev->gpu_llc_slice_enable = true;

	/* Get the system cache slice descriptor for GPU pagetables */
	adreno_dev->gpuhtw_llc_slice = llcc_slice_getd(LLCC_GPUHTW);
	ret = PTR_ERR_OR_ZERO(adreno_dev->gpuhtw_llc_slice);
	if (ret) {
		if (ret == -EPROBE_DEFER) {
			llcc_slice_putd(adreno_dev->gpu_llc_slice);
			return ret;
		}

		if (ret != -ENOENT)
			dev_warn(&pdev->dev,
				"Unable to get GPU HTW LLC slice: %d\n", ret);
	} else
		adreno_dev->gpuhtw_llc_slice_enable = true;

	return 0;
}
#else
static int adreno_probe_llcc(struct adreno_device *adreno_dev,
		struct platform_device *pdev)
{
	return 0;
}
#endif

static const struct kgsl_functable adreno_functable;

static void adreno_setup_device(struct adreno_device *adreno_dev)
{
	u32 i;

	adreno_dev->dev.name = "kgsl-3d0";
	adreno_dev->dev.ftbl = &adreno_functable;

	init_completion(&adreno_dev->dev.hwaccess_gate);
	init_completion(&adreno_dev->dev.halt_gate);

	idr_init(&adreno_dev->dev.context_idr);

	mutex_init(&adreno_dev->dev.mutex);
	INIT_LIST_HEAD(&adreno_dev->dev.globals);

	/* Set the fault tolerance policy to replay, skip, throttle */
	adreno_dev->ft_policy = BIT(KGSL_FT_REPLAY) |
		BIT(KGSL_FT_SKIPCMD) | BIT(KGSL_FT_THROTTLE);

	/* Enable command timeouts by default */
	adreno_dev->long_ib_detect = true;

	INIT_WORK(&adreno_dev->input_work, adreno_input_work);

	INIT_LIST_HEAD(&adreno_dev->active_list);
	spin_lock_init(&adreno_dev->active_list_lock);

	for (i = 0; i < ARRAY_SIZE(adreno_dev->ringbuffers); i++) {
		struct adreno_ringbuffer *rb = &adreno_dev->ringbuffers[i];

		INIT_LIST_HEAD(&rb->events.group);
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

}

static const struct of_device_id adreno_gmu_match[] = {
	{ .compatible = "qcom,gpu-gmu" },
	{ .compatible = "qcom,gpu-rgmu" },
	{},
};

int adreno_device_probe(struct platform_device *pdev,
		struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct device *dev = &pdev->dev;
	unsigned int priv = 0;
	int status;
	u32 size;

	place_marker("M - DRIVER GPU Init");

	/* Initialize the adreno device structure */
	adreno_setup_device(adreno_dev);

	dev_set_drvdata(dev, device);

	device->pdev = pdev;

	adreno_update_soc_hw_revision_quirks(adreno_dev, pdev);

	status = adreno_read_speed_bin(pdev);
	if (status < 0)
		return status;

	device->speed_bin = status;

	status = adreno_of_get_power(adreno_dev, pdev);
	if (status)
		return status;

	status = kgsl_bus_init(device, pdev);
	if (status)
		goto err;

	/*
	 * Bind the GMU components (if applicable) before doing the KGSL
	 * platform probe
	 */
	if (of_find_matching_node(dev->of_node, adreno_gmu_match)) {
		status = component_bind_all(dev, NULL);
		if (status) {
			kgsl_bus_close(device);
			return status;
		}
	}

	/*
	 * The SMMU APIs use unsigned long for virtual addresses which means
	 * that we cannot use 64 bit virtual addresses on a 32 bit kernel even
	 * though the hardware and the rest of the KGSL driver supports it.
	 */
	if (adreno_support_64bit(adreno_dev))
		kgsl_mmu_set_feature(device, KGSL_MMU_64BIT);

	/*
	 * Set the SMMU aperture on A6XX targets to use per-process pagetables.
	 */
	if (adreno_is_a6xx(adreno_dev))
		kgsl_mmu_set_feature(device, KGSL_MMU_SMMU_APERTURE);

	if (ADRENO_FEATURE(adreno_dev, ADRENO_IOCOHERENT))
		kgsl_mmu_set_feature(device, KGSL_MMU_IO_COHERENT);

	device->pwrctrl.bus_width = adreno_dev->gpucore->bus_width;

	device->mmu.secured = (IS_ENABLED(CONFIG_QCOM_SECURE_BUFFER) &&
		ADRENO_FEATURE(adreno_dev, ADRENO_CONTENT_PROTECTION));

	/* Probe the LLCC - this could return -EPROBE_DEFER */
	status = adreno_probe_llcc(adreno_dev, pdev);
	if (status)
		goto err;

	/*
	 * IF the GPU HTW slice was successsful set the MMU feature so the
	 * domain can set the appropriate attributes
	 */
	if (!IS_ERR_OR_NULL(adreno_dev->gpuhtw_llc_slice))
		kgsl_mmu_set_feature(device, KGSL_MMU_LLCC_ENABLE);

	status = kgsl_device_platform_probe(device);
	if (status)
		goto err;

	/* Probe for the optional CX_DBGC block */
	adreno_cx_dbgc_probe(device);

	/* Probe for the optional CX_MISC block */
	adreno_cx_misc_probe(device);

	adreno_isense_probe(device);

	/* Allocate the memstore for storing timestamps and other useful info */

	if (ADRENO_FEATURE(adreno_dev, ADRENO_APRIV))
		priv |= KGSL_MEMDESC_PRIVILEGED;

	device->memstore = kgsl_allocate_global(device,
		KGSL_MEMSTORE_SIZE, 0, 0, priv, "memstore");

	status = PTR_ERR_OR_ZERO(device->memstore);
	if (status) {
		kgsl_device_platform_remove(device);
		goto err;
	}

	/* Initialize the snapshot engine */
	size = adreno_dev->gpucore->snapshot_size;

	/*
	 * Use a default size if one wasn't specified, but print a warning so
	 * the developer knows to fix it
	 */

	if (WARN(!size, "The snapshot size was not specified in the gpucore\n"))
		size = SZ_1M;

	kgsl_device_snapshot_probe(device, size);

	adreno_debugfs_init(adreno_dev);
	adreno_profile_init(adreno_dev);

	adreno_sysfs_init(adreno_dev);

	kgsl_pwrscale_init(device, pdev, CONFIG_QCOM_ADRENO_DEFAULT_GOVERNOR);

	/* Initialize coresight for the target */
	adreno_coresight_init(adreno_dev);

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

	place_marker("M - DRIVER GPU Ready");

	return 0;
err:
	device->pdev = NULL;

	if (of_find_matching_node(dev->of_node, adreno_gmu_match))
		component_unbind_all(dev, NULL);

	kgsl_bus_close(device);

	return status;
}

static int adreno_bind(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	const struct adreno_gpu_core *gpucore;
	u32 chipid;

	if (adreno_is_gpu_disabled(pdev)) {
		dev_err(&pdev->dev, "adreno: GPU is disabled on this device\n");
		return -ENODEV;
	}

	gpucore = adreno_identify_gpu(pdev, &chipid);
	if (IS_ERR(gpucore))
		return PTR_ERR(gpucore);

	return gpucore->gpudev->probe(pdev, chipid, gpucore);
}

static void _adreno_free_memories(struct adreno_device *adreno_dev)
{
	struct adreno_firmware *pfp_fw = ADRENO_FW(adreno_dev, ADRENO_FW_PFP);
	struct adreno_firmware *pm4_fw = ADRENO_FW(adreno_dev, ADRENO_FW_PM4);

	/* Free local copies of firmware and other command streams */
	kfree(pfp_fw->fwvirt);
	pfp_fw->fwvirt = NULL;

	kfree(pm4_fw->fwvirt);
	pm4_fw->fwvirt = NULL;

	kfree(adreno_dev->gpmu_cmds);
	adreno_dev->gpmu_cmds = NULL;
}

static void adreno_unbind(struct device *dev)
{
	struct adreno_device *adreno_dev;
	struct kgsl_device *device;
	const struct adreno_gpudev *gpudev;

	device = dev_get_drvdata(dev);
	if (!device)
		return;

	adreno_dev = ADRENO_DEVICE(device);
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
	if (!IS_ERR_OR_NULL(adreno_dev->gpu_llc_slice))
		llcc_slice_putd(adreno_dev->gpu_llc_slice);

	if (!IS_ERR_OR_NULL(adreno_dev->gpuhtw_llc_slice))
		llcc_slice_putd(adreno_dev->gpuhtw_llc_slice);

	kgsl_pwrscale_close(device);

	if (!test_bit(GMU_DISPATCH, &device->gmu_core.flags)) {
		adreno_dispatcher_close(adreno_dev);

		adreno_ringbuffer_close(adreno_dev);

		adreno_fault_detect_stop(adreno_dev);
	}

	kfree(adreno_ft_regs);
	adreno_ft_regs = NULL;

	kfree(adreno_ft_regs_val);
	adreno_ft_regs_val = NULL;

	if (efuse_base != NULL)
		iounmap(efuse_base);

	kgsl_device_platform_remove(device);

	if (of_find_matching_node(dev->of_node, adreno_gmu_match))
		component_unbind_all(dev, NULL);

	clear_bit(ADRENO_DEVICE_PWRON_FIXUP, &adreno_dev->priv);
	clear_bit(ADRENO_DEVICE_INITIALIZED, &adreno_dev->priv);
}

static void adreno_resume(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);

	if (device->state == KGSL_STATE_SUSPEND) {
		adreno_dispatcher_unhalt(device);
		kgsl_pwrctrl_change_state(device, KGSL_STATE_SLUMBER);
	} else if (device->state != KGSL_STATE_INIT) {
		/*
		 * This is an error situation so wait for the device to idle and
		 * then put the device in SLUMBER state.  This will get us to
		 * the right place when we resume.
		 */
		if (device->state == KGSL_STATE_ACTIVE)
			adreno_idle(device);
		kgsl_pwrctrl_change_state(device, KGSL_STATE_SLUMBER);
		dev_err(device->dev, "resume invoked without a suspend\n");
	}
}

static int adreno_pm_resume(struct device *dev)
{
	struct kgsl_device *device = dev_get_drvdata(dev);
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	const struct adreno_power_ops *ops = ADRENO_POWER_OPS(adreno_dev);

	mutex_lock(&device->mutex);
	ops->pm_resume(adreno_dev);
	mutex_unlock(&device->mutex);

	return 0;
}

static int adreno_suspend(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	int status = kgsl_pwrctrl_change_state(device, KGSL_STATE_SUSPEND);

	if (!status && device->state == KGSL_STATE_SUSPEND)
		adreno_dispatcher_halt(device);

	return status;
}

static int adreno_pm_suspend(struct device *dev)
{
	struct kgsl_device *device = dev_get_drvdata(dev);
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	const struct adreno_power_ops *ops = ADRENO_POWER_OPS(adreno_dev);
	int status;

	mutex_lock(&device->mutex);
	status = ops->pm_suspend(adreno_dev);
	mutex_unlock(&device->mutex);

	return status;
}

static void adreno_fault_detect_init(struct adreno_device *adreno_dev)
{
	const struct adreno_gpudev *gpudev = ADRENO_GPU_DEVICE(adreno_dev);
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

static int adreno_init(struct kgsl_device *device)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	const struct adreno_gpudev *gpudev = ADRENO_GPU_DEVICE(adreno_dev);
	int ret;

	ret = kgsl_pwrctrl_change_state(device, KGSL_STATE_INIT);
	if (ret)
		return ret;

	/*
	 * initialization only needs to be done once initially until
	 * device is shutdown
	 */
	if (test_bit(ADRENO_DEVICE_INITIALIZED, &adreno_dev->priv))
		return 0;

	ret = adreno_dispatcher_init(adreno_dev);
	if (ret)
		return ret;

	ret = adreno_ringbuffer_init(adreno_dev);
	if (ret)
		return ret;

	/*
	 * Either the microcode read failed because the usermodehelper isn't
	 * available or the microcode was corrupted. Fail the init and force
	 * the user to try the open() again
	 */

	ret = gpudev->microcode_read(adreno_dev);
	if (ret)
		return ret;

	if (gpudev->init != NULL) {
		ret = gpudev->init(adreno_dev);
		if (ret)
			return ret;
	}

	/* Put the GPU in a responsive state */
	if (ADRENO_GPUREV(adreno_dev) < 600) {
		/* No need for newer generation architectures */
		ret = kgsl_pwrctrl_change_state(device, KGSL_STATE_AWARE);
		if (ret)
			return ret;
	}

	 adreno_iommu_init(adreno_dev);

	adreno_fault_detect_init(adreno_dev);

	adreno_dev->cooperative_reset = ADRENO_FEATURE(adreno_dev,
							ADRENO_COOP_RESET);

	/* Power down the device */
	if (ADRENO_GPUREV(adreno_dev) < 600)
		kgsl_pwrctrl_change_state(device, KGSL_STATE_SLUMBER);

	set_bit(ADRENO_DEVICE_INITIALIZED, &adreno_dev->priv);

	/*
	 * Allocate a small chunk of memory for precise drawobj profiling for
	 * those targets that have the always on timer
	 */

	if (!adreno_is_a3xx(adreno_dev)) {
		unsigned int priv = 0;

		if (ADRENO_FEATURE(adreno_dev, ADRENO_APRIV))
			priv |= KGSL_MEMDESC_PRIVILEGED;

		adreno_dev->profile_buffer =
			kgsl_allocate_global(device, PAGE_SIZE, 0, 0, priv,
				"alwayson");

		adreno_dev->profile_index = 0;

		if (!IS_ERR(adreno_dev->profile_buffer))
			set_bit(ADRENO_DEVICE_DRAWOBJ_PROFILE,
				&adreno_dev->priv);
	}

	return 0;
}

static bool regulators_left_on(struct kgsl_device *device)
{
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;

	if (gmu_core_gpmu_isenabled(device))
		return false;

	if (!IS_ERR_OR_NULL(pwr->cx_gdsc))
		if (regulator_is_enabled(pwr->cx_gdsc))
			return true;

	if (!IS_ERR_OR_NULL(pwr->gx_gdsc))
		return regulator_is_enabled(pwr->gx_gdsc);

	return false;
}

void adreno_set_active_ctxs_null(struct adreno_device *adreno_dev)
{
	int i;
	struct adreno_ringbuffer *rb;

	FOR_EACH_RINGBUFFER(adreno_dev, rb, i) {
		if (rb->drawctxt_active)
			kgsl_context_put(&(rb->drawctxt_active->base));
		rb->drawctxt_active = NULL;

		kgsl_sharedmem_writel(rb->pagetable_desc,
			PT_INFO_OFFSET(current_rb_ptname), 0);
	}
}

static int adreno_open(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	int ret;

	/*
	 * active_cnt special case: we are starting up for the first
	 * time, so use this sequence instead of the kgsl_pwrctrl_wake()
	 * which will be called by adreno_active_count_get().
	 */
	atomic_inc(&device->active_cnt);

	memset(device->memstore->hostptr, 0, device->memstore->size);

	ret = adreno_init(device);
	if (ret)
		goto err;

	ret = adreno_start(device, 0);
	if (ret)
		goto err;

	complete_all(&device->hwaccess_gate);
	kgsl_pwrctrl_change_state(device, KGSL_STATE_ACTIVE);
	adreno_active_count_put(adreno_dev);

	return 0;
err:
	kgsl_pwrctrl_change_state(device, KGSL_STATE_INIT);
	atomic_dec(&device->active_cnt);

	return ret;
}

static int adreno_first_open(struct kgsl_device *device)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	const struct adreno_power_ops *ops = ADRENO_POWER_OPS(adreno_dev);

	return ops->first_open(adreno_dev);
}

static int adreno_close(struct adreno_device *adreno_dev)
{
	return kgsl_pwrctrl_change_state(KGSL_DEVICE(adreno_dev),
			KGSL_STATE_INIT);
}

static int adreno_last_close(struct kgsl_device *device)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	const struct adreno_power_ops *ops = ADRENO_POWER_OPS(adreno_dev);

	/*
	 * Wait up to 1 second for the active count to go low
	 * and then start complaining about it
	 */
	if (kgsl_active_count_wait(device, 0, HZ)) {
		dev_err(device->dev,
			"Waiting for the active count to become 0\n");

		while (kgsl_active_count_wait(device, 0, HZ))
			dev_err(device->dev,
				"Still waiting for the active count\n");
	}

	return ops->last_close(adreno_dev);
}

static int adreno_pwrctrl_active_count_get(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	int ret = 0;

	if (WARN_ON(!mutex_is_locked(&device->mutex)))
		return -EINVAL;

	if ((atomic_read(&device->active_cnt) == 0) &&
		(device->state != KGSL_STATE_ACTIVE)) {
		mutex_unlock(&device->mutex);
		wait_for_completion(&device->hwaccess_gate);
		mutex_lock(&device->mutex);
		device->pwrctrl.superfast = true;
		ret = kgsl_pwrctrl_change_state(device, KGSL_STATE_ACTIVE);
	}
	if (ret == 0)
		atomic_inc(&device->active_cnt);
	trace_kgsl_active_count(device,
		(unsigned long) __builtin_return_address(0));
	return ret;
}

static void adreno_pwrctrl_active_count_put(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);

	if (WARN_ON(!mutex_is_locked(&device->mutex)))
		return;

	if (WARN(atomic_read(&device->active_cnt) == 0,
			"Unbalanced get/put calls to KGSL active count\n"))
		return;

	if (atomic_dec_and_test(&device->active_cnt)) {
		bool nap_on = !(device->pwrctrl.ctrl_flags &
			BIT(KGSL_PWRFLAGS_NAP_OFF));
		if (nap_on && device->state == KGSL_STATE_ACTIVE &&
			device->requested_state == KGSL_STATE_NONE) {
			kgsl_pwrctrl_request_state(device, KGSL_STATE_NAP);
			kgsl_schedule_work(&device->idle_check_ws);
		} else if (!nap_on) {
			kgsl_pwrscale_update_stats(device);
			kgsl_pwrscale_update(device);
		}

		kgsl_start_idle_timer(device);
	}

	trace_kgsl_active_count(device,
		(unsigned long) __builtin_return_address(0));

	wake_up(&device->active_cnt_wq);
}

int adreno_active_count_get(struct adreno_device *adreno_dev)
{
	const struct adreno_power_ops *ops = ADRENO_POWER_OPS(adreno_dev);

	return ops->active_count_get(adreno_dev);
}

void adreno_active_count_put(struct adreno_device *adreno_dev)
{
	const struct adreno_power_ops *ops = ADRENO_POWER_OPS(adreno_dev);

	ops->active_count_put(adreno_dev);
}

void adreno_get_bus_counters(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);

	if (!device->pwrctrl.bus_control)
		return;

	/* VBIF waiting for RAM */
	if (adreno_dev->starved_ram_lo == 0)
		if (adreno_perfcounter_get(adreno_dev,
			KGSL_PERFCOUNTER_GROUP_VBIF_PWR, 0,
			&adreno_dev->starved_ram_lo, NULL,
			PERFCOUNTER_FLAG_KERNEL))
			goto err;

	/* VBIF DDR cycles */
	if (!adreno_has_gbif(adreno_dev) && (adreno_dev->ram_cycles_lo == 0)) {
		if (adreno_perfcounter_get(adreno_dev,
			KGSL_PERFCOUNTER_GROUP_VBIF,
			VBIF_AXI_TOTAL_BEATS,
			&adreno_dev->ram_cycles_lo, NULL,
			PERFCOUNTER_FLAG_KERNEL))
			goto err;
		return;
	}

	if (adreno_dev->starved_ram_lo_ch1 == 0)
		if (adreno_perfcounter_get(adreno_dev,
			KGSL_PERFCOUNTER_GROUP_VBIF_PWR, 1,
			&adreno_dev->starved_ram_lo_ch1, NULL,
			PERFCOUNTER_FLAG_KERNEL))
			goto err;

	if (adreno_dev->ram_cycles_lo == 0)
		if (adreno_perfcounter_get(adreno_dev,
			KGSL_PERFCOUNTER_GROUP_VBIF,
			GBIF_AXI0_READ_DATA_TOTAL_BEATS,
			&adreno_dev->ram_cycles_lo, NULL,
			PERFCOUNTER_FLAG_KERNEL))
			goto err;

	if (adreno_dev->ram_cycles_lo_ch1_read == 0)
		if (adreno_perfcounter_get(adreno_dev,
			KGSL_PERFCOUNTER_GROUP_VBIF,
			GBIF_AXI1_READ_DATA_TOTAL_BEATS,
			&adreno_dev->ram_cycles_lo_ch1_read,
			NULL,
			PERFCOUNTER_FLAG_KERNEL))
			goto err;

	if (adreno_dev->ram_cycles_lo_ch0_write == 0)
		if (adreno_perfcounter_get(adreno_dev,
			KGSL_PERFCOUNTER_GROUP_VBIF,
			GBIF_AXI0_WRITE_DATA_TOTAL_BEATS,
			&adreno_dev->ram_cycles_lo_ch0_write,
			NULL,
			PERFCOUNTER_FLAG_KERNEL))
			goto err;

	if (adreno_dev->ram_cycles_lo_ch1_write == 0)
		if (adreno_perfcounter_get(adreno_dev,
			KGSL_PERFCOUNTER_GROUP_VBIF,
			GBIF_AXI1_WRITE_DATA_TOTAL_BEATS,
			&adreno_dev->ram_cycles_lo_ch1_write,
			NULL,
			PERFCOUNTER_FLAG_KERNEL))
			goto err;

	return;
err:
	dev_err(KGSL_DEVICE(adreno_dev)->dev,
		"Unable to get perf counters for bus DCVS\n");

}

void adreno_clear_dcvs_counters(struct adreno_device *adreno_dev)
{
	/* Clear the busy_data stats - we're starting over from scratch */
	adreno_dev->busy_data.gpu_busy = 0;
	adreno_dev->busy_data.bif_ram_cycles = 0;
	adreno_dev->busy_data.bif_ram_cycles_read_ch1 = 0;
	adreno_dev->busy_data.bif_ram_cycles_write_ch0 = 0;
	adreno_dev->busy_data.bif_ram_cycles_write_ch1 = 0;
	adreno_dev->busy_data.bif_starved_ram = 0;
	adreno_dev->busy_data.bif_starved_ram_ch1 = 0;
	adreno_dev->busy_data.num_ifpc = 0;
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
	const struct adreno_gpudev *gpudev = ADRENO_GPU_DEVICE(adreno_dev);
	int status;
	unsigned int state = device->state;
	bool regulator_left_on;

	/* make sure ADRENO_DEVICE_STARTED is not set here */
	WARN_ON(test_bit(ADRENO_DEVICE_STARTED, &adreno_dev->priv));

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

	adreno_ringbuffer_set_global(adreno_dev, 0);

	status = kgsl_mmu_start(device);
	if (status)
		goto error_pwr_off;

	adreno_get_bus_counters(adreno_dev);

	adreno_clear_dcvs_counters(adreno_dev);

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

	status = gpudev->rb_start(adreno_dev);
	if (status)
		goto error_pwr_off;

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

	return 0;

error_pwr_off:
	/* set the state back to original state */
	kgsl_pwrctrl_change_state(device, state);

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
	const struct adreno_gpudev *gpudev = ADRENO_GPU_DEVICE(adreno_dev);
	int error = 0;

	if (!test_bit(ADRENO_DEVICE_STARTED, &adreno_dev->priv))
		return 0;

	kgsl_pwrscale_update_stats(device);

	adreno_irqctrl(adreno_dev, 0);

	/* Save active coresight registers if applicable */
	adreno_coresight_stop(adreno_dev);

	/* Save physical performance counter values before GPU power down*/
	adreno_perfcounter_save(adreno_dev);

	if (gpudev->clear_pending_transactions)
		gpudev->clear_pending_transactions(adreno_dev);

	adreno_dispatcher_stop(adreno_dev);

	adreno_ringbuffer_stop(adreno_dev);

	if (!IS_ERR_OR_NULL(adreno_dev->gpu_llc_slice))
		llcc_slice_deactivate(adreno_dev->gpu_llc_slice);

	if (!IS_ERR_OR_NULL(adreno_dev->gpuhtw_llc_slice))
		llcc_slice_deactivate(adreno_dev->gpuhtw_llc_slice);

	adreno_set_active_ctxs_null(adreno_dev);

	clear_bit(ADRENO_DEVICE_STARTED, &adreno_dev->priv);

	return error;
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
	const struct adreno_gpudev *gpudev = ADRENO_GPU_DEVICE(adreno_dev);
	int ret = -EINVAL;
	int i;

	if (gpudev->reset)
		return gpudev->reset(device);

	/*
	 * Try soft reset first Do not do soft reset for a IOMMU fault (because
	 * the IOMMU hardware needs a reset too)
	 */

	if (!(fault & ADRENO_IOMMU_PAGE_FAULT))
		ret = adreno_soft_reset(device);

	if (ret) {
		/* If soft reset failed/skipped, then pull the power */
		kgsl_pwrctrl_change_state(device, KGSL_STATE_INIT);
		/* since device is officially off now clear start bit */
		clear_bit(ADRENO_DEVICE_STARTED, &adreno_dev->priv);

		/* Try to reset the device */
		ret = adreno_start(device, 0);

		for (i = 0; ret && i < NUM_TIMES_RESET_RETRY; i++) {
			msleep(20);
			ret = adreno_start(device, 0);
		}

		if (ret)
			return ret;

		if (i != 0)
			dev_warn(device->dev,
			      "Device hard reset tried %d tries\n", i);
	}

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
		.mmu_enabled = kgsl_mmu_has_feature(device, KGSL_MMU_PAGED),
		.gmem_gpubaseaddr = 0,
		.gmem_sizebytes = adreno_dev->gpucore->gmem_size,
	};

	return copy_prop(param, &devinfo, sizeof(devinfo));
}

static int adreno_prop_gpu_model(struct kgsl_device *device,
		struct kgsl_device_getproperty *param)
{
	struct kgsl_gpu_model model = {0};

	strlcpy(model.gpu_model, adreno_get_gpu_model(device),
			sizeof(model.gpu_model));

	return copy_prop(param, &model, sizeof(model));
}

static int adreno_prop_device_shadow(struct kgsl_device *device,
		struct kgsl_device_getproperty *param)
{
	struct kgsl_shadowprop shadowprop = { 0 };

	if (device->memstore->hostptr) {
		/* Pass a dummy address to identify memstore */
		shadowprop.gpuaddr =  KGSL_MEMSTORE_TOKEN_ADDRESS;
		shadowprop.size = device->memstore->size;

		shadowprop.flags = KGSL_FLAGS_INITIALIZED |
			KGSL_FLAGS_PER_CONTEXT_TIMESTAMPS;
	}

	return copy_prop(param, &shadowprop, sizeof(shadowprop));
}

static int adreno_prop_device_qdss_stm(struct kgsl_device *device,
		struct kgsl_device_getproperty *param)
{
	struct kgsl_qdss_stm_prop qdssprop = {0};

	if (!IS_ERR_OR_NULL(device->qdss_desc)) {
		qdssprop.gpuaddr = device->qdss_desc->gpuaddr;
		qdssprop.size = device->qdss_desc->size;
	}

	return copy_prop(param, &qdssprop, sizeof(qdssprop));
}

static int adreno_prop_device_qtimer(struct kgsl_device *device,
		struct kgsl_device_getproperty *param)
{
	struct kgsl_qtimer_prop qtimerprop = {0};

	if (!IS_ERR_OR_NULL(device->qtimer_desc)) {
		qtimerprop.gpuaddr = device->qtimer_desc->gpuaddr;
		qtimerprop.size = device->qtimer_desc->size;
	}

	return copy_prop(param, &qtimerprop, sizeof(qtimerprop));
}

static int adreno_prop_s32(struct kgsl_device *device,
		struct kgsl_device_getproperty *param)
{
	int val = 0;

	if (param->type == KGSL_PROP_MMU_ENABLE)
		val = kgsl_mmu_has_feature(device, KGSL_MMU_PAGED);
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
		val = device->speed_bin;
	else if (param->type == KGSL_PROP_VK_DEVICE_ID)
		val = adreno_get_vk_device_id(device);

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
	{ KGSL_PROP_GPU_MODEL, adreno_prop_gpu_model},
	{ KGSL_PROP_VK_DEVICE_ID, adreno_prop_u32},
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

static void adreno_force_on(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);

	if (gmu_core_isenabled(device)) {

		set_bit(GMU_DISABLE_SLUMBER, &device->gmu_core.flags);

		if (!adreno_active_count_get(adreno_dev))
			adreno_active_count_put(adreno_dev);

		return;
	}

	kgsl_pwrctrl_change_state(device, KGSL_STATE_ACTIVE);

	device->pwrctrl.ctrl_flags = KGSL_PWR_ON;

	adreno_fault_detect_stop(adreno_dev);
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
				if (gmu_core_isenabled(device))
					clear_bit(GMU_DISABLE_SLUMBER,
						&device->gmu_core.flags);
				else
					device->pwrctrl.ctrl_flags = 0;

				if (!adreno_active_count_get(adreno_dev)) {
					adreno_fault_detect_start(adreno_dev);
					adreno_active_count_put(adreno_dev);
				}

				kgsl_pwrscale_enable(device);
			} else {
				adreno_force_on(adreno_dev);
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
bool adreno_irq_pending(struct adreno_device *adreno_dev)
{
	unsigned int status;
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);

	if (gmu_core_isenabled(device))
		adreno_read_gmureg(adreno_dev,
			ADRENO_REG_GMU_AO_RBBM_INT_UNMASKED_STATUS, &status);
	else
		adreno_readreg(adreno_dev, ADRENO_REG_RBBM_INT_0_STATUS, &status);

	/*
	 * IRQ handler clears the RBBM INT0 status register immediately
	 * entering the ISR before actually serving the interrupt because
	 * of this we can't rely only on RBBM INT0 status only.
	 * Use pending_irq_refcnt along with RBBM INT0 to correctly
	 * determine whether any IRQ is pending or not.
	 */
	if ((status & adreno_dev->irq_mask) ||
		atomic_read(&adreno_dev->pending_irq_refcnt))
		return true;

	return false;
}

/*
 * adreno_soft_reset -  Do a soft reset of the GPU hardware
 * @device: KGSL device to soft reset
 *
 * "soft reset" the GPU hardware - this is a fast path GPU reset
 * The GPU hardware is reset but we never pull power so we can skip
 * a lot of the standard adreno_stop/adreno_start sequence
 */
static int adreno_soft_reset(struct kgsl_device *device)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	const struct adreno_gpudev *gpudev = ADRENO_GPU_DEVICE(adreno_dev);
	int ret;

	/*
	 * Don't allow a soft reset for a304 because the SMMU needs to be hard
	 * reset
	 */
	if (adreno_is_a304(adreno_dev))
		return -ENODEV;

	if (gpudev->clear_pending_transactions) {
		ret = gpudev->clear_pending_transactions(adreno_dev);
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

	_soft_reset(adreno_dev);

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

	/* Reinitialize the GPU */
	gpudev->start(adreno_dev);

	/* Re-initialize the coresight registers if applicable */
	adreno_coresight_start(adreno_dev);

	/* Enable IRQ */
	adreno_irqctrl(adreno_dev, 1);

	/* stop all ringbuffers to cancel RB events */
	adreno_ringbuffer_stop(adreno_dev);

	/* Start the ringbuffer(s) again */
	ret = gpudev->rb_start(adreno_dev);
	if (ret == 0) {
		device->reset_counter++;
		set_bit(ADRENO_DEVICE_STARTED, &adreno_dev->priv);
	}

	/* Restore physical performance counter values after soft reset */
	adreno_perfcounter_restore(adreno_dev);

	if (ret)
		dev_err(device->dev, "Device soft reset failed: %d\n", ret);

	return ret;
}

static bool adreno_isidle(struct adreno_device *adreno_dev)
{
	const struct adreno_gpudev *gpudev  = ADRENO_GPU_DEVICE(adreno_dev);
	struct adreno_ringbuffer *rb;
	int i;

	if (!kgsl_state_is_awake(KGSL_DEVICE(adreno_dev)))
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

	return gpudev->hw_isidle(adreno_dev);
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

		if (adreno_isidle(adreno_dev))
			return 0;

	} while (time_before(jiffies, wait));

	/*
	 * Under rare conditions, preemption can cause the while loop to exit
	 * without checking if the gpu is idle. check one last time before we
	 * return failure.
	 */
	if (adreno_gpu_fault(adreno_dev) != 0)
		return -EDEADLK;

	if (adreno_isidle(adreno_dev))
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
	if (adreno_isidle(adreno_dev))
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
int adreno_suspend_context(struct kgsl_device *device)
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

	/*
	 * kgsl panic notifier will be called in atomic context to get
	 * GPU snapshot. Also panic handler will skip snapshot dumping
	 * incase GPU is in SLUMBER state. So we can safely ignore the
	 * kgsl_pre_hwaccess().
	 */
	if (!device->snapshot_atomic && !in_interrupt())
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

	/*
	 * kgsl panic notifier will be called in atomic context to get
	 * GPU snapshot. Also panic handler will skip snapshot dumping
	 * incase GPU is in SLUMBER state. So we can safely ignore the
	 * kgsl_pre_hwaccess().
	 */
	if (!device->snapshot_atomic && !in_interrupt())
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

void adreno_read_gmu_wrapper(struct adreno_device *adreno_dev,
		u32 offsetwords, u32 *val)
{
	*val = __raw_readl(adreno_dev->gmu_wrapper_virt +
			(offsetwords << 2) - adreno_dev->gmu_wrapper_base);
	/* Order this read with respect to the following memory accesses */
	rmb();
}

void adreno_write_gmu_wrapper(struct adreno_device *adreno_dev,
		u32 offsetwords, u32 value)
{
	/*
	 * ensure previous writes post before this one,
	 * i.e. act like normal writel()
	 */
	wmb();
	__raw_writel(value, adreno_dev->gmu_wrapper_virt +
			(offsetwords << 2) - adreno_dev->gmu_wrapper_base);
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
	const struct adreno_gpudev *gpudev = ADRENO_GPU_DEVICE(adreno_dev);
	unsigned int reg_offset = gpudev->reg_offsets[offset];

	adreno_writereg(adreno_dev, offset, val);

	if (!gmu_core_isenabled(KGSL_DEVICE(adreno_dev)))
		return 0;

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

	if (i == GMU_CORE_LONG_WAKEUP_RETRY_LIMIT) {
		dev_err(adreno_dev->dev.dev,
			"Timed out waiting %d usecs to write fenced register 0x%x\n",
			i * GMU_CORE_WAKEUP_DELAY_US,
			reg_offset);
		return -ETIMEDOUT;
	}

	dev_err(adreno_dev->dev.dev,
		"Waited %d usecs to write fenced register 0x%x\n",
		i * GMU_CORE_WAKEUP_DELAY_US,
		reg_offset);

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

void adreno_profile_submit_time(struct adreno_submit_time *time)
{
	struct kgsl_drawobj *drawobj;
	struct kgsl_drawobj_cmd *cmdobj;
	struct kgsl_mem_entry *entry;
	struct kgsl_drawobj_profiling_buffer *profile_buffer;

	drawobj = time->drawobj;
	if (drawobj == NULL)
		return;

	cmdobj = CMDOBJ(drawobj);
	entry = cmdobj->profiling_buf_entry;
	if (!entry)
		return;

	profile_buffer = kgsl_gpuaddr_to_vaddr(&entry->memdesc,
			cmdobj->profiling_buffer_gpuaddr);

	if (profile_buffer == NULL)
		return;

	/* Return kernel clock time to the client if requested */
	if (drawobj->flags & KGSL_DRAWOBJ_PROFILING_KTIME) {
		u64 secs = time->ktime;

		profile_buffer->wall_clock_ns =
			do_div(secs, NSEC_PER_SEC);
		profile_buffer->wall_clock_s = secs;
	} else {
		profile_buffer->wall_clock_s = time->utime.tv_sec;
		profile_buffer->wall_clock_ns = time->utime.tv_nsec;
	}

	profile_buffer->gpu_ticks_queued = time->ticks;

	kgsl_memdesc_unmap(&entry->memdesc);
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
		kgsl_sharedmem_readl(device->memstore, timestamp,
			KGSL_MEMSTORE_OFFSET(index, soptimestamp));
		break;
	case KGSL_TIMESTAMP_RETIRED:
		kgsl_sharedmem_readl(device->memstore, timestamp,
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
static struct kgsl_device_private *adreno_device_private_create(void)
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
static void adreno_device_private_destroy(struct kgsl_device_private *dev_priv)
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
	const struct adreno_gpudev *gpudev  = ADRENO_GPU_DEVICE(adreno_dev);
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;
	struct adreno_busy_data *busy = &adreno_dev->busy_data;
	int64_t adj = 0;
	u64 gpu_busy = 0;
	unsigned int val;

	memset(stats, 0, sizeof(*stats));

	if (adreno_is_a619_holi(adreno_dev))
		adreno_read_gmu_wrapper(adreno_dev,
			adreno_dev->perfctr_pwr_lo, &val);
	else
		kgsl_regread(device, adreno_dev->perfctr_pwr_lo, &val);

	if (busy->gpu_busy)
		gpu_busy = (val >= busy->gpu_busy) ? val - busy->gpu_busy :
			(UINT_MAX - busy->gpu_busy) + val;

	busy->gpu_busy = val;

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
		s64 freq = kgsl_pwrctrl_active_freq(pwr) / 1000000;
		do_div(gpu_busy, freq);
		stats->busy_time = gpu_busy;
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
	const struct adreno_gpudev *gpudev  = ADRENO_GPU_DEVICE(adreno_dev);

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

static bool adreno_prepare_for_power_off(struct kgsl_device *device)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	const struct adreno_gpudev *gpudev  = ADRENO_GPU_DEVICE(adreno_dev);

	/*
	 * Skip power collapse for A304, if power ctrl flag is set to
	 * non zero. As A304 soft_reset will not work, power collapse
	 * needs to disable to avoid soft_reset.
	 */
	if (adreno_is_a304(adreno_dev) &&
			device->pwrctrl.ctrl_flags)
		return false;

	if (!adreno_isidle(adreno_dev) || !(gpudev->is_sptp_idle ?
				gpudev->is_sptp_idle(adreno_dev) : true))
		return false;

	if (gpudev->clear_pending_transactions(adreno_dev))
		return false;

	adreno_dispatcher_stop_fault_timer(device);

	return true;
}

static void adreno_regulator_disable(struct kgsl_device *device)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	const struct adreno_gpudev *gpudev  = ADRENO_GPU_DEVICE(adreno_dev);

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
	const struct adreno_gpudev *gpudev  = ADRENO_GPU_DEVICE(adreno_dev);

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

static void adreno_regulator_disable_poll(struct kgsl_device *device)
{
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;
	const struct adreno_gpudev *gpudev = ADRENO_GPU_DEVICE(
						ADRENO_DEVICE(device));

	if (gpudev->regulator_disable_poll)
		return gpudev->regulator_disable_poll(device);

	if (!kgsl_regulator_disable_wait(pwr->gx_gdsc, 200))
		dev_err(device->dev, "Regulator vdd is stuck on\n");

	if (!kgsl_regulator_disable_wait(pwr->cx_gdsc, 200))
		dev_err(device->dev, "Regulator vddcx is stuck on\n");
}

static void adreno_gpu_model(struct kgsl_device *device, char *str,
				size_t bufsz)
{
	scnprintf(str, bufsz, adreno_get_gpu_model(device));
}

static bool adreno_is_hwcg_on(struct kgsl_device *device)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);

	return adreno_dev->hwcg_enabled;
}

static int adreno_queue_cmds(struct kgsl_device_private *dev_priv,
	struct kgsl_context *context, struct kgsl_drawobj *drawobj[],
	u32 count, u32 *timestamp)
{
	struct kgsl_device *device = dev_priv->device;

	if (test_bit(GMU_DISPATCH, &device->gmu_core.flags))
		return adreno_hwsched_queue_cmds(dev_priv, context, drawobj,
				count, timestamp);

	return adreno_dispatcher_queue_cmds(dev_priv, context, drawobj, count,
			timestamp);
}

static void adreno_drawctxt_sched(struct kgsl_device *device,
		struct kgsl_context *context)
{
	if (test_bit(GMU_DISPATCH, &device->gmu_core.flags))
		return adreno_hwsched_queue_context(device,
			ADRENO_CONTEXT(context));

	adreno_dispatcher_queue_context(device, ADRENO_CONTEXT(context));
}

int adreno_power_cycle(struct adreno_device *adreno_dev,
	void (*callback)(struct adreno_device *adreno_dev, void *priv),
	void *priv)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	const struct adreno_power_ops *ops = ADRENO_POWER_OPS(adreno_dev);
	int ret;

	mutex_lock(&device->mutex);
	ret = ops->pm_suspend(adreno_dev);

	if (!ret) {
		callback(adreno_dev, priv);
		ops->pm_resume(adreno_dev);
	}

	mutex_unlock(&device->mutex);

	return ret;
}

int adreno_power_cycle_bool(struct adreno_device *adreno_dev,
	bool *flag, bool val)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	const struct adreno_power_ops *ops = ADRENO_POWER_OPS(adreno_dev);
	int ret;

	mutex_lock(&device->mutex);
	ret = ops->pm_suspend(adreno_dev);

	if (!ret) {
		*flag = val;
		ops->pm_resume(adreno_dev);
	}

	mutex_unlock(&device->mutex);

	return ret;
}

int adreno_power_cycle_u32(struct adreno_device *adreno_dev,
	u32 *flag, u32 val)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	const struct adreno_power_ops *ops = ADRENO_POWER_OPS(adreno_dev);
	int ret;

	mutex_lock(&device->mutex);
	ret = ops->pm_suspend(adreno_dev);

	if (!ret) {
		*flag = val;
		ops->pm_resume(adreno_dev);
	}

	mutex_unlock(&device->mutex);

	return ret;
}

static int adreno_gpu_clock_set(struct kgsl_device *device, u32 pwrlevel)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	const struct adreno_power_ops *ops = ADRENO_POWER_OPS(adreno_dev);
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;
	struct kgsl_pwrlevel *pl = &pwr->pwrlevels[pwrlevel];
	int ret;

	if (ops->gpu_clock_set)
		return ops->gpu_clock_set(adreno_dev, pwrlevel);

	ret = clk_set_rate(pwr->grp_clks[0], pl->gpu_freq);
	if (ret)
		dev_err(device->dev, "GPU clk freq set failure: %d\n", ret);

	return ret;
}

static int adreno_interconnect_bus_set(struct adreno_device *adreno_dev,
	int level, u32 ab)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;

	if ((level == pwr->cur_buslevel) && (ab == pwr->cur_ab))
		return 0;

	pwr->cur_buslevel = level;
	pwr->cur_ab = ab;

	icc_set_bw(pwr->icc_path, MBps_to_icc(ab),
		kBps_to_icc(pwr->ddr_table[level]));

	trace_kgsl_buslevel(device, pwr->active_pwrlevel, level);

	return 0;
}

static int adreno_gpu_bus_set(struct kgsl_device *device, int level, u32 ab)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	const struct adreno_power_ops *ops = ADRENO_POWER_OPS(adreno_dev);

	if (ops->gpu_bus_set)
		return ops->gpu_bus_set(adreno_dev, level, ab);

	return adreno_interconnect_bus_set(adreno_dev, level, ab);
}

static void adreno_deassert_gbif_halt(struct kgsl_device *device)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	const struct adreno_gpudev *gpudev = ADRENO_GPU_DEVICE(adreno_dev);

	if (gpudev->deassert_gbif_halt)
		gpudev->deassert_gbif_halt(adreno_dev);
}

static const struct kgsl_functable adreno_functable = {
	/* Mandatory functions */
	.regread = adreno_regread,
	.regwrite = adreno_regwrite,
	.idle = adreno_idle,
	.suspend_context = adreno_suspend_context,
	.first_open = adreno_first_open,
	.start = adreno_start,
	.stop = adreno_stop,
	.last_close = adreno_last_close,
	.getproperty = adreno_getproperty,
	.getproperty_compat = adreno_getproperty_compat,
	.waittimestamp = adreno_waittimestamp,
	.readtimestamp = adreno_readtimestamp,
	.queue_cmds = adreno_queue_cmds,
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
	.prepare_for_power_off = adreno_prepare_for_power_off,
	.regulator_disable = adreno_regulator_disable,
	.pwrlevel_change_settings = adreno_pwrlevel_change_settings,
	.regulator_disable_poll = adreno_regulator_disable_poll,
	.clk_set_options = adreno_clk_set_options,
	.gpu_model = adreno_gpu_model,
	.query_property_list = adreno_query_property_list,
	.is_hwcg_on = adreno_is_hwcg_on,
	.gpu_clock_set = adreno_gpu_clock_set,
	.gpu_bus_set = adreno_gpu_bus_set,
	.deassert_gbif_halt = adreno_deassert_gbif_halt,
};

static const struct component_master_ops adreno_ops = {
	.bind = adreno_bind,
	.unbind = adreno_unbind,
};

const struct adreno_power_ops adreno_power_operations = {
	.first_open = adreno_open,
	.last_close = adreno_close,
	.active_count_get = adreno_pwrctrl_active_count_get,
	.active_count_put = adreno_pwrctrl_active_count_put,
	.pm_suspend = adreno_suspend,
	.pm_resume = adreno_resume,
	.touch_wakeup = adreno_touch_wakeup,
};

static int _compare_of(struct device *dev, void *data)
{
	return (dev->of_node == data);
}

static void _release_of(struct device *dev, void *data)
{
	of_node_put(data);
}

static void adreno_add_gmu_components(struct device *dev,
		struct component_match **match)
{
	struct device_node *node;

	node = of_find_matching_node(NULL, adreno_gmu_match);
	if (!node)
		return;

	if (!of_device_is_available(node)) {
		of_node_put(node);
		return;
	}

	component_match_add_release(dev, match, _release_of,
		_compare_of, node);
}

static int adreno_probe(struct platform_device *pdev)
{
	struct component_match *match = NULL;

	adreno_add_gmu_components(&pdev->dev, &match);

	if (match)
		return component_master_add_with_match(&pdev->dev,
				&adreno_ops, match);
	else
		return adreno_bind(&pdev->dev);
}

static int adreno_remove(struct platform_device *pdev)
{
	if (of_find_matching_node(NULL, adreno_gmu_match))
		component_master_del(&pdev->dev, &adreno_ops);
	else
		adreno_unbind(&pdev->dev);

	return 0;
}

static const struct dev_pm_ops adreno_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(adreno_pm_suspend, adreno_pm_resume)
};

static struct platform_driver adreno_platform_driver = {
	.probe = adreno_probe,
	.remove = adreno_remove,
	.driver = {
		.name = "kgsl-3d",
		.pm = &adreno_pm_ops,
		.of_match_table = of_match_ptr(adreno_match_table),
	}
};

static int __init kgsl_3d_init(void)
{
	int ret;

	ret = kgsl_core_init();
	if (ret)
		return ret;

	gmu_core_register();
	ret = platform_driver_register(&adreno_platform_driver);
	if (ret)
		kgsl_core_exit();

	return ret;
}

static void __exit kgsl_3d_exit(void)
{
	platform_driver_unregister(&adreno_platform_driver);
	gmu_core_unregister();
	kgsl_core_exit();
}

module_init(kgsl_3d_init);
module_exit(kgsl_3d_exit);

MODULE_DESCRIPTION("3D Graphics driver");
MODULE_LICENSE("GPL v2");
MODULE_SOFTDEP("pre: qcom-arm-smmu-mod nvmem_qfprom");
