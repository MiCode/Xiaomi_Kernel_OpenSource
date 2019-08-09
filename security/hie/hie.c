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

#define DEBUG 1

#include <linux/bio.h>
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/key.h>
#include <linux/key-type.h>
#include <keys/user-type.h>
#include <linux/debugfs.h>
#include <linux/hie.h>
#include <linux/blk_types.h>
#include <linux/preempt.h>
#include <linux/fs.h>

#ifdef CONFIG_MTK_PLATFORM
#include <mt-plat/aee.h>
#else
#define aee_kernel_warning(...)
#endif

static DEFINE_SPINLOCK(hie_dev_list_lock);
static LIST_HEAD(hie_dev_list);
static DEFINE_SPINLOCK(hie_fs_list_lock);
static LIST_HEAD(hie_fs_list);

static struct hie_dev *hie_default_dev;
static struct hie_fs *hie_default_fs;

#ifdef CONFIG_HIE_DEBUG
struct dentry *hie_droot;
struct dentry *hie_ddebug;

u32 hie_dbg;
u64 hie_dbg_ino;
u64 hie_dbg_sector;
#endif

int hie_debug(unsigned int mask)
{
#ifdef CONFIG_HIE_DEBUG
	return (hie_dbg & mask);
#else
	return 0;
#endif
}

int hie_debug_ino(unsigned long ino)
{
#ifdef CONFIG_HIE_DEBUG
	return ((hie_dbg & HIE_DBG_FS) && (hie_dbg_ino == ino));
#else
	return 0;
#endif
}

int hie_is_ready(void)
{
	return (!IS_ERR_OR_NULL(hie_default_dev));
}
EXPORT_SYMBOL_GPL(hie_is_ready);

int hie_is_dummy(void)
{
#ifdef CONFIG_HIE_DUMMY_CRYPT
	return 1;
#else
	return 0;
#endif
}
EXPORT_SYMBOL_GPL(hie_is_dummy);

int hie_is_nocrypt(void)
{
#ifdef CONFIG_HIE_NO_CRYPT
	return 1;
#else
	return 0;
#endif
}
EXPORT_SYMBOL_GPL(hie_is_nocrypt);

int hie_register_device(struct hie_dev *dev)
{
	unsigned long flags;

	spin_lock_irqsave(&hie_dev_list_lock, flags);
	list_add(&dev->list, &hie_dev_list);
	spin_unlock_irqrestore(&hie_dev_list_lock, flags);

	if (IS_ERR_OR_NULL(hie_default_dev))
		hie_default_dev = dev;

	return 0;
}
EXPORT_SYMBOL_GPL(hie_register_device);

int hie_register_fs(struct hie_fs *fs)
{
	unsigned long flags;

	if (IS_ERR_OR_NULL(fs))
		return -EINVAL;

	spin_lock_irqsave(&hie_fs_list_lock, flags);
	list_add(&fs->list, &hie_fs_list);
	spin_unlock_irqrestore(&hie_fs_list_lock, flags);

	if (IS_ERR_OR_NULL(hie_default_fs))
		hie_default_fs = fs;

	return 0;
}
EXPORT_SYMBOL_GPL(hie_register_fs);

#ifdef CONFIG_HIE_DEBUG
#define __rw_str(bio) ((bio_data_dir(bio) == READ) ? "R" : "W")

static const char *get_page_name_nolock(struct page *p, char *buf, int len,
	unsigned long *ino)
{
	struct inode *inode;
	struct address_space *mapping = page_mapping(p);
	struct dentry *dentry = NULL;
	struct dentry *alias;
	char *ptr = buf;

	if (!mapping)
		return "?";

	inode = mapping->host;

	hlist_for_each_entry(alias, &inode->i_dentry, d_u.d_alias) {
		dentry = alias;
		if (dentry)
			break;
	}

	if (dentry && dentry->d_name.name)
		strncpy(buf, dentry->d_name.name, len);
	else
		return "#";

	if (ino)
		*ino = inode->i_ino;

	return ptr;
}

static const char *get_page_name(struct page *p, char *buf, int len,
	unsigned long *ino)
{
	struct inode *inode;
	struct dentry *dentry;
	struct address_space *mapping;
	char *ptr = buf;

	if (!p || !buf || len <= 0)
		return "#";

	mapping = page_mapping(p);

	if (!mapping || !mapping->host)
		return "?";

	if (in_interrupt() || irqs_disabled() || preempt_count())
		return get_page_name_nolock(p, buf, len, ino);

	inode = p->mapping->host;

	dentry = d_find_alias(inode);

	if (dentry) {
		ptr = dentry_path_raw(dentry, buf, len);
		dput(dentry);
	} else {
		return "?";
	}

	if (ino)
		*ino = inode->i_ino;

	return ptr;
}

