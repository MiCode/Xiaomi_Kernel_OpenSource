// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2019-2020, The Linux Foundation. All rights reserved.
 */

#include "nfc_common.h"

/** @brief   This API used to write I3C data to I3C device.
 *
 *  @param dev   the i3c_dev for the i3c device.
 *  @param buf   the data to write
 *  @param count the number of bytes of data to be written.
 *  @param max_retry_count the number of retries for the write operation
 *
 *  @return ret  number of bytes written ,negative error core otherwise.
 */
int i3c_write(struct nfc_dev *dev, const char *buf, const size_t count,
		  int max_retry_cnt)
{
	int ret = -EIO;
	int retry_count = 0;
	struct i3c_priv_xfer write_buf = {
		.rnw = NFC_I3C_WRITE,
		.len = count,
		.data.out = buf
	};

	do {
		ret = i3c_device_do_priv_xfers(dev->i3c_dev.device, &write_buf,
			(sizeof(write_buf) / sizeof(struct i3c_priv_xfer)));

		pr_debug("%s exit ret = %x\n", __func__, ret);
		if (!ret) {
			ret = count;
			break;
		}
		pr_err("%s errno = %x\n", __func__, write_buf.err);
		retry_count++;

		usleep_range(1000, 1100);
	} while (retry_count < max_retry_cnt);

	return ret;
}

/** @brief   This API used to read data from I3C device.
 *
 *  @param dev   the i3c_dev for the slave.
 *  @param buf   the buffer to copy the data.
 *  @param count the number of bytes to be read.
 *
 *  @return number of bytes read ,negative error code otherwise.
 */
int i3c_read(struct nfc_dev *dev, char *buf, size_t count)
{
	int ret = -EIO;
	struct i3c_priv_xfer read_buf = {
		.rnw = NFC_I3C_READ,
		.len = count,
		.data.in = buf
	};

	ret = i3c_device_do_priv_xfers(dev->i3c_dev.device, &read_buf,
			     (sizeof(read_buf) / sizeof(struct i3c_priv_xfer)));
	pr_debug("%s exit ret = %x\n", __func__, ret);
	if (!ret)
		ret = count;
	else
		pr_err("%s errno = %x\n", __func__, read_buf.err);

	return ret;
}

/** @brief   This API can be used to write data to nci buf.
 *           The API will overwrite the existing memory if
 *           it reaches the end of total allocated memory.
 *
 *  @param dev   the dev structure for driver.
 *  @param readbuf   the buffer to be copied data from
 *  @param count the number of bytes to copy to nci_buffer.
 *
 *  @return number of bytes copied to nci buffer , error code otherwise
 */
static ssize_t i3c_kbuf_store(struct i3c_dev *i3c_dev, const char *buf,
			      const size_t count)
{
	size_t buf_offset = 0;
	size_t requested_size = count;
	size_t available_size = 0;

	if (i3c_dev == NULL)
		return -ENODEV;
	else if (buf == NULL || count == 0)
		return -EINVAL;

	pr_debug("%s enter\n", __func__);
	if (count > i3c_dev->buf.total_size) {
		pr_err("%s No memory to copy the data\n", __func__);
		return -ENOMEM;
	}

	pr_debug("%s:total_size %zx write_off = %x read_off = %x\n",
		__func__, i3c_dev->buf.total_size, i3c_dev->buf.write_offset,
		i3c_dev->buf.read_offset);

	mutex_lock(&i3c_dev->nci_buf_mutex);

	available_size = i3c_dev->buf.total_size - i3c_dev->buf.write_offset;

	/*
	 * When available buffer is less than requested count,
	 * copy the data upto available memory.
	 * The remaining data is copied to the start of memory.
	 * The write offset is incremented by the remaining copied bytes
	 * from the beginning.
	 */

	if (requested_size > available_size) {
		pr_warn
		    ("%s:out of mem req_size = %zx avail = %zx\n",
		     __func__, requested_size, available_size);
		memcpy(i3c_dev->buf.kbuf + i3c_dev->buf.write_offset,
		       buf + buf_offset, available_size);
		requested_size = requested_size - available_size;
		i3c_dev->buf.write_offset = 0;
		buf_offset = available_size;
		pr_debug("%s: requested_size = %zx available_size = %zx\n",
			 __func__, requested_size, available_size);
	}
	if (requested_size) {
		memcpy(i3c_dev->buf.kbuf + i3c_dev->buf.write_offset,
		       buf + buf_offset, requested_size);
		i3c_dev->buf.write_offset += requested_size;
		if (i3c_dev->buf.write_offset == i3c_dev->buf.total_size)
			i3c_dev->buf.write_offset = 0;
	}
	complete(&i3c_dev->read_cplt);
	mutex_unlock(&i3c_dev->nci_buf_mutex);
	pr_debug("%s: total bytes req_size = %zx avail_size = %zx\n",
		 __func__, requested_size, available_size);

	return count;
}

