/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <asm/memory.h>
#include <asm/cacheflush.h>
#include <linux/kdebug.h>
#include <linux/module.h>
#include <mrdump.h>
#include <mt-plat/mtk_ram_console.h>
#include <mach/wd_api.h>
#include <linux/reboot.h>
#include "ipanic.h"
#include <asm/system_misc.h>
#include <mtk_rtc.h>

static u32 ipanic_iv = 0xaabbccdd;
static spinlock_t ipanic_lock;
struct ipanic_ops *ipanic_ops;
typedef int (*fn_next) (void *data, unsigned char *buffer, size_t sz_buf);
static bool ipanic_enable = 1;

int __weak ipanic_atflog_buffer(void *data, unsigned char *buffer, size_t sz_buf)
{
	return 0;
}

int __weak panic_dump_android_log(char *buffer, size_t sz_buf, int type)
{
	return 0;
}

int __weak has_mt_dump_support(void)
{
	pr_notice("%s: no mt_dump support!\n", __func__);
	return 0;
}

int __weak panic_dump_disp_log(void *data, unsigned char *buffer, size_t sz_buf)
{
	pr_notice("%s: weak function\n", __func__);
	return 0;
}

#if 1
void ipanic_block_scramble(u8 *buf, int buflen)
{
	int i;
	u32 *p = (u32 *) buf;

	for (i = 0; i < buflen; i += 4, p++)
		*p = *p ^ ipanic_iv;
}
#else
void ipanic_block_scramble(u8 *buf, int buflen)
{
}
#endif

static void ipanic_kick_wdt(void)
{
	int res = 0;
	struct wd_api *wd_api = NULL;

	res = get_wd_api(&wd_api);
	if (res == 0)
		wd_api->wd_restart(WD_TYPE_NOLOCK);
}

void register_ipanic_ops(struct ipanic_ops *ops)
{
#ifndef IPANIC_USERSPACE_READ
	ipanic_ops = ops;
#endif
}

struct aee_oops *ipanic_oops_copy(void)
{
	if (ipanic_ops)
		return ipanic_ops->oops_copy();
	else
		return NULL;
}
EXPORT_SYMBOL(ipanic_oops_copy);

void ipanic_oops_free(struct aee_oops *oops, int erase)
{
	if (ipanic_ops)
		ipanic_ops->oops_free(oops, erase);
}
EXPORT_SYMBOL(ipanic_oops_free);

static int ipanic_alog_buffer(void *data, unsigned char *buffer, size_t sz_buf);

static int ipanic_current_task_info(void *data, unsigned char *buffer, size_t sz_buf)
{
	return mrdump_task_info(buffer, sz_buf);
}

/*#ifdef CONFIG_MTK_MMPROFILE_SUPPORT*/
#ifdef CONFIG_MMPROFILE
static int ipanic_mmprofile(void *data, unsigned char *buffer, size_t sz_buf)
{
	int errno = 0;
	static unsigned int index;
	static unsigned int mmprofile_dump_size;
	unsigned long pbuf = 0;
	unsigned int bufsize = 0;

	if (mmprofile_dump_size == 0) {
		mmprofile_dump_size = MMProfileGetDumpSize();
		if (mmprofile_dump_size == 0 || mmprofile_dump_size > IPANIC_MMPROFILE_LIMIT) {
			LOGE("%s: INVALID MMProfile size[%x]", __func__, mmprofile_dump_size);
			return -3;
		}
	}

	MMProfileGetDumpBuffer(index, (unsigned long *)&pbuf, &bufsize);
	if (bufsize == 0) {
		errno = 0;
	} else if (bufsize > sz_buf) {
		errno = -4;
	} else {
		memcpy(buffer, (void *)pbuf, bufsize);
		index += bufsize;
		errno = bufsize;
	}
	return errno;
}
#endif

