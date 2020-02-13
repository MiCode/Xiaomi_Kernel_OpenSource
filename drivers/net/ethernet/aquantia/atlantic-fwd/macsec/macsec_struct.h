#ifndef _MACSEC_STRUCT_H_
#define _MACSEC_STRUCT_H_


/*! Represents the bitfields of a single row in the Egress CTL Filter
    table.*/
typedef struct    
{    
   /*! Width: 48
       This is used to store the 48 bit value used to compare SA, DA
       or halfDA+half SA value.*/
 uint32_t  sa_da[2]; 
   /*! Width: 16
       This is used to store the 16 bit ethertype value used for
       comparison.*/
 uint32_t  eth_type; 
   /*! Width: 16
       The match mask is per-nibble. 0 means don't care, i.e. every
       value will match successfully. The total data is 64 bit, i.e.
       16 nibbles masks.*/
 uint32_t  match_mask; 
   /*! Width: 4
       0: No compare, i.e. This entry is not used
       1: compare DA only
       2: compare SA only
       3: compare half DA + half SA
       4: compare ether type only
       5: compare DA + ethertype
       6: compare SA + ethertype
       7: compare DA+ range.*/
 uint32_t  match_type; 
   /*! Width: 1
       0: Bypass the remaining modules if matched.
       1: Forward to next module for more classifications.*/
 uint32_t  action; 
} AQ_API_SEC_EgressCTLFRecord;    

/*! Represents the bitfields of a single row in the Egress Packet
    Classifier table.*/
