/*===========================================================================
FILE:
   QCUSBNet.c

DESCRIPTION:
   Qualcomm USB Network device for Gobi 2000 
   
FUNCTIONS:
   QCSuspend
   QCResume
   QCNetDriverBind
   QCNetDriverUnbind
   QCUSBNetURBCallback
   QCUSBNetTXTimeout
   QCUSBNetAutoPMThread
   QCUSBNetStartXmit
   QCUSBNetOpen
   QCUSBNetStop
   QCUSBNetProbe
   QCUSBNetModInit
   QCUSBNetModExit

Copyright (c) 2010, The Linux Foundation. All rights reserved.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 and
only version 2 as published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
02110-1301, USA.

===========================================================================*/

//---------------------------------------------------------------------------
// Include Files
//---------------------------------------------------------------------------

#include "Structs.h"
#include "QMIDevice.h"
#include "QMI.h"

//-----------------------------------------------------------------------------
// Definitions
//-----------------------------------------------------------------------------

// Version Information
#define DRIVER_VERSION "1.0.110"
#define DRIVER_DESC "QCUSBNet2k"

// Debug flag
int debug;

// Class should be created during module init, so needs to be global
static struct class * gpClass;

/*===========================================================================
METHOD:
   QCSuspend (Public Method)

DESCRIPTION:
   Stops QMI traffic while device is suspended

PARAMETERS
   pIntf          [ I ] - Pointer to interface
   powerEvent     [ I ] - Power management event

RETURN VALUE:
   int - 0 for success
         negative errno for failure
===========================================================================*/
int QCSuspend( 
   struct usb_interface *     pIntf,
   pm_message_t               powerEvent )
{
   struct usbnet * pDev;
   sQCUSBNet * pQCDev;
   
   if (pIntf == 0)
   {
      return -ENOMEM;
   }
   
#if (LINUX_VERSION_CODE > KERNEL_VERSION( 2,6,23 ))
   pDev = usb_get_intfdata( pIntf );
#else
   pDev = (struct usbnet *)pIntf->dev.platform_data;
#endif

   if (pDev == NULL || pDev->net == NULL)
   {
      DBG( "failed to get netdevice\n" );
      return -ENXIO;
   }
   
   pQCDev = (sQCUSBNet *)pDev->data[0];
   if (pQCDev == NULL)
   {
      DBG( "failed to get QMIDevice\n" );
      return -ENXIO;
   }

   // Is this autosuspend or system suspend?
   //    do we allow remote wakeup?
#if (LINUX_VERSION_CODE < KERNEL_VERSION( 2,6,33 ))
   if (pDev->udev->auto_pm == 0)
#else
   if ((powerEvent.event & PM_EVENT_AUTO) == 0)
#endif
   {
      DBG( "device suspended to power level %d\n", 
           powerEvent.event );
      QSetDownReason( pQCDev, DRIVER_SUSPENDED );
   }
   else
   {
      DBG( "device autosuspend\n" );
   }    

   if (powerEvent.event & PM_EVENT_SUSPEND)
   {
      // Stop QMI read callbacks
      KillRead( pQCDev );
      pDev->udev->reset_resume = 0;
      
      // Store power state to avoid duplicate resumes
      pIntf->dev.power.power_state.event = powerEvent.event;
   }
   else
   {
      // Other power modes cause QMI connection to be lost
      pDev->udev->reset_resume = 1;
   }
   
   // Run usbnet's suspend function
   return usbnet_suspend( pIntf, powerEvent );
}
   
/*===========================================================================
METHOD:
   QCResume (Public Method)

DESCRIPTION:
   Resume QMI traffic or recreate QMI device

PARAMETERS
   pIntf          [ I ] - Pointer to interface

RETURN VALUE:
   int - 0 for success
         negative errno for failure
===========================================================================*/
int QCResume( struct usb_interface * pIntf )
{
   struct usbnet * pDev;
   sQCUSBNet * pQCDev;
   int nRet;
   int oldPowerState;
   
   if (pIntf == 0)
   {
      return -ENOMEM;
   }
   
#if (LINUX_VERSION_CODE > KERNEL_VERSION( 2,6,23 ))
   pDev = usb_get_intfdata( pIntf );
#else
   pDev = (struct usbnet *)pIntf->dev.platform_data;
#endif

   if (pDev == NULL || pDev->net == NULL)
   {
      DBG( "failed to get netdevice\n" );
      return -ENXIO;
   }
   
   pQCDev = (sQCUSBNet *)pDev->data[0];
   if (pQCDev == NULL)
   {
      DBG( "failed to get QMIDevice\n" );
      return -ENXIO;
   }

   oldPowerState = pIntf->dev.power.power_state.event;
   pIntf->dev.power.power_state.event = PM_EVENT_ON;
   DBG( "resuming from power mode %d\n", oldPowerState );

   if (oldPowerState & PM_EVENT_SUSPEND)
   {
      // It doesn't matter if this is autoresume or system resume
      QClearDownReason( pQCDev, DRIVER_SUSPENDED );
   
      nRet = usbnet_resume( pIntf );
      if (nRet != 0)
      {
         DBG( "usbnet_resume error %d\n", nRet );
         return nRet;
      }

      // Restart QMI read callbacks
      nRet = StartRead( pQCDev );
      if (nRet != 0)
      {
         DBG( "StartRead error %d\n", nRet );
         return nRet;
      }

      // Kick Auto PM thread to process any queued URBs
      up( &pQCDev->mAutoPM.mThreadDoWork );
   }
   else
   {
      DBG( "nothing to resume\n" );
      return 0;
   }
   
   return nRet;
}

