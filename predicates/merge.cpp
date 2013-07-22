#include <cstdlib>
#include <iostream>

#include "vm/program.hpp"
#include "interface.hpp"

using namespace vm;
using namespace process;
using namespace std;

static char *bytefile1;
static char *bytefile2;
static bool print = false;

static void help()
{

    cerr << "merge: execute linker program" << endl;
    cerr << "./merge <program file> [optional -c]" << endl;
    exit(EXIT_SUCCESS);
}

static void read_args(int argc, char **argv)
{

    argv++;
    --argc;

    while (argc > 0 && (argv[0][0] == '-')) {
        switch(argv[0][1]) {
            case 'f' :{
                        bytefile1 = argv[1];
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

    const string file(bytefile1);
    int i;    

    program prog(file);

    // read import file names
    cout << "Import Files...\n";
    for(i = 0; i < prog.num_imported_predicates(); i++){
        cout << prog.get_imported_predicate(i)->get_file() << endl; 
    } 

/*
    if(print)
    prog.print_code = true;    
    else 
    prog.print_code = false;

    prog.print_predicate_dependency(); 
*/
    return EXIT_SUCCESS;
}

