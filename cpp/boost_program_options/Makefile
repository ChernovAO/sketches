
MODULE = boost_program_options

CC = g++

CFLAGS = -std=c++11
LFLAGS = -lboost_program_options

RESULT_DIR = _result
SOURCE = $(MODULE).cpp
TARGET = $(RESULT_DIR)/$(MODULE)

all: build

build: prepare
	$(CC) $(CFLAGS) $(SOURCE) $(LFLAGS) -o $(TARGET) 

clean:
	if [ -d $(RESULT_DIR) ]; \
	then \
		rm -rf $(RESULT_DIR); \
	fi;

prepare:
	if ! [ -d $(RESULT_DIR) ]; \
	then \
		mkdir $(RESULT_DIR); \
	fi;
