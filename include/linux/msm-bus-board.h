/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2010-2016, The Linux Foundation. All rights reserved.
 */

#ifndef __ASM_ARCH_MSM_BUS_BOARD_H
#define __ASM_ARCH_MSM_BUS_BOARD_H

#include <linux/types.h>
#include <linux/input.h>

enum context {
	DUAL_CTX,
	ACTIVE_CTX,
	NUM_CTX
};

struct msm_bus_fabric_registration {
	unsigned int id;
	const char *name;
	struct msm_bus_node_info *info;
	unsigned int len;
	int ahb;
	const char *fabclk[NUM_CTX];
	const char *iface_clk;
	unsigned int offset;
	unsigned int haltid;
	unsigned int rpm_enabled;
	unsigned int nmasters;
	unsigned int nslaves;
	unsigned int ntieredslaves;
	bool il_flag;
	const struct msm_bus_board_algorithm *board_algo;
	int hw_sel;
	void *hw_data;
	uint32_t qos_freq;
	uint32_t qos_baseoffset;
	u64 nr_lim_thresh;
	uint32_t eff_fact;
	uint32_t qos_delta;
	bool virt;
};

struct msm_bus_device_node_registration {
	struct msm_bus_node_device_type *info;
	unsigned int num_devices;
	bool virt;
};

enum msm_bus_bw_tier_type {
	MSM_BUS_BW_TIER1 = 1,
	MSM_BUS_BW_TIER2,
	MSM_BUS_BW_COUNT,
	MSM_BUS_BW_SIZE = 0x7FFFFFFF,
};

struct msm_bus_halt_vector {
	uint32_t haltval;
	uint32_t haltmask;
};

extern struct msm_bus_fabric_registration msm_bus_apps_fabric_pdata;
extern struct msm_bus_fabric_registration msm_bus_sys_fabric_pdata;
extern struct msm_bus_fabric_registration msm_bus_mm_fabric_pdata;
extern struct msm_bus_fabric_registration msm_bus_sys_fpb_pdata;
extern struct msm_bus_fabric_registration msm_bus_cpss_fpb_pdata;
extern struct msm_bus_fabric_registration msm_bus_def_fab_pdata;

extern struct msm_bus_fabric_registration msm_bus_8960_apps_fabric_pdata;
extern struct msm_bus_fabric_registration msm_bus_8960_sys_fabric_pdata;
extern struct msm_bus_fabric_registration msm_bus_8960_mm_fabric_pdata;
extern struct msm_bus_fabric_registration msm_bus_8960_sg_mm_fabric_pdata;
extern struct msm_bus_fabric_registration msm_bus_8960_sys_fpb_pdata;
extern struct msm_bus_fabric_registration msm_bus_8960_cpss_fpb_pdata;

extern struct msm_bus_fabric_registration msm_bus_8064_apps_fabric_pdata;
extern struct msm_bus_fabric_registration msm_bus_8064_sys_fabric_pdata;
extern struct msm_bus_fabric_registration msm_bus_8064_mm_fabric_pdata;
extern struct msm_bus_fabric_registration msm_bus_8064_sys_fpb_pdata;
extern struct msm_bus_fabric_registration msm_bus_8064_cpss_fpb_pdata;

extern struct msm_bus_fabric_registration msm_bus_9615_sys_fabric_pdata;
extern struct msm_bus_fabric_registration msm_bus_9615_def_fab_pdata;

extern struct msm_bus_fabric_registration msm_bus_8930_apps_fabric_pdata;
extern struct msm_bus_fabric_registration msm_bus_8930_sys_fabric_pdata;
extern struct msm_bus_fabric_registration msm_bus_8930_mm_fabric_pdata;
extern struct msm_bus_fabric_registration msm_bus_8930_sys_fpb_pdata;
extern struct msm_bus_fabric_registration msm_bus_8930_cpss_fpb_pdata;

extern struct msm_bus_fabric_registration msm_bus_8974_sys_noc_pdata;
extern struct msm_bus_fabric_registration msm_bus_8974_mmss_noc_pdata;
extern struct msm_bus_fabric_registration msm_bus_8974_bimc_pdata;
extern struct msm_bus_fabric_registration msm_bus_8974_ocmem_noc_pdata;
extern struct msm_bus_fabric_registration msm_bus_8974_periph_noc_pdata;
extern struct msm_bus_fabric_registration msm_bus_8974_config_noc_pdata;
extern struct msm_bus_fabric_registration msm_bus_8974_ocmem_vnoc_pdata;

