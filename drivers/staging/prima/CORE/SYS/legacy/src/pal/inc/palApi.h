/*
 * Copyright (c) 2012-2013 The Linux Foundation. All rights reserved.
 *
 * Previously licensed under the ISC license by Qualcomm Atheros, Inc.
 *
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * This file was originally distributed by Qualcomm Atheros, Inc.
 * under proprietary terms before Copyright ownership was assigned
 * to the Linux Foundation.
 */

/** ------------------------------------------------------------------------- *
    ------------------------------------------------------------------------- *


    \file palApi.h

    \brief Exports and types for the Platform Abstraction Layer interfaces.

    $Id$
    This file contains all the interfaces for thge Platform Abstration Layer
    functions.  It is intended to be included in all modules that are using
    the PAL interfaces.

   ========================================================================== */
#ifndef PALAPI_H__
#define PALAPI_H__

#include "halTypes.h"

/**
    \mainpage Platform Abstraction Layer (PAL)

    \section intro Introduction

    palApi is the Platform Abstration Layer.

    This is the latest attempt to abstract the entire Platform, including the
    hardware, chip, OS and Bus into a generic API.  We are doing this to give
    the MAC the ability to call
    generic APIs that will allow the MAC to function in an abstract manner
    with any Airgo chipset, on any supported OS (Windows and Linux for now)
    across any system bus interface (PCI, PCIe, Cardbus, USB, etc.).

    \todo
    - palReadRegister:  register read
        -# add an Open/Close abstraction to accomodate the PAL before the entire MAC is loaded.
        -# Review with Linux folks to see this basic scructure works for them.
        -# Figure out how to organize the directory structure
    - palMemory: memory read/write
    - include async versions of read/write register
    - palTx: an abstraction for transmit frames that manages the Td and Tm rings
    - palRx: an abstracion for receiving frames from a chip across any of the supported buses
    - palInterrupt: abstract the interrupts into the HAL


    \section impl_notes Implementation Notes

    \subsection subsection_codeStructure Code strucure

 */


/** ---------------------------------------------------------------------------

    \fn palReadRegister

    \brief chip and bus agnostic funtion to read a register value

    \param hHdd - HDD context handle

    \param regAddress - address (offset) of the register to be read from the start
    of register space.

    \param pRegValue - pointer to the memory where the register contents are written

    \return eHalStatus - status of the register read.  Note that this function
    can fail.  In particular, when the card is removed, this function will return
    a failure.

  -------------------------------------------------------------------------------*/
eHalStatus palReadRegister( tHddHandle hHdd, tANI_U32 regAddress, tANI_U32 *pRegValue );


/** ---------------------------------------------------------------------------

    \fn palWriteRegister

    \brief chip and bus agnostic funtion to write a register value

    \param hHdd - HDD context handle

    \param regAddress - address (offset) of the register to be read from the start
    of register space.

    \param pRegValue - pointer to the value being written into the register

    \return eHalStatus - status of the register read.  Note that this function
    can fail.  In particular, when the card is removed, this function will return
    a failure.

  -------------------------------------------------------------------------------*/
eHalStatus palWriteRegister( tHddHandle hHdd, tANI_U32 regAddress, tANI_U32 regValue );

/** ---------------------------------------------------------------------------

    \fn palAsyncWriteRegister

    \brief chip and bus agnostic async funtion to write a register value

    \param hHdd - HDD context handle

    \param regAddress - address (offset) of the register to be written from the start
    of register space.

    \param regValue - value being written into the register

    \return eHalStatus - status of the register write.  Note that this function
    can fail.  In particular, when the card is removed, this function will return
    a failure.

  -------------------------------------------------------------------------------*/

eHalStatus palAsyncWriteRegister( tHddHandle hHdd, tANI_U32 regAddress, tANI_U32 regValue );


/** ---------------------------------------------------------------------------

    \fn palReadDeviceMemory

    \brief chip and bus agnostic funtion to read memory from the chip

    \param hHdd - HDD context handle

    \param memOffset - address (offset) of the memory from the top of the
    memory map (as exposed to the host) where the memory will be read from.

    \param pBuffer - pointer to a buffer where the memory will be placed in host
    memory space after retreived from the chip.

    \param numBytes - the number of bytes to be read.

    \return eHalStatus - status of the memory read.  Note that this function
    can fail.  In particular, when the card is removed, this function will return
    a failure.

  -------------------------------------------------------------------------------*/
