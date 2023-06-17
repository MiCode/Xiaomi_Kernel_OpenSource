// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2016-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#define pr_fmt(fmt) "%s:[%s][%d]: " fmt, KBUILD_MODNAME, __func__, __LINE__

#include <linux/debugfs.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/msm_ion.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/of.h>
#include <linux/dma-buf.h>
#include <linux/qcom_scm.h>
#include <soc/qcom/qseecomi.h>
#include <linux/qtee_shmbridge.h>
#include <linux/proc_fs.h>
#if IS_ENABLED(CONFIG_MSM_TMECOM_QMP)
#include <linux/tmelog.h>
#endif

/* QSEE_LOG_BUF_SIZE = 32K */
#define QSEE_LOG_BUF_SIZE 0x8000

/* enlarged qsee log buf size is 128K by default */
#define QSEE_LOG_BUF_SIZE_V2 0x20000

/* Tme log buffer size 20K */
#define TME_LOG_BUF_SIZE 0x5000

/* TZ Diagnostic Area legacy version number */
#define TZBSP_DIAG_MAJOR_VERSION_LEGACY	2

/* TZ Diagnostic Area version number */
#define TZBSP_FVER_MAJOR_MINOR_MASK     0x3FF  /* 10 bits */
#define TZBSP_FVER_MAJOR_SHIFT          22
#define TZBSP_FVER_MINOR_SHIFT          12
#define TZBSP_DIAG_MAJOR_VERSION_V9     9
#define TZBSP_DIAG_MINOR_VERSION_V2     2
#define TZBSP_DIAG_MINOR_VERSION_V21    3
#define TZBSP_DIAG_MINOR_VERSION_V22    4

/* TZ Diag Feature Version Id */
#define QCOM_SCM_FEAT_DIAG_ID           0x06

/*
 * Preprocessor Definitions and Constants
 */
#define TZBSP_MAX_CPU_COUNT 0x08
/*
 * Number of VMID Tables
 */
#define TZBSP_DIAG_NUM_OF_VMID 16
/*
 * VMID Description length
 */
#define TZBSP_DIAG_VMID_DESC_LEN 7
/*
 * Number of Interrupts
 */
#define TZBSP_DIAG_INT_NUM  32
/*
 * Length of descriptive name associated with Interrupt
 */
#define TZBSP_MAX_INT_DESC 16
/*
 * TZ 3.X version info
 */
#define QSEE_VERSION_TZ_3_X 0x800000
/*
 * TZ 4.X version info
 */
#define QSEE_VERSION_TZ_4_X 0x1000000

#define TZBSP_AES_256_ENCRYPTED_KEY_SIZE 256
#define TZBSP_NONCE_LEN 12
#define TZBSP_TAG_LEN 16

#define ENCRYPTED_TZ_LOG_ID 0
#define ENCRYPTED_QSEE_LOG_ID 1

/*
 * Directory for TZ DBG logs
 */
#define TZDBG_DIR_NAME "tzdbg"

/*
 * VMID Table
 */
struct tzdbg_vmid_t {
	uint8_t vmid; /* Virtual Machine Identifier */
	uint8_t desc[TZBSP_DIAG_VMID_DESC_LEN];	/* ASCII Text */
};
/*
 * Boot Info Table
 */
struct tzdbg_boot_info_t {
	uint32_t wb_entry_cnt;	/* Warmboot entry CPU Counter */
	uint32_t wb_exit_cnt;	/* Warmboot exit CPU Counter */
	uint32_t pc_entry_cnt;	/* Power Collapse entry CPU Counter */
	uint32_t pc_exit_cnt;	/* Power Collapse exit CPU counter */
	uint32_t warm_jmp_addr;	/* Last Warmboot Jump Address */
	uint32_t spare;	/* Reserved for future use. */
};
/*
 * Boot Info Table for 64-bit
 */
struct tzdbg_boot_info64_t {
	uint32_t wb_entry_cnt;  /* Warmboot entry CPU Counter */
	uint32_t wb_exit_cnt;   /* Warmboot exit CPU Counter */
	uint32_t pc_entry_cnt;  /* Power Collapse entry CPU Counter */
	uint32_t pc_exit_cnt;   /* Power Collapse exit CPU counter */
	uint32_t psci_entry_cnt;/* PSCI syscall entry CPU Counter */
	uint32_t psci_exit_cnt;   /* PSCI syscall exit CPU Counter */
	uint64_t warm_jmp_addr; /* Last Warmboot Jump Address */
	uint32_t warm_jmp_instr; /* Last Warmboot Jump Address Instruction */
};
/*
 * Reset Info Table
 */
struct tzdbg_reset_info_t {
	uint32_t reset_type;	/* Reset Reason */
	uint32_t reset_cnt;	/* Number of resets occurred/CPU */
};
/*
 * Interrupt Info Table
 */
struct tzdbg_int_t {
	/*
	 * Type of Interrupt/exception
	 */
	uint16_t int_info;
	/*
	 * Availability of the slot
	 */
	uint8_t avail;
	/*
	 * Reserved for future use
	 */
	uint8_t spare;
	/*
	 * Interrupt # for IRQ and FIQ
	 */
	uint32_t int_num;
	/*
	 * ASCII text describing type of interrupt e.g:
	 * Secure Timer, EBI XPU. This string is always null terminated,
	 * supporting at most TZBSP_MAX_INT_DESC characters.
	 * Any additional characters are truncated.
	 */
	uint8_t int_desc[TZBSP_MAX_INT_DESC];
	uint64_t int_count[TZBSP_MAX_CPU_COUNT]; /* # of times seen per CPU */
};

/*
 * Interrupt Info Table used in tz version >=4.X
 */
struct tzdbg_int_t_tz40 {
	uint16_t int_info;
	uint8_t avail;
	uint8_t spare;
	uint32_t int_num;
	uint8_t int_desc[TZBSP_MAX_INT_DESC];
	uint32_t int_count[TZBSP_MAX_CPU_COUNT]; /* uint32_t in TZ ver >= 4.x*/
};

/* warm boot reason for cores */
struct tzbsp_diag_wakeup_info_t {
	/* Wake source info : APCS_GICC_HPPIR */
	uint32_t HPPIR;
	/* Wake source info : APCS_GICC_AHPPIR */
	uint32_t AHPPIR;
};

/*
 * Log ring buffer position
 */
struct tzdbg_log_pos_t {
	uint16_t wrap;
	uint16_t offset;
};

struct tzdbg_log_pos_v2_t {
	uint32_t wrap;
	uint32_t offset;
};

 /*
  * Log ring buffer
  */
struct tzdbg_log_t {
	struct tzdbg_log_pos_t	log_pos;
	/* open ended array to the end of the 4K IMEM buffer */
	uint8_t					log_buf[];
};

struct tzdbg_log_v2_t {
	struct tzdbg_log_pos_v2_t	log_pos;
	/* open ended array to the end of the 4K IMEM buffer */
	uint8_t					log_buf[];
};

struct tzbsp_encr_info_for_log_chunk_t {
	uint32_t size_to_encr;
	uint8_t nonce[TZBSP_NONCE_LEN];
	uint8_t tag[TZBSP_TAG_LEN];
};

/*
 * Only `ENTIRE_LOG` will be used unless the
 * "OEM_tz_num_of_diag_log_chunks_to_encr" devcfg field >= 2.
 * If this is true, the diag log will be encrypted in two
 * separate chunks: a smaller chunk containing only error
 * fatal logs and a bigger "rest of the log" chunk. In this
 * case, `ERR_FATAL_LOG_CHUNK` and `BIG_LOG_CHUNK` will be
 * used instead of `ENTIRE_LOG`.
 */
enum tzbsp_encr_info_for_log_chunks_idx_t {
	BIG_LOG_CHUNK = 0,
	ENTIRE_LOG = 1,
	ERR_FATAL_LOG_CHUNK = 1,
	MAX_NUM_OF_CHUNKS,
};

struct tzbsp_encr_info_t {
	uint32_t num_of_chunks;
	struct tzbsp_encr_info_for_log_chunk_t chunks[MAX_NUM_OF_CHUNKS];
	uint8_t key[TZBSP_AES_256_ENCRYPTED_KEY_SIZE];
};

/*
 * Diagnostic Table
 * Note: This is the reference data structure for tz diagnostic table
 * supporting TZBSP_MAX_CPU_COUNT, the real diagnostic data is directly
 * copied into buffer from i/o memory.
 */
struct tzdbg_t {
	uint32_t magic_num;
	uint32_t version;
	/*
	 * Number of CPU's
	 */
	uint32_t cpu_count;
	/*
	 * Offset of VMID Table
	 */
	uint32_t vmid_info_off;
	/*
	 * Offset of Boot Table
	 */
	uint32_t boot_info_off;
	/*
	 * Offset of Reset info Table
	 */
	uint32_t reset_info_off;
	/*
	 * Offset of Interrupt info Table
	 */
	uint32_t int_info_off;
	/*
	 * Ring Buffer Offset
	 */
	uint32_t ring_off;
	/*
	 * Ring Buffer Length
	 */
	uint32_t ring_len;

