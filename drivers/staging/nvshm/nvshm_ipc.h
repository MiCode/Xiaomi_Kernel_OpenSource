/*
 * Copyright (C) 2012 NVIDIA Corporation.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _NVSHM_IPC_H
#define _NVSHM_IPC_H

/**
 * Register IPC for handle
 *
 * @param struct nvshm_handle
 * @return 0 if ok
 */
extern int nvshm_register_ipc(struct nvshm_handle *handle);

/**
 * Unregister IPC for handle
 *
 * @param struct nvshm_handle
 * @return 0 if ok
 */
extern int nvshm_unregister_ipc(struct nvshm_handle *handle);

/**
 * Generate an IPC interrupt with given mailbox content
 *
 * @param struct _nvshm_priv_handle
 * @return 0 if ok
 */
extern int nvshm_generate_ipc(struct nvshm_handle *handle);
#endif /* _NVHSM_IPC_H */
