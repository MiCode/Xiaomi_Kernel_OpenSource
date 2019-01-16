//#include "mtk-usb-hcd.h"
//#include <linux/usb.h>
//#include <linux/usb/hcd.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <asm/unaligned.h>
#include "xhci.h"
#include "mtk-test.h"

#if 0
/* Device for a quirk */
#define PCI_VENDOR_ID_FRESCO_LOGIC	0x1b73
#define PCI_DEVICE_ID_FRESCO_LOGIC_PDK	0x1000
#endif

/* FIXME tune these based on pool statistics ... */
static const size_t	pool_max [HCD_BUFFER_POOLS] = {
	/* platforms without dma-friendly caches might need to
	 * prevent cacheline sharing...
	 */
	32,
	128,
	512,
	PAGE_SIZE / 2
	/* bigger --> allocate pages */
};

#if 0
/**
 * hcd_buffer_create - initialize buffer pools
 * @hcd: the bus whose buffer pools are to be initialized
 * Context: !in_interrupt()
 *
 * Call this as part of initializing a host controller that uses the dma
 * memory allocators.  It initializes some pools of dma-coherent memory that
 * will be shared by all drivers using that controller, or returns a negative
 * errno value on error.
 *
 * Call hcd_buffer_destroy() to clean up after using those pools.
 */
int hcd_buffer_create(struct usb_hcd *hcd)
{
	char		name[16];
	int 		i, size;

	if (!hcd->self.controller->dma_mask &&
	    !(hcd->driver->flags & HCD_LOCAL_MEM))
		return 0;

	for (i = 0; i < HCD_BUFFER_POOLS; i++) {
		size = pool_max[i];
		if (!size)
			continue;
		snprintf(name, sizeof name, "buffer-%d", size);
		hcd->pool[i] = dma_pool_create(name, hcd->self.controller,
				size, size, 0);
		if (!hcd->pool [i]) {
			hcd_buffer_destroy(hcd);
			return -ENOMEM;
		}
	}
	return 0;
}
#endif
#if 0
/**
 * hcd_buffer_destroy - deallocate buffer pools
 * @hcd: the bus whose buffer pools are to be destroyed
 * Context: !in_interrupt()
 *
 * This frees the buffer pools created by hcd_buffer_create().
 */
void hcd_buffer_destroy(struct usb_hcd *hcd)
{
	int i;

	for (i = 0; i < HCD_BUFFER_POOLS; i++) {
		struct dma_pool *pool = hcd->pool[i];
		if (pool) {
			dma_pool_destroy(pool);
			hcd->pool[i] = NULL;
		}
	}
}
#endif
#if 0
/**
 * usb_hcd_irq - hook IRQs to HCD framework (bus glue)
 * @irq: the IRQ being raised
 * @__hcd: pointer to the HCD whose IRQ is being signaled
 *
 * If the controller isn't HALTed, calls the driver's irq handler.
 * Checks whether the controller is now dead.
 */
irqreturn_t usb_hcd_irq (int irq, void *__hcd)
{
	struct usb_hcd		*hcd = __hcd;
	unsigned long		flags;
	irqreturn_t		rc;

	/* IRQF_DISABLED doesn't work correctly with shared IRQs
	 * when the first handler doesn't use it.  So let's just
	 * assume it's never used.
	 */
	local_irq_save(flags);
	if (unlikely(hcd->state == HC_STATE_HALT ||
		     !test_bit(HCD_FLAG_HW_ACCESSIBLE, &hcd->flags))) {
		rc = IRQ_NONE;
	} else if (hcd->driver->irq(hcd) == IRQ_NONE) {
		rc = IRQ_NONE;
	} else {
		set_bit(HCD_FLAG_SAW_IRQ, &hcd->flags);

		if (unlikely(hcd->state == HC_STATE_HALT))
			usb_hc_died(hcd);
		rc = IRQ_HANDLED;
	}

	local_irq_restore(flags);
	return rc;
}
#endif
#if 0
/* Returns 1 if @usb_bus is WUSB, 0 otherwise */
static unsigned usb_bus_is_wusb(struct usb_bus *bus)
{
	struct usb_hcd *hcd = container_of(bus, struct usb_hcd, self);
	return hcd->wireless;
}
#endif

struct usb_device *mtk_usb_alloc_dev(struct usb_device *parent, struct usb_bus *bus, unsigned port1){
	struct usb_device *dev;
	struct usb_hcd *usb_hcd = container_of(bus, struct usb_hcd, self);
	unsigned root_hub = 0;
	
	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return NULL;

