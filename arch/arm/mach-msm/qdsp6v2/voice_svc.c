/* Copyright (c) 2014, The Linux Foundation. All rights reserved.
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

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/cdev.h>
#include <sound/voice_svc.h>
#include <mach/qdsp6v2/apr_tal.h>
#include <mach/qdsp6v2/apr.h>

#define DRIVER_NAME "voice_svc"
#define MINOR_NUMBER 1
#define APR_MAX_RESPONSE 10

#define MAX(a, b) ((a) >= (b) ? (a) : (b))

struct voice_svc_device {
	struct cdev *cdev;
	struct device *dev;
	int major;
};

struct voice_svc_prvt {
	void* apr_q6_mvm;
	void* apr_q6_cvs;
	uint16_t response_count;
	struct list_head response_queue;
	wait_queue_head_t response_wait;
	spinlock_t response_lock;
};

struct apr_data {
	struct apr_hdr hdr;
	__u8 payload[0];
} __packed;

struct apr_response_list {
	struct list_head list;
	struct voice_svc_cmd_response resp;
};

static struct voice_svc_device *voice_svc_dev;
static struct class *voice_svc_class;
static bool reg_dummy_sess;
static void *dummy_q6_mvm;
static void *dummy_q6_cvs;
dev_t device_num;

static int voice_svc_dummy_reg(void);
static int32_t qdsp_dummy_apr_callback(struct apr_client_data *data,
					void *priv);

static int32_t qdsp_apr_callback(struct apr_client_data *data, void *priv)
{
	struct voice_svc_prvt *prtd;
	struct apr_response_list *response_list;
	unsigned long spin_flags;

	if ((data == NULL) || (priv == NULL)) {
		pr_err("%s: data or priv is NULL\n", __func__);
		return -EINVAL;
	}

	prtd = (struct voice_svc_prvt*)priv;

	pr_debug("%s: data->opcode %x\n", __func__,
		 data->opcode);

	if (data->opcode == RESET_EVENTS) {
		if (data->reset_proc == APR_DEST_QDSP6) {
			pr_debug("%s: Received reset event\n", __func__);

			if (prtd->apr_q6_mvm != NULL) {
				apr_reset(prtd->apr_q6_mvm);
				prtd->apr_q6_mvm = NULL;
			}

			if (prtd->apr_q6_cvs != NULL) {
				apr_reset(prtd->apr_q6_cvs);
				prtd->apr_q6_cvs = NULL;
			}
		} else if (data->reset_proc ==APR_DEST_MODEM) {
			pr_debug("%s: Received Modem reset event\n", __func__);
		}
	}

	spin_lock_irqsave(&prtd->response_lock, spin_flags);

	if (prtd->response_count < APR_MAX_RESPONSE) {
		response_list = (struct apr_response_list *)kmalloc(
			sizeof(struct apr_response_list) + data->payload_size,
			GFP_ATOMIC);
		if (response_list == NULL) {
			pr_err("%s: kmalloc failed\n", __func__);

			return -ENOMEM;
		}

		response_list->resp.src_port = data->src_port;
		response_list->resp.dest_port = ((data->dest_port) >> 8);
		response_list->resp.token = data->token;
		response_list->resp.opcode = data->opcode;
		response_list->resp.payload_size = data->payload_size;
		if (data->payload != NULL && data->payload_size > 0) {
			memcpy(response_list->resp.payload, data->payload,
				data->payload_size);
		}

		list_add_tail(&response_list->list, &prtd->response_queue);
		prtd->response_count++;

		wake_up(&prtd->response_wait);
	} else {
		pr_err("%s: Response dropped since the queue is full\n", __func__);
	}

	spin_unlock_irqrestore(&prtd->response_lock, spin_flags);

	return 0;
}

static int32_t qdsp_dummy_apr_callback(struct apr_client_data *data, void *priv)
{
	/* Do Nothing */
	return 0;
}

static void voice_svc_update_hdr(struct voice_svc_cmd_request* apr_req_data,
			    struct apr_data *aprdata,
			    struct voice_svc_prvt *prtd)
{

