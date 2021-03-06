/*! \file Cmdd.c
  \brief The Cmdd program that receives commands from SIPd
  and acts upon them

  This is the updated Cmdd program which is actually a daemon.

  July 2005  rjn@mps.ohio-state.edu
*/
#define _GNU_SOURCE
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <libgen.h> //For Mac OS X
//Flightsoft includes
#include "configLib/configLib.h"
#include "kvpLib/keyValuePair.h"
#include "utilLib/utilLib.h"
#include "linkWatchLib/linkWatchLib.h"
#include <execinfo.h>

#include "includes/anitaStructures.h"
#include "includes/anitaFlight.h"
#include "includes/anitaCommand.h"

int sameCommand(CommandStruct_t *theCmd, CommandStruct_t *lastCmd);
void copyCommand(CommandStruct_t *theCmd, CommandStruct_t *cpCmd);
int checkCommand(CommandStruct_t *theCmd);
int checkForNewCommand();
int executeCommand(CommandStruct_t *theCmd); //Returns unixTime or 0
int executePrioritizerdCommand(int command, int value, int value2);
int executePlaybackCommand(int command, unsigned int value1, unsigned int value2);
int executeAcqdRateCommand(int command, unsigned char args[8]);
int executeAcqdExtraCommand(int command, unsigned char arg);
int executeGpsPhiMaskCommand(int command, unsigned char args[5]);
int executeSipdControlCommand(int command, unsigned char args[3]);
int executeLosdControlCommand(int command, unsigned char args[2]);
int executeGpsdExtracommand(int command, unsigned char arg[2]);
int executeCalibdExtraCommand(int command, unsigned char arg[2]);
int executeRTLCommand(int command, unsigned char arg[2]);
int executeTuffCommand(int command, unsigned char arg[6]);
int doTuffRawCommand(char rfcm, char stack, short cmd, time_t * tm );
int logRequestCommand(int logNum, int numLines);
int writeCommandEcho(CommandStruct_t *theCommand, int unixTime);
int readConfig();
int cleanDirs();
int clearRamdisk();
int sendConfig(int progMask);
int defaultConfig(int progMask);
int lastConfig(int progMask);
int switchConfig(int progMask,unsigned char configId);
int saveConfig(int progMask,unsigned char configId);
int killPrograms(int progMask);
int reallyKillPrograms(int progMask);
int startPrograms(int progMask);
int respawnPrograms(int progMask);
int disableDisk(int diskMask, int disFlag);
int mountNextNtu(int whichNumber);
int mountNextUsb(int whichUsb);
void prepWriterStructs();
int requestFile(char *filename, int numLines);
int requestJournalCtl(JournalctlOptionCommand_t jcOpt, int cmdArg, int numLines);
int readAcqdConfig();
int readArchivedConfig();
int readPrioritizerdConfig();
int readLosdConfig();
int readSipdConfig();
int readGpsdConfig();
int setEventDiskBitMask(int modOrSet, int bitMask);
int setHkDiskBitMask(int modOrSet, int bitMask);
int setPriDiskMask(int pri, int bitmask);
int setPriEncodingType(int pri, int encType);
int setAlternateUsb(int pri, int altUsb);
int setArchiveDecimatePri(int diskmask, int pri, float frac);
int setArchiveGlobalDecimate(int pri, float frac);
int setTelemPriEncodingType(int pri, int encType, int encClockType);
int setPidGoalScaleFactor(int surf, int dac, float scaleFactor);
int setLosdPriorityBandwidth(int pri, int bw);
int setSipdPriorityBandwidth(int pri, int bw);
int setSipdHkTelemOrder(int hk, int order);
int setSipdHkTelemMaxPackets(int hk, int numPackets);
int killDataPrograms();
int startDataPrograms();
int tryAndMountSatadrives();
void actuallyWriteCommandEcho(CommandEcho_t *echo, const char *mainDir, const char *linkDir);

int makeNewRunDirs();
int getNewRunNumber();
int getLatestEventNumber();
void handleBadSigs(int sig);
int sortOutPidFile(char *progName);



#define MAX_COMMANNDS 200  //Hard to believe we can get to a hundred

/* Global variables */
int cmdLengths[NUM_ANITA_COMMANDS];

CommandStruct_t theCmds[MAX_COMMANNDS];

int numCmdsInConfig=NUM_ANITA_COMMANDS;


//Debugging Output
int printToScreen=1;
int verbosity=1;

int disableHelium1;
int disableUsb;
int disableHelium2;
int disableNtu;
char usbName[FILENAME_MAX];
char ntuName[FILENAME_MAX];

//Needed for commands
int pidGoals[3];
int surfTrigBandMasks[ACTIVE_SURFS];
int dynamicL2ThresholdOverRate[PHI_SECTORS]={20000,20000,20000,20000,20000,20000,20000,20000,20000,20000,20000,20000,20000,20000,20000,20000};
int dynamicL2ThresholdUnderRate[PHI_SECTORS]={10000,10000,10000,10000,10000,10000,10000,10000,10000,10000,10000,10000,10000,10000,10000,10000};
int priDiskEncodingType[NUM_PRIORITIES];
int priTelemEncodingType[NUM_PRIORITIES];
int priTelemClockEncodingType[NUM_PRIORITIES];
float priorityFractionGlobalDecimate[NUM_PRIORITIES];
float priorityFractionDeleteHelium1[NUM_PRIORITIES];
float priorityFractionDeleteHelium2[NUM_PRIORITIES];
float priorityFractionDeleteUsb[NUM_PRIORITIES];
float priorityFractionDeleteNtu[NUM_PRIORITIES];
float priorityFractionDeletePmc[NUM_PRIORITIES];
int priDiskEventMask[NUM_PRIORITIES];
int alternateUsbs[NUM_PRIORITIES];
float pidGoalScaleFactors[ACTIVE_SURFS][SCALERS_PER_SURF];
int losdBandwidths[NUM_PRIORITIES];
int sipdBandwidths[NUM_PRIORITIES];
int sipdHkTelemOrder[22];
int sipdHkTelemMaxCopy[22];
float dacIGain[3];  // integral gain
float dacPGain[3];  // proportional gain
float dacDGain[3];  // derivative gain
int dacIMax[3];  // maximum intergrator state
int dacIMin[3]; // minimum integrator state

//Prioritizerd
float phiArrayDeg[48]={0};
float rArray[48]={0};
float zArray[48]={0};
float priorityParamsLowBinEdge[10];
float priorityParamsHighBinEdge[10];

int skipBlastRatioHPol[16];
int skipBlastRatioVPol[16];
float staticNotchesLowEdgeMHz[10];
float staticNotchesHighEdgeMHz[10];


//For GPS phi masking
int numSources=1;
float sourceLats[MAX_PHI_SOURCES]={0};
float sourceLongs[MAX_PHI_SOURCES]={0};
float sourceAlts[MAX_PHI_SOURCES]={0};
float sourceDistKm[MAX_PHI_SOURCES]={0};
int sourcePhiWidth[MAX_PHI_SOURCES]={0};

int hkDiskBitMask;
int eventDiskBitMask;
AnitaHkWriterStruct_t cmdWriter;


int diskBitMasks[DISK_TYPES]={HELIUM1_DISK_MASK,HELIUM2_DISK_MASK,USB_DISK_MASK,PMC_DISK_MASK,NTU_DISK_MASK};
char *diskNames[DISK_TYPES]={HELIUM1_DATA_MOUNT,HELIUM2_DATA_MOUNT,USB_DATA_MOUNT,SAFE_DATA_MOUNT,NTU_DATA_MOUNT};


int main (int argc, char *argv[])
{
  CommandStruct_t lastCmd;
  time_t lastCmdTime=0;
  time_t nowTime;
  int retVal;
  int count,ind=0;
  int numCmds;
  char logMessage[1024];
  /* Log stuff */
  char *progName=basename(argv[0]);
  retVal=sortOutPidFile(progName);
  if(retVal<0) {
    return -1; //Already have a Cmdd running
  }


  /* Setup log */
  setlogmask(LOG_UPTO(LOG_DEBUG));
  openlog (progName, LOG_PID, ANITA_LOG_FACILITY) ;

  syslog(LOG_INFO,"Starting Cmdd");

  /* Set signal handlers */
  signal(SIGUSR1, sigUsr1Handler);
  signal(SIGUSR2, sigUsr2Handler);
  signal(SIGTERM, handleBadSigs);
  signal(SIGINT, handleBadSigs);
  signal(SIGSEGV, handleBadSigs);



  makeDirectories(LOSD_CMD_ECHO_TELEM_LINK_DIR);
  makeDirectories(SIPD_CMD_ECHO_TELEM_LINK_DIR);
  makeDirectories(OPENPORTD_CMD_ECHO_TELEM_LINK_DIR);
  makeDirectories(CMDD_COMMAND_LINK_DIR);
  makeDirectories(REQUEST_TELEM_LINK_DIR);
  makeDirectories(PLAYBACK_LINK_DIR);
  makeDirectories(LOGWATCH_LINK_DIR);
  makeDirectories(TUFF_RAWCMD_LINK_DIR);

  do {
    retVal=readConfig();
    currentState=PROG_STATE_RUN;
    while(currentState==PROG_STATE_RUN) {
      //Check for new commands
      if((numCmds=checkForNewCommand())) {
	if(printToScreen)
	  printf("Got %d commands\n",numCmds);
	//Got at least one command
	for(count=0;count<numCmds;count++) {
	  if(printToScreen)
	    printf("Checking cmd %d, (%#x)\n",count,theCmds[count].cmd[0]);
	  printf("In loop count=%d\tnumCmds=%d\n",count,numCmds);


	  sprintf(logMessage,"Checking Cmd (%d bytes from SIPd %d)",
		  theCmds[count].numCmdBytes,
		  theCmds[count].fromSipd);


	  if(checkCommand(&theCmds[count])) {
	    for(ind=0;ind<theCmds[count].numCmdBytes;ind++)
	      sprintf(logMessage,"%s %d",logMessage,(int)theCmds[count].cmd[ind]);

	    syslog(LOG_INFO,"%s",logMessage);

	    if(printToScreen)
	      printf("cmd %d good , (%#x)\n",count,theCmds[count].cmd[0]);
	    syslog(LOG_INFO,"Got Cmd %d, numBytes %d\n",
		   theCmds[count].cmd[0],theCmds[count].numCmdBytes);

	    time(&nowTime);
	    if((nowTime-lastCmdTime)>30 ||
	       !sameCommand(&theCmds[count],&lastCmd)) {

	      retVal=executeCommand(&theCmds[count]);
	      lastCmdTime=retVal;
	      copyCommand(&theCmds[count],&lastCmd);
	    }
	    else {
	      printf("Same command: skipping\n");
	      retVal=lastCmdTime;
	    }
	    if(printToScreen)
	      printf("Writing command echo %d\n",count);
	    writeCommandEcho(&theCmds[count],retVal);

	  }
	}

      }
      usleep(1000);
    }

  } while(currentState==PROG_STATE_INIT);
  closeHkFilesAndTidy(&cmdWriter);
  unlink(CMDD_PID_FILE);
  syslog(LOG_INFO,"Cmdd terminating");
  return 0;
}


int checkForNewCommand() {
  static int wd=0;
  int uptoCmd,retVal=0;
  char currentFilename[FILENAME_MAX];
  char currentLinkname[FILENAME_MAX];
  int numCmdLinks=0;
  char *tempString;

  if(wd==0) {
    //First time need to prep the watcher directory
    wd=setupLinkWatchDir(CMDD_COMMAND_LINK_DIR);
    if(wd<=0) {
      fprintf(stderr,"Unable to watch %s\n",CMDD_COMMAND_LINK_DIR);
      syslog(LOG_ERR,"Unable to watch %s\n",CMDD_COMMAND_LINK_DIR);
      exit(0);
    }
    numCmdLinks=getNumLinks(wd);
  }

  //Check for new inotify events
  retVal=checkLinkDirs(1,0);
  if(retVal || numCmdLinks)
    numCmdLinks=getNumLinks(wd);

  if(numCmdLinks) {
    printf("There are %d cmd links\n",numCmdLinks);
    for(uptoCmd=0;uptoCmd<numCmdLinks;uptoCmd++) {
      if(uptoCmd==MAX_COMMANNDS) break;
      tempString=getFirstLink(wd);
      sprintf(currentFilename,"%s/%s",CMDD_COMMAND_DIR,tempString);
      sprintf(currentLinkname,"%s/%s",CMDD_COMMAND_LINK_DIR,tempString);
      fillCommand(&theCmds[uptoCmd],currentFilename);
      removeFile(currentFilename);
      removeFile(currentLinkname);
      printf("Removing %s -- %s\n",currentFilename,currentLinkname);
    }
  }
  return numCmdLinks;
}

int checkCommand(CommandStruct_t *theCmd) {
  if (theCmd->numCmdBytes == 0)
  {
    syslog(LOG_ERR,"Wrong number of command bytes for %d is %d\n",theCmd->cmd[0], theCmd->numCmdBytes);
    return 0;
  }
  if(theCmd->numCmdBytes!=cmdLengths[(int)theCmd->cmd[0]]) {
    syslog(LOG_ERR,"Wrong number of command bytes for %d: expected %d, got %d\n",theCmd->cmd[0],cmdLengths[(int)theCmd->cmd[0]],theCmd->numCmdBytes);
    fprintf(stderr,"Wrong number of command bytes for %d: expected %d, got %d\n",theCmd->cmd[0],cmdLengths[(int)theCmd->cmd[0]],theCmd->numCmdBytes);
    return 0;
  }
  return 1;

}

int sortOutPidFile(char *progName)
{
  int retVal=checkPidFile(CMDD_PID_FILE);
  if(retVal) {
    fprintf(stderr,"%s already running (%d)\nRemove pidFile to over ride (%s)\n",progName,retVal,CMDD_PID_FILE);
    syslog(LOG_ERR,"%s already running (%d)\n",progName,retVal);
    return -1;
  }
  writePidFile(CMDD_PID_FILE);
  return 0;
}


int readConfig() {
  /* Config file thingies */
  int status=0;
  //    int retVal=0;
  KvpErrorCode kvpStatus=0;
  char* eString ;
  char *tempString;

  /* Load Config */
  kvpReset () ;
  status = configLoad (GLOBAL_CONF_FILE,"global") ;
  status &= configLoad ("Cmdd.config","lengths") ;
  status &= configLoad ("Cmdd.config","output") ;
  eString = configErrorString (status) ;

  if (status == CONFIG_E_OK) {


    tempString=kvpGetString("usbName");
    if(tempString) {
      strncpy(usbName,tempString,FILENAME_MAX);
    }
    else {
      syslog(LOG_ERR,"Couldn't get usbName");
      fprintf(stderr,"Couldn't get usbName\n");
    }



    hkDiskBitMask=kvpGetInt("hkDiskBitMask",1);
    eventDiskBitMask=kvpGetInt("eventDiskBitMask",1);

    disableHelium1=kvpGetInt("disableHelium1",0);
    if(disableHelium1) {
      hkDiskBitMask&=(~HELIUM1_DISK_MASK);
      eventDiskBitMask&=(~HELIUM1_DISK_MASK);
    }
    disableHelium2=kvpGetInt("disableHelium2",0);
    if(disableHelium2) {
      hkDiskBitMask&=(~HELIUM2_DISK_MASK);
      eventDiskBitMask&=(~HELIUM2_DISK_MASK);
    }
    disableUsb=kvpGetInt("disableUsb",0);
    if(disableUsb) {
      hkDiskBitMask&=(~USB_DISK_MASK);
      eventDiskBitMask&=(~USB_DISK_MASK);
    }
    disableNtu=kvpGetInt("disableNtu",0);
    if(disableNtu) {
      hkDiskBitMask&=(~NTU_DISK_MASK);
      eventDiskBitMask&=(~NTU_DISK_MASK);
    }



    printToScreen=kvpGetInt("printToScreen",0);
    verbosity=kvpGetInt("verbosity",0);
    kvpStatus=kvpGetIntArray ("cmdLengths",cmdLengths,&numCmdsInConfig);
    if(kvpStatus!=KVP_E_OK) {
      syslog(LOG_ERR,"Problem getting cmdLengths -- %s\n",
	     kvpErrorString(kvpStatus));
      fprintf(stderr,"Problem getting cmdLengths -- %s\n",
	      kvpErrorString(kvpStatus));
    }

  }
  else {
    syslog(LOG_ERR,"Error reading config file: %s\n",eString);
  }
  return status;

}

