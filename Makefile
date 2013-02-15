CFLAGS = -pedantic -ansi -Wall -Werror -ggdb -DDEBUG

HEADERS = consts.h types.h as.h table.h output.h parse.h
OBJECTS = as.o table.o parse.o output.o
EXECUTABLE = as

PRODUCTS = $(OBJECTS) $(EXECUTABLE)

all: $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)

$(OBJECTS): $(HEADERS)

.PHONY: clean

clean: 
	rm -f $(PRODUCTS)
