/* Copyright (c) 2002,2007-2013, The Linux Foundation. All rights reserved.
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

#include "kgsl.h"
#include "kgsl_cffdump.h"
#include "kgsl_sharedmem.h"

#include "z180.h"
#include "z180_reg.h"
#include "z180_trace.h"

#define DRIVER_VERSION_MAJOR   3
#define DRIVER_VERSION_MINOR   1

#define GSL_VGC_INT_MASK \
	 (REG_VGC_IRQSTATUS__MH_MASK | \
	  REG_VGC_IRQSTATUS__G2D_MASK | \
	  REG_VGC_IRQSTATUS__FIFO_MASK)

#define VGV3_NEXTCMD_JUMP        0x01

#define VGV3_NEXTCMD_NEXTCMD_FSHIFT 12
#define VGV3_NEXTCMD_NEXTCMD_FMASK 0x7

#define VGV3_CONTROL_MARKADD_FSHIFT 0
#define VGV3_CONTROL_MARKADD_FMASK 0xfff

#define Z180_MARKER_SIZE 10
#define Z180_CALL_CMD     0x1000
#define Z180_MARKER_CMD   0x8000
#define Z180_STREAM_END_CMD 0x9000
#define Z180_STREAM_PACKET 0x7C000176
#define Z180_STREAM_PACKET_CALL 0x7C000275

#define NUMTEXUNITS             4
#define TEXUNITREGCOUNT         25
#define VG_REGCOUNT             0x39

#define PACKETSIZE_BEGIN        3
#define PACKETSIZE_G2DCOLOR     2
#define PACKETSIZE_TEXUNIT      (TEXUNITREGCOUNT * 2)
#define PACKETSIZE_REG          (VG_REGCOUNT * 2)
#define PACKETSIZE_STATE        (PACKETSIZE_TEXUNIT * NUMTEXUNITS + \
				 PACKETSIZE_REG + PACKETSIZE_BEGIN + \
				 PACKETSIZE_G2DCOLOR)
#define PACKETSIZE_STATESTREAM  (ALIGN((PACKETSIZE_STATE * \
				 sizeof(unsigned int)), 32) / \
				 sizeof(unsigned int))

#define Z180_INVALID_CONTEXT UINT_MAX

/* z180 MH arbiter config*/
#define Z180_CFG_MHARB \
	(0x10 \
		| (0 << MH_ARBITER_CONFIG__SAME_PAGE_GRANULARITY__SHIFT) \
		| (1 << MH_ARBITER_CONFIG__L1_ARB_ENABLE__SHIFT) \
		| (1 << MH_ARBITER_CONFIG__L1_ARB_HOLD_ENABLE__SHIFT) \
		| (0 << MH_ARBITER_CONFIG__L2_ARB_CONTROL__SHIFT) \
		| (1 << MH_ARBITER_CONFIG__PAGE_SIZE__SHIFT) \
		| (1 << MH_ARBITER_CONFIG__TC_REORDER_ENABLE__SHIFT) \
		| (1 << MH_ARBITER_CONFIG__TC_ARB_HOLD_ENABLE__SHIFT) \
		| (0 << MH_ARBITER_CONFIG__IN_FLIGHT_LIMIT_ENABLE__SHIFT) \
		| (0x8 << MH_ARBITER_CONFIG__IN_FLIGHT_LIMIT__SHIFT) \
		| (1 << MH_ARBITER_CONFIG__CP_CLNT_ENABLE__SHIFT) \
		| (1 << MH_ARBITER_CONFIG__VGT_CLNT_ENABLE__SHIFT) \
		| (1 << MH_ARBITER_CONFIG__TC_CLNT_ENABLE__SHIFT) \
		| (1 << MH_ARBITER_CONFIG__RB_CLNT_ENABLE__SHIFT) \
		| (1 << MH_ARBITER_CONFIG__PA_CLNT_ENABLE__SHIFT))

#define Z180_TIMESTAMP_EPSILON 20000
#define Z180_IDLE_COUNT_MAX 1000000

enum z180_cmdwindow_type {
	Z180_CMDWINDOW_2D = 0x00000000,
	Z180_CMDWINDOW_MMU = 0x00000002,
};

#define Z180_CMDWINDOW_TARGET_MASK		0x000000FF
#define Z180_CMDWINDOW_ADDR_MASK		0x00FFFF00
#define Z180_CMDWINDOW_TARGET_SHIFT		0
#define Z180_CMDWINDOW_ADDR_SHIFT		8

static int z180_init(struct kgsl_device *device);
static int z180_start(struct kgsl_device *device);
static int z180_stop(struct kgsl_device *device);
static int z180_wait(struct kgsl_device *device,
				struct kgsl_context *context,
				unsigned int timestamp,
				unsigned int msecs);
