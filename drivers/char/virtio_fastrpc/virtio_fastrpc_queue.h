/* SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef __VIRTIO_FASTRPC_QUEUE_H__
#define __VIRTIO_FASTRPC_QUEUE_H__

#include "virtio_fastrpc_core.h"

void *get_a_tx_buf(struct vfastrpc_file *vfl);
void vfastrpc_rxbuf_send(struct vfastrpc_file *vfl, void *data, unsigned int len);
int vfastrpc_txbuf_send(struct vfastrpc_file *vfl, void *data, unsigned int len);
#endif /*__VIRTIO_FASTRPC_QUEUE_H__*/
