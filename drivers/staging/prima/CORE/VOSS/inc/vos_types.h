/*
 * Copyright (c) 2014 The Linux Foundation. All rights reserved.
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

#if !defined( __VOS_TYPES_H )
#define __VOS_TYPES_H

/**=========================================================================
  \file  vos_Types.h

  \brief virtual Operating System Servies (vOS)

   Basic type definitions
  ========================================================================*/

/* $Header$ */

/*--------------------------------------------------------------------------
  Include Files
  ------------------------------------------------------------------------*/
#include "i_vos_types.h"
#include <string.h>

/*-------------------------------------------------------------------------- 
  Preprocessor definitions and constants
  ------------------------------------------------------------------------*/
// macro to get maximum of two values.
#define VOS_MAX( _x, _y ) ( ( (_x) > (_y) ) ? (_x) : (_y) )  

// macro to get minimum of two values
#define VOS_MIN( _x, _y ) ( ( (_x) < (_y) ) ? (_x) : (_y)  )  

// macro to get the ceiling of an integer division operation...
#define VOS_CEIL_DIV( _a, _b ) (( 0 != (_a) % (_b) ) ? ( (_a) / (_b) + 1 ) : ( (_a) / (_b) ))

// macro to return the floor of an integer division operation
#define VOS_FLOOR_DIV( _a, _b ) ( ( (_a) - ( (_a) % (_b) ) ) / (_b) )

#define VOS_SWAP_U16(_x) \
   ( ( ( (_x) << 8 ) & 0xFF00 ) | ( ( (_x) >> 8 ) & 0x00FF ) )

#define VOS_SWAP_U32(_x) \
  (( ( ( (_x) << 24 ) & 0xFF000000 ) | ( ( (_x) >> 24 ) & 0x000000FF ) ) | \
   ( ( ( (_x) << 8 ) & 0x00FF0000 ) | ( ( (_x) >> 8 ) & 0x0000FF00 ) ))

// Endian operations for Big Endian and Small Endian modes
#ifdef ANI_LITTLE_BYTE_ENDIAN

#define vos_cpu_to_be32(_x) VOS_SWAP_U32(_x)
#define vos_be32_to_cpu(_x) VOS_SWAP_U32(_x)
#define vos_cpu_to_be16(_x) VOS_SWAP_U16(_x)
#define vos_be16_to_cpu(_x) VOS_SWAP_U16(_x)
#define vos_cpu_to_le32(_x) (_x)
#define vos_le32_to_cpu(_x) (_x)
#define vos_cpu_to_le16(_x) (_x)
#define vos_le16_to_cpu(_x) (_x)

#endif

#ifdef ANI_BIG_BYTE_ENDIAN

#define vos_cpu_to_be32(_x) (_x)
#define vos_be32_to_cpu(_x) (_x)
#define vos_cpu_to_be16(_x) (_x)
#define vos_be16_to_cpu(_x) (_x)
#define vos_cpu_to_le32(_x) VOS_SWAP_U32(_x)
#define vos_le32_to_cpu(_x) VOS_SWAP_U32(_x)
#define vos_cpu_to_le16(_x) VOS_SWAP_U16(_x)
#define vos_le16_to_cpu(_x) VOS_SWAP_U16(_x)

#endif


/*--------------------------------------------------------------------------
  Type declarations
  ------------------------------------------------------------------------*/

/// Module IDs.  These are generic IDs that identify the various modules
/// in the software system.
typedef enum
{
   VOS_MODULE_ID_BAP        = 0,
   VOS_MODULE_ID_TL         = 1,
   VOS_MODULE_ID_WDI        = 2,
   VOS_MODULE_ID_SVC        = 3,
   // 4 is unused for historical purposes
   VOS_MODULE_ID_RSV4       = 4,
   VOS_MODULE_ID_HDD        = 5,
   VOS_MODULE_ID_SME        = 6,
   VOS_MODULE_ID_PE         = 7,
   VOS_MODULE_ID_WDA        = 8,
   VOS_MODULE_ID_SYS        = 9,
   VOS_MODULE_ID_VOSS       = 10,
   VOS_MODULE_ID_SAP        = 11,
   VOS_MODULE_ID_HDD_SOFTAP = 12,
   VOS_MODULE_ID_PMC        = 13,
   VOS_MODULE_ID_HDD_DATA   = 14,
   VOS_MODULE_ID_HDD_SAP_DATA   = 15,

   // not a real module ID.  This is used to identify the maxiumum
   // number of VOS_MODULE_IDs and should always be at the END of
   // this enum.  If IDs are added, they need to go in front of this
   VOS_MODULE_ID_MAX

} VOS_MODULE_ID;