extern struct msm_bus_fabric_registration msm_bus_9625_sys_noc_pdata;
extern struct msm_bus_fabric_registration msm_bus_9625_bimc_pdata;
extern struct msm_bus_fabric_registration msm_bus_9625_periph_noc_pdata;
extern struct msm_bus_fabric_registration msm_bus_9625_config_noc_pdata;

extern int msm_bus_device_match_adhoc(struct device *dev, void *id);

void msm_bus_rpm_set_mt_mask(void);
int msm_bus_board_rpm_get_il_ids(uint16_t *id);
int msm_bus_board_get_iid(int id);

#define NFAB_MSM8226 6
#define NFAB_MSM8610 5

/*
 * These macros specify the convention followed for allocating
 * ids to fabrics, masters and slaves for 8x60.
 *
 * A node can be identified as a master/slave/fabric by using
 * these ids.
 */
#define FABRIC_ID_KEY 1024
#define SLAVE_ID_KEY ((FABRIC_ID_KEY) >> 1)
#define MAX_FAB_KEY 7168  /* OR(All fabric ids) */
#define INT_NODE_START 10000

#define GET_FABID(id) ((id) & MAX_FAB_KEY)

#define NODE_ID(id) ((id) & (FABRIC_ID_KEY - 1))
#define IS_SLAVE(id) ((NODE_ID(id)) >= SLAVE_ID_KEY ? 1 : 0)
#define CHECK_ID(iid, id) (((iid & id) != id) ? -ENXIO : iid)

/*
 * The following macros are used to format the data for port halt
 * and unhalt requests.
 */
#define MSM_BUS_CLK_HALT 0x1
#define MSM_BUS_CLK_HALT_MASK 0x1
#define MSM_BUS_CLK_HALT_FIELDSIZE 0x1
#define MSM_BUS_CLK_UNHALT 0x0

#define MSM_BUS_MASTER_SHIFT(master, fieldsize) \
	((master) * (fieldsize))

#define MSM_BUS_SET_BITFIELD(word, fieldmask, fieldvalue) \
	{	\
		(word) &= ~(fieldmask);	\
		(word) |= (fieldvalue);	\
	}


#define MSM_BUS_MASTER_HALT(u32haltmask, u32haltval, master) \
	(\
	MSM_BUS_SET_BITFIELD(u32haltmask, \
		MSM_BUS_CLK_HALT_MASK<<MSM_BUS_MASTER_SHIFT((master),\
		MSM_BUS_CLK_HALT_FIELDSIZE), \
		MSM_BUS_CLK_HALT_MASK<<MSM_BUS_MASTER_SHIFT((master),\
		MSM_BUS_CLK_HALT_FIELDSIZE))\
	MSM_BUS_SET_BITFIELD(u32haltval, \
		MSM_BUS_CLK_HALT_MASK<<MSM_BUS_MASTER_SHIFT((master),\
		MSM_BUS_CLK_HALT_FIELDSIZE), \
		MSM_BUS_CLK_HALT<<MSM_BUS_MASTER_SHIFT((master),\
		MSM_BUS_CLK_HALT_FIELDSIZE))\
	)

#define MSM_BUS_MASTER_UNHALT(u32haltmask, u32haltval, master) \
	(\
	MSM_BUS_SET_BITFIELD(u32haltmask, \
		MSM_BUS_CLK_HALT_MASK<<MSM_BUS_MASTER_SHIFT((master),\
		MSM_BUS_CLK_HALT_FIELDSIZE), \
		MSM_BUS_CLK_HALT_MASK<<MSM_BUS_MASTER_SHIFT((master),\
		MSM_BUS_CLK_HALT_FIELDSIZE))\
	MSM_BUS_SET_BITFIELD(u32haltval, \
		MSM_BUS_CLK_HALT_MASK<<MSM_BUS_MASTER_SHIFT((master),\
		MSM_BUS_CLK_HALT_FIELDSIZE), \
		MSM_BUS_CLK_UNHALT<<MSM_BUS_MASTER_SHIFT((master),\
		MSM_BUS_CLK_HALT_FIELDSIZE))\
	)

#define RPM_BUS_SLAVE_REQ	0x766c7362
#define RPM_BUS_MASTER_REQ	0x73616d62

enum msm_bus_rpm_slave_field_type {
	RPM_SLAVE_FIELD_BW = 0x00007762,
};

enum msm_bus_rpm_mas_field_type {
	RPM_MASTER_FIELD_BW =		0x00007762,
	RPM_MASTER_FIELD_BW_T0 =	0x30747762,
	RPM_MASTER_FIELD_BW_T1 =	0x31747762,
	RPM_MASTER_FIELD_BW_T2 =	0x32747762,
};

#include <dt-bindings/msm/msm-bus-ids.h>


#endif /*__ASM_ARCH_MSM_BUS_BOARD_H */
