
// LVDSTX(Base Address:+800h/+900h)
// 1.Basic setting
#define LVDS_OUTPUT_CTRL   0x0818
  #define RG_LVDSRX_FIFO_EN  0x80000000  //[31] lvdsrx fifo enable
  #define RG_DPMODE          0x00000008  //[3] Reserved for ASFIFO test
  #define RG_SYNC_TRIG_MODE  0x00000004  //[2] lvds 7-> trig vsync mode enable  
  #define RG_OUT_FIFO_EN     0x00000002  //[1] lvds output fifo enable
  #define RG_LVDS_E          0x00000001  //[0] lvds 7bit-4bit fifo enable
      
#define LVDS_CLK_CTRL      0x0820
  #define RG_TEST_CK_SEL2    0x00000400  //[10] 0->lvds ifclk, 1->scan clock
  #define RG_TEST_CK_SEL1    0x00000200  //[9] 0->lvds ctsclk, 1->scan clock
  #define RG_TEST_CK_SEL0    0x00000100  //[8] 0->lvds pclk, 1->scan clock   
  #define RG_TEST_CK_EN      0x00000004  //[2] lvdstx test pattern clock enable
  #define RG_RX_CK_EN        0x00000002  //[1] lvdsrx clock enable
  #define RG_TX_CK_EN        0x00000001  //[0] lvdstx clock enable

#define LVDS_CLK_RESET     0x082c
  #define RG_CTSCLK_RESET_B  0x00000002  //[1] contraol ctsclk_reset_b
  #define RG_PCLK_RESET_B 	 0x00000001  //[0] control pclk_reset_b

typedef enum
{
	  LCD_DATA_FORMAT_VESA8BIT = 0,
	  LCD_DATA_FORMAT_VESA6BIT = 1,
	  LCD_DATA_FORMAT_DISM8BIT = 2
} LCD_DATA_FMT;


// 2.VESA Standard 8Bit/6Bit encoder
#define LVDS_FMT_CTRL      0x0800
  #define RG_8BIT_FORMAT     0x00000000  //[6:4] Data format select 8-bit mode, 000->8bit mode
  #define RG_6BIT_FORMAT     0x00000010  //[6:4] Data format select 8-bit mode, 001->6bit mode
  #define RG_DE_INV          0x00000004  //[2] Input DE invert
  #define RG_VSYNC_INV       0x00000002  //[1] Input VSYNC invert
  #define RG_HSYNC_INV       0x00000001  //[0] Input HSYNC invert

// 3.R_SEL/G_SEL/B_SEL should be set to the same value  
#define LVDS_R_SEL         0x080c
  #define RG_R_SEL_VESA      0x00000000  //VESA Standard
  #define RG_R_SEL_DISM      0x00492492  //DISM Standard

#define LVDS_G_SEL         0x0810
  #define RG_G_SEL_VESA      0x00000000  //VESA Standard
  #define RG_G_SEL_DISM      0x00492492  //DISM Standard

#define LVDS_B_SEL         0x0814
  #define RG_B_SEL_VESA      0x00000000  //VESA Standard
  #define RG_B_SEL_DISM      0x00492492  //DISM Standard

// 4.Build-in test pattern  
#define LVDS_RG_HV_TOTAL   0x0908
  #define LVDS_PTGEN_V_TOTAL   0x027b0000  //[27:16] V total From register for test ptgen
  #define LVDS_PTGEN_H_TOTAL   0x00000540  //[12:0] H total From register for test ptgen
    
#define LVDS_RG_HV_WIDTH   0x090c
  #define LVDS_PTGEN_V_WIDTH   0x00050000  //[27:16] V width From register for test ptgen
  #define LVDS_PTGEN_H_WIDTH   0x00000080  //[12:0] H width From register for test ptgen

#define LVDS_RG_HV_START   0x0910
  #define LVDS_PTGEN_V_START   0x00150000  //[27:16] V start From register for test ptgen
  #define LVDS_PTGEN_H_START   0x00000118  //[12:0] H start From register for test ptgen
  
#define LVDS_RG_HV_ACTIVE  0x0914
  #define LVDS_PTGEN_V_ACTIVE  0x02580000  //[27:16] V active From register for test ptgen
  #define LVDS_PTGEN_H_ACTIVE  0x00000400  //[12:0] H active From register for test ptgen

