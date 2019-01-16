#include <jpeg_cmdq.h>
#include <cmdq_core.h>
#include <cmdq_reg.h>

#include <mach/mt_clkmgr.h>

#include "jpeg_drv_6589_reg.h"

int32_t cmdqJpegClockOn(uint64_t engineFlag)
{
#if 0 
    if (engineFlag & (1 << CMDQ_ENG_JPEG_DEC)){
      enable_clock(MT65XX_PDN_MM_JPEG_DEC,"JPEG");
    }
    if (engineFlag & (1 << CMDQ_ENG_JPEG_ENC_PASS1)){
      enable_clock(MT65XX_PDN_MM_JPEG_DEC,"JPEG");
    }
    if (engineFlag & (1 << CMDQ_ENG_JPEG_ENC_PASS2)){
      enable_clock(MT65XX_PDN_MM_JPEG_DEC,"JPEG");
    }
#endif    
    return 0;
}


int32_t cmdqJpegDumpInfo(uint64_t engineFlag,
                        int level)
{

    return 0;
}


int32_t cmdqJpegResetEng(uint64_t engineFlag)
{
    if (engineFlag & (1 << CMDQ_ENG_JPEG_DEC)){
       IMG_REG_WRITE(0x0000FFFF, REG_ADDR_JPGDEC_INTERRUPT_STATUS); //ack decoder 
       IMG_REG_WRITE(0x00, REG_ADDR_JPGDEC_RESET); //REG_JPGDEC_RESET = 0x00;
       IMG_REG_WRITE(0x01, REG_ADDR_JPGDEC_RESET); //REG_JPGDEC_RESET = 0x01;
       
       IMG_REG_WRITE(0x00, REG_ADDR_JPGDEC_RESET);  //REG_JPGDEC_RESET = 0x00;    
       IMG_REG_WRITE(0x10, REG_ADDR_JPGDEC_RESET);  //REG_JPGDEC_RESET = 0x10;
    }
   
    if (engineFlag & (1 << CMDQ_ENG_JPEG_ENC)){    
      IMG_REG_WRITE( 0, REG_ADDR_JPEG_ENC_RSTB) ;  
      IMG_REG_WRITE( 1, REG_ADDR_JPEG_ENC_RSTB) ;  
    }
	 
	 if (engineFlag & (1 << CMDQ_ENG_JPEG_REMDC)){
      IMG_REG_WRITE( 0, REG_ADDR_JPEG_ENC_PASS2_RSTB) ;  
      IMG_REG_WRITE( 1, REG_ADDR_JPEG_ENC_PASS2_RSTB) ;
    }



    return 0;
}


int32_t cmdqJpegClockOff(uint64_t engineFlag)
{
   
#if 0    
    if (engineFlag & (1 << CMDQ_ENG_JPEG_DEC)){
      disable_clock(MT65XX_PDN_MM_JPEG_DEC,"JPEG");  
    }
    if (engineFlag & (1 << CMDQ_ENG_JPEG_ENC_PASS1)){
      disable_clock(MT65XX_PDN_MM_JPEG_DEC,"JPEG");  
    }
    if (engineFlag & (1 << CMDQ_ENG_JPEG_ENC_PASS2)){
      disable_clock(MT65XX_PDN_MM_JPEG_DEC,"JPEG");  
    }
#endif
    return 0;
}
