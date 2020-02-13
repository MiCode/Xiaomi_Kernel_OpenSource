#ifndef MSS_EGRESS_REGS_HEADER
#define MSS_EGRESS_REGS_HEADER


#define mssEgressVersionStatusRegister_ADDR 0x00005000
#define mssEgressControlRegister_ADDR 0x00005002
#define mssEgressPrescaleControlRegister_ADDR 0x00005004
#define mssEgressVlanTpid_0Register_ADDR 0x00005008
#define mssEgressVlanTpid_1Register_ADDR 0x0000500A
#define mssEgressVlanControlRegister_ADDR 0x0000500C
#define mssEgressPnControlRegister_ADDR 0x0000500E
#define mssEgressMtuSizeControlRegister_ADDR 0x00005010
#define mssEgressGcmDataInput_0ControlRegister_ADDR 0x00005012
#define mssEgressGcmDataInput_1ControlRegister_ADDR 0x00005014
#define mssEgressGcmDataInput_2ControlRegister_ADDR 0x00005016
#define mssEgressGcmDataInput_3ControlRegister_ADDR 0x00005018
#define mssEgressGcmKey_0ControlRegister_ADDR 0x0000501A
#define mssEgressGcmKey_0ControlRegister_word_REF(word) (0x0000501A
#define mssEgressGcmKey_1ControlRegister_ADDR 0x0000501C
#define mssEgressGcmKey_2ControlRegister_ADDR 0x0000501E
#define mssEgressGcmKey_3ControlRegister_ADDR 0x00005020
#define mssEgressGcmInitialVector_0ControlRegister_ADDR 0x00005022
#define mssEgressGcmInitialVector_1ControlRegister_ADDR 0x00005024
#define mssEgressGcmInitialVector_2ControlRegister_ADDR 0x00005026
#define mssEgressGcmInitialVector_3ControlRegister_ADDR 0x00005028
#define mssEgressGcmHashInput_0ControlRegister_ADDR 0x0000502A
#define mssEgressGcmHashInput_1ControlRegister_ADDR 0x0000502C
#define mssEgressGcmHashInput_2ControlRegister_ADDR 0x0000502E
#define mssEgressGcmHashInput_3ControlRegister_ADDR 0x00005030
#define mssEgressGcmDataOut_0StatusRegister_ADDR 0x00005032
#define mssEgressGcmDataOut_1StatusRegister_ADDR 0x00005034
#define mssEgressGcmDataOut_2StatusRegister_ADDR 0x00005036
#define mssEgressGcmDataOut_3StatusRegister_ADDR 0x00005038
#define mssEgressGcmHashOut_0StatusRegister_ADDR 0x0000503A
#define mssEgressGcmHashOut_1StatusRegister_ADDR 0x0000503C
#define mssEgressGcmHashOut_2StatusRegister_ADDR 0x0000503E
#define mssEgressGcmHashOut_3StatusRegister_ADDR 0x00005040
#define mssEgressBufferDebugStatusRegister_ADDR 0x00005042
#define mssEgressPacketFormatDebugStatusRegister_ADDR 0x00005044
#define mssEgressGcmMultLengthADebugStatusRegister_ADDR 0x00005046
#define mssEgressGcmMultLengthCDebugStatusRegister_ADDR 0x0000504A
#define mssEgressGcmMultDataCountDebugStatusRegister_ADDR 0x0000504E
#define mssEgressGcmCounterIncrmentDebugStatusRegister_ADDR 0x00005050
#define mssEgressGcmBusyDebugStatusRegister_ADDR 0x00005052
#define mssEgressSpareControlRegister_ADDR 0x00005054
#define mssEgressInterruptStatusRegister_ADDR 0x0000505C
#define mssEgressInterruptMaskRegister_ADDR 0x0000505E
#define mssEgressSaExpiredStatusRegister_ADDR 0x00005060
#define mssEgressSaThresholdExpiredStatusRegister_ADDR 0x00005062
#define mssEgressEccInterruptStatusRegister_ADDR 0x00005064
#define mssEgressEgprcLutEccError_1AddressStatusRegister_ADDR 0x00005066
#define mssEgressEgprcLutEccError_2AddressStatusRegister_ADDR 0x00005068
#define mssEgressEgprctlfLutEccErrorAddressStatusRegister_ADDR 0x0000506A
#define mssEgressEgpfmtLutEccErrorAddressStatusRegister_ADDR 0x0000506C
#define mssEgressEgmibEccErrorAddressStatusRegister_ADDR 0x0000506E
#define mssEgressEccControlRegister_ADDR 0x00005070
#define mssEgressDebugControlRegister_ADDR 0x00005072
#define mssEgressDebugStatusRegister_ADDR 0x00005074
#define mssEgressLutAddressControlRegister_ADDR 0x00005080
#define mssEgressLutControlRegister_ADDR 0x00005081
#define mssEgressLutDataControlRegister_ADDR 0x000050A0

//---------------------------------------------------------------------------------
//                  MSS Egress Version Status Register: 1E.5000 
//---------------------------------------------------------------------------------
struct mssEgressVersionStatusRegister_t
{
  union
  {
    struct
    {
                    /*! \brief 1E.5000.F:0 ROS MSS Egress ID [F:0]
                        mssEgressVersionStatusRegister_t.bits_0.mssEgressID
                        Default = 0x0016
                        MSS egress ID
                 <B>Notes:</B>
                        ID  */
      unsigned int   mssEgressID : 16;    // 1E.5000.F:0  ROS      Default = 0x0016 
                     /* MSS egress ID  */
    } bits_0;
    unsigned short word_0;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.5001.7:0 ROS MSS Egress Version [7:0]
                        mssEgressVersionStatusRegister_t.bits_1.mssEgressVersion
                        Default = 0x08
                        MSS egress version */
      unsigned int   mssEgressVersion : 8;    // 1E.5001.7:0  ROS      Default = 0x08 
                     /* MSS egress version
                          */
                    /*! \brief 1E.5001.F:8 ROS MSS Egress Revision [7:0]
                        mssEgressVersionStatusRegister_t.bits_1.mssEgressRevision
                        Default = 0x08
                        MSS egress revision */
      unsigned int   mssEgressRevision : 8;    // 1E.5001.F:8  ROS      Default = 0x08 
                     /* MSS egress revision */
    } bits_1;
    unsigned short word_1;
  };
};

//---------------------------------------------------------------------------------
//                  MSS Egress Control Register: 1E.5002 
//---------------------------------------------------------------------------------
struct mssEgressControlRegister_t
{
  union
  {
    struct
    {
                    /*! \brief 1E.5002.0 R/W MSS Egress Soft Reset
                        mssEgressControlRegister_t.bits_0.mssEgressSoftReset
                        Default = 0x0
                        1 = Soft reset
                 <B>Notes:</B>
                        S/W reset  */
      unsigned int   mssEgressSoftReset : 1;    // 1E.5002.0  R/W      Default = 0x0 
                     /* 1 = Soft reset
                          */
                    /*! \brief 1E.5002.1 R/W MSS Egress Drop KAY Packet
                        mssEgressControlRegister_t.bits_0.mssEgressDropKayPacket
                        Default = 0x0
                        1 = Drop KAY packet
                 <B>Notes:</B>
                        Decides whether KAY packets have to be dropped  */
      unsigned int   mssEgressDropKayPacket : 1;    // 1E.5002.1  R/W      Default = 0x0 
                     /* 1 = Drop KAY packet
                          */
                    /*! \brief 1E.5002.2 R/W MSS Egress Drop EGPRC LUT Miss
                        mssEgressControlRegister_t.bits_0.mssEgressDropEgprcLutMiss
                        Default = 0x0
                        1 = Drop Egress Classification LUT miss packets
                 <B>Notes:</B>
                        Decides whether Egress Pre-Security Classification (EGPRC) LUT miss packets are to be dropped  */
      unsigned int   mssEgressDropEgprcLutMiss : 1;    // 1E.5002.2  R/W      Default = 0x0 
                     /* 1 = Drop Egress Classification LUT miss packets */
                    /*! \brief 1E.5002.3 R/W MSS Egress GCM Start
                        mssEgressControlRegister_t.bits_0.mssEgressGcmStart
                        Default = 0x0
                        1 = Start GCM
                 <B>Notes:</B>
                        Indicates GCM to start  */
      unsigned int   mssEgressGcmStart : 1;    // 1E.5002.3  R/W      Default = 0x0 
                     /* 1 = Start GCM */
                    /*! \brief 1E.5002.4 R/W MSS Egresss GCM Test Mode
                        mssEgressControlRegister_t.bits_0.mssEgresssGcmTestMode
                        Default = 0x0
                        1 = Enable GCM test mode
                 <B>Notes:</B>
                        Enables GCM test mode  */
      unsigned int   mssEgresssGcmTestMode : 1;    // 1E.5002.4  R/W      Default = 0x0 
                     /* 1 = Enable GCM test mode */
                    /*! \brief 1E.5002.5 R/W MSS Egress Unmatched Use SC 0
                        mssEgressControlRegister_t.bits_0.mssEgressUnmatchedUseSc_0
                        Default = 0x0
                        1 = Use SC 0 for unmatched packets
                        0 = Unmatched packets are uncontrolled packets
                 <B>Notes:</B>
                        Use SC-Index 0 as default SC for unmatched packets. Otherwise the packets are treated as uncontrolled packets.  */
      unsigned int   mssEgressUnmatchedUseSc_0 : 1;    // 1E.5002.5  R/W      Default = 0x0 
                     /* 1 = Use SC 0 for unmatched packets
                        0 = Unmatched packets are uncontrolled packets */
                    /*! \brief 1E.5002.6 R/W MSS Egress Drop Invalid SA/SC Packets
                        mssEgressControlRegister_t.bits_0.mssEgressDropInvalidSa_scPackets
                        Default = 0x0
                        1 = Drop invalid SA/SC packets
                 <B>Notes:</B>
                        Enables dropping of invalid SA/SC packets.  */
      unsigned int   mssEgressDropInvalidSa_scPackets : 1;    // 1E.5002.6  R/W      Default = 0x0 
                     /* 1 = Drop invalid SA/SC packets */
                    /*! \brief 1E.5002.7 R/W MSS Egress Explicit SECTag Report Short Length
                        mssEgressControlRegister_t.bits_0.mssEgressExplicitSectagReportShortLength
                        Default = 0x0
                        Reserved
                 <B>Notes:</B>
                        Unused.  */
      unsigned int   mssEgressExplicitSectagReportShortLength : 1;    // 1E.5002.7  R/W      Default = 0x0 
                     /* Reserved */
                    /*! \brief 1E.5002.8 R/W MSS Egress External Classification Enable
                        mssEgressControlRegister_t.bits_0.mssEgressExternalClassificationEnable
                        Default = 0x0
                        1 = Drop EGPRC miss packets
                 <B>Notes:</B>
                        If set, internal classification is bypassed. Should always be set to 0.  */
      unsigned int   mssEgressExternalClassificationEnable : 1;    // 1E.5002.8  R/W      Default = 0x0 
                     /* 1 = Drop EGPRC miss packets */
                    /*! \brief 1E.5002.9 R/W MSS Egress ICV LSB 8 Bytes Enable
                        mssEgressControlRegister_t.bits_0.mssEgressIcvLsb_8BytesEnable
                        Default = 0x0
                        1 = Use LSB
                        0 = Use MSB
                 <B>Notes:</B>
                        This bit selects MSB or LSB 8 bytes selection in the case where the ICV is 8 bytes.
                        0 = MSB is used.  */
      unsigned int   mssEgressIcvLsb_8BytesEnable : 1;    // 1E.5002.9  R/W      Default = 0x0 
                     /* 1 = Use LSB
                        0 = Use MSB  */
                    /*! \brief 1E.5002.A R/W MSS Egress High Priority
                        mssEgressControlRegister_t.bits_0.mssEgressHighPriority
                        Default = 0x0
                        1 = MIB counter clear on read enable
                 <B>Notes:</B>
                        If this bit is set to 1, read is given high priority and the MIB count value becomes 0 after read.  */
      unsigned int   mssEgressHighPriority : 1;    // 1E.5002.A  R/W      Default = 0x0 
                     /* 1 = MIB counter clear on read enable */
                    /*! \brief 1E.5002.B R/W MSS Egress Clear Counter
                        mssEgressControlRegister_t.bits_0.mssEgressClearCounter
                        Default = 0x0
                        1 = Clear all MIB counters
                 <B>Notes:</B>
                        If this bit is set to 1, all MIB counters will be cleared.  */
      unsigned int   mssEgressClearCounter : 1;    // 1E.5002.B  R/W      Default = 0x0 
                     /* 1 = Clear all MIB counters */
                    /*! \brief 1E.5002.C R/W MSS Egress Clear Global Time
                        mssEgressControlRegister_t.bits_0.mssEgressClearGlobalTime
                        Default = 0x0
                        1 = Clear global time
                 <B>Notes:</B>
                        Clear global time.  */
      unsigned int   mssEgressClearGlobalTime : 1;    // 1E.5002.C  R/W      Default = 0x0 
                     /* 1 = Clear global time */
                    /*! \brief 1E.5002.F:D R/W MSS Egress Ethertype Explicit SECTag LSB [2:0]
                        mssEgressControlRegister_t.bits_0.mssEgressEthertypeExplicitSectagLsb
                        Default = 0x0
                        Ethertype for explicit SECTag bits 2:0.
                 <B>Notes:</B>
                        Ethertype for explicity SECTag.  */
      unsigned int   mssEgressEthertypeExplicitSectagLsb : 3;    // 1E.5002.F:D  R/W      Default = 0x0 
                     /* Ethertype for explicit SECTag bits 2:0. */
    } bits_0;
    unsigned short word_0;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.5003.C:0 R/W MSS Egress Ethertype Explicit SECTag MSB  [F:3]
                        mssEgressControlRegister_t.bits_1.mssEgressEthertypeExplicitSectagMsb
                        Default = 0x0000
                        Ethertype for explicit SECTag bits 15:3.
                 <B>Notes:</B>
                        Ethertype for explicity SECTag.  */
      unsigned int   mssEgressEthertypeExplicitSectagMsb : 13;    // 1E.5003.C:0  R/W      Default = 0x0000 
                     /* Ethertype for explicit SECTag bits 15:3. */
      unsigned int   reserved0 : 3;
    } bits_1;
    unsigned short word_1;
  };
};

//---------------------------------------------------------------------------------
//                  MSS Egress Prescale Control Register: 1E.5004 
//---------------------------------------------------------------------------------
struct mssEgressPrescaleControlRegister_t
{
  union
  {
    struct
    {
                    /*! \brief 1E.5004.F:0 R/W MSS Egress Prescale Configuration LSW [F:0]
                        mssEgressPrescaleControlRegister_t.bits_0.mssEgressPrescaleConfigurationLSW
                        Default = 0x5940
                        Prescale register bits 15:0
                 <B>Notes:</B>
                        Unused.  */
      unsigned int   mssEgressPrescaleConfigurationLSW : 16;    // 1E.5004.F:0  R/W      Default = 0x5940 
                     /* Prescale register bits 15:0 */
    } bits_0;
    unsigned short word_0;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.5005.F:0 R/W MSS Egress Prescale Configuration MSW [1F:10]
                        mssEgressPrescaleControlRegister_t.bits_1.mssEgressPrescaleConfigurationMSW
                        Default = 0x0773
                        Prescale register bits 31:16
                 <B>Notes:</B>
                        Unused. */
      unsigned int   mssEgressPrescaleConfigurationMSW : 16;    // 1E.5005.F:0  R/W      Default = 0x0773 
                     /* Prescale register bits 31:16 */
    } bits_1;
    unsigned short word_1;
  };
};

