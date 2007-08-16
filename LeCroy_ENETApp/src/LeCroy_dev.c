/*
   File:        LeCroy_dev.c
   Author:	Seth Nemesure
   Institution: Brookhaven National Laboratory / SNS Project

   EPICS device layer support for LeCroy LT364 Ethernet Interfaced Scope

   Hardware addressing:	
   #Ss Cc @params
   where c - scope ID number, ex. 0, 1, 2 ... 
	 s - Channel number for the scope, ex. 1, 2, 3, 4
	 params - for future use
 
   Modification Log:
   -----------------
   Seth Nemesure:  Added device support for most of the driver
   functions - and converted to asynchronous handling of records
   
*/

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

#include    <stdio.h>
#include    <stdlib.h>
#include    <string.h>
#include    <alarm.h>
#include    <dbDefs.h>
#include    <dbAccess.h>
#include    <dbScan.h>
#include    <recSup.h>
#include    <recGbl.h>
#include    <devSup.h>
#include    <special.h>
#include    <waveformRecord.h>
#include    <boRecord.h>
#include    <biRecord.h>
#include    <mbboRecord.h>
#include    <mbbiRecord.h>
#include    <aoRecord.h>
#include    <aiRecord.h>
#include    <stringinRecord.h>
#include    <iocsh.h>
#include    <epicsVersion.h>

#if     EPICS_VERSION>=3 && EPICS_REVISION>=14
#include    <epicsExport.h>
#endif

#include    "LeCroy_dev.h"

/* define parameter check functions */
#define CHECK_BOPARM(PARM)\
 if (!strncmp(bor->out.value.vmeio.parm, (PARM), strlen(PARM))){\
       DPVT_DATA* data = (DPVT_DATA*) malloc(sizeof(DPVT_DATA));\
       data->bufSize = 0;   /* not used */\
       data->buffer = NULL; /* not used */\
       if (strstr(bor->out.value.vmeio.parm,"reset"))\
          data->deviceId = LT_BO_RESET;\
       else if (strstr(bor->out.value.vmeio.parm,"enbl"))\
          data->deviceId = LT_BO_ENBLCH;\
       else if (strstr(bor->out.value.vmeio.parm,"autocalS"))\
          data->deviceId = LT_BO_AUTOCAL;\
       else\
          data->deviceId = LT_BO_RECOVER;\
       bor->dpvt=(void*)data;\
       bor->pact=FALSE;\
       paramOK = 1;\
 }

#define CHECK_BIPARM(PARM)\
 if (!strncmp(bir->inp.value.vmeio.parm, (PARM), strlen((PARM)))){\
       DPVT_DATA* data = (DPVT_DATA*) malloc(sizeof(DPVT_DATA));\
       data->bufSize = 0;   /* not used */\
       data->buffer = NULL; /* not used */\
       if (strstr(bir->inp.value.vmeio.parm, "statusch"))\
         data->deviceId = LT_BI_CHSTAT;\
       else if (strstr(bir->inp.value.vmeio.parm, "autocalM"))\
	 data->deviceId = LT_BI_AUTOCAL;\
       bir->dpvt=(void*)data;\
       return (0);\
 }

#define CHECK_MBBIPARM(PARM)\
 if (!strncmp(mbbir->inp.value.vmeio.parm, (PARM), strlen((PARM)))){\
       DPVT_DATA* data = (DPVT_DATA*) malloc(sizeof(DPVT_DATA));\
       data->bufSize = 0;   /* not used */\
       data->buffer = NULL; /* not used */\
       if (!strcmp(mbbir->inp.value.vmeio.parm, "memsizeM"))\
          data->deviceId = LT_MBBI_MEMSZ;\
       else if (!strcmp(mbbir->inp.value.vmeio.parm, "trgmodeM"))\
          data->deviceId = LT_MBBI_TRGMD;\
       else if (!strcmp(mbbir->inp.value.vmeio.parm, "trgsrcM"))\
          data->deviceId = LT_MBBI_TRGSRC;\
       else if (!strcmp(mbbir->inp.value.vmeio.parm, "linkstatusM"))\
          data->deviceId = LT_MBBI_LINKSTATUS;\
       mbbir->dpvt=(void*) data;\
       return (0);\
 }

#define CHECK_MBBOPARM(PARM)\
 if (!strncmp(mbbor->out.value.vmeio.parm, (PARM), strlen((PARM)))){\
       DPVT_DATA* data = (DPVT_DATA*) malloc(sizeof(DPVT_DATA));\
       data->bufSize = 0;   /* not used */\
       data->buffer = NULL; /* not used */\
       if (!strcmp(mbbor->out.value.vmeio.parm, "memsizeS"))\
          data->deviceId = LT_MBBO_MEMSZ;\
       else if (!strcmp(mbbor->out.value.vmeio.parm, "trgmodeS"))\
          data->deviceId = LT_MBBO_TRGMD;\
       else if (!strcmp(mbbor->out.value.vmeio.parm, "trgsrcS"))\
          data->deviceId = LT_MBBO_TRGSRC;\
       else if (!strcmp(mbbor->out.value.vmeio.parm, "ldpnlstp"))\
          data->deviceId = LT_MBBO_LDPNLSTP;\
       else if (!strcmp(mbbor->out.value.vmeio.parm, "svpnlstp"))\
          data->deviceId = LT_MBBO_SVPNLSTP;\
       mbbor->dpvt=(void*)data;\
       paramOK=1;\
 }

#define CHECK_AIPARM(PARM)\
 if (!strncmp(air->inp.value.vmeio.parm, (PARM), strlen(PARM))){\
       DPVT_DATA* data = (DPVT_DATA*) malloc(sizeof(DPVT_DATA));\
       data->bufSize = 0;   /* not used */\
       data->buffer = NULL; /* not used */\
       if (!strcmp(air->inp.value.vmeio.parm, "timedivM"))\
          data->deviceId = LT_AI_TIMEDIV;\
       else if (strstr(air->inp.value.vmeio.parm, "voltdiv"))\
          data->deviceId = LT_AI_VOLTDIV;\
       air->dpvt=(void*) data;\
       return (0);\
 }