/** @brief   This API can be used to retrieve data from driver buffer.
 *
 *  When data is not available, it waits for required data to be present.
 *  When data is present it copies the data into buffer reuested by read.
 *  @param dev  the dev structure for driver.
 *  @param buf   the buffer to copy the data.
 *  @param count the number of bytes to be read.
 *
 *  @return number of bytes copied , error code for failures .
 */
int i3c_nci_kbuf_retrieve(struct nfc_dev *dev, char *buf,
				     size_t count)
{
	size_t requested_size = count;
	size_t available_size = 0;
	size_t copied_size = 0;
	struct i3c_dev *i3c_dev = &dev->i3c_dev;
	int ret = 0;

	if (i3c_dev == NULL)
		return -ENODEV;
	else if (buf == NULL || count == 0)
		return -EINVAL;
	pr_debug("%s enter\n", __func__);

	/*
	 * When the requested data count is more than available data to read,
	 * wait on completion till the requested bytes are available.
	 * If write offset is more than read offset and available data is
	 * more than requested count, copy the requested bytes directly and
	 * increment the read_offset.
	 * If read offset is more than write offset,
	 * available size is total_size size - read_offset
	 * and upto write offset from the beginning of buffer.
	 */

	do {

		pr_debug("%s: read_offset = %x write_offset = %x\n", __func__,
			 i3c_dev->buf.read_offset, i3c_dev->buf.write_offset);

		mutex_lock(&i3c_dev->nci_buf_mutex);

		if (i3c_dev->buf.read_offset <= i3c_dev->buf.write_offset)
			available_size =
			    i3c_dev->buf.write_offset -
			    i3c_dev->buf.read_offset;
		else
			available_size =
			    (i3c_dev->buf.total_size -
			     i3c_dev->buf.read_offset) +
			    i3c_dev->buf.write_offset;
		mutex_unlock(&i3c_dev->nci_buf_mutex);
		if (available_size >= requested_size)
			break;

		reinit_completion(&i3c_dev->read_cplt);
		/*
		 * During probe if there is no response for NCI commands,
		 * probe shouldn't be blocked, that is why timeout is added.
		 */

		if (i3c_dev->is_probe_done) {
			ret = wait_for_completion_interruptible(
							&i3c_dev->read_cplt);
			if (ret != 0) {
				pr_err("nfc completion interrupted! ret %d\n",
							ret);
				return ret;
			}
		} else {
			ret = wait_for_completion_interruptible_timeout(
					&i3c_dev->read_cplt,
					msecs_to_jiffies(MAX_IBI_WAIT_TIME));
			if (ret <= 0) {
				pr_err("nfc completion timedout ret %d\n",
							ret);
				return ret;
			}
		}
	} while (available_size < requested_size);

	mutex_lock(&i3c_dev->nci_buf_mutex);

	if (i3c_dev->buf.write_offset >=
	    i3c_dev->buf.read_offset + requested_size) {
		/*
		 * Write offset is more than read offset + count , copy the data
		 * directly and increment the read offset
		 */
		memcpy(buf, i3c_dev->buf.kbuf + i3c_dev->buf.read_offset,
		       requested_size);
		i3c_dev->buf.read_offset += requested_size;
	} else {
		copied_size =
		    i3c_dev->buf.total_size - i3c_dev->buf.read_offset;
		if (copied_size > requested_size)
			copied_size = requested_size;
		/*
		 * Read offset is more than write offset. Copy requested data
		 * from read_offset to the total size and increment the read
		 * offset. If requested data is still greater than zero,
		 * copy the data from beginning of buffer.
		 */
		memcpy(buf, i3c_dev->buf.kbuf + i3c_dev->buf.read_offset,
		       copied_size);
		requested_size = requested_size - copied_size;
		i3c_dev->buf.read_offset += copied_size;
		if (requested_size) {
			pr_debug("%s  remaining copied bytes\n", __func__);
			memcpy(buf + copied_size, i3c_dev->buf.kbuf,
				requested_size);
			i3c_dev->buf.read_offset = requested_size;
		}
	}
	mutex_unlock(&i3c_dev->nci_buf_mutex);
	pr_debug("%s , count = %zx exit\n", __func__, count);
	return count;
}

