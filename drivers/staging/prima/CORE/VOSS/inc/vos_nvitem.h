/*
 * Copyright (c) 2011-2015 The Linux Foundation. All rights reserved.
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

#if !defined( __VOS_NVITEM_H )
#define __VOS_NVITEM_H

/**=========================================================================

  \file  vos_nvitem.h

  \brief virtual Operating System Services (vOSS): Non-Volatile storage API


  ========================================================================*/

/* $Header$ */

/*--------------------------------------------------------------------------
  Include Files
  ------------------------------------------------------------------------*/
#include "vos_types.h"
#include "vos_status.h"
#include "wlan_nv.h"
#include "wlan_nv2.h"

/* Maximum number of channels per country can be ignored */
#define MAX_CHANNELS_IGNORE 10
#define MAX_COUNTRY_IGNORE 5

#define TX_POWER_DEFAULT  30//in dbm

typedef struct sCsrIgnoreChannels
{
   tANI_U8 countryCode[NV_FIELD_COUNTRY_CODE_SIZE];
   tANI_U16 channelList[MAX_CHANNELS_IGNORE];
   tANI_U16 channelCount;
}tCsrIgnoreChannels;

extern tCsrIgnoreChannels countryIgnoreList[];

/*--------------------------------------------------------------------------
  Preprocessor definitions and constants
  ------------------------------------------------------------------------*/
// NV Items with their parameters are specified below
// Parameters include: enum label in VNV_TYPE (_name),
// element count (_cnt), element size (_size),
// enum of first element in NV module (_label)

/*
 * The VOS NV Item Table is compliant to AMSS NV service files of:
 * //source/qcom/qct/wconnect/wlan/private/csw/nv/ @ CL 431102
 *
 * VNV_REGULATORY_DOMAIN_TABLE - contains regulatory domain information;
 * the table is stored as $(REGDOMAIN_COUNT) NV items where each NV item
 * contains information for one domain; see definition of v_REGDOMAIN_t
 * for the regulatory domains
 *
 * VNV_FIELD_IMAGE - contains various elements such as MAC addresses
 *
 * VNV_RATE_TO_POWER_TABLE - table of power for each of 25 PHY rates
 *
 * VNV_DEFAULT_LOCATION  - default country code and regulatory domain
 *
 * VNV_MAXIMUM_TX_POWER
 *
 * VNV_RX_SENSITIVITY - table of sensitivity for each of %(MAC_RATE_COUNT) MAC
 * rates; see definition of v_MAC_RATE_t for the MAC rates
 *
 * VNV_NETWORK_TYPE - either A, B or G type
 *
 * VOV_QFUSE - 16 byte QFUSE data
 */
#define VNV_ITEM_TABLE \
ADD_VNV_ITEM( VNV_REGULARTORY_DOMAIN_TABLE, REGDOMAIN_COUNT, 144, \
      NV_WLAN_REGULATORY_DOMAIN_FCC_I ) \
