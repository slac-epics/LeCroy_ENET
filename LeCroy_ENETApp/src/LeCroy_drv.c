/**********************************************************************************/
/**  Author :  Sheng Peng (SNS, ORNL)                                            **/
/**  Contact:  pengs@ornl.gov     865-241-1779                                   **/
/**  Date:     02/24/2005                                                        **/
/**  Version:  V1.6.2                                                            **/
/**  Description: Driver for LeCroy with Ethernet Interface                      **/
/**********************************************************************************/


/**********************************************************************************/
/**  Author :  Sheng Peng (C-AD,BNL) for SNS Project, x8401                      **/
/**  Date:     06/16/2003                                                        **/
/**  Version:  V1.6.1 (This is tagged as R1-6, all further change, see cvs diff) **/
/**  Description: Driver for LeCroy with Ethernet Interface                      **/
/**  Change from V1.4                                                            **/
/**   1. Add read timeout, and link status                                       **/
/**   2. Guarantee read all segments and bytes when read command response        **/
/**   3. Check "0x1" after "0x80"("0x81")                                        **/
/**   4. Change socket option to add TCP_NODELAY and enlarge buffer              **/
/**   5. Turn off all command echo and waveform DEF9 to speed up and easy        **/
/**          to analyze readback                                                 **/
/**   6. Check if the scope is what we can support (template)                    **/
/**   7. Add LeCroy scope model read                                             **/
/**   8. Provide reconnect, and reinit scope when link down                      **/
/**   9. Provide readback linkstat                                               **/
/**   10. Provide readback scope model                                           **/
/**   11. Provide readback IP address                                            **/
/**   12. Support more than 8 bits up to 16 bits                                 **/
/**   13. support RIS and Segment waveform                                       **/
/**   14. *RST wouldn't affect INIT_STRING setting, but will affect other        **/
/**          setting, and update chnlenbl when check chnlstat                    **/
/**   15. Support 8 channnels                                                    **/
/**   16. More strict channel number parameter check for channel 0               **/
/**   17. Change driver function name from LT364 to LeCroy                       **/
/**   18. Use internal error number to take out printf from semaphore protection **/
/**   19. WAVEDESC is packed and aligned to 8 bytes for whole structure          **/
/**          (not per member), and using REALDESCSIZE to memcpy                  **/
/**   20. Add command to Ioctl to handle auto recalibration                      **/
/**   21. Add LeCroy_Get_LastTrgTime function, and add another semaphore         **/
/**                                                                              **/
/**	  Change from V1.5 to V1.6:                                              **/
/**   22. More LINK_OK check in LeCroy_Read_Response and LeCroy_Operate.         **/
/**   23. We can use string tool and memory tool to deal with scope response,    **/
/**           because we attach a '\0' to the end of return memory               **/
/**   24. Add a parameter to enable or disable link monitor in LeCroy_open       **/
/**   25. Delete link monitor task in LeCory_close                               **/
/**   26. Add a definition for LINK_CHECK_ONCE and LINK_CHECK_PRIORITY           **/
/**   27. Handel the difference between 2 channels and 4 channels scope          **/
/**   28. Add a link status LINK_UNSUPPORTED                                     **/
/**   29. Using errnoGet( ) instead of errno for backward compatibility          **/
/**   30. Read back channel status in LeCroy_open and LeCroy_Recover_Link.       **/
/**   31. Change semwavedesc to semOp, use it to protect LeCroyModel as well     **/
/**   32. LeCroy_open returns a valid pointer even fail to talk to scope,        **/
/**           so user has chance to recover it without call LeCroy_open again.   **/
/**   33. Add debug information                                                  **/
/**                                                                              **/
/**   Change from V1.6 t0 V1.6.1                                                 **/
/**   34. LeCroy_Open returns NULL instead of (LeCroyID)ERROR when failed        **/
/**   35. *RST will affect chnlenbl                                              **/
/**   36. UNKNOWN_MODEL should end with '\n'                                     **/
/**   37. only ETIMEDOUT will be LINK_DOWN, all else is LINK_UNSUPPORTED         **/
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

/* includes */
#include "LeCroy_drv.h"

int     LECROY_DRV_DEBUG=0;

struct LocalThreadData
        {
        LeCroyID thr_lecroyid;
        int thr_intervalsec;
        unsigned int thr_toutsec;
        };


/*****  internal low level functions  *****/

/*
 * emulate the vxworks connectWithTimeout() routine by using select() with connect()
 */
static int
connectWithTimeout( int fd, struct sockaddr *addr, int size, struct timeval *timeout)
        {
        int arg = 0;
        fd_set writeFds;

        FD_ZERO (&writeFds);
        FD_SET (fd, &writeFds);

        /* (void)ioctl( fd, FIOSNBIO, arg); */
        (void)fcntl( fd, O_NONBLOCK, arg);

        if( connect( fd, addr, size) < 0)
                {
                if( errno != EINPROGRESS)
                        return ERROR;
                if( select( fd + 1, NULL, &writeFds, NULL, timeout) <= 0)
                        return ERROR;
                }

        return OK;
        }



/** this function will read all wanted data from socket with timeout(second) **/
/** and set link status and return link status **/
/** It is only called by LeCroy_Read_Response, so we don't do parameters check **/
static int LeCroy_Read_Socket(LeCroyID lecroyid, char * pbuffer, int size, unsigned int toutsec)
{
        fd_set          readFds;
	struct timeval	timeout;
	int		wantnumber;	/* how many bytes we are still wating for */
	int		gotnumber;	/* how many bytes we got this time */

	if(lecroyid->linkstat!=LINK_OK) 
		return lecroyid->linkstat;

	wantnumber=size;
	gotnumber=0;

	while(wantnumber)
	{
		/* clear bits in read bit mask */
		FD_ZERO (&readFds);
		/* initialize bit mask */
		FD_SET (lecroyid->sFd, &readFds);

		timeout.tv_sec=toutsec;
		timeout.tv_usec=0;

		/* pend, waiting for socket to be ready or timeout */
		if (select (lecroyid->sFd+1, &readFds, NULL, NULL, &timeout) <= 0)
		{/* timeout or ERROR */
			lecroyid->linkstat=LINK_DOWN;
			close(lecroyid->sFd);
			lecroyid->sFd= ERROR;
			lecroyid->lasterr=LECROY_ERR_SELECT_SOCKET_TIMEOUT;
			return lecroyid->linkstat;
		}
		/* we don't need FD_ISSET, because we only check one sFd */
		if ((gotnumber=read(lecroyid->sFd,pbuffer+size-wantnumber,wantnumber)) <= 0)
		{/* error from read() */
			lecroyid->linkstat=LINK_DOWN;
			close(lecroyid->sFd);
			lecroyid->sFd= ERROR;
			lecroyid->lasterr=LECROY_ERR_READ_SOCKET_ERROR;
			return lecroyid->linkstat;
		}
		wantnumber=wantnumber-gotnumber;
	}
	return lecroyid->linkstat;
}

