/*
 * Broadcom Event  protocol definitions
 *
 * $Copyright Open Broadcom Corporation$
 *
 * Dependencies: proto/bcmeth.h
 *
 * $Id: dnglevent.h $
 *
 */

/*
 * Broadcom dngl Ethernet Events protocol defines
 *
 */

#ifndef _DNGLEVENT_H_
#define _DNGLEVENT_H_

#ifndef _TYPEDEFS_H_
#include <typedefs.h>
#endif
#include <proto/bcmeth.h>

/* This marks the start of a packed structure section. */
#include <packed_section_start.h>
#define BCM_DNGL_EVENT_MSG_VERSION		1
#define DNGL_E_SOCRAM_IND			0x2
typedef BWL_PRE_PACKED_STRUCT struct
{
	uint16  version; /* Current version is 1 */
	uint16  reserved; /* reserved for any future extension */
	uint16  event_type; /* DNGL_E_SOCRAM_IND */
	uint16  datalen; /* Length of the event payload */
} BWL_POST_PACKED_STRUCT bcm_dngl_event_msg_t;

typedef BWL_PRE_PACKED_STRUCT struct bcm_dngl_event {
	struct ether_header eth;
	bcmeth_hdr_t        bcm_hdr;
	bcm_dngl_event_msg_t      dngl_event;
	/* data portion follows */
} BWL_POST_PACKED_STRUCT bcm_dngl_event_t;


/* SOCRAM_IND type tags */
#define  SOCRAM_IND_ASSRT_TAG		0x1
#define SOCRAM_IND_TAG_HEALTH_CHECK	0x2
typedef BWL_PRE_PACKED_STRUCT struct bcm_dngl_socramind {
	uint16			tag;	/* data tag */
	uint16			length; /* data length */
	uint8			value[1]; /* data value with variable length specified by length */
} BWL_POST_PACKED_STRUCT bcm_dngl_socramind_t;

/* Health check top level module tags */
#define	HEALTH_CHECK_TOP_LEVEL_MODULE_PCIEDEV_RTE 1
typedef BWL_PRE_PACKED_STRUCT struct bcm_dngl_healthcheck {
	uint16			top_module_tag;	/* top level module tag */
	uint16			top_module_len; /* Type of PCIE issue indication */
	uint8			value[1]; /* data value with variable length specified by length */
} BWL_POST_PACKED_STRUCT bcm_dngl_healthcheck_t;

#define HEALTH_CHECK_PCIEDEV_VERSION	1
#define HEALTH_CHECK_PCIEDEV_FLAG_IN_D3_SHIFT	0
#define HEALTH_CHECK_PCIEDEV_FLAG_IN_D3_FLAG	1 << HEALTH_CHECK_PCIEDEV_FLAG_IN_D3_SHIFT
/* PCIE Module TAGs */
#define HEALTH_CHECK_PCIEDEV_INDUCED_IND	0x1
#define HEALTH_CHECK_PCIEDEV_H2D_DMA_IND	0x2
#define HEALTH_CHECK_PCIEDEV_D2H_DMA_IND	0x3
typedef BWL_PRE_PACKED_STRUCT struct bcm_dngl_pcie_hc {
	uint16			version; /* HEALTH_CHECK_PCIEDEV_VERSION */
	uint16			reserved;
	uint16			pcie_err_ind_type; /* PCIE Module TAGs */
	uint16			pcie_flag;
	uint32			pcie_control_reg;
} BWL_POST_PACKED_STRUCT bcm_dngl_pcie_hc_t;

/* This marks the end of a packed structure section. */
#include <packed_section_end.h>

#endif /* _DNGLEVENT_H_ */
