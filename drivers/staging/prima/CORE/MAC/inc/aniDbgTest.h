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

#ifndef _ANIDBGTEST_H_
#define _ANIDBGTEST_H_

#include "sirTypes.h"

#define MAX_PARMS_SIZE              256
#define MAX_RESPONSE_SIZE           512
#define MAX_PCI_CFG_WRDS            19
#define MAX_NUM_TST_STAS_PER_AP     3

#define ANI_PCI_CFG_MIN_GNT 4
#define ANI_PCI_CFG_MAX_LAT 32
/// EEPROM Product Ids for different types of NICs.
#define EEP_PRODUCT_ID_MPCI_3_2    1
#define EEP_PRODUCT_ID_MPCI_2_2    2
#define EEP_PRODUCT_ID_CARDBUS     3
#define EEP_PRODUCT_ID_HEAP_W_INT  4
#define EEP_PRODUCT_ID_HEAP_W_EXT  5
#define EEP_PRODUCT_ID_MRAP        6

#define ANI_CARDBUS_EXPECTED_CIS \
                  (((0x3416 + sizeof(tPolSystemParameters)) << 3)|1)


/// Here is an example of a test structure
typedef enum sANI_TEST_ID
{
    // All DVT test Ids, beginning with BMU, INT, HIF, SP, TFP, RHP,
    // RFP, SYS, PHY, SPI, CYG, LO, and EEPROM in the order specified.
    ANI_TESTID_NONE = 0,

    // PLEASE PRESERVE THE ORDER; IF YOU NEED TO CHANGE.
    // TEST SCRIPTS DEPEND ON THIS ORDER.

    ANI_BUS_ACCESS_TEST,
    ANI_MEMORY_TEST,
    ANI_INTERRUPT_TEST,
    ANI_REG_READ_WRITE_TEST,
    ANI_BMU_INIT_PDU_TEST,
    ANI_HASH_TBL_ADD_DEL_GET_TEST_ID,
    ANI_HASH_TBL_MULT_ENTRY_TEST_ID,

    // Data path tests
    ANI_SEND_CONTINUOUS_TEST,
    ANI_PKT_ROUTING_TEST,

    ANI_ADD_STA_TEST_ID,
    ANI_DELETE_STA_TEST_ID,

    // HIF Tests
    ANI_BURST_READ_WRITE_TEST_ID,
    ANI_MOVE_ACK_TEST_ID,
    ANI_RESET_WQ_TEST_ID,
    ANI_HIF_LPBK_TEST_ID,

    // SP Tests

    // TFP Tests
    ANI_BCN_GEN_TEST_ID,

    // RHP Tests

    ANI_HASH_TBL_DELETE_TEST_ID,
    ANI_HASH_TBL_GET_TEST_ID,

    // RHP Tests
    ANI_AGING_TEST_ID,
    ANI_COUNTERS_TEST_ID,

    // SYS Tests
    ANI_SET_PROMOSCOUS_MODE_TEST_ID,
    ANI_SET_SCAN_MODE_TEST_ID,

    // SPI Tests
    ANI_SET_CYG_REG_READ_WRITE_TEST_ID,

    // CYG Tests
    ANI_SET_BB_CAL_TEST_ID,

    // LO Tests
    ANI_SET_LO_CHAN_TEST_ID,

    // EEPROM Tests

    // Cal test
    ANI_CAL_TEST_ID,

    // HIF Burst DMA test
    ANI_HIF_BURST_DMA_TEST_ID,

    // Add all your test Ids above this.
    ANI_TESTS_MAX
} tANI_TEST_ID;

/// Test IDs for tests directly handled by the HDD
typedef enum sANI_HDD_TEST_ID {
    ANI_HDD_TESTS_START = 0x10000,
    ANI_PCI_CFG_TEST = ANI_HDD_TESTS_START,
    ANI_EEPROM_TEST,

    // Add all your HDD test Ids above this.
    ANI_HDD_TESTS_MAX

} tANI_HDD_TEST_ID;

/// Test trigger types can be enhanced with this enumeration
typedef enum sANI_TEST_TRIG_TYPE
{
    ANI_TRIG_TYPE_NONE = 0,

    ANI_TRIG_TYPE_CFG,
    ANI_TRIG_TYPE_START,
    ANI_TRIG_TYPE_GET_RESULTS,
    ANI_TRIG_TYPE_STOP,

    ANI_TRIG_TYPE_MAX
} tANI_TEST_TRIG_TYPE;

/// Test states
typedef enum sANI_DBG_TEST_STATE
{
    ANI_TEST_STATE_IDLE = 0,
    ANI_TEST_STATE_IN_PROGRESS,
    ANI_TEST_STATE_DONE

} tANI_DBG_TEST_STATE;

/**
 * The following CFG Types are defined for each type of a parameter that can
 * be independently set
 */
typedef enum sANI_DBG_CFG_TYPES
{
    ANI_DBG_CFG_PHY_MODE_RATE,
    ANI_DBG_CFG_ROUTING_FLAGS,
    ANI_DBG_CFG_STA_ID,
    ANI_DBG_CFG_IS_INFRASTRUCTURE_MODE,
    ANI_DBG_CFG_USE_REAL_PHY,
    ANI_DBG_CFG_PHY_DROPS,
    ANI_DBG_CFG_ADD_STA,
    ANI_DBG_CFG_DEL_STA,
    ANI_DBG_CFG_ENABLE_STA_TX,
    ANI_DBG_CFG_DISABLE_STA_TX,
    ANI_DBG_CFG_READ_REGISTER,
    ANI_DBG_CFG_WRITE_REGISTER,
    ANI_DBG_CFG_GET_ARQ_WINDOW_SIZE,
    ANI_DBG_CFG_SET_ARQ_WINDOW_SIZE,
    ANI_DBG_CFG_SET_CHANNEL,
    ANI_DBG_CFG_SET_MAC_ADDRESS,
    ANI_DBG_CFG_SET_MEM,
    ANI_DBG_CFG_GET_MEM,
    ANI_DBG_CFG_CTRL_TXWQ,
    ANI_DBG_CFG_GET_ACTIVITY_SET,
    ANI_DBG_CFG_SET_ACK_POLICY,
    ANI_DBG_CFG_AGING_CMD,
    ANI_DBG_CFG_SET_KEY,
    ANI_DBG_CFG_SET_PER_STA_KEY,
    ANI_DBG_CFG_TFP_ABORT,
    ANI_DBG_CFG_GET_ACT_CHAINS,
    ANI_DBG_CFG_IS_CHAIN_ACTIVE,
    ANI_DBG_CFG_BB_FILTER_CAL,
    ANI_DBG_CFG_DCO_CAL,
    ANI_DBG_CFG_IQ_CAL,
    ANI_DBG_CFG_TX_LO_LEAKAGE_CAL,
    ANI_DBG_CFG_SEND_PKTS,
    ANI_DBG_CFG_STOP_PKTS,
    ANI_DBG_CFG_ENABLE_DISABLE_BEACON_GEN,
    ANI_DBG_CFG_DCO_GET,
    ANI_DBG_CFG_DCO_SET,
    ANI_DBG_CFG_SET_PWR_TEMPL,
    ANI_DBG_CFG_GET_PWR_TEMPL,
    ANI_DBG_CFG_INIT,
    ANI_DBG_CFG_SET_EEPROM_FLD,
    ANI_DBG_CFG_GET_EEPROM_FLD,
    ANI_DBG_CFG_SET_NUM_TRANSMITTERS,
    ANI_DBG_CFG_SET_RX_CHAINS,
    ANI_DBG_CFG_HCF_TEST,
    ANI_DBG_CFG_POLARIS_REV_ID,
    ANI_DBG_CFG_UPDATE_DATA_FROM_EEPROM,
    ANI_DBG_CFG_GET_TEMP,
    ANI_DBG_CFG_SET_STA_ADDRLST,
    ANI_DBG_CFG_GET_PWR_GAIN,
    ANI_DBG_CFG_SW_CLOSED_LOOP_TPC,

    ANI_DBG_CFG_TYPE_MAX
} tANI_DBG_CFG_TYPES;

