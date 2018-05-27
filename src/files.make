# Copyright (C) 2018 Alexander Chernov <cher@ejudge.ru> 

#
# This file is part of ejudge-fuse.
#
# Ejudge-fuse is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# Ejudge-fuse is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with ejudge-fuse.  If not, see <http://www.gnu.org/licenses/>.

HFILES = \
 ejfuse.h\
 cJSON.h\
 contests_state.h\
 ejfuse_file.h\
 ejudge.h\
 ejudge_client.h\
 inode_hash.h\
 ops_cnts.h\
 ops_cnts_info.h\
 ops_cnts_log.h\
 ops_cnts_prob_dir.h\
 ops_cnts_prob_files.h\
 ops_cnts_prob_runs_run.h\
 ops_cnts_prob_runs_run_files.h\
 ops_cnts_prob_runs.h\
 ops_cnts_probs.h\
 ops_cnts_prob_submit.h\
 ops_cnts_prob_submit_comp.h\
 ops_cnts_prob_submit_comp_dir.h\
 ops_fuse.h\
 ops_generic.h\
 ops_root.h\
 settings.h\
 submit_thread.h

CFILES = \
 ejfuse.c\
 cJSON.c\
 contests_state.c\
 ejfuse_file.c\
 ejudge.c\
 ejudge_client.c\
 ejudge_json.c\
 info_text.c\
 inode_hash.c\
 ops_cnts.c\
 ops_cnts_info.c\
 ops_cnts_log.c\
 ops_cnts_prob_dir.c\
 ops_cnts_prob_files.c\
 ops_cnts_prob_runs_run.c\
 ops_cnts_prob_runs_run_files.c\
 ops_cnts_prob_runs.c\
 ops_cnts_probs.c\
 ops_cnts_prob_submit.c\
 ops_cnts_prob_submit_comp.c\
 ops_cnts_prob_submit_comp_dir.c\
 ops_fuse.c\
 ops_generic.c\
 ops_root.c\
 submit_thread.c
