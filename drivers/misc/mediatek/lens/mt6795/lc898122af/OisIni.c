//********************************************************************************
//
//		<< LC898122 Evaluation Soft >>
//		Program Name	: OisIni.c
//		Design			: Y.Yamada
//		History			: LC898122 						2013.01.09 Y.Shigeoka
//********************************************************************************
//**************************
//	Include Header File		
//**************************
#define		OISINI

//#include	"Main.h"
//#include	"Cmd.h"
#include	"Ois.h"
#include	"OisFil.h"
#include	"OisDef.h"

//**************************
//	Local Function Prottype	
//**************************
void	IniClk( void ) ;		// Clock Setting
void	IniIop( void ) ;		// I/O Port Initial Setting
void	IniMon( void ) ;		// Monitor & Other Initial Setting
void	IniSrv( void ) ;		// Servo Register Initial Setting
void	IniGyr( void ) ;		// Gyro Filter Register Initial Setting
void	IniFil( void ) ;		// Gyro Filter Initial Parameter Setting
void	IniAdj( void ) ;		// Adjust Fix Value Setting
void	IniCmd( void ) ;		// Command Execute Process Initial
void	IniDgy( void ) ;		// Digital Gyro Initial Setting
void	IniAf( void ) ;			// Open AF Initial Setting
void	IniPtAve( void ) ;		// Average setting


//********************************************************************************
// Function Name 	: IniSet
// Retun Value		: NON
// Argment Value	: NON
// Explanation		: Initial Setting Function
// History			: First edition 						2009.07.30 Y.Tashita
//********************************************************************************
void	IniSet( void )
{
	// Command Execute Process Initial
	IniCmd() ;
	// Clock Setting
	IniClk() ;
	// I/O Port Initial Setting
	IniIop() ;
	// DigitalGyro Initial Setting
	IniDgy() ;
	// Monitor & Other Initial Setting
	IniMon() ;
	// Servo Initial Setting
	IniSrv() ;
	// Gyro Filter Initial Setting
	IniGyr() ;
	// Gyro Filter Initial Setting
	IniFil() ;
	// Adjust Fix Value Setting
	IniAdj() ;

}

//********************************************************************************
// Function Name 	: IniSetAf
// Retun Value		: NON
// Argment Value	: NON
// Explanation		: Initial AF Setting Function
// History			: First edition 						2013.09.12 Y.Shigeoka
//********************************************************************************
void	IniSetAf( void )
{
	// Command Execute Process Initial
	IniCmd() ;
	// Clock Setting
	IniClk() ;
	// AF Initial Setting
	IniAf() ;

}



//********************************************************************************
// Function Name 	: IniClk
// Retun Value		: NON
// Argment Value	: NON
// Explanation		: Clock Setting
// History			: First edition 						2013.01.08 Y.Shigeoka
//********************************************************************************
void	IniClk( void )
{
	ChkCvr() ;								/* Read Cver */
	
	/*OSC Enables*/
	UcOscAdjFlg	= 0 ;					// Osc adj flag 
	
#ifdef	DEF_SET
	/*OSC ENABLE*/
	RegWriteA( OSCSTOP,		0x00 ) ;			// 0x0256
	RegWriteA( OSCSET,		0x90 ) ;			// 0x0257	OSC ini
	RegWriteA( OSCCNTEN,	0x00 ) ;			// 0x0258	OSC Cnt disable
#endif
	/*Clock Enables*/
	RegWriteA( CLKON,		0x1F ) ;			// 0x020B

  #ifdef	DEF_SET
	RegWriteA( CLKSEL,		0x00 ) ;			// 0x020C	
  #endif
	
 #ifdef	DEF_SET
	RegWriteA( PWMDIV,		0x00 ) ;			// 0x0210	48MHz/1
	RegWriteA( SRVDIV,		0x00 ) ;			// 0x0211	48MHz/1
	RegWriteA( GIFDIV,		0x03 ) ;			// 0x0212	48MHz/3 = 16MHz
	RegWriteA( AFPWMDIV,	0x02 ) ;			// 0x0213	48MHz/2 = 24MHz
	RegWriteA( OPAFDIV,		0x04 ) ;			// 0x0214	48MHz/4 = 12MHz
 #endif
}



//********************************************************************************
// Function Name 	: IniIop
// Retun Value		: NON
// Argment Value	: NON
// Explanation		: I/O Port Initial Setting
// History			: First edition 						2013.01.08 Y.Shigeoka
//********************************************************************************
void	IniIop( void )
{
#ifdef	DEF_SET
	/*set IOP direction*/
	RegWriteA( P0LEV,		0x00 ) ;	// 0x0220	[ - 	| - 	| WLEV5 | WLEV4 ][ WLEV3 | WLEV2 | WLEV1 | WLEV0 ]
	RegWriteA( P0DIR,		0x00 ) ;	// 0x0221	[ - 	| - 	| DIR5	| DIR4	][ DIR3  | DIR2  | DIR1  | DIR0  ]
	/*set pull up/down*/
	RegWriteA( P0PON,		0x0F ) ;	// 0x0222	[ -    | -	  | PON5 | PON4 ][ PON3  | PON2 | PON1 | PON0 ]
	RegWriteA( P0PUD,		0x0F ) ;	// 0x0223	[ -    | -	  | PUD5 | PUD4 ][ PUD3  | PUD2 | PUD1 | PUD0 ]
#endif
	/*select IOP signal*/
#ifdef	USE_3WIRE_DGYRO
	RegWriteA( IOP1SEL,		0x02 ); 	// 0x0231	IOP1 : IOP1
#else
	RegWriteA( IOP1SEL,		0x00 ); 	// 0x0231	IOP1 : DGDATAIN (ATT:0236h[0]=1)
#endif
#ifdef	DEF_SET
	RegWriteA( IOP0SEL,		0x02 ); 	// 0x0230	IOP0 : IOP0
	RegWriteA( IOP2SEL,		0x02 ); 	// 0x0232	IOP2 : IOP2
	RegWriteA( IOP3SEL,		0x00 ); 	// 0x0233	IOP3 : DGDATAOUT
	RegWriteA( IOP4SEL,		0x00 ); 	// 0x0234	IOP4 : DGSCLK
	RegWriteA( IOP5SEL,		0x00 ); 	// 0x0235	IOP5 : DGSSB
	RegWriteA( DGINSEL,		0x00 ); 	// 0x0236	DGDATAIN 0:IOP1 1:IOP2
	RegWriteA( I2CSEL,		0x00 );		// 0x0248	I2C noise reduction ON
	RegWriteA( DLMODE,		0x00 );		// 0x0249	Download OFF
#endif
	
}

//********************************************************************************
// Function Name 	: IniDgy
// Retun Value		: NON
// Argment Value	: NON
// Explanation		: Digital Gyro Initial Setting
// History			: First edition 						2013.01.08 Y.Shigeoka
//********************************************************************************
void	IniDgy( void )
{
 #ifdef USE_INVENSENSE
	unsigned char	UcGrini ;
 #endif
	
	/*************/
	/*For ST gyro*/
	/*************/
	
	/*Set SPI Type*/
 #ifdef USE_3WIRE_DGYRO
	RegWriteA( SPIM 	, 0x00 );							// 0x028F 	[ - | - | - | - ][ - | - | - | DGSPI4 ]
 #else
	RegWriteA( SPIM 	, 0x01 );							// 0x028F 	[ - | - | - | - ][ - | - | - | DGSPI4 ]
 #endif
															//				DGSPI4	0: 3-wire SPI, 1: 4-wire SPI

	/*Set to Command Mode*/
	RegWriteA( GRSEL	, 0x01 );							// 0x0280	[ - | - | - | - ][ - | SRDMOE | OISMODE | COMMODE ]

	/*Digital Gyro Read settings*/
	RegWriteA( GRINI	, 0x80 );							// 0x0281	[ PARA_REG | AXIS7EN | AXIS4EN | - ][ - | SLOWMODE | - | - ]

 #ifdef USE_INVENSENSE

	RegReadA( GRINI	, &UcGrini );					// 0x0281	[ PARA_REG | AXIS7EN | AXIS4EN | - ][ - | SLOWMODE | - | - ]
	RegWriteA( GRINI, ( UcGrini | SLOWMODE) );		// 0x0281	[ PARA_REG | AXIS7EN | AXIS4EN | - ][ - | SLOWMODE | - | - ]
	
	RegWriteA( GRADR0,	0x6A ) ;					// 0x0283	Set I2C_DIS
	RegWriteA( GSETDT,	0x10 ) ;					// 0x028A	Set Write Data
	RegWriteA( GRACC,	0x10 ) ;					/* 0x0282	Set Trigger ON				*/
	AccWit( 0x10 ) ;								/* Digital Gyro busy wait 				*/

	RegWriteA( GRADR0,	0x1B ) ;					// 0x0283	Set GYRO_CONFIG
	RegWriteA( GSETDT,	( FS_SEL << 3) ) ;			// 0x028A	Set Write Data
	RegWriteA( GRACC,	0x10 ) ;					/* 0x0282	Set Trigger ON				*/
	AccWit( 0x10 ) ;								/* Digital Gyro busy wait 				*/

	RegReadA( GRINI	, &UcGrini );					// 0x0281	[ PARA_REG | AXIS7EN | AXIS4EN | - ][ - | SLOWMODE | - | - ]
	RegWriteA( GRINI, ( UcGrini & ~SLOWMODE) );		// 0x0281	[ PARA_REG | AXIS7EN | AXIS4EN | - ][ - | SLOWMODE | - | - ]

 #endif
	
	RegWriteA( RDSEL,	0x7C ) ;				// 0x028B	RDSEL(Data1 and 2 for continuos mode)
	
	GyOutSignal() ;
	

}


