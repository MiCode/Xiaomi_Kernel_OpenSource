#ifndef __MACSEC_API_H__
#define __MACSEC_API_H__

#include "atl_hw.h"
#include "macsec_struct.h"


#define NUMROWS_INGRESSPRECTLFRECORD 24
#define ROWOFFSET_INGRESSPRECTLFRECORD 0

#define NUMROWS_INGRESSPRECLASSRECORD 48
#define ROWOFFSET_INGRESSPRECLASSRECORD 0

#define NUMROWS_INGRESSPOSTCLASSRECORD 48
#define ROWOFFSET_INGRESSPOSTCLASSRECORD 0

#define NUMROWS_INGRESSSCRECORD 32
#define ROWOFFSET_INGRESSSCRECORD 0
 
#define NUMROWS_INGRESSSARECORD 32
#define ROWOFFSET_INGRESSSARECORD 32

#define NUMROWS_INGRESSSAKEYRECORD 32
#define ROWOFFSET_INGRESSSAKEYRECORD 0

#define NUMROWS_INGRESSPOSTCTLFRECORD 24
#define ROWOFFSET_INGRESSPOSTCTLFRECORD 0

#define NUMROWS_EGRESSCTLFRECORD 24
#define ROWOFFSET_EGRESSCTLFRECORD 0

#define NUMROWS_EGRESSCLASSRECORD 48
#define ROWOFFSET_EGRESSCLASSRECORD 0

#define NUMROWS_EGRESSSCRECORD 32
#define ROWOFFSET_EGRESSSCRECORD 0

#define NUMROWS_EGRESSSARECORD 32
#define ROWOFFSET_EGRESSSARECORD 32

#define NUMROWS_EGRESSSAKEYRECORD 32
#define ROWOFFSET_EGRESSSAKEYRECORD 96

/*!  Read the raw table data from the specified row of the Egress CTL
     Filter table, and unpack it into the fields of rec.
    [OUT] rec - The raw table row data will be unpacked into the fields of rec.
    tableIndex - The table row to read (struct atl_hw *hw, max 23). */
int AQ_API_GetEgressCTLFRecord(struct atl_hw *hw, AQ_API_SEC_EgressCTLFRecord* rec, uint16_t tableIndex);

/*!  Pack the fields of rec, and write the packed data into the
     specified row of the Egress CTL Filter table.
    rec - [IN] The bitfield values to write to the table row.
    tableIndex - The table row to write(struct atl_hw *hw, max 23).*/
int AQ_API_SetEgressCTLFRecord(struct atl_hw *hw, const AQ_API_SEC_EgressCTLFRecord* rec, uint16_t tableIndex);

/*!  Read the raw table data from the specified row of the Egress
     Packet Classifier table, and unpack it into the fields of rec.
     [OUT] The raw table row data will be unpacked into the fields of rec.
     The table row to read (struct atl_hw *hw, max 47).*/
int AQ_API_GetEgressClassRecord(struct atl_hw *hw, AQ_API_SEC_EgressClassRecord* rec, uint16_t tableIndex);

/*!  Pack the fields of rec, and write the packed data into the
     specified row of the Egress Packet Classifier table.
     rec - [IN] The bitfield values to write to the table row.
     tableIndex - The table row to write (struct atl_hw *hw, max 47).*/
int AQ_API_SetEgressClassRecord(struct atl_hw *hw, const AQ_API_SEC_EgressClassRecord* rec, uint16_t tableIndex);

/*!  Read the raw table data from the specified row of the Egress SC
     Lookup table, and unpack it into the fields of rec.
    rec - [OUT] The raw table row data will be unpacked into the fields of rec.
    tableIndex - The table row to read (struct atl_hw *hw, max 31).*/
int AQ_API_GetEgressSCRecord(struct atl_hw *hw, AQ_API_SEC_EgressSCRecord* rec, uint16_t tableIndex);

/*!  Pack the fields of rec, and write the packed data into the
     specified row of the Egress SC Lookup table.
    rec - [IN] The bitfield values to write to the table row.
    tableIndex - The table row to write (struct atl_hw *hw, max 31).*/
int AQ_API_SetEgressSCRecord(struct atl_hw *hw, const AQ_API_SEC_EgressSCRecord* rec, uint16_t tableIndex);

/*!  Read the raw table data from the specified row of the Egress SA
     Lookup table, and unpack it into the fields of rec.
    rec - [OUT] The raw table row data will be unpacked into the fields of rec.
    tableIndex - The table row to read (struct atl_hw *hw, max 31).*/
int AQ_API_GetEgressSARecord(struct atl_hw *hw, AQ_API_SEC_EgressSARecord* rec, uint16_t tableIndex);

/*!  Pack the fields of rec, and write the packed data into the
     specified row of the Egress SA Lookup table.
    rec  - [IN] The bitfield values to write to the table row.
    tableIndex - The table row to write (struct atl_hw *hw, max 31).*/