eHalStatus palReadDeviceMemory( tHddHandle hHdd, tANI_U32 memOffset, tANI_U8 *pBuffer, tANI_U32 numBytes );

/** ---------------------------------------------------------------------------

    \fn palWriteDeviceMemory

    \brief chip and bus agnostic funtion to write memory to the chip

    \param hHdd - HDD context handle

    \param memOffset - address (offset) of the memory from the top of the on-chip
    memory that will be written.

    \param pBuffer - pointer to a buffer that has the source data that will be
    written to the chip.

    \param numBytes - the number of bytes to be written.

    \return eHalStatus - status of the memory read.  Note that this function
    can fail.  In particular, when the card is removed, this function will return
    a failure.

  -------------------------------------------------------------------------------*/
eHalStatus palWriteDeviceMemory( tHddHandle hHdd, tANI_U32 memOffset, tANI_U8 *pBuffer, tANI_U32 numBytes );


/** ---------------------------------------------------------------------------

    \fn palAllocateMemory

    \brief OS agnostic funtion to allocate host memory.

    \note  Host memory that needs to be shared between the host and the
    device needs to be allocated with the palAllocateSharedMemory()
    and free'd with palFreeSharedMemory() functions.

    \param hHdd - HDD context handle

    \param ppMemory - pointer to a void pointer where the address of the
    memory allocated will be placed upon return from this function.

    \param numBytes - the number of bytes to allocate.

    \return eHalStatus - status of the register read.  Note that this function
    can fail.  In the case of a failure, a non-successful return code will be
    returned and no memory will be allocated (the *ppMemory will be NULL so don't
    try to use it unless the status returns success).

  -------------------------------------------------------------------------------*/
#ifndef FEATURE_WLAN_PAL_MEM_DISABLE

#ifdef MEMORY_DEBUG
#define palAllocateMemory(hHdd, ppMemory, numBytes) palAllocateMemory_debug(hHdd, ppMemory, numBytes, __FILE__, __LINE__)
eHalStatus palAllocateMemory_debug( tHddHandle hHdd, void **ppMemory, tANI_U32 numBytes, char* fileName, tANI_U32 lineNum );
#else
eHalStatus palAllocateMemory( tHddHandle hHdd, void **ppMemory, tANI_U32 numBytes );
#endif


/** ---------------------------------------------------------------------------

    \fn palFreeMemory

    \brief OS agnostic funtion to free host memory that was allocated with
    palAllcoateMemory() calls.

    \note  Host memory that needs to be shared between the host and the
    device needs to be allocated with the palAllocateSharedMemory()
    and free'd with palFreeSharedMemory() functions.

    \param hHdd - HDD context handle

    \param pMemory - pointer to memory that will be free'd.

    \return eHalStatus - status of the register read.  Note that this function
    can fail.  In the case of a failure, a non-successful return code will be
    returned and no memory will be allocated (the *ppMemory will be NULL so don't
    try to use it unless the status returns success).

  -------------------------------------------------------------------------------*/
eHalStatus palFreeMemory( tHddHandle hHdd, void *pMemory );



/** ---------------------------------------------------------------------------

    \fn palFillMemory

    \brief OS agnostic funtion to fill host memory with a specified byte value

    \param hHdd - HDD context handle

    \param pMemory - pointer to memory that will be filled.

    \param numBytes - the number of bytes to be filled.

    \param fillValue - the byte to be written to fill the memory with.

    \return eHalStatus - status of the register read.  Note that this function
    can fail.  In the case of a failure, a non-successful return code will be
    returned and no memory will be allocated (the *ppMemory will be NULL so don't
    try to use it unless the status returns success).

  -------------------------------------------------------------------------------*/
eHalStatus palFillMemory( tHddHandle hHdd, void *pMemory, tANI_U32 numBytes, tANI_BYTE fillValue );

/** ---------------------------------------------------------------------------

    \fn palCopyMemory

    \brief OS agnostic funtion to copy host memory from one location to another

    \param hHdd - HDD context handle

    \param pSrc - pointer to source memory location (to copy from)

    \param pSrc - pointer to destination memory location (to copy to)

    \param numBytes - the number of bytes to be be copied.

    \return eHalStatus - status of the memory copy

  -------------------------------------------------------------------------------*/
