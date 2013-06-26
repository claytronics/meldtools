#include <fstream>
#include <sys/stat.h>
#include <iostream>
#include <boost/static_assert.hpp>
#include <string>
#include <stdint.h>
#include<assert.h>

#include "linker.hpp"


static const size_t PREDICATE_DESCRIPTOR_SIZE = 104;/* sizeof(code_size_t) +
                                                PRED_DESCRIPTOR_BASE_SIZE +
                                                PRED_ARGS_MAX +
                                                PRED_NAME_SIZE_MAX +
                                                PRED_AGG_INFO_MAX;*/


using namespace std;
void copy_to_output(int from,int to);

code_size_t const_code_size;

#define READ_CODE(TO, SIZE) do { \
	infile_m.read((char *)(TO), SIZE);	\
	position += (SIZE);				\
} while(false)

#define SEEK_CODE(SIZE) do { \
	infile_m.seekg(SIZE, ios_base::cur);	\
	position += (SIZE);	\
} while(false)

#define WRITE_CODE(TO, SIZE) do { \
	outfile_ml.write((char *)(TO), SIZE);	\
	position_wr += (SIZE);				\
} while(false)

#define COPY_TO_OUTPUT(FROM,TO) do { \
    infile_m.seekg(FROM);        \
    size = infile_m.tellg();     \
    cout<<"input.m position : "<<position<<" size : "<<size<<endl;  \
    buffer = new char[TO - FROM];    \
    infile_m.read(buffer,(TO - FROM));  \
    infile_m.seekg(TO); \
    WRITE_CODE(buffer,(TO - FROM)); \
} while(false)


int main() {


std::ifstream infile_m("test.m",std::ifstream::binary);
std::ifstream infile_md("def.md",std::ifstream::binary);

std::ofstream outfile_ml("output.m",std::ofstream::binary);


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
   
   if(!infile_m.is_open()){
        cout<<"Error : cannot open file\n"; 
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
   
    cout <<"PREDICATE_DESCRIPTOR_SIZE : "<<PREDICATE_DESCRIPTOR_SIZE<<endl;

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
            position_prev = position;

            char skip_filename[1024];

            READ_CODE(skip_filename, sizeof(skip_filename));

            ptr_val skip_ptr;

            READ_CODE(&skip_ptr, sizeof(skip_ptr));

            cout << "Id " << extern_id << " " << extern_name << endl;
         }
      }
   }




outfile_ml.close();
infile_m.close();
infile_md.close();


return 0;
}






