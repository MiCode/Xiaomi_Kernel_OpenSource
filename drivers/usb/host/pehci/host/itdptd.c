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
* This is a host controller driver file. Isochronous event processing is handled here.
* 
* Author : wired support <wired.support@stericsson.com>
*
*/
#ifdef CONFIG_ISO_SUPPORT
void phcd_clean_periodic_ep(void);
#endif

#ifdef CONFIG_ISO_SUPPORT

#define MAX_URBS		8
#define MAX_EPS			2/*maximum 2 endpoints supported in ISO transfers.*/
/*number of microframe per frame which is scheduled, for high speed device
* actually , NUMMICROFRAME should be 8 , but the micro frame #7 is fail , so
* there's just 4 microframe is used (#0 -> #4)
* Writer : LyNguyen - 25Nov09
*/
#define NUMMICROFRAME		8
struct urb *gstUrb_pending[MAX_URBS] = { 0, 0, 0, 0, 0, 0, 0, 0 };

struct usb_host_endpoint *periodic_ep[MAX_EPS];

int giUrbCount = 0;		/* count the pending urb*/
int giUrbIndex = 0;		/*the index of urb need to be scheduled next*/
/*
 * phcd_iso_sitd_to_ptd - convert an SITD into a PTD
 *
 * phci_hcd *hcd
 *      - Main host controller driver structure
 * struct ehci_sitd *sitd
 *  - Isochronous Transfer Descriptor, contains elements as defined by the
 *        EHCI standard plus a few more specific elements.
 * struct urb *urb
 *  - USB Request Block, contains information regarding the type and how much data
 *    is requested to be transferred.
 * void  * ptd
 *  - Points to the ISO ptd structure that needs to be initialized
 *
 * API Description
 * This is mainly responsible for:
 *  -Initializing the PTD that will be used for the ISO transfer
 */
void *
phcd_iso_sitd_to_ptd(phci_hcd * hcd,
	struct ehci_sitd *sitd, struct urb *urb, void *ptd)
{
	struct _isp1763_isoptd *iso_ptd;
	struct isp1763_mem_addr *mem_addr;

	unsigned long max_packet, mult, length, td_info1, td_info3;
	unsigned long token, port_num, hub_num, data_addr;
	unsigned long frame_number;

	iso_dbg(ISO_DBG_ENTRY, "phcd_iso_sitd_to_ptd entry\n");

	/* Variable initialization */
	iso_ptd = (struct _isp1763_isoptd *) ptd;
	mem_addr = &sitd->mem_addr;

	/*
	 * For both ISO and INT endpoints descriptors, new bit fields we added to
	 * specify whether or not the endpoint supports high bandwidth, and if so
	 * the number of additional packets that the endpoint can support during a
	 * single microframe.
	 * Bits 12:11 specify whether the endpoint supports high-bandwidth transfers
	 * Valid values:
	 *             00 None (1 transaction/uFrame)
	 *             01 1 additional transaction
	 *             10 2 additional transactions
	 *             11 reserved
	 */
	max_packet = usb_maxpacket(urb->dev, urb->pipe,usb_pipeout(urb->pipe));

	/*
	 * We need to add 1 since our Multi starts with 1 instead of the USB specs defined
	 * zero (0).
	 */
	mult = 1 + ((max_packet >> 11) & 0x3);
	max_packet &= 0x7ff;

	/* This is the size of the request (bytes to write or bytes to read) */
	length = sitd->length;

	/*
	 * Set V bit to indicate that there is payload to be sent or received. And
	 * indicate that the current PTD is active.
	 */
	td_info1 = QHA_VALID;

	/*
	 * Set the number of bytes that can be transferred by this PTD. This indicates
	 * the depth of the data field.
	 */
	td_info1 |= (length << 3);

	/*
	 * Set the maximum packet length which indicates the maximum number of bytes that
	 * can be sent to or received from the endpoint in a single data packet.
	 */
	if (urb->dev->speed != USB_SPEED_HIGH) {
		/*
		 * According to the ISP1763 specs for sITDs, OUT token max packet should
		 * not be more  than 188 bytes, while IN token max packet not more than
		 * 192 bytes (ISP1763 Rev 3.01, Table 72, page 79
		 */
		if (usb_pipein(urb->pipe) && (max_packet > 192)) {
			iso_dbg(ISO_DBG_INFO,
				"IN Max packet over maximum\n");
			max_packet = 192;
		}

		if ((!usb_pipein(urb->pipe)) && (max_packet > 188)) {
			iso_dbg(ISO_DBG_INFO,
				"OUT Max packet over maximum\n");
			max_packet = 188;
		}
	}
	td_info1 |= (max_packet << 18);

	/*
	 * Place the FIRST BIT of the endpoint number here.
	 */
	td_info1 |= (usb_pipeendpoint(urb->pipe) << 31);

	/*
	 * Set the number of successive packets the HC can submit to the endpoint.
	 */
	if (urb->dev->speed == USB_SPEED_HIGH) {
		td_info1 |= MULTI(mult);
	}

	/* Set the first DWORD */
	iso_ptd->td_info1 = td_info1;
	iso_dbg(ISO_DBG_DATA, "[phcd_iso_itd_to_ptd]: DWORD0 = 0x%08x\n",
		iso_ptd->td_info1);

	/*
	 * Since the first bit have already been added on the first DWORD of the PTD
	 * we only need to add the last 3-bits of the endpoint number.
	 */
	token = (usb_pipeendpoint(urb->pipe) & 0xE) >> 1;

	/*
	 * Get the device address and set it accordingly to its assigned bits of the 2nd
	 * DWORD.
	 */
	token |= usb_pipedevice(urb->pipe) << 3;

	/* See a split transaction is needed */
	if (urb->dev->speed != USB_SPEED_HIGH) {
		/*
		 * If we are performing a SPLIT transaction indicate that it is so by setting
		 * the S bit of the second DWORD.
		 */
		token |= 1 << 14;

		port_num = urb->dev->ttport;
		hub_num = urb->dev->tt->hub->devnum;

		/* Set the the port number of the hub or embedded TT */
		token |= port_num << 18;

		/*
		 * Set the hub address, this should be zero for the internal or
		 * embedded hub
		 */
		token |= hub_num << 25;
	}

	/* if(urb->dev->speed != USB_SPEED_HIGH) */
	/*
	 * Determine if the direction of this pipe is IN, if so set the Token bit of
	 * the second DWORD to indicate it as IN. Since it is initialized to zero and
	 * zero indicates an OUT token, then we do not need anything to the Token bit
	 * if it is an OUT token.
	 */
	if (usb_pipein(urb->pipe)) {
		token |= (IN_PID << 10);
	}

	/* Set endpoint type to Isochronous */
	token |= EPTYPE_ISO;

	/* Set the second DWORD */
	iso_ptd->td_info2 = token;
	iso_dbg(ISO_DBG_DATA, "[phcd_iso_itd_to_ptd]: DWORD1 = 0x%08x\n",
		iso_ptd->td_info2);

	/*
	 * Get the physical address of the memory location that was allocated for this PTD
	 * in the PAYLOAD region, using the formula indicated in sectin 7.2.2 of the ISP1763 specs
	 * rev 3.01 page 17 to 18.
	 */
	data_addr = ((unsigned long) (mem_addr->phy_addr) & 0xffff) - 0x400;
	data_addr >>= 3;

	/*  Set it to its location in the third DWORD */
	td_info3 =( 0xffff&data_addr) << 8;

	/*
	 * Set the frame number when this PTD will be sent for ISO OUT or IN
	 * Bits 0 to 2 are don't care, only bits 3 to 7.
	 */
	frame_number = sitd->framenumber;
	frame_number = sitd->start_frame;
	td_info3 |= (0xff& ((frame_number) << 3));

	/* Set the third DWORD */
	iso_ptd->td_info3 = td_info3;
	iso_dbg(ISO_DBG_DATA, "[phcd_iso_itd_to_ptd]: DWORD2 = 0x%08x\n",
		iso_ptd->td_info3);

	/*
	 * Set the A bit of the fourth DWORD to 1 to indicate that this PTD is active.
	 * This have the same functionality with the V bit of DWORD0
	 */
	iso_ptd->td_info4 = QHA_ACTIVE;
	iso_dbg(ISO_DBG_DATA, "[phcd_iso_itd_to_ptd]: DWORD3 = 0x%08x\n",
		iso_ptd->td_info4);

	/* Set the fourth DWORD to specify which uSOFs the start split needs to be placed */
	if (usb_pipein(urb->pipe)){
		iso_ptd->td_info5 = (sitd->ssplit);
	}else{
		iso_ptd->td_info5 = (sitd->ssplit << 2);
	}
	iso_dbg(ISO_DBG_DATA, "[phcd_iso_itd_to_ptd]: DWORD4 = 0x%08x\n",
		iso_ptd->td_info5);

	/*
	 * Set the fifth DWORD to specify which uSOFs the complete split needs to be sent.
	 * This is VALID only for IN (since ISO transfers don't have handshake stages)
	 */
	iso_ptd->td_info6 = sitd->csplit;
	iso_dbg(ISO_DBG_DATA, "[phcd_iso_itd_to_ptd]: DWORD5 = 0x%08x\n",
		iso_ptd->td_info6);

	/*printk(" [phcd_iso_itd_to_ptd]: DWORD0 = 0x%08x\n",iso_ptd->td_info1);
	printk(" [phcd_iso_itd_to_ptd]: DWORD1 = 0x%08x\n",iso_ptd->td_info2);
	printk(" [phcd_iso_itd_to_ptd]: DWORD2 = 0x%08x\n",iso_ptd->td_info3);
	printk(" [phcd_iso_itd_to_ptd]: DWORD3 = 0x%08x\n",iso_ptd->td_info4);
	printk(" [phcd_iso_itd_to_ptd]: DWORD4 = 0x%08x\n",iso_ptd->td_info5);
	printk(" [phcd_iso_itd_to_ptd]: DWORD5 = 0x%08x\n",iso_ptd->td_info6);*/
	iso_dbg(ISO_DBG_EXIT, "phcd_iso_itd_to_ptd exit\n");
	return iso_ptd;
}