static void hie_dump_bio(struct bio *bio, const char *prefix)
{
	struct bio_vec bvec;
	struct bvec_iter iter;
	struct page *last_page = NULL;
	unsigned int size = 0;
	const char *ptr = NULL;
	unsigned long ino = 0;
	unsigned long iv;
	char path[256];

	bio_for_each_segment(bvec, bio, iter) {
		size += bvec.bv_len;
		if (bvec.bv_page)
			if (last_page != bvec.bv_page && !ptr) {
				last_page = bvec.bv_page;
				ptr = get_page_name(bvec.bv_page, path, 255,
					&ino);
			}
	}

	iv = bio_bc_iv_get(bio);

	pr_info("HIE: %s: bio: %p %s, flag: %x, size: %d, file: %s, ino: %ld, iv: %lu\n",
		prefix, bio, __rw_str(bio),
		bio->bi_crypt_ctx.bc_flags,
		size, ptr?ptr:"", ino, iv);
}

int hie_dump_req(struct request *req, const char *prefix)
{
	struct bio *bio;

	__rq_for_each_bio(bio, req)
		hie_dump_bio(bio, prefix);

	return 0;
}
#else
int hie_dump_req(struct request *req, const char *prefix)
{
	return 0;
}
#endif

#if defined(CONFIG_HIE_DUMMY_CRYPT) && !defined(CONFIG_HIE_NO_CRYPT)
static void hie_xor(void *buf, unsigned int length, u32 key)
{
	unsigned int i;

	u32 *p = (u32 *)buf;

	for (i = 0; i < length; i += 4, p++)
		*p = *p ^ key;
}

static void hie_dummy_crypt_set_key(struct request *req, u32 key)
{
	if (req->bio) {
		struct bio *bio;

		__rq_for_each_bio(bio, req) {
			bio->bi_crypt_ctx.dummy_crypt_key = key;
		}
	}
}

static unsigned long hie_dummy_crypt_bio(const char *prefix, struct bio *bio,
	unsigned long max, unsigned int blksize, u64 *iv)
{
	unsigned long flags;
	struct bio_vec bv;
	struct bvec_iter iter;
	unsigned long ret = 0;
	unsigned int len;
#ifdef CONFIG_HIE_DEBUG
	const char *ptr = NULL;
	unsigned long ino = 0;
	char path[256];
#endif

	if (!bio)
		return 0;

	bio_for_each_segment(bv, bio, iter) {
		u32 key;
		char *data = bvec_kmap_irq(&bv, &flags);
		unsigned int i;
		unsigned int remain;

		if (max && (ret + bv.bv_len > max))
			len = max - ret;
		else
			len = bv.bv_len;

#ifdef CONFIG_HIE_DEBUG
		if (!ptr)
			ptr = get_page_name(bv.bv_page, path, 255, &ino);

		if (hie_debug(HIE_DBG_CRY)) {
			pr_info("HIE: %s: %s bio: %p, base: %p %s len: %d, file: %s, ino: %ld, sec: %lu, iv: %llx, pgidx: %u\n",
			  __func__, prefix, bio, data,
			  __rw_str(bio), bv.bv_len,
			  ptr, ino, (unsigned long)iter.bi_sector, *iv,
			  (unsigned int)bv.bv_page->index);

			print_hex_dump(KERN_DEBUG, "before crypt: ",
				DUMP_PREFIX_OFFSET, 32, 1, data, 32, 0);
		}
#endif
		remain = len;

		for (i = 0; i < len; i += blksize)	{
			key = bio->bi_crypt_ctx.dummy_crypt_key;

			if (iv && *iv) {
#ifdef CONFIG_HIE_DUMMY_CRYPT_IV
				key = (key & 0xFFFF0000) |
				      (((u32)*iv) & 0xFFFF);
#endif
				(*iv)++;
			}
			hie_xor(data+i,
			    (remain > blksize) ? blksize : remain, key);
			remain -= blksize;
		}

		ret += len;

#ifdef CONFIG_HIE_DEBUG
		if (hie_debug(HIE_DBG_CRY))
			print_hex_dump(KERN_DEBUG, "after crypt: ",
				DUMP_PREFIX_OFFSET, 32, 1, data, 32, 0);
#endif
		flush_dcache_page(bv.bv_page);
		bvec_kunmap_irq(data, &flags);
	}
	return ret;
}