/** @brief   This API can be used to read data from I3C device from HAL layer.
 *
 *  This read function is registered during probe.
 *  When data is not available, it waits for required data to be present.
 *  @param filp  the device file handle opened by HAL.
 *  @param buf   the buffer to read the data.
 *  @param count the number of bytes to be read.
 *  @param offset the offset in the buf.
 *
 *  @return Number of bytes read from I3C device ,error code for failures.
 */
ssize_t nfc_i3c_dev_read(struct file *filp, char __user *buf,
			 size_t count, loff_t *offset)
{
	int ret;
	char *tmp = NULL;
	struct nfc_dev *nfc_dev = filp->private_data;

	if (count > nfc_dev->kbuflen)
		count = nfc_dev->kbuflen;

	pr_debug("%s : reading %zu bytes\n", __func__, count);

	mutex_lock(&nfc_dev->read_mutex);

	tmp = nfc_dev->kbuf;
	ret = i3c_nci_kbuf_retrieve(nfc_dev, tmp, count);
	if (ret != count) {
		pr_err("%s: kbuf read err ret (%d)\n",
		       __func__, ret);
		ret = -EIO;
	} else if (copy_to_user(buf, tmp, ret)) {
		pr_warn("%s : failed to copy to user space\n", __func__);
		ret = -EFAULT;
	}

	mutex_unlock(&nfc_dev->read_mutex);

	return ret;
}

/** @brief   This API can be used to write data to I3C device.
 *
 *  @param dev   the i3c_dev for the slave.
 *  @param buf   the buffer to copy the data.
 *  @param count the number of bytes to be read.
 *
 *  @return ret count number of bytes written, error code for failures.
 */
ssize_t nfc_i3c_dev_write(struct file *filp, const char __user *buf,
			  size_t count, loff_t *offset)
{
	int ret;
	char *tmp = NULL;
	struct nfc_dev *nfc_dev = filp->private_data;

	if (!nfc_dev)
		return -ENODEV;


	if (count > nfc_dev->kbuflen) {
		pr_err("%s: out of memory\n", __func__);
		return -ENOMEM;
	}

	tmp = memdup_user(buf, count);
	if (IS_ERR(tmp)) {
		pr_err("%s: memdup_user failed\n", __func__);
		ret = PTR_ERR(tmp);
		goto out;
	}

	ret = i3c_write(nfc_dev, tmp, count, NO_RETRY);
	if (ret != count) {
		pr_err("%s: i3c_write err ret %d\n", __func__, ret);
		ret = -EIO;
		goto out_free;
	}
	pr_debug("%s : i3c-%d: NfcNciTx %x %x %x\n", __func__,
		 iminor(file_inode(filp)), tmp[0], tmp[1], tmp[2]);
	pr_debug("%s : ret = %x\n", __func__, ret);

out_free:
	kfree(tmp);
out:
	return ret;
}

