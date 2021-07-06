/******************************************************************************
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * GPL LICENSE SUMMARY
 *
 * Copyright(c) 2016 Intel Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * The full GNU General Public License is included in this distribution
 * in the file called LICENSE.GPL.
 *
 * Contact Information:
 *	Intel Corporation.
 *	linux-mei@linux.intel.com
 *	http://www.intel.com
 *
 * BSD LICENSE
 *
 * Copyright(c) 2016 Intel Corporation. All rights reserved.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  * Neither the name Intel Corporation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *****************************************************************************/
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <crypto/hash.h>
#include <linux/scatterlist.h>
#include <linux/sizes.h>

#include <linux/rpmb.h>

static const char id[] = "RPMB:SIM";
#define CAPACITY_UNIT SZ_128K
#define CAPACITY_MIN  SZ_128K
#define CAPACITY_MAX  SZ_16M
#define BLK_UNIT      SZ_256

static unsigned int max_wr_blks = 2;
module_param(max_wr_blks, uint, 0644);
MODULE_PARM_DESC(max_wr_blks, "max blocks that can be written in a single command (default: 2)");

static unsigned int daunits = 1;
module_param(daunits, uint, 0644);
MODULE_PARM_DESC(daunits, "number of data area units of 128K (default: 1)");

struct blk {
	u8 data[BLK_UNIT];
};

/**
 * struct rpmb_sim_dev
 *
 * @dev:  back pointer device
 * @rdev: rpmb device
 * @auth_key: Authentication key register which is used to authenticate
 *            accesses when MAC is calculated;
 * @auth_key_set: true if auth key was set
 * @write_counter: Counter value for the total amount of successful
 *             authenticated data write requests made by the host.
 *             The initial value of this register after production is 00000000h.
 *             The value will be incremented by one along with each successful
 *             programming access. The value cannot be reset. After the counter
 *             has reached the maximum value of FFFFFFFFh,
 *             it will not be incremented anymore (overflow prevention)
 * @hash_tfm:  hmac(sha256) tfm
 *
 * @res_frames: frame that holds the result of the last write operation
 * @out_frames: next read operation result frames
 * @out_frames_cnt: number of the output frames
 *
 * @capacity: size of the partition in bytes multiple of 128K
 * @blkcnt:   block count
 * @da:       data area in blocks
 */
struct rpmb_sim_dev {
	struct device *dev;
	struct rpmb_dev *rdev;
	u8 auth_key[32];
	bool auth_key_set;
	u32 write_counter;
	struct crypto_shash *hash_tfm;

	struct rpmb_frame res_frames[1];
	struct rpmb_frame *out_frames;
	unsigned int out_frames_cnt;

	size_t capacity;
	size_t blkcnt;
	struct blk *da;
};

static __be16 op_result(struct rpmb_sim_dev *rsdev, u16 result)
{
	if (!rsdev->auth_key_set)
		return cpu_to_be16(RPMB_ERR_NO_KEY);

	if (rsdev->write_counter == 0xFFFFFFFF)
		result |=  RPMB_ERR_COUNTER_EXPIRED;

	return cpu_to_be16(result);
}

static __be16 req_to_resp(u16 req)
{
	return cpu_to_be16(RPMB_REQ2RESP(req));
}

static int rpmb_sim_calc_hmac(struct rpmb_sim_dev *rsdev,
			      struct rpmb_frame *frames,
			      unsigned int blks, u8 *mac)
{
	SHASH_DESC_ON_STACK(desc, rsdev->hash_tfm);
	int i;
	int ret;

	desc->tfm = rsdev->hash_tfm;
	desc->flags = 0;

	ret = crypto_shash_init(desc);
	if (ret)
		goto out;

	for (i = 0; i < blks; i++) {
		ret = crypto_shash_update(desc, frames[i].data, hmac_data_len);
		if (ret)
			goto out;
	}
	ret = crypto_shash_final(desc, mac);
out:
	if (ret)
		dev_notice(rsdev->dev, "digest error = %d", ret);

	return ret;
}

static int rpmb_op_not_programmed(struct rpmb_sim_dev *rsdev, u16 req)
{
	struct rpmb_frame *res_frame = rsdev->res_frames;

	res_frame->req_resp = req_to_resp(req);
	res_frame->result = op_result(rsdev, RPMB_ERR_NO_KEY);

	dev_notice(rsdev->dev, "not programmed\n");

	return 0;
}