#define CHECK_AOPARM(PARM)\
 if (!strncmp(aor->out.value.vmeio.parm, (PARM), strlen((PARM)))){\
       DPVT_DATA* data = (DPVT_DATA*) malloc(sizeof(DPVT_DATA));\
       data->bufSize = 0;   /* not used */\
       data->buffer = NULL; /* not used */\
       if (!strcmp(aor->out.value.vmeio.parm, "timedivS"))\
	       data->deviceId = LT_AO_TIMEDIV;\
       else if (strstr(aor->out.value.vmeio.parm, "voltdiv"))\
	       data->deviceId = LT_AO_VOLTDIV;\
       aor->dpvt=(void*) data;\
       paramOK=1;\
 }

#define CHECK_STRINGIN(PARM)\
 if (!strncmp(stringinr->inp.value.vmeio.parm, (PARM), strlen(PARM))){\
       DPVT_DATA* data = (DPVT_DATA*) malloc(sizeof(DPVT_DATA));\
       data->bufSize = 0;   /* not used */\
       data->buffer = NULL; /* not used */\
       if (!strcmp(stringinr->inp.value.vmeio.parm, "model"))\
          data->deviceId = LT_STRINGIN_MODEL;\
       else if (!strcmp(stringinr->inp.value.vmeio.parm, "serial"))\
          data->deviceId = LT_STRINGIN_SERIAL;\
       else if (!strcmp(stringinr->inp.value.vmeio.parm, "version"))\
          data->deviceId = LT_STRINGIN_VERSION;\
       else if (!strcmp(stringinr->inp.value.vmeio.parm, "ipaddr"))\
          data->deviceId = LT_STRINGIN_IPADDR;\
       else if (strstr(stringinr->inp.value.vmeio.parm, "trgtimech"))\
          data->deviceId = LT_STRINGIN_TRGTIME;\
       stringinr->dpvt=(void*) data;\
       return (0);\
 }

static void
readLTHelper( void *parm)
{
  epicsMessageQueueId qID = (epicsMessageQueueId)parm;
  TASK_DATA message;

  for( ;;) {
    message.pRecord = NULL;
    if ( epicsMessageQueueReceive( qID, (void *)&message, MAX_MSG_LENGTH) == ERROR || message.pRecord == NULL) { 
      /* give up CPU for a bit */ 
      epicsThreadSleep( epicsThreadSleepQuantum());
      continue;
    }

    switch (message.cmd){
    case GETWF:
      handleWf(&message);
      break;
    case LT_STRINGIN_TRGTIME:
      handleStringIn(&message);
      break;
    case ENABLECHAN:
    case DISABLECHAN:
      handleEnableDisable(&message);
      break;
    case LT_BO_RECOVER:
    case ENABLEACAL:
    case DISABLEACAL:
    case RESET:
      handleBo(&message);
      break;
    case GETCHANSTAT:
    case GETACALSTAT:
      handleBi(&message);
      break;
    case GETMEMSIZE:
    case GETTRGMODE:
    case GETTRGSRC:
    case LT_MBBI_LINKSTATUS:
      handleMbbi(&message);
      break;
    case SETMEMSIZE:
    case SETTRGMODE:
    case SETTRGSRC:
    case LDPNLSTP:
    case SVPNLSTP:
      handleMbbo(&message);
      break;
    case SETTIMEDIV:
    case SETVOLTDIV:
      handleAo(&message);
      break;
    case GETTIMEDIV:
    case GETVOLTDIV:
      handleAi(&message);
      break;
    default:
      printf("Unknown record type\n");
    }
  }
}

/* initializiation routine */
void init_LT364(int num, char* ipaddr)
{
  epicsThreadId taskId;
  /* int	dummy,ch,status; */

  scopeID[num] = LeCroy_Open(ipaddr,FOUR_CHANNEL_SCOPE,1);
  
  /* create message queue - one per scope */
  msgQID[num] = epicsMessageQueueCreate( MAX_MSGS, MAX_MSG_LENGTH);
  if (msgQID[num] == NULL){
    /* problem creating task */
    printf("Error creating message queue for scope\n");
    SCOPE_STATUS[num] = ERROR;
    return;
  }
  taskId = epicsThreadCreate( "tScopeHandle", eventTaskPriority, 20 * 1024, readLTHelper, (void *)msgQID[num]);
  
  SCOPE_STATUS[num] = OK;
  /*for (ch = 1; ch <= MAX_CHANNELS; ch++){
    status = LeCroy_Ioctl(scopeID[num], ch, ENABLECHAN, &dummy);
    if (status == ERROR) {
      printf("devWfLT364 (init_LT364) channel enabling failed\n");
      SCOPE_STATUS[num] = ERROR;
    }
  }*/
  if (SCOPE_STATUS[num] == OK)
    printf("Scope %d with IP-addr (%s) initialized.\n", num, ipaddr);
}

/***************************************************************************************/
/***********************************  WAVEFORM RECORD **********************************/
/***************************************************************************************/
static long initRecord(struct waveformRecord* pwf)
{
  /* Use dpvt to store task ID for async task */
  DPVT_DATA* data = (DPVT_DATA*) malloc(sizeof(DPVT_DATA));
  data->deviceId = GETWF;
  data->bufSize = 0;
  data->buffer = NULL;
  pwf->dpvt=(void*)data;
  return(0);
}

static void handleWf(TASK_DATA* message)
{
  int num;
  struct waveformRecord* pwf = (struct waveformRecord*) message->pRecord;
  int element = pwf->nelm; /* this is a static variable so there is no
			      danger in initializing outside of a lock
			      set */
  DPVT_DATA* dpvt = (DPVT_DATA*) pwf->dpvt;
  if (dpvt->bufSize < element){
    dpvt->bufSize = element;
    dpvt->buffer = realloc(dpvt->buffer, dpvt->bufSize*sizeof(float));
  }
  if (dpvt->buffer == NULL){
    pwf->pact = FALSE;
    return;
  }

  num = LeCroy_Read(message->scopeID, message->channel, dpvt->buffer, element);

  dbScanLock(message->pRecord);

  if (num < 0)
    /* error condition or channel disabled */
    recGblSetSevr(pwf, READ_ALARM, INVALID_ALARM); /* READ Alarm status */
  else {
    memcpy((float*)(pwf->bptr), dpvt->buffer, num*sizeof(float));
    pwf->nord = num;
  }

  ((pwf->rset)->process)(message->pRecord);

  dbScanUnlock(message->pRecord);
}

