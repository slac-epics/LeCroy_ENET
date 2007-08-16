/*********************************************************************/
/**Author :  Sheng Peng (C-AD,BNL) for SNS Project, x8401           **/
/**Date:     06/16/2003                                             **/
/**Version:  V1.6.1                                                   **/
/**Description: control LeCroy Scope with Ethernet Interface        **/
/*********************************************************************/

#ifndef	_INC_LeCroy_DevSup
#define	_INC_LeCroy_DevSup

/*include*/
#if EPICS_VERSION>=3 && EPICS_REVISION>=14

#else
        #include "vxWorks.h"
        #include "semLib.h"
#endif /* EPICS_VERSION>=3 && EPICS_REVISION>=14 */

#define	TWO_CHANNEL_SCOPE	2
#define	FOUR_CHANNEL_SCOPE	4  

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

#if 0
/*
 * currently unused
 */
static struct TRIG_MODE
{
	char	cmd[20];
	char	response[20];
	int	val;
}	trigger_mode[4]={	{"TRMD AUTO",	"AUTO",		AUTO},
				{"TRMD NORM",	"NORM",		NORM},
				{"TRMD SINGLE",	"SINGLE",	SINGLE},
				{"TRMD STOP",	"STOP",		STOP} };
#endif /* 0 */
/* To support new_command, you might want to add new structure here */

typedef struct LECROY	* LeCroyID;

/* to add new_function, add prototype below */

/** This function should be called only once for one scope **/
/** because scope won't accept concurrent connection **/
/** channels should be either 2(TWO_CHANNEL_SCOPE) or 4(FOUR_CHANNEL_SCOPE) up to your scope */
LeCroyID LeCroy_Open(char * ipaddr, int channels, BOOL auto_link_recover);

/* this function keeps monitoring and trying to recover losted link to lecroy scope every interval secs */
/* there is a timeout for connect, so interval must bigger that timeout */
/* if interval equal -1, we just do it once */
STATUS	LeCroy_Recover_Link( LeCroyID lecroyid, int intervalsec, unsigned int toutsec);

STATUS	LeCroy_Get_LinkStat(LeCroyID lecroyid, int *plinkstat);

/* model must be a char array equal or bigger than 40 bytes */
STATUS	LeCroy_Get_Model(LeCroyID lecroyid, char * model);

/* ipaddr must be a char array equal or bigger than 40 bytes */
STATUS	LeCroy_Get_IPAddr(LeCroyID lecroyid, char * ipaddr);

/* chnl is 1~8 */
int LeCroy_Read(LeCroyID lecroyid, int chnl, float *pwaveform, int pts);

/* chnl number 0 is used for non-channel related operation */
STATUS	LeCroy_Ioctl(LeCroyID lecroyid, int chnl, int op, void * parg);

/* time should be a char array equal or bigger than 31 bytes */
STATUS	LeCroy_Get_LastTrgTime(LeCroyID lecroyid, int chnl, char * time);

#if 0
/* unused in EPICS version */
STATUS	LeCroy_Close(LeCroyID lecroyid);
#endif /* 0 */

STATUS	LeCroy_Print_Lasterr(LeCroyID lecroyid);

#ifdef __cplusplus
}
#endif  /* __cplusplus */

#endif
