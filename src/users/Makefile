# Compiler
CC = gcc
OPTS = -Wall -Wextra
CFLAGS=

# Libraries
LIBS = -lm -pthread

# Project name
PROJECT = CoreMess

# Directories
SRC := .
INC := ../include
OBJ := .

SOURCES := $(wildcard $(SRC)/*.c)
OBJECTS := $(patsubst $(SRC)/%.c, $(OBJ)/%.o, $(SOURCES))

all: $(OBJECTS)
	$(CC) $^ -o $@

%.o: $(SRC)/%.c
	$(CC) $(OPT) -I$(INC)  $< -o $@
	
clean:
	-rm *.o $(OBJECTS) :
	
buildrepo:
	@$(call make-repo)

# Create obj directory structure
define make-repo
	mkdir -p $(OBJDIR)
	for dir in $(SRCDIRS); \
	do \
		mkdir -p $(OBJDIR)/$$dir; \
	done
endef
