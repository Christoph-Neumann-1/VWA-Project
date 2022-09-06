---
title: some more ideas on the linker
author: Christoph Neumann
---

# Supported/recognized formats:
- compiled native files
- bc files
- interface files(Describes function/struct signatures and is used to link to uncompiled native modules/bc files.)
- .vwa source files -> get turned into interface files when no compiled version is found
- .ni(native interface files) -> get turned into interface files as well as compiled into .native files

__Side note__:
add an option to take provide a source file and let the compiler figure out what else to compile

## compiled native files
Are shared objects providing a few functions for initializing, linking and unloading the module.

## Interface files
Very simple file specifying inputs and outputs of functions as well as exported structs. It says nothing about what version to use, or how that struct looks. It is just meant to speed things up.
Oh, it stores imports too.

## bc files
See their document, but it is a collection of function/struct definitions along with the implementation

## .vwa
Source code which gets turned into .int and .bc files

## .ni
Describes a native module in a human readable way, gets turned into a cpp file along with a .int(Might remove this later and generate this from the modules load function to account for user modifications)

