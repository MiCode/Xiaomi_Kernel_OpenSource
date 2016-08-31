/*
 * drivers/video/tegra/host/gk20a/pmu_gk20a.h
 *
 * GK20A PMU (aka. gPMU outside gk20a context)
 *
 * Copyright (c) 2011-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 */
#ifndef __PMU_GK20A_H__
#define __PMU_GK20A_H__

/* defined by pmu hw spec */
#define GK20A_PMU_VA_START		((128 * 1024) << 10)
#define GK20A_PMU_VA_SIZE		(512 * 1024 * 1024)
#define GK20A_PMU_INST_SIZE		(4 * 1024)
#define GK20A_PMU_UCODE_SIZE_MAX	(256 * 1024)
#define GK20A_PMU_SEQ_BUF_SIZE		4096

#define ZBC_MASK(i)			(~(~(0) << ((i)+1)) & 0xfffe)

/* PMU Command/Message Interfaces for Adaptive Power */
/* Macro to get Histogram index */
#define PMU_AP_HISTOGRAM(idx)		(idx)
#define PMU_AP_HISTOGRAM_CONT		(4)

/* Total number of histogram bins */
#define PMU_AP_CFG_HISTOGRAM_BIN_N	(16)

/* Mapping between Idle counters and histograms */
#define PMU_AP_IDLE_MASK_HIST_IDX_0		(2)
#define PMU_AP_IDLE_MASK_HIST_IDX_1		(3)
#define PMU_AP_IDLE_MASK_HIST_IDX_2		(5)
#define PMU_AP_IDLE_MASK_HIST_IDX_3		(6)


/* Mapping between AP_CTRLs and Histograms */
#define PMU_AP_HISTOGRAM_IDX_GRAPHICS	(PMU_AP_HISTOGRAM(1))

/* Mapping between AP_CTRLs and Idle counters */
#define PMU_AP_IDLE_MASK_GRAPHICS	(PMU_AP_IDLE_MASK_HIST_IDX_1)

/* Adaptive Power Controls (AP_CTRL) */
enum {
	PMU_AP_CTRL_ID_GRAPHICS = 0x0,
	/* PMU_AP_CTRL_ID_MS         ,*/
	PMU_AP_CTRL_ID_MAX           ,
};

/* AP_CTRL Statistics */
struct pmu_ap_ctrl_stat {
	/*
	 * Represents whether AP is active or not
	 * TODO: This is NvBool in RM; is that 1 byte of 4 bytes?
	 */
	u8	b_active;

	/* Idle filter represented by histogram bin index */
	u8	idle_filter_x;
	u8	rsvd[2];

	/* Total predicted power saving cycles. */
	s32	power_saving_h_cycles;

	/* Counts how many times AP gave us -ve power benefits. */
	u32	bad_decision_count;

	/*
	 * Number of times ap structure needs to skip AP iterations
	 * KICK_CTRL from kernel updates this parameter.
	 */
	u32	skip_count;
	u8	bin[PMU_AP_CFG_HISTOGRAM_BIN_N];
};

/* Parameters initialized by INITn APCTRL command */
struct pmu_ap_ctrl_init_params {
	/* Minimum idle filter value in Us */
	u32	min_idle_filter_us;

	/*
	 * Minimum Targeted Saving in Us. AP will update idle thresholds only
	 * if power saving achieved by updating idle thresholds is greater than
	 * Minimum targeted saving.
	 */
	u32	min_target_saving_us;

	/* Minimum targeted residency of power feature in Us */
	u32	power_break_even_us;

	/*
	 * Maximum number of allowed power feature cycles per sample.
	 *
	 * We are allowing at max "pgPerSampleMax" cycles in one iteration of AP
	 * AKA pgPerSampleMax in original algorithm.
	 */
	u32	cycles_per_sample_max;
};

/* AP Commands/Message structures */

/*
 * Structure for Generic AP Commands
 */
struct pmu_ap_cmd_common {
	u8	cmd_type;
	u16	cmd_id;
};

/*
 * Structure for INIT AP command
 */
struct pmu_ap_cmd_init {
	u8	cmd_type;
	u16	cmd_id;
	u8	rsvd;
	u32	pg_sampling_period_us;
};

/*
 * Structure for Enable/Disable ApCtrl Commands
 */
struct pmu_ap_cmd_enable_ctrl {
	u8	cmd_type;
	u16	cmd_id;

	u8	ctrl_id;
};

struct pmu_ap_cmd_disable_ctrl {
	u8	cmd_type;
	u16	cmd_id;

