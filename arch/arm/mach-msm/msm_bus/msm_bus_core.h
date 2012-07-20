/* Copyright (c) 2011-2012, Code Aurora Forum. All rights reserved.
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

#define MSM_BUS_DBG(msg, ...) \
	pr_debug(msg, ## __VA_ARGS__)
#define MSM_BUS_ERR(msg, ...) \
	pr_err(msg, ## __VA_ARGS__)
#define MSM_BUS_WARN(msg, ...) \
	pr_warn(msg, ## __VA_ARGS__)
#define MSM_FAB_ERR(msg, ...) \
	dev_err(&fabric->fabdev.dev, msg, ## __VA_ARGS__)

#define IS_MASTER_VALID(mas) \
	(((mas >= MSM_BUS_MASTER_FIRST) && (mas <= MSM_BUS_MASTER_LAST)) \
	 ? 1 : 0)
#define IS_SLAVE_VALID(slv) \
	(((slv >= MSM_BUS_SLAVE_FIRST) && (slv <= MSM_BUS_SLAVE_LAST)) ? 1 : 0)

#define INTERLEAVED_BW(fab_pdata, bw, ports) \
	((fab_pdata->il_flag) ? DIV_ROUND_UP((bw), (ports)) : (bw))
#define INTERLEAVED_VAL(fab_pdata, n) \
	((fab_pdata->il_flag) ? (n) : 1)

enum msm_bus_dbg_op_type {
	MSM_BUS_DBG_UNREGISTER = -2,
	MSM_BUS_DBG_REGISTER,
	MSM_BUS_DBG_OP = 1,
};

enum msm_bus_hw_sel {
	MSM_BUS_RPM = 0,
	MSM_BUS_NOC,
	MSM_BUS_BIMC,
};

extern struct bus_type msm_bus_type;

struct msm_bus_node_info {
	unsigned int id;
	unsigned int priv_id;
	unsigned int mas_hw_id;
	unsigned int slv_hw_id;
	int gateway;
	int *masterp;
	int *qport;
	int num_mports;
	int *slavep;
	int num_sports;
	int *tier;
	int num_tiers;
	int ahb;
	int hw_sel;
	const char *slaveclk[NUM_CTX];
	const char *memclk[NUM_CTX];
	unsigned int buswidth;
	unsigned int ws;
	unsigned int mode;
	unsigned int perm_mode;
	unsigned int prio_lvl;
	unsigned int prio_rd;
	unsigned int prio_wr;
	unsigned int prio1;
	unsigned int prio0;
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
	struct nodeclk memclk[NUM_CTX];
	void *hw_data;
};

struct msm_bus_node_hw_info {
	bool dirty;
	unsigned int hw_id;
	uint64_t bw;
};

struct msm_bus_hw_algorithm {
	int (*allocate_commit_data)(struct msm_bus_fabric_registration
		*fab_pdata, void **cdata, int ctx);
	void *(*allocate_hw_data)(struct platform_device *pdev,
		struct msm_bus_fabric_registration *fab_pdata);
	void (*node_init)(void *hw_data, struct msm_bus_inode_info *info);
	void (*free_commit_data)(void *cdata);
	void (*update_bw)(struct msm_bus_inode_info *hop,
		struct msm_bus_inode_info *info,
		struct msm_bus_fabric_registration *fab_pdata,
		void *sel_cdata, int *master_tiers,
		long int add_bw);
	void (*fill_cdata_buffer)(int *curr, char *buf, const int max_size,
		void *cdata, int nmasters, int nslaves, int ntslaves);
	int (*commit)(struct msm_bus_fabric_registration
		*fab_pdata, void *hw_data, void **cdata);
	int (*port_unhalt)(uint32_t haltid, uint8_t mport);
	int (*port_halt)(uint32_t haltid, uint8_t mport);
};

struct msm_bus_fabric_device {
	int id;
	const char *name;
	struct device dev;
	const struct msm_bus_fab_algorithm *algo;
	const struct msm_bus_board_algorithm *board_algo;
	struct msm_bus_hw_algorithm hw_algo;
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

int msm_bus_remote_hw_commit(struct msm_bus_fabric_registration
	*fab_pdata, void *hw_data, void **cdata);
int msm_bus_fabric_device_register(struct msm_bus_fabric_device *fabric);
void msm_bus_fabric_device_unregister(struct msm_bus_fabric_device *fabric);
struct msm_bus_fabric_device *msm_bus_get_fabric_device(int fabid);
int msm_bus_get_num_fab(void);

void msm_bus_rpm_fill_cdata_buffer(int *curr, char *buf, const int max_size,
	void *cdata, int nmasters, int nslaves, int ntslaves);

int msm_bus_hw_fab_init(struct msm_bus_fabric_registration *pdata,
	struct msm_bus_hw_algorithm *hw_algo);
int msm_bus_rpm_hw_init(struct msm_bus_fabric_registration *pdata,
	struct msm_bus_hw_algorithm *hw_algo);
int msm_bus_noc_hw_init(struct msm_bus_fabric_registration *pdata,
	struct msm_bus_hw_algorithm *hw_algo);
int msm_bus_bimc_hw_init(struct msm_bus_fabric_registration *pdata,
	struct msm_bus_hw_algorithm *hw_algo);
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
