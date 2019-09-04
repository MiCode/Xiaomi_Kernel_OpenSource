/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __HIE_H_
#define __HIE_H_

#include <linux/fs.h>
#include <linux/blk_types.h>
#include <linux/blkdev.h>
#include <keys/user-type.h>

#define HIE_MAX_KEY_SIZE 64

#define HIE_DBG_FS  0x02
#define HIE_DBG_BIO 0x04
#define HIE_DBG_KEY 0x08
#define HIE_DBG_HIE 0x10
#define HIE_DBG_DRV 0x20
#define HIE_DBG_CRY 0x40

struct hie_fs {
	const char *name;
	int (*key_payload)(struct bio_crypt_ctx *ctx,
		const unsigned char **key);
	int (*set_bio_context)(struct inode *inode,
		struct bio *bio);
	void *priv; /* fs specific data */

	struct list_head list;
};

struct hie_dev {
	const char *name;
	unsigned int mode; /* encryption modes supported by the device */
	int (*encrypt)(unsigned int mode, const char *key, int len,
		struct request *req, void *priv);
	int (*decrypt)(unsigned int mode, const char *key, int len,
		struct request *req, void *priv);
	void *priv; /* device specific data */

	struct list_head list;
};

typedef int (*hie_act)(unsigned int, const char *, int,
	struct request *, void *);

static inline bool hie_request_crypted(struct request *req)
{
	return (req && req->bio) ?
		(req->bio->bi_crypt_ctx.bc_flags & BC_CRYPT) : 0;
}

#ifdef CONFIG_HIE
bool hie_is_capable(const struct super_block *sb);
int hie_is_dummy(void);
int hie_is_nocrypt(void);
int hie_register_fs(struct hie_fs *fs);
int hie_register_device(struct hie_dev *dev);
int hie_decrypt(struct hie_dev *dev, struct request *req, void *priv);
int hie_encrypt(struct hie_dev *dev, struct request *req, void *priv);
bool hie_key_verify(struct bio *bio1, struct bio *bio2);
int hie_set_bio_crypt_context(struct inode *inode, struct bio *bio);
int hie_set_dio_crypt_context(struct inode *inode, struct bio *bio,
	loff_t fs_offset);
u64 hie_get_iv(struct request *req);

int hie_debug(unsigned int mask);
int hie_debug_ino(unsigned long ino);
int hie_req_end_size(struct request *req, unsigned long bytes);
int hie_dump_req(struct request *req, const char *prefix);
#else
static inline
bool hie_is_capable(const struct super_block *sb)
{
	return false;
}

static inline
int hie_is_dummy(void)
{
	return 0;
}

static inline
int hie_is_nocrypt(void)
{
	return 0;
}

static inline
bool hie_key_verify(struct bio *bio1, struct bio *bio2)
{
	return true;
}

static inline
int hie_register_fs(struct hie_fs *fs)
{
	return 0;
}

static inline
int hie_register_device(struct hie_dev *dev)
{
	return 0;
}

static inline
int hie_decrypt(struct hie_dev *dev, struct request *req, void *priv)
{
	return 0;
}

static inline
int hie_encrypt(struct hie_dev *dev, struct request *req, void *priv)
{
	return 0;
}

static inline
int hie_set_bio_crypt_context(struct inode *inode, struct bio *bio)
{
	return 0;
}

static inline
int hie_set_dio_crypt_context(struct inode *inode, struct bio *bio,
	loff_t fs_offset)
{
	return 0;
}

static inline
u64 hie_get_iv(struct request *req)
{
	return 0;
}

static inline
int hie_debug(unsigned int mask)
{
	return 0;
}

static inline
int hie_debug_ino(unsigned long ino)
{
	return 0;
}

static inline
int hie_req_end_size(struct request *req, unsigned long bytes)
{
	return 0;
}

static inline
void hie_dump_bio_file(struct bio *bio, const char *prefix,
	const char *filename)
{
}

static inline
int hie_dump_req(struct request *req, const char *prefix)
{
	return 0;
}

#endif

static inline
int hie_req_end(struct request *req)
{
	return hie_req_end_size(req, 0);
}

#endif

