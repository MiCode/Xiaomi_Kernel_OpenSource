/* -----------------------------------------------------------------------------
 * Copyright (c) 2011 Ozmo Inc
 * Released under the GNU General Public License Version 2 (GPLv2).
 * -----------------------------------------------------------------------------
 */
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/poll.h>
#include <linux/sched.h>
#include "ozprotocol.h"
#include "oztrace.h"
#include "ozappif.h"
#include "ozeltbuf.h"
#include "ozpd.h"
#include "ozproto.h"
#include "ozcdev.h"
#include "ozkobject.h"
/*------------------------------------------------------------------------------
 */
#define OZ_RD_BUF_SZ	256
#define OZ_MODE_TFTP	0x1
#define OZ_MODE_SERIAL	0x0

struct oz_cdev {
	dev_t devnum;
	struct cdev cdev;
	wait_queue_head_t rdq;
	spinlock_t lock;
	u8 active_addr[ETH_ALEN];
	struct oz_pd *active_pd;
	atomic_t ref_count;
	u8 mode;
};

/* Per PD context for the serial service stored in the PD. */
struct oz_serial_ctx {
	atomic_t ref_count;
	u8 tx_seq_num;
	u8 rx_seq_num;
	u8 tx_done_seq_num;
	u8 padding;
	u8 rd_buf[OZ_RD_BUF_SZ];
	int rd_in;
	int rd_out;
	spinlock_t rd_lock;
	int dg_len[OZ_RD_BUF_SZ/4];
	int dg_in;
	int dg_out;
};
/*------------------------------------------------------------------------------
 */
static struct oz_cdev g_cdev;
struct class *g_oz_class;
struct device *g_oz_wpan_dev;
/*------------------------------------------------------------------------------
 * Context: softirq
 */

static void oz_cdev_elt_completion_callback(struct oz_pd *pd, long context)
{
	struct oz_serial_ctx *ctx;
	spin_lock_bh(&pd->app_lock[OZ_APPID_SERIAL-1]);
	ctx = (struct oz_serial_ctx *)pd->app_ctx[OZ_APPID_SERIAL-1];
	if (ctx)
		ctx->tx_done_seq_num = (u8)context;
	spin_unlock_bh(&pd->app_lock[OZ_APPID_SERIAL-1]);
}
/*------------------------------------------------------------------------------
 * Context: process and softirq
 */
static struct oz_serial_ctx *oz_cdev_claim_ctx(struct oz_pd *pd)
{
	struct oz_serial_ctx *ctx;
	spin_lock_bh(&pd->app_lock[OZ_APPID_SERIAL-1]);
	ctx = (struct oz_serial_ctx *)pd->app_ctx[OZ_APPID_SERIAL-1];
	if (ctx)
		atomic_inc(&ctx->ref_count);
	spin_unlock_bh(&pd->app_lock[OZ_APPID_SERIAL-1]);
	return ctx;
}
/*------------------------------------------------------------------------------
 * Context: softirq or process
 */
static void oz_cdev_release_ctx(struct oz_serial_ctx *ctx)
{
	if (ctx &&  atomic_dec_and_test(&ctx->ref_count)) {
		oz_trace("Dealloc serial context.\n");
		kfree(ctx);
	}
}
/*------------------------------------------------------------------------------
 * Context: process
 */
int oz_cdev_open(struct inode *inode, struct file *filp)
{
	struct oz_cdev *dev;

	if (!atomic_add_unless(&g_cdev.ref_count, 1, 1))
		return -EBUSY;

	dev = container_of(inode->i_cdev, struct oz_cdev, cdev);
	filp->private_data = dev;
	return 0;
}
/*------------------------------------------------------------------------------
 * Context: process
 */
int oz_cdev_release(struct inode *inode, struct file *filp)
{
	atomic_dec(&g_cdev.ref_count);
	return 0;
}
/*------------------------------------------------------------------------------
 * Context: process
 */
