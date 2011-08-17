/* 
* Copyright (C) ST-Ericsson AP Pte Ltd 2010 
*
* ISP1763 Linux OTG Controller driver : host
* 
* This program is free software; you can redistribute it and/or modify it under the terms of 
* the GNU General Public License as published by the Free Software Foundation; version 
* 2 of the License. 
* 
* This program is distributed in the hope that it will be useful, but WITHOUT ANY  
* WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS  
* FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more  
* details. 
* 
* You should have received a copy of the GNU General Public License 
* along with this program; if not, write to the Free Software 
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA. 
* 
* Refer to file ~/drivers/usb/host/ehci-dbg.h for copyright owners (kernel version 2.6.9)
* Code is modified for ST-Ericsson product 
* 
* Author : wired support <wired.support@stericsson.com>
*
*/

#ifndef	__PEHCI_H__
#define	__PEHCI_H__


#define	DRIVER_AUTHOR	"ST-ERICSSON	  "
#define	DRIVER_DESC "ISP1763 'Enhanced'	Host Controller	(EHCI) Driver"

/*    bus related stuff	*/
#define	__ACTIVE		0x01
#define	__SLEEPY		0x02
#define	__SUSPEND		0x04
#define	__TRANSIENT		0x80

#define	USB_STATE_HALT		0
#define	USB_STATE_RUNNING	(__ACTIVE)
#define	USB_STATE_READY		(__ACTIVE|__SLEEPY)
#define	USB_STATE_QUIESCING	(__SUSPEND|__TRANSIENT|__ACTIVE)
#define	USB_STATE_RESUMING	(__SUSPEND|__TRANSIENT)
#define	USB_STATE_SUSPENDED	(__SUSPEND)

/* System flags	 */
#define	HCD_MEMORY		0x0001
#define	HCD_USB2		0x0020
#define	HCD_USB11		0x0010

#define	HCD_IS_RUNNING(state) ((state) & __ACTIVE)
#define	HCD_IS_SUSPENDED(state)	((state) & __SUSPEND)


/*---------------------------------------------------
 *    Host controller related
 -----------------------------------------------------*/
/* IRQ line for	the ISP1763 */
#define	HCD_IRQ			IRQ_GPIO(25)
#define	CMD_RESET		(1<<1)	/* reset HC not	bus */
#define	CMD_RUN			(1<<0)	/* start/stop HC */
#define	STS_PCD			(1<<2)	/* port	change detect */
/* NOTE:  urb->transfer_flags expected to not use this bit !!! */
#define	EHCI_STATE_UNLINK	0x8000	/* urb being unlinked */

/*  Bits definations for qha*/
/* Bits	PID*/
#define	SETUP_PID		(2)
#define	OUT_PID			(0)
#define	IN_PID			(1)

/* Bits	MULTI*/
#define	MULTI(x)		((x)<< 29)
#define	XFER_PER_UFRAME(x)	(((x) >> 29) & 0x3)

/*Active, EP type and speed bits */
#define	QHA_VALID		(1<<0)
#define	QHA_ACTIVE		(1<<31)

/*1763 error bit maps*/
#define	HC_MSOF_INT		(1<< 0)
#define	HC_MSEC_INT		(1 << 1)
#define	HC_EOT_INT		(1 << 3)
#define     HC_OPR_REG_INT	(1<<4)
#define     HC_CLK_RDY_INT	(1<<6)
#define	HC_INTL_INT		(1 << 7)
#define	HC_ATL_INT		(1 << 8)
#define	HC_ISO_INT		(1 << 9)
#define	HC_OTG_INT		(1 << 10)

/*PTD error codes*/
#define	PTD_STATUS_HALTED	(1 << 30)
#define	PTD_XACT_ERROR		(1 << 28)
#define	PTD_BABBLE		(1 << 29)
#define PTD_ERROR		(PTD_STATUS_HALTED | PTD_XACT_ERROR | PTD_BABBLE)
/*ep types*/
#define	EPTYPE_BULK		(2 << 12)
#define	EPTYPE_CONTROL		(0 << 12)
#define	EPTYPE_INT		(3 << 12)
#define	EPTYPE_ISO		(1 << 12)

