
#include "macsec_api.h"
#include "MSS_Ingress_registers.h"
#include "MSS_Egress_registers.h"
#include "atl_mdio.h"

#define MMD_GLOBAL 0x1E

/**********************************************************************************************************************
*                                               MACSEC config and status
**********************************************************************************************************************/

static
int SetRawSECIngressRecordVal(struct atl_hw *hw, uint16_t* packedRecVal, uint8_t numWords, uint8_t tableID, uint16_t tableIndex)
{
    struct mssIngressLutAddressControlRegister_t tableSelReg;
    struct mssIngressLutControlRegister_t readWriteReg;

    unsigned int i;

  /* NOTE: MSS registers must always be read/written as adjacent pairs.
   * For instance, to write either or both 1E.80A0 and 1E.80A1, we have to:
   * 1. Write 1E.80A0 first
   * 2. Then write 1E.80A1 
   *
   * For HHD devices: These writes need to be performed consecutively, and 
   * to ensure this we use the PIF mailbox to delegate the reads/writes to 
   * the FW. 
   *
   * For EUR devices: Not need to use the PIF mailbox; it is safe to
   * write to the registers directly.
   * */

    /* Write the packed record words to the data buffer registers. */
    for (i = 0; i < numWords; i += 2)
    {
        atl_mdio_write(hw, 0, MMD_GLOBAL, mssIngressLutDataControlRegister_ADDR + i, packedRecVal[i]);
        atl_mdio_write(hw, 0, MMD_GLOBAL, mssIngressLutDataControlRegister_ADDR + i + 1, packedRecVal[i + 1]);
    }

    /* Clear out the unused data buffer registers. */
    for (i = numWords; i < 24; i += 2)
    {
        atl_mdio_write(hw, 0, MMD_GLOBAL, mssIngressLutDataControlRegister_ADDR + i, 0);
        atl_mdio_write(hw, 0, MMD_GLOBAL, mssIngressLutDataControlRegister_ADDR + i + 1, 0);
    }

    /* Select the table and row index to write to */
    tableSelReg.bits_0.mssIngressLutSelect = tableID;
    tableSelReg.bits_0.mssIngressLutAddress = tableIndex;

    /* Set the write bit to copy the raw record data buffer into the table */
    readWriteReg.bits_0.mssIngressLutRead = 0;
    readWriteReg.bits_0.mssIngressLutWrite = 1;

    /*Write register (EUR/CAL: 1E.8080) */
    atl_mdio_write(hw, 0, MMD_GLOBAL, mssIngressLutAddressControlRegister_ADDR, tableSelReg.word_0);
    /*Write register (EUR/CAL: 1E.8081) */
    atl_mdio_write(hw, 0, MMD_GLOBAL, mssIngressLutControlRegister_ADDR, readWriteReg.word_0);

    return 0;
}

/*! Read the specified Ingress LUT table row.  packedRecVal holds the row data. */
static
int GetRawSECIngressRecordVal(struct atl_hw *hw, uint16_t* packedRecVal, uint8_t numWords, uint8_t tableID, uint16_t tableIndex)
{
    struct mssIngressLutAddressControlRegister_t tableSelReg;
    struct mssIngressLutControlRegister_t readWriteReg;
    int ret;

    unsigned int i;

  /* NOTE: MSS registers must always be read/written as adjacent pairs.
   * For instance, to write either or both 1E.80A0 and 1E.80A1, we have to:
   * 1. Write 1E.80A0 first
   * 2. Then write 1E.80A1 
   *
   * For HHD devices: These writes need to be performed consecutively, and 
   * to ensure this we use the PIF mailbox to delegate the reads/writes to 
   * the FW. 
   *
   * For EUR devices: Not need to use the PIF mailbox; it is safe to
   * write to the registers directly.
   * */

    /* Select the table and row index to read */
    tableSelReg.bits_0.mssIngressLutSelect = tableID;
    tableSelReg.bits_0.mssIngressLutAddress = tableIndex;

    /* Set the read bit to copy the table record into the raw record data buffer
    * registers. */
    readWriteReg.bits_0.mssIngressLutRead = 1;
    readWriteReg.bits_0.mssIngressLutWrite = 0;

    /*Write register (EUR/CAL: 1E.8080) */
    ret = atl_mdio_write(hw, 0, MMD_GLOBAL, mssIngressLutAddressControlRegister_ADDR, tableSelReg.word_0);
    if (unlikely(ret))
        return ret;
    /*Write register (EUR/CAL: 1E.8081) */
    ret = atl_mdio_write(hw, 0, MMD_GLOBAL, mssIngressLutControlRegister_ADDR, readWriteReg.word_0);
    if (unlikely(ret))
        return ret;

    memset(packedRecVal, 0, sizeof(uint16_t) * numWords);

    /* Read the data buffer registers into the packed record words. */
    for (i = 0; i < numWords; i += 2)
    {
        ret = atl_mdio_read(hw, 0, MMD_GLOBAL, mssIngressLutDataControlRegister_ADDR + i, &packedRecVal[i]);
        if (unlikely(ret))
            return ret;
        ret = atl_mdio_read(hw, 0, MMD_GLOBAL, mssIngressLutDataControlRegister_ADDR + i + 1, &packedRecVal[i + 1]);
        if (unlikely(ret))
            return ret;
    }

    return 0;
}

/*! Write packedRecVal to the specified Egress LUT table row. */
static
int SetRawSECEgressRecordVal(struct atl_hw *hw, uint16_t* packedRecVal, uint8_t numWords, uint8_t tableID, uint16_t tableIndex)
{
    struct mssEgressLutAddressControlRegister_t tableSelReg;
    struct mssEgressLutControlRegister_t readWriteReg;

    unsigned int i;

  /* NOTE: MSS registers must always be read/written as adjacent pairs.
   * For instance, to write either or both 1E.80A0 and 1E.80A1, we have to:
   * 1. Write 1E.80A0 first
   * 2. Then write 1E.80A1 
   *
   * For HHD devices: These writes need to be performed consecutively, and 
   * to ensure this we use the PIF mailbox to delegate the reads/writes to 
   * the FW. 
   *
   * For EUR devices: Not need to use the PIF mailbox; it is safe to
   * write to the registers directly.
   * */

    /* Write the packed record words to the data buffer registers. */
    for (i = 0; i < numWords; i += 2)
    {
        atl_mdio_write(hw, 0, MMD_GLOBAL, mssEgressLutDataControlRegister_ADDR + i, packedRecVal[i]);
        atl_mdio_write(hw, 0, MMD_GLOBAL, mssEgressLutDataControlRegister_ADDR + i + 1, packedRecVal[i + 1]);
    }

    /* Clear out the unused data buffer registers. */
    for (i = numWords; i < 28; i += 2)
    { 
        atl_mdio_write(hw, 0, MMD_GLOBAL, mssEgressLutDataControlRegister_ADDR + i, 0);
        atl_mdio_write(hw, 0, MMD_GLOBAL, mssEgressLutDataControlRegister_ADDR + i + 1, 0);
    }

    /* Select the table and row index to write to */
    /*Assign to local representation of bitfield (HHD/EUR/CAL: 1E.5080.F:C) */
    tableSelReg.bits_0.mssEgressLutSelect = tableID;
    /*Assign to local representation of bitfield (HHD/EUR/CAL: 1E.5080.8:0) */
    tableSelReg.bits_0.mssEgressLutAddress = tableIndex;

    /* Set the write bit to copy the raw record data buffer into the table */
    /*Assign to local representation of bitfield (HHD/EUR/CAL: 1E.5081.E) */
    readWriteReg.bits_0.mssEgressLutRead = 0;
    /*Assign to local representation of bitfield (HHD/EUR/CAL: 1E.5081.F) */
    readWriteReg.bits_0.mssEgressLutWrite = 1;

    /*Write register (EUR/CAL: 1E.5080) */
    atl_mdio_write(hw, 0, MMD_GLOBAL, mssEgressLutAddressControlRegister_ADDR, tableSelReg.word_0);
    /*Write register (EUR/CAL: 1E.5081) */
    atl_mdio_write(hw, 0, MMD_GLOBAL, mssEgressLutControlRegister_ADDR, readWriteReg.word_0);

    return 0;
}

static
int GetRawSECEgressRecordVal(struct atl_hw *hw, uint16_t* packedRecVal, uint8_t numWords, uint8_t tableID, uint16_t tableIndex)
{
    struct mssEgressLutAddressControlRegister_t tableSelReg;
    struct mssEgressLutControlRegister_t readWriteReg;
    int ret;

    unsigned int i;

  /* NOTE: MSS registers must always be read/written as adjacent pairs.
   * For instance, to write either or both 1E.80A0 and 1E.80A1, we have to:
   * 1. Write 1E.80A0 first
   * 2. Then write 1E.80A1 
   *
   * For HHD devices: These writes need to be performed consecutively, and 
   * to ensure this we use the PIF mailbox to delegate the reads/writes to 
   * the FW. 
   *
   * For EUR devices: Not need to use the PIF mailbox; it is safe to
   * write to the registers directly.
   * */

    /* Select the table and row index to read */
    /*Assign to local representation of bitfield (HHD/EUR/CAL: 1E.5080.F:C) */
    tableSelReg.bits_0.mssEgressLutSelect = tableID;
    /*Assign to local representation of bitfield (HHD/EUR/CAL: 1E.5080.8:0) */
    tableSelReg.bits_0.mssEgressLutAddress = tableIndex;

    /* Set the read bit to copy the table record into the raw record data buffer
    * registers. */
    /*Assign to local representation of bitfield (HHD/EUR/CAL: 1E.5081.E) */
    readWriteReg.bits_0.mssEgressLutRead = 1;
    /*Assign to local representation of bitfield (HHD/EUR/CAL: 1E.5081.F) */
    readWriteReg.bits_0.mssEgressLutWrite = 0;

    /*Write register (EUR/CAL: 1E.5080) */
    ret = atl_mdio_write(hw, 0, MMD_GLOBAL, mssEgressLutAddressControlRegister_ADDR, tableSelReg.word_0);
    if (unlikely(ret))
        return ret;
    /*Write register (EUR/CAL: 1E.5081) */
    ret = atl_mdio_write(hw, 0, MMD_GLOBAL, mssEgressLutControlRegister_ADDR, readWriteReg.word_0);
    if (unlikely(ret))
        return ret;

    memset(packedRecVal, 0, sizeof(uint16_t) * numWords);

    /* Read the data buffer registers into the packed record words. */
    for (i = 0; i < numWords; i += 2)
    {
        ret = atl_mdio_read(hw, 0, MMD_GLOBAL, mssEgressLutDataControlRegister_ADDR + i, &packedRecVal[i]);
        if (unlikely(ret))
            return ret;
        ret = atl_mdio_read(hw, 0, MMD_GLOBAL, mssEgressLutDataControlRegister_ADDR + i + 1, &packedRecVal[i + 1]);
        if (unlikely(ret))
            return ret;
    }

    return 0;
}

int AQ_API_SetIngressPreCTLFRecord(struct atl_hw *hw, const AQ_API_SEC_IngressPreCTLFRecord* rec, uint16_t tableIndex)
{
    uint16_t packedRecVal[6];

    if (tableIndex >= NUMROWS_INGRESSPRECTLFRECORD)
        return -EINVAL;

    memset(packedRecVal, 0, sizeof(uint16_t) * 6);

    packedRecVal[0] = (packedRecVal[0] & 0x0000) | (((rec->sa_da[0] >> 0) & 0xFFFF) << 0);
    packedRecVal[1] = (packedRecVal[1] & 0x0000) | (((rec->sa_da[0] >> 16) & 0xFFFF) << 0);
    packedRecVal[2] = (packedRecVal[2] & 0x0000) | (((rec->sa_da[1] >> 0) & 0xFFFF) << 0);
    packedRecVal[3] = (packedRecVal[3] & 0x0000) | (((rec->eth_type >> 0) & 0xFFFF) << 0);
    packedRecVal[4] = (packedRecVal[4] & 0x0000) | (((rec->match_mask >> 0) & 0xFFFF) << 0);
    packedRecVal[5] = (packedRecVal[5] & 0xFFF0) | (((rec->match_type >> 0) & 0xF) << 0);
    packedRecVal[5] = (packedRecVal[5] & 0xFFEF) | (((rec->action >> 0) & 0x1) << 4);

    return SetRawSECIngressRecordVal(hw, packedRecVal, 6, 0, ROWOFFSET_INGRESSPRECTLFRECORD + tableIndex);
}

int AQ_API_GetIngressPreCTLFRecord(struct atl_hw *hw, AQ_API_SEC_IngressPreCTLFRecord* rec, uint16_t tableIndex)
{
    uint16_t packedRecVal[6];
    int ret;

    if (tableIndex >= NUMROWS_INGRESSPRECTLFRECORD)
        return -EINVAL;

      /* If the row that we want to read is odd, first read the previous even 
       * row, throw that value away, and finally read the desired row.
       * This is a workaround for EUR devices that allows us to read 
       * odd-numbered rows.  For HHD devices: this workaround will not work, 
       * so don't bother; odd-numbered rows are not readable. */
    if ((tableIndex % 2) > 0)
    {
        ret = GetRawSECIngressRecordVal(hw, packedRecVal, 6, 0, ROWOFFSET_INGRESSPRECTLFRECORD + tableIndex - 1);
        if (unlikely(ret))
            return ret;
    }

    memset(rec, 0, sizeof(AQ_API_SEC_IngressPreCTLFRecord));

    ret = GetRawSECIngressRecordVal(hw, packedRecVal, 6, 0, ROWOFFSET_INGRESSPRECTLFRECORD + tableIndex);
    if (unlikely(ret))
        return ret;

    rec->sa_da[0] = (rec->sa_da[0] & 0xFFFF0000) | (((packedRecVal[0] >> 0) & 0xFFFF) << 0);
    rec->sa_da[0] = (rec->sa_da[0] & 0x0000FFFF) | (((packedRecVal[1] >> 0) & 0xFFFF) << 16);

    rec->sa_da[1] = (rec->sa_da[1] & 0xFFFF0000) | (((packedRecVal[2] >> 0) & 0xFFFF) << 0);

    rec->eth_type = (rec->eth_type & 0xFFFF0000) | (((packedRecVal[3] >> 0) & 0xFFFF) << 0);

    rec->match_mask = (rec->match_mask & 0xFFFF0000) | (((packedRecVal[4] >> 0) & 0xFFFF) << 0);

    rec->match_type = (rec->match_type & 0xFFFFFFF0) | (((packedRecVal[5] >> 0) & 0xF) << 0);

    rec->action = (rec->action & 0xFFFFFFFE) | (((packedRecVal[5] >> 4) & 0x1) << 0);

    return 0;
}