	if (!usb_get_hcd(bus_to_hcd(bus))) {
		kfree(dev);
		return NULL;
	}
	device_initialize(&dev->dev);
	dev->children = kzalloc(31 * sizeof(struct usb_device *), GFP_KERNEL);
	
	/* Save readable and stable topology id, distinguishing devices
	 * by location for diagnostics, tools, driver model, etc.  The
	 * string is a path along hub ports, from the root.  Each device's
	 * dev->devpath will be stable until USB is re-cabled, and hubs
	 * are often labeled with these port numbers.  The name isn't
	 * as stable:  bus->busnum changes easily from modprobe order,
	 * cardbus or pci hotplugging, and so on.
	 */
	if (unlikely(!parent)) {
		dev->devpath[0] = '0';
		dev->route = 0;

		dev->dev.parent = bus->controller;
		dev_set_name(&dev->dev, "usb%d", bus->busnum);
		root_hub = 1;
	} else {
		/* match any labeling on the hubs; it's one-based */
		if (parent->devpath[0] == '0') {
			snprintf(dev->devpath, sizeof dev->devpath,	"%d", port1);
			/* Root ports are not counted in route string */
			dev->route = 0;
			printk(KERN_DEBUG "device attached on roothub\n");
		} else {
			snprintf(dev->devpath, sizeof dev->devpath,	"%s.%d", parent->devpath, port1);
			/* Route string assumes hubs have less than 16 ports */
			if (port1 < 15)
				dev->route = parent->route +
					(port1 << ((parent->level - 1)*4));
			else
				dev->route = parent->route +
					(15 << ((parent->level - 1)*4));
			printk(KERN_DEBUG "device route string %d\n", dev->route);
			printk(KERN_DEBUG "parent level %d\n", parent->level);
			printk(KERN_DEBUG "parent route string %d\n", parent->route);
		}

		dev->dev.parent = &parent->dev;
		dev_set_name(&dev->dev, "%d-%s", bus->busnum, dev->devpath);

		/* hub driver sets up TT records */
	}
	dev->portnum = port1;
	dev->bus = bus;
	dev->parent = parent;
	if (root_hub)	/* Root hub always ok [and always wired] */
		dev->authorized = 1;
	else {
		dev->authorized = usb_hcd->authorized_default;
//		dev->wusb = usb_bus_is_wusb(bus)? 1 : 0;
	}

	dev->ep0.desc.bLength = USB_DT_ENDPOINT_SIZE;
	dev->ep0.desc.bDescriptorType = USB_DT_ENDPOINT;
	dev->ep0.enabled = 1;
	dev->ep_in[0] = &dev->ep0;
	dev->ep_out[0] = &dev->ep0;
#if 0
	/* ep0 maxpacket comes later, from device descriptor */
	usb_enable_endpoint(dev, &dev->ep0, false);
#endif
	dev->can_submit = 1;
	
	return dev;
}
#if 0
void *hcd_buffer_alloc(
	struct usb_bus 	*bus,
	size_t			size,
	gfp_t			mem_flags,
	dma_addr_t		*dma
)
{
	struct usb_hcd		*hcd = bus_to_hcd(bus);
	int 			i;

	/* some USB hosts just use PIO */
	if (!bus->controller->dma_mask &&
	    !(hcd->driver->flags & HCD_LOCAL_MEM)) {
		*dma = ~(dma_addr_t) 0;
		return kmalloc(size, mem_flags);
	}

	for (i = 0; i < HCD_BUFFER_POOLS; i++) {
		if (size <= pool_max [i])
			return dma_pool_alloc(hcd->pool [i], mem_flags, dma);
	}
	return dma_alloc_coherent(hcd->self.controller, size, dma, mem_flags);
}

void hcd_buffer_free(
	struct usb_bus 	*bus,
	size_t			size,
	void 			*addr,
	dma_addr_t		dma
)
{
	struct usb_hcd		*hcd = bus_to_hcd(bus);
	int 			i;

	if (!addr)
		return;

	if (!bus->controller->dma_mask &&
	    !(hcd->driver->flags & HCD_LOCAL_MEM)) {
		kfree(addr);
		return;
	}

	for (i = 0; i < HCD_BUFFER_POOLS; i++) {
		if (size <= pool_max [i]) {
			dma_pool_free(hcd->pool [i], addr, dma);
			return;
		}
	}
	dma_free_coherent(hcd->self.controller, size, addr, dma);
}
#endif

