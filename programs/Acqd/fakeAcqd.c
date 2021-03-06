/*! \file fakeAcqd.c
    \brief Fake version of the Acqd program (fake as it makes up the data)
    
    Going to be used in the testing of the inter-process communication,
    and will form the basis of the real Acqd program, when we have some
    hardware.
    August 2004  rjn@mps.ohio-state.edu
*/
#define _GNU_SOURCE
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

#include "includes/anitaFlight.h"
#include "configLib/configLib.h"
//#include "socketLib/socketLib.h"
#include "kvpLib/keyValuePair.h"
#include "utilLib/utilLib.h"
#include "linkWatchLib/linkWatchLib.h"
#include "includes/anitaStructures.h"


#define DEFAULT_C3PO 133000000

int hadASyncSlip();
void fakeEvent(AnitaEventFull_t *theEventPtr);
int getEvent(AnitaEventFull_t *theEventPtr, const char *lastEventNumberFile);
void writeEventAndMakeLink(const char *theEventDir, const char *theLinkDir, AnitaEventFull_t *theEventPtr);

int waitForFakeTrigger();

/* Useful global thing-a-me-bobs */

int main (int argc, char *argv[])
{
  //    int retVal;


    

    /* Log stuff */
    char *progName=basename(argv[0]);

    /*Event object*/
    AnitaEventFull_t theEvent;

    
    /* Setup log */
    setlogmask(LOG_UPTO(LOG_DEBUG));
    openlog (progName, LOG_PID, ANITA_LOG_FACILITY) ;

  

    makeDirectories("/tmp/anita/trigAcqd/link");
    makeDirectories(ACQD_EVENT_DIR);
    makeDirectories(ACQD_EVENT_LINK_DIR);

    printf("\nThe Event Dir is: %s\n\n",ACQD_EVENT_DIR);

  
/* Main event getting loop */
    while(1) {
	if(getEvent(&theEvent,LAST_EVENT_NUMBER_FILE)) {
	  //	    syslog(LOG_INFO,"Got new event: %d, time %d",
	  //		   theEvent.header.eventNumber,theEvent.header.unixTime);
	    writeEventAndMakeLink(ACQD_EVENT_DIR,ACQD_EVENT_LINK_DIR,&theEvent);
	}
	else usleep(1);


    }
}

int getEvent(AnitaEventFull_t *theEventPtr, const char *lastEventNumberFile) {
    /*Returns 1 if it gets an event, 0 otrherwise */

    int retVal;
    static int firstTime=1;
    static int eventNumber=0;
    /* This is just to get the lastEventNumber in case of program restart. */
    FILE *pFile;
    if(firstTime) {
	pFile = fopen (lastEventNumberFile, "r");
	if(pFile == NULL) {
	    syslog (LOG_ERR,"fopen: %s ---  %s\n",strerror(errno),
		    lastEventNumberFile);
	}
	else {	    	    
	    retVal=fscanf(pFile,"%d",&eventNumber);
	    if(retVal<0) {
		syslog (LOG_ERR,"fscanff: %s ---  %s\n",strerror(errno),
			lastEventNumberFile);
	    }
	    fclose (pFile);
	}
	printf("The last event number is %d\n",eventNumber);
	firstTime=0;
    }

    /* At the moment have to get fake (random) waveforms  */
    if(waitForFakeTrigger()) {
	fakeEvent(theEventPtr);
	printf("Event %d, Time %d %d\n",theEventPtr->header.eventNumber,
	       theEventPtr->header.unixTime,theEventPtr->header.unixTimeUs);
	/* Need to increment eventNumber and write out the last eventNumber to 
	   a file. */
	eventNumber++;
	theEventPtr->header.eventNumber=eventNumber;
	
	if(eventNumber%100) {
	  pFile = fopen (lastEventNumberFile, "w");
	  if(pFile == NULL) {
	    syslog (LOG_ERR,"fopen: %s ---  %s\n",strerror(errno),
		    lastEventNumberFile);
	  }
	  else {
	    retVal=fprintf(pFile,"%d\n",eventNumber);
	    if(retVal<0) {
	      syslog (LOG_ERR,"fprintf: %s ---  %s\n",strerror(errno),
		      lastEventNumberFile);
	    }
	    fclose (pFile);
	  }
	}
	return 1;
    }
    return 0;
}

