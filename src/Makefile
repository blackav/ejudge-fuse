CC = gcc
CFLAGS = -Wall -g -Werror -std=gnu11 -D_FILE_OFFSET_BITS=64 -D_GNU_SOURCE -Wno-pointer-sign -pthread
LDLIBS = -lfuse -lcurl -lcrypto -lm

HFILES = cJSON.h contests_state.h ejfuse.h inode_hash.h ops_cnts_probs.h ops_contest.h ops_contest_info.h ops_contest_log.h ops_fuse.h ops_generic.h ops_root.h
CFILES = ejfuse.c cJSON.c contests_state.c inode_hash.c ops_cnts_probs.c ops_contest.c ops_contest_info.c ops_contest_log.c ops_fuse.c ops_generic.c ops_root.c

OFILES = $(CFILES:.c=.o)

all : ejfuse

deps.make : $(CFILES) $(HFILES)
	gcc $(CFLAGS) -MM $(CFILES) > deps.make

include deps.make

ejfuse : $(OFILES)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o$@ $(LDLIBS)

clean :
	rm -f ejfuse deps.make *.o