	u8	ctrl_id;
};

/*
 * Structure for INIT command
 */
struct pmu_ap_cmd_init_ctrl {
	u8				cmd_type;
	u16				cmd_id;
	u8				ctrl_id;
	struct pmu_ap_ctrl_init_params	params;
};

struct pmu_ap_cmd_init_and_enable_ctrl {
	u8				cmd_type;
	u16				cmd_id;
	u8				ctrl_id;
	struct pmu_ap_ctrl_init_params	params;
};

/*
 * Structure for KICK_CTRL command
 */
struct pmu_ap_cmd_kick_ctrl {
	u8	cmd_type;
	u16	cmd_id;
	u8	ctrl_id;

	u32	skip_count;
};

/*
 * Structure for PARAM command
 */
struct pmu_ap_cmd_param {
	u8	cmd_type;
	u16	cmd_id;
	u8	ctrl_id;

	u32	data;
};

/*
 * Defines for AP commands
 */
enum {
	PMU_AP_CMD_ID_INIT = 0x0          ,
	PMU_AP_CMD_ID_INIT_AND_ENABLE_CTRL,
	PMU_AP_CMD_ID_ENABLE_CTRL         ,
	PMU_AP_CMD_ID_DISABLE_CTRL        ,
	PMU_AP_CMD_ID_KICK_CTRL           ,
};

/*
 * AP Command
 */
union pmu_ap_cmd {
	u8					cmd_type;
	struct pmu_ap_cmd_common		cmn;
	struct pmu_ap_cmd_init			init;
	struct pmu_ap_cmd_init_and_enable_ctrl	init_and_enable_ctrl;
	struct pmu_ap_cmd_enable_ctrl		enable_ctrl;
	struct pmu_ap_cmd_disable_ctrl		disable_ctrl;
	struct pmu_ap_cmd_kick_ctrl		kick_ctrl;
};

/*
 * Structure for generic AP Message
 */
struct pmu_ap_msg_common {
	u8	msg_type;
	u16	msg_id;
};

/*
 * Structure for INIT_ACK Message
 */
struct pmu_ap_msg_init_ack {
	u8	msg_type;
	u16	msg_id;
	u8	ctrl_id;
	u32	stats_dmem_offset;
};

/*
 * Defines for AP messages
 */
enum {
	PMU_AP_MSG_ID_INIT_ACK = 0x0,
};

/*
 * AP Message
 */
union pmu_ap_msg {
	u8				msg_type;
	struct pmu_ap_msg_common	cmn;
	struct pmu_ap_msg_init_ack	init_ack;
};

/* Default Sampling Period of AELPG */
#define APCTRL_SAMPLING_PERIOD_PG_DEFAULT_US                    (1000000)

/* Default values of APCTRL parameters */
#define APCTRL_MINIMUM_IDLE_FILTER_DEFAULT_US                   (100)
#define APCTRL_MINIMUM_TARGET_SAVING_DEFAULT_US                 (10000)
#define APCTRL_POWER_BREAKEVEN_DEFAULT_US                       (2000)
#define APCTRL_CYCLES_PER_SAMPLE_MAX_DEFAULT                    (100)

/*
 * Disable reason for Adaptive Power Controller
 */
enum {
	APCTRL_DISABLE_REASON_RM_UNLOAD,
	APCTRL_DISABLE_REASON_RMCTRL,
};

/*
 * Adaptive Power Controller
 */
struct ap_ctrl {
	u32			stats_dmem_offset;
	u32			disable_reason_mask;
	struct pmu_ap_ctrl_stat	stat_cache;
	u8			b_ready;
};

/*
 * Adaptive Power structure
 *
 * ap structure provides generic infrastructure to make any power feature
 * adaptive.
 */
struct pmu_ap {
	u32			supported_mask;
	struct ap_ctrl		ap_ctrl[PMU_AP_CTRL_ID_MAX];
};


enum {
	GK20A_PMU_DMAIDX_UCODE		= 0,
	GK20A_PMU_DMAIDX_VIRT		= 1,
	GK20A_PMU_DMAIDX_PHYS_VID	= 2,
	GK20A_PMU_DMAIDX_PHYS_SYS_COH	= 3,
	GK20A_PMU_DMAIDX_PHYS_SYS_NCOH	= 4,
	GK20A_PMU_DMAIDX_RSVD		= 5,
	GK20A_PMU_DMAIDX_PELPG		= 6,
	GK20A_PMU_DMAIDX_END		= 7
};