int executeCommand(CommandStruct_t *theCmd)
//Returns 0 if there was a problem and unixTime otherwise
{
  printf("Have command %d\n",theCmd->cmd[0]);

  time_t rawtime;
  int retVal=0;
  int ivalue;
  int ivalue2;
  int ivalue3;
  short svalue;
  float fvalue;
  unsigned int uvalue,utemp;
  char theCommand[FILENAME_MAX];


  printf("Command: %d\n", theCmd->cmd[0]);

  switch(theCmd->cmd[0]) {
  case CMD_MAKE_NEW_RUN_DIRS:
    makeNewRunDirs();
    time(&rawtime);
    return rawtime;
    break;
  case CMD_START_NEW_RUN:
  NEW_RUN:
    {
      killDataPrograms();
      retVal=makeNewRunDirs();
      if(retVal==-1) return 0;
      time(&rawtime);
      startDataPrograms();
      return rawtime;
    }
  case CMD_TAIL_VAR_LOG_MESSAGES:
    ivalue=theCmd->cmd[1]+(theCmd->cmd[2]<<8);
    return requestJournalCtl(JOURNALCTL_NO_OPT,0,ivalue);
  case CMD_TAIL_VAR_LOG_ANITA:
    ivalue=theCmd->cmd[1]+(theCmd->cmd[2]<<8);
    return requestJournalCtl(JOURNALCTL_OPT_COMM,ID_ACQD,ivalue);
    //    return requestFile("/var/log/anita.log",ivalue);
  case LOG_REQUEST_COMMAND:
    ivalue=theCmd->cmd[1];
    ivalue2=theCmd->cmd[2]+(theCmd->cmd[3]<<8);
    return logRequestCommand(ivalue,ivalue2);
  case JOURNALCTL_REQUEST_COMMAND:
    ivalue=theCmd->cmd[1]; //< JournalctlOptionCommand_t;
    ivalue2=theCmd->cmd[2]+(theCmd->cmd[3]<<8); //< Num Lines
    ivalue3=theCmd->cmd[4]+(theCmd->cmd[5]<<8); //Option
    return requestJournalCtl(ivalue,ivalue3,ivalue2);
  case CMD_SHUTDOWN_HALT:
    //Halt
    time(&rawtime);
    killDataPrograms();
    sprintf(theCommand,"/sbin/halt");
    retVal=system(theCommand);
    syslog(LOG_ERR,"Sending /sbin/halt -- retVal == %d -- %s\n",retVal,
	   strerror(errno));
    if(retVal==-1) return 0;
    time(&rawtime);
    return rawtime;
  case CMD_REBOOT:
    //Reboot
    killDataPrograms();
    sprintf(theCommand,"/sbin/reboot");
    retVal=system(theCommand);
    syslog(LOG_ERR,"Sending /sbin/reboot -- retVal == %d -- %s\n",retVal,
	   strerror(errno));
    if(retVal==-1) return 0;
    time(&rawtime);
    return rawtime;
  case CMD_KILL_PROGS:
    //Kill prog
    ivalue=theCmd->cmd[1]+(theCmd->cmd[2]<<8) + (theCmd->cmd[3] << 16);
    retVal=killPrograms(ivalue);
    return retVal;
  case CMD_REALLY_KILL_PROGS:
    //Kill progs
    ivalue=theCmd->cmd[1]+(theCmd->cmd[2]<<8) + (theCmd->cmd[3] << 16);
    retVal=reallyKillPrograms(ivalue);
    return retVal;
  case CMD_RESPAWN_PROGS:
    //Respawn progs
    ivalue=theCmd->cmd[1]+(theCmd->cmd[2]<<8) + (theCmd->cmd[3] << 16);
    retVal=respawnPrograms(ivalue);
    return retVal;
  case CMD_START_PROGS:
    //Start progs
    ivalue=theCmd->cmd[1]+(theCmd->cmd[2]<<8) + (theCmd->cmd[3] << 16);
    retVal=startPrograms(ivalue);
    return retVal;
  case CMD_DISABLE_DISK:
    ivalue=theCmd->cmd[1];
    ivalue2=theCmd->cmd[2];//+(theCmd->cmd[3]<<8);
    printf("CMD_DISABLE_DISK: %d %#x %#x\n",theCmd->cmd[0],theCmd->cmd[1],theCmd->cmd[2]);
    printf("ivalue2==%d ivalue==%d\n",ivalue2,ivalue);
    //RJN change to switch order of arguments

    return disableDisk(ivalue2,ivalue);
  case CMD_MOUNT_ARGH:
    return tryAndMountSatadrives();
  case CMD_MOUNT_NEXT_NTU:   //RJN changed number of params
    printf("mount next ntu\n");
    ivalue=theCmd->cmd[1];
    return mountNextNtu(ivalue);
  case CMD_MOUNT_NEXT_USB:
    ivalue=theCmd->cmd[1];
    return mountNextUsb(ivalue);
  case CMD_EVENT_DISKTYPE:
    ivalue=theCmd->cmd[1];
    ivalue2=theCmd->cmd[2]+(theCmd->cmd[3]<<8);
    if(ivalue>=0 && ivalue<=2)
      return setEventDiskBitMask(ivalue,ivalue2);
    return 0;
  case CMD_HK_DISKTYPE:
    ivalue=theCmd->cmd[1];
    ivalue2=theCmd->cmd[2]+(theCmd->cmd[3]<<8);
    if(ivalue>=0 && ivalue<=2)
      return setHkDiskBitMask(ivalue,ivalue2);
    return 0;
  case ARCHIVE_STORAGE_TYPE:
    ivalue=theCmd->cmd[1];
    configModifyInt("Archived.config","archived","onboardStorageType",ivalue,&rawtime);
    respawnPrograms(ARCHIVED_ID_MASK);
    return rawtime;
  case ARCHIVE_PRI_DISK:
    ivalue=theCmd->cmd[1];
    ivalue2=theCmd->cmd[2]+(theCmd->cmd[3]<<8);
    return setPriDiskMask(ivalue,ivalue2);
  case ARCHIVE_PRI_ENC_TYPE:
    ivalue=theCmd->cmd[1];
    ivalue2=theCmd->cmd[2]+(theCmd->cmd[3]<<8);
    return setPriEncodingType(ivalue,ivalue2);

  case ARCHIVE_DECIMATE_PRI:
    ivalue=theCmd->cmd[1];
    ivalue2=theCmd->cmd[2];
    ivalue3=theCmd->cmd[3]+(theCmd->cmd[4]<<8);
    fvalue=((float)ivalue3)/1000.;
    return setArchiveDecimatePri(ivalue,ivalue2,fvalue);
  case ARCHIVE_GLOBAL_DECIMATE:
    ivalue=theCmd->cmd[1];
    ivalue2=theCmd->cmd[2]+(theCmd->cmd[3]<<8);
    fvalue=((float)ivalue2)/1000.;
    return setArchiveGlobalDecimate(ivalue,fvalue);
  case TELEM_TYPE:
    ivalue=theCmd->cmd[1];
    configModifyInt("Archived.config","archived","telemType",ivalue,&rawtime);
    respawnPrograms(ARCHIVED_ID_MASK);
    return rawtime;
  case TELEM_PRI_ENC_TYPE:
    ivalue=theCmd->cmd[1];
    ivalue2=theCmd->cmd[2]+(theCmd->cmd[3]<<8);
    ivalue3=theCmd->cmd[4]+(theCmd->cmd[5]<<8);
    return setTelemPriEncodingType(ivalue,ivalue2,ivalue3);
  case ARCHIVE_PPS_PRIORITIES:
    ivalue=theCmd->cmd[1];
    ivalue2=theCmd->cmd[2];
    ivalue3=theCmd->cmd[3];
    if(ivalue<0 || ivalue>9) ivalue=-1;
    if(ivalue2<0 || ivalue2>9) ivalue2=-1;
    if(ivalue3<0 || ivalue3>9) ivalue3=-1;
    configModifyInt("Archived.config","archived","priorityPPS1",ivalue,NULL);
    configModifyInt("Archived.config","archived","priorityPPS2",ivalue2,&rawtime);
    configModifyInt("Archived.config","archived","prioritySoft",ivalue3,&rawtime);
    retVal=sendSignal(ID_ARCHIVED,SIGUSR1);
    return rawtime;
  case ARCHIVE_PPS_DECIMATE:
    ivalue=theCmd->cmd[1];
    ivalue2=theCmd->cmd[2];
    if(ivalue<1 || ivalue>2) return -1;
    ivalue3=theCmd->cmd[3]+(theCmd->cmd[4]<<8);
    fvalue=((float)ivalue3)/1000.;
    printf("decimate -- %d %d %d %f\n",ivalue,ivalue2,ivalue3,fvalue);
    if(ivalue2==1) {
      if(ivalue==1)
	configModifyFloat("Archived.config","archived","pps1FractionDelete",fvalue,&rawtime);
      else if(ivalue==2)
	configModifyFloat("Archived.config","archived","pps1FractionTelem",fvalue,&rawtime);
      else {
	syslog(LOG_ERR,"Invalid ARCHIVE_PPS_DECIMATE argument %d %d",ivalue,ivalue2);
	return 0;
      }
    }
    else if(ivalue2==2) {
      if(ivalue==1)
	configModifyFloat("Archived.config","archived","pps2FractionDelete",fvalue,&rawtime);
      else if(ivalue==2)
	configModifyFloat("Archived.config","archived","pps2FractionTelem",fvalue,&rawtime);
      else {
	syslog(LOG_ERR,"Invalid ARCHIVE_PPS_DECIMATE argument %d %d",ivalue,ivalue2);
	return 0;
      }
    }
    else if(ivalue2==3) {
      if(ivalue==1)
	configModifyFloat("Archived.config","archived","softFractionDelete",fvalue,&rawtime);
      else if(ivalue==2)
	configModifyFloat("Archived.config","archived","softFractionTelem",fvalue,&rawtime);
      else {
	syslog(LOG_ERR,"Invalid ARCHIVE_PPS_DECIMATE argument %d %d",ivalue,ivalue2);
	return 0;
      }
    }
    else {
      syslog(LOG_ERR,"Invalid ARCHIVE_PPS_DECIMATE argument %d",ivalue);
      return 0;
    }
    retVal=sendSignal(ID_ARCHIVED,SIGUSR1);
    return rawtime;


    /*Modification for anita3 calibd - BenS 04/07/2014*/
  case CMD_TURN_TUFFS_ON:
    // modify both state flags, since relays lines are merged before reaching iRFCMS
    configModifyInt("Calibd.config","relays","stateTuff1",1,&rawtime);
    configModifyInt("Calibd.config","relays","stateTuff2",1,&rawtime);
    retVal=sendSignal(ID_CALIBD,SIGUSR1);
    if(retVal) return 0;
    return rawtime;
  case CMD_TURN_TUFFS_OFF:
    // modify both state flags, since relays lines are merged before reaching iRFCMS
    configModifyInt("Calibd.config","relays","stateTuff1",0,&rawtime);
    configModifyInt("Calibd.config","relays","stateTuff2",0,&rawtime);
    retVal=sendSignal(ID_CALIBD,SIGUSR1);
    if(retVal) return 0;
    return rawtime;
  case CMD_TURN_AMPAS_ON:
    // May as well modify both state flags, since relays lines are merged before reaching iRFCMS
    configModifyInt("Calibd.config","relays","stateBZAmpa1",1,&rawtime);
    configModifyInt("Calibd.config","relays","stateBZAmpa2",1,&rawtime);
    configModifyInt("Calibd.config","relays","stateNTUAmpa",1,&rawtime);
    retVal=sendSignal(ID_CALIBD,SIGUSR1);
    if(retVal) return 0;
    return rawtime;
  case CMD_TURN_AMPAS_OFF:
    // May as well modify both state flags, since relays lines are merged before reaching iRFCMS
    configModifyInt("Calibd.config","relays","stateBZAmpa1",0,&rawtime);
    configModifyInt("Calibd.config","relays","stateBZAmpa2",0,&rawtime);
    configModifyInt("Calibd.config","relays","stateNTUAmpa",0,&rawtime);
    retVal=sendSignal(ID_CALIBD,SIGUSR1);
    if(retVal) return 0;
    return rawtime;
  case CMD_TURN_NTU_AMPAS_ON:
    configModifyInt("Calibd.config","relays","stateNTUAmpa",1,&rawtime);
    retVal=sendSignal(ID_CALIBD,SIGUSR1);
    if(retVal) return 0;
    return rawtime;
  case CMD_TURN_NTU_AMPAS_OFF:
    configModifyInt("Calibd.config","relays","stateNTUAmpa",0,&rawtime);
    retVal=sendSignal(ID_CALIBD,SIGUSR1);
    if(retVal) return 0;
    return rawtime;
  case CMD_TURN_SHORT_BOARDS_ON:
    configModifyInt("Calibd.config","relays","stateSB",1,&rawtime);
    retVal=sendSignal(ID_CALIBD,SIGUSR1);
    if(retVal) return 0;
    return rawtime;
  case CMD_TURN_SHORT_BOARDS_OFF:
    configModifyInt("Calibd.config","relays","stateSB",0,&rawtime);
    retVal=sendSignal(ID_CALIBD,SIGUSR1);
    if(retVal) return 0;
    return rawtime;
  case CMD_TURN_NTU_SSD_5V_ON:
    configModifyInt("Calibd.config","relays","stateNTUSSD5V",1,&rawtime);
    retVal=sendSignal(ID_CALIBD,SIGUSR1);
    if(retVal) return 0;
    return rawtime;
  case CMD_TURN_NTU_SSD_5V_OFF:
    configModifyInt("Calibd.config","relays","stateNTUSSD5V",0,&rawtime);
    retVal=sendSignal(ID_CALIBD,SIGUSR1);
    if(retVal) return 0;
    return rawtime;
  case CMD_TURN_NTU_SSD_12V_ON:
    configModifyInt("Calibd.config","relays","stateNTUSSD12V",1,&rawtime);
    retVal=sendSignal(ID_CALIBD,SIGUSR1);
    if(retVal) return 0;
    return rawtime;
  case CMD_TURN_NTU_SSD_12V_OFF:
    configModifyInt("Calibd.config","relays","stateNTUSSD12V",0,&rawtime);
    retVal=sendSignal(ID_CALIBD,SIGUSR1);
    if(retVal) return 0;
    return rawtime;


  case CMD_TURN_ALL_ON:
    //turnAllOn
    // modify both state flags, since relays lines are merged before reaching iRFCMS
    configModifyInt("Calibd.config","relays","stateTuff1",1,&rawtime);
    configModifyInt("Calibd.config","relays","stateTuff2",1,&rawtime);
    configModifyInt("Calibd.config","relays","stateBZAmpa1",1,&rawtime);
    configModifyInt("Calibd.config","relays","stateBZAmpa2",1,&rawtime);
    configModifyInt("Calibd.config","relays","stateNTUAmpa",1,&rawtime);
    configModifyInt("Calibd.config","relays","stateSB",1,&rawtime);
    configModifyInt("Calibd.config","relays","stateNTUSSD5V",1,&rawtime);
    configModifyInt("Calibd.config","relays","stateNTUSSD12V",1,&rawtime);


    retVal=sendSignal(ID_CALIBD,SIGUSR1);
    if(retVal) return 0;
    return rawtime;

  case CMD_TURN_ALL_OFF:
    //turnAllOff
    // modify both state flags, since relays lines are merged before reaching iRFCMS
    configModifyInt("Calibd.config","relays","stateTuff1",0,&rawtime);
    configModifyInt("Calibd.config","relays","stateTuff2",0,&rawtime);
    configModifyInt("Calibd.config","relays","stateBZAmpa1",0,&rawtime);
    configModifyInt("Calibd.config","relays","stateBZAmpa2",0,&rawtime);
    configModifyInt("Calibd.config","relays","stateNTUAmpa",0,&rawtime);
    configModifyInt("Calibd.config","relays","stateSB",0,&rawtime);
    configModifyInt("Calibd.config","relays","stateNTUSSD5V",0,&rawtime);
    configModifyInt("Calibd.config","relays","stateNTUSSD12V",0,&rawtime);


    retVal=sendSignal(ID_CALIBD,SIGUSR1);
    if(retVal) return 0;
    return rawtime;

  case SET_CALIB_WRITE_PERIOD:
    ivalue=theCmd->cmd[1]+(theCmd->cmd[2]<<8);
    configModifyInt("Calibd.config","calibd","writePeriod",ivalue,&rawtime);
    retVal=sendSignal(ID_CALIBD,SIGUSR1);
    if(retVal) return 0;
    return rawtime;


  case SET_ADU5_PAT_PERIOD:
    ivalue=theCmd->cmd[1]+(theCmd->cmd[2]<<8);
    fvalue=((float)ivalue)/10.;
    //	    printf("ivalue: %d %x\t %x %x\n",ivalue,ivalue,theCmd->cmd[1],theCmd->cmd[2]);
    configModifyFloat("GPSd.config","adu5a","patPeriod",fvalue,&rawtime);
    configModifyFloat("GPSd.config","adu5b","patPeriod",fvalue,&rawtime);
    retVal=sendSignal(ID_GPSD,SIGUSR1);
    if(retVal) return 0;
    return rawtime;
  case SET_ADU5_SAT_PERIOD:
    ivalue=theCmd->cmd[1]+(theCmd->cmd[2]<<8);
    configModifyInt("GPSd.config","adu5a","satPeriod",ivalue,&rawtime);
    configModifyInt("GPSd.config","adu5b","satPeriod",ivalue,&rawtime);
    retVal=sendSignal(ID_GPSD,SIGUSR1);
    if(retVal) return 0;
    return rawtime;
  case SET_ADU5_VTG_PERIOD:
    ivalue=theCmd->cmd[1]+(theCmd->cmd[2]<<8);
    configModifyFloat("GPSd.config","adu5a","vtgPeriod",((float)ivalue),&rawtime);
    configModifyFloat("GPSd.config","adu5b","vtgPeriod",((float)ivalue),&rawtime);
    retVal=sendSignal(ID_GPSD,SIGUSR1);
    if(retVal) return 0;
    return rawtime;
  case SET_G12_PPS_PERIOD:
    ivalue=theCmd->cmd[1]+(theCmd->cmd[2]<<8);
    configModifyFloat("GPSd.config","g12","ppsPeriod",((float)ivalue)/1000.,&rawtime);
    retVal=sendSignal(ID_GPSD,SIGUSR1);
    if(retVal) return 0;
    return rawtime;
  case SET_G12_POS_PERIOD:
    ivalue=theCmd->cmd[1]+(theCmd->cmd[2]<<8);
    configModifyFloat("GPSd.config","g12","posPeriod",((float)ivalue)/1000.,&rawtime);
    retVal=sendSignal(ID_GPSD,SIGUSR1);
    if(retVal) return 0;
    return rawtime;
  case SET_G12_PPS_OFFSET:
    ivalue=theCmd->cmd[1]+(theCmd->cmd[2]<<8);
    ivalue2=theCmd->cmd[3]+(theCmd->cmd[4]<<8);
    ivalue3=theCmd->cmd[5];
    fvalue=((float)ivalue) +((float)ivalue2)/10000.;
    if(ivalue3==1) fvalue*=-1;

    //	    printf("%d %d %d -- Offset %3.4f\n",ivalue, ivalue2, ivalue3,fvalue);
    configModifyFloat("GPSd.config","g12","ppsOffset",fvalue,&rawtime);
    retVal=sendSignal(ID_GPSD,SIGUSR1);
    if(retVal) return 0;
    return rawtime;
  case SET_ADU5_CALIBRATION_12:
    ivalue=theCmd->cmd[1];
    svalue=theCmd->cmd[2];
    svalue|=(theCmd->cmd[3]<<8);
    if(ivalue==1)
      configModifyFloat("GPSd.config","adu5a","calibV12_1",((float)svalue)/1000.,&rawtime);
    else if(ivalue==2)
      configModifyFloat("GPSd.config","adu5b","calibV12_1",((float)svalue)/1000.,&rawtime);
    svalue=theCmd->cmd[4];
    svalue|=(theCmd->cmd[5]<<8);
    if(ivalue==1)
      configModifyFloat("GPSd.config","adu5a","calibV12_2",((float)svalue)/1000.,&rawtime);
    else if(ivalue==2)
      configModifyFloat("GPSd.config","adu5b","calibV12_2",((float)svalue)/1000.,&rawtime);

    svalue=theCmd->cmd[6];
    svalue|=(theCmd->cmd[7]<<8);
    if(ivalue==1)
      configModifyFloat("GPSd.config","adu5a","calibV12_3",((float)svalue)/1000.,&rawtime);
    else if(ivalue==2)
      configModifyFloat("GPSd.config","adu5b","calibV12_3",((float)svalue)/1000.,&rawtime);
    retVal=sendSignal(ID_GPSD,SIGUSR1);
    if(retVal) return 0;
    return rawtime;
  case SET_ADU5_CALIBRATION_13:
    ivalue=theCmd->cmd[1];
    svalue=theCmd->cmd[2];
    svalue|=(theCmd->cmd[3]<<8);
    if(ivalue==1)
      configModifyFloat("GPSd.config","adu5a","calibV13_1",((float)svalue)/1000.,&rawtime);
    else if(ivalue==2)
      configModifyFloat("GPSd.config","adu5b","calibV13_1",((float)svalue)/1000.,&rawtime);
    svalue=theCmd->cmd[4];
    svalue|=(theCmd->cmd[5]<<8);
    if(ivalue==1)
      configModifyFloat("GPSd.config","adu5a","calibV13_2",((float)svalue)/1000.,&rawtime);
    else if(ivalue==2)
      configModifyFloat("GPSd.config","adu5b","calibV13_2",((float)svalue)/1000.,&rawtime);

    svalue=theCmd->cmd[6];
    svalue|=(theCmd->cmd[7]<<8);
    if(ivalue==1)
      configModifyFloat("GPSd.config","adu5a","calibV13_3",((float)svalue)/1000.,&rawtime);
    else if(ivalue==2)
      configModifyFloat("GPSd.config","adu5b","calibV13_3",((float)svalue)/1000.,&rawtime);
    retVal=sendSignal(ID_GPSD,SIGUSR1);
    if(retVal) return 0;
    return rawtime;
  case SET_ADU5_CALIBRATION_14:
    ivalue=theCmd->cmd[1];
    svalue=theCmd->cmd[2];
    svalue|=(theCmd->cmd[3]<<8);
    if(ivalue==1)
      configModifyFloat("GPSd.config","adu5a","calibV14_1",((float)svalue)/1000.,&rawtime);
    else if(ivalue==2)
      configModifyFloat("GPSd.config","adu5b","calibV14_1",((float)svalue)/1000.,&rawtime);
    svalue=theCmd->cmd[4];
    svalue|=(theCmd->cmd[5]<<8);
    if(ivalue==1)
      configModifyFloat("GPSd.config","adu5a","calibV14_2",((float)svalue)/1000.,&rawtime);
    else if(ivalue==2)
      configModifyFloat("GPSd.config","adu5b","calibV14_2",((float)svalue)/1000.,&rawtime);

    svalue=theCmd->cmd[6];
    svalue|=(theCmd->cmd[7]<<8);
    if(ivalue==1)
      configModifyFloat("GPSd.config","adu5a","calibV14_3",((float)svalue)/1000.,&rawtime);
    else if(ivalue==2)
      configModifyFloat("GPSd.config","adu5b","calibV14_3",((float)svalue)/1000.,&rawtime);
    retVal=sendSignal(ID_GPSD,SIGUSR1);
    if(retVal) return 0;
    return rawtime;
  case SET_HK_PERIOD:
    ivalue=theCmd->cmd[1]+(theCmd->cmd[2]<<8);
    configModifyInt("Hkd.config","hkd","readoutPeriod",ivalue,&rawtime);
    retVal=sendSignal(ID_HKD,SIGUSR1);
    if(retVal) return 0;
    return rawtime;
  case SET_HK_CAL_PERIOD:
    ivalue=theCmd->cmd[1]+(theCmd->cmd[2]<<8);
    configModifyInt("Hkd.config","hkd","calibrationPeriod",ivalue,&rawtime);
    retVal=sendSignal(ID_HKD,SIGUSR1);
    if(retVal) return 0;
    return rawtime;
  case SET_HK_TELEM_EVERY:
    ivalue=theCmd->cmd[1];//+(theCmd->cmd[2]<<8);
    configModifyInt("Hkd.config","hkd","telemEvery",ivalue,&rawtime);
    retVal=sendSignal(ID_HKD,SIGUSR1);
    if(retVal) return 0;
    return rawtime;
  case ACQD_ADU5_TRIG_FLAG:
    ivalue=theCmd->cmd[1];
    configModifyInt("Acqd.config","trigger","enablePPS1Trigger",ivalue,&rawtime);
    retVal=sendSignal(ID_ACQD,SIGUSR1);
    if(retVal) return 0;
    return rawtime;
  case ACQD_G12_TRIG_FLAG:
    ivalue=theCmd->cmd[1];
    configModifyInt("Acqd.config","trigger","enablePPS2Trigger",ivalue,&rawtime);
    retVal=sendSignal(ID_ACQD,SIGUSR1);
    if(retVal) return 0;
    return rawtime;
  case ACQD_SOFT_TRIG_FLAG:
    ivalue=theCmd->cmd[1];
    configModifyInt("Acqd.config","trigger","sendSoftTrigger",ivalue,&rawtime);
    retVal=sendSignal(ID_ACQD,SIGUSR1);
    if(retVal) return 0;
    return rawtime;
  case ACQD_SOFT_TRIG_PERIOD:
    ivalue=theCmd->cmd[1];
    configModifyInt("Acqd.config","trigger","softTrigPeriod",ivalue,&rawtime);
    retVal=sendSignal(ID_ACQD,SIGUSR1);
    if(retVal) return 0;
    return rawtime;

  case ACQD_PEDESTAL_RUN:
    ivalue=theCmd->cmd[1];
    ivalue&=0x1;
    configModifyInt("Acqd.config","acqd","pedestalMode",ivalue,&rawtime);
    //	    retVal=sendSignal(ID_ACQD,SIGUSR1);
    goto NEW_RUN;
    if(retVal) return 0;
    return rawtime;
  case ACQD_THRESHOLD_SCAN:
    ivalue=theCmd->cmd[1];
    ivalue&=0x1;
    configModifyInt("Acqd.config","thresholdScan","doThresholdScan",ivalue,&rawtime);
    //	    retVal=sendSignal(ID_ACQD,SIGUSR1);
    goto NEW_RUN;
    if(retVal) return 0;
    return rawtime;


  case ACQD_REPROGRAM_TURF:
    ivalue=theCmd->cmd[1];
    configModifyInt("Acqd.config","acqd","reprogramTurf",ivalue,&rawtime);
    retVal=sendSignal(ID_ACQD,SIGUSR1);
    if(retVal) return 0;
    return rawtime;
  case ACQD_SURFHK_TELEM_EVERY:
    ivalue=theCmd->cmd[1];
    configModifyInt("Acqd.config","acqd","surfHkTelemEvery",ivalue,&rawtime);
    retVal=sendSignal(ID_ACQD,SIGUSR1);
    if(retVal) return 0;
    return rawtime;
  case ACQD_TURFHK_TELEM_EVERY:
    ivalue=theCmd->cmd[1];
    configModifyInt("Acqd.config","acqd","turfRateTelemEvery",ivalue,&rawtime);
    retVal=sendSignal(ID_ACQD,SIGUSR1);
    if(retVal) return 0;
    return rawtime;
  case ACQD_NUM_EVENTS_PEDESTAL:
    ivalue=theCmd->cmd[1]+(theCmd->cmd[2]<<8);
    configModifyInt("Acqd.config","pedestal","numPedEvents",ivalue,&rawtime);
    retVal=sendSignal(ID_ACQD,SIGUSR1);
    if(retVal) return 0;
    return rawtime;
  case ACQD_THRESH_SCAN_STEP_SIZE:
    ivalue=theCmd->cmd[1];
    configModifyInt("Acqd.config","thresholdScan","thresholdScanStepSize",ivalue,&rawtime);
    retVal=sendSignal(ID_ACQD,SIGUSR1);
    if(retVal) return 0;
    return rawtime;
  case ACQD_THRESH_SCAN_POINTS_PER_STEP:
    ivalue=theCmd->cmd[1];
    configModifyInt("Acqd.config","thresholdScan","thresholdScanPointsPerStep",ivalue,&rawtime);
    retVal=sendSignal(ID_ACQD,SIGUSR1);
    if(retVal) return 0;
    return rawtime;

  case SIPD_CONTROL_COMMAND:
    return executeSipdControlCommand(theCmd->cmd[1],&(theCmd->cmd[2]));
  case LOSD_CONTROL_COMMAND:
    return executeLosdControlCommand(theCmd->cmd[1],&(theCmd->cmd[2]));
  case GPSD_EXTRA_COMMAND:
    return executeGpsdExtracommand(theCmd->cmd[1],&(theCmd->cmd[2]));
  case CALIBD_EXTRA_COMMAND:
    return executeCalibdExtraCommand(theCmd->cmd[1],&(theCmd->cmd[2]));



  case MONITORD_RAMDISK_KILL_ACQD:
    ivalue=theCmd->cmd[1];
    configModifyInt("Monitord.config","monitord","ramdiskKillAcqd",ivalue,&rawtime);
    retVal=sendSignal(ID_MONITORD,SIGUSR1);
    return rawtime;
  case MONITORD_RAMDISK_DUMP_DATA:
    ivalue=theCmd->cmd[1];
    configModifyInt("Monitord.config","monitord","ramdiskDumpData",ivalue,&rawtime);
    retVal=sendSignal(ID_MONITORD,SIGUSR1);
    return rawtime;


  case MONITORD_MAX_ACQD_WAIT:
    ivalue=theCmd->cmd[1]+(theCmd->cmd[2]<<8);
    configModifyInt("Monitord.config","monitord","maxAcqdWaitPeriod",ivalue,&rawtime);
    retVal=sendSignal(ID_MONITORD,SIGUSR1);
    return rawtime;
  case MONITORD_PERIOD:
    ivalue=theCmd->cmd[1];
    configModifyInt("Monitord.config","monitord","monitorPeriod",ivalue,&rawtime);
    retVal=sendSignal(ID_MONITORD,SIGUSR1);
    return rawtime;
  case MONITORD_USB_THRESH:
    ivalue=theCmd->cmd[1];
    configModifyInt("Monitord.config","monitord","usbSwitchMB",ivalue,&rawtime);
    retVal=sendSignal(ID_MONITORD,SIGUSR1);
    return rawtime;
  case MONITORD_NTU_THRESH:
    ivalue=theCmd->cmd[1];
    ivalue2=theCmd->cmd[2];
    if(ivalue==0)
      configModifyInt("Monitord.config","monitord","ntuSwitchMB",ivalue2,&rawtime);
    else
      return -1;
    retVal=sendSignal(ID_MONITORD,SIGUSR1);
    return rawtime;
  case MONITORD_MAX_QUEUE:
    ivalue=theCmd->cmd[1]+(theCmd->cmd[2]<<8);
    configModifyInt("Monitord.config","monitord","maxEventQueueSize",ivalue,&rawtime);
    retVal=sendSignal(ID_MONITORD,SIGUSR1);
    return rawtime;
  case MONITORD_INODES_KILL_ACQD:
    ivalue=theCmd->cmd[1]+(theCmd->cmd[2]<<8);
    configModifyInt("Monitord.config","monitord","inodesKillAcqd",ivalue,&rawtime);
    retVal=sendSignal(ID_MONITORD,SIGUSR1);
    return rawtime;
  case MONITORD_INODES_DUMP_DATA:
    ivalue=theCmd->cmd[1]+(theCmd->cmd[2]<<8);
    configModifyInt("Monitord.config","monitord","inodesDumpData",ivalue,&rawtime);
    retVal=sendSignal(ID_MONITORD,SIGUSR1);
    return rawtime;
  case ACQD_RATE_COMMAND:
    return executeAcqdRateCommand(theCmd->cmd[1],&(theCmd->cmd[2]));
  case ACQD_EXTRA_COMMAND:
    return executeAcqdExtraCommand(theCmd->cmd[1],theCmd->cmd[2]);
  case GPS_PHI_MASK_COMMAND:
    return executeGpsPhiMaskCommand(theCmd->cmd[1],&(theCmd->cmd[2]));
  case PRIORITIZERD_COMMAND:
    ivalue=theCmd->cmd[1]; //Command num
    ivalue2=theCmd->cmd[2]+(theCmd->cmd[3]<<8); //command value
    ivalue3=theCmd->cmd[4]+(theCmd->cmd[5]<<8); //command value
    return executePrioritizerdCommand(ivalue,ivalue2,ivalue3);
  case PLAYBACKD_COMMAND:
    ivalue=theCmd->cmd[1]; //Command num
    utemp=(theCmd->cmd[2]);
    uvalue=utemp;
    utemp=(theCmd->cmd[3]);
    uvalue|=(utemp<<8);
    utemp=(theCmd->cmd[4]);
    uvalue|=(utemp<<16);
    utemp=(theCmd->cmd[5]);
    uvalue|=(utemp<<24);
    ivalue2=theCmd->cmd[6];//+(theCmd->cmd[3]<<8); //command value
    return executePlaybackCommand(ivalue,uvalue,ivalue2);
  case EVENTD_MATCH_GPS:
    ivalue=theCmd->cmd[1];
    if(ivalue<0 || ivalue>1) return -1;
    configModifyInt("Eventd.config","eventd","tryToMatchGps",ivalue,&rawtime);
    retVal=sendSignal(ID_EVENTD,SIGUSR1);
    return rawtime;
  case CLEAN_DIRS:
    cleanDirs();
    time(&rawtime);
    return rawtime;
  case CLEAR_RAMDISK:
    clearRamdisk();
    time(&rawtime);
    return rawtime;
  case SEND_CONFIG:
    ivalue=theCmd->cmd[1]+(theCmd->cmd[2]<<8) + (theCmd->cmd[3] << 16);
    return sendConfig(ivalue);
  case DEFAULT_CONFIG:
    ivalue=theCmd->cmd[1]+(theCmd->cmd[2]<<8) + (theCmd->cmd[3] << 16);
    return defaultConfig(ivalue);
  case SWITCH_CONFIG:
    ivalue=theCmd->cmd[1]+(theCmd->cmd[2]<<8) + (theCmd->cmd[3] << 16);
    return switchConfig(ivalue,theCmd->cmd[4]);
  case SAVE_CONFIG:
    ivalue=theCmd->cmd[1]+(theCmd->cmd[2]<<8) + (theCmd->cmd[3] << 16);
    return saveConfig(ivalue,theCmd->cmd[4]);
  case LAST_CONFIG:
    ivalue=theCmd->cmd[1]+(theCmd->cmd[2]<<8) + (theCmd->cmd[3] << 16);
    return lastConfig(ivalue);
  case  TUFFD_COMMAND:
    return executeTuffCommand(theCmd->cmd[1], theCmd->cmd+2);
  case  RTLD_COMMAND:
    return executeRTLCommand(theCmd->cmd[1], theCmd->cmd+2);

  default:
    syslog(LOG_WARNING,"Unrecognised command %d\n",theCmd->cmd[0]);
    return retVal;
  }
  return -1;
}


