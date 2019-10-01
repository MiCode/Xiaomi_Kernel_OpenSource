#ifndef MSS_INGRESS_REGS_HEADER
#define MSS_INGRESS_REGS_HEADER

#define mssIngressVersionStatusRegister_ADDR 0x00008000
#define mssIngressPrescaleControlRegister_ADDR 0x00008004
#define mssIngressVlanTpid_0Register_ADDR 0x00008006
#define mssIngressVlanTpid_1Register_ADDR 0x00008008
#define mssIngressVlanControlRegister_ADDR 0x0000800A
#define mssIngressMtuSizeControlRegister_ADDR 0x0000800C
#define mssIngressControlRegister_ADDR 0x0000800E
#define mssIngressSaControlRegister_ADDR 0x00008010
#define mssIngressDebugStatusRegister_ADDR 0x00008012
#define mssIngressBusyStatusRegister_ADDR 0x00008022
#define mssIngressSpareControlRegister_ADDR 0x00008024
#define mssIngressSciDefaultControlRegister_ADDR 0x0000802A
#define mssIngressInterruptStatusRegister_ADDR 0x0000802E
#define mssIngressInterruptMaskRegister_ADDR 0x00008030
#define mssIngressSaIcvErrorStatusRegister_ADDR 0x00008032
#define mssIngressSaReplayErrorStatusRegister_ADDR 0x00008034
#define mssIngressSaExpiredStatusRegister_ADDR 0x00008036
#define mssIngressSaThresholdExpiredStatusRegister_ADDR 0x00008038
#define mssIngressEccInterruptStatusRegister_ADDR 0x0000803A
#define mssIngressIgprcLutEccError_1AddressStatusRegister_ADDR 0x0000803C
#define mssIngressIgprcLutEccError_2AddressStatusRegister_ADDR 0x0000803E
#define mssIngressIgctlfLutEccErrorAddressStatusRegister_ADDR 0x00008040
#define mssIngressIgpfmtLutEccErrorAddressStatusRegister_ADDR 0x00008042
#define mssIngressIgpopLutEccErrorAddressStatusRegister_ADDR 0x00008044
#define mssIngressIgpoctlfLutEccErrorAddressStatusRegister_ADDR 0x00008046
#define mssIngressIgpocLutEccError_1AddressStatusRegister_ADDR 0x00008048
#define mssIngressIgpocLutEccError_2AddressStatusRegister_ADDR 0x0000804A
#define mssIngressIgmibEccErrorAddressStatusRegister_ADDR 0x0000804C
#define mssIngressEccControlRegister_ADDR 0x0000804E
#define mssIngressDebugControlRegister_ADDR 0x00008050
#define mssIngressAdditionalDebugStatusRegister_ADDR 0x00008052
#define mssIngressLutAddressControlRegister_ADDR 0x00008080
#define mssIngressLutControlRegister_ADDR 0x00008081
#define mssIngressLutDataControlRegister_ADDR 0x000080A0

//---------------------------------------------------------------------------------
//                  MSS Ingress Version Status Register: 1E.8000 
//---------------------------------------------------------------------------------
struct mssIngressVersionStatusRegister_t
{
  union
  {
    struct
    {
                    /*! \brief 1E.8000.F:0 ROS MSS Ingress ID [F:0]
                        mssIngressVersionStatusRegister_t.bits_0.mssIngressID
                        Default = 0x0016
                        MSS egress ID
                 <B>Notes:</B>
                        ID  */
      unsigned int   mssIngressID : 16;    // 1E.8000.F:0  ROS      Default = 0x0016 
                     /* MSS egress ID  */
    } bits_0;
    unsigned short word_0;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.8001.7:0 ROS MSS Ingress Version [7:0]
                        mssIngressVersionStatusRegister_t.bits_1.mssIngressVersion
                        Default = 0x08
                        MSS egress version
  */
      unsigned int   mssIngressVersion : 8;    // 1E.8001.7:0  ROS      Default = 0x08 
                     /* MSS egress version  */
                    /*! \brief 1E.8001.F:8 ROS MSS Ingress Revision [7:0]
                        mssIngressVersionStatusRegister_t.bits_1.mssIngressRevision
                        Default = 0x08
                        MSS egress revision
  */
      unsigned int   mssIngressRevision : 8;    // 1E.8001.F:8  ROS      Default = 0x08 
                     /* MSS egress revision
                        
                          */
    } bits_1;
    unsigned short word_1;
  };
};

//---------------------------------------------------------------------------------
//                  MSS Ingress Prescale Control Register: 1E.8004 
//---------------------------------------------------------------------------------
struct mssIngressPrescaleControlRegister_t
{
  union
  {
    struct
    {
                    /*! \brief 1E.8004.F:0 R/W MSS Ingress Prescale Configuration LSW [F:0]
                        mssIngressPrescaleControlRegister_t.bits_0.mssIngressPrescaleConfigurationLSW
                        Default = 0x5940
                        Prescale register bits 15:0
                 <B>Notes:</B>
                        Unused.  */
      unsigned int   mssIngressPrescaleConfigurationLSW : 16;    // 1E.8004.F:0  R/W      Default = 0x5940 
                     /* Prescale register bits 15:0
                          */
    } bits_0;
    unsigned short word_0;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.8005.F:0 R/W MSS Ingress Prescale Configuration MSW [1F:10]
                        mssIngressPrescaleControlRegister_t.bits_1.mssIngressPrescaleConfigurationMSW
                        Default = 0x0773
                        Prescale register bits 31:16
                 <B>Notes:</B>
                        Unused.  */
      unsigned int   mssIngressPrescaleConfigurationMSW : 16;    // 1E.8005.F:0  R/W      Default = 0x0773 
                     /* Prescale register bits 31:16
                          */
    } bits_1;
    unsigned short word_1;
  };
};

//---------------------------------------------------------------------------------
//                  MSS Ingress VLAN TPID 0 Register: 1E.8006 
//---------------------------------------------------------------------------------
struct mssIngressVlanTpid_0Register_t
{
  union
  {
    struct
    {
                    /*! \brief 1E.8006.F:0 R/W MSS Ingress VLAN STag [F:0]
                        mssIngressVlanTpid_0Register_t.bits_0.mssIngressVlanStag
                        Default = 0x0000
                        STag TPID
                 <B>Notes:</B>
                        Service Tag Protocol Identifier (TPID) values to identify a VLAN tag. The " See SEC Egress VLAN CP Tag Parse STag " bit must be set to 1 for the incoming packet's TPID to be parsed.  */
      unsigned int   mssIngressVlanStag : 16;    // 1E.8006.F:0  R/W      Default = 0x0000 
                     /* STag TPID */
    } bits_0;
    unsigned short word_0;
  };
  union
  {
    struct
    {
      unsigned int   reserved0 : 16;
    } bits_1;
    unsigned short word_1;
  };
};

//---------------------------------------------------------------------------------
//                  MSS Ingress VLAN TPID 1 Register: 1E.8008 
//---------------------------------------------------------------------------------
struct mssIngressVlanTpid_1Register_t
{
  union
  {
    struct
    {
                    /*! \brief 1E.8008.F:0 R/W MSS Ingress VLAN QTag [F:0]
                        mssIngressVlanTpid_1Register_t.bits_0.mssIngressVlanQtag
                        Default = 0x0000
                        QTag TPID
                 <B>Notes:</B>
                        Customer Tag Protocol Identifier (TPID) values to identify a VLAN tag. The " See SEC Egress VLAN CP Tag Parse QTag " bit must be set to 1 for the incoming packet's TPID to be parsed.  */
      unsigned int   mssIngressVlanQtag : 16;    // 1E.8008.F:0  R/W      Default = 0x0000 
                     /* QTag TPID
                          */
    } bits_0;
    unsigned short word_0;
  };
  union
  {
    struct
    {
      unsigned int   reserved0 : 16;
    } bits_1;
    unsigned short word_1;
  };
};

//---------------------------------------------------------------------------------
//                  MSS Ingress VLAN Control Register: 1E.800A 
//---------------------------------------------------------------------------------
struct mssIngressVlanControlRegister_t
{
  union
  {
    struct
    {
                    /*! \brief 1E.800A.F:0 R/W MSS Ingress VLAN UP Map Table LSW [F:0]
                        mssIngressVlanControlRegister_t.bits_0.mssIngressVlanUpMapTableLSW
                        Default = 0x0000
                        Map table bits 15:0
                 <B>Notes:</B>
                        If there is a customer TPID Tag match and no service TPID Tag match or the service TPID Tag match is disabled, the outer TAG's PCP is used to index into this map table to generate the packets user priority.
                        2:0 : UP value for customer Tag PCP 0x0
                        5:3: UP value for customer Tag PCP 0x0
                        8:6 : UP value for customer Tag PCP 0x0
                        11:9 : UP value for customer Tag PCP 0x0
                        14:12 : UP value for customer Tag PCP 0x0
                        17:15 : UP value for customer Tag PCP 0x0  */
      unsigned int   mssIngressVlanUpMapTableLSW : 16;    // 1E.800A.F:0  R/W      Default = 0x0000 
                     /* Map table bits 15:0
                          */
    } bits_0;
    unsigned short word_0;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.800B.7:0 R/W MSS Ingress VLAN UP Map Table MSW [17:10]
                        mssIngressVlanControlRegister_t.bits_1.mssIngressVlanUpMapTableMSW
                        Default = 0x00
                        UP Map table bits 23:16
                 <B>Notes:</B>
                        If there is a customer TPID Tag match and no service TPID Tag match or the service TPID Tag match is disabled, the outer TAG's PCP is used to index into this map table to generate the packets user priority.
                        2:0 : UP value for customer Tag PCP 0x0
                        5:3: UP value for customer Tag PCP 0x0
                        8:6 : UP value for customer Tag PCP 0x0
                        11:9 : UP value for customer Tag PCP 0x0
                        14:12 : UP value for customer Tag PCP 0x0
                        17:15 : UP value for customer Tag PCP 0x0
                        20:18 : UP value for customer Tag PCP 0x0
                        23:21 : UP value for customer Tag PCP 0x0  */
      unsigned int   mssIngressVlanUpMapTableMSW : 8;    // 1E.800B.7:0  R/W      Default = 0x00 
                     /* UP Map table bits 23:16
                          */
                    /*! \brief 1E.800B.A:8 R/W MSS Ingress VLAN UP Default [2:0]
                        mssIngressVlanControlRegister_t.bits_1.mssIngressVlanUpDefault
                        Default = 0x0
                        UP default
                 <B>Notes:</B>
                        User priority default  */
      unsigned int   mssIngressVlanUpDefault : 3;    // 1E.800B.A:8  R/W      Default = 0x0 
                     /* UP default */
                    /*! \brief 1E.800B.B R/W MSS Ingress VLAN STag UP Parse Enable
                        mssIngressVlanControlRegister_t.bits_1.mssIngressVlanStagUpParseEnable
                        Default = 0x0
                        VLAN CP Tag STag UP enable
                 <B>Notes:</B>
                        Enable controlled port service VLAN service Tag user priority field parsing.  */
      unsigned int   mssIngressVlanStagUpParseEnable : 1;    // 1E.800B.B  R/W      Default = 0x0 
                     /* VLAN CP Tag STag UP enable */
                    /*! \brief 1E.800B.C R/W MSS Ingress VLAN QTag UP Parse Enable
                        mssIngressVlanControlRegister_t.bits_1.mssIngressVlanQtagUpParseEnable
                        Default = 0x0
                        VLAN CP Tag QTag UP enable
                 <B>Notes:</B>
                        Enable controlled port customer VLAN customer Tag user priority field parsing.  */
      unsigned int   mssIngressVlanQtagUpParseEnable : 1;    // 1E.800B.C  R/W      Default = 0x0 
                     /* VLAN CP Tag QTag UP enable */
                    /*! \brief 1E.800B.D R/W MSS Ingress VLAN QinQ Parse Enable
                        mssIngressVlanControlRegister_t.bits_1.mssIngressVlanQinqParseEnable
                        Default = 0x0
                        VLAN CP Tag Parse QinQ
                 <B>Notes:</B>
                        Enable controlled port VLAN QinQ Tag parsing. When this bit is set to 1 both the outer and inner VLAN Tags will be parsed.  */
      unsigned int   mssIngressVlanQinqParseEnable : 1;    // 1E.800B.D  R/W      Default = 0x0 
                     /* VLAN CP Tag Parse QinQ */
                    /*! \brief 1E.800B.E R/W MSS Ingress VLAN STag Parse Enable
                        mssIngressVlanControlRegister_t.bits_1.mssIngressVlanStagParseEnable
                        Default = 0x0
                        1 = Enable VLAN STag parsing
                 <B>Notes:</B>
                        Enable controlled port VLAN service Tag parsing. When this bit is set to 1, the incoming packets outer TPID will be compared with the configured " See MSS Ingress VLAN Stag [F:0] " for matching. If the " See SEC Egress VLAN CP Tag Parse QinQ " bit is set to1, this will also be used to compare the incoming packet's inner TPID.  */
      unsigned int   mssIngressVlanStagParseEnable : 1;    // 1E.800B.E  R/W      Default = 0x0 
                     /* 1 = Enable VLAN STag parsing */
                    /*! \brief 1E.800B.F R/W MSS Ingress VLAN QTag Parse Enable
                        mssIngressVlanControlRegister_t.bits_1.mssIngressVlanQtagParseEnable
                        Default = 0x0
                        1 = Enable VLAN QTag parsing
                 <B>Notes:</B>
                        Enable controlled port VLAN customer Tag parsing. When this bit is set to 1, the incoming packet's outer TPID will be compared with the configured " See MSS Ingress VLAN QTag [F:0] " for matching. If the " See SEC Egress VLAN CP Tag Parse QinQ " bit is set to1, this will also be used to compare the incoming packet's inner TPID.  */
      unsigned int   mssIngressVlanQtagParseEnable : 1;    // 1E.800B.F  R/W      Default = 0x0 
                     /* 1 = Enable VLAN QTag parsing  */
    } bits_1;
    unsigned short word_1;
  };
};

