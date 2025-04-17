CC = gcc
CFLAGS = -Wall -std=c11 -g
LDFLAGS = -L.
INC = include/
SRC = src/
BIN = bin/
OBJDIR = src/

PARSER_OBJS = $(OBJDIR)/VCParser.o $(OBJDIR)/VCHelpers.o $(OBJDIR)/LinkedListAPI.o $(OBJDIR)/vcwrapper.o


all: parser

# -------- Build the parser shared library --------
parser: $(PARSER_OBJS)
	rm -rf $(BIN)/libvcparser.so
	$(CC) -shared -o $(BIN)libvcparser.so $(PARSER_OBJS)

# -------- Build the tester executable --------
tester: tester.o $(PARSER_OBJS)
	$(CC) $(CFLAGS) -o tester tester.o $(PARSER_OBJS)

tester.o: $(SRC)tester.c $(INC)VCParser.h
	$(CC) -I$(INC) $(CFLAGS) -c $(SRC)tester.c -o tester.o



# -------- Build the writeCard tester executable --------
writeCard: writeCard.o $(PARSER_OBJS)
	$(CC) $(CFLAGS) -o writeCard writeCard.o $(PARSER_OBJS)

writeCard.o: $(SRC)writeCard.c $(INC)VCParser.h
	$(CC) -I$(INC) $(CFLAGS) -c $(SRC)writeCard.c -o writeCard.o


# -------- Build the wrapper object files --------
$(OBJDIR)/vcwrapper.o: $(SRC)vcwrapper.c $(INC)VCParser.h $(INC)VCHelpers.h $(INC)LinkedListAPI.h
	$(CC) -I$(INC) $(CFLAGS) -c -fpic $(SRC)vcwrapper.c -o $(OBJDIR)/vcwrapper.o


# -------- Build the object files --------

$(OBJDIR)/VCParser.o: $(SRC)VCParser.c $(INC)VCParser.h $(INC)LinkedListAPI.h
	$(CC) -I$(INC) $(CFLAGS) -c -fpic $(SRC)VCParser.c -o $(OBJDIR)/VCParser.o

$(OBJDIR)/VCHelpers.o: $(SRC)VCHelpers.c $(INC)VCHelpers.h $(INC)LinkedListAPI.h
	$(CC) -I$(INC) $(CFLAGS) -c -fpic $(SRC)VCHelpers.c -o $(OBJDIR)/VCHelpers.o

$(OBJDIR)/LinkedListAPI.o: $(SRC)LinkedListAPI.c $(INC)LinkedListAPI.h
	$(CC) -I$(INC) $(CFLAGS) -c -fpic $(SRC)LinkedListAPI.c -o $(OBJDIR)/LinkedListAPI.o

# -------- Clean --------
clean:
	rm -f tester tester.o
	rm -f writeCard writeCard.o
	rm -rf $(BIN)/libvcparser.so
	rm -f $(OBJDIR)/*.o
