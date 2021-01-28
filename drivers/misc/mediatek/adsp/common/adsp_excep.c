// SPDX-License-Identifier: GPL-2.0
//
// adsp_excep.c--  Mediatek ADSP exception handling
//
// Copyright (c) 2018 MediaTek Inc.
// Author: HsinYi Chang <hsin-yi.chang@mediatek.com>

#include <linux/vmalloc.h>         /* needed by vmalloc */
#include <linux/slab.h>            /* needed by kmalloc */
#include <linux/sysfs.h>
#include <linux/device.h>       /* needed by device_* */
#include <linux/workqueue.h>
#include <linux/io.h>
#include <linux/mutex.h>
#include <mt-plat/aee.h>
#include <mt-plat/sync_write.h>
#include <linux/sched_clock.h>
#include <linux/ratelimit.h>
#include "adsp_ipi.h"
#include "adsp_helper.h"
#include "adsp_clk.h"
#include "adsp_excep.h"
#include "adsp_feature_define.h"
#include "adsp_reserved_mem.h"

static unsigned char *adsp_ke_buffer;
static unsigned char *adsp_A_dump_buffer;
unsigned char *adsp_A_dram_dump_buffer;
static struct adsp_work_t adsp_aed_work;
static struct mutex adsp_excep_mutex;
static struct mutex adsp_A_excep_dump_mutex;
static int adsp_A_dram_dump(void);
static ssize_t adsp_A_ramdump(char *buf, loff_t offset, size_t size);

#define ADSP_KE_DUMP_LEN  (ADSP_A_CFG_SIZE + ADSP_A_TCM_SIZE)
#define DRV_SetReg32(addr, val)   writel(readl(addr) | (val), addr)

/* An ELF note in memory */
struct memelfnote {
	const char *name;
	int type;
	unsigned int datasz;
	void *data;
};

static int notesize(struct memelfnote *en)
{
	int sz;

	sz = sizeof(struct elf32_note);
	sz += roundup((strlen(en->name) + 1), 4);
	sz += roundup(en->datasz, 4);

	return sz;
}

static uint8_t *storenote(struct memelfnote *men, uint8_t *bufp)
{
	struct elf32_note en;

	en.n_namesz = strlen(men->name) + 1;
	en.n_descsz = men->datasz;
	en.n_type = men->type;

	memcpy(bufp, &en, sizeof(en));
	bufp += sizeof(en);

	memcpy(bufp, men->name, en.n_namesz);
	bufp += en.n_namesz;

	bufp = (uint8_t *) roundup((unsigned long)bufp, 4);
	memcpy(bufp, men->data, men->datasz);
	bufp += men->datasz;

	bufp = (uint8_t *) roundup((unsigned long)bufp, 4);
	return bufp;
}

static uint8_t *core_write_cpu_note(int cpu, struct elf32_phdr *nhdr,
				    uint8_t *bufp, enum adsp_core_id id)
{
	struct memelfnote notes;
	struct elf32_prstatus prstatus;
	char cpustr[16];

	memset(&prstatus, 0, sizeof(struct elf32_prstatus));
	snprintf(cpustr, sizeof(cpustr), "CPU%d", cpu);
	/* set up the process status */
	notes.name = cpustr;
	notes.type = NT_PRSTATUS;
	notes.datasz = sizeof(struct elf32_prstatus);
	notes.data = &prstatus;

	prstatus.pr_pid = cpu + 1;

	if (prstatus.pr_reg[15] == 0x0 && (id == ADSP_A_ID))
		prstatus.pr_reg[15] = readl(ADSP_A_WDT_DEBUG_PC_REG);


	nhdr->p_filesz += notesize(&notes);
	return storenote(&notes, bufp);
}