typedef struct    
{    
   /*! Width: 12
       VLAN ID field.*/
 uint32_t  vlan_id; 
   /*! Width: 3
       VLAN UP field.*/
 uint32_t  vlan_up; 
   /*! Width: 1
       VLAN Present in the Packet.*/
 uint32_t  vlan_valid; 
   /*! Width: 8
       The 8 bit value used to compare with extracted value for byte
       3.*/
 uint32_t  byte3; 
   /*! Width: 8
       The 8 bit value used to compare with extracted value for byte
       2.*/
 uint32_t  byte2; 
   /*! Width: 8
       The 8 bit value used to compare with extracted value for byte
       1.*/
 uint32_t  byte1; 
   /*! Width: 8
       The 8 bit value used to compare with extracted value for byte
       0.*/
 uint32_t  byte0; 
   /*! Width: 8
       The 8 bit TCI field used to compare with extracted value.*/
 uint32_t  tci; 
   /*! Width: 64
       The 64 bit SCI field in the SecTAG.*/
 uint32_t  sci[2]; 
   /*! Width: 16
       The 16 bit Ethertype (in the clear) field used to compare with
       extracted value.*/
 uint32_t  eth_type; 
   /*! Width: 40
       This is to specify the 40bit SNAP header if the SNAP header's
       mask is enabled.*/
 uint32_t  snap[2]; 
   /*! Width: 24
       This is to specify the 24bit LLC header if the LLC header's
       mask is enabled.*/
 uint32_t  llc; 
   /*! Width: 48
       The 48 bit MAC_SA field used to compare with extracted value.*/
 uint32_t  mac_sa[2]; 
   /*! Width: 48
       The 48 bit MAC_DA field used to compare with extracted value.*/
 uint32_t  mac_da[2]; 
   /*! Width: 32
       The 32 bit Packet number used to compare with extracted
       value.*/
 uint32_t  pn; 
   /*! Width: 6
       0~63: byte location used extracted by packets comparator, which
       can be anything from the first 64 bytes of the MAC packets.
       This byte location counted from MAC' DA address. i.e. set to 0
       will point to byte 0 of DA address.*/
 uint32_t  byte3_location; 
   /*! Width: 1
       0: don't care
       1: enable comparison of extracted byte pointed by byte 3
       location.*/
 uint32_t  byte3_mask; 
   /*! Width: 6
       0~63: byte location used extracted by packets comparator, which
       can be anything from the first 64 bytes of the MAC packets.
       This byte location counted from MAC' DA address. i.e. set to 0
       will point to byte 0 of DA address.*/
 uint32_t  byte2_location; 
   /*! Width: 1
       0: don't care
       1: enable comparison of extracted byte pointed by byte 2
       location.*/
 uint32_t  byte2_mask; 
   /*! Width: 6
       0~63: byte location used extracted by packets comparator, which
       can be anything from the first 64 bytes of the MAC packets.
       This byte location counted from MAC' DA address. i.e. set to 0
       will point to byte 0 of DA address.*/
 uint32_t  byte1_location; 
   /*! Width: 1
       0: don't care
       1: enable comparison of extracted byte pointed by byte 1
       location.*/
 uint32_t  byte1_mask; 
   /*! Width: 6
       0~63: byte location used extracted by packets comparator, which
       can be anything from the first 64 bytes of the MAC packets.
       This byte location counted from MAC' DA address. i.e. set to 0
       will point to byte 0 of DA address.*/
 uint32_t  byte0_location; 
   /*! Width: 1
       0: don't care
       1: enable comparison of extracted byte pointed by byte 0
       location.*/
 uint32_t  byte0_mask; 
   /*! Width: 2
       Mask is per-byte.
       0: don't care
       1: enable comparison of extracted VLAN ID field.*/
 uint32_t  vlan_id_mask; 
   /*! Width: 1
       0: don't care
       1: enable comparison of extracted VLAN UP field.*/
 uint32_t  vlan_up_mask; 
   /*! Width: 1
       0: don't care
       1: enable comparison of extracted VLAN Valid field.*/
 uint32_t  vlan_valid_mask; 
   /*! Width: 8
       This is bit mask to enable comparison the 8 bit TCI field,
       including the AN field.
       For explicit SECTAG, AN is hardware controlled. For sending
       packet w/ explicit SECTAG, rest of the TCI fields are directly
       from the SECTAG.*/
 uint32_t  tci_mask; 
   /*! Width: 8
       Mask is per-byte.
       0: don't care
       1: enable comparison of SCI
       Note: If this field is not 0, this means the input packet's
       SECTAG is explicitly tagged and MACSEC module will only update
       the MSDU.
       PN number is hardware controlled.*/
 uint32_t  sci_mask; 
   /*! Width: 2
       Mask is per-byte.
       0: don't care
       1: enable comparison of Ethertype.*/
 uint32_t  eth_type_mask; 
   /*! Width: 5
       Mask is per-byte.
       0: don't care and no SNAP header exist.
       1: compare the SNAP header.
       If this bit is set to 1, the extracted filed will assume the
       SNAP header exist as encapsulated in 802.3 (RFC 1042). I.E. the
       next 5 bytes after the the LLC header is SNAP header.*/
 uint32_t  snap_mask; 
   /*! Width: 3
       0: don't care and no LLC header exist.
       1: compare the LLC header.
       If this bit is set to 1, the extracted filed will assume the
       LLC header exist as encapsulated in 802.3 (RFC 1042). I.E. the
       next three bytes after the 802.3MAC header is LLC header.*/
 uint32_t  llc_mask; 
   /*! Width: 6
       Mask is per-byte.
       0: don't care
       1: enable comparison of MAC_SA.*/
 uint32_t  sa_mask; 
   /*! Width: 6
       Mask is per-byte.
       0: don't care
       1: enable comparison of MAC_DA.*/
 uint32_t  da_mask; 
   /*! Width: 4
       Mask is per-byte.*/
 uint32_t  pn_mask; 
   /*! Width: 1
       Reserved. This bit should be always 0.*/
 uint32_t  eight02dot2; 
   /*! Width: 1
       1: For explicit sectag case use TCI_SC from table
       0: use TCI_SC from explicit sectag.*/
 uint32_t  tci_sc; 
   /*! Width: 1
       1: For explicit sectag case,use TCI_V,ES,SCB,E,C from table
       0: use TCI_V,ES,SCB,E,C from explicit sectag.*/
 uint32_t  tci_87543; 
   /*! Width: 1
       1: indicates that incoming packet has explicit sectag.*/
 uint32_t  exp_sectag_en; 
   /*! Width: 5
       If packet matches and tagged as controlled-packet, this SC/SA
       index is used for later SC and SA table lookup.*/
 uint32_t  sc_idx; 
   /*! Width: 2
       This field is used to specify how many SA entries are
       associated with 1 SC entry.
       2'b00: 1 SC has 4 SA.
       SC index is equivalent to {SC_Index[4:2], 1'b0}.
       SA index is equivalent to {SC_Index[4:2], SC entry's current
       AN[1:0]
       2'b10: 1 SC has 2 SA.
       SC index is equivalent to SC_Index[4:1]
       SA index is equivalent to {SC_Index[4:1], SC entry's current
       AN[0]}
       2'b11: 1 SC has 1 SA. No SC entry exists for the specific SA.
       SA index is equivalent to SC_Index[4:0]
       Note: if specified as 2'b11, hardware AN roll over is not
       supported.*/
 uint32_t  sc_sa; 
   /*! Width: 1
       0: the packets will be sent to MAC FIFO
       1: The packets will be sent to Debug/Loopback FIFO.
       If the above's action is drop, this bit has no meaning.*/
 uint32_t  debug; 
   /*! Width: 2
       0: forward to remaining modules
       1: bypass the next encryption modules. This packet is
       considered un-control packet.
       2: drop
       3: Reserved.*/
 uint32_t  action; 
   /*! Width: 1
       0: Not valid entry. This entry is not used
       1: valid entry.*/
 uint32_t  valid; 
} AQ_API_SEC_EgressClassRecord;    