//---------------------------------------------------------------------------------
//                  MSS Egress VLAN TPID 0 Register: 1E.5008 
//---------------------------------------------------------------------------------
struct mssEgressVlanTpid_0Register_t
{
  union
  {
    struct
    {
                    /*! \brief 1E.5008.F:0 R/W MSS Egress VLAN STag TPID [F:0]
                        mssEgressVlanTpid_0Register_t.bits_0.mssEgressVlanStagTpid
                        Default = 0x0000
                        STag TPID 
                        <B>Notes:</B>
                        Service Tag Protocol Identifier (TPID) values to identify a VLAN tag. The " See SEC Egress VLAN CP Tag Parse STag " bit must be set to 1 for the incoming packet's TPID to be parsed.  */
      unsigned int   mssEgressVlanStagTpid : 16;    // 1E.5008.F:0  R/W      Default = 0x0000 
                     /* STag TPID  */
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
//                  MSS Egress VLAN TPID 1 Register: 1E.500A 
//---------------------------------------------------------------------------------
struct mssEgressVlanTpid_1Register_t
{
  union
  {
    struct
    {
                    /*! \brief 1E.500A.F:0 R/W MSS Egress VLAN QTag TPID [F:0]
                        mssEgressVlanTpid_1Register_t.bits_0.mssEgressVlanQtagTpid
                        Default = 0x0000
                        QTag TPID
                        <B>Notes:</B>
                        Customer Tag Protocol Identifier (TPID) values to identify a VLAN tag. The " See SEC Egress VLAN CP Tag Parse QTag " bit must be set to 1 for the incoming packet's TPID to be parsed.  */
      unsigned int   mssEgressVlanQtagTpid : 16;    // 1E.500A.F:0  R/W      Default = 0x0000 
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
//! \brief                MSS Egress VLAN Control Register: 1E.500C 
//                  MSS Egress VLAN Control Register: 1E.500C 
//---------------------------------------------------------------------------------
struct mssEgressVlanControlRegister_t
{
  union
  {
    struct
    {
                    /*! \brief 1E.500C.F:0 R/W MSS Egress VLAN UP Map Table [F:0]
                        mssEgressVlanControlRegister_t.bits_0.mssEgressVlanUpMapTable
                        Default = 0x0000
                        UP Map table bits 15:0
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
      unsigned int   mssEgressVlanUpMapTable : 16;    // 1E.500C.F:0  R/W      Default = 0x0000 
                     /* UP Map table bits 15:0
                          */
    } bits_0;
    unsigned short word_0;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.500D.7:0 R/W MSS Egress VLAN UP Map Table MSW [17:10]
                        mssEgressVlanControlRegister_t.bits_1.mssEgressVlanUpMapTableMSW
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
      unsigned int   mssEgressVlanUpMapTableMSW : 8;    // 1E.500D.7:0  R/W      Default = 0x00 
                     /* UP Map table bits 23:16
                          */
                    /*! \brief 1E.500D.A:8 R/W MSS Egress VLAN UP Default [2:0]
                        mssEgressVlanControlRegister_t.bits_1.mssEgressVlanUpDefault
                        Default = 0x0
                        UP default
                 <B>Notes:</B>
                        User priority default  */
      unsigned int   mssEgressVlanUpDefault : 3;    // 1E.500D.A:8  R/W      Default = 0x0 
                     /* UP default
                          */
                    /*! \brief 1E.500D.B R/W MSS Egress VLAN STag UP Parse Enable
                        mssEgressVlanControlRegister_t.bits_1.mssEgressVlanStagUpParseEnable
                        Default = 0x0
                        VLAN CP Tag STag UP enable
                 <B>Notes:</B>
                        Enable controlled port service VLAN service Tag user priority field parsing.  */
      unsigned int   mssEgressVlanStagUpParseEnable : 1;    // 1E.500D.B  R/W      Default = 0x0 
                     /* VLAN CP Tag STag UP enable
                          */
                    /*! \brief 1E.500D.C R/W MSS Egress VLAN QTag UP Parse Enable
                        mssEgressVlanControlRegister_t.bits_1.mssEgressVlanQtagUpParseEnable
                        Default = 0x0
                        VLAN CP Tag QTag UP enable
                 <B>Notes:</B>
                        Enable controlled port customer VLAN customer Tag user priority field parsing.  */
      unsigned int   mssEgressVlanQtagUpParseEnable : 1;    // 1E.500D.C  R/W      Default = 0x0 
                     /* VLAN CP Tag QTag UP enable
                          */
                    /*! \brief 1E.500D.D R/W MSS Egress VLAN QinQ Parse Enable
                        mssEgressVlanControlRegister_t.bits_1.mssEgressVlanQinqParseEnable
                        Default = 0x0
                        VLAN CP Tag Parse QinQ
                 <B>Notes:</B>
                        Enable controlled port VLAN QinQ Tag parsing. When this bit is set to 1 both the outer and inner VLAN Tags will be parsed.  */
      unsigned int   mssEgressVlanQinqParseEnable : 1;    // 1E.500D.D  R/W      Default = 0x0 
                     /* VLAN CP Tag Parse QinQ
                          */
                    /*! \brief 1E.500D.E R/W MSS Egress VLAN STag Parse Enable
                        mssEgressVlanControlRegister_t.bits_1.mssEgressVlanStagParseEnable
                        Default = 0x0
                        1 = Enable VLAN STag parsing
                 <B>Notes:</B>
                        Enable controlled port VLAN service Tag parsing. When this bit is set to 1, the incoming packets outer TPID will be compared with the configured " See SEC Egress TPID 0 [F:0] " for matching. If the " See SEC Egress VLAN CP Tag Parse QinQ " bit is set to1, this will also be used to compare the incoming packet's inner TPID.  */
      unsigned int   mssEgressVlanStagParseEnable : 1;    // 1E.500D.E  R/W      Default = 0x0 
                     /* 1 = Enable VLAN STag parsing
                          */
                    /*! \brief 1E.500D.F R/W MSS Egress VLAN QTag Parse Enable
                        mssEgressVlanControlRegister_t.bits_1.mssEgressVlanQtagParseEnable
                        Default = 0x0
                        1 = Enable VLAN QTag parsing
                 <B>Notes:</B>
                        Enable controlled port VLAN customer Tag parsing. When this bit is set to 1, the incoming packet's outer TPID will be compared with the configured " See SEC Egress TPID 1 [F:0] " for matching. If the " See SEC Egress VLAN CP Tag Parse QinQ " bit is set to1, this will also be used to compare the incoming packet's inner TPID.  */
      unsigned int   mssEgressVlanQtagParseEnable : 1;    // 1E.500D.F  R/W      Default = 0x0 
                     /* 1 = Enable VLAN QTag parsing
                          */
    } bits_1;
    unsigned short word_1;
  };
};

//---------------------------------------------------------------------------------
//                  MSS Egress PN Control Register: 1E.500E 
//---------------------------------------------------------------------------------
struct mssEgressPnControlRegister_t
{
  union
  {
    struct
    {
                    /*! \brief 1E.500E.F:0 R/W MSS Egress SA PN Threshold LSW [F:0]
                        mssEgressPnControlRegister_t.bits_0.mssEgressSaPnThresholdLSW
                        Default = 0x0000
                        PN threshold bits 15:0
                 <B>Notes:</B>
                        Egress PN threshold to generate SA threshold interrupt.  */
      unsigned int   mssEgressSaPnThresholdLSW : 16;    // 1E.500E.F:0  R/W      Default = 0x0000 
                     /* PN threshold bits 15:0
                          */
    } bits_0;
    unsigned short word_0;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.500F.F:0 R/W MSS Egress SA PN Threshold MSW [1F:10]
                        mssEgressPnControlRegister_t.bits_1.mssEgressSaPnThresholdMSW
                        Default = 0x0000
                        PN threshold bits 31:16
                 <B>Notes:</B>
                        Egress PN threshold to generate SA threshold interrupt.  */
      unsigned int   mssEgressSaPnThresholdMSW : 16;    // 1E.500F.F:0  R/W      Default = 0x0000 
                     /* PN threshold bits 31:16
                          */
    } bits_1;
    unsigned short word_1;
  };
};

//---------------------------------------------------------------------------------
//                  MSS Egress MTU Size Control Register: 1E.5010 
//---------------------------------------------------------------------------------
struct mssEgressMtuSizeControlRegister_t
{
  union
  {
    struct
    {
                    /*! \brief 1E.5010.F:0 R/W MSS Egress Controlled Packet MTU Size [F:0]
                        mssEgressMtuSizeControlRegister_t.bits_0.mssEgressControlledPacketMtuSize
                        Default = 0x05DC
                        Maximum transmission unit for controlled packet
                 <B>Notes:</B>
                        Maximum transmission unit of controlled packet  */
      unsigned int   mssEgressControlledPacketMtuSize : 16;    // 1E.5010.F:0  R/W      Default = 0x05DC 
                     /* Maximum transmission unit for controlled packet
                          */
    } bits_0;
    unsigned short word_0;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.5011.F:0 R/W MSS Egress Uncontrolled Packet MTU Size [F:0]
                        mssEgressMtuSizeControlRegister_t.bits_1.mssEgressUncontrolledPacketMtuSize
                        Default = 0x05DC
                        Maximum transmission unit for uncontrolled packet
                 <B>Notes:</B>
                        Maximum transmission unit of uncontrolled packet  */
      unsigned int   mssEgressUncontrolledPacketMtuSize : 16;    // 1E.5011.F:0  R/W      Default = 0x05DC 
                     /* Maximum transmission unit for uncontrolled packet
                          */
    } bits_1;
    unsigned short word_1;
  };
};

//---------------------------------------------------------------------------------
//                  MSS Egress GCM Data Input 0 Control Register: 1E.5012 
//---------------------------------------------------------------------------------
struct mssEgressGcmDataInput_0ControlRegister_t
{
  union
  {
    struct
    {
                    /*! \brief 1E.5012.F:0 R/W MSS Egress GCM Data Input 0 LSW [F:0]
                        mssEgressGcmDataInput_0ControlRegister_t.bits_0.mssEgressGcmDataInput_0LSW

                        Default = 0x0000

                        GCM data input 0 bits 15:0
                        

                 <B>Notes:</B>
                        32-bit data word given as input to AES-GCM engine  */
      unsigned int   mssEgressGcmDataInput_0LSW : 16;    // 1E.5012.F:0  R/W      Default = 0x0000 
                     /* GCM data input 0 bits 15:0
                          */
    } bits_0;
    unsigned short word_0;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.5013.F:0 R/W MSS Egress GCM Data Input 0 MSW [1F:10]
                        mssEgressGcmDataInput_0ControlRegister_t.bits_1.mssEgressGcmDataInput_0MSW

                        Default = 0x0000

                        GCM data input 0 bits 31:16
                        

                 <B>Notes:</B>
                        32-bit data word given as input to AES-GCM engine  */
      unsigned int   mssEgressGcmDataInput_0MSW : 16;    // 1E.5013.F:0  R/W      Default = 0x0000 
                     /* GCM data input 0 bits 31:16
                          */
    } bits_1;
    unsigned short word_1;
  };
};


//---------------------------------------------------------------------------------
//                  MSS Egress GCM Data Input 1 Control Register: 1E.5014 
//---------------------------------------------------------------------------------
struct mssEgressGcmDataInput_1ControlRegister_t
{
  union
  {
    struct
    {
                    /*! \brief 1E.5014.F:0 R/W MSS Egress GCM Data Input 1 LSW [F:0]
                        mssEgressGcmDataInput_1ControlRegister_t.bits_0.mssEgressGcmDataInput_1LSW
                        Default = 0x0000
                        GCM data input 1 bits 15:0
                 <B>Notes:</B>
                        32-bit data word given as input to AES-GCM engine  */
      unsigned int   mssEgressGcmDataInput_1LSW : 16;    // 1E.5014.F:0  R/W      Default = 0x0000 
                     /* GCM data input 1 bits 15:0
                          */
    } bits_0;
    unsigned short word_0;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.5015.F:0 R/W MSS Egress GCM Data Input 1 MSW [1F:10]
                        mssEgressGcmDataInput_1ControlRegister_t.bits_1.mssEgressGcmDataInput_1MSW
                        Default = 0x0000
                        GCM data input 1 bits 31:16
                 <B>Notes:</B>
                        32-bit data word given as input to AES-GCM engine  */
      unsigned int   mssEgressGcmDataInput_1MSW : 16;    // 1E.5015.F:0  R/W      Default = 0x0000 
                     /* GCM data input 1 bits 31:16
                          */
    } bits_1;
    unsigned short word_1;
  };
};

//---------------------------------------------------------------------------------
//                  MSS Egress GCM Data Input 2 Control Register: 1E.5016 
//---------------------------------------------------------------------------------
struct mssEgressGcmDataInput_2ControlRegister_t
{
  union
  {
    struct
    {
                    /*! \brief 1E.5016.F:0 R/W MSS Egress GCM Data Input 2 LSW [F:0]
                        mssEgressGcmDataInput_2ControlRegister_t.bits_0.mssEgressGcmDataInput_2LSW

                        Default = 0x0000

                        GCM data input 2 bits 15:0
                        

                 <B>Notes:</B>
                        32-bit data word given as input to AES-GCM engine  */
      unsigned int   mssEgressGcmDataInput_2LSW : 16;    // 1E.5016.F:0  R/W      Default = 0x0000 
                     /* GCM data input 2 bits 15:0
                          */
    } bits_0;
    unsigned short word_0;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.5017.F:0 R/W MSS Egress GCM Data Input 2 MSW [1F:10]
                        mssEgressGcmDataInput_2ControlRegister_t.bits_1.mssEgressGcmDataInput_2MSW

                        Default = 0x0000

                        GCM data input 2 bits 31:16
                        

                 <B>Notes:</B>
                        32-bit data word given as input to AES-GCM engine  */
      unsigned int   mssEgressGcmDataInput_2MSW : 16;    // 1E.5017.F:0  R/W      Default = 0x0000 
                     /* GCM data input 2 bits 31:16
                          */
    } bits_1;
    unsigned short word_1;
  };
};


//---------------------------------------------------------------------------------
//                  MSS Egress GCM Data Input 3 Control Register: 1E.5018 
//---------------------------------------------------------------------------------
struct mssEgressGcmDataInput_3ControlRegister_t
{
  union
  {
    struct
    {
                    /*! \brief 1E.5018.F:0 R/W MSS Egress GCM Data Input 3 LSW [F:0]
                        mssEgressGcmDataInput_3ControlRegister_t.bits_0.mssEgressGcmDataInput_3LSW

                        Default = 0x0000

                        GCM data input 3 bits 15:0
                        

                 <B>Notes:</B>
                        32-bit data word given as input to AES-GCM engine  */
      unsigned int   mssEgressGcmDataInput_3LSW : 16;    // 1E.5018.F:0  R/W      Default = 0x0000 
                     /* GCM data input 3 bits 15:0
                          */
    } bits_0;
    unsigned short word_0;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.5019.F:0 R/W MSS Egress GCM Data Input 3 MSW [1F:10]
                        mssEgressGcmDataInput_3ControlRegister_t.bits_1.mssEgressGcmDataInput_3MSW

                        Default = 0x0000

                        GCM data input 3 bits 31:16
                        

                 <B>Notes:</B>
                        32-bit data word given as input to AES-GCM engine  */
      unsigned int   mssEgressGcmDataInput_3MSW : 16;    // 1E.5019.F:0  R/W      Default = 0x0000 
                     /* GCM data input 3 bits 31:16
                          */
    } bits_1;
    unsigned short word_1;
  };
};


//---------------------------------------------------------------------------------
//                  MSS Egress GCM Key 0 Control Register: 1E.501A 
//---------------------------------------------------------------------------------
struct mssEgressGcmKey_0ControlRegister_t
{
  union
  {
    struct
    {
                    /*! \brief 1E.501A.F:0 R/W MSS Egress GCM Key 0 LSW [F:0]
                        mssEgressGcmKey_0ControlRegister_t.bits_0.mssEgressGcmKey_0LSW

                        Default = 0x0000

                        GCM key 0 bits 15:0
                        

                 <B>Notes:</B>
                        Key provided to the AEC-GCM engine  */
      unsigned int   mssEgressGcmKey_0LSW : 16;    // 1E.501A.F:0  R/W      Default = 0x0000 
                     /* GCM key 0 bits 15:0
                          */
    } bits_0;
    unsigned short word_0;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.501B.F:0 R/W MSS Egress GCM Key 0 MSW [1F:10]
                        mssEgressGcmKey_0ControlRegister_t.bits_1.mssEgressGcmKey_0MSW