void adsp_exception_header_init(void *oldbufp, enum adsp_core_id id)
{
	struct elf32_phdr *nhdr, *phdr;
	struct elf32_hdr *elf;
	off_t offset = 0;

	uint8_t *bufp = oldbufp;
	uint32_t cpu;

	/* NT_PRPSINFO */
	struct elf32_prpsinfo prpsinfo;
	struct memelfnote notes;

	elf = (struct elf32_hdr *) bufp;
	bufp += sizeof(struct elf32_hdr);
	offset += sizeof(struct elf32_hdr);
	elf_setup_eident(elf->e_ident, ELFCLASS32);

	/*setup elf header*/
	elf->e_type = ET_CORE;
	elf->e_machine = EM_ARM;
	elf->e_version = EV_CURRENT;
	elf->e_entry = 0;
	elf->e_phoff = sizeof(struct elf32_hdr);
	elf->e_shoff = 0;
	elf->e_flags = ELF_CORE_EFLAGS;
	elf->e_ehsize = sizeof(struct elf32_hdr);
	elf->e_phentsize = sizeof(struct elf32_phdr);
	elf->e_phnum = 2;
	elf->e_shentsize = 0;
	elf->e_shnum = 0;
	elf->e_shstrndx = 0;

	nhdr = (struct elf32_phdr *) bufp;
	bufp += sizeof(struct elf32_phdr);
	offset += sizeof(struct elf32_phdr);
	memset(nhdr, 0, sizeof(struct elf32_phdr));
	nhdr->p_type = PT_NOTE;

	phdr = (struct elf32_phdr *) bufp;
	bufp += sizeof(struct elf32_phdr);
	offset += sizeof(struct elf32_phdr);
	phdr->p_flags = PF_R | PF_W | PF_X;
	phdr->p_offset = CRASH_MEMORY_HEADER_SIZE;
	phdr->p_vaddr = CRASH_MEMORY_OFFSET;
	phdr->p_paddr = CRASH_MEMORY_OFFSET;


	phdr->p_filesz = (ADSP_A_TCM_SIZE - CRASH_MEMORY_OFFSET);
	phdr->p_memsz = (ADSP_A_TCM_SIZE - CRASH_MEMORY_OFFSET);


	phdr->p_align = 0;
	phdr->p_type = PT_LOAD;

	nhdr->p_offset = offset;

	/* set up the process info */
	notes.name = CORE_STR;
	notes.type = NT_PRPSINFO;
	notes.datasz = sizeof(struct elf32_prpsinfo);
	notes.data = &prpsinfo;

	memset(&prpsinfo, 0, sizeof(struct elf32_prpsinfo));
	prpsinfo.pr_state = 0;
	prpsinfo.pr_sname = 'R';
	prpsinfo.pr_zomb = 0;
	prpsinfo.pr_gid = prpsinfo.pr_uid = 0x0;
	strlcpy(prpsinfo.pr_fname, "freertos8", sizeof(prpsinfo.pr_fname));
	strlcpy(prpsinfo.pr_psargs, "freertos8", ELF_PRARGSZ);

	nhdr->p_filesz += notesize(&notes);
	bufp = storenote(&notes, bufp);

	/* Store pre-cpu backtrace */
	for (cpu = 0; cpu < 1; cpu++)
		bufp = core_write_cpu_note(cpu, nhdr, bufp, id);
}

static u32 copy_from_buffer(void *dest, size_t destsize, void *src,
			    size_t srcsize, u32 offset, size_t request)
{
	/* if request == -1, offset == 0, copy full srcsize */
	if (offset + request > srcsize)
		request = srcsize - offset;

	/* if destsize == -1, don't check the request size */
	if (!dest || destsize < request) {
		pr_warn("%s, buffer null or not enough space", __func__);
		return 0;
	}

	memcpy(dest, src + offset, request);

	return request;
}

static u32 dump_adsp_cfg_reg(void *buf, size_t size)
{
	u32 clk_cfg = 0, uart_cfg = 0, n = 0;

	/* record CFG registers */
	clk_cfg = readl(ADSP_CLK_CTRL_BASE);
	uart_cfg = readl(ADSP_UART_CTRL);

	DRV_SetReg32(ADSP_CLK_CTRL_BASE, ADSP_CLK_UART_EN);
	DRV_SetReg32(ADSP_UART_CTRL, ADSP_UART_RST_N | ADSP_UART_BCLK_CG);

	n += copy_from_buffer(buf, size, ADSP_A_CFG, ADSP_A_CFG_SIZE, 0, -1);

	/* Restore ADSP CLK and UART setting */
	writel(clk_cfg, ADSP_CLK_CTRL_BASE);
	writel(uart_cfg, ADSP_UART_CTRL);

	return n;
}