static int hie_dummy_crypt_req(const char *prefix, struct request *req,
	unsigned long bytes)
{
	u64 iv;
	struct bio *bio;
	unsigned int blksize;

	if (!req->bio)
		return 0;

	iv = hie_get_iv(req);
	blksize = queue_physical_block_size(req->q);

	if (hie_debug(HIE_DBG_CRY)) {
		pr_info("HIE: %s: %s req: %p, req_iv: %llx\n",
		  __func__, prefix, req, iv);
	}

	__rq_for_each_bio(bio, req) {
		unsigned long cnt;

#ifdef CONFIG_HIE_DEBUG
		if (hie_debug(HIE_DBG_CRY)) {
			u64 bio_iv;

			bio_iv = bio_bc_iv_get(bio);
			pr_info("HIE: %s: %s req: %p, req_iv: %llx, bio: %p, %s, bio_iv: %llu\n",
			  __func__, prefix, req, iv, bio,
			  __rw_str(bio),
			  bio_iv);
		}
#endif
		cnt = hie_dummy_crypt_bio(prefix, bio, bytes, blksize, &iv);

		if (bytes)	{
			if (bytes > cnt)
				bytes -= cnt;
			else
				break;
		}
	}

	return 0;
}
#endif

int hie_req_end_size(struct request *req, unsigned long bytes)
{
#if defined(CONFIG_HIE_DUMMY_CRYPT) && !defined(CONFIG_HIE_NO_CRYPT)
	struct bio *bio = req->bio;

	if (!hie_request_crypted(req))
		return 0;

	return hie_dummy_crypt_req("<end>", req,
		(bio_data_dir(bio) == WRITE) ? 0 : bytes);
#else
	return 0;
#endif
}

/**
 * Verify the correctness of crypto_not_mergeable() @ block/blk-merge.c
 * The bios of different keys should not be merge in the same request.
 */
static int hie_req_verify(struct request *req, struct hie_dev *dev,
	unsigned int *crypt_mode)
{
	struct bio *bio;
	struct key *keyring_key;
	unsigned int key_size;
	unsigned int mode;
	unsigned int last_mode;
	unsigned int flag;
	unsigned long iv = BC_INVALD_IV;
	unsigned long count = 0;

	if (!req->bio)
		return -ENOENT;

	bio = req->bio;
	keyring_key = bio->bi_crypt_ctx.bc_keyring_key;
	key_size = bio->bi_crypt_ctx.bc_key_size;
	mode = last_mode = bio->bi_crypt_ctx.bc_flags & dev->mode;
	flag = bio->bi_crypt_ctx.bc_flags;

	if (bio_bcf_test(bio, BC_IV_PAGE_IDX))
		iv = bio_bc_iv_get(bio);

	__rq_for_each_bio(bio, req) {
		if ((!bio_encrypted(bio)) ||
			(keyring_key != bio->bi_crypt_ctx.bc_keyring_key) ||
			(key_size != bio->bi_crypt_ctx.bc_key_size)) {
			pr_info("%s: inconsistent context. bio: %p, key_size: %d, key: %p, req: %p.\n",
				__func__, bio, key_size, keyring_key, req);
			return -EINVAL;
		}
		mode = bio->bi_crypt_ctx.bc_flags & dev->mode;
		if (!mode) {
			pr_info("%s: %s: unsupported crypt mode %x\n",
				__func__, dev->name,
				bio->bi_crypt_ctx.bc_flags);
			return -EINVAL;
		}
		if (mode != last_mode) {
			pr_info("%s: %s: inconsistent crypt mode %x, expected: %x, bio: %p, req: %p\n",
				__func__, dev->name,
				mode, last_mode, bio, req);
			return -EINVAL;
		}

		if (bio->bi_crypt_ctx.bc_flags != flag) {
			pr_info("%s: %s: inconsistent flag %x, expected: %x, bio: %p, req: %p\n",
				__func__, dev->name,
				bio->bi_crypt_ctx.bc_flags, flag, bio, req);
			hie_dump_req(req, __func__);
			aee_kernel_warning("HIE", "inconsistent flags");
			return -EINVAL;
		}

		if (iv != BC_INVALD_IV) {
			struct bio_vec bv;
			struct bvec_iter iter;
			unsigned long bio_iv;

			bio_iv = bio_bc_iv_get(bio);

			if ((iv + count) != bio_iv) {
				pr_info("%s: %s: inconsis. iv %lu, expected: %lu, bio: %p, req: %p\n",
					__func__, dev->name, bio_iv,
					(iv + count), bio, req);
				hie_dump_req(req, __func__);
				aee_kernel_warning("HIE", "inconsistent iv.");
				return -EINVAL;
			}
			bio_for_each_segment(bv, bio, iter)
				count++;
		}
	}

	if (crypt_mode)
		*crypt_mode = mode;

	return 0;
}

