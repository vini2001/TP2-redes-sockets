OBJS = server.c equipment.c common.h

CC = gcc -pthread

all : $(OBJS) serverP equipmentP

serverP:
	$(CC) -o server server.c -Wformat-overflow=0
equipmentP:
	$(CC) -o equipment equipment.c -Wformat-overflow=0

