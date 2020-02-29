// SPDX-License-Identifier: GPL-2.0-only
/*
 * QTI CE 32-bit compatibility syscall for 64-bit systems
 *
 * Copyright (c) 2014-2020, The Linux Foundation. All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/qcedev.h>
#include <linux/compat.h>
#include "compat_qcedev.h"

static int compat_get_qcedev_pmem_info(
		struct compat_qcedev_pmem_info __user *pmem32,
		struct qcedev_pmem_info __user *pmem)
{
	compat_ulong_t offset;
	compat_int_t fd_src;
	compat_int_t fd_dst;
	int err, i;
	uint32_t len;

	err = get_user(fd_src, &pmem32->fd_src);
	err |= put_user(fd_src, &pmem->fd_src);

	for (i = 0; i < QCEDEV_MAX_BUFFERS; i++) {
		err |= get_user(offset, &pmem32->src[i].offset);
		err |= put_user(offset, &pmem->src[i].offset);
		err |= get_user(len, &pmem32->src[i].len);
		err |= put_user(len, &pmem->src[i].len);
	}

	err |= get_user(fd_dst, &pmem32->fd_dst);
	err |= put_user(fd_dst, &pmem->fd_dst);

	for (i = 0; i < QCEDEV_MAX_BUFFERS; i++) {
		err |= get_user(offset, &pmem32->dst[i].offset);
		err |= put_user(offset, &pmem->dst[i].offset);
		err |= get_user(len, &pmem32->dst[i].len);
		err |= put_user(len, &pmem->dst[i].len);
	}

	return err;
}

static int compat_put_qcedev_pmem_info(
		struct compat_qcedev_pmem_info __user *pmem32,
		struct qcedev_pmem_info __user *pmem)
{
	compat_ulong_t offset;
	compat_int_t fd_src;
	compat_int_t fd_dst;
	int err, i;
	uint32_t len;

	err = get_user(fd_src, &pmem->fd_src);
	err |= put_user(fd_src, &pmem32->fd_src);

	for (i = 0; i < QCEDEV_MAX_BUFFERS; i++) {
		err |= get_user(offset, &pmem->src[i].offset);
		err |= put_user(offset, &pmem32->src[i].offset);
		err |= get_user(len, &pmem->src[i].len);
		err |= put_user(len, &pmem32->src[i].len);
	}

	err |= get_user(fd_dst, &pmem->fd_dst);
	err |= put_user(fd_dst, &pmem32->fd_dst);

	for (i = 0; i < QCEDEV_MAX_BUFFERS; i++) {
		err |= get_user(offset, &pmem->dst[i].offset);
		err |= put_user(offset, &pmem32->dst[i].offset);
		err |= get_user(len, &pmem->dst[i].len);
		err |= put_user(len, &pmem32->dst[i].len);
	}

	return err;
}

static int compat_get_qcedev_vbuf_info(
		struct compat_qcedev_vbuf_info __user *vbuf32,
		struct qcedev_vbuf_info __user *vbuf)
{
	compat_uptr_t vaddr;
	int err = 0, i;
	uint32_t len;

	for (i = 0; i < QCEDEV_MAX_BUFFERS; i++) {
		err |= get_user(vaddr, &vbuf32->src[i].vaddr);
		err |= put_user(vaddr,
			(compat_uptr_t __user *)&vbuf->src[i].vaddr);
		err |= get_user(len, &vbuf32->src[i].len);
		err |= put_user(len, &vbuf->src[i].len);
	}

	for (i = 0; i < QCEDEV_MAX_BUFFERS; i++) {
		err |= get_user(vaddr, &vbuf32->dst[i].vaddr);
		err |= put_user(vaddr,
			(compat_uptr_t __user *)&vbuf->dst[i].vaddr);
		err |= get_user(len, &vbuf32->dst[i].len);
		err |= put_user(len, &vbuf->dst[i].len);
	}
	return err;
}

static int compat_put_qcedev_vbuf_info(
		struct compat_qcedev_vbuf_info __user *vbuf32,
		struct qcedev_vbuf_info __user *vbuf)
{
	compat_uptr_t vaddr;
	int err = 0, i;
	uint32_t len;

	for (i = 0; i < QCEDEV_MAX_BUFFERS; i++) {
		err |= get_user(vaddr,
				(compat_uptr_t __user *)&vbuf->src[i].vaddr);
		err |= put_user(vaddr, &vbuf32->src[i].vaddr);
		err |= get_user(len, &vbuf->src[i].len);
		err |= put_user(len, &vbuf32->src[i].len);
	}

	for (i = 0; i < QCEDEV_MAX_BUFFERS; i++) {
		err |= get_user(vaddr,
				(compat_uptr_t __user *)&vbuf->dst[i].vaddr);
		err |= put_user(vaddr, &vbuf32->dst[i].vaddr);
		err |= get_user(len, &vbuf->dst[i].len);
		err |= put_user(len, &vbuf32->dst[i].len);
	}
	return err;
}

static int compat_get_qcedev_cipher_op_req(
		struct compat_qcedev_cipher_op_req __user *data32,
		struct qcedev_cipher_op_req __user *data)
{
	enum qcedev_cipher_mode_enum mode;
	enum qcedev_cipher_alg_enum alg;
	compat_ulong_t byteoffset;
	enum qcedev_oper_enum op;
	compat_ulong_t data_len;
	compat_ulong_t encklen;
	compat_ulong_t entries;
	compat_ulong_t ivlen;
	uint8_t in_place_op;
	int err, i;
	uint8_t use_pmem;
	uint8_t enckey;
	uint8_t iv;

	err = get_user(use_pmem, &data32->use_pmem);
	err |= put_user(use_pmem, &data->use_pmem);

	if (use_pmem)
		err |= compat_get_qcedev_pmem_info(&data32->pmem, &data->pmem);
	else
		err |= compat_get_qcedev_vbuf_info(&data32->vbuf, &data->vbuf);

	err |= get_user(entries, &data32->entries);
	err |= put_user(entries, &data->entries);
	err |= get_user(data_len, &data32->data_len);
	err |= put_user(data_len, &data->data_len);
	err |= get_user(in_place_op, &data32->in_place_op);
	err |= put_user(in_place_op, &data->in_place_op);

	for (i = 0; i < QCEDEV_MAX_KEY_SIZE; i++) {
		err |= get_user(enckey, &(data32->enckey[i]));
		err |= put_user(enckey, &(data->enckey[i]));
	}

	err |= get_user(encklen, &data32->encklen);
	err |= put_user(encklen, &data->encklen);

	for (i = 0; i < QCEDEV_MAX_IV_SIZE; i++) {
		err |= get_user(iv, &(data32->iv[i]));
		err |= put_user(iv, &(data->iv[i]));
	}

	err |= get_user(ivlen, &data32->ivlen);
	err |= put_user(ivlen, &data->ivlen);
	err |= get_user(byteoffset, &data32->byteoffset);
	err |= put_user(byteoffset, &data->byteoffset);
	err |= get_user(alg, &data32->alg);
	err |= put_user(alg, &data->alg);
	err |= get_user(mode, &data32->mode);
	err |= put_user(mode, &data->mode);
	err |= get_user(op, &data32->op);
	err |= put_user(op, &data->op);

	return err;
}

static int compat_put_qcedev_cipher_op_req(
		struct compat_qcedev_cipher_op_req __user *data32,
		struct qcedev_cipher_op_req __user *data)
{
	enum qcedev_cipher_mode_enum mode;
	enum qcedev_cipher_alg_enum alg;
	compat_ulong_t byteoffset;
	enum qcedev_oper_enum op;
	compat_ulong_t data_len;
	compat_ulong_t encklen;
	compat_ulong_t entries;
	compat_ulong_t ivlen;
	uint8_t in_place_op;
	int err, i;
	uint8_t use_pmem;
	uint8_t enckey;
	uint8_t iv;

	err = get_user(use_pmem, &data->use_pmem);
	err |= put_user(use_pmem, &data32->use_pmem);

	if (use_pmem)
		err |= compat_put_qcedev_pmem_info(&data32->pmem, &data->pmem);
	else
		err |= compat_put_qcedev_vbuf_info(&data32->vbuf, &data->vbuf);

	err |= get_user(entries, &data->entries);
	err |= put_user(entries, &data32->entries);
	err |= get_user(data_len, &data->data_len);
	err |= put_user(data_len, &data32->data_len);
	err |= get_user(in_place_op, &data->in_place_op);
	err |= put_user(in_place_op, &data32->in_place_op);

	for (i = 0; i < QCEDEV_MAX_KEY_SIZE; i++) {
		err |= get_user(enckey, &(data->enckey[i]));
		err |= put_user(enckey, &(data32->enckey[i]));
	}

	err |= get_user(encklen, &data->encklen);
	err |= put_user(encklen, &data32->encklen);

	for (i = 0; i < QCEDEV_MAX_IV_SIZE; i++) {
		err |= get_user(iv, &(data->iv[i]));
		err |= put_user(iv, &(data32->iv[i]));
	}

	err |= get_user(ivlen, &data->ivlen);
	err |= put_user(ivlen, &data32->ivlen);
	err |= get_user(byteoffset, &data->byteoffset);
	err |= put_user(byteoffset, &data32->byteoffset);
	err |= get_user(alg, &data->alg);
	err |= put_user(alg, &data32->alg);
	err |= get_user(mode, &data->mode);
	err |= put_user(mode, &data32->mode);
	err |= get_user(op, &data->op);
	err |= put_user(op, &data32->op);

	return err;
}

static int compat_xfer_qcedev_map_buf_req(
			struct compat_qcedev_map_buf_req __user *data32,
			struct qcedev_map_buf_req __user *data, bool to_get)
{
	int rc = 0, i, fd = -1;
	uint32_t fd_size, fd_offset, num_fds, buf_vaddr;

	if (to_get) {
		/* copy from compat struct */
		for (i = 0; i < QCEDEV_MAX_BUFFERS; i++) {
			rc |= get_user(fd, &data32->fd[i]);
			rc |= put_user(fd, &data->fd[i]);
			rc |= get_user(fd_size, &data32->fd_size[i]);
			rc |= put_user(fd_size, &data->fd_size[i]);
			rc |= get_user(fd_offset, &data32->fd_offset[i]);
			rc |= put_user(fd_offset, &data->fd_offset[i]);
			rc |= get_user(buf_vaddr, &data32->buf_vaddr[i]);
			rc |= put_user(buf_vaddr, &data->buf_vaddr[i]);
		}

		rc |= get_user(num_fds, &data32->num_fds);
		rc |= put_user(num_fds, &data->num_fds);
	} else {
		/* copy to compat struct */
		for (i = 0; i < QCEDEV_MAX_BUFFERS; i++) {
			rc |= get_user(fd, &data->fd[i]);
			rc |= put_user(fd, &data32->fd[i]);
			rc |= get_user(fd_size, &data->fd_size[i]);
			rc |= put_user(fd_size, &data32->fd_size[i]);
			rc |= get_user(fd_offset, &data->fd_offset[i]);
			rc |= put_user(fd_offset, &data32->fd_offset[i]);
			rc |= get_user(buf_vaddr, &data->buf_vaddr[i]);
			rc |= put_user(buf_vaddr, &data32->buf_vaddr[i]);
		}
		rc |= get_user(num_fds, &data->num_fds);
		rc |= put_user(num_fds, &data32->num_fds);
	}

	return rc;
}

