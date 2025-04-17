#ifndef VCHELPERS_H
#define VCHELPERS_H


#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "VCParser.h"

//for the global error code to work properly.
extern VCardErrorCode globalError;


//helper function prototypes
bool validFileExtension(const char* fileName);
char* myStrDup(const char* str);
bool validCRLF(const char * line);
bool parseSingleVCardLine(const char * line, Card * card, bool * foundBegin, bool * foundEnd, bool * doneFlag, bool * foundVersion);
char * trimWhiteSpace(const char * str);


#endif