#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdbool.h>
#include <strings.h>

#include "VCParser.h"
#include "LinkedListAPI.h"
#include "VCHelpers.h"




//GLOBAL ERROR THAT WILL SWITCH TO HANDLE ERRORS
VCardErrorCode globalError = OK;



/*
    THIS IS THE FILE WHERE ALL MY FUNCTIONS WILL GO THAT WILL PARSE THE VCARD FILE
*/

/*

    This function will take in a file name and a pointer to a card object and will create a card object
    based on the information in the file.  The function will return an error code based on the success of the function
    
*/
VCardErrorCode createCard(char* fileName, Card** obj){

    //check for parameters first
    if(fileName == NULL || obj == NULL){
        return INV_FILE;
    }


    //check if the file extension is valid
    if(!validFileExtension(fileName)){
        *obj = NULL;
        return INV_FILE;
    }


    //now open the file
    FILE *fp = fopen(fileName, "rb");
    if(fp == NULL){
        *obj = NULL;
        return INV_FILE;
    }


    //now allocate memory for a new card obj
    Card * card = malloc(sizeof(Card));
    if(card == NULL){
        fclose(fp);
        *obj = NULL;
        return OTHER_ERROR;   // FOR NOW
    }

    //initialize the card object
    card->fn = NULL;
    card->optionalProperties = initializeList(&propertyToString, &deleteProperty, &compareProperties);

    //set to NULL for now because of garbage values
    card->birthday = NULL;
    card->anniversary = NULL;


    //variable setup for parsing
    bool foundBegin = false;
    bool foundEnd = false;
    bool done = false;
    bool foundVersion = false; //set true once we find version property



    //store unfolded lines
    char unfoldedBuffer[2048];
    unfoldedBuffer[0] = '\0';


    //read lines until we find END or run out of file
    while(!done){

        char rawBuffer[1024];
        if(!fgets(rawBuffer, sizeof(rawBuffer), fp)){
            //EOF or read error occured
            break;
        }

        if(!validCRLF(rawBuffer)){
            //invalid line ending
            deleteCard(card);
            fclose(fp);
            *obj = NULL;
            return INV_CARD;
        }


        //remove CRLF
        size_t rawLength = strlen(rawBuffer);
        rawBuffer[rawLength - 2] = '\0'; //remove CRLF


        //check if the line is a continuation line
        if(rawBuffer[0] == ' ' || rawBuffer[0] == '\t'){
            //the line continues into the previous line, skip the whitespace char
            char * toAppend = rawBuffer + 1;


            //append the remainder to the accumulated buffer
            if(strlen(unfoldedBuffer) + strlen(toAppend) + 1 < sizeof(unfoldedBuffer)){
                strcat(unfoldedBuffer, toAppend);
            } else {
                //buffer overflow
                deleteCard(card);
                fclose(fp);
                *obj = NULL;
                return INV_FILE;
            }
            
        } else {
            //this would be a fresh line
            //check if we have any unfolded lines

            if(unfoldedBuffer[0] != '\0'){
                //parse the unfolded line
                if(!parseSingleVCardLine(unfoldedBuffer, card, &foundBegin, &foundEnd, &done, &foundVersion)){
                    //invalid line
                    deleteCard(card);
                    fclose(fp);
                    *obj = NULL;
                    return globalError;
                }

                //clear the unfolded buffer
                unfoldedBuffer[0] = '\0';
            }

            //parse the fresh line
            char * trimmedLine = trimWhiteSpace(rawBuffer);
            strncpy(unfoldedBuffer, rawBuffer, sizeof(unfoldedBuffer) - 1);
            unfoldedBuffer[sizeof(unfoldedBuffer) - 1] = '\0';
            free(trimmedLine);
        }


    }
    //check if we have any unfolded lines left after the loop

    if(!done && unfoldedBuffer[0] != '\0'){
        //parse the unfolded line
        if(!parseSingleVCardLine(unfoldedBuffer, card, &foundBegin, &foundEnd, &done, &foundVersion)){
            //invalid line
            deleteCard(card);
            fclose(fp);
            *obj = NULL;
            return globalError;
        }
    }


    fclose(fp);
    //close and check if we found end
    if(!foundEnd || !foundBegin || (card->fn == NULL) || !foundVersion){
        deleteCard(card);
        *obj = NULL;
        return INV_CARD;
    } 

    *obj = card;
    return OK;

}