static u32 dump_adsp_tcm(void *buf, size_t size)
{
	u32 n = 0;

	n += copy_from_buffer(buf, size,
			ADSP_A_ITCM, ADSP_A_ITCM_SIZE, 0, -1);
	n += copy_from_buffer(buf + n, size - n,
			ADSP_A_DTCM, ADSP_A_DTCM_SIZE, 0, -1);
	return n;
}

static inline u32 dump_adsp_shared_memory(void *buf, size_t size, int id)
{
	void *mem_addr = adsp_get_reserve_mem_virt(id);
	size_t mem_size = adsp_get_reserve_mem_size(id);

	if (!mem_addr)
		return 0;

	return copy_from_buffer(buf, size, mem_addr, mem_size, 0, -1);
}

static inline u32 copy_from_adsp_shared_memory(void *buf, u32 offset,
					size_t size, int id)
{
	void *mem_addr = adsp_get_reserve_mem_virt(id);
	size_t mem_size = adsp_get_reserve_mem_size(id);

	if (!mem_addr)
		return 0;

	return copy_from_buffer(buf, -1, mem_addr, mem_size, offset, size);
}

/*
 * this function need ADSP to keeping awaken
 * adsp_crash_dump: dump adsp tcm info.
 * @param MemoryDump:   adsp dump struct
 * @param adsp_core_id:  core id
 * @return:             adsp dump size
 */
static u32 adsp_crash_dump(struct MemoryDump *pMemoryDump,
				    enum adsp_core_id id)
{
	u32 n = 0;

	adsp_enable_dsp_clk(true);
	mutex_lock(&adsp_sw_reset_mutex);

	adsp_exception_header_init(pMemoryDump, id);
	n += CRASH_MEMORY_HEADER_SIZE;
	n += dump_adsp_tcm((void *)&(pMemoryDump->memory), CRASH_MEMORY_LENGTH);
	n += CRASH_CORE_REG_SIZE;
	n += dump_adsp_cfg_reg((void *)&(pMemoryDump->cfg_reg),
			       CRASH_CFG_REG_SIZE);

	if (n != sizeof(struct MemoryDump))
		pr_info("%s(), size not match n(%x) != MemoryDump(%zd)",
			__func__, n, sizeof(struct MemoryDump));

	mutex_unlock(&adsp_sw_reset_mutex);
	adsp_enable_dsp_clk(false);

	return n;
}

/*
 * generate aee argument with adsp register dump
 * @param aed_str:  exception description
 * @param aed:      struct to store argument for aee api
 * @param id:       identify adsp core id
 */
static void adsp_prepare_aed_dump(void)
{
	struct MemoryDump *pMemoryDump = NULL;
	u32 n = 0;

	/*prepare adsp A db file*/
	pMemoryDump = (struct MemoryDump *)adsp_A_dump_buffer;
	if (!pMemoryDump) {
		pr_debug("[ADSP AEE]MemoryDump buf is null");
	} else {
		memset(pMemoryDump, 0, sizeof(*pMemoryDump));
		n = adsp_crash_dump(pMemoryDump, ADSP_A_ID);
	}
}

/*
 * generate an exception according to exception type
 * @param type: exception type
 */
