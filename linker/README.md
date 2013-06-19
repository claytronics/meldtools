The linker will combine meld files and external .o files into a single
meld bytecode file that can be run.  We will implement this in two phases.

## Phase 1

Combine a single input meld bytecode file with an external
definition file, and a set of .o files into an output modified
bytecode file which will contain pointers to the .o files which can be
run by the meld vm.

+ input: a meld bytecode file (.m), a definition file (.md)
+ output: a "linked" meld bytecode file (.ml)

The job of the linker is to create a consistent set of names for the
external functions and some new bytecodes which instruct the vm to
dynamically load the extenal .o files at compile time and associate
them with the appropriate identifiers to the vm can execute the
external functions properly.

+ The .m file is produced by the meld compiler
+ The .md file is created by the progammer.  It will include function
names, type signatures, and a set of .o filenames that have the
compiled external functions.
+ The .ml file is created by the linker.