static void z180_regread(struct kgsl_device *device,
				unsigned int offsetwords,
				unsigned int *value);
static void z180_regwrite(struct kgsl_device *device,
				unsigned int offsetwords,
				unsigned int value);
static void z180_cmdwindow_write(struct kgsl_device *device,
				unsigned int addr,
				unsigned int data);

#define Z180_MMU_CONFIG					     \
	(0x01							     \
	| (MMU_CONFIG << MH_MMU_CONFIG__RB_W_CLNT_BEHAVIOR__SHIFT)   \
	| (MMU_CONFIG << MH_MMU_CONFIG__CP_W_CLNT_BEHAVIOR__SHIFT)   \
	| (MMU_CONFIG << MH_MMU_CONFIG__CP_R0_CLNT_BEHAVIOR__SHIFT)  \
	| (MMU_CONFIG << MH_MMU_CONFIG__CP_R1_CLNT_BEHAVIOR__SHIFT)  \
	| (MMU_CONFIG << MH_MMU_CONFIG__CP_R2_CLNT_BEHAVIOR__SHIFT)  \
	| (MMU_CONFIG << MH_MMU_CONFIG__CP_R3_CLNT_BEHAVIOR__SHIFT)  \
	| (MMU_CONFIG << MH_MMU_CONFIG__CP_R4_CLNT_BEHAVIOR__SHIFT)  \
	| (MMU_CONFIG << MH_MMU_CONFIG__VGT_R0_CLNT_BEHAVIOR__SHIFT) \
	| (MMU_CONFIG << MH_MMU_CONFIG__VGT_R1_CLNT_BEHAVIOR__SHIFT) \
	| (MMU_CONFIG << MH_MMU_CONFIG__TC_R_CLNT_BEHAVIOR__SHIFT)   \
	| (MMU_CONFIG << MH_MMU_CONFIG__PA_W_CLNT_BEHAVIOR__SHIFT))

#define KGSL_LOG_LEVEL_DEFAULT 3

static const struct kgsl_functable z180_functable;

static struct z180_device device_2d0 = {
	.dev = {
		KGSL_DEVICE_COMMON_INIT(device_2d0.dev),
		.name = DEVICE_2D0_NAME,
		.id = KGSL_DEVICE_2D0,
		.mh = {
			.mharb = Z180_CFG_MHARB,
			.mh_intf_cfg1 = 0x00032f07,
			.mh_intf_cfg2 = 0x004b274f,
			/* turn off memory protection unit by setting
			   acceptable physical address range to include
			   all pages. */
			.mpu_base = 0x00000000,
			.mpu_range =  0xFFFFF000,
		},
		.mmu = {
			.config = Z180_MMU_CONFIG,
		},
		.pwrctrl = {
			.irq_name = KGSL_2D0_IRQ,
		},
		.iomemname = KGSL_2D0_REG_MEMORY,
		.ftbl = &z180_functable,
		.cmd_log = KGSL_LOG_LEVEL_DEFAULT,
		.ctxt_log = KGSL_LOG_LEVEL_DEFAULT,
		.drv_log = KGSL_LOG_LEVEL_DEFAULT,
		.mem_log = KGSL_LOG_LEVEL_DEFAULT,
		.pwr_log = KGSL_LOG_LEVEL_DEFAULT,
		.ft_log = KGSL_LOG_LEVEL_DEFAULT,
		.pm_dump_enable = 0,
	},
	.cmdwin_lock = __SPIN_LOCK_INITIALIZER(device_2d1.cmdwin_lock),
};

static struct z180_device device_2d1 = {
	.dev = {
		KGSL_DEVICE_COMMON_INIT(device_2d1.dev),
		.name = DEVICE_2D1_NAME,
		.id = KGSL_DEVICE_2D1,
		.mh = {
			.mharb = Z180_CFG_MHARB,
			.mh_intf_cfg1 = 0x00032f07,
			.mh_intf_cfg2 = 0x004b274f,
			/* turn off memory protection unit by setting
			   acceptable physical address range to include
			   all pages. */
			.mpu_base = 0x00000000,
			.mpu_range =  0xFFFFF000,
		},
		.mmu = {
			.config = Z180_MMU_CONFIG,
		},
		.pwrctrl = {
			.irq_name = KGSL_2D1_IRQ,
		},
		.iomemname = KGSL_2D1_REG_MEMORY,
		.ftbl = &z180_functable,
	},
	.cmdwin_lock = __SPIN_LOCK_INITIALIZER(device_2d1.cmdwin_lock),
};

