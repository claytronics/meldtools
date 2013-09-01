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

#define READ_CODE_TMP(TO, SIZE) do { \
    temp_in.read((char *)(TO), SIZE);	\
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

std::vector<string> meld_file;
std::vector<vm::program*> secondary_list;

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

//modify predicate and rule code for given program
void 
modify_code(vm::program* prog){


        // allowing changes to bytecode
        prog->set_modify(true);

        cout << "Modifying code for... " << prog->get_name() << endl;

        // modify rule code
        for(size_t i(0); i < prog->num_rules();i++){

            cout<<"rule : "<<i<<endl;
            cout<<prog->get_rule(i)->get_string()<<endl;

            prog->get_rule(i)->print(cout_null,prog);
        }

        //modify predicate code
        for(size_t i(0) ; i < prog->num_predicates(); i++){

            cout<<"predicate : "<<i<<endl;
            cout<< prog->get_predicate(i)->get_name()<<endl;

            prog->print_predicate_code(cout_null,prog->get_predicate(i));
        }

        // disallow changes to bytecode
        prog->set_modify(false);
}

// check if primary predicate is imported
bool 
is_imported(size_t id){
    
    string name = primary->get_predicate(id)->get_name();

    for(size_t i(0); i < primary->num_imported_predicates(); i++){

        if(!name.compare(primary->get_imported_predicate(i)->get_as()))
            return true; 
    }

    return false;
}

// get modified id of primary imported predicate
predicate_id 
get_imported_predicate_id(predicate_id id){

    string name = primary->get_predicate(id)->get_name();
    string name_;
    bool import = false;

    for(size_t i(0); i < primary->num_imported_predicates(); i++){

        if(!name.compare(primary->get_imported_predicate(i)->get_as())){
            import = true;
            name_ = primary->get_imported_predicate(i)->get_imp();
     
        }
    }

    if(import){
        for(size_t j(0); j < secondary_list.size(); j++){
//            for(size_t i(COMMON_PREDICATES); i < secondary_list[j]->num_predicates();i++){
            vm::predicate* pred;
//                if(!name_.compare(secondary_list[j]->get_predicate(i)->get_name().c_str())) 
                if((pred = secondary_list[j]->get_predicate_by_name(name_)) != NULL) 
                    return pred->get_linker_id();         
//            }
        }
    }
    return id;
}

