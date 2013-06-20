#include <iostream>
#include <dlfcn.h>

int main() {

    using std::cout;
    using std::cerr;
    using std::endl;
    
    // open library
    cout<< "Opening hello.so...\n";
    void* handle = dlopen("./hello.so", RTLD_LAZY);
    
    if(!handle){
        cerr<<"Cannot open library : "<< dlerror()<<endl;
        return 1;
    }

    // load the symbol
    cout<< "Loading symbol hello...\n";
    typedef void (*hello_t)();

    // reset errors
    dlerror();

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

    //close lib
    cout <<"Closing library...\n";
    dlclose(handle);

}

    
     
