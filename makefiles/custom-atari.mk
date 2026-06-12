# COMPILE FLAGS

# reserved memory for graphics
# LDFLAGS += -Wl -D,__RESERVED_MEMORY__=0x2000

#LDFLAGS += --start-addr 0x4400
LDFLAGS += -C cfg/atari.cfg
LDFLAGS += --start-addr 0x4000

CFLAGS += -DBWC_CUSTOM_CPUTC

NETSTREAM_REPO ?= ../fujinet-atari-netstream
NETSTREAM_REPO_EXPANDED := $(if $(filter ~/%,$(NETSTREAM_REPO)),$(HOME)/$(patsubst ~/%,%,$(NETSTREAM_REPO)),$(if $(filter ~,$(NETSTREAM_REPO)),$(HOME),$(NETSTREAM_REPO)))
NETSTREAM_INPUT_BUFSIZE ?= 1024
NETSTREAM_HANDLER_SRC := $(NETSTREAM_REPO_EXPANDED)/handler/ca65/netstream.s
NETSTREAM_HANDLER_OBJ := $(OBJDIR)/$(CURRENT_TARGET)/atari/netstream_handler.o

OBJECTS := $(filter-out $(OBJDIR)/$(CURRENT_TARGET)/atari/netstream/netstream_api.o,$(OBJECTS))
OBJECTS += $(NETSTREAM_HANDLER_OBJ)

$(NETSTREAM_HANDLER_OBJ): $(NETSTREAM_HANDLER_SRC) | $(OBJDIR)
	@$(call MKDIR,$(dir $@))
	ca65 -t $(CURRENT_TARGET) --create-dep $(@:.o=.d) --include-dir $(NETSTREAM_REPO_EXPANDED)/handler/ca65/include -D INPUT_BUFSIZE=$(NETSTREAM_INPUT_BUFSIZE) -o $@ $<

# DISK creation

SUFFIX = .com
DISK_TASKS += .atr
ASSETS_DIR := assets
PICOBOOT_DOWNLOAD_URL = https://github.com/FujiNetWIFI/assets/releases/download/picobin/picoboot.bin

# atari cache dir
ATARI_CACHE_DIR := $(CACHE_DIR)/atari

.atr:
	@which dir2atr > /dev/null 2>&1 ; \
	if [ $$? -ne 0 ] ; then \
		echo -e "\nERROR! You must compile and install dir2atr from https://github.com/HiassofT/AtariSIO to create atari disks\n" ; \
		exit 1 ; \
	fi
	$(call MKDIR,$(DIST_DIR)/atr)
	$(call MKDIR,$(CACHE_DIR))
	$(call MKDIR,$(ATARI_CACHE_DIR))
	cp $(DIST_DIR)/$(PROGRAM_TGT)$(SUFFIX) $(DIST_DIR)/atr/$(PROGRAM)$(SUFFIX)
	@if [ -f $(DIST_DIR)/$(PROGRAM).atr ] ; then \
		rm $(DIST_DIR)/$(PROGRAM).atr ; \
	fi
	@if [ ! -f $(ATARI_CACHE_DIR)/picoboot.bin ] ; then \
		echo "Downloading picoboot.bin"; \
		curl -sL $(PICOBOOT_DOWNLOAD_URL) -o $(ATARI_CACHE_DIR)/picoboot.bin; \
	fi
	dir2atr -m -S -B $(ATARI_CACHE_DIR)/picoboot.bin $(DIST_DIR)/$(PROGRAM).atr $(DIST_DIR)/atr
	rm -rf $(DIST_DIR)/atr

# TESTING / EMULATOR

# Specify ATARI_EMULATOR=[ALTIRRA|ATARI800] to set which one to run, default is ALTIRRA

ALTIRRA ?= $(ALTIRRA_BIN) \
  $(XS)/portable $(XS)/portablealt:altirra-debug.ini \
  $(XS)/debug \
  $(XS)/debugcmd: ".loadsym build\$(PROGRAM).$(CURRENT_TARGET).lbl" \

# Additional args that can be copied into the above lines
#   $(XS)/debugcmd: "bp _debug" \
#   $(XS)/debugcmd: "bp _network_open" \
#   $(XS)/debugcmd: "bp _bus_status" \

ATARI800 ?= $(ATARI800_HOME)/atari800 \
  -xl -nobasic -ntsc -xl-rev custom -config atari800-debug.cfg -run

atari_EMUCMD := $($(ATARI_EMULATOR))

ifeq ($(ATARI_EMULATOR),)
atari_EMUCMD := $(ALTIRRA)
endif
