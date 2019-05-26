# rg-2-midi
Command line tool for **Mac** and **Linux** for exporting Rosegarden
files to MIDI files.

The code in this repository is adapted from the Rosegarden project:

  https://www.rosegardenmusic.com/

Rg-2-midi was created by lifting out the minimal subset of code files
from Rosegarden that were needed to support the reading of .rg files
and the exporting of MIDI files, then gluing them to together with a
small bit of code in `main.cpp`.  Additionally, certain sections of
code were commented out to remove dependencies not necessary for the
task at hand, though nothing was added.

### License

The Rosegarden project is an open-source GPLv2-licensed project,
hence this project is also GPLv2, including the new code added to
it that was not taken from Rosegarden.

### Sample Usage

```
$ rg2midi /path/to/sample.rg /path/three/sample.mid
```

### How to Build

First ensure that you have basic C/C++ compiler tools installed on your
system (`sudo apt install build-essential` should do that for you on a
Linux `apt`-based system such as Ubuntu, Debian, Mint, etc.).  On Mac
you would need to install X-Code.  You will also need CMake installed.
If that is not installed then install it via your package manager (should
work on either Linux or Mac).  Then make sure that you have have the
following dependencies:

1. Qt5 Core
2. Qt5 Xml
3. zlib

On Linux you can find these in your package manager and on Mac you could
use e.g. `sudo port install qt5` (that is what the author does).  Once
you have that, then:

```
$ git clone https://github.com/dpacbach/rg-2-midi rg-2-midi
$ cd rg-2-midi
$ mkdir build
$ cd build
$ cmake .. -DCMAKE_BUILD_TYPE=Release
$ make -j4
```

Then you will find the binary in `build/src/rg2midi` which you can then
copy to wherever you like, though keep in mind that it will need the
relevant Qt5 and zlib libraries present in standard locations on the system
in order to run.
