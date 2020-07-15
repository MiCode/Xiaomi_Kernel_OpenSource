/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2008-2020, The Linux Foundation. All rights reserved.
 */
#ifndef __ADRENO_H
#define __ADRENO_H

#include "adreno_dispatch.h"
#include "adreno_drawctxt.h"
#include "adreno_perfcounter.h"
#include "adreno_profile.h"
#include "adreno_ringbuffer.h"
#include "kgsl_sharedmem.h"

#define DEVICE_3D_NAME "kgsl-3d"
#define DEVICE_3D0_NAME "kgsl-3d0"

/* ADRENO_DEVICE - Given a kgsl_device return the adreno device struct */
#define ADRENO_DEVICE(device) \
		container_of(device, struct adreno_device, dev)

/* KGSL_DEVICE - given an adreno_device, return the KGSL device struct */
#define KGSL_DEVICE(_dev) (&((_dev)->dev))

/* ADRENO_CONTEXT - Given a context return the adreno context struct */
#define ADRENO_CONTEXT(context) \
		container_of(context, struct adreno_context, base)

/* ADRENO_GPU_DEVICE - Given an adreno device return the GPU specific struct */
#define ADRENO_GPU_DEVICE(_a) ((_a)->gpucore->gpudev)

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

/**
 * ADRENO_QUIRK - return true if the specified quirk is required by the GPU
 */
#define ADRENO_QUIRK(_dev, _bit) \
	((_dev)->quirks & (_bit))

/*
 * ADRENO_PREEMPT_STYLE - return preemption style
 */
#define ADRENO_PREEMPT_STYLE(flags) \
	((flags & KGSL_CONTEXT_PREEMPT_STYLE_MASK) >> \
		  KGSL_CONTEXT_PREEMPT_STYLE_SHIFT)

/*
 * return the dispatcher drawqueue in which the given drawobj should
 * be submitted
 */
#define ADRENO_DRAWOBJ_DISPATCH_DRAWQUEUE(c)	\
	(&((ADRENO_CONTEXT(c->context))->rb->dispatch_q))

#define ADRENO_DRAWOBJ_RB(c)			\
	((ADRENO_CONTEXT(c->context))->rb)

#define ADRENO_FW(a, f)		(&(a->fw[f]))

/* Adreno core features */
/* The core supports SP/TP hw controlled power collapse */
#define ADRENO_SPTP_PC BIT(3)
/* The GPU supports content protection */
#define ADRENO_CONTENT_PROTECTION BIT(5)
/* The GPU supports preemption */
#define ADRENO_PREEMPTION BIT(6)
/* The core uses GPMU for power and limit management */
#define ADRENO_GPMU BIT(7)
/* The GPMU supports Limits Management */
#define ADRENO_LM BIT(8)
/* The core uses 64 bit GPU addresses */
#define ADRENO_64BIT BIT(9)
/* The GPU supports retention for cpz registers */
#define ADRENO_CPZ_RETENTION BIT(10)
/* The core has soft fault detection available */
#define ADRENO_SOFT_FAULT_DETECT BIT(11)
/* The GMU supports RPMh for power management*/
#define ADRENO_RPMH BIT(12)
/* The GMU supports IFPC power management*/
#define ADRENO_IFPC BIT(13)
/* The GMU supports HW based NAP */
#define ADRENO_HW_NAP BIT(14)
/* The GMU supports min voltage*/
#define ADRENO_MIN_VOLT BIT(15)
/* The core supports IO-coherent memory */
#define ADRENO_IOCOHERENT BIT(16)
/*
 * The GMU supports Adaptive Clock Distribution (ACD)
 * for droop mitigation
 */
#define ADRENO_ACD BIT(17)
/* ECP enabled GMU */
#define ADRENO_ECP BIT(18)
/* Cooperative reset enabled GMU */
#define ADRENO_COOP_RESET BIT(19)
/* Indicates that the specific target is no longer supported */
#define ADRENO_DEPRECATED BIT(20)
/* The target supports ringbuffer level APRIV */
#define ADRENO_APRIV BIT(21)
/*
 * Adreno GPU quirks - control bits for various workarounds
 */

/* Set TWOPASSUSEWFI in PC_DBG_ECO_CNTL (5XX/6XX) */
#define ADRENO_QUIRK_TWO_PASS_USE_WFI BIT(0)
/* Lock/unlock mutex to sync with the IOMMU */
#define ADRENO_QUIRK_IOMMU_SYNC BIT(1)
/* Submit critical packets at GPU wake up */
#define ADRENO_QUIRK_CRITICAL_PACKETS BIT(2)
/* Mask out RB1-3 activity signals from HW hang detection logic */
#define ADRENO_QUIRK_FAULT_DETECT_MASK BIT(3)
/* Disable RB sampler datapath clock gating optimization */
#define ADRENO_QUIRK_DISABLE_RB_DP2CLOCKGATING BIT(4)
/* Disable local memory(LM) feature to avoid corner case error */
#define ADRENO_QUIRK_DISABLE_LMLOADKILL BIT(5)
/* Allow HFI to use registers to send message to GMU */
#define ADRENO_QUIRK_HFI_USE_REG BIT(6)
/* Only set protected SECVID registers once */
#define ADRENO_QUIRK_SECVID_SET_ONCE BIT(7)
/*
 * Limit number of read and write transactions from
 * UCHE block to GBIF to avoid possible deadlock
 * between GBIF, SMMU and MEMNOC.
 */
#define ADRENO_QUIRK_LIMIT_UCHE_GBIF_RW BIT(8)
/* Select alternate secure context bank for mmu */
#define ADRENO_QUIRK_MMU_SECURE_CB_ALT BIT(9)
/* Do explicit mode control of cx gdsc */
#define ADRENO_QUIRK_CX_GDSC BIT(10)

/* Flags to control command packet settings */
#define KGSL_CMD_FLAGS_NONE             0
#define KGSL_CMD_FLAGS_PMODE		BIT(0)
#define KGSL_CMD_FLAGS_INTERNAL_ISSUE   BIT(1)
#define KGSL_CMD_FLAGS_WFI              BIT(2)
#define KGSL_CMD_FLAGS_PROFILE		BIT(3)
#define KGSL_CMD_FLAGS_PWRON_FIXUP      BIT(4)

/* Command identifiers */
#define CONTEXT_TO_MEM_IDENTIFIER	0x2EADBEEF
#define CMD_IDENTIFIER			0x2EEDFACE
#define CMD_INTERNAL_IDENTIFIER		0x2EEDD00D
#define START_IB_IDENTIFIER		0x2EADEABE
#define END_IB_IDENTIFIER		0x2ABEDEAD
#define START_PROFILE_IDENTIFIER	0x2DEFADE1
#define END_PROFILE_IDENTIFIER		0x2DEFADE2
#define PWRON_FIXUP_IDENTIFIER		0x2AFAFAFA

/* Number of times to try hard reset for pre-a6xx GPUs */
#define NUM_TIMES_RESET_RETRY 4

/* Number of times to poll the AHB fence in ISR */
#define FENCE_RETRY_MAX 100

/* One cannot wait forever for the core to idle, so set an upper limit to the
 * amount of time to wait for the core to go idle
 */
#define ADRENO_IDLE_TIMEOUT (20 * 1000)

#define ADRENO_FW_PFP 0
#define ADRENO_FW_SQE 0
#define ADRENO_FW_PM4 1

enum adreno_gpurev {
	ADRENO_REV_UNKNOWN = 0,
	ADRENO_REV_A304 = 304,
	ADRENO_REV_A305 = 305,
	ADRENO_REV_A305C = 306,
	ADRENO_REV_A306 = 307,
	ADRENO_REV_A306A = 308,
	ADRENO_REV_A310 = 310,
	ADRENO_REV_A320 = 320,
	ADRENO_REV_A330 = 330,
	ADRENO_REV_A305B = 335,
	ADRENO_REV_A405 = 405,
	ADRENO_REV_A418 = 418,
	ADRENO_REV_A420 = 420,
	ADRENO_REV_A430 = 430,
	ADRENO_REV_A505 = 505,
	ADRENO_REV_A506 = 506,
	ADRENO_REV_A508 = 508,
	ADRENO_REV_A510 = 510,
	ADRENO_REV_A512 = 512,
	ADRENO_REV_A530 = 530,
	ADRENO_REV_A540 = 540,
	ADRENO_REV_A610 = 610,
	ADRENO_REV_A612 = 612,
	ADRENO_REV_A615 = 615,
	ADRENO_REV_A616 = 616,
	ADRENO_REV_A618 = 618,
	ADRENO_REV_A619 = 619,
	ADRENO_REV_A620 = 620,
	ADRENO_REV_A630 = 630,
	ADRENO_REV_A640 = 640,
	ADRENO_REV_A650 = 650,
	ADRENO_REV_A680 = 680,
	ADRENO_REV_A702 = 702,
};

