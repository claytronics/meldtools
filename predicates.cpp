
#include <cstdlib>
#include <iostream>

#include "vm/program.hpp"

using namespace vm;
using namespace process;
using namespace std;

int
main(int argc, char **argv)
{
   if(argc != 2) {
      fprintf(stderr, "usage: predicates <bytecode file>\n");
      return EXIT_FAILURE;
   }
   
   const string file(argv[1]);
   int i;    

   program prog(file);
   
   prog.print_predicates(cout);

   prog.print_predicate_dependency(); 
   
   return EXIT_SUCCESS;
}
