//********************************************************************************
//
//		<< LC898122 Evaluation Soft >>
//	    Program Name	: OisCmd.c
//		Design			: Y.Yamada
//		History			: First edition						2009.07.31 Y.Tashita
//********************************************************************************
//**************************
//	Include Header File		
//**************************
#define		OISCMD

//#include	"Main.h"
//#include	"Cmd.h"
#include	"Ois.h"
#include	"OisDef.h"

//**************************
//	Local Function Prottype	
//**************************
void			MesFil( unsigned char ) ;					// Measure Filter Setting
unsigned long	GinMes( unsigned char ) ;					// Measure Result Getting
void			GyrCon( unsigned char ) ;					// Gyro Filter Control
short			GenMes( unsigned short, unsigned char ) ;	// General Measure
void 			StbOnn( void ) ;							// Servo ON Slope mode

void			SetSineWave(   unsigned char , unsigned char );
void			StartSineWave( void );
void			StopSineWave(  void );

void			SetMeasFil(  unsigned char );
void			ClrMeasFil( void );
unsigned char	TstActMov( unsigned char );



//**************************
//	define					
//**************************
#define		MES_XG1			0								// LXG1 Measure Mode
#define		MES_XG2			1								// LXG2 Measure Mode

#define		HALL_ADJ		0
#define		LOOPGAIN		1
#define		THROUGH			2
#define		NOISE			3

// Measure Mode

#ifdef H1COEF_CHANGER
 #ifdef	CORRECT_1DEG
  #define		MAXLMT		0x40600000				// 3.5
  #define		MINLMT		0x400CCCCD				// 2.2
  #define		CHGCOEF		0xBA0D89D9				// 
  #define		MINLMT_MOV	0x00000000				// 0.0
  #define		CHGCOEF_MOV	0xB8A49249
 #else
  #define		MAXLMT		0x40000000				// 2.0
  #define		MINLMT		0x3F8CCCCD				// 1.1
  #define		CHGCOEF		0xBA4C71C7				// 
  #define		MINLMT_MOV	0x00000000				// 0.0
  #define		CHGCOEF_MOV	0xB9700000
 #endif
#endif

//**************************
//	Global Variable			
//**************************
 unsigned short	UsStpSiz	= 0 ;							// Bias Step Size
 unsigned short	UsErrBia, UsErrOfs ;



//**************************
//	Const					
//**************************
// gxzoom Setting Value
#define		ZOOMTBL	16
const unsigned long	ClGyxZom[ ZOOMTBL ]	= {
		0x3F800000,
		0x3F800000,
		0x3F800000,
		0x3F800000,
		0x3F800000,
		0x3F800000,
		0x3F800000,
		0x3F800000,
		0x3F800000,
		0x3F800000,
		0x3F800000,
		0x3F800000,
		0x3F800000,
		0x3F800000,
		0x3F800000,
		0x3F800000
	} ;

// gyzoom Setting Value
const unsigned long	ClGyyZom[ ZOOMTBL ]	= {
		0x3F800000,
		0x3F800000,
		0x3F800000,
		0x3F800000,
		0x3F800000,
		0x3F800000,
		0x3F800000,
		0x3F800000,
		0x3F800000,
		0x3F800000,
		0x3F800000,
		0x3F800000,
		0x3F800000,
		0x3F800000,
		0x3F800000,
		0x3F800000
	} ;

// DI Coefficient Setting Value
#define		COEFTBL	7
const unsigned long	ClDiCof[ COEFTBL ]	= {
		DIFIL_S2,		/* 0 */
		DIFIL_S2,		/* 1 */
		DIFIL_S2,		/* 2 */
		DIFIL_S2,		/* 3 */
		DIFIL_S2,		/* 4 */
		DIFIL_S2,		/* 5 */
		DIFIL_S2		/* 6 */
	} ;
	


//********************************************************************************
// Function Name 	: MesFil
// Retun Value		: NON
// Argment Value	: Measure Filter Mode
// Explanation		: Measure Filter Setting Function
// History			: First edition 						2009.07.31  Y.Tashita
//********************************************************************************
void	MesFil( unsigned char	UcMesMod )
{
	if( !UcMesMod ) {								// Hall Bias&Offset Adjust
		// Measure Filter1 Setting
		RamWrite32A( mes1aa, 0x3CA175C0 ) ;		// 0x10F0	LPF150Hz
		RamWrite32A( mes1ab, 0x3CA175C0 ) ;		// 0x10F1
		RamWrite32A( mes1ac, 0x3F75E8C0 ) ;		// 0x10F2
		RamWrite32A( mes1ad, 0x00000000 ) ;		// 0x10F3
		RamWrite32A( mes1ae, 0x00000000 ) ;		// 0x10F4
		RamWrite32A( mes1ba, 0x3F800000 ) ;		// 0x10F5	Through
		RamWrite32A( mes1bb, 0x00000000 ) ;		// 0x10F6
		RamWrite32A( mes1bc, 0x00000000 ) ;		// 0x10F7
		RamWrite32A( mes1bd, 0x00000000 ) ;		// 0x10F8
		RamWrite32A( mes1be, 0x00000000 ) ;		// 0x10F9
		
		// Measure Filter2 Setting
		RamWrite32A( mes2aa, 0x3CA175C0 ) ;		// 0x11F0	LPF150Hz
		RamWrite32A( mes2ab, 0x3CA175C0 ) ;		// 0x11F1
		RamWrite32A( mes2ac, 0x3F75E8C0 ) ;		// 0x11F2
		RamWrite32A( mes2ad, 0x00000000 ) ;		// 0x11F3
		RamWrite32A( mes2ae, 0x00000000 ) ;		// 0x11F4
		RamWrite32A( mes2ba, 0x3F800000 ) ;		// 0x11F5	Through
		RamWrite32A( mes2bb, 0x00000000 ) ;		// 0x11F6
		RamWrite32A( mes2bc, 0x00000000 ) ;		// 0x11F7
		RamWrite32A( mes2bd, 0x00000000 ) ;		// 0x11F8
		RamWrite32A( mes2be, 0x00000000 ) ;		// 0x11F9
		
	} else if( UcMesMod == LOOPGAIN ) {				// Loop Gain Adjust
		// Measure Filter1 Setting
		RamWrite32A( mes1aa, 0x3DF21080 ) ;		// 0x10F0	LPF1000Hz
		RamWrite32A( mes1ab, 0x3DF21080 ) ;		// 0x10F1
		RamWrite32A( mes1ac, 0x3F437BC0 ) ;		// 0x10F2
		RamWrite32A( mes1ad, 0x00000000 ) ;		// 0x10F3
		RamWrite32A( mes1ae, 0x00000000 ) ;		// 0x10F4
		RamWrite32A( mes1ba, 0x3F7EF980 ) ;		// 0x10F5	HPF30Hz
		RamWrite32A( mes1bb, 0xBF7EF980 ) ;		// 0x10F6
		RamWrite32A( mes1bc, 0x3F7DF300 ) ;		// 0x10F7
		RamWrite32A( mes1bd, 0x00000000 ) ;		// 0x10F8
		RamWrite32A( mes1be, 0x00000000 ) ;		// 0x10F9
		
		// Measure Filter2 Setting
		RamWrite32A( mes2aa, 0x3DF21080 ) ;		// 0x11F0	LPF1000Hz
		RamWrite32A( mes2ab, 0x3DF21080 ) ;		// 0x11F1
		RamWrite32A( mes2ac, 0x3F437BC0 ) ;		// 0x11F2
		RamWrite32A( mes2ad, 0x00000000 ) ;		// 0x11F3
		RamWrite32A( mes2ae, 0x00000000 ) ;		// 0x11F4
		RamWrite32A( mes2ba, 0x3F7EF980 ) ;		// 0x11F5	HPF30Hz
		RamWrite32A( mes2bb, 0xBF7EF980 ) ;		// 0x11F6
		RamWrite32A( mes2bc, 0x3F7DF300 ) ;		// 0x11F7
		RamWrite32A( mes2bd, 0x00000000 ) ;		// 0x11F8
		RamWrite32A( mes2be, 0x00000000 ) ;		// 0x11F9
		
	} else if( UcMesMod == THROUGH ) {				// for Through
		// Measure Filter1 Setting
		RamWrite32A( mes1aa, 0x3F800000 ) ;		// 0x10F0	Through
		RamWrite32A( mes1ab, 0x00000000 ) ;		// 0x10F1
		RamWrite32A( mes1ac, 0x00000000 ) ;		// 0x10F2
		RamWrite32A( mes1ad, 0x00000000 ) ;		// 0x10F3
		RamWrite32A( mes1ae, 0x00000000 ) ;		// 0x10F4
		RamWrite32A( mes1ba, 0x3F800000 ) ;		// 0x10F5	Through
		RamWrite32A( mes1bb, 0x00000000 ) ;		// 0x10F6
		RamWrite32A( mes1bc, 0x00000000 ) ;		// 0x10F7
		RamWrite32A( mes1bd, 0x00000000 ) ;		// 0x10F8
		RamWrite32A( mes1be, 0x00000000 ) ;		// 0x10F9
		
		// Measure Filter2 Setting
		RamWrite32A( mes2aa, 0x3F800000 ) ;		// 0x11F0	Through
		RamWrite32A( mes2ab, 0x00000000 ) ;		// 0x11F1
		RamWrite32A( mes2ac, 0x00000000 ) ;		// 0x11F2
		RamWrite32A( mes2ad, 0x00000000 ) ;		// 0x11F3
		RamWrite32A( mes2ae, 0x00000000 ) ;		// 0x11F4
		RamWrite32A( mes2ba, 0x3F800000 ) ;		// 0x11F5	Through
		RamWrite32A( mes2bb, 0x00000000 ) ;		// 0x11F6
		RamWrite32A( mes2bc, 0x00000000 ) ;		// 0x11F7
		RamWrite32A( mes2bd, 0x00000000 ) ;		// 0x11F8
		RamWrite32A( mes2be, 0x00000000 ) ;		// 0x11F9
		
	} else if( UcMesMod == NOISE ) {				// SINE WAVE TEST for NOISE
		// Measure Filter1 Setting
		RamWrite32A( mes1aa, 0x3CA175C0 ) ;		// 0x10F0	LPF150Hz
		RamWrite32A( mes1ab, 0x3CA175C0 ) ;		// 0x10F1
		RamWrite32A( mes1ac, 0x3F75E8C0 ) ;		// 0x10F2
		RamWrite32A( mes1ad, 0x00000000 ) ;		// 0x10F3
		RamWrite32A( mes1ae, 0x00000000 ) ;		// 0x10F4
		RamWrite32A( mes1ba, 0x3CA175C0 ) ;		// 0x10F5	LPF150Hz
		RamWrite32A( mes1bb, 0x3CA175C0 ) ;		// 0x10F6
		RamWrite32A( mes1bc, 0x3F75E8C0 ) ;		// 0x10F7
		RamWrite32A( mes1bd, 0x00000000 ) ;		// 0x10F8
		RamWrite32A( mes1be, 0x00000000 ) ;		// 0x10F9
		
		// Measure Filter2 Setting
		RamWrite32A( mes2aa, 0x3CA175C0 ) ;		// 0x11F0	LPF150Hz
		RamWrite32A( mes2ab, 0x3CA175C0 ) ;		// 0x11F1
		RamWrite32A( mes2ac, 0x3F75E8C0 ) ;		// 0x11F2
		RamWrite32A( mes2ad, 0x00000000 ) ;		// 0x11F3
		RamWrite32A( mes2ae, 0x00000000 ) ;		// 0x11F4
		RamWrite32A( mes2ba, 0x3CA175C0 ) ;		// 0x11F5	LPF150Hz
		RamWrite32A( mes2bb, 0x3CA175C0 ) ;		// 0x11F6
		RamWrite32A( mes2bc, 0x3F75E8C0 ) ;		// 0x11F7
		RamWrite32A( mes2bd, 0x00000000 ) ;		// 0x11F8
		RamWrite32A( mes2be, 0x00000000 ) ;		// 0x11F9
	}
}