#define ADRENO_SOFT_FAULT BIT(0)
#define ADRENO_HARD_FAULT BIT(1)
#define ADRENO_TIMEOUT_FAULT BIT(2)
#define ADRENO_IOMMU_PAGE_FAULT BIT(3)
#define ADRENO_PREEMPT_FAULT BIT(4)
#define ADRENO_GMU_FAULT BIT(5)
#define ADRENO_CTX_DETATCH_TIMEOUT_FAULT BIT(6)
#define ADRENO_GMU_FAULT_SKIP_SNAPSHOT BIT(7)

#define ADRENO_SPTP_PC_CTRL 0
#define ADRENO_LM_CTRL      1
#define ADRENO_HWCG_CTRL    2
#define ADRENO_THROTTLING_CTRL 3
#define ADRENO_ACD_CTRL 4

/* VBIF,  GBIF halt request and ack mask */
#define GBIF_HALT_REQUEST       0x1E0
#define VBIF_RESET_ACK_MASK     0x00f0
#define VBIF_RESET_ACK_TIMEOUT  100

/* number of throttle counters for DCVS adjustment */
#define ADRENO_GPMU_THROTTLE_COUNTERS 4
/* base for throttle counters */
#define ADRENO_GPMU_THROTTLE_COUNTERS_BASE_REG 43

struct adreno_gpudev;

/* Time to allow preemption to complete (in ms) */
#define ADRENO_PREEMPT_TIMEOUT 10000

#define ADRENO_INT_BIT(a, _bit) (((a)->gpucore->gpudev->int_bits) ? \
		(adreno_get_int(a, _bit) < 0 ? 0 : \
		BIT(adreno_get_int(a, _bit))) : 0)

/**
 * enum adreno_preempt_states
 * ADRENO_PREEMPT_NONE: No preemption is scheduled
 * ADRENO_PREEMPT_START: The S/W has started
 * ADRENO_PREEMPT_TRIGGERED: A preeempt has been triggered in the HW
 * ADRENO_PREEMPT_FAULTED: The preempt timer has fired
 * ADRENO_PREEMPT_PENDING: The H/W has signaled preemption complete
 * ADRENO_PREEMPT_COMPLETE: Preemption could not be finished in the IRQ handler,
 * worker has been scheduled
 */
enum adreno_preempt_states {
	ADRENO_PREEMPT_NONE = 0,
	ADRENO_PREEMPT_START,
	ADRENO_PREEMPT_TRIGGERED,
	ADRENO_PREEMPT_FAULTED,
	ADRENO_PREEMPT_PENDING,
	ADRENO_PREEMPT_COMPLETE,
};

/**
 * struct adreno_preemption
 * @state: The current state of preemption
 * @scratch: Memory descriptor for the memory where the GPU writes the
 * current ctxt record address and preemption counters on switch
 * @timer: A timer to make sure preemption doesn't stall
 * @work: A work struct for the preemption worker (for 5XX)
 * preempt_level: The level of preemption (for 6XX)
 * skipsaverestore: To skip saverestore during L1 preemption (for 6XX)
 * usesgmem: enable GMEM save/restore across preemption (for 6XX)
 * count: Track the number of preemptions triggered
 */
struct adreno_preemption {
	atomic_t state;
	struct kgsl_memdesc scratch;
	struct timer_list timer;
	struct work_struct work;
	unsigned int preempt_level;
	bool skipsaverestore;
	bool usesgmem;
	unsigned int count;
};


struct adreno_busy_data {
	unsigned int gpu_busy;
	unsigned int bif_ram_cycles;
	unsigned int bif_ram_cycles_read_ch1;
	unsigned int bif_ram_cycles_write_ch0;
	unsigned int bif_ram_cycles_write_ch1;
	unsigned int bif_starved_ram;
	unsigned int bif_starved_ram_ch1;
	unsigned int num_ifpc;
	unsigned int throttle_cycles[ADRENO_GPMU_THROTTLE_COUNTERS];
};

/**
 * struct adreno_firmware - Struct holding fw details
 * @fwvirt: Buffer which holds the ucode
 * @size: Size of ucode buffer
 * @version: Version of ucode
 * @memdesc: Memory descriptor which holds ucode buffer info
 */
struct adreno_firmware {
	unsigned int *fwvirt;
	size_t size;
	unsigned int version;
	struct kgsl_memdesc memdesc;
};

/**
 * struct adreno_perfcounter_list_node - struct to store perfcounters
 * allocated by a process on a kgsl fd.
 * @groupid: groupid of the allocated perfcounter
 * @countable: countable assigned to the allocated perfcounter
 * @node: list node for perfcounter_list of a process
 */
struct adreno_perfcounter_list_node {
	unsigned int groupid;
	unsigned int countable;
	struct list_head node;
};

/**
 * struct adreno_device_private - Adreno private structure per fd
 * @dev_priv: the kgsl device private structure
 * @perfcounter_list: list of perfcounters used by the process
 */
struct adreno_device_private {
	struct kgsl_device_private dev_priv;
	struct list_head perfcounter_list;
};

/**
 * struct adreno_reglist - simple container for register offsets / values
 */
struct adreno_reglist {
	/** @offset: Offset of the register */
	u32 offset;
	/** @value: Default value of the register to write */
	u32 value;
};

/**
 * struct adreno_gpu_core - A specific GPU core definition
 * @gpurev: Unique GPU revision identifier
 * @core: Match for the core version of the GPU
 * @major: Match for the major version of the GPU
 * @minor: Match for the minor version of the GPU
 * @patchid: Match for the patch revision of the GPU
 * @features: Common adreno features supported by this core
 * @gpudev: Pointer to the GPU family specific functions for this core
 * @gmem_size: Amount of binning memory (GMEM/OCMEM) to reserve for the core
 * @busy_mask: mask to check if GPU is busy in RBBM_STATUS
 * @bus_width: Bytes transferred in 1 cycle
 */
struct adreno_gpu_core {
	enum adreno_gpurev gpurev;
	unsigned int core, major, minor, patchid;
	unsigned long features;
	struct adreno_gpudev *gpudev;
	size_t gmem_size;
	unsigned int busy_mask;
	u32 bus_width;
};

enum gpu_coresight_sources {
	GPU_CORESIGHT_GX = 0,
	GPU_CORESIGHT_CX = 1,
	GPU_CORESIGHT_MAX,
};