ADD_VNV_ITEM( VNV_FIELD_IMAGE, 1, 52, NV_WLAN_FIELD_IMAGE_I ) \
ADD_VNV_ITEM( VNV_RATE_TO_POWER_TABLE, 2, 66, NV_WLAN_RATE_TO_POWER_LIST_I )\
ADD_VNV_ITEM( VNV_DEFAULT_LOCATION, 1, 4, NV_WLAN_DEFAULT_LOCATION_INFO_I ) \
ADD_VNV_ITEM( VNV_TPC_POWER_TABLE, 14, 128, NV_WLAN_TPC_POWER_TABLE_I ) \
ADD_VNV_ITEM( VNV_TPC_PDADC_OFFSETS, 14, 2, NV_WLAN_TPC_PDADC_OFFSETS_I ) \
ADD_VNV_ITEM( VNV_MAXIMUM_TX_POWER, 1, 1, NV_WLAN_MAX_TX_POWER_I ) \
ADD_VNV_ITEM( VNV_RX_SENSITIVITY, 1, MAC_RATE_COUNT, NV_WLAN_RX_SENSITIVITY_I)\
ADD_VNV_ITEM( VNV_NETWORK_TYPE, 1, 1, NV_WLAN_NETWORK_TYPE_I ) \
ADD_VNV_ITEM( VNV_CAL_MEMORY, 1, 3460, NV_WLAN_CAL_MEMORY_I ) \
ADD_VNV_ITEM( VNV_FW_CONFIG, 1, 32, NV_WLAN_FW_CONFIG_I ) \
ADD_VNV_ITEM( VNV_RSSI_CHANNEL_OFFSETS, 2, 56, NV_WLAN_RSSI_CHANNEL_OFFSETS_I ) \
ADD_VNV_ITEM( VNV_HW_CAL_VALUES, 1, 48, NV_WLAN_HW_CAL_VALUES_I ) \
ADD_VNV_ITEM( VNV_ANTENNA_PATH_LOSS, 14, 2, NV_WLAN_ANTENNA_PATH_LOSS_I ) \
ADD_VNV_ITEM( VNV_PACKET_TYPE_POWER_LIMITS, 42, 2, NV_WLAN_PACKET_TYPE_POWER_LIMITS_I ) \
ADD_VNV_ITEM( VNV_OFDM_CMD_PWR_OFFSET, 1, 2, NV_WLAN_OFDM_CMD_PWR_OFFSET_I ) \
ADD_VNV_ITEM( VNV_TX_BB_FILTER_MODE, 1, 4, NV_TX_BB_FILTER_MODE_I ) \
ADD_VNV_ITEM( VNV_FREQUENCY_FOR_1_3V_SUPPLY, 1, 4, NV_FREQUENCY_FOR_1_3V_SUPPLY_I ) \
ADD_VNV_ITEM( VNV_TABLE_VIRTUAL_RATE, 1, 4, VNV_TABLE_VIRTUAL_RATE_I ) \


#define VOS_COUNTRY_CODE_LEN  2
#define VOS_MAC_ADDRESS_LEN   6
#define VOS_MAC_ADDR_LAST_3_BYTES   3
#define VOS_MAC_ADDR_FIRST_3_BYTES   3
#define VOS_NV_FREQUENCY_FOR_1_3V_SUPPLY_3P2MH 0   //3.2 Mhz
#define VOS_NV_FREQUENCY_FOR_1_3V_SUPPLY_1P6MH 1   //1.6 Mhz


/*!
 * The path (from the root of the DPP_FOLDER_PATH\QCOM) to the file containing
 * the CLPC provisioning data. This is being temporarily put here. This should go
 * to esp_dpp.h where the WLAN_PROVISION_DATA is present.
 */
#define CLPC_PROVISION_DATA L"WLAN_CLPC.PROVISION"
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,14,0))
#define IEEE80211_CHAN_PASSIVE_SCAN IEEE80211_CHAN_NO_IR
#define IEEE80211_CHAN_NO_IBSS IEEE80211_CHAN_NO_IR
#endif
/*--------------------------------------------------------------------------
  Type declarations
  ------------------------------------------------------------------------*/
// enum of RX sensitivity table index
typedef enum
{
   // 11b
   MAC_RATE_11B_1_MBPS,
   MAC_RATE_11B_2_MBPS,
   MAC_RATE_11B_5_5_MBPS,
   MAC_RATE_11B_11_MBPS,

   // 11g
   MAC_RATE_11G_6_MBPS,
   MAC_RATE_11G_9_MBPS,
   MAC_RATE_11G_12_MBPS,
   MAC_RATE_11G_18_MBPS,
   MAC_RATE_11G_24_MBPS,
   MAC_RATE_11G_36_MBPS,
   MAC_RATE_11G_48_MBPS,
   MAC_RATE_11G_54_MBPS,

   // 11n
   MAC_RATE_11N_MCS_0,
   MAC_RATE_11N_MCS_1,
   MAC_RATE_11N_MCS_2,
   MAC_RATE_11N_MCS_3,
   MAC_RATE_11N_MCS_4,
   MAC_RATE_11N_MCS_5,
   MAC_RATE_11N_MCS_6,
   MAC_RATE_11N_MCS_7,

   MAC_RATE_COUNT

} v_MAC_RATE_t;

// enum of regulatory doamains in WLAN
typedef enum
{
   REGDOMAIN_FCC,
   REGDOMAIN_ETSI,
   REGDOMAIN_JAPAN,
   REGDOMAIN_WORLD,
   REGDOMAIN_N_AMER_EXC_FCC,
   REGDOMAIN_APAC,
   REGDOMAIN_KOREA,
   REGDOMAIN_HI_5GHZ,
   REGDOMAIN_NO_5GHZ,
   // add new regulatory domain here
   REGDOMAIN_COUNT
}
v_REGDOMAIN_t;