int cleanDirs()
{


  return 0;
}

int sendConfig(int progMask)
{
  time_t rawtime=0;
  int errorCount=0;
  ProgramId_t prog;
  char configFile[FILENAME_MAX];
  char *fullFilename;
  int testMask;

  //As a freebie I'll send down anitaSoft.config
  sprintf(configFile,"anitaSoft.config");
  fullFilename=configFileSpec(configFile);
  requestFile(fullFilename,0);
  printf("sendConfig %d\n",progMask);
  for(prog=ID_FIRST;prog<ID_NOT_AN_ID;prog++) {
    testMask=getIdMask(prog);
    //	printf("%d %d\n",progMask,testMask);
    if(progMask&testMask) {
      sprintf(configFile,"%s.config",getProgName(prog));
      printf("Trying to send config file -- %s\n",configFile);
      fullFilename=configFileSpec(configFile);
      rawtime=requestFile(fullFilename,0);
    }
  }
  if(errorCount) return 0;
  return rawtime;

}

int defaultConfig(int progMask)
{
  return switchConfig(progMask,0);
}

int switchConfig(int progMask, unsigned char configId)
{
  time_t rawtime;
  //    time(&rawtime);
  int retVal;
  int errorCount=0;
  ProgramId_t prog;
  char configFile[FILENAME_MAX];

  int testMask;

  printf("switchConfig %d %d\n",progMask,configId);
  for(prog=ID_FIRST;prog<ID_NOT_AN_ID;prog++) {
    testMask=getIdMask(prog);
    //	printf("%d %d\n",progMask,testMask);
    if(progMask&testMask) {
      printf("Switching to config %s.config.%d\n",getProgName(prog),configId);
      sprintf(configFile,"%s.config",getProgName(prog));
      retVal=configSwitch(configFile,configId,&rawtime);
      if(retVal!=CONFIG_E_OK)
	errorCount++;
      else {
	retVal=sendSignal(prog,SIGUSR1);
	if(retVal)
	  errorCount++;
      }

    }
  }
  if(errorCount) return 0;
  return rawtime;
}


int saveConfig(int progMask, unsigned char configId)
{
  //    if(configId<10) return -1;
  time_t rawtime=time(NULL);
  //    time(&rawtime);
  int retVal;
  int errorCount=0;
  ProgramId_t prog;
  char configFile[FILENAME_MAX];
  char configFileNew[FILENAME_MAX];

  int testMask;

  printf("saveConfig %d %d\n",progMask,configId);
  for(prog=ID_FIRST;prog<ID_NOT_AN_ID;prog++) {
    testMask=getIdMask(prog);
    //	printf("%d %d\n",progMask,testMask);
    if(progMask&testMask) {
      sprintf(configFile,"/home/anita/flightSoft/config/%s.config",
	      getProgName(prog));
      sprintf(configFileNew,"/home/anita/flightSoft/config/defaults/%s.config.%d",
	      getProgName(prog),configId);

      retVal=copyFileToFile(configFile,configFileNew);
      if(retVal!=0) {
	syslog(LOG_ERR,"Error copying %s %s -- %s\n",configFile,configFileNew,strerror(errno));
      }
    }
  }
  if(errorCount) return 0;
  return rawtime;
}



int lastConfig(int progMask)
{
  time_t rawtime;
  //    time(&rawtime);
  int retVal;
  int errorCount=0;
  ProgramId_t prog;
  char configFile[FILENAME_MAX];

  int testMask;

  printf("lastConfig %x\n",progMask);
  for(prog=ID_FIRST;prog<ID_NOT_AN_ID;prog++) {
    testMask=getIdMask(prog);
    //	printf("%d %d\n",progMask,testMask);
    if(progMask&testMask) {
      printf("Switching to last config %s.config\n",getProgName(prog));
      sprintf(configFile,"%s.config",getProgName(prog));
      retVal=configSwitchToLast(configFile,&rawtime);
      if(retVal!=CONFIG_E_OK)
	errorCount++;
      else {
	retVal=sendSignal(prog,SIGUSR1);
	if(retVal)
	  errorCount++;
      }

    }
  }
  if(errorCount) return 0;
  return 0;
}






int killPrograms(int progMask)
{
  time_t rawtime;
  time(&rawtime);
  char daemonCommand[FILENAME_MAX];
  char panicCommand[FILENAME_MAX];
  int retVal=0;
  int errorCount=0;
  ProgramId_t prog;

  int testMask;
  syslog(LOG_INFO,"killPrograms %#x\n",progMask);
  //    printf("Kill programs %d\n",progMask);

  if(progMask&getIdMask(ID_OPENPORTD)) {
    pid_t thePid=0;
    FILE *fpPid=fopen("/tmp/openportCopyPid","r");
    if(fpPid) {
      fscanf(fpPid,"%d", &thePid) ;
      fclose(fpPid) ;
      kill(thePid,9);
    }
  }

  for(prog=ID_FIRST;prog<ID_NOT_AN_ID;prog++) {
    testMask=getIdMask(prog);
    //	printf("%d %d\n",progMask,testMask);
    if(progMask&testMask) {
      //	    printf("Killing prog %s\n",getProgName(prog));
      //

      sprintf(daemonCommand,"daemon --stop -n %s",getProgName(prog));
      syslog(LOG_INFO,"Sending command: %s",daemonCommand);
      retVal=system(daemonCommand);
      if(retVal!=0) {
	errorCount++;
	syslog(LOG_ERR,"Error killing %s",getProgName(prog));
	//Maybe do something clever
	retVal=sendSignal(prog,SIGUSR2);
	if(retVal==0) {
	  syslog(LOG_INFO,"Killed %s\n",getProgName(prog));
	}
	else {
	  sleep(1);
	  sprintf(panicCommand,"killall -9 %s",getProgName(prog));
	  retVal=system(panicCommand);
	}
      }
      else {
	syslog(LOG_INFO,"Killed %s\n",getProgName(prog));
      }
    }
  }
  if(errorCount) return 0;
  return rawtime;
}


int reallyKillPrograms(int progMask)
{
  time_t rawtime;
  time(&rawtime);
  char fileName[FILENAME_MAX];
  char systemCommand[FILENAME_MAX];
  int retVal=0;
  int errorCount=0;
  ProgramId_t prog;

  int testMask;
  syslog(LOG_INFO,"reallyKillPrograms %#x\n",progMask);
  //    printf("Kill programs %d\n",progMask);
  for(prog=ID_FIRST;prog<ID_NOT_AN_ID;prog++) {
    testMask=getIdMask(prog);
    //	printf("%d %d\n",progMask,testMask);
    if(progMask&testMask) {
      sprintf(fileName,"%s",getPidFile(prog));
      removeFile(fileName);
      retVal=sendSignal(prog,-9);
      //	    printf("Killing prog %s\n",getProgName(prog));
      if(retVal!=0) {
	sprintf(systemCommand,"killall -9 %s",getProgName(prog));
	retVal=system(systemCommand);
	syslog(LOG_INFO,"killall -9 %s\n",getProgName(prog));
      }
    }
  }
  if(errorCount) return 0;
  return rawtime;
}


int startPrograms(int progMask)
{
  time_t rawtime;
  time(&rawtime);
  char extraCommand[FILENAME_MAX];
  char daemonCommand[FILENAME_MAX];
  int needExtra=0;
  int retVal=0;
  ProgramId_t prog;
  int testMask;
  int errorCount=0;
  syslog(LOG_INFO,"startPrograms %#x\n",progMask);
  for(prog=ID_FIRST;prog<ID_NOT_AN_ID;prog++) {
    testMask=getIdMask(prog);
    if(progMask&testMask) {
      //Match
      //RJN note -- should test if program is already running, in particular if it isn't we should stop the daemon and the continue


      if(prog==ID_PRIORITIZERD) {
	needExtra=1;
	sprintf(extraCommand,"daemon -r X -n X --env=\"DISPLAY=:0\"");
	sprintf(daemonCommand,"daemon -r %s -n %s --env=\"DISPLAY=:0\"",
		getProgName(prog),getProgName(prog));
      }
      else if(prog==ID_ACQD) {
	sprintf(daemonCommand,"daemon -r %s -n %s ",
		getProgName(prog),getProgName(prog));
      }
      else if(prog==ID_ARCHIVED) {
	sprintf(daemonCommand,"daemon -r %s -n %s ",
		getProgName(prog),getProgName(prog));
      }
      else if(prog==ID_SIPD) {
	sprintf(daemonCommand,"nice daemon -r %s -n %s ",
		getProgName(prog),getProgName(prog));
      }
      else {
	sprintf(daemonCommand,"nice -n 20 daemon -r %s -n %s ",
		getProgName(prog),getProgName(prog));
      }

      if(needExtra) {
	retVal=system(extraCommand);
	sleep(1);
      }

      syslog(LOG_INFO,"Sending command: %s",daemonCommand);
      retVal=system(daemonCommand);
      if(retVal!=0) {
	errorCount++;
	syslog(LOG_ERR,"Error starting %s",getProgName(prog));
	//Maybe do something clever
      }
    }
  }
  if(errorCount) return 0;
  return rawtime;
}

int respawnPrograms(int progMask)
{
  time_t rawtime;
  time(&rawtime);
  FILE *fpPid ;
  char fileName[FILENAME_MAX];
  pid_t thePid;
  ProgramId_t prog;
  int retVal=0;
  int testMask;
  int errorCount=0;
  for(prog=ID_FIRST;prog<ID_NOT_AN_ID;prog++) {
    testMask=getIdMask(prog);
    if(progMask&testMask) {
      sprintf(fileName,"%s",getPidFile(prog));
      if (!(fpPid=fopen(fileName, "r"))) {
	errorCount++;
	fprintf(stderr," failed to open a file for PID, %s\n", fileName);
	syslog(LOG_ERR," failed to open a file for PID, %s\n", fileName);
	startPrograms(testMask);
      }
      else {
	fscanf(fpPid,"%d", &thePid) ;
	fclose(fpPid) ;
	retVal=kill(thePid,SIGUSR2);
	if(retVal!=0) {
	  syslog(LOG_ERR,"Error sending SIGUSR2 to %s (pid=%d) -- %s",getProgName(prog),thePid,strerror(errno));
	}
      }
    }
  }
  if(errorCount) return 0;
  return rawtime;

}

int setEventDiskBitMask(int modOrSet, int bitMask)
{
  time_t rawtime;
  readConfig(); //Get Latest bit mask
  if(modOrSet==0) {
    //Then we are in disabling mode
    eventDiskBitMask&=(~bitMask);
  }
  else if(modOrSet==1) {
    //Then we are in enabling mode
    eventDiskBitMask|=bitMask;
  }
  else if(modOrSet==2) {
    //Then we are in setting mode
    eventDiskBitMask=bitMask;
  }
  else return 0;
  configModifyUnsignedInt("anitaSoft.config","global","eventDiskBitMask",eventDiskBitMask,&rawtime);
  respawnPrograms(ARCHIVED_ID_MASK);
  return rawtime;
}

int disableDisk(int diskMask, int disFlag)
{
  printf("disableDisk %#x %d\n",diskMask,disFlag);
  int diskInd=0;
  time_t rawtime=0;
  if(disFlag<0 || disFlag>1)
    return 0;

  if(disFlag==1) {
    setHkDiskBitMask(0,diskMask);
    rawtime=setEventDiskBitMask(0,diskMask);
  }


  for(diskInd=0;diskInd<DISK_TYPES;diskInd++) {
    if(diskMask & diskBitMasks[diskInd]) {
      //Have a disk
      switch(diskInd) {
      case 0:
	//Disable helium1
	configModifyInt("anitaSoft.config","global","disableHelium1",disFlag,&rawtime);
	break;
      case 1:
	//Disable helium2
	configModifyInt("anitaSoft.config","global","disableHelium2",disFlag,&rawtime);
	break;
      case 2:
	//Disable USB
	configModifyInt("anitaSoft.config","global","disableUsb",disFlag,&rawtime);
	break;
      case 4:
	//Disable Ntu
	configModifyInt("anitaSoft.config","global","disableNtu",disFlag,&rawtime);
	break;
      default:
	break;
      }
    }
  }

  killDataPrograms();
  sleep(2);
  makeNewRunDirs();
  sleep(2);
  startDataPrograms();
  return rawtime;

}


int setHkDiskBitMask(int modOrSet, int bitMask)
{
  time_t rawtime;
  readConfig(); //Get Latest bit mask
  if(modOrSet==0) {
    //Then we are in disabling mode
    hkDiskBitMask&=(~bitMask);
  }
  else if(modOrSet==1) {
    //Then we are in enabling mode
    hkDiskBitMask|=bitMask;
  }
  else if(modOrSet==2) {
    //Then we are in setting mode
    hkDiskBitMask=bitMask;
  }
  else return 0;
  configModifyUnsignedInt("anitaSoft.config","global","hkDiskBitMask",hkDiskBitMask,&rawtime);
  closeHkFilesAndTidy(&cmdWriter);
  prepWriterStructs();
  respawnPrograms(HKD_ID_MASK);
  respawnPrograms(GPSD_ID_MASK);
  respawnPrograms(ACQD_ID_MASK);
  respawnPrograms(CALIBD_ID_MASK);
  respawnPrograms(MONITORD_ID_MASK);
  return rawtime;
}

int setPriDiskMask(int pri, int bitmask)
{
  time_t rawtime;
  readArchivedConfig();
  priDiskEventMask[pri]=bitmask;
  configModifyIntArray("Archived.config","archived","priDiskEventMask",priDiskEventMask,NUM_PRIORITIES,&rawtime);
  respawnPrograms(ARCHIVED_ID_MASK);
  return rawtime;
}

int setPriEncodingType(int pri, int encType)
{
  time_t rawtime;
  readArchivedConfig();
  priDiskEncodingType[pri]=encType;
  configModifyIntArray("Archived.config","archived","priDiskEncodingType",priDiskEncodingType,NUM_PRIORITIES,&rawtime);
  sendSignal(ID_ARCHIVED,SIGUSR1);
  return rawtime;
}

/* int setAlternateUsb(int pri, int altUsb) */
/* { */
/*     time_t rawtime; */
/*     readArchivedConfig(); */
/*     alternateUsbs[pri]=altUsb; */
/*     configModifyIntArray("Archived.config","archived","alternateUsbs",alternateUsbs,NUM_PRIORITIES,&rawtime); */
/*     sendSignal(ID_ARCHIVED,SIGUSR1); */
/*     return rawtime; */
/* } */


int setArchiveGlobalDecimate(int pri, float frac)
{
  time_t rawtime;
  readArchivedConfig();
  priorityFractionGlobalDecimate[pri]=frac;
  configModifyFloatArray("Archived.config","archived","priorityFractionGlobalDecimate",priorityFractionGlobalDecimate,NUM_PRIORITIES,&rawtime);
  sendSignal(ID_ARCHIVED,SIGUSR1);
  return rawtime;
}

