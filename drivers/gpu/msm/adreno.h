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
#include <linux/stat.h>
#include <linux/delay.h>

#ifdef CONFIG_MSM_OCMEM
#include <soc/qcom/ocmem.h>
#endif

#include "a3xx_reg.h"
#include "a4xx_reg.h"

#define DEVICE_3D_NAME "kgsl-3d"
#define DEVICE_3D0_NAME "kgsl-3d0"

#define ADRENO_PRIORITY_MAX_RB_LEVELS	4

/* ADRENO_DEVICE - Given a kgsl_device return the adreno device struct */
#define ADRENO_DEVICE(device) \
		container_of(device, struct adreno_device, dev)

/* ADRENO_CONTEXT - Given a context return the adreno context struct */
#define ADRENO_CONTEXT(context) \
		container_of(context, struct adreno_context, base)

/* ADRENO_GPU_DEVICE - Given an adreno device return the GPU specific struct */
#define ADRENO_GPU_DEVICE(_a) ((_a)->gpucore->gpudev)

/* ADRENO_PERFCOUNTERS - Given an adreno device, return the perfcounters list */
#define ADRENO_PERFCOUNTERS(_a) \
	(ADRENO_GPU_DEVICE(_a) ? ADRENO_GPU_DEVICE(_a)->perfcounters : NULL)

#define ADRENO_CHIPID_CORE(_id) (((_id) >> 24) & 0xFF)
#define ADRENO_CHIPID_MAJOR(_id) (((_id) >> 16) & 0xFF)
#define ADRENO_CHIPID_MINOR(_id) (((_id) >> 8) & 0xFF)
#define ADRENO_CHIPID_PATCH(_id) ((_id) & 0xFF)

/* ADRENO_GPUREV - Return the GPU ID for the given adreno_device */
#define ADRENO_GPUREV(_a) ((_a)->gpucore->gpurev)

/*
 * ADRENO_FEATURE - return true if the specified feature is supported by the GPU
 * core
 */
#define ADRENO_FEATURE(_dev, _bit) \
	((_dev)->gpucore->features & (_bit))

/* Adreno core features */
/* The core uses OCMEM for GMEM/binning memory */
#define ADRENO_USES_OCMEM     BIT(0)
/* The core requires the TLB to be flushed on map */
#define IOMMU_FLUSH_TLB_ON_MAP BIT(1)
/* The core supports an accelerated warm start */
#define ADRENO_WARM_START     BIT(2)
/* The core supports the microcode bootstrap functionality */
#define ADRENO_USE_BOOTSTRAP  BIT(3)
/* The microcode for the code supports the IOMMU sync lock functionality */
#define ADRENO_HAS_IOMMU_SYNC_LOCK BIT(4)
/* The core supports SP/TP hw controlled power collapse */
#define ADRENO_SPTP_PC BIT(5)

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
	ADRENO_REV_A306 = 307,
	ADRENO_REV_A310 = 310,
	ADRENO_REV_A320 = 320,
	ADRENO_REV_A330 = 330,
	ADRENO_REV_A305B = 335,
	ADRENO_REV_A405 = 405,
	ADRENO_REV_A420 = 420,
	ADRENO_REV_A430 = 430,
};

#define ADRENO_SOFT_FAULT BIT(0)
#define ADRENO_HARD_FAULT BIT(1)
#define ADRENO_TIMEOUT_FAULT BIT(2)
#define ADRENO_IOMMU_PAGE_FAULT BIT(3)

#define ADRENO_SPTP_PC_CTRL BIT(0)

/*
 * Maximum size of the dispatcher ringbuffer - the actual inflight size will be
 * smaller then this but this size will allow for a larger range of inflight
 * sizes that can be chosen at runtime
 */

#define ADRENO_DISPATCH_CMDQUEUE_SIZE 128

#define CMDQUEUE_NEXT(_i, _s) (((_i) + 1) % (_s))

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
 * @idle_gate: Gate to wait on for dispatcher to idle
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
	struct completion idle_gate;
};

enum adreno_dispatcher_flags {
	ADRENO_DISPATCHER_POWER = 0,
	ADRENO_DISPATCHER_ACTIVE = 1,
};

struct adreno_gpudev;

struct adreno_busy_data {
	unsigned int gpu_busy;
	unsigned int vbif_ram_cycles;
	unsigned int vbif_starved_ram;
};

