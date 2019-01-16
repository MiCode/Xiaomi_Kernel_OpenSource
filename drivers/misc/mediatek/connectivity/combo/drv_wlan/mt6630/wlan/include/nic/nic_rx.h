/*
** $Id: //Department/DaVinci/BRANCHES/MT6620_WIFI_DRIVER_V2_3/include/nic/nic_rx.h#1 $
*/

/*! \file   "nic_rx.h"
    \brief  The declaration of the nic rx functions

*/



/*
** $Log: nic_rx.h $
**
** 07 30 2013 tsaiyuan.hsu
** [BORA00002222] MT6630 unified MAC RXM
** add defragmentation.
**
** 07 23 2013 wh.su
** [BORA00002446] [MT6630] [Wi-Fi] [Driver] Update the security function code
** Sync the latest jb2.mp 11w code as draft version
** Not the CM bit for avoid wapi 1x drop at re-key
**
** 04 30 2013 tsaiyuan.hsu
** [BORA00002222] MT6630 unified MAC RXM
** change the definition of rx descriptor, AMP, ASF and Data, to align spec.
**
** 03 20 2013 tsaiyuan.hsu
** [BORA00002222] MT6630 unified MAC RXM
** add rx duplicate check.
**
** 03 12 2013 tsaiyuan.hsu
** [BORA00002222] MT6630 unified MAC RXM
** remove hif_rx_hdr usage.
**
** 03 12 2013 tsaiyuan.hsu
** [BORA00002222] MT6630 unified MAC RXM
** add rx data and mangement processing.
**
** 03 07 2013 tsaiyuan.hsu
** [BORA00002222] MT6630 unified MAC RXM
** use rx_status to locate packet type instead of hif_rx_header.
**
** 02 01 2013 cp.wu
** [BORA00002227] [MT6630 Wi-Fi][Driver] Update for Makefile and HIFSYS modifications
** 1. eliminate MT5931/MT6620/MT6628 logic
** 2. add firmware download control sequence
**
** 01 09 2013 tsaiyuan.hsu
** [BORA00002222] MT6630 unified MAC RXM
** add hw_mac_rx datatype and macro.
**
** 09 17 2012 cm.chang
** [BORA00002149] [MT6630 Wi-Fi] Initial software development
** Duplicate source from MT6620 v2.3 driver branch
** (Davinci label: MT6620_WIFI_Driver_V2_3_120913_1942_As_MT6630_Base)
 *
 * 11 07 2011 tsaiyuan.hsu
 * [WCXRP00001083] [MT6620 Wi-Fi][DRV]] dump debug counter or frames when debugging is triggered
 * add debug counters and periodically dump counters for debugging.
 *
 * 05 05 2011 cp.wu
 * [WCXRP00000702] [MT5931][Driver] Modify initialization sequence for E1 ASIC
 * add delay after whole-chip resetting for MT5931 E1 ASIC.
 *
 * 04 18 2011 terry.wu
 * [WCXRP00000660] [MT6620 Wi-Fi][Driver] Remove flag CFG_WIFI_DIRECT_MOVED
 * Remove flag CFG_WIFI_DIRECT_MOVED.
 *
 * 01 24 2011 cm.chang
 * [WCXRP00000384] [MT6620 Wi-Fi][Driver][FW] Handle 20/40 action frame in AP mode and stop ampdu timer when sta_rec is freed
 * Process received 20/40 coexistence action frame for AP mode
 *
 * 09 08 2010 cp.wu
 * NULL
 * use static memory pool for storing IEs of scanning result.
 *
 * 09 07 2010 yuche.tsai
 * NULL
 * Change prototype of API of adding P2P device to scan result.
 * Additional IE buffer is saved.
 *
 * 09 03 2010 kevin.huang
 * NULL
 * Refine #include sequence and solve recursive/nested #include issue
 *
 * 08 05 2010 yuche.tsai
 * NULL
 * Modify data structure for P2P Scan result.
 *
 * 08 03 2010 cp.wu
 * NULL
 * newly added P2P API should be declared in header file.
 *
 * 07 30 2010 cp.wu
 * NULL
 * 1) BoW wrapper: use definitions instead of hard-coded constant for error code
 * 2) AIS-FSM: eliminate use of desired RF parameters, use prTargetBssDesc instead
 * 3) add handling for RX_PKT_DESTINATION_HOST_WITH_FORWARD for GO-broadcast frames
 *
 * 07 08 2010 cp.wu
 *
 * [WPD00003833] [MT6620 and MT5931] Driver migration - move to new repository.
 *
 * 06 14 2010 cp.wu
 * [WPD00003833][MT6620 and MT5931] Driver migration
 * saa_fsm.c is migrated.
 *
 * 06 14 2010 cp.wu
 * [WPD00003833][MT6620 and MT5931] Driver migration
 * add management dispatching function table.
 *
 * 06 11 2010 cp.wu
 * [WPD00003833][MT6620 and MT5931] Driver migration
 * 1) migrate assoc.c.
 * 2) add ucTxSeqNum for tracking frames which needs TX-DONE awareness
 * 3) add configuration options for CNM_MEM and RSN modules
 * 4) add data path for management frames
 * 5) eliminate rPacketInfo of MSDU_INFO_T
 *
 * 06 06 2010 kevin.huang
 * [WPD00003832][MT6620 5931] Create driver base
 * [MT6620 5931] Create driver base
 *
 * 03 30 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * remove driver-land statistics.
 *
 * 03 24 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * generate information for OID_GEN_RCV_OK & OID_GEN_XMIT_OK
 *  *
 *
 * 03 11 2010 cp.wu
 * [WPD00003821][BUG] Host driver stops processing RX packets from HIF RX0
 * add RX starvation warning debug message controlled by CFG_HIF_RX_STARVATION_WARNING
 *
 * 03 10 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * code clean: removing unused variables and structure definitions
 *
 * 02 25 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * correct behavior to prevent duplicated RX handling for RX0_DONE and RX1_DONE
 *
 * 02 10 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * implement host-side firmware download logic
 *
 * 02 10 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * 1) remove unused function in nic_rx.c [which has been handled in que_mgt.c]
 *  * 2) firmware image length is now retrieved via NdisFileOpen
 *  * 3) firmware image is not structured by (P_IMG_SEC_HDR_T) anymore
 *  * 4) nicRxWaitResponse() revised
 *  * 5) another set of TQ counter default value is added for fw-download state
 *  * 6) Wi-Fi load address is now retrieved from registry too
 *
 * 12 30 2009 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * 1) According to CMD/EVENT documentation v0.8,
 *  *  *  * OID_CUSTOM_TEST_RX_STATUS & OID_CUSTOM_TEST_TX_STATUS is no longer used,
 *  *  *  * and result is retrieved by get ATInfo instead
 *  *  *  * 2) add 4 counter for recording aggregation statistics
**  \main\maintrunk.MT6620WiFiDriver_Prj\24 2009-12-10 16:49:09 GMT mtk02752
**  code clean
**  \main\maintrunk.MT6620WiFiDriver_Prj\23 2009-12-09 14:02:37 GMT MTK02468
**  Added ucStaRecIdx in SW_RFB_T and HALF_SEQ_NO_COUNT definition (to replace HALF_SEQ_NO_CNOUT)
**  \main\maintrunk.MT6620WiFiDriver_Prj\22 2009-11-27 11:07:54 GMT mtk02752
**  add flush for reset
**  \main\maintrunk.MT6620WiFiDriver_Prj\21 2009-11-25 18:18:09 GMT mtk02752
**  modify nicRxAddScanResult()
**  \main\maintrunk.MT6620WiFiDriver_Prj\20 2009-11-24 22:42:22 GMT mtk02752
**  add nicRxAddScanResult() to prepare to handle SCAN_RESULT event
**  \main\maintrunk.MT6620WiFiDriver_Prj\19 2009-11-24 19:57:06 GMT mtk02752
**  adopt P_HIF_RX_HEADER_T
**  \main\maintrunk.MT6620WiFiDriver_Prj\18 2009-11-16 21:43:04 GMT mtk02752
**  correct ENUM_RX_PKT_DESTINATION_T definitions
**  \main\maintrunk.MT6620WiFiDriver_Prj\17 2009-11-16 15:28:25 GMT mtk02752
**  add ucQueuedPacketNum for indicating how many packet are queued by RX reordering buffer/forwarding path
**  \main\maintrunk.MT6620WiFiDriver_Prj\16 2009-11-16 15:05:01 GMT mtk02752
**  add eTC for SW_RFB_T and structure RX_MAILBOX
**  \main\maintrunk.MT6620WiFiDriver_Prj\15 2009-11-13 21:16:57 GMT mtk02752
**  \main\maintrunk.MT6620WiFiDriver_Prj\14 2009-11-13 16:59:30 GMT mtk02752
**  add handler for event packet
**  \main\maintrunk.MT6620WiFiDriver_Prj\13 2009-11-13 13:45:50 GMT mtk02752
**  add port param for nicRxEnhanceReadBuffer()
**  \main\maintrunk.MT6620WiFiDriver_Prj\12 2009-11-11 10:12:31 GMT mtk02752
**  nicSDIOReadIntStatus() always read sizeof(ENHANCE_MODE_DATA_STRUCT_T) for int response, thus the number should be set to 0(:=16) instead of 10
**  \main\maintrunk.MT6620WiFiDriver_Prj\11 2009-10-29 19:53:32 GMT mtk01084
**  modify structure naming
**  \main\maintrunk.MT6620WiFiDriver_Prj\10 2009-10-23 16:08:23 GMT mtk01084
**  \main\maintrunk.MT6620WiFiDriver_Prj\9 2009-10-13 21:59:01 GMT mtk01084
**  update for new HW architecture design
**  \main\maintrunk.MT6620WiFiDriver_Prj\8 2009-05-20 12:23:33 GMT mtk01461
**  Add u4MaxEventBufferLen parameter to nicRxWaitResponse()
**  \main\maintrunk.MT6620WiFiDriver_Prj\7 2009-05-18 21:00:48 GMT mtk01426
**  Update SDIO_MAXIMUM_RX_STATUS value
**  \main\maintrunk.MT6620WiFiDriver_Prj\6 2009-04-28 10:36:15 GMT mtk01461
**  Remove unused define - SDIO_MAXIMUM_TX_STATUS
**  \main\maintrunk.MT6620WiFiDriver_Prj\5 2009-04-01 10:53:17 GMT mtk01461
**  Add function for HIF_LOOPBACK_PRE_TEST
**  \main\maintrunk.MT6620WiFiDriver_Prj\4 2009-03-18 20:56:19 GMT mtk01426
**  Add to support CFG_HIF_LOOPBACK and CFG_SDIO_RX_ENHANCE
**  \main\maintrunk.MT6620WiFiDriver_Prj\3 2009-03-17 20:19:56 GMT mtk01426
**  Add nicRxWaitResponse function proto type
**  \main\maintrunk.MT6620WiFiDriver_Prj\2 2009-03-10 20:16:35 GMT mtk01426
**  Init for develop
**
*/

