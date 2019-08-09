/*
 * Copyright (C) 2011-2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License version 2 as published by the
 * Free Software Foundation.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program.
 * If not, see <http://www.gnu.org/licenses/>.
 */

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



struct adsp_aed_cfg {
	int *log;
	int log_size;
	int *phy;
	int phy_size;
	char *detail;
	struct MemoryDump *pMemoryDump;
	int memory_dump_size;
};

struct adsp_status_reg {
	unsigned int pc;
	unsigned int sp;
	unsigned int m2h;
	unsigned int h2m;
};
static unsigned char *adsp_ke_buffer;
static unsigned char *adsp_A_detail_buffer;
static unsigned char *adsp_A_dump_buffer;
static unsigned char *adsp_A_dram_dump_buffer;
static unsigned char *adsp_A_dump_buffer_last;
static unsigned int adsp_A_dump_length;
static unsigned int adsp_A_task_context_addr;
static struct adsp_work_struct adsp_aed_work;
static struct adsp_status_reg adsp_A_aee_status;
static struct mutex adsp_excep_mutex;
static struct mutex adsp_A_excep_dump_mutex;
static int adsp_A_dram_dump(void);
static ssize_t adsp_A_ramdump(char *buf, loff_t offset, size_t size);

#define ADSP_KE_DUMP_LEN  (ADSP_A_CFG_SIZE + ADSP_A_TCM_SIZE)


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
	if (adsp_A_task_context_addr && (id == ADSP_A_ID)) {
		memcpy_from_adsp((void *)&(prstatus.pr_reg),
				 (void *)(ADSP_A_DTCM +
				 adsp_A_task_context_addr),
				 sizeof(prstatus.pr_reg));
	}


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
/*dump adsp reg*/
void adsp_reg_copy(void *bufp)
{
#if 0 // TBD if necessary
	struct adsp_reg_dump_list *adsp_reg;

	adsp_reg = (struct adsp_reg_dump_list *) bufp;

	/* setup adsp reg */
	adsp_reg->adsp_reg_magic = 0xDEADBEEF;
	adsp_reg->ap_resource = readl(ADSP_AP_RESOURCE);
	adsp_reg->bus_resource = readl(ADSP_BUS_RESOURCE);
	adsp_reg->cpu_sleep_status = readl(ADSP_SLEEP_STATUS_REG);
	adsp_reg->clk_sw_sel = readl(ADSP_CLK_SW_SEL);
	adsp_reg->clk_enable = readl(ADSP_CLK_ENABLE);
	adsp_reg->clk_high_core = readl(ADSP_CLK_HIGH_CORE_CG);
	adsp_reg->adsp_reg_magic_end = 0xDEADBEEF;
#endif
}
/*
 * return last pc for debugging
 */
uint32_t adsp_dump_pc(void)
{
	if (is_adsp_ready(ADSP_A_ID) == 1)
		return readl(ADSP_A_WDT_DEBUG_PC_REG);
	else
		return 0xFFFFFFFF;
}

/*
 * save adsp register when adsp crash
 * these data will be used to generate EE
 */
void adsp_aee_last_reg(void)
{
	pr_debug("%s\n", __func__);

	adsp_A_aee_status.pc = readl(ADSP_A_WDT_DEBUG_PC_REG);
	adsp_A_aee_status.sp = readl(ADSP_A_WDT_DEBUG_SP_REG);
	adsp_A_aee_status.m2h = readl(ADSP_A_TO_HOST_REG);
	adsp_A_aee_status.h2m = readl(ADSP_SWINT_REG);

	pr_debug("%s end\n", __func__);
}

/*
 * this function need ADSP to keeping awaken
 * adsp_crash_dump: dump adsp tcm info.
 * @param MemoryDump:   adsp dump struct
 * @param adsp_core_id:  core id
 * @return:             adsp dump size
 */
static unsigned int adsp_crash_dump(struct MemoryDump *pMemoryDump,
				    enum adsp_core_id id)
{
	unsigned int adsp_dump_size;