/** @brief   This API shall be called from  workqueue queue from IBI handler.
 *           First it will read HDR byte from I3C chip.Based on the length byte
 *           it will read the next length bytes.Then it will write these bytes
 *           to nci write buf.
 *  @param work   the work added into the workqueue.
 *
 *  @return void
 */
static void i3c_workqueue_handler(struct work_struct *work)
{
	int ret = 0;
	int length_byte = 0;
	unsigned char *tmp = NULL;
	unsigned char hdr_len = NCI_HDR_LEN;
	struct i3c_dev *i3c_dev = container_of(work, struct i3c_dev, work);
	struct nfc_dev *nfc_dev = container_of(i3c_dev,
						struct nfc_dev, i3c_dev);

	if (!i3c_dev) {
		pr_err("%s: dev not found\n", __func__);
		return;
	}
	hdr_len = i3c_dev->read_hdr;
	tmp = i3c_dev->read_kbuf;
	if (!tmp) {
		pr_err("%s: No memory to copy read data\n", __func__);
		return;
	}
	pr_debug("%s: hdr_len = %d\n", __func__, hdr_len);
	memset(tmp, 0x00, i3c_dev->read_kbuf_len);

	ret = i3c_read(nfc_dev, tmp, hdr_len);
	if (ret < 0) {
		pr_err("%s: i3c_read returned error %d\n", __func__, ret);
		return;
	}
	if (hdr_len == FW_HDR_LEN)
		length_byte = tmp[hdr_len - 1] + FW_CRC_LEN;
	else
		length_byte = tmp[hdr_len - 1];

	/* check if it's response of cold reset command
	 * NFC HAL process shouldn't receive this data as
	 * command was sent by SPI driver
	 */
	if (nfc_dev->cold_reset.rsp_pending
		&& (tmp[0] == COLD_RESET_RSP_GID)
		&& (tmp[1] == COLD_RESET_OID)) {
		read_cold_reset_rsp(nfc_dev, tmp);
		nfc_dev->cold_reset.rsp_pending = false;
		wake_up_interruptible(&nfc_dev->cold_reset.read_wq);
		/*
		 * NFC process doesn't know about cold reset command
		 * being sent as it was initiated by eSE process
		 * we shouldn't return any data to NFC process
		 */
		return;
	}

	ret = i3c_read(nfc_dev, tmp + hdr_len, length_byte);
	if (ret < 0) {
		pr_err("%s: i3c_read returned error %d\n", __func__, ret);
		i3c_kbuf_store(i3c_dev, tmp, hdr_len);
		return;
	}
	i3c_kbuf_store(i3c_dev, tmp, hdr_len + length_byte);
}

/** @brief   This API  is used to handle IBI coming from the I3C device.
 *  This will add work into the workqueue , which will call workqueue
 *  handler to read data from I3C device.
 *
 *  @param device   I3C device.
 *  @param payload  payload shall be NULL for NFC device.
 *
 *  @return void.
 */
static void i3c_ibi_handler(struct i3c_device *device,
			    const struct i3c_ibi_payload *payload)
{
	struct nfc_dev *nfc_dev = i3cdev_get_drvdata(device);
	struct i3c_dev *i3c_dev = &nfc_dev->i3c_dev;

	pr_debug("%s\n", __func__);
	if (device_may_wakeup(&device->dev))
		pm_wakeup_event(&device->dev, WAKEUP_SRC_TIMEOUT);

	if (atomic_read(&i3c_dev->pm_state) == PM_STATE_NORMAL) {
		if (!queue_work(i3c_dev->wq, &i3c_dev->work))
			pr_debug("%s: queue work success\n", __func__);
	} else {
		// got IBI when in suspend
		atomic_set(&i3c_dev->pm_state, PM_STATE_IBI_BEFORE_RESUME);
	}
}

