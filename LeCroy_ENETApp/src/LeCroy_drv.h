/**********************************************************************************/
/**  Author :  Sheng Peng (SNS, ORNL)                                            **/
/**  Contact:  pengs@ornl.gov     865-241-1779                                   **/
/**  Date:     02/24/2005                                                        **/
/**  Version:  V1.6.2                                                            **/
/**  Description: Driver for LeCroy with Ethernet Interface                      **/
/**********************************************************************************/

/**********************************************************************************/
/* Description:                                                                   */
/*                                                                                */
/* This file is a client-side implementation of the VICP network communications   */
/* protocol that is being used to control LeCroy Digital Oscilloscopes (DSOs).    */
/*                                                                                */
/* VICP Protocol Description/History:                                             */
/*                                                                                */
/* The VICP Protocol has been around since 1997/98. It did not change in any way  */
/* between it's conception, and June 2003, when a previously reserved field in    */
/* the header was assigned. This field, found at byte #2, is now used to allow    */
/* the client-end of a VICP communication to detect 'out of sync' situations, and */
/* therefore allows the GPIB 488.2 'Unread Response' mechanism to be emulated.    */
/*                                                                                */
/* These extensions to the original protocol did not cause a version number       */
/* change, and are referred to as version 1a. It was decided not to bump the      */
/* version number to reduce the impact on clients, many of which are looking      */
/* for a version number of 1. Clients and servers detect protocol version 1a      */
/* by examining the sequence number field, it should be 0 for version 1 of        */
/* the protocol (early clients), or non-zero for v1a.                             */
/*                                                                                */
/* VICP Headers:                                                                  */
/*                                                                                */
/*    Byte        Description                                                     */
/*    ------------------------------------------------                            */
/*    0              Operation                                                    */
/*    1              Version         1 = version 1                                */
/*    2              Sequence Number { 1..255 }, (was unused until June 2003)     */
/*    3              Unused                                                       */
/*    4              Block size, MSB  (not including this header)                 */
/*    5              Block size                                                   */
/*    6              Block size                                                   */
/*    7              Block size, LSB                                              */
/*    Note: The byte order of block size is big-endian, manual is wrong           */
/*                                                                                */
/* Operation bits:                                                                */
/*                                                                                */
/*    Bit             Mnemonic        Purpose                                     */
/*    -----------------------------------------------                             */
/*    D7              DATA            Data block (D0 indicates with/without EOI)  */
/*    D6              REMOTE          Remote Mode                                 */
/*    D5              LOCKOUT         Local Lockout (Lockout front panel)         */
/*    D4              CLEAR           Device Clear                                */
/*      (if sent with data, clear occurs before block is passed to parser)        */
/*    D3              SRQ             SRQ (Device -> PC only)                     */
/*    D2              SERIALPOLL      Request a serial poll                       */
/*    D1              Reserved        Reserved for future expansion               */
/*    D0              EOI             Block terminated in EOI                     */
/*                                                                                */
/* Known Limitations:                                                             */
/*    1. I don't think we need CLEAR in our cases.                                */
/*    2. When we get SRQ, we just simply ignore it.                               */
/*    3. We don't do SERIALPOLL(OOB or in-bound) here.                            */
/*                                                                                */
/**********************************************************************************/

#ifndef	_INC_LeCroy_drv
#define	_INC_LeCroy_drv

#ifndef BOOL
        #define BOOL int
#endif /* BOOL */

#ifndef STATUS
        #define STATUS int
#endif /* STATUS */

#ifndef ERROR
        #define ERROR (-1)
#endif /* ERROR */

#ifndef OK
        #define OK (0)
#endif /* OK */

/*include*/
#include <epicsThread.h>
#include <epicsMutex.h>
#include <epicsStdio.h>
#include <dbDefs.h>
#include <string.h>
#include <osiSock.h>
#include <drvSup.h>
#include <dbScan.h>
#include <fcntl.h>
#include "netinet/tcp.h"

#include <stdlib.h>
#include <ctype.h>

#include "LeCroy_GenType.h"

#define	MAX_CA_STRING_SIZE	40
#define	MAX_CMD_STRING_SIZE	256	/* LeCroy scope have 256 bytes buffer to hold cmd, longer command will slow down pergormance */