int setArchiveDecimatePri(int diskMask, int pri, float frac)
{
  time_t rawtime;
  readArchivedConfig();
  int diskInd;
  for(diskInd=0;diskInd<DISK_TYPES;diskInd++) {
    if(diskBitMasks[diskInd]&diskMask) {
      switch(diskInd) {
      case 0:
	priorityFractionDeleteHelium1[pri]=frac;
	configModifyFloatArray("Archived.config","archived","priorityFractionDeleteHelium1",priorityFractionDeleteHelium1,NUM_PRIORITIES,&rawtime);
	break;
      case 1:
	priorityFractionDeleteHelium2[pri]=frac;
	configModifyFloatArray("Archived.config","archived","priorityFractionDeleteHelium2",priorityFractionDeleteHelium2,NUM_PRIORITIES,&rawtime);
	break;
      case 2:
	priorityFractionDeleteUsb[pri]=frac;
	configModifyFloatArray("Archived.config","archived","priorityFractionDeleteUsb",priorityFractionDeleteUsb,NUM_PRIORITIES,&rawtime);
	break;
      case 3:
	priorityFractionDeleteNtu[pri]=frac;
	configModifyFloatArray("Archived.config","archived","priorityFractionDeleteNtu",priorityFractionDeleteNtu,NUM_PRIORITIES,&rawtime);
	break;
      case 4:
	priorityFractionDeletePmc[pri]=frac;
	configModifyFloatArray("Archived.config","archived","priorityFractionDeletePmc",priorityFractionDeletePmc,NUM_PRIORITIES,&rawtime);
	break;
      default:
	break;
      }
    }
  }
  sendSignal(ID_ARCHIVED,SIGUSR1);
  return rawtime;
  return 0;
}


int setLosdPriorityBandwidth(int pri, int bw)
{
  time_t rawtime;
  readLosdConfig();
  losdBandwidths[pri]=bw;
  configModifyIntArray("LOSd.config","bandwidth","priorityBandwidths",losdBandwidths,NUM_PRIORITIES,&rawtime);
  sendSignal(ID_LOSD,SIGUSR1);
  return rawtime;
}

int setSipdPriorityBandwidth(int pri, int bw)
{
  time_t rawtime;
  readSipdConfig();
  sipdBandwidths[pri]=bw;
  configModifyIntArray("SIPd.config","bandwidth","priorityBandwidths",sipdBandwidths,NUM_PRIORITIES,&rawtime);
  sendSignal(ID_SIPD,SIGUSR1);
  return rawtime;

}

int setSipdHkTelemOrder(int hk, int order)
{
  time_t rawtime;
  readSipdConfig();
  sipdHkTelemOrder[hk]=order;
  configModifyIntArray("SIPd.config","bandwidth","hkTelemOrder",sipdHkTelemOrder,22,&rawtime);
  sendSignal(ID_SIPD,SIGUSR1);
  return rawtime;

}

int setSipdHkTelemMaxPackets(int hk, int numPackets)
{
  time_t rawtime;
  readSipdConfig();
  sipdHkTelemMaxCopy[hk]=numPackets;
  configModifyIntArray("SIPd.config","bandwidth","hkTelemMaxCopy",sipdHkTelemMaxCopy,22,&rawtime);
  sendSignal(ID_SIPD,SIGUSR1);
  return rawtime;

}


int executeGpsdExtracommand(int command, unsigned char arg[2])
{
  printf("executeGpsdExtracommand %d %d %d\n",command,arg[0],arg[1]);
  time_t rawtime;
  int ivalue=arg[0];
  int ivalue2=arg[1];
  float fvalue=0;
  switch(command) {
  case GPS_SET_GGA_PERIOD:
    printf("GPS_SET_GGA_PERIOD %d %d\n",ivalue,ivalue2);
    if(ivalue==1)
      configModifyInt("GPSd.config","adu5a","ggaPeriod",ivalue2,&rawtime);
    else if(ivalue==2)
      configModifyInt("GPSd.config","adu5b","ggaPeriod",ivalue2,&rawtime);
    else if(ivalue==3)
      configModifyInt("GPSd.config","g12","ggaPeriod",ivalue2,&rawtime);
    else {
      syslog(LOG_ERR,"Invalid GPS_SET_GGA_PERIOD argument %d",ivalue);
      return 0;
    }
    sendSignal(ID_GPSD,SIGUSR1);
    return rawtime;
  case GPS_SET_PAT_TELEM_EVERY:
    if(ivalue==1)
      configModifyInt("GPSd.config","adu5a","patTelemEvery",ivalue2,&rawtime);
    else if(ivalue==2)
      configModifyInt("GPSd.config","adu5b","patTelemEvery",ivalue2,&rawtime);
    else {
      syslog(LOG_ERR,"Invalid GPS_SET_PAT_TELEM_EVERY argument %d",ivalue);
      return 0;
    }
    sendSignal(ID_GPSD,SIGUSR1);
    return rawtime;
  case GPS_SET_VTG_TELEM_EVERY:
    if(ivalue==1)
      configModifyInt("GPSd.config","adu5a","vtgTelemEvery",ivalue2,&rawtime);
    else if(ivalue==2)
      configModifyInt("GPSd.config","adu5b","vtgTelemEvery",ivalue2,&rawtime);
    else {
      syslog(LOG_ERR,"Invalid GPS_SET_VTG_TELEM_EVERY argument %d",ivalue);
      return 0;
    }
    sendSignal(ID_GPSD,SIGUSR1);
    return rawtime;
  case GPS_SET_SAT_TELEM_EVERY:
    if(ivalue==1)
      configModifyInt("GPSd.config","adu5a","satTelemEvery",ivalue2,&rawtime);
    else if(ivalue==2)
      configModifyInt("GPSd.config","adu5b","satTelemEvery",ivalue2,&rawtime);
    else if(ivalue==3)
      configModifyInt("GPSd.config","g12","satTelemEvery",ivalue2,&rawtime);
    else {
      syslog(LOG_ERR,"Invalid GPS_SET_SAT_TELEM_EVERY argument %d",ivalue);
      return 0;
    }
    sendSignal(ID_GPSD,SIGUSR1);
    return rawtime;
  case GPS_SET_GGA_TELEM_EVERY:
    if(ivalue==1)
      configModifyInt("GPSd.config","adu5a","ggaTelemEvery",ivalue2,&rawtime);
    else if(ivalue==2)
      configModifyInt("GPSd.config","adu5b","ggaTelemEvery",ivalue2,&rawtime);
    else if(ivalue==3)
      configModifyInt("GPSd.config","g12","ggaTelemEvery",ivalue2,&rawtime);
    else {
      syslog(LOG_ERR,"Invalid GPS_SET_GGA_TELEM_EVERY argument %d",ivalue);
      return 0;
    }
    sendSignal(ID_GPSD,SIGUSR1);
    return rawtime;
  case GPS_SET_POS_TELEM_EVERY:
    configModifyInt("GPSd.config","g12","posTelemEvery",ivalue2,&rawtime);
    sendSignal(ID_GPSD,SIGUSR1);
    return rawtime;
  case GPS_SET_INI_RESET_FLAG:
    if(ivalue==1)
      configModifyInt("GPSd.config","adu5a","iniReset",ivalue2,&rawtime);
    else if(ivalue==2)
      configModifyInt("GPSd.config","adu5b","iniReset",ivalue2,&rawtime);
    else if(ivalue==3)
      configModifyInt("GPSd.config","g12","iniReset",ivalue2,&rawtime);
    else {
      syslog(LOG_ERR,"Invalid GPS_SET_INI_RESET_FLAG argument %d",ivalue);
      return 0;
    }
    sendSignal(ID_GPSD,SIGUSR1);
    return rawtime;
  case GPS_SET_ELEVATION_MASK:
    if(ivalue==1)
      configModifyInt("GPSd.config","adu5a","elevationMask",ivalue2,&rawtime);
    else if(ivalue==2)
      configModifyInt("GPSd.config","adu5b","elevationMask",ivalue2,&rawtime);
    else if(ivalue==3)
      configModifyInt("GPSd.config","g12","elevationMask",ivalue2,&rawtime);
    else {
      syslog(LOG_ERR,"Invalid GPS_SET_ELEVATION_MASK argument %d",ivalue);
      return 0;
    }
    sendSignal(ID_GPSD,SIGUSR1);
    return rawtime;
  case GPS_SET_CYC_PHASE_ERROR:
    fvalue=((float)(2*ivalue2))/1000.;
    if(ivalue==1)
      configModifyFloat("GPSd.config","adu5a","cycPhaseErrorThreshold",fvalue,&rawtime);
    else if(ivalue==2)
      configModifyFloat("GPSd.config","adu5b","cycPhaseErrorThreshold",fvalue,&rawtime);
    else {
      syslog(LOG_ERR,"Invalid GPS_SET_CYC_PHASE_ERROR argument %d",ivalue);
      return 0;
    }
    sendSignal(ID_GPSD,SIGUSR1);
    return rawtime;
  case GPS_SET_MXB_BASELINE_ERROR:
    fvalue=((float)(ivalue2))/1000.;
    if(ivalue==1)
      configModifyFloat("GPSd.config","adu5a","mxbBaselineError",fvalue,&rawtime);
    else if(ivalue==2)
      configModifyFloat("GPSd.config","adu5b","mxbBaselineError",fvalue,&rawtime);
    else {
      syslog(LOG_ERR,"Invalid GPS_SET_MXB_BASELINE_ERROR argument %d",ivalue);
      return 0;
    }
    sendSignal(ID_GPSD,SIGUSR1);
    return rawtime;
  case GPS_SET_MXM_PHASE_ERROR:
    fvalue=((float)(ivalue2))/1000.;
    if(ivalue==1)
      configModifyFloat("GPSd.config","adu5a","mxmPhaseError",fvalue,&rawtime);
    else if(ivalue==2)
      configModifyFloat("GPSd.config","adu5b","mxmPhaseError",fvalue,&rawtime);
    else {
      syslog(LOG_ERR,"Invalid GPS_SET_MXM_PHASE_ERROR argument %d",ivalue);
      return 0;
    }
    sendSignal(ID_GPSD,SIGUSR1);
    return rawtime;
  default:
    syslog(LOG_ERR,"Unknown GPSd Extra Command -- %d",command);
    break;
  }
  return 0;
}


int executeCalibdExtraCommand(int command, unsigned char arg[2])
{
  printf("executeCalibdExtraCommand %d %d %d\n",command,arg[0],arg[1]);
  time_t rawtime;
  int ivalue=arg[0] + (arg[1]<<8);
  //  float fvalue=0;
  switch(command) {
  case CALIBD_READOUT_PERIOD:
    printf("CALIBD_READOUT_PERIOD %d\n",ivalue);
    configModifyInt("Calibd.config","calibd","readoutPeriod",ivalue,&rawtime);
    sendSignal(ID_CALIBD,SIGUSR1);
    return rawtime;
  case CALIBD_TELEM_EVERY:
    printf("CALIBD_TELEM_EVERY %d\n",ivalue);
    configModifyInt("Calibd.config","calibd","telemEvery",ivalue,&rawtime);
    sendSignal(ID_CALIBD,SIGUSR1);
    return rawtime;
  case CALIBD_TELEM_AVERAGE:
    printf("CALIBD_TELEM_AVERAGE %d\n",ivalue);
    configModifyInt("Calibd.config","calibd","telemAverage",ivalue,&rawtime);
    sendSignal(ID_CALIBD,SIGUSR1);
    return rawtime;
  case CALIBD_ADC_AVERAGE:
    printf("CALIBD_ADC_AVERAGE %d\n",ivalue);
    configModifyInt("Calibd.config","calibd","adcAverage",ivalue,&rawtime);
    sendSignal(ID_CALIBD,SIGUSR1);
    return rawtime;
  case CALIBD_CALIBRATION_PERIOD:
    printf("CALIBD_CALIBRATION_PERIOD %d\n",ivalue);
    configModifyInt("Calibd.config","calibd","calibrationPeriod",ivalue,&rawtime);
    sendSignal(ID_CALIBD,SIGUSR1);
    return rawtime;
  default:
    syslog(LOG_ERR,"Unknown Calibd Extra Command -- %d",command);
    break;
  }
  return 0;
}



int executePlaybackCommand(int command, unsigned int uvalue1, unsigned int uvalue2)
{
  PlaybackRequest_t pReq;
  time_t rawtime;
  char filename[FILENAME_MAX];
  switch(command) {
  case PLAY_GET_EVENT:
    pReq.eventNumber=uvalue1;
    pReq.pri=uvalue2;
    //	    printf("event %u, pri %d\n",uvalue1,uvalue2);
    sprintf(filename,"%s/play_%u.dat",PLAYBACK_DIR,pReq.eventNumber);
    normalSingleWrite((unsigned char*)&pReq,filename,sizeof(PlaybackRequest_t));
    makeLink(filename,PLAYBACK_LINK_DIR);
    rawtime=time(NULL);
    break;
  case PLAY_START_PLAY:
    configModifyInt("Playbackd.config","playbackd","sendData",1,&rawtime);
    respawnPrograms(PLAYBACKD_ID_MASK);
    break;
  case PLAY_STOP_PLAY:
    configModifyInt("Playbackd.config","playbackd","sendData",0,&rawtime);
    respawnPrograms(PLAYBACKD_ID_MASK);
    break;
  case PLAY_START_PRI:
    if(uvalue1<NUM_PRIORITIES) {
      configModifyInt("Playbackd.config","playbackd","startPriority",(int)uvalue1,&rawtime);
      respawnPrograms(PLAYBACKD_ID_MASK);
    }
    else return -1;
    break;
  case PLAY_STOP_PRI:
    if(uvalue1<NUM_PRIORITIES) {
      configModifyInt("Playbackd.config","playbackd","stopPriority",(int)uvalue1,&rawtime);
      respawnPrograms(PLAYBACKD_ID_MASK);
    }
    else return -1;
    break;
  case PLAY_SLEEP_PERIOD:
    configModifyInt("Playbackd.config","playbackd","msSleepPeriod",(int)uvalue1,&rawtime);
    respawnPrograms(PLAYBACKD_ID_MASK);
    break;
  case PLAY_USE_DISK:
    if(uvalue1>2)
      return 0;
    configModifyInt("Playbackd.config","playbackd","useDisk",uvalue1,&rawtime);
    return rawtime;
  case PLAY_MODE:
    configModifyInt("Playbackd.config","playbackd","playBackMode",(int)uvalue1,&rawtime);
    respawnPrograms(PLAYBACKD_ID_MASK);
    break;
  case PLAY_STARTING_RUN:
    configModifyInt("Playbackd.config","playbackd","startingRun",(int)uvalue1,&rawtime);
    respawnPrograms(PLAYBACKD_ID_MASK);
    break;

  default:
    return -1;
  }

  return rawtime;
}


int executePrioritizerdCommand(int command, int value, int value2)
{
  time_t rawtime;
  /* union */
  /* { */
  /*   unsigned int u; */
  /*   float f; */
  /* } cheat; */
  readPrioritizerdConfig();
  switch(command) {
  case PRI_PANIC_QUEUE_LENGTH:
    configModifyInt("Prioritizerd.config","prioritizerd","panicQueueLength",value,&rawtime);
    break;
  case PRI_PARAMS_LOW_BIN_EDGE:
    priorityParamsLowBinEdge[value]=value2/100.;
    configModifyFloatArray("Prioritizerd.config","prioritizerd","priorityParamsLowBinEdge",priorityParamsLowBinEdge,10,&rawtime);
    break;
  case PRI_PARAMS_HIGH_BIN_EDGE:
    priorityParamsHighBinEdge[value]=value2/100.;
    configModifyFloatArray("Prioritizerd.config","prioritizerd","priorityParamsHighBinEdge",priorityParamsHighBinEdge,10,&rawtime);
    break;
  case PRI_SLOPE_IMAGE_HILBERT:
    configModifyFloat("Prioritizerd.config","prioritizerd","slopeOfImagePeakVsHilbertPeak",(float)value2,&rawtime);
    break;
  case PRI_INTERCEPT_IMAGE_HILBERT:
    configModifyFloat("Prioritizerd.config","prioritizerd","interceptOfImagePeakVsHilbertPeak",(float)value2,&rawtime);
    break;
  case PRI_BIN_TO_BIN_THRESH:
    configModifyFloat("Prioritizerd.config","prioritizerd","binToBinDifferenceThresh_dB",value2/10.,&rawtime);
    break;
  case PRI_ABS_MAG_THRESH:
    configModifyFloat("Prioritizerd.config","prioritizerd","absMagnitudeThresh_dBm",value2/10.,&rawtime);
    break;
  case PRI_THETA_ANGLE_DEMOTION_LOW:
    configModifyFloat("Prioritizerd.config","prioritizerd","thetaAngleLowForDemotion",value2-90,&rawtime);
    break;
  case PRI_THETA_ANGLE_DEMOTION_HIGH:
    configModifyFloat("Prioritizerd.config","prioritizerd","thetaAngleHighForDemotion",value2,&rawtime);
    break;
  case PRI_THETA_ANGLE_DEMOTION_PRIORITY:
    configModifyInt("Prioritizerd.config","prioritizerd","thetaAnglePriorityDemotion",value,&rawtime);
    break;
  case PRI_POWER_SPECTRUM_PERIOD:
    configModifyInt("Prioritizerd.config","prioritizerd","writePowSpecPeriodSeconds",value2,&rawtime);
    break;
  case PRI_ANT_PHI_POS:
    phiArrayDeg[value]=value2/100.;
    configModifyFloatArray("Prioritizerd.config","antennalocations","phiArrayDeg",phiArrayDeg,48,&rawtime);
    break;
  case PRI_ANT_R_POS:
    rArray[value]=value2/1000.;
    configModifyFloatArray("Prioritizerd.config","antennalocations","rArray",rArray,48,&rawtime);
    break;
  case PRI_ANT_Z_POS:
    zArray[value]=-1.0*value2/10000.0;
    configModifyFloatArray("Prioritizerd.config","antennalocations","zArray",zArray,48,&rawtime);
    break;
  case PRI_POS_SATUATION:
    configModifyInt("Prioritizerd.config","prioritizerd","positiveSaturation",value2,&rawtime);
    break;
  case PRI_NEG_SATUATION:
    configModifyInt("Prioritizerd.config","prioritizerd","negativeSaturation",-1*value2,&rawtime);
    break;
  case PRI_ASYM_SATURATION:
    configModifyInt("Prioritizerd.config","prioritizerd","asymSaturation",value,&rawtime);
    break;
  case PRI_SLEEP_TIME_KILL_X:
    configModifyInt("Prioritizerd.config","prioritizerd","sleepTimeAfterKillingX",value,&rawtime);
    break;
  case PRI_DISABLE_GPU:
    configModifyInt("Prioritizerd.config","prioritizerd","disableGpu",value,&rawtime);
    break;
  case PRI_CALIB_VERSION:
    configModifyInt("Prioritizerd.config","prioritizerd","anitaCalibVersion",value,&rawtime);
    break;
  case PRI_INVERT_TOP_RING_SOFTWARE:
    configModifyInt("Prioritizerd.config","prioritizerd","invertTopRingInSoftware",value,&rawtime);
    break;
  case PRI_DEBUG_MODE:
    configModifyInt("Prioritizerd.config","prioritizerd","debugMode",value,&rawtime);
    break;
  case PRI_SKIP_BLAST_RATIO_HPOL:
    printf("got value %d, value2 %d\n", value, value2);
    skipBlastRatioHPol[value]=value2;
    configModifyIntArray("Prioritizerd.config","prioritizerd","skipBlastRatioHPol",skipBlastRatioHPol,16,&rawtime);
    break;
  case PRI_SKIP_BLAST_RATIO_VPOL:
    printf("got value %d, value2 %d\n", value, value2);
    skipBlastRatioVPol[value]=value2;
    configModifyIntArray("Prioritizerd.config","prioritizerd","skipBlastRatioVPol",skipBlastRatioVPol,16,&rawtime);
    break;
  case PRI_BLAST_RATIO_MAX:
    configModifyFloat("Prioritizerd.config","prioritizerd","blastRatioMax",((float)value)/100.,&rawtime);
    break;
  case PRI_BLAST_RATIO_MIN:
    configModifyFloat("Prioritizerd.config","prioritizerd","blastRatioMin",((float)value)/100.,&rawtime);
    break;
  case PRI_BLAST_GRADIENT:
    configModifyFloat("Prioritizerd.config","prioritizerd","blastGradient",(float)value,&rawtime);
    break;
  case PRI_STATIC_NOTCH_LOW_EDGE:
    staticNotchesLowEdgeMHz[value]=value2;
    configModifyFloatArray("Prioritizerd.config","prioritizerd","staticNotchesLowEdgeMHz",staticNotchesLowEdgeMHz,10,&rawtime);
    break;
  case PRI_STATIC_NOTCH_HIGH_EDGE:
    staticNotchesHighEdgeMHz[value]=value2;
    configModifyFloatArray("Prioritizerd.config","prioritizerd","staticNotchesHighEdgeMHz",staticNotchesHighEdgeMHz,10,&rawtime);
    break;
  case PRI_USE_LONG_DYNAMIC_FILTERING:
    configModifyInt("Prioritizerd.config","prioritizerd","useLongDynamicFiltering",value,&rawtime);
    break;
  case PRI_START_DYNAMIC_FREQUENCY:
    configModifyFloat("Prioritizerd.config","prioritizerd","startDynamicFrequency",(1.0*value),&rawtime);
    break;
  case PRI_STOP_DYNAMIC_FREQUENCY:
    configModifyFloat("Prioritizerd.config","prioritizerd","stopDynamicFrequency",(1.0*value),&rawtime);
    break;
  case PRI_CONSERVATIVE_START:
    configModifyInt("Prioritizerd.config","prioritizerd","conservativeStart",value,&rawtime);
    break;
  case PRI_THRESH_DB:
    configModifyFloat("Prioritizerd.config","prioritizerd","spikeThresh_dB",((float)value)/100,&rawtime);
    break;
  case PRI_DELTA_PHI_TRIG:
    configModifyInt("Prioritizerd.config","prioritizerd","deltaL3PhiTrig",value,&rawtime);
    break;

  default:
    return -1;
  }
  return rawtime;
}