static int compat_xfer_qcedev_unmap_buf_req(
			struct compat_qcedev_unmap_buf_req __user *data32,
			struct qcedev_unmap_buf_req __user *data, bool to_get)
{
	int i, rc = 0, fd = -1;
	uint32_t num_fds;

	if (to_get) {
		/* copy from compat struct */
		for (i = 0; i < QCEDEV_MAX_BUFFERS; i++) {
			rc |= get_user(fd, &data32->fd[i]);
			rc |= put_user(fd, &data->fd[i]);
		}
		rc |= get_user(num_fds, &data32->num_fds);
		rc |= put_user(num_fds, &data->num_fds);
	} else {
		/* copy to compat struct */
		for (i = 0; i < QCEDEV_MAX_BUFFERS; i++) {
			rc |= get_user(fd, &data->fd[i]);
			rc |= put_user(fd, &data32->fd[i]);
		}
		rc |= get_user(num_fds, &data->num_fds);
		rc |= put_user(num_fds, &data32->num_fds);
	}
	return rc;
}


static int compat_get_qcedev_sha_op_req(
		struct compat_qcedev_sha_op_req __user *data32,
		struct qcedev_sha_op_req __user *data)
{
	enum qcedev_sha_alg_enum alg;
	compat_ulong_t authklen;
	compat_ulong_t data_len;
	compat_ulong_t entries;
	compat_ulong_t diglen;
	compat_uptr_t authkey;
	compat_uptr_t vaddr;
	int err = 0, i;
	uint8_t digest;
	uint32_t len;

