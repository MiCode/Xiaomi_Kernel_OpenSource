/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#define KMSG_COMPONENT "SMCMOD"
#define pr_fmt(fmt) KMSG_COMPONENT ": " fmt

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/miscdevice.h>
#include <linux/mutex.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/printk.h>
#include <linux/msm_ion.h>
#include <asm/smcmod.h>
#include <asm/cacheflush.h>
#include <mach/scm.h>

static DEFINE_MUTEX(ioctl_lock);

#define SMCMOD_SVC_DEFAULT (0)
#define SMCMOD_SVC_CRYPTO (1)
#define SMCMOD_CRYPTO_CMD_CIPHER (1)
#define SMCMOD_CRYPTO_CMD_MSG_DIGEST_FIXED (2)
#define SMCMOD_CRYPTO_CMD_MSG_DIGEST (3)

/**
 * struct smcmod_cipher_scm_req - structure for sending the cipher cmd to
 * scm_call.
 *
 * @algorithm - specifies cipher algorithm
 * @operation - specifies encryption or decryption.
 * @mode - specifies cipher mode.
 * @key_phys_addr - physical address for key buffer.
 * @key_size - key size in bytes.
 * @plain_text_phys_addr - physical address for plain text buffer.
 * @plain_text_size - size of plain text in bytes.
 * @cipher_text_phys_addr - physical address for cipher text buffer.
 * @cipher_text_size - cipher text size in bytes.
 * @init_vector_phys_addr - physical address for init vector buffer.
 * @init_vector_size - size of initialization vector in bytes.
 */
struct smcmod_cipher_scm_req {
	uint32_t algorithm;
	uint32_t operation;
	uint32_t mode;
	uint32_t key_phys_addr;
	uint32_t key_size;
	uint32_t plain_text_phys_addr;
	uint32_t plain_text_size;
	uint32_t cipher_text_phys_addr;
	uint32_t cipher_text_size;
	uint32_t init_vector_phys_addr;
	uint32_t init_vector_size;
};

/**
 * struct smcmod_msg_digest_scm_req - structure for sending message digest
 * to scm_call.
 *
 * @algorithm - specifies the cipher algorithm.
 * @key_phys_addr - physical address of key buffer.
 * @key_size - hash key size in bytes.
 * @input_phys_addr - physical address of input buffer.
 * @input_size - input data size in bytes.
 * @output_phys_addr - physical address of output buffer.
 * @output_size - size of output buffer in bytes.
 * @verify - indicates whether to verify the hash value.
 */
struct smcmod_msg_digest_scm_req {
	uint32_t algorithm;
	uint32_t key_phys_addr;
	uint32_t key_size;
	uint32_t input_phys_addr;
	uint32_t input_size;
	uint32_t output_phys_addr;
	uint32_t output_size;
	uint8_t verify;
} __packed;

static void smcmod_inv_range(unsigned long start, unsigned long end)
{
	uint32_t cacheline_size;
	uint32_t ctr;

	/* get cache line size */
	asm volatile("mrc p15, 0, %0, c0, c0, 1" : "=r" (ctr));
	cacheline_size =  4 << ((ctr >> 16) & 0xf);

	/* invalidate the range */
	start = round_down(start, cacheline_size);
	end = round_up(end, cacheline_size);
	while (start < end) {
		asm ("mcr p15, 0, %0, c7, c6, 1" : : "r" (start)
			 : "memory");
		start += cacheline_size;
	}
	mb();
	isb();
}

/*
 * FIXME:
 * scm_call will no longer flush the entire cache before calling into the secure
 * world. Until individual buffers in this driver can be flushed, flush the
 * entire cache before calling into scm_call.
 */
int smcmod_scm_call(u32 svc_id, u32 cmd_id, const void *cmd_buf, size_t cmd_len,
		void *resp_buf, size_t resp_len)
{
	flush_cache_all();
	outer_flush_all();
	return scm_call(svc_id, cmd_id, cmd_buf, cmd_len, resp_buf, resp_len);
}

