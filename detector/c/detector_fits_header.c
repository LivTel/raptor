/* detector_fits_header.c
** Raptor Ninox-640 Infrared detector library : FITS header handling routines
*/
/**
 * Routines to look after lists of FITS headers to go into images.
 * @author Chris Mottram
 * @version $Id$
 */
/**
 * This hash define is needed before including source files give us POSIX.4/IEEE1003.1b-1993 prototypes.
 */
#define _POSIX_SOURCE 1
/**
 * This hash define is needed before including source files give us POSIX.4/IEEE1003.1b-1993 prototypes.
 */
#define _POSIX_C_SOURCE 199309L
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "fitsio.h"

#include "log_udp.h"

#include "detector_fits_header.h"
#include "detector_general.h"

/* hash defines */
/**
 * Maximum length of FITS header keyword (can go from column 1 to column 8 inclusive), plus a '\0' terminator.
 */
#define FITS_HEADER_KEYWORD_STRING_LENGTH (9)
/**
 * Maximum length of FITS header string value (can go from column 11 to column 80 inclusive), plus a '\0' terminator.
 */
#define FITS_HEADER_VALUE_STRING_LENGTH   (71)
/**
 * Maximum length of FITS header units (can go from column 10 to column 80 inclusive), plus a '\0' terminator.
 */
#define FITS_HEADER_UNITS_STRING_LENGTH   (72) 
/**
 * Maximum length of FITS header comment (can go from column 10 to column 80 inclusive), plus a '\0' terminator.
 */
#define FITS_HEADER_COMMENT_STRING_LENGTH (72) 

/* data types */
/**
 * Enumeration describing the type of data contained in a FITS header value.
 * <dl>
 * <dt>FITS_HEADER_TYPE_STRING</dt> <dd>String</dd>
 * <dt>FITS_HEADER_TYPE_INTEGER</dt> <dd>Integer</dd>
 * <dt>FITS_HEADER_TYPE_LONG_LONG_INTEGER</dt> <dd>Long Long Integer</dd>
 * <dt>FITS_HEADER_TYPE_FLOAT</dt> <dd>Floating point (double).</dd>
 * <dt>FITS_HEADER_TYPE_LOGICAL</dt> <dd>Boolean (integer having value 1 (TRUE) or 0 (FALSE).</dd>
 * </dl>
 */
enum Fits_Header_Type_Enum
{
	FITS_HEADER_TYPE_STRING,
	FITS_HEADER_TYPE_INTEGER,
	FITS_HEADER_TYPE_LONG_LONG_INTEGER,
	FITS_HEADER_TYPE_FLOAT,
	FITS_HEADER_TYPE_LOGICAL
};

/**
 * Structure containing information on a FITS header entry.
 * <dl>
 * <dt>Keyword</dt> <dd>Keyword string of length FITS_HEADER_KEYWORD_STRING_LENGTH.</dd>
 * <dt>Type</dt> <dd>Which value in the value union to use.</dd>
 * <dt>Value</dt> <dd>Union containing the following elements:
 *                    <ul>
 *                    <li>String (of length FITS_HEADER_VALUE_STRING_LENGTH).
 *                    <li>Int
 *                    <li>Long_Long_Int
 *                    <li>Float (of type double).
 *                    <li>Boolean (an integer, should be 0 (FALSE) or 1 (TRUE)).
 *                    </ul>
 *                </dd>
 * <dt>Units</dt> <dd>String of length FITS_HEADER_UNITS_STRING_LENGTH.</dd>
 * <dt>Comment</dt> <dd>String of length FITS_HEADER_COMMENT_STRING_LENGTH.</dd>
 * </dl>
 * @see #FITS_HEADER_VALUE_STRING_LENGTH
 * @see #FITS_HEADER_KEYWORD_STRING_LENGTH
 * @see #FITS_HEADER_UNITS_STRING_LENGTH
 * @see #FITS_HEADER_COMMENT_STRING_LENGTH
 */
struct Fits_Header_Card_Struct
{
	char Keyword[FITS_HEADER_KEYWORD_STRING_LENGTH]; /* columns 1-8 */
	enum Fits_Header_Type_Enum Type;
	union
	{
		char String[FITS_HEADER_VALUE_STRING_LENGTH]; /* columns 11-80 */
		int Int;
		long long int Long_Long_Int;
		double Float;
		int Boolean;
	} Value;
	char Units[FITS_HEADER_UNITS_STRING_LENGTH]; /* columns 10-80 */
	char Comment[FITS_HEADER_COMMENT_STRING_LENGTH]; /* columns 10-80  not already units columns */
};

/**
 * Structure defining the contents of a FITS header. Note the common basic FITS cards may not
 * be defined in this list.
 * <dl>
 * <dt>Card_List</dt> <dd>A list of Fits_Header_Card_Struct.</dd>
 * <dt>Card_Count</dt> <dd>The number of cards in the (reallocatable) list.</dd>
 * <dt>Allocated_Card_Count</dt> <dd>The amount of memory allocated in the Card_List pointer, 
 *     in terms of number of cards.</dd>
 * </dl>
 * @see #Fits_Header_Card_Struct
 */
struct Fits_Header_Struct
{
	struct Fits_Header_Card_Struct *Card_List;
	int Card_Count;
	int Allocated_Card_Count;
};

/* internal data */
/**
 * Revision Control System identifier.
 */
static char rcsid[] = "$Id$";

/**
 * Variable holding error code of last operation performed by the fits header routines.
 */
static int Fits_Header_Error_Number = 0;
/**
 * Local variable holding description of the last error that occured.
 * @see detector_general.html#DETECTOR_GENERAL_ERROR_STRING_LENGTH
 */
