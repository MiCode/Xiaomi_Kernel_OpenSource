/*
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define pr_fmt(fmt) "%s: " fmt, __func__

#include <linux/device.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <mach/tegra_bb.h>
#include <mach/tegra_bbc_proxy.h>
#include <nvshm_rpc_utils.h>
#include <nvshm_rpc_dispatcher.h>

/*
 * RSM APIs:
 * int bbc_edp_request(u32 mode, u32 state, u32 threshold)
 * int bbc_edp_register(u32 num_states, u32 states[])
 * int bbc_bw_register(u32 bw);
 * int bbc_bw_request(u32 mode, u32 bw, u32 lt)
 */

static struct device *proxy_dev, *tegra_bb;

static enum rpc_accept_stat rpc_bbc_edp_request(
	u32 version,
	struct nvshm_rpc_message *req,
	struct nvshm_rpc_message **resp)
{
	u32 mode;
	u32 state;
	u32 threshold;
	struct nvshm_rpc_datum_out req_data[] = {
		NVSHM_RPC_OUT_UINT(&mode),
		NVSHM_RPC_OUT_UINT(&state),
		NVSHM_RPC_OUT_UINT(&threshold),
	};
	int rc;

	/* Decode request */
	if (nvshm_rpc_utils_decode_args(req, false, req_data,
					ARRAY_SIZE(req_data)) < 0)
		return RPC_GARBAGE_ARGS;

	/* Call */
	rc = tegra_bbc_proxy_edp_request(proxy_dev, mode, state, threshold);

	/* Encode response */
	{
		struct nvshm_rpc_datum_in resp_data[] = {
			NVSHM_RPC_IN_SINT(rc),
		};

		*resp = nvshm_rpc_utils_prepare_response(req, RPC_SUCCESS,
				resp_data, ARRAY_SIZE(resp_data));
	}
	return *resp ? RPC_SUCCESS : RPC_SYSTEM_ERR;
}

static enum rpc_accept_stat rpc_bbc_edp_register(
	u32 version,
	struct nvshm_rpc_message *req,
	struct nvshm_rpc_message **resp)
{
	u32 num_states;
	u32 *states;
	struct nvshm_rpc_datum_out req_data[] = {
		NVSHM_RPC_OUT_ARRAY(TYPE_UINT, &num_states, &states),
	};
	int rc;

	/* Decode request */
	if (nvshm_rpc_utils_decode_args(req, false, req_data,
					ARRAY_SIZE(req_data)) < 0)
		return RPC_GARBAGE_ARGS;

	/* Call */
	rc = tegra_bbc_proxy_edp_register(proxy_dev, num_states, states);
	kfree(states);

	/* Encode response */
	{
		struct nvshm_rpc_datum_in resp_data[] = {
			NVSHM_RPC_IN_SINT(rc),
		};

		*resp = nvshm_rpc_utils_prepare_response(req, RPC_SUCCESS,
				resp_data, ARRAY_SIZE(resp_data));
	}
	return *resp ? RPC_SUCCESS : RPC_SYSTEM_ERR;
}

static enum rpc_accept_stat rpc_bbc_bw_register(
	u32 version,
	struct nvshm_rpc_message *req,
	struct nvshm_rpc_message **resp)
{
	u32 bw;
	struct nvshm_rpc_datum_out req_data[] = {
		NVSHM_RPC_OUT_UINT(&bw),
	};
	int rc;

	/* Decode request */
	if (nvshm_rpc_utils_decode_args(req, false, req_data,
					ARRAY_SIZE(req_data)) < 0)
		return RPC_GARBAGE_ARGS;

	/* Call */
	rc = tegra_bbc_proxy_bw_register(proxy_dev, bw);

	/* Encode response */
	{
		struct nvshm_rpc_datum_in resp_data[] = {
			NVSHM_RPC_IN_SINT(rc),
		};

		*resp = nvshm_rpc_utils_prepare_response(req, RPC_SUCCESS,
				resp_data, ARRAY_SIZE(resp_data));
	}
	return *resp ? RPC_SUCCESS : RPC_SYSTEM_ERR;
}

static enum rpc_accept_stat rpc_bbc_bw_request(
	u32 version,
	struct nvshm_rpc_message *req,
	struct nvshm_rpc_message **resp)
{
	u32 mode;
	u32 bw;
	u32 lt;
	u32 margin;
	u32 freq_floor;
	u32 flags;
	struct nvshm_rpc_datum_out req_data[] = {
		NVSHM_RPC_OUT_UINT(&mode),
		NVSHM_RPC_OUT_UINT(&bw),
		NVSHM_RPC_OUT_UINT(&lt),
		NVSHM_RPC_OUT_UINT(&margin),
		NVSHM_RPC_OUT_UINT(&freq_floor),
		NVSHM_RPC_OUT_UINT(&flags),
	};
	int rc;

	/* Decode request */
	if (nvshm_rpc_utils_decode_args(req, false, req_data,
					ARRAY_SIZE(req_data)) < 0)
		return RPC_GARBAGE_ARGS;

	/* Call */
	rc = tegra_bbc_proxy_bw_request(proxy_dev, mode, bw, lt, margin);
	tegra_bb_set_emc_floor(tegra_bb, freq_floor, flags);

	/* Encode response */
	{
		struct nvshm_rpc_datum_in resp_data[] = {
			NVSHM_RPC_IN_SINT(rc),
		};

		*resp = nvshm_rpc_utils_prepare_response(req, RPC_SUCCESS,
				resp_data, ARRAY_SIZE(resp_data));
	}
	return *resp ? RPC_SUCCESS : RPC_SYSTEM_ERR;
}

static nvshm_rpc_function_t procedures[] = {
	rpc_bbc_edp_register,
	rpc_bbc_edp_request,
	rpc_bbc_bw_register,
	rpc_bbc_bw_request,
};

static struct nvshm_rpc_program program = {
	.version_min = 0,
	.version_max = 0,
	.procedures_size = ARRAY_SIZE(procedures),
	.procedures = procedures,
};

static int __init prog_rsm_init(void)
{
	proxy_dev = bus_find_device_by_name(&platform_bus_type, NULL,
					"tegra_bbc_proxy");
	if (!proxy_dev) {
		pr_err("failed to get proxy device pointer\n");
		return -ENXIO;
	}

	tegra_bb = bus_find_device_by_name(&platform_bus_type, NULL,
					"tegra_bb.0");
	if (!tegra_bb) {
		pr_err("failed to get tegra_bb device pointer\n");
		return -ENXIO;
	}

	return nvshm_rpc_program_register(NVSHM_RPC_PROGRAM_RSM, &program);
}

static void __exit prog_rsm_exit(void)
{
	nvshm_rpc_program_unregister(NVSHM_RPC_PROGRAM_RSM);
}

module_init(prog_rsm_init);
module_exit(prog_rsm_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Hervé Fache <hfache@nvidia.com>");
MODULE_DESCRIPTION("NVSHM RPC RSM program");