#define	PHCI_QHA_LENGTH		32

#define usb_inc_dev_use		usb_get_dev
#define usb_dec_dev_use		usb_put_dev
#define usb_free_dev		usb_put_dev
/*1763 host controller periodic	size*/
#define PTD_PERIODIC_SIZE	16
#define MAX_PERIODIC_SIZE	16
#define PTD_FRAME_MASK		0x1f
/*periodic list*/
struct _periodic_list {
	int framenumber;
	struct list_head sitd_itd_head;
	char high_speed;	/*1 - HS ; 0 - FS*/
	u16 ptdlocation;
};
typedef	struct _periodic_list periodic_list;


/*iso ptd*/
struct _isp1763_isoptd {
	u32 td_info1;
	u32 td_info2;
	u32 td_info3;
	u32 td_info4;
	u32 td_info5;
	u32 td_info6;
	u32 td_info7;
	u32 td_info8;
} __attribute__	((aligned(32)));

typedef	struct _isp1763_isoptd isp1763_isoptd;

struct _isp1763_qhint {
	u32 td_info1;
	u32 td_info2;
	u32 td_info3;
	u32 td_info4;
	u32 td_info5;
#define	INT_UNDERRUN (1	<< 2)
#define	INT_BABBLE    (1 << 1)
#define	INT_EXACT     (1 << 0)
	u32 td_info6;
	u32 td_info7;
	u32 td_info8;
} __attribute__	((aligned(32)));

typedef	struct _isp1763_qhint isp1763_qhint;


struct _isp1763_qha {
	u32 td_info1;		/* First 32 bit	*/
	u32 td_info2;		/* Second 32 bit */
	u32 td_info3;		/* third 32 bit	*/
	u32 td_info4;		/* fourth 32 bit */
	u32 reserved[4];
};
typedef	struct _isp1763_qha isp1763_qha, *pisp1763_qha;




/*this does not	cover all interrupts in	1763 chip*/
typedef	struct _ehci_regs {

	/*standard ehci	registers */
	u32 command;
	u32 usbinterrupt;
	u32 usbstatus;
	u32 hcsparams;
	u32 frameindex;

	/*isp1763 interrupt specific registers */
	u16 hwmodecontrol;
	u16 interrupt;
	u16 interruptenable;
	u32 interruptthreshold;
	u16 iso_irq_mask_or;
	u16 int_irq_mask_or;
	u16 atl_irq_mask_or;
	u16 iso_irq_mask_and;
	u16 int_irq_mask_and;
	u16 atl_irq_mask_and;
	u16 buffer_status;

	/*isp1763 initialization registers */
	u32 reset;
	u32 configflag;
	u32 ports[4];
	u32 pwrdwn_ctrl;

	/*isp1763 transfer specific registers */
	u16 isotddonemap;
	u16 inttddonemap;
	u16 atltddonemap;
	u16 isotdskipmap;
	u16 inttdskipmap;
	u16 atltdskipmap;
	u16 isotdlastmap;
	u16 inttdlastmap;
	u16 atltdlastmap;
	u16 scratch;

} ehci_regs, *pehci_regs;

/*memory management structures*/
#define MEM_KV
#ifdef MEM_KV
typedef struct isp1763_mem_addr {
	u32 phy_addr;		/* Physical address of the memory */
	u32 virt_addr;		/* after ioremap() function call */
	u8 num_alloc;		/* In case n*smaller size is allocated then for clearing purpose */
	u32 blk_size;		/*block size */
	u8 blk_num;		/* number of the block */
	u8 used;		/*used/free */
} isp1763_mem_addr_t;
#else
typedef struct isp1763_mem_addr {
	void *phy_addr;		/* Physical address of the memory */
	void *virt_addr;	/* after ioremap() function call */
	u8 usage;
	u32 blk_size;		/*block size */
} isp1763_mem_addr_t;

