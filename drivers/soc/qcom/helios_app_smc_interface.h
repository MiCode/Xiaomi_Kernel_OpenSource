/* SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

enum heliosapp_op {
	LOAD_META_DATA,
	TRANSFER_AND_AUTHENTICATE_FW,
	COLLECT_RAMDUMP,
	FORCE_RESTART,
	SHUTDOWN,
	FORCE_POWER_DOWN
};

static inline int32_t
helios_app_release(struct Object self)
{
	return Object_invoke(self, Object_OP_release, 0, 0);
}

static inline int32_t
helios_app_retain(struct Object self)
{
	return Object_invoke(self, Object_OP_retain, 0, 0);
}

static inline int32_t
helios_app_load_meta_data(struct Object self, const void *metadata_ptr,
		size_t metadata_len)
{
	union ObjectArg arg[1] = {{{0, 0}}};

	arg[0].bi = (struct ObjectBufIn) { metadata_ptr, metadata_len * 1 };

	return Object_invoke(self, LOAD_META_DATA, arg,
			ObjectCounts_pack(1, 0, 0, 0));
}

static inline int32_t
helios_app_transfer_and_authenticate_fw(struct Object self,
		const void *firmware_ptr, size_t firmware_len)
{
	union ObjectArg arg[1] = {{{0, 0}}};

	arg[0].bi = (struct ObjectBufIn) { firmware_ptr, firmware_len * 1 };

	return Object_invoke(self, TRANSFER_AND_AUTHENTICATE_FW, arg,
			ObjectCounts_pack(1, 0, 0, 0));
}

static inline int32_t
helios_app_collect_ramdump(struct Object self, void *ramdump_ptr,
		size_t ramdump_len, size_t *ramdump_lenout)
{
	int32_t result;
	union ObjectArg arg[1] = {{{0, 0}}};

	arg[0].b = (struct ObjectBuf) { ramdump_ptr, ramdump_len * 1 };

	result = Object_invoke(self, COLLECT_RAMDUMP, arg,
			ObjectCounts_pack(0, 1, 0, 0));

	*ramdump_lenout = arg[0].b.size / 1;

	return result;
}

static inline int32_t
helios_app_force_restart(struct Object self)
{
	return Object_invoke(self, FORCE_RESTART, 0, 0);
}

static inline int32_t
helios_app_shutdown(struct Object self)
{
	return Object_invoke(self, SHUTDOWN, 0, 0);
}

static inline int32_t
helios_app_force_power_down(struct Object self)
{
	return Object_invoke(self, FORCE_POWER_DOWN, 0, 0);
}