void adsp_aed(enum adsp_excep_id type, enum adsp_core_id id)
{
	char detail[ADSP_AED_STR_LEN];
	struct MemoryDump *pMemoryDump;
	struct adsp_coredump *coredump;
	u32 n = 0;
	char *aed_type;
	int db_opt = DB_OPT_DEFAULT;
	int ret = 0;

	mutex_lock(&adsp_excep_mutex);
	/* get adsp title and exception type*/
	switch (type) {
	case EXCEP_LOAD_FIRMWARE:
		aed_type = "load firmware exception";
		break;
	case EXCEP_RESET:
		aed_type = "reset exception";
		break;
	case EXCEP_BOOTUP:
		aed_type = "boot exception";
		break;
	case EXCEP_RUNTIME:
		aed_type = "runtime exception";
		break;
	case EXCEP_KERNEL:
		aed_type = "kernel exception";
		db_opt |= DB_OPT_FTRACE;
		break;
	default:
		aed_type = "unknown exception";
		break;
	}

	if (type != EXCEP_LOAD_FIRMWARE) {
		mutex_lock(&adsp_A_excep_dump_mutex);
		adsp_prepare_aed_dump();
		ret = adsp_A_dram_dump();
		mutex_unlock(&adsp_A_excep_dump_mutex);
	}

	pMemoryDump = (struct MemoryDump *)adsp_A_dump_buffer;
	coredump = adsp_get_reserve_mem_virt(ADSP_A_CORE_DUMP_MEM_ID);

	/*print detail info. in kernel*/
	n += snprintf(detail + n, ADSP_AED_STR_LEN - n, "%s %s\n",
		      adsp_core_ids[ADSP_A_ID], aed_type);
	n += snprintf(detail + n, ADSP_AED_STR_LEN - n,
		      "adsp pc=0x%08x,exccause=0x%x,excvaddr=0x%x\n",
		      coredump->pc, coredump->exccause, coredump->excvaddr);
	n += snprintf(detail + n, ADSP_AED_STR_LEN - n,
		      "latch pc=0x%08x,sp=0x%08x\n",
		      *((u32 *)(pMemoryDump->cfg_reg + 0x0170)),
		      *((u32 *)(pMemoryDump->cfg_reg + 0x0174)));
	n += snprintf(detail + n, ADSP_AED_STR_LEN - n,
		      "CRDISPATCH_KEY:ADSP exception/%s\n",
		      coredump->task_name);
	n += snprintf(detail + n, ADSP_AED_STR_LEN - n, "%s",
		      coredump->assert_log);
	pr_debug("%s", detail);

#ifdef CFG_RECOVERY_SUPPORT
	if (ret > 0 && /* if dram backup done, notify reset.work it's okay */
	    (atomic_read(&adsp_reset_status) == ADSP_RESET_STATUS_START ||
	     adsp_recovery_flag[ADSP_A_ID] == ADSP_RECOVERY_START)) {
		/*complete adsp ee, if adsp reset by wdt or awake fail*/
		pr_info("[ADSP]aed finished, complete it\n");
		complete(&adsp_sys_reset_cp);

	}
#endif
	/* adsp aed api, only detail information available*/
	aed_common_exception_api("adsp", NULL, 0, NULL, 0, detail, db_opt);

	pr_debug("[ADSP] adsp exception dump is done\n");
	mutex_unlock(&adsp_excep_mutex);
}

/*
 * callback function for work struct
 * generate an exception and reset adsp
 * NOTE: this function may be blocked
 * and should not be called in interrupt context
 * @param ws:   work struct
 */
static void adsp_aed_reset_ws(struct work_struct *ws)
{
	struct adsp_work_t *sws = container_of(ws, struct adsp_work_t, work);
	enum adsp_excep_id type = sws->flags;
	enum adsp_core_id id = sws->id;

	pr_debug("[ADSP]%s: adsp_excep_id=%u adsp_core_id=%u\n",
		__func__, type, id);
	adsp_reset_ready(ADSP_A_ID);
	adsp_aed(type, id);
}

/*
 * schedule a work to generate an exception and reset adsp
 * @param type: exception type
 */
void adsp_aed_reset(enum adsp_excep_id type, enum adsp_core_id id)
{
	adsp_aed_work.flags = (unsigned int) type;
	adsp_aed_work.id = (unsigned int) id;
	queue_work(adsp_workqueue, &adsp_aed_work.work);
}

#if ADSP_TRAX
static ssize_t adsp_A_trax_show(struct file *filep, struct kobject *kobj,
				struct bin_attribute *attr,
				char *buf, loff_t offset, size_t size)
{
	unsigned int length = 0;
	void *trax_addr = adsp_get_reserve_mem_virt(ADSP_A_TRAX_MEM_ID);

	pr_debug("[ADSP] trax initiated=%d, done=%d, length=%d, offset=%lld\n",
		adsp_get_trax_initiated(), adsp_get_trax_done(),
		adsp_get_trax_length(), offset);

	if (adsp_get_trax_initiated() && adsp_get_trax_done()) {
		if (offset >= 0 && offset < adsp_get_trax_length()) {
			if ((offset + size) > adsp_get_trax_length())
				size = adsp_get_trax_length() - offset;

			memcpy(buf, trax_addr + offset, size);
			length = size;
		}
	}
	return length;
}

