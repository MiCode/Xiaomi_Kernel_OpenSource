#ifndef __XGBE_USR_OPT__
#define __XGBE_USR_OPT__

#define PF_DRIVER		1
#define NO_ELI			1
//#define BRCM_IPC	    0

#ifdef PF_DRIVER
#define XGBE_SRIOV_PF 1
#else
#define XGBE_SRIOV_PF 0
#endif

#ifdef VF_DRIVER
#define XGBE_SRIOV_VF 1
#else
#define XGBE_SRIOV_VF 0
#endif

/* ELI Enable macro.
 *
 * ELI_ENABLE 1 -> supports ELI mode
 * ELI_ENABLE 0 -> supports Non-ELI mode
 * Non-ELI mode supports only PF Driver
 *
 * While compiling VF driver set this macro to 0
 */

#ifdef VF_DRIVER
#define ELI_ENABLE		0
#define BRCM_BCMUTIL    0
#endif

#ifdef PF_DRIVER

#ifdef NO_ELI
#define ELI_ENABLE	0
#else
#define ELI_ENABLE	1
#endif

#define BRCM_BCMUTIL    1

#endif


/* Broadcom Flexible header support */

#ifdef FLEX_HEADER
#define BRCM_FH	1
#else
#define BRCM_FH	0
#endif

#endif