//********************************************************************************
// Function Name 	: IniMon
// Retun Value		: NON
// Argment Value	: NON
// Explanation		: Monitor & Other Initial Setting
// History			: First edition 						2013.01.08 Y.Shigeoka
//********************************************************************************
void	IniMon( void )
{
	RegWriteA( PWMMONA, 0x00 ) ;				// 0x0030	0:off
	
	RegWriteA( MONSELA, 0x5C ) ;				// 0x0270	DLYMON1
	RegWriteA( MONSELB, 0x5D ) ;				// 0x0271	DLYMON2
	RegWriteA( MONSELC, 0x00 ) ;				// 0x0272	
	RegWriteA( MONSELD, 0x00 ) ;				// 0x0273	

	// Monitor Circuit
	RegWriteA( WC_PINMON1,	0x00 ) ;			// 0x01C0		Filter Monitor
	RegWriteA( WC_PINMON2,	0x00 ) ;			// 0x01C1		
	RegWriteA( WC_PINMON3,	0x00 ) ;			// 0x01C2		
	RegWriteA( WC_PINMON4,	0x00 ) ;			// 0x01C3		
	/* Delay Monitor */
	RegWriteA( WC_DLYMON11,	0x04 ) ;			// 0x01C5		DlyMonAdd1[10:8]
	RegWriteA( WC_DLYMON10,	0x40 ) ;			// 0x01C4		DlyMonAdd1[ 7:0]
	RegWriteA( WC_DLYMON21,	0x04 ) ;			// 0x01C7		DlyMonAdd2[10:8]
	RegWriteA( WC_DLYMON20,	0xC0 ) ;			// 0x01C6		DlyMonAdd2[ 7:0]
	RegWriteA( WC_DLYMON31,	0x00 ) ;			// 0x01C9		DlyMonAdd3[10:8]
	RegWriteA( WC_DLYMON30,	0x00 ) ;			// 0x01C8		DlyMonAdd3[ 7:0]
	RegWriteA( WC_DLYMON41,	0x00 ) ;			// 0x01CB		DlyMonAdd4[10:8]
	RegWriteA( WC_DLYMON40,	0x00 ) ;			// 0x01CA		DlyMonAdd4[ 7:0]

/* Monitor */
	RegWriteA( PWMMONA, 0x80 ) ;				// 0x0030	1:on 
//	RegWriteA( IOP0SEL,		0x01 ); 			// 0x0230	IOP0 : MONA
/**/


}

