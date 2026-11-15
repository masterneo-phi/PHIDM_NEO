# bsd/ compatibility helpers (strlcpy/strlcat) built as C++.

CXX	= g++
CXXFLAGS = -std=c++17 -DBSD -O2 -Wall -finput-charset=UTF-8 -fexec-charset=CP932
BSDOBJ  = strlcpy.o strlcat.o
BSDSRC 	= strlcpy.cpp strlcat.cpp
BSDARC  = bsd.o

.cpp.o:
	$(CXX) $< $(CXXFLAGS) -c -o $@

$(BSDARC): $(BSDOBJ)
	ar -r ../$@ $(BSDOBJ)

clean:
	rm -f *.o
