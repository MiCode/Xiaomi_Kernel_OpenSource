/* Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
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

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/miscdevice.h>
#include <linux/poll.h>
#include <linux/fdtable.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/anon_inodes.h>
#include <linux/smcinvoke.h>
#include <soc/qcom/scm.h>
#include <asm/cacheflush.h>
#include "smcinvoke_object.h"
#include <soc/qcom/qseecomi.h>
#include "../../misc/qseecom_kernel.h"

#define SMCINVOKE_TZ_PARAM_ID		0x224
#define SMCINVOKE_TZ_CMD		0x32000600
#define SMCINVOKE_FILE		        "smcinvoke"
#define SMCINVOKE_TZ_ROOT_OBJ		1
#define SMCINVOKE_TZ_MIN_BUF_SIZE	4096
#define SMCINVOKE_ARGS_ALIGN_SIZE	(sizeof(uint64_t))
#define SMCINVOKE_TZ_OBJ_NULL		0

#define FOR_ARGS(ndxvar, counts, section)                      \
	for (ndxvar = object_counts_index_##section(counts);     \
		ndxvar < (object_counts_index_##section(counts)  \
		+ object_counts_num_##section(counts));          \
		++ndxvar)

static long smcinvoke_ioctl(struct file *, unsigned, unsigned long);
static int smcinvoke_open(struct inode *, struct file *);
static int smcinvoke_release(struct inode *, struct file *);

static const struct file_operations smcinvoke_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = smcinvoke_ioctl,
	.compat_ioctl = smcinvoke_ioctl,
	.open = smcinvoke_open,
	.release = smcinvoke_release,
};

static struct miscdevice smcinvoke_miscdev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "smcinvoke",
	.fops = &smcinvoke_fops
};

struct smcinvoke_buf_hdr {
	uint32_t offset;
	uint32_t size;
};

union smcinvoke_tz_args {
	struct smcinvoke_buf_hdr b;
	uint32_t		 tzhandle;
};
struct smcinvoke_msg_hdr {
	uint32_t	tzhandle;
	uint32_t	op;
	uint32_t	counts;
};

struct smcinvoke_tzobj_context {
	uint32_t	tzhandle;
};

/*
 * size_add saturates at SIZE_MAX. If integer overflow is detected,
 * this function would return SIZE_MAX otherwise normal a+b is returned.
 */
static inline size_t size_add(size_t a, size_t b)
{
	return (b > (SIZE_MAX - a)) ? SIZE_MAX : a + b;
}

/*
 * pad_size is used along with size_align to define a buffer overflow
 * protected version of ALIGN
 */
static inline size_t pad_size(size_t a, size_t b)
{
	return (~a + 1) % b;
}

/*
 * size_align saturates at SIZE_MAX. If integer overflow is detected, this
 * function would return SIZE_MAX otherwise next aligned size is returned.
 */
static inline size_t size_align(size_t a, size_t b)
{
	return size_add(a, pad_size(a, b));
}

/*
 * This function retrieves file pointer corresponding to FD provided. It stores
 * retrived file pointer until IOCTL call is concluded. Once call is completed,
 * all stored file pointers are released. file pointers are stored to prevent
 * other threads from releasing that FD while IOCTL is in progress.
 */
static int get_tzhandle_from_fd(int64_t fd, struct file **filp,
				uint32_t *tzhandle)
{
	int ret = -EBADF;
	struct file *tmp_filp = NULL;
	struct smcinvoke_tzobj_context *tzobj = NULL;

	if (fd == SMCINVOKE_USERSPACE_OBJ_NULL) {
		*tzhandle = SMCINVOKE_TZ_OBJ_NULL;
		ret = 0;
		goto out;
	} else if (fd < SMCINVOKE_USERSPACE_OBJ_NULL) {
		goto out;
	}

	tmp_filp = fget(fd);
	if (!tmp_filp)
		goto out;

	/* Verify if filp is smcinvoke device's file pointer */
	if (!tmp_filp->f_op || !tmp_filp->private_data ||
		(tmp_filp->f_op != &smcinvoke_fops)) {
		fput(tmp_filp);
		goto out;
	}

	tzobj = tmp_filp->private_data;
	*tzhandle = tzobj->tzhandle;
	*filp = tmp_filp;
	ret = 0;
out:
	return ret;
}