struct bin_attribute bin_attr_adsp_trax = {
	.attr = {
		.name = "adsp_trax",
		.mode = 0444,
	},
	.size = 0,
	.read = adsp_A_trax_show,
};
#endif

static int adsp_A_dram_dump(void)
{
	unsigned int dram_total = 0;
	void *dump_buf = NULL;
	uint32_t n = 0;

	/*wait EE dump done*/
#ifdef CFG_RECOVERY_SUPPORT
	if (wdt_counter == WDT_FIRST_WAIT_COUNT ||
	    wdt_counter == WDT_LAST_WAIT_COUNT)
		return -1;
#endif
	/* all dram */
	dram_total = adsp_get_reserve_mem_size(ADSP_A_SYSTEM_MEM_ID)
		 + adsp_get_reserve_mem_size(ADSP_A_CORE_DUMP_MEM_ID)
		 + adsp_get_reserve_mem_size(ADSP_A_IPI_MEM_ID)
		 + adsp_get_reserve_mem_size(ADSP_A_LOGGER_MEM_ID);

	/* allocate adsp dump dram buffer*/
	if (adsp_A_dram_dump_buffer == NULL)
		adsp_A_dram_dump_buffer = vmalloc(dram_total);

	if (!adsp_A_dram_dump_buffer)
		return -1;

	dump_buf = adsp_A_dram_dump_buffer;
	n += dump_adsp_shared_memory(dump_buf + n, dram_total - n,
				     ADSP_A_SYSTEM_MEM_ID);
	n += dump_adsp_shared_memory(dump_buf + n, dram_total - n,
				     ADSP_A_CORE_DUMP_MEM_ID);
	n += dump_adsp_shared_memory(dump_buf + n, dram_total - n,
				     ADSP_A_IPI_MEM_ID);
	n += dump_adsp_shared_memory(dump_buf + n, dram_total - n,
				     ADSP_A_LOGGER_MEM_ID);

	return n;
}
static ssize_t adsp_A_ramdump(char *buf, loff_t offset, size_t size)
{
	ssize_t n = 0;
	ssize_t threshold[5];

	threshold[0] = sizeof(struct MemoryDump);
	threshold[1] = threshold[0] +
		adsp_get_reserve_mem_size(ADSP_A_SYSTEM_MEM_ID);
	threshold[2] = threshold[1] +
		adsp_get_reserve_mem_size(ADSP_A_CORE_DUMP_MEM_ID);
	threshold[3] = threshold[2] +
		adsp_get_reserve_mem_size(ADSP_A_IPI_MEM_ID);
	threshold[4] = threshold[3] +
		adsp_get_reserve_mem_size(ADSP_A_LOGGER_MEM_ID);

	mutex_lock(&adsp_A_excep_dump_mutex);

	if (offset >= 0 && offset < threshold[0]) {
		n = copy_from_buffer(buf, -1, adsp_A_dump_buffer,
			threshold[0], offset, size);
		goto DONE;
	}

	if (adsp_A_dram_dump_buffer) { /* backup before: adsp_A_dram_dump */
		n = copy_from_buffer(buf, -1, adsp_A_dram_dump_buffer,
			threshold[4] - threshold[0],
			offset - threshold[0], size);

		if (n == 0) {
			vfree(adsp_A_dram_dump_buffer);
			adsp_A_dram_dump_buffer = NULL;
		}
		goto DONE;
	}

	/* not backup, dump immediately */
	if (offset >= threshold[0] && offset < threshold[1]) {
		n = copy_from_adsp_shared_memory(
				buf, offset - threshold[0],
				size, ADSP_A_SYSTEM_MEM_ID);
	} else if (offset >= threshold[1] && offset < threshold[2]) {
		n = copy_from_adsp_shared_memory(
				buf, offset - threshold[1],
				size, ADSP_A_CORE_DUMP_MEM_ID);
	} else if (offset >= threshold[2] && offset < threshold[3]) {
		n = copy_from_adsp_shared_memory(
				buf, offset - threshold[2],
				size, ADSP_A_IPI_MEM_ID);
	} else if (offset >= threshold[3] && offset < threshold[4]) {
		n = copy_from_adsp_shared_memory(
				buf, offset - threshold[3],
				size, ADSP_A_LOGGER_MEM_ID);
	}

	/* dump done*/
#ifdef CFG_RECOVERY_SUPPORT
	if (!n &&
	    (atomic_read(&adsp_reset_status) == ADSP_RESET_STATUS_START ||
	     adsp_recovery_flag[ADSP_A_ID] == ADSP_RECOVERY_START)) {
		/*complete scp ee, if scp reset by wdt or awake fail*/
		pr_info("[ADSP]aed finished, complete it\n");
		complete(&adsp_sys_reset_cp);
	}
#endif
DONE:
	mutex_unlock(&adsp_A_excep_dump_mutex);
	return n;
}

