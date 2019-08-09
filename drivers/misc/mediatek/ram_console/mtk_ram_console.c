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

#include <linux/slab.h>
#include <linux/atomic.h>
#include <linux/console.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/kthread.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_reserved_mem.h>
#include <linux/pstore.h>
#include <linux/io.h>
#include <mt-plat/aee.h>
#include "ram_console.h"
#include <mach/memory_layout.h>

#define RAM_CONSOLE_HEADER_STR_LEN 1024

#define THERMAL_RESERVED_TZS (10)
static int thermal_num = THERMAL_RESERVED_TZS;

static int mtk_cpu_num;

static int ram_console_init_done;
static unsigned int old_wdt_status;
static int ram_console_clear;
static bool reserve_mem_fail;

/*
 *  This group of API call by sub-driver module to report reboot reasons
 *  aee_rr_* stand for previous reboot reason
 */
struct last_reboot_reason {
	uint32_t fiq_step;
	/* 0xaeedeadX: X=1 (HWT), X=2 (KE), X=3 (nested panic) */
	uint32_t exp_type;
	uint64_t kaslr_offset;
	uint64_t ram_console_buffer_addr;

	uint32_t last_irq_enter[AEE_MTK_CPU_NUMS];
	uint64_t jiffies_last_irq_enter[AEE_MTK_CPU_NUMS];

	uint32_t last_irq_exit[AEE_MTK_CPU_NUMS];
	uint64_t jiffies_last_irq_exit[AEE_MTK_CPU_NUMS];

	uint8_t hotplug_footprint[AEE_MTK_CPU_NUMS];
	uint8_t hotplug_cpu_event;
	uint8_t hotplug_cb_index;
	uint64_t hotplug_cb_fp;
	uint64_t hotplug_cb_times;
	uint64_t hps_cb_enter_times;
	uint32_t hps_cb_cpu_bitmask;
	uint32_t hps_cb_footprint;
	uint64_t hps_cb_fp_times;
	uint32_t cpu_caller;
	uint32_t cpu_callee;
	uint64_t cpu_up_prepare_ktime;
	uint64_t cpu_starting_ktime;
	uint64_t cpu_online_ktime;
	uint64_t cpu_down_prepare_ktime;
	uint64_t cpu_dying_ktime;
	uint64_t cpu_dead_ktime;
	uint64_t cpu_post_dead_ktime;

	uint32_t mcdi_wfi;
	uint32_t mcdi_r15;
	uint32_t deepidle_data;
	uint32_t sodi3_data;
	uint32_t sodi_data;
	uint32_t mcsodi_data;
	uint32_t spm_suspend_data;
	uint32_t spm_common_scenario_data;
	uint32_t mtk_cpuidle_footprint[AEE_MTK_CPU_NUMS];
	uint32_t mcdi_footprint[AEE_MTK_CPU_NUMS];
	uint32_t clk_data[8];
	uint32_t suspend_debug_flag;
	uint32_t fiq_cache_step;

	uint32_t vcore_dvfs_opp;
	uint32_t vcore_dvfs_status;

	uint32_t ppm_cluster_limit[8];
	uint8_t ppm_step;
	uint8_t ppm_cur_state;
	uint32_t ppm_min_pwr_bgt;
	uint32_t ppm_policy_mask;
	uint8_t ppm_waiting_for_pbm;

	uint8_t cpu_dvfs_vproc_big;
	uint8_t cpu_dvfs_vproc_little;
	uint8_t cpu_dvfs_oppidx;
	uint8_t cpu_dvfs_cci_oppidx;
	uint8_t cpu_dvfs_status;
	uint8_t cpu_dvfs_step;
	uint8_t cpu_dvfs_pbm_step;
	uint8_t cpu_dvfs_cb;
	uint8_t cpufreq_cb;

	uint8_t gpu_dvfs_vgpu;
	uint8_t gpu_dvfs_oppidx;
	uint8_t gpu_dvfs_status;

	uint32_t drcc_0;
	uint32_t drcc_1;
	uint32_t drcc_2;
	uint32_t drcc_3;
	uint32_t drcc_dbg_ret;
	uint32_t drcc_dbg_off;
	uint64_t drcc_dbg_ts;
	uint32_t ptp_devinfo_0;
	uint32_t ptp_devinfo_1;
	uint32_t ptp_devinfo_2;
	uint32_t ptp_devinfo_3;
	uint32_t ptp_devinfo_4;
	uint32_t ptp_devinfo_5;
	uint32_t ptp_devinfo_6;
	uint32_t ptp_devinfo_7;
	uint32_t ptp_e0;
	uint32_t ptp_e1;
	uint32_t ptp_e2;
	uint32_t ptp_e3;
	uint32_t ptp_e4;
	uint32_t ptp_e5;
	uint32_t ptp_e6;
	uint32_t ptp_e7;
	uint32_t ptp_e8;
	uint32_t ptp_e9;
	uint32_t ptp_e10;
	uint32_t ptp_e11;
	uint64_t ptp_vboot;
	uint64_t ptp_cpu_big_volt;
	uint64_t ptp_cpu_big_volt_1;
	uint64_t ptp_cpu_big_volt_2;
	uint64_t ptp_cpu_big_volt_3;
	uint64_t ptp_cpu_2_little_volt;
	uint64_t ptp_cpu_2_little_volt_1;
	uint64_t ptp_cpu_2_little_volt_2;
	uint64_t ptp_cpu_2_little_volt_3;
	uint64_t ptp_cpu_little_volt;
	uint64_t ptp_cpu_little_volt_1;
	uint64_t ptp_cpu_little_volt_2;
	uint64_t ptp_cpu_little_volt_3;
	uint64_t ptp_cpu_cci_volt;
	uint64_t ptp_cpu_cci_volt_1;
	uint64_t ptp_cpu_cci_volt_2;
	uint64_t ptp_cpu_cci_volt_3;
	uint64_t ptp_gpu_volt;
	uint64_t ptp_gpu_volt_1;
	uint64_t ptp_gpu_volt_2;
	uint64_t ptp_gpu_volt_3;
	uint64_t ptp_temp;
	uint8_t ptp_status;
	uint8_t eem_pi_offset;
	uint8_t etc_status;
	uint8_t etc_mode;


	int8_t thermal_temp[THERMAL_RESERVED_TZS];
	uint8_t thermal_status;
	uint8_t thermal_ATM_status;
	uint64_t thermal_ktime;

	uint32_t idvfs_ctrl_reg;
	uint32_t idvfs_enable_cnt;
	uint32_t idvfs_swreq_cnt;
	uint16_t idvfs_curr_volt;
	uint16_t idvfs_sram_ldo;
	uint16_t idvfs_swavg_curr_pct_x100;
	uint16_t idvfs_swreq_curr_pct_x100;
	uint16_t idvfs_swreq_next_pct_x100;
	uint8_t idvfs_state_manchine;

	uint32_t ocp_target_limit[4];
	uint8_t ocp_enable;
	uint32_t scp_pc;
	uint32_t scp_lr;
	unsigned long last_init_func;
	uint8_t pmic_ext_buck;
	uint32_t hang_detect_timeout_count;
	unsigned long last_async_func;
	unsigned long last_sync_func;
	uint32_t gz_irq;
};

struct reboot_reason_pl {
	u32 wdt_status;
	u32 data[0];
};

struct reboot_reason_lk {
	u32 data[0];
};

struct ram_console_buffer {
	uint32_t sig;
	/* for size comptible */
	uint32_t off_pl;
	uint32_t off_lpl;	/* last preloader: struct reboot_reason_pl */
	uint32_t sz_pl;
	uint32_t off_lk;
	uint32_t off_llk;	/* last lk: struct reboot_reason_lk */
	uint32_t sz_lk;
	uint32_t padding[3];
	uint32_t sz_buffer;
	uint32_t off_linux;	/* struct last_reboot_reason */
	uint32_t off_console;

	/* console buffer */
	uint32_t log_start;
	uint32_t log_size;
	uint32_t sz_console;
};

#define REBOOT_REASON_SIG (0x43474244)	/* DBRR */
static int FIQ_log_size = sizeof(struct ram_console_buffer);

static struct ram_console_buffer *ram_console_buffer;
static struct ram_console_buffer *ram_console_old;
static struct ram_console_buffer *ram_console_buffer_pa;

static DEFINE_SPINLOCK(ram_console_lock);

static atomic_t rc_in_fiq = ATOMIC_INIT(0);

static void ram_console_init_val(void);

#include "desc/desc_s.h"
void aee_rr_get_desc_info(unsigned long *addr, unsigned long *size,
		unsigned long *start)
{
	if (addr == NULL || size == NULL || start == NULL)
		return;
	*addr = IDESC_ADDR;
	*size = IDESC_SIZE;
	*start = IDESC_START_POS;
}

#ifdef __aarch64__
static void *_memcpy(void *dest, const void *src, size_t count)
{
	char *tmp = dest;
	const char *s = src;

	while (count--)
		*tmp++ = *s++;
	return dest;
}

#define memcpy _memcpy
#endif