static int get_fd_from_tzhandle(uint32_t tzhandle, int64_t *fd)
{
	int unused_fd = -1, ret = -1;
	struct file *f = NULL;
	struct smcinvoke_tzobj_context *cxt = NULL;

	if (tzhandle == SMCINVOKE_TZ_OBJ_NULL) {
		*fd = SMCINVOKE_USERSPACE_OBJ_NULL;
		ret = 0;
		goto out;
	}

	cxt = kzalloc(sizeof(*cxt), GFP_KERNEL);
	if (!cxt) {
		ret = -ENOMEM;
		goto out;
	}
	unused_fd = get_unused_fd_flags(O_RDWR);
	if (unused_fd < 0)
		goto out;

	f = anon_inode_getfile(SMCINVOKE_FILE, &smcinvoke_fops, cxt, O_RDWR);
	if (IS_ERR(f))
		goto out;

	*fd = unused_fd;
	fd_install(*fd, f);
	((struct smcinvoke_tzobj_context *)
			(f->private_data))->tzhandle = tzhandle;
	return 0;
out:
	if (unused_fd >= 0)
		put_unused_fd(unused_fd);
	kfree(cxt);

	return ret;
}

static int prepare_send_scm_msg(const uint8_t *in_buf, size_t in_buf_len,
				const uint8_t *out_buf, size_t out_buf_len,
				int32_t *smcinvoke_result)
{
	int ret = 0;
	struct scm_desc desc = {0};
	size_t inbuf_flush_size = (1UL << get_order(in_buf_len)) * PAGE_SIZE;
	size_t outbuf_flush_size = (1UL << get_order(out_buf_len)) * PAGE_SIZE;

	desc.arginfo = SMCINVOKE_TZ_PARAM_ID;
	desc.args[0] = (uint64_t)virt_to_phys(in_buf);
	desc.args[1] = inbuf_flush_size;
	desc.args[2] = (uint64_t)virt_to_phys(out_buf);
	desc.args[3] = outbuf_flush_size;

	dmac_flush_range(in_buf, in_buf + inbuf_flush_size);
	dmac_flush_range(out_buf, out_buf + outbuf_flush_size);

	ret = scm_call2(SMCINVOKE_TZ_CMD, &desc);

	/* process listener request */
	if (!ret && (desc.ret[0] == QSEOS_RESULT_INCOMPLETE ||
		desc.ret[0] == QSEOS_RESULT_BLOCKED_ON_LISTENER))
		ret = qseecom_process_listener_from_smcinvoke(&desc);

	*smcinvoke_result = (int32_t)desc.ret[1];
	if (ret || desc.ret[1] || desc.ret[2] || desc.ret[0])
		pr_err("SCM call failed with ret val = %d %d %d %d\n",
						ret, (int)desc.ret[0],
				(int)desc.ret[1], (int)desc.ret[2]);

	dmac_inv_range(in_buf, in_buf + inbuf_flush_size);
	dmac_inv_range(out_buf, out_buf + outbuf_flush_size);
	return ret;
}

static int marshal_out(void *buf, uint32_t buf_size,
				struct smcinvoke_cmd_req *req,
				union smcinvoke_arg *args_buf)
{
	int ret = -EINVAL, i = 0;
	union smcinvoke_tz_args *tz_args = NULL;
	size_t offset = sizeof(struct smcinvoke_msg_hdr) +
				object_counts_total(req->counts) *
					sizeof(union smcinvoke_tz_args);

	if (offset > buf_size)
		goto out;

	tz_args = (union smcinvoke_tz_args *)
				(buf + sizeof(struct smcinvoke_msg_hdr));

	tz_args += object_counts_num_BI(req->counts);

	FOR_ARGS(i, req->counts, BO) {
		args_buf[i].b.size = tz_args->b.size;
		if ((buf_size - tz_args->b.offset < tz_args->b.size) ||
			tz_args->b.offset > buf_size) {
			pr_err("%s: buffer overflow detected\n", __func__);
			goto out;
		}
		if (copy_to_user((void __user *)(uintptr_t)(args_buf[i].b.addr),
			(uint8_t *)(buf) + tz_args->b.offset,
						tz_args->b.size)) {
			pr_err("Error %d copying ctxt to user\n", ret);
			goto out;
		}
		tz_args++;
	}
	tz_args += object_counts_num_OI(req->counts);