/** this function will send all data via socket **/
/** and set link status and return link status  **/
/** It is only called by LeCroy_Operate, so we don't do parameters check **/
/** We only write couple bytes command, so we don't select to timeout    **/
static int LeCroy_Write_Command(LeCroyID lecroyid, char * pCommand)
{
	char	* pCMD;	/* buffer to send out command */
	int	CMDLEN;
	
	if(lecroyid->linkstat!=LINK_OK) 
		return lecroyid->linkstat;

	/* buffer size should be command size + header size(8) and \0 */
	pCMD=(char *)malloc(strlen(pCommand)+8+1);

	pCMD[0]='\x81';
	pCMD[1]='\x1';
	pCMD[2]='\x0';
	pCMD[3]='\x0'; /* only one frame will be send to scope */

	/* if we don't call htonl,this is only good for BIG_ENDIAN platform */
	/* on LITTLE_ENDIAN platform, you have to call LONGSWAP */
	/* because even you set CORD LO; don't affect header */
	*((long int *)(pCMD+4))=htonl(strlen(pCommand));
	
	strcpy(pCMD+8,pCommand);
	
	CMDLEN=strlen(pCommand)+8;	/* don't send out \0 */
	
	if(write(lecroyid->sFd, pCMD, CMDLEN)!=CMDLEN)
	{/* error from write() */
		lecroyid->linkstat=LINK_DOWN;
		close(lecroyid->sFd);
		lecroyid->sFd= ERROR;
		lecroyid->lasterr=LECROY_ERR_WRITE_COMMAND_ERROR;
	}
	
	free(pCMD);

	return lecroyid->linkstat;
}

/** this function will read whole response from scope and set link status      **/
/** and return link status, user has to call free(*ppbuf) after successfully   **/
/** called this function, toutsec is in second and just for every socket read, **/
/** not for whole this function, this function might do many times socker read **/
/** It is only called by LeCroy_Operate, so we don't do parameters check       **/
static int LeCroy_Read_Response(LeCroyID lecroyid, char ** ppbuf, int * psize, unsigned int toutsec)
{
	int		loop;		/*loop control*/
	
	unsigned char 	header[8];
	
	unsigned int	packetsize=0;	/* size of incoming packet */
	
	char		* polddata;
	char		* pdata;

	unsigned int	olddatasize=0;	/* data size is real memory size minus one */
	unsigned int	datasize=0;	/* data size is real memory size minus one */

	if(lecroyid->linkstat!=LINK_OK) 
		return lecroyid->linkstat;	/* not so necessary, but for saving time */

	bzero((char *)header,8);

	pdata=(char *)malloc(1); /* we keep the read back ends with '\0', so following function can use string tool */
	pdata[0]='\0';
	datasize=0;	/* not necessary here, but just for easy reading */

	for(loop=0;loop<1000;loop++)	/* just for avoid dead loop, loop will end on 0x81 */
	{
		polddata=pdata;
		olddatasize=datasize;

		/*read head just like 0x80(0x81),1,0,0,4 bytes length*/
		
		if(LeCroy_Read_Socket(lecroyid, (char *)header, 8, toutsec)!=LINK_OK)
		{
			free(polddata);
			return	lecroyid->linkstat;
		}

		if( (header[0]&0xFE)!=0x80||header[1]!=0x1)
		{
			lecroyid->linkstat=LINK_DOWN;
			close(lecroyid->sFd);
			lecroyid->sFd= ERROR;
			lecroyid->lasterr=LECROY_ERR_RESPONSE_PROTOCOL_ERR;
			free(polddata);
			return	lecroyid->linkstat;
		}
		
		/* read header so we know the length of following block's length */
		packetsize=ntohl(*((long int *)(header+4)));	/* convert 4 bytes to integer.it is length of block following */

		if((pdata=(char *)malloc(olddatasize+packetsize+1))==NULL)
		{
			lecroyid->linkstat=LINK_DOWN;
			close(lecroyid->sFd);
			lecroyid->sFd= ERROR;
			lecroyid->lasterr=LECROY_ERR_RESPONSE_MALLOC_ERR;
			free(polddata);
			return	lecroyid->linkstat;
		}

		bzero(pdata,olddatasize+packetsize+1);
		memcpy(pdata,polddata,olddatasize);
		free(polddata);

		if(LeCroy_Read_Socket(lecroyid, pdata+olddatasize, packetsize, toutsec)!=LINK_OK)
		{
			free(pdata);
			return	lecroyid->linkstat;
		}

		datasize=olddatasize+packetsize;
		
		if(header[0]==0x81)	break;
	}
	
	if(loop==1000)
	{
		lecroyid->linkstat=LINK_DOWN;
		close(lecroyid->sFd);
		lecroyid->sFd= ERROR;
		lecroyid->lasterr=LECROY_ERR_RESPONSE_DEADLOOP;
		free(pdata);
		return	lecroyid->linkstat;
	}

	*ppbuf=pdata;
	*psize=datasize;
	return	lecroyid->linkstat;
}
/** call all three functions above must be protected by semaphore **/

/** user has to call free(*pprdbk) after successfully called this function,     **/
/** toutsec is in second and not for whole function, just for each socket read  **/
/** in this function, this function might do many times socket read. If "query" **/
/** is not TRUE, you can specify last three parameters to NULL,NULL,0           **/
static STATUS LeCroy_Operate(LeCroyID lecroyid, char * pCmd, BOOL query, char ** pprdbk, int *prdbksize, unsigned int toutsec)
{
	/* fail to LeCroy_Open, it's not necessary,because we never call this function standalone */ 
	if(lecroyid==NULL) return ERROR;
	/* Parameter check */
	if( pCmd == NULL || strlen(pCmd) == 0 || (query && (pprdbk == NULL || prdbksize == NULL)) )
	{/* This is a static function, it is only called internally, so it must not have parameter issue */
	 /* Once illegal parameter appeared, that is a program issue, we got to fix it */
	 /* We should simply return ERROR. If we do that, we might fail to do some important thing that user wants, */
	 /* but leave linkstat LINK_OK. So we would suspend task, and force to fix bug */
		printf("Illegal parameter when call LeCroy_Operate for scope[%s]!\n", lecroyid->IPAddr);
                epicsThreadSuspendSelf();
		return ERROR;
	}

	epicsMutexLock(lecroyid->semLecroy);

	if(lecroyid->linkstat!=LINK_OK) 
	{/* not so necessary, but for saving time */
		epicsMutexUnlock(lecroyid->semLecroy);
		return	ERROR;
	}

	if(LeCroy_Write_Command(lecroyid, pCmd)!=LINK_OK)
	{
		epicsMutexUnlock(lecroyid->semLecroy);
		return	ERROR;
	}

	if(query)
	{
		if(LeCroy_Read_Response(lecroyid, pprdbk, prdbksize,toutsec)!=LINK_OK)
		{
			epicsMutexUnlock(lecroyid->semLecroy);
			return	ERROR;
		}
	}
							
	epicsMutexUnlock(lecroyid->semLecroy);

	return OK;
}

