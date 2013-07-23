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

//#include "linker.hpp"

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

#define VERSION_AT_LEAST(MAJ, MIN) (major_version > (MAJ) || (major_version == (MAJ) && minor_version >= (MIN)))

#include "vm/program.hpp"
#include "interface.hpp"

using namespace vm;
using namespace process;
using namespace std;

static char *bytefile1;
static char *bytefile2;
static bool print = false;
static program* primary;
static program* secondary;

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

int linkExternalFunctions() {

#if 0
    // .m file
    std::ifstream infile_m(argv[1],std::ifstream::binary);

    // .md file
    std::ifstream infile_md;
    infile_md.open(argv[2]);

    // .ml file
    std::ofstream outfile_ml(argv[3],std::ofstream::binary);

    infile_m.seekg(0,infile_m.end);
    long size_tot = infile_m.tellg();
    infile_m.seekg(0);

    cout <<"total file size : "<<size_tot<<" bytes"<<endl;

    /*
    //get file size
    infile_m.seekg(0,infile_m.end);
    long size = infile_m.tellg();
    infile_m.seekg(0);
    cout<<"input.m size : "<<size<<" bytes"<<endl;

    //alloc mem
    char *buffer = new char[size];

    //read
    infile_m.read(buffer,size);

    //write
    outfile_ml.write(buffer,size);

    delete[] buffer;
    infile_m.close();
    infile_md.close();
    outfile_ml.close();

    return 0;
     */

    position = 0;
    position_wr = 0;
    position_prev = 0; 
    cout << "program : init"<<endl; 

    if(!infile_md.is_open()){
        cout<<"Error : cannot open *.md file\n";
        return 0;
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
        return 0;
    }  

    // read / write magic
    uint32_t magic1, magic2;
    READ_CODE(&magic1, sizeof(magic1));
    READ_CODE(&magic2, sizeof(magic2));

    if(magic1 != MAGIC1 || magic2 != MAGIC2){
        cout<<"Error : not a meld file\n";
        infile_m.close();
        infile_md.close();
        outfile_ml.close();
        return 0;     
    }

    // read / write version
    uint32_t major_version, minor_version;
    READ_CODE(&major_version, sizeof(uint32_t));
    READ_CODE(&minor_version, sizeof(uint32_t));

   if(!VERSION_AT_LEAST(0, 5) || VERSION_AT_LEAST(0, 10)){
        cout<<"Error : unsupported byte code version\n";
        infile_m.close();
        infile_md.close();
        outfile_ml.close();
        return 0;
    }

    cout <<"major_version : "<< major_version <<endl;
    cout <<"minor_version : "<< minor_version <<endl;


    // read number of predicates
    byte buf[PREDICATE_DESCRIPTOR_SIZE];
    READ_CODE(buf, sizeof(byte));

    // skip nodes
    uint_val num_nodes;
    READ_CODE(&num_nodes, sizeof(uint_val));

    SEEK_CODE(num_nodes * 8);

   // read imported/exported predicates
   if(VERSION_AT_LEAST(0, 9)) {
      uint32_t number_imported_predicates;

      READ_CODE(&number_imported_predicates, sizeof(number_imported_predicates));

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

         cout << "import " << buf_imp << " as " << buf_as << " from " << buf_file << endl;
      }

      uint32_t number_exported_predicates;

      READ_CODE(&number_exported_predicates, sizeof(number_exported_predicates));

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

    // get rule information
    uint_val n_rules;
    READ_CODE(&n_rules, sizeof(uint_val));

    for(size_t i(0); i < n_rules; ++i) {
        // read rule string length
        uint_val rule_len;

        READ_CODE(&rule_len, sizeof(uint_val));

        assert(rule_len > 0);

        char str[rule_len + 1];

        READ_CODE(str, sizeof(char) * rule_len);

        str[rule_len] = '\0';
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

    const_code = new byte_code_el[const_code_size];

    READ_CODE(const_code, const_code_size);

    // copy bytes from start to ml file
    COPY_TO_OUTPUT(position_prev,position);
    position_prev = position;


    if(VERSION_AT_LEAST(0, 6)) {
        // get function code
        uint_val n_functions;

        READ_CODE(&n_functions, sizeof(uint_val));

        cout<<"n_functions : "<<n_functions<<endl;

        for(size_t i(0); i < n_functions; ++i) {
            code_size_t fun_size;

            READ_CODE(&fun_size, sizeof(code_size_t));
            byte_code fun_code(new byte_code_el[fun_size]);
            READ_CODE(fun_code, fun_size);

            COPY_TO_OUTPUT(position_prev,position);
            position_prev = position;

        }

        if(major_version > 0 || minor_version >= 7) {
            // get external functions definitions
            uint_val n_externs;

            READ_CODE(&n_externs, sizeof(uint_val));

            cout << "n_externs : " <<n_externs<<endl;

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
                    cout << field_type_string(type) << ", ";
                }
                cout << endl;

                position_prev = position;
            }
        }
    }

    //get remaining byte size
    long size_rem = size_tot - infile_m.tellg();

    // copy remaining bytes without further ado
    COPY_TO_OUTPUT(position_prev,(position_prev + size_rem));

    outfile_ml.close();
    infile_m.close();
    infile_md.close();


    return 0;
