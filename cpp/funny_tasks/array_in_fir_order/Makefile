
MODULE = array_in_fir_order

CC = g++

CFLAGS = -g -std=c++11
LFLAGS = -l:libgtest.a -l:libgtest_main.a -lpthread

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