	/* Offset for Wakeup info */
	uint32_t wakeup_info_off;

	union {
		/* The elements in below structure have to be used for TZ where
		 * diag version = TZBSP_DIAG_MINOR_VERSION_V2
		 */
		struct {

			/*
			 * VMID to EE Mapping
			 */
			struct tzdbg_vmid_t vmid_info[TZBSP_DIAG_NUM_OF_VMID];
			/*
			 * Boot Info
			 */
			struct tzdbg_boot_info_t  boot_info[TZBSP_MAX_CPU_COUNT];
			/*
			 * Reset Info
			 */
			struct tzdbg_reset_info_t reset_info[TZBSP_MAX_CPU_COUNT];
			uint32_t num_interrupts;
			struct tzdbg_int_t  int_info[TZBSP_DIAG_INT_NUM];
			/* Wake up info */
			struct tzbsp_diag_wakeup_info_t  wakeup_info[TZBSP_MAX_CPU_COUNT];

			uint8_t key[TZBSP_AES_256_ENCRYPTED_KEY_SIZE];

			uint8_t nonce[TZBSP_NONCE_LEN];

			uint8_t tag[TZBSP_TAG_LEN];
		};
		/* The elements in below structure have to be used for TZ where
		 * diag version = TZBSP_DIAG_MINOR_VERSION_V21
		 */
		struct {

			uint32_t encr_info_for_log_off;

			/*
			 * VMID to EE Mapping
			 */
			struct tzdbg_vmid_t vmid_info_v2[TZBSP_DIAG_NUM_OF_VMID];
			/*
			 * Boot Info
			 */
			struct tzdbg_boot_info_t  boot_info_v2[TZBSP_MAX_CPU_COUNT];
			/*
			 * Reset Info
			 */
			struct tzdbg_reset_info_t reset_info_v2[TZBSP_MAX_CPU_COUNT];
			uint32_t num_interrupts_v2;
			struct tzdbg_int_t  int_info_v2[TZBSP_DIAG_INT_NUM];

			/* Wake up info */
			struct tzbsp_diag_wakeup_info_t  wakeup_info_v2[TZBSP_MAX_CPU_COUNT];

			struct tzbsp_encr_info_t encr_info_for_log;
		};
	};

	/*
	 * We need at least 2K for the ring buffer
	 */
	struct tzdbg_log_t ring_buffer;	/* TZ Ring Buffer */
};

struct hypdbg_log_pos_t {
	uint16_t wrap;
	uint16_t offset;
};

struct rmdbg_log_hdr_t {
	uint32_t write_idx;
	uint32_t size;
};
struct rmdbg_log_pos_t {
	uint32_t read_idx;
	uint32_t size;
};
struct hypdbg_boot_info_t {
	uint32_t warm_entry_cnt;
	uint32_t warm_exit_cnt;
};

struct hypdbg_t {
	/* Magic Number */
	uint32_t magic_num;

	/* Number of CPU's */
	uint32_t cpu_count;

	/* Ring Buffer Offset */
	uint32_t ring_off;

	/* Ring buffer position mgmt */
	struct hypdbg_log_pos_t log_pos;
	uint32_t log_len;

	/* S2 fault numbers */
	uint32_t s2_fault_counter;

	/* Boot Info */
	struct hypdbg_boot_info_t boot_info[TZBSP_MAX_CPU_COUNT];

	/* Ring buffer pointer */
	uint8_t log_buf_p[];
};

struct tme_log_pos {
	uint32_t offset;
	size_t size;
};

/*
 * Enumeration order for VMID's
 */
enum tzdbg_stats_type {
	TZDBG_BOOT = 0,
	TZDBG_RESET,
	TZDBG_INTERRUPT,
	TZDBG_VMID,
	TZDBG_GENERAL,
	TZDBG_LOG,
	TZDBG_QSEE_LOG,
	TZDBG_HYP_GENERAL,
	TZDBG_HYP_LOG,
	TZDBG_RM_LOG,
	TZDBG_TME_LOG,
	TZDBG_STATS_MAX
};

struct tzdbg_stat {
	size_t display_len;
	size_t display_offset;
	char *name;
	char *data;
	bool avail;
};

struct tzdbg {
	void __iomem *virt_iobase;
	void __iomem *hyp_virt_iobase;
	void __iomem *rmlog_virt_iobase;
	void __iomem *tmelog_virt_iobase;
	struct tzdbg_t *diag_buf;
	struct hypdbg_t *hyp_diag_buf;
	uint8_t *rm_diag_buf;
	uint8_t *tme_buf;
	char *disp_buf;
	int debug_tz[TZDBG_STATS_MAX];
	struct tzdbg_stat stat[TZDBG_STATS_MAX];
	uint32_t hyp_debug_rw_buf_size;
	uint32_t rmlog_rw_buf_size;
	bool is_hyplog_enabled;
	uint32_t tz_version;
	bool is_encrypted_log_enabled;
	bool is_enlarged_buf;
	bool is_full_encrypted_tz_logs_supported;
	bool is_full_encrypted_tz_logs_enabled;
	int tz_diag_minor_version;
	int tz_diag_major_version;
};

struct tzbsp_encr_log_t {
	/* Magic Number */
	uint32_t magic_num;
	/* version NUMBER */
	uint32_t version;
	/* encrypted log size */
	uint32_t encr_log_buff_size;
	/* Wrap value*/
	uint16_t wrap_count;
	/* AES encryption key wrapped up with oem public key*/
	uint8_t key[TZBSP_AES_256_ENCRYPTED_KEY_SIZE];
	/* Nonce used for encryption*/
	uint8_t nonce[TZBSP_NONCE_LEN];
	/* Tag to be used for Validation */
	uint8_t tag[TZBSP_TAG_LEN];
	/* Encrypted log buffer */
	uint8_t log_buf[1];
};

struct encrypted_log_info {
	phys_addr_t paddr;
	void *vaddr;
	size_t size;
	uint64_t shmb_handle;
};

static struct tzdbg tzdbg = {
	.stat[TZDBG_BOOT].name = "boot",
	.stat[TZDBG_RESET].name = "reset",
	.stat[TZDBG_INTERRUPT].name = "interrupt",
	.stat[TZDBG_VMID].name = "vmid",
	.stat[TZDBG_GENERAL].name = "general",
	.stat[TZDBG_LOG].name = "log",
	.stat[TZDBG_QSEE_LOG].name = "qsee_log",
	.stat[TZDBG_HYP_GENERAL].name = "hyp_general",
	.stat[TZDBG_HYP_LOG].name = "hyp_log",
	.stat[TZDBG_RM_LOG].name = "rm_log",
	.stat[TZDBG_TME_LOG].name = "tme_log",
};

static struct tzdbg_log_t *g_qsee_log;
static struct tzdbg_log_v2_t *g_qsee_log_v2;
static dma_addr_t coh_pmem;
static uint32_t debug_rw_buf_size;
static uint32_t display_buf_size;
static uint32_t qseelog_buf_size;
static phys_addr_t disp_buf_paddr;
static uint32_t tmecrashdump_address_offset;

static uint64_t qseelog_shmbridge_handle;
static struct encrypted_log_info enc_qseelog_info;
static struct encrypted_log_info enc_tzlog_info;

/*
 * Debugfs data structure and functions
 */

static int _disp_tz_general_stats(void)
{
	int len = 0;

	len += scnprintf(tzdbg.disp_buf + len, debug_rw_buf_size - 1,
			"   Version        : 0x%x\n"
			"   Magic Number   : 0x%x\n"
			"   Number of CPU  : %d\n",
			tzdbg.diag_buf->version,
			tzdbg.diag_buf->magic_num,
			tzdbg.diag_buf->cpu_count);
	tzdbg.stat[TZDBG_GENERAL].data = tzdbg.disp_buf;
	return len;
}

static int _disp_tz_vmid_stats(void)
{
	int i, num_vmid;
	int len = 0;
	struct tzdbg_vmid_t *ptr;

	ptr = (struct tzdbg_vmid_t *)((unsigned char *)tzdbg.diag_buf +
					tzdbg.diag_buf->vmid_info_off);
	num_vmid = ((tzdbg.diag_buf->boot_info_off -
				tzdbg.diag_buf->vmid_info_off)/
					(sizeof(struct tzdbg_vmid_t)));

	for (i = 0; i < num_vmid; i++) {
		if (ptr->vmid < 0xFF) {
			len += scnprintf(tzdbg.disp_buf + len,
				(debug_rw_buf_size - 1) - len,
				"   0x%x        %s\n",
				(uint32_t)ptr->vmid, (uint8_t *)ptr->desc);
		}
		if (len > (debug_rw_buf_size - 1)) {
			pr_warn("%s: Cannot fit all info into the buffer\n",
								__func__);
			break;
		}
		ptr++;
	}

	tzdbg.stat[TZDBG_VMID].data = tzdbg.disp_buf;
	return len;
}