static int hie_key_payload(struct bio_crypt_ctx *ctx, const char *data,
	const unsigned char **key)
{
	int ret = -EINVAL;
	unsigned long flags;
	struct hie_fs *fs, *n;

	spin_lock_irqsave(&hie_fs_list_lock, flags);
	list_for_each_entry_safe(fs, n, &hie_fs_list, list) {
		if (fs->key_payload) {
			ret = fs->key_payload(ctx, data, key);
			if (ret != -EINVAL || ret >= 0)
				break;
		}
	}
	spin_unlock_irqrestore(&hie_fs_list_lock, flags);

	return ret;
}

static int hie_req_key_act(struct hie_dev *dev, struct request *req,
	hie_act act, void *priv)
{
	struct key *keyring_key = NULL;
	const unsigned char *key = NULL;
	const struct user_key_payload *ukp;
	struct bio *bio = req->bio;
	unsigned int mode = 0;
	int key_size = 0;
	int ret;

	if (!hie_is_ready())
		return -ENODEV;

	if (!hie_request_crypted(req))
		return 0;

	if (hie_debug(HIE_DBG_BIO))
		hie_dump_req(req, __func__);

	if (hie_req_verify(req, dev, &mode))
		return -EINVAL;

	key_size = bio->bi_crypt_ctx.bc_key_size;
	keyring_key = bio->bi_crypt_ctx.bc_keyring_key;
try_lock_key:
	ret = down_read_trylock(&keyring_key->sem);
	if (!ret)
		goto try_lock_key;

	ukp = user_key_payload_locked(keyring_key);
	ret = hie_key_payload(&bio->bi_crypt_ctx, ukp->data, &key);

	if (ret == -EINVAL) {
		pr_info("HIE: %s: key payload was not recognized by fs: %p\n",
			__func__, ukp->data);
		ret = -ENOKEY;
	} else if (ret >= 0 && ret != key_size) {
		pr_info("HIE: %s: key size mismatch, ctx: %d, payload: %d\n",
			__func__, key_size, ret);
		ret = -ENOKEY;
	}

	if (ret < 0)
		goto out;

	ret = 0;

#ifndef CONFIG_HIE_NO_CRYPT
#ifdef CONFIG_HIE_DUMMY_CRYPT
#ifdef CONFIG_HIE_DUMMY_CRYPT_KEY_SWITCH
	hie_dummy_crypt_set_key(req, readl(key));
#else
	hie_dummy_crypt_set_key(req, 0xFFFFFFFF);
#endif
	if (bio_data_dir(bio) == WRITE)
		ret = hie_dummy_crypt_req("<req>", req, 0);
#else
	if (act)
		ret = act(mode, key, key_size, req, priv);
#endif
#endif

#ifdef CONFIG_HIE_DEBUG
	if (key && hie_debug(HIE_DBG_KEY)) {
		pr_info("HIE: %s: master key\n", __func__);
		print_hex_dump(KERN_DEBUG, "fs-key: ", DUMP_PREFIX_ADDRESS,
			16, 1, key, key_size, 0);
	}
#endif

out:
	if (keyring_key)
		up_read(&keyring_key->sem);

	return ret;
}

struct hie_key_info {
	char *key;
	int size;
};

/**
 * hie_decrypt / hie_encrypt - get key from bio and invoke cryption callback.
 * @dev:	hie device
 * @bio:	bio request
 * @priv:	private data to decryption callback.
 *
 * RETURNS:
 *   The return value of cryption callback.
 *   -ENODEV, if the hie device is not registered.
 *   -EINVAL, if the crpyt algorithm is not supported by the device.
 *   -ENOKEY, if the master key is absent.
 */
int hie_decrypt(struct hie_dev *dev, struct request *req, void *priv)
{
	int ret;

	ret = hie_req_key_act(dev, req, dev->decrypt, priv);
#ifdef CONFIG_HIE_DEBUG
	if (hie_debug(HIE_DBG_HIE))
		pr_info("HIE: %s: req: %p, ret=%d\n", __func__, req, ret);
#endif
	return ret;
}
EXPORT_SYMBOL(hie_decrypt);

