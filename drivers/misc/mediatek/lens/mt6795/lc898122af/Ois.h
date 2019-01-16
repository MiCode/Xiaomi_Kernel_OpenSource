//********************************************************************************
//
//		<< LC898122 Evaluation Soft>>
//		Program Name	: Ois.h
// 		Explanation		: LC898122 Global Declaration & ProtType Declaration
//		Design			: Y.Yamada
//		History			: First edition						2009.07.30 Y.Tashita
//********************************************************************************

#ifdef	OISINI
	#define	OISINI__
#else
	#define	OISINI__		extern
#endif







#ifdef	OISCMD
	#define	OISCMD__
#else
	#define	OISCMD__		extern
#endif


// Define According To Usage

/****************************** Define説明 ******************************/
/*	USE_3WIRE_DGYRO		Digital Gyro I/F 3線Mode使用					*/
/*	USE_INVENSENSE		Invensense Digital Gyro使用						*/
/*		USE_IDG2020		Inv IDG-2020使用								*/
/*	STANDBY_MODE		Standby制御使用(未確認)							*/
/*	GAIN_CONT			:Gain control機能使用							*/
/*		(disable)		DSC			:三脚Mode使用						*/
/************************************************************************/

/**************** Select Gyro Sensor **************/
//#define 	USE_3WIRE_DGYRO    //for D-Gyro SPI interface

#define		USE_INVENSENSE		// INVENSENSE
#ifdef USE_INVENSENSE
//			#define		FS_SEL		0		/* ±262LSB/?/s  */
//			#define		FS_SEL		1		/* ±131LSB/?/s  */
//			#define		FS_SEL		2		/* ±65.5LSB/?/s  */
			#define		FS_SEL		3		/* ±32.8LSB/?/s  */

//			#define		GYROSTBY			/* Sleep+STBY */
#endif

/**************** Model name *****************/
#define		MN_4BSF01P1
/**************** FW version *****************/
#ifdef	MN_4BSF01P1
 #define	MDL_VER			0x0A
 #define	FW_VER			0x03
#endif

/**************** Select Mode **************/
#define		STANDBY_MODE		// STANDBY Mode
#define		GAIN_CONT			// Gain Control Mode
#define		PWM_BREAK			// PWM mode select (disable zero cross)


#ifdef	MN_4BSF01P1
 #define		ACTREG_10P5OHM		// Use 10.5ohm
//#define		CORRECT_1DEG			// Correct 1deg   disable 0.5deg
 #define		IDG2030			// Disable is IDG-2021
#endif

#define		DEF_SET				// default value re-setting
#define		H1COEF_CHANGER			/* H1 coef lvl chage */
#define		MONITOR_OFF			// default Monitor output
#define		ACCEPTANCE					// Examination of Acceptance



// Command Status
#define		EXE_END		0x02		// Execute End (Adjust OK)
#define		EXE_HXADJ	0x06		// Adjust NG : X Hall NG (Gain or Offset)
#define		EXE_HYADJ	0x0A		// Adjust NG : Y Hall NG (Gain or Offset)
#define		EXE_LXADJ	0x12		// Adjust NG : X Loop NG (Gain)
#define		EXE_LYADJ	0x22		// Adjust NG : Y Loop NG (Gain)
#define		EXE_GXADJ	0x42		// Adjust NG : X Gyro NG (offset)
#define		EXE_GYADJ	0x82		// Adjust NG : Y Gyro NG (offset)
#define		EXE_OCADJ	0x402		// Adjust NG : OSC Clock NG
#define		EXE_ERR		0x99		// Execute Error End

#ifdef	ACCEPTANCE
 // Hall Examination of Acceptance
 #define		EXE_HXMVER	0x06		// X Err
 #define		EXE_HYMVER	0x0A		// Y Err
 
 // Gyro Examination of Acceptance
 #define		EXE_GXABOVE	0x06		// X Above
 #define		EXE_GXBELOW	0x0A		// X Below
 #define		EXE_GYABOVE	0x12		// Y Above
 #define		EXE_GYBELOW	0x22		// Y Below
#endif	//ACCEPTANCE

// Common Define
#define	SUCCESS			0x00		// Success
#define	FAILURE			0x01		// Failure

#ifndef ON
 #define	ON				0x01		// ON
 #define	OFF				0x00		// OFF