const struct ipanic_dt_op ipanic_dt_ops[] = {
	{"IPANIC_HEADER", 0, NULL},
	{"SYS_KERNEL_LOG", __LOG_BUF_LEN, ipanic_klog_buffer},
	{"SYS_WDT_LOG", WDT_LOG_LEN, ipanic_klog_buffer},
	{"SYS_WQ_LOG", 0, NULL},
	{"reserved", 0, NULL},
	{"reserved", 0, NULL},
	{"PROC_CUR_TSK", sizeof(struct aee_process_info), ipanic_current_task_info},
	{"_exp_detail.txt", OOPS_LOG_LEN, ipanic_klog_buffer},
	{"SYS_MINI_RDUMP", MRDUMP_MINI_BUF_SIZE, NULL},	/* 8 */
/*#ifdef CONFIG_MTK_MMPROFILE_SUPPORT*/
#ifdef CONFIG_MMPROFILE
	{"SYS_MMPROFILE", IPANIC_MMPROFILE_LIMIT, ipanic_mmprofile},
#else
	{"SYS_MMPROFILE", 0, NULL},
#endif
	{"SYS_MAIN_LOG_RAW", __MAIN_BUF_SIZE, ipanic_alog_buffer},
	{"SYS_SYSTEM_LOG_RAW", __SYSTEM_BUF_SIZE, ipanic_alog_buffer},
	{"SYS_EVENTS_LOG_RAW", __EVENTS_BUF_SIZE, ipanic_alog_buffer},
	{"SYS_RADIO_LOG_RAW", __RADIO_BUF_SIZE, ipanic_alog_buffer},
	{"SYS_LAST_LOG", LAST_LOG_LEN, ipanic_klog_buffer},
	{"SYS_ATF_LOG", ATF_LOG_SIZE, ipanic_atflog_buffer},
	{"SYS_DISP_LOG", DISP_LOG_SIZE, panic_dump_disp_log},	/* 16 */
	{"reserved", 0, NULL},
	{"reserved", 0, NULL},
	{"reserved", 0, NULL},
	{"reserved", 0, NULL},
	{"reserved", 0, NULL},
	{"reserved", 0, NULL},
	{"reserved", 0, NULL},
	{"reserved", 0, NULL},	/* 24 */
	{"reserved", 0, NULL},
	{"reserved", 0, NULL},
	{"reserved", 0, NULL},
	{"reserved", 0, NULL},
	{"reserved", 0, NULL},
	{"reserved", 0, NULL},
	{"reserved", 0, NULL},
};

static const char IPANIC_DT_STR[][16] = {"XXXXXXXXX", "XXXXXXXXXX",
	"XXXXXXXX", "XXXXXXXXXX", "XXXXXXXXX" };
static const char IPANIC_ERR_MSG[][16] = { "unaligned", "blk alignment" };

static struct ipanic_header ipanic_hdr, *iheader;

/* data: indicate dump scope; buffer: dump to; sz_buf: buffer size;
   return: real size dumped */
static int ipanic_memory_buffer(void *data, unsigned char *buffer, size_t sz_buf)
{
	unsigned long sz_real;
	struct ipanic_memory_block *mem = (struct ipanic_memory_block *)data;
	unsigned long start = mem->kstart;
	unsigned long end = mem->kend;
	unsigned long pos = mem->pos;

	if (pos > end || pos < start)
		return -1;
	sz_real = (end - pos) > sz_buf ? sz_buf : (end - pos);
	memcpy(buffer, (void *)pos, sz_real);
	mem->pos += sz_real;
	return sz_real;
}

static int ipanic_alog_buffer(void *data, unsigned char *buffer, size_t sz_buf)
{
	int rc;

	rc = panic_dump_android_log(buffer, sz_buf, (unsigned long)data);
	if (rc < 0)
		rc = -1;
	return rc;
}