/*===========================================================================
METHOD:
   QCNetDriverBind (Public Method)

DESCRIPTION:
   Setup in and out pipes

PARAMETERS
   pDev           [ I ] - Pointer to usbnet device
   pIntf          [ I ] - Pointer to interface

RETURN VALUE:
   int - 0 for success
         Negative errno for error
===========================================================================*/
static int QCNetDriverBind( 
   struct usbnet *         pDev, 
   struct usb_interface *  pIntf )
{
   int numEndpoints;
   int endpointIndex;
   struct usb_host_endpoint * pEndpoint = NULL;
   struct usb_host_endpoint * pIn = NULL;
   struct usb_host_endpoint * pOut = NULL;
   
   // Verify one altsetting
   if (pIntf->num_altsetting != 1)
   {
      DBG( "invalid num_altsetting %u\n", pIntf->num_altsetting );
      return -EINVAL;
   }

   // Verify correct interface (0)
   if (pIntf->cur_altsetting->desc.bInterfaceNumber != 0)
   {
      DBG( "invalid interface %d\n", 
           pIntf->cur_altsetting->desc.bInterfaceNumber );
      return -EINVAL;
   }
   
   // Collect In and Out endpoints
   numEndpoints = pIntf->cur_altsetting->desc.bNumEndpoints;
   for (endpointIndex = 0; endpointIndex < numEndpoints; endpointIndex++)
   {
      pEndpoint = pIntf->cur_altsetting->endpoint + endpointIndex;
      if (pEndpoint == NULL)
      {
         DBG( "invalid endpoint %u\n", endpointIndex );
         return -EINVAL;
      }
      
      if (usb_endpoint_dir_in( &pEndpoint->desc ) == true
      &&  usb_endpoint_xfer_int( &pEndpoint->desc ) == false)
      {
         pIn = pEndpoint;
      }
      else if (usb_endpoint_dir_out( &pEndpoint->desc ) == true)
      {
         pOut = pEndpoint;
      }
   }
   
   if (pIn == NULL || pOut == NULL)
   {
      DBG( "invalid endpoints\n" );
      return -EINVAL;
   }

   if (usb_set_interface( pDev->udev, 
                          pIntf->cur_altsetting->desc.bInterfaceNumber,
                          0 ) != 0)
   {
      DBG( "unable to set interface\n" );
      return -EINVAL;
   }

   pDev->in = usb_rcvbulkpipe( pDev->udev,
                   pIn->desc.bEndpointAddress & USB_ENDPOINT_NUMBER_MASK );
   pDev->out = usb_sndbulkpipe( pDev->udev,
                   pOut->desc.bEndpointAddress & USB_ENDPOINT_NUMBER_MASK );
                   
   DBG( "in %x, out %x\n", 
        pIn->desc.bEndpointAddress, 
        pOut->desc.bEndpointAddress );

   // In later versions of the kernel, usbnet helps with this
#if (LINUX_VERSION_CODE <= KERNEL_VERSION( 2,6,23 ))
   pIntf->dev.platform_data = (void *)pDev;
#endif

   return 0;
}

/*===========================================================================
METHOD:
   QCNetDriverUnbind (Public Method)

DESCRIPTION:
   Deregisters QMI device (Registration happened in the probe function)

PARAMETERS
   pDev           [ I ] - Pointer to usbnet device
   pIntfUnused    [ I ] - Pointer to interface

RETURN VALUE:
   None
===========================================================================*/
static void QCNetDriverUnbind( 
   struct usbnet *         pDev, 
   struct usb_interface *  pIntf)
{
   sQCUSBNet * pQCDev = (sQCUSBNet *)pDev->data[0];

   // Should already be down, but just in case...
   netif_carrier_off( pDev->net );