static long readWf(struct waveformRecord* pwf)
{
  /* Support asynchronous updates by spawning LeCroy_Read in a task */
  TASK_DATA message;

  if(!pwf->pact) { /* need to start async task */
    struct vmeio 	*pvmeio;
    LeCroyID		ltid;
    int		        ch, num, element;
    
    pvmeio = (struct vmeio *) &(pwf->inp.value);
    num = pvmeio->card;
    ch = pvmeio->signal;
    ltid = scopeID[num];
    element = pwf->nelm;
    if(!ltid) return 0;

    pwf->pact=TRUE;

    message.scopeID = ltid;
    message.channel = ch;
    message.cmd = GETWF;
    message.pRecord = (struct dbCommon*) pwf;
    if( epicsMessageQueueTrySend( msgQID[num], (void *)&message, sizeof(message)) == ERROR){
      recGblSetSevr(pwf, READ_ALARM, INVALID_ALARM); /* READ Alarm status */
      pwf->pact = FALSE;
      return ERROR;
    }
    return(OK);
  }

  /* all done now */
  pwf->pact = FALSE;
  return(OK);
}

/***************************************************************************************/
/**************************************  BO RECORD *************************************/
/***************************************************************************************/
static long initBo(struct boRecord *bor)
{    
  DPVT_DATA* dpvt;
  struct vmeio* pvmeio;
  int status=OK;
  int readback;
  int paramOK=0;
  
  /*  make sure this is the right type of record */
  if (bor->out.type!=VME_IO){
    recGblRecordError(S_db_badField, (void *)bor,
		      "devBoLT364 initBo - Illegal OUT");
    bor->pact=TRUE;
    return (S_db_badField);
  }
  
  CHECK_BOPARM("reset");
  CHECK_BOPARM("enblch1");
  CHECK_BOPARM("enblch2");
  CHECK_BOPARM("enblch3");
  CHECK_BOPARM("enblch4");
  CHECK_BOPARM("autocalS");
  CHECK_BOPARM("recoverlink");
  
  if (!paramOK){
    recGblRecordError(S_db_badField, (void*)bor,
		      "devBoLT364 initBo - bad parameter");
    bor->pact=TRUE;
    return (S_db_badField);
  }
  
  /* initialize bo records with current readback value */
  pvmeio = (struct vmeio *)&(bor->out.value);
  dpvt = (DPVT_DATA*) bor->dpvt;
  switch (dpvt->deviceId){
  case LT_BO_ENBLCH:
    status = LeCroy_Ioctl(scopeID[pvmeio->card], pvmeio->signal, GETCHANSTAT, &readback);
    if (status == OK){
      /* note: 0=off 1=on but rval implies the opposite */
      bor->rval=(readback-1)*(readback-1); /* this is
					      equivalent to !readback */
      bor->udf = FALSE;
    }
    else
      bor->udf = TRUE;
    break;
  case LT_BO_AUTOCAL:
    status = LeCroy_Ioctl(scopeID[pvmeio->card], pvmeio->signal, GETACALSTAT, &readback);
    if (status == OK){
      bor->rval=(readback-1)*(readback-1);
      bor->udf = FALSE;
    }
    else
      bor->udf = TRUE;
    break;
  case LT_BO_RESET:
  case LT_BO_RECOVER:
    /* nothing to initialize */
    break;
  }
  return (0);
}

static long writeBo(struct boRecord *bor)
{
  TASK_DATA message;
  DPVT_DATA* dpvt = (DPVT_DATA*) (bor->dpvt);
 
  if(!bor->pact) { /* need to start async task */
    struct vmeio *pvmeio;
    int status = OK;
    LeCroyID     ltid;
    int          num;
    
    switch (bor->out.type) {
    case (VME_IO) :
      pvmeio = (struct vmeio *)&(bor->out.value);
      break;  
    default :
      recGblRecordError(S_db_badField,(void *)bor,
			"devBoLT364 (writeBo) Illegal OUT field");
      return(ERROR);
    }
  
    /* Need IP for Scope - this comes from combination of card # and signal */
    num = pvmeio->card;
    ltid = scopeID[num];
    if (ltid == 0){
      recGblRecordError(S_db_badField,(void *)bor,
			"devBoLT364 (writeBo) Scope not connected");
      return(S_db_badField);
    }
    if (SCOPE_STATUS[num] == ERROR)
      return SCOPE_STATUS[num];

    bor->pact=TRUE;
    
    /* setup the message and send to the queue */
    message.scopeID = ltid;
    message.channel = pvmeio->signal;
    switch (dpvt->deviceId){
    case LT_BO_RESET:
      message.cmd = RESET;
      break;
    case LT_BO_RECOVER:
      message.cmd = LT_BO_RECOVER;
      break;
    case LT_BO_ENBLCH:
      if (bor->rval == 0)
	message.cmd = ENABLECHAN;
      else if (bor->rval == 1)
	message.cmd = DISABLECHAN;
      else
	status = ERROR;
      break;
    case LT_BO_AUTOCAL:
      if (bor->rval == 0)
	message.cmd = ENABLEACAL;
      else if (bor->rval == 1)
	message.cmd = DISABLEACAL;
      else
	status = ERROR;
      break;
    default:
      status = ERROR;
    };
    
    if (status == ERROR)
      return(ERROR);
    
    message.pRecord = (struct dbCommon*) bor;
    
    if( epicsMessageQueueTrySend( msgQID[num], (void *)&message, sizeof(message)) == ERROR){
      recGblSetSevr(bor, WRITE_ALARM, INVALID_ALARM); /* WRITE Alarm status */
      bor->pact = FALSE;
      return ERROR;
    }
    return (OK);
  }

  /* all done now */
  bor->pact = FALSE;
  return (OK);
}

static void handleEnableDisable(TASK_DATA* message)
{
  int dummy  = 0;
  int status;
  struct boRecord* bor = (struct boRecord*) message->pRecord;
 
  status = LeCroy_Ioctl(message->scopeID, message->channel, message->cmd, &dummy);

  dbScanLock(message->pRecord);

  if (status == ERROR)
    recGblSetSevr(bor, WRITE_ALARM, INVALID_ALARM); /* READ Alarm status */

  ((bor->rset)->process)(message->pRecord);

  dbScanUnlock(message->pRecord);
}

static void handleBo(TASK_DATA* message)
{
  int dummy = 0;
  int status;
  struct boRecord* bor = (struct boRecord*) message->pRecord;

  if (message->cmd == LT_BO_RECOVER)
    status = LeCroy_Recover_Link( message->scopeID, -1, 2);
  else
    status = LeCroy_Ioctl(message->scopeID, message->channel, message->cmd, &dummy);

  dbScanLock(message->pRecord);
 
  if (status == ERROR)
    recGblSetSevr(bor, WRITE_ALARM, INVALID_ALARM); /* READ Alarm status */
  
  ((bor->rset)->process)(message->pRecord);
  
  dbScanUnlock(message->pRecord);  
}