                        Default = 0x0000

                        GCM key 0 bits 31:16
                        

                 <B>Notes:</B>
                        Key provided to the AEC-GCM engine  */
      unsigned int   mssEgressGcmKey_0MSW : 16;    // 1E.501B.F:0  R/W      Default = 0x0000 
                     /* GCM key 0 bits 31:16
                          */
    } bits_1;
    unsigned short word_1;
  };
};

//---------------------------------------------------------------------------------
//                  MSS Egress GCM Key 1 Control Register: 1E.501C 
//---------------------------------------------------------------------------------
struct mssEgressGcmKey_1ControlRegister_t
{
  union
  {
    struct
    {
                    /*! \brief 1E.501C.F:0 R/W MSS Egress GCM Key 1 LSW [F:0]
                        mssEgressGcmKey_1ControlRegister_t.bits_0.mssEgressGcmKey_1LSW
                        Default = 0x0000
                        GCM key 1 bits 15:0
                 <B>Notes:</B>
                        Key provided to the AEC-GCM engine  */
      unsigned int   mssEgressGcmKey_1LSW : 16;    // 1E.501C.F:0  R/W      Default = 0x0000 
                     /* GCM key 1 bits 15:0
                          */
    } bits_0;
    unsigned short word_0;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.501D.F:0 R/W MSS Egress GCM Key 1 MSW [1F:10]
                        mssEgressGcmKey_1ControlRegister_t.bits_1.mssEgressGcmKey_1MSW
                        Default = 0x0000
                        GCM key 1 bits 31:16
                 <B>Notes:</B>
                        Key provided to the AEC-GCM engine  */
      unsigned int   mssEgressGcmKey_1MSW : 16;    // 1E.501D.F:0  R/W      Default = 0x0000 
                     /* GCM key 1 bits 31:16    */
    } bits_1;
    unsigned short word_1;
  };
};


//---------------------------------------------------------------------------------
//! \brief                MSS Egress GCM Key 2 Control Register: 1E.501E 
//                  MSS Egress GCM Key 2 Control Register: 1E.501E 
//---------------------------------------------------------------------------------
struct mssEgressGcmKey_2ControlRegister_t
{
  union
  {
    struct
    {
                    /*! \brief 1E.501E.F:0 R/W MSS Egress GCM Key 2 LSW [F:0]
                        mssEgressGcmKey_2ControlRegister_t.bits_0.mssEgressGcmKey_2LSW

                        Default = 0x0000

                        GCM key 2 bits 15:0
                        

                 <B>Notes:</B>
                        Key provided to the AEC-GCM engine  */
      unsigned int   mssEgressGcmKey_2LSW : 16;    // 1E.501E.F:0  R/W      Default = 0x0000 
                     /* GCM key 2 bits 15:0
                          */
    } bits_0;
    unsigned short word_0;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.501F.F:0 R/W MSS Egress GCM Key 2 MSW [1F:10]
                        mssEgressGcmKey_2ControlRegister_t.bits_1.mssEgressGcmKey_2MSW

                        Default = 0x0000

                        GCM key 2 bits 31:16
                        

                 <B>Notes:</B>
                        Key provided to the AEC-GCM engine  */
      unsigned int   mssEgressGcmKey_2MSW : 16;    // 1E.501F.F:0  R/W      Default = 0x0000 
                     /* GCM key 2 bits 31:16
                          */
    } bits_1;
    unsigned short word_1;
  };
};


//---------------------------------------------------------------------------------
//! \brief                MSS Egress GCM Key 3 Control Register: 1E.5020 
//                  MSS Egress GCM Key 3 Control Register: 1E.5020 
//---------------------------------------------------------------------------------
struct mssEgressGcmKey_3ControlRegister_t
{
  union
  {
    struct
    {
                    /*! \brief 1E.5020.F:0 R/W MSS Egress GCM Key 3 LSW [F:0]
                        mssEgressGcmKey_3ControlRegister_t.bits_0.mssEgressGcmKey_3LSW

                        Default = 0x0000

                        GCM key 3 bits 15:0
                        

                 <B>Notes:</B>
                        Key provided to the AEC-GCM engine  */
      unsigned int   mssEgressGcmKey_3LSW : 16;    // 1E.5020.F:0  R/W      Default = 0x0000 
                     /* GCM key 3 bits 15:0
                          */
    } bits_0;
    unsigned short word_0;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.5021.F:0 R/W MSS Egress GCM Key 3 MSW [1F:10]
                        mssEgressGcmKey_3ControlRegister_t.bits_1.mssEgressGcmKey_3MSW

                        Default = 0x0000

                        GCM key 3 bits 31:16
                        

                 <B>Notes:</B>
                        Key provided to the AEC-GCM engine  */
      unsigned int   mssEgressGcmKey_3MSW : 16;    // 1E.5021.F:0  R/W      Default = 0x0000 
                     /* GCM key 3 bits 31:16
                          */
    } bits_1;
    unsigned short word_1;
  };
};


//---------------------------------------------------------------------------------
//! \brief                MSS Egress GCM Initial Vector 0 Control Register: 1E.5022 
//                  MSS Egress GCM Initial Vector 0 Control Register: 1E.5022 
//---------------------------------------------------------------------------------
struct mssEgressGcmInitialVector_0ControlRegister_t
{
  union
  {
    struct
    {
                    /*! \brief 1E.5022.F:0 R/W MSS Egress GCM Initial Vector 0 LSW [F:0]
                        mssEgressGcmInitialVector_0ControlRegister_t.bits_0.mssEgressGcmInitialVector_0LSW

                        Default = 0x0000

                        GCM initial vector 0 bits 15:0
                        

                 <B>Notes:</B>
                        Initial Vector provided to the AEC-GCM engine  */
      unsigned int   mssEgressGcmInitialVector_0LSW : 16;    // 1E.5022.F:0  R/W      Default = 0x0000 
                     /* GCM initial vector 0 bits 15:0
                          */
    } bits_0;
    unsigned short word_0;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.5023.F:0 R/W MSS Egress GCM Initial Vector 0 MSW [1F:10]
                        mssEgressGcmInitialVector_0ControlRegister_t.bits_1.mssEgressGcmInitialVector_0MSW

                        Default = 0x0000

                        GCM initial vector 0 bits 31:16
                        

                 <B>Notes:</B>
                        Initial Vector provided to the AEC-GCM engine  */
      unsigned int   mssEgressGcmInitialVector_0MSW : 16;    // 1E.5023.F:0  R/W      Default = 0x0000 
                     /* GCM initial vector 0 bits 31:16
                          */
    } bits_1;
    unsigned short word_1;
  };
};


//---------------------------------------------------------------------------------
//! \brief                MSS Egress GCM Initial Vector 1 Control Register: 1E.5024 
//                  MSS Egress GCM Initial Vector 1 Control Register: 1E.5024 
//---------------------------------------------------------------------------------
struct mssEgressGcmInitialVector_1ControlRegister_t
{
  union
  {
    struct
    {
                    /*! \brief 1E.5024.F:0 R/W MSS Egress GCM Initial Vector 1 LSW [F:0]
                        mssEgressGcmInitialVector_1ControlRegister_t.bits_0.mssEgressGcmInitialVector_1LSW

                        Default = 0x0000

                        GCM initial vector 1 bits 15:0
                        

                 <B>Notes:</B>
                        Initial Vector provided to the AEC-GCM engine  */
      unsigned int   mssEgressGcmInitialVector_1LSW : 16;    // 1E.5024.F:0  R/W      Default = 0x0000 
                     /* GCM initial vector 1 bits 15:0
                          */
    } bits_0;
    unsigned short word_0;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.5025.F:0 R/W MSS Egress GCM Initial Vector 1 MSW [1F:10]
                        mssEgressGcmInitialVector_1ControlRegister_t.bits_1.mssEgressGcmInitialVector_1MSW

                        Default = 0x0000

                        GCM initial vector 1 bits 31:16
                        

                 <B>Notes:</B>
                        Initial Vector provided to the AEC-GCM engine  */
      unsigned int   mssEgressGcmInitialVector_1MSW : 16;    // 1E.5025.F:0  R/W      Default = 0x0000 
                     /* GCM initial vector 1 bits 31:16
                          */
    } bits_1;
    unsigned short word_1;
  };
};


//---------------------------------------------------------------------------------
//! \brief                MSS Egress GCM Initial Vector 2 Control Register: 1E.5026 
//                  MSS Egress GCM Initial Vector 2 Control Register: 1E.5026 
//---------------------------------------------------------------------------------
struct mssEgressGcmInitialVector_2ControlRegister_t
{
  union
  {
    struct
    {
                    /*! \brief 1E.5026.F:0 R/W MSS Egress GCM Initial Vector 2 LSW [F:0]
                        mssEgressGcmInitialVector_2ControlRegister_t.bits_0.mssEgressGcmInitialVector_2LSW

                        Default = 0x0000

                        GCM initial vector 2 bits 15:0
                        

                 <B>Notes:</B>
                        Initial Vector provided to the AEC-GCM engine  */
      unsigned int   mssEgressGcmInitialVector_2LSW : 16;    // 1E.5026.F:0  R/W      Default = 0x0000 
                     /* GCM initial vector 2 bits 15:0
                          */
    } bits_0;
    unsigned short word_0;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.5027.F:0 R/W MSS Egress GCM Initial Vector 2 MSW [1F:10]
                        mssEgressGcmInitialVector_2ControlRegister_t.bits_1.mssEgressGcmInitialVector_2MSW

                        Default = 0x0000

                        GCM initial vector 2 bits 31:16
                        

                 <B>Notes:</B>
                        Initial Vector provided to the AEC-GCM engine  */
      unsigned int   mssEgressGcmInitialVector_2MSW : 16;    // 1E.5027.F:0  R/W      Default = 0x0000 
                     /* GCM initial vector 2 bits 31:16
                          */
    } bits_1;
    unsigned short word_1;
  };
};


//---------------------------------------------------------------------------------
//! \brief                MSS Egress GCM Initial Vector 3 Control Register: 1E.5028 
//                  MSS Egress GCM Initial Vector 3 Control Register: 1E.5028 
//---------------------------------------------------------------------------------
struct mssEgressGcmInitialVector_3ControlRegister_t
{
  union
  {
    struct
    {
                    /*! \brief 1E.5028.F:0 R/W MSS Egress GCM Initial Vector 3 LSW [F:0]
                        mssEgressGcmInitialVector_3ControlRegister_t.bits_0.mssEgressGcmInitialVector_3LSW

                        Default = 0x0000

                        GCM initial vector 3 bits 15:0
                        

                 <B>Notes:</B>
                        Initial Vector provided to the AEC-GCM engine  */
      unsigned int   mssEgressGcmInitialVector_3LSW : 16;    // 1E.5028.F:0  R/W      Default = 0x0000 
                     /* GCM initial vector 3 bits 15:0
                          */
    } bits_0;
    unsigned short word_0;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.5029.F:0 R/W MSS Egress GCM Initial Vector 3 MSW [1F:10]
                        mssEgressGcmInitialVector_3ControlRegister_t.bits_1.mssEgressGcmInitialVector_3MSW

                        Default = 0x0000

                        GCM initial vector 3 bits 31:16
                        

                 <B>Notes:</B>
                        Initial Vector provided to the AEC-GCM engine  */
      unsigned int   mssEgressGcmInitialVector_3MSW : 16;    // 1E.5029.F:0  R/W      Default = 0x0000 
                     /* GCM initial vector 3 bits 31:16
                          */
    } bits_1;
    unsigned short word_1;
  };
};


//---------------------------------------------------------------------------------
//! \brief                MSS Egress GCM Hash Input 0 Control Register: 1E.502A 
//                  MSS Egress GCM Hash Input 0 Control Register: 1E.502A 
//---------------------------------------------------------------------------------
struct mssEgressGcmHashInput_0ControlRegister_t
{
  union
  {
    struct
    {
                    /*! \brief 1E.502A.F:0 R/W MSS Egress GCM Hash Input 0 LSW [F:0]
                        mssEgressGcmHashInput_0ControlRegister_t.bits_0.mssEgressGcmHashInput_0LSW

                        Default = 0x0000

                        GCM hash input 0 bits 15:0
                        

                 <B>Notes:</B>
                        Hash input provided to the AEC-GCM engine  */
      unsigned int   mssEgressGcmHashInput_0LSW : 16;    // 1E.502A.F:0  R/W      Default = 0x0000 
                     /* GCM hash input 0 bits 15:0
                          */
    } bits_0;
    unsigned short word_0;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.502B.F:0 R/W MSS Egress GCM Hash Input 0 MSW [1F:10]
                        mssEgressGcmHashInput_0ControlRegister_t.bits_1.mssEgressGcmHashInput_0MSW

                        Default = 0x0000

                        GCM hash input 0 bits 31:16
                        

                 <B>Notes:</B>
                        Hash input provided to the AEC-GCM engine  */
      unsigned int   mssEgressGcmHashInput_0MSW : 16;    // 1E.502B.F:0  R/W      Default = 0x0000 
                     /* GCM hash input 0 bits 31:16
                          */
    } bits_1;
    unsigned short word_1;
  };
};


//---------------------------------------------------------------------------------
//! \brief                MSS Egress GCM Hash Input 1 Control Register: 1E.502C 
//                  MSS Egress GCM Hash Input 1 Control Register: 1E.502C 
//---------------------------------------------------------------------------------
struct mssEgressGcmHashInput_1ControlRegister_t
{
  union
  {
    struct
    {
                    /*! \brief 1E.502C.F:0 R/W MSS Egress GCM Hash Input 1 LSW [F:0]
                        mssEgressGcmHashInput_1ControlRegister_t.bits_0.mssEgressGcmHashInput_1LSW

                        Default = 0x0000

                        GCM hash input 1 bits 15:0
                        

                 <B>Notes:</B>
                        Hash input provided to the AEC-GCM engine  */
      unsigned int   mssEgressGcmHashInput_1LSW : 16;    // 1E.502C.F:0  R/W      Default = 0x0000 
                     /* GCM hash input 1 bits 15:0
                          */
    } bits_0;
    unsigned short word_0;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.502D.F:0 R/W MSS Egress GCM Hash Input 1 MSW [1F:10]
                        mssEgressGcmHashInput_1ControlRegister_t.bits_1.mssEgressGcmHashInput_1MSW

                        Default = 0x0000

                        GCM hash input 1 bits 31:16
                        

                 <B>Notes:</B>
                        Hash input provided to the AEC-GCM engine  */
      unsigned int   mssEgressGcmHashInput_1MSW : 16;    // 1E.502D.F:0  R/W      Default = 0x0000 
                     /* GCM hash input 1 bits 31:16
                          */
    } bits_1;
    unsigned short word_1;
  };
};


//---------------------------------------------------------------------------------
//! \brief                MSS Egress GCM Hash Input 2 Control Register: 1E.502E 
//                  MSS Egress GCM Hash Input 2 Control Register: 1E.502E 
//---------------------------------------------------------------------------------
struct mssEgressGcmHashInput_2ControlRegister_t
{
  union
  {
    struct
    {
                    /*! \brief 1E.502E.F:0 R/W MSS Egress GCM Hash Input 2 LSW [F:0]
                        mssEgressGcmHashInput_2ControlRegister_t.bits_0.mssEgressGcmHashInput_2LSW

                        Default = 0x0000

                        GCM hash input 2 bits 15:0
                        

                 <B>Notes:</B>
                        Hash input provided to the AEC-GCM engine  */
      unsigned int   mssEgressGcmHashInput_2LSW : 16;    // 1E.502E.F:0  R/W      Default = 0x0000 
                     /* GCM hash input 2 bits 15:0
                          */
    } bits_0;
    unsigned short word_0;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.502F.F:0 R/W MSS Egress GCM Hash Input 2 MSW [1F:10]
                        mssEgressGcmHashInput_2ControlRegister_t.bits_1.mssEgressGcmHashInput_2MSW

