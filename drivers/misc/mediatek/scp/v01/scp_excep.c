/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/vmalloc.h>         /* needed by vmalloc */
#include <linux/sysfs.h>
#include <linux/device.h>       /* needed by device_* */
#include <linux/workqueue.h>
#include <linux/io.h>
#include <linux/mutex.h>
#include <mt-plat/aee.h>
#include <mt-plat/sync_write.h>
#include <linux/sched_clock.h>
#include <linux/ratelimit.h>
#include <linux/delay.h>
#include "scp_ipi.h"
#include "scp_helper.h"
#include "scp_excep.h"
#include "scp_l1c.h"

struct scp_aed_cfg {
	int *log;
	int log_size;
	int *phy;
	int phy_size;
	char *detail;
	struct MemoryDump *pMemoryDump;
	int memory_dump_size;
};

struct scp_status_reg {
	unsigned int pc;
	unsigned int lr;
	unsigned int psp;
	unsigned int sp;
	unsigned int m2h;
	unsigned int h2m;
};
static unsigned char *scp_A_detail_buffer;
static unsigned char *scp_A_dump_buffer;
static unsigned char *scp_A_dump_buffer_last;
static unsigned int scp_A_dump_length;
static unsigned int scp_A_task_context_addr;
static struct scp_work_struct scp_aed_work;
static struct scp_status_reg scp_A_aee_status;
static struct mutex scp_excep_mutex;
static struct mutex scp_A_excep_dump_mutex;
int scp_ee_enable;


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
		uint8_t *bufp, enum scp_core_id id)
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
	if (scp_A_task_context_addr && (id == SCP_A_ID)) {
		memcpy_from_scp((void *)&(prstatus.pr_reg),
				(void *)(SCP_TCM + scp_A_task_context_addr),
				sizeof(prstatus.pr_reg));
	}

	if (prstatus.pr_reg[15] == 0x0 && (id == SCP_A_ID))
		prstatus.pr_reg[15] = readl(SCP_A_DEBUG_PC_REG);
	if (prstatus.pr_reg[14] == 0x0 && (id == SCP_A_ID))
		prstatus.pr_reg[14] = readl(SCP_A_DEBUG_LR_REG);
	if (prstatus.pr_reg[13] == 0x0 && (id == SCP_A_ID))
		prstatus.pr_reg[13] = readl(SCP_A_DEBUG_PSP_REG);


	nhdr->p_filesz += notesize(&notes);
	return storenote(&notes, bufp);
}

