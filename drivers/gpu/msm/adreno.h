/* Copyright (c) 2008-2014, The Linux Foundation. All rights reserved.
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
#ifndef __ADRENO_H
#define __ADRENO_H

#include "kgsl_device.h"
#include "adreno_drawctxt.h"
#include "adreno_ringbuffer.h"
#include "adreno_profile.h"
#include "kgsl_iommu.h"
#include <mach/ocmem.h>

#include "a3xx_reg.h"

#define DEVICE_3D_NAME "kgsl-3d"
#define DEVICE_3D0_NAME "kgsl-3d0"

#define ADRENO_DEVICE(device) \
		KGSL_CONTAINER_OF(device, struct adreno_device, dev)

#define ADRENO_CONTEXT(device) \
		KGSL_CONTAINER_OF(device, struct adreno_context, base)

#define ADRENO_CHIPID_CORE(_id) (((_id) >> 24) & 0xFF)
#define ADRENO_CHIPID_MAJOR(_id) (((_id) >> 16) & 0xFF)
#define ADRENO_CHIPID_MINOR(_id) (((_id) >> 8) & 0xFF)
#define ADRENO_CHIPID_PATCH(_id) ((_id) & 0xFF)

/* Flags to control command packet settings */
#define KGSL_CMD_FLAGS_NONE             0
#define KGSL_CMD_FLAGS_PMODE		BIT(0)
#define KGSL_CMD_FLAGS_INTERNAL_ISSUE   BIT(1)
#define KGSL_CMD_FLAGS_WFI              BIT(2)
#define KGSL_CMD_FLAGS_PROFILE		BIT(3)
#define KGSL_CMD_FLAGS_PWRON_FIXUP      BIT(4)
#define KGSL_CMD_FLAGS_MEMLIST          BIT(5)

/* Command identifiers */
#define KGSL_CONTEXT_TO_MEM_IDENTIFIER	0x2EADBEEF
#define KGSL_CMD_IDENTIFIER		0x2EEDFACE
#define KGSL_CMD_INTERNAL_IDENTIFIER	0x2EEDD00D
#define KGSL_START_OF_IB_IDENTIFIER	0x2EADEABE
#define KGSL_END_OF_IB_IDENTIFIER	0x2ABEDEAD
#define KGSL_END_OF_FRAME_IDENTIFIER	0x2E0F2E0F
#define KGSL_NOP_IB_IDENTIFIER	        0x20F20F20
#define KGSL_START_OF_PROFILE_IDENTIFIER	0x2DEFADE1
#define KGSL_END_OF_PROFILE_IDENTIFIER	0x2DEFADE2
#define KGSL_PWRON_FIXUP_IDENTIFIER	0x2AFAFAFA

#ifdef CONFIG_MSM_SCM
#define ADRENO_DEFAULT_PWRSCALE_POLICY  (&kgsl_pwrscale_policy_tz)
#elif defined CONFIG_MSM_SLEEP_STATS_DEVICE
#define ADRENO_DEFAULT_PWRSCALE_POLICY  (&kgsl_pwrscale_policy_idlestats)
#else
#define ADRENO_DEFAULT_PWRSCALE_POLICY  NULL
#endif

void adreno_debugfs_init(struct kgsl_device *device);

#define ADRENO_ISTORE_START 0x5000 /* Istore offset */

#define ADRENO_NUM_CTX_SWITCH_ALLOWED_BEFORE_DRAW	50

/* One cannot wait forever for the core to idle, so set an upper limit to the
 * amount of time to wait for the core to go idle
 */

#define ADRENO_IDLE_TIMEOUT (20 * 1000)

enum adreno_gpurev {
	ADRENO_REV_UNKNOWN = 0,
	ADRENO_REV_A200 = 200,
	ADRENO_REV_A203 = 203,
	ADRENO_REV_A205 = 205,
	ADRENO_REV_A220 = 220,
	ADRENO_REV_A225 = 225,
	ADRENO_REV_A305 = 305,
	ADRENO_REV_A305C = 306,
	ADRENO_REV_A320 = 320,
	ADRENO_REV_A330 = 330,
	ADRENO_REV_A305B = 335,
};

enum coresight_debug_reg {
	DEBUG_BUS_CTL,
	TRACE_STOP_CNT,
	TRACE_START_CNT,
	TRACE_PERIOD_CNT,
	TRACE_CMD,
	TRACE_BUS_CTL,
};

#define ADRENO_SOFT_FAULT BIT(0)
#define ADRENO_HARD_FAULT BIT(1)
#define ADRENO_TIMEOUT_FAULT BIT(2)
#define ADRENO_IOMMU_PAGE_FAULT BIT(3)

/*
 * Maximum size of the dispatcher ringbuffer - the actual inflight size will be
 * smaller then this but this size will allow for a larger range of inflight
 * sizes that can be chosen at runtime
 */