#ifndef _NIC_RX_H
#define _NIC_RX_H

/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/

/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/
#define MAX_SEQ_NO                  4095
#define MAX_SEQ_NO_COUNT            4096
#define HALF_SEQ_NO_CNOUT           2048

#define HALF_SEQ_NO_COUNT           2048

#define MT6620_FIXED_WIN_SIZE         64
#define CFG_RX_MAX_BA_ENTRY            4
#define CFG_RX_MAX_BA_TID_NUM          8

#define RX_STATUS_FLAG_MORE_PACKET    BIT(30)
#define RX_STATUS_CHKSUM_MASK         BITS(0, 10)

#define RX_RFB_LEN_FIELD_LEN        4
#define RX_HEADER_OFFSET            2

#define RX_RETURN_INDICATED_RFB_TIMEOUT_SEC     3


#if defined(_HIF_SDIO) && defined(WINDOWS_DDK)
/*! On XP, maximum Tx+Rx Statue <= 64-4(HISR)*/
#define SDIO_MAXIMUM_RX_LEN_NUM              0	/*!< 0~15 (0: un-limited) */
#else
#define SDIO_MAXIMUM_RX_LEN_NUM              0	/*!< 0~15 (0: un-limited) */
#endif

/* RXM Definitions */
/* The payload format of a RX packet */
#define RX_PAYLOAD_FORMAT_MSDU                  0
#define RX_PAYLOAD_FORMAT_FIRST_SUB_AMSDU       3
#define RX_PAYLOAD_FORMAT_MIDDLE_SUB_AMSDU      2
#define RX_PAYLOAD_FORMAT_LAST_SUB_AMSDU        1

/* HAL RX from hal_hw_def_rom.h */
/*------------------------------------------------------------------------------
 * Cipher define
 *------------------------------------------------------------------------------
*/
#define CIPHER_SUITE_NONE               0
#define CIPHER_SUITE_WEP40              1
#define CIPHER_SUITE_TKIP               2
#define CIPHER_SUITE_TKIP_WO_MIC        3
#define CIPHER_SUITE_CCMP               4
#define CIPHER_SUITE_WEP104             5
#define CIPHER_SUITE_BIP                6
#define CIPHER_SUITE_WEP128             7
#define CIPHER_SUITE_WPI                8
#define CIPHER_SUITE_CCMP_W_CCX         9
#define CIPHER_SUITE_GCMP               10

/*------------------------------------------------------------------------------
 * Bit fields for HW_MAC_RX_DESC_T
 *------------------------------------------------------------------------------
*/

/*! MAC RX DMA Descriptor */
/* DW 0*/
/* Word 0 */
#define RX_STATUS_RX_BYTE_COUNT_MASK    BITS(0, 16)
/* Word 1 */
#define RX_STATUS_ETH_TYPE_OFFSET_MASK  BITS(0, 6)
#define RX_STATUS_ETH_TYPE_OFFSET       0
#define RX_STATUS_IP_CHKSUM             BIT(7)
#define RX_STATUS_UDP_TCP_CHKSUM        BIT(8)
#define RX_STATUS_GROUP_VLD_MASK        BITS(9, 12)
#define RX_STATUS_GROUP_VLD_OFFSET      9
#define RX_STATUS_PKT_TYPE_MASK         BITS(13, 15)
#define RX_STATUS_PKT_TYPE_OFFSET       13

/* DW 1 */
/* Byte 0 */
#define RX_STATUS_HTC                   BIT(0)
#define RX_STATUS_UC2ME                 BIT(1)
#define RX_STATUS_MC_FRAME              BIT(2)
#define RX_STATUS_BC_FRAME              BIT(3)
#define RX_STATUS_BCN_WITH_BMC          BIT(4)
#define RX_STATUS_BCN_WITH_UC           BIT(5)
#define RX_STATUS_KEYID_MASK            BITS(6, 7)
#define RX_STATUS_KEYID_OFFSET          6

/* Byte 1 */
#define RX_STATUS_CHAN_FREQ_MASK        BITS(0, 7)
/* Byte 2 */
#define RX_STATUS_HEADER_LEN_MASK       BITS(0, 5)
#define RX_STATUS_HEADER_OFFSET         BIT(6)
#define RX_STATUS_HEADER_TRAN           BIT(7)
/* Byte 3 */
#define RX_STATUS_PAYLOAD_FORMAT_MASK   BITS(0, 1)
#define RX_STATUS_PAYLOAD_FORMAT_OFFSET 0
#define RX_STATUS_BSSID_MASK            BITS(2, 7)
#define RX_STATUS_BSSID_OFFSET          2

