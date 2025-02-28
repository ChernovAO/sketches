
CC = g++
CFLAGS = -std=c++11
SOURCE = unique_characters.cpp
MODULE = unique_characters
TARGET = unique_characters

all: build

build:
	$(CC) $(CFLAGS) $(SOURCE) -o $(TARGET) 

clean:
	if [ -f $(TARGET) ]; \
	then \
		rm $(TARGET); \
	fi;