int hie_encrypt(struct hie_dev *dev, struct request *req, void *priv)
{
	int ret;

	ret = hie_req_key_act(dev, req, dev->encrypt, priv);
#ifdef CONFIG_HIE_DEBUG
	if (hie_debug(HIE_DBG_HIE))
		pr_info("HIE: %s: req: %p, ret=%d\n", __func__, req, ret);
#endif
	return ret;
}
EXPORT_SYMBOL(hie_encrypt);

/**
 * hie_set_bio_crypt_context - attach encrpytion info to the bio
 * @inode:	reference inode
 * @bio:    target bio
 * RETURNS:
 *   0, the inode has enabled encryption, and it's encryption info is
 *      successfuly attached to the bio.
 *   -EINVAL, the inode has not enabled encryption, or there's no matching
 *            file system.
 */
int hie_set_bio_crypt_context(struct inode *inode, struct bio *bio)
{
	int ret = 0;
	struct hie_fs *fs, *n;
	unsigned long flags;

	spin_lock_irqsave(&hie_fs_list_lock, flags);
	list_for_each_entry_safe(fs, n, &hie_fs_list, list) {
		if (fs->set_bio_context) {
			ret = fs->set_bio_context(inode, bio);
			if (ret != -EINVAL || ret == 0)
				break;
		}
	}
	spin_unlock_irqrestore(&hie_fs_list_lock, flags);

	return ret;
}
EXPORT_SYMBOL(hie_set_bio_crypt_context);

/**
 * hie_set_dio_crypt_context - attach encrpytion info to the bio of sdio
 * @inode:	reference inode
 * @bio:    target sdio->bio
 * RETURNS:
 *   0, the inode has enabled encryption, and it's encryption info is
 *      successfuly attached to the bio.
 *   -EINVAL, the inode has not enabled encryption, or there's no matching
 *            file system.
 */
int hie_set_dio_crypt_context(struct inode *inode, struct bio *bio,
	loff_t fs_offset)
{
	int ret = 0;

	ret = hie_set_bio_crypt_context(inode, bio);
	if (bio_encrypted(bio) && bio_bcf_test(bio, BC_IV_PAGE_IDX))
		bio_bc_iv_set(bio, fs_offset >> PAGE_SHIFT);

	return ret;
}
EXPORT_SYMBOL(hie_set_dio_crypt_context);


/**
 * hie_get_iv - get initialization vector(iv.) from the request.
 *     The iv. is the file logical block number translated from
 *     (page index * page size + page offset) / physical block size.
 * @req: request
 *
 * RETURNS:
 *   Zero, if the iv. was not assigned in the request,
 *         or the request was not crypt.
 *   Non-Zero, the iv. of the starting bio.
 */
u64 hie_get_iv(struct request *req)
{
	u64 ino;
	u64 iv;
	unsigned int bz_bits;
	struct bio *bio = req->bio;

	if (!req->q)
		return 0;

	if (!hie_request_crypted(req))
		return 0;

	if (!bio_bcf_test(bio, BC_IV_PAGE_IDX))
		return 0;

	ino = bio_bc_ino(bio);
	iv = bio_bc_iv_get(bio);

	WARN_ON(iv == BC_INVALD_IV);

	bz_bits = blksize_bits(queue_physical_block_size(req->q));

	if (bz_bits < PAGE_SHIFT) {
		struct bio_vec iter;

		bio_get_first_bvec(bio, &iter);
		iv = (iv << (PAGE_SHIFT - bz_bits)) +
			 (iter.bv_offset >> bz_bits);
	} else
		iv = iv >> (bz_bits - PAGE_SHIFT);

	iv = (ino << 32 | (iv & 0xFFFFFFFF));

	if (!iv)
		iv = ~iv;

	return iv;
}
EXPORT_SYMBOL(hie_get_iv);

#ifdef CONFIG_HIE_DEBUG
static void *hie_seq_start(struct seq_file *seq, loff_t *pos)
{
	unsigned int idx;

	if (*pos < 0 || *pos >= 1)
		return NULL;

	idx = *pos + 1;
	return (void *) ((unsigned long) idx);
}

static void *hie_seq_next(struct seq_file *seq, void *v, loff_t *pos)
{
	unsigned int idx;

	++*pos;
	if (*pos < 0 || *pos >= 1)
		return NULL;

	idx = *pos + 1;
	return (void *) ((unsigned long) idx);
}