//********************************************************************************
// Function Name 	: SrvCon
// Retun Value		: NON
// Argment Value	: X or Y Select, Servo ON/OFF
// Explanation		: Servo ON,OFF Function
// History			: First edition 						2013.01.09 Y.Shigeoka
//********************************************************************************
void	SrvCon( unsigned char	UcDirSel, unsigned char	UcSwcCon )
{
	if( UcSwcCon ) {
		if( !UcDirSel ) {						// X Direction
			RegWriteA( WH_EQSWX , 0x03 ) ;			// 0x0170
			RamWrite32A( sxggf, 0x00000000 ) ;		// 0x10B5
		} else {								// Y Direction
			RegWriteA( WH_EQSWY , 0x03 ) ;			// 0x0171
			RamWrite32A( syggf, 0x00000000 ) ;		// 0x11B5
		}
	} else {
		if( !UcDirSel ) {						// X Direction
			RegWriteA( WH_EQSWX , 0x02 ) ;			// 0x0170
			RamWrite32A( SXLMT, 0x00000000 ) ;		// 0x1477
		} else {								// Y Direction
			RegWriteA( WH_EQSWY , 0x02 ) ;			// 0x0171
			RamWrite32A( SYLMT, 0x00000000 ) ;		// 0x14F7
		}
	}
}






//********************************************************************************
// Function Name 	: RtnCen
// Retun Value		: Command Status
// Argment Value	: Command Parameter
// Explanation		: Return to center Command Function
// History			: First edition 						2013.01.15 Y.Shigeoka
//********************************************************************************
unsigned char	RtnCen( unsigned char	UcCmdPar )
{
	unsigned char	UcCmdSts ;

	UcCmdSts	= EXE_END ;

	GyrCon( OFF ) ;											// Gyro OFF

	if( !UcCmdPar ) {										// X,Y Centering

		StbOnn() ;											// Slope Mode
		
	} else if( UcCmdPar == 0x01 ) {							// X Centering Only

		SrvCon( X_DIR, ON ) ;								// X only Servo ON
		SrvCon( Y_DIR, OFF ) ;
	} else if( UcCmdPar == 0x02 ) {							// Y Centering Only

		SrvCon( X_DIR, OFF ) ;								// Y only Servo ON
		SrvCon( Y_DIR, ON ) ;
	}

	return( UcCmdSts ) ;
}



//********************************************************************************
// Function Name 	: GyrCon
// Retun Value		: NON
// Argment Value	: Gyro Filter ON or OFF
// Explanation		: Gyro Filter Control Function
// History			: First edition 						2013.01.15 Y.Shigeoka
//********************************************************************************
void	GyrCon( unsigned char	UcGyrCon )
{
	// Return HPF Setting
	RegWriteA( WG_SHTON, 0x00 ) ;									// 0x0107
	
	if( UcGyrCon == ON ) {												// Gyro ON

		
#ifdef	GAIN_CONT
		/* Gain3 Register */
//		AutoGainControlSw( ON ) ;											/* Auto Gain Control Mode ON */
#endif
		ClrGyr( 0x000E , CLR_FRAM1 );		// Gyro Delay RAM Clear

		RamWrite32A( sxggf, 0x3F800000 ) ;	// 0x10B5
		RamWrite32A( syggf, 0x3F800000 ) ;	// 0x11B5
		
	} else if( UcGyrCon == SPC ) {										// Gyro ON for LINE

		
#ifdef	GAIN_CONT
		/* Gain3 Register */
//		AutoGainControlSw( ON ) ;											/* Auto Gain Control Mode ON */
#endif

		RamWrite32A( sxggf, 0x3F800000 ) ;	// 0x10B5
		RamWrite32A( syggf, 0x3F800000 ) ;	// 0x11B5
		

	} else {															// Gyro OFF
		
		RamWrite32A( sxggf, 0x00000000 ) ;	// 0x10B5
		RamWrite32A( syggf, 0x00000000 ) ;	// 0x11B5
		

#ifdef	GAIN_CONT
		/* Gain3 Register */
//		AutoGainControlSw( OFF ) ;											/* Auto Gain Control Mode OFF */
#endif
	}
}



//********************************************************************************
// Function Name 	: OisEna
// Retun Value		: NON
// Argment Value	: Command Parameter
// Explanation		: OIS Enable Control Function
// History			: First edition 						2013.01.15 Y.Shigeoka
//********************************************************************************
void	OisEna( void )
{
	// Servo ON
	SrvCon( X_DIR, ON ) ;
	SrvCon( Y_DIR, ON ) ;

	GyrCon( ON ) ;
}

//********************************************************************************
// Function Name 	: OisEnaLin
// Retun Value		: NON
// Argment Value	: Command Parameter
// Explanation		: OIS Enable Control Function for Line adjustment
// History			: First edition 						2013.09.05 Y.Shigeoka
//********************************************************************************
void	OisEnaLin( void )
{
	// Servo ON
	SrvCon( X_DIR, ON ) ;
	SrvCon( Y_DIR, ON ) ;

	GyrCon( SPC ) ;
}



//********************************************************************************
// Function Name 	: TimPro
// Retun Value		: NON
// Argment Value	: NON
// Explanation		: Timer Interrupt Process Function
// History			: First edition 						2013.01.15 Y.Shigeoka
//********************************************************************************
void	TimPro( void )
{
}



//********************************************************************************
// Function Name 	: S2cPro
// Retun Value		: NON
// Argment Value	: NON
// Explanation		: S2 Command Function
// History			: First edition 						2013.01.15 Y.Shigeoka
//********************************************************************************
void	S2cPro( unsigned char uc_mode )
{
	if( uc_mode == 1 )
	{
#ifdef H1COEF_CHANGER
		SetH1cMod( S2MODE ) ;							/* cancel Lvl change */
#endif
		// HPF→Through Setting
		RegWriteA( WG_SHTON, 0x11 ) ;							// 0x0107
		RamWrite32A( gxh1c, DIFIL_S2 );							// 0x1012
		RamWrite32A( gyh1c, DIFIL_S2 );							// 0x1112
	}
	else
	{
		RamWrite32A( gxh1c, UlH1Coefval );							// 0x1012
		RamWrite32A( gyh1c, UlH1Coefval );							// 0x1112
		// HPF→Through Setting
		RegWriteA( WG_SHTON, 0x00 ) ;							// 0x0107

#ifdef H1COEF_CHANGER
		SetH1cMod( UcH1LvlMod ) ;							/* Re-setting */
#endif
	}
	
}