#define LVDS_RG_PTGEN_CTRL 0x0918
  #define LVDS_COLOR_BAR_TH    0x04000000  //[27:16] Threshold of de_v_count for test enable
  #define LVDS_PTGEN_TYPE      0x00000200  //[15:8] [1:0] 10:generate configurable color bar
  #define LVDS_TST_PAT_EN      0x00000001  //[0] Test Patgen Enable

#define LVDS_RG_PTGEN_BD   0x091c
  #define LVDS_PTGEN_BD_B      0x00200000  //[23:16] Background B From register for test ptgen
  #define LVDS_PTGEN_BD_G      0x00002000  //[15:8] Background G From register for test ptgen
  #define LVDS_PTGEN_BD_R      0x000000ff  //[7:0] Background R From register for test ptgen

#define LVDS_RG_PTGEN_DATA 0x0920
  #define LVDS_PTGEN_B         0x00ff0000  //[23:16] B From register for test ptgen
  #define LVDS_PTGEN_G         0x00002000  //[15:8] G From register for test ptgen
  #define LVDS_PTGEN_R         0x00000020  //[7:0] R From register for test ptgen

// 5.CRC check for digital function
#define LVDS_CRC_CTRL      0x0904
  #define RG_CRC_CLR           0x00000002  //[1] lvdstx_crc crc check clear control
  #define RG_CRC_START         0x00000001  //[0] lvdstx_crc crc check start control

#define LVDS_TX_CRC_STATUS 0x0934

// 6.LVDS analog test   
#define LVDS_RG_TST_CH     0x0830
  #define RG_LVDS_CH2        0x00a00000  //[23:20] Channel 2 From register for ANA test
  #define RG_LVDS_CH1        0x00002800  //[13:10] Channel 1 From register for ANA test
  #define RG_LVDS_CH0        0x0000000a  //[3:0] Channel 0 From register for ANA test
  
#define LVDS_RG_TST_CLK    0x0834
  #define RG_LVDS_PAT_EN     0x80000000  //[31] Source From register for Analog test enable
  #define RG_LVDS_CLK        0x00002800  //[13:10] Clcok channel From register for ANA test
  #define RG_LVDS_CH3        0x0000000a  //[3:0] Channel 3 From register for ANA test

// 7.Lvdstx_fmt output control
#define LVDS_RG_SRC        0x0804
  #define RG_B               0x00000000  //[31:24] Register source for B  
  #define RG_G               0x00000000  //[23:16] Register source for G
  #define RG_R               0x00000000  //[15:8] Register source for R
  #define RG_B_SEL           0x00000000  //[5:4] 00->B, 01->R, 10->G, 11->rg_b  
  #define RG_G_SEL           0x00000000  //[3:2] 00->G, 01->B, 10->R, 11->rg_g 
  #define RG_R_SEL           0x00000000  //[1:0] 00->R, 01->G, 10->B, 11->rg_r

#define LVDS_RG_CTRL       0x0808
  #define RG_DE              0x00000000  //[8] Register source for DE
  #define RG_VSYNC           0x00000000  //[7] Register source for VSYNC
  #define RG_HSYNC           0x00000000  //[6] Register source for HSYNC
  #define RG_DE_SEL          0x00000000  //[5:4] 00->de, 01->hsync, 10->vsync, 11->rg_de
  #define RG_VSYNC_SEL       0x00000000  //[3:2] 00->vsync, 01->de, 10->hsync, 11->rg_vsync 
  #define RG_HSYNC_SEL       0x00000000  //[1:0] 00->hsync, 01->vsync, 10->de, 11->rg_hsync

// 8.Channel swap and bit invert
#define LVDS_CH_SWAP       0x081c
  #define RG_SWAP_SEL        0x80000000  //[31] lvds_pa*_tmds_[6:0] Swap
  #define RG_TOP_PN          0x00000000  //[28:24] Channel P/N Swap
  #define RG_ML_SWAP         0x00000000  //[20:16] Channel MSB/LSB Swap
  #define RG_CLK_SEL         0x00000000  //[14:12] Clock channel source select
  #define RG_CH3_SEL         0x00000000  //[11:9] Channel 3 Source select
  #define RG_CH2_SEL         0x00000000  //[8:6] Channel 2 Source select
  #define RG_CH1_SEL         0x00000000  //[5:3] Channel 1 Source select
  #define RG_CH0_SEL         0x00000000  //[2:0] Channel 0 Source select


