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
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/kthread.h>
#include <linux/scatterlist.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/idr.h>
#include <linux/jiffies.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/spinlock.h>
#include <linux/virtio.h>
#include <uapi/linux/virtio_ids.h>
#include <linux/virtio_config.h>
#include <linux/i2c.h>

#define I2C_ADAPTER_NR	0x00

#define I2C_VIRTIO_RD		0x01
#define I2C_VIRTIO_WR		0x02
#define I2C_VIRTIO_RDWR		0x03

struct i2c_transfer_head {
	u32 type; /* read or write from or to slave */
	u32 addr; /* slave addr */
	u32 length; /* buffer length */
	u32 total_length; /* merge write and read will use this segment */
};

struct i2c_transfer_end {
	u32 result; /* return value from backend */
};

struct virtio_i2c_req {
	struct i2c_transfer_head head;
	char   *buf;
	struct i2c_transfer_end  end;
};

/**
 * struct virtio_i2c - virtio i2c device
 * @adapter: i2c adapter
 * @vdev: the virtio device
 * @i2c_req: description of the format of transfer data
 * @vq: i2c virtqueue
 */
struct virtio_i2c {
	struct i2c_adapter adapter;
	struct virtio_device *vdev;
	struct virtio_i2c_req i2c_req;
	struct virtqueue *vq;
	wait_queue_head_t inq;
};

static int virti2c_transfer(struct virtio_i2c *vi2c,
	struct virtio_i2c_req *i2c_req)
{
	struct virtqueue *vq = vi2c->vq;
	struct scatterlist outhdr, bufhdr, inhdr, *sgs[3];
	unsigned int num_out = 0, num_in = 0, err, len;
	struct virtio_i2c_req *req_handled = NULL;

	/* send the head queue to the backend */
	sg_init_one(&outhdr, &i2c_req->head, sizeof(i2c_req->head));
	sgs[num_out++] = &outhdr;

	/* send the buffer queue to the backend */
	sg_init_one(&bufhdr, i2c_req->buf,
		(i2c_req->head.type == I2C_VIRTIO_RDWR) ?
			i2c_req->head.total_length : i2c_req->head.length);
	if (i2c_req->head.type & I2C_VIRTIO_WR)
		sgs[num_out++] = &bufhdr;
	else
		sgs[num_out + num_in++] = &bufhdr;

	/* send the result queue to the backend */
	sg_init_one(&inhdr, &i2c_req->end, sizeof(i2c_req->end));
	sgs[num_out + num_in++] = &inhdr;

	/* call the virtqueue function */
	err = virtqueue_add_sgs(vq, sgs, num_out, num_in, i2c_req, GFP_KERNEL);
	if (err)
		goto req_exit;

	/* Tell Host to go! */
	err = virtqueue_kick(vq);

	wait_event(vi2c->inq,
		(req_handled = virtqueue_get_buf(vq, &len)));

	if (i2c_req->head.type == I2C_VIRTIO_RDWR) {
		if (i2c_req->end.result ==
			i2c_req->head.total_length - i2c_req->head.length)
			err = 0;
		else
			err = -EINVAL;
	} else {
		if (i2c_req->end.result == i2c_req->head.length)
			err = 0;
		else
			err = -EINVAL;
	}
req_exit:
	return err;
}

/* prepare the transfer req */
static int virti2c_transfer_prepare(struct i2c_msg *msg_1,
		struct i2c_msg *msg_2, struct virtio_i2c_req *i2c_req)
{
	if (IS_ERR_OR_NULL(msg_1) || !msg_1->len ||
				IS_ERR_OR_NULL(msg_1->buf))
		return -EINVAL;

	i2c_req->head.addr = msg_1->addr;
	i2c_req->head.length = msg_1->len;

	if (IS_ERR_OR_NULL(msg_2)) {
		i2c_req->buf = kzalloc(msg_1->len, GFP_KERNEL);
		if (!i2c_req->buf)
			return -ENOMEM;

		if (msg_1->flags & I2C_M_RD) {
			i2c_req->head.type = I2C_VIRTIO_RD;
		} else {
			i2c_req->head.type = I2C_VIRTIO_WR;
			memcpy(i2c_req->buf, msg_1->buf, msg_1->len);
		}

	} else {
		if (!msg_2->len || IS_ERR_OR_NULL(msg_2->buf))
			return -EINVAL;

		i2c_req->buf = kzalloc((msg_1->len + msg_2->len), GFP_KERNEL);
		if (!i2c_req->buf)
			return -ENOMEM;

		i2c_req->head.type = I2C_VIRTIO_RDWR;
		i2c_req->head.total_length = msg_1->len + msg_2->len;

		memcpy(i2c_req->buf, msg_1->buf, msg_1->len);
	}

	return 0;
}

static void virti2c_transfer_end(struct virtio_i2c_req *req,
						struct i2c_msg *msg)
{
	if (req->head.type == I2C_VIRTIO_RDWR)
		memcpy(msg->buf, req->buf + req->head.length, msg->len);
	else if (req->head.type == I2C_VIRTIO_RD)
		memcpy(msg->buf, req->buf, msg->len);
	kfree(req->buf);
	req->buf = NULL;
}

static int virtio_i2c_master_xfer(struct i2c_adapter *adap,
				struct i2c_msg *msgs, int num)
{
	int i, ret;
	struct virtio_i2c *vi2c = i2c_get_adapdata(adap);
	struct virtio_i2c_req *i2c_req = &vi2c->i2c_req;