static int _disp_tz_boot_stats(void)
{
	int i;
	int len = 0;
	struct tzdbg_boot_info_t *ptr = NULL;
	struct tzdbg_boot_info64_t *ptr_64 = NULL;

	pr_info("qsee_version = 0x%x\n", tzdbg.tz_version);
	if (tzdbg.tz_version >= QSEE_VERSION_TZ_3_X) {
		ptr_64 = (struct tzdbg_boot_info64_t *)((unsigned char *)
			tzdbg.diag_buf + tzdbg.diag_buf->boot_info_off);
	} else {
		ptr = (struct tzdbg_boot_info_t *)((unsigned char *)
			tzdbg.diag_buf + tzdbg.diag_buf->boot_info_off);
	}

	for (i = 0; i < tzdbg.diag_buf->cpu_count; i++) {
		if (tzdbg.tz_version >= QSEE_VERSION_TZ_3_X) {
			len += scnprintf(tzdbg.disp_buf + len,
					(debug_rw_buf_size - 1) - len,
					"  CPU #: %d\n"
					"     Warmboot jump address : 0x%llx\n"
					"     Warmboot entry CPU counter : 0x%x\n"
					"     Warmboot exit CPU counter : 0x%x\n"
					"     Power Collapse entry CPU counter : 0x%x\n"
					"     Power Collapse exit CPU counter : 0x%x\n"
					"     Psci entry CPU counter : 0x%x\n"
					"     Psci exit CPU counter : 0x%x\n"
					"     Warmboot Jump Address Instruction : 0x%x\n",
					i, (uint64_t)ptr_64->warm_jmp_addr,
					ptr_64->wb_entry_cnt,
					ptr_64->wb_exit_cnt,
					ptr_64->pc_entry_cnt,
					ptr_64->pc_exit_cnt,
					ptr_64->psci_entry_cnt,
					ptr_64->psci_exit_cnt,
					ptr_64->warm_jmp_instr);

			if (len > (debug_rw_buf_size - 1)) {
				pr_warn("%s: Cannot fit all info into the buffer\n",
						__func__);
				break;
			}
			ptr_64++;
		} else {
			len += scnprintf(tzdbg.disp_buf + len,
					(debug_rw_buf_size - 1) - len,
					"  CPU #: %d\n"
					"     Warmboot jump address     : 0x%x\n"
					"     Warmboot entry CPU counter: 0x%x\n"
					"     Warmboot exit CPU counter : 0x%x\n"
					"     Power Collapse entry CPU counter: 0x%x\n"
					"     Power Collapse exit CPU counter : 0x%x\n",
					i, ptr->warm_jmp_addr,
					ptr->wb_entry_cnt,
					ptr->wb_exit_cnt,
					ptr->pc_entry_cnt,
					ptr->pc_exit_cnt);

			if (len > (debug_rw_buf_size - 1)) {
				pr_warn("%s: Cannot fit all info into the buffer\n",
						__func__);
				break;
			}
			ptr++;
		}
	}
	tzdbg.stat[TZDBG_BOOT].data = tzdbg.disp_buf;
	return len;
}

static int _disp_tz_reset_stats(void)
{
	int i;
	int len = 0;
	struct tzdbg_reset_info_t *ptr;

	ptr = (struct tzdbg_reset_info_t *)((unsigned char *)tzdbg.diag_buf +
					tzdbg.diag_buf->reset_info_off);

	for (i = 0; i < tzdbg.diag_buf->cpu_count; i++) {
		len += scnprintf(tzdbg.disp_buf + len,
				(debug_rw_buf_size - 1) - len,
				"  CPU #: %d\n"
				"     Reset Type (reason)       : 0x%x\n"
				"     Reset counter             : 0x%x\n",
				i, ptr->reset_type, ptr->reset_cnt);

		if (len > (debug_rw_buf_size - 1)) {
			pr_warn("%s: Cannot fit all info into the buffer\n",
								__func__);
			break;
		}

		ptr++;
	}
	tzdbg.stat[TZDBG_RESET].data = tzdbg.disp_buf;
	return len;
}

static int _disp_tz_interrupt_stats(void)
{
	int i, j;
	int len = 0;
	int *num_int;
	void *ptr;
	struct tzdbg_int_t *tzdbg_ptr;
	struct tzdbg_int_t_tz40 *tzdbg_ptr_tz40;

	num_int = (uint32_t *)((unsigned char *)tzdbg.diag_buf +
			(tzdbg.diag_buf->int_info_off - sizeof(uint32_t)));
	ptr = ((unsigned char *)tzdbg.diag_buf +
					tzdbg.diag_buf->int_info_off);

	pr_info("qsee_version = 0x%x\n", tzdbg.tz_version);

	if (tzdbg.tz_version < QSEE_VERSION_TZ_4_X) {
		tzdbg_ptr = ptr;
		for (i = 0; i < (*num_int); i++) {
			len += scnprintf(tzdbg.disp_buf + len,
				(debug_rw_buf_size - 1) - len,
				"     Interrupt Number          : 0x%x\n"
				"     Type of Interrupt         : 0x%x\n"
				"     Description of interrupt  : %s\n",
				tzdbg_ptr->int_num,
				(uint32_t)tzdbg_ptr->int_info,
				(uint8_t *)tzdbg_ptr->int_desc);
			for (j = 0; j < tzdbg.diag_buf->cpu_count; j++) {
				len += scnprintf(tzdbg.disp_buf + len,
				(debug_rw_buf_size - 1) - len,
				"     int_count on CPU # %d      : %u\n",
				(uint32_t)j,
				(uint32_t)tzdbg_ptr->int_count[j]);
			}
			len += scnprintf(tzdbg.disp_buf + len,
					debug_rw_buf_size - 1, "\n");

			if (len > (debug_rw_buf_size - 1)) {
				pr_warn("%s: Cannot fit all info into buf\n",
								__func__);
				break;
			}
			tzdbg_ptr++;
		}
	} else {
		tzdbg_ptr_tz40 = ptr;
		for (i = 0; i < (*num_int); i++) {
			len += scnprintf(tzdbg.disp_buf + len,
				(debug_rw_buf_size - 1) - len,
				"     Interrupt Number          : 0x%x\n"
				"     Type of Interrupt         : 0x%x\n"
				"     Description of interrupt  : %s\n",
				tzdbg_ptr_tz40->int_num,
				(uint32_t)tzdbg_ptr_tz40->int_info,
				(uint8_t *)tzdbg_ptr_tz40->int_desc);
			for (j = 0; j < tzdbg.diag_buf->cpu_count; j++) {
				len += scnprintf(tzdbg.disp_buf + len,
				(debug_rw_buf_size - 1) - len,
				"     int_count on CPU # %d      : %u\n",
				(uint32_t)j,
				(uint32_t)tzdbg_ptr_tz40->int_count[j]);
			}
			len += scnprintf(tzdbg.disp_buf + len,
					debug_rw_buf_size - 1, "\n");

			if (len > (debug_rw_buf_size - 1)) {
				pr_warn("%s: Cannot fit all info into buf\n",
								__func__);
				break;
			}
			tzdbg_ptr_tz40++;
		}
	}

	tzdbg.stat[TZDBG_INTERRUPT].data = tzdbg.disp_buf;
	return len;
}

static int _disp_tz_log_stats_legacy(void)
{
	int len = 0;
	unsigned char *ptr;

	ptr = (unsigned char *)tzdbg.diag_buf +
					tzdbg.diag_buf->ring_off;
	len += scnprintf(tzdbg.disp_buf, (debug_rw_buf_size - 1) - len,
							"%s\n", ptr);

	tzdbg.stat[TZDBG_LOG].data = tzdbg.disp_buf;
	return len;
}