/**
 * struct adreno_device - The mothership structure for all adreno related info
 * @dev: Reference to struct kgsl_device
 * @priv: Holds the private flags specific to the adreno_device
 * @chipid: Chip ID specific to the GPU
 * @uche_gmem_base: Base address of GMEM for UCHE access
 * @cx_misc_len: Length of the CX MISC register block
 * @cx_misc_virt: Pointer where the CX MISC block is mapped
 * @rscc_base: Base physical address of the RSCC
 * @rscc_len: Length of the RSCC register block
 * @rscc_virt: Pointer where RSCC block is mapped
 * @isense_base: Base physical address of isense block
 * @isense_len: Length of the isense register block
 * @isense_virt: Pointer where isense block is mapped
 * @gpucore: Pointer to the adreno_gpu_core structure
 * @pfp_fw: Buffer which holds the pfp ucode
 * @pfp_fw_size: Size of pfp ucode buffer
 * @pfp_fw_version: Version of pfp ucode
 * @pfp: Memory descriptor which holds pfp ucode buffer info
 * @pm4_fw: Buffer which holds the pm4 ucode
 * @pm4_fw_size: Size of pm4 ucode buffer
 * @pm4_fw_version: Version of pm4 ucode
 * @pm4: Memory descriptor which holds pm4 ucode buffer info
 * @gpmu_cmds_size: Length of gpmu cmd stream
 * @gpmu_cmds: gpmu cmd stream
 * @ringbuffers: Array of pointers to adreno_ringbuffers
 * @num_ringbuffers: Number of ringbuffers for the GPU
 * @cur_rb: Pointer to the current ringbuffer
 * @next_rb: Ringbuffer we are switching to during preemption
 * @prev_rb: Ringbuffer we are switching from during preemption
 * @fast_hang_detect: Software fault detection availability
 * @ft_policy: Defines the fault tolerance policy
 * @long_ib_detect: Long IB detection availability
 * @ft_pf_policy: Defines the fault policy for page faults
 * @cooperative_reset: Indicates if graceful death handshake is enabled
 * between GMU and GPU
 * @profile: Container for adreno profiler information
 * @dispatcher: Container for adreno GPU dispatcher
 * @pwron_fixup: Command buffer to run a post-power collapse shader workaround
 * @pwron_fixup_dwords: Number of dwords in the command buffer
 * @input_work: Work struct for turning on the GPU after a touch event
 * @busy_data: Struct holding GPU VBIF busy stats
 * @ram_cycles_lo: Number of DDR clock cycles for the monitor session (Only
 * DDR channel 0 read cycles in case of GBIF)
 * @ram_cycles_lo_ch1_read: Number of DDR channel 1 Read clock cycles for
 * the monitor session
 * @ram_cycles_lo_ch0_write: Number of DDR channel 0 Write clock cycles for
 * the monitor session
 * @ram_cycles_lo_ch1_write: Number of DDR channel 0 Write clock cycles for
 * the monitor session
 * @starved_ram_lo: Number of cycles VBIF/GBIF is stalled by DDR (Only channel 0
 * stall cycles in case of GBIF)
 * @starved_ram_lo_ch1: Number of cycles GBIF is stalled by DDR channel 1
 * @perfctr_pwr_lo: GPU busy cycles
 * @perfctr_ifpc_lo: IFPC count
 * @halt: Atomic variable to check whether the GPU is currently halted
 * @pending_irq_refcnt: Atomic variable to keep track of running IRQ handlers
 * @ctx_d_debugfs: Context debugfs node
 * @pwrctrl_flag: Flag to hold adreno specific power attributes
 * @profile_buffer: Memdesc holding the drawobj profiling buffer
 * @profile_index: Index to store the start/stop ticks in the profiling
 * buffer
 * @pwrup_reglist: Memdesc holding the power up register list
 * which is used by CP during preemption and IFPC
 * @lm_fw: The LM firmware handle
 * @lm_sequence: Pointer to the start of the register write sequence for LM
 * @lm_size: The dword size of the LM sequence
 * @lm_limit: limiting value for LM
 * @lm_threshold_count: register value for counter for lm threshold breakin
 * @lm_threshold_cross: number of current peaks exceeding threshold
 * @lm_slope: Slope value in the fused register for LM
 * @ifpc_count: Number of times the GPU went into IFPC
 * @speed_bin: Indicate which power level set to use
 * @highest_bank_bit: Value of the highest bank bit
 * @csdev: Pointer to a coresight device (if applicable)
 * @gpmu_throttle_counters - counteers for number of throttled clocks
 * @irq_storm_work: Worker to handle possible interrupt storms
 * @active_list: List to track active contexts
 * @active_list_lock: Lock to protect active_list
 * @gpu_llc_slice: GPU system cache slice descriptor
 * @gpu_llc_slice_enable: To enable the GPU system cache slice or not
 * @gpuhtw_llc_slice: GPU pagetables system cache slice descriptor
 * @gpuhtw_llc_slice_enable: To enable the GPUHTW system cache slice or not
 * @zap_loaded: Used to track if zap was successfully loaded or not
 * @soc_hw_rev: Indicate which SOC hardware revision to use
 */
struct adreno_device {
	struct kgsl_device dev;    /* Must be first field in this struct */
	unsigned long priv;
	unsigned int chipid;
	u64 uche_gmem_base;
	unsigned long cx_dbgc_base;
	unsigned int cx_dbgc_len;
	void __iomem *cx_dbgc_virt;
	unsigned int cx_misc_len;
	void __iomem *cx_misc_virt;
	unsigned long rscc_base;
	unsigned int rscc_len;
	void __iomem *rscc_virt;
	unsigned long isense_base;
	unsigned int isense_len;
	void __iomem *isense_virt;
	const struct adreno_gpu_core *gpucore;
	struct adreno_firmware fw[2];
	size_t gpmu_cmds_size;
	unsigned int *gpmu_cmds;
	struct adreno_ringbuffer ringbuffers[KGSL_PRIORITY_MAX_RB_LEVELS];
	int num_ringbuffers;
	struct adreno_ringbuffer *cur_rb;
	struct adreno_ringbuffer *next_rb;
	struct adreno_ringbuffer *prev_rb;
	unsigned int fast_hang_detect;
	unsigned long ft_policy;
	unsigned int long_ib_detect;
	unsigned long ft_pf_policy;
	bool cooperative_reset;
	struct adreno_profile profile;
	struct adreno_dispatcher dispatcher;
	struct kgsl_memdesc pwron_fixup;
	unsigned int pwron_fixup_dwords;
	struct work_struct input_work;
	struct adreno_busy_data busy_data;
	unsigned int ram_cycles_lo;
	unsigned int ram_cycles_lo_ch1_read;
	unsigned int ram_cycles_lo_ch0_write;
	unsigned int ram_cycles_lo_ch1_write;
	unsigned int starved_ram_lo;
	unsigned int starved_ram_lo_ch1;
	unsigned int perfctr_pwr_lo;
	unsigned int perfctr_ifpc_lo;
	atomic_t halt;
	atomic_t pending_irq_refcnt;
	struct dentry *ctx_d_debugfs;
	unsigned long pwrctrl_flag;

	struct kgsl_memdesc profile_buffer;
	unsigned int profile_index;
	struct kgsl_memdesc pwrup_reglist;
	const struct firmware *lm_fw;
	uint32_t *lm_sequence;
	uint32_t lm_size;
	struct adreno_preemption preempt;
	struct work_struct gpmu_work;
	uint32_t lm_leakage;
	uint32_t lm_limit;
	uint32_t lm_threshold_count;
	uint32_t lm_threshold_cross;
	u32 lm_slope;
	uint32_t ifpc_count;

	unsigned int speed_bin;
	unsigned int highest_bank_bit;
	unsigned int quirks;

	struct coresight_device *csdev[GPU_CORESIGHT_MAX];
	uint32_t gpmu_throttle_counters[ADRENO_GPMU_THROTTLE_COUNTERS];
	struct work_struct irq_storm_work;

	struct list_head active_list;
	spinlock_t active_list_lock;

	void *gpu_llc_slice;
	bool gpu_llc_slice_enable;
	void *gpuhtw_llc_slice;
	bool gpuhtw_llc_slice_enable;
	unsigned int zap_loaded;
	unsigned int soc_hw_rev;
};

/**
 * enum adreno_device_flags - Private flags for the adreno_device
 * @ADRENO_DEVICE_PWRON - Set during init after a power collapse
 * @ADRENO_DEVICE_PWRON_FIXUP - Set if the target requires the shader fixup
 * after power collapse
 * @ADRENO_DEVICE_CORESIGHT - Set if the coresight (trace bus) registers should
 * be restored after power collapse
 * @ADRENO_DEVICE_HANG_INTR - Set if the hang interrupt should be enabled for
 * this target
 * @ADRENO_DEVICE_STARTED - Set if the device start sequence is in progress
 * @ADRENO_DEVICE_FAULT - Set if the device is currently in fault (and shouldn't
 * send any more commands to the ringbuffer)
 * @ADRENO_DEVICE_DRAWOBJ_PROFILE - Set if the device supports drawobj
 * profiling via the ALWAYSON counter
 * @ADRENO_DEVICE_PREEMPTION - Turn on/off preemption
 * @ADRENO_DEVICE_SOFT_FAULT_DETECT - Set if soft fault detect is enabled
 * @ADRENO_DEVICE_GPMU_INITIALIZED - Set if GPMU firmware initialization succeed
 * @ADRENO_DEVICE_ISDB_ENABLED - Set if the Integrated Shader DeBugger is
 * attached and enabled
 * @ADRENO_DEVICE_CACHE_FLUSH_TS_SUSPENDED - Set if a CACHE_FLUSH_TS irq storm
 * is in progress
 */
enum adreno_device_flags {
	ADRENO_DEVICE_PWRON = 0,
	ADRENO_DEVICE_PWRON_FIXUP = 1,
	ADRENO_DEVICE_INITIALIZED = 2,
	ADRENO_DEVICE_CORESIGHT = 3,
	ADRENO_DEVICE_HANG_INTR = 4,
	ADRENO_DEVICE_STARTED = 5,
	ADRENO_DEVICE_FAULT = 6,
	ADRENO_DEVICE_DRAWOBJ_PROFILE = 7,
	ADRENO_DEVICE_GPU_REGULATOR_ENABLED = 8,
	ADRENO_DEVICE_PREEMPTION = 9,
	ADRENO_DEVICE_SOFT_FAULT_DETECT = 10,
	ADRENO_DEVICE_GPMU_INITIALIZED = 11,
	ADRENO_DEVICE_ISDB_ENABLED = 12,
	ADRENO_DEVICE_CACHE_FLUSH_TS_SUSPENDED = 13,
	ADRENO_DEVICE_CORESIGHT_CX = 14,
};

