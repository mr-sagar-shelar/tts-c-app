CC=gcc
CFLAGS=-Wall -Wextra -O2
TARGET=sai
OBJS=main.o menu.o cJSON.o config.o contacts.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

main.o: main.c menu.h config.h contacts.h
	$(CC) $(CFLAGS) -c main.c

menu.o: menu.c menu.h cJSON.h
	$(CC) $(CFLAGS) -c menu.c

config.o: config.c config.h cJSON.h
	$(CC) $(CFLAGS) -c config.c

contacts.o: contacts.c contacts.h cJSON.h
	$(CC) $(CFLAGS) -c contacts.c

cJSON.o: cJSON.c cJSON.h
	$(CC) $(CFLAGS) -c cJSON.c

clean:
	rm -f $(TARGET) $(OBJS)
