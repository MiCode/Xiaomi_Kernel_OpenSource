/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Ping-Hsun Wu <ping-hsun.wu@mediatek.com>
 */

#ifndef __MTK_MML_DRIVER_H__
#define __MTK_MML_DRIVER_H__

#include <linux/platform_device.h>
#include <linux/mailbox_client.h>
#include <linux/mailbox/mtk-cmdq-mailbox-ext.h>
#include <linux/mailbox_controller.h>
#include <linux/of.h>

#define MML_MAX_COMPONENTS	50

struct mml_comp;
struct mml_dev;
struct mml_drm_ctx;
struct mml_topology_cache;
struct mml_task;
struct mml_comp_config;

struct platform_device *mml_get_plat_device(struct platform_device *pdev);

static inline int of_mml_count_comps(const struct device_node *np)
{
	return of_property_count_u32_elems(np, "comp-ids");
}

static inline int of_mml_read_comp_id_index(const struct device_node *np,
	u32 index, u32 *id)
{
	return of_property_read_u32_index(np, "comp-ids", index, id);
}

/*
 * mml_qos_update_tput - scan throughputs in all path client and update the max one
 *
 * @mml: The mml driver instance
 */
void mml_qos_update_tput(struct mml_dev *mml);

s32 mml_comp_init(struct platform_device *comp_pdev, struct mml_comp *comp);

s32 mml_comp_init_larb(struct mml_comp *comp, struct device *dev);
s32 mml_comp_pw_enable(struct mml_comp *comp);
s32 mml_comp_pw_disable(struct mml_comp *comp);
s32 mml_comp_clk_enable(struct mml_comp *comp);
s32 mml_comp_clk_disable(struct mml_comp *comp);

void mml_comp_qos_set(struct mml_comp *comp, struct mml_task *task,
	struct mml_comp_config *ccfg, u32 throughput);
void mml_comp_qos_clear(struct mml_comp *comp);

/*
 * mml_clock_lock - Lock clock mutex before clock counting or call clock api
 *
 * @mml: The mml driver instance
 */
void mml_clock_lock(struct mml_dev *mml);

/*
 * mml_clock_unlock - Unlock clock mutex before clock counting or call clock api
 *
 * @mml: The mml driver instance
 */
void mml_clock_unlock(struct mml_dev *mml);

s32 mml_subcomp_init(struct platform_device *comp_pdev,
	int subcomponent, struct mml_comp *comp);
s32 mml_register_comp(struct device *master, struct mml_comp *comp);
void mml_unregister_comp(struct device *master, struct mml_comp *comp);

struct mml_drm_ctx *mml_dev_get_drm_ctx(struct mml_dev *mml,
	struct mml_drm_ctx *(*ctx_create)(struct mml_dev *mml));
void mml_dev_put_drm_ctx(struct mml_dev *mml,
	void (*ctx_release)(struct mml_drm_ctx *ctx));

/*
 * mml_topology_get_cache - Get topology cache struct store in mml.
 *
 * @mml: The mml driver instance.
 *
 * Return: Topology cache struct for this mml.
 */
struct mml_topology_cache *mml_topology_get_cache(struct mml_dev *mml);

/*
 * mml_dev_get_comp_by_id - Get component instance by component id, which
 * represent specific hardware engine in MML.
 *
 * @mml: 	The mml driver instance.
 * @id:		Component ID, one of id in dt-bindings in specific IP.
 *
 * Return: pointer of component instance
 */
struct mml_comp *mml_dev_get_comp_by_id(struct mml_dev *mml, u32 id);

extern struct platform_driver mtk_mml_rdma_driver;
extern struct platform_driver mtk_mml_wrot_driver;
extern struct platform_driver mtk_mml_rsz_driver;
extern struct platform_driver mml_mutex_driver;
extern struct platform_driver mml_sys_driver;
extern struct platform_driver mtk_mml_aal_driver;
extern struct platform_driver mtk_mml_color_driver;
extern struct platform_driver mtk_mml_fg_driver;
extern struct platform_driver mtk_mml_hdr_driver;
extern struct platform_driver mtk_mml_tdshp_driver;
extern struct platform_driver mtk_mml_tcc_driver;

extern struct platform_driver mtk_mml_test_drv;

#endif	/* __MTK_MML_DRIVER_H__ */