/**
 * The following are the length definitions for each CFG TYPE defined
 * in earlier Type enum.
 */

// first byte mode and second rate
# define ANI_DBG_CFG_PHY_MODE_RATE_LEN 2

// MSN is type and LSN is sub-type in the MSB. 4 LSBs are routing flags
# define ANI_DBG_CFG_ROUTING_FLAGS_LEN 5

# define ANI_DBG_CFG_STA_ID_LEN        2

// ======================================================================
//
// Following are the structure definitions for the config input parameters
//
// ======================================================================
//
// --------------------------------------------------------

// --------------------------------------------------------
// Output Params for Read Register Config request
typedef struct sAniDbgCfgGetPolarisVersionResponse
{
    // Output parameters
    unsigned int rc;  // 0 - SUCCESS
    unsigned long version;
} tAniDbgCfgGetPolarisVersionResponse, *tpAniDbgCfgGetPolarisVersionResponse;

// -------------------------------------------------------------
// Input Params for the Phy Mode Rate

typedef struct sAniDbgCfgPhyModeRateParams
{
    unsigned int phyMode;
    unsigned int phyRate;
} tAniDbgCfgPhyModeRateParams, *tpAniDbgCfgPhyModeRateParams;

// --------------------------------------------------------
// Input Params for the Routing Flags
typedef struct sAniDbgCfgRoutingFlagsParams
{
    unsigned int type;
    unsigned int subType;
    unsigned int routingFlags;
} tAniDbgCfgRoutingFlagsParams, *tpAniDbgCfgRoutingFlagsParams;

// --------------------------------------------------------
// Input Params for the STA ID
typedef struct sAniDbgCfgStaIdParams
{
    unsigned int staId;
} tAniDbgCfgStaIdParams, *tpAniDbgCfgStaIdParams;

// --------------------------------------------------------
// Input Params for "Is Infrastructure Mode"
typedef struct sAniDbgCfgIsInfrastructureParams
{
    unsigned int isInfrastructureMode;
} tAniDbgCfgIsInfrastructureParams, *tpAniDbgCfgIsInfrastructureParams;

// --------------------------------------------------------
// Input Params for the Real Phy
typedef struct sAniDbgCfgRealPhyParams
{
    unsigned int useRealPhy;
} tAniDbgCfgRealPhyParams, *tpAniDbgCfgRealPhyParams;

// --------------------------------------------------------
// Input Params for Phy Drops
typedef struct sAniDbgCfgPhyDropParams
{
    unsigned int usePhyDrops;
    unsigned int rate;
    unsigned int burstSize;
    unsigned int mode;
} tAniDbgCfgPhyDropParams, *tpAniDbgCfgPhyDropParams;

// --------------------------------------------------------
// Input Params for Add Sta
typedef struct sAniDbgCfgAddStaParams
{
    unsigned int staId;
    unsigned char macAddr[6];
    unsigned int phyMode;
    unsigned int rate;
    unsigned int skipSP;
    unsigned int ackPolicy;
} tAniDbgCfgAddStaParams, *tpAniDbgCfgAddStaParams;

// --------------------------------------------------------
// Input Params for Delete Sta
typedef struct sAniDbgCfgDelStaParams
{
    unsigned int staId;
    unsigned char macAddr[6];
} tAniDbgCfgDelStaParams, *tpAniDbgCfgDelStaParams;

// --------------------------------------------------------
// Lowest register address allowable for the Read Register calls
#define ANI_TIT_MIN_REG_ADDR 0x02000000


// --------------------------------------------------------
// Highest register address allowable for the Read Register call
#define ANI_TIT_MAX_REG_ADDR 0x0203ffff

// --------------------------------------------------------
// Input Params for Read Register Config request
typedef struct sAniDbgCfgReadRegParams
{
    unsigned long regAddr;
} tAniDbgCfgReadRegParams, *tpAniDbgCfgReadRegParams;

// --------------------------------------------------------
// Input Params for Write Register Config request
typedef struct sAniDbgCfgWriteRegParams
{
    unsigned long regAddr;
    unsigned long regVal;
} tAniDbgCfgWriteRegParams, *tpAniDbgCfgWriteRegParams;




// --------------------------------------------------------
// ANI_DBG_CFG_SET_CAL_TONE,
typedef struct
{
    unsigned long toneID;   //0 = -28, 1 = -24, ..., 6 = -4, 7 = +4, 8 = +8, 9 = +12, ..., 13 = +28
}tAniDbgCfgSetToneId, *tpAniDbgCfgSetToneId;


// --------------------------------------------------------
// Output Params for Read Register Config request
typedef struct sAniDbgCfgRegReadResponse
{
    // Output parameters
    unsigned int rc;  // 0 - SUCCESS
    unsigned long regVal;
} tAniDbgCfgReadRegResponse, *tpAniDbgCfgReadRegResponse;

// --------------------------------------------------------
// Output Params for getting ARQ Window
typedef struct sAniDbgCfgGetArqWindowResponse
{
    unsigned int rc;  // 0 - SUCCESS
    unsigned long val;
} tAniDbgCfgGetArqWindowResponse, *tpAniDbgCfgGetArqWindowResponse;

// --------------------------------------------------------
// Input Params for Write Register Config request
typedef struct sAniDbgCfgSetArqWindowParams
{
    unsigned long windowSize;
} tAniDbgCfgSetArqWindowRegParams, *tpAniDbgCfgSetArqWindowParams;