/* DW 2 */
/* Byte 1 */
#define RX_STATUS_TID_MASK              BITS(0, 3)
#define RX_STATUS_SEC_MASK              BITS(4, 7)
#define RX_STATUS_SEC_OFFSET            4
/* Byte 2-3 */
#define RX_STATUS_SW_BIT                BIT(0)
#define RX_STATUS_FLAG_FCS_ERROR        BIT(1)
#define RX_STATUS_FLAG_CIPHER_MISMATCH  BIT(2)
#define RX_STATUS_FLAG_CIPHER_LENGTH_MISMATCH     BIT(3)
#define RX_STATUS_FLAG_ICV_ERROR        BIT(4)
#define RX_STATUS_FLAG_TKIPMIC_ERROR    BIT(5)
#define RX_STATUS_FLAG_LEN_MISMATCH     BIT(6)
#define RX_STATUS_FLAG_DE_AMSDU_FAIL    BIT(7)
#define RX_STATUS_FLAG_EXCEED_LEN       BIT(8)
#define RX_STATUS_LLC_MIS               BIT(9)
#define RX_STATUS_UDF_VLT               BIT(10)
#define RX_STATUS_FRAG                  BIT(11)
#define RX_STATUS_NULL                  BIT(12)
#define RX_STATUS_DATA                  BIT(13)
#define RX_STATUS_AMPDU_SUB_FRAME       BIT(14)
#define RX_STATUS_AMPDU_FORMAT          BIT(15)
#define PAYLOAD_FORMAT_IS_MSDU_FRAME    0
#define RX_STATUS_FLAG_ERROR_MASK      (RX_STATUS_FLAG_FCS_ERROR | RX_STATUS_FLAG_ICV_ERROR | \
					RX_STATUS_FLAG_CIPHER_LENGTH_MISMATCH)	/* No TKIP MIC error */

/* DW 3 */
#define RX_STATUS_RXV_SEQ_NO_MASK       BITS(0, 7)
#define RX_STATUS_TCL                   BIT(8)
#define RX_STATUS_CLS                   BIT(11)
#define RX_STATUS_OFLD_MASK             BITS(12, 13)
#define RX_STATUS_OFLD_OFFSET           12
#define RX_STATUS_EAPOL_PACKET          BIT(12)
#define RX_STATUS_ARP_NS_PACKET         BIT(13)
#define RX_STATUS_TDLS_PACKET           BITS(12, 13)
#define RX_STATUS_MGC                   BIT(14)
#define RX_STATUS_WOL_MASK              BITS(15, 19)
#define RX_STATUS_WOL_OFFSET            15
#define RX_STATUS_CLS_BITMAP_MASK       BITS(20, 29)
#define RX_STATUS_CLS_BITMAP_OFFSET     20
#define RX_STATUS_PF_MODE_BLACK_LIST    BIT(30)
#define RX_STATUS_PF_STS_CHECKED        BIT(31)


/* DW 12 */
#define RX_STATUS_FRAG_NUM_MASK         BITS(0, 3)
#define RX_STATUS_SEQ_NUM_MASK          BITS(4, 15)
#define RX_STATUS_SEQ_NUM_OFFSET        4


#define RX_STATUS_GROUP1_VALID    BIT(0)
#define RX_STATUS_GROUP2_VALID    BIT(1)
#define RX_STATUS_GROUP3_VALID    BIT(2)
#define RX_STATUS_GROUP4_VALID    BIT(3)

#define RX_STATUS_FIXED_LEN       16

#define RX_STATUS_CHAN_FREQ_MASK_FOR_BY_PASS_MPDE      BITS(0, 7)
#define RX_STATUS_FLAG_FCS_ERROR_FOR_BY_PASS_MODE      BIT(16)


/* Timing Measurement Report */
/* DW0 Word 1 */
#define RX_TMR_TOA_VALID          BIT(11)
#define RX_TMR_TOD_VALID          BIT(10)
#define RX_TMR_TYPE_MASK          BITS(8, 9)
#define RX_TMR_TYPE_OFFSET        8
#define RX_TMR_SUBTYPE_MASK       BITS(4, 7)
#define RX_TMR_SUBTYPE_OFFSET     4

/* DW0 Byte 1*/
#define RX_TMR_TM_FAILED          BIT(2)
#define RX_TMR_NOISY_CHAN         BIT(1)
#define RX_TMR_RESPONDER          BIT(0)



/* TBD */
#define DMA_OWN_TO_HW          BIT(0)
#define DMA_OWN_TO_FW_PENDING  BIT(1)
#define STATUS_IS_OWN_TO_FW(flag)   (((flag) & DMA_OWN_TO_HW) ? FALSE : TRUE)
#define STATUS_IS_FW_PENDING(flag)  (((flag) & DMA_OWN_TO_FW_PENDING) ? TRUE : FALSE)

/* DW 2 */
#define RX_STATUS_PACKET_LENGTH_MASK    BITS(0, 16)


#define RX_STATUS_HEADER_TRAN_MASK          BIT(7)
#define RX_STATUS_HEADER_TRAN_OFFSET        7
#define RX_STATUS_HEADER_TRAN_BSS0_MASK     BIT(6)
#define RX_STATUS_HEADER_TRAN_BSS0_OFFSET   6
#define RX_STATUS_HEADER_TRAN_BSS1_MASK     BIT(7)
#define RX_STATUS_HEADER_TRAN_BSS1_OFFSET   7


/* DW 4 */
#define RX_STATUS_MATCH_PACKET        BIT(4)

#define RX_STATUS_HEADER_OFFSET_MASK  0xC0
#define RX_STATUS_HEADER_OFFSET_OFFSET  6

/*------------------------------------------------------------------------------
 * Bit fields for HW_RX_VECTOR_DESC_T
 *------------------------------------------------------------------------------
*/
/* DW 2 */
#define RX_VECTOR_FOR_BA_ACK        BIT(7)


/*! HIF RX DMA Descriptor */
/* DW 2 */
#define HIF_RX_DESC_BUFFER_LEN                  BITS(0, 15)
#define HIF_RX_DESC_ETHER_TYPE_OFFSET_MASK      BITS(16, 23)
#define HIF_RX_DESC_ETHER_TYPE_OFFSET_OFFSET    16
#define HIF_RX_DESC_IP_CHKSUM_CHECK             BIT(24)
#define HIF_RX_DESC_TCP_UDP_CHKSUM_CHECK        BIT(25)

#define HIF_RX_DATA_QUEUE       0
#define HIF_RX_EVENT_QUEUE      1

/*------------------------------------------------------------------------------
 * Bit fields for PHY Vector
 *------------------------------------------------------------------------------
*/

/* RX Vector, 1st Cycle */
#define RX_VT_RX_RATE_AC_MASK      BITS(0,3)
#define RX_VT_RX_RATE_MASK         BITS(0,6)
#define RX_VT_RX_RATE_OFFSET       0
#define RX_VT_STBC_MASK            BITS(7,8)
#define RX_VT_STBC_OFFSET          7
#define RX_VT_LDPC                 BIT(9)
#define RX_VT_NESS_MASK            BITS(10,11)
#define RX_VT_NESS_OFFSET          10
#define RX_VT_RX_MODE_MASK         BITS(12,14)
#define RX_VT_RX_MODE_OFFSET       12
#define RX_VT_RX_MODE_VHT          BIT(14)
#define RX_VT_FR_MODE_MASK         BITS(15,16)
#define RX_VT_FR_MODE_OFFSET       15
#define RX_VT_TXOP_PS_NOT_ALLOWED  BIT(17)
#define RX_VT_AGGREGATION          BIT(18)
#define RX_VT_SHORT_GI             BIT(19)
#define RX_VT_SMOOTH               BIT(20)
#define RX_VT_NO_SOUNDING          BIT(21)
#define RX_VT_SOUNDING             BIT(21)
#define RX_VT_SHORT_GI_NSYM        BIT(22)
#define RX_VT_CODING_MASK          BITS(23,24)
#define RX_VT_CODING_OFFSET        23
#define RX_VT_BEAMFORMED           BIT(29)
#define RX_VT_GROUPID_0_MASK       BITS(30,31)
#define RX_VT_GROUPID_0_OFFSET     30