#endif
/* type	tag from {qh,itd,sitd,fstn}->hw_next */
#define	Q_NEXT_TYPE(dma) ((dma)	& __constant_cpu_to_le32 (3 << 1))

/* values for that type	tag */
#define	Q_TYPE_ITD	__constant_cpu_to_le32 (0 << 1)
#define	Q_TYPE_QH	__constant_cpu_to_le32 (1 << 1)
#define	Q_TYPE_SITD	__constant_cpu_to_le32 (2 << 1)
#define	Q_TYPE_FSTN	__constant_cpu_to_le32 (3 << 1)

/*next queuehead in execution*/
#define	QH_NEXT(dma)	cpu_to_le32((u32)dma)

struct ehci_qh {
	/* first part defined by EHCI spec */
	u32 hw_next;		/* see EHCI 3.6.1 */
	u32 hw_info1;		/* see EHCI 3.6.2 */

	u32 hw_info2;		/* see EHCI 3.6.2 */
	u32 hw_current;		/* qtd list - see EHCI 3.6.4 */

	/* qtd overlay (hardware parts of a struct ehci_qtd) */
	u32 hw_qtd_next;
	u32 hw_alt_next;
	u32 hw_token;
	u32 hw_buf[5];
	u32 hw_buf_hi[5];
	
	/* the rest is HCD-private */
	dma_addr_t qh_dma;	/* address of qh */
	struct list_head qtd_list;	/* sw qtd list */
	struct ehci_qtd	*dummy;
	struct ehci_qh *reclaim;	/* next	to reclaim */

	atomic_t refcount;
	wait_queue_head_t waitforcomplete;
	unsigned stamp;

	u8 qh_state;

	/* periodic schedule info */
	u8 usecs;		/* intr	bandwidth */
	u8 gap_uf;		/* uframes split/csplit	gap */
	u8 c_usecs;		/* ... split completion	bw */
	unsigned short period;	/* polling interval */
	unsigned short start;	/* where polling starts	*/
	u8 datatoggle;		/*data toggle */

	/*handling the ping stuffs */
	u8 ping;		/*ping bit */

	/*qtd <-> ptd management */

	u32 qtd_ptd_index;	/* Td-PTD map index for	this ptd */
	u32 type;		/* endpoint type */

	/*iso stuffs */
	struct usb_host_endpoint *ep;
	int next_uframe;	/*next uframe for this endpoint	*/
	struct list_head itd_list;	/*list of tds to this endpoint */
	isp1763_mem_addr_t memory_addr;
	struct _periodic_list periodic_list;
	/*scheduling requirements for this endpoint */
	u32 ssplit;
	u32 csplit;
	u8 totalptds;   // total number of PTDs needed for current URB
	u8 actualptds;	// scheduled PTDs until now for current URB
};

/* urb private part for	the driver. */
typedef	struct {
	struct ehci_qh *qh;
	u16 length;		/* number of tds associated with this request */
	u16 td_cnt;		/* number of tds already serviced */
	int state;		/* State machine state when URB	is deleted  */
	int timeout;		/* timeout for bulk transfers */
	wait_queue_head_t wait;	/* wait	State machine state when URB is	deleted	*/
	/*FIX solve the	full speed dying */
	struct timer_list urb_timer;
	struct list_head qtd_list;
	struct ehci_qtd	*qtd[0];	/* list	pointer	to all corresponding TDs associated with this request */

} urb_priv_t;

/*
 * EHCI	Specification 0.95 Section 3.6
 * QH: describes control/bulk/interrupt	endpoints
 * See Fig 3-7 "Queue Head Structure Layout".
 *
 * These appear	in both	the async and (for interrupt) periodic schedules.
 */


/*Defination required for the ehci Queuehead */
#define	QH_HEAD			0x00008000
#define	QH_STATE_LINKED		1	/* HC sees this	*/
#define	QH_STATE_UNLINK		2	/* HC may still	see this */
#define	QH_STATE_IDLE		3	/* HC doesn't see this */
#define	QH_STATE_UNLINK_WAIT	4	/* LINKED and on reclaim q */
#define	QH_STATE_COMPLETING	5	/* don't touch token.HALT */
#define	QH_STATE_TAKE_NEXT	8	/*take the new transfer	from */
#define	NO_FRAME ((unsigned short)~0)	/* pick	new start */