void exception_header_init(void *oldbufp, enum scp_core_id id)
{
	struct elf32_phdr *nhdr, *phdr;
	struct elf32_hdr *elf;
	off_t offset = 0;

	uint8_t *bufp = oldbufp;
	uint32_t cpu;
	uint32_t dram_size = 0;

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
	phdr->p_flags = PF_R|PF_W|PF_X;
	phdr->p_offset = CRASH_MEMORY_HEADER_SIZE;
	phdr->p_vaddr = CRASH_MEMORY_OFFSET;
	phdr->p_paddr = CRASH_MEMORY_OFFSET;

#if SCP_RECOVERY_SUPPORT
	if ((int)scp_region_info_copy.ap_dram_size > 0)
		dram_size = scp_region_info_copy.ap_dram_size;
#endif

	phdr->p_filesz = CRASH_MEMORY_LENGTH + roundup(dram_size, 4);
	phdr->p_memsz = CRASH_MEMORY_LENGTH + roundup(dram_size, 4);


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
	strlcpy(prpsinfo.pr_fname, "freertos8",
		sizeof(prpsinfo.pr_fname));
	strlcpy(prpsinfo.pr_psargs, "freertos8", ELF_PRARGSZ);

	nhdr->p_filesz += notesize(&notes);
	bufp = storenote(&notes, bufp);

	/* Store pre-cpu backtrace */
	for (cpu = 0; cpu < 1; cpu++)
		bufp = core_write_cpu_note(cpu, nhdr, bufp, id);
}
/*dump scp reg*/
void scp_reg_copy(void *bufp)
{
	struct scp_reg_dump_list *scp_reg;
	uint32_t tmp;

	scp_reg = (struct scp_reg_dump_list *) bufp;

	/*setup scp reg*/
	scp_reg->scp_reg_magic = 0xDEADBEEF;
	scp_reg->ap_resource = readl(SCP_AP_RESOURCE);
	scp_reg->bus_resource = readl(SCP_BUS_RESOURCE);
	scp_reg->slp_protect = readl(SCP_SLP_PROTECT_CFG);
	scp_reg->cpu_sleep_status = readl(SCP_CPU_SLEEP_STATUS);
	scp_reg->clk_sw_sel = readl(SCP_CLK_SW_SEL);
	scp_reg->clk_enable = readl(SCP_CLK_ENABLE);
	scp_reg->clk_high_core = readl(SCP_CLK_HIGH_CORE_CG);
	scp_reg->debug_wdt_sp = readl(SCP_WDT_SP);
	scp_reg->debug_wdt_lr = readl(SCP_WDT_LR);
	scp_reg->debug_wdt_psp = readl(SCP_WDT_PSP);
	scp_reg->debug_wdt_pc = readl(SCP_WDT_PC);
	scp_reg->debug_addr_s2r = readl(SCP_DEBUG_ADDR_S2R);
	scp_reg->debug_addr_dma = readl(SCP_DEBUG_ADDR_DMA);
	scp_reg->debug_addr_spi0 = readl(SCP_DEBUG_ADDR_SPI0);
	scp_reg->debug_addr_spi1 = readl(SCP_DEBUG_ADDR_SPI1);
	scp_reg->debug_addr_spi2 = readl(SCP_DEBUG_ADDR_SPI2);
	scp_reg->debug_bus_status = readl(SCP_DEBUG_BUS_STATUS);
	/*  scp_reg->debug_infra_mon = readl(SCP_SYS_INFRA_MON); */
	tmp = readl(SCP_BUS_CTRL)&(~dbg_irq_info_sel_mask);
	writel(tmp | (0 << dbg_irq_info_sel_shift), SCP_BUS_CTRL);
	scp_reg->infra_addr_latch = readl(SCP_DEBUG_IRQ_INFO);
	writel(tmp | (1 << dbg_irq_info_sel_shift), SCP_BUS_CTRL);
	scp_reg->ddebug_latch = readl(SCP_DEBUG_IRQ_INFO);
	writel(tmp | (2 << dbg_irq_info_sel_shift), SCP_BUS_CTRL);
	scp_reg->pdebug_latch = readl(SCP_DEBUG_IRQ_INFO);
	writel(tmp | (3 << dbg_irq_info_sel_shift), SCP_BUS_CTRL);
	scp_reg->pc_value = readl(SCP_DEBUG_IRQ_INFO);
	scp_reg->scp_reg_magic_end = 0xDEADBEEF;
}

/*dump scp l1c header*/
void scp_sub_header_init(void *bufp)
{
	struct scp_dump_header_list *scp_sub_head;

	scp_sub_head = (struct scp_dump_header_list *) bufp;
	/*setup scp reg*/
	scp_sub_head->scp_head_magic = 0xDEADBEEF;
#if SCP_RECOVERY_SUPPORT
	memcpy(&scp_sub_head->scp_region_info,
		&scp_region_info_copy, sizeof(scp_region_info_copy));
#endif
	scp_sub_head->scp_head_magic_end = 0xDEADBEEF;
}

/*
 * return last lr for debugging
 */
uint32_t scp_dump_lr(void)
{
	if (is_scp_ready(SCP_A_ID))
		return readl(SCP_A_DEBUG_LR_REG);
	else
		return 0xFFFFFFFF;
}

/*
 * return last pc for debugging
 */
uint32_t scp_dump_pc(void)
{
	if (is_scp_ready(SCP_A_ID))
		return readl(SCP_A_DEBUG_PC_REG);
	else
		return 0xFFFFFFFF;
}

/*
 * dump scp register for debugging
 */