   DeregisterQMIDevice( pQCDev );
   
#if (LINUX_VERSION_CODE >= KERNEL_VERSION( 2,6,29 ))
   kfree( pDev->net->netdev_ops );
   pDev->net->netdev_ops = NULL;
#endif

#if (LINUX_VERSION_CODE <= KERNEL_VERSION( 2,6,23 ))
   pIntf->dev.platform_data = NULL;
#endif

   kfree( pQCDev );
   pQCDev = NULL;
}

/*===========================================================================
METHOD:
   QCUSBNetURBCallback (Public Method)

DESCRIPTION:
   Write is complete, cleanup and signal that we're ready for next packet

PARAMETERS
   pURB     [ I ] - Pointer to sAutoPM struct

RETURN VALUE:
   None
===========================================================================*/
void QCUSBNetURBCallback( struct urb * pURB )
{
   unsigned long activeURBflags;
   sAutoPM * pAutoPM = (sAutoPM *)pURB->context;
   if (pAutoPM == NULL)
   {
      // Should never happen
      DBG( "bad context\n" );
      return;
   }

   if (pURB->status != 0)
   {
      // Note that in case of an error, the behaviour is no different
      DBG( "urb finished with error %d\n", pURB->status );
   }

   // Remove activeURB (memory to be freed later)
   spin_lock_irqsave( &pAutoPM->mActiveURBLock, activeURBflags );

   // EAGAIN used to signify callback is done
   pAutoPM->mpActiveURB = ERR_PTR( -EAGAIN );

   spin_unlock_irqrestore( &pAutoPM->mActiveURBLock, activeURBflags );

   up( &pAutoPM->mThreadDoWork );
   
   usb_free_urb( pURB );
}

/*===========================================================================
METHOD:
   QCUSBNetTXTimeout (Public Method)

DESCRIPTION:
   Timeout declared by the net driver.  Stop all transfers

PARAMETERS
   pNet     [ I ] - Pointer to net device

RETURN VALUE:
   None
===========================================================================*/
void QCUSBNetTXTimeout( struct net_device * pNet )
{
   struct sQCUSBNet * pQCDev;
   sAutoPM * pAutoPM;
   sURBList * pURBListEntry;
   unsigned long activeURBflags, URBListFlags;
   struct usbnet * pDev = netdev_priv( pNet );

   if (pDev == NULL || pDev->net == NULL)
   {
      DBG( "failed to get usbnet device\n" );
      return;
   }
   
   pQCDev = (sQCUSBNet *)pDev->data[0];
   if (pQCDev == NULL)
   {
      DBG( "failed to get QMIDevice\n" );
      return;
   }
   pAutoPM = &pQCDev->mAutoPM;

   DBG( "\n" );

   // Stop activeURB
   spin_lock_irqsave( &pAutoPM->mActiveURBLock, activeURBflags );

   if (pAutoPM->mpActiveURB != NULL)
   {
      usb_kill_urb( pAutoPM->mpActiveURB );
   }

   spin_unlock_irqrestore( &pAutoPM->mActiveURBLock, activeURBflags );

   // Cleanup URB List
   spin_lock_irqsave( &pAutoPM->mURBListLock, URBListFlags );

   pURBListEntry = pAutoPM->mpURBList;
   while (pURBListEntry != NULL)
   {
      pAutoPM->mpURBList = pAutoPM->mpURBList->mpNext;
      usb_free_urb( pURBListEntry->mpURB );
      kfree( pURBListEntry );
      pURBListEntry = pAutoPM->mpURBList;
   }

   spin_unlock_irqrestore( &pAutoPM->mURBListLock, URBListFlags );

   up( &pAutoPM->mThreadDoWork );

   return;
}

