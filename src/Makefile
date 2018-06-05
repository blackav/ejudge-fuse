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

CC = gcc
CFLAGS = -Wall -g -Werror -std=gnu11 -D_FILE_OFFSET_BITS=64 -D_GNU_SOURCE -Wno-pointer-sign -pthread
LDLIBS = -lfuse -lcurl -lcrypto -lm

include files.make

OFILES = $(CFILES:.c=.o)

all : ejudge-fuse

deps.make : $(CFILES) $(HFILES)
	gcc $(CFLAGS) -MM $(CFILES) > deps.make

include deps.make

ejudge-fuse : $(OFILES)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o$@ $(LDLIBS)

clean :
	rm -f ejudge-fuse deps.make *.o