#define	SCOPE_PORT_NUM		1861	/* scope's  listening port number*/
#define	TWO_CHANNEL_SCOPE	2
#define	FOUR_CHANNEL_SCOPE	4
#define	READ_TIMEOUT		6	/* read socket with timeout 6 seconds */
#define	CONN_TIMEOUT		6	/* Connect with timeout 6 seconds */

/* Use CFMT OFF to turn off block information, but even use CFMT DEF9 or CFMT IND0,
   no code modification needed, but turning it off will help speed slightly;
   for more than 8 bits resolution, use "CFMT OFF,WORD,BIN;" */
/* CHDR OFF will turn off command echo, if turn it on, waveform read will be still
   ok, but we have to change something to analyze response for other commands */
/* for little endian platform, use "CORD LO;", this affects all parts of WF readback */
#if defined(vxWorks)

#if	_BYTE_ORDER == _BIG_ENDIAN
#define	INIT_STRING	"CFMT OFF,BYTE,BIN;CHDR OFF;CORD HI;WFSU SP,0,NP,0,FP,0,SN,0"
#else
#define	INIT_STRING	"CFMT OFF,BYTE,BIN;CHDR OFF;CORD LO;WFSU SP,0,NP,0,FP,0,SN,0"
#endif

#elif defined(linux)

#if	__BYTE_ORDER == __BIG_ENDIAN
#define	INIT_STRING	"CFMT OFF,BYTE,BIN;CHDR OFF;CORD HI;WFSU SP,0,NP,0,FP,0,SN,0"
#else
#define	INIT_STRING	"CFMT OFF,BYTE,BIN;CHDR OFF;CORD LO;WFSU SP,0,NP,0,FP,0,SN,0"
#endif

#else
#error "Need to figure out byte order!"
#endif

/* Command to query template */
#define TMPL_STRING	"TMPL?"
/* template we supported, acturally we can support LECROY_2_X, X=1,2,3 */
#define	TEMPLATE	"LECROY_2_3"

/* Command to query model */
#define IDN_STRING	"*IDN?"
/* because the normal reaback is "LECROY, LTXXX(L)  ,serial no., ver no.\n" */
#define	UNKNOWN_MODEL	"unknown,unknown  ,unknown,unknown\n"

/* index 0~7(channel number 1~8) map to C1,C2,C3,C4,TA,TB,TC,TD */
/* if you have a two channels scope, use channel numner 1,2 & 5,6 */
#define	TOTALCHNLS	8
/* Command to query all channels' status */
/* for 4 channels scope */
#define	CHNLSTAT_STRING_4	"C1:TRA?;C2:TRA?;C3:TRA?;C4:TRA?;TA:TRA?;TB:TRA?;TC:TRA?;TD:TRA?"
/* for 2 channels scope */
#define	CHNLSTAT_STRING_2	"C1:TRA?;C2:TRA?;TA:TRA?;TB:TRA?"
const static char ChannelName[TOTALCHNLS][4]={"C1:","C2:","C3:","C4:","TA:","TB:","TC:","TD:"};

#define	LINK_CHECK_INTERVAL	30 /* every 30 seconds check link status */
#define	LINK_CHECK_ONCE		-1/* check and recover once */

#define	LINK_CHECK_PRIORITY	epicsThreadPriorityMedium	/* task priority for link monitor */

/* Link Status  **/
#define	LINK_DOWN		0
#define	LINK_OK			1
#define	LINK_RECOVER		2
#define	LINK_UNSUPPORTED	3

/*	Commands for control LeCroy scope	*/

#define	RESET		0

#define	ENABLECHAN	1
#define	DISABLECHAN	2
#define	GETCHANSTAT	3

#define	SETMEMSIZE	4
#define	GETMEMSIZE	5

#define	SETTIMEDIV	6
#define	GETTIMEDIV	7

#define	SETVOLTDIV	8
#define	GETVOLTDIV	9

#define	SETTRGMODE	10
#define GETTRGMODE	11

#define	SETTRGSRC	12
#define GETTRGSRC	13

#define	LDPNLSTP	14
#define	SVPNLSTP	15

#define	GETWF		16	/* this is not really used in driver, it is reserved for caller to tell caller use LeCroy_Read */

