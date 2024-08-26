#ifndef __LINUX_SOUTHCHIP_SUBPMIC_IRQ_H__
#define __LINUX_SOUTHCHIP_SUBPMIC_IRQ_H__

#define SUBPMIC_IRQ_HK           6
#define SUBPMIC_IRQ_CHARGER      0
#define SUBPMIC_IRQ_DPDM         3
#define SUBPMIC_IRQ_LED          2
#define SUBPMIC_IRQ_UFCS         5
#define SUBPMIC_IRQ_DVCHG        1
#define SUBPMIC_IRQ_CID          7
#define SUBPMIC_IRQ_MAX          8

/* NU6601 IRQ numbers */
#define USB_DET_DONE		3
#define VBUS_0V			4
#define VBUS_GD			5
#define CHG_FSM			6
#define CHG_OK			7

#define BOOST_GOOD		20
#define BOOST_FAIL		21

#define VBAT_OV		35

#define LED1_TIMEOUT		56
#define LED2_TIMEOUT		57

#define DM_COT_PLUSE_DONE	88
#define DP_COT_PLUSE_DONE	89
#define DPDM_2PLUSE_DONE	90
#define DPDM_3PLUSE_DONE	91
#define DM_16PLUSE_DONE		92
#define DP_16PLUSE_DONE		93
#define HVDCP_DET_FAIL		94
#define HVDCP_DET_OK		95


#define DCD_TIMEOUT		99
#define RID_CID_DET		100
#define BC12_DET_DONE		103

#endif