eHalStatus palCopyMemory( tHddHandle hHdd, void *pDst, const void *pSrc, tANI_U32 numBytes );

/** ---------------------------------------------------------------------------

    \fn palFillMemory

    \brief OS agnostic funtion to fill host memory with a specified byte value

    \param hHdd - HDD context handle

    \param pMemory - pointer to memory that will be filled.

    \param numBytes - the number of bytes to be filled.

    \param fillValue - the byte to be written to fill the memory with.

    \return eHalStatus - status of the register read.  Note that this function
    can fail.  In the case of a failure, a non-successful return code will be
    returned and no memory will be allocated (the *ppMemory will be NULL so don't
    try to use it unless the status returns success).

  -------------------------------------------------------------------------------*/
ANI_INLINE_FUNCTION
eHalStatus palZeroMemory( tHddHandle hHdd, void *pMemory, tANI_U32 numBytes )
{
    return( palFillMemory( hHdd, pMemory, numBytes, 0 ) );
}


/** ---------------------------------------------------------------------------

    \fn palEqualMemory

    \brief OS agnostic funtion to compare two pieces of memory, similar to
    memcmp function in standard C.

    \param hHdd - HDD context handle

    \param pMemory1 - pointer to one location in memory to compare.

    \param pMemory2 - pointer to second location in memory to compare.

    \param numBytes - the number of bytes to compare.

    \return tANI_BOOLEAN - returns a boolean value that tells if the memory
    locations are equal or now equal.

  -------------------------------------------------------------------------------*/
tANI_BOOLEAN palEqualMemory( tHddHandle hHdd, void *pMemory1, void *pMemory2, tANI_U32 numBytes );
#endif
/** ---------------------------------------------------------------------------

    \fn palFillDeviceMemory

    \brief OS agnostic funtion to fill device memory with a specified
    32bit value

    \param hHdd - HDD context handle

    \param memOffset - offset of the memory on the device to fill.

    \param numBytes - the number of bytes to be filled.

    \param fillValue - the byte pattern to fill into memory on the device

    \return eHalStatus - status of the register read.  Note that this function
    can fail.

    eHAL_STATUS_DEVICE_MEMORY_LENGTH_ERROR - length of the device memory is not
    a multiple of 4 bytes.

    eHAL_STATUS_DEVICE_MEMORY_MISALIGNED - memory address is not aligned on a
    4 byte boundary.

    \note return failure if the memOffset is not 32bit aligned and not a
    multiple of 4 bytes (the device does not support anything else).

  -------------------------------------------------------------------------------*/
eHalStatus palFillDeviceMemory( tHddHandle hHdd, tANI_U32 memOffset, tANI_U32 numBytes, tANI_BYTE fillValue );


/** ---------------------------------------------------------------------------

    \fn palZeroDeviceMemory

    \brief OS agnostic funtion to fill device memory with a specified byte value

    \param hHdd - HDD context handle

    \param memOffset - offset of the memory on the device to fill.

    \param numBytes - the number of bytes to be filled.

    \param fillValue - the 32bit pattern to fill the memory with.

    \return eHalStatus - status of the register read.  Note that this function
    can fail.

    eHAL_STATUS_DEVICE_MEMORY_LENGTH_ERROR - length of the device memory is not
    a multiple of 4 bytes.

    eHAL_STATUS_DEVICE_MEMORY_MISALIGNED - memory address is not aligned on a
    4 byte boundary.

    \note return failure if the memOffset is not 32bit aligned and not a
    multiple of 4 bytes (the device does not support anything else).

  -------------------------------------------------------------------------------*/
ANI_INLINE_FUNCTION
eHalStatus palZeroDeviceMemory( tHddHandle hHdd, tANI_U32 memOffset, tANI_U32 numBytes )
{
    return( palFillDeviceMemory( hHdd, memOffset, numBytes, 0 ) );
}

/*----------------------------------------------------------------------------------

    Allocate a packet for sending through the Tx APIs.

    \param hHdd - HDD context handle

    \param frmType - Frame type

    \param size

    \param data -

    \param ppPacket  -

    \return eHalStatus -
----------------------------------------------------------------------------------*/
eHalStatus palPktAlloc(tHddHandle hHdd, eFrameType frmType, tANI_U16 size, void **data, void **ppPacket) ;


