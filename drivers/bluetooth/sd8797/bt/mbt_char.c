/** @file mbt_char.c
  *
  * @brief This file contains the char device function calls
  *
  * Copyright (C) 2010-2012, Marvell International Ltd.
  *
  * This software file (the "File") is distributed by Marvell International
  * Ltd. under the terms of the GNU General Public License Version 2, June 1991
  * (the "License").  You may use, redistribute and/or modify this File in
  * accordance with the terms and conditions of the License, a copy of which
  * is available by writing to the Free Software Foundation, Inc.,
  * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA or on the
  * worldwide web at http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt.
  *
  * THE FILE IS DISTRIBUTED AS-IS, WITHOUT WARRANTY OF ANY KIND, AND THE
  * IMPLIED WARRANTIES OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE
  * ARE EXPRESSLY DISCLAIMED.  The License provides additional details about
  * this warranty disclaimer.
  *
  */

#include <linux/path.h>
#include <linux/namei.h>
#include <linux/mount.h>

#include "bt_drv.h"
#include "mbt_char.h"

LIST_HEAD(char_dev_list);

static DEFINE_SPINLOCK(char_dev_list_lock);

static int mbtchar_major = MBTCHAR_MAJOR_NUM;

/**
 *	@brief Changes permissions of the dev
 *
 *	@param name	pointer to character
 *	@param mode		mode_t type data
 *	@return			0--success otherwise failure
 */
int
mbtchar_chmod(char *name, mode_t mode)
{
	struct path path;
	struct inode *inode;
	struct iattr newattrs;
	int ret;
	int retrycount = 0;

	ENTER();
	do {
		os_sched_timeout(30);
		ret = kern_path(name, LOOKUP_FOLLOW, &path);
		if (++retrycount >= 10) {
			PRINTM(ERROR,
			       "mbtchar_chmod(): fail to get kern_path\n");
			LEAVE();
			return -EFAULT;
		}
	} while (ret);
	inode = path.dentry->d_inode;

	mutex_lock(&inode->i_mutex);
	ret = mnt_want_write(path.mnt);
	if (ret)
		goto out_unlock;
	newattrs.ia_mode = (mode & S_IALLUGO) | (inode->i_mode & ~S_IALLUGO);
	newattrs.ia_valid = ATTR_MODE | ATTR_CTIME;
	if (inode->i_op->setattr)
		ret = inode->i_op->setattr(path.dentry, &newattrs);
	else
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 35)
		ret = simple_setattr(path.dentry, &newattrs);
#else
		ret = inode_setattr(inode, &newattrs);
#endif

	mutex_unlock(&inode->i_mutex);
	mnt_drop_write(path.mnt);

	path_put(&path);
	LEAVE();
	return ret;
out_unlock:
	mutex_unlock(&inode->i_mutex);
	mnt_drop_write(path.mnt);
	path_put(&path);
	return ret;
}

/**
 *	@brief Changes ownership of the dev
 *
 *	@param name	pointer to character
 *	@param user		uid_t type data
 *	@param group	gid_t type data
 *	@return			0--success otherwise failure
 */
int
mbtchar_chown(char *name, uid_t user, gid_t group)
{
	struct path path;
	struct inode *inode = NULL;
	struct iattr newattrs;
	int ret = 0;
	int retrycount = 0;

	ENTER();
	do {
		os_sched_timeout(30);
		ret = kern_path(name, LOOKUP_FOLLOW, &path);
		if (++retrycount >= 10) {
			PRINTM(ERROR,
			       "mbtchar_chown(): fail to get kern_path\n");
			LEAVE();
			return -EFAULT;
		}
	} while (ret);
	inode = path.dentry->d_inode;
	mutex_lock(&inode->i_mutex);
	ret = mnt_want_write(path.mnt);
	if (ret)
		goto out_unlock;
	newattrs.ia_valid = ATTR_CTIME;
	if (user != (uid_t) (-1)) {
		newattrs.ia_valid |= ATTR_UID;
		newattrs.ia_uid = user;
	}
	if (group != (gid_t) (-1)) {
		newattrs.ia_valid |= ATTR_GID;
		newattrs.ia_gid = group;
	}
	if (!S_ISDIR(inode->i_mode))
		newattrs.ia_valid |=
			ATTR_KILL_SUID | ATTR_KILL_SGID | ATTR_KILL_PRIV;
	if (inode->i_op->setattr)
		ret = inode->i_op->setattr(path.dentry, &newattrs);
	else
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 35)
		ret = simple_setattr(path.dentry, &newattrs);