#define LAST_RR_SEC_VAL(header, sect, type, item) \
	(header->off_##sect ? \
	((type *)((void *)header + header->off_##sect))->item : 0)
#define LAST_RRR_BUF_VAL(buf, rr_item)	\
	LAST_RR_SEC_VAL(buf, linux, struct last_reboot_reason, rr_item)
#define LAST_RRPL_BUF_VAL(buf, rr_item)	\
	LAST_RR_SEC_VAL(buf, pl, struct reboot_reason_pl, rr_item)
#define LAST_RRR_VAL(rr_item)	\
	LAST_RR_SEC_VAL(ram_console_old, linux, struct last_reboot_reason, \
			rr_item)
#define LAST_RRPL_VAL(rr_item)	\
	LAST_RR_SEC_VAL(ram_console_old, pl, struct reboot_reason_pl, rr_item)

unsigned int ram_console_size(void)
{
	return ram_console_buffer->sz_console;
}

#ifdef CONFIG_PSTORE
void __weak pstore_bconsole_write(struct console *con, const char *s,
		unsigned int c)
{
}

void sram_log_save(const char *msg, int count)
{
	pstore_bconsole_write(NULL, msg, count);
}

void pstore_console_show(enum pstore_type_id type_id, struct seq_file *m,
		void *v)
{
	struct pstore_info *psi = psinfo;
	char *buf = NULL;
	ssize_t size;
	u64 id;
	int count;
	enum pstore_type_id type;
	struct timespec time;
	bool compressed;
	ssize_t ecc_notice_size = 0;

	if (!psi)
		return;
	mutex_lock(&psi->read_mutex);
	if (psi->open && psi->open(psi))
		goto out;

	while ((size = psi->read(&id, &type, &count, &time, &buf, &compressed,
					&ecc_notice_size, psi)) > 0) {
		if (type == type_id)
			seq_write(m, buf, size);
		kfree(buf);
		buf = NULL;
	}

	if (psi->close)
		psi->close(psi);
out:
	mutex_unlock(&psi->read_mutex);
}
#else
void sram_log_save(const char *msg, int count)
{
	struct ram_console_buffer *buffer;
	char *rc_console;
	int rem;
	unsigned int ram_console_buffer_size = ram_console_size();

	if (ram_console_buffer == NULL) {
		pr_notice("ram console buffer is NULL!\n");
		return;
	}

	buffer = ram_console_buffer;
	rc_console = (char *)ram_console_buffer +
		ram_console_buffer->off_console;

	/* count >= buffer_size, full the buffer */
	if (count >= ram_console_buffer_size) {
		memcpy(rc_console, msg + (count - ram_console_buffer_size),
		       ram_console_buffer_size);
		buffer->log_start = 0;
		buffer->log_size = ram_console_buffer_size;
	} else if (count > (ram_console_buffer_size - buffer->log_start)) {
		/* count > last buffer, full them and fill the head buffer */
		rem = ram_console_buffer_size - buffer->log_start;
		memcpy(rc_console + buffer->log_start, msg, rem);
		memcpy(rc_console, msg + rem, count - rem);
		buffer->log_start = count - rem;
		buffer->log_size = ram_console_buffer_size;
	} else {
		/* count <=  last buffer, fill in free buffer */
		memcpy(rc_console + buffer->log_start, msg, count);
		buffer->log_start += count;
		buffer->log_size += count;
		if (buffer->log_start >= ram_console_buffer_size)
			buffer->log_start = 0;
		if (buffer->log_size > ram_console_buffer_size)
			buffer->log_size = ram_console_buffer_size;
	}

}
#endif

#ifdef __aarch64__
#define FORMAT_LONG "%016lx "
#else
#define FORMAT_LONG "%08lx "
#endif
void aee_sram_fiq_save_bin(const char *msg, size_t len)
{
	int i;
	char buf[20];

	for (i = 0; i < len;) {
		snprintf(buf, sizeof(long) * 2 + 2,
				FORMAT_LONG, *(long *)(msg + i));
		sram_log_save(buf, sizeof(long) * 2 + 1);
		i += sizeof(long);
		if (i % 32 == 0)
			sram_log_save("\n", 1);
	}
}

void aee_disable_ram_console_write(void)
{
	atomic_set(&rc_in_fiq, 1);
}

void aee_sram_fiq_log(const char *msg)
{
	unsigned int count = strlen(msg);
	int delay = 100;
#ifndef CONFIG_PSTORE
	unsigned int ram_console_buffer_size = ram_console_size();

	if (FIQ_log_size + count > ram_console_buffer_size)
		return;
#endif

	atomic_set(&rc_in_fiq, 1);

	while ((delay > 0) && (spin_is_locked(&ram_console_lock))) {
		udelay(1);
		delay--;
	}

	sram_log_save(msg, count);
	FIQ_log_size += count;
}

void ram_console_write(struct console *console, const char *s,
		unsigned int count)
{
	unsigned long flags;

	if (atomic_read(&rc_in_fiq))
		return;

	spin_lock_irqsave(&ram_console_lock, flags);

	sram_log_save(s, count);

	spin_unlock_irqrestore(&ram_console_lock, flags);
}

static struct console ram_console = {
	.name = "ram",
	.write = ram_console_write,
	.flags = CON_PRINTBUFFER | CON_ENABLED | CON_ANYTIME,
	.index = -1,
};

void ram_console_enable_console(int enabled)
{
	if (enabled)
		ram_console.flags |= CON_ENABLED;
	else
		ram_console.flags &= ~CON_ENABLED;
}

static int ram_console_check_header(struct ram_console_buffer *buffer)
{
	if (!buffer || (buffer->sz_buffer != ram_console_buffer->sz_buffer)
		|| buffer->off_pl > buffer->sz_buffer
		|| buffer->off_lk > buffer->sz_buffer
		|| buffer->off_linux > buffer->sz_buffer
		|| buffer->off_console > buffer->sz_buffer
		|| buffer->off_pl + ALIGN(buffer->sz_pl, 64) != buffer->off_lpl
		|| buffer->off_lk + ALIGN(buffer->sz_lk, 64)
			!= buffer->off_llk) {
		pr_notice("ram_console: ilegal header.");
		return -1;
	} else
		return 0;
}

static int ram_console_lastk_show(struct ram_console_buffer *buffer,
		struct seq_file *m, void *v)
{
	unsigned int wdt_status;

	if (!buffer) {
		pr_notice("ram_console: buffer is null\n");
		seq_puts(m, "buffer is null.\n");
		return 0;
	}

	if (ram_console_check_header(buffer) && buffer->sz_buffer != 0) {
		pr_notice("ram_console: buffer %p, size %x(%x)\n",
			buffer, buffer->sz_buffer,
			ram_console_buffer->sz_buffer);
		seq_write(m, buffer, ram_console_buffer->sz_buffer);
		return 0;
	}
	if (buffer->off_pl == 0 || buffer->off_pl + ALIGN(buffer->sz_pl, 64)
			!= buffer->off_lpl) {
		/* workaround for compatibility to old preloader & lk (OTA) */
		wdt_status = *((unsigned char *)buffer + 12);
	} else
		wdt_status = LAST_RRPL_BUF_VAL(buffer, wdt_status);

	seq_printf(m, "ram console header, hw_status: %u, fiq step %u.\n",
		   wdt_status, LAST_RRR_BUF_VAL(buffer, fiq_step));
	seq_printf(m, "%s, old status is %u.\n",
			ram_console_clear ?
			"Clear" : "Not Clear", old_wdt_status);

#ifdef CONFIG_PSTORE_CONSOLE
	pstore_console_show(PSTORE_TYPE_CONSOLE, m, v);
#else
	if (buffer->off_console != 0
	    && buffer->off_linux + ALIGN(sizeof(struct last_reboot_reason),
					 64) == buffer->off_console
	    && buffer->sz_console == buffer->sz_buffer - buffer->off_console
	    && buffer->log_size <= buffer->sz_console
	    && buffer->log_start <= buffer->sz_console) {
		seq_write(m, (void *)buffer + buffer->off_console +
				buffer->log_start,
				buffer->log_size - buffer->log_start);
		seq_write(m, (void *)buffer + buffer->off_console,
				buffer->log_start);
	} else {
		seq_puts(m,
			"header may be corrupted, dump the raw buffer for reference only\n");
		seq_write(m, buffer, ram_console_buffer->sz_buffer);
	}
#endif
	return 0;
}

static void aee_rr_show_in_log(void)
{
	if (ram_console_check_header(ram_console_old))
		pr_notice("ram_console: no valid data\n");
	else {
		pr_notice("pmic & external buck: 0x%x\n",
				LAST_RRR_VAL(pmic_ext_buck));
		pr_notice("ram_console: CPU notifier status: %d, %d, 0x%llx, %llu\n",
				LAST_RRR_VAL(hotplug_cpu_event),
				LAST_RRR_VAL(hotplug_cb_index),
				LAST_RRR_VAL(hotplug_cb_fp),
				LAST_RRR_VAL(hotplug_cb_times));
		pr_notice("ram_console: CPU HPS footprint: %llu, 0x%x, %d, %llu\n",
				LAST_RRR_VAL(hps_cb_enter_times),
				LAST_RRR_VAL(hps_cb_cpu_bitmask),
				LAST_RRR_VAL(hps_cb_footprint),
				LAST_RRR_VAL(hps_cb_fp_times));
		pr_notice("ram_console: last init function: 0x%lx\n",
				LAST_RRR_VAL(last_init_func));
	}
}

static int __init ram_console_save_old(struct ram_console_buffer *buffer,
		size_t buffer_size)
{
	ram_console_old = kmalloc(buffer_size, GFP_KERNEL);
	if (ram_console_old == NULL) {
		pr_notice("ram_console: failed to allocate old buffer\n");
		return -1;
	}
	memcpy(ram_console_old, buffer, buffer_size);
	aee_rr_show_in_log();
	return 0;
}

static int __init ram_console_init(struct ram_console_buffer *buffer,
		size_t buffer_size)
{
	ram_console_buffer = buffer;
	buffer->sz_buffer = buffer_size;

	if (buffer->sig != REBOOT_REASON_SIG  ||
			ram_console_check_header(buffer)) {
		memset_io((void *)buffer, 0, buffer_size);
		buffer->sig = REBOOT_REASON_SIG;
		ram_console_clear = 1;
	} else {
		old_wdt_status = LAST_RRPL_BUF_VAL(buffer, wdt_status);
	}
	ram_console_save_old(buffer, buffer_size);
	if (buffer->sz_lk != 0 && buffer->off_lk + ALIGN(buffer->sz_lk, 64) ==
			buffer->off_llk)
		buffer->off_linux = buffer->off_llk + ALIGN(buffer->sz_lk, 64);
	else
		/* OTA:leave enough space for pl/lk */
		buffer->off_linux = 512;
	buffer->sz_buffer = buffer_size;
	buffer->off_console = buffer->off_linux +
		ALIGN(sizeof(struct last_reboot_reason), 64);
	buffer->sz_console = buffer->sz_buffer - buffer->off_console;
	buffer->log_start = 0;
	buffer->log_size = 0;
	memset_io((void *)buffer + buffer->off_linux, 0,
			buffer_size - buffer->off_linux);
	ram_console_init_desc(buffer->off_linux);
#ifndef CONFIG_PSTORE
	register_console(&ram_console);
#endif
	ram_console_init_val();
	ram_console_init_done = 1;
	return 0;
}

#if defined(CONFIG_MTK_RAM_CONSOLE_USING_DRAM)
static void *remap_lowmem(phys_addr_t start, phys_addr_t size)
{
	struct page **pages;
	phys_addr_t page_start;
	unsigned int page_count;
	pgprot_t prot;
	unsigned int i;
	void *vaddr;

	page_start = start - offset_in_page(start);
	page_count = DIV_ROUND_UP(size + offset_in_page(start), PAGE_SIZE);

	prot = pgprot_noncached(PAGE_KERNEL);

	pages = kmalloc_array(page_count, sizeof(struct page *), GFP_KERNEL);
	if (!pages)
		return NULL;

	for (i = 0; i < page_count; i++) {
		phys_addr_t addr = page_start + i * PAGE_SIZE;

		pages[i] = pfn_to_page(addr >> PAGE_SHIFT);
	}
	vaddr = vmap(pages, page_count, VM_MAP, prot);
	kfree(pages);
	if (!vaddr) {
		pr_notice("%s: Failed to map %u pages\n", __func__, page_count);
		return NULL;
	}

	return vaddr + offset_in_page(start);
}
#endif

struct mem_desc_t {
	unsigned long start;
	unsigned long size;
};

#if defined(CONFIG_MTK_RAM_CONSOLE_USING_SRAM)
#ifdef CONFIG_OF
static int __init dt_get_ram_console(unsigned long node, const char *uname,
		int depth, void *data)
{
	struct mem_desc_t *sram;

	if (depth != 1 || (strcmp(uname, "chosen") != 0
			&& strcmp(uname, "chosen@0") != 0))
		return 0;

	sram = (struct mem_desc_t *) of_get_flat_dt_prop(node,
			"ram_console", NULL);
	if (sram) {
		pr_notice("ram_console:[DT] 0x%lx@0x%lx\n",
				sram->size, sram->start);
		*(struct mem_desc_t *) data = *sram;
	}

	return 1;
}
#endif
#endif

static int __init ram_console_early_init(void)
{
	struct ram_console_buffer *bufp = NULL;
	size_t buffer_size = 0;
#if defined(CONFIG_MTK_RAM_CONSOLE_USING_SRAM)
#ifdef CONFIG_OF
	struct mem_desc_t sram = { 0 };

	if (of_scan_flat_dt(dt_get_ram_console, &sram)) {
		if (sram.start == 0) {
			sram.start = CONFIG_MTK_RAM_CONSOLE_ADDR;
			sram.size = CONFIG_MTK_RAM_CONSOLE_SIZE;
		}
		bufp = ioremap_wc(sram.start, sram.size);
		ram_console_buffer_pa = (struct ram_console_buffer *)sram.start;
		if (bufp)
			buffer_size = sram.size;
		else {
			pr_err("ram_console: ioremap failed, [0x%lx, 0x%lx]\n",
					sram.start,
			       sram.size);
			return 0;
		}
	} else {
		return 0;
	}
#else
	bufp = ioremap_wc(CONFIG_MTK_RAM_CONSOLE_ADDR,
			CONFIG_MTK_RAM_CONSOLE_SIZE);
	if (bufp)
		buffer_size = CONFIG_MTK_RAM_CONSOLE_SIZE;
		ram_console_buffer_pa = CONFIG_MTK_RAM_CONSOLE_ADDR;
	else {
		pr_err("ram_console: ioremap failed, [0x%x, 0x%x]\n",
				sram.start, sram.size);
		return 0;
	}
#endif
#elif defined(CONFIG_MTK_RAM_CONSOLE_USING_DRAM)
	bufp = remap_lowmem(CONFIG_MTK_RAM_CONSOLE_DRAM_ADDR,
			CONFIG_MTK_RAM_CONSOLE_DRAM_SIZE);
	ram_console_buffer_pa =
		(struct ram_console_buffer *)CONFIG_MTK_RAM_CONSOLE_DRAM_ADDR;
	if (bufp == NULL) {
		pr_err("ram_console: ioremap failed\n");
		return 0;
	}
	buffer_size = CONFIG_MTK_RAM_CONSOLE_DRAM_SIZE;
#else
	return 0;
#endif

	pr_notice("ram_console: buffer start: 0x%p, size: 0x%zx\n",
			bufp, buffer_size);
	mtk_cpu_num = num_present_cpus();
	return ram_console_init(bufp, buffer_size);
}

static int ram_console_show(struct seq_file *m, void *v)
{
	ram_console_lastk_show(ram_console_old, m, v);
	return 0;
}

static int ram_console_file_open(struct inode *inode, struct file *file)
{
	return single_open(file, ram_console_show, inode->i_private);
}

static const struct file_operations ram_console_file_ops = {
	.owner = THIS_MODULE,
	.open = ram_console_file_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int __init ram_console_late_init(void)
{
	struct proc_dir_entry *entry;

	if (reserve_mem_fail) {
		pr_info("ram_console/pstore wrong reserved memory\n");
		BUG();
	}
	entry = proc_create("last_kmsg", 0444, NULL, &ram_console_file_ops);
	if (!entry) {
		pr_err("ram_console: failed to create proc entry\n");
		kfree(ram_console_old);
		ram_console_old = NULL;
		return 0;
	}
	return 0;
}

console_initcall(ram_console_early_init);
late_initcall(ram_console_late_init);

int ram_console_pstore_reserve_memory(struct reserved_mem *rmem)
{
#ifndef DUMMY_MEMORY_LAYOUT
	if (rmem->base != KERN_PSTORE_BASE ||
		rmem->size > KERN_PSTORE_MAX_SIZE) {
		reserve_mem_fail = true;
		pr_info("pstore: Check the reserved address and size\n");
	}
#endif
	pr_info("[memblock]%s: 0x%llx - 0x%llx (0x%llx)\n", "mediatek,pstore",
		 (unsigned long long)rmem->base,
		 (unsigned long long)rmem->base +
		 (unsigned long long)rmem->size,
		 (unsigned long long)rmem->size);
	return 0;
}

int ram_console_binary_reserve_memory(struct reserved_mem *rmem)
{
#ifndef DUMMY_MEMORY_LAYOUT
	if (rmem->base != KERN_RAMCONSOLE_BASE ||
		rmem->size > KERN_RAMCONSOLE_MAX_SIZE) {
		reserve_mem_fail = true;
		pr_info("ram_console: Check the reserved address and size\n");
	}
#endif
	pr_info("[memblock]%s: 0x%llx - 0x%llx (0x%llx)\n",
		"mediatek,ram_console",
		 (unsigned long long)rmem->base,
		 (unsigned long long)rmem->base +
		 (unsigned long long)rmem->size,
		 (unsigned long long)rmem->size);
	return 0;
}

RESERVEDMEM_OF_DECLARE(reserve_memory_pstore, "mediatek,pstore",
		       ram_console_pstore_reserve_memory);
RESERVEDMEM_OF_DECLARE(reserve_memory_ram_console, "mediatek,ram_console",
		       ram_console_binary_reserve_memory);

/* aee sram flags save */
#define RR_BASE(stage)	\
	((void *)ram_console_buffer + ram_console_buffer->off_##stage)
#define RR_LINUX ((struct last_reboot_reason *)RR_BASE(linux))
#define RR_BASE_PA(stage)	\
	((void *)ram_console_buffer_pa + ram_console_buffer->off_##stage)
#define RR_LINUX_PA ((struct last_reboot_reason *)RR_BASE_PA(linux))

/*NOTICE: You should check if ram_console is null before call these macros*/
#define LAST_RR_SET(rr_item, value) (RR_LINUX->rr_item = value)

#define LAST_RR_SET_WITH_ID(rr_item, id, value) (RR_LINUX->rr_item[id] = value)

#define LAST_RR_VAL(rr_item)				\
	(ram_console_buffer ? RR_LINUX->rr_item : 0)

#define LAST_RR_MEMCPY(rr_item, str, len)				\
	(strlcpy(RR_LINUX->rr_item, str, len))

#define LAST_RR_MEMCPY_WITH_ID(rr_item, id, str, len)			\
	(strlcpy(RR_LINUX->rr_item[id], str, len))

static void ram_console_init_val(void)
{
	LAST_RR_SET(pmic_ext_buck, 0xff);
#if defined(CONFIG_RANDOMIZE_BASE) && defined(CONFIG_ARM64)
	LAST_RR_SET(kaslr_offset, 0xecab1e);
#else
	LAST_RR_SET(kaslr_offset, 0xd15ab1e);
#endif
	LAST_RR_SET(ram_console_buffer_addr,
		(unsigned long)&ram_console_buffer);
}

void aee_rr_rec_fiq_step(u8 step)
{
	if (!ram_console_init_done || !ram_console_buffer)
		return;
	LAST_RR_SET(fiq_step, step);
}

int aee_rr_curr_fiq_step(void)
{
	return LAST_RR_VAL(fiq_step);
}

void aee_rr_rec_exp_type(unsigned int type)
{
	if (!ram_console_init_done || !ram_console_buffer)
		return;
	if (LAST_RR_VAL(exp_type) == 0 && type < 16)
		LAST_RR_SET(exp_type, RAM_CONSOLE_EXP_TYPE_MAGIC | type);
}

unsigned int aee_rr_curr_exp_type(void)
{
	unsigned int exp_type = LAST_RR_VAL(exp_type);

	return RAM_CONSOLE_EXP_TYPE_DEC(exp_type);
}

void aee_rr_rec_kaslr_offset(uint64_t offset)
{
	if (!ram_console_init_done || !ram_console_buffer)
		return;
	LAST_RR_SET(kaslr_offset, offset);
}

/* composite api */
void aee_rr_rec_last_irq_enter(int cpu, int irq, u64 jiffies)
{
	if (!ram_console_init_done || !ram_console_buffer)
		return;
	if (cpu >= 0 && cpu < num_possible_cpus()) {
		LAST_RR_SET_WITH_ID(last_irq_enter, cpu, irq);
		LAST_RR_SET_WITH_ID(jiffies_last_irq_enter, cpu, jiffies);
	}
	mb();			/*TODO:need add comments */
}

void aee_rr_rec_last_irq_exit(int cpu, int irq, u64 jiffies)
{
	if (!ram_console_init_done || !ram_console_buffer)
		return;
	if (cpu >= 0 && cpu < num_possible_cpus()) {
		LAST_RR_SET_WITH_ID(last_irq_exit, cpu, irq);
		LAST_RR_SET_WITH_ID(jiffies_last_irq_exit, cpu, jiffies);
	}
	mb();			/*TODO:need add comments */
}

void aee_rr_rec_hotplug_footprint(int cpu, u8 fp)
{
	if (!ram_console_init_done || !ram_console_buffer)
		return;
	if (cpu >= 0 && cpu < num_possible_cpus())
		LAST_RR_SET_WITH_ID(hotplug_footprint, cpu, fp);
}

void aee_rr_rec_hotplug_cpu_event(u8 val)
{
	if (!ram_console_init_done || !ram_console_buffer)
		return;
	LAST_RR_SET(hotplug_cpu_event, val);
}

void aee_rr_rec_hotplug_cb_index(u8 val)
{
	if (!ram_console_init_done || !ram_console_buffer)
		return;
	LAST_RR_SET(hotplug_cb_index, val);
}

void aee_rr_rec_hotplug_cb_fp(unsigned long val)
{
	if (!ram_console_init_done || !ram_console_buffer)
		return;
	LAST_RR_SET(hotplug_cb_fp, val);
}

void aee_rr_rec_hotplug_cb_times(unsigned long val)
{
	if (!ram_console_init_done || !ram_console_buffer)
		return;
	LAST_RR_SET(hotplug_cb_times, val);
}

void aee_rr_rec_hps_cb_enter_times(unsigned long long val)
{
	if (!ram_console_init_done || !ram_console_buffer)
		return;
	LAST_RR_SET(hps_cb_enter_times, val);
}

void aee_rr_rec_hps_cb_cpu_bitmask(unsigned int val)
{
	if (!ram_console_init_done || !ram_console_buffer)
		return;
	LAST_RR_SET(hps_cb_cpu_bitmask, val);
}

void aee_rr_rec_hps_cb_footprint(unsigned int val)
{
	if (!ram_console_init_done || !ram_console_buffer)
		return;
	LAST_RR_SET(hps_cb_footprint, val);
}

void aee_rr_rec_hps_cb_fp_times(unsigned long long val)
{
	if (!ram_console_init_done || !ram_console_buffer)
		return;
	LAST_RR_SET(hps_cb_fp_times, val);
}

void aee_rr_rec_cpu_caller(u32 val)
{
	if (!ram_console_init_done || !ram_console_buffer)
		return;
	LAST_RR_SET(cpu_caller, val);
}

void aee_rr_rec_cpu_callee(u32 val)
{
	if (!ram_console_init_done || !ram_console_buffer)
		return;
	LAST_RR_SET(cpu_callee, val);
}

void aee_rr_rec_cpu_up_prepare_ktime(u64 val)
{
	if (!ram_console_init_done || !ram_console_buffer)
		return;
	LAST_RR_SET(cpu_up_prepare_ktime, val);
}

void aee_rr_rec_cpu_starting_ktime(u64 val)
{
	if (!ram_console_init_done || !ram_console_buffer)
		return;
	LAST_RR_SET(cpu_starting_ktime, val);
}

void aee_rr_rec_cpu_online_ktime(u64 val)
{
	if (!ram_console_init_done || !ram_console_buffer)
		return;
	LAST_RR_SET(cpu_online_ktime, val);
}

void aee_rr_rec_cpu_down_prepare_ktime(u64 val)
{
	if (!ram_console_init_done || !ram_console_buffer)
		return;
	LAST_RR_SET(cpu_down_prepare_ktime, val);
}

void aee_rr_rec_cpu_dying_ktime(u64 val)
{
	if (!ram_console_init_done || !ram_console_buffer)
		return;
	LAST_RR_SET(cpu_dying_ktime, val);
}

void aee_rr_rec_cpu_dead_ktime(u64 val)
{
	if (!ram_console_init_done || !ram_console_buffer)
		return;
	LAST_RR_SET(cpu_dead_ktime, val);
}

void aee_rr_rec_cpu_post_dead_ktime(u64 val)
{
	if (!ram_console_init_done || !ram_console_buffer)
		return;
	LAST_RR_SET(cpu_post_dead_ktime, val);
}
void aee_rr_rec_clk(int id, u32 val)
{
	if (!ram_console_init_done || !ram_console_buffer)
		return;
	LAST_RR_SET_WITH_ID(clk_data, id, val);
}

void aee_rr_rec_deepidle_val(u32 val)
{
	if (!ram_console_init_done || !ram_console_buffer)
		return;
	LAST_RR_SET(deepidle_data, val);
}

u32 aee_rr_curr_deepidle_val(void)
{
	return LAST_RR_VAL(deepidle_data);
}

void aee_rr_rec_mcdi_val(int id, u32 val)
{
	if (!ram_console_init_done || !ram_console_buffer)
		return;
	LAST_RR_SET_WITH_ID(mcdi_footprint, id, val);
}

void aee_rr_rec_mcdi_wfi_val(u32 val)
{
	if (!ram_console_init_done || !ram_console_buffer)
		return;
	LAST_RR_SET(mcdi_wfi, val);
}

u32 aee_rr_curr_mcdi_wfi_val(void)
{
	return LAST_RR_VAL(mcdi_wfi);
}

void aee_rr_rec_mcdi_r15_val(u32 val)
{
	if (!ram_console_init_done || !ram_console_buffer)
		return;
	LAST_RR_SET(mcdi_r15, val);
}

void aee_rr_rec_sodi3_val(u32 val)
{
	if (!ram_console_init_done)
		return;
	LAST_RR_SET(sodi3_data, val);
}

u32 aee_rr_curr_sodi3_val(void)
{
	if (!ram_console_init_done)
		return 0;
	return LAST_RR_VAL(sodi3_data);
}

void aee_rr_rec_sodi_val(u32 val)
{
	if (!ram_console_init_done || !ram_console_buffer)
		return;
	LAST_RR_SET(sodi_data, val);
}

u32 aee_rr_curr_sodi_val(void)
{
	return LAST_RR_VAL(sodi_data);
}

void aee_rr_rec_mcsodi_val(u32 val)
{
	if (!ram_console_init_done || !ram_console_buffer)
		return;
	LAST_RR_SET(mcsodi_data, val);
}

u32 aee_rr_curr_mcsodi_val(void)
{
	return LAST_RR_VAL(mcsodi_data);
}

void aee_rr_rec_spm_suspend_val(u32 val)
{
	if (!ram_console_init_done || !ram_console_buffer)
		return;
	LAST_RR_SET(spm_suspend_data, val);
}

u32 aee_rr_curr_spm_suspend_val(void)
{
	return LAST_RR_VAL(spm_suspend_data);
}

void aee_rr_rec_spm_common_scenario_val(u32 val)
{
	if (!ram_console_init_done || !ram_console_buffer)
		return;
	LAST_RR_SET(spm_common_scenario_data, val);
}

u32 aee_rr_curr_spm_common_scenario_val(void)
{
	return LAST_RR_VAL(spm_common_scenario_data);
}

/* special case without MMU, return addr directly,
 * strongly suggest not to use
 */
unsigned int *aee_rr_rec_mcdi_wfi(void)
{
#if 0
	if (ram_console_buffer)
		return &RR_LINUX->mcdi_wfi;
	else
		return NULL;
#endif
	return NULL;
}

unsigned long *aee_rr_rec_mtk_cpuidle_footprint_va(void)
{
	if (ram_console_buffer)
		return (unsigned long *)&RR_LINUX->mtk_cpuidle_footprint;
	else
		return NULL;
}

unsigned long *aee_rr_rec_fiq_cache_step_pa(void)
{
	if (ram_console_buffer_pa)
		return (unsigned long *)&RR_LINUX_PA->fiq_cache_step;
	else
		return NULL;
}

unsigned long *aee_rr_rec_mtk_cpuidle_footprint_pa(void)
{
	if (ram_console_buffer_pa)
		return (unsigned long *)&RR_LINUX_PA->mtk_cpuidle_footprint;
	else
		return NULL;
}

void aee_rr_rec_vcore_dvfs_opp(u32 val)
{
	if (!ram_console_init_done)
		return;
	LAST_RR_SET(vcore_dvfs_opp, val);
}

u32 aee_rr_curr_vcore_dvfs_opp(void)
{
	return LAST_RR_VAL(vcore_dvfs_opp);
}
EXPORT_SYMBOL(aee_rr_curr_vcore_dvfs_opp);

void aee_rr_rec_vcore_dvfs_status(u32 val)
{
	if (!ram_console_init_done)
		return;
	LAST_RR_SET(vcore_dvfs_status, val);
}

u32 aee_rr_curr_vcore_dvfs_status(void)
{
	return LAST_RR_VAL(vcore_dvfs_status);
}

void aee_rr_rec_ppm_cluster_limit(int id, u32 val)
{
	if (!ram_console_init_done || !ram_console_buffer)
		return;
	LAST_RR_SET_WITH_ID(ppm_cluster_limit, id, val);
}

void aee_rr_rec_ppm_step(u8 val)
{
	if (!ram_console_init_done || !ram_console_buffer)
		return;
	LAST_RR_SET(ppm_step, val);
}

void aee_rr_rec_ppm_cur_state(u8 val)
{
	if (!ram_console_init_done || !ram_console_buffer)
		return;
	LAST_RR_SET(ppm_cur_state, val);
}

void aee_rr_rec_ppm_min_pwr_bgt(u32 val)
{
	if (!ram_console_init_done || !ram_console_buffer)
		return;
	LAST_RR_SET(ppm_min_pwr_bgt, val);
}

void aee_rr_rec_ppm_policy_mask(u32 val)
{
	if (!ram_console_init_done || !ram_console_buffer)
		return;
	LAST_RR_SET(ppm_policy_mask, val);
}

void aee_rr_rec_ppm_waiting_for_pbm(u8 val)
{
	if (!ram_console_init_done || !ram_console_buffer)
		return;
	LAST_RR_SET(ppm_waiting_for_pbm, val);
}

void aee_rr_rec_cpu_dvfs_vproc_big(u8 val)
{
	if (!ram_console_init_done || !ram_console_buffer)
		return;
	LAST_RR_SET(cpu_dvfs_vproc_big, val);
}

void aee_rr_rec_cpu_dvfs_vproc_little(u8 val)
{
	if (!ram_console_init_done || !ram_console_buffer)
		return;
	LAST_RR_SET(cpu_dvfs_vproc_little, val);
}

void aee_rr_rec_cpu_dvfs_oppidx(u8 val)
{
	if (!ram_console_init_done || !ram_console_buffer)
		return;
	LAST_RR_SET(cpu_dvfs_oppidx, val);
}

void aee_rr_rec_cpu_dvfs_cci_oppidx(u8 val)
{
	if (!ram_console_init_done || !ram_console_buffer)
		return;
	LAST_RR_SET(cpu_dvfs_cci_oppidx, val);
}

u8 aee_rr_curr_cpu_dvfs_oppidx(void)
{
	return LAST_RR_VAL(cpu_dvfs_oppidx);
}

u8 aee_rr_curr_cpu_dvfs_cci_oppidx(void)
{
	return LAST_RR_VAL(cpu_dvfs_cci_oppidx);
}

void aee_rr_rec_cpu_dvfs_status(u8 val)
{
	if (!ram_console_init_done || !ram_console_buffer)
		return;
	LAST_RR_SET(cpu_dvfs_status, val);
}

u8 aee_rr_curr_cpu_dvfs_status(void)
{
	return LAST_RR_VAL(cpu_dvfs_status);
}

void aee_rr_rec_cpu_dvfs_step(u8 val)
{
	if (!ram_console_init_done || !ram_console_buffer)
		return;
	LAST_RR_SET(cpu_dvfs_step, val);
}

u8 aee_rr_curr_cpu_dvfs_step(void)
{
	return LAST_RR_VAL(cpu_dvfs_step);
}

void aee_rr_rec_cpu_dvfs_pbm_step(u8 val)
{
	if (!ram_console_init_done || !ram_console_buffer)
		return;
	LAST_RR_SET(cpu_dvfs_pbm_step, val);
}

u8 aee_rr_curr_cpu_dvfs_pbm_step(void)
{
	return LAST_RR_VAL(cpu_dvfs_pbm_step);
}

void aee_rr_rec_cpu_dvfs_cb(u8 val)
{
	if (!ram_console_init_done || !ram_console_buffer)
		return;
	LAST_RR_SET(cpu_dvfs_cb, val);
}

u8 aee_rr_curr_cpu_dvfs_cb(void)
{
	return LAST_RR_VAL(cpu_dvfs_cb);
}

void aee_rr_rec_cpufreq_cb(u8 val)
{
	if (!ram_console_init_done || !ram_console_buffer)
		return;
	LAST_RR_SET(cpufreq_cb, val);
}

u8 aee_rr_curr_cpufreq_cb(void)
{
	return LAST_RR_VAL(cpufreq_cb);
}

void aee_rr_rec_gpu_dvfs_vgpu(u8 val)
{
	if (!ram_console_init_done || !ram_console_buffer)
		return;
	LAST_RR_SET(gpu_dvfs_vgpu, val);
}

void aee_rr_rec_gpu_dvfs_oppidx(u8 val)
{
	if (!ram_console_init_done || !ram_console_buffer)
		return;
	LAST_RR_SET(gpu_dvfs_oppidx, val);
}

void aee_rr_rec_gpu_dvfs_status(u8 val)
{
	if (!ram_console_init_done || !ram_console_buffer)
		return;
	LAST_RR_SET(gpu_dvfs_status, val);
}

u8 aee_rr_curr_gpu_dvfs_status(void)
{
	return LAST_RR_VAL(gpu_dvfs_status);
}

void aee_rr_rec_drcc_0(u32 val)
{
	if (!ram_console_init_done || !ram_console_buffer)
		return;
	LAST_RR_SET(drcc_0, val);
}

void aee_rr_rec_drcc_1(u32 val)
{
	if (!ram_console_init_done || !ram_console_buffer)
		return;
	LAST_RR_SET(drcc_1, val);
}

void aee_rr_rec_drcc_2(u32 val)
{
	if (!ram_console_init_done || !ram_console_buffer)
		return;
	LAST_RR_SET(drcc_2, val);
}

void aee_rr_rec_drcc_3(u32 val)
{
	if (!ram_console_init_done || !ram_console_buffer)
		return;
	LAST_RR_SET(drcc_3, val);
}

void aee_rr_rec_ptp_devinfo_0(u32 val)
{
	if (!ram_console_init_done || !ram_console_buffer)
		return;
	LAST_RR_SET(ptp_devinfo_0, val);
}

void aee_rr_rec_ptp_devinfo_1(u32 val)
{
	if (!ram_console_init_done || !ram_console_buffer)
		return;
	LAST_RR_SET(ptp_devinfo_1, val);
}

void aee_rr_rec_ptp_devinfo_2(u32 val)
{
	if (!ram_console_init_done || !ram_console_buffer)
		return;
	LAST_RR_SET(ptp_devinfo_2, val);
}

void aee_rr_rec_ptp_devinfo_3(u32 val)
{
	if (!ram_console_init_done || !ram_console_buffer)
		return;
	LAST_RR_SET(ptp_devinfo_3, val);
}

void aee_rr_rec_ptp_devinfo_4(u32 val)
{
	if (!ram_console_init_done || !ram_console_buffer)
		return;
	LAST_RR_SET(ptp_devinfo_4, val);
}

void aee_rr_rec_ptp_devinfo_5(u32 val)
{
	if (!ram_console_init_done || !ram_console_buffer)
		return;
	LAST_RR_SET(ptp_devinfo_5, val);
}

void aee_rr_rec_ptp_devinfo_6(u32 val)
{
	if (!ram_console_init_done || !ram_console_buffer)
		return;
	LAST_RR_SET(ptp_devinfo_6, val);
}

void aee_rr_rec_ptp_devinfo_7(u32 val)
{
	if (!ram_console_init_done || !ram_console_buffer)
		return;
	LAST_RR_SET(ptp_devinfo_7, val);
}

void aee_rr_rec_ptp_e0(u32 val)
{
	if (!ram_console_init_done || !ram_console_buffer)
		return;
	LAST_RR_SET(ptp_e0, val);
}

void aee_rr_rec_ptp_e1(u32 val)
{
	if (!ram_console_init_done || !ram_console_buffer)
		return;
	LAST_RR_SET(ptp_e1, val);
}

void aee_rr_rec_ptp_e2(u32 val)
{
	if (!ram_console_init_done || !ram_console_buffer)
		return;
	LAST_RR_SET(ptp_e2, val);
}

void aee_rr_rec_ptp_e3(u32 val)
{
	if (!ram_console_init_done || !ram_console_buffer)
		return;
	LAST_RR_SET(ptp_e3, val);
}

void aee_rr_rec_ptp_e4(u32 val)
{
	if (!ram_console_init_done || !ram_console_buffer)
		return;
	LAST_RR_SET(ptp_e4, val);
}

void aee_rr_rec_ptp_e5(u32 val)
{
	if (!ram_console_init_done || !ram_console_buffer)
		return;
	LAST_RR_SET(ptp_e5, val);
}

void aee_rr_rec_ptp_e6(u32 val)
{
	if (!ram_console_init_done || !ram_console_buffer)
		return;
	LAST_RR_SET(ptp_e6, val);
}

void aee_rr_rec_ptp_e7(u32 val)
{
	if (!ram_console_init_done || !ram_console_buffer)
		return;
	LAST_RR_SET(ptp_e7, val);
}

void aee_rr_rec_ptp_e8(u32 val)
{
	if (!ram_console_init_done || !ram_console_buffer)
		return;
	LAST_RR_SET(ptp_e8, val);
}

void aee_rr_rec_ptp_e9(u32 val)
{
	if (!ram_console_init_done || !ram_console_buffer)
		return;
	LAST_RR_SET(ptp_e9, val);
}

void aee_rr_rec_ptp_e10(u32 val)
{
	if (!ram_console_init_done || !ram_console_buffer)
		return;
	LAST_RR_SET(ptp_e10, val);
}

void aee_rr_rec_ptp_e11(u32 val)
{
	if (!ram_console_init_done || !ram_console_buffer)
		return;
	LAST_RR_SET(ptp_e11, val);
}

void aee_rr_rec_ptp_vboot(u64 val)
{
	if (!ram_console_init_done || !ram_console_buffer)
		return;
	LAST_RR_SET(ptp_vboot, val);
}

void aee_rr_rec_ptp_cpu_big_volt(u64 val)
{
	if (!ram_console_init_done || !ram_console_buffer)
		return;
	LAST_RR_SET(ptp_cpu_big_volt, val);
}

void aee_rr_rec_ptp_cpu_big_volt_1(u64 val)
{
	if (!ram_console_init_done || !ram_console_buffer)
		return;
	LAST_RR_SET(ptp_cpu_big_volt_1, val);
}

void aee_rr_rec_ptp_cpu_big_volt_2(u64 val)
{
	if (!ram_console_init_done || !ram_console_buffer)
		return;
	LAST_RR_SET(ptp_cpu_big_volt_2, val);
}

void aee_rr_rec_ptp_cpu_big_volt_3(u64 val)
{
	if (!ram_console_init_done || !ram_console_buffer)
		return;
	LAST_RR_SET(ptp_cpu_big_volt_3, val);
}

void aee_rr_rec_ptp_cpu_2_little_volt(u64 val)
{
	if (!ram_console_init_done || !ram_console_buffer)
		return;
	LAST_RR_SET(ptp_cpu_2_little_volt, val);
}

void aee_rr_rec_ptp_cpu_2_little_volt_1(u64 val)
{
	if (!ram_console_init_done || !ram_console_buffer)
		return;
	LAST_RR_SET(ptp_cpu_2_little_volt_1, val);
}

void aee_rr_rec_ptp_cpu_2_little_volt_2(u64 val)
{
	if (!ram_console_init_done || !ram_console_buffer)
		return;
	LAST_RR_SET(ptp_cpu_2_little_volt_2, val);
}

void aee_rr_rec_ptp_cpu_2_little_volt_3(u64 val)
{
	if (!ram_console_init_done || !ram_console_buffer)
		return;
	LAST_RR_SET(ptp_cpu_2_little_volt_3, val);
}

void aee_rr_rec_ptp_cpu_little_volt(u64 val)
{
	if (!ram_console_init_done || !ram_console_buffer)
		return;
	LAST_RR_SET(ptp_cpu_little_volt, val);
}

void aee_rr_rec_ptp_cpu_little_volt_1(u64 val)
{
	if (!ram_console_init_done || !ram_console_buffer)
		return;
	LAST_RR_SET(ptp_cpu_little_volt_1, val);
}

void aee_rr_rec_ptp_cpu_little_volt_2(u64 val)
{
	if (!ram_console_init_done || !ram_console_buffer)
		return;
	LAST_RR_SET(ptp_cpu_little_volt_2, val);
}

void aee_rr_rec_ptp_cpu_little_volt_3(u64 val)
{
	if (!ram_console_init_done || !ram_console_buffer)
		return;
	LAST_RR_SET(ptp_cpu_little_volt_3, val);
}

void aee_rr_rec_ptp_cpu_cci_volt(u64 val)
{
	if (!ram_console_init_done || !ram_console_buffer)
		return;
	LAST_RR_SET(ptp_cpu_cci_volt, val);
}

void aee_rr_rec_ptp_cpu_cci_volt_1(u64 val)
{
	if (!ram_console_init_done || !ram_console_buffer)
		return;
	LAST_RR_SET(ptp_cpu_cci_volt_1, val);
}

void aee_rr_rec_ptp_cpu_cci_volt_2(u64 val)
{
	if (!ram_console_init_done || !ram_console_buffer)
		return;
	LAST_RR_SET(ptp_cpu_cci_volt_2, val);
}

void aee_rr_rec_ptp_cpu_cci_volt_3(u64 val)
{
	if (!ram_console_init_done || !ram_console_buffer)
		return;
	LAST_RR_SET(ptp_cpu_cci_volt_3, val);
}

void aee_rr_rec_ptp_gpu_volt(u64 val)
{
	if (!ram_console_init_done || !ram_console_buffer)
		return;
	LAST_RR_SET(ptp_gpu_volt, val);
}

void aee_rr_rec_ptp_gpu_volt_1(u64 val)
{
	if (!ram_console_init_done || !ram_console_buffer)
		return;
	LAST_RR_SET(ptp_gpu_volt_1, val);
}

void aee_rr_rec_ptp_gpu_volt_2(u64 val)
{
	if (!ram_console_init_done || !ram_console_buffer)
		return;
	LAST_RR_SET(ptp_gpu_volt_2, val);
}

void aee_rr_rec_ptp_gpu_volt_3(u64 val)
{
	if (!ram_console_init_done || !ram_console_buffer)
		return;
	LAST_RR_SET(ptp_gpu_volt_3, val);
}

void aee_rr_rec_ptp_temp(u64 val)
{
	if (!ram_console_init_done || !ram_console_buffer)
		return;
	LAST_RR_SET(ptp_temp, val);
}

void aee_rr_rec_ptp_status(u8 val)
{
	if (!ram_console_init_done || !ram_console_buffer)
		return;
	LAST_RR_SET(ptp_status, val);
}

void aee_rr_rec_eem_pi_offset(u8 val)
{
	if (!ram_console_init_done)
		return;
	LAST_RR_SET(eem_pi_offset, val);
}

void aee_rr_rec_etc_status(u8 val)
{
	if (!ram_console_init_done)
		return;
	LAST_RR_SET(etc_status, val);
}

void aee_rr_rec_etc_mode(u8 val)
{
	if (!ram_console_init_done)
		return;
	LAST_RR_SET(etc_mode, val);
}

int aee_rr_init_thermal_temp(int num)
{
	if (num < 0 || num > THERMAL_RESERVED_TZS) {
		pr_notice("%s num= %d\n", __func__, num);
		return -1;
	}

	thermal_num = num;
	return 0;
}

int aee_rr_rec_thermal_temp(int index, s8 val)
{
	if (!ram_console_init_done || !ram_console_buffer)
		return -1;

	if (index < 0 || index >= thermal_num) {
		pr_notice("%s index= %d\n", __func__, index);
		return -1;
	}

	LAST_RR_SET(thermal_temp[index], val);
	return 0;
}

void aee_rr_rec_thermal_status(u8 val)
{
	if (!ram_console_init_done || !ram_console_buffer)
		return;
	LAST_RR_SET(thermal_status, val);
}

void aee_rr_rec_thermal_ATM_status(u8 val)
{
	if (!ram_console_init_done || !ram_console_buffer)
		return;
	LAST_RR_SET(thermal_ATM_status, val);
}

void aee_rr_rec_thermal_ktime(u64 val)
{
	if (!ram_console_init_done || !ram_console_buffer)
		return;
	LAST_RR_SET(thermal_ktime, val);
}

void aee_rr_rec_idvfs_ctrl_reg(u32 val)
{
	if (!ram_console_init_done || !ram_console_buffer)
		return;
	LAST_RR_SET(idvfs_ctrl_reg, val);
}

void aee_rr_rec_idvfs_enable_cnt(u32 val)
{
	if (!ram_console_init_done || !ram_console_buffer)
		return;
	LAST_RR_SET(idvfs_enable_cnt, val);
}

void aee_rr_rec_idvfs_swreq_cnt(u32 val)
{
	if (!ram_console_init_done || !ram_console_buffer)
		return;
	LAST_RR_SET(idvfs_swreq_cnt, val);
}

void aee_rr_rec_idvfs_curr_volt(u16 val)
{
	if (!ram_console_init_done || !ram_console_buffer)
		return;
	LAST_RR_SET(idvfs_curr_volt, val);
}

void aee_rr_rec_idvfs_sram_ldo(u16 val)
{
	if (!ram_console_init_done || !ram_console_buffer)
		return;
	LAST_RR_SET(idvfs_sram_ldo, val);
}

void aee_rr_rec_idvfs_swavg_curr_pct_x100(u16 val)
{
	if (!ram_console_init_done || !ram_console_buffer)
		return;
	LAST_RR_SET(idvfs_swavg_curr_pct_x100, val);
}

void aee_rr_rec_idvfs_swreq_curr_pct_x100(u16 val)
{
	if (!ram_console_init_done || !ram_console_buffer)
		return;
	LAST_RR_SET(idvfs_swreq_curr_pct_x100, val);
}

void aee_rr_rec_idvfs_swreq_next_pct_x100(u16 val)
{
	if (!ram_console_init_done || !ram_console_buffer)
		return;
	LAST_RR_SET(idvfs_swreq_next_pct_x100, val);
}

void aee_rr_rec_idvfs_state_manchine(u8 val)
{
	if (!ram_console_init_done || !ram_console_buffer)
		return;
	LAST_RR_SET(idvfs_state_manchine, val);
}

void aee_rr_rec_ocp_target_limit(int id, u32 val)
{
	if (!ram_console_init_done || !ram_console_buffer)
		return;

	if (id < 0 || id >= 4) {
		pr_notice("%s: Invalid ocp id = %d\n", __func__, id);
		return;
	}

	LAST_RR_SET_WITH_ID(ocp_target_limit, id, val);
}

void aee_rr_rec_ocp_enable(u8 val)
{
	if (!ram_console_init_done || !ram_console_buffer)
		return;
	LAST_RR_SET(ocp_enable, val);
}

u32 aee_rr_curr_drcc_0(void)
{
	return LAST_RR_VAL(drcc_0);
}

u32 aee_rr_curr_drcc_1(void)
{
	return LAST_RR_VAL(drcc_1);
}

u32 aee_rr_curr_drcc_2(void)
{
	return LAST_RR_VAL(drcc_2);
}

u32 aee_rr_curr_drcc_3(void)
{
	return LAST_RR_VAL(drcc_3);
}

u32 aee_rr_curr_ptp_devinfo_0(void)
{
	return LAST_RR_VAL(ptp_devinfo_0);
}

u32 aee_rr_curr_ptp_devinfo_1(void)
{
	return LAST_RR_VAL(ptp_devinfo_1);
}

u32 aee_rr_curr_ptp_devinfo_2(void)
{
	return LAST_RR_VAL(ptp_devinfo_2);
}

u32 aee_rr_curr_ptp_devinfo_3(void)
{
	return LAST_RR_VAL(ptp_devinfo_3);
}

u32 aee_rr_curr_ptp_devinfo_4(void)
{
	return LAST_RR_VAL(ptp_devinfo_4);
}

u32 aee_rr_curr_ptp_devinfo_5(void)
{
	return LAST_RR_VAL(ptp_devinfo_5);
}

u32 aee_rr_curr_ptp_devinfo_6(void)
{
	return LAST_RR_VAL(ptp_devinfo_6);
}

u32 aee_rr_curr_ptp_devinfo_7(void)
{
	return LAST_RR_VAL(ptp_devinfo_7);
}

u32 aee_rr_curr_ptp_e0(void)
{
	return LAST_RR_VAL(ptp_e0);
}

u32 aee_rr_curr_ptp_e1(void)
{
	return LAST_RR_VAL(ptp_e1);
}

u32 aee_rr_curr_ptp_e2(void)
{
	return LAST_RR_VAL(ptp_e2);
}

u32 aee_rr_curr_ptp_e3(void)
{
	return LAST_RR_VAL(ptp_e3);
}

u32 aee_rr_curr_ptp_e4(void)
{
	return LAST_RR_VAL(ptp_e4);
}

u32 aee_rr_curr_ptp_e5(void)
{
	return LAST_RR_VAL(ptp_e5);
}

u32 aee_rr_curr_ptp_e6(void)
{
	return LAST_RR_VAL(ptp_e6);
}

u32 aee_rr_curr_ptp_e7(void)
{
	return LAST_RR_VAL(ptp_e7);
}

u32 aee_rr_curr_ptp_e8(void)
{
	return LAST_RR_VAL(ptp_e8);
}

u32 aee_rr_curr_ptp_e9(void)
{
	return LAST_RR_VAL(ptp_e9);
}

u32 aee_rr_curr_ptp_e10(void)
{
	return LAST_RR_VAL(ptp_e10);
}

u32 aee_rr_curr_ptp_e11(void)
{
	return LAST_RR_VAL(ptp_e11);
}

u64 aee_rr_curr_ptp_vboot(void)
{
	return LAST_RR_VAL(ptp_vboot);
}

u64 aee_rr_curr_ptp_cpu_big_volt(void)
{
	return LAST_RR_VAL(ptp_cpu_big_volt);
}

u64 aee_rr_curr_ptp_cpu_big_volt_1(void)
{
	return LAST_RR_VAL(ptp_cpu_big_volt_1);
}

u64 aee_rr_curr_ptp_cpu_big_volt_2(void)
{
	return LAST_RR_VAL(ptp_cpu_big_volt_2);
}

u64 aee_rr_curr_ptp_cpu_big_volt_3(void)
{
	return LAST_RR_VAL(ptp_cpu_big_volt_3);
}

u64 aee_rr_curr_ptp_cpu_2_little_volt(void)
{
	return LAST_RR_VAL(ptp_cpu_2_little_volt);
}

u64 aee_rr_curr_ptp_cpu_2_little_volt_1(void)
{
	return LAST_RR_VAL(ptp_cpu_2_little_volt_1);
}

u64 aee_rr_curr_ptp_cpu_2_little_volt_2(void)
{
	return LAST_RR_VAL(ptp_cpu_2_little_volt_2);
}

u64 aee_rr_curr_ptp_cpu_2_little_volt_3(void)
{
	return LAST_RR_VAL(ptp_cpu_2_little_volt_3);
}

u64 aee_rr_curr_ptp_cpu_little_volt(void)
{
	return LAST_RR_VAL(ptp_cpu_little_volt);
}

u64 aee_rr_curr_ptp_cpu_little_volt_1(void)
{
	return LAST_RR_VAL(ptp_cpu_little_volt_1);
}

u64 aee_rr_curr_ptp_cpu_little_volt_2(void)
{
	return LAST_RR_VAL(ptp_cpu_little_volt_2);
}

u64 aee_rr_curr_ptp_cpu_little_volt_3(void)
{
	return LAST_RR_VAL(ptp_cpu_little_volt_3);
}

u64 aee_rr_curr_ptp_cpu_cci_volt(void)
{
	return LAST_RR_VAL(ptp_cpu_cci_volt);
}

u64 aee_rr_curr_ptp_cpu_cci_volt_1(void)
{
	return LAST_RR_VAL(ptp_cpu_cci_volt_1);
}

u64 aee_rr_curr_ptp_cpu_cci_volt_2(void)
{
	return LAST_RR_VAL(ptp_cpu_cci_volt_2);
}

u64 aee_rr_curr_ptp_cpu_cci_volt_3(void)
{
	return LAST_RR_VAL(ptp_cpu_cci_volt_3);
}

u64 aee_rr_curr_ptp_gpu_volt(void)
{
	return LAST_RR_VAL(ptp_gpu_volt);
}

u64 aee_rr_curr_ptp_gpu_volt_1(void)
{
	return LAST_RR_VAL(ptp_gpu_volt_1);
}

u64 aee_rr_curr_ptp_gpu_volt_2(void)
{
	return LAST_RR_VAL(ptp_gpu_volt_2);
}

u64 aee_rr_curr_ptp_gpu_volt_3(void)
{
	return LAST_RR_VAL(ptp_gpu_volt_3);
}

u64 aee_rr_curr_ptp_temp(void)
{
	return LAST_RR_VAL(ptp_temp);
}

u8 aee_rr_curr_ptp_status(void)
{
	return LAST_RR_VAL(ptp_status);
}

u8 aee_rr_curr_eem_pi_offset(void)
{
	return LAST_RR_VAL(eem_pi_offset);
}

u8 aee_rr_curr_etc_status(void)
{
	return LAST_RR_VAL(etc_status);
}

u8 aee_rr_curr_etc_mode(void)
{
	return LAST_RR_VAL(etc_mode);
}

s8 aee_rr_curr_thermal_temp(int index)
{
	if (index < 0 || index >= thermal_num)
		return -127;
	else
		return LAST_RR_VAL(thermal_temp[index]);
}

u8 aee_rr_curr_thermal_status(void)
{
	return LAST_RR_VAL(thermal_status);
}

u8 aee_rr_curr_thermal_ATM_status(void)
{
	return LAST_RR_VAL(thermal_ATM_status);
}

u64 aee_rr_curr_thermal_ktime(void)
{
	return LAST_RR_VAL(thermal_ktime);
}

u32 aee_rr_curr_idvfs_ctrl_reg(void)
{
	return LAST_RR_VAL(idvfs_ctrl_reg);
}

u32 aee_rr_curr_idvfs_enable_cnt(void)
{
	return LAST_RR_VAL(idvfs_enable_cnt);
}

u32 aee_rr_curr_idvfs_swreq_cnt(void)
{
	return LAST_RR_VAL(idvfs_swreq_cnt);
}

u16 aee_rr_curr_idvfs_curr_volt(void)
{
	return LAST_RR_VAL(idvfs_curr_volt);
}

u16 aee_rr_curr_idvfs_sram_ldo(void)
{
	return LAST_RR_VAL(idvfs_sram_ldo);
}

u16 aee_rr_curr_idvfs_swavg_curr_pct_x100(void)
{
	return LAST_RR_VAL(idvfs_swavg_curr_pct_x100);
}

u16 aee_rr_curr_idvfs_swreq_curr_pct_x100(void)
{
	return LAST_RR_VAL(idvfs_swreq_curr_pct_x100);
}

u16 aee_rr_curr_idvfs_swreq_next_pct_x100(void)
{
	return LAST_RR_VAL(idvfs_swreq_next_pct_x100);
}

u8 aee_rr_curr_idvfs_state_manchine(void)
{
	return LAST_RR_VAL(idvfs_state_manchine);
}

u32 aee_rr_curr_ocp_target_limit(int id)
{
	if (id < 0 || id >= 4)
		return 0;
	else
		return LAST_RR_VAL(ocp_target_limit[id]);
}

u8 aee_rr_curr_ocp_enable(void)
{
	return LAST_RR_VAL(ocp_enable);
}

void aee_rr_rec_scp_pc(u32 val)
{
	if (!ram_console_init_done || !ram_console_buffer)
		return;
	LAST_RR_SET(scp_pc, val);
}

uint32_t aee_rr_curr_scp_pc(void)
{
	return LAST_RR_VAL(scp_pc);
}

void aee_rr_rec_scp_lr(u32 val)
{
	if (!ram_console_init_done || !ram_console_buffer)
		return;
	LAST_RR_SET(scp_lr, val);
}

uint32_t aee_rr_curr_scp_lr(void)
{
	return LAST_RR_VAL(scp_lr);
}

__weak uint32_t scp_dump_pc(void)
{
	return 0;
}

__weak uint32_t scp_dump_lr(void)
{
	return 0;
}

void aee_rr_rec_scp(void)
{
	u32 pc = scp_dump_pc();
	u32 lr = scp_dump_lr();

	aee_rr_rec_scp_pc(pc);
	aee_rr_rec_scp_lr(lr);
}

void aee_rr_rec_last_init_func(unsigned long val)
{
	if (!ram_console_init_done || !ram_console_buffer)
		return;
	if (LAST_RR_VAL(last_init_func) == ~(unsigned long)(0))
		return;
	LAST_RR_SET(last_init_func, val);
}

void aee_rr_rec_last_async_func(unsigned long val)
{
	if (!ram_console_init_done || !ram_console_buffer)
		return;
	if (LAST_RR_VAL(last_async_func) == ~(unsigned long)(0))
		return;
	LAST_RR_SET(last_async_func, val);
}

void aee_rr_rec_last_sync_func(unsigned long val)
{
	if (!ram_console_init_done || !ram_console_buffer)
		return;
	if (LAST_RR_VAL(last_sync_func) == ~(unsigned long)(0))
		return;
	LAST_RR_SET(last_sync_func, val);
}


void aee_rr_rec_set_bit_pmic_ext_buck(int bit, int loc)
{
	int8_t rr_pmic_ext_buck;

	if (!ram_console_init_done || !ram_console_buffer)
		return;
	if ((bit != 0 && bit != 1) || loc > 7)
		return;
	rr_pmic_ext_buck = LAST_RR_VAL(pmic_ext_buck);
	if (bit == 1)
		rr_pmic_ext_buck |= (1 << loc);
	else
		rr_pmic_ext_buck &= ~(1 << loc);
	LAST_RR_SET(pmic_ext_buck, rr_pmic_ext_buck);
}

void aee_rr_rec_hang_detect_timeout_count(unsigned int val)
{
	if (!ram_console_init_done || !ram_console_buffer)
		return;
	LAST_RR_SET(hang_detect_timeout_count, val);
}

unsigned long *aee_rr_rec_gz_irq_pa(void)
{
	if (ram_console_buffer_pa)
		return (unsigned long *)&RR_LINUX_PA->gz_irq;
	else
		return NULL;
}

void aee_rr_rec_drcc_dbg_info(uint32_t ret, uint32_t off, uint64_t ts)
{
	if (!ram_console_init_done || !ram_console_buffer)
		return;
	LAST_RR_SET(drcc_dbg_ret, ret);
	LAST_RR_SET(drcc_dbg_off, off);
	LAST_RR_SET(drcc_dbg_ts, ts);
}


void aee_rr_rec_suspend_debug_flag(u32 val)
{
	if (!ram_console_init_done || !ram_console_buffer)
		return;
	LAST_RR_SET(suspend_debug_flag, val);
}

/* aee sram flags print */
int aee_rr_last_fiq_step(void)
{
	return LAST_RRR_VAL(fiq_step);
}

typedef void (*last_rr_show_t) (struct seq_file *m);
typedef void (*last_rr_show_cpu_t) (struct seq_file *m, int cpu);

void aee_rr_show_wdt_status(struct seq_file *m)
{
	unsigned int wdt_status;
	struct ram_console_buffer *buffer = ram_console_old;

	if (buffer->off_pl == 0 || buffer->off_pl + ALIGN(buffer->sz_pl, 64)
			!= buffer->off_lpl) {
		/* workaround for compatibility to old preloader & lk (OTA) */
		wdt_status = *((unsigned char *)buffer + 12);
	} else
		wdt_status = LAST_RRPL_VAL(wdt_status);
	seq_printf(m, "WDT status: %d", wdt_status);
}

void aee_rr_show_fiq_step(struct seq_file *m)
{
	seq_printf(m, " fiq step: %u ", LAST_RRR_VAL(fiq_step));
}

void aee_rr_show_exp_type(struct seq_file *m)
{
	unsigned int exp_type = LAST_RRR_VAL(exp_type);

	seq_printf(m, " exception type: %u\n",
		   RAM_CONSOLE_EXP_TYPE_DEC(exp_type));
}

void aee_rr_show_kaslr_offset(struct seq_file *m)
{
	uint64_t kaslr_offset = LAST_RRR_VAL(kaslr_offset);

	seq_printf(m, "Kernel Offset: 0x%llx\n", kaslr_offset);
}

void aee_rr_show_ram_console_buffer_addr(struct seq_file *m)
{
	uint64_t ram_console_buffer_addr;

	ram_console_buffer_addr = LAST_RRR_VAL(ram_console_buffer_addr);
	seq_printf(m, "&ram_console_buffer: 0x%llx\n", ram_console_buffer_addr);
}

void aee_rr_show_last_irq_enter(struct seq_file *m, int cpu)
{
	seq_printf(m, "  irq: enter(%d, ", LAST_RRR_VAL(last_irq_enter[cpu]));
}

void aee_rr_show_jiffies_last_irq_enter(struct seq_file *m, int cpu)
{
	seq_printf(m, "%llu) ", LAST_RRR_VAL(jiffies_last_irq_enter[cpu]));
}

void aee_rr_show_last_irq_exit(struct seq_file *m, int cpu)
{
	seq_printf(m, "quit(%d, ", LAST_RRR_VAL(last_irq_exit[cpu]));
}

void aee_rr_show_jiffies_last_irq_exit(struct seq_file *m, int cpu)
{
	seq_printf(m, "%llu)\n", LAST_RRR_VAL(jiffies_last_irq_exit[cpu]));
}

void aee_rr_show_hotplug_footprint(struct seq_file *m, int cpu)
{
	seq_printf(m, "  hotplug: %d\n", LAST_RRR_VAL(hotplug_footprint[cpu]));
}

void aee_rr_show_hotplug_status(struct seq_file *m)
{
	seq_printf(m, "CPU notifier status: %d, %d, 0x%llx, %llu\n",
		   LAST_RRR_VAL(hotplug_cpu_event),
		   LAST_RRR_VAL(hotplug_cb_index),
		   LAST_RRR_VAL(hotplug_cb_fp),
		   LAST_RRR_VAL(hotplug_cb_times));
}

void aee_rr_show_hps_status(struct seq_file *m)
{
	seq_printf(m, "CPU HPS footprint: %llu, 0x%x, %d, %llu\n",
		   LAST_RRR_VAL(hps_cb_enter_times),
		   LAST_RRR_VAL(hps_cb_cpu_bitmask),
		   LAST_RRR_VAL(hps_cb_footprint),
		   LAST_RRR_VAL(hps_cb_fp_times));
}

void aee_rr_show_hotplug_caller_callee_status(struct seq_file *m)
{
	seq_printf(m, "CPU Hotplug: caller CPU%d, callee CPU%d\n",
		   LAST_RRR_VAL(cpu_caller),
		   LAST_RRR_VAL(cpu_callee));
}

void aee_rr_show_hotplug_up_prepare_ktime(struct seq_file *m)
{
	seq_printf(m, "CPU_UP_PREPARE: %lld\n",
			LAST_RRR_VAL(cpu_up_prepare_ktime));
}

void aee_rr_show_hotplug_starting_ktime(struct seq_file *m)
{
	seq_printf(m, "CPU_STARTING: %lld\n", LAST_RRR_VAL(cpu_starting_ktime));
}

void aee_rr_show_hotplug_online_ktime(struct seq_file *m)
{
	seq_printf(m, "CPU_ONLINE: %lld\n", LAST_RRR_VAL(cpu_online_ktime));
}

void aee_rr_show_hotplug_down_prepare_ktime(struct seq_file *m)
{
	seq_printf(m, "CPU_DOWN_PREPARE: %lld\n",
			LAST_RRR_VAL(cpu_down_prepare_ktime));
}

void aee_rr_show_hotplug_dying_ktime(struct seq_file *m)
{
	seq_printf(m, "CPU_DYING: %lld\n", LAST_RRR_VAL(cpu_dying_ktime));
}

void aee_rr_show_hotplug_dead_ktime(struct seq_file *m)
{
	seq_printf(m, "CPU_DEAD: %lld\n", LAST_RRR_VAL(cpu_dead_ktime));
}

void aee_rr_show_hotplug_post_dead_ktime(struct seq_file *m)
{
	seq_printf(m, "CPU_POST_DEAD: %lld\n",
			LAST_RRR_VAL(cpu_post_dead_ktime));
}

void aee_rr_show_mcdi(struct seq_file *m)
{
	seq_printf(m, "mcdi_wfi: 0x%x\n", LAST_RRR_VAL(mcdi_wfi));
}

void aee_rr_show_mcdi_r15(struct seq_file *m)
{
	seq_printf(m, "mcdi_r15: 0x%x\n", LAST_RRR_VAL(mcdi_r15));
}

void aee_rr_show_deepidle(struct seq_file *m)
{
	seq_printf(m, "deepidle: 0x%x\n", LAST_RRR_VAL(deepidle_data));
}

void aee_rr_show_sodi3(struct seq_file *m)
{
	seq_printf(m, "sodi3: 0x%x\n", LAST_RRR_VAL(sodi3_data));
}

void aee_rr_show_sodi(struct seq_file *m)
{
	seq_printf(m, "sodi: 0x%x\n", LAST_RRR_VAL(sodi_data));
}

void aee_rr_show_mcsodi(struct seq_file *m)
{
	seq_printf(m, "mcsodi: 0x%x\n", LAST_RRR_VAL(mcsodi_data));
}

void aee_rr_show_spm_suspend(struct seq_file *m)
{
	seq_printf(m, "spm_suspend: 0x%x\n", LAST_RRR_VAL(spm_suspend_data));
}

void aee_rr_show_spm_common_scenario(struct seq_file *m)
{
	seq_printf(m, "spm_common_scenario: 0x%x\n",
			LAST_RRR_VAL(spm_common_scenario_data));
}

void aee_rr_show_mtk_cpuidle_footprint(struct seq_file *m, int cpu)
{
	seq_printf(m, "  mtk_cpuidle_footprint: 0x%x\n",
			LAST_RRR_VAL(mtk_cpuidle_footprint[cpu]));
}

void aee_rr_show_mcdi_footprint(struct seq_file *m, int cpu)
{
	seq_printf(m, "  mcdi footprint: 0x%x\n",
			LAST_RRR_VAL(mcdi_footprint[cpu]));
}

void aee_rr_show_clk(struct seq_file *m)
{
	int i = 0;

	for (i = 0; i < 8; i++)
		seq_printf(m, "clk_data: 0x%x\n", LAST_RRR_VAL(clk_data[i]));
}

void aee_rr_show_fiq_cache_step(struct seq_file *m)
{
	seq_printf(m, "fiq_cache_step: %d\n", LAST_RRR_VAL(fiq_cache_step));
}

void aee_rr_show_vcore_dvfs_opp(struct seq_file *m)
{
	seq_printf(m, "vcore_dvfs_opp: 0x%x\n", LAST_RRR_VAL(vcore_dvfs_opp));
}

void aee_rr_show_vcore_dvfs_status(struct seq_file *m)
{
	seq_printf(m, "vcore_dvfs_status: 0x%x\n",
			LAST_RRR_VAL(vcore_dvfs_status));
}

void aee_rr_show_ppm_cluster_limit(struct seq_file *m)
{
	int i = 0;

	for (i = 0; i < 8; i++)
		seq_printf(m, "ppm_cluster_limit: 0x%08x\n",
				LAST_RRR_VAL(ppm_cluster_limit[i]));
}

void aee_rr_show_ppm_step(struct seq_file *m)
{
	seq_printf(m, "ppm_step: 0x%x\n", LAST_RRR_VAL(ppm_step));
}

void aee_rr_show_ppm_cur_state(struct seq_file *m)
{
	seq_printf(m, "ppm_cur_state: 0x%x\n", LAST_RRR_VAL(ppm_cur_state));
}

void aee_rr_show_ppm_min_pwr_bgt(struct seq_file *m)
{
	seq_printf(m, "ppm_min_pwr_bgt: %d\n", LAST_RRR_VAL(ppm_min_pwr_bgt));
}

void aee_rr_show_ppm_policy_mask(struct seq_file *m)
{
	seq_printf(m, "ppm_policy_mask: 0x%x\n", LAST_RRR_VAL(ppm_policy_mask));
}

void aee_rr_show_ppm_waiting_for_pbm(struct seq_file *m)
{
	seq_printf(m, "ppm_waiting_for_pbm: 0x%x\n",
			LAST_RRR_VAL(ppm_waiting_for_pbm));
}

void aee_rr_show_cpu_dvfs_vproc_big(struct seq_file *m)
{
	seq_printf(m, "cpu_dvfs_vproc_big: 0x%x\n",
			LAST_RRR_VAL(cpu_dvfs_vproc_big));
}

void aee_rr_show_cpu_dvfs_vproc_little(struct seq_file *m)
{
	seq_printf(m, "cpu_dvfs_vproc_little: 0x%x\n",
			LAST_RRR_VAL(cpu_dvfs_vproc_little));
}

void aee_rr_show_cpu_dvfs_oppidx(struct seq_file *m)
{
	seq_printf(m, "cpu_dvfs_oppidx: little = 0x%x\n",
			LAST_RRR_VAL(cpu_dvfs_oppidx) & 0xF);
	seq_printf(m, "cpu_dvfs_oppidx: big = 0x%x\n",
			(LAST_RRR_VAL(cpu_dvfs_oppidx) >> 4) & 0xF);
}

void aee_rr_show_cpu_dvfs_cci_oppidx(struct seq_file *m)
{
	seq_printf(m, "cpu_dvfs_oppidx: cci = 0x%x\n",
			LAST_RRR_VAL(cpu_dvfs_cci_oppidx) & 0xF);
}

void aee_rr_show_cpu_dvfs_status(struct seq_file *m)
{
	seq_printf(m, "cpu_dvfs_status: 0x%x\n", LAST_RRR_VAL(cpu_dvfs_status));
}

void aee_rr_show_cpu_dvfs_step(struct seq_file *m)
{
	seq_printf(m, "cpu_dvfs_step: 0x%x\n", LAST_RRR_VAL(cpu_dvfs_step));
}

void aee_rr_show_cpu_dvfs_pbm_step(struct seq_file *m)
{
	seq_printf(m, "cpu_dvfs_pbm_step: 0x%x\n",
			LAST_RRR_VAL(cpu_dvfs_pbm_step));
}

void aee_rr_show_cpu_dvfs_cb(struct seq_file *m)
{
	seq_printf(m, "cpu_dvfs_cb: 0x%x\n", LAST_RRR_VAL(cpu_dvfs_cb));
}

void aee_rr_show_cpufreq_cb(struct seq_file *m)
{
	seq_printf(m, "cpufreq_cb: 0x%x\n", LAST_RRR_VAL(cpufreq_cb));
}

void aee_rr_show_gpu_dvfs_vgpu(struct seq_file *m)
{
	seq_printf(m, "gpu_dvfs_vgpu: 0x%x\n", LAST_RRR_VAL(gpu_dvfs_vgpu));
}

void aee_rr_show_gpu_dvfs_oppidx(struct seq_file *m)
{
	seq_printf(m, "gpu_dvfs_oppidx: 0x%x\n", LAST_RRR_VAL(gpu_dvfs_oppidx));
}

void aee_rr_show_gpu_dvfs_status(struct seq_file *m)
{
	seq_printf(m, "gpu_dvfs_status: 0x%x\n", LAST_RRR_VAL(gpu_dvfs_status));
}

void aee_rr_show_drcc_0(struct seq_file *m)
{
	seq_printf(m, "drcc_0 = 0x%X\n", LAST_RRR_VAL(drcc_0));
}

void aee_rr_show_drcc_1(struct seq_file *m)
{
	seq_printf(m, "drcc_1 = 0x%X\n", LAST_RRR_VAL(drcc_1));
}

void aee_rr_show_drcc_2(struct seq_file *m)
{
	seq_printf(m, "drcc_2 = 0x%X\n", LAST_RRR_VAL(drcc_2));
}

void aee_rr_show_drcc_3(struct seq_file *m)
{
	seq_printf(m, "drcc_3 = 0x%X\n", LAST_RRR_VAL(drcc_3));
}

void aee_rr_show_ptp_devinfo_0(struct seq_file *m)
{
	seq_printf(m, "EEM devinfo0 = 0x%X\n", LAST_RRR_VAL(ptp_devinfo_0));
}

void aee_rr_show_ptp_devinfo_1(struct seq_file *m)
{
	seq_printf(m, "EEM devinfo1 = 0x%X\n", LAST_RRR_VAL(ptp_devinfo_1));
}

void aee_rr_show_ptp_devinfo_2(struct seq_file *m)
{
	seq_printf(m, "EEM devinfo2 = 0x%X\n", LAST_RRR_VAL(ptp_devinfo_2));
}

void aee_rr_show_ptp_devinfo_3(struct seq_file *m)
{
	seq_printf(m, "EEM devinfo3 = 0x%X\n", LAST_RRR_VAL(ptp_devinfo_3));
}

void aee_rr_show_ptp_devinfo_4(struct seq_file *m)
{
	seq_printf(m, "EEM devinfo4 = 0x%X\n", LAST_RRR_VAL(ptp_devinfo_4));
}

void aee_rr_show_ptp_devinfo_5(struct seq_file *m)
{
	seq_printf(m, "EEM devinfo5 = 0x%X\n", LAST_RRR_VAL(ptp_devinfo_5));
}

void aee_rr_show_ptp_devinfo_6(struct seq_file *m)
{
	seq_printf(m, "EEM devinfo6 = 0x%X\n", LAST_RRR_VAL(ptp_devinfo_6));
}

void aee_rr_show_ptp_devinfo_7(struct seq_file *m)
{
	seq_printf(m, "EEM devinfo7 = 0x%X\n", LAST_RRR_VAL(ptp_devinfo_7));
}

void aee_rr_show_ptp_e0(struct seq_file *m)
{
	seq_printf(m, "M_HW_RES0 = 0x%X\n", LAST_RRR_VAL(ptp_e0));
}

void aee_rr_show_ptp_e1(struct seq_file *m)
{
	seq_printf(m, "M_HW_RES1 = 0x%X\n", LAST_RRR_VAL(ptp_e1));
}

void aee_rr_show_ptp_e2(struct seq_file *m)
{
	seq_printf(m, "M_HW_RES2 = 0x%X\n", LAST_RRR_VAL(ptp_e2));
}

void aee_rr_show_ptp_e3(struct seq_file *m)
{
	seq_printf(m, "M_HW_RES3 = 0x%X\n", LAST_RRR_VAL(ptp_e3));
}

void aee_rr_show_ptp_e4(struct seq_file *m)
{
	seq_printf(m, "M_HW_RES4 = 0x%X\n", LAST_RRR_VAL(ptp_e4));
}

void aee_rr_show_ptp_e5(struct seq_file *m)
{
	seq_printf(m, "M_HW_RES5 = 0x%X\n", LAST_RRR_VAL(ptp_e5));
}

void aee_rr_show_ptp_e6(struct seq_file *m)
{
	seq_printf(m, "M_HW_RES6 = 0x%X\n", LAST_RRR_VAL(ptp_e6));
}

void aee_rr_show_ptp_e7(struct seq_file *m)
{
	seq_printf(m, "M_HW_RES7 = 0x%X\n", LAST_RRR_VAL(ptp_e7));
}

void aee_rr_show_ptp_e8(struct seq_file *m)
{
	seq_printf(m, "M_HW_RES8 = 0x%X\n", LAST_RRR_VAL(ptp_e8));
}

void aee_rr_show_ptp_e9(struct seq_file *m)
{
	seq_printf(m, "M_HW_RES9 = 0x%X\n", LAST_RRR_VAL(ptp_e9));
}

void aee_rr_show_ptp_e10(struct seq_file *m)
{
	seq_printf(m, "M_HW_RESA = 0x%X\n", LAST_RRR_VAL(ptp_e10));
}

void aee_rr_show_ptp_e11(struct seq_file *m)
{
	seq_printf(m, "M_HW_RESB = 0x%X\n", LAST_RRR_VAL(ptp_e11));
}

void aee_rr_show_ptp_vboot(struct seq_file *m)
{
	int i;

	for (i = 0; i < 8; i++)
		seq_printf(m, "ptp_bank_[%d]_vboot = 0x%llx\n", i,
			(LAST_RRR_VAL(ptp_vboot) >> (i * 8)) & 0xFF);
}

void aee_rr_show_ptp_cpu_big_volt(struct seq_file *m)
{
	int i;

	for (i = 0; i < 8; i++)
		seq_printf(m, "ptp_cpu_big_volt[%d] = %llx\n", i,
			(LAST_RRR_VAL(ptp_cpu_big_volt) >> (i * 8)) & 0xFF);
}

void aee_rr_show_ptp_cpu_big_volt_1(struct seq_file *m)
{
	int i;

	for (i = 0; i < 8; i++)
		seq_printf(m, "ptp_cpu_big_volt_1[%d] = %llx\n", i,
			(LAST_RRR_VAL(ptp_cpu_big_volt_1) >> (i * 8)) & 0xFF);
}

void aee_rr_show_ptp_cpu_big_volt_2(struct seq_file *m)
{
	int i;

	for (i = 0; i < 8; i++)
		seq_printf(m, "ptp_cpu_big_volt_2[%d] = %llx\n", i,
			(LAST_RRR_VAL(ptp_cpu_big_volt_2) >> (i * 8)) & 0xFF);
}

void aee_rr_show_ptp_cpu_big_volt_3(struct seq_file *m)
{
	int i;

	for (i = 0; i < 8; i++)
		seq_printf(m, "ptp_cpu_big_volt_3[%d] = %llx\n", i,
			(LAST_RRR_VAL(ptp_cpu_big_volt_3) >> (i * 8)) & 0xFF);
}

void aee_rr_show_ptp_cpu_2_little_volt(struct seq_file *m)
{
	int i;

	for (i = 0; i < 8; i++)
		seq_printf(m, "ptp_cpu_2_little_volt[%d] = %llx\n", i,
			(LAST_RRR_VAL(ptp_cpu_2_little_volt) >>
			(i * 8)) & 0xFF);
}

void aee_rr_show_ptp_cpu_2_little_volt_1(struct seq_file *m)
{
	int i;

	for (i = 0; i < 8; i++)
		seq_printf(m, "ptp_cpu_2_little_volt_1[%d] = %llx\n", i,
			(LAST_RRR_VAL(ptp_cpu_2_little_volt_1) >>
			(i * 8)) & 0xFF);
}

void aee_rr_show_ptp_cpu_2_little_volt_2(struct seq_file *m)
{
	int i;

	for (i = 0; i < 8; i++)
		seq_printf(m, "ptp_cpu_2_little_volt_2[%d] = %llx\n", i,
			(LAST_RRR_VAL(ptp_cpu_2_little_volt_2) >>
			(i * 8)) & 0xFF);
}

void aee_rr_show_ptp_cpu_2_little_volt_3(struct seq_file *m)
{
	int i;

	for (i = 0; i < 8; i++)
		seq_printf(m, "ptp_cpu_2_little_volt_3[%d] = %llx\n", i,
			(LAST_RRR_VAL(ptp_cpu_2_little_volt_3) >>
			(i * 8)) & 0xFF);
}

void aee_rr_show_ptp_cpu_little_volt(struct seq_file *m)
{
	int i;

	for (i = 0; i < 8; i++)
		seq_printf(m, "ptp_cpu_little_volt[%d] = %llx\n", i,
			(LAST_RRR_VAL(ptp_cpu_little_volt) >> (i * 8)) & 0xFF);
}

void aee_rr_show_ptp_cpu_little_volt_1(struct seq_file *m)
{
	int i;

	for (i = 0; i < 8; i++)
		seq_printf(m, "ptp_cpu_little_volt_1[%d] = %llx\n", i,
			(LAST_RRR_VAL(ptp_cpu_little_volt_1) >>
			(i * 8)) & 0xFF);
}

void aee_rr_show_ptp_cpu_little_volt_2(struct seq_file *m)
{
	int i;

	for (i = 0; i < 8; i++)
		seq_printf(m, "ptp_cpu_little_volt_2[%d] = %llx\n", i,
			(LAST_RRR_VAL(ptp_cpu_little_volt_2) >>
			(i * 8)) & 0xFF);
}

void aee_rr_show_ptp_cpu_little_volt_3(struct seq_file *m)
{
	int i;

	for (i = 0; i < 8; i++)
		seq_printf(m, "ptp_cpu_little_volt_3[%d] = %llx\n", i,
			(LAST_RRR_VAL(ptp_cpu_little_volt_3) >>
			 (i * 8)) & 0xFF);
}

void aee_rr_show_ptp_cpu_cci_volt(struct seq_file *m)
{
	int i;

	for (i = 0; i < 8; i++)
		seq_printf(m, "ptp_cpu_cci_volt[%d] = %llx\n", i,
			(LAST_RRR_VAL(ptp_cpu_cci_volt) >> (i * 8)) & 0xFF);
}

void aee_rr_show_ptp_cpu_cci_volt_1(struct seq_file *m)
{
	int i;

	for (i = 0; i < 8; i++)
		seq_printf(m, "ptp_cpu_cci_volt_1[%d] = %llx\n", i,
			(LAST_RRR_VAL(ptp_cpu_cci_volt_1) >> (i * 8)) & 0xFF);
}

void aee_rr_show_ptp_cpu_cci_volt_2(struct seq_file *m)
{
	int i;

	for (i = 0; i < 8; i++)
		seq_printf(m, "ptp_cpu_cci_volt_2[%d] = %llx\n", i,
			(LAST_RRR_VAL(ptp_cpu_cci_volt_2) >> (i * 8)) & 0xFF);
}

void aee_rr_show_ptp_cpu_cci_volt_3(struct seq_file *m)
{
	int i;

	for (i = 0; i < 8; i++)
		seq_printf(m, "ptp_cpu_cci_volt_3[%d] = %llx\n", i,
			(LAST_RRR_VAL(ptp_cpu_cci_volt_3) >> (i * 8)) & 0xFF);
}

void aee_rr_show_ptp_gpu_volt(struct seq_file *m)
{
	int i;

	for (i = 0; i < 8; i++)
		seq_printf(m, "ptp_gpu_volt[%d] = %llx\n", i,
			   (LAST_RRR_VAL(ptp_gpu_volt) >> (i * 8)) & 0xFF);
}

void aee_rr_show_ptp_gpu_volt_1(struct seq_file *m)
{
	int i;

	for (i = 0; i < 8; i++)
		seq_printf(m, "ptp_gpu_volt_1[%d] = %llx\n", i,
			   (LAST_RRR_VAL(ptp_gpu_volt_1) >> (i * 8)) & 0xFF);
}

void aee_rr_show_ptp_gpu_volt_2(struct seq_file *m)
{
	int i;

	for (i = 0; i < 8; i++)
		seq_printf(m, "ptp_gpu_volt_2[%d] = %llx\n", i,
			   (LAST_RRR_VAL(ptp_gpu_volt_2) >> (i * 8)) & 0xFF);
}

void aee_rr_show_ptp_gpu_volt_3(struct seq_file *m)
{
	int i;

	for (i = 0; i < 8; i++)
		seq_printf(m, "ptp_gpu_volt_3[%d] = %llx\n", i,
			   (LAST_RRR_VAL(ptp_gpu_volt_3) >> (i * 8)) & 0xFF);
}

void aee_rr_show_ptp_temp(struct seq_file *m)
{
	seq_printf(m, "ptp_temp: big = %llx\n", LAST_RRR_VAL(ptp_temp) & 0xFF);
	seq_printf(m, "ptp_temp: GPU = %llx\n",
			(LAST_RRR_VAL(ptp_temp) >> 8) & 0xFF);
	seq_printf(m, "ptp_temp: little = %llx\n",
			(LAST_RRR_VAL(ptp_temp) >> 16) & 0xFF);
}

void aee_rr_show_thermal_temp(struct seq_file *m)
{
	int i;

	for (i = 0; i < thermal_num; i++)
		seq_printf(m, "thermal_temp%d = %d\n",
				(i + 1), LAST_RRR_VAL(thermal_temp[i]));
}

void aee_rr_show_ptp_status(struct seq_file *m)
{
	seq_printf(m, "ptp_status: 0x%x\n", LAST_RRR_VAL(ptp_status));
}

void aee_rr_show_eem_pi_offset(struct seq_file *m)
{
	seq_printf(m, "eem_pi_offset : 0x%x\n", LAST_RRR_VAL(eem_pi_offset));
}

void aee_rr_show_etc_status(struct seq_file *m)
{
	seq_printf(m, "etc_status : 0x%x\n", LAST_RRR_VAL(etc_status));
}

void aee_rr_show_etc_mode(struct seq_file *m)
{
	seq_printf(m, "etc_mode : 0x%x\n", LAST_RRR_VAL(etc_mode));
}

void aee_rr_show_idvfs_ctrl_reg(struct seq_file *m)
{
	seq_printf(m, "idvfs_ctrl_reg = 0x%x\n", LAST_RRR_VAL(idvfs_ctrl_reg));
	seq_printf(m, "idvfs_Endis = %s\n",
			(LAST_RRR_VAL(idvfs_ctrl_reg) & 0x1) ?
			"Enable" : "Disable");
	seq_printf(m, "idvfs_SWP_Endis = %s\n",
			(LAST_RRR_VAL(idvfs_ctrl_reg) & 0x2) ?
			"Enable" : "Disable");
	seq_printf(m, "idvfs_OCP_Endis = %s\n",
			(LAST_RRR_VAL(idvfs_ctrl_reg) & 0x4) ?
			"Enable" : "Disable");
	seq_printf(m, "idvfs_OTP_Endis = %s\n",
			(LAST_RRR_VAL(idvfs_ctrl_reg) & 0x8) ?
			"Enable" : "Disable");
}

void aee_rr_show_idvfs_enable_cnt(struct seq_file *m)
{
	seq_printf(m, "idvfs_enable_cnt = %u\n",
			LAST_RRR_VAL(idvfs_enable_cnt));
}

void aee_rr_show_idvfs_swreq_cnt(struct seq_file *m)
{
	seq_printf(m, "idvfs_swreq_cnt = %u\n", LAST_RRR_VAL(idvfs_swreq_cnt));
}

void aee_rr_show_idvfs_curr_volt(struct seq_file *m)
{
	seq_printf(m, "idvfs_curr_volt = %umv, 0x5e = 0x%x\n",
		(((LAST_RRR_VAL(idvfs_curr_volt) & 0xff) * 10) + 300),
		(LAST_RRR_VAL(idvfs_curr_volt) >> 8));
}

void aee_rr_show_idvfs_sram_ldo(struct seq_file *m)
{
	seq_printf(m, "idvfs_sram_ldo = %umv\n", LAST_RRR_VAL(idvfs_sram_ldo));
}

void aee_rr_show_idvfs_swavg_curr_pct_x100(struct seq_file *m)
{
	seq_printf(m, "idvfs_swavg_curr_pct_x100 = %u, %uMHz\n",
		LAST_RRR_VAL(idvfs_swavg_curr_pct_x100),
		(LAST_RRR_VAL(idvfs_swavg_curr_pct_x100) / 4));
}

void aee_rr_show_idvfs_swreq_curr_pct_x100(struct seq_file *m)
{
	seq_printf(m, "idvfs_swreq_curr_pct_x100 = %u, %uMHz\n",
		LAST_RRR_VAL(idvfs_swreq_curr_pct_x100),
		(LAST_RRR_VAL(idvfs_swreq_curr_pct_x100) / 4));
}

void aee_rr_show_idvfs_swreq_next_pct_x100(struct seq_file *m)
{
	seq_printf(m, "idvfs_swreq_next_pct_x100 = %u, %uMHz\n",
		LAST_RRR_VAL(idvfs_swreq_next_pct_x100),
		(LAST_RRR_VAL(idvfs_swreq_next_pct_x100) / 4));
}

void aee_rr_show_idvfs_state_manchine(struct seq_file *m)
{
/*
 * 0: disable finish
 * 1: enable finish
 * 2: enable start
 * 3: disable start
 * 4: SWREQ start
 * 5: disable and wait SWREQ finish
 * 6: SWREQ finish can into disable
 */

	switch (LAST_RRR_VAL(idvfs_state_manchine)) {
	case 0:
		seq_puts(m, "idvfs state = 0: disable finish\n");
		break;
	case 1:
		seq_puts(m, "idvfs state = 1: enable finish\n");
		break;
	case 2:
		seq_puts(m, "idvfs state = 2: enable start\n");
		break;
	case 3:
		seq_puts(m, "idvfs state = 3: disable start\n");
		break;
	case 4:
		seq_puts(m, "idvfs state = 4: SWREQ start\n");
		break;
	case 5:
		seq_puts(m, "idvfs state = 5: disable and wait SWREQ finish\n");
		break;
	case 6:
		seq_puts(m, "idvfs state = 6: SWREQ finish can into disable\n");
		break;
	default:
		seq_printf(m, "idvfs state = %u, unknown state manchine\n",
				LAST_RRR_VAL(idvfs_state_manchine));
		break;
	}
}

void aee_rr_show_ocp_target_limit(struct seq_file *m)
{
	int i = 0;

	for (i = 0; i < 4; i++)
		seq_printf(m, "ocp_target_limit[%d]: %d\n", i,
				LAST_RRR_VAL(ocp_target_limit[i]));
}

void aee_rr_show_ocp_enable(struct seq_file *m)
{
	seq_printf(m, "ocp_enable = 0x%x\n", LAST_RRR_VAL(ocp_enable));
}

void aee_rr_show_thermal_status(struct seq_file *m)
{
	seq_printf(m, "thermal_status: %d\n", LAST_RRR_VAL(thermal_status));
}

void aee_rr_show_thermal_ATM_status(struct seq_file *m)
{
	seq_printf(m, "thermal_ATM_status: %d\n",
			LAST_RRR_VAL(thermal_ATM_status));
}

void aee_rr_show_thermal_ktime(struct seq_file *m)
{
	seq_printf(m, "thermal_ktime: %lld\n", LAST_RRR_VAL(thermal_ktime));
}

void aee_rr_show_scp_pc(struct seq_file *m)
{
	seq_printf(m, "scp_pc: 0x%x\n", LAST_RRR_VAL(scp_pc));
}

void aee_rr_show_scp_lr(struct seq_file *m)
{
	seq_printf(m, "scp_lr: 0x%x\n", LAST_RRR_VAL(scp_lr));
}

void aee_rr_show_last_init_func(struct seq_file *m)
{
	seq_printf(m, "last init function: 0x%lx\n",
			LAST_RRR_VAL(last_init_func));
}

void aee_rr_show_last_sync_func(struct seq_file *m)
{
	seq_printf(m, "last sync function: 0x%lx\n",
			LAST_RRR_VAL(last_sync_func));
}

void aee_rr_show_last_async_func(struct seq_file *m)
{
	seq_printf(m, "last async function: 0x%lx\n",
			LAST_RRR_VAL(last_async_func));
}

void aee_rr_show_pmic_ext_buck(struct seq_file *m)
{
	seq_printf(m, "pmic & external buck: 0x%x\n",
			LAST_RRR_VAL(pmic_ext_buck));
}

void aee_rr_show_hang_detect_timeout_count(struct seq_file *m)
{
	seq_printf(m, "hang detect time out: 0x%x\n",
			LAST_RRR_VAL(hang_detect_timeout_count));
}

void aee_rr_show_gz_irq(struct seq_file *m)
{
	seq_printf(m, "GZ IRQ: 0x%x\n", LAST_RRR_VAL(gz_irq));
}

void aee_rr_show_drcc_dbg_info(struct seq_file *m)
{
	seq_printf(m, "DRCC dbg info result: 0x%x\n",
			LAST_RRR_VAL(drcc_dbg_ret));
	seq_printf(m, "DRCC dbg info offset: 0x%x\n",
			LAST_RRR_VAL(drcc_dbg_off));
	seq_printf(m, "DRCC dbg info timestamp: 0x%llx\n",
			LAST_RRR_VAL(drcc_dbg_ts));
}

__weak uint32_t get_vcore_dvfs_sram_debug_regs(uint32_t index)
{
	return 0;
}

void aee_rr_show_vcore_dvfs_debug_regs(struct seq_file *m)
{
	int i;
	uint32_t count = get_vcore_dvfs_sram_debug_regs(0);

	for (i = 0; i < count; i++)
		seq_printf(m, "vcore dvfs debug regs(index %d) = 0x%x\n",
				i + 1, get_vcore_dvfs_sram_debug_regs(i + 1));
}

__weak uint32_t get_suspend_debug_flag(void)
{
	return LAST_RR_VAL(suspend_debug_flag);
}

void aee_rr_show_suspend_debug_flag(struct seq_file *m)
{
	uint32_t flag = get_suspend_debug_flag();

	seq_printf(m, "SPM Suspend debug = 0x%x\n", flag);
}

__weak uint32_t get_suspend_debug_regs(uint32_t index)
{
	return 0;
}

void aee_rr_show_suspend_debug_regs(struct seq_file *m)
{
	int i;
	uint32_t count = get_suspend_debug_regs(0);

	for (i = 0; i < count; i++)
		seq_printf(m, "SPM Suspend debug regs(index %d) = 0x%x\n",
				i + 1, get_suspend_debug_regs(i + 1));
}

__weak void *get_spm_firmware_version(uint32_t index)
{
	return NULL;
}

void aee_rr_show_spm_firmware_version(struct seq_file *m)
{
	int i;
	uint32_t *ptr = (uint32_t *)get_spm_firmware_version(0);

	if (ptr != NULL)
		for (i = 0; i < *ptr; i++)
			seq_printf(m, "SPM firmware version(index %d) = %s\n",
				i + 1,
				(char *)get_spm_firmware_version(i + 1));
}

int __weak mt_reg_dump(char *buf)
{
	return 1;
}

void aee_rr_show_last_pc(struct seq_file *m)
{
	char *reg_buf = kmalloc(4096, GFP_KERNEL);

	if (reg_buf) {
		if (mt_reg_dump(reg_buf) == 0)
			seq_printf(m, "%s\n", reg_buf);
		kfree(reg_buf);
	}
}

int __weak mt_lastbus_dump(char *buf)
{
	return 1;
}

void aee_rr_show_last_bus(struct seq_file *m)
{
	char *reg_buf = kmalloc(4096, GFP_KERNEL);

	if (reg_buf) {
		if (mt_lastbus_dump) {
			if (mt_lastbus_dump(reg_buf) == 0)
				seq_printf(m, "%s\n", reg_buf);
		}
		kfree(reg_buf);
	}
}


last_rr_show_t aee_rr_show[] = {
	aee_rr_show_wdt_status,
	aee_rr_show_fiq_step,
	aee_rr_show_exp_type,
	aee_rr_show_kaslr_offset,
	aee_rr_show_ram_console_buffer_addr,
	aee_rr_show_last_pc,
	aee_rr_show_last_bus,
	aee_rr_show_mcdi,
	aee_rr_show_mcdi_r15,
	aee_rr_show_suspend_debug_flag,
	aee_rr_show_suspend_debug_regs,
	aee_rr_show_spm_firmware_version,
	aee_rr_show_deepidle,
	aee_rr_show_sodi3,
	aee_rr_show_sodi,
	aee_rr_show_mcsodi,
	aee_rr_show_spm_suspend,
	aee_rr_show_spm_common_scenario,
	aee_rr_show_vcore_dvfs_opp,
	aee_rr_show_vcore_dvfs_status,
	aee_rr_show_vcore_dvfs_debug_regs,
	aee_rr_show_clk,
	aee_rr_show_fiq_cache_step,
	aee_rr_show_ppm_cluster_limit,
	aee_rr_show_ppm_step,
	aee_rr_show_ppm_cur_state,
	aee_rr_show_ppm_min_pwr_bgt,
	aee_rr_show_ppm_policy_mask,
	aee_rr_show_ppm_waiting_for_pbm,
	aee_rr_show_cpu_dvfs_vproc_big,
	aee_rr_show_cpu_dvfs_vproc_little,
	aee_rr_show_cpu_dvfs_oppidx,
	aee_rr_show_cpu_dvfs_cci_oppidx,
	aee_rr_show_cpu_dvfs_status,
	aee_rr_show_cpu_dvfs_step,
	aee_rr_show_cpu_dvfs_pbm_step,
	aee_rr_show_cpu_dvfs_cb,
	aee_rr_show_cpufreq_cb,
	aee_rr_show_gpu_dvfs_vgpu,
	aee_rr_show_gpu_dvfs_oppidx,
	aee_rr_show_gpu_dvfs_status,
	aee_rr_show_drcc_0,
	aee_rr_show_drcc_1,
	aee_rr_show_drcc_2,
	aee_rr_show_drcc_3,
	aee_rr_show_drcc_dbg_info,
	aee_rr_show_ptp_devinfo_0,
	aee_rr_show_ptp_devinfo_1,
	aee_rr_show_ptp_devinfo_2,
	aee_rr_show_ptp_devinfo_3,
	aee_rr_show_ptp_devinfo_4,
	aee_rr_show_ptp_devinfo_5,
	aee_rr_show_ptp_devinfo_6,
	aee_rr_show_ptp_devinfo_7,
	aee_rr_show_ptp_e0,
	aee_rr_show_ptp_e1,
	aee_rr_show_ptp_e2,
	aee_rr_show_ptp_e3,
	aee_rr_show_ptp_e4,
	aee_rr_show_ptp_e5,
	aee_rr_show_ptp_e6,
	aee_rr_show_ptp_e7,
	aee_rr_show_ptp_e8,
	aee_rr_show_ptp_e9,
	aee_rr_show_ptp_e10,
	aee_rr_show_ptp_e11,
	aee_rr_show_ptp_vboot,
	aee_rr_show_ptp_cpu_big_volt,
	aee_rr_show_ptp_cpu_big_volt_1,
	aee_rr_show_ptp_cpu_big_volt_2,
	aee_rr_show_ptp_cpu_big_volt_3,
	aee_rr_show_ptp_cpu_2_little_volt,
	aee_rr_show_ptp_cpu_2_little_volt_1,
	aee_rr_show_ptp_cpu_2_little_volt_2,
	aee_rr_show_ptp_cpu_2_little_volt_3,
	aee_rr_show_ptp_cpu_little_volt,
	aee_rr_show_ptp_cpu_little_volt_1,
	aee_rr_show_ptp_cpu_little_volt_2,
	aee_rr_show_ptp_cpu_little_volt_3,
	aee_rr_show_ptp_cpu_cci_volt,
	aee_rr_show_ptp_cpu_cci_volt_1,
	aee_rr_show_ptp_cpu_cci_volt_2,
	aee_rr_show_ptp_cpu_cci_volt_3,
	aee_rr_show_ptp_gpu_volt,
	aee_rr_show_ptp_gpu_volt_1,
	aee_rr_show_ptp_gpu_volt_2,
	aee_rr_show_ptp_gpu_volt_3,
	aee_rr_show_ptp_temp,
	aee_rr_show_ptp_status,
	aee_rr_show_eem_pi_offset,
	aee_rr_show_etc_status,
	aee_rr_show_etc_mode,
	aee_rr_show_thermal_temp,
	aee_rr_show_thermal_status,
	aee_rr_show_thermal_ATM_status,
	aee_rr_show_thermal_ktime,
	aee_rr_show_idvfs_ctrl_reg,
	aee_rr_show_idvfs_enable_cnt,
	aee_rr_show_idvfs_swreq_cnt,
	aee_rr_show_idvfs_curr_volt,
	aee_rr_show_idvfs_sram_ldo,
	aee_rr_show_idvfs_swavg_curr_pct_x100,
	aee_rr_show_idvfs_swreq_curr_pct_x100,
	aee_rr_show_idvfs_swreq_next_pct_x100,
	aee_rr_show_idvfs_state_manchine,
	aee_rr_show_ocp_target_limit,
	aee_rr_show_ocp_enable,
	aee_rr_show_scp_pc,
	aee_rr_show_scp_lr,
	aee_rr_show_hang_detect_timeout_count,
	aee_rr_show_last_async_func,
	aee_rr_show_last_sync_func,
	aee_rr_show_gz_irq,
	aee_rr_show_last_init_func,
	aee_rr_show_pmic_ext_buck,
	aee_rr_show_hps_status,
	aee_rr_show_hotplug_status,
	aee_rr_show_hotplug_caller_callee_status,
	aee_rr_show_hotplug_up_prepare_ktime,
	aee_rr_show_hotplug_starting_ktime,
	aee_rr_show_hotplug_online_ktime,
	aee_rr_show_hotplug_down_prepare_ktime,
	aee_rr_show_hotplug_dying_ktime,
	aee_rr_show_hotplug_dead_ktime,
	aee_rr_show_hotplug_post_dead_ktime

};

last_rr_show_cpu_t aee_rr_show_cpu[] = {
	aee_rr_show_last_irq_enter,
	aee_rr_show_jiffies_last_irq_enter,
	aee_rr_show_last_irq_exit,
	aee_rr_show_jiffies_last_irq_exit,
	aee_rr_show_hotplug_footprint,
	aee_rr_show_mtk_cpuidle_footprint,
	aee_rr_show_mcdi_footprint,
};

last_rr_show_t aee_rr_last_xxx[] = {
	aee_rr_show_last_pc,
	aee_rr_show_last_bus,
	aee_rr_show_suspend_debug_flag
};

#define array_size(x) (sizeof(x) / sizeof((x)[0]))
int aee_rr_reboot_reason_show(struct seq_file *m, void *v)
{
	int i, cpu;

	if (ram_console_check_header(ram_console_old)) {
		seq_puts(m, "NO VALID DATA.\n");
		seq_printf(m, "%s, old status is %u.\n", ram_console_clear ?
				"Clear" : "Not Clear", old_wdt_status);
		seq_puts(m, "Only try to dump last_XXX.\n");
		for (i = 0; i < array_size(aee_rr_last_xxx); i++)
			aee_rr_last_xxx[i] (m);
		return 0;
	}
	for (i = 0; i < array_size(aee_rr_show); i++)
		aee_rr_show[i] (m);

	for (cpu = 0; cpu < num_possible_cpus(); cpu++) {
		seq_printf(m, "CPU %d\n", cpu);
		for (i = 0; i < array_size(aee_rr_show_cpu); i++)
			aee_rr_show_cpu[i] (m, cpu);
	}
	return 0;
}