#define RX_VT_RX_RATE_1M      0x0
#define RX_VT_RX_RATE_2M      0x1
#define RX_VT_RX_RATE_5M      0x2
#define RX_VT_RX_RATE_11M     0x3
#define RX_VT_RX_RATE_6M      0xB
#define RX_VT_RX_RATE_9M      0xF
#define RX_VT_RX_RATE_12M     0xA
#define RX_VT_RX_RATE_18M     0xE
#define RX_VT_RX_RATE_24M     0x9
#define RX_VT_RX_RATE_36M     0xD
#define RX_VT_RX_RATE_48M     0x8
#define RX_VT_RX_RATE_54M     0xC

#define RX_VT_RX_RATE_MCS0    0
#define RX_VT_RX_RATE_MCS1    1
#define RX_VT_RX_RATE_MCS2    2
#define RX_VT_RX_RATE_MCS3    3
#define RX_VT_RX_RATE_MCS4    4
#define RX_VT_RX_RATE_MCS5    5
#define RX_VT_RX_RATE_MCS6    6
#define RX_VT_RX_RATE_MCS7    7
#define RX_VT_RX_RATE_MCS32   32

#define RX_VT_LEGACY_CCK      0
#define RX_VT_LEGACY_OFDM     1
#define RX_VT_MIXED_MODE      2
#define RX_VT_GREEN_MODE      3
#define RX_VT_VHT_MODE        4


#define RX_VT_LG20_HT20       0
#define RX_VT_DL40_HT40       1
#define RX_VT_U20             2
#define RX_VT_L20             3

#define RX_VT_FR_MODE_20      0
#define RX_VT_FR_MODE_40      1
#define RX_VT_FR_MODE_80      2
#define RX_VT_FR_MODE_160     3


#define RX_VT_CCK_SHORT_PREAMBLE   BIT(2)


/* RX Vector, 2nd Cycle */
#define RX_VT_RX_LEN_HT_MASK       BITS(0,15)
#define RX_VT_RX_LEN_LEACY_MASK    BITS(0,11)
#define RX_VT_RX_LEN_VHT_MASK      BITS(0,20)
#define RX_VT_GROUPID_1_MASK       BITS(21,24)
#define RX_VT_GROUPID_1_OFFSET     21
#define RX_VT_NSTS_MASK            BITS(25,27)
#define RX_VT_NSTS_OFFSET          25
#define RX_VT_AID_0_MASK           BITS(28,31)
#define RX_VT_AID_0_OFFSET         28

/* RX Vector, 3rd Cycle */
#define RX_VT_AID_1_MASK          BITS(0,4)
#define RX_VT_AID_1_OFFSET        0
#define RX_VT_SEL_ANT             BIT(7)
#define RX_VT_RCPI_MASK           BITS(8,15)
#define RX_VT_RCPI_OFFSET         8
#define RX_VT_OFDM_FREQ_TRANS_DET BIT(5)
#define RX_VT_FAGC0_EQ_CAL        BIT(16)

/* RX Vector, 4th Cycle */
#define RX_VT_IB_RSSI_MASK       BITS(0, 7)
#define RX_VT_WB_RSSI_MASK       BITS(8, 15)
#define RX_VT_WB_RSSI_OFFSET    8

/* RX Vector, 6th Cycle */
#define RX_VT_NF0_MASK          BITS(0,7)
#define RX_VT_NF0_OFFSET        0

/* RX Vector Group 2, the 1st cycle */
#define RX_VT_CCK_LQ            BITS(4, 10)
#define RX_VT_OFDM_LQ_BPSK      BITS(4, 9)
#define RX_VT_LQ_OFFSET         4
#define RX_VT_PRIM_ITFR_ENV     BIT(0)
#define RX_VT_SEC_ITFR_ENV      BIT(1)
#define RX_VT_SEC40_ITFR_ENV    BIT(2)


/* RX Vector Group 2, the 2nd cycle */
#define RX_VT_DYNA_BW_IN_NON_HT_DYNA   BIT(29)
#define RX_VT_CH_BW_IN_NON_HT_CBW40    BIT(30)
#define RX_VT_CH_BW_IN_NON_HT_CBW80    BIT(31)
#define RX_VT_CH_BW_IN_NON_HT_CBW160   BITS(30, 31)



/* RX Data Type */
#define RX_DATA_TYPE_RX_VECTOR  0
#define RX_DATA_TYPE_RX_DATA    1
#define RX_DATA_TYPE_RX_EVM     2
#define RX_DATA_TYPE_RX_AMBI    3
#define RX_DATA_TYPE_RX_BT      4


/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/
typedef enum _ENUM_RX_STATISTIC_COUNTER_T {
	RX_MPDU_TOTAL_COUNT = 0,
	RX_SIZE_ERR_DROP_COUNT,

	RX_DATA_INDICATION_COUNT,
	RX_DATA_RETURNED_COUNT,
	RX_DATA_RETAINED_COUNT,

	RX_DROP_TOTAL_COUNT,
	RX_TYPE_ERR_DROP_COUNT,
	RX_CLASS_ERR_DROP_COUNT,
	RX_DST_NULL_DROP_COUNT,

#if CFG_TCP_IP_CHKSUM_OFFLOAD || CFG_TCP_IP_CHKSUM_OFFLOAD_NDIS_60
	RX_CSUM_TCP_FAILED_COUNT,
	RX_CSUM_UDP_FAILED_COUNT,
	RX_CSUM_IP_FAILED_COUNT,
	RX_CSUM_TCP_SUCCESS_COUNT,
	RX_CSUM_UDP_SUCCESS_COUNT,
	RX_CSUM_IP_SUCCESS_COUNT,
	RX_CSUM_UNKNOWN_L4_PKT_COUNT,
	RX_CSUM_UNKNOWN_L3_PKT_COUNT,
	RX_IP_V6_PKT_CCOUNT,
#endif
	RX_STATISTIC_COUNTER_NUM
} ENUM_RX_STATISTIC_COUNTER_T;

typedef enum _ENUM_RX_PKT_DESTINATION_T {
	RX_PKT_DESTINATION_HOST,	/* to OS */
	RX_PKT_DESTINATION_FORWARD,	/* to TX queue for forward, AP mode */
	RX_PKT_DESTINATION_HOST_WITH_FORWARD,	/* to both TX and OS, AP mode broadcast packet */
	RX_PKT_DESTINATION_NULL,	/* packet to be freed */
	RX_PKT_DESTINATION_NUM
} ENUM_RX_PKT_DESTINATION_T;

/* Used for MAC RX */
typedef enum _ENUM_MAC_RX_PKT_TYPE_T {
	RX_PKT_TYPE_TX_STATUS = 0,
	RX_PKT_TYPE_RX_VECTOR,
	RX_PKT_TYPE_RX_DATA,
	RX_PKT_TYPE_DUP_RFB,
	RX_PKT_TYPE_TM_REPORT,
	RX_PKT_TYPE_SW_DEFINED = 7
} ENUM_MAC_RX_PKT_TYPE_T;

typedef enum _ENUM_MAC_RX_GROUP_VLD_T {
	RX_GROUP_VLD_1 = 0,
	RX_GROUP_VLD_2,
	RX_GROUP_VLD_3,
	RX_GROUP_VLD_4,
	RX_GROUP_VLD_NUM
} ENUM_MAC_RX_GROUP_VLD_T;

typedef enum _ENUM_MAC_GI_INFO_T {
	MAC_GI_NORMAL = 0,
	MAC_GI_SHORT
} ENUM_MAC_GI_INFO_T, *P_ENUM_MAC_GI_INFO_T;

#define RXM_RXD_PKT_TYPE_SW_BITMAP 0xE00F
#define RXM_RXD_PKT_TYPE_SW_EVENT  0xE000
#define RXM_RXD_PKT_TYPE_SW_FRAME  0xE001

