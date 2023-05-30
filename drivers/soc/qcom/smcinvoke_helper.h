/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */
#ifndef __SMCINVOKE_HELPER_H
#define __SMCINVOKE_HELPER_H

#include <linux/types.h>
#include <linux/firmware.h>
#include <linux/qtee_shmbridge.h>
#include <soc/qcom/smci_object.h>

int smcinvoke_release_from_kernel_client(int fd);

int get_root_fd(int *root_fd);

int process_invoke_request_from_kernel_client(
		int fd, struct smcinvoke_cmd_req *req);

#endif /* __SMCINVOKE_HELPER_H */
