CC=gcc
CFLAGS=-Wall -Wextra -O2
TARGET=sai
OBJS=main.o menu.o cJSON.o config.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

main.o: main.c menu.h config.h
	$(CC) $(CFLAGS) -c main.c

menu.o: menu.c menu.h cJSON.h
	$(CC) $(CFLAGS) -c menu.c

config.o: config.c config.h cJSON.h
	$(CC) $(CFLAGS) -c config.c

cJSON.o: cJSON.c cJSON.h
	$(CC) $(CFLAGS) -c cJSON.c

clean:
	rm -f $(TARGET) $(OBJS)
