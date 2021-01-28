/* SPDX-License-Identifier: GPL-2.0 */
/*
 * mddp_ctrl.h - Public API/structure provided by ctrl.
 *
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __MDDP_CTRL_H
#define __MDDP_CTRL_H

#include "mddp_export.h"

//------------------------------------------------------------------------------
// Define marco.
// -----------------------------------------------------------------------------

//------------------------------------------------------------------------------
// Struct definition.
// -----------------------------------------------------------------------------

//------------------------------------------------------------------------------
// Public functions.
// -----------------------------------------------------------------------------
struct mddp_app_t *mddp_get_app_inst(enum mddp_app_type_e type);
enum mddp_state_e mddp_get_state(struct mddp_app_t *app);
bool mddp_is_acted_state(enum mddp_app_type_e type);
uint8_t mddp_get_md_version(void);
void mddp_set_md_version(uint8_t version);

#endif				/* __MDDP_CTRL_H */
