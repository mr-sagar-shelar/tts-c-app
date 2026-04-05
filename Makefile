CC=gcc
CFLAGS=-Wall -Wextra -O2 -pthread
TARGET=sai
UNAME_S := $(shell uname -s)
ENABLE_MEMORY_WIDGET ?= 0
ENABLE_MENU_SPEECH ?= $(if $(filter Linux,$(UNAME_S)),1,0)
PREFIX ?= /usr/local
APP_BASE_DIR ?= $(PREFIX)/share/sai-base
AR ?= ar
RANLIB ?= ranlib

OBJS=main.o app_actions.o menu.o cJSON.o config.o contacts.o utils.o file_manager.o notepad.o dictionary.o entertainment.o tools.o typing_tutor.o alarm.o calendar.o radio.o text_processor.o document_reader.o speech_settings.o speech_engine.o voice_library.o download_manager.o download_ui.o task_ui.o
APP_DATA_FILES=menu.json en.json hi.json dict_en.json dict_hn.json typing_tutor.json typing_leaderboard_mock.json timezones.json userConfig.json

ifeq ($(ENABLE_MENU_SPEECH),1)
OBJS += menu_audio.o
CFLAGS += -DENABLE_MENU_SPEECH
else
OBJS += menu_audio_stub.o
endif

ifeq ($(ENABLE_MEMORY_WIDGET),1)
CFLAGS += -DENABLE_MEMORY_WIDGET
endif

SDL_CONFIG ?= $(shell command -v sdl2-config 2>/dev/null)
SDL_CFLAGS ?=
SDL_LIBS ?=
ifeq ($(strip $(SDL_CFLAGS) $(SDL_LIBS)),)
ifneq ($(SDL_CONFIG),)
SDL_CFLAGS := $(shell $(SDL_CONFIG) --cflags)
SDL_LIBS := $(shell $(SDL_CONFIG) --libs)
endif
endif
ifneq ($(strip $(SDL_CFLAGS) $(SDL_LIBS)),)
CFLAGS += -DHAVE_SDL_AUDIO $(SDL_CFLAGS)
endif

LOCAL_FLITEDIR := $(abspath third_party/flite)
FLITEDIR ?= $(LOCAL_FLITEDIR)
CC_MACHINE ?= $(shell $(CC) -dumpmachine 2>/dev/null)
FLITE_TARGET_CPU ?= $(firstword $(subst -, ,$(TARGET_PLATFORM)))
FLITE_TARGET_OS ?= $(if $(findstring linux,$(TARGET_PLATFORM)),linux,$(if $(findstring darwin,$(TARGET_PLATFORM)),darwin,$(word 2,$(subst -, ,$(TARGET_PLATFORM)))))
FLITE_SYSTEM_PLATFORM ?= $(if $(strip $(CC_MACHINE)),$(CC_MACHINE),$(TARGET_PLATFORM))
FLITE_SYSTEM_FULLOSTYPE ?= $(if $(findstring linux,$(FLITE_SYSTEM_PLATFORM)),linux-gnu,$(wordlist 3,99,$(subst -, ,$(FLITE_SYSTEM_PLATFORM))))

FLITE_LIBS =
FLITE_BUILD_DEPS =
ifneq ($(FLITEDIR),)
ifneq ($(wildcard $(FLITEDIR)/include/flite.h),)
CFLAGS += -DHAVE_FLITE -I$(FLITEDIR)/include
TARGET_PLATFORM ?= $(shell awk 'BEGIN{cpu=""; os=""} $$1=="TARGET_CPU" {cpu=$$3} $$1=="TARGET_OS" {os=$$3} END{if (cpu != "" && os != "") print cpu "-" os}' $(FLITEDIR)/config/config)
FLITELIBDIR := $(FLITEDIR)/build/$(TARGET_PLATFORM)/lib
FLITE_LIBS += -L$(FLITELIBDIR) -lflite_usenglish -lflite_cmulex -lflite_cmu_indic_lang -lflite_cmu_indic_lex -lflite_cmu_us_kal -lflite_cmu_us_slt -lflite_cmu_us_rms -lflite_cmu_us_awb -lflite -lm
ifneq ($(findstring linux,$(TARGET_PLATFORM)),)
FLITE_LIBS += -Wl,-rpath,$(FLITELIBDIR)
endif
endif
endif

FLITE_LIBS += $(SDL_LIBS) -pthread

ifeq ($(FLITEDIR),$(LOCAL_FLITEDIR))
FLITE_BUILD_DEPS = $(FLITEDIR)/build/.built
endif