/* To support new_command, you hace to add new definition below */
#define	ENABLEACAL	17
#define	DISABLEACAL	18
#define	GETACALSTAT	19
/* To support new_command, you have to add new definition above */


/** for chnlstat readback */
#define	OFF	0
#define	ON	1

/* for trigger mode read back */
#define	AUTO	0
#define	NORM	1
#define	SINGLE	2
#define	STOP	3

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/* if you don't use CHDR OFF, these two structs should be changed */
static struct	MEMORY_SIZE
{
	char	cmd[20];
	char	response[20];
	int	val;
}	msiz_op[14]={	{"MSIZ 500",	"500",	500},
			{"MSIZ 1000",	"1000",	1000},
			{"MSIZ 2500",	"2500",	2500},
			{"MSIZ 5000",	"5000",	5000},
			{"MSIZ 10K",	"10K",	10000},
			{"MSIZ 25K",	"25K",	25000},
			{"MSIZ 50K",	"50K",	50000},
			{"MSIZ 100K",	"100K",	100000},
			{"MSIZ 250K",	"250K",	250000},
			{"MSIZ 500K",	"500K",	500000},
			{"MSIZ 1M",	"1M",	1000000},
			{"MSIZ 2.5M",	"2.5M",	2500000},
			{"MSIZ 5M",	"5M",	5000000},
			{"MSIZ 10M",	"10M",	10000000}};

static struct TRIG_MODE
{
	char	cmd[20];
	char	response[20];
	int	val;
}	trigger_mode[4]={	{"TRMD AUTO",	"AUTO",		AUTO},
				{"TRMD NORM",	"NORM",		NORM},
				{"TRMD SINGLE",	"SINGLE",	SINGLE},
				{"TRMD STOP",	"STOP",		STOP} };
							
/* To support new_command, you might want to add new structure here */

struct WAVEDESC
{
	UINT8			DESCRIPTOR_NAME[16];
	UINT8			TEMPLATE_NAME[16];
	SINT16			COMM_TYPE;
	SINT16			COMM_ORDER;
	SINT32			WAVE_DESCRIPTOR;
	SINT32			USER_TEXT;
	SINT32			RES_DESC1;
	SINT32			TRIGTIME_ARRAY;
	SINT32			RIS_TIME_ARRAY;
	SINT32			RES_ARRAY1;
	SINT32			WAVE_ARRAY_1;
	SINT32			WAVE_ARRAY_2;
	SINT32			RES_ARRAY2;
	SINT32			RES_ARRAY3;
	UINT8			INSTRUMENT_NAME[16];
	SINT32			INSTRUMENT_NUMBER;
	UINT8			TRACE_LABEL[16];
	SINT16			RESERVED1;
	SINT16			RESERVED2;
	SINT32			WAVE_ARRAY_COUNT;
	SINT32			PNTS_PER_SCREEN;
	SINT32			FIRST_VALID_PNT;
	SINT32			LAST_VALID_PNT;
	SINT32			FIRST_POINT;
	SINT32			SPARSING_FACTOR;
	SINT32			SEGMENT_INDEX;
	SINT32			SUBARRAY_COUNT;
	SINT32			SWEEP_PER_ACQ;
	SINT16			POINTS_PER_PAIR;
	SINT16			PAIR_OFFSET;
	FLOAT32			VERTICAL_GAIN;
	FLOAT32			VERTICAL_OFFSET;
	FLOAT32			MAX_VALUE;
	FLOAT32			MIN_VALUE;
	SINT16			NOMINAL_BITS;
	SINT16			NOM_SUBARRAY_COUNT;
	FLOAT32			HORIZ_INTERVAL;
	DOUBLE64		HORIZ_OFFSET;
	DOUBLE64		PIXEL_OFFSET;
	UINT8			VERTUNIT[48];
	UINT8			HORUNIT[48];
	FLOAT32			HORIZ_UNCERTAINTY;
	struct	TIME_STAMP
	{
		DOUBLE64	seconds;
		UINT8		minutes;
		UINT8		hours;
		UINT8		days;
		UINT8		months;
		SINT16		year;
		SINT16		unused;
	}			TRIGGER_TIME;
	FLOAT32			ACQ_DURATION;
	SINT16			RECORD_TYPE;
	SINT16			PROCESSING_DONE;
	SINT16			RESERVED5;
	SINT16			RIS_SWEEPS;
	SINT16			TIMEBASE;
	SINT16			VERT_COUPLING;
	FLOAT32			PROBE_ATT;
	SINT16			FIXED_VERT_GAIN;
	SINT16			BANDWIDTH_LIMIT;
	FLOAT32			VERTICAL_VERNIER;
	FLOAT32			ACQ_VERT_OFFSET;
	SINT16			WAVE_SOURCE;
} __attribute__ ((packed)) __attribute__ (( aligned (8) ));
/* we really need "packed", because or else compiler aligned it to 8 bytes (DOUBLE64),the size will be 360 instead of 346 */
/* then when we read wavedesc and memcpy, some parameter will be incorrect because complier insert some junk space */
/* we need whole structure is aligned by 8 bytes, because we will define an array of it, or else the second member of array */
/* will have alignment problem, now size will be 352 */