/**
 * struct adreno_drawobj_profile_entry - a single drawobj entry in the
 * kernel profiling buffer
 * @started: Number of GPU ticks at start of the drawobj
 * @retired: Number of GPU ticks at the end of the drawobj
 */
struct adreno_drawobj_profile_entry {
	uint64_t started;
	uint64_t retired;
};

#define ADRENO_DRAWOBJ_PROFILE_COUNT \
	(PAGE_SIZE / sizeof(struct adreno_drawobj_profile_entry))

#define ADRENO_DRAWOBJ_PROFILE_OFFSET(_index, _member) \
	 ((_index) * sizeof(struct adreno_drawobj_profile_entry) \
	  + offsetof(struct adreno_drawobj_profile_entry, _member))


/**
 * adreno_regs: List of registers that are used in kgsl driver for all
 * 3D devices. Each device type has different offset value for the same
 * register, so an array of register offsets are declared for every device
 * and are indexed by the enumeration values defined in this enum
 */
enum adreno_regs {
	ADRENO_REG_CP_ME_RAM_WADDR,
	ADRENO_REG_CP_ME_RAM_DATA,
	ADRENO_REG_CP_PFP_UCODE_DATA,
	ADRENO_REG_CP_PFP_UCODE_ADDR,
	ADRENO_REG_CP_RB_BASE,
	ADRENO_REG_CP_RB_BASE_HI,
	ADRENO_REG_CP_RB_RPTR_ADDR_LO,
	ADRENO_REG_CP_RB_RPTR_ADDR_HI,
	ADRENO_REG_CP_RB_RPTR,
	ADRENO_REG_CP_RB_WPTR,
	ADRENO_REG_CP_CNTL,
	ADRENO_REG_CP_ME_CNTL,
	ADRENO_REG_CP_RB_CNTL,
	ADRENO_REG_CP_IB1_BASE,
	ADRENO_REG_CP_IB1_BASE_HI,
	ADRENO_REG_CP_IB1_BUFSZ,
	ADRENO_REG_CP_IB2_BASE,
	ADRENO_REG_CP_IB2_BASE_HI,
	ADRENO_REG_CP_IB2_BUFSZ,
	ADRENO_REG_CP_TIMESTAMP,
	ADRENO_REG_CP_SCRATCH_REG6,
	ADRENO_REG_CP_SCRATCH_REG7,
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
	ADRENO_REG_CP_PREEMPT,
	ADRENO_REG_CP_PREEMPT_DEBUG,
	ADRENO_REG_CP_PREEMPT_DISABLE,
	ADRENO_REG_CP_PROTECT_REG_0,
	ADRENO_REG_CP_CONTEXT_SWITCH_SMMU_INFO_LO,
	ADRENO_REG_CP_CONTEXT_SWITCH_SMMU_INFO_HI,
	ADRENO_REG_CP_CONTEXT_SWITCH_PRIV_NON_SECURE_RESTORE_ADDR_LO,
	ADRENO_REG_CP_CONTEXT_SWITCH_PRIV_NON_SECURE_RESTORE_ADDR_HI,
	ADRENO_REG_CP_CONTEXT_SWITCH_PRIV_SECURE_RESTORE_ADDR_LO,
	ADRENO_REG_CP_CONTEXT_SWITCH_PRIV_SECURE_RESTORE_ADDR_HI,
	ADRENO_REG_CP_CONTEXT_SWITCH_NON_PRIV_RESTORE_ADDR_LO,
	ADRENO_REG_CP_CONTEXT_SWITCH_NON_PRIV_RESTORE_ADDR_HI,
	ADRENO_REG_CP_PREEMPT_LEVEL_STATUS,
	ADRENO_REG_RBBM_STATUS,
	ADRENO_REG_RBBM_STATUS3,
	ADRENO_REG_RBBM_PERFCTR_CTL,
	ADRENO_REG_RBBM_PERFCTR_LOAD_CMD0,
	ADRENO_REG_RBBM_PERFCTR_LOAD_CMD1,
	ADRENO_REG_RBBM_PERFCTR_LOAD_CMD2,
	ADRENO_REG_RBBM_PERFCTR_LOAD_CMD3,
	ADRENO_REG_RBBM_PERFCTR_PWR_1_LO,
	ADRENO_REG_RBBM_INT_0_MASK,
	ADRENO_REG_RBBM_INT_0_STATUS,
	ADRENO_REG_RBBM_PM_OVERRIDE2,
	ADRENO_REG_RBBM_INT_CLEAR_CMD,
	ADRENO_REG_RBBM_SW_RESET_CMD,
	ADRENO_REG_RBBM_BLOCK_SW_RESET_CMD,
	ADRENO_REG_RBBM_BLOCK_SW_RESET_CMD2,
	ADRENO_REG_RBBM_CLOCK_CTL,
	ADRENO_REG_VPC_DEBUG_RAM_SEL,
	ADRENO_REG_VPC_DEBUG_RAM_READ,
	ADRENO_REG_PA_SC_AA_CONFIG,
	ADRENO_REG_SQ_GPR_MANAGEMENT,
	ADRENO_REG_SQ_INST_STORE_MANAGEMENT,
	ADRENO_REG_TP0_CHICKEN,
	ADRENO_REG_RBBM_RBBM_CTL,
	ADRENO_REG_UCHE_INVALIDATE0,
	ADRENO_REG_UCHE_INVALIDATE1,
	ADRENO_REG_RBBM_PERFCTR_RBBM_0_LO,
	ADRENO_REG_RBBM_PERFCTR_RBBM_0_HI,
	ADRENO_REG_RBBM_PERFCTR_LOAD_VALUE_LO,
	ADRENO_REG_RBBM_PERFCTR_LOAD_VALUE_HI,
	ADRENO_REG_RBBM_SECVID_TRUST_CONTROL,
	ADRENO_REG_RBBM_ALWAYSON_COUNTER_LO,
	ADRENO_REG_RBBM_ALWAYSON_COUNTER_HI,
	ADRENO_REG_RBBM_SECVID_TRUST_CONFIG,
	ADRENO_REG_RBBM_SECVID_TSB_CONTROL,
	ADRENO_REG_RBBM_SECVID_TSB_TRUSTED_BASE,
	ADRENO_REG_RBBM_SECVID_TSB_TRUSTED_BASE_HI,
	ADRENO_REG_RBBM_SECVID_TSB_TRUSTED_SIZE,
	ADRENO_REG_RBBM_GPR0_CNTL,
	ADRENO_REG_RBBM_GBIF_HALT,
	ADRENO_REG_RBBM_GBIF_HALT_ACK,
	ADRENO_REG_RBBM_VBIF_GX_RESET_STATUS,
	ADRENO_REG_VBIF_XIN_HALT_CTRL0,
	ADRENO_REG_VBIF_XIN_HALT_CTRL1,
	ADRENO_REG_VBIF_VERSION,
	ADRENO_REG_GBIF_HALT,
	ADRENO_REG_GBIF_HALT_ACK,
	ADRENO_REG_GMU_AO_AHB_FENCE_CTRL,
	ADRENO_REG_GMU_AO_INTERRUPT_EN,
	ADRENO_REG_GMU_AO_HOST_INTERRUPT_CLR,
	ADRENO_REG_GMU_AO_HOST_INTERRUPT_STATUS,
	ADRENO_REG_GMU_AO_HOST_INTERRUPT_MASK,
	ADRENO_REG_GMU_PWR_COL_KEEPALIVE,
	ADRENO_REG_GMU_AHB_FENCE_STATUS,
	ADRENO_REG_GMU_RPMH_POWER_STATE,
	ADRENO_REG_GMU_HFI_CTRL_STATUS,
	ADRENO_REG_GMU_HFI_VERSION_INFO,
	ADRENO_REG_GMU_HFI_SFR_ADDR,
	ADRENO_REG_GMU_GMU2HOST_INTR_CLR,
	ADRENO_REG_GMU_GMU2HOST_INTR_INFO,
	ADRENO_REG_GMU_GMU2HOST_INTR_MASK,
	ADRENO_REG_GMU_HOST2GMU_INTR_SET,
	ADRENO_REG_GMU_HOST2GMU_INTR_CLR,
	ADRENO_REG_GMU_HOST2GMU_INTR_RAW_INFO,
	ADRENO_REG_GMU_NMI_CONTROL_STATUS,
	ADRENO_REG_GMU_CM3_CFG,
	ADRENO_REG_GMU_RBBM_INT_UNMASKED_STATUS,
	ADRENO_REG_GPMU_POWER_COUNTER_ENABLE,
	ADRENO_REG_REGISTER_MAX,
};

