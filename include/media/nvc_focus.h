/* Copyright (C) 2012 NVIDIA Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307, USA
 */

#ifndef __NVC_FOCUS_H__
#define __NVC_FOCUS_H__

/* NVC_FOCUS_CAP_VER0: invalid */
/* NVC_FOCUS_CAP_VER1:
 * __u32 version
 * __u32 actuator_range
 * __u32 settle_time
 */
#define NVC_FOCUS_CAP_VER1		1
/* NVC_FOCUS_CAP_VER2 adds:
 * __u32 focus_macro;
 * __u32 focus_hyper;
 * __u32 focus_infinity;
 */
#define NVC_FOCUS_CAP_VER2		2
#define NVC_FOCUS_CAP_VER		2 /* latest version */

#define AF_POS_INVALID_VALUE INT_MAX

/* These are the slew rate values coming down from the configuration */
/* Disabled is the same as fastest. Default is the default */
/* slew rate configuration in the focuser */
#define SLEW_RATE_DISABLED		0
#define SLEW_RATE_DEFAULT		1
#define SLEW_RATE_SLOWEST		9


enum nvc_focus_sts {
	NVC_FOCUS_STS_UNKNOWN		= 1,
	NVC_FOCUS_STS_NO_DEVICE,
	NVC_FOCUS_STS_INITIALIZING,
	NVC_FOCUS_STS_INIT_ERR,
	NVC_FOCUS_STS_WAIT_FOR_MOVE_END,
	NVC_FOCUS_STS_WAIT_FOR_SETTLE,
	NVC_FOCUS_STS_LENS_SETTLED,
	NVC_FOCUS_STS_FORCE32		= 0x7FFFFFFF
};

struct nvc_focus_nvc {
	__u32 focal_length;
	__u32 fnumber;
	__u32 max_aperature;
} __packed;

struct nvc_focus_cap {
	__u32 version;
	__s32 actuator_range;
	__u32 settle_time;
	__s32 focus_macro;
	__s32 focus_hyper;
	__s32 focus_infinity;
	__u32 slew_rate;
	__u32 position_translate;
} __packed;


#define NV_FOCUSER_SET_MAX              10
#define NV_FOCUSER_SET_DISTANCE_PAIR    16

struct nv_focuser_set_dist_pairs {
	__s32 fdn;
	__s32 distance;
} __packed;

struct nv_focuser_set {
	__s32 posture;
	__s32 macro;
	__s32 hyper;
	__s32 inf;
	__s32 hysteresis;
	__u32 settle_time;
	__s32 macro_offset;
	__s32 inf_offset;
	__u32 num_dist_pairs;
	struct nv_focuser_set_dist_pairs
			dist_pair[NV_FOCUSER_SET_DISTANCE_PAIR];
} __packed;

struct nv_focuser_config {
	__u32 focal_length;
	__u32 fnumber;
	__u32 max_aperture;
	__u32 range_ends_reversed;
	__s32 pos_working_low;
	__s32 pos_working_high;
	__s32 pos_actual_low;
	__s32 pos_actual_high;
	__u32 slew_rate;
	__u32 circle_of_confusion;
	__u32 num_focuser_sets;
	struct nv_focuser_set focuser_set[NV_FOCUSER_SET_MAX];
} __packed;


#endif /* __NVC_FOCUS_H__ */

