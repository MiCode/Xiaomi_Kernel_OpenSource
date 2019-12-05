/*
 * Copyright (C) 2017 Google, Inc.
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

#include <linux/device-mapper.h>
#include <linux/module.h>
#include <linux/pfk.h>

#define DM_MSG_PREFIX "default-key"
#define DEFAULT_DUN_OFFSET 1

struct default_key_c {
	struct dm_dev *dev;
	sector_t start;
	struct blk_encryption_key key;
	bool set_dun;
	u64 dun_offset;
};

static void default_key_dtr(struct dm_target *ti)
{
	struct default_key_c *dkc = ti->private;

	if (dkc->dev)
		dm_put_device(ti, dkc->dev);
	kzfree(dkc);
}

static int default_key_ctr_optional(struct dm_target *ti,
				    unsigned int argc, char **argv)
{
	struct default_key_c *dkc = ti->private;
	struct dm_arg_set as = {0};
	static const struct dm_arg _args[] = {
		{0, 2, "Invalid number of feature args"},
	};
	unsigned int opt_params;
	const char *opt_string;
	char dummy;
	int ret;

	as.argc = argc;
	as.argv = argv;

	ret = dm_read_arg_group(_args, &as, &opt_params, &ti->error);
	if (ret)
		return ret;

	while (opt_params--) {
		opt_string = dm_shift_arg(&as);
		if (!opt_string) {
			ti->error = "Not enough feature arguments";
			return -EINVAL;
		}

		if (!strcasecmp(opt_string, "set_dun")) {
			dkc->set_dun = true;
		} else if (sscanf(opt_string, "dun_offset:%llu%c",
				&dkc->dun_offset, &dummy) == 1) {
			if (dkc->dun_offset == 0) {
				ti->error = "dun_offset cannot be 0";
				return -EINVAL;
			}
		} else {
			ti->error = "Invalid feature arguments";
			return -EINVAL;
		}
	}

	if (dkc->dun_offset && !dkc->set_dun) {
		ti->error = "Invalid: dun_offset without set_dun";
		return -EINVAL;
	}

	if (dkc->set_dun && !dkc->dun_offset)
		dkc->dun_offset = DEFAULT_DUN_OFFSET;

	return 0;
}

/*
 * Construct a default-key mapping: <mode> <key> <dev_path> <start>
 */
static int default_key_ctr(struct dm_target *ti, unsigned int argc, char **argv)
{
	struct default_key_c *dkc;
	size_t key_size;
	unsigned long long tmp;
	char dummy;
	int err;

	if (argc < 4) {
		ti->error = "Too few arguments";
		return -EINVAL;
	}

	dkc = kzalloc(sizeof(*dkc), GFP_KERNEL);
	if (!dkc) {
		ti->error = "Out of memory";
		return -ENOMEM;
	}
	ti->private = dkc;

	if (strcmp(argv[0], "AES-256-XTS") != 0) {
		ti->error = "Unsupported encryption mode";
		err = -EINVAL;
		goto bad;
	}

	key_size = strlen(argv[1]);
	if (key_size != 2 * BLK_ENCRYPTION_KEY_SIZE_AES_256_XTS) {
		ti->error = "Unsupported key size";
		err = -EINVAL;
		goto bad;
	}
	key_size /= 2;

	if (hex2bin(dkc->key.raw, argv[1], key_size) != 0) {
		ti->error = "Malformed key string";
		err = -EINVAL;
		goto bad;
	}

	err = dm_get_device(ti, argv[2], dm_table_get_mode(ti->table),
			    &dkc->dev);
	if (err) {
		ti->error = "Device lookup failed";
		goto bad;
	}

	if (sscanf(argv[3], "%llu%c", &tmp, &dummy) != 1) {
		ti->error = "Invalid start sector";
		err = -EINVAL;
		goto bad;
	}
	dkc->start = tmp;

	if (argc > 4) {
		err = default_key_ctr_optional(ti, argc - 4, &argv[4]);
		if (err)
			goto bad;
	}

	if (!blk_queue_inlinecrypt(bdev_get_queue(dkc->dev->bdev))) {
		ti->error = "Device does not support inline encryption";
		err = -EINVAL;
		goto bad;
	}

	/* Pass flush requests through to the underlying device. */
	ti->num_flush_bios = 1;

	/*
	 * We pass discard requests through to the underlying device, although
	 * the discarded blocks will be zeroed, which leaks information about
	 * unused blocks.  It's also impossible for dm-default-key to know not
	 * to decrypt discarded blocks, so they will not be read back as zeroes
	 * and we must set discard_zeroes_data_unsupported.
	 */
	ti->num_discard_bios = 1;

	/*
	 * It's unclear whether WRITE_SAME would work with inline encryption; it
	 * would depend on whether the hardware duplicates the data before or
	 * after encryption.  But since the internal storage in some  devices
	 * (MSM8998-based) doesn't claim to support WRITE_SAME anyway, we don't
	 * currently have a way to test it.  Leave it disabled it for now.
	 */
	/*ti->num_write_same_bios = 1;*/

	return 0;

bad:
	default_key_dtr(ti);
	return err;
}