#define ADRENO_DISPATCH_CMDQUEUE_SIZE 128

/**
 * struct adreno_dispatcher - container for the adreno GPU dispatcher
 * @mutex: Mutex to protect the structure
 * @state: Current state of the dispatcher (active or paused)
 * @timer: Timer to monitor the progress of the command batches
 * @inflight: Number of command batch operations pending in the ringbuffer
 * @fault: Non-zero if a fault was detected.
 * @pending: Priority list of contexts waiting to submit command batches
 * @plist_lock: Spin lock to protect the pending queue
 * @cmdqueue: Queue of command batches currently flight
 * @head: pointer to the head of of the cmdqueue.  This is the oldest pending
 * operation
 * @tail: pointer to the tail of the cmdqueue.  This is the most recently
 * submitted operation
 * @work: work_struct to put the dispatcher in a work queue
 * @kobj: kobject for the dispatcher directory in the device sysfs node
 */
struct adreno_dispatcher {
	struct mutex mutex;
	unsigned long priv;
	struct timer_list timer;
	struct timer_list fault_timer;
	unsigned int inflight;
	atomic_t fault;
	struct plist_head pending;
	spinlock_t plist_lock;
	struct kgsl_cmdbatch *cmdqueue[ADRENO_DISPATCH_CMDQUEUE_SIZE];
	unsigned int head;
	unsigned int tail;
	struct work_struct work;
	struct kobject kobj;
};

enum adreno_dispatcher_flags {
	ADRENO_DISPATCHER_POWER = 0,
};

struct adreno_gpudev;

struct adreno_busy_data {
	unsigned int gpu_busy;
	unsigned int vbif_ram_cycles;
	unsigned int vbif_starved_ram;
};

struct adreno_device {
	struct kgsl_device dev;    /* Must be first field in this struct */
	unsigned long priv;
	unsigned int chip_id;
	enum adreno_gpurev gpurev;
	unsigned long gmem_base;
	unsigned int gmem_size;
	struct adreno_context *drawctxt_active;
	const char *pfp_fwfile;
	unsigned int *pfp_fw;
	size_t pfp_fw_size;
	unsigned int pfp_fw_version;
	const char *pm4_fwfile;
	unsigned int *pm4_fw;
	size_t pm4_fw_size;
	unsigned int pm4_fw_version;
	struct adreno_ringbuffer ringbuffer;
	unsigned int mharb;
	struct adreno_gpudev *gpudev;
	unsigned int wait_timeout;
	unsigned int pm4_jt_idx;
	unsigned int pm4_jt_addr;
	unsigned int pm4_bstrp_size;
	unsigned int pfp_jt_idx;
	unsigned int pfp_jt_addr;
	unsigned int pfp_bstrp_size;
	unsigned int pfp_bstrp_ver;
	unsigned int istore_size;
	unsigned int pix_shader_start;
	unsigned int instruction_size;
	unsigned int ib_check_level;
	unsigned int fast_hang_detect;
	unsigned int ft_policy;
	unsigned int long_ib_detect;
	unsigned int ft_pf_policy;
	unsigned int gpulist_index;
	struct ocmem_buf *ocmem_hdl;
	unsigned int ocmem_base;
	struct adreno_profile profile;
	struct kgsl_memdesc pwron_fixup;
	unsigned int pwron_fixup_dwords;
	struct adreno_dispatcher dispatcher;
	struct adreno_busy_data busy_data;

	struct work_struct input_work;
	unsigned int ram_cycles_lo;
};

/**
 * enum adreno_device_flags - Private flags for the adreno_device
 * @ADRENO_DEVICE_PWRON - Set during init after a power collapse
 * @ADRENO_DEVICE_PWRON_FIXUP - Set if the target requires the shader fixup
 * after power collapse
 */
enum adreno_device_flags {
	ADRENO_DEVICE_PWRON = 0,
	ADRENO_DEVICE_PWRON_FIXUP = 1,
	ADRENO_DEVICE_INITIALIZED = 2,
	ADRENO_DEVICE_STARTED = 3,
	ADRENO_DEVICE_HANG_INTR = 4,
};

#define PERFCOUNTER_FLAG_NONE 0x0
#define PERFCOUNTER_FLAG_KERNEL 0x1

/* Structs to maintain the list of active performance counters */

/**
 * struct adreno_perfcount_register: register state
 * @countable: countable the register holds
 * @kernelcount: number of user space users of the register
 * @usercount: number of kernel users of the register
 * @offset: register hardware offset
 * @load_bit: The bit number in LOAD register which corresponds to this counter
 * @select: The countable register offset
 * @value: The 64 bit countable register value
 */
struct adreno_perfcount_register {
	unsigned int countable;
	unsigned int kernelcount;
	unsigned int usercount;
	unsigned int offset;
	unsigned int offset_hi;
	int load_bit;
	unsigned int select;
	uint64_t value;
};

