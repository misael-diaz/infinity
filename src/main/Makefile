#!/usr/bin/make
#
# BDX						December 05, 2023
#
# source: src/kind/Makefile
# author: @misael-diaz
#
# Synopsis:
# Defines the Makefile for building the program with GNU make.
#
# Copyright (c) 2023 Misael Diaz-Maldonado
# This file is released under the GNU General Public License as published
# by the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#

include make-inc

all: $(BIN)

$(BIN): $(MAIN_O)
	$(CC) $(CCOPT) $(MAIN_O) -o $(BIN)

$(MAIN_O): $(MAIN_C)
	$(CC) $(CCOPT) -c $(MAIN_C) -o $(MAIN_O)

clean:
	/bin/rm -f *.o *.bin