typedef enum
{
   COUNTRY_NV,
   COUNTRY_IE,
   COUNTRY_USER,
   COUNTRY_CELL_BASE,
   //add new sources here
   COUNTRY_QUERY,
   COUNTRY_MAX = COUNTRY_QUERY
}
v_CountryInfoSource_t;

//enum of NV version
typedef enum
{
   E_NV_V2,
   E_NV_V3,
   E_NV_INVALID
} eNvVersionType;

// enum of supported NV items in VOSS
typedef enum
{
#define ADD_VNV_ITEM(_name, _cnt, _size, _label) _name,
   VNV_ITEM_TABLE
#undef ADD_VNV_ITEM
   VNV_TYPE_COUNT
}
VNV_TYPE;

// country code type
typedef v_U8_t v_COUNTRYCODE_t[VOS_COUNTRY_CODE_LEN];

// MAC address type
typedef v_U8_t v_MAC_ADDRESS_t[VOS_MAC_ADDRESS_LEN];

/*-------------------------------------------------------------------------
  Function declarations and documenation
  ------------------------------------------------------------------------*/

const char * voss_DomainIdtoString(const v_U8_t domainIdCurrent);
/**------------------------------------------------------------------------

  \brief vos_nv_init() - initialize the NV module

  The \a vos_nv_init() initializes the NV module.  This read the binary
  file for country code and regulatory domain information.

  \return VOS_STATUS_SUCCESS - module is initialized successfully
          otherwise  - module is not initialized
  \sa

  -------------------------------------------------------------------------*/
VOS_STATUS vos_nv_init(void);

/**------------------------------------------------------------------------

  \brief vos_nv_getRegDomainFromCountryCode() - get the regulatory domain of
  a country given its country code

  The \a vos_nv_getRegDomainFromCountryCode() returns the regulatory domain of
  a country given its country code.  This is done from reading a cached
  copy of the binary file.

  \param pRegDomain  - pointer to regulatory domain

  \param countryCode - country code

  \param source      - source of country code

  \return VOS_STATUS_SUCCESS - regulatory domain is found for the given country
          VOS_STATUS_E_FAULT - invalid pointer error
          VOS_STATUS_E_EMPTY - country code table is empty
          VOS_STATUS_E_EXISTS - given country code does not exist in table

  \sa

  -------------------------------------------------------------------------*/
VOS_STATUS vos_nv_getRegDomainFromCountryCode( v_REGDOMAIN_t *pRegDomain,
      const v_COUNTRYCODE_t countryCode, v_CountryInfoSource_t source);

/**------------------------------------------------------------------------

  \brief vos_nv_getSupportedCountryCode() - get the list of supported
  country codes

  The \a vos_nv_getSupportedCountryCode() encodes the list of supported
  country codes with paddings in the provided buffer

  \param pBuffer     - pointer to buffer where supported country codes
                       and paddings are encoded; this may be set to NULL
                       if user wishes to query the required buffer size to
                       get the country code list

  \param pBufferSize - this is the provided buffer size on input;
                       this is the required or consumed buffer size on output

  \return VOS_STATUS_SUCCESS - country codes are successfully encoded
          VOS_STATUS_E_NOMEM - country codes are not encoded because either
                               the buffer is NULL or buffer size is
                               sufficient
  \sa

  -------------------------------------------------------------------------*/
VOS_STATUS vos_nv_getSupportedCountryCode( v_BYTE_t *pBuffer, v_SIZE_t *pBufferSize,
      v_SIZE_t paddingSize );

/**------------------------------------------------------------------------

  \brief vos_nv_setValidity() - set the validity of an NV item.

  The \a vos_nv_setValidity() validates and invalidates an NV item.  The
  validity information is stored in NV memory.
  One would get the VOS_STATUS_E_EXISTS error when reading an invalid item.
  An item becomes valid when one has written to it successfully.

  \param type        - NV item type

  \param itemIsValid - boolean value indicating the item's validity

  \return VOS_STATUS_SUCCESS - validity is set successfully
          VOS_STATUS_E_INVAL - one of the parameters is invalid
          VOS_STATUS_E_FAILURE - unknown error
  \sa

  -------------------------------------------------------------------------*/
