The linker will combine meld files and external .o files into a single
meld bytecode file that can be run.  We will implement this in two phases.

### Phase 1

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

### Phase 2

Allow meld to support modularity.  This will involve the linker
putting together multiple .m files into a single .ml file.  We will
extend meld to support an "export" directive which will define which
predicates in the file are publically available.  Each predicate that
is exported will have its name available for an "import" directive.
The programmer can associate an imported name with a local name.  All
other predicates in a file will not be visible outside of the file.
The linker will rename predicates, keeping track of the mapping, so
that the debugger can still be used.

+ inputs: a set of .m files, a .md file
+ outputs: a .ml file, a .mm file.  

If there is only a single .m file then no predicate renaming will
happen.  If there is more than one input .m file, then the predicates
will get names based on the file they are defined in.  This mapping
will be included in the .ml file so that the debugger can display the
original names of the predicates.

To simplify things we will assume that if a module is imported by
multiple other modules that all the predicates are completely
duplicated and not-shared between the two instances of the imported
modules.

#### syntax

export Predicate.  This indicates that Predicate will be exported by
   this module.  It is given a local name which will be resolved by
   the linker by a matching import statement.

import Predicate1 as Predicate2 from filename.  This indicates that
       Predicate1 found in filename will be called Predicate2 in the
       localfile.  Predicate2 will have to be given a type.

To simplify things we will also put cpp in the compile loop, so that a
programmer can define a .mh file which contains type definitions and
the like. We can also develop a tool which will automatically generate
the .mh file for a .m file with export statements.




