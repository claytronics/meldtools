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
        cout<<"Format Error !"<<endl<<"Correct format : "<<argv[0]<<" [[input .m file] or [output .ml file]] [byte to be found]"<<endl;
        return 0;
    }


    std::ifstream infile_m(argv[1],std::ifstream::binary);
    
    cout<<"byte requested : "<<argv[2]<<endl;
    char nibble1 = *argv[2];
    argv[2]++;
    char nibble2 = *argv[2];

    cout<<"nibble1:nibble2 = "<<nibble1<<":"<<nibble2<<endl;

    infile_m.seekg(0,infile_m.end);
    long size_tot1 = infile_m.tellg();
    infile_m.seekg(0);


    cout <<"total file size input.m : "<<size_tot1<<" bytes"<<endl;

    //alloc mem
    char *buffer1 = new char[size_tot1];

    //read
    infile_m.read(buffer1,size_tot1);

    //find
    int i;

    for(i=0;i<size_tot1;i++){

        if(buffer1[i] == nibble1)
          if(buffer1[i+1] == nibble2){
            cout<<"FOUND !\n";
            i++;
            }

    }

    cout<<endl;

    delete[] buffer1;

    infile_m.close();

    return 0;
}
