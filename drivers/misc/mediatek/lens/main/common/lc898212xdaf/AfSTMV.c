//********************************************************************************
//
//		<< LC89821x Step Move module >>
//	    Program Name	: AfSTMV.c
//		Design			: Y.Yamada
//		History			: First edition						2009.07.31 Y.Tashita
//		History			: LC898211 changes					2012.06.11 YS.Kim
//		History			: LC898212 changes					2013.07.19 Rex.Tang
//********************************************************************************
//**************************
//	Include Header File		
//**************************
#include	"AfSTMV.h"
#include	"AfDef.h"


//**************************
//	Definations					
//**************************
#define	abs(x)	((x) < 0 ? -(x) : (x))
#define	LC898211_fs	234375

/*--------------------------
    Local defination
---------------------------*/
static 	stSmvPar StSmvPar;

/* Step Move to Finish Check Function */
/* static 	unsigned char	StmvEnd( unsigned char ) ; */

/*====================================================================
	Interface functions (import)
=====================================================================*/
extern	void RamWriteA(unsigned short addr, unsigned short data);

extern	void RamReadA(unsigned short addr, unsigned short *data);

extern	void RegWriteA(unsigned short addr, unsigned char data);

extern	void RegReadA(unsigned short addr, unsigned char *data);

extern	void WaitTime(unsigned short msec);

/*-----------------------------------------------------------
    Function Name   : StmvSet, StmvTo, StmvEnd
	Description     : StepMove Setting, Execute, Finish, Current Limit 
    Arguments       : Stepmove Mode Parameter
	Return			: Stepmove Mode Parameter
-----------------------------------------------------------*/
//StmvSet -> StmvTo -> StmvEnd -> StmvTo -> StmvEnd ->ÅEÅEÅE

//********************************************************************************
// Function Name 	: StmvSet
// Retun Value		: NON
// Argment Value	: NON
// Explanation		: Stpmove parameter Setting Function
// History			: First edition 						2012.06.12 YS.Kim
// History			: Changes								2013.07.19 Rex.Tang
//********************************************************************************
void StmvSet( stSmvPar StSetSmv )
{
	unsigned char	UcSetEnb;
	unsigned char	UcSetSwt;
	unsigned short	UsParSiz;
	unsigned char	UcParItv;
    short 			SsParStt;	// StepMove Start Position

    StSmvPar.UsSmvSiz = StSetSmv.UsSmvSiz;
    StSmvPar.UcSmvItv = StSetSmv.UcSmvItv;
    StSmvPar.UcSmvEnb = StSetSmv.UcSmvEnb;
    
	RegWriteA( AFSEND_211	, 0x00 );										// StepMove Enable Bit Clear
	
	RegReadA( ENBL_211 ,	&UcSetEnb );
	UcSetEnb 	&= (unsigned char)0xFD ;
	RegWriteA( ENBL_211	,	UcSetEnb );										// Measuremenet Circuit1 Off
	
	RegReadA( SWTCH_211 ,	&UcSetSwt );
	UcSetSwt	&= (unsigned char)0x7F ;
	RegWriteA( SWTCH_211 , UcSetSwt );										// RZ1 Switch Cut Off
	
	RamReadA( RZ_211H ,	(unsigned short *)&SsParStt );										// Get Start Position
	UsParSiz	= StSetSmv.UsSmvSiz ;										// Get StepSize
	UcParItv	= StSetSmv.UcSmvItv ;										// Get StepInterval
	
	RamWriteA( ms11a_211H	, (unsigned short)0x0800 );						// Set Coefficient Value For StepMove
	RamWriteA( MS1Z22_211H	, (unsigned short)SsParStt );					// Set Start Positon
	RamWriteA( MS1Z12_211H	, UsParSiz );									// Set StepSize
	RegWriteA( STMINT_211	, UcParItv );									// Set StepInterval
	
	UcSetSwt	|= (unsigned char)0x80;
	RegWriteA( SWTCH_211, UcSetSwt );										// RZ1 Switch ON
}



