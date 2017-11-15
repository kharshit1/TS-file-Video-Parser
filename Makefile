CC=gcc
CFLAGS=-c 
LDFLAGS=
SOURCES=videoparser.c parserfunction.c
OBJECTS=$(SOURCES:.c=.o)
EXECUTABLE=videoparser

all: $(SOURCES) $(EXECUTABLE)
	
$(EXECUTABLE): $(OBJECTS) 
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@

.c.o:
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -rf *o videoparser