#endif
 #define	SPC				0x02		// Special Mode

#define	X_DIR			0x00		// X Direction
#define	Y_DIR			0x01		// Y Direction
#define	X2_DIR			0x10		// X Direction
#define	Y2_DIR			0x11		// Y Direction

#define	NOP_TIME		0.00004166F

#ifdef STANDBY_MODE
 // Standby mode
 #define		STB1_ON		0x00		// Standby1 ON
 #define		STB1_OFF	0x01		// Standby1 OFF
 #define		STB2_ON		0x02		// Standby2 ON
 #define		STB2_OFF	0x03		// Standby2 OFF
 #define		STB3_ON		0x04		// Standby3 ON
 #define		STB3_OFF	0x05		// Standby3 OFF
 #define		STB4_ON		0x06		// Standby4 ON			/* for Digital Gyro Read */
 #define		STB4_OFF	0x07		// Standby4 OFF
 #define		STB2_OISON	0x08		// Standby2 ON (only OIS)
 #define		STB2_OISOFF	0x09		// Standby2 OFF(only OIS)
 #define		STB2_AFON	0x0A		// Standby2 ON (only AF)
 #define		STB2_AFOFF	0x0B		// Standby2 OFF(only AF)
#endif


// OIS Adjust Parameter
 #define		DAHLXO_INI		0x0000
 #define		DAHLXB_INI		0xE000
 #define		DAHLYO_INI		0x0000
 #define		DAHLYB_INI		0xE000
 #define		SXGAIN_INI		0x3000
 #define		SYGAIN_INI		0x3000
 #define		HXOFF0Z_INI		0x0000
 #define		HYOFF1Z_INI		0x0000


#ifdef ACTREG_10P5OHM		// TDK 10.5/10.5ohm Actuator ***************************
 #define		BIAS_CUR_OIS	0x33		//2.0mA/2.0mA
 #define		AMP_GAIN_X		0x04		//x100
 #define		AMP_GAIN_Y		0x04		//x100

/* OSC Init */
 #define		OSC_INI			0x2E		/* VDD=2.8V */

/* AF Open para */
 #define		RWEXD1_L_AF		0x7FFF		//
 #define		RWEXD2_L_AF		0x39F0		//
 #define		RWEXD3_L_AF		0x638D		//
 #define		FSTCTIME_AF		0xF1		//
 #define		FSTMODE_AF		0x00		//

 /* (0.425X^3+0.55X)*(0.425X^3+0.55X) 10.5ohm*/
 #define		A3_IEXP3		0x3ED9999A
 #define		A1_IEXP1		0x3F0CCCCD
 
#endif

/* AF adjust parameter */
#define		DAHLZB_INI		0x8001
#define		DAHLZO_INI		0x0000
#define		BIAS_CUR_AF		0x00		//0.25mA
#define		AMP_GAIN_AF		0x00		//x6

// Digital Gyro offset Initial value 
#define		DGYRO_OFST_XH	0x00
#define		DGYRO_OFST_XL	0x00
#define		DGYRO_OFST_YH	0x00
#define		DGYRO_OFST_YL	0x00

#define		SXGAIN_LOP		0x3000
#define		SYGAIN_LOP		0x3000

#define		TCODEH_ADJ		0x0000

#define		GYRLMT1H		0x3DCCCCCD		//0.1F

#ifdef	CORRECT_1DEG
 #define		GYRLMT3_S1		0x3F19999A		//0.60F
 #define		GYRLMT3_S2		0x3F19999A		//0.60F

 #define		GYRLMT4_S1		0x40400000		//3.0F
 #define		GYRLMT4_S2		0x40400000		//3.0F

 #define		GYRA12_HGH		0x402CCCCD		/* 2.70F */
 #define		GYRA12_MID		0x3F800000		/* 1.0F */
 #define		GYRA34_HGH		0x3F000000		/* 0.5F */
 #define		GYRA34_MID		0x3DCCCCCD		/* 0.1F */

 #define		GYRB12_HGH		0x3E4CCCCD		/* 0.20F */
 #define		GYRB12_MID		0x3CA3D70A		/* 0.02F */
 #define		GYRB34_HGH		0x3CA3D70A		/* 0.02F */
 #define		GYRB34_MID		0x3C23D70A		/* 0.001F */