	/*flag use to indicate adsp awake success or not*/
	adsp_exception_header_init(pMemoryDump, id);
	memcpy_from_adsp((void *)&(pMemoryDump->memory),
			 (void *)(ADSP_A_ITCM), (ADSP_A_ITCM_SIZE));
	memcpy_from_adsp((void *)&(pMemoryDump->memory) + ADSP_A_ITCM_SIZE,
			 (void *)(ADSP_A_DTCM), (ADSP_A_DTCM_SIZE));
	//Liang : Dump Dram here?
	/*dump adsp reg*/
	adsp_reg_copy(&(pMemoryDump->adsp_reg_dump));

	adsp_dump_size = CRASH_MEMORY_HEADER_SIZE + ADSP_A_TCM_SIZE +
			 CRASH_REG_SIZE;

	return adsp_dump_size;
}
/*
 * generate aee argument without dump adsp register
 * @param aed_str:  exception description
 * @param aed:      struct to store argument for aee api
 */
static void adsp_prepare_aed(char *aed_str, struct adsp_aed_cfg *aed)
{
	char *detail, *log;
	u8 *phy;
	u32 log_size, phy_size;

	pr_debug("%s\n", __func__);

	detail = vmalloc(ADSP_AED_STR_LEN);
	if (!detail)
		return;

	memset(detail, 0, ADSP_AED_STR_LEN);
	snprintf(detail, ADSP_AED_STR_LEN, "%s\n", aed_str);
	detail[ADSP_AED_STR_LEN - 1] = '\0';

	log_size = 0;
	log = NULL;

	phy_size = 0;
	phy = NULL;

	aed->log = (int *)log;
	aed->log_size = log_size;
	aed->phy = (int *)phy;
	aed->phy_size = phy_size;
	aed->detail = detail;

	pr_debug("%s end\n", __func__);
}

/*
 * generate aee argument with adsp register dump
 * @param aed_str:  exception description
 * @param aed:      struct to store argument for aee api
 * @param id:       identify adsp core id
 */
static void adsp_prepare_aed_dump(char *aed_str, struct adsp_aed_cfg *aed,
				  enum adsp_core_id id)
{
#define TASKNAME_LENGTH (16)
#define TASKNAME_OFFSET (308)
#define ASSERTINFO_LENGTH (512)
#define ASSERTINFO_OFFSET (512)

	u8 *adsp_detail;
	u8 *adsp_dump_ptr, *str_ptr;
	u8 adsp_taskname[TASKNAME_LENGTH], adsp_assert_info[ASSERTINFO_LENGTH];
	u32 memory_dump_size;
	u64 core_memaddr;
	struct MemoryDump *pMemoryDump = NULL;

	pr_debug("%s:%s\n", __func__, aed_str);
	adsp_aee_last_reg();

	/* prepare adsp aee detail information */
	adsp_detail = adsp_A_detail_buffer;

	if (adsp_detail == NULL) {
		pr_debug("[ADSP AEE]detail buf is null\n");
	} else {
		/* prepare adsp aee detail information*/
		memset(adsp_detail, 0, ADSP_AED_STR_LEN);
		core_memaddr =
			adsp_get_reserve_mem_virt(ADSP_A_CORE_DUMP_MEM_ID);

		adsp_taskname[TASKNAME_LENGTH - 1] = '\0';
		str_ptr = (u8 *)core_memaddr + TASKNAME_OFFSET;
		memcpy(adsp_taskname, (void *)str_ptr,
			(TASKNAME_LENGTH - 1));
		pr_info("[ADSP AEE]last adsp_taskname=%s\n",
			adsp_taskname);

		adsp_assert_info[ASSERTINFO_LENGTH - 1] = '\0';
		str_ptr = (u8 *)core_memaddr + ASSERTINFO_OFFSET;
		memcpy(adsp_assert_info, (void *)str_ptr,
			(ASSERTINFO_LENGTH - 1));
		pr_info("[ADSP AEE]adsp_assert_info=%s\n",
			adsp_assert_info);

		snprintf(adsp_detail, ADSP_AED_STR_LEN,
		"%s\nadsp pc=0x%08x,sp=0x%08x\nCRDISPATCH_KEY:ADSP exception/%s\n%s",
		aed_str, adsp_A_aee_status.pc, adsp_A_aee_status.sp,
		adsp_taskname, adsp_assert_info);

		adsp_detail[ADSP_AED_STR_LEN - 1] = '\0';
	}