static int _disp_log_stats(struct tzdbg_log_t *log,
			struct tzdbg_log_pos_t *log_start, uint32_t log_len,
			size_t count, uint32_t buf_idx)
{
	uint32_t wrap_start;
	uint32_t wrap_end;
	uint32_t wrap_cnt;
	int max_len;
	int len = 0;
	int i = 0;

	wrap_start = log_start->wrap;
	wrap_end = log->log_pos.wrap;

	/* Calculate difference in # of buffer wrap-arounds */
	if (wrap_end >= wrap_start)
		wrap_cnt = wrap_end - wrap_start;
	else {
		/* wrap counter has wrapped around, invalidate start position */
		wrap_cnt = 2;
	}

	if (wrap_cnt > 1) {
		/* end position has wrapped around more than once, */
		/* current start no longer valid                   */
		log_start->wrap = log->log_pos.wrap - 1;
		log_start->offset = (log->log_pos.offset + 1) % log_len;
	} else if ((wrap_cnt == 1) &&
		(log->log_pos.offset > log_start->offset)) {
		/* end position has overwritten start */
		log_start->offset = (log->log_pos.offset + 1) % log_len;
	}

	pr_debug("diag_buf wrap = %u, offset = %u\n",
		log->log_pos.wrap, log->log_pos.offset);
	while (log_start->offset == log->log_pos.offset) {
		/*
		 * No data in ring buffer,
		 * so we'll hang around until something happens
		 */
		unsigned long t = msleep_interruptible(50);

		if (t != 0) {
			/* Some event woke us up, so let's quit */
			return 0;
}

		if (buf_idx == TZDBG_LOG)
			memcpy_fromio((void *)tzdbg.diag_buf, tzdbg.virt_iobase,
						debug_rw_buf_size);

	}

	max_len = (count > debug_rw_buf_size) ? debug_rw_buf_size : count;

	pr_debug("diag_buf wrap = %u, offset = %u\n",
		log->log_pos.wrap, log->log_pos.offset);
	/*
	 *  Read from ring buff while there is data and space in return buff
	 */
	while ((log_start->offset != log->log_pos.offset) && (len < max_len)) {
		tzdbg.disp_buf[i++] = log->log_buf[log_start->offset];
		log_start->offset = (log_start->offset + 1) % log_len;
		if (log_start->offset == 0)
			++log_start->wrap;
		++len;
	}

	/*
	 * return buffer to caller
	 */
	tzdbg.stat[buf_idx].data = tzdbg.disp_buf;
	return len;
}

static int _disp_log_stats_v2(struct tzdbg_log_v2_t *log,
			struct tzdbg_log_pos_v2_t *log_start, uint32_t log_len,
			size_t count, uint32_t buf_idx)
{
	uint32_t wrap_start;
	uint32_t wrap_end;
	uint32_t wrap_cnt;
	int max_len;
	int len = 0;
	int i = 0;

	wrap_start = log_start->wrap;
	wrap_end = log->log_pos.wrap;

	/* Calculate difference in # of buffer wrap-arounds */
	if (wrap_end >= wrap_start)
		wrap_cnt = wrap_end - wrap_start;
	else {
		/* wrap counter has wrapped around, invalidate start position */
		wrap_cnt = 2;
}

	if (wrap_cnt > 1) {
		/* end position has wrapped around more than once, */
		/* current start no longer valid                   */
		log_start->wrap = log->log_pos.wrap - 1;
		log_start->offset = (log->log_pos.offset + 1) % log_len;
	} else if ((wrap_cnt == 1) &&
		(log->log_pos.offset > log_start->offset)) {
		/* end position has overwritten start */
		log_start->offset = (log->log_pos.offset + 1) % log_len;
	}
	pr_debug("diag_buf wrap = %u, offset = %u\n",
		log->log_pos.wrap, log->log_pos.offset);

	while (log_start->offset == log->log_pos.offset) {
		/*
		 * No data in ring buffer,
		 * so we'll hang around until something happens
		 */
		unsigned long t = msleep_interruptible(50);

		if (t != 0) {
			/* Some event woke us up, so let's quit */
			return 0;
		}

		if (buf_idx == TZDBG_LOG)
			memcpy_fromio((void *)tzdbg.diag_buf, tzdbg.virt_iobase,
						debug_rw_buf_size);

	}

	max_len = (count > debug_rw_buf_size) ? debug_rw_buf_size : count;

	pr_debug("diag_buf wrap = %u, offset = %u\n",
		log->log_pos.wrap, log->log_pos.offset);

	/*
	 *  Read from ring buff while there is data and space in return buff
	 */
	while ((log_start->offset != log->log_pos.offset) && (len < max_len)) {
		tzdbg.disp_buf[i++] = log->log_buf[log_start->offset];
		log_start->offset = (log_start->offset + 1) % log_len;
		if (log_start->offset == 0)
			++log_start->wrap;
		++len;
	}

	/*
	 * return buffer to caller
	 */
	tzdbg.stat[buf_idx].data = tzdbg.disp_buf;
	return len;
}

static int __disp_hyp_log_stats(uint8_t *log,
			struct hypdbg_log_pos_t *log_start, uint32_t log_len,
			size_t count, uint32_t buf_idx)
{
	struct hypdbg_t *hyp = tzdbg.hyp_diag_buf;
	unsigned long t = 0;
	uint32_t wrap_start;
	uint32_t wrap_end;
	uint32_t wrap_cnt;
	int max_len;
	int len = 0;
	int i = 0;

	wrap_start = log_start->wrap;
	wrap_end = hyp->log_pos.wrap;

	/* Calculate difference in # of buffer wrap-arounds */
	if (wrap_end >= wrap_start)
		wrap_cnt = wrap_end - wrap_start;
	else {
		/* wrap counter has wrapped around, invalidate start position */
		wrap_cnt = 2;
	}

	if (wrap_cnt > 1) {
		/* end position has wrapped around more than once, */
		/* current start no longer valid                   */
		log_start->wrap = hyp->log_pos.wrap - 1;
		log_start->offset = (hyp->log_pos.offset + 1) % log_len;
	} else if ((wrap_cnt == 1) &&
		(hyp->log_pos.offset > log_start->offset)) {
		/* end position has overwritten start */
		log_start->offset = (hyp->log_pos.offset + 1) % log_len;
	}

	while (log_start->offset == hyp->log_pos.offset) {
		/*
		 * No data in ring buffer,
		 * so we'll hang around until something happens
		 */
		t = msleep_interruptible(50);
		if (t != 0) {
			/* Some event woke us up, so let's quit */
			return 0;
		}

		/* TZDBG_HYP_LOG */
		memcpy_fromio((void *)tzdbg.hyp_diag_buf, tzdbg.hyp_virt_iobase,
						tzdbg.hyp_debug_rw_buf_size);
	}

	max_len = (count > tzdbg.hyp_debug_rw_buf_size) ?
				tzdbg.hyp_debug_rw_buf_size : count;

	/*
	 *  Read from ring buff while there is data and space in return buff
	 */
	while ((log_start->offset != hyp->log_pos.offset) && (len < max_len)) {
		tzdbg.disp_buf[i++] = log[log_start->offset];
		log_start->offset = (log_start->offset + 1) % log_len;
		if (log_start->offset == 0)
			++log_start->wrap;
		++len;
	}

	/*
	 * return buffer to caller
	 */
	tzdbg.stat[buf_idx].data = tzdbg.disp_buf;
	return len;
}
static int __disp_rm_log_stats(uint8_t *log_ptr, uint32_t max_len)
{
	uint32_t i = 0;
	/*
	 *  Transfer data from rm dialog buff to display buffer in user space
	 */
	while ((i < max_len) && (i < display_buf_size)) {
		tzdbg.disp_buf[i] = log_ptr[i];
		i++;
	}
	if (i != max_len)
		pr_err("Dropping RM log message, max_len:%d display_buf_size:%d\n",
			i, display_buf_size);
	tzdbg.stat[TZDBG_RM_LOG].data = tzdbg.disp_buf;
	return i;
}

static int print_text(char *intro_message,
			unsigned char *text_addr,
			unsigned int size,
			char *buf, uint32_t buf_len)
{
	unsigned int   i;
	int len = 0;

	pr_debug("begin address %p, size %d\n", text_addr, size);
	len += scnprintf(buf + len, buf_len - len, "%s\n", intro_message);
	for (i = 0;  i < size;  i++) {
		if (buf_len <= len + 6) {
			pr_err("buffer not enough, buf_len %d, len %d\n",
				buf_len, len);
			return buf_len;
		}
		len += scnprintf(buf + len, buf_len - len, "%02hhx ",
					text_addr[i]);
		if ((i & 0x1f) == 0x1f)
			len += scnprintf(buf + len, buf_len - len, "%c", '\n');
	}
	len += scnprintf(buf + len, buf_len - len, "%c", '\n');
	return len;
}

static int _disp_encrpted_log_stats(struct encrypted_log_info *enc_log_info,
				enum tzdbg_stats_type type, uint32_t log_id)
{
	int ret = 0, len = 0;
	struct tzbsp_encr_log_t *encr_log_head;
	uint32_t size = 0;

	if ((!tzdbg.is_full_encrypted_tz_logs_supported) &&
		(tzdbg.is_full_encrypted_tz_logs_enabled))
		pr_info("TZ not supporting full encrypted log functionality\n");
	ret = qcom_scm_request_encrypted_log(enc_log_info->paddr,
		enc_log_info->size, log_id, tzdbg.is_full_encrypted_tz_logs_supported,
		tzdbg.is_full_encrypted_tz_logs_enabled);
	if (ret)
		return 0;
	encr_log_head = (struct tzbsp_encr_log_t *)(enc_log_info->vaddr);
	pr_debug("display_buf_size = %d, encr_log_buff_size = %d\n",
		display_buf_size, encr_log_head->encr_log_buff_size);
	size = encr_log_head->encr_log_buff_size;

