
#include <fstream>
#include <sys/stat.h>
#include <iostream>
#include <boost/static_assert.hpp>
#include<dlfcn.h>

#include "vm/program.hpp"
#include "db/tuple.hpp"
#include "vm/instr.hpp"
#include "db/database.hpp"
#include "utils/types.hpp"
#include "vm/state.hpp"
#include "version.hpp"

#ifdef USE_UI
#include "ui/macros.hpp"
#endif

using namespace std;
using namespace db;
using namespace vm;
using namespace vm::instr;
using namespace process;
using namespace utils;

std::ostream cnull(0);
namespace vm {

static const size_t PREDICATE_DESCRIPTOR_SIZE = sizeof(code_size_t) +
                                                PRED_DESCRIPTOR_BASE_SIZE +
                                                PRED_ARGS_MAX +
                                                PRED_NAME_SIZE_MAX +
                                                PRED_AGG_INFO_MAX;
#define READ_CODE(TO, SIZE) do { \
	fp.read((char *)(TO), SIZE);	\
	position += (SIZE);				\
} while(false)
#define SEEK_CODE(SIZE) do { \
	fp.seekg(SIZE, ios_base::cur);	\
	position += (SIZE);	\
} while(false)
#define VERSION_AT_LEAST(MAJ, MIN) (major_version > (MAJ) || (major_version == (MAJ) && minor_version >= (MIN)))

// most integers in the byte-code have 4 bytes
BOOST_STATIC_ASSERT(sizeof(uint_val) == 4);

program::program(const string& _filename):
   filename(_filename),
   init(NULL)
{
	size_t position(0);
  
   ifstream fp(filename.c_str(), ios::in | ios::binary);
   
   if(!fp.is_open())
      throw load_file_error(filename, "unable to open file");

   // linker change 
   set_modify(false);
 
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
   cout << "num_predicates : " << num_predicates << endl; 
    
   predicates.resize(num_predicates);
   code_size.resize(num_predicates);
   code.resize(num_predicates);

   // skip nodes
   uint_val num_nodes;
	READ_CODE(&num_nodes, sizeof(uint_val));
    cout << "num_nodes : " << num_nodes << endl;
    set_num_nodes(num_nodes);

//	SEEK_CODE(num_nodes * database::node_size);
    size_t db_size = num_nodes * database::node_size;
    unsigned char* db_buf = new unsigned char[db_size];
    READ_CODE(db_buf,db_size);
    set_buffer(db_buf);

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

         imported_predicates.push_back(new import(buf_imp, buf_as, buf_file));
      }
      assert(imported_predicates.size() == number_imported_predicates);

      uint32_t number_exported_predicates;

      READ_CODE(&number_exported_predicates, sizeof(number_exported_predicates));

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
    cout << "num_args : " << num_args << endl; 

   // get rule information
   uint_val n_rules;

   READ_CODE(&n_rules, sizeof(uint_val));

   number_rules = n_rules;
   cout << "n_rules : " << n_rules << endl; 

   for(size_t i(0); i < n_rules; ++i) {
      // read rule string length
      uint_val rule_len;

      READ_CODE(&rule_len, sizeof(uint_val));

      assert(rule_len > 0);

      char str[rule_len + 1];

      READ_CODE(str, sizeof(char) * rule_len);

      str[rule_len] = '\0';
      cout << "rule " << i << " : " <<  str << endl; 

      rules.push_back(new rule((rule_id)i, string(str)));
   }

	// read string constants
	int_val num_strings;
	READ_CODE(&num_strings, sizeof(int_val));
    cout << "num_strings : " << num_strings << endl; 
	
	default_strings.reserve(num_strings);
	
	for(int i(0); i < num_strings; ++i) {
		int_val length;
		
		READ_CODE(&length, sizeof(int_val));
		
		char str[length + 1];
		READ_CODE(str, sizeof(char) * length);
		str[length] = '\0';
        cout << "const string " << i << " : " <<  str << endl; 

		default_strings.push_back(runtime::rstring::make_default_string(str));
	}
	
	// read constants code
	uint_val num_constants;
	READ_CODE(&num_constants, sizeof(uint_val));
    cout << "num_constants : " << num_constants << endl; 
	
	// read constant types
	const_types.resize(num_constants);
	
	for(uint_val i(0); i < num_constants; ++i) {
		byte b;
		READ_CODE(&b, sizeof(byte));
		const_types[i] = (field_type)b;
        cout << "const type " << i << " : " << b << endl; 
	}
	
	// read constants code
	READ_CODE(&const_code_size, sizeof(code_size_t));
    cout << "const_code_size : " << const_code_size << endl; 
    
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

         // linker
         set_num_ext_functions(n_externs);
         set_ext_function_position(position);

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
      predicates[i] = predicate::make_predicate_from_buf((unsigned char*)buf, &size, (predicate_id)i);
      code_size[i] = size;

      cout << "predicate " << i << " : " << predicates[i]->get_name() << endl;  
      MAX_STRAT_LEVEL = max(predicates[i]->get_strat_level() + 1, MAX_STRAT_LEVEL);
		
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