/* when we do memcpy, we don't wanna copy 352 bytes (sizeof), that might cause access to unauthorized memory space */
#define	REALDESCSIZE	346
#define EXPDDESCSIZE	352

typedef struct LECROY
{
	char		IPAddr[MAX_CA_STRING_SIZE];
	int		channels;	/* 2(TWO_CHANNEL_SCOPE) or 4(FOUR_CHANNEL_SCOPE) */

	int		sFd;
	int		linkstat;
	epicsThreadId	MonTaskID;	/* link monitor task */

        epicsMutexId    semLecroy;	/* to protect access to scope */
	int		lasterr;

	int		remoteEnable;	/* Show REMOTE on LCD of scope */
	int		lockFP;		/* Lockout front panel */
	int		lastSeqNum;	/* 1 ~ 255 for V1a, 0 for V1, we can always send non-zero */

        epicsMutexId    semOp;  	/* to protect access to WAVEDESC structure and LeCroyModel */
	int		VICP_Version;	/* So far the version in header is always 1 */
	char		VICP_Revision;	/* but revision could be '0' or 'a' so far */
	char		LeCroyModel[MAX_CA_STRING_SIZE];
	char		chanenbl[TOTALCHNLS];
	struct WAVEDESC	channel_desc[TOTALCHNLS];
}	* LeCroyID;			/* it is not necessary to say packed here, cause we access all member by name */

/* Here we define something in communication header */
#define	COMM_HDR_SIZE		8

/* First byte of header */
#define	COMM_HDR_OPER_DATA	0x80
#define	COMM_HDR_OPER_REMOTE	0x40
#define	COMM_HDR_OPER_LOCKFP	0x20
#define	COMM_HDR_OPER_CLEAR	0x10
#define	COMM_HDR_OPER_SRQ	0x08
#define	COMM_HDR_OPER_SPOLL	0x04
#define	COMM_HDR_OPER_RESV	0x02
#define	COMM_HDR_OPER_EOI	0x01

/* Second byte of header */
#define COMM_HDR_VER_1		0x01

/* Revision, we are not using string to avoid mutex issue */
#define	COMM_HDR_REV_0		'0'
#define	COMM_HDR_REV_A		'a'