static irqreturn_t z180_irq_handler(struct kgsl_device *device)
{
	irqreturn_t result = IRQ_NONE;
	unsigned int status;
	struct z180_device *z180_dev = Z180_DEVICE(device);

	z180_regread(device, ADDR_VGC_IRQSTATUS >> 2, &status);

	trace_kgsl_z180_irq_status(device, status);

	if (status & GSL_VGC_INT_MASK) {
		z180_regwrite(device,
			ADDR_VGC_IRQSTATUS >> 2, status & GSL_VGC_INT_MASK);

		result = IRQ_HANDLED;

		if (status & REG_VGC_IRQSTATUS__FIFO_MASK)
			KGSL_DRV_ERR(device, "z180 fifo interrupt\n");
		if (status & REG_VGC_IRQSTATUS__MH_MASK)
			kgsl_mh_intrcallback(device);
		if (status & REG_VGC_IRQSTATUS__G2D_MASK) {
			int count;

			z180_regread(device,
					 ADDR_VGC_IRQ_ACTIVE_CNT >> 2,
					 &count);

			count >>= 8;
			count &= 255;
			z180_dev->timestamp += count;

			queue_work(device->work_queue, &device->ts_expired_ws);
			wake_up_interruptible(&device->wait_queue);
		}
	}

	if (device->requested_state == KGSL_STATE_NONE) {
		kgsl_pwrctrl_request_state(device, KGSL_STATE_NAP);
		queue_work(device->work_queue, &device->idle_check_ws);
	}
	mod_timer_pending(&device->idle_timer,
			jiffies + device->pwrctrl.interval_timeout);

	return result;
}

static void z180_cleanup_pt(struct kgsl_device *device,
			       struct kgsl_pagetable *pagetable)
{
	struct z180_device *z180_dev = Z180_DEVICE(device);

	kgsl_mmu_unmap(pagetable, &device->mmu.setstate_memory);

	kgsl_mmu_unmap(pagetable, &device->memstore);

	kgsl_mmu_unmap(pagetable, &z180_dev->ringbuffer.cmdbufdesc);
}

static int z180_setup_pt(struct kgsl_device *device,
			     struct kgsl_pagetable *pagetable)
{
	int result = 0;
	struct z180_device *z180_dev = Z180_DEVICE(device);

	result = kgsl_mmu_map_global(pagetable, &device->mmu.setstate_memory);

	if (result)
		goto error;

	result = kgsl_mmu_map_global(pagetable, &device->memstore);
	if (result)
		goto error_unmap_dummy;

	result = kgsl_mmu_map_global(pagetable,
				     &z180_dev->ringbuffer.cmdbufdesc);
	if (result)
		goto error_unmap_memstore;
	/*
	 * Set the mpu end to the last "normal" global memory we use.
	 * For the IOMMU, this will be used to restrict access to the
	 * mapped registers.
	 */
	device->mh.mpu_range = z180_dev->ringbuffer.cmdbufdesc.gpuaddr +
				z180_dev->ringbuffer.cmdbufdesc.size;
	return result;

error_unmap_dummy:
	kgsl_mmu_unmap(pagetable, &device->mmu.setstate_memory);

error_unmap_memstore:
	kgsl_mmu_unmap(pagetable, &device->memstore);

error:
	return result;
}

static inline unsigned int rb_offset(unsigned int timestamp)
{
	return (timestamp % Z180_PACKET_COUNT)
		*sizeof(unsigned int)*(Z180_PACKET_SIZE);
}

static inline unsigned int rb_gpuaddr(struct z180_device *z180_dev,
					unsigned int timestamp)
{
	return z180_dev->ringbuffer.cmdbufdesc.gpuaddr + rb_offset(timestamp);
}

static void addmarker(struct z180_ringbuffer *rb, unsigned int timestamp)
{
	char *ptr = (char *)(rb->cmdbufdesc.hostptr);
	unsigned int *p = (unsigned int *)(ptr + rb_offset(timestamp));

	*p++ = Z180_STREAM_PACKET;
	*p++ = (Z180_MARKER_CMD | 5);
	*p++ = ADDR_VGV3_LAST << 24;
	*p++ = ADDR_VGV3_LAST << 24;
	*p++ = ADDR_VGV3_LAST << 24;
	*p++ = Z180_STREAM_PACKET;
	*p++ = 5;
	*p++ = ADDR_VGV3_LAST << 24;
	*p++ = ADDR_VGV3_LAST << 24;
	*p++ = ADDR_VGV3_LAST << 24;
}

