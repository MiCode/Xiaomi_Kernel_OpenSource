#ifndef __LINUX_SC2150_H__
#define __LINUX_SC2150_H__

#include "std_tcpci_v10.h"
#include "pd_dbg_info.h"

#define ENABLE_SC2150_DBG   0

#define SUPPORT_SOUTHCHIP_CPU_BOOST 1

#define SC2150_REG_ANA_CTRL1        0x90
#define SC2150_REG_VCONN_OCP_CTRL   0x93
#define SC2150_REG_ANA_STATUS       0x97
#define SC2150_REG_ANA_INT          0x98
#define SC2150_REG_ANA_MASK         0x99
#define SC2150_REG_ANA_CTRL2        0x9B
#define SC2150_REG_ANA_CTRL3        0x9E

#define SC2150_REG_RST_CTRL         0xA0
#define SC2150_REG_DRP_CTRL         0xA2
#define SC2150_REG_DRP_DUTY_CTRL    0xA3

/**
 * SC2150_REG_ANA_CTRL1             (0x90)
 */
#define SC2150_REG_VCONN_DISCHARGE_EN       (1 << 5)
#define SC2150_REG_LPM_EN                   (1 << 3)

/**
 * SC2150_REG_ANA_STATUS        (0x97) 
 */
#define SC2150_REG_VBUS_80      (1 << 1)

/**
 * SC2150_REG_ANA_INT        (0x98) 
 */
#define SC2150_REG_INT_HDRST          (1 << 7)
#define SC2150_REG_INT_RA_DATECH    (1 << 5)
#define SC2150_REG_INT_CC_OVP       (1 << 2)
#define SC2150_REG_INT_VBUS_80      (1 << 1)

/**
 * SC2150_REG_ANA_MASK          (0x99)
 */
#define SC2150_REG_MASK_HDRST          (1 << 7)
#define SC2150_REG_MASK_RA_DATECH    (1 << 5)
#define SC2150_REG_MASK_CC_OVP       (1 << 2)
#define SC2150_REG_MASK_VBUS_80      (1 << 1)

/**
 * SC2150_REG_ANA_CTRL2          (0x9B)
 */
#define SC2150_REG_SHUTDOWN_OFF         (1 << 5)

/**
 * SC2150_REG_ANA_CTRL3          (0x9E)
 */
#define SC2150_IICRST_300        (1 << 0)
#define SC2150_IICRST_EN        (1 << 7)

#if ENABLE_SC2150_DBG
#define SC2150_INFO(format, args...) \
	pd_dbg_info("%s() line-%d: " format,\
	__func__, __LINE__, ##args)
#else
#define SC2150_INFO(foramt, args...)
#endif

#endif /* __LINUX_SC2150_H__ */