#pragma once
#include "stat.hpp"
#include <algorithm>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>

#if __cplusplus >= 201703L
    #include <string_view>
    using std::string_view;
#else
    #include "string_view.hpp"
    using nonstd::string_view;
#endif


#if __cplusplus >= 201703L
    #include <filesystem>
    namespace fs = std::filesystem;
#else
    #include "filesystem.hpp"
    namespace fs = ghc::filesystem;
#endif

inline bool FileExists(const std::string &path, size_t size = 50) noexcept {
  if (fs::exists(path)) {
    return fs::is_regular_file(path) && fs::file_size(path) > size;
  } else {
    return false;
  }
}


inline bool IsOlderThan(const fs::path& path, int hrs) noexcept {
  auto now = fs::file_time_type::clock::now();
  using namespace std::chrono;
  return duration_cast<hours>(now - fs::last_write_time(path)).count() > hrs;
}


inline std::vector<fs::path> FilesOlderThan(const fs::path &dir, int hrs) noexcept {
    std::vector<fs::path> result;

    for (const auto& p : fs::recursive_directory_iterator(dir)) {
        if (fs::is_regular_file(p) && IsOlderThan(p, hrs)) {
            result.push_back(p);
        }
    }

    return result;
}


inline std::pair<int, bool> RemoveFilesOlderThan(const fs::path &dir, unsigned int days) noexcept {
    int cnt = 0 ;
    try {
        for (const auto& p : FilesOlderThan(dir, days * 24)) {
            fs::remove(p);
            ++cnt;
        }

        return std::make_pair(cnt, true);
    }
    catch (const std::exception&) {
        return std::make_pair(cnt, false);
    }
}




#if __cplusplus >= 202002L
  struct string_hash {
    using is_transparent = void;
    [[nodiscard]] size_t operator()(const char *s) const {
      return std::hash<std::string_view>{}(s);
    }
    [[nodiscard]] size_t operator()(std::string_view s) const {
      return std::hash<std::string_view>{}(s);
    }
    [[nodiscard]] size_t operator()(const std::string &s) const {
      return std::hash<std::string>{}(s);
    }
  };