// --------------------------------------------------------
// Input Params for Write Register Config request
typedef struct sAniDbgCfgSetChanParams
{
    unsigned long chId;
} tAniDbgCfgSetChanParams, *tpAniDbgCfgSetChanParams;


// --------------------------------------------------------
// Input Params for Write Register Config request
typedef struct sAniDbgCfgSetMacAddrParams
{
    unsigned char macAddr[6];
    unsigned int  flag;       // 0 - BSSID; 1 - MAC Address of DUT
} tAniDbgCfgSetMacAddrParams, *tpAniDbgCfgSetMacAddrParams;

// --------------------------------------------------------
// Input Params for Set Memory request
typedef struct sAniDbgCfgSetMemoryParams
{
    unsigned int fUseBurstDma;
    unsigned int numOfWords;
    unsigned int ahbAddr;
    unsigned int writeData; // Pattern to be written out in memory
} tAniDbgCfgSetMemoryParams, *tpAniDbgCfgSetMemoryParams;

// --------------------------------------------------------
// Input Params for Get Memory request
typedef struct sAniDbgCfgGetMemoryParams
{
    unsigned int fUseBurstDma;
    unsigned int numOfWords;
    unsigned int ahbAddr;
} tAniDbgCfgGetMemoryParams, *tpAniDbgCfgGetMemoryParams;

// --------------------------------------------------------
// Response structure for the Get Memory request
typedef struct sAniDbgCfgGetMemoryResponse
{
    unsigned int rc;  // 0 - SUCCESS; Otherwise FAILED
    unsigned int readData[1];
} tAniDbgCfgGetMemoryResponse, *tpAniDbgCfgGetMemoryResponse;


// --------------------------------------------------------
// Input Params for Controls Enable/Disable of TX WQ
typedef struct sAniDbgCfgCtrlTxWqParams
{
    unsigned int staId;
    unsigned int wqId;
    unsigned int action; // 0 - Disable; 1 - Enable

} tAniDbgCfgCtrlTxWqParams, *tpAniDbgCfgCtrlTxWqParams;

// --------------------------------------------------------
// Input Params for Getting TX/RX Activity Set
typedef struct sAniDbgCfgGetAsParams
{
    unsigned int id;     // 0 - tx; 1 - rx

} tAniDbgCfgGetAsParams, *tpAniDbgCfgGetAsParams;

// Input Params for Getting TX/RX Activity Set
typedef struct sAniDbgCfgGetAsResponse
{
    unsigned int rc;  // 0 - Success
    unsigned int nEntries;
    unsigned int entries[64];

} tAniDbgCfgGetAsResponse, *tpAniDbgCfgGetAsResponse;

// --------------------------------------------------------
// Input Params for Set ACK Policy
typedef struct sAniDbgCfgSetAckPolicyParams
{
    unsigned int id;     // 0 - tx; 1 - rx
    unsigned int policy;
    unsigned int staId;
    unsigned int tcId;

} tAniDbgCfgSetAckPolicyParams, *tpAniDbgCfgSetAckPolicyParams;

// --------------------------------------------------------
// Input Params to Run AGING command
typedef struct sAniDbgCfgAgingCmdParams
{
    unsigned int staId;
    unsigned int tcId;

} tAniDbgCfgAgingCmdParams, *tpAniDbgCfgAgingCmdParams;

// --------------------------------------------------------
// Input Params to TFP Abort command
typedef struct sAniDbgCfgTfpAbortParams
{
    unsigned int staId;

} tAniDbgCfgTfpAbortParams, *tpAniDbgCfgTfpAbortParams;

// --------------------------------------------------------
// Input Params to Enable/Disable Beacon command
typedef struct sAniDbgCfgEnableBeaconParams
{
    unsigned int fEnableBeacons;

} tAniDbgCfgEnableBeaconParams, *tpAniDbgCfgEnableBeaconParams;

// --------------------------------------------------------
// Input Params to SET WEP / AES MULTICAST KEY
typedef struct sAniDbgCfgSetKeyParams
{
    unsigned int type;  // 0 - WEP; 1 - AES
    unsigned int keyId; // If AES, 0 - TX; 1 - RX
    unsigned int keyLen;
    unsigned char key[16];

} tAniDbgCfgSetKeyParams, *tpAniDbgCfgSetKeyParams;

// --------------------------------------------------------
// Input Params to SET per STA keys
typedef struct sAniDbgCfgSetPerStaKeyParams
{
    unsigned int staId;
    unsigned int id0;
    unsigned int id1;
    unsigned int keyValid; // 0 - Tx; Anything else Rx
    unsigned int useDefaultKey;
    unsigned int defaultKeyId;
    unsigned int edPolicy;
    unsigned char key[16];
    unsigned char keylen;

} tAniDbgCfgSetPerStaKeyParams, *tpAniDbgCfgSetPerStaKeyParams;

// --------------------------------------------------------
// Cal test and Set Chan functions
typedef struct sAniDbgCalTestParams
{
    unsigned int id;   // 0 - Cal; 1 - Set Channel
    unsigned int chId; // if id == 1; then chId is 1 - 14 or 36 - end of 11a
}tAniDbgCalTestParams, *tpAniDbgCalTestParams;

typedef struct sAniDbgCalTestResponse
{
    unsigned int rc;
}tAniDbgCalTestResponse, *tpAniDbgCalTestResponse;

// ---------------------------------------------------------
// Input params to Get DCO params
typedef struct sAniDbgCfgGetDcoParams
{
    unsigned int chain;
    unsigned int address;
} tAniDbgCfgGetDcoParams, *tpAniDbgCfgGetDcoParams;

typedef struct sAniDbgCfgGetDcoResponse
{
    unsigned int rc;
    unsigned int val;
} tAniDbgCfgGetDcoResponse, *tpAniDbgCfgGetDcoResponse;

// --------------------------------------------------------
// Input params to Get DCO params
typedef struct sAniDbgCfgSetDcoParams
{
    unsigned int chain;
    unsigned int address;
    unsigned int val;
} tAniDbgCfgSetDcoParams, *tpAniDbgCfgSetDcoParams;

// --------------------------------------------------------
// Input params to setting power template
typedef struct sAniDbgCfgSetPwrTemplParams
{
    unsigned int staId;
    unsigned int mode;
    unsigned int rate;
} tAniDbgCfgSetPwrTemplParams, *tpAniDbgCfgSetPwrTemplParams;


// Response struct for Getting power template
typedef struct sAniDbgCfgGetPwrTemplParams
{
    unsigned int staId;
} tAniDbgCfgGetPwrTemplParams, *tpAniDbgCfgGetPwrTemplParams;

