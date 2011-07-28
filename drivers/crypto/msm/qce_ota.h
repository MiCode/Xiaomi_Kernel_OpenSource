/* Copyright (c) 2010-2011, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

/* Qualcomm Crypto Engine driver OTA APIi */

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