/* This function will be shared by LeCroy_Open and LeCroy_Recover_Link */
/* toutsec is seconds to timeout connect, for LeCroy_Operate, we use READ_TIMEOUT */
/* This function doesn't use semLecroy inside, caller should judge if it's need */
static STATUS	LeCroy_Init(LeCroyID lecroyid, unsigned int toutsec)
{
	struct	sockaddr_in	serverAddr;	/* server's socket address */
	int			sockAddrSize;	/* size of socket address structure */
	int			optval;		/* for setsockopt */
	struct	timeval		timeout;	/* for connectWithTimeout */

	char			* prdbk;	/* to read back template and IDN and channel status */
	int			rdbksize;	/* to read back template and IDN and channel status */

	int			loop;		/* check all channels' status */
	char			* pLast = NULL;	/* for strtok_r */

	/* fail to LeCroy_Open, it's not necessary,because we never call this function standalone */
	if(lecroyid==NULL)
		return ERROR;

	/* build server socket address */
	sockAddrSize = sizeof (struct sockaddr_in);
	bzero ((char *) &serverAddr, sockAddrSize);
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons (SCOPE_PORT_NUM);
	serverAddr.sin_addr.s_addr = inet_addr (lecroyid->IPAddr);

	/* create client's socket */
	if( (lecroyid->sFd = socket(AF_INET, SOCK_STREAM, 0)) == ERROR )
	{  
		lecroyid->linkstat=LINK_DOWN;
		lecroyid->lasterr=LECROY_ERR_INIT_SOCKET_ERR;
		if(LECROY_DRV_DEBUG)	printf("Create socket failed for %s!\n", lecroyid->IPAddr);
		return ERROR;
	}

	/* Set socket options to make our performance better */ 
	/* force TCP to send out short message immidiately */
	optval=1;
	setsockopt(lecroyid->sFd, IPPROTO_TCP, TCP_NODELAY, (char *)&optval, sizeof (optval));
	/* enlarge receive buffer */
	optval=8192;
	setsockopt(lecroyid->sFd, SOL_SOCKET, SO_RCVBUF, (char *)&optval, sizeof (optval));

	/* bind not required - port number is dynamic */

	/* connect to LeCroy scope */
	timeout.tv_sec=toutsec;
	timeout.tv_usec=0;
	if( connectWithTimeout(lecroyid->sFd, (struct sockaddr *) &serverAddr, sockAddrSize, &timeout) == ERROR)
	{  
		if(LECROY_DRV_DEBUG) perror("Try to connect to scope:");
		if(errno!=ETIMEDOUT)
		{/* no listener on this port, so we got either ECONNREFUSED or ENOTCONN */
			lecroyid->linkstat=LINK_UNSUPPORTED;
			lecroyid->lasterr=LECROY_ERR_INIT_CONN_REFUSED;
			if(LECROY_DRV_DEBUG) printf("Connect was refused by scope[%s]!\n", lecroyid->IPAddr);
		}
		else
		{/* nothing on this IP or scope busy, so we got ETIMEDOUT */
			lecroyid->linkstat=LINK_DOWN;
			lecroyid->lasterr=LECROY_ERR_INIT_CONN_TIMEOUT;
			if(LECROY_DRV_DEBUG) printf("Connect to scope[%s] timeout!\n", lecroyid->IPAddr);
		}
		close (lecroyid->sFd);
		lecroyid->sFd= ERROR;
		return ERROR;
	}
	else
		lecroyid->linkstat=LINK_OK;
	/* reach here, finished setting up tcp connection to LeCroy scope */

	/* initialize scope communication mode */
	if(LeCroy_Operate(lecroyid,INIT_STRING,FALSE,NULL,NULL,0)==ERROR)
	{
		/*lecroyid->linkstat=LINK_DOWN;*/	/* LeCroy_Operate already set it */
		lecroyid->lasterr=LECROY_ERR_INIT_INITSCOPE_ERR;
		/*close (lecroyid->sFd);*/ /* LeCroy_Operate already closed it, it's dangerous to close twice */
		/*lecroyid->sFd= ERROR;*/
		if(LECROY_DRV_DEBUG) printf("Fail to init scope[%s]!\n", lecroyid->IPAddr);
		return ERROR;
	}

	/* readback template */
	if(LeCroy_Operate(lecroyid,TMPL_STRING,TRUE,&prdbk,&rdbksize,READ_TIMEOUT)==ERROR)
	{
		/*lecroyid->linkstat=LINK_DOWN;*/	/* LeCroy_Operate already set it */
		lecroyid->lasterr=LECROY_ERR_INIT_RDTMPL_ERR;
		/*close (lecroyid->sFd);*/ /* LeCroy_Operate already closed it, it's dangerous to close twice */
		/*lecroyid->sFd= ERROR;*/
		if(LECROY_DRV_DEBUG) printf("Fail to read template of scope[%s]!\n", lecroyid->IPAddr);
		return ERROR;
	}

	/* check if we can support this temlpate */
	if(strstr(prdbk,TEMPLATE)==NULL)
	{/* we can't support this template */
		free(prdbk);
		lecroyid->linkstat=LINK_UNSUPPORTED;
		lecroyid->lasterr=LECROY_ERR_INIT_TMPL_UNSPT;
		close (lecroyid->sFd);
		lecroyid->sFd= ERROR;
		if(LECROY_DRV_DEBUG) printf("We can't support the template of the scope[%s]!\n", lecroyid->IPAddr);
		return ERROR;
	}
	else
		free(prdbk);

	/* try to get model information of scope */
	if(LeCroy_Operate(lecroyid,IDN_STRING,TRUE,&prdbk,&rdbksize,READ_TIMEOUT)==ERROR)
	{
		/*lecroyid->linkstat=LINK_DOWN;*/	/* LeCroy_Operate already set it */
		lecroyid->lasterr=LECROY_ERR_INIT_IDN_ERR;
		/*close (lecroyid->sFd);*/ /* LeCroy_Operate already closed it, it's dangerous to close twice */
		/*lecroyid->sFd= ERROR;*/
		if(LECROY_DRV_DEBUG) printf("Fail to read model information of scope[%s]!\n", lecroyid->IPAddr);
		return ERROR;
	}
	else
	{
		epicsMutexLock(lecroyid->semOp); /* Protect LeCroyModel for function like LeCroy_Get_Model */
		strncpy(lecroyid->LeCroyModel,(char *)strstr(prdbk,"LECROY"),MAX_CA_STRING_SIZE-1);
		lecroyid->LeCroyModel[MAX_CA_STRING_SIZE-1]='\0';
		epicsMutexUnlock(lecroyid->semOp);
		free(prdbk);
	}

	/* get all channels' status of scope */
	if(lecroyid->channels==TWO_CHANNEL_SCOPE)
	{/* 2 channels scope */
		if(LeCroy_Operate(lecroyid,CHNLSTAT_STRING_2,TRUE,&prdbk,&rdbksize,READ_TIMEOUT)==ERROR)
		{
			/*lecroyid->linkstat=LINK_DOWN;*/	/* LeCroy_Operate already set it */
			lecroyid->lasterr=LECROY_ERR_INIT_CHNLSTAT_ERR;
			/*close (lecroyid->sFd);*/ /* LeCroy_Operate already closed it, it's dangerous to close twice */
			/*lecroyid->sFd= ERROR;*/
			if(LECROY_DRV_DEBUG) printf("Fail to read current channel status of scope[%s]!\n", lecroyid->IPAddr);
			return ERROR;
		}
	}
	else
	{/* 4 channels scope */
		if(LeCroy_Operate(lecroyid,CHNLSTAT_STRING_4,TRUE,&prdbk,&rdbksize,READ_TIMEOUT)==ERROR)
		{
			/*lecroyid->linkstat=LINK_DOWN;*/	/*LeCroy_Operate already set it */
			lecroyid->lasterr=LECROY_ERR_INIT_CHNLSTAT_ERR;
			/*close (lecroyid->sFd);*/ /* LeCroy_Operate already closed it, it's dangerous to close twice */
			/*lecroyid->sFd= ERROR;*/
			printf("Fail to read scope's current channel status!\n");
			return ERROR;
		}
	}		

	/* readback channels' status successfully, try to get each channel status of scope */
	if(LECROY_DRV_DEBUG) printf("Channel status readback for scope[%s]: %s\n", lecroyid->IPAddr, prdbk);
	for(loop=0;loop<TOTALCHNLS;loop++)	/* for channel 1~8, index is 0~7 */
	{
		if(lecroyid->channels==TWO_CHANNEL_SCOPE && (loop==2 || loop==3 || loop==6 || loop==7))
			continue; /* for 2 channels scope, we have no C3;C4;TC;TD */
		if(strstr( strtok_r((loop==0?prdbk:NULL), ";", &pLast), "ON" )==NULL)
		{
			lecroyid->chanenbl[loop]=OFF;
		}
		else
		{
			lecroyid->chanenbl[loop]=ON;
		}
	}
	free(prdbk);
	if(LECROY_DRV_DEBUG)
		printf("Scope[%s] channel status: %d,%d,%d,%d,%d,%d,%d,%d\n", lecroyid->IPAddr,
			lecroyid->chanenbl[0],lecroyid->chanenbl[1],lecroyid->chanenbl[2],lecroyid->chanenbl[3],
			lecroyid->chanenbl[4],lecroyid->chanenbl[5],lecroyid->chanenbl[6],lecroyid->chanenbl[7]);

	return OK;
}