	len += scnprintf(tzdbg.disp_buf + len,
			(display_buf_size - 1) - len,
			"\n-------- New Encrypted %s --------\n",
			((log_id == ENCRYPTED_QSEE_LOG_ID) ?
				"QSEE Log" : "TZ Dialog"));

	len += scnprintf(tzdbg.disp_buf + len,
			(display_buf_size - 1) - len,
			"\nMagic_Num :\n0x%x\n"
			"\nVerion :\n%d\n"
			"\nEncr_Log_Buff_Size :\n%d\n"
			"\nWrap_Count :\n%d\n",
			encr_log_head->magic_num,
			encr_log_head->version,
			encr_log_head->encr_log_buff_size,
			encr_log_head->wrap_count);

	len += print_text("\nKey : ", encr_log_head->key,
			TZBSP_AES_256_ENCRYPTED_KEY_SIZE,
			tzdbg.disp_buf + len, display_buf_size);
	len += print_text("\nNonce : ", encr_log_head->nonce,
			TZBSP_NONCE_LEN,
			tzdbg.disp_buf + len, display_buf_size - len);
	len += print_text("\nTag : ", encr_log_head->tag,
			TZBSP_TAG_LEN,
			tzdbg.disp_buf + len, display_buf_size - len);

	if (len > display_buf_size - size)
		pr_warn("Cannot fit all info into the buffer\n");

	pr_debug("encrypted log size %d, disply buffer size %d, used len %d\n",
			size, display_buf_size, len);

	len += print_text("\nLog : ", encr_log_head->log_buf, size,
				tzdbg.disp_buf + len, display_buf_size - len);
	memset(enc_log_info->vaddr, 0, enc_log_info->size);
	tzdbg.stat[type].data = tzdbg.disp_buf;
	return len;
}

static int _disp_tz_log_stats(size_t count)
{
	static struct tzdbg_log_pos_v2_t log_start_v2 = {0};
	static struct tzdbg_log_pos_t log_start = {0};
	struct tzdbg_log_v2_t *log_v2_ptr;
	struct tzdbg_log_t *log_ptr;

	log_ptr = (struct tzdbg_log_t *)((unsigned char *)tzdbg.diag_buf +
			tzdbg.diag_buf->ring_off -
			offsetof(struct tzdbg_log_t, log_buf));

	log_v2_ptr = (struct tzdbg_log_v2_t *)((unsigned char *)tzdbg.diag_buf +
			tzdbg.diag_buf->ring_off -
			offsetof(struct tzdbg_log_v2_t, log_buf));

	if (!tzdbg.is_enlarged_buf)
		return _disp_log_stats(log_ptr, &log_start,
				tzdbg.diag_buf->ring_len, count, TZDBG_LOG);

	return _disp_log_stats_v2(log_v2_ptr, &log_start_v2,
			tzdbg.diag_buf->ring_len, count, TZDBG_LOG);
}

static int _disp_hyp_log_stats(size_t count)
{
	static struct hypdbg_log_pos_t log_start = {0};
	uint8_t *log_ptr;
	uint32_t log_len;

	log_ptr = (uint8_t *)((unsigned char *)tzdbg.hyp_diag_buf +
				tzdbg.hyp_diag_buf->ring_off);
	log_len = tzdbg.hyp_debug_rw_buf_size - tzdbg.hyp_diag_buf->ring_off;

	return __disp_hyp_log_stats(log_ptr, &log_start,
			log_len, count, TZDBG_HYP_LOG);
}

static int _disp_rm_log_stats(size_t count)
{
	static struct rmdbg_log_pos_t log_start = { 0 };
	struct rmdbg_log_hdr_t *p_log_hdr = NULL;
	uint8_t *log_ptr = NULL;
	uint32_t log_len = 0;
	static bool wrap_around = { false };

	/* Return 0 to close the display file,if there is nothing else to do */
	if ((log_start.size == 0x0) && wrap_around) {
		wrap_around = false;
		return 0;
	}
	/* Copy RM log data to tzdbg diag buffer for the first time */
	/* Initialize the tracking data structure */
	if (tzdbg.rmlog_rw_buf_size != 0) {
		if (!wrap_around) {
			memcpy_fromio((void *)tzdbg.rm_diag_buf,
					tzdbg.rmlog_virt_iobase,
					tzdbg.rmlog_rw_buf_size);
			/* get RM header info first */
			p_log_hdr = (struct rmdbg_log_hdr_t *)tzdbg.rm_diag_buf;
			/* Update RM log buffer index tracker and its size */
			log_start.read_idx = 0x0;
			log_start.size = p_log_hdr->size;
		}
		/* Update RM log buffer starting ptr */
		log_ptr =
			(uint8_t *) ((unsigned char *)tzdbg.rm_diag_buf +
				 sizeof(struct rmdbg_log_hdr_t));
	} else {
	/* Return 0 to close the display file,if there is nothing else to do */
		pr_err("There is no RM log to read, size is %d!\n",
			tzdbg.rmlog_rw_buf_size);
		return 0;
	}
	log_len = log_start.size;
	log_ptr += log_start.read_idx;
	/* Check if we exceed the max length provided by user space */
	log_len = (count > log_len) ? log_len : count;
	/* Update tracking data structure */
	log_start.size -= log_len;
	log_start.read_idx += log_len;

	if (log_start.size)
		wrap_around =  true;
	return __disp_rm_log_stats(log_ptr, log_len);
}

static int _disp_qsee_log_stats(size_t count)
{
	static struct tzdbg_log_pos_t log_start = {0};
	static struct tzdbg_log_pos_v2_t log_start_v2 = {0};

	if (!tzdbg.is_enlarged_buf)
		return _disp_log_stats(g_qsee_log, &log_start,
			QSEE_LOG_BUF_SIZE - sizeof(struct tzdbg_log_pos_t),
			count, TZDBG_QSEE_LOG);

	return _disp_log_stats_v2(g_qsee_log_v2, &log_start_v2,
		QSEE_LOG_BUF_SIZE_V2 - sizeof(struct tzdbg_log_pos_v2_t),
		count, TZDBG_QSEE_LOG);
}

static int _disp_hyp_general_stats(size_t count)
{
	int len = 0;
	int i;
	struct hypdbg_boot_info_t *ptr = NULL;

	len += scnprintf((unsigned char *)tzdbg.disp_buf + len,
			tzdbg.hyp_debug_rw_buf_size - 1,
			"   Magic Number    : 0x%x\n"
			"   CPU Count       : 0x%x\n"
			"   S2 Fault Counter: 0x%x\n",
			tzdbg.hyp_diag_buf->magic_num,
			tzdbg.hyp_diag_buf->cpu_count,
			tzdbg.hyp_diag_buf->s2_fault_counter);

	ptr = tzdbg.hyp_diag_buf->boot_info;
	for (i = 0; i < tzdbg.hyp_diag_buf->cpu_count; i++) {
		len += scnprintf((unsigned char *)tzdbg.disp_buf + len,
				(tzdbg.hyp_debug_rw_buf_size - 1) - len,
				"  CPU #: %d\n"
				"     Warmboot entry CPU counter: 0x%x\n"
				"     Warmboot exit CPU counter : 0x%x\n",
				i, ptr->warm_entry_cnt, ptr->warm_exit_cnt);

		if (len > (tzdbg.hyp_debug_rw_buf_size - 1)) {
			pr_warn("%s: Cannot fit all info into the buffer\n",
								__func__);
			break;
		}
		ptr++;
	}

	tzdbg.stat[TZDBG_HYP_GENERAL].data = (char *)tzdbg.disp_buf;
	return len;
}

#if IS_ENABLED(CONFIG_MSM_TMECOM_QMP)
static int _disp_tme_log_stats(size_t count)
{
	static struct tme_log_pos log_start = { 0 };
	static bool wrap_around = { false };
	uint32_t buf_size;
	uint8_t *log_ptr = NULL;
	uint32_t log_len = 0;
	int ret = 0;

	/* Return 0 to close the display file */
	if ((log_start.size == 0x0) && wrap_around) {
		wrap_around = false;
		return 0;
	}

	/* Copy TME log data to tzdbg diag buffer for the first time */
	if (!wrap_around) {
		if (tmelog_process_request(tmecrashdump_address_offset,
								   TME_LOG_BUF_SIZE, &buf_size)) {
			pr_err("Read tme log failed, ret=%d, buf_size: %#x\n", ret, buf_size);
			return 0;
		}
		log_start.offset = 0x0;
		log_start.size = buf_size;
	}

	log_ptr = tzdbg.tmelog_virt_iobase;
	log_len = log_start.size;
	log_ptr += log_start.offset;

	/* Check if we exceed the max length provided by user space */
	log_len = min(min((uint32_t)count, log_len), display_buf_size);

	log_start.size -= log_len;
	log_start.offset += log_len;
	pr_debug("log_len: %d, log_start.offset: %#x, log_start.size: %#x\n",
			log_len, log_start.offset, log_start.size);

	if (log_start.size)
		wrap_around =  true;

	/* Copy TME log data to display buffer */
	memcpy_fromio(tzdbg.disp_buf, log_ptr, log_len);

	tzdbg.stat[TZDBG_TME_LOG].data = tzdbg.disp_buf;
	return log_len;
}
#else
static int _disp_tme_log_stats(size_t count)
{
	return 0;
}
#endif