/* AMPDU data frame with no errors including FC/FM/I/T/LM/DAF/EL/LLC-MIS/ UDFVLT and Class 3 error */
#define RXS_DW2_AMPDU_nERR_BITMAP  0xFFBF
#define RXS_DW2_AMPDU_nERR_VALUE   0x0000
/* no error including FC/FM/I/T/LM/DAF/EL/LLC-MIS/ UDFVLT */
#define RXS_DW2_RX_nERR_BITMAP     0x07F8
#define RXS_DW2_RX_nERR_VALUE      0x0000
/* Non-Data frames */
#define RXS_DW2_RX_nDATA_BITMAP    0x3000
#define RXS_DW2_RX_nDATA_VALUE     0x2000
/* Claas Error */
#define RXS_DW2_RX_CLASSERR_BITMAP 0x0001
#define RXS_DW2_RX_CLASSERR_VALUE  0x0001
/* Fragmentation */
#define RXS_DW2_RX_FRAG_BITMAP     0x3800
#define RXS_DW2_RX_FRAG_VALUE      0x0800

/*******************************************************************************
*                            P U B L I C   D A T A
********************************************************************************
*/

/*! A data structure which is identical with MAC RX DMA Descriptor */
typedef struct _HW_MAC_RX_DESC_T {
	UINT_16 u2RxByteCount;	/* DW 0 */
	UINT_16 u2PktTYpe;
	UINT_8 ucMatchPacket;	/* DW 1 */
	UINT_8 ucChanFreq;
	UINT_8 ucHeaderLen;
	UINT_8 ucBssid;
	UINT_8 ucWlanIdx;	/* DW 2 */
	UINT_8 ucTidSecMode;
	UINT_16 u2StatusFlag;
	UINT_32 u4PatternFilterInfo;	/* DW 3 */
} HW_MAC_RX_DESC_T, *P_HW_MAC_RX_DESC_T;

typedef struct _HW_MAC_RX_STS_GROUP_1_T {
	UINT_8 aucPN[16];
} HW_MAC_RX_STS_GROUP_1_T, *P_HW_MAC_RX_STS_GROUP_1_T;


typedef struct _HW_MAC_RX_STS_GROUP_2_T {
	UINT_32 u4Timestamp;	/* DW 12 */
	UINT_32 u4CRC;		/* DW 13 */
} HW_MAC_RX_STS_GROUP_2_T, *P_HW_MAC_RX_STS_GROUP_2_T;


typedef struct _HW_MAC_RX_STS_GROUP_4_T {
	/* For HDR_TRAN */
	UINT_16 u2FrameCtl;	/* DW 4 */
	UINT_8 aucTA[6];	/* DW 4~5 */
	UINT_16 u2SeqFrag;	/* DW 6 */
	UINT_16 u2Qos;		/* DW 6 */
	UINT_32 u4HTC;		/* DW 7 */
} HW_MAC_RX_STS_GROUP_4_T, *P_HW_MAC_RX_STS_GROUP_4_T;


typedef struct _HW_MAC_RX_STS_GROUP_3_T {
	/*!  RX Vector Info */
	UINT_32 u4RxVector[6];	/* DW 14~19 */
} HW_MAC_RX_STS_GROUP_3_T, *P_HW_MAC_RX_STS_GROUP_3_T;

typedef struct _HW_MAC_RX_TMRI_PKT_FORMAT_T {
	UINT_8 ucPID;
	UINT_8 ucStatus;
	UINT_16 u2PktTYpe;
	UINT_32 u4Reserved[2];
	UINT_32 u4ToA;
	UINT_32 u4ToD;
} HW_MAC_RX_TMRI_PKT_FORMAT_T, *P_HW_MAC_RX_TMRI_PKT_FORMAT_T;

typedef struct _HW_MAC_RX_TMRR_PKT_FORMAT_T {
	UINT_8 ucVtSeq;
	UINT_8 ucStatus;
	UINT_16 u2PktTYpe;
	UINT_8 aucTALow[2];
	UINT_16 u2SnField;
	UINT_8 aucTAHigh[4];
	UINT_32 u4ToA;
	UINT_32 u4ToD;
} HW_MAC_RX_TMRR_PKT_FORMAT_T, *P_HW_MAC_RX_TMRR_PKT_FORMAT_T;

/*! A data structure which is identical with MAC RX Vector DMA Descriptor */
typedef struct _HW_RX_VECTOR_DESC_T {
	UINT_8 aucTA[6];	/* DW 0~1 */
	UINT_8 ucRxVtSeqNo;
	/*!  RX Vector Info */
	UINT_32 u4RxVector[9];	/* DW 2~10 */

} HW_RX_VECTOR_DESC_T, *P_HW_RX_VECTOR_DESC_T;

struct _SW_RFB_T {
	QUE_ENTRY_T rQueEntry;
	PVOID pvPacket;		/*!< ptr to rx Packet Descriptor */
	PUINT_8 pucRecvBuff;	/*!< ptr to receive data buffer */

	/* add fot mt6630 */
	UINT_8 ucGroupVLD;
	UINT_16 u2RxStatusOffst;
	P_HW_MAC_RX_DESC_T prRxStatus;
	P_HW_MAC_RX_STS_GROUP_1_T prRxStatusGroup1;
	P_HW_MAC_RX_STS_GROUP_2_T prRxStatusGroup2;
	P_HW_MAC_RX_STS_GROUP_3_T prRxStatusGroup3;
	P_HW_MAC_RX_STS_GROUP_4_T prRxStatusGroup4;

	/* rx data information */
	PVOID pvHeader;
	UINT_16 u2PacketLen;
	UINT_16 u2HeaderLen;

	PUINT_8 pucPayload;
	UINT_16 u2PayloadLength;

	P_STA_RECORD_T prStaRec;

	UINT_8 ucPacketType;

	/* rx sta record */
	UINT_8 ucWlanIdx;
	UINT_8 ucStaRecIdx;

	BOOLEAN fgReorderBuffer;
	BOOLEAN fgDataFrame;
	BOOLEAN fgFragFrame;
	/* duplicate detection */
	UINT_16 u2FrameCtrl;
	UINT_16 u2SequenceControl;
	UINT_16 u2SSN;
	UINT_8 ucTid;

	ENUM_CSUM_RESULT_T aeCSUM[CSUM_TYPE_NUM];
	ENUM_RX_PKT_DESTINATION_T eDst;
	ENUM_TRAFFIC_CLASS_INDEX_T eTC;	/* only valid when eDst == FORWARD */
};

/*! RX configuration type structure */
typedef struct _RX_CTRL_T {
	UINT_32 u4RxCachedSize;
	PUINT_8 pucRxCached;
	QUE_T rFreeSwRfbList;
	QUE_T rReceivedRfbList;
	QUE_T rIndicatedRfbList;

#if CFG_SDIO_RX_AGG
	PUINT_8 pucRxCoalescingBufPtr;
#endif

	PVOID apvIndPacket[CFG_RX_MAX_PKT_NUM];
	PVOID apvRetainedPacket[CFG_RX_MAX_PKT_NUM];

	UINT_8 ucNumIndPacket;
	UINT_8 ucNumRetainedPacket;
	UINT_64 au8Statistics[RX_STATISTIC_COUNTER_NUM];	/*!< RX Counters */

#if CFG_HIF_STATISTICS
	UINT_32 u4TotalRxAccessNum;
	UINT_32 u4TotalRxPacketNum;
#endif

#if CFG_HIF_RX_STARVATION_WARNING
	UINT_32 u4QueuedCnt;
	UINT_32 u4DequeuedCnt;
#endif

#if CFG_RX_PKTS_DUMP
	UINT_32 u4RxPktsDumpTypeMask;
#endif

#if CFG_SUPPORT_SNIFFER
    UINT_32 u4AmpduRefNum;
#endif
} RX_CTRL_T, *P_RX_CTRL_T;

typedef struct _RX_MAILBOX_T {
	UINT_32 u4RxMailbox[2];	/* for Device-to-Host Mailbox */
} RX_MAILBOX_T, *P_RX_MAILBOX_T;