#else
		ret = inode_setattr(inode, &newattrs);
#endif

	mutex_unlock(&inode->i_mutex);
	mnt_drop_write(path.mnt);

	path_put(&path);
	LEAVE();
	return ret;
out_unlock:
	mutex_unlock(&inode->i_mutex);
	mnt_drop_write(path.mnt);
	path_put(&path);
	return ret;
}

/**
 *	@brief write handler for char dev
 *
 *	@param filp	pointer to structure file
 *	@param buf		pointer to char buffer
 *	@param count	size of receive buffer
 *	@param f_pos	pointer to loff_t type data
 *	@return			number of bytes written
 */
ssize_t
chardev_write(struct file * filp, const char *buf, size_t count, loff_t * f_pos)
{
	int nwrite = 0;
	struct sk_buff *skb;
	struct char_dev *dev = (struct char_dev *)filp->private_data;
	struct m_dev *m_dev = dev->m_dev;
	ENTER();

	if (!dev || !dev->m_dev) {
		LEAVE();
		return -ENXIO;
	}
	nwrite = count;
	skb = bt_skb_alloc(count, GFP_ATOMIC);
	if (!skb) {
		PRINTM(ERROR, "mbtchar_write(): fail to alloc skb\n");
		LEAVE();
		return -ENOMEM;
	}

	if (copy_from_user((void *)skb_put(skb, count), buf, count)) {
		PRINTM(ERROR, "mbtchar_write(): cp_from_user failed\n");
		kfree_skb(skb);
		nwrite = -EFAULT;
		goto exit;
	}

	skb->dev = (void *)m_dev;
	bt_cb(skb)->pkt_type = *((unsigned char *)skb->data);
	skb_pull(skb, 1);

	PRINTM(DATA, "Write: pkt_type: 0x%x, len=%d @%lu\n",
	       bt_cb(skb)->pkt_type, skb->len, jiffies);
	DBG_HEXDUMP(DAT_D, "chardev_write", skb->data, skb->len);

	/* Send skb to the hci wrapper layer */
	if (m_dev->send(m_dev, skb)) {
		PRINTM(ERROR, "Write: Fail\n");
		nwrite = 0;
		/* Send failed */
		kfree_skb(skb);
	}
exit:
	LEAVE();
	return nwrite;
}

/**
 *	@brief read handler for BT char dev
 *
 *	@param filp	pointer to structure file
 *	@param buf		pointer to char buffer
 *	@param count	size of receive buffer
 *	@param f_pos	pointer to loff_t type data
 *	@return			number of bytes read
 */
