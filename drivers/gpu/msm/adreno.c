// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2002,2007-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
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
#include <linux/msm_kgsl.h>
#include <linux/regulator/consumer.h>
#include <linux/nvmem-consumer.h>
#include <linux/reset.h>
#include <linux/soc/qcom/llcc-qcom.h>
#include <linux/trace.h>
#include <soc/qcom/dcvs.h>

#include "adreno.h"
#include "adreno_a3xx.h"
#include "adreno_a5xx.h"
#include "adreno_a6xx.h"
#include "adreno_compat.h"
#include "adreno_pm4types.h"
#include "adreno_trace.h"
#include "kgsl_bus.h"
#include "kgsl_reclaim.h"
#include "kgsl_trace.h"
#include "kgsl_util.h"

/* Include the master list of GPU cores that are supported */
#include "adreno-gpulist.h"

static void adreno_unbind(struct device *dev);
static void adreno_input_work(struct work_struct *work);
static int adreno_soft_reset(struct kgsl_device *device);
static unsigned int counter_delta(struct kgsl_device *device,
	unsigned int reg, unsigned int *counter);
static struct device_node *
	adreno_get_gpu_model_node(struct platform_device *pdev);

static struct adreno_device device_3d0;

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
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	int ret;

	if (!name || adreno_dev->zap_loaded)
		return 0;

	ret = kgsl_zap_shader_load(&device->pdev->dev, name);
	if (!ret)
		adreno_dev->zap_loaded = true;

	return ret;
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
 * adreno_get_rptr() - Get the current ringbuffer read pointer
 * @rb: Pointer the ringbuffer to query
 *
 * Get the latest rptr
 */