                        Default = 0x0000

                        GCM hash input 2 bits 31:16
                        

                 <B>Notes:</B>
                        Hash input provided to the AEC-GCM engine  */
      unsigned int   mssEgressGcmHashInput_2MSW : 16;    // 1E.502F.F:0  R/W      Default = 0x0000 
                     /* GCM hash input 2 bits 31:16
                          */
    } bits_1;
    unsigned short word_1;
  };
};


//---------------------------------------------------------------------------------
//! \brief                MSS Egress GCM Hash Input 3 Control Register: 1E.5030 
//                  MSS Egress GCM Hash Input 3 Control Register: 1E.5030 
//---------------------------------------------------------------------------------
struct mssEgressGcmHashInput_3ControlRegister_t
{
  union
  {
    struct
    {
                    /*! \brief 1E.5030.F:0 R/W MSS Egress GCM Hash Input 3 LSW [F:0]
                        mssEgressGcmHashInput_3ControlRegister_t.bits_0.mssEgressGcmHashInput_3LSW

                        Default = 0x0000

                        GCM hash input 3 bits 15:0
                        

                 <B>Notes:</B>
                        Hash input provided to the AEC-GCM engine  */
      unsigned int   mssEgressGcmHashInput_3LSW : 16;    // 1E.5030.F:0  R/W      Default = 0x0000 
                     /* GCM hash input 3 bits 15:0
                          */
    } bits_0;
    unsigned short word_0;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.5031.F:0 R/W MSS Egress GCM Hash Input 3 MSW [1F:10]
                        mssEgressGcmHashInput_3ControlRegister_t.bits_1.mssEgressGcmHashInput_3MSW

                        Default = 0x0000

                        GCM hash input 3 bits 31:16
                        

                 <B>Notes:</B>
                        Hash input provided to the AEC-GCM engine  */
      unsigned int   mssEgressGcmHashInput_3MSW : 16;    // 1E.5031.F:0  R/W      Default = 0x0000 
                     /* GCM hash input 3 bits 31:16
                          */
    } bits_1;
    unsigned short word_1;
  };
};


//---------------------------------------------------------------------------------
//! \brief                MSS Egress GCM Data Out 0 Status Register: 1E.5032 
//                  MSS Egress GCM Data Out 0 Status Register: 1E.5032 
//---------------------------------------------------------------------------------
struct mssEgressGcmDataOut_0StatusRegister_t
{
  union
  {
    struct
    {
                    /*! \brief 1E.5032.F:0 ROS MSS Egress GCM Data Out 0 LSW [F:0]
                        mssEgressGcmDataOut_0StatusRegister_t.bits_0.mssEgressGcmDataOut_0LSW

                        Default = 0x0000

                        GCM data output 0 bits 15:0
                        

                 <B>Notes:</B>
                        Encrypted output of AEC-GCM engine  */
      unsigned int   mssEgressGcmDataOut_0LSW : 16;    // 1E.5032.F:0  ROS      Default = 0x0000 
                     /* GCM data output 0 bits 15:0
                          */
    } bits_0;
    unsigned short word_0;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.5033.F:0 ROS MSS Egress GCM Data Out 0 MSW [1F:10]
                        mssEgressGcmDataOut_0StatusRegister_t.bits_1.mssEgressGcmDataOut_0MSW

                        Default = 0x0000

                        GCM data output 0 bits 31:16
                        

                 <B>Notes:</B>
                        Encrypted output of AEC-GCM engine  */
      unsigned int   mssEgressGcmDataOut_0MSW : 16;    // 1E.5033.F:0  ROS      Default = 0x0000 
                     /* GCM data output 0 bits 31:16
                          */
    } bits_1;
    unsigned short word_1;
  };
};


//---------------------------------------------------------------------------------
//! \brief                MSS Egress GCM Data Out 1 Status Register: 1E.5034 
//                  MSS Egress GCM Data Out 1 Status Register: 1E.5034 
//---------------------------------------------------------------------------------
struct mssEgressGcmDataOut_1StatusRegister_t
{
  union
  {
    struct
    {
                    /*! \brief 1E.5034.F:0 ROS MSS Egress GCM Data Out 1 LSW [F:0]
                        mssEgressGcmDataOut_1StatusRegister_t.bits_0.mssEgressGcmDataOut_1LSW

                        Default = 0x0000

                        GCM data output 1 bits 15:0
                        

                 <B>Notes:</B>
                        Encrypted output of AEC-GCM engine  */
      unsigned int   mssEgressGcmDataOut_1LSW : 16;    // 1E.5034.F:0  ROS      Default = 0x0000 
                     /* GCM data output 1 bits 15:0
                          */
    } bits_0;
    unsigned short word_0;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.5035.F:0 ROS MSS Egress GCM Data Out 1 MSW [1F:10]
                        mssEgressGcmDataOut_1StatusRegister_t.bits_1.mssEgressGcmDataOut_1MSW

                        Default = 0x0000

                        GCM data output 1 bits 31:16
                        

                 <B>Notes:</B>
                        Encrypted output of AEC-GCM engine  */
      unsigned int   mssEgressGcmDataOut_1MSW : 16;    // 1E.5035.F:0  ROS      Default = 0x0000 
                     /* GCM data output 1 bits 31:16
                          */
    } bits_1;
    unsigned short word_1;
  };
};


//---------------------------------------------------------------------------------
//! \brief                MSS Egress GCM Data Out 2 Status Register: 1E.5036 
//                  MSS Egress GCM Data Out 2 Status Register: 1E.5036 
//---------------------------------------------------------------------------------
struct mssEgressGcmDataOut_2StatusRegister_t
{
  union
  {
    struct
    {
                    /*! \brief 1E.5036.F:0 ROS MSS Egress GCM Data Out 2 LSW [F:0]
                        mssEgressGcmDataOut_2StatusRegister_t.bits_0.mssEgressGcmDataOut_2LSW

                        Default = 0x0000

                        GCM data output 2 bits 15:0
                        

                 <B>Notes:</B>
                        Encrypted output of AEC-GCM engine  */
      unsigned int   mssEgressGcmDataOut_2LSW : 16;    // 1E.5036.F:0  ROS      Default = 0x0000 
                     /* GCM data output 2 bits 15:0
                          */
    } bits_0;
    unsigned short word_0;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.5037.F:0 ROS MSS Egress GCM Data Out 2 MSW [1F:10]
                        mssEgressGcmDataOut_2StatusRegister_t.bits_1.mssEgressGcmDataOut_2MSW

                        Default = 0x0000

                        GCM data output 2 bits 31:16
                        

                 <B>Notes:</B>
                        Encrypted output of AEC-GCM engine  */
      unsigned int   mssEgressGcmDataOut_2MSW : 16;    // 1E.5037.F:0  ROS      Default = 0x0000 
                     /* GCM data output 2 bits 31:16
                          */
    } bits_1;
    unsigned short word_1;
  };
};


//---------------------------------------------------------------------------------
//! \brief                MSS Egress GCM Data Out 3 Status Register: 1E.5038 
//                  MSS Egress GCM Data Out 3 Status Register: 1E.5038 
//---------------------------------------------------------------------------------
struct mssEgressGcmDataOut_3StatusRegister_t
{
  union
  {
    struct
    {
                    /*! \brief 1E.5038.F:0 ROS MSS Egress GCM Data Out 3 LSW [F:0]
                        mssEgressGcmDataOut_3StatusRegister_t.bits_0.mssEgressGcmDataOut_3LSW

                        Default = 0x0000

                        GCM data output 3 bits 15:0
                        

                 <B>Notes:</B>
                        Encrypted output of AEC-GCM engine  */
      unsigned int   mssEgressGcmDataOut_3LSW : 16;    // 1E.5038.F:0  ROS      Default = 0x0000 
                     /* GCM data output 3 bits 15:0
                          */
    } bits_0;
    unsigned short word_0;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.5039.F:0 ROS MSS Egress GCM Data Out 3 MSW [1F:10]
                        mssEgressGcmDataOut_3StatusRegister_t.bits_1.mssEgressGcmDataOut_3MSW

                        Default = 0x0000

                        GCM data output 3 bits 31:16
                        

                 <B>Notes:</B>
                        Encrypted output of AEC-GCM engine  */
      unsigned int   mssEgressGcmDataOut_3MSW : 16;    // 1E.5039.F:0  ROS      Default = 0x0000 
                     /* GCM data output 3 bits 31:16
                          */
    } bits_1;
    unsigned short word_1;
  };
};


//---------------------------------------------------------------------------------
//! \brief                MSS Egress GCM Hash Out 0 Status Register: 1E.503A 
//                  MSS Egress GCM Hash Out 0 Status Register: 1E.503A 
//---------------------------------------------------------------------------------
struct mssEgressGcmHashOut_0StatusRegister_t
{
  union
  {
    struct
    {
                    /*! \brief 1E.503A.F:0 ROS MSS Egress GCM Hash Out 0 LSW [F:0]
                        mssEgressGcmHashOut_0StatusRegister_t.bits_0.mssEgressGcmHashOut_0LSW

                        Default = 0x0000

                        GCM hash output 0 bits 15:0
                        

                 <B>Notes:</B>
                        Hash output of AEC-GCM engine  */
      unsigned int   mssEgressGcmHashOut_0LSW : 16;    // 1E.503A.F:0  ROS      Default = 0x0000 
                     /* GCM hash output 0 bits 15:0
                          */
    } bits_0;
    unsigned short word_0;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.503B.F:0 ROS MSS Egress GCM Hash Out 0 MSW [1F:10]
                        mssEgressGcmHashOut_0StatusRegister_t.bits_1.mssEgressGcmHashOut_0MSW

                        Default = 0x0000

                        GCM hash output 0 bits 31:16
                        

                 <B>Notes:</B>
                        Hash output of AEC-GCM engine  */
      unsigned int   mssEgressGcmHashOut_0MSW : 16;    // 1E.503B.F:0  ROS      Default = 0x0000 
                     /* GCM hash output 0 bits 31:16
                          */
    } bits_1;
    unsigned short word_1;
  };
};


//---------------------------------------------------------------------------------
//! \brief                MSS Egress GCM Hash Out 1 Status Register: 1E.503C 
//                  MSS Egress GCM Hash Out 1 Status Register: 1E.503C 
//---------------------------------------------------------------------------------
struct mssEgressGcmHashOut_1StatusRegister_t
{
  union
  {
    struct
    {
                    /*! \brief 1E.503C.F:0 ROS MSS Egress GCM Hash Out 1 LSW [F:0]
                        mssEgressGcmHashOut_1StatusRegister_t.bits_0.mssEgressGcmHashOut_1LSW

                        Default = 0x0000

                        GCM hash output 1 bits 15:0
                        

                 <B>Notes:</B>
                        Hash output of AEC-GCM engine  */
      unsigned int   mssEgressGcmHashOut_1LSW : 16;    // 1E.503C.F:0  ROS      Default = 0x0000 
                     /* GCM hash output 1 bits 15:0
                          */
    } bits_0;
    unsigned short word_0;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.503D.F:0 ROS MSS Egress GCM Hash Out 1 MSW [1F:10]
                        mssEgressGcmHashOut_1StatusRegister_t.bits_1.mssEgressGcmHashOut_1MSW

                        Default = 0x0000

                        GCM hash output 1 bits 31:16
                        

                 <B>Notes:</B>
                        Hash output of AEC-GCM engine  */
      unsigned int   mssEgressGcmHashOut_1MSW : 16;    // 1E.503D.F:0  ROS      Default = 0x0000 
                     /* GCM hash output 1 bits 31:16
                          */
    } bits_1;
    unsigned short word_1;
  };
};


//---------------------------------------------------------------------------------
//! \brief                MSS Egress GCM Hash Out 2 Status Register: 1E.503E 
//                  MSS Egress GCM Hash Out 2 Status Register: 1E.503E 
//---------------------------------------------------------------------------------
struct mssEgressGcmHashOut_2StatusRegister_t
{
  union
  {
    struct
    {
                    /*! \brief 1E.503E.F:0 ROS MSS Egress GCM Hash Out 2 LSW [F:0]
                        mssEgressGcmHashOut_2StatusRegister_t.bits_0.mssEgressGcmHashOut_2LSW

                        Default = 0x0000

                        GCM hash output 2 bits 15:0
                        

                 <B>Notes:</B>
                        Hash output of AEC-GCM engine  */
      unsigned int   mssEgressGcmHashOut_2LSW : 16;    // 1E.503E.F:0  ROS      Default = 0x0000 
                     /* GCM hash output 2 bits 15:0
                          */
    } bits_0;
    unsigned short word_0;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.503F.F:0 ROS MSS Egress GCM Hash Out 2 MSW [1F:10]
                        mssEgressGcmHashOut_2StatusRegister_t.bits_1.mssEgressGcmHashOut_2MSW

                        Default = 0x0000

                        GCM hash output 2 bits 31:16
                        

                 <B>Notes:</B>
                        Hash output of AEC-GCM engine  */
      unsigned int   mssEgressGcmHashOut_2MSW : 16;    // 1E.503F.F:0  ROS      Default = 0x0000 
                     /* GCM hash output 2 bits 31:16
                          */
    } bits_1;
    unsigned short word_1;
  };
};


//---------------------------------------------------------------------------------
//! \brief                MSS Egress GCM Hash Out 3 Status Register: 1E.5040 
//                  MSS Egress GCM Hash Out 3 Status Register: 1E.5040 
//---------------------------------------------------------------------------------
struct mssEgressGcmHashOut_3StatusRegister_t
{
  union
  {
    struct
    {
                    /*! \brief 1E.5040.F:0 ROS MSS Egress GCM Hash Out 3 LSW [F:0]
                        mssEgressGcmHashOut_3StatusRegister_t.bits_0.mssEgressGcmHashOut_3LSW

                        Default = 0x0000

                        GCM hash output 3 bits 15:0
                        

                 <B>Notes:</B>
                        Hash output of AEC-GCM engine  */
      unsigned int   mssEgressGcmHashOut_3LSW : 16;    // 1E.5040.F:0  ROS      Default = 0x0000 
                     /* GCM hash output 3 bits 15:0
                          */
    } bits_0;
    unsigned short word_0;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.5041.F:0 ROS MSS Egress GCM Hash Out 3 MSW [1F:10]
                        mssEgressGcmHashOut_3StatusRegister_t.bits_1.mssEgressGcmHashOut_3MSW

                        Default = 0x0000

                        GCM hash output 3 bits 31:16
                        

                 <B>Notes:</B>
                        Hash output of AEC-GCM engine  */
      unsigned int   mssEgressGcmHashOut_3MSW : 16;    // 1E.5041.F:0  ROS      Default = 0x0000 
                     /* GCM hash output 3 bits 31:16
                          */
    } bits_1;
    unsigned short word_1;
  };
};


//---------------------------------------------------------------------------------
//! \brief                MSS Egress Buffer Debug Status Register: 1E.5042 
//                  MSS Egress Buffer Debug Status Register: 1E.5042 
//---------------------------------------------------------------------------------
struct mssEgressBufferDebugStatusRegister_t
{
  union
  {
    struct
    {
                    /*! \brief 1E.5042.3:0 ROS MSS Egress EOF Insertion Debug [3:0]
                        mssEgressBufferDebugStatusRegister_t.bits_0.mssEgressEofInsertionDebug

                        Default = 0x0

                        Packet format EOF insertion debug
                        

                 <B>Notes:</B>
                        These bits are debug status from the EOF insertion block. This block performs inseriton of bytes at the EOF.
                        Bits 1:0 : Buffer FIFO depth.
                        Bits 3:2 : State machine  */
      unsigned int   mssEgressEofInsertionDebug : 4;    // 1E.5042.3:0  ROS      Default = 0x0 
                     /* Packet format EOF insertion debug
                          */
                    /*! \brief 1E.5042.7:4 ROS MSS Egress Removal Debug [3:0]
                        mssEgressBufferDebugStatusRegister_t.bits_0.mssEgressRemovalDebug

                        Default = 0x0

                        Packet format removal debug
                        

                 <B>Notes:</B>
                        These bits are debug status from the packet removal block. This block performs removal of pad bytes.
                        Bits 1:0 : Buffer FIFO depth.
                        Bits 3:2 : State machine  */
      unsigned int   mssEgressRemovalDebug : 4;    // 1E.5042.7:4  ROS      Default = 0x0 
                     /* Packet format removal debug
                          */
                    /*! \brief 1E.5042.D:8 ROS MSS Egress Removal S/W Debug [5:0]
                        mssEgressBufferDebugStatusRegister_t.bits_0.mssEgressRemovalS_wDebug

                        Default = 0x00

                        Packet format removal debug
                        

                 <B>Notes:</B>
                        Unused. Tied to 0x00.  */
      unsigned int   mssEgressRemovalS_wDebug : 6;    // 1E.5042.D:8  ROS      Default = 0x00 
                     /* Packet format removal debug
                          */
      unsigned int   reserved0 : 2;
    } bits_0;
    unsigned short word_0;
  };
};