static char Fits_Header_Error_String[DETECTOR_GENERAL_ERROR_STRING_LENGTH] = "";
/**
 * This instance of the Fits_Header_Struct contains the FITS headers to be used for this detector.
 * @see #Fits_Header_Struct
 */
struct Fits_Header_Struct Fits_Header;

/* internal functions */
static int Fits_Header_Find_Card(char *keyword,int *found_index);
static int Fits_Header_Add_Card(struct Fits_Header_Card_Struct card);
static void Fits_Header_Uppercase(char *string);

/* ----------------------------------------------------------------------------
** 		external functions 
** ---------------------------------------------------------------------------- */
/**
 * Routine to initialise the fits header of cards. This does <b>not</b> free the card list memory.
 * @return The routine returns TRUE on success, and FALSE on failure. On failure, Fits_Header_Error_Number
 *         and Fits_Header_Error_String should be filled in with suitable values.
 * @see detector_general.html#Detector_General_Log
 * @see #Fits_Header_Error_Number
 * @see #Fits_Header_Error_String
 * @see #Fits_Header
 */
int Detector_Fits_Header_Initialise(void)
{
#if LOGGING > 1
	Detector_General_Log(LOG_VERBOSITY_INTERMEDIATE,"Detector_Fits_Header_Initialise: Started.");
#endif
	Fits_Header.Card_List = NULL;
	Fits_Header.Allocated_Card_Count = 0;
	Fits_Header.Card_Count = 0;
#if LOGGING > 1
	Detector_General_Log(LOG_VERBOSITY_INTERMEDIATE,"Detector_Fits_Header_Initialise: Finished.");
#endif
	return TRUE;
}

/**
 * Routine to clear the fits header of cards. This does <b>not</b> free the card list memory.
 * @return The routine returns TRUE on success, and FALSE on failure. On failure, Fits_Header_Error_Number
 *         and Fits_Header_Error_String should be filled in with suitable values.
 * @see detector_general.html#Detector_General_Log
 * @see #Fits_Header_Error_Number
 * @see #Fits_Header_Error_String
 * @see #Fits_Header
 */
int Detector_Fits_Header_Clear(void)
{
#if LOGGING > 1
	Detector_General_Log(LOG_VERBOSITY_INTERMEDIATE,"Detector_Fits_Header_Clear: Started.");
#endif
	/* reset number of cards, without resetting allocated cards */
	Fits_Header.Card_Count = 0;
#if LOGGING > 1
	Detector_General_Log(LOG_VERBOSITY_INTERMEDIATE,"Detector_Fits_Header_Clear: Finished.");
#endif
	return TRUE;
}

/**
 * Routine to delete the specified keyword from the FITS header. 
 * The list is not reallocated, Detector_Fits_Header_Free will eventually free the allocated memory.
 * The routine fails (returns FALSE) if a card with the specified keyword (uppercased) is NOT in the list.
 * @param keyword The keyword of the FITS header card to remove from the list.
 * @return The routine returns TRUE on success, and FALSE on failure. On failure, Fits_Header_Error_Number
 *         and Fits_Header_Error_String should be filled in with suitable values.
 * @see detector_general.html#Detector_General_Log
 * @see detector_general.html#Detector_General_Log_Format
 * @see #FITS_HEADER_KEYWORD_STRING_LENGTH
 * @see #Fits_Header_Error_Number
 * @see #Fits_Header_Error_String
 * @see #Fits_Header
 * @see #Fits_Header_Uppercase
 */
int Detector_Fits_Header_Delete(char *keyword)
{
	char uppercase_keyword[FITS_HEADER_KEYWORD_STRING_LENGTH];
	int found_index,index,done;

#if LOGGING > 1
	Detector_General_Log(LOG_VERBOSITY_INTERMEDIATE,"Detector_Fits_Header_Delete: Started.");
#endif
	/* check parameters */
	if(keyword == NULL)
	{
		Fits_Header_Error_Number = 4;
		sprintf(Fits_Header_Error_String,"Detector_Fits_Header_Delete:Keyword is NULL.");
		return FALSE;
	}
	if(strlen(keyword) > (FITS_HEADER_KEYWORD_STRING_LENGTH-1))
	{
		Fits_Header_Error_Number = 3;
		sprintf(Fits_Header_Error_String,"Detector_Fits_Header_Delete:Keyword '%s' is too long (%ld vs %d).",
			keyword,strlen(keyword),FITS_HEADER_KEYWORD_STRING_LENGTH);
		return FALSE;
	}
	/* uppercase keyword */
	strcpy(uppercase_keyword,keyword);
	Fits_Header_Uppercase(uppercase_keyword);
	/* find keyword in header */
	found_index = 0;
	done  = FALSE;
	while((found_index < Fits_Header.Card_Count) && (done == FALSE))
	{
		if(strcmp(Fits_Header.Card_List[found_index].Keyword,uppercase_keyword) == 0)
		{
			done = TRUE;
		}
		else
			found_index++;
	}
	/* if we failed to find the header, then error */
	if(done == FALSE)
	{
		Fits_Header_Error_Number = 5;
		sprintf(Fits_Header_Error_String,"Detector_Fits_Header_Delete:"
			"Failed to find Keyword '%s' in header of %d cards.",uppercase_keyword,Fits_Header.Card_Count);
		return FALSE;
	}
	/* if we found a card with this keyword, delete it. 
	** Move all cards beyond index down by one. */
	for(index=found_index; index < (Fits_Header.Card_Count-1); index++)
	{
		Fits_Header.Card_List[index] = Fits_Header.Card_List[index+1];
	}
	/* decrement headers in this list */
	Fits_Header.Card_Count--;
	/* leave memory allocated for reuse - this is deleted in CCD_Fits_Header_Free */
#if LOGGING > 1
	Detector_General_Log(LOG_VERBOSITY_INTERMEDIATE,"Detector_Fits_Header_Delete: Finished.");
#endif
	return TRUE;
}