enum adreno_int_bits {
	ADRENO_INT_RBBM_AHB_ERROR,
	ADRENO_INT_BITS_MAX,
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
#define ADRENO_REG_SKIP	0xFFFFFFFE
#define ADRENO_REG_DEFINE(_offset, _reg)[_offset] = _reg
#define ADRENO_INT_DEFINE(_offset, _val) ADRENO_REG_DEFINE(_offset, _val)

/*
 * struct adreno_vbif_snapshot_registers - Holds an array of vbif registers
 * listed for snapshot dump for a particular core
 * @version: vbif version
 * @mask: vbif revision mask
 * @registers: vbif registers listed for snapshot dump
 * @count: count of vbif registers listed for snapshot
 */
struct adreno_vbif_snapshot_registers {
	const unsigned int version;
	const unsigned int mask;
	const unsigned int *registers;
	const int count;
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
		__ATTR(_attrname, 0644, \
		adreno_coresight_show_register, \
		adreno_coresight_store_register), \
		(_reg), }

/**
 * struct adreno_coresight - GPU specific coresight definition
 * @registers - Array of GPU specific registers to configure trace bus output
 * @count - Number of registers in the array
 * @groups - Pointer to an attribute list of control files
 * @atid - The unique ATID value of the coresight device
 */
struct adreno_coresight {
	struct adreno_coresight_register *registers;
	unsigned int count;
	const struct attribute_group **groups;
	unsigned int atid;
};


struct adreno_irq_funcs {
	void (*func)(struct adreno_device *adreno_dev, int mask);
};
#define ADRENO_IRQ_CALLBACK(_c) { .func = _c }

struct adreno_irq {
	unsigned int mask;
	struct adreno_irq_funcs *funcs;
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
 * @cp_pfp: CP PFP data section size
 * @cp_me: CP ME data section size
 * @vpc_mem: VPC memory section size
 * @cp_meq: CP MEQ size
 * @shader_mem: Size of shader memory of 1 shader section
 * @cp_merciu: CP MERCIU size
 * @roq: ROQ size
 */
struct adreno_snapshot_sizes {
	int cp_pfp;
	int cp_me;
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

enum adreno_cp_marker_type {
	IFPC_DISABLE,
	IFPC_ENABLE,
	IB1LIST_START,
	IB1LIST_END,
};

struct adreno_gpudev {
	/*
	 * These registers are in a different location on different devices,
	 * so define them in the structure and use them as variables.
	 */
	const struct adreno_reg_offsets *reg_offsets;
	unsigned int *const int_bits;
	const struct adreno_ft_perf_counters *ft_perf_counters;
	unsigned int ft_perf_counters_count;

	struct adreno_perfcounters *perfcounters;
	const struct adreno_invalid_countables *invalid_countables;
	struct adreno_snapshot_data *snapshot_data;

	struct adreno_coresight *coresight[GPU_CORESIGHT_MAX];

