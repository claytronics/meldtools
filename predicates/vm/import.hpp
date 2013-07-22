
#ifndef VM_IMPORT_HPP
#define VM_IMPORT_HPP

#include <string>
#include <ostream>

namespace vm
{

class import
{
   private:

      std::string imp;
      std::string as;
      std::string file;

  public:

      inline void print(std::ostream& out) const
      {
         out << imp << " as " << as << " from " << file;
      }

      explicit import(const std::string& _imp, const std::string& _as,
            const std::string& _file):
         imp(_imp), as(_as), file(_file)
      {
        file.append(".m");
      }

      inline std::string get_imp(void) const { return imp; }   
      inline std::string get_as(void) const { return as; }   
      inline std::string get_file(void) const { return file; }   
};

inline std::ostream&
operator<<(std::ostream& out, const import& imp)
{
   imp.print(out);
   return out;
}


}

#endif