static ssize_t tzdbg_fs_read_unencrypted(int tz_id, char __user *buf,
	size_t count, loff_t *offp)
{
	int len = 0;

	if (tz_id == TZDBG_BOOT || tz_id == TZDBG_RESET ||
		tz_id == TZDBG_INTERRUPT || tz_id == TZDBG_GENERAL ||
		tz_id == TZDBG_VMID || tz_id == TZDBG_LOG)
		memcpy_fromio((void *)tzdbg.diag_buf, tzdbg.virt_iobase,
						debug_rw_buf_size);

	if (tz_id == TZDBG_HYP_GENERAL || tz_id == TZDBG_HYP_LOG)
		memcpy_fromio((void *)tzdbg.hyp_diag_buf,
				tzdbg.hyp_virt_iobase,
				tzdbg.hyp_debug_rw_buf_size);

	switch (tz_id) {
	case TZDBG_BOOT:
		len = _disp_tz_boot_stats();
		break;
	case TZDBG_RESET:
		len = _disp_tz_reset_stats();
		break;
	case TZDBG_INTERRUPT:
		len = _disp_tz_interrupt_stats();
		break;
	case TZDBG_GENERAL:
		len = _disp_tz_general_stats();
		break;
	case TZDBG_VMID:
		len = _disp_tz_vmid_stats();
		break;
	case TZDBG_LOG:
		if (TZBSP_DIAG_MAJOR_VERSION_LEGACY <
				(tzdbg.diag_buf->version >> 16)) {
			len = _disp_tz_log_stats(count);
			*offp = 0;
		} else {
			len = _disp_tz_log_stats_legacy();
		}
		break;
	case TZDBG_QSEE_LOG:
		len = _disp_qsee_log_stats(count);
		*offp = 0;
		break;
	case TZDBG_HYP_GENERAL:
		len = _disp_hyp_general_stats(count);
		break;
	case TZDBG_HYP_LOG:
		len = _disp_hyp_log_stats(count);
		*offp = 0;
		break;
	case TZDBG_RM_LOG:
		len = _disp_rm_log_stats(count);
		*offp = 0;
		break;
	case TZDBG_TME_LOG:
		len = _disp_tme_log_stats(count);
		*offp = 0;
		break;
	default:
		break;
	}

	if (len > count)
		len = count;

	return simple_read_from_buffer(buf, len, offp,
				tzdbg.stat[tz_id].data, len);
}

static ssize_t tzdbg_fs_read_encrypted(int tz_id, char __user *buf,
	size_t count, loff_t *offp)
{
	int len = 0, ret = 0;
	struct tzdbg_stat *stat = &(tzdbg.stat[tz_id]);

	pr_debug("%s: tz_id = %d\n", __func__, tz_id);

	if (tz_id >= TZDBG_STATS_MAX) {
		pr_err("invalid encrypted log id %d\n", tz_id);
		return ret;
	}

	if (!stat->display_len) {
		if (tz_id == TZDBG_QSEE_LOG)
			stat->display_len = _disp_encrpted_log_stats(
					&enc_qseelog_info,
					tz_id, ENCRYPTED_QSEE_LOG_ID);
		else
			stat->display_len = _disp_encrpted_log_stats(
					&enc_tzlog_info,
					tz_id, ENCRYPTED_TZ_LOG_ID);
		stat->display_offset = 0;
	}
	len = stat->display_len;
	if (len > count)
		len = count;

	*offp = 0;
	ret = simple_read_from_buffer(buf, len, offp,
				tzdbg.stat[tz_id].data + stat->display_offset,
				count);
	stat->display_offset += ret;
	stat->display_len -= ret;
	pr_debug("ret = %d, offset = %d\n", ret, (int)(*offp));
	pr_debug("display_len = %d, offset = %d\n",
			stat->display_len, stat->display_offset);
	return ret;
}

static ssize_t tzdbg_fs_read(struct file *file, char __user *buf,
	size_t count, loff_t *offp)
{
	struct seq_file *seq = file->private_data;
	int tz_id = TZDBG_STATS_MAX;

	if (seq)
		tz_id = *(int *)(seq->private);
	else {
		pr_err("%s: Seq data null unable to proceed\n", __func__);
		return 0;
	}

	if (!tzdbg.is_encrypted_log_enabled ||
	    (tz_id == TZDBG_HYP_GENERAL || tz_id == TZDBG_HYP_LOG)
	    || tz_id == TZDBG_RM_LOG || tz_id == TZDBG_TME_LOG)
		return tzdbg_fs_read_unencrypted(tz_id, buf, count, offp);
	else
		return tzdbg_fs_read_encrypted(tz_id, buf, count, offp);
}

static int tzdbg_procfs_open(struct inode *inode, struct file *file)
{
	return single_open(file, NULL, PDE_DATA(inode));
}

static int tzdbg_procfs_release(struct inode *inode, struct file *file)
{
	return single_release(inode, file);
}

struct proc_ops tzdbg_fops = {
	.proc_flags   = PROC_ENTRY_PERMANENT,
	.proc_read    = tzdbg_fs_read,
	.proc_open    = tzdbg_procfs_open,
	.proc_release = tzdbg_procfs_release,
};

static int tzdbg_init_tme_log(struct platform_device *pdev, void __iomem *virt_iobase)
{
	/*
	 * Tme logs are dumped in tme log ddr region but that region is not
	 * accessible to hlos. Instead, collect logs at tme crashdump ddr
	 * region with tmecom interface and then display logs reading from
	 * crashdump region.
	 */
	if (of_property_read_u32((&pdev->dev)->of_node, "tmecrashdump-address-offset",
				&tmecrashdump_address_offset)) {
		pr_err("Tme Crashdump address offset need to be defined!\n");
		return -EINVAL;
	}

	tzdbg.tmelog_virt_iobase =
		devm_ioremap(&pdev->dev, tmecrashdump_address_offset, TME_LOG_BUF_SIZE);
	if (!tzdbg.tmelog_virt_iobase) {
		pr_err("ERROR: Could not ioremap: start=%#x, len=%u\n",
				tmecrashdump_address_offset, TME_LOG_BUF_SIZE);
		return -ENXIO;
	}

	return 0;
}

/*
 * Allocates log buffer from ION, registers the buffer at TZ
 */
static int tzdbg_register_qsee_log_buf(struct platform_device *pdev)
{
	int ret = 0;
	void *buf = NULL;
	uint32_t ns_vmids[] = {VMID_HLOS};
	uint32_t ns_vm_perms[] = {PERM_READ | PERM_WRITE};
	uint32_t ns_vm_nums = 1;

	if (tzdbg.is_enlarged_buf) {
		if (of_property_read_u32((&pdev->dev)->of_node,
			"qseelog-buf-size-v2", &qseelog_buf_size)) {
			pr_debug("Enlarged qseelog buf size isn't defined\n");
			qseelog_buf_size = QSEE_LOG_BUF_SIZE_V2;
		}
	}  else {
		qseelog_buf_size = QSEE_LOG_BUF_SIZE;
	}
	pr_debug("qseelog buf size is 0x%x\n", qseelog_buf_size);

	buf = dma_alloc_coherent(&pdev->dev,
			qseelog_buf_size, &coh_pmem, GFP_KERNEL);
	if (buf == NULL)
		return -ENOMEM;

	if (!tzdbg.is_encrypted_log_enabled) {
		ret = qtee_shmbridge_register(coh_pmem,
			qseelog_buf_size, ns_vmids, ns_vm_perms, ns_vm_nums,
			PERM_READ | PERM_WRITE,
			&qseelog_shmbridge_handle);
		if (ret) {
			pr_err("failed to create bridge for qsee_log buf\n");
			goto exit_free_mem;
		}
	}

	g_qsee_log = (struct tzdbg_log_t *)buf;
	g_qsee_log->log_pos.wrap = g_qsee_log->log_pos.offset = 0;

	g_qsee_log_v2 = (struct tzdbg_log_v2_t *)buf;
	g_qsee_log_v2->log_pos.wrap = g_qsee_log_v2->log_pos.offset = 0;

	ret = qcom_scm_register_qsee_log_buf(coh_pmem, qseelog_buf_size);
	if (ret != QSEOS_RESULT_SUCCESS) {
		pr_err(
		"%s: scm_call to register log buf failed, resp result =%lld\n",
		__func__, ret);
		goto exit_dereg_bridge;
	}

	return ret;

exit_dereg_bridge:
	if (!tzdbg.is_encrypted_log_enabled)
		qtee_shmbridge_deregister(qseelog_shmbridge_handle);
exit_free_mem:
	dma_free_coherent(&pdev->dev, qseelog_buf_size,
			(void *)g_qsee_log, coh_pmem);
	return ret;
}