/*
 * phcd_iso_itd_to_ptd - convert an ITD into a PTD
 *
 * phci_hcd *hcd
 *      - Main host controller driver structure
 * struct ehci_itd *itd
 *  - Isochronous Transfer Descriptor, contains elements as defined by the
 *        EHCI standard plus a few more ST-ERICSSON specific elements.
 * struct urb *urb
 *  - USB Request Block, contains information regarding the type and how much data
 *    is requested to be transferred.
 * void  * ptd
 *  - Points to the ISO ptd structure that needs to be initialized
 *
 * API Description
 * This is mainly responsible for:
 *  -Initializing the PTD that will be used for the ISO transfer
 */
void *
phcd_iso_itd_to_ptd(phci_hcd * hcd,
	struct ehci_itd *itd, struct urb *urb, void *ptd)
{
	struct _isp1763_isoptd *iso_ptd;
	struct isp1763_mem_addr *mem_addr;

	unsigned long max_packet, mult, length, td_info1, td_info3;
	unsigned long token, port_num, hub_num, data_addr;
	unsigned long frame_number;
	int maxpacket;
	iso_dbg(ISO_DBG_ENTRY, "phcd_iso_itd_to_ptd entry\n");

	/* Variable initialization */
	iso_ptd = (struct _isp1763_isoptd *) ptd;
	mem_addr = &itd->mem_addr;

	/*
	 * For both ISO and INT endpoints descriptors, new bit fields we added to
	 * specify whether or not the endpoint supports high bandwidth, and if so
	 * the number of additional packets that the endpoint can support during a
	 * single microframe.
	 * Bits 12:11 specify whether the endpoint supports high-bandwidth transfers
	 * Valid values:
	 *             00 None (1 transaction/uFrame)
	 *             01 1 additional transaction
	 *             10 2 additional transactions
	 *             11 reserved
	 */
	max_packet = usb_maxpacket(urb->dev, urb->pipe, usb_pipeout(urb->pipe));

	maxpacket = usb_maxpacket(urb->dev, urb->pipe, usb_pipeout(urb->pipe));	

	/*
	 * We need to add 1 since our Multi starts with 1 instead of the USB specs defined
	 * zero (0).
	 */
	maxpacket &= 0x7ff;
	mult = 1 + ((max_packet >> 11) & 0x3);


	max_packet &= 0x7ff;

	/* This is the size of the request (bytes to write or bytes to read) */
	length = itd->length;

	/*
	 * Set V bit to indicate that there is payload to be sent or received. And
	 * indicate that the current PTD is active.
	 */
	td_info1 = QHA_VALID;

	/*
	 * Set the number of bytes that can be transferred by this PTD. This indicates
	 * the depth of the data field.
	 */
	td_info1 |= (length << 3);

	/*
	 * Set the maximum packet length which indicates the maximum number of bytes that
	 * can be sent to or received from the endpoint in a single data packet.
	 */
	if (urb->dev->speed != USB_SPEED_HIGH) {
		/*
		 * According to the ISP1763 specs for sITDs, OUT token max packet should
		 * not be more  than 188 bytes, while IN token max packet not more than
		 * 192 bytes (ISP1763 Rev 3.01, Table 72, page 79
		 */
		if (usb_pipein(urb->pipe) && (max_packet > 192)) {
			iso_dbg(ISO_DBG_INFO,
				"[phcd_iso_itd_to_ptd]: IN Max packet over maximum\n");
			max_packet = 192;
		}

		if ((!usb_pipein(urb->pipe)) && (max_packet > 188)) {
			iso_dbg(ISO_DBG_INFO,
				"[phcd_iso_itd_to_ptd]: OUT Max packet over maximum\n");
			max_packet = 188;
		}
	} else {		/*HIGH SPEED */

		if (max_packet > 1024){
			max_packet = 1024;
		}
	}
	td_info1 |= (max_packet << 18);

	/*
	 * Place the FIRST BIT of the endpoint number here.
	 */
	td_info1 |= (usb_pipeendpoint(urb->pipe) << 31);

	/*
	 * Set the number of successive packets the HC can submit to the endpoint.
	 */
	if (urb->dev->speed == USB_SPEED_HIGH) {
		td_info1 |= MULTI(mult);
	}

	/* Set the first DWORD */
	iso_ptd->td_info1 = td_info1;
	iso_dbg(ISO_DBG_DATA, "[phcd_iso_itd_to_ptd]: DWORD0 = 0x%08x\n",
		iso_ptd->td_info1);

	/*
	 * Since the first bit have already been added on the first DWORD of the PTD
	 * we only need to add the last 3-bits of the endpoint number.
	 */
	token = (usb_pipeendpoint(urb->pipe) & 0xE) >> 1;

	/*
	 * Get the device address and set it accordingly to its assigned bits of the 2nd
	 * DWORD.
	 */
	token |= usb_pipedevice(urb->pipe) << 3;

	/* See a split transaction is needed */
	if (urb->dev->speed != USB_SPEED_HIGH) {
		/*
		 * If we are performing a SPLIT transaction indicate that it is so by setting
		 * the S bit of the second DWORD.
		 */
		token |= 1 << 14;

		port_num = urb->dev->ttport;
		hub_num = urb->dev->tt->hub->devnum;

		/* Set the the port number of the hub or embedded TT */
		token |= port_num << 18;

		/*
		 * Set the hub address, this should be zero for the internal or
		 * embedded hub
		 */
		token |= hub_num << 25;
	}

	/* if(urb->dev->speed != USB_SPEED_HIGH) */
	/*
	 * Determine if the direction of this pipe is IN, if so set the Token bit of
	 * the second DWORD to indicate it as IN. Since it is initialized to zero and
	 * zero indicates an OUT token, then we do not need anything to the Token bit
	 * if it is an OUT token.
	 */
	if (usb_pipein(urb->pipe)){
		token |= (IN_PID << 10);
	}

	/* Set endpoint type to Isochronous */
	token |= EPTYPE_ISO;

	/* Set the second DWORD */
	iso_ptd->td_info2 = token;
	iso_dbg(ISO_DBG_DATA, "[phcd_iso_itd_to_ptd]: DWORD1 = 0x%08x\n",
		iso_ptd->td_info2);

	/*
	 * Get the physical address of the memory location that was allocated for this PTD
	 * in the PAYLOAD region, using the formula indicated in sectin 7.2.2 of the ISP1763 specs
	 * rev 3.01 page 17 to 18.
	 */
	data_addr = ((unsigned long) (mem_addr->phy_addr) & 0xffff) - 0x400;
	data_addr >>= 3;

	/*  Set it to its location in the third DWORD */
	td_info3 = (data_addr&0xffff) << 8;

	/*
	 * Set the frame number when this PTD will be sent for ISO OUT or IN
	 * Bits 0 to 2 are don't care, only bits 3 to 7.
	 */
	frame_number = itd->framenumber;
	td_info3 |= (0xff&(frame_number << 3));

	/* Set the third DWORD */
	iso_ptd->td_info3 = td_info3;
	iso_dbg(ISO_DBG_DATA, "[phcd_iso_itd_to_ptd]: DWORD2 = 0x%08x\n",
		iso_ptd->td_info3);

	/*
	 * Set the A bit of the fourth DWORD to 1 to indicate that this PTD is active.
	 * This have the same functionality with the V bit of DWORD0
	 */
	iso_ptd->td_info4 = QHA_ACTIVE;
	iso_dbg(ISO_DBG_DATA, "[phcd_iso_itd_to_ptd]: DWORD3 = 0x%08x\n",
		iso_ptd->td_info4);

	/* Set the fourth DWORD to specify which uSOFs the start split needs to be placed */
	iso_ptd->td_info5 = itd->ssplit;
	iso_dbg(ISO_DBG_DATA, "[phcd_iso_itd_to_ptd]: DWORD4 = 0x%08x\n",
		iso_ptd->td_info5);

	/*
	 * Set the fifth DWORD to specify which uSOFs the complete split needs to be sent.
	 * This is VALID only for IN (since ISO transfers don't have handshake stages)
	 */
	iso_ptd->td_info6 = itd->csplit;
	iso_dbg(ISO_DBG_DATA, "[phcd_iso_itd_to_ptd]: DWORD5 = 0x%08x\n",
		iso_ptd->td_info6);

	iso_dbg(ISO_DBG_EXIT, "phcd_iso_itd_to_ptd exit\n");
	return iso_ptd;
}				/* phcd_iso_itd_to_ptd */

/*
 * phcd_iso_scheduling_info - Initializing the start split and complete split.
 *
 * phci_hcd *hcd
 *      - Main host controller driver structure
 * struct ehci_qh *qhead
 *  - Contains information about the endpoint.
 * unsigned long max_pkt
 *  - Maximum packet size that the endpoint in capable of handling
 * unsigned long high_speed
 *  - Indicates if the bus is a high speed bus
 * unsigned long ep_in
 *  - Inidcates if the endpoint is an IN endpoint
 *
 * API Description
 * This is mainly responsible for:
 *  - Determining the number of start split needed during an OUT transaction or
 *    the number of complete splits needed during an IN transaction.
 */