/**
 * Routine to add a keyword with a string value to a FITS header.
 * @param keyword The keyword string, must be at least 1 character less in length than 
 *        FITS_HEADER_KEYWORD_STRING_LENGTH.
 * @param value The value string, which if longer than FITS_HEADER_VALUE_STRING_LENGTH-1 characters will be truncated.
 * @param comment The comment string, which if longer than FITS_HEADER_COMMENT_STRING_LENGTH-1 
 *        characters will be truncated. This parameter can also be NULL.
 * @return The routine returns TRUE on success, and FALSE on failure. On failure, Fits_Header_Error_Number
 *         and Fits_Header_Error_String should be filled in with suitable values.
 * @see #Fits_Header_Card_Struct
 * @see #Fits_Header_Type_Enum
 * @see #FITS_HEADER_KEYWORD_STRING_LENGTH
 * @see #FITS_HEADER_VALUE_STRING_LENGTH
 * @see #FITS_HEADER_COMMENT_STRING_LENGTH
 * @see #Fits_Header_Add_Card
 * @see detector_general.html#Detector_General_Log
 * @see detector_general.html#Detector_General_Log_Format
 * @see #Fits_Header_Error_Number
 * @see #Fits_Header_Error_String
 * @see #Fits_Header
 */
int Detector_Fits_Header_Add_String(char *keyword,char *value,char *comment)
{
	struct Fits_Header_Card_Struct card;

#if LOGGING > 1
	Detector_General_Log(LOG_VERBOSITY_INTERMEDIATE,"Detector_Fits_Header_Add_String: Started.");
#endif
	if(keyword == NULL)
	{
		Fits_Header_Error_Number = 6;
		sprintf(Fits_Header_Error_String,"Detector_Fits_Header_Add_String:"
			"Keyword is NULL.");
		return FALSE;
	}
	if(strlen(keyword) > (FITS_HEADER_KEYWORD_STRING_LENGTH-1))
	{
		Fits_Header_Error_Number = 7;
		sprintf(Fits_Header_Error_String,"Detector_Fits_Header_Add_String:"
			"Keyword %s (%lu) was too long.",keyword,strlen(keyword));
		return FALSE;
	}
	if(value == NULL)
	{
		Fits_Header_Error_Number = 8;
		sprintf(Fits_Header_Error_String,"Detector_Fits_Header_Add_String:"
			"Value string is NULL.");
		return FALSE;
	}
#if LOGGING > 5
	Detector_General_Log_Format(LOG_VERBOSITY_INTERMEDIATE,
			       "Detector_Fits_Header_Add_String: Adding keyword %s with value %s of length %d.",
			       keyword,value,strlen(value));
#endif
	strcpy(card.Keyword,keyword);
	card.Type = FITS_HEADER_TYPE_STRING;
	/* the value will be truncated to FITS_HEADER_VALUE_STRING_LENGTH-1 */
	strncpy(card.Value.String,value,FITS_HEADER_VALUE_STRING_LENGTH-1);
	card.Value.String[FITS_HEADER_VALUE_STRING_LENGTH-1] = '\0';
	/* the comment will be truncated to FITS_HEADER_COMMENT_STRING_LENGTH-1 */
	if(comment != NULL)
	{
		strncpy(card.Comment,comment,FITS_HEADER_COMMENT_STRING_LENGTH-1);
		card.Comment[FITS_HEADER_COMMENT_STRING_LENGTH-1] = '\0';
	}
	else
	{
		strcpy(card.Comment,"");
	}
	strcpy(card.Units,"");
	if(!Fits_Header_Add_Card(card))
		return FALSE;
#if LOGGING > 1
	Detector_General_Log(LOG_VERBOSITY_INTERMEDIATE,"Detector_Fits_Header_Add_String: Finished.");
#endif
	return TRUE;
}

/**
 * Routine to add a keyword with an integer value to a FITS header.
 * @param keyword The keyword string, must be at least 1 character less in length than 
 *        FITS_HEADER_KEYWORD_STRING_LENGTH.
 * @param value The integer value.
 * @param comment The comment string, which if longer than FITS_HEADER_COMMENT_STRING_LENGTH-1 
 *        characters will be truncated. This parameter can also be NULL.
 * @return The routine returns TRUE on success, and FALSE on failure. On failure, Fits_Header_Error_Number
 *         and Fits_Header_Error_String should be filled in with suitable values.
 * @see #Fits_Header_Card_Struct
 * @see #Fits_Header_Type_Enum
 * @see #FITS_HEADER_KEYWORD_STRING_LENGTH
 * @see #FITS_HEADER_COMMENT_STRING_LENGTH
 * @see #Fits_Header_Add_Card
 * @see detector_general.html#Detector_General_Log
 * @see #Fits_Header_Error_Number
 * @see #Fits_Header_Error_String
 * @see #Fits_Header
 */