/*! Represents the bitfields of a single row in the Egress SC Lookup
    table.*/
typedef struct    
{    
   /*! Width: 32
       This is to specify when the SC was first used. Set by HW.*/
 uint32_t  start_time; 
   /*! Width: 32
       This is to specify when the SC was last used. Set by HW.*/
 uint32_t  stop_time; 
   /*! Width: 2
       This is to specify which of the SA entries are used by current
       HW.
       Note: This value need to be set by SW after reset.  It will be
       automatically updated by HW, if AN roll over is enabled..*/
 uint32_t  curr_an; 
   /*! Width: 1
       0: Clear the SA Valid Bit after PN expiry.
       1: Do not Clear the SA Valid bit after PN expiry of the current
       SA.
       When the Enable AN roll over is set, S/W does not need to
       program the new SA's and the H/W will automatically roll over
       between the SA's without session expiry.
       For normal operation, Enable AN Roll over will be set to '0'
       and in which case, the SW needs to program the new SA values
       after the current PN expires.*/
 uint32_t  an_roll; 
   /*! Width: 6
       This is the TCI field used if packet is not explicitly
       tagged.*/
 uint32_t  tci; 
   /*! Width: 8
       This value indicates the offset where the decryption will
       start.[[Values of 0, 4, 8-50].*/
 uint32_t  enc_off; 
   /*! Width: 1
       0: Do not protect frames, all the packets will be forwarded
       unchanged. MIB counter (OutPktsUntagged) will be updated.
       1: Protect.*/
 uint32_t  protect; 
   /*! Width: 1
       0: when none of the SA related to SC has inUse set.
       1: when either of the SA related to the SC has inUse set.
       This bit is set by HW.*/
 uint32_t  recv; 
   /*! Width: 1
       0: H/W Clears this bit on the first use.
       1: SW updates this entry, when programming the SC Table..*/
 uint32_t  fresh; 
   /*! Width: 2
       AES Key size
       00 - 128bits
       01 - 192bits
       10 - 256bits
       11 - Reserved.*/
 uint32_t  sak_len; 
   /*! Width: 1
       0: Invalid SC
       1: Valid SC.*/
 uint32_t  valid; 
} AQ_API_SEC_EgressSCRecord;    

/*! Represents the bitfields of a single row in the Egress SA Lookup
    table.*/
typedef struct    
{    
   /*! Width: 32
       This is to specify when the SC was first used. Set by HW.*/
 uint32_t  start_time; 
   /*! Width: 32
       This is to specify when the SC was last used. Set by HW.*/
 uint32_t  stop_time; 
   /*! Width: 32
       This is set by SW and updated by HW to store the Next PN number
       used for encryption.*/
 uint32_t  next_pn; 
   /*! Width: 1
       The Next_PN number is going to wrapped around from 0xFFFF_FFFF
       to 0. set by HW.*/
 uint32_t  sat_pn; 
   /*! Width: 1
       0: This SA is in use.
       1: This SA is Fresh and set by SW.*/
 uint32_t  fresh; 
   /*! Width: 1
       0: Invalid SA
       1: Valid SA.*/
 uint32_t  valid; 
} AQ_API_SEC_EgressSARecord;    

/*! Represents the bitfields of a single row in the Egress SA Key
    Lookup table.*/
typedef struct    
{    
   /*! Width: 256
       Key for AES-GCM processing.*/
 uint32_t  key[8]; 
} AQ_API_SEC_EgressSAKeyRecord;    


/*! Represents the bitfields of a single row in the Ingress Pre-MACSec
    CTL Filter table.*/
typedef struct    
{    
   /*! Width: 48
       This is used to store the 48 bit value used to compare SA, DA
       or halfDA+half SA value.*/
 uint32_t  sa_da[2]; 
   /*! Width: 16
       This is used to store the 16 bit ethertype value used for
       comparison.*/
 uint32_t  eth_type; 
   /*! Width: 16
       The match mask is per-nibble. 0 means don't care, i.e. every
       value will match successfully. The total data is 64 bit, i.e.
       16 nibbles masks.*/
 uint32_t  match_mask; 
   /*! Width: 4
       0: No compare, i.e. This entry is not used
       1: compare DA only
       2: compare SA only
       3: compare half DA + half SA
       4: compare ether type only
       5: compare DA + ethertype
       6: compare SA + ethertype
       7: compare DA+ range.*/
 uint32_t  match_type; 
   /*! Width: 1
       0: Bypass the remaining modules if matched.
       1: Forward to next module for more classifications.*/
 uint32_t  action; 
} AQ_API_SEC_IngressPreCTLFRecord;    