/** @brief   This API can be used to enable IBI from the I3C device.
 *
 *  @param i3c_dev   the i3c_dev for the slave.
 *
 *  @return 0 on success, error code for failures.
 */
int i3c_enable_ibi(struct nfc_dev *dev)
{
	int ret = 0;
	int retry_count = 0;

	if (!dev->i3c_dev.ibi_enabled) {

		do {
			ret = i3c_device_enable_ibi(dev->i3c_dev.device);
			if (!ret) {
				dev->i3c_dev.ibi_enabled = true;
				break;
			}

			pr_debug("en_ibi ret %d retrying..\n", ret);
			retry_count++;
			usleep_range(RETRY_WAIT_TIME_USEC,
				(RETRY_WAIT_TIME_USEC+100));
		} while (retry_count < RETRY_COUNT_IBI);
	} else {
		pr_debug("%s: already enabled\n", __func__);
	}
	return ret;
}

/** @brief   This API can be used to disable IBI from the I3C device.
 *
 *  @param i3c_dev   the i3c_dev for the slave.
 *
 *  @return 0 on success, error code for failures.
 */
int i3c_disable_ibi(struct nfc_dev *dev)
{
	int ret = 0;
	int retry_count = 0;

	if (dev->i3c_dev.ibi_enabled) {

		do {
			ret = i3c_device_disable_ibi(dev->i3c_dev.device);
			if (!ret) {
				dev->i3c_dev.ibi_enabled = false;
				break;
			}

			pr_debug("dis_ibi ret %d retrying..\n", ret);
			retry_count++;
			usleep_range(RETRY_WAIT_TIME_USEC,
					(RETRY_WAIT_TIME_USEC+100));
		} while (retry_count < RETRY_COUNT_IBI);

	} else {
		pr_debug("%s: already disabled\n", __func__);
	}
	return ret;
}

/** @brief   This API can be used to  request IBI from the I3C device.
 *  This function will request IBI from master controller for the device
 *  and register ibi handler, enable IBI .
 *
 *  @param i3c_dev   the i3c_dev for the slave.
 *
 *  @return 0 on success, error code for failures.
 */
static int i3c_request_ibi(struct i3c_dev *i3c_dev)
{
	int ret = 0;
	struct i3c_ibi_setup ibireq = {
		.handler = i3c_ibi_handler,
		.max_payload_len = MAX_IBI_PAYLOAD_LEN,
		.num_slots = NUM_NFC_IBI_SLOT,
	};

	ret = i3c_device_request_ibi(i3c_dev->device, &ibireq);
	pr_debug("%s Request IBI status = %d\n", __func__, ret);
	return ret;
}

/** @brief   This API can be used to create a workqueue for
 *           handling IBI request from I3C device.
 *
 *  @param dev   the i3c_dev for the slave.
 *
 *  @return 0 on success, error code for failures.
 */
static int i3c_init_workqueue(struct i3c_dev *i3c_dev)
{
	i3c_dev->wq = alloc_workqueue(NFC_I3C_WORKQUEUE, 0, 0);
	if (!i3c_dev->wq)
		return -ENOMEM;

	INIT_WORK(&i3c_dev->work, i3c_workqueue_handler);
	pr_debug("%s ibi workqueue created successfully\n", __func__);
	return 0;
}

/** @brief   This API can be used to set the NCI buf to zero.
 *
 *  @param  dev   the dev for the driver.
 *
 *  @return 0 on success, error code for failures.
 */
static int i3c_reset_nci_buf(struct i3c_dev *i3c_dev)
{
	i3c_dev->buf.write_offset = 0;
	i3c_dev->buf.read_offset = 0;
	memset(i3c_dev->buf.kbuf, 0, i3c_dev->buf.total_size);
	return 0;
}

