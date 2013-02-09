CFLAGS = -pedantic -ansi -Wall -Werror -ggdb -DDEBUG

OBJECTS = as.o table.o parse.o output.o
EXECUTABLE = as

PRODUCTS = $(OBJECTS) $(EXECUTABLE)

all: $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)

.PHONY: clean

clean: 
	rm -f $(PRODUCTS)