/// Concurrency role.  These are generic IDs that identify the various roles
/// in the software system.
typedef enum
{ /*ON linux maintain 1-1 corespondence with device_mode_t in hdd*/
    VOS_STA_MODE=0, 
    VOS_STA_SAP_MODE=1, //to support softAp  mode . This is misleading. It means AP MODE only. 
                        //The constant name has historical reason
    VOS_P2P_CLIENT_MODE,
    VOS_P2P_GO_MODE,
    VOS_MONITOR_MODE,
    VOS_FTM_MODE = 5,
    VOS_IBSS_MODE,
    VOS_P2P_DEVICE,
    VOS_MAX_NO_OF_MODE
} tVOS_CON_MODE;

//This is a bit pattern to be set for each mode
//bit 0 - sta mode
//bit 1 - ap mode
//bit 2 - p2p client mode
//bit 3 - p2p go mode
typedef enum
{
    VOS_STA=1, 
    VOS_SAP=2,
    VOS_STA_SAP=3, //to support sta, softAp  mode . This means STA+AP mode
    VOS_P2P_CLIENT=4,
    VOS_P2P_GO=8,
    VOS_MAX_CONCURRENCY_PERSONA=4
} tVOS_CONCURRENCY_MODE;
 
#if !defined( NULL )
#ifdef __cplusplus
#define NULL    0
#else
#define NULL    ((void *)0)
#endif
#endif

enum
{
   VOS_FALSE = 0, 
   VOS_TRUE  = ( !VOS_FALSE )
};

/// pointer to void types
typedef v_VOID_t *v_PVOID_t;

/// "Size" type... 
typedef v_UINT_t v_SIZE_t;

/// 'Time' type
typedef v_ULONG_t v_TIME_t;

// typedef for VOSS Context...
typedef v_VOID_t *v_CONTEXT_t;


/// MAC address data type and corresponding macros/functions to 
/// manipulate MAC addresses...
/// Macro defining the size of a MAC Address...
#define VOS_MAC_ADDR_SIZE ( 6 )

typedef struct
{
   /// the bytes that make up the macAddress.  
   v_BYTE_t bytes[ VOS_MAC_ADDR_SIZE ];
    
} v_MACADDR_t;


/// This macro is used to initialize a vOSS MacAddress to the 
/// broadcast MacAddress.  It is used like this...
/// v_MACADDR_t macAddress = VOS_MAC_ADDR_BROADCAST_INITIALIZER;
#define VOS_MAC_ADDR_BROADCAST_INITIALIZER { { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff } }

/// This macro is used to initialize a vOSS MacAddress to zero
/// It is used like this...
/// v_MACADDR_t macAddress = VOS_MAC_ADDR_ZERO_INITIALIZER;
#define VOS_MAC_ADDR_ZERO_INITIALIZER { { 0, 0, 0, 0, 0, 0 } }



/*----------------------------------------------------------------------------
  
  \brief vos_is_macaddr_equal() - compare two vOSS MacAddress

  This function returns a boolean that tells if a two vOSS MacAddress' 
  are equivalent.
  
  \param pMacAddr1 - pointer to one voss MacAddress to compare
  \param pMacAddr2 - pointer to the other voss MacAddress to compare
  
  \return  true - the MacAddress's are equal
           not true - the MacAddress's are not equal
  
  \sa
    
  --------------------------------------------------------------------------*/
VOS_INLINE_FN v_BOOL_t vos_is_macaddr_equal( v_MACADDR_t *pMacAddr1, 
                                             v_MACADDR_t *pMacAddr2 )
{
   return ( 0 == memcmp( pMacAddr1, pMacAddr2, VOS_MAC_ADDR_SIZE ) );
}



/*----------------------------------------------------------------------------
  
  \brief vos_is_macaddr_zero() - check for a MacAddress of all zeros. 

  This function returns a boolean that tells if a MacAddress is made up of
  all zeros.
  
  \param pMacAddr - pointer to the v_MACADDR_t to check.
  
  \return  true - the MacAddress is all Zeros
           not true - the MacAddress is not all Zeros.
    
  \sa
  
  --------------------------------------------------------------------------*/