/***************************************************************************************/
/**************************************  BI RECORD *************************************/
/***************************************************************************************/
static long biIoinitInfo(int cmd, biRecord* bir, IOSCANPVT* iopvt)
{
  return 0;
}

static long initBi(struct biRecord *bir)
{
  switch (bir->inp.type){
  case VME_IO:
    CHECK_BIPARM("statusch1");
    CHECK_BIPARM("statusch2");
    CHECK_BIPARM("statusch3");
    CHECK_BIPARM("statusch4");
    CHECK_BIPARM("autocalM");
    /* Only gets here if a problem */
    recGblRecordError(S_db_badField, (void*)bir,
		      "devBiLT364 initBi - bad parameter");
    bir->pact=TRUE;
    return (S_db_badField);
    break;
  default:
    recGblRecordError(S_db_badField, (void*) bir,
		      "devBiLT364 (initBi) Illegal INP field");
    return(S_db_badField);
  }
  return (OK);
}

static long readBi(struct biRecord* bir)
{
  TASK_DATA message;
  DPVT_DATA* dpvt = (DPVT_DATA*) (bir->dpvt);
  
  if (!bir->pact) { /* need to start async task */
    struct vmeio *pvmeio;
    int status = OK;
    LeCroyID     ltid;
    int          num;
    
    /* bi.inp must be a VME_IO */
    switch (bir->inp.type) {
    case VME_IO :
      pvmeio = (struct vmeio *)&(bir->inp.value);
      break;  
    default:
      recGblRecordError(S_db_badField,(void *)bir,
			"devBiLT364 (readBi) Illegal INP field");
      return(S_db_badField);
    }
    
    /* Need IP for Scope - this comes from combination of card # and signal */
    num = pvmeio->card;
    ltid = scopeID[num];
    if (ltid == 0){
      recGblRecordError(S_db_badField,(void *)bir,
			"devBoLT364 (readBi) Scope not connected?");
      return(S_db_badField);
    }
    if (SCOPE_STATUS[num] == ERROR)
      return SCOPE_STATUS[num];
    
    bir->pact = TRUE;
    
    /* setup message and send to queue */
    message.scopeID = ltid;
    message.channel = pvmeio->signal;
    switch (dpvt->deviceId){
    case LT_BI_CHSTAT:
      message.cmd = GETCHANSTAT;
      break;
    case LT_BI_AUTOCAL:
      message.cmd = GETACALSTAT;
      break;
    default:
      status = ERROR;
    };
    
    if (status == ERROR)
      return(ERROR);

    
    message.pRecord = (struct dbCommon*) bir;
    
    if( epicsMessageQueueTrySend( msgQID[num], (void *)&message, sizeof(message)) == ERROR){
      recGblSetSevr(bir, READ_ALARM, INVALID_ALARM); /* READ Alarm status */
      bir->pact = FALSE;
      return ERROR;
    }
    return (OK);
  }
  
  /* all done now */
  bir->pact = FALSE;
  return (OK);
}

static void handleBi(TASK_DATA* message)
{
  int status;
  int value;
  struct biRecord* bir = (struct biRecord*) message->pRecord;

  status = LeCroy_Ioctl(message->scopeID, message->channel, message->cmd, &value);

  dbScanLock(message->pRecord);

  if (status == ERROR)
    recGblSetSevr(bir, READ_ALARM, INVALID_ALARM); /* READ Alarm status */
  else
    bir->rval=value;

  ((bir->rset)->process)(message->pRecord);

  dbScanUnlock(message->pRecord);      
}

/***************************************************************************************/
/*************************************  MBBI RECORD ************************************/
/***************************************************************************************/
static long mbbiIoinitInfo(int cmd, mbbiRecord* mbbir, IOSCANPVT* iopvt)
{
  return 0;
}

static void setbiMemSizeLegalValues(struct mbbiRecord* mbbir)
{
  mbbir->zrvl = msiz_op[0].val;
  strcpy(mbbir->zrst, msiz_op[0].cmd);
  mbbir->onvl = msiz_op[1].val;
  strcpy(mbbir->onst, msiz_op[1].cmd);
  mbbir->twvl = msiz_op[2].val;;
  strcpy(mbbir->twst, msiz_op[2].cmd);
  mbbir->thvl = msiz_op[3].val;;
  strcpy(mbbir->thst, msiz_op[3].cmd);
  mbbir->frvl = msiz_op[4].val;;
  strcpy(mbbir->frst, msiz_op[4].cmd);
  mbbir->fvvl = msiz_op[5].val;;
  strcpy(mbbir->fvst, msiz_op[5].cmd);
  mbbir->sxvl = msiz_op[6].val;;
  strcpy(mbbir->sxst, msiz_op[6].cmd);
  mbbir->svvl = msiz_op[7].val;;
  strcpy(mbbir->svst, msiz_op[7].cmd);
  mbbir->eivl = msiz_op[8].val;;
  strcpy(mbbir->eist, msiz_op[8].cmd);
  mbbir->nivl = msiz_op[9].val;;
  strcpy(mbbir->nist, msiz_op[9].cmd);
  mbbir->tevl = msiz_op[10].val;;
  strcpy(mbbir->test, msiz_op[10].cmd);
  mbbir->elvl = msiz_op[11].val;;
  strcpy(mbbir->elst, msiz_op[11].cmd);
  mbbir->tvvl = msiz_op[12].val;;
  strcpy(mbbir->tvst, msiz_op[12].cmd);
  mbbir->ttvl = msiz_op[13].val;;
  strcpy(mbbir->ttst, msiz_op[13].cmd);  
}

static long initMbbi(struct mbbiRecord* mbbir)
{
  if (mbbir->inp.type!=VME_IO){
    recGblRecordError(S_db_badField, (void*)mbbir, "devMbbiLT364 (initMbbi) Illegal INP field");
    mbbir->pact=TRUE;
    return(S_db_badField);
  }

  /* set the legal values for mbbi record of type memsizeM */
  if (strstr(mbbir->name, "memsize"))
    setbiMemSizeLegalValues(mbbir);
  
  CHECK_MBBIPARM("memsizeM");
  CHECK_MBBIPARM("trgmodeM");
  CHECK_MBBIPARM("trgsrcM");
  CHECK_MBBIPARM("linkstatusM");

  /* Only gets here if a problem */
  recGblRecordError(S_db_badField, (void*)mbbir,
		    "devMbbiLT364 initMbbi - bad parameter");
  mbbir->pact=TRUE;
  return (S_db_badField);
}