	FOR_ARGS(i, req->counts, OO) {
		/*
		 * create a new FD and assign to output object's
		 * context
		 */
		ret = get_fd_from_tzhandle(tz_args->tzhandle,
						&(args_buf[i].o.fd));
		if (ret)
			goto out;
		tz_args++;
	}
	ret = 0;
out:
	return ret;
}

/*
 * SMC expects arguments in following format
 * ---------------------------------------------------------------------------
 * | cxt | op | counts | ptr|size |ptr|size...|ORef|ORef|...| rest of payload |
 * ---------------------------------------------------------------------------
 * cxt: target, op: operation, counts: total arguments
 * offset: offset is from beginning of buffer i.e. cxt
 * size: size is 8 bytes aligned value
 */
static size_t compute_in_msg_size(const struct smcinvoke_cmd_req *req,
					const union smcinvoke_arg *args_buf)
{
	uint32_t i = 0;

	size_t total_size = sizeof(struct smcinvoke_msg_hdr) +
				object_counts_total(req->counts) *
					sizeof(union smcinvoke_tz_args);

	/* Computed total_size should be 8 bytes aligned from start of buf */
	total_size = ALIGN(total_size, SMCINVOKE_ARGS_ALIGN_SIZE);

	/* each buffer has to be 8 bytes aligned */
	while (i < object_counts_num_buffers(req->counts))
		total_size = size_add(total_size,
		size_align(args_buf[i++].b.size, SMCINVOKE_ARGS_ALIGN_SIZE));

	/* Since we're using get_free_pages, no need for explicit PAGE align */
	return total_size;
}

static int marshal_in(const struct smcinvoke_cmd_req *req,
			const union smcinvoke_arg *args_buf, uint32_t tzhandle,
			uint8_t *buf, size_t buf_size, struct file **arr_filp)
{
	int ret = -EINVAL, i = 0;
	union smcinvoke_tz_args *tz_args = NULL;
	struct smcinvoke_msg_hdr msg_hdr = {tzhandle, req->op, req->counts};
	uint32_t offset = sizeof(struct smcinvoke_msg_hdr) +
				sizeof(union smcinvoke_tz_args) *
				object_counts_total(req->counts);

	if (buf_size < offset)
		goto out;

	*(struct smcinvoke_msg_hdr *)buf = msg_hdr;
	tz_args = (union smcinvoke_tz_args *)
			(buf + sizeof(struct smcinvoke_msg_hdr));

	FOR_ARGS(i, req->counts, BI) {
		offset = size_align(offset, SMCINVOKE_ARGS_ALIGN_SIZE);
		if ((offset > buf_size) ||
			(args_buf[i].b.size > (buf_size - offset)))
			goto out;

		tz_args->b.offset = offset;
		tz_args->b.size = args_buf[i].b.size;
		tz_args++;

		if (copy_from_user(buf+offset,
			(void __user *)(uintptr_t)(args_buf[i].b.addr),
						args_buf[i].b.size))
			goto out;

		offset += args_buf[i].b.size;
	}
	FOR_ARGS(i, req->counts, BO) {
		offset = size_align(offset, SMCINVOKE_ARGS_ALIGN_SIZE);
		if ((offset > buf_size) ||
			(args_buf[i].b.size > (buf_size - offset)))
			goto out;

		tz_args->b.offset = offset;
		tz_args->b.size = args_buf[i].b.size;
		tz_args++;

		offset += args_buf[i].b.size;
	}
	FOR_ARGS(i, req->counts, OI) {
		if (get_tzhandle_from_fd(args_buf[i].o.fd,
					&arr_filp[i], &(tz_args->tzhandle)))
			goto out;
		tz_args++;
	}
	ret = 0;
out:
	return ret;
}

