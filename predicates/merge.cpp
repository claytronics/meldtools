#include <cstdlib>
#include <iostream>

#include <fstream>
#include <sys/stat.h>
#include <iostream>
#include <boost/static_assert.hpp>
#include <string>
#include <stdint.h>
#include<assert.h>
#include<stdio.h>
#include<string.h>
#include<algorithm>

#include "linker.hpp"
#include "vm/program.hpp"
#include "db/tuple.hpp"
#include "vm/instr.hpp"
#include "db/database.hpp"
#include "utils/types.hpp"
#include "vm/state.hpp"
#include "version.hpp"

#define READ_CODE(TO, SIZE) do { \
    infile_m.read((char *)(TO), SIZE);	\
    position += (SIZE);				\
} while(false)

#define SEEK_CODE(SIZE) do { \
    infile_m.seekg(SIZE, ios_base::cur);	\
    position += (SIZE);	\
} while(false)

#define WRITE_CODE(FROM, SIZE) do { \
    outfile_ml.write((char *)(FROM), SIZE);	\
    position_wr += (SIZE);				\
} while(false)

#define COPY_TO_OUTPUT(FROM,TO) do { \
    infile_m.seekg(FROM);        \
    buffer = new char[TO - FROM];    \
    infile_m.read(buffer,(TO - FROM));  \
    infile_m.seekg(TO); \
    WRITE_CODE(buffer,(TO - FROM)); \
} while(false)

#define VERSION_AT_LEAST(MAJ, MIN) (maj > (MAJ) || (maj == (MAJ) && min >= (MIN)))
#define PRED_DESC_SZ 104

using namespace vm;
using namespace process;
using namespace std;
using namespace db;
using namespace vm::instr;
using namespace process;
using namespace utils;

static char *bytefile1;
static char *m_file;
static char *md_file;
static char *ml_file;

static bool print = false;
static program* primary;
static program* secondary;

std::ostream cout_null(0);
code_size_t const_code_size;

std::string
field_type_string_(field_type type)
{
    switch(type) {
        case FIELD_INT: return "int";
        case FIELD_FLOAT: return "float";
        case FIELD_NODE: return "node";
        case FIELD_LIST_INT: return "int list";
        case FIELD_LIST_FLOAT: return "float list";
        case FIELD_LIST_NODE: return "node list";
        case FIELD_STRING: return "string";
        default:
                           return "Unrecognized field type"; 
    }

    assert(false);
    return "";
}

static void help()
{

    cerr << "merge: execute linker program" << endl;
    cerr << "./merge -fm <.m input file> -fd <.md input file> -fl <.ml output file>" << endl;
    exit(EXIT_SUCCESS);
}

