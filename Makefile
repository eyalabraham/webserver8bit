#####################################################################################
#
#  This make file is for compiling the 
#  8-bit NEC v25 based web server
#
#  Use:
#    clean      - clean environment
#    all        - build all outputs
#
#####################################################################################

#------------------------------------------------------------------------------------
# generate debug information wit WD.EXE
# change to 'yes' or 'no'
#------------------------------------------------------------------------------------
DEBUG = no

# remove existing implicit rule (specifically '%.o: %c')
.SUFFIXES:

#------------------------------------------------------------------------------------
# project directories
#------------------------------------------------------------------------------------
INCDIR = include
PRECOMP = ../ws.pch
IPDIR = ip

VPATH = $(INCDIR)

#------------------------------------------------------------------------------------
# IP stack files include
#------------------------------------------------------------------------------------
include $(IPDIR)/ipStackFiles.mk

COREOBJ=$(STACKCORE:.c=.o)
NETIFOBJ=$(INTERFACE:.c=.o)
NETWORKOBJ=$(NETWORK:.c=.o)
TRANSPORTOBJ=$(TRANSPORT:.c=.o)

#------------------------------------------------------------------------------------
# build utilities
#------------------------------------------------------------------------------------
ASM = wasm
CC = wcc
LIB = wlib
LINK = wlink

#------------------------------------------------------------------------------------
# tool options
#------------------------------------------------------------------------------------
ifeq ($(DEBUG),yes)
CCDBG = -d2
ASMDBG = -d1
LINKDBG = DEBUG LINES 
else
CCDBG = -d0
ASMDBG =
LINKDBG =
endif

#CCOPT = -0 -ml $(CCDBG) -zu -fh=$(PRECOMP) -s -i=/home/eyal/bin/watcom/h -i=$(INCDIR)
CCOPT = -0 -ml $(CCDBG) -zu -s -zp1 -i=/home/eyal/bin/watcom/h -i=$(INCDIR)
ASMOPT = -0 -ml $(ASMDBG)
SERLOOPLINKCFG = LIBPATH /home/eyal/bin/watcom/lib286/dos \
                 LIBPATH /home/eyal/bin/watcom/lib286     \
                 FORMAT DOS                               \
                 OPTION MAP=ser1loop                      \
                 OPTION ELIMINATE

SPITESTLINKCFG = LIBPATH /home/eyal/bin/watcom/lib286/dos \
                 LIBPATH /home/eyal/bin/watcom/lib286     \
                 FORMAT DOS                               \
                 OPTION MAP=spitest                       \
                 OPTION ELIMINATE

ETHTESTLINKCFG = LIBPATH /home/eyal/bin/watcom/lib286/dos \
                 LIBPATH /home/eyal/bin/watcom/lib286     \
                 FORMAT DOS                               \
                 OPTION STACK=16k                         \
                 OPTION MAP=ethtest                       \
                 OPTION ELIMINATE

HTTPDLINKCFG = LIBPATH /home/eyal/bin/watcom/lib286/dos   \
               LIBPATH /home/eyal/bin/watcom/lib286       \
               FORMAT DOS                                 \
               OPTION STACK=16k                           \
               OPTION MAP=httpd                           \
               OPTION ELIMINATE

STARTLINKCFG  = LIBPATH /home/eyal/bin/watcom/lib286/dos  \
            LIBPATH /home/eyal/bin/watcom/lib286          \
            FORMAT DOS                                    \
            OPTION MAP=startup                            \
            OPTION ELIMINATE                              \

WSLINKCFG = LIBPATH /home/eyal/bin/watcom/lib286/dos \
            LIBPATH /home/eyal/bin/watcom/lib286     \
            FORMAT DOS                               \
            OPTION MAP=ws                            \
            OPTION ELIMINATE                         \
            $(LINKDBG)

#------------------------------------------------------------------------------------
# some variables for the linker
# file names list
#------------------------------------------------------------------------------------
COM = ,
EMPTY =
SPC = $(EMPTY) $(EMPTY)

#------------------------------------------------------------------------------------
# new make patterns
#------------------------------------------------------------------------------------
%.o: %.c
	$(CC) $< $(CCOPT) -fo=$(notdir $@)

#------------------------------------------------------------------------------------
# build all targets
#------------------------------------------------------------------------------------
all: ws spitest ethtest

#------------------------------------------------------------------------------------
# build ser1loop.exe test program
#------------------------------------------------------------------------------------
ser1loop: ser1loop.exe

ser1loop.o: ser1loop.c v25.h

ser1loop.exe: ser1loop.o
	$(LINK) $(SERLOOPLINKCFG) FILE $^ NAME $@

#------------------------------------------------------------------------------------
# build common objects
#------------------------------------------------------------------------------------
ppispi.o: ppispi.c ppispi.h v25.h

st7735.o: st7735.c st7735.h ppispi.h

ip: ipnetif ipcore

ipcore: $(COREOBJ)
ipnetif: $(NETIFOBJ)
ipnetwork: $(NETWORKOBJ)
iptransport: $(TRANSPORTOBJ)

#------------------------------------------------------------------------------------
# build spitest.exe test program
#------------------------------------------------------------------------------------
spitest: spitest.exe

spitest.o: spitest.c ppispi.h st7735.h

spitest.exe: spitest.o ppispi.o st7735.o
	$(LINK) $(SPITESTLINKCFG) FILE $(subst $(SPC),$(COM),$(notdir $^)) NAME $@

#------------------------------------------------------------------------------------
# build ethtest.exe test program
#------------------------------------------------------------------------------------
ethtest: ethtest.exe

ethtest.o: ethtest.c

ethtest.exe: ethtest.o ppispi.o $(COREOBJ) $(NETIFOBJ) $(NETWORKOBJ) $(TRANSPORTOBJ)
	$(LINK) $(ETHTESTLINKCFG) FILE $(subst $(SPC),$(COM),$(notdir $^)) NAME $@

#------------------------------------------------------------------------------------
# build httpd.exe test program
#------------------------------------------------------------------------------------
httpd: httpd.exe

httpd.o: httpd.c

httpd.exe: httpd.o ppispi.o $(COREOBJ) $(NETIFOBJ) $(NETWORKOBJ) $(TRANSPORTOBJ)
	$(LINK) $(HTTPDLINKCFG) FILE $(subst $(SPC),$(COM),$(notdir $^)) NAME $@

#------------------------------------------------------------------------------------
# generate startup.exe
#------------------------------------------------------------------------------------
startup: startup.exe

startup.exe: startup.o
	$(LINK) $(STARTLINKCFG) FILE $(subst $(SPC),$(COM),$(notdir $^)) NAME $@

#------------------------------------------------------------------------------------
# generate ws.exe
#------------------------------------------------------------------------------------
ws: ws.exe

lmtea.o: _lmte.asm
	$(ASM) $(ASMOPT) -fo=$@ $<

ws.exe: lmtea.o lmte.o xprintf.o t_term.o t_dummy.o ws.o
	$(LINK) $(WSLINKCFG) FILE $(subst $(SPC),$(COM),$(notdir $^)) NAME $@

#------------------------------------------------------------------------------------
# cleanup
#------------------------------------------------------------------------------------

.PHONY: clean

clean:
	rm -f *.exe
	rm -f *.o
	rm -f *.lst
	rm -f *.map
	rm -f *.lib
	rm -f *.bak
	rm -f *.cap
	rm -f *.err