/*! Represents the bitfields of a single row in the Ingress Pre-MACSec
    Packet Classifier table.*/
typedef struct    
{    
   /*! Width: 64
       The 64 bit SCI field used to compare with extracted value.
       Should have SCI value in case TCI[SCI_SEND] == 0. This will be
       used for ICV calculation..*/
 uint32_t  sci[2]; 
   /*! Width: 8
       The 8 bit TCI field used to compare with extracted value.*/
 uint32_t  tci; 
   /*! Width: 8
       8 bit encryption offset.*/
 uint32_t  encr_offset; 
   /*! Width: 16
       The 16 bit Ethertype (in the clear) field used to compare with
       extracted value.*/
 uint32_t  eth_type; 
   /*! Width: 40
       This is to specify the 40bit SNAP header if the SNAP header's
       mask is enabled.*/
 uint32_t  snap[2]; 
   /*! Width: 24
       This is to specify the 24bit LLC header if the LLC header's
       mask is enabled.*/
 uint32_t  llc; 
   /*! Width: 48
       The 48 bit MAC_SA field used to compare with extracted value.*/
 uint32_t  mac_sa[2]; 
   /*! Width: 48
       The 48 bit MAC_DA field used to compare with extracted value.*/
 uint32_t  mac_da[2]; 
   /*! Width: 1
       0: this is to compare with non-LPBK packet
       1: this is to compare with LPBK packet.
       This value is used to compare with a controlled-tag which goes
       with the packet when looped back from Egress port.*/
 uint32_t  lpbk_packet; 
   /*! Width: 2
       The value of this bit mask will affects how the SC index and SA
       index created.
       2'b00: 1 SC has 4 SA.
       SC index is equivalent to {SC_Index[4:2], 1'b0}.
       SA index is equivalent to {SC_Index[4:2], SECTAG's AN[1:0]}
       Here AN bits are not compared.
       2'b10: 1 SC has 2 SA.
       SC index is equivalent to SC_Index[4:1]
       SA index is equivalent to {SC_Index[4:1], SECTAG's AN[0]}
       Compare AN[1] field only
       2'b11: 1 SC has 1 SA. No SC entry exists for the specific SA.
       SA index is equivalent to SC_Index[4:0]
       AN[1:0] bits are compared.
       NOTE: This design is to supports different usage of AN. User
       can either ping-pong buffer 2 SA by using only the AN[0] bit.
       Or use 4 SA per SC by use AN[1:0] bits. Or even treat each SA
       as independent. i.e. AN[1:0] is just another matching pointer
       to select SA..*/
 uint32_t  an_mask; 
   /*! Width: 6
       This is bit mask to enable comparison the upper 6 bits TCI
       field, which does not include the AN field.
       0: don't compare
       1: enable comparison of the bits.*/
 uint32_t  tci_mask; 
   /*! Width: 8
       0: don't care
       1: enable comparison of SCI.*/
 uint32_t  sci_mask; 
   /*! Width: 2
       Mask is per-byte.
       0: don't care
       1: enable comparison of Ethertype.*/
 uint32_t  eth_type_mask; 
   /*! Width: 5
       Mask is per-byte.
       0: don't care and no SNAP header exist.
       1: compare the SNAP header.
       If this bit is set to 1, the extracted filed will assume the
       SNAP header exist as encapsulated in 802.3 (RFC 1042). I.E. the
       next 5 bytes after the the LLC header is SNAP header.*/
 uint32_t  snap_mask; 
   /*! Width: 3
       Mask is per-byte.
       0: don't care and no LLC header exist.
       1: compare the LLC header.
       If this bit is set to 1, the extracted filed will assume the
       LLC header exist as encapsulated in 802.3 (RFC 1042). I.E. the
       next three bytes after the 802.3MAC header is LLC header.*/
 uint32_t  llc_mask; 
   /*! Width: 1
       Reserved. This bit should be always 0.*/
 uint32_t  _802_2_encapsulate; 
   /*! Width: 6
       Mask is per-byte.
       0: don't care
       1: enable comparison of MAC_SA.*/
 uint32_t  sa_mask; 
   /*! Width: 6
       Mask is per-byte.
       0: don't care
       1: enable comparison of MAC_DA.*/
 uint32_t  da_mask; 
   /*! Width: 1
       0: don't care
       1: enable checking if this is loopback packet or not.*/
 uint32_t  lpbk_mask; 
   /*! Width: 5
       If packet matches and tagged as controlled-packet. This SC/SA
       index is used for later SC and SA table lookup.*/
 uint32_t  sc_idx; 
   /*! Width: 1
       0: the packets will be sent to MAC FIFO
       1: The packets will be sent to Debug/Loopback FIFO.
       If the above's action is drop. This bit has no meaning.*/
 uint32_t  proc_dest; 
   /*! Width: 2
       0: Process: Forward to next two modules for 802.1AE decryption.
       1: Process but keep SECTAG: Forward to next two modules for
       802.1AE decryption but keep the MACSEC header with added error
       code information. ICV will be stripped for all control packets
       2: Bypass: Bypass the next two decryption modules but processed
       by post-classification.
       3: Drop: drop this packet and update counts accordingly..*/
 uint32_t  action; 
   /*! Width: 1
       0: This is a controlled-port packet if matched.
       1: This is an uncontrolled-port packet if matched.*/
 uint32_t  ctrl_unctrl; 
   /*! Width: 1
       Use the SCI value from the Table if 'SC' bit of the input
       packet is not present.*/
 uint32_t  sci_from_table; 
   /*! Width: 4
       Reserved.*/
 uint32_t  reserved; 
   /*! Width: 1
       0: Not valid entry. This entry is not used
       1: valid entry.*/
 uint32_t  valid; 
} AQ_API_SEC_IngressPreClassRecord;    

