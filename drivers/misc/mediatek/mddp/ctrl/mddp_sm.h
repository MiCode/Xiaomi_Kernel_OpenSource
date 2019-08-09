/*
 * mddp_sm.h - Structure/API provided by MDDP state machine.
 *
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef __MDDP_SM_H
#define __MDDP_SM_H

#include <linux/types.h>

#include "mddp_ctrl.h"
#include "mddp_ipc.h"

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

	MDDP_EVT_CNT,
	MDDP_EVT_DUMMY = 0x7fff  /* Make it a 2-byte enum */
};

/*!
 * Structure of state machine entry.
 */
struct mddp_app_t;
typedef void (*mddp_sm_action_t)(struct mddp_app_t *);
typedef int32_t (*mddp_md_recv_msg_hdlr_t)(struct ipc_ilm *);
typedef int32_t (*mddp_reg_drv_cbf_t)(struct mddp_drv_handle_t *);

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
	uint32_t        ipc_md_mod_id;
};

struct mddp_md_queue_t {
	struct list_head        list;
	struct work_struct      work;
	spinlock_t              locker;
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
	struct mddp_md_queue_t      md_send_queue; /**< Send msg to MD queue. */

	mddp_reg_drv_cbf_t          reg_drv_callback; /**< Register callback. */
	struct mddp_drv_handle_t    drv_hdlr; /**< Driver handler. */

	struct mddp_sm_entry_t     *state_machines[MDDP_STATE_CNT];
};


//------------------------------------------------------------------------------
// Constant variable.
// -----------------------------------------------------------------------------
typedef uint16_t MDDP_MOD_TYPE;

static const MDDP_MOD_TYPE mddp_sm_module_list_s[] = {
	#undef MDDP_MODULE_ID
	#undef MDDP_MODULE_PREFIX
	//#undef MDDP_MODULE_PREFIX_SEMICOLON
	#define MDDP_MODULE_ID(_id) _id,
	#define MDDP_MODULE_PREFIX(_prefix)
	//#define MDDP_MODULE_PREFIX_SEMICOLON(_prefix);
	#include "mddp_app_config.h"
};

//------------------------------------------------------------------------------
// Helper macro.
// -----------------------------------------------------------------------------
#define MDDP_MOD_CNT    (sizeof(mddp_sm_module_list_s) / sizeof(MDDP_MOD_TYPE))

//------------------------------------------------------------------------------
// External functions.
// -----------------------------------------------------------------------------

// Can not use marco because of coding style check error
#if 0
#undef MDDP_MODULE_ID
#undef MDDP_MODULE_PREFIX
#define MDDP_MODULE_ID(_id)
#define MDDP_MODULE_PREFIX(_prefix) \
	extern int32_t _prefix ## _sm_init(struct mddp_app_t *app)

#include "mddp_app_config.h"
#else
int32_t mddpu_sm_init(struct mddp_app_t *app);
int32_t mddpwh_sm_init(struct mddp_app_t *app);
#endif

//------------------------------------------------------------------------------
// Public functions.
// -----------------------------------------------------------------------------
int32_t mddp_sm_init(void);
void mddp_sm_uninit(void);
struct mddp_app_t *mddp_get_default_app_inst(void);
enum mddp_state_e mddp_sm_set_state_by_md_rsp(struct mddp_app_t *app,
		enum mddp_state_e new_state, bool md_rsp_result);
#if defined __MDDP_DEBUG__
void mddp_dump_sm_table(struct mddp_app_t *app);
#else
#define mddp_dump_sm_table(...)
#endif
enum mddp_state_e mddp_sm_on_event(struct mddp_app_t *app,
		enum mddp_event_e event);
int32_t mddp_sm_msg_hdlr(enum mddp_app_type_e type, struct ipc_ilm *ilm);
int32_t mddp_sm_reg_callback(
	struct mddp_drv_conf_t *conf,
	struct mddp_drv_handle_t *handle);
#endif /* __MDDP_SM_H */
