/* detector_temperature.h */
#ifndef DETECTOR_TEMPERATURE_H
#define DETECTOR_TEMPERATURE_H
extern int Detector_Temperature_Initialise(int adc_zeroC,int adc_fortyC,int dac_zeroC,int dac_fortyC);

extern int Detector_Temperature_Get_Error_Number(void);
extern void Detector_Temperature_Error(void);
extern void Detector_Temperature_Error_String(char *error_string);

#endif
