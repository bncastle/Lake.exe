## Lake.exe - Build Instructions

There are a few things to note when building Lake:

 - You need to have Lua 5.x installed and in your path.
 - There 2 sets of libs included, one for using MinGW (for Windows) and another using the VS2013 compiler.
 - MinGW produces an XP compatible binary and is also a bit smaller than the MSVC version. Newer MSVC compilers do not produce XP-compatible binaries.

To Build the exe, open a command line, change to the src directory, and type:

		lua lake