void scp_A_dump_regs(void)
{
	uint32_t tmp;

	if (is_scp_ready(SCP_A_ID)) {
		pr_debug("[SCP]ready PC:0x%x,LR:0x%x,PSP:0x%x,SP:0x%x\n"
		, readl(SCP_A_DEBUG_PC_REG), readl(SCP_A_DEBUG_LR_REG)
		, readl(SCP_A_DEBUG_PSP_REG), readl(SCP_A_DEBUG_SP_REG));
	} else {
		pr_debug("[SCP]not ready PC:0x%x,LR:0x%x,PSP:0x%x,SP:0x%x\n"
		, readl(SCP_A_DEBUG_PC_REG), readl(SCP_A_DEBUG_LR_REG)
		, readl(SCP_A_DEBUG_PSP_REG), readl(SCP_A_DEBUG_SP_REG));
	}

	pr_debug("[SCP]GIPC     0x%x\n", readl(SCP_GIPC_IN_REG));
	pr_debug("[SCP]BUS_CTRL 0x%x\n", readl(SCP_BUS_CTRL));
	pr_debug("[SCP]SLEEP_STATUS 0x%x\n", readl(SCP_CPU_SLEEP_STATUS));
	pr_debug("[SCP]INFRA_STATUS 0x%x\n", readl(INFRA_CTRL_STATUS));
	pr_debug("[SCP]IRQ_STATUS 0x%x\n", readl(SCP_INTC_IRQ_STATUS));
	pr_debug("[SCP]IRQ_ENABLE 0x%x\n", readl(SCP_INTC_IRQ_ENABLE));
	pr_debug("[SCP]IRQ_SLEEP 0x%x\n", readl(SCP_INTC_IRQ_SLEEP));
	pr_debug("[SCP]IRQ_STATUS_MSB 0x%x\n", readl(SCP_INTC_IRQ_STATUS_MSB));
	pr_debug("[SCP]IRQ_ENABLE_MSB 0x%x\n", readl(SCP_INTC_IRQ_ENABLE_MSB));
	pr_debug("[SCP]IRQ_SLEEP_MSB 0x%x\n", readl(SCP_INTC_IRQ_SLEEP_MSB));
	pr_debug("[SCP]CLK_CTRL_SEL 0x%x\n", readl(SCP_CLK_SW_SEL));
	pr_debug("[SCP]CLK_ENABLE  0x%x\n", readl(SCP_CLK_ENABLE));
	pr_debug("[SCP]SLEEP_DEBUG 0x%x\n", readl(SCP_A_SLEEP_DEBUG_REG));

	tmp = readl(SCP_BUS_CTRL)&(~dbg_irq_info_sel_mask);
	writel(tmp | (0 << dbg_irq_info_sel_shift), SCP_BUS_CTRL);
	pr_debug("[SCP]BUS:INFRA LATCH,  0x%x\n", readl(SCP_DEBUG_IRQ_INFO));
	writel(tmp | (1 << dbg_irq_info_sel_shift), SCP_BUS_CTRL);
	pr_debug("[SCP]BUS:DCACHE LATCH,  0x%x\n", readl(SCP_DEBUG_IRQ_INFO));
	writel(tmp | (2 << dbg_irq_info_sel_shift), SCP_BUS_CTRL);
	pr_debug("[SCP]BUS:ICACHE LATCH,  0x%x\n", readl(SCP_DEBUG_IRQ_INFO));
	writel(tmp | (3 << dbg_irq_info_sel_shift), SCP_BUS_CTRL);
	pr_debug("[SCP]BUS:PC LATCH,  0x%x\n", readl(SCP_DEBUG_IRQ_INFO));
}

/*
 * save scp register when scp crash
 * these data will be used to generate EE
 */
void scp_aee_last_reg(void)
{
	pr_debug("[SCP] %s begins\n", __func__);

	scp_A_aee_status.pc = readl(SCP_A_DEBUG_PC_REG);
	scp_A_aee_status.lr = readl(SCP_A_DEBUG_LR_REG);
	scp_A_aee_status.psp = readl(SCP_A_DEBUG_PSP_REG);
	scp_A_aee_status.sp = readl(SCP_A_DEBUG_SP_REG);
	scp_A_aee_status.m2h = readl(SCP_A_TO_HOST_REG);
	scp_A_aee_status.h2m = readl(SCP_GIPC_IN_REG);

	pr_debug("[SCP] %s ends\n", __func__);
}

/*
 * this function need SCP to keeping awaken
 * scp_crash_dump: dump scp tcm info.
 * @param MemoryDump:   scp dump struct
 * @param scp_core_id:  core id
 * @return:             scp dump size
 */
static unsigned int scp_crash_dump(struct MemoryDump *pMemoryDump,
		enum scp_core_id id)
{
	unsigned int lock;
	unsigned int *reg;
	unsigned int scp_dump_size;
	unsigned int scp_awake_fail_flag;
#if SCP_RECOVERY_SUPPORT
	uint32_t dram_start = 0;
#endif
	uint32_t dram_size = 0;

	/*flag use to indicate scp awake success or not*/
	scp_awake_fail_flag = 0;
	/*check SRAM lock ,awake scp*/
	if (scp_awake_lock(id) == -1) {
		pr_err("[SCP] %s: awake scp fail, scp id=%u\n", __func__, id);
		scp_awake_fail_flag = 1;
	}