	struct adreno_irq *irq;
	int num_prio_levels;
	int cp_rb_cntl;
	unsigned int vbif_xin_halt_ctrl0_mask;
	unsigned int gbif_client_halt_mask;
	unsigned int gbif_arb_halt_mask;
	unsigned int gbif_gx_halt_mask;
	/* GPU specific function hooks */
	void (*irq_trace)(struct adreno_device *adreno_dev,
				unsigned int status);
	void (*snapshot)(struct adreno_device *adreno_dev,
				struct kgsl_snapshot *snapshot);
	void (*platform_setup)(struct adreno_device *adreno_dev);
	void (*init)(struct adreno_device *adreno_dev);
	void (*remove)(struct adreno_device *adreno_dev);
	int (*rb_start)(struct adreno_device *adreno_dev);
	int (*microcode_read)(struct adreno_device *adreno_dev);
	void (*perfcounter_init)(struct adreno_device *adreno_dev);
	void (*start)(struct adreno_device *adreno_dev);
	bool (*is_sptp_idle)(struct adreno_device *adreno_dev);
	int (*regulator_enable)(struct adreno_device *adreno_dev);
	void (*regulator_disable)(struct adreno_device *adreno_dev);
	void (*pwrlevel_change_settings)(struct adreno_device *adreno_dev,
				unsigned int prelevel, unsigned int postlevel,
				bool post);
	int64_t (*read_throttling_counters)(struct adreno_device *adreno_dev);
	void (*count_throttles)(struct adreno_device *adreno_dev,
					uint64_t adj);
	unsigned int (*preemption_pre_ibsubmit)(
				struct adreno_device *adreno_dev,
				struct adreno_ringbuffer *rb,
				unsigned int *cmds,
				struct kgsl_context *context);
	int (*preemption_yield_enable)(unsigned int *cmds);
	unsigned int (*set_marker)(unsigned int *cmds,
				enum adreno_cp_marker_type type);
	unsigned int (*preemption_post_ibsubmit)(
				struct adreno_device *adreno_dev,
				unsigned int *cmds);
	int (*preemption_init)(struct adreno_device *adreno_dev);
	void (*preemption_close)(struct adreno_device *adreno_dev);
	void (*preemption_schedule)(struct adreno_device *adreno_dev);
	int (*preemption_context_init)(struct kgsl_context *context);
	void (*preemption_context_destroy)(struct kgsl_context *context);
	void (*enable_64bit)(struct adreno_device *adreno_dev);
	void (*clk_set_options)(struct adreno_device *adreno_dev,
				const char *name, struct clk *clk, bool on);
	void (*llc_configure_gpu_scid)(struct adreno_device *adreno_dev);
	void (*llc_configure_gpuhtw_scid)(struct adreno_device *adreno_dev);
	void (*llc_enable_overrides)(struct adreno_device *adreno_dev);
	void (*pre_reset)(struct adreno_device *adreno_dev);
	void (*gpu_keepalive)(struct adreno_device *adreno_dev,
			bool state);
	bool (*hw_isidle)(struct adreno_device *adreno_dev);
	const char *(*iommu_fault_block)(struct kgsl_device *device,
				unsigned int fsynr1);
	int (*reset)(struct kgsl_device *device, int fault);
	int (*soft_reset)(struct adreno_device *adreno_dev);
	bool (*sptprac_is_on)(struct adreno_device *adreno_dev);
	unsigned int (*ccu_invalidate)(struct adreno_device *adreno_dev,
				unsigned int *cmds);
	int (*perfcounter_update)(struct adreno_device *adreno_dev,
				struct adreno_perfcount_register *reg,
				bool update_reg);
};

/**
 * enum kgsl_ft_policy_bits - KGSL fault tolerance policy bits
 * @KGSL_FT_OFF: Disable fault detection (not used)
 * @KGSL_FT_REPLAY: Replay the faulting command
 * @KGSL_FT_SKIPIB: Skip the faulting indirect buffer
 * @KGSL_FT_SKIPFRAME: Skip the frame containing the faulting IB
 * @KGSL_FT_DISABLE: Tells the dispatcher to disable FT for the command obj
 * @KGSL_FT_TEMP_DISABLE: Disables FT for all commands
 * @KGSL_FT_THROTTLE: Disable the context if it faults too often
 * @KGSL_FT_SKIPCMD: Skip the command containing the faulting IB
 */
enum kgsl_ft_policy_bits {
	KGSL_FT_OFF = 0,
	KGSL_FT_REPLAY = 1,
	KGSL_FT_SKIPIB = 2,
	KGSL_FT_SKIPFRAME = 3,
	KGSL_FT_DISABLE = 4,
	KGSL_FT_TEMP_DISABLE = 5,
	KGSL_FT_THROTTLE = 6,
	KGSL_FT_SKIPCMD = 7,
	/* KGSL_FT_MAX_BITS is used to calculate the mask */
	KGSL_FT_MAX_BITS,
	/* Internal bits - set during GFT */
	/* Skip the PM dump on replayed command obj's */
	KGSL_FT_SKIP_PMDUMP = 31,
};

#define KGSL_FT_POLICY_MASK GENMASK(KGSL_FT_MAX_BITS - 1, 0)

#define  KGSL_FT_DEFAULT_POLICY \
	(BIT(KGSL_FT_REPLAY) | \
	 BIT(KGSL_FT_SKIPCMD) | \
	 BIT(KGSL_FT_THROTTLE))

#define ADRENO_FT_TYPES \
	{ BIT(KGSL_FT_OFF), "off" }, \
	{ BIT(KGSL_FT_REPLAY), "replay" }, \
	{ BIT(KGSL_FT_SKIPIB), "skipib" }, \
	{ BIT(KGSL_FT_SKIPFRAME), "skipframe" }, \
	{ BIT(KGSL_FT_DISABLE), "disable" }, \
	{ BIT(KGSL_FT_TEMP_DISABLE), "temp" }, \
	{ BIT(KGSL_FT_THROTTLE), "throttle"}, \
	{ BIT(KGSL_FT_SKIPCMD), "skipcmd" }

/**
 * enum kgsl_ft_pagefault_policy_bits - KGSL pagefault policy bits
 * @KGSL_FT_PAGEFAULT_INT_ENABLE: No longer used, but retained for compatibility
 * @KGSL_FT_PAGEFAULT_GPUHALT_ENABLE: enable GPU halt on pagefaults
 * @KGSL_FT_PAGEFAULT_LOG_ONE_PER_PAGE: log one pagefault per page
 * @KGSL_FT_PAGEFAULT_LOG_ONE_PER_INT: log one pagefault per interrupt
 */
enum {
	KGSL_FT_PAGEFAULT_INT_ENABLE = 0,
	KGSL_FT_PAGEFAULT_GPUHALT_ENABLE = 1,
	KGSL_FT_PAGEFAULT_LOG_ONE_PER_PAGE = 2,
	KGSL_FT_PAGEFAULT_LOG_ONE_PER_INT = 3,
	/* KGSL_FT_PAGEFAULT_MAX_BITS is used to calculate the mask */
	KGSL_FT_PAGEFAULT_MAX_BITS,
};

#define KGSL_FT_PAGEFAULT_MASK GENMASK(KGSL_FT_PAGEFAULT_MAX_BITS - 1, 0)

#define KGSL_FT_PAGEFAULT_DEFAULT_POLICY 0

#define FOR_EACH_RINGBUFFER(_dev, _rb, _i)			\
	for ((_i) = 0, (_rb) = &((_dev)->ringbuffers[0]);	\
		(_i) < (_dev)->num_ringbuffers;			\
		(_i)++, (_rb)++)

struct adreno_ft_perf_counters {
	unsigned int counter;
	unsigned int countable;
};

extern unsigned int *adreno_ft_regs;
extern unsigned int adreno_ft_regs_num;
extern unsigned int *adreno_ft_regs_val;

extern struct adreno_gpudev adreno_a3xx_gpudev;
extern struct adreno_gpudev adreno_a5xx_gpudev;
extern struct adreno_gpudev adreno_a6xx_gpudev;

extern int adreno_wake_nice;
extern unsigned int adreno_wake_timeout;

int adreno_start(struct kgsl_device *device, int priority);
int adreno_soft_reset(struct kgsl_device *device);
long adreno_ioctl(struct kgsl_device_private *dev_priv,
		unsigned int cmd, unsigned long arg);

long adreno_ioctl_helper(struct kgsl_device_private *dev_priv,
		unsigned int cmd, unsigned long arg,
		const struct kgsl_ioctl *cmds, int len);

int a5xx_critical_packet_submit(struct adreno_device *adreno_dev,
		struct adreno_ringbuffer *rb);
int adreno_set_unsecured_mode(struct adreno_device *adreno_dev,
		struct adreno_ringbuffer *rb);
void adreno_spin_idle_debug(struct adreno_device *adreno_dev, const char *str);
int adreno_spin_idle(struct adreno_device *device, unsigned int timeout);
int adreno_idle(struct kgsl_device *device);
bool adreno_isidle(struct kgsl_device *device);

int adreno_set_constraint(struct kgsl_device *device,
				struct kgsl_context *context,
				struct kgsl_device_constraint *constraint);

void adreno_snapshot(struct kgsl_device *device,
		struct kgsl_snapshot *snapshot,
		struct kgsl_context *context);

int adreno_reset(struct kgsl_device *device, int fault);

void adreno_fault_skipcmd_detached(struct adreno_device *adreno_dev,
					 struct adreno_context *drawctxt,
					 struct kgsl_drawobj *drawobj);

int adreno_coresight_init(struct adreno_device *adreno_dev);

void adreno_coresight_start(struct adreno_device *adreno_dev);
void adreno_coresight_stop(struct adreno_device *adreno_dev);

void adreno_coresight_remove(struct adreno_device *adreno_dev);

bool adreno_hw_isidle(struct adreno_device *adreno_dev);

void adreno_fault_detect_start(struct adreno_device *adreno_dev);
void adreno_fault_detect_stop(struct adreno_device *adreno_dev);

void adreno_hang_int_callback(struct adreno_device *adreno_dev, int bit);
void adreno_cp_callback(struct adreno_device *adreno_dev, int bit);

int adreno_sysfs_init(struct adreno_device *adreno_dev);
void adreno_sysfs_close(struct adreno_device *adreno_dev);

void adreno_irqctrl(struct adreno_device *adreno_dev, int state);

long adreno_ioctl_perfcounter_get(struct kgsl_device_private *dev_priv,
	unsigned int cmd, void *data);

long adreno_ioctl_perfcounter_put(struct kgsl_device_private *dev_priv,
	unsigned int cmd, void *data);

int adreno_efuse_map(struct adreno_device *adreno_dev);
int adreno_efuse_read_u32(struct adreno_device *adreno_dev, unsigned int offset,
		unsigned int *val);
void adreno_efuse_unmap(struct adreno_device *adreno_dev);

bool adreno_is_cx_dbgc_register(struct kgsl_device *device,
		unsigned int offset);
void adreno_cx_dbgc_regread(struct kgsl_device *adreno_device,
		unsigned int offsetwords, unsigned int *value);
void adreno_cx_dbgc_regwrite(struct kgsl_device *device,
		unsigned int offsetwords, unsigned int value);
void adreno_cx_misc_regread(struct adreno_device *adreno_dev,
		unsigned int offsetwords, unsigned int *value);
void adreno_cx_misc_regwrite(struct adreno_device *adreno_dev,
		unsigned int offsetwords, unsigned int value);
void adreno_cx_misc_regrmw(struct adreno_device *adreno_dev,
		unsigned int offsetwords,
		unsigned int mask, unsigned int bits);
void adreno_rscc_regread(struct adreno_device *adreno_dev,
		unsigned int offsetwords, unsigned int *value);
void adreno_isense_regread(struct adreno_device *adreno_dev,
		unsigned int offsetwords, unsigned int *value);
u32 adreno_get_ucode_version(const u32 *data);


#define ADRENO_TARGET(_name, _id) \
static inline int adreno_is_##_name(struct adreno_device *adreno_dev) \
{ \
	return (ADRENO_GPUREV(adreno_dev) == (_id)); \
}

static inline int adreno_is_a3xx(struct adreno_device *adreno_dev)
{
	return ((ADRENO_GPUREV(adreno_dev) >= 300) &&
		(ADRENO_GPUREV(adreno_dev) < 400));
}

ADRENO_TARGET(a304, ADRENO_REV_A304)
ADRENO_TARGET(a306, ADRENO_REV_A306)
ADRENO_TARGET(a306a, ADRENO_REV_A306A)

static inline int adreno_is_a5xx(struct adreno_device *adreno_dev)
{
	return ADRENO_GPUREV(adreno_dev) >= 500 &&
			ADRENO_GPUREV(adreno_dev) < 600;
}

ADRENO_TARGET(a505, ADRENO_REV_A505)
ADRENO_TARGET(a506, ADRENO_REV_A506)
ADRENO_TARGET(a508, ADRENO_REV_A508)
ADRENO_TARGET(a510, ADRENO_REV_A510)
ADRENO_TARGET(a512, ADRENO_REV_A512)
ADRENO_TARGET(a530, ADRENO_REV_A530)
ADRENO_TARGET(a540, ADRENO_REV_A540)

static inline int adreno_is_a530v2(struct adreno_device *adreno_dev)
{
	return (ADRENO_GPUREV(adreno_dev) == ADRENO_REV_A530) &&
		(ADRENO_CHIPID_PATCH(adreno_dev->chipid) == 1);
}

static inline int adreno_is_a530v3(struct adreno_device *adreno_dev)
{
	return (ADRENO_GPUREV(adreno_dev) == ADRENO_REV_A530) &&
		(ADRENO_CHIPID_PATCH(adreno_dev->chipid) == 2);
}

static inline int adreno_is_a505_or_a506(struct adreno_device *adreno_dev)
{
	return ADRENO_GPUREV(adreno_dev) >= 505 &&
			ADRENO_GPUREV(adreno_dev) <= 506;
}

static inline int adreno_is_a6xx(struct adreno_device *adreno_dev)
{
	int rev = ADRENO_GPUREV(adreno_dev);

	return (rev >= 600 && rev < 700) || (rev == 702);
}

ADRENO_TARGET(a610, ADRENO_REV_A610)
ADRENO_TARGET(a612, ADRENO_REV_A612)
ADRENO_TARGET(a618, ADRENO_REV_A618)
ADRENO_TARGET(a619, ADRENO_REV_A619)
ADRENO_TARGET(a620, ADRENO_REV_A620)
ADRENO_TARGET(a630, ADRENO_REV_A630)
ADRENO_TARGET(a640, ADRENO_REV_A640)
ADRENO_TARGET(a650, ADRENO_REV_A650)
ADRENO_TARGET(a680, ADRENO_REV_A680)
ADRENO_TARGET(a702, ADRENO_REV_A702)

/*
 * All the derived chipsets from A615 needs to be added to this
 * list such as A616, A618, A619 etc.
 */
static inline int adreno_is_a615_family(struct adreno_device *adreno_dev)
{
	unsigned int rev = ADRENO_GPUREV(adreno_dev);

	return (rev == ADRENO_REV_A615 || rev == ADRENO_REV_A616 ||
			rev == ADRENO_REV_A618 || rev == ADRENO_REV_A619);
}

/*
 * Derived GPUs from A640 needs to be added to this list.
 * A640 and A680 belongs to this family.
 */
static inline int adreno_is_a640_family(struct adreno_device *adreno_dev)
{
	unsigned int rev = ADRENO_GPUREV(adreno_dev);

	return (rev == ADRENO_REV_A640 || rev == ADRENO_REV_A680);
}

/*
 * Derived GPUs from A650 needs to be added to this list.
 * A650 is derived from A640 but register specs has been
 * changed hence do not belongs to A640 family. A620,
 * A660, A690 follows the register specs of A650.
 *
 */
static inline int adreno_is_a650_family(struct adreno_device *adreno_dev)
{
	unsigned int rev = ADRENO_GPUREV(adreno_dev);

	return (rev == ADRENO_REV_A650 || rev == ADRENO_REV_A620);
}

static inline int adreno_is_a620v1(struct adreno_device *adreno_dev)
{
	return (ADRENO_GPUREV(adreno_dev) == ADRENO_REV_A620) &&
		(ADRENO_CHIPID_PATCH(adreno_dev->chipid) == 0);
}

static inline int adreno_is_a640v2(struct adreno_device *adreno_dev)
{
	return (ADRENO_GPUREV(adreno_dev) == ADRENO_REV_A640) &&
		(ADRENO_CHIPID_PATCH(adreno_dev->chipid) == 1);
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
		gpudev->reg_offsets->offsets[offset_name] == ADRENO_REG_UNUSED)
		return false;