	/*prepare adsp A db file*/
	memory_dump_size = 0;
	adsp_dump_ptr = adsp_A_dump_buffer_last;
	if (!adsp_dump_ptr) {
		pr_debug("[ADSP AEE]MemoryDump buf is null, size=0x%x\n",
		       memory_dump_size);
		memory_dump_size = 0;
	} else {
		pr_debug("[ADSP AEE]adsp A dump ptr:0x%p\n", adsp_dump_ptr);
		pMemoryDump = (struct MemoryDump *) adsp_dump_ptr;
		memset(pMemoryDump, 0x0, sizeof(*pMemoryDump));
		memory_dump_size = adsp_crash_dump(pMemoryDump, ADSP_A_ID);
	}
	/* adsp_dump_buffer_set */
	adsp_A_dump_buffer_last = adsp_A_dump_buffer;
	adsp_A_dump_buffer = adsp_dump_ptr;
	adsp_A_dump_length = memory_dump_size;

	aed->log = NULL;
	aed->log_size = 0;
	aed->phy = NULL;
	aed->phy_size = 0;
	aed->detail = adsp_detail;
	aed->pMemoryDump = NULL;
	aed->memory_dump_size = 0;

	pr_debug("%s end\n", __func__);
}

/*
 * generate an exception according to exception type
 * @param type: exception type
 */
void adsp_aed(enum adsp_excep_id type, enum adsp_core_id id)
{
	struct adsp_aed_cfg aed;
	char *adsp_aed_title;
	int db_opt = DB_OPT_DEFAULT;

	mutex_lock(&adsp_excep_mutex);

	/* get adsp title and exception type*/
	switch (type) {
	case EXCEP_LOAD_FIRMWARE:
		adsp_prepare_aed("adsp firmware load exception", &aed);
		if (id == ADSP_A_ID)
			adsp_aed_title = "ADSP_A load firmware exception";
		else
			adsp_aed_title = "ADSP load firmware exception";
		break;
	case EXCEP_RESET:
		if (id == ADSP_A_ID)
			adsp_aed_title = "ADSP_A reset exception";
		else
			adsp_aed_title = "ADSP reset exception";
		break;
	case EXCEP_BOOTUP:
		if (id == ADSP_A_ID)
			adsp_aed_title = "ADSP_A boot exception";
		else
			adsp_aed_title = "ADSP boot exception";
		adsp_get_log(id);
		break;
	case EXCEP_RUNTIME:
		if (id == ADSP_A_ID)
			adsp_aed_title = "ADSP_A runtime exception";
		else
			adsp_aed_title = "ADSP runtime exception";
		adsp_get_log(id);
		break;
	case EXCEP_KERNEL:
		if (id == ADSP_A_ID)
			adsp_aed_title = "ADSP_A kernel exception";
		else
			adsp_aed_title = "ADSP kernel exception";
		db_opt |= DB_OPT_FTRACE;
		break;
	default:
		adsp_prepare_aed("adsp unknown exception", &aed);
		if (id == ADSP_A_ID)
			adsp_aed_title = "ADSP_A unknown exception";
		else
			adsp_aed_title = "ADSP unknown exception";
		break;
	}
	/*print adsp message*/
	pr_debug("adsp_aed_title=%s", adsp_aed_title);

	if (type != EXCEP_LOAD_FIRMWARE) {
		adsp_enable_dsp_clk(true);
		mutex_lock(&adsp_A_excep_dump_mutex);
		mutex_lock(&adsp_sw_reset_mutex);
		adsp_prepare_aed_dump(adsp_aed_title, &aed, id);
		adsp_A_dram_dump();
		mutex_unlock(&adsp_sw_reset_mutex);
		mutex_unlock(&adsp_A_excep_dump_mutex);
		adsp_enable_dsp_clk(false);
	}
	/*print detail info. in kernel*/
	pr_debug("%s", aed.detail);

	/* adsp aed api, only detail information available*/
	aed_common_exception_api("adsp", NULL, 0, NULL, 0, aed.detail, db_opt);

	pr_debug("[ADSP] adsp exception dump is done\n");

	mutex_unlock(&adsp_excep_mutex);
}

