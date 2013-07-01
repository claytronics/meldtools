static const size_t PREDICATE_DESCRIPTOR_SIZE = 104;/* sizeof(code_size_t) +
                                                       PRED_DESCRIPTOR_BASE_SIZE +
                                                       PRED_ARGS_MAX +
                                                       PRED_NAME_SIZE_MAX +
                                                       PRED_AGG_INFO_MAX;*/



const int MAX_CHARS_PER_LINE = 512;
const int MAX_TOKENS_PER_LINE = 3;
const char* const DELIMITER = " ";


typedef uint32_t code_size_t;
const uint32_t MAGIC1 = 0x646c656d;
const uint32_t MAGIC2 = 0x6c696620;
typedef unsigned char byte;
typedef uint32_t uint_val;
typedef int32_t int_val;
typedef unsigned char byte_code_el;
typedef byte_code_el* byte_code;
byte_code const_code;
typedef uint64_t ptr_val;

size_t position;
size_t position_wr;
size_t position_prev;
char *buffer;
char dummy_path[] = "/home/rupesh/ls.o";
long size;
int num_of_lines = 0;
code_size_t const_code_size;

struct func_table{
char func_name[254];
char file_name[1024];
};

char* get_filename(char *func_name,struct func_table* table,int size);