#else
 #define		GYRLMT3_S1		0x3ECCCCCD		//0.40F
 #define		GYRLMT3_S2		0x3ECCCCCD		//0.40F

 #define		GYRLMT4_S1		0x40000000		//2.0F
 #define		GYRLMT4_S2		0x40000000		//2.0F

 #define		GYRA12_HGH		0x3FE00000		/* 1.75F */
 #define		GYRA12_MID		0x3F800000		/* 1.0F */
 #define		GYRA34_HGH		0x3F000000		/* 0.5F */
 #define		GYRA34_MID		0x3DCCCCCD		/* 0.1F */

 #define		GYRB12_HGH		0x3E4CCCCD		/* 0.20F */
 #define		GYRB12_MID		0x3CA3D70A		/* 0.02F */
 #define		GYRB34_HGH		0x3CA3D70A		/* 0.02F */
 #define		GYRB34_MID		0x3C23D70A		/* 0.001F */

#endif


//#define		OPTCEN_X		0x0000
//#define		OPTCEN_Y		0x0000

#ifdef USE_INVENSENSE
 #ifdef	MN_4BSF01P1
  #define		SXQ_INI			0x3F800000
  #define		SYQ_INI			0x3F800000

  #define		GXGAIN_INI		0x3F147AE1
  #define		GYGAIN_INI		0xBF147AE1

  #define		GYROX_INI		0x45
  #define		GYROY_INI		0x43
  
  #define		GXHY_GYHX		0
  
  #define		G_45G_INI		0x3EBFED46		// 32.8/87.5=0.3748...
 
 #endif
#endif


/* Optical Center & Gyro Gain for Mode */
 #define	VAL_SET				0x00		// Setting mode
 #define	VAL_FIX				0x01		// Fix Set value
 #define	VAL_SPC				0x02		// Special mode


struct STFILREG {
	unsigned short	UsRegAdd ;
	unsigned char	UcRegDat ;
} ;													// Register Data Table

struct STFILRAM {
	unsigned short	UsRamAdd ;
	unsigned long	UlRamDat ;
} ;													// Filter Coefficient Table

struct STCMDTBL
{
	unsigned short Cmd ;
	unsigned int UiCmdStf ;
	void ( *UcCmdPtr )( void ) ;
} ;

/*** caution [little-endian] ***/

// Word Data Union
union	WRDVAL{
	unsigned short	UsWrdVal ;
	unsigned char	UcWrkVal[ 2 ] ;
	struct {
		unsigned char	UcLowVal ;
		unsigned char	UcHigVal ;
	} StWrdVal ;
} ;

typedef union WRDVAL	UnWrdVal ;

union	DWDVAL {
	unsigned long	UlDwdVal ;
	unsigned short	UsDwdVal[ 2 ] ;
	struct {
		unsigned short	UsLowVal ;
		unsigned short	UsHigVal ;
	} StDwdVal ;
	struct {
		unsigned char	UcRamVa0 ;
		unsigned char	UcRamVa1 ;
		unsigned char	UcRamVa2 ;
		unsigned char	UcRamVa3 ;
	} StCdwVal ;
} ;

typedef union DWDVAL	UnDwdVal;

// Float Data Union
union	FLTVAL {
	float			SfFltVal ;
	unsigned long	UlLngVal ;
	unsigned short	UsDwdVal[ 2 ] ;
	struct {
		unsigned short	UsLowVal ;
		unsigned short	UsHigVal ;
	} StFltVal ;
} ;

typedef union FLTVAL	UnFltVal ;

typedef struct STMEASGINFO {
	struct {
		unsigned long	UlGyrXagl ;				// X Gyro Angle
		unsigned long	UlGyrYagl ;				// Y Gyro Angle
	} StGyrAgl ;

	struct {
		unsigned short	UsGyrXamp ;				// X Gyro Amp
		unsigned short	UsGyrYamp ;				// Y Gyro Amp
	} StGyrAmp ;

} stMeasGInfo ;

OISCMD__	stMeasGInfo	StMeasGInfo ;			// 