// --------------------------------------------------------
// Request struct for Setting the init config parameters
typedef struct sAniDbgCfgInitParams
{
    unsigned int mode; // default mode
    unsigned int rate; // default rate
    unsigned int fIsInfMode; // Set 1 for infrastructure mode
    unsigned int staId;
    unsigned char ownMacAddr[6]; // Set the configured MAC address
    unsigned int fFwd2Host; // Set 1 to Forward data to host
    unsigned int fUseRealPhy; // Set 1 to use the real Phy
    unsigned int fEnablePhyDrops; // Set 1 to enable Phy drops
    unsigned int dropModeRate; // Set this when Phy drops are enabled
    unsigned int dropModeSize; // Set this when Phy drops are enabled
    unsigned int dropModeMode; // Set this when Phy drops are enabled
} tAniDbgCfgInitParams, *tpAniDbgCfgInitParams;


// Response struct for Getting power template
typedef struct sAniDbgCfgGetPwrTemplResponse
{
    unsigned int rc;
    unsigned int val;
} tAniDbgCfgGetPwrTemplResponse, *tpAniDbgCfgGetPwrTemplResponse;


// --------------------------------------------------------
// Input params for setting a field in the EEPROM

typedef union sAniDbgCfgEepByteSetParams
{
    unsigned char mask;
    unsigned char value;

} tAniDbgCfgEepByteSetParams, *tpAniDbgCfgEepByteSetParams;


// Request struct for Setting the EEPROM field
typedef struct sAniDbgCfgEepSetParams
{
    unsigned int offset;
    unsigned int size;
    unsigned int fIsMaskPresent;
    unsigned char setParams[1];
} tAniDbgCfgEepSetParams, *tpAniDbgCfgEepSetParams;

// The response structure for this particular request is same as
// the generic response structure.

// ------------------------------------------------------------
// Input params for getting the value of a field in the EEPROM

// Request struct for Getting the EEPROM field
typedef struct sAniDbgCfgEepGetParams
{
    unsigned int offset;
    unsigned int size;
} tAniDbgCfgEepGetParams, *tpAniDbgCfgEepGetParams;


// Response struct for Getting the EEPROM field
typedef struct sAniDbgCfgEepGetResponse
{
    unsigned int rc;
    unsigned char value[1];
} tAniDbgCfgEepGetResponse, *tpAniDbgCfgEepGetResponse;

// --------------------------------------------------------
// Input params for setting the number of transmitters

// Request struct for setting the number of transmitters
typedef struct sAniDbgCfgSetNumTransmitters
{
    unsigned int numTransmitters;

} tAniDbgCfgSetNumTransmitters, *tpAniDbgCfgSetNumTransmitters;

// --------------------------------------------------------
// Input params for Enabling/Disabling Rx chains

// Request struct for Enabling/Disabling Rx chains
typedef struct sAniDbgCfgSetRxChains
{
    unsigned int numChains;
    unsigned char chainIndices[3];
} tAniDbgCfgSetRxChains, *tpAniDbgCfgSetRxChains;

// --------------------------------------------------------
// Input params for enable/disable SW closed loop TPC

// Request struct for enable/disable SW closed loop TPC
typedef struct sAniDbgCfgSwClosedLoopTpc
{
    unsigned int action;
} tAniDbgCfgSwClosedLoopTpc, *tpAniDbgCfgSwClosedLoopTpc;

// --------------------------------------------------------
// Input params for setting the list of test STA MAC address,
// that will be operating with the AP, for the Multi-NIC tests.
typedef struct sAniDbgCfgSetStaAddrLst
{
    unsigned long numStas;
    unsigned char macAddrLst[MAX_NUM_TST_STAS_PER_AP][6];
} tAniDbgCfgSetStaAddrLst, *tpAniDbgCfgSetStaAddrLst;

// --------------------------------------------------------
// Output Params for getting the current Power and Gain settings
// for a particular STA.
typedef struct sAniDbgCfgGetPwrGainResponse
{
    unsigned int rc;  // 0 - SUCCESS
    unsigned long pwrCode;
    unsigned long gain0;
    unsigned long gain1;
} tAniDbgCfgGetPwrGainResponse, *tpAniDbgCfgGetPwrGainResponse;

// --------------------------------------------------------
// Input Params containing the STAID for getting the current Power and Gain settings.
typedef struct sAniDbgCfgGetPwrGainParams
{
    unsigned long staID;
} tAniDbgCfgGetPwrGainParams, *tpAniDbgCfgGetPwrGainParams;


// --------------------------------------------------------
// Output params for getting temperature

// Response struct for getting temperature of the radio card
// NOTE:- Stop the traffic to measure temperature
typedef struct sAniDbgCfgGetTempRsp
{
    unsigned int rc;     // 0 - SUCCESS; Otherwise FAILED
    unsigned int temp0;  // Chain 0 temperature
    unsigned int temp1;  // Chain 1 temperature
} tAniDbgCfgGetTempRsp, *tpAniDbgCfgGetTempRsp;


// --------------------------------------------------------
// Generic Response structure for Config requests
typedef struct sAniDbgCfgResponse
{
    unsigned int rc;  // 0 - SUCCESS; Otherwise FAILED
} tAniDbgCfgResponse, *tpAniDbgCfgResponse;

// ====================================================================
/*
   For each test there will be a structure defined in this file with
   the following test descriptions of testId, test trigger type, input
   parameters and expected output

   This is a template for DBG test structure

   typedef struct sANIAPI_XXXX_TEST_PARAMS
   {
   // Input paramters
   int xyz;  // Test specific
   } tANIAPI_XXXX_TEST_PARAMS;

   typedef struct sANIAPI_XXXX_TEST_RESPONSE
   {
   // Output parameters
   int rc;  // Must be ZERO for success and must have error code
   // for failure
   int zyx; // Any counters or any debug these will be test spefics
   } tANIAPI_XXXX_TEST_RESPONSE;

   ********************************************* */
// =====================================================================

#define ANI_CFG_OPER_GET    0x0
#define ANI_CFG_OPER_SET    0x1


typedef struct sAniDbgCfg
{
    unsigned char oper;
    unsigned char data[124]; // total parms structure must be MAX 128 bytes
} tAniDbgCfg;

// --------------------------------------------------------------------

// Response structures for InitPdu test
// No Input Params for this test
typedef struct sAniDbgInitPduTestResponse
{
    // Output parameters
    unsigned int rc;  // Must be ZERO for success and must have error code
                      // for failure

    unsigned int expected;
    unsigned int i_current;
    unsigned int totalPduCount;
} tAniDbgInitPduTestResponse, *tpAniDbgInitPduTestResponse;