program::~program(void)
{

   for(size_t i(0); i < num_predicates(); ++i) {
      delete predicates[i];
      delete []code[i];
   }
	for(size_t i(0); i < num_rules(); ++i) {
		delete rules[i];
	}
      delete data_rule;

   for(size_t i(0); i < functions.size(); ++i) {
      delete functions[i];

   }
	delete []const_code;

   for(size_t i(0); i < imported_predicates.size(); ++i) {
      delete imported_predicates[i];

   }
   MAX_STRAT_LEVEL = 0;
}

// linker change  
void
program::set_modify(bool val) {

 modify_bytecode = val;

}

bool 
program::allow_modify(void) const { 

return modify_bytecode;

} 

void 
program::add_external_function(external_function_ptr ptr,size_t num_args,field_type
        ret,field_type *arg){

#define EXTERN(NAME) (external_function_ptr) NAME
#define EXTERNAL0(NAME, RET) external0(EXTERN(NAME), RET)
#define EXTERNAL1(NAME, RET, ARG1) external1(EXTERN(NAME), RET, ARG1)
#define EXTERNAL2(NAME, RET, ARG1, ARG2) external2(EXTERN(NAME), RET, ARG1, ARG2)
#define EXTERNAL3(NAME, RET, ARG1, ARG2, ARG3) external3(EXTERN(NAME), RET, ARG1, ARG2, ARG3)

    switch(num_args){

        case 0 : cout<<"arg0 register_func_id :"<<register_external_function(EXTERNAL0(ptr,ret));
                 cout<<endl;
                 break;

        case 1 :  cout<<"arg1 register_func_id:"<< register_external_function(EXTERNAL1(ptr, ret, arg[0]));
                  cout<<endl;  
                  break;  

        case 2 : cout<<"arg2 register_func_id:"<<register_external_function(EXTERNAL2(ptr,ret,arg[0],arg[1]));
                 cout<<endl;
                 break;

        case 3 :cout<<"arg3 register_func_id:"<<register_external_function(EXTERNAL3(ptr,ret,arg[0],arg[1],arg[2]));
                 cout<<endl;   
                 break;

        default : break;
    }

}

ptr_val 
program::get_function_pointer(char *lib_path,char* func_name){

    cout<<"Opening shared object file..."<<lib_path<<endl;
    void *handle = dlopen(lib_path,RTLD_LAZY);

    if(!handle){
        cerr<<"Cannot Open Library : "<<dlerror()<<endl;
        return 1;
    }

    cout<<"Loading symbol..."<<func_name<<endl;
    typedef void (*func_t)();

    //reset errors
    dlerror();

    func_t func = (func_t)dlsym(handle,func_name);
    const char *dlsym_error = dlerror();

    if(dlsym_error){
        cerr<<"Cannot load symbol..."<<dlsym_error<<endl;
        dlclose(handle);
        return 0;
    }

    return (ptr_val)func;

}

predicate*
program::get_predicate(const predicate_id& id) const
{
   if((size_t)id >= num_predicates()) {
      return NULL;
   }
   
   return predicates[id];
}

