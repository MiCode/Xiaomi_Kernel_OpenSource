/* SPDX-License-Identifier: GPL-2.0 */
/*
 * mddp_sm.h - Structure/API provided by MDDP state machine.
 *
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __MDDP_SM_H
#define __MDDP_SM_H

#include <linux/completion.h>
#include <linux/workqueue.h>

#include "mddp_export.h"
#include "mddp_ipc.h"

#define MDDP_ABNORMAL_CCCI_SEND_FAILED                      (1U << 0)
#define MDDP_ABNORMAL_CHECK_FEATURE_ABSENT                  (1U << 1)
#define MDDP_ABNORMAL_WIFI_DRV_GET_FEATURE_BEFORE_MD_READY  (1U << 2)
//------------------------------------------------------------------------------
// Struct definition.
// -----------------------------------------------------------------------------
/*!
 * Event for MDDP state machine.
 */
enum mddp_event_e {
	MDDP_EVT_FUNC_ENABLE,  /**< Enable MDDP. */
	MDDP_EVT_FUNC_DISABLE,  /**< Disable MDDP. */
	MDDP_EVT_FUNC_ACT,  /**< Activate MDDP. */
	MDDP_EVT_FUNC_DEACT,  /**< Deactivate MDDP. */

	MDDP_EVT_MD_RSP_OK,  /**< MD Response OK. */
	MDDP_EVT_MD_RSP_FAIL,  /**< MD Response FAIL. */
	MDDP_EVT_MD_RSP_TIMEOUT,  /**<MD Response timeout. */

	MDDP_EVT_MD_RESET,  /**<MD send RESET. */

	MDDP_EVT_CNT,
	MDDP_EVT_DUMMY = 0x7fff  /* Make it a 2-byte enum */
};

enum mddp_sysfs_cmd_e {
	MDDP_SYSFS_CMD_ENABLE_READ,  /* User read ENABLE sysfs */
	MDDP_SYSFS_CMD_ENABLE_WRITE,  /* User write ENABLE sysfs */
	MDDP_SYSFS_CMD_STATISTIC_READ,  /* User read STATISTIC sysfs */

#ifdef MDDP_EM_SUPPORT
	MDDP_SYSFS_EM_CMD_TEST_READ,
	MDDP_SYSFS_EM_CMD_TEST_WRITE,
#endif
};

/*!
 * Structure of state machine entry.
 */
struct mddp_app_t;
typedef void (*mddp_sm_action_t)(struct mddp_app_t *);
typedef int32_t (*mddp_md_recv_msg_hdlr_t)(uint32_t msg_id,
		void *buf, uint32_t buf_len);
typedef int32_t (*mddp_reg_drv_cbf_t)(struct mddp_drv_handle_t *);
typedef ssize_t (*mddp_sysfs_cbf_t)(struct mddp_app_t *app,
				    enum mddp_sysfs_cmd_e,
				    char *buf, size_t buf_len);

struct mddp_sm_entry_t {
	enum mddp_event_e       event;
	enum mddp_state_e       new_state;
	mddp_sm_action_t        action;
};

struct mddp_ap_cfg_t {
	uint8_t         ul_dev_name[IFNAMSIZ];
	uint8_t         dl_dev_name[IFNAMSIZ];
};

struct mddp_md_cfg_t {
	uint32_t        ipc_ap_mod_id;
	enum mdfpm_user_id_e ipc_md_user_id;
};

/*!
 * MDDP application structure.
 */
struct mddp_app_t {
	uint32_t                    is_config; /**< app is configured or not. */
	enum mddp_app_type_e        type; /**< app type. */
	enum mddp_state_e           state; /**< app status. */
	spinlock_t                  locker; /**< spin locker. */

	struct mddp_ap_cfg_t        ap_cfg; /**< AP config. */
	struct mddp_md_cfg_t        md_cfg; /**< MD config. */

	mddp_md_recv_msg_hdlr_t     md_recv_msg_hdlr; /**< Recv msg from MD. */

	mddp_reg_drv_cbf_t          reg_drv_callback; /**< Register callback. */
	mddp_reg_drv_cbf_t          dereg_drv_callback; /**< DeReg callback. */
	struct mddp_drv_handle_t    drv_hdlr; /**< Driver handler. */

	mddp_sysfs_cbf_t            sysfs_callback; /**< Sysfs callback. */

	struct mddp_sm_entry_t     *state_machines[MDDP_STATE_CNT];
	uint32_t                    drv_reg;
	atomic_t                    feature;
	uint32_t                    abnormal_flags;
	uint32_t                    reset_cnt;
	struct completion           md_resp_comp;
};


//------------------------------------------------------------------------------
// Constant variable.
// -----------------------------------------------------------------------------
typedef uint16_t MDDP_MOD_TYPE;

static const MDDP_MOD_TYPE mddp_sm_module_list_s[] = {
	#undef MDDP_MODULE_ID
	#undef MDDP_MODULE_PREFIX
	#define MDDP_MODULE_ID(_id) _id,
	#define MDDP_MODULE_PREFIX(_prefix)
	#include "mddp_app_config.h"
};

//------------------------------------------------------------------------------
// Helper macro.
// -----------------------------------------------------------------------------
#define MDDP_MOD_CNT    (sizeof(mddp_sm_module_list_s) / sizeof(MDDP_MOD_TYPE))

//------------------------------------------------------------------------------
// External functions.
// -----------------------------------------------------------------------------
int32_t mddpu_sm_init(struct mddp_app_t *app);
int32_t mddpwh_sm_init(struct mddp_app_t *app);

//------------------------------------------------------------------------------
// Public functions.
// -----------------------------------------------------------------------------
int32_t mddp_sm_init(void);
void mddp_sm_uninit(void);
struct mddp_app_t *mddp_get_default_app_inst(void);
enum mddp_state_e mddp_sm_set_state_by_md_rsp(struct mddp_app_t *app,
		enum mddp_state_e new_state, bool md_rsp_result);
#ifdef __MDDP_DEBUG__
void mddp_dump_sm_table(struct mddp_app_t *app);
#else
#define mddp_dump_sm_table(...)
#endif
enum mddp_state_e mddp_sm_on_event(struct mddp_app_t *app, enum mddp_event_e event);
void mddp_sm_wait_pre(struct mddp_app_t *app);
void mddp_sm_wait(struct mddp_app_t *app, enum mddp_event_e event);

void mddp_check_feature(void);

int32_t mddp_sm_msg_hdlr(uint32_t user_id,
		uint32_t msg_id, void *buf, uint32_t buf_len);
int32_t mddp_sm_reg_callback(
	struct mddp_drv_conf_t *conf,
	struct mddp_drv_handle_t *handle);
void mddp_sm_dereg_callback(
	struct mddp_drv_conf_t *conf,
	struct mddp_drv_handle_t *handle);
void mddp_netdev_notifier_exit(void);
#endif /* __MDDP_SM_H */