//---------------------------------------------------------------------------------
//                  MSS Ingress MTU Size Control Register: 1E.800C 
//---------------------------------------------------------------------------------
struct mssIngressMtuSizeControlRegister_t
{
  union
  {
    struct
    {
                    /*! \brief 1E.800C.F:0 R/W MSS Ingress Controlled Packet MTU Size [F:0]
                        mssIngressMtuSizeControlRegister_t.bits_0.mssIngressControlledPacketMtuSize
                        Default = 0x05DC
                        Maximum transmission unit for controlled packet
                 <B>Notes:</B>
                        Maximum transmission unit of controlled packet  */
      unsigned int   mssIngressControlledPacketMtuSize : 16;    // 1E.800C.F:0  R/W      Default = 0x05DC 
                     /* Maximum transmission unit for controlled packet */
    } bits_0;
    unsigned short word_0;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.800D.F:0 R/W MSS Ingress Uncontrolled Packet MTU Size [F:0]
                        mssIngressMtuSizeControlRegister_t.bits_1.mssIngressUncontrolledPacketMtuSize
                        Default = 0x05DC
                        Maximum transmission unit for uncontrolled packet
                 <B>Notes:</B>
                        Maximum transmission unit of uncontrolled packet  */
      unsigned int   mssIngressUncontrolledPacketMtuSize : 16;    // 1E.800D.F:0  R/W      Default = 0x05DC 
                     /* Maximum transmission unit for uncontrolled packet  */
    } bits_1;
    unsigned short word_1;
  };
};

//---------------------------------------------------------------------------------
//                  MSS Ingress Control Register: 1E.800E 
//---------------------------------------------------------------------------------
struct mssIngressControlRegister_t
{
  union
  {
    struct
    {
                    /*! \brief 1E.800E.0 R/W MSS Ingress Soft Reset
                        mssIngressControlRegister_t.bits_0.mssIngressSoftReset
                        Default = 0x0
                        1 = Soft reset
                 <B>Notes:</B>
                        S/W reset  */
      unsigned int   mssIngressSoftReset : 1;    // 1E.800E.0  R/W      Default = 0x0 
                     /* 1 = Soft reset */
                    /*! \brief 1E.800E.1 R/W MSS Ingress Operation Point To Point
                        mssIngressControlRegister_t.bits_0.mssIngressOperationPointToPoint
                        Default = 0x0
                        1 = Enable the SCI for authorization default
                 <B>Notes:</B>
                        The default SCI for authorization is configured in  See MSS Ingress SCI Default [F:0]   See MSS Ingress SCI Default [1F:10] , See MSS Ingress SCI Default [2F:20] , and  See MSS Ingress SCI Default [3F:30] .  */
      unsigned int   mssIngressOperationPointToPoint : 1;    // 1E.800E.1  R/W      Default = 0x0 
                     /* 1 = Enable the SCI for authorization default  */
                    /*! \brief 1E.800E.2 R/W MSS Ingress Create SCI
                        mssIngressControlRegister_t.bits_0.mssIngressCreateSci
                        Default = 0x0
                        0 = SCI from IGPRC LUT
                 <B>Notes:</B>
                        If the SCI is not in the packet and this bit is set to 0, the SCI will be taken from the IGPRC LUT.  */
      unsigned int   mssIngressCreateSci : 1;    // 1E.800E.2  R/W      Default = 0x0 
                     /* 0 = SCI from IGPRC LUT
                          */
                    /*! \brief 1E.800E.3 R/W MSS Ingress Mask Short Length Error
                        mssIngressControlRegister_t.bits_0.mssIngressMaskShortLengthError
                        Default = 0x0
                        Unused
                 <B>Notes:</B>
                        Unused  */
      unsigned int   mssIngressMaskShortLengthError : 1;    // 1E.800E.3  R/W      Default = 0x0 
                     /* Unused */
                    /*! \brief 1E.800E.4 R/W MSS Ingress Drop Kay Packet
                        mssIngressControlRegister_t.bits_0.mssIngressDropKayPacket
                        Default = 0x0
                        1 = Drop KaY packets
                 <B>Notes:</B>
                        Decides whether KaY packets have to be dropped  */
      unsigned int   mssIngressDropKayPacket : 1;    // 1E.800E.4  R/W      Default = 0x0 
                     /* 1 = Drop KaY packets   */
                    /*! \brief 1E.800E.5 R/W MSS Ingress Drop IGPRC Miss
                        mssIngressControlRegister_t.bits_0.mssIngressDropIgprcMiss
                        Default = 0x0
                        1 = Drop IGPRC miss packets
                 <B>Notes:</B>
                        Decides whether Ingress Pre-Security Classification (IGPRC) LUT miss packets are to be dropped  */
      unsigned int   mssIngressDropIgprcMiss : 1;    // 1E.800E.5  R/W      Default = 0x0 
                     /* 1 = Drop IGPRC miss packets */
                    /*! \brief 1E.800E.6 R/W MSS Ingress Check ICV
                        mssIngressControlRegister_t.bits_0.mssIngressCheckIcv
                        Default = 0x0
                        Unused
                 <B>Notes:</B>
                        Unused  */
      unsigned int   mssIngressCheckIcv : 1;    // 1E.800E.6  R/W      Default = 0x0 
                     /* Unused */
                    /*! \brief 1E.800E.7 R/W MSS Ingress Clear Global Time
                        mssIngressControlRegister_t.bits_0.mssIngressClearGlobalTime
                        Default = 0x0
                        1 = Clear global time
                 <B>Notes:</B>
                        Clear global time  */
      unsigned int   mssIngressClearGlobalTime : 1;    // 1E.800E.7  R/W      Default = 0x0 
                     /* 1 = Clear global time */
                    /*! \brief 1E.800E.8 R/W MSS Ingress Clear Count
                        mssIngressControlRegister_t.bits_0.mssIngressClearCount
                        Default = 0x0
                        1 = Clear all MIB counters
                 <B>Notes:</B>
                        If this bit is set to 1, all MIB counters will be cleared.  */
      unsigned int   mssIngressClearCount : 1;    // 1E.800E.8  R/W      Default = 0x0 
                     /* 1 = Clear all MIB counters                        */
                    /*! \brief 1E.800E.9 R/W MSS Ingress High Priority
                        mssIngressControlRegister_t.bits_0.mssIngressHighPriority
                        Default = 0x0
                        1 = MIB counter clear on read enable
                 <B>Notes:</B>
                        If this bit is set to 1, read is given high priority and the MIB count value becomes 0 after read.  */
      unsigned int   mssIngressHighPriority : 1;    // 1E.800E.9  R/W      Default = 0x0 
                     /* 1 = MIB counter clear on read enable */
                    /*! \brief 1E.800E.A R/W MSS Ingress Remove SECTag
                        mssIngressControlRegister_t.bits_0.mssIngressRemoveSectag
                        Default = 0x0
                        1 = Enable removal of SECTag
                 <B>Notes:</B>
                        If this bit is set and either of the following two conditions occurs, the SECTag will be removed.
                        Controlled packet and either the SA or SC is invalid.
                        IGPRC miss.  */
      unsigned int   mssIngressRemoveSectag : 1;    // 1E.800E.A  R/W      Default = 0x0 
                     /* 1 = Enable removal of SECTag */
                    /*! \brief 1E.800E.C:B R/W MSS Ingress Global Validate Frames [1:0]
                        mssIngressControlRegister_t.bits_0.mssIngressGlobalValidateFrames
                        Default = 0x0
                        Default validate frames configuration
                 <B>Notes:</B>
                        If the SC is invalid or if an IGPRC miss packet condition occurs, this default will be used for the validate frames configuration instead of the validate frame entry in the Ingress SC Table (IGSCT).  */
      unsigned int   mssIngressGlobalValidateFrames : 2;    // 1E.800E.C:B  R/W      Default = 0x0 
                     /* Default validate frames configuration
                          */
                    /*! \brief 1E.800E.D R/W MSS Ingress ICV LSB 8 Bytes Enable
                        mssIngressControlRegister_t.bits_0.mssIngressIcvLsb_8BytesEnable
                        Default = 0x0
                        1 = Use LSB
                        0 = Use MSB
                 <B>Notes:</B>
                        This bit selects MSB or LSB 8 bytes selection in the case where the ICV is 8 bytes.
                        0 = MSB is used.  */
      unsigned int   mssIngressIcvLsb_8BytesEnable : 1;    // 1E.800E.D  R/W      Default = 0x0 
                     /* 1 = Use LSB
                        0 = Use MSB */
      unsigned int   reserved0 : 2;
    } bits_0;
    unsigned short word_0;
  };
  union
  {
    struct
    {
      unsigned int   reserved0 : 16;
    } bits_1;
    unsigned short word_1;
  };
};

//---------------------------------------------------------------------------------
//                  MSS Ingress SA Control Register: 1E.8010 
//---------------------------------------------------------------------------------
struct mssIngressSaControlRegister_t
{
  union
  {
    struct
    {
                    /*! \brief 1E.8010.F:0 R/W MSS Ingress SA Threshold LSW [F:0]
                        mssIngressSaControlRegister_t.bits_0.mssIngressSaThresholdLSW
                        Default = 0x0000
                        SA threshold bits 15:0
                 <B>Notes:</B>
                        Ingress PN threshold to generate SA threshold interrupt.  */
      unsigned int   mssIngressSaThresholdLSW : 16;    // 1E.8010.F:0  R/W      Default = 0x0000 
                     /* SA threshold bits 15:0  */
    } bits_0;
    unsigned short word_0;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.8011.F:0 R/W MSS Ingress SA Threshold MSW [1F:10]
                        mssIngressSaControlRegister_t.bits_1.mssIngressSaThresholdMSW
                        Default = 0x0000
                        SA threshold bits 31:16
                 <B>Notes:</B>
                        Ingress PN threshold to generate SA threshold interrupt.  */
      unsigned int   mssIngressSaThresholdMSW : 16;    // 1E.8011.F:0  R/W      Default = 0x0000 
                     /* SA threshold bits 31:16
                          */
    } bits_1;
    unsigned short word_1;
  };
};

//---------------------------------------------------------------------------------
//                  MSS Ingress Debug Status Register: 1E.8012 
//---------------------------------------------------------------------------------
struct mssIngressDebugStatusRegister_t
{
  union
  {
    struct
    {
                    /*! \brief 1E.8012.3:0 ROS MSS Ingress IGPFMT State Machine [3:0]
                        mssIngressDebugStatusRegister_t.bits_0.mssIngressIgpfmtStateMachine

                        Default = 0x0

                        IGPFMT state machine
                        
  */
      unsigned int   mssIngressIgpfmtStateMachine : 4;    // 1E.8012.3:0  ROS      Default = 0x0 
                     /* IGPFMT state machine
                          */
                    /*! \brief 1E.8012.7:4 ROS MSS Ingress IGPFMT Buffer Depth [3:0]
                        mssIngressDebugStatusRegister_t.bits_0.mssIngressIgpfmtBufferDepth

                        Default = 0x0

                        IGPFMT buffer depth
                        
  */
      unsigned int   mssIngressIgpfmtBufferDepth : 4;    // 1E.8012.7:4  ROS      Default = 0x0 
                     /* IGPFMT buffer depth
                          */
                    /*! \brief 1E.8012.B:8 ROS MSS Ingress IGPFMT Pad Insertion State Machine [3:0]
                        mssIngressDebugStatusRegister_t.bits_0.mssIngressIgpfmtPadInsertionStateMachine

                        Default = 0x0

                        IGPFMT pad insertion state machine
                        
  */
      unsigned int   mssIngressIgpfmtPadInsertionStateMachine : 4;    // 1E.8012.B:8  ROS      Default = 0x0 
                     /* IGPFMT pad insertion state machine
                          */
                    /*! \brief 1E.8012.D:C ROS MSS Ingress IGPFMT Pad Insertion Buffer Depth [1:0]
                        mssIngressDebugStatusRegister_t.bits_0.mssIngressIgpfmtPadInsertionBufferDepth

                        Default = 0x0

                        IGPFMT pad insertion buffer depth
                        
  */
      unsigned int   mssIngressIgpfmtPadInsertionBufferDepth : 2;    // 1E.8012.D:C  ROS      Default = 0x0 
                     /* IGPFMT pad insertion buffer depth
                          */
      unsigned int   reserved0 : 2;
    } bits_0;
    unsigned short word_0;
  };
  union
  {
    struct
    {
      unsigned int   reserved0 : 16;
    } bits_1;
    unsigned short word_1;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.8014.3:0 ROS MSS Ingress IGPOP State Machine [3:0]
                        mssIngressDebugStatusRegister_t.bits_2.mssIngressIgpopStateMachine

