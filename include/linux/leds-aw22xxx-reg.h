#ifndef __LEDS_AW22XXX_REG_H__
#define __LEDS_AW22XXX_REG_H__

/******************************************************
 *
 * Register List
 *
 *****************************************************/
#define REG_CHIPID      0x00
#define REG_SRST        0x01
#define REG_GCR         0x02
#define REG_CLKCTR      0x03
#define REG_MCUCTR      0x04
#define REG_TASK0       0x05
#define REG_TASK1       0x06
#define REG_PST         0x07
#define REG_INTCFG      0x08
#define REG_INTEN       0x09
#define REG_INTST       0x0a
#define REG_IMAX        0x0b
#define REG_AUDCTR      0x0c
#define REG_IGAIN       0x0d
#define REG_GAIN        0x0e
#define REG_UVLO        0x0f
#define REG_UVLOTHR     0x10
#define REG_DBGCTR      0x20
#define REG_ADDR1       0x21
#define REG_ADDR2       0x22
#define REG_DATA        0x23
#define REG_BSTCTR      0x24
#define REG_FLSBSTD0    0x25
#define REG_FLSBSTD1    0x26
#define REG_FLSCTR      0x30
#define REG_FLSCFG1     0x31
#define REG_FLSCFG2     0x32
#define REG_FLSCFG3     0x33
#define REG_FLSCFG4     0x34
#define REG_FLSWP1      0x35
#define REG_FLSWP2      0x36
#define REG_FLSRP       0x37
#define REG_DECCFG      0x38
#define REG_FLSWP3      0x39
#define REG_PAGE        0xff

/******************************************************
 *
 * Register Write/Read Access
 *
 *****************************************************/
#define REG_NONE_ACCESS                 0
#define REG_RD_ACCESS                   1 << 0
#define REG_WR_ACCESS                   1 << 1
#define AW22XXX_REG_MAX                 0x100

const unsigned char aw22xxx_reg_access[AW22XXX_REG_MAX] = {
	[REG_CHIPID  ] = REG_RD_ACCESS,
	[REG_SRST    ] = REG_RD_ACCESS|REG_WR_ACCESS,
	[REG_GCR     ] = REG_RD_ACCESS|REG_WR_ACCESS,
	[REG_CLKCTR  ] = REG_RD_ACCESS|REG_WR_ACCESS,
	[REG_MCUCTR  ] = REG_RD_ACCESS|REG_WR_ACCESS,
	[REG_TASK0   ] = REG_RD_ACCESS|REG_WR_ACCESS,
	[REG_TASK1   ] = REG_RD_ACCESS|REG_WR_ACCESS,
	[REG_PST     ] = REG_RD_ACCESS,
	[REG_INTCFG  ] = REG_RD_ACCESS|REG_WR_ACCESS,
	[REG_INTEN   ] = REG_RD_ACCESS|REG_WR_ACCESS,
	[REG_INTST   ] = REG_RD_ACCESS|REG_WR_ACCESS,
	[REG_IMAX    ] = REG_RD_ACCESS|REG_WR_ACCESS,
	[REG_AUDCTR  ] = REG_RD_ACCESS|REG_WR_ACCESS,
	[REG_IGAIN   ] = REG_RD_ACCESS|REG_WR_ACCESS,
	[REG_GAIN    ] = REG_RD_ACCESS,
	[REG_UVLO    ] = REG_RD_ACCESS|REG_WR_ACCESS,
	[REG_UVLOTHR ] = REG_RD_ACCESS|REG_WR_ACCESS,
	[REG_DBGCTR  ] = REG_RD_ACCESS|REG_WR_ACCESS,
	[REG_ADDR1   ] = REG_RD_ACCESS|REG_WR_ACCESS,
	[REG_ADDR2   ] = REG_RD_ACCESS|REG_WR_ACCESS,
	[REG_DATA    ] = REG_RD_ACCESS|REG_WR_ACCESS,
	[REG_BSTCTR  ] = REG_RD_ACCESS|REG_WR_ACCESS,
	[REG_FLSBSTD0] = REG_RD_ACCESS,
	[REG_FLSBSTD1] = REG_RD_ACCESS,
	[REG_FLSCTR  ] = REG_RD_ACCESS|REG_WR_ACCESS,
	[REG_FLSCFG1 ] = REG_RD_ACCESS|REG_WR_ACCESS,
	[REG_FLSCFG2 ] = REG_RD_ACCESS|REG_WR_ACCESS,
	[REG_FLSCFG3 ] = REG_RD_ACCESS|REG_WR_ACCESS,
	[REG_FLSCFG4 ] = REG_RD_ACCESS|REG_WR_ACCESS,
	[REG_FLSWP1  ] = REG_RD_ACCESS|REG_WR_ACCESS,
	[REG_FLSWP2  ] = REG_RD_ACCESS|REG_WR_ACCESS,
	[REG_FLSRP   ] = REG_RD_ACCESS|REG_WR_ACCESS,
	[REG_DECCFG  ] = REG_RD_ACCESS|REG_WR_ACCESS,
	[REG_FLSWP3  ] = REG_RD_ACCESS|REG_WR_ACCESS,
	[REG_PAGE    ] = REG_RD_ACCESS|REG_WR_ACCESS,
};