int Detector_Fits_Header_Add_Int(char *keyword,int value,char *comment)
{
	struct Fits_Header_Card_Struct card;

#if LOGGING > 1
	Detector_General_Log(LOG_VERBOSITY_INTERMEDIATE,"Detector_Fits_Header_Add_Int: Started.");
#endif
	if(keyword == NULL)
	{
		Fits_Header_Error_Number = 9;
		sprintf(Fits_Header_Error_String,"Detector_Fits_Header_Add_Int:Keyword is NULL.");
		return FALSE;
	}
	if(strlen(keyword) > (FITS_HEADER_KEYWORD_STRING_LENGTH-1))
	{
		Fits_Header_Error_Number = 10;
		sprintf(Fits_Header_Error_String,"Detector_Fits_Header_Add_Int:"
			"Keyword %s (%lu) was too long.",keyword,strlen(keyword));
		return FALSE;
	}
	strcpy(card.Keyword,keyword);
	card.Type = FITS_HEADER_TYPE_INTEGER;
	card.Value.Int = value;
	/* the comment will be truncated to FITS_HEADER_COMMENT_STRING_LENGTH-1 */
	if(comment != NULL)
	{
		strncpy(card.Comment,comment,FITS_HEADER_COMMENT_STRING_LENGTH-1);
		card.Comment[FITS_HEADER_COMMENT_STRING_LENGTH-1] = '\0';
	}
	else
	{
		strcpy(card.Comment,"");
	}
	strcpy(card.Units,"");
	if(!Fits_Header_Add_Card(card))
		return FALSE;
#if LOGGING > 1
	Detector_General_Log(LOG_VERBOSITY_INTERMEDIATE,"Detector_Fits_Header_Add_Int: Finished.");
#endif
	return TRUE;
}

/**
 * Routine to add a keyword with an long long integer value to a FITS header.
 * @param keyword The keyword string, must be at least 1 character less in length than 
 *        FITS_HEADER_KEYWORD_STRING_LENGTH.
 * @param value The long long integer value.
 * @param comment The comment string, which if longer than FITS_HEADER_COMMENT_STRING_LENGTH-1 
 *        characters will be truncated. This parameter can also be NULL.
 * @return The routine returns TRUE on success, and FALSE on failure. On failure, Fits_Header_Error_Number
 *         and Fits_Header_Error_String should be filled in with suitable values.
 * @see #Fits_Header_Card_Struct
 * @see #Fits_Header_Type_Enum
 * @see #FITS_HEADER_KEYWORD_STRING_LENGTH
 * @see #FITS_HEADER_COMMENT_STRING_LENGTH
 * @see #Fits_Header_Add_Card
 * @see detector_general.html#Detector_General_Log
 * @see #Fits_Header_Error_Number
 * @see #Fits_Header_Error_String
 * @see #Fits_Header
 */
int Detector_Fits_Header_Add_Long_Long_Int(char *keyword,long long int value,char *comment)
{
	struct Fits_Header_Card_Struct card;

#if LOGGING > 1
	Detector_General_Log(LOG_VERBOSITY_INTERMEDIATE,"Detector_Fits_Header_Add_Long_Long_Int: Started.");
#endif
	if(keyword == NULL)
	{
		Fits_Header_Error_Number = 1;
		sprintf(Fits_Header_Error_String,"Detector_Fits_Header_Add_Long_Long_Int:Keyword is NULL.");
		return FALSE;
	}
	if(strlen(keyword) > (FITS_HEADER_KEYWORD_STRING_LENGTH-1))
	{
		Fits_Header_Error_Number = 2;
		sprintf(Fits_Header_Error_String,"Detector_Fits_Header_Add_Long_Long_Int:"
			"Keyword %s (%lu) was too long.",keyword,strlen(keyword));
		return FALSE;
	}
	strcpy(card.Keyword,keyword);
	card.Type = FITS_HEADER_TYPE_LONG_LONG_INTEGER;
	card.Value.Long_Long_Int = value;
	/* the comment will be truncated to FITS_HEADER_COMMENT_STRING_LENGTH-1 */
	if(comment != NULL)
	{
		strncpy(card.Comment,comment,FITS_HEADER_COMMENT_STRING_LENGTH-1);
		card.Comment[FITS_HEADER_COMMENT_STRING_LENGTH-1] = '\0';
	}
	else
	{
		strcpy(card.Comment,"");
	}
	strcpy(card.Units,"");
	if(!Fits_Header_Add_Card(card))
		return FALSE;
#if LOGGING > 1
	Detector_General_Log(LOG_VERBOSITY_INTERMEDIATE,"Detector_Fits_Header_Add_Long_Long_Int: Finished.");
#endif
	return TRUE;
}

/**
 * Routine to add a keyword with an float (double) value to a FITS header.
 * @param keyword The keyword string, must be at least 1 character less in length than 
 *        FITS_HEADER_KEYWORD_STRING_LENGTH.
 * @param value The float value of type double.
 * @param comment The comment string, which if longer than FITS_HEADER_COMMENT_STRING_LENGTH-1 
 *        characters will be truncated. This parameter can also be NULL.
 * @return The routine returns TRUE on success, and FALSE on failure. On failure, Fits_Header_Error_Number
 *         and Fits_Header_Error_String should be filled in with suitable values.
 * @see #Fits_Header_Type_Enum
 * @see #FITS_HEADER_KEYWORD_STRING_LENGTH
 * @see #FITS_HEADER_COMMENT_STRING_LENGTH
 * @see #Fits_Header_Add_Card
 * @see detector_general.html#Detector_General_Log
 * @see detector_general.html#Detector_General_Log_Format
 * @see #Fits_Header_Error_Number
 * @see #Fits_Header_Error_String
 * @see #Fits_Header
 */