//********************************************************************************
// Function Name 	: IniSrv
// Retun Value		: NON
// Argment Value	: NON
// Explanation		: Servo Initial Setting
// History			: First edition 						2013.01.08 Y.Shigeoka
//********************************************************************************
void	IniSrv( void )
{
	unsigned char	UcStbb0 ;

	UcPwmMod = INIT_PWMMODE ;					// Driver output mode

	RegWriteA( WC_EQON,		0x00 ) ;				// 0x0101		Filter Calcu
	RegWriteA( WC_RAMINITON,0x00 ) ;				// 0x0102		
	ClrGyr( 0x0000 , CLR_ALL_RAM );					// All Clear
	
	RegWriteA( WH_EQSWX,	0x02 ) ;				// 0x0170		[ - | - | Sw5 | Sw4 ][ Sw3 | Sw2 | Sw1 | Sw0 ]
	RegWriteA( WH_EQSWY,	0x02 ) ;				// 0x0171		[ - | - | Sw5 | Sw4 ][ Sw3 | Sw2 | Sw1 | Sw0 ]
	
	RamAccFixMod( OFF ) ;							// 32bit Float mode
	
	/* Monitor Gain */
	RamWrite32A( dm1g, 0x3F800000 ) ;			// 0x109A
	RamWrite32A( dm2g, 0x3F800000 ) ;			// 0x109B
	RamWrite32A( dm3g, 0x3F800000 ) ;			// 0x119A
	RamWrite32A( dm4g, 0x3F800000 ) ;			// 0x119B
	
	/* Hall output limitter */
	RamWrite32A( sxlmta1,   0x3F800000 ) ;			// 0x10E6		Hall X output Limit
	RamWrite32A( sylmta1,   0x3F800000 ) ;			// 0x11E6		Hall Y output Limit
	
	/* Emargency Stop */
	RegWriteA( WH_EMGSTPON,	0x00 ) ;				// 0x0178		Emargency Stop OFF
	RegWriteA( WH_EMGSTPTMR,0xFF ) ;				// 0x017A		255*(16/23.4375kHz)=174ms
	
	RamWrite32A( sxemglev,   0x3F800000 ) ;			// 0x10EC		Hall X Emargency threshold
	RamWrite32A( syemglev,   0x3F800000 ) ;			// 0x11EC		Hall Y Emargency threshold
	
	/* Hall Servo smoothing */
	RegWriteA( WH_SMTSRVON,	0x00 ) ;				// 0x017C		Smooth Servo OFF
	RegWriteA( WH_SMTSRVSMP,0x06 ) ;				// 0x017D		2.7ms=2^06/23.4375kHz
	RegWriteA( WH_SMTTMR,	0x01 ) ;				// 0x017E		1.3ms=(1+1)*16/23.4375kHz
	
	RamWrite32A( sxsmtav,   0xBC800000 ) ;			// 0x10ED		1/64 X smoothing ave coefficient
	RamWrite32A( sysmtav,   0xBC800000 ) ;			// 0x11ED		1/64 Y smoothing ave coefficient
	RamWrite32A( sxsmtstp,  0x3AE90466 ) ;			// 0x10EE		0.001778 X smoothing offset
	RamWrite32A( sysmtstp,  0x3AE90466 ) ;			// 0x11EE		0.001778 Y smoothing offset
	
	/* High-dimensional correction  */
	RegWriteA( WH_HOFCON,	0x11 ) ;				// 0x0174		OUT 3x3
	
	/* Front */
	RamWrite32A( sxiexp3,   A3_IEXP3 ) ;			// 0x10BA		
	RamWrite32A( sxiexp2,   0x00000000 ) ;			// 0x10BB		
	RamWrite32A( sxiexp1,   A1_IEXP1 ) ;			// 0x10BC		
	RamWrite32A( sxiexp0,   0x00000000 ) ;			// 0x10BD		
	RamWrite32A( sxiexp,    0x3F800000 ) ;			// 0x10BE		

	RamWrite32A( syiexp3,   A3_IEXP3 ) ;			// 0x11BA		
	RamWrite32A( syiexp2,   0x00000000 ) ;			// 0x11BB		
	RamWrite32A( syiexp1,   A1_IEXP1 ) ;			// 0x11BC		
	RamWrite32A( syiexp0,   0x00000000 ) ;			// 0x11BD		
	RamWrite32A( syiexp,    0x3F800000 ) ;			// 0x11BE		

	/* Back */
	RamWrite32A( sxoexp3,   A3_IEXP3 ) ;			// 0x10FA		
	RamWrite32A( sxoexp2,   0x00000000 ) ;			// 0x10FB		
	RamWrite32A( sxoexp1,   A1_IEXP1 ) ;			// 0x10FC		
	RamWrite32A( sxoexp0,   0x00000000 ) ;			// 0x10FD		
	RamWrite32A( sxoexp,    0x3F800000 ) ;			// 0x10FE		

	RamWrite32A( syoexp3,   A3_IEXP3 ) ;			// 0x11FA		
	RamWrite32A( syoexp2,   0x00000000 ) ;			// 0x11FB		
	RamWrite32A( syoexp1,   A1_IEXP1 ) ;			// 0x11FC		
	RamWrite32A( syoexp0,   0x00000000 ) ;			// 0x11FD		
	RamWrite32A( syoexp,    0x3F800000 ) ;			// 0x11FE		
	
	/* Sine wave */
#ifdef	DEF_SET
	RegWriteA( WC_SINON,	0x00 ) ;				// 0x0180		Sin Wave off
	RegWriteA( WC_SINFRQ0,	0x00 ) ;				// 0x0181		
	RegWriteA( WC_SINFRQ1,	0x60 ) ;				// 0x0182		
	RegWriteA( WC_SINPHSX,	0x00 ) ;				// 0x0183		
	RegWriteA( WC_SINPHSY,	0x00 ) ;				// 0x0184		
	
	/* AD over sampling */
	RegWriteA( WC_ADMODE,	0x06 ) ;				// 0x0188		AD Over Sampling
	
	/* Measure mode */
	RegWriteA( WC_MESMODE,		0x00 ) ;				// 0x0190		Measurement Mode
	RegWriteA( WC_MESSINMODE,	0x00 ) ;				// 0x0191		
	RegWriteA( WC_MESLOOP0,		0x08 ) ;				// 0x0192		
	RegWriteA( WC_MESLOOP1,		0x02 ) ;				// 0x0193		
	RegWriteA( WC_MES1ADD0,		0x00 ) ;				// 0x0194		
	RegWriteA( WC_MES1ADD1,		0x00 ) ;				// 0x0195		
	RegWriteA( WC_MES2ADD0,		0x00 ) ;				// 0x0196		
	RegWriteA( WC_MES2ADD1,		0x00 ) ;				// 0x0197		
	RegWriteA( WC_MESABS,		0x00 ) ;				// 0x0198		
	RegWriteA( WC_MESWAIT,		0x00 ) ;				// 0x0199		
	
	/* auto measure */
	RegWriteA( WC_AMJMODE,		0x00 ) ;				// 0x01A0		Automatic measurement mode
	
	RegWriteA( WC_AMJLOOP0,		0x08 ) ;				// 0x01A2		Self-Aadjustment
	RegWriteA( WC_AMJLOOP1,		0x02 ) ;				// 0x01A3		
	RegWriteA( WC_AMJIDL0,		0x02 ) ;				// 0x01A4		
	RegWriteA( WC_AMJIDL1,		0x00 ) ;				// 0x01A5		
	RegWriteA( WC_AMJ1ADD0,		0x00 ) ;				// 0x01A6		
	RegWriteA( WC_AMJ1ADD1,		0x00 ) ;				// 0x01A7		
	RegWriteA( WC_AMJ2ADD0,		0x00 ) ;				// 0x01A8		
	RegWriteA( WC_AMJ2ADD1,		0x00 ) ;				// 0x01A9		
	
	/* Data Pass */
	RegWriteA( WC_DPI1ADD0,		0x00 ) ;				// 0x01B0		Data Pass
	RegWriteA( WC_DPI1ADD1,		0x00 ) ;				// 0x01B1		
	RegWriteA( WC_DPI2ADD0,		0x00 ) ;				// 0x01B2		
	RegWriteA( WC_DPI2ADD1,		0x00 ) ;				// 0x01B3		
	RegWriteA( WC_DPI3ADD0,		0x00 ) ;				// 0x01B4		
	RegWriteA( WC_DPI3ADD1,		0x00 ) ;				// 0x01B5		
	RegWriteA( WC_DPI4ADD0,		0x00 ) ;				// 0x01B6		
	RegWriteA( WC_DPI4ADD1,		0x00 ) ;				// 0x01B7		
	RegWriteA( WC_DPO1ADD0,		0x00 ) ;				// 0x01B8		Data Pass
	RegWriteA( WC_DPO1ADD1,		0x00 ) ;				// 0x01B9		
	RegWriteA( WC_DPO2ADD0,		0x00 ) ;				// 0x01BA		
	RegWriteA( WC_DPO2ADD1,		0x00 ) ;				// 0x01BB		
	RegWriteA( WC_DPO3ADD0,		0x00 ) ;				// 0x01BC		
	RegWriteA( WC_DPO3ADD1,		0x00 ) ;				// 0x01BD		
	RegWriteA( WC_DPO4ADD0,		0x00 ) ;				// 0x01BE		
	RegWriteA( WC_DPO4ADD1,		0x00 ) ;				// 0x01BF		
	RegWriteA( WC_DPON,			0x00 ) ;				// 0x0105		Data pass OFF
	
	/* Interrupt Flag */
	RegWriteA( WC_INTMSK,	0xFF ) ;				// 0x01CE		All Mask
	
#endif
	
	/* Ram Access */
	RamAccFixMod( OFF ) ;							// 32bit float mode

	// PWM Signal Generate
	DrvSw( OFF ) ;									/* 0x0070	Drvier Block Ena=0 */
	RegWriteA( DRVFC2	, 0x90 );					// 0x0002	Slope 3, Dead Time = 30 ns
	RegWriteA( DRVSELX	, 0xFF );					// 0x0003	PWM X drv max current  DRVSELX[7:0]
	RegWriteA( DRVSELY	, 0xFF );					// 0x0004	PWM Y drv max current  DRVSELY[7:0]

#ifdef	PWM_BREAK
	if( UcCvrCod == CVER122 ) {
		RegWriteA( PWMFC,   0x2D ) ;					// 0x0011	VREF, PWMCLK/256, MODE0B, 12Bit Accuracy
	} else {
		RegWriteA( PWMFC,   0x3D ) ;					// 0x0011	VREF, PWMCLK/128, MODE0B, 12Bit Accuracy
	}
#else
	RegWriteA( PWMFC,   0x21 ) ;					// 0x0011	VREF, PWMCLK/256, MODE1, 12Bit Accuracy
#endif


	RegWriteA( PWMA,    0x00 ) ;					// 0x0010	PWM X/Y standby
	RegWriteA( PWMDLYX,  0x04 ) ;					// 0x0012	X Phase Delay Setting
	RegWriteA( PWMDLYY,  0x04 ) ;					// 0x0013	Y Phase Delay Setting
	
#ifdef	DEF_SET
	RegWriteA( DRVCH1SEL,	0x00 ) ;				// 0x0005	OUT1/OUT2	X axis
	RegWriteA( DRVCH2SEL,	0x00 ) ;				// 0x0006	OUT3/OUT4	Y axis
	
	RegWriteA( PWMDLYTIMX,	0x00 ) ;				// 0x0014		PWM Timing
	RegWriteA( PWMDLYTIMY,	0x00 ) ;				// 0x0015		PWM Timing
#endif
	
	if( UcCvrCod == CVER122 ) {
		RegWriteA( PWMPERIODY,	0x00 ) ;				// 0x001A		PWM Carrier Freq
		RegWriteA( PWMPERIODY2,	0x00 ) ;				// 0x001B		PWM Carrier Freq
	} else {
		RegWriteA( PWMPERIODX,	0x00 ) ;				// 0x0018		PWM Carrier Freq
		RegWriteA( PWMPERIODX2,	0x00 ) ;				// 0x0019		PWM Carrier Freq
		RegWriteA( PWMPERIODY,	0x00 ) ;				// 0x001A		PWM Carrier Freq
		RegWriteA( PWMPERIODY2,	0x00 ) ;				// 0x001B		PWM Carrier Freq
	}
	
	/* Linear PWM circuit setting */
	RegWriteA( CVA		, 0xC0 );			// 0x0020	Linear PWM mode enable

	if( UcCvrCod == CVER122 ) {
		RegWriteA( CVFC 	, 0x22 );			// 0x0021	
	}

	RegWriteA( CVFC2 	, 0x80 );			// 0x0022
	if( UcCvrCod == CVER122 ) {
		RegWriteA( CVSMTHX	, 0x00 );			// 0x0023	smooth off
		RegWriteA( CVSMTHY	, 0x00 );			// 0x0024	smooth off
	}

	RegReadA( STBB0 	, &UcStbb0 );		// 0x0250 	[ STBAFDRV | STBOISDRV | STBOPAAF | STBOPAY ][ STBOPAX | STBDACI | STBDACV | STBADC ]
	UcStbb0 &= 0x80 ;
	RegWriteA( STBB0, UcStbb0 ) ;			// 0x0250	OIS standby
	
}