static long readMbbi(struct mbbiRecord* mbbir)
{
  TASK_DATA message;
  DPVT_DATA* dpvt = (DPVT_DATA*) (mbbir->dpvt);

  if (!mbbir->pact) { /* need to start async task */
    struct vmeio *pvmeio;
    int status = OK;
    LeCroyID ltid;
    int num;
    /* must be a VME_IO */
    if (mbbir->inp.type!=VME_IO){
      recGblRecordError(S_db_badField, (void*)mbbir, "devMbbiLT364 (readMbbi) Illegal INP field");
      return(S_db_badField);
    }
    pvmeio = (struct vmeio*)&(mbbir->inp.value);
    num = pvmeio->card;
    ltid = scopeID[num];
    if (ltid == 0){
      recGblRecordError(S_db_badField,(void *)mbbir,
			"devBoLT364 (readMbbi) Scope not connected?");
      return(S_db_badField);
    }
    if (SCOPE_STATUS[num] == ERROR)
      return SCOPE_STATUS[num];
    
    mbbir->pact = TRUE;

    /* setup message and send to queue */
    message.scopeID = ltid;
    message.channel = pvmeio->signal;
    switch (dpvt->deviceId){
    case LT_MBBI_MEMSZ :
      message.cmd = GETMEMSIZE;
      break;
    case LT_MBBI_TRGMD :
      message.cmd = GETTRGMODE;
      break;
    case LT_MBBI_TRGSRC :
      message.cmd = GETTRGSRC;
      break;
    case LT_MBBI_LINKSTATUS :
      message.cmd = LT_MBBI_LINKSTATUS;
      break;
    default:
      status = ERROR;
    };

    if (status == ERROR){
       recGblRecordError(S_db_badField, (void *)mbbir,
			"devBiLT364 (readMbbi) Unrecognized record - record not put in queue");
      return(ERROR);
    }

    message.pRecord = (struct dbCommon*) mbbir;
    if (message.cmd != LT_MBBI_LINKSTATUS){
      if( epicsMessageQueueTrySend( msgQID[num], (void *)&message, sizeof(message)) == ERROR){
	recGblSetSevr(mbbir, READ_ALARM, INVALID_ALARM); /* READ Alarm status */
	mbbir->pact = FALSE;
	return ERROR;
      }
      return (OK);
    } else
      /* handle link status synchronously */
      handleMbbi(&message);
  }
  
  /* all done now */
  mbbir->pact = FALSE;
  return (OK);
}

static void handleMbbi(TASK_DATA* message)
{
  int status;
  int value;
  struct mbbiRecord* mbbir = (struct mbbiRecord*) message->pRecord;

  if (message->cmd == LT_MBBI_LINKSTATUS)
    status = LeCroy_Get_LinkStat(message->scopeID, &value);
  else
    status = LeCroy_Ioctl(message->scopeID, message->channel, message->cmd, &value);

  dbScanLock(message->pRecord);

  if (status != OK)
    recGblSetSevr(mbbir, READ_ALARM, INVALID_ALARM); /* READ Alarm status */
  else
    mbbir->rval=value;

  ((mbbir->rset)->process)(message->pRecord);

  dbScanUnlock(message->pRecord);      

}

/***************************************************************************************/
/*************************************  MBBO RECORD ************************************/
/***************************************************************************************/
static void setboMemSizeLegalValues(struct mbboRecord* mbbor)
{
  mbbor->zrvl = 0;
  strcpy(mbbor->zrst, msiz_op[0].cmd);
  mbbor->onvl = 1;
  strcpy(mbbor->onst, msiz_op[1].cmd);
  mbbor->twvl = 2;
  strcpy(mbbor->twst, msiz_op[2].cmd);
  mbbor->thvl = 3;
  strcpy(mbbor->thst, msiz_op[3].cmd);
  mbbor->frvl = 4;
  strcpy(mbbor->frst, msiz_op[4].cmd);
  mbbor->fvvl = 5;
  strcpy(mbbor->fvst, msiz_op[5].cmd);
  mbbor->sxvl = 6;
  strcpy(mbbor->sxst, msiz_op[6].cmd);
  mbbor->svvl = 7;
  strcpy(mbbor->svst, msiz_op[7].cmd);
  mbbor->eivl = 8;
  strcpy(mbbor->eist, msiz_op[8].cmd);
  mbbor->nivl = 9;
  strcpy(mbbor->nist, msiz_op[9].cmd);
  mbbor->tevl = 10;
  strcpy(mbbor->test, msiz_op[10].cmd);
  mbbor->elvl = 11;
  strcpy(mbbor->elst, msiz_op[11].cmd);
  mbbor->tvvl = 12;
  strcpy(mbbor->tvst, msiz_op[12].cmd);
  mbbor->ttvl = 13;
  strcpy(mbbor->ttst, msiz_op[13].cmd);  
}

static long initMbbo(struct mbboRecord* mbbor)
{
  DPVT_DATA* dpvt;
  struct vmeio* pvmeio;
  int value, index;
  int paramOK=0;
  int status = OK;
  
  if (mbbor->out.type!=VME_IO){
    recGblRecordError(S_db_badField, (void*) mbbor,
		      "devMbboLT364 initMbbo - Illegal OUT");
    mbbor->pact=TRUE;
    return (S_db_badField);
  }

  /* set the legal values for mbbo record of type memsizeM */
  if (strstr(mbbor->name, "memsize"))
    setboMemSizeLegalValues(mbbor);

  CHECK_MBBOPARM("memsizeS");
  CHECK_MBBOPARM("trgmodeS");
  CHECK_MBBOPARM("trgsrcS");
  CHECK_MBBOPARM("ldpnlstp");
  CHECK_MBBOPARM("svpnlstp");

  if (!paramOK){
    recGblRecordError(S_db_badField, (void*)mbbor,
		      "devMbboLT364 initMbbo - bad parameter");
    mbbor->pact=TRUE;
    return (S_db_badField);
  }

  /* initialize values to readbacks */
  pvmeio = (struct vmeio *)&(mbbor->out.value);
  dpvt = (DPVT_DATA*) mbbor->dpvt;
  switch (dpvt->deviceId){
  case LT_MBBO_MEMSZ:
    status = LeCroy_Ioctl(scopeID[pvmeio->card], pvmeio->signal, GETMEMSIZE, &value);
    /* Ioctl returns the actual mem size - need to convert to rval
       index */
    for (index=0;index<15;index++)
      if (msiz_op[index].val == value){
	value = index;
	break;
      }
    break;
  case LT_MBBO_TRGMD:
    status = LeCroy_Ioctl(scopeID[pvmeio->card], pvmeio->signal, GETTRGMODE, &value);
    break;
  case LT_MBBO_TRGSRC:
    status = LeCroy_Ioctl(scopeID[pvmeio->card], pvmeio->signal, GETTRGSRC, &value);
    break;
  case LT_MBBO_LDPNLSTP:
    /* nothing to initialize */
    return status;
    break;
  case LT_MBBO_SVPNLSTP:
    /* set to first item in list */
    mbbor->rval = 1;
    mbbor->udf = FALSE;
    return status;
    break;
  }

  if (status == OK){
    mbbor->rval=value;
    mbbor->udf = FALSE;
  } else
    mbbor->udf = TRUE;
  
  return (0);
}