unsigned long
phcd_iso_scheduling_info(phci_hcd * hcd,
	struct ehci_qh *qhead,
	unsigned long max_pkt,
	unsigned long high_speed, unsigned long ep_in)
{
	unsigned long count, usof, temp;

	/* Local variable initialization */
	usof = 0x1;

	if (high_speed) {
		qhead->csplit = 0;

		/* Always send high speed transfers in first uframes */
		qhead->ssplit = 0x1;
		return 0;
	}

	/* Determine how many 188 byte-transfers are needed to send all data */
	count = max_pkt / 188;

	/*
	 * Check is the data is not a factor of 188, if it is not then we need
	 * one more 188 transfer to move the last set of data less than 188.
	 */
	if (max_pkt % 188){
		count += 1;
	}

	/*
	 * Remember that usof was initialized to 0x1 so that means
	 * that usof is always guranteed a value of 0x1 and then
	 * depending on the maxp, other bits of usof will also be set.
	 */
	for (temp = 0; temp < count; temp++){
		usof |= (0x1 << temp);
	}

	if (ep_in) {
		/*
		 * Send start split into first frame.
		 */
		qhead->ssplit = 0x1;

		/*
		 * Inidicate that we can send a complete split starting from
		 * the third uFrame to how much complete split is needed to
		 * retrieve all data.
		 *
		 * Of course, the first uFrame is reserved for the start split, the
		 * second is reserved for the TT to send the request and get some
		 * data.
		 */
		qhead->csplit = (usof << 2);
	} else {
		/*
		 * For ISO OUT we don't need to send out a complete split
		 * since we do not require and data coming in to us (since ISO
		 * do not have integrity checking/handshake).
		 *
		 * For start split we indicate that we send a start split from the
		 * first uFrame up to the the last uFrame needed to retrieve all
		 * data
		 */
		qhead->ssplit = usof;
		qhead->csplit = 0;
	}	/* else for if(ep_in) */
	return 0;
}				/* phcd_iso_scheduling_info */

/*
 * phcd_iso_sitd_fill - Allocate memory from the PAYLOAD memory region
 *
 * phci_hcd *pHcd_st
 *  - Main host controller driver structure
 * struct ehci_sitd *sitd
 *  - Isochronous Transfer Descriptor, contains elements as defined by the
 *        EHCI standard plus a few more  specific elements.
 * struct urb *urb
 *  - USB Request Block, contains information regarding the type and how much data
 *    is requested to be transferred.
 * unsigned long packets
 *  - Total number of packets to completely transfer this ISO transfer request.
 *
 * API Description
 * This is mainly responsible for:
 * - Initialize the following elements of the ITS structure
 *       > sitd->length = length;        -- the size of the request
 *       > sitd->multi = multi;          -- the number of transactions for
 *                                         this EP per micro frame
 *       > sitd->hw_bufp[0] = buf_dma;   -- The base address of the buffer where
 *                                         to put the data (this base address was
 *                                         the buffer provided plus the offset)
 * - Allocating memory from the PAYLOAD memory area, where the data coming from
 *   the requesting party will be placed or data requested by the requesting party will
 *   be retrieved when it is available.
 */
unsigned long
phcd_iso_sitd_fill(phci_hcd * hcd,
	struct ehci_sitd *sitd,
	struct urb *urb, unsigned long packets)
{
	unsigned long length, offset, pipe;
	unsigned long max_pkt;
	dma_addr_t buff_dma;
	struct isp1763_mem_addr *mem_addr;

#ifdef COMMON_MEMORY
	struct ehci_qh *qhead = NULL;
#endif

	iso_dbg(ISO_DBG_ENTRY, "phcd_iso_itd_fill entry\n");
	/*
	 * The value for both these variables are supplied by the one
	 * who submitted the URB.
	 */
	length = urb->iso_frame_desc[packets].length;
	offset = urb->iso_frame_desc[packets].offset;

	/* Initialize the status and actual length of this packet */
	urb->iso_frame_desc[packets].actual_length = 0;
	urb->iso_frame_desc[packets].status = -EXDEV;

	/* Buffer for this packet */
	buff_dma = (u32) ((unsigned char *) urb->transfer_buffer + offset);

	/* Memory for this packet */
	mem_addr = &sitd->mem_addr;

	pipe = urb->pipe;
	max_pkt = usb_maxpacket(urb->dev, pipe, usb_pipeout(pipe));

	max_pkt = max_pkt & 0x7FF;

	if ((length < 0) || (max_pkt < length)) {
		iso_dbg(ISO_DBG_ERR,
			"[phcd_iso_itd_fill Error]: No available memory.\n");
		return -ENOSPC;
	}
	sitd->buf_dma = buff_dma;


#ifndef COMMON_MEMORY
	/*
	 * Allocate memory in the PAYLOAD memory region for the
	 * data buffer for this SITD
	 */
	phci_hcd_mem_alloc(length, mem_addr, 0);
	if (length && ((mem_addr->phy_addr == 0) || (mem_addr->virt_addr == 0))) {
		mem_addr = 0;
		iso_dbg(ISO_DBG_ERR,
			"[phcd_iso_itd_fill Error]: No payload memory available\n");
		return -ENOMEM;
	}
#else
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
	qhead=urb->hcpriv;
#else
	qhead = urb->ep->hcpriv;
#endif
	if (qhead) {

		mem_addr->phy_addr = qhead->memory_addr.phy_addr + offset;

		mem_addr->virt_addr = qhead->memory_addr.phy_addr + offset;
	} else {
		iso_dbg(ISO_DBG_ERR,
			"[phcd_iso_itd_fill Error]: No payload memory available\n");
		return -ENOMEM;
	}


#endif
	/* Length of this packet */
	sitd->length = length;

	/* Buffer address, one ptd per packet */
	sitd->hw_bufp[0] = buff_dma;

	iso_dbg(ISO_DBG_EXIT, "phcd_iso_sitd_fill exit\n");
	return 0;
}

/*
 * phcd_iso_itd_fill - Allocate memory from the PAYLOAD memory region
 *
 * phci_hcd *pHcd_st
 *  - Main host controller driver structure
 * struct ehci_itd *itd
 *  - Isochronous Transfer Descriptor, contains elements as defined by the
 *        EHCI standard plus a few more IC specific elements.
 * struct urb *urb
 *  - USB Request Block, contains information regarding the type and how much data
 *    is requested to be transferred.
 * unsigned long packets
 *  - Total number of packets to completely transfer this ISO transfer request.
 *
 * API Description
 * This is mainly responsible for:
 * - Initialize the following elements of the ITS structure
 *       > itd->length = length;        -- the size of the request
 *       > itd->multi = multi;          -- the number of transactions for
 *                                         this EP per micro frame
 *       > itd->hw_bufp[0] = buf_dma;   -- The base address of the buffer where
 *                                         to put the data (this base address was
 *                                         the buffer provided plus the offset)
 * - Allocating memory from the PAYLOAD memory area, where the data coming from
 *   the requesting party will be placed or data requested by the requesting party will
 *   be retrieved when it is available.
 */
unsigned long
phcd_iso_itd_fill(phci_hcd * hcd,
	struct ehci_itd *itd,
	struct urb *urb,
	unsigned long packets, unsigned char numofPkts)
{
	unsigned long length, offset, pipe;
	unsigned long max_pkt, mult;
	dma_addr_t buff_dma;
	struct isp1763_mem_addr *mem_addr;
#ifdef COMMON_MEMORY
	struct ehci_qh *qhead = NULL;
#endif
	int i = 0;

	iso_dbg(ISO_DBG_ENTRY, "phcd_iso_itd_fill entry\n");
	for (i = 0; i < 8; i++){
		itd->hw_transaction[i] = 0;
	}
	/*
	 * The value for both these variables are supplied by the one
	 * who submitted the URB.
	 */
	length = urb->iso_frame_desc[packets].length;
	offset = urb->iso_frame_desc[packets].offset;

	/* Initialize the status and actual length of this packet */
	urb->iso_frame_desc[packets].actual_length = 0;
	urb->iso_frame_desc[packets].status = -EXDEV;

	/* Buffer for this packet */
	buff_dma = cpu_to_le32((unsigned char *) urb->transfer_buffer + offset);

	/* Memory for this packet */
	mem_addr = &itd->mem_addr;

	pipe = urb->pipe;
	max_pkt = usb_maxpacket(urb->dev, pipe, usb_pipeout(pipe));

	mult = 1 + ((max_pkt >> 11) & 0x3);
	max_pkt = max_pkt & 0x7FF;
	max_pkt *= mult;

	if ((length < 0) || (max_pkt < length)) {
		iso_dbg(ISO_DBG_ERR,
			"[phcd_iso_itd_fill Error]: No available memory.\n");
		return -ENOSPC;
	}
	itd->buf_dma = buff_dma;
	for (i = packets + 1; i < numofPkts + packets; i++)
		length += urb->iso_frame_desc[i].length;

	/*
	 * Allocate memory in the PAYLOAD memory region for the
	 * data buffer for this ITD
	 */
#ifndef COMMON_MEMORY

	phci_hcd_mem_alloc(length, mem_addr, 0);
	if (length && ((mem_addr->phy_addr == 0) || (mem_addr->virt_addr == 0))) {
		mem_addr = 0;
		iso_dbg(ISO_DBG_ERR,
			"[phcd_iso_itd_fill Error]: No payload memory available\n");
		return -ENOMEM;
	}
#else
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24)
	qhead = urb->ep->hcpriv;
#else
	qhead=urb->hcpriv;
#endif
	if (qhead) {

		mem_addr->phy_addr = qhead->memory_addr.phy_addr + offset;

		mem_addr->virt_addr = qhead->memory_addr.phy_addr + offset;
	} else {
		iso_dbg(ISO_DBG_ERR,
			"[phcd_iso_itd_fill Error]: No payload memory available\n");
		return -ENOMEM;
	}