static int rpmb_op_program_key(struct rpmb_sim_dev *rsdev,
			       struct rpmb_frame *in_frame, u32 cnt)
{
	struct rpmb_frame *res_frame = rsdev->res_frames;
	u16 req;
	int ret;
	u16 err = RPMB_ERR_OK;

	req = be16_to_cpu(in_frame[0].req_resp);

	if (req != RPMB_PROGRAM_KEY)
		return -EINVAL;

	if (cnt != 1) {
		dev_notice(rsdev->dev, "wrong number of frames %d != 1\n", cnt);
		return -EINVAL;
	}

	if (rsdev->auth_key_set) {
		dev_notice(rsdev->dev, "key allread set\n");
		err = RPMB_ERR_WRITE;
		goto out;
	}

	ret = crypto_shash_setkey(rsdev->hash_tfm, in_frame[0].key_mac, 32);
	if (ret) {
		dev_notice(rsdev->dev, "set key failed = %d\n", ret);
		err = RPMB_ERR_GENERAL;
		goto out;
	}

	dev_dbg(rsdev->dev, "digest size %u\n",
		crypto_shash_digestsize(rsdev->hash_tfm));

	memcpy(rsdev->auth_key, in_frame[0].key_mac, 32);
	rsdev->auth_key_set = true;
out:

	memset(res_frame, 0, sizeof(struct rpmb_frame));
	res_frame->req_resp = req_to_resp(req);
	res_frame->result = op_result(rsdev, err);

	return 0;
}

static int rpmb_op_get_wr_counter(struct rpmb_sim_dev *rsdev,
				  struct rpmb_frame *in_frame, u32 cnt)
{
	struct rpmb_frame *frame;
	int ret = 0;
	u16 req;
	u16 err;

	req = be16_to_cpu(in_frame[0].req_resp);
	if (req != RPMB_GET_WRITE_COUNTER)
		return -EINVAL;

	if (cnt != 1) {
		dev_notice(rsdev->dev, "wrong number of frames %d != 1\n", cnt);
		return -EINVAL;
	}

	frame = kcalloc(1, sizeof(struct rpmb_frame), GFP_KERNEL);
	if (!frame) {
		err = RPMB_ERR_READ;
		ret = -ENOMEM;
		rsdev->out_frames = rsdev->res_frames;
		rsdev->out_frames_cnt = cnt;
		goto out;
	}

	rsdev->out_frames = frame;
	rsdev->out_frames_cnt = cnt;

	frame->req_resp = req_to_resp(req);
	frame->write_counter = cpu_to_be32(rsdev->write_counter);
	memcpy(frame->nonce, in_frame[0].nonce, 16);

	err = RPMB_ERR_OK;
	if (rpmb_sim_calc_hmac(rsdev, frame, cnt, frame->key_mac))
		err = RPMB_ERR_READ;

out:
	rsdev->out_frames[0].req_resp = req_to_resp(req);
	rsdev->out_frames[0].result = op_result(rsdev, err);

	return ret;
}

static int rpmb_op_write_data(struct rpmb_sim_dev *rsdev,
			      struct rpmb_frame *in_frame, u32 cnt)
{
	struct rpmb_frame *res_frame = rsdev->res_frames;
	u8 mac[32];
	u16 req, err, addr, blks;
	unsigned int i;
	int ret = 0;

	req = be16_to_cpu(in_frame[0].req_resp);
	if (req != RPMB_WRITE_DATA)
		return -EINVAL;

	if (rsdev->write_counter == 0xFFFFFFFF) {
		err = RPMB_ERR_WRITE;
		goto out;
	}

	blks = be16_to_cpu(in_frame[0].block_count);
	if (blks == 0 || blks > cnt) {
		dev_notice(rsdev->dev, "wrong number of frames %u > %u\n",
			blks, cnt);
		ret = -EINVAL;
		err = RPMB_ERR_GENERAL;
		goto out;
	}

	if (blks > max_wr_blks) {
		err = RPMB_ERR_WRITE;
		goto out;
	}

	addr = be16_to_cpu(in_frame[0].addr);
	if (addr >= rsdev->blkcnt) {
		err = RPMB_ERR_ADDRESS;
		goto out;
	}

	if (rpmb_sim_calc_hmac(rsdev, in_frame, blks, mac)) {
		err = RPMB_ERR_AUTH;
		goto out;
	}

	/* mac is in the last frame */
	if (memcmp(mac, in_frame[blks - 1].key_mac, sizeof(mac)) != 0) {
		err = RPMB_ERR_AUTH;
		goto out;
	}

	if (be32_to_cpu(in_frame[0].write_counter) != rsdev->write_counter) {
		err = RPMB_ERR_COUNTER;
		goto out;
	}

