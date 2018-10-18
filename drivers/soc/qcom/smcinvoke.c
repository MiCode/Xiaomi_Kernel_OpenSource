/* Copyright (c) 2016-2018, The Linux Foundation. All rights reserved.
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
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/miscdevice.h>
#include <linux/poll.h>
#include <linux/fdtable.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/anon_inodes.h>
#include <linux/smcinvoke.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/clk.h>
#include <linux/msm-bus.h>
#include <linux/of.h>
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
#define SMCINVOKE_CE_CLK_100MHZ         100000000
#define SMCINVOKE_CE_CLK_DIV		1000000
#define SMCINVOKE_DEINIT_CLK(x) \
	{ if (x) { clk_put(x); x = NULL; } }
#define SMCINVOKE_ENABLE_CLK(x) \
	{ if (x) clk_prepare_enable(x); }
#define SMCINVOKE_DISABLE_CLK(x) \
	{ if (x) clk_disable_unprepare(x); }

#define FOR_ARGS(ndxvar, counts, section)                      \
	for (ndxvar = object_counts_index_##section(counts);     \
		ndxvar < (object_counts_index_##section(counts)  \
		+ object_counts_num_##section(counts));          \
		++ndxvar)

static DEFINE_MUTEX(smcinvoke_lock);
static long smcinvoke_ioctl(struct file *, unsigned , unsigned long);
static int smcinvoke_open(struct inode *, struct file *);
static int smcinvoke_release(struct inode *, struct file *);

static const struct file_operations smcinvoke_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = smcinvoke_ioctl,
	.compat_ioctl = smcinvoke_ioctl,
	.open = smcinvoke_open,
	.release = smcinvoke_release,
};

enum clk_types { CE_CORE_SRC_CLK, CE_CORE_CLK, CE_CLK, CE_BUS_CLK, CE_MAX_CLK };
static const char *clk_names[CE_MAX_CLK] = {
			"core_clk_src", "core_clk", "iface_clk", "bus_clk"
};
enum bandwidth_request_mode {BW_INACTIVE = 0, BW_HIGH};

static dev_t smcinvoke_device_no;
static struct cdev smcinvoke_cdev;
static struct class *driver_class;
static struct device *class_dev;
static struct platform_device *smcinvoke_pdev;
static struct msm_bus_scale_pdata *bus_scale_pdata;
static uint32_t qsee_perf_client;
static bool support_clocks;
static uint32_t ce_opp_freq_hz;
static enum bandwidth_request_mode current_mode;

struct smcinvoke_clk {
	struct clk *clks[CE_MAX_CLK];
	uint32_t clk_access_cnt;
};
static struct smcinvoke_clk g_clk;

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

static void disable_clocks(void)
{
	int i;

	if (g_clk.clk_access_cnt == 0 || --g_clk.clk_access_cnt > 0)
		return;

	for (i = CE_MAX_CLK; i > 0; i--)
		SMCINVOKE_DISABLE_CLK(g_clk.clks[i-1]);
}

static int enable_clocks(void)
{
	int rc = 0, i, j;

	if (g_clk.clk_access_cnt > 0)
		goto out;

	for (i = 0; i < CE_MAX_CLK; i++) {
		if (g_clk.clks[i]) {
			rc = clk_prepare_enable(g_clk.clks[i]);
			if (rc) {
				pr_err("Err %d enabling %s", rc, clk_names[i]);
				break;
			}
		}
	}
	if (rc) {
		for (j = i-1; j >= 0; j--)
			SMCINVOKE_DISABLE_CLK(g_clk.clks[j]);
		return rc;
	}
out:
	g_clk.clk_access_cnt++;
	return rc;
}

static int set_msm_bus_request_locked(enum bandwidth_request_mode mode)
{
	int ret = 0;

	if (support_clocks == 0)
		return ret;

	if (g_clk.clks[CE_CORE_SRC_CLK] == NULL) {
		pr_err("%s clock NULL\n", __func__);
		return ret;
	}

	if (mode == BW_INACTIVE) {
		disable_clocks();
	} else {
		ret = enable_clocks();
		if (ret)
			goto out;
	}

	if (current_mode != mode) {
		ret = msm_bus_scale_client_update_request(
					qsee_perf_client, mode);
		if (ret) {
			pr_err("BW req failed(%d) MODE (%d)\n", ret, mode);
			if (mode == BW_INACTIVE)
				enable_clocks();
			else
				disable_clocks();
			goto out;
		}
		current_mode = mode;
	}
out:
	return ret;
}

static void deinit_clocks(void)
{
	int i;

	for (i = CE_MAX_CLK; i > 0; i--)
		SMCINVOKE_DEINIT_CLK(g_clk.clks[i-1])

	g_clk.clk_access_cnt = 0;
}

static struct clk *get_clk(const char *clk_name)
{
	int rc = 0;
	struct clk *clk = clk_get(class_dev, clk_name);

	if (!IS_ERR(clk)) {
		if (!strcmp(clk_name, clk_names[CE_CORE_SRC_CLK])) {
			rc = clk_set_rate(clk, ce_opp_freq_hz);
			if (rc) {
				SMCINVOKE_DEINIT_CLK(clk);
				pr_err("Err %d setting clk %s to %uMhz\n",
					rc, clk_name,
					ce_opp_freq_hz/SMCINVOKE_CE_CLK_DIV);
			}
		}
	} else {
		pr_warn("Err %d getting clk %s\n", IS_ERR(clk), clk_name);
		clk = NULL;
	}
	return clk;
}

static int init_clocks(void)
{
	int i = 0;
	int rc = -1;

	for (i = 0; i < CE_MAX_CLK; i++) {
		g_clk.clks[i] = get_clk(clk_names[i]);
		if (!g_clk.clks[i])
			goto exit;
	}
	g_clk.clk_access_cnt = 0;
	return 0;
exit:
	for ( ; i >= 0; i--)
		SMCINVOKE_DEINIT_CLK(g_clk.clks[i]);
	return rc;
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

	mutex_lock(&smcinvoke_lock);
	set_msm_bus_request_locked(BW_HIGH);
	mutex_unlock(&smcinvoke_lock);
	ret = scm_call2(SMCINVOKE_TZ_CMD, &desc);

	/* process listener request */
	if (!ret && (desc.ret[0] == QSEOS_RESULT_INCOMPLETE ||
		desc.ret[0] == QSEOS_RESULT_BLOCKED_ON_LISTENER))
		ret = qseecom_process_listener_from_smcinvoke(&desc);

	mutex_lock(&smcinvoke_lock);
	set_msm_bus_request_locked(BW_INACTIVE);
	mutex_unlock(&smcinvoke_lock);

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

		/*
		 * if invoke op results in an err, no need to marshal_out and
		 * copy args buf to user space
		 */
		if (!req.result) {
			ret = marshal_out(in_msg, inmsg_size, &req, args_buf);

			ret |=  copy_to_user(
					(void __user *)(uintptr_t)(req.args),
					args_buf, nr_args * req.argsize);
		}
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