int executeGpsPhiMaskCommand(int command, unsigned char args[5])
{
  int retVal=0;
  unsigned int uvalue[4]={0};
  unsigned int utemp=0;
  int ivalue[4]={0};
  float fvalue=0;
  time_t rawtime;
  switch(command) {
  case GPS_PHI_MASK_ENABLE:
    ivalue[0]=(int)args[0];
    if(ivalue[0]==1) {
      //Enable gpsPhiMasking
      configModifyInt("GPSd.config","sourceList","enableGpsPhiMasking",1,&rawtime);
      retVal=sendSignal(ID_GPSD,SIGUSR1);
      if(retVal) return 0;
    }
    else {
      //Disable gpsPhiMasking
      configModifyInt("GPSd.config","sourceList","enableGpsPhiMasking",0,&rawtime);
      configModifyInt("Acqd.config","thresholds","gpsPhiTrigMask",0,&rawtime);
      retVal=sendSignal(ID_GPSD,SIGUSR1);
      retVal=sendSignal(ID_ACQD,SIGUSR1);
      if(retVal) return 0;
    }
    return rawtime;
  case GPS_PHI_MASK_UPDATE_PERIOD:
    uvalue[0]=args[0]+(args[1]<<8);
    configModifyInt("GPSd.config","sourceList","phiMaskingPeriod",uvalue[0],&rawtime);
    retVal=sendSignal(ID_GPSD,SIGUSR1);
    if(retVal) return 0;
    return rawtime;
  case GPS_PHI_MASK_SET_SOURCE_LATITUDE:
    readGpsdConfig();
    ivalue[0]=args[0]; //Source number
    if(ivalue[0]>=MAX_PHI_SOURCES) return 0;
    if(ivalue[0]<=0) return 0;
    utemp=(args[0]);
    uvalue[0]=utemp;
    utemp=(args[1]);
    uvalue[0]|=(utemp<<8);
    utemp=(args[2]);
    uvalue[0]|=(utemp<<16);
    utemp=(args[3]);
    uvalue[0]|=(utemp<<24);

    fvalue=((float)uvalue[0])/1e7;
    fvalue-=180; //The latitude
    if(ivalue[0]>numSources) {
      numSources=ivalue[0];
      configModifyInt("GPSd.config","sourceList","numSources",numSources,&rawtime);
    }
    sourceLats[ivalue[0]-1]=fvalue;
    configModifyFloatArray("GPSd.config","sourceList","sourceLats",sourceLats,MAX_PHI_SOURCES,&rawtime);
    retVal=sendSignal(ID_GPSD,SIGUSR1);
    if(retVal) return 0;
    return rawtime;
  case GPS_PHI_MASK_SET_SOURCE_LONGITUDE:
    readGpsdConfig();
    ivalue[0]=args[0]; //Source number
    if(ivalue[0]>=MAX_PHI_SOURCES) return 0;
    if(ivalue[0]<=0) return 0;
    utemp=(args[0]);
    uvalue[0]=utemp;
    utemp=(args[1]);
    uvalue[0]|=(utemp<<8);
    utemp=(args[2]);
    uvalue[0]|=(utemp<<16);
    utemp=(args[3]);
    uvalue[0]|=(utemp<<24);

    fvalue=((float)uvalue[0])/1e7;
    fvalue-=180; //The longitude
    if(ivalue[0]>numSources) {
      numSources=ivalue[0];
      configModifyInt("GPSd.config","sourceList","numSources",numSources,&rawtime);
    }
    sourceLongs[ivalue[0]-1]=fvalue;
    configModifyFloatArray("GPSd.config","sourceList","sourceLongs",sourceLongs,MAX_PHI_SOURCES,&rawtime);
    retVal=sendSignal(ID_GPSD,SIGUSR1);
    if(retVal) return 0;
    return rawtime;
  case GPS_PHI_MASK_SET_SOURCE_ALTITUDE:
    readGpsdConfig();
    ivalue[0]=args[0]; //Source number
    if(ivalue[0]>=MAX_PHI_SOURCES) return 0;
    if(ivalue[0]<=0) return 0;
    utemp=(args[0]);
    uvalue[0]=utemp;
    utemp=(args[1]);
    uvalue[0]|=(utemp<<8);
    utemp=(args[2]);
    uvalue[0]|=(utemp<<16);
    utemp=(args[3]);
    uvalue[0]|=(utemp<<24);

    fvalue=((float)uvalue[0])/1e3; //Altitude
    if(ivalue[0]>numSources) {
      numSources=ivalue[0];
      configModifyInt("GPSd.config","sourceList","numSources",numSources,&rawtime);
    }
    sourceAlts[ivalue[0]-1]=fvalue;
    configModifyFloatArray("GPSd.config","sourceList","sourceAlts",sourceAlts,MAX_PHI_SOURCES,&rawtime);
    retVal=sendSignal(ID_GPSD,SIGUSR1);
    if(retVal) return 0;
    return rawtime;
  case GPS_PHI_MASK_SET_SOURCE_HORIZON:
    readGpsdConfig();
    ivalue[0]=args[0]; //Source number
    if(ivalue[0]>=MAX_PHI_SOURCES) return 0;
    if(ivalue[0]<=0) return 0;
    utemp=(args[0]);
    uvalue[0]=utemp;
    utemp=(args[1]);
    uvalue[0]|=(utemp<<8);
    utemp=(args[2]);
    uvalue[0]|=(utemp<<16);
    utemp=(args[3]);
    uvalue[0]|=(utemp<<24);

    fvalue=((float)uvalue[0])/1e3; //Horizon
    //   fvalue-=180; //The latitude
    if(ivalue[0]>numSources) {
      numSources=ivalue[0];
      configModifyInt("GPSd.config","sourceList","numSources",numSources,&rawtime);
    }
    sourceDistKm[ivalue[0]-1]=fvalue;
    configModifyFloatArray("GPSd.config","sourceList","sourceDistKm",sourceDistKm,MAX_PHI_SOURCES,&rawtime);
    retVal=sendSignal(ID_GPSD,SIGUSR1);
    if(retVal) return 0;
    return rawtime;
  case GPS_PHI_MASK_SET_SOURCE_WIDTH:
    readGpsdConfig();
    ivalue[0]=args[0]; //Source number
    if(ivalue[0]>=MAX_PHI_SOURCES) return 0;
    if(ivalue[0]<=0) return 0;
    utemp=(args[0]); //Phi Width
    if(utemp>8) return 0;

    if(ivalue[0]>numSources) {
      numSources=ivalue[0];
      configModifyInt("GPSd.config","sourceList","numSources",numSources,&rawtime);
    }
    sourcePhiWidth[ivalue[0]-1]=utemp;
    configModifyIntArray("GPSd.config","sourceList","sourceDistKm",sourcePhiWidth,MAX_PHI_SOURCES,&rawtime);
    retVal=sendSignal(ID_GPSD,SIGUSR1);
    if(retVal) return 0;
    return rawtime;
  default:
    fprintf(stderr,"Unknown GPS Phi Mask Command -- %d\n",command);
    syslog(LOG_ERR,"Unknown GPS Phi Mask Command -- %d\n",command);
    return 0;
  }
  return 0;
}



int executeAcqdExtraCommand(int command, unsigned char arg)
{
  int retVal=0;
  //  unsigned int utemp=0;
  int ivalue=0;
  time_t rawtime;
  switch(command) {
  case ACQD_SET_PHOTO_SHUTTER_MASK:
    ivalue=arg&0x7;
    configModifyInt("Acqd.config","turfio","photoShutterMask",ivalue,&rawtime);
    retVal=sendSignal(ID_ACQD,SIGUSR1);
    if(retVal) return 0;
    return rawtime;
  case ACQD_SET_PPS_SOURCE:
    ivalue=arg&0x3;
    configModifyInt("Acqd.config","turfio","ppsSource",ivalue,&rawtime);
    retVal=sendSignal(ID_ACQD,SIGUSR1);
    if(retVal) return 0;
    return rawtime;
  case ACQD_SET_REF_CLOCK_SOURCE:
    ivalue=arg&0x1;
    configModifyInt("Acqd.config","turfio","refClockSource",ivalue,&rawtime);
    retVal=sendSignal(ID_ACQD,SIGUSR1);
    if(retVal) return 0;
    return rawtime;
  case ACQD_SET_TURF_RATE_AVERAGE:
    ivalue=arg;
    configModifyInt("Acqd.config","acqd","turfRateAverage",ivalue,&rawtime);
    retVal=sendSignal(ID_ACQD,SIGUSR1);
    if(retVal) return 0;
    return rawtime;
  default:
    syslog(LOG_ERR,"Unknown Acqd extra command -- %d\n",command);
    fprintf(stderr,"Unknown Acqd extra command -- %d\n",command);
    return -1;
  }
  return rawtime;
}


int executeAcqdRateCommand(int command, unsigned char args[8])
{
  int retVal=0;
  unsigned int uvalue[4]={0};
  unsigned int utemp=0;
  int ivalue[4]={0};
  float fvalue=0;
  time_t rawtime;
  switch(command) {
  case ACQD_RATE_ENABLE_CHAN_SERVO:
    ivalue[0]=args[0];
    configModifyInt("Acqd.config","thresholds","enableChanServo",ivalue[0],&rawtime);
    retVal=sendSignal(ID_ACQD,SIGUSR1);
    if(retVal) return 0;
    return rawtime;
  case ACQD_RATE_SET_PID_GOALS:
    uvalue[0]=args[0]+(args[1]<<8); //top
    uvalue[1]=args[2]+(args[3]<<8); //middle
    uvalue[2]=args[4]+(args[5]<<8); //bottom
    //    uvalue[3]=args[6]+(args[7]<<8);
    pidGoals[0]=uvalue[0];
    pidGoals[1]=uvalue[1];
    pidGoals[2]=uvalue[2];
    //    pidGoals[3]=uvalue[3];
    configModifyIntArray("Acqd.config","thresholds","pidGoals",pidGoals,3,&rawtime);
    retVal=sendSignal(ID_ACQD,SIGUSR1);
    if(retVal) return 0;
    return rawtime;
  case ACQD_SET_NADIR_PID_GOALS:
    uvalue[0]=args[0]+(args[1]<<8);
    uvalue[1]=args[2]+(args[3]<<8);
    uvalue[2]=args[4]+(args[5]<<8);
    uvalue[3]=args[6]+(args[7]<<8);
    pidGoals[0]=uvalue[0];
    pidGoals[1]=uvalue[1];
    pidGoals[2]=uvalue[2];
    //    pidGoals[3]=uvalue[3];
    //    configModifyIntArray("Acqd.config","thresholds","nadirPidGoals",pidGoals,BANDS_PER_ANT,&rawtime);
    retVal=sendSignal(ID_ACQD,SIGUSR1);
    if(retVal) return 0;
    return rawtime;
  case ACQD_RATE_SET_L2_TRIG_MASK:
    utemp=(args[0]);
    uvalue[0]=utemp;
    utemp=(args[1]);
    uvalue[0]|=(utemp<<8);
    //    utemp=(args[2]);
    //    uvalue[0]|=(utemp<<16);
    //    utemp=(args[3]);
    //    uvalue[0]|=(utemp<<24);
    syslog(LOG_INFO,"ACQD_SET_L2_TRIG_MASK: %d %d %d %d -- %u -- %#x\n",args[0],args[1],
	   args[2],args[3],uvalue[0],(unsigned int)uvalue[0]);

    configModifyUnsignedInt("Acqd.config","thresholds","l2TrigMask",uvalue[0],&rawtime);
    retVal=sendSignal(ID_ACQD,SIGUSR1);
    if(retVal) return 0;
    return rawtime;
  /* case ACQD_RATE_SET_L1_TRIG_MASK_HPOL: */
  /*   utemp=(args[0]); */
  /*   uvalue[0]=utemp; */
  /*   utemp=(args[1]); */
  /*   uvalue[0]|=(utemp<<8); */
  /*   utemp=(args[2]); */
  /*   uvalue[0]|=(utemp<<16); */
  /*   utemp=(args[3]); */
  /*   uvalue[0]|=(utemp<<24); */
  /*   syslog(LOG_INFO,"ACQD_SET_L1_TRIG_MASK_HPOL: %d %d %d %d -- %u -- %#x\n",args[0],args[1], */
  /* 	   args[2],args[3],uvalue[0],(unsigned int)uvalue[0]); */

  /*   configModifyUnsignedInt("Acqd.config","thresholds","l1TrigMaskH",uvalue[0],&rawtime); */
  /*   retVal=sendSignal(ID_ACQD,SIGUSR1); */
  /*   if(retVal) return 0; */
  /*   return rawtime; */
  case ACQD_RATE_SET_PHI_MASK:
    utemp=(args[0]);
    uvalue[0]=utemp;
    utemp=(args[1]);
    uvalue[0]|=(utemp<<8);
    syslog(LOG_INFO,"ACQD_RATE_SET_PHI_MASK: %d %d -- %u -- %#x\n",args[0],args[1],uvalue[0],(unsigned int)uvalue[0]);

    configModifyUnsignedInt("Acqd.config","thresholds","phiTrigMask",uvalue[0],&rawtime);
    retVal=sendSignal(ID_ACQD,SIGUSR1);
    if(retVal) return 0;
    return rawtime;
  case ACQD_RATE_SET_SURF_BAND_TRIG_MASK:
    ivalue[0]=(args[0]); //Which Surf
    ivalue[1]=(args[1])+(args[2]<<8);
    readAcqdConfig();
    if(ivalue[0]>-1 && ivalue[0]<ACTIVE_SURFS) {
      surfTrigBandMasks[ivalue[0]]=ivalue[1];
      configModifyIntArray("Acqd.config","thresholds","surfTrigBandMasks",&surfTrigBandMasks[0],ACTIVE_SURFS,&rawtime);
    }
    retVal=sendSignal(ID_ACQD,SIGUSR1);
    if(retVal) return 0;
    return rawtime;
  case ACQD_RATE_SET_CHAN_PID_GOAL_SCALE:
    ivalue[0]=args[0]; //surf 0:11
    ivalue[1]=args[1]; //dac 0:11
    if(ivalue[0]>11 || ivalue[1]>11) return 0;
    ivalue[2]=args[2]+(args[3]<<8);
    fvalue=((float)ivalue[2])/1000.;
    return setPidGoalScaleFactor(ivalue[0],ivalue[1],fvalue);
  case ACQD_RATE_SET_RATE_SERVO:
    ivalue[0]=args[0]+(args[1]<<8);
    if(ivalue[0]>0) {
      configModifyInt("Acqd.config","rates","enableRateServo",1,NULL);
      fvalue=((float)ivalue[0])/1000.;
      configModifyFloat("Acqd.config","rates","rateGoal",fvalue,&rawtime);
    }
    else {
      configModifyInt("Acqd.config","rates","enableRateServo",0,&rawtime);
    }
    retVal=sendSignal(ID_ACQD,SIGUSR1);
    return rawtime;
  case ACQD_RATE_ENABLE_DYNAMIC_PHI_MASK:
    utemp=(args[0]);
    ivalue[0]=utemp;
    configModifyInt("Acqd.config","rates","enableDynamicPhiMasking",ivalue[0],&rawtime);
    retVal=sendSignal(ID_ACQD,SIGUSR1);
    if(retVal) return 0;
    return rawtime;
  case ACQD_RATE_ENABLE_DYNAMIC_L2_MASK:
    utemp=(args[0]);
    ivalue[0]=utemp;
    configModifyInt("Acqd.config","rates","enableDynamicL2Masking",ivalue[0],&rawtime);
    retVal=sendSignal(ID_ACQD,SIGUSR1);
    if(retVal) return 0;
    return rawtime;
  case ACQD_RATE_SET_DYNAMIC_PHI_MASK_OVER:
    utemp=(args[0]);
    ivalue[0]=utemp; //This will be the rate
    utemp=(args[1]);
    ivalue[1]=utemp; //This is the window size in seconds
    configModifyInt("Acqd.config","rates","dynamicPhiThresholdOverRate",ivalue[0],&rawtime);
    configModifyInt("Acqd.config","rates","dynamicPhiThresholdOverWindow",ivalue[1],&rawtime);
    retVal=sendSignal(ID_ACQD,SIGUSR1);
    if(retVal) return 0;
    return rawtime;
  case ACQD_RATE_SET_DYNAMIC_PHI_MASK_UNDER:
    utemp=(args[0]);
    ivalue[0]=utemp; //This will be the rate
    utemp=(args[1]);
    ivalue[1]=utemp; //This is the window size in seconds
    configModifyInt("Acqd.config","rates","dynamicPhiThresholdUnderRate",ivalue[0],&rawtime);
    configModifyInt("Acqd.config","rates","dynamicPhiThresholdUnderWindow",ivalue[1],&rawtime);
    retVal=sendSignal(ID_ACQD,SIGUSR1);
    if(retVal) return 0;
    return rawtime;
  case ACQD_RATE_SET_DYNAMIC_L2_MASK_OVER:
    readAcqdConfig();
    utemp=(args[0]);
    uvalue[0]=utemp;
    utemp=(args[1]);
    uvalue[0]|=(utemp<<8);
    utemp=(args[2]);
    uvalue[0]|=(utemp<<16);
    utemp=(args[3]);
    uvalue[0]|=(utemp<<24);
    utemp=(args[4]);
    ivalue[0]=utemp; //This is the phi sector
    if(ivalue[0]<0 || ivalue[0]>15) return 0;
    dynamicL2ThresholdOverRate[ivalue[0]]=uvalue[0];
    configModifyIntArray("Acqd.config","rates","dynamicL2ThresholdOverRate",&dynamicL2ThresholdOverRate[0],PHI_SECTORS,&rawtime);
    retVal=sendSignal(ID_ACQD,SIGUSR1);
    if(retVal) return 0;
    return rawtime;
  case  ACQD_RATE_SET_DYNAMIC_L2_MASK_OVER_WINDOW:
    utemp=(args[0]);
    ivalue[0]=utemp;
    configModifyInt("Acqd.config","rates","dynamicL2ThresholdOverWindow",ivalue[0],&rawtime);
    retVal=sendSignal(ID_ACQD,SIGUSR1);
    if(retVal) return 0;
    return rawtime;
  case  ACQD_RATE_SET_DYNAMIC_L2_MASK_UNDER_WINDOW:
    utemp=(args[0]);
    ivalue[0]=utemp;
    configModifyInt("Acqd.config","rates","dynamicL2ThresholdUnderWindow",ivalue[0],&rawtime);
    retVal=sendSignal(ID_ACQD,SIGUSR1);
    if(retVal) return 0;
    return rawtime;
  case ACQD_RATE_SET_DYNAMIC_L2_MASK_UNDER:
    readAcqdConfig();
    utemp=(args[0]);
    uvalue[0]=utemp;
    utemp=(args[1]);
    uvalue[0]|=(utemp<<8);
    utemp=(args[2]);
    uvalue[0]|=(utemp<<16);
    utemp=(args[3]);
    uvalue[0]|=(utemp<<24);
    utemp=(args[4]);
    ivalue[0]=utemp; //This is the phi sector
    if(ivalue[0]<0 || ivalue[0]>15) return 0;
    dynamicL2ThresholdUnderRate[ivalue[0]]=uvalue[0];
    configModifyIntArray("Acqd.config","rates","dynamicL2ThresholdUnderRate",&dynamicL2ThresholdUnderRate[0],PHI_SECTORS,&rawtime);
    retVal=sendSignal(ID_ACQD,SIGUSR1);
    if(retVal) return 0;
    return rawtime;
  case ACQD_RATE_SET_GLOBAL_THRESHOLD:
    ivalue[0]=args[0]+(args[1]<<8);
    configModifyInt("Acqd.config","thresholds","globalThreshold",ivalue[0],NULL);
    if(ivalue[0]>0)
      configModifyInt("Acqd.config","thresholds","setGlobalThreshold",1,&rawtime);
    else
      configModifyInt("Acqd.config","thresholds","setGlobalThreshold",0,&rawtime);
    retVal=sendSignal(ID_ACQD,SIGUSR1);
    if(retVal) return 0;
    return rawtime;
  case ACQD_RATE_SET_GPS_PHI_MASK:
    utemp=(args[0]);
    uvalue[0]=utemp;
    utemp=(args[1]);
    uvalue[0]|=(utemp<<8);
    syslog(LOG_INFO,"ACQD_RATE_SET_GPS_PHI_MASK: %d %d -- %u -- %#x\n",args[0],args[1],uvalue[0],(unsigned int)uvalue[0]);

    configModifyUnsignedInt("Acqd.config","thresholds","gpsPhiMask",uvalue[0],&rawtime);
    retVal=sendSignal(ID_ACQD,SIGUSR1);
    if(retVal) return 0;
    return rawtime;
  case ACQD_SET_PID_PGAIN:
    readAcqdConfig();
    uvalue[0]=(args[0]);
    if(uvalue[0]>3)
      return 0;
    utemp=(args[1]);
    uvalue[1]=utemp;
    utemp=(args[2]);
    uvalue[1]|=(utemp<<8);
    utemp=(args[3]);
    uvalue[1]|=(utemp<<16);
    utemp=(args[4]);
    uvalue[1]|=(utemp<<24);
    fvalue=uvalue[1]/10000.;
    dacPGain[uvalue[0]]=fvalue;
    configModifyFloatArray("Acqd.config","thresholds","dacPGain",dacPGain,3,&rawtime);
    retVal=sendSignal(ID_ACQD,SIGUSR1);
    if(retVal) return 0;
    return rawtime;
  case ACQD_SET_PID_IGAIN:
    readAcqdConfig();
    uvalue[0]=(args[0]);
    if(uvalue[0]>3)
      return 0;
    utemp=(args[1]);
    uvalue[1]=utemp;
    utemp=(args[2]);
    uvalue[1]|=(utemp<<8);
    utemp=(args[3]);
    uvalue[1]|=(utemp<<16);
    utemp=(args[4]);
    uvalue[1]|=(utemp<<24);
    fvalue=uvalue[1]/10000.;
    dacIGain[uvalue[0]]=fvalue;
    configModifyFloatArray("Acqd.config","thresholds","dacIGain",dacIGain,3,&rawtime);
    retVal=sendSignal(ID_ACQD,SIGUSR1);
    if(retVal) return 0;
    return rawtime;
  case ACQD_SET_PID_DGAIN:
    readAcqdConfig();
    uvalue[0]=(args[0]);
    if(uvalue[0]>3)
      return 0;
    utemp=(args[1]);
    uvalue[1]=utemp;
    utemp=(args[2]);
    uvalue[1]|=(utemp<<8);
    utemp=(args[3]);
    uvalue[1]|=(utemp<<16);
    utemp=(args[4]);
    uvalue[1]|=(utemp<<24);
    fvalue=uvalue[1]/10000.;
    dacDGain[uvalue[0]]=fvalue;
    configModifyFloatArray("Acqd.config","thresholds","dacDGain",dacDGain,3,&rawtime);
    retVal=sendSignal(ID_ACQD,SIGUSR1);
    if(retVal) return 0;
    return rawtime;
  case ACQD_SET_PID_IMAX:
    readAcqdConfig();
    uvalue[0]=(args[0]);
    if(uvalue[0]>3)
      return 0;
    utemp=(args[1]);
    uvalue[1]=utemp;
    utemp=(args[2]);
    uvalue[1]|=(utemp<<8);
    utemp=(args[3]);
    uvalue[1]|=(utemp<<16);
    utemp=(args[4]);
    uvalue[1]|=(utemp<<24);
    fvalue=uvalue[1];
    dacIMax[uvalue[0]]=uvalue[1];
    configModifyIntArray("Acqd.config","thresholds","dacIMax",dacIMax,3,&rawtime);
    retVal=sendSignal(ID_ACQD,SIGUSR1);
    if(retVal) return 0;
    return rawtime;
  case ACQD_SET_PID_IMIN:
    readAcqdConfig();
    uvalue[0]=(args[0]);
    if(uvalue[0]>3)
      return 0;
    utemp=(args[1]);
    uvalue[1]=utemp;
    utemp=(args[2]);
    uvalue[1]|=(utemp<<8);
    utemp=(args[3]);
    uvalue[1]|=(utemp<<16);
    utemp=(args[4]);
    uvalue[1]|=(utemp<<24);
    fvalue=uvalue[1];
    dacIMin[uvalue[0]]=fvalue*-1;
    configModifyIntArray("Acqd.config","thresholds","dacIMin",dacIMin,3,&rawtime);
    retVal=sendSignal(ID_ACQD,SIGUSR1);
    if(retVal) return 0;
    return rawtime;
  /* case ACQD_RATE_SET_PHI_MASK_HPOL: 	     */
  /*   utemp=(args[0]);	     */
  /*   uvalue[0]=utemp; */
  /*   utemp=(args[1]); */
  /*   uvalue[0]|=(utemp<<8); */
  /*   syslog(LOG_INFO,"ACQD_RATE_SET_PHI_MASK_HPOL: %d %d -- %u -- %#x\n",args[0],args[1],uvalue[0],(unsigned int)uvalue[0]); */

  /*   configModifyUnsignedInt("Acqd.config","thresholds","phiTrigMaskH",uvalue[0],&rawtime); */
  /*   retVal=sendSignal(ID_ACQD,SIGUSR1); */
  /*   if(retVal) return 0; */
  /*   return rawtime; */

  default:
    fprintf(stderr,"Unknown Acqd Rate Command -- %d\n",command);
    syslog(LOG_ERR,"Unknown Acqd Rate Command -- %d\n",command);
    return 0;
  }
  return 0;
}