//---------------------------------------------------------------------------------
//! \brief                MSS Egress Packet Format Debug Status Register: 1E.5044 
//                  MSS Egress Packet Format Debug Status Register: 1E.5044 
//---------------------------------------------------------------------------------
struct mssEgressPacketFormatDebugStatusRegister_t
{
  union
  {
    struct
    {
                    /*! \brief 1E.5044.7:0 ROS MSS Egress Packet Format Debug [7:0]
                        mssEgressPacketFormatDebugStatusRegister_t.bits_0.mssEgressPacketFormatDebug

                        Default = 0x00

                        Packet format debug status
                        

                 <B>Notes:</B>
                        These bits are debug status from the egress packet format block. This block does the MAC SECTag addition based on the action table. Takes in the action table address. Updates the AN, PN number.
                        3:0 : State machine
                        7:4 : FIFO depth  */
      unsigned int   mssEgressPacketFormatDebug : 8;    // 1E.5044.7:0  ROS      Default = 0x00 
                     /* Packet format debug status
                          */
                    /*! \brief 1E.5044.D:8 ROS MSS Egress SECTag Insertion Debug [5:0]
                        mssEgressPacketFormatDebugStatusRegister_t.bits_0.mssEgressSectagInsertionDebug

                        Default = 0x00

                        Packet format debug status
                        

                 <B>Notes:</B>
                        These bits are debug status from the egress SECTag insertion block. This block performs insertion of the SECTag bytes.
                        2:0 : State machine
                        5:3: FIFO depth  */
      unsigned int   mssEgressSectagInsertionDebug : 6;    // 1E.5044.D:8  ROS      Default = 0x00 
                     /* Packet format debug status
                          */
                    /*! \brief 1E.5044.F:E ROS MSS Egress Pad Insertion Debug LSB [1:0]
                        mssEgressPacketFormatDebugStatusRegister_t.bits_0.mssEgressPadInsertionDebugLsb

                        Default = 0x0

                        Pad insertion debug status
                        

                 <B>Notes:</B>
                        These bits are debug status from the pad insertion block. This block performs insertion of the pad bytes.
                        1:0 : State machine
                        5:2: FIFO depth  */
      unsigned int   mssEgressPadInsertionDebugLsb : 2;    // 1E.5044.F:E  ROS      Default = 0x0 
                     /* Pad insertion debug status
                          */
    } bits_0;
    unsigned short word_0;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.5045.3:0 ROS MSS Egress Pad Insertion Debug MSB [5:2]
                        mssEgressPacketFormatDebugStatusRegister_t.bits_1.mssEgressPadInsertionDebugMsb

                        Default = 0x0

                        Pad insertion debug status
                        

                 <B>Notes:</B>
                        These bits are debug status from the pad insertion block. This block performs insertion of the pad bytes.
                        1:0 : State machine
                        5:2: FIFO depth  */
      unsigned int   mssEgressPadInsertionDebugMsb : 4;    // 1E.5045.3:0  ROS      Default = 0x0 
                     /* Pad insertion debug status
                          */
                    /*! \brief 1E.5045.9:4 ROS MSS Egress SECTag Removal Debug [5:0]
                        mssEgressPacketFormatDebugStatusRegister_t.bits_1.mssEgressSectagRemovalDebug

                        Default = 0x00

                        Packet format debug status
                        

                 <B>Notes:</B>
                        These bits are debug status from the egress SECTag removal block. This block performs removal of the SECTag bytes.
                        1:0 : State machine
                        5:2: FIFO depth  */
      unsigned int   mssEgressSectagRemovalDebug : 6;    // 1E.5045.9:4  ROS      Default = 0x00 
                     /* Packet format debug status
                          */
      unsigned int   reserved0 : 6;
    } bits_1;
    unsigned short word_1;
  };
};


//---------------------------------------------------------------------------------
//! \brief                MSS Egress GCM Mult Length A Debug Status Register: 1E.5046 
//                  MSS Egress GCM Mult Length A Debug Status Register: 1E.5046 
//---------------------------------------------------------------------------------
struct mssEgressGcmMultLengthADebugStatusRegister_t
{
  union
  {
    struct
    {
                    /*! \brief 1E.5046.F:0 ROS MSS Egress GCM Mult Length A Debug 0 [F:0]
                        mssEgressGcmMultLengthADebugStatusRegister_t.bits_0.mssEgressGcmMultLengthADebug_0

                        Default = 0x0000

                        GCM length A bits 15:0
                        

                 <B>Notes:</B>
                        AES debug register  */
      unsigned int   mssEgressGcmMultLengthADebug_0 : 16;    // 1E.5046.F:0  ROS      Default = 0x0000 
                     /* GCM length A bits 15:0
                          */
    } bits_0;
    unsigned short word_0;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.5047.F:0 ROS MSS Egress GCM Mult Length A Debug 1 [1F:10]
                        mssEgressGcmMultLengthADebugStatusRegister_t.bits_1.mssEgressGcmMultLengthADebug_1

                        Default = 0x0000

                        GCM length A bits 31:16
                        

                 <B>Notes:</B>
                        AES debug register  */
      unsigned int   mssEgressGcmMultLengthADebug_1 : 16;    // 1E.5047.F:0  ROS      Default = 0x0000 
                     /* GCM length A bits 31:16
                          */
    } bits_1;
    unsigned short word_1;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.5048.F:0 ROS MSS Egress GCM Mult Length A Debug 2 [2F:20]
                        mssEgressGcmMultLengthADebugStatusRegister_t.bits_2.mssEgressGcmMultLengthADebug_2

                        Default = 0x0000

                        GCM length A bits 47:32
                        

                 <B>Notes:</B>
                        AES debug register  */
      unsigned int   mssEgressGcmMultLengthADebug_2 : 16;    // 1E.5048.F:0  ROS      Default = 0x0000 
                     /* GCM length A bits 47:32
                          */
    } bits_2;
    unsigned int word_2;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.5049.F:0 ROS MSS Egress GCM Mult Length A Debug 3 [3F:30]
                        mssEgressGcmMultLengthADebugStatusRegister_t.bits_3.mssEgressGcmMultLengthADebug_3

                        Default = 0x0000

                        GCM length A bits 63:48
                        

                 <B>Notes:</B>
                        AES debug register  */
      unsigned int   mssEgressGcmMultLengthADebug_3 : 16;    // 1E.5049.F:0  ROS      Default = 0x0000 
                     /* GCM length A bits 63:48
                          */
    } bits_3;
    unsigned int word_3;
  };
};


//---------------------------------------------------------------------------------
//! \brief                MSS Egress GCM Mult Length C Debug Status Register: 1E.504A 
//                  MSS Egress GCM Mult Length C Debug Status Register: 1E.504A 
//---------------------------------------------------------------------------------
struct mssEgressGcmMultLengthCDebugStatusRegister_t
{
  union
  {
    struct
    {
                    /*! \brief 1E.504A.F:0 ROS MSS Egress GCM Mult Length C Debug 0 [F:0]
                        mssEgressGcmMultLengthCDebugStatusRegister_t.bits_0.mssEgressGcmMultLengthCDebug_0

                        Default = 0x0000

                        GCM length A bits 15:0
                        

                 <B>Notes:</B>
                        AES debug register  */
      unsigned int   mssEgressGcmMultLengthCDebug_0 : 16;    // 1E.504A.F:0  ROS      Default = 0x0000 
                     /* GCM length A bits 15:0
                          */
    } bits_0;
    unsigned short word_0;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.504B.F:0 ROS MSS Egress GCM Mult Length C Debug 1 [1F:10]
                        mssEgressGcmMultLengthCDebugStatusRegister_t.bits_1.mssEgressGcmMultLengthCDebug_1

                        Default = 0x0000

                        GCM length A bits 31:16
                        

                 <B>Notes:</B>
                        AES debug register  */
      unsigned int   mssEgressGcmMultLengthCDebug_1 : 16;    // 1E.504B.F:0  ROS      Default = 0x0000 
                     /* GCM length A bits 31:16
                          */
    } bits_1;
    unsigned short word_1;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.504C.F:0 ROS MSS Egress GCM Mult Length C Debug 2 [2F:20]
                        mssEgressGcmMultLengthCDebugStatusRegister_t.bits_2.mssEgressGcmMultLengthCDebug_2

                        Default = 0x0000

                        GCM length A bits 47:32
                        

                 <B>Notes:</B>
                        AES debug register  */
      unsigned int   mssEgressGcmMultLengthCDebug_2 : 16;    // 1E.504C.F:0  ROS      Default = 0x0000 
                     /* GCM length A bits 47:32
                          */
    } bits_2;
    unsigned int word_2;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.504D.F:0 ROS MSS Egress GCM Mult Length C Debug 3 [3F:30]
                        mssEgressGcmMultLengthCDebugStatusRegister_t.bits_3.mssEgressGcmMultLengthCDebug_3

                        Default = 0x0000

                        GCM length A bits 63:48
                        

                 <B>Notes:</B>
                        AES debug register  */
      unsigned int   mssEgressGcmMultLengthCDebug_3 : 16;    // 1E.504D.F:0  ROS      Default = 0x0000 
                     /* GCM length A bits 63:48
                          */
    } bits_3;
    unsigned int word_3;
  };
};


//---------------------------------------------------------------------------------
//! \brief                MSS Egress GCM Mult Data Count Debug Status Register: 1E.504E 
//                  MSS Egress GCM Mult Data Count Debug Status Register: 1E.504E 
//---------------------------------------------------------------------------------
struct mssEgressGcmMultDataCountDebugStatusRegister_t
{
  union
  {
    struct
    {
                    /*! \brief 1E.504E.1:0 ROS MSS Egress GCM Mult Data Count Debug [1:0]
                        mssEgressGcmMultDataCountDebugStatusRegister_t.bits_0.mssEgressGcmMultDataCountDebug

                        Default = 0x0

                        GCM mult data count 
                        

                 <B>Notes:</B>
                        AES debug register  */
      unsigned int   mssEgressGcmMultDataCountDebug : 2;    // 1E.504E.1:0  ROS      Default = 0x0 
                     /* GCM mult data count 
                          */
      unsigned int   reserved0 : 14;
    } bits_0;
    unsigned short word_0;
  };
};


//---------------------------------------------------------------------------------
//! \brief                MSS Egress GCM Counter Incrment Debug Status Register: 1E.5050 
//                  MSS Egress GCM Counter Incrment Debug Status Register: 1E.5050 
//---------------------------------------------------------------------------------
struct mssEgressGcmCounterIncrmentDebugStatusRegister_t
{
  union
  {
    struct
    {
                    /*! \brief 1E.5050.F:0 ROS MSS Egress GCM Counter Increment Debug LSW [F:0]
                        mssEgressGcmCounterIncrmentDebugStatusRegister_t.bits_0.mssEgressGcmCounterIncrementDebugLSW

                        Default = 0x0000

                        GCM counter increment bits 15:0
                        

                 <B>Notes:</B>
                        AES debug register  */
      unsigned int   mssEgressGcmCounterIncrementDebugLSW : 16;    // 1E.5050.F:0  ROS      Default = 0x0000 
                     /* GCM counter increment bits 15:0
                          */
    } bits_0;
    unsigned short word_0;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.5051.F:0 ROS MSS Egress GCM Counter Increment Debug MSW [1F:10]
                        mssEgressGcmCounterIncrmentDebugStatusRegister_t.bits_1.mssEgressGcmCounterIncrementDebugMSW

                        Default = 0x0000

                        GCM counter increment bits 31:16
                        

                 <B>Notes:</B>
                        AES debug register  */
      unsigned int   mssEgressGcmCounterIncrementDebugMSW : 16;    // 1E.5051.F:0  ROS      Default = 0x0000 
                     /* GCM counter increment bits 31:16
                          */
    } bits_1;
    unsigned short word_1;
  };
};


//---------------------------------------------------------------------------------
//! \brief                MSS Egress GCM Busy Debug Status Register: 1E.5052 
//                  MSS Egress GCM Busy Debug Status Register: 1E.5052 
//---------------------------------------------------------------------------------
struct mssEgressGcmBusyDebugStatusRegister_t
{
  union
  {
    struct
    {
                    /*! \brief 1E.5052.0 ROS MSS Egress Datapath Busy
                        mssEgressGcmBusyDebugStatusRegister_t.bits_0.mssEgressDatapathBusy

                        Default = 0x0

                        1 = Busy
                        

                 <B>Notes:</B>
                        AES debug register  */
      unsigned int   mssEgressDatapathBusy : 1;    // 1E.5052.0  ROS      Default = 0x0 
                     /* 1 = Busy
                          */
                    /*! \brief 1E.5052.1 ROS MSS Egress Packet Format Busy
                        mssEgressGcmBusyDebugStatusRegister_t.bits_0.mssEgressPacketFormatBusy

                        Default = 0x0

                        1 = Busy
                        

                 <B>Notes:</B>
                        AES debug register  */
      unsigned int   mssEgressPacketFormatBusy : 1;    // 1E.5052.1  ROS      Default = 0x0 
                     /* 1 = Busy
                          */
                    /*! \brief 1E.5052.2 ROS MSS Egress AES Counter Busy
                        mssEgressGcmBusyDebugStatusRegister_t.bits_0.mssEgressAesCounterBusy

                        Default = 0x0

                        1 = Busy
                        

                 <B>Notes:</B>
                        AES debug register  */
      unsigned int   mssEgressAesCounterBusy : 1;    // 1E.5052.2  ROS      Default = 0x0 
                     /* 1 = Busy
                          */
                    /*! \brief 1E.5052.3 ROS MSS Egress Post GCM Buffer Busy
                        mssEgressGcmBusyDebugStatusRegister_t.bits_0.mssEgressPostGcmBufferBusy

                        Default = 0x0

                        1 = Busy
                        

                 <B>Notes:</B>
                        AES debug register  */
      unsigned int   mssEgressPostGcmBufferBusy : 1;    // 1E.5052.3  ROS      Default = 0x0 
                     /* 1 = Busy
                          */
                    /*! \brief 1E.5052.4 ROS MSS Egress GCM Buffer Busy
                        mssEgressGcmBusyDebugStatusRegister_t.bits_0.mssEgressGcmBufferBusy

                        Default = 0x0

                        1 = Busy
                        

                 <B>Notes:</B>
                        AES debug register  */
      unsigned int   mssEgressGcmBufferBusy : 1;    // 1E.5052.4  ROS      Default = 0x0 
                     /* 1 = Busy
                          */
                    /*! \brief 1E.5052.5 ROS MSS Egress Lookup Busy
                        mssEgressGcmBusyDebugStatusRegister_t.bits_0.mssEgressLookupBusy

                        Default = 0x0

                        1 = Busy
                        

                 <B>Notes:</B>
                        AES debug register  */
      unsigned int   mssEgressLookupBusy : 1;    // 1E.5052.5  ROS      Default = 0x0 
                     /* 1 = Busy
                          */
      unsigned int   reserved0 : 10;
    } bits_0;
    unsigned short word_0;
  };
};


