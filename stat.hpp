#pragma once

#include <fstream>
#include <future>
#include <string>
#include <vector>


#if __cplusplus >= 201703L
    #include <string_view>
    using std::string_view;
#else
    #include "string_view.hpp"
    using nonstd::string_view;
#endif



inline bool starts_with(string_view s, string_view prefix) noexcept {
#if __cplusplus >= 202002L
  return s.starts_with(prefix);
#else
  return s.size() >= prefix.size() &&
      s.compare(0, prefix.size(), prefix) == 0;
#endif
}


inline bool ends_with(string_view s, string_view suffix) noexcept {
#if __cplusplus >= 202002L
  return s.ends_with(suffix);
#else
  return s.size() >= suffix.size() &&
         s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
#endif
}


inline std::vector<string_view> split(string_view s, char delim = ' ') noexcept {
  std::vector<string_view> out;

  const char* data = s.data();
  const char* start = data;
  const char* end = data + s.size();

  for (const char* p = data; p != end; ++p) {
      if (*p == delim) {
          out.emplace_back(start, p - start);
          start = p + 1;
      }
  }

  // last token
  out.emplace_back(start, end - start);

  return out;
}


inline std::vector<string_view> split(string_view s, string_view delim) noexcept {
  std::vector<string_view> out;

    const char* data = s.data();
    const char* start = data;
    const char* end = data + s.size();

    for (const char* p = data; p != end; ++p) {
        if (delim.find(*p) != string_view::npos) {
            out.emplace_back(start, p - start);
            start = p + 1;
        }
    }

    // last token
    out.emplace_back(start, end - start);

    return out;
}


inline string_view strip(string_view s, char c) noexcept {
    const char* begin = s.data();
    const char* end   = begin + s.size();

    // trim left
    while (begin < end && static_cast<char>(*begin) == c) {
        ++begin;
    }

    // trim right
    while (end > begin && static_cast<char>(*(end - 1)) == c) {
        --end;
    }

    return string_view(begin, end - begin);
}


inline string_view strip(string_view s) noexcept {
    const char* begin = s.data();
    const char* end   = begin + s.size();

    // trim left
    while (begin < end && std::isspace(static_cast<char>(*begin))) {
        ++begin;
    }

    // trim right
    while (end > begin && std::isspace(static_cast<char>(*(end - 1)))) {
        --end;
    }

    return string_view(begin, end - begin);
}


inline bool is_float(const char *c) {
    bool point = false;

    if (*c == 0) {
        return false;
    }
    if (*c == '-') {
        c++;
    }

    for (; *c != 0; c++) {
        if (*c == '.') {
            if (!point) {
                point = true;
            } else {
                return false;
            }

            continue;
        }

        if (*c < '0' || *c > '9') {
            return false;
        }
    }

    return true;
} 

inline bool is_float(const std::string &s) noexcept {
    const char *c = s.c_str();

    return is_float(c);
}


inline bool is_int(const char* c) noexcept {
    if (*c == 0) {
        return false;
    }
    if (*c == '-') {
        c++;
    }

    for (; *c != 0; c++) {
        if (*c < '0' || *c > '9') {
            return false;
        }
    }

    return true;
}
inline bool is_int(const std::string &s) noexcept {
    const char *c = s.c_str();

    return is_int(c);
}


inline bool is_uint(const char *s) noexcept {
    if (*s == 0) {
        return false;
    }

    for (; *s != 0; s++) {
        if (*s < '0' || *s > '9') {
            return false;
        }
    }

    return true;
}
inline bool is_uint(const std::string &s) noexcept {
    const char *c = s.data();
    return is_uint(c);
}


inline void write_to_file(const std::string& filename, const std::string& data) noexcept {
    std::ofstream out(filename);
    if (out) {
        out << data;
    }
}


inline void async_write(const std::string& filename, const std::string& data) noexcept {
    std::async(std::launch::async, write_to_file, filename, data);
}