int AQ_API_SetIngressPreClassRecord(struct atl_hw *hw, const AQ_API_SEC_IngressPreClassRecord* rec, uint16_t tableIndex)
{
    uint16_t packedRecVal[20];

    if (tableIndex >= NUMROWS_INGRESSPRECLASSRECORD)
        return -EINVAL;

    memset(packedRecVal, 0, sizeof(uint16_t) * 20);

    packedRecVal[0] = (packedRecVal[0] & 0x0000) | (((rec->sci[0] >> 0) & 0xFFFF) << 0);
    packedRecVal[1] = (packedRecVal[1] & 0x0000) | (((rec->sci[0] >> 16) & 0xFFFF) << 0);

    packedRecVal[2] = (packedRecVal[2] & 0x0000) | (((rec->sci[1] >> 0) & 0xFFFF) << 0);
    packedRecVal[3] = (packedRecVal[3] & 0x0000) | (((rec->sci[1] >> 16) & 0xFFFF) << 0);

    packedRecVal[4] = (packedRecVal[4] & 0xFF00) | (((rec->tci >> 0) & 0xFF) << 0);

    packedRecVal[4] = (packedRecVal[4] & 0x00FF) | (((rec->encr_offset >> 0) & 0xFF) << 8);

    packedRecVal[5] = (packedRecVal[5] & 0x0000) | (((rec->eth_type >> 0) & 0xFFFF) << 0);

    packedRecVal[6] = (packedRecVal[6] & 0x0000) | (((rec->snap[0] >> 0) & 0xFFFF) << 0);
    packedRecVal[7] = (packedRecVal[7] & 0x0000) | (((rec->snap[0] >> 16) & 0xFFFF) << 0);

    packedRecVal[8] = (packedRecVal[8] & 0xFF00) | (((rec->snap[1] >> 0) & 0xFF) << 0);

    packedRecVal[8] = (packedRecVal[8] & 0x00FF) | (((rec->llc >> 0) & 0xFF) << 8);
    packedRecVal[9] = (packedRecVal[9] & 0x0000) | (((rec->llc >> 8) & 0xFFFF) << 0);

    packedRecVal[10] = (packedRecVal[10] & 0x0000) | (((rec->mac_sa[0] >> 0) & 0xFFFF) << 0);
    packedRecVal[11] = (packedRecVal[11] & 0x0000) | (((rec->mac_sa[0] >> 16) & 0xFFFF) << 0);

    packedRecVal[12] = (packedRecVal[12] & 0x0000) | (((rec->mac_sa[1] >> 0) & 0xFFFF) << 0);

    packedRecVal[13] = (packedRecVal[13] & 0x0000) | (((rec->mac_da[0] >> 0) & 0xFFFF) << 0);
    packedRecVal[14] = (packedRecVal[14] & 0x0000) | (((rec->mac_da[0] >> 16) & 0xFFFF) << 0);

    packedRecVal[15] = (packedRecVal[15] & 0x0000) | (((rec->mac_da[1] >> 0) & 0xFFFF) << 0);

    packedRecVal[16] = (packedRecVal[16] & 0xFFFE) | (((rec->lpbk_packet >> 0) & 0x1) << 0);

    packedRecVal[16] = (packedRecVal[16] & 0xFFF9) | (((rec->an_mask >> 0) & 0x3) << 1);

    packedRecVal[16] = (packedRecVal[16] & 0xFE07) | (((rec->tci_mask >> 0) & 0x3F) << 3);

    packedRecVal[16] = (packedRecVal[16] & 0x01FF) | (((rec->sci_mask >> 0) & 0x7F) << 9);
    packedRecVal[17] = (packedRecVal[17] & 0xFFFE) | (((rec->sci_mask >> 7) & 0x1) << 0);

    packedRecVal[17] = (packedRecVal[17] & 0xFFF9) | (((rec->eth_type_mask >> 0) & 0x3) << 1);

    packedRecVal[17] = (packedRecVal[17] & 0xFF07) | (((rec->snap_mask >> 0) & 0x1F) << 3);

    packedRecVal[17] = (packedRecVal[17] & 0xF8FF) | (((rec->llc_mask >> 0) & 0x7) << 8);

    packedRecVal[17] = (packedRecVal[17] & 0xF7FF) | (((rec->_802_2_encapsulate >> 0) & 0x1) << 11);

    packedRecVal[17] = (packedRecVal[17] & 0x0FFF) | (((rec->sa_mask >> 0) & 0xF) << 12);
    packedRecVal[18] = (packedRecVal[18] & 0xFFFC) | (((rec->sa_mask >> 4) & 0x3) << 0);

    packedRecVal[18] = (packedRecVal[18] & 0xFF03) | (((rec->da_mask >> 0) & 0x3F) << 2);

    packedRecVal[18] = (packedRecVal[18] & 0xFEFF) | (((rec->lpbk_mask >> 0) & 0x1) << 8);

    packedRecVal[18] = (packedRecVal[18] & 0xC1FF) | (((rec->sc_idx >> 0) & 0x1F) << 9);

    packedRecVal[18] = (packedRecVal[18] & 0xBFFF) | (((rec->proc_dest >> 0) & 0x1) << 14);

    packedRecVal[18] = (packedRecVal[18] & 0x7FFF) | (((rec->action >> 0) & 0x1) << 15);
    packedRecVal[19] = (packedRecVal[19] & 0xFFFE) | (((rec->action >> 1) & 0x1) << 0);

    packedRecVal[19] = (packedRecVal[19] & 0xFFFD) | (((rec->ctrl_unctrl >> 0) & 0x1) << 1);

    packedRecVal[19] = (packedRecVal[19] & 0xFFFB) | (((rec->sci_from_table >> 0) & 0x1) << 2);

    packedRecVal[19] = (packedRecVal[19] & 0xFF87) | (((rec->reserved >> 0) & 0xF) << 3);

    packedRecVal[19] = (packedRecVal[19] & 0xFF7F) | (((rec->valid >> 0) & 0x1) << 7);

    return SetRawSECIngressRecordVal(hw, packedRecVal, 20, 1, ROWOFFSET_INGRESSPRECLASSRECORD + tableIndex);
}

int AQ_API_GetIngressPreClassRecord(struct atl_hw *hw, AQ_API_SEC_IngressPreClassRecord* rec, uint16_t tableIndex)
{
    uint16_t packedRecVal[20];
    int ret;

    if (tableIndex >= NUMROWS_INGRESSPRECLASSRECORD)
        return -EINVAL;

      /* If the row that we want to read is odd, first read the previous even 
       * row, throw that value away, and finally read the desired row.
       * This is a workaround for EUR devices that allows us to read 
       * odd-numbered rows.  For HHD devices: this workaround will not work, 
       * so don't bother; odd-numbered rows are not readable. */
    if ((tableIndex % 2) > 0)
    {
        ret = GetRawSECIngressRecordVal(hw, packedRecVal, 20, 1, 
                  ROWOFFSET_INGRESSPRECLASSRECORD + tableIndex - 1);
        if (unlikely(ret))
            return ret;
    }

    memset(rec, 0, sizeof(AQ_API_SEC_IngressPreClassRecord));

    ret = GetRawSECIngressRecordVal(hw, packedRecVal, 20, 1, ROWOFFSET_INGRESSPRECLASSRECORD + tableIndex);
    if (unlikely(ret))
        return ret;

    rec->sci[0] = (rec->sci[0] & 0xFFFF0000) | (((packedRecVal[0] >> 0) & 0xFFFF) << 0);
    rec->sci[0] = (rec->sci[0] & 0x0000FFFF) | (((packedRecVal[1] >> 0) & 0xFFFF) << 16);

    rec->sci[1] = (rec->sci[1] & 0xFFFF0000) | (((packedRecVal[2] >> 0) & 0xFFFF) << 0);
    rec->sci[1] = (rec->sci[1] & 0x0000FFFF) | (((packedRecVal[3] >> 0) & 0xFFFF) << 16);

    rec->tci = (rec->tci & 0xFFFFFF00) | (((packedRecVal[4] >> 0) & 0xFF) << 0);

    rec->encr_offset = (rec->encr_offset & 0xFFFFFF00) | (((packedRecVal[4] >> 8) & 0xFF) << 0);

    rec->eth_type = (rec->eth_type & 0xFFFF0000) | (((packedRecVal[5] >> 0) & 0xFFFF) << 0);

    rec->snap[0] = (rec->snap[0] & 0xFFFF0000) | (((packedRecVal[6] >> 0) & 0xFFFF) << 0);
    rec->snap[0] = (rec->snap[0] & 0x0000FFFF) | (((packedRecVal[7] >> 0) & 0xFFFF) << 16);

    rec->snap[1] = (rec->snap[1] & 0xFFFFFF00) | (((packedRecVal[8] >> 0) & 0xFF) << 0);

    rec->llc = (rec->llc & 0xFFFFFF00) | (((packedRecVal[8] >> 8) & 0xFF) << 0);
    rec->llc = (rec->llc & 0xFF0000FF) | (((packedRecVal[9] >> 0) & 0xFFFF) << 8);

    rec->mac_sa[0] = (rec->mac_sa[0] & 0xFFFF0000) | (((packedRecVal[10] >> 0) & 0xFFFF) << 0);
    rec->mac_sa[0] = (rec->mac_sa[0] & 0x0000FFFF) | (((packedRecVal[11] >> 0) & 0xFFFF) << 16);

    rec->mac_sa[1] = (rec->mac_sa[1] & 0xFFFF0000) | (((packedRecVal[12] >> 0) & 0xFFFF) << 0);

    rec->mac_da[0] = (rec->mac_da[0] & 0xFFFF0000) | (((packedRecVal[13] >> 0) & 0xFFFF) << 0);
    rec->mac_da[0] = (rec->mac_da[0] & 0x0000FFFF) | (((packedRecVal[14] >> 0) & 0xFFFF) << 16);

    rec->mac_da[1] = (rec->mac_da[1] & 0xFFFF0000) | (((packedRecVal[15] >> 0) & 0xFFFF) << 0);

    rec->lpbk_packet = (rec->lpbk_packet & 0xFFFFFFFE) | (((packedRecVal[16] >> 0) & 0x1) << 0);

    rec->an_mask = (rec->an_mask & 0xFFFFFFFC) | (((packedRecVal[16] >> 1) & 0x3) << 0);

    rec->tci_mask = (rec->tci_mask & 0xFFFFFFC0) | (((packedRecVal[16] >> 3) & 0x3F) << 0);

    rec->sci_mask = (rec->sci_mask & 0xFFFFFF80) | (((packedRecVal[16] >> 9) & 0x7F) << 0);
    rec->sci_mask = (rec->sci_mask & 0xFFFFFF7F) | (((packedRecVal[17] >> 0) & 0x1) << 7);

    rec->eth_type_mask = (rec->eth_type_mask & 0xFFFFFFFC) | (((packedRecVal[17] >> 1) & 0x3) << 0);

    rec->snap_mask = (rec->snap_mask & 0xFFFFFFE0) | (((packedRecVal[17] >> 3) & 0x1F) << 0);

    rec->llc_mask = (rec->llc_mask & 0xFFFFFFF8) | (((packedRecVal[17] >> 8) & 0x7) << 0);

    rec->_802_2_encapsulate = (rec->_802_2_encapsulate & 0xFFFFFFFE) | (((packedRecVal[17] >> 11) & 0x1) << 0);

    rec->sa_mask = (rec->sa_mask & 0xFFFFFFF0) | (((packedRecVal[17] >> 12) & 0xF) << 0);
    rec->sa_mask = (rec->sa_mask & 0xFFFFFFCF) | (((packedRecVal[18] >> 0) & 0x3) << 4);

    rec->da_mask = (rec->da_mask & 0xFFFFFFC0) | (((packedRecVal[18] >> 2) & 0x3F) << 0);

    rec->lpbk_mask = (rec->lpbk_mask & 0xFFFFFFFE) | (((packedRecVal[18] >> 8) & 0x1) << 0);

    rec->sc_idx = (rec->sc_idx & 0xFFFFFFE0) | (((packedRecVal[18] >> 9) & 0x1F) << 0);

    rec->proc_dest = (rec->proc_dest & 0xFFFFFFFE) | (((packedRecVal[18] >> 14) & 0x1) << 0);

    rec->action = (rec->action & 0xFFFFFFFE) | (((packedRecVal[18] >> 15) & 0x1) << 0);
    rec->action = (rec->action & 0xFFFFFFFD) | (((packedRecVal[19] >> 0) & 0x1) << 1);

    rec->ctrl_unctrl = (rec->ctrl_unctrl & 0xFFFFFFFE) | (((packedRecVal[19] >> 1) & 0x1) << 0);

    rec->sci_from_table = (rec->sci_from_table & 0xFFFFFFFE) | (((packedRecVal[19] >> 2) & 0x1) << 0);

    rec->reserved = (rec->reserved & 0xFFFFFFF0) | (((packedRecVal[19] >> 3) & 0xF) << 0);

    rec->valid = (rec->valid & 0xFFFFFFFE) | (((packedRecVal[19] >> 7) & 0x1) << 0);

    return 0;
}


int AQ_API_SetIngressSCRecord(struct atl_hw *hw, const AQ_API_SEC_IngressSCRecord* rec, uint16_t tableIndex)
{
    uint16_t packedRecVal[8];

    if (tableIndex >= NUMROWS_INGRESSSCRECORD)
        return -EINVAL;

    memset(packedRecVal, 0, sizeof(uint16_t) * 8);

    packedRecVal[0] = (packedRecVal[0] & 0x0000) | (((rec->stop_time >> 0) & 0xFFFF) << 0);
    packedRecVal[1] = (packedRecVal[1] & 0x0000) | (((rec->stop_time >> 16) & 0xFFFF) << 0);

    packedRecVal[2] = (packedRecVal[2] & 0x0000) | (((rec->start_time >> 0) & 0xFFFF) << 0);
    packedRecVal[3] = (packedRecVal[3] & 0x0000) | (((rec->start_time >> 16) & 0xFFFF) << 0);

    packedRecVal[4] = (packedRecVal[4] & 0xFFFC) | (((rec->validate_frames >> 0) & 0x3) << 0);

    packedRecVal[4] = (packedRecVal[4] & 0xFFFB) | (((rec->replay_protect >> 0) & 0x1) << 2);

    packedRecVal[4] = (packedRecVal[4] & 0x0007) | (((rec->anti_replay_window >> 0) & 0x1FFF) << 3);
    packedRecVal[5] = (packedRecVal[5] & 0x0000) | (((rec->anti_replay_window >> 13) & 0xFFFF) << 0);
    packedRecVal[6] = (packedRecVal[6] & 0xFFF8) | (((rec->anti_replay_window >> 29) & 0x7) << 0);

    packedRecVal[6] = (packedRecVal[6] & 0xFFF7) | (((rec->receiving >> 0) & 0x1) << 3);

    packedRecVal[6] = (packedRecVal[6] & 0xFFEF) | (((rec->fresh >> 0) & 0x1) << 4);

    packedRecVal[6] = (packedRecVal[6] & 0xFFDF) | (((rec->an_rol >> 0) & 0x1) << 5);

    packedRecVal[6] = (packedRecVal[6] & 0x003F) | (((rec->reserved >> 0) & 0x3FF) << 6);
    packedRecVal[7] = (packedRecVal[7] & 0x8000) | (((rec->reserved >> 10) & 0x7FFF) << 0);

    packedRecVal[7] = (packedRecVal[7] & 0x7FFF) | (((rec->valid >> 0) & 0x1) << 15);

    return SetRawSECIngressRecordVal(hw, packedRecVal, 8, 3, ROWOFFSET_INGRESSSCRECORD + tableIndex);
}