typedef struct STADJPAR {
	struct {
		unsigned char	UcAdjPhs ;				// Hall Adjust Phase

		unsigned short	UsHlxCna ;				// Hall Center Value after Hall Adjust
		unsigned short	UsHlxMax ;				// Hall Max Value
		unsigned short	UsHlxMxa ;				// Hall Max Value after Hall Adjust
		unsigned short	UsHlxMin ;				// Hall Min Value
		unsigned short	UsHlxMna ;				// Hall Min Value after Hall Adjust
		unsigned short	UsHlxGan ;				// Hall Gain Value
		unsigned short	UsHlxOff ;				// Hall Offset Value
		unsigned short	UsAdxOff ;				// Hall A/D Offset Value
		unsigned short	UsHlxCen ;				// Hall Center Value

		unsigned short	UsHlyCna ;				// Hall Center Value after Hall Adjust
		unsigned short	UsHlyMax ;				// Hall Max Value
		unsigned short	UsHlyMxa ;				// Hall Max Value after Hall Adjust
		unsigned short	UsHlyMin ;				// Hall Min Value
		unsigned short	UsHlyMna ;				// Hall Min Value after Hall Adjust
		unsigned short	UsHlyGan ;				// Hall Gain Value
		unsigned short	UsHlyOff ;				// Hall Offset Value
		unsigned short	UsAdyOff ;				// Hall A/D Offset Value
		unsigned short	UsHlyCen ;				// Hall Center Value
	} StHalAdj ;

	struct {
		unsigned short	UsLxgVal ;				// Loop Gain X
		unsigned short	UsLygVal ;				// Loop Gain Y
		unsigned short	UsLxgSts ;				// Loop Gain X Status
		unsigned short	UsLygSts ;				// Loop Gain Y Status
	} StLopGan ;

	struct {
		unsigned short	UsGxoVal ;				// Gyro A/D Offset X
		unsigned short	UsGyoVal ;				// Gyro A/D Offset Y
		unsigned short	UsGxoSts ;				// Gyro Offset X Status
		unsigned short	UsGyoSts ;				// Gyro Offset Y Status
	} StGvcOff ;
	
	unsigned char		UcOscVal ;				// OSC value

} stAdjPar ;

OISCMD__	stAdjPar	StAdjPar ;				// Execute Command Parameter

OISCMD__	unsigned char	UcOscAdjFlg ;		// For Measure trigger
  #define	MEASSTR		0x01
  #define	MEASCNT		0x08
  #define	MEASFIX		0x80

OISINI__	unsigned short	UsCntXof ;				/* OPTICAL Center Xvalue */
OISINI__	unsigned short	UsCntYof ;				/* OPTICAL Center Yvalue */

OISINI__	unsigned char	UcPwmMod ;				/* PWM MODE */
#define		PWMMOD_CVL	0x00		// CVL PWM MODE
#define		PWMMOD_PWM	0x01		// PWM MODE

#define		INIT_PWMMODE	PWMMOD_CVL		// initial output mode

OISINI__	unsigned char	UcCvrCod ;				/* CverCode */
 #define	CVER122		0x93		 // LC898122
 #define	CVER122A	0xA1		 // LC898122A


// Prottype Declation
OISINI__ void	IniSet( void ) ;													// Initial Top Function
OISINI__ void	IniSetAf( void ) ;													// Initial Top Function

OISINI__ void	ClrGyr( unsigned short, unsigned char ); 							   // Clear Gyro RAM
	#define CLR_FRAM0		 	0x01
	#define CLR_FRAM1 			0x02
	#define CLR_ALL_RAM 		0x03
OISINI__ void	BsyWit( unsigned short, unsigned char ) ;				// Busy Wait Function
//OISINI__ void	WitTim( unsigned short ) ;											// Wait
OISINI__ void	MemClr( unsigned char *, unsigned short ) ;							// Memory Clear Function
OISINI__ void	GyOutSignal( void ) ;									// Slect Gyro Output signal Function
OISINI__ void	GyOutSignalCont( void ) ;								// Slect Gyro Output Continuos Function
#ifdef STANDBY_MODE
OISINI__ void	AccWit( unsigned char ) ;								// Acc Wait Function
OISINI__ void	SelectGySleep( unsigned char ) ;						// Select Gyro Mode Function
#endif
#ifdef	GAIN_CONT
OISINI__ void	AutoGainControlSw( unsigned char ) ;							// Auto Gain Control Sw
#endif
OISINI__ void	DrvSw( unsigned char UcDrvSw ) ;						// Driver Mode setting function
OISINI__ void	AfDrvSw( unsigned char UcDrvSw ) ;						// AF Driver Mode setting function
OISINI__ void	RamAccFixMod( unsigned char ) ;							// Ram Access Fix Mode setting function
OISINI__ void	IniPtMovMod( unsigned char ) ;							// Pan/Tilt parameter setting by mode function
OISINI__ void	ChkCvr( void ) ;													// Check Function
	