static void addcmd(struct z180_ringbuffer *rb, unsigned int timestamp,
			unsigned int cmd, unsigned int nextcnt)
{
	char * ptr = (char *)(rb->cmdbufdesc.hostptr);
	unsigned int *p = (unsigned int *)(ptr + (rb_offset(timestamp)
			   + (Z180_MARKER_SIZE * sizeof(unsigned int))));

	*p++ = Z180_STREAM_PACKET_CALL;
	*p++ = cmd;
	*p++ = Z180_CALL_CMD | nextcnt;
	*p++ = ADDR_VGV3_LAST << 24;
	*p++ = ADDR_VGV3_LAST << 24;
}

static void z180_cmdstream_start(struct kgsl_device *device)
{
	struct z180_device *z180_dev = Z180_DEVICE(device);
	unsigned int cmd = VGV3_NEXTCMD_JUMP << VGV3_NEXTCMD_NEXTCMD_FSHIFT;

	addmarker(&z180_dev->ringbuffer, 0);

	z180_cmdwindow_write(device, ADDR_VGV3_MODE, 4);

	z180_cmdwindow_write(device, ADDR_VGV3_NEXTADDR,
			     rb_gpuaddr(z180_dev, z180_dev->current_timestamp));

	z180_cmdwindow_write(device, ADDR_VGV3_NEXTCMD, cmd | 5);

	z180_cmdwindow_write(device, ADDR_VGV3_WRITEADDR,
			device->memstore.gpuaddr);

	cmd = (int)(((1) & VGV3_CONTROL_MARKADD_FMASK)
			<< VGV3_CONTROL_MARKADD_FSHIFT);

	z180_cmdwindow_write(device, ADDR_VGV3_CONTROL, cmd);

	z180_cmdwindow_write(device, ADDR_VGV3_CONTROL, 0);
}

static int room_in_rb(struct z180_device *device)
{
	int ts_diff;

	ts_diff = device->current_timestamp - device->timestamp;

	return ts_diff < Z180_PACKET_COUNT;
}

static int z180_idle(struct kgsl_device *device)
{
	int status = 0;
	struct z180_device *z180_dev = Z180_DEVICE(device);

	if (timestamp_cmp(z180_dev->current_timestamp,
		z180_dev->timestamp) > 0)
		status = z180_wait(device, NULL,
				z180_dev->current_timestamp,
				Z180_IDLE_TIMEOUT);

	if (status)
		KGSL_DRV_ERR(device, "z180_waittimestamp() timed out\n");

	return status;
}