inline int ipanic_func_write(fn_next next, void *data, int off, int total, int encrypt)
{
	int errno = 0;
	size_t size;
	int start = off;
	struct ipanic_header *iheader = ipanic_header();
	unsigned char *ipanic_buffer = (unsigned char *)(unsigned long)iheader->buf;
	size_t sz_ipanic_buffer = iheader->bufsize;
	size_t blksize = iheader->blksize;
	int many = total > iheader->bufsize;

	LOGV("off[%x], encrypt[%d]\n", off, encrypt);

	if (off & (blksize - 1))
		return -2;	/*invalid offset, not block aligned */
	do {
		errno = next(data, ipanic_buffer, sz_ipanic_buffer);
		if (IS_ERR(ERR_PTR(errno)))
			break;
		size = (size_t) errno;
		if (size == 0)
			return (off - start);
		if ((off - start + size) > total) {
			LOGE("%s: data oversize(%zx>%x@%x)\n", __func__, off - start + size, total,
			     start);
			errno = -EFBIG;
			break;
		}
		if (encrypt)
			ipanic_block_scramble(ipanic_buffer, size);
		if (size != sz_ipanic_buffer)
			memset(ipanic_buffer + size, 0, sz_ipanic_buffer - size);
		LOGV("%x@%x\n", size, off);

		if (ipanic_enable)
			errno = ipanic_write_size(ipanic_buffer, off, ALIGN(size, blksize));
		else
			errno = -10;
		if (IS_ERR(ERR_PTR(errno)))
			break;
		off += size;
		if (many == 0)
			return size;
	} while (many);
	return errno;
}

inline int ipanic_next_write(fn_next next, void *data, int off, int total, int encrypt)
{
	return ipanic_func_write(next, data, off, total, encrypt);
}

inline int ipanic_mem_write(void *buf, int off, int len, int encrypt)
{
	struct ipanic_memory_block mem_info = {
		.kstart = (unsigned long)buf,
		.kend = (unsigned long)buf + len,
		.pos = (unsigned long)buf,
	};
	return ipanic_next_write(ipanic_memory_buffer, &mem_info, off, len, encrypt);
}

static int ipanic_header_to_sd(struct ipanic_data_header *header)
{
	int errno = 0;
	int first_write = 0;
	struct ipanic_header *ipanic_hdr = ipanic_header();

	if (!ipanic_hdr->datas)
		first_write = 1;
	if (header) {
		ipanic_hdr->datas |= (0x1 < header->type);
		header->valid = 1;
	}
	if (ipanic_hdr->dhblk == 0 || header == NULL || first_write == 1)
		errno = ipanic_mem_write(ipanic_hdr, 0, sizeof(struct ipanic_header), 0);
	if (ipanic_hdr->dhblk && header)
		errno =
		    ipanic_mem_write(header, header->offset - ipanic_hdr->dhblk,
				     sizeof(struct ipanic_data_header), 0);

	if (IS_ERR(ERR_PTR(errno)))
		LOGW("%s: failed[%x-%d]\n", __func__, header ? header->type : 0, errno);
	return errno;
}

static int ipanic_data_is_valid(int dt)
{
	struct ipanic_header *ipanic_hdr = ipanic_header();
	struct ipanic_data_header *dheader = &ipanic_hdr->data_hdr[dt];

	return (dheader->valid == 1);
}

int ipanic_data_to_sd(int dt, void *data)
{
	int errno = 0;
	int (*next)(void *data, unsigned char *buffer, size_t sz_buf);
	struct ipanic_header *ipanic_hdr = ipanic_header();
	struct ipanic_data_header *dheader = &ipanic_hdr->data_hdr[dt];

	if (!ipanic_dt_active(dt) || dheader->valid == 1)
		return -4;

	next = ipanic_dt_ops[dt].next;
	if (next == NULL) {
		errno = -3;
	} else {
		errno =
		    ipanic_next_write(next, data, dheader->offset, dheader->total,
				      dheader->encrypt);
	}
	if (IS_ERR(ERR_PTR(errno))) {
		LOGW("%s: dump %s failed[%d]\n", __func__, dheader->name, errno);
		if (errno == -EFBIG)
			dheader->used = dheader->total;
		else
			return errno;
	} else {
		dheader->used = (size_t) errno;
	}
	ipanic_header_to_sd(dheader);
	return errno;
}

