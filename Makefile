CFLAGS = -pedantic -ansi -Wall -Werror -g
LDLIBS = -lm

HEADERS = consts.h types.h as.h table.h output.h parse.h
OBJECTS = as.o table.o parse.o output.o
EXECUTABLE = as

all: $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)

$(OBJECTS): $(HEADERS)

.PHONY: clean
clean: 
	rm -f $(OBJECTS) $(EXECUTABLE)

.PHONY: test
test: $(EXECUTABLE)
	./as ps ps2 ps3 ps4