	aprdata->hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD, \
				       APR_HDR_LEN(sizeof(struct apr_hdr)),\
				       APR_PKT_VER);
	aprdata->hdr.src_port = ((apr_req_data->src_port) << 8 | 0x0001);
	aprdata->hdr.dest_port = apr_req_data->dest_port;
	aprdata->hdr.token = apr_req_data->token;
	aprdata->hdr.opcode = apr_req_data->opcode;
	aprdata->hdr.pkt_size  = APR_PKT_SIZE(APR_HDR_SIZE,
					apr_req_data->payload_size);
	memcpy(aprdata->payload, apr_req_data->payload,
	       apr_req_data->payload_size);
}

static int voice_svc_send_req(struct voice_svc_cmd_request *apr_request,
			      struct voice_svc_prvt *prtd)
{
	int ret = 0;
	void *apr_handle = NULL;
	struct apr_data *aprdata = NULL;
	uint32_t user_payload_size = 0;

	if (apr_request == NULL) {
		pr_err("%s: apr_request is NULL\n", __func__);

		ret = -EINVAL;
		goto done;
	}

	user_payload_size = apr_request->payload_size;

	aprdata = kmalloc(sizeof(struct apr_data) + user_payload_size,
			  GFP_KERNEL);

	if (aprdata == NULL) {
		pr_err("%s: aprdata kmalloc failed.", __func__);

		ret = -ENOMEM;
		goto done;
	}

	voice_svc_update_hdr(apr_request, aprdata, prtd);

	if (!strncmp(apr_request->svc_name, VOICE_SVC_CVS_STR,
	    MAX(sizeof(apr_request->svc_name), sizeof(VOICE_SVC_CVS_STR)))) {
		apr_handle = prtd->apr_q6_cvs;
	} else if (!strncmp(apr_request->svc_name, VOICE_SVC_MVM_STR,
	    MAX(sizeof(apr_request->svc_name), sizeof(VOICE_SVC_MVM_STR)))) {
		apr_handle = prtd->apr_q6_mvm;
	} else {
		pr_err("%s: Invalid service %s\n", __func__,
			apr_request->svc_name);

		ret = -EINVAL;
		goto done;
	}

	ret = apr_send_pkt(apr_handle, (uint32_t *)aprdata);

	if (ret < 0) {
		pr_err("%s: Fail in sending SNDRV_VOICE_SVC_REQUEST\n",
			__func__);
		ret = -EINVAL;
	} else {
		pr_debug("%s: apr packet sent successfully %d\n",
				__func__, ret);
		ret = 0;
	}

done:
	if (aprdata != NULL)
		kfree(aprdata);

	return ret;
}
static int voice_svc_reg(char *svc, uint32_t src_port,
			 struct voice_svc_prvt *prtd, void **handle)
{
	int ret = 0;

	if (handle == NULL) {
		pr_err("%s: handle is NULL\n", __func__);
		ret = -EINVAL;
		goto done;
	}

	if (*handle != NULL) {
		pr_err("%s: svc handle not NULL\n", __func__);
		ret = -EINVAL;
		goto done;
	}

	if (src_port == (APR_MAX_PORTS - 1)) {
		pr_err("%s: SRC port reserved for dummy session\n", __func__);
		pr_err("%s: Unable to register %s\n", __func__, svc);
		ret = -EINVAL;
		goto done;
	}

	*handle = apr_register("ADSP",
		svc, qdsp_apr_callback,
		((src_port) << 8 | 0x0001),
		prtd);

	if (*handle == NULL) {
		pr_err("%s: Unable to register %s\n",
		__func__, svc);

		ret = -EFAULT;
		goto done;
	}
	pr_debug("%s: register %s successful\n",
		__func__, svc);
done:
	return ret;
}

static int voice_svc_dereg(char *svc, void **handle)
{
	int ret = 0;
	if (handle == NULL) {
		pr_err("%s: handle is NULL\n", __func__);
		ret = -EINVAL;
		goto done;
	}

	apr_deregister(*handle);
	*handle = NULL;
	pr_debug("%s: deregister %s successful\n",
		__func__, svc);

done:
	return 0;
}