/*===========================================================================
METHOD:
   QCUSBNetAutoPMThread (Public Method)

DESCRIPTION:
   Handle device Auto PM state asynchronously
   Handle network packet transmission asynchronously

PARAMETERS
   pData     [ I ] - Pointer to sAutoPM struct

RETURN VALUE:
   int - 0 for success
         Negative errno for error
===========================================================================*/
static int QCUSBNetAutoPMThread( void * pData )
{
   unsigned long activeURBflags, URBListFlags;
   sURBList * pURBListEntry;
   int status;
   struct usb_device * pUdev;
   sAutoPM * pAutoPM = (sAutoPM *)pData;
   if (pAutoPM == NULL)
   {
      DBG( "passed null pointer\n" );
      return -EINVAL;
   }
   
   pUdev = interface_to_usbdev( pAutoPM->mpIntf );

   DBG( "traffic thread started\n" );

   while (pAutoPM->mbExit == false)
   {
      // Wait for someone to poke us
      down( &pAutoPM->mThreadDoWork );

      // Time to exit?
      if (pAutoPM->mbExit == true)
      {
         // Stop activeURB
         spin_lock_irqsave( &pAutoPM->mActiveURBLock, activeURBflags );

         if (pAutoPM->mpActiveURB != NULL)
         {
            usb_kill_urb( pAutoPM->mpActiveURB );
         }
         // Will be freed in callback function

         spin_unlock_irqrestore( &pAutoPM->mActiveURBLock, activeURBflags );

         // Cleanup URB List
         spin_lock_irqsave( &pAutoPM->mURBListLock, URBListFlags );

         pURBListEntry = pAutoPM->mpURBList;
         while (pURBListEntry != NULL)
         {
            pAutoPM->mpURBList = pAutoPM->mpURBList->mpNext;
            usb_free_urb( pURBListEntry->mpURB );
            kfree( pURBListEntry );
            pURBListEntry = pAutoPM->mpURBList;
         }

         spin_unlock_irqrestore( &pAutoPM->mURBListLock, URBListFlags );

         break;
      }
      
      // Is our URB active?
      spin_lock_irqsave( &pAutoPM->mActiveURBLock, activeURBflags );

      // EAGAIN used to signify callback is done
      if (IS_ERR( pAutoPM->mpActiveURB ) 
      &&  PTR_ERR( pAutoPM->mpActiveURB ) == -EAGAIN )
      {
         pAutoPM->mpActiveURB = NULL;

         // Restore IRQs so task can sleep
         spin_unlock_irqrestore( &pAutoPM->mActiveURBLock, activeURBflags );
         
         // URB is done, decrement the Auto PM usage count
         usb_autopm_put_interface( pAutoPM->mpIntf );

         // Lock ActiveURB again
         spin_lock_irqsave( &pAutoPM->mActiveURBLock, activeURBflags );
      }

      if (pAutoPM->mpActiveURB != NULL)
      {
         // There is already a URB active, go back to sleep
         spin_unlock_irqrestore( &pAutoPM->mActiveURBLock, activeURBflags );
         continue;
      }
      
      // Is there a URB waiting to be submitted?
      spin_lock_irqsave( &pAutoPM->mURBListLock, URBListFlags );
      if (pAutoPM->mpURBList == NULL)
      {
         // No more URBs to submit, go back to sleep
         spin_unlock_irqrestore( &pAutoPM->mURBListLock, URBListFlags );
         spin_unlock_irqrestore( &pAutoPM->mActiveURBLock, activeURBflags );
         continue;
      }

      // Pop an element
      pURBListEntry = pAutoPM->mpURBList;
      pAutoPM->mpURBList = pAutoPM->mpURBList->mpNext;
      spin_unlock_irqrestore( &pAutoPM->mURBListLock, URBListFlags );

      // Set ActiveURB
      pAutoPM->mpActiveURB = pURBListEntry->mpURB;
      spin_unlock_irqrestore( &pAutoPM->mActiveURBLock, activeURBflags );

      // Tell autopm core we need device woken up
      status = usb_autopm_get_interface( pAutoPM->mpIntf );
      if (status < 0)
      {
         DBG( "unable to autoresume interface: %d\n", status );

         // likely caused by device going from autosuspend -> full suspend
         if (status == -EPERM)
         {
#if (LINUX_VERSION_CODE < KERNEL_VERSION( 2,6,33 ))
            pUdev->auto_pm = 0;
#endif
            QCSuspend( pAutoPM->mpIntf, PMSG_SUSPEND );
         }

         // Add pURBListEntry back onto pAutoPM->mpURBList
         spin_lock_irqsave( &pAutoPM->mURBListLock, URBListFlags );
         pURBListEntry->mpNext = pAutoPM->mpURBList;
         pAutoPM->mpURBList = pURBListEntry;
         spin_unlock_irqrestore( &pAutoPM->mURBListLock, URBListFlags );
         
         spin_lock_irqsave( &pAutoPM->mActiveURBLock, activeURBflags );
         pAutoPM->mpActiveURB = NULL;
         spin_unlock_irqrestore( &pAutoPM->mActiveURBLock, activeURBflags );
         
         // Go back to sleep
         continue;
      }

      // Submit URB
      status = usb_submit_urb( pAutoPM->mpActiveURB, GFP_KERNEL );
      if (status < 0)
      {
         // Could happen for a number of reasons
         DBG( "Failed to submit URB: %d.  Packet dropped\n", status );
         spin_lock_irqsave( &pAutoPM->mActiveURBLock, activeURBflags );
         usb_free_urb( pAutoPM->mpActiveURB );
         pAutoPM->mpActiveURB = NULL;
         spin_unlock_irqrestore( &pAutoPM->mActiveURBLock, activeURBflags );
         usb_autopm_put_interface( pAutoPM->mpIntf );

         // Loop again
         up( &pAutoPM->mThreadDoWork );
      }
      
      kfree( pURBListEntry );
   }   
   
   DBG( "traffic thread exiting\n" );
   pAutoPM->mpThread = NULL;
   return 0;
}      

