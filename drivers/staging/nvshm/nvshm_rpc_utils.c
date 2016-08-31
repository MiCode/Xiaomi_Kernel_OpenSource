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

#define pr_fmt(fmt) "%s:" fmt, __func__

#include <nvshm_rpc_utils.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/printk.h>
#include <linux/export.h>
#include <linux/sunrpc/xdr.h>

/*
 * Call fields (on top of header)
 * - RPC version            (=2)
 * - Program
 * - Program version
 * - Procedure
 * - Credentials (x2)       (=0,0)
 * - Verifier (x2)          (=0,0)
 */
#define SUN_RPC_CALL_HDR_SIZE 8

/*
 * Call fields (on top of header)
 * - Reply status (always accepted here)
 * - Verifier (x2)          (=0,0)
 * - Accept status
 * (Mismatch info to be allocated as opaque)
 */
#define SUN_RPC_ACC_REPLY_HDR_SIZE 4

/*
 * Check that there are enough bytes left in message payload, given length
 * needed, and current read pointer
 */
static inline bool is_too_short(const struct nvshm_rpc_message *message,
				const void *reader,
				u32 data_needed)
{
	u32 data_left = message->length - (reader - message->payload);
	if (data_left < data_needed) {
		/* We use 1 to check for emptiness */
		if (data_needed != 1)
			pr_err("Not enough data left in buffer: %d < %d\n",
			       data_left, data_needed);

		return true;
	}

	return false;
}

static int nvshm_rpc_utils_encode_args(const struct nvshm_rpc_datum_in *data,
				       u32 number,
				       u32 *writer)
{
	u32 n;

	for (n = 0; n < number; ++n) {
		const struct nvshm_rpc_datum_in *datum = &data[n];

		if ((datum->type & TYPE_ARRAY_FLAG) == 0) {
			switch (datum->type) {
			case TYPE_SINT:
				*writer++ = cpu_to_be32(datum->d.sint_data);
				break;
			case TYPE_UINT:
				*writer++ = cpu_to_be32(datum->d.uint_data);
				break;
			case TYPE_STRING:
				writer = xdr_encode_opaque(writer,
					datum->d.string_data,
					strlen(datum->d.string_data) + 1);
				break;
			case TYPE_BLOB:
				writer = xdr_encode_opaque(writer,
					datum->d.blob_data,
					datum->length);
				break;
			default:
				pr_err("unknown RPC type %d\n", datum->type);
				return -EINVAL;
			}
		} else {
			enum nvshm_rpc_datumtype type;

			type = datum->type & ~TYPE_ARRAY_FLAG;
			*writer++ = cpu_to_be32(datum->length);
			if ((type == TYPE_SINT) || (type == TYPE_UINT)) {
				const u32 *a;
				u32 d;

				a = (const u32 *) datum->d.blob_data;
				for (d = 0; d < datum->length; ++d, ++a)
					*writer++ = cpu_to_be32(*a);
			} else if (type == TYPE_STRING) {
				const char * const *a;
				u32 d;

				a = (const char * const *) datum->d.blob_data;
				for (d = 0; d < datum->length; ++d, ++a)
					writer = xdr_encode_opaque(writer, *a,
								strlen(*a) + 1);
			} else {
				pr_err("invalid RPC type for array %d\n", type);
				return -EINVAL;
			}
		}
	}
	return 0;
}

int nvshm_rpc_utils_encode_size(bool is_response,
				const struct nvshm_rpc_datum_in *data,
				u32 number)
{
	int quad_length;
	u32 n;

	if (is_response)
		quad_length = SUN_RPC_ACC_REPLY_HDR_SIZE;
	else
		quad_length = SUN_RPC_CALL_HDR_SIZE;
	for (n = 0; n < number; ++n) {
		const struct nvshm_rpc_datum_in *datum = &data[n];

		if ((datum->type & TYPE_ARRAY_FLAG) == 0) {
			switch (datum->type) {
			case TYPE_SINT:
			case TYPE_UINT:
				++quad_length;
				break;
			case TYPE_STRING:
				++quad_length;
				quad_length += XDR_QUADLEN(
					strlen(datum->d.string_data) + 1);
				break;
			case TYPE_BLOB:
				++quad_length;
				quad_length += XDR_QUADLEN(datum->length);
				break;
			default:
				pr_err("unknown RPC type %d\n", datum->type);
				return -EINVAL;
			}
		} else {
			enum nvshm_rpc_datumtype type;

			type = datum->type & ~TYPE_ARRAY_FLAG;
			++quad_length;
			if ((type == TYPE_SINT) || (type == TYPE_UINT)) {
				quad_length += datum->length;
			} else if (type == TYPE_STRING) {
				const char * const *a;
				u32 d;

				a = (const char * const *) datum->d.blob_data;
				for (d = 0; d < datum->length; ++d, ++a) {
					u32 len = strlen(*a) + 1;

					++quad_length;
					quad_length += XDR_QUADLEN(len);
				}
			} else {
				pr_err("invalid RPC type for array %d\n", type);
				return -EINVAL;
			}
		}
	}
	return quad_length << 2;
}
EXPORT_SYMBOL_GPL(nvshm_rpc_utils_encode_size);