/*
 * Some usb host controllers can only perform dma using a small SRAM area.
 * The usb core itself is however optimized for host controllers that can dma
 * using regular system memory - like pci devices doing bus mastering.
 *
 * To support host controllers with limited dma capabilites we provide dma
 * bounce buffers. This feature can be enabled using the HCD_LOCAL_MEM flag.
 * For this to work properly the host controller code must first use the
 * function dma_declare_coherent_memory() to point out which memory area
 * that should be used for dma allocations.
 *
 * The HCD_LOCAL_MEM flag then tells the usb code to allocate all data for
 * dma using dma_alloc_coherent() which in turn allocates from the memory
 * area pointed out with dma_declare_coherent_memory().
 *
 * So, to summarize...
 *
 * - We need "local" memory, canonical example being
 *   a small SRAM on a discrete controller being the
 *   only memory that the controller can read ...
 *   (a) "normal" kernel memory is no good, and
 *   (b) there's not enough to share
 *
 * - The only *portable* hook for such stuff in the
 *   DMA framework is dma_declare_coherent_memory()
 *
 * - So we use that, even though the primary requirement
 *   is that the memory be "local" (hence addressible
 *   by that device), not "coherent".
 *
 */

static int hcd_alloc_coherent(struct usb_bus *bus,
			      gfp_t mem_flags, dma_addr_t *dma_handle,
			      void **vaddr_handle, size_t size,
			      enum dma_data_direction dir)
{
	unsigned char *vaddr;

	vaddr = hcd_buffer_alloc(bus, size + sizeof(vaddr),
				 mem_flags, dma_handle);
	if (!vaddr)
		return -ENOMEM;

	/*
	 * Store the virtual address of the buffer at the end
	 * of the allocated dma buffer. The size of the buffer
	 * may be uneven so use unaligned functions instead
	 * of just rounding up. It makes sense to optimize for
	 * memory footprint over access speed since the amount
	 * of memory available for dma may be limited.
	 */
	put_unaligned((unsigned long)*vaddr_handle,
		      (unsigned long *)(vaddr + size));

	if (dir == DMA_TO_DEVICE)
		memcpy(vaddr, *vaddr_handle, size);

	*vaddr_handle = vaddr;
	return 0;
}

void hcd_free_coherent(struct usb_bus *bus, dma_addr_t *dma_handle,
			      void **vaddr_handle, size_t size,
			      enum dma_data_direction dir)
{
	unsigned char *vaddr = *vaddr_handle;

	vaddr = (void *)get_unaligned((unsigned long *)(vaddr + size));

	if (dir == DMA_FROM_DEVICE)
		memcpy(vaddr, *vaddr_handle, size);

	hcd_buffer_free(bus, size + sizeof(vaddr), *vaddr_handle, *dma_handle);

	*vaddr_handle = vaddr;
	*dma_handle = 0;
}

#if 0
void unmap_urb_for_dma(struct usb_hcd *hcd, struct urb *urb)
{
	enum dma_data_direction dir;

	dir = usb_urb_dir_in(urb) ? DMA_FROM_DEVICE : DMA_TO_DEVICE;
	hcd_free_coherent(urb->dev->bus,
			&urb->transfer_dma,
			&urb->transfer_buffer,
			urb->transfer_buffer_length,
			dir);

}


int map_urb_for_dma(struct usb_hcd *hcd, struct urb *urb,
			   gfp_t mem_flags)
{
	enum dma_data_direction dir;
	int ret = 0;
	dir = usb_urb_dir_in(urb) ? DMA_FROM_DEVICE : DMA_TO_DEVICE;
	/* Map the URB's buffers for DMA access.
	 * Lower level HCD code should use *_dma exclusively,
	 * unless it uses pio or talks to another transport,
	 * or uses the provided scatter gather list for bulk.
	 */
	ret = hcd_alloc_coherent(
					urb->dev->bus, mem_flags,
					&urb->transfer_dma,
					&urb->transfer_buffer,
					urb->transfer_buffer_length,
					dir);

	if (ret == 0)
		urb->transfer_flags |= URB_MAP_LOCAL;
	return ret;
}