	if (num < 1) {
		dev_err(&vi2c->vdev->dev,
		"error on number of msgs(%d) received\n", num);
		return -EINVAL;
	}

	if (IS_ERR_OR_NULL(msgs)) {
		dev_err(&vi2c->vdev->dev, " error no msgs Accessing invalid pointer location\n");
		return PTR_ERR(msgs);
	}

	for (i = 0; i < num; i++) {

		if (msgs[i].flags & I2C_M_RD) {
			/* read the data from slave to master*/
			ret = virti2c_transfer_prepare(&msgs[i], NULL, i2c_req);

		} else if ((i + 1 < num) && (msgs[i + 1].flags & I2C_M_RD) &&
				(msgs[i].addr == msgs[i + 1].addr)) {
			/* write then read from same address*/
			ret  = virti2c_transfer_prepare(&msgs[i],
							&msgs[i+1], i2c_req);
			i += 1;

		} else {
			/* write the data to slave */
			ret = virti2c_transfer_prepare(&msgs[i], NULL, i2c_req);
		}

		if (ret)
			goto err;
		ret = virti2c_transfer(vi2c, i2c_req);
		virti2c_transfer_end(i2c_req, &msgs[i]);
		if (ret)
			goto err;
	}
	return num;
err:
	return ret;
}

static u32 virtio_i2c_functionality(struct i2c_adapter *adapter)
{
	return  I2C_FUNC_I2C | I2C_FUNC_SMBUS_EMUL;
}

static struct i2c_algorithm virtio_i2c_algorithm = {
	.master_xfer = virtio_i2c_master_xfer,
	.functionality = virtio_i2c_functionality,
};

/* virtqueue incoming data interrupt IRQ */
static void virti2c_vq_isr(struct virtqueue *vq)
{
	struct virtio_i2c *vi2c = vq->vdev->priv;

	wake_up(&vi2c->inq);
}

static int virti2c_init_vqs(struct virtio_i2c *vi2c)
{
	struct virtqueue *vqs[1];
	vq_callback_t *cbs[] = { virti2c_vq_isr };
	static const char * const names[] = { "virti2c_vq_isr" };
	int err;

	err = vi2c->vdev->config->find_vqs(vi2c->vdev, 1, vqs, cbs, names);
	if (err)
		return err;
	vi2c->vq = vqs[0];

	return 0;
}

static void virti2c_del_vqs(struct virtio_i2c *vi2c)
{
	vi2c->vdev->config->del_vqs(vi2c->vdev);
}

static int virti2c_init_hw(struct virtio_device *vdev,
				struct virtio_i2c *vi2c)
{
	int err;

	i2c_set_adapdata(&vi2c->adapter, vi2c);
	vi2c->adapter.algo = &virtio_i2c_algorithm;

	vi2c->adapter.owner = THIS_MODULE;
	vi2c->adapter.dev.parent = &vdev->dev;
	vi2c->adapter.dev.of_node = vdev->dev.parent->of_node;

	/* read virtio i2c config info */
	vi2c->adapter.nr = virtio_cread32(vdev, I2C_ADAPTER_NR);
	snprintf(vi2c->adapter.name, sizeof(vi2c->adapter.name),
				"virtio_i2c_%d", vi2c->adapter.nr);

	err = i2c_add_numbered_adapter(&vi2c->adapter);
	if (err)
		return err;
	return 0;
}

static int virti2c_probe(struct virtio_device *vdev)
{
	struct virtio_i2c *vi2c;
	int err = 0;

	if (!virtio_has_feature(vdev, VIRTIO_F_VERSION_1))
		return -ENODEV;

	vi2c = kzalloc(sizeof(*vi2c), GFP_KERNEL);
	if (!vi2c)
		return -ENOMEM;

	vi2c->vdev = vdev;
	vdev->priv = vi2c;
	init_waitqueue_head(&vi2c->inq);

	err = virti2c_init_vqs(vi2c);
	if (err)
		goto err_init_vq;

	err = virti2c_init_hw(vdev, vi2c);
	if (err)
		goto err_init_hw;

	virtio_device_ready(vdev);

	virtqueue_enable_cb(vi2c->vq);
	return 0;

err_init_hw:
	virti2c_del_vqs(vi2c);
err_init_vq:
	kfree(vi2c);
	return err;
}
static void virti2c_remove(struct virtio_device *vdev)
{
	struct virtio_i2c *vi2c = vdev->priv;

	i2c_del_adapter(&vi2c->adapter);
	vdev->config->reset(vdev);
	virti2c_del_vqs(vi2c);
	kfree(vi2c);
}

static unsigned int features[] = {
	/* none */
};
static struct virtio_device_id id_table[] = {
	{ VIRTIO_ID_I2C, VIRTIO_DEV_ANY_ID },
	{ 0 },
};

static struct virtio_driver virtio_i2c_driver = {
	.driver.name		= KBUILD_MODNAME,
	.driver.owner		= THIS_MODULE,
	.feature_table		= features,
	.feature_table_size	= ARRAY_SIZE(features),
	.id_table		= id_table,
	.probe			= virti2c_probe,
	.remove			= virti2c_remove,
};

module_virtio_driver(virtio_i2c_driver);
MODULE_DEVICE_TABLE(virtio, id_table);

MODULE_DESCRIPTION("Virtio i2c frontend driver");
MODULE_LICENSE("GPL v2");