#endif
}


// table lookup
char* get_filename(char *func_name,struct func_table* table,int size){

#if 0
    int i;
    for(i = 0; i < size;i++){

        if(!strcmp(func_name,table[i].func_name))
            return table[i].file_name;
    }
#endif
    return NULL;

}

void linkerStageOne(){

size_t position(0);
   
   ifstream fp(filename.c_str(), ios::in | ios::binary);
   
   if(!fp.is_open())
      throw load_file_error(filename, "unable to open file");

   // read magic
   uint32_t magic1, magic2;
   READ_CODE(&magic1, sizeof(magic1));
   READ_CODE(&magic2, sizeof(magic2));
   if(magic1 != MAGIC1 || magic2 != MAGIC2)
      throw load_file_error(filename, "not a meld byte code file");

   // read version
   READ_CODE(&major_version, sizeof(uint32_t));
   READ_CODE(&minor_version, sizeof(uint32_t));
   if(!VERSION_AT_LEAST(0, 5))
      throw load_file_error(filename, string("unsupported byte code version"));

   if(VERSION_AT_LEAST(0, 10))
      throw load_file_error(filename, string("unsupported byte code version"));

   // read number of predicates
   byte buf[PREDICATE_DESCRIPTOR_SIZE];
   
    READ_CODE(buf, sizeof(byte));
   
   const size_t num_predicates = (size_t)buf[0];
   
   predicates.resize(num_predicates);
   code_size.resize(num_predicates);
   code.resize(num_predicates);

   // skip nodes
   uint_val num_nodes;
    READ_CODE(&num_nodes, sizeof(uint_val));

    SEEK_CODE(num_nodes * database::node_size);

   // read imported/exported predicates
   if(VERSION_AT_LEAST(0, 9)) {
      uint32_t number_imported_predicates;

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

         imported_predicates.push_back(new import(buf_imp, buf_as, buf_file));
      }
      assert(imported_predicates.size() == number_imported_predicates);

      uint32_t number_exported_predicates;

      READ_CODE(&number_exported_predicates,
sizeof(number_exported_predicates));

      for(uint32_t i(0); i < number_exported_predicates; ++i) {
         uint32_t str_size;
         READ_CODE(&str_size, sizeof(str_size));
         char buf[str_size + 1];
         READ_CODE(buf, str_size);
         buf[str_size] = '\0';
         exported_predicates.push_back(string(buf));
      }
      assert(exported_predicates.size() == number_exported_predicates);
   }

    // get number of args needed
    byte n_args;

    READ_CODE(&n_args, sizeof(byte));
    num_args = (size_t)n_args;

   // get rule information
   uint_val n_rules;

   READ_CODE(&n_rules, sizeof(uint_val));

   number_rules = n_rules;

   for(size_t i(0); i < n_rules; ++i) {
      // read rule string length
      uint_val rule_len;

      READ_CODE(&rule_len, sizeof(uint_val));

      assert(rule_len > 0);

      char str[rule_len + 1];

      READ_CODE(str, sizeof(char) * rule_len);

      str[rule_len] = '\0';

      rules.push_back(new rule((rule_id)i, string(str)));
   }

    // read string constants
    int_val num_strings;
    READ_CODE(&num_strings, sizeof(int_val));
    
    default_strings.reserve(num_strings);
    
    for(int i(0); i < num_strings; ++i) {
        int_val length;
        
        READ_CODE(&length, sizeof(int_val));
        
        char str[length + 1];
        READ_CODE(str, sizeof(char) * length);
        str[length] = '\0';
        default_strings.push_back(runtime::rstring::make_default_string(str));
    }
    
    // read constants code
    uint_val num_constants;
    READ_CODE(&num_constants, sizeof(uint_val));
    
    // read constant types
    const_types.resize(num_constants);
    
    for(uint_val i(0); i < num_constants; ++i) {
        byte b;
        READ_CODE(&b, sizeof(byte));
        const_types[i] = (field_type)b;
    }
    
    // read constants code
    READ_CODE(&const_code_size, sizeof(code_size_t));
    
    const_code = new byte_code_el[const_code_size];
    
    READ_CODE(const_code, const_code_size);

   MAX_STRAT_LEVEL = 0;

   if(VERSION_AT_LEAST(0, 6)) {
      // get function code
      uint_val n_functions;

      READ_CODE(&n_functions, sizeof(uint_val));

      functions.resize(n_functions);

      for(size_t i(0); i < n_functions; ++i) {
         code_size_t fun_size;

         READ_CODE(&fun_size, sizeof(code_size_t));
         byte_code fun_code(new byte_code_el[fun_size]);
         READ_CODE(fun_code, fun_size);

         functions[i] = new vm::function(fun_code, fun_size);
      }

    //init functions defined in external namespace
    init_external_functions();

      if(major_version > 0 || minor_version >= 7) {
         // get external functions definitions
         uint_val n_externs;

         READ_CODE(&n_externs, sizeof(uint_val));

         for(size_t i(0); i < n_externs; ++i) {
            uint_val extern_id;

            READ_CODE(&extern_id, sizeof(uint_val));
            char extern_name[256];

            READ_CODE(extern_name, sizeof(extern_name));

            char skip_filename[1024];

            READ_CODE(skip_filename, sizeof(skip_filename));

            ptr_val skip_ptr;

            READ_CODE(&skip_ptr, sizeof(skip_ptr));

            //dlopen call
            //dlsym call
            skip_ptr = get_function_pointer(skip_filename,extern_name);

            uint_val num_args;

            READ_CODE(&num_args, sizeof(num_args));

            byte b;
            READ_CODE(&b,sizeof(byte)); 
            field_type ret_type = (field_type)b;

            cout << "Id " << extern_id << " " << extern_name << " ";
            cout <<"Num_args "<<num_args<<endl;

            field_type arg_type[num_args];
            if(num_args){

                for(uint_val j(0); j != num_args; ++j) {
                    byte b;
                    READ_CODE(&b, sizeof(byte));
                    arg_type[j] = (field_type)b;
                    cout << field_type_string(arg_type[j]) << " ";
                }

            add_external_function((external_function_ptr)skip_ptr,num_args,ret_type,arg_type);             
            }else
            add_external_function((external_function_ptr)skip_ptr,0,ret_type,NULL);
            cout << endl;
         }
      }
   }

   // read predicate information
   for(size_t i(0); i < num_predicates; ++i) {
      code_size_t size;

        READ_CODE(buf, PREDICATE_DESCRIPTOR_SIZE);
      
      predicates[i] = predicate::make_predicate_from_buf((unsigned char*)buf,
&size, (predicate_id)i);
      code_size[i] = size;

      MAX_STRAT_LEVEL = max(predicates[i]->get_strat_level() + 1,
MAX_STRAT_LEVEL);
        
      if(predicates[i]->is_route_pred())
         route_predicates.push_back(predicates[i]);
   }

   safe = true;
   for(size_t i(0); i < num_predicates; ++i) {
      predicates[i]->cache_info(this);
      if(predicates[i]->is_aggregate() && predicates[i]->is_unsafe_agg()) {
         safe = false;
        }
   }

    // get global priority information
    byte global_info;
    
    READ_CODE(&global_info, sizeof(byte));

   initial_priority.int_priority = 0;
   initial_priority.float_priority = 0.0;

   is_data_file = false;
    
   switch(global_info) {
      case 0x01: { // priority by predicate
         cerr << "Not supported anymore" << endl;
         assert(false);
      }
      break;
      case 0x02: { // normal priority
         byte type(0x0);
         byte asc_desc;

         READ_CODE(&type, sizeof(byte));
         switch(type) {
            case 0x01: priority_type = FIELD_FLOAT; break;
            case 0x02: priority_type = FIELD_INT; break;
            default: assert(false);
         }

         READ_CODE(&asc_desc, sizeof(byte));
         priority_order = (asc_desc ? PRIORITY_ASC : PRIORITY_DESC);

         if(priority_type == FIELD_FLOAT)
            READ_CODE(&initial_priority.float_priority, sizeof(float_val));
         else
            READ_CODE(&initial_priority.int_priority, sizeof(int_val));
      }
      break;
      case 0x03: { // data file
         is_data_file = true;
      }
      break;
      default:
      priority_type = FIELD_FLOAT; 
      priority_order = PRIORITY_DESC;
      break;
   }
   
   // read predicate code
   for(size_t i(0); i < num_predicates; ++i) {
      const size_t size = code_size[i];
      code[i] = new byte_code_el[size];
      
        READ_CODE(code[i], size);
   }

   // read rules code
    uint_val num_rules_code;
    READ_CODE(&num_rules_code, sizeof(uint_val));

   assert(num_rules_code == number_rules);

   for(size_t i(0); i < num_rules_code; ++i) {
      code_size_t code_size;
      byte_code code;

      READ_CODE(&code_size, sizeof(code_size_t));

      code = new byte_code_el[code_size];

      READ_CODE(code, code_size);

      rules[i]->set_bytecode(code_size, code);

      byte is_persistent(0x0);

      READ_CODE(&is_persistent, sizeof(byte));

      if(is_persistent == 0x1)
         rules[i]->set_as_persistent();

      uint_val num_preds;

      READ_CODE(&num_preds, sizeof(uint_val));

      assert(num_preds < 10);

      for(size_t j(0); j < num_preds; ++j) {
         predicate_id id;
         READ_CODE(&id, sizeof(predicate_id));
         predicate *pred(predicates[id]);

         pred->affected_rules.push_back(i);
         rules[i]->add_predicate(pred);
      }
   }

   data_rule = NULL;



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

    primary = new program(file);
    // read import file names
    cout << "Import Files...\n";

    for(i = 0; i < primary->num_imported_predicates(); i++){

        cout << primary->get_imported_predicate(i)->get_file() << endl; 
        secondary = new program(primary->get_imported_predicate(i)->get_file());
        linkerStageOne();
        delete secondary;

    }
 
    delete primary;
    return EXIT_SUCCESS;
}