	reg = (unsigned int *) (scpreg.cfg + SCP_LOCK_OFS);
	lock = *reg;
	*reg &= ~SCP_TCM_LOCK_BIT;
	dsb(SY);
	if ((*reg & SCP_TCM_LOCK_BIT)) {
		pr_debug("[SCP]unlock failed, skip dump\n");
		return 0;
	}

#if SCP_RECOVERY_SUPPORT
	/* L1C support? */
	if ((int)(scp_region_info_copy.ap_dram_size) <= 0) {
		scp_dump_size = sizeof(struct MemoryDump);
	} else {

		dram_start = scp_region_info_copy.ap_dram_start;
		dram_size = scp_region_info_copy.ap_dram_size;
		scp_dump_size = sizeof(struct MemoryDump) +
			roundup(dram_size, 4);
	}
#else
	scp_dump_size = 0;
#endif

	exception_header_init(pMemoryDump, id);
	/* init sub header*/
	scp_sub_header_init(&(pMemoryDump->scp_dump_header));

	memcpy_from_scp((void *)&(pMemoryDump->memory),
		(void *)(SCP_TCM + CRASH_MEMORY_OFFSET),
		(SCP_A_TCM_SIZE - CRASH_MEMORY_OFFSET));

	/* L1C support? */
	if (dram_size) {
		/* l1c enable,flua l1c */
		scp_l1c_flua(SCP_DL1C);
		scp_l1c_flua(SCP_IL1C);
		udelay(10);
#if SCP_RECOVERY_SUPPORT
		pr_debug("scp:scp_l1c_start_virt 0x%p\n",
			scp_l1c_start_virt);
		/* copy dram data*/
		memcpy((void *)&(pMemoryDump->scp_reg_dump),
			scp_l1c_start_virt, dram_size);
		/* dump scp reg */
#endif
		scp_reg_copy((void *)(&pMemoryDump->scp_reg_dump) +
			roundup(dram_size, 4));
	} else {
		/* dump scp reg */
		scp_reg_copy(&(pMemoryDump->scp_reg_dump));
	}

	*reg = lock;
	dsb(SY);
	/*check SRAM unlock*/
	if (scp_awake_fail_flag != 1) {
		if (scp_awake_unlock(id) == -1)
			pr_debug("[SCP]%s awake unlock fail, scp id=%u\n",
				__func__, id);
	}

	return scp_dump_size;
}
/*
 * generate aee argument without dump scp register
 * @param aed_str:  exception description
 * @param aed:      struct to store argument for aee api
 */
static void scp_prepare_aed(char *aed_str, struct scp_aed_cfg *aed)
{
	char *detail, *log;
	u8 *phy;
	u32 log_size, phy_size;

	pr_debug("[SCP] %s begins\n", __func__);

	aed->detail = NULL;
	detail = scp_A_detail_buffer;
	if (!detail)
		return;

	memset(detail, 0, SCP_AED_STR_LEN);
	snprintf(detail, SCP_AED_STR_LEN, "%s\n", aed_str);
	detail[SCP_AED_STR_LEN - 1] = '\0';

	log_size = 0;
	log = NULL;

	phy_size = 0;
	phy = NULL;

	aed->log = (int *)log;
	aed->log_size = log_size;
	aed->phy = (int *)phy;
	aed->phy_size = phy_size;
	aed->detail = detail;

	pr_debug("[SCP] %s ends\n", __func__);
}

/*
 * generate aee argument with scp register dump
 * @param aed_str:  exception description
 * @param aed:      struct to store argument for aee api
 * @param id:       identify scp core id
 */
static void scp_prepare_aed_dump(char *aed_str,
		struct scp_aed_cfg *aed,
		enum scp_core_id id)
{
	u8 *scp_detail;
	u8 *scp_dump_ptr;

	u32 memory_dump_size;
	struct MemoryDump *pMemoryDump = NULL;

	char *scp_A_log = NULL;

	pr_debug("[SCP] %s begins:%s\n", __func__, aed_str);
	scp_aee_last_reg();

	scp_A_log = scp_get_last_log(SCP_A_ID);


	/* prepare scp aee detail information */
	scp_detail = scp_A_detail_buffer;

