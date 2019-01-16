#include "mtk-protocol.h"
#include "mtk-test-lib.h"
#include <linux/random.h>


//#define MTK_PROTO_DBG

#ifdef MTK_PROTO_DBG
#define mtk_proto_dbg(fmt, args...) \
	do { printk( KERN_ERR "%s(%d):" fmt, __func__, __LINE__, ##args); } while (0)
#else
#define mtk_proto_dbg(fmt, args...)
#endif



int dev_query_result()
{
	int ret;
	struct usb_ctrlrequest *dr;
	struct usb_device *udev, *rhdev;
	struct urb *urb;
	struct protocol_query *query;

	rhdev = my_hcd->self.root_hub;
	udev = rhdev->children[g_port_id-1];

	dr = kmalloc(sizeof(struct usb_ctrlrequest), GFP_NOIO);
	query= kmalloc(AT_CMD_ACK_DATA_LENGTH, GFP_NOIO);

	memset(query, 0, AT_CMD_ACK_DATA_LENGTH);

	dr->bRequestType = USB_DIR_IN | USB_TYPE_VENDOR | USB_RECIP_DEVICE;;
	dr->bRequest = AT_CMD_ACK;
	dr->wValue = cpu_to_le16(0);
	dr->wIndex = cpu_to_le16(0);
	dr->wLength = cpu_to_le16(AT_CMD_ACK_DATA_LENGTH);

//	ret=hcd_ctrl_request(dr,ptr);
	urb = alloc_ctrl_urb(dr, query, udev);
	ret = f_ctrlrequest(urb, udev);

	memcpy(query, urb->transfer_buffer, AT_CMD_ACK_DATA_LENGTH);
	if(ret != RET_SUCCESS){
		printk(KERN_ERR "[DEV]query status ctrl request failed!!\n");
		ret = STATUS_FAIL;
	}
	else{
		ret = query->result;
	}
	kfree(dr);
	kfree(query);
//	kfree(urb);
	usb_free_urb(urb);
	return ret;
}

int dev_polling_result(){
	int i, value;
	int count = POLLING_COUNT;
	int delay_msecs = POLLING_STOP_DELAY_MSECS;

	for(i=0; i<count; i++){
		value=dev_query_result();
		printk(KERN_INFO "polling device status: %d !!\n", value);
		msleep(delay_msecs);
		if(value != STATUS_BUSY)
			break;
	}
	return value;
}


int dev_query_status(struct usb_device *dev)
{
	int ret;
	struct usb_ctrlrequest *dr;
	struct usb_device *udev, *rhdev;
	struct urb *urb;
	struct protocol_query *query;

	if(dev){
		udev = dev;
	}
	else{
		rhdev = my_hcd->self.root_hub;
		udev = rhdev->children[g_port_id-1];
	}

	dr = kmalloc(sizeof(struct usb_ctrlrequest), GFP_NOIO);
	query= kmalloc(AT_CMD_ACK_DATA_LENGTH, GFP_NOIO);

	memset(query, 0, AT_CMD_ACK_DATA_LENGTH);

	dr->bRequestType = USB_DIR_IN | USB_TYPE_VENDOR | USB_RECIP_DEVICE;;
	dr->bRequest = AT_CMD_ACK;
	dr->wValue = cpu_to_le16(0);
	dr->wIndex = cpu_to_le16(0);
	dr->wLength = cpu_to_le16(AT_CMD_ACK_DATA_LENGTH);

//	ret=hcd_ctrl_request(dr,ptr);
	urb = alloc_ctrl_urb(dr, query, udev);
	ret = f_ctrlrequest(urb, udev);

	memcpy(query, urb->transfer_buffer, AT_CMD_ACK_DATA_LENGTH);
	if(ret != RET_SUCCESS){
		printk(KERN_ERR "[DEV]query status ctrl request failed!!\n");
		ret = STATUS_FAIL;
	}
	else{
		ret = query->status;
	}
	kfree(dr);
	kfree(query);
//	kfree(urb);
	usb_free_urb(urb);
	return ret;
}

int dev_polling_status(struct usb_device *dev){
	int i, value;
	int count = POLLING_COUNT;
	int delay_msecs = POLLING_DELAY_MSECS;

	for(i=0; i<count; i++){
		value = dev_query_status(dev);
//		printk(KERN_INFO "polling device status: %d !!\n", value);
		msleep(delay_msecs);
		if(value != STATUS_BUSY)
			break;
	}

    mtk_proto_dbg("status(%d)", value);
	return value;
}

void phy_hsrx_reset(void);
int dev_reset(USB_DEV_SPEED speed, struct usb_device *dev){
	struct usb_device *udev, *rhdev;
	struct usb_ctrlrequest *dr;
	struct urb *urb;
	int ret;
	char *ptr;

	if(dev){
		udev = dev;
	}
	else{
		rhdev = my_hcd->self.root_hub;
		udev = rhdev->children[g_port_id-1];
	}

	dr = kmalloc(sizeof(struct usb_ctrlrequest), GFP_NOIO);
	ptr= kmalloc(2048, GFP_NOIO);

	memset(ptr, 0, RESET_STATE_DATA_LENGTH);
	*ptr=0x55;
	*(ptr+1)=0xAA;
	*(ptr+2)=RESET_STATE_DATA_LENGTH&0xFF;
	*(ptr+3)=RESET_STATE_DATA_LENGTH>>8;
	*(ptr+4)=RESET_STATE&0xFF;
	*(ptr+5)=RESET_STATE>>8;
	*(ptr+6)=speed;

	dr->bRequestType = USB_DIR_OUT | USB_TYPE_VENDOR | USB_RECIP_DEVICE;;
	dr->bRequest = AT_CMD_SET;
	dr->wValue = cpu_to_le16(0);
	dr->wIndex = cpu_to_le16(0);
	dr->wLength = cpu_to_le16(RESET_STATE_DATA_LENGTH);

	urb = alloc_ctrl_urb(dr, ptr, udev);
	ret = f_ctrlrequest(urb, udev);
//		ret=hcd_ctrl_request(dr,ptr);

	kfree(dr);
	kfree(ptr);
	usb_free_urb(urb);
//	kfree(urb);

	if(ret)
	{
		printk(KERN_ERR "[DEV]reset device ctrl request failed!!\n");
		return RET_FAIL;
	}

    phy_hsrx_reset();
	return RET_SUCCESS;
}

int dev_config_ep0(short int maxp, struct usb_device *usbdev){
	struct usb_device *udev, *rhdev;
	struct urb *urb;
	struct usb_ctrlrequest *dr;
	char *ptr;
	int ret;

	if(!usbdev){
		rhdev = my_hcd->self.root_hub;
		udev = rhdev->children[g_port_id-1];
	}
	else{
		udev = usbdev;
	}

	dr = kmalloc(sizeof(struct usb_ctrlrequest), GFP_NOIO);
	ptr= kmalloc(2048, GFP_NOIO);

	memset(ptr, 0, CONFIGEP_STATE_DATA_LENGTH);

	*ptr=0x55;
	*(ptr+1)=0xAA;
	*(ptr+2)=CONFIGEP_STATE_DATA_LENGTH&0xFF;
	*(ptr+3)=CONFIGEP_STATE_DATA_LENGTH>>8;
	*(ptr+4)=CONFIGEP_STATE&0xFF;
	*(ptr+5)=CONFIGEP_STATE>>8;
	*(ptr+6)=0;	//ep_num
	*(ptr+7)=0;	//dir
	*(ptr+8)=EPATT_CTRL; //transfer_type
	*(ptr+9)=0; //interval
	*(ptr+10)=maxp&0xFF; //maxp
	*(ptr+11)=(maxp>>8); //maxp
	*(ptr+12)=0; //mult

	dr->bRequestType = USB_DIR_OUT | USB_TYPE_VENDOR | USB_RECIP_DEVICE;;
	dr->bRequest = AT_CMD_SET;
	dr->wValue = cpu_to_le16(0);
	dr->wIndex = cpu_to_le16(0);
	dr->wLength = cpu_to_le16(CONFIGEP_STATE_DATA_LENGTH);

//		ret=hcd_ctrl_request(dr,ptr);
	urb = alloc_ctrl_urb(dr, ptr, udev);
	ret = f_ctrlrequest(urb, udev);

	kfree(dr);
	kfree(ptr);
	usb_free_urb(urb);
//	kfree(urb);

	if(ret)
	{
		printk(KERN_ERR "[DEV]config ep ctrl request failed!!\n");
		return RET_FAIL;
	}

	ret=dev_polling_status(udev);
	//mdelay(200);

	if(ret)
	{
		printk(KERN_ERR "query request fail!!\n");
		return RET_FAIL;
	}

	return RET_SUCCESS;
}
int dev_config_ep(char ep_num,char dir,char type,short int maxp,char bInterval
,char slot, char burst, char mult, struct usb_device *dev){
	struct usb_device *udev, *rhdev;
	struct urb *urb;
	struct usb_ctrlrequest *dr;
	char *ptr;
	int ret;

	rhdev = my_hcd->self.root_hub;
	if(dev){
		udev = dev;
	}
	else{
		udev = rhdev->children[g_port_id-1];
	}

	dr = kmalloc(sizeof(struct usb_ctrlrequest), GFP_NOIO);
	ptr= kmalloc(2048, GFP_NOIO);

	memset(ptr, 0, CONFIGEP_STATE_DATA_LENGTH);
	*ptr=0x55;
	*(ptr+1)=0xAA;
	*(ptr+2)=CONFIGEP_STATE_DATA_LENGTH&0xFF;
	*(ptr+3)=CONFIGEP_STATE_DATA_LENGTH>>8;
	*(ptr+4)=CONFIGEP_STATE&0xFF;
	*(ptr+5)=CONFIGEP_STATE>>8;
	*(ptr+6)=ep_num;
	*(ptr+7)=dir;
	*(ptr+8)=type;
	*(ptr+9)=bInterval;
	*(ptr+10)=maxp&0xFF;
	*(ptr+11)=(maxp>>8);
	*(ptr+12)=slot;
	*(ptr+13)=burst;
	*(ptr+14)=mult;

	dr->bRequestType = USB_DIR_OUT | USB_TYPE_VENDOR | USB_RECIP_DEVICE;;
	dr->bRequest = AT_CMD_SET;
	dr->wValue = cpu_to_le16(0);
	dr->wIndex = cpu_to_le16(0);
	dr->wLength = cpu_to_le16(CONFIGEP_STATE_DATA_LENGTH);

//		ret=hcd_ctrl_request(dr,ptr);
	urb = alloc_ctrl_urb(dr, ptr, udev);
	ret = f_ctrlrequest(urb, udev);

	kfree(dr);
	kfree(ptr);
	usb_free_urb(urb);
//	kfree(urb);

	if(ret)
	{
		printk(KERN_ERR "[DEV]config ep ctrl request failed!!\n");
		return RET_FAIL;
	}

	ret = dev_polling_status(udev);
	//mdelay(200);

	if(ret)
	{
		printk(KERN_ERR "query request fail!!\n");
		return RET_FAIL;
	}

	return RET_SUCCESS;
}

int dev_ctrl_transfer(char dir,int length,char *buffer, struct usb_device *dev){
	struct usb_device *udev, *rhdev;
	struct urb *urb;
	struct usb_ctrlrequest *dr;
	//char *ptr;
	int ret;

	rhdev = my_hcd->self.root_hub;
	if(dev){
		udev = dev;
	}
	else{
		udev = rhdev->children[g_port_id-1];
	}

	dr = kmalloc(sizeof(struct usb_ctrlrequest), GFP_NOIO);
	dr->bRequestType = dir | USB_TYPE_VENDOR | USB_RECIP_DEVICE;;
	dr->bRequest = AT_CTRL_TEST;
	dr->wValue = cpu_to_le16(0);
	dr->wIndex = cpu_to_le16(0);
	dr->wLength = cpu_to_le16(length);

//		ret=hcd_ctrl_request(dr,ptr);
	urb = alloc_ctrl_urb(dr, buffer, udev);

	ret = f_ctrlrequest(urb, udev);



//	memcpy(buffer, urb->transfer_buffer, length);

	//printk(KERN_ERR "urb->transfer_buffer :%x\n",urb->transfer_buffer);
	//printk(KERN_ERR "buffer :%x\n",buffer);


	kfree(dr);
	usb_free_urb(urb);

	if(ret)
	{
		printk(KERN_ERR "[DEV]config ep ctrl request failed!!\n");
		return RET_FAIL;
	}
/*
	ret=dev_polling_status(udev);

	if(ret)
	{
		printk(KERN_ERR "query request fail!!\n");
		return RET_FAIL;
	}
*/
	return RET_SUCCESS;
}


int dev_ctrl_loopback(int length, struct usb_device *dev){
	char *ptr1,*ptr2;
	int ret,i;

	ptr1= kmalloc(length, GFP_NOIO);
	get_random_bytes(ptr1, length);
	ret=dev_ctrl_transfer(USB_DIR_OUT,length,ptr1,dev);

	if(ret)
	{
		printk(KERN_ERR "ctrl loopback fail!!\n");
		return RET_FAIL;
	}
	ptr2= kmalloc(length, GFP_NOIO);
	memset(ptr2, 0, length);

	ret=dev_ctrl_transfer(USB_DIR_IN,length,ptr2,dev);

	if(ret)
	{
		printk(KERN_ERR "ctrl loopback fail!!\n");
		return RET_FAIL;
	}

	for(i=0; i<length; i++){
		if((*(ptr1+i)) != (*(ptr2+i))){
			printk(KERN_ERR "[ERROR] buffer %d not match, tx 0x%x, rx 0x%x\n", i, *(ptr1+i), *(ptr2+i));
			break;
		}
	}

	kfree(ptr1);
	kfree(ptr2);

	return RET_SUCCESS;
}

int dev_loopback(char bdp,int length,int gpd_buf_size,int bd_buf_size, char dram_offset, char extension, struct usb_device *dev){
	struct usb_device *udev, *rhdev;
	struct usb_ctrlrequest *dr;
	char *ptr;
	int ret;
	struct urb *urb;

	int dma_burst = 3;
	int dma_limiter = 3;

	rhdev = my_hcd->self.root_hub;
	if(dev){
		udev = dev;
	}
	else{
		udev = rhdev->children[g_port_id-1];
	}

	dr = kmalloc(sizeof(struct usb_ctrlrequest), GFP_NOIO);
	ptr= kmalloc(2048, GFP_NOIO);

	memset(ptr, 0, LOOPBACK_STATE_DATA_LENGTH);
	*ptr=0x55;
	*(ptr+1)=0xAA;
	*(ptr+2)=LOOPBACK_STATE_DATA_LENGTH&0xFF;
	*(ptr+3)=LOOPBACK_STATE_DATA_LENGTH>>8;
	*(ptr+4)=LOOPBACK_STATE&0xFF;
	*(ptr+5)=LOOPBACK_STATE>>8;
	*(ptr+6)=length&0xFF;
	*(ptr+7)=(length>>8)&0xFF;
	*(ptr+8)=(length>>16)&0xFF;
	*(ptr+9)=(length>>24)&0xFF;
	*(ptr+10)=gpd_buf_size&0xFF;
	*(ptr+11)=(gpd_buf_size>>8)&0xFF;
	*(ptr+12)=(gpd_buf_size>>16)&0xFF;
	*(ptr+13)=(gpd_buf_size>>24)&0xFF;
	*(ptr+14)=bd_buf_size&0xFF;
	*(ptr+15)=(bd_buf_size>>8)&0xFF;
	*(ptr+16)=bdp;
	*(ptr+17)=dram_offset;
	*(ptr+18)=extension;
	*(ptr+19)=dma_burst;
	*(ptr+20)=dma_limiter;


	dr->bRequestType = USB_DIR_OUT | USB_TYPE_VENDOR | USB_RECIP_DEVICE;;
	dr->bRequest = AT_CMD_SET;
	dr->wValue = cpu_to_le16(0);
	dr->wIndex = cpu_to_le16(0);
	dr->wLength = cpu_to_le16(LOOPBACK_STATE_DATA_LENGTH);

	urb = alloc_ctrl_urb(dr, ptr, udev);
	ret = f_ctrlrequest(urb, udev);
//		ret=hcd_ctrl_request(dr,ptr);

	kfree(dr);
	kfree(ptr);
	usb_free_urb(urb);
//	kfree(urb);

	if(ret)
	{
		printk(KERN_ERR "ctrl request fail!!\n");
		return RET_FAIL;
	}

	return RET_SUCCESS;
}
#if 0
int dev_remotewakeup(char bdp,int length,int gpd_buf_size,int bd_buf_size, struct usb_device *dev){
	struct usb_device *udev, *rhdev;
	struct usb_ctrlrequest *dr;
	char *ptr;
	int ret;
	struct urb *urb;

	rhdev = my_hcd->self.root_hub;
	if(dev){
		udev = dev;
	}
	else{
		udev = rhdev->children[g_port_id-1];
	}

	dr = kmalloc(sizeof(struct usb_ctrlrequest), GFP_NOIO);
	ptr= kmalloc(2048, GFP_NOIO);

	memset(ptr, 0, LOOPBACK_STATE_DATA_LENGTH);

	*ptr=0x55;
	*(ptr+1)=0xAA;
	*(ptr+2)=LOOPBACK_STATE_DATA_LENGTH&0xFF;
	*(ptr+3)=LOOPBACK_STATE_DATA_LENGTH>>8;
	*(ptr+4)=REMOTE_WAKEUP&0xFF;
	*(ptr+5)=REMOTE_WAKEUP>>8;
	*(ptr+6)=length&0xFF;
	*(ptr+7)=(length>>8)&0xFF;
	*(ptr+8)=(length>>16)&0xFF;
	*(ptr+9)=(length>>24)&0xFF;
	*(ptr+10)=gpd_buf_size&0xFF;
	*(ptr+11)=(gpd_buf_size>>8)&0xFF;
	*(ptr+12)=bd_buf_size&0xFF;
	*(ptr+13)=(bd_buf_size>>8)&0xFF;
	*(ptr+14)=bdp;


	dr->bRequestType = USB_DIR_OUT | USB_TYPE_VENDOR | USB_RECIP_DEVICE;;
	dr->bRequest = AT_CMD_SET;
	dr->wValue = cpu_to_le16(0);
	dr->wIndex = cpu_to_le16(0);
	dr->wLength = cpu_to_le16(LOOPBACK_STATE_DATA_LENGTH);

	urb = alloc_ctrl_urb(dr, ptr, udev);
	ret = f_ctrlrequest(urb, udev);
//		ret=hcd_ctrl_request(dr,ptr);

	kfree(dr);
	kfree(ptr);
	usb_free_urb(urb);
//	kfree(urb);

	if(ret)
	{
		printk(KERN_ERR "ctrl request fail!!\n");
		return RET_FAIL;
	}

	return RET_SUCCESS;
}
#endif
#if 1 //for concurrently resume test
int dev_remotewakeup(int delay){
        struct usb_device *udev, *rhdev;
        struct usb_ctrlrequest *dr;
        char *ptr;
        int ret;
        struct urb *urb;

        rhdev = my_hcd->self.root_hub;
        udev = rhdev->children[g_port_id-1];

        dr = kmalloc(sizeof(struct usb_ctrlrequest), GFP_NOIO);
        ptr= kmalloc(2048, GFP_NOIO);

        memset(ptr, 0, REMOTE_WAKEUP_DATA_LENGTH);

        *ptr=0x55;
        *(ptr+1)=0xAA;
        *(ptr+2)=REMOTE_WAKEUP_DATA_LENGTH&0xFF;
        *(ptr+3)=REMOTE_WAKEUP_DATA_LENGTH>>8;
        *(ptr+4)=REMOTE_WAKEUP&0xFF;
        *(ptr+5)=REMOTE_WAKEUP>>8;
        *(ptr+6)=delay&0xFF;
        *(ptr+7)=(delay>>8)&0xFF;

        dr->bRequestType = USB_DIR_OUT | USB_TYPE_VENDOR | USB_RECIP_DEVICE;;
        dr->bRequest = AT_CMD_SET;
        dr->wValue = cpu_to_le16(0);
        dr->wIndex = cpu_to_le16(0);
        dr->wLength = cpu_to_le16(REMOTE_WAKEUP_DATA_LENGTH);

        urb = alloc_ctrl_urb(dr, ptr, udev);
        ret = f_ctrlrequest(urb, udev);

        kfree(dr);
        kfree(ptr);
        usb_free_urb(urb);

        if(ret)
        {
                printk(KERN_ERR "ctrl request fail!!\n");
                return RET_FAIL;
        }

        return RET_SUCCESS;
}

#endif
int dev_stress(char bdp,int length,int gpd_buf_size,int bd_buf_size,char num, struct usb_device *usbdev){
	struct usb_device *udev, *rhdev;
	struct usb_ctrlrequest *dr;
	char *ptr;
	int ret;
	struct urb *urb;

	if(!usbdev){
		rhdev = my_hcd->self.root_hub;
		udev = rhdev->children[g_port_id-1];
	}
	else{
		udev = usbdev;
	}

	dr = kmalloc(sizeof(struct usb_ctrlrequest), GFP_NOIO);
	ptr= kmalloc(2048, GFP_NOIO);
	memset(ptr, 0, STRESS_DATA_LENGTH);
	*ptr=0x55;
	*(ptr+1)=0xAA;
	*(ptr+2)=STRESS_DATA_LENGTH&0xFF;
	*(ptr+3)=STRESS_DATA_LENGTH>>8;
	*(ptr+4)=STRESS&0xFF;
	*(ptr+5)=STRESS>>8;
	*(ptr+6)=length&0xFF;
	*(ptr+7)=(length>>8)&0xFF;
	*(ptr+8)=(length>>16)&0xFF;
	*(ptr+9)=(length>>24)&0xFF;
	*(ptr+10)=gpd_buf_size&0xFF;
	*(ptr+11)=(gpd_buf_size>>8)&0xFF;
	*(ptr+12)=(gpd_buf_size>>16)&0xFF;
	*(ptr+13)=(gpd_buf_size>>24)&0xFF;
	*(ptr+14)=bd_buf_size&0xFF;
	*(ptr+15)=(bd_buf_size>>8)&0xFF;
	*(ptr+16)=bdp;
	*(ptr+17)=num;

	dr->bRequestType = USB_DIR_OUT | USB_TYPE_VENDOR | USB_RECIP_DEVICE;;
	dr->bRequest = AT_CMD_SET;
	dr->wValue = cpu_to_le16(0);
	dr->wIndex = cpu_to_le16(0);
	dr->wLength = cpu_to_le16(STRESS_DATA_LENGTH);

	urb = alloc_ctrl_urb(dr, ptr, udev);
	ret = f_ctrlrequest(urb, udev);
//		ret=hcd_ctrl_request(dr,ptr);

	kfree(dr);
	kfree(ptr);
	usb_free_urb(urb);
//	kfree(urb);

	if(ret)
	{
		printk(KERN_ERR "ctrl request fail!!\n");
		return RET_FAIL;
	}

	return RET_SUCCESS;
}


int dev_random_stop(int length,int gpd_buf_size,int bd_buf_size,char dev_dir_1,char dev_dir_2,int stop_count_1,int stop_count_2){
	struct usb_device *udev, *rhdev;
	struct usb_ctrlrequest *dr;
	char *ptr;
	int ret;
	struct urb *urb;

	rhdev = my_hcd->self.root_hub;
	udev = rhdev->children[g_port_id-1];

	dr = kmalloc(sizeof(struct usb_ctrlrequest), GFP_NOIO);
	ptr= kmalloc(2048, GFP_NOIO);
	memset(ptr, 0, RANDOM_STOP_STATE_DATA_LENGTH);
	*ptr=0x55;
	*(ptr+1)=0xAA;
	*(ptr+2)=RANDOM_STOP_STATE_DATA_LENGTH&0xFF;
	*(ptr+3)=RANDOM_STOP_STATE_DATA_LENGTH>>8;
	*(ptr+4)=RANDOM_STOP_STATE&0xFF;
	*(ptr+5)=RANDOM_STOP_STATE>>8;
	*(ptr+6)=length&0xFF;
	*(ptr+7)=(length>>8)&0xFF;
	*(ptr+8)=(length>>16)&0xFF;
	*(ptr+9)=(length>>24)&0xFF;
	*(ptr+10)=gpd_buf_size&0xFF;
	*(ptr+11)=(gpd_buf_size>>8)&0xFF;
	*(ptr+12)=(gpd_buf_size>>16)&0xFF;
	*(ptr+13)=(gpd_buf_size>>24)&0xFF;
	*(ptr+14)=bd_buf_size&0xFF;
	*(ptr+15)=(bd_buf_size>>8)&0xFF;
	*(ptr+16)=dev_dir_1;
	*(ptr+17)=dev_dir_2;
	*(ptr+18)=stop_count_1&0xFF;
	*(ptr+19)=(stop_count_1>>8)&0xFF;
	*(ptr+20)=(stop_count_1>>16)&0xFF;
	*(ptr+21)=(stop_count_1>>24)&0xFF;
	*(ptr+22)=stop_count_2&0xFF;
	*(ptr+23)=(stop_count_2>>8)&0xFF;
	*(ptr+24)=(stop_count_2>>16)&0xFF;
	*(ptr+25)=(stop_count_2>>24)&0xFF;

	dr->bRequestType = USB_DIR_OUT | USB_TYPE_VENDOR | USB_RECIP_DEVICE;;
	dr->bRequest = AT_CMD_SET;
	dr->wValue = cpu_to_le16(0);
	dr->wIndex = cpu_to_le16(0);
	dr->wLength = cpu_to_le16(RANDOM_STOP_STATE_DATA_LENGTH);

	urb = alloc_ctrl_urb(dr, ptr, udev);
	ret = f_ctrlrequest(urb, udev);
//		ret=hcd_ctrl_request(dr,ptr);

	kfree(dr);
	kfree(ptr);
	usb_free_urb(urb);
//	kfree(urb);

	if(ret)
	{
		printk(KERN_ERR "ctrl request fail!!\n");
		return RET_FAIL;
	}

	return RET_SUCCESS;
}

int dev_notifiaction(int type,int valuel,int valueh){
	struct usb_device *udev, *rhdev;
	struct usb_ctrlrequest *dr;
	char *ptr;
	int ret;
	struct urb *urb;

	rhdev = my_hcd->self.root_hub;
	udev = rhdev->children[g_port_id-1];

	dr = kmalloc(sizeof(struct usb_ctrlrequest), GFP_NOIO);
	ptr= kmalloc(2048, GFP_NOIO);
	memset(ptr, 0, DEV_NOTIFICATION_DATA_LENGTH);
	*ptr=0x55;
	*(ptr+1)=0xAA;
	*(ptr+2)=DEV_NOTIFICATION_DATA_LENGTH&0xFF;
	*(ptr+3)=DEV_NOTIFICATION_DATA_LENGTH>>8;
	*(ptr+4)=DEV_NOTIFICATION_STATE&0xFF;
	*(ptr+5)=DEV_NOTIFICATION_STATE>>8;
	*(ptr+6)=valuel&0xFF;
	*(ptr+7)=(valuel>>8)&0xFF;
	*(ptr+8)=(valuel>>16)&0xFF;
	*(ptr+9)=(valuel>>24)&0xFF;
	*(ptr+10)=valueh&0xFF;
	*(ptr+11)=(valueh>>8)&0xFF;
	*(ptr+12)=(valueh>>16)&0xFF;
	*(ptr+13)=(valueh>>24)&0xFF;
	*(ptr+14)=type&0xFF;


	dr->bRequestType = USB_DIR_OUT | USB_TYPE_VENDOR | USB_RECIP_DEVICE;;
	dr->bRequest = AT_CMD_SET;
	dr->wValue = cpu_to_le16(0);
	dr->wIndex = cpu_to_le16(0);
	dr->wLength = cpu_to_le16(DEV_NOTIFICATION_DATA_LENGTH);

	urb = alloc_ctrl_urb(dr, ptr, udev);
	ret = f_ctrlrequest(urb, udev);
//		ret=hcd_ctrl_request(dr,ptr);

	kfree(dr);
	kfree(ptr);
	usb_free_urb(urb);
//	kfree(urb);

	if(ret)
	{
		printk(KERN_ERR "ctrl request fail!!\n");
		return RET_FAIL;
	}

	return RET_SUCCESS;
}


int dev_power(int test_mode, char u1_value, char u2_value,char en_u1, char en_u2, struct usb_device *usbdev){
	struct usb_device *udev, *rhdev;
	struct usb_ctrlrequest *dr;
	char *ptr;
	int ret;
	int mode;
	struct urb *urb;

	if(usbdev == NULL){
		rhdev = my_hcd->self.root_hub;
		udev = rhdev->children[g_port_id-1];
	}
	else{
		udev = usbdev;
	}
	if(test_mode != 3){
		if((u1_value == 0) && (u2_value != 0)){
			mode = 2;
		}
		else if((u1_value != 0) && (u2_value == 0)){
			mode = 1;
		}
		else{
			mode = 0;
		}
	}
	else{
		mode = 3;
	}
	dr = kmalloc(sizeof(struct usb_ctrlrequest), GFP_NOIO);
	ptr= kmalloc(2048, GFP_NOIO);
	memset(ptr, 0, POWER_STATE_DATA_LENGTH);
	*ptr=0x55;
	*(ptr+1)=0xAA;
	*(ptr+2)=POWER_STATE_DATA_LENGTH&0xFF;
	*(ptr+3)=POWER_STATE_DATA_LENGTH>>8;
	*(ptr+4)=POWER_STATE&0xFF;
	*(ptr+5)=POWER_STATE>>8;
	*(ptr+6)=mode&0xFF;
	*(ptr+7)=(mode>>8)&0xFF;
	*(ptr+8)=(mode>>16)&0xFF;
	*(ptr+9)=(mode>>24)&0xFF;
	*(ptr+10)= u1_value&0xFF;
	*(ptr+11)= u2_value&0xFF;
	*(ptr+12)= en_u1&0xFF;
	*(ptr+13)= en_u2&0xFF;


	dr->bRequestType = USB_DIR_OUT | USB_TYPE_VENDOR | USB_RECIP_DEVICE;;
	dr->bRequest = AT_CMD_SET;
	dr->wValue = cpu_to_le16(0);
	dr->wIndex = cpu_to_le16(0);
	dr->wLength = cpu_to_le16(POWER_STATE_DATA_LENGTH);

	urb = alloc_ctrl_urb(dr, ptr, udev);
	ret = f_ctrlrequest(urb, udev);

	kfree(dr);
	kfree(ptr);
	usb_free_urb(urb);

	if(ret)
	{
		printk(KERN_ERR "ctrl request fail!!\n");
		return RET_FAIL;
	}

	return RET_SUCCESS;
}

int dev_lpm_config_host(char lpm_mode, char wakeup,
	char beslck, char beslck_u3, char beslckd, char cond, char cond_en){
	struct usb_device *udev, *rhdev;
	struct usb_ctrlrequest *dr;
	char *ptr;
	int ret;
	struct urb *urb;

	rhdev = my_hcd->self.root_hub;
	udev = rhdev->children[g_port_id-1];

	dr = kmalloc(sizeof(struct usb_ctrlrequest), GFP_NOIO);
	ptr= kmalloc(2048, GFP_NOIO);
	memset(ptr, 0, LPM_STATE_DATA_LENGTH);
	*ptr=0x55;
	*(ptr+1)=0xAA;
	*(ptr+2)=LPM_STATE_DATA_LENGTH&0xFF;
	*(ptr+3)=LPM_STATE_DATA_LENGTH>>8;
	*(ptr+4)=LPM_STATE&0xFF;
	*(ptr+5)=LPM_STATE>>8;

	*(ptr+6)=lpm_mode&0xFF;
	*(ptr+7)=wakeup&0xFF;
	*(ptr+8)=beslck&0xFF;
	*(ptr+9)=beslck_u3&0xFF;
	*(ptr+10)=beslckd&0xFF;
	*(ptr+11)=cond&0xFF;
	*(ptr+12)=cond_en&0xFF;

	dr->bRequestType = USB_DIR_OUT | USB_TYPE_VENDOR | USB_RECIP_DEVICE;;
	dr->bRequest = AT_CMD_SET;
	dr->wValue = cpu_to_le16(0);
	dr->wIndex = cpu_to_le16(0);
	dr->wLength = cpu_to_le16(LPM_STATE_DATA_LENGTH);

	urb = alloc_ctrl_urb(dr, ptr, udev);
	ret = f_ctrlrequest(urb, udev);

	kfree(dr);
	kfree(ptr);
	usb_free_urb(urb);

	if(ret)
	{
		printk(KERN_ERR "ctrl request fail!!\n");
		return RET_FAIL;
	}

	ret = dev_polling_status(udev);
	if(ret)
	{
		printk(KERN_ERR "device is still busy!!!\n");
		return RET_FAIL;
	}

	return RET_SUCCESS;
}


int dev_polling_stop_status(struct usb_device *dev){
	int i, value;
	int count = POLLING_COUNT;
	int delay_msecs = POLLING_STOP_DELAY_MSECS;

	for(i=0; i<count; i++){
		value=dev_query_result();
//		printk(KERN_INFO "polling device status: %d !!\n", value);
		msleep(delay_msecs);
		if(value != STATUS_BUSY)
			break;
	}
	return value;
}


