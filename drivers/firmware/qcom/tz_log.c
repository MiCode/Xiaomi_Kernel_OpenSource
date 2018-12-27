// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2016-2018, The Linux Foundation. All rights reserved.
 */
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
#include <linux/ion_kernel.h>

#include <soc/qcom/scm.h>
#include <soc/qcom/qseecomi.h>

/* QSEE_LOG_BUF_SIZE = 32K */
#define QSEE_LOG_BUF_SIZE 0x8000


/* TZ Diagnostic Area legacy version number */
#define TZBSP_DIAG_MAJOR_VERSION_LEGACY	2
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

 /*
  * Log ring buffer
  */
struct tzdbg_log_t {
	struct tzdbg_log_pos_t	log_pos;
	/* open ended array to the end of the 4K IMEM buffer */
	uint8_t					log_buf[];
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

	/*
	 * We need at least 2K for the ring buffer
	 */
	struct tzdbg_log_t ring_buffer;	/* TZ Ring Buffer */
};

struct hypdbg_log_pos_t {
	uint16_t wrap;
	uint16_t offset;
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
	TZDBG_STATS_MAX
};

struct tzdbg_stat {
	char *name;
	char *data;
};

struct tzdbg {
	void __iomem *virt_iobase;
	void __iomem *hyp_virt_iobase;
	struct tzdbg_t *diag_buf;
	struct hypdbg_t *hyp_diag_buf;
	char *disp_buf;
	int debug_tz[TZDBG_STATS_MAX];
	struct tzdbg_stat stat[TZDBG_STATS_MAX];
	uint32_t hyp_debug_rw_buf_size;
	bool is_hyplog_enabled;
	uint32_t tz_version;
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
};

static struct tzdbg_log_t *g_qsee_log;
static dma_addr_t coh_pmem;
static uint32_t debug_rw_buf_size;

/*
 * Debugfs data structure and functions
 */