int
z180_cmdstream_issueibcmds(struct kgsl_device_private *dev_priv,
			struct kgsl_context *context,
			struct kgsl_ibdesc *ibdesc,
			unsigned int numibs,
			uint32_t *timestamp,
			unsigned int ctrl)
{
	long result = 0;
	unsigned int ofs        = PACKETSIZE_STATESTREAM * sizeof(unsigned int);
	unsigned int cnt        = 5;
	unsigned int old_timestamp = 0;
	unsigned int nextcnt    = Z180_STREAM_END_CMD | 5;
	struct kgsl_mem_entry *entry = NULL;
	unsigned int cmd;
	struct kgsl_device *device = dev_priv->device;
	struct kgsl_pagetable *pagetable = dev_priv->process_priv->pagetable;
	struct z180_device *z180_dev = Z180_DEVICE(device);
	unsigned int sizedwords;

	if (device->state & KGSL_STATE_HUNG) {
		result = -EINVAL;
		goto error;
	}
	if (numibs != 1) {
		KGSL_DRV_ERR(device, "Invalid number of ibs: %d\n", numibs);
		result = -EINVAL;
		goto error;
	}
	cmd = ibdesc[0].gpuaddr;
	sizedwords = ibdesc[0].sizedwords;
	/*
	 * Get a kernel mapping to the IB for monkey patching.
	 * See the end of this function.
	 */
	entry = kgsl_sharedmem_find_region(dev_priv->process_priv, cmd,
		sizedwords);
	if (entry == NULL) {
		KGSL_DRV_ERR(device, "Bad ibdesc: gpuaddr 0x%x size %d\n",
			     cmd, sizedwords);
		result = -EINVAL;
		goto error;
	}
	/*
	 * This will only map memory if it exists, otherwise it will reuse the
	 * mapping. And the 2d userspace reuses IBs so we likely won't create
	 * too many mappings.
	 */
	if (kgsl_gpuaddr_to_vaddr(&entry->memdesc, cmd) == NULL) {
		KGSL_DRV_ERR(device,
			     "Cannot make kernel mapping for gpuaddr 0x%x\n",
			     cmd);
		result = -EINVAL;
		goto error;
	}

	KGSL_CMD_INFO(device, "ctxt %d ibaddr 0x%08x sizedwords %d\n",
		context->id, cmd, sizedwords);
	/* context switch */
	if ((context->id != (int)z180_dev->ringbuffer.prevctx) ||
	    (ctrl & KGSL_CONTEXT_CTX_SWITCH)) {
		KGSL_CMD_INFO(device, "context switch %d -> %d\n",
			context->id, z180_dev->ringbuffer.prevctx);
		kgsl_mmu_setstate(&device->mmu, pagetable,
				KGSL_MEMSTORE_GLOBAL);
		cnt = PACKETSIZE_STATESTREAM;
		ofs = 0;
	}
	kgsl_setstate(&device->mmu,
			KGSL_MEMSTORE_GLOBAL,
			kgsl_mmu_pt_get_flags(device->mmu.hwpagetable,
			device->id));

	result = wait_event_interruptible_timeout(device->wait_queue,
				  room_in_rb(z180_dev),
				  msecs_to_jiffies(KGSL_TIMEOUT_DEFAULT));
	if (result < 0) {
		KGSL_CMD_ERR(device, "wait_event_interruptible_timeout "
			"failed: %ld\n", result);
		goto error;
	}
	result = 0;

	old_timestamp = z180_dev->current_timestamp;
	z180_dev->current_timestamp++;
	*timestamp = z180_dev->current_timestamp;

	z180_dev->ringbuffer.prevctx = context->id;

	addcmd(&z180_dev->ringbuffer, old_timestamp, cmd + ofs, cnt);
	kgsl_pwrscale_busy(device);

	/* Make sure the next ringbuffer entry has a marker */
	addmarker(&z180_dev->ringbuffer, z180_dev->current_timestamp);

	/* monkey patch the IB so that it jumps back to the ringbuffer */
	kgsl_sharedmem_writel(device, &entry->memdesc,
		      ((sizedwords + 1) * sizeof(unsigned int)),
		      rb_gpuaddr(z180_dev, z180_dev->current_timestamp));
	kgsl_sharedmem_writel(device, &entry->memdesc,
			      ((sizedwords + 2) * sizeof(unsigned int)),
			      nextcnt);

	/* sync memory before activating the hardware for the new command*/
	mb();

	cmd = (int)(((2) & VGV3_CONTROL_MARKADD_FMASK)
		<< VGV3_CONTROL_MARKADD_FSHIFT);

	z180_cmdwindow_write(device, ADDR_VGV3_CONTROL, cmd);
	z180_cmdwindow_write(device, ADDR_VGV3_CONTROL, 0);
error:

	kgsl_trace_issueibcmds(device, context->id, ibdesc, numibs,
		*timestamp, ctrl, result, 0);

	return (int)result;
}

static int z180_ringbuffer_init(struct kgsl_device *device)
{
	struct z180_device *z180_dev = Z180_DEVICE(device);
	memset(&z180_dev->ringbuffer, 0, sizeof(struct z180_ringbuffer));
	z180_dev->ringbuffer.prevctx = Z180_INVALID_CONTEXT;
	z180_dev->ringbuffer.cmdbufdesc.flags = KGSL_MEMFLAGS_GPUREADONLY;
	return kgsl_allocate_contiguous(&z180_dev->ringbuffer.cmdbufdesc,
		Z180_RB_SIZE);
}

static void z180_ringbuffer_close(struct kgsl_device *device)
{
	struct z180_device *z180_dev = Z180_DEVICE(device);
	kgsl_sharedmem_free(&z180_dev->ringbuffer.cmdbufdesc);
	memset(&z180_dev->ringbuffer, 0, sizeof(struct z180_ringbuffer));
}

static int __devinit z180_probe(struct platform_device *pdev)
{
	int status = -EINVAL;
	struct kgsl_device *device = NULL;
	struct z180_device *z180_dev;

	device = (struct kgsl_device *)pdev->id_entry->driver_data;
	device->parentdev = &pdev->dev;

	z180_dev = Z180_DEVICE(device);

	status = z180_ringbuffer_init(device);
	if (status != 0)
		goto error;

	status = kgsl_device_platform_probe(device);
	if (status)
		goto error_close_ringbuffer;

	kgsl_pwrscale_init(device);
	kgsl_pwrscale_attach_policy(device, Z180_DEFAULT_PWRSCALE_POLICY);

	return status;

error_close_ringbuffer:
	z180_ringbuffer_close(device);
error:
	device->parentdev = NULL;
	return status;
}

static int __devexit z180_remove(struct platform_device *pdev)
{
	struct kgsl_device *device = NULL;

	device = (struct kgsl_device *)pdev->id_entry->driver_data;

	kgsl_pwrscale_close(device);
	kgsl_device_platform_remove(device);

	z180_ringbuffer_close(device);

	return 0;
}