predicate*
program::get_route_predicate(const size_t& i) const
{
   assert(i < num_route_predicates());
   
   return route_predicates[i];
}

void
program::print_predicate_code(ostream& out, predicate* p) const
{
   out << "PROCESS " << p->get_name()
      << " (" << code_size[p->get_id()] << "):" << endl;
   instrs_print(code[p->get_id()], code_size[p->get_id()], 0, this, out);
   out << "END PROCESS;" << endl;
}

void
program::print_bytecode(ostream& out) const
{
   out << "VERSION " << major_version << "." << minor_version << endl << endl;

	out << "CONST CODE" << endl;
	
	instrs_print(const_code, const_code_size, 0, this, out);

   out << endl;

   out << "FUNCTION CODE" << endl;
   for(size_t i(0); i < functions.size(); ++i) {
      out << "FUNCTION " << i << endl;
      instrs_print(functions[i]->get_bytecode(), functions[i]->get_bytecode_size(), 0, this, out);
      out << endl;
   }

   out << endl;
   out << "PREDICATE CODE" << endl;
	
   for(size_t i = 0; i < num_predicates(); ++i) {
      predicate_id id = (predicate_id)i;
      
      if(i != 0)
         out << endl;
         
      print_predicate_code(out, get_predicate(id));
   }

   out << "RULES CODE" << endl;

   for(size_t i(0); i < number_rules; ++i) {
      out << endl;
      out << "RULE " << i << endl;
		rules[i]->print(out, this);
   }
}

void
program::print_bytecode_by_predicate(ostream& out, const string& name) const
{
	predicate *p(get_predicate_by_name(name));
	
	if(p == NULL) {
		cerr << "Predicate " << name << " not found." << endl;
		return;
	}
   print_predicate_code(out, get_predicate_by_name(name));
}

void
program::print_predicates(ostream& cout) const
{
   if(is_safe())
      cout << ">> Safe program" << endl;
   else
      cout << ">> Unsafe program" << endl;
   cout << ">> Priorities: " << (priority_order == PRIORITY_ASC ? "ascending" : "descending") << " ";
   cout << "initial: " << (priority_type == FIELD_FLOAT ? initial_priority.float_priority : initial_priority.int_priority) << endl;
   if(is_data())
      cout << ">> Data file" << endl;
   cout << ">> Predicates:" << endl;
   for(size_t i(0); i < num_predicates(); ++i) {
      cout << predicates[i] << " " << *predicates[i] << endl;
   }
   cout << ">> Imported Predicates:" << endl;
   for(size_t i(0); i < imported_predicates.size(); i++) {
      cout << *imported_predicates[i] << endl;
   }
   cout << ">> Exported Predicates:" << endl;
   for(size_t i(0); i < exported_predicates.size(); i++) {
      cout << exported_predicates[i] << endl;
   }
}

#ifdef USE_UI
using namespace json_spirit;

Value
program::dump_json(void) const
{
	Array preds_json;

	for(size_t i(0); i < num_predicates(); ++i) {
		Object obj;
		predicate *pred(get_predicate((predicate_id)i));

		UI_ADD_FIELD(obj, "name", pred->get_name());

		Array field_types;

		for(size_t j(0); j < pred->num_fields(); ++j) {
			switch(pred->get_field_type(j)) {
				case FIELD_INT:
					UI_ADD_ELEM(field_types, "int");
					break;
				case FIELD_FLOAT:
					UI_ADD_ELEM(field_types, "float");
					break;
				case FIELD_NODE:
					UI_ADD_ELEM(field_types, "node");
					break;
				case FIELD_STRING:
					UI_ADD_ELEM(field_types, "string");
					break;
				case FIELD_LIST_INT:
					UI_ADD_ELEM(field_types, "list int");
					break;
				default:
					throw type_error("Unrecognized field type " + field_type_string(pred->get_field_type(j)) + " (program::dump_json)");
			}
		}
		UI_ADD_FIELD(obj, "fields", field_types);

		UI_ADD_FIELD(obj, "route",
				pred->is_route_pred() ? UI_YES : UI_NIL);
		UI_ADD_FIELD(obj, "reverse_route",
				pred->is_reverse_route_pred() ? UI_YES : UI_NIL);
		UI_ADD_FIELD(obj, "linear",
			pred->is_linear_pred() ? UI_YES : UI_NIL);

		UI_ADD_ELEM(preds_json, obj);
	}

	return preds_json;
}
#endif