OISCMD__ void			SrvCon( unsigned char, unsigned char ) ;					// Servo ON/OFF
OISCMD__ unsigned char	RtnCen( unsigned char ) ;									// Return to Center Function
OISCMD__ void			OisEna( void ) ;											// OIS Enable Function
OISCMD__ void			OisEnaLin( void ) ;											// OIS Enable Function for Line adjustment
OISCMD__ void			TimPro( void ) ;											// Timer Interrupt Process Function
OISCMD__ void			S2cPro( unsigned char ) ;									// S2 Command Process Function

 #ifdef	MN_4BSF01P1
	#define		DIFIL_S2		0x3F7FFE00
 #endif
OISCMD__ void			SetSinWavePara( unsigned char , unsigned char ) ;			// Sin wave Test Function
	#define		SINEWAVE	0
	#define		XHALWAVE	1
	#define		YHALWAVE	2
	#define		XACTTEST	10
	#define		YACTTEST	11
	#define		CIRCWAVE	255
OISCMD__ void			SetZsp( unsigned char ) ;									// Set Zoom Step parameter Function
OISCMD__ void			OptCen( unsigned char, unsigned short, unsigned short ) ;	// Set Optical Center adjusted value Function
OISCMD__ void			StbOnnN( unsigned char , unsigned char ) ;					// Stabilizer For Servo On Function
#ifdef STANDBY_MODE
 OISCMD__ void			SetStandby( unsigned char ) ;								/* Standby control	*/
#endif

OISCMD__ void			GyrGan( unsigned char , unsigned long , unsigned long ) ;	/* Set Gyro Gain Function */
OISCMD__ void			SetPanTiltMode( unsigned char ) ;							/* Pan_Tilt control Function */
#ifdef GAIN_CONT
OISCMD__ unsigned char	TriSts( void ) ;													// Read Status of Tripod mode Function
#endif
OISCMD__ unsigned char	DrvPwmSw( unsigned char ) ;											// Select Driver mode Function
	#define		Mlnp		0					// Linear PWM
	#define		Mpwm		1					// PWM
OISCMD__ void			SetGcf( unsigned char ) ;									// Set DI filter coefficient Function
OISCMD__	unsigned long	UlH1Coefval ;		// H1 coefficient value
#ifdef H1COEF_CHANGER
 OISCMD__	unsigned char	UcH1LvlMod ;		// H1 level coef mode
 OISCMD__	void			SetH1cMod( unsigned char ) ;								// Set H1C coefficient Level chang Function
 #define		S2MODE		0x40
 #define		ACTMODE		0x80
 #define		MOVMODE		0xFF
#endif
OISCMD__	unsigned short	RdFwVr( void ) ;										// Read Fw Version Function

#ifdef	ACCEPTANCE
OISCMD__	unsigned char	RunHea( void ) ;										// Hall Examination of Acceptance
 #define		ACT_CHK_LVL		0x3ECCCCCD		// 0.4
 #define		ACT_THR			0x0400			// 28dB 20log(4/(0.4*256))
OISCMD__	unsigned char	RunGea( void ) ;										// Gyro Examination of Acceptance
 #define		GEA_DIF_HIG		0x0010
 #define		GEA_DIF_LOW		0x0001
#endif	//ACCEPTANCE
// Dead Lock Check
OISCMD__	unsigned char CmdRdChk( void );
	#define READ_COUNT_NUM	3
OISCMD__	void	MeasGyroAmp( unsigned char , unsigned char ) ;					// Gyro amp measurement Function

void RegWriteA(unsigned short RegAddr, unsigned char RegData);
void RegReadA(unsigned short RegAddr, unsigned char *RegData);
void RamWriteA( unsigned short RamAddr, unsigned short RamData );
void RamReadA( unsigned short RamAddr, void * ReadData );
void RamWrite32A(unsigned short RamAddr, unsigned long RamData );
void RamRead32A(unsigned short RamAddr, void * ReadData );
void WitTim(unsigned short  UsWitTim );
void LC898prtvalue(unsigned short  value );