int AQ_API_GetIngressSCRecord(struct atl_hw *hw, AQ_API_SEC_IngressSCRecord* rec, uint16_t tableIndex)
{
    uint16_t packedRecVal[8];
    int ret;

    if (tableIndex >= NUMROWS_INGRESSSCRECORD)
        return -EINVAL;

    memset(rec, 0, sizeof(AQ_API_SEC_IngressSCRecord));

    ret = GetRawSECIngressRecordVal(hw, packedRecVal, 8, 3, ROWOFFSET_INGRESSSCRECORD + tableIndex);
    if (unlikely(ret))
        return ret;

    rec->stop_time = (rec->stop_time & 0xFFFF0000) | (((packedRecVal[0] >> 0) & 0xFFFF) << 0);
    rec->stop_time = (rec->stop_time & 0x0000FFFF) | (((packedRecVal[1] >> 0) & 0xFFFF) << 16);

    rec->start_time = (rec->start_time & 0xFFFF0000) | (((packedRecVal[2] >> 0) & 0xFFFF) << 0);
    rec->start_time = (rec->start_time & 0x0000FFFF) | (((packedRecVal[3] >> 0) & 0xFFFF) << 16);

    rec->validate_frames = (rec->validate_frames & 0xFFFFFFFC) | (((packedRecVal[4] >> 0) & 0x3) << 0);

    rec->replay_protect = (rec->replay_protect & 0xFFFFFFFE) | (((packedRecVal[4] >> 2) & 0x1) << 0);

    rec->anti_replay_window = (rec->anti_replay_window & 0xFFFFE000) | (((packedRecVal[4] >> 3) & 0x1FFF) << 0);
    rec->anti_replay_window = (rec->anti_replay_window & 0xE0001FFF) | (((packedRecVal[5] >> 0) & 0xFFFF) << 13);
    rec->anti_replay_window = (rec->anti_replay_window & 0x1FFFFFFF) | (((packedRecVal[6] >> 0) & 0x7) << 29);

    rec->receiving = (rec->receiving & 0xFFFFFFFE) | (((packedRecVal[6] >> 3) & 0x1) << 0);

    rec->fresh = (rec->fresh & 0xFFFFFFFE) | (((packedRecVal[6] >> 4) & 0x1) << 0);

    rec->an_rol = (rec->an_rol & 0xFFFFFFFE) | (((packedRecVal[6] >> 5) & 0x1) << 0);

    rec->reserved = (rec->reserved & 0xFFFFFC00) | (((packedRecVal[6] >> 6) & 0x3FF) << 0);
    rec->reserved = (rec->reserved & 0xFE0003FF) | (((packedRecVal[7] >> 0) & 0x7FFF) << 10);

    rec->valid = (rec->valid & 0xFFFFFFFE) | (((packedRecVal[7] >> 15) & 0x1) << 0);

    return 0;
}

int AQ_API_SetIngressSARecord(struct atl_hw *hw, const AQ_API_SEC_IngressSARecord* rec, uint16_t tableIndex)
{
    uint16_t packedRecVal[8];

    if (tableIndex >= NUMROWS_INGRESSSARECORD)
        return -EINVAL;

    memset(packedRecVal, 0, sizeof(uint16_t) * 8);

    packedRecVal[0] = (packedRecVal[0] & 0x0000) | (((rec->stop_time >> 0) & 0xFFFF) << 0);
    packedRecVal[1] = (packedRecVal[1] & 0x0000) | (((rec->stop_time >> 16) & 0xFFFF) << 0);

    packedRecVal[2] = (packedRecVal[2] & 0x0000) | (((rec->start_time >> 0) & 0xFFFF) << 0);
    packedRecVal[3] = (packedRecVal[3] & 0x0000) | (((rec->start_time >> 16) & 0xFFFF) << 0);

    packedRecVal[4] = (packedRecVal[4] & 0x0000) | (((rec->next_pn >> 0) & 0xFFFF) << 0);
    packedRecVal[5] = (packedRecVal[5] & 0x0000) | (((rec->next_pn >> 16) & 0xFFFF) << 0);

    packedRecVal[6] = (packedRecVal[6] & 0xFFFE) | (((rec->sat_nextpn >> 0) & 0x1) << 0);

    packedRecVal[6] = (packedRecVal[6] & 0xFFFD) | (((rec->in_use >> 0) & 0x1) << 1);

    packedRecVal[6] = (packedRecVal[6] & 0xFFFB) | (((rec->fresh >> 0) & 0x1) << 2);

    packedRecVal[6] = (packedRecVal[6] & 0x0007) | (((rec->reserved >> 0) & 0x1FFF) << 3);
    packedRecVal[7] = (packedRecVal[7] & 0x8000) | (((rec->reserved >> 13) & 0x7FFF) << 0);

    packedRecVal[7] = (packedRecVal[7] & 0x7FFF) | (((rec->valid >> 0) & 0x1) << 15);

    return SetRawSECIngressRecordVal(hw, packedRecVal, 8, 3, ROWOFFSET_INGRESSSARECORD + tableIndex);
}

int AQ_API_GetIngressSARecord(struct atl_hw *hw, AQ_API_SEC_IngressSARecord* rec, uint16_t tableIndex)
{
    uint16_t packedRecVal[8];
    int ret;

    if (tableIndex >= NUMROWS_INGRESSSARECORD)
        return -EINVAL;

    memset(rec, 0, sizeof(AQ_API_SEC_IngressSARecord));

    ret = GetRawSECIngressRecordVal(hw, packedRecVal, 8, 3, ROWOFFSET_INGRESSSARECORD + tableIndex);
    if (unlikely(ret))
        return ret;

    rec->stop_time = (rec->stop_time & 0xFFFF0000) | (((packedRecVal[0] >> 0) & 0xFFFF) << 0);
    rec->stop_time = (rec->stop_time & 0x0000FFFF) | (((packedRecVal[1] >> 0) & 0xFFFF) << 16);

    rec->start_time = (rec->start_time & 0xFFFF0000) | (((packedRecVal[2] >> 0) & 0xFFFF) << 0);
    rec->start_time = (rec->start_time & 0x0000FFFF) | (((packedRecVal[3] >> 0) & 0xFFFF) << 16);

    rec->next_pn = (rec->next_pn & 0xFFFF0000) | (((packedRecVal[4] >> 0) & 0xFFFF) << 0);
    rec->next_pn = (rec->next_pn & 0x0000FFFF) | (((packedRecVal[5] >> 0) & 0xFFFF) << 16);

    rec->sat_nextpn = (rec->sat_nextpn & 0xFFFFFFFE) | (((packedRecVal[6] >> 0) & 0x1) << 0);

    rec->in_use = (rec->in_use & 0xFFFFFFFE) | (((packedRecVal[6] >> 1) & 0x1) << 0);

    rec->fresh = (rec->fresh & 0xFFFFFFFE) | (((packedRecVal[6] >> 2) & 0x1) << 0);

    rec->reserved = (rec->reserved & 0xFFFFE000) | (((packedRecVal[6] >> 3) & 0x1FFF) << 0);
    rec->reserved = (rec->reserved & 0xF0001FFF) | (((packedRecVal[7] >> 0) & 0x7FFF) << 13);

    rec->valid = (rec->valid & 0xFFFFFFFE) | (((packedRecVal[7] >> 15) & 0x1) << 0);

    return 0;
}

int AQ_API_SetIngressSAKeyRecord(struct atl_hw *hw, const AQ_API_SEC_IngressSAKeyRecord* rec, uint16_t tableIndex)
{
    uint16_t packedRecVal[18];

    if (tableIndex >= NUMROWS_INGRESSSAKEYRECORD)
        return -EINVAL;

    memset(packedRecVal, 0, sizeof(uint16_t) * 18);

    packedRecVal[0] = (packedRecVal[0] & 0x0000) | (((rec->key[0] >> 0) & 0xFFFF) << 0);
    packedRecVal[1] = (packedRecVal[1] & 0x0000) | (((rec->key[0] >> 16) & 0xFFFF) << 0);

    packedRecVal[2] = (packedRecVal[2] & 0x0000) | (((rec->key[1] >> 0) & 0xFFFF) << 0);
    packedRecVal[3] = (packedRecVal[3] & 0x0000) | (((rec->key[1] >> 16) & 0xFFFF) << 0);

    packedRecVal[4] = (packedRecVal[4] & 0x0000) | (((rec->key[2] >> 0) & 0xFFFF) << 0);
    packedRecVal[5] = (packedRecVal[5] & 0x0000) | (((rec->key[2] >> 16) & 0xFFFF) << 0);

    packedRecVal[6] = (packedRecVal[6] & 0x0000) | (((rec->key[3] >> 0) & 0xFFFF) << 0);
    packedRecVal[7] = (packedRecVal[7] & 0x0000) | (((rec->key[3] >> 16) & 0xFFFF) << 0);

    packedRecVal[8] = (packedRecVal[8] & 0x0000) | (((rec->key[4] >> 0) & 0xFFFF) << 0);
    packedRecVal[9] = (packedRecVal[9] & 0x0000) | (((rec->key[4] >> 16) & 0xFFFF) << 0);

    packedRecVal[10] = (packedRecVal[10] & 0x0000) | (((rec->key[5] >> 0) & 0xFFFF) << 0);
    packedRecVal[11] = (packedRecVal[11] & 0x0000) | (((rec->key[5] >> 16) & 0xFFFF) << 0);

    packedRecVal[12] = (packedRecVal[12] & 0x0000) | (((rec->key[6] >> 0) & 0xFFFF) << 0);
    packedRecVal[13] = (packedRecVal[13] & 0x0000) | (((rec->key[6] >> 16) & 0xFFFF) << 0);

    packedRecVal[14] = (packedRecVal[14] & 0x0000) | (((rec->key[7] >> 0) & 0xFFFF) << 0);
    packedRecVal[15] = (packedRecVal[15] & 0x0000) | (((rec->key[7] >> 16) & 0xFFFF) << 0);

    packedRecVal[16] = (packedRecVal[16] & 0xFFFC) | (((rec->key_len >> 0) & 0x3) << 0);

    return SetRawSECIngressRecordVal(hw, packedRecVal, 18, 2, ROWOFFSET_INGRESSSAKEYRECORD + tableIndex);
}

int AQ_API_GetIngressSAKeyRecord(struct atl_hw *hw, AQ_API_SEC_IngressSAKeyRecord* rec, uint16_t tableIndex)
{
    uint16_t packedRecVal[18];
    int ret;

    if (tableIndex >= NUMROWS_INGRESSSAKEYRECORD)
        return -EINVAL;

    memset(rec, 0, sizeof(AQ_API_SEC_IngressSAKeyRecord));

    ret = GetRawSECIngressRecordVal(hw, packedRecVal, 18, 2, ROWOFFSET_INGRESSSAKEYRECORD + tableIndex);
    if (unlikely(ret))
        return ret;

    rec->key[0] = (rec->key[0] & 0xFFFF0000) | (((packedRecVal[0] >> 0) & 0xFFFF) << 0);
    rec->key[0] = (rec->key[0] & 0x0000FFFF) | (((packedRecVal[1] >> 0) & 0xFFFF) << 16);

    rec->key[1] = (rec->key[1] & 0xFFFF0000) | (((packedRecVal[2] >> 0) & 0xFFFF) << 0);
    rec->key[1] = (rec->key[1] & 0x0000FFFF) | (((packedRecVal[3] >> 0) & 0xFFFF) << 16);

    rec->key[2] = (rec->key[2] & 0xFFFF0000) | (((packedRecVal[4] >> 0) & 0xFFFF) << 0);
    rec->key[2] = (rec->key[2] & 0x0000FFFF) | (((packedRecVal[5] >> 0) & 0xFFFF) << 16);

    rec->key[3] = (rec->key[3] & 0xFFFF0000) | (((packedRecVal[6] >> 0) & 0xFFFF) << 0);
    rec->key[3] = (rec->key[3] & 0x0000FFFF) | (((packedRecVal[7] >> 0) & 0xFFFF) << 16);

    rec->key[4] = (rec->key[4] & 0xFFFF0000) | (((packedRecVal[8] >> 0) & 0xFFFF) << 0);
    rec->key[4] = (rec->key[4] & 0x0000FFFF) | (((packedRecVal[9] >> 0) & 0xFFFF) << 16);

    rec->key[5] = (rec->key[5] & 0xFFFF0000) | (((packedRecVal[10] >> 0) & 0xFFFF) << 0);
    rec->key[5] = (rec->key[5] & 0x0000FFFF) | (((packedRecVal[11] >> 0) & 0xFFFF) << 16);

    rec->key[6] = (rec->key[6] & 0xFFFF0000) | (((packedRecVal[12] >> 0) & 0xFFFF) << 0);
    rec->key[6] = (rec->key[6] & 0x0000FFFF) | (((packedRecVal[13] >> 0) & 0xFFFF) << 16);

    rec->key[7] = (rec->key[7] & 0xFFFF0000) | (((packedRecVal[14] >> 0) & 0xFFFF) << 0);
    rec->key[7] = (rec->key[7] & 0x0000FFFF) | (((packedRecVal[15] >> 0) & 0xFFFF) << 16);

    rec->key_len = (rec->key_len & 0xFFFFFFFC) | (((packedRecVal[16] >> 0) & 0x3) << 0);

    return 0;
}

int AQ_API_SetIngressPostClassRecord(struct atl_hw *hw, const AQ_API_SEC_IngressPostClassRecord* rec, uint16_t tableIndex)
{
    uint16_t packedRecVal[8];

    if (tableIndex >= NUMROWS_INGRESSPOSTCLASSRECORD)
        return -EINVAL;

    memset(packedRecVal, 0, sizeof(uint16_t) * 8);

    packedRecVal[0] = (packedRecVal[0] & 0xFF00) | (((rec->byte0 >> 0) & 0xFF) << 0);

    packedRecVal[0] = (packedRecVal[0] & 0x00FF) | (((rec->byte1 >> 0) & 0xFF) << 8);

    packedRecVal[1] = (packedRecVal[1] & 0xFF00) | (((rec->byte2 >> 0) & 0xFF) << 0);

    packedRecVal[1] = (packedRecVal[1] & 0x00FF) | (((rec->byte3 >> 0) & 0xFF) << 8);

    packedRecVal[2] = (packedRecVal[2] & 0x0000) | (((rec->eth_type >> 0) & 0xFFFF) << 0);

    packedRecVal[3] = (packedRecVal[3] & 0xFFFE) | (((rec->eth_type_valid >> 0) & 0x1) << 0);

    packedRecVal[3] = (packedRecVal[3] & 0xE001) | (((rec->vlan_id >> 0) & 0xFFF) << 1);

    packedRecVal[3] = (packedRecVal[3] & 0x1FFF) | (((rec->vlan_up >> 0) & 0x7) << 13);

    packedRecVal[4] = (packedRecVal[4] & 0xFFFE) | (((rec->vlan_valid >> 0) & 0x1) << 0);

    packedRecVal[4] = (packedRecVal[4] & 0xFFC1) | (((rec->sai >> 0) & 0x1F) << 1);

    packedRecVal[4] = (packedRecVal[4] & 0xFFBF) | (((rec->sai_hit >> 0) & 0x1) << 6);

    packedRecVal[4] = (packedRecVal[4] & 0xF87F) | (((rec->eth_type_mask >> 0) & 0xF) << 7);

    packedRecVal[4] = (packedRecVal[4] & 0x07FF) | (((rec->byte3_location >> 0) & 0x1F) << 11);
    packedRecVal[5] = (packedRecVal[5] & 0xFFFE) | (((rec->byte3_location >> 5) & 0x1) << 0);

    packedRecVal[5] = (packedRecVal[5] & 0xFFF9) | (((rec->byte3_mask >> 0) & 0x3) << 1);

    packedRecVal[5] = (packedRecVal[5] & 0xFE07) | (((rec->byte2_location >> 0) & 0x3F) << 3);

    packedRecVal[5] = (packedRecVal[5] & 0xF9FF) | (((rec->byte2_mask >> 0) & 0x3) << 9);

    packedRecVal[5] = (packedRecVal[5] & 0x07FF) | (((rec->byte1_location >> 0) & 0x1F) << 11);
    packedRecVal[6] = (packedRecVal[6] & 0xFFFE) | (((rec->byte1_location >> 5) & 0x1) << 0);

    packedRecVal[6] = (packedRecVal[6] & 0xFFF9) | (((rec->byte1_mask >> 0) & 0x3) << 1);

    packedRecVal[6] = (packedRecVal[6] & 0xFE07) | (((rec->byte0_location >> 0) & 0x3F) << 3);

    packedRecVal[6] = (packedRecVal[6] & 0xF9FF) | (((rec->byte0_mask >> 0) & 0x3) << 9);

    packedRecVal[6] = (packedRecVal[6] & 0xE7FF) | (((rec->eth_type_valid_mask >> 0) & 0x3) << 11);

    packedRecVal[6] = (packedRecVal[6] & 0x1FFF) | (((rec->vlan_id_mask >> 0) & 0x7) << 13);
    packedRecVal[7] = (packedRecVal[7] & 0xFFFE) | (((rec->vlan_id_mask >> 3) & 0x1) << 0);

    packedRecVal[7] = (packedRecVal[7] & 0xFFF9) | (((rec->vlan_up_mask >> 0) & 0x3) << 1);

    packedRecVal[7] = (packedRecVal[7] & 0xFFE7) | (((rec->vlan_valid_mask >> 0) & 0x3) << 3);

    packedRecVal[7] = (packedRecVal[7] & 0xFF9F) | (((rec->sai_mask >> 0) & 0x3) << 5);

    packedRecVal[7] = (packedRecVal[7] & 0xFE7F) | (((rec->sai_hit_mask >> 0) & 0x3) << 7);

    packedRecVal[7] = (packedRecVal[7] & 0xFDFF) | (((rec->firstlevel_actions >> 0) & 0x1) << 9);

    packedRecVal[7] = (packedRecVal[7] & 0xFBFF) | (((rec->secondlevel_actions >> 0) & 0x1) << 10);

    packedRecVal[7] = (packedRecVal[7] & 0x87FF) | (((rec->reserved >> 0) & 0xF) << 11);

    packedRecVal[7] = (packedRecVal[7] & 0x7FFF) | (((rec->valid >> 0) & 0x1) << 15);

    return SetRawSECIngressRecordVal(hw, packedRecVal, 8, 4, ROWOFFSET_INGRESSPOSTCLASSRECORD + tableIndex);
}