struct pmu_mem {
	u32 dma_base;
	u8  dma_offset;
	u8  dma_idx;
};

struct pmu_dmem {
	u16 size;
	u32 offset;
};

/* Make sure size of this structure is a multiple of 4 bytes */
struct pmu_cmdline_args {
	u32 cpu_freq_HZ;		/* Frequency of the clock driving the PMU */
	u32 falc_trace_size;		/* falctrace buffer size (bytes) */
	u32 falc_trace_dma_base;	/* 256-byte block address */
	u32 falc_trace_dma_idx;		/* dmaIdx for DMA operations */
	struct pmu_mem gc6_ctx;		/* dmem offset of gc6 context */
};

#define GK20A_PMU_DMEM_BLKSIZE2		8

#define GK20A_PMU_UCODE_NB_MAX_OVERLAY	    32
#define GK20A_PMU_UCODE_NB_MAX_DATE_LENGTH  64

struct pmu_ucode_desc {
	u32 descriptor_size;
	u32 image_size;
	u32 tools_version;
	u32 app_version;
	char date[GK20A_PMU_UCODE_NB_MAX_DATE_LENGTH];
	u32 bootloader_start_offset;
	u32 bootloader_size;
	u32 bootloader_imem_offset;
	u32 bootloader_entry_point;
	u32 app_start_offset;
	u32 app_size;
	u32 app_imem_offset;
	u32 app_imem_entry;
	u32 app_dmem_offset;
	u32 app_resident_code_offset;  /* Offset from appStartOffset */
	u32 app_resident_code_size;    /* Exact size of the resident code ( potentially contains CRC inside at the end ) */
	u32 app_resident_data_offset;  /* Offset from appStartOffset */
	u32 app_resident_data_size;    /* Exact size of the resident code ( potentially contains CRC inside at the end ) */
	u32 nb_overlays;
	struct {u32 start; u32 size;} load_ovl[GK20A_PMU_UCODE_NB_MAX_OVERLAY];
	u32 compressed;
};

#define PMU_UNIT_REWIND		(0x00)
#define PMU_UNIT_I2C		(0x01)
#define PMU_UNIT_SEQ		(0x02)
#define PMU_UNIT_PG		(0x03)
#define PMU_UNIT_AVAILABLE1	(0x04)
#define PMU_UNIT_AVAILABLE2	(0x05)
#define PMU_UNIT_MEM		(0x06)
#define PMU_UNIT_INIT		(0x07)
#define PMU_UNIT_FBBA		(0x08)
#define PMU_UNIT_DIDLE		(0x09)
#define PMU_UNIT_AVAILABLE3	(0x0A)
#define PMU_UNIT_AVAILABLE4	(0x0B)
#define PMU_UNIT_HDCP_MAIN	(0x0C)
#define PMU_UNIT_HDCP_V		(0x0D)
#define PMU_UNIT_HDCP_SRM	(0x0E)
#define PMU_UNIT_NVDPS		(0x0F)
#define PMU_UNIT_DEINIT		(0x10)
#define PMU_UNIT_AVAILABLE5	(0x11)
#define PMU_UNIT_PERFMON	(0x12)
#define PMU_UNIT_FAN		(0x13)
#define PMU_UNIT_PBI		(0x14)
#define PMU_UNIT_ISOBLIT	(0x15)
#define PMU_UNIT_DETACH		(0x16)
#define PMU_UNIT_DISP		(0x17)
#define PMU_UNIT_HDCP		(0x18)
#define PMU_UNIT_REGCACHE	(0x19)
#define PMU_UNIT_SYSMON		(0x1A)
#define PMU_UNIT_THERM		(0x1B)
#define PMU_UNIT_PMGR		(0x1C)
#define PMU_UNIT_PERF		(0x1D)
#define PMU_UNIT_PCM		(0x1E)
#define PMU_UNIT_RC		(0x1F)
#define PMU_UNIT_NULL		(0x20)
#define PMU_UNIT_LOGGER		(0x21)
#define PMU_UNIT_SMBPBI		(0x22)
#define PMU_UNIT_END		(0x23)

#define PMU_UNIT_TEST_START	(0xFE)
#define PMU_UNIT_END_SIM	(0xFF)
#define PMU_UNIT_TEST_END	(0xFF)

#define PMU_UNIT_ID_IS_VALID(id)		\
		(((id) < PMU_UNIT_END) || ((id) >= PMU_UNIT_TEST_START))