static int smcmod_ion_fd_to_phys(int32_t fd, struct ion_client *ion_clientp,
	struct ion_handle **ion_handlep, uint32_t *phys_addrp, size_t *sizep)
{
	int ret = 0;

	/* sanity check args */
	if ((fd < 0) || IS_ERR_OR_NULL(ion_clientp) ||
		IS_ERR_OR_NULL(ion_handlep) || IS_ERR_OR_NULL(phys_addrp) ||
		IS_ERR_OR_NULL(sizep))
		return -EINVAL;

	/* import the buffer fd */
	*ion_handlep = ion_import_dma_buf(ion_clientp, fd);

	/* sanity check the handle */
	if (IS_ERR_OR_NULL(*ion_handlep))
		return -EINVAL;

	/* get the physical address */
	ret = ion_phys(ion_clientp, *ion_handlep, (ion_phys_addr_t *)phys_addrp,
		sizep);

	return ret;
}

static int smcmod_send_buf_cmd(struct smcmod_buf_req *reqp)
{
	int ret = 0;
	struct ion_client *ion_clientp = NULL;
	struct ion_handle *ion_cmd_handlep = NULL;
	struct ion_handle *ion_resp_handlep = NULL;
	void *cmd_vaddrp = NULL;
	void *resp_vaddrp = NULL;
	unsigned long cmd_buf_size = 0;
	unsigned long resp_buf_size = 0;

	/* sanity check the argument */
	if (IS_ERR_OR_NULL(reqp))
		return -EINVAL;

	/* sanity check the fds */
	if (reqp->ion_cmd_fd < 0)
		return -EINVAL;

	/* create an ion client */
	ion_clientp = msm_ion_client_create(UINT_MAX, "smcmod");

	/* check for errors */
	if (IS_ERR_OR_NULL(ion_clientp))
		return -EINVAL;

	/* import the command buffer fd */
	ion_cmd_handlep = ion_import_dma_buf(ion_clientp, reqp->ion_cmd_fd);

	/* sanity check the handle */
	if (IS_ERR_OR_NULL(ion_cmd_handlep)) {
		ret = -EINVAL;
		goto buf_cleanup;
	}

	/* retrieve the size of the buffer */
	if (ion_handle_get_size(ion_clientp, ion_cmd_handlep,
		&cmd_buf_size) < 0) {
		ret = -EINVAL;
		goto buf_cleanup;
	}

	/* ensure that the command buffer size is not
	 * greater than the size of the buffer.
	 */
	if (reqp->cmd_len > cmd_buf_size) {
		ret = -EINVAL;
		goto buf_cleanup;
	}

	/* map the area to get a virtual address */
	cmd_vaddrp = ion_map_kernel(ion_clientp, ion_cmd_handlep);

	/* sanity check the address */
	if (IS_ERR_OR_NULL(cmd_vaddrp)) {
		ret = -EINVAL;
		goto buf_cleanup;
	}

	/* check if there is a response buffer */
	if (reqp->ion_resp_fd >= 0) {
		/* import the handle */
		ion_resp_handlep =
			ion_import_dma_buf(ion_clientp, reqp->ion_resp_fd);

		/* sanity check the handle */
		if (IS_ERR_OR_NULL(ion_resp_handlep)) {
			ret = -EINVAL;
			goto buf_cleanup;
		}

		/* retrieve the size of the buffer */
		if (ion_handle_get_size(ion_clientp, ion_resp_handlep,
			&resp_buf_size) < 0) {
			ret = -EINVAL;
			goto buf_cleanup;
		}

		/* ensure that the command buffer size is not
		 * greater than the size of the buffer.
		 */
		if (reqp->resp_len > resp_buf_size) {
			ret = -EINVAL;
			goto buf_cleanup;
		}

		/* map the area to get a virtual address */
		resp_vaddrp = ion_map_kernel(ion_clientp, ion_resp_handlep);

		/* sanity check the address */
		if (IS_ERR_OR_NULL(resp_vaddrp)) {
			ret = -EINVAL;
			goto buf_cleanup;
		}
	}

	/* call scm function to switch to secure world */
	reqp->return_val = smcmod_scm_call(reqp->service_id, reqp->command_id,
		cmd_vaddrp, reqp->cmd_len, resp_vaddrp, reqp->resp_len);

buf_cleanup:
	/* if the client and handle(s) are valid, free them */
	if (!IS_ERR_OR_NULL(ion_clientp)) {
		if (!IS_ERR_OR_NULL(ion_cmd_handlep)) {
			if (!IS_ERR_OR_NULL(cmd_vaddrp))
				ion_unmap_kernel(ion_clientp, ion_cmd_handlep);
			ion_free(ion_clientp, ion_cmd_handlep);
		}

		if (!IS_ERR_OR_NULL(ion_resp_handlep)) {
			if (!IS_ERR_OR_NULL(resp_vaddrp))
				ion_unmap_kernel(ion_clientp, ion_resp_handlep);
			ion_free(ion_clientp, ion_resp_handlep);
		}

		ion_client_destroy(ion_clientp);
	}

	return ret;
}