typedef WLAN_STATUS(*PROCESS_RX_MGT_FUNCTION) (P_ADAPTER_T, P_SW_RFB_T);

typedef struct _EMU_MAC_RATE_INFO_T {
	UINT_8 ucPhyRateCode;
	UINT_32 u4PhyRate[4][2];
} EMU_MAC_RATE_INFO_T, *P_EMU_MAC_RATE_INFO_T;

/*******************************************************************************
*                           P R I V A T E   D A T A
********************************************************************************
*/

/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/
#define RATE_INFO(_RateCode, _Bw20, _Bw20SGI, _Bw40, _BW40SGI, _Bw80, _Bw80SGI, _Bw160, _Bw160SGI) \
				{ \
					.ucPhyRateCode									  = (_RateCode), \
					.u4PhyRate[RX_VT_FR_MODE_20][MAC_GI_NORMAL] 	  = (_Bw20), \
					.u4PhyRate[RX_VT_FR_MODE_20][MAC_GI_SHORT]		  = (_Bw20SGI), \
					.u4PhyRate[RX_VT_FR_MODE_40][MAC_GI_NORMAL] 	  = (_Bw40), \
					.u4PhyRate[RX_VT_FR_MODE_40][MAC_GI_SHORT]		  = (_BW40SGI), \
					.u4PhyRate[RX_VT_FR_MODE_80][MAC_GI_NORMAL] 	  = (_Bw80), \
					.u4PhyRate[RX_VT_FR_MODE_80][MAC_GI_SHORT]		  = (_Bw80SGI), \
					.u4PhyRate[RX_VT_FR_MODE_160][MAC_GI_NORMAL]	  = (_Bw160), \
					.u4PhyRate[RX_VT_FR_MODE_160][MAC_GI_SHORT] 	  = (_Bw160SGI), \
				}

#define RX_INC_CNT(prRxCtrl, eCounter)              \
    {((P_RX_CTRL_T)prRxCtrl)->au8Statistics[eCounter]++; }

#define RX_ADD_CNT(prRxCtrl, eCounter, u8Amount)    \
    {((P_RX_CTRL_T)prRxCtrl)->au8Statistics[eCounter] += (UINT_64)u8Amount; }

#define RX_GET_CNT(prRxCtrl, eCounter)              \
    (((P_RX_CTRL_T)prRxCtrl)->au8Statistics[eCounter])

#define RX_RESET_ALL_CNTS(prRxCtrl)                 \
    {kalMemZero(&prRxCtrl->au8Statistics[0], sizeof(prRxCtrl->au8Statistics)); }

#define RX_STATUS_TEST_MORE_FLAG(flag) \
    ((BOOL)((flag & RX_STATUS_FLAG_MORE_PACKET) ? TRUE : FALSE))

/*------------------------------------------------------------------------------
 * MACRO for HW_MAC_RX_DESC_T
 *------------------------------------------------------------------------------
*/
/* DW 0 */
#define HAL_RX_STATUS_GET_RX_BYTE_CNT(_prHwMacRxDesc) ((_prHwMacRxDesc)->u2RxByteCount)
#define HAL_RX_STATUS_GET_ETH_TYPE_OFFSET(_prHwMacRxDesc) (((_prHwMacRxDesc)->u2PktTYpe & RX_STATUS_ETH_TYPE_OFFSET_MASK) >> RX_STATUS_ETH_TYPE_OFFSET)
#define HAL_RX_STATUS_GET_GROUP_VLD(_prHwMacRxDesc) (((_prHwMacRxDesc)->u2PktTYpe & RX_STATUS_GROUP_VLD_MASK) >> RX_STATUS_GROUP_VLD_OFFSET)
#define HAL_RX_STATUS_GET_PKT_TYPE(_prHwMacRxDesc) (((_prHwMacRxDesc)->u2PktTYpe & RX_STATUS_PKT_TYPE_MASK) >> RX_STATUS_PKT_TYPE_OFFSET)

/* DW 1 */
#define HAL_RX_STATUS_IS_HTC_EXIST(_prHwMacRxDesc)        (((_prHwMacRxDesc)->ucMatchPacket & RX_STATUS_HTC)?TRUE:FALSE)
#define HAL_RX_STATUS_IS_UC2ME(_prHwMacRxDesc)        (((_prHwMacRxDesc)->ucMatchPacket & RX_STATUS_UC2ME)?TRUE:FALSE)
#define HAL_RX_STATUS_IS_MC(_prHwMacRxDesc)        (((_prHwMacRxDesc)->ucMatchPacket & RX_STATUS_MC_FRAME)?TRUE:FALSE)
#define HAL_RX_STATUS_IS_BC(_prHwMacRxDesc)        (((_prHwMacRxDesc)->ucMatchPacket & RX_STATUS_BC_FRAME)?TRUE:FALSE)
#define HAL_RX_STATUS_IS_BCN_WITH_BMC(_prHwMacRxDesc)     (((_prHwMacRxDesc)->ucMatchPacket & RX_STATUS_BCN_WITH_BMC)?TRUE:FALSE)
#define HAL_RX_STATUS_IS_BCN_WITH_UC(_prHwMacRxDesc)      (((_prHwMacRxDesc)->ucMatchPacket & RX_STATUS_BCN_WITH_UC)?TRUE:FALSE)
#define HAL_RX_STATUS_GET_KEY_ID(_prHwMacRxDesc)          (((_prHwMacRxDesc)->ucMatchPacket & RX_STATUS_KEYID_MASK) >> RX_STATUS_KEYID_OFFSET)
#define HAL_RX_STATUS_GET_CHAN_FREQ(_prHwMacRxDesc) ((_prHwMacRxDesc)->ucChanFreq)
#define HAL_RX_STATUS_GET_HEADER_LEN(_prHwMacRxDesc) ((_prHwMacRxDesc)->ucHeaderLen & RX_STATUS_HEADER_LEN_MASK)
#define HAL_RX_STATUS_IS_HEADER_OFFSET(_prHwMacRxDesc) (((_prHwMacRxDesc)->ucHeaderLen & RX_STATUS_HEADER_OFFSET)?TRUE:FALSE)
#define HAL_RX_STATUS_GET_HEADER_OFFSET(_prHwMacRxDesc) (((_prHwMacRxDesc)->ucHeaderLen & RX_STATUS_HEADER_OFFSET) ? 2 : 0)
#define HAL_RX_STATUS_IS_HEADER_TRAN(_prHwMacRxDesc) (((_prHwMacRxDesc)->ucHeaderLen & RX_STATUS_HEADER_TRAN)?TRUE:FALSE)
#define HAL_RX_STATUS_GET_HEADER_TRAN(_prHwMacRxDesc) HAL_RX_STATUS_IS_HEADER_TRAN(_prHwMacRxDesc)
#define HAL_RX_STATUS_GET_PAYLOAD_FORMAT(_prHwMacRxDesc)  (((_prHwMacRxDesc)->ucBssid & RX_STATUS_PAYLOAD_FORMAT_MASK) >> RX_STATUS_PAYLOAD_FORMAT_OFFSET)
#define HAL_RX_STATUS_GET_BSSID(_prHwMacRxDesc) (((_prHwMacRxDesc)->ucBssid & RX_STATUS_BSSID_MASK) >> RX_STATUS_BSSID_OFFSET)

