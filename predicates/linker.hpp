
//not including init from each file
const int COMMON_PREDICATES = 8;

const int MAX_CHARS_PER_LINE = 512;
const int MAX_TOKENS_PER_LINE = 3;
const char* const DELIMITER = " ";
const int RULE0 = 0;
size_t DEFAULT_ID = 0xffffffff;

size_t position;
size_t position_wr;
size_t position_prev;
char *buffer;
char dummy_path[] = "/home/rupesh/ls.o";
int num_of_lines = 0;


struct func_table{
char func_name[254];
char file_name[1024];
};

char* get_filename(char *func_name,struct func_table* table,int size);
