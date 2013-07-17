#include <iostream>
#include <dlfcn.h>

int main() {

    using std::cout;
    using std::cerr;
    using std::endl;


    // open library
    cout<< "Opening...dummy.so"<<endl;
    void* handle = dlopen("dummy.so", RTLD_LAZY);
    
    if(!handle){
        cerr<<"Cannot open library : "<< dlerror()<<endl;
        return 1;
    }


    // load the symbol
    cout<< "Loading symbol...\n";

    // reset errors
    dlerror();
/*
    typedef void (*hello_t)();
    hello_t hello = (hello_t) dlsym(handle,"hello");
    const char *dlsym_error = dlerror();

    if(dlsym_error){
        cerr << "Cannot load symbol 'hello' : "<<dlsym_error <<endl;
        dlclose(handle);
        return 1;
    }


    // call the symbol
    cout<< "Calling hello...\n";
    hello();
*/
    typedef int (*inta_t)(int p,int q,int r);

    inta_t inta = (inta_t) dlsym(handle,"inta");
    const char *dlsym_error = dlerror();

    if(dlsym_error){
        cerr << "Cannot load symbol 'inta' : "<<dlsym_error <<endl;
        dlclose(handle);
        return 1;
    }


    // call the symbol
    cout<< "Calling inta...\n";
    cout<<inta(1,2,3)<<endl;

/*
    hello_t intb = (hello_t) dlsym(handle,"intb");
    const char *dlsym_error1 = dlerror();

    if(dlsym_error1){
        cerr << "Cannot load symbol 'intb' : "<<dlsym_error <<endl;
        dlclose(handle);
        return 1;
    }


    // call the symbol
    cout<< "Calling intb...\n";
    cout<<intb(4)<<endl;
*/
    //close lib
    cout <<"Closing library...\n";
    dlclose(handle);

}

    
     