//********************************************************************************
// Function Name 	: GenMes
// Retun Value		: A/D Convert Result
// Argment Value	: Measure Filter Input Signal Ram Address
// Explanation		: General Measure Function
// History			: First edition 						2013.01.10 Y.Shigeoka
//********************************************************************************
short	GenMes( unsigned short	UsRamAdd, unsigned char	UcMesMod )
{
	short	SsMesRlt ;

	RegWriteA( WC_MES1ADD0, (unsigned char)UsRamAdd ) ;							// 0x0194
	RegWriteA( WC_MES1ADD1, (unsigned char)(( UsRamAdd >> 8 ) & 0x0001 ) ) ;	// 0x0195
	RamWrite32A( MSABS1AV, 0x00000000 ) ;				// 0x1041	Clear
	
	if( !UcMesMod ) {
		RegWriteA( WC_MESLOOP1, 0x04 ) ;				// 0x0193
		RegWriteA( WC_MESLOOP0, 0x00 ) ;				// 0x0192	1024 Times Measure
		RamWrite32A( msmean	, 0x3A7FFFF7 );				// 0x1230	1/CmMesLoop[15:0]
	} else {
		RegWriteA( WC_MESLOOP1, 0x00 ) ;				// 0x0193
		RegWriteA( WC_MESLOOP0, 0x01 ) ;				// 0x0192	1 Times Measure
		RamWrite32A( msmean	, 0x3F800000 );				// 0x1230	1/CmMesLoop[15:0]
	}

	RegWriteA( WC_MESABS, 0x00 ) ;						// 0x0198	none ABS
	BsyWit( WC_MESMODE, 0x01 ) ;						// 0x0190	normal Measure

	RamAccFixMod( ON ) ;							// Fix mode
	
	RamReadA( MSABS1AV, ( unsigned short * )&SsMesRlt ) ;	// 0x1041

	RamAccFixMod( OFF ) ;							// Float mode
	
	return( SsMesRlt ) ;
}


//********************************************************************************
// Function Name 	: SetSinWavePara
// Retun Value		: NON
// Argment Value	: NON
// Explanation		: Sine wave Test Function
// History			: First edition 						2013.01.15 Y.Shigeoka
//********************************************************************************
	/********* Parameter Setting *********/
	/* Servo Sampling Clock		=	23.4375kHz						*/
	/* Freq						=	CmSinFreq*Fs/65536/16			*/
	/* 05 00 XX MM 				XX:Freq MM:Sin or Circle */
const unsigned short	CucFreqVal[ 17 ]	= {
		0xFFFF,				//  0:  Stop
		0x002C,				//  1: 0.983477Hz
		0x0059,				//  2: 1.989305Hz
		0x0086,				//  3: 2.995133Hz	
		0x00B2,				//  4: 3.97861Hz
		0x00DF,				//  5: 4.984438Hz
		0x010C,				//  6: 5.990267Hz
		0x0139,				//  7: 6.996095Hz
		0x0165,				//  8: 7.979572Hz
		0x0192,				//  9: 8.9854Hz
		0x01BF,				//  A: 9.991229Hz
		0x01EC,				//  B: 10.99706Hz
		0x0218,				//  C: 11.98053Hz
		0x0245,				//  D: 12.98636Hz
		0x0272,				//  E: 13.99219Hz
		0x029F,				//  F: 14.99802Hz
		0x02CB				// 10: 15.9815Hz
	} ;
	
#define		USE_SINLPF			/* if sin or circle movement is used LPF , this define has to enable */
	
/* 振幅はsxsin(0x10D5),sysin(0x11D5)で調整 */
void	SetSinWavePara( unsigned char UcTableVal ,  unsigned char UcMethodVal )
{
	unsigned short	UsFreqDat ;
	unsigned char	UcEqSwX , UcEqSwY ;

	
	if(UcTableVal > 0x10 )
		UcTableVal = 0x10 ;			/* Limit */
	UsFreqDat = CucFreqVal[ UcTableVal ] ;	
	
	if( UcMethodVal == SINEWAVE) {
		RegWriteA( WC_SINPHSX, 0x00 ) ;					/* 0x0183	*/
		RegWriteA( WC_SINPHSY, 0x00 ) ;					/* 0x0184	*/
	}else if( UcMethodVal == CIRCWAVE ){
		RegWriteA( WC_SINPHSX,	0x00 ) ;				/* 0x0183	*/
		RegWriteA( WC_SINPHSY,	0x20 ) ;				/* 0x0184	*/
	}else{
		RegWriteA( WC_SINPHSX, 0x00 ) ;					/* 0x0183	*/
		RegWriteA( WC_SINPHSY, 0x00 ) ;					/* 0x0184	*/
	}

#ifdef	USE_SINLPF
	if(( UcMethodVal == CIRCWAVE ) || ( UcMethodVal == SINEWAVE )) {
		MesFil( NOISE ) ;			/* LPF */
	}
#endif

	if( UsFreqDat == 0xFFFF )			/* Sine波中止 */
	{

		RegReadA( WH_EQSWX, &UcEqSwX ) ;				/* 0x0170	*/
		RegReadA( WH_EQSWY, &UcEqSwY ) ;				/* 0x0171	*/
		UcEqSwX &= ~EQSINSW ;
		UcEqSwY &= ~EQSINSW ;
		RegWriteA( WH_EQSWX, UcEqSwX ) ;				/* 0x0170	*/
		RegWriteA( WH_EQSWY, UcEqSwY ) ;				/* 0x0171	*/
		
#ifdef	USE_SINLPF
		if(( UcMethodVal == CIRCWAVE ) || ( UcMethodVal == SINEWAVE ) || ( UcMethodVal == XACTTEST ) || ( UcMethodVal == YACTTEST )) {
			RegWriteA( WC_DPON,     0x00 ) ;			/* 0x0105	Data pass off */
			RegWriteA( WC_DPO1ADD0, 0x00 ) ;			/* 0x01B8	output initial */
			RegWriteA( WC_DPO1ADD1, 0x00 ) ;			/* 0x01B9	output initial */
			RegWriteA( WC_DPO2ADD0, 0x00 ) ;			/* 0x01BA	output initial */
			RegWriteA( WC_DPO2ADD1, 0x00 ) ;			/* 0x01BB	output initial */
			RegWriteA( WC_DPI1ADD0, 0x00 ) ;			/* 0x01B0	input initial */
			RegWriteA( WC_DPI1ADD1, 0x00 ) ;			/* 0x01B1	input initial */
			RegWriteA( WC_DPI2ADD0, 0x00 ) ;			/* 0x01B2	input initial */
			RegWriteA( WC_DPI2ADD1, 0x00 ) ;			/* 0x01B3	input initial */
			
			/* Ram Access */
			RamAccFixMod( ON ) ;							// Fix mode
			
			RamWriteA( SXOFFZ1, UsCntXof ) ;			/* 0x1461	set optical value */
			RamWriteA( SYOFFZ1, UsCntYof ) ;			/* 0x14E1	set optical value */
			
			/* Ram Access */
			RamAccFixMod( OFF ) ;							// Float mode
	
			RegWriteA( WC_MES1ADD0,  0x00 ) ;			/* 0x0194	*/
			RegWriteA( WC_MES1ADD1,  0x00 ) ;			/* 0x0195	*/
			RegWriteA( WC_MES2ADD0,  0x00 ) ;			/* 0x0196	*/
			RegWriteA( WC_MES2ADD1,  0x00 ) ;			/* 0x0197	*/
			
		}
#endif
		RegWriteA( WC_SINON,     0x00 ) ;			/* 0x0180	Sine wave  */
		
	}
	else
	{
		
		RegReadA( WH_EQSWX, &UcEqSwX ) ;				/* 0x0170	*/
		RegReadA( WH_EQSWY, &UcEqSwY ) ;				/* 0x0171	*/
		
		if(( UcMethodVal == CIRCWAVE ) || ( UcMethodVal == SINEWAVE )) {
#ifdef	USE_SINLPF
			RegWriteA( WC_DPI1ADD0,  ( unsigned char )MES1BZ2 ) ;						/* 0x01B0	input Meas-Fil */
			RegWriteA( WC_DPI1ADD1,  ( unsigned char )(( MES1BZ2 >> 8 ) & 0x0001 ) ) ;	/* 0x01B1	input Meas-Fil */
			RegWriteA( WC_DPI2ADD0,  ( unsigned char )MES2BZ2 ) ;						/* 0x01B2	input Meas-Fil */
			RegWriteA( WC_DPI2ADD1,  ( unsigned char )(( MES2BZ2 >> 8 ) & 0x0001 ) ) ;	/* 0x01B3	input Meas-Fil */
			RegWriteA( WC_DPO1ADD0, ( unsigned char )SXOFFZ1 ) ;						/* 0x01B8	output SXOFFZ1 */
			RegWriteA( WC_DPO1ADD1, ( unsigned char )(( SXOFFZ1 >> 8 ) & 0x0001 ) ) ;	/* 0x01B9	output SXOFFZ1 */
			RegWriteA( WC_DPO2ADD0, ( unsigned char )SYOFFZ1 ) ;						/* 0x01BA	output SYOFFZ1 */
			RegWriteA( WC_DPO2ADD1, ( unsigned char )(( SYOFFZ1 >> 8 ) & 0x0001 ) ) ;	/* 0x01BA	output SYOFFZ1 */
			
			RegWriteA( WC_MES1ADD0,  ( unsigned char )SINXZ ) ;							/* 0x0194	*/
			RegWriteA( WC_MES1ADD1,  ( unsigned char )(( SINXZ >> 8 ) & 0x0001 ) ) ;	/* 0x0195	*/
			RegWriteA( WC_MES2ADD0,  ( unsigned char )SINYZ ) ;							/* 0x0196	*/
			RegWriteA( WC_MES2ADD1,  ( unsigned char )(( SINYZ >> 8 ) & 0x0001 ) ) ;	/* 0x0197	*/
			
			RegWriteA( WC_DPON,     0x03 ) ;			/* 0x0105	Data pass[1:0] on */
			
			UcEqSwX &= ~EQSINSW ;
			UcEqSwY &= ~EQSINSW ;
#else
			UcEqSwX |= 0x08 ;
			UcEqSwY |= 0x08 ;
#endif
		} else if(( UcMethodVal == XACTTEST ) || ( UcMethodVal == YACTTEST )) {
			RegWriteA( WC_DPI2ADD0,  ( unsigned char )MES2BZ2 ) ;						/* 0x01B2	input Meas-Fil */
			RegWriteA( WC_DPI2ADD1,  ( unsigned char )(( MES2BZ2 >> 8 ) & 0x0001 ) ) ;	/* 0x01B3	input Meas-Fil */
			if( UcMethodVal == XACTTEST ){
				RegWriteA( WC_DPO2ADD0, ( unsigned char )SXOFFZ1 ) ;						/* 0x01BA	output SXOFFZ1 */
				RegWriteA( WC_DPO2ADD1, ( unsigned char )(( SXOFFZ1 >> 8 ) & 0x0001 ) ) ;	/* 0x01BB	output SXOFFZ1 */
				RegWriteA( WC_MES2ADD0,  ( unsigned char )SINXZ ) ;							/* 0x0196	*/
				RegWriteA( WC_MES2ADD1,  ( unsigned char )(( SINXZ >> 8 ) & 0x0001 ) ) ;	/* 0x0197	*/
			} else {
				RegWriteA( WC_DPO2ADD0, ( unsigned char )SYOFFZ1 ) ;						/* 0x01BA	output SYOFFZ1 */
				RegWriteA( WC_DPO2ADD1, ( unsigned char )(( SYOFFZ1 >> 8 ) & 0x0001 ) ) ;	/* 0x01BB	output SYOFFZ1 */
				RegWriteA( WC_MES2ADD0,  ( unsigned char )SINYZ ) ;							/* 0x0196	*/
				RegWriteA( WC_MES2ADD1,  ( unsigned char )(( SINYZ >> 8 ) & 0x0001 ) ) ;	/* 0x0197	*/
			}
			
			RegWriteA( WC_DPON,     0x02 ) ;			/* 0x0105	Data pass[1] on */
			
			UcEqSwX &= ~EQSINSW ;
			UcEqSwY &= ~EQSINSW ;

		}else{
			if( UcMethodVal == XHALWAVE ){
		    	UcEqSwX = 0x22 ;				/* SW[5] */
//		    	UcEqSwY = 0x03 ;
			}else{
//				UcEqSwX = 0x03 ;
				UcEqSwY = 0x22 ;				/* SW[5] */
			}
		}
		
		RegWriteA( WC_SINFRQ0,	(unsigned char)UsFreqDat ) ;				// 0x0181		Freq L
		RegWriteA( WC_SINFRQ1,	(unsigned char)(UsFreqDat >> 8) ) ;			// 0x0182		Freq H
		RegWriteA( WC_MESSINMODE,     0x00 ) ;			/* 0x0191	Sine 0 cross  */

		RegWriteA( WH_EQSWX, UcEqSwX ) ;				/* 0x0170	*/
		RegWriteA( WH_EQSWY, UcEqSwY ) ;				/* 0x0171	*/

		RegWriteA( WC_SINON,     0x01 ) ;			/* 0x0180	Sine wave  */
		
	}
	
	
}




