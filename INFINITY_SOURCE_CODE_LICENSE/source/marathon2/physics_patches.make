#   File:       physics_patches.make
#   Target:     physics_patches
#   Sources:    physics_patches.c
#   Created:    Tuesday, October 25, 1994 10:34:03 PM


COptions= -i {CSeriesInterfaces} -opt full -b2 -r -mbg on -d DEBUG -d PREPROCESSING_CODE -d TERMINAL_EDITOR -mc68020 -k {CSeriesLibraries}

OBJECTS = �
		physics_patches.c.o
		
physics_patches �� physics_patches.make  {OBJECTS}
	Link �
		-t 'MPST' �
		-c 'MPS ' �
		{OBJECTS} �
		"{CLibraries}"StdClib.o �
 		"{Libraries}"Runtime.o �
 		"{Libraries}"Interface.o �
		":Objects:Game:68k:Beta:wad.lib" �
		{CSeriesLibraries}cseries.debug.lib �
		-o physics_patches
		
physics_patches.c.o � export_definitions.make extensions.h