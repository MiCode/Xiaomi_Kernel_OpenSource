/* SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 */

#ifndef _CNSS_PLAT_IPC_QMI_H
#define _CNSS_PLAT_IPC_QMI_H

/**
 * struct cnss_plat_user_config: Config options provided by user space
 * @dms_mac_addr_supported: DMS MAC address provisioning support
 * @qdss_hw_trace_override: QDSS config for HW trace enable
 * @cal_file_available_bitmask: Calibration file available
 */
struct cnss_plat_ipc_user_config {
	u8 dms_mac_addr_supported;
	u8 qdss_hw_trace_override;
	u32 cal_file_available_bitmask;
};

typedef void (*cnss_plat_ipc_connection_update)(void *cb_ctx,
						bool connection_status);

int cnss_plat_ipc_register(cnss_plat_ipc_connection_update
			   connection_update_cb, void *cb_ctx);
void cnss_plat_ipc_unregister(void *cb_ctx);
int cnss_plat_ipc_qmi_file_download(char *file_name, char *buf, u32 *size);
int cnss_plat_ipc_qmi_file_upload(char *file_name, u8 *file_buf,
				  u32 file_size);
struct cnss_plat_ipc_user_config *cnss_plat_ipc_qmi_user_config(void);
#endif