#ifdef STANDBY_MODE
//********************************************************************************
// Function Name 	: SetStandby
// Retun Value		: NON
// Argment Value	: 0:Standby ON 1:Standby OFF 2:Standby2 ON 3:Standby2 OFF 
//					: 4:Standby3 ON 5:Standby3 OFF
// Explanation		: Set Standby
// History			: First edition 						2013.01.15 Y.Shigeoka
//********************************************************************************
void	SetStandby( unsigned char UcContMode )
{
	unsigned char	UcStbb0 , UcClkon ;
	
	switch(UcContMode)
	{
	case STB1_ON:

		RegWriteA( DRVFCAF	, 0x00 );				// 0x0081	Drv.MODEAF=0,Drv.ENAAF=0,MODE-0
		RegWriteA( STBB0 	, 0x00 );		// 0x0250 	[ STBAFDRV | STBOISDRV | STBOPAAF | STBOPAY ][ STBOPAX | STBDACI | STBDACV | STBADC ]
		RegWriteA( STBB1 	, 0x00 );		// 0x0264 	[ - | - | - | - ][ - | STBAFOP1 | - | STBAFDAC ]
		RegWriteA( PWMA 	, 0x00 );		// 0x0010		PWM Standby
		RegWriteA( PWMAAF 	, 0x00 );		// 0x0090		AF PWM Standby
		RegWriteA( CVA,  0x00 ) ;			/* 0x0020	LINEAR PWM mode standby	*/
		DrvSw( OFF ) ;						/* Driver OFF */
		AfDrvSw( OFF ) ;					/* AF Driver OFF */
#ifdef	MONITOR_OFF
#else
		RegWriteA( PWMMONA, 0x00 ) ;		// 0x0030	Monitor Standby
#endif
//		RegWriteA( DACMONFC, 0x01 ) ;		// 0x0032	DAC Monitor Standby
		SelectGySleep( ON ) ;				/* Gyro Sleep */
		break ;
	case STB1_OFF:
		SelectGySleep( OFF ) ;				/* Gyro Wake Up */
//		RegWriteA( DACMONFC, 0x81 ) ;		// 0x0032	DAC Monitor Active
		RegWriteA( PWMMONA, 0x80 ) ;		/* 0x0030	Monitor Active	*/
		DrvSw( ON ) ;						/* Driver Mode setting */
		AfDrvSw( ON ) ;						/* AF Driver Mode setting */
		RegWriteA( CVA		, 0xC0 );		// 0x0020	Linear PWM mode enable
		RegWriteA( PWMAAF 	, 0x00 );		// 0x0090		AF PWM Standby
		RegWriteA( PWMA		, 0xC0 );		// 0x0010	PWM enable
		RegWriteA( STBB1	, 0x05 ) ;		// 0x0264	[ - | - | - | - ][ - | STBAFOP1 | - | STBAFDAC ]
		RegWriteA( STBB0	, 0xDF );		// 0x0250 	[ STBAFDRV | STBOISDRV | STBOPAAF | STBOPAY ][ STBOPAX | STBDACI | STBDACV | STBADC ]
		break ;
	case STB2_ON:
		RegWriteA( DRVFCAF	, 0x00 );				// 0x0081	Drv.MODEAF=0,Drv.ENAAF=0,MODE-0
		RegWriteA( STBB0 	, 0x00 );		// 0x0250 	[ STBAFDRV | STBOISDRV | STBOPAAF | STBOPAY ][ STBOPAX | STBDACI | STBDACV | STBADC ]
		RegWriteA( STBB1 	, 0x00 );		// 0x0264 	[ - | - | - | - ][ - | STBAFOP1 | - | STBAFDAC ]
		RegWriteA( PWMA 	, 0x00 );		// 0x0010		PWM Standby
		RegWriteA( PWMAAF 	, 0x00 );		// 0x0090		AF PWM Standby
		RegWriteA( CVA,  0x00 ) ;			/* 0x0020	LINEAR PWM mode standby	*/
		DrvSw( OFF ) ;						/* Drvier Block Ena=0 */
		AfDrvSw( OFF ) ;					/* AF Drvier Block Ena=0 */
#ifdef	MONITOR_OFF
#else
		RegWriteA( PWMMONA, 0x00 ) ;		// 0x0030	Monitor Standby
#endif
//		RegWriteA( DACMONFC, 0x01 ) ;		// 0x0032	DAC Monitor Standby
		SelectGySleep( ON ) ;				/* Gyro Sleep */
		RegWriteA( CLKON, 0x00 ) ;			/* 0x020B	Servo & PWM Clock OFF + D-Gyro I/F OFF	*/
		break ;
	case STB2_OFF:
		RegWriteA( CLKON,	0x1F ) ;		// 0x020B	[ - | - | CmOpafClkOn | CmAfpwmClkOn | CMGifClkOn  | CmScmClkOn  | CmSrvClkOn  | CmPwmClkOn  ]
		SelectGySleep( OFF ) ;				/* Gyro Wake Up */
//		RegWriteA( DACMONFC, 0x81 ) ;		// 0x0032	DAC Monitor Active
		RegWriteA( PWMMONA, 0x80 ) ;		/* 0x0030	Monitor Active	*/
		DrvSw( ON ) ;						/* Driver Mode setting */
		AfDrvSw( ON ) ;						/* AF Driver Mode setting */
		RegWriteA( CVA, 	0xC0 );			// 0x0020	Linear PWM mode enable
		RegWriteA( PWMAAF 	, 0x00 );		// 0x0090		AF PWM Standby
		RegWriteA( PWMA	, 	0xC0 );			// 0x0010	PWM enable
		RegWriteA( STBB1	, 0x05 ) ;		// 0x0264	[ - | - | - | - ][ - | STBAFOP1 | - | STBAFDAC ]
		RegWriteA( STBB0	, 0xDF );		// 0x0250 	[ STBAFDRV | STBOISDRV | STBOPAAF | STBOPAY ][ STBOPAX | STBDACI | STBDACV | STBADC ]
		break ;
	case STB3_ON:
		RegWriteA( DRVFCAF	, 0x00 );				// 0x0081	Drv.MODEAF=0,Drv.ENAAF=0,MODE-0
		RegWriteA( STBB0 	, 0x00 );		// 0x0250 	[ STBAFDRV | STBOISDRV | STBOPAAF | STBOPAY ][ STBOPAX | STBDACI | STBDACV | STBADC ]
		RegWriteA( STBB1 	, 0x00 );		// 0x0264 	[ - | - | - | - ][ - | STBAFOP1 | - | STBAFDAC ]
		RegWriteA( PWMA 	, 0x00 );			// 0x0010		PWM Standby
		RegWriteA( PWMAAF 	, 0x00 );		// 0x0090		AF PWM Standby
		RegWriteA( CVA,  0x00 ) ;			/* 0x0020	LINEAR PWM mode standby	*/
		DrvSw( OFF ) ;						/* Drvier Block Ena=0 */
		AfDrvSw( OFF ) ;					/* AF Drvier Block Ena=0 */
#ifdef	MONITOR_OFF
#else
		RegWriteA( PWMMONA, 0x00 ) ;		// 0x0030	Monitor Standby
#endif
//		RegWriteA( DACMONFC, 0x01 ) ;		// 0x0032	DAC Monitor Standby
		SelectGySleep( ON ) ;				/* Gyro Sleep */
		RegWriteA( CLKON, 0x00 ) ;			/* 0x020B	Servo & PWM Clock OFF + D-Gyro I/F OFF	*/
		RegWriteA( I2CSEL, 0x01 ) ;			/* 0x0248	I2C Noise Cancel circuit OFF	*/
		RegWriteA( OSCSTOP, 0x02 ) ;		// 0x0256	Source Clock Input OFF
		break ;
	case STB3_OFF:
		RegWriteA( OSCSTOP, 0x00 ) ;		// 0x0256	Source Clock Input ON
		RegWriteA( I2CSEL, 0x00 ) ;			/* 0x0248	I2C Noise Cancel circuit ON	*/
		RegWriteA( CLKON,	0x1F ) ;		// 0x020B	[ - | - | - | - | CMGifClkOn  | CmScmClkOn  | CmSrvClkOn  | CmPwmClkOn  ]
		SelectGySleep( OFF ) ;				/* Gyro Wake Up */
//		RegWriteA( DACMONFC, 0x81 ) ;		// 0x0032	DAC Monitor Active
		RegWriteA( PWMMONA, 0x80 ) ;		/* 0x0030	Monitor Active	*/
		DrvSw( ON ) ;						/* Driver Mode setting */
		AfDrvSw( ON ) ;						/* AF Driver Mode setting */
		RegWriteA( CVA, 	0xC0 );			// 0x0020	Linear PWM mode enable
		RegWriteA( PWMAAF,	0x00 );			// 0x0090		AF PWM Standby
		RegWriteA( PWMA	, 	0xC0 );			// 0x0010	PWM enable
		RegWriteA( STBB1	, 0x05 ) ;		// 0x0264	[ - | - | - | - ][ - | STBAFOP1 | - | STBAFDAC ]
		RegWriteA( STBB0	, 0xDF );		// 0x0250 	[ STBAFDRV | STBOISDRV | STBOPAAF | STBOPAY ][ STBOPAX | STBDACI | STBDACV | STBADC ]
		break ;
		
	case STB4_ON:
		RegWriteA( DRVFCAF	, 0x00 );				// 0x0081	Drv.MODEAF=0,Drv.ENAAF=0,MODE-0
		RegWriteA( STBB0 	, 0x00 );		// 0x0250 	[ STBAFDRV | STBOISDRV | STBOPAAF | STBOPAY ][ STBOPAX | STBDACI | STBDACV | STBADC ]
		RegWriteA( STBB1 	, 0x00 );		// 0x0264 	[ - | - | - | - ][ - | STBAFOP1 | - | STBAFDAC ]
		RegWriteA( PWMA 	, 0x00 );		// 0x0010		PWM Standby
		RegWriteA( PWMAAF 	, 0x00 );		// 0x0090		AF PWM Standby
		RegWriteA( CVA,  	0x00 ) ;		/* 0x0020	LINEAR PWM mode standby	*/
		DrvSw( OFF ) ;						/* Drvier Block Ena=0 */
		AfDrvSw( OFF ) ;					/* AF Drvier Block Ena=0 */
#ifdef	MONITOR_OFF
#else
		RegWriteA( PWMMONA, 0x00 ) ;		// 0x0030	Monitor Standby
#endif
//		RegWriteA( DACMONFC, 0x01 ) ;		// 0x0032	DAC Monitor Standby
		GyOutSignalCont( ) ;				/* Gyro Continuos mode */
		RegWriteA( CLKON, 0x04 ) ;			/* 0x020B	Servo & PWM Clock OFF + D-Gyro I/F ON	*/
		break ;
	case STB4_OFF:
		RegWriteA( CLKON,	0x1F ) ;		// 0x020B	[ - | - | - | - | CMGifClkOn  | CmScmClkOn  | CmSrvClkOn  | CmPwmClkOn  ]
		SelectGySleep( OFF ) ;				/* Gyro OIS mode */
//		RegWriteA( DACMONFC, 0x81 ) ;		// 0x0032	DAC Monitor Active
		RegWriteA( PWMMONA, 0x80 ) ;		/* 0x0030	Monitor Active	*/
		DrvSw( ON ) ;						/* Driver Mode setting */
		AfDrvSw( ON ) ;						/* AF Driver Mode setting */
		RegWriteA( CVA, 	0xC0 );			// 0x0020	Linear PWM mode enable
		RegWriteA( PWMAAF, 	0x00 );			// 0x0090		AF PWM Standby
		RegWriteA( PWMA	, 	0xC0 );			// 0x0010	PWM enable
		RegWriteA( STBB1	, 0x05 ) ;		// 0x0264	[ - | - | - | - ][ - | STBAFOP1 | - | STBAFDAC ]
		RegWriteA( STBB0	, 0xDF );		// 0x0250 	[ STBAFDRV | STBOISDRV | STBOPAAF | STBOPAY ][ STBOPAX | STBDACI | STBDACV | STBADC ]
		break ;
		
		/************** special mode ************/
	case STB2_OISON:
		RegReadA( STBB0 	, &UcStbb0 );		// 0x0250 	[ STBAFDRV | STBOISDRV | STBOPAAF | STBOPAY ][ STBOPAX | STBDACI | STBDACV | STBADC ]
		UcStbb0 &= 0x80 ;
		RegWriteA( STBB0 	, UcStbb0 );		// 0x0250 	[ STBAFDRV | STBOISDRV | STBOPAAF | STBOPAY ][ STBOPAX | STBDACI | STBDACV | STBADC ]
		RegWriteA( PWMA 	, 0x00 );		// 0x0010		PWM Standby
		RegWriteA( CVA,  0x00 ) ;			/* 0x0020	LINEAR PWM mode standby	*/
		DrvSw( OFF ) ;						/* Drvier Block Ena=0 */
#ifdef	MONITOR_OFF
#else
		RegWriteA( PWMMONA, 0x00 ) ;		// 0x0030	Monitor Standby
#endif
//		RegWriteA( DACMONFC, 0x01 ) ;		// 0x0032	DAC Monitor Standby
		SelectGySleep( ON ) ;				/* Gyro Sleep */
		RegReadA( CLKON, &UcClkon ) ;		/* 0x020B	PWM Clock OFF + D-Gyro I/F OFF	SRVCLK can't OFF */
		UcClkon &= 0x1A ;
		RegWriteA( CLKON, UcClkon ) ;		/* 0x020B	PWM Clock OFF + D-Gyro I/F OFF	SRVCLK can't OFF */
		break ;
	case STB2_OISOFF:
		RegReadA( CLKON, &UcClkon ) ;		/* 0x020B	PWM Clock OFF + D-Gyro I/F ON  */
		UcClkon |= 0x05 ;
		RegWriteA( CLKON,	UcClkon ) ;		// 0x020B	[ - | - | CmOpafClkOn | CmAfpwmClkOn | CMGifClkOn  | CmScmClkOn  | CmSrvClkOn  | CmPwmClkOn  ]
		SelectGySleep( OFF ) ;				/* Gyro Wake Up */
//		RegWriteA( DACMONFC, 0x81 ) ;		// 0x0032	DAC Monitor Active
		RegWriteA( PWMMONA, 0x80 ) ;		/* 0x0030	Monitor Active	*/
		DrvSw( ON ) ;						/* Driver Mode setting */
		RegWriteA( CVA, 	0xC0 );			// 0x0020	Linear PWM mode enable
		RegWriteA( PWMA	, 	0xC0 );			// 0x0010	PWM enable
		RegReadA( STBB0	, &UcStbb0 );		// 0x0250 	[ STBAFDRV | STBOISDRV | STBOPAAF | STBOPAY ][ STBOPAX | STBDACI | STBDACV | STBADC ]
		UcStbb0 |= 0x5F ;
		RegWriteA( STBB0	, UcStbb0 );	// 0x0250 	[ STBAFDRV | STBOISDRV | STBOPAAF | STBOPAY ][ STBOPAX | STBDACI | STBDACV | STBADC ]
		break ;
		
	case STB2_AFON:
		RegWriteA( DRVFCAF	, 0x00 );				// 0x0081	Drv.MODEAF=0,Drv.ENAAF=0,MODE-0
		RegReadA( STBB0 	, &UcStbb0 );		// 0x0250 	[ STBAFDRV | STBOISDRV | STBOPAAF | STBOPAY ][ STBOPAX | STBDACI | STBDACV | STBADC ]
		UcStbb0 &= 0x7F ;
		RegWriteA( STBB0 	, UcStbb0 );		// 0x0250 	[ STBAFDRV | STBOISDRV | STBOPAAF | STBOPAY ][ STBOPAX | STBDACI | STBDACV | STBADC ]
		RegWriteA( STBB1 	, 0x00 );		// 0x0264 	[ - | - | - | - ][ - | STBAFOP1 | - | STBAFDAC ]
		RegWriteA( PWMAAF 	, 0x00 );		// 0x0090		AF PWM Standby
		AfDrvSw( OFF ) ;					/* AF Drvier Block Ena=0 */
#ifdef	MONITOR_OFF
#else
		RegWriteA( PWMMONA, 0x00 ) ;		// 0x0030	Monitor Standby
#endif
		RegReadA( CLKON, &UcClkon ) ;		/* 0x020B	OPAF Clock OFF + AFPWM OFF	SRVCLK can't OFF	*/
		UcClkon &= 0x07 ;
		RegWriteA( CLKON, UcClkon ) ;		/* 0x020B	OPAF Clock OFF + AFPWM OFF	SRVCLK can't OFF	*/
		break ;
	case STB2_AFOFF:
		RegReadA( CLKON, &UcClkon ) ;		/* 0x020B	OPAF Clock ON + AFPWM ON  */
		UcClkon |= 0x18 ;
		RegWriteA( CLKON,	UcClkon ) ;		// 0x020B	[ - | - | CmOpafClkOn | CmAfpwmClkOn | CMGifClkOn  | CmScmClkOn  | CmSrvClkOn  | CmPwmClkOn  ]
		AfDrvSw( ON ) ;						/* AF Driver Mode setting */
		RegWriteA( PWMAAF 	, 0x00 );		// 0x0090		AF PWM Standby
		RegWriteA( STBB1	, 0x05 ) ;		// 0x0264	[ - | - | - | - ][ - | STBAFOP1 | - | STBAFDAC ]
		RegReadA( STBB0	, &UcStbb0 );		// 0x0250 	[ STBAFDRV | STBOISDRV | STBOPAAF | STBOPAY ][ STBOPAX | STBDACI | STBDACV | STBADC ]
		UcStbb0 |= 0x80 ;
		RegWriteA( STBB0	, UcStbb0 );	// 0x0250 	[ STBAFDRV | STBOISDRV | STBOPAAF | STBOPAY ][ STBOPAX | STBDACI | STBDACV | STBADC ]
		break ;
		/************** special mode ************/
	}
}
#endif