int AQ_API_GetIngressPostClassRecord(struct atl_hw *hw, AQ_API_SEC_IngressPostClassRecord* rec, uint16_t tableIndex)
{
    uint16_t packedRecVal[8];
    int ret;

    if (tableIndex >= NUMROWS_INGRESSPOSTCLASSRECORD)
        return -EINVAL;

      /* If the row that we want to read is odd, first read the previous even 
       * row, throw that value away, and finally read the desired row.
       * This is a workaround for EUR devices that allows us to read 
       * odd-numbered rows.  For HHD devices: this workaround will not work, 
       * so don't bother; odd-numbered rows are not readable. */
    if ((tableIndex % 2) > 0)
    {
        ret = GetRawSECIngressRecordVal(hw, packedRecVal, 8, 4, ROWOFFSET_INGRESSPOSTCLASSRECORD + tableIndex - 1);
        if (unlikely(ret))
            return ret;
    }

    memset(rec, 0, sizeof(AQ_API_SEC_IngressPostClassRecord));

    ret = GetRawSECIngressRecordVal(hw, packedRecVal, 8, 4, ROWOFFSET_INGRESSPOSTCLASSRECORD + tableIndex);
    if (unlikely(ret))
        return ret;

    rec->byte0 = (rec->byte0 & 0xFFFFFF00) | (((packedRecVal[0] >> 0) & 0xFF) << 0);

    rec->byte1 = (rec->byte1 & 0xFFFFFF00) | (((packedRecVal[0] >> 8) & 0xFF) << 0);

    rec->byte2 = (rec->byte2 & 0xFFFFFF00) | (((packedRecVal[1] >> 0) & 0xFF) << 0);

    rec->byte3 = (rec->byte3 & 0xFFFFFF00) | (((packedRecVal[1] >> 8) & 0xFF) << 0);

    rec->eth_type = (rec->eth_type & 0xFFFF0000) | (((packedRecVal[2] >> 0) & 0xFFFF) << 0);

    rec->eth_type_valid = (rec->eth_type_valid & 0xFFFFFFFE) | (((packedRecVal[3] >> 0) & 0x1) << 0);

    rec->vlan_id = (rec->vlan_id & 0xFFFFF000) | (((packedRecVal[3] >> 1) & 0xFFF) << 0);

    rec->vlan_up = (rec->vlan_up & 0xFFFFFFF8) | (((packedRecVal[3] >> 13) & 0x7) << 0);

    rec->vlan_valid = (rec->vlan_valid & 0xFFFFFFFE) | (((packedRecVal[4] >> 0) & 0x1) << 0);

    rec->sai = (rec->sai & 0xFFFFFFE0) | (((packedRecVal[4] >> 1) & 0x1F) << 0);

    rec->sai_hit = (rec->sai_hit & 0xFFFFFFFE) | (((packedRecVal[4] >> 6) & 0x1) << 0);

    rec->eth_type_mask = (rec->eth_type_mask & 0xFFFFFFF0) | (((packedRecVal[4] >> 7) & 0xF) << 0);

    rec->byte3_location = (rec->byte3_location & 0xFFFFFFE0) | (((packedRecVal[4] >> 11) & 0x1F) << 0);
    rec->byte3_location = (rec->byte3_location & 0xFFFFFFDF) | (((packedRecVal[5] >> 0) & 0x1) << 5);

    rec->byte3_mask = (rec->byte3_mask & 0xFFFFFFFC) | (((packedRecVal[5] >> 1) & 0x3) << 0);

    rec->byte2_location = (rec->byte2_location & 0xFFFFFFC0) | (((packedRecVal[5] >> 3) & 0x3F) << 0);

    rec->byte2_mask = (rec->byte2_mask & 0xFFFFFFFC) | (((packedRecVal[5] >> 9) & 0x3) << 0);

    rec->byte1_location = (rec->byte1_location & 0xFFFFFFE0) | (((packedRecVal[5] >> 11) & 0x1F) << 0);
    rec->byte1_location = (rec->byte1_location & 0xFFFFFFDF) | (((packedRecVal[6] >> 0) & 0x1) << 5);

    rec->byte1_mask = (rec->byte1_mask & 0xFFFFFFFC) | (((packedRecVal[6] >> 1) & 0x3) << 0);

    rec->byte0_location = (rec->byte0_location & 0xFFFFFFC0) | (((packedRecVal[6] >> 3) & 0x3F) << 0);

    rec->byte0_mask = (rec->byte0_mask & 0xFFFFFFFC) | (((packedRecVal[6] >> 9) & 0x3) << 0);

    rec->eth_type_valid_mask = (rec->eth_type_valid_mask & 0xFFFFFFFC) | (((packedRecVal[6] >> 11) & 0x3) << 0);

    rec->vlan_id_mask = (rec->vlan_id_mask & 0xFFFFFFF8) | (((packedRecVal[6] >> 13) & 0x7) << 0);
    rec->vlan_id_mask = (rec->vlan_id_mask & 0xFFFFFFF7) | (((packedRecVal[7] >> 0) & 0x1) << 3);

    rec->vlan_up_mask = (rec->vlan_up_mask & 0xFFFFFFFC) | (((packedRecVal[7] >> 1) & 0x3) << 0);

    rec->vlan_valid_mask = (rec->vlan_valid_mask & 0xFFFFFFFC) | (((packedRecVal[7] >> 3) & 0x3) << 0);

    rec->sai_mask = (rec->sai_mask & 0xFFFFFFFC) | (((packedRecVal[7] >> 5) & 0x3) << 0);

    rec->sai_hit_mask = (rec->sai_hit_mask & 0xFFFFFFFC) | (((packedRecVal[7] >> 7) & 0x3) << 0);

    rec->firstlevel_actions = (rec->firstlevel_actions & 0xFFFFFFFE) | (((packedRecVal[7] >> 9) & 0x1) << 0);

    rec->secondlevel_actions = (rec->secondlevel_actions & 0xFFFFFFFE) | (((packedRecVal[7] >> 10) & 0x1) << 0);

    rec->reserved = (rec->reserved & 0xFFFFFFF0) | (((packedRecVal[7] >> 11) & 0xF) << 0);

    rec->valid = (rec->valid & 0xFFFFFFFE) | (((packedRecVal[7] >> 15) & 0x1) << 0);

    return 0;
}


int AQ_API_SetIngressPostCTLFRecord(struct atl_hw *hw, const AQ_API_SEC_IngressPostCTLFRecord* rec, uint16_t tableIndex)
{
    uint16_t packedRecVal[6];

    if (tableIndex >= NUMROWS_INGRESSPOSTCTLFRECORD)
        return -EINVAL;

    memset(packedRecVal, 0, sizeof(uint16_t) * 6);

    packedRecVal[0] = (packedRecVal[0] & 0x0000) | (((rec->sa_da[0] >> 0) & 0xFFFF) << 0);
    packedRecVal[1] = (packedRecVal[1] & 0x0000) | (((rec->sa_da[0] >> 16) & 0xFFFF) << 0);

    packedRecVal[2] = (packedRecVal[2] & 0x0000) | (((rec->sa_da[1] >> 0) & 0xFFFF) << 0);

    packedRecVal[3] = (packedRecVal[3] & 0x0000) | (((rec->eth_type >> 0) & 0xFFFF) << 0);

    packedRecVal[4] = (packedRecVal[4] & 0x0000) | (((rec->match_mask >> 0) & 0xFFFF) << 0);

    packedRecVal[5] = (packedRecVal[5] & 0xFFF0) | (((rec->match_type >> 0) & 0xF) << 0);

    packedRecVal[5] = (packedRecVal[5] & 0xFFEF) | (((rec->action >> 0) & 0x1) << 4);

    return SetRawSECIngressRecordVal(hw, packedRecVal, 6, 5, ROWOFFSET_INGRESSPOSTCTLFRECORD + tableIndex);
}

int AQ_API_GetIngressPostCTLFRecord(struct atl_hw *hw, AQ_API_SEC_IngressPostCTLFRecord* rec, uint16_t tableIndex)
{
    uint16_t packedRecVal[6];
    int ret;

    if (tableIndex >= NUMROWS_INGRESSPOSTCTLFRECORD)
        return -EINVAL;

      /* If the row that we want to read is odd, first read the previous even 
       * row, throw that value away, and finally read the desired row.
       * This is a workaround for EUR devices that allows us to read 
       * odd-numbered rows.  For HHD devices: this workaround will not work, 
       * so don't bother; odd-numbered rows are not readable. */
    if ((tableIndex % 2) > 0)
    {
        ret = GetRawSECIngressRecordVal(hw, packedRecVal, 6, 5, ROWOFFSET_INGRESSPOSTCTLFRECORD + tableIndex - 1);
        if (unlikely(ret))
            return ret;
    }

    memset(rec, 0, sizeof(AQ_API_SEC_IngressPostCTLFRecord));

    ret = GetRawSECIngressRecordVal(hw, packedRecVal, 6, 5, ROWOFFSET_INGRESSPOSTCTLFRECORD + tableIndex);
    if (unlikely(ret))
        return ret;

    rec->sa_da[0] = (rec->sa_da[0] & 0xFFFF0000) | (((packedRecVal[0] >> 0) & 0xFFFF) << 0);
    rec->sa_da[0] = (rec->sa_da[0] & 0x0000FFFF) | (((packedRecVal[1] >> 0) & 0xFFFF) << 16);

    rec->sa_da[1] = (rec->sa_da[1] & 0xFFFF0000) | (((packedRecVal[2] >> 0) & 0xFFFF) << 0);

    rec->eth_type = (rec->eth_type & 0xFFFF0000) | (((packedRecVal[3] >> 0) & 0xFFFF) << 0);

    rec->match_mask = (rec->match_mask & 0xFFFF0000) | (((packedRecVal[4] >> 0) & 0xFFFF) << 0);

    rec->match_type = (rec->match_type & 0xFFFFFFF0) | (((packedRecVal[5] >> 0) & 0xF) << 0);

    rec->action = (rec->action & 0xFFFFFFFE) | (((packedRecVal[5] >> 4) & 0x1) << 0);

    return 0;
}

int AQ_API_SetEgressCTLFRecord(struct atl_hw *hw, const AQ_API_SEC_EgressCTLFRecord* rec, uint16_t tableIndex)
{
    uint16_t packedRecVal[6];

    if (tableIndex >= NUMROWS_EGRESSCTLFRECORD)
        return -EINVAL;

    memset(packedRecVal, 0, sizeof(uint16_t) * 6);

    packedRecVal[0] = (packedRecVal[0] & 0x0000) | (((rec->sa_da[0] >> 0) & 0xFFFF) << 0);
    packedRecVal[1] = (packedRecVal[1] & 0x0000) | (((rec->sa_da[0] >> 16) & 0xFFFF) << 0);

    packedRecVal[2] = (packedRecVal[2] & 0x0000) | (((rec->sa_da[1] >> 0) & 0xFFFF) << 0);

    packedRecVal[3] = (packedRecVal[3] & 0x0000) | (((rec->eth_type >> 0) & 0xFFFF) << 0);

    packedRecVal[4] = (packedRecVal[4] & 0x0000) | (((rec->match_mask >> 0) & 0xFFFF) << 0);

    packedRecVal[5] = (packedRecVal[5] & 0xFFF0) | (((rec->match_type >> 0) & 0xF) << 0);

    packedRecVal[5] = (packedRecVal[5] & 0xFFEF) | (((rec->action >> 0) & 0x1) << 4);

    return SetRawSECEgressRecordVal(hw, packedRecVal, 6, 0, ROWOFFSET_EGRESSCTLFRECORD + tableIndex);
}

int AQ_API_GetEgressCTLFRecord(struct atl_hw *hw, AQ_API_SEC_EgressCTLFRecord* rec, uint16_t tableIndex)
{
    uint16_t packedRecVal[6];
    int ret;

    if (tableIndex >= NUMROWS_EGRESSCTLFRECORD)
        return -EINVAL;

      /* If the row that we want to read is odd, first read the previous even 
       * row, throw that value away, and finally read the desired row.
       * This is a workaround for EUR devices that allows us to read 
       * odd-numbered rows.  For HHD devices: this workaround will not work, 
       * so don't bother; odd-numbered rows are not readable. */
    if ((tableIndex % 2) > 0)
    {
        ret = GetRawSECEgressRecordVal(hw, packedRecVal, 6, 0, ROWOFFSET_EGRESSCTLFRECORD + tableIndex - 1);
        if (unlikely(ret))
            return ret;
    }

    memset(rec, 0, sizeof(AQ_API_SEC_EgressCTLFRecord));

    ret = GetRawSECEgressRecordVal(hw, packedRecVal, 6, 0, ROWOFFSET_EGRESSCTLFRECORD + tableIndex);
    if (unlikely(ret))
        return ret;

    rec->sa_da[0] = (rec->sa_da[0] & 0xFFFF0000) | (((packedRecVal[0] >> 0) & 0xFFFF) << 0);
    rec->sa_da[0] = (rec->sa_da[0] & 0x0000FFFF) | (((packedRecVal[1] >> 0) & 0xFFFF) << 16);

    rec->sa_da[1] = (rec->sa_da[1] & 0xFFFF0000) | (((packedRecVal[2] >> 0) & 0xFFFF) << 0);

    rec->eth_type = (rec->eth_type & 0xFFFF0000) | (((packedRecVal[3] >> 0) & 0xFFFF) << 0);

    rec->match_mask = (rec->match_mask & 0xFFFF0000) | (((packedRecVal[4] >> 0) & 0xFFFF) << 0);

    rec->match_type = (rec->match_type & 0xFFFFFFF0) | (((packedRecVal[5] >> 0) & 0xF) << 0);

    rec->action = (rec->action & 0xFFFFFFFE) | (((packedRecVal[5] >> 4) & 0x1) << 0);

    return 0;
}