// main linker function
void linker(){

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

    // read external function info
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
    size_t num_imported_predicates = 0;
    size_t num_rules_new = 0;
    size_t num_common_predicates;    

    // count predicates,rules from import file
    for(uint32_t i(0); i < secondary_list.size(); i++){
    
    num_imported_predicates = num_imported_predicates + secondary_list[i]->num_predicates();
    num_rules_new = num_rules_new + secondary_list[i]->num_rules() - 1;

    }     
    
    num_common_predicates = (COMMON_PREDICATES) * secondary_list.size();
    
    const size_t num_predicates_new = num_predicates_orig + num_imported_predicates - num_common_predicates - primary->num_imported_predicates();
    buf[0] = (byte)(num_predicates_new);   
    WRITE_CODE(buf,sizeof(byte));   

    cout << "Original predicates : " << num_predicates_orig << endl;
    cout << "New predicates : " << num_predicates_new << endl;    

    // reset last byte read pointer 
    position_prev = position;

    // num nodes
    uint_val num_nodes;
    READ_CODE(&num_nodes, sizeof(uint_val));


    // get file with max nodes
    int max = num_nodes;
    int index = -1;    

    for(size_t i(0); i < secondary_list.size(); i++){
        
        if(secondary_list[i]->get_num_nodes() > max){
            max = secondary_list[i]->get_num_nodes();
            index = i;
        }        

    }
    
    if(index != -1){
        size_t num_nodes_new = secondary_list[index]->get_num_nodes();
        WRITE_CODE(&num_nodes_new,sizeof(uint_val));

        uint_val new_size = num_nodes_new * database::node_size;    

        WRITE_CODE(secondary_list[index]->get_buffer(),new_size);   
    }
    else{
        size_t num_nodes_new = primary->get_num_nodes();
        WRITE_CODE(&num_nodes_new,sizeof(uint_val));

        uint_val new_size = num_nodes_new * database::node_size;    

        WRITE_CODE(primary->get_buffer(),new_size);   
    }

    SEEK_CODE(num_nodes * database::node_size);

    position_prev = position;

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

    // add rules info from import file, include init from imported file
    n_rules_new = n_rules_orig + num_rules_new;
    WRITE_CODE(&n_rules_new, sizeof(uint_val));   

    cout << "No of source rules : " << n_rules_orig << endl;
    cout << "No of import rules : " << num_rules_new << endl;    

    position_prev = position;

    //add init rules from primary and secondary files

    size_t rule_offset = 0;

/*
    // primary program init rule
    {   
        uint_val rule_len;

        READ_CODE(&rule_len, sizeof(uint_val));

        assert(rule_len > 0);

        char str[rule_len + 1];

        READ_CODE(str, sizeof(char) * rule_len);
        
        str[rule_len] = '\0';

//        primary->get_rule(RULE0)->set_linker_id(rule_offset);
//        rule_offset++;
    }

//    COPY_TO_OUTPUT(position_prev,position);
    position_prev = position;
*/

/*
    // add init rule from import files
    for(uint32_t i(0); i < secondary_list.size(); i++){
    // read rule string length
    uint_val rule_len;

    rule_len = strlen(secondary_list[i]->get_rule(RULE0)->get_string().c_str());
    WRITE_CODE(&rule_len, sizeof(uint_val));

    assert(rule_len > 0);
    // get string and append  
    const char *str = secondary_list[i]->get_rule(RULE0)->get_string().c_str();

    WRITE_CODE(str,rule_len);       

    secondary_list[i]->get_rule(RULE0)->set_linker_id(rule_offset);
    rule_offset++;
    }

*/

    // add primary rules
    for(size_t i(0); i < n_rules_orig; ++i) {
        // read rule string length
        uint_val rule_len;

        READ_CODE(&rule_len, sizeof(uint_val));

        assert(rule_len > 0);

        char str[rule_len + 1];

        READ_CODE(str, sizeof(char) * rule_len);

        str[rule_len] = '\0';

        primary->get_rule(i)->set_linker_id(rule_offset);
        rule_offset++;
    }

    COPY_TO_OUTPUT(position_prev,position);
    position_prev = position;

    // add remaining rules from secondary programs, skipping init
    for(size_t j(0); j < secondary_list.size();j++){
        for(size_t i(1); i < secondary_list[j]->num_rules(); ++i) {
            // read rule string length
            uint_val rule_len;

            rule_len = strlen(secondary_list[j]->get_rule(i)->get_string().c_str());
            WRITE_CODE(&rule_len, sizeof(uint_val));

            assert(rule_len > 0);
            // get string and append  
            const char *str = secondary_list[j]->get_rule(i)->get_string().c_str();

            WRITE_CODE(str,rule_len);       

            secondary_list[j]->get_rule(i)->set_linker_id(rule_offset);
            rule_offset++;
        }
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

    COPY_TO_OUTPUT(position_prev,position);
    position_prev = position;

    if(VERSION_AT_LEAST(0, 6)) {
        // get function code
        uint_val n_functions,n_functions_new;

        READ_CODE(&n_functions, sizeof(uint_val));
        position_prev = position;

        n_functions_new = 0;
        for(size_t i(0); i < secondary_list.size(); i++){
            n_functions_new = n_functions_new + secondary_list[i]->get_num_functions();
        }

        cout << "n_functions : " << n_functions << endl;
        cout << "n_functions_new : " << n_functions_new << endl;

        uint_val n_functions_total = n_functions + n_functions_new;
        WRITE_CODE(&n_functions_total,sizeof(uint_val));

        for(size_t i(0); i < n_functions; ++i) {
            code_size_t fun_size;

            READ_CODE(&fun_size, sizeof(code_size_t));
            byte_code fun_code(new byte_code_el[fun_size]);
            READ_CODE(fun_code, fun_size);
            delete fun_code;
        }

        COPY_TO_OUTPUT(position_prev,position);
        position_prev = position;

        for(size_t i(0); i < secondary_list.size(); i++){
            for(size_t j(0); j < secondary_list[i]->get_num_functions(); j++){
                code_size_t fun_size = secondary_list[i]->get_function(j)->get_bytecode_size();
                WRITE_CODE(&fun_size, sizeof(code_size_t));
                WRITE_CODE(secondary_list[i]->get_function(j)->get_bytecode(),fun_size);

            }
        }

        //init functions defined in external namespace
        //        init_external_functions();

        if(maj > 0 || min >= 7) {
            // get external functions definitions
            uint_val n_externs,n_externs_new;

            READ_CODE(&n_externs, sizeof(uint_val));
            position_prev = position;

            n_externs_new = 0;
            for(size_t i(0); i < secondary_list.size(); i++){

                n_externs_new = n_externs_new + secondary_list[i]->get_num_ext_functions();

            }

            cout << "n_externs : " << n_externs << endl;
            cout << "n_externs_new : " << n_externs_new << endl;       

            uint_val n_externs_total = n_externs + n_externs_new;
            WRITE_CODE(&n_externs_total, sizeof(uint_val));

            // primary
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
                    memcpy(skip_filename,file_name,strlen(file_name));
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

            //secondary
            size_t new_extern_id = n_externs;        
            for(size_t k(0) ; k < secondary_list.size() ; k++){

                cout << "linking external functions" << endl;
                cout << "Opening ..." << secondary_list[k]->get_name().c_str() << endl;

                std::ifstream temp_in(secondary_list[k]->get_name().c_str(),std::ifstream::binary);
                temp_in.seekg(secondary_list[k]->get_ext_function_position(), ios_base::beg);

                for(size_t i(0); i < secondary_list[k]->get_num_ext_functions(); ++i) {

                    uint_val extern_id;

                    READ_CODE_TMP(&extern_id, sizeof(uint_val));
                    cout << "old external id : " << extern_id << endl;
                    cout << "new external id : " << new_extern_id << endl;

                    WRITE_CODE(&new_extern_id,sizeof(uint_val));

                    char extern_name[256];

                    READ_CODE_TMP(extern_name, sizeof(extern_name));
                    WRITE_CODE(extern_name, sizeof(extern_name));

                    char skip_filename[1024];
                    char *file_name = get_filename(extern_name,table,num_of_lines);

                    READ_CODE_TMP(skip_filename, sizeof(skip_filename));

                    cout<<"skip_filename : "<<skip_filename<<endl;

                    if(file_name){
                        cout<<"external function : "<<extern_name<<" found !\n";
                        cout << "filename : "<<file_name<<endl;
                        memcpy(skip_filename,file_name,strlen(file_name));
                    } 
                    else{
                        cout<<"external function : "<<extern_name<<" not defined in .md file\n";
                        memcpy(skip_filename,dummy_path,sizeof(dummy_path));
                    }

                    WRITE_CODE(skip_filename,sizeof(skip_filename));

                    ptr_val skip_ptr;

                    READ_CODE_TMP(&skip_ptr, sizeof(skip_ptr));
                    WRITE_CODE(&skip_ptr,sizeof(skip_ptr));

                    uint_val num_args;

                    READ_CODE_TMP(&num_args, sizeof(num_args));
                    WRITE_CODE(&num_args,sizeof(num_args));

                    cout << "Id : " << new_extern_id << " Name :" << extern_name << endl;
                    cout<<"Num args : "<<num_args<<endl;
                    new_extern_id++;    

                    for(uint_val j(0); j != num_args + 1; ++j) {
                        byte b;
                        READ_CODE_TMP(&b, sizeof(byte));
                        WRITE_CODE(&b,sizeof(byte));

                        field_type type = (field_type)b;
                        cout << field_type_string_(type) << ", ";
                    }
                    cout << endl;

                }

                temp_in.close();
            }

        }
    }

    COPY_TO_OUTPUT(position_prev,position);
    position_prev = position;    


    size_t offset = 0;

    // read source predicate information, avoid imported predicates
    for(size_t i(0); i < num_predicates_orig; ++i) {
        
        READ_CODE(buf, PRED_DESC_SZ);

        if(!is_imported(i)){
            COPY_TO_OUTPUT(position_prev,position);
            primary->get_predicate(i)->set_linker_id(offset);
            offset++;      
        }
        else
            primary->get_predicate(i)->set_linker_id(DEFAULT_ID);  

        position_prev = position;
    }

    // offset used to number secondary predicates
    assert(offset == (primary->num_predicates()-primary->num_imported_predicates()));

/*
    // add init predicate from secondary files
    for(size_t j(0); j < secondary_list.size(); j++){

        // write buffer
        string filename = secondary_list[j]->get_name();
        secondary_list[j]->get_predicate(0)->rename(filename);
        WRITE_CODE(secondary_list[j]->get_predicate(0)->get_desc_buffer(),PRED_DESC_SZ);
        secondary_list[j]->get_predicate(0)->set_linker_id(0);
        offset++;
        
    }
*/
    // add import predicate information/change predicate names
    for(size_t j(0); j < secondary_list.size();j++){
        for(size_t i(COMMON_PREDICATES) ; i < secondary_list[j]->num_predicates() ; i++) {
            // write buffer
            string filename = secondary_list[j]->get_name();
            secondary_list[j]->get_predicate(i)->rename(filename);
            WRITE_CODE(secondary_list[j]->get_predicate(i)->get_desc_buffer(),PRED_DESC_SZ);
            secondary_list[j]->get_predicate(i)->set_linker_id(offset);
            offset++;
        }
    }


    //link primary imported predicate ID
    for(size_t i(0); i < num_predicates_orig; ++i) {
        
        if(is_imported(i)){
            size_t id = get_imported_predicate_id(i); 
            primary->get_predicate(i)->set_linker_id(id);
        }
    } 

    // modify rule and predicate code for primary and secondary programs  
    modify_code(primary);

    for(size_t i(0); i < secondary_list.size(); i++){
    
    modify_code(secondary_list[i]);

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

    COPY_TO_OUTPUT(position_prev,position);
    position_prev = position;

    // read predicate code, avoid imported predicates
    for(size_t i(0); i < primary->num_predicates(); ++i) {
        const size_t size = primary->get_predicate_bytecode_size(i);
        byte_code code = new byte_code_el[size];

        READ_CODE(code, size);
        
        if(!is_imported(i))
            WRITE_CODE(primary->get_predicate_bytecode(i),size);  
//            COPY_TO_OUTPUT(position_prev,position);
            
        position_prev = position;

        delete code;
    }

/*
    // write init predicate code for import files
    for(size_t j(0); j < secondary_list.size();j++){
        const size_t size = secondary_list[j]->get_predicate_bytecode_size(0);
        
        WRITE_CODE(secondary_list[j]->get_predicate_bytecode(0),size);
    }
*/
    // write import predicate code    
    for(size_t j(0); j < secondary_list.size();j++){
        for(size_t i(COMMON_PREDICATES); i < secondary_list[0]->num_predicates(); i++){
            const size_t size = secondary_list[j]->get_predicate_bytecode_size(i);

            WRITE_CODE(secondary_list[j]->get_predicate_bytecode(i),size);
        }
    }

    // read rules code, avoid multiple init rule
    uint_val num_rules_code_orig,num_rules_code_new;

    READ_CODE(&num_rules_code_orig, sizeof(uint_val));

    num_rules_code_new = num_rules_code_orig + num_rules_new; 
    WRITE_CODE(&num_rules_code_new,sizeof(uint_val));     
    position_prev = position;

    

/*
    //rule0 of primary program
    {

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
        
        COPY_TO_OUTPUT(position_prev,position);
        position_prev = position;

        for(size_t j(0); j < num_preds; ++j) {
            predicate_id id,id_imported;
            READ_CODE(&id, sizeof(predicate_id));
            id_imported = get_imported_predicate_id(id); 

            WRITE_CODE(&id_imported,sizeof(predicate_id));
            position_prev = position;
        }

        delete code;

    }
*/
/*
    { 
        code_size_t code_size;
        byte_code code;

        code_size = primary->get_rule(RULE0)->get_codesize();
        code = primary->get_rule(RULE0)->get_bytecode();        

        WRITE_CODE(&code_size, sizeof(code_size_t));

        WRITE_CODE(code, code_size);

        byte is_persistent = primary->get_rule(RULE0)->as_persistent();

        WRITE_CODE(&is_persistent, sizeof(byte));

        uint_val num_preds = primary->get_rule(RULE0)->num_predicates();

        WRITE_CODE(&num_preds, sizeof(uint_val));

        assert(num_preds < 10);

        for(size_t j(0); j < num_preds; ++j) {
            predicate_id id = primary->get_rule(RULE0)->get_predicate_number(j)->get_linker_id();
            WRITE_CODE(&id, sizeof(predicate_id));
        }
    }
*/
/*
    //rule0 of secondary programs

    for(size_t k(0); k < secondary_list.size();k++){

        code_size_t code_size;
        byte_code code;

        code_size = secondary_list[k]->get_rule(RULE0)->get_codesize();
        code = secondary_list[k]->get_rule(RULE0)->get_bytecode();        

        WRITE_CODE(&code_size, sizeof(code_size_t));

        WRITE_CODE(code, code_size);

        byte is_persistent = secondary_list[k]->get_rule(RULE0)->as_persistent();

        WRITE_CODE(&is_persistent, sizeof(byte));

        uint_val num_preds = secondary_list[k]->get_rule(RULE0)->num_predicates();

        WRITE_CODE(&num_preds, sizeof(uint_val));

        assert(num_preds < 10);

        for(size_t j(0); j < num_preds; ++j) {
            predicate_id id = secondary_list[k]->get_rule(RULE0)->get_predicate_number(j)->get_linker_id();
            WRITE_CODE(&id, sizeof(predicate_id));
        }

    }
*/

/*
    // remaining primary rules
    for(size_t i(1); i < num_rules_code_orig; ++i) {
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
        
        COPY_TO_OUTPUT(position_prev,position);
        position_prev = position;

        for(size_t j(0); j < num_preds; ++j) {
            predicate_id id,id_imported;
            READ_CODE(&id, sizeof(predicate_id));
            id_imported = get_imported_predicate_id(id); 

            WRITE_CODE(&id_imported,sizeof(predicate_id));
            position_prev = position;
        }

        delete code;
    }
*/

    for(size_t i(0); i < primary->num_rules(); ++i) {
        code_size_t code_size;
        byte_code code;

        code_size = primary->get_rule(i)->get_codesize();
        code = primary->get_rule(i)->get_bytecode();        

        WRITE_CODE(&code_size, sizeof(code_size_t));

        WRITE_CODE(code, code_size);

        byte is_persistent = primary->get_rule(i)->as_persistent();

        WRITE_CODE(&is_persistent, sizeof(byte));

        uint_val num_preds = primary->get_rule(i)->num_predicates();

        WRITE_CODE(&num_preds, sizeof(uint_val));

        assert(num_preds < 10);

        for(size_t j(0); j < num_preds; ++j) {
            predicate_id id = primary->get_rule(i)->get_predicate_number(j)->get_linker_id();
            WRITE_CODE(&id, sizeof(predicate_id));
        }
    }


    //remaining secondary rules
    for(size_t k(0); k < secondary_list.size();k++){
        for(size_t i(1); i < secondary_list[k]->num_rules(); ++i) {
            code_size_t code_size;
            byte_code code;

            code_size = secondary_list[k]->get_rule(i)->get_codesize();
            code = secondary_list[k]->get_rule(i)->get_bytecode();        

            WRITE_CODE(&code_size, sizeof(code_size_t));

            WRITE_CODE(code, code_size);

            byte is_persistent = secondary_list[k]->get_rule(i)->as_persistent();

            WRITE_CODE(&is_persistent, sizeof(byte));

            uint_val num_preds = secondary_list[k]->get_rule(i)->num_predicates();

            WRITE_CODE(&num_preds, sizeof(uint_val));

            assert(num_preds < 10);

            for(size_t j(0); j < num_preds; ++j) {
                predicate_id id = secondary_list[k]->get_rule(i)->get_predicate_number(j)->get_linker_id();
                WRITE_CODE(&id, sizeof(predicate_id));
            }
        }
    }

    delete[] buffer;
    infile_m.close();
    infile_md.close();
    outfile_ml.close();

}

//checks if import is valid
bool
checkExport(string predicate){

    for(size_t i(0); i < secondary->num_exported_predicates();i++){
        
       if(!predicate.compare(secondary->get_exported_predicate(i))){
            cout << "import " << predicate << " valid\n";
            return true; 

        }
    }

    return false;
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

    string file(bytefile1);
    int i;    

    primary = new program(file);

    // read import file names
    cout << "Import Files...\n";

    //For multiple import files
    for(i = 0; i < primary->num_imported_predicates();i++){

        string import_file = primary->get_imported_predicate(i)->get_file(); 
        cout << "importing from ... " << import_file << endl;

        if (std::find(meld_file.begin(),meld_file.end(), import_file) != meld_file.end())
        {
           cout << import_file << " already processed\n"; 
           continue;
        }  
    
        meld_file.push_back(import_file);
    
    }

    //list of import programs
    for(i = 0 ; i < meld_file.size(); i++){
        
        string import_file = meld_file[i];
        cout << "importing from ... " << import_file << endl;
        
        secondary = new program(import_file);
        secondary_list.push_back(secondary);

    }

    // TODO : support for multiple import files
    linker();

    primary->print_code = true;
//    primary->print_predicate_dependency();
/*
    for(i = 0; i < primary->num_imported_predicates();i++){

        cout << primary->get_imported_predicate(i)->get_file() << endl;
        secondary = new program(primary->get_imported_predicate(i)->get_file());
        checkExport(primary->get_imported_predicate(i)->get_imp());
        linker();

    }
*/
//    delete secondary;    
    delete primary;

    for(i = 0; i < secondary_list.size(); i++){

    delete secondary_list[i];

    }

    return EXIT_SUCCESS;
}