/*****  internal low level functions finished *****/

/** user visible driver functions **/

/* this function keeps monitoring and trying to recover losted link to lecroy scope every interval secs */
/* there is a timeout for connect, so interval must bigger that timeout */
/* if interval equal -1, we just do it once */
STATUS
LeCroy_Recover_Link( LeCroyID lecroyid, int intervalsec, unsigned int toutsec)
{
	BOOL	tryonce;	/* keep monitoring or try recovery once */

	if( lecroyid==NULL) return ERROR; /* fail to LeCroy_Open */

	if(intervalsec!=LINK_CHECK_ONCE && intervalsec<toutsec)
	{
		lecroyid->lasterr=LECROY_ERR_RECOVER_TOO_OFTEN;
		return	ERROR;
	}

	if(intervalsec==LINK_CHECK_ONCE) tryonce=TRUE;
	else	tryonce=FALSE;

	do
	{
		if(!tryonce)	epicsThreadSleep((double)intervalsec);

		/* we just need semaphore to protect state change from LINK_DOWN(LINK_RECOVER)->LINK_OK
		   to avoid all low level read write function return a misleading result, but for safety,
		   we use semaphore to protect whole procedure, to avoid to long hold of connect, we use
		   connectWithTimeout */
		epicsMutexLock(lecroyid->semLecroy);

		if(lecroyid->linkstat==LINK_DOWN || lecroyid->linkstat==LINK_UNSUPPORTED)
		{/* if link down, we wanna recover, even unsupported, we wanna too, because user may change scope */
		 /* reach here we know socket already closed, we don't want to close socket here, */
		 /* cause we may close more than once, that could close something else */
			lecroyid->linkstat=LINK_RECOVER;
			LeCroy_Init(lecroyid, toutsec);
		}

		epicsMutexUnlock(lecroyid->semLecroy);
	}while(!tryonce);

	if(lecroyid->linkstat==LINK_OK) return	OK;
	else	return ERROR;
}

/** This function should be called only once for one scope **/
/** because scope won't accept concurrent connection **/
static void
LeCroyRecoverLinkThread( void *parm)
        {
        struct LocalThreadData *thrdatap = (struct LocalThreadData *)parm;

	if( parm != NULL)
                {
                if( thrdatap->thr_lecroyid != NULL)
                        (void)LeCroy_Recover_Link( thrdatap->thr_lecroyid, thrdatap->thr_intervalsec, thrdatap->thr_toutsec);
                free( parm);
                }
        return;
        }