/*
 * generate an exception and reset adsp right now
 * NOTE: this function may be blocked
 * and should not be called in interrupt context
 * @param type: exception type
 */
void adsp_aed_reset_inplace(enum adsp_excep_id type, enum adsp_core_id id)
{
	pr_debug("[ADSP]%s\n", __func__);
	adsp_reset_ready(ADSP_A_ID);
	adsp_aed(type, id);
#ifndef CFG_RECOVERY_SUPPORT
	/* workaround for QA, not reset ADSP in WDT */
	if (type == EXCEP_RUNTIME)
		return;
#endif

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
	struct adsp_work_struct *sws = container_of(ws, struct adsp_work_struct,
						    work);
	enum adsp_excep_id type = (enum adsp_excep_id) sws->flags;
	enum adsp_core_id id = (enum adsp_core_id) sws->id;

	pr_debug("[ADSP]%s: adsp_excep_id=%u adsp_core_id=%u\n",
		__func__, type, id);
	adsp_aed_reset_inplace(type, id);
}

/* IPI for ramdump config
 * @param id:   IPI id
 * @param data: IPI data
 * @param len:  IPI data length
 */
static void adsp_A_ram_dump_ipi_handler(int id, void *data, unsigned int len)
{
	adsp_A_task_context_addr = *(unsigned int *)data;
	pr_debug("[ADSP]get adsp_A_task_context_addr: 0x%x\n",
		 adsp_A_task_context_addr);
}

/*
 * schedule a work to generate an exception and reset adsp
 * @param type: exception type
 */
void adsp_aed_reset(enum adsp_excep_id type, enum adsp_core_id id)
{
	adsp_aed_work.flags = (unsigned int) type;
	adsp_aed_work.id = (unsigned int) id;
	adsp_schedule_work(&adsp_aed_work);
}