int Detector_Fits_Header_Add_Float(char *keyword,double value,char *comment)
{
	struct Fits_Header_Card_Struct card;

#if LOGGING > 1
	Detector_General_Log(LOG_VERBOSITY_INTERMEDIATE,"Detector_Fits_Header_Add_Float: Started.");
#endif
	if(keyword == NULL)
	{
		Fits_Header_Error_Number = 11;
		sprintf(Fits_Header_Error_String,"Detector_Fits_Header_Add_Float:Keyword is NULL.");
		return FALSE;
	}
	if(strlen(keyword) > (FITS_HEADER_KEYWORD_STRING_LENGTH-1))
	{
		Fits_Header_Error_Number = 12;
		sprintf(Fits_Header_Error_String,"Detector_Fits_Header_Add_Float:"
			"Keyword %s (%lu) was too long.",keyword,strlen(keyword));
		return FALSE;
	}
	strcpy(card.Keyword,keyword);
	card.Type = FITS_HEADER_TYPE_FLOAT;
	card.Value.Float = value;
	/* the comment will be truncated to FITS_HEADER_COMMENT_STRING_LENGTH-1 */
	if(comment != NULL)
	{
		strncpy(card.Comment,comment,FITS_HEADER_COMMENT_STRING_LENGTH-1);
		card.Comment[FITS_HEADER_COMMENT_STRING_LENGTH-1] = '\0';
	}
	else
	{
		strcpy(card.Comment,"");
	}
	strcpy(card.Units,"");
	if(!Fits_Header_Add_Card(card))
		return FALSE;
#if LOGGING > 1
	Detector_General_Log(LOG_VERBOSITY_INTERMEDIATE,"Detector_Fits_Header_Add_Float: Finished.");
#endif
	return TRUE;
}

/**
 * Routine to add a keyword with a boolean value to a FITS header.
 * @param header The address of a Fits_Header_Struct structure to modify.
 * @param keyword The keyword string, must be at least 1 character less in length than 
 *        FITS_HEADER_KEYWORD_STRING_LENGTH.
 * @param value The boolean value, an integer with value 0 (FALSE) or 1 (TRUE).
 * @param comment The comment string, which if longer than FITS_HEADER_COMMENT_STRING_LENGTH-1 
 *        characters will be truncated. This parameter can also be NULL.
 * @return The routine returns TRUE on success, and FALSE on failure. On failure, Fits_Header_Error_Number
 *         and Fits_Header_Error_String should be filled in with suitable values.
 * @see #Fits_Header_Struct
 * @see #Fits_Header_Type_Enum
 * @see #FITS_HEADER_KEYWORD_STRING_LENGTH
 * @see #FITS_HEADER_COMMENT_STRING_LENGTH
 * @see #Fits_Header_Add_Card
 * @see detector_general.html#DETECTOR_IS_BOOLEAN
 * @see detector_general.html#Detector_General_Log
 * @see detector_general.html#Detector_General_Log_Format
 * @see #Fits_Header_Error_Number
 * @see #Fits_Header_Error_String
 * @see #Fits_Header
 */
int Detector_Fits_Header_Add_Logical(char *keyword,int value,char *comment)
{
	struct Fits_Header_Card_Struct card;

#if LOGGING > 1
	Detector_General_Log(LOG_VERBOSITY_INTERMEDIATE,"Detector_Fits_Header_Add_Logical: Started.");
#endif
	if(keyword == NULL)
	{
		Fits_Header_Error_Number = 13;
		sprintf(Fits_Header_Error_String,"Detector_Fits_Header_Add_Logical:Keyword is NULL.");
		return FALSE;
	}
	if(strlen(keyword) > (FITS_HEADER_KEYWORD_STRING_LENGTH-1))
	{
		Fits_Header_Error_Number = 14;
		sprintf(Fits_Header_Error_String,"Detector_Fits_Header_Add_Logical:"
			"Keyword %s (%lu) was too long.",keyword,strlen(keyword));
		return FALSE;
	}
	if(!DETECTOR_IS_BOOLEAN(value))
	{
		Fits_Header_Error_Number = 15;
		sprintf(Fits_Header_Error_String,"Detector_Fits_Header_Add_Logical:"
			"Value (%d) was not a boolean.",value);
		return FALSE;
	}
	strcpy(card.Keyword,keyword);
	card.Type = FITS_HEADER_TYPE_LOGICAL;
	card.Value.Boolean = value;
	/* the comment will be truncated to FITS_HEADER_COMMENT_STRING_LENGTH-1 */
	if(comment != NULL)
	{
		strncpy(card.Comment,comment,FITS_HEADER_COMMENT_STRING_LENGTH-1);
		card.Comment[FITS_HEADER_COMMENT_STRING_LENGTH-1] = '\0';
	}
	else
	{
		strcpy(card.Comment,"");
	}
	strcpy(card.Units,"");
	if(!Fits_Header_Add_Card(card))
		return FALSE;
#if LOGGING > 1
	Detector_General_Log(LOG_VERBOSITY_INTERMEDIATE,"Detector_Fits_Header_Add_Logical: Finished.");
#endif
	return TRUE;
}

/**
 * Routine to add a comment section to the header, of a FITS header whoose keyword/value pair already
 * exists in the list of FITS headers.
 * @param keyword The keyword string, must be at least 1 character less in length than 
 *        FITS_HEADER_KEYWORD_STRING_LENGTH. The keyword should already be present in the list of headers.
 * @param comment A string to put in the comment section of the FITS header.
 * @return The routine returns TRUE on success, and FALSE on failure. On failure, Fits_Header_Error_Number
 *         and Fits_Header_Error_String should be filled in with suitable values.
 * @see #Fits_Header_Struct
 * @see #Fits_Header_Find_Card
 * @see #Fits_Header
 * @see #FITS_HEADER_COMMENT_STRING_LENGTH
 */
int Detector_Fits_Header_Add_Comment(char *keyword,char *comment)
{
	int found_index;

	if(keyword == NULL)
	{
		Fits_Header_Error_Number = 22;
		sprintf(Fits_Header_Error_String,"Detector_Fits_Header_Add_Comment:Keyword is NULL.");
		return FALSE;
	}
	if(!Fits_Header_Find_Card(keyword,&found_index))
	{
		Fits_Header_Error_Number = 23;
		sprintf(Fits_Header_Error_String,
			"Detector_Fits_Header_Add_Comment:Failed to find keyword '%s' in header.",keyword);
		return FALSE;
	}
	/* the units will be truncated to FITS_HEADER_COMMENT_STRING_LENGTH-1 */
	strncpy(Fits_Header.Card_List[found_index].Comment,comment,FITS_HEADER_COMMENT_STRING_LENGTH-1);
	Fits_Header.Card_List[found_index].Comment[FITS_HEADER_COMMENT_STRING_LENGTH-1] = '\0';
	return TRUE;
}

