/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
 */

#ifndef _CAM_VFE_H_
#define _CAM_VFE_H_

/**
 * @brief : API to register VFE hw to platform framework.
 * @return struct platform_device pointer on on success, or ERR_PTR() on error.
 */
int cam_vfe_init_module(void);

/**
 * @brief : API to remove VFE  Hw from platform framework.
 */
void cam_vfe_exit_module(void);

#endif /* _CAM_VFE_H_ */