// This should return Ssome sort of status.....
void palPktFree( tHddHandle hHdd, eFrameType frmType, void* buf, void *pPacket);



//PAL lock functions
//pHandle -- pointer to a caller allocated tPalSpinLockHandle object
eHalStatus palSpinLockAlloc( tHddHandle hHdd, tPalSpinLockHandle *pHandle );
//hSpinLock -- a handle returned by palSpinLockAlloc
eHalStatus palSpinLockFree( tHddHandle hHdd, tPalSpinLockHandle hSpinLock );
//hSpinLock -- a handle returned by palSpinLockAlloc
eHalStatus palSpinLockTake( tHddHandle hHdd, tPalSpinLockHandle hSpinLock );
//hSpinLock -- a handle returned by palSpinLockAlloc
eHalStatus palSpinLockGive( tHddHandle hHdd, tPalSpinLockHandle hSpinLock );
//PAL lock functions end


//This function send a message to MAC,
//pMsgBuf is a buffer allocated by caller. The actual structure varies base on message type
//The beginning of the buffer can always map to tSirMbMsg
//This function must take care of padding if it is required for the OS
eHalStatus palSendMBMessage(tHddHandle hHdd, void *pBuf);

extern void palGetUnicastStats(tHddHandle hHdd, tANI_U32 *tx, tANI_U32 *rx);


/*----------------------------------------------------------------------------------
    this function is to return a tick count (one tick = ~10ms). It is used to calculate
    time difference.

    \param hHdd - HDD context handle

    \return tick count.
----------------------------------------------------------------------------------*/
tANI_U32 palGetTickCount(tHddHandle hHdd);

/** ---------------------------------------------------------------------------

    \fn palReadRegMemory

    \brief chip and bus agnostic function to read memory from the PHY register space as memory
    i.e. to read more than 4 bytes from the contiguous register space

    \param hHdd - HDD context handle

    \param memOffset - address (offset) of the memory from the top of the
    memory map (as exposed to the host) where the memory will be read from.

    \param pBuffer - pointer to a buffer where the memory will be placed in host
    memory space after retreived from the chip.

    \param numBytes - the number of bytes to be read.

    \return eHalStatus - status of the memory read.  Note that this function
    can fail.  In particular, when the card is removed, this function will return
    a failure.

  -------------------------------------------------------------------------------*/
eHalStatus palReadRegMemory( tHddHandle hHdd, tANI_U32 memOffset, tANI_U8 *pBuffer, tANI_U32 numBytes );

/** ---------------------------------------------------------------------------

    \fn palAsyncWriteRegMemory

    \brief chip and bus agnostic function to write memory to the PHY register space as memory
    i.e. to write more than 4 bytes from the contiguous register space. In USB interface, this
    API does the write asynchronously.

    \param hHdd - HDD context handle

    \param memOffset - address (offset) of the memory from the top of the on-chip
    memory that will be written.

    \param pBuffer - pointer to a buffer that has the source data that will be
    written to the chip.

    \param numBytes - the number of bytes to be written.

    \return eHalStatus - status of the memory read.  Note that this function
    can fail.  In particular, when the card is removed, this function will return
    a failure.

  -------------------------------------------------------------------------------*/
eHalStatus palAsyncWriteRegMemory( tHddHandle hHdd, tANI_U32 memOffset, tANI_U8 *pBuffer, tANI_U32 numBytes );

/** ---------------------------------------------------------------------------

    \fn palWriteRegMemory
    \brief chip and bus agnostic function to write memory to the PHY register space as memory
    i.e. to write more than 4 bytes from the contiguous register space. The difference from the
    above routine is, in USB interface, this routine performs the write synchronously where as
    the above routine performs it asynchronously.

    \param hHdd - HDD context handle

    \param memOffset - address (offset) of the memory from the top of the on-chip
    memory that will be written.

    \param pBuffer - pointer to a buffer that has the source data that will be
    written to the chip.

    \param numBytes - the number of bytes to be written.

    \return eHalStatus - status of the memory read.  Note that this function
    can fail.  In particular, when the card is removed, this function will return
    a failure.

  -------------------------------------------------------------------------------*/
eHalStatus palWriteRegMemory( tHddHandle hHdd, tANI_U32 memOffset, tANI_U8 *pBuffer, tANI_U32 numBytes );


