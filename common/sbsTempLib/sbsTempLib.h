/*! \file sbsTempLib.h
    \brief Some useful functions for reading out the temperatures from the XCR14 (nee SBS)
    
    Some useful functions for reading out the temperatures from the XCR14 (nee SBS)
   
   May 2013 r.nichol@ucl.ac.uk
*/


#ifndef SBSTEMPLIB_H
#define SBSTEMPLIB_H

#define NUM_SBS_TEMPS 6
///< Will give them more meaningfull enumerations soon
typedef enum {
  SBS_TEMP_0 = 0,
  SBS_TEMP_1 = 1,
  SBS_TEMP_2 = 2,
  SBS_TEMP_3 = 3,
  SBS_TEMP_4 = 4,
  SBS_TEMP_5 = 5
} SbsTempIndex_t;


const char *getSBSTemperatureLabel(SbsTempIndex_t index);
int readSBSTemperature(SbsTempIndex_t index);
int readSBSTemperatureFile(const char *tempFile);
int convertRawToAnita(int rawTempInmC) {return (rawTempInmC/100);}
float convertAnitaToHuman(int anitaTemp) {return (anitaTemp/10.);}
int readNTUTemps(SbsTempIndex_t index);


#endif /* SBSTEMPLIB_H */
