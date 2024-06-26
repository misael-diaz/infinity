#	CSERIES.LIB
#	--------------------------------------------------------------
#	Thursday, July 11, 1991 11:30:44 AM
#	Saturday, January 15, 1994 2:04:56 PM

# Define our object file directory, and give a directory dependency rule
Obj= :Objects:
{Obj} � :

COptions= -opt full -b2 -r -mbg on -d DEBUG -k {CSeriesLibraries}
#COptions= -opt full -b2 -r -mbg off -k {CSeriesLibraries}
OBJECTS= {Obj}macintosh_utilities.c.o {Obj}my32bqd.c.o {Obj}rle.c.o {Obj}preferences.c.o {Obj}proximity_strcmp.c.o �
	{Obj}temporary_files.c.o {Obj}devices.c.o {Obj}dialogs.c.o

macintosh_cseries.h � cseries.h

### Portable
{Obj}proximity_strcmp.c.o � proximity_strcmp.h cseries.h cseries.lib.make
{Obj}rle.c.o � rle.h cseries.h cseries.lib.make

### Macintosh
{Obj}macintosh_utilities.c.o � macintosh_cseries.h cseries.lib.make
{Obj}my32bqd.c.o � my32bqd.h macintosh_cseries.h cseries.lib.make
{Obj}devices.c.o � macintosh_cseries.h cseries.lib.make
{Obj}dialogs.c.o � macintosh_cseries.h cseries.lib.make
{Obj}preferences.c.o � preferences.h macintosh_cseries.h cseries.lib.make
{Obj}temporary_files.c.o � temporary_files.h macintosh_cseries.h cseries.lib.make

cseries.lib �� {OBJECTS}
	Lib -o cseries.lib {OBJECTS}
	Move -y cseries.lib {CSeriesLibraries}

cseries.lib �� {CSeriesInterfaces}cseries.h {CSeriesInterfaces}my32bqd.h {CSeriesInterfaces}rle.h �
	{CSeriesInterfaces}preferences.h {CSeriesInterfaces}macintosh_cseries.h {CSeriesInterfaces}proximity_strcmp.h �
	{CSeriesInterfaces}temporary_files.h {CSeriesInterfaces}cseries.a {CSeriesInterfaces}macintosh_interfaces.c

{CSeriesInterfaces}cseries.h � cseries.h
	Duplicate -y cseries.h {CSeriesInterfaces}
{CSeriesInterfaces}cseries.a � cseries.a
	Duplicate -y cseries.a {CSeriesInterfaces}
{CSeriesInterfaces}my32bqd.h � my32bqd.h
	Duplicate -y my32bqd.h {CSeriesInterfaces}
{CSeriesInterfaces}rle.h � rle.h
	Duplicate -y rle.h {CSeriesInterfaces}
{CSeriesInterfaces}preferences.h � preferences.h
	Duplicate -y preferences.h {CSeriesInterfaces}
{CSeriesInterfaces}macintosh_cseries.h � macintosh_cseries.h
	Duplicate -y macintosh_cseries.h {CSeriesInterfaces}
{CSeriesInterfaces}proximity_strcmp.h � proximity_strcmp.h
	Duplicate -y proximity_strcmp.h {CSeriesInterfaces}
{CSeriesInterfaces}temporary_files.h � temporary_files.h
	Duplicate -y temporary_files.h {CSeriesInterfaces}
{CSeriesInterfaces}macintosh_interfaces.c � macintosh_interfaces.c
	Duplicate -y macintosh_interfaces.c {CSeriesInterfaces}macintosh_interfaces.c