/** ---------------------------------------------------------------------------

    \fn palWaitRegVal

    \brief is a blocking function which reads the register and waits for the given number of iterations
    until the read value matches the waitRegVal. The delay between is perIterWaitInNanoSec(in nanoseconds)

    \param hHdd - HDD context handle

    \param reg - address of the register to be read

    \param mask - mask to be applied for the read value

    \param waitRegVal - expected value from the register after applying the mask.

    \param perIterWaitInNanoSec - delay between the two iterations in nanoseconds

    \param numIter - max number of reads before the timeout

    \param pReadRegVal - the value read from the register

    \return eHalStatus - status of the memory read.  Note that this function
    can fail.  In particular, when the card is removed, this function will return
    a failure.

  -------------------------------------------------------------------------------*/
eHalStatus palWaitRegVal( tHddHandle hHdd, tANI_U32 reg, tANI_U32 mask,
                             tANI_U32 waitRegVal, tANI_U32 perIterWaitInNanoSec,
                             tANI_U32 numIter, tANI_U32 *pReadRegVal );

/** ---------------------------------------------------------------------------

    \fn palReadModifyWriteReg

    \brief chip and bus agnostic function to read a PHY register apply the given masks(AND and OR masks)
    and writes back the new value to the register

    \param hHdd - HDD context handle

    \param reg - address of the register to be modified.

    \param andMask - The value read will be ANDed with this mask

    \parma orMask - The value after applying the andMask will be ORed with this value

    \return eHalStatus - status of the memory read.  Note that this function
    can fail.  In particular, when the card is removed, this function will return
    a failure.

  -------------------------------------------------------------------------------*/
eHalStatus palReadModifyWriteReg( tHddHandle hHdd, tANI_U32 reg, tANI_U32 andMask, tANI_U32 orMask );

//PAL semaphore functions
eHalStatus palSemaphoreAlloc( tHddHandle hHdd, tPalSemaphoreHandle *pHandle, tANI_S32 count );
eHalStatus palSemaphoreFree( tHddHandle hHdd, tPalSemaphoreHandle hSemaphore );
eHalStatus palSemaphoreTake( tHddHandle hHdd, tPalSemaphoreHandle hSemaphore );
eHalStatus palSemaphoreGive( tHddHandle hHdd, tPalSemaphoreHandle hSemaphore );
eHalStatus palMutexAlloc( tHddHandle hHdd, tPalSemaphoreHandle *pHandle) ;
eHalStatus palMutexAllocLocked( tHddHandle hHdd, tPalSemaphoreHandle *pHandle) ;

//PAL irq/softirq
eAniBoolean pal_in_interrupt(void) ;
void pal_local_bh_disable(void) ;
void pal_local_bh_enable(void) ;

//PAL byte swap
tANI_U32 pal_be32_to_cpu(tANI_U32 x) ;
tANI_U32 pal_cpu_to_be32(tANI_U32 x) ;
tANI_U16 pal_be16_to_cpu(tANI_U16 x) ;
tANI_U16 pal_cpu_to_be16(tANI_U16 x) ;


#if defined( ANI_LITTLE_BYTE_ENDIAN )

// Need to eliminate these and use the ani_cpu_to_le, etc. macros....
ANI_INLINE_FUNCTION unsigned long i_htonl( unsigned long ul )
{
  return( ( ( ul & 0x000000ff ) << 24 ) |
          ( ( ul & 0x0000ff00 ) <<  8 ) |
          ( ( ul & 0x00ff0000 ) >>  8 ) |
          ( ( ul & 0xff000000 ) >> 24 )   );
}

ANI_INLINE_FUNCTION unsigned short i_htons( unsigned short us )
{
  return( ( ( us >> 8 ) & 0x00ff ) + ( ( us << 8 ) & 0xff00 ) );
}

ANI_INLINE_FUNCTION unsigned short i_ntohs( unsigned short us )
{
  return( i_htons( us ) );
}

ANI_INLINE_FUNCTION unsigned long i_ntohl( unsigned long ul )
{
  return( i_htonl( ul ) );
}

#endif //#if defined( ANI_LITTLE_BYTE_ENDIAN )