                        Default = 0x0

                        IGPOP debug status
                        
  */
      unsigned int   mssIngressIgpopStateMachine : 4;    // 1E.8014.3:0  ROS      Default = 0x0 
                     /* IGPOP debug status
                          */
                    /*! \brief 1E.8014.8:4 ROS MSS Ingress IGPOP Buffer Depth [4:0]
                        mssIngressDebugStatusRegister_t.bits_2.mssIngressIgpopBufferDepth

                        Default = 0x00

                        IGPOP debug status
                        
  */
      unsigned int   mssIngressIgpopBufferDepth : 5;    // 1E.8014.8:4  ROS      Default = 0x00 
                     /* IGPOP debug status
                          */
                    /*! \brief 1E.8014.A:9 ROS MSS Ingress IGPFMT SECTag Removal State Machine [1:0]
                        mssIngressDebugStatusRegister_t.bits_2.mssIngressIgpfmtSectagRemovalStateMachine

                        Default = 0x0

                        IGPFMT SECTag removal debug status
                        
  */
      unsigned int   mssIngressIgpfmtSectagRemovalStateMachine : 2;    // 1E.8014.A:9  ROS      Default = 0x0 
                     /* IGPFMT SECTag removal debug status
                          */
                    /*! \brief 1E.8014.D:B ROS MSS Ingress IGPFMT SECTag Removal Buffer Depth [2:0]
                        mssIngressDebugStatusRegister_t.bits_2.mssIngressIgpfmtSectagRemovalBufferDepth

                        Default = 0x0

                        IGPFMT SECTag removal debug status
                        
  */
      unsigned int   mssIngressIgpfmtSectagRemovalBufferDepth : 3;    // 1E.8014.D:B  ROS      Default = 0x0 
                     /* IGPFMT SECTag removal debug status
                          */
                    /*! \brief 1E.8014.F:E ROS MSS Ingress IGPFMT Last Bytes Removal State Machine [1:0]
                        mssIngressDebugStatusRegister_t.bits_2.mssIngressIgpfmtLastBytesRemovalStateMachine

                        Default = 0x0

                        IGPFMT last bytes removal debug status
                        
  */
      unsigned int   mssIngressIgpfmtLastBytesRemovalStateMachine : 2;    // 1E.8014.F:E  ROS      Default = 0x0 
                     /* IGPFMT last bytes removal debug status
                          */
    } bits_2;
    unsigned int word_2;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.8015.1:0 ROS MSS Ingress IGPFMT Last Bytes Removal Buffer Depth [3:2]
                        mssIngressDebugStatusRegister_t.bits_3.mssIngressIgpfmtLastBytesRemovalBufferDepth

                        Default = 0x0

                        IGPFMT last bytes removal debug status
                        
  */
      unsigned int   mssIngressIgpfmtLastBytesRemovalBufferDepth : 2;    // 1E.8015.1:0  ROS      Default = 0x0 
                     /* IGPFMT last bytes removal debug status
                          */
      unsigned int   reserved0 : 14;
    } bits_3;
    unsigned int word_3;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.8016.F:0 R/W MSS Ingress GCM Counter LSW [F:0]
                        mssIngressDebugStatusRegister_t.bits_4.mssIngressGcmCounterLSW

                        Default = 0x0000

                        GCM Counter bits 15:0
                        

                 <B>Notes:</B>
                        Debug  */
      unsigned int   mssIngressGcmCounterLSW : 16;    // 1E.8016.F:0  R/W      Default = 0x0000 
                     /* GCM Counter bits 15:0
                          */
    } bits_4;
    unsigned int word_4;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.8017.F:0 R/W MSS Ingress GCM Counter MSW [1F:10]
                        mssIngressDebugStatusRegister_t.bits_5.mssIngressGcmCounterMSW

                        Default = 0x0000

                        GCM counter bits 31:16
                        

                 <B>Notes:</B>
                        Debug  */
      unsigned int   mssIngressGcmCounterMSW : 16;    // 1E.8017.F:0  R/W      Default = 0x0000 
                     /* GCM counter bits 31:16
                          */
    } bits_5;
    unsigned int word_5;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.8018.1:0 ROS MSS Ingress GMC Mult Buffer Depth [1:0]
                        mssIngressDebugStatusRegister_t.bits_6.mssIngressGmcMultBufferDepth

                        Default = 0x0

                        GCM buffer depth
                        
  */
      unsigned int   mssIngressGmcMultBufferDepth : 2;    // 1E.8018.1:0  ROS      Default = 0x0 
                     /* GCM buffer depth
                          */
      unsigned int   reserved0 : 14;
    } bits_6;
    unsigned int word_6;
  };
  union
  {
    struct
    {
      unsigned int   reserved0 : 16;
    } bits_7;
    unsigned int word_7;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.801A.F:0 ROS MSS Ingress GCM Mult Length A 0 [F:0]
                        mssIngressDebugStatusRegister_t.bits_8.mssIngressGcmMultLengthA_0

                        Default = 0x0000

                        GCM mult length A bits 15:0
                        
  */
      unsigned int   mssIngressGcmMultLengthA_0 : 16;    // 1E.801A.F:0  ROS      Default = 0x0000 
                     /* GCM mult length A bits 15:0
                          */
    } bits_8;
    unsigned int word_8;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.801B.F:0 ROS MSS Ingress GCM Mult Length A 1 [1F:10]
                        mssIngressDebugStatusRegister_t.bits_9.mssIngressGcmMultLengthA_1

                        Default = 0x0000

                        GCM mult length A bits 31:16
                        
  */
      unsigned int   mssIngressGcmMultLengthA_1 : 16;    // 1E.801B.F:0  ROS      Default = 0x0000 
                     /* GCM mult length A bits 31:16
                          */
    } bits_9;
    unsigned int word_9;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.801C.F:0 ROS MSS Ingress GCM Mult Length A 2 [2F:20]
                        mssIngressDebugStatusRegister_t.bits_10.mssIngressGcmMultLengthA_2

                        Default = 0x0000

                        GCM mult lengthA bits 47:32
                        
  */
      unsigned int   mssIngressGcmMultLengthA_2 : 16;    // 1E.801C.F:0  ROS      Default = 0x0000 
                     /* GCM mult lengthA bits 47:32
                          */
    } bits_10;
    unsigned short word_10;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.801D.F:0 ROS MSS Ingress GCM Mult Length A 3 [3F:30]
                        mssIngressDebugStatusRegister_t.bits_11.mssIngressGcmMultLengthA_3

                        Default = 0x0000

                        GCM mult length A bits 63:48
                        
  */
      unsigned int   mssIngressGcmMultLengthA_3 : 16;    // 1E.801D.F:0  ROS      Default = 0x0000 
                     /* GCM mult length A bits 63:48
                          */
    } bits_11;
    unsigned short word_11;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.801E.F:0 ROS MSS Ingress GCM Mult Length C 0 [F:0]
                        mssIngressDebugStatusRegister_t.bits_12.mssIngressGcmMultLengthC_0

                        Default = 0x0000

                        GCM mult length C bits 15:0
                        
  */
      unsigned int   mssIngressGcmMultLengthC_0 : 16;    // 1E.801E.F:0  ROS      Default = 0x0000 
                     /* GCM mult length C bits 15:0
                          */
    } bits_12;
    unsigned short word_12;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.801F.F:0 ROS MSS Ingress GCM Mult Length C 1 [1F:10]
                        mssIngressDebugStatusRegister_t.bits_13.mssIngressGcmMultLengthC_1

                        Default = 0x0000

                        GCM mult length C bits 31:16
                        
  */
      unsigned int   mssIngressGcmMultLengthC_1 : 16;    // 1E.801F.F:0  ROS      Default = 0x0000 
                     /* GCM mult length C bits 31:16
                          */
    } bits_13;
    unsigned short word_13;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.8020.F:0 ROS MSS Ingress GCM Mult Length C 2 [2F:20]
                        mssIngressDebugStatusRegister_t.bits_14.mssIngressGcmMultLengthC_2

                        Default = 0x0000

                        GCM mult length C bits 47:32
                        
  */
      unsigned int   mssIngressGcmMultLengthC_2 : 16;    // 1E.8020.F:0  ROS      Default = 0x0000 
                     /* GCM mult length C bits 47:32
                          */
    } bits_14;
    unsigned short word_14;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.8021.F:0 ROS MSS Ingress GCM Mult Length C 3 [3F:30]
                        mssIngressDebugStatusRegister_t.bits_15.mssIngressGcmMultLengthC_3

                        Default = 0x0000

                        GCM mult length C bits 63:48
                        
  */
      unsigned int   mssIngressGcmMultLengthC_3 : 16;    // 1E.8021.F:0  ROS      Default = 0x0000 
                     /* GCM mult length C bits 63:48
                          */
    } bits_15;
    unsigned short word_15;
  };
};


//---------------------------------------------------------------------------------
//! \brief                MSS Ingress Busy Status Register: 1E.8022 
//                  MSS Ingress Busy Status Register: 1E.8022 
//---------------------------------------------------------------------------------
struct mssIngressBusyStatusRegister_t
{
  union
  {
    struct
    {
                    /*! \brief 1E.8022.0 ROS MSS Ingress Data Path Busy
                        mssIngressBusyStatusRegister_t.bits_0.mssIngressDataPathBusy

                        Default = 0x0

                        1 = IGPOC busy
                        

                 <B>Notes:</B>
                        This bit is an OR of the above bits.  */
      unsigned int   mssIngressDataPathBusy : 1;    // 1E.8022.0  ROS      Default = 0x0 
                     /* 1 = IGPOC busy
                          */
                    /*! \brief 1E.8022.1 ROS MSS Ingress IGPFMT Busy
                        mssIngressBusyStatusRegister_t.bits_0.mssIngressIgpfmtBusy

                        Default = 0x0

                        1 = PFMT busy
                        
  */
      unsigned int   mssIngressIgpfmtBusy : 1;    // 1E.8022.1  ROS      Default = 0x0 
                     /* 1 = PFMT busy
                          */
                    /*! \brief 1E.8022.2 ROS MSS Ingress AES Counter Busy
                        mssIngressBusyStatusRegister_t.bits_0.mssIngressAesCounterBusy

                        Default = 0x0

                        1 = AES counter busy
                        
  */
      unsigned int   mssIngressAesCounterBusy : 1;    // 1E.8022.2  ROS      Default = 0x0 
                     /* 1 = AES counter busy
                          */
                    /*! \brief 1E.8022.3 ROS MSS Ingress IGPOP Busy
                        mssIngressBusyStatusRegister_t.bits_0.mssIngressIgpopBusy

                        Default = 0x0

                        1 = IGPOP busy
                        
  */
      unsigned int   mssIngressIgpopBusy : 1;    // 1E.8022.3  ROS      Default = 0x0 
                     /* 1 = IGPOP busy
                          */
                    /*! \brief 1E.8022.4 ROS MSS Ingress GCM Buffer Busy
                        mssIngressBusyStatusRegister_t.bits_0.mssIngressGcmBufferBusy

                        Default = 0x0

                        1 = GCM buffer busy
                        

                 <B>Notes:</B>
                        GCM buffer is after the IGCTLF, IGPRC and IGPFMT.  */
      unsigned int   mssIngressGcmBufferBusy : 1;    // 1E.8022.4  ROS      Default = 0x0 
                     /* 1 = GCM buffer busy
                          */
                    /*! \brief 1E.8022.5 ROS MSS Ingress IGPRC Busy
                        mssIngressBusyStatusRegister_t.bits_0.mssIngressIgprcBusy

                        Default = 0x0

                        1 = Lookup busy
                        

                 <B>Notes:</B>
                        IGCTLF and IGPRC lookup.  */
      unsigned int   mssIngressIgprcBusy : 1;    // 1E.8022.5  ROS      Default = 0x0 
                     /* 1 = Lookup busy
                          */
                    /*! \brief 1E.8022.6 ROS MSS Ingress IGPOC Busy
                        mssIngressBusyStatusRegister_t.bits_0.mssIngressIgpocBusy

                        Default = 0x0

                        1 = IGPOC busy
                        
  */
      unsigned int   mssIngressIgpocBusy : 1;    // 1E.8022.6  ROS      Default = 0x0 
                     /* 1 = IGPOC busy
                          */
      unsigned int   reserved0 : 9;
    } bits_0;
    unsigned short word_0;
  };
};