void ipanic_mrdump_mini(AEE_REBOOT_MODE reboot_mode, const char *msg, ...)
{
	int ret;
	struct ipanic_header *ipanic_hdr;
	loff_t sd_offset;
	struct ipanic_data_header *dheader;
	va_list ap;
	/* write sd is unreliable, so gen mrdump header first */
	if (ipanic_data_is_valid(IPANIC_DT_MINI_RDUMP))
		return;

	va_start(ap, msg);
	ipanic_hdr = ipanic_header();
	sd_offset = ipanic_hdr->data_hdr[IPANIC_DT_MINI_RDUMP].offset;
	dheader = &ipanic_hdr->data_hdr[IPANIC_DT_MINI_RDUMP];
	ret = mrdump_mini_create_oops_dump(reboot_mode, ipanic_mem_write, sd_offset, msg, ap);
	va_end(ap);
	if (!IS_ERR(ERR_PTR(ret))) {
		dheader->used = ret;
		ipanic_header_to_sd(dheader);
	}
}

void *ipanic_data_from_sd(struct ipanic_data_header *dheader, int encrypt)
{
	void *data;

	data = expdb_read_size(dheader->offset, dheader->used);
	/* data = ipanic_read_size(dheader->offset, dheader->used); */
	if (data != 0 && encrypt != 0)
		ipanic_block_scramble((unsigned char *)data, dheader->used);
	return data;
}

struct ipanic_header *ipanic_header_from_sd(unsigned int offset, unsigned int magic)
{
	struct ipanic_data_header *dheader;
	int dt;
	char str[256];
	size_t size = 0;
	struct ipanic_header *header;
	struct ipanic_data_header dheader_header = {
		.type = IPANIC_DT_HEADER,
		.offset = offset,
		.used = sizeof(struct ipanic_header),
	};
	header = (struct ipanic_header *)ipanic_data_from_sd(&dheader_header, 0);
	if (IS_ERR_OR_NULL((void *)header)) {
		LOGD("read header failed[%ld]\n", PTR_ERR((void *)header));
		header = NULL;
	} else if (header->magic != magic) {
		LOGD("no ipanic data[%x]\n", header->magic);
		kfree(header);
		header = NULL;
		ipanic_erase();
	} else {
		for (dt = IPANIC_DT_HEADER + 1; dt < IPANIC_DT_RESERVED31; dt++) {
			dheader = &header->data_hdr[dt];
			if (dheader->valid) {
				size += snprintf(str + size, 256 - size, "%s[%x@%x],",
						 dheader->name, dheader->used, dheader->offset);
			}
		}
		LOGD("ipanic data available^v^%s^v^\n", str);
	}
	return header;
}

struct aee_oops *ipanic_oops_from_sd(void)
{
	struct aee_oops *oops = NULL;
	struct ipanic_header *hdr = NULL;
	struct ipanic_data_header *dheader;
	char *data;
	int i;

	hdr = ipanic_header_from_sd(0, AEE_IPANIC_MAGIC);
	if (hdr == NULL)
		return NULL;

	oops = aee_oops_create(AE_DEFECT_FATAL, AE_KE, IPANIC_MODULE_TAG);
	if (oops == NULL) {
		LOGE("%s: can not allocate buffer\n", __func__);
		return NULL;
	}