LeCroyID	LeCroy_Open(char * ipaddr, int channels, BOOL auto_link_recover)
{/* we don't use lasterr here because lasterr is in structure */
	LeCroyID lecroyid;
        struct LocalThreadData *thrdatap;

	char	MonTaskName[80];

	if( sizeof(struct WAVEDESC) != EXPDDESCSIZE )
	{
		printf("Please check your compiler, looks like packed option doesn't work!");
                epicsThreadSuspendSelf();
		return(NULL);
	}

	/* parameters check */
	if( ipaddr == NULL || strlen(ipaddr) == 0 || inet_addr(ipaddr) == ERROR )
	{
		printf("Please give a legal IP address!\n");
		return(NULL);
	}

	if(channels!=TWO_CHANNEL_SCOPE && channels!=FOUR_CHANNEL_SCOPE)
	{
		printf("Illegal total channels for scope[%s], must be either 2(TWO_CHANNEL_SCOPE) or 4(FOUR_CHANNEL_SCOPE)!\n", ipaddr);
		return(NULL);
	}

	/* malloc memory */
	if((lecroyid=(LeCroyID)malloc(sizeof(struct LECROY)))==NULL)	
	{/* we have to have piece of memory to hold this structure, or else no way to recover */
		printf("Malloc memory for scope[%s] failed!\n", ipaddr);
		return(NULL);
	}
	bzero ((char *) lecroyid,sizeof(struct LECROY));

	/* init all control varibles to default value */
	lecroyid->sFd= ERROR; /* will be initialized later */
	lecroyid->linkstat=LINK_DOWN; /* not necessary, already is 0 */
	lecroyid->MonTaskID=(epicsThreadId)ERROR; /* so if we don't spawn a task, LeCroy_close wouldn't try to delete a task */
	/* lecroyid->IPAddr will be initialized later, current is all 0,no default */
	strcpy(lecroyid->LeCroyModel,UNKNOWN_MODEL);
	lecroyid->channels=channels; /* 2 or 4 channels scope */
	lecroyid->lasterr=LECROY_ERR_NO_ERROR; /* not necessary, already is 0 */
	lecroyid->semLecroy=epicsMutexCreate();
	/* lecroyid->chanenbl will be initialized later, current is all disabled,no default */
	/* lecroyid->channel_desc will be initialized later, current is all 0,no default */
	lecroyid->semOp=epicsMutexCreate();

	if(lecroyid->semLecroy == NULL || lecroyid->semOp == NULL)
	{
		if(lecroyid->semLecroy)	epicsMutexDestroy(lecroyid->semLecroy);
		if(lecroyid->semOp)	epicsMutexDestroy(lecroyid->semOp);
		free(lecroyid);
		printf("Fail to create semaphore for scope[%s]!\n", ipaddr);
		return(NULL);
	}

	/* save IP address */
	strncpy(lecroyid->IPAddr, ipaddr, MAX_CA_STRING_SIZE-1);
	lecroyid->IPAddr[MAX_CA_STRING_SIZE-1]='\0';	/* actually IP never longer than 20 */

	/* here we can already goto SpawnLinkMonitor, then recover will be able to re-connect to scope */

	/* Init connection immediately */
	LeCroy_Init(lecroyid, CONN_TIMEOUT);

	/* vxWorks "i" shell command will display only 11 charactors for task name */
	/* for scope with IP 130.199.123.234, we will have a name "L_M_123.234" */
	if(auto_link_recover)
	{
		strcpy(MonTaskName,"L_M_");
		strcat(MonTaskName,strchr( (strchr(ipaddr,'.')+1), '.') +1);

                if(( thrdatap = (struct LocalThreadData *)malloc( sizeof *thrdatap)) == NULL)
                        {
			printf("Cannot allocate memory for new task\n");
                        return NULL;
                        }
                thrdatap->thr_lecroyid = lecroyid;
                thrdatap->thr_intervalsec = LINK_CHECK_INTERVAL;
                thrdatap->thr_toutsec = CONN_TIMEOUT;

		lecroyid->MonTaskID = epicsThreadCreate( MonTaskName, LINK_CHECK_PRIORITY, 20 * 1024, LeCroyRecoverLinkThread, (void *)thrdatap);

		if(lecroyid->MonTaskID==(epicsThreadId)ERROR)
		{
			lecroyid->lasterr=LECROY_ERR_OPEN_MONTASK_ERR;
			printf("Fail to spawn link monitor task for scope[%s], you have to use manual recover!\n", ipaddr);
		}
	}

	return	lecroyid;
}

STATUS	LeCroy_Get_LinkStat(LeCroyID lecroyid, int *plinkstat)
{
	if(lecroyid==NULL) return ERROR; /* fail to LeCroy_Open */

	*plinkstat=lecroyid->linkstat;
	return OK;
}

/* model must be a char array equal or bigger than 40 bytes */
STATUS	LeCroy_Get_Model(LeCroyID lecroyid, char * model)
{
	if(lecroyid==NULL) return ERROR; /* fail to LeCroy_Open */

	epicsMutexLock(lecroyid->semOp);
	strcpy(model,lecroyid->LeCroyModel);
	epicsMutexUnlock(lecroyid->semOp);

	if(lecroyid->linkstat!=LINK_OK)
		return	ERROR;	/* user might change scope, so return ERROR to tell user that model is not trustable */
	else
		return OK;
}

/* ipaddr must be a char array equal or bigger than 40 bytes */
STATUS	LeCroy_Get_IPAddr(LeCroyID lecroyid, char * ipaddr)
{
	if(lecroyid==NULL) return ERROR; /* fail to LeCroy_Open */

	strcpy(ipaddr,lecroyid->IPAddr);
	return OK;
}