#define EHCI_ITD_TRANLENGTH	0x0fff0000	/*transaction length */
#define EHCI_ITD_PG		0x00007000	/*page select */
#define EHCI_ITD_TRANOFFSET	0x00000fff	/*transaction offset */
#define EHCI_ITD_BUFFPTR	0xfffff000	/*buffer pointer */

struct ehci_sitd {
	/* first part defined by EHCI spec */
	u32 hw_next;		/* see EHCI 3.3.1 */
	u32 hw_transaction[8];	/* see EHCI 3.3.2 */
#define EHCI_ISOC_ACTIVE	(1<<31)	/* activate transfer this slot */
#define EHCI_ISOC_BUF_ERR	(1<<30)	/* Data buffer error */
#define EHCI_ISOC_BABBLE	(1<<29)	/* babble detected */
#define EHCI_ISOC_XACTERR	(1<<28)	/* XactErr - transaction error */

#define EHCI_ITD_LENGTH(tok)	(((tok)>>16) & 0x7fff)
#define EHCI_ITD_IOC		(1 << 15)	/* interrupt on complete */

	u32 hw_bufp[7];		/* see EHCI 3.3.3 */
	u32 hw_bufp_hi[7];	/* Appendix B */

	/* the rest is HCD-private */
	dma_addr_t sitd_dma;	/* for this itd */
	struct urb *urb;
	struct list_head sitd_list;	/* list of urb frames' itds */
	dma_addr_t buf_dma;	/* frame's buffer address */

	/* for now, only one hw_transaction per itd */
	u32 transaction;
	u16 index;		/* in urb->iso_frame_desc */
	u16 uframe;		/* in periodic schedule */
	u16 usecs;
	/*memory address */
	struct isp1763_mem_addr mem_addr;
	int length;
	u32 framenumber;
	u32 ptdframe;
	int sitd_index;
	/*scheduling fields */
	u32 ssplit;
	u32 csplit;
	u32 start_frame;
};

struct ehci_itd	{
	/* first part defined by EHCI spec */
	u32 hw_next;		/* see EHCI 3.3.1 */
	u32 hw_transaction[8];	/* see EHCI 3.3.2 */
#define	EHCI_ISOC_ACTIVE	(1<<31)	/* activate transfer this slot */
#define	EHCI_ISOC_BUF_ERR	(1<<30)	/* Data	buffer error */
#define	EHCI_ISOC_BABBLE	(1<<29)	/* babble detected */
#define	EHCI_ISOC_XACTERR	(1<<28)	/* XactErr - transaction error */

#define	EHCI_ITD_LENGTH(tok)	(((tok)>>16) & 0x7fff)
#define	EHCI_ITD_IOC		(1 << 15)	/* interrupt on	complete */

	u32 hw_bufp[7];		/* see EHCI 3.3.3 */
	u32 hw_bufp_hi[7];	/* Appendix B */

	/* the rest is HCD-private */
	dma_addr_t itd_dma;	/* for this itd	*/
	struct urb *urb;
	struct list_head itd_list;	/* list	of urb frames' itds */
	dma_addr_t buf_dma;	/* frame's buffer address */
	u8 num_of_pkts;		/*number of packets for this ITD */
	/* for now, only one hw_transaction per	itd */
	u32 transaction;
	u16 index;		/* in urb->iso_frame_desc */
	u16 uframe;		/* in periodic schedule	*/
	u16 usecs;
	/*memory address */
	struct isp1763_mem_addr	mem_addr;
	int length;
	u32 multi;
	u32 framenumber;
	u32 ptdframe;
	int itd_index;
	/*scheduling fields */
	u32 ssplit;
	u32 csplit;
};