predicate*
program::get_predicate_by_name(const string& name) const
{
   for(size_t i(0); i < num_predicates(); ++i) {
      predicate *pred(get_predicate((predicate_id)i));
      
      if(pred->get_name() == name)
         return pred;
   }
   
   return NULL;
}

predicate*
program::get_init_predicate(void) const
{
   if(init == NULL) {
      init = get_predicate(INIT_PREDICATE_ID);
      if(init->get_name() != "_init") {
         cerr << "_init program should be predicate #" << (int)INIT_PREDICATE_ID << endl;
         init = get_predicate_by_name("_init");
      }
      assert(init->get_name() == "_init");
   }

   assert(init != NULL);
      
   return init;
}

predicate*
program::get_edge_predicate(void) const
{
   return get_predicate_by_name("edge");
}

tuple*
program::new_tuple(const predicate_id& id) const
{
   return new tuple(get_predicate(id));
}

bool
program::add_data_file(program& other)
{
   if(num_predicates() < other.num_predicates()) {
      return false;
   }

   for(size_t i(0); i < other.num_predicates(); ++i) {
      predicate *mine(predicates[i]);
      predicate *oth(other.get_predicate(i));
      if(*mine != *oth) {
         cerr << "Predicates " << *mine << " and " << *oth << " are different" << endl;
         return false;
      }
   }

   assert(rules.size() > 0);
   assert(other.rules.size() > 0);

   data_rule = other.rules[0];
   other.rules.erase(other.rules.begin());
   other.number_rules--;

   vm::rule *init_rule(rules[0]);
   byte_code init_code(init_rule->get_bytecode());
   init_code += init_rule->get_codesize();

   init_code -= (RETURN_BASE + NEXT_BASE + RETURN_DERIVED_BASE + MOVE_BASE + ptr_size);
   assert(fetch(init_code) == MOVE_INSTR);
   assert(val_is_ptr(move_from(init_code)));
   assert(val_is_reg(move_to(init_code)));
   *move_to_ptr(init_code) = VAL_PCOUNTER;
   *((ptr_val *)(init_code + MOVE_BASE)) = (ptr_val)data_rule->get_bytecode();

   //instrs_print(init_rule->get_bytecode(), init_rule->get_codesize(), 0, this, cout);
   return true;
}

void 
program::print_predicate_dependency(){

    size_t i,j;

    for(i = 0; i < number_rules;i++){

        cout<<"rule : "<<i<<endl;

        if(print_code)
            rules[i]->print(cout,this);
        else{
            cout<<rules[i]->get_string()<<endl;
            rules[i]->print(cnull,this);
        }    

        cout<<"Rule depends on : "<<endl;
        dependency_print();
        cout<<endl;  
        
        for(j = 0; j < get_dependency_size(); j++){
        
        rules[i]->add_dependency(get_predicate_by_name(get_dependency_number(j)));

        }
        
        dependency_clear();
    }


    dependency_clear();

    for(i = 0 ; i < num_predicates(); i++){

        cout<<"predicate : "<<i<<endl;
        cout<<predicates[i]->name<<endl;

        for(j = 0 ; j < predicates[i]->affected_rules.size();j++)
            cout<<"affected rule : "<<predicates[i]->affected_rules[j]<<endl;

        if(print_code)
            print_predicate_code(cout,get_predicate_by_name(predicates[i]->name));
        else
            print_predicate_code(cnull,get_predicate_by_name(predicates[i]->name));

        cout<<"Predicate depends on : "<<endl;
        dependency_print();
        cout<<endl;   

        for(j = 0; j < get_dependency_size(); j++){
        
        predicates[i]->add_dependency(get_predicate_by_name(get_dependency_number(j)));

        }

        dependency_clear();
    }


}

}