#define PMU_DMEM_ALLOC_ALIGNMENT	(32)
#define PMU_DMEM_ALIGNMENT		(4)

#define PMU_CMD_FLAGS_PMU_MASK		(0xF0)

#define PMU_CMD_FLAGS_STATUS		BIT(0)
#define PMU_CMD_FLAGS_INTR		BIT(1)
#define PMU_CMD_FLAGS_EVENT		BIT(2)
#define PMU_CMD_FLAGS_WATERMARK		BIT(3)

struct pmu_hdr {
	u8 unit_id;
	u8 size;
	u8 ctrl_flags;
	u8 seq_id;
};
#define PMU_MSG_HDR_SIZE	sizeof(struct pmu_hdr)
#define PMU_CMD_HDR_SIZE	sizeof(struct pmu_hdr)

#define PMU_QUEUE_COUNT		5

struct pmu_allocation {
	u8 pad[3];
	u8 fb_mem_use;
	struct {
		struct pmu_dmem dmem;
		struct pmu_mem fb;
	} alloc;
};

enum {
	PMU_INIT_MSG_TYPE_PMU_INIT = 0,
};

struct pmu_init_msg_pmu {
	u8 msg_type;
	u8 pad;

	struct {
		u16 size;
		u16 offset;
		u8  index;
		u8  pad;
	} queue_info [PMU_QUEUE_COUNT];

	u16 sw_managed_area_offset;
	u16 sw_managed_area_size;
};

struct pmu_init_msg {
	union {
		u8 msg_type;
		struct pmu_init_msg_pmu pmu_init;
	};
};

enum {
	PMU_PG_ELPG_MSG_INIT_ACK,
	PMU_PG_ELPG_MSG_DISALLOW_ACK,
	PMU_PG_ELPG_MSG_ALLOW_ACK,
	PMU_PG_ELPG_MSG_FREEZE_ACK,
	PMU_PG_ELPG_MSG_FREEZE_ABORT,
	PMU_PG_ELPG_MSG_UNFREEZE_ACK,
};

struct pmu_pg_msg_elpg_msg {
	u8 msg_type;
	u8 engine_id;
	u16 msg;
};

enum {
	PMU_PG_STAT_MSG_RESP_DMEM_OFFSET = 0,
};

struct pmu_pg_msg_stat {
	u8 msg_type;
	u8 engine_id;
	u16 sub_msg_id;
	u32 data;
};

enum {
	PMU_PG_MSG_ENG_BUF_LOADED,
	PMU_PG_MSG_ENG_BUF_UNLOADED,
	PMU_PG_MSG_ENG_BUF_FAILED,
};

struct pmu_pg_msg_eng_buf_stat {
	u8 msg_type;
	u8 engine_id;
	u8 buf_idx;
	u8 status;
};

struct pmu_pg_msg {
	union {
		u8 msg_type;
		struct pmu_pg_msg_elpg_msg elpg_msg;
		struct pmu_pg_msg_stat stat;
		struct pmu_pg_msg_eng_buf_stat eng_buf_stat;
		/* TBD: other pg messages */
		union pmu_ap_msg ap_msg;
	};
};

enum {
	PMU_RC_MSG_TYPE_UNHANDLED_CMD = 0,
};

struct pmu_rc_msg_unhandled_cmd {
	u8 msg_type;
	u8 unit_id;
};

struct pmu_rc_msg {
	u8 msg_type;
	struct pmu_rc_msg_unhandled_cmd unhandled_cmd;
};

enum {
	PMU_PG_CMD_ID_ELPG_CMD = 0,
	PMU_PG_CMD_ID_ENG_BUF_LOAD,
	PMU_PG_CMD_ID_ENG_BUF_UNLOAD,
	PMU_PG_CMD_ID_PG_STAT,
	PMU_PG_CMD_ID_PG_LOG_INIT,
	PMU_PG_CMD_ID_PG_LOG_FLUSH,
	PMU_PG_CMD_ID_PG_PARAM,
	PMU_PG_CMD_ID_ELPG_INIT,
	PMU_PG_CMD_ID_ELPG_POLL_CTXSAVE,
	PMU_PG_CMD_ID_ELPG_ABORT_POLL,
	PMU_PG_CMD_ID_ELPG_PWR_UP,
	PMU_PG_CMD_ID_ELPG_DISALLOW,
	PMU_PG_CMD_ID_ELPG_ALLOW,
	PMU_PG_CMD_ID_AP,
	PMU_PG_CMD_ID_ZBC_TABLE_UPDATE,
	PMU_PG_CMD_ID_PWR_RAIL_GATE_DISABLE = 0x20,
	PMU_PG_CMD_ID_PWR_RAIL_GATE_ENABLE,
	PMU_PG_CMD_ID_PWR_RAIL_SMU_MSG_DISABLE
};