/*
 * EHCI	Specification 0.95 Section 3.5
 * QTD:	describe data transfer components (buffer, direction, ...)
 * See Fig 3-6 "Queue Element Transfer Descriptor Block	Diagram".
 *
 * These are associated	only with "QH" (Queue Head) structures,
 * used	with control, bulk, and	interrupt transfers.
 */
struct ehci_qtd	{
	/* first part defined by EHCI spec */
	u32 hw_next;		/* see EHCI 3.5.1 */
	u32 hw_alt_next;	/* see EHCI 3.5.2 */
	u32 hw_token;		/* see EHCI 3.5.3 */

	u32 hw_buf[5];		/* see EHCI 3.5.4 */
	u32 hw_buf_hi[5];	/* Appendix B */

	/* the rest is HCD-private */
	dma_addr_t qtd_dma;	/* qtd address */
	struct list_head qtd_list;	/* sw qtd list */
	struct urb *urb;	/* qtd's urb */
	size_t length;		/* length of buffer */
	u32 state;		/*state	of the qtd */
#define	QTD_STATE_NEW			0x100
#define	QTD_STATE_DONE			0x200
#define	QTD_STATE_SCHEDULED		0x400
#define	QTD_STATE_LAST			0x800
	struct isp1763_mem_addr	mem_addr;
};

#define	QTD_TOGGLE			(1 << 31)	/* data	toggle */
#define	QTD_LENGTH(tok)			(((tok)>>16) & 0x7fff)
#define	QTD_IOC				(1 << 15)	/* interrupt on	complete */
#define	QTD_CERR(tok)			(((tok)>>10) & 0x3)
#define	QTD_PID(tok)			(((tok)>>8) & 0x3)
#define	QTD_STS_ACTIVE			(1 << 7)	/* HC may execute this */
#define	QTD_STS_HALT			(1 << 6)	/* halted on error */
#define	QTD_STS_DBE			(1 << 5)	/* data	buffer error (in HC) */
#define	QTD_STS_BABBLE			(1 << 4)	/* device was babbling (qtd halted) */
#define	QTD_STS_XACT			(1 << 3)	/* device gave illegal response	*/
#define	QTD_STS_MMF			(1 << 2)	/* incomplete split transaction	*/
#define	QTD_STS_STS			(1 << 1)	/* split transaction state */
#define	QTD_STS_PING			(1 << 0)	/* issue PING? */

/* for periodic/async schedules	and qtd	lists, mark end	of list	*/
#define	EHCI_LIST_END	__constant_cpu_to_le32(1)	/* "null pointer" to hw	*/
#define	QTD_NEXT(dma)	cpu_to_le32((u32)dma)

struct _phci_driver;
struct _isp1763_hcd;
#define	EHCI_MAX_ROOT_PORTS 1

#include <linux/usb/hcd.h>

#define USBNET
#ifdef USBNET 
struct isp1763_async_cleanup_urb {
        struct list_head urb_list;
        struct urb *urb;
};
#endif


/*host controller*/
typedef	struct _phci_hcd {

	struct usb_hcd usb_hcd;
	spinlock_t lock;

	/* async schedule support */
	struct ehci_qh *async;
	struct ehci_qh *reclaim;
	/* periodic schedule support */
	unsigned periodic_size;
	int next_uframe;	/* scan	periodic, start	here */
	int periodic_sched;	/* periodic activity count */
	int periodic_more_urb;
	struct usb_device *otgdev;	/*otg deice, with address 2 */
	struct timer_list rh_timer;	/* drives root hub */
	struct list_head dev_list;	/* devices on this bus */
	struct list_head urb_list;	/*iso testing */

	/*msec break in	interrupts */
	atomic_t nuofsofs;
	atomic_t missedsofs;

	struct isp1763_dev *dev;
	/*hw info */
	u8 *iobase;
	u32 iolength;
	u8 *plxiobase;
	u32 plxiolength;

	int irq;		/* irq allocated */
	int state;		/*state	of the host controller */
	unsigned long reset_done[EHCI_MAX_ROOT_PORTS];
	ehci_regs regs;

	struct _isp1763_qha qha;
	struct _isp1763_qhint qhint;
	struct _isp1763_isoptd isotd;

	struct tasklet_struct tasklet;
	/*this timer is	going to run every 20 msec */
	struct timer_list watchdog;
	void (*worker_function)	(struct	_phci_hcd * hcd);
	struct _periodic_list periodic_list[PTD_PERIODIC_SIZE];
#ifdef USBNET 
	struct isp1763_async_cleanup_urb cleanup_urb;
#endif
} phci_hcd, *pphci_hcd;

