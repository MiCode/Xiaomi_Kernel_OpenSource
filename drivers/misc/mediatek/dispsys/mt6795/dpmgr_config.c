unsigned int module_list_scenario[DDP_SCENARIO_MAX][DDP_ENING_NUM] = {
/*PRIMARY_DISP  */  {DISP_MODULE_OVL0,  DISP_MODULE_COLOR0, DISP_MODULE_AAL,      DISP_MODULE_OD,    DISP_MODULE_RDMA0,    DISP_MODULE_UFOE,     DISP_MODULE_DSI0,      -1,                   -1},
/*PRIMARY_MEMOUT*/  {DISP_MODULE_OVL0,  DISP_MODULE_WDMA0,  -1,                   -1,                -1,                   -1,                   -1,                   -1,                   -1},
/*PRIMARY_ALL   */  {DISP_MODULE_OVL0,  DISP_MODULE_WDMA0,  DISP_MODULE_COLOR0,   DISP_MODULE_AAL,   DISP_MODULE_OD,       DISP_MODULE_RDMA0,    DISP_MODULE_UFOE,     DISP_MODULE_DSI0,      -1},
/*SUB_DISP      */  {DISP_MODULE_OVL1,  DISP_MODULE_COLOR1, DISP_MODULE_GAMMA,    DISP_MODULE_RDMA1, DISP_MODULE_DPI, -1,                   -1,                   -1,                   -1},
/*SUB_MEMOUT    */  {DISP_MODULE_OVL1,  DISP_MODULE_WDMA1,  -1,                   -1,                -1,                   -1,                   -1,                   -1,                   -1},
/*SUB_ALL       */  {DISP_MODULE_OVL1,  DISP_MODULE_WDMA1,  DISP_MODULE_COLOR1,   DISP_MODULE_GAMMA, DISP_MODULE_RDMA1,    DISP_MODULE_DPI, -1,                   -1,                   -1},
/*MHL_DISP      */  {DISP_MODULE_OVL1,  DISP_MODULE_COLOR1, DISP_MODULE_GAMMA,    DISP_MODULE_RDMA1, DISP_MODULE_DPI,      -1,                   -1,                   -1,                   -1},
/*RDMA0_DISP    */  {DISP_MODULE_RDMA0, DISP_MODULE_UFOE,   DISP_MODULE_DSI0, -1,                -1,                   -1,                   -1,                   -1,                   -1},
/*RDMA2_DISP    */  {DISP_MODULE_RDMA2, DISP_MODULE_DSI0,    -1,                   -1,                -1,                   -1,                   -1,                   -1,                   -1},
/*OD_DUMP       */  {DISP_MODULE_OVL0,  DISP_MODULE_COLOR0, DISP_MODULE_AAL,      DISP_MODULE_OD,    DISP_MODULE_WDMA0,    -1,                   -1,                   -1,                   -1},
};
#define	DSI_STATUS_REG_ADDR				0xF401B01C
#define	DSI_STATUS_IDLE_BIT			0x80000000

#define 	DSI_IRQ_BIT_RD_RDY 						(0x1<<0)
#define	DSI_IRQ_BIT_CMD_DONE					(0x1<<1)
#define	DSI_IRQ_BIT_TE_RDY						(0x1<<2)
#define	DSI_IRQ_BIT_VM_DONE						(0x1<<3)
#define	DSI_IRQ_BIT_EXT_TE						(0x1<<4)
#define	DSI_IRQ_BIT_VM_CMD_DONE				(0x1<<5)
#define	DSI_IRQ_BIT_SLEEPOUT_DONE				(0x1<<6)

#define	RDMA0_IRQ_BIT_REG_UPDATE				(0x1<<0)
#define	RDMA0_IRQ_BIT_START						(0x1<<1)
#define	RDMA0_IRQ_BIT_DONE						(0x1<<2)
#define	RDMA0_IRQ_BIT_ABNORMAL					(0x1<<3)
#define	RDMA0_IRQ_BIT_UNDERFLOW				(0x1<<4)
#define	RDMA0_IRQ_BIT_TARGET_LINE				(0x1<<5)

#define	RDMA1_IRQ_BIT_REG_UPDATE				(0x1<<0)
#define	RDMA1_IRQ_BIT_START						(0x1<<1)
#define	RDMA1_IRQ_BIT_DONE						(0x1<<2)
#define	RDMA1_IRQ_BIT_ABNORMAL					(0x1<<3)
#define	RDMA1_IRQ_BIT_UNDERFLOW				(0x1<<4)
#define	RDMA1_IRQ_BIT_TARGET_LINE				(0x1<<5)

#define	RDMA2_IRQ_BIT_REG_UPDATE				(0x1<<0)
#define	RDMA2_IRQ_BIT_START						(0x1<<1)
#define	RDMA2_IRQ_BIT_DONE						(0x1<<2)
#define	RDMA2_IRQ_BIT_ABNORMAL					(0x1<<3)
#define	RDMA2_IRQ_BIT_UNDERFLOW				(0x1<<4)
#define	RDMA2_IRQ_BIT_TARGET_LINE				(0x1<<5)

#define 	OVL0_IRQ_BIT_REG_UPDATE					(0x1<<0)
#define 	OVL0_IRQ_BIT_REG_FRAME_DONE			(0x1<<1)
#define 	OVL0_IRQ_BIT_REG_FRAME_UNDERFLOW		(0x1<<2)

#define 	OVL1_IRQ_BIT_REG_UPDATE					(0x1<<0)
#define 	OVL1_IRQ_BIT_REG_FRAME_DONE			(0x1<<1)
#define 	OVL1_IRQ_BIT_REG_FRAME_UNDERFLOW		(0x1<<2)


//TODO: move IRQ ID define into mt_irq.h
#define MT_DISP_OVL0_IRQ_ID    212
#define MT_DISP_OVL1_IRQ_ID    213
#define MT_DISP_RDMA0_IRQ_ID   214
#define MT_DISP_RDMA1_IRQ_ID   215
#define MT_DISP_RDMA2_IRQ_ID   216
#define MT_DISP_WDMA0_IRQ_ID   217
#define MT_DISP_WDMA1_IRQ_ID   218
#define MT_DISP_COLOR0_IRQ_ID  219
#define MT_DISP_COLOR1_IRQ_ID  220
#define MT_DISP_AAL_IRQ_ID     221
#define MT_DISP_GAMMA_IRQ_ID   222
#define MT_DISP_UFOE_IRQ_ID    223
#define MT_DISP_MUTEX_IRQ_ID   201




