/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*
 * Copyright (c) 2018 MediaTek Inc.
 */

#ifndef __UAPI_MTK_CCD_CONTROLS_H__
#define __UAPI_MTK_CCD_CONTROLS_H__

#define CCD_NAME_MAX_LEN		(32)
#define CCD_BUF_MAX_SIZE		(1024)

#define CCD_LISTEN_OBJECT_PREPARING     (0)
#define CCD_LISTEN_OBJECT_READY		(1)

enum ccd_master_cmd {
	CCD_MASTER_CMD_CREATE = 1,
	CCD_MASTER_CMD_DESTROY
};

enum ccd_master_state {
	CCD_MASTER_INIT = 0,
	CCD_MASTER_ACTIVE,
	CCD_MASTER_EXIT
};

struct ccd_master_status_item {
	unsigned int state;
};

struct ccd_master_listen_item {
	unsigned int src;
	char name[CCD_NAME_MAX_LEN];
	unsigned int cmd;
};

struct ccd_worker_item {
	unsigned int	src;
	unsigned int	id;
	unsigned char	sbuf[CCD_BUF_MAX_SIZE];
	unsigned int	len;
};

#define IOCTL_CCD_MASTER_INIT	 _IOWR('c', 1, struct ccd_master_status_item)
#define IOCTL_CCD_MASTER_LISTEN  _IOWR('c', 2, struct ccd_master_listen_item)
#define IOCTL_CCD_MASTER_DESTROY _IOWR('c', 3, struct ccd_master_status_item)
#define IOCTL_CCD_WORKER_READ	 _IOWR('c', 4, struct ccd_worker_item)
#define IOCTL_CCD_WORKER_WRITE	 _IOWR('c', 5, struct ccd_worker_item)

/**
 * enum ipi_id - the id of inter-processor interrupt
 *
 * @SCP_IPI_INIT:	 The interrupt from scp is to notfiy kernel
 *			 SCP initialization completed.
 *			 IPI_SCP_INIT is sent from SCP when firmware is
 *			 loaded. AP doesn't need to send IPI_SCP_INIT
 *			 command to SCP.
 *			 For other IPI below, AP should send the request
 *			 to SCP to trigger the interrupt.
 * @CCD_IPI_MAX:	 The maximum IPI number
 */

enum ccd_ipi_id {
	CCD_IPI_INIT = 0,
	CCD_IPI_ISP_MAIN,
	CCD_IPI_ISP_SLAVE,
	CCD_IPI_ISP_TRICAM,
	CCD_IPI_MRAW_CMD,
	CCD_IPI_FD_CMD,
	CCD_IPI_MAX
};

/**
 * struct mem_obj - memory buffer allocated in kernel
 *
 * @iova:	iova of buffer
 * @len:	buffer length
 * @pa:	physical address
 * @va: kernel virtual address
 */
struct ccd_mem_obj {
	unsigned int iova;
	unsigned int len;
	unsigned long long pa;
	unsigned long long va;
};
#endif