VOS_STATUS vos_nv_setValidity( VNV_TYPE type, v_BOOL_t itemIsValid );

/**------------------------------------------------------------------------

  \brief vos_nv_getValidity() - get the validity of an NV item.

  The \a vos_nv_getValidity() indicates if an NV item is valid.  The
  validity information is stored in NV memory.
  One would get the VOS_STATUS_E_EXISTS error when reading an invalid item.
  An item becomes valid when one has written to it successfully.

  \param type        - NV item type

  \param pItemIsValid- pointer to the boolean value indicating the item's
                       validity

  \return VOS_STATUS_SUCCESS - validity is determined successfully
          VOS_STATUS_E_INVAL - one of the parameters is invalid
          VOS_STATUS_E_FAILURE - unknown error
  \sa

  -------------------------------------------------------------------------*/
VOS_STATUS vos_nv_getValidity( VNV_TYPE type, v_BOOL_t *pItemIsValid );

/**------------------------------------------------------------------------

  \brief vos_nv_read() - read a NV item to an output buffer

  The \a vos_nv_read() reads a NV item to an output buffer.  If the item is
  an array, this function would read the entire array. One would get a
  VOS_STATUS_E_EXISTS error when reading an invalid item.

  For error conditions of VOS_STATUS_E_EXISTS and VOS_STATUS_E_FAILURE,
  if a default buffer is provided (with a non-NULL value),
  the default buffer content is copied to the output buffer.

  \param type  - NV item type

  \param outputBuffer   - output buffer

  \param defaultBuffer  - default buffer

  \param bufferSize  - output buffer size

  \return VOS_STATUS_SUCCESS - NV item is read successfully
          VOS_STATUS_E_INVAL - one of the parameters is invalid
          VOS_STATUS_E_FAULT - defaultBuffer point is NULL
          VOS_STATUS_E_EXISTS - NV type is unsupported
          VOS_STATUS_E_FAILURE - unknown error
  \sa

  -------------------------------------------------------------------------*/
VOS_STATUS vos_nv_read( VNV_TYPE type, v_VOID_t *outputBuffer,
      v_VOID_t *defaultBuffer, v_SIZE_t bufferSize );

/**------------------------------------------------------------------------

  \brief vos_nv_readAtIndex() - read an element of a NV array to an output
  buffer

  The \a vos_nv_readAtIndex() reads an element of a NV item to an output
  buffer. If the item is not array, this function only works for index of 0.
  One would get a VOS_STATUS_E_EXISTS error when reading an invalid item.

  For error conditions of VOS_STATUS_E_EXISTS and VOS_STATUS_E_FAILURE,
  if a default buffer is provided (with a non-NULl value),
  the default buffer content is copied to the output buffer.

  \param type  - NV item type

  \param index - NV array index

  \param outputBuffer   - output buffer

  \param defaultBuffer  - default buffer

  \param bufferSize  - output buffer size

  \return VOS_STATUS_SUCCESS - NV item is read successfully
          VOS_STATUS_E_INVAL - one of the parameters is invalid
          VOS_STATUS_E_FAULT - defaultBuffer point is NULL
          VOS_STATUS_E_EXISTS - NV type is unsupported
          VOS_STATUS_E_FAILURE - unknown error
  \sa

  -------------------------------------------------------------------------*/
VOS_STATUS vos_nv_readAtIndex( VNV_TYPE type, v_UINT_t index,
      v_VOID_t *outputBuffer, v_VOID_t *defaultBuffer, v_SIZE_t bufferSize );

/**------------------------------------------------------------------------

  \brief vos_nv_write() - write to a NV item from an input buffer

  The \a vos_nv_write() writes to a NV item from an input buffer. This would
  validate the NV item if the write operation is successful.

  \param type - NV item type

  \param inputBuffer - input buffer

  \param inputBufferSize - input buffer size

  \return VOS_STATUS_SUCCESS - NV item is read successfully
          VOS_STATUS_E_INVAL - one of the parameters is invalid
          VOS_STATUS_E_FAULT - outputBuffer pointer is NULL
          VOS_STATUS_E_EXISTS - NV type is unsupported
          VOS_STATUS_E_FAILURE   - unknown error
  \sa

  -------------------------------------------------------------------------*/