unsigned int adreno_get_rptr(struct adreno_ringbuffer *rb)
{
	struct adreno_device *adreno_dev = ADRENO_RB_DEVICE(rb);
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	u32 rptr = 0;

	if (adreno_is_a3xx(adreno_dev))
		kgsl_regread(device, A3XX_CP_RB_RPTR, &rptr);
	else
		kgsl_sharedmem_readl(device->scratch, &rptr,
				SCRATCH_RB_OFFSET(rb->id, rptr));

	return rptr;
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

	adreno_dev->wake_on_touch = true;

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

	if (adreno_dev->wake_on_touch)
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
		adreno_dev->wake_on_touch = true;
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

	clear_bit(ADRENO_DEVICE_GPU_REGULATOR_ENABLED, &adreno_dev->priv);
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
	adreno_dispatcher_fault(adreno_dev, ADRENO_HARD_FAULT);
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

static irqreturn_t adreno_irq_handler(int irq, void *data)
{
	struct kgsl_device *device = data;
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

static irqreturn_t adreno_freq_limiter_irq_handler(int irq, void *data)
{
	struct kgsl_device *device = data;
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;

	dev_err_ratelimited(device->dev,
		"Max GPU freq supported:%u, but requested freq:%u from prev freq:%u\n",
		device->speed_bin ? (device->speed_bin - 2) * 4800000 :
		pwr->pwrlevels[0].gpu_freq,
		pwr->pwrlevels[pwr->active_pwrlevel].gpu_freq,
		pwr->pwrlevels[pwr->previous_pwrlevel].gpu_freq);

	reset_control_reset(device->freq_limiter_irq_clear);

	return IRQ_HANDLED;
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

static int adreno_get_chipid(struct platform_device *pdev, u32 *chipid);

static inline bool _rev_match(unsigned int id, unsigned int entry)
{
	return (entry == ANY_ID || entry == id);
}

static const struct adreno_gpu_core *
_get_gpu_core(struct platform_device *pdev, u32 *chipid)
{
	int i;
	struct device_node *node;

	/*
	 * When "qcom,gpu-models" is defined, use gpu model node to match
	 * on a compatible string, otherwise match using legacy way.
	 */
	node = adreno_get_gpu_model_node(pdev);
	if (!node || !of_find_property(node, "compatible", NULL))
		node = pdev->dev.of_node;

	*chipid = 0;

	/* Check to see if any of the entries match on a compatible string */
	for (i = 0; i < ARRAY_SIZE(adreno_gpulist); i++) {
		if (adreno_gpulist[i]->compatible &&
				of_device_is_compatible(node,
					adreno_gpulist[i]->compatible)) {
			/*
			 * We matched compat string, set chipid based on
			 * dtsi, then gpulist, else fail.
			 */
			if (adreno_get_chipid(pdev, chipid))
				*chipid = adreno_gpulist[i]->chipid;

			if (*chipid)
				return adreno_gpulist[i];

			dev_crit(&pdev->dev,
					"No chipid associated with %s\n",
					adreno_gpulist[i]->compatible);
			return NULL;
		}
	}

	/* No compatible string so try and match on chipid */
	if (!adreno_get_chipid(pdev, chipid)) {
		unsigned int core = ADRENO_CHIPID_CORE(*chipid);
		unsigned int major = ADRENO_CHIPID_MAJOR(*chipid);
		unsigned int minor = ADRENO_CHIPID_MINOR(*chipid);
		unsigned int patchid = ADRENO_CHIPID_PATCH(*chipid);

		for (i = 0; i < ARRAY_SIZE(adreno_gpulist); i++) {
			if (core == adreno_gpulist[i]->core &&
				_rev_match(major, adreno_gpulist[i]->major) &&
				_rev_match(minor, adreno_gpulist[i]->minor) &&
				_rev_match(patchid, adreno_gpulist[i]->patchid))
				return adreno_gpulist[i];
		}
	}

	dev_crit(&pdev->dev, "Unknown GPU chip ID %8.8x\n", *chipid);
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

static int adreno_get_chipid(struct platform_device *pdev, u32 *chipid)
{
	return of_property_read_u32(pdev->dev.of_node, "qcom,chipid", chipid);
}

static void
adreno_update_soc_hw_revision_quirks(struct adreno_device *adreno_dev,
		struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	int i;

	/* update quirk */
	for (i = 0; i < ARRAY_SIZE(adreno_quirks); i++) {
		if (of_property_read_bool(node, adreno_quirks[i].prop))
			adreno_dev->quirks |= adreno_quirks[i].quirk;
	}
}

static const struct adreno_gpu_core *
adreno_identify_gpu(struct platform_device *pdev, u32 *chipid)
{
	const struct adreno_gpu_core *gpucore;

	gpucore = _get_gpu_core(pdev, chipid);
	if (!gpucore)
		return ERR_PTR(-ENODEV);

	/*
	 * Identify non-longer supported targets and spins and print a helpful
	 * message
	 */
	if (gpucore->features & ADRENO_DEPRECATED) {
		if (gpucore->compatible)
			dev_err(&pdev->dev,
				"Support for GPU %s has been deprecated\n",
				gpucore->compatible);
		else
			dev_err(&pdev->dev,
				"Support for GPU %x.%d.%x.%d has been deprecated\n",
				gpucore->core, gpucore->major,
				gpucore->minor, gpucore->patchid);
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

		if (index >= ARRAY_SIZE(pwr->pwrlevels)) {
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
		level->cx_level = 0xffffffff;

		of_property_read_u32(child, "qcom,acd-level",
			&level->acd_level);

		of_property_read_u32(child, "qcom,cx-level",
			&level->cx_level);

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

static void adreno_of_get_initial_pwrlevels(struct kgsl_pwrctrl *pwr,
		struct device_node *node)
{
	int level;

	/* Get and set the initial power level */
	if (of_property_read_u32(node, "qcom,initial-pwrlevel", &level))
		level = 1;

	if (level < 0 || level >= pwr->num_pwrlevels)
		level = 1;

	pwr->active_pwrlevel = level;
	pwr->default_pwrlevel = level;

	/* Set the max power level */
	pwr->max_pwrlevel = 0;

	/* Get and set the min power level */
	if (of_property_read_u32(node, "qcom,initial-min-pwrlevel", &level))
		level = pwr->num_pwrlevels - 1;

	if (level < 0 || level >= pwr->num_pwrlevels || level < pwr->default_pwrlevel)
		level = pwr->num_pwrlevels - 1;

	pwr->min_render_pwrlevel = level;
	pwr->min_pwrlevel = level;
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
		adreno_of_get_initial_pwrlevels(&device->pwrctrl, parent);
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

			adreno_of_get_initial_pwrlevels(&device->pwrctrl, child);

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
		"GPU speed_bin:%d mismatch for bin:%d\n",
		device->speed_bin, bin);
	return -ENODEV;
}

static int register_l3_voter(struct kgsl_device *device)
{
	int ret = 0;

	mutex_lock(&device->mutex);

	if (!device->l3_vote)
		goto done;

	/* This indicates that we are already set up */
	if (device->num_l3_pwrlevels != 0)
		goto done;

	memset(device->l3_freq, 0x0, sizeof(device->l3_freq));

	ret = qcom_dcvs_register_voter(KGSL_L3_DEVICE, DCVS_L3, DCVS_SLOW_PATH);
	if (ret) {
		dev_err_once(&device->pdev->dev,
			"Unable to register l3 dcvs voter: %d\n", ret);
		goto done;
	}

	ret = qcom_dcvs_hw_minmax_get(DCVS_L3, &device->l3_freq[1],
		&device->l3_freq[2]);
	if (ret) {
		dev_err_once(&device->pdev->dev,
			"Unable to get min/max for l3 dcvs: %d\n", ret);
		qcom_dcvs_unregister_voter(KGSL_L3_DEVICE, DCVS_L3,
			DCVS_SLOW_PATH);
		memset(device->l3_freq, 0x0, sizeof(device->l3_freq));
		goto done;
	}

	device->num_l3_pwrlevels = 3;

done:
	mutex_unlock(&device->mutex);

	return ret;
}

static int adreno_of_get_power(struct adreno_device *adreno_dev,
		struct platform_device *pdev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	int ret;

	ret = adreno_of_get_pwrlevels(adreno_dev, pdev->dev.of_node);
	if (ret)
		return ret;

	device->pwrctrl.interval_timeout = CONFIG_QCOM_KGSL_IDLE_TIMEOUT;

	device->pwrctrl.minbw_timeout = 10;

	/* Set default bus control to true on all targets */
	device->pwrctrl.bus_control = true;

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

	adreno_dev->cx_dbgc_base = res->start - device->regmap.base->start;
	adreno_dev->cx_dbgc_len = resource_size(res);
	adreno_dev->cx_dbgc_virt = devm_ioremap(&device->pdev->dev,
			device->regmap.base->start +
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

	adreno_dev->isense_base = res->start - device->regmap.base->start;
	adreno_dev->isense_len = resource_size(res);
	adreno_dev->isense_virt = devm_ioremap(&device->pdev->dev, res->start,
					adreno_dev->isense_len);
	if (adreno_dev->isense_virt == NULL)
		dev_warn(device->dev, "isense ioremap failed\n");
}

/* Read the fuse through the new and fancy nvmem method */
static int adreno_read_speed_bin(struct platform_device *pdev)
{
	struct nvmem_cell *cell = nvmem_cell_get(&pdev->dev, "speed_bin");
	int ret = PTR_ERR_OR_ZERO(cell);
	void *buf;
	int val = 0;
	size_t len;

	if (ret) {
		if (ret == -ENOENT)
			return 0;

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
	void *buf;
	int val = 0;
	size_t len;

	if (IS_ERR(cell))
		return PTR_ERR(cell);

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

const char *adreno_get_gpu_model(struct kgsl_device *device)
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

	if (adreno_is_a621(adreno_dev)) {
		/* Get the system cache slice descriptor for GPU MV grid buffer */
		adreno_dev->gpumv_llc_slice = llcc_slice_getd(LLCC_GPUMV);
		ret = PTR_ERR_OR_ZERO(adreno_dev->gpumv_llc_slice);
		if (ret) {
			if (ret == -EPROBE_DEFER) {
				llcc_slice_putd(adreno_dev->gpu_llc_slice);
				llcc_slice_putd(adreno_dev->gpuhtw_llc_slice);
				return ret;
			}

			if (ret != -ENOENT)
				dev_warn(&pdev->dev,
					"Unable to get GPUMV buffer slice: %d\n", ret);
		} else
			adreno_dev->gpumv_llc_slice_enable = true;
	}

	return 0;
}
#else
static int adreno_probe_llcc(struct adreno_device *adreno_dev,
		struct platform_device *pdev)
{
	return 0;
}
#endif

static void adreno_regmap_op_preaccess(struct kgsl_regmap_region *region)
{
	struct kgsl_device *device = region->priv;
	/*
	 * kgsl panic notifier will be called in atomic context to get
	 * GPU snapshot. Also panic handler will skip snapshot dumping
	 * incase GPU is in SLUMBER state. So we can safely ignore the
	 * kgsl_pre_hwaccess().
	 */
	if (!device->snapshot_atomic && !in_interrupt())
		kgsl_pre_hwaccess(device);
}

static const struct kgsl_regmap_ops adreno_regmap_ops = {
	.preaccess = adreno_regmap_op_preaccess,
};

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
	 * Some GPUs needs specific alignment for UCHE GMEM base address.
	 * Configure UCHE GMEM base based on GMEM size and align it accordingly.
	 * This needs to be done based on GMEM size to avoid overlap between
	 * RB and UCHE GMEM range.
	 */
	if (adreno_dev->gpucore->uche_gmem_alignment)
		adreno_dev->uche_gmem_base =
			ALIGN(adreno_dev->gpucore->gmem_size,
				adreno_dev->gpucore->uche_gmem_alignment);
}

static const struct of_device_id adreno_component_match[] = {
	{ .compatible = "qcom,gen7-gmu" },
	{ .compatible = "qcom,gpu-gmu" },
	{ .compatible = "qcom,gpu-rgmu" },
	{ .compatible = "qcom,kgsl-smmu-v2" },
	{ .compatible = "qcom,smmu-kgsl-cb" },
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

	/* Initialize the adreno device structure */
	adreno_setup_device(adreno_dev);

	dev_set_drvdata(dev, device);

	device->pdev = pdev;

	adreno_update_soc_hw_revision_quirks(adreno_dev, pdev);

	status = adreno_read_speed_bin(pdev);
	if (status < 0)
		goto err;

	device->speed_bin = status;

	status = adreno_of_get_power(adreno_dev, pdev);
	if (status)
		goto err;

	status = kgsl_bus_init(device, pdev);
	if (status)
		goto err;

	status = kgsl_regmap_init(pdev, &device->regmap, "kgsl_3d0_reg_memory",
		&adreno_regmap_ops, device);
	if (status)
		goto err_bus_close;

	/*
	 * The SMMU APIs use unsigned long for virtual addresses which means
	 * that we cannot use 64 bit virtual addresses on a 32 bit kernel even
	 * though the hardware and the rest of the KGSL driver supports it.
	 */
	if (adreno_support_64bit(adreno_dev))
		kgsl_mmu_set_feature(device, KGSL_MMU_64BIT);

	/*
	 * Set the SMMU aperture on A6XX/Gen7 targets to use per-process
	 * pagetables.
	 */
	if (ADRENO_GPUREV(adreno_dev) >= 600)
		kgsl_mmu_set_feature(device, KGSL_MMU_SMMU_APERTURE);

	if (ADRENO_FEATURE(adreno_dev, ADRENO_IOCOHERENT))
		kgsl_mmu_set_feature(device, KGSL_MMU_IO_COHERENT);

	device->pwrctrl.bus_width = adreno_dev->gpucore->bus_width;

	device->mmu.secured = (IS_ENABLED(CONFIG_QCOM_SECURE_BUFFER) &&
		ADRENO_FEATURE(adreno_dev, ADRENO_CONTENT_PROTECTION));

	/* Probe the LLCC - this could return -EPROBE_DEFER */
	status = adreno_probe_llcc(adreno_dev, pdev);
	if (status)
		goto err_bus_close;

	/*
	 * IF the GPU HTW slice was successsful set the MMU feature so the
	 * domain can set the appropriate attributes
	 */
	if (!IS_ERR_OR_NULL(adreno_dev->gpuhtw_llc_slice))
		kgsl_mmu_set_feature(device, KGSL_MMU_LLCC_ENABLE);

	 /* Bind the components before doing the KGSL platform probe. */
	status = component_bind_all(dev, NULL);
	if (status)
		goto err_remove_llcc;

	status = kgsl_request_irq(pdev, "kgsl_3d0_irq", adreno_irq_handler, device);
	if (status < 0)
		goto err_unbind;

	device->pwrctrl.interrupt_num = status;

	device->freq_limiter_intr_num = kgsl_request_irq(pdev, "freq_limiter_irq",
				adreno_freq_limiter_irq_handler, device);

	device->freq_limiter_irq_clear =
		devm_reset_control_get(&pdev->dev, "freq_limiter_irq_clear");

	status = kgsl_device_platform_probe(device);
	if (status)
		goto err_unbind;

	adreno_fence_trace_array_init(device);

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
		trace_array_put(device->fence_trace_array);
		kgsl_device_platform_remove(device);
		goto err_unbind;
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

	adreno_dev->perfcounter = false;

	adreno_sysfs_init(adreno_dev);

	/* Ignore return value, as driver can still function without pwrscale enabled */
	kgsl_pwrscale_init(device, pdev, CONFIG_QCOM_ADRENO_DEFAULT_GOVERNOR);

	/* Initialize coresight for the target */
	adreno_coresight_init(adreno_dev);

	if (ADRENO_FEATURE(adreno_dev, ADRENO_L3_VOTE))
		device->l3_vote = true;

#ifdef CONFIG_INPUT

	if (!of_property_read_bool(pdev->dev.of_node,
			"qcom,disable-wake-on-touch")) {
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

	kgsl_qcom_va_md_register(device);

	return 0;

err_unbind:
	component_unbind_all(dev, NULL);

err_remove_llcc:
	if (!IS_ERR_OR_NULL(adreno_dev->gpu_llc_slice))
		llcc_slice_putd(adreno_dev->gpu_llc_slice);

	if (!IS_ERR_OR_NULL(adreno_dev->gpuhtw_llc_slice))
		llcc_slice_putd(adreno_dev->gpuhtw_llc_slice);

	if (!IS_ERR_OR_NULL(adreno_dev->gpumv_llc_slice))
		llcc_slice_putd(adreno_dev->gpumv_llc_slice);

err_bus_close:
	kgsl_bus_close(device);

err:
	device->pdev = NULL;
	dev_err(&pdev->dev, "%s failed ret %d\n", __func__, status);
	return status;
}

static int adreno_bind(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	const struct adreno_gpu_core *gpucore;
	int ret;
	u32 chipid;

	gpucore = adreno_identify_gpu(pdev, &chipid);
	if (IS_ERR(gpucore))
		return PTR_ERR(gpucore);

	ret = gpucore->gpudev->probe(pdev, chipid, gpucore);

	if (!ret) {
		struct kgsl_device *device = dev_get_drvdata(dev);

		device->pdev_loaded = true;
		srcu_init_notifier_head(&device->nh);
	} else {
		/*
		 * Handle resource clean up through unbind, instead of a
		 * lengthy goto error path.
		 */
		dev_err(dev, "%s failed ret %d unbinding...\n", __func__, ret);
		adreno_unbind(dev);
	}

	return ret;
}

static void adreno_unbind(struct device *dev)
{
	struct adreno_device *adreno_dev;
	struct kgsl_device *device;
	const struct adreno_gpudev *gpudev;

	device = dev_get_drvdata(dev);
	if (!device)
		return;

	/* Return if cleanup happens in adreno_device_probe */
	if (!device->pdev)
		return;

	if (device->pdev_loaded) {
		srcu_cleanup_notifier_head(&device->nh);
		device->pdev_loaded = false;
	}

	adreno_dev = ADRENO_DEVICE(device);
	gpudev = ADRENO_GPU_DEVICE(adreno_dev);

	trace_array_put(device->fence_trace_array);

	if (gpudev->remove != NULL)
		gpudev->remove(adreno_dev);

#ifdef CONFIG_INPUT
	if (adreno_input_handler.private)
		input_unregister_handler(&adreno_input_handler);
#endif

	kgsl_qcom_va_md_unregister(device);
	adreno_coresight_remove(adreno_dev);
	adreno_profile_close(adreno_dev);

	/* Release the system cache slice descriptor */
	if (!IS_ERR_OR_NULL(adreno_dev->gpu_llc_slice))
		llcc_slice_putd(adreno_dev->gpu_llc_slice);

	if (!IS_ERR_OR_NULL(adreno_dev->gpuhtw_llc_slice))
		llcc_slice_putd(adreno_dev->gpuhtw_llc_slice);

	if (!IS_ERR_OR_NULL(adreno_dev->gpumv_llc_slice))
		llcc_slice_putd(adreno_dev->gpumv_llc_slice);

	kgsl_pwrscale_close(device);

	if (adreno_dev->dispatch_ops && adreno_dev->dispatch_ops->close)
		adreno_dev->dispatch_ops->close(adreno_dev);

	kgsl_device_platform_remove(device);

	component_unbind_all(dev, NULL);

	kgsl_bus_close(device);
	device->pdev = NULL;

	if (device->num_l3_pwrlevels != 0)
		qcom_dcvs_unregister_voter(KGSL_L3_DEVICE, DCVS_L3,
			DCVS_SLOW_PATH);

	clear_bit(ADRENO_DEVICE_PWRON_FIXUP, &adreno_dev->priv);
	clear_bit(ADRENO_DEVICE_INITIALIZED, &adreno_dev->priv);
}

static void adreno_resume(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);

	if (device->state == KGSL_STATE_SUSPEND) {
		adreno_put_gpu_halt(adreno_dev);
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

	kgsl_reclaim_start();
	return 0;
}

static int adreno_suspend(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	int status = kgsl_pwrctrl_change_state(device, KGSL_STATE_SUSPEND);

	if (!status && device->state == KGSL_STATE_SUSPEND)
		adreno_get_gpu_halt(adreno_dev);

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

	if (status)
		return status;

	kgsl_reclaim_close();
	flush_workqueue(device->events_wq);
	flush_workqueue(kgsl_driver.lockless_workqueue);

	return status;
}

void adreno_create_profile_buffer(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	unsigned int priv = 0;

	if (ADRENO_FEATURE(adreno_dev, ADRENO_APRIV))
		priv = KGSL_MEMDESC_PRIVILEGED;

	adreno_allocate_global(device, &adreno_dev->profile_buffer,
		PAGE_SIZE, 0, 0, priv, "alwayson");

	adreno_dev->profile_index = 0;

	if (!IS_ERR(adreno_dev->profile_buffer))
		set_bit(ADRENO_DEVICE_DRAWOBJ_PROFILE,
			&adreno_dev->priv);
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

	ret = gpudev->init(adreno_dev);
	if (ret)
		return ret;

	set_bit(ADRENO_DEVICE_INITIALIZED, &adreno_dev->priv);

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
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);

	FOR_EACH_RINGBUFFER(adreno_dev, rb, i) {
		if (rb->drawctxt_active)
			kgsl_context_put(&(rb->drawctxt_active->base));
		rb->drawctxt_active = NULL;

		kgsl_sharedmem_writel(device->scratch,
			SCRATCH_RB_OFFSET(rb->id, current_rb_ptname),
			0);
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

	if (!device->pdev_loaded)
		return -ENODEV;

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
	int ret = 0;

	if (!device->pwrctrl.bus_control)
		return;

	/* VBIF waiting for RAM */
	ret |= adreno_perfcounter_kernel_get(adreno_dev,
		KGSL_PERFCOUNTER_GROUP_VBIF_PWR, 0,
		&adreno_dev->starved_ram_lo, NULL);

	/* Target has GBIF */
	if (adreno_is_gen7(adreno_dev) ||
		(adreno_is_a6xx(adreno_dev) && !adreno_is_a630(adreno_dev))) {
		ret |= adreno_perfcounter_kernel_get(adreno_dev,
			KGSL_PERFCOUNTER_GROUP_VBIF_PWR, 1,
			&adreno_dev->starved_ram_lo_ch1, NULL);

		ret |= adreno_perfcounter_kernel_get(adreno_dev,
			KGSL_PERFCOUNTER_GROUP_VBIF,
			GBIF_AXI0_READ_DATA_TOTAL_BEATS,
			&adreno_dev->ram_cycles_lo, NULL);

		ret |= adreno_perfcounter_kernel_get(adreno_dev,
			KGSL_PERFCOUNTER_GROUP_VBIF,
			GBIF_AXI1_READ_DATA_TOTAL_BEATS,
			&adreno_dev->ram_cycles_lo_ch1_read, NULL);

		ret |= adreno_perfcounter_kernel_get(adreno_dev,
			KGSL_PERFCOUNTER_GROUP_VBIF,
			GBIF_AXI0_WRITE_DATA_TOTAL_BEATS,
			&adreno_dev->ram_cycles_lo_ch0_write, NULL);

		ret |= adreno_perfcounter_kernel_get(adreno_dev,
			KGSL_PERFCOUNTER_GROUP_VBIF,
			GBIF_AXI1_WRITE_DATA_TOTAL_BEATS,
			&adreno_dev->ram_cycles_lo_ch1_write, NULL);
	} else {
		/* VBIF DDR cycles */
		ret |= adreno_perfcounter_kernel_get(adreno_dev,
			KGSL_PERFCOUNTER_GROUP_VBIF,
			VBIF_AXI_TOTAL_BEATS,
			&adreno_dev->ram_cycles_lo, NULL);
	}

	if (ret)
		dev_err(KGSL_DEVICE(adreno_dev)->dev,
			"Unable to get perf counters for bus DCVS\n");
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

	/*
	 * TLB operations are skipped during slumber. Incase CX doesn't
	 * go down, it can result in incorrect translations due to stale
	 * TLB entries. Flush TLB before boot up to ensure fresh start.
	 */
	kgsl_mmu_flush_tlb(&device->mmu);

	/* Set any stale active contexts to NULL */
	adreno_set_active_ctxs_null(adreno_dev);

	/* Set the bit to indicate that we've just powered on */
	set_bit(ADRENO_DEVICE_PWRON, &adreno_dev->priv);

	/* Clear the busy_data stats - we're starting over from scratch */
	memset(&adreno_dev->busy_data, 0, sizeof(adreno_dev->busy_data));

	/* Soft reset the GPU if a regulator is stuck on*/
	if (regulator_left_on)
		_soft_reset(adreno_dev);

	/* Start the GPU */
	status = gpudev->start(adreno_dev);
	if (status)
		goto error_pwr_off;

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

	/*
	 * There is a possible deadlock scenario during kgsl firmware reading
	 * (request_firmware) and devfreq update calls. During first boot, kgsl
	 * device mutex is held and then request_firmware is called for reading
	 * firmware. request_firmware internally takes dev_pm_qos_mtx lock.
	 * Whereas in case of devfreq update calls triggered by thermal/bcl or
	 * devfreq sysfs, it first takes the same dev_pm_qos_mtx lock and then
	 * tries to take kgsl device mutex as part of get_dev_status/target
	 * calls. This results in deadlock when both thread are unable to acquire
	 * the mutex held by other thread. Enable devfreq updates now as we are
	 * done reading all firmware files.
	 */
	device->pwrscale.devfreq_enabled = true;

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

	if (!IS_ERR_OR_NULL(adreno_dev->gpumv_llc_slice))
		llcc_slice_deactivate(adreno_dev->gpumv_llc_slice);

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
		return gpudev->reset(adreno_dev);

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

		for (i = 0; ret && i < 4; i++) {
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
	else if (param->type == KGSL_PROP_IS_LPAC_ENABLED)
		val = adreno_dev->lpac_enabled ? 1 : 0;

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
	{ KGSL_PROP_IS_LPAC_ENABLED, adreno_prop_u32 },
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

		status = register_l3_voter(device);
		if (status)
			break;

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
	const struct adreno_gpudev *gpudev = ADRENO_GPU_DEVICE(adreno_dev);

	switch (type) {
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
		status = gpudev->setproperty(dev_priv, type, value, sizebytes);
		break;
	}

	return status;
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

bool adreno_isidle(struct adreno_device *adreno_dev)
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

static int adreno_drain_and_idle(struct kgsl_device *device)
{
	int ret;

	reinit_completion(&device->halt_gate);

	ret = kgsl_active_count_wait(device, 0, HZ);
	if (ret)
		return ret;

	return adreno_idle(device);
}

/* Caller must hold the device mutex. */
int adreno_suspend_context(struct kgsl_device *device)
{
	/* process any profiling results that are available */
	adreno_profile_process_results(ADRENO_DEVICE(device));

	/* Wait for the device to go idle */
	return adreno_idle(device);
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
	trace_kgsl_regwrite(offsetwords, value);

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

bool adreno_gx_is_on(struct adreno_device *adreno_dev)
{
	const struct adreno_gpudev *gpudev  = ADRENO_GPU_DEVICE(adreno_dev);

	return gpudev->gx_is_on(adreno_dev);
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

	if (!time)
		return;

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

	return gpudev->power_stats(adreno_dev, stats);
}

static int adreno_regulator_enable(struct kgsl_device *device)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	const struct adreno_gpudev *gpudev  = ADRENO_GPU_DEVICE(adreno_dev);

	if (gpudev->regulator_enable)
		return gpudev->regulator_enable(adreno_dev);

	return 0;
}

static bool adreno_is_hw_collapsible(struct kgsl_device *device)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	const struct adreno_gpudev *gpudev  = ADRENO_GPU_DEVICE(adreno_dev);

	if (!gpudev->is_hw_collapsible(adreno_dev))
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

	if (gpudev->regulator_disable)
		gpudev->regulator_disable(adreno_dev);
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
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	const struct adreno_gpudev *gpudev = ADRENO_GPU_DEVICE(adreno_dev);

	if (gpudev->clk_set_options)
		gpudev->clk_set_options(adreno_dev, name, clk, on);
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
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);

	if (WARN_ON(!adreno_dev->dispatch_ops || !adreno_dev->dispatch_ops->queue_cmds))
		return -ENODEV;

	return adreno_dev->dispatch_ops->queue_cmds(dev_priv, context, drawobj,
		count, timestamp);
}

static inline bool _verify_ib(struct kgsl_device_private *dev_priv,
		struct kgsl_context *context, struct kgsl_memobj_node *ib)
{
	struct kgsl_device *device = dev_priv->device;
	struct kgsl_process_private *private = dev_priv->process_priv;

	/* The maximum allowable size for an IB in the CP is 0xFFFFF dwords */
	if (ib->size == 0 || ((ib->size >> 2) > 0xFFFFF)) {
		pr_context(device, context, "ctxt %u invalid ib size %lld\n",
			context->id, ib->size);
		return false;
	}

	/* Make sure that the address is in range and dword aligned */
	if (!kgsl_mmu_gpuaddr_in_range(private->pagetable, ib->gpuaddr,
		ib->size) || !IS_ALIGNED(ib->gpuaddr, 4)) {
		pr_context(device, context, "ctxt %u invalid ib gpuaddr %llX\n",
			context->id, ib->gpuaddr);
		return false;
	}

	return true;
}

int adreno_verify_cmdobj(struct kgsl_device_private *dev_priv,
		struct kgsl_context *context, struct kgsl_drawobj *drawobj[],
		uint32_t count)
{
	struct kgsl_device *device = dev_priv->device;
	struct kgsl_memobj_node *ib;
	unsigned int i;

	for (i = 0; i < count; i++) {
		/* Verify the IBs before they get queued */
		if (drawobj[i]->type == CMDOBJ_TYPE) {
			struct kgsl_drawobj_cmd *cmdobj = CMDOBJ(drawobj[i]);

			list_for_each_entry(ib, &cmdobj->cmdlist, node)
				if (!_verify_ib(dev_priv,
					&ADRENO_CONTEXT(context)->base, ib))
					return -EINVAL;

			/*
			 * Clear the wake on touch bit to indicate an IB has
			 * been submitted since the last time we set it.
			 * But only clear it when we have rendering commands.
			 */
			ADRENO_DEVICE(device)->wake_on_touch = false;
		}

		/* A3XX does not have support for drawobj profiling */
		if (adreno_is_a3xx(ADRENO_DEVICE(device)) &&
			(drawobj[i]->flags & KGSL_DRAWOBJ_PROFILING))
			return -EOPNOTSUPP;
	}

	return 0;
}

static int adreno_queue_recurring_cmd(struct kgsl_device_private *dev_priv,
	struct kgsl_context *context, struct kgsl_drawobj *drawobj)
{
	struct kgsl_device *device = dev_priv->device;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct adreno_hwsched *hwsched = &adreno_dev->hwsched;
	const struct adreno_gpudev *gpudev  = ADRENO_GPU_DEVICE(adreno_dev);
	struct kgsl_drawobj_cmd *cmdobj = CMDOBJ(drawobj);
	int ret;

	if (!ADRENO_FEATURE(adreno_dev, ADRENO_LSR))
		return -EOPNOTSUPP;

	if (!gpudev->send_recurring_cmdobj)
		return -ENODEV;

	ret = adreno_verify_cmdobj(dev_priv, context, &drawobj, 1);
	if (ret)
		return ret;

	mutex_lock(&device->mutex);

	/* Only one recurring command allowed */
	if (hwsched->recurring_cmdobj) {
		mutex_unlock(&device->mutex);
		return -EINVAL;
	}

	ret = kgsl_check_context_state(context);
	if (ret) {
		mutex_unlock(&device->mutex);
		return ret;
	}

	set_bit(CMDOBJ_RECURRING_START, &cmdobj->priv);

	ret = gpudev->send_recurring_cmdobj(adreno_dev, cmdobj);
	mutex_unlock(&device->mutex);

	if (!ret)
		srcu_notifier_call_chain(&device->nh, GPU_GMU_READY, NULL);

	return ret;
}

static int adreno_dequeue_recurring_cmd(struct kgsl_device *device,
	struct kgsl_context *context)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct adreno_hwsched *hwsched = &adreno_dev->hwsched;
	const struct adreno_gpudev *gpudev  = ADRENO_GPU_DEVICE(adreno_dev);
	struct kgsl_drawobj *recurring_drawobj;
	int ret;

	if (!ADRENO_FEATURE(adreno_dev, ADRENO_LSR))
		return -EOPNOTSUPP;

	if (!gpudev->send_recurring_cmdobj)
		return -ENODEV;

	mutex_lock(&device->mutex);

	/* We can safely return here as recurring wokload is already untracked */
	if (hwsched->recurring_cmdobj == NULL) {
		mutex_unlock(&device->mutex);
		return -EINVAL;
	}

	recurring_drawobj = DRAWOBJ(hwsched->recurring_cmdobj);

	/* Check if the recurring command is for same context or not*/
	if (recurring_drawobj->context != context) {
		mutex_unlock(&device->mutex);
		return -EINVAL;
	}

	ret = kgsl_check_context_state(context);
	if (ret) {
		mutex_unlock(&device->mutex);
		return ret;
	}

	clear_bit(CMDOBJ_RECURRING_START, &hwsched->recurring_cmdobj->priv);
	set_bit(CMDOBJ_RECURRING_STOP, &hwsched->recurring_cmdobj->priv);

	ret = gpudev->send_recurring_cmdobj(adreno_dev, hwsched->recurring_cmdobj);

	mutex_unlock(&device->mutex);

	if (!ret)
		srcu_notifier_call_chain(&device->nh, GPU_GMU_STOP, NULL);

	return ret;
}

static void adreno_drawctxt_sched(struct kgsl_device *device,
		struct kgsl_context *context)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);

	if (WARN_ON(!adreno_dev->dispatch_ops || !adreno_dev->dispatch_ops->queue_context))
		return;

	adreno_dev->dispatch_ops->queue_context(adreno_dev,
		ADRENO_CONTEXT(context));
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

struct cycle_data {
	void *ptr;
	void *val;
};

static void cycle_set_bool(struct adreno_device *adreno_dev, void *priv)
{
	struct cycle_data *data = priv;

	*((bool *) data->ptr) = *((bool *) data->val);
}

int adreno_power_cycle_bool(struct adreno_device *adreno_dev,
	bool *flag, bool val)
{
	struct cycle_data data = { .ptr = flag, .val = &val };

	return adreno_power_cycle(adreno_dev, cycle_set_bool, &data);
}

static void cycle_set_u32(struct adreno_device *adreno_dev, void *priv)
{
	struct cycle_data *data = priv;

	*((u32 *) data->ptr) = *((u32 *) data->val);
}

int adreno_power_cycle_u32(struct adreno_device *adreno_dev,
	u32 *flag, u32 val)
{
	struct cycle_data data = { .ptr = flag, .val = &val };

	return adreno_power_cycle(adreno_dev, cycle_set_u32, &data);
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

static void adreno_create_hw_fence(struct kgsl_device *device, struct kgsl_sync_fence *kfence)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);

	if (WARN_ON(!adreno_dev->dispatch_ops))
		return;

	if (adreno_dev->dispatch_ops->create_hw_fence)
		adreno_dev->dispatch_ops->create_hw_fence(adreno_dev, kfence);
}

static int adreno_cx_gdsc_event(struct notifier_block *nb,
	unsigned long event, void *data)
{
	struct kgsl_pwrctrl *pwr = container_of(nb, struct kgsl_pwrctrl, cx_gdsc_nb);

	if (!(event & REGULATOR_EVENT_DISABLE) || !pwr->cx_gdsc_wait)
		return 0;

	pwr->cx_gdsc_wait = false;
	complete_all(&pwr->cx_gdsc_gate);

	return 0;
}

static int adreno_register_gdsc_notifier(struct kgsl_device *device)
{
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	const struct adreno_power_ops *ops = ADRENO_POWER_OPS(adreno_dev);

	if (ops->register_gdsc_notifier)
		return ops->register_gdsc_notifier(adreno_dev);

	if (IS_ERR_OR_NULL(pwr->cx_gdsc))
		return 0;

	pwr->cx_gdsc_nb.notifier_call = adreno_cx_gdsc_event;
	return devm_regulator_register_notifier(pwr->cx_gdsc, &pwr->cx_gdsc_nb);
}

static const struct kgsl_functable adreno_functable = {
	/* Mandatory functions */
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
	.snapshot = adreno_snapshot,
	.drain_and_idle = adreno_drain_and_idle,
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
	.clk_set_options = adreno_clk_set_options,
	.query_property_list = adreno_query_property_list,
	.is_hwcg_on = adreno_is_hwcg_on,
	.gpu_clock_set = adreno_gpu_clock_set,
	.gpu_bus_set = adreno_gpu_bus_set,
	.deassert_gbif_halt = adreno_deassert_gbif_halt,
	.queue_recurring_cmd = adreno_queue_recurring_cmd,
	.dequeue_recurring_cmd = adreno_dequeue_recurring_cmd,
	.create_hw_fence = adreno_create_hw_fence,
	.register_gdsc_notifier = adreno_register_gdsc_notifier,
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

static void adreno_add_components(struct device *dev,
		struct component_match **match)
{
	struct device_node *node;

	/*
	 * Add kgsl-smmu, context banks and gmu as components, if supported.
	 * Master bind (adreno_bind) will be called only once all added
	 * components are available.
	 */
	for_each_matching_node(node, adreno_component_match) {
		if (!of_device_is_available(node))
			continue;

		component_match_add_release(dev, match, _release_of, _compare_of, node);
	}
}

static int adreno_probe(struct platform_device *pdev)
{
	struct component_match *match = NULL;

	adreno_add_components(&pdev->dev, &match);

	if (!match)
		return -ENODEV;

	return component_master_add_with_match(&pdev->dev,
			&adreno_ops, match);
}

static int adreno_remove(struct platform_device *pdev)
{
	component_master_del(&pdev->dev, &adreno_ops);

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

	ret = kgsl_mmu_init();
	if (ret) {
		kgsl_core_exit();
		return ret;
	}

	gmu_core_register();
	ret = platform_driver_register(&adreno_platform_driver);
	if (ret) {
		gmu_core_unregister();
		kgsl_mmu_exit();
		kgsl_core_exit();
	}

	return ret;
}

static void __exit kgsl_3d_exit(void)
{
	platform_driver_unregister(&adreno_platform_driver);
	gmu_core_unregister();
	kgsl_mmu_exit();
	kgsl_core_exit();
}

module_init(kgsl_3d_init);
module_exit(kgsl_3d_exit);

MODULE_DESCRIPTION("3D Graphics driver");
MODULE_LICENSE("GPL v2");
MODULE_SOFTDEP("pre: arm_smmu nvmem_qfprom");