/**
 * Routine to add a units section to the header comment, of a FITS header whoose keyword/value pair already
 * exists in the list of FITS headers.
 * @param keyword The keyword string, must be at least 1 character less in length than 
 *        FITS_HEADER_KEYWORD_STRING_LENGTH. The keyword should already be present in the list of headers.
 * @param units A string to put in the units section of the FITS header.
 * @return The routine returns TRUE on success, and FALSE on failure. On failure, Fits_Header_Error_Number
 *         and Fits_Header_Error_String should be filled in with suitable values.
 * @see #Fits_Header_Struct
 * @see #Fits_Header_Find_Card
 * @see #Fits_Header
 * @see #FITS_HEADER_UNITS_STRING_LENGTH
 */
int Detector_Fits_Header_Add_Units(char *keyword,char *units)
{
	int found_index;

	if(keyword == NULL)
	{
		Fits_Header_Error_Number = 16;
		sprintf(Fits_Header_Error_String,"Detector_Fits_Header_Add_Units:Keyword is NULL.");
		return FALSE;
	}
	if(!Fits_Header_Find_Card(keyword,&found_index))
	{
		Fits_Header_Error_Number = 19;
		sprintf(Fits_Header_Error_String,"Detector_Fits_Header_Add_Units:Failed to find keyword '%s' in header.",
			keyword);
		return FALSE;
	}
	/* the units will be truncated to FITS_HEADER_UNITS_STRING_LENGTH-1 */
	strncpy(Fits_Header.Card_List[found_index].Units,units,FITS_HEADER_UNITS_STRING_LENGTH-1);
	Fits_Header.Card_List[found_index].Units[FITS_HEADER_UNITS_STRING_LENGTH-1] = '\0';
	return TRUE;
}

/**
 * Routine to free an allocated FITS header list.
 * @param header The address of a Fits_Header_Struct structure to modify.
 * @return The routine returns TRUE on success, and FALSE on failure. On failure, Fits_Header_Error_Number
 *         and Fits_Header_Error_String should be filled in with suitable values.
 * @see detector_general.html#Detector_General_Log
 * @see detector_general.html#Detector_General_Log_Format
 * @see #Fits_Header_Error_Number
 * @see #Fits_Header_Error_String
 * @see #Fits_Header
 */
int Detector_Fits_Header_Free(void)
{
#if LOGGING > 1
	Detector_General_Log(LOG_VERBOSITY_INTERMEDIATE,"Detector_Fits_Header_Free: Started.");
#endif
	if(Fits_Header.Card_List != NULL)
		free(Fits_Header.Card_List);
	Fits_Header.Card_List = NULL;
	Fits_Header.Card_Count = 0;
	Fits_Header.Allocated_Card_Count = 0;
#if LOGGING > 1
	Detector_General_Log(LOG_VERBOSITY_INTERMEDIATE,"Detector_Fits_Header_Free: Finished.");
#endif
	return TRUE;
}


/**
 * Write the information contained in the header structure to the specified fitsfile.
 * @param fits_fp A previously created CFITSIO file pointer to write the headers into.
 * @return The routine returns TRUE on success, and FALSE on failure. On failure, Fits_Header_Error_Number
 *         and Fits_Header_Error_String should be filled in with suitable values.
 * @see detector_general.html#Detector_General_Log
 * @see detector_general.html#Detector_General_Log_Format
 * @see #Fits_Header_Error_Number
 * @see #Fits_Header_Error_String
 * @see #Fits_Header
 */