#endif
	/* Length of this packet */
	itd->length = length;

	/* Number of transaction per uframe */
	itd->multi = mult;

	/* Buffer address, one ptd per packet */
	itd->hw_bufp[0] = buff_dma;

	iso_dbg(ISO_DBG_EXIT, "phcd_iso_itd_fill exit\n");
	return 0;
}				/* phcd_iso_itd_fill */

/*
 * phcd_iso_get_sitd_ptd_index - Allocate an ISO PTD from the ISO PTD map list
 *
 * phci_hcd *hcd
 *      - Main host controller driver structure
 * struct ehci_sitd *sitd
 *  - Isochronous Transfer Descriptor, contains elements as defined by the
 *        EHCI standard plus a few more  specific elements.
 *
 * API Description
 * This is mainly responsible for:
 * - Allocating an ISO PTD from the ISO PTD map list
 * - Set the equivalent bit of the allocated PTD to active
 *   in the bitmap so that this PTD will be included into
 *   the periodic schedule
 */
void
phcd_iso_get_sitd_ptd_index(phci_hcd * hcd, struct ehci_sitd *sitd)
{
	td_ptd_map_buff_t *ptd_map_buff;
	unsigned long buff_type, max_ptds;
	unsigned char sitd_index, bitmap;

	/* Local variable initialization */
	bitmap = 0x1;
	buff_type = td_ptd_pipe_x_buff_type[TD_PTD_BUFF_TYPE_ISTL];
	ptd_map_buff = (td_ptd_map_buff_t *) & (td_ptd_map_buff[buff_type]);
	max_ptds = ptd_map_buff->max_ptds;
	sitd->sitd_index = TD_PTD_INV_PTD_INDEX;

	for (sitd_index = 0; sitd_index < max_ptds; sitd_index++) {
		/*
		 * ISO have 32 PTDs, the first thing to do is look for a free PTD.
		 */
		if (ptd_map_buff->map_list[sitd_index].state == TD_PTD_NEW) {
			iso_dbg(ISO_DBG_INFO,
				"[phcd_iso_get_itd_ptd_index] There's a free PTD No. %d\n",
				sitd_index);
			/*
			 * Determine if this is a newly allocated SITD by checking the
			 * itd_index, since it was set to TD_PTD_INV_PTD_INDEX during
			 * initialization
			 */
			if (sitd->sitd_index == TD_PTD_INV_PTD_INDEX) {
				sitd->sitd_index = sitd_index;
			}

			/* Once there is a free slot, indicate that it is already taken */
			ptd_map_buff->map_list[sitd_index].datatoggle = 0;
			ptd_map_buff->map_list[sitd_index].state =
				TD_PTD_ACTIVE;
			ptd_map_buff->map_list[sitd_index].qtd = NULL;

			/* Put a connection to the SITD with the PTD maplist */
			ptd_map_buff->map_list[sitd_index].sitd = sitd;
			ptd_map_buff->map_list[sitd_index].itd = NULL;
			ptd_map_buff->map_list[sitd_index].qh = NULL;

			/* ptd_bitmap just holds the bit assigned to this PTD. */
			ptd_map_buff->map_list[sitd_index].ptd_bitmap =
				bitmap << sitd_index;

			phci_hcd_fill_ptd_addresses(&ptd_map_buff->
				map_list[sitd_index], sitd->sitd_index,
				buff_type);

			/*
			 * Indicate that this SITD is the last in the list and update
			 * the number of active PTDs
			 */
			ptd_map_buff->map_list[sitd_index].lasttd = 0;
			ptd_map_buff->total_ptds++;


			ptd_map_buff->active_ptd_bitmap |=
				(bitmap << sitd_index);
			ptd_map_buff->pending_ptd_bitmap |= (bitmap << sitd_index);	
			break;
		}		/* if(ptd_map_buff->map_list[sitd_index].state == TD_PTD_NEW) */
	}			/* for(itd_index = 0; itd_index < max_ptds; itd_index++) */
	return;
}

/*
 * phcd_iso_get_itd_ptd_index - Allocate an ISO PTD from the ISO PTD map list
 *
 * phci_hcd *hcd
 *      - Main host controller driver structure
 * struct ehci_itd *itd
 *  - Isochronous Transfer Descriptor, contains elements as defined by the
 *        EHCI standard plus a few more IC specific elements.
 *
 * API Description
 * This is mainly responsible for:
 * - Allocating an ISO PTD from the ISO PTD map list
 * - Set the equivalent bit of the allocated PTD to active
 *   in the bitmap so that this PTD will be included into
 *   the periodic schedule
 */
void
phcd_iso_get_itd_ptd_index(phci_hcd * hcd, struct ehci_itd *itd)
{
	td_ptd_map_buff_t *ptd_map_buff;
	unsigned long buff_type, max_ptds;
	unsigned char itd_index, bitmap;

	/* Local variable initialization */
	bitmap = 0x1;
	buff_type = td_ptd_pipe_x_buff_type[TD_PTD_BUFF_TYPE_ISTL];
	ptd_map_buff = (td_ptd_map_buff_t *) & (td_ptd_map_buff[buff_type]);
	max_ptds = ptd_map_buff->max_ptds;

	itd->itd_index = TD_PTD_INV_PTD_INDEX;

	for (itd_index = 0; itd_index < max_ptds; itd_index++) {
		/*
		 * ISO have 32 PTDs, the first thing to do is look for a free PTD.
		 */
		if (ptd_map_buff->map_list[itd_index].state == TD_PTD_NEW) {
			/*
			 * Determine if this is a newly allocated ITD by checking the
			 * itd_index, since it was set to TD_PTD_INV_PTD_INDEX during
			 * initialization
			 */
			if (itd->itd_index == TD_PTD_INV_PTD_INDEX) {
				itd->itd_index = itd_index;
			}

			/* Once there is a free slot, indicate that it is already taken */
			ptd_map_buff->map_list[itd_index].datatoggle = 0;
			ptd_map_buff->map_list[itd_index].state = TD_PTD_ACTIVE;
			ptd_map_buff->map_list[itd_index].qtd = NULL;

			/* Put a connection to the ITD with the PTD maplist */
			ptd_map_buff->map_list[itd_index].itd = itd;
			ptd_map_buff->map_list[itd_index].qh = NULL;

			/* ptd_bitmap just holds the bit assigned to this PTD. */
			ptd_map_buff->map_list[itd_index].ptd_bitmap =
				bitmap << itd_index;

			phci_hcd_fill_ptd_addresses(&ptd_map_buff->
				map_list[itd_index],
				itd->itd_index, buff_type);

			/*
			 * Indicate that this ITD is the last in the list and update
			 * the number of active PTDs
			 */
			ptd_map_buff->map_list[itd_index].lasttd = 0;
			ptd_map_buff->total_ptds++;

			ptd_map_buff->active_ptd_bitmap |=
				(bitmap << itd_index);
			ptd_map_buff->pending_ptd_bitmap |= (bitmap << itd_index);	
			break;
		}		/* if(ptd_map_buff->map_list[itd_index].state == TD_PTD_NEW) */
	}			/* for(itd_index = 0; itd_index < max_ptds; itd_index++) */
	return;
}				/* phcd_iso_get_itd_ptd_index */

/*
 * phcd_iso_sitd_free_list - Free memory used by SITDs in SITD list
 *
 * phci_hcd *hcd
 *      - Main host controller driver structure
 * struct urb *urb
 *  - USB Request Block, contains information regarding the type and how much data
 *    is requested to be transferred.
 * unsigned long status
 *  - Variable provided by the calling routine that contain the status of the
 *        SITD list.
 *
 * API Description
 * This is mainly responsible for:
 *  - Cleaning up memory used by each SITD in the SITD list
 */
void
phcd_iso_sitd_free_list(phci_hcd * hcd, struct urb *urb, unsigned long status)
{
	td_ptd_map_buff_t *ptd_map_buff;
	struct ehci_sitd *first_sitd, *next_sitd, *sitd;
	td_ptd_map_t *td_ptd_map;

	/* Local variable initialization */
	ptd_map_buff = &(td_ptd_map_buff[TD_PTD_BUFF_TYPE_ISTL]);
	first_sitd = (struct ehci_sitd *) urb->hcpriv;
	sitd = first_sitd;

	/*
	 * Check if there is only one SITD, if so immediately
	 * go and clean it up.
	 */
	if (sitd->hw_next == EHCI_LIST_END) {
		if (sitd->sitd_index != TD_PTD_INV_PTD_INDEX) {
			td_ptd_map = &ptd_map_buff->map_list[sitd->sitd_index];
			td_ptd_map->state = TD_PTD_NEW;
		}

		if (status != -ENOMEM) {
			phci_hcd_mem_free(&sitd->mem_addr);
		}

		list_del(&sitd->sitd_list);
		qha_free(qha_cache, sitd);

		urb->hcpriv = 0;
		return;
	}
	/* if(sitd->hw_next == EHCI_LIST_END) */
	while (1) {
		/* Get the SITD following the head SITD */
		next_sitd = (struct ehci_sitd *) (sitd->hw_next);
		if (next_sitd->hw_next == EHCI_LIST_END) {
			/*
			 * If the next SITD is the end of the list, check if space have
			 * already been allocated in the PTD array.
			 */
			if (next_sitd->sitd_index != TD_PTD_INV_PTD_INDEX) {
				/* Free up its allocation */
				td_ptd_map =
					&ptd_map_buff->map_list[next_sitd->
					sitd_index];
				td_ptd_map->state = TD_PTD_NEW;
			}

			/*
			 * If the error is not about memory allocation problems, then
			 * free up the memory used.
			 */
			if (status != -ENOMEM) {
				iso_dbg(ISO_DBG_ERR,
					"[phcd_iso_itd_free_list Error]: Memory not available\n");
				phci_hcd_mem_free(&next_sitd->mem_addr);
			}

			/* Remove from the SITD list and free up space allocated for SITD structure */
			list_del(&next_sitd->sitd_list);
			qha_free(qha_cache, next_sitd);
			break;
		}

		/* if(next_itd->hw_next == EHCI_LIST_END) */
		/*
		 * If SITD is not the end of the list, it only means that it already have everything allocated
		 * and there is no need to check which procedure failed. So just free all resourcs immediately
		 */
		sitd->hw_next = next_sitd->hw_next;

		td_ptd_map = &ptd_map_buff->map_list[next_sitd->sitd_index];
		td_ptd_map->state = TD_PTD_NEW;
		phci_hcd_mem_free(&next_sitd->mem_addr);
		list_del(&next_sitd->sitd_list);
		qha_free(qha_cache, next_sitd);
	}			/*  while(1) */

	/* Now work on the head SITD, it is the last one processed. */
	if (first_sitd->sitd_index != TD_PTD_INV_PTD_INDEX) {
		td_ptd_map = &ptd_map_buff->map_list[first_sitd->sitd_index];
		td_ptd_map->state = TD_PTD_NEW;
	}

	if (status != -ENOMEM) {
		iso_dbg(ISO_DBG_ERR,
			"[phcd_iso_itd_free_list Error]: No memory\n");
		phci_hcd_mem_free(&first_sitd->mem_addr);
	}

	list_del(&first_sitd->sitd_list);
	qha_free(qha_cache, first_sitd);
	urb->hcpriv = 0;
	return;
}

