/* Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _ARCH_ARM_MACH_MSM_BUS_CORE_H
#define _ARCH_ARM_MACH_MSM_BUS_CORE_H

#include <linux/types.h>
#include <linux/device.h>
#include <linux/radix-tree.h>
#include <linux/platform_device.h>
#include <mach/msm_bus_board.h>
#include <mach/msm_bus.h>

#if defined DEBUG

#define MSM_BUS_DBG(msg, ...) \
	printk(KERN_DEBUG "AXI: %s(): " msg, __func__, ## __VA_ARGS__)

#else
#define MSM_BUS_DBG(msg, ...) no_printk("AXI")
#endif

#define MSM_BUS_ERR(msg, ...) \
	printk(KERN_ERR "AXI: %s(): " msg, __func__, ## __VA_ARGS__)
#define MSM_BUS_WARN(msg, ...) \
	printk(KERN_WARNING "AXI: %s(): " msg, __func__, ## __VA_ARGS__)
#define MSM_FAB_ERR(msg, ...) \
	dev_err(&fabric->fabdev.dev, "AXI: %s(): " msg, __func__, ## \
	__VA_ARGS__)

enum msm_bus_dbg_op_type {
	MSM_BUS_DBG_UNREGISTER = -2,
	MSM_BUS_DBG_REGISTER,
	MSM_BUS_DBG_OP = 1,
};

extern struct bus_type msm_bus_type;

struct msm_bus_node_info {
	unsigned int id;
	unsigned int priv_id;
	int gateway;
	int *masterp;
	int num_mports;
	int *slavep;
	int num_sports;
	int *tier;
	int num_tiers;
	int ahb;
	const char *slaveclk[NUM_CTX];
	const char *memclk;
	unsigned int buswidth;
};

struct path_node {
	unsigned long clk[NUM_CTX];
	unsigned long bw[NUM_CTX];
	unsigned long *sel_clk;
	unsigned long *sel_bw;
	int next;
};

struct msm_bus_link_info {
	unsigned long clk[NUM_CTX];
	unsigned long *sel_clk;
	unsigned long memclk;
	long bw[NUM_CTX];
	long *sel_bw;
	int *tier;
	int num_tiers;
};

struct nodeclk {
	struct clk *clk;
	unsigned long rate;
	bool dirty;
	bool enable;
};

struct msm_bus_inode_info {
	struct msm_bus_node_info *node_info;
	unsigned long max_bw;
	unsigned long max_clk;
	struct msm_bus_link_info link_info;
	int num_pnodes;
	struct path_node *pnode;
	int commit_index;
	struct nodeclk nodeclk[NUM_CTX];
	struct nodeclk memclk;
};

struct msm_bus_fabric_device {
	int id;
	const char *name;
	struct device dev;
	const struct msm_bus_fab_algorithm *algo;
	const struct msm_bus_board_algorithm *board_algo;
	int visited;
};
#define to_msm_bus_fabric_device(d) container_of(d, \
		struct msm_bus_fabric_device, d)


struct msm_bus_fab_algorithm {
	int (*update_clks)(struct msm_bus_fabric_device *fabdev,
		struct msm_bus_inode_info *pme, int index,
		unsigned long curr_clk, unsigned long req_clk,
		unsigned long bwsum, int flag, int ctx,
		unsigned int cl_active_flag);
	int (*port_halt)(struct msm_bus_fabric_device *fabdev, int portid);
	int (*port_unhalt)(struct msm_bus_fabric_device *fabdev, int portid);
	int (*commit)(struct msm_bus_fabric_device *fabdev);
	struct msm_bus_inode_info *(*find_node)(struct msm_bus_fabric_device
		*fabdev, int id);
	struct msm_bus_inode_info *(*find_gw_node)(struct msm_bus_fabric_device
		*fabdev, int id);
	struct list_head *(*get_gw_list)(struct msm_bus_fabric_device *fabdev);
	void (*update_bw)(struct msm_bus_fabric_device *fabdev, struct
		msm_bus_inode_info * hop, struct msm_bus_inode_info *info,
		long int add_bw, int *master_tiers, int ctx);
};

struct msm_bus_board_algorithm {
	const int board_nfab;
	void (*assign_iids)(struct msm_bus_fabric_registration *fabreg,
		int fabid);
	int (*get_iid)(int id);
};

/**
 * Used to store the list of fabrics and other info to be
 * maintained outside the fabric structure.
 * Used while calculating path, and to find fabric ptrs
 */
struct msm_bus_fabnodeinfo {
	struct list_head list;
	struct msm_bus_inode_info *info;
};

struct msm_bus_client {
	int id;
	struct msm_bus_scale_pdata *pdata;
	int *src_pnode;
	int curr;
};

int msm_bus_fabric_device_register(struct msm_bus_fabric_device *fabric);
void msm_bus_fabric_device_unregister(struct msm_bus_fabric_device *fabric);
struct msm_bus_fabric_device *msm_bus_get_fabric_device(int fabid);
int msm_bus_get_num_fab(void);

int allocate_commit_data(struct msm_bus_fabric_registration *fab_pdata,
	void **cdata);
struct msm_rpm_iv_pair *allocate_rpm_data(struct msm_bus_fabric_registration
	*fab_pdata);
int msm_bus_rpm_commit(struct msm_bus_fabric_registration
	*fab_pdata, struct msm_rpm_iv_pair *rpm_data, void **cdata);
void free_commit_data(void *cdata);
void msm_bus_rpm_update_bw(struct msm_bus_inode_info *hop,
	struct msm_bus_inode_info *info,
	struct msm_bus_fabric_registration *fab_pdata,
	void *sel_cdata, int *master_tiers,
	long int add_bw);
void msm_bus_rpm_fill_cdata_buffer(int *curr, char *buf, const int max_size,
	void *cdata, int nmasters, int nslaves, int ntslaves);
bool msm_bus_rpm_is_mem_interleaved(void);

#if defined(CONFIG_DEBUG_FS) && defined(CONFIG_MSM_BUS_SCALING)
void msm_bus_dbg_client_data(struct msm_bus_scale_pdata *pdata, int index,
	uint32_t cl);
void msm_bus_dbg_commit_data(const char *fabname, void *cdata,
	int nmasters, int nslaves, int ntslaves, int op);
#else
static inline void msm_bus_dbg_client_data(struct msm_bus_scale_pdata *pdata,
	int index, uint32_t cl)
{
}
static inline void msm_bus_dbg_commit_data(const char *fabname,
	void *cdata, int nmasters, int nslaves, int ntslaves,
	int op)
{
}
#endif

#endif /*_ARCH_ARM_MACH_MSM_BUS_CORE_H*/
