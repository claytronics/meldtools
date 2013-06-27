#include <fstream>
#include <iostream>
#include <string>
#include <stdint.h>
#include<assert.h>
#include<stdio.h>
#include<string.h>

using namespace std;

int main(int argc,char **argv) {

    if(argc != 3){
        cout<<"Format Error !"<<endl<<"Correct format : "<<argv[0]<<" [input .m file] [output .ml file]"<<endl;
        return 0;
    }


    std::ifstream infile_m(argv[1],std::ifstream::binary);
    std::ifstream infile_ml(argv[2],std::ofstream::binary);

    infile_m.seekg(0,infile_m.end);
    long size_tot1 = infile_m.tellg();
    infile_m.seekg(0);

    infile_ml.seekg(0,infile_ml.end);
    long size_tot2 = infile_ml.tellg();
    infile_ml.seekg(0);

    cout <<"total file size input.m : "<<size_tot1<<" bytes"<<endl;
    cout <<"total file size output.ml : "<<size_tot2<<" bytes"<<endl;

    //alloc mem
    char *buffer1 = new char[size_tot1];
    char *buffer2 = new char[size_tot2];

    //read
    infile_m.read(buffer1,size_tot1);
    infile_ml.read(buffer2,size_tot2);

    //compare
    int i;

    for(i=0;i<size_tot1;i++){

        if(buffer1[i] != buffer2[i])
            cout<<buffer2[i];

    }

    cout<<endl;

    delete[] buffer1;
    delete[] buffer2;

    infile_m.close();
    infile_ml.close();

    return 0;
}