	for (i = 0; i < QCEDEV_MAX_BUFFERS; i++) {
		err |= get_user(vaddr, &data32->data[i].vaddr);
		err |= put_user(vaddr,
			(compat_uptr_t __user *)&data->data[i].vaddr);
		err |= get_user(len, &data32->data[i].len);
		err |= put_user(len, &data->data[i].len);
	}

	err |= get_user(entries, &data32->entries);
	err |= put_user(entries, &data->entries);
	err |= get_user(data_len, &data32->data_len);
	err |= put_user(data_len, &data->data_len);

	for (i = 0; i < QCEDEV_MAX_SHA_DIGEST; i++) {
		err |= get_user(digest, &(data32->digest[i]));
		err |= put_user(digest, &(data->digest[i]));
	}

	err |= get_user(diglen, &data32->diglen);
	err |= put_user(diglen, &data->diglen);
	err |= get_user(authkey, &data32->authkey);
	err |= put_user(authkey, (compat_uptr_t __user *)&data->authkey);
	err |= get_user(authklen, &data32->authklen);
	err |= put_user(authklen, &data->authklen);
	err |= get_user(alg, &data32->alg);
	err |= put_user(alg, &data->alg);

	return err;
}

static int compat_put_qcedev_sha_op_req(
		struct compat_qcedev_sha_op_req __user *data32,
		struct qcedev_sha_op_req __user *data)
{
	enum qcedev_sha_alg_enum alg;
	compat_ulong_t authklen;
	compat_ulong_t data_len;
	compat_ulong_t entries;
	compat_ulong_t diglen;
	compat_uptr_t authkey;
	compat_uptr_t vaddr;
	int err = 0, i;
	uint8_t digest;
	uint32_t len;