static long writeMbbo(struct mbboRecord* mbbor)
{
  TASK_DATA message;
  DPVT_DATA* dpvt = (DPVT_DATA*) (mbbor->dpvt);

  if(!mbbor->pact) { /* need to start async task */
    struct vmeio* pvmeio;
    int status = OK;
    LeCroyID     ltid;
    int          num;

    if (mbbor->out.type!=VME_IO){
      recGblRecordError(S_db_badField, (void*) mbbor,
			"devMbboLT364 writeMbbo - Illegal OUT field");
      mbbor->pact=TRUE;
      return (S_db_badField);
    }

    /* Need IP for Scope - this comes from combination of card # and
       signal */
    pvmeio = (struct vmeio *)&(mbbor->out.value);
    num = pvmeio->card;
    ltid = scopeID[num];
    if (ltid == 0){
      recGblRecordError(S_db_badField,(void *)mbbor,
			"devMbboLT364 (writeMbbo) Scope not connected?");
      return(S_db_badField);
    }
    if (SCOPE_STATUS[num] == ERROR)
      return SCOPE_STATUS[num];

    mbbor->pact = TRUE;

    /* setup the message and send to the queue */
    message.scopeID = ltid;
    message.channel = pvmeio->signal;

    switch (dpvt->deviceId){
    case LT_MBBO_MEMSZ :
      message.cmd = SETMEMSIZE;
      break;
    case LT_MBBO_TRGMD :
      message.cmd = SETTRGMODE;
      break;
    case LT_MBBO_TRGSRC :
      message.cmd = SETTRGSRC;
      break;
    case LT_MBBO_LDPNLSTP :
      message.cmd = LDPNLSTP;
      break;
    case LT_MBBO_SVPNLSTP :
      message.cmd = SVPNLSTP;
      break;
    default:
      status = ERROR;
    };

    if (status == ERROR)
      return(ERROR);
    
    message.pRecord = (struct dbCommon*) mbbor;

    if( epicsMessageQueueTrySend( msgQID[num], (void *)&message, sizeof(message)) == ERROR){
      recGblSetSevr(mbbor, WRITE_ALARM, INVALID_ALARM); /* WRITE Alarm status */
      mbbor->pact = FALSE;
      return ERROR;
    }
    return (OK);
  }

  /* all done now */
  mbbor->pact = FALSE;
  return (OK);
}

static void handleMbbo(TASK_DATA* message)
{
  int status;
  struct mbboRecord* mbbo = (struct mbboRecord*) message->pRecord;
  int value = mbbo->rval;

  status = LeCroy_Ioctl(message->scopeID, message->channel, message->cmd, &value);

  dbScanLock(message->pRecord);

  if (status == ERROR)
    recGblSetSevr(mbbo, WRITE_ALARM, INVALID_ALARM); /* READ Alarm status */

  ((mbbo->rset)->process)(message->pRecord);

  dbScanUnlock(message->pRecord);  

}

/***************************************************************************************/
/**************************************  AO RECORD *************************************/
/***************************************************************************************/
static long initAo(struct aoRecord *aor)
{
  int paramOK=0;
  int status = OK;
  DPVT_DATA* dpvt;
  float value;
  struct vmeio* pvmeio;
  
  /*  make sure this is the right type of record */
  if (aor->out.type!=VME_IO){
    recGblRecordError(S_db_badField, (void *)aor,
		      "devAoLT364 initAo - Illegal OUT");
    aor->pact=TRUE;
    return (S_db_badField);
  }

  CHECK_AOPARM("timedivS");
  CHECK_AOPARM("voltdivch1S");
  CHECK_AOPARM("voltdivch2S");
  CHECK_AOPARM("voltdivch3S");
  CHECK_AOPARM("voltdivch4S");

  if (!paramOK){
    recGblRecordError(S_db_badField, (void*)aor,
		      "devAoLT364 initAo - bad parameter");
    aor->pact=TRUE;
    return (S_db_badField);
  }

  /* initialize the record */
  pvmeio = (struct vmeio *)&(aor->out.value);
  dpvt = (DPVT_DATA*) aor->dpvt;
  switch (dpvt->deviceId){
  case LT_AO_TIMEDIV:
    status = LeCroy_Ioctl(scopeID[pvmeio->card], pvmeio->signal, GETTIMEDIV, &value);  
    break;
  case LT_AO_VOLTDIV:
    status = LeCroy_Ioctl(scopeID[pvmeio->card], pvmeio->signal, GETVOLTDIV, &value);  
    break;
  }
  if (status == OK){
    aor->val = value;
    aor->udf = FALSE;
    aor->pact = FALSE;
  }
  else{
    aor->udf = TRUE; 
    return (0);
  }

  return (2); /* no conversion of value */
}
  
