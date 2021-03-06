ifeq (, $(shell which jemalloc-config))
JEMALLOC =
else
JEMALLOCLD = $(shell jemalloc-config --libdir)
JEMALLOC = -L$(JEMALLOCLD) -ljemalloc
endif

# Always compile with LONG (note that timings on small graphs with less than
# 2^{32} edges may be slightly faster without this flag)
INTT = -DLONG

ifdef EDGELONG
INTE = -DEDGELONG
endif

FORMAT = -DCSR # (oneof {DCSR, DCOO})

INCLUDE_DIRS = -I../../../

OPT = -O3 -g -DNDEBUG
#OPT = -O0 -g

INTEL_PCM_HOME = /usr0/home/ldhulipa/pcm/
ifdef INTEL_PCM_HOME
	INCLUDE_DIRS+=-I$(INTEL_PCM_HOME)
	EXT_OBJECTS=$(INTEL_PCM_HOME)/msr.o $(INTEL_PCM_HOME)/cpucounters.o $(INTEL_PCM_HOME)/pci.o $(INTEL_PCM_HOME)/client_bw.o $(INTEL_PCM_HOME)/utils.o $(INTEL_PCM_HOME)/mmio.o
	PCM = -DUSE_PCM_LIB
endif

CFLAGS = $(INCLUDE_DIRS) -mcx16 -ldl -std=c++17 -march=native $(OPT) $(INTT) $(INTE) -DAMORTIZEDPD $(CONCEPTS) -DUSEMALLOC $(FORMAT) $(PCM) $(EXT_OBJECTS) #-DREPORT_PATH_LENGTHS -DREPORT_MAX_TRIES

OMPFLAGS = -DOPENMP -fopenmp
CILKFLAGS = -DCILK -fcilkplus
HGFLAGS = -DHOMEGROWN -pthread

ifdef CLANG
CC = clang++
PFLAGS = $(CILKFLAGS)
else ifdef CILK
CC = g++-7
PFLAGS = $(CILKFLAGS)
else ifdef OPENMP
CC = g++-7
PFLAGS = $(OMPFLAGS)
else ifdef HOMEGROWN
CC = g++-7
PFLAGS = $(HGFLAGS)
else ifdef SERIAL
CC = g++-7
PFLAGS =
else # default is homegrown
CC = g++-7
PFLAGS = $(HGFLAGS)
endif

ALL=benchmark_sampling benchmark_edge_gather

all: $(ALL)

% : %.cc
	$(CC) $(CFLAGS) $(PFLAGS) -o $@ $<

.PHONY : clean

clean :
	rm -f *.o $(ALL)