//---------------------------------------------------------------------------------
//! \brief                MSS Egress Spare Control Register: 1E.5054 
//                  MSS Egress Spare Control Register: 1E.5054 
//---------------------------------------------------------------------------------
struct mssEgressSpareControlRegister_t
{
  union
  {
    struct
    {
                    /*! \brief 1E.5054.F:0 R/W MSS Egress Spare Configuration 1 LSW [F:0]
                        mssEgressSpareControlRegister_t.bits_0.mssEgressSpareConfiguration_1LSW

                        Default = 0x0000

                        Spare configuration 1 bits 15:0
                        

                 <B>Notes:</B>
                        Spare  */
      unsigned int   mssEgressSpareConfiguration_1LSW : 16;    // 1E.5054.F:0  R/W      Default = 0x0000 
                     /* Spare configuration 1 bits 15:0
                          */
    } bits_0;
    unsigned short word_0;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.5055.F:0 R/W MSS Egress Spare Configuration 1 MSW [1F:10]
                        mssEgressSpareControlRegister_t.bits_1.mssEgressSpareConfiguration_1MSW

                        Default = 0x0000

                        Spare configuration 1 bits 31:16
                        

                 <B>Notes:</B>
                        Spare  */
      unsigned int   mssEgressSpareConfiguration_1MSW : 16;    // 1E.5055.F:0  R/W      Default = 0x0000 
                     /* Spare configuration 1 bits 31:16
                          */
    } bits_1;
    unsigned short word_1;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.5056.F:0 R/W MSS Egress Spare Configuration 2 LSW [F:0]
                        mssEgressSpareControlRegister_t.bits_2.mssEgressSpareConfiguration_2LSW

                        Default = 0x0000

                        Spare configuration 2 bits 15:0
                        

                 <B>Notes:</B>
                        Spare  */
      unsigned int   mssEgressSpareConfiguration_2LSW : 16;    // 1E.5056.F:0  R/W      Default = 0x0000 
                     /* Spare configuration 2 bits 15:0
                          */
    } bits_2;
    unsigned int word_2;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.5057.F:0 R/W MSS Egress Spare Configuration 2 MSW [1F:10]
                        mssEgressSpareControlRegister_t.bits_3.mssEgressSpareConfiguration_2MSW

                        Default = 0x0000

                        Spare configuration 2 bits 31:16
                        

                 <B>Notes:</B>
                        Spare  */
      unsigned int   mssEgressSpareConfiguration_2MSW : 16;    // 1E.5057.F:0  R/W      Default = 0x0000 
                     /* Spare configuration 2 bits 31:16
                          */
    } bits_3;
    unsigned int word_3;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.5058.F:0 R/W MSS Egress Spare Configuration 3 LSW [F:0]
                        mssEgressSpareControlRegister_t.bits_4.mssEgressSpareConfiguration_3LSW

                        Default = 0x0000

                        Spare configuration 3 bits 15:0
                        

                 <B>Notes:</B>
                        Spare  */
      unsigned int   mssEgressSpareConfiguration_3LSW : 16;    // 1E.5058.F:0  R/W      Default = 0x0000 
                     /* Spare configuration 3 bits 15:0
                          */
    } bits_4;
    unsigned int word_4;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.5059.F:0 R/W MSS Egress Spare Configuration 3 MSW [1F:10]
                        mssEgressSpareControlRegister_t.bits_5.mssEgressSpareConfiguration_3MSW

                        Default = 0x0000

                        Spare configuration 3 bits 31:16
                        

                 <B>Notes:</B>
                        Spare  */
      unsigned int   mssEgressSpareConfiguration_3MSW : 16;    // 1E.5059.F:0  R/W      Default = 0x0000 
                     /* Spare configuration 3 bits 31:16
                          */
    } bits_5;
    unsigned int word_5;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.505A.F:0 R/W MSS Egress Spare Configuration 4 LSW [F:0]
                        mssEgressSpareControlRegister_t.bits_6.mssEgressSpareConfiguration_4LSW

                        Default = 0x0000

                        Spare configuration 4 bits 15:0
                        

                 <B>Notes:</B>
                        Spare  */
      unsigned int   mssEgressSpareConfiguration_4LSW : 16;    // 1E.505A.F:0  R/W      Default = 0x0000 
                     /* Spare configuration 4 bits 15:0
                          */
    } bits_6;
    unsigned int word_6;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.505B.F:0 R/W MSS Egress Spare Configuration 4 MSW [1F:10]
                        mssEgressSpareControlRegister_t.bits_7.mssEgressSpareConfiguration_4MSW

                        Default = 0x0000

                        Spare configuration 4 bits 31:16
                        

                 <B>Notes:</B>
                        Spare  */
      unsigned int   mssEgressSpareConfiguration_4MSW : 16;    // 1E.505B.F:0  R/W      Default = 0x0000 
                     /* Spare configuration 4 bits 31:16
                          */
    } bits_7;
    unsigned int word_7;
  };
};


//---------------------------------------------------------------------------------
//! \brief                MSS Egress Interrupt Status Register: 1E.505C 
//                  MSS Egress Interrupt Status Register: 1E.505C 
//---------------------------------------------------------------------------------
struct mssEgressInterruptStatusRegister_t
{
  union
  {
    struct
    {
                    /*! \brief 1E.505C.0 COW MSS Egress Master Interrupt
                        mssEgressInterruptStatusRegister_t.bits_0.mssEgressMasterInterrupt

                        Default = 0x0

                        1 = Interrupt
                        

                 <B>Notes:</B>
                        Write to 1 to clear. This bit is set when any one of the above interrupt and the corresponding interrupt enable are both set. The interrupt enable for this bit must also be set for this bit to be set.  */
      unsigned int   mssEgressMasterInterrupt : 1;    // 1E.505C.0  COW      Default = 0x0 
                     /* 1 = Interrupt
                          */
                    /*! \brief 1E.505C.1 COW MSS Egress SA Expired Interrupt
                        mssEgressInterruptStatusRegister_t.bits_0.mssEgressSaExpiredInterrupt

                        Default = 0x0

                        1 = Interrupt
                        

                 <B>Notes:</B>
                        Write to 1 to clear. This bit is set when the SA PN reaches all ones saturation.  */
      unsigned int   mssEgressSaExpiredInterrupt : 1;    // 1E.505C.1  COW      Default = 0x0 
                     /* 1 = Interrupt
                          */
                    /*! \brief 1E.505C.2 COW MSS Egress SA Threshold Expired Interrupt
                        mssEgressInterruptStatusRegister_t.bits_0.mssEgressSaThresholdExpiredInterrupt

                        Default = 0x0

                        1 = Interrupt
                        

                 <B>Notes:</B>
                        Write to 1 to clear. This bit is set when the SA PN reaches the  See SEC Egress PN Threshold [F:0] and  See SEC Egress PN Threshold [1F:10] .  */
      unsigned int   mssEgressSaThresholdExpiredInterrupt : 1;    // 1E.505C.2  COW      Default = 0x0 
                     /* 1 = Interrupt
                          */
                    /*! \brief 1E.505C.3 COW MSS Egress MIB Saturation Interrupt
                        mssEgressInterruptStatusRegister_t.bits_0.mssEgressMibSaturationInterrupt

                        Default = 0x0

                        1 = Interrupt
                        

                 <B>Notes:</B>
                        Write to 1 to clear. This bit is set when the MIB counters reaches all ones saturation.  */
      unsigned int   mssEgressMibSaturationInterrupt : 1;    // 1E.505C.3  COW      Default = 0x0 
                     /* 1 = Interrupt
                          */
                    /*! \brief 1E.505C.4 COW MSS Egress ECC Error Interrupt
                        mssEgressInterruptStatusRegister_t.bits_0.mssEgressEccErrorInterrupt

                        Default = 0x0

                        1 = Interrupt
                        

                 <B>Notes:</B>
                        Write to 1 to clear. This bit is set when anyone of the memories detects an ECC error.  */
      unsigned int   mssEgressEccErrorInterrupt : 1;    // 1E.505C.4  COW      Default = 0x0 
                     /* 1 = Interrupt
                          */
      unsigned int   reserved0 : 11;
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
//! \brief                MSS Egress Interrupt Mask Register: 1E.505E 
//                  MSS Egress Interrupt Mask Register: 1E.505E 
//---------------------------------------------------------------------------------
struct mssEgressInterruptMaskRegister_t
{
  union
  {
    struct
    {
                    /*! \brief 1E.505E.0 COW MSS Egress Master Interrupt Enable
                        mssEgressInterruptMaskRegister_t.bits_0.mssEgressMasterInterruptEnable

                        Default = 0x0

                        1 = Interrupt enabled
                        

                 <B>Notes:</B>
                        Write to 1 to clear.  */
      unsigned int   mssEgressMasterInterruptEnable : 1;    // 1E.505E.0  COW      Default = 0x0 
                     /* 1 = Interrupt enabled
                          */
                    /*! \brief 1E.505E.1 COW MSS Egress SA Expired Interrupt Enable
                        mssEgressInterruptMaskRegister_t.bits_0.mssEgressSaExpiredInterruptEnable

                        Default = 0x0

                        1 = Interrupt enabled
                        

                 <B>Notes:</B>
                        Write to 1 to clear. This bit is set when the SA PN reaches all ones saturation.  */
      unsigned int   mssEgressSaExpiredInterruptEnable : 1;    // 1E.505E.1  COW      Default = 0x0 
                     /* 1 = Interrupt enabled
                          */
                    /*! \brief 1E.505E.2 COW MSS Egress SA Expired Threshold Interrupt Enable
                        mssEgressInterruptMaskRegister_t.bits_0.mssEgressSaExpiredThresholdInterruptEnable

                        Default = 0x0

                        1 = Interrupt enabled
                        

                 <B>Notes:</B>
                        Write to 1 to clear. This bit is set when the SA PN reaches the configured threshold  See SEC Egress PN Threshold [F:0] and  See SEC Egress PN Threshold [1F:10] .  */
      unsigned int   mssEgressSaExpiredThresholdInterruptEnable : 1;    // 1E.505E.2  COW      Default = 0x0 
                     /* 1 = Interrupt enabled
                          */
                    /*! \brief 1E.505E.3 COW MSS Egress MIB Saturation Interrupt Enable
                        mssEgressInterruptMaskRegister_t.bits_0.mssEgressMibSaturationInterruptEnable

                        Default = 0x0

                        1 = Interrupt enabled
                        

                 <B>Notes:</B>
                        Write to 1 to clear. This bit is set when the MIB counters reaches all ones saturation.  */
      unsigned int   mssEgressMibSaturationInterruptEnable : 1;    // 1E.505E.3  COW      Default = 0x0 
                     /* 1 = Interrupt enabled
                          */
                    /*! \brief 1E.505E.4 COW MSS Egress ECC Error Interrupt Enable
                        mssEgressInterruptMaskRegister_t.bits_0.mssEgressEccErrorInterruptEnable

                        Default = 0x0

                        1 = Interrupt enabled
                        

                 <B>Notes:</B>
                        Write to 1 to clear. This bit is set when anyone of the memories detects an ECC error.  */
      unsigned int   mssEgressEccErrorInterruptEnable : 1;    // 1E.505E.4  COW      Default = 0x0 
                     /* 1 = Interrupt enabled
                          */
      unsigned int   reserved0 : 11;
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
//! \brief                MSS Egress SA Expired Status Register: 1E.5060 
//                  MSS Egress SA Expired Status Register: 1E.5060 
//---------------------------------------------------------------------------------
struct mssEgressSaExpiredStatusRegister_t
{
  union
  {
    struct
    {
                    /*! \brief 1E.5060.F:0 COW MSS Egress SA Expired LSW [F:0]
                        mssEgressSaExpiredStatusRegister_t.bits_0.mssEgressSaExpiredLSW

                        Default = 0x0000

                        SA expired bits 15:0
                        

                 <B>Notes:</B>
                        Write these bits to 1 to clear.
                        When set, these bits identify the SA that has expired when the SA PN reaches all-ones saturation.  */
      unsigned int   mssEgressSaExpiredLSW : 16;    // 1E.5060.F:0  COW      Default = 0x0000 
                     /* SA expired bits 15:0
                          */
    } bits_0;
    unsigned short word_0;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.5061.F:0 COW MSS Egress SA Expired MSW [1F:10]
                        mssEgressSaExpiredStatusRegister_t.bits_1.mssEgressSaExpiredMSW

                        Default = 0x0000

                        SA expired bits 31:16
                        

                 <B>Notes:</B>
                        Write these bits to 1 to clear.
                        When set, these bits identify the SA that has expired when the SA PN reaches all-ones saturation.  */
      unsigned int   mssEgressSaExpiredMSW : 16;    // 1E.5061.F:0  COW      Default = 0x0000 
                     /* SA expired bits 31:16
                          */
    } bits_1;
    unsigned short word_1;
  };
};


//---------------------------------------------------------------------------------
//! \brief                MSS Egress SA Threshold Expired Status Register: 1E.5062 
//                  MSS Egress SA Threshold Expired Status Register: 1E.5062 
//---------------------------------------------------------------------------------
struct mssEgressSaThresholdExpiredStatusRegister_t
{
  union
  {
    struct
    {
                    /*! \brief 1E.5062.F:0 COW MSS Egress SA Threshold Expired LSW [F:0]
                        mssEgressSaThresholdExpiredStatusRegister_t.bits_0.mssEgressSaThresholdExpiredLSW

                        Default = 0x0000

                        SA threshold expired bits 15:0
                        

                 <B>Notes:</B>
                        Write these bits to 1 to clear.
                        When set, these bits identify the SA that has expired when the SA PN has reached the configured threshold  See SEC Egress PN Threshold [F:0] and  See SEC Egress PN Threshold [1F:10] .  */
      unsigned int   mssEgressSaThresholdExpiredLSW : 16;    // 1E.5062.F:0  COW      Default = 0x0000 
                     /* SA threshold expired bits 15:0
                          */
    } bits_0;
    unsigned short word_0;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.5063.F:0 COW MSS Egress SA Threshold Expired MSW [1F:10]
                        mssEgressSaThresholdExpiredStatusRegister_t.bits_1.mssEgressSaThresholdExpiredMSW

                        Default = 0x0000

                        SA threshold expired bits 31:16
                        

                 <B>Notes:</B>
                        Write these bits to 1 to clear.
                        When set, these bits identify the SA that has expired when the SA PN has reached the configured threshold  See SEC Egress PN Threshold [F:0] and  See SEC Egress PN Threshold [1F:10] .   */
      unsigned int   mssEgressSaThresholdExpiredMSW : 16;    // 1E.5063.F:0  COW      Default = 0x0000 
                     /* SA threshold expired bits 31:16
                          */
    } bits_1;
    unsigned short word_1;
  };
};


//---------------------------------------------------------------------------------
//! \brief                MSS Egress ECC Interrupt Status Register: 1E.5064 
//                  MSS Egress ECC Interrupt Status Register: 1E.5064 
//---------------------------------------------------------------------------------
struct mssEgressEccInterruptStatusRegister_t
{
  union
  {
    struct
    {
                    /*! \brief 1E.5064.F:0 COW MSS Egress SA ECC Error Interrupt LSW [F:0]
                        mssEgressEccInterruptStatusRegister_t.bits_0.mssEgressSaEccErrorInterruptLSW

                        Default = 0x0000

                        SA ECC error interrupt bits 15:0
                        

                 <B>Notes:</B>
                        Write these bits to 1 to clear.
                        When set to 1, indicates that an ECC error occured for the SA.  */
      unsigned int   mssEgressSaEccErrorInterruptLSW : 16;    // 1E.5064.F:0  COW      Default = 0x0000 
                     /* SA ECC error interrupt bits 15:0
                          */
    } bits_0;
    unsigned short word_0;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.5065.F:0 COW MSS Egress SA ECC Error Interrupt MSW [1F:10]
                        mssEgressEccInterruptStatusRegister_t.bits_1.mssEgressSaEccErrorInterruptMSW

                        Default = 0x0000

                        SA ECC error interrupt bits 31:16
                        