static long writeAo(struct aoRecord *aor)
{
  TASK_DATA message;
  DPVT_DATA* dpvt = (DPVT_DATA*) (aor->dpvt);

  if (!aor->pact){ /* need to start async task */
    struct vmeio *pvmeio;
    int status = OK;
    LeCroyID     ltid;
    int          num;

    switch (aor->out.type){
    case (VME_IO) :
      pvmeio = (struct vmeio *)&(aor->out.value);
      break;  
    default :
      recGblRecordError(S_db_badField,(void *)aor,
			"devAoLT364 (writeAo) Illegal OUT field");
      aor->udf = TRUE;
      return(S_db_badField);
    }
  
    /* Need IP for Scope - this comes from combination of card # and signal */
    num = pvmeio->card;
    ltid = scopeID[num];
    if (ltid == 0){
      recGblRecordError(S_db_badField,(void *)aor,
			"devAoLT364 (writeAo) Scope not connected?");
      return(S_db_badField);
    }
    if (SCOPE_STATUS[num] == ERROR)
      return SCOPE_STATUS[num];

    aor->pact = TRUE;

    /* setup the message and send to the queue */
    message.scopeID = ltid;
    message.channel = pvmeio->signal;
    switch (dpvt->deviceId){
    case LT_AO_TIMEDIV :
      message.cmd = SETTIMEDIV;
      break;
    case LT_AO_VOLTDIV :
      message.cmd = SETVOLTDIV;
      break;
    default:
      aor->udf = TRUE;
      status = ERROR;
    };
    
    if (status == ERROR)
      return(ERROR);

    message.pRecord = (struct dbCommon*)aor;
    if( epicsMessageQueueTrySend( msgQID[num], (void *)&message, sizeof(message)) == ERROR){
      recGblSetSevr(aor, WRITE_ALARM, INVALID_ALARM); /* WRITE Alarm status */
      aor->pact = FALSE;
      return ERROR;
    }
    return (2);
  }

  /* all done now */
  aor->udf = FALSE; 
  aor->pact = FALSE;
  return (2); /* don't allow conversion of double values to int */
}

static void handleAo(TASK_DATA* message)
{
  int status;
  struct aoRecord* aor = (struct aoRecord*) message->pRecord;
  float value =  aor->val;
  
  status = LeCroy_Ioctl(message->scopeID, message->channel, message->cmd, &value);

  dbScanLock(message->pRecord);

  if (status == ERROR)
    recGblSetSevr(aor, WRITE_ALARM, INVALID_ALARM); /* READ Alarm status */
  else
    aor->val = value; /* store in val (rather than rval) since it's in
			 engineering units already */

  ((aor->rset)->process)(message->pRecord);

  dbScanUnlock(message->pRecord);  

}

/***************************************************************************************/
/**************************************  AI RECORD *************************************/
/***************************************************************************************/
static long aiIoinitInfo(int cmd, aiRecord* air, IOSCANPVT* iopvt)
{
  return 0;
}

static long initAi(struct aiRecord *air)
{
  switch (air->inp.type){
  case VME_IO:
    CHECK_AIPARM("timedivM");
    CHECK_AIPARM("voltdivch1M");
    CHECK_AIPARM("voltdivch2M");
    CHECK_AIPARM("voltdivch3M");
    CHECK_AIPARM("voltdivch4M");
    /* Only gets here if a problem */
    recGblRecordError(S_db_badField, (void*)air,
		      "devAiLT364 initAi - bad parameter");
    air->pact=TRUE;
    return (S_db_badField);
    break;
  default:
    recGblRecordError(S_db_badField, (void*) air,
		      "devAiLT364 (initAi) Illegal INP field");
    return(S_db_badField);
  }  

  return (OK);
}

static long readAi(struct aiRecord* air)
{
  TASK_DATA message;
  DPVT_DATA* dpvt = (DPVT_DATA*) (air->dpvt);

  if (!air->pact) { /* need to start async task */
    struct vmeio *pvmeio;
    int status = OK;
    LeCroyID     ltid;
    int          num;

    /* ai must be a VME_IO */
    switch (air->inp.type) {
    case (VME_IO):
      pvmeio = (struct vmeio*)&(air->inp.value);
      break;
    default:
      recGblRecordError(S_db_badField,(void *)air,
			"devAiLT364 (readAi) Illegal INP field");
      return(S_db_badField);
    }

    /* Need IP for Scope - this comes from combination of card # and signal */
    num = pvmeio->card;
    ltid = scopeID[num];
    if (ltid == 0){
      recGblRecordError(S_db_badField,(void *)air,
			"devAiLT364 (readAi) Scope not connected?");
      return(S_db_badField);
    }
    if (SCOPE_STATUS[num] == ERROR)
      return SCOPE_STATUS[num];

    air->pact = TRUE;

    /* setup message and send to queue */
    message.scopeID = ltid;
    message.channel = pvmeio->signal;
    switch (dpvt->deviceId) {
    case LT_AI_TIMEDIV :
      message.cmd = GETTIMEDIV;
      break;  
    case LT_AI_VOLTDIV :
      message.cmd = GETVOLTDIV;
      break;
    default:
      status = ERROR;
    }

    if (status != OK) {
      recGblRecordError(S_db_badField, (void*) air,"devAiLT364 (readAi) Unknown record - record not put in queue");
      return(ERROR);
    }
    
    message.pRecord = (struct dbCommon*) air;
    if( epicsMessageQueueTrySend( msgQID[num], (void *)&message, sizeof(message)) == ERROR){
      recGblSetSevr(air, READ_ALARM, INVALID_ALARM); /* READ Alarm status */
      air->pact = FALSE;
      return ERROR;
    }
    return (2);
  }

  /* all done now */
  air->udf = FALSE;
  air->pact = FALSE;
  return(2); /* don't convert value because it is a double */
}

static void handleAi(TASK_DATA* message)
{
  int status;
  float value;
  
  struct aiRecord* air = (struct aiRecord*) message->pRecord;

  status = LeCroy_Ioctl(message->scopeID, message->channel, message->cmd, &value);

  dbScanLock(message->pRecord);

  if (status == ERROR)
    recGblSetSevr(air, READ_ALARM, INVALID_ALARM); /* READ Alarm status */
  else
    air->val=value; /* store in val (rather than rval) since it's already in engineering
		       units */

  ((air->rset)->process)(message->pRecord);

  dbScanUnlock(message->pRecord);  
}

/***************************************************************************************/
/********************************* Stringin RECORD *************************************/
/***************************************************************************************/
static long initStringIn(struct stringinRecord *stringinr)
{
  switch (stringinr->inp.type){
  case VME_IO :
    CHECK_STRINGIN("model");
    CHECK_STRINGIN("serial");
    CHECK_STRINGIN("version");
    CHECK_STRINGIN("ipaddr");
    CHECK_STRINGIN("trgtimech1");
    CHECK_STRINGIN("trgtimech2");
    CHECK_STRINGIN("trgtimech3");
    CHECK_STRINGIN("trgtimech4");
    /* Only gets here if a problem */
    recGblRecordError(S_db_badField, (void*)stringinr,
		      "devStringInLT364 initStringIn - bad parameter");
    stringinr->pact=TRUE;
    return (S_db_badField);
    break;
  default:
    recGblRecordError(S_db_badField, (void*) stringinr,
		      "devStringInLT364 (initStringIn) Illegal INP field");
    return(S_db_badField);
  }  

  return (OK);
}