ssize_t
chardev_read(struct file * filp, char *buf, size_t count, loff_t * f_pos)
{
	struct char_dev *dev = (struct char_dev *)filp->private_data;
	struct m_dev *m_dev = dev->m_dev;
	DECLARE_WAITQUEUE(wait, current);
	ssize_t ret = 0;
	struct sk_buff *skb = NULL;

	ENTER();
	if (!dev || !dev->m_dev) {
		LEAVE();
		return -ENXIO;
	}
	/* Wait for rx data */
	add_wait_queue(&m_dev->req_wait_q, &wait);
	while (1) {
		set_current_state(TASK_INTERRUPTIBLE);
		skb = skb_dequeue(&m_dev->rx_q);
		if (skb)
			break;
		if (!test_bit(HCI_UP, &m_dev->flags)) {
			ret = -EBUSY;
			break;
		}

		if (filp->f_flags & O_NONBLOCK) {
			ret = -EAGAIN;
			break;
		}
		if (signal_pending(current)) {
			ret = -EINTR;
			break;
		}
		schedule();
	}
	set_current_state(TASK_RUNNING);
	remove_wait_queue(&m_dev->req_wait_q, &wait);

	if (!skb)
		goto out;

	if (m_dev->read_continue_flag == 0) {
		/* Put type byte before the data */
		memcpy(skb_push(skb, 1), &bt_cb(skb)->pkt_type, 1);
		PRINTM(DATA, "Read: pkt_type: 0x%x, len=%d @%lu\n",
		       bt_cb(skb)->pkt_type, skb->len, jiffies);
	}
	DBG_HEXDUMP(DAT_D, "chardev_read", skb->data, skb->len);
	if (skb->len > count) {
		/* user data length is smaller than the skb length */
		if (copy_to_user(buf, skb->data, count)) {
			ret = -EFAULT;
			goto outf;
		}
		skb_pull(skb, count);
		skb_queue_head(&m_dev->rx_q, skb);
		m_dev->read_continue_flag = 1;
		wake_up_interruptible(&m_dev->req_wait_q);
		ret = count;
		goto out;
	} else {
		if (copy_to_user(buf, skb->data, skb->len)) {
			ret = -EFAULT;
			goto outf;
		}
		m_dev->read_continue_flag = 0;
		ret = skb->len;
	}
outf:
	kfree_skb(skb);
out:
	LEAVE();
	return ret;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 36)
/**
 *	@brief ioctl handler for char dev
 *
 *	@param inode	pointer to structure inode
 *	@param filp	pointer to structure file
 *	@param cmd		contains the IOCTL
 *	@param arg		contains the arguement
 *	@return			0--success otherwise failure
 */
int
chardev_ioctl(struct inode *inode, struct file *filp,
	      unsigned int cmd, unsigned long arg)
#else
/**
 *	@brief ioctl handler for char dev
 *
 *	@param filp	pointer to structure file
 *	@param cmd		contains the IOCTL
 *	@param arg		contains the arguement
 *	@return			0--success otherwise failure
 */
long
chardev_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
#endif
{
	struct char_dev *dev = (struct char_dev *)filp->private_data;
	struct m_dev *m_dev = NULL;
	ENTER();
	if (!dev || !dev->m_dev) {
		LEAVE();
		return -ENXIO;
	}
	m_dev = dev->m_dev;
	PRINTM(INFO, "IOCTL: cmd=%d\n", cmd);
	switch (cmd) {
	case MBTCHAR_IOCTL_RELEASE:
		m_dev->close(m_dev);
		break;
	case MBTCHAR_IOCTL_QUERY_TYPE:
		m_dev->query(m_dev, arg);
		break;
	default:
		break;
	}
	LEAVE();
	return 0;
}

/**
 *	@brief open handler for char dev
 *
 *	@param inode	pointer to structure inode
 *	@param filp	pointer to structure file
 *	@return			0--success otherwise failure
 */
int
chardev_open(struct inode *inode, struct file *filp)
{
	int ret = 0;
	struct char_dev *dev = NULL;
	struct m_dev *m_dev = NULL;
	ENTER();

	dev = container_of(inode->i_cdev, struct char_dev, cdev);
	if (!dev->m_dev) {
		ret = -ENXIO;
		goto done;
	}
	filp->private_data = dev;	/* for other methods */
	m_dev = dev->m_dev;
	mdev_req_lock(m_dev);
	if (test_bit(HCI_UP, &m_dev->flags)) {
		ret = -EALREADY;
		goto done;
	}
	if (m_dev->open(m_dev)) {
		ret = -EIO;
		goto done;
	}
	set_bit(HCI_UP, &m_dev->flags);

done:
	mdev_req_unlock(m_dev);
	LEAVE();
	return ret;
}

/**
 *	@brief release handler for char dev
 *
 *	@param inode	pointer to structure inode
 *	@param filp	pointer to structure file
 *	@return			0--success otherwise failure
 */