ssize_t oz_cdev_read(struct file *filp, char __user *buf, size_t count,
		loff_t *fpos)
{
	int n;
	int ix;
	int is_tftp;
	struct oz_pd *pd;
	struct oz_serial_ctx *ctx;

	spin_lock_bh(&g_cdev.lock);
	pd = g_cdev.active_pd;
	if (pd)
		oz_pd_get(pd);
	is_tftp = (g_cdev.mode & OZ_MODE_TFTP) ? 1 : 0;
	spin_unlock_bh(&g_cdev.lock);
	if (pd == NULL)
		return -1;
	ctx = oz_cdev_claim_ctx(pd);
	if (ctx == NULL)
		goto out2;

	spin_lock_bh(&ctx->rd_lock);

	if (is_tftp) {
		/* if n is non zero we have a datagram */
		n = ctx->dg_len[ctx->dg_out];

		if (n == 0) {
			count = 0;
			spin_unlock_bh(&ctx->rd_lock);
			goto out1;
		}

		ix = ctx->rd_out;
		spin_unlock_bh(&ctx->rd_lock);

		/* copy n bytes of datagram to user bufer */
		if ((ix + n) < OZ_RD_BUF_SZ) {
			if (copy_to_user(buf, &ctx->rd_buf[ix], n)) {
				count = 0;
				goto out1;
			}
			spin_lock_bh(&ctx->rd_lock);
			ctx->rd_out += n;
		} else {
			int b = (OZ_RD_BUF_SZ - ix);
			/* datagram maybe split in between the end and start of
			 *  the buffer */
			if (copy_to_user(buf, &ctx->rd_buf[ix], b)) {
				count = 0;
				goto out1;
			}
			if (copy_to_user(&buf[b], ctx->rd_buf, n - b)) {
				count = 0;
				goto out1;
			}
			spin_lock_bh(&ctx->rd_lock);
			ctx->rd_out = n - (OZ_RD_BUF_SZ - ix);
		}

		count = n;

		ctx->dg_len[ctx->dg_out] = 0;
		ctx->dg_out++;
		if ((OZ_RD_BUF_SZ/4) == ctx->dg_out)
			ctx->dg_out = 0;
		spin_unlock_bh(&ctx->rd_lock);
	} else {
		n = ctx->rd_in - ctx->rd_out;
		if (n < 0)
			n += OZ_RD_BUF_SZ;
		if (count > n)
			count = n;

		ix = ctx->rd_out;
		spin_unlock_bh(&ctx->rd_lock);
		n = OZ_RD_BUF_SZ - ix;
		if (n > count)
			n = count;

		if (copy_to_user(buf, &ctx->rd_buf[ix], n)) {
			count = 0;
			goto out1;
		}

		ix += n;
		if (ix == OZ_RD_BUF_SZ)
			ix = 0;
		if (n < count) {
			if (copy_to_user(&buf[n], ctx->rd_buf, count-n)) {
				count = 0;
				goto out1;
			}
			ix = count-n;
		}

		spin_lock_bh(&ctx->rd_lock);
		ctx->rd_out = ix;
		spin_unlock_bh(&ctx->rd_lock);
	}
out1:
	oz_cdev_release_ctx(ctx);
out2:
	oz_pd_put(pd);
	return count;
}
/*------------------------------------------------------------------------------
 * Context: process
 */