	for (i = IPANIC_DT_HEADER + 1; i < IPANIC_DT_RESERVED31; i++) {
		dheader = &hdr->data_hdr[i];
		if (dheader->valid == 0)
			continue;
		data = ipanic_data_from_sd(dheader, 1);
		if (data) {
			switch (i) {
			case IPANIC_DT_KERNEL_LOG:
				oops->console = data;
				oops->console_len = dheader->used;
				break;
			case IPANIC_DT_MINI_RDUMP:
				oops->mini_rdump = data;
				oops->mini_rdump_len = dheader->used;
				break;
			case IPANIC_DT_MAIN_LOG:
				oops->android_main = data;
				oops->android_main_len = dheader->used;
				break;
			case IPANIC_DT_SYSTEM_LOG:
				oops->android_system = data;
				oops->android_system_len = dheader->used;
				break;
			case IPANIC_DT_EVENTS_LOG:
				/* Todo .. */
				break;
			case IPANIC_DT_RADIO_LOG:
				oops->android_radio = data;
				oops->android_radio_len = dheader->used;
				break;
			case IPANIC_DT_CURRENT_TSK:
				memcpy(oops->process_path, data, AEE_PROCESS_NAME_LENGTH - 1);
				break;
			case IPANIC_DT_MMPROFILE:
				oops->mmprofile = data;
				oops->mmprofile_len = dheader->used;
				break;
			default:
				LOGI("%s: [%d] NOT USED.\n", __func__, i);
			}
		} else {
			LOGW("%s: read %s failed, %x@%x\n", __func__,
			     dheader->name, dheader->used, dheader->offset);
		}
	}
	return oops;
}

int ipanic(struct notifier_block *this, unsigned long event, void *ptr)
{
	struct ipanic_data_header *dheader;
	struct kmsg_dumper dumper;
	struct ipanic_atf_log_rec atf_log = { ATF_LOG_SIZE, 0, 0 };
	void *data = NULL;
	int dt;
	int errno;
	struct ipanic_header *ipanic_hdr;

	memset(&dumper, 0x0, sizeof(struct kmsg_dumper));
	aee_rr_rec_fiq_step(AEE_FIQ_STEP_KE_IPANIC_START);
	aee_rr_rec_exp_type(2);
	bust_spinlocks(1);
	spin_lock_irq(&ipanic_lock);
	aee_disable_api();
	mrdump_mini_ke_cpu_regs(NULL);
	flush_cache_all();
	if (!has_mt_dump_support())
		emergency_restart();
	ipanic_mrdump_mini(AEE_REBOOT_MODE_KERNEL_PANIC, "kernel PANIC");
	if (!ipanic_data_is_valid(IPANIC_DT_KERNEL_LOG)) {
		ipanic_klog_region(&dumper);
		errno = ipanic_data_to_sd(IPANIC_DT_KERNEL_LOG, &dumper);
		if (errno == -1)
			aee_nested_printf("$");
	}
	ipanic_klog_region(&dumper);
	errno = ipanic_data_to_sd(IPANIC_DT_OOPS_LOG, &dumper);
	if (errno == -1)
		aee_nested_printf("$");
	ipanic_data_to_sd(IPANIC_DT_CURRENT_TSK, 0);
	/* kick wdt after save the most critical infos */
	ipanic_kick_wdt();
	ipanic_data_to_sd(IPANIC_DT_MAIN_LOG, (void *)1);
	ipanic_data_to_sd(IPANIC_DT_SYSTEM_LOG, (void *)4);
	ipanic_data_to_sd(IPANIC_DT_EVENTS_LOG, (void *)2);
	ipanic_data_to_sd(IPANIC_DT_RADIO_LOG, (void *)3);
	aee_wdt_dump_info();
	ipanic_klog_region(&dumper);
	ipanic_data_to_sd(IPANIC_DT_WDT_LOG, &dumper);
#ifdef CONFIG_MTK_WQ_DEBUG
	wq_debug_dump();
#endif
	ipanic_klog_region(&dumper);
	ipanic_data_to_sd(IPANIC_DT_WQ_LOG, &dumper);
	ipanic_data_to_sd(IPANIC_DT_MMPROFILE, 0);
	ipanic_data_to_sd(IPANIC_DT_ATF_LOG, &atf_log);
	ipanic_data_to_sd(IPANIC_DT_DISP_LOG, data);
	errno = ipanic_header_to_sd(0);
	if (!IS_ERR(ERR_PTR(errno)))
		mrdump_mini_ipanic_done();
	ipanic_klog_region(&dumper);
	ipanic_data_to_sd(IPANIC_DT_LAST_LOG, &dumper);
	LOGD("ipanic done^_^");
	ipanic_hdr = ipanic_header();
	for (dt = IPANIC_DT_HEADER + 1; dt < IPANIC_DT_RESERVED31; dt++) {
		dheader = &ipanic_hdr->data_hdr[dt];
		if (dheader->valid)
			LOGD("%s[%x@%x],", dheader->name, dheader->used, dheader->offset);
	}
	LOGD("^_^\n");
	aee_rr_rec_fiq_step(AEE_FIQ_STEP_KE_IPANIC_DONE);

	return NOTIFY_DONE;
}