//---------------------------------------------------------------------------------
//! \brief                MSS Ingress Spare Control Register: 1E.8024 
//                  MSS Ingress Spare Control Register: 1E.8024 
//---------------------------------------------------------------------------------
struct mssIngressSpareControlRegister_t
{
  union
  {
    struct
    {
                    /*! \brief 1E.8024.F:0 R/W MSS Ingress Spare Control 1 LSW [F:0]
                        mssIngressSpareControlRegister_t.bits_0.mssIngressSpareControl_1LSW

                        Default = 0x0000

                        Spare control bits 15:0
                        

                 <B>Notes:</B>
                        Reserved  */
      unsigned int   mssIngressSpareControl_1LSW : 16;    // 1E.8024.F:0  R/W      Default = 0x0000 
                     /* Spare control bits 15:0
                          */
    } bits_0;
    unsigned short word_0;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.8025.F:0 R/W MSS Ingress Spare Control 1 MSW [1F:10]
                        mssIngressSpareControlRegister_t.bits_1.mssIngressSpareControl_1MSW

                        Default = 0x0000

                        Spare control bits 31:16
                        

                 <B>Notes:</B>
                        Reserved  */
      unsigned int   mssIngressSpareControl_1MSW : 16;    // 1E.8025.F:0  R/W      Default = 0x0000 
                     /* Spare control bits 31:16
                          */
    } bits_1;
    unsigned short word_1;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.8026.F:0 R/W MSS Ingress Spare Control 2 LSW [F:0]
                        mssIngressSpareControlRegister_t.bits_2.mssIngressSpareControl_2LSW

                        Default = 0x0000

                        Spare control bits 15:0
                        

                 <B>Notes:</B>
                        Reserved  */
      unsigned int   mssIngressSpareControl_2LSW : 16;    // 1E.8026.F:0  R/W      Default = 0x0000 
                     /* Spare control bits 15:0
                          */
    } bits_2;
    unsigned int word_2;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.8027.F:0 R/W MSS Ingress Spare Control 2 MSW [1F:10]
                        mssIngressSpareControlRegister_t.bits_3.mssIngressSpareControl_2MSW

                        Default = 0x0000

                        Spare control bits 31:16
                        

                 <B>Notes:</B>
                        Reserved  */
      unsigned int   mssIngressSpareControl_2MSW : 16;    // 1E.8027.F:0  R/W      Default = 0x0000 
                     /* Spare control bits 31:16
                          */
    } bits_3;
    unsigned int word_3;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.8028.F:0 R/W MSS Ingress Spare Control 3 LSW [F:0]
                        mssIngressSpareControlRegister_t.bits_4.mssIngressSpareControl_3LSW

                        Default = 0x0000

                        Spare control bits 15:0
                        

                 <B>Notes:</B>
                        Reserved  */
      unsigned int   mssIngressSpareControl_3LSW : 16;    // 1E.8028.F:0  R/W      Default = 0x0000 
                     /* Spare control bits 15:0
                          */
    } bits_4;
    unsigned int word_4;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.8029.F:0 R/W MSS Ingress Spare Control 3 MSW [1F:10]
                        mssIngressSpareControlRegister_t.bits_5.mssIngressSpareControl_3MSW

                        Default = 0x0000

                        Spare control bits 31:16
                        

                 <B>Notes:</B>
                        Reserved  */
      unsigned int   mssIngressSpareControl_3MSW : 16;    // 1E.8029.F:0  R/W      Default = 0x0000 
                     /* Spare control bits 31:16
                          */
    } bits_5;
    unsigned int word_5;
  };
};


//---------------------------------------------------------------------------------
//! \brief                MSS Ingress SCI Default Control Register: 1E.802A 
//                  MSS Ingress SCI Default Control Register: 1E.802A 
//---------------------------------------------------------------------------------
struct mssIngressSciDefaultControlRegister_t
{
  union
  {
    struct
    {
                    /*! \brief 1E.802A.F:0 R/W MSS Ingress SCI Default 0 [F:0]
                        mssIngressSciDefaultControlRegister_t.bits_0.mssIngressSciDefault_0

                        Default = 0x0000

                        SCI default bits 15:0
                        
  */
      unsigned int   mssIngressSciDefault_0 : 16;    // 1E.802A.F:0  R/W      Default = 0x0000 
                     /* SCI default bits 15:0
                          */
    } bits_0;
    unsigned short word_0;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.802B.F:0 R/W MSS Ingress SCI Default 1 [1F:10]
                        mssIngressSciDefaultControlRegister_t.bits_1.mssIngressSciDefault_1

                        Default = 0x0000

                        SCI default bits 31:16
                        
  */
      unsigned int   mssIngressSciDefault_1 : 16;    // 1E.802B.F:0  R/W      Default = 0x0000 
                     /* SCI default bits 31:16
                          */
    } bits_1;
    unsigned short word_1;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.802C.F:0 R/W MSS Ingress SCI Default 2 [2F:20]
                        mssIngressSciDefaultControlRegister_t.bits_2.mssIngressSciDefault_2

                        Default = 0x0000

                        SCI default bits 47:32
                        
  */
      unsigned int   mssIngressSciDefault_2 : 16;    // 1E.802C.F:0  R/W      Default = 0x0000 
                     /* SCI default bits 47:32
                          */
    } bits_2;
    unsigned int word_2;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.802D.F:0 R/W MSS Ingress SCI Default 3 [3F:30]
                        mssIngressSciDefaultControlRegister_t.bits_3.mssIngressSciDefault_3

                        Default = 0x0000

                        SCI default bits 63:48
                        
  */
      unsigned int   mssIngressSciDefault_3 : 16;    // 1E.802D.F:0  R/W      Default = 0x0000 
                     /* SCI default bits 63:48
                          */
    } bits_3;
    unsigned int word_3;
  };
};


//---------------------------------------------------------------------------------
//! \brief                MSS Ingress Interrupt Status Register: 1E.802E 
//                  MSS Ingress Interrupt Status Register: 1E.802E 
//---------------------------------------------------------------------------------
struct mssIngressInterruptStatusRegister_t
{
  union
  {
    struct
    {
                    /*! \brief 1E.802E.0 COW MSS Master Ingress Interrupt
                        mssIngressInterruptStatusRegister_t.bits_0.mssMasterIngressInterrupt

                        Default = 0x0

                        1 = Interrupt
                        

                 <B>Notes:</B>
                        Write to 1 to clear. This bit is set when any one of the above interrupt and the corresponding interrupt enable are both set. The interrupt enable for this bit must also be set for this bit to be set.  */
      unsigned int   mssMasterIngressInterrupt : 1;    // 1E.802E.0  COW      Default = 0x0 
                     /* 1 = Interrupt
                          */
                    /*! \brief 1E.802E.1 COW MSS Ingress SA Expired Interrupt
                        mssIngressInterruptStatusRegister_t.bits_0.mssIngressSaExpiredInterrupt

                        Default = 0x0

                        1 = Interrupt
                        

                 <B>Notes:</B>
                        Write to 1 to clear. This bit is set when the SA PN reaches all ones saturation.  */
      unsigned int   mssIngressSaExpiredInterrupt : 1;    // 1E.802E.1  COW      Default = 0x0 
                     /* 1 = Interrupt
                          */
                    /*! \brief 1E.802E.2 COW MSS Ingress SA Threshold Expired Interrupt
                        mssIngressInterruptStatusRegister_t.bits_0.mssIngressSaThresholdExpiredInterrupt

                        Default = 0x0

                        1 = Interrupt
                        

                 <B>Notes:</B>
                        Write to 1 to clear. This bit is set when the SA PN reaches the  See SEC Egress PN Threshold [F:0] and  See SEC Egress PN Threshold [1F:10] .  */
      unsigned int   mssIngressSaThresholdExpiredInterrupt : 1;    // 1E.802E.2  COW      Default = 0x0 
                     /* 1 = Interrupt
                          */
                    /*! \brief 1E.802E.3 COW MSS Ingress ICV Error Interrupt
                        mssIngressInterruptStatusRegister_t.bits_0.mssIngressIcvErrorInterrupt

                        Default = 0x0

                        1 = Interrupt
                        

                 <B>Notes:</B>
                        Write to 1 to clear.  */
      unsigned int   mssIngressIcvErrorInterrupt : 1;    // 1E.802E.3  COW      Default = 0x0 
                     /* 1 = Interrupt
                          */
                    /*! \brief 1E.802E.4 COW MSS Ingress Replay Error Interrupt
                        mssIngressInterruptStatusRegister_t.bits_0.mssIngressReplayErrorInterrupt

                        Default = 0x0

                        1 = Interrupt
                        

                 <B>Notes:</B>
                        Write to 1 to clear.  */
      unsigned int   mssIngressReplayErrorInterrupt : 1;    // 1E.802E.4  COW      Default = 0x0 
                     /* 1 = Interrupt
                          */
                    /*! \brief 1E.802E.5 COW MSS Ingress MIB Saturation Interrupt
                        mssIngressInterruptStatusRegister_t.bits_0.mssIngressMibSaturationInterrupt

                        Default = 0x0

                        1 = Interrupt
                        

                 <B>Notes:</B>
                        Write to 1 to clear. This bit is set when the MIB counters reaches all ones saturation.  */
      unsigned int   mssIngressMibSaturationInterrupt : 1;    // 1E.802E.5  COW      Default = 0x0 
                     /* 1 = Interrupt
                          */
                    /*! \brief 1E.802E.6 COW MSS Ingress ECC Error Interrupt
                        mssIngressInterruptStatusRegister_t.bits_0.mssIngressEccErrorInterrupt

                        Default = 0x0

                        1 = Interrupt
                        

                 <B>Notes:</B>
                        Write to 1 to clear.  */
      unsigned int   mssIngressEccErrorInterrupt : 1;    // 1E.802E.6  COW      Default = 0x0 
                     /* 1 = Interrupt
                          */
                    /*! \brief 1E.802E.7 COW MSS Ingress TCI E/C Error Interrupt
                        mssIngressInterruptStatusRegister_t.bits_0.mssIngressTciE_cErrorInterrupt

                        Default = 0x0

                        1 = Interrupt
                        

                 <B>Notes:</B>
                        Write to 1 to clear. This error occurs when the TCI E bit is 1 and the TCI C bit is 0. The packet is not dropped, uncontrolled, or untagged.  */
      unsigned int   mssIngressTciE_cErrorInterrupt : 1;    // 1E.802E.7  COW      Default = 0x0 
                     /* 1 = Interrupt
                          */
                    /*! \brief 1E.802E.8 COW MSS Ingress IGPOC Miss Interrupt
                        mssIngressInterruptStatusRegister_t.bits_0.mssIngressIgpocMissInterrupt

                        Default = 0x0

                        1 = Interrupt
                        
  */
      unsigned int   mssIngressIgpocMissInterrupt : 1;    // 1E.802E.8  COW      Default = 0x0 
                     /* 1 = Interrupt
                          */
      unsigned int   reserved0 : 7;
    } bits_0;
    unsigned short word_0;
  };
  union
  {
    struct
    {
      unsigned int   reserved0 : 16;
    } bits_1;
    unsigned short word_1;
  };
};