enum {
	PMU_PG_ELPG_CMD_INIT,
	PMU_PG_ELPG_CMD_DISALLOW,
	PMU_PG_ELPG_CMD_ALLOW,
	PMU_PG_ELPG_CMD_FREEZE,
	PMU_PG_ELPG_CMD_UNFREEZE,
};

struct pmu_pg_cmd_elpg_cmd {
	u8 cmd_type;
	u8 engine_id;
	u16 cmd;
};

struct pmu_pg_cmd_eng_buf_load {
	u8 cmd_type;
	u8 engine_id;
	u8 buf_idx;
	u8 pad;
	u16 buf_size;
	u32 dma_base;
	u8 dma_offset;
	u8 dma_idx;
};

enum {
	PMU_PG_STAT_CMD_ALLOC_DMEM = 0,
};

struct pmu_pg_cmd_stat {
	u8 cmd_type;
	u8 engine_id;
	u16 sub_cmd_id;
	u32 data;
};

struct pmu_pg_cmd {
	union {
		u8 cmd_type;
		struct pmu_pg_cmd_elpg_cmd elpg_cmd;
		struct pmu_pg_cmd_eng_buf_load eng_buf_load;
		struct pmu_pg_cmd_stat stat;
		/* TBD: other pg commands */
		union pmu_ap_cmd ap_cmd;
	};
};

/* PERFMON */
#define PMU_DOMAIN_GROUP_PSTATE		0
#define PMU_DOMAIN_GROUP_GPC2CLK	1
#define PMU_DOMAIN_GROUP_NUM		2

/* TBD: smart strategy */
#define PMU_PERFMON_PCT_TO_INC		58
#define PMU_PERFMON_PCT_TO_DEC		23

struct pmu_perfmon_counter {
	u8 index;
	u8 flags;
	u8 group_id;
	u8 valid;
	u16 upper_threshold; /* units of 0.01% */
	u16 lower_threshold; /* units of 0.01% */
};

#define PMU_PERFMON_FLAG_ENABLE_INCREASE	(0x00000001)
#define PMU_PERFMON_FLAG_ENABLE_DECREASE	(0x00000002)
#define PMU_PERFMON_FLAG_CLEAR_PREV		(0x00000004)

/* PERFMON CMD */
enum {
	PMU_PERFMON_CMD_ID_START = 0,
	PMU_PERFMON_CMD_ID_STOP  = 1,
	PMU_PERFMON_CMD_ID_INIT  = 2
};

struct pmu_perfmon_cmd_start {
	u8 cmd_type;
	u8 group_id;
	u8 state_id;
	u8 flags;
	struct pmu_allocation counter_alloc;
};

struct pmu_perfmon_cmd_stop {
	u8 cmd_type;
};

struct pmu_perfmon_cmd_init {
	u8 cmd_type;
	u8 to_decrease_count;
	u8 base_counter_id;
	u32 sample_period_us;
	struct pmu_allocation counter_alloc;
	u8 num_counters;
	u8 samples_in_moving_avg;
	u16 sample_buffer;
};

struct pmu_perfmon_cmd {
	union {
		u8 cmd_type;
		struct pmu_perfmon_cmd_start start;
		struct pmu_perfmon_cmd_stop stop;
		struct pmu_perfmon_cmd_init init;
	};
};

struct pmu_zbc_cmd {
	u8 cmd_type;
	u8 pad;
	u16 entry_mask;
};

/* PERFMON MSG */
enum {
	PMU_PERFMON_MSG_ID_INCREASE_EVENT = 0,
	PMU_PERFMON_MSG_ID_DECREASE_EVENT = 1,
	PMU_PERFMON_MSG_ID_INIT_EVENT     = 2,
	PMU_PERFMON_MSG_ID_ACK            = 3
};

struct pmu_perfmon_msg_generic {
	u8 msg_type;
	u8 state_id;
	u8 group_id;
	u8 data;
};

struct pmu_perfmon_msg {
	union {
		u8 msg_type;
		struct pmu_perfmon_msg_generic gen;
	};
};