static long readStringIn(struct stringinRecord* stringinr)
{
  TASK_DATA message;
  DPVT_DATA* dpvt = (DPVT_DATA*) (stringinr->dpvt);

  if (!stringinr->pact) { /* need to start async task */
    struct vmeio *pvmeio;
    int status = OK;
    LeCroyID     ltid;
    int          num;

    /* stringin must be a VME_IO */
    switch (stringinr->inp.type) {
    case VME_IO :
      pvmeio = (struct vmeio*)&(stringinr->inp.value);
      break;
    default:
      recGblRecordError(S_db_badField,(void *)stringinr,
			"devStringInLT364 (readStringIn) Illegal INP field");
      return(S_db_badField);
    }

    /* Need IP for Scope - this comes from combination of card # and signal */
    num = pvmeio->card;
    ltid = scopeID[num];
    if (ltid == 0){
      recGblRecordError(S_db_badField,(void *)stringinr,
			"devStringInLT364 (readStringIn) Scope not connected?");
      return(S_db_badField);
    }
    if (SCOPE_STATUS[num] == ERROR)
      return SCOPE_STATUS[num];

    stringinr->pact = TRUE;

    /* setup message and send to queue */
    message.scopeID = ltid;
    message.channel = pvmeio->signal;
    switch (dpvt->deviceId) {
    case LT_STRINGIN_MODEL :
      message.cmd = LT_STRINGIN_MODEL;
      break;
    case LT_STRINGIN_SERIAL :
      message.cmd = LT_STRINGIN_SERIAL;
      break;
    case LT_STRINGIN_VERSION :
      message.cmd = LT_STRINGIN_VERSION;
      break;
    case LT_STRINGIN_IPADDR :
      message.cmd = LT_STRINGIN_IPADDR;
      break;
    case LT_STRINGIN_TRGTIME :
      message.cmd = LT_STRINGIN_TRGTIME;
      break;
    default:
      status = ERROR;
    }

    if (status != OK) {
      recGblRecordError(S_db_badField, (void*) stringinr,
			"devStringInLT364 (readStringIn) Unknown record - record not processed");
      return(ERROR);
    }
    
    message.pRecord = (struct dbCommon*) stringinr;
    
    /* no records need to be handled asynchronously */
    if (message.cmd == ERROR){
      message.pRecord = (struct dbCommon*) stringinr;
      if( epicsMessageQueueTrySend( msgQID[num], (void *)&message, sizeof(message)) == ERROR){
	recGblSetSevr(stringinr, READ_ALARM, INVALID_ALARM); /* READ Alarm status */
	stringinr->pact = FALSE;
	return ERROR;
      }
      return 0;
    }
    else
      handleStringIn(&message);
  }

  /* all done now */
  stringinr->udf = FALSE;
  stringinr->pact = FALSE;
  return OK;
}

static void handleStringIn(TASK_DATA* message)
{
  int status, i, index, col;
  char inString[128];
  char val[40];

  struct stringinRecord* stringinr = (struct stringinRecord*) message->pRecord;
  col = index = 0;
  
  if (message->cmd == LT_STRINGIN_TRGTIME)
    status = LeCroy_Get_LastTrgTime(message->scopeID, message->channel, inString);
  else if (message->cmd == LT_STRINGIN_MODEL || message->cmd == LT_STRINGIN_SERIAL ||
      message->cmd == LT_STRINGIN_VERSION)
    status = LeCroy_Get_Model(message->scopeID, inString);
  else if (message->cmd == LT_STRINGIN_IPADDR)
    status = LeCroy_Get_IPAddr(message->scopeID, inString);
  else 
    status = ERROR;

  if (status == ERROR) {
    recGblSetSevr(stringinr, READ_ALARM, INVALID_ALARM); /* READ Alarm status */
  }

/* parse the model record */
  switch (message->cmd){
  case LT_STRINGIN_TRGTIME:
  case LT_STRINGIN_IPADDR:
    strcpy(stringinr->val,inString); /* store the string in the record */
    break;
  case LT_STRINGIN_MODEL:
    /* model is inString up to second ',' */
    for (i=0; i<strlen(inString); i++){
      if (inString[i] == ',') col++;
      if (col == 2) break;
      val[index++] = inString[i];
    }
    val[index]=0;
    strcpy(stringinr->val,val);
    break;
  case LT_STRINGIN_SERIAL:
    /* serial is inString from second ',' to third ',' */
    for (i=0; i<strlen(inString); i++){
      if (inString[i] == ','){
	col++;
	continue;
      }
      if (col == 2) val[index++] = inString[i];
      if (col > 2) break;
    }
    /* terminate string */
    val[index]=0;
    strcpy(stringinr->val,val);
    break;
  case LT_STRINGIN_VERSION:
    /* version is inString after last ',' */
    for (i=0; i<strlen(inString); i++){
      if (inString[i] == ','){
	col++;
	continue;
      }
      if (col == 3) val[index++] = inString[i];
    }
    /* terminate string (remove newline character) */
    if(val[index-1] == '\n') val[index-1] = 0;
    else val[index]=0; 
    strcpy(stringinr->val,val);
    break;
  }
}
/*
 * Register iocsh commands
 */

static const iocshArg init_LT364Arg0 = { "num",iocshArgInt };
/* static const iocshArg init_LT364Arg1 = { "ipaddr",iocshArgInt }; */
static const iocshArg init_LT364Arg1 = { "ipaddr",iocshArgString };
static const iocshArg * const init_LT364Args[2] = {
       &init_LT364Arg0,
       &init_LT364Arg1};
static const iocshFuncDef init_LT364FuncDef = {"init_LT364",2,init_LT364Args};
static void init_LT364CallFunc(const iocshArgBuf *args)
{
/*    init_LT364(args[0].ival,args[1].ival); */
    init_LT364(args[0].ival,args[1].sval);
}
void LeCroy_ENETRegister(void)
{
   iocshRegister(&init_LT364FuncDef, init_LT364CallFunc);
}
epicsExportRegistrar(LeCroy_ENETRegister);