#endif
std::string Convert(const std::string &device, const std::string &lot, const std::string &step, const std::vector<std::string> &parameters, bool exactLot=false) {
  using namespace std;

#ifndef M_XTD
  const string dir = "../dlg";
  const string HOME_DIR = "./pat/";
#else
  const string dir = "/mnt/log/"+device;
  const string HOME_DIR = "/var/tmp/pat/";
#endif

  if (!fs::is_directory(dir)) {
    return {};
  }
  if (!fs::is_directory(HOME_DIR)) {
      fs::create_directory(HOME_DIR);
  }
  vector<fs::directory_entry> logs;
  vector<fs::directory_entry> tables;

// #if 0
//   for (const auto& entry : fs::directory_iterator(dir)) {
//     auto filename = entry.path().filename();
//     string _lot_ = exactLot ? "_"+lot+"_" : "_"+lot;
//     if (filename.string().find(_lot_) != string::npos) {
//       if (filename.string().find(step) != string::npos) {
//         string ext = filename.extension().string();
//         if (ext == ".dlg" || ext == ".locked") {
//           logs.push_back(entry);
//         } else if (ends_with(filename.string(),"TestTable")) {
//           tables.push_back(entry);
//         }
//       }
//     }
//   }
// #else
  for (const auto& entry : fs::directory_iterator(dir)) {
    string filename = entry.path().filename().string();
    string path = entry.path().string();
    string ext = entry.path().extension().string();
    string _lot_ = exactLot ? "_"+lot+"_" : "_"+lot;

    if (filename.find(_lot_) != string::npos && filename.find(step) != string::npos) {
        if (ext == ".locked") {
          filename.erase(filename.size() - ext.size());
          path.erase(path.size() - ext.size());
        }
        if (ends_with(filename,"Data.dlg")) {
          string table_path = path.replace(path.find("Data.dlg"), sizeof("Data.dlg") - 1, "TestTable");
          if (fs::exists(table_path)) {
            logs.push_back(entry);
            tables.push_back(fs::directory_entry(table_path));
          }
        } 
    }
  }
// #endif

  // Sort by last modification time (older first)
  auto is_older = [](const fs::directory_entry& a, const fs::directory_entry& b) {
      return fs::last_write_time(a) < fs::last_write_time(b);
  };
  std::sort(logs.begin(), logs.end(), is_older);
  std::sort(tables.begin(), tables.end(), is_older);

  // simple sanity check, every log should have coresponding table
  if (tables.empty() || logs.empty() || tables.size() != logs.size()) {
    return {};
  }

  ostringstream hashes;
  for (auto it = logs.begin(), jt = tables.begin(); it!=logs.end(), jt!=tables.end(); ++it, ++jt) {
      hashes << it->path().filename().string() << ' ' << it->file_size() << '\n';
      hashes << jt->path().filename().string() << ' ' << jt->file_size() << '\n';
  }
  async_write(HOME_DIR+lot+"_"+step+".txt", hashes.str());


  //  c++ std before 20 does not support heterogeneous lookup (required to use string_view to search)
  // https://www.cppstories.com/2021/heterogeneous-access-cpp20/
#if __cplusplus >= 202002L
  unordered_set<string, string_hash, std::equal_to<>> params;
#else
  unordered_set<string> params;
#endif
  params.insert("D_WFR_ID");
  for (const auto &it : parameters) {
      params.insert(it);
  }

  string buf;
  for (auto it = tables.begin(), jt = logs.begin(); it!=tables.end(), jt!=logs.end(); ++it, ++jt) {
      ifstream table(it->path());
      ifstream log(jt->path());
      vector<string> header = {"lot id","die timestamp","site number","bincode type","part id"};
#if __cplusplus >= 202002L
      unordered_set<string, string_hash, std::equal_to<>> codes;
#else
      unordered_set<string> codes;
#endif
      if (table.is_open()) {
          string line;
          for (size_t code = 1; std::getline(table,line); ++code) {
              string_view parameter = strip(split(line)[0], '\"');
#if __cplusplus >= 202002L
              if (params.find(parameter) != params.end()) {
#else
              if (params.find(string(parameter)) != params.end()) {
#endif
                  header.emplace_back(parameter);
                  codes.insert(std::to_string(code));
              }
          }
      }

      // csv header
      if (buf.empty()) {
          for (auto &it : header) {
              if (it == "D_WFR_ID") {
                  it = "wafer number";
              }

             buf.append(it).append(",");
          }
          buf.pop_back(); // remove last comma
      }

      if (log.is_open()) {
        string line; // line from dlg file
        string str; // string to hold current row, will be appended to buf when new part is found
        string lotname;
        bool isGood = true; // die is exported only if good
        int paramCounter = 0; // count of parameters found for current die

        while(getline(log, line)) {
          // lotname is in header of the file
          if (starts_with(line, "1 ")) {
            lotname = string(split(line)[1]);
            break;
          }
        }

          while(getline(log, line)) {
              if (starts_with(line,"30 ")) { // evaluated parameters
                  const std::vector<string_view> &splits = split(line);
#if __cplusplus >= 202002L
                  if (codes.find(splits[1]) != codes.end())
#else
                  if (codes.find(string(splits[1])) != codes.end())
#endif
                  {
                    paramCounter++; // new parameter found for current die
                    string_view val = splits[3];
                    str.append(",").append(val.data(), val.size());
                  }

                } else if (starts_with(line,"38 ")) { // new part with timestamp
                    // isGood &= (paramCounter == codes.size()); // die is good only if all parameters are found
                    isGood &= (std::count(str.begin(), str.end(), ',') == header.size()-1); // die is good only if all parameters are found
                    if (isGood) {
                        buf.append(str);
                    }
                    paramCounter = 0; // reset parameter counter for new die
                    isGood = true; // reset die status for new die
                    str.clear();
                    string_view val =  split(line, " ,")[1];
                    str.append("\n").append(lotname).append(",").append(val.data(), val.size());
                } else if (starts_with(line,"27 ")) { // site number
                    string_view val = split(line)[1];
                    str.append(",").append(val.data(), val.size());
                } else if (starts_with(line,"28 ")) { // bincode
                    int bin = atoi(split(line)[1].data());
                    bool isPass = (bin < 200);
                    isGood &= isPass;
                    str.append(",").append(isPass ? "P" : "F");
                } else if (starts_with(line,"53 ")) { // part id
                    string_view val = split(line)[1];
                    str.append(",").append(val.data(), val.size());
                }
            }
            // isGood &= (paramCounter == codes.size()); // die is good only if all parameters are found
            isGood &= (std::count(str.begin(), str.end(), ',') == header.size()-1); // die is good only if all parameters are found
            if (isGood) {
                buf.append(str);
            }
        }
    }

    return buf;
}

std::string Convert_2(const std::string &device, const std::string &lot, const std::string &step, const std::vector<std::string> &parameters, bool onlyPass=true, bool exactLot=false) {
  using namespace std;

#ifndef M_XTD
  const string dir = "../dlg";
  const string HOME_DIR = "./pat/";
#else
  const string dir = "/mnt/log/"+device;
  const string HOME_DIR = "/var/tmp/pat/";
#endif

  if (!fs::is_directory(dir)) {
    return {};
  }
  if (!fs::is_directory(HOME_DIR)) {
      fs::create_directory(HOME_DIR);
  }
  vector<fs::directory_entry> logs;
  vector<fs::directory_entry> tables;
  //  c++ std before 20 does not support heterogeneous lookup (required to use string_view to search)
  // https://www.cppstories.com/2021/heterogeneous-access-cpp20/
#if __cplusplus >= 202002L
  unordered_set<string, string_hash, std::equal_to<>> params;
#else
  unordered_set<string> params;
#endif
  params.insert("D_WFR_ID");
  for (const auto &it : parameters) {
      params.insert(it);
  }

  // Collect files from /mnt/log
  for (const auto& entry : fs::directory_iterator(dir)) {
    auto filename = entry.path().filename();
    string _lot_ = exactLot ? "_"+lot+"_" : "_"+lot;
    if (filename.string().find(_lot_) != string::npos) {
      if (filename.string().find(step) != string::npos) {
        string ext = filename.extension().string();
        if (ext == ".dlg" || ext == ".locked") {
          logs.push_back(entry);
        } else if (ends_with(filename.string(),"TestTable")) {
          tables.push_back(entry);
        }
      }
    }
  }
  // for (const auto& entry : fs::directory_iterator(dir)) {
  //   string filename = entry.path().filename().string();
  //   string path = entry.path().string();
  //   string ext = entry.path().extension().string();
  //   string _lot_ = exactLot ? "_"+lot+"_" : "_"+lot;

  //   if (filename.find(_lot_) != string::npos && filename.find(step) != string::npos) {
  //       if (ext == ".locked") {
  //         filename.erase(filename.size() - ext.size());
  //       }
  //       if (ends_with(filename,"Data.dlg")) {
  //         string table_path = path.replace(path.find("Data.dlg"), sizeof("Data.dlg") - 1, "TestTable");
  //         if (fs::exists(table_path)) {
  //           logs.push_back(entry);
  //           tables.push_back(fs::directory_entry(table_path));
  //         }
  //       } 
  //   }
  // }

  // Sort by last modification time (older first)
  auto is_older = [](const fs::directory_entry& a, const fs::directory_entry& b) {
      return fs::last_write_time(a) < fs::last_write_time(b);
  };
  std::sort(logs.begin(), logs.end(), is_older);
  std::sort(tables.begin(), tables.end(), is_older);

  // simple sanity check, every log should have coresponding table
  if (tables.empty() || tables.size() != logs.size()) {
    return {};
  }

  ostringstream hashes;
  for (auto it = logs.begin(); it!=logs.end(); ++it) {
      hashes << it->path().filename().string() << ' ' << it->file_size() << '\n';
  }
  async_write(HOME_DIR+lot+"_"+step+".txt", hashes.str());
  string buf;
  for (auto it = tables.begin(), jt = logs.begin(); it!=tables.end(), jt!=logs.end(); ++it, ++jt) {
      ifstream table(it->path());
      ifstream log(jt->path());
      vector<string> header = {"lot id","die timestamp","site number","bincode type","part id"};
      vector<string> order; // same as header but codes
#if __cplusplus >= 202002L
      unordered_map<string, string, string_hash, std::equal_to<>> codes;
#else
      unordered_map<string, string> codes;
#endif
      if (table.is_open()) {
          string line;
          for (size_t code = 1; std::getline(table,line); ++code) {
              string_view parameter = strip(split(line)[0], '\"');
#if __cplusplus >= 202002L
              if (params.find(parameter) != params.end()) {
#else
              if (params.find(string(parameter)) != params.end()) {
#endif
                  header.emplace_back(parameter);
                  string code_str = std::to_string(code);
                  order.emplace_back(code_str);
                  codes.insert(make_pair(code_str, parameter));
              }
          }
      }

      // csv header
      if (buf.empty()) {
          for (auto &it : header) {
              if (it == "D_WFR_ID") {
                  it = "wafer number";
              }

             buf.append(it).append(",");
          }
          buf[buf.size()-1] = '\n'; // replace last comma with newline
      }

      if (log.is_open()) {
        string line; // line from dlg file
#if __cplusplus >= 202002L
        unordered_map<string, string, string_hash, std::equal_to<>> row;
#else
        unordered_map<string, string> row;
#endif
        string lotname;
        bool isPass = false;

        while(getline(log, line)) {
          // lotname is in header of the file
          if (starts_with(line, "1 ")) {
            lotname = string(split(line)[1]);
            break;
          }
        }

          while(getline(log, line)) {
              if (starts_with(line,"30 ")) { // evaluated parameters
                  const std::vector<string_view> &splits = split(line);
#if __cplusplus >= 202002L
                  if (codes.find(splits[1]) != codes.end())
#else
                  if (codes.find(string(splits[1])) != codes.end())
#endif
                  {
                    string_view code = splits[1];
                    string_view val = splits[3];
                    row[codes[string(code)]] = string(val);
                  }

                } else if (starts_with(line,"38 ")) { // new part, contains part is and time
                  if (!row.empty() && isPass) {
                    for (auto &it : header) {
                        buf.append(row[it]).append(",");
                    }
                    buf[buf.size()-1] = '\n'; // replace last comma with newline
                  }
                    row.clear();
                    string_view val =  split(line, " ,")[1];
                    row["lot id"] = lotname;
                    row["die timestamp"] = string(val.data(), val.size());
                } else if (starts_with(line,"27 ")) { // site number
                    string_view val = split(line)[1];
                    row["site number"] = string(val.data(), val.size());
                } else if (starts_with(line,"28 ")) { // bincode
                    int bin = atoi(split(line)[1].data());
                    isPass = (bin < 200);
                    row["bincode type"] = isPass ? "P" : "F";
                } else if (starts_with(line,"53 ")) { // part id
                    string_view val = split(line)[1];
                    row["part id"] = string(val.data(), val.size());
                }
            }
            if (!row.empty() && isPass) {
              for (auto &it : header) {
                  buf.append(row[it]).append(",");
              }
              buf[buf.size()-1] = '\n'; // replace last comma with newline
            }
        }
    }

    return buf;
}