void rh_port_clear_change(struct xhci_hcd *xhci, int port_id){
	u32 temp,status;
	u32 __iomem *addr;
	port_id--;
	status = 0;
	
	addr = &xhci->op_regs->port_status_base + NUM_PORT_REGS*(port_id & 0xff);
	temp = xhci_readl(xhci, addr);
	xhci_dbg(xhci, "to clear port change, actual port %d status  = 0x%x\n", port_id, temp);
	temp = mtktest_xhci_port_state_to_clear_change(temp);
	xhci_writel(xhci, temp, addr);
	temp = xhci_readl(xhci, addr);
	xhci_dbg(xhci, "clear port change, actual port %d status  = 0x%x\n", port_id, temp);
}

int rh_get_port_status(struct xhci_hcd *xhci, int port_id){
	u32 temp,status;
	u32 __iomem *addr;
	
	port_id--;
	status = 0;
	
	addr = &xhci->op_regs->port_status_base + NUM_PORT_REGS*(port_id & 0xff);
	temp = xhci_readl(xhci, addr);
	xhci_dbg(xhci, "get port status, actual port %d status  = 0x%x\n", port_id, temp);

	/* wPortChange bits */
	if (temp & PORT_CSC)
		status |= USB_PORT_STAT_C_CONNECTION << 16;
	if (temp & PORT_PEC)
		status |= USB_PORT_STAT_C_ENABLE << 16;
	if ((temp & PORT_OCC))
		status |= USB_PORT_STAT_C_OVERCURRENT << 16;
	if ((temp & PORT_RC))
		status |= USB_PORT_STAT_C_RESET << 16;
	if ((temp & PORT_PLC))
		status |= USB_PORT_STAT_C_SUSPEND << 16;
	/*
	 * FIXME ignoring suspend, reset, and USB 2.1/3.0 specific
	 * changes
	 */
	if (temp & PORT_CONNECT) {
		status |= USB_PORT_STAT_CONNECTION;
		status |= mtktest_xhci_port_speed(temp);
	}
	if (temp & PORT_PE)
		status |= USB_PORT_STAT_ENABLE;
	if (temp & PORT_OC)
		status |= USB_PORT_STAT_OVERCURRENT;
	if (temp & PORT_RESET)
		status |= USB_PORT_STAT_RESET;
	if (temp & PORT_POWER)
		status |= USB_PORT_STAT_POWER;
	xhci_dbg(xhci, "Get port status returned 0x%x\n", status);
	temp = mtktest_xhci_port_state_to_neutral(temp);
	xhci_writel(xhci, temp, addr);
	temp = xhci_readl(xhci, addr);
	xhci_dbg(xhci, "clear port change, actual port %d status  = 0x%x\n", port_id, temp);
#if 0
	put_unaligned(cpu_to_le32(status), (__le32 *) buf);
#endif
	return status;
}
#endif

int mtk_map_urb_for_dma(struct usb_hcd *hcd, struct urb *urb, gfp_t mem_flags){
	enum dma_data_direction dir;
	int ret = 0;
	dir = usb_urb_dir_in(urb) ? DMA_FROM_DEVICE : DMA_TO_DEVICE;
	/* Map the URB's buffers for DMA access.
	 * Lower level HCD code should use *_dma exclusively,
	 * unless it uses pio or talks to another transport,
	 * or uses the provided scatter gather list for bulk.
	 */
	ret = hcd_alloc_coherent(
					urb->dev->bus, mem_flags,
					&urb->transfer_dma,
					&urb->transfer_buffer,
					urb->transfer_buffer_length,
					dir);

	if (ret == 0)
		urb->transfer_flags |= URB_MAP_LOCAL;
	return ret;
}
void mtk_unmap_urb_for_dma(struct usb_hcd *hcd, struct urb *urb){
	enum dma_data_direction dir;

	dir = usb_urb_dir_in(urb) ? DMA_FROM_DEVICE : DMA_TO_DEVICE;
	hcd_free_coherent(urb->dev->bus,
			&urb->transfer_dma,
			&urb->transfer_buffer,
			urb->transfer_buffer_length,
			dir);
}

/**
 * usb_alloc_dev - usb device constructor (usbcore-internal)
 * @parent: hub to which device is connected; null to allocate a root hub
 * @bus: bus used to access the device
 * @port1: one-based index of port; ignored for root hubs
 * Context: !in_interrupt()
 *
 * Only hub drivers (including virtual root hub drivers for host
 * controllers) should ever call this.
 *
 * This call may not be used in a non-sleeping context.
 */
