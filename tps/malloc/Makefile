CFLAGS := -ggdb3 -O2 -Wall -Wextra -std=gnu11
CFLAGS += -Wmissing-prototypes
LIBFLAGS := -shared -fPIC $(CFLAGS)

# To compile using different strategies:
# - For First Free
#     make -B -e USE_FF=true
# - For Best Free
#     make -B -e USE_BF=true
ifdef USE_FF
	CFLAGS += -D FIRST_FIT
endif
ifdef USE_BF
	CFLAGS += -D BEST_FIT
endif

ifdef DEBUG
	CFLAGS += -D DEBUG
endif

EXECD := test-d
EXECS := test-s
LIBNAME := libmalloc.so
SRCS := $(wildcard *.c)
OBJS := $(SRCS:%.c=%.o)
LOGS := $(wildcard *.log)

all: $(EXECS) $(EXECD)

$(EXECS): $(OBJS)
	cc $(CFLAGS) -o $@ $^

$(EXECD): $(filter-out malloc.o, $(OBJS))
	cc $(CFLAGS) -o $@ $^

$(LIBNAME): malloc.c printfmt.o
	cc $(LIBFLAGS) -o $@ $^

run-d: $(EXECD) $(LIBNAME)
	./run-d.sh

run-s: $(EXECS)
	./run-s.sh

format: .clang-files .clang-format
	xargs -r clang-format -i <$<

clean:
	rm -f *.o $(EXECS) $(EXECD) $(LIBNAME) $(LOGS)

.PHONY: clean format run-d run-s
