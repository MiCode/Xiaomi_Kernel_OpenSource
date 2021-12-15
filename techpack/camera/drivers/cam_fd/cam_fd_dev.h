/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
 */

#ifndef _CAM_FD_DEV_H_
#define _CAM_FD_DEV_H_

/**
 * @brief : API to register FD Dev to platform framework.
 * @return struct platform_device pointer on on success, or ERR_PTR() on error.
 */
int cam_fd_dev_init_module(void);

/**
 * @brief : API to remove FD Dev interface from platform framework.
 */
void cam_fd_dev_exit_module(void);

#endif /* _CAM_FD_DEV_H_ */