VOS_INLINE_FN v_BOOL_t vos_is_macaddr_zero( v_MACADDR_t *pMacAddr )
{
   v_MACADDR_t zeroMacAddr = VOS_MAC_ADDR_ZERO_INITIALIZER;
   
   return( vos_is_macaddr_equal( pMacAddr, &zeroMacAddr ) );
}


/*----------------------------------------------------------------------------
  
  \brief vos_zero_macaddr() - zero out a MacAddress

  This function zeros out a vOSS MacAddress type.
  
  \param pMacAddr - pointer to the v_MACADDR_t to zero.
  
  \return  nothing
    
  \sa
  
  --------------------------------------------------------------------------*/
VOS_INLINE_FN v_VOID_t vos_zero_macaddr( v_MACADDR_t *pMacAddr )
{
   memset( pMacAddr, 0, VOS_MAC_ADDR_SIZE );
}


/*----------------------------------------------------------------------------
  
  \brief vos_is_macaddr_group() - check for a MacAddress is a 'group' address 

  This function returns a boolean that tells if a the input vOSS MacAddress
  is a "group" address.  Group addresses have the 'group address bit' turned
  on in the MacAddress.  Group addresses are made up of Broadcast and 
  Multicast addresses.
  
  \param pMacAddr1 - pointer to the voss MacAddress to check
  
  \return  true - the input MacAddress is a Group address
           not true - the input MacAddress is not a Group address
  
  \sa
    
  --------------------------------------------------------------------------*/
VOS_INLINE_FN v_BOOL_t vos_is_macaddr_group( v_MACADDR_t *pMacAddr )
{
    return( pMacAddr->bytes[ 0 ] & 0x01 );
}


/*----------------------------------------------------------------------------
  
  \brief vos_is_macaddr_broadcast() - check for a MacAddress is a broadcast address 

  This function returns a boolean that tells if a the input vOSS MacAddress
  is a "broadcast" address.
  
  \param pMacAddr - pointer to the voss MacAddress to check
  
  \return  true - the input MacAddress is a broadcast address
           not true - the input MacAddress is not a broadcast address
  
  \sa
    
  --------------------------------------------------------------------------*/
VOS_INLINE_FN v_BOOL_t vos_is_macaddr_broadcast( v_MACADDR_t *pMacAddr )
{
   v_MACADDR_t broadcastMacAddr = VOS_MAC_ADDR_BROADCAST_INITIALIZER;
   
   return( vos_is_macaddr_equal( pMacAddr, &broadcastMacAddr ) );
}

/*----------------------------------------------------------------------------
  
  \brief vos_is_macaddr_multicast() - check for a MacAddress is a multicast address 

  This function returns a boolean that tells if a the input vOSS MacAddress
  is a "Multicast" address.
  
  \param pMacAddr - pointer to the voss MacAddress to check
  
  \return  true - the input MacAddress is a Multicast address
           not true - the input MacAddress is not a Multicast address
  
  \sa
    
  --------------------------------------------------------------------------*/
VOS_INLINE_FN v_BOOL_t vos_is_macaddr_multicast( v_MACADDR_t *pMacAddr )
{
   return( vos_is_macaddr_group( pMacAddr ) &&
          !vos_is_macaddr_broadcast( pMacAddr ) );
}



/*----------------------------------------------------------------------------
  
  \brief vos_is_macaddr_directed() - check for a MacAddress is a directed address 

  This function returns a boolean that tells if a the input vOSS MacAddress
  is a "directed" address.
  
  \param pMacAddr - pointer to the voss MacAddress to check
  
  \return  true - the input MacAddress is a directed address
           not true - the input MacAddress is not a directed address
  
  \sa
    
  --------------------------------------------------------------------------*/
VOS_INLINE_FN v_BOOL_t vos_is_macaddr_directed( v_MACADDR_t *pMacAddr )
{
    return( !vos_is_macaddr_group( pMacAddr ) );
}