int
chardev_release(struct inode *inode, struct file *filp)
{
	int ret = 0;
	struct char_dev *dev = (struct char_dev *)filp->private_data;
	ENTER();
	if (!dev || !dev->m_dev) {
		LEAVE();
		return -ENXIO;
	}
	ret = dev->m_dev->close(dev->m_dev);
	filp->private_data = NULL;
	LEAVE();
	return ret;
}

/**
 *	@brief poll handler for char dev
 *
 *	@param filp	pointer to structure file
 *	@param wait		pointer to poll_table structure
 *	@return			mask
 */
static unsigned int
chardev_poll(struct file *filp, poll_table * wait)
{
	unsigned int mask;
	struct char_dev *dev = (struct char_dev *)filp->private_data;
	struct m_dev *m_dev = NULL;
	ENTER();
	if (!dev || !dev->m_dev) {
		LEAVE();
		return -ENXIO;
	}

	m_dev = dev->m_dev;
	poll_wait(filp, &m_dev->req_wait_q, wait);
	mask = POLLOUT | POLLWRNORM;
	if (skb_peek(&m_dev->rx_q))
		mask |= POLLIN | POLLRDNORM;
	if (!test_bit(HCI_UP, &(m_dev->flags)))
		mask |= POLLHUP;
	PRINTM(INFO, "poll mask=0x%x\n", mask);
	LEAVE();
	return mask;
}

/* File ops for the Char driver */
const struct file_operations chardev_fops = {
	.read = chardev_read,
	.write = chardev_write,
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 36)
	.ioctl = chardev_ioctl,
#else
	.unlocked_ioctl = chardev_ioctl,
#endif
	.open = chardev_open,
	.release = chardev_release,
	.poll = chardev_poll,
};

/**
 *	@brief This function creates the char dev
 *
 *	@param dev			A pointer to structure char_dev
 *  @param char_class	A pointer to class struct
 *  @param mod_name		A pointer to char
 *  @param dev_name		A pointer to char
 *	@return				0--success otherwise failure
 */
int
register_char_dev(struct char_dev *dev, struct class *char_class,
		  char *mod_name, char *dev_name)
{
	int ret = 0, dev_num;
	unsigned long flags;
	ENTER();
	/* create the chrdev region */
	if (mbtchar_major) {
		dev_num = MKDEV(mbtchar_major, dev->minor);
		ret = register_chrdev_region(dev_num, 1, mod_name);
	} else {
		PRINTM(INFO, "chardev: no major # yet\n");
		ret = alloc_chrdev_region(&dev_num, dev->minor, 1, mod_name);
	}

	if (ret) {
		PRINTM(ERROR, "chardev: create chrdev_region failed\n");
		LEAVE();
		return ret;
	}
	if (!mbtchar_major) {
		/* Store the allocated dev major # */
		mbtchar_major = MAJOR(dev_num);
	}
	cdev_init(&dev->cdev, &chardev_fops);
	dev->cdev.owner = THIS_MODULE;
	dev->cdev.ops = &chardev_fops;
	dev_num = MKDEV(mbtchar_major, dev->minor);

	if (cdev_add(&dev->cdev, dev_num, 1)) {
		PRINTM(ERROR, "chardev: cdev_add failed\n");
		ret = -EFAULT;
		goto free_cdev_region;
	}
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 26)
	if ((dev->dev_type == BT_TYPE) || (dev->dev_type == BT_AMP_TYPE)) {
		device_create(char_class, NULL,
			      MKDEV(mbtchar_major, dev->minor), NULL, dev_name);
	}
	if (dev->dev_type == FM_TYPE) {
		device_create(char_class, NULL,
			      MKDEV(mbtchar_major, dev->minor), NULL, dev_name);
	}
	if (dev->dev_type == NFC_TYPE) {
		device_create(char_class, NULL,
			      MKDEV(mbtchar_major, dev->minor), NULL, dev_name);
	}
	if (dev->dev_type == DEBUG_TYPE) {
		device_create(char_class, NULL,
			      MKDEV(mbtchar_major, dev->minor), NULL, dev_name);
	}