struct usb_device *mtk_usb_alloc_rhdev(struct usb_device *parent,
				 struct usb_bus *bus, unsigned port1)
{
	struct usb_device *dev;
	struct usb_hcd *usb_hcd = container_of(bus, struct usb_hcd, self);
	unsigned root_hub = 0;

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return NULL;

	if (!usb_get_hcd(bus_to_hcd(bus))) {
		kfree(dev);
		return NULL;
	}
	device_initialize(&dev->dev);
	dev->children = kzalloc(31 * sizeof(struct usb_device *), GFP_KERNEL);
	dev->dev.dma_mask = bus->controller->dma_mask;
	atomic_set(&dev->urbnum, 0);
	dev->can_submit = 1;

	/* Save readable and stable topology id, distinguishing devices
	 * by location for diagnostics, tools, driver model, etc.  The
	 * string is a path along hub ports, from the root.  Each device's
	 * dev->devpath will be stable until USB is re-cabled, and hubs
	 * are often labeled with these port numbers.  The name isn't
	 * as stable:  bus->busnum changes easily from modprobe order,
	 * cardbus or pci hotplugging, and so on.
	 */
	if (unlikely(!parent)) {
		dev->devpath[0] = '0';
		dev->route = 0;
		dev->dev.parent = bus->controller;
		dev_set_name(&dev->dev, "usb%d", bus->busnum);
		root_hub = 1;
	} 

	dev->portnum = port1;
	dev->bus = bus;
	dev->parent = parent;

	dev->authorized = 1;
	return dev;
}


/**
 * usb_add_hcd - finish generic HCD structure initialization and register
 * @hcd: the usb_hcd structure to initialize
 * @irqnum: Interrupt line to allocate
 * @irqflags: Interrupt type flags
 *
 * Finish the remaining parts of generic HCD initialization: allocate the
 * buffers of consistent memory, register the bus, request the IRQ line,
 * and call the driver's reset() and start() routines.
 */