static int process_reg_cmd(struct voice_svc_register apr_reg_svc,
			     struct voice_svc_prvt *prtd)
{
	int ret = 0;
	char *svc = NULL;
	void **handle = NULL;

	if (!strncmp(apr_reg_svc.svc_name, VOICE_SVC_MVM_STR,
	    MAX(sizeof(apr_reg_svc.svc_name), sizeof(VOICE_SVC_MVM_STR)))) {
		svc = VOICE_SVC_MVM_STR;
		handle = &prtd->apr_q6_mvm;
	} else if (!strncmp(apr_reg_svc.svc_name, VOICE_SVC_CVS_STR,
            MAX(sizeof(apr_reg_svc.svc_name), sizeof(VOICE_SVC_CVS_STR)))) {
		svc = VOICE_SVC_CVS_STR;
		handle = &prtd->apr_q6_cvs;
	} else {
		pr_err("%s: Invalid Service: %s\n", __func__,
				apr_reg_svc.svc_name);
		ret = -EINVAL;
		goto done;
	}

	if (*handle == NULL &&
	    apr_reg_svc.reg_flag) {
		ret = voice_svc_reg(svc, apr_reg_svc.src_port, prtd,
				    handle);
	} else if (handle != NULL &&
		   !apr_reg_svc.reg_flag) {
		ret = voice_svc_dereg(svc, handle);
	}

done:
	return ret;
}

static long voice_svc_ioctl(struct file *file, unsigned int cmd,
			    unsigned long u_arg)
{
	int ret = 0;
	struct voice_svc_prvt *prtd;
	struct voice_svc_register apr_reg_svc;
	struct voice_svc_cmd_request *apr_request = NULL;
	struct voice_svc_cmd_response *apr_response = NULL;
	struct apr_response_list *resp;
	void __user *arg = (void __user *)u_arg;
	uint32_t user_payload_size = 0;
	unsigned long spin_flags;

	pr_debug("%s: cmd: %u\n", __func__, cmd);

	prtd = (struct voice_svc_prvt*)file->private_data;

	switch (cmd) {
	case SNDRV_VOICE_SVC_REGISTER_SVC:
		pr_debug("%s: size of struct: %d\n", __func__,
				sizeof(apr_reg_svc));
		if (copy_from_user(&apr_reg_svc, arg, sizeof(apr_reg_svc))) {
			pr_err("%s: copy_from_user failed\n", __func__);

			ret = -EFAULT;
			goto done;
		}

		ret = process_reg_cmd(apr_reg_svc, prtd);

		break;
	case SNDRV_VOICE_SVC_CMD_REQUEST:
		if (!access_ok(VERIFY_READ, arg,
				sizeof(struct voice_svc_cmd_request))) {
			pr_err("%s: Unable to read user data", __func__);

			ret = -EFAULT;
			goto done;
		}

		user_payload_size =
			((struct voice_svc_cmd_request*)arg)->payload_size;

		apr_request = kmalloc(sizeof(struct voice_svc_cmd_request) +
				      user_payload_size, GFP_KERNEL);

		if (apr_request == NULL) {
			pr_err("%s: apr_request kmalloc failed.", __func__);

			ret = -ENOMEM;
			goto done;
		}

		if (copy_from_user(apr_request, arg,
				sizeof(struct voice_svc_cmd_request) +
				user_payload_size)) {
			pr_err("%s: copy from user failed, size %d\n", __func__,
				sizeof(struct voice_svc_cmd_request) +
				user_payload_size);

			ret = -EFAULT;
			goto done;
		}

		ret = voice_svc_send_req(apr_request, prtd);

		break;

	case SNDRV_VOICE_SVC_CMD_RESPONSE:
		do {
			if (!access_ok(VERIFY_READ, arg,
				sizeof(struct voice_svc_cmd_response))) {
				pr_err("%s: Unable to read user data",
				       __func__);

				ret = -EFAULT;
				goto done;
			}

			user_payload_size =
			    ((struct voice_svc_cmd_response*)arg)->payload_size;
			pr_debug("%s: RESPONSE: user payload size %d",
				 __func__, user_payload_size);

			spin_lock_irqsave(&prtd->response_lock, spin_flags);
			if (!list_empty(&prtd->response_queue)) {
				resp = list_first_entry(&prtd->response_queue,
						struct apr_response_list, list);

				if (user_payload_size <
					resp->resp.payload_size) {
					pr_err("%s: Invalid payload size %d,%d",
					       __func__, user_payload_size,
					       resp->resp.payload_size);
					ret = -ENOMEM;
					spin_unlock_irqrestore(
						&prtd->response_lock,
						spin_flags);
					goto done;
				}

				if (!access_ok(VERIFY_WRITE, arg,
					sizeof(struct voice_svc_cmd_response) +
					resp->resp.payload_size)) {
					ret = -EFAULT;
					spin_unlock_irqrestore(
						&prtd->response_lock,
						spin_flags);
					goto done;
				}

				if (copy_to_user(arg, &resp->resp,
					sizeof(struct voice_svc_cmd_response) +
					resp->resp.payload_size)) {
					pr_err("%s: copy to user failed, size \
						%d\n", __func__,
					sizeof(struct voice_svc_cmd_response) +
						resp->resp.payload_size);

					ret = -EFAULT;
					spin_unlock_irqrestore(
						&prtd->response_lock,
						spin_flags);
					goto done;
				}

				prtd->response_count--;

				list_del(&resp->list);
				kfree(resp);
				spin_unlock_irqrestore(&prtd->response_lock,
							spin_flags);
				goto done;
			} else {
				spin_unlock_irqrestore(&prtd->response_lock,
							spin_flags);
				wait_event_interruptible(prtd->response_wait,
					!list_empty(&prtd->response_queue));
				pr_debug("%s: Interupt recieved for response",
					 __func__);
			}
		} while(!apr_response);
		break;
	default:
		pr_debug("%s: cmd: %u\n", __func__, cmd);
		ret = -EINVAL;
	}

done:
	if (apr_request != NULL)
		kfree(apr_request);

	return ret;
}

