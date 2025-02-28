
MODULE = duplicates_linked_list

CC = g++

CFLAGS = -std=c++11

SOURCE = $(MODULE).cpp
TARGET = $(MODULE)

all: build

build:
	$(CC) $(CFLAGS) $(SOURCE) -o $(TARGET) 

clean:
	if [ -f $(TARGET) ]; \
	then \
		rm $(TARGET); \
	fi;
