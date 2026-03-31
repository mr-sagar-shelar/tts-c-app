CC=gcc
CFLAGS=-Wall -Wextra -O2
TARGET=sai
OBJS=main.o app_actions.o menu.o cJSON.o config.o contacts.o utils.o file_manager.o notepad.o dictionary.o entertainment.o tools.o typing_tutor.o alarm.o calendar.o radio.o text_processor.o document_reader.o speech_settings.o speech_engine.o voice_library.o download_manager.o download_ui.o task_ui.o

FLITEDIR ?=
ifeq ($(FLITEDIR),)
ifneq ($(wildcard ../flite/include/flite.h),)
FLITEDIR := $(abspath ../flite)
else ifneq ($(wildcard /usr/include/flite.h),)
FLITEDIR := /usr
endif
endif

FLITE_LIBS =
ifneq ($(FLITEDIR),)
ifneq ($(wildcard $(FLITEDIR)/include/flite.h),)
CFLAGS += -DHAVE_FLITE -I$(FLITEDIR)/include
TARGET_PLATFORM ?= $(notdir $(firstword $(wildcard $(FLITEDIR)/build/*)))
FLITELIBDIR := $(FLITEDIR)/build/$(TARGET_PLATFORM)/lib
ifneq ($(wildcard $(FLITELIBDIR)/libflite.a),)
FLITE_LIBS += -L$(FLITELIBDIR) -lflite_usenglish -lflite_cmulex -lflite_cmu_indic_lang -lflite_cmu_indic_lex -lflite_cmu_us_kal -lflite_cmu_us_slt -lflite_cmu_us_rms -lflite_cmu_us_awb -lflite -lm
endif
endif
endif

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS) $(FLITE_LIBS)

main.o: main.c menu.h config.h contacts.h utils.h file_manager.h notepad.h dictionary.h entertainment.h tools.h typing_tutor.h alarm.h calendar.h radio.h
	$(CC) $(CFLAGS) -c main.c

app_actions.o: app_actions.c app_actions.h menu.h
	$(CC) $(CFLAGS) -c app_actions.c

text_processor.o: text_processor.c text_processor.h
	$(CC) $(CFLAGS) -c text_processor.c

document_reader.o: document_reader.c document_reader.h
	$(CC) $(CFLAGS) -c document_reader.c

speech_settings.o: speech_settings.c speech_settings.h config.h
	$(CC) $(CFLAGS) -c speech_settings.c

speech_engine.o: speech_engine.c speech_engine.h config.h
	$(CC) $(CFLAGS) -c speech_engine.c

voice_library.o: voice_library.c voice_library.h utils.h
	$(CC) $(CFLAGS) -c voice_library.c

download_manager.o: download_manager.c download_manager.h
	$(CC) $(CFLAGS) -c download_manager.c

download_ui.o: download_ui.c download_ui.h download_manager.h utils.h
	$(CC) $(CFLAGS) -c download_ui.c

task_ui.o: task_ui.c task_ui.h utils.h
	$(CC) $(CFLAGS) -c task_ui.c

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

typing_tutor.o: typing_tutor.c typing_tutor.h utils.h cJSON.h
	$(CC) $(CFLAGS) -c typing_tutor.c

alarm.o: alarm.c alarm.h utils.h cJSON.h
	$(CC) $(CFLAGS) -c alarm.c

calendar.o: calendar.c calendar.h cJSON.h utils.h config.h
	$(CC) $(CFLAGS) -c calendar.c

radio.o: radio.c radio.h utils.h
	$(CC) $(CFLAGS) -c radio.c

clean:
	rm -f $(TARGET) $(OBJS)
