//this file will be used to test the writeCard function


//Evan Bucholski
//1226299
//March 7th, 2025

#include <stdio.h>
#include <string.h>
#include "VCParser.h"
#include "LinkedListAPI.h"
#include "VCHelpers.h"

int main(int argc, char *argv[]){

    //check that the user has entered the correct amount of arguments
    //has to be the input vcard file and the desired output file
    if(argc != 3){
        fprintf(stderr, "Usage %s <vCard file> <output file>\n", argv[0]);
        return 1;
    }

    //create a card object
    Card *myCard = NULL;
    VCardErrorCode error = createCard(argv[1], &myCard);
    if(error != OK){
        fprintf(stderr, "Error: %d\n", error);
        return 1;
    }

    //call writecard to write the card to the output file
    error = writeCard(argv[2], myCard);
    if(error != OK){
        fprintf(stderr, "Error: %d\n", error);
        deleteCard(myCard);
        return 1;
    }

    printf("Card written to: %s\n", argv[2]);

    deleteCard(myCard);
    return 0;

}