//---------------------------------------------------------------------------------
//! \brief                MSS Ingress Interrupt Mask Register: 1E.8030 
//                  MSS Ingress Interrupt Mask Register: 1E.8030 
//---------------------------------------------------------------------------------
struct mssIngressInterruptMaskRegister_t
{
  union
  {
    struct
    {
                    /*! \brief 1E.8030.0 R/W MSS Ingress Master Interrupt Enable
                        mssIngressInterruptMaskRegister_t.bits_0.mssIngressMasterInterruptEnable

                        Default = 0x0

                        1 = Interrupt enabled
                        
  */
      unsigned int   mssIngressMasterInterruptEnable : 1;    // 1E.8030.0  R/W      Default = 0x0 
                     /* 1 = Interrupt enabled
                          */
                    /*! \brief 1E.8030.1 R/W MSS Ingress SA Expired Interrupt Enable
                        mssIngressInterruptMaskRegister_t.bits_0.mssIngressSaExpiredInterruptEnable

                        Default = 0x0

                        1 = Interrupt enabled
                        
  */
      unsigned int   mssIngressSaExpiredInterruptEnable : 1;    // 1E.8030.1  R/W      Default = 0x0 
                     /* 1 = Interrupt enabled
                          */
                    /*! \brief 1E.8030.2 R/W MSS Ingress SA Threshold Expired Interrupt Enable
                        mssIngressInterruptMaskRegister_t.bits_0.mssIngressSaThresholdExpiredInterruptEnable

                        Default = 0x0

                        1 = Interrupt enabled
                        
  */
      unsigned int   mssIngressSaThresholdExpiredInterruptEnable : 1;    // 1E.8030.2  R/W      Default = 0x0 
                     /* 1 = Interrupt enabled
                          */
                    /*! \brief 1E.8030.3 R/W MSS Ingress ICV Error Interrupt Enable
                        mssIngressInterruptMaskRegister_t.bits_0.mssIngressIcvErrorInterruptEnable

                        Default = 0x0

                        1 = Interrupt enabled
                        
  */
      unsigned int   mssIngressIcvErrorInterruptEnable : 1;    // 1E.8030.3  R/W      Default = 0x0 
                     /* 1 = Interrupt enabled
                          */
                    /*! \brief 1E.8030.4 R/W MSS Ingress Replay Error Interrupt Enable
                        mssIngressInterruptMaskRegister_t.bits_0.mssIngressReplayErrorInterruptEnable

                        Default = 0x0

                        1 = Interrupt enabled
                        
  */
      unsigned int   mssIngressReplayErrorInterruptEnable : 1;    // 1E.8030.4  R/W      Default = 0x0 
                     /* 1 = Interrupt enabled
                          */
                    /*! \brief 1E.8030.5 R/W MSS Ingress MIB Saturation Interrupt Enable
                        mssIngressInterruptMaskRegister_t.bits_0.mssIngressMibSaturationInterruptEnable

                        Default = 0x0

                        1 = Interrupt enabled
                        
  */
      unsigned int   mssIngressMibSaturationInterruptEnable : 1;    // 1E.8030.5  R/W      Default = 0x0 
                     /* 1 = Interrupt enabled
                          */
                    /*! \brief 1E.8030.6 R/W MSS Ingress ECC Error Interrupt Enable
                        mssIngressInterruptMaskRegister_t.bits_0.mssIngressEccErrorInterruptEnable

                        Default = 0x0

                        1 = Interrupt enabled
                        
  */
      unsigned int   mssIngressEccErrorInterruptEnable : 1;    // 1E.8030.6  R/W      Default = 0x0 
                     /* 1 = Interrupt enabled
                          */
                    /*! \brief 1E.8030.7 R/W MSS Ingress TCI E/C Error Interrupt Enable
                        mssIngressInterruptMaskRegister_t.bits_0.mssIngressTciE_cErrorInterruptEnable

                        Default = 0x0

                        1 = Interrupt enabled
                        
  */
      unsigned int   mssIngressTciE_cErrorInterruptEnable : 1;    // 1E.8030.7  R/W      Default = 0x0 
                     /* 1 = Interrupt enabled
                          */
                    /*! \brief 1E.8030.8 R/W MSS Ingress IGPOC Miss Interrupt Enable
                        mssIngressInterruptMaskRegister_t.bits_0.mssIngressIgpocMissInterruptEnable

                        Default = 0x0

                        1 = Interrupt enabled
                        
  */
      unsigned int   mssIngressIgpocMissInterruptEnable : 1;    // 1E.8030.8  R/W      Default = 0x0 
                     /* 1 = Interrupt enabled
                          */
      unsigned int   reserved0 : 7;
    } bits_0;
    unsigned short word_0;
  };
  union
  {
    struct
    {
      unsigned int   reserved0 : 16;
    } bits_1;
    unsigned short word_1;
  };
};


//---------------------------------------------------------------------------------
//! \brief                MSS Ingress SA ICV Error Status Register: 1E.8032 
//                  MSS Ingress SA ICV Error Status Register: 1E.8032 
//---------------------------------------------------------------------------------
struct mssIngressSaIcvErrorStatusRegister_t
{
  union
  {
    struct
    {
                    /*! \brief 1E.8032.F:0 COW MSS Ingress SA ICV Error LSW [F:0]
                        mssIngressSaIcvErrorStatusRegister_t.bits_0.mssIngressSaIcvErrorLSW

                        Default = 0x0000

                        SA ICV error bits 15:0
                        

                 <B>Notes:</B>
                        When set, these bits identify the SA that has an ICV error. Write these bits to 1 to clear.  */
      unsigned int   mssIngressSaIcvErrorLSW : 16;    // 1E.8032.F:0  COW      Default = 0x0000 
                     /* SA ICV error bits 15:0
                          */
    } bits_0;
    unsigned short word_0;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.8033.F:0 COW MSS Ingress SA ICV Error MSW [1F:10]
                        mssIngressSaIcvErrorStatusRegister_t.bits_1.mssIngressSaIcvErrorMSW

                        Default = 0x0000

                        SA ICV error bits 31:16
                        

                 <B>Notes:</B>
                        When set, these bits identify the SA that has an ICV error. Write these bits to 1 to clear.  */
      unsigned int   mssIngressSaIcvErrorMSW : 16;    // 1E.8033.F:0  COW      Default = 0x0000 
                     /* SA ICV error bits 31:16
                          */
    } bits_1;
    unsigned short word_1;
  };
};


//---------------------------------------------------------------------------------
//! \brief                MSS Ingress SA Replay Error Status Register: 1E.8034 
//                  MSS Ingress SA Replay Error Status Register: 1E.8034 
//---------------------------------------------------------------------------------
struct mssIngressSaReplayErrorStatusRegister_t
{
  union
  {
    struct
    {
                    /*! \brief 1E.8034.F:0 COW MSS Ingress SA Replay Error LSW [F:0]
                        mssIngressSaReplayErrorStatusRegister_t.bits_0.mssIngressSaReplayErrorLSW

                        Default = 0x0000

                        SA replay error bits 15:0
                        

                 <B>Notes:</B>
                        When set, these bits identify the SA that has a replay error. Write these bits to 1 to clear.  */
      unsigned int   mssIngressSaReplayErrorLSW : 16;    // 1E.8034.F:0  COW      Default = 0x0000 
                     /* SA replay error bits 15:0
                          */
    } bits_0;
    unsigned short word_0;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.8035.F:0 COW MSS Ingress SA Replay Error MSW [1F:10]
                        mssIngressSaReplayErrorStatusRegister_t.bits_1.mssIngressSaReplayErrorMSW

                        Default = 0x0000

                        SA replay error bits 31:16
                        

                 <B>Notes:</B>
                        When set, these bits identify the SA that has a replay error. Write these bits to 1 to clear.  */
      unsigned int   mssIngressSaReplayErrorMSW : 16;    // 1E.8035.F:0  COW      Default = 0x0000 
                     /* SA replay error bits 31:16
                          */
    } bits_1;
    unsigned short word_1;
  };
};


//---------------------------------------------------------------------------------
//! \brief                MSS Ingress SA Expired Status Register: 1E.8036 
//                  MSS Ingress SA Expired Status Register: 1E.8036 
//---------------------------------------------------------------------------------
struct mssIngressSaExpiredStatusRegister_t
{
  union
  {
    struct
    {
                    /*! \brief 1E.8036.F:0 ROS MSS Ingress SA Expired LSW [F:0]
                        mssIngressSaExpiredStatusRegister_t.bits_0.mssIngressSaExpiredLSW

                        Default = 0x0000

                        SA expired bits 15:0
                        

                 <B>Notes:</B>
                        When set, these bits identify the SA that has expired when the SA PN reaches all-ones saturation. Write these bits to 1 to clear.  */
      unsigned int   mssIngressSaExpiredLSW : 16;    // 1E.8036.F:0  ROS      Default = 0x0000 
                     /* SA expired bits 15:0
                          */
    } bits_0;
    unsigned short word_0;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.8037.F:0 ROS MSS Ingress SA Expired MSW [1F:10]
                        mssIngressSaExpiredStatusRegister_t.bits_1.mssIngressSaExpiredMSW

                        Default = 0x0000

                        SA expired bits 31:16
                        

                 <B>Notes:</B>
                        When set, these bits identify the SA that has expired when the SA PN reaches all-ones saturation. Write these bits to 1 to clear.  */
      unsigned int   mssIngressSaExpiredMSW : 16;    // 1E.8037.F:0  ROS      Default = 0x0000 
                     /* SA expired bits 31:16
                          */
    } bits_1;
    unsigned short word_1;
  };
};


//---------------------------------------------------------------------------------
//! \brief                MSS Ingress SA Threshold Expired Status Register: 1E.8038 
//                  MSS Ingress SA Threshold Expired Status Register: 1E.8038 
//---------------------------------------------------------------------------------
struct mssIngressSaThresholdExpiredStatusRegister_t
{
  union
  {
    struct
    {
                    /*! \brief 1E.8038.F:0 ROS MSS Ingress SA Threshold Expired LSW [F:0]
                        mssIngressSaThresholdExpiredStatusRegister_t.bits_0.mssIngressSaThresholdExpiredLSW

                        Default = 0x0000

                        SA threshold expired bits 15:0
                        

                 <B>Notes:</B>
                        When set, these bits identify the SA that has expired when the SA PN has reached the configured threshold  See SEC Egress PN Threshold [F:0] and  See SEC Egress PN Threshold [1F:10] . Write these bits to 1 to clear.  */
      unsigned int   mssIngressSaThresholdExpiredLSW : 16;    // 1E.8038.F:0  ROS      Default = 0x0000 
                     /* SA threshold expired bits 15:0
                          */
    } bits_0;
    unsigned short word_0;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.8039.F:0 ROS MSS Ingress SA Threshold Expired MSW [1F:10]
                        mssIngressSaThresholdExpiredStatusRegister_t.bits_1.mssIngressSaThresholdExpiredMSW

                        Default = 0x0000

                        SA threshold expired bits 31:16
                        

                 <B>Notes:</B>
                        When set, these bits identify the SA that has expired when the SA PN has reached the configured threshold  See SEC Egress PN Threshold [F:0] and  See SEC Egress PN Threshold [1F:10] . Write these bits to 1 to clear.  */
      unsigned int   mssIngressSaThresholdExpiredMSW : 16;    // 1E.8039.F:0  ROS      Default = 0x0000 
                     /* SA threshold expired bits 31:16
                          */
    } bits_1;
    unsigned short word_1;
  };
};


//---------------------------------------------------------------------------------
//! \brief                MSS Ingress ECC Interrupt Status Register: 1E.803A 
//                  MSS Ingress ECC Interrupt Status Register: 1E.803A 
//---------------------------------------------------------------------------------
struct mssIngressEccInterruptStatusRegister_t
{
  union
  {
    struct
    {
                    /*! \brief 1E.803A.F:0 R/W MSS Ingress SA ECC Error Interrupt LSW [F:0]
                        mssIngressEccInterruptStatusRegister_t.bits_0.mssIngressSaEccErrorInterruptLSW

                        Default = 0x0000

                        SA ECC error interrupt bits 15:0
                        

                 <B>Notes:</B>
                        When set to 1, indicates that an ECC error occured for the SA.  */
      unsigned int   mssIngressSaEccErrorInterruptLSW : 16;    // 1E.803A.F:0  R/W      Default = 0x0000 
                     /* SA ECC error interrupt bits 15:0
                          */
    } bits_0;
    unsigned short word_0;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.803B.F:0 R/W MSS Ingress SA ECC Error Interrupt MSW [1F:10]
                        mssIngressEccInterruptStatusRegister_t.bits_1.mssIngressSaEccErrorInterruptMSW

                        Default = 0x0000

                        SA ECC error interrupt bits 31:16
                        

                 <B>Notes:</B>
                        When set to 1, indicates that an ECC error occured for the SA.  */
      unsigned int   mssIngressSaEccErrorInterruptMSW : 16;    // 1E.803B.F:0  R/W      Default = 0x0000 
                     /* SA ECC error interrupt bits 31:16
                          */
    } bits_1;
    unsigned short word_1;
  };
};


//---------------------------------------------------------------------------------
//! \brief                MSS Ingress IGPRC LUT ECC Error 1 Address Status Register: 1E.803C 
//                  MSS Ingress IGPRC LUT ECC Error 1 Address Status Register: 1E.803C 
//---------------------------------------------------------------------------------
struct mssIngressIgprcLutEccError_1AddressStatusRegister_t
{
  union
  {
    struct
    {
                    /*! \brief 1E.803C.F:0 ROS MSS Ingress IGPRC LUT ECC Error 1 Address LSW [F:0]
                        mssIngressIgprcLutEccError_1AddressStatusRegister_t.bits_0.mssIngressIgprcLutEccError_1AddressLSW

                        Default = 0x0000

                        Classification LUT ECC error 1 address bits 15:0
                        

                 <B>Notes:</B>
                        Ingress Pre-Security classification LUT. 4 LUT's read simultaneously - any could be the ECC cause.
                        LUT Entry Index A {[9:6],1'b0,[5]}
                        LUT Entry Index B {[4:1],1'b0,[0]}
                        [15:10] Set to 0.  */
      unsigned int   mssIngressIgprcLutEccError_1AddressLSW : 16;    // 1E.803C.F:0  ROS      Default = 0x0000 
                     /* Classification LUT ECC error 1 address bits 15:0
                          */
    } bits_0;
    unsigned short word_0;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.803D.F:0 ROS MSS Ingress IGPRC LUT ECC Error 1 Address MSW [1F:10]
                        mssIngressIgprcLutEccError_1AddressStatusRegister_t.bits_1.mssIngressIgprcLutEccError_1AddressMSW

                        Default = 0x0000

                        Classification LUT ECC error 1 address bits 31:16
                        

                 <B>Notes:</B>
                        Set to 0.  */
      unsigned int   mssIngressIgprcLutEccError_1AddressMSW : 16;    // 1E.803D.F:0  ROS      Default = 0x0000 
                     /* Classification LUT ECC error 1 address bits 31:16
                          */
    } bits_1;
    unsigned short word_1;
  };
};


