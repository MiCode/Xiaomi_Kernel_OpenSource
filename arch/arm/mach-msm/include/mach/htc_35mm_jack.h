/* arch/arm/mach-msm/include/mach/htc_35mm_jack.h
 *
 * Copyright (C) 2009 HTC, Inc.
 * Author: Arec Kao <Arec_Kao@htc.com>
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

#ifndef HTC_35MM_REMOTE_H
#define HTC_35MM_REMOTE_H

/* Driver interfaces */
int htc_35mm_jack_plug_event(int insert, int *hpin_stable);
int htc_35mm_key_event(int key, int *hpin_stable);

/* Platform Specific Callbacks */
struct h35mm_platform_data {
	int (*plug_event_enable)(void);
	int (*headset_has_mic)(void);
	int (*key_event_enable)(void);
	int (*key_event_disable)(void);
};
#endif