static int default_key_map(struct dm_target *ti, struct bio *bio)
{
	const struct default_key_c *dkc = ti->private;

	bio_set_dev(bio, dkc->dev->bdev);
	if (bio_sectors(bio)) {
		bio->bi_iter.bi_sector = dkc->start +
			dm_target_offset(ti, bio->bi_iter.bi_sector);
	}

	if (!bio->bi_crypt_key && !bio->bi_crypt_skip) {
		bio->bi_crypt_key = &dkc->key;

		if (dkc->set_dun)
			bio_dun(bio) = (dm_target_offset(ti,
							 bio->bi_iter.bi_sector)
					>> 3) + dkc->dun_offset;
	}

	return DM_MAPIO_REMAPPED;
}

static void default_key_status(struct dm_target *ti, status_type_t type,
			       unsigned int status_flags, char *result,
			       unsigned int maxlen)
{
	const struct default_key_c *dkc = ti->private;
	unsigned int sz = 0;
	int num_feature_args = 0;

	switch (type) {
	case STATUSTYPE_INFO:
		result[0] = '\0';
		break;

	case STATUSTYPE_TABLE:

		/* encryption mode */
		DMEMIT("AES-256-XTS");

		/* reserved for key; dm-crypt shows it, but we don't for now */
		DMEMIT(" -");

		/* name of underlying device, and the start sector in it */
		DMEMIT(" %s %llu", dkc->dev->name,
		       (unsigned long long)dkc->start);

		num_feature_args += dkc->set_dun;
		num_feature_args += dkc->set_dun
			&& dkc->dun_offset != DEFAULT_DUN_OFFSET;

		if (num_feature_args) {
			DMEMIT(" %d", num_feature_args);
			if (dkc->set_dun)
				DMEMIT(" set_dun");
			if (dkc->set_dun
			    && dkc->dun_offset != DEFAULT_DUN_OFFSET)
				DMEMIT(" dun_offset:%llu", dkc->dun_offset);
		}

		break;
	}
}

static int default_key_prepare_ioctl(struct dm_target *ti,
				     struct block_device **bdev, fmode_t *mode)
{
	struct default_key_c *dkc = ti->private;
	struct dm_dev *dev = dkc->dev;

	*bdev = dev->bdev;

	/*
	 * Only pass ioctls through if the device sizes match exactly.
	 */
	if (dkc->start ||
	    ti->len != i_size_read(dev->bdev->bd_inode) >> SECTOR_SHIFT)
		return 1;
	return 0;
}

static int default_key_iterate_devices(struct dm_target *ti,
				       iterate_devices_callout_fn fn,
				       void *data)
{
	struct default_key_c *dkc = ti->private;

	return fn(ti, dkc->dev, dkc->start, ti->len, data);
}

static struct target_type default_key_target = {
	.name   = "default-key",
	.version = {1, 1, 0},
	.module = THIS_MODULE,
	.ctr    = default_key_ctr,
	.dtr    = default_key_dtr,
	.map    = default_key_map,
	.status = default_key_status,
	.prepare_ioctl = default_key_prepare_ioctl,
	.iterate_devices = default_key_iterate_devices,
};

static int __init dm_default_key_init(void)
{
	return dm_register_target(&default_key_target);
}

static void __exit dm_default_key_exit(void)
{
	dm_unregister_target(&default_key_target);
}

module_init(dm_default_key_init);
module_exit(dm_default_key_exit);

MODULE_AUTHOR("Paul Lawrence <paullawrence@google.com>");
MODULE_AUTHOR("Paul Crowley <paulcrowley@google.com>");
MODULE_AUTHOR("Eric Biggers <ebiggers@google.com>");
MODULE_DESCRIPTION(DM_NAME " target for encrypting filesystem metadata");
MODULE_LICENSE("GPL v2");