static int smcmod_send_cipher_cmd(struct smcmod_cipher_req *reqp)
{
	int ret = 0;
	struct smcmod_cipher_scm_req scm_req;
	struct ion_client *ion_clientp = NULL;
	struct ion_handle *ion_key_handlep = NULL;
	struct ion_handle *ion_plain_handlep = NULL;
	struct ion_handle *ion_cipher_handlep = NULL;
	struct ion_handle *ion_iv_handlep = NULL;
	size_t size = 0;

	if (IS_ERR_OR_NULL(reqp))
		return -EINVAL;

	/* sanity check the fds */
	if ((reqp->ion_plain_text_fd < 0) ||
		(reqp->ion_cipher_text_fd < 0) ||
		(reqp->ion_init_vector_fd < 0))
		return -EINVAL;

	/* create an ion client */
	ion_clientp = msm_ion_client_create(UINT_MAX, "smcmod");

	/* check for errors */
	if (IS_ERR_OR_NULL(ion_clientp))
		return -EINVAL;

	/* fill in the scm request structure */
	scm_req.algorithm = reqp->algorithm;
	scm_req.operation = reqp->operation;
	scm_req.mode = reqp->mode;
	scm_req.key_phys_addr = 0;
	scm_req.key_size = reqp->key_size;
	scm_req.plain_text_size = reqp->plain_text_size;
	scm_req.cipher_text_size = reqp->cipher_text_size;
	scm_req.init_vector_size = reqp->init_vector_size;

	if (!reqp->key_is_null) {
		/* import the key buffer and get the physical address */
		ret = smcmod_ion_fd_to_phys(reqp->ion_key_fd, ion_clientp,
			&ion_key_handlep, &scm_req.key_phys_addr, &size);
		if (ret < 0)
			goto buf_cleanup;

		/* ensure that the key size is not
		 * greater than the size of the buffer.
		 */
		if (reqp->key_size > size) {
			ret = -EINVAL;
			goto buf_cleanup;
		}
	}

	/* import the plain text buffer and get the physical address */
	ret = smcmod_ion_fd_to_phys(reqp->ion_plain_text_fd, ion_clientp,
		&ion_plain_handlep, &scm_req.plain_text_phys_addr, &size);

	if (ret < 0)
		goto buf_cleanup;

	/* ensure that the plain text size is not
	 * greater than the size of the buffer.
	 */
	if (reqp->plain_text_size > size) {
		ret = -EINVAL;
		goto buf_cleanup;
	}

	/* import the cipher text buffer and get the physical address */
	ret = smcmod_ion_fd_to_phys(reqp->ion_cipher_text_fd, ion_clientp,
		&ion_cipher_handlep, &scm_req.cipher_text_phys_addr, &size);
	if (ret < 0)
		goto buf_cleanup;

	/* ensure that the cipher text size is not
	 * greater than the size of the buffer.
	 */
	if (reqp->cipher_text_size > size) {
		ret = -EINVAL;
		goto buf_cleanup;
	}

	/* import the init vector buffer and get the physical address */
	ret = smcmod_ion_fd_to_phys(reqp->ion_init_vector_fd, ion_clientp,
		&ion_iv_handlep, &scm_req.init_vector_phys_addr, &size);
	if (ret < 0)
		goto buf_cleanup;

	/* ensure that the init vector size is not
	 * greater than the size of the buffer.
	 */
	if (reqp->init_vector_size > size) {
		ret = -EINVAL;
		goto buf_cleanup;
	}

	/* call scm function to switch to secure world */
	reqp->return_val = smcmod_scm_call(SMCMOD_SVC_CRYPTO,
		SMCMOD_CRYPTO_CMD_CIPHER, &scm_req,
		sizeof(scm_req), NULL, 0);

	/* for decrypt, plain text is the output, otherwise it's cipher text */
	if (reqp->operation) {
		void *vaddrp = NULL;

		/* map the plain text region to get the virtual address */
		vaddrp = ion_map_kernel(ion_clientp, ion_plain_handlep);
		if (IS_ERR_OR_NULL(vaddrp)) {
			ret = -EINVAL;
			goto buf_cleanup;
		}

		/* invalidate the range */
		smcmod_inv_range((unsigned long)vaddrp,
			(unsigned long)(vaddrp + scm_req.plain_text_size));

		/* unmap the mapped area */
		ion_unmap_kernel(ion_clientp, ion_plain_handlep);
	} else {
		void *vaddrp = NULL;

		/* map the cipher text region to get the virtual address */
		vaddrp = ion_map_kernel(ion_clientp, ion_cipher_handlep);
		if (IS_ERR_OR_NULL(vaddrp)) {
			ret = -EINVAL;
			goto buf_cleanup;
		}

		/* invalidate the range */
		smcmod_inv_range((unsigned long)vaddrp,
			(unsigned long)(vaddrp + scm_req.cipher_text_size));

		/* unmap the mapped area */
		ion_unmap_kernel(ion_clientp, ion_cipher_handlep);
	}

buf_cleanup:
	/* if the client and handles are valid, free them */
	if (!IS_ERR_OR_NULL(ion_clientp)) {
		if (!IS_ERR_OR_NULL(ion_key_handlep))
			ion_free(ion_clientp, ion_key_handlep);

		if (!IS_ERR_OR_NULL(ion_plain_handlep))
			ion_free(ion_clientp, ion_plain_handlep);

		if (!IS_ERR_OR_NULL(ion_cipher_handlep))
			ion_free(ion_clientp, ion_cipher_handlep);

		if (!IS_ERR_OR_NULL(ion_iv_handlep))
			ion_free(ion_clientp, ion_iv_handlep);

		ion_client_destroy(ion_clientp);
	}

	return ret;
}
static int smcmod_send_msg_digest_cmd(struct smcmod_msg_digest_req *reqp)
{
	int ret = 0;
	struct smcmod_msg_digest_scm_req scm_req;
	struct ion_client *ion_clientp = NULL;
	struct ion_handle *ion_key_handlep = NULL;
	struct ion_handle *ion_input_handlep = NULL;
	struct ion_handle *ion_output_handlep = NULL;
	size_t size = 0;
	void *vaddrp = NULL;

	if (IS_ERR_OR_NULL(reqp))
		return -EINVAL;

	/* sanity check the fds */
	if ((reqp->ion_input_fd < 0) || (reqp->ion_output_fd < 0))
		return -EINVAL;

	/* create an ion client */
	ion_clientp = msm_ion_client_create(UINT_MAX, "smcmod");

	/* check for errors */
	if (IS_ERR_OR_NULL(ion_clientp))
		return -EINVAL;

	/* fill in the scm request structure */
	scm_req.algorithm = reqp->algorithm;
	scm_req.key_phys_addr = 0;
	scm_req.key_size = reqp->key_size;
	scm_req.input_size = reqp->input_size;
	scm_req.output_size = reqp->output_size;
	scm_req.verify = 0;

	if (!reqp->key_is_null) {
		/* import the key buffer and get the physical address */
		ret = smcmod_ion_fd_to_phys(reqp->ion_key_fd, ion_clientp,
			&ion_key_handlep, &scm_req.key_phys_addr, &size);
		if (ret < 0)
			goto buf_cleanup;

		/* ensure that the key size is not
		 * greater than the size of the buffer.
		 */
		if (reqp->key_size > size) {
			ret = -EINVAL;
			goto buf_cleanup;
		}
	}

	/* import the input buffer and get the physical address */
	ret = smcmod_ion_fd_to_phys(reqp->ion_input_fd, ion_clientp,
		&ion_input_handlep, &scm_req.input_phys_addr, &size);
	if (ret < 0)
		goto buf_cleanup;

	/* ensure that the input size is not
	 * greater than the size of the buffer.
	 */
	if (reqp->input_size > size) {
		ret = -EINVAL;
		goto buf_cleanup;
	}

	/* import the output buffer and get the physical address */
	ret = smcmod_ion_fd_to_phys(reqp->ion_output_fd, ion_clientp,
		&ion_output_handlep, &scm_req.output_phys_addr, &size);
	if (ret < 0)
		goto buf_cleanup;

	/* ensure that the output size is not
	 * greater than the size of the buffer.
	 */
	if (reqp->output_size > size) {
		ret = -EINVAL;
		goto buf_cleanup;
	}

	/* call scm function to switch to secure world */
	if (reqp->fixed_block)
		reqp->return_val = smcmod_scm_call(SMCMOD_SVC_CRYPTO,
			SMCMOD_CRYPTO_CMD_MSG_DIGEST_FIXED,
			&scm_req,
			sizeof(scm_req),
			NULL, 0);
	else
		reqp->return_val = smcmod_scm_call(SMCMOD_SVC_CRYPTO,
			SMCMOD_CRYPTO_CMD_MSG_DIGEST,
			&scm_req,
			sizeof(scm_req),
			NULL, 0);


	/* map the output region to get the virtual address */
	vaddrp = ion_map_kernel(ion_clientp, ion_output_handlep);
	if (IS_ERR_OR_NULL(vaddrp)) {
		ret = -EINVAL;
		goto buf_cleanup;
	}

	/* invalidate the range */
	smcmod_inv_range((unsigned long)vaddrp,
		(unsigned long)(vaddrp + scm_req.output_size));

	/* unmap the mapped area */
	ion_unmap_kernel(ion_clientp, ion_output_handlep);

buf_cleanup:
	/* if the client and handles are valid, free them */
	if (!IS_ERR_OR_NULL(ion_clientp)) {
		if (!IS_ERR_OR_NULL(ion_key_handlep))
			ion_free(ion_clientp, ion_key_handlep);

		if (!IS_ERR_OR_NULL(ion_input_handlep))
			ion_free(ion_clientp, ion_input_handlep);

		if (!IS_ERR_OR_NULL(ion_output_handlep))
			ion_free(ion_clientp, ion_output_handlep);

		ion_client_destroy(ion_clientp);
	}

	return ret;
}

