# -------------------------------------
#  This make file is for compiling the 
#  8-bit NEC v25 based web server
#
#  Use:
#    clean      - clean environment
#    all        - build all outputs
#
# -------------------------------------

# remove existing implicit rule (dpecifically '%.o: %c')
.SUFFIXES:

#
# project directories
#
INCDIR = ./include

#
# build utilities
#
TASM = ./build.sh -tasm
BCC = ./build.sh -bcc
TLIB = ./build.sh -tlib
TLINK = ./build.sh -tlink

#
# dependencies
#
SER1LOOPDEP = $(INCDIR)/v25.h BccDos.cfg

LMTEDEP = $(INCDIR)/internal.h $(INCDIR)/lmte.h $(INCDIR)/v25.h BccDos.cfg
LMTEOBJ = lmteasm.obj lmte.obj

WSDEP = $(INCDIR)/lmte.h $(INCDIR)/names.h $(INCDIR)/messages.h BccDos.cfg
WSOBJ = lmte.lib ws.obj 

#
# new make patterns
#
%.obj: %.c
	$(BCC) $@ $<

%.lib: %.obj
	$(TLIB) $@ $?
	
#
# build all targets
#
all: lmte.lib ws.exe ser1loop.exe

#
# build ser1loop.exe test program
#
ser1loop.obj: ser1loop.c $(SER1LOOPDEP)

ser1loop.exe: ser1loop.obj
	$(TLINK) $@ $^

#
# generate lmte.lib
#
lmteasm.obj: lmte.asm
	$(TASM) $@ $<

lmte.obj: lmte.c $(LMTEDEP)

lmte.lib: $(LMTEOBJ)

#
# generate ws.exe
#
ws.exe: $(WSDEP)
	@echo "ws.exe under construction..."

.PHONY: CLEAN

clean:
	rm -f *.exe
	rm -f *.obj
	rm -f *.lst
	rm -f *.lib
	rm -f *.out
	rm -f *.bak
	rm -f *.rsp