static int z180_init(struct kgsl_device *device)
{
	struct z180_device *z180_dev = Z180_DEVICE(device);

	z180_dev->timestamp = 0;
	z180_dev->current_timestamp = 0;

	return 0;
}

static int z180_start(struct kgsl_device *device)
{
	int status = 0;

	kgsl_pwrctrl_set_state(device, KGSL_STATE_INIT);

	kgsl_pwrctrl_enable(device);

	/* Set interrupts to 0 to ensure a good state */
	z180_regwrite(device, (ADDR_VGC_IRQENABLE >> 2), 0x0);

	kgsl_mh_start(device);

	status = kgsl_mmu_start(device);
	if (status)
		goto error_clk_off;

	z180_cmdstream_start(device);

	kgsl_pwrctrl_irq(device, KGSL_PWRFLAGS_ON);
	device->ftbl->irqctrl(device, 1);

	device->reset_counter++;

	return 0;

error_clk_off:
	z180_regwrite(device, (ADDR_VGC_IRQENABLE >> 2), 0);
	kgsl_pwrctrl_disable(device);
	return status;
}

static int z180_stop(struct kgsl_device *device)
{
	device->ftbl->irqctrl(device, 0);
	z180_idle(device);

	del_timer_sync(&device->idle_timer);

	kgsl_mmu_stop(&device->mmu);

	/* Disable the clocks before the power rail. */
	kgsl_pwrctrl_irq(device, KGSL_PWRFLAGS_OFF);

	kgsl_pwrctrl_disable(device);

	return 0;
}

static int z180_getproperty(struct kgsl_device *device,
				enum kgsl_property_type type,
				void *value,
				unsigned int sizebytes)
{
	int status = -EINVAL;

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
		devinfo.chip_id = 0;
		devinfo.mmu_enabled = kgsl_mmu_enabled();

		if (copy_to_user(value, &devinfo, sizeof(devinfo)) !=
				0) {
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

	default:
		KGSL_DRV_ERR(device, "invalid property: %d\n", type);
		status = -EINVAL;
	}
	return status;
}

static unsigned int z180_isidle(struct kgsl_device *device)
{
	struct z180_device *z180_dev = Z180_DEVICE(device);

	return (timestamp_cmp(z180_dev->timestamp,
		z180_dev->current_timestamp) == 0) ? true : false;
}

static int z180_suspend_context(struct kgsl_device *device)
{
	struct z180_device *z180_dev = Z180_DEVICE(device);

	z180_dev->ringbuffer.prevctx = Z180_INVALID_CONTEXT;

	return 0;
}

/* Not all Z180 registers are directly accessible.
 * The _z180_(read|write)_simple functions below handle the ones that are.
 */
static void _z180_regread_simple(struct kgsl_device *device,
				unsigned int offsetwords,
				unsigned int *value)
{
	unsigned int *reg;

	BUG_ON(offsetwords * sizeof(uint32_t) >= device->reg_len);

	reg = (unsigned int *)(device->reg_virt + (offsetwords << 2));

	/*ensure this read finishes before the next one.
	 * i.e. act like normal readl() */
	*value = __raw_readl(reg);
	rmb();

}

static void _z180_regwrite_simple(struct kgsl_device *device,
				 unsigned int offsetwords,
				 unsigned int value)
{
	unsigned int *reg;

	BUG_ON(offsetwords*sizeof(uint32_t) >= device->reg_len);

	reg = (unsigned int *)(device->reg_virt + (offsetwords << 2));
	kgsl_cffdump_regwrite(device, offsetwords << 2, value);
	/*ensure previous writes post before this one,
	 * i.e. act like normal writel() */
	wmb();
	__raw_writel(value, reg);
}


/* The MH registers must be accessed through via a 2 step write, (read|write)
 * process. These registers may be accessed from interrupt context during
 * the handling of MH or MMU error interrupts. Therefore a spin lock is used
 * to ensure that the 2 step sequence is not interrupted.
 */
static void _z180_regread_mmu(struct kgsl_device *device,
			     unsigned int offsetwords,
			     unsigned int *value)
{
	struct z180_device *z180_dev = Z180_DEVICE(device);
	unsigned long flags;

	spin_lock_irqsave(&z180_dev->cmdwin_lock, flags);
	_z180_regwrite_simple(device, (ADDR_VGC_MH_READ_ADDR >> 2),
				offsetwords);
	_z180_regread_simple(device, (ADDR_VGC_MH_DATA_ADDR >> 2), value);
	spin_unlock_irqrestore(&z180_dev->cmdwin_lock, flags);
}