ssize_t oz_cdev_write(struct file *filp, const char __user *buf, size_t count,
		loff_t *fpos)
{
	struct oz_pd *pd;
	struct oz_elt_buf *eb;
	struct oz_elt_info *ei;
	struct oz_elt *elt;
	struct oz_ext_elt *ext_elt;
	struct oz_app_hdr *app_hdr;
	struct oz_serial_ctx *ctx;

	if (count > sizeof(ei->data) - sizeof(*elt) - sizeof(*app_hdr))
		return -EINVAL;

	spin_lock_bh(&g_cdev.lock);
	pd = g_cdev.active_pd;
	if (pd)
		oz_pd_get(pd);
	spin_unlock_bh(&g_cdev.lock);
	if (pd == NULL)
		return -1;
	if (!(pd->state & OZ_PD_S_CONNECTED))
		return -ENXIO;
	eb = &pd->elt_buff;
	ei = oz_elt_info_alloc(eb);
	if (ei == NULL) {
		count = 0;
		goto out;
	}
	spin_lock_bh(&g_cdev.lock);
	if (g_cdev.mode & OZ_MODE_TFTP) {
		spin_unlock_bh(&g_cdev.lock);
		ei->app_id = OZ_APPID_SERIAL;
		ei->flags |= OZ_EI_F_EXT_ELM;
		ext_elt = (struct oz_ext_elt *)ei->data;
		app_hdr = (struct oz_app_hdr *)(ext_elt+1);
		app_hdr->app_id = OZ_APPID_TFTP;
		ext_elt->length = sizeof(struct oz_app_hdr) + count;
		ext_elt->type = OZ_ELT_APP_DATA_EX;
		ei->length = ext_elt->length + sizeof(struct oz_ext_elt);
		ext_elt->length = cpu_to_le16(ext_elt->length);
	} else {
		spin_unlock_bh(&g_cdev.lock);
		ei->app_id = OZ_APPID_SERIAL;
		elt = (struct oz_elt *)ei->data;
		app_hdr = (struct oz_app_hdr *)(elt+1);
		app_hdr->app_id = OZ_APPID_SERIAL;
		elt->length = sizeof(struct oz_app_hdr) + count;
		elt->type = OZ_ELT_APP_DATA;
		ei->length = elt->length + sizeof(struct oz_elt);
	}
	if (copy_from_user(app_hdr+1, buf, count))
		goto out;
	spin_lock_bh(&pd->app_lock[OZ_APPID_SERIAL-1]);
	ctx = (struct oz_serial_ctx *)pd->app_ctx[OZ_APPID_SERIAL-1];
	if (ctx) {
		app_hdr->elt_seq_num = ctx->tx_seq_num++;
		if (ctx->tx_seq_num == 0)
			ctx->tx_seq_num = 1;
		ei->callback = oz_cdev_elt_completion_callback;
		ei->context = ctx->tx_seq_num;
		spin_lock(&eb->lock);
		if (oz_queue_elt_info(eb, 0, 0, ei) == 0)
			ei = NULL;
		spin_unlock(&eb->lock);
	}
	spin_unlock_bh(&pd->app_lock[OZ_APPID_SERIAL-1]);
out:
	if (ei) {
		count = 0;
		spin_lock_bh(&eb->lock);
		oz_elt_info_free(eb, ei);
		spin_unlock_bh(&eb->lock);
	}
	oz_pd_put(pd);
	return count;
}
/*------------------------------------------------------------------------------
 * Context: process
 */
int oz_set_active_pd(const u8 *addr)
{
	int rc = 0;
	struct oz_pd *pd;
	struct oz_pd *old_pd;
	struct oz_serial_ctx *ctx;
	pd = oz_pd_find(addr);
	if (pd) {
		spin_lock_bh(&g_cdev.lock);
		if (memcmp(g_cdev.active_addr, addr, ETH_ALEN) == 0) {
			spin_unlock_bh(&g_cdev.lock);
			return rc;
		}
		memcpy(g_cdev.active_addr, addr, ETH_ALEN);
		old_pd = g_cdev.active_pd;
		g_cdev.active_pd = pd;
		spin_unlock_bh(&g_cdev.lock);

		/*Reset buffer pointers if new device is selected*/
		ctx = oz_cdev_claim_ctx(pd);
		if (ctx != NULL) {
			spin_lock_bh(&ctx->rd_lock);
			ctx->dg_in = 0;
			ctx->dg_out = 0;
			ctx->dg_len[0] = 0;
			ctx->rd_out = 0;
			ctx->rd_in = 0;
			spin_unlock_bh(&ctx->rd_lock);
		}

		if (old_pd)
			oz_pd_put(old_pd);
	} else {
		if (is_zero_ether_addr(addr)) {
			spin_lock_bh(&g_cdev.lock);
			pd = g_cdev.active_pd;
			g_cdev.active_pd = NULL;
			memset(g_cdev.active_addr, 0,
				sizeof(g_cdev.active_addr));
			spin_unlock_bh(&g_cdev.lock);
			if (pd)
				oz_pd_put(pd);
		} else {
			rc = -1;
		}
	}
	return rc;
}
/*------------------------------------------------------------------------------
 * Context: process
 */
void oz_get_active_pd(u8 *addr)
{
	spin_lock_bh(&g_cdev.lock);
	memcpy(addr, g_cdev.active_addr, ETH_ALEN);
	spin_unlock_bh(&g_cdev.lock);

}
/*------------------------------------------------------------------------------
 * Context: process
 */