// DGI0 (Base Address:+400h/+500h)
// 1.Basic setting
#define DGI0_DEC_CTRL      0x0400
  #define FIFO_WRITE_EN      0x00008000  //[15] fifo write enable
  #define RESET_COUNTER      0x08000000  //[27] reset the counter for timing generate
  #define CLEAR_COUNTER      0x00000000
	
#define DGI0_FIFO_CTRL     0x0404
  #define SW_RST             0x00080000  //[19] soft reset
  #define FIFO_RESET_ON      0x00020000  //[17] fifo reset or not
  #define RD_START           0x00000040  //[6:0]
  
#define DGI0_DATA_OUT_CTRL 0x0408
  #define DATA_OUT_SWAP      0x04000000  //[26]rise fifo data and fall fifo swap bit
  #define TTL_TIM_SWAP	     0x00003000  //[14:12] ttl out timing swap HSYNC ->DE
  #define TTL_TIM_SWAP2	     0x00005000  //[14:12] ttl out timing swap VSYNC ->DE

#define DGI0_DITHER_CTRL0  0x0410
  #define FRC_EN             0x10000000  //[28] frc dither enable
  #define FCNT_DIF_EN        0x00000800  //[11] fcnt diffuse enable
  #define SYNC_SEL           0x00000200  //[9] 0->internal sync generation by input DE, 1->input sync
  #define OUT_FMT            0x00000010  //[5:4] 00->4bit, 01->6bit, 10->8bit, 11->10bit
  
#define DGI0_DITHER_CTRL1  0x0414  
#define DGI0_DITHER_CTRL2  0x0418

#define DGI0_TG_CTRL00     0x041c
  #define PRGS_OUT           0x00104000  //[20] progressive out
  
#define DGI0_TG_CTRL01     0x0420
  #define RG_VSYNC_FORWARD   0x80000000  //[31] verical delay forward or back
  #define RG_VSYNC_DELAY     0x00020000  //[28:16] vertical delay
  #define RG_HSYNC_DELAY     0x000003a8  //[12:0] horizontal delay
  
#define DGI0_TG_CTRL02     0x0424
  #define VSYNC_TOTAL        0x020d0000  //[27:16] vertical total
  #define HSYNC_TOTAL        0x000003aa  //[12:0] horizontal total
                      
#define DGI0_TG_CTRL03     0x0428
  #define VSYNC_WIDTH        0x00060000  //[24:16] vsync width
  #define HSYNC_WIDTH        0x0000003e  //[11:0] hsync width

#define DGI0_TG_CTRL04     0x042c
  #define H_ACT2_EN          0x00000400  //[10] horizontal active 2 enable
  #define V_ACT2_EN          0x00000200  //[9] vertical active 2 enable
  #define HD_ON              0x00000100  //[8] HD on
  #define VSYNC_POL          0x00000040  //[6] vsync polarity
  #define HSYNC_POL          0x00000020  //[5] hsync polarity
  #define DE_POL             0x00000010  //[4] de polarity
  
#define DGI0_TG_CTRL05     0x0430
  #define X_ACTIVE_START     0x007a0000  //[28:16] horizontal start point
  #define X_ACTIVE_END       0x00000399  //[12:0] horizontal end point
  
#define DGI0_TG_CTRL06     0x0434
  #define Y_ACTIVE_OSTART    0x00240000  //[27:16] odd vertical start point
  #define Y_ACTIVE_OEND      0x00000203  //[11:0] odd vertical end point
  
#define DGI0_TG_CTRL07     0x0438
  #define Y_ACTIVE_ESTART    0x00240000  //[27:16] even vertical start point
  #define Y_ACTIVE_EEND      0x00000203  //[11:0] even vertical end point

#define DGI0_TG_CTRL08     0x043c
  #define X_ACTIVE_START_1   0x007a0000  //[28:16] horizontal start point 1
  #define X_ACTIVE_END_1     0x00000399  //[12:0] horizontal end point 1
  
#define DGI0_TG_CTRL09     0x0440
  #define Y_ACTIVE_OSTART_1  0x00240000  //[27:16] odd vertical start point 1
  #define Y_ACTIVE_OEND_1    0x00000203  //[11:0] odd vertical end point 1
  