int executeLosdControlCommand(int command, unsigned char args[2])
{
  int ivalue=0,ivalue2=0;
  double fvalue;
  time_t rawtime;
  int retVal=0;

  switch(command) {
  case LOSD_PRIORITY_BANDWIDTH:
    ivalue=args[0];
    ivalue2=args[1];
    if(ivalue<0 || ivalue>9) return -1;
    if(ivalue2<0 || ivalue2>100) return -1;
    return setLosdPriorityBandwidth(ivalue,ivalue2);
  case LOSD_SEND_DATA:
    ivalue=args[0];
    if(ivalue>1 || ivalue<0) return -1;
    configModifyInt("LOSd.config","losd","sendData",ivalue,&rawtime);
    retVal=sendSignal(ID_LOSD,SIGUSR1);
    if(retVal!=0) return 0;
    return rawtime;
  case LOSD_MIN_WAIT_TIME_B:
    ivalue = args[0];
    fvalue = ivalue / 1000.;
    configModifyFloat("LOSd.config","losd","minTimeWait_b", fvalue, &rawtime);
    retVal=sendSignal(ID_LOSD,SIGUSR1);
    if(retVal!=0) return 0;
    return rawtime;

  case LOSD_MIN_WAIT_TIME_M:
    ivalue = args[0];
    fvalue = ivalue / 1.e6;
    configModifyFloat("LOSd.config","losd","minTimeWait_m", fvalue, &rawtime);
    retVal=sendSignal(ID_LOSD,SIGUSR1);
    if(retVal!=0) return 0;
    return rawtime;


  default:
    syslog(LOG_ERR,"Unknown LOSd command -- %d\n",command);
    fprintf(stderr,"Unknown LOSd command -- %d\n",command);
    return -1;
  }

  return rawtime;
}


int executeSipdControlCommand(int command, unsigned char args[3])
{
  int ivalue=0,ivalue2=0;
  time_t rawtime;
  int retVal=0;

  switch(command) {
  case SIPD_SEND_WAVE:
    ivalue=args[0];
    if(ivalue<0 || ivalue>1) return -1;
    configModifyInt("SIPd.config","sipd","sendWavePackets",ivalue,&rawtime);
    retVal=sendSignal(ID_SIPD,SIGUSR1);
    if(retVal!=0) return 0;
    return rawtime;
  case SIPD_THROTTLE_RATE:
    ivalue=args[0]+(args[1]<<8);
    //    if(ivalue>680) return -1;
    configModifyInt("SIPd.config","sipd","throttleRate",ivalue,&rawtime);
    retVal=sendSignal(ID_SIPD,SIGUSR1);
    if(retVal!=0) return 0;
    return rawtime;
  case SIPD_HEADERS_PER_EVENT:
    ivalue=args[0];
    configModifyInt("SIPd.config","bandwidth","headersPerEvent",ivalue,&rawtime);
    retVal=sendSignal(ID_SIPD,SIGUSR1);
    if(retVal!=0) return 0;
    return rawtime;
  case SIPD_PRIORITY_BANDWIDTH:
    ivalue=args[0];
    ivalue2=args[1];
    if(ivalue<0 || ivalue>9) return -1;
    if(ivalue2<0 || ivalue2>100) return -1;
    return setSipdPriorityBandwidth(ivalue,ivalue2);
    break;
  case SIPD_HK_TELEM_ORDER:
    ivalue=args[0];
    ivalue2=args[1];
    if(ivalue<0 || ivalue>20) return -1;
    if(ivalue2<0 || ivalue2>20) return -1;
    return setSipdHkTelemOrder(ivalue,ivalue2);
    break;
  case SIPD_HK_TELEM_MAX_PACKETS:
    ivalue=args[0];
    ivalue2=args[1];
    if(ivalue<0 || ivalue>20) return -1;
    if(ivalue2<0 || ivalue2>20) return -1;
    return setSipdHkTelemMaxPackets(ivalue,ivalue2);
    break;


  default:
    syslog(LOG_ERR,"Unknown SIPd command -- %d\n",command);
    fprintf(stderr,"Unknown SIPd command -- %d\n",command);
    return -1;
  }

  return rawtime;
}




int setTelemPriEncodingType(int pri, int encType,int encClockType)
{
  time_t rawtime;
  readArchivedConfig();
  priTelemEncodingType[pri]=encType;
  priTelemClockEncodingType[pri]=encClockType;
  configModifyIntArray("Archived.config","archived","priTelemEncodingType",priTelemEncodingType,NUM_PRIORITIES,&rawtime);
  configModifyIntArray("Archived.config","archived","priTelemClockEncodingType",priTelemClockEncodingType,NUM_PRIORITIES,&rawtime);
  sendSignal(ID_ARCHIVED,SIGUSR1);
  return rawtime;
}

int setPidGoalScaleFactor(int surf, int dac, float scaleFactor)
{
  time_t rawtime;
  char keyName[180];
  readAcqdConfig();
  pidGoalScaleFactors[surf][dac]=scaleFactor;
  sprintf(keyName,"pidGoalScaleSurf%d",surf+1);
  configModifyFloatArray("Acqd.config","thresholds",keyName,&(pidGoalScaleFactors[surf][0]),SCALERS_PER_SURF,&rawtime);
  sendSignal(ID_ACQD,SIGUSR1);
  return rawtime;
}


int mountNextNtu(int whichDrive) {
  time_t rawtime;
  int currentNum=0;
  readConfig(); // Get latest names and status
  //    if(disableHelium1) return -1;

  sscanf(ntuName,"disk%d",&currentNum);
  if(whichDrive==0)
    currentNum++;
  else {
    currentNum=whichDrive;
  }

  if(currentNum>NUM_NTU_SSD) return -1;

  //Kill all programs that write to disk
  killDataPrograms();
  closeHkFilesAndTidy(&cmdWriter);
  sleep(1);
  //Change to new drive
  sprintf(ntuName,"disk%d",currentNum);
  configModifyString("anitaSoft.config","global","ntuName",ntuName,&rawtime);
  system("/home/anita/flightSoft/bin/mountCurrentNtu.sh");
  sleep(2);
  makeNewRunDirs();
  sleep(2);
  prepWriterStructs();
  startDataPrograms();
  return rawtime;
}



int mountNextUsb(int whichUsb) {
  time_t rawtime;
  int currentPort=1;
  int currentDrive=1;
  //  int currentNum=0;
  //  int currentNum[2]={0};
  readConfig(); // Get latest names and status

  //    if(hkDiskBitMask&USB_DISK_MASK) disableUsb=0;
  //    if(eventDiskBitMask&USB_DISK_MASK) disableUsb=0;

  //    if(hkDiskBitMask&USBEXT_DISK_MASK) disableUsbExt=0;
  //    if(eventDiskBitMask&USBEXT_DISK_MASK) disableUsbExt=0;

  //    if(intExtFlag<1 || intExtFlag>3) return 0;

  //    if(intExtFlag==1 && disableUsb) return 0;
  //    else if(intExtFlag==2 && disableUsbExt) return 0;
  //    else if(intExtFlag==3 && disableUsbExt && disableUsb) return 0;
  //    else if(intExtFlag==3 && disableUsb) intExtFlag=2;
  //    else if(intExtFlag==3 && disableUsbExt) intExtFlag=1;

  sscanf(usbName,"rf_%d_%d",&currentPort,&currentDrive);
  if(whichUsb==0) {
    currentDrive++;
    if(currentDrive>8) {
      currentDrive=1;
      currentPort++;
      if(currentPort>3) {
	return 0;
      }
    }
  }
  else  {
    //    currentNum=whichUsb;
    currentPort=(whichUsb/8)+1;
    currentDrive=(whichUsb%8)+1;
    if(currentPort>3) {
      return 0;
    }
  }
  //Kill all programs that write to disk
  killDataPrograms();
  closeHkFilesAndTidy(&cmdWriter);
  sleep(2); //Let them gzip their data

  //    exit(0);
  //Change to new drive
  //  sprintf(usbName,"bigint%02d",currentNum[0]);
  sprintf(usbName,"rf_%d_%d",currentPort,currentDrive);
  configModifyString("anitaSoft.config","global","usbName",usbName,&rawtime);
  system("/home/anita/flightSoft/bin/mountCurrentUsb.sh");

  sleep(2);
  makeNewRunDirs();
  sleep(2);
  prepWriterStructs();
  startDataPrograms();
  return rawtime;

}


static int requestCounter=0;

int requestJournalCtl(JournalctlOptionCommand_t jcOpt, int cmdArg, int numLines)
{

  LogWatchRequest_t theRequest;
  char outName[FILENAME_MAX];
  time_t rawtime;
  time(&rawtime);
  theRequest.numLines=numLines;
  theRequest.logReq=LOG_REQUEST_JOURNALCTL;
  theRequest.jclOpt=jcOpt;
  theRequest.optArg=cmdArg;
  sprintf(outName,"%s/request_%d.dat",LOGWATCH_DIR,requestCounter);
  requestCounter++;
  writeStruct(&theRequest,outName,sizeof(LogWatchRequest_t));
  makeLink(outName,LOGWATCH_LINK_DIR);
  return rawtime;

}


int requestFile(char *filename, int numLines) {
  LogWatchRequest_t theRequest;
  char outName[FILENAME_MAX];
  time_t rawtime;
  time(&rawtime);
  theRequest.numLines=numLines;
  theRequest.logReq=LOG_REQUEST_FILE;
  strncpy(theRequest.filename,filename,179);
  sprintf(outName,"%s/request_%d.dat",LOGWATCH_DIR,requestCounter);
  requestCounter++;
  writeStruct(&theRequest,outName,sizeof(LogWatchRequest_t));
  makeLink(outName,LOGWATCH_LINK_DIR);
  return rawtime;
}


void actuallyWriteCommandEcho(CommandEcho_t *echoPtr, const char *mainDir, const char *linkDir) {
  char filename[FILENAME_MAX];
  //Write cmd echo for losd
  sprintf(filename,"%s/cmd_%d.dat",mainDir,echoPtr->unixTime);
  {
    FILE *pFile;
    int fileTag=1;
    pFile = fopen(filename,"rb");
    while(pFile!=NULL) {
      fclose(pFile);
      sprintf(filename,"%s/cmd_%d_%d.dat",mainDir,echoPtr->unixTime,fileTag);
      pFile=fopen(filename,"rb");
      fileTag++;
    }
  }
  printf("Writing to file %s\n",filename);
  fillGenericHeader(echoPtr,echoPtr->gHdr.code,sizeof(CommandEcho_t));
  writeStruct(echoPtr,filename,sizeof(CommandEcho_t));
  makeLink(filename,linkDir);
}


int writeCommandEcho(CommandStruct_t *theCmd, int unixTime)
{
  //    if(printToScreen)
  //	printf("writeCommandEcho\n");
  CommandEcho_t theEcho;
  int count,retVal=0;

  char cmdString[100];
  theEcho.gHdr.code=PACKET_CMD_ECHO;
  if(theCmd->fromSipd==0)
    theEcho.gHdr.code|=CMD_FROM_PAYLOAD;
  sprintf(cmdString,"%d",theCmd->cmd[0]);
  theEcho.numCmdBytes=theCmd->numCmdBytes;
  theEcho.cmd[0]=theCmd->cmd[0];
  //    if(printToScreen)
  //	printf("copying command\n");

  if(theCmd->numCmdBytes>1) {
    for(count=1;count<theCmd->numCmdBytes;count++) {
      sprintf(cmdString,"%s %d",cmdString,theCmd->cmd[count]);
      theEcho.cmd[count]=theCmd->cmd[count];
    }
  }
  //    if(printToScreen)
  //	printf("Doing something\n");

  if(unixTime) {
    theEcho.unixTime=unixTime;
    theEcho.goodFlag=1;
  }
  else {
    time_t rawtime;
    time(&rawtime);
    theEcho.unixTime=rawtime;
    theEcho.goodFlag=0;
    syslog(LOG_ERR,"Error executing cmd: %s",cmdString);
    fprintf(stderr,"Error executing cmd: %s\n",cmdString);
  }

  //Write cmd echo for losd,sipd,openportd
  actuallyWriteCommandEcho(&theEcho,LOSD_CMD_ECHO_TELEM_DIR,LOSD_CMD_ECHO_TELEM_LINK_DIR);
  actuallyWriteCommandEcho(&theEcho,SIPD_CMD_ECHO_TELEM_DIR,SIPD_CMD_ECHO_TELEM_LINK_DIR);
  actuallyWriteCommandEcho(&theEcho,OPENPORTD_CMD_ECHO_TELEM_DIR,OPENPORTD_CMD_ECHO_TELEM_LINK_DIR);


  //Write Archive
  cleverHkWrite((unsigned char*)&theEcho,sizeof(CommandEcho_t),
		theEcho.unixTime,&cmdWriter);

  return retVal;
}


void prepWriterStructs() {
  int diskInd;
  if(printToScreen)
    printf("Preparing Writer Structs\n");
  //Hk Writer

  sprintf(cmdWriter.relBaseName,"%s/",CMD_ARCHIVE_DIR);
  sprintf(cmdWriter.filePrefix,"cmd");
  for(diskInd=0;diskInd<DISK_TYPES;diskInd++)
    cmdWriter.currentFilePtr[diskInd]=0;
  cmdWriter.writeBitMask=hkDiskBitMask;

}


int readAcqdConfig()

/* Load Acqd config stuff */
{
  /* Config file thingies */
  int status=0,surf,dac;
  char keyName[180];
  int tempNum=12;
  //    int surfSlots[MAX_SURFS];
  //    int surfBuses[MAX_SURFS];
  //    int surfSlotCount=0;
  //    int surfBusCount=0;
  KvpErrorCode kvpStatus=0;
  char* eString ;
  //    char *tempString;

  kvpReset();
  status = configLoad (GLOBAL_CONF_FILE,"global") ;
  status += configLoad ("Acqd.config","output") ;
  status += configLoad ("Acqd.config","locations") ;
  status += configLoad ("Acqd.config","debug") ;
  status += configLoad ("Acqd.config","trigger") ;
  status += configLoad ("Acqd.config","thresholds") ;
  status += configLoad ("Acqd.config","acqd") ;
  status += configLoad ("Acqd.config","pedestal") ;
  //    printf("Debug rc1\n");
  if(status == CONFIG_E_OK) {
    //	hkDiskBitMask=kvpGetInt("hkDiskBitMask",1);
    //        niceValue=kvpGetInt("niceValue",-20);
    //	pedestalMode=kvpGetInt("pedestalMode",0);
    //	useInterrupts=kvpGetInt("useInterrupts",0);
    //	firmwareVersion=kvpGetInt("firmwareVersion",2);
    //	turfFirmwareVersion=kvpGetInt("turfFirmwareVersion",2);
    //	surfHkPeriod=kvpGetInt("surfHkPeriod",1);
    //	surfHkTelemEvery=kvpGetInt("surfHkTelemEvery",1);
    //	turfRateTelemEvery=kvpGetInt("turfRateTelemEvery",1);
    //	turfRateTelemInterval=kvpGetInt("turfRateTelemInterval",1);
    //	sendSoftTrigger=kvpGetInt("sendSoftTrigger",0);
    //	softTrigSleepPeriod=kvpGetInt("softTrigSleepPeriod",0);
    //	reprogramTurf=kvpGetInt("reprogramTurf",0);
    //	pps1TrigFlag=kvpGetInt("enablePPS1Trigger",1);
    //	pps2TrigFlag=kvpGetInt("enablePPS2Trigger",1);
    //	doThresholdScan=kvpGetInt("doThresholdScan",0);
    //	doSingleChannelScan=kvpGetInt("doSingleChannelScan",0);
    //	surfForSingle=kvpGetInt("surfForSingle",0);
    //	dacForSingle=kvpGetInt("dacForSingle",0);
    //	thresholdScanStepSize=kvpGetInt("thresholdScanStepSize",0);
    //	thresholdScanPointsPerStep=kvpGetInt("thresholdScanPointsPerStep",0);
    //	threshSwitchConfigAtEnd=kvpGetInt("threshSwitchConfigAtEnd",1);
    //	setGlobalThreshold=kvpGetInt("setGlobalThreshold",0);
    //	globalThreshold=kvpGetInt("globalThreshold",0);
    //	dacPGain=kvpGetFloat("dacPGain",0);
    //	dacIGain=kvpGetFloat("dacIGain",0);
    //	dacDGain=kvpGetFloat("dacDGain",0);
    //	dacIMin=kvpGetInt("dacIMin",0);
    //	dacIMax=kvpGetInt("dacIMax",0);
    //	enableChanServo=kvpGetInt("enableChanServo",1);
    //	if(doThresholdScan)
    //	    enableChanServo=0;
    //	pidGoal=kvpGetInt("pidGoal",2000);
    //	pidPanicVal=kvpGetInt("pidPanicVal",200);
    //	pidAverage=kvpGetInt("pidAverage",1);
    //	if(pidAverage<1) pidAverage=1;
    //	tempNum=2;
    //	antTrigMask=kvpGetUnsignedInt("antTrigMask",0);
    //	turfioPos.bus=kvpGetInt("turfioBus",3); //in seconds
    //	turfioPos.slot=kvpGetInt("turfioSlot",10); //in seconds

    //PID Stuff
    tempNum=BANDS_PER_ANT;
    kvpStatus=kvpGetFloatArray("dacPGain",&dacPGain[0],&tempNum);
    if(kvpStatus!=KVP_E_OK) {
      syslog(LOG_WARNING,"kvpGetIntArray(dacPGain): %s",
	     kvpErrorString(kvpStatus));
      fprintf(stderr,"kvpGetIntArray(dacPGain): %s\n",
	      kvpErrorString(kvpStatus));
    }
    tempNum=BANDS_PER_ANT;
    kvpStatus=kvpGetFloatArray("dacIGain",&dacIGain[0],&tempNum);
    if(kvpStatus!=KVP_E_OK) {
      syslog(LOG_WARNING,"kvpGetIntArray(dacIGain): %s",
	     kvpErrorString(kvpStatus));
      fprintf(stderr,"kvpGetIntArray(dacIGain): %s\n",
	      kvpErrorString(kvpStatus));
    }
    tempNum=BANDS_PER_ANT;
    kvpStatus=kvpGetFloatArray("dacDGain",&dacDGain[0],&tempNum);
    if(kvpStatus!=KVP_E_OK) {
      syslog(LOG_WARNING,"kvpGetIntArray(dacDGain): %s",
	     kvpErrorString(kvpStatus));
      fprintf(stderr,"kvpGetIntArray(dacDGain): %s\n",
	      kvpErrorString(kvpStatus));
    }

    tempNum=BANDS_PER_ANT;
    kvpStatus=kvpGetIntArray("dacIMin",&dacIMin[0],&tempNum);
    if(kvpStatus!=KVP_E_OK) {
      syslog(LOG_WARNING,"kvpGetIntArray(dacIMin): %s",
	     kvpErrorString(kvpStatus));
      fprintf(stderr,"kvpGetIntArray(dacIMin): %s\n",
	      kvpErrorString(kvpStatus));
    }
    tempNum=BANDS_PER_ANT;
    kvpStatus=kvpGetIntArray("dacIMax",&dacIMax[0],&tempNum);
    if(kvpStatus!=KVP_E_OK) {
      syslog(LOG_WARNING,"kvpGetIntArray(dacIMax): %s",
	     kvpErrorString(kvpStatus));
      fprintf(stderr,"kvpGetIntArray(dacIMax): %s\n",
	      kvpErrorString(kvpStatus));
    }

    tempNum=ACTIVE_SURFS;
    kvpStatus=kvpGetIntArray("surfTrigBandMasks",&surfTrigBandMasks[0],&tempNum);
    if(kvpStatus!=KVP_E_OK) {
      syslog(LOG_WARNING,"kvpGetIntArray(surfTrigBandMasks): %s",
	     kvpErrorString(kvpStatus));
      if(printToScreen)
	fprintf(stderr,"kvpGetIntArray(surfTrigBandMasks): %s\n",
		kvpErrorString(kvpStatus));
    }

 tempNum=PHI_SECTORS;
    kvpStatus = kvpGetIntArray("dynamicL2ThresholdOverRate",&(dynamicL2ThresholdOverRate[0]),&tempNum);
    if(kvpStatus!=KVP_E_OK) {
      syslog(LOG_WARNING,"kvpGetIntArray(dynamicL2ThresholdOverRate): %s",
	     kvpErrorString(kvpStatus));
      if(printToScreen)
	fprintf(stderr,"kvpGetIntArray(dynamicL2ThresholdOverRate): %s\n",
		kvpErrorString(kvpStatus));
    }

    //    dynamicL2ThresholdUnderRate=kvpGetInt("dynamicL2ThresholdUnderRate",100000);
    tempNum=PHI_SECTORS;
    kvpStatus = kvpGetIntArray("dynamicL2ThresholdUnderRate",&(dynamicL2ThresholdUnderRate[0]),&tempNum);
    if(kvpStatus!=KVP_E_OK) {
      syslog(LOG_WARNING,"kvpGetIntArray(dynamicL2ThresholdUnderRate): %s",
	     kvpErrorString(kvpStatus));
      if(printToScreen)
	fprintf(stderr,"kvpGetIntArray(dynamicL2ThresholdUnderRate): %s\n",
		kvpErrorString(kvpStatus));
    }



    for(surf=0;surf<ACTIVE_SURFS;surf++) {
      for(dac=0;dac<SCALERS_PER_SURF;dac++) {
	pidGoalScaleFactors[surf][dac]=0;
      }
      sprintf(keyName,"pidGoalScaleSurf%d",surf+1);
      tempNum=SCALERS_PER_SURF;
      kvpStatus = kvpGetFloatArray(keyName,&(pidGoalScaleFactors[surf][0]),&tempNum);
      if(kvpStatus!=KVP_E_OK) {
	syslog(LOG_WARNING,"kvpGetFloatArray(%s): %s",keyName,
	       kvpErrorString(kvpStatus));
	if(printToScreen)
	  fprintf(stderr,"kvpGetFloatArray(%s): %s\n",keyName,
		  kvpErrorString(kvpStatus));
      }

    }




    /* 	tempNum=12; */
    /* 	kvpStatus = kvpGetIntArray("surfBus",surfBuses,&tempNum); */
    /* 	if(kvpStatus!=KVP_E_OK) { */
    /* 	    syslog(LOG_WARNING,"kvpGetIntArray(surfBus): %s", */
    /* 		   kvpErrorString(kvpStatus)); */
    /* 	    if(printToScreen) */
    /* 		fprintf(stderr,"kvpGetIntArray(surfBus): %s\n", */
    /* 			kvpErrorString(kvpStatus)); */
    /* 	} */
    /* 	else { */
    /* 	    for(count=0;count<tempNum;count++) { */
    /* 		surfPos[count].bus=surfBuses[count]; */
    /* 		if(surfBuses[count]!=-1) { */
    /* 		    surfBusCount++; */
    /* 		} */
    /* 	    } */
    /* 	} */
    /* 	tempNum=12; */
    /* 	kvpStatus = kvpGetIntArray("surfSlot",surfSlots,&tempNum); */
    /* 	if(kvpStatus!=KVP_E_OK) { */
    /* 	    syslog(LOG_WARNING,"kvpGetIntArray(surfSlot): %s", */
    /* 		   kvpErrorString(kvpStatus)); */
    /* 	    if(printToScreen) */
    /* 		fprintf(stderr,"kvpGetIntArray(surfSlot): %s\n", */
    /* 			kvpErrorString(kvpStatus)); */
    /* 	} */
    /* 	else { */
    /* 	    for(count=0;count<tempNum;count++) { */
    /* 		surfPos[count].slot=surfSlots[count]; */
    /* 		if(surfSlots[count]!=-1) { */
    /* 		    surfSlotCount++; */
    /* 		} */
    /* 	    } */
    /* 	} */
    /* 	if(surfSlotCount==surfBusCount) numSurfs=surfBusCount; */
    /* 	else { */
    /* 	    syslog(LOG_ERR,"Confused config file bus and slot count do not agree"); */
    /* 	    if(printToScreen) */
    /* 		fprintf(stderr,"Confused config file bus and slot count do not agree\n"); */
    /* 	    exit(0); */
    /* 	} */

    /* 	tempNum=12; */
    /* 	kvpStatus = kvpGetIntArray("dacSurfs",dacSurfs,&tempNum); */
    /* 	if(kvpStatus!=KVP_E_OK) { */
    /* 	    syslog(LOG_WARNING,"kvpGetIntArray(dacSurfs): %s", */
    /* 		   kvpErrorString(kvpStatus)); */
    /* 	    if(printToScreen) */
    /* 		fprintf(stderr,"kvpGetIntArray(dacSurfs): %s\n", */
    /* 			kvpErrorString(kvpStatus)); */
    /* 	} */

    /* 	for(surf=0;surf<ACTIVE_SURFS;surf++) { */
    /* 	    for(dac=0;dac<N_RFTRIG;dac++) { */
    /* 		thresholdArray[surf][dac]=0; */
    /* 	    } */
    /* 	    if(dacSurfs[surf]) {		 */
    /* 		sprintf(keyName,"threshSurf%d",surf+1); */
    /* 		tempNum=32;	        */
    /* 		kvpStatus = kvpGetIntArray(keyName,&(thresholdArray[surf][0]),&tempNum); */
    /* 		if(kvpStatus!=KVP_E_OK) { */
    /* 		    syslog(LOG_WARNING,"kvpGetIntArray(%s): %s",keyName, */
    /* 			   kvpErrorString(kvpStatus)); */
    /* 		    if(printToScreen) */
    /* 			fprintf(stderr,"kvpGetIntArray(%s): %s\n",keyName, */
    /* 				kvpErrorString(kvpStatus)); */
    /* 		} */
    /* 	    } */
    /* 	} */

    /* 	//Pedestal things */
    /* 	writeDebugData=kvpGetInt("writeDebugData",1); */
    /* 	numPedEvents=kvpGetInt("numPedEvents",4000); */
    /* 	pedSwitchConfigAtEnd=kvpGetInt("pedSwitchConfigAtEnd",1); */


  }
  else {
    eString=configErrorString (status) ;
    syslog(LOG_ERR,"Error reading Acqd.config: %s\n",eString);
    //	if(printToScreen)
    fprintf(stderr,"Error reading Acqd.config: %s\n",eString);

  }

  return status;
}

