/*
 * Platform data for Android USB
 *
 * Copyright (C) 2008 Google, Inc.
 * Author: Mike Lockwood <lockwood@android.com>
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
#ifndef	__LINUX_USB_ANDROID_H
#define	__LINUX_USB_ANDROID_H

#define FUNC_NAME_LEN 15

enum android_function_index {
	ANDROID_FFS,
	ANDROID_MBIM_BAM,
	ANDROID_ECM_BAM,
	ANDROID_AUDIO,
	ANDROID_RMNET,
	ANDROID_GPS,
	ANDROID_DIAG,
	ANDROID_QDSS_BAM,
	ANDROID_SERIAL,
	ANDROID_SERIAL_CONFIG2,
	ANDROID_CCID,
	ANDROID_ACM,
	ANDROID_MTP,
	ANDROID_PTP,
	ANDROID_RNDIS,
	ANDROID_RNDIS_BAM,
	ANDROID_ECM,
	ANDROID_NCM,
	ANDROID_UMS,
	ANDROID_ACCESSORY,
	ANDROID_AUDIO_SRC,
	ANDROID_CHARGER,
	ANDROID_MIDI,
	ANDROID_RNDIS_GSI,
	ANDROID_ECM_GSI,
	ANDROID_RMNET_GSI,
	ANDROID_MBIM_GSI,
	ANDROID_DPL_GSI,
	ANDROID_MAX_FUNC_CNT,
	ANDROID_INVALID_FUNC,
};

static enum android_function_index name_to_func_idx(const char *name)
{
	if (!name)
		return ANDROID_INVALID_FUNC;

	if (!strncasecmp("FFS", name, FUNC_NAME_LEN))
		return ANDROID_FFS;
	if (!strncasecmp("USB_MBIM", name, FUNC_NAME_LEN))
		return ANDROID_MBIM_BAM;
	if (!strncasecmp("ECM_QC", name, FUNC_NAME_LEN))
		return ANDROID_ECM_BAM;
	if (!strncasecmp("AUDIO", name, FUNC_NAME_LEN))
		return ANDROID_AUDIO;
	if (!strncasecmp("RMNET", name, FUNC_NAME_LEN))
		return ANDROID_RMNET;
	if (!strncasecmp("GPS", name, FUNC_NAME_LEN))
		return ANDROID_GPS;
	if (!strncasecmp("DIAG", name, FUNC_NAME_LEN))
		return ANDROID_DIAG;
	if (!strncasecmp("QDSS", name, FUNC_NAME_LEN))
		return ANDROID_QDSS_BAM;
	if (!strncasecmp("SERIAL", name, FUNC_NAME_LEN))
		return ANDROID_SERIAL;
	if (!strncasecmp("SERIAL_CONFIG2", name, FUNC_NAME_LEN))
		return ANDROID_SERIAL_CONFIG2;
	if (!strncasecmp("CCID", name, FUNC_NAME_LEN))
		return ANDROID_CCID;
	if (!strncasecmp("ACM", name, FUNC_NAME_LEN))
		return ANDROID_ACM;
	if (!strncasecmp("MTP", name, FUNC_NAME_LEN))
		return ANDROID_MTP;
	if (!strncasecmp("PTP", name, FUNC_NAME_LEN))
		return ANDROID_PTP;
	if (!strncasecmp("RNDIS", name, FUNC_NAME_LEN))
		return ANDROID_RNDIS;
	if (!strncasecmp("RNDIS_QC", name, FUNC_NAME_LEN))
		return ANDROID_RNDIS_BAM;
	if (!strncasecmp("ECM", name, FUNC_NAME_LEN))
		return ANDROID_ECM;
	if (!strncasecmp("NCM", name, FUNC_NAME_LEN))
		return ANDROID_NCM;
	if (!strncasecmp("MASS_STORAGE", name, FUNC_NAME_LEN))
		return ANDROID_UMS;
	if (!strncasecmp("ACCESSORY", name, FUNC_NAME_LEN))
		return ANDROID_ACCESSORY;
	if (!strncasecmp("AUDIO_SOURCE", name, FUNC_NAME_LEN))
		return ANDROID_AUDIO_SRC;
	if (!strncasecmp("CHARGING", name, FUNC_NAME_LEN))
		return ANDROID_CHARGER;
	if (!strncasecmp("MIDI", name, FUNC_NAME_LEN))
		return ANDROID_MIDI;
	if (!strncasecmp("RNDIS_GSI", name, FUNC_NAME_LEN))
		return ANDROID_RNDIS_GSI;
	if (!strncasecmp("ECM_GSI", name, FUNC_NAME_LEN))
		return ANDROID_ECM_GSI;
	if (!strncasecmp("RMNET_GSI", name, FUNC_NAME_LEN))
		return ANDROID_RMNET_GSI;
	if (!strncasecmp("MBIM_GSI", name, FUNC_NAME_LEN))
		return ANDROID_MBIM_GSI;
	if (!strncasecmp("DPL_GSI", name, FUNC_NAME_LEN))
		return ANDROID_DPL_GSI;

	return ANDROID_INVALID_FUNC;
}

enum android_pm_qos_state {
	WFI,
	IDLE_PC,
	IDLE_PC_RPM,
	NO_USB_VOTE,
	MAX_VOTES = NO_USB_VOTE,
};

struct android_usb_platform_data {
	int (*update_pid_and_serial_num)(uint32_t, const char *);
	u32 pm_qos_latency[MAX_VOTES];
	u8 usb_core_id;
};

extern int gport_setup(struct usb_configuration *c);
extern void gport_cleanup(void);
extern int gserial_init_port(int port_num, const char *name,
					const char *port_name);
extern void gserial_deinit_port(void);
extern bool gserial_is_connected(void);
extern bool gserial_is_dun_w_softap_enabled(void);
extern void gserial_dun_w_softap_enable(bool enable);
extern bool gserial_is_dun_w_softap_active(void);


int acm_port_setup(struct usb_configuration *c);
void acm_port_cleanup(void);
int acm_init_port(int port_num, const char *name);

#endif	/* __LINUX_USB_ANDROID_H */