// --------------------------------------------------------
// Response structures for BusAccess/RegReadWrite tests
// No Input Params for this test

typedef struct sAniDbgRegReadWriteTestResponse
{
    // Output parameters
    unsigned int rc;  // 0 - SUCCESS; Anything else is an ERROR
    unsigned int registerAddress;
    unsigned int expected;
    unsigned int i_current;
} tAniDbgRegReadWriteTestResponse, *tpAniDbgRegReadWriteTestResponse;

// --------------------------------------------------------
// Response structures for Interrupt test
// No Input Params for this test

typedef struct sAniDbgIntrTestResponse
{
    // Output parameters
    unsigned int rc;       // 0 - SUCCESS; Anything else is an ERROR
    unsigned int status;   // Interrupt status register
    unsigned int mask;     // Interrupt mask in test
    unsigned int bmu;      // BMU MB status register contents

} tAniDbgIntrTestResponse, *tpAniDbgIntrTestResponse;

// --------------------------------------------------------
// No Input Params for this test

typedef struct sAniDbgMemoryTestParams
{
    unsigned int startAddress;
    unsigned int endAddress;
    unsigned int pattern;
    unsigned int testLevel;
} tAniDbgMemoryTestParams, *tpAniDbgMemoryTestParams;

// ---------------------------------------------------------------------

// Response structures for RegReadWrite test

typedef struct sAniDbgMemoryTestResponse
{
    // Output parameters
    unsigned int rc;  // Must be ZERO for success and must have error code
    // for failure
    unsigned int expected;
    unsigned int i_current;
    unsigned int currentAddress;
    unsigned int testAddress;
} tAniDbgMemoryTestResponse, *tpAniDbgMemoryTestResponse;

// --------------------------------------------------------
// Input Parameters for the HIF Burst Read Write test

typedef struct sAniDbgHIFBurstRdWrtTestParams
{
    unsigned int AHBAddr;
    unsigned int size;
    unsigned char data[1];

} tAniDbgHIFBurstRdWrtTestParams, *tpAniDbgHIFBurstRdWrtTestParams;

// ---------------------------------------------------------------------

// Response structure for the HIF Burst Read Write test

typedef struct sAniDbgHIFBurstRdWrtTestResponse
{
    // Output parameters
    unsigned int rc;  // Must be ZERO for success and must have error code
    // for failure
    unsigned char readData[1];

} tAniDbgHIFBurstRdWrtTestResponse, *tpAniDbgHIFBurstRdWrtTestResponse;


// --------------------------------------------------------

// Take the routing flags and number of frames, as Params for this test

typedef struct sAniDbgSendContinuousTestParams
{
    unsigned int  routingFlags;

    // 0 - continuously send packets till it is instructed to stop.
    unsigned int  numFramesToSend;

    // 1 - TM ring, otherwise the TD ring will be used.
    unsigned int  fUseTMRing;

    // 1 - loopback packets through the MAC
    unsigned int fLoopBkPkts;
} tAniDbgSendContinuousTestParams, *tpAniDbgSendContinuousTestParams;

// Response structures for SendContinuous test

typedef struct sAniDbgSendContinuousTestResponse
{
    unsigned int rc;  // 0 - Success and anything else is a failure

    unsigned int numPktsSent;
    unsigned int numPktsFailed;
    // Size of the last frame attempted to be sent, in case of a failure
    unsigned int lastFailedPayloadSize;
} tAniDbgSendContinuousTestResponse, *tpAniDbgSendContinuousTestResponse;

// --------------------------------------------------------
// Input parameters for the Packet Routing test
typedef struct sAniDbgPktRoutingTestParams
{
    // Routing flags for the test
    unsigned int  routingFlags;

    // Enter 1 for Hardware Seqno
    unsigned int hsBit;

    // Enter unicast ackPolicy(4 for random)
    unsigned int  ackPolicy;

    // RTS (2 for random)
    unsigned int  rtsFlag;

    // 0 - continuously send packets till it is instructed to stop.
    unsigned int  numPktsToSend;

    // 0xffffffff - For random frame types.
    unsigned int  frameType;

    // 0xffffffff - generates random frame sub-types.
    unsigned int  frmSubType;

    // 1 - TM ring; Otherwise the TD ring will be used.
    unsigned int  fUseTMRing;

    // 0 - random payload sizes
    unsigned int  payloadSize;

    // If "payloadSize" above is set to zero then this is ignored
    unsigned int  fragSize;

    // This parameter specifies whether a unicast packet should be fragmented.
    // Ignored, if "payloadSize" above is set to a non-zero value.
    unsigned int  fragment;

    // Use a value >3 for random staId generation
    unsigned int staId;

    // Use a value >7 for random tcId generation
    unsigned int tcId;

    // Enter random STA range (1 for STAs(0-1),3 for STAs(0-3))
    unsigned int staRange;

    // TC range (1 for TCs(0-1),3 for TCs(0-3),7 for TCs(0-7)
    unsigned int tcRange;

    // burst size (< 11)
    unsigned int burst;

    // Enter 1 to compute CRC
    unsigned int crc;

    // Enter 1 to loopback packets through the MAC
    unsigned int fLoopBkPkts;

} tAniDbgPktRoutingTestParams, *tpAniDbgPktRoutingTestParams;

// Structures where response parameters are constructed by the
// dvtSendPackets() routine

typedef struct sAniDbgSendPktResponse
{
    // Total packets sent
    unsigned int pktGenCount;

    unsigned int fragCnt;

    unsigned int byteCnt;

    unsigned int lowPduCnt;

    unsigned int qFullCnt;

    // Specifies the size of the last frame attempted to be sent
    unsigned int lastPayloadSize;

    // Specifies the size of the last frame's fragment size
    unsigned int lastFragSize;

    // Immediate ACK
    unsigned int cumImmAck;

    // NoACK
    unsigned int cumNoAck;

    // RTS
    unsigned int cumRTS;

    // No ACK
    unsigned int cumNoRTS;

    // TC histogram (pkts)
    unsigned int cumTC[8];

    //Fragments histogram (frags)
    unsigned int cumFrag[16];

    // STA histogram (pkts,frags)
    unsigned int cumSTA[4];
    unsigned int cumSTAFrags[4];

} tAniDbgSendPktResponse, *tpAniDbgSendPktResponse;

// Response structures for the Packet Routing test
typedef struct sAniDbgPktRoutingTestResponse
{
    // Output parameters
    unsigned int rc;  // Must be ZERO for success and must have error code
    // for failure

    // Field where response parameters are constructed by the
    // dvtSendPackets() routine
    tAniDbgSendPktResponse sendPktsRsp;

} tAniDbgPktRoutingTestResponse, *tpAniDbgPktRoutingTestResponse;

// --------------------------------------------------------