//********************************************************************************
// Function Name 	: IniGyr
// Retun Value		: NON
// Argment Value	: NON
// Explanation		: Gyro Filter Setting Initialize Function
// History			: First edition 						2013.01.09 Y.Shigeoka
//********************************************************************************
#ifdef GAIN_CONT
  #define	TRI_LEVEL		0x3A031280		/* 0.0005 */
  #define	TIMELOW			0x50			/* */
  #define	TIMEHGH			0x05			/* */
  #define	TIMEBSE			0x5D			/* 3.96ms */
  #define	MONADR			GXXFZ
  #define	GANADR			gxadj
  #define	XMINGAIN		0x00000000
  #define	XMAXGAIN		0x3F800000
  #define	YMINGAIN		0x00000000
  #define	YMAXGAIN		0x3F800000
  #define	XSTEPUP			0x38D1B717		/* 0.0001	 */
  #define	XSTEPDN			0xBD4CCCCD		/* -0.05 	 */
  #define	YSTEPUP			0x38D1B717		/* 0.0001	 */
  #define	YSTEPDN			0xBD4CCCCD		/* -0.05 	 */
#endif


void	IniGyr( void )
{
	
	
	/*Gyro Filter Setting*/
	RegWriteA( WG_EQSW	, 0x03 );		// 0x0110		[ - | Sw6 | Sw5 | Sw4 ][ Sw3 | Sw2 | Sw1 | Sw0 ]
	
	/*Gyro Filter Down Sampling*/
	
	RegWriteA( WG_SHTON	, 0x10 );		// 0x0107		[ - | - | - | CmSht2PanOff ][ - | - | CmShtOpe(1:0) ]
										//				CmShtOpe[1:0] 00: シャッターOFF, 01: シャッターON, 1x:外部制御
										
#ifdef	DEF_SET
	RegWriteA( WG_SHTDLYTMR , 0x00 );	// 0x0117	 	Shutter Delay
	RegWriteA( WG_GADSMP, 	  0x00 );	// 0x011C		Sampling timing
	RegWriteA( WG_HCHR, 	  0x00 );	// 0x011B		H-filter limitter control not USE
	RegWriteA( WG_LMT3MOD , 0x00 );		// 0x0118 	[ - | - | - | - ][ - | - | - | CmLmt3Mod ]
										//				CmLmt3Mod	0: 通常リミッター動作, 1: 円の半径リミッター動作
	RegWriteA( WG_VREFADD , 0x12 );		// 0x0119	 	センター戻しを行う遅延RAMのアドレス下位6ビット　(default 0x12 = GXH1Z2/GYH1Z2)
#endif
	RegWriteA( WG_SHTMOD , 0x06 );		// 0x0116	 	Shutter Hold mode

	// Limiter
	RamWrite32A( gxlmt1H, GYRLMT1H ) ;			// 0x1028
	RamWrite32A( gylmt1H, GYRLMT1H ) ;			// 0x1128

	RamWrite32A( gxlmt3HS0, GYRLMT3_S1 ) ;		// 0x1029
	RamWrite32A( gylmt3HS0, GYRLMT3_S1 ) ;		// 0x1129
	
	RamWrite32A( gxlmt3HS1, GYRLMT3_S2 ) ;		// 0x102A
	RamWrite32A( gylmt3HS1, GYRLMT3_S2 ) ;		// 0x112A

	RamWrite32A( gylmt4HS0, GYRLMT4_S1 ) ;		//0x112B	Y軸Limiter4 High閾値0
	RamWrite32A( gxlmt4HS0, GYRLMT4_S1 ) ;		//0x102B	X軸Limiter4 High閾値0
	
	RamWrite32A( gxlmt4HS1, GYRLMT4_S2 ) ;		//0x102C	X軸Limiter4 High閾値1
	RamWrite32A( gylmt4HS1, GYRLMT4_S2 ) ;		//0x112C	Y軸Limiter4 High閾値1

	
	/* Pan/Tilt parameter */
	RegWriteA( WG_PANADDA, 		0x12 );		// 0x0130	GXH1Z2/GYH1Z2 Select
	RegWriteA( WG_PANADDB, 		0x09 );		// 0x0131	GXIZ/GYIZ Select
	
	 //Threshold
	RamWrite32A( SttxHis, 	0x00000000 );			// 0x1226
	RamWrite32A( SttxaL, 	0x00000000 );			// 0x109D
	RamWrite32A( SttxbL, 	0x00000000 );			// 0x109E
	RamWrite32A( Sttx12aM, 	GYRA12_MID );	// 0x104F
	RamWrite32A( Sttx12aH, 	GYRA12_HGH );	// 0x105F
	RamWrite32A( Sttx12bM, 	GYRB12_MID );	// 0x106F
	RamWrite32A( Sttx12bH, 	GYRB12_HGH );	// 0x107F
	RamWrite32A( Sttx34aM, 	GYRA34_MID );	// 0x108F
	RamWrite32A( Sttx34aH, 	GYRA34_HGH );	// 0x109F
	RamWrite32A( Sttx34bM, 	GYRB34_MID );	// 0x10AF
	RamWrite32A( Sttx34bH, 	GYRB34_HGH );	// 0x10BF
	RamWrite32A( SttyaL, 	0x00000000 );			// 0x119D
	RamWrite32A( SttybL, 	0x00000000 );			// 0x119E
	RamWrite32A( Stty12aM, 	GYRA12_MID );	// 0x114F
	RamWrite32A( Stty12aH, 	GYRA12_HGH );	// 0x115F
	RamWrite32A( Stty12bM, 	GYRB12_MID );	// 0x116F
	RamWrite32A( Stty12bH, 	GYRB12_HGH );	// 0x117F
	RamWrite32A( Stty34aM, 	GYRA34_MID );	// 0x118F
	RamWrite32A( Stty34aH, 	GYRA34_HGH );	// 0x119F
	RamWrite32A( Stty34bM, 	GYRB34_MID );	// 0x11AF
	RamWrite32A( Stty34bH, 	GYRB34_HGH );	// 0x11BF
	
	// Pan level
	RegWriteA( WG_PANLEVABS, 		0x00 );		// 0x0133
	
	// Average parameter are set IniAdj

	// Phase Transition Setting
	// State 2 -> 1
	RegWriteA( WG_PANSTT21JUG0, 	0x00 );		// 0x0140
	RegWriteA( WG_PANSTT21JUG1, 	0x00 );		// 0x0141
	// State 3 -> 1
	RegWriteA( WG_PANSTT31JUG0, 	0x00 );		// 0x0142
	RegWriteA( WG_PANSTT31JUG1, 	0x00 );		// 0x0143
	// State 4 -> 1
	RegWriteA( WG_PANSTT41JUG0, 	0x01 );		// 0x0144
	RegWriteA( WG_PANSTT41JUG1, 	0x00 );		// 0x0145
	// State 1 -> 2
	RegWriteA( WG_PANSTT12JUG0, 	0x00 );		// 0x0146
	RegWriteA( WG_PANSTT12JUG1, 	0x07 );		// 0x0147
	// State 1 -> 3
	RegWriteA( WG_PANSTT13JUG0, 	0x00 );		// 0x0148
	RegWriteA( WG_PANSTT13JUG1, 	0x00 );		// 0x0149
	// State 2 -> 3
	RegWriteA( WG_PANSTT23JUG0, 	0x11 );		// 0x014A
	RegWriteA( WG_PANSTT23JUG1, 	0x00 );		// 0x014B
	// State 4 -> 3
	RegWriteA( WG_PANSTT43JUG0, 	0x00 );		// 0x014C
	RegWriteA( WG_PANSTT43JUG1, 	0x00 );		// 0x014D
	// State 3 -> 4
	RegWriteA( WG_PANSTT34JUG0, 	0x01 );		// 0x014E
	RegWriteA( WG_PANSTT34JUG1, 	0x00 );		// 0x014F
	// State 2 -> 4
	RegWriteA( WG_PANSTT24JUG0, 	0x00 );		// 0x0150
	RegWriteA( WG_PANSTT24JUG1, 	0x00 );		// 0x0151
	// State 4 -> 2
	RegWriteA( WG_PANSTT42JUG0, 	0x44 );		// 0x0152
	RegWriteA( WG_PANSTT42JUG1, 	0x04 );		// 0x0153

	// State Timer
	RegWriteA( WG_PANSTT1LEVTMR, 	0x00 );		// 0x015B
	RegWriteA( WG_PANSTT2LEVTMR, 	0x00 );		// 0x015C
	RegWriteA( WG_PANSTT3LEVTMR, 	0x00 );		// 0x015D
	RegWriteA( WG_PANSTT4LEVTMR, 	0x03 );		// 0x015E
	
	// Control filter
	RegWriteA( WG_PANTRSON0, 		0x11 );		// 0x0132	USE I12/iSTP/Gain-Filter
	
	// State Setting
	IniPtMovMod( OFF ) ;							// Pan/Tilt setting (Still)
	
	// Hold
	RegWriteA( WG_PANSTTSETILHLD,	0x00 );		// 0x015F
	
	
	// State2,4 Step Time Setting
	RegWriteA( WG_PANSTT2TMR0,	0x01 );		// 0x013C
	RegWriteA( WG_PANSTT2TMR1,	0x00 );		// 0x013D	
	RegWriteA( WG_PANSTT4TMR0,	0x02 );		// 0x013E
	RegWriteA( WG_PANSTT4TMR1,	0x07 );		// 0x013F	
	
	RegWriteA( WG_PANSTTXXXTH,	0x00 );		// 0x015A

#ifdef GAIN_CONT
	RamWrite32A( gxlevlow, TRI_LEVEL );					// 0x10AE	Low Th
	RamWrite32A( gylevlow, TRI_LEVEL );					// 0x11AE	Low Th
	RamWrite32A( gxadjmin, XMINGAIN );					// 0x1094	Low gain
	RamWrite32A( gxadjmax, XMAXGAIN );					// 0x1095	Hgh gain
	RamWrite32A( gxadjdn, XSTEPDN );					// 0x1096	-step
	RamWrite32A( gxadjup, XSTEPUP );					// 0x1097	+step
	RamWrite32A( gyadjmin, YMINGAIN );					// 0x1194	Low gain
	RamWrite32A( gyadjmax, YMAXGAIN );					// 0x1195	Hgh gain
	RamWrite32A( gyadjdn, YSTEPDN );					// 0x1196	-step
	RamWrite32A( gyadjup, YSTEPUP );					// 0x1197	+step
	
	RegWriteA( WG_LEVADD, (unsigned char)MONADR );		// 0x0120	Input signal
	RegWriteA( WG_LEVTMR, 		TIMEBSE );				// 0x0123	Base Time
	RegWriteA( WG_LEVTMRLOW, 	TIMELOW );				// 0x0121	X Low Time
	RegWriteA( WG_LEVTMRHGH, 	TIMEHGH );				// 0x0122	X Hgh Time
	RegWriteA( WG_ADJGANADD, (unsigned char)GANADR );		// 0x0128	control address
	RegWriteA( WG_ADJGANGO, 		0x00 );					// 0x0108	manual off

	/* exe function */
//	AutoGainControlSw( OFF ) ;							/* Auto Gain Control Mode OFF */
	AutoGainControlSw( ON ) ;							/* Auto Gain Control Mode ON  */
#endif
	
}


