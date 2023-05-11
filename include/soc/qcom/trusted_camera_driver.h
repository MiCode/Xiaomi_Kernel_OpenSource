/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef __TRUSTED_CAMERA_DRIVER_H
#define __TRUSTED_CAMERA_DRIVER_H

#include <soc/qcom/smci_object.h>
#include "trusted_camera_driver_notify.h"

struct tc_driver_sensor_info {
	uint32_t version;
	uint32_t protect;
	uint32_t csid_hw_idx_mask;
	uint32_t cdm_hw_idx_mask;
	uint64_t vc_mask;
	uint64_t phy_lane_sel_mask;
	uint64_t reserved;
};

struct port_info {
	uint32_t hw_type;
	uint32_t protect;
	uint32_t phy_id;
	uint32_t mask;
};

#define ITRUSTEDCAMERADRIVER_IFE0 0
#define ITRUSTEDCAMERADRIVER_IFE1 1
#define ITRUSTEDCAMERADRIVER_IFE2 2
#define ITRUSTEDCAMERADRIVER_IFE3 3
#define ITRUSTEDCAMERADRIVER_IFE4 4
#define ITRUSTEDCAMERADRIVER_IFE5 5
#define ITRUSTEDCAMERADRIVER_IFE6 6
#define ITRUSTEDCAMERADRIVER_IFE7 7
#define ITRUSTEDCAMERADRIVER_IFE8 8
#define ITRUSTEDCAMERADRIVER_IFE9 9
#define ITRUSTEDCAMERADRIVER_IFE_LITE_0 10
#define ITRUSTEDCAMERADRIVER_IFE_LITE_1 11
#define ITRUSTEDCAMERADRIVER_IFE_LITE_2 12
#define ITRUSTEDCAMERADRIVER_IFE_LITE_3 13
#define ITRUSTEDCAMERADRIVER_IFE_LITE_4 14
#define ITRUSTEDCAMERADRIVER_IFE_LITE_5 15
#define ITRUSTEDCAMERADRIVER_IFE_LITE_6 16
#define ITRUSTEDCAMERADRIVER_IFE_LITE_7 17
#define ITRUSTEDCAMERADRIVER_IFE_LITE_8 18
#define ITRUSTEDCAMERADRIVER_IFE_LITE_9 19
#define ITRUSTEDCAMERADRIVER_HWTYPE_MAX 20

#define ITRUSTEDCAMERADRIVER_OP_DYNAMICPROTECTSENSOR 0
#define ITRUSTEDCAMERADRIVER_OP_GETVERSION 1
#define ITRUSTEDCAMERADRIVER_OP_REGISTERNOTIFYCB 2
#define ITRUSTEDCAMERADRIVER_OP_DYNAMICCONFIGUREPORTS 3

static inline int32_t
trusted_camera_driver_release(struct smci_object self)
{
	return smci_object_invoke(self, SMCI_OBJECT_OP_RELEASE, 0, 0);
}

static inline int32_t
trusted_camera_driver_retain(struct smci_object self)
{
	return smci_object_invoke(self, SMCI_OBJECT_OP_RETAIN, 0, 0);
}

static inline int32_t
trusted_camera_driver_dynamic_protect_sensor(struct smci_object self,
				const struct tc_driver_sensor_info *phy_info_ptr)
{
	union smci_object_arg a[1] = {{{0, 0}}};

	a[0].bi = (struct smci_object_buf_in) { phy_info_ptr,
				sizeof(struct tc_driver_sensor_info) };

	return smci_object_invoke(self, ITRUSTEDCAMERADRIVER_OP_DYNAMICPROTECTSENSOR, a,
			SMCI_OBJECT_COUNTS_PACK(1, 0, 0, 0));
}

static inline int32_t
trusted_camera_driver_get_version(struct smci_object self,
	uint32_t *arch_ver_ptr, uint32_t *max_ver_ptr, uint32_t *min_ver_ptr)
{
	union smci_object_arg a[1] = {{{0, 0}}};
	int32_t result;
	struct {
		uint32_t m_arch_ver;
		uint32_t m_max_ver;
		uint32_t m_min_ver;
	} o;

	a[0].b = (struct smci_object_buf) { &o, 12 };

	result = smci_object_invoke(self, ITRUSTEDCAMERADRIVER_OP_GETVERSION, a,
			SMCI_OBJECT_COUNTS_PACK(0, 1, 0, 0));

	*arch_ver_ptr = o.m_arch_ver;
	*max_ver_ptr = o.m_max_ver;
	*min_ver_ptr = o.m_min_ver;

	return result;
}

static inline int32_t
trusted_camera_driver_register_notify_cb(struct smci_object self, struct smci_object cb_val)
{
	union smci_object_arg a[1] = {{{0, 0}}};

	a[0].o = cb_val;

	return smci_object_invoke(self, ITRUSTEDCAMERADRIVER_OP_REGISTERNOTIFYCB, a,
			SMCI_OBJECT_COUNTS_PACK(0, 0, 1, 0));
}

static inline int32_t
trusted_camera_driver_dynamic_configure_ports(struct smci_object self,
		const struct port_info *port_info_ptr, size_t port_info_len)
{
	union smci_object_arg a[1] = {{{0, 0}}};

	a[0].bi = (struct smci_object_buf_in) { port_info_ptr,
			port_info_len * sizeof(struct port_info) };

	return smci_object_invoke(self, ITRUSTEDCAMERADRIVER_OP_DYNAMICCONFIGUREPORTS, a,
		SMCI_OBJECT_COUNTS_PACK(1, 0, 0, 0));
}

#endif /* __TRUSTED_CAMERA_DRIVER_H */
