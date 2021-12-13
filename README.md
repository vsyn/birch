# Birch

Binary search with options for string, ints of any size, and standard C floats. All sizes and offsets in bits.

Attempts to find the "smallest" collection of one match from each search group. Collections will not span multiple directory tree branches.

Usage: `birch ROOTS... PATTERNS... [OPTIONS...]`

ROOTS: Pathnames at which to start the search, can be files or directories, if directories, a resursive search will be performed within.

PATTENRS: Of the form: "type size pattern".

type modifier | description
-- | --
f | float
i | int
s | string
a | aligned
u | unaligned
l | little endian
b | big endian
n | native endian
g | group with last

Data type, alignment and endian can all be combined, further args maintain type settings from previous.

Patterns example: `-ial 32 42 -gf 32 42`
a pattern group containing a 32 bit aligned little endian integer and a 32 bit aligned little endian float.

OPTIONS: `-r` number of results to print, default 1.

Example: `birch ./ -s 40 hello -ia 8 7 -gf 32 7 -gf 64 7`
will search the current directory for the closest grouping of the string "hello" and the number 7 represented as either an 8 bit integer, float or a double.