/* chnl is 1~8 mapping to array index 0~7, so we use chnl-1 to access array */
int LeCroy_Read(LeCroyID lecroyid, int chnl, float *pwaveform, int pts)
{  
	char			CMD[40];
	char			* prdbk;	/* to read back response */
	int			rdbksize;

	char			* pWaveDesc;
	signed char		* pWaveDataB;	/* if it's 8 bits waveform */
	signed short int	* pWaveDataW;	/* if it's 16 bits waveform */

	int			wflength=0;

	int			cploop;		/*for copy data to float array*/
	
	if(lecroyid==NULL) return ERROR; /* fail to LeCroy_Open */
	
	if(lecroyid->channels==FOUR_CHANNEL_SCOPE && (chnl<1||chnl>TOTALCHNLS) )
	{
		lecroyid->lasterr=LECROY_ERR_READWF_CHNLNUM_ERR;
		return(ERROR);
	}

	if(lecroyid->channels==TWO_CHANNEL_SCOPE && chnl!=1 && chnl!=2 && chnl!=5 && chnl!=6)
	{
		lecroyid->lasterr=LECROY_ERR_READWF_CHNLNUM_ERR;
		return(ERROR);
	}

	epicsMutexLock(lecroyid->semLecroy); /* still need it cause we will touch struct */


	if(lecroyid->chanenbl[chnl-1]!=ON) 
	{/* save network bandwith */
		lecroyid->lasterr=LECROY_ERR_READWF_CHNL_DISABLED;
		epicsMutexUnlock(lecroyid->semLecroy);
		return	ERROR;
	}
	
	bzero(CMD,40);
	strcpy(CMD,ChannelName[chnl-1]);	/* put Cx: */
	strcat(CMD,"WF?");

	if (LeCroy_Operate(lecroyid,CMD,TRUE,&prdbk,&rdbksize,READ_TIMEOUT)==ERROR)
	{  
		lecroyid->lasterr=LECROY_ERR_READWF_FAILED;
		epicsMutexUnlock(lecroyid->semLecroy);
		return (ERROR);
	} 

	/* the first 8 bytes of WAVEDESC block is always "WAVEDESC" */
	pWaveDesc=(char *)strstr(prdbk,"WAVEDESC");

	epicsMutexLock(lecroyid->semOp); /* Protect WAVEDESC for function like LeCroy_Get_LastTrgTime */

	/* even on little endian platform, if you use CORD LO;, this will be still good */
	memcpy( &(lecroyid->channel_desc[chnl-1]), pWaveDesc, REALDESCSIZE/*sizeof(struct WAVEDESC)*/);
	/* actually we can release semOp here, just for saver we release it later */
	wflength=lecroyid->channel_desc[chnl-1].LAST_VALID_PNT-lecroyid->channel_desc[chnl-1].FIRST_VALID_PNT+1;
	

	pWaveDataB=(signed char *)pWaveDesc + lecroyid->channel_desc[chnl-1].WAVE_DESCRIPTOR
										+ lecroyid->channel_desc[chnl-1].USER_TEXT
										+ lecroyid->channel_desc[chnl-1].RES_DESC1
										+ lecroyid->channel_desc[chnl-1].TRIGTIME_ARRAY
										+ lecroyid->channel_desc[chnl-1].RIS_TIME_ARRAY
										+ lecroyid->channel_desc[chnl-1].RES_ARRAY1;
	pWaveDataW=(signed short int *)pWaveDataB;

	if(lecroyid->channel_desc[chnl-1].COMM_TYPE==0)
	{/* byte mode, 8 bits resolution,all signed */
		
		/*copy data to float array*/
		for(cploop=0;cploop<min(pts,wflength);cploop++)
		/* for(cploop=0;cploop<pts;cploop++) */
		{
			pwaveform[cploop]=pWaveDataB[cploop+lecroyid->channel_desc[chnl-1].FIRST_VALID_PNT]
				*lecroyid->channel_desc[chnl-1].VERTICAL_GAIN-lecroyid->channel_desc[chnl-1].VERTICAL_OFFSET;
		}
	}
	else
	{/* word mode, up to 16 bits resolution,all signed */
		
		/*copy data to float array*/
		for(cploop=0;cploop<min(pts,wflength);cploop++)
		{
			pwaveform[cploop]=pWaveDataW[cploop+lecroyid->channel_desc[chnl-1].FIRST_VALID_PNT]
				*lecroyid->channel_desc[chnl-1].VERTICAL_GAIN-lecroyid->channel_desc[chnl-1].VERTICAL_OFFSET;
		}
	}

	epicsMutexUnlock(lecroyid->semOp); /* Protect WAVEDESC for function like LeCroy_Get_LastTrgTime */

	free(prdbk);

	epicsMutexUnlock(lecroyid->semLecroy);

	return (min(pts,wflength));
}  