//---------------------------------------------------------------------------------
//! \brief                MSS Ingress IGPRC LUT ECC Error 2 Address Status Register: 1E.803E 
//                  MSS Ingress IGPRC LUT ECC Error 2 Address Status Register: 1E.803E 
//---------------------------------------------------------------------------------
struct mssIngressIgprcLutEccError_2AddressStatusRegister_t
{
  union
  {
    struct
    {
                    /*! \brief 1E.803E.F:0 ROS MSS Ingress IGPRC LUT ECC Error 2 Address LSW [F:0]
                        mssIngressIgprcLutEccError_2AddressStatusRegister_t.bits_0.mssIngressIgprcLutEccError_2AddressLSW

                        Default = 0x0000

                        Classification LUTECC error 1 address bits 15:0
                        

                 <B>Notes:</B>
                        LUT Entry Index C {[9:6],1'b1,[5]}
                        LUT Entry Index D {[4:1],1'b1,[0]}
                        [15:10] Set to 0  */
      unsigned int   mssIngressIgprcLutEccError_2AddressLSW : 16;    // 1E.803E.F:0  ROS      Default = 0x0000 
                     /* Classification LUTECC error 1 address bits 15:0
                          */
    } bits_0;
    unsigned short word_0;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.803F.F:0 ROS MSS Ingress IGPRC LUT ECC Error 2 Address MSW [1F:10]
                        mssIngressIgprcLutEccError_2AddressStatusRegister_t.bits_1.mssIngressIgprcLutEccError_2AddressMSW

                        Default = 0x0000

                        Classification LUTECC error 1 address bits 31:16
                        

                 <B>Notes:</B>
                        Set to 0.  */
      unsigned int   mssIngressIgprcLutEccError_2AddressMSW : 16;    // 1E.803F.F:0  ROS      Default = 0x0000 
                     /* Classification LUTECC error 1 address bits 31:16
                          */
    } bits_1;
    unsigned short word_1;
  };
};


//---------------------------------------------------------------------------------
//! \brief                MSS Ingress IGCTLF LUT ECC Error Address Status Register: 1E.8040 
//                  MSS Ingress IGCTLF LUT ECC Error Address Status Register: 1E.8040 
//---------------------------------------------------------------------------------
struct mssIngressIgctlfLutEccErrorAddressStatusRegister_t
{
  union
  {
    struct
    {
                    /*! \brief 1E.8040.F:0 ROS MSS Ingress IGCTLF LUT ECC Error Address LSW [F:0]
                        mssIngressIgctlfLutEccErrorAddressStatusRegister_t.bits_0.mssIngressIgctlfLutEccErrorAddressLSW

                        Default = 0x0000

                        IGCTLF LUT ECC error address bits 15:0
                        

                 <B>Notes:</B>
                        Ingress Pre-Security MAC control packet filter LUT. 2 LUT's read simultaneously. Either could be ECC cause.
                        [4:0] LUT Entry Index A
                        [9:5] LUT Entry Index B
                        [15:10] set to 0.  */
      unsigned int   mssIngressIgctlfLutEccErrorAddressLSW : 16;    // 1E.8040.F:0  ROS      Default = 0x0000 
                     /* IGCTLF LUT ECC error address bits 15:0
                          */
    } bits_0;
    unsigned short word_0;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.8041.F:0 ROS MSS Ingress IGCTLF LUT ECC Error Address MSW [1F:10]
                        mssIngressIgctlfLutEccErrorAddressStatusRegister_t.bits_1.mssIngressIgctlfLutEccErrorAddressMSW

                        Default = 0x0000

                        IGCTLF LUT ECC error address bits 31:16
                        

                 <B>Notes:</B>
                        Set to 0.  */
      unsigned int   mssIngressIgctlfLutEccErrorAddressMSW : 16;    // 1E.8041.F:0  ROS      Default = 0x0000 
                     /* IGCTLF LUT ECC error address bits 31:16
                          */
    } bits_1;
    unsigned short word_1;
  };
};


//---------------------------------------------------------------------------------
//! \brief                MSS Ingress IGPFMT LUT ECC Error Address Status Register: 1E.8042 
//                  MSS Ingress IGPFMT LUT ECC Error Address Status Register: 1E.8042 
//---------------------------------------------------------------------------------
struct mssIngressIgpfmtLutEccErrorAddressStatusRegister_t
{
  union
  {
    struct
    {
                    /*! \brief 1E.8042.F:0 ROS MSS Ingress IGPFMT LUT ECC Error Address LSW [F:0]
                        mssIngressIgpfmtLutEccErrorAddressStatusRegister_t.bits_0.mssIngressIgpfmtLutEccErrorAddressLSW

                        Default = 0x0000

                        IGPFMT LUT ECC error address bits 15:0
                        

                 <B>Notes:</B>
                        [4:0] LUT Entry Index
                        Index to 32 256-bit SA Keys
                        [15:5] Set to 0  */
      unsigned int   mssIngressIgpfmtLutEccErrorAddressLSW : 16;    // 1E.8042.F:0  ROS      Default = 0x0000 
                     /* IGPFMT LUT ECC error address bits 15:0
                          */
    } bits_0;
    unsigned short word_0;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.8043.F:0 ROS MSS Ingress IGPFMT LUT ECC Error Address MSW [1F:10]
                        mssIngressIgpfmtLutEccErrorAddressStatusRegister_t.bits_1.mssIngressIgpfmtLutEccErrorAddressMSW

                        Default = 0x0000

                        IGPFMT LUT ECC error address bits 31:16
                        

                 <B>Notes:</B>
                        Set to 0.  */
      unsigned int   mssIngressIgpfmtLutEccErrorAddressMSW : 16;    // 1E.8043.F:0  ROS      Default = 0x0000 
                     /* IGPFMT LUT ECC error address bits 31:16
                          */
    } bits_1;
    unsigned short word_1;
  };
};


//---------------------------------------------------------------------------------
//! \brief                MSS Ingress IGPOP LUT ECC Error Address Status Register: 1E.8044 
//                  MSS Ingress IGPOP LUT ECC Error Address Status Register: 1E.8044 
//---------------------------------------------------------------------------------
struct mssIngressIgpopLutEccErrorAddressStatusRegister_t
{
  union
  {
    struct
    {
                    /*! \brief 1E.8044.F:0 ROS MSS Ingress IGPOP LUT ECC Error Address LSW [F:0]
                        mssIngressIgpopLutEccErrorAddressStatusRegister_t.bits_0.mssIngressIgpopLutEccErrorAddressLSW

                        Default = 0x0000

                        IGPOP LUT ECC error address bits 15:0
                        

                 <B>Notes:</B>
                        Ingress post-security packet process Replay/SC/SA LUT.
                        [5:0] LUT Entry Index.
                        Entries 0-31 to 32 128-bit SC Information words.
                        Entries 32-63 to 32 128-bit SA Information words.
                        [15:6] set to 0.  */
      unsigned int   mssIngressIgpopLutEccErrorAddressLSW : 16;    // 1E.8044.F:0  ROS      Default = 0x0000 
                     /* IGPOP LUT ECC error address bits 15:0
                          */
    } bits_0;
    unsigned short word_0;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.8045.F:0 ROS MSS Ingress IGPOP LUT ECC Error Address MSW [1F:10]
                        mssIngressIgpopLutEccErrorAddressStatusRegister_t.bits_1.mssIngressIgpopLutEccErrorAddressMSW

                        Default = 0x0000

                        IGPOP LUT ECC error address bits 31:16
                        

                 <B>Notes:</B>
                        Set to 0.  */
      unsigned int   mssIngressIgpopLutEccErrorAddressMSW : 16;    // 1E.8045.F:0  ROS      Default = 0x0000 
                     /* IGPOP LUT ECC error address bits 31:16
                          */
    } bits_1;
    unsigned short word_1;
  };
};


//---------------------------------------------------------------------------------
//! \brief                MSS Ingress IGPOCTLF LUT ECC Error Address Status Register: 1E.8046 
//                  MSS Ingress IGPOCTLF LUT ECC Error Address Status Register: 1E.8046 
//---------------------------------------------------------------------------------
struct mssIngressIgpoctlfLutEccErrorAddressStatusRegister_t
{
  union
  {
    struct
    {
                    /*! \brief 1E.8046.F:0 ROS MSS Ingress IGPOCTLF LUT ECC Error Address LSW [F:0]
                        mssIngressIgpoctlfLutEccErrorAddressStatusRegister_t.bits_0.mssIngressIgpoctlfLutEccErrorAddressLSW

                        Default = 0x0000

                        IGPOCTLF LUT ECC error address bits 15:0
                        

                 <B>Notes:</B>
                        Ingress Post-Security MAC control packet filter LUT. 2 LUT's read simultaneously. Either could be ECC cause.
                        [4:0] LUT Entry Index A
                        [9:5] LUT Entry Index B
                        [15:10] set to 0  */
      unsigned int   mssIngressIgpoctlfLutEccErrorAddressLSW : 16;    // 1E.8046.F:0  ROS      Default = 0x0000 
                     /* IGPOCTLF LUT ECC error address bits 15:0
                          */
    } bits_0;
    unsigned short word_0;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.8047.F:0 ROS MSS Ingress IGPOCTLF LUT ECC Error Address MSW [1F:10]
                        mssIngressIgpoctlfLutEccErrorAddressStatusRegister_t.bits_1.mssIngressIgpoctlfLutEccErrorAddressMSW

                        Default = 0x0000

                        IGPOCTLF LUT ECC error address bits 31:16
                        

                 <B>Notes:</B>
                        Set to 0.  */
      unsigned int   mssIngressIgpoctlfLutEccErrorAddressMSW : 16;    // 1E.8047.F:0  ROS      Default = 0x0000 
                     /* IGPOCTLF LUT ECC error address bits 31:16
                          */
    } bits_1;
    unsigned short word_1;
  };
};


//---------------------------------------------------------------------------------
//! \brief                MSS Ingress IGPOC LUT ECC Error 1 Address Status Register: 1E.8048 
//                  MSS Ingress IGPOC LUT ECC Error 1 Address Status Register: 1E.8048 
//---------------------------------------------------------------------------------
struct mssIngressIgpocLutEccError_1AddressStatusRegister_t
{
  union
  {
    struct
    {
                    /*! \brief 1E.8048.F:0 ROS MSS Ingress IGPOC LUT ECC Error 1 Address LSW [F:0]
                        mssIngressIgpocLutEccError_1AddressStatusRegister_t.bits_0.mssIngressIgpocLutEccError_1AddressLSW

                        Default = 0x0000

                        IGPOC LUT ECC error address bits 15:0
                        

                 <B>Notes:</B>
                        Ingress IGPOC LUT. 4 LUT's read simultaneously - any could be the ECC cause.
                        LUT Entry Index A {[9:6],1'b0,[5]}
                        LUT Entry Index B {[4:1],1'b0,[0]}
                        [15:10] Set to 0.  */
      unsigned int   mssIngressIgpocLutEccError_1AddressLSW : 16;    // 1E.8048.F:0  ROS      Default = 0x0000 
                     /* IGPOC LUT ECC error address bits 15:0
                          */
    } bits_0;
    unsigned short word_0;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.8049.F:0 ROS MSS Ingress IGPOC LUT ECC Error 1 Address MSW [1F:10]
                        mssIngressIgpocLutEccError_1AddressStatusRegister_t.bits_1.mssIngressIgpocLutEccError_1AddressMSW

                        Default = 0x0000

                        IGPOC LUT ECC error address bits 31:16
                        

                 <B>Notes:</B>
                        Set to 0.  */
      unsigned int   mssIngressIgpocLutEccError_1AddressMSW : 16;    // 1E.8049.F:0  ROS      Default = 0x0000 
                     /* IGPOC LUT ECC error address bits 31:16
                          */
    } bits_1;
    unsigned short word_1;
  };
};


//---------------------------------------------------------------------------------
//! \brief                MSS Ingress IGPOC LUT ECC Error 2 Address Status Register: 1E.804A 
//                  MSS Ingress IGPOC LUT ECC Error 2 Address Status Register: 1E.804A 
//---------------------------------------------------------------------------------
struct mssIngressIgpocLutEccError_2AddressStatusRegister_t
{
  union
  {
    struct
    {
                    /*! \brief 1E.804A.F:0 ROS MSS Ingress IGPOC LUT ECC Error 2 Address LSW [F:0]
                        mssIngressIgpocLutEccError_2AddressStatusRegister_t.bits_0.mssIngressIgpocLutEccError_2AddressLSW

                        Default = 0x0000

                        IGPOC LUT ECC error address bits 15:0
                        

                 <B>Notes:</B>
                        Ingress IGPOC LUT
                        LUT Entry Index C{[9:6],1'b1,[5]}
                        LUT Entry Index D{[4:1],1'b1,[0]}
                        [15:10] Set to 0.  */
      unsigned int   mssIngressIgpocLutEccError_2AddressLSW : 16;    // 1E.804A.F:0  ROS      Default = 0x0000 
                     /* IGPOC LUT ECC error address bits 15:0
                          */
    } bits_0;
    unsigned short word_0;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.804B.F:0 ROS MSS Ingress IGPOC LUT ECC Error 2 Address MSW [1F:10]
                        mssIngressIgpocLutEccError_2AddressStatusRegister_t.bits_1.mssIngressIgpocLutEccError_2AddressMSW