/**
 * struct adreno_gpu_core - A specific GPU core definition
 * @gpurev: Unique GPU revision identifier
 * @core: Match for the core version of the GPU
 * @major: Match for the major version of the GPU
 * @minor: Match for the minor version of the GPU
 * @patchid: Match for the patch revision of the GPU
 * @features: Common adreno features supported by this core
 * @pm4fw_name: Filename for th PM4 firmware
 * @pfpfw_name: Filename for the PFP firmware
 * @gpudev: Pointer to the GPU family specific functions for this core
 * @gmem_size: Amount of binning memory (GMEM/OCMEM) to reserve for the core
 * @sync_lock_pm4_ver: For IOMMUv0 cores the version of PM4 microcode that
 * supports the sync lock mechanism
 * @sync_lock_pfp_ver: For IOMMUv0 cores the version of PFP microcode that
 * supports the sync lock mechanism
 * @pm4_jt_idx: Index of the jump table in the PM4 microcode
 * @pm4_jt_addr: Address offset to load the jump table for the PM4 microcode
 * @pfp_jt_idx: Index of the jump table in the PFP microcode
 * @pfp_jt_addr: Address offset to load the jump table for the PFP microcode
 * @pm4_bstrp_size: Size of the bootstrap loader for PM4 microcode
 * @pfp_bstrp_size: Size of the bootstrap loader for PFP microcde
 * @pfp_bstrp_ver: Version of the PFP microcode that supports bootstraping
 */
struct adreno_gpu_core {
	enum adreno_gpurev gpurev;
	unsigned int core, major, minor, patchid;
	unsigned long features;
	const char *pm4fw_name;
	const char *pfpfw_name;
	struct adreno_gpudev *gpudev;
	size_t gmem_size;
	unsigned int sync_lock_pm4_ver;
	unsigned int sync_lock_pfp_ver;
	unsigned int pm4_jt_idx;
	unsigned int pm4_jt_addr;
	unsigned int pfp_jt_idx;
	unsigned int pfp_jt_addr;
	unsigned int pm4_bstrp_size;
	unsigned int pfp_bstrp_size;
	unsigned int pfp_bstrp_ver;
};

struct adreno_device {
	struct kgsl_device dev;    /* Must be first field in this struct */
	unsigned long priv;
	unsigned int chipid;
	unsigned long gmem_base;
	unsigned long gmem_size;
	const struct adreno_gpu_core *gpucore;
	unsigned int *pfp_fw;
	size_t pfp_fw_size;
	unsigned int pfp_fw_version;
	unsigned int *pm4_fw;
	size_t pm4_fw_size;
	unsigned int pm4_fw_version;
	struct adreno_ringbuffer ringbuffers[ADRENO_PRIORITY_MAX_RB_LEVELS];
	int num_ringbuffers;
	struct adreno_ringbuffer *cur_rb;
	unsigned int wait_timeout;
	unsigned int ib_check_level;
	unsigned int fast_hang_detect;
	unsigned int ft_policy;
	unsigned int long_ib_detect;
	unsigned int ft_pf_policy;
	struct ocmem_buf *ocmem_hdl;
	struct adreno_profile profile;
	struct adreno_dispatcher dispatcher;
	struct kgsl_memdesc pwron_fixup;
	unsigned int pwron_fixup_dwords;
	struct work_struct start_work;
	struct work_struct input_work;
	struct adreno_busy_data busy_data;
	unsigned int ram_cycles_lo;
	unsigned int starved_ram_lo;
	atomic_t halt;
	struct dentry *ctx_d_debugfs;
	unsigned long pwrctrl_flag;
};

/**
 * enum adreno_device_flags - Private flags for the adreno_device
 * @ADRENO_DEVICE_PWRON - Set during init after a power collapse
 * @ADRENO_DEVICE_PWRON_FIXUP - Set if the target requires the shader fixup
 * after power collapse
 * @ADRENO_DEVICE_CORESIGHT - Set if the coresight (trace bus) registers should
 * be restored after power collapse
 */