/*
    This function will delete a card object and free all the memory that was allocated for it
*/
void deleteCard(Card* obj){

    //will be able to delete any card properties by calling the other functions in this one

    //check if the card object is null
    if(obj == NULL){
        return;
    }

    //free the FN property
    if(obj->fn != NULL){
        deleteProperty(obj->fn);
        obj->fn = NULL;
    }

    //free the optional properties
    if(obj->optionalProperties != NULL){
        freeList(obj->optionalProperties);
        obj->optionalProperties = NULL;
    }

    //if there is date time then free it too
    if(obj->birthday != NULL){
        deleteDate(obj->birthday);
        obj->birthday = NULL;
    }



    if(obj->anniversary != NULL){
        deleteDate(obj->anniversary);
        obj->anniversary = NULL;
    }


    //then free the card itself
    free(obj);
    
}


/*
    This function will take in a card object and return a string representation of the card object
*/

//did not end up using param and value as I was able to use the propertyToString function to get the values and debug, still created them though
char* cardToString(const Card* obj){
    
    //if it is null then return something saying
    if(obj == NULL){
        char * errorString = malloc(16);
        strcpy(errorString, "Card is NULL");
        return errorString;
    }

    //allocate memory for the string, random amount but should be enough
    char * cardString = malloc(4096);

    //ensure empty string
    cardString[0] = '\0';
    
    //add the FN
    if(obj->fn != NULL){
        char * fnString = propertyToString(obj->fn);
        strcat(cardString, "Full Name:\n");
        strcat(cardString, fnString);
        strcat(cardString, "\n\n");
        free(fnString);
    } else {
        strcat(cardString, "Full Name: NULL\n\n");
    }

    //then add the optional properties if there is any
    strcat(cardString, "\n---Optional Properties---\n");
    void * elem;
    ListIterator iter = createIterator(obj->optionalProperties);
    while((elem = nextElement(&iter)) != NULL){
        Property * prop = (Property*)elem;
        char * propString = propertyToString(prop);
        strcat(cardString, propString);
        strcat(cardString, "\n");
        free(propString);
    }

    //print bday and anniversary if it is present
    if(obj->birthday){
        char * bdayString = dateToString(obj->birthday);
        strcat(cardString, "Birthday:\n");
        strcat(cardString, bdayString);
        strcat(cardString, "\n\n");
        free(bdayString);
    } else {
        strcat(cardString, "Birthday: NULL\n\n");
    }
    

    if(obj->anniversary){
        char * annString = dateToString(obj->anniversary);
        strcat(cardString, "Anniversary:\n");
        strcat(cardString, annString);
        strcat(cardString, "\n\n");
        free(annString);
    } else {
        strcat(cardString, "Anniversary: NULL\n\n");
    }


    strcat(cardString, "\n---End of Card---\n");

    return cardString;
}

/*
    This function will take in an error code and return a string representation of the error code
*/
char* errorToString(VCardErrorCode err){
    char * error = NULL;

    //run a switch statement to determine the error

    switch(err){
        case OK:
            error = myStrDup("OK");
            break;
        case INV_FILE:
            error = myStrDup("Invalid File");
            break;
        case INV_CARD:
            error = myStrDup("Invalid Card");
            break;
        case INV_PROP:
            error = myStrDup("Invalid Property");
            break;
        case INV_DT:
            error = myStrDup("Invalid Date Time");
            break;
        case WRITE_ERROR:
            error = myStrDup("Write Error");
            break;
        case OTHER_ERROR:
            error = myStrDup("Other Error");
            break;
        default:
            error = myStrDup("Invalid error code");
            break;
    }

    return error;
}

// NOW THE LIST HELPER FUNCTIONS





//PROPERTY RELATED FUNCTIONS
/*
    This function will delete a property object and free all the memory that was allocated for it
*/
void deleteProperty(void* toBeDeleted){


    //check if the property is null
    if(toBeDeleted == NULL){
        return;
    }


    //cast the property to a property object
    Property * prop = (Property*)toBeDeleted;

    //free the name
    if(prop->name != NULL){
        free(prop->name);
        prop->name = NULL;
    }

    //free the group
    if(prop->group != NULL){
        free(prop->group);
        prop->group = NULL;
    }

    //free the list of parameters if there is any
    if(prop->parameters != NULL){
        freeList(prop->parameters);
        prop->parameters = NULL;
    }

    //free the list of values if there is any
    if(prop->values != NULL){
        freeList(prop->values);
        prop->values = NULL;
    }

    //free the property itself
    free(prop);

}

/*
    This function will compare two property objects and return an integer based on the comparison when it gets used..
*/
int compareProperties(const void* first,const void* second){
    return 0;
}