static int _disp_tz_general_stats(void)
{
	int len = 0;

	len += snprintf(tzdbg.disp_buf + len, debug_rw_buf_size - 1,
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
			len += snprintf(tzdbg.disp_buf + len,
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
			len += snprintf(tzdbg.disp_buf + len,
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
			len += snprintf(tzdbg.disp_buf + len,
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
		len += snprintf(tzdbg.disp_buf + len,
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
			len += snprintf(tzdbg.disp_buf + len,
				(debug_rw_buf_size - 1) - len,
				"     Interrupt Number          : 0x%x\n"
				"     Type of Interrupt         : 0x%x\n"
				"     Description of interrupt  : %s\n",
				tzdbg_ptr->int_num,
				(uint32_t)tzdbg_ptr->int_info,
				(uint8_t *)tzdbg_ptr->int_desc);
			for (j = 0; j < tzdbg.diag_buf->cpu_count; j++) {
				len += snprintf(tzdbg.disp_buf + len,
				(debug_rw_buf_size - 1) - len,
				"     int_count on CPU # %d      : %u\n",
				(uint32_t)j,
				(uint32_t)tzdbg_ptr->int_count[j]);
			}
			len += snprintf(tzdbg.disp_buf + len,
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
			len += snprintf(tzdbg.disp_buf + len,
				(debug_rw_buf_size - 1) - len,
				"     Interrupt Number          : 0x%x\n"
				"     Type of Interrupt         : 0x%x\n"
				"     Description of interrupt  : %s\n",
				tzdbg_ptr_tz40->int_num,
				(uint32_t)tzdbg_ptr_tz40->int_info,
				(uint8_t *)tzdbg_ptr_tz40->int_desc);
			for (j = 0; j < tzdbg.diag_buf->cpu_count; j++) {
				len += snprintf(tzdbg.disp_buf + len,
				(debug_rw_buf_size - 1) - len,
				"     int_count on CPU # %d      : %u\n",
				(uint32_t)j,
				(uint32_t)tzdbg_ptr_tz40->int_count[j]);
			}
			len += snprintf(tzdbg.disp_buf + len,
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
	len += snprintf(tzdbg.disp_buf, (debug_rw_buf_size - 1) - len,
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
	if (wrap_end >= wrap_start) {
		wrap_cnt = wrap_end - wrap_start;
	} else {
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
	if (wrap_end >= wrap_start) {
		wrap_cnt = wrap_end - wrap_start;
	} else {
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

static int _disp_tz_log_stats(size_t count)
{
	static struct tzdbg_log_pos_t log_start = {0};
	struct tzdbg_log_t *log_ptr;

	log_ptr = (struct tzdbg_log_t *)((unsigned char *)tzdbg.diag_buf +
				tzdbg.diag_buf->ring_off -
				offsetof(struct tzdbg_log_t, log_buf));

	return _disp_log_stats(log_ptr, &log_start,
				tzdbg.diag_buf->ring_len, count, TZDBG_LOG);
}

static int _disp_hyp_log_stats(size_t count)
{
	static struct hypdbg_log_pos_t log_start = {0};
	uint8_t *log_ptr;

	log_ptr = (uint8_t *)((unsigned char *)tzdbg.hyp_diag_buf +
				tzdbg.hyp_diag_buf->ring_off);

	return __disp_hyp_log_stats(log_ptr, &log_start,
			tzdbg.hyp_debug_rw_buf_size, count, TZDBG_HYP_LOG);
}

static int _disp_qsee_log_stats(size_t count)
{
	static struct tzdbg_log_pos_t log_start = {0};

	return _disp_log_stats(g_qsee_log, &log_start,
			QSEE_LOG_BUF_SIZE - sizeof(struct tzdbg_log_pos_t),
			count, TZDBG_QSEE_LOG);
}

static int _disp_hyp_general_stats(size_t count)
{
	int len = 0;
	int i;
	struct hypdbg_boot_info_t *ptr = NULL;

	len += snprintf((unsigned char *)tzdbg.disp_buf + len,
			tzdbg.hyp_debug_rw_buf_size - 1,
			"   Magic Number    : 0x%x\n"
			"   CPU Count       : 0x%x\n"
			"   S2 Fault Counter: 0x%x\n",
			tzdbg.hyp_diag_buf->magic_num,
			tzdbg.hyp_diag_buf->cpu_count,
			tzdbg.hyp_diag_buf->s2_fault_counter);

	ptr = tzdbg.hyp_diag_buf->boot_info;
	for (i = 0; i < tzdbg.hyp_diag_buf->cpu_count; i++) {
		len += snprintf((unsigned char *)tzdbg.disp_buf + len,
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

static ssize_t tzdbgfs_read(struct file *file, char __user *buf,
	size_t count, loff_t *offp)
{
	int len = 0;
	int *tz_id =  file->private_data;

	if (*tz_id == TZDBG_BOOT || *tz_id == TZDBG_RESET ||
		*tz_id == TZDBG_INTERRUPT || *tz_id == TZDBG_GENERAL ||
		*tz_id == TZDBG_VMID || *tz_id == TZDBG_LOG)
		memcpy_fromio((void *)tzdbg.diag_buf, tzdbg.virt_iobase,
						debug_rw_buf_size);

	if (*tz_id == TZDBG_HYP_GENERAL || *tz_id == TZDBG_HYP_LOG)
		memcpy_fromio((void *)tzdbg.hyp_diag_buf, tzdbg.hyp_virt_iobase,
					tzdbg.hyp_debug_rw_buf_size);

	switch (*tz_id) {
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
	default:
		break;
	}

	if (len > count)
		len = count;

	return simple_read_from_buffer(buf, len, offp,
				tzdbg.stat[(*tz_id)].data, len);
}

static const struct file_operations tzdbg_fops = {
	.owner   = THIS_MODULE,
	.read    = tzdbgfs_read,
	.open    = simple_open,
};


/*
 * Allocates log buffer from ION, registers the buffer at TZ
 */
static void tzdbg_register_qsee_log_buf(struct platform_device *pdev)
{
	size_t len;
	int ret = 0;
	struct scm_desc desc = {0};
	void *buf = NULL;

	len = QSEE_LOG_BUF_SIZE;
	buf = dma_alloc_coherent(&pdev->dev, len, &coh_pmem, GFP_KERNEL);
	if (buf == NULL) {
		pr_err("Failed to alloc memory for size %zu\n", len);
		return;
	}

	g_qsee_log = (struct tzdbg_log_t *)buf;

	desc.args[0] = coh_pmem;
	desc.args[1] = len;
	desc.arginfo = 0x22;
	ret = scm_call2(SCM_QSEEOS_FNID(1, 6), &desc);

	if (ret) {
		pr_err("%s: scm_call to register log buffer failed\n",
			__func__);
		goto err;
	}

	if (desc.ret[0] != QSEOS_RESULT_SUCCESS) {
		pr_err(
		"%s: scm_call to register log buf failed, resp result =%d\n",
		__func__, desc.ret[0]);
		goto err;
	}

	g_qsee_log->log_pos.wrap = g_qsee_log->log_pos.offset = 0;
	return;

err:
	dma_free_coherent(&pdev->dev, len, (void *)g_qsee_log, coh_pmem);
}

static int  tzdbgfs_init(struct platform_device *pdev)
{
	int rc = 0;
	int i;
	struct dentry           *dent_dir;
	struct dentry           *dent;

	dent_dir = debugfs_create_dir("tzdbg", NULL);
	if (dent_dir == NULL) {
		dev_err(&pdev->dev, "tzdbg debugfs_create_dir failed\n");
		return -ENOMEM;
	}

	for (i = 0; i < TZDBG_STATS_MAX; i++) {
		tzdbg.debug_tz[i] = i;
		dent = debugfs_create_file_unsafe(tzdbg.stat[i].name,
				0444, dent_dir,
				&tzdbg.debug_tz[i], &tzdbg_fops);
		if (dent == NULL) {
			dev_err(&pdev->dev, "TZ debugfs_create_file failed\n");
			rc = -ENOMEM;
			goto err;
		}
	}
	tzdbg.disp_buf = kzalloc(max(debug_rw_buf_size,
			tzdbg.hyp_debug_rw_buf_size), GFP_KERNEL);
	if (tzdbg.disp_buf == NULL)
		goto err;
	platform_set_drvdata(pdev, dent_dir);
	return 0;
err:
	debugfs_remove_recursive(dent_dir);

	return rc;
}

static void tzdbgfs_exit(struct platform_device *pdev)
{
	struct dentry           *dent_dir;

	kzfree(tzdbg.disp_buf);
	dent_dir = platform_get_drvdata(pdev);
	debugfs_remove_recursive(dent_dir);
	if (g_qsee_log)
		dma_free_coherent(&pdev->dev, QSEE_LOG_BUF_SIZE,
					 (void *)g_qsee_log, coh_pmem);
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

	tzdbg.hyp_virt_iobase = devm_ioremap_nocache(&pdev->dev,
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

static void tzdbg_get_tz_version(void)
{
	uint32_t smc_id = 0;
	uint32_t feature = 10;
	struct scm_desc desc = {0};
	int ret = 0;

	smc_id = TZ_INFO_GET_FEATURE_VERSION_ID;
	desc.arginfo = TZ_INFO_GET_FEATURE_VERSION_ID_PARAM_ID;
	desc.args[0] = feature;
	ret = scm_call2(smc_id, &desc);

	if (ret)
		pr_err("%s: scm_call to get tz version failed\n",
				__func__);
	else
		tzdbg.tz_version = desc.ret[0];

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
	int ret = 0;

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
	virt_iobase = devm_ioremap_nocache(&pdev->dev, resource->start,
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
				dev_err(&pdev->dev, "%s() failed to get device tree data ret = %d\n",
						__func__, ret);
				return -EINVAL;
			}
		} else {
			dev_info(&pdev->dev, "Hyp log service is not supported\n");
		}
	} else {
		dev_dbg(&pdev->dev, "Device tree data is not found\n");
	}

	/*
	 * Retrieve the address of diagnostic data
	 */
	tzdiag_phy_iobase = readl_relaxed(virt_iobase);

	/*
	 * Map the diagnostic information area
	 */
	tzdbg.virt_iobase = devm_ioremap_nocache(&pdev->dev,
				tzdiag_phy_iobase, debug_rw_buf_size);

	if (!tzdbg.virt_iobase) {
		dev_err(&pdev->dev,
			"%s: ERROR could not ioremap: start=%pr, len=%u\n",
			__func__, &tzdiag_phy_iobase,
			debug_rw_buf_size);
		return -ENXIO;
	}

	ptr = kzalloc(debug_rw_buf_size, GFP_KERNEL);
	if (ptr == NULL)
		return -ENXIO;

	tzdbg.diag_buf = (struct tzdbg_t *)ptr;

	if (tzdbgfs_init(pdev))
		goto err;

	tzdbg_register_qsee_log_buf(pdev);

	tzdbg_get_tz_version();

	return 0;
err:
	kfree(tzdbg.diag_buf);
	return -ENXIO;
}


static int tz_log_remove(struct platform_device *pdev)
{
	kzfree(tzdbg.diag_buf);
	kzfree(tzdbg.hyp_diag_buf);
	tzdbgfs_exit(pdev);

	return 0;
}

static const struct of_device_id tzlog_match[] = {
	{	.compatible = "qcom,tz-log",
	},
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

static int __init tz_log_init(void)
{
	return platform_driver_register(&tz_log_driver);
}

static void __exit tz_log_exit(void)
{
	platform_driver_unregister(&tz_log_driver);
}

module_init(tz_log_init);
module_exit(tz_log_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("TZ Log driver");
MODULE_ALIAS("platform:tz_log");