int AQ_API_SetEgressSARecord(struct atl_hw *hw, const AQ_API_SEC_EgressSARecord* rec, uint16_t tableIndex);

/*!  Read the raw table data from the specified row of the Egress SA
     Key Lookup table, and unpack it into the fields of rec.
    rec - [OUT] The raw table row data will be unpacked into the fields of rec.
    tableIndex - The table row to read (struct atl_hw *hw, max 31).*/
int AQ_API_GetEgressSAKeyRecord(struct atl_hw *hw, AQ_API_SEC_EgressSAKeyRecord* rec, uint16_t tableIndex); 

/*!  Pack the fields of rec, and write the packed data into the
     specified row of the Egress SA Key Lookup table.
    rec - [IN] The bitfield values to write to the table row.
    tableIndex - The table row to write (struct atl_hw *hw, max 31).*/
int AQ_API_SetEgressSAKeyRecord(struct atl_hw *hw, const AQ_API_SEC_EgressSAKeyRecord* rec, uint16_t tableIndex);

/*!  Read the raw table data from the specified row of the Ingress
     Pre-MACSec CTL Filter table, and unpack it into the fields of rec.
    rec - [OUT] The raw table row data will be unpacked into the fields of rec.
    tableIndex - The table row to read (struct atl_hw *hw, max 23).*/
int AQ_API_GetIngressPreCTLFRecord(struct atl_hw *hw, AQ_API_SEC_IngressPreCTLFRecord* rec, uint16_t);

/*!  Pack the fields of rec, and write the packed data into the
     specified row of the Ingress Pre-MACSec CTL Filter table.
    rec - [IN] The bitfield values to write to the table row.
    tableIndex - The table row to write(struct atl_hw *hw, max 23).*/
int AQ_API_SetIngressPreCTLFRecord(struct atl_hw *hw, const AQ_API_SEC_IngressPreCTLFRecord* rec, uint16_t tableIndex);

/*!  Read the raw table data from the specified row of the Ingress
     Pre-MACSec Packet Classifier table, and unpack it into the fields of rec.
    rec - [OUT] The raw table row data will be unpacked into the fields of rec.
    tableIndex - The table row to read (struct atl_hw *hw, max 47).*/
int AQ_API_GetIngressPreClassRecord(struct atl_hw *hw, AQ_API_SEC_IngressPreClassRecord* rec, uint16_t tableIndex);

/*!  Pack the fields of rec, and write the packed data into the
     specified row of the Ingress Pre-MACSec Packet Classifier table.
    rec - [IN] The bitfield values to write to the table row.
    tableIndex - The table row to write(struct atl_hw *hw, max 47).*/
int AQ_API_SetIngressPreClassRecord(struct atl_hw *hw, const AQ_API_SEC_IngressPreClassRecord* rec, uint16_t tableIndex);

/*!  Read the raw table data from the specified row of the Ingress SC
     Lookup table, and unpack it into the fields of rec.
    rec - [OUT] The raw table row data will be unpacked into the fields of rec.
    tableIndex - The table row to read (struct atl_hw *hw, max 31).*/
int AQ_API_GetIngressSCRecord(struct atl_hw *hw, AQ_API_SEC_IngressSCRecord* rec, uint16_t tableIndex);

/*!  Pack the fields of rec, and write the packed data into the
     specified row of the Ingress SC Lookup table.
    rec - [IN] The bitfield values to write to the table row.
    tableIndex - The table row to write(struct atl_hw *hw, max 31).*/
int AQ_API_SetIngressSCRecord(struct atl_hw *hw, const AQ_API_SEC_IngressSCRecord* rec, uint16_t tableIndex);

/*!  Read the raw table data from the specified row of the Ingress SA
     Lookup table, and unpack it into the fields of rec.
    rec - [OUT] The raw table row data will be unpacked into the fields of rec.
    tableIndex - The table row to read (struct atl_hw *hw, max 31).*/
int AQ_API_GetIngressSARecord(struct atl_hw *hw, AQ_API_SEC_IngressSARecord* rec, uint16_t);

/*!  Pack the fields of rec, and write the packed data into the
     specified row of the Ingress SA Lookup table.
    rec - [IN] The bitfield values to write to the table row.
    tableIndex - The table row to write(struct atl_hw *hw, max 31).*/
int AQ_API_SetIngressSARecord(struct atl_hw *hw, const AQ_API_SEC_IngressSARecord* rec, uint16_t tableIndex);

/*!  Read the raw table data from the specified row of the Ingress SA
     Key Lookup table, and unpack it into the fields of rec.
    rec - [OUT] The raw table row data will be unpacked into the fields of rec.
    tableIndex - The table row to read (struct atl_hw *hw, max 31).*/
int AQ_API_GetIngressSAKeyRecord(struct atl_hw *hw, AQ_API_SEC_IngressSAKeyRecord* rec, uint16_t tableIndex);

