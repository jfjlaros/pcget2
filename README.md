# PC to Amiga link
This document covers a simple data cable and the software to transfer ADF
images from a PC directly to an Amiga diskette.


## Background
This project is a modified version of Greg Craske's
[PC-to-Amiga link](https://web.archive.org/web/20060613201604/http://goanna.cs.rmit.edu.au/~craske/amiga)
project. The modifications made are marked in de source code by the tag: `DISK
WRITE`. The modified version can write directly to an unformatted Amiga
diskette, which may be necessary if no temporary space (like a hard drive) is
not present on the Amiga.

Support for unformatted diskettes is based on `hd2f.c` from Markus Wandel, see
his [Amiga Software](http://wandel.ca/homepage/amiga_sw/) page for more
information.

For other PC to Amiga link software, see the
[A.S.T. Amiga PAGE](https://web.archive.org/web/20031203001841/http://homepage.uibk.ac.at:80/homepage/c725/c72578/amiga).


## Hardware configuration
See the
[original pages](https://web.archive.org/web/20060613201604/http://goanna.cs.rmit.edu.au/~craske/amiga)
for more information on how to make the parallel cable.


## Installation
Retrieve the source code with Git.

    git clone https://github.com/jfjlaros/pcget2.git

Compile the Linux binary:

    cd pcget2
    gcc -O2 -o amigaput amigaput.c

To compile the Amiga binary, use
[DICE](https://virtuallyfun.com/2013/01/11/dice-c-compiler-for-the-amiga/)
either on the Amiga itself, or in an emulator like
[UAE](https://en.wikipedia.org/wiki/UAE_(emulator)).


## Usage
This version works exactly like the
[original one](https://web.archive.org/web/20060613201604/http://goanna.cs.rmit.edu.au/~craske/amiga)
on the PC side. For the Amiga, the `pcget` binary has an additional command
line option `-w` to write directly to a diskette.

    pcget -w

`pcget` will ask to change diskette (running this program from diskette is
recommended because it uses quite some memory otherwise). Then it will format
the newly inserted diskette, filling it with data provided by `amigaput` 20
tracks at a time. Note that this will overwrite all data on the diskette.

If `pcget` does not work for some reason,  the `pcget_debug` program can be
used for diagnostic purposes. It writes 5 tracks at a time, reads them back and
compares the tracks. If a mismatch is found, an error message is displayed.


## Known bugs
- `pcget` reports the file size to be 0 when using the `-w` option.
- `pcget` writes everything to diskette when using the `-w` option. No checks
  on file size or headers are performed.
