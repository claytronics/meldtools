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

using namespace std;


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


int main(int argc,char **argv) {

    if(argc != 4){

        cout<<"Format Error ! \nCorrect Format : " <<argv[0]<<" [.m file] [.md file] [.ml file]\n";
        return 0;

    }

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

    if(major_version == 0 && minor_version < 5){
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


    if(major_version > 0 || (major_version == 0 && minor_version >= 6)) {
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