/*===========================================================================
METHOD:
   QCUSBNetStartXmit (Public Method)

DESCRIPTION:
   Convert sk_buff to usb URB and queue for transmit

PARAMETERS
   pNet     [ I ] - Pointer to net device

RETURN VALUE:
   NETDEV_TX_OK on success
   NETDEV_TX_BUSY on error
===========================================================================*/
int QCUSBNetStartXmit( 
   struct sk_buff *     pSKB,
   struct net_device *  pNet )
{
   unsigned long URBListFlags;
   struct sQCUSBNet * pQCDev;
   sAutoPM * pAutoPM;
   sURBList * pURBListEntry, ** ppURBListEnd;
   void * pURBData;
   struct usbnet * pDev = netdev_priv( pNet );
   
   DBG( "\n" );
   
   if (pDev == NULL || pDev->net == NULL)
   {
      DBG( "failed to get usbnet device\n" );
      return NETDEV_TX_BUSY;
   }
   
   pQCDev = (sQCUSBNet *)pDev->data[0];
   if (pQCDev == NULL)
   {
      DBG( "failed to get QMIDevice\n" );
      return NETDEV_TX_BUSY;
   }
   pAutoPM = &pQCDev->mAutoPM;
   
   if (QTestDownReason( pQCDev, DRIVER_SUSPENDED ) == true)
   {
      // Should not happen
      DBG( "device is suspended\n" );
      dump_stack();
      return NETDEV_TX_BUSY;
   }
   
   // Convert the sk_buff into a URB

   // Allocate URBListEntry
   pURBListEntry = kmalloc( sizeof( sURBList ), GFP_ATOMIC );
   if (pURBListEntry == NULL)
   {
      DBG( "unable to allocate URBList memory\n" );
      return NETDEV_TX_BUSY;
   }
   pURBListEntry->mpNext = NULL;

   // Allocate URB
   pURBListEntry->mpURB = usb_alloc_urb( 0, GFP_ATOMIC );
   if (pURBListEntry->mpURB == NULL)
   {
      DBG( "unable to allocate URB\n" );
      return NETDEV_TX_BUSY;
   }

   // Allocate URB transfer_buffer
   pURBData = kmalloc( pSKB->len, GFP_ATOMIC );
   if (pURBData == NULL)
   {
      DBG( "unable to allocate URB data\n" );
      return NETDEV_TX_BUSY;
   }
   // Fill will SKB's data
   memcpy( pURBData, pSKB->data, pSKB->len );

   usb_fill_bulk_urb( pURBListEntry->mpURB,
                      pQCDev->mpNetDev->udev,
                      pQCDev->mpNetDev->out,
                      pURBData,
                      pSKB->len,
                      QCUSBNetURBCallback,
                      pAutoPM );
   
   // Aquire lock on URBList
   spin_lock_irqsave( &pAutoPM->mURBListLock, URBListFlags );
   
   // Add URB to end of list
   ppURBListEnd = &pAutoPM->mpURBList;
   while ((*ppURBListEnd) != NULL)
   {
      ppURBListEnd = &(*ppURBListEnd)->mpNext;
   }
   *ppURBListEnd = pURBListEntry;

   spin_unlock_irqrestore( &pAutoPM->mURBListLock, URBListFlags );

   up( &pAutoPM->mThreadDoWork );

   // Start transfer timer
   pNet->trans_start = jiffies;
   // Free SKB
   dev_kfree_skb_any( pSKB );

   return NETDEV_TX_OK;
}