                 <B>Notes:</B>
                        Write these bits to 1 to clear.
                        When set to 1, indicates that an ECC error occured for the SA.  */
      unsigned int   mssEgressSaEccErrorInterruptMSW : 16;    // 1E.5065.F:0  COW      Default = 0x0000 
                     /* SA ECC error interrupt bits 31:16
                          */
    } bits_1;
    unsigned short word_1;
  };
};


//---------------------------------------------------------------------------------
//! \brief                MSS Egress EGPRC LUT ECC Error 1 Address Status Register: 1E.5066 
//                  MSS Egress EGPRC LUT ECC Error 1 Address Status Register: 1E.5066 
//---------------------------------------------------------------------------------
struct mssEgressEgprcLutEccError_1AddressStatusRegister_t
{
  union
  {
    struct
    {
                    /*! \brief 1E.5066.F:0 ROS MSS Egress EGPRC LUT ECC Error 1 Address LSW [F:0]
                        mssEgressEgprcLutEccError_1AddressStatusRegister_t.bits_0.mssEgressEgprcLutEccError_1AddressLSW

                        Default = 0x0000

                        EGPRC LUT ECC error 1 address bits 15:0
                        

                 <B>Notes:</B>
                        Egress Pre-Security Classification LUT. 4 LUT's read simultaneously - any could be the ECC cause.
                        LUT Entry Index A {[9:6],1'b0,[5]}
                        LUT Entry Index B {[4:1],1'b0,[0]}
                        [15:10] Set to 0.  */
      unsigned int   mssEgressEgprcLutEccError_1AddressLSW : 16;    // 1E.5066.F:0  ROS      Default = 0x0000 
                     /* EGPRC LUT ECC error 1 address bits 15:0
                          */
    } bits_0;
    unsigned short word_0;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.5067.F:0 ROS MSS Egress EGPRC LUT ECC Error 1 Address MSW [1F:10]
                        mssEgressEgprcLutEccError_1AddressStatusRegister_t.bits_1.mssEgressEgprcLutEccError_1AddressMSW

                        Default = 0x0000

                        EGPRC LUT ECC error 1 address bits 31:16
                        

                 <B>Notes:</B>
                        Set to 0.  */
      unsigned int   mssEgressEgprcLutEccError_1AddressMSW : 16;    // 1E.5067.F:0  ROS      Default = 0x0000 
                     /* EGPRC LUT ECC error 1 address bits 31:16
                          */
    } bits_1;
    unsigned short word_1;
  };
};


//---------------------------------------------------------------------------------
//! \brief                MSS Egress EGPRC LUT ECC Error 2 Address Status Register: 1E.5068 
//                  MSS Egress EGPRC LUT ECC Error 2 Address Status Register: 1E.5068 
//---------------------------------------------------------------------------------
struct mssEgressEgprcLutEccError_2AddressStatusRegister_t
{
  union
  {
    struct
    {
                    /*! \brief 1E.5068.F:0 ROS MSS Egress EGPRC LUT ECC Error 2 Address LSW [F:0]
                        mssEgressEgprcLutEccError_2AddressStatusRegister_t.bits_0.mssEgressEgprcLutEccError_2AddressLSW

                        Default = 0x0000

                        EGPRC LUT ECC error 2 address bits 15:0
                        

                 <B>Notes:</B>
                        LUT Entry Index C {[9:6],1'b1,[5]}
                        LUT Entry Index D {[4:1],1'b1,[0]}
                        [15:10] Set to 0  */
      unsigned int   mssEgressEgprcLutEccError_2AddressLSW : 16;    // 1E.5068.F:0  ROS      Default = 0x0000 
                     /* EGPRC LUT ECC error 2 address bits 15:0
                          */
    } bits_0;
    unsigned short word_0;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.5069.F:0 ROS MSS Egress EGPRC LUT ECC Error 2 Address MSW [1F:10]
                        mssEgressEgprcLutEccError_2AddressStatusRegister_t.bits_1.mssEgressEgprcLutEccError_2AddressMSW

                        Default = 0x0000

                        EGPRC LUT ECC error 2 address bits 31:16
                        

                 <B>Notes:</B>
                        Set to 0.  */
      unsigned int   mssEgressEgprcLutEccError_2AddressMSW : 16;    // 1E.5069.F:0  ROS      Default = 0x0000 
                     /* EGPRC LUT ECC error 2 address bits 31:16
                          */
    } bits_1;
    unsigned short word_1;
  };
};


//---------------------------------------------------------------------------------
//! \brief                MSS Egress EGPRCTLF LUT ECC Error Address Status Register: 1E.506A 
//                  MSS Egress EGPRCTLF LUT ECC Error Address Status Register: 1E.506A 
//---------------------------------------------------------------------------------
struct mssEgressEgprctlfLutEccErrorAddressStatusRegister_t
{
  union
  {
    struct
    {
                    /*! \brief 1E.506A.F:0 ROS MSS Egress EGPRCTLF LUT ECC Error Address LSW [F:0]
                        mssEgressEgprctlfLutEccErrorAddressStatusRegister_t.bits_0.mssEgressEgprctlfLutEccErrorAddressLSW

                        Default = 0x0000

                        EGPRCTLF LUT ECC error address bits 15:0
                        

                 <B>Notes:</B>
                        2 LUT's read simultaneously. Either could be ECC cause.
                        Bits [4:0] contain the address for the Egress Pre-Security MAC control packet filter (CTLF) LUT.
                        Bits [9:5] contain the address for the Egress Pre-Security MAC control packet filter (CTLF) LUT.
                        Bits [15:10] set to 0.  */
      unsigned int   mssEgressEgprctlfLutEccErrorAddressLSW : 16;    // 1E.506A.F:0  ROS      Default = 0x0000 
                     /* EGPRCTLF LUT ECC error address bits 15:0
                          */
    } bits_0;
    unsigned short word_0;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.506B.F:0 ROS MSS Egress EGPRCTLF LUT ECC Error Address MSW [1F:10]
                        mssEgressEgprctlfLutEccErrorAddressStatusRegister_t.bits_1.mssEgressEgprctlfLutEccErrorAddressMSW

                        Default = 0x0000

                        EGPRCTLF LUT ECC error address bits 31:16
                        

                 <B>Notes:</B>
                        Set to 0.  */
      unsigned int   mssEgressEgprctlfLutEccErrorAddressMSW : 16;    // 1E.506B.F:0  ROS      Default = 0x0000 
                     /* EGPRCTLF LUT ECC error address bits 31:16
                          */
    } bits_1;
    unsigned short word_1;
  };
};


//---------------------------------------------------------------------------------
//! \brief                MSS Egress EGPFMT LUT ECC Error Address Status Register: 1E.506C 
//                  MSS Egress EGPFMT LUT ECC Error Address Status Register: 1E.506C 
//---------------------------------------------------------------------------------
struct mssEgressEgpfmtLutEccErrorAddressStatusRegister_t
{
  union
  {
    struct
    {
                    /*! \brief 1E.506C.F:0 ROS MSS Egress EGPFMT LUT ECC Error Address LSW [F:0]
                        mssEgressEgpfmtLutEccErrorAddressStatusRegister_t.bits_0.mssEgressEgpfmtLutEccErrorAddressLSW

                        Default = 0x0000

                        EGPFMT LUT ECC error address bits 15:0
                        

                 <B>Notes:</B>
                        LUT Entry Index bits [7:0]
                        Entries 0-31: SC Information Words.
                        Entries 32-63: SA Information Words
                        Entries 64-127 SA Keys (each key is stored in two entries +32 apart, e.g., key 0 is at entry 64 and entry 96. Bit 7 is part of ECC address but should always read 0 for only 127 entries.
                        Bits [15:8] set to 0.  */
      unsigned int   mssEgressEgpfmtLutEccErrorAddressLSW : 16;    // 1E.506C.F:0  ROS      Default = 0x0000 
                     /* EGPFMT LUT ECC error address bits 15:0
                          */
    } bits_0;
    unsigned short word_0;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.506D.F:0 ROS MSS Egress EGPFMT LUT ECC Error Address MSW [1F:10]
                        mssEgressEgpfmtLutEccErrorAddressStatusRegister_t.bits_1.mssEgressEgpfmtLutEccErrorAddressMSW

                        Default = 0x0000

                        EGPFMT LUT ECC error address bits 31:16
                        

                 <B>Notes:</B>
                        Set to 0.  */
      unsigned int   mssEgressEgpfmtLutEccErrorAddressMSW : 16;    // 1E.506D.F:0  ROS      Default = 0x0000 
                     /* EGPFMT LUT ECC error address bits 31:16
                          */
    } bits_1;
    unsigned short word_1;
  };
};


//---------------------------------------------------------------------------------
//! \brief                MSS Egress EGMIB ECC Error Address Status Register: 1E.506E 
//                  MSS Egress EGMIB ECC Error Address Status Register: 1E.506E 
//---------------------------------------------------------------------------------
struct mssEgressEgmibEccErrorAddressStatusRegister_t
{
  union
  {
    struct
    {
                    /*! \brief 1E.506E.F:0 ROS MSS Egress EGMIB ECC Error Address LSW [F:0]
                        mssEgressEgmibEccErrorAddressStatusRegister_t.bits_0.mssEgressEgmibEccErrorAddressLSW

                        Default = 0x0000

                        EGMIB ECC error address bits 15:0
                        

                 <B>Notes:</B>
                        2 LUT's read simultaneously. Either could be ECC cause.
                        LUT Entry Index A {[6:0],1'b0}
                        LUT Entry Index B {[6:0],1'b1}
                        Each entry stores 2 64-bit MIB counters
                        Bits 15:7 set to 0.  */
      unsigned int   mssEgressEgmibEccErrorAddressLSW : 16;    // 1E.506E.F:0  ROS      Default = 0x0000 
                     /* EGMIB ECC error address bits 15:0
                          */
    } bits_0;
    unsigned short word_0;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.506F.F:0 ROS MSS Egress EGMIB ECC Error Address MSW [1F:10]
                        mssEgressEgmibEccErrorAddressStatusRegister_t.bits_1.mssEgressEgmibEccErrorAddressMSW

                        Default = 0x0000

                        EGMIB ECC error address bits 31:16
                        

                 <B>Notes:</B>
                        Set to 0.  */
      unsigned int   mssEgressEgmibEccErrorAddressMSW : 16;    // 1E.506F.F:0  ROS      Default = 0x0000 
                     /* EGMIB ECC error address bits 31:16
                          */
    } bits_1;
    unsigned short word_1;
  };
};


//---------------------------------------------------------------------------------
//! \brief                MSS Egress ECC Control Register: 1E.5070 
//                  MSS Egress ECC Control Register: 1E.5070 
//---------------------------------------------------------------------------------
struct mssEgressEccControlRegister_t
{
  union
  {
    struct
    {
                    /*! \brief 1E.5070.0 R/W MSS Egress ECC Enable
                        mssEgressEccControlRegister_t.bits_0.mssEgressEccEnable

                        Default = 0x0

                        1 = Enable ECC
                        
  */
      unsigned int   mssEgressEccEnable : 1;    // 1E.5070.0  R/W      Default = 0x0 
                     /* 1 = Enable ECC
                          */
                    /*! \brief 1E.5070.1 R/W MSS Egress Fast PN Number Enable
                        mssEgressEccControlRegister_t.bits_0.mssEgressFastPnNumberEnable

                        Default = 0x0

                        Reserved
                        

                 <B>Notes:</B>
                        Unused.  */
      unsigned int   mssEgressFastPnNumberEnable : 1;    // 1E.5070.1  R/W      Default = 0x0 
                     /* Reserved
                          */
      unsigned int   reserved0 : 14;
    } bits_0;
    unsigned short word_0;
  };
};


//---------------------------------------------------------------------------------
//! \brief                MSS Egress Debug Control Register: 1E.5072 
//                  MSS Egress Debug Control Register: 1E.5072 
//---------------------------------------------------------------------------------
struct mssEgressDebugControlRegister_t
{
  union
  {
    struct
    {
                    /*! \brief 1E.5072.1:0 R/W MSS Egress Debug Bus Select [1:0]
                        mssEgressDebugControlRegister_t.bits_0.mssEgressDebugBusSelect

                        Default = 0x0

                        1 = Enable ECC
                        

                 <B>Notes:</B>
                        Unused.  */
      unsigned int   mssEgressDebugBusSelect : 2;    // 1E.5072.1:0  R/W      Default = 0x0 
                     /* 1 = Enable ECC
                          */
      unsigned int   reserved0 : 14;
    } bits_0;
    unsigned short word_0;
  };
};


//---------------------------------------------------------------------------------
//! \brief                MSS Egress Debug Status Register: 1E.5074 
//                  MSS Egress Debug Status Register: 1E.5074 
//---------------------------------------------------------------------------------
struct mssEgressDebugStatusRegister_t
{
  union
  {
    struct
    {
                    /*! \brief 1E.5074.B:0 ROS MSS Egress Classification LUT Debug [B:0]
                        mssEgressDebugStatusRegister_t.bits_0.mssEgressClassificationLutDebug

                        Default = 0x000

                        Classification LUT debug
                        

                 <B>Notes:</B>
                        Debug only.  */
      unsigned int   mssEgressClassificationLutDebug : 12;    // 1E.5074.B:0  ROS      Default = 0x000 
                     /* Classification LUT debug
                          */
                    /*! \brief 1E.5074.F:C ROS MSS Egress MAC Control Filter Debug LSB [3:0]
                        mssEgressDebugStatusRegister_t.bits_0.mssEgressMacControlFilterDebugLsb

                        Default = 0x0

                        MAC control filter (CTLF) LUT debug bits 3:0
                        

                 <B>Notes:</B>
                        Debug only.  */
      unsigned int   mssEgressMacControlFilterDebugLsb : 4;    // 1E.5074.F:C  ROS      Default = 0x0 
                     /* MAC control filter (CTLF) LUT debug bits 3:0
                          */
    } bits_0;
    unsigned short word_0;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.5075.6:0 ROS MSS Egress MAC Control Filter Debug MSB [A:4]
                        mssEgressDebugStatusRegister_t.bits_1.mssEgressMacControlFilterDebugMsb

                        Default = 0x00

                        MAC control filter (CTLF) LUT debug bits 10:4
                        

                 <B>Notes:</B>
                        Debug only.  */
      unsigned int   mssEgressMacControlFilterDebugMsb : 7;    // 1E.5075.6:0  ROS      Default = 0x00 
                     /* MAC control filter (CTLF) LUT debug bits 10:4
                          */
                    /*! \brief 1E.5075.E:7 ROS MSS Egress SECTag Processing LUT Debug [7:0]
                        mssEgressDebugStatusRegister_t.bits_1.mssEgressSectagProcessingLutDebug

                        Default = 0x00

                        SECTag processing LUT debug
                        

                 <B>Notes:</B>
                        Debug only.  */
      unsigned int   mssEgressSectagProcessingLutDebug : 8;    // 1E.5075.E:7  ROS      Default = 0x00 
                     /* SECTag processing LUT debug
                          */
      unsigned int   reserved0 : 1;
    } bits_1;
    unsigned short word_1;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.5076.F:0 ROS MSS Egress Lookup Debug LSW [F:0]
                        mssEgressDebugStatusRegister_t.bits_2.mssEgressLookupDebugLSW

                        Default = 0x0000

                        Lookup debug bits 15:0
                        

                 <B>Notes:</B>
                        Debug only.  */
      unsigned int   mssEgressLookupDebugLSW : 16;    // 1E.5076.F:0  ROS      Default = 0x0000 
                     /* Lookup debug bits 15:0
                          */
    } bits_2;
    unsigned int word_2;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.5077.F:0 ROS MSS Egress Lookup Debug MSW [1F:10]
                        mssEgressDebugStatusRegister_t.bits_3.mssEgressLookupDebugMSW

                        Default = 0x0000

                        Lookup debug bits 31:16
                        

                 <B>Notes:</B>
                        Debug only.  */
      unsigned int   mssEgressLookupDebugMSW : 16;    // 1E.5077.F:0  ROS      Default = 0x0000 
                     /* Lookup debug bits 31:16
                          */
    } bits_3;
    unsigned int word_3;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.5078.5:0 ROS MSS Egress Datapath Debug [5:0]
                        mssEgressDebugStatusRegister_t.bits_4.mssEgressDatapathDebug

                        Default = 0x00

                        Datapath debug
                        

                 <B>Notes:</B>
                        Debug only.  */
      unsigned int   mssEgressDatapathDebug : 6;    // 1E.5078.5:0  ROS      Default = 0x00 
                     /* Datapath debug
                          */
      unsigned int   reserved0 : 10;
    } bits_4;
    unsigned int word_4;
  };
};