/*! Represents the bitfields of a single row in the Ingress SC Lookup
    table.*/
typedef struct    
{    
   /*! Width: 32
       This is to specify when the SC was first used. Set by HW..*/
 uint32_t  stop_time; 
   /*! Width: 32
       This is to specify when the SC was first used. Set by HW.*/
 uint32_t  start_time; 
   /*! Width: 2
       0: Strict
       1: Check
       2: Disabled.*/
 uint32_t  validate_frames; 
   /*! Width: 1
       1: Replay control enabled.
       0: replay control disabled.*/
 uint32_t  replay_protect; 
   /*! Width: 32
       This is to specify the window range for anti-replay. Default is
       0.
       0: is strict order enforcement.*/
 uint32_t  anti_replay_window; 
   /*! Width: 1
       0: when none of the SA related to SC has inUse set.
       1: when either of the SA related to the SC has inUse set.
       This bit is set by HW.*/
 uint32_t  receiving; 
   /*! Width: 1
       0: when hardware processed the SC for the first time, it clears
       this bit
       1: This bit is set by SW, when it sets up the SC.*/
 uint32_t  fresh; 
   /*! Width: 1
       0: The AN number will not automatically roll over if Next_PN is
       saturated.
       1: The AN number will automatically roll over if Next_PN is
       saturated.
       Rollover is valid only after expiry. Normal roll over between
       SA's should be normal process.*/
 uint32_t  an_rol; 
   /*! Width: 25
       Reserved.*/
 uint32_t  reserved; 
   /*! Width: 1
       0: Invalid SC
       1: Valid SC.*/
 uint32_t  valid; 
} AQ_API_SEC_IngressSCRecord;    

/*! Represents the bitfields of a single row in the Ingress SA Lookup
    table.*/
typedef struct    
{    
   /*! Width: 32
       This is to specify when the SC was first used. Set by HW..*/
 uint32_t  stop_time; 
   /*! Width: 32
       This is to specify when the SC was first used. Set by HW.*/
 uint32_t  start_time; 
   /*! Width: 32
       This is updated by HW to store the expected NextPN number for
       anti-replay.*/
 uint32_t  next_pn; 
   /*! Width: 1
       The Next_PN number is going to wrapped around from 0XFFFF_FFFF
       to 0. set by HW.*/
 uint32_t  sat_nextpn; 
   /*! Width: 1
       0: This SA is not yet used.
       1: This SA is inUse.*/
 uint32_t  in_use; 
   /*! Width: 1
       0: when hardware processed the SC for the first time, it clears
       this timer
       1: This bit is set by SW, when it sets up the SC.*/
 uint32_t  fresh; 
   /*! Width: 28
       Reserved.*/
 uint32_t  reserved; 
   /*! Width: 1
       0: Invalid SA.
       1: Valid SA.*/
 uint32_t  valid; 
} AQ_API_SEC_IngressSARecord;    

/*! Represents the bitfields of a single row in the Ingress SA Key
    Lookup table.*/
typedef struct    
{    
   /*! Width: 256
       Key for AES-GCM processing.*/
 uint32_t  key[8]; 
   /*! Width: 2
       AES key size
       00 - 128bits
       01 - 192bits
       10 - 256bits
       11 - reserved.*/
 uint32_t  key_len; 
} AQ_API_SEC_IngressSAKeyRecord;    