static const struct file_operations nfc_i3c_dev_fops = {
	.owner = THIS_MODULE,
	.llseek = no_llseek,
	.read = nfc_i3c_dev_read,
	.write = nfc_i3c_dev_write,
	.open = nfc_dev_open,
	.release = nfc_dev_close,
	.unlocked_ioctl = nfc_dev_ioctl,
};

/** @brief   This API can be used to probe I3c device.
 *
 *  @param device   the i3c_dev for the slave.
 *
 *  @return 0 on success, error code for failures.
 */
int nfc_i3c_dev_probe(struct i3c_device *device)
{
	int ret = 0;
	struct nfc_dev *nfc_dev = NULL;
	struct platform_gpio nfc_gpio;
	struct platform_ldo nfc_ldo;

	pr_debug("%s: enter\n", __func__);

	//retrieve gpio details from dt

	ret = nfc_parse_dt(&device->dev, &nfc_gpio, &nfc_ldo, PLATFORM_IF_I3C);
	if (ret) {
		pr_err("%s : failed to parse nfc dt node\n", __func__);
		goto err;
	}

	nfc_dev = kzalloc(sizeof(struct nfc_dev), GFP_KERNEL);
	if (nfc_dev == NULL) {
		ret = -ENOMEM;
		goto err;
	}

	/*
	 * Memory allocation for the read from the I3C before
	 * storing it in Kbuf store
	 */

	nfc_dev->i3c_dev.read_hdr = NCI_HDR_LEN;
	nfc_dev->i3c_dev.read_kbuf_len = MAX_BUFFER_SIZE;
	nfc_dev->i3c_dev.read_kbuf = kzalloc(MAX_BUFFER_SIZE,
						GFP_DMA | GFP_KERNEL);
	if (!nfc_dev->i3c_dev.read_kbuf) {
		ret = -ENOMEM;
		goto err_free_nfc_dev;
	}

	/*
	 * Kbuf memory for storing NCI/Firmware Mode Buffers before
	 * actual read from the user
	 */

	nfc_dev->i3c_dev.buf.kbuf = (char *)__get_free_pages(GFP_KERNEL, 0);
	if (!nfc_dev->i3c_dev.buf.kbuf) {
		ret = -ENOMEM;
		goto err_free_kbuf;
	}
	nfc_dev->i3c_dev.buf.total_size = PAGE_SIZE;
	i3c_reset_nci_buf(&nfc_dev->i3c_dev);
	nfc_dev->interface = PLATFORM_IF_I3C;
	nfc_dev->nfc_write = i3c_write;
	nfc_dev->nfc_read = i3c_nci_kbuf_retrieve;
	nfc_dev->i3c_dev.nfc_read_direct = i3c_read;
	nfc_dev->nfc_enable_intr = i3c_enable_ibi;
	nfc_dev->nfc_disable_intr = i3c_disable_ibi;
	nfc_dev->i3c_dev.device = device;

	ret = configure_gpio(nfc_gpio.ven, GPIO_OUTPUT_HIGH);
	if (ret) {
		pr_err("%s: unable to request nfc reset gpio [%d]\n",
		       __func__, nfc_gpio.ven);
		goto err_free_pages;
	}
	ret = configure_gpio(nfc_gpio.dwl_req, GPIO_OUTPUT);
	if (ret) {
		pr_err("%s: unable to request nfc firm downl gpio [%d]\n",
		       __func__, nfc_gpio.dwl_req);
		goto err_free_ven;
	}

	ret = configure_gpio(nfc_gpio.clkreq, GPIO_INPUT);
	if (ret) {
		pr_err("%s: unable to request nfc clkreq gpio [%d]\n",
		       __func__, nfc_gpio.clkreq);
		goto err_free_dwl_req;
	}

	nfc_dev->gpio.ven = nfc_gpio.ven;
	nfc_dev->gpio.clkreq = nfc_gpio.clkreq;
	nfc_dev->gpio.dwl_req = nfc_gpio.dwl_req;

	/* init mutex and queues */
	init_completion(&nfc_dev->i3c_dev.read_cplt);
	mutex_init(&nfc_dev->i3c_dev.nci_buf_mutex);
	mutex_init(&nfc_dev->dev_ref_mutex);
	mutex_init(&nfc_dev->read_mutex);
	ret = i3c_init_workqueue(&nfc_dev->i3c_dev);
	if (ret) {
		pr_err("%s: alloc workqueue failed\n", __func__);
		goto err_mutex_destroy;
	}
	ret = nfc_misc_probe(nfc_dev, &nfc_i3c_dev_fops, DEV_COUNT,
				NFC_CHAR_DEV_NAME, CLASS_NAME);
	if (ret) {
		pr_err("%s: nfc_misc_probe failed\n", __func__);
		goto err_wq_destroy;
	}

	i3cdev_set_drvdata(device, nfc_dev);

	ret = i3c_request_ibi(&nfc_dev->i3c_dev);
	if (ret) {
		pr_err("%s: i3c_request_ibi failed\n", __func__);
		goto err_nfc_misc_remove;
	}

	ret = nfcc_hw_check(nfc_dev);
	if (ret) {
		pr_err("nfc hw check failed ret %d\n", ret);
		goto err_nfcc_hw_check;
	}

	atomic_set(&nfc_dev->i3c_dev.pm_state, PM_STATE_NORMAL);
	device_init_wakeup(&device->dev, true);
	nfc_dev->i3c_dev.is_probe_done = true;

	pr_info("%s success\n", __func__);

	return 0;

err_nfcc_hw_check:
	i3c_device_free_ibi(device);
err_nfc_misc_remove:
	nfc_misc_remove(nfc_dev, DEV_COUNT);
err_wq_destroy:
	destroy_workqueue(nfc_dev->i3c_dev.wq);
err_mutex_destroy:
	mutex_destroy(&nfc_dev->read_mutex);
	mutex_destroy(&nfc_dev->dev_ref_mutex);
	mutex_destroy(&nfc_dev->i3c_dev.nci_buf_mutex);
	gpio_free(nfc_dev->gpio.clkreq);
err_free_dwl_req:
	gpio_free(nfc_dev->gpio.dwl_req);
err_free_ven:
	gpio_free(nfc_dev->gpio.ven);
err_free_pages:
	if (nfc_dev->i3c_dev.buf.kbuf)
		free_pages((unsigned long)nfc_dev->i3c_dev.buf.kbuf, 0);
err_free_kbuf:
	kfree(nfc_dev->i3c_dev.read_kbuf);
err_free_nfc_dev:
	kfree(nfc_dev);
err:
	pr_err("%s: failed\n", __func__);
	return ret;
}