static int smcinvoke_probe(struct platform_device *pdev)
{
	unsigned int baseminor = 0;
	unsigned int count = 1;
	int rc = 0;

	rc = alloc_chrdev_region(&smcinvoke_device_no, baseminor, count,
							SMCINVOKE_FILE);
	if (rc < 0) {
		pr_err("chrdev_region failed %d for %s\n", rc, SMCINVOKE_FILE);
		return rc;
	}
	driver_class = class_create(THIS_MODULE, SMCINVOKE_FILE);
	if (IS_ERR(driver_class)) {
		rc = -ENOMEM;
		pr_err("class_create failed %d\n", rc);
		goto exit_unreg_chrdev_region;
	}
	class_dev = device_create(driver_class, NULL, smcinvoke_device_no,
						NULL, SMCINVOKE_FILE);
	if (!class_dev) {
		pr_err("class_device_create failed %d\n", rc);
		rc = -ENOMEM;
		goto exit_destroy_class;
	}

	cdev_init(&smcinvoke_cdev, &smcinvoke_fops);
	smcinvoke_cdev.owner = THIS_MODULE;

	rc = cdev_add(&smcinvoke_cdev, MKDEV(MAJOR(smcinvoke_device_no), 0),
								count);
	if (rc < 0) {
		pr_err("cdev_add failed %d for %s\n", rc, SMCINVOKE_FILE);
		goto exit_destroy_device;
	}
	smcinvoke_pdev = pdev;
	class_dev->of_node = pdev->dev.of_node;

	if (pdev->dev.of_node) {
		support_clocks =
				of_property_read_bool(pdev->dev.of_node,
						"qcom,clock-support");
		if (of_property_read_u32(pdev->dev.of_node,
						"qcom,ce-opp-freq",
					&ce_opp_freq_hz)) {
			pr_debug("CE op freq not defined, setting to 100MHZ\n");
			ce_opp_freq_hz = SMCINVOKE_CE_CLK_100MHZ;
		}
	}
	if (support_clocks) {
		init_clocks();
		bus_scale_pdata	= msm_bus_cl_get_pdata(pdev);
		if (bus_scale_pdata)
			qsee_perf_client = msm_bus_scale_register_client(
							bus_scale_pdata);
	}
	return  0;

exit_destroy_device:
	device_destroy(driver_class, smcinvoke_device_no);
exit_destroy_class:
	class_destroy(driver_class);
exit_unreg_chrdev_region:
	unregister_chrdev_region(smcinvoke_device_no, count);
	return rc;
}

static int smcinvoke_remove(struct platform_device *pdev)
{
	int count = 1;

	if (support_clocks) {
		/* ok to call with NULL */
		msm_bus_scale_unregister_client(qsee_perf_client);
		if (bus_scale_pdata)
			msm_bus_cl_clear_pdata(bus_scale_pdata);
		deinit_clocks();
		support_clocks = false;
	}
	cdev_del(&smcinvoke_cdev);
	device_destroy(driver_class, smcinvoke_device_no);
	class_destroy(driver_class);
	unregister_chrdev_region(smcinvoke_device_no, count);
	return 0;
}

static int smcinvoke_suspend(struct platform_device *pdev, pm_message_t state)
{
	if (current_mode == BW_HIGH)
		return 1;
	else
		return 0;
}

static int smcinvoke_resume(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id smcinvoke_match[] = {
	{
		.compatible = "qcom,smcinvoke",
	},
	{},
};

static struct platform_driver smcinvoke_plat_driver = {
	.probe = smcinvoke_probe,
	.remove = smcinvoke_remove,
	.suspend = smcinvoke_suspend,
	.resume = smcinvoke_resume,
	.driver = {
		.name = "smcinvoke",
		.owner = THIS_MODULE,
		.of_match_table = smcinvoke_match,
	},
};

static int smcinvoke_init(void)
{
	return platform_driver_register(&smcinvoke_plat_driver);
}

static void smcinvoke_exit(void)
{
	platform_driver_unregister(&smcinvoke_plat_driver);
}

module_init(smcinvoke_init);
module_exit(smcinvoke_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("SMC Invoke driver");
