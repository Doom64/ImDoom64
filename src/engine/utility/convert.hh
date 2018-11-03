// -*- mode: c++ -*-
#ifndef __IMP_CONVERT__63784703
#define __IMP_CONVERT__63784703

#include "types.hh"
#include "string_view.hh"

namespace imp {
  template <class T, typename = std::enable_if_t<std::is_integral<T>::value>>
  constexpr size_t to_size(T i)
  {
      // This if-statement should hopefully be removed for unsigned integrals for -O3.
      if (i < 0)
          throw std::logic_error("Casting negative integer to size_t");
      return static_cast<size_t>(i);
  };

  std::string type_to_string(const std::type_info&);

  template <class T>
  std::string type_to_string()
  { return type_to_string(typeid(T)); }
}

#endif //__IMP_CONVERT__63784703