static int voice_svc_dummy_reg()
{
	uint32_t src_port = APR_MAX_PORTS - 1;

	pr_debug("%s\n", __func__);
	dummy_q6_mvm = apr_register("ADSP", "MVM",
				qdsp_dummy_apr_callback,
				src_port,
				NULL);
	if (dummy_q6_mvm == NULL) {
		pr_err("%s: Unable to register dummy MVM\n", __func__);
		goto err;
	}

	dummy_q6_cvs = apr_register("ADSP", "CVS",
				qdsp_dummy_apr_callback,
				src_port,
				NULL);
	if (dummy_q6_cvs == NULL) {
		pr_err("%s: Unable to register dummy CVS\n", __func__);
		goto err;
	}
	return 0;
err:
	if (dummy_q6_mvm != NULL) {
		apr_deregister(dummy_q6_mvm);
		dummy_q6_mvm = NULL;
	}
	return -EINVAL;
}

static int voice_svc_open(struct inode *inode, struct file *file)
{
	struct voice_svc_prvt *prtd = NULL;

	prtd = kmalloc(sizeof(struct voice_svc_prvt), GFP_KERNEL);

	if (prtd == NULL) {
		pr_err("%s: kmalloc failed", __func__);

		return -ENOMEM;
	}

	memset(prtd, 0, sizeof(struct voice_svc_prvt));
	prtd->apr_q6_cvs = NULL;
	prtd->apr_q6_mvm = NULL;
	prtd->response_count = 0;

	INIT_LIST_HEAD(&prtd->response_queue);
	init_waitqueue_head(&prtd->response_wait);
	spin_lock_init(&prtd->response_lock);

	file->private_data = (void*)prtd;

	/* Current APR implementation doesn't support session based
	 * multiple service registrations. The apr_deregister()
	 * function sets the destination and client IDs to zero, if
	 * deregister is called for a single service instance.
	 * To avoid this, register for additional services.
	 */
	if (!reg_dummy_sess) {
		voice_svc_dummy_reg();
		reg_dummy_sess = 1;
	}
	return 0;
}