#if ADSP_TRAX
static ssize_t adsp_A_trax_show(struct file *filep, struct kobject *kobj,
				struct bin_attribute *attr,
				char *buf, loff_t offset, size_t size)
{
	unsigned int length = 0;

	pr_debug("[ADSP] trax initiated=%d, done=%d, length=%d, offset=%lld\n",
		adsp_get_trax_initiated(), adsp_get_trax_done(),
		adsp_get_trax_length(), offset);

	if (adsp_get_trax_initiated() && adsp_get_trax_done()) {
		if (offset >= 0 && offset < adsp_get_trax_length()) {
			if ((offset + size) > adsp_get_trax_length())
				size = adsp_get_trax_length() - offset;
			memcpy(buf,
			(void *)adsp_get_reserve_mem_virt(ADSP_A_TRAX_MEM_ID)
			+ offset, size);
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
	unsigned int sys_memsize = 0, core_memsize = 0;
	unsigned int dram_total = 0;
	uint32_t offset = 0;
	unsigned int clk_cfg = 0, uart_cfg = 0;
	u64 core_memaddr = 0;

	/*wait EE dump done*/
	if (wdt_counter == WDT_FIRST_WAIT_COUNT ||
	    wdt_counter == WDT_LAST_WAIT_COUNT)
		return -1;
	sys_memsize = adsp_get_reserve_mem_size(ADSP_A_SYSTEM_MEM_ID);
	core_memsize = adsp_get_reserve_mem_size(ADSP_A_CORE_DUMP_MEM_ID);
	/* all dram */
	dram_total = (sys_memsize + core_memsize +
		      ADSP_A_CFG_SIZE + ADSP_RESERVED_DRAM_SIZE);
	/* allocate adsp dump dram buffer*/
	if (adsp_A_dram_dump_buffer == NULL)
		adsp_A_dram_dump_buffer = vmalloc(dram_total);
	if (!adsp_A_dram_dump_buffer) {
		pr_debug("%s DRAM dump size malloc failed\n", __func__);
		return -1;
	}
	/* all CFG registers */
	clk_cfg = readl(ADSP_CLK_CTRL_BASE);
	uart_cfg = readl(ADSP_UART_CTRL);
	writel(readl(ADSP_CLK_CTRL_BASE) | ADSP_CLK_UART_EN,
		ADSP_CLK_CTRL_BASE);
	writel(readl(ADSP_UART_CTRL) | ADSP_UART_RST_N |
		ADSP_UART_BCLK_CG, ADSP_UART_CTRL);
	memcpy(adsp_A_dram_dump_buffer, ADSP_A_CFG, ADSP_A_CFG_SIZE);
	/* Restore ADSP CLK and UART setting */
	writel(clk_cfg, ADSP_CLK_CTRL_BASE);
	writel(uart_cfg, ADSP_UART_CTRL);
	offset = ADSP_A_CFG_SIZE;
	memcpy((void *)(adsp_A_dram_dump_buffer + offset),
		ADSP_A_SYS_DRAM, sys_memsize);
	offset = offset + sys_memsize;
	/* core reg dump */
	core_memaddr = adsp_get_reserve_mem_virt(ADSP_A_CORE_DUMP_MEM_ID);
	memcpy((void *)(adsp_A_dram_dump_buffer + offset),
		(void *)core_memaddr, core_memsize);
	offset = offset + core_memsize;
	/* reserved dram dump */
	memcpy((void *)(adsp_A_dram_dump_buffer + offset),
		ADSP_A_SYS_DRAM + sys_memsize, ADSP_RESERVED_DRAM_SIZE);

#ifdef CFG_RECOVERY_SUPPORT
	if (atomic_read(&adsp_reset_status) == ADSP_RESET_STATUS_START ||
		adsp_recovery_flag[ADSP_A_ID] == ADSP_RECOVERY_START) {
		/*complete adsp ee, if adsp reset by wdt or awake fail*/
		pr_info("[ADSP]aed finished, complete it\n");
		complete(&adsp_sys_reset_cp);
	}
#endif
	return 0;
}
static ssize_t adsp_A_ramdump(char *buf, loff_t offset, size_t size)
{
	unsigned int length = 0, sys_memsize = 0, core_memsize = 0;
	unsigned int threshold1 = 0, threshold2 = 0, threshold3 = 0;
	unsigned int threshold4 = 0, threshold5 = 0;
	unsigned int clk_cfg = 0, uart_cfg = 0;
	u64 core_memaddr = 0;

	sys_memsize = adsp_get_reserve_mem_size(ADSP_A_SYSTEM_MEM_ID);
	core_memsize = adsp_get_reserve_mem_size(ADSP_A_CORE_DUMP_MEM_ID);
	threshold1 = adsp_A_dump_length;
	threshold2 = threshold1 + ADSP_A_CFG_SIZE;
	threshold3 = threshold2 + sys_memsize;
	threshold4 = threshold3 + core_memsize;
	threshold5 = threshold4;

	mutex_lock(&adsp_A_excep_dump_mutex);

	/* CRASH_MEMORY_HEADER_SIZE + ADSP_A_TCM_SIZE + CRASH_REG_SIZE */
	if (offset >= 0 && offset < threshold1) {
		if ((offset + size) > threshold1)
			size = threshold1 - offset;

		memcpy(buf, adsp_A_dump_buffer + offset, size);
		length = size;
	}
	/* dump hifi3 cfgreg */
	else if (offset >= threshold1 &&
			 offset < threshold2) {
		/* IF ADSP in suspend enable clk to dump*/
		adsp_enable_dsp_clk(true);
		mutex_lock(&adsp_sw_reset_mutex);
		/* Enable ADSP CLK and UART to avoid bus hang */
		clk_cfg = readl(ADSP_CLK_CTRL_BASE);
		uart_cfg = readl(ADSP_UART_CTRL);
		writel(readl(ADSP_CLK_CTRL_BASE) | ADSP_CLK_UART_EN,
			ADSP_CLK_CTRL_BASE);
		writel(readl(ADSP_UART_CTRL) | ADSP_UART_RST_N |
			ADSP_UART_BCLK_CG, ADSP_UART_CTRL);

		if ((offset + size) > threshold2)
			size = threshold2 - offset;

		memcpy(buf, ADSP_A_CFG + offset - threshold1, size);
		length = size;

		/* Restore ADSP CLK and UART setting */
		writel(clk_cfg, ADSP_CLK_CTRL_BASE);
		writel(uart_cfg, ADSP_UART_CTRL);
		mutex_unlock(&adsp_sw_reset_mutex);
		adsp_enable_dsp_clk(false);
	}
	/* dump dram(reserved for adsp) */
	else if (offset >= threshold2 &&
			 offset < threshold3) {

		if ((offset + size) > threshold3)
			size = threshold3 - offset;

		memcpy(buf, ADSP_A_SYS_DRAM + offset - threshold2, size);
		length = size;
	}
	/* dump hifi3 core info */
	else if (offset >= threshold3 &&
			 offset < threshold4) {
		core_memaddr =
			adsp_get_reserve_mem_virt(ADSP_A_CORE_DUMP_MEM_ID);

		if ((offset + size) > threshold4)
			size = threshold4 - offset;

		memcpy(buf, (void *)core_memaddr + offset - threshold3, size);
		length = size;
	}
	else {
		offset -= threshold5;
		if ((offset + size) > 0x1000000) //Dump reserved dram 16MB
			size = 0x1000000 - offset;

		memcpy(buf, ADSP_A_SYS_DRAM + 0x700000  + offset, size);
		length = size;
	}
	mutex_unlock(&adsp_A_excep_dump_mutex);
	/* dump done*/
#ifdef CFG_RECOVERY_SUPPORT
	if (!length) {
		if (atomic_read(&adsp_reset_status) == ADSP_RESET_STATUS_START
		    || adsp_recovery_flag[ADSP_A_ID] == ADSP_RECOVERY_START) {
			/*complete scp ee, if scp reset by wdt or awake fail*/
			pr_info("[ADSP]aed finished, complete it\n");
			complete(&adsp_sys_reset_cp);
		}
	}
#endif
	return length;
}

static ssize_t adsp_A_ramdump_buf(char *buf, loff_t offset, size_t size)
{

	unsigned int length = 0, sys_memsize = 0, core_memsize = 0;
	unsigned int threshold1 = 0, threshold2 = 0;

	sys_memsize = adsp_get_reserve_mem_size(ADSP_A_SYSTEM_MEM_ID);
	core_memsize = adsp_get_reserve_mem_size(ADSP_A_CORE_DUMP_MEM_ID);
	threshold1 = adsp_A_dump_length;
	threshold2 = threshold1 + ADSP_A_CFG_SIZE +
		     sys_memsize + core_memsize + ADSP_RESERVED_DRAM_SIZE;
	/* if dynamically allocate buffer done*/
	if (adsp_A_dram_dump_buffer && adsp_A_dump_buffer) {

		mutex_lock(&adsp_A_excep_dump_mutex);

	/* CRASH_MEMORY_HEADER_SIZE + ADSP_A_TCM_SIZE + CRASH_REG_SIZE */
		if (offset >= 0 && offset < threshold1) {
			if ((offset + size) > threshold1)
				size = threshold1 - offset;
			memcpy(buf, adsp_A_dump_buffer + offset, size);
			length = size;
		} else {
			if ((offset + size) > threshold2)
				size = threshold2 - offset;
			memcpy(buf, (adsp_A_dram_dump_buffer +
				offset - threshold1), size);
			length = size;
			if (length == 0) {
				if (adsp_A_dram_dump_buffer) {
					vfree(adsp_A_dram_dump_buffer);
					adsp_A_dram_dump_buffer = NULL;
				}
			}
		}

		mutex_unlock(&adsp_A_excep_dump_mutex);

	} else
		length = adsp_A_ramdump(buf, offset, size);

	return length;
}

void adsp_dump_cfgreg(void)
{
	unsigned int clk_cfg = 0, uart_cfg = 0, offset = 0;

	adsp_enable_dsp_clk(true);
	/* all CFG registers */
	clk_cfg = readl(ADSP_CLK_CTRL_BASE);
	uart_cfg = readl(ADSP_UART_CTRL);
	writel(readl(ADSP_CLK_CTRL_BASE) | ADSP_CLK_UART_EN,
		ADSP_CLK_CTRL_BASE);
	writel(readl(ADSP_UART_CTRL) | ADSP_UART_RST_N |
		ADSP_UART_BCLK_CG, ADSP_UART_CTRL);
	memcpy(adsp_ke_buffer, ADSP_A_CFG, ADSP_A_CFG_SIZE);
	/* Restore ADSP CLK and UART setting */
	writel(clk_cfg, ADSP_CLK_CTRL_BASE);
	writel(uart_cfg, ADSP_UART_CTRL);
	offset = ADSP_A_CFG_SIZE;
	/* all TCM */
	memcpy((void *)(adsp_ke_buffer + offset),
			(void *)(ADSP_A_ITCM), (ADSP_A_ITCM_SIZE));
	offset += ADSP_A_ITCM_SIZE;
	memcpy((void *)(adsp_ke_buffer + offset),
			(void *)(ADSP_A_DTCM), (ADSP_A_DTCM_SIZE));

	adsp_enable_dsp_clk(false);
}

static ssize_t adsp_A_dump_show(struct file *filep, struct kobject *kobj,
				struct bin_attribute *attr,
				char *buf, loff_t offset, size_t size)
{
	return adsp_A_ramdump_buf(buf, offset, size);
}

static ssize_t adsp_A_dump_ke_show(struct file *filep, struct kobject *kobj,
				struct bin_attribute *attr,
				char *buf, loff_t offset, size_t size)
{
	struct MemoryDump *pMemoryDump;

	if (offset == 0) { // only do ITCM/DTCM ramdump once at start
		pMemoryDump = (struct MemoryDump *)adsp_A_dump_buffer;
		adsp_enable_dsp_clk(true);
		mutex_lock(&adsp_sw_reset_mutex);
		memset(pMemoryDump, 0x0, sizeof(*pMemoryDump));
		adsp_A_dump_length = adsp_crash_dump(pMemoryDump, ADSP_A_ID);
		mutex_unlock(&adsp_sw_reset_mutex);
		adsp_enable_dsp_clk(false);
		pr_debug("[ADSP] %s ITCM/DTCM dump done\n", __func__);
	}
	return adsp_A_ramdump(buf, offset, size);
}

struct bin_attribute bin_attr_adsp_dump = {
	.attr = {
		.name = "adsp_dump",
		.mode = 0444,
	},
	.size = 0,
	.read = adsp_A_dump_show,
};

struct bin_attribute bin_attr_adsp_dump_ke = {
	.attr = {
		.name = "adsp_dump_ke",
		.mode = 0444,
	},
	.size = 0,
	.read = adsp_A_dump_ke_show,
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
	adsp_A_detail_buffer = vmalloc(ADSP_AED_STR_LEN);
	if (!adsp_A_detail_buffer)
		return -1;

	adsp_A_dump_buffer = vmalloc(sizeof(struct MemoryDump));
	if (!adsp_A_dump_buffer)
		return -1;

	adsp_A_dump_buffer_last = vmalloc(sizeof(struct MemoryDump));
	if (!adsp_A_dump_buffer_last)
		return -1;

	adsp_ke_buffer = kmalloc(ADSP_KE_DUMP_LEN, GFP_KERNEL);
	if (!adsp_ke_buffer)
		return -1;

	/* init global values */
	adsp_A_dump_length = 0;


	return 0;
}
/*
 * ram dump init
 */
void adsp_ram_dump_init(void)
{
	/* init global values */
	adsp_A_task_context_addr = 0;

	/* ipi handler registration */
	adsp_ipi_registration(ADSP_IPI_ADSP_A_RAM_DUMP,
			      adsp_A_ram_dump_ipi_handler,
			      "A_ramdp");

	pr_debug("[ADSP] %s done\n", __func__);
}

/*
 * cleanup adsp exception
 */
void adsp_excep_cleanup(void)
{
	vfree(adsp_A_detail_buffer);
	vfree(adsp_A_dump_buffer_last);
	vfree(adsp_A_dump_buffer);
	kfree(adsp_ke_buffer);

	adsp_A_task_context_addr = 0;

	pr_debug("[ADSP] %s done\n", __func__);
}

void get_adsp_aee_buffer(unsigned long *vaddr, unsigned long *size)
{
	if (adsp_ke_buffer) {
		adsp_dump_cfgreg();
		*vaddr = (unsigned long)adsp_ke_buffer;
		*size = ADSP_KE_DUMP_LEN;
	}
}
EXPORT_SYMBOL(get_adsp_aee_buffer);
