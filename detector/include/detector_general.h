/* detector_general.h */
#ifndef DETECTOR_GENERAL_H
#define DETECTOR_GENERAL_H


/* hash defines */
/**
 * TRUE is the value usually returned from routines to indicate success.
 */
#ifndef TRUE
#define TRUE 1
#endif
/**
 * FALSE is the value usually returned from routines to indicate failure.
 */
#ifndef FALSE
#define FALSE 0
#endif
/**
 * Macro to check whether the parameter is either TRUE or FALSE.
 * @see #TRUE
 * @see #FALSE
 */
#define DETECTOR_IS_BOOLEAN(value)	(((value) == TRUE)||((value) == FALSE))

/**
 * How long the error string is.
 */
#define DETECTOR_GENERAL_ERROR_STRING_LENGTH (1024)

/**
 * The number of nanoseconds in one second. A struct timespec has fields in nanoseconds.
 */
#define DETECTOR_GENERAL_ONE_SECOND_NS	     (1000000000)
/**
 * The number of nanoseconds in one millisecond. A struct timespec has fields in nanoseconds.
 */
#define DETECTOR_GENERAL_ONE_MILLISECOND_NS  (1000000)
/**
 * The number of milliseconds in one second.
 */
#define DETECTOR_GENERAL_ONE_SECOND_MS	     (1000)
/**
 * The number of nanoseconds in one microsecond.
 */
#define DETECTOR_GENERAL_ONE_MICROSECOND_NS  (1000)

#ifndef fdifftime
/**
 * Return double difference (in seconds) between two struct timespec's.
 * @param t0 A struct timespec.
 * @param t1 A struct timespec.
 * @return A double, in seconds, representing the time elapsed from t0 to t1.
 * @see #DETECTOR_GENERAL_ONE_SECOND_NS
 */
#define fdifftime(t1, t0) (((double)(((t1).tv_sec)-((t0).tv_sec))+(double)(((t1).tv_nsec)-((t0).tv_nsec))/DETECTOR_GENERAL_ONE_SECOND_NS))
#endif

/* external functions */
extern int Detector_General_Is_Error(void);
extern void Detector_General_Error(void);
extern void Detector_General_Error_To_String(char *error_string);
extern int Detector_General_Get_Error_Number(void);
extern void Detector_General_Get_Current_Time_String(char *time_string,int string_length);

extern void Detector_General_Log_Format(int level,char *format,...);
extern void Detector_General_Log(int level,char *string);
extern void Detector_General_Set_Log_Handler_Function(void (*log_fn)(int level,char *string));
extern void Detector_General_Set_Log_Filter_Function(int (*filter_fn)(int level,char *string));
extern void Detector_General_Log_Handler_Stdout(int level,char *string);
extern void Detector_General_Set_Log_Filter_Level(int level);
extern int Detector_General_Log_Filter_Level_Absolute(int level,char *string);
extern int Detector_General_Log_Filter_Level_Bitwise(int level,char *string);

extern int Detector_General_Mutex_Lock(void);
extern int Detector_General_Mutex_Unlock(void);

#endif