/*
    This function will take in a property object and return a string representation of the property object
*/
char* propertyToString(void* prop){
    
    //if the property is null then return an error string
    if(prop == NULL){
        return myStrDup("Property is NULL");
    }

    //cast the property to a property object
    Property * propData = (Property*)prop;

    //allocate memory for the string
    char * propString = malloc(1024);
    propString[0] = '\0';

    //add name
    if(propData->name){
        strcat(propString, "Name: ");
        strcat(propString, propData->name);
        strcat(propString, "\n");
    }


    //print parameters if any
    if(propData->parameters != NULL && getLength(propData->parameters) > 0){
        strcat(propString, "Parameters:\n");
        void * elem;
        ListIterator iter = createIterator(propData->parameters);
        while((elem = nextElement(&iter)) != NULL){
            Parameter * param = (Parameter*)elem;
            strcat(propString, "     - ");
            strcat(propString, param->name);
            strcat(propString, " = ");
            strcat(propString, param->value);
            strcat(propString, "\n");
        }
    }


    //print values if any
    strcat(propString, "Values:\n");
    void * valElem;
    ListIterator valIter = createIterator(propData->values);
    while((valElem = nextElement(&valIter)) != NULL){
        char * value = (char*)valElem;
        strcat(propString, "     - ");
        strcat(propString, value);
        strcat(propString, "\n");
    }

    return propString;
}


//PARAMETER RELATED FUNCTIONS

/*
    This function will delete a parameter object and free all the memory that was allocated for it
*/
void deleteParameter(void* toBeDeleted){

    //check if the parameter is null
    if(toBeDeleted == NULL){
        return;
    }

    //cast the parameter to a parameter object
    Parameter * param = (Parameter*)toBeDeleted;

    //free the name
    if(param->name != NULL){
        free(param->name);
        param->name = NULL;
    }

    //free the value
    if(param->value != NULL){
        free(param->value);
        param->value = NULL;
    }

    //free the parameter itself
    free(param);

}

/*
    This function will compare two parameter objects and return an integer based on the comparison
*/
int compareParameters(const void* first,const void* second){
    return 0;
}

/*
    This function will take in a parameter object and return a string representation of the parameter object
*/
char* parameterToString(void* param){
    
    //if the parameter is null then return an error string
    if(param == NULL){
        return myStrDup("Parameter is NULL");
    }

    
    //cast the parameter to a parameter object
    Parameter * paramData = (Parameter*)param;

    //determine length for the output string
    size_t length = 7 + strlen(paramData->name) + 9 + strlen(paramData->value) + 1;

    //allocate memory
    char * paramString = malloc(length);
    if(paramString == NULL){
        return myStrDup("Memory Allocation Error");
    }

    //format string with the name and value
    sprintf(paramString, "Name: %s, Value: %s", paramData->name, paramData->value);


    return paramString;

}


//VALUE RELATED FUNCTIONS

/*
    This function will delete a value object and free all the memory that was allocated for it
*/
void deleteValue(void* toBeDeleted){

    //check if the value is null
    if(toBeDeleted == NULL){
        return;
    }

    //cast the value to a value object
    char * val = (char*)toBeDeleted;

    //free the value
    free(val);

}

/*
    This function will compare two value objects and return an integer based on the comparison
*/
int compareValues(const void* first,const void* second){
    return 0;
}

/*
    This function will take in a value object and return a string representation of the value object
*/
char* valueToString(void* val){
    
    //if the value is null then return an error string
    if(val == NULL){
        return myStrDup("NULL");
    }

    //otherwise cast to a char* then return
    return myStrDup((char*)val);

}


//DATE RELATED FUNCTIONS

/*
    This function will delete a date object and free all the memory that was allocated for it
*/
void deleteDate(void* toBeDeleted){

    //check if the date is null
    if(toBeDeleted == NULL){
        return;
    }

    //cast the date to a date object
    DateTime * date = (DateTime*)toBeDeleted;

    //free the date time
    if(date->date != NULL){
        free(date->date);
        date->date = NULL;
    }

    //free the time
    if(date->time != NULL){
        free(date->time);
        date->time = NULL;
    }

    //free the text
    if(date->text != NULL){
        free(date->text);
        date->text = NULL;
    }

    //free the date itself
    free(date);
}

/*
    This function will compare two date objects and return an integer based on the comparison
*/
int compareDates(const void* first,const void* second){
    return 0;
}

