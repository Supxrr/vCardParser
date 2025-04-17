#include "VCHelpers.h"
#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>


// Evan Bucholski
// March 7th, 2025
// 1226299

bool parseSingleVCardLine(const char * line, Card * card, bool * foundBegin, bool * foundEnd, bool * doneFlag, bool * foundVersion){

    //check for begin and end here
    
    if(strncasecmp(line, "BEGIN:", 6) == 0){
        //skip any spaces after begin
        const char * beginPtr = line + 6;
        while(*beginPtr == ' ' || *beginPtr == '\t'){
            beginPtr++;
        }

        //now can see whats left is vcard
        if(strncasecmp(beginPtr, "VCARD", 5) == 0){
            *foundBegin = true;
            return true;
        } else {
            //invalid begin
            globalError = INV_CARD;
            return false;
        }
    }

    //check for end tag
    if(strncasecmp(line, "END:", 4) == 0){
        //skip any spaces after end
        const char * endPtr = line + 4;
        while(*endPtr == ' ' || *endPtr == '\t'){
            endPtr++;
        }

        //now can see whats left is vcard
        if(strncasecmp(endPtr, "VCARD", 5) == 0){
            if(!(*foundBegin)){
                //invalid end
                globalError = INV_CARD;
                return false;
            }
            *foundEnd = true;
            *doneFlag = true;
            return true;
        } else {
            //invalid end
            globalError = INV_CARD;
            return false;
        }
    }

    //if we havent found begin yet then treat as invalid
    if(!(*foundBegin)){
        return true;
    }

    //otherwise parse the property line
    //find colon
    char * colonPtr = strchr(line, ':');
    if(!colonPtr){
        globalError = INV_PROP;
        return false; //no colon means invalid
    }


    //copy the line into a buffer then split it at the colon
    char buffer[1024];
    strncpy(buffer, line, sizeof(buffer));
    buffer[sizeof(buffer) - 1] = '\0';
    char * colonPtr2 = strchr(buffer, ':');

    //check if the colon is at the end of the line
    if(colonPtr2 == NULL){
        globalError = INV_PROP;
        return false;
    }
    *colonPtr2 = '\0';


    char * propParams = buffer;

    //check length of propParams
    if(strlen(propParams) == 0){
        globalError = INV_PROP;
        return false;
    }
    char * propValue = colonPtr2 + 1;


    //before tokenzing values, trim the property value
    char * trimmedPropvalue = trimWhiteSpace(propValue);





    //create the new property with it
    Property * newProp = malloc(sizeof(Property));
    if(!newProp){
        globalError = OTHER_ERROR;
        free(trimmedPropvalue);
        return false;
    }

    //initialize the property
    newProp->parameters = initializeList(&parameterToString, &deleteParameter, &compareParameters);
    newProp->values = initializeList(&valueToString, &deleteValue, &compareValues);
    newProp->group = NULL;
    newProp->name = NULL;



    //parse the property name
    char * semicolonPtr = strchr(propParams, ';');
    if(semicolonPtr){
        *semicolonPtr = '\0';
        newProp->name = myStrDup(propParams);

        char * paramSubstring = semicolonPtr + 1;
        char * paramToken = strtok(paramSubstring, ";");

        while(paramToken){
            //each parameter must have a name=value pair
            char * equalsPtr = strchr(paramToken, '=');
            if(!equalsPtr){
                globalError = INV_PROP;
                free(newProp->name);
                freeList(newProp->parameters);
                freeList(newProp->values);
                free(newProp);
                free(trimmedPropvalue);
                return false;
            }
            *equalsPtr = '\0';

            //ensures neither parameter name or value is empty
            if(strlen(paramToken) == 0 || strlen(equalsPtr + 1) == 0){
                globalError = INV_PROP;
                free(newProp->name);
                freeList(newProp->parameters);
                freeList(newProp->values);
                free(newProp);
                free(trimmedPropvalue);
                return false;
            }
            Parameter * p = malloc(sizeof(Parameter));
            if(!p){
                globalError = OTHER_ERROR;
                free(newProp->name);
                freeList(newProp->parameters);
                freeList(newProp->values);
                free(newProp);
                free(trimmedPropvalue);
                return false;
            }

            //initialize the parameter
            p->name = myStrDup(paramToken);
            p->value = myStrDup(equalsPtr + 1);
            insertBack(newProp->parameters, p);
            paramToken = strtok(NULL, ";");
        }
    } else {
        //no parameter so simply duplicate the property name
        newProp->name = myStrDup(propParams);
    }


    //handle group names, if a dot is present then split into group and actual property name
    char * dotPtr = strchr(newProp->name, '.');
    if(dotPtr){
        *dotPtr = '\0';
        newProp->group = myStrDup(newProp->name);
        char * tempName = myStrDup(dotPtr + 1);
        free(newProp->name);
        newProp->name = tempName;
    }
    //ensure group is now null
    if(newProp->group == NULL){
        newProp->group = myStrDup("");
    }


    //trim the property name to remove leading and trailing whitespace
    char * trimmedName = trimWhiteSpace(newProp->name);
    free(newProp->name);
    newProp->name = trimmedName;
    


    //parse the property value, using the trimmed property value
    char tempValue[1024];
    strncpy(tempValue, propValue, sizeof(tempValue) - 1);
    tempValue[sizeof(tempValue) - 1] = '\0';
    free(trimmedPropvalue);




    //tokenize the values
    

    char * start = tempValue;
    char * pos = NULL;
    //for all properties including N process every token
    //the strpbrk function is better than strtok since it can handle multiple delimiters
    while((pos = strpbrk(start, ";")) != NULL){

        size_t tokenLength = pos - start;
        char * token = malloc(tokenLength + 1);
        if(!token){
            globalError = OTHER_ERROR;
            break;
        }

        strncpy(token, start, tokenLength);
        token[tokenLength] = '\0';

        //trim the token to preverse empty tokens as empty strings
        char * trimmedToken = trimWhiteSpace(token);
        free(token);

        //insert back even if it is empty
        insertBack(newProp->values, trimmedToken);
        start = pos + 1; 
    }
    //process the final token
    char * token = trimWhiteSpace(start);
    insertBack(newProp->values, token);



    //|| strlen((char*)getFromFront(newProp->values)) == 0


    //check at least one value exists
    if(getLength(newProp->values) == 0){
        globalError = INV_PROP;
        free(newProp->name);
        free(newProp->group);
        freeList(newProp->parameters);
        freeList(newProp->values);
        free(newProp);
        return false;
    }


    //special handling for certain property names like BDAY and ANNIVERSARY
    if(strcasecmp(newProp->name, "VERSION") == 0){
        
        //check if the version property is exactly one value and it equals 4.0
        if(getLength(newProp->values) < 1){
            globalError = INV_PROP;
            free(newProp->name);
            free(newProp->group);
            freeList(newProp->parameters);
            freeList(newProp->values);
            free(newProp);
            return false;
        }

        char * versionVal = (char*)getFromFront(newProp->values);
        //check if the version is 4.0
        if(strcmp(versionVal, "4.0") != 0){
            globalError = INV_CARD;
            free(newProp->name);
            free(newProp->group);
            freeList(newProp->parameters);
            freeList(newProp->values);
            free(newProp);
            return false;
        }

        *foundVersion = true;

        //version is valid
        //delete the property since it is not needed
        free(newProp->name);
        free(newProp->group);
        freeList(newProp->parameters);
        freeList(newProp->values);
        free(newProp);
        return true;

    } else if(strcasecmp(newProp->name, "FN") == 0){
        //first FN property is stored in card->fn
        card->fn = newProp;
    } else if(strcasecmp(newProp->name, "BDAY") == 0 || strcasecmp(newProp->name, "ANNIVERSARY") == 0){
        //for bday and anniversary, created a stub date time struct
        DateTime * dt = malloc(sizeof(DateTime));

        if(!dt){
            globalError = OTHER_ERROR;
            deleteProperty(newProp);
            return false;
        }
        dt->UTC = false;

        //get the property value as a string
        char * propVal = (char*)getFromFront(newProp->values);

        //parse the date time, if theres a T then its a date time

        char * tPtr = strchr(propVal, 'T');

        if(tPtr != NULL){
            dt->isText = false;
            //parse the date time
            size_t dateLength = tPtr - propVal;
            dt->date = malloc(dateLength + 1);

            if(dt->date){
                strncpy(dt->date, propVal, dateLength);
                dt->date[dateLength] = '\0';
            } else {
                dt->date = myStrDup("");
            }

            //parse the time
            dt->time = myStrDup(tPtr + 1);
            dt->text = myStrDup("");

        
        } else if(propVal && propVal[0] == '-' && propVal[1] == '-'){
            dt->isText = false;
            dt->date = myStrDup(propVal);
            dt->time = myStrDup("");
            dt->text = myStrDup("");
        } else if(propVal && !isdigit((unsigned char)propVal[0])){
            //otherwise the first character is not a digit
            dt->isText = true;
            dt->date = myStrDup("");
            dt->time = myStrDup("");
            dt->text = myStrDup(propVal);
        } else {
            //otherwise it is a date
            dt->isText = false;
            dt->date = myStrDup(propVal);
            dt->time = myStrDup("");
            dt->text = myStrDup("");
        }

        //set the date time in the card
        if(strcasecmp(newProp->name, "BDAY") == 0){
            card->birthday = dt;
        } else {
            card->anniversary = dt;
        }
        //delete the property since its info is in dt
        deleteProperty(newProp);
    } else {
        //insert the property into the optional properties list
        insertBack(card->optionalProperties, newProp);

    }
    return true;
}



