# -------------------------------------
#  This make file is for compiling the 
#  8-bit NEC v25 based web server
#
#  Use:
#    clean      - clean environment
#    all        - build all outputs
#
# -------------------------------------

#
# generate debug information wit WD.EXE
# change to 'yes' or 'no'
#
DEBUG = no

# remove existing implicit rule (dpecifically '%.o: %c')
.SUFFIXES:

#
# project directories
#
INCDIR = ./include
PRECOMP = ../ws.pch

#
# build utilities
#
ASM = wasm
CC = wcc
LIB = wlib
LINK = wlink

#
# tool options
#
ifeq ($(DEBUG),yes)
CCDBG = -d2
ASMDBG = -d1
LINKDBG = DEBUG LINES 
else
CCDBG = -d0
ASMDBG =
LINKDBG =
endif

CCOPT = -0 -ml $(CCDBG) -zu -fh=$(PRECOMP) -s -i=/home/eyal/bin/watcom/h -i=$(INCDIR)
ASMOPT = -0 -ml $(ASMDBG)
SERLOOPLINKCFG = LIBPATH /home/eyal/bin/watcom/lib286/dos \
                 LIBPATH /home/eyal/bin/watcom/lib286     \
                 FORMAT DOS                               \
                 OPTION MAP=ser1loop			  \
                 OPTION ELIMINATE
WSLINKCFG = LIBPATH /home/eyal/bin/watcom/lib286/dos \
            LIBPATH /home/eyal/bin/watcom/lib286     \
            FORMAT DOS                               \
            OPTION MAP=ws  			     \
            OPTION ELIMINATE                         \
            $(LINKDBG)

#
# dependencies
#
SER1LOOPDEP = $(INCDIR)/v25.h
WSDEP = $(INCDIR)/lmte.h $(INCDIR)/names.h $(INCDIR)/messages.h $(INCDIR)/t_term.h $(INCDIR)/t_dummy.h

#
# some variables for the linker
# file names list
#
COM = ,
EMPTY = 
SPC = $(EMPTY) $(EMPTY)

#
# new make patterns
#
%.o: %.c
	$(CC) $< $(CCOPT) -fo=$@

#
# build all targets
#
all: ws ser1loop

#
# build ser1loop.exe test program
#
ser1loop: ser1loop.exe

ser1loop.o: ser1loop.c $(SER1LOOPDEP)

ser1loop.exe: ser1loop.o
	$(LINK) $(SERLOOPLINKCFG) FILE $^ NAME $@

#
# generate ws.exe
#
ws: ws.exe

lmtea.o: _lmte.asm
	$(ASM) $(ASMOPT) -fo=$@ $<

ws.exe: lmtea.o lmte.o t_term.o t_dummy.o ws.o
	$(LINK) $(WSLINKCFG) FILE $(subst $(SPC),$(COM),$^) NAME $@

.PHONY: CLEAN

clean:
	rm -f *.exe
	rm -f *.o
	rm -f *.lst
	rm -f *.map
	rm -f *.lib
	rm -f *.bak
	rm -f *.err