static int voice_svc_release(struct inode *inode, struct file *file)
{
	kfree(file->private_data);
	return 0;
}

static const struct file_operations voice_svc_fops = {
	.owner =                THIS_MODULE,
	.open =                 voice_svc_open,
	.unlocked_ioctl =       voice_svc_ioctl,
	.release =              voice_svc_release,
};


static int voice_svc_probe(struct platform_device *pdev)
{
	int ret = 0;

	voice_svc_dev = devm_kzalloc(&pdev->dev, sizeof(struct voice_svc_device),
			GFP_KERNEL);
	if (!voice_svc_dev) {
		pr_err("%s: kzalloc failed\n", __func__);
		ret = -ENOMEM;
		goto done;
	}

	ret = alloc_chrdev_region(&device_num, 0, MINOR_NUMBER, DRIVER_NAME);
	if (ret) {
		pr_err("%s: Failed to alloc chrdev\n", __func__);
		ret = -ENODEV;
		goto done;
	}

	voice_svc_dev->major = MAJOR(device_num);
	voice_svc_class = class_create(THIS_MODULE, DRIVER_NAME);
	if (IS_ERR(voice_svc_class)) {
		ret = PTR_ERR(voice_svc_class);
		pr_err("%s: Failed to create class; err = %d\n", __func__,
			ret);
		goto class_err;
	}

	voice_svc_dev->dev = device_create(voice_svc_class, NULL, device_num,
					   NULL, DRIVER_NAME);
	if (IS_ERR(voice_svc_dev->dev)) {
		ret = PTR_ERR(voice_svc_dev->dev);
		pr_err("%s: Failed to create device; err = %d\n", __func__,
			ret);
		goto dev_err;
	}

	voice_svc_dev->cdev = cdev_alloc();
	cdev_init(voice_svc_dev->cdev, &voice_svc_fops);
	ret = cdev_add(voice_svc_dev->cdev, device_num, MINOR_NUMBER);
	if (ret) {
		pr_err("%s: Failed to register chrdev; err = %d\n", __func__,
			ret);
		goto add_err;
	}
	pr_debug("%s: Device created\n", __func__);
	goto done;

add_err:
	cdev_del(voice_svc_dev->cdev);
	device_destroy(voice_svc_class, device_num);
dev_err:
	class_destroy(voice_svc_class);
class_err:
	unregister_chrdev_region(0, MINOR_NUMBER);
done:
	return ret;
}

static int voice_svc_remove(struct platform_device *pdev)
{
	cdev_del(voice_svc_dev->cdev);
	kfree(voice_svc_dev->cdev);
	device_destroy(voice_svc_class, device_num);
	class_destroy(voice_svc_class);
	unregister_chrdev_region(0, MINOR_NUMBER);
	kfree(voice_svc_dev);

	return 0;
}

static struct of_device_id voice_svc_of_match[] = {
	{.compatible = "qcom,msm-voice-svc"},
	{ }
};
MODULE_DEVICE_TABLE(of, voice_svc_of_match);

static struct platform_driver voice_svc_driver = {
	.probe          = voice_svc_probe,
	.remove         = voice_svc_remove,
	.driver         = {
		.name   = "msm-voice-svc",
		.owner  = THIS_MODULE,
		.of_match_table = voice_svc_of_match,
	},
};

static int __init voice_svc_init(void)
{
	return platform_driver_register(&voice_svc_driver);
}

static void __exit voice_svc_exit(void)
{
	platform_driver_unregister(&voice_svc_driver);
}

module_init(voice_svc_init);
module_exit(voice_svc_exit);

MODULE_DESCRIPTION("Soc QDSP6v2 Audio APR driver");
MODULE_LICENSE("GPL v2");
