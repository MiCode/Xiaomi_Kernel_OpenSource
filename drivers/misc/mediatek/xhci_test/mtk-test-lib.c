#include <linux/init.h>
#include <linux/irq.h>
#include <linux/log2.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/slab.h>

#include <linux/kernel.h>       /* printk() */
#include <linux/fs.h>           /* everything... */
#include <linux/errno.h>        /* error codes */
#include <linux/types.h>        /* size_t */
#include <linux/proc_fs.h>
#include <linux/fcntl.h>        /* O_ACCMODE */
#include <linux/seq_file.h>
#include <linux/cdev.h>
//#include <linux/pci.h>
#include <linux/string.h>
#include <linux/random.h>
#include <linux/scatterlist.h>
#include <asm/unaligned.h>
#include <linux/usb/ch9.h>
#include <linux/wait.h>
#include <linux/dma-mapping.h>

#include <linux/kthread.h>
#include <linux/kobject.h>
#include <linux/miscdevice.h>

#define MTK_TEST_LIB
#include "mtk-test-lib.h"
#undef MTK_TEST_LIB
#include "mtk-test.h"
#include "mtk-usb-hcd.h"
#include "mtk-protocol.h"
#include "xhci.h"
#include "xhci-mtk-power.h"
//#include "xhci-hub.c"

//#include "mtk-usb-hcd.h"
//#include <linux/usb/hcd.h>

// USBIF , to send IF uevent
extern int usbif_u3h_test_send_event(char *event) ;


extern int mtktest_mtk_xhci_scheduler_init(void);

/* FIXME, USB_IF workaround, change original flow*/
#define USB_IF_DMA_WORKAROUND
#ifdef USB_IF_DMA_WORKAROUND
static dma_addr_t my_dma;	
static char *my_buf;
static char *tmp_buf;
#define MY_DMA_BUF_LEN 2048
#endif

#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
int xhci_usbif_nirq = -1; 
void __iomem *xhci_usbif_base;
void __iomem *xhci_usbif_sif_base;


// extern u32 mtktest_xhci_port_state_to_neutral(u32 state);

void print_speed(int speed){
	if(speed == USB_SPEED_SUPER){
		printk(KERN_DEBUG "SUPER_SPEED device\n");
	}
	if(speed == USB_SPEED_HIGH){
		printk(KERN_DEBUG "HIGH_SPEED device\n");
	}
	if(speed == USB_SPEED_FULL){
		printk(KERN_DEBUG "FULL_SPEED device\n");
	}
	if(speed == USB_SPEED_LOW){
		printk(KERN_DEBUG "LOW_SPEED device\n");
	}
}

int mtk_xhci_handshake(struct xhci_hcd *xhci, void __iomem *ptr,
		      u32 mask, u32 done, int msec)
{
	u32	result;

	do {
		result = xhci_readl(xhci, ptr);
		if (result == ~(u32)0){		/* card removed */
			printk("[XHCI] mtk_xhci_handshake card removed, result is 0x%x\n", result) ;
			return RET_FAIL;
		}
		result &= mask;
		if (result == done){
			printk("[XHCI] mtk_xhci_handshake done, result is 0x%x\n", result) ;
			return RET_SUCCESS;
		}
		mdelay(1);
		msec--;
	} while (msec > 0);
	printk("[XHCI] mtk_xhci_handshake timeout !, result is 0x%x\n", result) ;
	return RET_FAIL;
}

int get_port_id(int slot_id){
	struct xhci_hcd *xhci;
	struct xhci_virt_device *virt_dev;
	struct xhci_slot_ctx *out_ctx;

	xhci = hcd_to_xhci(my_hcd);
	virt_dev = xhci->devs[slot_id];
	out_ctx = mtktest_xhci_get_slot_ctx(xhci, virt_dev->out_ctx);
	return (((out_ctx->dev_info2) >> 16) & 0xff);
}

int get_port_index(int port_id){
	int i;
	struct xhci_port *port;
#if 0
	xhci_dbg(xhci, "get_port_index is called\n");
	for(i=0; i<RH_PORT_NUM; i++){
		port = rh_port[i];
		printk("port[%d]: 0x%x\n", i, port);
		printk("   port_id=%d\n", port->port_id);
		printk("   port_speed=%d\n", port->port_speed);
		printk("   port_status=%d\n", port->port_status);
	}
#endif
	for(i=0; i<RH_PORT_NUM; i++){
		port = rh_port[i];
		if(port->port_id == port_id){
			return i;
		}
	}
	for(i=0; i<RH_PORT_NUM; i++){
		port = rh_port[i];
		if(port->port_id == 0){
			return i;
		}
	}
	for(i=0; i<RH_PORT_NUM; i++){
		port = rh_port[i];
		if(port->port_status == DISCONNECTED){
			port->port_id = port_id;
			return i;
		}
	}
	return RH_PORT_NUM;
}

int wait_not_event_on_timeout(int *ptr, int value, int msecs){
	int i;
	for(i= msecs; i--; i>0){
		if(*ptr != value){
			return RET_SUCCESS;
		}
		msleep(1);
	}
	if(*ptr != value)
		return RET_SUCCESS;
	else
		return RET_FAIL;
}

int wait_event_on_timeout(int *ptr, int done, int msecs){
	int i;
	for(i= msecs; i--; i>0){
		mb() ;
		if(*ptr == done){
			return RET_SUCCESS;
		}
		msleep(1);
		//mdelay(1);
#if 0
		if(i%50 == 0){
			printk(KERN_ERR "[OTG_H] wait event check...\n");
		}
#endif
	}
	if(*ptr == done)
		return RET_SUCCESS;
	else
		return RET_FAIL;
}

int wait_event_on_timeout_esc_running(int *ptr, int msecs){
	int i;
	for(i= msecs; i--; i>0){
		mb() ;
		if(*ptr != CMD_RUNNING){
			return RET_SUCCESS;
		}
		msleep(1);

	}

	return RET_FAIL;
}

int xhci_usbif_resource_get(void)
{
	struct device_node *node = NULL;

	/* got related info from dtsi */
	node = of_find_compatible_node(NULL, NULL, USB_XHCI_COMPATIBLE_NAME);
	if(node == NULL){
		printk(KERN_ERR "xhci_test get node failed\n");
		return -1;
	}
	
	xhci_usbif_nirq = irq_of_parse_and_map(node, 0);
	xhci_usbif_base = of_iomap(node, 0);
	xhci_usbif_sif_base = of_iomap(node, 1);

	return 0;
}


int f_test_lib_init(){
	int ret;
	int i;
	struct xhci_port *port;

	my_hcd = NULL;
	g_port_connect = false;
	g_port_reset = false;
	g_port_id = 0;
	g_slot_id = 0;
	g_speed = 0; // UNKNOWN_SPEED
	g_cmd_status = CMD_DONE;
	g_event_full = false;
	g_got_event_full = false;
	g_intr_handled = -1;
	g_is_bei = false;
	g_td_to_noop = false;
	g_iso_frame = false;
	g_test_random_stop_ep = false;
	g_stopping_ep = false;
	g_cmd_ring_pointer1 = 0;
	g_cmd_ring_pointer2 = 0;
	g_idt_transfer = false;
	g_hs_block_reset = false;
	g_concurrent_resume = false;
//	g_con_is_enter = false;		
	mb() ;
	printk(KERN_ERR "[OTG_H] f_test_lib_init, g_port_connect is %d\n", g_port_connect);

	for(i=0; i<DEV_NUM; i++){
		dev_list[i] = NULL;
	}
	for(i=0; i<HUB_DEV_NUM; i++){
		hdev_list[i] = NULL;
	}
	for(i=0; i<RH_PORT_NUM; i++){
		port = NULL;
		port = kmalloc(sizeof(struct xhci_port), GFP_NOIO);
		port->port_id = 0;
		port->port_speed = 0;
		port->port_status = 0;
		rh_port[i] = port;
	}
	port = NULL;
	for(i=0; i<RH_PORT_NUM; i++){
		port = rh_port[i];
		printk("port[%d]: 0x%p\n", i, port);
		printk("   port_id=%d\n", port->port_id);
		printk("   port_speed=%d\n", port->port_speed);
		printk("   port_status=%d\n", port->port_status);
	}

	ret = mtk_xhci_hcd_init();
	if(ret){
		printk(KERN_DEBUG "hcd init fail!!\n");
		return RET_FAIL;
	}
	return RET_SUCCESS;
}

int f_test_lib_cleanup(){
	u32 temp;
	int ret;
	int i;
	struct xhci_port *port;

	g_stopped = true;
	mb() ;
	
	while(g_exec_done == false){
		msleep(100) ;
	}
	// wait test thread done
	
	for(i=0; i<RH_PORT_NUM; i++){
		port = rh_port[i];
		kfree(port);
		rh_port[i] = NULL;
	}
	if(my_hcd == NULL){
		printk(KERN_ERR "driver already cleared\n");
		return RET_SUCCESS;
	}

	// USBIF, reset this 
	g_otg_test = false ;
	mb() ;
	//set host sel
    printk(KERN_ERR "[OTG_H] going to set dma to host\n");
while((readl(SSUSB_OTG_STS) & 0x2000) == 0x2000){
        }
#if 0
writel(0x0f0f0f0f, 0xf00447bc);
while((readl(0xf00447c4) & 0x2000) == 0x2000){

        }
#endif
    printk(KERN_ERR "[OTG_H] can set dma to host\n");
// USBIF, set to host mode ?
#if 1
    temp = readl(SSUSB_U2_CTRL(0));
    temp = temp | SSUSB_U2_PORT_HOST_SEL;
    writel(temp, SSUSB_U2_CTRL(0));
#endif
	mtk_xhci_hcd_cleanup();
	my_hcd = NULL;

	return RET_SUCCESS;
}

int f_port_set_pls(int port_id, int pls){
	struct xhci_hcd *xhci;
	u32 __iomem *addr;
	int temp;

	xhci = hcd_to_xhci(my_hcd);
	addr = &xhci->op_regs->port_status_base + NUM_PORT_REGS*(port_id-1 & 0xff);
	temp = xhci_readl(xhci, addr);
	temp = mtktest_xhci_port_state_to_neutral(temp);
	temp = temp & ~(PORT_PLS_MASK);
	temp = temp | (pls << 5) | PORT_LINK_STROBE;
	xhci_writel(xhci, temp, addr);
	mtk_xhci_handshake(xhci, addr, (15<<5), (pls<<5), 3000);
	temp = xhci_readl(xhci, addr);
	if(PORT_PLS_VALUE(temp) != pls){
		return RET_FAIL;
	}
	return RET_SUCCESS;
}

struct random_regs_data{
	struct xhci_hcd *xhci;
	int port_id;
	int speed;
	int power_required;
};

static int access_regs_thread(void *data){
	struct random_regs_data *rreg_data = data;
	struct xhci_hcd *xhci;
	int port_id;
	int speed;
	unsigned int randomSleep;
	u32 __iomem *addr;
	int num_u3_port;
	int port_index;
	struct xhci_port *port;
	int temp;
	unsigned int randomIndex;
	unsigned int randomOffset;

	g_power_down_allowed = 1;
	xhci = rreg_data->xhci;
	port_id = rreg_data->port_id;
	speed = rreg_data->speed;
	num_u3_port = SSUSB_U3_PORT_NUM(readl(SSUSB_IP_CAP));
	port_index = get_port_index(port_id);
	port = rh_port[port_index];

	xhci_err(xhci, "random access regs thread initial\n");
	do {
//		xhci_err(xhci, "round \n");
		//randomly sleep a while
		randomSleep = get_random_int();
		randomSleep = randomSleep%100;
//		xhci_err(xhci, "sleep %d msecs\n", randomSleep);
		msleep(randomSleep);
		//enable port clock/power if needed
		if(rreg_data->power_required){
			g_power_down_allowed = 0;
			mtktest_enablePortClockPower(port_id,rreg_data->speed);
		}
		//random access(read) MAC3/MAC2 regs
		randomIndex = get_random_int()%3;
		if(randomIndex == 0){
			addr = SSUSB_U3_MAC_BASE;
		}
		else if(randomIndex == 1){
			addr = SSUSB_U3_SYS_BASE;
		}
		else if(randomIndex == 2){
			addr = SSUSB_U2_SYS_BASE;
		}
//		xhci_err(xhci, "randomIndex %d\n", randomIndex);
		randomOffset = get_random_int()%0x80;	//4 bytes align addr
//		xhci_err(xhci, "randomOffset 0x%x\n", randomOffset);
		addr = addr + randomOffset;
//		xhci_err(xhci, "read 0x%x\n", addr);
		temp = xhci_readl(xhci, addr);
		//access port_status regs
		addr = &xhci->op_regs->port_status_base + NUM_PORT_REGS*(g_port_id-1 & 0xff);
//		xhci_err(xhci, "read 0x%x\n", addr);
		temp = xhci_readl(xhci, addr);
	} while(1);
	xhci_err(xhci, "random access regs stopped\n");
	return RET_SUCCESS;
}

int f_add_random_access_reg_thread(struct xhci_hcd *xhci, int port_id, int port_rev
	, int power_required){

	struct random_regs_data *rreg_data;
	xhci_err(xhci, "start random access regs thread, port_id: %d, port_rev %d, power required %d\n"
		, port_id, port_rev, power_required);
	rreg_data = kzalloc(sizeof(struct random_regs_data), GFP_KERNEL);
	rreg_data->xhci = xhci;
	rreg_data->port_id = port_id;
	rreg_data->speed = port_rev;
	rreg_data->power_required = power_required;
	kthread_run(access_regs_thread, rreg_data, "rrgt");
}

struct random_doorbell_data {
	struct xhci_hcd *xhci;
	int slot_id;
	int ep_index;
};

static int ring_doorbell_thread(void *data)
{
	struct random_doorbell_data *rrdb_data = data;
	struct xhci_hcd *xhci;
	int slot_id;
	int ep_index;
	struct xhci_virt_device *vdev;
	struct xhci_virt_ep *ep;
	unsigned int randomSleep = 1000;
	u32 field;

	xhci = rrdb_data->xhci;
	slot_id = rrdb_data->slot_id;
	ep_index = rrdb_data->ep_index;
	__u32 __iomem *db_addr = &xhci->dba->doorbell[slot_id];
//	xhci_err(xhci, "random ring doorbell thread is running\n");
	vdev = xhci->devs[slot_id];
	ep = &(vdev->eps[ep_index]);
//	xhci_err(xhci, "ep_state: 0x%x\n", ep->ep_state);
	do {
		randomSleep = get_random_int();
		randomSleep = randomSleep%100;
//		xhci_err(xhci, "sleep: %d\n", randomSleep);
		msleep(randomSleep);
//		xhci_err(xhci, "ring ep doorbell, slot id: %d, ep index: %d\n"
//			, slot_id, ep_index);
		if (!(ep->ep_state & EP_HALT_PENDING) && !(ep->ep_state & SET_DEQ_PENDING)
			&& !(ep->ep_state & EP_HALTED)) {
			field = xhci_readl(xhci, db_addr) & DB_MASK;
			field |= EPI_TO_DB(ep_index);
			writel(field, db_addr);
//			xhci_writel(xhci, field, db_addr);
		}
		vdev = xhci->devs[slot_id];
//		xhci_err(xhci, "vdev: 0x%x\n", vdev);
		if(vdev)
			ep = &(vdev->eps[ep_index]);
//		xhci_err(xhci, "ep: 0x%x\n", ep);
	} while (vdev && ep && (!(ep->ep_state & EP_HALT_PENDING) && !(ep->ep_state & SET_DEQ_PENDING)
			&& !(ep->ep_state & EP_HALTED)));
	xhci_err(xhci, "ring_doorbell thread stopped, slot id: %d, ep index: %d, state: 0x%x\n"
		, slot_id, ep_index, ep->ep_state);
	return 0;
}

int f_add_random_ring_doorbell_thread(struct xhci_hcd *xhci, int slot_id, int ep_index){

	struct random_doorbell_data *rrdb_data;
	xhci_err(xhci, "start random ring doorbell thread, eps: %d\n", ep_index);
	rrdb_data = kzalloc(sizeof(struct random_doorbell_data), GFP_KERNEL);
	rrdb_data->xhci = xhci;
	rrdb_data->slot_id = slot_id;
	rrdb_data->ep_index = ep_index;
	kthread_run(ring_doorbell_thread, rrdb_data, "rdbt");
}

struct stop_endpoint_data {
	struct xhci_hcd *xhci;
	int slot_id;
	int ep_index;
};

static int stop_endpoint_thread(void *data){
	struct stop_endpoint_data *stpep_data = data;
	struct xhci_hcd *xhci;
	int slot_id;
	int ep_index;
	struct xhci_virt_device *vdev;
	struct xhci_virt_ep *ep;
	unsigned int randomSleep = 1000;
	u32 field;
	unsigned long flags;

	xhci = stpep_data->xhci;
	slot_id = stpep_data->slot_id;
	ep_index = stpep_data->ep_index;
	__u32 __iomem *db_addr = &xhci->dba->doorbell[slot_id];
	vdev = xhci->devs[slot_id];
	ep = &(vdev->eps[ep_index]);
	do {
		randomSleep = get_random_int();
		randomSleep = randomSleep%100;
		msleep(randomSleep);
		if (!(ep->ep_state & EP_HALT_PENDING) && !(ep->ep_state & SET_DEQ_PENDING)
			&& !(ep->ep_state & EP_HALTED)) {
			while(g_stopping_ep){msleep(1);}
			g_stopping_ep = true;
			spin_lock_irqsave(&xhci->lock, flags);
			mtktest_xhci_queue_stop_endpoint(xhci, slot_id, ep_index);
			spin_unlock_irqrestore(&xhci->lock, flags);
			mtktest_xhci_ring_cmd_db(xhci);
			g_stopping_ep = false;
			msleep(100);
			field = xhci_readl(xhci, db_addr) & DB_MASK;
			field |= EPI_TO_DB(ep_index);
			writel(field, db_addr);
		}
	}while (vdev && ep && (!(ep->ep_state & EP_HALT_PENDING) && !(ep->ep_state & SET_DEQ_PENDING)
			&& !(ep->ep_state & EP_HALTED)));
	xhci_err(xhci, "stop endpoint thread stopped, slot id: %d, ep index: %d, state: 0x%x\n"
		, slot_id, ep_index, ep->ep_state);
	return 0;
}

int f_add_random_stop_ep_thread(struct xhci_hcd *xhci, int slot_id, int ep_index){
	struct stop_endpoint_data *stpep_data;
	xhci_err(xhci, "start random stop ep thread, eps: %d\n", ep_index);
	stpep_data = kzalloc(sizeof(struct stop_endpoint_data), GFP_KERNEL);
	stpep_data->xhci = xhci;
	stpep_data->slot_id = slot_id;
	stpep_data->ep_index = ep_index;
	kthread_run(stop_endpoint_thread, stpep_data, "rstpep");
}


extern void phy_dump_regs();

int f_enable_port(int index){
	int timeout = ATTACH_TIMEOUT;
	struct xhci_port *port = rh_port[index];

    //phy_dump_regs();

	//waiting for device to connect
	xhci_err(xhci, "Waiting for device[%d] to attach\n", index);
	while(port->port_status != ENABLED && timeout > 0){
#if TEST_OTG
		if(g_stopped){
			printk(KERN_ERR "[OTG_H]force stop\n");
            return RET_FAIL;
		}
#endif
		msleep(1);
		timeout--;
	}
	if(port->port_status != ENABLED){
		xhci_err(xhci, "[ERROR] port[%d] enabled timeout\n", index);
		return RET_FAIL;
	}
	g_port_id = port->port_id;
	xhci_dbg(xhci, "port_index(%d), port_id(%d), speed(%d), status(%d), reenabled(%d)\n",
        index, port->port_id, port->port_reenabled, port->port_speed, port->port_status);
	return RET_SUCCESS;
}

int f_disconnect_port(int index){
	struct xhci_port *port;
	int timeout = ATTACH_TIMEOUT;

	port = rh_port[index];
	//waiting for device to disconnect
	xhci_dbg(xhci, "Waiting for device[%d] to disconnect\n", index);
	while(port->port_status != DISCONNECTED && timeout > 0){
		ssleep(1);
		timeout--;
	}
	if(port->port_status != DISCONNECTED){
		xhci_err(xhci, "[ERROR] Device disconnect timeout\n");
		return RET_FAIL;
	}
	g_port_id = port->port_id;
	xhci_dbg(xhci, "port [%d] disconnect done\n", index);
	return RET_SUCCESS;
}

void start_port_reenabled(int index, int speed){
	struct xhci_port *port;
	port = rh_port[index];
	g_port_id = port->port_id;
	int next_port_index;
	int cur_speed;
	if(index == 0){
		next_port_index = 1;
	}
	else{
		next_port_index = 0;
	}
	cur_speed = DEV_SPEED_SUPER;
	if(port->port_speed == USB_SPEED_SUPER){
		cur_speed = DEV_SPEED_SUPER;
	}
	else if(port->port_speed == USB_SPEED_HIGH){
		cur_speed = DEV_SPEED_HIGH;
	}
	else if(port->port_speed == USB_SPEED_FULL){
		cur_speed = DEV_SPEED_FULL;
	}
	if((cur_speed != speed) && ((speed == DEV_SPEED_SUPER) || (cur_speed == DEV_SPEED_SUPER))){
		rh_port[next_port_index]->port_id = port->port_id;
		rh_port[next_port_index]->port_speed = port->port_speed;
		rh_port[next_port_index]->port_reenabled = port->port_reenabled;
		rh_port[next_port_index]->port_status = port->port_status;
		port->port_id = 0;
		port->port_speed = 0;
		port->port_reenabled = 1;
		port->port_status = DISCONNECTED;
	}
	else{
		port->port_reenabled = 0;
	}

    xhci_err(xhci, "port_reenabled(%d)\n", port->port_reenabled);

	return;
}

int f_reenable_port(int index){
	struct xhci_port *port;
	int timeout = ATTACH_TIMEOUT;

	port = rh_port[index];
	return wait_event_on_timeout(&(port->port_reenabled), 2, ATTACH_TIMEOUT);
}

int f_enable_dev_note(){
	struct xhci_hcd *xhci;
	u32 __iomem *addr;
	u32 temp;

	xhci = hcd_to_xhci(my_hcd);
	addr = &xhci->op_regs->dev_notification;
	temp = 0xffff;
	xhci_writel(xhci, temp, addr);
	return RET_SUCCESS;
}

int f_enable_slot(struct usb_device *dev){
	struct xhci_hcd *xhci;
	struct usb_device *udev, *rhdev;
	struct xhci_virt_device *virt_dev;
	struct xhci_port *port;
	int port_index;
	int ret;
	unsigned long flags;

	xhci = hcd_to_xhci(my_hcd);
	if(dev == NULL){
		if(g_port_id == 0){
			xhci_err(xhci, "[ERROR] f_enable_slot - g_port_id is 0!\n");
			return RET_FAIL;
		}
		port_index = get_port_index(g_port_id);
		port = rh_port[port_index];

		//new usb device
		rhdev = my_hcd->self.root_hub;
		printk("[OTG_H] rhdev = my_hcd->self.root_hub = 0x%p", rhdev) ;
		
		udev = rhdev->children[g_port_id-1];
		udev = mtk_usb_alloc_dev(rhdev, rhdev->bus, g_port_id);
		printk("[OTG_H] f_enable_slot => mtk_usb_alloc_dev udev = 0x%p", udev) ;
		udev->speed = port->port_speed;
		udev->level = rhdev->level + 1;
		rhdev->children[g_port_id-1] = udev;
		printk("[OTG_H] g_port_id %d, rhdev->children[g_port_id-1] = 0x%p", g_port_id, rhdev->children[g_port_id-1]) ;
	}
	else{
		udev = dev;
	}
	//enable slot
	spin_lock_irqsave(&xhci->lock, flags);

	if (g_slot_id != 0 && xhci->devs[g_slot_id]!=NULL){
		mtktest_xhci_free_virt_device(xhci, g_slot_id);
	}
	
	g_cmd_status = CMD_RUNNING;
	ret = mtktest_xhci_queue_slot_control(xhci, TRB_ENABLE_SLOT, 0);
	if (ret) {
		xhci_err(xhci, "[ERROR]FIXME: allocate a command ring segment\n");
		spin_unlock_irqrestore(&xhci->lock, flags);		
		return RET_FAIL;
	}
	spin_unlock_irqrestore(&xhci->lock, flags);
	xhci_dbg(xhci, "Enable slot command\n");
	mtktest_xhci_ring_cmd_db(xhci);
	//wait_event_on_timeout(&g_cmd_status, CMD_DONE, CMD_TIMEOUT);
	wait_event_on_timeout_esc_running(&g_cmd_status , CMD_TIMEOUT);
	if(g_cmd_status == CMD_RUNNING){
		xhci_err(xhci, "[ERROR]command timeout\n");
		//return RET_FAIL;
	}
	if(g_cmd_status == CMD_FAIL){
		xhci_err(xhci, "[ERROR]command failed\n");
		return RET_FAIL;
	}
	xhci_dbg(xhci, "enable slot done\n");

	//alloc xhci_virt_device
	xhci_err(xhci, "g_slot_id %d\n", g_slot_id);
	udev->slot_id = g_slot_id;
	if (!mtktest_xhci_alloc_virt_device(xhci, g_slot_id, udev, GFP_KERNEL)) {
		spin_lock_irqsave(&xhci->lock, flags);		
		/* Disable slot, if we can do it without mem alloc */
		xhci_warn(xhci, "[WARN]Could not allocate xHCI USB device data structures\n");
		if (!mtktest_xhci_queue_slot_control(xhci, TRB_DISABLE_SLOT, g_slot_id))
			mtktest_xhci_ring_cmd_db(xhci);
		spin_unlock_irqrestore(&xhci->lock, flags);
		
		return RET_FAIL;
	}
	xhci_dbg(xhci, "alloc xhci_virt_device done\n");
	return RET_SUCCESS;
}

int f_address_slot(char isBSR, struct usb_device *dev){
	struct xhci_hcd *xhci;
	struct usb_device *udev, *rhdev;
	struct xhci_virt_device *virt_dev;
	struct xhci_slot_ctx *slot_ctx;
	unsigned long flags;
	int ret;

	xhci = hcd_to_xhci(my_hcd);
	if(dev == NULL){
		if(g_slot_id == 0){
			xhci_err(xhci, "[ERROR] global slot ID not valid\n");
			return RET_FAIL;
		}
		rhdev = my_hcd->self.root_hub;
		udev = rhdev->children[g_port_id-1];
	}
	else{
		udev = dev;
	}
	//address device
	g_cmd_status = CMD_RUNNING;
	virt_dev = xhci->devs[udev->slot_id];
	slot_ctx = mtktest_xhci_get_slot_ctx(xhci, virt_dev->in_ctx);
	/*
	 * If this is the first Set Address since device plug-in or
	 * virt_device realloaction after a resume with an xHCI power loss,
	 * then set up the slot context.
	 */
	spin_lock_irqsave(&xhci->lock, flags); 
	if (!slot_ctx->dev_info)
		mtktest_xhci_setup_addressable_virt_dev(xhci, udev);
	/* Otherwise, update the control endpoint ring enqueue pointer. */
	else{
		mtktest_xhci_copy_ep0_dequeue_into_input_ctx(xhci, udev);
	}
	//spin_lock_irqsave(&xhci->lock, flags);
	ret = mtktest_xhci_queue_address_device(xhci, virt_dev->in_ctx->dma, udev->slot_id, isBSR);
	if (ret) {
		xhci_err(xhci, "[ERROR]FIXME: allocate a command ring segment\n");
		spin_unlock_irqrestore(&xhci->lock, flags);
		return RET_FAIL;
	}
	spin_unlock_irqrestore(&xhci->lock, flags);
	xhci_dbg(xhci, "Address Device command\n");
	mtktest_xhci_ring_cmd_db(xhci);
	wait_event_on_timeout(&g_cmd_status, CMD_DONE, CMD_TIMEOUT);
	if(g_cmd_status == CMD_RUNNING){
		xhci_err(xhci, "[ERROR]command timeout\n");
		return RET_FAIL;
	}
	if(g_cmd_status == CMD_FAIL){
		xhci_err(xhci, "[ERROR]command failed\n");
		return RET_FAIL;
	}
	xhci_dbg(xhci, "address device done\n");

	//wait device to finish set_address request, 3351 capability
    //msleep(100);

	return RET_SUCCESS;
}

int f_disable_slot(){
	struct xhci_hcd *xhci;
	unsigned long flags;

	xhci_err(xhci, "[OTG_H]disable slot begin, g_slot_id is %d\n", g_slot_id);

	if (g_slot_id == 0){
		xhci_err(xhci, "[ERROR] f_disable_slot - skip\n");
		return RET_SUCCESS;
	}
	
	xhci = hcd_to_xhci(my_hcd);
	//disable slot
	g_cmd_status = CMD_RUNNING;
	
	spin_lock_irqsave(&xhci->lock, flags);	
	mtktest_xhci_queue_slot_control(xhci, TRB_DISABLE_SLOT, g_slot_id);		
	spin_unlock_irqrestore(&xhci->lock, flags);
	
	mtktest_xhci_ring_cmd_db(xhci);
	//wait_event_on_timeout(&g_cmd_status, CMD_DONE, CMD_TIMEOUT);
	wait_event_on_timeout_esc_running(&g_cmd_status , CMD_TIMEOUT/4);
	if(g_cmd_status == CMD_RUNNING){
		xhci_err(xhci, "[ERROR] disable slot timeout\n");
		return RET_FAIL;
	}else{
		if(g_cmd_status == CMD_DONE){
			xhci_err(xhci, "disable slot done\n");
		}
		else{
			xhci_err(xhci, "[ERROR]disable slot fail\n");
			return RET_FAIL;
		}
	}
	mtktest_xhci_free_virt_device(xhci, g_slot_id);
	g_slot_id = 0;
	return RET_SUCCESS;
}

int f_evaluate_context(int max_exit_latency, int maxp0, int preping_mode, int preping, int besl, int besld){
	struct xhci_hcd *xhci;
	struct usb_device *udev, *rhdev;
	struct xhci_virt_device *virt_dev;
	struct xhci_slot_ctx *out_ctx, *in_ctx;
	struct xhci_container_ctx *in_container_ctx;
	struct xhci_ep_ctx *ep0_in_ctx, *ep0_out_ctx;
	struct xhci_port *port;
	int port_id, port_index;
	int orig_maxp, new_maxp, orig_max_exit_latency, new_max_exit_latency, orig_preping_mode
		, new_preping_mode, orig_preping, new_preping;
	int orig_besl, new_besl, orig_besld, new_besld;
	int ret;
	unsigned long flags;

	ret = 0;
	xhci = hcd_to_xhci(my_hcd);
	rhdev = my_hcd->self.root_hub;
	udev = rhdev->children[g_port_id-1];
	port_id = get_port_id(g_slot_id);
	port_index = get_port_index(port_id);
	port = rh_port[port_index];
	virt_dev = xhci->devs[g_slot_id];
	out_ctx = mtktest_xhci_get_slot_ctx(xhci, virt_dev->out_ctx);
	in_ctx = mtktest_xhci_get_slot_ctx(xhci, virt_dev->in_ctx);
	in_container_ctx = virt_dev->in_ctx;
	ep0_out_ctx = mtktest_xhci_get_ep_ctx(xhci, virt_dev->out_ctx, 0);
	ep0_in_ctx = mtktest_xhci_get_ep_ctx(xhci, virt_dev->in_ctx, 0);

	//if default state, ep0 may be used before
	if(GET_SLOT_STATE(out_ctx->dev_state) == SLOT_STATE_DEFAULT ||
		GET_SLOT_STATE(out_ctx->dev_state) == SLOT_STATE_ADDRESSED ||
		GET_SLOT_STATE(out_ctx->dev_state) == SLOT_STATE_CONFIGURED){
		mtktest_xhci_copy_ep0_dequeue_into_input_ctx(xhci, udev);
	}
	orig_maxp = (ep0_out_ctx->ep_info2 >> 16);
	orig_max_exit_latency = (out_ctx->dev_info2 & 0xffff);

	ep0_in_ctx->ep_info2 &= ~(0xffff << 16);
	ep0_in_ctx->ep_info2 |= (maxp0 << 16);
	in_ctx->dev_info2 &= ~(0xffff);
	in_ctx->dev_info2 |= max_exit_latency;
	in_ctx->reserved[0] &= ~(0x7fff);
	in_ctx->reserved[0] |= preping;
	in_ctx->reserved[0] &= ~(1 << 16);
	in_ctx->reserved[0] |= (preping_mode << 16);
	in_ctx->reserved[1] &= ~(0xffff);
	in_ctx->reserved[1] |= ((besld << 8) | besl);
	mtktest_xhci_dbg_ctx(xhci, in_container_ctx, 0);
	g_cmd_status = CMD_RUNNING;
	spin_lock_irqsave(&xhci->lock, flags);
	mtktest_xhci_queue_evaluate_context(xhci, in_container_ctx->dma, g_slot_id);
	spin_unlock_irqrestore(&xhci->lock, flags);
	
	mtktest_xhci_ring_cmd_db(xhci);
	wait_event_on_timeout(&g_cmd_status, CMD_DONE, CMD_TIMEOUT);
	if(g_cmd_status == CMD_RUNNING){
		xhci_err(xhci, "[ERROR] evaluate context timeout\n");
		return RET_FAIL;
	}
	if(g_cmd_status == CMD_DONE){
		xhci_dbg(xhci, "evaluate context done\n");
	}
	else{
		xhci_err(xhci, "[ERROR]evaluate context fail\n");
		return RET_FAIL;
	}
	mtktest_xhci_dbg_ctx(xhci, virt_dev->out_ctx, 0);
	new_maxp = (ep0_out_ctx->ep_info2 >> 16);
	new_max_exit_latency = (out_ctx->dev_info2 & 0xffff);
	new_preping_mode = ((out_ctx->reserved[0] >> 16) & 0x1);
	new_preping = (out_ctx->reserved[0] & 0x7fff);
	new_besld = ((out_ctx->reserved[1] >> 8) & 0xff);
	new_besl = (out_ctx->reserved[1] & 0xff);
	if(new_maxp != maxp0){
		xhci_err(xhci, "[ERROR] maxp doesn't match input[%d], new[%d]\n", maxp0, new_maxp);
		ret = RET_FAIL;
	}
	if(new_max_exit_latency != max_exit_latency){
		xhci_err(xhci, "[ERROR] max_exit_latency doesn't match input[%d], new[%d]\n"
			, max_exit_latency, new_max_exit_latency);
		ret = RET_FAIL;
	}
	if(new_preping_mode != preping_mode){
		xhci_err(xhci, "[ERROR] preping_mode doesn't match input[%d], new[%d]\n"
			, preping_mode, new_preping_mode);
		ret = RET_FAIL;
	}
	if(new_preping != preping){
		xhci_err(xhci, "[ERROR] preping doesn't match input[%d], new[%d]\n"
			, preping, new_preping);
		ret = RET_FAIL;
	}
	if(new_besld != besld){
		xhci_err(xhci, "[ERROR] besld doesn't match input[%d], new[%d]\n"
			, besld, new_besld);
		ret = RET_FAIL;
	}
	if(new_besl != besl){
		xhci_err(xhci, "[ERROR] besl doesn't match input[%d], new[%d]\n"
			, besl, new_besl);
		ret = RET_FAIL;
	}
	return ret;

}

struct urb *alloc_ctrl_urb(struct usb_ctrlrequest *dr, char *buffer, struct usb_device *udev){
	struct urb *urb;
	dma_addr_t mapping;
	struct device	*dev;
	struct xhci_hcd *xhci;

	/* FIXME, USB_IF workaround, change original flow*/
#ifdef USB_IF_DMA_WORKAROUND
	static int firstShot = 0;
	if(!firstShot){
		my_buf = dma_alloc_coherent(&udev->dev, MY_DMA_BUF_LEN, &my_dma, GFP_KERNEL);
		firstShot = 1;
	}
	tmp_buf = buffer;
	buffer = my_buf;
#endif

	xhci = hcd_to_xhci(my_hcd);
	dev = xhci_to_hcd(xhci)->self.controller;
	urb = usb_alloc_urb(0, GFP_NOIO);
	if(dr->bRequestType & USB_DIR_IN){
		usb_fill_control_urb(urb, udev, usb_rcvctrlpipe(udev, 0), dr, buffer,
			dr->wLength, NULL, NULL);
	}
	else{
		usb_fill_control_urb(urb, udev, usb_sndctrlpipe(udev, 0), dr, buffer,
			dr->wLength, NULL, NULL);
	}
	urb->status = -EINPROGRESS;
	urb->actual_length = 0;
	urb->transfer_flags |= (dr->bRequestType & USB_DIR_MASK);
	urb->ep = &udev->ep0;
	if(buffer){
		/* FIXME, USB_IF workaround, change original flow*/
#ifdef USB_IF_DMA_WORKAROUND
		urb->transfer_dma = my_dma;
#else
		mapping = dma_map_single(dev, buffer, dr->wLength, DMA_BIDIRECTIONAL);
		urb->transfer_dma = mapping;
		dma_sync_single_for_device(dev, mapping, urb->transfer_buffer_length, DMA_BIDIRECTIONAL);
#endif

	}
//	mtk_map_urb_for_dma(my_hcd, urb, GFP_KERNEL);

	return urb;
}

int f_ctrlrequest_nowait(struct urb *urb, struct usb_device *udev){
	int ret;
	int i;
	char *tmp;
	struct device	*dev;
	struct xhci_hcd *xhci;
	xhci = hcd_to_xhci(my_hcd);
	struct urb_priv	*urb_priv;
	unsigned long flags;
	int size;

	dev = xhci_to_hcd(xhci)->self.controller;
	size = 1;
	urb_priv = kmalloc(sizeof(struct urb_priv) + size * sizeof(struct xhci_td *), GFP_KERNEL);

	if (!urb_priv){
		xhci_err(xhci, "[ERROR] allocate urb_priv failed\n");
		return RET_FAIL;
	}

	for (i = 0; i < size; i++) {
		urb_priv->td[i] = kmalloc(sizeof(struct xhci_td), GFP_KERNEL);
		if (!urb_priv->td[i]) {
			urb_priv->length = i;
			mtktest_xhci_urb_free_priv(xhci, urb_priv);
			return RET_FAIL;
		}
	}

	urb_priv->length = size;
	urb_priv->td_cnt = 0;
	urb->hcpriv = urb_priv;
	urb->status = -EINPROGRESS;
	xhci_dbg(xhci, "ctrl request\n");
	xhci_dbg(xhci, "setup packet: 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x\n"
		, *(urb->setup_packet), *(urb->setup_packet+1), *(urb->setup_packet+2), *(urb->setup_packet+3)
		, *(urb->setup_packet+4), *(urb->setup_packet+5), *(urb->setup_packet+6), *(urb->setup_packet+7));
	spin_lock_irqsave(&xhci->lock, flags);
	ret = mtktest_xhci_queue_ctrl_tx(xhci, GFP_ATOMIC, urb,	udev->slot_id, 0);
	spin_unlock_irqrestore(&xhci->lock, flags);
#if 0
	wait_not_event_on_timeout(&(urb->status), -EINPROGRESS, TRANS_TIMEOUT);
	if(urb->status == 0){
		if(urb->transfer_buffer_length > 0){
			dma_sync_single_for_cpu(dev,urb->transfer_dma,urb->transfer_buffer_length,DMA_BIDIRECTIONAL);
		}
		mtktest_xhci_urb_free_priv(xhci, urb->hcpriv);
		return RET_SUCCESS;
	}
	else{
		xhci_err(xhci, "[ERROR] control request failed\n");
		ret = urb->status;
		mtktest_xhci_urb_free_priv(xhci, urb_priv);
		return RET_FAIL;
	}
#endif
	return RET_SUCCESS;
}

int f_ctrlrequest(struct urb *urb, struct usb_device *udev){
	int ret;
	int i;
	char *tmp;
	struct device	*dev;
	struct xhci_hcd *xhci;
	xhci = hcd_to_xhci(my_hcd);
	struct urb_priv	*urb_priv;
	unsigned long flags;
	int size;

	dev = xhci_to_hcd(xhci)->self.controller;
	size = 1;
	urb_priv = kmalloc(sizeof(struct urb_priv) + size * sizeof(struct xhci_td *), GFP_KERNEL);

	if (!urb_priv){
		xhci_err(xhci, "[ERROR] allocate urb_priv failed\n");
		return RET_FAIL;
	}

	for (i = 0; i < size; i++) {
		urb_priv->td[i] = kmalloc(sizeof(struct xhci_td), GFP_KERNEL);
		if (!urb_priv->td[i]) {
			urb_priv->length = i;
			mtktest_xhci_urb_free_priv(xhci, urb_priv);
			return RET_FAIL;
		}
	}

	urb_priv->length = size;
	urb_priv->td_cnt = 0;
	urb->hcpriv = urb_priv;
	urb->status = -EINPROGRESS;
	xhci_dbg(xhci, "ctrl request\n");
	xhci_dbg(xhci, "setup packet: 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x\n"
		, *(urb->setup_packet), *(urb->setup_packet+1), *(urb->setup_packet+2), *(urb->setup_packet+3)
		, *(urb->setup_packet+4), *(urb->setup_packet+5), *(urb->setup_packet+6), *(urb->setup_packet+7));

		
	spin_lock_irqsave(&xhci->lock, flags);
	ret = mtktest_xhci_queue_ctrl_tx(xhci, GFP_ATOMIC, urb,	udev->slot_id, 0);
	spin_unlock_irqrestore(&xhci->lock, flags);
	wait_not_event_on_timeout(&(urb->status), -EINPROGRESS, TRANS_TIMEOUT);
	xhci_dbg(xhci, "ctrl request status = %d\n", urb->status);
	if(urb->status == 0){
		if(urb->transfer_buffer_length > 0){
			/* FIXME, USB_IF workaround, change original flow*/
#ifdef USB_IF_DMA_WORKAROUND
			memcpy(tmp_buf, my_buf, urb->transfer_buffer_length);
#else
			dma_sync_single_for_cpu(dev,urb->transfer_dma,urb->transfer_buffer_length,DMA_BIDIRECTIONAL);
#endif	
		}
		mtktest_xhci_urb_free_priv(xhci, urb->hcpriv);
		return RET_SUCCESS;
	}
	else{
		xhci_err(xhci, "[ERROR] control request failed\n");
		ret = urb->status;
		mtktest_xhci_urb_free_priv(xhci, urb_priv);
		return RET_FAIL;
	}
}

int f_update_hub_device(struct usb_device *udev, int num_ports){
	int ret;
	struct xhci_hcd *xhci;
	struct xhci_virt_device *vdev;
	struct xhci_slot_ctx *in_ctx;

	ret = 0;
	xhci = hcd_to_xhci(my_hcd);
	vdev = xhci->devs[udev->slot_id];
	in_ctx = mtktest_xhci_get_slot_ctx(xhci, vdev->in_ctx);
	in_ctx->dev_info |= DEV_HUB;
	in_ctx->dev_info2 |= XHCI_MAX_PORTS(num_ports);
	in_ctx->tt_info |= TT_THINK_TIME(0);

	return ret;
}

int f_udev_add_ep(struct usb_host_endpoint *ep, struct usb_device *udev){
	int ret;
	struct xhci_hcd *xhci;
	struct xhci_virt_device *virt_dev;
	int epnum, is_out;
	int ep_index;

	epnum = usb_endpoint_num(&ep->desc);
	is_out = usb_endpoint_dir_out(&ep->desc);
	xhci = hcd_to_xhci(my_hcd);
	virt_dev = xhci->devs[udev->slot_id];

	if(is_out){
		udev->ep_out[epnum] = ep;
	}
	else{
		udev->ep_in[epnum] = ep;
	}
	ret = mtktest_xhci_mtk_add_endpoint(my_hcd, udev, ep);
	if(ret){
		xhci_err(xhci, "[ERROR] add endpoint failed\n");
		return RET_FAIL;
	}
	ep_index = mtktest_xhci_get_endpoint_index(&ep->desc);
	virt_dev->eps[ep_index].ring = virt_dev->eps[ep_index].new_ring;

	return RET_SUCCESS;
}

int f_udev_drop_ep(int is_out, int epnum, struct usb_device *udev){
	int ret;
	struct xhci_hcd *xhci;
	struct xhci_virt_device *virt_dev;
	int ep_index;
	struct usb_host_endpoint *ep;

	if(is_out){
		ep = udev->ep_out[epnum];
	}
	else{
		ep = udev->ep_in[epnum];
	}
	ep_index = mtktest_xhci_get_endpoint_index(&ep->desc);
	ret = mtktest_xhci_mtk_drop_endpoint(my_hcd, udev, ep);
	if(ret){
		xhci_err(xhci, "[ERROR] drop endpoint failed\n");
		return RET_FAIL;
	}
	kfree(ep);
	if(is_out){
		udev->ep_out[epnum] = NULL;
	}
	else{
		udev->ep_in[epnum] = NULL;
	}

	return RET_SUCCESS;
}

int f_xhci_config_ep(struct usb_device *udev){
	int ret;
	struct xhci_hcd *xhci;
	struct xhci_virt_device *virt_dev;
	struct xhci_input_control_ctx *ctrl_ctx;
	int i;
	unsigned long flags;

	xhci = hcd_to_xhci(my_hcd);
	virt_dev = xhci->devs[udev->slot_id];
	ctrl_ctx = mtktest_xhci_get_input_control_ctx(xhci, virt_dev->in_ctx);
	ctrl_ctx->add_flags |= SLOT_FLAG;
	ctrl_ctx->add_flags &= ~EP0_FLAG;
	ctrl_ctx->drop_flags &= ~SLOT_FLAG;
	ctrl_ctx->drop_flags &= ~EP0_FLAG;

	g_cmd_status = CMD_RUNNING;
	spin_lock_irqsave(&xhci->lock, flags);
	ret = mtktest_xhci_queue_configure_endpoint(xhci, virt_dev->in_ctx->dma,
				udev->slot_id, false);
	spin_unlock_irqrestore(&xhci->lock, flags);
	
	mtktest_xhci_ring_cmd_db(xhci);
	wait_event_on_timeout(&g_cmd_status, CMD_DONE, CMD_TIMEOUT);
	if(g_cmd_status == CMD_RUNNING){
		xhci_err(xhci, "[ERROR] config endpoint timeout\n");
		return RET_FAIL;
	}
	if(g_cmd_status == CMD_FAIL){
		xhci_err(xhci, "[ERROR] config endpoint failed\n");
		return RET_FAIL;
	}
	for (i = 1; i < 31; ++i) {
		if ((le32_to_cpu(ctrl_ctx->drop_flags) & (1 << (i + 1))) &&
		    !(le32_to_cpu(ctrl_ctx->add_flags) & (1 << (i + 1))))
			mtktest_xhci_free_or_cache_endpoint_ring(xhci, virt_dev, i);
	}
	mtktest_xhci_zero_in_ctx(xhci, virt_dev);
	return RET_SUCCESS;
}

int f_xhci_deconfig_ep(struct usb_device *udev){
	int ret;
	struct xhci_hcd *xhci;
	struct xhci_virt_device *virt_dev;
	struct xhci_input_control_ctx *ctrl_ctx;
	int i;
	unsigned long flags;

	xhci = hcd_to_xhci(my_hcd);
	virt_dev = xhci->devs[udev->slot_id];
	ctrl_ctx = mtktest_xhci_get_input_control_ctx(xhci, virt_dev->in_ctx);
	ctrl_ctx->add_flags |= SLOT_FLAG;
	ctrl_ctx->add_flags &= ~EP0_FLAG;
	ctrl_ctx->drop_flags &= ~SLOT_FLAG;
	ctrl_ctx->drop_flags &= ~EP0_FLAG;

	g_cmd_status = CMD_RUNNING;
	spin_lock_irqsave(&xhci->lock, flags);
	ret = mtktest_xhci_queue_deconfigure_endpoint(xhci, virt_dev->in_ctx->dma,
				udev->slot_id, false);
	spin_unlock_irqrestore(&xhci->lock, flags);
	
	mtktest_xhci_ring_cmd_db(xhci);
	wait_event_on_timeout(&g_cmd_status, CMD_DONE, CMD_TIMEOUT);
	if(g_cmd_status == CMD_RUNNING){
		xhci_err(xhci, "[ERROR] config endpoint timeout\n");
		return RET_FAIL;
	}
	if(g_cmd_status == CMD_FAIL){
		xhci_err(xhci, "[ERROR] config endpoint failed\n");
		return RET_FAIL;
	}
	for (i = 1; i < 31; ++i) {
		if ((le32_to_cpu(ctrl_ctx->drop_flags) & (1 << (i + 1))) &&
		    !(le32_to_cpu(ctrl_ctx->add_flags) & (1 << (i + 1))))
			mtktest_xhci_free_or_cache_endpoint_ring(xhci, virt_dev, i);
	}
	mtktest_xhci_zero_in_ctx(xhci, virt_dev);
	return RET_SUCCESS;
}

int f_config_ep(char ep_num,int ep_dir,int transfer_type, int maxp,int bInterval, int burst, int mult, struct usb_device *udev,int config_xhci){
	int ret;
	struct xhci_hcd *xhci;
	struct usb_host_endpoint *ep_tx, *ep_rx;
	struct usb_device *dev, *rhdev;

	xhci = hcd_to_xhci(my_hcd);
	if(udev == NULL){
		rhdev = my_hcd->self.root_hub;
		dev = rhdev->children[g_port_id-1];
	}
	else{
		dev = udev;
	}

	if(maxp>1024){
		maxp = 1024 |(((maxp/1024)-1) << 11);

	}

	ep_tx = kmalloc(sizeof(struct usb_host_endpoint), GFP_NOIO);
	ep_tx->desc.bDescriptorType = USB_DT_ENDPOINT;
	ep_tx->desc.bEndpointAddress = EPADD_NUM(ep_num) | ep_dir;
	ep_tx->desc.bmAttributes = transfer_type;
	ep_tx->desc.wMaxPacketSize = maxp;
	if(dev->speed == USB_SPEED_HIGH){
		ep_tx->desc.wMaxPacketSize |= (mult << 11);
	}
	ep_tx->desc.bInterval = bInterval;
	ep_tx->ss_ep_comp.bMaxBurst = burst;
	ep_tx->ss_ep_comp.bmAttributes = mult;
	if(usb_endpoint_xfer_isoc(&ep_tx->desc)){
		ep_tx->ss_ep_comp.wBytesPerInterval = (burst+1) * (mult+1) * maxp;
	}
	else if(usb_endpoint_xfer_int(&ep_tx->desc)){
		ep_tx->ss_ep_comp.wBytesPerInterval = (burst+1) * maxp;
	}
	else{
		ep_tx->ss_ep_comp.wBytesPerInterval = 0;
	}


	ret = f_udev_add_ep(ep_tx, dev);
	if(ret != RET_SUCCESS){
		return RET_FAIL;
	}

	if(config_xhci){
		ret = f_xhci_config_ep(dev);
		if(ret != RET_SUCCESS){
			return RET_FAIL;
		}
	}
	return RET_SUCCESS;
}

int f_deconfig_ep(char is_all, char ep_num,int ep_dir,struct usb_device *usbdev,int config_xhci){
	int i;
	int ret;
	int is_out;
	struct usb_device *udev, *rhdev;
	struct usb_host_endpoint *ep;

	if(usbdev == NULL){
		rhdev = my_hcd->self.root_hub;
		udev = rhdev->children[g_port_id-1];
	}
	else{
		udev = usbdev;
	}

	if(is_all){
		for(i=1; i<=15; i++){
			ep = udev->ep_out[i];
			if(ep){
				kfree(ep);
				udev->ep_out[i] = NULL;
			}
			ep = udev->ep_in[i];
			if(ep){
				kfree(ep);
				udev->ep_out[i] = NULL;
			}
		}
		return f_xhci_deconfig_ep(udev);
	}
	else{
		if(ep_dir == EPADD_OUT){
			is_out = 1;
		}
		else{
			is_out = 0;
		}
		ret = f_udev_drop_ep(is_out, ep_num, udev);
		if(config_xhci){
			ret = f_xhci_config_ep(udev);
			if(ret != RET_SUCCESS){
				return RET_FAIL;
			}
		}
		return RET_SUCCESS;
	}
}


int f_loopback_config_ep(char ep_out,char ep_in,int transfer_type, int maxp,int bInterval, struct usb_device *udev){
	int ret;
	struct xhci_hcd *xhci;
	struct usb_host_endpoint *ep_tx, *ep_rx;
	struct usb_device *dev, *rhdev;

	xhci = hcd_to_xhci(my_hcd);
	if(udev == NULL){
		rhdev = my_hcd->self.root_hub;
		dev = rhdev->children[g_port_id-1];
	}
	else{
		dev = udev;
	}

	ep_tx = kmalloc(sizeof(struct usb_host_endpoint), GFP_NOIO);
	ep_tx->desc.bDescriptorType = USB_DT_ENDPOINT;
	ep_tx->desc.bEndpointAddress = EPADD_NUM(ep_out) | EPADD_OUT;
	ep_tx->desc.bmAttributes = transfer_type;
	ep_tx->desc.wMaxPacketSize = maxp;
	ep_tx->desc.bInterval = bInterval;

	ret = f_udev_add_ep(ep_tx, dev);
	if(ret != RET_SUCCESS){
		return RET_FAIL;
	}

	ep_rx = kmalloc(sizeof(struct usb_host_endpoint), GFP_NOIO);
	ep_rx->desc.bDescriptorType = USB_DT_ENDPOINT;
	ep_rx->desc.bEndpointAddress = EPADD_NUM(ep_in) | EPADD_IN;
	ep_rx->desc.bmAttributes = transfer_type;
	ep_rx->desc.wMaxPacketSize = maxp;
	ep_rx->desc.bInterval = bInterval;

	ret = f_udev_add_ep(ep_rx, dev);
	if(ret != RET_SUCCESS){
		return RET_FAIL;
	}

	ret = f_xhci_config_ep(dev);
	if(ret != RET_SUCCESS){
		return RET_FAIL;
	}
	return RET_SUCCESS;
}

int f_reset_ep(int slot_id, int ep_index){
	struct xhci_hcd *xhci;
	struct xhci_ring *ep_ring;
	struct xhci_virt_ep *ep;
	struct xhci_td *cur_td = NULL;
	struct xhci_dequeue_state deq_state;
	struct xhci_virt_device *dev;
	unsigned long flags;

	xhci = hcd_to_xhci(my_hcd);
	g_cmd_status = CMD_RUNNING;
	spin_lock_irqsave(&xhci->lock, flags);
	mtktest_xhci_queue_reset_ep(xhci, slot_id, ep_index);
	spin_unlock_irqrestore(&xhci->lock, flags);
	
	mtktest_xhci_ring_cmd_db(xhci);
	wait_event_on_timeout(&g_cmd_status, CMD_DONE, CMD_TIMEOUT);
	if(g_cmd_status == CMD_RUNNING){
		xhci_err(xhci, "[ERROR] reset endpoint timout\n");
		return RET_FAIL;
	}
	if(g_cmd_status == CMD_FAIL){
		xhci_err(xhci, "[ERROR] reset endpiont failed\n");
		return RET_FAIL;
	}
	return RET_SUCCESS;
}


int f_queue_urb(struct urb *urb,int wait, struct usb_device *dev){

	struct usb_device *udev, *rhdev;
	struct xhci_hcd *xhci;
	struct usb_host_endpoint *ep;
	struct urb_priv	*urb_priv;
	int ep_index;
	int ret;
	int size, i;
	unsigned long flags;

	xhci = hcd_to_xhci(my_hcd);
	rhdev = my_hcd->self.root_hub;
	if(dev){
		udev = dev;
	}
	else{
		udev = rhdev->children[g_port_id-1];
	}
	ep = urb->ep;
	ep_index = mtktest_xhci_get_endpoint_index(&ep->desc);

	size = 1;
	if(!urb->hcpriv){
		if (usb_endpoint_xfer_isoc(&ep->desc))
			size = urb->number_of_packets;
		else
			size = 1;
		urb_priv = kmalloc(sizeof(struct urb_priv) + size * sizeof(struct xhci_td *), GFP_KERNEL);
		if (!urb_priv){
			xhci_err(xhci, "[ERROR] allocate urb_priv failed\n");
			return RET_FAIL;
		}

		for (i = 0; i < size; i++) {
			urb_priv->td[i] = kmalloc(sizeof(struct xhci_td), GFP_KERNEL);
			if (!urb_priv->td[i]) {
				urb_priv->length = i;
				mtktest_xhci_urb_free_priv(xhci, urb_priv);
				return RET_FAIL;
			}
		}

		urb_priv->length = size;
		urb_priv->td_cnt = 0;
		urb->hcpriv = urb_priv;
	}
	else{
		urb_priv = urb->hcpriv;
		size = urb_priv->length;
		urb_priv->td_cnt = 0;
	}
	if(usb_endpoint_xfer_bulk(&ep->desc)){
		spin_lock_irqsave(&xhci->lock, flags);
		ret = mtktest_xhci_queue_bulk_tx(xhci, GFP_KERNEL, urb, udev->slot_id, ep_index);
		spin_unlock_irqrestore(&xhci->lock, flags);
	}
	else if(usb_endpoint_xfer_int(&ep->desc)){
		spin_lock_irqsave(&xhci->lock, flags);
		ret = mtktest_xhci_queue_intr_tx(xhci, GFP_KERNEL, urb, udev->slot_id, ep_index);
		spin_unlock_irqrestore(&xhci->lock, flags);
	}
	else if(usb_endpoint_xfer_isoc(&ep->desc)){
		spin_lock_irqsave(&xhci->lock, flags);
		ret = mtktest_xhci_queue_isoc_tx_prepare(xhci, GFP_KERNEL, urb, udev->slot_id, ep_index);
		spin_unlock_irqrestore(&xhci->lock, flags);
	}

	if(ret){
	xhci_err(xhci, "[ERROR] queue tx error %d\n", ret);
	}

	if(wait){
		if(usb_endpoint_xfer_isoc(&ep->desc)){
			wait_not_event_on_timeout(&(urb->status), -EINPROGRESS, 500000);
		}
		else{
			wait_not_event_on_timeout(&(urb->status), -EINPROGRESS, TRANS_TIMEOUT);
		}
		if(urb->status != 0){
			xhci_err(xhci, "[ERROR] Tx transfer error, status=%d\n", urb->status);
			if(urb->status == -EINPROGRESS){
				xhci_err(xhci, "[ERROR] Timeout, stop endpoint and set tr dequeue pointer\n");
				f_ring_stop_ep(g_slot_id, ep_index);
				f_ring_set_tr_dequeue_pointer(g_slot_id, ep_index, urb);
				f_reset_ep(g_slot_id,ep_index);
				mtktest_xhci_urb_free_priv(xhci, urb_priv);
				urb->hcpriv = NULL;
				return RET_FAIL;
			}
			else if(urb->status == -EPIPE){
				xhci_err(xhci, "[ERROR] Tx transfer error EPIPE, status=%d\n", urb->status);
				f_reset_ep(g_slot_id,ep_index);
				f_ring_set_tr_dequeue_pointer(g_slot_id, ep_index, urb);
				mtktest_xhci_urb_free_priv(xhci, urb_priv);
				urb->hcpriv = NULL;
				return RET_FAIL;
			}
		}
		mtktest_xhci_urb_free_priv(xhci, urb->hcpriv);
		urb->hcpriv = NULL;
		xhci_dbg(xhci, "Tx done, status=%d\n", urb->status);
	}

	ret = RET_SUCCESS;

	return ret;
}


void f_free_urb(struct urb *urb,int data_length, int start_add){
	struct device	*dev;
	struct xhci_virt_device *virt_dev;
	struct xhci_hcd *xhci;
	//struct urb *urb;
	struct usb_host_endpoint *ep;
	int ep_index;
	void *buffer;
	dma_addr_t mapping;

	xhci = hcd_to_xhci(my_hcd);
	dev = xhci_to_hcd(xhci)->self.controller;

	urb->transfer_buffer -= start_add;
	urb->transfer_dma -= start_add;
	urb->transfer_buffer_length = data_length+start_add;

	xhci_dbg(xhci, "free transfer buffer address 0x%p\n", urb->transfer_buffer);
	xhci_dbg(xhci, "free transfer dma address 0x%p\n", (void *)(unsigned long)urb->transfer_dma);
//	unmap_urb_for_dma(my_hcd, urb);
	dma_unmap_single(dev, urb->transfer_dma, urb->transfer_buffer_length,DMA_BIDIRECTIONAL);
	kfree(urb->transfer_buffer);
	usb_free_urb(urb);
}
int f_fill_urb_with_buffer(struct urb *urb,int ep_num,int data_length,void *buffer,int start_add,int dir, int iso_num_packets, int psize
, dma_addr_t dma_mapping, struct usb_device *usbdev){

	struct usb_device *udev, *rhdev;
	struct device	*dev;
	struct xhci_virt_device *virt_dev;
	struct xhci_hcd *xhci;
	//struct urb *urb;
	struct usb_host_endpoint *ep;
	int ep_index;
	u8 *tmp1, *tmp2;
	//int data_length;
	int num_sgs;
	dma_addr_t mapping;//dma stream buffer
	int ret;
	int i, j;

	ret = 0;
	num_sgs = 0;
	xhci = hcd_to_xhci(my_hcd);
	dev = xhci_to_hcd(xhci)->self.controller;//dma stream buffer
	if(!usbdev){
		rhdev = my_hcd->self.root_hub;
		udev = rhdev->children[g_port_id-1];
	}
	else{
		udev = usbdev;
	}
	if(dir==URB_DIR_OUT){
		ep = udev->ep_out[ep_num];
	}else{
		ep = udev->ep_in[ep_num];
	}
	ep_index = mtktest_xhci_get_endpoint_index(&ep->desc);
	//Tx
	if(!dma_mapping){
    	mapping = dma_map_single(dev, buffer,data_length, DMA_BIDIRECTIONAL);
	}
	else{
		mapping = dma_mapping;
	}
	xhci_dbg(xhci, "data_length(%d) psize %d\n", data_length, psize);
	xhci_dbg(xhci, "buffer(0x%p), dma(0x%p)\n", buffer, (void *)(unsigned long)mapping);
	if(usb_endpoint_xfer_bulk(&ep->desc) && usb_endpoint_dir_out(&ep->desc)){
		usb_fill_bulk_urb(urb, udev, usb_sndbulkpipe(udev, usb_endpoint_num(&ep->desc)), buffer, data_length, NULL, NULL);
	}
	else if(usb_endpoint_xfer_bulk(&ep->desc) && usb_endpoint_dir_in(&ep->desc)){
		usb_fill_bulk_urb(urb, udev, usb_rcvbulkpipe(udev, usb_endpoint_num(&ep->desc)), buffer, data_length, NULL, NULL);
	}
	else if(usb_endpoint_xfer_int(&ep->desc) && usb_endpoint_dir_out(&ep->desc)){
		usb_fill_int_urb(urb, udev, usb_sndintpipe(udev, usb_endpoint_num(&ep->desc)), buffer, data_length, NULL, NULL, ep->desc.bInterval);
	}
	else if(usb_endpoint_xfer_int(&ep->desc) && usb_endpoint_dir_in(&ep->desc)){
		usb_fill_int_urb(urb, udev, usb_rcvintpipe(udev, usb_endpoint_num(&ep->desc)), buffer, data_length, NULL, NULL, ep->desc.bInterval);
	}
	else if(usb_endpoint_xfer_isoc(&ep->desc) && usb_endpoint_dir_out(&ep->desc)){
		urb->dev = udev;
		urb->pipe = usb_sndisocpipe(udev,usb_endpoint_num(&ep->desc));
		urb->transfer_flags = URB_ISO_ASAP;
		urb->transfer_buffer = buffer;
		urb->number_of_packets = iso_num_packets;
		urb->transfer_buffer_length = data_length;
		for(j=0; j<iso_num_packets; j++){
			urb->iso_frame_desc[j].offset = j * psize;
			xhci_dbg(xhci, "[Debug] iso frame offset %d\n", urb->iso_frame_desc[j].offset);
			if(j == iso_num_packets-1){
				urb->iso_frame_desc[j].length = (data_length-(j*psize));
			}
			else{
				urb->iso_frame_desc[j].length = psize;
			}
			xhci_dbg(xhci, "[Debug] iso frame length %d\n", urb->iso_frame_desc[j].length);
		}
	}
	else if(usb_endpoint_xfer_isoc(&ep->desc) && usb_endpoint_dir_in(&ep->desc)){
		urb->dev = udev;
		urb->pipe = usb_sndisocpipe(udev,usb_endpoint_num(&ep->desc));
		urb->transfer_flags = URB_ISO_ASAP;
		urb->transfer_buffer = buffer;
		urb->number_of_packets = iso_num_packets;
		urb->transfer_buffer_length = data_length;
		for(j=0; j<iso_num_packets; j++){
			urb->iso_frame_desc[j].offset = j * psize;
			xhci_dbg(xhci, "[Debug] iso frame offset %d\n", urb->iso_frame_desc[j].offset);
			if(j == iso_num_packets-1){
				urb->iso_frame_desc[j].length = (data_length-(j*psize));
			}
			else{
				urb->iso_frame_desc[j].length = psize;
			}
			xhci_dbg(xhci, "[Debug] iso frame length %d\n", urb->iso_frame_desc[j].length);
		}
	}
	urb->transfer_dma = mapping;
	urb->status = -EINPROGRESS;
	urb->actual_length = 0;
	urb->transfer_flags |= dir | URB_ZERO_PACKET;
	urb->ep = ep;
	urb->num_sgs = num_sgs;

//	ret = map_urb_for_dma(my_hcd, urb, GFP_KERNEL);
	urb->transfer_buffer += start_add;
	urb->transfer_dma += start_add;
	urb->transfer_buffer_length = data_length;

	if(dir==URB_DIR_OUT){
		get_random_bytes(urb->transfer_buffer, data_length);
	}

/*
	if(dir==URB_DIR_OUT){
		for(i=0; i<data_length; i++){
			tmp1 = urb->transfer_buffer+i;
			if(*tmp1 == 0xff){
				*tmp1 = 0xfe;
			}
		}
	}
*/
	dma_sync_single_for_device(dev, urb->transfer_dma, data_length, DMA_BIDIRECTIONAL);

	return ret;
}

int f_fill_urb(struct urb *urb,int ep_num,int data_length, int start_add,int dir, int iso_num_packets, int psize, struct usb_device *usbdev){
	struct usb_device *udev, *rhdev;
	struct device	*dev;
	struct xhci_virt_device *virt_dev;
	struct xhci_hcd *xhci;
	//struct urb *urb;
	struct usb_host_endpoint *ep;
	int ep_index;
	void *buffer;
	u8 *tmp1, *tmp2;
	//int data_length;
	dma_addr_t mapping;//dma stream buffer
	struct scatterlist *sg;
	void *tmp_sg;
	int cur_sg_len;
	int ret;
	int i, j;

	ret = 0;
	xhci = hcd_to_xhci(my_hcd);
	dev = xhci_to_hcd(xhci)->self.controller;//dma stream buffer
	rhdev = my_hcd->self.root_hub;
	if(usbdev){
		udev = usbdev;
	}
	else{
		udev = rhdev->children[g_port_id-1];
	}
	if(dir==URB_DIR_OUT){
		ep = udev->ep_out[ep_num];
	}else{
		ep = udev->ep_in[ep_num];
	}
	ep_index = mtktest_xhci_get_endpoint_index(&ep->desc);
	//Tx
	buffer = kmalloc(data_length+start_add, GFP_KERNEL);
    mapping = dma_map_single(dev, buffer,data_length+start_add, DMA_BIDIRECTIONAL);
	xhci_dbg(xhci, "[Debug] psize %d\n", psize);
	xhci_dbg(xhci, "dma buffer address 0x%p\n", buffer);
	xhci_dbg(xhci, "mapping buffer address 0x%p\n", (void *)(unsigned long)mapping);
	if(usb_endpoint_xfer_bulk(&ep->desc) && usb_endpoint_dir_out(&ep->desc)){
		usb_fill_bulk_urb(urb, udev, usb_sndbulkpipe(udev, usb_endpoint_num(&ep->desc)), buffer, data_length+start_add, NULL, NULL);
	}
	else if(usb_endpoint_xfer_bulk(&ep->desc) && usb_endpoint_dir_in(&ep->desc)){
		usb_fill_bulk_urb(urb, udev, usb_rcvbulkpipe(udev, usb_endpoint_num(&ep->desc)), buffer, data_length+start_add, NULL, NULL);
	}
	else if(usb_endpoint_xfer_int(&ep->desc) && usb_endpoint_dir_out(&ep->desc)){
		usb_fill_int_urb(urb, udev, usb_sndintpipe(udev, usb_endpoint_num(&ep->desc)), buffer, data_length+start_add, NULL, NULL, ep->desc.bInterval);
	}
	else if(usb_endpoint_xfer_int(&ep->desc) && usb_endpoint_dir_in(&ep->desc)){
		usb_fill_int_urb(urb, udev, usb_rcvintpipe(udev, usb_endpoint_num(&ep->desc)), buffer, data_length+start_add, NULL, NULL, ep->desc.bInterval);
	}
	else if(usb_endpoint_xfer_isoc(&ep->desc) && usb_endpoint_dir_out(&ep->desc)){
		urb->dev = udev;
		urb->pipe = usb_sndisocpipe(udev,usb_endpoint_num(&ep->desc));
		urb->transfer_flags = URB_ISO_ASAP;
		urb->transfer_buffer = buffer;
		urb->number_of_packets = iso_num_packets;
		urb->transfer_buffer_length = data_length;
		for(j=0; j<iso_num_packets; j++){
			urb->iso_frame_desc[j].offset = j * psize;
			xhci_dbg(xhci, "[Debug] iso frame offset %d\n", urb->iso_frame_desc[j].offset);
			if(j == iso_num_packets-1){
				urb->iso_frame_desc[j].length = (data_length-(j*psize));
			}
			else{
				urb->iso_frame_desc[j].length = psize;
			}
			xhci_dbg(xhci, "[Debug] iso frame length %d\n", urb->iso_frame_desc[j].length);
		}
	}
	else if(usb_endpoint_xfer_isoc(&ep->desc) && usb_endpoint_dir_in(&ep->desc)){
		urb->dev = udev;
		urb->pipe = usb_sndisocpipe(udev,usb_endpoint_num(&ep->desc));
		urb->transfer_flags = URB_ISO_ASAP;
		urb->transfer_buffer = buffer;
		urb->number_of_packets = iso_num_packets;
		urb->transfer_buffer_length = data_length;
		for(j=0; j<iso_num_packets; j++){
			urb->iso_frame_desc[j].offset = j * psize;
			xhci_dbg(xhci, "[Debug] iso frame offset %d\n", urb->iso_frame_desc[j].offset);
			if(j == iso_num_packets-1){
				urb->iso_frame_desc[j].length = (data_length-(j*psize));
			}
			else{
				urb->iso_frame_desc[j].length = psize;
			}
			xhci_dbg(xhci, "[Debug] iso frame length %d\n", urb->iso_frame_desc[j].length);
		}
	}
	urb->transfer_dma = mapping;
	urb->status = -EINPROGRESS;
	urb->actual_length = 0;
	urb->transfer_flags |= dir | URB_ZERO_PACKET;
	urb->ep = ep;
	urb->num_sgs = 0;

//	ret = map_urb_for_dma(my_hcd, urb, GFP_KERNEL);
	urb->transfer_buffer += start_add;
	urb->transfer_dma += start_add;
	urb->transfer_buffer_length = data_length;
	get_random_bytes(urb->transfer_buffer, data_length);

	if(dir==URB_DIR_OUT){
		for(i=0; i<data_length; i++){
			tmp1 = urb->transfer_buffer+i;
			if(*tmp1 == 0xff){
				*tmp1 = 0xfe;
			}
		}
	}

	dma_sync_single_for_device(dev, mapping, data_length+start_add, DMA_BIDIRECTIONAL);
	urb->hcpriv = NULL;
	return ret;
}

struct mtk_sg {
	int sg_len;
	struct list_head list;
};

#define MIN_SG_LENGTH 512
#define MAX_SG_LENGTH 4096
int f_fill_urb_sg(struct urb *urb, int dir,int ep_num,int data_length, int sg_len, struct usb_device *usbdev){
	struct usb_device *udev, *rhdev;
	struct device	*dev;
	struct xhci_virt_device *virt_dev;
	struct xhci_hcd *xhci;
	int ep_index;
	struct usb_host_endpoint *ep;
	struct scatterlist *sg;
	void *tmp_sg;
	int cur_sg_len, num_sgs, data_length_left;
	unsigned int tmp_sg_len;
	int ret;
	int i, j;
	struct list_head sg_head, *next;
	struct mtk_sg *tmp_mtk_sg;
	dma_addr_t cur_transfer_dma;

	ret = 0;
	xhci = hcd_to_xhci(my_hcd);
	dev = xhci_to_hcd(xhci)->self.controller;//dma stream buffer
	rhdev = my_hcd->self.root_hub;
	if(usbdev){
		udev = usbdev;
	}
	else{
		udev = rhdev->children[g_port_id-1];
	}
	if(dir==URB_DIR_OUT){
		ep = udev->ep_out[ep_num];
	}else{
		ep = udev->ep_in[ep_num];
	}
	ep_index = mtktest_xhci_get_endpoint_index(&ep->desc);
	num_sgs = 0;
	if(sg_len != 0){
		num_sgs = data_length/sg_len;
		if(data_length % sg_len != 0){
			num_sgs++;
		}
		tmp_sg = kmalloc((sizeof(struct scatterlist)*num_sgs), GFP_KERNEL);
		sg = tmp_sg;
		for(i=0; i<num_sgs; i++){
			xhci_dbg(xhci, "sg address 0x%p\n", sg);
			if(i == (num_sgs-1)){
				sg->page_link = 0x02;
			}
			else{
				sg->page_link = 0x0;
			}
			if((data_length - (i*sg_len)) > sg_len){
				cur_sg_len = sg_len;
			}
			else{
				cur_sg_len = (data_length - (i*sg_len));
			}
//			sg->dma_length = cur_sg_len;
			sg->length = cur_sg_len;
			sg->dma_address = urb->transfer_dma + (i*sg_len);

			sg++;

		}
	}
	else{
		data_length_left = data_length;
		//random sg_len from 512 ~ 4096
		INIT_LIST_HEAD(&sg_head);
		while(data_length_left > 0){
			if(data_length_left < 512){
				cur_sg_len = data_length_left;
			}
			else{
				cur_sg_len = (get_random_int() + MIN_SG_LENGTH)%MAX_SG_LENGTH;
			}
			num_sgs++;
			tmp_mtk_sg = kmalloc(sizeof(struct mtk_sg), GFP_KERNEL);
			tmp_mtk_sg->sg_len = cur_sg_len;
			list_add_tail(&tmp_mtk_sg->list, &sg_head);
		}
		tmp_sg = kmalloc((sizeof(struct scatterlist)*num_sgs), GFP_KERNEL);
		cur_transfer_dma = urb->transfer_dma;
		sg = tmp_sg;
		for(i=0; i<num_sgs; i++){
			tmp_mtk_sg = (struct mtk_sg *)list_entry(sg_head.next, struct mtk_sg, list);
			sg->length = tmp_mtk_sg->sg_len;
			sg->dma_address = cur_transfer_dma;
			cur_transfer_dma = cur_transfer_dma + tmp_mtk_sg->sg_len;
			sg++;
			list_del(sg_head.next);
			kfree(tmp_mtk_sg);
		}
	}
	urb->sg = tmp_sg;
	urb->num_sgs = num_sgs;
	return ret;
}

/* Return the maximum endpoint service interval time (ESIT) payload.
 * Basically, this is the maxpacket size, multiplied by the burst size
 * and mult size.
 */
static inline u32 mtk_xhci_get_max_esit_payload(struct xhci_hcd *xhci,
		struct usb_device *udev,
		struct usb_host_endpoint *ep)
{
	int max_burst;
	int max_packet;

	/* Only applies for interrupt or isochronous endpoints */
	if (usb_endpoint_xfer_control(&ep->desc) ||
			usb_endpoint_xfer_bulk(&ep->desc))
		return 0;

	if (udev->speed == USB_SPEED_SUPER)
		return ep->ss_ep_comp.wBytesPerInterval;

//	max_packet = ep->desc.wMaxPacketSize & 0x3ff;
	max_packet = ep->desc.wMaxPacketSize & 0x7ff;
	max_burst = (ep->desc.wMaxPacketSize & 0x1800) >> 11;
	/* A 0 in max burst means 1 transfer per ESIT */
	return max_packet * (max_burst + 1);
}

int f_loopback_loop(int ep_out, int ep_in, int data_length, int start_add, struct usb_device *usbdev){
	struct device	*dev;
	struct usb_device *udev, *rhdev;
	struct xhci_virt_device *virt_dev;
	struct xhci_hcd *xhci;
	struct urb *urb_tx, *urb_rx;
	struct usb_host_endpoint *ep_tx, *ep_rx;
	int ep_index_tx, ep_index_rx;
	void *buffer_tx, *buffer_rx;
	u8 *tmp1, *tmp2;
	//int data_length;
	int num_sgs;
	int ret;
	int i;
	int max_esit_payload;
	int iso_num_packets;

	ret = 0;
	xhci = hcd_to_xhci(my_hcd);
	dev = xhci_to_hcd(xhci)->self.controller;//dma stream buffer
	rhdev = my_hcd->self.root_hub;
	if(usbdev){
		udev = usbdev;
	}
	else{
		udev = rhdev->children[g_port_id-1];
	}
	ep_tx = udev->ep_out[ep_out];
	ep_rx = udev->ep_in[ep_in];
	ep_index_tx = mtktest_xhci_get_endpoint_index(&ep_tx->desc);
	ep_index_rx = mtktest_xhci_get_endpoint_index(&ep_rx->desc);
	if(usb_endpoint_xfer_isoc(&ep_tx->desc)){
		//if superspeed, need to add ss_ep_comp.wBytesPerInterval in ep structure
		max_esit_payload = mtk_xhci_get_max_esit_payload(xhci, udev, ep_tx);
//		printk(KERN_ERR "[Debug] max_esit_payload: %d\n", max_esit_payload);
		if(data_length%max_esit_payload == 0){
			iso_num_packets = data_length/max_esit_payload;
		}
		else{
			iso_num_packets = data_length/max_esit_payload + 1;
		}
	}
	else{
		iso_num_packets = 0;
	}
	if(usb_endpoint_xfer_isoc(&ep_tx->desc)){
		msleep(1000);
	}
	//Tx
	urb_tx = usb_alloc_urb(iso_num_packets, GFP_KERNEL);
	ret = f_fill_urb(urb_tx,ep_out,data_length,start_add,URB_DIR_OUT, iso_num_packets, max_esit_payload, udev);
	if(ret){
		xhci_err(xhci, "[ERROR]fill tx urb Error!!\n");
		return RET_FAIL;
	}
	ret = f_queue_urb(urb_tx,1, udev);
	if(ret){
		xhci_err(xhci, "[ERROR]tx urb transfer failed!!\n");
		return RET_FAIL;
	}
	if(usb_endpoint_xfer_isoc(&ep_tx->desc)){
		msleep(1000);
	}
	//Rx
	urb_rx = usb_alloc_urb(iso_num_packets, GFP_NOIO);
	ret = f_fill_urb(urb_rx,ep_in,data_length,start_add,URB_DIR_IN, iso_num_packets, max_esit_payload, udev);
	if(ret){
		xhci_err(xhci, "[ERROR]fill rx urb Error!!\n");
		return RET_FAIL;
	}
	ret = f_queue_urb(urb_rx,1,udev);
	if(ret){
		xhci_err(xhci, "[ERROR]rx urb transfer failed!!\n");
		return RET_FAIL;
	}
	dma_sync_single_for_cpu(dev,urb_tx->transfer_dma-start_add,data_length+start_add,DMA_BIDIRECTIONAL);
	dma_sync_single_for_cpu(dev,urb_rx->transfer_dma-start_add,data_length+start_add,DMA_BIDIRECTIONAL);
	//Compare
	for(i=0; i<urb_tx->transfer_buffer_length; i++){
		tmp1 = urb_tx->transfer_buffer+i;
		tmp2 = urb_rx->transfer_buffer+i;
		if((*tmp1) != (*tmp2)){
			xhci_err(xhci, "[ERROR] buffer %d not match, tx 0x%x, rx 0x%x\n", i, *tmp1, *tmp2);
			ret = RET_FAIL;
			break;
		}
	}
	xhci_dbg(xhci, "Buffer compared done\n");
	f_free_urb(urb_tx,data_length,start_add);
	f_free_urb(urb_rx,data_length,start_add);
	return ret;
}

int f_loopback_sg_loop(int ep_out, int ep_in, int data_length, int start_add, int sg_len, struct usb_device *usbdev){
	//if sg_len = 0, means random sg_length
	//can not test isoc transfer type
	struct device	*dev;
	struct usb_device *udev, *rhdev;
	struct xhci_virt_device *virt_dev;
	struct xhci_hcd *xhci;
	struct urb *urb_tx, *urb_rx;
	struct usb_host_endpoint *ep_tx, *ep_rx;
	int ep_index_tx, ep_index_rx;
	int sg_length;
	void *buffer_tx, *buffer_rx;
	u8 *tmp1, *tmp2;
	//int data_length;
	int num_sgs;
	int ret;
	int i;
	int max_esit_payload;

	ret = 0;
	dev = xhci_to_hcd(xhci)->self.controller;//dma stream buffer
	xhci = hcd_to_xhci(my_hcd);
	rhdev = my_hcd->self.root_hub;
	if(usbdev){
		udev = usbdev;
	}
	else{
		udev = rhdev->children[g_port_id-1];
	}
	ep_tx = udev->ep_out[ep_out];
	ep_rx = udev->ep_in[ep_in];
	ep_index_tx = mtktest_xhci_get_endpoint_index(&ep_tx->desc);
	ep_index_rx = mtktest_xhci_get_endpoint_index(&ep_rx->desc);

	urb_tx = usb_alloc_urb(0, GFP_KERNEL);
	ret = f_fill_urb(urb_tx,ep_out,data_length,start_add,URB_DIR_OUT
		, 0, max_esit_payload, udev);
	if(ret){
		xhci_err(xhci, "[ERROR]fill tx urb Error!!\n");
		return RET_FAIL;
	}
	ret = f_fill_urb_sg(urb_tx, URB_DIR_OUT, ep_out, data_length, sg_len, udev);
	if(ret){
		xhci_err(xhci, "[ERROR]fill tx urb sg Error!!\n");
		return RET_FAIL;
	}
	ret = f_queue_urb(urb_tx,1,udev);
	if(ret){
		xhci_err(xhci, "[ERROR]tx urb transfer failed!!\n");
		return RET_FAIL;
	}
	//Rx
	urb_rx = usb_alloc_urb(0, GFP_NOIO);
	ret = f_fill_urb(urb_rx,ep_in,data_length,start_add,URB_DIR_IN, 0, max_esit_payload, udev);
	if(ret){
		xhci_err(xhci, "[ERROR]fill rx urb Error!!\n");
		return RET_FAIL;
	}
	ret = f_fill_urb_sg(urb_rx, URB_DIR_IN, ep_in, data_length, sg_len, udev);
	if(ret){
		xhci_err(xhci, "[ERROR]fill tx urb sg Error!!\n");
		return RET_FAIL;
	}
	ret = f_queue_urb(urb_rx,1,udev);
	if(ret){
		xhci_err(xhci, "[ERROR]rx urb transfer failed!!\n");
		return RET_FAIL;
	}
	dma_sync_single_for_cpu(dev,urb_tx->transfer_dma-start_add,data_length+start_add,DMA_BIDIRECTIONAL);
	dma_sync_single_for_cpu(dev,urb_rx->transfer_dma-start_add,data_length+start_add,DMA_BIDIRECTIONAL);
	//Compare
	for(i=0; i<urb_tx->transfer_buffer_length; i++){
		tmp1 = urb_tx->transfer_buffer+i;
		tmp2 = urb_rx->transfer_buffer+i;
		if((*tmp1) != (*tmp2)){
			xhci_err(xhci, "[ERROR] buffer %d not match, tx 0x%x, rx 0x%x\n", i, *tmp1, *tmp2);
			ret = RET_FAIL;
			break;
		}
	}
	xhci_dbg(xhci, "Buffer compared done\n");
	kfree(urb_tx->sg);
	kfree(urb_rx->sg);
	f_free_urb(urb_tx,data_length,start_add);
	f_free_urb(urb_rx,data_length,start_add);
	return ret;
}

int f_loopback_loop_gpd(int ep_out, int ep_in, int data_length, int start_add, int gpd_length, struct usb_device *usbdev){
	struct xhci_hcd *xhci;
	struct device	*dev;
	struct usb_device *udev, *rhdev;
	int ret, i;
   	u8 *tmp1, *tmp2;
	struct urb *urb_tx, *urb_rx;
	int iso_num_packets;
	struct usb_host_endpoint *ep_tx, *ep_rx;
	int max_esit_payload;
	int num_urbs;
	void *buffer_tx, *buffer_rx;
	dma_addr_t mapping_tx, mapping_rx;
	int tmp_urb_len, total_running;

	ret = 0;
	iso_num_packets = 0;
	xhci = hcd_to_xhci(my_hcd);
	dev = xhci_to_hcd(xhci)->self.controller;//dma stream buffer
	if(!usbdev){
		rhdev = my_hcd->self.root_hub;
		udev = rhdev->children[g_port_id-1];
	}
	else{
		udev = usbdev;
	}
	ep_tx = udev->ep_out[ep_out];
	ep_rx = udev->ep_in[ep_in];

	ret = 0;
	buffer_tx = kmalloc(data_length+start_add, GFP_KERNEL);
	mapping_tx = dma_map_single(dev, buffer_tx,data_length+start_add, DMA_BIDIRECTIONAL);
	xhci_dbg(xhci, "buffer_tx 0x%p dma 0x%p\n", buffer_tx, (void *)(unsigned long)mapping_tx);
	num_urbs = data_length/gpd_length;
	if(data_length % gpd_length != 0){
		num_urbs++;
	}
	xhci_dbg(xhci, "[LOOPBACK]Num urbs: %d\n", num_urbs);
	total_running = 0;
	xhci_dbg(xhci, "[LOOPBACK]Start to do Tx, buffer 0x%p, mapping 0x%p\n", buffer_tx, (void *)(unsigned long)mapping_tx);

	for(i=0; i<num_urbs; i++){
		if(usb_endpoint_xfer_isoc(&ep_tx->desc)){
			mdelay(1000);
		}
		tmp_urb_len = min_t(int, gpd_length, (data_length-total_running));
		if(usb_endpoint_xfer_isoc(&ep_tx->desc)){
			//if superspeed, need to add ss_ep_comp.wBytesPerInterval in ep structure
			max_esit_payload = mtk_xhci_get_max_esit_payload(xhci, udev, ep_tx);
			xhci_dbg(xhci, "[Debug] max_esit_payload: %d\n", max_esit_payload);
			if(tmp_urb_len%max_esit_payload == 0){
				iso_num_packets = tmp_urb_len/max_esit_payload + 1;
			}
			else{
				iso_num_packets = tmp_urb_len/max_esit_payload + 1;
			}
		}
		xhci_dbg(xhci, "[LOOPBACK]Tx round %d, urb_length %d, buffer 0x%p, mapping 0x%p\n"
			, i, tmp_urb_len, buffer_tx+total_running, (void *)(unsigned long)(mapping_tx+total_running));
		urb_tx = usb_alloc_urb(iso_num_packets, GFP_KERNEL);
		f_fill_urb_with_buffer(urb_tx, ep_out, tmp_urb_len, buffer_tx+total_running
			, start_add, URB_DIR_OUT, iso_num_packets, max_esit_payload, mapping_tx+total_running, udev);
		if(ret){
			xhci_err(xhci, "[ERROR]fill tx urb Error!!\n");
			return RET_FAIL;
		}
//		printk(KERN_ERR "urb_tx buffer 0x%x transfer_dma 0x%x\n", urb_tx->transfer_buffer, urb_tx->transfer_dma);
		ret = f_queue_urb(urb_tx,1,NULL);
		if(ret){
			xhci_err(xhci, "[ERROR]queue tx urb Error!!\n");
			return RET_FAIL;
		}
		urb_tx->transfer_buffer = NULL;
		urb_tx->transfer_dma = NULL;
		usb_free_urb(urb_tx);
		total_running += tmp_urb_len;
	}


	buffer_rx = kmalloc(data_length+start_add, GFP_KERNEL);
	memset(buffer_rx, 0, data_length+start_add);
	mapping_rx = dma_map_single(dev, buffer_rx,data_length+start_add, DMA_BIDIRECTIONAL);
	xhci_dbg(xhci, "buffer_rx 0x%p dma 0x%p\n", buffer_rx, (void *)(unsigned long)mapping_rx);
	total_running = 0;
	xhci_dbg(xhci, "[LOOPBACK]Start to do Rx, buffer 0x%p, mapping 0x%p\n", buffer_rx, (void *)(unsigned long)mapping_rx);
	for(i=0; i<num_urbs; i++){
		if(usb_endpoint_xfer_isoc(&ep_tx->desc)){
			mdelay(1000);
		}
		tmp_urb_len = min_t(int, gpd_length, (data_length-total_running));
		if(usb_endpoint_xfer_isoc(&ep_tx->desc)){
			//if superspeed, need to add ss_ep_comp.wBytesPerInterval in ep structure
			max_esit_payload = mtk_xhci_get_max_esit_payload(xhci, udev, ep_tx);
//			printk(KERN_ERR "[Debug] max_esit_payload: %d\n", max_esit_payload);
			if(tmp_urb_len%max_esit_payload == 0){
				iso_num_packets = tmp_urb_len/max_esit_payload + 1;
			}
			else{
				iso_num_packets = tmp_urb_len/max_esit_payload + 1;
			}
		}
		urb_rx = usb_alloc_urb(iso_num_packets, GFP_KERNEL);
		xhci_dbg(xhci, "[LOOPBACK]Rx round %d, urb_length %d, buffer 0x%p, mapping 0x%p\n"
			, i, tmp_urb_len, buffer_rx+total_running, (void *)(unsigned long)(mapping_rx+total_running));
		f_fill_urb_with_buffer(urb_rx, ep_in, tmp_urb_len, buffer_rx+total_running
			, start_add, URB_DIR_IN, iso_num_packets, max_esit_payload, mapping_rx+total_running,udev);
		xhci_dbg(xhci, "urb_rx buffer 0x%p transfer_dma 0x%p\n", urb_rx->transfer_buffer, (void *)(unsigned long)urb_rx->transfer_dma);
		if(ret){
			xhci_err(xhci, "[ERROR]fill tx urb Error!!\n");
			return RET_FAIL;
		}
		ret = f_queue_urb(urb_rx,1,NULL);
		if(ret){
			xhci_err(xhci, "[ERROR]queue rx urb Error!!\n");
			return RET_FAIL;
		}
		urb_rx->transfer_buffer = NULL;
		urb_rx->transfer_dma = NULL;
		usb_free_urb(urb_rx);
		total_running += tmp_urb_len;
	}
	dma_sync_single_for_cpu(dev,mapping_tx,data_length+start_add,DMA_BIDIRECTIONAL);
	dma_sync_single_for_cpu(dev,mapping_rx,data_length+start_add,DMA_BIDIRECTIONAL);
	for(i=0; i<data_length; i++){
		tmp1 = buffer_tx+i+start_add;
		tmp2 = buffer_rx+i+start_add;
		if((*tmp1) != (*tmp2)){
			xhci_err(xhci, "[ERROR] buffer %d not match, tx buf 0x%p, rx buf 0x%p, tx 0x%x, rx 0x%x\n"
				, i, tmp1, tmp2, *tmp1, *tmp2);
			ret = RET_FAIL;
			break;
		}
	}
	kfree(buffer_tx);
	kfree(buffer_rx);
	return ret;
}

int f_loopback_sg_loop_gpd(int ep_out, int ep_in, int data_length, int start_add, int sg_len, int gpd_length, struct usb_device *usbdev){
	struct xhci_hcd *xhci;
	struct device	*dev;
	struct usb_device *udev, *rhdev;
	int ret, i;;
   	u8 *tmp1, *tmp2;
	struct urb *urb_tx, *urb_rx;
	int iso_num_packets;
	struct usb_host_endpoint *ep_tx, *ep_rx;
	int max_esit_payload;
	int num_urbs;
	void *buffer_tx, *buffer_rx;
	dma_addr_t mapping_tx, mapping_rx;
	int tmp_urb_len, total_running;

	ret = 0;
	iso_num_packets = 0;
	xhci = hcd_to_xhci(my_hcd);
	dev = xhci_to_hcd(xhci)->self.controller;//dma stream buffer
	if(!usbdev){
		rhdev = my_hcd->self.root_hub;
		udev = rhdev->children[g_port_id-1];
	}
	else{
		udev = usbdev;
	}
	ep_tx = udev->ep_out[ep_out];
	ep_rx = udev->ep_in[ep_in];

	ret = 0;
	buffer_tx = kmalloc(data_length+start_add, GFP_KERNEL);
	mapping_tx = dma_map_single(dev, buffer_tx,data_length+start_add, DMA_BIDIRECTIONAL);
	num_urbs = data_length/gpd_length;
	if(data_length % gpd_length != 0){
		num_urbs++;
	}
	xhci_dbg(xhci, "[LOOPBACK]Num urbs: %d\n", num_urbs);
	total_running = 0;
	xhci_dbg(xhci, "[LOOPBACK]Start to do Tx, buffer 0x%p, mapping 0x%p\n", buffer_tx, (void *)(unsigned long)mapping_tx);
	for(i=0; i<num_urbs; i++){

		tmp_urb_len = min_t(int, gpd_length, (data_length-total_running));
		if(usb_endpoint_xfer_isoc(&ep_tx->desc)){
			//if superspeed, need to add ss_ep_comp.wBytesPerInterval in ep structure
			max_esit_payload = mtk_xhci_get_max_esit_payload(xhci, udev, ep_tx);
//			printk(KERN_ERR "[Debug] max_esit_payload: %d\n", max_esit_payload);
			if(tmp_urb_len%max_esit_payload == 0){
				iso_num_packets = tmp_urb_len/max_esit_payload;
			}
			else{
				iso_num_packets = tmp_urb_len/max_esit_payload + 1;
			}
		}
		xhci_dbg(xhci, "[LOOPBACK]Tx round %d, urb_length %d, buffer 0x%p, mapping 0x%p\n"
			, i, tmp_urb_len, buffer_tx+total_running, (void *)(unsigned long)(mapping_tx+total_running));
		urb_tx = usb_alloc_urb(iso_num_packets, GFP_KERNEL);
		ret = f_fill_urb_with_buffer(urb_tx, ep_out, tmp_urb_len, buffer_tx+total_running
			, start_add, URB_DIR_OUT, iso_num_packets, max_esit_payload, mapping_tx+total_running, udev);
		if(ret){
			xhci_err(xhci, "[ERROR]fill tx urb Error!!\n");
			return RET_FAIL;
		}
		ret = f_fill_urb_sg(
			urb_tx,URB_DIR_OUT,ep_out,tmp_urb_len,sg_len,udev);
		if(ret){
			xhci_err(xhci, "[ERROR]fill tx urb sg Error!!\n");
			return RET_FAIL;
		}
		ret = f_queue_urb(urb_tx,1,NULL);
		if(ret){
			xhci_err(xhci, "[ERROR]queue tx urb Error!!\n");
			return RET_FAIL;
		}
		urb_tx->transfer_buffer = NULL;
		urb_tx->transfer_dma = NULL;
		usb_free_urb(urb_tx);
		total_running += tmp_urb_len;
	}
	if(usb_endpoint_xfer_isoc(&ep_tx->desc)){
		mdelay(500);
	}

	buffer_rx = kmalloc(data_length+start_add, GFP_KERNEL);
	memset(buffer_rx, 0, data_length+start_add);
	mapping_rx = dma_map_single(dev, buffer_rx,data_length+start_add, DMA_BIDIRECTIONAL);

	total_running = 0;
	xhci_dbg(xhci, "[LOOPBACK]Start to do Rx, buffer 0x%p, mapping 0x%p\n", buffer_rx, (void *)(unsigned long)mapping_rx);
	for(i=0; i<num_urbs; i++){
		tmp_urb_len = min_t(int, gpd_length, (data_length-total_running));
		if(usb_endpoint_xfer_isoc(&ep_tx->desc)){
			//if superspeed, need to add ss_ep_comp.wBytesPerInterval in ep structure
			max_esit_payload = mtk_xhci_get_max_esit_payload(xhci, udev, ep_tx);
//			printk(KERN_ERR "[Debug] max_esit_payload: %d\n", max_esit_payload);
			if(tmp_urb_len%max_esit_payload == 0){
				iso_num_packets = tmp_urb_len/max_esit_payload;
			}
			else{
				iso_num_packets = tmp_urb_len/max_esit_payload + 1;
			}
		}
		urb_rx = usb_alloc_urb(iso_num_packets, GFP_KERNEL);
		xhci_dbg(xhci, "[LOOPBACK]Rx round %d, urb_length %d, buffer 0x%p, mapping 0x%p\n"
			, i, tmp_urb_len, buffer_rx+total_running, (void *)(unsigned long)(mapping_rx+total_running));
		ret = f_fill_urb_with_buffer(urb_rx, ep_in, tmp_urb_len, buffer_rx+total_running
			, start_add, URB_DIR_IN, iso_num_packets, max_esit_payload, mapping_rx+total_running,udev);
		if(ret){
			xhci_err(xhci, "[ERROR]fill tx urb Error!!\n");
			return RET_FAIL;
		}
		ret = f_fill_urb_sg(
			urb_rx,URB_DIR_IN,ep_in,tmp_urb_len,sg_len,udev);
		if(ret){
			xhci_err(xhci, "[ERROR]fill rx urb sg Error!!\n");
			return RET_FAIL;
		}
		ret = f_queue_urb(urb_rx,1,NULL);
		if(ret){
			xhci_err(xhci, "[ERROR]queue rx urb Error!!\n");
			return RET_FAIL;
		}
		urb_rx->transfer_buffer = NULL;
		urb_rx->transfer_dma = NULL;
		usb_free_urb(urb_rx);
		total_running += tmp_urb_len;
	}
	dma_sync_single_for_cpu(dev,mapping_tx,data_length+start_add,DMA_BIDIRECTIONAL);
	dma_sync_single_for_cpu(dev,mapping_rx,data_length+start_add,DMA_BIDIRECTIONAL);
	for(i=0; i<data_length; i++){
		tmp1 = buffer_tx+i+start_add;
		tmp2 = buffer_rx+i+start_add;
		if((*tmp1) != (*tmp2)){
			xhci_err(xhci, "[ERROR] buffer %d not match, tx 0x%x, rx 0x%x\n", i, *tmp1, *tmp2);
			ret = RET_FAIL;
			break;
		}
	}
	kfree(buffer_tx);
	kfree(buffer_rx);
	return ret;
}


struct loopback_data {
		int dev_num;
        struct xhci_hcd *xhci;
        struct usb_device *udev;
		int out_ep_num;
		int in_ep_num;
		struct mutex *lock;
		char is_ctrl;
};

static int loopback_thread(void *data){
	struct loopback_data *lb_data = data;
	char bdp;
	short gpd_buf_size,bd_buf_size;
	struct device	*dev;
	struct usb_device *udev, *rhdev;
	struct xhci_virt_device *virt_dev;
	struct xhci_hcd *xhci;
	struct urb *urb_tx, *urb_rx;
	struct usb_host_endpoint *ep_tx, *ep_rx;
	int ep_index_tx, ep_index_rx;
	int sg_length, length, start_add;
	void *buffer_tx, *buffer_rx;
	u8 *tmp1, *tmp2;
	int ret;
	int i;
	int max_esit_payload;

	ret = 0;

	dev = xhci_to_hcd(xhci)->self.controller;
	xhci = lb_data->xhci;
	udev = lb_data->udev;

	if(lb_data->is_ctrl){
		do{
			length = ((get_random_int() % 128) + 1) * 4;
			xhci_err(xhci, "[CTRL LOOPBACK] dev %d, length %d\n"
				, lb_data->dev_num, length);
			mutex_lock(lb_data->lock);
			ret = dev_ctrl_loopback(length,udev);
			if(ret){
				xhci_err(xhci, "control loopback fail!!\n");
				g_stress_status = RET_FAIL;
				break;
			}
			mutex_unlock(lb_data->lock);
		}while(g_stress_status != RET_FAIL);
	}
	else{
		ep_tx = udev->ep_out[lb_data->out_ep_num];
		ep_rx = udev->ep_in[lb_data->in_ep_num];
		ep_index_tx = mtktest_xhci_get_endpoint_index(&ep_tx->desc);
		ep_index_rx = mtktest_xhci_get_endpoint_index(&ep_rx->desc);
		do{
			length = (get_random_int() % 65023) + 1;
			start_add = get_random_int() % 64;
			sg_length = (get_random_int() % 4) * 1024;

			bdp=1;
		    gpd_buf_size=length;
			bd_buf_size=8192;

			xhci_err(xhci, "[LOOPBACK] dev %d, ep_out %d, ep_in %d, length %d, start_add %d, sg_length %d\n"
				, lb_data->dev_num, lb_data->out_ep_num, lb_data->in_ep_num, length, start_add, sg_length);
			mutex_lock(lb_data->lock);
			ret=dev_loopback(bdp,length,gpd_buf_size,bd_buf_size, 0, 0,udev);
			if(ret)
			{
				xhci_err(xhci, "loopback request fail!!\n");
				g_stress_status = RET_FAIL;
				break;
			}
			mutex_unlock(lb_data->lock);
			if((length > sg_length) && sg_length != 0 && (length/sg_length < 60)){
				ret = f_loopback_sg_loop(
					lb_data->out_ep_num, lb_data->in_ep_num, length, start_add, sg_length, udev);
			}
			else{
				ret = f_loopback_loop(lb_data->out_ep_num, lb_data->in_ep_num, length, start_add, udev);
			}
			if(ret)
			{
				xhci_err(xhci, "loopback fail!!\n");
				g_stress_status = RET_FAIL;
				break;
			}
		}while(g_stress_status != RET_FAIL);
	}
}

int f_add_loopback_thread(struct xhci_hcd *xhci, int dev_num
	, struct usb_device *udev, int out_ep_num, int in_ep_num, struct mutex *lock, char is_ctrl){

	struct loopback_data *lb_data;
	xhci_err(xhci, "[LOOPBACK]Start loopback thread, devnum %d, ep_out %d, ep_in %d, isctrl %d\n"
		, dev_num, out_ep_num, in_ep_num, is_ctrl);
	lb_data = kzalloc(sizeof(struct loopback_data), GFP_KERNEL);
	lb_data->xhci = xhci;
	lb_data->dev_num = dev_num;
	lb_data->udev = udev;
	lb_data->out_ep_num = out_ep_num;
	lb_data->in_ep_num = in_ep_num;
	lb_data->lock = lock;
	lb_data->is_ctrl = is_ctrl;
	kthread_run(loopback_thread, lb_data, "lbt");
}

int f_slot_reset_device(int slot_id, char isWarmReset){
	int ret, slot_state, i;
	struct xhci_hcd *xhci;
	struct xhci_virt_device *virt_dev;
//	struct xhci_container_ctx *out_ctx;
	struct xhci_slot_ctx *out_ctx;
	struct xhci_ep_ctx *ep_ctx;
	struct xhci_port *port;
	int last_freed_endpoint;
	u32 __iomem *addr;
	int temp;
	int c_slot_id, port_id, port_index;
	unsigned long flags;

	ret = 0;

//	isWarmReset = false;
	c_slot_id = slot_id;
	if(c_slot_id == 0)
		c_slot_id = g_slot_id;
	if(c_slot_id == 0){
		printk(KERN_ERR "[ERROR] slot ID not valid\n");
	}
	xhci = hcd_to_xhci(my_hcd);
	port_id = get_port_id(c_slot_id);
	port_index = get_port_index(port_id);
	port = rh_port[port_index];
	port->port_status = RESET;

//	g_port_reset = false;
	mtktest_enablePortClockPower(port_index,0x3);
	if(isWarmReset && port->port_speed == USB_SPEED_SUPER){
		//do warm reset
		addr = &xhci->op_regs->port_status_base + NUM_PORT_REGS*(port_id-1 & 0xff);
		temp = xhci_readl(xhci, addr);
		xhci_dbg(xhci, "before reset port %d = 0x%x\n", port_id-1, temp);
		temp = mtktest_xhci_port_state_to_neutral(temp);
		temp = (temp | PORT_WR);
		xhci_writel(xhci, temp, addr);
		temp = xhci_readl(xhci, addr);
		xhci_dbg(xhci, "after reset port %d = 0x%x\n", port_id-1, temp);

	}
	else{
		//hot reset port
		addr = &xhci->op_regs->port_status_base + NUM_PORT_REGS*(port_id-1 & 0xff);
		temp = xhci_readl(xhci, addr);
		xhci_dbg(xhci, "before reset port %d = 0x%x\n", port_id-1, temp);
		temp = mtktest_xhci_port_state_to_neutral(temp);
		temp = (temp | PORT_RESET);
		xhci_writel(xhci, temp, addr);
	}
	wait_event_on_timeout(&(port->port_status), ENABLED, ATTACH_TIMEOUT);
	if(port->port_status != ENABLED){
		xhci_err(xhci, "[ERROR]Device reset timeout\n");
		return RET_FAIL;
	}
	xhci_dbg(xhci, "port reset done\n");

	virt_dev = xhci->devs[g_slot_id];
	g_cmd_status = CMD_RUNNING;
	spin_lock_irqsave(&xhci->lock, flags);
	ret = mtktest_xhci_queue_reset_device(xhci, c_slot_id);
	spin_unlock_irqrestore(&xhci->lock, flags);

	
	if (ret){
		xhci_err(xhci, "[ERROR]FIXME: allocate a command ring segment\n");
		return ret;
	}
	xhci_dbg(xhci, "reset dev command\n");
	mtktest_xhci_ring_cmd_db(xhci);
	wait_event_on_timeout(&g_cmd_status, CMD_DONE, CMD_TIMEOUT);
	if(g_cmd_status == CMD_RUNNING){
		xhci_err(xhci, "[ERROR]reset device command timeout\n");
		return RET_FAIL;
	}
	if(g_cmd_status == CMD_FAIL){
		xhci_err(xhci, "[ERROR]reset device command failed\n");
		return RET_FAIL;
	}
	xhci_dbg(xhci, "reset device success\n");
#if 0
	slot_state = mtktest_xhci_get_slot_state(xhci, virt_dev->out_ctx);
	print_slot_state(slot_state);
#endif
	xhci_dbg(xhci, "Output Control Context, after:\n");
	mtktest_xhci_dbg_slot_ctx(xhci, virt_dev->out_ctx);
	//check parameters
	out_ctx = mtktest_xhci_get_slot_ctx(xhci, virt_dev->out_ctx);
	if(LAST_CTX_TO_EP_NUM(out_ctx->dev_info) != 0){
		xhci_err(xhci, "[FAIL] slot context entries is not 1 after reset device command\n");
		ret = RET_FAIL;
	}
	if(GET_SLOT_STATE(out_ctx->dev_state) != SLOT_STATE_DEFAULT){
		xhci_err(xhci, "[FAIL] slot state is not default after reset device command\n");
		ret = RET_FAIL;
	}
	/* Everything but endpoint 0 is disabled, so free or cache the rings. */
	last_freed_endpoint = 1;
	for (i = 1; i < 31; ++i) {
		if (!virt_dev->eps[i].ring)
			continue;
		mtktest_xhci_free_or_cache_endpoint_ring(xhci, virt_dev, i);
		last_freed_endpoint = i;
	}
	xhci_dbg(xhci, "Output context after successful reset device cmd:\n");
	mtktest_xhci_dbg_ctx(xhci, virt_dev->out_ctx, last_freed_endpoint);
	//check ep_ctx
	ep_ctx = mtktest_xhci_get_ep_ctx(xhci, virt_dev->out_ctx, 0);
	if( (ep_ctx->ep_info & EP_STATE_MASK) != EP_STATE_STOPPED){
		xhci_err(xhci, "[FAIL] EP0 state is not in stoped after reset device command\n");
		ret = RET_FAIL;
	}
	for (i = 1; i < 31; ++i) {
		ep_ctx = mtktest_xhci_get_ep_ctx(xhci, virt_dev->out_ctx, i);
		if((ep_ctx->ep_info & EP_STATE_MASK) != EP_STATE_DISABLED){
			xhci_err(xhci, "[FAIL] EP%d state is not in disabled after reset device command\n", i);
			ret = RET_FAIL;
		}
	}
	mtktest_xhci_slot_copy(xhci, virt_dev->in_ctx, virt_dev->out_ctx);
	return ret;
}

int f_power_suspend(){
	int ret, i;
	struct xhci_hcd *xhci;
	struct xhci_virt_device *virt_dev;
	struct xhci_port *port;
	u32 __iomem *addr;
	int temp;
	int port_index;
	int num_u3_port;

	ret = RET_SUCCESS;
#if 0
	if(g_slot_id == 0){
		printk(KERN_ERR "[ERROR] slot ID not valid\n");
		return RET_FAIL;
	}
#endif
	xhci = hcd_to_xhci(my_hcd);
	num_u3_port = SSUSB_U3_PORT_NUM(readl(SSUSB_IP_CAP));
	port_index = get_port_index(g_port_id);
	port = rh_port[port_index];
	//set power/clock gating
	if(!g_concurrent_resume){
		if(port->port_speed == USB_SPEED_SUPER){
			mtktest_disablePortClockPower((g_port_id-1), 0x3);
		}
		else{
			mtktest_disablePortClockPower((g_port_id-1-num_u3_port), 0x2);
		}
	}
	//set PLS = 3
	addr = &xhci->op_regs->port_status_base + NUM_PORT_REGS*(g_port_id-1 & 0xff);
	temp = xhci_readl(xhci, addr);
	xhci_dbg(xhci, "before reset port %d = 0x%x\n", g_port_id-1, temp);
	temp = mtktest_xhci_port_state_to_neutral(temp);
	temp = (temp & ~(0xf << 5));
	temp = (temp | (3 << 5) | PORT_LINK_STROBE);
	xhci_writel(xhci, temp, addr);
	if(!g_concurrent_resume){
		msleep(100);
		if(port->port_speed == USB_SPEED_SUPER){
			mtktest_enablePortClockPower((g_port_id-1), 0x3);
		}
		else{
			mtktest_enablePortClockPower((g_port_id-1-num_u3_port), 0x2);
		}
		mtk_xhci_handshake(xhci, addr, (15<<5), (3<<5), 3*1000);
		temp = xhci_readl(xhci, addr);
		if(PORT_PLS_VALUE(temp) != 3){
			xhci_err(xhci, "port not enter U3 state\n");
			ret = RET_FAIL;
		}
		else{
			if(port->port_speed == USB_SPEED_SUPER){
			    mtktest_disablePortClockPower((g_port_id-1), 0x3);
    		}
    		else{
    			mtktest_disablePortClockPower((g_port_id-1-num_u3_port), 0x2);
    		}
			//mtktest_disablePortClockPower();
			ret = RET_SUCCESS;
		}
	}
	return ret;
}

int f_power_resume(){
	struct xhci_hcd *xhci;
	struct xhci_virt_device *virt_dev;
	struct usb_device *udev, *rhdev;
	struct xhci_port *port;
	u32 __iomem *addr;
	int temp;
	int i;
	int port_index;
	int num_u3_port;
#if 0
	if(g_slot_id == 0){
		printk(KERN_ERR "[ERROR] slot ID not valid\n");
		return RET_FAIL;
	}
#endif
	xhci = hcd_to_xhci(my_hcd);
	num_u3_port = SSUSB_U3_PORT_NUM(readl(SSUSB_IP_CAP));
	port_index = get_port_index(g_port_id);
	port = rh_port[port_index];
	if(!g_concurrent_resume){
		if(port->port_speed == USB_SPEED_SUPER){
			mtktest_enablePortClockPower((g_port_id-1), 0x3);
		}
		else{
			mtktest_enablePortClockPower((g_port_id-1-num_u3_port), 0x2);
		}
	}
	addr = &xhci->op_regs->port_status_base + NUM_PORT_REGS*(g_port_id-1 & 0xff);
	temp = xhci_readl(xhci, addr);
	if(!g_concurrent_resume){
		xhci_dbg(xhci, "before resume port %d = 0x%x\n", g_port_id-1, temp);
		if(PORT_PLS(temp) != (3 << 5)){
			xhci_err(xhci, "port not in U3 state\n");
			return RET_FAIL;
		}
	}
	temp = mtktest_xhci_port_state_to_neutral(temp);
	temp = (temp & ~(0xf << 5));
	rhdev = my_hcd->self.root_hub;
	udev = rhdev->children[g_port_id-1];
	if(DEV_SUPERSPEED(temp)){
		//superspeed direct set U0
		temp = (temp | PORT_LINK_STROBE);
		xhci_writel(xhci, temp, addr);
	}
	else{
		//HS/FS, set resume for 20ms, then set U0
		temp = (temp | (15 << 5) | PORT_LINK_STROBE);
		xhci_writel(xhci, temp, addr);
		mdelay(20);
		temp = xhci_readl(xhci, addr);
		temp = mtktest_xhci_port_state_to_neutral(temp);
		temp = (temp & ~(0xf << 5));
		temp = (temp | PORT_LINK_STROBE);
		xhci_writel(xhci, temp, addr);
	}
	for(i=0; i<200; i++){
		temp = xhci_readl(xhci, addr);
		if(PORT_PLS_VALUE(temp) == 0){
			break;
		}
		msleep(1);
		
	}
	if(PORT_PLS_VALUE(temp) != 0){
		xhci_err(xhci, "port not return U0 state\n");
		return RET_FAIL;
	}
	else{
	}
	return RET_SUCCESS;
}

int f_power_remotewakeup(){
	struct xhci_hcd *xhci;
	struct xhci_virt_device *virt_dev;
	struct usb_device *udev, *rhdev;
	u32 __iomem *addr;
	struct xhci_port *port;
	int temp, ret,i;
	int num_u3_port;
	int port_index;

	if(g_slot_id == 0){
		printk(KERN_ERR "[ERROR] slot ID not valid\n");
		return RET_FAIL;
	}
	xhci = hcd_to_xhci(my_hcd);
	num_u3_port = SSUSB_U3_PORT_NUM(readl(SSUSB_IP_CAP));
	port_index = get_port_index(g_port_id);
	port = rh_port[port_index];	
	//suspend first
	addr = &xhci->op_regs->port_status_base + NUM_PORT_REGS*(g_port_id-1 & 0xff);
	temp = xhci_readl(xhci, addr);
	xhci_dbg(xhci, "before suspend port %d = 0x%x\n", g_port_id-1, temp);
	temp = mtktest_xhci_port_state_to_neutral(temp);
	temp = (temp & ~(0xf << 5));
	temp = (temp | (3 << 5) | PORT_LINK_STROBE);
	xhci_writel(xhci, temp, addr);
	/* go into the U3 state */
	mtk_xhci_handshake(xhci, addr, (15<<5), (3<<5), 30*1000);
	temp = xhci_readl(xhci, addr);
	//set power/clock gating
	if(PORT_PLS_VALUE(temp) != 3){
		xhci_err(xhci, "port not enter U3 state\n");
		return RET_FAIL;
	}
	else{
		xhci_err(xhci, "port enter U3 OK, OPSTATE(0x%x)\n", xhci_readl(xhci, SSUSB_U2_SYS_BASE + 0x60));
		if(port->port_speed == USB_SPEED_SUPER){
			xhci_err(xhci, "port speed super\n");
			mtktest_disablePortClockPower((g_port_id-1), 0x3);
		}
		else{
			xhci_err(xhci, "port speed not super\n");
			mtktest_disablePortClockPower((g_port_id-1-num_u3_port), 0x2);
		}
	}
	g_port_resume = 0;

	wait_event_on_timeout(&g_port_resume, 1, 5000);
	if(g_port_resume == 0){
		xhci_err(xhci, "port not in Resume state after timeout\n");
		return RET_FAIL;
	}
	else{
	}

	if(port->port_speed != USB_SPEED_SUPER){
		mdelay(20);
		//msleep(20);
	}

	port_index = get_port_index(g_port_id);
	port = rh_port[port_index];

	if(port->port_speed == USB_SPEED_SUPER){
		mtktest_enablePortClockPower((g_port_id-1), 0x3);
	}
	else{
		mtktest_enablePortClockPower((g_port_id-1-num_u3_port), 0x2);
	}

	temp = mtktest_xhci_port_state_to_neutral(temp);
	temp = (temp & ~(0xf << 5));
	temp = (temp | PORT_LINK_STROBE);


	xhci_writel(xhci, temp, addr);
	mtk_xhci_handshake(xhci, addr, (15<<5), 0, 10*1000);
	temp = xhci_readl(xhci, addr);

	if(PORT_PLS_VALUE(temp) != 0){//(temp & PORT_PLS(15)) != PORT_PLS(0)){
		xhci_err(xhci, "port not return to U0\n");
		return RET_FAIL;
	}
	else{
	}

	return RET_SUCCESS;
}


int f_power_set_u1u2(int u_num, int value1, int value2){
	int ret, i;
	struct xhci_hcd *xhci;
	u32 __iomem *addr;
	int temp;

	if(g_slot_id == 0){
		printk(KERN_ERR "[ERROR] slot ID not valid\n");
		return RET_FAIL;
	}

	xhci = hcd_to_xhci(my_hcd);
	//set PLS = n
	addr = &xhci->op_regs->port_power_base + NUM_PORT_REGS*(g_port_id-1 & 0xff);
	temp = xhci_readl(xhci, addr);
	if(u_num == 1){
		temp = temp & (~(0x000000ff));
		temp = temp | value1;
	}
	else if(u_num == 2){
		temp = temp & (~(0x0000ff00));
		temp = temp | (value1 << 8);
	}
	else if(u_num == 3){
		temp = temp & (~(0x0000ffff));
		temp = temp | (value1) | (value2 << 8);
	}
	if(u_num >= 2 && value2 > 0){
		mtktest_disablePortClockPower(g_port_id-1, 0x3);
	}
	else if(u_num >=2 && value2 == 0){
		mtktest_enablePortClockPower(g_port_id-1, 0x3);
	}
	xhci_writel(xhci, temp, addr);
	return RET_SUCCESS;
}

#define USB3_U1_STATE_INFO	(SSUSB_U3_MAC_BASE + 0x50)	//0xf0042450
#define USB3_U2_STATE_INFO	(SSUSB_U3_MAC_BASE + 0x54)	//0xf0042454

int f_power_reset_u1u2_counter(int u_num){
	int temp = (1<<16);
	if(u_num == 1){
		writel(temp, USB3_U1_STATE_INFO);
	}
	else if(u_num == 2){
		writel(temp, USB3_U2_STATE_INFO);
	}
	return RET_SUCCESS;
}

int f_power_get_u1u2_counter(int u_num){
	if(u_num == 1){
		return (readl(USB3_U1_STATE_INFO) & 0xff);
	}
	else if(u_num == 2){
		return (readl(USB3_U2_STATE_INFO) & 0xff);
	}
}

int f_power_send_fla(int value){
	int ret;
	struct xhci_hcd *xhci;
	u32 __iomem *addr;
	int temp;

	if(g_slot_id == 0){
		printk(KERN_ERR "[ERROR] slot ID not valid\n");
		return RET_FAIL;
	}
	xhci = hcd_to_xhci(my_hcd);
	addr = &xhci->op_regs->port_power_base + NUM_PORT_REGS*(g_port_id-1 & 0xff);
	temp = xhci_readl(xhci, addr);
	temp &= ~(1<<16);
	temp |= ((value & 0x1)<<16);
	xhci_writel(xhci, temp, addr);
	return RET_SUCCESS;
}

int f_power_reset_L1_counter(int direct){
	//direct: 1:ENTRY, 2:EXIT
	int temp;
	if(direct == 1){
		temp = (1<<8);
	}
	else if (direct == 2){
		temp = (1<<9);
	}
	writel(temp, USB20_LPM_ENTRY_COUNT);
}

int f_power_get_L1_counter(int direct){
	//direct: 1:ENTRY, 2:EXIT
	if(direct == 1){
		return (readl(USB20_LPM_ENTRY_COUNT) && 0xff);
	}
	else if (direct == 2){
		return ((readl(USB20_LPM_ENTRY_COUNT) && 0xff0000) >> 16);
	}

}

int f_power_get_l1s(void)
{
	struct xhci_hcd *xhci;
	struct xhci_port *port;
	u32 __iomem *addr;
	int temp;

	xhci = hcd_to_xhci(my_hcd);
	//get L1S
	addr = &xhci->op_regs->port_power_base + NUM_PORT_REGS*((g_port_id-1) & 0xff);
	temp = xhci_readl(xhci, addr) & MSK_L1S;

	return temp;
}

int f_power_config_lpm(u32 slot_id, u32 hirdm, u32 L1_timeout, u32 rwe, u32 besl, u32 besld, u32 hle
	, u32 int_nak_active, u32 bulk_nyet_active){
	struct xhci_hcd *xhci;
	struct xhci_port *port;
	int port_index;
	u32 __iomem *addr;
	int ret;
	int temp;
	int num_u3_port;

	xhci = hcd_to_xhci(my_hcd);
	//set bulk_nyet_active_mask & int_nak_active_mask
	// USBIF	
	addr = SSUSB_XHCI_HSCH_CFG2;
	
	//addr = &xhci->cap_regs->hc_capbase + 0x900 + 0x7c;
	temp = xhci_readl(xhci, addr);

	printk("[OTG_H] read SSUSB_XHCI_HSCH_CFG2 address is 0x%p , value is 0x%x\n", addr, temp) ;
	//pdn

	//directly fill port 0 reg, which U2 port should be considered in driver
	temp &= ~(SSUSB_XHCI_ACTIVE_MASK);
	temp = temp | (int_nak_active << 8) | bulk_nyet_active;
	xhci_writel(xhci, temp, addr);
	//set PORTPMSC
	addr = &xhci->op_regs->port_power_base + NUM_PORT_REGS*((g_port_id-1) & 0xff);

	printk("[OTG_H] read PORTPMSC address is 0x%p", addr) ;
	
	temp = xhci_readl(xhci, addr);
	temp = temp & ~(MSK_RWE|MSK_BESL|MSK_L1_DEV_SLOT|MSK_HLE);
	temp |= (rwe<<3) & MSK_RWE;
	temp |= (besl<<4) & MSK_BESL;
	temp |= (slot_id<<8) & MSK_L1_DEV_SLOT;
	temp |= (hle<<16) & MSK_HLE;
	xhci_writel(xhci, temp, addr);
	xhci_dbg(xhci, "addr: %p\n", addr);
	xhci_dbg(xhci, "data: %d\n", temp);

	//set PORTHLPMC
	/* reference ssub_xHCI_u2_port_csr.xlsm for the configure fields */
	addr = &xhci->op_regs->port_lpm_ctrl_base + NUM_PORT_REGS*(g_port_id-1 & 0xff);
	temp = xhci_readl(xhci, addr);

	printk("[OTG_H] read port_lpm_ctrl_base address is 0x%p , value is 0x%x\n", addr, temp) ;
	//HIRDM = 1, L1 TIMEOUT = 0 (128us), BESLD = 0x7;
	temp = hirdm | (L1_timeout << 2) | (besld << 10);
	xhci_writel(xhci, temp, addr);
	xhci_dbg(xhci, "addr: %p\n", addr);
	xhci_dbg(xhci, "data: %d\n", temp);

	//if hle and rwe is set, we want device to remote wakeup
	//write LPM_L1_EXIT_TIMER to max value
	port_index = get_port_index(g_port_id);
	port = rh_port[port_index];
	// USBIF
	addr = SSUSB_XHCI_U2PORT_CFG;
	//addr = &xhci->cap_regs->hc_capbase + 0x900 + 0x78;
	if(hle == 1 && rwe == 1){
		temp = 0xff;
		xhci_writel(xhci, temp, addr);
	}
	else{
		if(port->port_speed == USB_SPEED_HIGH)
			temp = 0x4;
		else if(port->port_speed == USB_SPEED_FULL)
			temp = 0x8;
		xhci_writel(xhci, temp, addr);
	}
	num_u3_port = SSUSB_U3_PORT_NUM(readl(SSUSB_IP_CAP));
	if(hle == 1){
		mtktest_disablePortClockPower(g_port_id-1-num_u3_port, 0x02);
	}
	else{
		mtktest_enablePortClockPower(g_port_id-1-num_u3_port, 0x02);
	}

	return RET_SUCCESS;
}


int f_ring_enlarge(int ep_dir, int ep_num, int dev_num){
	struct xhci_hcd *xhci;
	struct usb_host_endpoint *ep;
	struct xhci_virt_device *virt_dev;
	struct usb_device *udev, *rhdev;
	struct xhci_ring *ep_ring;
	struct xhci_segment	*next, *prev;
	u32 val, cycle_bit;
	int i, ret;
	int slot_id, ep_index;

	ret = 0;

	xhci = hcd_to_xhci(my_hcd);
	if(dev_num == -1){
		rhdev = my_hcd->self.root_hub;
		udev = rhdev->children[g_port_id-1];
		slot_id = udev->slot_id;
	}
	else{
		udev = dev_list[dev_num-1];
		slot_id = udev->slot_id;
	}
	virt_dev = xhci->devs[udev->slot_id];
	if(ep_dir == EPADD_OUT){
		ep = udev->ep_out[ep_num];
	}
	else{
		ep = udev->ep_in[ep_num];
	}
	ep_index = mtktest_xhci_get_endpoint_index(&ep->desc);
	ep_ring = (&(virt_dev->eps[ep_index]))->ring;

	prev = ep_ring->enq_seg;
	next = mtktest_xhci_segment_alloc(xhci, GFP_NOIO);
	next->next = prev->next;
	next->trbs[TRBS_PER_SEGMENT-1].link.segment_ptr = prev->next->dma;
	val = next->trbs[TRBS_PER_SEGMENT-1].link.control;
	val &= ~TRB_TYPE_BITMASK;
	val |= TRB_TYPE(TRB_LINK);
	val |= TRB_CHAIN;
	next->trbs[TRBS_PER_SEGMENT-1].link.control = val;
	xhci_dbg(xhci, "Linking segment 0x%llx to segment 0x%llx (DMA)\n",
			(unsigned long long)prev->dma,
			(unsigned long long)next->dma);
	//adjust cycle bit
	if(ep_ring->cycle_state == 1){
		cycle_bit = 0;
	}
	else{
		cycle_bit = 1;
	}
	for(i=0; i<TRBS_PER_SEGMENT; i++){
		val = next->trbs[i].generic.field[3];
		if(cycle_bit == 1){
			val |= cycle_bit;
		}
		else{
			val &= ~cycle_bit;
		}
		next->trbs[i].generic.field[3] = val;
		xhci_dbg(xhci, "Set new segment trb %d cycle bit 0x%x\n", i, val);
	}
	mtktest_xhci_link_segments(xhci, prev, next, true);
	if(prev->trbs[TRBS_PER_SEGMENT-1].link.control & LINK_TOGGLE){
		val = prev->trbs[TRBS_PER_SEGMENT-1].link.control;
		val &= ~LINK_TOGGLE;
		prev->trbs[TRBS_PER_SEGMENT-1].link.control = val;
		val = next->trbs[TRBS_PER_SEGMENT-1].link.control;
		val |= LINK_TOGGLE;
		next->trbs[TRBS_PER_SEGMENT-1].link.control = val;
	}
	return ret;
}

int f_ring_stop_ep(int slot_id, int ep_index){
	struct xhci_hcd *xhci;
	unsigned long flags;

	xhci = hcd_to_xhci(my_hcd);
	g_cmd_status = CMD_RUNNING;
	spin_lock_irqsave(&xhci->lock, flags);
	mtktest_xhci_queue_stop_endpoint(xhci, slot_id, ep_index);
	spin_unlock_irqrestore(&xhci->lock, flags);
	
	mtktest_xhci_ring_cmd_db(xhci);
	wait_event_on_timeout(&g_cmd_status, CMD_DONE, CMD_TIMEOUT);
	if(g_cmd_status == CMD_RUNNING){
		xhci_err(xhci, "[ERROR] stop ep ring timout\n");
		return RET_FAIL;
	}
	if(g_cmd_status == CMD_FAIL){
		xhci_err(xhci, "[ERROR] stop ep ring failed\n");
		return RET_FAIL;
	}
	return RET_SUCCESS;
}

int f_ring_set_tr_dequeue_pointer(int slot_id, int ep_index, struct urb *urb){
	struct xhci_hcd *xhci;
	struct xhci_ring *ep_ring;
	struct xhci_virt_ep *ep;
	struct xhci_td *cur_td = NULL;
	struct xhci_dequeue_state deq_state;
	struct xhci_virt_device *dev;
	unsigned long flags;

	xhci = hcd_to_xhci(my_hcd);
	dev = xhci->devs[slot_id];
	memset(&deq_state, 0, sizeof(deq_state));
	ep_ring = mtktest_xhci_urb_to_transfer_ring(xhci, urb);
	ep = &xhci->devs[slot_id]->eps[ep_index];
	cur_td = ep->stopped_td;
	if(!cur_td){
		cur_td = list_entry(ep_ring->td_list.next, struct xhci_td, td_list);
		dev->eps[ep_index].stopped_trb = cur_td->first_trb;

	}
	mtktest_xhci_find_new_dequeue_state(xhci, slot_id, ep_index,
		0,
		cur_td, &deq_state);

	spin_lock_irqsave(&xhci->lock, flags);	
	mtktest_xhci_queue_new_dequeue_state(xhci,
				slot_id, ep_index,
				0,
				&deq_state);
	spin_unlock_irqrestore(&xhci->lock, flags);
	
	g_cmd_status = CMD_RUNNING;
	mtktest_xhci_ring_cmd_db(xhci);
	wait_event_on_timeout(&g_cmd_status, CMD_DONE, CMD_TIMEOUT);
	if(g_cmd_status == CMD_RUNNING){
		xhci_err(xhci, "[ERROR] set tr dequeue pointer timout\n");
		return RET_FAIL;
	}
	if(g_cmd_status == CMD_FAIL){
		xhci_err(xhci, "[ERROR] set tr dequeue pointer failed\n");
		return RET_FAIL;
	}
	return RET_SUCCESS;
}

int f_ring_stop_cmd(){
	struct xhci_hcd *xhci;
	u32 __iomem *addr;
	u64 val_64;
	int temp, tmp_add, i;

	if(my_hcd == NULL){
		printk(KERN_ERR "[ERROR]host controller driver not initiated\n");
		return RET_FAIL;
	}
	xhci = hcd_to_xhci(my_hcd);

	val_64 = xhci_read_64(xhci, &xhci->op_regs->cmd_ring);
	val_64 = val_64 | CMD_RING_PAUSE;
	xhci_dbg(xhci, "// Setting command ring register to 0x%llx\n", val_64);
	g_cmd_status = CMD_RUNNING;
	xhci_write_64(xhci, val_64, &xhci->op_regs->cmd_ring);
	wait_event_on_timeout(&g_cmd_status, CMD_DONE, CMD_TIMEOUT);
	val_64 = xhci_read_64(xhci, &xhci->op_regs->cmd_ring);
	if(g_cmd_status == CMD_RUNNING){
		xhci_err(xhci, "[ERROR] stop command ring timout\n");
		return RET_FAIL;
	}
	if(g_cmd_status == CMD_FAIL){
		xhci_err(xhci, "[ERROR] stop command ring failed\n");
		return RET_FAIL;
	}
	xhci_dbg(xhci, "command ring register 0x%llx\n", val_64);
	if(val_64 & CMD_RING_RUNNING != 0){
		xhci_err(xhci, "[ERROR] command ring doesn't stop\n");
		return RET_FAIL;
	}
	return RET_SUCCESS;
}

int f_ring_abort_cmd(){
	struct xhci_virt_device *virt_dev;
	struct xhci_hcd *xhci;
	u64 val_64;
	int temp;

	//get xhci struct
	xhci = hcd_to_xhci(my_hcd);
	//initial wait queue
	val_64 = xhci_read_64(xhci, &xhci->op_regs->cmd_ring);
	val_64 = val_64 | CMD_RING_ABORT;
	g_cmd_status = CMD_RUNNING;
	//mtktest_xhci_ring_cmd_db(xhci);
	temp = xhci_readl(xhci, &xhci->dba->doorbell[0]) & DB_MASK;
	xhci_writel(xhci, temp | DB_TARGET_HOST, &xhci->dba->doorbell[0]);
	udelay(5);
	xhci_write_64(xhci, val_64, &xhci->op_regs->cmd_ring);
	wait_event_on_timeout(&g_cmd_status, CMD_DONE, CMD_TIMEOUT);
	if(g_cmd_status == CMD_RUNNING){
		xhci_err(xhci, "[ERROR] abort command timeout\n");
		return RET_FAIL;
	}
	return RET_SUCCESS;
}

int f_hub_getPortStatus(int hdev_num, int port_num, u32 *status){
	int ret;
	struct usb_device *udev, *rhdev;
	struct xhci_virt_device *virt_dev;
	struct xhci_hcd *xhci;
	struct urb *urb;
	struct usb_ctrlrequest *dr;
	struct usb_config_descriptor *desc;
	int i;
	char *tmp;
	u32 tmp_status;

	ret = RET_SUCCESS;

	//get xhci struct
	xhci = hcd_to_xhci(my_hcd);
	//initial wait queue

	rhdev = my_hcd->self.root_hub;
	udev = hdev_list[hdev_num-1];
	virt_dev = xhci->devs[udev->slot_id];
	//get status

//	urb = usb_alloc_urb(0, GFP_NOIO);
	dr = kmalloc(sizeof(struct usb_ctrlrequest), GFP_NOIO);
	dr->bRequestType = USB_DIR_IN | USB_RT_PORT;
	dr->bRequest = USB_REQ_GET_STATUS;
	dr->wValue = cpu_to_le16(0);
	dr->wIndex = cpu_to_le16(port_num);
	dr->wLength = cpu_to_le16(USB_HUB_PORT_STATUS_SIZE);
	desc = kmalloc(USB_HUB_PORT_STATUS_SIZE, GFP_KERNEL);
	memset(desc, 0, USB_HUB_PORT_STATUS_SIZE);
	urb = alloc_ctrl_urb(dr, desc, udev);
	ret = f_ctrlrequest(urb, udev);

	if(urb->status == 0){
		xhci_dbg(xhci, "get status success\n buffer:\n");
		for(i=0; i<urb->transfer_buffer_length; i++){
			tmp = urb->transfer_buffer+i;
			xhci_dbg(xhci, "0x%x ", *tmp);
			tmp_status = (u32)*tmp;
			*status |= (tmp_status << (i*8));
		}

	}
	else{
		xhci_err(xhci, "[ERROR] get status failed\n");
		ret = RET_FAIL;
	}
	kfree(dr);
	kfree(desc);
	usb_free_urb(urb);
	return ret;
}

int f_hub_sethubfeature(int hdev_num, int wValue){
	int ret;
	struct usb_device *udev, *rhdev;
	struct xhci_virt_device *virt_dev;
	struct xhci_hcd *xhci;
	struct urb *urb;
	struct usb_ctrlrequest *dr;
	struct usb_config_descriptor *desc;
	int i;
	char *tmp;
	ret = RET_SUCCESS;

	//get xhci struct
	xhci = hcd_to_xhci(my_hcd);
	//initial wait queue

	rhdev = my_hcd->self.root_hub;
	udev = hdev_list[hdev_num-1];
	virt_dev = xhci->devs[udev->slot_id];
	//set configuration
//	urb = usb_alloc_urb(0, GFP_NOIO);
	dr = kmalloc(sizeof(struct usb_ctrlrequest), GFP_NOIO);
	dr->bRequestType = USB_RT_HUB;
	dr->bRequest = USB_REQ_SET_FEATURE;
	dr->wValue = cpu_to_le16(wValue);
	dr->wIndex = cpu_to_le16(0);
	dr->wLength = cpu_to_le16(0);
	urb = alloc_ctrl_urb(dr, NULL, udev);
	f_ctrlrequest(urb, udev);

	if(urb->status == 0){
		xhci_dbg(xhci, "set feature success\n");
		kfree(dr);
		usb_free_urb(urb);
	}
	else{
		xhci_err(xhci, "[ERROR] set feature failed\n");
		ret = urb->status;
		kfree(dr);
		usb_free_urb(urb);
		return RET_FAIL;
	}

	return ret;
}

int f_hub_setportfeature(int hdev_num, int wValue, int wIndex){
	int ret;
	struct usb_device *udev, *rhdev;
	struct xhci_virt_device *virt_dev;
	struct xhci_hcd *xhci;
	struct urb *urb;
	struct usb_ctrlrequest *dr;
	struct usb_config_descriptor *desc;
	int i;
	char *tmp;
	ret = RET_SUCCESS;

	//get xhci struct
	xhci = hcd_to_xhci(my_hcd);
	//initial wait queue

	rhdev = my_hcd->self.root_hub;
	udev = hdev_list[hdev_num-1];
	virt_dev = xhci->devs[udev->slot_id];
	//set configuration
//	urb = usb_alloc_urb(0, GFP_NOIO);
	dr = kmalloc(sizeof(struct usb_ctrlrequest), GFP_NOIO);
	dr->bRequestType = USB_RT_PORT;
	dr->bRequest = USB_REQ_SET_FEATURE;
	dr->wValue = cpu_to_le16(wValue);
	dr->wIndex = cpu_to_le16(wIndex);
	dr->wLength = cpu_to_le16(0);
	urb = alloc_ctrl_urb(dr, NULL, udev);
	f_ctrlrequest(urb, udev);

	if(urb->status == 0){
		xhci_dbg(xhci, "set feature success\n");
		kfree(dr);
		usb_free_urb(urb);
	}
	else{
		xhci_err(xhci, "[ERROR] set feature failed\n");
		ret = urb->status;
		kfree(dr);
		usb_free_urb(urb);
		return RET_FAIL;
	}

	return ret;
}

int f_hub_clearportfeature(int hdev_num, int wValue, int wIndex){
	int ret;
	struct usb_device *udev, *rhdev;
	struct xhci_hcd *xhci;
	struct urb *urb;
	struct usb_ctrlrequest *dr;
	struct usb_config_descriptor *desc;
	int i;
	char *tmp;
	ret = RET_SUCCESS;

	//get xhci struct
	xhci = hcd_to_xhci(my_hcd);
	//initial wait queue

	rhdev = my_hcd->self.root_hub;
	udev = hdev_list[hdev_num-1];
	//set
//	urb = usb_alloc_urb(0, GFP_NOIO);
	dr = kmalloc(sizeof(struct usb_ctrlrequest), GFP_NOIO);
	dr->bRequestType = USB_RT_PORT;
	dr->bRequest = USB_REQ_CLEAR_FEATURE;
	dr->wValue = cpu_to_le16(wValue);
	dr->wIndex = cpu_to_le16(wIndex);
	dr->wLength = cpu_to_le16(0);

	urb = alloc_ctrl_urb(dr, NULL, udev);

	f_ctrlrequest(urb, udev);
	if(urb->status == 0){
		xhci_dbg(xhci, "clear feature success\n");

	}
	else{
		xhci_err(xhci, "[ERROR] clear feature failed\n");
		ret = RET_FAIL;
	}
	kfree(dr);
	usb_free_urb(urb);
	return ret;
}

int f_hub_configep(int hdev_num, int rh_port_index){
	struct xhci_hcd *xhci;
	struct usb_device *udev, *rhdev;
	struct xhci_virt_device *virt_dev;
	struct xhci_command *config_cmd;
	struct xhci_input_control_ctx *ctrl_ctx;
	struct xhci_slot_ctx *slot_ctx;
	struct xhci_container_ctx *in_ctx, *out_ctx;
	struct urb *urb;
	struct usb_ctrlrequest *dr;
	struct usb_host_endpoint *ep;
	void *buffer;
	int slot_state;
	int ret, i;
	char *tmp;
	struct xhci_port *port;
	xhci = hcd_to_xhci(my_hcd);
	udev = hdev_list[hdev_num-1];
	virt_dev = xhci->devs[udev->slot_id];
	port = rh_port[rh_port_index];

	//get device descriptor
//	urb = usb_alloc_urb(0, GFP_NOIO);
	dr = kmalloc(sizeof(struct usb_ctrlrequest), GFP_NOIO);
	dr->bRequestType = USB_DIR_IN;
	dr->bRequest = USB_REQ_GET_DESCRIPTOR;
	dr->wValue = cpu_to_le16((USB_DT_DEVICE << 8) + 0);
	dr->wIndex = cpu_to_le16(0);
	dr->wLength = cpu_to_le16(USB_DT_DEVICE_SIZE);
	buffer = kmalloc(USB_DT_DEVICE_SIZE, GFP_KERNEL);
	memset(buffer, 0, USB_DT_DEVICE_SIZE);
	urb = alloc_ctrl_urb(dr, buffer, udev);
	f_ctrlrequest(urb, udev);

	if(urb->status == 0){
		xhci_dbg(xhci, "get descriptor success\n buffer:\n");
		for(i=0; i<urb->transfer_buffer_length; i++){
			tmp = urb->transfer_buffer+i;
			xhci_dbg(xhci, "0x%x ", *tmp);
		}
		kfree(dr);
		kfree(buffer);
		usb_free_urb(urb);
	}
	else{
		xhci_err(xhci, "[ERROR] get descriptor failed\n");
		ret = urb->status;
		kfree(dr);
		kfree(buffer);
		usb_free_urb(urb);
		return RET_FAIL;
	}
	//get config descriptor 255 bytes
//	urb = usb_alloc_urb(0, GFP_NOIO);
	dr = kmalloc(sizeof(struct usb_ctrlrequest), GFP_NOIO);
	dr->bRequestType = USB_DIR_IN;
	dr->bRequest = USB_REQ_GET_DESCRIPTOR;
	dr->wValue = cpu_to_le16((USB_DT_CONFIG << 8) + 0);
	dr->wIndex = cpu_to_le16(0);
	dr->wLength = cpu_to_le16(255);
	buffer = kmalloc(255, GFP_KERNEL);
	memset(buffer, 0, 255);

	urb = alloc_ctrl_urb(dr, buffer, udev);
	f_ctrlrequest(urb, udev);
	if(urb->status == 0){
		xhci_dbg(xhci, "get config descriptor success\n buffer:\n");
		for(i=0; i<urb->transfer_buffer_length; i++){
			tmp = urb->transfer_buffer+i;
			xhci_dbg(xhci, "0x%x ", *tmp);
		}
		kfree(dr);
		kfree(buffer);
		usb_free_urb(urb);
	}
	else{
		xhci_err(xhci, "[ERROR] get config descriptor failed\n");
		ret = urb->status;
		kfree(dr);
		kfree(buffer);
		usb_free_urb(urb);
		return RET_FAIL;
	}
#if 0
	//get hub descriptor
//	urb = usb_alloc_urb(0, GFP_NOIO);
	dr = kmalloc(sizeof(struct usb_ctrlrequest), GFP_NOIO);
	dr->bRequestType = USB_DIR_IN | USB_RT_HUB;
	dr->bRequest = USB_REQ_GET_DESCRIPTOR;
	dr->wValue = cpu_to_le16(0);
	dr->wIndex = cpu_to_le16(0);
	dr->wLength = cpu_to_le16(71);
	buffer = kmalloc(71, GFP_KERNEL);
	memset(buffer, 0, 71);
	urb = alloc_ctrl_urb(dr, buffer, udev);
	f_ctrlrequest(urb, udev);

	if(urb->status == 0){
		xhci_dbg(xhci, "get hub descriptor success\n buffer:\n");
		for(i=0; i<urb->transfer_buffer_length; i++){
			tmp = urb->transfer_buffer+i;
			xhci_dbg(xhci, "0x%x ", *tmp);
		}
		kfree(dr);
		kfree(buffer);
		usb_free_urb(urb);
	}
	else{
		xhci_err(xhci, "[ERROR] get hub descriptor failed\n");
		ret = urb->status;
		kfree(dr);
		kfree(buffer);
		usb_free_urb(urb);
		return RET_FAIL;
	}
#endif
	//set configuration
//	urb = usb_alloc_urb(0, GFP_NOIO);
	dr = kmalloc(sizeof(struct usb_ctrlrequest), GFP_NOIO);
	dr->bRequestType = 0;
	dr->bRequest = USB_REQ_SET_CONFIGURATION;
	dr->wValue = cpu_to_le16(1);
	dr->wIndex = cpu_to_le16(0);
	dr->wLength = cpu_to_le16(0);

	urb = alloc_ctrl_urb(dr, buffer, udev);
	f_ctrlrequest(urb, udev);
	if(urb->status == 0){
		xhci_dbg(xhci, "set configuration success\n");
		kfree(dr);
		usb_free_urb(urb);
	}
	else{
		xhci_err(xhci, "[ERROR] set configuration failed\n");
		ret = urb->status;
		kfree(dr);
		usb_free_urb(urb);
		return RET_FAIL;
	}
	//superspeed set hub depth
	//config ep
	if(port->port_speed == USB_SPEED_SUPER){
		dr = kmalloc(sizeof(struct usb_ctrlrequest), GFP_NOIO);
		dr->bRequestType = 0x20;
		dr->bRequest = 0x0c;
		dr->wValue = cpu_to_le16(0);
		dr->wIndex = cpu_to_le16(0);
		dr->wLength = cpu_to_le16(0);

		urb = alloc_ctrl_urb(dr, buffer, udev);
		f_ctrlrequest(urb, udev);
		if(urb->status == 0){
			xhci_dbg(xhci, "set hub depth success\n");
			kfree(dr);
			usb_free_urb(urb);
		}
		else{
			xhci_err(xhci, "[ERROR] set hub depth failed\n");
			ret = urb->status;
			kfree(dr);
			usb_free_urb(urb);
			return RET_FAIL;
		}
		//config endpoint
		//prepare ep description
		ep = kmalloc(sizeof(struct usb_host_endpoint), GFP_NOIO);
		ep->desc.bDescriptorType = USB_DT_ENDPOINT;
		ep->desc.bEndpointAddress = EPADD_NUM(1) | EPADD_IN;
		ep->desc.bmAttributes = EPATT_INT;
		ep->desc.wMaxPacketSize = 2;
		ep->desc.bInterval = 16;
		ep->ss_ep_comp.bMaxBurst = 0;
		ep->ss_ep_comp.bmAttributes = 0;
		ep->ss_ep_comp.wBytesPerInterval = 2;
		//SW add endpoint in context
		ret = f_udev_add_ep(ep,  udev);
	//	ret = mtktest_xhci_mtk_add_endpoint(my_hcd, udev, ep);
		if(ret){
			xhci_err(xhci, "[ERROR] add endpoint failed\n");
			return RET_FAIL;
		}
		ret = f_xhci_config_ep(udev);
		if(ret){
			xhci_err(xhci, "[ERROR] config endpoint failed\n");
			return RET_FAIL;
		}

	}
	else{
		//config endpoint
		//prepare ep description
		ep = kmalloc(sizeof(struct usb_host_endpoint), GFP_NOIO);
		ep->desc.bDescriptorType = USB_DT_ENDPOINT;
		ep->desc.bEndpointAddress = EPADD_NUM(1) | EPADD_IN;
		ep->desc.bmAttributes = EPATT_INT;
		ep->desc.wMaxPacketSize = 1;
		ep->desc.bInterval = 2046;
		//SW add endpoint in context
		ret = f_udev_add_ep(ep,  udev);
	//	ret = mtktest_xhci_mtk_add_endpoint(my_hcd, udev, ep);
		if(ret){
			xhci_err(xhci, "[ERROR] add endpoint failed\n");
			return RET_FAIL;
		}
		ret = f_xhci_config_ep(udev);
		if(ret){
			xhci_err(xhci, "[ERROR] config endpoint failed\n");
			return RET_FAIL;
		}
	}
	return RET_SUCCESS;

}

int f_hub_config_subhub(int parent_hub_num, int hub_num, int port_num){
	u32 status;
	struct usb_device *udev, *hdev;
	struct xhci_virt_device *virt_dev;
	struct xhci_hcd *xhci;
	struct xhci_input_control_ctx *ctrl_ctx;
	int ret, i, speed;
	int count = 0;
	int count_down = 1000;

	xhci = hcd_to_xhci(my_hcd);

	hdev = hdev_list[parent_hub_num-1];

	count = count_down;
	while(!(status & USB_PORT_STAT_CONNECTION) && count > 0){
		ret = f_hub_getPortStatus(parent_hub_num, port_num, &status);
		if(ret != RET_SUCCESS){
			xhci_err(xhci, "[ERROR] Get status failed\n");
			return RET_FAIL;
		}
		count--;
	}
	if(count == 0){
		xhci_err(xhci, "[ERRROR] Wait port connection status timeout\n");
		return RET_FAIL;
	}
	xhci_dbg(xhci, "Got port connection status\n");

	if(f_hub_setportfeature(parent_hub_num, HUB_FEATURE_PORT_RESET, port_num) != RET_SUCCESS){
		xhci_err(xhci, "[ERROR] Set port reset failed\n");
		return RET_FAIL;
	}
	status = 0;
	count = count_down;
	while(!((status>>16) & USB_PORT_STAT_C_RESET) && count > 0){
		ret = ret = f_hub_getPortStatus(parent_hub_num, port_num, &status);
		if(ret != RET_SUCCESS){
			xhci_err(xhci, "[ERROR] Get status failed\n");
			return RET_FAIL;
		}
		count--;
	}
	if(count == 0){
		xhci_err(xhci, "[ERROR] Wait port reset change status timeout\n");
		return RET_FAIL;
	}
	xhci_dbg(xhci, "Got port reset change status\n");

	//FIXME: check superspeed
	if(status & USB_PORT_STAT_HIGH_SPEED){
//		g_speed = USB_SPEED_HIGH;
		speed = USB_SPEED_HIGH;
	}
	else if(status & USB_PORT_STAT_LOW_SPEED){
//		g_speed = USB_SPEED_LOW;
		speed = USB_SPEED_LOW;
	}
	else{
//		g_speed = USB_SPEED_FULL;
		speed = USB_SPEED_FULL;
	}

	if(f_hub_clearportfeature(parent_hub_num, HUB_FEATURE_C_PORT_RESET, port_num) != RET_SUCCESS){
		xhci_err(xhci, "[ERROR] Set port reset failed\n");
		return RET_FAIL;
	}
	//new usb device
	udev = hdev->children[port_num-1];
	udev = mtk_usb_alloc_dev(hdev, hdev->bus, port_num);
	udev->level = hdev->level + 1;
	udev->speed = speed;
	hdev->children[port_num-1] = udev;
	hdev_list[hub_num-1] = udev;
	print_speed(udev->speed);
	if (hdev->tt) {
		udev->tt = hdev->tt;
		udev->ttport = hdev->ttport;
	}
	else if(udev->speed != USB_SPEED_HIGH
			&& hdev->speed == USB_SPEED_HIGH) {
		udev->tt = kzalloc(sizeof(struct usb_tt), GFP_KERNEL);
		udev->tt->hub = hdev;
		udev->tt->multi = false;
		udev->tt->think_time = 0;
		udev->ttport = port_num;
	}
	//enable slot
	ret = f_enable_slot(udev);

	if(ret != RET_SUCCESS){
		return RET_FAIL;
	}
	//address device
	ret = f_address_slot(false, NULL);
	if(ret != RET_SUCCESS){
		return RET_FAIL;
	}
	if(f_hub_configep(hub_num, 0) != RET_SUCCESS){
		xhci_err(xhci, "config hub endpoint failed\n");
		return RET_FAIL;
	}
	//set port_power
	for(i=1; i<=4; i++){
		if(f_hub_setportfeature(hub_num, HUB_FEATURE_PORT_POWER, i) != RET_SUCCESS){
			xhci_err(xhci, "[ERROR] set port_power 1 failed\n");
			return RET_FAIL;
		}
	}
	//clear C_PORT_CONNECTION
	for(i=1; i<=4; i++){
		if(f_hub_clearportfeature(hub_num, HUB_FEATURE_C_PORT_CONNECTION, i) != RET_SUCCESS){
			xhci_err(xhci, "[ERROR] clear c_port_connection failed\n");
		}
	}

	return RET_SUCCESS;
}

int f_hub_init_device(int hub_num, int port_num, int dev_num){
	u32 status;
	int ret;
	struct usb_device *udev, *hdev;
	struct xhci_virt_device *virt_dev;
	struct xhci_hcd *xhci;
	struct xhci_input_control_ctx *ctrl_ctx;
	int slot_state;
	int count = 0;
	int count_down = 1000;
	int speed;

	xhci = hcd_to_xhci(my_hcd);
	hdev = hdev_list[hub_num-1];
	status = 0;
	count = count_down;
	while(!(status & USB_PORT_STAT_CONNECTION) && count > 0){
		ret = f_hub_getPortStatus(hub_num, port_num, &status);
		if(ret != RET_SUCCESS){
			xhci_err(xhci, "[ERROR] Get status failed\n");
			return RET_FAIL;
		}
		count--;
	}
	if(count == 0){
		xhci_err(xhci, "[ERRROR] Wait port connection status timeout\n");
		return RET_FAIL;
	}
	xhci_dbg(xhci, "Got port connection status\n");
	if(f_hub_setportfeature(hub_num, HUB_FEATURE_PORT_RESET, port_num) != RET_SUCCESS){
		xhci_err(xhci, "[ERROR] Set port reset failed\n");
		return RET_FAIL;
	}

	status = 0;
	count = count_down;
	while(!((status>>16) & USB_PORT_STAT_C_RESET) && count > 0){
		ret = f_hub_getPortStatus(hub_num, port_num, &status);
		if(ret != RET_SUCCESS){
			xhci_err(xhci, "[ERROR] Get status failed\n");
			return RET_FAIL;
		}
		count--;
	}
	if(count == 0){
		xhci_err(xhci, "[ERROR] Wait port reset change status timeout\n");
		return RET_FAIL;
	}
	xhci_dbg(xhci, "Port reset done\n");

	//FIXME: check superspeed
	if(status & USB_PORT_STAT_HIGH_SPEED){
		speed = USB_SPEED_HIGH;
	}
	else if(status & USB_PORT_STAT_LOW_SPEED){
		speed = USB_SPEED_LOW;
	}
	else{
		speed = USB_SPEED_FULL;
	}

	if(f_hub_clearportfeature(hub_num, HUB_FEATURE_C_PORT_RESET, port_num) != RET_SUCCESS){
		xhci_err(xhci, "[ERROR] Set port reset failed\n");
		return RET_FAIL;
	}
	//new usb device
	udev = hdev->children[port_num-1];
	udev = mtk_usb_alloc_dev(hdev, hdev->bus, port_num);
	udev->level = hdev->level + 1;
	udev->speed = speed;
	hdev->children[port_num-1] = udev;
	//need to add tt handler
	if (hdev->tt) {
		udev->tt = hdev->tt;
		udev->ttport = hdev->ttport;
	}
	else if(udev->speed != USB_SPEED_HIGH
			&& hdev->speed == USB_SPEED_HIGH) {
		udev->tt = kzalloc(sizeof(struct usb_tt), GFP_KERNEL);
		udev->tt->hub = hdev;
		udev->tt->multi = false;
		udev->tt->think_time = 0;
		udev->ttport = port_num;

	}
	dev_list[dev_num-1] = udev;
	print_speed(udev->speed);
	//enable slot
	ret = f_enable_slot(udev);
	if(ret != RET_SUCCESS){
		return RET_FAIL;
	}

	//address device
	ret = f_address_slot(false, udev);
	if(ret != RET_SUCCESS){
		return RET_FAIL;
	}

	return RET_SUCCESS;
}

int f_hub_reset_dev(struct usb_device *udev,int dev_num, int port_num, int speed){
	struct xhci_hcd *xhci;
	int ret;
	int count;
	int count_down = 1000;

	xhci = hcd_to_xhci(my_hcd);
	ret = RET_SUCCESS;
	ret = dev_reset(speed,udev);
	if(f_hub_clearportfeature(1, HUB_FEATURE_PORT_POWER, port_num) != RET_SUCCESS){
		xhci_err(xhci, "[ERROR] clear port_power %d failed\n", port_num);
		return RET_FAIL;
	}
	mdelay(500);
	if(f_hub_setportfeature(1, HUB_FEATURE_PORT_POWER, port_num) != RET_SUCCESS){
		xhci_err(xhci, "[ERROR] set port_power %d failed\n", port_num);
		return RET_FAIL;
	}
	if(f_hub_clearportfeature(1, HUB_FEATURE_C_PORT_CONNECTION, port_num) != RET_SUCCESS){
		xhci_err(xhci, "[ERROR] clear c_port_connection failed\n");
	}
	g_slot_id = udev->slot_id;
	f_disable_slot();
	kfree(udev);
	dev_list[dev_num-1] = NULL;
	f_hub_init_device(1, port_num, dev_num);
	return ret;
}


#define DATA_LENGTH	2000

#define TOTAL_RX_URB	30
#define TOTAL_TX_URB	30
#define URB_STATUS_IDLE	150
#define URB_STATUS_TX	151

struct ixia_data {
	int dev_num;
    struct xhci_hcd *xhci;
    struct usb_device *udev;
	int ep_out;
	int ep_in;
	struct urb *urb_rx_list[TOTAL_RX_URB];
	struct urb *urb_tx_list[TOTAL_RX_URB];
};

static int ixia_rx_thread(void *data){
	int i, ret;
	struct urb *urb_rx;
	struct ixia_data *ix_data = data;
	struct ixia_dev *ix_dev = ix_dev_list[ix_data->dev_num - 1];
	struct usb_device *udev;
	struct xhci_virt_device *virt_dev;
	struct xhci_hcd *xhci;
	struct usb_host_endpoint *ep_tx, *ep_rx;
	int ep_index_tx, ep_index_rx;
	int max_esit_payload;
	int rx_index;
	char is_running;
	int rx_status;

	is_running = true;
	ret = 0;
	xhci = ix_data->xhci;
	udev = ix_data->udev;
	ep_tx = udev->ep_out[ix_data->ep_out];
	ep_rx = udev->ep_in[ix_data->ep_in];
	ep_index_tx = mtktest_xhci_get_endpoint_index(&ep_tx->desc);
	ep_index_rx = mtktest_xhci_get_endpoint_index(&ep_rx->desc);
	max_esit_payload = mtk_xhci_get_max_esit_payload(xhci, udev, ep_tx);
	rx_index = 0;
	xhci_err(xhci, "[IXIA] start rx threading\n");
	do{
		urb_rx = ix_data->urb_rx_list[rx_index];
		rx_status = urb_rx->status;
		while(rx_status != URB_STATUS_IDLE){
			if(rx_status != 0 && rx_status != -EINPROGRESS
				&& rx_status != URB_STATUS_IDLE && rx_status != URB_STATUS_TX){
				xhci_err(xhci, "[ERROR] urb_rx %d not in valid status - %d\n", i, urb_rx->status);
				is_running = false;
				break;
			}
			msleep(1);
			rx_status = urb_rx->status;
		}
		xhci_dbg(xhci, "[IXIA] queue rx urb %d\n", rx_index);
		//queue free rx urb
		//memset(urb_rx->transfer_buffer, 0, 1514);
		urb_rx->actual_length = 0;
		urb_rx->status = -EINPROGRESS;
		f_queue_urb(urb_rx,0,udev);
		//*****************
		rx_index++;
		if(rx_index == TOTAL_RX_URB){
			rx_index = 0;
		}
	}while(is_running);
	xhci_err(xhci, "[ERROR] exit rx urb handler thread\n");
}

static int ixia_rx_done_thread(void *data){
	int i, ret;
	struct urb *urb_tx, *urb_rx;
	struct ixia_data *ix_data = data;
	struct ixia_dev *ix_dev = ix_dev_list[ix_data->dev_num - 1];
	struct usb_device *udev;
	struct xhci_virt_device *virt_dev;
	struct xhci_hcd *xhci;
	struct usb_host_endpoint *ep_tx, *ep_rx;
	int ep_index_tx, ep_index_rx;
	int max_esit_payload;
	int rx_index;
	char is_running;
	int rx_status;

	is_running = true;
	ret = 0;
	xhci = ix_data->xhci;
	udev = ix_data->udev;
	ep_tx = udev->ep_out[ix_data->ep_out];
	ep_rx = udev->ep_in[ix_data->ep_in];
	ep_index_tx = mtktest_xhci_get_endpoint_index(&ep_tx->desc);
	ep_index_rx = mtktest_xhci_get_endpoint_index(&ep_rx->desc);
	max_esit_payload = mtk_xhci_get_max_esit_payload(xhci, udev, ep_tx);
	rx_index = 0;
	xhci_err(xhci, "[IXIA] start rx done threading\n");
	do{
		urb_rx = ix_data->urb_rx_list[rx_index];
		rx_status = urb_rx->status;
		while(rx_status != 0){
			if(rx_status != 0 && rx_status != -EINPROGRESS
				&& rx_status != URB_STATUS_IDLE && rx_status != URB_STATUS_TX){
				xhci_err(xhci, "[ERROR] urb_rx %d not in valid status - %d\n", i, urb_rx->status);
				is_running = false;
				break;
			}
			msleep(1);
			rx_status = urb_rx->status;
		}
		xhci_dbg(xhci, "[IXIA] rx urb %d success, queue tx urb\n", rx_index);
		urb_rx->status = URB_STATUS_TX;
		//queue tx urb
		urb_tx = ix_data->urb_tx_list[rx_index];
		urb_tx->status = -EINPROGRESS;
		urb_tx->transfer_buffer = urb_rx->transfer_buffer;
		urb_tx->transfer_buffer_length = urb_rx->actual_length;
		urb_tx->transfer_dma = urb_rx->transfer_dma;
		xhci_dbg(xhci, "[IXIA] tx urb 0x%p\n", urb_tx);
		f_queue_urb(urb_tx,0,udev);
		//*****************
		rx_index++;
		if(rx_index == TOTAL_RX_URB){
			rx_index = 0;
		}
	}while(is_running);
	xhci_err(xhci, "[ERROR] exit rx done urb handler thread\n");
}

static int ixia_tx_done_thread(void *data){
	int i, ret;
	struct urb *urb_tx, *urb_rx;
	struct ixia_data *ix_data = data;
	struct ixia_dev *ix_dev = ix_dev_list[ix_data->dev_num - 1];
	struct usb_device *udev;
	struct xhci_virt_device *virt_dev;
	struct xhci_hcd *xhci;
	struct usb_host_endpoint *ep_tx, *ep_rx;
	int ep_index_tx, ep_index_rx;
	int max_esit_payload;
	int rx_index;
	char is_running;
	int tx_status;

	is_running = true;
	ret = 0;
	xhci = ix_data->xhci;
	udev = ix_data->udev;
	ep_tx = udev->ep_out[ix_data->ep_out];
	ep_rx = udev->ep_in[ix_data->ep_in];
	ep_index_tx = mtktest_xhci_get_endpoint_index(&ep_tx->desc);
	ep_index_rx = mtktest_xhci_get_endpoint_index(&ep_rx->desc);
	max_esit_payload = mtk_xhci_get_max_esit_payload(xhci, udev, ep_tx);
	rx_index = 0;
	xhci_err(xhci, "[IXIA] start tx done threading\n");
	do{

		urb_tx = ix_data->urb_tx_list[rx_index];
		tx_status = urb_tx->status;
		xhci_dbg(xhci, "[IXIA] check tx urb %d 0x%p\n", rx_index, urb_tx);
		while(tx_status != 0){
			//xhci_dbg(xhci, "[IXIA] tx_status %d\n", urb_tx->status);
			if(tx_status != 0 && tx_status != -EINPROGRESS){
				xhci_err(xhci, "[ERROR] urb_tx %d not in valid status - %d\n", i, urb_tx->status);
				is_running = false;
				break;
			}
			msleep(1);
			tx_status = urb_tx->status;
		}
		xhci_dbg(xhci, "[IXIA] tx urb %d done\n", rx_index);
		//change rx status
		urb_rx = ix_data->urb_rx_list[rx_index];
		urb_rx->status = URB_STATUS_IDLE;
		urb_tx->status = -EINPROGRESS;
		//*****************
		rx_index++;
		if(rx_index == TOTAL_RX_URB){
			rx_index = 0;
		}
	}while(is_running);
	xhci_err(xhci, "[ERROR] exit tx done urb handler thread\n");
}

int f_add_ixia_thread(struct xhci_hcd *xhci, int dev_num, struct ixia_dev *ix_dev){
	int i, ret;
	struct usb_device *udev;
	struct ixia_data *ix_data;
	struct urb *urb_tx, *urb_rx;
	struct usb_host_endpoint *ep_tx, *ep_rx;
	int max_esit_payload;

	ret =0;
	xhci_err(xhci, "[IXIA]Start process, devnum %d\n", dev_num);
	udev = dev_list[dev_num-1];
	ep_tx = udev->ep_out[ix_dev->ep_out];
	max_esit_payload = mtk_xhci_get_max_esit_payload(xhci, udev, ep_tx);

	ix_data = kzalloc(sizeof(struct ixia_data), GFP_KERNEL);
	ix_data->xhci = xhci;
	ix_data->dev_num = dev_num;
	ix_data->udev = udev;
	ix_data->ep_in = ix_dev->ep_in;
	ix_data->ep_out = ix_dev->ep_out;

	for(i=0; i<TOTAL_RX_URB; i++){
		urb_rx = usb_alloc_urb(0, GFP_NOIO);
		ret = f_fill_urb(urb_rx,ix_data->ep_in,1514,0,URB_DIR_IN, 0, max_esit_payload, udev);
		if(ret){
			xhci_err(xhci, "[ERROR]fill rx urb Error!!\n");
			return RET_FAIL;
		}
		urb_rx->status = URB_STATUS_IDLE;
		ix_data->urb_rx_list[i] = urb_rx;
		xhci_err(xhci, "[IXIA] URB_RX %d -- 0x%p\n", i, urb_rx);
		urb_rx = NULL;
	}
	for(i=0; i<TOTAL_RX_URB; i++){
		urb_tx = usb_alloc_urb(0, GFP_NOIO);
		ret = f_fill_urb(urb_tx,ix_data->ep_out,1514,0,URB_DIR_OUT, 0, max_esit_payload, udev);
		if(ret){
			xhci_err(xhci, "[ERROR]fill tx urb Error!!\n");
			return RET_FAIL;
		}
		kfree(urb_tx->transfer_buffer);
		ix_data->urb_tx_list[i] = urb_tx;
		xhci_err(xhci, "[IXIA] URB_TX %d -- 0x%p\n", i, urb_tx);
		urb_tx = NULL;
	}
	kthread_run(ixia_rx_thread, ix_data, "ixiarxt");
	kthread_run(ixia_rx_done_thread, ix_data, "ixiarxdt");
	kthread_run(ixia_tx_done_thread, ix_data, "ixiatxdt");
}

void SetETHEPConfig(int dev_num, char *buf, struct usb_device *udev){
	unsigned char bEndCount;
	int nTxEP;
	int nRxEP;
    struct MUSB_ConfigurationDescriptor *pConfDes;
    struct MUSB_InterfaceDescriptor *pInterDes;
    struct MUSB_EndpointDescriptor *pEndDes;
    struct MUSB_DeviceDescriptor *pDevDes;
    char *pBuf;
    int i, j, ret;
	int transfer_type, maxp_tx, maxp_rx;
	struct ixia_dev *ix_dev;

    pBuf = pConfDes = (struct MUSB_ConfigurationDescriptor *) buf;
	printk(KERN_ERR "pBuf 0x%p\n", pBuf);
    pBuf += 9;//sizeof(struct MUSB_ConfigurationDescriptor);
	printk(KERN_ERR "pBuf 0x%p\n", pBuf);
	printk(KERN_ERR "bNumInterfaces %d\n", pConfDes->bNumInterfaces);
    for (i=0; i<pConfDes->bNumInterfaces; i++) {
        pInterDes = (struct MUSB_InterfaceDescriptor *) pBuf;
        pBuf += sizeof(struct MUSB_InterfaceDescriptor);
		printk(KERN_ERR "pBuf 0x%p\n", pBuf);
        bEndCount = pInterDes->bNumEndpoints;
		printk(KERN_ERR "bNumEndpoints %d\n", pInterDes->bNumEndpoints);
        for (j=0; j<pInterDes->bNumEndpoints; j++) {
            pEndDes = (struct MUSB_EndpointDescriptor *) pBuf;
            unsigned int ep_num = pEndDes->bEndpointAddress & 0xf;
            USB_DIR dir = (MUSB_DIR_OUT == (pEndDes->bEndpointAddress & 0x80)) ? USB_TX : USB_RX;
            if(dir == USB_TX){
                nTxEP = ep_num;
				maxp_tx = pEndDes->wMaxPacketSize;
				printk(KERN_ERR "nTxEP %d\n", nTxEP);
				printk(KERN_ERR "maxp_tx %d\n", maxp_tx);
            }
            else if(dir == USB_RX){
                nRxEP = ep_num;
				maxp_rx = pEndDes->wMaxPacketSize;
				printk(KERN_ERR "nRxEP %d\n", nRxEP);
				printk(KERN_ERR "maxp_rx %d\n", maxp_rx);
            }
            transfer_type = pEndDes->bmAttributes & 0x3;
			printk(KERN_ERR "transfer_type %d\n", transfer_type);
            pBuf += 7;//sizeof(struct MUSB_EndpointDescriptor);
			printk(KERN_ERR "pBuf 0x%p\n", pBuf);
        }
    }
	ix_dev = kmalloc(sizeof(struct ixia_dev), GFP_NOIO);
	ix_dev->udev = udev;
	ix_dev->ep_out = nTxEP;
	ix_dev->ep_in = nRxEP;
	ix_dev_list[dev_num-1] = ix_dev;
	ret = f_config_ep(nTxEP,EPADD_OUT,transfer_type,maxp_tx,0,0,0,udev ,0);
	if(ret){
		xhci_err(xhci, "[FAIL] config out endpoint failed\n");
	}
	ret = f_config_ep(nRxEP,EPADD_IN,transfer_type,maxp_rx,0,0,0,udev ,1);
    if(ret){
		xhci_err(xhci, "[FAIL] config in endpoint failed\n");
	}
}

int f_hub_configure_eth_device(int hub_num, int port_num, int dev_num){
	int nEnumStep, count;
	struct usb_device *udev, *hdev;
	struct xhci_hcd *xhci;
	struct urb *urb;
	struct usb_ctrlrequest *dr;
	char *ptr;
	unsigned char eth_enum_write_index;
	int ret;

	struct ethenumeration_t ethenumeration_step[] = {
    {"MUSB_REQ_VENDER_1F_1", //nodata
        {(MUSB_DIR_OUT|MUSB_TYPE_VENDOR|MUSB_RECIP_DEVICE),
          (AX_CMD_WRITE_GPIOS),
          (0x13),
          (0),
          (0)
        }
    },

    {"MUSB_REQ_VENDER_1F_2", //nodata
        {(MUSB_DIR_OUT|MUSB_TYPE_VENDOR|MUSB_RECIP_DEVICE),
          (AX_CMD_WRITE_GPIOS),
          (0x01),
          (0),
          (0)
        }
    },

    {"MUSB_REQ_VENDER_1F_3",  //nodata
        {(MUSB_DIR_OUT|MUSB_TYPE_VENDOR|MUSB_RECIP_DEVICE),
          (AX_CMD_WRITE_GPIOS),
          (0x03),
          (0),
          (0)
        }
    },

    {"MUSB_REQ_VENDER_10_4", //nodata
        {(MUSB_DIR_OUT|MUSB_TYPE_VENDOR|MUSB_RECIP_DEVICE),
          (AX_CMD_WRITE_RX_CTL),
          (0x80),
          (0),
          (0)
        }
    },

    {"MUSB_REQ_VENDER_17_5", //data in
        {(MUSB_DIR_IN|MUSB_TYPE_VENDOR|MUSB_RECIP_DEVICE),
          (AX_CMD_READ_NODE_ID),
          (0),
          (0),
          (0x06)
        }
    },

    {"MUSB_REQ_VENDER_19_6", // data in
        {(MUSB_DIR_IN|MUSB_TYPE_VENDOR|MUSB_RECIP_DEVICE),
          (AX_CMD_READ_PHY_ID),
          (0),
          (0),
          (0x02)
        }
    },

    {"MUSB_REQ_VENDER_06_7", // no data
        {(MUSB_DIR_OUT|MUSB_TYPE_VENDOR|MUSB_RECIP_DEVICE),
          (AX_CMD_SET_SW_MII),
          (0),
          (0),
          (0)
        }
    },
#if 1
    {"MUSB_REQ_VENDER_08_8", //data out
        {(MUSB_DIR_OUT|MUSB_TYPE_VENDOR|MUSB_RECIP_DEVICE),
          (AX_CMD_WRITE_MII_REG),
          (0x03),
          (0),
          (0x02)
        }
    },
#endif
    {"MUSB_REQ_VENDER_0A_9",
        {(MUSB_DIR_OUT|MUSB_TYPE_VENDOR|MUSB_RECIP_DEVICE),
          (AX_CMD_SET_HW_MII),
          (0),
          (0),
          (0)
        }
    },

    {"MUSB_REQ_VENDER_06_10",
        {(MUSB_DIR_OUT|MUSB_TYPE_VENDOR|MUSB_RECIP_DEVICE),
          (AX_CMD_SET_SW_MII),
          (0),
          (0),
          (0)
        }
    },
#if 1
    {"MUSB_REQ_VENDER_08_11",
        {(MUSB_DIR_OUT|MUSB_TYPE_VENDOR|MUSB_RECIP_DEVICE),
          (AX_CMD_WRITE_MII_REG),
          (0x03),
          (0x04),
          (0x02)
        }
    },
#endif
    {"MUSB_REQ_VENDER_0A_12",
        {(MUSB_DIR_OUT|MUSB_TYPE_VENDOR|MUSB_RECIP_DEVICE),
          (AX_CMD_SET_HW_MII),
          (0),
          (0),
          (0)
        }
    },

    {"MUSB_REQ_VENDER_06_13",
        {(MUSB_DIR_OUT|MUSB_TYPE_VENDOR|MUSB_RECIP_DEVICE),
          (AX_CMD_SET_SW_MII),
          (0),
          (0),
          (0)
        }
    },

    {"MUSB_REQ_VENDER_07_14",
        {(MUSB_DIR_IN|MUSB_TYPE_VENDOR|MUSB_RECIP_DEVICE),
          (AX_CMD_READ_MII_REG),
          (0x03),
          (0),
          (0x02)
        }
    },

    {"MUSB_REQ_VENDER_0A_15",
        {(MUSB_DIR_OUT|MUSB_TYPE_VENDOR|MUSB_RECIP_DEVICE),
          (AX_CMD_SET_HW_MII),
          (0),
          (0),
          (0)
        }
    },

    {"MUSB_REQ_VENDER_06_16",
        {(MUSB_DIR_OUT|MUSB_TYPE_VENDOR|MUSB_RECIP_DEVICE),
          (AX_CMD_SET_SW_MII),
          (0),
          (0),
          (0)
        }
    },
#if 1
    {"MUSB_REQ_VENDER_08_17",
        {(MUSB_DIR_OUT|MUSB_TYPE_VENDOR|MUSB_RECIP_DEVICE),
          (AX_CMD_WRITE_MII_REG),
          (0x03),
          (0),
          (0x02)
        }
    },
#endif
    {"MUSB_REQ_VENDER_0A_18",
        {(MUSB_DIR_OUT|MUSB_TYPE_VENDOR|MUSB_RECIP_DEVICE),
          (AX_CMD_SET_HW_MII),
          (0),
          (0),
          (0)
        }
    },

// ifconfig
    {"MUSB_REQ_VENDER_10_19",
        {(MUSB_DIR_OUT|MUSB_TYPE_VENDOR|MUSB_RECIP_DEVICE),
          (AX_CMD_WRITE_RX_CTL),
          (0x8c),
          (0),
          (0)
        }
    },
#if 1
    {"MUSB_REQ_VENDER_16_20",
        {(MUSB_DIR_OUT|MUSB_TYPE_VENDOR|MUSB_RECIP_DEVICE),
          (AX_CMD_WRITE_MULTI_FILTER),
          (0),
          (0),
          (0x08)
        }
    },
#endif
    {"MUSB_REQ_VENDER_10_21",
        {(MUSB_DIR_OUT|MUSB_TYPE_VENDOR|MUSB_RECIP_DEVICE),
          (AX_CMD_WRITE_RX_CTL),
          (0x9c),
          (0),
          (0)
        }
    },
#if 1
    {"MUSB_REQ_VENDER_16_22",
        {(MUSB_DIR_OUT|MUSB_TYPE_VENDOR|MUSB_RECIP_DEVICE),
          (AX_CMD_WRITE_MULTI_FILTER),
          (0),
          (0),
          (0x08)
        }
    },
#endif
    {"MUSB_REQ_VENDER_10_23",
        {(MUSB_DIR_OUT|MUSB_TYPE_VENDOR|MUSB_RECIP_DEVICE),
          (AX_CMD_WRITE_RX_CTL),
          (0x9c),
          (0),
          (0)
        }
    },

// Write Medium Status
    {"MUSB_REQ_VENDOR_1A_24",
        {(MUSB_DIR_OUT|MUSB_TYPE_VENDOR|MUSB_RECIP_DEVICE),
          (AX_CMD_WRITE_MEDIUM_MODE),
          (0x16),
          (0),
          (0)
        }
    },
#if 1
// Read Medium Status
    {"MUSB_REQ_VENDOR_1A_25",
        {(MUSB_DIR_IN|MUSB_TYPE_VENDOR|MUSB_RECIP_DEVICE),
          (AX_CMD_READ_MEDIUM_MODE),
          (0x00),
          (0),
          (1000)
        }
    },
#endif
#if 0

// Read Operation Mode
    {"MUSB_REQ_VENDOR_09_26",
        {(MUSB_DIR_IN|MUSB_TYPE_VENDOR|MUSB_RECIP_DEVICE),
          (AX_CMD_READ_MII_OPERATION_MODE),
          (0x00),
          (0),
          (0x01)
        }
    },

// Read Rx Control Register
    {"MUSB_REQ_VENDOR_0F_27",
        {(MUSB_DIR_IN|MUSB_TYPE_VENDOR|MUSB_RECIP_DEVICE),
          (AX_CMD_READ_RX_CONTROL_REG),
          (0x00),
          (0),
          (0x02)
        }
    },

// Read IPG/IPG1/IPG2 Register
    {"MUSB_REQ_VENDOR_11_28",
        {(MUSB_DIR_IN|MUSB_TYPE_VENDOR|MUSB_RECIP_DEVICE),
          (AX_CMD_READ_IPG012),
          (0x00),
          (0),
          (0x03)
        }
    },

// Read Multi-Filter Array
    {"MUSB_REQ_VENDOR_15_29",
        {(MUSB_DIR_IN|MUSB_TYPE_VENDOR|MUSB_RECIP_DEVICE),
          (AX_CMD_READ_MULTIFILTER_ARRAY),
          (0x00),
          (0),
          (0x08)
        }
    },

// Read Monitor Mode Status
    {"MUSB_REQ_VENDOR_1c_30",
        {(MUSB_DIR_IN|MUSB_TYPE_VENDOR|MUSB_RECIP_DEVICE),
          (AX_CMD_READ_MONITOR_MODE),
          (0x00),
          (0),
          (0x01)
        }
    },

// Read GPIOs
    {"MUSB_REQ_VENDOR_1e_31",
        {(MUSB_DIR_IN|MUSB_TYPE_VENDOR|MUSB_RECIP_DEVICE),
          (AX_CMD_READ_GPIOS),
          (0x00),
          (0),
          (0x01)
        }
    },
#endif

    {NULL,
        {0,
          (0),
          (0),
          (0),
          (0)
        }
    },

};
	unsigned char eth_enum_write_value[5][8] = { {0, 0x80}, {0xe1, 0x05}, {0, 0x32},      // 0x08
            {0, 0, 0, 0x80, 0, 0, 0, 0}, {0, 0, 0, 0x80, 0, 0, 0, 0} };

	xhci = hcd_to_xhci(my_hcd);
	hdev = hdev_list[hub_num-1];
	udev = dev_list[dev_num-1];

	ret = 0;

	dr = kmalloc(sizeof(struct usb_ctrlrequest), GFP_NOIO);
	ptr = kmalloc(2048, GFP_NOIO);
	memset(ptr, 0, 2048);
	dr->bRequestType = MUSB_DIR_IN|MUSB_TYPE_STANDARD|MUSB_RECIP_DEVICE;
	dr->bRequest = MUSB_REQ_GET_DESCRIPTOR;
	dr->wValue = MUSB_DT_CONFIG << 8;
	dr->wIndex = 0;
	dr->wLength = 0x39;
	urb = alloc_ctrl_urb(dr, ptr, udev);
	ret = f_ctrlrequest(urb, udev);
	//parse tx ep, rx ep

	if(ret){
		printk(KERN_ERR "[DEV]Get Device descriptor ctrl request failed!!\n");
		kfree(dr);
		kfree(ptr);
		usb_free_urb(urb);
		return RET_FAIL;
	}
	SetETHEPConfig(dev_num, ptr,udev);

	kfree(dr);
	kfree(ptr);
	usb_free_urb(urb);
	dr = kmalloc(sizeof(struct usb_ctrlrequest), GFP_NOIO);
	dr->bRequestType = 0;
	dr->bRequest = USB_REQ_SET_CONFIGURATION;
	dr->wValue = cpu_to_le16(1);
	dr->wIndex = cpu_to_le16(0);
	dr->wLength = cpu_to_le16(0);
	urb = alloc_ctrl_urb(dr, ptr, udev);
	f_ctrlrequest(urb, udev);
	if(urb->status == 0){
		xhci_dbg(xhci, "set configuration success\n");
		kfree(dr);
		usb_free_urb(urb);
	}
	else{
		xhci_err(xhci, "[ERROR] set configuration failed\n");
		ret = urb->status;
		kfree(dr);
		usb_free_urb(urb);
		return RET_FAIL;
	}

	nEnumStep = 0;
	eth_enum_write_index = 0;
	while(ethenumeration_step[nEnumStep].pDesciptor != NULL){
		struct MUSB_DeviceRequest *pDevReq = &(ethenumeration_step[nEnumStep].sDevReq);

		dr = kmalloc(sizeof(struct usb_ctrlrequest), GFP_NOIO);
		ptr= kmalloc(2048, GFP_NOIO);
		memset(ptr, 0, 2048);

		switch(pDevReq->bRequest){
            case AX_CMD_WRITE_MII_REG:        //0x08
            {
                *ptr = eth_enum_write_value[eth_enum_write_index][0];
                *(ptr+1) = eth_enum_write_value[eth_enum_write_index++][1];
                break;
            }
            case AX_CMD_WRITE_MULTI_FILTER: //0x16
            {
                *ptr = eth_enum_write_value[eth_enum_write_index][0];
                *(ptr+1) = eth_enum_write_value[eth_enum_write_index][1];
                *(ptr+2) = eth_enum_write_value[eth_enum_write_index][2];
                *(ptr+3) = eth_enum_write_value[eth_enum_write_index][3];
                *(ptr+4) = eth_enum_write_value[eth_enum_write_index][4];
                *(ptr+5) = eth_enum_write_value[eth_enum_write_index][5];
                *(ptr+6) = eth_enum_write_value[eth_enum_write_index][6];
                *(ptr+7) = eth_enum_write_value[eth_enum_write_index++][7];
                break;
            }
            default:
			break;
        }
		dr->bRequestType = pDevReq->bmRequestType;
		dr->bRequest = pDevReq->bRequest;
		dr->wValue = pDevReq->wValue;
		dr->wIndex = pDevReq->wIndex;
		dr->wLength = pDevReq->wLength;
		urb = alloc_ctrl_urb(dr, ptr, udev);
		ret = f_ctrlrequest(urb, udev);
		kfree(dr);
		kfree(ptr);
		usb_free_urb(urb);
		if(ret)
		{
			printk(KERN_ERR "[DEV]config ep ctrl request failed!!\n");
			return RET_FAIL;
		}
		nEnumStep++;
	}
	return ret;
}



int f_hub_configuredevice(int hub_num, int port_num, int dev_num
		, int transfer_type, int maxp, int bInterval, char is_config_ep, char is_stress, int stress_config){
	u32 status;
	int ret;
	struct usb_device *udev, *hdev;
	struct xhci_virt_device *virt_dev;
	struct xhci_hcd *xhci;
	struct xhci_input_control_ctx *ctrl_ctx;
	struct usb_host_endpoint *ep_tx, *ep_rx;
	int ep_index_tx, ep_index_rx;
	int slot_state;
	int count = 0;
	int count_down = 1000;
	int speed;
	int mult,burst;
	int dev_slot;

	mult = 0;
	burst = 8;
	dev_slot = 1;
	xhci = hcd_to_xhci(my_hcd);
	hdev = hdev_list[hub_num-1];
	status = 0;
	count = count_down;
	speed = USB_SPEED_HIGH;
	if(hdev->speed == USB_SPEED_SUPER){
		speed = USB_SPEED_SUPER;
	}
	if(transfer_type == EPATT_INT){
		mult = 0;
		burst = 0;
	}
	if(transfer_type == EPATT_ISO){
		mult = 0;
		burst = 0;
		dev_slot = 3;
	}
	while(!(status & USB_PORT_STAT_CONNECTION) && count > 0){
		ret = f_hub_getPortStatus(hub_num, port_num, &status);
		if(ret != RET_SUCCESS){
			xhci_err(xhci, "[ERROR] Get status failed\n");
			return RET_FAIL;
		}
		count--;
	}
	if(count == 0){
		xhci_err(xhci, "[ERRROR] Wait port connection status timeout\n");
		return RET_FAIL;
	}
	xhci_dbg(xhci, "Got port connection status\n");
#if 0
	if(speed == USB_SPEED_SUPER){
		if(f_hub_clearportfeature(hub_num, HUB_FEATURE_C_PORT_CONNECTION, port_num) != RET_SUCCESS){
			xhci_err(xhci, "[ERROR] Clear port reset change failed\n");
			return RET_FAIL;
		}
	}
#endif
	if(f_hub_setportfeature(hub_num, HUB_FEATURE_PORT_RESET, port_num) != RET_SUCCESS){
		xhci_err(xhci, "[ERROR] Set port reset failed\n");
		return RET_FAIL;
	}

	status = 0;
	count = count_down;
	while(!((status>>16) & USB_PORT_STAT_C_RESET) && count > 0){
		ret = f_hub_getPortStatus(hub_num, port_num, &status);
		if(ret != RET_SUCCESS){
			xhci_err(xhci, "[ERROR] Get status failed\n");
			return RET_FAIL;
		}
		count--;
	}
	if(count == 0){
		xhci_err(xhci, "[ERROR] Wait port reset change status timeout\n");
		return RET_FAIL;
	}
	xhci_dbg(xhci, "Port reset done\n");

	//FIXME: check superspeed

	if(speed != USB_SPEED_SUPER){
		if(status & USB_PORT_STAT_HIGH_SPEED){
			speed = USB_SPEED_HIGH;
		}
		else if(status & USB_PORT_STAT_LOW_SPEED){
			speed = USB_SPEED_LOW;
		}
		else{
			speed = USB_SPEED_FULL;
		}
	}

	if(f_hub_clearportfeature(hub_num, HUB_FEATURE_C_PORT_RESET, port_num) != RET_SUCCESS){
		xhci_err(xhci, "[ERROR] clear port reset change failed\n");
		return RET_FAIL;
	}
	//new usb device
	udev = hdev->children[port_num-1];
	udev = mtk_usb_alloc_dev(hdev, hdev->bus, port_num);
	udev->level = hdev->level + 1;
	udev->speed = speed;
	hdev->children[port_num-1] = udev;
	//need to add tt handler
	if (hdev->tt) {
		udev->tt = hdev->tt;
		udev->ttport = hdev->ttport;
	}
	else if(udev->speed != USB_SPEED_HIGH
			&& hdev->speed == USB_SPEED_HIGH) {
		udev->tt = kzalloc(sizeof(struct usb_tt), GFP_KERNEL);
		udev->tt->hub = hdev;
		udev->tt->multi = false;
		udev->tt->think_time = 0;
		udev->ttport = port_num;

	}
	dev_list[dev_num-1] = udev;
	print_speed(udev->speed);
	//enable slot
	ret = f_enable_slot(udev);
	if(ret != RET_SUCCESS){
		return RET_FAIL;
	}

	//address device
	ret = f_address_slot(false, udev);
	if(ret != RET_SUCCESS){
		return RET_FAIL;
	}
	if(is_config_ep){
		//return f_loopback_config_ep(1,2,transfer_type, maxp, bInterval, udev);
		ret = dev_config_ep(1, USB_RX, transfer_type, maxp, bInterval,dev_slot,burst,mult, udev);
		if(ret != RET_SUCCESS){
			return RET_FAIL;
		}
		ret = dev_config_ep(1, USB_TX, transfer_type, maxp, bInterval,dev_slot,burst,mult, udev);
		if(ret != RET_SUCCESS){
			return RET_FAIL;
		}

		ret = f_config_ep(1, EPADD_OUT, transfer_type, maxp, bInterval,burst,mult, udev, 0);
		if(ret != RET_SUCCESS){
			return RET_FAIL;
		}
		ret = f_config_ep(1, EPADD_IN, transfer_type, maxp, bInterval,burst,mult, udev, 1);
		if(ret != RET_SUCCESS){
			return RET_FAIL;
		}
	}
	else if(is_stress){
		if(speed == USB_SPEED_SUPER){
			if(stress_config == 1){//BULK+INT
				ret = dev_config_ep(1, USB_RX, EPATT_BULK, 1024, 1,1,8,0, udev);
				if(ret != RET_SUCCESS){
					return RET_FAIL;
				}
				ret = dev_config_ep(1, USB_TX, EPATT_BULK, 1024, 1, 1, 8,0, udev);
				if(ret != RET_SUCCESS){
					return RET_FAIL;
				}
				ret = f_config_ep(1, EPADD_OUT, EPATT_BULK, 1024, 0,8,0, udev, 0);
				if(ret != RET_SUCCESS){
					return RET_FAIL;
				}
				ret = f_config_ep(1, EPADD_IN, EPATT_BULK, 1024, 0,8,0, udev, 0);
				if(ret != RET_SUCCESS){
					return RET_FAIL;
				}
				ret = dev_config_ep(2, USB_RX, EPATT_INT, 1024, 1,1,0,0, udev);
				if(ret != RET_SUCCESS){
					return RET_FAIL;
				}
				ret = dev_config_ep(2, USB_TX, EPATT_INT, 1024, 1, 1, 0,0, udev);
				if(ret != RET_SUCCESS){
					return RET_FAIL;
				}
				ret = f_config_ep(2, EPADD_OUT, EPATT_INT, 1024, 1,0,0, udev, 0);
				if(ret != RET_SUCCESS){
					return RET_FAIL;
				}
				ret = f_config_ep(2, EPADD_IN, EPATT_INT, 1024, 1,0,0, udev, 1);
				if(ret != RET_SUCCESS){
					return RET_FAIL;
				}
			}
			else if(stress_config == 2){//BULK+ISO
				ret = dev_config_ep(1, USB_RX, EPATT_BULK, 1024, 0,1,8,0, udev);
				if(ret != RET_SUCCESS){
					return RET_FAIL;
				}
				ret = dev_config_ep(1, USB_TX, EPATT_BULK, 1024, 0, 1, 8,0, udev);
				if(ret != RET_SUCCESS){
					return RET_FAIL;
				}
				ret = f_config_ep(1, EPADD_OUT, EPATT_BULK, 1024, 0,8,0, udev, 0);
				if(ret != RET_SUCCESS){
					return RET_FAIL;
				}
				ret = f_config_ep(1, EPADD_IN, EPATT_BULK, 1024, 0,8,0, udev, 0);
				if(ret != RET_SUCCESS){
					return RET_FAIL;
				}
				ret = dev_config_ep(2, USB_RX, EPATT_ISO, 1024, 4,3,0,0, udev);
				if(ret != RET_SUCCESS){
					return RET_FAIL;
				}
				ret = dev_config_ep(2, USB_TX, EPATT_ISO, 1024, 4, 3, 0,0, udev);
				if(ret != RET_SUCCESS){
					return RET_FAIL;
				}
				ret = f_config_ep(2, EPADD_OUT, EPATT_ISO, 1024, 4,0,0, udev, 0);
				if(ret != RET_SUCCESS){
					return RET_FAIL;
				}
				ret = f_config_ep(2, EPADD_IN, EPATT_ISO, 1024, 4,0,0, udev, 1);
				if(ret != RET_SUCCESS){
					return RET_FAIL;
				}
				f_ring_enlarge(EPADD_OUT, 2, dev_num);
				f_ring_enlarge(EPADD_OUT, 2, dev_num);
				f_ring_enlarge(EPADD_IN, 2, dev_num);
				f_ring_enlarge(EPADD_IN, 2, dev_num);
			}
		}
		else if(speed == USB_SPEED_HIGH){
			//return f_loopback_config_ep(1,2,transfer_type, maxp, bInterval, udev);
			ret = dev_config_ep(1, USB_RX, EPATT_BULK, 512, 0,dev_slot,burst,mult, udev);
			if(ret != RET_SUCCESS){
				return RET_FAIL;
			}
			ret = dev_config_ep(1, USB_TX, EPATT_BULK, 512, 0, dev_slot, burst,mult, udev);
			if(ret != RET_SUCCESS){
				return RET_FAIL;
			}
#if 1
			ret = dev_config_ep(2, USB_RX, EPATT_INT, 1024, 1, dev_slot,burst,mult, udev);
			if(ret != RET_SUCCESS){
				return RET_FAIL;
			}
			ret = dev_config_ep(2, USB_TX, EPATT_INT, 1024, 1, dev_slot,burst,mult, udev);
			if(ret != RET_SUCCESS){
				return RET_FAIL;
			}
#endif
			ret = f_config_ep(1, EPADD_OUT, EPATT_BULK, 512, 0,0,0, udev, 0);
			if(ret != RET_SUCCESS){
				return RET_FAIL;
			}
			ret = f_config_ep(1, EPADD_IN, EPATT_BULK, 512, 0,0,0, udev, 0);
			if(ret != RET_SUCCESS){
				return RET_FAIL;
			}
#if 1
			ret = f_config_ep(2, EPADD_OUT, EPATT_INT, 1024, 1,0,0, udev, 0);
			if(ret != RET_SUCCESS){
				return RET_FAIL;
			}
			ret = f_config_ep(2, EPADD_IN, EPATT_INT, 1024, 1,0,0, udev, 1);
			if(ret != RET_SUCCESS){
				return RET_FAIL;
			}
#endif
		}
		else if(speed == USB_SPEED_FULL){
			ret = dev_config_ep(1, USB_RX, EPATT_BULK, 64, 0,dev_slot,burst,mult, udev);
			if(ret != RET_SUCCESS){
				return RET_FAIL;
			}
			ret = dev_config_ep(1, USB_TX, EPATT_BULK, 64, 0, dev_slot, burst,mult, udev);
			if(ret != RET_SUCCESS){
				return RET_FAIL;
			}
#if 1
			ret = dev_config_ep(2, USB_RX, EPATT_INT, 64, 1, dev_slot,burst,mult, udev);
			if(ret != RET_SUCCESS){
				return RET_FAIL;
			}
			ret = dev_config_ep(2, USB_TX, EPATT_INT, 64, 1, dev_slot,burst,mult, udev);
			if(ret != RET_SUCCESS){
				return RET_FAIL;
			}
#endif
			ret = f_config_ep(1, EPADD_OUT, EPATT_BULK, 64, 0,0,0, udev, 0);
			if(ret != RET_SUCCESS){
				return RET_FAIL;
			}
			ret = f_config_ep(1, EPADD_IN, EPATT_BULK, 64, 0,0,0, udev, 0);
			if(ret != RET_SUCCESS){
				return RET_FAIL;
			}
#if 1
			ret = f_config_ep(2, EPADD_OUT, EPATT_INT, 64, 1,0,0, udev, 0);
			if(ret != RET_SUCCESS){
				return RET_FAIL;
			}
			ret = f_config_ep(2, EPADD_IN, EPATT_INT, 64, 1,0,0, udev, 1);
			if(ret != RET_SUCCESS){
				return RET_FAIL;
			}
#endif
		}

	}
	return RET_SUCCESS;

}

void f_hub_alloc_urb(struct urb *urb, void *buffer, int transfer_type, int data_length, int ep_num
		, int dir, int interval, struct usb_host_endpoint *ep, int dev_index){
	struct usb_device *udev, *rhdev;
	struct xhci_hcd *xhci;
	int ret;
	int max_data_length = MAX_DATA_LENGTH;

	xhci = hcd_to_xhci(my_hcd);
	udev = dev_list[dev_index];

	if(transfer_type == EPATT_BULK && dir == URB_DIR_OUT){
		usb_fill_bulk_urb(urb, udev, usb_sndbulkpipe(udev, ep_num), buffer, max_data_length, NULL, NULL);
	}
	else if(transfer_type == EPATT_INT && dir == URB_DIR_OUT ){
		usb_fill_int_urb(urb, udev, usb_sndintpipe(udev, ep_num), buffer, max_data_length, NULL, NULL, interval);
	}
	else if(transfer_type == EPATT_BULK && dir == URB_DIR_IN){
		usb_fill_bulk_urb(urb, udev, usb_rcvbulkpipe(udev, ep_num), buffer, max_data_length, NULL, NULL);
	}
	else if(transfer_type == EPATT_INT && dir == URB_DIR_IN){
		usb_fill_int_urb(urb, udev, usb_rcvintpipe(udev, ep_num), buffer, max_data_length, NULL, NULL, interval);
	}
	urb->status = -EINPROGRESS;
	urb->actual_length = 0;
	urb->transfer_flags |= dir | URB_ZERO_PACKET;
	urb->ep = ep;
	urb->num_sgs = 0;

	ret = mtk_map_urb_for_dma(my_hcd, urb, GFP_KERNEL);
	urb->transfer_buffer_length = data_length;
	xhci_dbg(xhci, "urb 0x%p\n", urb);
	xhci_dbg(xhci, "urb transfer buffer 0x%p\n", urb->transfer_buffer);
}

#define TRANSFER_MAX_LENGTH	16*1024-1
#define MORE_TRANSFER_TIMES	100

struct transfer_data {
	struct xhci_hcd *xhci;
    struct usb_device *udev;
	int ep_num;
	int dir;
	int stop_count;
	int cur_stop_count;
	int transfer_length;
	volatile char is_running;
	volatile char is_correct;
};

static int transfer_thread(void *data){
	int ret;
	struct transfer_data *t_data = data;
	struct device	*dev;
	struct usb_device *udev;
	struct xhci_hcd *xhci;
	struct urb *urb;
	struct usb_host_endpoint *ep;
	int max_esit_payload;
	char is_going;
	u32 length;
	int transfer_times = 0;
	int more_transfer_times = MORE_TRANSFER_TIMES;
	
	ret =0;
	xhci = t_data->xhci;
	dev = xhci_to_hcd(xhci)->self.controller;
	udev = t_data->udev;
	if(t_data->dir == URB_DIR_OUT){
		ep = udev->ep_out[t_data->ep_num];
	}
	else{
		ep = udev->ep_in[t_data->ep_num];
	}
	max_esit_payload = mtk_xhci_get_max_esit_payload(xhci, udev, ep);
	length = t_data->transfer_length;
	urb = usb_alloc_urb(0, GFP_KERNEL);
	ret = f_fill_urb(urb,t_data->ep_num,length,0,t_data->dir, 0, max_esit_payload, udev);
	xhci_err(xhci, "Start random stop transfer thread, cur_stop_count %d, stop_count %d\n"
		, t_data->cur_stop_count, t_data->stop_count);
	is_going = true;
	do{
		//queue urb
		urb->status = -EINPROGRESS;
		urb->actual_length = 0;
		urb->transfer_buffer_length = length;
		ret = f_queue_urb(urb,1, udev);
		if(ret){
			t_data->cur_stop_count++;


			xhci_err(xhci, "Transfer of ep %d, dir %x  t_data->cur_stop_count :%d   stop_count :%d \n"
							, t_data->ep_num, t_data->dir,t_data->cur_stop_count ,t_data->stop_count);


			if(t_data->cur_stop_count > t_data->stop_count){
				//error
				xhci_err(xhci, "Transfer of ep %d, dir %d fail occurred more than set %d times\n"
				, t_data->ep_num, t_data->dir, t_data->stop_count);
				t_data->is_correct = false;
				is_going = false;
				xhci_err(xhci, "[FAIL]\n");
			}
			//must delay some time
			dev_polling_stop_status(udev);
		}
		else{
			if(t_data->cur_stop_count == t_data->stop_count){
				transfer_times++;
				xhci_err(xhci, "transfer_times++ %d\n", transfer_times);
				if(transfer_times >= more_transfer_times){
					is_going = false;
					t_data->is_correct = true;
					xhci_err(xhci, "[PASS]\n");
				}
			}
		}
	}while(is_going && !g_stopped);
	length = TRANSFER_MAX_LENGTH;
	f_free_urb(urb,length,0);
	xhci_err(xhci, "[INFO]Exit transfer thread, ep_num %d, dir %d\n", t_data->ep_num, t_data->dir);
	t_data->is_running = false;

	g_exec_done = true;  // let f_test_lib_cleanup to get the thread done info
	return ret;
}

int f_random_stop(int ep_1_num, int ep_2_num, int stop_count_1, int stop_count_2, int urb_dir_1, int urb_dir_2, int length){
	struct transfer_data *t_data_1, *t_data_2;
	struct xhci_hcd *xhci;
	struct device	*dev;
	struct usb_device *udev, *rhdev;
	int ret;
	ret = 0;

	xhci = hcd_to_xhci(my_hcd);
	dev = xhci_to_hcd(xhci)->self.controller;//dma stream buffer
	rhdev = my_hcd->self.root_hub;
	udev = rhdev->children[g_port_id-1];
	t_data_1 = kzalloc(sizeof(struct transfer_data), GFP_KERNEL);
	t_data_1->xhci = xhci;
	t_data_1->udev = udev;
	t_data_1->ep_num = ep_1_num;
	t_data_1->dir = urb_dir_1;
	t_data_1->cur_stop_count = 0;
	t_data_1->stop_count = stop_count_1;
	t_data_1->transfer_length = length;
	t_data_1->is_running = true;
	t_data_1->is_correct = true;

	t_data_2 = kzalloc(sizeof(struct transfer_data), GFP_KERNEL);
	t_data_2->xhci = xhci;
	t_data_2->udev = udev;
	t_data_2->ep_num = ep_2_num;
	t_data_2->dir = urb_dir_2;
	t_data_2->cur_stop_count = 0;
	t_data_2->stop_count = stop_count_2;
	t_data_2->transfer_length = length;
	t_data_2->is_running = true;
	t_data_2->is_correct = true;
	g_stopped = false;
	g_exec_done = false ;
	kthread_run(transfer_thread, t_data_1, "transfer_1_t");
	kthread_run(transfer_thread, t_data_2, "transfer_2_t");
#if 0
	while(t_data_1->is_running || t_data_2->is_running){
		msleep(10);
	}
	if(!t_data_1->is_correct || !t_data_2->is_correct){
		return RET_FAIL;
	}
#endif
	return RET_SUCCESS;
}

struct stress_data{
	int dev_num;
	int ep_num;
	struct xhci_hcd *xhci;
    struct usb_device *udev;
	struct urb *urb_rx_list[TOTAL_URB];
	struct urb *urb_tx_list[TOTAL_URB];
	int loop_count[TOTAL_URB];
	char *buffer[TOTAL_URB];
	char isCompare;
	int max_esit_payload;
	int max_buffer_len[TOTAL_URB];
};

static int stress_ep0_thread(void *data){
	int ret;
	struct stress_data *str_data = data;
	struct xhci_hcd *xhci;
	struct usb_device *udev;
	struct device	*dev;
	char is_running;
	int length;
	char *ptr1,*ptr2;
	struct usb_ctrlrequest *dr1, *dr2;
	struct urb *urb1, *urb2;
	int count, count_boundary;
	dma_addr_t mapping;
	int i;

	xhci = str_data->xhci;
	udev = str_data->udev;
	is_running = true;
#if 0
	dev = xhci_to_hcd(xhci)->self.controller;
	is_running = true;
	length = 100;

	ptr1= kmalloc(2048, GFP_NOIO);
	get_random_bytes(ptr1, 2048);

	ptr2= kzalloc(2048, GFP_NOIO);
	memcpy(ptr2, ptr1, 2048);

	dr1 = kmalloc(sizeof(struct usb_ctrlrequest), GFP_NOIO);
	dr1->bRequestType = USB_DIR_OUT | USB_TYPE_VENDOR | USB_RECIP_DEVICE;;
	dr1->bRequest = AT_CTRL_TEST;
	dr1->wValue = cpu_to_le16(0);
	dr1->wIndex = cpu_to_le16(0);
	dr1->wLength = cpu_to_le16(2048);

	dr2 = kmalloc(sizeof(struct usb_ctrlrequest), GFP_NOIO);
	dr2->bRequestType = USB_DIR_IN | USB_TYPE_VENDOR | USB_RECIP_DEVICE;;
	dr2->bRequest = AT_CTRL_TEST;
	dr2->wValue = cpu_to_le16(0);
	dr2->wIndex = cpu_to_le16(0);
	dr2->wLength = cpu_to_le16(2048);

	urb1 = usb_alloc_urb(0, GFP_NOIO);
	usb_fill_control_urb(urb1, udev, usb_sndctrlpipe(udev, 0), dr1, ptr1,
			dr1->wLength, NULL, NULL);
	urb1->ep = &udev->ep0;
	mapping = dma_map_single(dev, ptr1, 2048, DMA_BIDIRECTIONAL);
	urb1->transfer_dma = mapping;
	dma_sync_single_for_device(dev, mapping, 2048, DMA_BIDIRECTIONAL);

	urb2 = usb_alloc_urb(0, GFP_NOIO);
	usb_fill_control_urb(urb2, udev, usb_rcvctrlpipe(udev, 0), dr2, ptr2,
			dr2->wLength, NULL, NULL);
	urb2->ep = &udev->ep0;
	mapping = dma_map_single(dev, ptr2, 2048, DMA_BIDIRECTIONAL);
	urb2->transfer_dma = mapping;
	dma_sync_single_for_device(dev, mapping, 2048, DMA_BIDIRECTIONAL);
#endif
	count = 0;
	count_boundary = 100;
	do{
		msleep(10);
		dev_polling_status(udev);
#if 0
		length = ((get_random_int()%2048) + 1);
		//ret=dev_ctrl_loopback(length,udev);
		urb1->status = -EINPROGRESS;
		urb1->actual_length = 0;
		urb1->transfer_buffer_length = length;
		ret = f_ctrlrequest(urb1, udev);
		if(ret != RET_SUCCESS){
			g_correct = false;
			is_running = false;
		}

		urb2->status = -EINPROGRESS;
		urb2->actual_length = 0;
		urb2->transfer_buffer_length = length;
		ret = f_ctrlrequest(urb2, udev);
		if(ret != RET_SUCCESS){
			g_correct = false;
			is_running = false;
		}
		count++;
		if(count >= count_boundary){
			dma_sync_single_for_device(dev, urb2->transfer_dma, 2048, DMA_BIDIRECTIONAL);
			for(i=0; i<2048; i++){
				if((*(ptr1+i)) != (*(ptr2+i))){
					xhci_err(xhci, "[ERROR] buffer %d not match, tx 0x%x, rx 0x%x\n", i, *(ptr1+i), *(ptr2+i));
					return RET_FAIL;
				}
			}
			count = 0;
		}
#endif

	}while(is_running && g_correct);
	xhci_err(xhci, "[ERROR] exit ep0 stress thread, dev_num %d\n"
		, str_data->dev_num);
}

static int stress_tx_thread(void *data){
	int i, ret;
	struct urb *urb_tx;
	struct stress_data *str_data = data;
	struct usb_device *udev;
	struct xhci_virt_device *virt_dev;
	struct xhci_hcd *xhci;
	struct usb_host_endpoint *ep_tx, *ep_rx;
	int ep_index_tx, ep_index_rx;
	int max_esit_payload;
	int tx_index;
	int tx_status;
	char is_running;

	is_running = true;
	xhci = str_data->xhci;
	udev = str_data->udev;

	tx_index = 0;
	xhci_err(xhci, "[STRESS] start tx threading, dev_num %d, ep_num %d\n", str_data->dev_num, str_data->ep_num);
	do{
		urb_tx = str_data->urb_tx_list[tx_index];
		tx_status = urb_tx->status;
		while(tx_status != URB_STATUS_IDLE){
			if(tx_status != 0 && tx_status != -EINPROGRESS
				&& tx_status != URB_STATUS_IDLE && tx_status != URB_STATUS_RX){
				xhci_err(xhci, "[STRESS][ERROR] dev %d, ep %d, urb_tx %d not in valid status - %d\n"
					, str_data->dev_num, str_data->ep_num, tx_index, urb_tx->status);
				is_running = false;
				g_correct = false;
				break;
			}
			msleep(1);
			tx_status = urb_tx->status;
		}
		xhci_dbg(xhci, "[STRESS] queue tx urb %d, dev %d, ep %d\n"
			, tx_index, str_data->dev_num, str_data->ep_num);
		urb_tx->actual_length = 0;
		urb_tx->status = -EINPROGRESS;
		if(urb_tx->number_of_packets > 0){
			for(i=0; i<urb_tx->number_of_packets; i++){
				urb_tx->iso_frame_desc[i].actual_length = 0;
			}
		}
		while(g_stopping_ep){}
		f_queue_urb(urb_tx, 0, udev);

		tx_index++;
		if(tx_index == TOTAL_URB){
			tx_index = 0;
		}
	}while(is_running && g_correct);
	xhci_err(xhci, "[ERROR] exit tx urb handler thread, dev_num %d, ep_num %d\n"
		, str_data->dev_num, str_data->ep_num);
	kfree(str_data);
}

static int stress_tx_done_thread(void *data){
	int i, ret;
	struct urb *urb_tx, *urb_rx;
	struct stress_data *str_data = data;
	struct usb_device *udev;
	struct xhci_virt_device *virt_dev;
	struct xhci_hcd *xhci;
	struct usb_host_endpoint *ep_tx, *ep_rx;
	int ep_index_tx, ep_index_rx;
	int max_esit_payload;
	int tx_index;
	int tx_status;
	char is_running;
	is_running = true;

	xhci = str_data->xhci;
	udev = str_data->udev;
	tx_index = 0;
	xhci_err(xhci, "[STRESS] start tx done threading, dev_num %d, ep_num %d\n"
		, str_data->dev_num, str_data->ep_num);
	do{
		urb_tx = str_data->urb_tx_list[tx_index];
		tx_status = urb_tx->status;
		while(tx_status != 0){
			if(tx_status != 0 && tx_status != -EINPROGRESS
				&& tx_status != URB_STATUS_IDLE && tx_status != URB_STATUS_RX){
				xhci_err(xhci, "[STRESS][ERROR] dev %d, ep %d, urb_tx %d not in valid status - %d\n"
					, str_data->dev_num, str_data->ep_num, tx_index, urb_tx->status);
				is_running = false;
				g_correct = false;
				break;
			}
			msleep(1);
			tx_status = urb_tx->status;
		}
		ep_rx = urb_tx->ep;
		if(usb_endpoint_xfer_isoc(&ep_rx->desc)){
			msleep(1500);
		}
		//queue rx
		xhci_dbg(xhci, "[STRESS] tx urb %d success, queue rx urb, dev %d, ep %d\n"
			, tx_index, str_data->dev_num, str_data->ep_num);
		urb_tx->status = URB_STATUS_RX;
		urb_rx = str_data->urb_rx_list[tx_index];
		urb_rx->status = -EINPROGRESS;
		urb_rx->transfer_buffer_length = urb_tx->actual_length;
		urb_rx->actual_length = 0;
		if(urb_rx->number_of_packets > 0){
			for(i=0; i<urb_rx->number_of_packets; i++){
				urb_rx->iso_frame_desc[i].actual_length = 0;
			}
		}
		while(g_stopping_ep){}
		f_queue_urb(urb_rx, 0, udev);
		tx_index++;
		if(tx_index == TOTAL_URB){
			tx_index = 0;
		}
	} while(is_running && g_correct);
	xhci_err(xhci, "[STRESS][ERROR] exit tx urb done thread, dev_num %d, ep_num %d\n"
		, str_data->dev_num, str_data->ep_num);
	kfree(str_data);
}

static stress_rx_done_thread(void *data){
	int i, ret;
	struct urb *urb_tx, *urb_rx;
	struct stress_data *str_data = data;
	struct usb_device *udev;
	struct xhci_virt_device *virt_dev;
	struct xhci_hcd *xhci;
	struct usb_host_endpoint *ep_tx, *ep_rx;
	int ep_index_tx, ep_index_rx;
	int max_esit_payload;
	int tx_index;
	int rx_status;
	char is_running;
	int data_len;
	int count;
#if 0
	u32 __iomem *addr;
	int temp;
#endif
	is_running = true;
	char *tmp1, *tmp2;
	xhci = str_data->xhci;
	udev = str_data->udev;
	tx_index = 0;
	count = 0;
	xhci_err(xhci, "[STRESS] start rx done threading, dev_num %d, ep_num %d\n"
		, str_data->dev_num, str_data->ep_num);
	do{
		urb_rx = str_data->urb_rx_list[tx_index];
		rx_status = urb_rx->status;
		xhci_dbg(xhci, "[STRESS] check rx urb %d 0x%p, dev %d, ep %d\n"
			, tx_index, urb_rx, str_data->dev_num, str_data->ep_num);
		while(rx_status != 0){
			if(rx_status != 0 && rx_status != -EINPROGRESS){
				xhci_err(xhci, "[STRESS][ERROR] dev %d, ep %d, urb_rx %d not in valid status - %d\n"
					, str_data->dev_num, str_data->ep_num, tx_index, urb_rx->status);
				is_running = false;
				g_correct = false;
				break;
			}
			msleep(1);
			rx_status = urb_rx->status;
		}
		//update urb_rx status to IDLE
		//update urb_rx status to INPROGRESS
		xhci_dbg(xhci, "[STRESS] rx urb %d done, dev %d, ep %d\n"
		, tx_index, str_data->dev_num, str_data->ep_num);

		if(str_data->isCompare){
			str_data->loop_count[tx_index]++;
			if(((str_data->ep_num==1) && (str_data->loop_count[tx_index] == 300))
				|| ((str_data->ep_num==2) && (str_data->loop_count[tx_index] == 300))){
				data_len = GPD_LENGTH;
				//xhci_err(xhci, "[STRESS] comparing dev %d ep %d buffer %d\n", str_data->dev_num, str_data->ep_num, tx_index);
				//compare buffer data
				for(i=0; i<data_len; i++){
					tmp1 = urb_rx->transfer_buffer+i;
					tmp2 = str_data->buffer[tx_index]+i;
					if((*tmp1) != (*tmp2)){
#if 0
						//generate LGO_U1
						addr = &xhci->op_regs->port_power_base + NUM_PORT_REGS*(0 & 0xff);
						temp = xhci_readl(xhci, addr);
						temp = temp & (~(0x000000ff));
						temp = temp | 1;
						xhci_writel(xhci, temp, addr);
						msleep(5);
						temp = 0;
						xhci_writel(xhci, temp, addr);
#endif
						xhci_err(xhci, "[STRESS][ERROR] buffer %d not match, rx 0x%x, buffer 0x%x, dev %d, ep %d\n"
							, i, *tmp1, *tmp2, str_data->dev_num, str_data->ep_num);

						is_running = false;
//						while(1);
						g_correct = false;
						break;


					}
				}
				//xhci_err(xhci, "[STRESS] comparing buffer dev %d ep %d %d done\n", str_data->dev_num, str_data->ep_num, tx_index);
				//reset loop_count
				str_data->loop_count[tx_index]=0;
			}
		}

		urb_tx = str_data->urb_tx_list[tx_index];
		urb_tx->status = URB_STATUS_IDLE;
		urb_rx->status = -EINPROGRESS;

		tx_index++;
		if(tx_index == TOTAL_URB){
			count++;
			if(count == 100){
				xhci_err(xhci, "[STRESS] stress is running, dev %d ep %d\n",str_data->dev_num, str_data->ep_num);
				count = 0;
			}
			tx_index = 0;
		}
	}while(is_running && g_correct);
	xhci_err(xhci, "[STRESS][ERROR] exit rx urb done thread, dev_num %d, ep_num %d\n"
		, str_data->dev_num, str_data->ep_num);
	kfree(str_data);
}

static int stress_rdn_len_tx_thread(void *data){
	int i, ret;
	struct urb *urb_tx;
	struct stress_data *str_data = data;
	struct usb_device *udev;
	struct xhci_virt_device *virt_dev;
	struct xhci_hcd *xhci;
	struct usb_host_endpoint *ep_tx, *ep_rx;
	struct urb_priv	*urb_priv;
	int ep_index_tx, ep_index_rx;
	int max_esit_payload;
	int tx_index;
	int tx_status;
	char is_running;
	int this_len;
	int size;

	is_running = true;
	xhci = str_data->xhci;
	udev = str_data->udev;

	tx_index = 0;
	xhci_err(xhci, "[STRESS] start tx threading, dev_num %d, ep_num %d\n", str_data->dev_num, str_data->ep_num);
	do{
		urb_tx = str_data->urb_tx_list[tx_index];
		tx_status = urb_tx->status;
		while(tx_status != URB_STATUS_IDLE){
			if(tx_status != 0 && tx_status != -EINPROGRESS
				&& tx_status != URB_STATUS_IDLE && tx_status != URB_STATUS_RX){
				xhci_err(xhci, "[STRESS][ERROR] dev %d, ep %d, urb_tx %d not in valid status - %d\n"
					, str_data->dev_num, str_data->ep_num, tx_index, urb_tx->status);
				is_running = false;
				g_correct = false;
				break;
			}
			msleep(1);
			tx_status = urb_tx->status;
		}
		xhci_dbg(xhci, "[STRESS] queue tx urb %d, dev %d, ep %d\n"
			, tx_index, str_data->dev_num, str_data->ep_num);
		urb_tx->actual_length = 0;
		urb_tx->status = -EINPROGRESS;

		this_len = (get_random_int()%(GPD_LENGTH_RDN-512)) + 1;
		if((this_len % str_data->max_esit_payload) == 0){
			this_len++;
		}
		if(urb_tx->number_of_packets > 0){
			max_esit_payload = str_data->max_esit_payload;
			urb_tx->number_of_packets = ((this_len+max_esit_payload)/max_esit_payload);
			for(i=0; i<urb_tx->number_of_packets; i++){
				urb_tx->iso_frame_desc[i].actual_length = 0;
				if(i == urb_tx->number_of_packets-1){
					urb_tx->iso_frame_desc[i].length = (this_len-(i*max_esit_payload));
				}
				else{
					urb_tx->iso_frame_desc[i].length = max_esit_payload;
				}
			}
			if(urb_tx->hcpriv){
				mtktest_xhci_urb_free_priv(xhci, urb_tx->hcpriv);
			}
			size = urb_tx->number_of_packets;
			urb_priv = kmalloc(sizeof(struct urb_priv) + size * sizeof(struct xhci_td *), GFP_KERNEL);
			if (!urb_priv){
				xhci_err(xhci, "[ERROR] allocate urb_priv failed\n");
				return RET_FAIL;
			}
			for (i = 0; i < size; i++) {
				urb_priv->td[i] = kmalloc(sizeof(struct xhci_td), GFP_KERNEL);
				if (!urb_priv->td[i]) {
					urb_priv->length = i;
					mtktest_xhci_urb_free_priv(xhci, urb_priv);
					return RET_FAIL;
				}
			}
			urb_priv->length = size;
			urb_priv->td_cnt = 0;
			urb_tx->hcpriv = urb_priv;
		}

		while(g_stopping_ep){}
		urb_tx->transfer_buffer_length = this_len;
		if(this_len > str_data->max_buffer_len[tx_index]){
			str_data->max_buffer_len[tx_index] = this_len;
		}
		f_queue_urb(urb_tx, 0, udev);

		tx_index++;
		if(tx_index == TOTAL_URB){
			tx_index = 0;
		}
	}while(is_running && g_correct);
	xhci_err(xhci, "[ERROR] exit tx urb handler thread, dev_num %d, ep_num %d\n"
		, str_data->dev_num, str_data->ep_num);
	kfree(str_data);
}

static int stress_rdn_len_tx_done_thread(void *data){
	int i, ret;
	struct urb *urb_tx, *urb_rx;
	struct stress_data *str_data = data;
	struct usb_device *udev;
	struct xhci_virt_device *virt_dev;
	struct xhci_hcd *xhci;
	struct usb_host_endpoint *ep_tx, *ep_rx;
	struct urb_priv	*urb_priv;
	int ep_index_tx, ep_index_rx;
	int max_esit_payload;
	int tx_index;
	int tx_status;
	char is_running;
	is_running = true;
	int size;

	xhci = str_data->xhci;
	udev = str_data->udev;
	tx_index = 0;
	xhci_err(xhci, "[STRESS] start tx done threading, dev_num %d, ep_num %d\n"
		, str_data->dev_num, str_data->ep_num);
	do{
		urb_tx = str_data->urb_tx_list[tx_index];
		tx_status = urb_tx->status;
		while(tx_status != 0){
			if(tx_status != 0 && tx_status != -EINPROGRESS
				&& tx_status != URB_STATUS_IDLE && tx_status != URB_STATUS_RX){
				xhci_err(xhci, "[STRESS][ERROR] dev %d, ep %d, urb_tx %d not in valid status - %d\n"
					, str_data->dev_num, str_data->ep_num, tx_index, urb_tx->status);
				is_running = false;
				g_correct = false;
				break;
			}
			msleep(1);
			tx_status = urb_tx->status;
		}
		ep_rx = urb_tx->ep;
		if(usb_endpoint_xfer_isoc(&ep_rx->desc)){
			msleep(1500);
		}
		//queue rx
		xhci_dbg(xhci, "[STRESS] tx urb %d success, queue rx urb, dev %d, ep %d\n"
			, tx_index, str_data->dev_num, str_data->ep_num);
		urb_tx->status = URB_STATUS_RX;
		urb_rx = str_data->urb_rx_list[tx_index];
		urb_rx->status = -EINPROGRESS;
		urb_rx->transfer_buffer_length = urb_tx->actual_length;
		urb_rx->actual_length = 0;
		if(urb_rx->number_of_packets > 0){
			max_esit_payload = str_data->max_esit_payload;
			urb_rx->number_of_packets = ((urb_tx->actual_length+max_esit_payload)/max_esit_payload);
			for(i=0; i<urb_rx->number_of_packets; i++){
				urb_rx->iso_frame_desc[i].actual_length = 0;
			}
			if(urb_rx->hcpriv){
				mtktest_xhci_urb_free_priv(xhci, urb_rx->hcpriv);
			}
			size = urb_rx->number_of_packets;
			urb_priv = kmalloc(sizeof(struct urb_priv) + size * sizeof(struct xhci_td *), GFP_KERNEL);
			if (!urb_priv){
				xhci_err(xhci, "[ERROR] allocate urb_priv failed\n");
				return RET_FAIL;
			}
			for (i = 0; i < size; i++) {
				urb_priv->td[i] = kmalloc(sizeof(struct xhci_td), GFP_KERNEL);
				if (!urb_priv->td[i]) {
					urb_priv->length = i;
					mtktest_xhci_urb_free_priv(xhci, urb_priv);
					return RET_FAIL;
				}
			}
			urb_priv->length = size;
			urb_priv->td_cnt = 0;
			urb_rx->hcpriv = urb_priv;
		}
		while(g_stopping_ep){}
		f_queue_urb(urb_rx, 0, udev);
		tx_index++;
		if(tx_index == TOTAL_URB){
			tx_index = 0;
		}
	} while(is_running && g_correct);
	xhci_err(xhci, "[STRESS][ERROR] exit tx urb done thread, dev_num %d, ep_num %d\n"
		, str_data->dev_num, str_data->ep_num);
	kfree(str_data);
}

static stress_rdn_len_rx_done_thread(void *data){
	int i, ret;
	struct urb *urb_tx, *urb_rx;
	struct stress_data *str_data = data;
	struct usb_device *udev;
	struct xhci_virt_device *virt_dev;
	struct xhci_hcd *xhci;
	struct usb_host_endpoint *ep_tx, *ep_rx;
	int ep_index_tx, ep_index_rx;
	int max_esit_payload;
	int tx_index;
	int rx_status;
	char is_running;
	int data_len;
	int count;
#if 0
	u32 __iomem *addr;
	int temp;
#endif
	is_running = true;
	char *tmp1, *tmp2;
	xhci = str_data->xhci;
	udev = str_data->udev;
	tx_index = 0;
	count = 0;
	xhci_err(xhci, "[STRESS] start rx done threading, dev_num %d, ep_num %d\n"
		, str_data->dev_num, str_data->ep_num);
	do{
		urb_rx = str_data->urb_rx_list[tx_index];
		rx_status = urb_rx->status;
		xhci_dbg(xhci, "[STRESS] check rx urb %d 0x%p, dev %d, ep %d\n"
			, tx_index, urb_rx, str_data->dev_num, str_data->ep_num);
		while(rx_status != 0){
			if(rx_status != 0 && rx_status != -EINPROGRESS){
				xhci_err(xhci, "[STRESS][ERROR] dev %d, ep %d, urb_rx %d not in valid status - %d\n"
					, str_data->dev_num, str_data->ep_num, tx_index, urb_rx->status);
				is_running = false;
				g_correct = false;
				break;
			}
			msleep(1);
			rx_status = urb_rx->status;
		}
		//update urb_rx status to IDLE
		//update urb_rx status to INPROGRESS
		xhci_dbg(xhci, "[STRESS] rx urb %d done, dev %d, ep %d\n"
		, tx_index, str_data->dev_num, str_data->ep_num);

		if(str_data->isCompare){
			str_data->loop_count[tx_index]++;
			if(((str_data->ep_num==1) && (str_data->loop_count[tx_index] == 300))
				|| ((str_data->ep_num==2) && (str_data->loop_count[tx_index] == 300))){
				data_len = str_data->max_buffer_len[tx_index];
				//xhci_err(xhci, "[STRESS] comparing dev %d ep %d buffer %d\n", str_data->dev_num, str_data->ep_num, tx_index);
				//compare buffer data
				for(i=0; i<data_len; i++){
					tmp1 = urb_rx->transfer_buffer+i;
					tmp2 = str_data->buffer[tx_index]+i;
					if((*tmp1) != (*tmp2)){
						xhci_err(xhci, "[STRESS][ERROR] buffer %d not match, rx 0x%x, buffer 0x%x, dev %d, ep %d\n"
							, i, *tmp1, *tmp2, str_data->dev_num, str_data->ep_num);
						is_running = false;

						g_correct = false;
						break;
					}
				}
				//xhci_err(xhci, "[STRESS] comparing buffer dev %d ep %d %d done\n", str_data->dev_num, str_data->ep_num, tx_index);
				//reset loop_count
				str_data->loop_count[tx_index]=0;
			}
		}

		urb_tx = str_data->urb_tx_list[tx_index];
		urb_tx->status = URB_STATUS_IDLE;
		urb_rx->status = -EINPROGRESS;

		tx_index++;
		if(tx_index == TOTAL_URB){
			count++;
			if(count == 100){
				xhci_err(xhci, "[STRESS] stress is running, dev %d ep %d\n",str_data->dev_num, str_data->ep_num);
				count = 0;
			}
			tx_index = 0;
		}
	}while(is_running && g_correct);
	xhci_err(xhci, "[STRESS][ERROR] exit rx urb done thread, dev_num %d, ep_num %d\n"
		, str_data->dev_num, str_data->ep_num);
	kfree(str_data);
}


int f_add_rdn_len_str_threads(int dev_num, int ep_num, int maxp, char isCompare, struct usb_device *usbdev, char isEP0){
	int ret,i,j;
	struct device	*dev;
	struct xhci_hcd *xhci;
	struct usb_device *udev, *rhdev;
	struct stress_data *str_data;
	int data_len;
	dma_addr_t mapping;
	int iso_packet_num;
	struct usb_host_endpoint *ep;
	u8 *tmp;

	xhci = hcd_to_xhci(my_hcd);
	dev = xhci_to_hcd(xhci)->self.controller;
	if(!usbdev){
		rhdev = my_hcd->self.root_hub;
		udev = rhdev->children[g_port_id-1];
	}
	else{
		udev = usbdev;
	}

	ep = udev->ep_out[ep_num];

	if(usb_endpoint_xfer_isoc(&ep->desc)){
		iso_packet_num = ((GPD_LENGTH_RDN+maxp)/maxp);
	}
	else{
		iso_packet_num = 0;
	}

//	xhci_dbg(xhci, "[STRESS DBG]dev slot id %d\n", udev->slot_id);
	xhci_err(xhci, "[STRESS]Start stress process, dev_num %d, ep_num %d\n", dev_num, ep_num);
	str_data = kzalloc(sizeof(struct stress_data), GFP_KERNEL);
	str_data->xhci = xhci;
	str_data->udev = udev;
	str_data->dev_num = dev_num;
	str_data->ep_num = ep_num;
	str_data->isCompare = isCompare;
	str_data->max_esit_payload = maxp;
	xhci_err(xhci, "[STRESS] str_data address 0x%p\n", str_data);

	for(i=0; i<TOTAL_URB; i++){
		str_data->loop_count[i] = 0;
		str_data->max_buffer_len[i] = 0;
	}

	data_len = GPD_LENGTH_RDN;
	for(i=0; i<TOTAL_URB; i++){
		str_data->buffer[i] = kmalloc(data_len, GFP_KERNEL);
		get_random_bytes(str_data->buffer[i], data_len);
#if 0
		for(j=0; j<data_len; j++){
			tmp = str_data->buffer[i]+j;
			if((j%1024)==0){
				*tmp = (u8)i&0xff;
			}
			else if(j%1024==1){
				*tmp = (u8)(j/1024)&0xff;
			}
			else{
				*tmp = (u8)((j+i)&0xff);
			}
		}
#endif
	}
//	xhci_err(xhci, "iso_packet_num %d\n", iso_packet_num);
	for(i=0; i<TOTAL_URB; i++){
		str_data->urb_tx_list[i] = usb_alloc_urb(iso_packet_num, GFP_NOIO);
		ret = f_fill_urb(str_data->urb_tx_list[i], ep_num
			, data_len, 0, EPADD_OUT, iso_packet_num, maxp, udev);
		if(ret){
			xhci_err(xhci, "[ERROR]fill tx urb Error!!\n");
			return RET_FAIL;
		}
		memcpy(str_data->urb_tx_list[i]->transfer_buffer, str_data->buffer[i], data_len);
		dma_sync_single_for_device(dev, str_data->urb_tx_list[i]->transfer_dma
			, data_len, DMA_BIDIRECTIONAL);
		str_data->urb_tx_list[i]->status = URB_STATUS_IDLE;
//		xhci_err(xhci, "[STRESS] URB_TX %d -- 0x%x\n", i, str_data->urb_tx_list[i]);
	}

	for(i=0; i<TOTAL_URB; i++){
		str_data->urb_rx_list[i] = usb_alloc_urb(iso_packet_num, GFP_NOIO);
		ret = f_fill_urb_with_buffer(str_data->urb_rx_list[i]
			,ep_num, data_len, str_data->urb_tx_list[i]->transfer_buffer
			, 0, EPADD_IN, iso_packet_num, maxp, str_data->urb_tx_list[i]->transfer_dma, udev);
		str_data->urb_rx_list[i]->status = -EINPROGRESS;
//		xhci_err(xhci, "[STRESS] URB_RX %d -- 0x%x\n", i, str_data->urb_rx_list[i]);
	}
	kthread_run(stress_rdn_len_tx_thread, str_data, "stresstxt");
	kthread_run(stress_rdn_len_tx_done_thread, str_data, "stresstxdt");
	kthread_run(stress_rdn_len_rx_done_thread, str_data, "stress_rxdt");
	if(isEP0){
		kthread_run(stress_ep0_thread, str_data, "stress_ep0");
	}
}

int f_add_str_threads(int dev_num, int ep_num, int maxp, char isCompare, struct usb_device *usbdev, char isEP0){
	int ret,i,j;
	struct device	*dev;
	struct xhci_hcd *xhci;
	struct usb_device *udev, *rhdev;
	struct stress_data *str_data;
	int data_len;
	dma_addr_t mapping;
	int iso_packet_num;
	struct usb_host_endpoint *ep;
	u8 *tmp;

	xhci = hcd_to_xhci(my_hcd);
	dev = xhci_to_hcd(xhci)->self.controller;
	if(!usbdev){
		rhdev = my_hcd->self.root_hub;
		udev = rhdev->children[g_port_id-1];
	}
	else{
		udev = usbdev;
	}

	ep = udev->ep_out[ep_num];

	if(usb_endpoint_xfer_isoc(&ep->desc)){
		iso_packet_num = ((GPD_LENGTH+maxp)/maxp);
	}
	else{
		iso_packet_num = 0;
	}
	xhci_dbg(xhci, "[STRESS DBG]dev slot id %d\n", udev->slot_id);
	xhci_err(xhci, "[STRESS]Start stress process, dev_num %d, ep_num %d\n", dev_num, ep_num);
	str_data = kzalloc(sizeof(struct stress_data), GFP_KERNEL);
	str_data->xhci = xhci;
	str_data->udev = udev;
	str_data->dev_num = dev_num;
	str_data->ep_num = ep_num;
	xhci_err(xhci, "[STRESS] str_data address 0x%p\n", str_data);
	str_data->isCompare = isCompare;

	for(i=0; i<TOTAL_URB; i++){
		str_data->loop_count[i] = 0;
	}

	data_len = GPD_LENGTH;
	for(i=0; i<TOTAL_URB; i++){
		str_data->buffer[i] = kmalloc(data_len, GFP_KERNEL);
		//get_random_bytes(str_data->buffer[i], data_len);
		for(j=0; j<data_len; j++){
			tmp = str_data->buffer[i]+j;
			if((j%1024)==0){
				*tmp = (u8)i&0xff;
			}
			else if(j%1024==1){
				*tmp = (u8)(j/1024)&0xff;
			}
			else{
				*tmp = (u8)((j+i)&0xff);
			}
		}
	}
//	xhci_err(xhci, "iso_packet_num %d\n", iso_packet_num);
	for(i=0; i<TOTAL_URB; i++){
		str_data->urb_tx_list[i] = usb_alloc_urb(iso_packet_num, GFP_NOIO);
		ret = f_fill_urb(str_data->urb_tx_list[i], ep_num
			, data_len, 0, EPADD_OUT, iso_packet_num, maxp, udev);
		if(ret){
			xhci_err(xhci, "[ERROR]fill tx urb Error!!\n");
			return RET_FAIL;
		}
		memcpy(str_data->urb_tx_list[i]->transfer_buffer, str_data->buffer[i], data_len);
		dma_sync_single_for_device(dev, str_data->urb_tx_list[i]->transfer_dma
			, data_len, DMA_BIDIRECTIONAL);
		str_data->urb_tx_list[i]->status = URB_STATUS_IDLE;
//		xhci_err(xhci, "[STRESS] URB_TX %d -- 0x%x\n", i, str_data->urb_tx_list[i]);
	}

	for(i=0; i<TOTAL_URB; i++){
		str_data->urb_rx_list[i] = usb_alloc_urb(iso_packet_num, GFP_NOIO);
		ret = f_fill_urb_with_buffer(str_data->urb_rx_list[i]
			,ep_num, data_len, str_data->urb_tx_list[i]->transfer_buffer
			, 0, EPADD_IN, iso_packet_num, maxp, str_data->urb_tx_list[i]->transfer_dma, udev);
		str_data->urb_rx_list[i]->status = -EINPROGRESS;
//		xhci_err(xhci, "[STRESS] URB_RX %d -- 0x%x\n", i, str_data->urb_rx_list[i]);
	}
	kthread_run(stress_tx_thread, str_data, "stresstxt");
	kthread_run(stress_tx_done_thread, str_data, "stresstxdt");
	kthread_run(stress_rx_done_thread, str_data, "stress_rxdt");
	if(isEP0){
		kthread_run(stress_ep0_thread, str_data, "stress_ep0");
	}
}

int otg_dev_A_host_thread(void *data){
	u32 temp;
	printk(KERN_ERR "[OTG_H] enter u3h_dev_A_host thread\n");
	if(f_test_lib_init() != RET_SUCCESS){
		printk(KERN_ERR "[OTG_H][FAIL] controller driver init failed\n");
		return RET_FAIL;
	}
	//emulate device attach
	writel(SSUSB_ATTACH_A_ROLE, SSUSB_OTG_STS);
#if 1
	temp = readl(SSUSB_U2_CTRL(0));
	temp = temp | SSUSB_U2_PORT_HOST_SEL;// | SSUSB_U2_PORT_OTG_HOST_VBUSVALID_SEL;
	writel(temp, SSUSB_U2_CTRL(0));
#endif
	if(f_enable_port(0) != RET_SUCCESS){
		printk(KERN_ERR "[OTG_H][FAIL] Device attach timeout\n");
		return RET_FAIL;
	}
	printk(KERN_ERR "[OTG_H] Device attached\n");
	if(f_enable_slot(NULL) != RET_SUCCESS){
		printk(KERN_ERR "[OTG_H][FAIL] Enable slot failed\n");
		return RET_FAIL;
	}
	if(f_address_slot(false, NULL) != RET_SUCCESS){
		printk(KERN_ERR "[OTG_H][FAIL] Address device failed\n");
		return RET_FAIL;
	}
	printk(KERN_ERR "[OTG_H] exit u3h_dev_A_host thread\n");
}

int otg_dev_B_hnp(void *data){
	printk(KERN_ERR "[OTG_H] enter u3h_otg_dev_B_hnp thread\n");
	writel((readl(SSUSB_U2_SYS_BASE+0xc)|0x100), SSUSB_U2_SYS_BASE+0xc);
	writel((readl(SSUSB_U3_XHCI_BASE + 0x9b4) & ~(0x10000)), SSUSB_U3_XHCI_BASE + 0x9b4);
	//wait to become host
	wait_event_on_timeout(&g_otg_hnp_become_host, true, TRANS_TIMEOUT);
	if(!g_otg_hnp_become_host){
		printk(KERN_ERR "[OTG_H][FAIL] doesn't become host\n");
		return RET_FAIL;
	}
	printk(KERN_ERR "[OTG_H] HNP become host\n");
	if(f_enable_port(0) != RET_SUCCESS){
		printk(KERN_ERR "[OTG_H][FAIL] Device attach timeout\n");
		return RET_FAIL;
	}
	printk(KERN_ERR "[OTG_H] Device attached\n");
	if(f_enable_slot(NULL) != RET_SUCCESS){
		printk(KERN_ERR "[OTG_H][FAIL] Enable slot failed\n");
		return RET_FAIL;
	}
	if(f_address_slot(false, NULL) != RET_SUCCESS){
		printk(KERN_ERR "[OTG_H][FAIL] Address device failed\n");
		return RET_FAIL;
	}
	printk(KERN_ERR "[OTG_H] exit u3h_otg_dev_B_hnp\n");
}

int otg_dev_A_hnp(void *data){
	int ret;
	u32 temp;
	u32 __iomem *addr;
	int port_id;
	struct xhci_hcd *xhci;

	printk(KERN_ERR "[OTG_H] enter u3h_otg_dev_A_hnp\n");
	g_otg_hnp_become_dev = false;
	//suspend port
	port_id = 1;
	xhci = hcd_to_xhci(my_hcd);
	addr = &xhci->op_regs->port_status_base + NUM_PORT_REGS*(port_id-1 & 0xff);
	temp = xhci_readl(xhci, addr);
	xhci_dbg(xhci, "before reset port %d = 0x%x\n", port_id-1, temp);
	temp = mtktest_xhci_port_state_to_neutral(temp);
	temp = (temp & ~(0xf << 5));
	temp = (temp | (3 << 5) | PORT_LINK_STROBE);
	xhci_writel(xhci, temp, addr);
#if 0
	//polling dev_mode
	temp = readl(SSUSB_OTG_STS);
	while((temp & SSUSB_HOST_DEV_MODE) == SSUSB_HOST_DEV_MODE){
		msleep(1);
		temp = readl(SSUSB_OTG_STS);
	}
#endif
	wait_event_on_timeout(&g_otg_hnp_become_dev, true, TRANS_TIMEOUT);
	printk(KERN_ERR "[OTG_H] device A become device\n");
	ret = f_disable_slot();
	if(ret){
		printk(KERN_ERR "[OTG_H][FAIL] disable slot failed!!!!!!!!!!\n");
		return ret;
	}
	//alert device
	writel(SSUSB_CHG_B_ROLE_B, SSUSB_OTG_STS);
	printk(KERN_ERR "[OTG_H] exit u3h_otg_dev_A_hnp\n");
}

int otg_dev_A_hnp_back(void *data){
	int ret;
	printk(KERN_ERR "[OTG_H] enter u3h_otg_dev_A_hnp_back\n");
	wait_event_on_timeout(&g_otg_hnp_become_host, true, TRANS_TIMEOUT);
	printk(KERN_ERR "[OTG_H] device_A become host\n");
	if(f_enable_port(0) != RET_SUCCESS){
		printk(KERN_ERR "[OTG_H][FAIL] Device attach timeout\n");
		return RET_FAIL;
	}
	printk(KERN_ERR "[OTG_H] Device attached\n");
	if(f_enable_slot(NULL) != RET_SUCCESS){
		printk(KERN_ERR "[OTG_H][FAIL] Enable slot failed\n");
		return RET_FAIL;
	}
	if(f_address_slot(false, NULL) != RET_SUCCESS){
		printk(KERN_ERR "[OTG_H][FAIL] Address device failed\n");
		return RET_FAIL;
	}
	printk(KERN_ERR "[OTG_H] exit u3h_otg_dev_A_hnp_back\n");
}

int otg_dev_B_hnp_back(void *data){
	int ret;
	u32 temp;
	u32 __iomem *addr;
	int port_id;
	struct xhci_hcd *xhci;

	printk(KERN_ERR "[OTG_H] enter u3h_otg_dev_B_hnp_back\n");
	//suspend port
	port_id = 1;
	xhci = hcd_to_xhci(my_hcd);
	addr = &xhci->op_regs->port_status_base + NUM_PORT_REGS*(port_id-1 & 0xff);
	temp = xhci_readl(xhci, addr);
	xhci_dbg(xhci, "before reset port %d = 0x%x\n", port_id-1, temp);
	temp = mtktest_xhci_port_state_to_neutral(temp);
	temp = (temp & ~(0xf << 5));
	temp = (temp | (3 << 5) | PORT_LINK_STROBE);
	xhci_writel(xhci, temp, addr);

	//polling dev_mode
	wait_event_on_timeout(&g_otg_hnp_become_dev, true, TRANS_TIMEOUT);
	if(g_otg_hnp_become_dev){
	}
	else{
		printk(KERN_ERR "[OTG_H][FAIL] doesn't get B_ROLE_A interrupt\n");
		return RET_FAIL;
	}
#if 0
	temp = readl(SSUSB_OTG_STS);
	while((temp & SSUSB_HOST_DEV_MODE) == SSUSB_HOST_DEV_MODE){
		msleep(1);
		temp = readl(SSUSB_OTG_STS);
	}
#endif
	printk(KERN_ERR "[OTG_H] device B become device\n");
	ret = f_disable_slot();
	if(ret){
		printk(KERN_ERR "[OTG_H][FAIL] disable slot failed!!!!!!!!!!\n");
		return ret;
	}
	//set B_role_B
	writel(SSUSB_CHG_B_ROLE_B, SSUSB_OTG_STS);
	printk(KERN_ERR "[OTG_H] exit u3h_otg_dev_B_hnp_back\n");
}

int otg_dev_A_srp(void *data){
	int ret;
	u32 temp;
	u32 __iomem *addr;
	struct xhci_hcd *xhci;
	int port_id;

	printk(KERN_ERR "[OTG_H] enter u3h_otg_dev_A_srp thread\n");
	xhci = hcd_to_xhci(my_hcd);
	if(!g_port_reset){
		printk(KERN_ERR "[OTG_H][FAIL] device not reset\n");
		return RET_FAIL;
	}
	if(g_slot_id == 0){
		printk(KERN_ERR "[OTG_H][FAIL] slot not enabled\n");
		return RET_FAIL;
	}
	//start_port_reenabled(0, DEV_SPEED_HIGH);
	ret = f_disable_slot();
	if(ret){
		printk(KERN_ERR "[OTG_H][FAIL] disable slot failed!!!!!!!!!!\n");
		return ret;
	}
	struct xhci_port *port = rh_port[0];
	port->port_status = DISCONNECTED;

	//turn vbus off
	port_id = 1;
	addr = &xhci->op_regs->port_status_base + NUM_PORT_REGS*(port_id-1 & 0xff);
	temp = xhci_readl(xhci, addr);
	temp = mtktest_xhci_port_state_to_neutral(temp);
	temp &= ~PORT_POWER;
	xhci_writel(xhci, temp, addr);
	//ret = f_reenable_port(0);
	ret = f_enable_port(0);
	if(ret){
		printk(KERN_ERR "[OTG_H][FAIL]  port reenable failed\n");
		return RET_FAIL;
	}
	ret = f_enable_slot(NULL);
	if(ret){
		printk(KERN_ERR "[OTG_H][FAIL] enable slot failed!!!!!!!!!!\n");
		return ret;
	}

	ret=f_address_slot(false, NULL);
	if(ret){
		printk(KERN_ERR "[OTG_H][FAIL] address slot failed!!!!!!!!!!\n");
		return ret;
	}
	//reset SW scheduler algorithm
	mtktest_mtk_xhci_scheduler_init();

	printk(KERN_ERR "[OTG_H] exit u3h_otg_dev_A_srp thread\n");
}

// USBIF

/*
	@return 
	2 : defined in interface class
	1 : support
	0 : not support
*/


static int is_targeted(struct usb_device_descriptor *desc)
{	
#define USB_CLASS_ID_HID			3
#define USB_CLASS_ID_MASS_STORAGE		8
#define USB_CLASS_ID_HUB			9
	/*  list the RXXE TPL here */

	/* HNP test device is _never_ targeted (see OTG spec 6.6.6) */
	if ((le16_to_cpu(desc->idVendor) == 0x1a0a &&
	     le16_to_cpu(desc->idProduct) == 0xbadd))
		return 0;	

	// PET
	if ((le16_to_cpu(desc->idVendor) == 0x1A0A &&
	     le16_to_cpu(desc->idProduct) == 0x0200)){
		return 1;
	}

	// PET : unsupport test
	if ((le16_to_cpu(desc->idVendor) == 0x1A0A &&
	     le16_to_cpu(desc->idProduct) == 0x0201)){
		return 0;
	}

	// PET : unsupport test
	if ((le16_to_cpu(desc->idVendor) == 0x1A0A &&
	     le16_to_cpu(desc->idProduct) == 0x0202)){
		return 0;
	}

	// HUB
	/*
	if (le16_to_cpu(desc->bDeviceClass) == USB_CLASS_ID_HUB &&
		le16_to_cpu(desc->bDeviceSubClass) == 0 &&
		le16_to_cpu(desc->bDeviceProtocol) == 0){
		return 1 ;
	}

	// HUB
	if (le16_to_cpu(desc->bDeviceClass) == USB_CLASS_ID_HUB &&
		le16_to_cpu(desc->bDeviceSubClass) == 0 &&
		le16_to_cpu(desc->bDeviceProtocol) == 1){
		return 1 ;
	}
	*/
	// HID
	if (le16_to_cpu(desc->bDeviceClass) == USB_CLASS_ID_HID &&
		le16_to_cpu(desc->bDeviceSubClass) == 0 &&
		le16_to_cpu(desc->bDeviceProtocol) == 0){
		return 1 ;
	}

	// STORAGE
	if (le16_to_cpu(desc->bDeviceClass) == USB_CLASS_ID_MASS_STORAGE&&
		le16_to_cpu(desc->bDeviceSubClass) == 0 &&
		le16_to_cpu(desc->bDeviceProtocol) == 0){
		return 1 ;
	}	

	// Defined in Interface Configuration
	if (le16_to_cpu(desc->bDeviceClass) == 0&&
		le16_to_cpu(desc->bDeviceSubClass) == 0 &&
		le16_to_cpu(desc->bDeviceProtocol) == 0){
		return 2 ;
	}		

	return 0 ;
}

int f_getdescriptor(){
	int ret;
	struct usb_device *udev, *rhdev;
	struct xhci_virt_device *virt_dev;
	struct xhci_hcd *xhci;
	struct urb *urb;
	struct usb_ctrlrequest *dr;
	struct usb_device_descriptor *desc;
	int i;
	char *tmp;
	ret = 0;

	printk(KERN_ERR "[OTG_H] ===> f_getdescriptor\n");
	
	if(g_slot_id == 0){
		printk(KERN_ERR "[ERROR] slot ID not valid\n");
		return RET_FAIL;
	}

	xhci = hcd_to_xhci(my_hcd);
	rhdev = my_hcd->self.root_hub;
	udev = rhdev->children[g_port_id-1];

	dr = kmalloc(sizeof(struct usb_ctrlrequest), GFP_NOIO);
	dr->bRequestType = USB_DIR_IN;
	dr->bRequest = USB_REQ_GET_DESCRIPTOR;
	dr->wValue = cpu_to_le16((USB_DT_DEVICE << 8) + 0);
	dr->wIndex = cpu_to_le16(0);
	dr->wLength = cpu_to_le16(USB_DT_DEVICE_SIZE);
	desc = kmalloc(USB_DT_DEVICE_SIZE, GFP_KERNEL);
	memset(desc, 0, USB_DT_DEVICE_SIZE);
	
	urb = alloc_ctrl_urb(dr, desc, udev);
	ret = f_ctrlrequest(urb, udev);
	if (ret == RET_SUCCESS){
		printk(KERN_ERR "[OTG_H] -----------------------desc->idProduct: 0x%x-------------------\n", desc->idProduct);
		printk(KERN_ERR "[OTG_H] -----------------------desc->idVendor: 0x%x-------------------\n", desc->idVendor);
		printk(KERN_ERR "[OTG_H] -----------------------desc->bDeviceClass: 0x%x-------------------\n", desc->bDeviceClass);
		printk(KERN_ERR "[OTG_H] -----------------------desc->bDeviceSubClass: 0x%x-------------------\n", desc->bDeviceSubClass);
		printk(KERN_ERR "[OTG_H] -----------------------desc->bDeviceProtocol: 0x%x-------------------\n", desc->bDeviceProtocol);
		printk(KERN_ERR "[OTG_H] -----------------------desc->bcdDevice: 0x%x-------------------\n", desc->bcdDevice);
		printk(KERN_ERR "[OTG_H] -----------------------desc->bNumConfigurations: 0x%x-------------------\n", desc->bNumConfigurations);
		
		if(desc->idProduct == 0x1234){
			printk(KERN_ERR "[OTG_H] ===================Device not support OTG====================\n");
		}
		if(desc->idProduct == 0x0201){
			printk(KERN_ERR "[OTG_H] ==============Unsupported device=====================\n");
			g_otg_unsupported_dev = true;
		}

		if (is_targeted(desc) == 0){
			printk(KERN_ERR "[OTG_H] ============== not support device=====================\n");
			usbif_u3h_test_send_event(USBIF_OTG_EVENT_DEV_NOT_SUPPORTED);  
		}	
	}else{
		printk(KERN_ERR "[OTG_H] [ERROR] f_getdescriptor failed\n");
	}

	kfree(dr);
	kfree(desc);			
	usb_free_urb(urb);
//	kfree(urb);
	printk(KERN_ERR "[OTG_H] <=== f_getdescriptor, ret = %d\n", ret);
	return ret;
}

int f_getqualifierdescriptor(){
	int ret;
	struct usb_device *udev, *rhdev;
	struct xhci_virt_device *virt_dev;
	struct xhci_hcd *xhci;
	struct urb *urb;
	struct usb_ctrlrequest *dr;
	struct usb_device_descriptor *desc;
	int i;
	char *tmp;
	ret = 0;

	printk(KERN_ERR "[OTG_H] ===> f_getqualifierdescriptor\n");
	
	if(g_slot_id == 0){
		printk(KERN_ERR "[ERROR] slot ID not valid\n");
		return RET_FAIL;
	}

	xhci = hcd_to_xhci(my_hcd);
	rhdev = my_hcd->self.root_hub;
	udev = rhdev->children[g_port_id-1];

	dr = kmalloc(sizeof(struct usb_ctrlrequest), GFP_NOIO);
	dr->bRequestType = USB_DIR_IN;
	dr->bRequest = USB_REQ_GET_DESCRIPTOR;
	dr->wValue = cpu_to_le16((USB_DT_DEVICE_QUALIFIER << 8) + 0);
	dr->wIndex = cpu_to_le16(0);
	dr->wLength = cpu_to_le16(10);
	desc = kmalloc(10, GFP_KERNEL);
	memset(desc, 0, 10);
	
	urb = alloc_ctrl_urb(dr, desc, udev);
	ret = f_ctrlrequest(urb, udev);
	kfree(dr);
	kfree(desc);
	usb_free_urb(urb);
//	kfree(urb);

	printk(KERN_ERR "[OTG_H] <=== f_getqualifierdescriptor, ret = %d\n", ret);
	
	return ret;
}

int f_getconfiguration_desc(){
	int ret;
	int config_len;
	struct usb_device *udev, *rhdev;
	struct xhci_virt_device *virt_dev;
	struct xhci_hcd *xhci;
	struct urb *urb;
	struct usb_ctrlrequest *dr;
	struct usb_config_descriptor *desc;
	int i;
	char *tmp;
	ret = 0;

	printk(KERN_ERR "[OTG_H] ===> f_getconfiguration_desc\n");
	
	if(g_slot_id == 0){
		printk(KERN_ERR "[ERROR] slot ID not valid\n");
		return RET_FAIL;
	}

	xhci = hcd_to_xhci(my_hcd);
	rhdev = my_hcd->self.root_hub;
	udev = rhdev->children[g_port_id-1];

	dr = kmalloc(sizeof(struct usb_ctrlrequest), GFP_NOIO);
	dr->bRequestType = USB_DIR_IN;
	dr->bRequest = USB_REQ_GET_DESCRIPTOR;
	dr->wValue = cpu_to_le16((USB_DT_CONFIG << 8) + 0);
	dr->wIndex = cpu_to_le16(0);
	dr->wLength = cpu_to_le16(USB_DT_CONFIG_SIZE);
	desc = kmalloc(USB_DT_CONFIG_SIZE, GFP_KERNEL);
	memset(desc, 0, USB_DT_CONFIG_SIZE);
	
	urb = alloc_ctrl_urb(dr, desc, udev);
	ret = f_ctrlrequest(urb, udev);
	config_len = desc->wTotalLength;
	kfree(dr);
	kfree(desc);
	usb_free_urb(urb);
//	kfree(urb);
	g_otg_dev_conf_len = config_len;
	//get all configuration
	dr = kmalloc(sizeof(struct usb_ctrlrequest), GFP_NOIO);
	dr->bRequestType = USB_DIR_IN;
	dr->bRequest = USB_REQ_GET_DESCRIPTOR;
	dr->wValue = cpu_to_le16((USB_DT_CONFIG << 8) + 0);
	dr->wIndex = cpu_to_le16(0);
	dr->wLength = cpu_to_le16(config_len);
	desc = kmalloc(config_len, GFP_KERNEL);
	memset(desc, 0, config_len);
	
	urb = alloc_ctrl_urb(dr, desc, udev);
	ret = f_ctrlrequest(urb, udev);

	if (ret == RET_SUCCESS){
		printk(KERN_ERR "[OTG_H] -----------------------desc->bDescriptorType: 0x%x-------------------\n", desc->bDescriptorType);
		printk(KERN_ERR "[OTG_H] -----------------------desc->wTotalLength: 0x%x-------------------\n", desc->wTotalLength);
		printk(KERN_ERR "[OTG_H] -----------------------desc->bNumInterfaces: 0x%x-------------------\n", desc->bNumInterfaces);
		printk(KERN_ERR "[OTG_H] -----------------------desc->bmAttributes: 0x%x-------------------\n", desc->bmAttributes);
		printk(KERN_ERR "[OTG_H] -----------------------desc->bMaxPower: 0x%x-------------------\n", desc->bMaxPower);
	}else{
		printk(KERN_ERR "[OTG_H] [ERROR] f_getconfiguration_desc failed\n");
	}
	
	kfree(dr);
	kfree(desc);
	usb_free_urb(urb);

	printk(KERN_ERR "[OTG_H] <=== f_getconfiguration_desc, ret = %dr\n", ret);
	return ret;
}


int f_interface_desc(){
	int ret;
	int config_len;
	struct usb_device *udev, *rhdev;
	struct xhci_virt_device *virt_dev;
	struct xhci_hcd *xhci;
	struct urb *urb;
	struct usb_ctrlrequest *dr;
	struct usb_config_descriptor *desc;
	int i;
	char *tmp;
	ret = 0;

	if(g_slot_id == 0){
		printk(KERN_ERR "[ERROR] slot ID not valid\n");
		return RET_FAIL;
	}

	xhci = hcd_to_xhci(my_hcd);
	rhdev = my_hcd->self.root_hub;
	udev = rhdev->children[g_port_id-1];

/*
	dr = kmalloc(sizeof(struct usb_ctrlrequest), GFP_NOIO);
	dr->bRequestType = USB_DIR_IN;
	dr->bRequest = USB_REQ_GET_DESCRIPTOR;
	dr->wValue = cpu_to_le16((USB_DT_CONFIG << 8) + 0);
	dr->wIndex = cpu_to_le16(0);
	dr->wLength = cpu_to_le16(USB_DT_CONFIG_SIZE);
	desc = kmalloc(USB_DT_CONFIG_SIZE, GFP_KERNEL);
	memset(desc, 0, USB_DT_CONFIG_SIZE);
	
	urb = alloc_ctrl_urb(dr, desc, udev);
	ret = f_ctrlrequest(urb, udev);
	config_len = desc->wTotalLength;
	kfree(dr);
	kfree(desc);
	usb_free_urb(urb);
//	kfree(urb);
	
	//get all configuration
	dr = kmalloc(sizeof(struct usb_ctrlrequest), GFP_NOIO);
	dr->bRequestType = USB_DIR_IN;
	dr->bRequest = USB_REQ_GET_DESCRIPTOR;
	dr->wValue = cpu_to_le16((USB_DT_CONFIG << 8) + 0);
	dr->wIndex = cpu_to_le16(0);
	dr->wLength = cpu_to_le16(config_len);
	desc = kmalloc(config_len, GFP_KERNEL);
	memset(desc, 0, config_len);
	
	urb = alloc_ctrl_urb(dr, desc, udev);
	ret = f_ctrlrequest(urb, udev);
	kfree(dr);
	kfree(desc);
	usb_free_urb(urb);
*/

	return ret;
}
int f_get_string_desc(){
	int ret;
	int config_len;
	struct usb_device *udev, *rhdev;
	struct xhci_virt_device *virt_dev;
	struct xhci_hcd *xhci;
	struct urb *urb;
	struct usb_ctrlrequest *dr;
	struct usb_string_descriptor *desc;
	int i;
	char *tmp;
	ret = 0;

	printk(KERN_ERR "[OTG_H] ===> f_get_string_desc\n");
	
	if(g_slot_id == 0){
		printk(KERN_ERR "[ERROR] slot ID not valid\n");
		return RET_FAIL;
	}

	xhci = hcd_to_xhci(my_hcd);
	rhdev = my_hcd->self.root_hub;
	udev = rhdev->children[g_port_id-1];

	dr = kmalloc(sizeof(struct usb_ctrlrequest), GFP_NOIO);
	dr->bRequestType = USB_DIR_IN;
	dr->bRequest = USB_REQ_GET_DESCRIPTOR;
	dr->wValue = cpu_to_le16((USB_DT_STRING << 8) + 0);
	dr->wIndex = cpu_to_le16(0);
	dr->wLength = cpu_to_le16(255);
	desc = kmalloc(255, GFP_KERNEL);
	memset(desc, 0, 255);
	
	urb = alloc_ctrl_urb(dr, (char *)desc, udev);
	ret = f_ctrlrequest(urb, udev);
	kfree(dr);
	kfree(desc);
	usb_free_urb(urb);

	printk(KERN_ERR "[OTG_H] <=== f_get_string_desc 0 , ret = %d\n", ret);
	
	//get string 2
	dr = kmalloc(sizeof(struct usb_ctrlrequest), GFP_NOIO);
	dr->bRequestType = USB_DIR_IN;
	dr->bRequest = USB_REQ_GET_DESCRIPTOR;
	dr->wValue = cpu_to_le16((USB_DT_STRING << 8) + 2);
	dr->wIndex = cpu_to_le16(0x409);
	dr->wLength = cpu_to_le16(255);
	desc = kmalloc(255, GFP_KERNEL);
	memset(desc, 0, 255);
	
	urb = alloc_ctrl_urb(dr, desc, udev);
	ret = f_ctrlrequest(urb, udev);
	kfree(dr);
	kfree(desc);
	usb_free_urb(urb);


	
	printk(KERN_ERR "[OTG_H] <=== f_get_string_desc 2 , ret = %d\n", ret);

	//get string 1
	dr = kmalloc(sizeof(struct usb_ctrlrequest), GFP_NOIO);
	dr->bRequestType = USB_DIR_IN;
	dr->bRequest = USB_REQ_GET_DESCRIPTOR;
	dr->wValue = cpu_to_le16((USB_DT_STRING << 8) + 1);
	dr->wIndex = cpu_to_le16(0x409);
	dr->wLength = cpu_to_le16(255);
	desc = kmalloc(255, GFP_KERNEL);
	memset(desc, 0, 255);
	
	urb = alloc_ctrl_urb(dr, desc, udev);
	ret = f_ctrlrequest(urb, udev);
	kfree(dr);
	kfree(desc);
	usb_free_urb(urb);
	printk(KERN_ERR "[OTG_H] <=== f_get_string_desc 1 , ret = %d\n", ret);
	
	return ret ;
}

int f_set_hnp(){
	int ret;
	struct usb_device *udev, *rhdev;
	struct xhci_virt_device *virt_dev;
	struct xhci_hcd *xhci;
	struct urb *urb;
	struct usb_ctrlrequest *dr;
	int i;
	char *tmp;
	char isConfigMouse;
	ret = 0;

	isConfigMouse = false;
	
	if(g_slot_id == 0){
		printk(KERN_ERR "[ERROR] slot ID not valid\n");
		return RET_FAIL;
	}

	xhci = hcd_to_xhci(my_hcd);
	rhdev = my_hcd->self.root_hub;
	udev = rhdev->children[g_port_id-1];

	//set hnp
	dr = kmalloc(sizeof(struct usb_ctrlrequest), GFP_NOIO);
	dr->bRequestType = USB_DIR_OUT;
	dr->bRequest = USB_REQ_SET_FEATURE;
	dr->wValue = cpu_to_le16(3); //b_hnp_enable
	dr->wIndex = cpu_to_le16(0);
	dr->wLength = cpu_to_le16(0);
	urb = alloc_ctrl_urb(dr, NULL, udev);
	ret = f_ctrlrequest(urb,udev);
	kfree(dr);
	usb_free_urb(urb);

	if(ret)
	{
		printk(KERN_ERR "[OTG_H] f_set_hnp Fail!\n");
		// USBIF , show "Device No Response" 
		usbif_u3h_test_send_event(USBIF_OTG_EVENT_NO_RESP_FOR_HNP_ENABLE);
		return RET_FAIL;
	}
	
//	kfree(urb);
	return ret;
}

int f_set_a_hnp(){
	int ret;
	struct usb_device *udev, *rhdev;
	struct xhci_virt_device *virt_dev;
	struct xhci_hcd *xhci;
	struct urb *urb;
	struct usb_ctrlrequest *dr;
	int i;
	char *tmp;
	char isConfigMouse;
	ret = 0;

	isConfigMouse = false;
	
	if(g_slot_id == 0){
		printk(KERN_ERR "[ERROR] slot ID not valid\n");
		return RET_FAIL;
	}

	xhci = hcd_to_xhci(my_hcd);
	rhdev = my_hcd->self.root_hub;
	udev = rhdev->children[g_port_id-1];

	//set hnp
	dr = kmalloc(sizeof(struct usb_ctrlrequest), GFP_NOIO);
	dr->bRequestType = USB_DIR_OUT;
	dr->bRequest = USB_REQ_SET_FEATURE;
	dr->wValue = cpu_to_le16(4); //b_hnp_enable
	dr->wIndex = cpu_to_le16(0);
	dr->wLength = cpu_to_le16(0);
	urb = alloc_ctrl_urb(dr, NULL, udev);
	ret = f_ctrlrequest(urb,udev);
	if (ret == RET_SUCCESS){
		printk(KERN_ERR "[OTG_H] f_set_a_hnp OK\n");
	}else{
		printk(KERN_ERR "[OTG_H] [ERROR] f_set_a_hnp failed\n");
	}
	kfree(dr);
	usb_free_urb(urb);
	
//	kfree(urb);
	return ret;
}

int f_set_configuration(){
	int ret;
	struct usb_device *udev, *rhdev;
	struct xhci_virt_device *virt_dev;
	struct xhci_hcd *xhci;
	struct urb *urb;
	struct usb_ctrlrequest *dr;
	int i;
	char *tmp;
	char isConfigMouse;
	ret = 0;

	isConfigMouse = false;
	
	if(g_slot_id == 0){
		printk(KERN_ERR "[ERROR] slot ID not valid\n");
		return RET_FAIL;
	}

	xhci = hcd_to_xhci(my_hcd);
	rhdev = my_hcd->self.root_hub;
	udev = rhdev->children[g_port_id-1];

	//set hnp
	dr = kmalloc(sizeof(struct usb_ctrlrequest), GFP_NOIO);
	dr->bRequestType = USB_DIR_OUT;
	dr->bRequest = USB_REQ_SET_CONFIGURATION;
	dr->wValue = cpu_to_le16(0); //b_hnp_enable 1 if opt, pet use 0 , configuration value 1 or 0 ?
	dr->wIndex = cpu_to_le16(0);
	dr->wLength = cpu_to_le16(0);
	urb = alloc_ctrl_urb(dr, NULL, udev);
	ret = f_ctrlrequest(urb,udev);
	if (ret == RET_SUCCESS){
		printk(KERN_ERR "[OTG_H] f_ctrlrequest OK\n");
	}else{
		printk(KERN_ERR "[OTG_H] [ERROR] f_ctrlrequest failed\n");
	}
	
	kfree(dr);
	usb_free_urb(urb);
	
//	kfree(urb);
	return ret;
}

int f_get_otg_status(){
	int ret;
	int config_len;
	struct usb_device *udev, *rhdev;
	struct xhci_virt_device *virt_dev;
	struct xhci_hcd *xhci;
	struct urb *urb;
	struct usb_ctrlrequest *dr;
	u8 *desc;
	int i;
	ret = 0;

	if(g_slot_id == 0){
		printk(KERN_ERR "[ERROR] slot ID not valid\n");
		return RET_FAIL;
	}

	xhci = hcd_to_xhci(my_hcd);
	rhdev = my_hcd->self.root_hub;
	udev = rhdev->children[g_port_id-1];

	dr = kmalloc(sizeof(struct usb_ctrlrequest), GFP_NOIO);
	dr->bRequestType = USB_DIR_IN;
	dr->bRequest = USB_REQ_GET_STATUS;
	dr->wValue = cpu_to_le16(0);
	dr->wIndex = cpu_to_le16(0xf000);
	dr->wLength = cpu_to_le16(1);
	desc = kmalloc(1, GFP_KERNEL);
	*desc = 0;
	//desc = 0;
//	printk("[OTG_H] before get OTG status, addr 0x%x content=%d\n", desc, *desc);
	urb = alloc_ctrl_urb(dr, desc, udev);
	ret = f_ctrlrequest(urb, udev);
//	printk("[OTG_H] get OTG status, addr 0x%x content=%d\n", desc, *desc);
	ret = *desc;
	kfree(dr);
	kfree(desc);
	usb_free_urb(urb);
	return ret;
}

static const u8 musb_host_test_packet[53] = {
	/* implicit SYNC then DATA0 to start */

	/* JKJKJKJK x9 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* JJKKJJKK x8 */
	0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
	/* JJJJKKKK x8 */
	0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee,
	/* JJJJJJJKKKKKKK x8 */
	0xfe, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	/* JJJJJJJK x8 */
	0x7f, 0xbf, 0xdf, 0xef, 0xf7, 0xfb, 0xfd,
	/* JKKKKKKK x10, JK */
	0xfc, 0x7e, 0xbf, 0xdf, 0xef, 0xf7, 0xfb, 0xfd, 0x7e

	/* implicit CRC16 then EOP to end */
};

u32 f_load_to_fifo(u32 ep_num,u32 length,u8 *buf,u32 maxp){

	u32 residue;
	u32 count;
	u32 temp;

	count = residue = length;

	while(residue > 0) {

		if(residue==1) {
			temp=((*buf)&0xFF);
			writeb(USB_FIFO(ep_num), temp);
			buf += 1;
			residue -= 1;
		} else if(residue==2) {
			temp=((*buf)&0xFF)+(((*(buf+1))<<8)&0xFF00);
			writeb(USB_FIFO(ep_num), temp);
			buf += 2;
			residue -= 2;
		} else if(residue==3) {
			temp=((*buf)&0xFF)+(((*(buf+1))<<8)&0xFF00);
			writeb(USB_FIFO(ep_num), temp);
			buf += 2;

			temp=((*buf)&0xFF);
			writeb(USB_FIFO(ep_num), temp);
			buf += 1;
			residue -= 3;
		} else {
			temp=((*buf)&0xFF)+(((*(buf+1))<<8)&0xFF00)+(((*(buf+2))<<16)&0xFF0000)+(((*(buf+3))<<24)&0xFF000000);
			writeb(USB_FIFO(ep_num), temp);
			buf += 4;
			residue -= 4;
		}
	}

#if 0
	if (ep_num == 0) {
		if (count == 0) {
			os_printk(K_DEBUG,"USB_EP0_DATAEND %8X+\n", os_readl(U3D_EP0CSR));
			os_writel(U3D_EP0CSR, os_readl(U3D_EP0CSR) | EP0_DATAEND | EP0_TXPKTRDY);
			os_printk(K_DEBUG,"USB_EP0_DATAEND %8X-\n", os_readl(U3D_EP0CSR));
		} else {
#ifdef AUTOSET
			if (count < maxp) {
				os_writel(U3D_EP0CSR, os_readl(U3D_EP0CSR) | EP0_TXPKTRDY);
				os_printk(K_DEBUG,"EP0_TXPKTRDY\n");
			}
#else
			os_writel(U3D_EP0CSR, os_readl(U3D_EP0CSR)|EP0_TXPKTRDY);
#endif
		}
	} else {
		if (count == 0) {
			USB_WriteCsr32(U3D_TX1CSR0, ep_num, USB_ReadCsr32(U3D_TX1CSR0, ep_num) | TX_TXPKTRDY);
		} else {
#ifdef AUTOSET
			if(count < maxp) {
				USB_WriteCsr32(U3D_TX1CSR0, ep_num, USB_ReadCsr32(U3D_TX1CSR0, ep_num) | TX_TXPKTRDY);
				os_printk(K_DEBUG,"short packet\n");
			}
#else
			USB_WriteCsr32(U3D_TX1CSR0, ep_num, USB_ReadCsr32(U3D_TX1CSR0, ep_num) | TX_TXPKTRDY);
#endif
		}
	}
#endif	
	return count;
}

void musb_otg_write_fifo(u16 len,u8 *buf){
    int i;
    for(i=0;i<len;i++){
    	//musb_writeb(mtk_musb->mregs,0x20,*(buf+i));
    	writeb(*(buf+i), U3D_FIFO0); // EP0 FIFO
    }
}

void musb_host_load_testpacket()
{
    //musb_otg_write_fifo(53,(u8*)musb_host_test_packet);
    f_load_to_fifo(0 , 53, (u8*)musb_host_test_packet , 64) ;
}



#define U3_P1_PMSC_RO_MASK (0x7)
#define U3_P1_PMSC_PORT_TEST_CTRL_OFFSET (28)
#define U3_P1_PMSC_PORT_TEST_MODE_MASK 			(0xF << U3_P1_PMSC_PORT_TEST_CTRL_OFFSET)
#define U3_P1_PMSC_PORT_TEST_MODE_J_STATE 		(1 << U3_P1_PMSC_PORT_TEST_CTRL_OFFSET)
#define U3_P1_PMSC_PORT_TEST_MODE_K_STATE 		(2 << U3_P1_PMSC_PORT_TEST_CTRL_OFFSET)
#define U3_P1_PMSC_PORT_TEST_MODE_SE0_NAK 		(3 << U3_P1_PMSC_PORT_TEST_CTRL_OFFSET)
#define U3_P1_PMSC_PORT_TEST_MODE_TEST_PACKET 	(4 << U3_P1_PMSC_PORT_TEST_CTRL_OFFSET)
#define U3_P1_PMSC_PORT_TEST_MODE_FORCE_EN 	(5 << U3_P1_PMSC_PORT_TEST_CTRL_OFFSET)


void otg_dump_xhci_register(void){
	u32 temp;
	struct xhci_hcd *xhci;
	int port_id;
	u32 __iomem *addr;

	port_id = 1 ;
	xhci = hcd_to_xhci(my_hcd);

	printk("\n\n[otg_dump_xhci_register]\n");

	printk("cap_regs: hc_capbase ==================> \n");
	addr = &xhci->cap_regs->hc_capbase ;
	temp = xhci_readl(xhci, addr);
	printk( "[0x%p] 0x%x \n" , addr, temp);
	addr = &xhci->cap_regs->hcs_params1 ;
	temp = xhci_readl(xhci, addr);
	printk( "[0x%p] 0x%x \n" , addr, temp);
	addr = &xhci->cap_regs->hcs_params2 ;
	temp = xhci_readl(xhci, addr);
	printk( "[0x%p] 0x%x \n" , addr, temp);
	addr = &xhci->cap_regs->hcs_params3 ;
	temp = xhci_readl(xhci, addr);
	printk( "[0x%p] 0x%x \n" , addr, temp);
	addr = &xhci->cap_regs->hcc_params ;
	temp = xhci_readl(xhci, addr);
	printk( "[0x%p] 0x%x \n" , addr, temp);
	addr = &xhci->cap_regs->db_off ;
	temp = xhci_readl(xhci, addr);
	printk( "[0x%p] 0x%x \n" , addr, temp);
	addr = &xhci->cap_regs->run_regs_off ;
	temp = xhci_readl(xhci, addr);
	printk( "[0x%p] 0x%x \n" , addr, temp);						
	printk( "cap_regs: hc_capbase <================== \n");

	printk("op_regs:command_base ==================> \n");
	addr = &xhci->op_regs->command ;
	temp = xhci_readl(xhci, addr);
	printk( "[0x%p] 0x%x \n" , addr, temp);
	addr = &xhci->op_regs->status ;
	temp = xhci_readl(xhci, addr);
	printk( "[0x%p] 0x%x \n" , addr, temp);
	addr = &xhci->op_regs->page_size ;
	temp = xhci_readl(xhci, addr);
	printk( "[0x%p] 0x%x \n" , addr, temp);
	addr = &xhci->op_regs->dev_notification ;
	temp = xhci_readl(xhci, addr);
	printk( "[0x%p] 0x%x \n" , addr, temp);	
	addr = &xhci->op_regs->cmd_ring ;
	temp = xhci_readl(xhci, addr);
	printk( "[0x%p] 0x%x \n" , addr, temp);
	addr = &xhci->op_regs->dcbaa_ptr ;
	temp = xhci_readl(xhci, addr);
	printk( "[0x%p] 0x%x \n" , addr, temp);
	addr = &xhci->op_regs->config_reg ;
	temp = xhci_readl(xhci, addr);
	printk( "[0x%p] 0x%x \n" , addr, temp);
	printk( "op_regs: port_status_base <================== \n");	

	
	printk("op_regs: port_status_base ==================> \n");
	addr = &xhci->op_regs->port_status_base + NUM_PORT_REGS*((port_id-1) & 0xff) ; // PORT_SC
	temp = xhci_readl(xhci, addr);
	printk( "[0x%p] 0x%x \n" , addr, temp);
	addr = &xhci->op_regs->port_power_base + NUM_PORT_REGS*((port_id-1) & 0xff) ; // PORT_SC
	temp = xhci_readl(xhci, addr);
	printk( "[0x%p] 0x%x \n" , addr, temp);
	addr = &xhci->op_regs->port_link_base + NUM_PORT_REGS*((port_id-1) & 0xff) ; // PORT_SC
	temp = xhci_readl(xhci, addr);
	printk( "[0x%p] 0x%x \n" , addr, temp);
	addr = &xhci->op_regs->port_lpm_ctrl_base + NUM_PORT_REGS*((port_id-1) & 0xff) ; // PORT_SC
	temp = xhci_readl(xhci, addr);
	printk( "[0x%p] 0x%x \n" , addr, temp);
	printk( "op_regs: port_status_base <================== \n");	

	printk("run_reg: run_reg_base ==================> \n");
	addr = &xhci->run_regs->microframe_index;
	temp = xhci_readl(xhci, addr);
	printk( "[0x%p] 0x%x \n" , addr, temp);
	addr = &xhci->run_regs->rsvd[0];
	temp = xhci_readl(xhci, addr);
	printk( "[0x%p] 0x%x \n" , addr, temp);
	addr = &xhci->run_regs->rsvd[1];
	temp = xhci_readl(xhci, addr);
	printk( "[0x%p] 0x%x \n" , addr, temp);
	addr = &xhci->run_regs->rsvd[2];
	temp = xhci_readl(xhci, addr);
	printk( "[0x%p] 0x%x \n" , addr, temp);
	addr = &xhci->run_regs->rsvd[3];
	temp = xhci_readl(xhci, addr);
	printk( "[0x%p] 0x%x \n" , addr, temp);
	addr = &xhci->run_regs->rsvd[4];
	temp = xhci_readl(xhci, addr);
	printk( "[0x%p] 0x%x \n" , addr, temp);
	addr = &xhci->run_regs->rsvd[5];
	temp = xhci_readl(xhci, addr);
	printk( "[0x%p] 0x%x \n" , addr, temp);
	addr = &xhci->run_regs->rsvd[6];
	temp = xhci_readl(xhci, addr);
	printk( "[0x%p] 0x%x \n" , addr, temp);
	addr = &xhci->run_regs->rsvd[7];
	temp = xhci_readl(xhci, addr);
	printk( "[0x%p] 0x%x \n" , addr, temp);
	printk( "run_reg: run_reg_base <================== \n");

	printk("intr_reg: xhci_intr_reg_base ==================> \n");
	addr = &xhci->ir_set->irq_pending ;
	temp = xhci_readl(xhci, addr);
	printk( "[0x%p] 0x%x \n" , addr, temp);
	addr = &xhci->ir_set->irq_control ;
	temp = xhci_readl(xhci, addr);
	printk( "[0x%p] 0x%x \n" , addr, temp);
	addr = &xhci->ir_set->erst_size ;
	temp = xhci_readl(xhci, addr);
	printk( "[0x%p] 0x%x \n" , addr, temp);
	addr = &xhci->ir_set->erst_base ;
	temp = xhci_readl(xhci, addr);
	printk( "[0x%p] 0x%x \n" , addr, temp);
	addr = &xhci->ir_set->erst_dequeue ;
	temp = xhci_readl(xhci, addr);
	printk( "[0x%p] 0x%x \n" , addr, temp);	


}

extern void usb_phy_recover(unsigned int clk_on);
extern void usb_phy_savecurrent(unsigned int clk_on);

extern void mu3d_hal_dump_register(void) ;
extern void mtk_set_host_mode_in_host() ;

extern void mtk_set_host_mode_out() ;

int otg_drv_vbus_on(bool on){
	u32 temp;
	int ret;
	u32 __iomem *addr;
	int port_id;
	struct xhci_hcd *xhci;
	//int cur_iddig;
	u32 temp3;
	//set host sel
	printk(KERN_ERR "[OTG_H] going to set dma to host\n");	
	while((readl(SSUSB_OTG_STS) & 0x2000) == 0x2000){ // 0x2000, SSUSB_DEV_DMA_REQ, wait to be host mode
		msleep(2) ;
	}
	printk(KERN_ERR "[OTG_H] Set dma to host done\n");
	temp = readl(SSUSB_U2_CTRL(0));
	temp = temp | SSUSB_U2_PORT_HOST_SEL;
	writel(temp, SSUSB_U2_CTRL(0));
	printk(KERN_ERR "[OTG_H] enter u3h_pet_A_host thread\n");
	//if(f_test_lib_init() != RET_SUCCESS){
	//	printk(KERN_ERR "[OTG_H][FAIL] controller driver init failed\n");
	//}

	xhci = hcd_to_xhci(my_hcd);
	// USBIF , this should be done by IDDIG interrupt in real chip 
	printk(KERN_ERR "[OTG_H] manual set SSUSB_ATTACH_A_ROLE\n");

	g_hs_block_reset = false;
	if(g_hs_block_reset){
		writel((readl(SSUSB_U2_SYS_BASE+0xc)|0x100), SSUSB_U2_SYS_BASE+0xc);
	}
	else{
		writel((readl(SSUSB_U2_SYS_BASE+0xc)&(~0x100)), SSUSB_U2_SYS_BASE+0xc);
	}
	// USBIF , enable USB20
	// writel((readl(0xf00447c8)|0x1fd0000), 0xf00447c8);
	//temp = readl(SSUSB_U3_XHCI_BASE + 0x47c8) ;
	//writel((temp|0x1fd0000), SSUSB_U3_XHCI_BASE + 0x47c8);
	//temp = readl(SSUSB_U3_XHCI_BASE + 0x47c8) ;
	//printk(KERN_ERR "[OTG_H] read  SSUSB_U3_XHCI_BASE + 0x47c8 is 0x%x\n", temp);
	//port_id = 1;
	//xhci = hcd_to_xhci(my_hcd);
	//addr = &xhci->cap_regs->hc_capbase + NUM_PORT_REGS*(port_id-1 & 0xff);
	//addr += 0x47c8 ;
	addr = SIFSLV_IPPC + 0xc8 ;
	writel((readl(addr) |0x1fd0000), addr); // enable USBIF OTG20 timing

	
	msleep(1) ;
	temp3 = readl(SSUSB_OTG_STS);
	mb() ;


	if (!on){
		//turn off vbus	
		printk("[OTG_H] turn off vbus\n");
			
		//reset MAC2
		temp = readl(SSUSB_U2_CTRL(0));
		temp = temp | (SSUSB_U2_PORT_DIS);
		writel(temp, SSUSB_U2_CTRL(0));
		temp = readl(SSUSB_U2_CTRL(0));
		temp = temp & (~SSUSB_U2_PORT_DIS);
		writel(temp, SSUSB_U2_CTRL(0));
		if(g_hs_block_reset){
			writel((readl(SSUSB_U2_SYS_BASE+0xc)|0x100), SSUSB_U2_SYS_BASE+0xc);
		}
		else{
			writel((readl(SSUSB_U2_SYS_BASE+0xc)&(~0x100)), SSUSB_U2_SYS_BASE+0xc);
	    }
		//set port_power=0
		mtktest_disableXhciAllPortPower(xhci) ;
		mtk_set_host_mode_out() ;				

		printk("[OTG_H]turn off vbus done, OTG_STS: 0x%x\n", readl(SSUSB_OTG_STS));

	}else{  //HOST mode
		//turn on vbus
		printk("[OTG_H]turn on vbus\n");
			
		// USBIF, WARN
		//printk(KERN_ERR "[OTG_H] Set dma to host done\n");
		temp = readl(SSUSB_U2_CTRL(0));
		temp = temp | SSUSB_U2_PORT_HOST_SEL;
		writel(temp, SSUSB_U2_CTRL(0));
		
		//reset MAC2
		temp = readl(SSUSB_U2_CTRL(0));
		temp = temp | (SSUSB_U2_PORT_DIS);
		writel(temp, SSUSB_U2_CTRL(0));
		temp = readl(SSUSB_U2_CTRL(0));
		temp = temp & (~SSUSB_U2_PORT_DIS);
		writel(temp, SSUSB_U2_CTRL(0));
		if(g_hs_block_reset){
			writel((readl(SSUSB_U2_SYS_BASE+0xc)|0x100), SSUSB_U2_SYS_BASE+0xc);
		}
		else{
			writel((readl(SSUSB_U2_SYS_BASE+0xc)&(~0x100)), SSUSB_U2_SYS_BASE+0xc);
	    }
		//set port_power=1		
		mtk_set_host_mode_in_host() ;

		mtktest_enableXhciAllPortPower(xhci) ;

		printk("[OTG_H] turn on vbus done, OTG_STS: 0x%x\n", readl(SSUSB_OTG_STS));	
		//mu3d_hal_dump_register() ;			
	}

	
	printk(KERN_ERR "[OTG_H] Exit otg_drv_vbus_on\n");

	return 0 ;
}

int f_host_test_mode(int case_num){
	//u8 temp_8;
	u32 cmd;
	u32 temp;
	struct xhci_hcd *xhci;
	u32 halted;
	int port_id;
	u32 __iomem *addr;

	printk("f_host_test_mode ===> \n");
	xhci = hcd_to_xhci(my_hcd);
	g_exec_done = false ;

	// step 0, set to HOST mode and disable the OTG AUTO SEL	
	temp = readl(SSUSB_U2_CTRL(0));
	temp = temp & ~(SSUSB_U2_PORT_OTG_MAC_AUTO_SEL | SSUSB_U2_PORT_OTG_SEL) ;
	temp = temp | SSUSB_U2_PORT_HOST_SEL;
	writel(temp, SSUSB_U2_CTRL(0));
	temp = readl(SSUSB_U2_CTRL(0));
	printk("SSUSB_U2_CTRL is 0x%x\n",temp);

	port_id = 1 ;
	
#if 1 
	otg_drv_vbus_on(false) ;
#else		
	addr = &xhci->op_regs->port_status_base + NUM_PORT_REGS*(port_id-1 & 0xff) ; // PORT_SC
	temp = xhci_readl(xhci, addr);
		
	printk("USB2_PORT_SC(0x%p is 0x%x\n", addr, temp);

	//xhci_writel(xhci, temp & (~PORT_POWER), addr); // must turn off port power in test mode 
	//turn vbus off
	temp = mtktest_xhci_port_state_to_neutral(temp);
	temp &= ~PORT_POWER;
	xhci_writel(xhci, temp, addr);
#endif		
	usb_phy_recover(0) ;
	
	printk("U3D_POWER_MANAGEMENT is 0x%x\n", readl(POWER_MANAGEMENT));
	writel(readl(POWER_MANAGEMENT) | HS_ENABLE, POWER_MANAGEMENT);
	

	//msleep(2000) ;
	
	printk("SSUSB_OTG_STS is 0x%x\n", readl(SSUSB_OTG_STS)) ;
	
	switch (case_num) {
	case HOST_CMD_TEST_SE0_NAK:
		printk("TEST_SE0_NAK\n");
		// step 1 : set the Run/Stop in USBCMD to 0
		cmd = xhci_readl(xhci, &xhci->op_regs->command);
		cmd &= ~CMD_RUN;;
		xhci_writel(xhci, cmd, &xhci->op_regs->command);

		// step 2 : wait for HCHalted 
		mtk_xhci_handshake(xhci, &xhci->op_regs->status, STS_HALT, 1, XHCI_MAX_HALT_USEC);		

		//msleep(2000) ;
		addr = &xhci->op_regs->port_status_base + NUM_PORT_REGS*(port_id-1 & 0xff) ; // PORT_SC
		temp = xhci_readl(xhci, addr);
		
		printk("USB2_PORT_SC(0x%p) is 0x%x\n", addr, temp);

		addr = &xhci->op_regs->port_power_base + NUM_PORT_REGS*(port_id-1 & 0xff)  ; // PORT_PMSC
		temp = xhci_readl(xhci, addr);
		
		printk("USB2_PORT_PMSC(0x%p) is 0x%x\n", addr, temp);
		
		// step 3 : set the corresponding test mode
		temp = temp & 0x0FFFFFFF;
		temp |= U3_P1_PMSC_PORT_TEST_MODE_SE0_NAK;

		xhci_writel(xhci, temp, addr);

		temp = xhci_readl(xhci, addr);
		printk("USB2_PORT_PMSC(0x%p) is 0x%x\n", addr, temp);
		printk("U3D_POWER_MANAGEMENT is 0x%x\n", readl(POWER_MANAGEMENT));
		//temp = readl(U3_P1_PMSC);		
		//temp = temp & (~U3_P1_PMSC_RO_MASK);
		//temp |= U3_P1_PMSC_PORT_TEST_MODE_SE0_NAK;
		//writel(temp, U3_P1_PMSC);
		break;
	case HOST_CMD_TEST_J:
		printk("TEST_J\n");
		// step 1 : set the Run/Stop in USBCMD to 0
		cmd = xhci_readl(xhci, &xhci->op_regs->command);
		cmd &= ~CMD_RUN;;
		xhci_writel(xhci, cmd, &xhci->op_regs->command);

		// step 2 : wait for HCHalted 
		mtk_xhci_handshake(xhci, &xhci->op_regs->status, STS_HALT, 1, XHCI_MAX_HALT_USEC);
		//msleep(2000) ;
		
		// step 3 : set the corresponding test mode
#if 1
		addr = &xhci->op_regs->port_status_base + NUM_PORT_REGS*(port_id-1 & 0xff) ; // PORT_SC
		temp = xhci_readl(xhci, addr);

		printk("USB2_PORT_SC(0x%p) is 0x%x\n", addr, temp);

		addr = &xhci->op_regs->port_power_base + NUM_PORT_REGS*(port_id-1 & 0xff)  ; // PORT_PMSC
		temp = xhci_readl(xhci, addr);

		printk("USB2_PORT_PMSC(0x%p) is 0x%x\n", addr, temp);

		// step 3 : set the corresponding test mode 
		temp = temp & 0x0FFFFFFF;
		temp |= U3_P1_PMSC_PORT_TEST_MODE_J_STATE;

		xhci_writel(xhci, temp, addr);

		temp = xhci_readl(xhci, addr);
		printk("USB2_PORT_PMSC(0x%p) is 0x%x\n", addr, temp);
		printk("U3D_POWER_MANAGEMENT is 0x%x\n", readl(POWER_MANAGEMENT));

#else
		temp = readl(U3_P1_PMSC);
		printk("U3_P1_PMSC is 0x%x\n", temp);
		temp = temp & (~U3_P1_PMSC_RO_MASK);
		temp |= U3_P1_PMSC_PORT_TEST_MODE_J_STATE;
		writel(temp, U3_P1_PMSC);
#endif		

		break;
	case HOST_CMD_TEST_K:
		printk("TEST_K\n");
		
		// step 1 : set the Run/Stop in USBCMD to 0
		cmd = xhci_readl(xhci, &xhci->op_regs->command);
		cmd &= ~CMD_RUN;;
		xhci_writel(xhci, cmd, &xhci->op_regs->command);

		// step 2 : wait for HCHalted 
		mtk_xhci_handshake(xhci, &xhci->op_regs->status, STS_HALT, 1, XHCI_MAX_HALT_USEC);
		//msleep(2000) ;
		
		// step 3 : set the corresponding test mode
#if 1
		addr = &xhci->op_regs->port_status_base + NUM_PORT_REGS*(port_id-1 & 0xff) ; // PORT_SC
		temp = xhci_readl(xhci, addr);

		printk("USB2_PORT_SC(0x%p) is 0x%x\n", addr, temp);

		addr = &xhci->op_regs->port_power_base + NUM_PORT_REGS*(port_id-1 & 0xff)  ; // PORT_PMSC
		temp = xhci_readl(xhci, addr);

		printk("USB2_PORT_PMSC(0x%p) is 0x%x\n", addr, temp);

		// step 3 : set the corresponding test mode 
		temp = temp & 0x0FFFFFFF;
		temp |= U3_P1_PMSC_PORT_TEST_MODE_K_STATE;

		xhci_writel(xhci, temp, addr);

		temp = xhci_readl(xhci, addr);
		printk("USB2_PORT_PMSC(0x%p) is 0x%x\n", addr, temp);
		printk("U3D_POWER_MANAGEMENT is 0x%x\n", readl(POWER_MANAGEMENT));

#else		
		temp = readl(U3_P1_PMSC);
		printk("U3_P1_PMSC is 0x%x\n", temp);
		temp = temp & (~U3_P1_PMSC_RO_MASK);
		temp |= U3_P1_PMSC_PORT_TEST_MODE_K_STATE;
		writel(temp, U3_P1_PMSC);
#endif		

		break;
	case HOST_CMD_TEST_PACKET:
		printk("TEST_PACKET\n");
		// step 1 : set the Run/Stop in USBCMD to 0
		cmd = xhci_readl(xhci, &xhci->op_regs->command);
		cmd &= ~CMD_RUN;;
		xhci_writel(xhci, cmd, &xhci->op_regs->command);

		// step 2 : wait for HCHalted 
		mtk_xhci_handshake(xhci, &xhci->op_regs->status, STS_HALT, 1, XHCI_MAX_HALT_USEC);
		//msleep(2000) ;
		
		// step 3 : set the corresponding test mode
		//musb_host_load_testpacket();		

#if 1
		addr = &xhci->op_regs->port_status_base + NUM_PORT_REGS*(port_id-1 & 0xff) ; // PORT_SC
		temp = xhci_readl(xhci, addr);

		printk("USB2_PORT_SC(0x%p) is 0x%x\n", addr, temp);

		addr = &xhci->op_regs->port_power_base + NUM_PORT_REGS*(port_id-1 & 0xff)  ; // PORT_PMSC
		temp = xhci_readl(xhci, addr);

		printk("USB2_PORT_PMSC(0x%p) is 0x%x\n", addr, temp);

		// step 3 : set the corresponding test mode 
		temp = temp & 0x0FFFFFFF;
		temp |= U3_P1_PMSC_PORT_TEST_MODE_TEST_PACKET ;

		xhci_writel(xhci, temp, addr);

		temp = xhci_readl(xhci, addr);
		printk("USB2_PORT_PMSC(0x%p) is 0x%x\n", addr, temp);
		printk("U3D_POWER_MANAGEMENT is 0x%x\n", readl(POWER_MANAGEMENT));

		//mu3d_hal_dump_register() ;
		//otg_dump_xhci_register() ;
#else	

		temp = readl(U3_P1_PMSC);
		printk("U3_P1_PMSC is 0x%x\n", temp);
		temp = temp & (~U3_P1_PMSC_RO_MASK);
		temp |= U3_P1_PMSC_PORT_TEST_MODE_TEST_PACKET ;
		writel(temp, U3_P1_PMSC);
#endif	


		//writel(readl(U3D_EP0CSR) | EP0_TXPKTRDY, U3D_EP0CSR);		
		
		break;

	case HOST_CMD_SUSPEND_RESUME://HS_HOST_PORT_SUSPEND_RESUME
		printk("HS_HOST_PORT_SUSPEND_RESUME\n");
		msleep(5000);//the host must continue sending SOFs for 15s
		printk("please begin to trigger suspend!\n");
		msleep(10000);

		//suspend bus
		addr = &xhci->op_regs->port_status_base + NUM_PORT_REGS*(port_id-1 & 0xff);
		temp = xhci_readl(xhci, addr);
		printk( "[OTG_H]before reset port %d = 0x%x\n", port_id-1, temp);
		temp = mtktest_xhci_port_state_to_neutral(temp);
		temp = (temp & ~(0xf << 5));
		temp = (temp | (3 << 5) | PORT_LINK_STROBE); // set PORT to U3 state , suspend 
		xhci_writel(xhci, temp, addr);

		//power = musb_readb(musb->mregs,MUSB_POWER);
		//power |= MUSB_POWER_SUSPENDM | MUSB_POWER_ENSUSPEND;
		//musb_writeb(musb->mregs,MUSB_POWER,power);
		
		msleep(5000);
		printk("please begin to trigger resume!\n");
		msleep(10000);
		
		//power &= ~MUSB_POWER_SUSPENDM;
		//power |= MUSB_POWER_RESUME;
		//musb_writeb(musb->mregs,MUSB_POWER,power);
		
		temp = xhci_readl(xhci, addr);
		temp = mtktest_xhci_port_state_to_neutral(temp);
		temp = (temp & ~(0xf << 5));
		temp = (temp | PORT_LINK_STROBE);

		xhci_writel(xhci, temp, addr);
		mtk_xhci_handshake(xhci, addr, (15<<5), 0, 10*1000);		
		mdelay(25);
		//power &= ~MUSB_POWER_RESUME;
		//musb_writeb(musb->mregs,MUSB_POWER,power);
		
		//SOF continue
		//musb_h_setup(&setup_packet);
		f_getdescriptor();
		break;
	case HOST_CMD_GET_DESCRIPTOR://SINGLE_STEP_GET_DEVICE_DESCRIPTOR setup
		printk("HOST_CMD_GET_DESCRIPTOR\n");
		msleep(15000);//the host issues SOFs for 15s allowing the test engineer to raise the scope trigger just above the SOF voltage level.
		//musb_h_setup(&setup_packet);
		f_getdescriptor();
		break;
	case HOST_CMD_SET_FEATURE://SINGLE_STEP_GET_DEVICE_DESCRIPTOR execute
		printk("HOST_CMD_SET_FEATURE\n");
		//get device descriptor
		f_getdescriptor();
		//musb_h_setup(&setup_packet);
		msleep(15000);
		//musb_h_in_data((char*)&device_descriptor,sizeof(struct usb_device_descriptor));
		//musb_h_out_status();
		break;
	default:
		break;

	}

	g_exec_done = true ;
	
	return 0 ;
}

int f_host_test_mode_stop(){
	u32 temp;
	struct xhci_hcd *xhci;
	u32 halted;
	int port_id;
	u32 __iomem *addr;

	xhci = hcd_to_xhci(my_hcd);

	printk("f_host_test_mode_stop ===> \n");
	
	//temp = readl(U3_P1_PMSC);
	//temp = temp & (~U3_P1_PMSC_RO_MASK);
	//temp = temp & (~U3_P1_PMSC_PORT_TEST_MODE_MASK);
	//writel(temp, U3_P1_PMSC);	

	port_id = 1 ;

	addr = &xhci->op_regs->port_power_base + NUM_PORT_REGS*(port_id-1 & 0xff)  ; // PORT_PMSC
	temp = xhci_readl(xhci, addr);
	
	printk("USB2_PORT_PMSC(0x%p) is 0x%x\n", addr, temp);
	
	// step 3 : set the corresponding test mode 
	temp = temp & 0x0FFFFFFF;
	
	xhci_writel(xhci, temp, addr);
	
	temp = xhci_readl(xhci, addr);
	printk("USB2_PORT_PMSC(0x%p) is 0x%x\n", addr, temp);
	
#if 0 
	addr = &xhci->op_regs->port_status_base + NUM_PORT_REGS*(port_id-1 & 0xff) ; // PORT_SC
	temp = xhci_readl(xhci, addr);
		
	printk("USB2_PORT_SC(0x%p) is 0x%x\n", addr, temp);

	xhci_writel(xhci, temp | (PORT_POWER), addr); // turn on port power after leaving test mode 
#endif
	usb_phy_savecurrent(1) ;

	return 0 ;
}

int f_drv_vbus(){
	struct xhci_hcd *xhci;

	g_exec_done = false ;
	//xhci = hcd_to_xhci(my_hcd);
	//mtktest_enableXhciAllPortPower(xhci);	

	otg_drv_vbus_on(true) ;

	g_exec_done = true ;
	return 0;
}

int f_stop_vbus(){
	struct xhci_hcd *xhci;

	//xhci = hcd_to_xhci(my_hcd);
	//mtktest_disableXhciAllPortPower(xhci);	

	otg_drv_vbus_on(false) ;

	return 0;
}

int otg_opt_uut_a(void *data){
	u32 temp;
	int ret;
	u32 __iomem *addr;
	int port_id;
	struct xhci_hcd *xhci;
	g_otg_test = true;
	g_otg_dev_B = false;
	g_stopped = false;
	g_exec_done = false ;
	//enable OTG interrupt
	temp = readl(SSUSB_OTG_INT_EN);
	temp = temp | SSUSB_ATTACH_A_ROLE_INT_EN | SSUSB_CHG_A_ROLE_A_INT_EN 
		| SSUSB_CHG_B_ROLE_A_INT_EN | SSUSB_SRP_REQ_INTR_EN;
    writel(temp, SSUSB_OTG_INT_EN);
	//set host sel
	printk(KERN_ERR "[OTG_H] going to set dma to host\n");
	while((readl(SSUSB_OTG_STS) & 0x2000) == 0x2000){
	}
#if 0
writel(0x0f0f0f0f, 0xf00447bc);
while((readl(0xf00447c4) & 0x2000) == 0x2000){
		
	}
#endif
	printk(KERN_ERR "[OTG_H] can set dma to host\n");
#if 1
	temp = readl(SSUSB_U2_CTRL(0));
	temp = temp | SSUSB_U2_PORT_HOST_SEL;
	writel(temp, SSUSB_U2_CTRL(0));
#endif	
	printk(KERN_ERR "[OTG_H] enter u3h_uut_A_host thread\n");
	if(f_test_lib_init() != RET_SUCCESS){
		printk(KERN_ERR "[OTG_H][FAIL] controller driver init failed\n");
		goto FAIL_RETURN;
	}
	//emulate device attach
	writel(SSUSB_ATTACH_A_ROLE, SSUSB_OTG_STS);
	if(f_enable_port(0) != RET_SUCCESS){
		printk(KERN_ERR "[OTG_H][FAIL] Device attach timeout\n");
		goto FAIL_RETURN;
	}
	printk(KERN_ERR "[OTG_H] Device attached\n");
	if(f_enable_slot(NULL) != RET_SUCCESS){
		printk(KERN_ERR "[OTG_H][FAIL] Enable slot failed\n");
		goto FAIL_RETURN;
	}
	//set address
	if(f_address_slot(false, NULL) != RET_SUCCESS){
		printk(KERN_ERR "[OTG_H][FAIL] Address device failed\n");
		goto FAIL_RETURN;
	}
	//get device descriptor
	f_getdescriptor();
	printk(KERN_ERR "[OTG_H]Get descriptor done\n");
	//get configuration descriptor and all configuration descriptor
	f_getconfiguration_desc();
	printk(KERN_ERR "[OTG_H]Get configuration done\n");
	//set hnp
	f_set_hnp();
	printk(KERN_ERR "[OTG_H]Set HNP done\n");
	//set configuration
	f_set_configuration();
	printk(KERN_ERR "[OTG_H]Set configuration done\n");
	g_port_resume = 0;
	g_port_connect = 1;
TD_4_6:
	g_otg_hnp_become_dev = false;
	//suspend bus
	port_id = 1;
	xhci = hcd_to_xhci(my_hcd);
	addr = &xhci->op_regs->port_status_base + NUM_PORT_REGS*(port_id-1 & 0xff);
	temp = xhci_readl(xhci, addr);
	xhci_dbg(xhci, "before reset port %d = 0x%x\n", port_id-1, temp);
	temp = mtktest_xhci_port_state_to_neutral(temp);
	temp = (temp & ~(0xf << 5));
	temp = (temp | (3 << 5) | PORT_LINK_STROBE);
	xhci_writel(xhci, temp, addr);
	//check if disconnect or resume
	while(1){
		if((g_port_resume == 1) || (g_port_connect == 0)){
			break;
		}
		if(g_stopped){
			printk(KERN_ERR "[OTG_H] force stop\n");
			goto FAIL_RETURN;
		}
		msleep(1);
	}
	printk(KERN_ERR "[OTG_H]Device wakeup or disconnect\n");
	if(g_port_resume == 1){
		//do remote wakeup
		printk(KERN_ERR "[OTG_H]Device do remote wakeup\n");
		msleep(20);
		temp = xhci_readl(xhci, addr);
		temp = mtktest_xhci_port_state_to_neutral(temp);
		temp = (temp & ~(0xf << 5));
		temp = (temp | PORT_LINK_STROBE);

		xhci_writel(xhci, temp, addr);
		mtk_xhci_handshake(xhci, addr, (15<<5), 0, 10*1000);
		temp = xhci_readl(xhci, addr);
		if(PORT_PLS_VALUE(temp) != 0){//(temp & PORT_PLS(15)) != PORT_PLS(0)){
			xhci_err(xhci, "[OTG_H]port not return to U0\n");
			printk(KERN_ERR "[OTG_H]exit u3h_uut_A_host thread\n");
		}
		else{
			printk(KERN_ERR "[OTG_H]exit u3h_uut_A_host thread\n");
		}
	}
	if(g_port_connect == 0){
		printk(KERN_ERR "[OTG_H]Disconnect, looks device do hnp\n");
		if(!g_otg_hnp_become_dev){
			printk(KERN_ERR "[OTG_H][FAILE]Doesn't become device, exit\n");
			goto FAIL_RETURN;
		}
		ret = f_disable_slot();
		if(ret){
			printk(KERN_ERR "[OTG_H][FAIL] disable slot failed!!!!!!!!!!\n");
			goto FAIL_RETURN;
		}
		//alert device
		writel(SSUSB_CHG_B_ROLE_B, SSUSB_OTG_STS);
	}
	g_otg_hnp_become_host = false;
	printk(KERN_ERR "[OTG_H]Wait to become host again\n");
	while(!g_otg_hnp_become_host){
		msleep(1);
		if(g_stopped){
			printk(KERN_ERR "[OTG_H]force stop\n");
			goto FAIL_RETURN;
		}
	}
	printk(KERN_ERR "[OTG_H]Back to become host again\n");
	if(f_enable_port(0) != RET_SUCCESS){
		printk(KERN_ERR "[OTG_H][FAIL] Device attach timeout\n");
		goto FAIL_RETURN;
	}
	goto TD_4_6;
	printk(KERN_ERR "[OTG_H] Device attached\n");
	printk(KERN_ERR "[OTG_H] exit u3h_uut_A_host thread\n");

	FAIL_RETURN :
	g_exec_done = true ; // let f_test_lib_cleanup to get the thread done info
	return 0 ;
}

int otg_opt_uut_b(void *data){
	u32 temp;
	int ret;
	u32 __iomem *addr;
	int port_id;
	struct xhci_hcd *xhci;
	
	g_otg_test = true;
	g_otg_dev_B = true;
	g_otg_hnp_become_host = false;
	g_stopped = false;
	g_exec_done = false ;
	//enable OTG interrupt
	temp = readl(SSUSB_OTG_INT_EN);
	temp = temp | SSUSB_ATTACH_A_ROLE_INT_EN | SSUSB_CHG_A_ROLE_A_INT_EN 
		| SSUSB_CHG_B_ROLE_A_INT_EN | SSUSB_SRP_REQ_INTR_EN;
    writel(temp, SSUSB_OTG_INT_EN);
	printk(KERN_ERR "[OTG_H] enter u3h_uut_B_host thread\n");

	//set host sel
        printk(KERN_ERR "[OTG_H] going to set dma to host\n");
while((readl(SSUSB_OTG_STS) & 0x2000) == 0x2000){
        }
#if 0
writel(0x0f0f0f0f, 0xf00447bc);
while((readl(0xf00447c4) & 0x2000) == 0x2000){

        }
#endif
#if 1
        printk(KERN_ERR "[OTG_H] can set dma to host\n");
        temp = readl(SSUSB_U2_CTRL(0));
        temp = temp | SSUSB_U2_PORT_HOST_SEL;
        writel(temp, SSUSB_U2_CTRL(0));	
#endif	
	if(f_test_lib_init() != RET_SUCCESS){
		printk(KERN_ERR "[OTG_H] controller driver init failed\n");
		goto FAIL_RETURN ;
	}
	g_hs_block_reset = true;
	//wait to become host
	writel((readl(SSUSB_U2_SYS_BASE+0xc)|0x100), SSUSB_U2_SYS_BASE+0xc);
	// USBIF, HCRST should not be software reset for U2 MAC port0
	// writel((readl(0xf00409b4) & ~(0x10000)), 0xf00409b4);
	// writel((readl(SSUSB_U3_XHCI_BASE + 0x9b4) & ~(0x10000)), SSUSB_U3_XHCI_BASE + 0x9b4);
	port_id = 1;
	xhci = hcd_to_xhci(my_hcd);
	addr = &xhci->cap_regs->hc_capbase + NUM_PORT_REGS*(port_id-1 & 0xff);
	addr += 0x9b4 ;
	writel((readl(addr) & ~(0x10000)), addr);
    
    
	printk(KERN_ERR "[OTG_H] Wait to become host\n");
	while(!g_otg_hnp_become_host){
		msleep(1);
		if(g_stopped){
			printk(KERN_ERR "[OTG_H]force stop\n");
			goto FAIL_RETURN ;
		}
	}
	printk(KERN_ERR "[OTG_H] Become host\n");
#if 1 
	if(f_enable_port(0) != RET_SUCCESS){
		printk(KERN_ERR "[OTG_H][FAIL] Device attach timeout\n");
		goto FAIL_RETURN ;
	}
	printk(KERN_ERR "[OTG_H] Device attached\n");
#endif
	while(!g_port_reset){
	}
	printk(KERN_ERR "[OTG_H]Device reset]\n");
	if(f_enable_slot(NULL) != RET_SUCCESS){
		printk(KERN_ERR "[OTG_H][FAIL] Enable slot failed\n");
		goto FAIL_RETURN ;
	}
	if(f_address_slot(false, NULL) != RET_SUCCESS){
		printk(KERN_ERR "[OTG_H][FAIL] Address device failed\n");
		goto FAIL_RETURN ;
	}
	//get device descriptor
	f_getdescriptor();
	printk(KERN_ERR "[OTG_H]Get descriptor done\n");
	//get configuration descriptor and all configuration descriptor
	f_getconfiguration_desc();
	printk(KERN_ERR "[OTG_H]Get configuration done\n");

	g_otg_hnp_become_dev = false;
	//suspend bus
	port_id = 1;
	xhci = hcd_to_xhci(my_hcd);
	addr = &xhci->op_regs->port_status_base + NUM_PORT_REGS*(port_id-1 & 0xff);
	temp = xhci_readl(xhci, addr);
	xhci_dbg(xhci, "before reset port %d = 0x%x\n", port_id-1, temp);
	temp = mtktest_xhci_port_state_to_neutral(temp);
	temp = (temp & ~(0xf << 5));
	temp = (temp | (3 << 5) | PORT_LINK_STROBE);
	xhci_writel(xhci, temp, addr);

	//polling dev_mode
	//wait_event_on_timeout(&g_otg_hnp_become_dev, true, TRANS_TIMEOUT);
	while(!g_otg_hnp_become_dev && !g_stopped){
		msleep(1);
	}
	if(g_stopped){
		printk(KERN_ERR "[OTG_H]force stop\n");
		goto FAIL_RETURN ;
	}
	if(g_otg_hnp_become_dev){
	}
	else{
		printk(KERN_ERR "[OTG_H][FAIL] doesn't get B_ROLE_A interrupt\n");
		goto FAIL_RETURN ;
	}
	printk(KERN_ERR "[OTG_H] device B become device\n");
	ret = f_disable_slot();
	if(ret){
		printk(KERN_ERR "[OTG_H][FAIL] disable slot failed!!!!!!!!!!\n");
		goto FAIL_RETURN ;
	}
	//set B_role_B
	writel(SSUSB_CHG_B_ROLE_B, SSUSB_OTG_STS);

	FAIL_RETURN :
	
	g_exec_done = true ;// let f_test_lib_cleanup to get the thread done info

	return 0 ;
}


int otg_pet_uut_a(void *data){
	u32 temp;
	int ret;
	u32 __iomem *addr;
	int port_id;
	struct xhci_hcd *xhci;
	int cur_iddig;
	u32 temp3;
	u32 vbus_on_timeout;
	u32 vbus_on;
	u32 test = 0 ;
	int i=0 ;

	xhci_usbif_resource_get();			
	
	g_otg_test = true;
	g_otg_dev_B = false;
	g_stopped = false;
	g_exec_done = false ;
	g_otg_csc = false;
	g_port_connect = false;
	mb() ;
	//enable OTG interrupt
	temp = readl(SSUSB_OTG_INT_EN);
	temp = temp | SSUSB_ATTACH_A_ROLE_INT_EN | SSUSB_CHG_A_ROLE_A_INT_EN 
		| SSUSB_CHG_B_ROLE_A_INT_EN | SSUSB_SRP_REQ_INTR_EN;
    writel(temp, SSUSB_OTG_INT_EN);
	//set host sel
	printk(KERN_ERR "[OTG_H] going to set dma to host\n");	
	while((readl(SSUSB_OTG_STS) & 0x2000) == 0x2000){ // 0x2000, SSUSB_DEV_DMA_REQ, wait to be host mode
		msleep(2) ;
	}
	printk(KERN_ERR "[OTG_H] Set dma to host done\n");
	temp = readl(SSUSB_U2_CTRL(0));
	temp = temp | SSUSB_U2_PORT_HOST_SEL;
	writel(temp, SSUSB_U2_CTRL(0));
	printk(KERN_ERR "[OTG_H] enter u3h_pet_A_host thread\n");
	if(f_test_lib_init() != RET_SUCCESS){
		printk(KERN_ERR "[OTG_H][FAIL] controller driver init failed\n");
		goto FAIL_RETURN ; 
	}

	xhci = hcd_to_xhci(my_hcd);
	// USBIF , this should be done by IDDIG interrupt in real chip 
	printk(KERN_ERR "[OTG_H] manual set SSUSB_ATTACH_A_ROLE\n");
	//writel(SSUSB_ATTACH_A_ROLE, SSUSB_OTG_STS);	
	g_otg_pet_status = OTG_DISCONNECTED;	
	//g_otg_iddig = 1; //default uutb mode
	cur_iddig = 1;
	vbus_on =false;
	vbus_on_timeout = 0;
	g_otg_slot_enabled = false;
	g_hs_block_reset = false;
	if(g_hs_block_reset){
		writel((readl(SSUSB_U2_SYS_BASE+0xc)|0x100), SSUSB_U2_SYS_BASE+0xc);
	}
	else{
		writel((readl(SSUSB_U2_SYS_BASE+0xc)&(~0x100)), SSUSB_U2_SYS_BASE+0xc);
	}
	// USBIF , enable USB20
	// writel((readl(0xf00447c8)|0x1fd0000), 0xf00447c8);
	//temp = readl(SSUSB_U3_XHCI_BASE + 0x47c8) ;
	//writel((temp|0x1fd0000), SSUSB_U3_XHCI_BASE + 0x47c8);
	//temp = readl(SSUSB_U3_XHCI_BASE + 0x47c8) ;
	//printk(KERN_ERR "[OTG_H] read  SSUSB_U3_XHCI_BASE + 0x47c8 is 0x%x\n", temp);
	//port_id = 1;
	//xhci = hcd_to_xhci(my_hcd);
	//addr = &xhci->cap_regs->hc_capbase + NUM_PORT_REGS*(port_id-1 & 0xff);
	//addr += 0x47c8 ;
	addr = SIFSLV_IPPC + 0xc8 ;
	writel((readl(addr) |0x1fd0000), addr); // enable USBIF OTG20 timing


	while(g_stopped == false){	
		msleep(1) ;
		temp3 = readl(SSUSB_OTG_STS);
		mb() ;
		if(temp3 & SSUSB_IDDIG){
			//g_otg_iddig = 1;
		}
		else{
			//g_otg_iddig = 0;
		}		

#if 0
		if((g_otg_pet_status == OTG_DISCONNECTED) && (vbus_on_timeout == 0) && vbus_on){
			//turn off vbus if vbus on
			printk("[OTG_H] vbus on and no connect event timeout, turn off vbus\n");
#if 0
			//set HOST_VBUSVALID_SEL=0
			temp = readl(SSUSB_U2_CTRL(0));
			temp = temp & (~SSUSB_U2_PORT_OTG_HOST_VBUSVALID_SEL);
			writel(temp, SSUSB_U2_CTRL(0));
#endif
			//reset MAC2
			temp = readl(SSUSB_U2_CTRL(0));
			temp = temp | (SSUSB_U2_PORT_DIS);
			writel(temp, SSUSB_U2_CTRL(0));
			temp = readl(SSUSB_U2_CTRL(0));
			temp = temp & (~SSUSB_U2_PORT_DIS);
			writel(temp, SSUSB_U2_CTRL(0));
			if(g_hs_block_reset){
				writel((readl(SSUSB_U2_SYS_BASE+0xc)|0x100), SSUSB_U2_SYS_BASE+0xc);
			}
			else{
				writel((readl(SSUSB_U2_SYS_BASE+0xc)&(~0x100)), SSUSB_U2_SYS_BASE+0xc);
		    }
			//set port_power=0
			addr = &xhci->op_regs->port_status_base + NUM_PORT_REGS*(0 & 0xff);
			temp = xhci_readl(xhci, addr);
			temp = mtktest_xhci_port_state_to_neutral(temp);
			temp &= ~PORT_POWER;
			xhci_writel(xhci, temp, addr);
			vbus_on = false;
			cur_iddig = 1;
			msleep(10);
		}
		else if(g_otg_pet_status == OTG_DISCONNECTED && vbus_on_timeout > 0){
			vbus_on_timeout--;
			msleep(1);
		}
#endif
		if(g_otg_iddig != cur_iddig ){	
			if(g_otg_iddig == 1){  //DEVICE mode			
				//turn off vbus	
				//printk("[OTG_H] iddig up, turn off vbus, g_otg_iddig %d, cur_iddig %d\n", g_otg_iddig, cur_iddig);
#if 0				
				//set HOST_VBUSVALID_SEL=0
				temp = readl(SSUSB_U2_CTRL(0));
				temp = temp & (~SSUSB_U2_PORT_OTG_HOST_VBUSVALID_SEL);
				writel(temp, SSUSB_U2_CTRL(0));
#endif				
				//reset MAC2
				temp = readl(SSUSB_U2_CTRL(0));
				temp = temp | (SSUSB_U2_PORT_DIS);
				writel(temp, SSUSB_U2_CTRL(0));
				temp = readl(SSUSB_U2_CTRL(0));
				temp = temp & (~SSUSB_U2_PORT_DIS);
				writel(temp, SSUSB_U2_CTRL(0));
				msleep(10);
				
				if(g_hs_block_reset){
					writel((readl(SSUSB_U2_SYS_BASE+0xc)|0x100), SSUSB_U2_SYS_BASE+0xc);
				}
				else{
					writel((readl(SSUSB_U2_SYS_BASE+0xc)&(~0x100)), SSUSB_U2_SYS_BASE+0xc);
			    }
				//set port_power=0
				#if 0
				addr = &xhci->op_regs->port_status_base + NUM_PORT_REGS*(0 & 0xff);
				temp = xhci_readl(xhci, addr);
				temp = mtktest_xhci_port_state_to_neutral(temp);
				temp &= ~PORT_POWER;
				xhci_writel(xhci, temp, addr);
				#else
				mtktest_disableXhciAllPortPower(xhci) ;
				#endif

				mtk_set_host_mode_out() ;				
			
				printk("[OTG_H] iddig up, turn off vbus done, OTG_STS: 0x%x\n", readl(SSUSB_OTG_STS));
				cur_iddig = 1;
				g_otg_iddig_toggled = true;
				vbus_on = false;
			}else{  //HOST mode
				//turn on vbus
				//printk("[OTG_H] iddig down, turn on vbus, g_otg_iddig %d, cur_iddig %d \n", g_otg_iddig, cur_iddig);
#if 0				
				//set HOST_VBUSVALID_SEL=1
				temp = readl(SSUSB_U2_CTRL(0));
				temp = temp | (SSUSB_U2_PORT_OTG_HOST_VBUSVALID_SEL);
				writel(temp, SSUSB_U2_CTRL(0));
#endif				
				// USBIF, WARN
				//printk(KERN_ERR "[OTG_H] Set dma to host done\n");
				temp = readl(SSUSB_U2_CTRL(0));
				temp = temp | SSUSB_U2_PORT_HOST_SEL;
				writel(temp, SSUSB_U2_CTRL(0));
				
				//reset MAC2
				temp = readl(SSUSB_U2_CTRL(0));
				temp = temp | (SSUSB_U2_PORT_DIS);
				writel(temp, SSUSB_U2_CTRL(0));
				temp = readl(SSUSB_U2_CTRL(0));
				temp = temp & (~SSUSB_U2_PORT_DIS);
				writel(temp, SSUSB_U2_CTRL(0));
				msleep(10);

				if(g_hs_block_reset){
					writel((readl(SSUSB_U2_SYS_BASE+0xc)|0x100), SSUSB_U2_SYS_BASE+0xc);
				}
				else{
					writel((readl(SSUSB_U2_SYS_BASE+0xc)&(~0x100)), SSUSB_U2_SYS_BASE+0xc);
			    }
				//set port_power=1		
				mtk_set_host_mode_in_host() ;

				#if 0
				addr = &xhci->op_regs->port_status_base + NUM_PORT_REGS*(0 & 0xff);
				temp = xhci_readl(xhci, addr);
				temp = mtktest_xhci_port_state_to_neutral(temp);
				temp |= PORT_POWER;
				xhci_writel(xhci, temp, addr);
				#else
				mtktest_enableXhciAllPortPower(xhci) ;
				#endif
			
				printk("[OTG_H] iddig down, turn on vbus done, OTG_STS: 0x%x\n", readl(SSUSB_OTG_STS));
				cur_iddig = 0;
				//vbus_on_timeout = 200;	//vbus will drop after some time
				vbus_on = true;
				g_otg_iddig_toggled = true;
	
				//mu3d_hal_dump_register() ;			
			}
		}				
		if(g_otg_csc && g_otg_pet_status != OTG_HNP_DEV){
			g_otg_csc = false;
			mb() ;
			if(g_port_connect){
				printk("[OTG_H] device attached, wait for reset done\n");
				while(!g_port_reset && g_port_connect){
					msleep(2) ;
				}
				if(!g_port_connect){
					printk(KERN_ERR "[OTG_H] Device disconnected, goto DISCONNECT\n");
					g_otg_csc = false;
					g_otg_pet_status = OTG_DISCONNECTED;
					continue;
				}
				g_otg_csc = false;
				g_otg_unsupported_dev = false;
				g_port_reset = false;
				printk(KERN_ERR "[OTG_H] Device reset done\n");
				mb() ;
				if(!g_port_connect){
					printk(KERN_ERR "[OTG_H] Device disconnected\n");
					g_otg_csc = false;
					g_otg_pet_status = OTG_DISCONNECTED;
					continue;
				}
				 f_disable_slot(); 
				if(f_enable_slot(NULL) != RET_SUCCESS){
					printk(KERN_ERR "[OTG_H][FAIL] Enable slot failed\n");
					// USBIF , show "Device No Response"
					usbif_u3h_test_send_event(USBIF_OTG_EVENT_DEV_CONN_TMOUT);
					//goto FAIL_RETURN ; 
				}
				g_otg_slot_enabled = true;
				mb() ;
				if(!g_port_connect){
					printk(KERN_ERR "[OTG_H] Device disconnected\n");
					g_otg_csc = false;
					ret = f_disable_slot();
					if(ret){
						printk(KERN_ERR "[OTG_H][FAIL] disable slot failed!!!!!!!!!!\n");
						//goto FAIL_RETURN ; 
					}
					g_otg_pet_status = OTG_DISCONNECTED;
					g_otg_slot_enabled = false;
					continue;
				}
				f_address_slot(true,NULL);
				mb() ;
				if(!g_port_connect){
					printk(KERN_ERR "[OTG_H] Device disconnected\n");
					g_otg_csc = false;
					ret = f_disable_slot();
					if(ret){
						printk(KERN_ERR "[OTG_H][FAIL] disable slot failed!!!!!!!!!!\n");
						//goto FAIL_RETURN ; 
					}
					g_otg_pet_status = OTG_DISCONNECTED;
					g_otg_slot_enabled = false;
					continue;
				}
				ret = f_getdescriptor();
				if(ret==RET_FAIL){
					printk(KERN_ERR "[OTG_H] ===================Device No Response=================\n");
					usbif_u3h_test_send_event(USBIF_OTG_EVENT_DEV_CONN_TMOUT);
					while(g_port_connect){
						msleep(2) ;
						mb() ; 
					}
					printk(KERN_ERR "[OTG_H] Port disconnected\n");
					ret = f_disable_slot();
					if(ret){
						printk(KERN_ERR "[OTG_H][FAIL] disable slot failed!!!!!!!!!!\n");
						//goto FAIL_RETURN ; 
					}
					g_otg_csc = false;
					g_otg_pet_status = OTG_DISCONNECTED;
					g_otg_slot_enabled = false;
					continue;
				}
				printk(KERN_ERR "[OTG_H]Get descriptor done\n");
				if(g_otg_unsupported_dev){
					//waiting for disconnect
					while(g_port_connect){
						msleep(2) ;
						mb() ;
					}
					printk(KERN_ERR "[OTG_H] Port disconnected\n");
					ret = f_disable_slot();
					if(ret){
						printk(KERN_ERR "[OTG_H][FAIL] disable slot failed!!!!!!!!!!\n");
						//goto FAIL_RETURN ; 
					}
					g_otg_csc = false;
					g_otg_pet_status = OTG_DISCONNECTED;
					g_otg_slot_enabled = false;
					continue;
				}
				if(!g_port_connect){
					printk(KERN_ERR "[OTG_H] Device disconnected\n");
					g_otg_csc = false;
					ret = f_disable_slot();
					if(ret){
						printk(KERN_ERR "[OTG_H][FAIL] disable slot failed!!!!!!!!!!\n");
						//goto FAIL_RETURN ; 
					}
					g_otg_csc = false;
					g_otg_pet_status = OTG_DISCONNECTED;
					g_otg_slot_enabled = false;
					continue;
				}				
				
				if(f_address_slot(false, NULL) != RET_SUCCESS){
					printk(KERN_ERR "[OTG_H][FAIL] Address device failed\n");
					//goto FAIL_RETURN ; 
				}				
				if(!g_port_connect){
					printk(KERN_ERR "[OTG_H] Device disconnected\n");
					g_otg_csc = false;
					ret = f_disable_slot();
					if(ret){
						printk(KERN_ERR "[OTG_H][FAIL] disable slot failed!!!!!!!!!!\n");
						//goto FAIL_RETURN ; 
					}
					g_otg_csc = false;
					g_otg_pet_status = OTG_DISCONNECTED;
					g_otg_slot_enabled = false;
					continue;
				}
				f_getdescriptor();
				printk(KERN_ERR "[OTG_H]Get descriptor done\n");
				mb() ;
				if(!g_port_connect){
					printk(KERN_ERR "[OTG_H] Device disconnected\n");
					g_otg_csc = false;
					ret = f_disable_slot();
					if(ret){
						printk(KERN_ERR "[OTG_H][FAIL] disable slot failed!!!!!!!!!!\n");
						//goto FAIL_RETURN ; 
					}
					g_otg_csc = false;
					g_otg_pet_status = OTG_DISCONNECTED;
					g_otg_slot_enabled = false;
					continue;
				}				
				f_getqualifierdescriptor();
				printk(KERN_ERR "[OTG_H]Get qualifier descriptor done\n");
				mb() ;
				if(!g_port_connect){
					printk(KERN_ERR "[OTG_H] Device disconnected\n");
					g_otg_csc = false;
					ret = f_disable_slot();
					if(ret){
						printk(KERN_ERR "[OTG_H][FAIL] disable slot failed!!!!!!!!!!\n");
						//goto FAIL_RETURN ; 
					}
					g_otg_csc = false;
					g_otg_pet_status = OTG_DISCONNECTED;
					g_otg_slot_enabled = false;
					continue;
				}				
				f_getconfiguration_desc();
				printk(KERN_ERR "[OTG_H]Get configuration done\n");
				mb() ;
				if(!g_port_connect){
					printk(KERN_ERR "[OTG_H] Device disconnected\n");
					g_otg_csc = false;
					ret = f_disable_slot();
					if(ret){
						printk(KERN_ERR "[OTG_H][FAIL] disable slot failed!!!!!!!!!!\n");
						//goto FAIL_RETURN ; 
					}
					g_otg_csc = false;
					g_otg_pet_status = OTG_DISCONNECTED;
					g_otg_slot_enabled = false;
					continue;
				}
				printk(KERN_ERR "[OTG_H] g_otg_dev_conf_len is %d\n", g_otg_dev_conf_len);				
				f_get_string_desc();
				printk(KERN_ERR "[OTG_H]Get string descirptor done\n");
				mb() ;
				if(!g_port_connect){
					printk(KERN_ERR "[OTG_H] Device disconnected\n");
					g_otg_csc = false;
					ret = f_disable_slot();
					if(ret){
						printk(KERN_ERR "[OTG_H][FAIL] disable slot failed!!!!!!!!!!\n");
						//goto FAIL_RETURN ; 
					}
					g_otg_csc = false;
					g_otg_pet_status = OTG_DISCONNECTED;
					g_otg_slot_enabled = false;
					continue;
				}
				printk(KERN_ERR "[OTG_H] g_otg_dev_conf_len is %d\n", g_otg_dev_conf_len);
				if(g_otg_dev_conf_len == 21){
					//otg13
					//set host hnp
					printk(KERN_ERR "[OTG_H] OTG13 set A_HNP_SUPPORT\n");
					f_set_a_hnp();
				}
				f_set_configuration();
				printk(KERN_ERR "[OTG_H]Set configuration done\n");
				if(g_otg_dev_conf_len == 23){
					g_otg_pet_status = OTG_POLLING_STATUS;
				}else{
					g_otg_pet_status = OTG_SET_NHP;
				}
				
				continue;
				
			}
			else{
				printk(KERN_ERR "[OTG_H]Port disconnected\n");
				
				if(g_otg_pet_status == OTG_HNP_SUSPEND){
					while(!g_otg_hnp_become_dev || (readl(SSUSB_OTG_STS)&SSUSB_HOST_DEV_MODE)){
						msleep(1);
					}
				}

				if(g_otg_pet_status == OTG_HNP_SUSPEND){
					g_otg_hnp_become_host = false;
					g_otg_pet_status = OTG_HNP_DEV;
					//alert device
					printk(KERN_ERR "[OTG_H] set CHG_B_ROLE_B\n");
					writel(SSUSB_CHG_B_ROLE_B, SSUSB_OTG_STS);
					printk(KERN_ERR "[OTG_H]Port become device\n");
					//continue;
				}
				else{
					g_otg_pet_status = OTG_DISCONNECTED;
				}

				if(g_otg_slot_enabled){
				// USBIF, disabe slot may timeout and cause the USBIF test case fail				
					//ret = f_disable_slot();
				/*
					if(ret){
						printk(KERN_ERR "[OTG_H][FAIL] disable slot failed!!!!!!!!!!\n");
						goto FAIL_RETURN ; 
					}
				*/
						g_otg_slot_enabled = false;						
				}
				mb() ;
			}
		}
		if(g_otg_pet_status == OTG_POLLING_STATUS){
			// USBIF , WARN
			msleep(1500);	//don't polling too frequent
			if(g_otg_csc){
				//disconnect happened
				continue;
			}
			ret = f_get_otg_status();
			if(ret == 1){
				printk(KERN_ERR "[OTG_H]HNP enabled got\n");
				g_otg_pet_status = OTG_SET_NHP;
			}
			//printk(KERN_ERR "[OTG_H]get otg status = %d\n", ret);
			continue;
		}
		if(g_otg_pet_status == OTG_SET_NHP){
			ret = f_set_hnp();
			if(ret == RET_FAIL){
				printk(KERN_ERR "[OTG_H]============================Device No Response=====================");
				printk(KERN_ERR "[OTG_H] suspending port\n");	
				//usbif_u3h_test_send_event(USBIF_OTG_EVENT_NO_RESP_FOR_HNP_ENABLE);
				//suspend bus
				port_id = 1;
				xhci = hcd_to_xhci(my_hcd);
				addr = &xhci->op_regs->port_status_base + NUM_PORT_REGS*(port_id-1 & 0xff);
				temp = xhci_readl(xhci, addr);
				xhci_dbg(xhci, "[OTG_H]before reset port %d = 0x%x\n", port_id-1, temp);
				temp = mtktest_xhci_port_state_to_neutral(temp);
				temp = (temp & ~(0xf << 5));
				temp = (temp | (3 << 5) | PORT_LINK_STROBE);
				xhci_writel(xhci, temp, addr);
				while(g_port_connect){
					msleep(2) ;
					mb() ;
				}
				printk(KERN_ERR "[OTG_H] Device disconnected\n");
				ret = f_disable_slot();
				if(ret){
					printk(KERN_ERR "[OTG_H][FAIL] disable slot failed!!!!!!!!!!\n");
					//goto FAIL_RETURN ; 
				}
				g_otg_pet_status = OTG_DISCONNECTED;
				g_otg_slot_enabled = false;
				continue;
			}
			printk(KERN_ERR "[OTG_H]set hnp done\n");
			g_otg_pet_status = OTG_HNP_INIT; 
			continue;
		}

		if(g_otg_pet_status == OTG_HNP_INIT){
			g_otg_hnp_become_dev = false;
			g_port_resume = 0;
			g_otg_iddig_toggled = false;	//prevent PET toggle IDDIG when in device mode
			mb() ;
			//suspend bus
			port_id = 1;
			xhci = hcd_to_xhci(my_hcd);
			addr = &xhci->op_regs->port_status_base + NUM_PORT_REGS*(port_id-1 & 0xff);
			temp = xhci_readl(xhci, addr);
			xhci_dbg(xhci, "[OTG_H]before reset port %d = 0x%x\n", port_id-1, temp);
			temp = mtktest_xhci_port_state_to_neutral(temp);
			temp = (temp & ~(0xf << 5));
			temp = (temp | (3 << 5) | PORT_LINK_STROBE);
			xhci_writel(xhci, temp, addr);
			
			g_otg_pet_status = OTG_HNP_SUSPEND;
			mb() ;
			printk(KERN_ERR "[OTG_H]suspend port\n");
		}
		if(g_otg_pet_status == OTG_HNP_SUSPEND){
			
		}
		if((g_otg_pet_status == OTG_HNP_DEV) && g_otg_hnp_become_host){
			printk(KERN_ERR "[OTG_H]Back to become host again\n");
#if 0
			vbus_on_timeout = 500;
			while((!g_port_connect || !g_port_reset) && (vbus_on_timeout>0)){
				if(g_otg_iddig == 1 ){
					printk(KERN_ERR "[OTG_H] IDDIG is up, just goto disconnect state\n");
					vbus_on_timeout = 0;
					break;
				}
				vbus_on_timeout--;
				msleep(1);
			}
	
			if(vbus_on_timeout == 0){
				printk(KERN_ERR "[OTG_H] Not connected after role change back to host, back to DISCONNECT state\n");
				g_otg_csc = false;
				g_port_reset = false;
				g_otg_pet_status = OTG_DISCONNECTED;
				continue;
			}
#endif
			while(!g_port_reset && !g_otg_iddig_toggled && g_port_connect){
				msleep(2) ;
				mb() ;
			}
			if(g_otg_iddig_toggled){
				printk(KERN_ERR "[OTG_H] IDDIG just toggled, go to DISCONNECTED\n");
				g_otg_csc = false;
				g_port_reset = false;
				g_otg_pet_status = OTG_DISCONNECTED;
				mb() ;
				continue;
			}
			if(!g_port_connect){
				printk(KERN_ERR "[OTG_H] Device disconnected, go to DISCONNECTED\n");
				g_otg_csc = false;
				g_port_reset = false;
				g_otg_pet_status = OTG_DISCONNECTED;
				mb() ;
				continue;
			}
			printk(KERN_ERR "[OTG_H]Device attached and reset\n");
			g_otg_csc = false;
			g_port_reset = false;
			g_otg_unsupported_dev = false;
			mb() ;
			if(!g_port_connect){
				printk(KERN_ERR "[OTG_H] Device disconnected\n");
				g_otg_csc = false;
				g_otg_pet_status = OTG_DISCONNECTED;
				mb() ;
				continue;
			}
			 f_disable_slot();
			if(f_enable_slot(NULL) != RET_SUCCESS){
				printk(KERN_ERR "[OTG_H][FAIL] Enable slot failed\n");
				//goto FAIL_RETURN ; 
			}
			g_otg_slot_enabled = true;
			mb() ;
			if(!g_port_connect){
				printk(KERN_ERR "[OTG_H] Device disconnected\n");
				g_otg_csc = false;
				ret = f_disable_slot();
				if(ret){
					printk(KERN_ERR "[OTG_H][FAIL] disable slot failed!!!!!!!!!!\n");
					//goto FAIL_RETURN ; 
				}
				g_otg_pet_status = OTG_DISCONNECTED;
				g_otg_slot_enabled = false;
				mb() ;
				continue;
			}
			if(f_address_slot(true, NULL) != RET_SUCCESS){
				printk(KERN_ERR "[OTG_H][FAIL] Address device failed\n");
				//goto FAIL_RETURN ; 
			}
			if(!g_port_connect){
				printk(KERN_ERR "[OTG_H] Device disconnected\n");
				g_otg_csc = false;
				ret = f_disable_slot();
				if(ret){
					printk(KERN_ERR "[OTG_H][FAIL] disable slot failed!!!!!!!!!!\n");
					//goto FAIL_RETURN ; 
				}
				g_otg_pet_status = OTG_DISCONNECTED;
				g_otg_slot_enabled = false;
				mb() ;
				continue;
			}
			f_getdescriptor();
			if(!g_port_connect){
				printk(KERN_ERR "[OTG_H] Device disconnected\n");
				g_otg_csc = false;
				ret = f_disable_slot();
				if(ret){
					printk(KERN_ERR "[OTG_H][FAIL] disable slot failed!!!!!!!!!!\n");
					//goto FAIL_RETURN ; 
				}
				g_otg_pet_status = OTG_DISCONNECTED;
				g_otg_slot_enabled = false;
				mb() ;
				continue;
			}
			printk(KERN_ERR "[OTG_H]Get descriptor done\n");
			if(g_otg_unsupported_dev){
				//waiting for disconnect
				while(g_port_connect){
					msleep(2) ;
				}
				printk(KERN_ERR "[OTG_H] Port disconnected\n");
				ret = f_disable_slot();
				if(ret){
					printk(KERN_ERR "[OTG_H][FAIL] disable slot failed!!!!!!!!!!\n");
					//goto FAIL_RETURN ; 
				}
				g_otg_csc = false;
				g_otg_pet_status = OTG_DISCONNECTED;
				g_otg_slot_enabled = false;
				mb() ;
				continue;
			}
			if(f_address_slot(false, NULL) != RET_SUCCESS){
				printk(KERN_ERR "[OTG_H][FAIL] Address device failed\n");
				//goto FAIL_RETURN ; 
			}
			if(!g_port_connect){
				printk(KERN_ERR "[OTG_H] Device disconnected\n");
				g_otg_csc = false;
				ret = f_disable_slot();
				if(ret){
					printk(KERN_ERR "[OTG_H][FAIL] disable slot failed!!!!!!!!!!\n");
					//goto FAIL_RETURN ; 
				}
				g_otg_pet_status = OTG_DISCONNECTED;
				g_otg_slot_enabled = false;
				mb() ;
				continue;
			}
			f_getdescriptor();
			printk(KERN_ERR "[OTG_H]Get descriptor done\n");
			if(!g_port_connect){
				printk(KERN_ERR "[OTG_H] Device disconnected\n");
				g_otg_csc = false;
				ret = f_disable_slot();
				if(ret){
					printk(KERN_ERR "[OTG_H][FAIL] disable slot failed!!!!!!!!!!\n");
					//goto FAIL_RETURN ; 
				}
				g_otg_pet_status = OTG_DISCONNECTED;
				g_otg_slot_enabled = false;
				mb() ;
				continue;
			}
			f_getconfiguration_desc();
			printk(KERN_ERR "[OTG_H]Get configuration done\n");
			f_get_string_desc();
			printk(KERN_ERR "[OTG_H]Get string descirptor done\n");
			if(g_otg_dev_conf_len == 21){
				//otg13
				//set host hnp
				printk(KERN_ERR "[OTG_H] OTG13 set A_HNP_SUPPORT\n");
				f_set_a_hnp();
			}
			f_set_configuration();
			printk(KERN_ERR "[OTG_H]Set configuration done\n");
			if(g_otg_dev_conf_len == 23)
				g_otg_pet_status = OTG_POLLING_STATUS;
			else
				g_otg_pet_status = OTG_SET_NHP;

			mb() ;
			continue;
		}	
	}

	FAIL_RETURN :

	g_exec_done = true;  // let f_test_lib_cleanup to get the thread done info
	mb() ;
	
	printk(KERN_ERR "[OTG_H] Exit PET UUT A thread\n");

	return 0 ;
}

int otg_pet_uut_b(void *data){
	u32 temp;
	int ret;
	u32 __iomem *addr;
	int port_id;
	struct xhci_hcd *xhci;
	
	xhci_usbif_resource_get();			

	g_otg_test = true;
	g_otg_dev_B = true;
	g_otg_hnp_become_host = false;
	g_stopped = false;
	g_exec_done = false ;
	mb() ;
	//enable OTG interrupt
	temp = readl(SSUSB_OTG_INT_EN);
	temp = temp | SSUSB_ATTACH_A_ROLE_INT_EN | SSUSB_CHG_A_ROLE_A_INT_EN 
		| SSUSB_CHG_B_ROLE_A_INT_EN | SSUSB_SRP_REQ_INTR_EN;
    writel(temp, SSUSB_OTG_INT_EN);
	printk(KERN_ERR "[OTG_H] enter u3h_uut_B_host thread\n");

	//set host sel
	printk(KERN_ERR "[OTG_H] going to set dma to host\n");
	while((readl(SSUSB_OTG_STS) & 0x2000) == 0x2000){  // wait device to change role to host
		msleep(2) ;
	}
	printk(KERN_ERR "[OTG_H] can set dma to host\n");
    temp = readl(SSUSB_U2_CTRL(0));
    temp = temp | SSUSB_U2_PORT_HOST_SEL;
    writel(temp, SSUSB_U2_CTRL(0));	
	if(f_test_lib_init() != RET_SUCCESS){
		printk(KERN_ERR "[OTG_H] controller driver init failed\n");
		goto FAIL_RETURN ;
	}
	//g_hs_block_reset = true;
	g_hs_block_reset = false;
	mb() ;
	if(g_hs_block_reset){
		writel((readl(SSUSB_U2_SYS_BASE+0xc)|0x100), SSUSB_U2_SYS_BASE+0xc);
	}
	else{
		writel((readl(SSUSB_U2_SYS_BASE+0xc)&(~0x100)), SSUSB_U2_SYS_BASE+0xc);
    }
	//USBIF , HCRST should not be software reset for U2 MAC port0	
	// writel((readl(0xf00409b4) & ~(0x10000)), 0xf00409b4);  // HCRST_U2_MAC_EN_0P RW PUBLIC , host下HCRST的時候不會去reset MAC2
	//writel((readl(SSUSB_U3_XHCI_BASE + 0x9b4) & ~(0x10000)), SSUSB_U3_XHCI_BASE + 0x9b4);
	port_id = 1;
	xhci = hcd_to_xhci(my_hcd);
	addr = &xhci->cap_regs->hc_capbase + NUM_PORT_REGS*(port_id-1 & 0xff);
	addr += 0x9b4 ;
	writel((readl(addr) & ~(0x10000)), addr);
	
	addr = SIFSLV_IPPC + 0xc8 ;
	writel((readl(addr) |0x1fd0000), addr); // enable USBIF OTG20 timing

	
	g_otg_pet_status = OTG_DEV;	
	g_otg_hnp_become_host = false;
	g_otg_wait_con = false;
	mb() ;
	while(g_stopped == false){
		msleep(1);
		mb() ;
		if(g_otg_pet_status == OTG_DEV){
			mb() ;
			if(g_otg_hnp_become_host){  // recieve SSUSB_CHG_A_ROLE_A from U3D test driver
				//become host
				mb() ;
				printk(KERN_ERR "[OTG_H] Become host , g_otg_wait_con is %d , g_port_connect is %d\n",g_otg_wait_con, g_port_connect);
				if(!g_otg_wait_con && !g_port_connect){
					printk(KERN_ERR "[OTG_H]Disconnected, back to device\n");
					#if 0
					port_id = 1;
					xhci = hcd_to_xhci(my_hcd);
					addr = &xhci->op_regs->port_status_base + NUM_PORT_REGS*(port_id-1 & 0xff);
					temp = xhci_readl(xhci, addr);
					xhci_dbg(xhci, "[OTG_H]before reset port %d = 0x%x\n", port_id-1, temp);
					temp = mtktest_xhci_port_state_to_neutral(temp);
					temp = (temp & ~(0xf << 5));
					temp = (temp | (3 << 5) | PORT_LINK_STROBE); // set PORT to U3 state , suspend 
					xhci_writel(xhci, temp, addr);
					g_otg_pet_status = OTG_HNP_SUSPEND;
					printk(KERN_ERR "[OTG_H]suspend port\n");
					
					
					#endif
					//ret = f_disable_slot();

					mtktest_disableXhciAllPortPower(xhci);					
					g_otg_hnp_become_host = false;
					mb() ;
#if 0					
					printk(KERN_ERR "[OTG_H]alert device\n");
				 	//set B_role_B
					while(readl(SSUSB_OTG_STS) & SSUSB_XHCI_MAS_DMA_REQ){
						msleep(1);
						printk(KERN_ERR "[OTG_H] DMA in used\n");
					}
					writel(SSUSB_CHG_B_ROLE_B, SSUSB_OTG_STS);
					printk(KERN_ERR "[OTG_H]alert device done\n");
#endif					
					continue;
				}
				if(g_port_connect && g_port_reset){
					printk(KERN_ERR "[OTG_H] Device attached\n");
					msleep(200); // avoid doing the follow action with the fast disconnection case
					if (g_otg_hnp_become_dev){
						printk(KERN_ERR "[OTG_H] early disconnectd, skip enumeration\n");
						writel(SSUSB_CHG_B_ROLE_B, SSUSB_OTG_STS); // set B ROLE B to notify U3D back to device mode
						mtktest_disableXhciAllPortPower(xhci);											
						g_otg_hnp_become_host = false;
						mb() ;						
						continue ;
					}

					printk(KERN_ERR "[OTG_H] start enumeration device\n");
					if (f_disable_slot() == RET_FAIL){
						g_otg_hnp_become_host = false;
						g_otg_pet_status = OTG_HNP_DISCONNECTED;
						continue;
					}
					
					if(f_enable_slot(NULL) != RET_SUCCESS){
						printk(KERN_ERR "[OTG_H][FAIL] Enable slot failed\n");
						//goto FAIL_RETURN ;
					}						
					printk(KERN_ERR "[OTG_H] enable slot done\n");
					if(!g_port_connect){
						printk(KERN_ERR "[OTG_H]Disconnect event, handle it\n");
						g_otg_pet_status = OTG_HNP_DISCONNECTED;
						mb() ;
						continue;
					}
					f_address_slot(true,NULL);
					printk(KERN_ERR "[OTG_H] address slot with BSR done\n");
					mb() ;
					if(!g_port_connect){
						printk(KERN_ERR "[OTG_H]Disconnect event, handle it\n");
						g_otg_pet_status = OTG_HNP_DISCONNECTED;
						mb() ;
						continue;
					}
					f_getdescriptor();
					printk(KERN_ERR "[OTG_H]Get descriptor done\n");
					if(f_address_slot(false, NULL) != RET_SUCCESS){
						printk(KERN_ERR "[OTG_H][FAIL] Address device failed\n");
						//goto FAIL_RETURN ;
					}
					if(!g_port_connect){
						printk(KERN_ERR "[OTG_H]Disconnect event, handle it\n");
						g_otg_pet_status = OTG_HNP_DISCONNECTED;
						mb() ;
						continue;
					}
					f_getdescriptor();
					printk(KERN_ERR "[OTG_H]Get descriptor done\n");
					if(!g_port_connect){
						printk(KERN_ERR "[OTG_H]Disconnect event, handle it\n");
						g_otg_pet_status = OTG_HNP_DISCONNECTED;
						mb() ;
						continue;
					}
					f_getconfiguration_desc();
					printk(KERN_ERR "[OTG_H]Get configuration done\n");
					if(!g_port_connect){
						printk(KERN_ERR "[OTG_H]Disconnect event, handle it\n");
						g_otg_pet_status = OTG_HNP_DISCONNECTED;
						mb() ;
						continue;
					}
					f_get_string_desc();
					printk(KERN_ERR "[OTG_H]Get string descirptor done\n");
					if(!g_port_connect){
						printk(KERN_ERR "[OTG_H]Disconnect event, handle it\n");
						g_otg_pet_status = OTG_HNP_DISCONNECTED;
						mb() ;
						continue;
					}
					f_set_configuration();
					printk(KERN_ERR "[OTG_H]Set configuration done\n");
					if(!g_port_connect){
						printk(KERN_ERR "[OTG_H]Disconnect event, handle it\n");
						g_otg_pet_status = OTG_HNP_DISCONNECTED;
						mb() ;
						continue;
					}
					g_otg_pet_status = OTG_POLLING_STATUS;
					g_otg_hnp_become_dev = false;
					g_port_resume = 0;
					mb() ;

					while(readl(SSUSB_OTG_STS) & SSUSB_XHCI_MAS_DMA_REQ){
						msleep(1);
						printk(KERN_ERR "[OTG_H] DMA in used\n");
					}
					//suspend bus
					port_id = 1;
					xhci = hcd_to_xhci(my_hcd);
					addr = &xhci->op_regs->port_status_base + NUM_PORT_REGS*(port_id-1 & 0xff);
					temp = xhci_readl(xhci, addr);
					xhci_dbg(xhci, "[OTG_H]before reset port %d = 0x%x\n", port_id-1, temp);
					temp = mtktest_xhci_port_state_to_neutral(temp);
					temp = (temp & ~(0xf << 5));
					temp = (temp | (3 << 5) | PORT_LINK_STROBE); // set PORT to U3 state , suspend 
					xhci_writel(xhci, temp, addr);
					g_otg_pet_status = OTG_HNP_SUSPEND;
					mb() ;
					printk(KERN_ERR "[OTG_H]suspend port\n");
					continue;
				}
			}
		}
		mb() ;
		if((g_otg_pet_status != OTG_DEV) && (g_otg_hnp_become_dev)){ // get SSUSB_CHG_B_ROLE_A, and prepare beeing back to device mode 
			printk(KERN_ERR "[OTG_H]Become device\n");			
			//ret = f_disable_slot();
			//if(ret){
			//	printk(KERN_ERR "[OTG_H][FAIL] disable slot failed!!!!!!!!!!\n");
			//	goto FAIL_RETURN ;
			//}

			mtktest_disableXhciAllPortPower(xhci);	
			if(g_otg_pet_status == OTG_HNP_SUSPEND){
				g_otg_hnp_become_host = false;
				g_otg_pet_status = OTG_DEV;
				g_port_connect = false;
				g_port_reset = false;
				mb() ;
				printk(KERN_ERR "[OTG_H]alert device, g_port_connect is %d\n", g_port_connect);
				//set B_role_B
				writel(SSUSB_CHG_B_ROLE_B, SSUSB_OTG_STS); // set B ROLE B to notify U3D back to device mode
				//continue;
			}
			

		}
		mb() ;
		if((g_otg_pet_status != OTG_DEV) && (!g_port_connect)){
			printk(KERN_ERR "[OTG_H]Device disconnected after enumeration, back to device first\n");

			mtktest_disableXhciAllPortPower(xhci);	
			g_otg_hnp_become_host = false;
			g_otg_pet_status = OTG_DEV;
			g_port_reset = false;
			mb() ;
			printk(KERN_ERR "[OTG_H]alert device, g_port_connect is %d\n",g_port_connect);
			//set B_role_B
			writel(SSUSB_CHG_B_ROLE_B, SSUSB_OTG_STS);
			ret = f_disable_slot();
			//if(ret){
			//	printk(KERN_ERR "[OTG_H][FAIL] disable slot failed!!!!!!!!!!\n");
			//	goto FAIL_RETURN ;
			//}			
			continue;
		}
		mb() ;
		if(g_otg_pet_status == OTG_HNP_DISCONNECTED){  // disconnect during enumeration, back to device mode 
			printk(KERN_ERR "[OTG_H]Device disconnected when enumerating, back to device first\n");

			mtktest_disableXhciAllPortPower(xhci);	
			g_otg_hnp_become_host = false;
			g_otg_pet_status = OTG_DEV;
			g_port_connect = false;
			g_port_reset = false;
			mb() ;
			printk(KERN_ERR "[OTG_H]alert device, g_port_connect is %d\n", g_port_connect);
			//set B_role_B
			writel(SSUSB_CHG_B_ROLE_B, SSUSB_OTG_STS); // set B ROLE B to notify U3D back to device mode
			//ret = f_disable_slot();
			//if(ret){
			//	printk(KERN_ERR "[OTG_H][FAIL] disable slot failed!!!!!!!!!!\n");
			//	goto FAIL_RETURN ;
			//}			
			continue;
		}
	}

	FAIL_RETURN :

	g_exec_done = true;  // let f_test_lib_cleanup to get the thread done info
	mb() ;
	
	printk(KERN_ERR "[OTG_H] Exit PET UUT A thread\n");
}
