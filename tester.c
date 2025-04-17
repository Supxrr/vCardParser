#include <stdio.h>
#include <string.h>
#include "LinkedListAPI.h"
#include "VCParser.h"

/*
    THIS IS THE FILE WHERE ALL MY FUNCTIONS WILL GO THAT WILL PARSE THE VCARD FILE
    FOR TESTING PURPOSES


   
*/




int main(int argc, char *argv[]){


    //if the user does not enter the correct amount of arguments
    if(argc != 2){
        fprintf(stderr, "Usage %s <vCard file> \n", argv[0]);
        return 1;
    }


    Card *myCard = NULL;
    VCardErrorCode error = createCard(argv[1], &myCard);

    printf("\n=== CARD TEST ===\n");
    printf("Parsing: %s\n", argv[1]);

    if(error != OK){
        fprintf(stderr, "Error: %d\n", error);
        return 1;
    }

    printf("Card parsed successfully\n\n");

    char * cardString = cardToString(myCard);
    printf("%s\n", cardString);
    free(cardString);

    deleteCard(myCard);


    return 0;
}
