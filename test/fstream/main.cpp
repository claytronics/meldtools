#include <fstream>
#include <iostream>

int main() {

using namespace std;

std::ifstream infile_m("input.m",std::ifstream::binary);
std::ifstream infile_md("def.md",std::ifstream::binary);

std::ofstream outfile_ml("output.ml",std::ofstream::binary);

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

outfile_ml.close();
infile_m.close();
infile_md.close();

return 0;
}




