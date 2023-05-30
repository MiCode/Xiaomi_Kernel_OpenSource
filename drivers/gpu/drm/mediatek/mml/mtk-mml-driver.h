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
struct mml_drm_param;
struct mml_dle_ctx;
struct mml_dle_param;
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
 *
 * Return: throughput upper bound from opp table
 */
u32 mml_qos_update_tput(struct mml_dev *mml);

s32 mml_comp_init(struct platform_device *comp_pdev, struct mml_comp *comp);

s32 mml_comp_init_larb(struct mml_comp *comp, struct device *dev);
s32 mml_comp_pw_enable(struct mml_comp *comp);
s32 mml_comp_pw_disable(struct mml_comp *comp);
s32 mml_comp_clk_enable(struct mml_comp *comp);
s32 mml_comp_clk_disable(struct mml_comp *comp);

void mml_comp_qos_set(struct mml_comp *comp, struct mml_task *task,
	struct mml_comp_config *ccfg, u32 throughput, u32 tput_up);
void mml_comp_qos_clear(struct mml_comp *comp);

/*
 * mml_sram_get - get sram addr from slbc and power on
 *
 * @mml:	The mml driver instance
 *
 * Return:	The address of sram
 */
void __iomem *mml_sram_get(struct mml_dev *mml);

/*
 * mml_sram_put - power off sram and release slbc object
 *
 * @mml:	The mml driver instance
 */
void mml_sram_put(struct mml_dev *mml);

/*
 * mml_sram_get_racing_height - get racing height in mml cache
 *
 * @mml:	The mml driver instance
 *
 * Return:	The racing height in pixel.
 */
u8 mml_sram_get_racing_height(struct mml_dev *mml);

/*
 * mml_racing_enable - enable racing mode (inline rotate) or not
 *
 * @mml:	The mml driver instance
 *
 * Return:	True for enable, false for disable.
 */
bool mml_racing_enable(struct mml_dev *mml);

/*
 * mml_ir_get_mml_ready_event - get inline rot sync event mml_ready
 *
 * @mml:	The mml driver instance
 *
 * Return:	The event id of mml_ready
 */
u16 mml_ir_get_mml_ready_event(struct mml_dev *mml);

/*
 * mml_ir_get_disp_ready_event - get inline rot sync event disp_ready
 *
 * @mml:	The mml driver instance
 *
 * Return:	The event id of disp_ready
 */
u16 mml_ir_get_disp_ready_event(struct mml_dev *mml);

/*
 * mml_ir_get_mml_stop_event - get inline rot sync event mml_stop
 *
 * @mml:	The mml driver instance
 *
 * Return:	The event id of disp_ready
 */
u16 mml_ir_get_mml_stop_event(struct mml_dev *mml);

/*
 * mml_dump_thread - dump cmdq threads hold by mml
 *
 * @mml:	The mml driver instance
 */
void mml_dump_thread(struct mml_dev *mml);

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

/*
 * mml_lock_wake_lock - wake lock to prevent system off
 *
 * @mml: The mml driver instance
 */
void mml_lock_wake_lock(struct mml_dev *mml, bool lock);

/*
 * mml_record_track - record mml task behavior for debug print
 *
 * @mml: The mml driver instance
 * @task: The mml task to record
 */
void mml_record_track(struct mml_dev *mml, struct mml_task *task);

/*
 * mml_record_dump - dump records to log
 *
 * @mml: The mml driver instance
 */
void mml_record_dump(struct mml_dev *mml);

s32 mml_subcomp_init(struct platform_device *comp_pdev,
	int subcomponent, struct mml_comp *comp);
s32 mml_register_comp(struct device *master, struct mml_comp *comp);
void mml_unregister_comp(struct device *master, struct mml_comp *comp);

struct mml_drm_ctx *mml_dev_get_drm_ctx(struct mml_dev *mml,
	struct mml_drm_param *disp,
	struct mml_drm_ctx *(*ctx_create)(struct mml_dev *mml,
	struct mml_drm_param *disp));
void mml_dev_put_drm_ctx(struct mml_dev *mml,
	void (*ctx_release)(struct mml_drm_ctx *ctx));
struct mml_dle_ctx *mml_dev_get_dle_ctx(struct mml_dev *mml,
	struct mml_dle_param *dl,
	struct mml_dle_ctx *(*ctx_create)(struct mml_dev *mml,
	struct mml_dle_param *dl));
void mml_dev_put_dle_ctx(struct mml_dev *mml,
	void (*ctx_release)(struct mml_dle_ctx *ctx));

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

extern struct platform_driver mml_sys_driver;
extern struct platform_driver mml_aal_driver;
extern struct platform_driver mml_color_driver;
extern struct platform_driver mml_fg_driver;
extern struct platform_driver mml_hdr_driver;
extern struct platform_driver mml_mutex_driver;
extern struct platform_driver mml_rdma_driver;
extern struct platform_driver mml_rsz_driver;
extern struct platform_driver mml_tcc_driver;
extern struct platform_driver mml_tdshp_driver;
extern struct platform_driver mml_wrot_driver;

#if IS_ENABLED(CONFIG_MTK_MML_DEBUG)
extern struct platform_driver mtk_mml_test_drv;
#endif

#endif	/* __MTK_MML_DRIVER_H__ */