//********************************************************************************
// Function Name 	: IniFil
// Retun Value		: NON
// Argment Value	: NON
// Explanation		: Gyro Filter Initial Parameter Setting
// History			: First edition 						2009.07.30 Y.Tashita
//********************************************************************************
void	IniFil( void )
{
 	unsigned short	UsAryId ;

	// Filter Registor Parameter Setting
	UsAryId	= 0 ;
	while( CsFilReg[ UsAryId ].UsRegAdd != 0xFFFF )
	{
		RegWriteA( CsFilReg[ UsAryId ].UsRegAdd, CsFilReg[ UsAryId ].UcRegDat ) ;
		UsAryId++ ;
	}

	// Filter Ram Parameter Setting
	UsAryId	= 0 ;
	while( CsFilRam[ UsAryId ].UsRamAdd != 0xFFFF )
	{
		RamWrite32A( CsFilRam[ UsAryId ].UsRamAdd, CsFilRam[ UsAryId ].UlRamDat ) ;
		UsAryId++ ;
	}
	
}



//********************************************************************************
// Function Name 	: IniAdj
// Retun Value		: NON
// Argment Value	: NON
// Explanation		: Adjust Value Setting
// History			: First edition 						2009.07.30 Y.Tashita
//********************************************************************************
void	IniAdj( void )
{
	RegWriteA( WC_RAMACCXY, 0x00 ) ;			// 0x018D	Filter copy off

	IniPtAve( ) ;								// Average setting
	
	/* OIS */
	RegWriteA( CMSDAC0, BIAS_CUR_OIS ) ;		// 0x0251	Hall Dac電流
	RegWriteA( OPGSEL0, AMP_GAIN_X ) ;			// 0x0253	Hall amp Gain X
	RegWriteA( OPGSEL1, AMP_GAIN_Y ) ;			// 0x0254	Hall amp Gain Y
	/* AF */
	RegWriteA( CMSDAC1, BIAS_CUR_AF ) ;			// 0x0252	Hall Dac電流
	RegWriteA( OPGSEL2, AMP_GAIN_AF ) ;			// 0x0255	Hall amp Gain AF

	RegWriteA( OSCSET, OSC_INI ) ;				// 0x0257	OSC ini
	
	/* adjusted value */
	RegWriteA( IZAH,	DGYRO_OFST_XH ) ;	// 0x02A0		Set Offset High byte
	RegWriteA( IZAL,	DGYRO_OFST_XL ) ;	// 0x02A1		Set Offset Low byte
	RegWriteA( IZBH,	DGYRO_OFST_YH ) ;	// 0x02A2		Set Offset High byte
	RegWriteA( IZBL,	DGYRO_OFST_YL ) ;	// 0x02A3		Set Offset Low byte
	
	/* Ram Access */
	RamAccFixMod( ON ) ;							// 16bit Fix mode
	
	/* OIS adjusted parameter */
	RamWriteA( DAXHLO,		DAHLXO_INI ) ;		// 0x1479
	RamWriteA( DAXHLB,		DAHLXB_INI ) ;		// 0x147A
	RamWriteA( DAYHLO,		DAHLYO_INI ) ;		// 0x14F9
	RamWriteA( DAYHLB,		DAHLYB_INI ) ;		// 0x14FA
	RamWriteA( OFF0Z,		HXOFF0Z_INI ) ;		// 0x1450
	RamWriteA( OFF1Z,		HYOFF1Z_INI ) ;		// 0x14D0
	RamWriteA( sxg,			SXGAIN_INI ) ;		// 0x10D3
	RamWriteA( syg,			SYGAIN_INI ) ;		// 0x11D3
//	UsCntXof = OPTCEN_X ;						/* Clear Optical center X value */
//	UsCntYof = OPTCEN_Y ;						/* Clear Optical center Y value */
//	RamWriteA( SXOFFZ1,		UsCntXof ) ;		// 0x1461
//	RamWriteA( SYOFFZ1,		UsCntYof ) ;		// 0x14E1

	/* AF adjusted parameter */
	RamWriteA( DAZHLO,		DAHLZO_INI ) ;		// 0x1529
	RamWriteA( DAZHLB,		DAHLZB_INI ) ;		// 0x152A

	/* Ram Access */
	RamAccFixMod( OFF ) ;							// 32bit Float mode
	
	RamWrite32A( gxzoom, GXGAIN_INI ) ;		// 0x1020 Gyro X axis Gain adjusted value
	RamWrite32A( gyzoom, GYGAIN_INI ) ;		// 0x1120 Gyro Y axis Gain adjusted value

	RamWrite32A( sxq, SXQ_INI ) ;			// 0x10E5	X axis output direction initial value
	RamWrite32A( syq, SYQ_INI ) ;			// 0x11E5	Y axis output direction initial value
	
#ifdef USE_INVENSENSE
 #ifdef 	IDG2030
	RamWrite32A( gx45g, G_45G_INI ) ;			// 0x1000
	RamWrite32A( gy45g, G_45G_INI ) ;			// 0x1100
	ClrGyr( 0x00FF , CLR_FRAM1 );		// Gyro Delay RAM Clear
 #endif
#endif
	if( GXHY_GYHX ){			/* GX -> HY , GY -> HX */
		RamWrite32A( sxgx, 0x00000000 ) ;			// 0x10B8
		RamWrite32A( sxgy, 0x3F800000 ) ;			// 0x10B9
		
		RamWrite32A( sygy, 0x00000000 ) ;			// 0x11B8
		RamWrite32A( sygx, 0x3F800000 ) ;			// 0x11B9
	}
	
	SetZsp(0) ;								// Zoom coefficient Initial Setting
	
	RegWriteA( PWMA 	, 0xC0 );			// 0x0010		PWM enable

	RegWriteA( STBB0 	, 0xDF );			// 0x0250 	[ STBAFDRV | STBOISDRV | STBOPAAF | STBOPAY ][ STBOPAX | STBDACI | STBDACV | STBADC ]
	RegWriteA( WC_EQSW	, 0x02 ) ;			// 0x01E0
	RegWriteA( WC_MESLOOP1	, 0x02 ) ;		// 0x0193
	RegWriteA( WC_MESLOOP0	, 0x00 ) ;		// 0x0192
	RegWriteA( WC_AMJLOOP1	, 0x02 ) ;		// 0x01A3
	RegWriteA( WC_AMJLOOP0	, 0x00 ) ;		// 0x01A2
	
	
	SetPanTiltMode( OFF ) ;					/* Pan/Tilt OFF */
	
	SetGcf( 0 ) ;							/* DI initial value */
#ifdef H1COEF_CHANGER
	SetH1cMod( ACTMODE ) ;					/* Lvl Change Active mode */
#endif
	
	DrvSw( ON ) ;							/* 0x0001		Driver Mode setting */
	
	RegWriteA( WC_EQON, 0x01 ) ;			// 0x0101	Filter ON
}



