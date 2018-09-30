Included below is from the front page of the originating sourceforge site, http://z8encore.sourceforge.net/

# Z8 Encore Tools
This site hosts a collection of command-line tools for use with the Z8 Encore microcontroller from Zilog.

### About
The set of Z8 Encore command-line tools consist of the following.

**ez8mon**
    This is a command-line debugger that is used during code development. This tool can download code to the device, set breakpoints, single step code, view memory, alter memory, disassemble code, and other tasks commonly found in debuggers.
**flashutil**
    A command-line utility to program code into the flash of a device. This program has a multi-pass mode which allows several devices to be programmed consecutively with little intervention. This tool could be used on a production assembly-line.
**crcgen**
    A command-line utility to calculate the CRC-CCITT checksum of data in an intel hexfile. This can be used to compare against the hardware CRC checksum returned by the on-chip debugger.

### Project Page
This project is hosted on sourceforge at
http://sourceforge.net/projects/z8encore.

#### Download
Windows binary distributions can be downloaded from
http://downloads.sourceforge.net/z8encore.

#### Source Code
The latest source code is available through CVS.

To grab the source code via CVS, you first need a CVS client.
TortoiseCVS is a popular windows CVS client. http://www.tortoisecvs.org/

The following command will checkout the source from the CVS repository.

cvs -d :pserver:anonymous@z8encore.cvs.sourceforge.net:/cvsroot/z8encore checkout dbgutils

#### Building
The source code has been built on windows, linux, and solaris.

The windows port is built using the mingw GNU gcc compiler from http://www.mingw.org.

You will also need a copy of the gnu readline library.
You can use the one from mingw at http://sf.net/projects/mingwrep/
You can also use the one from GnuWin32 at http://gnuwin32.sourceforge.net/

You will also need a copy of the TCL library.
Main TCL sourcecode website is at http://www.tcl.tk/
Windows binary distributions are made available from ActiveState, look for ActiveTcl.