VOS_STATUS vos_nv_write( VNV_TYPE type, v_VOID_t *inputBuffer,
       v_SIZE_t inputBufferSize );

/**------------------------------------------------------------------------

  \brief vos_nv_writeAtIndex() - write to an element of a NV array from an
  input buffer

  The \a vos_nv_writeAtIndex() writes to an element of a NV array from an
  input buffer.  If the item is not an array, this function only works for
  an array index of 0.  This would automatically validate the NV item if the
  write operation is successful.

  \param type - NV item type

  \param index - NV array index

  \param inputBuffer - input buffer

  \param inputBufferSize - input buffer size

  \return VOS_STATUS_SUCCESS - NV item is read successfully
          VOS_STATUS_E_INVAL - one of the parameters is invalid
          VOS_STATUS_E_FAULT - outputBuffer pointer is NULL
          VOS_STATUS_E_EXISTS - NV type is unsupported
          VOS_STATUS_E_FAILURE   - unknown error
  \sa

  -------------------------------------------------------------------------*/
VOS_STATUS vos_nv_writeAtIndex( VNV_TYPE type, v_UINT_t index,
      v_VOID_t *inputBuffer, v_SIZE_t inputBufferSize );

/**------------------------------------------------------------------------

  \brief vos_nv_getElementCount() - return element count of a NV array

  The \a vos_nv_getElementCount() returns element count of a NV array

  \param type - NV item type

  \return count if type is valid; 0 otherwise

  \sa

  -------------------------------------------------------------------------*/
VOS_INLINE_FN v_SIZE_t vos_nv_getElementCount( VNV_TYPE type )
{
   switch (type)
   {
#define ADD_VNV_ITEM(_name, _cnt, _size, _label) case (_name): return (_cnt);
      VNV_ITEM_TABLE
#undef ADD_VNV_ITEM
      default:
         return 0;
   }
}

/**------------------------------------------------------------------------

  \brief vos_nv_getElementSize() - return size of a NV element

  The \a vos_nv_getElementSize() returns size of a NV element.

  \param type - NV item type

  \return size if type is valid; 0 otherwise

  \sa

  -------------------------------------------------------------------------*/
VOS_INLINE_FN v_SIZE_t vos_nv_getElementSize( VNV_TYPE type )
{
   switch (type)
   {
#define ADD_VNV_ITEM(_name, _cnt, _size, _label) case (_name): return (_size);
      VNV_ITEM_TABLE
#undef ADD_VNV_ITEM
      default:
         return 0;
   }
}

/**------------------------------------------------------------------------

  \brief vos_nv_getItemSize() - return size of a NV item

  The \a vos_nv_getItemSize() returns size of a NV item.

  \param type - NV item type

  \return size of a NV item array if type is valid; 0 otherwise

  \sa

  -------------------------------------------------------------------------*/
VOS_INLINE_FN v_SIZE_t vos_nv_getItemSize( VNV_TYPE type )
{
   return vos_nv_getElementCount(type) * vos_nv_getElementSize(type);
}

// TODO: HAL NV interface should be used to access individual NV items
// instead of below functions once that is ready

/**------------------------------------------------------------------------

  \brief vos_nv_readTxAntennaCount() - return number of TX antenna

  \param pTxAntennaCount   - antenna count

  \return status of the NV read operation

  \sa

  -------------------------------------------------------------------------*/
VOS_STATUS vos_nv_readTxAntennaCount( v_U8_t *pTxAntennaCount );

/**------------------------------------------------------------------------

  \brief vos_nv_readRxAntennaCount() - return number of RX antenna

  \param pRxAntennaCount   - antenna count

  \return status of the NV read operation

  \sa

  -------------------------------------------------------------------------*/
VOS_STATUS vos_nv_readRxAntennaCount( v_U8_t *pRxAntennaCount );

/**------------------------------------------------------------------------

  \brief vos_nv_readMacAddress() - return the MAC address

  \param pMacAddress - MAC address

  \return status of the NV read operation

  \sa

  -------------------------------------------------------------------------*/
VOS_STATUS vos_nv_readMacAddress( v_MAC_ADDRESS_t pMacAddress );

/**------------------------------------------------------------------------

  \brief vos_nv_readMultiMacAddress() - return the Multiple MAC addresses

  \param pMacAddress - MAC address
  \param macCount - Count of valid MAC addresses to get from NV field

  \return status of the NV read operation

  \sa

  -------------------------------------------------------------------------*/