u8 oz_get_serial_mode(void)
{
	u8 serial_mode;

	spin_lock_bh(&g_cdev.lock);
	serial_mode = g_cdev.mode;
	spin_unlock_bh(&g_cdev.lock);
	return serial_mode;
}
/*------------------------------------------------------------------------------
 * Context: process
 */
void oz_set_serial_mode(u8 mode)
{
	u8 addr[ETH_ALEN];
	struct oz_pd *pd;
	struct oz_serial_ctx *ctx;

	oz_get_active_pd(addr);
	pd = oz_pd_find(addr);
	if (!pd)
		return;
	ctx = oz_cdev_claim_ctx(pd);
	if (!ctx) {
		oz_pd_put(pd);
		return;
	}
	spin_lock_bh(&g_cdev.lock);
	g_cdev.mode = mode;
	spin_unlock_bh(&g_cdev.lock);
	oz_cdev_release_ctx(ctx);
	oz_pd_put(pd);
}
/*------------------------------------------------------------------------------
 * Context: process
 */
long oz_cdev_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int rc = 0;
	if (_IOC_TYPE(cmd) != OZ_IOCTL_MAGIC)
		return -ENOTTY;
	if (_IOC_NR(cmd) > OZ_IOCTL_MAX)
		return -ENOTTY;
	if (_IOC_DIR(cmd) & _IOC_READ)
		rc = !access_ok(VERIFY_WRITE, (void __user *)arg,
			_IOC_SIZE(cmd));
	else if (_IOC_DIR(cmd) & _IOC_WRITE)
		rc = !access_ok(VERIFY_READ, (void __user *)arg,
			_IOC_SIZE(cmd));
	if (rc)
		return -EFAULT;
	switch (cmd) {
	case OZ_IOCTL_GET_PD_LIST: {
			struct oz_pd_list list;
			memset(&list, 0, sizeof(list));
			list.count = oz_get_pd_list(list.addr, OZ_MAX_PDS);
			if (copy_to_user((void __user *)arg, &list,
				sizeof(list)))
				return -EFAULT;
		}
		break;
	case OZ_IOCTL_SET_ACTIVE_PD: {
			u8 addr[ETH_ALEN];
			if (copy_from_user(addr, (void __user *)arg, ETH_ALEN))
				return -EFAULT;
			rc = oz_set_active_pd(addr);
		}
		break;
	case OZ_IOCTL_GET_ACTIVE_PD: {
			u8 addr[ETH_ALEN];
			spin_lock_bh(&g_cdev.lock);
			memcpy(addr, g_cdev.active_addr, ETH_ALEN);
			spin_unlock_bh(&g_cdev.lock);
			if (copy_to_user((void __user *)arg, addr, ETH_ALEN))
				return -EFAULT;
		}
		break;
	case OZ_IOCTL_ADD_BINDING:
	case OZ_IOCTL_REMOVE_BINDING: {
			struct oz_binding_info b;
			if (copy_from_user(&b, (void __user *)arg,
				sizeof(struct oz_binding_info))) {
				return -EFAULT;
			}
			/* Make sure name is null terminated. */
			b.name[OZ_MAX_BINDING_LEN-1] = 0;
			if (cmd == OZ_IOCTL_ADD_BINDING)
				oz_binding_add(b.name);
			else
				oz_binding_remove(b.name);
		}
		break;
	}
	return rc;
}
/*------------------------------------------------------------------------------
 * Context: process
 */
unsigned int oz_cdev_poll(struct file *filp, poll_table *wait)
{
	unsigned int ret = 0;
	struct oz_cdev *dev = filp->private_data;
	spin_lock_bh(&dev->lock);
	if (dev->active_pd) {
		struct oz_serial_ctx *ctx = oz_cdev_claim_ctx(dev->active_pd);
		spin_unlock_bh(&dev->lock);
		if (ctx) {
			spin_lock_bh(&ctx->rd_lock);
			if (ctx->rd_in != ctx->rd_out)
				ret |= POLLIN | POLLRDNORM;

			if (ctx->tx_seq_num == ctx->tx_done_seq_num)
				ret |= POLLOUT;
			spin_unlock_bh(&ctx->rd_lock);
			oz_cdev_release_ctx(ctx);
		}
	} else
		spin_unlock_bh(&dev->lock);

	if (wait)
		poll_wait(filp, &dev->rdq, wait);
	return ret;
}
/*------------------------------------------------------------------------------
 */