void copyCommand(CommandStruct_t *theCmd, CommandStruct_t *cpCmd)
{
  int i;
  cpCmd->numCmdBytes=theCmd->numCmdBytes;
  for(i=0;i<theCmd->numCmdBytes;i++) {
    cpCmd->cmd[i]=theCmd->cmd[i];
  }
}

int sameCommand(CommandStruct_t *theCmd, CommandStruct_t *lastCmd)
{
  int i;
  int different=0;
  different+=(theCmd->numCmdBytes!=lastCmd->numCmdBytes);
  for(i=0;i<theCmd->numCmdBytes;i++) {
    different+=(theCmd->cmd[i]!=lastCmd->cmd[i]);
  }
  if(different==0) return 1;
  return 0;
}



int readArchivedConfig()
/* Load Archived config stuff */
{
  /* Config file thingies */
  int status=0,tempNum=0;
  char* eString ;
  KvpErrorCode kvpStatus;
  kvpReset();
  status = configLoad ("Archived.config","archived") ;
  if(status == CONFIG_E_OK) {
    tempNum=10;
    kvpStatus = kvpGetIntArray("priDiskEncodingType",
			       priDiskEncodingType,&tempNum);
    if(kvpStatus!=KVP_E_OK) {
      syslog(LOG_WARNING,"kvpGetIntArray(priDiskEncodingType): %s",
	     kvpErrorString(kvpStatus));
      if(printToScreen)
	fprintf(stderr,"kvpGetIntArray(priDiskEncodingType): %s\n",
		kvpErrorString(kvpStatus));
    }



    tempNum=10;
    kvpStatus = kvpGetIntArray("priTelemClockEncodingType",
			       priTelemClockEncodingType,&tempNum);
    if(kvpStatus!=KVP_E_OK) {
      syslog(LOG_WARNING,"kvpGetIntArray(priTelemClockEncodingType): %s",
	     kvpErrorString(kvpStatus));
      if(printToScreen)
	fprintf(stderr,"kvpGetIntArray(priTelemClockEncodingType): %s\n",
		kvpErrorString(kvpStatus));
    }


    tempNum=10;
    kvpStatus = kvpGetIntArray("priTelemEncodingType",
			       priTelemEncodingType,&tempNum);
    if(kvpStatus!=KVP_E_OK) {
      syslog(LOG_WARNING,"kvpGetIntArray(priTelemEncodingType): %s",
	     kvpErrorString(kvpStatus));
      if(printToScreen)
	fprintf(stderr,"kvpGetIntArray(priTelemEncodingType): %s\n",
		kvpErrorString(kvpStatus));
    }


    tempNum=NUM_PRIORITIES;
    kvpStatus = kvpGetFloatArray("priorityFractionGlobalDecimate",
				 priorityFractionGlobalDecimate,&tempNum);
    if(kvpStatus!=KVP_E_OK) {
      syslog(LOG_WARNING,"kvpGetFloatArray(priorityFractionGlobalDecimate): %s",
	     kvpErrorString(kvpStatus));
      fprintf(stderr,"kvpGetFloatArray(priorityFractionGlobalDecimate): %s\n",
	      kvpErrorString(kvpStatus));
    }

    tempNum=NUM_PRIORITIES;
    kvpStatus = kvpGetFloatArray("priorityFractionDeleteHelium1",
				 priorityFractionDeleteHelium1,&tempNum);
    if(kvpStatus!=KVP_E_OK) {
      syslog(LOG_WARNING,"kvpGetFloatArray(priorityFractionDeleteHelium1): %s",
	     kvpErrorString(kvpStatus));
      fprintf(stderr,"kvpGetFloatArray(priorityFractionDeleteHelium1): %s\n",
	      kvpErrorString(kvpStatus));
    }

    tempNum=NUM_PRIORITIES;
    kvpStatus = kvpGetFloatArray("priorityFractionDeleteHelium2",
				 priorityFractionDeleteHelium2,&tempNum);
    if(kvpStatus!=KVP_E_OK) {
      syslog(LOG_WARNING,"kvpGetFloatArray(priorityFractionDeleteHelium2): %s",
	     kvpErrorString(kvpStatus));
      fprintf(stderr,"kvpGetFloatArray(priorityFractionDeleteHelium2): %s\n",
	      kvpErrorString(kvpStatus));
    }

    tempNum=NUM_PRIORITIES;
    kvpStatus = kvpGetFloatArray("priorityFractionDeleteUsb",
				 priorityFractionDeleteUsb,&tempNum);
    if(kvpStatus!=KVP_E_OK) {
      syslog(LOG_WARNING,"kvpGetFloatArray(priorityFractionDeleteUsb): %s",
	     kvpErrorString(kvpStatus));
      fprintf(stderr,"kvpGetFloatArray(priorityFractionDeleteUsb): %s\n",
	      kvpErrorString(kvpStatus));
    }

    tempNum=NUM_PRIORITIES;
    kvpStatus = kvpGetFloatArray("priorityFractionDeleteNtu",
				 priorityFractionDeleteNtu,&tempNum);
    if(kvpStatus!=KVP_E_OK) {
      syslog(LOG_WARNING,"kvpGetFloatArray(priorityFractionDeleteNtu): %s",
	     kvpErrorString(kvpStatus));
      fprintf(stderr,"kvpGetFloatArray(priorityFractionDeleteNtu): %s\n",
	      kvpErrorString(kvpStatus));
    }

    tempNum=NUM_PRIORITIES;
    kvpStatus = kvpGetFloatArray("priorityFractionDeletePmc",
				 priorityFractionDeletePmc,&tempNum);
    if(kvpStatus!=KVP_E_OK) {
      syslog(LOG_WARNING,"kvpGetFloatArray(priorityFractionDeletePmc): %s",
	     kvpErrorString(kvpStatus));
      fprintf(stderr,"kvpGetFloatArray(priorityFractionDeletePmc): %s\n",
	      kvpErrorString(kvpStatus));
    }


    tempNum=10;
    kvpStatus = kvpGetIntArray("priDiskEventMask",
			       priDiskEventMask,&tempNum);
    if(kvpStatus!=KVP_E_OK) {
      syslog(LOG_WARNING,"kvpGetIntArray(priDiskEventMask): %s",
	     kvpErrorString(kvpStatus));
      if(printToScreen)
	fprintf(stderr,"kvpGetIntArray(priDiskEventMask): %s\n",
		kvpErrorString(kvpStatus));
    }


    tempNum=10;
    kvpStatus = kvpGetIntArray("alternateUsbs",
			       alternateUsbs,&tempNum);
    if(kvpStatus!=KVP_E_OK) {
      syslog(LOG_WARNING,"kvpGetIntArray(alternateUsbs): %s",
	     kvpErrorString(kvpStatus));
      if(printToScreen)
	fprintf(stderr,"kvpGetIntArray(alternateUsbs): %s\n",
		kvpErrorString(kvpStatus));
    }



  }
  else {
    eString=configErrorString (status) ;
    syslog(LOG_ERR,"Error reading Archived.config: %s\n",eString);
  }

  return status;
}



int readPrioritizerdConfig()
/* Load Prioritizerd config stuff */
{
  /* Config file thingies */
  int status=0,tempNum=0;
  char* eString ;
  KvpErrorCode kvpStatus;
  kvpReset();
  status = configLoad ("Prioritizerd.config","prioritizerd") ;
  status = configLoad ("Prioritizerd.config","antennalocations") ;
  if(status == CONFIG_E_OK) {
    tempNum=48;
    kvpStatus = kvpGetFloatArray("phiArrayDeg",
			       phiArrayDeg,&tempNum);
    if(kvpStatus!=KVP_E_OK) {
      syslog(LOG_WARNING,"kvpGetFloatArray(phiArrayDeg): %s",
	     kvpErrorString(kvpStatus));
      if(printToScreen)
	fprintf(stderr,"kvpGetFloatArray(phiArrayDeg): %s\n",
		kvpErrorString(kvpStatus));
    }

    tempNum=48;
    kvpStatus = kvpGetFloatArray("rArray",
			       rArray,&tempNum);
    if(kvpStatus!=KVP_E_OK) {
      syslog(LOG_WARNING,"kvpGetFloatArray(rArray): %s",
	     kvpErrorString(kvpStatus));
      if(printToScreen)
	fprintf(stderr,"kvpGetFloatArray(rArray): %s\n",
		kvpErrorString(kvpStatus));
    }

    tempNum=48;
    kvpStatus = kvpGetFloatArray("zArray",
			       zArray,&tempNum);
    if(kvpStatus!=KVP_E_OK) {
      syslog(LOG_WARNING,"kvpGetFloatArray(zArray): %s",
	     kvpErrorString(kvpStatus));
      if(printToScreen)
	fprintf(stderr,"kvpGetFloatArray(zArray): %s\n",
		kvpErrorString(kvpStatus));
    }


    tempNum=10;
    kvpStatus = kvpGetFloatArray("priorityParamsLowBinEdge",
				 priorityParamsLowBinEdge,&tempNum);
    if(kvpStatus!=KVP_E_OK) {
      syslog(LOG_WARNING,"kvpGetFloatArray(priorityParamsLowBinEdge): %s",
	     kvpErrorString(kvpStatus));
      fprintf(stderr,"kvpGetFloatArray(priorityParamsLowBinEdge): %s\n",
	      kvpErrorString(kvpStatus));
    }
    tempNum=10;
    kvpStatus = kvpGetFloatArray("priorityParamsHighBinEdge",
				 priorityParamsHighBinEdge,&tempNum);
    if(kvpStatus!=KVP_E_OK) {
      syslog(LOG_WARNING,"kvpGetFloatArray(priorityParamsHighBinEdge): %s",
	     kvpErrorString(kvpStatus));
      fprintf(stderr,"kvpGetFloatArray(priorityParamsHighBinEdge): %s\n",
	      kvpErrorString(kvpStatus));
    }


    tempNum = 16;
    kvpStatus = kvpGetIntArray("skipBlastRatioHPol", skipBlastRatioHPol, &tempNum);
    if(kvpStatus!=KVP_E_OK) {
      syslog(LOG_WARNING,"kvpGetIntArray(skipBlastRatioHPol): %s",
	     kvpErrorString(kvpStatus));
      fprintf(stderr,"kvpGetIntArray(skipBlastRatioHPol): %s\n",
	      kvpErrorString(kvpStatus));
    }

    tempNum = 16;
    kvpStatus = kvpGetIntArray("skipBlastRatioVPol", skipBlastRatioVPol, &tempNum);
    if(kvpStatus!=KVP_E_OK) {
      syslog(LOG_WARNING,"kvpGetIntArray(skipBlastRatioVPol): %s",
	     kvpErrorString(kvpStatus));
      fprintf(stderr,"kvpGetIntArray(skipBlastRatioVPol): %s\n",
	      kvpErrorString(kvpStatus));
    }

    tempNum = 10;
    kvpStatus = kvpGetFloatArray("staticNotchesLowEdgeMHz", staticNotchesLowEdgeMHz, &tempNum);
    if(kvpStatus!=KVP_E_OK) {
      syslog(LOG_WARNING,"kvpGetFloatArray(staticNotchesLowEdgeMHz): %s",
	     kvpErrorString(kvpStatus));
      fprintf(stderr,"kvpGetFloatArray(staticNotchesLowEdgeMHz): %s\n",
	      kvpErrorString(kvpStatus));
    }

    tempNum = 10;
    kvpStatus = kvpGetFloatArray("staticNotchesHighEdgeMHz", staticNotchesHighEdgeMHz, &tempNum);
    if(kvpStatus!=KVP_E_OK) {
      syslog(LOG_WARNING,"kvpGetFloatArray(staticNotchesHighEdgeMHz): %s",
	     kvpErrorString(kvpStatus));
      fprintf(stderr,"kvpGetFloatArray(staticNotchesHighEdgeMHz): %s\n",
	      kvpErrorString(kvpStatus));
    }






  }
  else {
    eString=configErrorString (status) ;
    syslog(LOG_ERR,"Error reading Prioritizerd.config: %s\n",eString);
  }

  return status;
}



int readLosdConfig()
/* Load Losd config stuff */
{
  /* Config file thingies */
  int status=0,tempNum=0;
  char* eString ;
  KvpErrorCode kvpStatus;
  kvpReset();
  status = configLoad ("LOSd.config","bandwidth") ;
  if(status == CONFIG_E_OK) {
    tempNum=10;
    kvpStatus = kvpGetIntArray("priorityBandwidths",
			       losdBandwidths,&tempNum);
    if(kvpStatus!=KVP_E_OK) {
      syslog(LOG_WARNING,"kvpGetIntArray(priorityBandwidths): %s",
	     kvpErrorString(kvpStatus));
      if(printToScreen)
	fprintf(stderr,"kvpGetIntArray(priorityBandwidths): %s\n",
		kvpErrorString(kvpStatus));
    }
  }
  else {
    eString=configErrorString (status) ;
    syslog(LOG_ERR,"Error reading LOSd.config: %s\n",eString);
  }

  return status;
}


int readGpsdConfig()
/* Load Gpsd config stuff */
{
  /* Config file thingies */
  int status=0,tempNum=0;
  char* eString ;
  KvpErrorCode kvpStatus;
  kvpReset();
  status = configLoad ("GPSd.config","sourceList") ;
  if(status == CONFIG_E_OK) {
    numSources=kvpGetInt("numSources",0);
    tempNum=MAX_PHI_SOURCES;
    kvpStatus = kvpGetFloatArray("sourceLats",
				 sourceLats,&tempNum);
    if(kvpStatus!=KVP_E_OK) {
      syslog(LOG_WARNING,"kvpGetIntArray(sourceLats): %s",
	     kvpErrorString(kvpStatus));
      if(printToScreen)
	fprintf(stderr,"kvpGetIntArray(sourceLats): %s\n",
		kvpErrorString(kvpStatus));
    }

    tempNum=MAX_PHI_SOURCES;
    kvpStatus = kvpGetFloatArray("sourceLongs",
				 sourceLongs,&tempNum);
    if(kvpStatus!=KVP_E_OK) {
      syslog(LOG_WARNING,"kvpGetIntArray(sourceLongs): %s",
	     kvpErrorString(kvpStatus));
      if(printToScreen)
	fprintf(stderr,"kvpGetIntArray(sourceLongs): %s\n",
		kvpErrorString(kvpStatus));
    }

    tempNum=MAX_PHI_SOURCES;
    kvpStatus = kvpGetFloatArray("sourceAlts",
				 sourceAlts,&tempNum);
    if(kvpStatus!=KVP_E_OK) {
      syslog(LOG_WARNING,"kvpGetIntArray(sourceAlts): %s",
	     kvpErrorString(kvpStatus));
      if(printToScreen)
	fprintf(stderr,"kvpGetIntArray(sourceAlts): %s\n",
		kvpErrorString(kvpStatus));
    }

    tempNum=MAX_PHI_SOURCES;
    kvpStatus = kvpGetFloatArray("sourceDistKm",
				 sourceDistKm,&tempNum);
    if(kvpStatus!=KVP_E_OK) {
      syslog(LOG_WARNING,"kvpGetIntArray(sourceDistKm): %s",
	     kvpErrorString(kvpStatus));
      if(printToScreen)
	fprintf(stderr,"kvpGetIntArray(sourceDistKm): %s\n",
		kvpErrorString(kvpStatus));
    }

    tempNum=MAX_PHI_SOURCES;
    kvpStatus = kvpGetIntArray("sourcePhiWidth",
			       sourcePhiWidth,&tempNum);
    if(kvpStatus!=KVP_E_OK) {
      syslog(LOG_WARNING,"kvpGetIntArray(sourcePhiWidth): %s",
	     kvpErrorString(kvpStatus));
      if(printToScreen)
	fprintf(stderr,"kvpGetIntArray(sourcePhiWidth): %s\n",
		kvpErrorString(kvpStatus));
    }



  }
  else {
    eString=configErrorString (status) ;
    syslog(LOG_ERR,"Error reading GPSd.config: %s\n",eString);
  }

  return status;
}

