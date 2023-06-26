/*******************************************************************************
 * @file     dpm.c
 * @author   USB PD Firmware Team
 *
 * Copyright 2018 ON Semiconductor. All rights reserved.
 *
 * This software and/or documentation is licensed by ON Semiconductor under
 * limited terms and conditions. The terms and conditions pertaining to the
 * software and/or documentation are available at
 * http://www.onsemi.com/site/pdf/ONSEMI_T&C.pdf
 * ("ON Semiconductor Standard Terms and Conditions of Sale, Section 8 Software").
 *
 * DO NOT USE THIS SOFTWARE AND/OR DOCUMENTATION UNLESS YOU HAVE CAREFULLY
 * READ AND YOU AGREE TO THE LIMITED TERMS AND CONDITIONS. BY USING THIS
 * SOFTWARE AND/OR DOCUMENTATION, YOU AGREE TO THE LIMITED TERMS AND CONDITIONS.
 ******************************************************************************/
#include "vendor_info.h"
#include "dpm.h"
#include "platform.h"
#include "TypeC.h"

typedef enum
{
    dpmSelectSourceCap,
    dpmIdle,
} DpmState_t;

/** Definition of DPM interface
 *
 */
struct devicePolicy_t
{
    struct Port	*ports[NUM_PORTS];     ///< List of port managed
    FSC_U8		num_ports;
    DpmState_t	dpm_state;
};

static DevicePolicy_t devicePolicyMgr;

void DPM_Init(DevicePolicy_t **dpm)
{
    devicePolicyMgr.num_ports = 0;
    devicePolicyMgr.dpm_state = dpmIdle;

    *dpm = &devicePolicyMgr;
}

void DPM_AddPort(DevicePolicy_t *dpm, struct Port *port)
{
    dpm->ports[dpm->num_ports++] = port;
}

sopMainHeader_t* DPM_GetSourceCapHeader(DevicePolicy_t *dpm, struct Port *port)
{
    /* The DPM has access to all ports.  If needed, update this port here based
     * on the status of other ports - e.g. power sharing, etc.
     */
    return &(port->src_cap_header);
}

sopMainHeader_t* DPM_GetSinkCapHeader(DevicePolicy_t *dpm, struct Port *port)
{
    /* The DPM has access to all ports.  If needed, update this port here based
     * on the status of other ports - e.g. power sharing, etc.
     */
    return &(port->snk_cap_header);
}

doDataObject_t* DPM_GetSourceCap(DevicePolicy_t *dpm, struct Port *port)
{
    /* The DPM has access to all ports.  If needed, update this port here based
     * on the status of other ports - e.g. power sharing, etc.
     */
    return port->src_caps;
}

doDataObject_t* DPM_GetSinkCap(DevicePolicy_t *dpm, struct Port *port)
{
    /* The DPM has access to all ports.  If needed, update this port here based
     * on the status of other ports - e.g. power sharing, etc.
     */
    return port->snk_caps;
}

FSC_BOOL DPM_TransitionSource(DevicePolicy_t *dpm, struct Port *port, FSC_U8 index)
{
    FSC_BOOL status = TRUE;
    FSC_U32 voltage = 0;

    if (port->src_caps[index].PDO.SupplyType == pdoTypeFixed)
    {
      /* Convert 10mA units to mA for setting supply current */
      platform_set_pps_current(port->PortID,
                               port->PolicyRxDataObj[0].FVRDO.OpCurrent*10);

      if (port->src_caps[index].FPDOSupply.Voltage == 100)
      {
          platform_set_pps_voltage(port->PortID, 250);
          platform_set_vbus_lvl_enable(port->PortID, VBUS_LVL_5V, TRUE, TRUE);
      }
      else
      {
          /* Convert 50mV units to 20mV for setting supply voltage */
          voltage = (port->src_caps[index].FPDOSupply.Voltage * 5) / 2;
          platform_set_pps_voltage(port->PortID, voltage);
          platform_set_vbus_lvl_enable(port->PortID, VBUS_LVL_HV, TRUE, TRUE);
      }
    }
    else if (port->src_caps[index].PDO.SupplyType == pdoTypeAugmented)
    {
      /* PPS request is already in 20mV units */
      platform_set_pps_voltage(port->PortID,
                               port->PolicyRxDataObj[0].PPSRDO.Voltage);

      /* Convert 50mA units to mA for setting supply current */
      platform_set_pps_current(port->PortID,
                               port->PolicyRxDataObj[0].PPSRDO.OpCurrent * 50);
      platform_set_vbus_lvl_enable(port->PortID, VBUS_LVL_HV, TRUE, TRUE);
    }

    return status;
}