//********************************************************************************
// Function Name 	: IniCmd
// Retun Value		: NON
// Argment Value	: NON
// Explanation		: Command Execute Process Initial
// History			: First edition 						2009.07.30 Y.Tashita
//********************************************************************************
void	IniCmd( void )
{

	MemClr( ( unsigned char * )&StAdjPar, sizeof( stAdjPar ) ) ;	// Adjust Parameter Clear
	
}


//********************************************************************************
// Function Name 	: BsyWit
// Retun Value		: NON
// Argment Value	: Trigger Register Address, Trigger Register Data
// Explanation		: Busy Wait Function
// History			: First edition 						2013.01.09 Y.Shigeoka
//********************************************************************************
void	BsyWit( unsigned short	UsTrgAdr, unsigned char	UcTrgDat )
{
	unsigned char	UcFlgVal ;

	RegWriteA( UsTrgAdr, UcTrgDat ) ;	// Trigger Register Setting

	UcFlgVal	= 1 ;

	while( UcFlgVal ) {

		RegReadA( UsTrgAdr, &UcFlgVal ) ;
		UcFlgVal	&= 	( UcTrgDat & 0x0F ) ;

		if( CmdRdChk() !=0 )	break;		// Dead Lock check (responce check)

	} ;

}


//********************************************************************************
// Function Name 	: MemClr
// Retun Value		: void
// Argment Value	: Clear Target Pointer, Clear Byte Number
// Explanation		: Memory Clear Function
// History			: First edition 						2009.07.30 Y.Tashita
//********************************************************************************
void	MemClr( unsigned char	*NcTgtPtr, unsigned short	UsClrSiz )
{
	unsigned short	UsClrIdx ;

	for ( UsClrIdx = 0 ; UsClrIdx < UsClrSiz ; UsClrIdx++ )
	{
		*NcTgtPtr	= 0 ;
		NcTgtPtr++ ;
	}
}



//********************************************************************************
// Function Name 	: WitTim
// Retun Value		: NON
// Argment Value	: Wait Time(ms)
// Explanation		: Timer Wait Function
// History			: First edition 						2009.07.31 Y.Tashita
//********************************************************************************
/*void	WitTim( unsigned short	UsWitTim )
{
	unsigned long	UlLopIdx, UlWitCyc ;

	UlWitCyc	= ( unsigned long )( ( float )UsWitTim / NOP_TIME / ( float )12 ) ;

	for( UlLopIdx = 0 ; UlLopIdx < UlWitCyc ; UlLopIdx++ )
	{
		;
	}
}*/

//********************************************************************************
// Function Name 	: GyOutSignal
// Retun Value		: NON
// Argment Value	: NON
// Explanation		: Select Gyro Signal Function
// History			: First edition 						2010.12.27 Y.Shigeoka
//********************************************************************************
void	GyOutSignal( void )
{

	RegWriteA( GRADR0,	GYROX_INI ) ;			// 0x0283	Set Gyro XOUT H~L
	RegWriteA( GRADR1,	GYROY_INI ) ;			// 0x0284	Set Gyro YOUT H~L
	
	/*Start OIS Reading*/
	RegWriteA( GRSEL	, 0x02 );			// 0x0280	[ - | - | - | - ][ - | SRDMOE | OISMODE | COMMODE ]

}

//********************************************************************************
// Function Name 	: GyOutSignalCont
// Retun Value		: NON
// Argment Value	: NON
// Explanation		: Select Gyro Continuosl Function
// History			: First edition 						2013.06.06 Y.Shigeoka
//********************************************************************************
void	GyOutSignalCont( void )
{

	/*Start OIS Reading*/
	RegWriteA( GRSEL	, 0x04 );			// 0x0280	[ - | - | - | - ][ - | SRDMOE | OISMODE | COMMODE ]

}

#ifdef STANDBY_MODE
//********************************************************************************
// Function Name 	: AccWit
// Retun Value		: NON
// Argment Value	: Trigger Register Data
// Explanation		: Acc Wait Function
// History			: First edition 						2010.12.27 Y.Shigeoka
//********************************************************************************
void	AccWit( unsigned char UcTrgDat )
{
	unsigned char	UcFlgVal ;

	UcFlgVal	= 1 ;

	while( UcFlgVal ) {
		RegReadA( GRACC, &UcFlgVal ) ;			// 0x0282
		UcFlgVal	&= UcTrgDat ;

		if( CmdRdChk() !=0 )	break;		// Dead Lock check (responce check)

	} ;

}