	if (scp_detail == NULL) {
		pr_err("[SCP AEE]detail buf is null\n");
	} else {
		/* prepare scp aee detail information*/
		memset(scp_detail, 0, SCP_AED_STR_LEN);

		snprintf(scp_detail, SCP_AED_STR_LEN,
			"%s\nscp_A pc=0x%08x, lr=0x%08x, psp=0x%08x, sp=0x%08x"
			"\nlast log:\n%s",
			aed_str, scp_A_aee_status.pc, scp_A_aee_status.lr,
			scp_A_aee_status.psp, scp_A_aee_status.sp, scp_A_log);

		scp_detail[SCP_AED_STR_LEN - 1] = '\0';
	}

	/*prepare scp A db file*/
	memory_dump_size = 0;
	scp_dump_ptr = scp_A_dump_buffer_last;
	if (!scp_dump_ptr) {
		pr_err("[SCP AEE]MemoryDump buf is null, size=0x%x\n",
			memory_dump_size);
		memory_dump_size = 0;
	} else {
		pr_debug("[SCP AEE]scp A dump ptr:%p\n", scp_dump_ptr);
		pMemoryDump = (struct MemoryDump *) scp_dump_ptr;
		memset(pMemoryDump, 0x0, sizeof(*pMemoryDump));
		memory_dump_size = scp_crash_dump(pMemoryDump, SCP_A_ID);
	}
	/* scp_dump_buffer_set */
	mutex_lock(&scp_A_excep_dump_mutex);
	scp_A_dump_buffer_last = scp_A_dump_buffer;
	scp_A_dump_buffer = scp_dump_ptr;
	scp_A_dump_length = memory_dump_size;
	mutex_unlock(&scp_A_excep_dump_mutex);


	aed->log = NULL;
	aed->log_size = 0;
	aed->phy = NULL;
	aed->phy_size = 0;
	aed->detail = scp_detail;
	aed->pMemoryDump = NULL;
	aed->memory_dump_size = 0;

	pr_debug("[SCP] %s ends\n", __func__);
}

/*
 * generate an exception according to exception type
 * @param type: exception type
 */
void scp_aed(enum scp_excep_id type, enum scp_core_id id)
{
	struct scp_aed_cfg aed;
	char *scp_aed_title;

	mutex_lock(&scp_excep_mutex);
	aed.detail = NULL;
	/* get scp title and exception type*/
	switch (type) {
	case EXCEP_LOAD_FIRMWARE:
		scp_prepare_aed("scp firmware load exception", &aed);
		if (id == SCP_A_ID)
			scp_aed_title = "SCP_A load firmware exception";
		else
			scp_aed_title = "SCP load firmware exception";
		break;
	case EXCEP_RESET:
		if (id == SCP_A_ID)
			scp_aed_title = "SCP_A reset exception";
		else
			scp_aed_title = "SCP reset exception";
		break;
	case EXCEP_BOOTUP:
		if (id == SCP_A_ID)
			scp_aed_title = "SCP_A boot exception";
		else
			scp_aed_title = "SCP boot exception";
		scp_get_log(id);
		break;
	case EXCEP_RUNTIME:
		if (id == SCP_A_ID)
			scp_aed_title = "SCP_A runtime exception";
		else
			scp_aed_title = "SCP runtime exception";
		scp_get_log(id);
			break;
	default:
		if (id == SCP_A_ID)
			scp_aed_title = "SCP_A unknown exception";
		else
			scp_aed_title = "SCP unknown exception";
		break;
	}
	/*print scp message*/
	pr_debug("scp_aed_title=%s", scp_aed_title);

	if (type != EXCEP_LOAD_FIRMWARE)
		scp_prepare_aed_dump(scp_aed_title, &aed, id);
	/*print detail info. in kernel*/
	pr_debug("%s", aed.detail);

	/* scp aed api, only detail information available*/
	aed_common_exception_api("scp", NULL, 0, NULL, 0,
			aed.detail, DB_OPT_DEFAULT);

	pr_debug("[SCP] scp exception dump is done\n");

	mutex_unlock(&scp_excep_mutex);
}

/*
 * generate an exception and reset scp right now
 * NOTE: this function may be blocked and
 * should not be called in interrupt context
 * @param type: exception type
 */
