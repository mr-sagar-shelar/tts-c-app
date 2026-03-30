CC=gcc
CFLAGS=-Wall -Wextra -O2
TARGET=sai
OBJS=main.o menu.o cJSON.o config.o contacts.o utils.o file_manager.o notepad.o dictionary.o entertainment.o tools.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

main.o: main.c menu.h config.h contacts.h utils.h file_manager.h notepad.h dictionary.h entertainment.h tools.h
	$(CC) $(CFLAGS) -c main.c

menu.o: menu.c menu.h cJSON.h
	$(CC) $(CFLAGS) -c menu.c

config.o: config.c config.h cJSON.h
	$(CC) $(CFLAGS) -c config.c

contacts.o: contacts.c contacts.h cJSON.h utils.h
	$(CC) $(CFLAGS) -c contacts.c

cJSON.o: cJSON.c cJSON.h
	$(CC) $(CFLAGS) -c cJSON.c

utils.o: utils.c utils.h
	$(CC) $(CFLAGS) -c utils.c

file_manager.o: file_manager.c file_manager.h menu.h utils.h
	$(CC) $(CFLAGS) -c file_manager.c

notepad.o: notepad.c notepad.h utils.h file_manager.h
	$(CC) $(CFLAGS) -c notepad.c

dictionary.o: dictionary.c dictionary.h utils.h config.h cJSON.h
	$(CC) $(CFLAGS) -c dictionary.c

entertainment.o: entertainment.c entertainment.h utils.h cJSON.h
	$(CC) $(CFLAGS) -c entertainment.c

tools.o: tools.c tools.h utils.h config.h cJSON.h
	$(CC) $(CFLAGS) -c tools.c

clean:
	rm -f $(TARGET) $(OBJS)