/*! Represents the bitfields of a single row in the Ingress Post-
    MACSec Packet Classifier table.*/
typedef struct    
{    
   /*! Width: 8
       The 8 bit value used to compare with extracted value for byte
       0.*/
 uint32_t  byte0; 
   /*! Width: 8
       The 8 bit value used to compare with extracted value for byte
       1.*/
 uint32_t  byte1; 
   /*! Width: 8
       The 8 bit value used to compare with extracted value for byte
       2.*/
 uint32_t  byte2; 
   /*! Width: 8
       The 8 bit value used to compare with extracted value for byte
       3.*/
 uint32_t  byte3; 
   /*! Width: 16
       Ethertype in the packet.*/
 uint32_t  eth_type; 
   /*! Width: 1
       Ether Type value > 1500 (0x5dc).*/
 uint32_t  eth_type_valid; 
   /*! Width: 12
       VLAN ID after parsing.*/
 uint32_t  vlan_id; 
   /*! Width: 3
       VLAN priority after parsing.*/
 uint32_t  vlan_up; 
   /*! Width: 1
       Valid VLAN coding.*/
 uint32_t  vlan_valid; 
   /*! Width: 5
       SA index.*/
 uint32_t  sai; 
   /*! Width: 1
       SAI hit, i.e. controlled packet.*/
 uint32_t  sai_hit; 
   /*! Width: 4
       Mask for payload ethertype field.*/
 uint32_t  eth_type_mask; 
   /*! Width: 6
       0~63: byte location used extracted by packets comparator, which
       can be anything from the first 64 bytes of the MAC packets.
       This byte location counted from MAC' DA address. i.e. set to 0
       will point to byte 0 of DA address.*/
 uint32_t  byte3_location; 
   /*! Width: 2
       Mask for Byte Offset 3.*/
 uint32_t  byte3_mask; 
   /*! Width: 6
       0~63: byte location used extracted by packets comparator, which
       can be anything from the first 64 bytes of the MAC packets.
       This byte location counted from MAC' DA address. i.e. set to 0
       will point to byte 0 of DA address.*/
 uint32_t  byte2_location; 
   /*! Width: 2
       Mask for Byte Offset 2.*/
 uint32_t  byte2_mask; 
   /*! Width: 6
       0~63: byte location used extracted by packets comparator, which
       can be anything from the first 64 bytes of the MAC packets.
       This byte location counted from MAC' DA address. i.e. set to 0
       will point to byte 0 of DA address.*/
 uint32_t  byte1_location; 
   /*! Width: 2
       Mask for Byte Offset 1.*/
 uint32_t  byte1_mask; 
   /*! Width: 6
       0~63: byte location used extracted by packets comparator, which
       can be anything from the first 64 bytes of the MAC packets.
       This byte location counted from MAC' DA address. i.e. set to 0
       will point to byte 0 of DA address.*/
 uint32_t  byte0_location; 
   /*! Width: 2
       Mask for Byte Offset 0.*/
 uint32_t  byte0_mask; 
   /*! Width: 2
       Mask for Ethertype valid field. Indicates 802.3 vs. Other.*/
 uint32_t  eth_type_valid_mask; 
   /*! Width: 4
       Mask for VLAN ID field.*/
 uint32_t  vlan_id_mask; 
   /*! Width: 2
       Mask for VLAN UP field.*/
 uint32_t  vlan_up_mask; 
   /*! Width: 2
       Mask for VLAN valid field.*/
 uint32_t  vlan_valid_mask; 
   /*! Width: 2
       Mask for SAI.*/
 uint32_t  sai_mask; 
   /*! Width: 2
       Mask for SAI_HIT.*/
 uint32_t  sai_hit_mask; 
   /*! Width: 1
       Action if only first level matches and second level does not.
       0: pass
       1: drop (fail).*/
 uint32_t  firstlevel_actions; 
   /*! Width: 1
       Action if both first and second level matched.
       0: pass
       1: drop (fail).*/
 uint32_t  secondlevel_actions; 
   /*! Width: 4
       Reserved.*/
 uint32_t  reserved; 
   /*! Width: 1
       0: Not valid entry. This entry is not used
       1: valid entry.*/
 uint32_t  valid; 
} AQ_API_SEC_IngressPostClassRecord;    

/*! Represents the bitfields of a single row in the Ingress Post-
    MACSec CTL Filter table.*/