int AQ_API_SetEgressClassRecord(struct atl_hw *hw, const AQ_API_SEC_EgressClassRecord* rec, uint16_t tableIndex)
{
    uint16_t packedRecVal[28];

    if (tableIndex >= NUMROWS_EGRESSCLASSRECORD)
        return -EINVAL;

    memset(packedRecVal, 0, sizeof(uint16_t) * 28);

    packedRecVal[0] = (packedRecVal[0] & 0xF000) | (((rec->vlan_id >> 0) & 0xFFF) << 0);

    packedRecVal[0] = (packedRecVal[0] & 0x8FFF) | (((rec->vlan_up >> 0) & 0x7) << 12);

    packedRecVal[0] = (packedRecVal[0] & 0x7FFF) | (((rec->vlan_valid >> 0) & 0x1) << 15);

    packedRecVal[1] = (packedRecVal[1] & 0xFF00) | (((rec->byte3 >> 0) & 0xFF) << 0);

    packedRecVal[1] = (packedRecVal[1] & 0x00FF) | (((rec->byte2 >> 0) & 0xFF) << 8);

    packedRecVal[2] = (packedRecVal[2] & 0xFF00) | (((rec->byte1 >> 0) & 0xFF) << 0);

    packedRecVal[2] = (packedRecVal[2] & 0x00FF) | (((rec->byte0 >> 0) & 0xFF) << 8);

    packedRecVal[3] = (packedRecVal[3] & 0xFF00) | (((rec->tci >> 0) & 0xFF) << 0);

    packedRecVal[3] = (packedRecVal[3] & 0x00FF) | (((rec->sci[0] >> 0) & 0xFF) << 8);
    packedRecVal[4] = (packedRecVal[4] & 0x0000) | (((rec->sci[0] >> 8) & 0xFFFF) << 0);
    packedRecVal[5] = (packedRecVal[5] & 0xFF00) | (((rec->sci[0] >> 24) & 0xFF) << 0);

    packedRecVal[5] = (packedRecVal[5] & 0x00FF) | (((rec->sci[1] >> 0) & 0xFF) << 8);
    packedRecVal[6] = (packedRecVal[6] & 0x0000) | (((rec->sci[1] >> 8) & 0xFFFF) << 0);
    packedRecVal[7] = (packedRecVal[7] & 0xFF00) | (((rec->sci[1] >> 24) & 0xFF) << 0);

    packedRecVal[7] = (packedRecVal[7] & 0x00FF) | (((rec->eth_type >> 0) & 0xFF) << 8);
    packedRecVal[8] = (packedRecVal[8] & 0xFF00) | (((rec->eth_type >> 8) & 0xFF) << 0);

    packedRecVal[8] = (packedRecVal[8] & 0x00FF) | (((rec->snap[0] >> 0) & 0xFF) << 8);
    packedRecVal[9] = (packedRecVal[9] & 0x0000) | (((rec->snap[0] >> 8) & 0xFFFF) << 0);
    packedRecVal[10] = (packedRecVal[10] & 0xFF00) | (((rec->snap[0] >> 24) & 0xFF) << 0);

    packedRecVal[10] = (packedRecVal[10] & 0x00FF) | (((rec->snap[1] >> 0) & 0xFF) << 8);

    packedRecVal[11] = (packedRecVal[11] & 0x0000) | (((rec->llc >> 0) & 0xFFFF) << 0);
    packedRecVal[12] = (packedRecVal[12] & 0xFF00) | (((rec->llc >> 16) & 0xFF) << 0);

    packedRecVal[12] = (packedRecVal[12] & 0x00FF) | (((rec->mac_sa[0] >> 0) & 0xFF) << 8);
    packedRecVal[13] = (packedRecVal[13] & 0x0000) | (((rec->mac_sa[0] >> 8) & 0xFFFF) << 0);
    packedRecVal[14] = (packedRecVal[14] & 0xFF00) | (((rec->mac_sa[0] >> 24) & 0xFF) << 0);

    packedRecVal[14] = (packedRecVal[14] & 0x00FF) | (((rec->mac_sa[1] >> 0) & 0xFF) << 8);
    packedRecVal[15] = (packedRecVal[15] & 0xFF00) | (((rec->mac_sa[1] >> 8) & 0xFF) << 0);

    packedRecVal[15] = (packedRecVal[15] & 0x00FF) | (((rec->mac_da[0] >> 0) & 0xFF) << 8);
    packedRecVal[16] = (packedRecVal[16] & 0x0000) | (((rec->mac_da[0] >> 8) & 0xFFFF) << 0);
    packedRecVal[17] = (packedRecVal[17] & 0xFF00) | (((rec->mac_da[0] >> 24) & 0xFF) << 0);

    packedRecVal[17] = (packedRecVal[17] & 0x00FF) | (((rec->mac_da[1] >> 0) & 0xFF) << 8);
    packedRecVal[18] = (packedRecVal[18] & 0xFF00) | (((rec->mac_da[1] >> 8) & 0xFF) << 0);

    packedRecVal[18] = (packedRecVal[18] & 0x00FF) | (((rec->pn >> 0) & 0xFF) << 8);
    packedRecVal[19] = (packedRecVal[19] & 0x0000) | (((rec->pn >> 8) & 0xFFFF) << 0);
    packedRecVal[20] = (packedRecVal[20] & 0xFF00) | (((rec->pn >> 24) & 0xFF) << 0);

    packedRecVal[20] = (packedRecVal[20] & 0xC0FF) | (((rec->byte3_location >> 0) & 0x3F) << 8);

    packedRecVal[20] = (packedRecVal[20] & 0xBFFF) | (((rec->byte3_mask >> 0) & 0x1) << 14);

    packedRecVal[20] = (packedRecVal[20] & 0x7FFF) | (((rec->byte2_location >> 0) & 0x1) << 15);
    packedRecVal[21] = (packedRecVal[21] & 0xFFE0) | (((rec->byte2_location >> 1) & 0x1F) << 0);

    packedRecVal[21] = (packedRecVal[21] & 0xFFDF) | (((rec->byte2_mask >> 0) & 0x1) << 5);

    packedRecVal[21] = (packedRecVal[21] & 0xF03F) | (((rec->byte1_location >> 0) & 0x3F) << 6);

    packedRecVal[21] = (packedRecVal[21] & 0xEFFF) | (((rec->byte1_mask >> 0) & 0x1) << 12);

    packedRecVal[21] = (packedRecVal[21] & 0x1FFF) | (((rec->byte0_location >> 0) & 0x7) << 13);
    packedRecVal[22] = (packedRecVal[22] & 0xFFF8) | (((rec->byte0_location >> 3) & 0x7) << 0);

    packedRecVal[22] = (packedRecVal[22] & 0xFFF7) | (((rec->byte0_mask >> 0) & 0x1) << 3);

    packedRecVal[22] = (packedRecVal[22] & 0xFFCF) | (((rec->vlan_id_mask >> 0) & 0x3) << 4);

    packedRecVal[22] = (packedRecVal[22] & 0xFFBF) | (((rec->vlan_up_mask >> 0) & 0x1) << 6);

    packedRecVal[22] = (packedRecVal[22] & 0xFF7F) | (((rec->vlan_valid_mask >> 0) & 0x1) << 7);

    packedRecVal[22] = (packedRecVal[22] & 0x00FF) | (((rec->tci_mask >> 0) & 0xFF) << 8);

    packedRecVal[23] = (packedRecVal[23] & 0xFF00) | (((rec->sci_mask >> 0) & 0xFF) << 0);

    packedRecVal[23] = (packedRecVal[23] & 0xFCFF) | (((rec->eth_type_mask >> 0) & 0x3) << 8);

    packedRecVal[23] = (packedRecVal[23] & 0x83FF) | (((rec->snap_mask >> 0) & 0x1F) << 10);

    packedRecVal[23] = (packedRecVal[23] & 0x7FFF) | (((rec->llc_mask >> 0) & 0x1) << 15);
    packedRecVal[24] = (packedRecVal[24] & 0xFFFC) | (((rec->llc_mask >> 1) & 0x3) << 0);

    packedRecVal[24] = (packedRecVal[24] & 0xFF03) | (((rec->sa_mask >> 0) & 0x3F) << 2);

    packedRecVal[24] = (packedRecVal[24] & 0xC0FF) | (((rec->da_mask >> 0) & 0x3F) << 8);

    packedRecVal[24] = (packedRecVal[24] & 0x3FFF) | (((rec->pn_mask >> 0) & 0x3) << 14);
    packedRecVal[25] = (packedRecVal[25] & 0xFFFC) | (((rec->pn_mask >> 2) & 0x3) << 0);

    packedRecVal[25] = (packedRecVal[25] & 0xFFFB) | (((rec->eight02dot2 >> 0) & 0x1) << 2);

    packedRecVal[25] = (packedRecVal[25] & 0xFFF7) | (((rec->tci_sc >> 0) & 0x1) << 3);

    packedRecVal[25] = (packedRecVal[25] & 0xFFEF) | (((rec->tci_87543 >> 0) & 0x1) << 4);

    packedRecVal[25] = (packedRecVal[25] & 0xFFDF) | (((rec->exp_sectag_en >> 0) & 0x1) << 5);

    packedRecVal[25] = (packedRecVal[25] & 0xF83F) | (((rec->sc_idx >> 0) & 0x1F) << 6);

    packedRecVal[25] = (packedRecVal[25] & 0xE7FF) | (((rec->sc_sa >> 0) & 0x3) << 11);

    packedRecVal[25] = (packedRecVal[25] & 0xDFFF) | (((rec->debug >> 0) & 0x1) << 13);

    packedRecVal[25] = (packedRecVal[25] & 0x3FFF) | (((rec->action >> 0) & 0x3) << 14);

    packedRecVal[26] = (packedRecVal[26] & 0xFFF7) | (((rec->valid >> 0) & 0x1) << 3);

    return SetRawSECEgressRecordVal(hw, packedRecVal, 28, 1, ROWOFFSET_EGRESSCLASSRECORD + tableIndex);
}