/*----------------------------------------------------------------------------
  
  \brief vos_copy_macaddr() - copy a vOSS MacAddress

  This function copies a vOSS MacAddress into another vOSS MacAddress.
  
  \param pDst - pointer to the voss MacAddress to copy TO (the destination)
  \param pSrc - pointer to the voss MacAddress to copy FROM (the source)
  
  \return  nothing
    
  \sa
    
  --------------------------------------------------------------------------*/
VOS_INLINE_FN v_VOID_t vos_copy_macaddr( v_MACADDR_t *pDst, v_MACADDR_t *pSrc )
{
    *pDst = *pSrc;
}    


/*----------------------------------------------------------------------------
  
  \brief vos_set_macaddr_broadcast() - set a vOSS MacAddress to the 'broadcast' 
                                       
  This function sets a vOSS MacAddress to the 'broadcast' MacAddress. Broadcast
  MacAddress contains all 0xFF bytes.
  
  \param pMacAddr - pointer to the voss MacAddress to set to broadcast
  
  \return  nothing
    
  \sa
    
  --------------------------------------------------------------------------*/
VOS_INLINE_FN v_VOID_t vos_set_macaddr_broadcast( v_MACADDR_t *pMacAddr )
{
   memset( pMacAddr, 0xff, VOS_MAC_ADDR_SIZE );
}

/*----------------------------------------------------------------------------
  
  \brief vos_atomic_set() - set a variable atomically

  \param pTarget - pointer to the variable to set.

  \param value - the value to set in the  variable.

  \return This function returns the value previously in the uintptr_t before
          the new value is set.

  \sa vos_atomic_increment_U32(), vos_atomic_decrement_U32()
  
  --------------------------------------------------------------------------*/                                                 
uintptr_t vos_atomic_set( uintptr_t *pTarget, uintptr_t value );


// TODO: the below function is a stub to perform atomic set on a BYTE
// Clearly the function below is not an atomic function
VOS_INLINE_FN v_U8_t vos_atomic_set_U8( v_U8_t *pVariable, v_U8_t value )
{
  if (pVariable == NULL)
  {
    return 0;
  }
  *pVariable = value;
  return value;
}

/*----------------------------------------------------------------------------
  
  \brief vos_atomic_increment_U32() - Increment a U32 variable atomically 
  
  \param pTarget - pointer to the v_U32_t to increment.
  
  \return This function returns the value of the variable after the 
          increment occurs.
    
  \sa vos_atomic_decrement_U32(), vos_atomic_set_U32()
  
  --------------------------------------------------------------------------*/                                                 
v_U32_t vos_atomic_increment_U32( v_U32_t *pTarget );


/*----------------------------------------------------------------------------
  
  \brief vos_atomic_decrement_U32() - Decrement a U32 variable atomically 
  
  \param pTarget - pointer to the v_U32_t to decrement.
  
  \return This function returns the value of the variable after the 
          decrement occurs.
    
  \sa vos_atomic_increment_U32(), vos_atomic_set_U32()
  
  --------------------------------------------------------------------------*/                                                 
v_U32_t vos_atomic_decrement_U32( v_U32_t *pTarget );

/*----------------------------------------------------------------------------
  
  \brief vos_atomic_increment_U32_by_value() - Increment a U32 variable atomically
  by a given value
  
  \param pTarget - pointer to the v_U32_t to decrement.
  \param value   - the value that needs to be added to target
  
  \return This function returns the value of the variable after the 
          decrement occurs.
    
  \sa vos_atomic_increment_U32(), vos_atomic_set_U32()
  
  --------------------------------------------------------------------------*/                                                 
v_U32_t vos_atomic_increment_U32_by_value( v_U32_t *pTarget, v_U32_t value  );

/*----------------------------------------------------------------------------
  
  \brief vos_atomic_decrement_U32_by_value() - Decrement a U32 variable atomically
  by a given value
  
  \param pTarget - pointer to the v_U32_t to decrement.
  \param value   - the value that needs to be substracted from target
  
  \return This function returns the value of the variable after the 
          decrement occurs.
    
  \sa vos_atomic_increment_U32(), vos_atomic_set_U32()
  
  --------------------------------------------------------------------------*/                                                 
v_U32_t vos_atomic_decrement_U32_by_value( v_U32_t *pTarget, v_U32_t value  );


v_U32_t vos_get_skip_ssid_check(void); 

v_U32_t vos_get_skip_11e_check(void); 



#endif // if !defined __VOSS_TYPES_H
