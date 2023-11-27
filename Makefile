CC=gcc
CFLAGS=-Wall -Wextra -Werror

all: clean build

default: build

build: server.cpp client.cpp
	g++ -Wall -Wextra -o server server.cpp
	g++ -Wall -Wextra -o client client.cpp

clean:
	rm -f server client output.txt project2.zip

zip: 
	zip project2.zip server.c client.c utils.h Makefile README