/*
 * phcd_iso_itd_free_list - Free memory used by ITDs in ITD list
 *
 * phci_hcd *hcd
 *      - Main host controller driver structure
 * struct urb *urb
 *  - USB Request Block, contains information regarding the type and how much data
 *    is requested to be transferred.
 * unsigned long status
 *  - Variable provided by the calling routine that contain the status of the
 *        ITD list.
 *
 * API Description
 * This is mainly responsible for:
 *  - Cleaning up memory used by each ITD in the ITD list
 */
void
phcd_iso_itd_free_list(phci_hcd * hcd, struct urb *urb, unsigned long status)
{
	td_ptd_map_buff_t *ptd_map_buff;
	struct ehci_itd *first_itd, *next_itd, *itd;
	td_ptd_map_t *td_ptd_map;

	/* Local variable initialization */
	ptd_map_buff = &(td_ptd_map_buff[TD_PTD_BUFF_TYPE_ISTL]);
	first_itd = (struct ehci_itd *) urb->hcpriv;
	itd = first_itd;

	/*
	 * Check if there is only one ITD, if so immediately
	 * go and clean it up.
	 */
	if (itd->hw_next == EHCI_LIST_END) {
		if (itd->itd_index != TD_PTD_INV_PTD_INDEX) {
			td_ptd_map = &ptd_map_buff->map_list[itd->itd_index];
			td_ptd_map->state = TD_PTD_NEW;
		}

		if (status != -ENOMEM) {
			phci_hcd_mem_free(&itd->mem_addr);
		}

		list_del(&itd->itd_list);
		qha_free(qha_cache, itd);

		urb->hcpriv = 0;
		return;
	}
	/* if(itd->hw_next == EHCI_LIST_END) */
	while (1) {
		/* Get the ITD following the head ITD */
		next_itd = (struct ehci_itd *) le32_to_cpu(itd->hw_next);
		if (next_itd->hw_next == EHCI_LIST_END) {
			/*
			 * If the next ITD is the end of the list, check if space have
			 * already been allocated in the PTD array.
			 */
			if (next_itd->itd_index != TD_PTD_INV_PTD_INDEX) {
				/* Free up its allocation */
				td_ptd_map =
					&ptd_map_buff->map_list[next_itd->
					itd_index];
				td_ptd_map->state = TD_PTD_NEW;
			}

			/*
			 * If the error is not about memory allocation problems, then
			 * free up the memory used.
			 */
			if (status != -ENOMEM) {
				iso_dbg(ISO_DBG_ERR,
					"[phcd_iso_itd_free_list Error]: Memory not available\n");
				phci_hcd_mem_free(&next_itd->mem_addr);
			}

			/* Remove from the ITD list and free up space allocated for ITD structure */
			list_del(&next_itd->itd_list);
			qha_free(qha_cache, next_itd);
			break;
		}

		/* if(next_itd->hw_next == EHCI_LIST_END) */
		/*
		 * If ITD is not the end of the list, it only means that it already have everything allocated
		 * and there is no need to check which procedure failed. So just free all resourcs immediately
		 */
		itd->hw_next = next_itd->hw_next;

		td_ptd_map = &ptd_map_buff->map_list[next_itd->itd_index];
		td_ptd_map->state = TD_PTD_NEW;
		phci_hcd_mem_free(&next_itd->mem_addr);
		list_del(&next_itd->itd_list);
		qha_free(qha_cache, next_itd);
	}			/*  while(1) */

	/* Now work on the head ITD, it is the last one processed. */
	if (first_itd->itd_index != TD_PTD_INV_PTD_INDEX) {
		td_ptd_map = &ptd_map_buff->map_list[first_itd->itd_index];
		td_ptd_map->state = TD_PTD_NEW;
	}

	if (status != -ENOMEM) {
		iso_dbg(ISO_DBG_ERR,
			"[phcd_iso_itd_free_list Error]: No memory\n");
		phci_hcd_mem_free(&first_itd->mem_addr);
	}

	list_del(&first_itd->itd_list);
	qha_free(qha_cache, first_itd);
	urb->hcpriv = 0;
	return;
}				/* phcd_iso_itd_free_list */

void
phcd_clean_iso_qh(phci_hcd * hcd, struct ehci_qh *qh)
{
	unsigned int i = 0;
	u16 skipmap=0;
	struct ehci_sitd *sitd;
	struct ehci_itd *itd;

	iso_dbg(ISO_DBG_ERR, "phcd_clean_iso_qh \n");
	if (!qh){
		return;
	}
	skipmap = isp1763_reg_read16(hcd->dev, hcd->regs.isotdskipmap, skipmap);
	skipmap |= qh->periodic_list.ptdlocation;
	isp1763_reg_write16(hcd->dev, hcd->regs.isotdskipmap, skipmap);
#ifdef COMMON_MEMORY
	phci_hcd_mem_free(&qh->memory_addr);
#endif
	for (i = 0; i < 16 && qh->periodic_list.ptdlocation; i++) {
		if (qh->periodic_list.ptdlocation & (0x1 << i)) {
			printk("[phcd_clean_iso_qh] : %x \n",
				qh->periodic_list.high_speed);

			qh->periodic_list.ptdlocation &= ~(0x1 << i);

			if (qh->periodic_list.high_speed == 0) {
				if (td_ptd_map_buff[TD_PTD_BUFF_TYPE_ISTL].
					map_list[i].sitd) {

					printk("SITD found \n");
					sitd = td_ptd_map_buff
						[TD_PTD_BUFF_TYPE_ISTL].
						map_list[i].sitd;
#ifndef COMMON_MEMORY
					phci_hcd_mem_free(&sitd->mem_addr);
#endif
					/*
					if(sitd->urb)
						urb=sitd->urb;
					*/
					sitd->urb = NULL;
					td_ptd_map_buff[TD_PTD_BUFF_TYPE_ISTL].
						map_list[i].state = TD_PTD_NEW;
					td_ptd_map_buff[TD_PTD_BUFF_TYPE_ISTL].
						map_list[i].sitd = NULL;
					qha_free(qha_cache, sitd);
				}
			} else {
				if (td_ptd_map_buff[TD_PTD_BUFF_TYPE_ISTL].
					map_list[i].itd) {

					printk("ITD found \n");
					itd = td_ptd_map_buff
						[TD_PTD_BUFF_TYPE_ISTL].
						map_list[i].itd;
#ifdef COMMON_MEMORY
					phci_hcd_mem_free(&itd->mem_addr);
#endif

					/*
					if(itd->urb)
					urb=itd->urb;
					*/
					itd->urb = NULL;
					td_ptd_map_buff[TD_PTD_BUFF_TYPE_ISTL].
						map_list[i].state = TD_PTD_NEW;
					td_ptd_map_buff[TD_PTD_BUFF_TYPE_ISTL].
						map_list[i].itd = NULL;
					qha_free(qha_cache, itd);
				}
			}

		}
	}


}


/*
 * phcd_store_urb_pending - store requested URB into a queue
 *
 * phci_hcd *hcd
 *      - Main host controller driver structure
 * struct urb *urb
 *  - USB Request Block, contains information regarding the type and how much data
 *    is requested to be transferred.
 * unsigned long *status
 *  - Variable provided by the calling routine that will contain the status of the
 *        phcd_submit_iso actions
 *
 * API Description
 * This is mainly responsible for:
 *  - Store URB into a queue
 *  - If ther's enough free PTD slots , repairing the PTDs
 */
void phcd_clean_periodic_ep(void){
	periodic_ep[0] = NULL;
	periodic_ep[1] = NULL;
}