typedef enum sPciCfgTestStatus
{
    ePCI_CFG_TEST_SUCCESS,
    ePCI_CFG_TEST_READ_FAILURE,
    ePCI_CFG_TEST_VEN_DEV_ID_MISMATCH,
    ePCI_CFG_TEST_MIN_GNT_MISMATCH,
    ePCI_CFG_TEST_MAX_LAT_MISMATCH,
    ePCI_CFG_TEST_CIS_PTR_MISMATCH,
    ePCI_CFG_TEST_CIS_CONTENTS_MISMATCH
} tPciCfgTestStatus;

// Response structures for the PCI Config test
typedef struct sAniDbgPciCfgTestResponse
{
    // Output parameters
    unsigned int rc;  // Must be ZERO for success and must have error code
    // for failure

    // Field where the PCI config words, for Polaris are returned by the HDD
    unsigned int pciConfig[MAX_PCI_CFG_WRDS];

} tAniDbgPciCfgTestResponse, *tpAniDbgPciCfgTestResponse;

// --------------------------------------------------------

// Various return codes returned for the EEPROM test.
typedef enum sEepromTestStatus
{
    eEEPROM_TEST_SUCCESS,
    eEEPROM_TEST_FILE_OPEN_FAILURE,
    eEEPROM_TEST_FILE_MAP_FAILURE,
    eEEPROM_TEST_INVALID_FILE_SIZE,
    eEEPROM_TEST_MEMORY_ALLOC_FAILURE,
    eEEPROM_TEST_CRC_MISMATCH_FAILURE
} tEepromTestStatus;


// Take the EEPROM filename, as Params for this test

typedef struct sAniDbgEepromTestParams
{
    // EEPROM File Name.
    char  eepromFilename[256];

} tAniDbgEepromTestParams, *tpAniDbgEepromTestParams;

// Response structures for SendContinuous test

typedef struct sAniDbgEepromTestResponse
{
    unsigned int rc;  // 0 - Success and anything else is a failure

} tAniDbgEepromTestResponse, *tpAniDbgEepromTestResponse;

// ---------------------------------------------------------------------

// input params for RHP HASH TBL tests

typedef struct sAniDbgRhpHashTblMultipleEntryTestParams
{
    unsigned int n; // number of entries

} tAniDbgRhpHashTblMultipleEntryTestParams,
    *tpAniDbgRhpHashTblMultipleEntryTestParams;

typedef struct sAniDbgRhpHashTblTestParams
{
    unsigned int  staId;
    unsigned char macAddr[6];
    unsigned int  flags;
    unsigned int  hashFlagRsvd;
    unsigned int  rsvdField;

} tAniDbgRhpHashTblTestParams, *tpAniDbgRhpHashTblTestParams;


typedef struct sAniDbgRhpHashTblTestResponse
{
    unsigned int rc;  // 0 For Success
} tAniDbgRhpHashTblTestResponse, *tpAniDbgRhpHashTblTestResponse;


// -----------------------------------------------------------

// Here both pParms and pResponse structures are interpreted by the
// User of the API based on testId

typedef struct sANI_DBG_TEST_INFO {

    tANI_TEST_ID testId;

    // This field indicates this test runs synchronously or not.
    // If it is not, then, the test originator will have the
    // ability to query for intermediate results.
    // ANI_START = 1; ANI_GET_RESULTS = 2; ANI_STOP = 3
    tANI_TEST_TRIG_TYPE testTriggerType;

    // test state
    tANI_DBG_TEST_STATE testState;

    // This points to a structure which contains parameters for
    // test defined by dvtTestId. May be NULL if no parameters
    // are needed
    unsigned long sizeOfParms;
    unsigned char parms[MAX_PARMS_SIZE];

    // This points to a buffer to hold response from the test
    // Response shall be there from the test and it MUST have
    // return code ZERO for SUCCESS and error code for test
    // failure
    unsigned long sizeOfResponse;
    unsigned char response[MAX_RESPONSE_SIZE];

} tANI_DBG_TEST_INFO;

// Returns test start function pointer or stop function pointer or
// get info on test function pointer from the function pointer array
// that is initialized during the dvtInitGlobal routine.
#ifdef __cplusplus
extern "C" void* dvtGetFuncPtr(void *, int, int );
#else
extern void* dvtGetFuncPtr(void *, int, int );
#endif


// declare a function prototype for 'start', 'update' and 'stop' routines
typedef void t_DbgTestRoutine(void *);

// called by the test routine when it completes
extern void dbgTestCompleted(void *mpAdapterPtr);


// DBG/DVT dump information structures & defines

#define ANI_DBG_GRP_INFO_TYPE_MISC      0x00000001
#define ANI_DBG_GRP_INFO_TYPE_BMU       0x00000002
#define ANI_DBG_GRP_INFO_TYPE_TFP       0x00000004
#define ANI_DBG_GRP_INFO_TYPE_RHP       0x00000008
#define ANI_DBG_GRP_INFO_TYPE_RFP       0x00000010
#define ANI_DBG_GRP_INFO_TYPE_STA       0x00000020
#define ANI_DBG_GRP_INFO_TYPE_FPHY      0x00000040
#define ANI_DBG_GRP_INFO_TYPE_FPHY_FIFO 0x00000080
#define ANI_DBG_GRP_INFO_TYPE_RPHY      0x00000100
#define ANI_DBG_GRP_INFO_TYPE_HCF       0x00000200
#define ANI_DBG_GRP_INFO_TYPE_SP        0x00000400
#define ANI_DBG_GRP_INFO_TYPE_CP        0x00000800


#define ANI_DBG_GRP_INFO_TYPE_ALL       (ANI_DBG_GRP_INFO_TYPE_MISC |   \
                                     ANI_DBG_GRP_INFO_TYPE_BMU |        \
                                     ANI_DBG_GRP_INFO_TYPE_TFP |        \
                                     ANI_DBG_GRP_INFO_TYPE_RHP |        \
                                     ANI_DBG_GRP_INFO_TYPE_RFP |        \
                                     ANI_DBG_GRP_INFO_TYPE_STA |        \
                                     ANI_DBG_GRP_INFO_TYPE_FPHY|        \
                                     ANI_DBG_GRP_INFO_TYPE_FPHY_FIFO |  \
                                     ANI_DBG_GRP_INFO_TYPE_RPHY |       \
                                     ANI_DBG_GRP_INFO_TYPE_HCF |        \
                                     ANI_DBG_GRP_INFO_TYPE_SP)