//********************************************************************************
// Function Name 	: SetZsp
// Retun Value		: NON
// Argment Value	: Command Parameter
// Explanation		: Set Zoom Step parameter Function
// History			: First edition 						2013.01.15 Y.Shigeoka
//********************************************************************************
void	SetZsp( unsigned char	UcZoomStepDat )
{
	unsigned long	UlGyrZmx, UlGyrZmy, UlGyrZrx, UlGyrZry ;

	
	/* Zoom Step */
	if(UcZoomStepDat > (ZOOMTBL - 1))
		UcZoomStepDat = (ZOOMTBL -1) ;										/* 上限をZOOMTBL-1に設定する */

	if( UcZoomStepDat == 0 )				/* initial setting	*/
	{
		UlGyrZmx	= ClGyxZom[ 0 ] ;		// Same Wide Coefficient
		UlGyrZmy	= ClGyyZom[ 0 ] ;		// Same Wide Coefficient
		/* Initial Rate value = 1 */
	}
	else
	{
		UlGyrZmx	= ClGyxZom[ UcZoomStepDat ] ;
		UlGyrZmy	= ClGyyZom[ UcZoomStepDat ] ;
		
		
	}
	
	// Zoom Value Setting
	RamWrite32A( gxlens, UlGyrZmx ) ;		/* 0x1022 */
	RamWrite32A( gylens, UlGyrZmy ) ;		/* 0x1122 */

	RamRead32A( gxlens, &UlGyrZrx ) ;		/* 0x1022 */
	RamRead32A( gylens, &UlGyrZry ) ;		/* 0x1122 */

	// Zoom Value Setting Error Check
	if( UlGyrZmx != UlGyrZrx ) {
		RamWrite32A( gxlens, UlGyrZmx ) ;		/* 0x1022 */
	}

	if( UlGyrZmy != UlGyrZry ) {
		RamWrite32A( gylens, UlGyrZmy ) ;		/* 0x1122 */
	}

}

