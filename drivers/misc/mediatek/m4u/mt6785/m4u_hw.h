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

#ifndef __M4U_HW_H__
#define __M4U_HW_H__

#define M4U_PGSIZES (SZ_4K | SZ_64K | SZ_1M | SZ_16M)

#define TOTAL_M4U_NUM           2
#define MVA_DOMAIN_NR           TOTAL_M4U_NUM
#define M4U_SEC_MVA_DOMAIN      0

#ifdef __MTK_M4U_BANK_IRQ_SUPPORT__
/* bank count: exclude normal and secure banks */
#define MTK_M4U_BANK_NODE_COUNT  (3)
#endif

/*0x7FC00000 ~ (0x8010 0000 - 1) not use in vpu_iommu*/
#define VPU_IOMMU_MVA_START     0x7FC00000
#define VPU_IOMMU_MVA_END       0x80100000

#define VPU_IOMMU_MVA_SIZE      \
	(VPU_IOMMU_MVA_END - VPU_IOMMU_MVA_START)

/* m4u0 has 2 slaves, iommu(m4u1) has 2 slave */
#define M4U_SLAVE_NUM(m4u_id)   ((m4u_id) ? 2 : 2)

/* m4u call atf debug parameter */
#define M4U_ATF_SECURITY_DEBUG_EN  (20)
#define M4U_ATF_BANK1_4_TF         (21)
#define M4U_ATF_DUMP_INFO          (22)
#define M4U_PROTECT_BANK           (3)

/* differ by chip
 * this is for larb base address, currently we map all
 *   SMI larb for code simplify
 * we can get correct hw larb id by api "m4u_port_2_larbid"
 *   using "gM4uPort" defined in platform.h
 *
 * larb 0 1 2 3 5 6 connected to 6785 iommu
 */
static const char *const gM4U_SMILARB[] = {
	"mediatek,smi_larb0", "mediatek,smi_larb1", "mediatek,smi_larb2",
	"mediatek,smi_larb3", "mediatek,smi_larb4", "mediatek,smi_larb5",
	"mediatek,smi_larb6",
};

#define SMI_LARB_NR     ARRAY_SIZE(gM4U_SMILARB)

/* seq range related */
#if 0
#define SEQ_NR_PER_MM_SLAVE    8
#define SEQ_NR_PER_PERI_SLAVE    0

#define M4U0_SEQ_NR         (SEQ_NR_PER_MM_SLAVE*M4U_SLAVE_NUM(0))
#define M4U1_SEQ_NR         (SEQ_NR_PER_PERI_SLAVE*M4U_SLAVE_NUM(1))
#define M4U_SEQ_NUM(m4u_id)   ((m4u_id) ? M4U1_SEQ_NR : M4U0_SEQ_NR)

#define M4U_SEQ_ALIGN_MSK   (0x100000-1)
#define M4U_SEQ_ALIGN_SIZE  0x100000
#endif

#define M4U0_MAU_NR    1

/* mau related */
#define MAU_NR_PER_M4U_SLAVE    1

/* CCU fault ID */
#define CCU_FAULT_ID    24

/* prog pfh dist related */
#define PROG_PFH_DIST    2

#define M4U0_PROG_PFH_NR         (PROG_PFH_DIST)
#define M4U1_PROG_PFH_NR         (PROG_PFH_DIST)
#define M4U_PROG_PFH_NUM(m4u_id)   \
	((m4u_id) ? M4U1_PROG_PFH_NR : M4U0_PROG_PFH_NR)

/* VPU_IOMMU AXI_ID */
#define VP6A_IDMA0_MSK    (0x207) /* 0x_xxxx_x100 */
#define VP6A_IDMA0_VAL    (0x4)

#define VP6A_CORE0_MSK    (0x387) /* 00_0xxx_x000 */
#define VP6A_CORE0_VAL    (0x0)


#define VP6B_IDMA1_MSK    (0x207) /* 0x_xxxx_x101 */
#define VP6B_IDMA1_VAL    (0x5)

#define VP6B_CORE1_MSK    (0x387) /* 00_0xxx_x001 */
#define VP6B_CORE1_VAL    (0x1)

struct M4U_PERF_COUNT {
	unsigned int transaction_cnt;
	unsigned int main_tlb_miss_cnt;
	unsigned int pfh_tlb_miss_cnt;
	unsigned int pfh_cnt;
	unsigned int rs_perf_cnt;
};

struct mmu_tlb_t {
	unsigned int tag;
	unsigned int desc;
};

struct mmu_pfh_tlb_t {
	unsigned int va;
	unsigned int va_msk;
	char layer;
	char x16;
	char sec;
	char pfh;
	char valid;
	unsigned int desc[MMU_PAGE_PER_LINE];
	int set;
	int way;
	unsigned int page_size;
	unsigned int tag;
};