int
phcd_clean_urb_pending(phci_hcd * hcd, struct urb *urb)
{
	unsigned int i = 0;
	struct ehci_qh *qhead;
	struct ehci_sitd *sitd;
	struct ehci_itd *itd;
	u16 skipmap=0;;

	iso_dbg(ISO_DBG_ENTRY, "[phcd_clean_urb_pending] : Enter\n");
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
	qhead=urb->hcpriv;
	if (periodic_ep[0] == qhead->ep) {
		periodic_ep[0] = NULL;

	}

	if (periodic_ep[1] == qhead->ep) {
		periodic_ep[1] = NULL;
	}
#else	
	qhead = urb->ep->hcpriv;
	if (periodic_ep[0] == urb->ep) {
		periodic_ep[0] = NULL;

	}

	if (periodic_ep[1] == urb->ep) {
		periodic_ep[1] = NULL;
	}
#endif	
	if (!qhead) {
		return 0;
	}
	skipmap = isp1763_reg_read16(hcd->dev, hcd->regs.isotdskipmap, skipmap);
	skipmap |= qhead->periodic_list.ptdlocation;
	isp1763_reg_write16(hcd->dev, hcd->regs.isotdskipmap, skipmap);
#ifdef COMMON_MEMORY
	phci_hcd_mem_free(&qhead->memory_addr);
#endif

	for (i = 0; i < 16 && qhead->periodic_list.ptdlocation; i++) {

		qhead->periodic_list.ptdlocation &= ~(0x1 << i);

		if (qhead->periodic_list.ptdlocation & (0x1 << i)) {

			printk("[phcd_clean_urb_pending] : %x \n",
				qhead->periodic_list.high_speed);

			if (qhead->periodic_list.high_speed == 0) {

				if (td_ptd_map_buff[TD_PTD_BUFF_TYPE_ISTL].
					map_list[i].sitd) {

					sitd = td_ptd_map_buff
						[TD_PTD_BUFF_TYPE_ISTL].
						map_list[i].sitd;
#ifndef COMMON_MEMORY
					phci_hcd_mem_free(&sitd->mem_addr);
#endif
					td_ptd_map_buff[TD_PTD_BUFF_TYPE_ISTL].
						map_list[i].state = TD_PTD_NEW;
					td_ptd_map_buff[TD_PTD_BUFF_TYPE_ISTL].
						map_list[i].sitd = NULL;
					qha_free(qha_cache, sitd);
				}
			} else {

				if (td_ptd_map_buff[TD_PTD_BUFF_TYPE_ISTL].
					map_list[i].itd) {

					itd = td_ptd_map_buff
						[TD_PTD_BUFF_TYPE_ISTL].
						map_list[i].itd;
#ifdef COMMON_MEMORY
					phci_hcd_mem_free(&itd->mem_addr);
#endif
					td_ptd_map_buff[TD_PTD_BUFF_TYPE_ISTL].
						map_list[i].state = TD_PTD_NEW;
					td_ptd_map_buff[TD_PTD_BUFF_TYPE_ISTL].
						map_list[i].itd = NULL;
					qha_free(qha_cache, itd);
				}
			}

		}

	}
	INIT_LIST_HEAD(&qhead->periodic_list.sitd_itd_head);
	iso_dbg(ISO_DBG_ENTRY, "[phcd_clean_urb_pending] : Exit\n");
	return 0;
}



int
phcd_store_urb_pending(phci_hcd * hcd, int index, struct urb *urb, int *status)
{
	unsigned int uiNumofPTDs = 0;
	unsigned int uiNumofSlots = 0;
	unsigned int uiMult = 0;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24)
	iso_dbg(ISO_DBG_ENTRY, "[phcd_store_urb_pending] : Enter\n");
	if (urb != NULL) {
		if (periodic_ep[0] != urb->ep && periodic_ep[1] != urb->ep) {
			if (periodic_ep[0] == NULL) {
			//	printk("storing in 0 %x %x\n",urb,urb->pipe);
				periodic_ep[0] = urb->ep;
			} else if (periodic_ep[1] == NULL) {
				printk("storing in 1\n");
				periodic_ep[1] = urb->ep;
				usb_hcd_link_urb_to_ep(&(hcd->usb_hcd), urb);
				return -1;
			} else {
				iso_dbg(ISO_DBG_ERR,
					"Support only 2 ISO endpoints simultaneously \n");
				*status = -1;
				return -1;
			}
		}
		usb_hcd_link_urb_to_ep(&(hcd->usb_hcd), urb);
		iso_dbg(ISO_DBG_DATA,
			"[phcd_store_urb_pending] : Add an urb into gstUrb_pending array at index : %d\n",
			giUrbCount);
		giUrbCount++;
	} else {

		iso_dbg(ISO_DBG_ENTRY,
			"[phcd_store_urb_pending] : getting urb from list \n");
		if (index > 0 && index < 2) {
			if (periodic_ep[index - 1]){
				urb = container_of(periodic_ep[index - 1]->
					urb_list.next, struct urb,
					urb_list);
			}
		} else {
			iso_dbg(ISO_DBG_ERR, " Unknown enpoints Error \n");
			*status = -1;
			return -1;
		}

	}


	if ((urb != NULL && (urb->ep->urb_list.next == &urb->urb_list))){
		iso_dbg(ISO_DBG_DATA,
			"[phcd_store_urb_pending] : periodic_sched : %d\n",
			hcd->periodic_sched);
		iso_dbg(ISO_DBG_DATA,
			"[phcd_store_urb_pending] : number_of_packets : %d\n",
			urb->number_of_packets);
		iso_dbg(ISO_DBG_DATA,
			"[phcd_store_urb_pending] : Maximum PacketSize : %d\n",
			usb_maxpacket(urb->dev,urb->pipe, usb_pipeout(urb->pipe)));
		/*if enough free slots */
		if (urb->dev->speed == USB_SPEED_FULL) {	/*for FULL SPEED */
	//		if (hcd->periodic_sched < 
		//		MAX_PERIODIC_SIZE - urb->number_of_packets) {
			if(1){
				if (phcd_submit_iso(hcd, 
					#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
						struct usb_host_endpoint *ep,
					#endif
						urb,
						( unsigned long *) &status) == 0) {
					pehci_hcd_iso_schedule(hcd, urb);
				} else{
				//*status = 0;
				}
			}
		} else if (urb->dev->speed == USB_SPEED_HIGH) {	/*for HIGH SPEED */
			/*number of slots for 1 PTD */
			uiNumofSlots = NUMMICROFRAME / urb->interval;
			/*max packets size */
			uiMult = usb_maxpacket(urb->dev, urb->pipe,
					usb_pipeout(urb->pipe));
			/*mult */
			uiMult = 1 + ((uiMult >> 11) & 0x3);
			/*number of PTDs need to schedule for this PTD */
			uiNumofPTDs =
				(urb->number_of_packets / uiMult) /
				uiNumofSlots;
			if ((urb->number_of_packets / uiMult) % uiNumofSlots != 0){
				uiNumofPTDs += 1;
			}

			iso_dbg(ISO_DBG_DATA,
				"[phcd_store_urb_pending] : interval : %d\n",
				urb->interval);
			iso_dbg(ISO_DBG_DATA,
				"[phcd_store_urb_pending] : uiMult : %d\n",
				uiMult);
			iso_dbg(ISO_DBG_DATA,
				"[phcd_store_urb_pending] : uiNumofPTDs : %d\n",
				uiNumofPTDs);

			if (hcd->periodic_sched <=
				MAX_PERIODIC_SIZE - uiNumofPTDs) {

				if (phcd_submit_iso(hcd,
					#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
						struct usb_host_endpoint *ep,
					#endif
					urb, (unsigned long *) &status)== 0) {

					pehci_hcd_iso_schedule(hcd, urb);
				}
			} else{
				*status = 0;
			}
		}
	} else{
		iso_dbg(ISO_DBG_DATA,
			"[phcd_store_urb_pending] : nextUrb is NULL\n");
	}
#endif
	iso_dbg(ISO_DBG_ENTRY, "[phcd_store_urb_pending] : Exit\n");
	return 0;
}

/*
 * phcd_submit_iso - ISO transfer URB submit routine
 *
 * phci_hcd *hcd
 *      - Main host controller driver structure
 * struct urb *urb
 *  - USB Request Block, contains information regarding the type and how much data
 *    is requested to be transferred.
 * unsigned long *status
 *  - Variable provided by the calling routine that will contain the status of the
 *        phcd_submit_iso actions
 *
 * API Description
 * This is mainly responsible for:
 *  - Allocating memory for the endpoint information structure (pQHead_st)
 *  - Requesting for bus bandwidth from the USB core
 *  - Allocating and initializing Payload and PTD memory
 */
unsigned long
phcd_submit_iso(phci_hcd * hcd,
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
	struct usb_host_endpoint *ep,
#else
#endif
		struct urb *urb, unsigned long *status)
{
	struct _periodic_list *periodic_list;
	struct hcd_dev *dev;
	struct ehci_qh *qhead;
	struct ehci_itd *itd, *prev_itd;
	struct ehci_sitd *sitd, *prev_sitd;
	struct list_head *sitd_itd_list;
	unsigned long ep_in, max_pkt, mult;
	unsigned long bus_time, high_speed, start_frame;
	unsigned long temp;
	unsigned long packets;
	/*for high speed device */
	unsigned int iMicroIndex = 0;
	unsigned int iNumofSlots = 0;
	unsigned int iNumofPTDs = 0;
	unsigned int iPTDIndex = 0;
	unsigned int iNumofPks = 0;
	int iPG = 0;
	dma_addr_t buff_dma;
	unsigned long length, offset;
	int i = 0;

	iso_dbg(ISO_DBG_ENTRY, "phcd_submit_iso Entry\n");

	*status = 0;
	/* Local variable initialization */
	high_speed = 0;
	periodic_list = &hcd->periodic_list[0];
	dev = (struct hcd_dev *) urb->hcpriv;
	urb->hcpriv = (void *) 0;
	prev_itd = (struct ehci_itd *) 0;
	itd = (struct ehci_itd *) 0;
	prev_sitd = (struct ehci_sitd *) 0;
	sitd = (struct ehci_sitd *) 0;
	start_frame = 0;

	ep_in = usb_pipein(urb->pipe);