#define DGI0_TG_CTRL10     0x0444
  #define Y_ACTIVE_ESTART_1  0x00240000  //[27:16] even vertical start point 1
  #define Y_ACTIVE_EEND_1    0x00000203  //[11:0] even vertical end point 1
  
#define DGI0_ANAIF_CTRL2   0x0448
  #define HDMIPLL_REG_CLK_SEL 0x00000000  //[31] hdmipll reference clock select bit, 0->dgi1_ref_clk, 1->dgi0_ref_clk 
  #define DGI1_DEL_D2_D4_SEL 0x00000000  //[17] dgi1 reference clock devide 2 or devide 4 select, 0->devide 2, 1->devide 4 
  #define DGI1_DEL_D1_SEL    0x00000000  //[16] dgi1 reference clock devide 1 select, 0->devide 1, 1->select the d2_d4 clock 
  #define DGI0_DEL_D2_D4_SEL 0x00000000  //[1] dgi0 reference clock devide 2 or devide 4 select, 0->devide 2, 1->devide 4  
  #define DGI0_DEL_D1_SEL    0x00000000  //[0] dgi0 reference clock devide 1 select, 0->devide 1, 1->select the d2_d4 clock
  
#define DGI0_ANAIF_CTRL0   0x044c
  #define DGI0_CLK_DELAY_SEL1  0x0000000  //[29:24] dgi0 clock delay chain select 1
  #define DGI0_CLK_DELAY_SEL0  0x0000000  //[21:16] dgi0 clock delay chain select 0
  #define DGI0_CK_INV_PRE_CTRL 0x0000000  //[1] dgi0 pad clock invert
  #define DGI0_PAD_CLK_ENABLE  0x0000001  //[0] dgi0 pad clock enable
  #define DGI0_PAD_CLK_DISABLE 0x0000000  //[0] dgi0 pad clock disable
  
#define DGI0_ANAIF_CTRL1   0x0450
  #define DGI1_PAD_CLK_INV_EN  0x0000000  //[31] invter clock
  #define DGI1_CLK_DELAY_SEL1  0x0000000  //[29:24] dgi1 clock delay chain select 1
  #define DGI1_CLK_DELAY_SEL0  0x0000000  //[21:16] dgi1 clock delay chain select 0
  #define DATA_IN_TV_MODE      0x0000000  //[9] data in TV mode
  #define DATA_IN_BIT_INV      0x0000000  //[8] data in bit inv
  #define ANAIF_DGI1_CLK_SEL   0x0000000  //[6] anaif dgi1 clock select from dgi0 clock or dgi1 clock
  #define CLK_SEL_TV_MODE      0x0000000  //[4] clock in TV mode
  #define CLK_MODE_SEL         0x0000000  //[3] clock mode select
  #define NWEB_CLK_EN          0x0000000  //[2] nweb clock enable
  #define DGI1_PAD_CLK_EN      0x0000000  //[1] dgi1 pad clk enable
  #define TV_MODE_CLK_EN       0x0000000  //[0] TV mode clock enable
  
  
#define DGI0_TTL_ANAIF_CTRL 0x0454
  #define TTL_CLK_TEST_MODE  0x00000000  //[31] ttl clock test mode, 0->lvds display clock, 1->dgi0 anaif clock
  #define TTL_CLK_DELAY_SEL1 0x00000000  //[21:16] ttl clock delay chain select 1
  #define TTL_CLK_DELAY_SEL0 0x00000000  //[13:8] ttl clock delay chain select 0
  #define TTL_CLK_INV_ENABLE 0x00000001  //[0] ttl clock invert enable
  
#define DGI0_TTL_ANAIF_CTRL1 0x0458
  #define PAD_TTL_EN_PP        0x00000002  //[1] enable ttl out preplace, set to 1
  #define PAD_TTL_EN_FUN_SEL   0x00000001  //[0] TTL Pinmux, 1->function 2, 0->function 1
       