//---------------------------------------------------------------------------------
//! \brief                MSS Egress LUT Address Control Register: 1E.5080 
//                  MSS Egress LUT Address Control Register: 1E.5080 
//---------------------------------------------------------------------------------
struct mssEgressLutAddressControlRegister_t
{
  union
  {
    struct
    {
                    /*! \brief 1E.5080.8:0 R/W MSS Egress LUT Address [8:0]
                        mssEgressLutAddressControlRegister_t.bits_0.mssEgressLutAddress

                        Default = 0x000

                        LUT address
                        
  */
      unsigned int   mssEgressLutAddress : 9;    // 1E.5080.8:0  R/W      Default = 0x000 
                     /* LUT address
                          */
      unsigned int   reserved0 : 3;
                    /*! \brief 1E.5080.F:C R/W MSS Egress LUT Select [3:0]
                        mssEgressLutAddressControlRegister_t.bits_0.mssEgressLutSelect

                        Default = 0x0

                        LUT select
                        

                 <B>Notes:</B>
                        0x0 : Egress MAC Control FIlter (CTLF) LUT
                        0x1 : Egress Classification LUT
                        0x2 : Egress SC/SA LUT
                        0x3 : Egress SMIB  */
      unsigned int   mssEgressLutSelect : 4;    // 1E.5080.F:C  R/W      Default = 0x0 
                     /* LUT select
                          */
    } bits_0;
    unsigned short word_0;
  };
};

//---------------------------------------------------------------------------------
//                  MSS Egress LUT Control Register: 1E.5081 
//---------------------------------------------------------------------------------
struct mssEgressLutControlRegister_t
{
  union
  {
    struct
    {
      unsigned int   reserved0 : 14;
                    /*! \brief 1E.5081.E R/W MSS Egress LUT Read
                        mssEgressLutControlRegister_t.bits_0.mssEgressLutRead
                        Default = 0x0
                        1 = LUT read
                 <B>Notes:</B>
                        Setting this bit to 1, will read the LUT. This bit will automatically clear to 0.  */
      unsigned int   mssEgressLutRead : 1;    // 1E.5081.E  R/W      Default = 0x0 
                     /* 1 = LUT read */
                    /*! \brief 1E.5081.F R/W MSS Egress LUT Write
                        mssEgressLutControlRegister_t.bits_0.mssEgressLutWrite
                        Default = 0x0
                        1 = LUT write
                 <B>Notes:</B>
                        Setting this bit to 1, will write the LUT. This bit will automatically clear to 0.  */
      unsigned int   mssEgressLutWrite : 1;    // 1E.5081.F  R/W      Default = 0x0 
                     /* 1 = LUT write */
    } bits_0;
    unsigned short word_0;
  };
};

//---------------------------------------------------------------------------------
//                  MSS Egress LUT Data Control Register: 1E.50A0 
//---------------------------------------------------------------------------------
struct mssEgressLutDataControlRegister_t
{
  union
  {
    struct
    {
                    /*! \brief 1E.50A0.F:0 R/W MSS Egress LUT Data 0 [F:0]
                        mssEgressLutDataControlRegister_t.bits_0.mssEgressLutData_0
                        Default = 0x0000
                        LUT data bits 15:0 */
      unsigned int   mssEgressLutData_0 : 16;    // 1E.50A0.F:0  R/W      Default = 0x0000 
                     /* LUT data bits 15:0
                          */
    } bits_0;
    unsigned short word_0;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.50A1.F:0 R/W MSS Egress LUT Data 1 [1F:10]
                        mssEgressLutDataControlRegister_t.bits_1.mssEgressLutData_1
                        Default = 0x0000
                        LUT data bits 31:16 */
      unsigned int   mssEgressLutData_1 : 16;    // 1E.50A1.F:0  R/W      Default = 0x0000 
                     /* LUT data bits 31:16
                          */
    } bits_1;
    unsigned short word_1;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.50A2.F:0 R/W MSS Egress LUT Data 2 [2F:20]
                        mssEgressLutDataControlRegister_t.bits_2.mssEgressLutData_2
                        Default = 0x0000
                        LUT data bits 47:32 */
      unsigned int   mssEgressLutData_2 : 16;    // 1E.50A2.F:0  R/W      Default = 0x0000 
                     /* LUT data bits 47:32
                          */
    } bits_2;
    unsigned int word_2;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.50A3.F:0 R/W MSS Egress LUT Data 3 [3F:30]
                        mssEgressLutDataControlRegister_t.bits_3.mssEgressLutData_3
                        Default = 0x0000
                        LUT data bits 63:48 */
      unsigned int   mssEgressLutData_3 : 16;    // 1E.50A3.F:0  R/W      Default = 0x0000 
                     /* LUT data bits 63:48
                          */
    } bits_3;
    unsigned int word_3;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.50A4.F:0 R/W MSS Egress LUT Data 4 [4F:40]
                        mssEgressLutDataControlRegister_t.bits_4.mssEgressLutData_4
                        Default = 0x0000
                        LUT data bits 79:64 */
      unsigned int   mssEgressLutData_4 : 16;    // 1E.50A4.F:0  R/W      Default = 0x0000 
                     /* LUT data bits 79:64
                          */
    } bits_4;
    unsigned int word_4;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.50A5.F:0 R/W MSS Egress LUT Data 5 [5F:50]
                        mssEgressLutDataControlRegister_t.bits_5.mssEgressLutData_5

                        Default = 0x0000

                        LUT data bits 95:80
                        
  */
      unsigned int   mssEgressLutData_5 : 16;    // 1E.50A5.F:0  R/W      Default = 0x0000 
                     /* LUT data bits 95:80
                          */
    } bits_5;
    unsigned int word_5;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.50A6.F:0 R/W MSS Egress LUT Data 6 [6F:60]
                        mssEgressLutDataControlRegister_t.bits_6.mssEgressLutData_6

                        Default = 0x0000

                        LUT data bits 111:96
                        
  */
      unsigned int   mssEgressLutData_6 : 16;    // 1E.50A6.F:0  R/W      Default = 0x0000 
                     /* LUT data bits 111:96
                          */
    } bits_6;
    unsigned int word_6;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.50A7.F:0 R/W MSS Egress LUT Data 7 [7F:70]
                        mssEgressLutDataControlRegister_t.bits_7.mssEgressLutData_7

                        Default = 0x0000

                        LUT data bits 127:112
                        
  */
      unsigned int   mssEgressLutData_7 : 16;    // 1E.50A7.F:0  R/W      Default = 0x0000 
                     /* LUT data bits 127:112
                          */
    } bits_7;
    unsigned int word_7;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.50A8.F:0 R/W MSS Egress LUT Data 8 [8F:80]
                        mssEgressLutDataControlRegister_t.bits_8.mssEgressLutData_8

                        Default = 0x0000

                        LUT data bits 143:128
                        
  */
      unsigned int   mssEgressLutData_8 : 16;    // 1E.50A8.F:0  R/W      Default = 0x0000 
                     /* LUT data bits 143:128
                          */
    } bits_8;
    unsigned int word_8;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.50A9.F:0 R/W MSS Egress LUT Data 9 [9F:90]
                        mssEgressLutDataControlRegister_t.bits_9.mssEgressLutData_9

                        Default = 0x0000

                        LUT data bits 159:144
                        
  */
      unsigned int   mssEgressLutData_9 : 16;    // 1E.50A9.F:0  R/W      Default = 0x0000 
                     /* LUT data bits 159:144
                          */
    } bits_9;
    unsigned int word_9;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.50AA.F:0 R/W MSS Egress LUT Data 10 [AF:A0]
                        mssEgressLutDataControlRegister_t.bits_10.mssEgressLutData_10

                        Default = 0x0000

                        LUT data bits 175:160
                        
  */
      unsigned int   mssEgressLutData_10 : 16;    // 1E.50AA.F:0  R/W      Default = 0x0000 
                     /* LUT data bits 175:160
                          */
    } bits_10;
    unsigned short word_10;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.50AB.F:0 R/W MSS Egress LUT Data 11 [BF:B0]
                        mssEgressLutDataControlRegister_t.bits_11.mssEgressLutData_11

                        Default = 0x0000

                        LUT data bits 191:176
                        
  */
      unsigned int   mssEgressLutData_11 : 16;    // 1E.50AB.F:0  R/W      Default = 0x0000 
                     /* LUT data bits 191:176
                          */
    } bits_11;
    unsigned short word_11;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.50AC.F:0 R/W MSS Egress LUT Data 12 [CF:C0]
                        mssEgressLutDataControlRegister_t.bits_12.mssEgressLutData_12
                        Default = 0x0000
                        LUT data bits 207:192 */
      unsigned int   mssEgressLutData_12 : 16;    // 1E.50AC.F:0  R/W      Default = 0x0000 
                     /* LUT data bits 207:192 */
    } bits_12;
    unsigned short word_12;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.50AD.F:0 R/W MSS Egress LUT Data 13 [DF:D0]
                        mssEgressLutDataControlRegister_t.bits_13.mssEgressLutData_13
                        Default = 0x0000
                        LUT data bits 223:208 */
      unsigned int   mssEgressLutData_13 : 16;    // 1E.50AD.F:0  R/W      Default = 0x0000 
                     /* LUT data bits 223:208 */
    } bits_13;
    unsigned short word_13;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.50AE.F:0 R/W MSS Egress LUT Data 14 [EF:E0]
                        mssEgressLutDataControlRegister_t.bits_14.mssEgressLutData_14
                        Default = 0x0000
                        LUT data bits 239:224 */
      unsigned int   mssEgressLutData_14 : 16;    // 1E.50AE.F:0  R/W      Default = 0x0000 
                     /* LUT data bits 239:224 */
    } bits_14;
    unsigned short word_14;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.50AF.F:0 R/W MSS Egress LUT Data 15 [FF:F0]
                        mssEgressLutDataControlRegister_t.bits_15.mssEgressLutData_15
                        Default = 0x0000
                        LUT data bits 255:240 */
      unsigned int   mssEgressLutData_15 : 16;    // 1E.50AF.F:0  R/W      Default = 0x0000 
                     /* LUT data bits 255:240 */
    } bits_15;
    unsigned short word_15;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.50B0.F:0 R/W MSS Egress LUT Data 16 [10F:100]
                        mssEgressLutDataControlRegister_t.bits_16.mssEgressLutData_16
                        Default = 0x0000
                        LUT data bits 271:256 */
      unsigned int   mssEgressLutData_16 : 16;    // 1E.50B0.F:0  R/W      Default = 0x0000 
                     /* LUT data bits 271:256 */
    } bits_16;
    unsigned short word_16;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.50B1.F:0 R/W MSS Egress LUT Data 17 [11F:110]
                        mssEgressLutDataControlRegister_t.bits_17.mssEgressLutData_17
                        Default = 0x0000
                        LUT data bits 287:272 */
      unsigned int   mssEgressLutData_17 : 16;    // 1E.50B1.F:0  R/W      Default = 0x0000 
                     /* LUT data bits 287:272 */
    } bits_17;
    unsigned short word_17;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.50B2.F:0 R/W MSS Egress LUT Data 18 [12F:120]
                        mssEgressLutDataControlRegister_t.bits_18.mssEgressLutData_18
                        Default = 0x0000
                        LUT data bits 303:288 */
      unsigned int   mssEgressLutData_18 : 16;    // 1E.50B2.F:0  R/W      Default = 0x0000 
                     /* LUT data bits 303:288 */
    } bits_18;
    unsigned short word_18;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.50B3.F:0 R/W MSS Egress LUT Data 19 [13F:130]
                        mssEgressLutDataControlRegister_t.bits_19.mssEgressLutData_19
                        Default = 0x0000
                        LUT data bits 319:304 */
      unsigned int   mssEgressLutData_19 : 16;    // 1E.50B3.F:0  R/W      Default = 0x0000 
                     /* LUT data bits 319:304 */
    } bits_19;
    unsigned short word_19;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.50B4.F:0 R/W MSS Egress LUT Data 20 [14F:140]
                        mssEgressLutDataControlRegister_t.bits_20.mssEgressLutData_20
                        Default = 0x0000
                        LUT data bits 335:320 */
      unsigned int   mssEgressLutData_20 : 16;    // 1E.50B4.F:0  R/W      Default = 0x0000 
                     /* LUT data bits 335:320 */
    } bits_20;
    unsigned int word_20;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.50B5.F:0 R/W MSS Egress LUT Data 21 [15F:150]
                        mssEgressLutDataControlRegister_t.bits_21.mssEgressLutData_21
                        Default = 0x0000
                        LUT data bits 351:336 */
      unsigned int   mssEgressLutData_21 : 16;    // 1E.50B5.F:0  R/W      Default = 0x0000 
                     /* LUT data bits 351:336 */
    } bits_21;
    unsigned int word_21;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.50B6.F:0 R/W MSS Egress LUT Data 22 [16F:160]
                        mssEgressLutDataControlRegister_t.bits_22.mssEgressLutData_22
                        Default = 0x0000
                        LUT data bits 367:352 */
      unsigned int   mssEgressLutData_22 : 16;    // 1E.50B6.F:0  R/W      Default = 0x0000 
                     /* LUT data bits 367:352 */
    } bits_22;
    unsigned int word_22;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.50B7.F:0 R/W MSS Egress LUT Data 23 [17F:170]
                        mssEgressLutDataControlRegister_t.bits_23.mssEgressLutData_23
                        Default = 0x0000
                        LUT data bits 383:368 */
      unsigned int   mssEgressLutData_23 : 16;    // 1E.50B7.F:0  R/W      Default = 0x0000 
                     /* LUT data bits 383:368 */
    } bits_23;
    unsigned int word_23;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.50B8.F:0 R/W MSS Egress LUT Data 24 [18F:180]
                        mssEgressLutDataControlRegister_t.bits_24.mssEgressLutData_24
                        Default = 0x0000
                        LUT data bits 399:384 */
      unsigned int   mssEgressLutData_24 : 16;    // 1E.50B8.F:0  R/W      Default = 0x0000 
                     /* LUT data bits 399:384 */
    } bits_24;
    unsigned int word_24;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.50B9.F:0 R/W MSS Egress LUT Data 25 [19F:190]
                        mssEgressLutDataControlRegister_t.bits_25.mssEgressLutData_25
                        Default = 0x0000
                        LUT data bits 415:400 */
      unsigned int   mssEgressLutData_25 : 16;    // 1E.50B9.F:0  R/W      Default = 0x0000 
                     /* LUT data bits 415:400 */
    } bits_25;
    unsigned int word_25;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.50BA.F:0 R/W MSS Egress LUT Data 26 [1AF:1A0]
                        mssEgressLutDataControlRegister_t.bits_26.mssEgressLutData_26
                        Default = 0x0000
                        LUT data bits 431:416 */
      unsigned int   mssEgressLutData_26 : 16;    // 1E.50BA.F:0  R/W      Default = 0x0000 
                     /* LUT data bits 431:416 */
    } bits_26;
    unsigned int word_26;
  };
  union
  {
    struct
    {
                    /*! \brief 1E.50BB.F:0 R/W MSS Egress LUT Data 27 [1BF:1B0]
                        mssEgressLutDataControlRegister_t.bits_27.mssEgressLutData_27
                        Default = 0x0000
                        LUT data bits 447:432 */
      unsigned int   mssEgressLutData_27 : 16;    // 1E.50BB.F:0  R/W      Default = 0x0000 
                     /* LUT data bits 447:432 */
    } bits_27;
    unsigned int word_27;
  };
};

#endif /* MSS_EGRESS_REGS_HEADER */