	/*
	 * Take the endpoint, if there is still no memory allocated
	 * for it allocate some and indicate this is for ISO.
	 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
	qhead = ep->hcpriv;
#else
	qhead = urb->ep->hcpriv;
#endif
	if (!qhead) {

		qhead = phci_hcd_qh_alloc(hcd);
		if (qhead == 0) {
			iso_dbg(ISO_DBG_ERR,
				"[phcd_submit_iso Error]: Not enough memory\n");
			return -ENOMEM;
		}

		qhead->type = TD_PTD_BUFF_TYPE_ISTL;
		INIT_LIST_HEAD(&qhead->periodic_list.sitd_itd_head);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
		qhead->ep=ep;
		ep->hcpriv = qhead;
		urb->hcpriv=qhead;
#else
		urb->ep->hcpriv = qhead;
#endif
	}

		urb->hcpriv=qhead;

	/* if(!qhead) */
	/*
	 * Get the number of additional packets that the endpoint can support during a
	 * single microframe.
	 */
	max_pkt = usb_maxpacket(urb->dev, urb->pipe, usb_pipeout(urb->pipe));

	/*
	 * We need to add 1 since our Multi starts with 1 instead of the USB specs defined
	 * zero (0).
	 */
	mult = 1 + ((max_pkt >> 11) & 0x3);

	/* This is the actual length per for the whole transaction */
	max_pkt *= mult;

	/* Check bandwidth */
	bus_time = 0;

	if (urb->dev->speed == USB_SPEED_FULL) {

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
		if (urb->bandwidth == 0) {
			bus_time = usb_check_bandwidth(urb->dev, urb);
			if (bus_time < 0) {
				usb_dec_dev_use(urb->dev);
				*status = bus_time;
				return *status;
			}
		}
#else
#endif
	} else {			/*HIGH SPEED */

		high_speed = 1;

		/*
		 * Calculate bustime as dictated by the USB Specs Section 5.11.3
		 * for high speed ISO
		 */
		bus_time = 633232L;
		bus_time +=
			(2083L * ((3167L + BitTime(max_pkt) * 1000L) / 1000L));
		bus_time = bus_time / 1000L;
		bus_time += BW_HOST_DELAY;
		bus_time = NS_TO_US(bus_time);
	}
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
	usb_claim_bandwidth(urb->dev, urb, bus_time, 1);
#else
#endif

	qhead->periodic_list.ptdlocation = 0;
	/* Initialize the start split (ssplit) and complete split (csplit) variables of qhead */
	if (phcd_iso_scheduling_info(hcd, qhead, max_pkt, high_speed, ep_in) <
		0) {

		iso_dbg(ISO_DBG_ERR,
			"[phcd_submit_iso Error]: No space available\n");
		return -ENOSPC;
	}

	if (urb->dev->speed == USB_SPEED_HIGH) {
		iNumofSlots = NUMMICROFRAME / urb->interval;
		/*number of PTDs need to schedule for this PTD */
		iNumofPTDs = (urb->number_of_packets / mult) / iNumofSlots;
		if ((urb->number_of_packets / mult) % iNumofSlots != 0){	
			/*get remainder */
			iNumofPTDs += 1;
		}
	}
	if (urb->iso_frame_desc[0].offset != 0) {
		*status = -EINVAL;
		iso_dbg(ISO_DBG_ERR,
			"[phcd_submit_iso Error]: Invalid value\n");
		return *status;
	}
	if (1) {
		/* Calculate the current frame number */
		if (0){
			if (urb->transfer_flags & URB_ISO_ASAP){
				start_frame =
					isp1763_reg_read16(hcd->dev,
						hcd->regs.frameindex,
						start_frame);
			} else {
				start_frame = urb->start_frame;
			}
		}

		start_frame =
			isp1763_reg_read16(hcd->dev, hcd->regs.frameindex,
				start_frame);

		/* The only valid bits of the frame index is the lower 14 bits. */

		/*
		 * Remove the count for the micro frame (uSOF) and just leave the
		 * count for the frame (SOF). Since 1 SOF is equal to 8 uSOF then
		 * shift right by three is like dividing it by 8 (each shift is divide by two)
		 */
		start_frame >>= 3;
		if (urb->dev->speed != USB_SPEED_HIGH){
			start_frame += 1;
		}else{
			start_frame += 2;
		}
		start_frame = start_frame & PTD_FRAME_MASK;
		temp = start_frame;
		if (urb->dev->speed != USB_SPEED_HIGH) {
			qhead->next_uframe =
				start_frame + urb->number_of_packets;
		} else {
			qhead->next_uframe = start_frame + iNumofPTDs;
		}
		qhead->next_uframe %= PTD_FRAME_MASK;
		iso_dbg(ISO_DBG_DATA, "[phcd_submit_iso]: startframe = %ld\n",
			start_frame);
	} else {
		/*
		 * The periodic frame list size is only 32 elements deep, so we need
		 * the frame index to be less than or equal to 32 (actually 31 if we
		 * start from 0)
		 */
		start_frame = (qhead->next_uframe) % PTD_FRAME_MASK;
		if (urb->dev->speed != USB_SPEED_HIGH){
			qhead->next_uframe =
				start_frame + urb->number_of_packets;
				iNumofPTDs=urb->number_of_packets;
		} else {
			qhead->next_uframe = start_frame + iNumofPTDs;
		}

		qhead->next_uframe %= PTD_FRAME_MASK;
	}


	iso_dbg(ISO_DBG_DATA, "[phcd_submit_iso]: Start frame index: %ld\n",
		start_frame);
	iso_dbg(ISO_DBG_DATA, "[phcd_submit_iso]: Max packet: %d\n",
		(int) max_pkt);

#ifdef COMMON_MEMORY
	if(urb->number_of_packets>8 && urb->dev->speed!=USB_SPEED_HIGH)
		phci_hcd_mem_alloc(8*max_pkt, &qhead->memory_addr, 0);
	else
	phci_hcd_mem_alloc(urb->transfer_buffer_length, &qhead->memory_addr, 0);
	if (urb->transfer_buffer_length && ((qhead->memory_addr.phy_addr == 0)
		|| (qhead->memory_addr.virt_addr ==0))) {
		iso_dbg(ISO_DBG_ERR,
			"[URB FILL MEMORY Error]: No payload memory available\n");
		return -ENOMEM;
	}
#endif

