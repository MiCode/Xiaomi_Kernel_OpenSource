#ifndef __TRUSTZONE_TA_DDP__
#define __TRUSTZONE_TA_DDP__

#define TZ_TA_DDPU_NAME "DDPU TA"
#define TZ_TA_DDPU_UUID "11d28272-5c14-47a9-9f2b-180dc48ec29f" 

/* Data Structure for Test TA */
/* You should define data structure used both in REE/TEE here
   N/A for Test TA */

/* Command for DDP TA */
#define TZCMD_DDP_OVL_START          0
#define TZCMD_DDP_OVL_STOP           1
#define TZCMD_DDP_OVL_RESET          2
#define TZCMD_DDP_OVL_ROI            3
#define TZCMD_DDP_OVL_LAYER_SWITCH   4
#define TZCMD_DDP_OVL_LAYER_CONFIG   5
#define TZCMD_DDP_OVL_3D_CONFIG      6
#define TZCMD_DDP_OVL_LAYER_TDSHP_EN 7
#define TZCMD_DDP_OVL_TEST           8
#define TZCMD_DDP_OVL_CONFIG_LAYER_ADDR      9
#define TZCMD_DDP_OVL_IS_EN      10

/* Data Structure for Test TA */
/* You should define data structure used both in REE/TEE here
   N/A for Test TA */

/* Command for DDP TA */
/* rotator control */
#define TZCMD_DDPU_ROT_ENABLE         0
#define TZCMD_DDPU_ROT_DISABLE        1
#define TZCMD_DDPU_ROT_RESET          2
#define TZCMD_DDPU_ROT_CONFIG         3
#define TZCMD_DDPU_ROT_CON            4
#define TZCMD_DDPU_ROT_EXTEND_FUNC    5

/* write dma control */
#define TZCMD_DDPU_WDMA_START         30
#define TZCMD_DDPU_WDMA_STOP          31
#define TZCMD_DDPU_WDMA_RESET         32
#define TZCMD_DDPU_WDMA_CONFIG        33
#define TZCMD_DDPU_WDMA_CONFIG_UV     34
#define TZCMD_DDPU_WDMA_WAIT          35
#define TZCMD_DDPU_WDMA_EXTEND_FUNC   36

#define TZCMD_DDPU_INTR_CALLBACK      39
#define TZCMD_DDPU_REGISTER_INTR      40

#define TZCMD_DDPU_SET_DAPC_MODE      50

#endif /* __TRUSTZONE_TA_DDP__ */