/*!  Pack the fields of rec, and write the packed data into the
     specified row of the Ingress SA Key Lookup table.
    rec - [IN] The bitfield values to write to the table row.
    tableIndex - The table row to write(struct atl_hw *hw, max 31).*/
int AQ_API_SetIngressSAKeyRecord(struct atl_hw *hw, const AQ_API_SEC_IngressSAKeyRecord* rec, uint16_t tableIndex);

/*!  Read the raw table data from the specified row of the Ingress
     Post-MACSec Packet Classifier table, and unpack it into the
     fields of rec.
    rec - [OUT] The raw table row data will be unpacked into the fields of rec.
    tableIndex - The table row to read (struct atl_hw *hw, max 48).*/
int AQ_API_GetIngressPostClassRecord(struct atl_hw *hw, AQ_API_SEC_IngressPostClassRecord* rec, uint16_t tableIndex);

/*!  Pack the fields of rec, and write the packed data into the
     specified row of the Ingress Post-MACSec Packet Classifier table.
    rec - [IN] The bitfield values to write to the table row.
    tableIndex - The table row to write(struct atl_hw *hw, max 48).*/
int AQ_API_SetIngressPostClassRecord(struct atl_hw *hw, const AQ_API_SEC_IngressPostClassRecord* rec, uint16_t tableIndex);

/*!  Read the raw table data from the specified row of the Ingress
     Post-MACSec CTL Filter table, and unpack it into the fields of rec.
    rec - [OUT] The raw table row data will be unpacked into the fields of rec.
    tableIndex - The table row to read (struct atl_hw *hw, max 23).*/
int AQ_API_GetIngressPostCTLFRecord(struct atl_hw *hw, AQ_API_SEC_IngressPostCTLFRecord* rec, uint16_t tableIndex);

/*!  Pack the fields of rec, and write the packed data into the
     specified row of the Ingress Post-MACSec CTL Filter table.
    rec - [IN] The bitfield values to write to the table row.
    tableIndex - The table row to write(struct atl_hw *hw, max 23).*/
int AQ_API_SetIngressPostCTLFRecord(struct atl_hw *hw, const AQ_API_SEC_IngressPostCTLFRecord* rec, uint16_t tableIndex);

/*!  Read the counters for the specified SC, and unpack them into the
     fields of counters.
    counters - [OUT] The raw table row data will be unpacked into the fields of counters.
    SCIndex - The table row to read (struct atl_hw *hw, max 31).*/
int AQ_API_GetEgressSCCounters(struct atl_hw *hw, AQ_API_SEC_EgressSCCounters* counters, uint16_t SCIndex);

/*!  Read the counters for the specified SA, and unpack them into the
     fields of counters.
    counters - [OUT] The raw table row data will be unpacked into the fields of counters.
    SAindex - The table row to read (struct atl_hw *hw, max 31). */
int AQ_API_GetEgressSACounters(struct atl_hw *hw, AQ_API_SEC_EgressSACounters* counters, uint16_t SAindex);

/*!  Read the counters for the common egress counters, and unpack them
     into the fields of counters.
     counters - [OUT] The raw table row data will be unpacked into the fields of counters.*/
int AQ_API_GetEgressCommonCounters(struct atl_hw *hw, AQ_API_SEC_EgressCommonCounters* counters);

/*!  Clear all Egress counters to 0.*/
int AQ_API_ClearEgressCounters(struct atl_hw *hw);

/*!  Read the counters for the specified SA, and unpack them into the
     fields of counters.
    counters - [OUT] The raw table row data will be unpacked into the fields of counters.
    SAindex - The table row to read (struct atl_hw *hw, max 31).*/
int AQ_API_GetIngressSACounters(struct atl_hw *hw, AQ_API_SEC_IngressSACounters* counters, uint16_t SAindex);

/*!  Read the counters for the common ingress counters, and unpack them into the fields of counters.
    counters - [OUT] The raw table row data will be unpacked into the fields of counters.*/
int AQ_API_GetIngressCommonCounters(struct atl_hw *hw, AQ_API_SEC_IngressCommonCounters* counters);
/* Clear all Ingress counters to 0. */
int AQ_API_ClearIngressCounters(struct atl_hw *hw);


/* Get Egress SA expired. */
int AQ_API_GetEgressSAExpired(struct atl_hw *hw, uint32_t *expired);
/* Get Egress SA threshold expired. */
int AQ_API_GetEgressSAThresholdExpired(struct atl_hw *hw, uint32_t *expired);
/* Set Egress SA expired. */
int AQ_API_SetEgressSAExpired(struct atl_hw *hw, uint32_t expired);
/* Set Egress SA threshold expired. */
int AQ_API_SetEgressSAThresholdExpired(struct atl_hw *hw, uint32_t expired);



#endif /* __MACSEC_API_H__ */
