#!/usr/bin/make
#
# Infinity				June 07, 2024
#
# source: Makefile
# author: @misael-diaz
#
# Synopsis:
# Defines the Makefile for building the program with GNU make.
#
# Copyright (c) 2024 Misael Diaz-Maldonado
# This file is released under the GNU General Public License as published
# by the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#

all: infinity

utils:
	@$(MAKE) -C util
infinity: utils
	@$(MAKE) -C main
clean:
	@$(MAKE) -C util clean
	@$(MAKE) -C main clean
