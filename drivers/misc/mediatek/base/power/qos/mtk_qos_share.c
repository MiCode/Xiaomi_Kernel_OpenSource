// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/io.h>
#include <mtk_qos_ipi.h>
#include <mtk_qos_share.h>

#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
#include <sspm_reservedmem_define.h>
#include <sspm_reservedmem.h>
#endif

struct qos_rec_data *qos_share_ref;

/* share dram for subsys related table communication */
static phys_addr_t rec_phys_addr, rec_virt_addr;
static unsigned long long rec_size;

static void qos_share_sspm_setup(void)
{
#if defined(CONFIG_MTK_TINYSYS_SSPM_SUPPORT)
	struct qos_ipi_data qos_ipi_d;

	qos_ipi_d.cmd = QOS_IPI_QOS_SHARE_INIT;
	qos_ipi_d.u.qos_share_init.dram_addr = rec_phys_addr;
	qos_ipi_d.u.qos_share_init.dram_size = rec_size;
	qos_ipi_to_sspm_command(&qos_ipi_d, 3);
#endif
}



static void qos_get_rec_addr(phys_addr_t *phys,
		       phys_addr_t *virt,
		       unsigned long long *size)
{
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
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

unsigned int qos_rec_get_hist_bw(unsigned int idx, unsigned int type)
{
	unsigned int val = 0;

	if (!qos_share_ref)
		return val;

	if (idx >= HIST_NUM || type >= BW_TYPE)
		return val;

	val = qos_share_ref->bw_hist[idx][type];

	return val;
}

unsigned int qos_rec_get_hist_data_bw(unsigned int idx, unsigned int type)
{
	unsigned int val = 0;

	if (!qos_share_ref)
		return val;

	if (idx >= HIST_NUM || type >= BW_TYPE)
		return val;

	val = qos_share_ref->data_bw_hist[idx][type];

	return val;
}

unsigned int qos_rec_get_hist_idx(void)
{
	if (qos_share_ref)
		return qos_share_ref->current_hist;
	else
		return 0xFFFF;
}
