/* detector_fits_header.h */
#ifndef DETECTOR_FITS_HEADER_H
#define DETECTOR_FITS_HEADER_H

/* for fitsfile declaration */
#include "fitsio.h"

extern int Detector_Fits_Header_Initialise(void);
extern int Detector_Fits_Header_Clear(void);
extern int Detector_Fits_Header_Delete(char *keyword);
extern int Detector_Fits_Header_Add_String(char *keyword,char *value,char *comment);
extern int Detector_Fits_Header_Add_Int(char *keyword,int value,char *comment);
extern int Detector_Fits_Header_Add_Long_Long_Int(char *keyword,long long int value,char *comment);
extern int Detector_Fits_Header_Add_Float(char *keyword,double value,char *comment);
extern int Detector_Fits_Header_Add_Logical(char *keyword,int value,char *comment);
extern int Detector_Fits_Header_Add_Comment(char *keyword,char *comment);
extern int Detector_Fits_Header_Add_Units(char *keyword,char *units);

extern int Detector_Fits_Header_Free(void);

extern int Detector_Fits_Header_Write_To_Fits(fitsfile *fits_fp);

extern int Detector_Fits_Header_Get_Error_Number(void);
extern void Detector_Fits_Header_Error(void);
extern void Detector_Fits_Header_Error_String(char *error_string);


#endif
