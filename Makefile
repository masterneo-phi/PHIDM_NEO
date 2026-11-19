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
		tcp.o phi_class.o $(LUAOBJS)
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

CC	= gcc
LUAOBJS	= lua/lapi.o lua/lcode.o lua/lctype.o lua/ldebug.o lua/ldo.o \
		lua/ldump.o lua/lfunc.o lua/lgc.o lua/llex.o lua/lmem.o \
		lua/lobject.o lua/lopcodes.o lua/lparser.o lua/lstate.o lua/lstring.o \
		lua/ltable.o lua/ltm.o lua/lundump.o lua/lvm.o lua/lzio.o \
		lua/lauxlib.o lua/lbaselib.o lua/lmathlib.o lua/lstrlib.o lua/ltablib.o

# BSD make には GNU make 式の % パターンルールがないため、Lua の各ファイルを明示ルールで書く
lua/lapi.o: lua/lapi.c
	$(CC) -O2 -c -o $@ lua/lapi.c
lua/lcode.o: lua/lcode.c
	$(CC) -O2 -c -o $@ lua/lcode.c
lua/lctype.o: lua/lctype.c
	$(CC) -O2 -c -o $@ lua/lctype.c
lua/ldebug.o: lua/ldebug.c
	$(CC) -O2 -c -o $@ lua/ldebug.c
lua/ldo.o: lua/ldo.c
	$(CC) -O2 -c -o $@ lua/ldo.c
lua/ldump.o: lua/ldump.c
	$(CC) -O2 -c -o $@ lua/ldump.c
lua/lfunc.o: lua/lfunc.c
	$(CC) -O2 -c -o $@ lua/lfunc.c
lua/lgc.o: lua/lgc.c
	$(CC) -O2 -c -o $@ lua/lgc.c
lua/llex.o: lua/llex.c
	$(CC) -O2 -c -o $@ lua/llex.c
lua/lmem.o: lua/lmem.c
	$(CC) -O2 -c -o $@ lua/lmem.c
lua/lobject.o: lua/lobject.c
	$(CC) -O2 -c -o $@ lua/lobject.c
lua/lopcodes.o: lua/lopcodes.c
	$(CC) -O2 -c -o $@ lua/lopcodes.c
lua/lparser.o: lua/lparser.c
	$(CC) -O2 -c -o $@ lua/lparser.c
lua/lstate.o: lua/lstate.c
	$(CC) -O2 -c -o $@ lua/lstate.c
lua/lstring.o: lua/lstring.c
	$(CC) -O2 -c -o $@ lua/lstring.c
lua/ltable.o: lua/ltable.c
	$(CC) -O2 -c -o $@ lua/ltable.c
lua/ltm.o: lua/ltm.c
	$(CC) -O2 -c -o $@ lua/ltm.c
lua/lundump.o: lua/lundump.c
	$(CC) -O2 -c -o $@ lua/lundump.c
lua/lvm.o: lua/lvm.c
	$(CC) -O2 -c -o $@ lua/lvm.c
lua/lzio.o: lua/lzio.c
	$(CC) -O2 -c -o $@ lua/lzio.c
lua/lauxlib.o: lua/lauxlib.c
	$(CC) -O2 -c -o $@ lua/lauxlib.c
lua/lbaselib.o: lua/lbaselib.c
	$(CC) -O2 -c -o $@ lua/lbaselib.c
lua/lmathlib.o: lua/lmathlib.c
	$(CC) -O2 -c -o $@ lua/lmathlib.c
lua/lstrlib.o: lua/lstrlib.c
	$(CC) -O2 -c -o $@ lua/lstrlib.c
lua/ltablib.o: lua/ltablib.c
	$(CC) -O2 -c -o $@ lua/ltablib.c

$(PROGRAM): $(OBJS)
	$(CXX) -o $@ $(OBJS) $(LIBS)

$(BSDARC):
	cd $(BSDDIR); $(MAKE)

install:
	@echo "install is disabled in the C++ port tree."
	@echo "Copy $(PROGRAM) to a test world directory by hand."
	@false

clean:
	rm -f *.o lua/*.o $(PROGRAM)
	cd $(BSDDIR); $(MAKE) clean

cdcnv: cdcnv.o $(BSDARC)
	$(CXX) -o cdcnv cdcnv.o $(BSDARC)
