/** @file hci_wrapper.h
 *  @brief This file contains HCI related definitions
 *
 *  Copyright (C) 2011-2012, Marvell International Ltd.
 *
 *  This software file (the "File") is distributed by Marvell International
 *  Ltd. under the terms of the GNU General Public License Version 2, June 1991
 *  (the "License").  You may use, redistribute and/or modify this File in
 *  accordance with the terms and conditions of the License, a copy of which
 *  is available along with the File in the gpl.txt file or by writing to
 *  the Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 *  02111-1307 or on the worldwide web at http://www.gnu.org/licenses/gpl.txt.
 *
 *  THE FILE IS DISTRIBUTED AS-IS, WITHOUT WARRANTY OF ANY KIND, AND THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE
 *  ARE EXPRESSLY DISCLAIMED.  The License provides additional details about
 *  this warranty disclaimer.
 *
 */

#ifndef _HCI_WRAPPER_H_
#define _HCI_WRAPPER_H_

#include <linux/module.h>
#include <net/bluetooth/bluetooth.h>
#include <net/bluetooth/hci_core.h>

/**  Define Seq num */
#define BT_SEQ      0
#define FM_SEQ      1
#define NFC_SEQ     2
#define DEBUG_SEQ   3

/** Define dev type */
#define BT_TYPE     1
#define BT_AMP_TYPE 2
#define FM_TYPE     3
#define NFC_TYPE    4
#define DEBUG_TYPE  5

/** Define spec type */
#define BLUEZ_SPEC     1
#define IANYWHERE_SPEC 2
#define GENERIC_SPEC   3

/** Define lock/unlock wrapper */
#define mdev_req_lock(d)		down(&d->req_lock)
#define mdev_req_unlock(d)		up(&d->req_lock)

/** Length of device name */
#define DEV_NAME_LEN				32

/** Define struct m_dev */
struct m_dev {
	char name[DEV_NAME_LEN];
	int index;
	unsigned long flags;
	spinlock_t lock;
	struct semaphore req_lock;
	struct sk_buff_head rx_q;
	wait_queue_head_t req_wait_q;
	struct hci_dev_stats stat;
	struct module *owner;
	void *dev_pointer;
	int dev_type;
	int spec_type;
	void *driver_data;
	int read_continue_flag;

	int (*open) (struct m_dev * m_dev);
	int (*close) (struct m_dev * m_dev);
	int (*flush) (struct m_dev * m_dev);
	int (*send) (struct m_dev * m_dev, struct sk_buff * skb);
	void (*destruct) (struct m_dev * m_dev);
	void (*notify) (struct m_dev * m_dev, unsigned int evt);
	int (*ioctl) (struct m_dev * m_dev, unsigned int cmd,
		      unsigned long arg);
	void (*query) (struct m_dev * m_dev, unsigned long arg);
	void (*poweroff) (struct m_dev * m_dev);

};

/** Define struct mbt_dev */
struct mbt_dev {
	/** maybe could add some private member later */
	char name[DEV_NAME_LEN];
	unsigned long flags;
	__u8 type;

	__u16 pkt_type;
	__u16 esco_type;
	__u16 link_policy;
	__u16 link_mode;

	__u32 idle_timeout;
	__u16 sniff_min_interval;
	__u16 sniff_max_interval;

	struct sk_buff *reassembly[3];

	atomic_t promisc;
};

/** Define 'fm' interface specific struct fm_dev */
struct fm_dev {
	/** maybe could add some private member later */
	char name[DEV_NAME_LEN];
	unsigned long flags;
};

/** Define 'nfc' interface specific struct fm_dev */
struct nfc_dev {
	/** maybe could add some private member later */
	char name[DEV_NAME_LEN];
	unsigned long flags;
};

struct debug_dev {
	/** maybe could add some private member later */
	char name[DEV_NAME_LEN];
	unsigned long flags;
};

/** This function frees m_dev allocation */
void free_m_dev(struct m_dev *m_dev);

/**
 *  @brief This function receives frames
 *
 *  @param skb	A pointer to struct sk_buff
 *  @return	0--success otherwise error code
 */
static inline int
mdev_recv_frame(struct sk_buff *skb)
{
	struct m_dev *m_dev = (struct m_dev *)skb->dev;
	if (!m_dev || (!test_bit(HCI_UP, &m_dev->flags)
		       && !test_bit(HCI_INIT, &m_dev->flags))) {
		kfree_skb(skb);
		return -ENXIO;
	}

	/* Incomming skb */
	bt_cb(skb)->incoming = 1;

	/* Time stamp */
	__net_timestamp(skb);

	/* Queue frame for rx task */
	skb_queue_tail(&m_dev->rx_q, skb);

	/* Wakeup rx thread */
	wake_up_interruptible(&m_dev->req_wait_q);

	return 0;
}

/**
 *  @brief mbt dev suspend handler
 *
 *  @param m_dev   A pointer to struct m_dev
 *  @return        0
 */
static inline int
mbt_hci_suspend_dev(struct m_dev *m_dev)
{
	return 0;
}

/**
 *  @brief mbt dev resume handler
 *
 *  @param m_dev   A pointer to struct m_dev
 *  @return        0
 */
static inline int
mbt_hci_resume_dev(struct m_dev *m_dev)
{
	return 0;
}

#endif /* _HCI_WRAPPER_H_ */
