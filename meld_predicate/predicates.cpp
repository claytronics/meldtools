#include <cstdlib>
#include <iostream>

#include "vm/program.hpp"
#include "interface.hpp"

using namespace vm;
using namespace process;
using namespace std;

char *bytefile;
bool print = false;

static void help()
{

    cerr << "predicates: execute predicates program" << endl;
    cerr << "./predicates <program file> [optional -c]" << endl;
    exit(EXIT_SUCCESS);
}

static void read_args(int argc, char **argv)
{

    argv++;
    --argc;

    while (argc > 0 && (argv[0][0] == '-')) {
        switch(argv[0][1]) {
            case 'f' :{

                        bytefile = argv[1];
                        argc--;                            
                        argv++;
                        }            
                        break;

            case 'c': {
                          cout<<"printing code : \n";  
                          print = true;		
                          argc--;
                          argv++;
                      }
                      break;

            default:
                      help();
        }

        /* advance */
        argc--; argv++;
    }

}

int
main(int argc, char **argv)
{
    if(argc <= 2) {
        help();  
    }

    read_args(argc,argv);

    const string file(bytefile);
    int i;    

    program prog(file);

    if(print)
    prog.print_code = true;    
    else 
    prog.print_code = false;

//    prog.print_predicates(cout);

    prog.print_predicate_dependency(); 

    return EXIT_SUCCESS;
}