long smcinvoke_ioctl(struct file *filp, unsigned cmd, unsigned long arg)
{
	int    ret = -1, i = 0, nr_args = 0;
	struct smcinvoke_cmd_req req = {0};
	void   *in_msg = NULL;
	size_t inmsg_size = 0;
	void   *out_msg = NULL;
	union  smcinvoke_arg *args_buf = NULL;
	struct file *filp_to_release[object_counts_max_OO] = {NULL};
	struct smcinvoke_tzobj_context *tzobj = filp->private_data;

	switch (cmd) {
	case SMCINVOKE_IOCTL_INVOKE_REQ:
		if (_IOC_SIZE(cmd) != sizeof(req)) {
			ret =  -EINVAL;
			goto out;
		}
		ret = copy_from_user(&req, (void __user *)arg, sizeof(req));
		if (ret) {
			ret =  -EFAULT;
			goto out;
		}

		nr_args = object_counts_num_buffers(req.counts) +
				object_counts_num_objects(req.counts);

		if (req.argsize != sizeof(union smcinvoke_arg)) {
			ret = -EINVAL;
			goto out;
		}

		if (nr_args) {

			args_buf = kzalloc(nr_args * req.argsize, GFP_KERNEL);
			if (!args_buf) {
				ret = -ENOMEM;
				goto out;
			}

			ret = copy_from_user(args_buf,
					(void __user *)(uintptr_t)(req.args),
						nr_args * req.argsize);

			if (ret) {
				ret = -EFAULT;
				goto out;
			}
		}

		inmsg_size = compute_in_msg_size(&req, args_buf);
		in_msg = (void *)__get_free_pages(GFP_KERNEL,
						get_order(inmsg_size));
		if (!in_msg) {
			ret = -ENOMEM;
			goto out;
		}

		out_msg = (void *)__get_free_page(GFP_KERNEL);
		if (!out_msg) {
			ret = -ENOMEM;
			goto out;
		}

		ret = marshal_in(&req, args_buf, tzobj->tzhandle, in_msg,
					inmsg_size, filp_to_release);
		if (ret)
			goto out;

		ret = prepare_send_scm_msg(in_msg, inmsg_size, out_msg,
				SMCINVOKE_TZ_MIN_BUF_SIZE, &req.result);
		if (ret)
			goto out;

		ret = marshal_out(in_msg, inmsg_size, &req, args_buf);

		ret |=  copy_to_user((void __user *)(uintptr_t)(req.args),
					args_buf, nr_args * req.argsize);
		ret |=  copy_to_user((void __user *)arg, &req, sizeof(req));
		if (ret)
			goto out;

		break;
	default:
		ret = -ENOIOCTLCMD;
		break;
	}
out:
	free_page((long)out_msg);
	free_pages((long)in_msg, get_order(inmsg_size));
	kfree(args_buf);
	for (i = 0; i < object_counts_max_OO; i++) {
		if (filp_to_release[i])
			fput(filp_to_release[i]);
	}

	return ret;
}

static int smcinvoke_open(struct inode *nodp, struct file *filp)
{
	struct smcinvoke_tzobj_context *tzcxt = NULL;

	tzcxt = kzalloc(sizeof(*tzcxt), GFP_KERNEL);
	if (!tzcxt)
		return -ENOMEM;

	tzcxt->tzhandle = SMCINVOKE_TZ_ROOT_OBJ;
	filp->private_data = tzcxt;

	return 0;
}


static int smcinvoke_release(struct inode *nodp, struct file *filp)
{
	int ret = 0, smcinvoke_result = 0;
	uint8_t *in_buf = NULL;
	uint8_t *out_buf = NULL;
	struct smcinvoke_msg_hdr hdr = {0};
	struct smcinvoke_tzobj_context *tzobj = filp->private_data;
	uint32_t tzhandle = tzobj->tzhandle;

	/* Root object is special in sense it is indestructible */
	if (!tzhandle || tzhandle == SMCINVOKE_TZ_ROOT_OBJ)
		goto out;

	in_buf = (uint8_t *)__get_free_page(GFP_KERNEL);
	out_buf = (uint8_t *)__get_free_page(GFP_KERNEL);
	if (!in_buf || !out_buf)
		goto out;

	hdr.tzhandle = tzhandle;
	hdr.op = object_op_RELEASE;
	hdr.counts = 0;
	*(struct smcinvoke_msg_hdr *)in_buf = hdr;

	ret = prepare_send_scm_msg(in_buf, SMCINVOKE_TZ_MIN_BUF_SIZE,
			out_buf, SMCINVOKE_TZ_MIN_BUF_SIZE, &smcinvoke_result);
out:
	kfree(filp->private_data);
	free_page((long)in_buf);
	free_page((long)out_buf);

	return ret;
}

static int __init smcinvoke_init(void)
{
	return misc_register(&smcinvoke_miscdev);
}

device_initcall(smcinvoke_init);
MODULE_LICENSE("GPL v2");