/*===========================================================================
METHOD:
   QCUSBNetOpen (Public Method)

DESCRIPTION:
   Wrapper to usbnet_open, correctly handling autosuspend
   Start AutoPM thread

PARAMETERS
   pNet     [ I ] - Pointer to net device

RETURN VALUE:
   int - 0 for success
         Negative errno for error
===========================================================================*/
int QCUSBNetOpen( struct net_device * pNet )
{
   int status = 0;
   struct sQCUSBNet * pQCDev;
   struct usbnet * pDev = netdev_priv( pNet );
   
   if (pDev == NULL)
   {
      DBG( "failed to get usbnet device\n" );
      return -ENXIO;
   }
   
   pQCDev = (sQCUSBNet *)pDev->data[0];
   if (pQCDev == NULL)
   {
      DBG( "failed to get QMIDevice\n" );
      return -ENXIO;
   }

   DBG( "\n" );

   // Start the AutoPM thread
   pQCDev->mAutoPM.mpIntf = pQCDev->mpIntf;
   pQCDev->mAutoPM.mbExit = false;
   pQCDev->mAutoPM.mpURBList = NULL;
   pQCDev->mAutoPM.mpActiveURB = NULL;
   spin_lock_init( &pQCDev->mAutoPM.mURBListLock );
   spin_lock_init( &pQCDev->mAutoPM.mActiveURBLock );
   sema_init( &pQCDev->mAutoPM.mThreadDoWork, 0 );
   
   pQCDev->mAutoPM.mpThread = kthread_run( QCUSBNetAutoPMThread, 
                              &pQCDev->mAutoPM, 
                              "QCUSBNetAutoPMThread" );
   if (IS_ERR( pQCDev->mAutoPM.mpThread ))
   {
      DBG( "AutoPM thread creation error\n" );
      return PTR_ERR( pQCDev->mAutoPM.mpThread );
   }

   // Allow traffic
   QClearDownReason( pQCDev, NET_IFACE_STOPPED );

   // Pass to usbnet_open if defined
   if (pQCDev->mpUSBNetOpen != NULL)
   {
      status = pQCDev->mpUSBNetOpen( pNet );
   
      // If usbnet_open was successful enable Auto PM
      if (status == 0)
      {
#if (LINUX_VERSION_CODE < KERNEL_VERSION( 2,6,33 ))
         usb_autopm_enable( pQCDev->mpIntf );
#else
         usb_autopm_put_interface( pQCDev->mpIntf );
#endif
      }
   }
   else
   {
      DBG( "no USBNetOpen defined\n" );
   }
   
   return status;
}

/*===========================================================================
METHOD:
   QCUSBNetStop (Public Method)

DESCRIPTION:
   Wrapper to usbnet_stop, correctly handling autosuspend
   Stop AutoPM thread

PARAMETERS
   pNet     [ I ] - Pointer to net device

RETURN VALUE:
   int - 0 for success
         Negative errno for error
===========================================================================*/
int QCUSBNetStop( struct net_device * pNet )
{
   struct sQCUSBNet * pQCDev;
   struct usbnet * pDev = netdev_priv( pNet );

   if (pDev == NULL || pDev->net == NULL)
   {
      DBG( "failed to get netdevice\n" );
      return -ENXIO;
   }
   
   pQCDev = (sQCUSBNet *)pDev->data[0];
   if (pQCDev == NULL)
   {
      DBG( "failed to get QMIDevice\n" );
      return -ENXIO;
   }

   // Stop traffic
   QSetDownReason( pQCDev, NET_IFACE_STOPPED );

   // Tell traffic thread to exit
   pQCDev->mAutoPM.mbExit = true;
   up( &pQCDev->mAutoPM.mThreadDoWork );
   
   // Wait for it to exit
   while( pQCDev->mAutoPM.mpThread != NULL )
   {
      msleep( 100 );
   }
   DBG( "thread stopped\n" );

   // Pass to usbnet_stop, if defined
   if (pQCDev->mpUSBNetStop != NULL)
   {
      return pQCDev->mpUSBNetStop( pNet );
   }
   else
   {
      return 0;
   }
}

/*=========================================================================*/
// Struct driver_info
/*=========================================================================*/
static const struct driver_info QCNetInfo = 
{
   .description   = "QCUSBNet Ethernet Device",
   .flags         = FLAG_ETHER,
   .bind          = QCNetDriverBind,
   .unbind        = QCNetDriverUnbind,
   .data          = 0,
};