int mtk_usb_add_hcd(struct usb_hcd *hcd,
		unsigned int irqnum, unsigned long irqflags)
{
	int retval;
	struct usb_device *rhdev;
	dev_info(hcd->self.controller, "%s\n", hcd->product_desc);

	hcd->authorized_default = hcd->wireless? 0 : 1;
	set_bit(HCD_FLAG_HW_ACCESSIBLE, &hcd->flags);

	/* HC is in reset state, but accessible.  Now do the one-time init,
	 * bottom up so that hcds can customize the root hubs before khubd
	 * starts talking to them.  (Note, bus id is assigned early too.)
	 */
	if ((retval = hcd_buffer_create(hcd)) != 0) {
		dev_dbg(hcd->self.controller, "pool alloc failed\n");
		return retval;
	}
	
	if ((rhdev = mtk_usb_alloc_rhdev(NULL, &hcd->self, 0)) == NULL) {
		dev_err(hcd->self.controller, "unable to allocate root hub\n");
		retval = -ENOMEM;
		goto err_allocate_root_hub;
	}
	hcd->self.root_hub = rhdev;
#if 0
	if ((retval = usb_register_bus(&hcd->self)) < 0)
		goto err_register_bus;

	if ((rhdev = usb_alloc_dev(NULL, &hcd->self, 0)) == NULL) {
		dev_err(hcd->self.controller, "unable to allocate root hub\n");
		retval = -ENOMEM;
		goto err_allocate_root_hub;
	}

	switch (hcd->driver->flags & HCD_MASK) {
	case HCD_USB11:
		rhdev->speed = USB_SPEED_FULL;
		break;
	case HCD_USB2:
		rhdev->speed = USB_SPEED_HIGH;
		break;
	case HCD_USB3:
		rhdev->speed = USB_SPEED_SUPER;
		break;
	default:
		goto err_allocate_root_hub;
	}
	hcd->self.root_hub = rhdev;

	/* wakeup flag init defaults to "everything works" for root hubs,
	 * but drivers can override it in reset() if needed, along with
	 * recording the overall controller's system wakeup capability.
	 */
	device_init_wakeup(&rhdev->dev, 1);
#endif

	/* "reset" is misnamed; its role is now one-time init. the controller
	 * should already have been reset (and boot firmware kicked off etc).
	 */
	printk(KERN_DEBUG "call xhci_mtk_setup\n");
	if (hcd->driver->reset && (retval = hcd->driver->reset(hcd)) < 0) {
		dev_err(hcd->self.controller, "can't setup\n");
		goto err_hcd_driver_setup;
	}
#if 0
	/* NOTE: root hub and controller capabilities may not be the same */
	if (device_can_wakeup(hcd->self.controller)
			&& device_can_wakeup(&hcd->self.root_hub->dev))
		dev_dbg(hcd->self.controller, "supports USB remote wakeup\n");
#endif
	/* enable irqs just before we start the controller */
	if (hcd->driver->irq) {

		/* IRQF_DISABLED doesn't work as advertised when used together
		 * with IRQF_SHARED. As usb_hcd_irq() will always disable
		 * interrupts we can remove it here.
		 */
		if (irqflags & IRQF_SHARED)
			irqflags &= ~IRQF_DISABLED;

		snprintf(hcd->irq_descr, sizeof(hcd->irq_descr), "%s:usb%d",
				hcd->driver->description, hcd->self.busnum);
		if ((retval = request_irq(irqnum, &usb_hcd_irq, irqflags,
				hcd->irq_descr, hcd)) != 0) {
			dev_err(hcd->self.controller,
					"request interrupt %d failed\n", irqnum);
			goto err_request_irq;
		}
		hcd->irq = irqnum;
		dev_info(hcd->self.controller, "irq %d, %s 0x%08llx\n", irqnum,
				(hcd->driver->flags & HCD_MEMORY) ?
					"io mem" : "io base",
					(unsigned long long)hcd->rsrc_start);
	} else {
		hcd->irq = -1;
		if (hcd->rsrc_start)
			dev_info(hcd->self.controller, "%s 0x%08llx\n",
					(hcd->driver->flags & HCD_MEMORY) ?
					"io mem" : "io base",
					(unsigned long long)hcd->rsrc_start);
	}

	if ((retval = hcd->driver->start(hcd)) < 0) {
		dev_err(hcd->self.controller, "startup error %d\n", retval);
		goto err_hcd_driver_start;
	}
#if 0
	/* starting here, usbcore will pay attention to this root hub */
	rhdev->bus_mA = min(500u, hcd->power_budget);
	if ((retval = register_root_hub(hcd)) != 0)
		goto err_register_root_hub;

	retval = sysfs_create_group(&rhdev->dev.kobj, &usb_bus_attr_group);
	if (retval < 0) {
		printk(KERN_ERR "Cannot register USB bus sysfs attributes: %d\n",
		       retval);
		goto error_create_attr_group;
	}
	if (hcd->uses_new_polling && hcd->poll_rh)
		usb_hcd_poll_rh_status(hcd);
#endif
	return retval;

error_create_attr_group:
	mutex_lock(&usb_bus_list_lock);
	usb_disconnect(&hcd->self.root_hub);
	mutex_unlock(&usb_bus_list_lock);
err_register_root_hub:
	hcd->driver->stop(hcd);
err_hcd_driver_start:
	if (hcd->irq >= 0)
		free_irq(irqnum, hcd);
err_request_irq:
err_hcd_driver_setup:
	hcd->self.root_hub = NULL;
	usb_put_dev(rhdev);
#if 1
err_allocate_root_hub:
	hcd->driver->stop(hcd);
#endif
err_register_bus:
	hcd_buffer_destroy(hcd);
	return retval;
} 


/**
 * usb_remove_hcd - shutdown processing for generic HCDs
 * @hcd: the usb_hcd structure to remove
 * Context: !in_interrupt()
 *
 * Disconnects the root hub, then reverses the effects of usb_add_hcd(),
 * invoking the HCD's stop() method.
 */