struct pmu_cmd {
	struct pmu_hdr hdr;
	union {
		struct pmu_perfmon_cmd perfmon;
		struct pmu_pg_cmd pg;
		struct pmu_zbc_cmd zbc;
	} cmd;
};

struct pmu_msg {
	struct pmu_hdr hdr;
	union {
		struct pmu_init_msg init;
		struct pmu_perfmon_msg perfmon;
		struct pmu_pg_msg pg;
		struct pmu_rc_msg rc;
	} msg;
};

#define PMU_SHA1_GID_SIGNATURE		0xA7C66AD2
#define PMU_SHA1_GID_SIGNATURE_SIZE	4

#define PMU_SHA1_GID_SIZE	16

struct pmu_sha1_gid {
	bool valid;
	u8 gid[PMU_SHA1_GID_SIZE];
};

struct pmu_sha1_gid_data {
	u8 signature[PMU_SHA1_GID_SIGNATURE_SIZE];
	u8 gid[PMU_SHA1_GID_SIZE];
};

#define PMU_COMMAND_QUEUE_HPQ		0	/* write by sw, read by pmu, protected by sw mutex lock */
#define PMU_COMMAND_QUEUE_LPQ		1	/* write by sw, read by pmu, protected by sw mutex lock */
#define PMU_COMMAND_QUEUE_BIOS		2	/* read/write by sw/hw, protected by hw pmu mutex, id = 2 */
#define PMU_COMMAND_QUEUE_SMI		3	/* read/write by sw/hw, protected by hw pmu mutex, id = 3 */
#define PMU_MESSAGE_QUEUE		4	/* write by pmu, read by sw, accessed by interrupt handler, no lock */
#define PMU_QUEUE_COUNT			5

enum {
	PMU_MUTEX_ID_RSVD1 = 0	,
	PMU_MUTEX_ID_GPUSER	,
	PMU_MUTEX_ID_QUEUE_BIOS	,
	PMU_MUTEX_ID_QUEUE_SMI	,
	PMU_MUTEX_ID_GPMUTEX	,
	PMU_MUTEX_ID_I2C	,
	PMU_MUTEX_ID_RMLOCK	,
	PMU_MUTEX_ID_MSGBOX	,
	PMU_MUTEX_ID_FIFO	,
	PMU_MUTEX_ID_PG		,
	PMU_MUTEX_ID_GR		,
	PMU_MUTEX_ID_CLK	,
	PMU_MUTEX_ID_RSVD6	,
	PMU_MUTEX_ID_RSVD7	,
	PMU_MUTEX_ID_RSVD8	,
	PMU_MUTEX_ID_RSVD9	,
	PMU_MUTEX_ID_INVALID
};

#define PMU_IS_COMMAND_QUEUE(id)	\
		((id)  < PMU_MESSAGE_QUEUE)

#define PMU_IS_SW_COMMAND_QUEUE(id)	\
		(((id) == PMU_COMMAND_QUEUE_HPQ) || \
		 ((id) == PMU_COMMAND_QUEUE_LPQ))

#define  PMU_IS_MESSAGE_QUEUE(id)	\
		((id) == PMU_MESSAGE_QUEUE)

enum
{
	OFLAG_READ = 0,
	OFLAG_WRITE
};

#define QUEUE_SET		(true)
#define QUEUE_GET		(false)

#define QUEUE_ALIGNMENT		(4)

#define PMU_PGENG_GR_BUFFER_IDX_INIT	(0)
#define PMU_PGENG_GR_BUFFER_IDX_ZBC	(1)
#define PMU_PGENG_GR_BUFFER_IDX_FECS	(2)

enum
{
    PMU_DMAIDX_UCODE         = 0,
    PMU_DMAIDX_VIRT          = 1,
    PMU_DMAIDX_PHYS_VID      = 2,
    PMU_DMAIDX_PHYS_SYS_COH  = 3,
    PMU_DMAIDX_PHYS_SYS_NCOH = 4,
    PMU_DMAIDX_RSVD          = 5,
    PMU_DMAIDX_PELPG         = 6,
    PMU_DMAIDX_END           = 7
};

struct pmu_gk20a;
struct pmu_queue;

struct pmu_queue {

	/* used by hw, for BIOS/SMI queue */
	u32 mutex_id;
	u32 mutex_lock;
	/* used by sw, for LPQ/HPQ queue */
	struct mutex mutex;

	/* current write position */
	u32 position;
	/* physical dmem offset where this queue begins */
	u32 offset;
	/* logical queue identifier */
	u32 id;
	/* physical queue index */
	u32 index;
	/* in bytes */
	u32 size;

