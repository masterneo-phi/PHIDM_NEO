# Makefile for the C++ port of phi_dm (stage 0: builds as C++, code is still C)
#
# Original C build used: gcc -DBSD -O3 -fcommon -Inames
# Stage 0 uses g++ with C++17. No -fcommon: tentative definitions in
# headers were converted to extern declarations (see docs/cxx-port-notes.md).
#
# NOTE: the original Makefile's `install` target copied the binary into a
# live world directory (DISTDIR). It is intentionally disabled here so a
# build can never overwrite a running server. Copy the binary by hand.

SHELL	= /bin/sh
CXX	= g++
CXXFLAGS = -std=c++17 -DBSD -O2 -Inames -Wall -finput-charset=UTF-8 -fexec-charset=CP932
LIBS	= -lm

BSDDIR  = bsd
BSDARC  = bsd.o

OBJS	= phi_dm.o phi_data.o phi_enemy.o \
		phi_magic.o phi_attack.o phi_use.o phi_calc.o \
		phi_str.o phi_m57.o phi_m81.o new_proto.o ext_data.o new_command.o \
		expression_op.o ext_skills.o ext_party.o \
		$(BSDARC) \
		tcp.o
SRCS	= phi_dm.cpp phi_data.cpp phi_enemy.cpp \
		phi_magic.cpp phi_attack.cpp phi_use.cpp phi_calc.cpp \
		phi_str.cpp phi_m57.cpp phi_m81.cpp new_proto.cpp ext_data.cpp new_command.cpp \
		expression_op.cpp ext_skills.cpp ext_party.cpp \
		tcp.cpp
PROGRAM	= phi_dm
DOCS	= phi.dat README.sjis

# Every object depends on every header: the original Makefile had no
# header dependencies, so editing a header left stale .o files behind.
HEADERS	= $(wildcard *.h) $(wildcard names/*.h) phi_dm.cnf

all:	$(PROGRAM)

$(OBJS) cdcnv.o: $(HEADERS)

.cpp.o:
	$(CXX) $< $(CXXFLAGS) -c -o $@

$(PROGRAM): $(OBJS)
	$(CXX) -o $@ $(OBJS) $(LIBS)

$(BSDARC):
	cd $(BSDDIR); $(MAKE)

install:
	@echo "install is disabled in the C++ port tree."
	@echo "Copy $(PROGRAM) to a test world directory by hand."
	@false

clean:
	rm -f *.o $(PROGRAM)
	cd $(BSDDIR); $(MAKE) clean

cdcnv: cdcnv.o $(BSDARC)
	$(CXX) -o cdcnv cdcnv.o $(BSDARC)