/*
    This function will take in a date object and return a string representation of the date object
*/
char* dateToString(void* date){
    //check if date is null
    if(date == NULL){
        return myStrDup("Date is NULL");
    }

    DateTime * dateData = (DateTime*)date;

    //if datetime is marked as test then return its text field
    if(dateData->isText){
        return myStrDup(dateData->text);
    } else {
        //otherwise if its not test then combine date and time
        size_t length = strlen(dateData->date) + strlen(dateData->time) + 10;
        char * result = malloc(length);

        if(result == NULL){
            return myStrDup("Memory Allocation Error");
        }


        //copy the date and time to the result
        strcpy(result, dateData->date);


        //if there is a time then add it to the result with the T
        if(strlen(dateData->time) > 0){
            strcat(result, "T");
            strcat(result, dateData->time);
        }

        return result;
    }
}


//ASSIGNMENT 2 FUNCTIONS

//this function will take the card object and write it to a file
VCardErrorCode writeCard(const char* fileName, const Card* obj) {

    //check if the file name and card object are null
    if(fileName == NULL || obj == NULL){
        return WRITE_ERROR;
    }

    //check if the file extension is valid
    if(!validFileExtension(fileName)){
        return WRITE_ERROR;
    }

    //open the file
    FILE *fp = fopen(fileName, "w");
    if(fp == NULL){
        return WRITE_ERROR;
    }

    //write the card to the file
    fprintf(fp, "BEGIN:VCARD\r\n");


    //write the version line which is always 4.0
    fprintf(fp, "VERSION:4.0\r\n");

    //write the FN property
    if(obj->fn != NULL){
        //assume that FN's value is always stored as the first value in the list
        if(obj->fn->values != NULL && obj->fn->values->head != NULL){
            fprintf(fp, "FN:%s\r\n", (char*)obj->fn->values->head->data);
        }
    }

    //write the optional properties
    if(obj->optionalProperties != NULL){
        void * elem;
        ListIterator iter = createIterator(obj->optionalProperties);
        while((elem = nextElement(&iter)) != NULL){
            Property * prop = (Property*)elem;
            
            //if the property belongs to a group, print group and a dot
            if(prop->group != NULL && strlen(prop->group) > 0){
                fprintf(fp, "%s.", prop->group);
            }

            //print the property name
            fprintf(fp, "%s", prop->name);
            

            //print any parameters
            if(prop->parameters != NULL && getLength(prop->parameters) > 0){
                void * paramElem;
                ListIterator paramIter = createIterator(prop->parameters);
                while((paramElem = nextElement(&paramIter)) != NULL){
                    Parameter * param = (Parameter*)paramElem;
                    fprintf(fp, ";%s=%s", param->name, param->value);
                }
            }

            //print the colon the start the value list
            fprintf(fp, ":");
            
            //print property values separated by commas
            if(prop->values != NULL && getLength(prop->values) > 0){
                void * valElem;
                ListIterator valIter = createIterator(prop->values);
                while((valElem = nextElement(&valIter)) != NULL){
                    char * value = (char*)valElem;
                    fprintf(fp, "%s", value);
                    if(valIter.current != NULL){
                        fprintf(fp, ";");
                    }
                }
            }
            fprintf(fp, "\r\n");
        }
    }

    //write the birthday
    if(obj->birthday != NULL){
        char * bdayString = dateToString(obj->birthday);
        fprintf(fp, "BDAY:%s\r\n", bdayString);
        free(bdayString);
    }

    //write the anniversary
    if(obj->anniversary != NULL){
        char * annString = dateToString(obj->anniversary);
        fprintf(fp, "ANNIVERSARY:%s\r\n", annString);
        free(annString);
    }

    //write the end
    fprintf(fp, "END:VCARD\r\n");

    //close the file
    fclose(fp);

    return OK;

}