int Detector_Fits_Header_Write_To_Fits(fitsfile *fits_fp)
{
	int i,status,retval;
	char buff[32]; /* fits_get_errstatus returns 30 chars max */
	char *comment = NULL;

#if LOGGING > 1
	Detector_General_Log(LOG_VERBOSITY_INTERMEDIATE,"Detector_Fits_Header_Write_To_Fits: Started.");
#endif
	status = 0;
	for(i=0;i<Fits_Header.Card_Count;i++)
	{
		/* convert empty comment to NULL comment for CFITSIO */
		if(strlen(Fits_Header.Card_List[i].Comment) > 0)
			comment = Fits_Header.Card_List[i].Comment;
		else
			comment = NULL;
		switch(Fits_Header.Card_List[i].Type)
		{
			case FITS_HEADER_TYPE_STRING:
#if LOGGING > 9
				Detector_General_Log_Format(LOG_VERBOSITY_INTERMEDIATE,
						       "Detector_Fits_Header_Write_To_Fits:%d: %s = %s.",i,
						       Fits_Header.Card_List[i].Keyword,
						       Fits_Header.Card_List[i].Value.String);
#endif
				retval = fits_update_key(fits_fp,TSTRING,Fits_Header.Card_List[i].Keyword,
							 Fits_Header.Card_List[i].Value.String,
							 comment,&status);
				break;
			case FITS_HEADER_TYPE_INTEGER:
#if LOGGING > 9
				Detector_General_Log_Format(LOG_VERBOSITY_INTERMEDIATE,
						       "Detector_Fits_Header_Write_To_Fits:%d: %s = %d.",i,
						       Fits_Header.Card_List[i].Keyword,
						       Fits_Header.Card_List[i].Value.Int);
#endif
				retval = fits_update_key(fits_fp,TINT,Fits_Header.Card_List[i].Keyword,
							 &(Fits_Header.Card_List[i].Value.Int),
							 comment,&status);
				break;
			case FITS_HEADER_TYPE_LONG_LONG_INTEGER:
#if LOGGING > 9
				Detector_General_Log_Format(LOG_VERBOSITY_INTERMEDIATE,
						       "Detector_Fits_Header_Write_To_Fits:%d: %s = %ld.",i,
						       Fits_Header.Card_List[i].Keyword,
						       Fits_Header.Card_List[i].Value.Long_Long_Int);
#endif
				/* we could use TLONGLONG, or even TULONG */
				retval = fits_update_key(fits_fp,TLONG,Fits_Header.Card_List[i].Keyword,
							 &(Fits_Header.Card_List[i].Value.Long_Long_Int),
							 comment,&status);
				break;
			case FITS_HEADER_TYPE_FLOAT:
#if LOGGING > 9
				Detector_General_Log_Format(LOG_VERBOSITY_INTERMEDIATE,
						       "Detector_Fits_Header_Write_To_Fits:%d: %s = %.2f.",i,
						      Fits_Header.Card_List[i].Keyword,
						       Fits_Header.Card_List[i].Value.Float);
#endif
				retval = fits_update_key_fixdbl(fits_fp,Fits_Header.Card_List[i].Keyword,
								Fits_Header.Card_List[i].Value.Float,6,
								comment,&status);
				break;
			case FITS_HEADER_TYPE_LOGICAL:
#if LOGGING > 9
				Detector_General_Log_Format(LOG_VERBOSITY_INTERMEDIATE,
						       "Detector_Fits_Header_Write_To_Fits:%d: %s = %d.",i,
						       Fits_Header.Card_List[i].Keyword,
						       Fits_Header.Card_List[i].Value.Boolean);
#endif
				retval = fits_update_key(fits_fp,TLOGICAL,Fits_Header.Card_List[i].Keyword,
							 &(Fits_Header.Card_List[i].Value.Boolean),
							 comment,&status);
				break;
			default:
				Fits_Header_Error_Number = 17;
				sprintf(Fits_Header_Error_String,"Detector_Fits_Header_Write_To_Fits:"
					"Card %d (Keyword %s) has unknown type %d.",i,Fits_Header.Card_List[i].Keyword,
					Fits_Header.Card_List[i].Type);
				return FALSE;
				break;
		}
		if(retval)
		{
			fits_get_errstatus(status,buff);
			Fits_Header_Error_Number = 18;
			sprintf(Fits_Header_Error_String,"Detector_Fits_Header_Write_To_Fits:"
				"Failed to update %d %s (%s).",i,Fits_Header.Card_List[i].Keyword,buff);
			return FALSE;
		}
		/* units */
		if(strlen(Fits_Header.Card_List[i].Units) > 0)
		{
			retval = fits_write_key_unit(fits_fp,Fits_Header.Card_List[i].Keyword,
						     Fits_Header.Card_List[i].Units,&status);
			if(retval)
			{
				fits_get_errstatus(status,buff);
				Fits_Header_Error_Number = 21;
				sprintf(Fits_Header_Error_String,"Detector_Fits_Header_Write_To_Fits:"
				     "Failed to update FITS header Units for index='%d' keyword='%s' units='%s' (%s).",
					i,Fits_Header.Card_List[i].Keyword,Fits_Header.Card_List[i].Units,buff);
				return FALSE;
			}
		}
	}/* end for */
#if LOGGING > 1
	Detector_General_Log_Format(LOG_VERBOSITY_INTERMEDIATE,"Detector_Fits_Header_Write_To_Fits:Finished.");
#endif
	return TRUE;
}

/**
 * Get the current value of detector_fits_header's error number.
 * @return The current value of detector_fits_header's error number.
 * @see #Fits_Header_Error_Number
 */
int Detector_Fits_Header_Get_Error_Number(void)
{
	return Fits_Header_Error_Number;
}

/**
 * The error routine that reports any errors occuring in detector_fits_header in a standard way.
 * @see detector_general.html#Detector_General_Get_Current_Time_String
 * @see #Fits_Header_Error_Number
 * @see #Fits_Header_Error_String
 */
void Detector_Fits_Header_Error(void)
{
	char time_string[32];

	Detector_General_Get_Current_Time_String(time_string,32);
	/* if the error number is zero an error message has not been set up
	** This is in itself an error as we should not be calling this routine
	** without there being an error to display */
	if(Fits_Header_Error_Number == 0)
		sprintf(Fits_Header_Error_String,"Logic Error:No Error defined");
	fprintf(stderr,"%s Detector_Fits_Header:Error(%d) : %s\n",time_string,Fits_Header_Error_Number,
		Fits_Header_Error_String);
}

/**
 * The error routine that reports any errors occuring in detector_fits_header in a standard way. This routine places the
 * generated error string at the end of a passed in string argument.
 * @param error_string A string to put the generated error in. This string should be initialised before
 * being passed to this routine. The routine will try to concatenate it's error string onto the end
 * of any string already in existance.
 * @see detector_general.html#Detector_General_Get_Current_Time_String
 * @see #Fits_Header_Error_Number
 * @see #Fits_Header_Error_String
 */
void Detector_Fits_Header_Error_String(char *error_string)
{
	char time_string[32];

	Detector_General_Get_Current_Time_String(time_string,32);
	/* if the error number is zero an error message has not been set up
	** This is in itself an error as we should not be calling this routine
	** without there being an error to display */
	if(Fits_Header_Error_Number == 0)
		sprintf(Fits_Header_Error_String,"Logic Error:No Error defined");
	sprintf(error_string+strlen(error_string),"%s Detector_Fits_Header:Error(%d) : %s\n",time_string,
		Fits_Header_Error_Number,Fits_Header_Error_String);
}