int AQ_API_GetEgressClassRecord(struct atl_hw *hw, AQ_API_SEC_EgressClassRecord* rec, uint16_t tableIndex)
{
    uint16_t packedRecVal[28];
    int ret;

    if (tableIndex >= NUMROWS_EGRESSCLASSRECORD)
        return -EINVAL;

      /* If the row that we want to read is odd, first read the previous even 
       * row, throw that value away, and finally read the desired row.
       * This is a workaround for EUR devices that allows us to read 
       * odd-numbered rows.  For HHD devices: this workaround will not work, 
       * so don't bother; odd-numbered rows are not readable. */
    if ((tableIndex % 2) > 0) {
        ret = GetRawSECEgressRecordVal(hw, packedRecVal, 28, 1, ROWOFFSET_EGRESSCLASSRECORD + tableIndex - 1);
        if (unlikely(ret))
            return ret;
    }

    memset(rec, 0, sizeof(AQ_API_SEC_EgressClassRecord));

    ret = GetRawSECEgressRecordVal(hw, packedRecVal, 28, 1, ROWOFFSET_EGRESSCLASSRECORD + tableIndex);
    if (unlikely(ret))
        return ret;

    rec->vlan_id = (rec->vlan_id & 0xFFFFF000) | (((packedRecVal[0] >> 0) & 0xFFF) << 0);

    rec->vlan_up = (rec->vlan_up & 0xFFFFFFF8) | (((packedRecVal[0] >> 12) & 0x7) << 0);

    rec->vlan_valid = (rec->vlan_valid & 0xFFFFFFFE) | (((packedRecVal[0] >> 15) & 0x1) << 0);

    rec->byte3 = (rec->byte3 & 0xFFFFFF00) | (((packedRecVal[1] >> 0) & 0xFF) << 0);

    rec->byte2 = (rec->byte2 & 0xFFFFFF00) | (((packedRecVal[1] >> 8) & 0xFF) << 0);

    rec->byte1 = (rec->byte1 & 0xFFFFFF00) | (((packedRecVal[2] >> 0) & 0xFF) << 0);

    rec->byte0 = (rec->byte0 & 0xFFFFFF00) | (((packedRecVal[2] >> 8) & 0xFF) << 0);

    rec->tci = (rec->tci & 0xFFFFFF00) | (((packedRecVal[3] >> 0) & 0xFF) << 0);

    rec->sci[0] = (rec->sci[0] & 0xFFFFFF00) | (((packedRecVal[3] >> 8) & 0xFF) << 0);
    rec->sci[0] = (rec->sci[0] & 0xFF0000FF) | (((packedRecVal[4] >> 0) & 0xFFFF) << 8);
    rec->sci[0] = (rec->sci[0] & 0x00FFFFFF) | (((packedRecVal[5] >> 0) & 0xFF) << 24);

    rec->sci[1] = (rec->sci[1] & 0xFFFFFF00) | (((packedRecVal[5] >> 8) & 0xFF) << 0);
    rec->sci[1] = (rec->sci[1] & 0xFF0000FF) | (((packedRecVal[6] >> 0) & 0xFFFF) << 8);
    rec->sci[1] = (rec->sci[1] & 0x00FFFFFF) | (((packedRecVal[7] >> 0) & 0xFF) << 24);

    rec->eth_type = (rec->eth_type & 0xFFFFFF00) | (((packedRecVal[7] >> 8) & 0xFF) << 0);
    rec->eth_type = (rec->eth_type & 0xFFFF00FF) | (((packedRecVal[8] >> 0) & 0xFF) << 8);

    rec->snap[0] = (rec->snap[0] & 0xFFFFFF00) | (((packedRecVal[8] >> 8) & 0xFF) << 0);
    rec->snap[0] = (rec->snap[0] & 0xFF0000FF) | (((packedRecVal[9] >> 0) & 0xFFFF) << 8);
    rec->snap[0] = (rec->snap[0] & 0x00FFFFFF) | (((packedRecVal[10] >> 0) & 0xFF) << 24);

    rec->snap[1] = (rec->snap[1] & 0xFFFFFF00) | (((packedRecVal[10] >> 8) & 0xFF) << 0);

    rec->llc = (rec->llc & 0xFFFF0000) | (((packedRecVal[11] >> 0) & 0xFFFF) << 0);
    rec->llc = (rec->llc & 0xFF00FFFF) | (((packedRecVal[12] >> 0) & 0xFF) << 16);

    rec->mac_sa[0] = (rec->mac_sa[0] & 0xFFFFFF00) | (((packedRecVal[12] >> 8) & 0xFF) << 0);
    rec->mac_sa[0] = (rec->mac_sa[0] & 0xFF0000FF) | (((packedRecVal[13] >> 0) & 0xFFFF) << 8);
    rec->mac_sa[0] = (rec->mac_sa[0] & 0x00FFFFFF) | (((packedRecVal[14] >> 0) & 0xFF) << 24);

    rec->mac_sa[1] = (rec->mac_sa[1] & 0xFFFFFF00) | (((packedRecVal[14] >> 8) & 0xFF) << 0);
    rec->mac_sa[1] = (rec->mac_sa[1] & 0xFFFF00FF) | (((packedRecVal[15] >> 0) & 0xFF) << 8);

    rec->mac_da[0] = (rec->mac_da[0] & 0xFFFFFF00) | (((packedRecVal[15] >> 8) & 0xFF) << 0);
    rec->mac_da[0] = (rec->mac_da[0] & 0xFF0000FF) | (((packedRecVal[16] >> 0) & 0xFFFF) << 8);
    rec->mac_da[0] = (rec->mac_da[0] & 0x00FFFFFF) | (((packedRecVal[17] >> 0) & 0xFF) << 24);

    rec->mac_da[1] = (rec->mac_da[1] & 0xFFFFFF00) | (((packedRecVal[17] >> 8) & 0xFF) << 0);
    rec->mac_da[1] = (rec->mac_da[1] & 0xFFFF00FF) | (((packedRecVal[18] >> 0) & 0xFF) << 8);

    rec->pn = (rec->pn & 0xFFFFFF00) | (((packedRecVal[18] >> 8) & 0xFF) << 0);
    rec->pn = (rec->pn & 0xFF0000FF) | (((packedRecVal[19] >> 0) & 0xFFFF) << 8);
    rec->pn = (rec->pn & 0x00FFFFFF) | (((packedRecVal[20] >> 0) & 0xFF) << 24);

    rec->byte3_location = (rec->byte3_location & 0xFFFFFFC0) | (((packedRecVal[20] >> 8) & 0x3F) << 0);

    rec->byte3_mask = (rec->byte3_mask & 0xFFFFFFFE) | (((packedRecVal[20] >> 14) & 0x1) << 0);

    rec->byte2_location = (rec->byte2_location & 0xFFFFFFFE) | (((packedRecVal[20] >> 15) & 0x1) << 0);
    rec->byte2_location = (rec->byte2_location & 0xFFFFFFC1) | (((packedRecVal[21] >> 0) & 0x1F) << 1);

    rec->byte2_mask = (rec->byte2_mask & 0xFFFFFFFE) | (((packedRecVal[21] >> 5) & 0x1) << 0);

    rec->byte1_location = (rec->byte1_location & 0xFFFFFFC0) | (((packedRecVal[21] >> 6) & 0x3F) << 0);

    rec->byte1_mask = (rec->byte1_mask & 0xFFFFFFFE) | (((packedRecVal[21] >> 12) & 0x1) << 0);

    rec->byte0_location = (rec->byte0_location & 0xFFFFFFF8) | (((packedRecVal[21] >> 13) & 0x7) << 0);
    rec->byte0_location = (rec->byte0_location & 0xFFFFFFC7) | (((packedRecVal[22] >> 0) & 0x7) << 3);

    rec->byte0_mask = (rec->byte0_mask & 0xFFFFFFFE) | (((packedRecVal[22] >> 3) & 0x1) << 0);

    rec->vlan_id_mask = (rec->vlan_id_mask & 0xFFFFFFFC) | (((packedRecVal[22] >> 4) & 0x3) << 0);

    rec->vlan_up_mask = (rec->vlan_up_mask & 0xFFFFFFFE) | (((packedRecVal[22] >> 6) & 0x1) << 0);

    rec->vlan_valid_mask = (rec->vlan_valid_mask & 0xFFFFFFFE) | (((packedRecVal[22] >> 7) & 0x1) << 0);

    rec->tci_mask = (rec->tci_mask & 0xFFFFFF00) | (((packedRecVal[22] >> 8) & 0xFF) << 0);

    rec->sci_mask = (rec->sci_mask & 0xFFFFFF00) | (((packedRecVal[23] >> 0) & 0xFF) << 0);

    rec->eth_type_mask = (rec->eth_type_mask & 0xFFFFFFFC) | (((packedRecVal[23] >> 8) & 0x3) << 0);

    rec->snap_mask = (rec->snap_mask & 0xFFFFFFE0) | (((packedRecVal[23] >> 10) & 0x1F) << 0);

    rec->llc_mask = (rec->llc_mask & 0xFFFFFFFE) | (((packedRecVal[23] >> 15) & 0x1) << 0);
    rec->llc_mask = (rec->llc_mask & 0xFFFFFFF9) | (((packedRecVal[24] >> 0) & 0x3) << 1);

    rec->sa_mask = (rec->sa_mask & 0xFFFFFFC0) | (((packedRecVal[24] >> 2) & 0x3F) << 0);

    rec->da_mask = (rec->da_mask & 0xFFFFFFC0) | (((packedRecVal[24] >> 8) & 0x3F) << 0);

    rec->pn_mask = (rec->pn_mask & 0xFFFFFFFC) | (((packedRecVal[24] >> 14) & 0x3) << 0);
    rec->pn_mask = (rec->pn_mask & 0xFFFFFFF3) | (((packedRecVal[25] >> 0) & 0x3) << 2);

    rec->eight02dot2 = (rec->eight02dot2 & 0xFFFFFFFE) | (((packedRecVal[25] >> 2) & 0x1) << 0);

    rec->tci_sc = (rec->tci_sc & 0xFFFFFFFE) | (((packedRecVal[25] >> 3) & 0x1) << 0);

    rec->tci_87543 = (rec->tci_87543 & 0xFFFFFFFE) | (((packedRecVal[25] >> 4) & 0x1) << 0);

    rec->exp_sectag_en = (rec->exp_sectag_en & 0xFFFFFFFE) | (((packedRecVal[25] >> 5) & 0x1) << 0);

    rec->sc_idx = (rec->sc_idx & 0xFFFFFFE0) | (((packedRecVal[25] >> 6) & 0x1F) << 0);

    rec->sc_sa = (rec->sc_sa & 0xFFFFFFFC) | (((packedRecVal[25] >> 11) & 0x3) << 0);

    rec->debug = (rec->debug & 0xFFFFFFFE) | (((packedRecVal[25] >> 13) & 0x1) << 0);

    rec->action = (rec->action & 0xFFFFFFFC) | (((packedRecVal[25] >> 14) & 0x3) << 0);

    rec->valid = (rec->valid & 0xFFFFFFFE) | (((packedRecVal[26] >> 3) & 0x1) << 0);

    return 0;
}

int AQ_API_SetEgressSCRecord(struct atl_hw *hw, const AQ_API_SEC_EgressSCRecord* rec, uint16_t tableIndex)
{
    uint16_t packedRecVal[8];

    if (tableIndex >= NUMROWS_EGRESSSCRECORD)
        return -EINVAL;

    memset(packedRecVal, 0, sizeof(uint16_t) * 8);

    packedRecVal[0] = (packedRecVal[0] & 0x0000) | (((rec->start_time >> 0) & 0xFFFF) << 0);
    packedRecVal[1] = (packedRecVal[1] & 0x0000) | (((rec->start_time >> 16) & 0xFFFF) << 0);

    packedRecVal[2] = (packedRecVal[2] & 0x0000) | (((rec->stop_time >> 0) & 0xFFFF) << 0);
    packedRecVal[3] = (packedRecVal[3] & 0x0000) | (((rec->stop_time >> 16) & 0xFFFF) << 0);

    packedRecVal[4] = (packedRecVal[4] & 0xFFFC) | (((rec->curr_an >> 0) & 0x3) << 0);

    packedRecVal[4] = (packedRecVal[4] & 0xFFFB) | (((rec->an_roll >> 0) & 0x1) << 2);

    packedRecVal[4] = (packedRecVal[4] & 0xFE07) | (((rec->tci >> 0) & 0x3F) << 3);

    packedRecVal[4] = (packedRecVal[4] & 0x01FF) | (((rec->enc_off >> 0) & 0x7F) << 9);
    packedRecVal[5] = (packedRecVal[5] & 0xFFFE) | (((rec->enc_off >> 7) & 0x1) << 0);

    packedRecVal[5] = (packedRecVal[5] & 0xFFFD) | (((rec->protect >> 0) & 0x1) << 1);

    packedRecVal[5] = (packedRecVal[5] & 0xFFFB) | (((rec->recv >> 0) & 0x1) << 2);

    packedRecVal[5] = (packedRecVal[5] & 0xFFF7) | (((rec->fresh >> 0) & 0x1) << 3);

    packedRecVal[5] = (packedRecVal[5] & 0xFFCF) | (((rec->sak_len >> 0) & 0x3) << 4);

    packedRecVal[7] = (packedRecVal[7] & 0x7FFF) | (((rec->valid >> 0) & 0x1) << 15);

    return SetRawSECEgressRecordVal(hw, packedRecVal, 8, 2, ROWOFFSET_EGRESSSCRECORD + tableIndex);
}

int AQ_API_GetEgressSCRecord(struct atl_hw *hw, AQ_API_SEC_EgressSCRecord* rec, uint16_t tableIndex)
{
    uint16_t packedRecVal[8];
    int ret;

    if (tableIndex >= NUMROWS_EGRESSSCRECORD)
        return -EINVAL;

    memset(rec, 0, sizeof(AQ_API_SEC_EgressSCRecord));

    ret = GetRawSECEgressRecordVal(hw, packedRecVal, 8, 2, ROWOFFSET_EGRESSSCRECORD + tableIndex);
    if (unlikely(ret))
        return ret;

    rec->start_time = (rec->start_time & 0xFFFF0000) | (((packedRecVal[0] >> 0) & 0xFFFF) << 0);
    rec->start_time = (rec->start_time & 0x0000FFFF) | (((packedRecVal[1] >> 0) & 0xFFFF) << 16);

    rec->stop_time = (rec->stop_time & 0xFFFF0000) | (((packedRecVal[2] >> 0) & 0xFFFF) << 0);
    rec->stop_time = (rec->stop_time & 0x0000FFFF) | (((packedRecVal[3] >> 0) & 0xFFFF) << 16);

    rec->curr_an = (rec->curr_an & 0xFFFFFFFC) | (((packedRecVal[4] >> 0) & 0x3) << 0);

    rec->an_roll = (rec->an_roll & 0xFFFFFFFE) | (((packedRecVal[4] >> 2) & 0x1) << 0);

    rec->tci = (rec->tci & 0xFFFFFFC0) | (((packedRecVal[4] >> 3) & 0x3F) << 0);

    rec->enc_off = (rec->enc_off & 0xFFFFFF80) | (((packedRecVal[4] >> 9) & 0x7F) << 0);
    rec->enc_off = (rec->enc_off & 0xFFFFFF7F) | (((packedRecVal[5] >> 0) & 0x1) << 7);

    rec->protect = (rec->protect & 0xFFFFFFFE) | (((packedRecVal[5] >> 1) & 0x1) << 0);

    rec->recv = (rec->recv & 0xFFFFFFFE) | (((packedRecVal[5] >> 2) & 0x1) << 0);

    rec->fresh = (rec->fresh & 0xFFFFFFFE) | (((packedRecVal[5] >> 3) & 0x1) << 0);

    rec->sak_len = (rec->sak_len & 0xFFFFFFFC) | (((packedRecVal[5] >> 4) & 0x3) << 0);

    rec->valid = (rec->valid & 0xFFFFFFFE) | (((packedRecVal[7] >> 15) & 0x1) << 0);

    return 0;
}

int AQ_API_SetEgressSARecord(struct atl_hw *hw, const AQ_API_SEC_EgressSARecord* rec, uint16_t tableIndex)
{
    uint16_t packedRecVal[8];

    if (tableIndex >= NUMROWS_EGRESSSARECORD)
        return -EINVAL;

    memset(packedRecVal, 0, sizeof(uint16_t) * 8);

    packedRecVal[0] = (packedRecVal[0] & 0x0000) | (((rec->start_time >> 0) & 0xFFFF) << 0);
    packedRecVal[1] = (packedRecVal[1] & 0x0000) | (((rec->start_time >> 16) & 0xFFFF) << 0);

    packedRecVal[2] = (packedRecVal[2] & 0x0000) | (((rec->stop_time >> 0) & 0xFFFF) << 0);
    packedRecVal[3] = (packedRecVal[3] & 0x0000) | (((rec->stop_time >> 16) & 0xFFFF) << 0);

    packedRecVal[4] = (packedRecVal[4] & 0x0000) | (((rec->next_pn >> 0) & 0xFFFF) << 0);
    packedRecVal[5] = (packedRecVal[5] & 0x0000) | (((rec->next_pn >> 16) & 0xFFFF) << 0);

    packedRecVal[6] = (packedRecVal[6] & 0xFFFE) | (((rec->sat_pn >> 0) & 0x1) << 0);

    packedRecVal[6] = (packedRecVal[6] & 0xFFFD) | (((rec->fresh >> 0) & 0x1) << 1);

    packedRecVal[7] = (packedRecVal[7] & 0x7FFF) | (((rec->valid >> 0) & 0x1) << 15);

    return SetRawSECEgressRecordVal(hw, packedRecVal, 8, 2, ROWOFFSET_EGRESSSARECORD + tableIndex);
}

int AQ_API_GetEgressSARecord(struct atl_hw *hw, AQ_API_SEC_EgressSARecord* rec, uint16_t tableIndex)
{
    uint16_t packedRecVal[8];
    int ret;

    if (tableIndex >= NUMROWS_EGRESSSARECORD)
        return -EINVAL;

    memset(rec, 0, sizeof(AQ_API_SEC_EgressSARecord));

    ret = GetRawSECEgressRecordVal(hw, packedRecVal, 8, 2, ROWOFFSET_EGRESSSARECORD + tableIndex);
    if (unlikely(ret))
        return ret;

    rec->start_time = (rec->start_time & 0xFFFF0000) | (((packedRecVal[0] >> 0) & 0xFFFF) << 0);
    rec->start_time = (rec->start_time & 0x0000FFFF) | (((packedRecVal[1] >> 0) & 0xFFFF) << 16);

    rec->stop_time = (rec->stop_time & 0xFFFF0000) | (((packedRecVal[2] >> 0) & 0xFFFF) << 0);
    rec->stop_time = (rec->stop_time & 0x0000FFFF) | (((packedRecVal[3] >> 0) & 0xFFFF) << 16);

    rec->next_pn = (rec->next_pn & 0xFFFF0000) | (((packedRecVal[4] >> 0) & 0xFFFF) << 0);
    rec->next_pn = (rec->next_pn & 0x0000FFFF) | (((packedRecVal[5] >> 0) & 0xFFFF) << 16);

    rec->sat_pn = (rec->sat_pn & 0xFFFFFFFE) | (((packedRecVal[6] >> 0) & 0x1) << 0);

    rec->fresh = (rec->fresh & 0xFFFFFFFE) | (((packedRecVal[6] >> 1) & 0x1) << 0);

    rec->valid = (rec->valid & 0xFFFFFFFE) | (((packedRecVal[7] >> 15) & 0x1) << 0);

    return 0;
}