	if (urb->dev->speed != USB_SPEED_HIGH) {
		iNumofPks = urb->number_of_packets;
		qhead->totalptds=urb->number_of_packets;
		qhead->actualptds=0;

		/* Make as many tds as number of packets */
		for (packets = 0; packets < urb->number_of_packets; packets++) {
			/*
			 * Allocate memory for the SITD data structure and initialize it.
			 *
			 * This data structure follows the format of the SITD
			 * structure defined by the EHCI standard on the top part
			 * but also contains specific elements in the bottom
			 * part
			 */
			sitd = kmalloc(sizeof(*sitd), GFP_ATOMIC);
			if (!sitd) {
				*status = -ENOMEM;
				if (((int)(qhead->next_uframe -
					urb->number_of_packets)) < 0){
					/*plus max PTDs*/
					qhead->next_uframe = qhead->next_uframe + PTD_PERIODIC_SIZE;	
					
				}
				qhead->next_uframe -= urb->number_of_packets;

				/* Handle SITD list cleanup */
				if (urb->hcpriv) {
					phcd_iso_sitd_free_list(hcd, urb, 
						*status);
				}
				iso_dbg(ISO_DBG_ERR,
					"[phcd_submit_iso Error]: No memory available\n");
				return *status;
			}

			memset(sitd, 0, sizeof(struct ehci_sitd));

			INIT_LIST_HEAD(&sitd->sitd_list);

			sitd->sitd_dma = (u32) (sitd);
			sitd->urb = urb;

			/*
			 * Indicate that this SITD is the last in the list.
			 *
			 * Also set the itd_index to TD_PTD_INV_PTD_INDEX
			 * (0xFFFFFFFF). This would indicate when we allocate
			 * a PTD that this SITD did not have a PTD allocated
			 * before.
			 */

			sitd->hw_next = EHCI_LIST_END;
			sitd->sitd_index = TD_PTD_INV_PTD_INDEX;

			/* This SITD will go into this frame */
			sitd->framenumber = start_frame + packets;
			sitd->start_frame = temp + packets;

			/* Number of the packet */
			sitd->index = packets;

			sitd->framenumber = sitd->framenumber & PTD_FRAME_MASK;
			sitd->ssplit = qhead->ssplit;
			sitd->csplit = qhead->csplit;

			/* Initialize the following elements of the ITS structure
			 *      > sitd->length = length;                 -- the size of the request
			 *      > sitd->multi = multi;                   -- the number of transactions for
			 *                                         this EP per micro frame
			 *      > sitd->hw_bufp[0] = buf_dma;    -- The base address of the buffer where
			 *                                         to put the data (this base address was
			 *                                         the buffer provided plus the offset)
			 * And then, allocating memory from the PAYLOAD memory area, where the data
			 * coming from the requesting party will be placed or data requested by the
			 * requesting party will be retrieved when it is available.
			 */
			*status = phcd_iso_sitd_fill(hcd, sitd, urb, packets);

			if (*status != 0) {
				if (((int)(qhead->next_uframe - 
					urb->number_of_packets)) < 0){
					/*plus max PTDs*/
					qhead->next_uframe = qhead->next_uframe + 
						PTD_PERIODIC_SIZE;	
				}
				qhead->next_uframe -= urb->number_of_packets;

				/* Handle SITD list cleanup */
				if (urb->hcpriv) {
					phcd_iso_sitd_free_list(hcd, urb,
						*status);
				}
				iso_dbg(ISO_DBG_ERR,
					"[phcd_submit_iso Error]: Error in filling up SITD\n");
				return *status;
			}

			/*
			 * If this SITD is not the head/root SITD, link this SITD to the SITD
			 * that came before it.
			 */
			if (prev_sitd) {
				prev_sitd->hw_next = (u32) (sitd);
			}

			prev_sitd = sitd;

			if(packets<8){  //bcs of memory constraint , we use only first 8 PTDs if number_of_packets is more than 8.
			/*
			 * Allocate an ISO PTD from the ISO PTD map list and
			 * set the equivalent bit of the allocated PTD to active
			 * in the bitmap so that this PTD will be included into
			 * the periodic schedule
			 */
			phcd_iso_get_sitd_ptd_index(hcd, sitd);
			iso_dbg(ISO_DBG_DATA,
				"[phcd_submit_iso]: SITD index %d\n",
				sitd->sitd_index);

			/*if we dont have any space left */
			if (sitd->sitd_index == TD_PTD_INV_PTD_INDEX) {
				*status = -ENOSPC;
				if (((int) (qhead->next_uframe -
					urb->number_of_packets)) < 0){
					/*plus max PTDs*/
					qhead->next_uframe = qhead->next_uframe + PTD_PERIODIC_SIZE;	
				}
				qhead->next_uframe -= urb->number_of_packets;

				/* Handle SITD list cleanup */
				if (urb->hcpriv) {
					phcd_iso_sitd_free_list(hcd, urb,
						*status);
				}
				return *status;
			}
					qhead->actualptds++;
			}
			/* Insert this td into the periodic list */

			sitd_itd_list = &qhead->periodic_list.sitd_itd_head;
			list_add_tail(&sitd->sitd_list, sitd_itd_list);
			qhead->periodic_list.high_speed = 0;
			if(sitd->sitd_index!=TD_PTD_INV_PTD_INDEX)
			qhead->periodic_list.ptdlocation |=
				0x1 << sitd->sitd_index;
			/* Inidcate that a new SITD have been scheduled */
			hcd->periodic_sched++;

			/* Determine if there are any SITD scheduled before this one. */
			if (urb->hcpriv == 0){
				urb->hcpriv = sitd;
			}
		}	/* for(packets = 0; packets... */
	} else if (urb->dev->speed == USB_SPEED_HIGH) {	
		iNumofPks = iNumofPTDs;

		packets = 0;
		iPTDIndex = 0;
		while (packets < urb->number_of_packets) {
			iNumofSlots = NUMMICROFRAME / urb->interval;
			/*
			 * Allocate memory for the ITD data structure and initialize it.
			 *
			 * This data structure follows the format of the ITD
			 * structure defined by the EHCI standard on the top part
			 * but also contains specific elements in the bottom
			 * part
			 */
			itd = kmalloc(sizeof(*itd), GFP_ATOMIC);
			if (!itd) {
				*status = -ENOMEM;
				if(((int) (qhead->next_uframe - iNumofPTDs))<0){
					/*plus max PTDs*/
					qhead->next_uframe = qhead->next_uframe + 
						PTD_PERIODIC_SIZE;	
				}
				qhead->next_uframe -= iNumofPTDs;

				/* Handle ITD list cleanup */
				if (urb->hcpriv) {
					phcd_iso_itd_free_list(hcd, urb,
							       *status);
				}
				iso_dbg(ISO_DBG_ERR,
					"[phcd_submit_iso Error]: No memory available\n");
				return *status;
			}
			memset(itd, 0, sizeof(struct ehci_itd));

			INIT_LIST_HEAD(&itd->itd_list);

			itd->itd_dma = (u32) (itd);
			itd->urb = urb;
			/*
			 * Indicate that this ITD is the last in the list.
			 *
			 * Also set the itd_index to TD_PTD_INV_PTD_INDEX
			 * (0xFFFFFFFF). This would indicate when we allocate
			 * a PTD that this SITD did not have a PTD allocated
			 * before.
			 */

			itd->hw_next = EHCI_LIST_END;
			itd->itd_index = TD_PTD_INV_PTD_INDEX;
			/* This ITD will go into this frame */
			itd->framenumber = start_frame + iPTDIndex;
			/* Number of the packet */
			itd->index = packets;

			itd->framenumber = itd->framenumber & 0x1F;

			itd->ssplit = qhead->ssplit;
			itd->csplit = qhead->csplit;

			/*caculate the number of packets for this itd */
			itd->num_of_pkts = iNumofSlots * mult;
			/*for the case , urb number_of_packets is less than (number of slot*mult*x times) */
			if (itd->num_of_pkts >= urb->number_of_packets)
			{
				itd->num_of_pkts = urb->number_of_packets;
			}
			else {
				if (itd->num_of_pkts >
					urb->number_of_packets - packets){
					itd->num_of_pkts =
						urb->number_of_packets -
						packets;
				}
			}

			/* Initialize the following elements of the ITS structure
			 *      > itd->length = length;                 -- the size of the request
			 *      > itd->multi = multi;                   -- the number of transactions for
			 *                                         this EP per micro frame
			 *      > itd->hw_bufp[0] = buf_dma;    -- The base address of the buffer where
			 *                                         to put the data (this base address was
			 *                                         the buffer provided plus the offset)
			 * And then, allocating memory from the PAYLOAD memory area, where the data
			 * coming from the requesting party will be placed or data requested by the
			 * requesting party will be retrieved when it is available.
			 */
			iso_dbg(ISO_DBG_DATA,
				"[phcd_submit_iso] packets index = %ld itd->num_of_pkts = %d\n",
				packets, itd->num_of_pkts);
			*status =
				phcd_iso_itd_fill(hcd, itd, urb, packets,
						itd->num_of_pkts);
			if (*status != 0) {
				if (((int) (qhead->next_uframe - iNumofPTDs)) <
					0) {
					qhead->next_uframe = qhead->next_uframe + PTD_PERIODIC_SIZE;	/*plus max PTDs*/
				}
				qhead->next_uframe -= iNumofPTDs;

				/* Handle SITD list cleanup */
				if (urb->hcpriv) {
					phcd_iso_itd_free_list(hcd, urb,
						*status);
				}
				iso_dbg(ISO_DBG_ERR,
					"[phcd_submit_iso Error]: Error in filling up ITD\n");
				return *status;
			}

			iPG = 0;
			iMicroIndex = 0;
			while (iNumofSlots > 0) {
				offset = urb->iso_frame_desc[packets].offset;
				/* Buffer for this packet */
				buff_dma =
					(u32) ((unsigned char *) urb->
						transfer_buffer + offset);

				/*for the case mult is 2 or 3 */
				length = 0;
				for (i = packets; i < packets + mult; i++) {
					length += urb->iso_frame_desc[i].length;
				}
				itd->hw_transaction[iMicroIndex] =
					EHCI_ISOC_ACTIVE | (length & 
					EHCI_ITD_TRANLENGTH)
					<< 16 | iPG << 12 | buff_dma;
					
				if (itd->hw_bufp[iPG] != buff_dma){
					itd->hw_bufp[++iPG] = buff_dma;
				}

				iso_dbg(ISO_DBG_DATA,
					"[%s] offset : %ld buff_dma : 0x%08x length : %ld\n",
					__FUNCTION__, offset,
					(unsigned int) buff_dma, length);

				itd->ssplit |= 1 << iMicroIndex;
				packets++;
				iMicroIndex += urb->interval;
				iNumofSlots--;

				/*last packets or last slot */
				if (packets == urb->number_of_packets
					|| iNumofSlots == 0) {

					itd->hw_transaction[iMicroIndex] |=
						EHCI_ITD_IOC;

					break;
					
				}
			}

			/*
			 * If this SITD is not the head/root SITD, link this SITD to the SITD
			 * that came before it.
			 */
			if (prev_itd) {
				prev_itd->hw_next = (u32) (itd);
			}

			prev_itd = itd;

			/*
			 * Allocate an ISO PTD from the ISO PTD map list and
			 * set the equivalent bit of the allocated PTD to active
			 * in the bitmap so that this PTD will be included into
			 * the periodic schedule
			 */


			iso_dbg(ISO_DBG_DATA,
				"[phcd_submit_iso]: ITD index %d\n",
				itd->framenumber);
			phcd_iso_get_itd_ptd_index(hcd, itd);
			iso_dbg(ISO_DBG_DATA,
				"[phcd_submit_iso]: ITD index %d\n",
				itd->itd_index);

			/*if we dont have any space left */
			if (itd->itd_index == TD_PTD_INV_PTD_INDEX) {
				*status = -ENOSPC;
				if (((int) (qhead->next_uframe - iNumofPTDs)) <
					0){
					/*plus max PTDs*/
					qhead->next_uframe = qhead->next_uframe + PTD_PERIODIC_SIZE;	
				}
				qhead->next_uframe -= iNumofPTDs;

				/* Handle SITD list cleanup */
				if (urb->hcpriv) {
					phcd_iso_itd_free_list(hcd, urb,
							       *status);
				}
				return *status;
			}

			sitd_itd_list = &qhead->periodic_list.sitd_itd_head;
			list_add_tail(&itd->itd_list, sitd_itd_list);
			qhead->periodic_list.high_speed = 1;
			qhead->periodic_list.ptdlocation |=
				0x1 << itd->itd_index;

			/* Inidcate that a new SITD have been scheduled */
			hcd->periodic_sched++;

			/* Determine if there are any ITD scheduled before this one. */
			if (urb->hcpriv == 0){
				urb->hcpriv = itd;
			}
			iPTDIndex++;

		}		/*end of while */
	}

	/*end of HIGH SPEED */
	/* Last td of current transaction */
	if (high_speed == 0){
		sitd->hw_next = EHCI_LIST_END;
	}
	urb->error_count = 0;
	return *status;
}				/* phcd_submit_iso */
#endif /* CONFIG_ISO_SUPPORT */