//********************************************************************************
// Function Name 	: SelectGySleep
// Retun Value		: NON
// Argment Value	: mode	
// Explanation		: Select Gyro mode Function
// History			: First edition 						2010.12.27 Y.Shigeoka
//********************************************************************************
void	SelectGySleep( unsigned char UcSelMode )
{
 #ifdef USE_INVENSENSE
	unsigned char	UcRamIni ;
	unsigned char	UcGrini ;

	if(UcSelMode == ON)
	{
		RegWriteA( WC_EQON, 0x00 ) ;		// 0x0101	Equalizer OFF
		RegWriteA( GRSEL,	0x01 ) ;		/* 0x0280	Set Command Mode			*/

		RegReadA( GRINI	, &UcGrini );					// 0x0281	[ PARA_REG | AXIS7EN | AXIS4EN | - ][ - | SLOWMODE | - | - ]
		RegWriteA( GRINI, ( UcGrini | SLOWMODE) );		// 0x0281	[ PARA_REG | AXIS7EN | AXIS4EN | - ][ - | SLOWMODE | - | - ]
		
		RegWriteA( GRADR0,	0x6B ) ;		/* 0x0283	Set Write Command			*/
		RegWriteA( GRACC,	0x01 ) ;		/* 0x0282	Set Read Trigger ON				*/
		AccWit( 0x01 ) ;					/* Digital Gyro busy wait 				*/
		RegReadA( GRDAT0H, &UcRamIni ) ;	/* 0x0290 */
		
		UcRamIni |= 0x40 ;					/* Set Sleep bit */
  #ifdef GYROSTBY
		UcRamIni &= ~0x01 ;					/* Clear PLL bit(internal oscillator */
  #endif
		
		RegWriteA( GRADR0,	0x6B ) ;		/* 0x0283	Set Write Command			*/
		RegWriteA( GSETDT,	UcRamIni ) ;	/* 0x028A	Set Write Data(Sleep ON)	*/
		RegWriteA( GRACC,	0x10 ) ;		/* 0x0282	Set Trigger ON				*/
		AccWit( 0x10 ) ;					/* Digital Gyro busy wait 				*/

  #ifdef GYROSTBY
		RegWriteA( GRADR0,	0x6C ) ;		/* 0x0283	Set Write Command			*/
		RegWriteA( GSETDT,	0x07 ) ;		/* 0x028A	Set Write Data(STBY ON)	*/
		RegWriteA( GRACC,	0x10 ) ;		/* 0x0282	Set Trigger ON				*/
		AccWit( 0x10 ) ;					/* Digital Gyro busy wait 				*/
  #endif
	}
	else
	{
  #ifdef GYROSTBY
		RegWriteA( GRADR0,	0x6C ) ;		/* 0x0283	Set Write Command			*/
		RegWriteA( GSETDT,	0x00 ) ;		/* 0x028A	Set Write Data(STBY OFF)	*/
		RegWriteA( GRACC,	0x10 ) ;		/* 0x0282	Set Trigger ON				*/
		AccWit( 0x10 ) ;					/* Digital Gyro busy wait 				*/
  #endif
		RegWriteA( GRADR0,	0x6B ) ;		/* 0x0283	Set Write Command			*/
		RegWriteA( GRACC,	0x01 ) ;		/* 0x0282	Set Read Trigger ON				*/
		AccWit( 0x01 ) ;					/* Digital Gyro busy wait 				*/
		RegReadA( GRDAT0H, &UcRamIni ) ;	/* 0x0290 */
		
		UcRamIni &= ~0x40 ;					/* Clear Sleep bit */
  #ifdef GYROSTBY
		UcRamIni |=  0x01 ;					/* Set PLL bit */
  #endif
		
		RegWriteA( GSETDT,	UcRamIni ) ;	// 0x028A	Set Write Data(Sleep OFF)
		RegWriteA( GRACC,	0x10 ) ;		/* 0x0282	Set Trigger ON				*/
		AccWit( 0x10 ) ;					/* Digital Gyro busy wait 				*/
		
		RegReadA( GRINI	, &UcGrini );					// 0x0281	[ PARA_REG | AXIS7EN | AXIS4EN | - ][ LSBF | SLOWMODE | I2CMODE | - ]
		RegWriteA( GRINI, ( UcGrini & ~SLOWMODE) );		// 0x0281	[ PARA_REG | AXIS7EN | AXIS4EN | - ][ LSBF | SLOWMODE | I2CMODE | - ]
		
		GyOutSignal( ) ;					/* Select Gyro output signal 			*/
		
		WitTim( 50 ) ;						// 50ms wait
		
		RegWriteA( WC_EQON, 0x01 ) ;		// 0x0101	GYRO Equalizer ON

		ClrGyr( 0x007F , CLR_FRAM1 );		// Gyro Delay RAM Clear
	}
 #else									/* Panasonic */
	
//	unsigned char	UcRamIni ;


	if(UcSelMode == ON)
	{
		RegWriteA( WC_EQON, 0x00 ) ;		// 0x0101	GYRO Equalizer OFF
		RegWriteA( GRSEL,	0x01 ) ;		/* 0x0280	Set Command Mode			*/
		RegWriteA( GRADR0,	0x4C ) ;		/* 0x0283	Set Write Command			*/
		RegWriteA( GSETDT,	0x02 ) ;		/* 0x028A	Set Write Data(Sleep ON)	*/
		RegWriteA( GRACC,	0x10 ) ;		/* 0x0282	Set Trigger ON				*/
		AccWit( 0x10 ) ;					/* Digital Gyro busy wait 				*/
	}
	else
	{
		RegWriteA( GRADR0,	0x4C ) ;		// 0x0283	Set Write Command
		RegWriteA( GSETDT,	0x00 ) ;		// 0x028A	Set Write Data(Sleep OFF)
		RegWriteA( GRACC,	0x10 ) ;		/* 0x0282	Set Trigger ON				*/
		AccWit( 0x10 ) ;					/* Digital Gyro busy wait 				*/
		GyOutSignal( ) ;					/* Select Gyro output signal 			*/
		
		WitTim( 50 ) ;						// 50ms wait
		
		RegWriteA( WC_EQON, 0x01 ) ;		// 0x0101	GYRO Equalizer ON
		ClrGyr( 0x007F , CLR_FRAM1 );		// Gyro Delay RAM Clear
	}
 #endif
}
#endif

#ifdef	GAIN_CONT
//********************************************************************************
// Function Name 	: AutoGainControlSw
// Retun Value		: NON
// Argment Value	: 0 :OFF  1:ON
// Explanation		: Select Gyro Signal Function
// History			: First edition 						2010.11.30 Y.Shigeoka
//********************************************************************************
void	AutoGainControlSw( unsigned char UcModeSw )
{

	if( UcModeSw == OFF )
	{
		RegWriteA( WG_ADJGANGXATO, 	0xA0 );					// 0x0129	X exe off
		RegWriteA( WG_ADJGANGYATO, 	0xA0 );					// 0x012A	Y exe off
		RamWrite32A( GANADR			 , XMAXGAIN ) ;			// Gain Through
		RamWrite32A( GANADR | 0x0100 , YMAXGAIN ) ;			// Gain Through
	}
	else
	{
		RegWriteA( WG_ADJGANGXATO, 	0xA3 );					// 0x0129	X exe on
		RegWriteA( WG_ADJGANGYATO, 	0xA3 );					// 0x012A	Y exe on
	}

}
#endif


//********************************************************************************
// Function Name 	: ClrGyr
// Retun Value		: NON
// Argment Value	: UsClrFil - Select filter to clear.  If 0x0000, clears entire filter
//					  UcClrMod - 0x01: FRAM0 Clear, 0x02: FRAM1, 0x03: All RAM Clear
// Explanation		: Gyro RAM clear function
// History			: First edition 						2013.01.09 Y.Shigeoka
//********************************************************************************
void	ClrGyr( unsigned short UsClrFil , unsigned char UcClrMod )
{
	unsigned char	UcRamClr;
	unsigned char	count = 0; 

	/*Select Filter to clear*/
	RegWriteA( WC_RAMDLYMOD1,	(unsigned char)(UsClrFil >> 8) ) ;		// 0x018F		FRAM Initialize Hbyte
	RegWriteA( WC_RAMDLYMOD0,	(unsigned char)UsClrFil ) ;				// 0x018E		FRAM Initialize Lbyte

	/*Enable Clear*/
	RegWriteA( WC_RAMINITON	, UcClrMod ) ;	// 0x0102	[ - | - | - | - ][ - | - | 遅延Clr | 係数Clr ]
	
	/*Check RAM Clear complete*/
	do{
		RegReadA( WC_RAMINITON, &UcRamClr );
		UcRamClr &= UcClrMod;

		if( count++ >= 100 ){
			break;
		}

	}while( UcRamClr != 0x00 );
}


//********************************************************************************
// Function Name 	: DrvSw
// Retun Value		: NON
// Argment Value	: 0:OFF  1:ON
// Explanation		: Driver Mode setting function
// History			: First edition 						2012.04.25 Y.Shigeoka
//********************************************************************************
void	DrvSw( unsigned char UcDrvSw )
{
	if( UcDrvSw == ON )
	{
		if( UcPwmMod == PWMMOD_CVL ) {
			RegWriteA( DRVFC	, 0xF0 );			// 0x0001	Drv.MODE=1,Drv.BLK=1,MODE2,LCEN
		} else {
#ifdef	PWM_BREAK
			RegWriteA( DRVFC	, 0x00 );			// 0x0001	Drv.MODE=0,Drv.BLK=0,MODE0B
#else
			RegWriteA( DRVFC	, 0xC0 );			// 0x0001	Drv.MODE=1,Drv.BLK=1,MODE1
#endif
		}
	}
	else
	{
		if( UcPwmMod == PWMMOD_CVL ) {
			RegWriteA( DRVFC	, 0x30 );				// 0x0001	Drvier Block Ena=0
		} else {
#ifdef	PWM_BREAK
			RegWriteA( DRVFC	, 0x00 );				// 0x0001	Drv.MODE=0,Drv.BLK=0,MODE0B
#else
			RegWriteA( DRVFC	, 0x00 );				// 0x0001	Drvier Block Ena=0
#endif
		}
	}
}