static ssize_t adsp_A_dump_show(struct file *filep, struct kobject *kobj,
				struct bin_attribute *attr,
				char *buf, loff_t offset, size_t size)
{
	return adsp_A_ramdump(buf, offset, size);
}

static ssize_t adsp_A_dump_ke_show(struct file *filep, struct kobject *kobj,
				struct bin_attribute *attr,
				char *buf, loff_t offset, size_t size)
{
	if (offset == 0) /* only do ITCM/DTCM ramdump once at start */
		adsp_prepare_aed_dump();

	return adsp_A_ramdump(buf, offset, size);
}

static struct bin_attribute bin_attr_adsp_dump = {
	.attr = {
		.name = "adsp_dump",
		.mode = 0444,
	},
	.size = 0,
	.read = adsp_A_dump_show,
};

static struct bin_attribute bin_attr_adsp_dump_ke = {
	.attr = {
		.name = "adsp_dump_ke",
		.mode = 0444,
	},
	.size = 0,
	.read = adsp_A_dump_ke_show,
};

static struct bin_attribute *adsp_excep_bin_attrs[] = {
	&bin_attr_adsp_dump,
	&bin_attr_adsp_dump_ke,
#if ADSP_TRAX
	&bin_attr_adsp_trax,
#endif
	NULL,
};

struct attribute_group adsp_excep_attr_group = {
	.bin_attrs = adsp_excep_bin_attrs,
};

/*
 * init a work struct
 */
int adsp_excep_init(void)
{
	mutex_init(&adsp_excep_mutex);
	mutex_init(&adsp_A_excep_dump_mutex);

	INIT_WORK(&adsp_aed_work.work, adsp_aed_reset_ws);

	/* alloc dump memory*/
	adsp_A_dump_buffer = vmalloc(sizeof(struct MemoryDump));
	if (!adsp_A_dump_buffer)
		return -1;

	adsp_ke_buffer = kmalloc(ADSP_KE_DUMP_LEN, GFP_KERNEL);
	if (!adsp_ke_buffer)
		return -1;

	return 0;
}

/*
 * cleanup adsp exception
 */
void adsp_excep_cleanup(void)
{
	vfree(adsp_A_dump_buffer);
	kfree(adsp_ke_buffer);

	pr_debug("[ADSP] %s done\n", __func__);
}

void get_adsp_aee_buffer(unsigned long *vaddr, unsigned long *size)
{
	u32 n = 0;

	if (adsp_ke_buffer) {
		adsp_enable_dsp_clk(true);
		n += dump_adsp_cfg_reg(adsp_ke_buffer, ADSP_KE_DUMP_LEN);
		n += dump_adsp_tcm(adsp_ke_buffer + n, ADSP_KE_DUMP_LEN - n);
		adsp_enable_dsp_clk(false);

		*vaddr = (unsigned long)adsp_ke_buffer;
		*size = ADSP_KE_DUMP_LEN;
	}
}
EXPORT_SYMBOL(get_adsp_aee_buffer);