bool validCRLF(const char * line){

    //will check if the /r and /n are at the end of the line
    size_t length = strlen(line);
    if(length < 2){
        return false;
    }

    //will check if the last two characters are /r and /n
    return (line[length - 2] == '\r' && line[length - 1] == '\n');
}


char * trimWhiteSpace(const char * str){
    //check if null
    if(str == NULL){
        return myStrDup("");
    }


    //skip leading whitespace, uses the isspace function to check for whitespace
    while(isspace((unsigned char)*str)){
        str++;
    }

    //then skip trailing whitespace
    if(*str == '\0'){
        return myStrDup("");
    }

    const char * end = str + strlen(str) - 1;

    //skip trailing whitespace
    while(end > str && isspace((unsigned char)*end)){
        end--;
    }
    size_t len = end - str + 1;

    //then copy the trimmed string
    char * trimmed = malloc(len + 1);
    if(trimmed){
        strncpy(trimmed, str, len);
        trimmed[len] = '\0';
    }

    return trimmed;
}



char* myStrDup(const char* str){
    if(str == NULL){
        return NULL;
    }

    size_t length = strlen(str) + 1;
    char * newStr = malloc(length);
    if(newStr){
        strcpy(newStr, str);
    }
    return newStr;
}

bool validFileExtension(const char * fileName){


    //if no filename is given then obviously it is invalid
    if(fileName == NULL){
        return false;
    }

    const char *dot = strrchr(fileName, '.');
    //if there is no dot in the filename then it is invalid
    if(dot == NULL){
        return false;
    }

    //if the file extension is not .vcf  or .vcard then it is invalid

    if(strcasecmp(dot, ".vcf") == 0 || strcasecmp(dot, ".vcard") == 0){
        return true;
    }
    return false;
}