all: $(TARGET)

$(TARGET): $(OBJS) $(FLITE_BUILD_DEPS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS) $(FLITE_LIBS)

install-rootfs: $(TARGET) $(FLITE_BUILD_DEPS)
	@test -n "$(DESTDIR)" || (echo "DESTDIR is required for install-rootfs" >&2; exit 1)
	@install -d "$(DESTDIR)$(APP_BASE_DIR)" "$(DESTDIR)$(APP_BASE_DIR)/Downloads" "$(DESTDIR)$(APP_BASE_DIR)/UserSpace" "$(DESTDIR)$(APP_BASE_DIR)/voices" "$(DESTDIR)$(PREFIX)/lib"
	@install -m 0755 "$(TARGET)" "$(DESTDIR)$(APP_BASE_DIR)/$(TARGET)"
	@for file in $(APP_DATA_FILES); do \
		install -m 0644 "$$file" "$(DESTDIR)$(APP_BASE_DIR)/$$file"; \
	done
	@if [ -d "$(FLITELIBDIR)" ]; then \
		set -- "$(FLITELIBDIR)"/libflite*.so*; \
		if [ -e "$$1" ]; then \
			cp -a "$$@" "$(DESTDIR)$(PREFIX)/lib/"; \
		fi; \
	fi

$(LOCAL_FLITEDIR)/build/.built:
	@sed -i.bak 's/-no-cpp-precomp//g' $(LOCAL_FLITEDIR)/config/config
	@sed -i.bak 's/^TARGET_CPU.*/TARGET_CPU   = $(FLITE_TARGET_CPU)/' $(LOCAL_FLITEDIR)/config/config
	@sed -i.bak 's/^TARGET_OS.*/TARGET_OS    = $(FLITE_TARGET_OS)/' $(LOCAL_FLITEDIR)/config/config
	@sed -i.bak 's/^HOST_CPU.*/HOST_CPU   = $(FLITE_TARGET_CPU)/' $(LOCAL_FLITEDIR)/config/config
	@sed -i.bak 's/^HOST_OS.*/HOST_OS    = $(FLITE_TARGET_OS)/' $(LOCAL_FLITEDIR)/config/config
	@sed -i.bak 's|^CC .*|CC       = $(CC)|' $(LOCAL_FLITEDIR)/config/config
	@sed -i.bak 's|^AR .*|AR       = $(AR)|' $(LOCAL_FLITEDIR)/config/config
	@sed -i.bak 's|^RANLIB .*|RANLIB   = $(RANLIB)|' $(LOCAL_FLITEDIR)/config/config
	@sed -i.bak 's/^OSTYPE.*/OSTYPE		:= $(FLITE_TARGET_OS)/' $(LOCAL_FLITEDIR)/config/system.mak
	@sed -i.bak 's/^MACHINETYPE.*/MACHINETYPE	:= $(FLITE_TARGET_CPU)/' $(LOCAL_FLITEDIR)/config/system.mak
	@sed -i.bak 's/^PLATFORM.*/PLATFORM	:= $(FLITE_SYSTEM_PLATFORM)/' $(LOCAL_FLITEDIR)/config/system.mak
	@sed -i.bak 's/^FULLOSTYPE.*/FULLOSTYPE	:= $(FLITE_SYSTEM_FULLOSTYPE)/' $(LOCAL_FLITEDIR)/config/system.mak
	@rm -f $(LOCAL_FLITEDIR)/config/config.bak $(LOCAL_FLITEDIR)/config/system.mak.bak
	LD_LIBRARY_PATH=$(FLITELIBDIR):$(LD_LIBRARY_PATH) $(MAKE) -C $(LOCAL_FLITEDIR) CC='$(CC)' AR='$(AR)' RANLIB='$(RANLIB)'
	@mkdir -p $(LOCAL_FLITEDIR)/build
	@touch $(LOCAL_FLITEDIR)/build/.built

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

menu_audio.o: menu_audio.c menu_audio.h speech_engine.h config.h
	$(CC) $(CFLAGS) -c menu_audio.c

menu_audio_stub.o: menu_audio_stub.c menu_audio.h
	$(CC) $(CFLAGS) -c menu_audio_stub.c

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
	@if [ -d "$(LOCAL_FLITEDIR)" ]; then $(MAKE) -C $(LOCAL_FLITEDIR) clean; fi
	@rm -f $(LOCAL_FLITEDIR)/build/.built

.PHONY: all clean install-rootfs