const struct file_operations oz_fops = {
	.owner =	THIS_MODULE,
	.open =		oz_cdev_open,
	.release =	oz_cdev_release,
	.read =		oz_cdev_read,
	.write =	oz_cdev_write,
	.unlocked_ioctl = oz_cdev_ioctl,
	.poll =		oz_cdev_poll
};
/*------------------------------------------------------------------------------
 * Context: process
 */
int oz_cdev_register(void)
{
	int err;
	memset(&g_cdev, 0, sizeof(g_cdev));
	err = alloc_chrdev_region(&g_cdev.devnum, 0, 1, "ozwpan");
	if (err < 0)
		goto out3;
	cdev_init(&g_cdev.cdev, &oz_fops);
	g_cdev.cdev.owner = THIS_MODULE;
	g_cdev.cdev.ops = &oz_fops;
	spin_lock_init(&g_cdev.lock);
	init_waitqueue_head(&g_cdev.rdq);
	err = cdev_add(&g_cdev.cdev, g_cdev.devnum, 1);
	if (err < 0) {
		oz_trace("Failed to add cdev\n");
		goto out2;
	}
	g_oz_class = class_create(THIS_MODULE, "ozmo_wpan");
	if (IS_ERR(g_oz_class)) {
		oz_trace("Failed to register ozmo_wpan class\n");
		err = PTR_ERR(g_oz_class);
		goto out1;
	}
	g_oz_wpan_dev = device_create(g_oz_class, NULL, g_cdev.devnum, NULL,
								"ozwpan");
	if (IS_ERR(g_oz_wpan_dev)) {
		oz_trace("Failed to create sysfs entry for cdev\n");
		err = PTR_ERR(g_oz_wpan_dev);
		goto out1;
	}
	oz_create_sys_entry();
	return 0;
out1:
	cdev_del(&g_cdev.cdev);
out2:
	unregister_chrdev_region(g_cdev.devnum, 1);
out3:
	return err;
}
/*------------------------------------------------------------------------------
 * Context: process
 */
int oz_cdev_deregister(void)
{
	cdev_del(&g_cdev.cdev);
	unregister_chrdev_region(g_cdev.devnum, 1);
	if (g_oz_class) {
		oz_destroy_sys_entry();
		device_destroy(g_oz_class, g_cdev.devnum);
		class_destroy(g_oz_class);
	}
	return 0;
}
/*------------------------------------------------------------------------------
 * Context: process
 */
int oz_cdev_init(void)
{
	oz_app_enable(OZ_APPID_SERIAL, 1);
	return 0;
}
/*------------------------------------------------------------------------------
 * Context: process
 */
void oz_cdev_term(void)
{
	oz_app_enable(OZ_APPID_SERIAL, 0);
}
/*------------------------------------------------------------------------------
 * Context: softirq-serialized
 */
int oz_cdev_start(struct oz_pd *pd, int resume)
{
	struct oz_serial_ctx *ctx;
	struct oz_serial_ctx *old_ctx;
	if (resume) {
		oz_trace("Serial service resumed.\n");
		return 0;
	}
	ctx = kzalloc(sizeof(struct oz_serial_ctx), GFP_ATOMIC);
	if (ctx == NULL)
		return -ENOMEM;
	atomic_set(&ctx->ref_count, 1);
	ctx->tx_seq_num = 1;
	ctx->tx_done_seq_num = 1;
	spin_lock_bh(&pd->app_lock[OZ_APPID_SERIAL-1]);
	old_ctx = pd->app_ctx[OZ_APPID_SERIAL-1];
	if (old_ctx) {
		spin_unlock_bh(&pd->app_lock[OZ_APPID_SERIAL-1]);
		kfree(ctx);
	} else {
		pd->app_ctx[OZ_APPID_SERIAL-1] = ctx;
		spin_unlock_bh(&pd->app_lock[OZ_APPID_SERIAL-1]);
	}
	spin_lock(&g_cdev.lock);
	if ((g_cdev.active_pd == NULL) &&
		(memcmp(pd->mac_addr, g_cdev.active_addr, ETH_ALEN) == 0)) {
		oz_pd_get(pd);
		g_cdev.active_pd = pd;
	}
	spin_unlock(&g_cdev.lock);
	oz_trace("Serial service started.\n");
	return 0;
}
/*------------------------------------------------------------------------------
 * Context: softirq or process
 */