/** ---------------------------------------------------------------------------

    \fn pal_set_U32

    \brief Assign 32-bit unsigned value to a byte array base on CPU's endianness.

    \note Caller must validate the byte array has enough space to hold the vlaue

    \param ptr - Starting address of a byte array

    \param value - The value to assign to the byte array

    \return - The address to the byte after the assignment. This may or may not
    be valid. Caller to verify.

  -------------------------------------------------------------------------------*/
ANI_INLINE_FUNCTION tANI_U8 * pal_set_U32(tANI_U8 *ptr, tANI_U32 value)
{
#if defined( ANI_BIG_BYTE_ENDIAN )
     *(ptr) = ( tANI_U8 )( value >> 24 );
     *(ptr + 1) = ( tANI_U8 )( value >> 16 );
     *(ptr + 2) = ( tANI_U8 )( value >> 8 );
     *(ptr + 3) = ( tANI_U8 )( value );
#else
    *(ptr + 3) = ( tANI_U8 )( value >> 24 );
    *(ptr + 2) = ( tANI_U8 )( value >> 16 );
    *(ptr + 1) = ( tANI_U8 )( value >> 8 );
    *(ptr) = ( tANI_U8 )( value );
#endif

    return (ptr + 4);
}


/** ---------------------------------------------------------------------------

    \fn pal_set_U16

    \brief Assign 16-bit unsigned value to a byte array base on CPU's endianness.

    \note Caller must validate the byte array has enough space to hold the vlaue

    \param ptr - Starting address of a byte array

    \param value - The value to assign to the byte array

    \return - The address to the byte after the assignment. This may or may not
    be valid. Caller to verify.

  -------------------------------------------------------------------------------*/
ANI_INLINE_FUNCTION tANI_U8 * pal_set_U16(tANI_U8 *ptr, tANI_U16 value)
{
#if defined( ANI_BIG_BYTE_ENDIAN )
     *(ptr) = ( tANI_U8 )( value >> 8 );
     *(ptr + 1) = ( tANI_U8 )( value );
#else
    *(ptr + 1) = ( tANI_U8 )( value >> 8 );
    *(ptr) = ( tANI_U8 )( value );
#endif

    return (ptr + 2);
}


/** ---------------------------------------------------------------------------

    \fn pal_get_U16

    \brief Retrieve a 16-bit unsigned value from a byte array base on CPU's endianness.

    \note Caller must validate the byte array has enough space to hold the vlaue

    \param ptr - Starting address of a byte array

    \param pValue - Pointer to a caller allocated buffer for 16 bit value. Value is to assign
    to this location.

    \return - The address to the byte after the assignment. This may or may not
    be valid. Caller to verify.

  -------------------------------------------------------------------------------*/
ANI_INLINE_FUNCTION tANI_U8 * pal_get_U16(tANI_U8 *ptr, tANI_U16 *pValue)
{
#if defined( ANI_BIG_BYTE_ENDIAN )
    *pValue = (((tANI_U16) (*ptr << 8)) |
            ((tANI_U16) (*(ptr+1))));
#else
    *pValue = (((tANI_U16) (*(ptr+1) << 8)) |
            ((tANI_U16) (*ptr)));
#endif

    return (ptr + 2);
}


/** ---------------------------------------------------------------------------

    \fn pal_get_U32

    \brief Retrieve a 32-bit unsigned value from a byte array base on CPU's endianness.

    \note Caller must validate the byte array has enough space to hold the vlaue

    \param ptr - Starting address of a byte array

    \param pValue - Pointer to a caller allocated buffer for 32 bit value. Value is to assign
    to this location.

    \return - The address to the byte after the assignment. This may or may not
    be valid. Caller to verify.

  -------------------------------------------------------------------------------*/
ANI_INLINE_FUNCTION tANI_U8 * pal_get_U32(tANI_U8 *ptr, tANI_U32 *pValue)
{
#if defined( ANI_BIG_BYTE_ENDIAN )
    *pValue = ( (tANI_U32)(*(ptr) << 24) |
             (tANI_U32)(*(ptr+1) << 16) |
             (tANI_U32)(*(ptr+2) << 8) |
             (tANI_U32)(*(ptr+3)) );
#else
    *pValue = ( (tANI_U32)(*(ptr+3) << 24) |
             (tANI_U32)(*(ptr+2) << 16) |
             (tANI_U32)(*(ptr+1) << 8) |
             (tANI_U32)(*(ptr)) );
#endif

    return (ptr + 4);
}


#endif