//********************************************************************************
// Function Name 	: StbOnn
// Retun Value		: NON
// Argment Value	: NON
// Explanation		: Stabilizer For Servo On Function
// History			: First edition 						2013.01.09 Y.Shigeoka
//********************************************************************************
 
void StbOnn( void )
{
	unsigned char	UcRegValx,UcRegValy;					// Registor value 
	unsigned char	UcRegIni ;
	unsigned char	UcRegIniCnt = 0;
	
	RegReadA( WH_EQSWX , &UcRegValx ) ;			// 0x0170
	RegReadA( WH_EQSWY , &UcRegValy ) ;			// 0x0171
	
	if( (( UcRegValx & 0x01 ) != 0x01 ) && (( UcRegValy & 0x01 ) != 0x01 ))
	{
		
		RegWriteA( WH_SMTSRVON,	0x01 ) ;				// 0x017C		Smooth Servo ON
		
		SrvCon( X_DIR, ON ) ;
		SrvCon( Y_DIR, ON ) ;
		
		UcRegIni = 0x11;
		while( (UcRegIni & 0x77) != 0x66 )
		{
			RegReadA( RH_SMTSRVSTT,	&UcRegIni ) ;		// 0x01F8		Smooth Servo phase read
			
			if( CmdRdChk() !=0 )	break;				// Dead Lock check (responce check)
			if((UcRegIni & 0x77 ) == 0 )	UcRegIniCnt++ ;
			if( UcRegIniCnt > 10 ){
				break ;			// Status Error
			}
			
		}
		RegWriteA( WH_SMTSRVON,	0x00 ) ;				// 0x017C		Smooth Servo OFF
		
	}
	else
	{
		SrvCon( X_DIR, ON ) ;
		SrvCon( Y_DIR, ON ) ;
	}
}

//********************************************************************************
// Function Name 	: StbOnnN
// Retun Value		: NON
// Argment Value	: NON
// Explanation		: Stabilizer For Servo On Function
// History			: First edition 						2013.10.09 Y.Shigeoka
//********************************************************************************
 
void StbOnnN( unsigned char UcStbY , unsigned char UcStbX )
{
	unsigned char	UcRegIni ;
	unsigned char	UcSttMsk = 0 ;
	unsigned char	UcRegIniCnt = 0;
	
	
	RegWriteA( WH_SMTSRVON,	0x01 ) ;				// 0x017C		Smooth Servo ON
	if( UcStbX == ON )	UcSttMsk |= 0x07 ;
	if( UcStbY == ON )	UcSttMsk |= 0x70 ;
	
	SrvCon( X_DIR, UcStbX ) ;
	SrvCon( Y_DIR, UcStbY ) ;
	
	UcRegIni = 0x11;
	while( (UcRegIni & UcSttMsk) != ( 0x66 & UcSttMsk ) )
	{
		RegReadA( RH_SMTSRVSTT,	&UcRegIni ) ;		// 0x01F8		Smooth Servo phase read
		
		if( CmdRdChk() !=0 )	break;				// Dead Lock check (responce check)
		if((UcRegIni & 0x77 ) == 0 )	UcRegIniCnt++ ;
		if( UcRegIniCnt > 10 ){
			break ;			// Status Error
		}
		
	}
	RegWriteA( WH_SMTSRVON,	0x00 ) ;				// 0x017C		Smooth Servo OFF
		
}

//********************************************************************************
// Function Name 	: OptCen
// Retun Value		: NON
// Argment Value	: UcOptMode 0:Set 1:Save&Set
//					: UsOptXval Xaxis offset
//					: UsOptYval Yaxis offset
// Explanation		: Send Optical Center
// History			: First edition 						2013.01.15 Y.Shigeoka
//********************************************************************************
void	OptCen( unsigned char UcOptmode , unsigned short UsOptXval , unsigned short UsOptYval )
{
	RamAccFixMod( ON ) ;							// Fix mode
	
	switch ( UcOptmode ) {
		case VAL_SET :
			RamWriteA( SXOFFZ1   , UsOptXval ) ;		/* 0x1461	Check Hall X optical center */
			RamWriteA( SYOFFZ1   , UsOptYval ) ;		/* 0x14E1	Check Hall Y optical center */
			break ;
		case VAL_FIX :
			UsCntXof = UsOptXval ;
			UsCntYof = UsOptYval ;
			RamWriteA( SXOFFZ1   , UsCntXof ) ;		/* 0x1461	Check Hall X optical center */
			RamWriteA( SYOFFZ1   , UsCntYof ) ;		/* 0x14E1	Check Hall Y optical center */

			break ;
		case VAL_SPC :
			RamReadA( SXOFFZ1   , &UsOptXval ) ;		/* 0x1461	Check Hall X optical center */
			RamReadA( SYOFFZ1   , &UsOptYval ) ;		/* 0x14E1	Check Hall Y optical center */
			UsCntXof = UsOptXval ;
			UsCntYof = UsOptYval ;


			break ;
	}

	RamAccFixMod( OFF ) ;							// Float mode
	
}






//********************************************************************************
// Function Name 	: GyrGan
// Retun Value		: NON
// Argment Value	: UcGygmode 0:Set 1:Save&Set
//					: UlGygXval Xaxis Gain
//					: UlGygYval Yaxis Gain
// Explanation		: Send Gyro Gain
// History			: First edition 						2011.02.09 Y.Shigeoka
//********************************************************************************
void	GyrGan( unsigned char UcGygmode , unsigned long UlGygXval , unsigned long UlGygYval )
{
	switch ( UcGygmode ) {
		case VAL_SET :
			RamWrite32A( gxzoom, UlGygXval ) ;		// 0x1020
			RamWrite32A( gyzoom, UlGygYval ) ;		// 0x1120
			break ;
		case VAL_FIX :
			RamWrite32A( gxzoom, UlGygXval ) ;		// 0x1020
			RamWrite32A( gyzoom, UlGygYval ) ;		// 0x1120

			break ;
		case VAL_SPC :
			RamRead32A( gxzoom, &UlGygXval ) ;		// 0x1020
			RamRead32A( gyzoom, &UlGygYval ) ;		// 0x1120
		
			break ;
	}

}

//********************************************************************************
// Function Name 	: SetPanTiltMode
// Retun Value		: NON
// Argment Value	: NON
// Explanation		: Pan-Tilt Enable/Disable
// History			: First edition 						2013.01.09 Y.Shigeoka
//********************************************************************************
void	SetPanTiltMode( unsigned char UcPnTmod )
{
	switch ( UcPnTmod ) {
		case OFF :
			RegWriteA( WG_PANON, 0x00 ) ;			// 0x0109	X,Y Pan/Tilt Function OFF
			break ;
		case ON :
			RegWriteA( WG_PANON, 0x01 ) ;			// 0x0109	X,Y Pan/Tilt Function ON
//			RegWriteA( WG_PANON, 0x10 ) ;			// 0x0109	X,Y New Pan/Tilt Function ON
			break ;
	}

}