	for (i = 0; i < QCEDEV_MAX_BUFFERS; i++) {
		err |= get_user(vaddr,
			(compat_uptr_t __user *)&data->data[i].vaddr);
		err |= put_user(vaddr, &data32->data[i].vaddr);
		err |= get_user(len, &data->data[i].len);
		err |= put_user(len, &data32->data[i].len);
	}

	err |= get_user(entries, &data->entries);
	err |= put_user(entries, &data32->entries);
	err |= get_user(data_len, &data->data_len);
	err |= put_user(data_len, &data32->data_len);

	for (i = 0; i < QCEDEV_MAX_SHA_DIGEST; i++) {
		err |= get_user(digest, &(data->digest[i]));
		err |= put_user(digest, &(data32->digest[i]));
	}

	err |= get_user(diglen, &data->diglen);
	err |= put_user(diglen, &data32->diglen);
	err |= get_user(authkey,
			(compat_uptr_t __user *)&data->authkey);
	err |= put_user(authkey, &data32->authkey);
	err |= get_user(authklen, &data->authklen);
	err |= put_user(authklen, &data32->authklen);
	err |= get_user(alg, &data->alg);
	err |= put_user(alg, &data32->alg);

	return err;
}

static unsigned int convert_cmd(unsigned int cmd)
{
	switch (cmd) {
	case COMPAT_QCEDEV_IOCTL_ENC_REQ:
		return QCEDEV_IOCTL_ENC_REQ;
	case COMPAT_QCEDEV_IOCTL_DEC_REQ:
		return QCEDEV_IOCTL_DEC_REQ;
	case COMPAT_QCEDEV_IOCTL_SHA_INIT_REQ:
		return QCEDEV_IOCTL_SHA_INIT_REQ;
	case COMPAT_QCEDEV_IOCTL_SHA_UPDATE_REQ:
		return QCEDEV_IOCTL_SHA_UPDATE_REQ;
	case COMPAT_QCEDEV_IOCTL_SHA_FINAL_REQ:
		return QCEDEV_IOCTL_SHA_FINAL_REQ;
	case COMPAT_QCEDEV_IOCTL_GET_SHA_REQ:
		return QCEDEV_IOCTL_GET_SHA_REQ;
	case COMPAT_QCEDEV_IOCTL_GET_CMAC_REQ:
		return QCEDEV_IOCTL_GET_CMAC_REQ;
	case COMPAT_QCEDEV_IOCTL_MAP_BUF_REQ:
		return QCEDEV_IOCTL_MAP_BUF_REQ;
	case COMPAT_QCEDEV_IOCTL_UNMAP_BUF_REQ:
		return QCEDEV_IOCTL_UNMAP_BUF_REQ;
	default:
		return cmd;
	}

}