/*=========================================================================*/
// Qualcomm Gobi 2000 VID/PIDs
/*=========================================================================*/
static const struct usb_device_id QCVIDPIDTable [] =
{
   // Acer Gobi 2000
   {
      USB_DEVICE( 0x05c6, 0x9215 ),
      .driver_info = (unsigned long)&QCNetInfo
   },
   // Asus Gobi 2000
   {
      USB_DEVICE( 0x05c6, 0x9265 ),
      .driver_info = (unsigned long)&QCNetInfo
   },
   // CMOTech Gobi 2000
   {
      USB_DEVICE( 0x16d8, 0x8002 ),
      .driver_info = (unsigned long)&QCNetInfo
   },
   // Dell Gobi 2000
   {
      USB_DEVICE( 0x413c, 0x8186 ),
      .driver_info = (unsigned long)&QCNetInfo
   },
   // Entourage Gobi 2000
   {
      USB_DEVICE( 0x1410, 0xa010 ),
      .driver_info = (unsigned long)&QCNetInfo
   },
   // Entourage Gobi 2000
   {
      USB_DEVICE( 0x1410, 0xa011 ),
      .driver_info = (unsigned long)&QCNetInfo
   },
   // Entourage Gobi 2000
   {
      USB_DEVICE( 0x1410, 0xa012 ),
      .driver_info = (unsigned long)&QCNetInfo
   },
   // Entourage Gobi 2000
   {
      USB_DEVICE( 0x1410, 0xa013 ),
      .driver_info = (unsigned long)&QCNetInfo
   },
   // HP Gobi 2000
   { 
      USB_DEVICE( 0x03f0, 0x251d ),
      .driver_info = (unsigned long)&QCNetInfo 
   },
   // Lenovo Gobi 2000
   {
      USB_DEVICE( 0x05c6, 0x9205 ),
      .driver_info = (unsigned long)&QCNetInfo
   },
   // Panasonic Gobi 2000
   {
      USB_DEVICE( 0x04da, 0x250f ),
      .driver_info = (unsigned long)&QCNetInfo
   },
   // Samsung Gobi 2000
   {
      USB_DEVICE( 0x05c6, 0x9245 ),
      .driver_info = (unsigned long)&QCNetInfo
   },
   // Sierra Wireless Gobi 2000
   {
      USB_DEVICE( 0x1199, 0x9001 ),
      .driver_info = (unsigned long)&QCNetInfo
   },
   // Sierra Wireless Gobi 2000
   {
      USB_DEVICE( 0x1199, 0x9002 ),
      .driver_info = (unsigned long)&QCNetInfo
   },
   // Sierra Wireless Gobi 2000
   {
      USB_DEVICE( 0x1199, 0x9003 ),
      .driver_info = (unsigned long)&QCNetInfo
   },
   // Sierra Wireless Gobi 2000
   {
      USB_DEVICE( 0x1199, 0x9004 ),
      .driver_info = (unsigned long)&QCNetInfo
   },
   // Sierra Wireless Gobi 2000
   {
      USB_DEVICE( 0x1199, 0x9005 ),
      .driver_info = (unsigned long)&QCNetInfo
   },
   // Sierra Wireless Gobi 2000
   {
      USB_DEVICE( 0x1199, 0x9006 ),
      .driver_info = (unsigned long)&QCNetInfo
   },
   // Sierra Wireless Gobi 2000
   {
      USB_DEVICE( 0x1199, 0x9007 ),
      .driver_info = (unsigned long)&QCNetInfo
   },
   // Sierra Wireless Gobi 2000
   {
      USB_DEVICE( 0x1199, 0x9008 ),
      .driver_info = (unsigned long)&QCNetInfo
   },
   // Sierra Wireless Gobi 2000
   {
      USB_DEVICE( 0x1199, 0x9009 ),
      .driver_info = (unsigned long)&QCNetInfo
   },
   // Sierra Wireless Gobi 2000
   {
      USB_DEVICE( 0x1199, 0x900a ),
      .driver_info = (unsigned long)&QCNetInfo
   },
   // Sony Gobi 2000
   {
      USB_DEVICE( 0x05c6, 0x9225 ),
      .driver_info = (unsigned long)&QCNetInfo
   },
   // Top Global Gobi 2000
   {
      USB_DEVICE( 0x05c6, 0x9235 ),
      .driver_info = (unsigned long)&QCNetInfo
   },
   // iRex Technologies Gobi 2000
   {
      USB_DEVICE( 0x05c6, 0x9275 ),
      .driver_info = (unsigned long)&QCNetInfo
   },
   // Generic Gobi 2000
   {
      USB_DEVICE( 0x5c6, 0x920B ),
      .driver_info = (unsigned long)&QCNetInfo
   },

   //Terminating entry
   { }
};

MODULE_DEVICE_TABLE( usb, QCVIDPIDTable );

/*===========================================================================
METHOD:
   QCUSBNetProbe (Public Method)

DESCRIPTION:
   Run usbnet_probe
   Setup QMI device

PARAMETERS
   pIntf        [ I ] - Pointer to interface
   pVIDPIDs     [ I ] - Pointer to VID/PID table

RETURN VALUE:
   int - 0 for success
         Negative errno for error
===========================================================================*/
int QCUSBNetProbe( 
   struct usb_interface *        pIntf, 
   const struct usb_device_id *  pVIDPIDs )
{
   int status;
   struct usbnet * pDev;
   sQCUSBNet * pQCDev;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION( 2,6,29 ))
   struct net_device_ops * pNetDevOps;
#endif

   status = usbnet_probe( pIntf, pVIDPIDs );
   if(status < 0 )
   {
      DBG( "usbnet_probe failed %d\n", status );
      return status;
   }

#if (LINUX_VERSION_CODE > KERNEL_VERSION( 2,6,23 ))
   pDev = usb_get_intfdata( pIntf );
#else
   pDev = (struct usbnet *)pIntf->dev.platform_data;
#endif

   if (pDev == NULL || pDev->net == NULL)
   {
      DBG( "failed to get netdevice\n" );
      return -ENXIO;
   }

   pQCDev = kmalloc( sizeof( sQCUSBNet ), GFP_KERNEL );
   if (pQCDev == NULL)
   {
      DBG( "falied to allocate device buffers" );
      return -ENOMEM;
   }
   
   pDev->data[0] = (unsigned long)pQCDev;
   
   pQCDev->mpNetDev = pDev;

   // Overload PM related network functions
