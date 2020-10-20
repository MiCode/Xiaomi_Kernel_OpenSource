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
bool mddp_is_acted_state(enum mddp_app_type_e type);

#endif				/* __MDDP_CTRL_H */
