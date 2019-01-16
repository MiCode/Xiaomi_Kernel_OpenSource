
#ifndef	_USB_UNIFIED_H_
#define _USB_UNIFIED_H_

#include <linux/types.h>
#include <linux/mu3d/hal/mu3d_hal_comm.h>


#undef EXTERN

#ifdef _USB_UNIFIED_H_
#define EXTERN
#else
#define EXTERN extern
#endif


EXTERN DEV_INT32 TS_AUTO_TEST(DEV_INT32 argc, DEV_INT8** argv);
EXTERN DEV_INT32 TS_AUTO_TEST_STOP(DEV_INT32 argc, DEV_INT8** argv);
EXTERN DEV_INT32 u3init(int argc, char**argv);
EXTERN DEV_INT32 u3r(DEV_INT32 argc, DEV_INT8**argv);
EXTERN DEV_INT32 u3w(DEV_INT32 argc, DEV_INT8**argv);
EXTERN DEV_INT32 U3D_Phy_Cfg_Cmd(DEV_INT32 argc, DEV_INT8 **argv);
EXTERN DEV_INT32 u3d_linkup(DEV_INT32 argc, DEV_INT8 **argv);
EXTERN DEV_INT32 dbg_phy_eyeinit(int argc, char** argv);
EXTERN DEV_INT32 dbg_phy_eyescan(int argc, char** argv);

EXTERN void sram_write(DEV_UINT32 mode, DEV_UINT32 addr, DEV_UINT32 data);
EXTERN DEV_UINT32 sram_read(DEV_UINT32 mode, DEV_UINT32 addr);
EXTERN void sram_dbg(void);

EXTERN DEV_INT32 otg_top(int argc, char** argv);


#undef EXTERN

#endif