void ipanic_recursive_ke(struct pt_regs *regs, struct pt_regs *excp_regs, int cpu)
{
	int errno;
	struct kmsg_dumper dumper;

	aee_nested_printf("minidump\n");
	aee_rr_rec_exp_type(3);
	bust_spinlocks(1);
	flush_cache_all();
#ifdef __aarch64__
	cpu_cache_off();
#else
	cpu_proc_fin();
#endif
	mrdump_mini_ke_cpu_regs(excp_regs);
	mrdump_mini_per_cpu_regs(cpu, regs);
	flush_cache_all();
	if (!has_mt_dump_support())
		emergency_restart();
	ipanic_mrdump_mini(AEE_REBOOT_MODE_NESTED_EXCEPTION, "Nested Panic");

	ipanic_data_to_sd(IPANIC_DT_CURRENT_TSK, 0);
	ipanic_kick_wdt();
	memset(&dumper, 0x0, sizeof(struct kmsg_dumper));
	ipanic_klog_region(&dumper);
	ipanic_data_to_sd(IPANIC_DT_KERNEL_LOG, &dumper);
	errno = ipanic_header_to_sd(0);
	if (!IS_ERR(ERR_PTR(errno)))
		mrdump_mini_ipanic_done();
	bust_spinlocks(0);
}
EXPORT_SYMBOL(ipanic_recursive_ke);

struct ipanic_header *ipanic_header(void)
{
	int i;
	struct ipanic_data_header *dheader;
	int next_offset;

	if (iheader)
		return iheader;
	iheader = &ipanic_hdr;
	iheader->magic = AEE_IPANIC_MAGIC;
	iheader->version = AEE_IPANIC_PHDR_VERSION;
	if (ipanic_msdc_info(iheader)) {
		LOGE("ipanic initialize msdc fail.");
		aee_nested_printf("$");
		return NULL;
	}
	iheader->size = sizeof(struct ipanic_header);
	iheader->datas = 0;
#if 1
	iheader->dhblk = ALIGN(sizeof(struct ipanic_data_header), iheader->blksize);
#else
	iheader->dhblk = 0;
#endif
	next_offset = ALIGN(sizeof(struct ipanic_header), iheader->blksize);
	for (i = IPANIC_DT_HEADER + 1; i < IPANIC_DT_RESERVED31; i++) {
		dheader = &iheader->data_hdr[i];
		dheader->type = i;
		dheader->valid = 0;
		dheader->used = 0;
		strncpy(dheader->name, ipanic_dt_ops[i].string, 31);
		if (ipanic_dt_active(i) && ipanic_dt_ops[i].size) {
			dheader->encrypt = ipanic_dt_encrypt(i);
			dheader->offset = next_offset + iheader->dhblk;
			dheader->total = ALIGN(ipanic_dt_ops[i].size, iheader->blksize);
			if (iheader->partsize < (dheader->offset + dheader->total)) {
				LOGW("skip %s[%x@%x>%x]\n", dheader->name, dheader->total,
				     dheader->offset, iheader->partsize);
				dheader->offset = INT_MAX;
				dheader->total = 0;
				continue;
			}
			next_offset += dheader->total + iheader->dhblk;
		} else {
			dheader->offset = INT_MAX;
			dheader->total = 0;
		}
	}
	return iheader;
}
EXPORT_SYMBOL(ipanic_header);