	if (addr + blks > rsdev->blkcnt) {
		err = RPMB_ERR_WRITE;
		goto out;
	}

	dev_dbg(rsdev->dev, "Writing = %u blokcs at addr = 0x%X\n", blks, addr);
	err = RPMB_ERR_OK;
	for (i = 0; i < blks; i++)
		memcpy(rsdev->da[addr + i].data, in_frame[i].data, BLK_UNIT);

	rsdev->write_counter++;

	memset(res_frame, 0, sizeof(struct rpmb_frame));
	res_frame->req_resp = req_to_resp(req);
	res_frame->write_counter = cpu_to_be32(rsdev->write_counter);
	res_frame->addr = cpu_to_be16(addr);
	if (rpmb_sim_calc_hmac(rsdev, res_frame, 1, res_frame->key_mac))
		err = RPMB_ERR_READ;

out:
	if (err != RPMB_ERR_OK) {
		memset(res_frame, 0, sizeof(struct rpmb_frame));
		res_frame->req_resp = req_to_resp(req);
	}
	res_frame->result = op_result(rsdev, err);

	return ret;
}

static int rpmb_do_read_data(struct rpmb_sim_dev *rsdev,
			     struct rpmb_frame *in_frame, u32 cnt)
{
	struct rpmb_frame *res_frame = rsdev->res_frames;
	struct rpmb_frame *out_frames = NULL;
	u8 mac[32];
	u16 req, err, addr, blks;
	unsigned int i;
	int ret;

	req = be16_to_cpu(in_frame->req_resp);
	if (req != RPMB_READ_DATA)
		return -EINVAL;

	/* eMMc intentially set 0 here */
	blks = be16_to_cpu(in_frame->block_count);
	blks = blks ?: cnt;
	if (blks > cnt) {
		dev_notice(rsdev->dev, "wrong number of frames cnt %u\n", blks);
		ret = -EINVAL;
		err = RPMB_ERR_GENERAL;
		goto out;
	}

	out_frames = kcalloc(blks, sizeof(struct rpmb_frame), GFP_KERNEL);
	if (!out_frames) {
		ret = -ENOMEM;
		err = RPMB_ERR_READ;
		goto out;
	}

	ret = 0;
	addr = be16_to_cpu(in_frame[0].addr);
	if (addr >= rsdev->blkcnt) {
		err = RPMB_ERR_ADDRESS;
		goto out;
	}

	if (addr + blks > rsdev->blkcnt) {
		err = RPMB_ERR_READ;
		goto out;
	}

	dev_dbg(rsdev->dev, "reading = %u blokcs at addr = 0x%X\n", blks, addr);
	for (i = 0; i < blks; i++) {
		memcpy(out_frames[i].data, rsdev->da[addr + i].data, BLK_UNIT);
		memcpy(out_frames[i].nonce, in_frame[0].nonce, 16);
		out_frames[i].req_resp = req_to_resp(req);
		out_frames[i].addr = in_frame[0].addr;
		out_frames[i].block_count = cpu_to_be16(blks);
	}

	if (rpmb_sim_calc_hmac(rsdev, out_frames, blks, mac)) {
		err = RPMB_ERR_AUTH;
		goto out;
	}

	memcpy(out_frames[blks - 1].key_mac, mac, sizeof(mac));

	err = RPMB_ERR_OK;
	for (i = 0; i < blks; i++)
		out_frames[i].result = op_result(rsdev, err);

	rsdev->out_frames = out_frames;
	rsdev->out_frames_cnt = cnt;

	return 0;

out:
	memset(res_frame, 0, sizeof(struct rpmb_frame));
	res_frame->req_resp = req_to_resp(req);
	res_frame->result = op_result(rsdev, err);
	kfree(out_frames);
	rsdev->out_frames = res_frame;
	rsdev->out_frames_cnt = 1;

	return ret;
}

static int rpmb_op_read_data(struct rpmb_sim_dev *rsdev,
			     struct rpmb_frame *in_frame, u32 cnt)
{
	struct rpmb_frame *res_frame = rsdev->res_frames;
	u16 req;

	req = be16_to_cpu(in_frame->req_resp);
	if (req != RPMB_READ_DATA)
		return -EINVAL;

	memcpy(res_frame, in_frame, sizeof(*res_frame));

	rsdev->out_frames = res_frame;
	rsdev->out_frames_cnt = 1;

	return 0;
}

