CC = gcc
CFLAGS = -Wall -g -Werror -std=gnu11 -D_FILE_OFFSET_BITS=64 -D_GNU_SOURCE -Wno-pointer-sign -pthread
LDLIBS = -lfuse -lcurl -lcrypto -lm

HFILES = cJSON.h contests_state.h ejfuse.h ejfuse_file.h ejudge_client.h inode_hash.h ops_cnts_prob_dir.h ops_cnts_prob_files.h ops_cnts_prob_runs.h ops_cnts_probs.h ops_cnts_prob_submit.h ops_cnts_prob_submit_compiler.h ops_cnts_prob_submit_compiler_dir.h ops_contest.h ops_contest_info.h ops_contest_log.h ops_fuse.h ops_generic.h ops_root.h submit_thread.h ejudge.h
CFILES = ejfuse.c cJSON.c contests_state.c ejfuse_file.c ejudge_client.c inode_hash.c ops_cnts_prob_dir.c ops_cnts_prob_files.c ops_cnts_prob_runs.c ops_cnts_probs.c ops_cnts_prob_submit.c ops_cnts_prob_submit_compiler.c ops_cnts_prob_submit_compiler_dir.c ops_contest.c ops_contest_info.c ops_contest_log.c ops_fuse.c ops_generic.c ops_root.c submit_thread.c ejudge.c

OFILES = $(CFILES:.c=.o)

all : ejfuse

deps.make : $(CFILES) $(HFILES)
	gcc $(CFLAGS) -MM $(CFILES) > deps.make

include deps.make

ejfuse : $(OFILES)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o$@ $(LDLIBS)

clean :
	rm -f ejfuse deps.make *.o