#ifdef GAIN_CONT
//********************************************************************************
// Function Name 	: TriSts
// Retun Value		: Tripod Status
//					: bit0( 1:Y Tripod ON / 0:OFF)
//					: bit4( 1:X Tripod ON / 0:OFF)
//					: bit7( 1:Tripod ENABLE  / 0:DISABLE)
// Argment Value	: NON
// Explanation		: Read Status of Tripod mode Function
// History			: First edition 						2013.02.18 Y.Shigeoka
//********************************************************************************
unsigned char	TriSts( void )
{
	unsigned char UcRsltSts = 0;
	unsigned char UcVal ;

	RegReadA( WG_ADJGANGXATO, &UcVal ) ;	// 0x0129
	if( UcVal & 0x03 ){						// Gain control enable?
		RegReadA( RG_LEVJUGE, &UcVal ) ;	// 0x01F4
		UcRsltSts = UcVal & 0x11 ;		// bit0, bit4 set
		UcRsltSts |= 0x80 ;				// bit7 ON
	}
	return( UcRsltSts ) ;
}
#endif

//********************************************************************************
// Function Name 	: DrvPwmSw
// Retun Value		: Mode Status
//					: bit4( 1:PWM / 0:LinearPwm)
// Argment Value	: NON
// Explanation		: Select Driver mode Function
// History			: First edition 						2013.02.18 Y.Shigeoka
//********************************************************************************
unsigned char	DrvPwmSw( unsigned char UcSelPwmMod )
{

	switch ( UcSelPwmMod ) {
		case Mlnp :
			RegWriteA( DRVFC	, 0xF0 );			// 0x0001	Drv.MODE=1,Drv.BLK=1,MODE2,LCEN
			UcPwmMod = PWMMOD_CVL ;
			break ;
		
		case Mpwm :
			RegWriteA( DRVFC	, 0x00 );			// 0x0001	Drv.MODE=0,Drv.BLK=0,MODE0B
			UcPwmMod = PWMMOD_PWM ;
 			break ;
	}
	
	return( UcSelPwmMod << 4 ) ;
}

//********************************************************************************
// Function Name 	: SetGcf
// Retun Value		: NON
// Argment Value	: Command Parameter
// Explanation		: Set DI filter coefficient Function
// History			: First edition 						2013.03.22 Y.Shigeoka
//********************************************************************************
void	SetGcf( unsigned char	UcSetNum )
{
	
	/* Zoom Step */
	if(UcSetNum > (COEFTBL - 1))
		UcSetNum = (COEFTBL -1) ;			/* 上限をCOEFTBL-1に設定する */

	UlH1Coefval	= ClDiCof[ UcSetNum ] ;
		
	// Zoom Value Setting
	RamWrite32A( gxh1c, UlH1Coefval ) ;		/* 0x1012 */
	RamWrite32A( gyh1c, UlH1Coefval ) ;		/* 0x1112 */

#ifdef H1COEF_CHANGER
		SetH1cMod( UcSetNum ) ;							/* Re-setting */
#endif

}

#ifdef H1COEF_CHANGER
//********************************************************************************
// Function Name 	: SetH1cMod
// Retun Value		: NON
// Argment Value	: Command Parameter
// Explanation		: Set H1C coefficient Level chang Function
// History			: First edition 						2013.04.18 Y.Shigeoka
//********************************************************************************
void	SetH1cMod( unsigned char	UcSetNum )
{
	
	switch( UcSetNum ){
	case ( ACTMODE ):				// initial 
		IniPtMovMod( OFF ) ;							// Pan/Tilt setting (Still)
		
		/* enable setting */
		/* Zoom Step */
		UlH1Coefval	= ClDiCof[ 0 ] ;
			
		UcH1LvlMod = 0 ;
		
		// Limit value Value Setting
		RamWrite32A( gxlmt6L, MINLMT ) ;		/* 0x102D L-Limit */
		RamWrite32A( gxlmt6H, MAXLMT ) ;		/* 0x102E H-Limit */

		RamWrite32A( gylmt6L, MINLMT ) ;		/* 0x112D L-Limit */
		RamWrite32A( gylmt6H, MAXLMT ) ;		/* 0x112E H-Limit */

		RamWrite32A( gxhc_tmp, 	UlH1Coefval ) ;	/* 0x100E Base Coef */
		RamWrite32A( gxmg, 		CHGCOEF ) ;		/* 0x10AA Change coefficient gain */

		RamWrite32A( gyhc_tmp, 	UlH1Coefval ) ;	/* 0x110E Base Coef */
		RamWrite32A( gymg, 		CHGCOEF ) ;		/* 0x11AA Change coefficient gain */
		
		RegWriteA( WG_HCHR, 0x12 ) ;			// 0x011B	GmHChrOn[1]=1 Sw ON
		break ;
		
	case( S2MODE ):				// cancel lvl change mode 
		RegWriteA( WG_HCHR, 0x10 ) ;			// 0x011B	GmHChrOn[1]=0 Sw OFF
		break ;
		
	case( MOVMODE ):			// Movie mode 
		IniPtMovMod( ON ) ;							// Pan/Tilt setting (Movie)
		
		RamWrite32A( gxlmt6L, MINLMT_MOV ) ;	/* 0x102D L-Limit */
		RamWrite32A( gylmt6L, MINLMT_MOV ) ;	/* 0x112D L-Limit */

		RamWrite32A( gxmg, CHGCOEF_MOV ) ;		/* 0x10AA Change coefficient gain */
		RamWrite32A( gymg, CHGCOEF_MOV ) ;		/* 0x11AA Change coefficient gain */
			
		RamWrite32A( gxhc_tmp, UlH1Coefval ) ;		/* 0x100E Base Coef */
		RamWrite32A( gyhc_tmp, UlH1Coefval ) ;		/* 0x110E Base Coef */
		
		RegWriteA( WG_HCHR, 0x12 ) ;			// 0x011B	GmHChrOn[1]=1 Sw ON
		break ;
		
	default :
		IniPtMovMod( OFF ) ;							// Pan/Tilt setting (Still)
		
		UcH1LvlMod = UcSetNum ;
			
		RamWrite32A( gxlmt6L, MINLMT ) ;		/* 0x102D L-Limit */
		RamWrite32A( gylmt6L, MINLMT ) ;		/* 0x112D L-Limit */
		
		RamWrite32A( gxmg, 	CHGCOEF ) ;			/* 0x10AA Change coefficient gain */
		RamWrite32A( gymg, 	CHGCOEF ) ;			/* 0x11AA Change coefficient gain */
			
		RamWrite32A( gxhc_tmp, UlH1Coefval ) ;		/* 0x100E Base Coef */
		RamWrite32A( gyhc_tmp, UlH1Coefval ) ;		/* 0x110E Base Coef */
		
		RegWriteA( WG_HCHR, 0x12 ) ;			// 0x011B	GmHChrOn[1]=1 Sw ON
		break ;
	}
}
#endif

//********************************************************************************
// Function Name 	: RdFwVr
// Retun Value		: Firmware version
// Argment Value	: NON
// Explanation		: Read Fw Version Function
// History			: First edition 						2013.05.07 Y.Shigeoka
//********************************************************************************
unsigned short	RdFwVr( void )
{
	unsigned short	UsVerVal ;
	
	UsVerVal = (unsigned short)((MDL_VER << 8) | FW_VER ) ;
	return( UsVerVal ) ;
}

#ifdef	ACCEPTANCE
//********************************************************************************
// Function Name 	: RunHea
// Retun Value		: Result
// Argment Value	: NON
// Explanation		: Hall Examination of Acceptance
// History			: First edition 						2014.02.26 Y.Shigeoka
//********************************************************************************
unsigned char	RunHea( void )
{
	unsigned char 	UcRst ;
	
	UcRst = EXE_END ;
	UcRst |= TstActMov( X_DIR) ;
	UcRst |= TstActMov( Y_DIR) ;
	
	return( UcRst ) ;
}
unsigned char	TstActMov( unsigned char UcDirSel )
{
	unsigned char UcRsltSts;
	unsigned short	UsMsppVal ;

	MesFil( NOISE ) ;					// 測定用フィルターを設定する。

	if ( !UcDirSel ) {
		RamWrite32A( sxsin , ACT_CHK_LVL );												// 0x10D5
		RamWrite32A( sysin , 0x000000 );												// 0x11D5
		SetSinWavePara( 0x05 , XACTTEST ); 
	}else{
		RamWrite32A( sxsin , 0x000000 );												// 0x10D5
		RamWrite32A( sysin , ACT_CHK_LVL );												// 0x11D5
		SetSinWavePara( 0x05 , YACTTEST ); 
	}

	if ( !UcDirSel ) {					// AXIS X
		RegWriteA( WC_MES1ADD0,  ( unsigned char )SXINZ1 ) ;							/* 0x0194	*/
		RegWriteA( WC_MES1ADD1,  ( unsigned char )(( SXINZ1 >> 8 ) & 0x0001 ) ) ;		/* 0x0195	*/
	} else {							// AXIS Y
		RegWriteA( WC_MES1ADD0,  ( unsigned char )SYINZ1 ) ;							/* 0x0194	*/
		RegWriteA( WC_MES1ADD1,  ( unsigned char )(( SYINZ1 >> 8 ) & 0x0001 ) ) ;		/* 0x0195	*/
	}

	RegWriteA( WC_MESSINMODE, 0x00 ) ;			// 0x0191	0 cross
	RegWriteA( WC_MESLOOP1, 0x00 ) ;				// 0x0193
	RegWriteA( WC_MESLOOP0, 0x02 ) ;				// 0x0192	2 Times Measure
	RamWrite32A( msmean	, 0x3F000000 );				// 0x10AE	1/CmMesLoop[15:0]/2
	RegWriteA( WC_MESABS, 0x00 ) ;				// 0x0198	none ABS
	BsyWit( WC_MESMODE, 0x02 ) ;				// 0x0190		Sine wave Measure

	RamAccFixMod( ON ) ;							// Fix mode
	RamReadA( MSPP1AV, &UsMsppVal ) ;
	RamAccFixMod( OFF ) ;							// Float mode

	if ( !UcDirSel ) {					// AXIS X
		SetSinWavePara( 0x00 , XACTTEST ); 	/* STOP */
	}else{
		SetSinWavePara( 0x00 , YACTTEST ); 	/* STOP */
	}

	
	UcRsltSts = EXE_END ;
	if( UsMsppVal > ACT_THR ){
		if ( !UcDirSel ) {					// AXIS X
			UcRsltSts = EXE_HXMVER ;
		}else{								// AXIS Y
			UcRsltSts = EXE_HYMVER ;
		}
	}
	
	return( UcRsltSts ) ;
}


