#ifndef __DDP_DPI_H__
#define __DDP_DPI_H__

#ifdef BUILD_UBOOT
#include <asm/arch/mt65xx_typedefs.h>
#else
#include <mach/mt_typedefs.h>
#endif

#include "lcm_drv.h"
#include "ddp_info.h"
#include "cmdq_record.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DPI_CHECK_RET(expr)             \
    do {                                \
        DPI_STATUS ret = (expr);        \
        ASSERT(DPI_STATUS_OK == ret);   \
    } while (0)

// for legacy DPI Driver
typedef enum
{
    LCD_IF_PARALLEL_0 = 0,
    LCD_IF_PARALLEL_1 = 1,
    LCD_IF_PARALLEL_2 = 2,
    LCD_IF_SERIAL_0   = 3,
    LCD_IF_SERIAL_1   = 4,

    LCD_IF_ALL = 0xFF,
} LCD_IF_ID;

typedef struct
{
	unsigned rsv_0   :4;
	unsigned addr    :4;
	unsigned rsv_8   :24;
} LCD_REG_CMD_ADDR, *PLCD_REG_CMD_ADDR;

typedef struct
{
	unsigned rsv_0   :4;
	unsigned addr    :4;
	unsigned rsv_8   :24;
} LCD_REG_DAT_ADDR, *PLCD_REG_DAT_ADDR;


typedef enum
{
    LCD_IF_FMT_COLOR_ORDER_RGB = 0,
    LCD_IF_FMT_COLOR_ORDER_BGR = 1,
} LCD_IF_FMT_COLOR_ORDER;


typedef enum
{
    LCD_IF_FMT_TRANS_SEQ_MSB_FIRST = 0,
    LCD_IF_FMT_TRANS_SEQ_LSB_FIRST = 1,
} LCD_IF_FMT_TRANS_SEQ;


typedef enum
{
    LCD_IF_FMT_PADDING_ON_LSB = 0,
    LCD_IF_FMT_PADDING_ON_MSB = 1,
} LCD_IF_FMT_PADDING;


typedef enum
{
    LCD_IF_FORMAT_RGB332 = 0,
    LCD_IF_FORMAT_RGB444 = 1,
    LCD_IF_FORMAT_RGB565 = 2,
    LCD_IF_FORMAT_RGB666 = 3,
    LCD_IF_FORMAT_RGB888 = 4,
} LCD_IF_FORMAT;

typedef enum
{
    LCD_IF_WIDTH_8_BITS  = 0,
    LCD_IF_WIDTH_9_BITS  = 2,
    LCD_IF_WIDTH_16_BITS = 1,
    LCD_IF_WIDTH_18_BITS = 3,
    LCD_IF_WIDTH_24_BITS = 4,
	LCD_IF_WIDTH_32_BITS = 5,
} LCD_IF_WIDTH;


typedef enum
{	
   DPI_STATUS_OK = 0,

   DPI_STATUS_ERROR,
} DPI_STATUS;

typedef enum
{
    DPI_POLARITY_RISING  = 0,
    DPI_POLARITY_FALLING = 1
} DPI_POLARITY;

typedef enum
{
    DPI_RGB_ORDER_RGB = 0,
    DPI_RGB_ORDER_BGR = 1,
} DPI_RGB_ORDER;

typedef struct
{
    unsigned RGB_ORDER      : 1;
    unsigned BYTE_ORDER     : 1;
    unsigned PADDING        : 1;
    unsigned DATA_FMT       : 3;
    unsigned IF_FMT         : 2;
    unsigned COMMAND        : 5;
    unsigned rsv_13         : 2;
    unsigned ENC            : 1;
    unsigned rsv_16         : 8;
    unsigned SEND_RES_MODE  : 1;
    unsigned IF_24          : 1;
	unsigned rsv_6          : 6;
}LCD_REG_WROI_CON, *PLCD_REG_WROI_CON;

DPI_STATUS DPI_EnableColorBar(void);
int ddp_dpi_stop(DISP_MODULE_ENUM module, void *cmdq_handle);
int ddp_dpi_power_on(DISP_MODULE_ENUM module, void *cmdq_handle);
int ddp_dpi_power_off(DISP_MODULE_ENUM module, void *cmdq_handle);
int ddp_dpi_dump(DISP_MODULE_ENUM module, int level);
unsigned int ddp_dpi_get_cur_addr(bool rdma_mode, int layerid );
int ddp_dpi_start(DISP_MODULE_ENUM module, void *cmdq);
int ddp_dpi_init(DISP_MODULE_ENUM module, void *cmdq);
int ddp_dpi_deinit(DISP_MODULE_ENUM module, void *cmdq_handle);
int ddp_dpi_config(DISP_MODULE_ENUM module, disp_ddp_path_config *config, void *cmdq_handle);
int ddp_dpi_trigger(DISP_MODULE_ENUM module, void *cmdq);

#ifdef __cplusplus
}
#endif

#endif // __DPI_DRV_H__