/* DW 2 */
#define HAL_RX_STATUS_GET_WLAN_IDX(_prHwMacRxDesc) ((_prHwMacRxDesc)->ucWlanIdx)
#define HAL_RX_STATUS_GET_TID(_prHwMacRxDesc)        (((_prHwMacRxDesc)->ucTidSecMode & RX_STATUS_TID_MASK))
#define HAL_RX_STATUS_GET_SEC_MODE(_prHwMacRxDesc)        (((_prHwMacRxDesc)->ucTidSecMode & RX_STATUS_SEC_MASK) >> RX_STATUS_SEC_OFFSET)
#define HAL_RX_STATUS_GET_SW_BIT(_prHwMacRxDesc)        (((_prHwMacRxDesc)->u2StatusFlag & RX_STATUS_SW_BIT)?TRUE:FALSE)
#define HAL_RX_STATUS_IS_FCS_ERROR(_prHwMacRxDesc)        (((_prHwMacRxDesc)->u2StatusFlag & RX_STATUS_FLAG_FCS_ERROR)?TRUE:FALSE)
#define HAL_RX_STATUS_IS_CIPHER_MISMATCH(_prHwMacRxDesc)  (((_prHwMacRxDesc)->u2StatusFlag & RX_STATUS_FLAG_CIPHER_MISMATCH)?TRUE:FALSE)
#define HAL_RX_STATUS_IS_CLM_ERROR(_prHwMacRxDesc)        (((_prHwMacRxDesc)->u2StatusFlag & RX_STATUS_FLAG_CIPHER_LENGTH_MISMATCH)?TRUE:FALSE)
#define HAL_RX_STATUS_IS_ICV_ERROR(_prHwMacRxDesc)        (((_prHwMacRxDesc)->u2StatusFlag & RX_STATUS_FLAG_ICV_ERROR)?TRUE:FALSE)
#define HAL_RX_STATUS_IS_TKIP_MIC_ERROR(_prHwMacRxDesc)   (((_prHwMacRxDesc)->u2StatusFlag & RX_STATUS_FLAG_TKIPMIC_ERROR) > 0?TRUE:FALSE)
#define HAL_RX_STATUS_IS_ERROR(_prHwMacRxDesc)            (((_prHwMacRxDesc)->u2StatusFlag & RX_STATUS_FLAG_ERROR_MASK)?TRUE:FALSE)
#define HAL_RX_STATUS_IS_LEN_MISMATCH(_prHwMacRxDesc)     (((_prHwMacRxDesc)->u2StatusFlag & RX_STATUS_FLAG_LEN_MISMATCH)?TRUE:FALSE)
#define HAL_RX_STATUS_IS_DE_AMSDU_FAIL(_prHwMacRxDesc)    (((_prHwMacRxDesc)->u2StatusFlag & RX_STATUS_FLAG_DE_AMSDU_FAIL)?TRUE:FALSE)
#define HAL_RX_STATUS_IS_EXCEED_LEN(_prHwMacRxDesc)       (((_prHwMacRxDesc)->u2StatusFlag & RX_STATUS_FLAG_EXCEED_LEN)?TRUE:FALSE)
#define HAL_RX_STATUS_IS_LLC_MIS(_prHwMacRxDesc) (((_prHwMacRxDesc)->u2StatusFlag & RX_STATUS_LLC_MIS)?TRUE:FALSE)
#define HAL_RX_STATUS_IS_UDF_VLT(_prHwMacRxDesc) (((_prHwMacRxDesc)->u2StatusFlag & RX_STATUS_UDF_VLT)?TRUE:FALSE)
#define HAL_RX_STATUS_IS_FRAG(_prHwMacRxDesc)        (((_prHwMacRxDesc)->u2StatusFlag & RX_STATUS_FRAG)?TRUE:FALSE)
#define HAL_RX_STATUS_IS_NULL(_prHwMacRxDesc)        (((_prHwMacRxDesc)->u2StatusFlag & RX_STATUS_NULL)?TRUE:FALSE)
#define HAL_RX_STATUS_IS_DATA(_prHwMacRxDesc)        (((_prHwMacRxDesc)->u2StatusFlag & RX_STATUS_DATA)?FALSE:TRUE)
#define HAL_RX_STATUS_IS_AMPDU_SUB_FRAME(_prHwMacRxDesc)  (((_prHwMacRxDesc)->u2StatusFlag & RX_STATUS_AMPDU_SUB_FRAME)?FALSE:TRUE)
#define HAL_RX_STATUS_IS_AMPDU_FORMAT(_prHwMacRxDesc)     (((_prHwMacRxDesc)->u2StatusFlag & RX_STATUS_AMPDU_FORMAT)?FALSE:TRUE)

/* DW 3 */
#define HAL_RX_STATUS_IS_RV_VALID(_prHwMacRxDesc)         (((_prHwMacRxDesc)->u4PatternFilterInfo & RX_STATUS_RXV_SEQ_NO_MASK)?TRUE:FALSE)
#define HAL_RX_STATUS_GET_RXV_SEQ_NO(_prHwMacRxDesc)      ((_prHwMacRxDesc)->u4PatternFilterInfo & RX_STATUS_RXV_SEQ_NO_MASK)
#define HAL_RX_STATUS_GET_TCL(_prHwMacRxDesc)            (((_prHwMacRxDesc)->u4PatternFilterInfo & RX_STATUS_TCL)?TRUE:FALSE)
#define HAL_RX_STATUS_IS_CLS(_prHwMacRxDesc)              (((_prHwMacRxDesc)->u4PatternFilterInfo & RX_STATUS_CLS)?TRUE:FALSE)
#define HAL_RX_STATUS_GET_OFLD(_prHwMacRxDesc)            (((_prHwMacRxDesc)->u4PatternFilterInfo & RX_STATUS_OFLD_MASK) >> RX_STATUS_OFLD_OFFSET)
#define HAL_RX_STATUS_IS_MGC(_prHwMacRxDesc)              (((_prHwMacRxDesc)->u4PatternFilterInfo & RX_STATUS_MGC)?TRUE:FALSE)
#define HAL_RX_STATUS_GET_WOL(_prHwMacRxDesc)             (((_prHwMacRxDesc)->u4PatternFilterInfo & RX_STATUS_WOL_MASK) >> RX_STATUS_WOL_OFFSET)
#define HAL_RX_STATUS_GET_CLS_BITMAP(_prHwMacRxDesc)      (((_prHwMacRxDesc)->u4PatternFilterInfo & RX_STATUS_CLS_BITMAP_MASK) >> RX_STATUS_CLS_BITMAP_OFFSET)
#define HAL_RX_STATUS_IS_PF_BLACK_LIST(_prHwMacRxDesc)    (((_prHwMacRxDesc)->u4PatternFilterInfo & RX_STATUS_PF_MODE_BLACK_LIST)?TRUE:FALSE)
#define HAL_RX_STATUS_IS_PF_CHECKED(_prHwMacRxDesc)       (((_prHwMacRxDesc)->u4PatternFilterInfo & RX_STATUS_PF_STS_CHECKED)?TRUE:FALSE)

/* DW 4~7 */
#define HAL_RX_STATUS_GET_FRAME_CTL_FIELD(_prHwMacRxStsGroup4) ((_prHwMacRxStsGroup4)->u2FrameCtl)
#define HAL_RX_STATUS_GET_TA(_prHwMacRxStsGroup4, pucTA)   \
{\
    kalMemCopy(pucTA, &(_prHwMacRxStsGroup4)->aucTA[0], 6); \
}
#define HAL_RX_STATUS_GET_SEQ_FRAG_NUM(_prHwMacRxStsGroup4)   ((_prHwMacRxStsGroup4)->u2SeqFrag)
#define HAL_RX_STATUS_GET_QOS_CTL_FIELD(_prHwMacRxStsGroup4) ((_prHwMacRxStsGroup4)->u2Qos)

#define HAL_RX_STATUS_GET_SEQFrag_NUM(_prHwMacRxStsGroup4)   ((_prHwMacRxStsGroup4)->u2SeqFrag)
#define HAL_RX_STATUS_GET_HTC(_prHwMacRxStsGroup4) ((_prHwMacRxStsGroup4)->u4HTC)

/* DW 8~11 */
#define HAL_RX_STATUS_GET_RSC(_prHwMacRxStsGroup1, pucRSC)   \
{\
    kalMemCopy(pucRSC, &(_prHwMacRxStsGroup1)->aucPN[0], 6); \
}

#define HAL_RX_STATUS_GET_PN(_prHwMacRxStsGroup1, pucPN)   \
{\
    kalMemCopy(pucPN, &(_prHwMacRxStsGroup1)->aucPN[0], 16); \
}