static void read_args(int argc, char **argv)
{

    argv++;
    --argc;

    while (argc > 0 && (argv[0][0] == '-')) {
        switch(argv[0][1]) {
            case 'f' :{
                          switch(argv[0][2]){                      
                              case 'm' : bytefile1 = argv[1];
                                         m_file = argv[1];
                                         break;

                              case 'd' : md_file = argv[1];
                                         break;

                              case 'l' : ml_file = argv[1];
                                         break;    

                              default : break;
                          }     
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

// table lookup
char* get_filename(char *func_name,struct func_table* table,int size){

    int i;
    for(i = 0; i < size;i++){

        if(!strcmp(func_name,table[i].func_name))
            return table[i].file_name;
    }
    return NULL;

}

//relinking predicate dependencies
void 
modify_code(){

    secondary->enable_modify();

    cout << "Modifying code for....\n";

    // modify rule code
    for(size_t i(0); i < secondary->num_rules();i++){

        cout<<"rule : "<<i<<endl;
        cout<<secondary->get_rule(i)->get_string()<<endl;

        secondary->get_rule(i)->print(cout_null,secondar b vc);
    }

    //modify predicate code
    for(i = 0 ; i < secondary->num_predicates(); i++){

        cout<<"predicate : "<<i<<endl;
        cout<<secondary->get_predicate(i)->name<<endl;

        secondary->print_predicate_code(cout_null,get_predicate(i));
    }

    secondary->disable_modify();
}


void linkerStageOne(){

    // .m file
    std::ifstream infile_m(m_file,std::ifstream::binary);

    // .md file
    std::ifstream infile_md;
    infile_md.open(md_file);

    // .ml file
    std::ofstream outfile_ml(ml_file,std::ofstream::binary);

    infile_m.seekg(0,infile_m.end);
    long size_tot = infile_m.tellg();
    infile_m.seekg(0);

    cout <<"total file size : "<<size_tot<<" bytes"<<endl;

    position = 0;
    position_wr = 0;
    position_prev = 0; 
    cout << "merging .m files......"<<endl; 

    if(!infile_md.is_open()){
        cout<<"Error : cannot open *.md file\n";
        return;
    }

    // read num of lines in .md file , equal to num of functions defined
    num_of_lines = count(istreambuf_iterator<char>(infile_md),
            istreambuf_iterator<char>(),'\n');

    cout << "num of external functions : "<<num_of_lines<<endl;

    // create table of func_names and file_names
    struct func_table table[num_of_lines];

    // reset file pointer
    infile_md.seekg(0);
    int n = 0;

    while(n < num_of_lines){
        char buf[MAX_CHARS_PER_LINE];
        infile_md.getline(buf,MAX_CHARS_PER_LINE);


        const char* token[MAX_TOKENS_PER_LINE] = {};

        token[0] = strtok(buf,DELIMITER);

        if(token[0]){

            //copy func name        
            memcpy(table[n].func_name,token[0],strlen(token[0])+1);   

            //ignore argument
            token[1] = strtok(0,DELIMITER);

            // copy file path
            token[2] = strtok(0,DELIMITER);
            memcpy(table[n].file_name,token[2],strlen(token[2])+1);   

            cout << "func_name : "<<table[n].func_name<<endl;
            cout << "func_arg : "<<token[1]<<endl;
            cout << "file_name : "<<table[n].file_name<<endl;
        }
        n++;    

    }

    if(!infile_m.is_open()){
        cout<<"Error : cannot open *.m file\n"; 
    }

    // read magic
    uint32_t magic1, magic2;
    READ_CODE(&magic1, sizeof(magic1));
    READ_CODE(&magic2, sizeof(magic2));

    // read version
    uint32_t maj,min;
    READ_CODE(&maj, sizeof(uint32_t));
    READ_CODE(&min, sizeof(uint32_t));


    COPY_TO_OUTPUT(position_prev,position);


    // read number of predicates
    utils::byte buf[PRED_DESC_SZ];

    READ_CODE(buf, sizeof(byte));

    const size_t num_predicates_orig = (size_t)buf[0];

    // add predicates from import file
    const size_t num_predicates_new = num_predicates_orig + secondary->num_predicates();
    buf[0] = (byte)(num_predicates_new - COMMON_PREDICATES);   
    WRITE_CODE(buf,sizeof(byte));   

    cout << "No of source predicates : " << num_predicates_orig << endl;
    cout << "No of import predicates : " << (secondary->num_predicates() -
COMMON_PREDICATES) << endl;    

    // reset last byte read pointer 
    position_prev = position;


    // skip nodes
    uint_val num_nodes;
    READ_CODE(&num_nodes, sizeof(uint_val));

    SEEK_CODE(num_nodes * database::node_size);


    uint32_t number_imported_predicates;
    // read imported/exported predicates
    if(VERSION_AT_LEAST(0, 9)) {

        READ_CODE(&number_imported_predicates,
                sizeof(number_imported_predicates));

        for(uint32_t i(0); i < number_imported_predicates; ++i) {
            uint32_t size;
            READ_CODE(&size, sizeof(size));

            char buf_imp[size + 1];
            READ_CODE(buf_imp, size);
            buf_imp[size] = '\0';

            READ_CODE(&size, sizeof(size));
            char buf_as[size + 1];
            READ_CODE(buf_as, size);
            buf_as[size] = '\0';

            READ_CODE(&size, sizeof(size));
            char buf_file[size + 1];
            READ_CODE(buf_file, size);
            buf_file[size] = '\0';

            cout << "import " << buf_imp << " as " << buf_as << " from " <<
                buf_file << endl;

        }

        uint32_t number_exported_predicates;

        READ_CODE(&number_exported_predicates,
                sizeof(number_exported_predicates));

        for(uint32_t i(0); i < number_exported_predicates; ++i) {
            uint32_t str_size;
            READ_CODE(&str_size, sizeof(str_size));
            char buf[str_size + 1];
            READ_CODE(buf, str_size);
            buf[str_size] = '\0';

            cout << "export " << buf << endl;
        }
    }

    // get number of args needed
    byte n_args;

    READ_CODE(&n_args, sizeof(byte));

    // copy from position_prev to position
    COPY_TO_OUTPUT(position_prev,position);
    position_prev = position;

    // get rule information
    uint_val n_rules_orig,n_rules_new;

    READ_CODE(&n_rules_orig, sizeof(uint_val));

    cout << endl << " Adding Rules ..." << endl;

    // add rules info from import file
    n_rules_new = n_rules_orig + secondary->num_rules();
    WRITE_CODE(&n_rules_new, sizeof(uint_val));   

    cout << "No of source rules : " << n_rules_orig << endl;
    cout << "No of import rules : " << secondary->num_rules() << endl;    

    position_prev = position;

    for(size_t i(0); i < n_rules_orig; ++i) {
        // read rule string length
        uint_val rule_len;

        READ_CODE(&rule_len, sizeof(uint_val));

        assert(rule_len > 0);

        char str[rule_len + 1];

        READ_CODE(str, sizeof(char) * rule_len);

        str[rule_len] = '\0';

    }

    COPY_TO_OUTPUT(position_prev,position);
    position_prev = position;

    // add rules from import file
    for(size_t i(0); i < secondary->num_rules(); ++i) {
        // read rule string length
        uint_val rule_len;

        rule_len = strlen(secondary->get_rule(i)->get_string().c_str());
        WRITE_CODE(&rule_len, sizeof(uint_val));

        assert(rule_len > 0);
        // get string and append  
        const char *str = secondary->get_rule(i)->get_string().c_str();

        WRITE_CODE(str,rule_len);       

    }

    // read string constants
    int_val num_strings;
    READ_CODE(&num_strings, sizeof(int_val));


    for(int i(0); i < num_strings; ++i) {
        int_val length;

        READ_CODE(&length, sizeof(int_val));

        char str[length + 1];
        READ_CODE(str, sizeof(char) * length);
        str[length] = '\0';
    }

    // read constants code
    uint_val num_constants;
    READ_CODE(&num_constants, sizeof(uint_val));

    for(uint_val i(0); i < num_constants; ++i) {
        byte b;
        READ_CODE(&b, sizeof(byte));
    }

    // read constants code
    READ_CODE(&const_code_size, sizeof(code_size_t));

    byte_code const_code = new byte_code_el[const_code_size];

    READ_CODE(const_code, const_code_size);
    delete const_code;

    if(VERSION_AT_LEAST(0, 6)) {
        // get function code
        uint_val n_functions;

        READ_CODE(&n_functions, sizeof(uint_val));

        for(size_t i(0); i < n_functions; ++i) {
            code_size_t fun_size;

            READ_CODE(&fun_size, sizeof(code_size_t));
            byte_code fun_code(new byte_code_el[fun_size]);
            READ_CODE(fun_code, fun_size);
            delete fun_code;
        }

        //init functions defined in external namespace
        init_external_functions();

        if(maj > 0 || min >= 7) {
            // get external functions definitions
            uint_val n_externs;

            READ_CODE(&n_externs, sizeof(uint_val));

            for(size_t i(0); i < n_externs; ++i) {

                uint_val extern_id;

                READ_CODE(&extern_id, sizeof(uint_val));
                char extern_name[256];

                READ_CODE(extern_name, sizeof(extern_name));

                COPY_TO_OUTPUT(position_prev,position);

                char skip_filename[1024];
                char *file_name = get_filename(extern_name,table,num_of_lines);

                READ_CODE(skip_filename, sizeof(skip_filename));

                cout<<"skip_filename : "<<skip_filename<<endl;

                if(file_name){
                    cout<<"external function : "<<extern_name<<" found !\n";
                    cout << "filename : "<<file_name<<endl;
                    memcpy(skip_filename,table[1].file_name,strlen(file_name));
                } 
                else{
                    cout<<"external function : "<<extern_name<<" not defined in .md file\n";
                    memcpy(skip_filename,dummy_path,sizeof(dummy_path));
                }

                WRITE_CODE(skip_filename,sizeof(skip_filename));

                ptr_val skip_ptr;

                READ_CODE(&skip_ptr, sizeof(skip_ptr));
                WRITE_CODE(&skip_ptr,sizeof(skip_ptr));

                uint_val num_args;

                READ_CODE(&num_args, sizeof(num_args));
                WRITE_CODE(&num_args,sizeof(num_args));

                cout << "Id : " << extern_id << " Name :" << extern_name << endl;
                cout<<"Num args : "<<num_args<<endl;

                for(uint_val j(0); j != num_args + 1; ++j) {
                    byte b;
                    READ_CODE(&b, sizeof(byte));
                    WRITE_CODE(&b,sizeof(byte));

                    field_type type = (field_type)b;
                    cout << field_type_string_(type) << ", ";
                }
                cout << endl;

                position_prev = position;
            }
        }
    }

    // read source predicate information
    for(size_t i(0); i < num_predicates_orig; ++i) {

        READ_CODE(buf, PRED_DESC_SZ);

    }

    COPY_TO_OUTPUT(position_prev,position); 
    position_prev = position;

    size_t offset = primary->num_predicates();
    // add import predicate information/change predicate names
    for(size_t i(COMMON_PREDICATES) ; i < secondary->num_predicates() ; i++) {

        // write buffer
        WRITE_CODE(secondary->get_predicate(i)->get_desc_buffer(),PRED_DESC_SZ);
        secondary->get_predicate(i)->set_linker_id(offset);
        offset++;
    }

    // get global priority information
    byte global_info;

    READ_CODE(&global_info, sizeof(byte));

    switch(global_info) {
        case 0x01: { // priority by predicate
                       cerr << "Not supported anymore" << endl;
                       assert(false);
                   }
                   break;
        case 0x02: { // normal priority
                       byte type(0x0);
                       byte asc_desc;

                       //dummy hold values 
                       field_type priority_type;
                       int32_t int_value;  
                       double float_value;                     

                       READ_CODE(&type, sizeof(byte));
                       switch(type) {
                           case 0x01: priority_type = FIELD_FLOAT; break;
                           case 0x02: priority_type = FIELD_INT; break;
                           default: assert(false);
                       }

                       READ_CODE(&asc_desc, sizeof(byte));

                       if(priority_type == FIELD_FLOAT)
                           READ_CODE(&float_value, sizeof(float_val));
                       else
                           READ_CODE(&int_value, sizeof(int_val));
                   }
                   break;
        case 0x03: { // data file
                   }
                   break;
        default:
                   break;
    }

    // read predicate code
    for(size_t i(0); i < primary->num_predicates(); ++i) {
        const size_t size = primary->get_predicate_bytecode_size(i);
        byte_code code = new byte_code_el[size];

        READ_CODE(code, size);
        delete code;
    }

    COPY_TO_OUTPUT(position_prev,position);
    position_prev = position;

    // write import predicate code    
    for(size_t i(COMMON_PREDICATES); i < secondary->num_predicates(); i++){
        const size_t size = secondary->get_predicate_bytecode_size(i);

        WRITE_CODE(secondary->get_predicate_bytecode(i),size);
    }

    // read rules code
    uint_val num_rules_code_orig,num_rules_code_new;

    READ_CODE(&num_rules_code_orig, sizeof(uint_val));

    num_rules_code_new = num_rules_code_orig + secondary->num_rules(); 
    WRITE_CODE(&num_rules_code_new,sizeof(uint_val));     
    position_prev = position;

    for(size_t i(0); i < num_rules_code_orig; ++i) {
        code_size_t code_size;
        byte_code code;

        READ_CODE(&code_size, sizeof(code_size_t));

        code = new byte_code_el[code_size];

        READ_CODE(code, code_size);

        byte is_persistent(0x0);

        READ_CODE(&is_persistent, sizeof(byte));

        uint_val num_preds;

        READ_CODE(&num_preds, sizeof(uint_val));

        assert(num_preds < 10);

        for(size_t j(0); j < num_preds; ++j) {
            predicate_id id;
            READ_CODE(&id, sizeof(predicate_id));
        }

        delete code;
    }

    COPY_TO_OUTPUT(position_prev,position);
    position_prev = position;

    for(size_t i(0); i < secondary->num_rules(); ++i) {
        code_size_t code_size;
        byte_code code;

        code_size = secondary->get_rule(i)->get_codesize();
        code = secondary->get_rule(i)->get_bytecode();        

        WRITE_CODE(&code_size, sizeof(code_size_t));

        WRITE_CODE(code, code_size);

        byte is_persistent = secondary->get_rule(i)->as_persistent();

        WRITE_CODE(&is_persistent, sizeof(byte));

        uint_val num_preds = secondary->get_rule(i)->num_predicates();

        WRITE_CODE(&num_preds, sizeof(uint_val));

        assert(num_preds < 10);

        for(size_t j(0); j < num_preds; ++j) {
            predicate_id id = secondary->get_rule(i)->get_predicate_number(j)->get_linker_id();
            WRITE_CODE(&id, sizeof(predicate_id));
        }
    }

    delete[] buffer;
    infile_m.close();
    infile_md.close();
    outfile_ml.close();

}

int
main(int argc, char **argv)
{
    if(argc < 7) {
        help();  
    }

    read_args(argc,argv);

    cout << ".m file : " << m_file << endl;
    cout << ".md file : " << md_file << endl;
    cout << ".ml file : " << ml_file << endl;

    const string file(bytefile1);
    int i;    

    primary = new program(file);

    // read import file names
    cout << "Import Files...\n";
    for(uint32_t i(0); i < primary->num_imported_predicates(); i++){

        cout << primary->get_imported_predicate(i)->get_file() << endl; 
        secondary = new program(primary->get_imported_predicate(i)->get_file());
        linkerStageOne();
        delete secondary;

    }

    delete primary;
    return EXIT_SUCCESS;
}