void mtk_usb_remove_hcd(struct usb_hcd *hcd)
{
	dev_info(hcd->self.controller, "remove, state %x\n", hcd->state);

	if (HC_IS_RUNNING (hcd->state))
		hcd->state = HC_STATE_QUIESCING;

	dev_dbg(hcd->self.controller, "roothub graceful disconnect\n");
#if 0
	spin_lock_irq (&hcd_root_hub_lock);
	hcd->rh_registered = 0;
	spin_unlock_irq (&hcd_root_hub_lock);
#endif
#if 0 //#ifdef CONFIG_USB_SUSPEND
	cancel_work_sync(&hcd->wakeup_work);
#endif
#if 0
	sysfs_remove_group(&hcd->self.root_hub->dev.kobj, &usb_bus_attr_group);
	mutex_lock(&usb_bus_list_lock);
	usb_disconnect(&hcd->self.root_hub);
	mutex_unlock(&usb_bus_list_lock);
#endif
	hcd->driver->stop(hcd);
	hcd->state = HC_STATE_HALT;
#if 0
	hcd->poll_rh = 0;
	del_timer_sync(&hcd->rh_timer);
#endif
	if (hcd->irq >= 0)
		free_irq(hcd->irq, hcd);
#if 0
	usb_deregister_bus(&hcd->self);
#endif
	hcd_buffer_destroy(hcd);
}
/**
 * usb_bus_init - shared initialization code
 * @bus: the bus structure being initialized
 *
 * This code is used to initialize a usb_bus structure, memory for which is
 * separately managed.
 */
static void mtk_usb_bus_init (struct usb_bus *bus)
{
	memset (&bus->devmap, 0, sizeof(struct usb_devmap));

	bus->devnum_next = 1;

	bus->root_hub = NULL;
	bus->busnum = -1;
	bus->bandwidth_allocated = 0;
	bus->bandwidth_int_reqs  = 0;
	bus->bandwidth_isoc_reqs = 0;

	INIT_LIST_HEAD (&bus->bus_list);
}

/*-------------------------------------------------------------------------*/

/**
 * usb_create_hcd - create and initialize an HCD structure
 * @driver: HC driver that will use this hcd
 * @dev: device for this HC, stored in hcd->self.controller
 * @bus_name: value to store in hcd->self.bus_name
 * Context: !in_interrupt()
 *
 * Allocate a struct usb_hcd, with extra space at the end for the
 * HC driver's private data.  Initialize the generic members of the
 * hcd structure.
 *
 * If memory is unavailable, returns NULL.
 */
struct usb_hcd *mtk_usb_create_hcd (const struct hc_driver *driver,
		struct device *dev, const char *bus_name)
{
	struct usb_hcd *hcd;

	hcd = kzalloc(sizeof(*hcd) + driver->hcd_priv_size, GFP_KERNEL);
	if (!hcd) {
		dev_dbg (dev, "hcd alloc failed\n");
		return NULL;
	}

    hcd->bandwidth_mutex = kmalloc(sizeof(*hcd->bandwidth_mutex), GFP_KERNEL);
	if (!hcd->bandwidth_mutex) {
			kfree(hcd);
			dev_dbg(dev, "hcd bandwidth mutex alloc failed\n");
			return NULL;
	}
    mutex_init(hcd->bandwidth_mutex);
    
    printk(KERN_ERR "====%s(%d)==== hcd->pool[0] = 0x%p!\n", __func__, __LINE__, hcd->pool[0]);
	dev_set_drvdata(dev, hcd);
	kref_init(&hcd->kref);
    
    printk(KERN_ERR "====%s(%d)==== hcd->pool[0] = 0x%p!\n", __func__, __LINE__, hcd->pool[0]);
    
	mtk_usb_bus_init(&hcd->self);
	hcd->self.controller = dev;
	hcd->self.bus_name = bus_name;
	hcd->self.uses_dma = (dev->dma_mask != NULL);

    printk(KERN_ERR "====%s(%d)==== hcd->pool[0] = 0x%p!\n", __func__, __LINE__, hcd->pool[0]);
    
//	init_timer(&hcd->rh_timer);
//	hcd->rh_timer.function = rh_timer_func;
//	hcd->rh_timer.data = (unsigned long) hcd;
#if 0 //#ifdef CONFIG_USB_SUSPEND
	INIT_WORK(&hcd->wakeup_work, hcd_resume_work);
#endif
    printk(KERN_ERR "====%s(%d)==== hcd->pool[0] = 0x%p!\n", __func__, __LINE__, hcd->pool[0]);
	hcd->driver = driver;
	hcd->product_desc = (driver->product_desc) ? driver->product_desc :
			"USB Host Controller";
	return hcd;
}


/* Find the flag for this endpoint (for use in the control context).  Use the
 * endpoint index to create a bitmask.  The slot context is bit 0, endpoint 0 is
 * bit 1, etc.
 */
unsigned int mtktest_xhci_get_endpoint_flag(struct usb_endpoint_descriptor *desc)
{
	return 1 << (mtktest_xhci_get_endpoint_index(desc) + 1);
}