	/*
	 * GPU register programming is kept common as much as possible
	 * across the cores, Use ADRENO_REG_SKIP when certain register
	 * programming needs to be skipped for certain GPU cores.
	 * Example: Certain registers on a5xx like IB1_BASE are 64 bit.
	 * Common programming programs 64bit register but upper 32 bits
	 * are skipped in a3xx using ADRENO_REG_SKIP.
	 */
	if (gpudev->reg_offsets->offsets[offset_name] == ADRENO_REG_SKIP)
		return false;

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
		kgsl_regread(KGSL_DEVICE(adreno_dev),
				gpudev->reg_offsets->offsets[offset_name], val);
	else
		*val = 0;
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
		kgsl_regwrite(KGSL_DEVICE(adreno_dev),
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

/*
 * adreno_read_gmureg() - Read a GMU register by getting its offset from the
 * offset array defined in gpudev node
 * @adreno_dev:		Pointer to the the adreno device
 * @offset_name:	The register enum that is to be read
 * @val:		Register value read is placed here
 */
static inline void adreno_read_gmureg(struct adreno_device *adreno_dev,
				enum adreno_regs offset_name, unsigned int *val)
{
	struct adreno_gpudev *gpudev = ADRENO_GPU_DEVICE(adreno_dev);

	if (adreno_checkreg_off(adreno_dev, offset_name))
		gmu_core_regread(KGSL_DEVICE(adreno_dev),
				gpudev->reg_offsets->offsets[offset_name], val);
	else
		*val = 0;
}

/*
 * adreno_write_gmureg() - Write a GMU register by getting its offset from the
 * offset array defined in gpudev node
 * @adreno_dev:		Pointer to the the adreno device
 * @offset_name:	The register enum that is to be written
 * @val:		Value to write
 */
static inline void adreno_write_gmureg(struct adreno_device *adreno_dev,
				enum adreno_regs offset_name, unsigned int val)
{
	struct adreno_gpudev *gpudev = ADRENO_GPU_DEVICE(adreno_dev);

	if (adreno_checkreg_off(adreno_dev, offset_name))
		gmu_core_regwrite(KGSL_DEVICE(adreno_dev),
				gpudev->reg_offsets->offsets[offset_name], val);
}

/*
 * adreno_get_int() - Returns the offset value of an interrupt bit from
 * the interrupt bit array in the gpudev node
 * @adreno_dev:		Pointer to the the adreno device
 * @bit_name:		The interrupt bit enum whose bit is returned
 */
static inline unsigned int adreno_get_int(struct adreno_device *adreno_dev,
				enum adreno_int_bits bit_name)
{
	struct adreno_gpudev *gpudev = ADRENO_GPU_DEVICE(adreno_dev);

	if (bit_name >= ADRENO_INT_BITS_MAX)
		return -ERANGE;

	return gpudev->int_bits[bit_name];
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
	/* make sure we're reading the latest value */
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
	atomic_or(state, &adreno_dev->dispatcher.fault);

	/* make sure other CPUs see the update */
	smp_wmb();
}

static inline bool adreno_gmu_gpu_fault(struct adreno_device *adreno_dev)
{
	return adreno_gpu_fault(adreno_dev) & ADRENO_GMU_FAULT;
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

	/* make sure other CPUs see the update */
	smp_wmb();
}

/**
 * adreno_gpu_halt() - Return the GPU halt refcount
 * @adreno_dev: A pointer to the adreno_device
 */
static inline int adreno_gpu_halt(struct adreno_device *adreno_dev)
{
	/* make sure we're reading the latest value */
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

	/* make sure other CPUs see the update */
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
	/* Make sure the refcount is good */
	int ret = atomic_dec_if_positive(&adreno_dev->halt);

	WARN(ret < 0, "GPU halt refcount unbalanced\n");
}


/**
 * adreno_reglist_write - Write each register in a reglist
 * @adreno_dev: An Adreno GPU device handle
 * @reglist: A list of &struct adreno_reglist items
 * @count: Number of items in @reglist
 *
 * Write each register listed in @reglist.
 */
void adreno_reglist_write(struct adreno_device *adreno_dev,
		const struct adreno_reglist *list, u32 count);

#ifdef CONFIG_DEBUG_FS
void adreno_debugfs_init(struct adreno_device *adreno_dev);
void adreno_context_debugfs_init(struct adreno_device *adreno_dev,
				struct adreno_context *ctx);
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
	if (adreno_dev->fw[ADRENO_FW_PM4].version == version)
		return 0;

	return (adreno_dev->fw[ADRENO_FW_PM4].version > version) ? 1 : -1;
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
	if (adreno_dev->fw[ADRENO_FW_PFP].version == version)
		return 0;

	return (adreno_dev->fw[ADRENO_FW_PFP].version > version) ? 1 : -1;
}

/**
 * adreno_in_preempt_state() - Check if preemption state is equal to given state
 * @adreno_dev: Device whose preemption state is checked
 * @state: State to compare against
 */
static inline bool adreno_in_preempt_state(struct adreno_device *adreno_dev,
			enum adreno_preempt_states state)
{
	return atomic_read(&adreno_dev->preempt.state) == state;
}
/**
 * adreno_set_preempt_state() - Set the specified preemption state
 * @adreno_dev: Device to change preemption state
 * @state: State to set
 */
static inline void adreno_set_preempt_state(struct adreno_device *adreno_dev,
		enum adreno_preempt_states state)
{
	/*
	 * atomic_set doesn't use barriers, so we need to do it ourselves.  One
	 * before...
	 */
	smp_wmb();
	atomic_set(&adreno_dev->preempt.state, state);

	/* ... and one after */
	smp_wmb();
}

static inline bool adreno_is_preemption_enabled(
				struct adreno_device *adreno_dev)
{
	return test_bit(ADRENO_DEVICE_PREEMPTION, &adreno_dev->priv);
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
	 * If preemption is disabled then everybody needs to go on the same
	 * ringbuffer
	 */