/*usb_device->hcpriv, points to	this structure*/
typedef	struct hcd_dev {
	struct list_head dev_list;
	struct list_head urb_list;
} hcd_dev;

#define	usb_hcd_to_pehci_hcd(hcd)   container_of(hcd, struct _phci_hcd,	usb_hcd)

/*td allocation*/
#ifdef CONFIG_PHCI_MEM_SLAB

#define	qha_alloc(t,c) kmem_cache_alloc(c,ALLOC_FLAGS)
#define	qha_free(c,x) kmem_cache_free(c,x)
static kmem_cache_t *qha_cache,	*qh_cache, *qtd_cache;
static int
phci_hcd_mem_init(void)
{
	/* qha TDs accessed by controllers and host */
	qha_cache = kmem_cache_create("phci_ptd", sizeof(isp1763_qha), 0,
				      SLAB_HWCACHE_ALIGN, NULL,	NULL);
	if (!qha_cache)	{
		printk("no TD cache?");
		return -ENOMEM;
	}

	/* qh TDs accessed by controllers and host */
	qh_cache = kmem_cache_create("phci_ptd", sizeof(isp1763_qha), 0,
				     SLAB_HWCACHE_ALIGN, NULL, NULL);
	if (!qh_cache) {
		printk("no TD cache?");
		return -ENOMEM;
	}

	/* qtd	accessed by controllers	and host */
	qtd_cache = kmem_cache_create("phci_ptd", sizeof(isp1763_qha), 0,
				      SLAB_HWCACHE_ALIGN, NULL,	NULL);
	if (!qtd_cache)	{
		printk("no TD cache?");
		return -ENOMEM;
	}
	return 0;
}
static void
phci_mem_cleanup(void)
{
	if (qha_cache && kmem_cache_destroy(qha_cache))
		err("td_cache remained");
	qha_cache = 0;
}
#else

#define	qha_alloc(t,c)			kmalloc(t,ALLOC_FLAGS)
#define	qha_free(c,x)			kfree(x)
#define	qha_cache			0


#ifdef CONFIG_ISO_SUPPORT
/*memory constants*/
#define BLK_128_	2
#define BLK_256_	3
#define BLK_1024_	1
#define BLK_2048_	3
#define BLK_4096_	3 //1
#define BLK_8196_	0 //1
#define BLK_TOTAL	(BLK_128_+BLK_256_ + BLK_1024_ +BLK_2048_+ BLK_4096_+BLK_8196_)

#define BLK_SIZE_128	128
#define BLK_SIZE_256	256
#define BLK_SIZE_1024	1024
#define BLK_SIZE_2048	2048
#define BLK_SIZE_4096	4096
#define BLK_SIZE_8192	8192

#define  COMMON_MEMORY	1

#else
#define BLK_256_	8
#define BLK_1024_	6
#define BLK_4096_	3
#define BLK_TOTAL	(BLK_256_ + BLK_1024_ + BLK_4096_)
#define BLK_SIZE_256	256
#define BLK_SIZE_1024	1024
#define BLK_SIZE_4096	4096
#endif
static void phci_hcd_mem_init(void);
static inline void
phci_mem_cleanup(void)
{
	return;
}

#endif