int nvshm_rpc_utils_encode_request(const struct nvshm_rpc_procedure *procedure,
				   const struct nvshm_rpc_datum_in *data,
				   u32 number,
				   struct nvshm_rpc_message *message)
{
	u32 *writer = message->payload;

	/* RPC version */
	*writer++ = cpu_to_be32(2);
	/* Procedure */
	*writer++ = cpu_to_be32(procedure->program);
	*writer++ = cpu_to_be32(procedure->version);
	*writer++ = cpu_to_be32(procedure->procedure);
	/* Authentication (AUTH_NONE, size = 0) */
	*writer++ = cpu_to_be32(0);
	*writer++ = cpu_to_be32(0);
	/* Verifier (AUTH_NONE, size = 0) */
	*writer++ = cpu_to_be32(0);
	*writer++ = cpu_to_be32(0);
	return nvshm_rpc_utils_encode_args(data, number, writer);
}

int nvshm_rpc_utils_encode_response(enum rpc_accept_stat status,
				    const struct nvshm_rpc_datum_in *data,
				    u32 number,
				    struct nvshm_rpc_message *message)
{
	u32 *writer = message->payload;

	/* Reply status (always accepted) */
	*writer++ = cpu_to_be32(0);
	/* Verifier (AUTH_NONE, size = 0) */
	*writer++ = cpu_to_be32(0);
	*writer++ = cpu_to_be32(0);
	/* Accept status */
	*writer++ = cpu_to_be32(status);
	return nvshm_rpc_utils_encode_args(data, number, writer);
}
EXPORT_SYMBOL_GPL(nvshm_rpc_utils_encode_response);

int nvshm_rpc_utils_make_request(
	const struct nvshm_rpc_procedure *procedure,
	const struct nvshm_rpc_datum_in *data,
	u32 number,
	void (*callback)(struct nvshm_rpc_message *message, void *context),
	void *context)
{
	int rc;
	struct nvshm_rpc_message *request;
	int length;

	length = nvshm_rpc_utils_encode_size(false, data, number);
	if (length < 0)
		return length;

	request = nvshm_rpc_allocrequest(length, callback, context);
	if (!request)
		return -ENOMEM;

	rc = nvshm_rpc_utils_encode_request(procedure, data, number, request);
	if (rc < 0)
		goto error;

	rc = nvshm_rpc_send(request);
	if (rc < 0)
		goto error;

	return 0;
error:
	nvshm_rpc_free(request);
	return rc;
}
EXPORT_SYMBOL_GPL(nvshm_rpc_utils_make_request);

struct nvshm_rpc_message *nvshm_rpc_utils_prepare_response(
	const struct nvshm_rpc_message *request,
	enum rpc_accept_stat status,
	const struct nvshm_rpc_datum_in *data,
	u32 number)
{
	struct nvshm_rpc_message *response;
	int length;

	length = nvshm_rpc_utils_encode_size(true, data, number);
	if (length < 0)
		return NULL;

	response = nvshm_rpc_allocresponse(length, request);
	if (!response)
		return NULL;

	if (nvshm_rpc_utils_encode_response(status, data, number, response)) {
		nvshm_rpc_free(response);
		return NULL;
	}
	return response;
}
EXPORT_SYMBOL_GPL(nvshm_rpc_utils_prepare_response);

void nvshm_rpc_utils_decode_procedure(const struct nvshm_rpc_message *request,
				      struct nvshm_rpc_procedure *procedure)
{
	const u32 *reader = request->payload;

	/* Skip RPC version */
	reader += 1;
	procedure->program = be32_to_cpup((__be32 *) reader++);
	procedure->version = be32_to_cpup((__be32 *) reader++);
	procedure->procedure = be32_to_cpup((__be32 *) reader);
}

enum rpc_accept_stat
nvshm_rpc_utils_decode_status(const struct nvshm_rpc_message *response)
{
	const u32 *reader = response->payload;

	/* Skip reply status and verifier */
	reader += 3;
	return be32_to_cpup((__be32 *) reader);
}

int nvshm_rpc_utils_decode_versions(
	const struct nvshm_rpc_message *response,
	u32 *version_min,
	u32 *version_max)
{
	struct nvshm_rpc_datum_out versions[] = {
		NVSHM_RPC_OUT_UINT(version_min),
		NVSHM_RPC_OUT_UINT(version_max),
	};
	return nvshm_rpc_utils_decode_args(response, true, versions,
					   ARRAY_SIZE(versions));
}