/* error no and message */
#define	LECROY_ERR_NO_ERROR			0
#define	LECROY_ERR_SELECT_SOCKET_TIMEOUT	1
#define	LECROY_ERR_READ_SOCKET_ERROR		2
#define	LECROY_ERR_WRITE_COMMAND_ERROR		3
#define	LECROY_ERR_RESPONSE_PROTOCOL_ERR	4
#define	LECROY_ERR_RESPONSE_MALLOC_ERR		5
#define	LECROY_ERR_RESPONSE_DEADLOOP		6
#define	LECROY_ERR_INIT_SOCKET_ERR		7
#define	LECROY_ERR_INIT_CONN_REFUSED		8
#define	LECROY_ERR_INIT_CONN_TIMEOUT		9
#define	LECROY_ERR_INIT_INITSCOPE_ERR		10
#define	LECROY_ERR_INIT_RDTMPL_ERR		11
#define	LECROY_ERR_INIT_TMPL_UNSPT		12
#define	LECROY_ERR_INIT_IDN_ERR			13
#define	LECROY_ERR_INIT_CHNLSTAT_ERR		14
#define	LECROY_ERR_OPEN_MONTASK_ERR		15
#define	LECROY_ERR_RECOVER_TOO_OFTEN		16
#define	LECROY_ERR_READWF_CHNLNUM_ERR		17
#define	LECROY_ERR_READWF_CHNL_DISABLED		18
#define	LECROY_ERR_READWF_FAILED		19
#define	LECROY_ERR_IOCTL_CHNLNUM_ERR		20	
#define	LECROY_ERR_IOCTL_CHNL_DISABLED		21
#define	LECROY_ERR_IOCTL_WRONG_MSIZ		22
#define	LECROY_ERR_IOCTL_WRONG_VDIV_CHNL	23
#define	LECROY_ERR_IOCTL_WRONG_TRIG_MODE	24
#define	LECROY_ERR_IOCTL_WRONG_TRIG_SRC		25
#define	LECROY_ERR_IOCTL_WRONG_NVMEM_INDEX	26
#define	LECROY_ERR_IOCTL_UNSUPPORTED_CMD	27
#define	LECROY_ERR_IOCTL_MISUSE_CHNL_ZERO	28
#define	LECROY_ERR_LASTTRGTIME_CHNLNUM_ERR	29
/* To add new_command or new_function, you might want more error numner */

const static char Error_Msg[50][256]=
{
	/*0*/	"No error happened\n",
	/*1*/	"Fail to select socket in LeCroy_Read_Socket, Link Down, either timeout or something wrong\n",
	/*2*/	"Fail to read socket in LeCroy_Read_Socket, Link Down, either timeout or something wrong\n",
	/*3*/	"Fail to write socket in Lecroy_Write_Command, Link Down or something wrong\n",
	/*4*/	"Protocol error in LeCroy_Read_Response, we expect 0x80(0x81),0x1 ..., but we get something else!\n",
	/*5*/	"Fail to malloc memory in LeCroy_Read_Response to receive incoming packet, so we force link dow!\n",
	/*6*/	"Something causes dead loop in LeCroy_Read_Response, so we force link dow!\n",
	/*7*/	"Fail to create socket in LeCroy_Init!\n",
	/*8*/	"Try to connect to scope in LeCroy_Init, but refused by peer!\n",
	/*9*/	"Try to connect to scope in LeCroy_Init, but timeout!\n",
	/*10*/	"Failed to init scope in LeCroy_Init!\n",
	/*11*/	"Failed to read template from scope in LeCroy_Init!\n",
	/*12*/	"Found unsupported template in LeCroy_Init!\n",
	/*13*/	"Failed to read model info from scope in LeCroy_Init!\n",
	/*14*/	"Failed to read channel status from scope in LeCroy_Init!\n",
	/*15*/	"Failed to spawn a link monitor task in LeCroy_Open!\n",
	/*16*/	"Interval is smaller than timeout in LeCroy_Recover_Link!\n",
	/*17*/	"Channel number is out of range in LeCroy_Read!\n",
	/*18*/	"Try to operate a disabled channel in Lecroy_Read!\n",
	/*19*/	"Fail to retrieve waveform in LeCroy_Read!\n",
	/*20*/	"Channel number is out of range in LeCroy_Ioctl!\n",
	/*21*/	"Try to operate a disabled channel in Lecroy_Ioctl!\n",
	/*22*/	"Memory size ERROR in LeCroy_Ioctl!\n",
	/*23*/	"VDIV command is only good for channel C1 -> C4 in LeCroy_Ioctl\n",
	/*24*/	"Trigger mode is illegal in LeCroy_Ioctl\n",
	/*25*/	"Trigger source is illegal in LeCroy_Ioctl\n",
	/*26*/	"Nvmem index is illegal in LeCroy_Ioctl\n",
	/*27*/	"Unsupported command in LeCroy_Ioctl\n",
	/*28*/	"Try to use channel number 0 for channel related operation in LeCroy_Ioctl\n",
	/*29*/	"Channel number is out of range in LeCroy_Get_LastTrgTime!\n"
/* To add new_command or new_function, you might want more error message */
};
#ifndef min
#define min(x, y) (((x) < (y)) ? (x) : (y))
#endif

#ifdef __cplusplus
}
#endif  /* __cplusplus */

#endif
