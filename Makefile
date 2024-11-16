VERSION = 1.3.99
DEBUG = 0

# We use fixed addresses to avoid overlap when relocating
# and other trouble with initrd

# Load the bootstrap at 1Mb
TEXTADDR	= 0x100000
# Malloc block of 1MB
MALLOCSIZE	= 0x100000
# Load kernel and ramdisk at as low as possible
KERNELADDR	= 0x00000000

# Set this to the prefix of your cross-compiler, if you have one.
# Else leave it empty.
#
CROSS =

CC		:= $(CROSS)gcc
LD		:= $(CROSS)ld
AS		:= $(CROSS)as
OBJCOPY		:= $(CROSS)objcopy

# The compiler to use for yaboot itself
YBCC = $(CC)

# The flags for the yaboot binary.
#
YBCFLAGS = -Os -m32 -nostdinc -Wall -isystem `$(YBCC) -m32 -print-file-name=include` -fsigned-char -mbig-endian
YBCFLAGS += -DVERSION="\"${VERSION}${VERSIONEXTRA}\""
YBCFLAGS += -DTEXTADDR=$(TEXTADDR) -DDEBUG=$(DEBUG)
YBCFLAGS += -DMALLOCADDR=$(MALLOCADDR) -DMALLOCSIZE=$(MALLOCSIZE)
YBCFLAGS += -DKERNELADDR=$(KERNELADDR)
YBCFLAGS += -fgnu89-inline -fno-builtin-malloc -fno-stack-protector -no-pie
YBCFLAGS += -fcommon
YBCFLAGS += -I ./include
YBCFLAGS += -fno-strict-aliasing

YBCFLAGS += -DCONFIG_COLOR_TEXT
YBCFLAGS += -DCONFIG_SET_COLORMAP
YBCFLAGS += -DUSE_MD5_PASSWORDS

# Link flags
#
LFLAGS = -Ttext $(TEXTADDR) -Bstatic -melf32ppclinux

## End of configuration section

OBJS = second/crt0.o second/yaboot.o second/cache.o second/prom.o second/file.o \
	second/partition.o second/fs.o second/cfg.o second/setjmp.o second/cmdline.o \
	second/fs_of.o second/fs_iso.o second/fs_swap.o \
	second/iso_util.o second/md5.o \
	lib/nonstd.o \
	lib/nosys.o lib/string.o lib/strtol.o lib/vsprintf.o lib/ctype.o lib/malloc.o lib/strstr.o

all: yaboot

yaboot: $(OBJS)
	$(LD) $(LFLAGS) $(OBJS) -o second/$@
	chmod -x second/yaboot

%.o: %.c
	$(YBCC) $(YBCFLAGS) -c -o $@ $<

%.o: %.S
	$(YBCC) $(YBCFLAGS) -D__ASSEMBLY__  -c -o $@ $<

clean:
	rm -f second/yaboot $(OBJS)