static long smcmod_ioctl(struct file *file, unsigned cmd, unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	int ret = 0;

	/* sanity check */
	if (!argp)
		return -EINVAL;

	/*
	 * The SMC instruction should only be initiated by one process
	 * at a time, hence the critical section here. Note that this
	 * does not prevent user space from modifying the
	 * allocated buffer contents.  Extra steps are needed to
	 * prevent that from happening.
	 */
	mutex_lock(&ioctl_lock);

	switch (cmd) {
	case SMCMOD_IOCTL_SEND_REG_CMD:
		{
			struct smcmod_reg_req req;

			/* copy struct from user */
			if (copy_from_user((void *)&req, argp, sizeof(req))) {
				ret = -EFAULT;
				goto cleanup;
			}

			/* call the correct scm function to switch to secure
			 * world
			 */
			if (req.num_args == 1) {
				req.return_val =
					scm_call_atomic1(req.service_id,
					req.command_id, req.args[0]);
			} else if (req.num_args == 2) {
				req.return_val =
					scm_call_atomic2(req.service_id,
					req.command_id, req.args[0],
					req.args[1]);
			} else {
				ret = -EINVAL;
				goto cleanup;
			}

			/* copy result back to user */
			if (copy_to_user(argp, (void *)&req, sizeof(req))) {
				ret = -EFAULT;
				goto cleanup;
			}
		}
		break;

	/* This is an example of how to pass buffers to/from the secure
	 * side using the ion driver.
	 */
	case SMCMOD_IOCTL_SEND_BUF_CMD:
		{
			struct smcmod_buf_req req;

			/* copy struct from user */
			if (copy_from_user((void *)&req, argp, sizeof(req))) {
				ret = -EFAULT;
				goto cleanup;
			}

			/* send the command */
			ret = smcmod_send_buf_cmd(&req);
			if (ret < 0)
				goto cleanup;

			/* copy result back to user */
			if (copy_to_user(argp, (void *)&req, sizeof(req))) {
				ret = -EFAULT;
				goto cleanup;
			}
		}
		break;

	case SMCMOD_IOCTL_SEND_CIPHER_CMD:
		{
			struct smcmod_cipher_req req;

			/* copy struct from user */
			if (copy_from_user((void *)&req, argp, sizeof(req))) {
				ret = -EFAULT;
				goto cleanup;
			}

			ret = smcmod_send_cipher_cmd(&req);
			if (ret < 0)
				goto cleanup;

			/* copy result back to user */
			if (copy_to_user(argp, (void *)&req, sizeof(req))) {
				ret = -EFAULT;
				goto cleanup;
			}
		}
		break;

	case SMCMOD_IOCTL_SEND_MSG_DIGEST_CMD:
		{
			struct smcmod_msg_digest_req req;

			/* copy struct from user */
			if (copy_from_user((void *)&req, argp, sizeof(req))) {
				ret = -EFAULT;
				goto cleanup;
			}

			ret = smcmod_send_msg_digest_cmd(&req);
			if (ret < 0)
				goto cleanup;

			/* copy result back to user */
			if (copy_to_user(argp, (void *)&req, sizeof(req))) {
				ret = -EFAULT;
				goto cleanup;
			}
		}
		break;

	case SMCMOD_IOCTL_GET_VERSION:
		{
			uint32_t req;

			/* call scm function to switch to secure world */
			req = scm_get_version();

			/* copy result back to user */
			if (copy_to_user(argp, (void *)&req, sizeof(req))) {
				ret = -EFAULT;
				goto cleanup;
			}
		}
		break;

	default:
		ret = -EINVAL;
	}

cleanup:
	mutex_unlock(&ioctl_lock);
	return ret;
}

static int smcmod_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int smcmod_release(struct inode *inode, struct file *file)
{
	return 0;
}

static const struct file_operations smcmod_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = smcmod_ioctl,
	.open = smcmod_open,
	.release = smcmod_release,
};

static struct miscdevice smcmod_misc_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = SMCMOD_DEV,
	.fops = &smcmod_fops
};

static int __init smcmod_init(void)
{
	return misc_register(&smcmod_misc_dev);
}

static void __exit smcmod_exit(void)
{
	misc_deregister(&smcmod_misc_dev);
}

MODULE_DESCRIPTION("Qualcomm SMC Module");
MODULE_LICENSE("GPL v2");

module_init(smcmod_init);
module_exit(smcmod_exit);