typedef struct sANI_DBG_MISC_INFO {

    unsigned long sysMode;
    unsigned long sysIntrMask;
    unsigned long intrMask;
    unsigned long phyIntrMask;
    unsigned long intrStatus[32];
    unsigned long phyIntrStatus[16];
    unsigned long eofSofExceptionResets;  
    unsigned long bmuExceptionResets;      
    unsigned long lowPduExceptionResets;  
    unsigned long userTriggeredResets;
    unsigned long logPExceptionResets;

} tANI_DBG_MISC_INFO;


typedef struct sANI_DBG_BMU_INFO {

    unsigned long control;
    unsigned long fp_hptr;
    unsigned long tptr;
    unsigned long pdu;
    unsigned long exception;
    unsigned long exceptionMaster;
    unsigned long dropCount;
    unsigned long workQueue[10][4];

} tANI_DBG_BMU_INFO;


typedef struct sANI_DBG_TFP_INFO {

    unsigned long control;
    unsigned long modeEnable;
    unsigned long templEnable;
    unsigned long retryQid;
    unsigned long tsfHi;
    unsigned long tsfLo;
    unsigned long beacon;
    unsigned long probeDelay;
    unsigned long tbttHi;
    unsigned long tbttLo;
    unsigned long nav;
    unsigned long listenInterval;
    unsigned long delayTx;
    unsigned long dtimPeriod;
    unsigned long rtsCount;
    unsigned long rtsFailure;

} tANI_DBG_TFP_INFO;


typedef struct sANI_DBG_RHP_INFO {

    unsigned long sof;
    unsigned long sof_chunk;
    unsigned long fragCount;
    unsigned long dropCount;
    unsigned long fcsCount;
    unsigned long bssIdMismatch;
    unsigned long destMismatch;
    unsigned long lengthError;
    unsigned long pduError;
    unsigned long abortCount;
    unsigned long reqRate;
    unsigned long delayAB;
    unsigned long macAddrHi;
    unsigned long macAddrLo;
    unsigned long bssIdHi;
    unsigned long bssIdLo;
    unsigned long relayCount;
    unsigned long hash_MissCount;
    unsigned long hash_srcHi;
    unsigned long hash_srcLo;
    unsigned long hash_type;
    unsigned long hash_subType;
    unsigned long dbg_hangStatus;
    unsigned long dbg_fragIgnoreCount;
    unsigned long pduCount;

} tANI_DBG_RHP_INFO;


typedef struct sANI_DBG_RFP_INFO {

    unsigned long packets;
    unsigned long multicastPackets;
    unsigned long dupPackets;
    unsigned long byteCount;
    unsigned long dropCount;
    unsigned long byte64;
    unsigned long byte128;
    unsigned long byte256;
    unsigned long byte512;
    unsigned long byte1024;
    unsigned long byte1519;
    unsigned long byte2048;
    unsigned long byte4096;

} tANI_DBG_RFP_INFO;

typedef struct sANI_DBG_SP_INFO {

    unsigned long wep_dky0_w0;
    unsigned long wep_dky0_w1;
    unsigned long wep_default_rc0;

    unsigned long wep_dky1_w0;
    unsigned long wep_dky1_w1;
    unsigned long wep_default_rc1;

    unsigned long wep_dky2_w0;
    unsigned long wep_dky2_w1;
    unsigned long wep_default_rc2;

    unsigned long wep_dky3_w0;
    unsigned long wep_dky3_w1;
    unsigned long wep_default_rc3;

} tANI_DBG_SP_INFO;

typedef struct sANI_DBG_CP_INFO {

    unsigned long cp_control;
    unsigned long Compression_Expansion_Cnt;
    unsigned long Compression_NUM_pkts;

    unsigned long Decompression_NUM_pkts;
    unsigned long Compression_50p_NUM_pkts;
    unsigned long CP_Error_status;

    unsigned long Cp_maximum_pkt_len;
 
} tANI_DBG_CP_INFO;


typedef struct sANI_DBG_STA_TX_WQ_INFO {

    unsigned long txWqAddr;
    unsigned long txWqDump[4];

    unsigned long tptr;
    unsigned long hptr;
    unsigned long aptr;
    unsigned long a_tpkts;
    unsigned long h_tpkts;
    unsigned long frag;
    unsigned long bytes;
    unsigned long ack;
    unsigned long valid;

} tANI_DBG_STA_TX_WQ_INFO;



typedef struct sANI_DBG_TC_DESC {

    unsigned long valid;
    unsigned long rxAckType;
    unsigned long newPkt;
    unsigned long rxSeqNum;
    unsigned long rxPktTimeStamp;
    unsigned long SV;
    unsigned long ackTimeout;
    unsigned long numOfFragsSucessful;
    unsigned long rxBDPtr;
    unsigned long txReplayCountHi;
    unsigned long txReplayCountLo;
    unsigned long rxReplayCountHi;
    unsigned long rxReplayCountLo;

} tANI_DBG_TC_DESC;


typedef struct sANI_DBG_PWR_TEMPL {

    unsigned long retryPhyMode;
    unsigned long retryCb;
    unsigned long retryEsf;
    unsigned long sb;
    unsigned long rate;
    unsigned long esf;
    unsigned long tifs;
    unsigned long edcf;
    unsigned long cb;
    unsigned long mode;
    unsigned long pwrLvl;
    unsigned long nTransmitters;
    unsigned long retry1rate;
    unsigned long retry2rate;

    unsigned long pwrTemplate; //entire value

} tANI_DBG_PWR_TEMPL;


typedef struct sANI_DBG_STA {

    unsigned long staDescAddr;
    unsigned long staDump[256];

    tANI_DBG_TC_DESC tcDesc[9]; // 8 tc Ids and 9th mgmt TC

    unsigned long cbits_hcf;
    unsigned long cbits_ps;
    unsigned long cbits_ps1;
    unsigned long cbits_tx_en;

    unsigned long descStat_aes_sent;
    unsigned long descStat_aes_recv;
    unsigned long descStat_replays;
    unsigned long descStat_formaterr;
    unsigned long descStat_aes_decypterr_default;
    unsigned long descStat_aes_decypterr_ucast;

    unsigned long tfpStat_failed;
    unsigned long tfpStat_retry;
    unsigned long tfpStat_multiretry;
    unsigned long tfpStat_ackto;
    unsigned long tfpStat_frags;
    unsigned long tfpStat_rtsBrqs;
    unsigned long tfpStat_pkts;
    unsigned long tfpStat_ctsBackTimeouts;


    unsigned long phyStatHi;
    unsigned long phyStatLo;

    unsigned long ackToNonPrimRates;
    unsigned long nFragSuccNonPrimRates;

} tANI_DBG_STA;


typedef struct sANI_DBG_STA_INFO {

    unsigned long staId;    // input

    tANI_DBG_STA sta;

    tANI_DBG_STA_TX_WQ_INFO txwq[8];

    tANI_DBG_PWR_TEMPL pwrTempl;

} tANI_DBG_STA_INFO;