typedef struct    
{    
   /*! Width: 48
       This is used to store the 48 bit value used to compare SA, DA
       or halfDA+half SA value.*/
 uint32_t  sa_da[2]; 
   /*! Width: 16
       This is used to store the 16 bit ethertype value used for
       comparison.*/
 uint32_t  eth_type; 
   /*! Width: 16
       The match mask is per-nibble. 0 means don't care, i.e. every
       value will match successfully. The total data is 64 bit, i.e.
       16 nibbles masks.*/
 uint32_t  match_mask; 
   /*! Width: 4
       0: No compare, i.e. This entry is not used
       1: compare DA only
       2: compare SA only
       3: compare half DA + half SA
       4: compare ether type only
       5: compare DA + ethertype
       6: compare SA + ethertype
       7: compare DA+ range.*/
 uint32_t  match_type; 
   /*! Width: 1
       0: Bypass the remaining modules if matched.
       1: Forward to next module for more classifications.*/
 uint32_t  action; 
} AQ_API_SEC_IngressPostCTLFRecord;    

/*! Represents the Egress MIB counters for a single SC. Counters are
    64 bits, lower 32 bits in field[0].*/
typedef struct    
{    
   /*!  The number of integrity protected but not encrypted packets
        for this transmitting SC. */
 uint32_t  sc_protected_pkts[2]; 
   /*!  The number of integrity protected and encrypted packets for
        this transmitting SC. */
 uint32_t  sc_encrypted_pkts[2]; 
   /*!  The number of plain text octets that are integrity protected
        but not encrypted on the transmitting SC. */
 uint32_t  sc_protected_octets[2]; 
   /*!  The number of plain text octets that are integrity protected
        and encrypted on the transmitting SC. */
 uint32_t  sc_encrypted_octets[2]; 
} AQ_API_SEC_EgressSCCounters;    

/*! Represents the Egress MIB counters for a single SA. Counters are
    64 bits, lower 32 bits in field[0].*/
typedef struct    
{    
   /*!  The number of dropped packets for this transmitting SA. */
 uint32_t  sa_hit_drop_redirect[2]; 
   /*!  TODO */
 uint32_t  sa_protected2_pkts[2]; 
   /*!  The number of integrity protected but not encrypted packets
        for this transmitting SA. */
 uint32_t  sa_protected_pkts[2]; 
   /*!  The number of integrity protected and encrypted packets for
        this transmitting SA. */
 uint32_t  sa_encrypted_pkts[2]; 
} AQ_API_SEC_EgressSACounters;    


/*! Represents the common Egress MIB counters; the counter not
    associated with a particular SC/SA. Counters are 64 bits, lower 32
    bits in field[0].*/
typedef struct    
{    
   /*!  The number of transmitted packets classified as MAC_CTL
        packets.*/
 uint32_t  ctl_pkt[2]; 
   /*!  The number of transmitted packets that did not match any rows
        in the Egress Packet Classifier table. */
 uint32_t  unknown_sa_pkts[2]; 
   /*!  The number of transmitted packets where the SC table entry has
        protect=0 (so packets are forwarded unchanged).*/
 uint32_t  untagged_pkts[2]; 
   /*!  The number of transmitted packets discarded because the packet
        length is greater than the ifMtu of the Common Port interface.
        */
 uint32_t  too_long[2]; 
   /*!  The number of transmitted packets for which table memory was
        affected by an ECC error during processing. */
 uint32_t  ecc_error_pkts[2]; 
   /*!  The number of transmitted packets for where the matched row in
        the Egress Packet Classifier table has action=drop. */
 uint32_t  unctrl_hit_drop_redir[2]; 
} AQ_API_SEC_EgressCommonCounters;    

/*! Represents the Ingress MIB counters for a single SA. Counters are
    64 bits, lower 32 bits in field[0].*/