//********************************************************************************
// Function Name 	: AfDrvSw
// Retun Value		: NON
// Argment Value	: 0:OFF  1:ON
// Explanation		: AF Driver Mode setting function
// History			: First edition 						2013.09.12 Y.Shigeoka
//********************************************************************************
void	AfDrvSw( unsigned char UcDrvSw )
{
	if( UcDrvSw == ON )
	{
		RegWriteA( DRVFCAF	, 0x20 );				// 0x0081	Drv.MODEAF=0,Drv.ENAAF=0,MODE-2
		RegWriteA( CCAAF,   0x80 ) ;				// 0x00A0	[7]=0:OFF 1:ON
	}
	else
	{
		RegWriteA( CCAAF,   0x00 ) ;				// 0x00A0	[7]=0:OFF 1:ON
	}
}

//********************************************************************************
// Function Name 	: RamAccFixMod
// Retun Value		: NON
// Argment Value	: 0:OFF  1:ON
// Explanation		: Ram Access Fix Mode setting function
// History			: First edition 						2013.05.21 Y.Shigeoka
//********************************************************************************
void	RamAccFixMod( unsigned char UcAccMod )
{
	switch ( UcAccMod ) {
		case OFF :
			RegWriteA( WC_RAMACCMOD,	0x00 ) ;		// 0x018C		GRAM Access Float32bit
			break ;
		case ON :
			RegWriteA( WC_RAMACCMOD,	0x31 ) ;		// 0x018C		GRAM Access Fix32bit
			break ;
	}
}
	

//********************************************************************************
// Function Name 	: IniAf
// Retun Value		: NON
// Argment Value	: NON
// Explanation		: Open AF Initial Setting
// History			: First edition 						2013.09.12 Y.Shigeoka
//********************************************************************************
void	IniAf( void )
{
	unsigned char	UcStbb0 ;
	
	AfDrvSw( OFF ) ;								/* AF Drvier Block Ena=0 */
	RegWriteA( DRVFCAF	, 0x20 );					// 0x0081	Drv.MODEAF=0,Drv.ENAAF=0,MODE-2
	RegWriteA( DRVFC3AF	, 0x00 );					// 0x0083	DGAINDAF	Gain 0
	RegWriteA( DRVFC4AF	, 0x80 );					// 0x0084	DOFSTDAF
	RegWriteA( PWMAAF,    0x00 ) ;					// 0x0090	AF PWM standby
	RegWriteA( AFFC,   0x80 ) ;						// 0x0088	OpenAF/-/-
	RegWriteA( DRVFC2AF,    0x00 ) ;				// 0x0082	AF slope0
	RegWriteA( DRVCH3SEL,   0x00 ) ;				// 0x0085	AF H bridge control
	RegWriteA( PWMFCAF,     0x01 ) ;				// 0x0091	AF VREF , Carrier , MODE1
	RegWriteA( PWMPERIODAF, 0x20 ) ;				// 0x0099	AF none-synchronism
	RegWriteA( CCFCAF,   0x40 ) ;					// 0x00A1	GND/-
	
	RegReadA( STBB0 	, &UcStbb0 );		// 0x0250 	[ STBAFDRV | STBOISDRV | STBOPAAF | STBOPAY ][ STBOPAX | STBDACI | STBDACV | STBADC ]
	UcStbb0 &= 0x7F ;
	RegWriteA( STBB0, UcStbb0 ) ;			// 0x0250	OIS standby
	RegWriteA( STBB1, 0x00 ) ;				// 0x0264	All standby
	
	/* AF Initial setting */
	RegWriteA( FSTMODE,		FSTMODE_AF ) ;		// 0x0302
	RamWriteA( RWEXD1_L,	RWEXD1_L_AF ) ;		// 0x0396 - 0x0397 (Register continuos write)
	RamWriteA( RWEXD2_L,	RWEXD2_L_AF ) ;		// 0x0398 - 0x0399 (Register continuos write)
	RamWriteA( RWEXD3_L,	RWEXD3_L_AF ) ;		// 0x039A - 0x039B (Register continuos write)
	RegWriteA( FSTCTIME,	FSTCTIME_AF ) ;		// 0x0303 	
	RamWriteA( TCODEH,		0x0000 ) ;			// 0x0304 - 0x0305 (Register continuos write)
	

	UcStbb0 |= 0x80 ;
	RegWriteA( STBB0, UcStbb0 ) ;			// 0x0250	
	RegWriteA( STBB1	, 0x05 ) ;			// 0x0264	[ - | - | - | - ][ - | STBAFOP1 | - | STBAFDAC ]

	AfDrvSw( ON ) ;								/* AF Drvier Block Ena=1 */
}



//********************************************************************************
// Function Name 	: IniPtAve
// Retun Value		: NON
// Argment Value	: NON
// Explanation		: Pan/Tilt Average parameter setting function
// History			: First edition 						2013.09.26 Y.Shigeoka
//********************************************************************************
void	IniPtAve( void )
{
	RegWriteA( WG_PANSTT1DWNSMP0, 0x00 );		// 0x0134
	RegWriteA( WG_PANSTT1DWNSMP1, 0x00 );		// 0x0135
	RegWriteA( WG_PANSTT2DWNSMP0, 0x90 );		// 0x0136 400
	RegWriteA( WG_PANSTT2DWNSMP1, 0x01 );		// 0x0137
	RegWriteA( WG_PANSTT3DWNSMP0, 0x64 );		// 0x0138 100
	RegWriteA( WG_PANSTT3DWNSMP1, 0x00 );		// 0x0139
	RegWriteA( WG_PANSTT4DWNSMP0, 0x00 );		// 0x013A
	RegWriteA( WG_PANSTT4DWNSMP1, 0x00 );		// 0x013B

	RamWrite32A( st1mean, 0x3f800000 );		// 0x1235
	RamWrite32A( st2mean, 0x3B23D700 );		// 0x1236	1/400
	RamWrite32A( st3mean, 0x3C23D700 );		// 0x1237	1/100
	RamWrite32A( st4mean, 0x3f800000 );		// 0x1238
			
}
	
//********************************************************************************
// Function Name 	: IniPtMovMod
// Retun Value		: NON
// Argment Value	: OFF:Still  ON:Movie
// Explanation		: Pan/Tilt parameter setting by mode function
// History			: First edition 						2013.09.26 Y.Shigeoka
//********************************************************************************
void	IniPtMovMod( unsigned char UcPtMod )
{
	switch ( UcPtMod ) {
		case OFF :
			RegWriteA( WG_PANSTTSETGYRO, 	0x00 );		// 0x0154
			RegWriteA( WG_PANSTTSETGAIN, 	0x54 );		// 0x0155
			RegWriteA( WG_PANSTTSETISTP, 	0x14 );		// 0x0156
			RegWriteA( WG_PANSTTSETIFTR,	0x94 );		// 0x0157
			RegWriteA( WG_PANSTTSETLFTR,	0x00 );		// 0x0158

			break ;
		case ON :
			RegWriteA( WG_PANSTTSETGYRO, 	0x00 );		// 0x0154
			RegWriteA( WG_PANSTTSETGAIN, 	0x00 );		// 0x0155
			RegWriteA( WG_PANSTTSETISTP, 	0x14 );		// 0x0156
			RegWriteA( WG_PANSTTSETIFTR,	0x94 );		// 0x0157
			RegWriteA( WG_PANSTTSETLFTR,	0x00 );		// 0x0158
			break ;
	}
}

//********************************************************************************
// Function Name 	: ChkCvr
// Retun Value		: NON
// Argment Value	: NON
// Explanation		: Check Cver function
// History			: First edition 						2013.10.03 Y.Shigeoka
//********************************************************************************
void	ChkCvr( void )
{
	RegReadA( CVER ,	&UcCvrCod );		// 0x027E
	RegWriteA( MDLREG ,	MDL_VER );			// 0x00FF	Model
	RegWriteA( VRREG ,	FW_VER );			// 0x02D0	Version
}