static void _z180_regwrite_mmu(struct kgsl_device *device,
			      unsigned int offsetwords,
			      unsigned int value)
{
	struct z180_device *z180_dev = Z180_DEVICE(device);
	unsigned int cmdwinaddr;
	unsigned long flags;

	cmdwinaddr = ((Z180_CMDWINDOW_MMU << Z180_CMDWINDOW_TARGET_SHIFT) &
			Z180_CMDWINDOW_TARGET_MASK);
	cmdwinaddr |= ((offsetwords << Z180_CMDWINDOW_ADDR_SHIFT) &
			Z180_CMDWINDOW_ADDR_MASK);

	spin_lock_irqsave(&z180_dev->cmdwin_lock, flags);
	_z180_regwrite_simple(device, ADDR_VGC_MMUCOMMANDSTREAM >> 2,
			     cmdwinaddr);
	_z180_regwrite_simple(device, ADDR_VGC_MMUCOMMANDSTREAM >> 2, value);
	spin_unlock_irqrestore(&z180_dev->cmdwin_lock, flags);
}

/* the rest of the code doesn't want to think about if it is writing mmu
 * registers or normal registers so handle it here
 */
static void z180_regread(struct kgsl_device *device,
			unsigned int offsetwords,
			unsigned int *value)
{
	if (!in_interrupt())
		kgsl_pre_hwaccess(device);

	if ((offsetwords >= MH_ARBITER_CONFIG &&
	     offsetwords <= MH_AXI_HALT_CONTROL) ||
	    (offsetwords >= MH_MMU_CONFIG &&
	     offsetwords <= MH_MMU_MPU_END)) {
		_z180_regread_mmu(device, offsetwords, value);
	} else {
		_z180_regread_simple(device, offsetwords, value);
	}
}

static void z180_regwrite(struct kgsl_device *device,
				unsigned int offsetwords,
				unsigned int value)
{
	if (!in_interrupt())
		kgsl_pre_hwaccess(device);

	if ((offsetwords >= MH_ARBITER_CONFIG &&
	     offsetwords <= MH_CLNT_INTF_CTRL_CONFIG2) ||
	    (offsetwords >= MH_MMU_CONFIG &&
	     offsetwords <= MH_MMU_MPU_END)) {
		_z180_regwrite_mmu(device, offsetwords, value);
	} else {
		_z180_regwrite_simple(device, offsetwords, value);
	}
}

static void z180_cmdwindow_write(struct kgsl_device *device,
		unsigned int addr, unsigned int data)
{
	unsigned int cmdwinaddr;

	cmdwinaddr = ((Z180_CMDWINDOW_2D << Z180_CMDWINDOW_TARGET_SHIFT) &
			Z180_CMDWINDOW_TARGET_MASK);
	cmdwinaddr |= ((addr << Z180_CMDWINDOW_ADDR_SHIFT) &
			Z180_CMDWINDOW_ADDR_MASK);

	z180_regwrite(device, ADDR_VGC_COMMANDSTREAM >> 2, cmdwinaddr);
	z180_regwrite(device, ADDR_VGC_COMMANDSTREAM >> 2, data);
}

static unsigned int z180_readtimestamp(struct kgsl_device *device,
		struct kgsl_context *context, enum kgsl_timestamp_type type)
{
	struct z180_device *z180_dev = Z180_DEVICE(device);
	(void)context;
	/* get current EOP timestamp */
	return z180_dev->timestamp;
}

static int z180_waittimestamp(struct kgsl_device *device,
				struct kgsl_context *context,
				unsigned int timestamp,
				unsigned int msecs)
{
	int status = -EINVAL;

	/* Don't wait forever, set a max of Z180_IDLE_TIMEOUT */
	if (msecs == -1)
		msecs = Z180_IDLE_TIMEOUT;

	mutex_unlock(&device->mutex);
	status = z180_wait(device, context, timestamp, msecs);
	mutex_lock(&device->mutex);

	return status;
}

static int z180_wait(struct kgsl_device *device,
				struct kgsl_context *context,
				unsigned int timestamp,
				unsigned int msecs)
{
	int status = -EINVAL;
	long timeout = 0;

	timeout = wait_io_event_interruptible_timeout(
			device->wait_queue,
			kgsl_check_timestamp(device, context, timestamp),
			msecs_to_jiffies(msecs));

	if (timeout > 0)
		status = 0;
	else if (timeout == 0) {
		status = -ETIMEDOUT;
		kgsl_pwrctrl_set_state(device, KGSL_STATE_HUNG);
		kgsl_postmortem_dump(device, 0);
	} else
		status = timeout;

	return status;
}

