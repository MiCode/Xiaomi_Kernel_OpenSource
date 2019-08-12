/* Copyright (c) 2019, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/device-mapper.h>
#include <linux/fs.h>
#include <linux/string.h>
#include "uapi/linux/dm-ioctl.h"
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include "do_mounts.h"

#define DM_BUF_SIZE 4096

#define DM_MSG_PREFIX "verity"

static void __init init_param(struct dm_ioctl *param, const char *name)
{
	memset(param, 0, DM_BUF_SIZE);
	param->data_size = DM_BUF_SIZE;
	param->data_start = sizeof(struct dm_ioctl);
	param->version[0] = 4;
	param->version[1] = 0;
	param->version[2] = 0;
	param->flags = DM_READONLY_FLAG;
	strlcpy(param->name, name, sizeof(param->name));
}

static void __init dm_setup_drive(void)
{
	struct device_node *dt_node;
	const char *name;
	const char *version;
	const char *data_device;
	const char *hash_device;
	const char *data_block_size;
	const char *hash_block_size;
	const char *number_of_data_blocks;
	const char *hash_start_block;
	const char *algorithm;
	const char *digest;
	const char *salt;
	const char *opt;
	int len;
	unsigned long long data_blocks;
	char dummy;
	char *verity_params;
	size_t bufsize;
	char *buffer = kzalloc(DM_BUF_SIZE, GFP_KERNEL);
	struct dm_ioctl *param = (struct dm_ioctl *) buffer;
	size_t dm_sz = sizeof(struct dm_ioctl);
	struct dm_target_spec *tgt = (struct dm_target_spec *) &buffer[dm_sz];

	if (!buffer)
		goto fail;
	dt_node = of_find_node_by_path("/soc/dm_verity");
	if (!dt_node) {
		DMERR("(E) Failed to find device-tree node: /soc/dm_verity");
		goto fail;
	}

	name = of_get_property(dt_node, "dmname", &len);
	if (name == NULL)
		goto fail;
	DMDEBUG("(I) name=%s", name);

	if (strcmp(name, "disabled") == 0) {
		pr_info("dm: dm-verity is disabled.");
		kfree(buffer);
		return;
	}

	version = of_get_property(dt_node, "version", &len);
	if (version == NULL)
		goto fail;
	DMDEBUG("(I) version=%s", version);

	data_device = of_get_property(dt_node, "data_device", &len);
	if (data_device == NULL)
		goto fail;
	DMDEBUG("(I) data_device=%s", data_device);

	hash_device = of_get_property(dt_node, "hash_device", &len);
	if (hash_device == NULL)
		goto fail;
	DMDEBUG("(I) hash_device=%s", hash_device);

	data_block_size = of_get_property(dt_node, "data_block_size", &len);
	if (data_block_size == NULL)
		goto fail;
	DMDEBUG("(I) data_block_size=%s", data_block_size);

	hash_block_size = of_get_property(dt_node, "hash_block_size", &len);
	if (hash_block_size == NULL)
		goto fail;
	DMDEBUG("(I) hash_block_size=%s", hash_block_size);

	number_of_data_blocks = of_get_property(dt_node,
						"number_of_data_blocks",
						&len);
	if (number_of_data_blocks == NULL)
		goto fail;
	DMDEBUG("(I) number_of_data_blocks=%s", number_of_data_blocks);

	hash_start_block = of_get_property(dt_node, "hash_start_block", &len);
	if (hash_start_block == NULL)
		goto fail;
	DMDEBUG("(I) hash_start_block=%s", hash_start_block);

	algorithm = of_get_property(dt_node, "algorithm", &len);
	if (algorithm == NULL)
		goto fail;
	DMDEBUG("(I) algorithm=%s", algorithm);

	digest = of_get_property(dt_node, "digest", &len);
	if (digest == NULL)
		goto fail;
	DMDEBUG("(I) digest=%s", digest);

	salt = of_get_property(dt_node, "salt", &len);
	if (salt == NULL)
		goto fail;
	DMDEBUG("(I) salt=%s", salt);

	opt = of_get_property(dt_node, "opt", &len);
	if (opt == NULL)
		goto fail;
	DMDEBUG("(I) opt=%s", opt);

	init_param(param, name);
	if (dm_ioctrl(DM_DEV_CREATE_CMD, param)) {
		DMERR("(E) failed to create the device");
		goto fail;
	}

	init_param(param, name);
	param->target_count = 1;
	/* set tgt arguments */
	tgt->status = 0;
	tgt->sector_start = 0;
	if (sscanf(number_of_data_blocks, "%llu%c", &data_blocks, &dummy) != 1)
		goto fail;

	tgt->length = data_blocks*4096/512; /* size in sector of data dev */
	strlcpy(tgt->target_type, "verity", sizeof(tgt->target_type));
	/* build the verity params here */
	verity_params = buffer + dm_sz + sizeof(struct dm_target_spec);
	bufsize = DM_BUF_SIZE - (verity_params - buffer);

	verity_params += snprintf(verity_params, bufsize,
				  "%s %s %s %s %s %s %s %s %s %s 1 %s",
				  version,
				  data_device, hash_device,
				  data_block_size, hash_block_size,
				  number_of_data_blocks, hash_start_block,
				  algorithm, digest, salt, opt);

	tgt->next = verity_params - buffer;
	if (dm_ioctrl(DM_TABLE_LOAD_CMD, param)) {
		DMERR("(E) failed to load the device");
		goto fail;
	}

	init_param(param, name);
	if (dm_ioctrl(DM_DEV_SUSPEND_CMD, param)) {
		DMERR("(E) failed to suspend the device");
		goto fail;
	}

	pr_info("dm: dm-0 (%s) is ready", data_device);
	kfree(buffer);
	return;

fail:
	pr_info("dm: starting dm-0 failed");
	kfree(buffer);
	return;

}

void __init dm_verity_setup(void)
{
	pr_info("dm: attempting early device configuration.");
	dm_setup_drive();
}