/* ----------------------------------------------------------------------------
** 		internal functions 
** ---------------------------------------------------------------------------- */
/**
 * Find the FITS header card with a specified keyword. We uppercase the checked keyword, as all keywords in the list
 * are stored uppercase. This stops the case of having two cards differing by keyword case, which will be written
 * as the same keyword by CFITSIO.
 * @param keyword A string representing the keyword to search for.
 * @param found_index The address of an integer. If the routine returns true, found_index will contain
 *        the index in the header for FITS keyword keyword.
 * @return The routine returns TRUE if the keyword is found in the header, and FALSE if it is not.
 * @see #FITS_HEADER_KEYWORD_STRING_LENGTH
 * @see #Fits_Header.
 */
static int Fits_Header_Find_Card(char *keyword,int *found_index)
{
	char uppercase_keyword[FITS_HEADER_KEYWORD_STRING_LENGTH];
	int done;

	if(keyword == NULL)
	{
		return FALSE;
	}
	if(strlen(keyword) > (FITS_HEADER_KEYWORD_STRING_LENGTH-1))
	{
		return FALSE;
	}
	if(found_index == NULL)
	{
		return FALSE;
	}
	/* uppercase keyword */
	strcpy(uppercase_keyword,keyword);
	Fits_Header_Uppercase(uppercase_keyword);
	/* find keyword in header */
	(*found_index) = 0;
	done  = FALSE;
	while(((*found_index) < Fits_Header.Card_Count) && (done == FALSE))
	{
		if(strcmp(Fits_Header.Card_List[(*found_index)].Keyword,uppercase_keyword) == 0)
		{
			done = TRUE;
		}
		else
			(*found_index)++;
	}
	return done;
}

/**
 * Routine to add a card to the list. If the keyword already exists, that card will be updated with the new value,
 * otherwise a new card will be allocated (if necessary) and added to the list. The keyword is converted to all
 * uppercase. This matches the way CFITSIO handles keywords so we don't get a lower-case version of the same keyword
 * with a different value overwriting the upprcase one.
 * @param card The new card to add to the list. 
 *             If the (uppercase) keyword already exists, that card will be updated with the new value,
 *             otherwise a new card will be allocated (if necessary) and added to the list.
 * @return The routine returns TRUE on success, and FALSE on failure. On failure, Fits_Header_Error_Number
 *         and Fits_Header_Error_String should be filled in with suitable values.
 * @see detector_general.html#Detector_General_Log
 * @see detector_general.html#Detector_General_Log_Format
 * @see #Fits_Header_Error_Number
 * @see #Fits_Header_Error_String
 * @see #Fits_Header
 * @see #Fits_Header_Uppercase
 */
static int Fits_Header_Add_Card(struct Fits_Header_Card_Struct card)
{
	int index,done;

#if LOGGING > 1
	Detector_General_Log(LOG_VERBOSITY_VERBOSE,"Fits_Header_Add_Card: Started.");
#endif
	/* uppercase keyword */
	Fits_Header_Uppercase(card.Keyword);
	index = 0;
	done  = FALSE;
	while((index < Fits_Header.Card_Count) && (done == FALSE))
	{
		if(strcmp(Fits_Header.Card_List[index].Keyword,card.Keyword) == 0)
		{
			done = TRUE;
		}
		else
			index++;
	}
	/* if we found a card with this keyword, update it. */
	if(done == TRUE)
	{
#if LOGGING > 5
		Detector_General_Log_Format(LOG_VERBOSITY_VERY_VERBOSE,
					    "Fits_Header_Add_Card:Found keyword %s at index %d:Card updated.",
					    card.Keyword,index);
#endif
		Fits_Header.Card_List[index] = card;
		return TRUE;
	}
	/* add the card to the list */
	/* if we need to allocate more memory... */
	if((Fits_Header.Card_Count+1) >= Fits_Header.Allocated_Card_Count)
	{
		/* allocate card list memory to be the current card count +1 */
		if(Fits_Header.Card_List == NULL)
		{
			Fits_Header.Card_List = (struct Fits_Header_Card_Struct *)malloc((Fits_Header.Card_Count+1)*
									  sizeof(struct Fits_Header_Card_Struct));
		}
		else
		{
			Fits_Header.Card_List = (struct Fits_Header_Card_Struct *)realloc(Fits_Header.Card_List,
					    (Fits_Header.Card_Count+1)*sizeof(struct Fits_Header_Card_Struct));
		}
		if(Fits_Header.Card_List == NULL)
		{
			Fits_Header.Card_Count = 0;
			Fits_Header.Allocated_Card_Count = 0;
			Fits_Header_Error_Number = 20;
			sprintf(Fits_Header_Error_String,"Fits_Header_Add_Card:"
				"Failed to reallocate card list (%d,%d).",(Fits_Header.Card_Count+1),
				Fits_Header.Allocated_Card_Count);
			return FALSE;
		}
		/* upcate allocated card count */
		Fits_Header.Allocated_Card_Count = Fits_Header.Card_Count+1;
	}/* end if more memory needed */
	/* add the card to the list */
	Fits_Header.Card_List[Fits_Header.Card_Count] = card;
	Fits_Header.Card_Count++;
#if LOGGING > 1
	Detector_General_Log(LOG_VERBOSITY_VERBOSE,"Fits_Header_Add_Card: Finished.");
#endif
	return TRUE;

}

/**
 * Routine to uppercase the specified string.
 * @param string The string to uppercase.
 */
static void Fits_Header_Uppercase(char *string)
{
	int i;
	
	for(i = 0;i < strlen(string); i++)
	{
		string[i] = toupper(string[i]);
	}
}