int AQ_API_SetEgressSAKeyRecord(struct atl_hw *hw, const AQ_API_SEC_EgressSAKeyRecord* rec, uint16_t tableIndex)
{
    uint16_t packedRecVal[16];
    int ret;

    if (tableIndex >= NUMROWS_EGRESSSAKEYRECORD)
        return -EINVAL;

    memset(packedRecVal, 0, sizeof(uint16_t) * 16);

    packedRecVal[0] = (packedRecVal[0] & 0x0000) | (((rec->key[0] >> 0) & 0xFFFF) << 0);
    packedRecVal[1] = (packedRecVal[1] & 0x0000) | (((rec->key[0] >> 16) & 0xFFFF) << 0);

    packedRecVal[2] = (packedRecVal[2] & 0x0000) | (((rec->key[1] >> 0) & 0xFFFF) << 0);
    packedRecVal[3] = (packedRecVal[3] & 0x0000) | (((rec->key[1] >> 16) & 0xFFFF) << 0);

    packedRecVal[4] = (packedRecVal[4] & 0x0000) | (((rec->key[2] >> 0) & 0xFFFF) << 0);
    packedRecVal[5] = (packedRecVal[5] & 0x0000) | (((rec->key[2] >> 16) & 0xFFFF) << 0);

    packedRecVal[6] = (packedRecVal[6] & 0x0000) | (((rec->key[3] >> 0) & 0xFFFF) << 0);
    packedRecVal[7] = (packedRecVal[7] & 0x0000) | (((rec->key[3] >> 16) & 0xFFFF) << 0);

    packedRecVal[8] = (packedRecVal[8] & 0x0000) | (((rec->key[4] >> 0) & 0xFFFF) << 0);
    packedRecVal[9] = (packedRecVal[9] & 0x0000) | (((rec->key[4] >> 16) & 0xFFFF) << 0);

    packedRecVal[10] = (packedRecVal[10] & 0x0000) | (((rec->key[5] >> 0) & 0xFFFF) << 0);
    packedRecVal[11] = (packedRecVal[11] & 0x0000) | (((rec->key[5] >> 16) & 0xFFFF) << 0);

    packedRecVal[12] = (packedRecVal[12] & 0x0000) | (((rec->key[6] >> 0) & 0xFFFF) << 0);
    packedRecVal[13] = (packedRecVal[13] & 0x0000) | (((rec->key[6] >> 16) & 0xFFFF) << 0);

    packedRecVal[14] = (packedRecVal[14] & 0x0000) | (((rec->key[7] >> 0) & 0xFFFF) << 0);
    packedRecVal[15] = (packedRecVal[15] & 0x0000) | (((rec->key[7] >> 16) & 0xFFFF) << 0);

    ret = SetRawSECEgressRecordVal(hw, packedRecVal, 8, 2, ROWOFFSET_EGRESSSAKEYRECORD + tableIndex);
    if (unlikely(ret))
        return ret;
    ret = SetRawSECEgressRecordVal(hw, packedRecVal + 8, 8, 2, ROWOFFSET_EGRESSSAKEYRECORD + tableIndex - 32);
    if (unlikely(ret))
        return ret;

    return 0;
}

int AQ_API_GetEgressSAKeyRecord(struct atl_hw *hw, AQ_API_SEC_EgressSAKeyRecord* rec, uint16_t tableIndex)
{
    uint16_t packedRecVal[16];
    int ret;

    if (tableIndex >= NUMROWS_EGRESSSAKEYRECORD)
        return -EINVAL;

    memset(rec, 0, sizeof(AQ_API_SEC_EgressSAKeyRecord));

    ret = GetRawSECEgressRecordVal(hw, packedRecVal, 8, 2, ROWOFFSET_EGRESSSAKEYRECORD + tableIndex);
    if (unlikely(ret))
        return ret;
    ret = GetRawSECEgressRecordVal(hw, packedRecVal + 8, 8, 2, ROWOFFSET_EGRESSSAKEYRECORD + tableIndex - 32);
    if (unlikely(ret))
        return ret;

    rec->key[0] = (rec->key[0] & 0xFFFF0000) | (((packedRecVal[0] >> 0) & 0xFFFF) << 0);
    rec->key[0] = (rec->key[0] & 0x0000FFFF) | (((packedRecVal[1] >> 0) & 0xFFFF) << 16);

    rec->key[1] = (rec->key[1] & 0xFFFF0000) | (((packedRecVal[2] >> 0) & 0xFFFF) << 0);
    rec->key[1] = (rec->key[1] & 0x0000FFFF) | (((packedRecVal[3] >> 0) & 0xFFFF) << 16);

    rec->key[2] = (rec->key[2] & 0xFFFF0000) | (((packedRecVal[4] >> 0) & 0xFFFF) << 0);
    rec->key[2] = (rec->key[2] & 0x0000FFFF) | (((packedRecVal[5] >> 0) & 0xFFFF) << 16);

    rec->key[3] = (rec->key[3] & 0xFFFF0000) | (((packedRecVal[6] >> 0) & 0xFFFF) << 0);
    rec->key[3] = (rec->key[3] & 0x0000FFFF) | (((packedRecVal[7] >> 0) & 0xFFFF) << 16);

    rec->key[4] = (rec->key[4] & 0xFFFF0000) | (((packedRecVal[8] >> 0) & 0xFFFF) << 0);
    rec->key[4] = (rec->key[4] & 0x0000FFFF) | (((packedRecVal[9] >> 0) & 0xFFFF) << 16);

    rec->key[5] = (rec->key[5] & 0xFFFF0000) | (((packedRecVal[10] >> 0) & 0xFFFF) << 0);
    rec->key[5] = (rec->key[5] & 0x0000FFFF) | (((packedRecVal[11] >> 0) & 0xFFFF) << 16);

    rec->key[6] = (rec->key[6] & 0xFFFF0000) | (((packedRecVal[12] >> 0) & 0xFFFF) << 0);
    rec->key[6] = (rec->key[6] & 0x0000FFFF) | (((packedRecVal[13] >> 0) & 0xFFFF) << 16);

    rec->key[7] = (rec->key[7] & 0xFFFF0000) | (((packedRecVal[14] >> 0) & 0xFFFF) << 0);
    rec->key[7] = (rec->key[7] & 0x0000FFFF) | (((packedRecVal[15] >> 0) & 0xFFFF) << 16);

    return 0;
}

int AQ_API_GetEgressSCCounters(struct atl_hw *hw, AQ_API_SEC_EgressSCCounters* counters, uint16_t SCIndex)
{
    uint16_t packedRecVal[4];
    int ret;

    if (SCIndex >= NUMROWS_EGRESSSCRECORD)
        return -EINVAL;

    memset(counters, 0, sizeof(AQ_API_SEC_EgressSCCounters));

    ret = GetRawSECEgressRecordVal(hw, packedRecVal, 4, 3, SCIndex * 8 + 4);
    if (unlikely(ret))
        return ret;
    counters->sc_protected_pkts[0] = packedRecVal[0] | (packedRecVal[1] << 16);
    counters->sc_protected_pkts[1] = packedRecVal[2] | (packedRecVal[3] << 16);

    ret = GetRawSECEgressRecordVal(hw, packedRecVal, 4, 3, SCIndex * 8 + 5);
    if (unlikely(ret))
        return ret;
    counters->sc_encrypted_pkts[0] = packedRecVal[0] | (packedRecVal[1] << 16);
    counters->sc_encrypted_pkts[1] = packedRecVal[2] | (packedRecVal[3] << 16);

    ret = GetRawSECEgressRecordVal(hw, packedRecVal, 4, 3, SCIndex * 8 + 6);
    if (unlikely(ret))
        return ret;
    counters->sc_protected_octets[0] = packedRecVal[0] | (packedRecVal[1] << 16);
    counters->sc_protected_octets[1] = packedRecVal[2] | (packedRecVal[3] << 16);

    ret = GetRawSECEgressRecordVal(hw, packedRecVal, 4, 3, SCIndex * 8 + 7);
    if (unlikely(ret))
        return ret;
    counters->sc_encrypted_octets[0] = packedRecVal[0] | (packedRecVal[1] << 16);
    counters->sc_encrypted_octets[1] = packedRecVal[2] | (packedRecVal[3] << 16);

    return 0;
}

int AQ_API_GetEgressSACounters(struct atl_hw *hw, AQ_API_SEC_EgressSACounters* counters, uint16_t SAIndex)
{
    uint16_t packedRecVal[4];
    int ret;

    if (SAIndex >= NUMROWS_EGRESSSARECORD)
        return -EINVAL;

    memset(counters, 0, sizeof(AQ_API_SEC_EgressSACounters));

    ret = GetRawSECEgressRecordVal(hw, packedRecVal, 4, 3, SAIndex * 8 + 0);
    if (unlikely(ret))
        return ret;
    counters->sa_hit_drop_redirect[0] = packedRecVal[0] | (packedRecVal[1] << 16);
    counters->sa_hit_drop_redirect[1] = packedRecVal[2] | (packedRecVal[3] << 16);

    ret = GetRawSECEgressRecordVal(hw, packedRecVal, 4, 3, SAIndex * 8 + 1);
    if (unlikely(ret))
        return ret;
    counters->sa_protected2_pkts[0] = packedRecVal[0] | (packedRecVal[1] << 16);
    counters->sa_protected2_pkts[1] = packedRecVal[2] | (packedRecVal[3] << 16);

    ret = GetRawSECEgressRecordVal(hw, packedRecVal, 4, 3, SAIndex * 8 + 2);
    if (unlikely(ret))
        return ret;
    counters->sa_protected_pkts[0] = packedRecVal[0] | (packedRecVal[1] << 16);
    counters->sa_protected_pkts[1] = packedRecVal[2] | (packedRecVal[3] << 16);

    ret = GetRawSECEgressRecordVal(hw, packedRecVal, 4, 3, SAIndex * 8 + 3);
    if (unlikely(ret))
        return ret;
    counters->sa_encrypted_pkts[0] = packedRecVal[0] | (packedRecVal[1] << 16);
    counters->sa_encrypted_pkts[1] = packedRecVal[2] | (packedRecVal[3] << 16);

    return 0;
}

int AQ_API_GetEgressCommonCounters(struct atl_hw *hw, AQ_API_SEC_EgressCommonCounters* counters)
{
    uint16_t packedRecVal[4];
    int ret;

    memset(counters, 0, sizeof(AQ_API_SEC_EgressCommonCounters));

    ret = GetRawSECEgressRecordVal(hw, packedRecVal, 4, 3, 256 + 0);
    if (unlikely(ret))
        return ret;
    counters->ctl_pkt[0] = packedRecVal[0] | (packedRecVal[1] << 16);
    counters->ctl_pkt[1] = packedRecVal[2] | (packedRecVal[3] << 16);

    ret = GetRawSECEgressRecordVal(hw, packedRecVal, 4, 3, 256 + 1);
    if (unlikely(ret))
        return ret;
    counters->unknown_sa_pkts[0] = packedRecVal[0] | (packedRecVal[1] << 16);
    counters->unknown_sa_pkts[1] = packedRecVal[2] | (packedRecVal[3] << 16);

    ret = GetRawSECEgressRecordVal(hw, packedRecVal, 4, 3, 256 + 2);
    if (unlikely(ret))
        return ret;
    counters->untagged_pkts[0] = packedRecVal[0] | (packedRecVal[1] << 16);
    counters->untagged_pkts[1] = packedRecVal[2] | (packedRecVal[3] << 16);

    ret = GetRawSECEgressRecordVal(hw, packedRecVal, 4, 3, 256 + 3);
    if (unlikely(ret))
        return ret;
    counters->too_long[0] = packedRecVal[0] | (packedRecVal[1] << 16);
    counters->too_long[1] = packedRecVal[2] | (packedRecVal[3] << 16);

    ret = GetRawSECEgressRecordVal(hw, packedRecVal, 4, 3, 256 + 4);
    if (unlikely(ret))
        return ret;
    counters->ecc_error_pkts[0] = packedRecVal[0] | (packedRecVal[1] << 16);
    counters->ecc_error_pkts[1] = packedRecVal[2] | (packedRecVal[3] << 16);

    ret = GetRawSECEgressRecordVal(hw, packedRecVal, 4, 3, 256 + 5);
    if (unlikely(ret))
        return ret;
    counters->unctrl_hit_drop_redir[0] = packedRecVal[0] | (packedRecVal[1] << 16);
    counters->unctrl_hit_drop_redir[1] = packedRecVal[2] | (packedRecVal[3] << 16);

    return 0;
}

int AQ_API_ClearEgressCounters(struct atl_hw *hw)
{
    struct mssEgressControlRegister_t controlReg;
    int ret;

    memset(&controlReg, 0, sizeof(struct mssEgressControlRegister_t));

  /* NOTE: MSS registers must always be read/written as adjacent pairs.
   * For instance, to write either or both 1E.80A0 and 1E.80A1, we have to:
   * 1. Write 1E.80A0 first
   * 2. Then write 1E.80A1 
   * These writes need to be performed consecutively, and to ensure this
   * we use the PIF mailbox to delegate the reads/writes to the FW (HHD only; 
   * on EUR, it is not necessary to use the PIF mailbox.). */

  /* Toggle the Egress MIB clear bit 0->1->0 */

    /*Read register (EUR/CAL: 1E.5002) */
    ret = atl_mdio_read(hw, 0, MMD_GLOBAL, mssEgressControlRegister_ADDR, &controlReg.word_0);
    if (unlikely(ret))
        return ret;
    /*Read register (EUR/CAL: 1E.5002 + 1) */
    ret = atl_mdio_read(hw, 0, MMD_GLOBAL, mssEgressControlRegister_ADDR + 4, &controlReg.word_1);
    if (unlikely(ret))
        return ret;

    /*Assign to local representation of bitfield (EUR/CAL: 1E.5002.B) */
    controlReg.bits_0.mssEgressClearCounter = 0;

    /* FIXME */

    //controlReg.bits_1.mssEgressClearCounter = 0;
    /*Write register (EUR/CAL: 1E.5002) */
    ret = atl_mdio_write(hw, 0, MMD_GLOBAL, mssEgressControlRegister_ADDR, controlReg.word_0);
    if (unlikely(ret))
        return ret;
    /*Write register (EUR/CAL: 1E.5002 + 1) */
    ret = atl_mdio_write(hw, 0, MMD_GLOBAL, mssEgressControlRegister_ADDR + 4, controlReg.word_1);
    if (unlikely(ret))
        return ret;

    /*Assign to local representation of bitfield (EUR/CAL: 1E.5002.B) */
    controlReg.bits_0.mssEgressClearCounter = 1;
    //controlReg.bits_1.mssEgressClearCounter = 1;
    /*Write register (EUR/CAL: 1E.5002) */
    ret = atl_mdio_write(hw, 0, MMD_GLOBAL, mssEgressControlRegister_ADDR, controlReg.word_0);
    if (unlikely(ret))
        return ret;
    /*Write register (EUR/CAL: 1E.5002 + 1) */
    ret = atl_mdio_write(hw, 0, MMD_GLOBAL, mssEgressControlRegister_ADDR + 4, controlReg.word_1);
    if (unlikely(ret))
        return ret;

    /*Assign to local representation of bitfield (EUR/CAL: 1E.5002.B) */
    controlReg.bits_0.mssEgressClearCounter = 0;
    //controlReg.bits_1.mssEgressClearCounter = 0;
    /*Write register (EUR/CAL: 1E.5002) */
    ret = atl_mdio_write(hw, 0, MMD_GLOBAL, mssEgressControlRegister_ADDR, controlReg.word_0);
    if (unlikely(ret))
        return ret;
    /*Write register (EUR/CAL: 1E.5002 + 1) */
    ret = atl_mdio_write(hw, 0, MMD_GLOBAL, mssEgressControlRegister_ADDR + 4,  controlReg.word_1);
    if (unlikely(ret))
        return ret;

    return 0;
}