VOS_STATUS vos_nv_readMultiMacAddress( v_U8_t* pMacAddr, v_U8_t  macCount);

/**------------------------------------------------------------------------
  \brief vos_nv_getDefaultRegDomain() - return the default regulatory domain
  \return default regulatory domain
  \sa
  -------------------------------------------------------------------------*/
v_REGDOMAIN_t vos_nv_getDefaultRegDomain( void );

/**------------------------------------------------------------------------
  \brief vos_nv_getSupportedChannels() - function to return the list of
          supported channels
  \param p20MhzChannels - list of 20 Mhz channels
  \param pNum20MhzChannels - number of 20 Mhz channels
  \param p40MhzChannels - list of 20 Mhz channels
  \param pNum40MhzChannels - number of 20 Mhz channels
  \return status of the NV read operation
  \Note: 40Mhz not currently supported
  \sa
  -------------------------------------------------------------------------*/
VOS_STATUS vos_nv_getSupportedChannels( v_U8_t *p20MhzChannels, int *pNum20MhzChannels,
                                        v_U8_t *p40MhzChannels, int *pNum40MhzChannels);

/**------------------------------------------------------------------------
  \brief vos_nv_readDefaultCountryTable() - return the default Country table
  \param table data - a union to return the default country table data in.
  \return status of the NV read operation
  \sa
  -------------------------------------------------------------------------*/
VOS_STATUS vos_nv_readDefaultCountryTable( uNvTables *tableData );

/**------------------------------------------------------------------------
  \brief vos_nv_getChannelListWithPower() - function to return the list of
          supported channels with the power limit info too.
  \param pChannels20MHz - list of 20 Mhz channels
  \param pNum20MHzChannelsFound - number of 20 Mhz channels
  \param pChannels40MHz - list of 20 Mhz channels
  \param pNum40MHzChannelsFound - number of 20 Mhz channels
  \return status of the NV read operation
  \Note: 40Mhz not currently supported
  \sa
  -------------------------------------------------------------------------*/
VOS_STATUS vos_nv_getChannelListWithPower(tChannelListWithPower *pChannels20MHz /*[NUM_LEGIT_RF_CHANNELS] */,
                                          tANI_U8 *pNum20MHzChannelsFound,
                                          tChannelListWithPower *pChannels40MHz /*[NUM_CHAN_BOND_CHANNELS] */,
                                          tANI_U8 *pNum40MHzChannelsFound
                                          );

/**------------------------------------------------------------------------

  \brief vos_nv_open() - initialize the NV module

  The \a vos_nv_open() initializes the NV module.  This function read the binary
  file qcom_nv.bin for macaddress,country code,regulatory domain information and etc.

  \return VOS_STATUS_SUCCESS - module is initialized successfully
          otherwise  - module is not initialized
  \sa

  -------------------------------------------------------------------------*/
VOS_STATUS vos_nv_open(void);

/**------------------------------------------------------------------------

  \brief vos_nv_close() - uninitialize the NV module

  The \a vos_nv_init() uninitializes the NV module.  This function release the binary
  file qcom_nv.bin data buffer.

  \return VOS_STATUS_SUCCESS - module is initialized successfully
          otherwise  - module is not initialized
  \sa

  -------------------------------------------------------------------------*/

VOS_STATUS vos_nv_close(void);

/**------------------------------------------------------------------------
  \brief vos_nv_getNVBuffer -
  \param pBuffer  - to return the buffer address
         pSize    - buffer size.
  \return status of the NV read operation
  \sa
  -------------------------------------------------------------------------*/
VOS_STATUS vos_nv_getNVBuffer(v_VOID_t **pNvBuffer, v_SIZE_t *pSize);

/**------------------------------------------------------------------------
  \brief vos_nv_getNVEncodedBuffer -
  \param pBuffer  - to return the buffer address
         pSize    - buffer size.
  \return status of the NV read operation
  \sa
  -------------------------------------------------------------------------*/
VOS_STATUS vos_nv_getNVEncodedBuffer(v_VOID_t **pNvBuffer, v_SIZE_t *pSize);


/**------------------------------------------------------------------------
  \brief vos_nv_getNVDictionary -
  \param pBuffer  - to return the buffer address
         pSize    - buffer size.
  \return status of the NV read operation
  \sa
  -------------------------------------------------------------------------*/
