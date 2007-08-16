#ifndef _LECROY_DEV_H_
#define _LECROY_DEV_H_

#include "LeCroy_DevSup.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

#include <epicsThread.h>
#include <epicsMessageQueue.h>

/* Task Definitions */
#define MAX_MSGS 50 /* At the time of development there are 37
		       records.  set to 50 for worst case scenario of
		       all records being processed at the same time */
#define MAX_CHANNELS 4
#define MAX_SCOPES 20
static int SCOPE_STATUS[MAX_SCOPES];
#define eventTaskPriority epicsThreadPriorityHigh

static LeCroyID scopeID[MAX_SCOPES]; 
static epicsMessageQueueId msgQID[MAX_SCOPES];
/* define structure to be passed to task for performing asynchronous
   functions */
typedef struct {
  LeCroyID scopeID;          /* scope id */
  int channel;               /* channel to perform task on.  zero if not channel
				related */
  int cmd;                   /* defines the task to be performed */
  struct dbCommon* pRecord;  /* record type */
} TASK_DATA;
#define MAX_MSG_LENGTH sizeof(TASK_DATA)

typedef struct {
  int deviceId;              /* enumerated type defining name of
				device */
  float* buffer;             /* array to store waveform data */
  int bufSize;               /* buffer size of the waveform */
} DPVT_DATA;

/* define parameter indicator flags */
typedef enum {
  LT_BO_RESET,
  LT_BO_ENBLCH,
  LT_BO_AUTOCAL,
  LT_BI_CHSTAT,
  LT_BI_AUTOCAL,
  LT_MBBI_MEMSZ,
  LT_MBBO_MEMSZ,
  LT_MBBI_TRGMD,
  LT_MBBO_TRGMD,
  LT_MBBI_TRGSRC,
  LT_MBBO_TRGSRC,
  LT_MBBO_LDPNLSTP,
  LT_MBBO_SVPNLSTP,
  LT_AI_TIMEDIV,
  LT_AO_TIMEDIV,
  LT_AI_VOLTDIV,
  LT_AO_VOLTDIV,
  LT_STRINGIN_MODEL,
  LT_STRINGIN_SERIAL,
  LT_STRINGIN_VERSION,
  LT_STRINGIN_IPADDR,
  LT_STRINGIN_TRGTIME,
  LT_MBBI_LINKSTATUS=500, /* set to 500 to avoid conflict with types
			     defined in driver */
  LT_BO_RECOVER
} LTTYPE;

static long initRecord();
static void readLTHelper( void *parm);
static void handleWf(TASK_DATA* message);
static long readWf();
static long initBo();
static long writeBo();
static void handleEnableDisable(TASK_DATA* message);
static void handleBo(TASK_DATA* message);
static long initBi();
static long biIoinitInfo(int cmd, biRecord* bir, IOSCANPVT* iopvt);
static long readBi();
static void handleBi(TASK_DATA* message);
static long initMbbi();
static long mbbiIoinitInfo(int cmd, mbbiRecord* mbbir, IOSCANPVT* iopvt);
static long readMbbi();
static void handleMbbi(TASK_DATA* message);
static long initMbbo();
static long writeMbbo();
static void handleMbbo(TASK_DATA* message);
static long initAi();
static long aiIoinitInfo(int cmd, aiRecord* air, IOSCANPVT* iopvt);
static long readAi();
static void handleAi(TASK_DATA* message);
static long initAo();
static long writeAo();
static void handleAo(TASK_DATA* message);
static long initStringIn();
static long readStringIn();
static void handleStringIn(TASK_DATA* message);

typedef struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	read_write;
	DEVSUPFUN	special_linconv;
} VME_DEV_SUP_SET;

VME_DEV_SUP_SET devWfLT364=   {6, NULL, NULL, initRecord, NULL, readWf, NULL};
VME_DEV_SUP_SET devBoLT364=   {6, NULL, NULL, initBo, NULL, writeBo, NULL};
VME_DEV_SUP_SET devBiLT364=   {6, NULL, NULL, initBi, biIoinitInfo, readBi, NULL};
VME_DEV_SUP_SET devMbbiLT364= {6, NULL, NULL, initMbbi, mbbiIoinitInfo, readMbbi, NULL};
VME_DEV_SUP_SET devMbboLT364= {6, NULL, NULL, initMbbo, NULL, writeMbbo, NULL};
VME_DEV_SUP_SET devAoLT364=   {6, NULL, NULL, initAo, NULL, writeAo, NULL};
VME_DEV_SUP_SET devAiLT364=   {6, NULL, NULL, initAi, aiIoinitInfo, readAi, NULL};
VME_DEV_SUP_SET devStringInLT364= {6, NULL, NULL, initStringIn, NULL, readStringIn, NULL};
#if     EPICS_VERSION>=3 && EPICS_REVISION>=14
epicsExportAddress(dset, devWfLT364);
epicsExportAddress(dset, devBoLT364);
epicsExportAddress(dset, devBiLT364);
epicsExportAddress(dset, devMbbiLT364);
epicsExportAddress(dset, devMbboLT364);
epicsExportAddress(dset, devAoLT364);
epicsExportAddress(dset, devAiLT364);
epicsExportAddress(dset, devStringInLT364);
#endif

#ifdef __cplusplus
}
#endif  /* __cplusplus */

#endif