                        Default = 0x0000

                        IGPOC LUT ECC error address bits 31:16
                        

                 <B>Notes:</B>
                        Set to 0.  */
      unsigned int   mssIngressIgpocLutEccError_2AddressMSW : 16;    // 1E.804B.F:0  ROS      Default = 0x0000 
                     /* IGPOC LUT ECC error address bits 31:16
                          */
    } bits_1;
    unsigned short word_1;
  };
};


//---------------------------------------------------------------------------------
//! \brief                MSS Ingress IGMIB ECC Error Address Status Register: 1E.804C 
//                  MSS Ingress IGMIB ECC Error Address Status Register: 1E.804C 
//---------------------------------------------------------------------------------
struct mssIngressIgmibEccErrorAddressStatusRegister_t
{
  union
  {
    struct
    {
                    /*! \brief 1E.804C.F:0 ROS MSS Ingress IGMIB ECC Error Address LSW [F:0]
                        mssIngressIgmibEccErrorAddressStatusRegister_t.bits_0.mssIngressIgmibEccErrorAddressLSW

                        Default = 0x0000

                        IGMIB ECC error address bits 15:0
                        

                 <B>Notes:</B>
                        Egress SMIB. 4 LUT's read simultaneously - any could be the ECC cause. 4 64-bit MIB counters per entry.
                        MIB Counter Entry Index A {[7:0], 2'b00}
                        MIB Counter Entry Index B{[7:0], 2'b01}
                        MIB Counter Entry Index C{[7:0], 2'b10}
                        MIB Counter Entry Index D{[7:0], 2'b11}
                        Only entries 0-96 in use. Only one counter in entry 96 at LSB.
                        [15:8] set to 0.  */
      unsigned int   mssIngressIgmibEccErrorAddressLSW : 16;    // 1E.804C.F:0  ROS      Default = 0x0000 
                     /* IGMIB ECC error address bits 15:0
                          */
    } bits_0;
    unsigned short word_0;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.804D.F:0 ROS MSS Ingress IGMIB ECC Error Address MSW [1F:10]
                        mssIngressIgmibEccErrorAddressStatusRegister_t.bits_1.mssIngressIgmibEccErrorAddressMSW

                        Default = 0x0000

                        IGMIB ECC error address bits 31:16
                        

                 <B>Notes:</B>
                        Set to 0.  */
      unsigned int   mssIngressIgmibEccErrorAddressMSW : 16;    // 1E.804D.F:0  ROS      Default = 0x0000 
                     /* IGMIB ECC error address bits 31:16
                          */
    } bits_1;
    unsigned short word_1;
  };
};


//---------------------------------------------------------------------------------
//! \brief                MSS Ingress ECC Control Register: 1E.804E 
//                  MSS Ingress ECC Control Register: 1E.804E 
//---------------------------------------------------------------------------------
struct mssIngressEccControlRegister_t
{
  union
  {
    struct
    {
                    /*! \brief 1E.804E.0 R/W MSS Ingress ECC Enable
                        mssIngressEccControlRegister_t.bits_0.mssIngressEccEnable

                        Default = 0x0

                        1 = Enable ECC
                        
  */
      unsigned int   mssIngressEccEnable : 1;    // 1E.804E.0  R/W      Default = 0x0 
                     /* 1 = Enable ECC
                          */
      unsigned int   reserved0 : 15;
    } bits_0;
    unsigned short word_0;
  };
};


//---------------------------------------------------------------------------------
//! \brief                MSS Ingress Debug Control Register: 1E.8050 
//                  MSS Ingress Debug Control Register: 1E.8050 
//---------------------------------------------------------------------------------
struct mssIngressDebugControlRegister_t
{
  union
  {
    struct
    {
                    /*! \brief 1E.8050.1:0 R/W MSS Ingress Debug Bus Select [1:0]
                        mssIngressDebugControlRegister_t.bits_0.mssIngressDebugBusSelect

                        Default = 0x0

                        1 = Enable ECC
                        

                 <B>Notes:</B>
                        Unused.  */
      unsigned int   mssIngressDebugBusSelect : 2;    // 1E.8050.1:0  R/W      Default = 0x0 
                     /* 1 = Enable ECC
                          */
      unsigned int   reserved0 : 14;
    } bits_0;
    unsigned short word_0;
  };
};


//---------------------------------------------------------------------------------
//! \brief                MSS Ingress Additional Debug Status Register: 1E.8052 
//                  MSS Ingress Additional Debug Status Register: 1E.8052 
//---------------------------------------------------------------------------------
struct mssIngressAdditionalDebugStatusRegister_t
{
  union
  {
    struct
    {
                    /*! \brief 1E.8052.A:0 ROS MSS Ingress IGPOCTLF LUT Debug [A:0]
                        mssIngressAdditionalDebugStatusRegister_t.bits_0.mssIngressIgpoctlfLutDebug

                        Default = 0x000

                        IGPOCTLF LUT debug
                        

                 <B>Notes:</B>
                        Debug only.  */
      unsigned int   mssIngressIgpoctlfLutDebug : 11;    // 1E.8052.A:0  ROS      Default = 0x000 
                     /* IGPOCTLF LUT debug
                          */
                    /*! \brief 1E.8052.F:B ROS MSS Ingress IGPOC Debug LSB [4:0]
                        mssIngressAdditionalDebugStatusRegister_t.bits_0.mssIngressIgpocDebugLsb

                        Default = 0x00

                        IGPOC LUT debug bits 4:0
                        

                 <B>Notes:</B>
                        Debug only.  */
      unsigned int   mssIngressIgpocDebugLsb : 5;    // 1E.8052.F:B  ROS      Default = 0x00 
                     /* IGPOC LUT debug bits 4:0
                          */
    } bits_0;
    unsigned short word_0;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.8053.E:0 ROS MSS Ingress IGPOC Debug MSB [13:5]
                        mssIngressAdditionalDebugStatusRegister_t.bits_1.mssIngressIgpocDebugMsb

                        Default = 0x0000

                        IGPOC LUT debug bits 19:5
                        

                 <B>Notes:</B>
                        Debug only.  */
      unsigned int   mssIngressIgpocDebugMsb : 15;    // 1E.8053.E:0  ROS      Default = 0x0000 
                     /* IGPOC LUT debug bits 19:5
                          */
      unsigned int   reserved0 : 1;
    } bits_1;
    unsigned short word_1;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.8054.D:0 ROS MSS Ingress IGPOC LUT Debug [D:0]
                        mssIngressAdditionalDebugStatusRegister_t.bits_2.mssIngressIgpocLutDebug

                        Default = 0x0000

                        IGPOC LUT debug
                        

                 <B>Notes:</B>
                        Debug only.  */
      unsigned int   mssIngressIgpocLutDebug : 14;    // 1E.8054.D:0  ROS      Default = 0x0000 
                     /* IGPOC LUT debug
                          */
                    /*! \brief 1E.8054.F:E ROS MSS Ingress IGPOP Debug LSB [1:0]
                        mssIngressAdditionalDebugStatusRegister_t.bits_2.mssIngressIgpopDebugLsb

                        Default = 0x0

                        IGPOP debug bits 1:0
                        

                 <B>Notes:</B>
                        Debug only.  */
      unsigned int   mssIngressIgpopDebugLsb : 2;    // 1E.8054.F:E  ROS      Default = 0x0 
                     /* IGPOP debug bits 1:0
                          */
    } bits_2;
    unsigned int word_2;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.8055.5:0 ROS MSS Ingress IGPOP Debug MSB [7:2]
                        mssIngressAdditionalDebugStatusRegister_t.bits_3.mssIngressIgpopDebugMsb

                        Default = 0x00

                        IGPOP debug bits 7:2
                        

                 <B>Notes:</B>
                        Debug only.  */
      unsigned int   mssIngressIgpopDebugMsb : 6;    // 1E.8055.5:0  ROS      Default = 0x00 
                     /* IGPOP debug bits 7:2
                          */
                    /*! \brief 1E.8055.F:6 ROS MSS Ingress Data Path Debug [9:0]
                        mssIngressAdditionalDebugStatusRegister_t.bits_3.mssIngressDataPathDebug

                        Default = 0x000

                        Data path debug
                        

                 <B>Notes:</B>
                        Debug only.  */
      unsigned int   mssIngressDataPathDebug : 10;    // 1E.8055.F:6  ROS      Default = 0x000 
                     /* Data path debug
                          */
    } bits_3;
    unsigned int word_3;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.8056.A:0 ROS MSS Ingress IGPRCTLF LUT Debug [A:0]
                        mssIngressAdditionalDebugStatusRegister_t.bits_4.mssIngressIgprctlfLutDebug

                        Default = 0x000

                        IGPRCTLF LUT debug
                        

                 <B>Notes:</B>
                        Debug only.  */
      unsigned int   mssIngressIgprctlfLutDebug : 11;    // 1E.8056.A:0  ROS      Default = 0x000 
                     /* IGPRCTLF LUT debug
                          */
                    /*! \brief 1E.8056.F:B ROS MSS Ingress IGPRC LUT Debug LSB [4:0]
                        mssIngressAdditionalDebugStatusRegister_t.bits_4.mssIngressIgprcLutDebugLsb

                        Default = 0x00

                        IGPRC LUT debug bits 4:0
                        

                 <B>Notes:</B>
                        Debug only.  */
      unsigned int   mssIngressIgprcLutDebugLsb : 5;    // 1E.8056.F:B  ROS      Default = 0x00 
                     /* IGPRC LUT debug bits 4:0
                          */
    } bits_4;
    unsigned int word_4;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.8057.7:0 ROS MSS Ingress IGPRC LUT Debug MSB [C:5]
                        mssIngressAdditionalDebugStatusRegister_t.bits_5.mssIngressIgprcLutDebugMsb

                        Default = 0x00

                        IGPRC LUT debug bits 12:5
                        

                 <B>Notes:</B>
                        Debug only.  */
      unsigned int   mssIngressIgprcLutDebugMsb : 8;    // 1E.8057.7:0  ROS      Default = 0x00 
                     /* IGPRC LUT debug bits 12:5
                          */
      unsigned int   reserved0 : 8;
    } bits_5;
    unsigned int word_5;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.8058.B:0 ROS MSS Ingress Lookup Debug [B:0]
                        mssIngressAdditionalDebugStatusRegister_t.bits_6.mssIngressLookupDebug

                        Default = 0x000

                        Lookup debug
                        

                 <B>Notes:</B>
                        Debug only.  */
      unsigned int   mssIngressLookupDebug : 12;    // 1E.8058.B:0  ROS      Default = 0x000 
                     /* Lookup debug
                          */
                    /*! \brief 1E.8058.F:C ROS MSS Ingress Processing Debug LSB [3:0]
                        mssIngressAdditionalDebugStatusRegister_t.bits_6.mssIngressProcessingDebugLsb

                        Default = 0x0

                        Ingress processing debug bits 3:0
                        

                 <B>Notes:</B>
                        Debug only.  */
      unsigned int   mssIngressProcessingDebugLsb : 4;    // 1E.8058.F:C  ROS      Default = 0x0 
                     /* Ingress processing debug bits 3:0
                          */
    } bits_6;
    unsigned int word_6;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.8059.6:0 ROS MSS Ingress Processing Debug MSB [A:4]
                        mssIngressAdditionalDebugStatusRegister_t.bits_7.mssIngressProcessingDebugMsb
                        Default = 0x00
                        Ingress processing debug bits 10:4
                 <B>Notes:</B>
                        Debug only.  */
      unsigned int   mssIngressProcessingDebugMsb : 7;    // 1E.8059.6:0  ROS      Default = 0x00 
                     /* Ingress processing debug bits 10:4
                          */
      unsigned int   reserved0 : 9;
    } bits_7;
    unsigned int word_7;
  };
};

//---------------------------------------------------------------------------------
//                  MSS Ingress LUT Address Control Register: 1E.8080 
//---------------------------------------------------------------------------------
struct mssIngressLutAddressControlRegister_t
{
  union
  {
    struct
    {
                    /*! \brief 1E.8080.8:0 R/W MSS Ingress LUT Address [8:0]
                        mssIngressLutAddressControlRegister_t.bits_0.mssIngressLutAddress
                        Default = 0x000
                        LUT address  */
      unsigned int   mssIngressLutAddress : 9;    // 1E.8080.8:0  R/W      Default = 0x000 
                     /* LUT address
                          */
      unsigned int   reserved0 : 3;
                    /*! \brief 1E.8080.F:C R/W MSS Ingress LUT Select [3:0]
                        mssIngressLutAddressControlRegister_t.bits_0.mssIngressLutSelect
                        Default = 0x0
                        LUT select
                 <B>Notes:</B>
                        0x0 : Ingress Pre-Security MAC Control FIlter (IGPRCTLF) LUT
                        0x1 : Ingress Pre-Security Classification LUT (IGPRC)
                        0x2 : Ingress Packet Format (IGPFMT) SAKey LUT
                        0x3 : Ingress Packet Format (IGPFMT) SC/SA LUT
                        0x4 : Ingress Post-Security Classification LUT (IGPOC)
                        0x5 : Ingress Post-Security MAC Control Filter (IGPOCTLF) LUT
                        0x6 : Ingress MIB (IGMIB)  */
      unsigned int   mssIngressLutSelect : 4;    // 1E.8080.F:C  R/W      Default = 0x0 
                     /* LUT select
                          */
    } bits_0;
    unsigned short word_0;
  };
};


