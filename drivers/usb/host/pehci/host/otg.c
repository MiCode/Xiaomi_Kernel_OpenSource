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
* This is a host controller driver file. OTG related events are handled here.
* 
* Author : wired support <wired.support@stericsson.com>
*
*/


/*hub device which connected with root port*/
struct usb_device *hubdev = 0;
/* hub interrupt urb*/
struct urb *huburb;

/*return otghub from here*/
struct usb_device *
phci_register_otg_device(struct isp1763_dev *dev)
{
	printk("OTG dev %x %d\n",(u32) hubdev, hubdev->devnum);
	if (hubdev && hubdev->devnum >= 0x2) {
		return hubdev;
	}

	return NULL;
}
EXPORT_SYMBOL(phci_register_otg_device);

/*suspend the otg port(0)
 * needed when port is switching
 * from host to device
 * */
int
phci_suspend_otg_port(struct isp1763_dev *dev, u32 command)
{
	int status = 0;
	hubdev->otgstate = USB_OTG_SUSPEND;
	if (huburb->status == -EINPROGRESS) {
		huburb->status = 0;
	}

	huburb->status = 0;
	huburb->complete(huburb);
	return status;
}
EXPORT_SYMBOL(phci_suspend_otg_port);

/*set the flag to enumerate the device*/
int
phci_enumerate_otg_port(struct isp1763_dev *dev, u32 command)
{
	/*set the flag to enumerate */
	/*connect change interrupt will happen from
	 * phci_intl_worker only
	 * */
	hubdev->otgstate = USB_OTG_ENUMERATE;
	if (huburb->status == -EINPROGRESS) {
		huburb->status = 0;
	}
	/*complete the urb */

	huburb->complete(huburb);

	/*reset the otghub urb status */
	huburb->status = -EINPROGRESS;
	return 0;
}
EXPORT_SYMBOL(phci_enumerate_otg_port);

/*host controller resume sequence at otg port*/
int
phci_resume_otg_port(struct isp1763_dev *dev, u32 command)
{
	printk("Resume is called\n");
	hubdev->otgstate = USB_OTG_RESUME;
	if (huburb->status == -EINPROGRESS) {
		huburb->status = 0;
	}
	/*complete the urb */

	huburb->complete(huburb);

	/*reset the otghub urb status */
	huburb->status = -EINPROGRESS;
	return 0;
}
EXPORT_SYMBOL(phci_resume_otg_port);
/*host controller remote wakeup sequence at otg port*/
int
phci_remotewakeup(struct isp1763_dev *dev)
{
    printk("phci_remotewakeup_otg_port is called\n");
    hubdev->otgstate = USB_OTG_REMOTEWAKEUP;
    if(huburb->status == -EINPROGRESS)
        huburb->status = 0;
    /*complete the urb*/
#if ((defined LINUX_269) || defined (LINUX_2611))
    huburb->complete(huburb,NULL);      
#else
	 huburb->complete(huburb);
#endif
    /*reset the otghub urb status*/
    huburb->status = -EINPROGRESS;
    return 0;
}
EXPORT_SYMBOL(phci_remotewakeup);

/*host controller wakeup sequence at otg port*/
int
phci_resume_wakeup(struct isp1763_dev *dev)
{
    printk("phci_wakeup_otg_port is called\n");
#if 0
    hubdev->otgstate = USB_OTG_WAKEUP_ALL;
    if(huburb->status == -EINPROGRESS)
#endif
        huburb->status = 0;
    /*complete the urb*/
#if ((defined LINUX_269) || defined (LINUX_2611))
    huburb->complete(huburb,NULL);      
#else
	 huburb->complete(huburb);
#endif
    /*reset the otghub urb status*/
    huburb->status = -EINPROGRESS;
    return 0;
}
EXPORT_SYMBOL(phci_resume_wakeup);

struct isp1763_driver *host_driver;
struct isp1763_driver *device_driver;

void
pehci_delrhtimer(struct isp1763_dev *dev)
{

	struct usb_hcd *usb_hcd =
		container_of(huburb->dev->parent->bus, struct usb_hcd, self);
	del_timer_sync(&usb_hcd->rh_timer);
	del_timer(&usb_hcd->rh_timer);

}
EXPORT_SYMBOL(pehci_delrhtimer);

int
pehci_Deinitialize(struct isp1763_dev *dev)
{
	dev -= 2;
	if (dev->index == 0) {
		if (dev->driver) {
			if (dev->driver->powerdown) {
				dev->driver->powerdown(dev);
			}
		}
	}
return 0;
}
EXPORT_SYMBOL(pehci_Deinitialize);

int
pehci_Reinitialize(struct isp1763_dev *dev)
{

	dev -= 2;
	if (dev->index == 0) {
		if(dev->driver->powerup){
			dev->driver->powerup(dev);
		}
	}
return 0;
}
EXPORT_SYMBOL(pehci_Reinitialize);


