
MODULE = hash_table

CC = g++

CFLAGS = -std=c++11

RESULT_DIR = _result
SOURCE = $(MODULE).cpp
TARGET = $(RESULT_DIR)/$(MODULE)

all: build

build: prepare
	$(CC) $(CFLAGS) $(SOURCE) -o $(TARGET) 

clean:
	if [ -f $(RESULT_DIR) ]; \
	then \
		rm -rf $(RESULT_DIR); \
	fi;

prepare:
	if ! [ -d $(RESULT_DIR) ]; \
	then \
		mkdir $(RESULT_DIR); \
	fi;
