// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/io.h>
#include <mtk_qos_ipi.h>
#include <mtk_qos_share.h>

#if IS_ENABLED(CONFIG_MTK_TINYSYS_SSPM_V2) || IS_ENABLED(CONFIG_MTK_TINYSYS_SSPM_V3)
#include <sspm_reservedmem.h>
#include <sspm_reservedmem_define.h>
#endif

static void __iomem *qos_share_sram_base;
static unsigned int qos_share_sram_bound;

struct qos_rec_data *qos_share_ref;

static int qos_share_use_sram;

/* share dram for subsys related table communication */
static phys_addr_t rec_phys_addr, rec_virt_addr;
static unsigned long long rec_size;

static void qos_share_sspm_setup(void)
{
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SSPM_V2)
	struct qos_ipi_data qos_ipi_d;

	qos_ipi_d.cmd = QOS_IPI_QOS_SHARE_INIT;
	qos_ipi_d.u.qos_share_init.dram_addr = rec_phys_addr;
	qos_ipi_d.u.qos_share_init.dram_size = rec_size;
	qos_ipi_to_sspm_command(&qos_ipi_d, 3);
#elif IS_ENABLED(CONFIG_MTK_TINYSYS_SSPM_V3)
	struct qos_ipi_data qos_ipi_d;

	qos_ipi_d.cmd = QOS_IPI_QOS_SHARE_INIT;
	qos_ipi_d.u.qos_share_init.dram_addr = rec_phys_addr;
	qos_ipi_d.u.qos_share_init.dram_size = rec_size;
	qos_ipi_to_sspm_scmi_command(qos_ipi_d.cmd,
						qos_ipi_d.u.qos_share_init.dram_addr,
						qos_ipi_d.u.qos_share_init.dram_size, 0, 0);
#endif
}

static void qos_get_rec_addr(phys_addr_t *phys,
		       phys_addr_t *virt,
		       unsigned long long *size)
{
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SSPM_V2) || IS_ENABLED(CONFIG_MTK_TINYSYS_SSPM_V3)
	/* get sspm reserved mem */
	*phys = sspm_reserve_mem_get_phys(QOS_MEM_ID);
	*virt = sspm_reserve_mem_get_virt(QOS_MEM_ID);
	*size = sspm_reserve_mem_get_size(QOS_MEM_ID);

	pr_info("qos: rec phy_addr = 0x%llx, virt_addr=0x%llx, size = %llu\n",
		(unsigned long long) *phys,
		(unsigned long long) *virt,
		*size);

#endif
}

static int qos_reserve_mem_init(phys_addr_t *virt,
			   unsigned long long *size)
{
	int i;
	unsigned char *ptr;

	if (!virt)
		return -1;

	/* clear reserve mem */
	ptr = (unsigned char *)(uintptr_t)*virt;
	for (i = 0; i < *size; i++)
		ptr[i] = 0x0;

	return 0;
}

int qos_init_rec_share(void)
{
	int ret = 0;

	qos_get_rec_addr(&rec_phys_addr,
			  &rec_virt_addr,
			  &rec_size);

	qos_share_ref = (struct qos_rec_data *)(uintptr_t)rec_virt_addr;

	if (!qos_share_ref) {
		pr_info("qos: get sspm dram addr failed\n");
		ret = -1;
		goto end;
	}

	ret = qos_reserve_mem_init(&rec_virt_addr, &rec_size);
	qos_share_sspm_setup();

end:
	return ret;

}
EXPORT_SYMBOL_GPL(qos_init_rec_share);

unsigned int qos_rec_get_hist_bw(unsigned int idx, unsigned int type)
{
	unsigned int val = 0;

	if (!qos_share_ref)
		return val;

	if (idx >= HIST_NUM || type >= BW_TYPE)
		return val;

	if (qos_share_use_sram)
		val = qos_share_sram_read(QOS_SHARE_HIST_BW + (BW_TYPE * idx + type)*4);
	else
		val = qos_share_ref->bw_hist[idx][type];

	return val;
}
EXPORT_SYMBOL_GPL(qos_rec_get_hist_bw);

unsigned int qos_rec_get_hist_data_bw(unsigned int idx, unsigned int type)
{
	unsigned int val = 0;

	if (!qos_share_ref)
		return val;

	if (idx >= HIST_NUM || type >= BW_TYPE)
		return val;

	if (qos_share_use_sram)
		val = qos_share_sram_read(QOS_SHARE_HIST_DATA_BW + (BW_TYPE * idx + type)*4);
	else
		val = qos_share_ref->data_bw_hist[idx][type];

	return val;
}
EXPORT_SYMBOL_GPL(qos_rec_get_hist_data_bw);

unsigned int qos_rec_get_hist_idx(void)
{
	if (qos_share_use_sram)
		return qos_share_sram_read(QOS_SHARE_CURR_IDX);
	else if (qos_share_ref)
		return qos_share_ref->current_hist;
	else
		return 0xFFFF;
}
EXPORT_SYMBOL_GPL(qos_rec_get_hist_idx);
int qos_share_init_sram(void __iomem *regs, unsigned int bound)
{
	int i;

	qos_share_sram_base = regs;
	qos_share_sram_bound = bound;

	pr_info("qos share sram addr:0x%p len:%d\n",
		qos_share_sram_base, qos_share_sram_bound);
	qos_share_use_sram = 1;
	/* init zero except for version addr */
	for (i = 0; i < bound; i += 4)
		writel(0x0, qos_share_sram_base+i);

	pr_info("qos share use sram = %d\n", qos_share_use_sram);
	return 0;
}
EXPORT_SYMBOL_GPL(qos_share_init_sram);

extern u32 qos_share_sram_read(u32 offset)
{
	if (!qos_share_sram_base || offset >= qos_share_sram_bound)
		return 0;

	return readl(qos_share_sram_base + offset);
}
EXPORT_SYMBOL_GPL(qos_share_sram_read);
