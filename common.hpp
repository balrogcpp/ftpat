
#if __cplusplus >= 201703L
    #include <filesystem>
    namespace fs = std::filesystem;
#else
    #include "filesystem.hpp"
    namespace fs = ghc::filesystem;
#endif

#if (defined(_MSVC_LANG) && _MSVC_LANG >= 202002L) \
    || (defined(__cplusplus) && __cplusplus >= 202002L && !defined(__APPLE__))
  #include <chrono>
  #include <format>
  namespace fmt = std;
#else
  #define FMT_HEADER_ONLY 1
  #include "fmt/chrono.h"
  #include "fmt/format.h"
#endif

#if __cplusplus >= 201703L
    #include <string_view>
    using std::string_view;
#else
    #include "string_view.hpp"
    using nonstd::string_view;
#endif

#if __cplusplus >= 201703L
    #include <string_view>
    using std::string_view;
#else
    #include "string_view.hpp"
    using nonstd::string_view;
#endif