int waitForFakeTrigger()
{
    struct dirent **triggerList;
    int numTriggers,count;
    char filename[FILENAME_MAX];
    while(1) {
	numTriggers=getListofLinks("/tmp/anita/trigAcqd/link",&triggerList);
	if(numTriggers>0) {
	    sprintf(filename,"/tmp/anita/trigAcqd/%s",triggerList[0]->d_name);
	    removeFile(filename);
	    sprintf(filename,"/tmp/anita/trigAcqd/link/%s",triggerList[0]->d_name);
	    removeFile(filename);
	    for(count=0;count<numTriggers;count++)
		free(triggerList[count]);
	    free(triggerList);	    
	    return numTriggers;
	}
	usleep(10);
    }
}


void fakeEvent(AnitaEventFull_t *theEventPtr)
{
  static unsigned int ppsOffset=0;
  int chan,samp,value;
  struct timeval timeStruct;
  if(hadASyncSlip())
    ppsOffset=0;
  gettimeofday(&timeStruct,NULL);
  theEventPtr->header.unixTime=timeStruct.tv_sec;
  theEventPtr->header.unixTimeUs=timeStruct.tv_usec;
  double fracTime=((double)timeStruct.tv_usec)/1e6;
  theEventPtr->header.turfio.trigTime=(int)(fracTime*DEFAULT_C3PO);
  if(ppsOffset==0)
    ppsOffset=timeStruct.tv_sec;
  theEventPtr->header.turfio.ppsNum=timeStruct.tv_sec-ppsOffset;

//    theEventPtr->header.unixTime=time(NULL);
/*     theEventPtr->header.trigDelay=64; */
/*     theEventPtr->header.numChannels=NUM_DIGITZED_CHANNELS; */
/*     theEventPtr->header.numSamples=256; */
/*     theEventPtr->header.Dtype=1; */
/*     theEventPtr->header.trigCouple=0; */
/*     theEventPtr->header.trigSlope=0; */
/*     theEventPtr->header.trigSource=-1; */
/*     theEventPtr->header.trigLevel=10; 	    sprintf(filename,"/tmp/anita/trigAcqd/%s",triggerList[0]->d_name);
*/
/*     theEventPtr->header.sampInt=50; /\* sample interval in units of 10 ps *\/ */
/*     theEventPtr->header.holdOff=0; /\* number of 100ms intervals to hold between triggers *\/ */
/*     theEventPtr->header.clocktype=0; /\* start/stop external=4, external=2, continuous ext.=1, internal=0 *\/ */

    /* Channel Stuff */
    for(chan=0;chan<NUM_DIGITZED_CHANNELS;chan++) {
/* 	theEventPtr->body.channel[chan].header.ch_id=chan;  */
/* 	theEventPtr->body.channel[chan].header.ch_fs=1000; /\* -1V to +1V *\/ */
/* 	theEventPtr->body.channel[chan].header.ch_offs=2048; /\*1/2 of 12bits *\/ */
/* 	theEventPtr->body.channel[chan].header.ch_couple=0; */
/* 	theEventPtr->body.channel[chan].header.ch_bw=0; */
/* 	theEventPtr->body.channel[chan].header.ch_mean=0; */
/* 	theEventPtr->body.channel[chan].header.ch_sdev=0; */

	for(samp=0;samp<MAX_NUMBER_SAMPLES;samp++) {
	    value=(short)4096.0*drand48();
	    theEventPtr->body.channel[chan].data[samp]=value;
	}
    }
    
}

void writeEventAndMakeLink(const char *theEventDir, const char *theLinkDir, AnitaEventFull_t *theEventPtr)
{
    char theFilename[FILENAME_MAX];
    //   int retVal;
    AnitaEventHeader_t *theHeader=&(theEventPtr->header);
    AnitaEventBody_t *theBody=&(theEventPtr->body);


    sprintf(theFilename,"%s/ev_%d.dat",theEventDir,
	    theEventPtr->header.eventNumber);
    writeStruct(theBody,theFilename,sizeof(AnitaEventBody_t));  
      
    sprintf(theFilename,"%s/hd_%d.dat",theEventDir,
	    theEventPtr->header.eventNumber);
    writeStruct(theHeader,theFilename,sizeof(AnitaEventHeader_t));

    /* Make links, not sure what to do with return value here */
    makeLink(theFilename,theLinkDir);
}


int hadASyncSlip() {
    double randomNumber=drand48();
    if(randomNumber<0.05) return 1;
    return 0;

}