//********************************************************************************
// Function Name 	: StmvTo
// Retun Value		: Stepmove Parameter
// Argment Value	: Stepmove Parameter, Target Position
// Explanation		: Stpmove Function
// History			: First edition 						2012.06.12 YS.Kim
// History			: Changes								2013.07.19 Rex.Tang
//********************************************************************************
unsigned char StmvTo( short SsSmvEnd )
{
	/* unsigned short	UsSmvTim; */
	unsigned short	UsSmvDpl;
    short 			SsParStt;	// StepMove Start Position
	
	//PIOA_SetOutput(_PIO_PA29);													// Monitor I/O Port
	
	RamReadA( RZ_211H ,	(unsigned short *)&SsParStt );											// Get Start Position
	UsSmvDpl = abs( SsParStt - SsSmvEnd );
	
	if( ( UsSmvDpl <= StSmvPar.UsSmvSiz ) && (( StSmvPar.UcSmvEnb & STMSV_ON ) == STMSV_ON ) ){
		if( StSmvPar.UcSmvEnb & STMCHTG_ON ){
			RegWriteA( MSSET_211	, INI_MSSET_211 | (unsigned char)0x01 );
		}
		RamWriteA( MS1Z22_211H, SsSmvEnd );										// Handling Single Step For ES1
		StSmvPar.UcSmvEnb	|= STMVEN_ON;										// Combine StepMove Enable Bit & StepMove Mode Bit
	} else {
		if( SsParStt < SsSmvEnd ){												// Check StepMove Direction
			RamWriteA( MS1Z12_211H	, StSmvPar.UsSmvSiz );
		} else if( SsParStt > SsSmvEnd ){
			RamWriteA( MS1Z12_211H	, -StSmvPar.UsSmvSiz );
		}
		
		RamWriteA( STMVENDH_211, SsSmvEnd );									// Set StepMove Target Positon
		StSmvPar.UcSmvEnb	|= STMVEN_ON;										// Combine StepMove Enable Bit & StepMove Mode Bit
		RegWriteA( STMVEN_211	, StSmvPar.UcSmvEnb );							// Start StepMove
	}
	
#if 1
	/*
	 * AF step_moveto is set per 33ms, so don't need this waiting.
	 */
	return 0;
#else
	UsSmvTim=(UsSmvDpl/STMV_SIZE)*((STMV_INTERVAL+1)*10000 / LC898211_fs);			// Stepmove Operation time
    WaitTime( UsSmvTim );
	//TRACE("STMV Operation Time = %d \n", UsSmvTim ) ;
	
	return StmvEnd( StSmvPar.UcSmvEnb );
#endif
}



//********************************************************************************
// Function Name 	: StmvEnd
// Retun Value		: Stepmove Parameter
// Argment Value	: Stepmove Parameter
// Explanation		: Stpmove Finish Check Function
// History			: First edition 						2012.06.12 YS.Kim
// History			: Changes								2013.07.19 Rex.Tang
//********************************************************************************
/* unsigned char StmvEnd( unsigned char UcParMod )
{
	unsigned char	UcChtGst;
	unsigned short  i = 0;
	
	while( (UcParMod & (unsigned char)STMVEN_ON ) && (i++ < 100))								// Wait StepMove operation end
	{
		RegReadA( STMVEN_211 , &UcParMod );
	}

	if( ( UcParMod & (unsigned char)0x08 ) == (unsigned char)STMCHTG_ON ){		// If Convergence Judgement is Enabled
        for(i=0; i<CHTGOKN_WAIT; i++)
		{
        	RegReadA( MSSET_211, &UcChtGst );
        	if(!(UcChtGst & 0x01))	break;
        	WaitTime(1);	
		}
	}
	
	if( UcChtGst & 0x01 ){
		UcParMod	|= (unsigned char)0x80 ;									// STMV Success But Settling Time Over
		//PIOA_ClearOutput(_PIO_PA29);											// Monitor I/O Port
	}else{
		UcParMod	&= (unsigned char)0x7F ;									// STMV Success 
	}
	
	return UcParMod;															// Bit0:0 Successful convergence Bit0:1 Time Over
} */