static int rpmb_op_result_read(struct rpmb_sim_dev *rsdev,
			       struct rpmb_frame *frames, u32 cnt)
{
	u16 req = be16_to_cpu(frames[0].req_resp);
	u16 blks = be16_to_cpu(frames[0].block_count);

	if (req != RPMB_RESULT_READ)
		return -EINVAL;

	if (blks != 0) {
		dev_notice(rsdev->dev, "wrong number of frames %u != 0\n",
				blks);
		return -EINVAL;
	}

	rsdev->out_frames = rsdev->res_frames;
	rsdev->out_frames_cnt = 1;
	return 0;
}

static int rpmb_sim_write(struct rpmb_sim_dev *rsdev,
			  struct rpmb_frame *frames, u32 cnt)
{
	u16 req;
	int ret;

	if (!frames || !cnt)
		return -EINVAL;

	req = be16_to_cpu(frames[0].req_resp);
	if (!rsdev->auth_key_set && req != RPMB_PROGRAM_KEY)
		return rpmb_op_not_programmed(rsdev, req);

	switch (req) {
	case RPMB_PROGRAM_KEY:
		dev_dbg(rsdev->dev, "rpmb: program key\n");
		ret = rpmb_op_program_key(rsdev, frames, cnt);
		break;
	case RPMB_WRITE_DATA:
		dev_dbg(rsdev->dev, "rpmb: write data\n");
		ret = rpmb_op_write_data(rsdev, frames, cnt);
		break;
	case RPMB_GET_WRITE_COUNTER:
		dev_dbg(rsdev->dev, "rpmb: get write counter\n");
		ret = rpmb_op_get_wr_counter(rsdev, frames, cnt);
		break;
	case RPMB_READ_DATA:
		dev_dbg(rsdev->dev, "rpmb: read data\n");
		ret = rpmb_op_read_data(rsdev, frames, cnt);
		break;
	case RPMB_RESULT_READ:
		dev_dbg(rsdev->dev, "rpmb: result read\n");
		ret = rpmb_op_result_read(rsdev, frames, cnt);
		break;
	default:
		dev_notice(rsdev->dev, "unsupported command %u\n", req);
		ret = -EINVAL;
		break;
	}

	dev_dbg(rsdev->dev, "rpmb: ret=%d\n", ret);

	return ret;
}

static int rpmb_sim_read(struct rpmb_sim_dev *rsdev,
			 struct rpmb_frame *frames, u32 cnt)
{
	int i;

	if (!frames || !cnt)
		return -EINVAL;

	if (!rsdev->out_frames || rsdev->out_frames_cnt == 0) {
		dev_notice(rsdev->dev, "out_frames are not set\n");
		return -EINVAL;
	}

	if (rsdev->out_frames->req_resp == cpu_to_be16(RPMB_READ_DATA))
		rpmb_do_read_data(rsdev, rsdev->out_frames, cnt);

	for (i = 0; i < min_t(u32, rsdev->out_frames_cnt, cnt); i++)
		memcpy(&frames[i], &rsdev->out_frames[i], sizeof(frames[i]));

	if (rsdev->out_frames != rsdev->res_frames)
		kfree(rsdev->out_frames);

	rsdev->out_frames = NULL;
	rsdev->out_frames_cnt = 0;
	dev_dbg(rsdev->dev, "rpmb: cnt=%d\n", cnt);

	return 0;
}

static int rpmb_sim_cmd_seq(struct device *dev,
			    struct rpmb_cmd *cmds, u32 ncmds)
{
	struct rpmb_sim_dev *rsdev;
	int i;
	int ret;
	struct rpmb_cmd *cmd;

	if (!dev)
		return -EINVAL;

	dev_notice(dev, "rpmb_cmd_seq\n");

	rsdev = dev_get_drvdata(dev);

	if (!rsdev)
		return -EINVAL;

	for (ret = 0, i = 0; i < ncmds && !ret; i++) {
		cmd = &cmds[i];
		if (cmd->flags & RPMB_F_WRITE)
			ret = rpmb_sim_write(rsdev, cmd->frames, cmd->nframes);
		else
			ret = rpmb_sim_read(rsdev, cmd->frames, cmd->nframes);
	}
	return ret;
}

static struct rpmb_ops rpmb_sim_ops = {
	.cmd_seq = rpmb_sim_cmd_seq,
	.type = RPMB_TYPE_EMMC,
};