/** @brief   This API is automatically called on shutdown or crash.
 *
 *  @param device   the i3c_dev for the slave.
 *
 *  @return 0 on success, error code for failures.
 */
int nfc_i3c_dev_remove(struct i3c_device *device)
{
	struct nfc_dev *nfc_dev = i3cdev_get_drvdata(device);
	struct i3c_dev *i3c_dev = NULL;

	if (!nfc_dev) {
		pr_err("%s: device doesn't exist anymore\n", __func__);
		return -ENODEV;
	}
	i3c_dev = &nfc_dev->i3c_dev;

	i3c_device_free_ibi(device);

	if (i3c_dev->wq)
		destroy_workqueue(i3c_dev->wq);

	if (i3c_dev->buf.kbuf)
		free_pages((unsigned long)i3c_dev->buf.kbuf, 0);

	nfc_misc_remove(nfc_dev, DEV_COUNT);

	mutex_destroy(&nfc_dev->read_mutex);
	mutex_destroy(&nfc_dev->dev_ref_mutex);
	mutex_destroy(&i3c_dev->nci_buf_mutex);

	if (gpio_is_valid(nfc_dev->gpio.clkreq))
		gpio_free(nfc_dev->gpio.clkreq);

	if (gpio_is_valid(nfc_dev->gpio.dwl_req))
		gpio_free(nfc_dev->gpio.dwl_req);

	if (gpio_is_valid(nfc_dev->gpio.ven))
		gpio_free(nfc_dev->gpio.ven);

	kfree(i3c_dev->read_kbuf);

	kfree(nfc_dev);
	return 0;
}

