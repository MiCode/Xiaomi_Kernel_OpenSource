/* Copyright (c) 2011-2014, The Linux Foundation. All rights reserved.
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
#include <linux/msm-bus-board.h>
#include <linux/msm-bus.h>

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
	((fab_pdata->il_flag) ? ((bw < 0) \
	? -msm_bus_div64((ports), (-bw)) : msm_bus_div64((ports), (bw))) : (bw))
#define INTERLEAVED_VAL(fab_pdata, n) \
	((fab_pdata->il_flag) ? (n) : 1)
#define KBTOB(a) (a * 1000ULL)

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

struct msm_bus_arb_ops {
	uint32_t (*register_client)(struct msm_bus_scale_pdata *pdata);
	int (*update_request)(uint32_t cl, unsigned int index);
	void (*unregister_client)(uint32_t cl);
};

enum {
	SLAVE_NODE,
	MASTER_NODE,
	CLK_NODE,
	NR_LIM_NODE,
};


extern struct bus_type msm_bus_type;
extern struct msm_bus_arb_ops arb_ops;
extern void msm_bus_arb_setops_legacy(struct msm_bus_arb_ops *arb_ops);

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
	const char *iface_clk_node;
	unsigned int buswidth;
	unsigned int ws;
	unsigned int mode;
	unsigned int perm_mode;
	unsigned int prio_lvl;
	unsigned int prio_rd;
	unsigned int prio_wr;
	unsigned int prio1;
	unsigned int prio0;
	unsigned int num_thresh;
	u64 *th;
	u64 cur_lim_bw;
	unsigned int mode_thresh;
	bool dual_conf;
	u64 *bimc_bw;
	bool nr_lim;
	u32 ff;
	bool rt_mas;
	u32 bimc_gp;
	u32 bimc_thmp;
	u64 floor_bw;
	const char *name;
};

struct path_node {
	uint64_t clk[NUM_CTX];
	uint64_t bw[NUM_CTX];
	uint64_t *sel_clk;
	uint64_t *sel_bw;
	int next;
};

struct msm_bus_link_info {
	uint64_t clk[NUM_CTX];
	uint64_t *sel_clk;
	uint64_t memclk;
	int64_t bw[NUM_CTX];
	int64_t *sel_bw;
	int *tier;
	int num_tiers;
};

struct nodeclk {
	struct clk *clk;
	uint64_t rate;
	bool dirty;
	bool enable;
};

struct msm_bus_inode_info {
	struct msm_bus_node_info *node_info;
	uint64_t max_bw;
	uint64_t max_clk;
	uint64_t cur_lim_bw;
	uint64_t cur_prg_bw;
	struct msm_bus_link_info link_info;
	int num_pnodes;
	struct path_node *pnode;
	int commit_index;
	struct nodeclk nodeclk[NUM_CTX];
	struct nodeclk memclk[NUM_CTX];
	struct nodeclk iface_clk;
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
		int64_t add_bw);
	void (*fill_cdata_buffer)(int *curr, char *buf, const int max_size,
		void *cdata, int nmasters, int nslaves, int ntslaves);
	int (*commit)(struct msm_bus_fabric_registration
		*fab_pdata, void *hw_data, void **cdata);
	int (*port_unhalt)(uint32_t haltid, uint8_t mport);
	int (*port_halt)(uint32_t haltid, uint8_t mport);
	void (*config_master)(struct msm_bus_fabric_registration *fab_pdata,
		struct msm_bus_inode_info *info,
		uint64_t req_clk, uint64_t req_bw);
	void (*config_limiter)(struct msm_bus_fabric_registration *fab_pdata,
		struct msm_bus_inode_info *info);
};

struct msm_bus_fabric_device {
	int id;
	const char *name;
	struct device dev;
	const struct msm_bus_fab_algorithm *algo;
	const struct msm_bus_board_algorithm *board_algo;
	struct msm_bus_hw_algorithm hw_algo;
	int visited;
	int num_nr_lim;
	u64 nr_lim_thresh;
	u32 eff_fact;
};
#define to_msm_bus_fabric_device(d) container_of(d, \
		struct msm_bus_fabric_device, d)

struct msm_bus_fabric {
	struct msm_bus_fabric_device fabdev;
	int ahb;
	void *cdata[NUM_CTX];
	bool arb_dirty;
	bool clk_dirty;
	struct radix_tree_root fab_tree;
	int num_nodes;
	struct list_head gateways;
	struct msm_bus_inode_info info;
	struct msm_bus_fabric_registration *pdata;
	void *hw_data;
};
#define to_msm_bus_fabric(d) container_of(d, \
	struct msm_bus_fabric, d)


struct msm_bus_fab_algorithm {
	int (*update_clks)(struct msm_bus_fabric_device *fabdev,
		struct msm_bus_inode_info *pme, int index,
		uint64_t curr_clk, uint64_t req_clk,
		uint64_t bwsum, int flag, int ctx,
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
		int64_t add_bw, int *master_tiers, int ctx);
	void (*config_master)(struct msm_bus_fabric_device *fabdev,
		struct msm_bus_inode_info *info, uint64_t req_clk,
		uint64_t req_bw);
	void (*config_limiter)(struct msm_bus_fabric_device *fabdev,
		struct msm_bus_inode_info *info);
};

struct msm_bus_board_algorithm {
	int board_nfab;
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

uint64_t msm_bus_div64(unsigned int width, uint64_t bw);
int msm_bus_fabric_device_register(struct msm_bus_fabric_device *fabric);
void msm_bus_fabric_device_unregister(struct msm_bus_fabric_device *fabric);
struct msm_bus_fabric_device *msm_bus_get_fabric_device(int fabid);
int msm_bus_get_num_fab(void);


int msm_bus_hw_fab_init(struct msm_bus_fabric_registration *pdata,
	struct msm_bus_hw_algorithm *hw_algo);
void msm_bus_board_init(struct msm_bus_fabric_registration *pdata);
void msm_bus_board_set_nfab(struct msm_bus_fabric_registration *pdata,
	int nfab);
#if defined(CONFIG_MSM_RPM_SMD)
int msm_bus_rpm_hw_init(struct msm_bus_fabric_registration *pdata,
	struct msm_bus_hw_algorithm *hw_algo);
int msm_bus_remote_hw_commit(struct msm_bus_fabric_registration
	*fab_pdata, void *hw_data, void **cdata);
void msm_bus_rpm_fill_cdata_buffer(int *curr, char *buf, const int max_size,
	void *cdata, int nmasters, int nslaves, int ntslaves);
#else
static inline int msm_bus_rpm_hw_init(struct msm_bus_fabric_registration *pdata,
	struct msm_bus_hw_algorithm *hw_algo)
{
	return 0;
}
static inline int msm_bus_remote_hw_commit(struct msm_bus_fabric_registration
	*fab_pdata, void *hw_data, void **cdata)
{
	return 0;
}
static inline void msm_bus_rpm_fill_cdata_buffer(int *curr, char *buf,
	const int max_size, void *cdata, int nmasters, int nslaves,
	int ntslaves)
{
}
#endif

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

#ifdef CONFIG_CORESIGHT
int msmbus_coresight_init(struct platform_device *pdev);
void msmbus_coresight_remove(struct platform_device *pdev);
int msmbus_coresight_init_adhoc(struct platform_device *pdev,
		struct device_node *of_node);
void msmbus_coresight_remove_adhoc(struct platform_device *pdev);
#else
static inline int msmbus_coresight_init(struct platform_device *pdev)
{
	return 0;
}

static inline void msmbus_coresight_remove(struct platform_device *pdev)
{
}

static inline int msmbus_coresight_init_adhoc(struct platform_device *pdev,
		struct device_node *of_node)
{
	return 0;
}

static inline void msmbus_coresight_remove_adhoc(struct platform_device *pdev)
{
}
#endif


#ifdef CONFIG_OF
void msm_bus_of_get_nfab(struct platform_device *pdev,
		struct msm_bus_fabric_registration *pdata);
struct msm_bus_fabric_registration
	*msm_bus_of_get_fab_data(struct platform_device *pdev);
#else
static inline void msm_bus_of_get_nfab(struct platform_device *pdev,
		struct msm_bus_fabric_registration *pdata)
{
	return;
}

static inline struct msm_bus_fabric_registration
	*msm_bus_of_get_fab_data(struct platform_device *pdev)
{
	return NULL;
}
#endif

#endif /*_ARCH_ARM_MACH_MSM_BUS_CORE_H*/
