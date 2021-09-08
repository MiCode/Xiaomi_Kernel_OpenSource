/* SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 */

#ifndef _CNSS_PLAT_IPC_QMI_H
#define _CNSS_PLAT_IPC_QMI_H

#include "cnss_plat_ipc_service_v01.h"

/* As the value of CNSS_PLAT_IPC_MAX_QMI_CLIENTS will keep changing
 * addition of new QMI client, it cannot be kept in IDL as change in
 * existing value can cause backward compatibily issue. Keep it here
 * and update its value with new QMI client ID added in enum in IDL.
 */
#define CNSS_PLAT_IPC_MAX_QMI_CLIENTS CNSS_PLAT_IPC_DAEMON_QMI_CLIENT_V01

/**
 * cnss_plat_ipc_daemon_config: Config options provided by cnss-daemon
 * @dms_mac_addr_supported: DMS MAC address provisioning support
 * @qdss_hw_trace_override: QDSS config for HW trace enable
 * @cal_file_available_bitmask: Calibration file available
 */
struct cnss_plat_ipc_daemon_config {
	u8 dms_mac_addr_supported;
	u8 qdss_hw_trace_override;
	u32 cal_file_available_bitmask;
};

typedef void (*cnss_plat_ipc_connection_update)(void *cb_ctx,
						bool connection_status);

/**
 * Persistent caldb file store which is a runtime FW param based feature will
 * fail if CONFIG_CNSS_PLAT_IPC_QMI_SVC  is not enabled.
 **/
#if IS_ENABLED(CONFIG_CNSS_PLAT_IPC_QMI_SVC)
int cnss_plat_ipc_register(enum cnss_plat_ipc_qmi_client_id_v01 client_id,
			   cnss_plat_ipc_connection_update
			   connection_update_cb, void *cb_ctx);
void cnss_plat_ipc_unregister(enum cnss_plat_ipc_qmi_client_id_v01 client_id,
			      void *cb_ctx);
int cnss_plat_ipc_qmi_file_download(enum cnss_plat_ipc_qmi_client_id_v01
				    client_id, char *file_name, char *buf,
				    u32 *size);
int cnss_plat_ipc_qmi_file_upload(enum cnss_plat_ipc_qmi_client_id_v01
				  client_id, char *file_name, u8 *file_buf,
				  u32 file_size);
struct cnss_plat_ipc_daemon_config *cnss_plat_ipc_qmi_daemon_config(void);
#else
static inline
int cnss_plat_ipc_register(enum cnss_plat_ipc_qmi_client_id_v01 client_id,
			   cnss_plat_ipc_connection_update
			   connection_update_cb, void *cb_ctx)
{
	return 0;
}

static inline
void cnss_plat_ipc_unregister(enum cnss_plat_ipc_qmi_client_id_v01 client_id,
			      void *cb_ctx)
{
}

static inline
int cnss_plat_ipc_qmi_file_download(enum cnss_plat_ipc_qmi_client_id_v01
				    client_id, char *file_name, char *buf,
				    u32 *size)
{
	return -EOPNOTSUPP;
}

static inline
int cnss_plat_ipc_qmi_file_upload(enum cnss_plat_ipc_qmi_client_id_v01
				  client_id, char *file_name, u8 *file_buf,
				  u32 file_size)
{
	return -EOPNOTSUPP;
}

static inline
struct cnss_plat_ipc_daemon_config *cnss_plat_ipc_qmi_daemon_config(void)
{
	return NULL;
}

#endif
#endif