struct kgsl_context *
z180_drawctxt_create(struct kgsl_device_private *dev_priv,
			uint32_t *flags)
{
	int ret;
	struct kgsl_context *context = kzalloc(sizeof(*context), GFP_KERNEL);
	if (context == NULL)
		return ERR_PTR(-ENOMEM);
	ret = kgsl_context_init(dev_priv, context);
	if (ret != 0) {
		kfree(context);
		return ERR_PTR(ret);
	}
	return context;
}

static void
z180_drawctxt_detach(struct kgsl_context *context)
{
	struct kgsl_device *device;
	struct z180_device *z180_dev;

	device = context->device;
	z180_dev = Z180_DEVICE(device);

	z180_idle(device);

	if (z180_dev->ringbuffer.prevctx == context->id) {
		z180_dev->ringbuffer.prevctx = Z180_INVALID_CONTEXT;
		device->mmu.hwpagetable = device->mmu.defaultpagetable;
		kgsl_setstate(&device->mmu, KGSL_MEMSTORE_GLOBAL,
				KGSL_MMUFLAGS_PTUPDATE);
	}
}

static void
z180_drawctxt_destroy(struct kgsl_context *context)
{
	kfree(context);
}

static void z180_power_stats(struct kgsl_device *device,
			    struct kgsl_power_stats *stats)
{
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;
	s64 tmp = ktime_to_us(ktime_get());

	if (pwr->time == 0) {
		pwr->time = tmp;
		stats->total_time = 0;
		stats->busy_time = 0;
	} else {
		stats->total_time = tmp - pwr->time;
		pwr->time = tmp;
		stats->busy_time = tmp - device->on_time;
		device->on_time = tmp;
	}
}

static void z180_irqctrl(struct kgsl_device *device, unsigned int mask)
{
	/* Control interrupts for Z180 and the Z180 MMU */

	if (mask) {
		z180_regwrite(device, (ADDR_VGC_IRQENABLE >> 2), 3);
		z180_regwrite(device, MH_INTERRUPT_MASK,
			kgsl_mmu_get_int_mask());
	} else {
		z180_regwrite(device, (ADDR_VGC_IRQENABLE >> 2), 0);
		z180_regwrite(device, MH_INTERRUPT_MASK, 0);
	}
}

static unsigned int z180_gpuid(struct kgsl_device *device, unsigned int *chipid)
{
	if (chipid != NULL)
		*chipid = 0;

	/* Standard KGSL gpuid format:
	 * top word is 0x0002 for 2D or 0x0003 for 3D
	 * Bottom word is core specific identifer
	 */

	return (0x0002 << 16) | 180;
}

static const struct kgsl_functable z180_functable = {
	/* Mandatory functions */
	.regread = z180_regread,
	.regwrite = z180_regwrite,
	.idle = z180_idle,
	.isidle = z180_isidle,
	.suspend_context = z180_suspend_context,
	.init = z180_init,
	.start = z180_start,
	.stop = z180_stop,
	.getproperty = z180_getproperty,
	.waittimestamp = z180_waittimestamp,
	.readtimestamp = z180_readtimestamp,
	.issueibcmds = z180_cmdstream_issueibcmds,
	.setup_pt = z180_setup_pt,
	.cleanup_pt = z180_cleanup_pt,
	.power_stats = z180_power_stats,
	.irqctrl = z180_irqctrl,
	.gpuid = z180_gpuid,
	.irq_handler = z180_irq_handler,
	/* Optional functions */
	.drawctxt_create = z180_drawctxt_create,
	.drawctxt_detach = z180_drawctxt_detach,
	.drawctxt_destroy = z180_drawctxt_destroy,
	.ioctl = NULL,
	.postmortem_dump = z180_dump,
};

static struct platform_device_id z180_id_table[] = {
	{ DEVICE_2D0_NAME, (kernel_ulong_t)&device_2d0.dev, },
	{ DEVICE_2D1_NAME, (kernel_ulong_t)&device_2d1.dev, },
	{ },
};
MODULE_DEVICE_TABLE(platform, z180_id_table);

static struct platform_driver z180_platform_driver = {
	.probe = z180_probe,
	.remove = __devexit_p(z180_remove),
	.suspend = kgsl_suspend_driver,
	.resume = kgsl_resume_driver,
	.id_table = z180_id_table,
	.driver = {
		.owner = THIS_MODULE,
		.name = DEVICE_2D_NAME,
		.pm = &kgsl_pm_ops,
	}
};

static int __init kgsl_2d_init(void)
{
	return platform_driver_register(&z180_platform_driver);
}

static void __exit kgsl_2d_exit(void)
{
	platform_driver_unregister(&z180_platform_driver);
}

module_init(kgsl_2d_init);
module_exit(kgsl_2d_exit);

MODULE_DESCRIPTION("2D Graphics driver");
MODULE_VERSION("1.2");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:kgsl_2d");
