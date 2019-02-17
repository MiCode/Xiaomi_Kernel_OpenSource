/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * QTI Crypto Engine driver OTA API
 *
 * Copyright (c) 2010-2019, The Linux Foundation. All rights reserved.
 */

#ifndef __CRYPTO_MSM_QCE_OTA_H
#define __CRYPTO_MSM_QCE_OTA_H

#include <linux/platform_device.h>
#include <linux/qcota.h>


int qce_f8_req(void *handle, struct qce_f8_req *req,
		void *cookie, qce_comp_func_ptr_t qce_cb);
int qce_f8_multi_pkt_req(void *handle, struct qce_f8_multi_pkt_req *req,
		void *cookie, qce_comp_func_ptr_t qce_cb);
int qce_f9_req(void *handle, struct qce_f9_req *req,
		void *cookie, qce_comp_func_ptr_t qce_cb);

#endif /* __CRYPTO_MSM_QCE_OTA_H */