int nvshm_rpc_utils_decode_args(const struct nvshm_rpc_message *message,
				bool is_response,
				struct nvshm_rpc_datum_out *data,
				u32 number)
{
	const __be32 *reader = message->payload;
	void *arrays[number];
	u32 n, arrays_index = 0;
	int rc = -EPROTO;

	if (is_response) {
		if (is_too_short(message, reader, SUN_RPC_ACC_REPLY_HDR_SIZE))
			return rc;

		reader += SUN_RPC_ACC_REPLY_HDR_SIZE;
	} else {
		if (is_too_short(message, reader, SUN_RPC_CALL_HDR_SIZE))
			return rc;

		reader += SUN_RPC_CALL_HDR_SIZE;
	}

	for (n = 0; n < number; ++n) {
		struct nvshm_rpc_datum_out *datum = &data[n];
		enum nvshm_rpc_datumtype type = datum->type & ~TYPE_ARRAY_FLAG;
		u32 uint;

		/* There is always a number, either the data or its length */
		if (is_too_short(message, reader, sizeof(uint)))
			goto err_mem_free;

		uint = be32_to_cpup((__be32 *) reader);
		reader++;
		if ((datum->type & TYPE_ARRAY_FLAG) == 0) {
			if (((type == TYPE_STRING) || (type == TYPE_BLOB)) &&
			    is_too_short(message, reader, sizeof(uint)))
				goto err_mem_free;

			switch (datum->type) {
			case TYPE_SINT:
				*datum->d.sint_data = (s32) uint;
				break;
			case TYPE_UINT:
				*datum->d.uint_data = uint;
				break;
			case TYPE_STRING:
				*datum->d.string_data = (const char *) reader;
				reader += XDR_QUADLEN(uint);
				break;
			case TYPE_BLOB:
				*datum->length = uint;
				*datum->d.blob_data = reader;
				reader += XDR_QUADLEN(uint);
				break;
			default:
				pr_err("unknown RPC type %d\n", datum->type);
				rc = -EINVAL;
				goto err_mem_free;
			}
		} else {
			*datum->length = uint;
			if ((type == TYPE_SINT) || (type == TYPE_UINT)) {
				u32 *a;
				u32 d;

				if (is_too_short(message, reader, uint * 4))
					break;

				a = kmalloc(uint * sizeof(u32), GFP_KERNEL);
				if (!a) {
					pr_err("kmalloc failed\n");
					rc = -ENOMEM;
					goto err_mem_free;
				}

				arrays[arrays_index++] = a;
				*datum->d.blob_data = a;
				for (d = 0; d < uint; ++d, ++a) {
					*a = be32_to_cpup((__be32 *) reader);
					reader++;
				}
			} else if (type == TYPE_STRING) {
				const char **a;
				u32 d;

				a = kmalloc(uint * sizeof(const char *),
					    GFP_KERNEL);
				if (!a) {
					pr_err("kmalloc failed\n");
					rc = -ENOMEM;
					goto err_mem_free;
				}

				arrays[arrays_index++] = a;
				*datum->d.blob_data = a;
				for (d = 0; d < uint; ++d, ++a) {
					u32 len;

					if (is_too_short(message, reader,
							 sizeof(len)))
						goto err_mem_free;

					len = be32_to_cpup((__be32 *) reader);
					reader++;
					if (is_too_short(message, reader,
							 XDR_QUADLEN(len)))
						goto err_mem_free;

					*a = (const char *) reader;
					reader += XDR_QUADLEN(len);
				}
			} else {
				pr_err("invalid RPC type for array %d\n", type);
				rc = -EINVAL;
				goto err_mem_free;
			}
		}
	}

	/* Check that things went well and there is no more data in buffer */
	if ((n == number) && is_too_short(message, reader, 1))
		return 0;

err_mem_free:
	/* Failure: need to free what's been allocated */
	for (n = 0; n < arrays_index; n++)
		kfree(arrays[n]);

	return rc;
}
EXPORT_SYMBOL_GPL(nvshm_rpc_utils_decode_args);

int nvshm_rpc_utils_decode_response(
	const struct nvshm_rpc_message *response,
	enum rpc_accept_stat *status,
	struct nvshm_rpc_datum_out *data,
	u32 number,
	u32 *version_min,
	u32 *version_max)
{
	int rc = 0;

	*status = nvshm_rpc_utils_decode_status(response);
	if (*status == RPC_SUCCESS)
		rc = nvshm_rpc_utils_decode_args(response, true, data, number);
	else if ((*status == RPC_PROG_MISMATCH) && version_min && version_max)
		rc = nvshm_rpc_utils_decode_versions(response, version_min,
						     version_max);
	return rc;
}
EXPORT_SYMBOL_GPL(nvshm_rpc_utils_decode_response);