VOS_STATUS vos_nv_getNVDictionary(v_VOID_t **pNvBuffer, v_SIZE_t *pSize);

/**------------------------------------------------------------------------
  \brief vos_nv_isEmbeddedNV() - NV.bin is embedded or not

  \return VOS_STATUS_SUCCESS - if NV is embedded
          otherwise  - NOT embedded
  \sa
  -------------------------------------------------------------------------*/
VOS_STATUS vos_nv_isEmbeddedNV(void);

/**------------------------------------------------------------------------
  \brief vos_nv_setNVEncodedBuffer() - set Encode Buffer

  \return VOS_STATUS_SUCCESS - if able to set encoded buffer successfully
          otherwise  - NOT able to set encoded data
  \sa
  -------------------------------------------------------------------------*/
VOS_STATUS vos_nv_setNVEncodedBuffer(v_U8_t *pNvBuffer, v_SIZE_t size);

/**------------------------------------------------------------------------
  \brief vos_nv_get_dictionary_data() - read dictionary data

  \return VOS_STATUS_SUCCESS - if dictionary data is read successfully
          otherwise  - NOT able to read dictionary data
  \sa
  -------------------------------------------------------------------------*/
VOS_STATUS vos_nv_get_dictionary_data(void);

/**------------------------------------------------------------------------
  \brief vos_nv_setRegDomain -
  \param clientCtxt  - Client Context, Not used for PRIMA
              regId  - Regulatory Domain ID
              sendRegHint - send hint to cfg80211
  \return status set REG domain operation
  \sa
  -------------------------------------------------------------------------*/
VOS_STATUS vos_nv_setRegDomain(void * clientCtxt, v_REGDOMAIN_t regId,
                                                  v_BOOL_t sendRegHint);

/**------------------------------------------------------------------------
  \brief vos_nv_getChannelEnabledState -
  \param rfChannel  - input channel number to know enabled state
  \return eNVChannelEnabledType enabled state for channel
             * enabled
             * disabled
             * DFS
  \sa
  -------------------------------------------------------------------------*/

eNVChannelEnabledType vos_nv_getChannelEnabledState
(
   v_U32_t    rfChannel
);

VOS_STATUS vos_init_wiphy_from_nv_bin(void);

/**------------------------------------------------------------------------
  \brief vos_nv_getNvVersion -
  \param NONE
  \return eNvVersionType NV.bin version
             * E_NV_V2
             * E_NV_V3
             * E_NV_INVALID
  \sa
  -------------------------------------------------------------------------*/
eNvVersionType vos_nv_getNvVersion
(
   void
);


/**------------------------------------------------------------------------
  \brief vos_chan_to_freq -
  \param   - input channel number to know channel frequency
  \return Channel frequency
  \sa
  -------------------------------------------------------------------------*/
v_U16_t vos_chan_to_freq(v_U8_t chanNum);

/**------------------------------------------------------------------------
  \brief vos_freq_to_chan -
  \param   - input channel frequency to know channel number
  \return Channel frequency
  \sa
  -------------------------------------------------------------------------*/
v_U8_t vos_freq_to_chan(v_U32_t freq);

/**------------------------------------------------------------------------
  \brief vos_is_nv_country_non_zero -
  \param   NONE
  \return Success if default Country is Non-Zero
  \sa
  -------------------------------------------------------------------------*/

v_BOOL_t vos_is_nv_country_non_zero
(
   void
);

/**------------------------------------------------------------------------
  \brief vos_is_channel_valid_for_vht80 -
  \param   chan
  \return TRUE if channel is 80 mhz
  \sa
  -------------------------------------------------------------------------*/

v_BOOL_t vos_is_channel_valid_for_vht80(v_U32_t chan);

#ifdef CONFIG_ENABLE_LINUX_REG
/**------------------------------------------------------------------------
  \brief vos_getCurrentCountryCode -
  \param   countrycode
  \return None
  \sa
  -------------------------------------------------------------------------*/

void vos_getCurrentCountryCode
(
   tANI_U8 *cc
);
#endif

int vos_update_nv_table_from_wiphy_band(void *hdd_ctx,
                                         void *wiphy,v_U8_t nBandCapability);

#endif // __VOS_NVITEM_H
