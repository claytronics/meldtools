#include <iostream>

extern "C" int inta(int p, int q, int r) {    
    std::cout << "int a" << '\n';
    return (p+q+r);
}

extern "C" int intb(int p) {    
    std::cout << "int b" << '\n';
    return p;
}