int nfc_i3c_dev_suspend(struct device *pdev)
{
	struct i3c_device *device = dev_to_i3cdev(pdev);
	struct nfc_dev *nfc_dev = i3cdev_get_drvdata(device);

	pr_debug("%s: enter\n", __func__);
	if (!nfc_dev) {
		pr_err("%s: device doesn't exist anymore\n", __func__);
		return -ENODEV;
	}

	if (device_may_wakeup(&device->dev) && nfc_dev->i3c_dev.ibi_enabled)
		atomic_set(&nfc_dev->i3c_dev.pm_state, PM_STATE_SUSPEND);

	return 0;
}

int nfc_i3c_dev_resume(struct device *pdev)
{
	struct i3c_device *device = dev_to_i3cdev(pdev);
	struct nfc_dev *nfc_dev = i3cdev_get_drvdata(device);
	struct i3c_dev *i3c_dev = NULL;

	pr_debug("%s: enter\n", __func__);
	if (!nfc_dev) {
		pr_err("%s: device doesn't exist anymore\n", __func__);
		return -ENODEV;
	}
	i3c_dev = &nfc_dev->i3c_dev;
	if (device_may_wakeup(&device->dev)) {

		if (atomic_read(&i3c_dev->pm_state) ==
				PM_STATE_IBI_BEFORE_RESUME) {

			/*queue work in response to wakeup IBI */
			if (!queue_work(i3c_dev->wq, &i3c_dev->work))
				pr_debug("%s: Added workqueue successfully\n",
						__func__);
		}
		atomic_set(&i3c_dev->pm_state, PM_STATE_NORMAL);
	}

	return 0;
}

static const struct i3c_device_id nfc_i3c_dev_id[] = {
	I3C_DEVICE(NFC_I3C_MANU_ID, NFC_I3C_PART_ID, 0),
	{},
};

static const struct of_device_id nfc_i3c_dev_match_table[] = {
	{
		.compatible = NFC_I3C_DRV_STR
	},
	{}
};

static const struct dev_pm_ops nfc_i3c_dev_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(nfc_i3c_dev_suspend, nfc_i3c_dev_resume)
};

static struct i3c_driver nfc_i3c_dev_driver = {
	.id_table = nfc_i3c_dev_id,
	.probe = nfc_i3c_dev_probe,
	.remove = nfc_i3c_dev_remove,
	.driver = {
		   .owner = THIS_MODULE,
		   .name = NFC_I3C_DRV_STR,
		   .pm = &nfc_i3c_dev_pm_ops,
		   .of_match_table = nfc_i3c_dev_match_table,
		   .probe_type = PROBE_PREFER_ASYNCHRONOUS,
		   },
};

MODULE_DEVICE_TABLE(of, nfc_i3c_dev_match_table);

static int __init nfc_dev_i3c_init(void)
{
	int ret = 0;

	ret = i3c_driver_register_with_owner(&nfc_i3c_dev_driver, THIS_MODULE);
	if (ret != 0)
		pr_err("NFC I3C driver register error ret = %d\n", ret);
	return ret;
}

module_init(nfc_dev_i3c_init);

static void __exit nfc_i3c_dev_exit(void)
{
	pr_debug("Unloading NFC I3C driver\n");
	i3c_driver_unregister(&nfc_i3c_dev_driver);
}

module_exit(nfc_i3c_dev_exit);

MODULE_DESCRIPTION("QTI NFC I3C driver");
MODULE_LICENSE("GPL v2");