int readSipdConfig()
/* Load Sipd config stuff */
{
  /* Config file thingies */
  int status=0,tempNum=0;
  char* eString ;
  KvpErrorCode kvpStatus;
  kvpReset();
  status = configLoad ("SIPd.config","bandwidth") ;
  if(status == CONFIG_E_OK) {
    tempNum=10;
    kvpStatus = kvpGetIntArray("priorityBandwidths",
			       sipdBandwidths,&tempNum);
    if(kvpStatus!=KVP_E_OK) {
      syslog(LOG_WARNING,"kvpGetIntArray(priorityBandwidths): %s",
	     kvpErrorString(kvpStatus));
      if(printToScreen)
	fprintf(stderr,"kvpGetIntArray(priorityBandwidths): %s\n",
		kvpErrorString(kvpStatus));
    }

    tempNum=22;
    kvpStatus = kvpGetIntArray("hkTelemOrder",sipdHkTelemOrder,&tempNum);
    if(kvpStatus!=KVP_E_OK) {
      syslog(LOG_WARNING,"kvpGetIntArray(hkTelemOrder): %s",
	     kvpErrorString(kvpStatus));
      if(printToScreen)
	fprintf(stderr,"kvpGetIntArray(hkTelemOrder): %s\n",
		kvpErrorString(kvpStatus));
    }
    tempNum=22;
    kvpStatus = kvpGetIntArray("hkTelemMaxCopy",sipdHkTelemMaxCopy,&tempNum);
    if(kvpStatus!=KVP_E_OK) {
      syslog(LOG_WARNING,"kvpGetIntArray(hkTelemMaxCopy): %s",
	     kvpErrorString(kvpStatus));
      if(printToScreen)
	fprintf(stderr,"kvpGetIntArray(hkTelemMaxCopy): %s\n",
		kvpErrorString(kvpStatus));
    }

  }
  else {
    eString=configErrorString (status) ;
    syslog(LOG_ERR,"Error reading SIPd.config: %s\n",eString);
  }

  return status;
}


int makeNewRunDirs() {
  char filename[FILENAME_MAX];

  int diskInd=0;
  int retVal=0;
  int runNum=getNewRunNumber();
  char newRunDir[FILENAME_MAX];
  char currentDirLink[FILENAME_MAX];
  char mistakeDirName[FILENAME_MAX];
  RunStart_t runStart;
  runStart.runNumber=runNum;
  runStart.unixTime=time(NULL);
  runStart.eventNumber=getLatestEventNumber();
  runStart.eventNumber/=100;
  runStart.eventNumber++;
  runStart.eventNumber*=100; //Roughly

  //First make dirs
  for(diskInd=0;diskInd<DISK_TYPES;diskInd++) {
    if(!(hkDiskBitMask&diskBitMasks[diskInd]) &&
       !(eventDiskBitMask&diskBitMasks[diskInd]))
      continue; // Disk not enabled

    sprintf(newRunDir,"%s/run%d",diskNames[diskInd],runNum);
    sprintf(currentDirLink,"%s/current",diskNames[diskInd]);

    makeDirectories(newRunDir);
    removeFile(currentDirLink);
    if(is_dir(currentDirLink)) {
      sprintf(mistakeDirName,"%s/run%d.current",diskNames[diskInd],runNum-1);
      rename(currentDirLink,mistakeDirName);
    }
    symlink(newRunDir,currentDirLink);
    //    char theCommand[180];
    //    sprintf(theCommand,"/home/anita/flightSoft/bin/makeNewRunDirs.sh");
    //    retVal=system(theCommand);
  }

  //Now delete dirs
  //For now with system command
  system("rm -rf /tmp/anita/acqd /tmp/anita/eventd /tmp/anita/gpsd /tmp/anita/prioritizerd /tmp/anita/calibd /tmp/buffer/*");

  system("/home/anita/flightSoft/bin/createConfigAndLog.sh");

  fillGenericHeader(&runStart,PACKET_RUN_START,sizeof(RunStart_t));
  //Write file for Monitord
  normalSingleWrite((unsigned char*)&runStart,RUN_START_FILE,sizeof(RunStart_t));

  sprintf(filename,"%s/run_%u.dat",LOSD_CMD_ECHO_TELEM_DIR,runStart.runNumber);
  normalSingleWrite((unsigned char*)&runStart,filename,sizeof(RunStart_t));
  makeLink(filename,LOSD_CMD_ECHO_TELEM_LINK_DIR);

  sprintf(filename,"%s/run_%u.dat",SIPD_CMD_ECHO_TELEM_DIR,runStart.runNumber);
  normalSingleWrite((unsigned char*)&runStart,filename,sizeof(RunStart_t));
  makeLink(filename,SIPD_CMD_ECHO_TELEM_LINK_DIR);

  sprintf(filename,"%s/run_%u.dat",OPENPORTD_CMD_ECHO_TELEM_DIR,runStart.runNumber);
  normalSingleWrite((unsigned char*)&runStart,filename,sizeof(RunStart_t));
  makeLink(filename,OPENPORTD_CMD_ECHO_TELEM_LINK_DIR);

  return retVal;
}


int getNewRunNumber() {
  static int firstTime=1;
  static int runNumber=0;
  int retVal=0;
  /* This is just to get the lastRunNumber in case of program restart. */
  if(firstTime) {
    runNumber=getRunNumber();
  }
  runNumber++;

  FILE *pFile = fopen (LAST_RUN_NUMBER_FILE, "w");
  if(pFile == NULL) {
    syslog (LOG_ERR,"fopen: %s ---  %s\n",strerror(errno),
	    LAST_RUN_NUMBER_FILE);
    fprintf(stderr,"fopen: %s ---  %s\n",strerror(errno),
	    LAST_RUN_NUMBER_FILE);
  }
  else {
    retVal=fprintf(pFile,"%d\n",runNumber);
    if(retVal<0) {
      syslog (LOG_ERR,"fprintf: %s ---  %s\n",strerror(errno),
	      LAST_RUN_NUMBER_FILE);
      fprintf(stderr,"fprintf: %s ---  %s\n",strerror(errno),
	      LAST_RUN_NUMBER_FILE);
    }
    fclose (pFile);
  }
  return runNumber;

}

int clearRamdisk()
{
  int progMask=0;
  progMask|=SIPD_ID_MASK;
  progMask|=LOSD_ID_MASK;
  killDataPrograms();
  killPrograms(progMask);
  system("daemon --stop -n X");


  sleep(2);
  int retVal=system("rm -rf /tmp/anita /tmp/buffer /tmp/ntu");
  makeDirectories(LOSD_CMD_ECHO_TELEM_LINK_DIR);
  makeDirectories(SIPD_CMD_ECHO_TELEM_LINK_DIR);
  makeDirectories(CMDD_COMMAND_LINK_DIR);
  makeDirectories(REQUEST_TELEM_LINK_DIR);
  makeDirectories(PLAYBACK_LINK_DIR);
  makeDirectories(LOGWATCH_LINK_DIR);
  startPrograms(progMask);
  makeNewRunDirs();
  startDataPrograms();
  return retVal;
}


int getLatestEventNumber() {
  int retVal=0;
  int eventNumber=0;

  FILE *pFile;
  pFile = fopen (LAST_EVENT_NUMBER_FILE, "r");
  if(pFile == NULL) {
    syslog (LOG_ERR,"fopen: %s ---  %s\n",strerror(errno),
	    LAST_EVENT_NUMBER_FILE);
  }
  else {
    retVal=fscanf(pFile,"%d",&eventNumber);
    if(retVal<0) {
      syslog (LOG_ERR,"fscanff: %s ---  %s\n",strerror(errno),
	      LAST_EVENT_NUMBER_FILE);
    }
    fclose (pFile);
  }
  return eventNumber;
}


void handleBadSigs(int sig)
{
  fprintf(stderr,"Received sig %d -- will exit immediately\n",sig);
  syslog(LOG_WARNING,"Received sig %d -- will exit immediately\n",sig);
  unlink(CMDD_PID_FILE);
  syslog(LOG_INFO,"Cmdd terminating");

  if (sig == SIGSEGV)
  {
    size_t size;
    void * traceback[20];
    size = backtrace(traceback, 20);
    backtrace_symbols_fd(traceback,size, STDERR_FILENO);
  }


  exit(0);
}

int logRequestCommand(int logNum, int numLines)
{
  switch(logNum) {
  case LOG_REQUEST_JOURNALCTL:
    return requestJournalCtl(JOURNALCTL_NO_OPT,0,numLines);
    //    return requestFile("/var/log/messages",numLines);
  case LOG_REQUEST_ANITA:
    return requestJournalCtl(JOURNALCTL_OPT_COMM,ID_ACQD,numLines);
    //    return requestFile("/var/log/anita.log",numLines);
  case LOG_REQUEST_SECURITY:
    return requestFile("/var/log/security",numLines);
  case LOG_REQUEST_NTU:
    return requestFile("/var/log/ntu",numLines);
  case LOG_REQUEST_BOOT:
    return requestFile("/var/log/boot",numLines);
  case LOG_REQUEST_PROC_CPUINFO:
    return requestFile("/proc/cpuinfo",numLines);
  case LOG_REQUEST_PROC_DEVICES:
    return requestFile("/proc/devices",numLines);
  case LOG_REQUEST_PROC_DISKSTATS:
    return requestFile("/proc/diskstats",numLines);
  case LOG_REQUEST_PROC_FILESYSTEMS:
    return requestFile("/proc/filesystems",numLines);
  case LOG_REQUEST_PROC_INTERRUPTS:
    return requestFile("/proc/interrupts",numLines);
  case LOG_REQUEST_PROC_IOMEM:
    return requestFile("/proc/iomem",numLines);
  case LOG_REQUEST_PROC_IOPORTS:
    return requestFile("/proc/ioports",numLines);
  case LOG_REQUEST_PROC_LOADAVG:
    return requestFile("/proc/loadavg",numLines);
  case LOG_REQUEST_PROC_MEMINFO:
    return requestFile("/proc/meminfo",numLines);
  case LOG_REQUEST_PROC_MISC:
    return requestFile("/proc/misc",numLines);
  case LOG_REQUEST_PROC_MODULES:
    return requestFile("/proc/modules",numLines);
  case LOG_REQUEST_PROC_MOUNTS:
    return requestFile("/proc/mounts",numLines);
  case LOG_REQUEST_PROC_MTRR:
    return requestFile("/proc/mtrr",numLines);
  case LOG_REQUEST_PROC_PARTITIONS:
    return requestFile("/proc/partitions",numLines);
  case LOG_REQUEST_PROC_SCHEDDEBUG:
    return requestFile("/proc/sched_debug",numLines);
  case LOG_REQUEST_PROC_SCHEDSTAT:
    return requestFile("/proc/sched_stat",numLines);
  case LOG_REQUEST_PROC_STAT:
    return requestFile("/proc/stat",numLines);
  case LOG_REQUEST_PROC_SWAPS:
    return requestFile("/proc/swaps",numLines);
  case LOG_REQUEST_PROC_TIMERLIST:
    return requestFile("/proc/timer_list",numLines);
  case LOG_REQUEST_PROC_UPTIME:
    return requestFile("/proc/uptime",numLines);
  case LOG_REQUEST_PROC_VERSION:
    return requestFile("/proc/version",numLines);
  case LOG_REQUEST_PROC_VMCORE:
    return requestFile("/proc/vmcore",numLines);
  case LOG_REQUEST_PROC_VMSTAT:
    return requestFile("/proc/vmstat",numLines);
  case LOG_REQUEST_PROC_ZONEINFO:
    return requestFile("/proc/zoneinfo",numLines);
  default:
    return 0;
  }
  return 0;


}

int killDataPrograms()
{
  int numDataProgs=12;
  int dataProgs[12]={ID_MONITORD,
		     ID_HKD,
		     ID_GPSD,
		     ID_ARCHIVED,
		     ID_CALIBD,
		     ID_PRIORITIZERD,
		     ID_EVENTD,
		     ID_ACQD,
		     ID_PLAYBACKD,
		     ID_LOGWATCHD,
		     ID_OPENPORTD,
		     ID_NTUD};

  int index=0,sleepCount=0;

  //Kill the disk writing programs
  for(index=0;index<numDataProgs;index++) {
    int progMask=getIdMask(dataProgs[index]);
    killPrograms(progMask);
    if(index==0) sleep(1);
  }
  //Give them some time
  sleep(5);

  //Check they have actually gone away
  for(index=0;index<numDataProgs;index++) {
    int fileExists=0;
    do {
      FILE *test =fopen(getPidFile(dataProgs[index]),"r");
      if(test==NULL) fileExists=0;
      else {
	fclose(test);
	fileExists=1;
	sleepCount++;
	sleep(1);
	if(sleepCount>20) {
	  syslog(LOG_INFO,"%s not dead after %d seconds\n",getProgName(dataProgs[index]),sleepCount);
	  reallyKillPrograms(getIdMask(dataProgs[index]));
	  sleep(1);
	  unlink(getPidFile(dataProgs[index]));
	}
      }
    }
    while(fileExists && sleepCount<30);
  }
  system("killall -9 rsync");
  system("killall sleep");
  sleep(2);
  return 0;
}

int startDataPrograms() {


  int numDataProgs=12;
  int dataProgs[12]={ID_HKD,
		     ID_GPSD,
		     ID_ARCHIVED,
		     ID_CALIBD,
		     ID_MONITORD,
		     ID_PRIORITIZERD,
		     ID_EVENTD,
		     ID_PLAYBACKD,
		     ID_LOGWATCHD,
		     ID_NTUD,
		     ID_ACQD,
		     ID_OPENPORTD};

  int index=0;
  for(index=0;index<numDataProgs;index++) {
    int progMask=getIdMask(dataProgs[index]);
    startPrograms(progMask);
  }
  return 0;
}

int tryAndMountSatadrives()
{
  time_t rawtime;
  configModifyInt("anitaSoft.config","global","disableHelium1",0,&rawtime);
  sleep(1);
  configModifyInt("anitaSoft.config","global","disableHelium2",0,&rawtime);
  sleep(1);

  char theCommand[180];
  sprintf(theCommand,"sudo /etc/init.d/anitamount start");
  int retVal=system(theCommand);
  sleep(10);

  killDataPrograms();
  retVal=makeNewRunDirs();
  if(retVal==-1) return 0;
  time(&rawtime);
  startDataPrograms();
  return rawtime;
}

int executeRTLCommand(int command, unsigned char arg[2])
{
  time_t when;
  unsigned short argAsShort = arg[0] + ( arg[1] << 8);
  int num_rtls = NUM_RTLSDR;
  float gain[NUM_RTLSDR];
  int disabled[NUM_RTLSDR];
  switch(command)
  {
    case RTL_SET_TELEM_EVERY:
      configModifyInt("RTLd.config","rtl","telemEvery",(int) arg[0], &when);
      break;
    case RTL_SET_START_FREQUENCY:
      configModifyInt("RTLd.config","rtl","startFreq",argAsShort * 1e6, &when);
      break;
    case RTL_SET_END_FREQUENCY:
      configModifyInt("RTLd.config","rtl","endFreq",argAsShort * 1e6, &when);
      break;
    case RTL_SET_FREQUENCY_STEP:
      configModifyInt("RTLd.config","rtl","stepFreq",argAsShort*1e3, &when);
      break;
    case RTL_SET_GAIN:
      configLoad("RTLd.config","rtl");
      kvpGetFloatArray("gain", gain, &num_rtls);
      gain[ argAsShort & (~(0xffff << NBITS_FOR_RTL_INDEX)) ] = 0.1  * (argAsShort >> NBITS_FOR_RTL_INDEX);
      configModifyFloatArray("RTLd.config","rtl","gain",gain, NUM_RTLSDR, &when);
      break;
    case RTL_SET_DISABLED:
      configLoad("RTLd.config","rtl");
      kvpGetIntArray("disabled", disabled, &num_rtls);
      disabled[arg[0]] = arg[1];
      configModifyIntArray("RTLd.config","rtl","disabled",disabled, NUM_RTLSDR, &when);
      break;
    case RTL_SET_GRACEFUL_TIMEOUT:
      configModifyInt("RTLd.config","rtl","gracefulTimeout", argAsShort, &when);
      break;
    case RTL_SET_FAIL_TIMEOUT:
      configModifyInt("RTLd.config","rtl","failTimeout", argAsShort, &when);
      break;
    case RTL_SET_MAX_FAIL:
      configModifyInt("RTLd.config","rtl","failThreshold", argAsShort, &when);
      break;

    default:
      syslog(LOG_ERR,"Unknown RTLd command -- %d\n",command);
      return 0;
  }

  sendSignal(ID_RTLD, SIGUSR1);
  return when;

  return 0;
}

static void signed_char_arr_to_int_arr(int * vint, const char * vchar, int length)
{
  int i;
  for (i = 0; i < length; i++)
  {
    vint[i] = vchar[i];
  }
}



static void char_arr_to_int_arr(int * vint, const unsigned char * vchar, int length)
{
  int i;
  for (i = 0; i < length; i++)
  {
    vint[i] = vchar[i];
  }
}


int executeTuffCommand(int command, unsigned char arg[6])
{
  time_t when;
  int notch_array[2*NUM_TUFF_NOTCHES];
  int adjustArray[NUM_TUFF_NOTCHES];
  char notch, start_notch, end_notch;
  char rfcm, start_rfcm, end_rfcm;
  char chan, start_chan, end_chan;
  int capArray[NUM_TUFF_NOTCHES][NUM_RFCM][NUM_TUFFS_PER_RFCM];
  unsigned char cap;

  unsigned short cmd;

  switch(command)
  {

    case TUFF_SET_NOTCH:
       char_arr_to_int_arr(notch_array, arg, 2 * NUM_TUFF_NOTCHES);
       configModifyIntArray("Tuffd.config","notch","notchPhiSectors",notch_array,2*NUM_TUFF_NOTCHES, &when);
       break;

    case TUFF_SEND_RAW:
       cmd = arg[1] + (arg[2] << 8);
       doTuffRawCommand(arg[0] >> 1, arg[0] & 1 ,cmd,&when);
       break;

    case TUFF_SET_SLEEP_AMOUNT:
       configModifyInt("Tuffd.config","behavior","sleepAmount", arg[0], &when);
       break;
    case TUFF_SET_READ_TEMPERATURE:
       configModifyInt("Tuffd.config","behavior","readTemperature", arg[0], &when);
       break;
    case TUFF_SET_TELEM_EVERY:
       configModifyInt("Tuffd.config","behavior","telemEvery", arg[0], &when);
       break;
    case TUFF_SET_TELEM_AFTER_CHANGE:
       configModifyInt("Tuffd.config","behavior","telemAfterChange", arg[0], &when);
       break;
    case TUFF_ADJUST_ACCORDING_TO_HEADING:
       char_arr_to_int_arr(adjustArray, arg, NUM_TUFF_NOTCHES);
       configModifyIntArray("Tuffd.config","notch","adjustAccordingToHeading",adjustArray, NUM_TUFF_NOTCHES, &when);
       configModifyInt("GPSd.config","output", "saveAttitudeForTuff", (arg[0] || arg[1] || arg[2]) ? 1 : 0, &when);
       sendSignal(ID_GPSD,SIGUSR1);
       break;
    case TUFF_DEGREES_FROM_NORTH_TO_NOTCH:
       signed_char_arr_to_int_arr(adjustArray, (const char*) arg, NUM_TUFF_NOTCHES);
       configModifyIntArray("Tuffd.config","notch","tuffDegreesFromNorth",adjustArray, NUM_TUFF_NOTCHES, &when);
       break;
    case TUFF_SLOPE_THRESHOLD_TO_NOTCH_NEXT_SECTOR:
       char_arr_to_int_arr(adjustArray, arg, NUM_TUFF_NOTCHES);
       configModifyIntArray("Tuffd.config","notch","slopeThresholdToNotchNextSector",adjustArray, NUM_TUFF_NOTCHES, &when);
       break;
    case TUFF_MAX_HEADING_AGE:
       configModifyInt("Tuffd.config","notch","maxHeadingAge",arg[0], &when);
       break;
    case TUFF_SET_CAPS_ON_STARTUP:
       char_arr_to_int_arr(adjustArray, arg, NUM_TUFF_NOTCHES);
       configModifyIntArray("Tuffd.config","cap","setCapsOnStartup", adjustArray, NUM_TUFF_NOTCHES, &when);
       break;

    case TUFF_SET_CAP_VALUE:
       notch = (char) arg[0];
       start_notch = notch < 0 ? 0 : notch;
       end_notch = notch < 0 ? NUM_TUFF_NOTCHES : notch+1;
       rfcm = (char) arg[1];
       start_rfcm = rfcm < 0 ? 0: rfcm;
       end_rfcm = rfcm < 0 ? NUM_RFCM : rfcm+1;
       chan = (char) arg[2];
       start_chan = chan < 0 ? 0 : chan;
       end_chan = chan < 0 ? NUM_TUFFS_PER_RFCM : chan+1;
       cap = arg[3];
       for (notch = start_notch; notch < end_notch; notch++)
       {
         char key[128];
         int nread = NUM_TUFFS_PER_RFCM * NUM_RFCM;
         int kvpStatus;
         kvpReset();
         sprintf(key,"notch%dCaps", notch);
         kvpStatus = configLoad("Tuffd.config","cap");
         if (kvpStatus!= CONFIG_E_OK)
         {
           syslog(LOG_WARNING, "problem when trying to load Tuffd.config section cap %s", kvpErrorString(kvpStatus));
           return -1;
         }
         kvpStatus = kvpGetIntArray(key, &capArray[(int) notch][0][0], &nread) ;
         if (kvpStatus !=CONFIG_E_OK || nread != NUM_RFCM * NUM_TUFFS_PER_RFCM)
         {
           syslog(LOG_ERR, "Problem reading in %s aborting set cap value",key);
           return -1;
         }

         for (rfcm = start_rfcm; rfcm < end_rfcm; rfcm++)
         {
           for (chan = start_chan; chan < end_chan; chan++)
           {
             capArray[(int)notch][(int)rfcm][(int)chan]=cap;
           }
         }

         configModifyIntArray("Tuffd.config","cap",key, &capArray[(int)notch][0][0],NUM_RFCM*NUM_TUFFS_PER_RFCM, &when);
       }
       break;
    default:
      syslog(LOG_ERR,"Unknown Tuffd command -- %d\n",command);
      return 0;
  }

  sendSignal(ID_TUFFD, SIGUSR1);
  return when;
}



static int rawtuff_counter = 0;
int doTuffRawCommand(char rfcm, char stack, short cmd, time_t * tm )
{

  TuffRawCmd_t raw;
  char outName[FILENAME_MAX];
  time_t now = time(0);
  raw.requestedTime = now;
  raw.cmd = cmd;
  raw.irfcm = rfcm;
  raw.tuffStack = stack;
  sprintf(outName,"%s/tuff_rawcmd_%d.dat",TUFF_RAWCMD_DIR, rawtuff_counter++);
  writeStruct(&raw, outName, sizeof(TuffRawCmd_t));
  makeLink(outName,TUFF_RAWCMD_LINK_DIR);
  *tm = now;
  return 0;
}