/******************************************************
 *
 * Register Detail
 *
 *****************************************************/
#define BIT_GCR_ADPDN_MASK              (~(1<<7))
#define BIT_GCR_ADPDN_DISABLE           (1<<7)
#define BIT_GCR_ADPDN_ENABLE            (0<<7)
#define BIT_GCR_ENPDN_MASK              (~(1<<6))
#define BIT_GCR_ENPDN_DISABLE           (1<<6)
#define BIT_GCR_ENPDN_ENABLE            (0<<6)
#define BIT_GCR_OTMD_MASK               (~(1<<5))
#define BIT_GCR_OTMD_DISABLE            (1<<5)
#define BIT_GCR_OTMD_ENABLE             (0<<5)
#define BIT_GCR_OTPD_MASK               (~(1<<4))
#define BIT_GCR_OTPD_DISABLE            (1<<4)
#define BIT_GCR_OTPD_ENABLE             (0<<4)
#define BIT_GCR_UVLOME_MASK             (~(1<<3))
#define BIT_GCR_UVLOME_DISABLE          (1<<3)
#define BIT_GCR_UVLOME_ENABLE           (0<<3)
#define BIT_GCR_UVLOEN_MASK             (~(1<<2))
#define BIT_GCR_UVLOEN_ENABLE           (1<<2)
#define BIT_GCR_UVLOEN_DISABLE          (0<<2)
#define BIT_GCR_OSCDIS_MASK             (~(1<<1))
#define BIT_GCR_OSCDIS_DISABLE          (1<<1)
#define BIT_GCR_OSCDIS_ENABLE           (0<<1)
#define BIT_GCR_CHIPEN_MASK             (~(1<<0))
#define BIT_GCR_CHIPEN_ENABLE           (1<<0)
#define BIT_GCR_CHIPEN_DISABLE          (0<<0)

#define BIT_CLKCTR_LOCS                 (1<<7)
#define BIT_CLKCTR_LOCS_DETECTED        (1<<7)
#define BIT_CLKCTR_LOCS_CLKEXIST        (0<<7)
#define BIT_CLKCTR_LOCPD_MASK           (~(1<<6))
#define BIT_CLKCTR_LOCPD_DISABLE        (1<<6)
#define BIT_CLKCTR_LOCPD_ENABLE         (0<<6)
#define BIT_CLKCTR_CLKSEL_MASK          (~(3<<4))
#define BIT_CLKCTR_CLKSEL_EXTCLK        (3<<4)
#define BIT_CLKCTR_CLKSEL_INTCLK_OP     (1<<4)
#define BIT_CLKCTR_CLKSEL_INTCLK_HZ     (0<<4)
#define BIT_CLKCTR_FREQ_MASK            (~(15<<0))
#define BIT_CLKCTR_FREQ_1MHZ            (6<<0)
#define BIT_CLKCTR_FREQ_2MHZ            (5<<0)
#define BIT_CLKCTR_FREQ_4MHZ            (4<<0)
#define BIT_CLKCTR_FREQ_6MHZ            (3<<0)
#define BIT_CLKCTR_FREQ_8MHZ            (2<<0)
#define BIT_CLKCTR_FREQ_12MHZ           (1<<0)
#define BIT_CLKCTR_FREQ_24MHZ           (0<<0)


#define BIT_MCUCTR_MCU_WAKEUP_MASK      (~(1<<2))
#define BIT_MCUCTR_MCU_WAKEUP_ENABLE    (1<<2)
#define BIT_MCUCTR_MCU_WAKEUP_DISABLE   (0<<2)
#define BIT_MCUCTR_MCU_RESET_MASK       (~(1<<1))
#define BIT_MCUCTR_MCU_RESET_DISABLE    (1<<1)
#define BIT_MCUCTR_MCU_RESET_ENABLE     (0<<1)
#define BIT_MCUCTR_MCU_WORK_MASK        (~(1<<0))
#define BIT_MCUCTR_MCU_WORK_ENABLE      (1<<0)
#define BIT_MCUCTR_MCU_WORK_DISABLE     (0<<0)

#define BIT_INTCFG_INTWTH_MASK          (~(127<<1))
#define BIT_INTCFG_INTMD_MASK           (~(1<<0))
#define BIT_INTCFG_INTMD_NEG_PULSE      (1<<0)
#define BIT_INTCFG_INTMD_LOW_LEVEL      (0<<0)

