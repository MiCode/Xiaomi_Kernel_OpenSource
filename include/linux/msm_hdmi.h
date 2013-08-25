/* include/linux/msm_hdmi.h
 *
 * Copyright (c) 2014 The Linux Foundation. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef _MSM_HDMI_H_
#define _MSM_HDMI_H_

/*
 * HDMI cable notify handler sturcture.
 * link A link for the linked list
 * status Current status of HDMI cable connection
 * hpd_notify Callback function to provide cable status
 */
struct hdmi_cable_notify {
	struct list_head link;
	int status;
	void (*hpd_notify) (struct hdmi_cable_notify *h);
};

#ifdef CONFIG_FB_MSM_MDSS_HDMI_PANEL
/*
 * Register for HDMI cable connect or disconnect notification.
 * @param handler callback handler for notification
 * @return negative value as error otherwise current status of cable
 */
int register_hdmi_cable_notification(
		struct hdmi_cable_notify *handler);

/*
 * Un-register for HDMI cable connect or disconnect notification.
 * @param handler callback handler for notification
 * @return negative value as error
 */
int unregister_hdmi_cable_notification(
		struct hdmi_cable_notify *handler);
#else
int register_hdmi_cable_notification(
		struct hdmi_cable_notify *handler) {
	return 0;
}

int unregister_hdmi_cable_notification(
		struct hdmi_cable_notify *handler) {
	return 0;
}
#endif /* CONFIG_FB_MSM_MDSS_HDMI_PANEL */

#endif /*_MSM_HDMI_H_*/