//---------------------------------------------------------------------------------
//                  MSS Ingress LUT Control Register: 1E.8081 
//---------------------------------------------------------------------------------
struct mssIngressLutControlRegister_t
{
  union
  {
    struct
    {
      unsigned int   reserved0 : 14;
                    /*! \brief 1E.8081.E R/W MSS Ingress LUT Read
                        mssIngressLutControlRegister_t.bits_0.mssIngressLutRead
                        Default = 0x0
                        1 = LUT read
                 <B>Notes:</B>
                        Setting this bit to 1, will read the LUT. This bit will automatically clear to 0.  */
      unsigned int   mssIngressLutRead : 1;    // 1E.8081.E  R/W      Default = 0x0 
                     /* 1 = LUT read
                          */
                    /*! \brief 1E.8081.F R/W MSS Ingress LUT Write
                        mssIngressLutControlRegister_t.bits_0.mssIngressLutWrite
                        Default = 0x0
                        1 = LUT write
                 <B>Notes:</B>
                        Setting this bit to 1, will write the LUT. This bit will automatically clear to 0.  */
      unsigned int   mssIngressLutWrite : 1;    // 1E.8081.F  R/W      Default = 0x0 
                     /* 1 = LUT write
                          */
    } bits_0;
    unsigned short word_0;
  };
};

//---------------------------------------------------------------------------------
//                  MSS Ingress LUT Data Control Register: 1E.80A0 
//---------------------------------------------------------------------------------
struct mssIngressLutDataControlRegister_t
{
  union
  {
    struct
    {
                    /*! \brief 1E.80A0.F:0 R/W MSS Ingress LUT Data 0 [F:0]
                        mssIngressLutDataControlRegister_t.bits_0.mssIngressLutData_0
                        Default = 0x0000
                        LUT data bits 15:0
  */
      unsigned int   mssIngressLutData_0 : 16;    // 1E.80A0.F:0  R/W      Default = 0x0000 
                     /* LUT data bits 15:0
                          */
    } bits_0;
    unsigned short word_0;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.80A1.F:0 R/W MSS Ingress LUT Data 1 [1F:10]
                        mssIngressLutDataControlRegister_t.bits_1.mssIngressLutData_1
                        Default = 0x0000
                        LUT data bits 31:16
  */
      unsigned int   mssIngressLutData_1 : 16;    // 1E.80A1.F:0  R/W      Default = 0x0000 
                     /* LUT data bits 31:16
                          */
    } bits_1;
    unsigned short word_1;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.80A2.F:0 R/W MSS Ingress LUT Data 2 [2F:20]
                        mssIngressLutDataControlRegister_t.bits_2.mssIngressLutData_2
                        Default = 0x0000
                        LUT data bits 47:32
  */
      unsigned int   mssIngressLutData_2 : 16;    // 1E.80A2.F:0  R/W      Default = 0x0000 
                     /* LUT data bits 47:32
                          */
    } bits_2;
    unsigned int word_2;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.80A3.F:0 R/W MSS Ingress LUT Data 3 [3F:30]
                        mssIngressLutDataControlRegister_t.bits_3.mssIngressLutData_3
                        Default = 0x0000
                        LUT data bits 63:48
  */
      unsigned int   mssIngressLutData_3 : 16;    // 1E.80A3.F:0  R/W      Default = 0x0000 
                     /* LUT data bits 63:48
                          */
    } bits_3;
    unsigned int word_3;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.80A4.F:0 R/W MSS Ingress LUT Data 4 [4F:40]
                        mssIngressLutDataControlRegister_t.bits_4.mssIngressLutData_4
                        Default = 0x0000
                        LUT data bits 79:64
  */
      unsigned int   mssIngressLutData_4 : 16;    // 1E.80A4.F:0  R/W      Default = 0x0000 
                     /* LUT data bits 79:64
                          */
    } bits_4;
    unsigned int word_4;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.80A5.F:0 R/W MSS Ingress LUT Data 5 [5F:50]
                        mssIngressLutDataControlRegister_t.bits_5.mssIngressLutData_5
                        Default = 0x0000
                        LUT data bits 95:80
  */
      unsigned int   mssIngressLutData_5 : 16;    // 1E.80A5.F:0  R/W      Default = 0x0000 
                     /* LUT data bits 95:80
                          */
    } bits_5;
    unsigned int word_5;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.80A6.F:0 R/W MSS Ingress LUT Data 6 [6F:60]
                        mssIngressLutDataControlRegister_t.bits_6.mssIngressLutData_6
                        Default = 0x0000
                        LUT data bits 111:96
  */
      unsigned int   mssIngressLutData_6 : 16;    // 1E.80A6.F:0  R/W      Default = 0x0000 
                     /* LUT data bits 111:96
                          */
    } bits_6;
    unsigned int word_6;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.80A7.F:0 R/W MSS Ingress LUT Data 7 [7F:70]
                        mssIngressLutDataControlRegister_t.bits_7.mssIngressLutData_7
                        Default = 0x0000
                        LUT data bits 127:112
  */
      unsigned int   mssIngressLutData_7 : 16;    // 1E.80A7.F:0  R/W      Default = 0x0000 
                     /* LUT data bits 127:112
                          */
    } bits_7;
    unsigned int word_7;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.80A8.F:0 R/W MSS Ingress LUT Data 8 [8F:80]
                        mssIngressLutDataControlRegister_t.bits_8.mssIngressLutData_8
                        Default = 0x0000
                        LUT data bits 143:128
  */
      unsigned int   mssIngressLutData_8 : 16;    // 1E.80A8.F:0  R/W      Default = 0x0000 
                     /* LUT data bits 143:128 */
    } bits_8;
    unsigned int word_8;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.80A9.F:0 R/W MSS Ingress LUT Data 9 [9F:90]
                        mssIngressLutDataControlRegister_t.bits_9.mssIngressLutData_9
                        Default = 0x0000
                        LUT data bits 159:144 */
      unsigned int   mssIngressLutData_9 : 16;    // 1E.80A9.F:0  R/W      Default = 0x0000 
                     /* LUT data bits 159:144 */
    } bits_9;
    unsigned int word_9;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.80AA.F:0 R/W MSS Ingress LUT Data 10 [AF:A0]
                        mssIngressLutDataControlRegister_t.bits_10.mssIngressLutData_10
                        Default = 0x0000
                        LUT data bits 175:160
  */
      unsigned int   mssIngressLutData_10 : 16;    // 1E.80AA.F:0  R/W      Default = 0x0000 
                     /* LUT data bits 175:160 */
    } bits_10;
    unsigned short word_10;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.80AB.F:0 R/W MSS Ingress LUT Data 11 [BF:B0]
                        mssIngressLutDataControlRegister_t.bits_11.mssIngressLutData_11
                        Default = 0x0000
                        LUT data bits 191:176  */
      unsigned int   mssIngressLutData_11 : 16;    // 1E.80AB.F:0  R/W      Default = 0x0000 
                     /* LUT data bits 191:176 */
    } bits_11;
    unsigned short word_11;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.80AC.F:0 R/W MSS Ingress LUT Data 12 [CF:C0]
                        mssIngressLutDataControlRegister_t.bits_12.mssIngressLutData_12
                        Default = 0x0000
                        LUT data bits 207:192  */
      unsigned int   mssIngressLutData_12 : 16;    // 1E.80AC.F:0  R/W      Default = 0x0000 
                     /* LUT data bits 207:192 */
    } bits_12;
    unsigned short word_12;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.80AD.F:0 R/W MSS Ingress LUT Data 13 [DF:D0]
                        mssIngressLutDataControlRegister_t.bits_13.mssIngressLutData_13
                        Default = 0x0000
                        LUT data bits 223:208  */
      unsigned int   mssIngressLutData_13 : 16;    // 1E.80AD.F:0  R/W      Default = 0x0000 
                     /* LUT data bits 223:208 */
    } bits_13;
    unsigned short word_13;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.80AE.F:0 R/W MSS Ingress LUT Data 14 [EF:E0]
                        mssIngressLutDataControlRegister_t.bits_14.mssIngressLutData_14
                        Default = 0x0000
                        LUT data bits 239:224  */
      unsigned int   mssIngressLutData_14 : 16;    // 1E.80AE.F:0  R/W      Default = 0x0000 
                     /* LUT data bits 239:224 */
    } bits_14;
    unsigned short word_14;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.80AF.F:0 R/W MSS Ingress LUT Data 15 [FF:F0]
                        mssIngressLutDataControlRegister_t.bits_15.mssIngressLutData_15
                        Default = 0x0000
                        LUT data bits 255:240  */
      unsigned int   mssIngressLutData_15 : 16;    // 1E.80AF.F:0  R/W      Default = 0x0000 
                     /* LUT data bits 255:240 */
    } bits_15;
    unsigned short word_15;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.80B0.F:0 R/W MSS Ingress LUT Data 16 [10F:100]
                        mssIngressLutDataControlRegister_t.bits_16.mssIngressLutData_16
                        Default = 0x0000
                        LUT data bits 271:256  */
      unsigned int   mssIngressLutData_16 : 16;    // 1E.80B0.F:0  R/W      Default = 0x0000 
                     /* LUT data bits 271:256 */
    } bits_16;
    unsigned short word_16;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.80B1.F:0 R/W MSS Ingress LUT Data 17 [11F:110]
                        mssIngressLutDataControlRegister_t.bits_17.mssIngressLutData_17
                        Default = 0x0000
                        LUT data bits 287:272  */
      unsigned int   mssIngressLutData_17 : 16;    // 1E.80B1.F:0  R/W      Default = 0x0000 
                     /* LUT data bits 287:272 */
    } bits_17;
    unsigned short word_17;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.80B2.F:0 R/W MSS Ingress LUT Data 18 [12F:120]
                        mssIngressLutDataControlRegister_t.bits_18.mssIngressLutData_18
                        Default = 0x0000
                        LUT data bits 303:288  */
      unsigned int   mssIngressLutData_18 : 16;    // 1E.80B2.F:0  R/W      Default = 0x0000 
                     /* LUT data bits 303:288 */
    } bits_18;
    unsigned short word_18;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.80B3.F:0 R/W MSS Ingress LUT Data 19 [13F:130]
                        mssIngressLutDataControlRegister_t.bits_19.mssIngressLutData_19
                        Default = 0x0000
                        LUT data bits 319:304  */
      unsigned int   mssIngressLutData_19 : 16;    // 1E.80B3.F:0  R/W      Default = 0x0000 
                     /* LUT data bits 319:304 */
    } bits_19;
    unsigned short word_19;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.80B4.F:0 R/W MSS Ingress LUT Data 20 [14F:140]
                        mssIngressLutDataControlRegister_t.bits_20.mssIngressLutData_20
                        Default = 0x0000
                        LUT data bits 335:320  */
      unsigned int   mssIngressLutData_20 : 16;    // 1E.80B4.F:0  R/W      Default = 0x0000 
                     /* LUT data bits 335:320 */
    } bits_20;
    unsigned int word_20;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.80B5.F:0 R/W MSS Ingress LUT Data 21 [15F:150]
                        mssIngressLutDataControlRegister_t.bits_21.mssIngressLutData_21
                        Default = 0x0000
                        LUT data bits 351:336  */
      unsigned int   mssIngressLutData_21 : 16;    // 1E.80B5.F:0  R/W      Default = 0x0000 
                     /* LUT data bits 351:336 */
    } bits_21;
    unsigned int word_21;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.80B6.F:0 R/W MSS Ingress LUT Data 22 [16F:160]
                        mssIngressLutDataControlRegister_t.bits_22.mssIngressLutData_22
                        Default = 0x0000
                        LUT data bits 367:352 */
      unsigned int   mssIngressLutData_22 : 16;    // 1E.80B6.F:0  R/W      Default = 0x0000 
                     /* LUT data bits 367:352 */
    } bits_22;
    unsigned int word_22;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.80B7.F:0 R/W MSS Ingress LUT Data 23 [17F:170]
                        mssIngressLutDataControlRegister_t.bits_23.mssIngressLutData_23
                        Default = 0x0000
                        LUT data bits 383:368  */
      unsigned int   mssIngressLutData_23 : 16;    // 1E.80B7.F:0  R/W      Default = 0x0000 
                     /* LUT data bits 383:368 */
    } bits_23;
    unsigned int word_23;
  };
};

#endif /* MSS_INGRESS_REGS_HEADER */