typedef struct    
{    
   /*!  For this SA, the number of received packets without a SecTAG.
        */
 uint32_t  untagged_hit_pkts[2]; 
   /*!  For this SA, the number of received packets that were dropped.
        */
 uint32_t  ctrl_hit_drop_redir_pkts[2]; 
   /*!  For this SA which is not currently in use, the number of
        received packets that have been discarded, and have either the
        packets encrypted or the matched row in the Ingress SC Lookup
        table has validate_frames=Strict.*/
 uint32_t  not_using_sa[2]; 
   /*!  For this SA which is not currently in use, the number of
        received, unencrypted, packets with the matched row in the
        Ingress SC Lookup table has validate_frames!=Strict. */
 uint32_t  unused_sa[2]; 
   /*!  For this SA, the number discarded packets with the condition
        that the packets are not valid and one of the following
        conditions are true: either the matched row in the Ingress SC
        Lookup table has validate_frames=Strict or the packets
        encrypted. */
 uint32_t  not_valid_pkts[2]; 
   /*!  For this SA, the number of packets with the condition that the
        packets are not valid and the matched row in the Ingress SC
        Lookup table has validate_frames=Check. */
 uint32_t  invalid_pkts[2]; 
   /*!  For this SA, the number of validated packets. */
 uint32_t  ok_pkts[2]; 
   /*!  For this SC, the number of received packets that have been
        discarded with the condition : the matched row in the Ingress
        SC Lookup table has replay_protect=1 and the PN of the packet
        is lower than the lower bound replay check PN. */
 uint32_t  late_pkts[2]; 
   /*!  For this SA, the number of packets with the condition that the
        PN of the packets is lower than the lower bound replay
        protection PN. */
 uint32_t  delayed_pkts[2]; 
   /*!  For this SC, the number of packets with the following
        condition:
        -the matched row in the Ingress SC Lookup table has
        replay_protect=0 or
        -the matched row in the Ingress SC Lookup table has
        replay_protect=1 and the packet is not encrypted and the
        integrity check has failed or
        -the matched row in the Ingress SC Lookup table has
        replay_protect=1 and the packet is encrypted and integrity
        check has failed. */
 uint32_t  unchecked_pkts[2]; 
   /*!  The number of octets of plaintext recovered from received
        packets that were integrity protected but not encrypted. */
 uint32_t  validated_octets[2]; 
   /*!  The number of octets of plaintext recovered from received
        packets that were integrity protected and encrypted. */
 uint32_t  decrypted_octets[2]; 
} AQ_API_SEC_IngressSACounters;    

/*! Represents the common Ingress MIB counters; the counter not
    associated with a particular SA. Counters are 64 bits, lower 32
    bits in field[0].*/
typedef struct    
{    
   /*!  The number of received packets classified as MAC_CTL packets.
        */
 uint32_t  ctl_pkts[2]; 
   /*!  The number of received packets with the MAC security tag
        (SecTAG), not matching any rows in the Ingress Pre-MACSec
        Packet Classifier table. */
 uint32_t  tagged_miss_pkts[2]; 
   /*!  The number of received packets without the MAC security tag
        (SecTAG), not matching any rows in the Ingress Pre-MACSec
        Packet Classifier table. */
 uint32_t  untagged_miss_pkts[2]; 
   /*!  The number of received packets discarded without the MAC
        security tag (SecTAG) and with the matched row in the Ingress
        SC Lookup table having validate_frames=Strict. */
 uint32_t  notag_pkts[2]; 
   /*!  The number of received packets without the MAC security tag
        (SecTAG) and with the matched row in the Ingress SC Lookup
        table having validate_frames!=Strict. */
 uint32_t  untagged_pkts[2]; 
   /*!  The number of received packets discarded with an invalid
        SecTAG or a zero value PN or an invalid ICV. */
 uint32_t  bad_tag_pkts[2]; 
   /*!  The number of received packets discarded with unknown SCI
        information with the condition :
        the matched row in the Ingress SC Lookup table has
        validate_frames=Strict or the C bit in the SecTAG is set.*/
 uint32_t  no_sci_pkts[2]; 
   /*!  The number of received packets with unknown SCI with the
        condition :
        The matched row in the Ingress SC Lookup table has
        validate_frames!=Strict and the C bit in the SecTAG is not
        set. */
 uint32_t  unknown_sci_pkts[2]; 
   /*!  The number of received packets by the controlled port service
        that passed the Ingress Post-MACSec Packet Classifier table
        check. */
 uint32_t  ctrl_prt_pass_pkts[2]; 
   /*!  The number of received packets by the uncontrolled port
        service that passed the Ingress Post-MACSec Packet Classifier
        table check. */
 uint32_t  unctrl_prt_pass_pkts[2]; 
   /*!  The number of received packets by the controlled port service
        that failed the Ingress Post-MACSec Packet Classifier table
        check. */
 uint32_t  ctrl_prt_fail_pkts[2]; 
   /*!  The number of received packets by the uncontrolled port
        service that failed the Ingress Post-MACSec Packet Classifier
        table check. */
 uint32_t  unctrl_prt_fail_pkts[2]; 
   /*!  The number of received packets discarded because the packet
        length is greater than the ifMtu of the Common Port interface.
        */
 uint32_t  too_long_pkts[2]; 
   /*!  The number of received packets classified as MAC_CTL by the
        Ingress Post-MACSec CTL Filter table. */
 uint32_t  igpoc_ctl_pkts[2]; 
   /*!  The number of received packets for which table memory was
        affected by an ECC error during processing. */
 uint32_t  ecc_error_pkts[2]; 
   /*!  The number of received packets by the uncontrolled port
        service that were dropped. */
 uint32_t  unctrl_hit_drop_redir[2]; 
} AQ_API_SEC_IngressCommonCounters;    

#endif