static void hie_seq_stop(struct seq_file *seq, void *v)
{
}

static struct {
	const char *name;
	unsigned int flag;
} crypt_type[] = {
	{"AES_128_XTS", BC_AES_128_XTS},
	{"AES_192_XTS", BC_AES_192_XTS},
	{"AES_256_XTS", BC_AES_256_XTS},
	{"AES_128_CBC", BC_AES_128_CBC},
	{"AES_256_CBC", BC_AES_256_CBC},
	{"AES_128_ECB", BC_AES_128_ECB},
	{"AES_256_ECB", BC_AES_256_ECB},
	{"", 0}
};

static int hie_seq_status_dev(struct seq_file *seq, struct hie_dev *dev)
{
	int i;

	seq_printf(seq, "<%s>\n", dev->name);
	seq_puts(seq, "supported modes:");

	for (i = 0; crypt_type[i].flag ; i++)
		if (crypt_type[i].flag & dev->mode)
			seq_printf(seq, " %s", crypt_type[i].name);

	seq_puts(seq, "\n");

	return 0;
}

static int hie_seq_status_show(struct seq_file *seq, void *v)
{
	struct hie_dev *dev, *dn;
	struct hie_fs *fs, *fn;
	unsigned long flags;

	seq_puts(seq, "[Config]\n");

	if (hie_is_nocrypt())
		seq_puts(seq, "no-crypt\n");
	else if (hie_is_dummy()) {
		seq_puts(seq, "dummy-crpyt");
#ifdef CONFIG_HIE_DUMMY_CRYPT_KEY_SWITCH
		seq_puts(seq, " (key swtich)");
#endif
#ifdef CONFIG_HIE_DUMMY_CRYPT_IV
		seq_puts(seq, " (iv.)");
#endif
		seq_puts(seq, "\n");
	} else
		seq_puts(seq, "hardware-inline-crpyt\n");

	seq_puts(seq, "\n[Registered file systems]\n");
	spin_lock_irqsave(&hie_fs_list_lock, flags);
	list_for_each_entry_safe(fs, fn, &hie_fs_list, list) {
		seq_printf(seq, "%s\n", fs->name);
	}
	spin_unlock_irqrestore(&hie_fs_list_lock, flags);

	seq_puts(seq, "\n[Registered devices]\n");
	spin_lock_irqsave(&hie_dev_list_lock, flags);
	list_for_each_entry_safe(dev, dn, &hie_dev_list, list) {
		hie_seq_status_dev(seq, dev);
	}
	spin_unlock_irqrestore(&hie_dev_list_lock, flags);

	return 0;
}

static const struct seq_operations hie_seq_ops = {
	.start  = hie_seq_start,
	.next   = hie_seq_next,
	.stop   = hie_seq_stop,
	.show   = hie_seq_status_show,
};

static int hie_seq_open(struct inode *inode, struct file *file)
{
	int rc;

	rc = seq_open(file, &hie_seq_ops);

	return rc;
}

static ssize_t hie_seq_write(struct file *file, const char __user *ubuf,
	size_t count, loff_t *ppos)
{
	return count;
}

static const struct file_operations hie_status_fops = {
	.owner		= THIS_MODULE,
	.open		= hie_seq_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= seq_release,
	.write		= hie_seq_write,
};
#endif

static void hie_init_debugfs(void)
{
#ifdef CONFIG_HIE_DEBUG
	if (hie_droot)
		return;

	hie_droot = debugfs_create_dir("hie", NULL);
	if (IS_ERR(hie_droot)) {
		pr_info("[HIE] fail to create debugfs root\n");
		hie_droot = NULL;
		return;
	}

	hie_dbg = 0;
	hie_dbg_ino = 0;
	hie_dbg_sector = 0;

	hie_ddebug = debugfs_create_u32("debug", 0660, hie_droot, &hie_dbg);
	debugfs_create_u64("ino", 0660, hie_droot, &hie_dbg_ino);
	debugfs_create_u64("sector", 0660, hie_droot, &hie_dbg_sector);

	debugfs_create_file("status", 0444, hie_droot,
		(void *)0, &hie_status_fops);
#endif
}

static int __init hie_init(void)
{
	hie_init_debugfs();
	return 0;
}

static void __exit hie_exit(void)
{
}

module_init(hie_init);
module_exit(hie_exit);

MODULE_AUTHOR("Perry Hsu <perry.hsu@mediatek.com>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Hardware Inline Encryption");