//will expand on the card validation by checking the properties and their values
VCardErrorCode validateCard(const Card* obj){

    //check for null card obj
    if(obj == NULL){
        return INV_CARD;
    }

    //verify there is the basic ones needed
    if(obj->fn == NULL || obj->optionalProperties == NULL){
        return INV_CARD;
    }

    //check the FN property
    if(obj->fn->name == NULL || getLength(obj->fn->values) < 1){
        return INV_CARD;
    }

    //check the version property in the optional properties
    void * element;


    //iterate through the optional properties to find the version property, if it is found then return INV_CARD
    ListIterator iterator = createIterator(obj->optionalProperties);
    while((element = nextElement(&iterator)) != NULL){
        Property * prop = (Property*)element;
        if(strcasecmp(prop->name, "VERSION") == 0){
            return INV_CARD;
        }
    }
    

    //validate all properties are from RFC 6350
    const char * validProperties[] = {
        "FN", "N", "BDAY", "ANNIVERSARY", "GENDER", "ADR", "TEL", "EMAIL",
        "IMPP", "LANG", "TZ", "GEO", "TITLE", "ROLE", "LOGO", "ORG",
        "MEMBER", "RELATED", "CATEGORIES", "NOTE", "PRODID", "REV", "SOUND",
        "UID", "CLIENTPIDMAP", "URL", "KEY", "FBURL", "CALADRURI", "CALURI"
    };

    //iterate through the optional properties
    void * elem;
    ListIterator iter = createIterator(obj->optionalProperties);

    //bool to check if FN is found
    //bool foundN = false;
    int nCount = 0;

    while((elem = nextElement(&iter)) != NULL){
        Property * prop = (Property*)elem;

        //ensure name is not null or empty
        if(prop->name == NULL || strlen(prop->name) == 0){
            return INV_PROP;
        }

        //ensure property name is valid
        bool validProp = false;
        for(int i = 0; i < sizeof(validProperties) / sizeof(validProperties[0]); i++){
            if(strcasecmp(validProperties[i], prop->name) == 0){
                validProp = true;
                break;
            }
        }
        if(!validProp){
            return INV_PROP;
        }

        //ensure paramaters and value lists are not null
        if(prop->parameters == NULL || prop->values == NULL){
            return INV_PROP;
        }

        //check for duplicate version property
        if(strcasecmp(prop->name, "VERSION") == 0){
            return INV_PROP;
        }

        //special cases for cardinality
        if(strcasecmp(prop->name, "N") == 0){
            nCount++;
            if(getLength(prop->values) != 5){
                return INV_PROP;
            }
        }

        //check for empty values
        //some properties can have empty values
        const char * mustHaveValues[] = {
            "FN", "N", "TEL", "EMAIL", "IMPP", "LANG", "TZ", "GEO", 
            "TITLE", "ROLE", "ORG", "MEMBER", "RELATED", "URL"
        };

        //check if the property requires a value
        bool requiresValue = false;
        for(int i = 0; i < sizeof(mustHaveValues) / sizeof(mustHaveValues[0]); i++){
            if(strcasecmp(mustHaveValues[i], prop->name) == 0){
                requiresValue = true;
                break;
            }
        }

        //check if the property has values
        if(requiresValue && getLength(prop->values) == 0){
            return INV_PROP;
        }
    }

    if(nCount > 1){
        return INV_PROP;
    }


    
    
    //validate BDAY and ANNIVERSARY which is the datetime struct
    if(obj->birthday){
        if(obj->birthday->isText){
            //text based datetime must have a non empty text field
            if(strlen(obj->birthday->text) == 0 || strlen(obj->birthday->date) > 0 || strlen(obj->birthday->time) > 0 || obj->birthday->UTC){
                return INV_DT;
            }
        } else {
            //ensure valid date and time formats for example HHMMSS
            if((strlen(obj->birthday->date) != 8 && strlen(obj->birthday->date) != 0) ||
               (strlen(obj->birthday->time) != 6 && strlen(obj->birthday->time) != 0) ||
               (strlen(obj->birthday->text) > 0)){
                return INV_DT;
            }
        }
    }

    //validate anniversary
    if(obj->anniversary){
        if(obj->anniversary->isText){
            //text based datetime must have a non empty text field
            if(strlen(obj->anniversary->text) == 0 || strlen(obj->anniversary->date) > 0 || strlen(obj->anniversary->time) > 0 || obj->anniversary->UTC){
                return INV_DT;
            }
        } else {
            //ensure valid date and time formats for example HHMMSS
            if((strlen(obj->anniversary->date) != 8 && strlen(obj->anniversary->date) != 0) ||
               (strlen(obj->anniversary->time) != 6 && strlen(obj->anniversary->time) != 0) ||
               (strlen(obj->anniversary->text) > 0)){
                return INV_DT;
            }
        }
    }

    //check optional properties for invalid bday/anniversary placement
    void * elem2;
    ListIterator iter2 = createIterator(obj->optionalProperties);
    while((elem2 = nextElement(&iter2)) != NULL){
        Property * prop = (Property*)elem2;
        
        if(strcasecmp(prop->name, "VERSION") == 0){
            return INV_CARD;
        }

        if(strcasecmp(prop->name, "BDAY") == 0 || strcasecmp(prop->name, "ANNIVERSARY") == 0){
            return INV_DT;
        } 
    }


    
    //if all checks pass then return OK for valid card
    return OK;
}