static int rpmb_sim_hmac_256_alloc(struct rpmb_sim_dev *rsdev)
{
	struct crypto_shash *hash_tfm;

	hash_tfm = crypto_alloc_shash("hmac(sha256)", 0, 0);
	if (IS_ERR(hash_tfm))
		return PTR_ERR(hash_tfm);

	rsdev->hash_tfm = hash_tfm;

	dev_dbg(rsdev->dev, "hamac(sha256) registered\n");
	return 0;
}

static void rpmb_sim_hmac_256_free(struct rpmb_sim_dev *rsdev)
{
	if (rsdev->hash_tfm)
		crypto_free_shash(rsdev->hash_tfm);

	rsdev->hash_tfm = NULL;
}

static int rpmb_sim_probe(struct device *dev)
{
	struct rpmb_sim_dev *rsdev;
	int ret;

	rsdev = kzalloc(sizeof(*rsdev), GFP_KERNEL);
	if (!rsdev)
		return -ENOMEM;

	rsdev->dev = dev;

	ret = rpmb_sim_hmac_256_alloc(rsdev);
	if (ret)
		goto err;

	rsdev->capacity = CAPACITY_UNIT * daunits;
	rsdev->blkcnt  = rsdev->capacity / BLK_UNIT;
	rsdev->da = kzalloc(rsdev->capacity, GFP_KERNEL);
	if (!rsdev->da) {
		ret = -ENOMEM;
		goto err;
	}

	rpmb_sim_ops.dev_id_len = strlen(id);
	rpmb_sim_ops.dev_id = id;
	rpmb_sim_ops.reliable_wr_cnt = max_wr_blks;

	rsdev->rdev = rpmb_dev_register(rsdev->dev, &rpmb_sim_ops);
	if (IS_ERR(rsdev->rdev)) {
		ret = PTR_ERR(rsdev->rdev);
		goto err;
	}

	dev_info(dev, "registered RPMB capacity = %zu of %zu blocks\n",
		 rsdev->capacity, rsdev->blkcnt);

	dev_set_drvdata(dev, rsdev);

	return 0;
err:
	rpmb_sim_hmac_256_free(rsdev);
	if (rsdev)
		kfree(rsdev->da);
	kfree(rsdev);
	return ret;
}

static int rpmb_sim_remove(struct device *dev)
{
	struct rpmb_sim_dev *rsdev;

	rsdev = dev_get_drvdata(dev);

	rpmb_dev_unregister(rsdev->dev);

	dev_set_drvdata(dev, NULL);

	rpmb_sim_hmac_256_free(rsdev);

	kfree(rsdev->da);
	kfree(rsdev);
	return 0;
}

static void rpmb_sim_shutdown(struct device *dev)
{
	rpmb_sim_remove(dev);
}

static int rpmb_sim_match(struct device *dev, struct device_driver *drv)
{
	return 1;
}

static struct bus_type rpmb_sim_bus = {
	.name = "rpmb_sim",
	.match = rpmb_sim_match,
};

static struct device_driver rpmb_sim_drv = {
	.name  = "rpmb_sim",
	.probe = rpmb_sim_probe,
	.remove = rpmb_sim_remove,
	.shutdown = rpmb_sim_shutdown,
};

static void rpmb_sim_dev_release(struct device *dev)
{
}

static struct device rpmb_sim_dev;

static int __init rpmb_sim_init(void)
{
	int ret;
	struct device *dev = &rpmb_sim_dev;
	struct device_driver *drv = &rpmb_sim_drv;

	ret = bus_register(&rpmb_sim_bus);
	if (ret)
		return ret;

	dev->bus = &rpmb_sim_bus;
	dev->release = rpmb_sim_dev_release;
	dev_set_name(dev, "%s", "rpmb_sim");
	ret = device_register(dev);
	if (ret) {
		pr_notice("device register failed %d\n", ret);
		goto err_device;
	}

	drv->bus = &rpmb_sim_bus;
	ret = driver_register(drv);
	if (ret) {
		pr_notice("driver register failed %d\n", ret);
		goto err_driver;
	}

	return 0;

err_driver:
	device_unregister(dev);
err_device:
	bus_unregister(&rpmb_sim_bus);
	return ret;
}

static void __exit rpmb_sim_exit(void)
{
	struct device *dev = &rpmb_sim_dev;
	struct device_driver *drv = &rpmb_sim_drv;

	device_unregister(dev);
	driver_unregister(drv);
	bus_unregister(&rpmb_sim_bus);
}

module_init(rpmb_sim_init);
module_exit(rpmb_sim_exit);

MODULE_AUTHOR("Tomas Winkler <tomas.winkler@intel.com");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_ALIAS("rpmb_sim:rpmb_sim");