int AQ_API_GetIngressSACounters(struct atl_hw *hw, AQ_API_SEC_IngressSACounters* counters, uint16_t SAIndex)
{
    uint16_t packedRecVal[4];
    int ret;

    if (SAIndex >= NUMROWS_INGRESSSARECORD)
        return -EINVAL;

    memset(counters, 0, sizeof(AQ_API_SEC_IngressSACounters));

    ret = GetRawSECIngressRecordVal(hw, packedRecVal, 4, 6, SAIndex * 12 + 0);
    if (unlikely(ret))
        return ret;
    counters->untagged_hit_pkts[0] = packedRecVal[0] | (packedRecVal[1] << 16);
    counters->untagged_hit_pkts[1] = packedRecVal[2] | (packedRecVal[3] << 16);

    ret = GetRawSECIngressRecordVal(hw, packedRecVal, 4, 6, SAIndex * 12 + 1);
    if (unlikely(ret))
        return ret;
    counters->ctrl_hit_drop_redir_pkts[0] = packedRecVal[0] | (packedRecVal[1] << 16);
    counters->ctrl_hit_drop_redir_pkts[1] = packedRecVal[2] | (packedRecVal[3] << 16);

    ret = GetRawSECIngressRecordVal(hw, packedRecVal, 4, 6, SAIndex * 12 + 2);
    if (unlikely(ret))
        return ret;
    counters->not_using_sa[0] = packedRecVal[0] | (packedRecVal[1] << 16);
    counters->not_using_sa[1] = packedRecVal[2] | (packedRecVal[3] << 16);

    ret = GetRawSECIngressRecordVal(hw, packedRecVal, 4, 6, SAIndex * 12 + 3);
    if (unlikely(ret))
        return ret;
    counters->unused_sa[0] = packedRecVal[0] | (packedRecVal[1] << 16);
    counters->unused_sa[1] = packedRecVal[2] | (packedRecVal[3] << 16);

    ret = GetRawSECIngressRecordVal(hw, packedRecVal, 4, 6, SAIndex * 12 + 4);
    if (unlikely(ret))
        return ret;
    counters->not_valid_pkts[0] = packedRecVal[0] | (packedRecVal[1] << 16);
    counters->not_valid_pkts[1] = packedRecVal[2] | (packedRecVal[3] << 16);

    ret = GetRawSECIngressRecordVal(hw, packedRecVal, 4, 6, SAIndex * 12 + 5);
    if (unlikely(ret))
        return ret;
    counters->invalid_pkts[0] = packedRecVal[0] | (packedRecVal[1] << 16);
    counters->invalid_pkts[1] = packedRecVal[2] | (packedRecVal[3] << 16);

    ret = GetRawSECIngressRecordVal(hw, packedRecVal, 4, 6, SAIndex * 12 + 6);
    if (unlikely(ret))
        return ret;
    counters->ok_pkts[0] = packedRecVal[0] | (packedRecVal[1] << 16);
    counters->ok_pkts[1] = packedRecVal[2] | (packedRecVal[3] << 16);

    ret = GetRawSECIngressRecordVal(hw, packedRecVal, 4, 6, SAIndex * 12 + 7);
    if (unlikely(ret))
        return ret;
    counters->late_pkts[0] = packedRecVal[0] | (packedRecVal[1] << 16);
    counters->late_pkts[1] = packedRecVal[2] | (packedRecVal[3] << 16);

    ret = GetRawSECIngressRecordVal(hw, packedRecVal, 4, 6, SAIndex * 12 + 8);
    if (unlikely(ret))
        return ret;
    counters->delayed_pkts[0] = packedRecVal[0] | (packedRecVal[1] << 16);
    counters->delayed_pkts[1] = packedRecVal[2] | (packedRecVal[3] << 16);

    ret = GetRawSECIngressRecordVal(hw, packedRecVal, 4, 6, SAIndex * 12 + 9);
    if (unlikely(ret))
        return ret;
    counters->unchecked_pkts[0] = packedRecVal[0] | (packedRecVal[1] << 16);
    counters->unchecked_pkts[1] = packedRecVal[2] | (packedRecVal[3] << 16);

    ret = GetRawSECIngressRecordVal(hw, packedRecVal, 4, 6, SAIndex * 12 + 10);
    if (unlikely(ret))
        return ret;
    counters->validated_octets[0] = packedRecVal[0] | (packedRecVal[1] << 16);
    counters->validated_octets[1] = packedRecVal[2] | (packedRecVal[3] << 16);

    ret = GetRawSECIngressRecordVal(hw, packedRecVal, 4, 6, SAIndex * 12 + 11);
    if (unlikely(ret))
        return ret;
    counters->decrypted_octets[0] = packedRecVal[0] | (packedRecVal[1] << 16);
    counters->decrypted_octets[1] = packedRecVal[2] | (packedRecVal[3] << 16);

    return 0;
}

int AQ_API_GetIngressCommonCounters(struct atl_hw *hw, AQ_API_SEC_IngressCommonCounters* counters)
{
    uint16_t packedRecVal[4];
    int ret;

    memset(counters, 0, sizeof(AQ_API_SEC_IngressCommonCounters));

    ret = GetRawSECIngressRecordVal(hw, packedRecVal, 4, 6, 385 + 0);
    if (unlikely(ret))
        return ret;
    counters->ctl_pkts[0] = packedRecVal[0] | (packedRecVal[1] << 16);
    counters->ctl_pkts[1] = packedRecVal[2] | (packedRecVal[3] << 16);

    ret = GetRawSECIngressRecordVal(hw, packedRecVal, 4, 6, 385 + 1);
    if (unlikely(ret))
        return ret;
    counters->tagged_miss_pkts[0] = packedRecVal[0] | (packedRecVal[1] << 16);
    counters->tagged_miss_pkts[1] = packedRecVal[2] | (packedRecVal[3] << 16);

    ret = GetRawSECIngressRecordVal(hw, packedRecVal, 4, 6, 385 + 2);
    if (unlikely(ret))
        return ret;
    counters->untagged_miss_pkts[0] = packedRecVal[0] | (packedRecVal[1] << 16);
    counters->untagged_miss_pkts[1] = packedRecVal[2] | (packedRecVal[3] << 16);

    ret = GetRawSECIngressRecordVal(hw, packedRecVal, 4, 6, 385 + 3);
    if (unlikely(ret))
        return ret;
    counters->notag_pkts[0] = packedRecVal[0] | (packedRecVal[1] << 16);
    counters->notag_pkts[1] = packedRecVal[2] | (packedRecVal[3] << 16);

    ret = GetRawSECIngressRecordVal(hw, packedRecVal, 4, 6, 385 + 4);
    if (unlikely(ret))
        return ret;
    counters->untagged_pkts[0] = packedRecVal[0] | (packedRecVal[1] << 16);
    counters->untagged_pkts[1] = packedRecVal[2] | (packedRecVal[3] << 16);

    ret = GetRawSECIngressRecordVal(hw, packedRecVal, 4, 6, 385 + 5);
    if (unlikely(ret))
        return ret;
    counters->bad_tag_pkts[0] = packedRecVal[0] | (packedRecVal[1] << 16);
    counters->bad_tag_pkts[1] = packedRecVal[2] | (packedRecVal[3] << 16);

    ret = GetRawSECIngressRecordVal(hw, packedRecVal, 4, 6, 385 + 6);
    if (unlikely(ret))
        return ret;
    counters->no_sci_pkts[0] = packedRecVal[0] | (packedRecVal[1] << 16);
    counters->no_sci_pkts[1] = packedRecVal[2] | (packedRecVal[3] << 16);

    ret = GetRawSECIngressRecordVal(hw, packedRecVal, 4, 6, 385 + 7);
    if (unlikely(ret))
        return ret;
    counters->unknown_sci_pkts[0] = packedRecVal[0] | (packedRecVal[1] << 16);
    counters->unknown_sci_pkts[1] = packedRecVal[2] | (packedRecVal[3] << 16);

    ret = GetRawSECIngressRecordVal(hw, packedRecVal, 4, 6, 385 + 8);
    if (unlikely(ret))
        return ret;
    counters->ctrl_prt_pass_pkts[0] = packedRecVal[0] | (packedRecVal[1] << 16);
    counters->ctrl_prt_pass_pkts[1] = packedRecVal[2] | (packedRecVal[3] << 16);

    ret = GetRawSECIngressRecordVal(hw, packedRecVal, 4, 6, 385 + 9);
    if (unlikely(ret))
        return ret;
    counters->unctrl_prt_pass_pkts[0] = packedRecVal[0] | (packedRecVal[1] << 16);
    counters->unctrl_prt_pass_pkts[1] = packedRecVal[2] | (packedRecVal[3] << 16);

    ret = GetRawSECIngressRecordVal(hw, packedRecVal, 4, 6, 385 + 10);
    if (unlikely(ret))
        return ret;
    counters->ctrl_prt_fail_pkts[0] = packedRecVal[0] | (packedRecVal[1] << 16);
    counters->ctrl_prt_fail_pkts[1] = packedRecVal[2] | (packedRecVal[3] << 16);

    ret = GetRawSECIngressRecordVal(hw, packedRecVal, 4, 6, 385 + 11);
    if (unlikely(ret))
        return ret;
    counters->unctrl_prt_fail_pkts[0] = packedRecVal[0] | (packedRecVal[1] << 16);
    counters->unctrl_prt_fail_pkts[1] = packedRecVal[2] | (packedRecVal[3] << 16);

    ret = GetRawSECIngressRecordVal(hw, packedRecVal, 4, 6, 385 + 12);
    if (unlikely(ret))
        return ret;
    counters->too_long_pkts[0] = packedRecVal[0] | (packedRecVal[1] << 16);
    counters->too_long_pkts[1] = packedRecVal[2] | (packedRecVal[3] << 16);

    ret = GetRawSECIngressRecordVal(hw, packedRecVal, 4, 6, 385 + 13);
    if (unlikely(ret))
        return ret;
    counters->igpoc_ctl_pkts[0] = packedRecVal[0] | (packedRecVal[1] << 16);
    counters->igpoc_ctl_pkts[1] = packedRecVal[2] | (packedRecVal[3] << 16);

    ret = GetRawSECIngressRecordVal(hw, packedRecVal, 4, 6, 385 + 14);
    if (unlikely(ret))
        return ret;
    counters->ecc_error_pkts[0] = packedRecVal[0] | (packedRecVal[1] << 16);
    counters->ecc_error_pkts[1] = packedRecVal[2] | (packedRecVal[3] << 16);

    ret = GetRawSECIngressRecordVal(hw, packedRecVal, 4, 6, 385 + 15);
    if (unlikely(ret))
        return ret;
    counters->unctrl_hit_drop_redir[0] = packedRecVal[0] | (packedRecVal[1] << 16);
    counters->unctrl_hit_drop_redir[1] = packedRecVal[2] | (packedRecVal[3] << 16);

    return 0;
}

int AQ_API_ClearIngressCounters(struct atl_hw *hw)
{
    struct mssIngressControlRegister_t controlReg;
    int ret;

    memset(&controlReg, 0, sizeof(struct mssIngressControlRegister_t));

  /* NOTE: MSS registers must always be read/written as adjacent pairs.
   * For instance, to write either or both 1E.80A0 and 1E.80A1, we have to:
   * 1. Write 1E.80A0 first
   * 2. Then write 1E.80A1 
   * These writes need to be performed consecutively, and to ensure this
   * we use the PIF mailbox to delegate the reads/writes to the FW (HHD only; 
   * on EUR, it is not necessary to use the PIF mailbox.). */

    /* Toggle the Ingress MIB clear bit 0->1->0 */
    /*Read register (EUR/CAL: 1E.800E) */
    ret = atl_mdio_read(hw, 0, MMD_GLOBAL, mssIngressControlRegister_ADDR, &controlReg.word_0);
    if (unlikely(ret))
        return ret;
    /*Read register (EUR/CAL: 1E.800E + 1) */
    ret = atl_mdio_read(hw, 0, MMD_GLOBAL, mssIngressControlRegister_ADDR +4, &controlReg.word_1);
    if (unlikely(ret))
        return ret;

    /*Assign to local representation of bitfield (EUR/CAL: 1E.800E.8) */

    /* FIXME */

    controlReg.bits_0.mssIngressClearCount = 0;
      /*Write register (EUR/CAL: 1E.800E) */
    ret = atl_mdio_write(hw, 0, MMD_GLOBAL, mssIngressControlRegister_ADDR, controlReg.word_0);
    if (unlikely(ret))
        return ret;
      /*Write register (EUR/CAL: 1E.800E + 1) */
    ret = atl_mdio_write(hw, 0, MMD_GLOBAL, mssIngressControlRegister_ADDR + 4, controlReg.word_1);
    if (unlikely(ret))
        return ret;

    /*Assign to local representation of bitfield (EUR/CAL: 1E.800E.8) */
    controlReg.bits_0.mssIngressClearCount = 1;
      /*Write register (EUR/CAL: 1E.800E) */
    ret = atl_mdio_write(hw, 0, MMD_GLOBAL, mssIngressControlRegister_ADDR, controlReg.word_0);
    if (unlikely(ret))
        return ret;
      /*Write register (EUR/CAL: 1E.800E + 1) */
    ret = atl_mdio_write(hw, 0, MMD_GLOBAL, mssIngressControlRegister_ADDR + 4, controlReg.word_1);
    if (unlikely(ret))
        return ret;

      /*Assign to local representation of bitfield (EUR/CAL: 1E.800E.8) */
    controlReg.bits_0.mssIngressClearCount = 0;
      /*Write register (EUR/CAL: 1E.800E) */
    ret = atl_mdio_write(hw, 0, MMD_GLOBAL, mssIngressControlRegister_ADDR, controlReg.word_0);
    if (unlikely(ret))
        return ret;
      /*Write register (EUR/CAL: 1E.800E + 1) */
    ret = atl_mdio_write(hw, 0, MMD_GLOBAL, mssIngressControlRegister_ADDR + 4, controlReg.word_1);
    if (unlikely(ret))
        return ret;

    return 0;
}

int AQ_API_GetEgressSAExpired(struct atl_hw *hw, uint32_t *expired)
{
  uint16_t val;
  int ret;

  ret = atl_mdio_read(hw, 0, MMD_GLOBAL, mssEgressSaExpiredStatusRegister_ADDR, &val);
  if (unlikely(ret))
    return ret;

  *expired = val;

  ret = atl_mdio_read(hw, 0, MMD_GLOBAL, mssEgressSaExpiredStatusRegister_ADDR + 1, &val);
  if (unlikely(ret))
    return ret;

  *expired |= val << 16;

  return 0;
}

int AQ_API_GetEgressSAThresholdExpired(struct atl_hw *hw, uint32_t *expired)
{
  uint16_t val;
  int ret;

  ret = atl_mdio_read(hw, 0, MMD_GLOBAL, mssEgressSaThresholdExpiredStatusRegister_ADDR, &val);
  if (unlikely(ret))
    return ret;

  *expired = val;

  ret = atl_mdio_read(hw, 0, MMD_GLOBAL, mssEgressSaThresholdExpiredStatusRegister_ADDR + 1, &val);
  if (unlikely(ret))
    return ret;

  *expired |= val << 16;

  return 0;
}

int AQ_API_SetEgressSAExpired(struct atl_hw *hw, uint32_t expired)
{
  int ret;

  ret = atl_mdio_write(hw, 0, MMD_GLOBAL, mssEgressSaExpiredStatusRegister_ADDR, expired & 0xFFFF);
  if (unlikely(ret))
    return ret;

  ret = atl_mdio_write(hw, 0, MMD_GLOBAL, mssEgressSaExpiredStatusRegister_ADDR + 1, expired >> 16);
  if (unlikely(ret))
    return ret;

  return 0;
}

int AQ_API_SetEgressSAThresholdExpired(struct atl_hw *hw, uint32_t expired)
{
  int ret;

  ret = atl_mdio_write(hw, 0, MMD_GLOBAL, mssEgressSaThresholdExpiredStatusRegister_ADDR, expired & 0xFFFF);
  if (unlikely(ret))
    return ret;

  ret = atl_mdio_write(hw, 0, MMD_GLOBAL, mssEgressSaThresholdExpiredStatusRegister_ADDR + 1, expired >> 16);
  if (unlikely(ret))
    return ret;

  return 0;
}