/* chnl number 0 is used for non-channel related operation */
/* chnl is 1~8 mapping to array index 0~7, so we use chnl-1 to access array */
/* If you don't use CHDR OFF, you have to modify code to analyze rdbk */
STATUS	LeCroy_Ioctl(LeCroyID lecroyid, int chnl, int op, void * parg)
{
	char		CMD[100];
	char		* prdbk;
	int		rdbksize;

	int		loop;	/* for readback comparasion */
	
	if(lecroyid==NULL) return ERROR; /* fail to LeCroy_Open */ 

	/* chnl 0 will be used for non-channel-related options */
	if( lecroyid->channels==FOUR_CHANNEL_SCOPE && (chnl<0||chnl>TOTALCHNLS) )
	{
		lecroyid->lasterr=LECROY_ERR_IOCTL_CHNLNUM_ERR;
		return(ERROR);
	}

	if(lecroyid->channels==TWO_CHANNEL_SCOPE && chnl!=0 && chnl!=1 && chnl!=2 && chnl!=5 && chnl!=6)
	{
		lecroyid->lasterr=LECROY_ERR_IOCTL_CHNLNUM_ERR;
		return(ERROR);
	}

	epicsMutexLock(lecroyid->semLecroy);

	if(chnl!=0&&op!=ENABLECHAN&&op!=GETCHANSTAT)
	{
		if(lecroyid->chanenbl[chnl-1]!=ON) 
		{/* Save bandwith */
			lecroyid->lasterr=LECROY_ERR_IOCTL_CHNL_DISABLED;
			epicsMutexUnlock(lecroyid->semLecroy);
			return	ERROR;
		}
	}
	
	bzero(CMD,100);

	switch(op)
	{
	case RESET:	/* non-channel related operation */
		if(LeCroy_Operate(lecroyid,"*RST",FALSE,NULL,NULL,0)==ERROR)
		{
			epicsMutexUnlock(lecroyid->semLecroy);
			return	ERROR;
		}
		lecroyid->chanenbl[0]=ON;
		lecroyid->chanenbl[1]=ON;
		lecroyid->chanenbl[2]=OFF;
		lecroyid->chanenbl[3]=OFF;
		lecroyid->chanenbl[4]=OFF;
		lecroyid->chanenbl[5]=OFF;
		lecroyid->chanenbl[6]=OFF;
		lecroyid->chanenbl[7]=OFF;	/* *RST woll load default panel, that has only first two channels enabled */
		break;

	case ENABLECHAN:
		if(chnl==0)
		{
			lecroyid->lasterr=LECROY_ERR_IOCTL_MISUSE_CHNL_ZERO;
			epicsMutexUnlock(lecroyid->semLecroy);
			return(ERROR);
		}

		strcpy(CMD,ChannelName[chnl-1]);
		strcat(CMD,"TRA ON");
		if(LeCroy_Operate(lecroyid,CMD,FALSE,NULL,NULL,0)==ERROR)
		{
			epicsMutexUnlock(lecroyid->semLecroy);
			return	ERROR;
		}
		lecroyid->chanenbl[chnl-1]=ON;
		break;

	case DISABLECHAN:
		if(chnl==0)
		{
			lecroyid->lasterr=LECROY_ERR_IOCTL_MISUSE_CHNL_ZERO;
			epicsMutexUnlock(lecroyid->semLecroy);
			return(ERROR);
		}

		strcpy(CMD,ChannelName[chnl-1]);
		strcat(CMD,"TRA OFF");
		if(LeCroy_Operate(lecroyid,CMD,FALSE,NULL,NULL,0)==ERROR)
		{
			epicsMutexUnlock(lecroyid->semLecroy);
			return	ERROR;
		}
		lecroyid->chanenbl[chnl-1]=OFF;
		break;

	case GETCHANSTAT:
		if(chnl==0)
		{
			lecroyid->lasterr=LECROY_ERR_IOCTL_MISUSE_CHNL_ZERO;
			epicsMutexUnlock(lecroyid->semLecroy);
			return(ERROR);
		}

		strcpy(CMD,ChannelName[chnl-1]);
		strcat(CMD,"TRA?");
		if(LeCroy_Operate(lecroyid,CMD,TRUE,&prdbk,&rdbksize,READ_TIMEOUT)==ERROR)
		{
			epicsMutexUnlock(lecroyid->semLecroy);
			return	ERROR;
		}
		if(strstr(prdbk,"ON")==NULL)
		{
			lecroyid->chanenbl[chnl-1]=OFF;
			*(int *)parg=OFF;
		}
		else
		{
			lecroyid->chanenbl[chnl-1]=ON;
			*(int *)parg=ON;
		}
		free(prdbk);
		break;

	case SETMEMSIZE:	/* non-channel related operation, see header file */
		if((*(int *)parg)>13||(*(int *)parg)<0)
		{
			lecroyid->lasterr=LECROY_ERR_IOCTL_WRONG_MSIZ;
			epicsMutexUnlock(lecroyid->semLecroy);
			return	ERROR;
		}

		if(LeCroy_Operate(lecroyid,msiz_op[*(int *)parg].cmd,FALSE,NULL,NULL,0)==ERROR)
		{
			epicsMutexUnlock(lecroyid->semLecroy);
			return	ERROR;
		}
		break;

	case GETMEMSIZE:	/* non-channel related operation, see header file */
		if(LeCroy_Operate(lecroyid,"MSIZ?",TRUE,&prdbk,&rdbksize,READ_TIMEOUT)==ERROR)
		{
			epicsMutexUnlock(lecroyid->semLecroy);
			return	ERROR;
		}
		for(loop=0;loop<14;loop++)
		{
			if(strncmp(prdbk,msiz_op[loop].response,strlen(prdbk)-1)==0)	break;/*minus 1 is because prdbk end with 0x0A*/
		}
                if(loop < 14)
		{
                    *(int *)parg=msiz_op[loop].val;
                }
                else
                {
                    float ftempval; 
                    sscanf(prdbk, "%g", &ftempval);
                    *(int *)parg=(int)ftempval;
                }
		free(prdbk);
		break;

	case SETTIMEDIV:	/* non-channel related operation */
		sprintf(CMD,"TDIV %eS",*(float *)parg);
		if(LeCroy_Operate(lecroyid,CMD,FALSE,NULL,NULL,0)==ERROR)
		{
			epicsMutexUnlock(lecroyid->semLecroy);
			return	ERROR;
		}
		break;

	case GETTIMEDIV:	/* non-channel related operation */
		if(LeCroy_Operate(lecroyid,"TDIV?",TRUE,&prdbk,&rdbksize,READ_TIMEOUT)==ERROR)
		{
			epicsMutexUnlock(lecroyid->semLecroy);
			return	ERROR;
		}
		sscanf(prdbk,"%e",(float *)parg);
		free(prdbk);
		break;

	case SETVOLTDIV:	/* this command is not good for math channel */
		if(chnl<1||chnl>4)
		{
			lecroyid->lasterr=LECROY_ERR_IOCTL_WRONG_VDIV_CHNL;
			epicsMutexUnlock(lecroyid->semLecroy);
			return(ERROR);
		}

		sprintf(CMD,"C0:VDIV %eV",*(float *)parg);
		CMD[1]+=chnl;

		if(LeCroy_Operate(lecroyid,CMD,FALSE,NULL,NULL,0)==ERROR)
		{
			epicsMutexUnlock(lecroyid->semLecroy);
			return	ERROR;
		}
		break;

	case GETVOLTDIV:	/* this command is not good for math channel */
		if(chnl<1||chnl>4)
		{
			lecroyid->lasterr=LECROY_ERR_IOCTL_WRONG_VDIV_CHNL;
			epicsMutexUnlock(lecroyid->semLecroy);
			return(ERROR);
		}

		strcpy(CMD,"C0:VDIV?");
		CMD[1]+=chnl;

		if(LeCroy_Operate(lecroyid,CMD,TRUE,&prdbk,&rdbksize,READ_TIMEOUT)==ERROR)
		{
			epicsMutexUnlock(lecroyid->semLecroy);
			return	ERROR;
		}
		sscanf(prdbk,"%e",(float *)parg);
		free(prdbk);
		break;

	case SETTRGMODE:	/* non-channel related operation, see header file */
		if((*(int *)parg)>3||(*(int *)parg)<0)
		{
			lecroyid->lasterr=LECROY_ERR_IOCTL_WRONG_TRIG_MODE;
			epicsMutexUnlock(lecroyid->semLecroy);
			return	ERROR;
		}

		if(LeCroy_Operate(lecroyid,trigger_mode[*(int *)parg].cmd,FALSE,NULL,NULL,0)==ERROR)
		{
			epicsMutexUnlock(lecroyid->semLecroy);
			return	ERROR;
		}
		break;

	case GETTRGMODE:	/* non-channel related operation, see header file */
		if(LeCroy_Operate(lecroyid,"TRMD?",TRUE,&prdbk,&rdbksize,READ_TIMEOUT)==ERROR)
		{
			epicsMutexUnlock(lecroyid->semLecroy);
			return	ERROR;
		}
		for(loop=0;loop<4;loop++)
		{
			if(strncmp(prdbk,trigger_mode[loop].response,strlen(prdbk)-1)==0)	break;/*minus 1 is because prdbk end with 0x0A*/
		}
		*(int *)parg=trigger_mode[loop].val;
		free(prdbk);
		break;

	case SETTRGSRC:	/* non-channel related operation */
		if(lecroyid->channels==FOUR_CHANNEL_SCOPE && ((*(int *)parg)>4||(*(int *)parg)<0) )
		{
			lecroyid->lasterr=LECROY_ERR_IOCTL_WRONG_TRIG_SRC;
			epicsMutexUnlock(lecroyid->semLecroy);
			return	ERROR;
		}
		if(lecroyid->channels==TWO_CHANNEL_SCOPE && ((*(int *)parg)>2||(*(int *)parg)<0) )
		{/* two channel scope has no C3, C4 */
			lecroyid->lasterr=LECROY_ERR_IOCTL_WRONG_TRIG_SRC;
			epicsMutexUnlock(lecroyid->semLecroy);
			return	ERROR;
		}

		strcpy(CMD,"TRSE EDGE,SR,EX,HT,OFF");
		if((*(int *)parg)!=0)
		{
			CMD[13]='C';
			CMD[14]='0'+(*(int *)parg);
		}
		if(LeCroy_Operate(lecroyid,CMD,FALSE,NULL,NULL,0)==ERROR)
		{
			epicsMutexUnlock(lecroyid->semLecroy);
			return	ERROR;
		}
		break;

	case GETTRGSRC:	/* non-channel related operation */
		if(LeCroy_Operate(lecroyid,"TRSE?",TRUE,&prdbk,&rdbksize,READ_TIMEOUT)==ERROR)
		{
			epicsMutexUnlock(lecroyid->semLecroy);
			return	ERROR;
		}
		if(prdbk[8]=='E')	*(int *)parg=0;
		else	*(int *)parg=prdbk[9]-'0';
		free(prdbk);
		break;

	case LDPNLSTP:	/* non-channel related operation */
		if((*(int *)parg)>4||(*(int *)parg)<0)
		{
			lecroyid->lasterr=LECROY_ERR_IOCTL_WRONG_NVMEM_INDEX;
			epicsMutexUnlock(lecroyid->semLecroy);
			return	ERROR;
		}
		strcpy(CMD,"*RCL 0");
		CMD[5]+=*(int *)parg;
		if(LeCroy_Operate(lecroyid,CMD,FALSE,NULL,NULL,0)==ERROR)
		{
			epicsMutexUnlock(lecroyid->semLecroy);
			return	ERROR;
		}
		break;

	case SVPNLSTP:	/* non-channel related operation */
		if((*(int *)parg)>4||(*(int *)parg)<1)
		{
			lecroyid->lasterr=LECROY_ERR_IOCTL_WRONG_NVMEM_INDEX;
			epicsMutexUnlock(lecroyid->semLecroy);
			return	ERROR;
		}
		strcpy(CMD,"*SAV 0");
		CMD[5]+=*(int *)parg;
		if(LeCroy_Operate(lecroyid,CMD,FALSE,NULL,NULL,0)==ERROR)
		{
			epicsMutexUnlock(lecroyid->semLecroy);
			return	ERROR;
		}
		break;

/* To support new_command, add code below */

/*	DEMO
	case DEMO:
		Parameter (chnl & *parg) check;
		Prepare CMD to send to scope;
		Call LeCroy_Operate, parameter depend on query or not; 
		If query, analyze readback then set *parg and free(prdbk);
		break;
*/

	case ENABLEACAL:	/* non-channel related operation */
		if(LeCroy_Operate(lecroyid,"ACAL ON",FALSE,NULL,NULL,0)==ERROR)
		{
			epicsMutexUnlock(lecroyid->semLecroy);
			return	ERROR;
		}
		break;

	case DISABLEACAL:	/* non-channel related operation */
		if(LeCroy_Operate(lecroyid,"ACAL OFF",FALSE,NULL,NULL,0)==ERROR)
		{
			epicsMutexUnlock(lecroyid->semLecroy);
			return	ERROR;
		}
		break;

	case GETACALSTAT:	/* non-channel related operation */
		if(LeCroy_Operate(lecroyid,"ACAL?",TRUE,&prdbk,&rdbksize,READ_TIMEOUT)==ERROR)
		{
			epicsMutexUnlock(lecroyid->semLecroy);
			return	ERROR;
		}
		if(strstr(prdbk,"ON")==NULL)
			*(int *)parg=OFF;
		else
			*(int *)parg=ON;
		free(prdbk);
		break;

	default:
		lecroyid->lasterr=LECROY_ERR_IOCTL_UNSUPPORTED_CMD;
		epicsMutexUnlock(lecroyid->semLecroy);
		return ERROR;
	}

	epicsMutexUnlock(lecroyid->semLecroy);

	return (OK);

}

