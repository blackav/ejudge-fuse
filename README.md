# ejudge-fuse

ejudge-fuse is a FUSE (Linux filesystem-in-user-space) driver for ejudge contest management system (https://ejudge.ru).
ejudge-fuse maps most operations that a contestant usually performs during a contest to operations with files.
For example, the contestant lists his/her runs for a particular problem by requesting the directory listing
of the corresponding path. The contestant submits a solution by copying a source file to a specific directory
in the fuse-mounted ejudge filesystem.

## Compilation

### Prerequsites

ejudge-fuse is written in the C programming language, thus a relatively recent C compiler is required. GCC version 6 or later should do.
ejudge-fuse uses libcrypto library from openssl, libcurl and libfuse. Make sure that the developer's versions of the abovementioned
packages should be installed. For Redhat derived distributions of Linux (Fedora, RHEL, CentOS) the packages are named 'openssl-devel',
'libcurl-devel', 'libfuse-devel'.

### Compilation

```
cd src
make
```

As a result `ejudge-fuse` executable file should appear in the directory.

## Running

In addition to the standard fuse command line options the following ejudge-fuse options are supported:

* `--url` - specify the base URL for accessing ejudge
* `--user` - specify the user login for ejudge
* `--password` - specify the user's password (the registration one)

`--url` option must be specified in the command line. If `--user` or `--password` is not specified,
the corresponding data are read from the terminal. Terminal echo is disabled when the password is requested.

Note, that fuse-specific `use_ino` mount option must be enabled for correct operation.

A command line example is as follows:
```
./ejudge-fuse --user ejudge --url http://localhost/cgi-bin/ /mnt/ejudge -o use_ino
```

The password for user ejudge will be requested from terminal.

Here `http://localhost/cgi-bin/` is the base url to access ejudge. ejudge-fuse uses two URLs, namely `register` and `client`.
They must be available as `http://localhost/cgi-bin/register` and `http://localhost/cgi-bin/client`.

`/mnt/ejudge` is the mount point for the new filesystem. Files in the mounted filesysteam are accessible only to the user who
mounted it. Other users cannot access ejudge-fuse mounted files.

`-o use_ino` is an additional FUSE option required for correct operation of ejudge-fuse.

Filesystem unmounting is performed by `fusermount`, as usually:

```
fusermount -u /mnt/ejudge
```

## ejudge-fuse filesystem layout

Currently ejudge-fuse supports only unprivileged operations for contest participants. Moreover, only contests with contest-specific
contest passwords disabled are available to ejudge-fuse (see `disable_team_password` in XML contest configuration file).

The following paths are relative to the ejudge-fuse filesystem root. For example, human-readable contest information file `INFO` for
contest 1 has path `/1/INFO`. If ejudge-fuse is mounted to `/mnt/ejudge`, the full path will be `/mnt/ejudge/1/INFO`.

### Top-level contest listing

The root directory of the filesystem contains the list of contests, in which the authentificated user is allowed to participate.

```
$ ls -l /mnt/ejudge/
total 0
dr-x------ 2 cher cher 4096 Jun  5 18:22 10,Test contest (contest-specific actions)
dr-x------ 2 cher cher 4096 Jun  5 18:22 2,Test contest (Tokens)
dr-x------ 2 cher cher 4096 Jun  5 18:22 3,Test contest (variants)
dr-x------ 2 cher cher 4096 Jun  5 18:22 4,Test contest
dr-x------ 2 cher cher 4096 Jun  5 18:22 6,Test contest (xuser mongo)
dr-x------ 2 cher cher 4096 Jun  5 18:22 7,Test contest (Olympiad)
dr-x------ 2 cher cher 4096 Jun  5 18:22 8,Test contest (pending review)
dr-x------ 2 cher cher 4096 Jun  5 18:22 9,Test contest (no tester)
```

The contest directory entries contain the contest number and the contest name, separated by a colon. Note, however, that ejudge-fuse
driver ignores symbols after the comma. Thus, paths `/mnt/ejudge/4,Test contest` and `/mnt/ejudge/4` are actually the same.

```
$ stat /mnt/ejudge/4\,Test\ contest
  File: ‘/mnt/ejudge/4,Test contest’
  Size: 4096      	Blocks: 0          IO Block: 4096   directory
Device: 2fh/47d	Inode: 4           Links: 2
Access: (0500/dr-x------)  Uid: ( 1000/    cher)   Gid: ( 1000/    cher)
Access: 2018-06-05 18:26:27.639721000 +0300
Modify: 2018-06-05 18:22:48.486343000 +0300
Change: 2018-06-05 18:22:48.486343000 +0300
 Birth: -
$ stat /mnt/ejudge/4
  File: ‘/mnt/ejudge/4’
  Size: 4096      	Blocks: 0          IO Block: 4096   directory
Device: 2fh/47d	Inode: 4           Links: 2
Access: (0500/dr-x------)  Uid: ( 1000/    cher)   Gid: ( 1000/    cher)
Access: 2018-06-05 18:26:31.196111000 +0300
Modify: 2018-06-05 18:22:48.486343000 +0300
Change: 2018-06-05 18:22:48.486343000 +0300
 Birth: -
```

Note the same inode number 4 for both paths.

This rule holds for problem directory entries, run entries, language entries etc. Thus by requesting directory listing one may get
verbose information, but still access the files by short paths.

### Contest directory

The contest directory contains the following files and directories:

```
-r-------- 1 cher cher  448 Jun  5 18:22 INFO
-r-------- 1 cher cher 5064 Jun  5 18:22 info.json
-r-------- 1 cher cher   93 Jun  5 18:22 LOG
dr-x------ 2 cher cher 4096 Jun  5 18:22 problems
```

`INFO` is a human-readable contest description:

```
Contest information: 
    Name:               Test contest
    Type:               ACM
    Duration:           UNLIMITED
    Status:             RUNNING
    Start Time:         2015-01-01 00:00:00
    Elapsed Time:       3y5n6d18h31m3s
Server statistics: 
    Server time:        2018-06-05 18:31:03
    On-line users:      1
    Max On-line users:  1
    Max On-line Time:   2015-10-28 21:37:00
    Status update time: 2018-06-05 18:31:03.318054
```

`info.json` is the file as received from the server. It contains the contest description in JSON format.

`LOG` is the operations log. Operation failures are logged here.

`problems` is the directory containing the contest problems.

```
dr-x------ 2 cher cher 4096 Jun  5 18:22 A,Sum 1
dr-x------ 2 cher cher 4096 Jun  5 18:22 B,Sum 2
```

Note, as mentioned above, only the first part of the directory entiry name is mandatory, i.e. paths `A,Sum 1` and `A` are the same.

### Problem directory

The problem directory contains the following files and directories:

```
-r-------- 2 cher cher  521 Jun  5 18:34 INFO
-r-------- 2 cher cher  690 Jun  5 18:34 info.json
dr-x------ 2 cher cher 4096 Jun  5 18:22 runs
-r-------- 2 cher cher 1174 Jun  5 18:33 statement.html
dr-x------ 2 cher cher 4096 Jun  5 18:22 submit
```

`INFO` is a human-readable problem description

```
Problem information: 
    Short name:           A
    Long name:            Sum 1
    Type:                 standard
    Full Score:           100
    Input:                standard input
    Output:               standard output
    Time Limit (ms):      1000
    Real Time Limit (ms): 5000
    Max VM Size:          64M
    Max Stack Size:       64M
Your statistics: 
    Best RunId:           2
    Failed Attempts:      1
    All Attempts:         1
Server information: 
    Server time:          2018-06-05 18:34:50
```

`info.json` is the file as received from the server. It contains the problem description in JSON format.

`statement.html` is the problem statement.

`runs` is the directory with user runs for this problem.

`submit` is the submit directory.

### Submitting a solution

The `submit` directory has a subdirectory for each programming language enabled for this contest and problem.
In order to submit a solution the user copies a source file to the directory corresponding to the programming language.
For example:

```
cp solution.cpp /mnt/ejudge/4/problems/A/submit/g++
```

This command submits a solution `solution.cpp` to problem `A` written for compiler `g++`.

### Runs directory

`runs` directory contains the list of user's submits for this problem. Each directory entry consists of
run_id, run status, and either score, or failed test number, if such info is applicable. For example,

```
dr-x------ 2 cher cher 4096 Mar 20 20:26 1115,OK,98
dr-x------ 2 cher cher 4096 Feb 26 10:36 154,Ignored
dr-x------ 2 cher cher 4096 Feb 26 10:36 155,Compilation error
dr-x------ 2 cher cher 4096 Feb 26 10:44 156,Ignored
dr-x------ 2 cher cher 4096 Feb 26 10:48 157,Rejected
dr-x------ 2 cher cher 4096 Mar 16 19:26 780,Rejected
```

Only the run_id part is mandatory, status and score or failed test may be omitted.

### Run directory

A run directory contains files with information about the run status, source code, the compiler and the valuer outputs, etc.
The directory listing may look as follows:

```
-r-------- 2 cher cher  210 Jun  6 07:21 compiler.txt
-r-------- 2 cher cher  379 Jun  6 07:21 INFO
-r-------- 2 cher cher 3925 Jun  6 07:21 info.json
-r-------- 2 cher cher 2739 Mar 20 20:26 source.cpp
dr-x------ 2 cher cher 4096 Mar 20 20:26 tests
```

`INFO` is a human-readable problem description

`info.json` is the file as received from the server. It contains the problem description in JSON format.

`compiler.txt` is the compiler output.

`source.cpp` is the source file. The file has basename `source` and the suffix depending on the programming language
specified when this run was submitted.

`tests` is the directory with tests data. It is available only if the corresponding option is set in the ejudge config configuration file.
The directory contains a subdirectory for each test, for example:

```
dr-x------ 2 cher cher 4096 Mar 20 20:26 1
dr-x------ 2 cher cher 4096 Mar 20 20:26 2
dr-x------ 2 cher cher 4096 Mar 20 20:26 3
dr-x------ 2 cher cher 4096 Mar 20 20:26 4
dr-x------ 2 cher cher 4096 Mar 20 20:26 5
dr-x------ 2 cher cher 4096 Mar 20 20:26 6
```

### Test directory

A test directory contains files with the input data, correct answer, program output, program standard error stream and checker output, as shown below:

```
-r-------- 1 cher cher  3 Mar 20 20:26 checker
-r-------- 1 cher cher 70 Mar 20 20:26 correct
-r-------- 1 cher cher  0 Mar 20 20:26 error
-r-------- 1 cher cher  0 Mar 20 20:26 input
-r-------- 1 cher cher 70 Mar 20 20:26 output
```
