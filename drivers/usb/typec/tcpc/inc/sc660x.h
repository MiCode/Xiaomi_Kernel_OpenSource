#ifndef __LINUX_SC660X_H__
#define __LINUX_SC660X_H__

#include "std_tcpci_v10.h"
#include "pd_dbg_info.h"

#define ENABLE_SC660X_DBG   			0

#define SC660X_REG_ANA_CTRL1        0x90
#define SC660X_REG_VCONN_OCP_CTRL   0x93
#define SC660X_REG_ANA_STATUS       0x97
#define SC660X_REG_ANA_INT          0x98
#define SC660X_REG_ANA_MASK         0x99
#define SC660X_REG_ANA_CTRL2        0x9B

#define SC660X_REG_RST_CTRL         0xA0
#define SC660X_REG_DRP_CTRL         0xA2
#define SC660X_REG_DRP_DUTY_CTRL    0xA3

/**
 * SC660X_REG_ANA_CTRL1             (0x90)
 */
#define SC660X_REG_VCONN_DISCHARGE_EN       (1 << 5)
#define SC660X_REG_LPM_EN                   (1 << 3)

/**
 * SC660X_REG_ANA_STATUS        (0x97) 
 */
#define SC660X_REG_VBUS_80      (1 << 1)

/**
 * SC660X_REG_ANA_INT        (0x98) 
 */
#define SC660X_REG_INT_HDRST          (1 << 7)
#define SC660X_REG_INT_RA_DATECH    (1 << 5)
#define SC660X_REG_INT_CC_OVP       (1 << 2)
#define SC660X_REG_INT_VBUS_80      (1 << 1)

/**
 * SC660X_REG_ANA_MASK          (0x99)
 */
#define SC660X_REG_MASK_HDRST          (1 << 7)
#define SC660X_REG_MASK_RA_DATECH    (1 << 5)
#define SC660X_REG_MASK_CC_OVP       (1 << 2)
#define SC660X_REG_MASK_VBUS_80      (1 << 1)

/**
 * SC660X_REG_ANA_CTRL2          (0x9B)
 */
#define SC660X_REG_SHUTDOWN_OFF         (1 << 5)

#if ENABLE_SC660X_DBG
#define SC660X_INFO(format, args...) \
	pd_dbg_info("%s() line-%d: " format,\
	__func__, __LINE__, ##args)
#else
#define SC660X_INFO(foramt, args...)
#endif

#endif /* __LINUX_SC660X_H__ */