long compat_qcedev_ioctl(struct file *file,
		unsigned int cmd, unsigned long arg)
{
	long ret;

	switch (cmd) {
	case COMPAT_QCEDEV_IOCTL_ENC_REQ:
	case COMPAT_QCEDEV_IOCTL_DEC_REQ: {
		struct compat_qcedev_cipher_op_req __user *data32;
		struct qcedev_cipher_op_req __user *data;
		int err;

		data32 = compat_ptr(arg);
		data = compat_alloc_user_space(sizeof(*data));
		if (!data)
			return -EFAULT;

		err = compat_get_qcedev_cipher_op_req(data32, data);
		if (err)
			return err;

		ret = qcedev_ioctl(file, convert_cmd(cmd), (unsigned long)data);
		err = compat_put_qcedev_cipher_op_req(data32, data);
		return ret ? ret : err;
	}
	case COMPAT_QCEDEV_IOCTL_SHA_INIT_REQ:
	case COMPAT_QCEDEV_IOCTL_SHA_UPDATE_REQ:
	case COMPAT_QCEDEV_IOCTL_SHA_FINAL_REQ:
	case COMPAT_QCEDEV_IOCTL_GET_CMAC_REQ:
	case COMPAT_QCEDEV_IOCTL_GET_SHA_REQ: {
		struct compat_qcedev_sha_op_req __user *data32;
		struct qcedev_sha_op_req __user *data;
		int err;

		data32 = compat_ptr(arg);
		data = compat_alloc_user_space(sizeof(*data));
		if (!data)
			return -EFAULT;

		err = compat_get_qcedev_sha_op_req(data32, data);
		if (err)
			return err;

		ret = qcedev_ioctl(file, convert_cmd(cmd), (unsigned long)data);
		err = compat_put_qcedev_sha_op_req(data32, data);
		return ret ? ret : err;
	}
	case COMPAT_QCEDEV_IOCTL_MAP_BUF_REQ: {
		struct compat_qcedev_map_buf_req __user *data32;
		struct qcedev_map_buf_req __user *data;
		int err;

		data32 = compat_ptr(arg);
		data = compat_alloc_user_space(sizeof(*data));
		if (!data)
			return -EINVAL;

		err = compat_xfer_qcedev_map_buf_req(data32, data, true);
		if (err)
			return err;

		ret = qcedev_ioctl(file, convert_cmd(cmd), (unsigned long)data);
		err = compat_xfer_qcedev_map_buf_req(data32, data, false);
		return ret ? ret : err;

		break;
	}
	case COMPAT_QCEDEV_IOCTL_UNMAP_BUF_REQ: {
		struct compat_qcedev_unmap_buf_req __user *data32;
		struct qcedev_unmap_buf_req __user *data;
		int err;

		data32 = compat_ptr(arg);
		data = compat_alloc_user_space(sizeof(*data));
		if (!data)
			return -EINVAL;

		err = compat_xfer_qcedev_unmap_buf_req(data32, data, true);
		if (err)
			return err;

		ret = qcedev_ioctl(file, convert_cmd(cmd), (unsigned long)data);
		err = compat_xfer_qcedev_unmap_buf_req(data32, data, false);
		return ret ? ret : err;

		break;
	}
	default:
		return -ENOIOCTLCMD;
	}
	return 0;
}