/* time should be a char array equal or bigger than 31 bytes */
/* chnl is 1~8 mapping to array index 0~7, so we use chnl-1 to access array */
STATUS	LeCroy_Get_LastTrgTime(LeCroyID lecroyid, int chnl, char * time)
{
	if(lecroyid==NULL) return ERROR; /* fail to LeCroy_Open */ 

	if( lecroyid->channels==FOUR_CHANNEL_SCOPE && (chnl<1||chnl>TOTALCHNLS) )
	{
		lecroyid->lasterr=LECROY_ERR_LASTTRGTIME_CHNLNUM_ERR;
		return(ERROR);
	}

	if( lecroyid->channels==TWO_CHANNEL_SCOPE && chnl!=1 && chnl!=2 && chnl!=5 && chnl!=6 )
	{
		lecroyid->lasterr=LECROY_ERR_LASTTRGTIME_CHNLNUM_ERR;
		return(ERROR);
	}

	epicsMutexLock(lecroyid->semOp);
	
	sprintf(time,"%02d/%02d/%04d,%02d:%02d:%013.10f",	/* totally 30 bytes */
		lecroyid->channel_desc[chnl-1].TRIGGER_TIME.months,
		lecroyid->channel_desc[chnl-1].TRIGGER_TIME.days,
		lecroyid->channel_desc[chnl-1].TRIGGER_TIME.year,
		lecroyid->channel_desc[chnl-1].TRIGGER_TIME.hours,
		lecroyid->channel_desc[chnl-1].TRIGGER_TIME.minutes,
		lecroyid->channel_desc[chnl-1].TRIGGER_TIME.seconds);

	epicsMutexUnlock(lecroyid->semOp);
	if(lecroyid->channel_desc[chnl-1].TRIGGER_TIME.months==0)
		return	ERROR; /* never trigged */
	else
		return	OK;
}

#if 0
/*
 * unused in EPICS, this needs to be rethought for use elsewhere
 */
STATUS	LeCroy_Close(LeCroyID lecroyid)
{
	if(lecroyid==NULL) return ERROR; /* fail to LeCroy_Open */

	/* guarantee no one is using this link */
	epicsMutexLock(lecroyid->semLecroy); /* not so necessary */
	
	if(lecroyid->MonTaskID!=(epicsThreadId)ERROR)	taskDeleteForce(lecroyid->MonTaskID);
	
	if(lecroyid->linkstat==LINK_OK)
	{
		lecroyid->linkstat=LINK_DOWN; /* not so necessary */
		close(lecroyid->sFd);
		lecroyid->sFd= ERROR;
	}
	
	epicsMutexUnlock(lecroyid->semLecroy); /* not so necessary */

	epicsMutexDestroy(lecroyid->semLecroy);
	epicsMutexDestroy(lecroyid->semOp);
	free(lecroyid);
	return OK;
}
#endif /* 0 */

STATUS	LeCroy_Print_Lasterr(LeCroyID lecroyid)
{
	if(lecroyid==NULL)
	{ /* fail to LeCroy_Open */
		printf("This id is not associated with a scope, LeCroy_Open failed!\n");
		return ERROR;
	}
	printf("Scope[%s]: %s!\n", lecroyid->IPAddr, Error_Msg[lecroyid->lasterr]);
	return OK;
}