void oz_cdev_stop(struct oz_pd *pd, int pause)
{
	struct oz_serial_ctx *ctx;
	if (pause) {
		oz_trace("Serial service paused.\n");
		return;
	}
	spin_lock_bh(&pd->app_lock[OZ_APPID_SERIAL-1]);
	ctx = (struct oz_serial_ctx *)pd->app_ctx[OZ_APPID_SERIAL-1];
	pd->app_ctx[OZ_APPID_SERIAL-1] = NULL;
	spin_unlock_bh(&pd->app_lock[OZ_APPID_SERIAL-1]);
	if (ctx)
		oz_cdev_release_ctx(ctx);
	spin_lock(&g_cdev.lock);
	if (pd == g_cdev.active_pd)
		g_cdev.active_pd = NULL;
	else
		pd = NULL;
	spin_unlock(&g_cdev.lock);
	if (pd) {
		oz_pd_put(pd);
	}
	oz_trace("Serial service stopped.\n");
}
/*------------------------------------------------------------------------------
 * Context: softirq-serialized
 */
void oz_cdev_rx(struct oz_pd *pd, struct oz_elt *elt)
{
	struct oz_serial_ctx *ctx;
	struct oz_app_hdr *app_hdr;
	u8 *data;
	int len;
	int space;
	int copy_sz;
	int ix;
	int is_tftp;

	spin_lock_bh(&g_cdev.lock);
	is_tftp = (g_cdev.mode & OZ_MODE_TFTP) ? 1 : 0;
	spin_unlock_bh(&g_cdev.lock);

	ctx = oz_cdev_claim_ctx(pd);
	if (ctx == NULL) {
		oz_trace("Cannot claim serial context.\n");
		return;
	}

	app_hdr = (struct oz_app_hdr *)(oz_elt_data(elt));
	/* If sequence number is non-zero then check it is not a duplicate.
	 */
	if (app_hdr->elt_seq_num != 0) {
		if (((ctx->rx_seq_num - app_hdr->elt_seq_num) & 0x80) == 0) {
			/* Reject duplicate element. */
			oz_trace("Duplicate element:%02x %02x\n",
				app_hdr->elt_seq_num, ctx->rx_seq_num);
			goto out;
		}
	}
	ctx->rx_seq_num = app_hdr->elt_seq_num;
	len = oz_elt_data_len(elt) - sizeof(struct oz_app_hdr);
	data = ((u8 *)(app_hdr + 1));
	if (len <= 0)
		goto out;
	spin_lock_bh(&ctx->rd_lock);
	space = ctx->rd_out - ctx->rd_in - 1;
	if (space < 0)
		space += OZ_RD_BUF_SZ;
	if (len > space) {
		oz_trace("Not enough space:%d %d\n", len, space);
		len = space;
	}


	if (is_tftp) {
		if (len != 0) {
			/* remember length of datagram */
			ctx->dg_len[ctx->dg_in] = len;

			ctx->dg_in++;
			if ((OZ_RD_BUF_SZ/4) == ctx->dg_in)
				ctx->dg_in = 0;
		}
	}

	ix = ctx->rd_in;
	copy_sz = OZ_RD_BUF_SZ - ix;
	if (copy_sz > len)
		copy_sz = len;
	memcpy(&ctx->rd_buf[ix], data, copy_sz);
	len -= copy_sz;
	ix += copy_sz;
	if (ix == OZ_RD_BUF_SZ)
		ix = 0;
	if (len) {
		memcpy(ctx->rd_buf, data+copy_sz, len);
		ix = len;
	}
	ctx->rd_in = ix;
	spin_unlock_bh(&ctx->rd_lock);
	wake_up(&g_cdev.rdq);
out:
	oz_cdev_release_ctx(ctx);
}
/*------------------------------------------------------------------------------
 * Context: softirq
 */
void oz_cdev_heartbeat(struct oz_pd *pd)
{
}