static void tzdbg_free_qsee_log_buf(struct platform_device *pdev)
{
	if (!tzdbg.is_encrypted_log_enabled)
		qtee_shmbridge_deregister(qseelog_shmbridge_handle);
	dma_free_coherent(&pdev->dev, qseelog_buf_size,
				(void *)g_qsee_log, coh_pmem);
}

static int tzdbg_allocate_encrypted_log_buf(struct platform_device *pdev)
{
	int ret = 0;
	uint32_t ns_vmids[] = {VMID_HLOS};
	uint32_t ns_vm_perms[] = {PERM_READ | PERM_WRITE};
	uint32_t ns_vm_nums = 1;

	if (!tzdbg.is_encrypted_log_enabled)
		return 0;

	/* max encrypted qsee log buf zize (include header, and page align) */
	enc_qseelog_info.size = qseelog_buf_size + PAGE_SIZE;

	enc_qseelog_info.vaddr = dma_alloc_coherent(&pdev->dev,
					enc_qseelog_info.size,
					&enc_qseelog_info.paddr, GFP_KERNEL);
	if (enc_qseelog_info.vaddr == NULL)
		return -ENOMEM;

	ret = qtee_shmbridge_register(enc_qseelog_info.paddr,
			enc_qseelog_info.size, ns_vmids,
			ns_vm_perms, ns_vm_nums,
			PERM_READ | PERM_WRITE, &enc_qseelog_info.shmb_handle);
	if (ret) {
		pr_err("failed to create encr_qsee_log bridge, ret %d\n", ret);
		goto exit_free_qseelog;
	}
	pr_debug("Alloc memory for encr_qsee_log, size = %zu\n",
			enc_qseelog_info.size);

	enc_tzlog_info.size = debug_rw_buf_size;
	enc_tzlog_info.vaddr = dma_alloc_coherent(&pdev->dev,
					enc_tzlog_info.size,
					&enc_tzlog_info.paddr, GFP_KERNEL);
	if (enc_tzlog_info.vaddr == NULL)
		goto exit_unreg_qseelog;

	ret = qtee_shmbridge_register(enc_tzlog_info.paddr,
			enc_tzlog_info.size, ns_vmids, ns_vm_perms, ns_vm_nums,
			PERM_READ | PERM_WRITE, &enc_tzlog_info.shmb_handle);
	if (ret) {
		pr_err("failed to create encr_tz_log bridge, ret = %d\n", ret);
		goto exit_free_tzlog;
	}
	pr_debug("Alloc memory for encr_tz_log, size %zu\n",
		enc_qseelog_info.size);

	return 0;

exit_free_tzlog:
	dma_free_coherent(&pdev->dev, enc_tzlog_info.size,
			enc_tzlog_info.vaddr, enc_tzlog_info.paddr);
exit_unreg_qseelog:
	qtee_shmbridge_deregister(enc_qseelog_info.shmb_handle);
exit_free_qseelog:
	dma_free_coherent(&pdev->dev, enc_qseelog_info.size,
			enc_qseelog_info.vaddr, enc_qseelog_info.paddr);
	return -ENOMEM;
}

static void tzdbg_free_encrypted_log_buf(struct platform_device *pdev)
{
	qtee_shmbridge_deregister(enc_tzlog_info.shmb_handle);
	dma_free_coherent(&pdev->dev, enc_tzlog_info.size,
			enc_tzlog_info.vaddr, enc_tzlog_info.paddr);
	qtee_shmbridge_deregister(enc_qseelog_info.shmb_handle);
	dma_free_coherent(&pdev->dev, enc_qseelog_info.size,
			enc_qseelog_info.vaddr, enc_qseelog_info.paddr);
}

static int  tzdbg_fs_init(struct platform_device *pdev)
{
	int rc = 0;
	int i;
	struct proc_dir_entry *dent_dir;
	struct proc_dir_entry *dent;

	dent_dir = proc_mkdir(TZDBG_DIR_NAME, NULL);
	if (dent_dir == NULL) {
		dev_err(&pdev->dev, "tzdbg proc_mkdir failed\n");
		return -ENOMEM;
	}

	for (i = 0; i < TZDBG_STATS_MAX; i++) {
		tzdbg.debug_tz[i] = i;
		if (!tzdbg.stat[i].avail)
			continue;
		dent = proc_create_data(tzdbg.stat[i].name,
				0444, dent_dir,
				&tzdbg_fops, &tzdbg.debug_tz[i]);
		if (dent == NULL) {
			dev_err(&pdev->dev, "TZ proc_create_data failed\n");
			rc = -ENOMEM;
			goto err;
		}
	}
	platform_set_drvdata(pdev, dent_dir);
	return 0;
err:
	remove_proc_entry(TZDBG_DIR_NAME, NULL);

	return rc;
}

static void tzdbg_fs_exit(struct platform_device *pdev)
{
	struct proc_dir_entry *dent_dir;

	dent_dir = platform_get_drvdata(pdev);
	if (dent_dir)
		remove_proc_entry(TZDBG_DIR_NAME, NULL);
}

static int __update_hypdbg_base(struct platform_device *pdev,
			void __iomem *virt_iobase)
{
	phys_addr_t hypdiag_phy_iobase;
	uint32_t hyp_address_offset;
	uint32_t hyp_size_offset;
	struct hypdbg_t *hyp;
	uint32_t *ptr = NULL;

	if (of_property_read_u32((&pdev->dev)->of_node, "hyplog-address-offset",
							&hyp_address_offset)) {
		dev_err(&pdev->dev, "hyplog address offset is not defined\n");
		return -EINVAL;
	}
	if (of_property_read_u32((&pdev->dev)->of_node, "hyplog-size-offset",
							&hyp_size_offset)) {
		dev_err(&pdev->dev, "hyplog size offset is not defined\n");
		return -EINVAL;
	}

	hypdiag_phy_iobase = readl_relaxed(virt_iobase + hyp_address_offset);
	tzdbg.hyp_debug_rw_buf_size = readl_relaxed(virt_iobase +
					hyp_size_offset);

	tzdbg.hyp_virt_iobase = devm_ioremap(&pdev->dev,
					hypdiag_phy_iobase,
					tzdbg.hyp_debug_rw_buf_size);
	if (!tzdbg.hyp_virt_iobase) {
		dev_err(&pdev->dev, "ERROR could not ioremap: start=%pr, len=%u\n",
			&hypdiag_phy_iobase, tzdbg.hyp_debug_rw_buf_size);
		return -ENXIO;
	}

	ptr = kzalloc(tzdbg.hyp_debug_rw_buf_size, GFP_KERNEL);
	if (!ptr)
		return -ENOMEM;

	tzdbg.hyp_diag_buf = (struct hypdbg_t *)ptr;
	hyp = tzdbg.hyp_diag_buf;
	hyp->log_pos.wrap = hyp->log_pos.offset = 0;
	return 0;
}