	/* open-flag */
	u32 oflag;
	bool opened; /* opened implies locked */
	bool locked; /* check free space after setting locked but before setting opened */
};


#define PMU_MUTEX_ID_IS_VALID(id)	\
		((id) < PMU_MUTEX_ID_INVALID)

#define PMU_INVALID_MUTEX_OWNER_ID	(0)

struct pmu_mutex {
	u32 id;
	u32 index;
	u32 ref_cnt;
};

#define PMU_MAX_NUM_SEQUENCES		(256)
#define PMU_SEQ_BIT_SHIFT		(5)
#define PMU_SEQ_TBL_SIZE	\
		(PMU_MAX_NUM_SEQUENCES >> PMU_SEQ_BIT_SHIFT)

#define PMU_INVALID_SEQ_DESC		(~0)

enum
{
	PMU_SEQ_STATE_FREE = 0,
	PMU_SEQ_STATE_PENDING,
	PMU_SEQ_STATE_USED,
	PMU_SEQ_STATE_CANCELLED
};

struct pmu_payload {
	struct {
		void *buf;
		u32 offset;
		u32 size;
	} in, out;
};

typedef void (*pmu_callback)(struct gk20a *, struct pmu_msg *, void *, u32, u32);

struct pmu_sequence {
	u8 id;
	u32 state;
	u32 desc;
	struct pmu_msg *msg;
	struct pmu_allocation in;
	struct pmu_allocation out;
	u8 *out_payload;
	pmu_callback callback;
	void* cb_params;
};

struct pmu_pg_stats {
	u64 pg_entry_start_timestamp;
	u64 pg_ingating_start_timestamp;
	u64 pg_exit_start_timestamp;
	u64 pg_ungating_start_timestamp;
	u32 pg_avg_entry_time_us;
	u32 pg_ingating_cnt;
	u32 pg_ingating_time_us;
	u32 pg_avg_exit_time_us;
	u32 pg_ungating_count;
	u32 pg_ungating_time_us;
	u32 pg_gating_cnt;
	u32 pg_gating_deny_cnt;
};

#define PMU_PG_IDLE_THRESHOLD_SIM		1000
#define PMU_PG_POST_POWERUP_IDLE_THRESHOLD_SIM	4000000
/* TBD: QT or else ? */
#define PMU_PG_IDLE_THRESHOLD			15000
#define PMU_PG_POST_POWERUP_IDLE_THRESHOLD	1000000

/* state transition :
    OFF => [OFF_ON_PENDING optional] => ON_PENDING => ON => OFF
    ON => OFF is always synchronized */
#define PMU_ELPG_STAT_OFF		0   /* elpg is off */
#define PMU_ELPG_STAT_ON		1   /* elpg is on */
#define PMU_ELPG_STAT_ON_PENDING	2   /* elpg is off, ALLOW cmd has been sent, wait for ack */
#define PMU_ELPG_STAT_OFF_PENDING	3   /* elpg is on, DISALLOW cmd has been sent, wait for ack */
#define PMU_ELPG_STAT_OFF_ON_PENDING	4   /* elpg is off, caller has requested on, but ALLOW
					       cmd hasn't been sent due to ENABLE_ALLOW delay */

/* Falcon Register index */
#define PMU_FALCON_REG_R0		(0)
#define PMU_FALCON_REG_R1		(1)
#define PMU_FALCON_REG_R2		(2)
#define PMU_FALCON_REG_R3		(3)
#define PMU_FALCON_REG_R4		(4)
#define PMU_FALCON_REG_R5		(5)
#define PMU_FALCON_REG_R6		(6)
#define PMU_FALCON_REG_R7		(7)
#define PMU_FALCON_REG_R8		(8)
#define PMU_FALCON_REG_R9		(9)
#define PMU_FALCON_REG_R10		(10)
#define PMU_FALCON_REG_R11		(11)
#define PMU_FALCON_REG_R12		(12)
#define PMU_FALCON_REG_R13		(13)
#define PMU_FALCON_REG_R14		(14)
#define PMU_FALCON_REG_R15		(15)
#define PMU_FALCON_REG_IV0		(16)
#define PMU_FALCON_REG_IV1		(17)
#define PMU_FALCON_REG_UNDEFINED	(18)
#define PMU_FALCON_REG_EV		(19)
#define PMU_FALCON_REG_SP		(20)
#define PMU_FALCON_REG_PC		(21)
#define PMU_FALCON_REG_IMB		(22)
#define PMU_FALCON_REG_DMB		(23)
#define PMU_FALCON_REG_CSW		(24)
#define PMU_FALCON_REG_CCR		(25)
#define PMU_FALCON_REG_SEC		(26)
#define PMU_FALCON_REG_CTX		(27)
#define PMU_FALCON_REG_EXCI		(28)
#define PMU_FALCON_REG_RSVD0		(29)
#define PMU_FALCON_REG_RSVD1		(30)
#define PMU_FALCON_REG_RSVD2		(31)
#define PMU_FALCON_REG_SIZE		(32)