enum adreno_device_flags {
	ADRENO_DEVICE_PWRON = 0,
	ADRENO_DEVICE_PWRON_FIXUP = 1,
	ADRENO_DEVICE_INITIALIZED = 2,
	ADRENO_DEVICE_CORESIGHT = 3,
	ADRENO_DEVICE_HANG_INTR = 4,
	ADRENO_DEVICE_STARTED = 5,
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

/**
 * adreno_invalid_countabless: Invalid countables that do not work properly
 * @countables: List of unusable countables
 * @num_countables: Number of unusable countables
 */
struct adreno_invalid_countables {
	const unsigned int *countables;
	int num_countables;
};

#define ADRENO_PERFCOUNTER_GROUP_FLAGS(core, offset, name, flags) \
	[KGSL_PERFCOUNTER_GROUP_##offset] = { core##_perfcounters_##name, \
	ARRAY_SIZE(core##_perfcounters_##name), __stringify(name), flags }

#define ADRENO_PERFCOUNTER_GROUP(core, offset, name) \
	ADRENO_PERFCOUNTER_GROUP_FLAGS(core, offset, name, 0)

#define ADRENO_PERFCOUNTER_INVALID_COUNTABLE(name, off) \
	[KGSL_PERFCOUNTER_GROUP_##off] = { name##_invalid_countables, \
				ARRAY_SIZE(name##_invalid_countables) }

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
	ADRENO_REG_CP_ME_RAM_RADDR,
	ADRENO_REG_CP_ROQ_ADDR,
	ADRENO_REG_CP_ROQ_DATA,
	ADRENO_REG_CP_MERCIU_ADDR,
	ADRENO_REG_CP_MERCIU_DATA,
	ADRENO_REG_CP_MERCIU_DATA2,
	ADRENO_REG_CP_MEQ_ADDR,
	ADRENO_REG_CP_MEQ_DATA,
	ADRENO_REG_CP_HW_FAULT,
	ADRENO_REG_CP_PROTECT_STATUS,
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
	ADRENO_REG_RBBM_AHB_CMD,
	ADRENO_REG_RBBM_INT_CLEAR_CMD,
	ADRENO_REG_RBBM_SW_RESET_CMD,
	ADRENO_REG_RBBM_CLOCK_CTL,
	ADRENO_REG_RBBM_AHB_ME_SPLIT_STATUS,
	ADRENO_REG_RBBM_AHB_PFP_SPLIT_STATUS,
	ADRENO_REG_VPC_DEBUG_RAM_SEL,
	ADRENO_REG_VPC_DEBUG_RAM_READ,
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
	ADRENO_REG_TP0_CHICKEN,
	ADRENO_REG_RBBM_RBBM_CTL,
	ADRENO_REG_UCHE_INVALIDATE0,
	ADRENO_REG_RBBM_PERFCTR_LOAD_VALUE_LO,
	ADRENO_REG_RBBM_PERFCTR_LOAD_VALUE_HI,
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
	unsigned int *const offsets;
	enum adreno_regs offset_0;
};

#define ADRENO_REG_UNUSED	0xFFFFFFFF
#define ADRENO_REG_DEFINE(_offset, _reg) [_offset] = _reg

/*
 * struct adreno_vbif_data - Describes vbif register value pair
 * @reg: Offset to vbif register
 * @val: The value that should be programmed in the register at reg
 */
struct adreno_vbif_data {
	unsigned int reg;
	unsigned int val;
};

/*
 * struct adreno_vbif_platform - Holds an array of vbif reg value pairs
 * for a particular core
 * @devfunc: Pointer to platform/core identification function
 * @vbif: Array of reg value pairs for vbif registers
 */
struct adreno_vbif_platform {
	int(*devfunc)(struct adreno_device *);
	const struct adreno_vbif_data *vbif;
};

/*
 * struct adreno_vbif_snapshot_registers - Holds an array of vbif registers
 * listed for snapshot dump for a particular core
 * @vbif_version: vbif version
 * @vbif_snapshot_registers: vbif registers listed for snapshot dump
 * @vbif_snapshot_registers_count: count of vbif registers listed for snapshot
 */
struct adreno_vbif_snapshot_registers {
	const unsigned int vbif_version;
	const unsigned int *vbif_snapshot_registers;
	const int vbif_snapshot_registers_count;
};

/**
 * struct adreno_coresight_register - Definition for a coresight (tracebus)
 * debug register
 * @offset: Offset of the debug register in the KGSL mmio region
 * @initial: Default value to write when coresight is enabled
 * @value: Current shadow value of the register (to be reprogrammed after power
 * collapse)
 */
struct adreno_coresight_register {
	unsigned int offset;
	unsigned int initial;
	unsigned int value;
};

struct adreno_coresight_attr {
	struct device_attribute attr;
	struct adreno_coresight_register *reg;
};

ssize_t adreno_coresight_show_register(struct device *device,
		struct device_attribute *attr, char *buf);

ssize_t adreno_coresight_store_register(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size);

#define ADRENO_CORESIGHT_ATTR(_attrname, _reg) \
	struct adreno_coresight_attr coresight_attr_##_attrname  = { \
		__ATTR(_attrname, S_IRUGO | S_IWUSR, \
		adreno_coresight_show_register, \
		adreno_coresight_store_register), \
		(_reg), }

/**
 * struct adreno_coresight - GPU specific coresight definition
 * @registers - Array of GPU specific registers to configure trace bus output
 * @count - Number of registers in the array
 * @groups - Pointer to an attribute list of control files
 */
struct adreno_coresight {
	struct adreno_coresight_register *registers;
	unsigned int count;
	const struct attribute_group **groups;
};


struct adreno_irq_funcs {
	void (*func)(struct adreno_device *, int);
};
#define ADRENO_IRQ_CALLBACK(_c) { .func = _c }

struct adreno_irq {
	unsigned int mask;
	struct adreno_irq_funcs *funcs;
	int funcs_count;
};

/*
 * struct adreno_debugbus_block - Holds info about debug buses of a chip
 * @block_id: Bus identifier
 * @dwords: Number of dwords of data that this block holds
 */
struct adreno_debugbus_block {
	unsigned int block_id;
	unsigned int dwords;
};

/*
 * struct adreno_snapshot_section_sizes - Structure holding the size of
 * different sections dumped during device snapshot
 * @cp_state_deb_size: Debug data section size
 * @vpc_mem_size: VPC memory section size
 * @cp_meq_size: CP MEQ size
 * @shader_mem_size: Size of shader memory of 1 shader section
 * @cp_merciu_size: CP MERCIU size
 * @roq_size: ROQ size
 */
struct adreno_snapshot_sizes {
	int cp_state_deb;
	int vpc_mem;
	int cp_meq;
	int shader_mem;
	int cp_merciu;
	int roq;
};

/*
 * struct adreno_snapshot_data - Holds data used in snapshot
 * @sect_sizes: Has sections sizes
 */
struct adreno_snapshot_data {
	struct adreno_snapshot_sizes *sect_sizes;
};

struct adreno_gpudev {
	/*
	 * These registers are in a different location on different devices,
	 * so define them in the structure and use them as variables.
	 */
	const struct adreno_reg_offsets *reg_offsets;

	struct adreno_perfcounters *perfcounters;
	const struct adreno_invalid_countables
			*invalid_countables;
	struct adreno_snapshot_data *snapshot_data;

	struct adreno_coresight *coresight;

	struct adreno_irq *irq;
	int num_prio_levels;
	/* GPU specific function hooks */
	irqreturn_t (*irq_handler)(struct adreno_device *);
	void (*irq_control)(struct adreno_device *, int);
	unsigned int (*irq_pending)(struct adreno_device *);
	void (*irq_setup)(struct adreno_device *);
	void (*snapshot)(struct adreno_device *, struct kgsl_snapshot *);
	int (*rb_init)(struct adreno_device *, struct adreno_ringbuffer *);
	int (*perfcounter_init)(struct adreno_device *);
	void (*perfcounter_close)(struct adreno_device *);
	void (*perfcounter_save)(struct adreno_device *);
	void (*perfcounter_restore)(struct adreno_device *);
	void (*fault_detect_start)(struct adreno_device *);
	void (*fault_detect_stop)(struct adreno_device *);
	void (*start)(struct adreno_device *);
	void (*busy_cycles)(struct adreno_device *, struct adreno_busy_data *);
	int (*perfcounter_enable)(struct adreno_device *, unsigned int group,
		unsigned int counter, unsigned int countable);
	uint64_t (*perfcounter_read)(struct adreno_device *adreno_dev,
		unsigned int group, unsigned int counter);
	void (*perfcounter_write)(struct adreno_device *adreno_dev,
		unsigned int group, unsigned int counter);
	bool (*is_sptp_idle)(struct adreno_device *);
	void (*enable_pc)(struct adreno_device *);
	void (*regulator_enable)(struct adreno_device *);
	void (*regulator_disable)(struct adreno_device *);
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
#define  KGSL_FT_DEFAULT_POLICY (BIT(KGSL_FT_REPLAY) + \
	BIT(KGSL_FT_SKIPCMD) + BIT(KGSL_FT_THROTTLE))
#define KGSL_FT_POLICY_MASK (BIT(KGSL_FT_OFF) + \
	BIT(KGSL_FT_REPLAY) + BIT(KGSL_FT_SKIPIB) \
	+ BIT(KGSL_FT_SKIPFRAME) + BIT(KGSL_FT_DISABLE) + \
	BIT(KGSL_FT_TEMP_DISABLE) + BIT(KGSL_FT_THROTTLE) + \
	BIT(KGSL_FT_SKIPCMD))

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

#define FOR_EACH_RINGBUFFER(adreno_dev, rb, i)				\
	for (i = 0, rb = &(adreno_dev->ringbuffers[0]);			\
		i < adreno_dev->num_ringbuffers;			\
		i++, rb++)

extern struct adreno_gpudev adreno_a3xx_gpudev;
extern struct adreno_gpudev adreno_a4xx_gpudev;

/* A3XX register set defined in adreno_a3xx.c */
extern const unsigned int a3xx_registers[];
extern const unsigned int a3xx_registers_count;

extern const unsigned int a3xx_hlsq_registers[];
extern const unsigned int a3xx_hlsq_registers_count;

extern const unsigned int a330_registers[];
extern const unsigned int a330_registers_count;

/* A4XX register set defined in adreno_a4xx.c */
extern const unsigned int a4xx_registers[];
extern const unsigned int a4xx_registers_count;

extern const unsigned int a4xx_sp_tp_registers[];
extern const unsigned int a4xx_sp_tp_registers_count;

extern const unsigned int a4xx_xpu_registers[];
extern const unsigned int a4xx_xpu_reg_cnt;

extern const struct adreno_vbif_snapshot_registers
				a4xx_vbif_snapshot_registers[];
extern const unsigned int a4xx_vbif_snapshot_reg_cnt;

extern unsigned int ft_detect_regs[];

int adreno_spin_idle(struct kgsl_device *device);
int adreno_idle(struct kgsl_device *device);
bool adreno_isidle(struct kgsl_device *device);

int adreno_perfcounter_query_group(struct adreno_device *adreno_dev,
	unsigned int groupid, unsigned int __user *countables,
	unsigned int count, unsigned int *max_counters);

int adreno_perfcounter_read_group(struct adreno_device *adreno_dev,
	struct kgsl_perfcounter_read_group __user *reads, unsigned int count);

int adreno_set_constraint(struct kgsl_device *device,
				struct kgsl_context *context,
				struct kgsl_device_constraint *constraint);

void adreno_shadermem_regread(struct kgsl_device *device,
						unsigned int offsetwords,
						unsigned int *value);

unsigned int adreno_a3xx_rbbm_clock_ctl_default(struct adreno_device
							*adreno_dev);

void adreno_snapshot(struct kgsl_device *device,
		struct kgsl_snapshot *snapshot,
		struct kgsl_context *context);

void adreno_dispatcher_start(struct kgsl_device *device);
int adreno_dispatcher_init(struct adreno_device *adreno_dev);
void adreno_dispatcher_close(struct adreno_device *adreno_dev);
int adreno_dispatcher_idle(struct adreno_device *adreno_dev);
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

int adreno_a3xx_pwron_fixup_init(struct adreno_device *adreno_dev);

int adreno_coresight_init(struct adreno_device *adreno_dev);

void adreno_coresight_start(struct adreno_device *adreno_dev);
void adreno_coresight_stop(struct adreno_device *adreno_dev);

void adreno_coresight_remove(struct adreno_device *adreno_dev);

bool adreno_hw_isidle(struct adreno_device *adreno_dev);

int adreno_rb_readtimestamp(struct kgsl_device *device,
	void *priv, enum kgsl_timestamp_type type,
	unsigned int *timestamp);

int adreno_iommu_set_pt(struct adreno_ringbuffer *rb,
			struct kgsl_pagetable *new_pt);

static inline int adreno_is_a3xx(struct adreno_device *adreno_dev)
{
	return ((ADRENO_GPUREV(adreno_dev) >= 300) &&
		(ADRENO_GPUREV(adreno_dev) < 400));
}

static inline int adreno_is_a305(struct adreno_device *adreno_dev)
{
	return (ADRENO_GPUREV(adreno_dev) == ADRENO_REV_A305);
}

static inline int adreno_is_a305b(struct adreno_device *adreno_dev)
{
	return (ADRENO_GPUREV(adreno_dev) == ADRENO_REV_A305B);
}

static inline int adreno_is_a305c(struct adreno_device *adreno_dev)
{
	return (ADRENO_GPUREV(adreno_dev) == ADRENO_REV_A305C);
}

static inline int adreno_is_a306(struct adreno_device *adreno_dev)
{
	return (ADRENO_GPUREV(adreno_dev) == ADRENO_REV_A306);
}

static inline int adreno_is_a310(struct adreno_device *adreno_dev)
{
	return (ADRENO_GPUREV(adreno_dev) == ADRENO_REV_A310);
}

static inline int adreno_is_a320(struct adreno_device *adreno_dev)
{
	return (ADRENO_GPUREV(adreno_dev) == ADRENO_REV_A320);
}

static inline int adreno_is_a330(struct adreno_device *adreno_dev)
{
	return (ADRENO_GPUREV(adreno_dev) == ADRENO_REV_A330);
}

static inline int adreno_is_a330v2(struct adreno_device *adreno_dev)
{
	return ((ADRENO_GPUREV(adreno_dev) == ADRENO_REV_A330) &&
		(ADRENO_CHIPID_PATCH(adreno_dev->chipid) > 0));
}

static inline int adreno_is_a330v21(struct adreno_device *adreno_dev)
{
	return ((ADRENO_GPUREV(adreno_dev) == ADRENO_REV_A330) &&
		(ADRENO_CHIPID_PATCH(adreno_dev->chipid) > 0xF));
}

static inline int adreno_is_a4xx(struct adreno_device *adreno_dev)
{
	return (ADRENO_GPUREV(adreno_dev) >= 400);
}

static inline int adreno_is_a405(struct adreno_device *adreno_dev)
{
	return (ADRENO_GPUREV(adreno_dev) == ADRENO_REV_A405);
}

static inline int adreno_is_a420(struct adreno_device *adreno_dev)
{
	return (ADRENO_GPUREV(adreno_dev) == ADRENO_REV_A420);
}

static inline int adreno_is_a430(struct adreno_device *adreno_dev)
{
	return (ADRENO_GPUREV(adreno_dev) == ADRENO_REV_A430);
}

static inline int adreno_rb_ctxtswitch(unsigned int *cmd)
{
	return (cmd[0] == cp_nop_packet(1) &&
		cmd[1] == KGSL_CONTEXT_TO_MEM_IDENTIFIER);
}

/**
 * adreno_context_timestamp() - Return the last queued timestamp for the context
 * @k_ctxt: Pointer to the KGSL context to query
 *
 * Return the last queued context for the given context. This is used to verify
 * that incoming requests are not using an invalid (unsubmitted) timestamp
 */
static inline int adreno_context_timestamp(struct kgsl_context *k_ctxt)
{
	struct adreno_context *drawctxt = ADRENO_CONTEXT(k_ctxt);
	return drawctxt->timestamp;
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

static inline int adreno_add_bank_change_cmds(unsigned int *cmds,
					int cur_ctx_bank,
					unsigned int nop_gpuaddr)
{
	unsigned int *start = cmds;

	*cmds++ = cp_type0_packet(A3XX_CP_STATE_DEBUG_INDEX, 1);
	*cmds++ = (cur_ctx_bank ? 0 : 0x20);
	cmds += __adreno_add_idle_indirect_cmds(cmds, nop_gpuaddr);
	return cmds - start;
}

/*
 * adreno_read_cmds - Add pm4 packets to perform read
 * @cmds - Pointer to memory where read commands need to be added
 * @addr - gpu address of the read
 * @val - The GPU will wait until the data at address addr becomes
 * @nop_gpuaddr - NOP GPU address
 * equal to value
 */
static inline int adreno_add_read_cmds(unsigned int *cmds, unsigned int addr,
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
 * adreno_wait_reg_mem() - Add a CP_WAIT_REG_MEM command
 * @cmds:	Pointer to memory where commands are to be added
 * @addr:	Regiater address to poll for
 * @val:	Value to poll for
 * @mask:	The value against which register value is masked
 * @interval:	wait interval
 */
static inline int adreno_wait_reg_mem(unsigned int *cmds, unsigned int addr,
				unsigned int val, unsigned int mask,
				unsigned int interval)
{
	unsigned int *start = cmds;
	*cmds++ = cp_type3_packet(CP_WAIT_REG_MEM, 5);
	*cmds++ = 0x3; /* Function = Equals */
	*cmds++ = addr; /* Poll address */
	*cmds++ = val; /* ref val */
	*cmds++ = mask;
	*cmds++ = interval;

	/* WAIT_REG_MEM turns back on protected mode - push it off */
	*cmds++ = cp_type3_packet(CP_SET_PROTECTED_MODE, 1);
	*cmds++ = 0;

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
	struct adreno_gpudev *gpudev = ADRENO_GPU_DEVICE(adreno_dev);

	if (offset_name >= ADRENO_REG_REGISTER_MAX ||
		ADRENO_REG_UNUSED == gpudev->reg_offsets->offsets[offset_name])
			BUG();

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
	struct adreno_gpudev *gpudev = ADRENO_GPU_DEVICE(adreno_dev);
	if (adreno_checkreg_off(adreno_dev, offset_name))
		kgsl_regread(&adreno_dev->dev,
				gpudev->reg_offsets->offsets[offset_name], val);
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
	struct adreno_gpudev *gpudev = ADRENO_GPU_DEVICE(adreno_dev);
	if (adreno_checkreg_off(adreno_dev, offset_name))
		kgsl_regwrite(&adreno_dev->dev,
				gpudev->reg_offsets->offsets[offset_name], val);
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
	struct adreno_gpudev *gpudev = ADRENO_GPU_DEVICE(adreno_dev);
	if (!adreno_checkreg_off(adreno_dev, offset_name))
		return ADRENO_REG_REGISTER_MAX;
	return gpudev->reg_offsets->offsets[offset_name];
}

/**
 * adreno_gpu_fault() - Return the current state of the GPU
 * @adreno_dev: A pointer to the adreno_device to query
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

/**
 * adreno_gpu_halt() - Return the GPU halt refcount
 * @adreno_dev: A pointer to the adreno_device
 */
static inline int adreno_gpu_halt(struct adreno_device *adreno_dev)
{
	smp_rmb();
	return atomic_read(&adreno_dev->halt);
}


/**
 * adreno_clear_gpu_halt() - Clear the GPU halt refcount
 * @adreno_dev: A pointer to the adreno_device
 */
static inline void adreno_clear_gpu_halt(struct adreno_device *adreno_dev)
{
	atomic_set(&adreno_dev->halt, 0);
	smp_wmb();
}

/**
 * adreno_get_gpu_halt() - Increment GPU halt refcount
 * @adreno_dev: A pointer to the adreno_device
 */
static inline void adreno_get_gpu_halt(struct adreno_device *adreno_dev)
{
	atomic_inc(&adreno_dev->halt);
}

/**
 * adreno_put_gpu_halt() - Decrement GPU halt refcount
 * @adreno_dev: A pointer to the adreno_device
 */
static inline void adreno_put_gpu_halt(struct adreno_device *adreno_dev)
{
	if (atomic_dec_return(&adreno_dev->halt) < 0)
		BUG();
}


/*
 * adreno_vbif_start() - Program VBIF registers, called in device start
 * @adreno_dev: Pointer to device whose vbif data is to be programmed
 * @vbif_platforms: list register value pair of vbif for a family
 * of adreno cores
 * @num_platforms: Number of platforms contained in vbif_platforms
 */
static inline void adreno_vbif_start(struct adreno_device *adreno_dev,
			const struct adreno_vbif_platform *vbif_platforms,
			int num_platforms)
{
	int i;
	const struct adreno_vbif_data *vbif = NULL;

	for (i = 0; i < num_platforms; i++) {
		if (vbif_platforms[i].devfunc(adreno_dev)) {
			vbif = vbif_platforms[i].vbif;
			break;
		}
	}
	BUG_ON(vbif == NULL);
	while (vbif->reg != 0) {
		kgsl_regwrite(&adreno_dev->dev, vbif->reg, vbif->val);
		vbif++;
	}
}

/**
 * adreno_set_protected_registers() - Protect the specified range of registers
 * from being accessed by the GPU
 * @adreno_dev: pointer to the Adreno device
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
static inline void adreno_set_protected_registers(
		struct adreno_device *adreno_dev, unsigned int *index,
		unsigned int reg, int mask_len)
{
	unsigned int val;

	/* A430 has 24 registers (yay!).  Everything else has 16 (boo!) */

	if (adreno_is_a430(adreno_dev))
		BUG_ON(*index >= 24);
	else
		BUG_ON(*index >= 16);

	val = 0x60000000 | ((mask_len & 0x1F) << 24) | ((reg << 2) & 0xFFFFF);

	/*
	 * Write the protection range to the next available protection
	 * register
	 */

	if (adreno_is_a4xx(adreno_dev))
		kgsl_regwrite(&adreno_dev->dev,
				A4XX_CP_PROTECT_REG_0 + *index, val);
	else if (adreno_is_a3xx(adreno_dev))
		kgsl_regwrite(&adreno_dev->dev,
				A3XX_CP_PROTECT_REG_0 + *index, val);
	*index = *index + 1;
}

#ifdef CONFIG_DEBUG_FS
void adreno_debugfs_init(struct adreno_device *adreno_dev);
void adreno_context_debugfs_init(struct adreno_device *,
				struct adreno_context *);
#else
static inline void adreno_debugfs_init(struct adreno_device *adreno_dev) { }
static inline void adreno_context_debugfs_init(struct adreno_device *device,
						struct adreno_context *context)
						{ }
#endif

/**
 * adreno_compare_pm4_version() - Compare the PM4 microcode version
 * @adreno_dev: Pointer to the adreno_device struct
 * @version: Version number to compare again
 *
 * Compare the current version against the specified version and return -1 if
 * the current code is older, 0 if equal or 1 if newer.
 */
static inline int adreno_compare_pm4_version(struct adreno_device *adreno_dev,
	unsigned int version)
{
	if (adreno_dev->pm4_fw_version == version)
		return 0;

	return (adreno_dev->pm4_fw_version > version) ? 1 : -1;
}

/**
 * adreno_compare_pfp_version() - Compare the PFP microcode version
 * @adreno_dev: Pointer to the adreno_device struct
 * @version: Version number to compare against
 *
 * Compare the current version against the specified version and return -1 if
 * the current code is older, 0 if equal or 1 if newer.
 */
static inline int adreno_compare_pfp_version(struct adreno_device *adreno_dev,
	unsigned int version)
{
	if (adreno_dev->pfp_fw_version == version)
		return 0;

	return (adreno_dev->pfp_fw_version > version) ? 1 : -1;
}

/*
 * adreno_bootstrap_ucode() - Checks if Ucode bootstrapping is supported
 * @adreno_dev:		Pointer to the the adreno device
 */
static inline int adreno_bootstrap_ucode(struct adreno_device *adreno_dev)
{
	return (ADRENO_FEATURE(adreno_dev, ADRENO_USE_BOOTSTRAP) &&
		adreno_compare_pfp_version(adreno_dev,
			adreno_dev->gpucore->pfp_bstrp_ver) >= 0) ? 1 : 0;
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
	if (adreno_dev->cur_rb == rb) {
		adreno_readreg(adreno_dev, ADRENO_REG_CP_RB_RPTR, &(rb->rptr));
		rmb();
	}
	return rb->rptr;
}

/**
 * adreno_ctx_get_rb() - Return the ringbuffer that a context should
 * use based on priority
 * @adreno_dev: The adreno device that context is using
 * @drawctxt: The context pointer
 */
static inline struct adreno_ringbuffer *adreno_ctx_get_rb(
				struct adreno_device *adreno_dev,
				struct adreno_context *drawctxt)
{
	struct kgsl_context *context;
	int level;
	if (!drawctxt)
		return NULL;

	context = &(drawctxt->base);

	/*
	 * Math to convert the priority field in context structure to an RB ID.
	 * Divide up the context priority based on number of ringbuffer levels.
	 */
	level = context->priority / adreno_dev->num_ringbuffers;
	if (level < adreno_dev->num_ringbuffers)
		return &(adreno_dev->ringbuffers[level]);
	else
		return &(adreno_dev->ringbuffers[
				adreno_dev->num_ringbuffers - 1]);
}
/*
 * adreno_set_active_ctx_null() - Put back reference to any active context
 * and set the active context to NULL
 * @adreno_dev: The adreno device
 */
static inline void adreno_set_active_ctx_null(struct adreno_device *adreno_dev)
{
	int i;
	struct adreno_ringbuffer *rb;
	FOR_EACH_RINGBUFFER(adreno_dev, rb, i) {
		if (rb->drawctxt_active)
			kgsl_context_put(&(rb->drawctxt_active->base));
		rb->drawctxt_active = NULL;
	}
}

/**
 * adreno_use_cpu_path() - Use CPU instead of the GPU to manage the mmu?
 * @adreno_dev: the device
 *
 * In many cases it is preferable to poke the iommu directly rather
 * than using the GPU command stream. If we are idle or trying to go to a low
 * power state, using the command stream will be slower and asynchronous, which
 * needlessly complicates the power state transitions. Additionally,
 * the hardware simulators do not support command stream MMU operations so
 * the command stream can never be used if we are capturing CFF data.
 *
 */
static inline bool adreno_use_cpu_path(struct adreno_device *adreno_dev)
{
	return (adreno_isidle(&adreno_dev->dev) ||
		KGSL_STATE_ACTIVE != adreno_dev->dev.state ||
		atomic_read(&adreno_dev->dev.active_cnt) == 0 ||
		adreno_dev->dev.cff_dump_enable);
}

#endif /*__ADRENO_H */
