/*===========================================================================
FILE:
   QMIDevice.h

DESCRIPTION:
   Functions related to the QMI interface device
   
FUNCTIONS:
   Generic functions
      IsDeviceValid
      PrintHex
      QSetDownReason
      QClearDownReason
      QTestDownReason

   Driver level asynchronous read functions
      ReadCallback
      IntCallback
      StartRead
      KillRead

   Internal read/write functions
      ReadAsync
      UpSem
      ReadSync
      WriteSyncCallback
      WriteSync

   Internal memory management functions
      GetClientID
      ReleaseClientID
      FindClientMem
      AddToReadMemList
      PopFromReadMemList
      AddToNotifyList
      NotifyAndPopNotifyList
      AddToURBList
      PopFromURBList

   Userspace wrappers
      UserspaceOpen
      UserspaceIOCTL
      UserspaceClose
      UserspaceRead
      UserspaceWrite

   Initializer and destructor
      RegisterQMIDevice
      DeregisterQMIDevice

   Driver level client management
      QMIReady
      QMIWDSCallback
      SetupQMIWDSCallback
      QMIDMSGetMEID

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
// Pragmas
//---------------------------------------------------------------------------
#pragma once

//---------------------------------------------------------------------------
// Include Files
//---------------------------------------------------------------------------
#include "Structs.h"
#include "QMI.h"

/*=========================================================================*/
// Generic functions
/*=========================================================================*/

// Basic test to see if device memory is valid
bool IsDeviceValid( sQCUSBNet * pDev );

// Print Hex data, for debug purposes
void PrintHex(
   void *         pBuffer,
   u16            bufSize );

// Sets mDownReason and turns carrier off
void QSetDownReason(
   sQCUSBNet *    pDev,
   u8             reason );

// Clear mDownReason and may turn carrier on
void QClearDownReason(
   sQCUSBNet *    pDev,
   u8             reason );

// Tests mDownReason and returns whether reason is set
bool QTestDownReason(
   sQCUSBNet *    pDev,
   u8             reason );

/*=========================================================================*/
// Driver level asynchronous read functions
/*=========================================================================*/

// Read callback
//    Put the data in storage and notify anyone waiting for data
void ReadCallback( struct urb * pReadURB );

// Inturrupt callback
//    Data is available, start a read URB
void IntCallback( struct urb * pIntURB );

// Start continuous read "thread"
int StartRead( sQCUSBNet * pDev );

// Kill continuous read "thread"
void KillRead( sQCUSBNet * pDev );

/*=========================================================================*/
// Internal read/write functions
/*=========================================================================*/

// Start asynchronous read
//     Reading client's data store, not device
int ReadAsync(
   sQCUSBNet *    pDev,
   u16            clientID,
   u16            transactionID,
   void           (*pCallback)(sQCUSBNet *, u16, void *),
   void *         pData );

// Notification function for synchronous read
void UpSem( 
   sQCUSBNet *    pDev,
   u16            clientID,
   void *         pData );

// Start synchronous read
//     Reading client's data store, not device
int ReadSync(
   sQCUSBNet *    pDev,
   void **        ppOutBuffer,
   u16            clientID,
   u16            transactionID );

// Write callback
void WriteSyncCallback( struct urb * pWriteURB );

// Start synchronous write
int WriteSync(
   sQCUSBNet *    pDev,
   char *         pInWriteBuffer,
   int            size,
   u16            clientID );

/*=========================================================================*/
// Internal memory management functions
/*=========================================================================*/

// Create client and allocate memory
int GetClientID( 
   sQCUSBNet *      pDev,
   u8               serviceType );

// Release client and free memory
void ReleaseClientID(
   sQCUSBNet *      pDev,
   u16              clientID );

// Find this client's memory
sClientMemList * FindClientMem(
   sQCUSBNet *      pDev,
   u16              clientID );

// Add Data to this client's ReadMem list
bool AddToReadMemList( 
   sQCUSBNet *      pDev,
   u16              clientID,
   u16              transactionID,
   void *           pData,
   u16              dataSize );

// Remove data from this client's ReadMem list if it matches 
// the specified transaction ID.
bool PopFromReadMemList( 
   sQCUSBNet *      pDev,
   u16              clientID,
   u16              transactionID,
   void **          ppData,
   u16 *            pDataSize );

// Add Notify entry to this client's notify List
bool AddToNotifyList( 
   sQCUSBNet *      pDev,
   u16              clientID,
   u16              transactionID,
   void             (* pNotifyFunct)(sQCUSBNet *, u16, void *),
   void *           pData );

// Remove first Notify entry from this client's notify list 
//    and Run function
bool NotifyAndPopNotifyList( 
   sQCUSBNet *      pDev,
   u16              clientID,
   u16              transactionID );

// Add URB to this client's URB list
bool AddToURBList( 
   sQCUSBNet *      pDev,
   u16              clientID,
   struct urb *     pURB );

// Remove URB from this client's URB list
struct urb * PopFromURBList( 
   sQCUSBNet *      pDev,
   u16              clientID );

/*=========================================================================*/
// Userspace wrappers
/*=========================================================================*/

// Userspace open
int UserspaceOpen( 
   struct inode *   pInode, 
   struct file *    pFilp );

// Userspace ioctl
int UserspaceIOCTL( 
   struct inode *    pUnusedInode, 
   struct file *     pFilp,
   unsigned int      cmd, 
   unsigned long     arg );

// Userspace close
int UserspaceClose( 
   struct file *       pFilp,
   fl_owner_t          unusedFileTable );

// Userspace read (synchronous)
ssize_t UserspaceRead( 
   struct file *        pFilp,
   char __user *        pBuf, 
   size_t               size,
   loff_t *             pUnusedFpos );

// Userspace write (synchronous)
ssize_t UserspaceWrite(
   struct file *        pFilp, 
   const char __user *  pBuf, 
   size_t               size,
   loff_t *             pUnusedFpos );

/*=========================================================================*/
// Initializer and destructor
/*=========================================================================*/

// QMI Device initialization function
int RegisterQMIDevice( sQCUSBNet * pDev );

// QMI Device cleanup function
void DeregisterQMIDevice( sQCUSBNet * pDev );

/*=========================================================================*/
// Driver level client management
/*=========================================================================*/

// Check if QMI is ready for use
bool QMIReady(
   sQCUSBNet *    pDev,
   u16            timeout );

// QMI WDS callback function
void QMIWDSCallback(
   sQCUSBNet *    pDev,
   u16            clientID,
   void *         pData );

// Fire off reqests and start async read for QMI WDS callback
int SetupQMIWDSCallback( sQCUSBNet * pDev );

// Register client, send req and parse MEID response, release client
int QMIDMSGetMEID( sQCUSBNet * pDev );



