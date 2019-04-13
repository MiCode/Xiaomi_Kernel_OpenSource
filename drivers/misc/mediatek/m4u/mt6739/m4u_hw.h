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

#define TOTAL_M4U_NUM 1
#define M4U_SLAVE_NUM(m4u_id)                                                  \
	((m4u_id) ? 2 : 1) /* m4u0 has 2 slaves, iommu(m4u1) has 1 slave */

/* seq range related */
#define SEQ_NR_PER_MM_SLAVE 8
#define SEQ_NR_PER_PERI_SLAVE 0

#define M4U0_SEQ_NR (SEQ_NR_PER_MM_SLAVE * M4U_SLAVE_NUM(0))
#define M4U1_SEQ_NR (SEQ_NR_PER_PERI_SLAVE * M4U_SLAVE_NUM(1))
#define M4U_SEQ_NUM(m4u_id) ((m4u_id) ? M4U1_SEQ_NR : M4U0_SEQ_NR)

#define M4U0_MAU_NR 1

#define M4U_SEQ_ALIGN_MSK (0x100000 - 1)
#define M4U_SEQ_ALIGN_SIZE 0x100000

/* mau related */
#define MAU_NR_PER_M4U_SLAVE 1

/* smi */
#define SMI_LARB_NR 3

/* prog pfh dist related */
#define PROG_PFH_DIST 2

#define M4U0_PROG_PFH_NR (PROG_PFH_DIST)
#define M4U1_PROG_PFH_NR (PROG_PFH_DIST)
#define M4U_PROG_PFH_NUM(m4u_id)                                               \
	((m4u_id) ? M4U1_PROG_PFH_NR : M4U0_PROG_PFH_NR)

typedef struct _M4U_PERF_COUNT {
	unsigned int transaction_cnt;
	unsigned int main_tlb_miss_cnt;
	unsigned int pfh_tlb_miss_cnt;
	unsigned int pfh_cnt;
	unsigned int rs_perf_cnt;
} M4U_PERF_COUNT;

typedef struct __mmu_tlb {
	unsigned int tag;
	unsigned int desc;
} mmu_tlb_t;

typedef struct _pfh_tlb {
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
} mmu_pfh_tlb_t;

typedef struct {
	char *name;
	unsigned m4u_id : 2;
	unsigned m4u_slave : 2;
	unsigned larb_id : 4;
	unsigned larb_port : 8;
	unsigned tf_id : 12; /* 12 bits */
	bool enable_tf;
	m4u_reclaim_mva_callback_t *reclaim_fn;
	void *reclaim_data;
	m4u_fault_callback_t *fault_fn;
	void *fault_data;
} m4u_port_t;

typedef struct _M4U_RANGE_DES	/* sequential entry range */
{
	unsigned int Enabled;
	M4U_PORT_ID port;
	unsigned int MVAStart;
	unsigned int MVAEnd;
	/* unsigned int entryCount; */
} M4U_RANGE_DES_T;

typedef struct _M4U_MAU_STATUS	/* mau entry */
{
	bool Enabled;
	M4U_PORT_ID port;
	unsigned int MVAStart;
	unsigned int MVAEnd;
} M4U_MAU_STATUS_T;

typedef struct _M4U_PROG_DIST { /* prog pfh dist */
	unsigned int Enabled;
	M4U_PORT_ID port;
	unsigned int mm_id;
	unsigned int dir;
	unsigned int dist;
	unsigned int en;
	unsigned int sel;
} M4U_PROG_DIST_T;

extern m4u_port_t gM4uPort[];
extern int gM4u_port_num;

static inline char *m4u_get_port_name(M4U_PORT_ID portID)
{
	if ((portID < gM4u_port_num) && (portID >= M4U_PORT_DISP_OVL0))
		return gM4uPort[portID].name;

	return "m4u_port_unknown";
}

static inline int m4u_get_port_by_tf_id(int m4u_id, int tf_id)
{
	int i, tf_id_old;

	tf_id_old = tf_id;

	if (m4u_id == 0)
		tf_id &= F_MMU0_INT_ID_TF_MSK;

	for (i = 0; i < gM4u_port_num; i++) {
		if ((gM4uPort[i].tf_id == tf_id) &&
		    (gM4uPort[i].m4u_id == m4u_id))
			return i;
	}
	M4UMSG("error: m4u_id=%d, tf_id=0x%x\n", m4u_id, tf_id_old);
	return gM4u_port_num;
}

static inline int m4u_port_2_larb_port(M4U_PORT_ID port)
{
	if (port >= 0 && port < M4U_PORT_UNKNOWN)
		return gM4uPort[port].larb_port;
	M4UMSG("%s fail, port=%d\n", __func__, port);
	return gM4u_port_num;
}

static inline int m4u_port_2_larb_id(M4U_PORT_ID port)
{
	if ((port < gM4u_port_num) && (port >= M4U_PORT_DISP_OVL0))
		return gM4uPort[port].larb_id;
	return 0xff;
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

static inline int m4u_port_2_m4u_id(M4U_PORT_ID port)
{
	if (port >= 0 && port < gM4u_port_num)
		return gM4uPort[port].m4u_id;

	M4UMSG("%s fail, port=%d\n", __func__, port);
	return gM4u_port_num;
}

static inline int m4u_port_2_m4u_slave_id(M4U_PORT_ID port)
{
	if (port >= 0 && port < gM4u_port_num)
		return gM4uPort[port].m4u_slave;

	M4UMSG("%s fail, port=%d\n", __func__, port);
	return gM4u_port_num;
}

static inline int larb_port_2_m4u_port(int larb, int larb_port)
{
	int i;

	for (i = 0; i < gM4u_port_num; i++) {
		if (gM4uPort[i].larb_id == larb &&
		    gM4uPort[i].larb_port == larb_port)
			return i;
	}
	/* M4UMSG("unknown larb port: larb=%d, larb_port=%d\n", larb, larb_port); */
	return M4U_PORT_UNKNOWN;
}

void m4u_print_perf_counter(int m4u_index, int m4u_slave_id, const char *msg);
int m4u_dump_reg(int m4u_index, unsigned int start);
/* For build while larb0 clk has not been implemented.*/
static inline void smi_larb0_clock_on(void)
{
}
static inline void smi_larb0_clock_off(void)
{
}

extern struct m4u_device *gM4uDev;

#ifdef M4U_TEE_SERVICE_ENABLE
extern int m4u_tee_en;
#endif

#endif