static void ipanic_oops_done(struct aee_oops *oops, int erase)
{
	if (oops)
		aee_oops_free(oops);
	if (erase)
		ipanic_erase();
}

static int ipanic_die(struct notifier_block *self, unsigned long cmd, void *ptr)
{
	struct kmsg_dumper dumper;
	struct die_args *dargs = (struct die_args *)ptr;

	aee_rr_rec_exp_type(2);
	aee_rr_rec_fiq_step(AEE_FIQ_STEP_KE_IPANIC_DIE);
	aee_disable_api();

	if (aee_rr_curr_exp_type() == 1)
		__mrdump_create_oops_dump(AEE_REBOOT_MODE_WDT, dargs->regs, "WDT/HWT");
	else
		__mrdump_create_oops_dump(AEE_REBOOT_MODE_KERNEL_OOPS, dargs->regs, "Kernel Oops");

	rtc_mark_kernel_panic();

	__show_regs(dargs->regs);
	dump_stack();
	aee_rr_rec_scp();
#ifdef CONFIG_SCHED_DEBUG
	if (aee_rr_curr_exp_type() == 1)
		sysrq_sched_debug_show_at_AEE();
#endif
#ifdef CONFIG_MTK_WQ_DEBUG
	wq_debug_dump();
#endif

	mrdump_mini_ke_cpu_regs(dargs->regs);
	__disable_dcache__inner_flush_dcache_L1__inner_flush_dcache_L2();

#if defined(CONFIG_MTK_MLC_NAND_SUPPORT) || defined(CONFIG_MTK_TLC_NAND_SUPPORT)
	LOGE("MLC/TLC project, disable ipanic flow\n");
	ipanic_enable = 0; /*for mlc/tlc nand project, only enable lk flow*/
#endif

	if (!has_mt_dump_support())
		emergency_restart();

	ipanic_mrdump_mini(AEE_REBOOT_MODE_KERNEL_PANIC, "kernel Oops");
	memset(&dumper, 0x0, sizeof(struct kmsg_dumper));
	ipanic_klog_region(&dumper);
	ipanic_data_to_sd(IPANIC_DT_KERNEL_LOG, &dumper);
	ipanic_data_to_sd(IPANIC_DT_CURRENT_TSK, dargs->regs);
	return NOTIFY_DONE;
}

static struct notifier_block panic_blk = {
	.notifier_call = ipanic,
};

static struct ipanic_ops ipanic_oops_ops = {
	.oops_copy = ipanic_oops_from_sd,
	.oops_free = ipanic_oops_done,
};

static struct notifier_block die_blk = {
	.notifier_call = ipanic_die,
};

int __init aee_ipanic_init(void)
{
	spin_lock_init(&ipanic_lock);

	mrdump_init();

	atomic_notifier_chain_register(&panic_notifier_list, &panic_blk);
	register_die_notifier(&die_blk);
	register_ipanic_ops(&ipanic_oops_ops);
	ipanic_log_temp_init();
	ipanic_msdc_init();
	LOGI("ipanic: startup, partition assgined %s\n", AEE_IPANIC_PLABEL);
	return 0;
}

arch_initcall(aee_ipanic_init);

module_param(ipanic_enable, bool, S_IRUGO | S_IWUSR);