#define	PORT_WKOC_E			(1<<22)	/* wake	on overcurrent (enable)	*/
#define	PORT_WKDISC_E			(1<<21)	/* wake	on disconnect (enable) */
#define	PORT_WKCONN_E			(1<<20)	/* wake	on connect (enable) */
/* 19:16 for port testing */
/* 15:14 for using port	indicator leds (if HCS_INDICATOR allows) */
#define	PORT_OWNER			(1<<13)	/* true: companion hc owns this	port */
#define	PORT_POWER			(1<<12)	/* true: has power (see	PPC) */
#define	PORT_USB11(x)			(((x)&(3<<10))==(1<<10))	/* USB 1.1 device */
/* 11:10 for detecting lowspeed	devices	(reset vs release ownership) */
/* 9 reserved */
#define	PORT_RESET			(1<<8)	/* reset port */
#define	PORT_SUSPEND			(1<<7)	/* suspend port	*/
#define	PORT_RESUME			(1<<6)	/* resume it */
#define	PORT_OCC			(1<<5)	/* over	current	change */

#define	PORT_OC				(1<<4)	/* over	current	active */
#define	PORT_PEC			(1<<3)	/* port	enable change */
#define	PORT_PE				(1<<2)	/* port	enable */
#define	PORT_CSC			(1<<1)	/* connect status change */
#define	PORT_CONNECT			(1<<0)	/* device connected */
#define PORT_RWC_BITS	(PORT_CSC | PORT_PEC | PORT_OCC)	
/*Legends,
 * ATL	  control, bulk	transfer
 * INTL	  interrupt transfer
 * ISTL	  iso transfer
 * */

/*buffer(transfer) bitmaps*/
#define	ATL_BUFFER			0x1
#define	INT_BUFFER			0x2
#define	ISO_BUFFER			0x4
#define	BUFFER_MAP			0x7

/* buffer type for ST-ERICSSON HC */
#define	TD_PTD_BUFF_TYPE_ATL		0	/* ATL buffer */
#define	TD_PTD_BUFF_TYPE_INTL		1	/* INTL	buffer */
#define	TD_PTD_BUFF_TYPE_ISTL		2	/* ISO buffer */
#define	TD_PTD_TOTAL_BUFF_TYPES		(TD_PTD_BUFF_TYPE_ISTL +1)
/*maximum number of tds	per transfer type*/
#define	TD_PTD_MAX_BUFF_TDS		16

/*invalid td index in the headers*/
#define	TD_PTD_INV_PTD_INDEX		0xFFFF
/*Host controller buffer defination*/
#define	INVALID_FRAME_NUMBER		0xFFFFFFFF
/*per td transfer size*/
#define	HC_ATL_PL_SIZE			4096
#define	HC_ISTL_PL_SIZE			1024
#define	HC_INTL_PL_SIZE			1024

/*TD_PTD_MAP states*/
#define	TD_PTD_NEW			0x0000
#define	TD_PTD_ACTIVE			0x0001
#define	TD_PTD_IDLE			0x0002
#define	TD_PTD_REMOVE			0x0004
#define	TD_PTD_RELOAD			0x0008
#define	TD_PTD_IN_SCHEDULE		0x0010
#define	TD_PTD_DONE			0x0020

#define	PTD_RETRY(x)			(((x) >> 23) & 0x3)
#define	PTD_PID(x)			(((x) >> 10) & (0x3))
#define	PTD_NEXTTOGGLE(x)		(((x) >> 25) & (0x1))
#define	PTD_XFERRED_LENGTH(x)		((x) & 0x7fff)
#define	PTD_XFERRED_NONHSLENGTH(x)	((x) & 0x7ff)
#define	PTD_PING_STATE(x)		(((x) >> 26) & (0x1))

/* urb state*/
#define	DELETE_URB			0x0008
#define	NO_TRANSFER_ACTIVE		0xFFFF
#define	NO_TRANSFER_DONE		0x0000
#define	MAX_PTD_BUFFER_SIZE		4096	/*max ptd size */