struct pmu_gk20a {

	struct gk20a *g;

	struct pmu_ucode_desc *desc;
	struct pmu_mem_desc ucode;

	struct pmu_mem_desc pg_buf;
	/* TBD: remove this if ZBC seq is fixed */
	struct pmu_mem_desc seq_buf;
	bool buf_loaded;

	struct pmu_cmdline_args args;
	struct pmu_sha1_gid gid_info;

	struct pmu_queue queue[PMU_QUEUE_COUNT];

	struct pmu_sequence *seq;
	unsigned long pmu_seq_tbl[PMU_SEQ_TBL_SIZE];
	u32 next_seq_desc;

	struct pmu_mutex *mutex;
	u32 mutex_cnt;

	struct mutex pmu_copy_lock;
	struct mutex pmu_seq_lock;

	struct nvhost_allocator dmem;

	u32 *ucode_image;
	bool pmu_ready;

	u32 zbc_save_done;

	u32 stat_dmem_offset;

	bool elpg_ready;
	u32 elpg_stat;
	wait_queue_head_t pg_wq;

#define PMU_ELPG_ENABLE_ALLOW_DELAY_MSEC	1 /* msec */
	struct delayed_work elpg_enable; /* deferred elpg enable */
	bool elpg_enable_allow; /* true after init, false after disable, true after delay */
	struct mutex elpg_mutex; /* protect elpg enable/disable */
	struct mutex pg_init_mutex; /* protect pmu pg_initialization routine */
	int elpg_refcnt; /* disable -1, enable +1, <=0 elpg disabled, > 0 elpg enabled */

	struct pmu_perfmon_counter perfmon_counter;
	u32 perfmon_state_id[PMU_DOMAIN_GROUP_NUM];

	bool initialized;

	void (*remove_support)(struct pmu_gk20a *pmu);
	bool sw_ready;
	bool perfmon_ready;

	u32 sample_buffer;

	struct mutex isr_mutex;
	bool zbc_ready;
};

struct gk20a_pmu_save_state {
	struct pmu_sequence *seq;
	u32 next_seq_desc;
	struct pmu_mutex *mutex;
	u32 mutex_cnt;
	struct pmu_ucode_desc *desc;
	struct pmu_mem_desc ucode;
	struct pmu_mem_desc seq_buf;
	struct pmu_mem_desc pg_buf;
	struct delayed_work elpg_enable;
	wait_queue_head_t pg_wq;
	bool sw_ready;
};

int gk20a_init_pmu_support(struct gk20a *g);
int gk20a_init_pmu_setup_hw2(struct gk20a *g);

void gk20a_pmu_isr(struct gk20a *g);

/* send a cmd to pmu */
int gk20a_pmu_cmd_post(struct gk20a *g, struct pmu_cmd *cmd, struct pmu_msg *msg,
		struct pmu_payload *payload, u32 queue_id,
		pmu_callback callback, void* cb_param,
		u32 *seq_desc, unsigned long timeout);

int gk20a_pmu_enable_elpg(struct gk20a *g);
int gk20a_pmu_disable_elpg(struct gk20a *g);

void pmu_save_zbc(struct gk20a *g, u32 entries);

int gk20a_pmu_perfmon_enable(struct gk20a *g, bool enable);

int pmu_mutex_acquire(struct pmu_gk20a *pmu, u32 id, u32 *token);
int pmu_mutex_release(struct pmu_gk20a *pmu, u32 id, u32 *token);
int gk20a_pmu_destroy(struct gk20a *g);
int gk20a_pmu_load_norm(struct gk20a *g, u32 *load);
int gk20a_pmu_debugfs_init(struct platform_device *dev);
void gk20a_pmu_reset_load_counters(struct gk20a *g);
void gk20a_pmu_get_load_counters(struct gk20a *g, u32 *busy_cycles,
				 u32 *total_cycles);

#endif /*__PMU_GK20A_H__*/