typedef struct sANI_DBG_FPHY_INFO {

    unsigned long fphy_symPer;
    unsigned long cca_delayOffset;
    unsigned long cca_startDelay;
    unsigned long timeStamp_Hi;
    unsigned long timeStamp_Lo;
    unsigned long dropRate;
    unsigned long burstSize;
    unsigned long reg;
    unsigned long stat_bytes01;
    unsigned long stat_bytes23;
    unsigned long stat_bytes45;
    unsigned long pkts_tx;
    unsigned long pkts_rx;
    unsigned long pkts_drops;
    unsigned long rxin_sof;
    unsigned long rxin_eof;
    unsigned long rxout_sof;
    unsigned long rxout_eof;
    unsigned long txin_sof;
    unsigned long txin_eof;
    unsigned long txout_sof;
    unsigned long txout_eof;

} tANI_DBG_FPHY_INFO;


#define MAX_FIFO_ENTRIES_PER_REQUEST 200

typedef struct sAniDvtPhyfEntry
{
    unsigned char bytes[50];
    unsigned char len;
    unsigned char mode;
    unsigned char phyLen;
    unsigned char macLen;
    unsigned char dropByte;
    unsigned char reserved;
    unsigned long sof;
    unsigned long eof;
} tAniDvtPhyfEntry;


#define ANI_DBG_FIFO_CMD_GET    0x1
#define ANI_DBG_FIFO_CMD_CLEAR  0x2


typedef struct sANI_DBG_FPHY_FIFO {

    unsigned long command; // set to either 'get' of 'clear' fifo
    unsigned long entries; // entries to read
    unsigned long offset; // offset to read from

    unsigned long totalBytes;
    unsigned long approxEntries;

    unsigned long validEntries;
    tAniDvtPhyfEntry Fifo[MAX_FIFO_ENTRIES_PER_REQUEST];

} tANI_DBG_FPHY_FIFO;


typedef struct sANI_DBG_RPHY_MPI_INFO {

    unsigned long tfp_phy_sof;
    unsigned long tfp_phy_eof;
    unsigned long phy_tfp_req;
    unsigned long txa_mpi_data_req;
    unsigned long txb_mpi_data_req;
    unsigned long mpi_txa_data_val;
    unsigned long mpi_txb_data_val;
    unsigned long mpi_txa_pktend;
    unsigned long mpi_txb_pktend;
    unsigned long mpi_txctl_pktend;
    unsigned long mpi_txctl_ctlbytes_val;

} tANI_DBG_RPHY_MPI_INFO;


typedef struct sANI_DBG_RPHY_PMI_INFO {

    unsigned long rxa_mpi_pktstart;
    unsigned long rxb_mpi_pktstart;
    unsigned long rxa_mpi_pktend;
    unsigned long rxb_mpi_pktend;
    unsigned long rxa_mpi_data_val;
    unsigned long rxb_mpi_data_val;
    unsigned long rhp_phy_shutoff;
    unsigned long rhp_phy_sof_c;
    unsigned long rhp_phy_sof_p;
    unsigned long rhp_phy_eof_c;
    unsigned long rhp_phy_eof_p;
    unsigned long phy_rhp_data_val;
    unsigned long pmi_int;

} tANI_DBG_RPHY_PMI_INFO;

typedef struct sANI_DBG_RPHY_PHYINT_INFO {

    unsigned long status;
    unsigned long fast_mask;
    unsigned long slow_mask;
    unsigned long host_mask;

} tANI_DBG_RPHY_PHYINT_INFO;


typedef struct sANI_DBG_RPHY_INFO {

    tANI_DBG_RPHY_MPI_INFO mpi;
    tANI_DBG_RPHY_PMI_INFO pmi;
    tANI_DBG_RPHY_PHYINT_INFO phyint;

} tANI_DBG_RPHY_INFO;


typedef struct sANI_DBG_SCH_INFO
{
    unsigned long curSch; // RO
    unsigned long numSch; // RO
    unsigned long numInt; // RO
    unsigned long numEndInt; // RO
    unsigned long numCFB; // RO
    unsigned long firstCFB; // RO

    unsigned long fixedSch; // RW
    unsigned long gDvtPoll; // RW
    unsigned long maxTimeout; // RW
    unsigned long minTxop; // RW
    unsigned long maxTxop; // RW
    unsigned long maxTcid; // RW
    unsigned long maxSta; // RW
    unsigned long minSta; // RW
    unsigned long maxInst; // RW

    unsigned long firstSch; // RO
    unsigned long cfbStart; // RO
    unsigned long cfbEnd; // RO
    unsigned long cumCFB; // RO
    unsigned long cumCP; // RO

    unsigned long haltSch; // RW
    unsigned long numTim; // RO
} tANI_DBG_SCH_INFO;


typedef struct sANI_DBG_HCF_INFO {

    unsigned long       bSetInfo;     // 0 = read info, 1 = set info
    tANI_DBG_SCH_INFO   schInfo;

} tANI_DBG_HCF_INFO;

typedef struct sANI_DBG_AP_SWITCH_INFO {


    // Space to hold the SSIDList and the BSSID.
    unsigned char   SSID_BSSID_BUF[262]; // 262 == WNIAPI_MAX_SSID_LIST_STR + WNIAPI_BSSID_SIZE


} tANI_DBG_AP_SWITCH_INFO;


typedef struct sANI_DBG_INFO {

    unsigned long dbgInfoMask; // indicates which members are valid

    tANI_DBG_MISC_INFO miscInfo;
    tANI_DBG_BMU_INFO bmuInfo;
    tANI_DBG_TFP_INFO tfpInfo;
    tANI_DBG_RHP_INFO rhpInfo;
    tANI_DBG_RFP_INFO rfpInfo;
    tANI_DBG_STA_INFO staInfo;
    tANI_DBG_FPHY_INFO fphyInfo;
    tANI_DBG_FPHY_FIFO fphyFifo;
    tANI_DBG_RPHY_INFO rphyInfo;
    tANI_DBG_HCF_INFO hcfInfo;
    tANI_DBG_SP_INFO spInfo;
    tANI_DBG_CP_INFO cpInfo;

} tANI_DBG_INFO;

extern tSirRetStatus dvtGetDumpInfo(void *pMac, tANI_DBG_INFO *pDbgInfo );

extern tSirRetStatus dvtGetConfigInfo(void *pMac, tANI_DBG_TEST_INFO *pTestInfo );

#ifdef __cplusplus
extern "C" void dvtSetStopTestFlag( unsigned char stopTest );
#else
extern void dvtSetStopTestFlag( unsigned char stopTest );
#endif


#endif // _ANIDBGTEST_H_