/**
 * struct adreno_perfcount_group: registers for a hardware group
 * @regs: available registers for this group
 * @reg_count: total registers for this group
 * @name: group name for this group
 */
struct adreno_perfcount_group {
	struct adreno_perfcount_register *regs;
	unsigned int reg_count;
	const char *name;
	unsigned long flags;
};

/*
 * ADRENO_PERFCOUNTER_GROUP_FIXED indicates that a perfcounter group is fixed -
 * instead of having configurable countables like the other groups, registers in
 * fixed groups have a hardwired countable.  So when the user requests a
 * countable in one of these groups, that countable should be used as the
 * register offset to return
 */

#define ADRENO_PERFCOUNTER_GROUP_FIXED BIT(0)

/**
 * adreno_perfcounts: all available perfcounter groups
 * @groups: available groups for this device
 * @group_count: total groups for this device
 */
struct adreno_perfcounters {
	struct adreno_perfcount_group *groups;
	unsigned int group_count;
};

#define ADRENO_PERFCOUNTER_GROUP(core, name) { core##_perfcounters_##name, \
	ARRAY_SIZE(core##_perfcounters_##name), __stringify(name), 0 }

#define ADRENO_PERFCOUNTER_GROUP_FLAGS(core, name, flags) \
	{ core##_perfcounters_##name, \
	ARRAY_SIZE(core##_perfcounters_##name), __stringify(name), flags }

/**
 * adreno_regs: List of registers that are used in kgsl driver for all
 * 3D devices. Each device type has different offset value for the same
 * register, so an array of register offsets are declared for every device
 * and are indexed by the enumeration values defined in this enum
 */
enum adreno_regs {
	ADRENO_REG_CP_DEBUG,
	ADRENO_REG_CP_ME_RAM_WADDR,
	ADRENO_REG_CP_ME_RAM_DATA,
	ADRENO_REG_CP_PFP_UCODE_DATA,
	ADRENO_REG_CP_PFP_UCODE_ADDR,
	ADRENO_REG_CP_WFI_PEND_CTR,
	ADRENO_REG_CP_RB_BASE,
	ADRENO_REG_CP_RB_RPTR_ADDR,
	ADRENO_REG_CP_RB_RPTR,
	ADRENO_REG_CP_RB_WPTR,
	ADRENO_REG_CP_PROTECT_CTRL,
	ADRENO_REG_CP_ME_CNTL,
	ADRENO_REG_CP_RB_CNTL,
	ADRENO_REG_CP_IB1_BASE,
	ADRENO_REG_CP_IB1_BUFSZ,
	ADRENO_REG_CP_IB2_BASE,
	ADRENO_REG_CP_IB2_BUFSZ,
	ADRENO_REG_CP_TIMESTAMP,
	ADRENO_REG_CP_HW_FAULT,
	ADRENO_REG_SCRATCH_ADDR,
	ADRENO_REG_SCRATCH_UMSK,
	ADRENO_REG_SCRATCH_REG2,
	ADRENO_REG_RBBM_STATUS,
	ADRENO_REG_RBBM_PERFCTR_CTL,
	ADRENO_REG_RBBM_PERFCTR_LOAD_CMD0,
	ADRENO_REG_RBBM_PERFCTR_LOAD_CMD1,
	ADRENO_REG_RBBM_PERFCTR_LOAD_CMD2,
	ADRENO_REG_RBBM_PERFCTR_PWR_1_LO,
	ADRENO_REG_RBBM_INT_0_MASK,
	ADRENO_REG_RBBM_INT_0_STATUS,
	ADRENO_REG_RBBM_AHB_ERROR_STATUS,
	ADRENO_REG_RBBM_PM_OVERRIDE2,
	ADRENO_REG_VPC_VPC_DEBUG_RAM_SEL,
	ADRENO_REG_VPC_VPC_DEBUG_RAM_READ,
	ADRENO_REG_VSC_PIPE_DATA_ADDRESS_0,
	ADRENO_REG_VSC_PIPE_DATA_LENGTH_7,
	ADRENO_REG_VSC_SIZE_ADDRESS,
	ADRENO_REG_VFD_CONTROL_0,
	ADRENO_REG_VFD_FETCH_INSTR_0_0,
	ADRENO_REG_VFD_FETCH_INSTR_1_F,
	ADRENO_REG_VFD_INDEX_MAX,
	ADRENO_REG_SP_VS_PVT_MEM_ADDR_REG,
	ADRENO_REG_SP_FS_PVT_MEM_ADDR_REG,
	ADRENO_REG_SP_VS_OBJ_START_REG,
	ADRENO_REG_SP_FS_OBJ_START_REG,
	ADRENO_REG_PA_SC_AA_CONFIG,
	ADRENO_REG_SQ_GPR_MANAGEMENT,
	ADRENO_REG_SQ_INST_STORE_MANAGMENT,
	ADRENO_REG_TC_CNTL_STATUS,
	ADRENO_REG_TP0_CHICKEN,
	ADRENO_REG_RBBM_RBBM_CTL,
	ADRENO_REG_UCHE_INVALIDATE0,
	ADRENO_REG_REGISTER_MAX,
};

/**
 * adreno_reg_offsets: Holds array of register offsets
 * @offsets: Offset array of size defined by enum adreno_regs
 * @offset_0: This is the index of the register in offset array whose value
 * is 0. 0 is a valid register offset and during initialization of the
 * offset array we need to know if an offset value is correctly defined to 0
 */
struct adreno_reg_offsets {
	unsigned int *offsets;
	enum adreno_regs offset_0;
};

#define ADRENO_REG_UNUSED	0xFFFFFFFF
#define ADRENO_REG_DEFINE(_offset, _reg) [_offset] = _reg

struct adreno_gpudev {
	/*
	 * These registers are in a different location on different devices,
	 * so define them in the structure and use them as variables.
	 */
	struct adreno_reg_offsets *reg_offsets;
	/* keeps track of when we need to execute the draw workaround code */
	int ctx_switches_since_last_draw;

	struct adreno_perfcounters *perfcounters;

	/* GPU specific function hooks */
	int (*ctxt_create)(struct adreno_device *, struct adreno_context *);
	irqreturn_t (*irq_handler)(struct adreno_device *);
	void (*irq_control)(struct adreno_device *, int);
	unsigned int (*irq_pending)(struct adreno_device *);
	void * (*snapshot)(struct adreno_device *, void *, int *, int);
	int (*rb_init)(struct adreno_device *, struct adreno_ringbuffer *);
	int (*perfcounter_init)(struct adreno_device *);
	void (*perfcounter_close)(struct adreno_device *);
	void (*perfcounter_save)(struct adreno_device *);
	void (*perfcounter_restore)(struct adreno_device *);
	void (*fault_detect_start)(struct adreno_device *);
	void (*fault_detect_stop)(struct adreno_device *);
	void (*start)(struct adreno_device *);
	int (*perfcounter_enable)(struct adreno_device *, unsigned int group,
		unsigned int counter, unsigned int countable);
	void (*busy_cycles)(struct adreno_device *, struct adreno_busy_data *);
	uint64_t (*perfcounter_read)(struct adreno_device *adreno_dev,
		unsigned int group, unsigned int counter);
	void (*perfcounter_write)(struct adreno_device *adreno_dev,
		unsigned int group, unsigned int counter);
	int (*coresight_enable) (struct kgsl_device *device);
	void (*coresight_disable) (struct kgsl_device *device);
	void (*coresight_config_debug_reg) (struct kgsl_device *device,
			int debug_reg, unsigned int val);
	void (*soft_reset)(struct adreno_device *device);
	void (*postmortem_dump)(struct adreno_device *adreno_dev);
};

#define FT_DETECT_REGS_COUNT 14

struct log_field {
	bool show;
	const char *display;
};

/* Fault Tolerance policy flags */
#define  KGSL_FT_OFF                      0
#define  KGSL_FT_REPLAY                   1
#define  KGSL_FT_SKIPIB                   2
#define  KGSL_FT_SKIPFRAME                3
#define  KGSL_FT_DISABLE                  4
#define  KGSL_FT_TEMP_DISABLE             5
#define  KGSL_FT_THROTTLE                 6
#define  KGSL_FT_SKIPCMD                  7
#define  KGSL_FT_DEFAULT_POLICY (BIT(KGSL_FT_REPLAY) + BIT(KGSL_FT_SKIPCMD) \
				+ BIT(KGSL_FT_THROTTLE))

/* This internal bit is used to skip the PM dump on replayed command batches */
#define  KGSL_FT_SKIP_PMDUMP              31

/* Pagefault policy flags */
#define KGSL_FT_PAGEFAULT_INT_ENABLE         BIT(0)
#define KGSL_FT_PAGEFAULT_GPUHALT_ENABLE     BIT(1)
#define KGSL_FT_PAGEFAULT_LOG_ONE_PER_PAGE   BIT(2)
#define KGSL_FT_PAGEFAULT_LOG_ONE_PER_INT    BIT(3)
#define KGSL_FT_PAGEFAULT_DEFAULT_POLICY     KGSL_FT_PAGEFAULT_INT_ENABLE

#define ADRENO_FT_TYPES \
	{ BIT(KGSL_FT_OFF), "off" }, \
	{ BIT(KGSL_FT_REPLAY), "replay" }, \
	{ BIT(KGSL_FT_SKIPIB), "skipib" }, \
	{ BIT(KGSL_FT_SKIPFRAME), "skipframe" }, \
	{ BIT(KGSL_FT_DISABLE), "disable" }, \
	{ BIT(KGSL_FT_TEMP_DISABLE), "temp" }, \
	{ BIT(KGSL_FT_THROTTLE), "throttle"}, \
	{ BIT(KGSL_FT_SKIPCMD), "skipcmd" }

#define ADRENO_CMDBATCH_FLAGS \
	{ KGSL_CMDBATCH_CTX_SWITCH, "CTX_SWITCH" }, \
	{ KGSL_CMDBATCH_SYNC, "SYNC" }, \
	{ KGSL_CMDBATCH_END_OF_FRAME, "EOF" }, \
	{ KGSL_CMDBATCH_PWR_CONSTRAINT, "PWR_CONSTRAINT" }

extern struct adreno_gpudev adreno_a2xx_gpudev;
extern struct adreno_gpudev adreno_a3xx_gpudev;

/* A2XX register sets defined in adreno_a2xx.c */
extern const unsigned int a200_registers[];
extern const unsigned int a220_registers[];
extern const unsigned int a225_registers[];
extern const unsigned int a200_registers_count;
extern const unsigned int a220_registers_count;
extern const unsigned int a225_registers_count;

/* A3XX register set defined in adreno_a3xx.c */
extern const unsigned int a3xx_registers[];
extern const unsigned int a3xx_registers_count;

extern const unsigned int a3xx_hlsq_registers[];
extern const unsigned int a3xx_hlsq_registers_count;

extern const unsigned int a330_registers[];
extern const unsigned int a330_registers_count;

extern unsigned int ft_detect_regs[];

int adreno_coresight_enable(struct coresight_device *csdev);
void adreno_coresight_disable(struct coresight_device *csdev);
void adreno_coresight_remove(struct platform_device *pdev);
int adreno_coresight_init(struct platform_device *pdev);

bool adreno_hw_isidle(struct kgsl_device *device);
int adreno_idle(struct kgsl_device *device);
bool adreno_isidle(struct kgsl_device *device);

void adreno_shadermem_regread(struct kgsl_device *device,
						unsigned int offsetwords,
						unsigned int *value);

int adreno_dump(struct kgsl_device *device, int manual);
void adreno_dump_fields(struct kgsl_device *device,
			const char *start, const struct log_field *lines,
			int num);
unsigned int adreno_a3xx_rbbm_clock_ctl_default(struct adreno_device
							*adreno_dev);

struct kgsl_memdesc *adreno_find_region(struct kgsl_device *device,
						phys_addr_t pt_base,
						unsigned int gpuaddr,
						unsigned int size,
						struct kgsl_mem_entry **entry);

uint8_t *adreno_convertaddr(struct kgsl_device *device,
	phys_addr_t pt_base, unsigned int gpuaddr, unsigned int size,
	struct kgsl_mem_entry **entry);

struct kgsl_memdesc *adreno_find_ctxtmem(struct kgsl_device *device,
	phys_addr_t pt_base, unsigned int gpuaddr, unsigned int size);

void *adreno_snapshot(struct kgsl_device *device, void *snapshot, int *remain,
		int hang);

void adreno_dispatcher_start(struct kgsl_device *device);
int adreno_dispatcher_init(struct adreno_device *adreno_dev);
void adreno_dispatcher_close(struct adreno_device *adreno_dev);
int adreno_dispatcher_idle(struct adreno_device *adreno_dev,
		unsigned int timeout);
void adreno_dispatcher_irq_fault(struct kgsl_device *device);
void adreno_dispatcher_stop(struct adreno_device *adreno_dev);

int adreno_dispatcher_queue_cmd(struct adreno_device *adreno_dev,
		struct adreno_context *drawctxt, struct kgsl_cmdbatch *cmdbatch,
		uint32_t *timestamp);

void adreno_dispatcher_schedule(struct kgsl_device *device);
void adreno_dispatcher_pause(struct adreno_device *adreno_dev);
void adreno_dispatcher_queue_context(struct kgsl_device *device,
	struct adreno_context *drawctxt);
int adreno_reset(struct kgsl_device *device);

int adreno_ft_init_sysfs(struct kgsl_device *device);
void adreno_ft_uninit_sysfs(struct kgsl_device *device);

void adreno_fault_skipcmd_detached(struct kgsl_device *device,
					 struct adreno_context *drawctxt,
					 struct kgsl_cmdbatch *cmdbatch);

int adreno_perfcounter_get_groupid(struct adreno_device *adreno_dev,
					const char *name);

const char *adreno_perfcounter_get_name(struct adreno_device
					*adreno_dev, unsigned int groupid);

int adreno_perfcounter_get(struct adreno_device *adreno_dev,
	unsigned int groupid, unsigned int countable, unsigned int *offset,
	unsigned int *offset_hi, unsigned int flags);

int adreno_perfcounter_put(struct adreno_device *adreno_dev,
	unsigned int groupid, unsigned int countable, unsigned int flags);

int adreno_soft_reset(struct kgsl_device *device);

int adreno_a3xx_pwron_fixup_init(struct adreno_device *adreno_dev);

static inline int adreno_is_a200(struct adreno_device *adreno_dev)
{
	return (adreno_dev->gpurev == ADRENO_REV_A200);
}

static inline int adreno_is_a203(struct adreno_device *adreno_dev)
{
	return (adreno_dev->gpurev == ADRENO_REV_A203);
}

static inline int adreno_is_a205(struct adreno_device *adreno_dev)
{
	return (adreno_dev->gpurev == ADRENO_REV_A205);
}

static inline int adreno_is_a20x(struct adreno_device *adreno_dev)
{
	return (adreno_dev->gpurev <= 209);
}

static inline int adreno_is_a220(struct adreno_device *adreno_dev)
{
	return (adreno_dev->gpurev == ADRENO_REV_A220);
}

static inline int adreno_is_a225(struct adreno_device *adreno_dev)
{
	return (adreno_dev->gpurev == ADRENO_REV_A225);
}

static inline int adreno_is_a22x(struct adreno_device *adreno_dev)
{
	return (adreno_dev->gpurev  == ADRENO_REV_A220 ||
		adreno_dev->gpurev == ADRENO_REV_A225);
}

static inline int adreno_is_a2xx(struct adreno_device *adreno_dev)
{
	return (adreno_dev->gpurev <= 299);
}

static inline int adreno_is_a3xx(struct adreno_device *adreno_dev)
{
	return (adreno_dev->gpurev >= 300);
}

static inline int adreno_is_a305(struct adreno_device *adreno_dev)
{
	return (adreno_dev->gpurev == ADRENO_REV_A305);
}

static inline int adreno_is_a305b(struct adreno_device *adreno_dev)
{
	return (adreno_dev->gpurev == ADRENO_REV_A305B);
}

static inline int adreno_is_a305c(struct adreno_device *adreno_dev)
{
	return (adreno_dev->gpurev == ADRENO_REV_A305C);
}

static inline int adreno_is_a320(struct adreno_device *adreno_dev)
{
	return (adreno_dev->gpurev == ADRENO_REV_A320);
}

static inline int adreno_is_a330(struct adreno_device *adreno_dev)
{
	return (adreno_dev->gpurev == ADRENO_REV_A330);
}

static inline int adreno_is_a330v2(struct adreno_device *adreno_dev)
{
	return ((adreno_dev->gpurev == ADRENO_REV_A330) &&
		(ADRENO_CHIPID_PATCH(adreno_dev->chip_id) > 0));
}

static inline int adreno_rb_ctxtswitch(unsigned int *cmd)
{
	return (cmd[0] == cp_nop_packet(1) &&
		cmd[1] == KGSL_CONTEXT_TO_MEM_IDENTIFIER);
}

/**
 * adreno_context_timestamp() - Return the last queued timestamp for the context
 * @k_ctxt: Pointer to the KGSL context to query
 * @rb: Pointer to the ringbuffer structure for the GPU
 *
 * Return the last queued context for the given context. This is used to verify
 * that incoming requests are not using an invalid (unsubmitted) timestamp
 */
static inline int adreno_context_timestamp(struct kgsl_context *k_ctxt,
		struct adreno_ringbuffer *rb)
{
	if (k_ctxt) {
		struct adreno_context *a_ctxt = ADRENO_CONTEXT(k_ctxt);
		return a_ctxt->timestamp;
	}
	return rb->global_ts;
}

/**
 * adreno_encode_istore_size - encode istore size in CP format
 * @adreno_dev - The 3D device.
 *
 * Encode the istore size into the format expected that the
 * CP_SET_SHADER_BASES and CP_ME_INIT commands:
 * bits 31:29 - istore size as encoded by this function
 * bits 27:16 - vertex shader start offset in instructions
 * bits 11:0 - pixel shader start offset in instructions.
 */
static inline int adreno_encode_istore_size(struct adreno_device *adreno_dev)
{
	unsigned int size;
	/* in a225 the CP microcode multiplies the encoded
	 * value by 3 while decoding.
	 */
	if (adreno_is_a225(adreno_dev))
		size = adreno_dev->istore_size/3;
	else
		size = adreno_dev->istore_size;

	return (ilog2(size) - 5) << 29;
}

static inline int __adreno_add_idle_indirect_cmds(unsigned int *cmds,
						unsigned int nop_gpuaddr)
{
	/* Adding an indirect buffer ensures that the prefetch stalls until
	 * the commands in indirect buffer have completed. We need to stall
	 * prefetch with a nop indirect buffer when updating pagetables
	 * because it provides stabler synchronization */
	*cmds++ = CP_HDR_INDIRECT_BUFFER_PFD;
	*cmds++ = nop_gpuaddr;
	*cmds++ = 2;
	*cmds++ = cp_type3_packet(CP_WAIT_FOR_IDLE, 1);
	*cmds++ = 0x00000000;
	return 5;
}

static inline int adreno_add_change_mh_phys_limit_cmds(unsigned int *cmds,
						unsigned int new_phys_limit,
						unsigned int nop_gpuaddr)
{
	unsigned int *start = cmds;

	*cmds++ = cp_type0_packet(MH_MMU_MPU_END, 1);
	*cmds++ = new_phys_limit;
	cmds += __adreno_add_idle_indirect_cmds(cmds, nop_gpuaddr);
	return cmds - start;
}

static inline int adreno_add_bank_change_cmds(unsigned int *cmds,
					int cur_ctx_bank,
					unsigned int nop_gpuaddr)
{
	unsigned int *start = cmds;

	*cmds++ = cp_type0_packet(REG_CP_STATE_DEBUG_INDEX, 1);
	*cmds++ = (cur_ctx_bank ? 0 : 0x20);
	cmds += __adreno_add_idle_indirect_cmds(cmds, nop_gpuaddr);
	return cmds - start;
}

/*
 * adreno_read_cmds - Add pm4 packets to perform read
 * @device - Pointer to device structure
 * @cmds - Pointer to memory where read commands need to be added
 * @addr - gpu address of the read
 * @val - The GPU will wait until the data at address addr becomes
 * equal to value
 */
static inline int adreno_add_read_cmds(struct kgsl_device *device,
				unsigned int *cmds, unsigned int addr,
				unsigned int val, unsigned int nop_gpuaddr)
{
	unsigned int *start = cmds;

	*cmds++ = cp_type3_packet(CP_WAIT_REG_MEM, 5);
	/* MEM SPACE = memory, FUNCTION = equals */
	*cmds++ = 0x13;
	*cmds++ = addr;
	*cmds++ = val;
	*cmds++ = 0xFFFFFFFF;
	*cmds++ = 0xFFFFFFFF;

	/* WAIT_REG_MEM turns back on protected mode - push it off */
	*cmds++ = cp_type3_packet(CP_SET_PROTECTED_MODE, 1);
	*cmds++ = 0;

	cmds += __adreno_add_idle_indirect_cmds(cmds, nop_gpuaddr);
	return cmds - start;
}

/*
 * adreno_idle_cmds - Add pm4 packets for GPU idle
 * @adreno_dev - Pointer to device structure
 * @cmds - Pointer to memory where idle commands need to be added
 */
static inline int adreno_add_idle_cmds(struct adreno_device *adreno_dev,
							unsigned int *cmds)
{
	unsigned int *start = cmds;

	*cmds++ = cp_type3_packet(CP_WAIT_FOR_IDLE, 1);
	*cmds++ = 0;

	if (adreno_is_a3xx(adreno_dev)) {
		*cmds++ = cp_type3_packet(CP_WAIT_FOR_ME, 1);
		*cmds++ = 0;
	}

	return cmds - start;
}

/*
 * adreno_wait_reg_eq() - Add a CP_WAIT_REG_EQ command
 * @cmds:	Pointer to memory where commands are to be added
 * @addr:	Regiater address to poll for
 * @val:	Value to poll for
 * @mask:	The value against which register value is masked
 * @interval:	wait interval
 */
static inline int adreno_wait_reg_eq(unsigned int *cmds, unsigned int addr,
					unsigned int val, unsigned int mask,
					unsigned int interval)
{
	unsigned int *start = cmds;
	*cmds++ = cp_type3_packet(CP_WAIT_REG_EQ, 4);
	*cmds++ = addr;
	*cmds++ = val;
	*cmds++ = mask;
	*cmds++ = interval;
	return cmds - start;
}

/*
 * adreno_checkreg_off() - Checks the validity of a register enum
 * @adreno_dev:		Pointer to adreno device
 * @offset_name:	The register enum that is checked
 */
static inline bool adreno_checkreg_off(struct adreno_device *adreno_dev,
					enum adreno_regs offset_name)
{
	if (offset_name >= ADRENO_REG_REGISTER_MAX ||
		ADRENO_REG_UNUSED ==
			adreno_dev->gpudev->reg_offsets->offsets[offset_name]) {
		BUG_ON(1);
	}
	return true;
}

/*
 * adreno_readreg() - Read a register by getting its offset from the
 * offset array defined in gpudev node
 * @adreno_dev:		Pointer to the the adreno device
 * @offset_name:	The register enum that is to be read
 * @val:		Register value read is placed here
 */
static inline void adreno_readreg(struct adreno_device *adreno_dev,
				enum adreno_regs offset_name, unsigned int *val)
{
	struct kgsl_device *device = &adreno_dev->dev;
	if (adreno_checkreg_off(adreno_dev, offset_name))
		kgsl_regread(device,
			adreno_dev->gpudev->reg_offsets->offsets[offset_name],
								val);
}

/*
 * adreno_writereg() - Write a register by getting its offset from the
 * offset array defined in gpudev node
 * @adreno_dev:		Pointer to the the adreno device
 * @offset_name:	The register enum that is to be written
 * @val:		Value to write
 */
static inline void adreno_writereg(struct adreno_device *adreno_dev,
				enum adreno_regs offset_name, unsigned int val)
{
	struct kgsl_device *device = &adreno_dev->dev;
	if (adreno_checkreg_off(adreno_dev, offset_name))
		kgsl_regwrite(device,
		adreno_dev->gpudev->reg_offsets->offsets[offset_name], val);
}

/*
 * adreno_getreg() - Returns the offset value of a register from the
 * register offset array in the gpudev node
 * @adreno_dev:		Pointer to the the adreno device
 * @offset_name:	The register enum whore offset is returned
 */
static inline unsigned int adreno_getreg(struct adreno_device *adreno_dev,
				enum adreno_regs offset_name)
{
	if (!adreno_checkreg_off(adreno_dev, offset_name))
		return ADRENO_REG_REGISTER_MAX;
	return adreno_dev->gpudev->reg_offsets->offsets[offset_name];
}

#ifdef CONFIG_DEBUG_FS
void adreno_debugfs_init(struct kgsl_device *device);
#else
static inline void adreno_debugfs_init(struct kgsl_device *device) { }
#endif

/**
 * adreno_gpu_fault() - Return the current state of the GPU
 * @adreno_dev: A ponter to the adreno_device to query
 *
 * Return 0 if there is no fault or positive with the last type of fault that
 * occurred
 */
static inline unsigned int adreno_gpu_fault(struct adreno_device *adreno_dev)
{
	smp_rmb();
	return atomic_read(&adreno_dev->dispatcher.fault);
}

/**
 * adreno_set_gpu_fault() - Set the current fault status of the GPU
 * @adreno_dev: A pointer to the adreno_device to set
 * @state: fault state to set
 *
 */
static inline void adreno_set_gpu_fault(struct adreno_device *adreno_dev,
	int state)
{
	/* only set the fault bit w/o overwriting other bits */
	atomic_add(state, &adreno_dev->dispatcher.fault);
	smp_wmb();
}

/**
 * adreno_clear_gpu_fault() - Clear the GPU fault register
 * @adreno_dev: A pointer to an adreno_device structure
 *
 * Clear the GPU fault status for the adreno device
 */

static inline void adreno_clear_gpu_fault(struct adreno_device *adreno_dev)
{
	atomic_set(&adreno_dev->dispatcher.fault, 0);
	smp_wmb();
}

/*
 * adreno_bootstrap_ucode() - Checks if Ucode bootstrapping is supported
 * @adreno_dev:		Pointer to the the adreno device
 */
static inline int adreno_bootstrap_ucode(struct adreno_device *adreno_dev)
{
	if ((adreno_dev->pfp_bstrp_size) && (adreno_dev->pm4_bstrp_size)
		&& (adreno_dev->pfp_fw_version >= adreno_dev->pfp_bstrp_ver))
		return 1;
	else
		return 0;
}

/**
 * adreno_get_rptr() - Get the current ringbuffer read pointer
 * @rb: Pointer the ringbuffer to query
 *
 * Get the current read pointer from the GPU register.
 */
static inline unsigned int
adreno_get_rptr(struct adreno_ringbuffer *rb)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(rb->device);
	unsigned int result;
	adreno_readreg(adreno_dev, ADRENO_REG_CP_RB_RPTR, &result);
	return result;
}

/*
 * adreno_set_protected_registers() - Protect the specified range of registers
 * from being accessed by the GPU
 * @device: pointer to the KGSL device
 * @index: Pointer to the index of the protect mode register to write to
 * @reg: Starting dword register to write
 * @mask_len: Size of the mask to protect (# of registers = 2 ** mask_len)
 *
 * Add the range of registers to the list of protected mode registers that will
 * cause an exception if the GPU accesses them.  There are 16 available
 * protected mode registers.  Index is used to specify which register to write
 * to - the intent is to call this function multiple times with the same index
 * pointer for each range and the registers will be magically programmed in
 * incremental fashion
 */
static inline void adreno_set_protected_registers(struct kgsl_device *device,
	unsigned int *index, unsigned int reg, int mask_len)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	unsigned int val;

	/* This function is only for adreno A3XX and beyond */
	BUG_ON(adreno_is_a2xx(adreno_dev));

	/* There are only 16 registers available */
	BUG_ON(*index >= 16);

	val = 0x60000000 | ((mask_len & 0x1F) << 24) | ((reg << 2) & 0x1FFFF);

	/*
	 * Write the protection range to the next available protection
	 * register
	 */

	kgsl_regwrite(device, A3XX_CP_PROTECT_REG_0 + *index, val);
	*index = *index + 1;
}

#ifdef CONFIG_DEBUG_FS
void adreno_debugfs_init(struct kgsl_device *device);
#else
static inline void adreno_debugfs_init(struct kgsl_device *device) { }
#endif

#endif /*__ADRENO_H */