#define DGI0_CLK_RST_CTRL  0x045c
  #define DGI0_TEST_MODE         0x80000000  //[31] dgi0 clk_out test mode
  #define CLK_OUT_TO_IN_INV      0x00000000  //[5] clk_in_inv use clk_out when pat_gen
  #define CLK_OUT_TO_IN          0x00000000  //[4] clk_in use clk_out when pat_gen
  #define CLK_PAT_GEN_EN         0x00000008  //[3] pat_gen clock enable
  #define DGI0_CLK_OUT_ENABLE    0x00000004  //[2] dgi0 clk_out enable
  #define DGI0_CLK_IN_INV_ENABLE 0x00000002  //[1] dgi0 clk_in_inv enable
  #define DGI0_CLK_IN_ENABLE     0x00000001  //[0] dgi0 clk_in enable
  #define DGI0_CLK_OUT_DISABLE   0x00000000  //[2] dgi0 clk_out disable

#define DGI0_PAT_GEN_CTRL0 0x0500
  #define RG_PTGEN_V_TOTAL   0x027b0000  //[27:16] V total From register for test ptgen    
  #define RG_PTGEN_H_TOTAL   0x00000540  //[12:0] H total From register for test ptgen 
  
#define DGI0_PAT_GEN_CTRL1 0x0504
  #define RG_PTGEN_V_WIDTH   0x00050000  //[27:16] V width From register for test ptgen    
  #define RG_PTGEN_H_WIDTH   0x00000080  //[12:0] H width From register for test ptgen
  
#define DGI0_PAT_GEN_CTRL2 0x0508
  #define RG_PTGEN_V_START   0x00150000  //[27:16] V start From register for test ptgen    
  #define RG_PTGEN_H_START   0x00000118  //[12:0] H start From register for test ptgen
  
#define DGI0_PAT_GEN_CTRL3 0x050c
  #define RG_PTGEN_V_ACTIVE  0x02580000  //[27:16] V active From register for test ptgen    
  #define RG_PTGEN_H_ACTIVE  0x00000400  //[12:0] H active From register for test ptgen
        
#define DGI0_PAT_GEN_CTRL4 0x0510
  #define RG_COLOR_BAR_TH	 0x04000000  //[27:16] Threshold of de_v_count for test enable
  #define RG_PTGEN_TYPE      0x00000200  //[15:8] [1:0]=10:generate configurable color bar
  #define PAT_GEN_RST        0x00000004  //[2] pat_gen reset
  #define PAT_IN             0x00000002  //[1] data in use pattern gen data
  #define RG_TST_PAT_EN      0x00000001  //[0] Test Patgen Enable
  
#define DGI0_PAT_GEN_CTRL5 0x0514
  #define RG_PTGEN_BD_B      0x00ff0000  //[23:16] Background B From register for test ptgen
  #define RG_PTGEN_BD_G      0x00008800  //[15:8] Background G From register for test ptgen
  #define RG_PTGEN_BD_R      0x00000044  //[7:0] Background R From register for test ptgen
  
#define DGI0_PAT_GEN_CTRL6 0x0518
  #define RG_PTGEN_B         0x00ff0000  //[23:16] B From register for test ptgen
  #define RG_PTGEN_G         0x00008800  //[15:8] G From register for test ptgen
  #define RG_PTGEN_R         0x00000044  //[7:0] R From register for test ptgen
  
#define DGI0_CRC_MON_CT    0x051c
  #define C_CRC_CLR          0x00000002
  #define C_CRC_START        0x00000001
  
#define DGI0_CRC_OUT       0x0520
  #define CRC_RDY            0x10000000
  #define CRC_OUT            0x00ffffff
  
#define DGI0_MON           0x0524

//ckgen setting
#define REG_LVDS_DISP_CKCFG  0x1014
#define REG_LVDS_CTSCLKCFG   0x1018
#define REG_LVDS_PWR_RST_B   0x1108
#define REG_LVDS_PWR_CTRL    0x110c

#define RG_LVDSWRAP_CTRL1  0x1254
  #define RG_DCXO_POR_MON_EN 0x00000100  //[8]dcxo_por mon enable
  #define RG_PLL1_DIV        0x00000004  //[3:0] rg_pll1_div
  #define RG_PLL1_DIV2 	     0x00000002  //[3:0] rg_pll1_div
  #define RG_PLL1_DIV3	     0x00000006  //[3:0] rg_pll1_div

