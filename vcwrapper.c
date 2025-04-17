#include "VCParser.h"
#include "VCHelpers.h"
#include <stdio.h>
#include <stdlib.h>



//this wrapper function creates a card object from a given file
//converts it to a string, deletes the card, and returns the string

char * getCardSummary(const char * fileName){

    if(fileName == NULL){
        //return copy of error message
        return myStrDup("Error: File name is NULL");
    }

    Card * card = NULL;

    VCardErrorCode error = createCard((char *)fileName, &card);

    if(error != OK || card == NULL){
        char errorMsg[100];
        sprintf(errorMsg, "Error: %s", errorToString(error));
        return myStrDup(errorMsg);
    }

    //get a string represtation of the card
    char * summary = cardToString(card);

    //clean up the card object
    deleteCard(card);

    //return the summary string
    return summary;
}


VCardErrorCode updateCardFN(Card * card, const char * newFN){

    if(card == NULL || newFN == NULL){
        return INV_CARD;
    }

    //check that newFN is not an empty string
    if(strlen(newFN) == 0){
        return INV_PROP;
    }

    
    //if the FN property is not found, create a new one
    if(card->fn == NULL){
        Property * newProp = malloc(sizeof(Property));
        if(newProp == NULL){
            return OTHER_ERROR;
        }
        newProp->name = myStrDup("FN");
        newProp->group = myStrDup("");
        newProp->parameters = initializeList(&parameterToString, &deleteParameter, &compareParameters);
        newProp->values = initializeList(&valueToString, &deleteValue, &compareValues);
        char * fnValue = myStrDup(newFN);
        insertBack(newProp->values, fnValue);
        card->fn = newProp;
    } else {
        //if FN exists, update the first value
        if(card->fn->values != NULL && card->fn->values->head != NULL){
            free(card->fn->values->head->data);
            card->fn->values->head->data = myStrDup(newFN);
        } else {
            //create a new values list if needed
            if(card->fn->values == NULL){
                card->fn->values = initializeList(&valueToString, &deleteValue, &compareValues);
            }
            insertBack(card->fn->values, myStrDup(newFN));
        }
    }
    return OK;  
}

//helper function to update the FN property of a card that exists
VCardErrorCode updateCard(const char * fileName, const char * newFN){

    if(fileName == NULL || newFN == NULL){
        return INV_FILE;
    }

    Card * card = NULL;

    VCardErrorCode error = createCard((char *)fileName, &card);

    if(error != OK || card == NULL){
        return error;
    }

    //update the FN property
    error = updateCardFN(card, newFN);
    if(error != OK){
        deleteCard(card);
        return error;
    }


    //validate the updated card
    error = validateCard(card);
    if(error != OK){
        deleteCard(card);
        return error;
    }

    //write the updated card to a file
    error = writeCard((char *)fileName, card);

    //clean up the card object
    deleteCard(card);

    return error;
}

//wrapper function to create a new card
VCardErrorCode createNewCard(const char * fileName, const char * fnValue){

    if(fileName == NULL || fnValue == NULL){
        return INV_FILE;
    }

    //check if the file already exists
    FILE * file = fopen(fileName, "r");
    if(file){
        fclose(file);
        return INV_FILE;
    }


    //allocate a new card obj
    Card * newCard = malloc(sizeof(Card));
    if(!newCard){
        return OTHER_ERROR;
    }
    //initialize the card object
    newCard->fn = NULL;
    newCard->optionalProperties = initializeList(&propertyToString, &deleteProperty, &compareProperties);
    newCard->anniversary = NULL;
    newCard->birthday = NULL;

    //build an FN property, only allowed to edit filename and FN
    Property * fnProp = malloc(sizeof(Property));
    if(!fnProp){
        free(newCard);
        return OTHER_ERROR;
    }
    fnProp->name = myStrDup("FN");
    fnProp->group = myStrDup("");
    fnProp->parameters = initializeList(&parameterToString, &deleteParameter, &compareParameters);

    //initialize the values list for FN property
    fnProp->values = initializeList(&valueToString, &deleteValue, &compareValues);

    //create a new values list
    char * theValue = myStrDup(fnValue);
    insertBack(fnProp->values, theValue);
    newCard->fn = fnProp;


    //validate the new card
    VCardErrorCode error = validateCard(newCard);
    if(error != OK){
        deleteCard(newCard);
        return error;
    }

    //write the card to disk
    VCardErrorCode writeError = writeCard((char *)fileName, newCard);

    //clean up the card object
    deleteCard(newCard);

    return writeError;

}