/* DW 12~13 */
#define HAL_RX_STATUS_GET_TIMESTAMP(_prHwMacRxStsGroup2, _ucIdx) ((_prHwMacRxStsGroup2)->u4Timestamp)
#define HAL_RX_STATUS_GET_FCS32(_prHwMacRxStsGroup2)             ((_prHwMacRxStsGroup2)->u4CRC)


/* DW 14~19 */
#define HAL_RX_STATUS_GET_RX_VECTOR(_prHwMacRxStsGroup3, _ucIdx) ((_prHwMacRxStsGroup3)->u4RxVector[_ucIdx])

#define HAL_RX_STATUS_GET_RCPI(_prHwMacRxStsGroup3) (((_prHwMacRxStsGroup3)->u4RxVector[2] & RX_VT_RCPI_MASK) >> RX_VT_RCPI_OFFSET)


/* TBD */
#define HAL_RX_STATUS_GET_RX_PACKET_LEN(_prHwMacRxDesc)
#define HAL_RX_STATUS_IS_MATCH_PACKET(_prHwMacRxDesc)

#define HAL_RX_STATUS_GET_CHNL_NUM(_prHwMacRxDesc) \
    ((((_prHwMacRxDesc)->ucChanFreq) > HW_CHNL_NUM_MAX_4G_5G) ? \
      (((_prHwMacRxDesc)->ucChanFreq) - HW_CHNL_NUM_MAX_4G_5G) : \
      ((_prHwMacRxDesc)->ucChanFreq))

/* To do: support more bands other than 2.4G and 5G */
#define HAL_RX_STATUS_GET_RF_BAND(_prHwMacRxDesc) \
    ((((_prHwMacRxDesc)->ucChanFreq) <= HW_CHNL_NUM_MAX_2G4) ? \
      BAND_2G4 : BAND_5G)

/*------------------------------------------------------------------------------
 * MACRO for HW_RX_VECTOR_DESC_T
 *------------------------------------------------------------------------------
*/
#define HAL_RX_VECTOR_GET_TA(_prHwRxVector, pucTA)   \
{\
    kalMemCopy(pucTA, &(_prHwRxVector)->aucTA[0], 6); \
}

#define HAL_RX_VECTOR_GET_SEQ_NO(_prHwRxVector)       ((_prHwRxVector)->ucRxVtSeqNo & RX_STATUS_RXV_SEQ_NO_MASK)
#define HAL_RX_VECTOR_IS_FOR_BA_ACK(_prHwRxVector)    (((_prHwRxVector)->ucRxVtSeqNo & RX_VECTOR_FOR_BA_ACK)?TRUE:FALSE)
#define HAL_RX_VECTOR_GET_RX_VECTOR(_prHwRxVector, _ucIdx) ((_prHwRxVector)->u4RxVector[_ucIdx])

#define RXM_IS_QOS_DATA_FRAME(_u2FrameCtrl) \
       (((_u2FrameCtrl & MASK_FRAME_TYPE) == MAC_FRAME_QOS_DATA) ? TRUE : FALSE)

/*******************************************************************************
*                   F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/

VOID nicRxInitialize(IN P_ADAPTER_T prAdapter);

VOID nicRxUninitialize(IN P_ADAPTER_T prAdapter);

VOID nicRxProcessRFBs(IN P_ADAPTER_T prAdapter);

#if !CFG_SDIO_INTR_ENHANCE
VOID nicRxReceiveRFBs(IN P_ADAPTER_T prAdapter);

WLAN_STATUS nicRxReadBuffer(IN P_ADAPTER_T prAdapter, IN OUT P_SW_RFB_T prSwRfb);

#else
VOID nicRxSDIOReceiveRFBs(IN P_ADAPTER_T prAdapter);

WLAN_STATUS
nicRxEnhanceReadBuffer(IN P_ADAPTER_T prAdapter,
		       IN UINT_32 u4DataPort, IN UINT_16 u2RxLength, IN OUT P_SW_RFB_T prSwRfb);
#endif				/* CFG_SDIO_INTR_ENHANCE */


#if CFG_SDIO_RX_AGG
VOID nicRxSDIOAggReceiveRFBs(IN P_ADAPTER_T prAdapter);
#endif

WLAN_STATUS nicRxSetupRFB(IN P_ADAPTER_T prAdapter, IN P_SW_RFB_T prRfb);

VOID nicRxReturnRFB(IN P_ADAPTER_T prAdapter, IN P_SW_RFB_T prRfb);

VOID nicProcessRxInterrupt(IN P_ADAPTER_T prAdapter);

VOID nicRxProcessPktWithoutReorder(IN P_ADAPTER_T prAdapter, IN P_SW_RFB_T prSwRfb);

VOID nicRxProcessForwardPkt(IN P_ADAPTER_T prAdapter, IN P_SW_RFB_T prSwRfb);

VOID nicRxProcessGOBroadcastPkt(IN P_ADAPTER_T prAdapter, IN P_SW_RFB_T prSwRfb);


VOID nicRxFillRFB(IN P_ADAPTER_T prAdapter, IN OUT P_SW_RFB_T prSwRfb);

P_SW_RFB_T
incRxDefragMPDU(IN P_ADAPTER_T prAdapter, IN P_SW_RFB_T prSWRfb, OUT P_QUE_T prReturnedQue);

BOOLEAN nicRxIsDuplicateFrame(IN OUT P_SW_RFB_T prSwRfb);

#if CFG_SUPPORT_SNIFFER
VOID nicRxProcessMonitorPacket(IN P_ADAPTER_T prAdapter, IN OUT P_SW_RFB_T prSwRfb);
#endif

VOID nicRxProcessDataPacket(IN P_ADAPTER_T prAdapter, IN OUT P_SW_RFB_T prSwRfb);

VOID nicRxProcessEventPacket(IN P_ADAPTER_T prAdapter, IN OUT P_SW_RFB_T prSwRfb);

VOID nicRxProcessMgmtPacket(IN P_ADAPTER_T prAdapter, IN OUT P_SW_RFB_T prSwRfb);

#if CFG_TCP_IP_CHKSUM_OFFLOAD
VOID
nicRxFillChksumStatus(IN P_ADAPTER_T prAdapter,
		      IN OUT P_SW_RFB_T prSwRfb, IN UINT_32 u4TcpUdpIpCksStatus);

VOID nicRxUpdateCSUMStatistics(IN P_ADAPTER_T prAdapter, IN const ENUM_CSUM_RESULT_T aeCSUM[]
    );
#endif				/* CFG_TCP_IP_CHKSUM_OFFLOAD */


VOID nicRxQueryStatus(IN P_ADAPTER_T prAdapter, IN PUINT_8 pucBuffer, OUT PUINT_32 pu4Count);

VOID nicRxClearStatistics(IN P_ADAPTER_T prAdapter);

VOID nicRxQueryStatistics(IN P_ADAPTER_T prAdapter, IN PUINT_8 pucBuffer, OUT PUINT_32 pu4Count);

WLAN_STATUS
nicRxWaitResponse(IN P_ADAPTER_T prAdapter,
		  IN UINT_8 ucPortIdx,
		  OUT PUINT_8 pucRspBuffer, IN UINT_32 u4MaxRespBufferLen, OUT PUINT_32 pu4Length);

VOID nicRxEnablePromiscuousMode(IN P_ADAPTER_T prAdapter);


VOID nicRxDisablePromiscuousMode(IN P_ADAPTER_T prAdapter);


WLAN_STATUS nicRxFlush(IN P_ADAPTER_T prAdapter);

WLAN_STATUS nicRxProcessActionFrame(IN P_ADAPTER_T prAdapter, IN P_SW_RFB_T prSwRfb);

VOID nicRxMgmtNoWTBLHandling(P_ADAPTER_T prAdapter, P_SW_RFB_T prSwRfb);
#endif				/* _NIC_RX_H */