#define REG_LVDS_ANACFG0   0x1310
  #define RG_LVDS_APD        0xf8000000  //[31:27]
  #define RG_LVDS_BIASA_PD   0x02000000  //[25]
  #define RG_LVDS_ATERM_EN   0x00800000  //[24:23]00: No source termination; 10: 1k Ohm termination; 01: 100 Ohm termination; 11: 90 Ohm termination;
  #define RG_LVDS_APSRC      0x00400000  //[22:20]LVDS A Group P Slew Rate Control; Strongest: 000; Weakest: 111
  #define RG_LVDS_ANSRC      0x00070000  //[19:17]LVDS A Group N Slew Rate Control; Strongest: 111; Weakest: 000
  #define RG_LVDS_ATVCM      0x0000c000  //[16:14]LVDS A Group Common Mode Voltage Control
  #define RG_LVDS_ATVO       0x00002000  //LVDS A Group Output Swing Control

#define REG_LVDS_ANACFG1   0x1314
  #define RG_LVDS_AE4        0x80000000  //[31]A Group TTL Output Enable 4ma strength control. 
  #define RG_LVDS_AE8        0x40000000  //[30]A Group TTL Output Enable 8ma strength control. 
  #define RG_LVDS_ASR        0x20000000  //[29]A Group TTL Output Slew Rate control
  #define RG_LVDS_ASMTEN     0x10000000  //[28]A Group TTL Input Smitch Trigger Enable
  #define RG_LVDS_AMINI_SEL_CK0 0x00000000  //[1] A Group Mini-LVDS ck channel enable
  
#define REG_LVDS_ANACFG2   0x1318 
  #define RG_VPLL_BC         0x30000000  //[31:27]Integral path cap value
  #define RG_VPLL_BIC        0x02000000  //[26:24]Integral path charge pump current
  #define RG_VPLL_BIR        0x00200000  //[23:20]Proportional path charge pump current 
  #define RG_VPLL_BP         0x00010000  //[19:16]Power down for VOPLL Bias
  #define RG_VPLL_BG_PD      0x00008000  //[15]Power down for Bandgap
  #define RG_VPLL_BR         0x00005000  //[14:12]Proportional path res value
  #define RG_VPLL_BIAS_PD    0x00000800  //[11]Power down for VOPLL Bias

#define REG_LVDS_ANACFG3   0x131c
  #define RG_VPLL_DIV        0x00040000  //[21:17]Divider setting
  #define RG_VPLL_DPIX_CKSEL 0x00001000  //[13:12]VPLL_DPIX_CLK Selection
  #define RG_LVDS_DELAY      0x00000080  //[10:8]LVDS_DPIX_CLK Delay setting 70ps / step
  #define RG_VPLL_MKVCO      0x00000040  //[7]VCO Range From    160 MHz~ 390 MHz
  #define RG_VPLL_POSTDIV_EN 0x00000010  //[4]

#define REG_LVDS_ANACFG4   0x1320
  #define RG_VPLL_RST        0x00800000  //[23]
  #define RG_T2TTLO_EN       0x00000800  //[11]
  #define RG_VPLL_PD         0x00000400  //[10]  
  #define RG_BYPASS          0x00000200  //[9]mini clock by pass phase interpolater
  #define RG_LVDS_BYPASS     0x00000100  //[8]
  
#define REG_PLL_GPANACFG0  0x134c
  #define RG_PLL1_EN         0x80000000  //[31]Power Down 0: Power down 1: Power on
  #define RG_PLL1_FBDIV      0x4c000000  //[30:26] Feedback divide ratio (N+1 Divider)
  #define RG_PLL1_FBDIV2     0x2c000000  // 
  #define RG_PLL1_FBDIV3     0x6c000000  //  
  #define RG_PLL1_PREDIV     0x01000000  //[25:24] Pre-divider ratio
  #define RG_PLL1_RST_DLY    0x00300000  //[21:20]Reset Time Control,Tin=1/Fref 00: 2^5 * Tin
  #define RG_PLL1_LF         0x00000800  //[11]Frequency Band Control
  #define RG_PLL1_MONCKEN    0x00000100  //[8]PLL1 clock monitor enable
  #define RG_PLL1_VODEN      0x00000080  //[7]CHP OverDrive Enable
  #define RG_NFIPLL_EN       0x00000002  //[1]Power Down 0: Power down 1: Power on
  
#define REG_PLL_GPANACFG2              0x1354
#define PLLGP_ANACFG2_PLLGP_BIAS_EN       (1U<<20)
  