#define BIT_INTEN_LOC_MASK              (~(1<<7))
#define BIT_INTEN_LOC_ENABLE            (1<<7)
#define BIT_INTEN_LOC_DISABLE           (0<<7)
#define BIT_INTEN_UVLO_MASK             (~(1<<6))
#define BIT_INTEN_UVLO_ENABLE           (1<<6)
#define BIT_INTEN_UVLO_DISABLE          (0<<6)
#define BIT_INTEN_OT_MASK               (~(1<<5))
#define BIT_INTEN_OT_ENABLE             (1<<5)
#define BIT_INTEN_OT_DISABLE            (0<<5)
#define BIT_INTEN_WTD_MASK              (~(1<<4))
#define BIT_INTEN_WTD_ENABLE            (1<<4)
#define BIT_INTEN_WTD_DISABLE           (0<<4)
#define BIT_INTEN_FWVER_MASK            (~(1<<3))
#define BIT_INTEN_FWVER_ENABLE          (1<<3)
#define BIT_INTEN_FWVER_DISABLE         (0<<3)
#define BIT_INTEN_FLASH_MASK            (~(1<<2))
#define BIT_INTEN_FLASH_ENABLE          (1<<2)
#define BIT_INTEN_FLASH_DISABLE         (0<<2)
#define BIT_INTEN_MCUCHK_MASK           (~(1<<1))
#define BIT_INTEN_MCUCHK_ENABLE         (1<<1)
#define BIT_INTEN_MCUCHK_DISABLE        (0<<1)
#define BIT_INTEN_FUNCMPE_MASK          (~(1<<0))
#define BIT_INTEN_FUNCMPE_ENABLE        (1<<0)
#define BIT_INTEN_FUNCMPE_DISABLE       (0<<0)

#define BIT_INTST_LOC                   (1<<7)
#define BIT_INTST_UVLO                  (1<<6)
#define BIT_INTST_OT                    (1<<5)
#define BIT_INTST_WTD                   (1<<4)
#define BIT_INTST_FWVER                 (1<<3)
#define BIT_INTST_FLASH                 (1<<2)
#define BIT_INTST_MCUCHK                (1<<1)
#define BIT_INTST_FUNCMPE               (1<<0)

#define BIT_IMAX_MASK                   (~(15<<0))
#define BIT_IMAX_75mA                   ( 7<<0)
#define BIT_IMAX_60mA                   ( 6<<0)
#define BIT_IMAX_45mA                   ( 5<<0)
#define BIT_IMAX_40mA                   (14<<0)
#define BIT_IMAX_30mA                   ( 4<<0)
#define BIT_IMAX_20mA                   (12<<0)
#define BIT_IMAX_15mA                   ( 3<<0)
#define BIT_IMAX_10mA                   (11<<0)
#define BIT_IMAX_9mA                    ( 2<<0)
#define BIT_IMAX_6mA                    ( 1<<0)
#define BIT_IMAX_4mA                    ( 9<<0)
#define BIT_IMAX_3mA                    ( 0<<0)
#define BIT_IMAX_2mA                    ( 8<<0)

#define BIT_AUDCTR_PRCHG_MASK           (~(1<<3))
#define BIT_AUDCTR_PRCHG_ENABLE         (1<<3)
#define BIT_AUDCTR_PRCHG_DISABLE        (0<<3)
#define BIT_AUDCTR_AGCEN_MASK           (~(1<<1))
#define BIT_AUDCTR_AGCEN_ENABLE         (1<<1)
#define BIT_AUDCTR_AGCEN_DISABLE        (0<<1)
#define BIT_AUDCTR_AUDEN_MASK           (~(1<<0))
#define BIT_AUDCTR_AUDEN_ENABLE         (1<<0)
#define BIT_AUDCTR_AUDEN_DISABLE        (0<<0)

#define BIT_DBGCTR_MODE_MASK            (~(3<<0))
#define BIT_DBGCTR_FLASH_MODE           (3<<0)
#define BIT_DBGCTR_SFR_MODE             (2<<0)
#define BIT_DBGCTR_IRAM_MODE            (1<<0)
#define BIT_DBGCTR_NORMAL_MODE          (0<<0)

#define BIT_BSTCTR_TYPE_MASK            (~(1<<2))
#define BIT_BSTCTR_TYPE_FLASH           (1<<2)
#define BIT_BSTCTR_TYPE_SRAM            (0<<2)
#define BIT_BSTCTR_BSTRUN_MASK          (~(1<<1))
#define BIT_BSTCTR_BSTRUN_RUN           (1<<1)
#define BIT_BSTCTR_BSTRUN_STOP          (0<<1)
#define BIT_BSTCTR_BSTEN_MASK           (~(1<<0))
#define BIT_BSTCTR_BSTEN_ENABLE         (1<<0)
#define BIT_BSTCTR_BSTEN_DISABLE        (0<<0)

#endif