#else
	if ((dev->dev_type == BT_TYPE) || (dev->dev_type == BT_AMP_TYPE)) {
		device_create(char_class, NULL,
			      MKDEV(mbtchar_major, dev->minor), dev_name);
	}
	if (dev->dev_type == FM_TYPE) {
		device_create(char_class, NULL,
			      MKDEV(mbtchar_major, dev->minor), dev_name);
	}
	if (dev->dev_type == NFC_TYPE) {
		device_create(char_class, NULL,
			      MKDEV(mbtchar_major, dev->minor), dev_name);
	}
	if (dev->dev_type == DEBUG_TYPE) {
		device_create(char_class, NULL,
			      MKDEV(mbtchar_major, dev->minor), dev_name);
	}
#endif
	PRINTM(INFO, "register char dev=%s\n", dev_name);

	/** modify later */

	spin_lock_irqsave(&char_dev_list_lock, flags);
	list_add_tail(&dev->list, &char_dev_list);
	spin_unlock_irqrestore(&char_dev_list_lock, flags);

	LEAVE();
	return ret;
free_cdev_region:
	unregister_chrdev_region(MKDEV(mbtchar_major, dev->minor), 1);
	LEAVE();
	return ret;
}

/**
 *	@brief This function deletes the char dev
 *
 *  @param dev			A pointer to structure char_dev
 *  @param char_class	A pointer to class struct
 *  @param dev_name		A pointer to char
 *  @return				0--success otherwise failure
 */
int
unregister_char_dev(struct char_dev *dev, struct class *char_class,
		    char *dev_name)
{
	ENTER();
	device_destroy(char_class, MKDEV(mbtchar_major, dev->minor));
	cdev_del(&dev->cdev);
	unregister_chrdev_region(MKDEV(mbtchar_major, dev->minor), 1);
	PRINTM(INFO, "unregister char dev=%s\n", dev_name);

	LEAVE();
	return 0;
}

/**
 *	@brief This function cleans module
 *
 *  @param char_class	A pointer to class struct
 *  @return				N/A
 */
void
chardev_cleanup(struct class *char_class)
{
	unsigned long flags;
	struct list_head *p = NULL;
	struct char_dev *dev = NULL;
	ENTER();
	spin_lock_irqsave(&char_dev_list_lock, flags);
	do {
		dev = NULL;
		list_for_each(p, &char_dev_list) {
			dev = list_entry(p, struct char_dev, list);
			list_del(p);
			spin_unlock_irqrestore(&char_dev_list_lock, flags);
			unregister_char_dev(dev, char_class, dev->m_dev->name);
			kfree(dev);
			spin_lock_irqsave(&char_dev_list_lock, flags);
			break;
		}
	} while (dev);
	spin_unlock_irqrestore(&char_dev_list_lock, flags);
	class_destroy(char_class);
	LEAVE();
}

/**
 *	@brief This function cleans module
 *
 *  @param m_dev	A pointer to m_dev struct
 *  @param char_class	A pointer to class struct
 *  @return			N/A
 */
void
chardev_cleanup_one(struct m_dev *m_dev, struct class *char_class)
{
	unsigned long flags;
	struct list_head *p = NULL;
	struct char_dev *dev = NULL;
	ENTER();
	spin_lock_irqsave(&char_dev_list_lock, flags);
	list_for_each(p, &char_dev_list) {
		dev = list_entry(p, struct char_dev, list);
		if (dev->minor == m_dev->index) {
			list_del(p);
			spin_unlock_irqrestore(&char_dev_list_lock, flags);
			dev->m_dev = NULL;
			unregister_char_dev(dev, char_class, m_dev->name);
			kfree(dev);
			spin_lock_irqsave(&char_dev_list_lock, flags);
			break;
		}
	}
	spin_unlock_irqrestore(&char_dev_list_lock, flags);
	LEAVE();
}