FSC_BOOL DPM_IsSourceCapEnabled(DevicePolicy_t *dpm, struct Port *port, FSC_U8 index)
{
    FSC_BOOL status = FALSE;
    FSC_U32 sourceVoltage = 0;

    if (port->src_caps[index].PDO.SupplyType == pdoTypeFixed)
    {
      sourceVoltage = port->src_caps[index].FPDOSupply.Voltage;

      if (!isVBUSOverVoltage(port,
            VBUS_MV_NEW_MAX(VBUS_PD_TO_MV(sourceVoltage)) + MDAC_MV_LSB) &&
          isVBUSOverVoltage(port,
            VBUS_MV_NEW_MIN(VBUS_PD_TO_MV(sourceVoltage))))
      {
          status = TRUE;
      }
    }
    else if (port->src_caps[index].PDO.SupplyType == pdoTypeAugmented)
    {
      sourceVoltage = port->USBPDContract.PPSRDO.Voltage;

      if (!isVBUSOverVoltage(port,
            VBUS_MV_NEW_MAX(VBUS_PPS_TO_MV(sourceVoltage)) + MDAC_MV_LSB) &&
          isVBUSOverVoltage(port,
            VBUS_MV_NEW_MIN(VBUS_PPS_TO_MV(sourceVoltage))))
      {
          status = TRUE;
      }
    }

    return status;
}

SpecRev DPM_SpecRev(struct Port *port, SopType sop)
{
    if (sop == SOP_TYPE_SOP)
    {
        /* Port Partner */
        return port->PdRevSop;
    }
    else if (sop == SOP_TYPE_SOP1 || sop == SOP_TYPE_SOP2)
    {
        /* Cable marker */
        return port->PdRevCable;
    }
    else
    {
        /* Debug, default, etc. Handle as needed. */
        return USBPDSPECREV2p0;
    }
}

void DPM_SetSpecRev(struct Port *port, SopType sop, SpecRev rev)
{
    if (rev >= USBPDSPECREVMAX)
    {
        /* Compliance test tries invalid revision value */
        rev = USBPDSPECREVMAX - 1;
    }

    if (sop == SOP_TYPE_SOP && port->PdRevSop > rev)
    {
        port->PdRevSop = rev;
    }
    else if (sop == SOP_TYPE_SOP1 || sop == SOP_TYPE_SOP2)
    {
        port->PdRevCable = rev;
    }

    /* Adjust according to compatibility table */
    if (port->PdRevSop == USBPDSPECREV2p0 &&
        port->PdRevCable == USBPDSPECREV3p0)
    {
        port->PdRevCable = USBPDSPECREV2p0;
    }
}

#ifdef FSC_HAVE_VDM
SvdmVersion DPM_SVdmVer(struct Port *port, SopType sop)
{
    return (DPM_SpecRev(port, sop) == USBPDSPECREV2p0) ? V1P0 : V2P0;
}
#endif /* FSC_HAVE_VDM */

FSC_U8 DPM_Retries(struct Port *port, SopType sop)
{
    SpecRev rev = DPM_SpecRev(port, sop);

    return (rev == USBPDSPECREV3p0) ? 2 : 3;
}