/*information of the td	in headers of host memory*/
typedef	struct td_ptd_map {
	u32 state;		/* ACTIVE, NEW,	TO_BE_REMOVED */
	u8 datatoggle;		/*to preserve the data toggle for ATL/ISTL transfers */
	u32 ptd_bitmap;		/* Bitmap of this ptd in HC headers */
	u32 ptd_header_addr;	/* headers address of  this td */
	u32 ptd_data_addr;	/*data address of this td to write in and read from */
	/*this is address is actual RAM	address	not the	CPU address
	 * RAM address = (CPU ADDRESS-0x400) >>	3
	 * */
	u32 ptd_ram_data_addr;
	u8 lasttd;		/*last td , complete the transfer */
	struct ehci_qh *qh;	/* endpoint */
	struct ehci_qtd	*qtd;	/* qtds	for this endpoint */
	struct ehci_itd	*itd;	/*itd pointer */
	struct ehci_sitd *sitd;	/*itd pointer */
	/*iso specific only */
	u32 grouptdmap;		/*if td	need to	complete with error, then process all the tds
				   in the groupmap    */
} td_ptd_map_t;

/*buffer(ATL/ISTL/INTL)	managemnet*/
typedef	struct td_ptd_map_buff {
	u8 buffer_type;		/* Buffer type:	BUFF_TYPE_ATL/INTL/ISTL0/ISTL1 */
	u8 active_ptds;		/* number of active td's in the	buffer */
	u8 total_ptds;		/* Total number	of td's	present	in the buffer (active +	tobe removed + skip) */
	u8 max_ptds;		/* Maximum number of ptd's(32) this buffer can withstand */
	u16 active_ptd_bitmap;	/* Active PTD's	bitmap */
	u16 pending_ptd_bitmap;	/* skip	PTD's bitmap */
	td_ptd_map_t map_list[TD_PTD_MAX_BUFF_TDS];	/* td_ptd_map list */
} td_ptd_map_buff_t;


#define     USB_HCD_MAJOR           0
#define     USB_HCD_MODULE_NAME     "isp1763hcd"
/* static char devpath[] = "/dev/isp1763hcd"; */

#define HCD_IOC_MAGIC	'h'

#define     HCD_IOC_POWERDOWN							_IO(HCD_IOC_MAGIC, 1)
#define     HCD_IOC_POWERUP								_IO(HCD_IOC_MAGIC, 2)
#define     HCD_IOC_TESTSE0_NACK						_IO(HCD_IOC_MAGIC, 3)
#define     HCD_IOC_TEST_J								_IO(HCD_IOC_MAGIC,4)
#define     HCD_IOC_TEST_K								_IO(HCD_IOC_MAGIC,5)
#define     HCD_IOC_TEST_TESTPACKET						_IO(HCD_IOC_MAGIC,6)
#define     HCD_IOC_TEST_FORCE_ENABLE					_IO(HCD_IOC_MAGIC,7)
#define	  HCD_IOC_TEST_SUSPEND_RESUME				_IO(HCD_IOC_MAGIC,8)
#define     HCD_IOC_TEST_SINGLE_STEP_GET_DEV_DESC		_IO(HCD_IOC_MAGIC,9)
#define     HCD_IOC_TEST_SINGLE_STEP_SET_FEATURE		_IO(HCD_IOC_MAGIC,10)
#define     HCD_IOC_TEST_STOP							_IO(HCD_IOC_MAGIC,11)
#define     HCD_IOC_SUSPEND_BUS							_IO(HCD_IOC_MAGIC,12)
#define     HCD_IOC_RESUME_BUS							_IO(HCD_IOC_MAGIC,13)
#define     HCD_IOC_REMOTEWAKEUP_BUS					_IO(HCD_IOC_MAGIC,14)

#define HOST_COMPILANCE_TEST_ENABLE	1
#define HOST_COMP_TEST_SE0_NAK	1
#define HOST_COMP_TEST_J	2
#define HOST_COMP_TEST_K	3
#define HOST_COMP_TEST_PACKET		4
#define HOST_COMP_TEST_FORCE_ENABLE	5
#define HOST_COMP_HS_HOST_PORT_SUSPEND_RESUME	6
#define HOST_COMP_SINGLE_STEP_GET_DEV_DESC	7
#define HOST_COMP_SINGLE_STEP_SET_FEATURE	8

#endif