static int __update_rmlog_base(struct platform_device *pdev,
			       void __iomem *virt_iobase)
{
	uint32_t rmlog_address;
	uint32_t rmlog_size;
	uint32_t *ptr = NULL;

	/* if we don't get the node just ignore it */
	if (of_property_read_u32((&pdev->dev)->of_node, "rmlog-address",
							&rmlog_address)) {
		dev_err(&pdev->dev, "RM log address is not defined\n");
		tzdbg.rmlog_rw_buf_size = 0;
		return 0;
	}
	/* if we don't get the node just ignore it */
	if (of_property_read_u32((&pdev->dev)->of_node, "rmlog-size",
							&rmlog_size)) {
		dev_err(&pdev->dev, "RM log size is not defined\n");
		tzdbg.rmlog_rw_buf_size = 0;
		return 0;
	}

	tzdbg.rmlog_rw_buf_size = rmlog_size;

	/* Check if there is RM log to read */
	if (!tzdbg.rmlog_rw_buf_size) {
		tzdbg.rmlog_virt_iobase = NULL;
		tzdbg.rm_diag_buf = NULL;
		dev_err(&pdev->dev, "RM log size is %d\n",
			tzdbg.rmlog_rw_buf_size);
		return 0;
	}

	tzdbg.rmlog_virt_iobase = devm_ioremap(&pdev->dev,
					rmlog_address,
					rmlog_size);
	if (!tzdbg.rmlog_virt_iobase) {
		dev_err(&pdev->dev, "ERROR could not ioremap: start=%pr, len=%u\n",
			rmlog_address, tzdbg.rmlog_rw_buf_size);
		return -ENXIO;
	}

	ptr = kzalloc(tzdbg.rmlog_rw_buf_size, GFP_KERNEL);
	if (!ptr)
		return -ENOMEM;

	tzdbg.rm_diag_buf = (uint8_t *)ptr;
	return 0;
}
static int tzdbg_get_tz_version(void)
{
	u64 version;
	int ret = 0;

	ret = qcom_scm_get_tz_log_feat_id(&version);

	if (ret) {
		pr_err("%s: scm_call to get tz version failed\n",
				__func__);
		return ret;
	}
	tzdbg.tz_version = version;

	ret = qcom_scm_get_tz_feat_id_version(QCOM_SCM_FEAT_DIAG_ID, &version);
	if (ret) {
		pr_err("%s: scm_call to get tz diag version failed, ret = %d\n",
				__func__, ret);
		return ret;
	}
	pr_warn("tz diag version is %x\n", version);
	tzdbg.tz_diag_major_version =
		((version >> TZBSP_FVER_MAJOR_SHIFT) & TZBSP_FVER_MAJOR_MINOR_MASK);
	tzdbg.tz_diag_minor_version =
		((version >> TZBSP_FVER_MINOR_SHIFT) & TZBSP_FVER_MAJOR_MINOR_MASK);
	if (tzdbg.tz_diag_major_version == TZBSP_DIAG_MAJOR_VERSION_V9) {
		switch (tzdbg.tz_diag_minor_version) {
		case TZBSP_DIAG_MINOR_VERSION_V2:
		case TZBSP_DIAG_MINOR_VERSION_V21:
		case TZBSP_DIAG_MINOR_VERSION_V22:
			tzdbg.is_enlarged_buf = true;
			break;
		default:
			tzdbg.is_enlarged_buf = false;
		}
	} else {
		tzdbg.is_enlarged_buf = false;
	}
	return ret;
}

static void tzdbg_query_encrypted_log(void)
{
	int ret = 0;
	uint64_t enabled;

	ret = qcom_scm_query_encrypted_log_feature(&enabled);
	if (ret) {
		pr_err("scm_call QUERY_ENCR_LOG_FEATURE failed ret %d\n", ret);
		tzdbg.is_encrypted_log_enabled = false;
	} else {
		pr_warn("encrypted qseelog enabled is %d\n", enabled);
		tzdbg.is_encrypted_log_enabled = enabled;
	}
}

/*
 * Driver functions
 */
static int tz_log_probe(struct platform_device *pdev)
{
	struct resource *resource;
	void __iomem *virt_iobase;
	phys_addr_t tzdiag_phy_iobase;
	uint32_t *ptr = NULL;
	int ret = 0, i;

	/*
	 * By default all nodes will be created.
	 * Mark avail as false later selectively if there's need to skip proc node creation.
	 */
	for (i = 0; i < TZDBG_STATS_MAX; i++)
		tzdbg.stat[i].avail = true;

	ret = tzdbg_get_tz_version();
	if (ret)
		return ret;

	/*
	 * Get address that stores the physical location diagnostic data
	 */
	resource = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!resource) {
		dev_err(&pdev->dev,
				"%s: ERROR Missing MEM resource\n", __func__);
		return -ENXIO;
	}

	/*
	 * Get the debug buffer size
	 */
	debug_rw_buf_size = resource_size(resource);

	/*
	 * Map address that stores the physical location diagnostic data
	 */
	virt_iobase = devm_ioremap(&pdev->dev, resource->start,
				debug_rw_buf_size);
	if (!virt_iobase) {
		dev_err(&pdev->dev,
			"%s: ERROR could not ioremap: start=%pr, len=%u\n",
			__func__, &resource->start,
			(unsigned int)(debug_rw_buf_size));
		return -ENXIO;
	}

	if (pdev->dev.of_node) {
		tzdbg.is_hyplog_enabled = of_property_read_bool(
			(&pdev->dev)->of_node, "qcom,hyplog-enabled");
		if (tzdbg.is_hyplog_enabled) {
			ret = __update_hypdbg_base(pdev, virt_iobase);
			if (ret) {
				dev_err(&pdev->dev,
					"%s: fail to get hypdbg_base ret %d\n",
					__func__, ret);
				return -EINVAL;
			}
			ret = __update_rmlog_base(pdev, virt_iobase);
			if (ret) {
				dev_err(&pdev->dev,
					"%s: fail to get rmlog_base ret %d\n",
					__func__, ret);
				return -EINVAL;
			}
		} else {
			dev_info(&pdev->dev, "Hyp log service not support\n");
		}
	} else {
		dev_dbg(&pdev->dev, "Device tree data is not found\n");
	}

	/*
	 * Retrieve the address of diagnostic data
	 */
	tzdiag_phy_iobase = readl_relaxed(virt_iobase);

	tzdbg_query_encrypted_log();
	/*
	 * Map the diagnostic information area if encryption is disabled
	 */
	if (!tzdbg.is_encrypted_log_enabled) {
		tzdbg.virt_iobase = devm_ioremap(&pdev->dev,
				tzdiag_phy_iobase, debug_rw_buf_size);

		if (!tzdbg.virt_iobase) {
			dev_err(&pdev->dev,
				"%s: could not ioremap: start=%pr, len=%u\n",
				__func__, &tzdiag_phy_iobase,
				debug_rw_buf_size);
			return -ENXIO;
		}
		/* allocate diag_buf */
		ptr = kzalloc(debug_rw_buf_size, GFP_KERNEL);
		if (ptr == NULL)
			return -ENOMEM;
		tzdbg.diag_buf = (struct tzdbg_t *)ptr;
	} else {
		if ((tzdbg.tz_diag_major_version == TZBSP_DIAG_MAJOR_VERSION_V9) &&
			(tzdbg.tz_diag_minor_version >= TZBSP_DIAG_MINOR_VERSION_V22))
			tzdbg.is_full_encrypted_tz_logs_supported = true;
		if (pdev->dev.of_node) {
			tzdbg.is_full_encrypted_tz_logs_enabled = of_property_read_bool(
				(&pdev->dev)->of_node, "qcom,full-encrypted-tz-logs-enabled");
		}
	}

	/* Init for tme log */
	ret = tzdbg_init_tme_log(pdev, virt_iobase);
	if (ret < 0) {
		tzdbg.stat[TZDBG_TME_LOG].avail = false;
		pr_warn("Tme log initialization failed!\n");
	}

	/* register unencrypted qsee log buffer */
	ret = tzdbg_register_qsee_log_buf(pdev);
	if (ret)
		goto exit_free_diag_buf;

	/* allocate encrypted qsee and tz log buffer */
	ret = tzdbg_allocate_encrypted_log_buf(pdev);
	if (ret) {
		dev_err(&pdev->dev,
			"Failed to allocate encrypted log buffer\n",
			__func__);
		goto exit_free_qsee_log_buf;
	}

	/* allocate display_buf */
	if (UINT_MAX/4 < qseelog_buf_size) {
		pr_err("display_buf_size integer overflow\n");
		goto exit_free_qsee_log_buf;
	}
	display_buf_size = qseelog_buf_size * 4;
	tzdbg.disp_buf = dma_alloc_coherent(&pdev->dev, display_buf_size,
		&disp_buf_paddr, GFP_KERNEL);
	if (tzdbg.disp_buf == NULL) {
		ret = -ENOMEM;
		goto exit_free_encr_log_buf;
	}

	if (tzdbg_fs_init(pdev))
		goto exit_free_disp_buf;
	return 0;

exit_free_disp_buf:
	dma_free_coherent(&pdev->dev, display_buf_size,
			(void *)tzdbg.disp_buf, disp_buf_paddr);
exit_free_encr_log_buf:
	tzdbg_free_encrypted_log_buf(pdev);
exit_free_qsee_log_buf:
	tzdbg_free_qsee_log_buf(pdev);
exit_free_diag_buf:
	if (!tzdbg.is_encrypted_log_enabled)
		kfree(tzdbg.diag_buf);
	return -ENXIO;
}

static int tz_log_remove(struct platform_device *pdev)
{
	tzdbg_fs_exit(pdev);
	dma_free_coherent(&pdev->dev, display_buf_size,
			(void *)tzdbg.disp_buf, disp_buf_paddr);
	tzdbg_free_encrypted_log_buf(pdev);
	tzdbg_free_qsee_log_buf(pdev);
	if (!tzdbg.is_encrypted_log_enabled)
		kfree(tzdbg.diag_buf);
	return 0;
}

static const struct of_device_id tzlog_match[] = {
	{.compatible = "qcom,tz-log"},
	{}
};

static struct platform_driver tz_log_driver = {
	.probe		= tz_log_probe,
	.remove		= tz_log_remove,
	.driver		= {
		.name = "tz_log",
		.of_match_table = tzlog_match,
		.probe_type = PROBE_PREFER_ASYNCHRONOUS,
	},
};

module_platform_driver(tz_log_driver);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("TZ Log driver");