#if (LINUX_VERSION_CODE < KERNEL_VERSION( 2,6,29 ))
   pQCDev->mpUSBNetOpen = pDev->net->open;
   pDev->net->open = QCUSBNetOpen;
   pQCDev->mpUSBNetStop = pDev->net->stop;
   pDev->net->stop = QCUSBNetStop;
   pDev->net->hard_start_xmit = QCUSBNetStartXmit;
   pDev->net->tx_timeout = QCUSBNetTXTimeout;
#else
   pNetDevOps = kmalloc( sizeof( struct net_device_ops ), GFP_KERNEL );
   if (pNetDevOps == NULL)
   {
      DBG( "falied to allocate net device ops" );
      return -ENOMEM;
   }
   memcpy( pNetDevOps, pDev->net->netdev_ops, sizeof( struct net_device_ops ) );
   
   pQCDev->mpUSBNetOpen = pNetDevOps->ndo_open;
   pNetDevOps->ndo_open = QCUSBNetOpen;
   pQCDev->mpUSBNetStop = pNetDevOps->ndo_stop;
   pNetDevOps->ndo_stop = QCUSBNetStop;
   pNetDevOps->ndo_start_xmit = QCUSBNetStartXmit;
   pNetDevOps->ndo_tx_timeout = QCUSBNetTXTimeout;

   pDev->net->netdev_ops = pNetDevOps;
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION( 2,6,31 ))
   memset( &(pQCDev->mpNetDev->stats), 0, sizeof( struct net_device_stats ) );
#else
   memset( &(pQCDev->mpNetDev->net->stats), 0, sizeof( struct net_device_stats ) );
#endif

   pQCDev->mpIntf = pIntf;
   memset( &(pQCDev->mMEID), '0', 14 );
   
   DBG( "Mac Address:\n" );
   PrintHex( &pQCDev->mpNetDev->net->dev_addr[0], 6 );

   pQCDev->mbQMIValid = false;
   memset( &pQCDev->mQMIDev, 0, sizeof( sQMIDev ) );

   pQCDev->mQMIDev.mpDevClass = gpClass;
   
   sema_init( &pQCDev->mAutoPM.mThreadDoWork, 0 );
   spin_lock_init( &pQCDev->mQMIDev.mClientMemLock );

   // Default to device down
   pQCDev->mDownReason = 0;
   QSetDownReason( pQCDev, NO_NDIS_CONNECTION );
   QSetDownReason( pQCDev, NET_IFACE_STOPPED );

   // Register QMI
   status = RegisterQMIDevice( pQCDev );
   if (status != 0)
   {
      // Clean up
      DeregisterQMIDevice( pQCDev );
      return status;
   }
   
   // Success
   return status;
}

EXPORT_SYMBOL_GPL( QCUSBNetProbe );

static struct usb_driver QCUSBNet =
{
   .name       = "QCUSBNet2k",
   .id_table   = QCVIDPIDTable,
   .probe      = QCUSBNetProbe,
   .disconnect = usbnet_disconnect,
   .suspend    = QCSuspend,
   .resume     = QCResume,
   .supports_autosuspend = true,
};

/*===========================================================================
METHOD:
   QCUSBNetModInit (Public Method)

DESCRIPTION:
   Initialize module
   Create device class
   Register out usb_driver struct

RETURN VALUE:
   int - 0 for success
         Negative errno for error
===========================================================================*/
static int __init QCUSBNetModInit( void )
{
   gpClass = class_create( THIS_MODULE, "QCQMI" );
   if (IS_ERR( gpClass ) == true)
   {
      DBG( "error at class_create %ld\n",
           PTR_ERR( gpClass ) );
      return -ENOMEM;
   }

   // This will be shown whenever driver is loaded
   printk( KERN_INFO "%s: %s\n", DRIVER_DESC, DRIVER_VERSION );

   return usb_register( &QCUSBNet );
}
module_init( QCUSBNetModInit );

/*===========================================================================
METHOD:
   QCUSBNetModExit (Public Method)

DESCRIPTION:
   Deregister module
   Destroy device class

RETURN VALUE:
   void
===========================================================================*/
static void __exit QCUSBNetModExit( void )
{
   usb_deregister( &QCUSBNet );

   class_destroy( gpClass );
}
module_exit( QCUSBNetModExit );

#ifdef bool
#undef bool
#endif

MODULE_VERSION( DRIVER_VERSION );
MODULE_DESCRIPTION( DRIVER_DESC );
MODULE_LICENSE( "GPL v2" );

module_param( debug, bool, S_IRUGO | S_IWUSR );
MODULE_PARM_DESC( debug, "Debuging enabled or not" );