struct m4u_port_t {
	char *name;
	unsigned m4u_id:2;
	unsigned m4u_slave:2;
	unsigned larb_id:5;
	unsigned larb_port:7;
	unsigned tf_id:12;	/* 12 bits */
	bool enable_tf;
	m4u_reclaim_mva_callback_t *reclaim_fn;
	void *reclaim_data;
	m4u_fault_callback_t *fault_fn;
	void *fault_data;
};

struct M4U_RANGE_DES_T {	/* sequential entry range */
	unsigned int Enabled;
	int port;
	unsigned int MVAStart;
	unsigned int MVAEnd;
	/* unsigned int entryCount; */
};

struct M4U_MAU_STATUS_T {	/* mau entry */
	bool Enabled;
	int port;
	unsigned int MVAStart;
	unsigned int MVAEnd;
};

struct M4U_PROG_DIST_T {	/* prog pfh dist */
	unsigned int Enabled;
	int port;
	unsigned int mm_id;
	unsigned int dir;
	unsigned int dist;
	unsigned int en;
	unsigned int sel;
};


extern struct m4u_port_t gM4uPort[];
extern int gM4u_port_num;

static inline char *m4u_get_vpu_port_name(int fault_id)
{
	fault_id &= F_MSK(9, 0);

	if ((fault_id & VP6A_IDMA0_MSK) == VP6A_IDMA0_VAL)
		return "VPU_IDMA_0";
	else if ((fault_id & VP6A_CORE0_MSK) == VP6A_CORE0_VAL)
		return "VPU_CORE_0";
	else if ((fault_id & VP6B_IDMA1_MSK) == VP6B_IDMA1_VAL)
		return "VPU_IDMA_1";
	else if ((fault_id & VP6B_CORE1_MSK) == VP6B_CORE1_VAL)
		return "VPU_CORE_1";
	else
		return "VPU_UNKNOWN";
}

static inline char *m4u_get_port_name(int portID)
{
	if ((portID < gM4u_port_num) && (portID >= M4U_PORT_DISP_POSTMASK0))
		return gM4uPort[portID].name;

	return "m4u_port_unknown";
}

static inline int m4u_get_port_by_tf_id(int m4u_id, int tf_id)
{
	int i, tf_id_old;

	tf_id_old = tf_id;

	if (m4u_id == 0) {
		tf_id &= F_MMU0_INT_ID_TF_MSK;
		if ((tf_id >> 7) == CCU_FAULT_ID)
			return M4U_PORT_CCU0;
	} else if (m4u_id == 1)
		return M4U_PORT_VPU;

	for (i = 0; i < gM4u_port_num; i++) {
		if (gM4uPort[i].tf_id == tf_id &&
			gM4uPort[i].m4u_id == m4u_id)
			return i;
	}
	M4UMSG("error: m4u_id=%d, tf_id=0x%x, domain=%d\n",
		m4u_id, tf_id_old, m4u_id);
	return gM4u_port_num;
}

static inline int m4u_port_2_larb_port(int port)
{
	return gM4uPort[port].larb_port;
}

/* return hw larb id
 * get larb id by platform define
 */
static inline int m4u_port_2_larbid(int port)
{
	if ((port < gM4u_port_num) && (port >= M4U_PORT_DISP_POSTMASK0))
		return gM4uPort[port].larb_id;
	return 0xff;
}

static inline unsigned int m4u_port_2_larb_id(unsigned int port)
{
	return m4u_port_2_larbid(port);
}

static inline int larb_2_m4u_slave_id(int larb)
{
	int i;

	for (i = 0; i < gM4u_port_num; i++) {
		if (gM4uPort[i].larb_id == larb)
			return gM4uPort[i].m4u_slave;
	}
	return 0;
}

static inline int m4u_port_2_m4u_id(int port)
{
	return gM4uPort[port].m4u_id;
}

static inline int m4u_port_2_m4u_slave_id(int port)
{
	return gM4uPort[port].m4u_slave;
}

static inline int larb_port_2_m4u_port(int larb, int larb_port)
{
	int i;

	for (i = 0; i < gM4u_port_num; i++) {
		if (gM4uPort[i].larb_id == larb &&
			gM4uPort[i].larb_port == larb_port)
			return i;
	}
	/* M4UMSG("unknown larb port: larb=%d,
	 * larb_port=%d\n", larb, larb_port);
	 */
	return M4U_PORT_UNKNOWN;
}

void m4u_print_perf_counter(int m4u_index, int m4u_slave_id, const char *msg);
int m4u_dump_reg(int m4u_index, unsigned int start, unsigned int end);
void m4u_call_atf_debug(int m4u_debug_id);
extern struct m4u_device *gM4uDev;


#ifdef M4U_TEE_SERVICE_ENABLE
extern int m4u_tee_en;
#endif

#endif