	if (!adreno_is_preemption_enabled(adreno_dev))
		return &(adreno_dev->ringbuffers[0]);

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
 * adreno_compare_prio_level() - Compares 2 priority levels based on enum values
 * @p1: First priority level
 * @p2: Second priority level
 *
 * Returns greater than 0 if p1 is higher priority, 0 if levels are equal else
 * less than 0
 */
static inline int adreno_compare_prio_level(int p1, int p2)
{
	return p2 - p1;
}

void adreno_readreg64(struct adreno_device *adreno_dev,
		enum adreno_regs lo, enum adreno_regs hi, uint64_t *val);

void adreno_writereg64(struct adreno_device *adreno_dev,
		enum adreno_regs lo, enum adreno_regs hi, uint64_t val);

unsigned int adreno_get_rptr(struct adreno_ringbuffer *rb);

static inline bool adreno_rb_empty(struct adreno_ringbuffer *rb)
{
	return (adreno_get_rptr(rb) == rb->wptr);
}

static inline bool adreno_soft_fault_detect(struct adreno_device *adreno_dev)
{
	return adreno_dev->fast_hang_detect &&
		!test_bit(ADRENO_DEVICE_ISDB_ENABLED, &adreno_dev->priv);
}

static inline bool adreno_long_ib_detect(struct adreno_device *adreno_dev)
{
	return adreno_dev->long_ib_detect &&
		!test_bit(ADRENO_DEVICE_ISDB_ENABLED, &adreno_dev->priv);
}

/*
 * adreno_support_64bit() - Check the feature flag only if it is in
 * 64bit kernel otherwise return false
 * adreno_dev: The adreno device
 */
#if BITS_PER_LONG == 64
static inline bool adreno_support_64bit(struct adreno_device *adreno_dev)
{
	return ADRENO_FEATURE(adreno_dev, ADRENO_64BIT);
}
#else
static inline bool adreno_support_64bit(struct adreno_device *adreno_dev)
{
	return false;
}
#endif /*BITS_PER_LONG*/

static inline void adreno_ringbuffer_set_global(
		struct adreno_device *adreno_dev, int name)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);

	kgsl_sharedmem_writel(device,
		&adreno_dev->ringbuffers[0].pagetable_desc,
		PT_INFO_OFFSET(current_global_ptname), name);
}

static inline void adreno_ringbuffer_set_pagetable(struct adreno_ringbuffer *rb,
		struct kgsl_pagetable *pt)
{
	struct adreno_device *adreno_dev = ADRENO_RB_DEVICE(rb);
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	unsigned long flags;

	spin_lock_irqsave(&rb->preempt_lock, flags);

	kgsl_sharedmem_writel(device, &rb->pagetable_desc,
		PT_INFO_OFFSET(current_rb_ptname), pt->name);

	kgsl_sharedmem_writeq(device, &rb->pagetable_desc,
		PT_INFO_OFFSET(ttbr0), kgsl_mmu_pagetable_get_ttbr0(pt));

	kgsl_sharedmem_writel(device, &rb->pagetable_desc,
		PT_INFO_OFFSET(contextidr),
		kgsl_mmu_pagetable_get_contextidr(pt));

	spin_unlock_irqrestore(&rb->preempt_lock, flags);
}

static inline bool is_power_counter_overflow(struct adreno_device *adreno_dev,
	unsigned int reg, unsigned int prev_val, unsigned int *perfctr_pwr_hi)
{
	unsigned int val;
	bool ret = false;

	/*
	 * If prev_val is zero, it is first read after perf counter reset.
	 * So set perfctr_pwr_hi register to zero.
	 */
	if (prev_val == 0) {
		*perfctr_pwr_hi = 0;
		return ret;
	}
	adreno_readreg(adreno_dev, ADRENO_REG_RBBM_PERFCTR_RBBM_0_HI, &val);
	if (val != *perfctr_pwr_hi) {
		*perfctr_pwr_hi = val;
		ret = true;
	}
	return ret;
}

static inline unsigned int counter_delta(struct kgsl_device *device,
			unsigned int reg, unsigned int *counter)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	unsigned int val;
	unsigned int ret = 0;
	bool overflow = true;
	static unsigned int perfctr_pwr_hi;

	/* Read the value */
	kgsl_regread(device, reg, &val);

	if (adreno_is_a5xx(adreno_dev) && reg == adreno_getreg
		(adreno_dev, ADRENO_REG_RBBM_PERFCTR_RBBM_0_LO))
		overflow = is_power_counter_overflow(adreno_dev, reg,
				*counter, &perfctr_pwr_hi);

	/* Return 0 for the first read */
	if (*counter != 0) {
		if (val >= *counter) {
			ret = val - *counter;
		} else if (overflow) {
			ret = (0xFFFFFFFF - *counter) + val;
		} else {
			/*
			 * Since KGSL got abnormal value from the counter,
			 * We will drop the value from being accumulated.
			 */
			dev_warn_once(device->dev,
				"Abnormal value :0x%x (0x%x) from perf counter : 0x%x\n",
				val, *counter, reg);
			return 0;
		}
	}

	*counter = val;
	return ret;
}

static inline int adreno_perfcntr_active_oob_get(struct kgsl_device *device)
{
	int ret = kgsl_active_count_get(device);

	if (!ret) {
		ret = gmu_core_dev_oob_set(device, oob_perfcntr);
		if (ret) {
			gmu_core_snapshot(device);
			adreno_set_gpu_fault(ADRENO_DEVICE(device),
				ADRENO_GMU_FAULT_SKIP_SNAPSHOT);
			adreno_dispatcher_schedule(device);
			kgsl_active_count_put(device);
		}
	}

	return ret;
}

static inline void adreno_perfcntr_active_oob_put(struct kgsl_device *device)
{
	gmu_core_dev_oob_clear(device, oob_perfcntr);
	kgsl_active_count_put(device);
}

static inline bool adreno_has_sptprac_gdsc(struct adreno_device *adreno_dev)
{
	if (adreno_is_a630(adreno_dev) || adreno_is_a615_family(adreno_dev))
		return true;
	else
		return false;
}

static inline bool adreno_has_gbif(struct adreno_device *adreno_dev)
{
	if (!adreno_is_a6xx(adreno_dev) || adreno_is_a630(adreno_dev))
		return false;
	else
		return true;
}

/**
 * adreno_wait_for_halt_ack() - wait for GBIF/VBIF acknowledgment
 * for given HALT request.
 * @ack_reg: register offset to wait for acknowledge
 */
static inline int adreno_wait_for_halt_ack(struct kgsl_device *device,
	int ack_reg, unsigned int mask)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	unsigned long wait_for_vbif;
	unsigned int val;
	int ret = 0;

	/* wait for the transactions to clear */
	wait_for_vbif = jiffies + msecs_to_jiffies(VBIF_RESET_ACK_TIMEOUT);
	while (1) {
		adreno_readreg(adreno_dev, ack_reg,
			&val);
		if ((val & mask) == mask)
			break;
		if (time_after(jiffies, wait_for_vbif)) {
			dev_err(device->dev,
				"GBIF/VBIF Halt ack timeout: reg=%08X mask=%08X status=%08X\n",
				ack_reg, mask, val);
			ret = -ETIMEDOUT;
			break;
		}
	}

	return ret;
}

static inline void adreno_deassert_gbif_halt(struct adreno_device *adreno_dev)
{
	if (adreno_has_gbif(adreno_dev)) {
		adreno_writereg(adreno_dev, ADRENO_REG_GBIF_HALT, 0x0);

		/*
		 * Release GBIF GX halt. For A615 family, GPU GX halt
		 * will be cleared automatically on reset.
		 */
		if (!gmu_core_gpmu_isenabled(KGSL_DEVICE(adreno_dev)) &&
			!adreno_is_a615_family(adreno_dev))
			adreno_writereg(adreno_dev,
				ADRENO_REG_RBBM_GBIF_HALT, 0x0);
	}
}
void adreno_gmu_clear_and_unmask_irqs(struct adreno_device *adreno_dev);
void adreno_gmu_mask_and_clear_irqs(struct adreno_device *adreno_dev);
int adreno_gmu_fenced_write(struct adreno_device *adreno_dev,
	enum adreno_regs offset, unsigned int val,
	unsigned int fence_mask);
int adreno_clear_pending_transactions(struct kgsl_device *device);
void adreno_gmu_send_nmi(struct adreno_device *adreno_dev);
void adreno_smmu_resume(struct adreno_device *adreno_dev);
#endif /*__ADRENO_H */