//********************************************************************************
// Function Name 	: RunGea
// Retun Value		: Result
// Argment Value	: NON
// Explanation		: Gyro Examination of Acceptance
// History			: First edition 						2014.02.13 T.Tokoro
//********************************************************************************
unsigned char	RunGea( void )
{
	unsigned char 	UcRst, UcCnt, UcXLowCnt, UcYLowCnt, UcXHigCnt, UcYHigCnt ;
	unsigned short	UsGxoVal[10], UsGyoVal[10], UsDif;
	
	UcRst = EXE_END ;
	UcXLowCnt = UcYLowCnt = UcXHigCnt = UcYHigCnt = 0 ;
	
	MesFil( THROUGH ) ;				// 測定用フィルターを設定する。
	
	for( UcCnt = 0 ; UcCnt < 10 ; UcCnt++ )
	{
		// X
		RegWriteA( WC_MES1ADD0, 0x00 ) ;		// 0x0194
		RegWriteA( WC_MES1ADD1, 0x00 ) ;		// 0x0195
		ClrGyr( 0x1000 , CLR_FRAM1 );							// Measure Filter RAM Clear
		UsGxoVal[UcCnt] = (unsigned short)GenMes( AD2Z, 0 );	// 64回の平均値測定	GYRMON1(0x1110) <- GXADZ(0x144A)
		
		// Y
		RegWriteA( WC_MES1ADD0, 0x00 ) ;		// 0x0194
		RegWriteA( WC_MES1ADD1, 0x00 ) ;		// 0x0195
		ClrGyr( 0x1000 , CLR_FRAM1 );							// Measure Filter RAM Clear
		UsGyoVal[UcCnt] = (unsigned short)GenMes( AD3Z, 0 );	// 64回の平均値測定	GYRMON2(0x1111) <- GYADZ(0x14CA)
		
		
		if( UcCnt > 0 )
		{
			if ( (short)UsGxoVal[0] > (short)UsGxoVal[UcCnt] ) {
				UsDif = (unsigned short)((short)UsGxoVal[0] - (short)UsGxoVal[UcCnt]) ;
			} else {
				UsDif = (unsigned short)((short)UsGxoVal[UcCnt] - (short)UsGxoVal[0]) ;
			}
			
			if( UsDif > GEA_DIF_HIG ) {
				//UcRst = UcRst | EXE_GXABOVE ;
				UcXHigCnt ++ ;
			}
			if( UsDif < GEA_DIF_LOW ) {
				//UcRst = UcRst | EXE_GXBELOW ;
				UcXLowCnt ++ ;
			}
			
			if ( (short)UsGyoVal[0] > (short)UsGyoVal[UcCnt] ) {
				UsDif = (unsigned short)((short)UsGyoVal[0] - (short)UsGyoVal[UcCnt]) ;
			} else {
				UsDif = (unsigned short)((short)UsGyoVal[UcCnt] - (short)UsGyoVal[0]) ;
			}
			
			if( UsDif > GEA_DIF_HIG ) {
				//UcRst = UcRst | EXE_GYABOVE ;
				UcYHigCnt ++ ;
			}
			if( UsDif < GEA_DIF_LOW ) {
				//UcRst = UcRst | EXE_GYBELOW ;
				UcYLowCnt ++ ;
			}
		}
	}
	
	if( UcXHigCnt >= 1 ) {
		UcRst = UcRst | EXE_GXABOVE ;
	}
	if( UcXLowCnt > 8 ) {
		UcRst = UcRst | EXE_GXBELOW ;
	}
	
	if( UcYHigCnt >= 1 ) {
		UcRst = UcRst | EXE_GYABOVE ;
	}
	if( UcYLowCnt > 8 ) {
		UcRst = UcRst | EXE_GYBELOW ;
	}
	
	return( UcRst ) ;
}
#endif	//ACCEPTANCE


//********************************************************************************
// Function Name 	: CmdRdChk
// Retun Value		: 1 : ERROR
// Argment Value	: NON
// Explanation		: Check Cver function
// History			: First edition 						2014.02.27 K.abe
//********************************************************************************

unsigned char CmdRdChk( void )
{
	unsigned char UcTestRD;
	unsigned char UcCount;
	
	for(UcCount=0; UcCount < READ_COUNT_NUM; UcCount++){
		RegReadA( TESTRD ,	&UcTestRD );					// 0x027F
		if( UcTestRD == 0xAC){
			return(0);
		}
	}
	return(1);
}


//********************************************************************************
// Function Name 	: MeasGyroAmp
// Retun Value		: NON
// Argment Value	: NON
// Explanation		: Measuring Gyro Amp
// History			: First edition 						
//********************************************************************************
 #define		FIXCOEFF		412L	// 32.8*2*PI*2
void	MeasGyroAmp ( unsigned char	UcFreq , unsigned char	UcMeasMode )
{

	MesFil( NOISE ) ;					// 測定用フィルターを設定する。


	RamWrite32A( sxsin , 0x00000000 );		// 0x10D5
    RamWrite32A( sysin , 0x00000000 ); 		// 0x11D5
	SetSinWavePara( 0x04 , XHALWAVE ); 

	if( UcMeasMode == 0x00 ){
		RegWriteA( WC_MES1ADD0,  ( unsigned char )GXIZ ) ;							/* 0x0194	*/
		RegWriteA( WC_MES1ADD1,  ( unsigned char )(( GXIZ >> 8 ) & 0x0001 ) ) ;		/* 0x0195	*/
		RegWriteA( WC_MES2ADD0,  ( unsigned char )GYIZ ) ;							/* 0x0196	*/
		RegWriteA( WC_MES2ADD1,  ( unsigned char )(( GYIZ >> 8 ) & 0x0001 ) ) ;		/* 0x0197	*/
	} else {
		RegWriteA( WC_MES1ADD0,  ( unsigned char )GX2SXZ ) ;							/* 0x0194	*/
		RegWriteA( WC_MES1ADD1,  ( unsigned char )(( GX2SXZ >> 8 ) & 0x0001 ) ) ;		/* 0x0195	*/
		RegWriteA( WC_MES2ADD0,  ( unsigned char )GY2SYZ ) ;							/* 0x0196	*/
		RegWriteA( WC_MES2ADD1,  ( unsigned char )(( GY2SYZ >> 8 ) & 0x0001 ) ) ;		/* 0x0197	*/
	}

	RegWriteA( WC_MESLOOP1	, 0x00 );			// 0x0193	CmMesLoop[15:8]
//	RegWriteA( WC_MESLOOP0	, 0x10);			// 0x0192	CmMesLoop[7:0]
//	RamWrite32A( msmean	, 0x3D800000 );			// 0x1230	1/CmMesLoop[15:0]
	RegWriteA( WC_MESLOOP0	, 0x08);			// 0x0192	CmMesLoop[7:0]
	RamWrite32A( msmean	, 0x3E000000 );			// 0x1230	1/CmMesLoop[15:0]
	
	RamWrite32A( MSPP1AV, 	0x00000000 ) ;		// 0x1042
	RamWrite32A( MSPP2AV, 	0x00000000 ) ;		// 0x1142
	
	RegWriteA( WC_MESABS, 0x00 ) ;				// 0x0198	none ABS
	BsyWit( WC_MESMODE, 0x02 ) ;				// 0x0190		Sine wave Measure

	RamAccFixMod( ON ) ;							// Fix mode
	RamReadA( MSPP1AV, &StMeasGInfo.StGyrAmp.UsGyrXamp ) ;		// 0x1042
	RamReadA( MSPP2AV, &StMeasGInfo.StGyrAmp.UsGyrYamp ) ;		// 0x1142
	RamAccFixMod( OFF ) ;							// Float mode

	SetSinWavePara( 0x00 , XHALWAVE ); 	/* STOP */
	
	
	if( UcMeasMode == 0x00 ){
		StMeasGInfo.StGyrAgl.UlGyrXagl = (unsigned long)StMeasGInfo.StGyrAmp.UsGyrXamp * 1000L / ( (unsigned long)UcFreq * FIXCOEFF );
		StMeasGInfo.StGyrAgl.UlGyrYagl = (unsigned long)StMeasGInfo.StGyrAmp.UsGyrYamp * 1000L / ( (unsigned long)UcFreq * FIXCOEFF );

	} else {
		StMeasGInfo.StGyrAgl.UlGyrXagl = 0x00000000 ;
		StMeasGInfo.StGyrAgl.UlGyrYagl = 0x00000000 ;
	}
}