void scp_aed_reset_inplace(enum scp_excep_id type, enum scp_core_id id)
{
	pr_debug("[SCP] %s begins\n", __func__);
	if (scp_ee_enable)
		scp_aed(type, id);
	else
		pr_debug("[SCP]ee disable value=%d\n", scp_ee_enable);

#if (SCP_RECOVERY_SUPPORT == 0)
	/* workaround for QA, not reset SCP in WDT */
	if (type == EXCEP_RUNTIME)
		return;

#endif

#if SCP_RECOVERY_SUPPORT
	if (atomic_read(&scp_reset_status) == RESET_STATUS_START) {
		/*complete scp ee, if scp reset by wdt or awake fail*/
		pr_debug("[SCP]aed finished, complete it\n");
		complete(&scp_sys_reset_cp);
	}
#endif
}

/*
 * callback function for work struct
 * generate an exception and reset scp
 * NOTE: this function may be blocked
 * and should not be called in interrupt context
 * @param ws:   work struct
 */
static void scp_aed_reset_ws(struct work_struct *ws)
{
	struct scp_work_struct *sws = container_of(ws,
			struct scp_work_struct, work);
	enum scp_excep_id type = (enum scp_excep_id) sws->flags;
	enum scp_core_id id = (enum scp_core_id) sws->id;

	pr_debug("[SCP] %s begins: scp_excep_id=%u scp_core_id=%u\n",
		__func__, type, id);
	scp_aed_reset_inplace(type, id);
}


/*
 * schedule a work to generate an exception and reset scp
 * @param type: exception type
 */
void scp_aed_reset(enum scp_excep_id type, enum scp_core_id id)
{
	scp_aed_work.flags = (unsigned int) type;
	scp_aed_work.id = (unsigned int) id;
	scp_schedule_work(&scp_aed_work);
}

static ssize_t scp_A_dump_show(struct file *filep,
		struct kobject *kobj, struct bin_attribute *attr,
		char *buf, loff_t offset, size_t size)
{
	unsigned int length = 0;

	mutex_lock(&scp_A_excep_dump_mutex);

	if (offset >= 0 && offset < scp_A_dump_length) {
		if ((offset + size) > scp_A_dump_length)
			size = scp_A_dump_length - offset;

		memcpy(buf, scp_A_dump_buffer + offset, size);
		length = size;
	}

	mutex_unlock(&scp_A_excep_dump_mutex);

	return length;
}


struct bin_attribute bin_attr_scp_dump = {
	.attr = {
		.name = "scp_dump",
		.mode = 0444,
	},
	.size = 0,
	.read = scp_A_dump_show,
};



/*
 * init a work struct
 */
int scp_excep_init(void)
{
	int dram_size = 0;

	mutex_init(&scp_excep_mutex);
	mutex_init(&scp_A_excep_dump_mutex);

	INIT_WORK(&scp_aed_work.work, scp_aed_reset_ws);

	/* alloc dump memory */
	scp_A_detail_buffer = vmalloc(SCP_AED_STR_LEN);
	if (!scp_A_detail_buffer)
		return -1;

#if SCP_RECOVERY_SUPPORT
	/* support L1C or not? */
	if ((int)(scp_region_info->ap_dram_size) > 0)
		dram_size = scp_region_info->ap_dram_size;
#else
	dram_size = 0;
#endif

	scp_A_dump_buffer = vmalloc(sizeof(struct MemoryDump) +
		roundup(dram_size, 4));
	if (!scp_A_dump_buffer)
		return -1;

	scp_A_dump_buffer_last = vmalloc(sizeof(struct MemoryDump) +
		roundup(dram_size, 4));
	if (!scp_A_dump_buffer_last)
		return -1;

	/* init global values */
	scp_A_dump_length = 0;
	/* 1: ee on, 0: ee disable */
	scp_ee_enable = 1;

	return 0;
}


/******************************************************************************
 * This function is called in the interrupt context. Note that scp_region_info
 * was initialized in scp_region_info_init() which must be called before this
 * function is called.
 *****************************************************************************/
void scp_ram_dump_init(void)
{
#if SCP_RECOVERY_SUPPORT
	scp_A_task_context_addr = scp_region_info->TaskContext_ptr;
	pr_debug("[SCP] get scp_A_task_context_addr: 0x%x\n",
		scp_A_task_context_addr);
#else
	scp_A_task_context_addr = 0;
#endif
}


/*
 * cleanup scp exception
 */
void scp_excep_cleanup(void)
{
	vfree(scp_A_detail_buffer);
	vfree(scp_A_dump_buffer_last);
	vfree(scp_A_dump_buffer);

	scp_A_task_context_addr = 0;

	pr_debug("[SCP] %s ends\n", __func__);
}

