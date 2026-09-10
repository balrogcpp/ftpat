#include "pat.h"
#include "dlg.hpp"
#include "rapidcsv.hpp"
#include "statcore.hpp"
#include "stat.hpp"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <future>
#include <map>
#include <vector>
#include <unordered_map>

// format is not available on apple
#if __has_include(<format>) && ((defined(_MSVC_LANG) && _MSVC_LANG >= 202002L) \
    || (defined(__cplusplus) && __cplusplus >= 202002L && !defined(__APPLE__)))
  #include <chrono>
  #include <format>
  namespace fmt = std;
#else
  #define FMT_HEADER_ONLY 1
  #include "fmt/chrono.h"
  #include "fmt/format.h"
#endif
using fmt::format_to;


using namespace std;
const static string HOME_DIR = "./pat/";


// helpers
namespace {



} // anonymous namespace

namespace PAT {

std::string DownloadLog(const std::string &device, const std::string &lot, const std::string &spec, const std::string &condition, const std::vector<std::string> &parameters) {
    if (!fs::is_directory(HOME_DIR)) {
        fs::create_directory(HOME_DIR);
    }

    // check for input parameter correctness
    if (device.empty() || lot.empty() || condition.empty() || parameters.empty()) {
      return "";
    }

    for (const auto &it : parameters) {
      if (it.empty()) {
        return "";
      }
    }

    const std::string out = HOME_DIR + lot + "_" + condition + ".csv";

    if (FileExists(out)) {
      return "";
    }

    std::ostringstream url;
    url << "https://dataconverter.sofia.elex.be/data-converter/datalog?lot=" << lot;
    if (!spec.empty()) {
      url << "&exactlot=true";
    } 
    url << "&skipLogsWithBadSpecs=true&skipmeta=true&step=" << condition << "&productID=" << device;
    if (!spec.empty()) {
      url << "&specV=" << spec;
    }
    url << "&source=file&headermismatchallowed=true&verbose=NORMAL";
    url << "&parameters=";
    auto param = parameters.begin();
    url << "x.equals%28%22" << *param;

    ++param;
    for (; param != parameters.end(); ++param) {
        url << "%22%29%7C%7Cx.equals%28%22" << *param;
    }
    url << "%22%29";

    std::string command = "curl -sS '" + url.str() + "' -o " + out;
    system(command.c_str());

    return "";
}

struct Settings {
  std::string device;
  std::string baselot;
  std::string lot;
  std::vector<std::string> conList;
  std::vector<std::string> patlist;
  std::vector<std::vector<std::string>> mdlist;
};



rapidcsv::Document FilterLog(const std::string &device, const std::string &baselot, const std::string &lot, const std::vector<std::string> &conList,
     const std::vector<std::string> &patlist, const std::vector<std::vector<std::string>> &mdlist, bool exactLot) {
  std::unordered_map<std::string, std::string> merged;
  std::unordered_map<std::string, std::string> uniqueID;
  const bool useSublot = (baselot != lot);

  map<string, vector<string>> parameters;
  for (const auto &it : patlist) {
    size_t pos = it.find_last_of('_');
    string left  = it.substr(0, pos); //left is parameter
    string right = it.substr(pos + 1); // right is condition/step
    if (pos != std::string::npos && find(conList.begin(), conList.end(), right) != conList.end()) {;
      auto &parameter_list = parameters[right];
      if (find(parameter_list.begin(), parameter_list.end(), left) == parameter_list.end()) {
        parameter_list.push_back(left);
      }
    }
  }
  for (const auto &jt : mdlist) {
    for (const auto &it : jt) {
      size_t pos = it.find_last_of('_');
      string left  = it.substr(0, pos);
      string right = it.substr(pos + 1);
      if (pos != std::string::npos && find(conList.begin(), conList.end(), right) != conList.end()) {
        auto &parameter_list = parameters[right];
        if (find(parameter_list.begin(), parameter_list.end(), left) == parameter_list.end()) {
          parameter_list.push_back(left);
        }
      }
    }
  }


  if (!fs::is_directory(HOME_DIR)) {
      fs::create_directory(HOME_DIR);
  }


  std::vector<rapidcsv::Document> logs(conList.size());
  std::vector<std::future<void>> futures;
  futures.reserve(conList.size());
  for (size_t i = 0; i < conList.size(); ++i) {
    futures.emplace_back(std::async(std::launch::async, [&, i]() {
      std::string log = Convert(device, lot, conList[i], parameters[conList[i]], useSublot);
      write_to_file(HOME_DIR + lot + "_" + conList[i] + ".csv", log);
      std::istringstream istr(log);
      logs[i] = rapidcsv::Document(istr, rapidcsv::LabelParams(0, -1));
    }));
  }

  // wait for all tasks
  for (auto& f : futures) {
      f.get();
  }


  merged.reserve(logs[0].GetRowCount());
  uniqueID.reserve(logs[0].GetRowCount());
    ssize_t logIndex = conList.size() - 1;
    for (vector<string>::const_reverse_iterator it = conList.rbegin(); it != conList.rend(); ++it) {
        uniqueID.clear();
        const bool lastCondition = (it == conList.rbegin());
        const auto &logFile = logs.at(logIndex--);
        const ssize_t part_id = logFile.GetColumnIdx("part id");
        const ssize_t die_timestamp = logFile.GetColumnIdx("die timestamp");
        const ssize_t bincode_type = logFile.GetColumnIdx("bincode type");
        const ssize_t lot_id = logFile.GetColumnIdx("lot id");
        const ssize_t wafer_number = logFile.GetColumnIdx("wafer number");
        const ssize_t site_number = logFile.GetColumnIdx("site number");
        vector<ssize_t> idx;
        for(const auto &jt : parameters[*it]) {
          idx.push_back(logFile.GetColumnIdx(jt));
        }

        for (ssize_t row = logFile.GetRowCount()-1; row >= 0; --row) {
            const std::vector<std::string> &Row = logFile.GetRowRaw(row);
            if (Row.size() != logFile.GetColumnCount()) {
                continue;
            }

            const std::string &id = Row[part_id];
            const std::string &timeStamp = Row[die_timestamp];
            const std::string &bincodeType = Row[bincode_type];
            bool isPass = (!bincodeType.empty() && bincodeType[0] == 'P');
            if (!isPass || !is_uint(id) || timeStamp.empty()) {
                continue;
            }

            // not necessary to be here, but it saves memory
            if (lastCondition && useSublot) {
                if (Row[lot_id] != lot) {
                    continue;
                }
            }

            // filtering out parts not present at previous step
            if (!lastCondition && merged.find(id) == merged.end()) {
                continue;
            }

            // sorting by timestamp
            const auto &entry = uniqueID.find(id);
            if (entry != uniqueID.end()) {
                if (timeStamp < entry->second) {
                    continue;
                }
            }

            uniqueID[id] = timeStamp;
        }

        //
        for (ssize_t row = logFile.GetRowCount()-1; row >= 0; --row) {
            const std::vector<std::string> &Row = logFile.GetRowRaw(row);
            if (Row.size() != logFile.GetColumnCount()) {
                continue;
            }

            //
            const std::string &id = Row[part_id];
            const std::string &bincodeType = Row[bincode_type];
            const std::string &timeStamp = Row[die_timestamp];
            bool isPass = (!bincodeType.empty() && bincodeType[0] == 'P');
            if (!isPass || !is_uint(id) || timeStamp.empty()) {
                continue;
            }

            // not necessary to be here, but it saves memory
            // if (lastCondition && useSublot) {
            //     if (Row[lot_id] != lot) {
            //         continue;
            //     }
            // }

            bool rowIsValid = (uniqueID.find(id) != uniqueID.end() && timeStamp == uniqueID[id]);

            // early exit, to avoid parsing of unused rows
            if (!rowIsValid) {
              continue;
            }

            string line;
            if (lastCondition) {
              const std::string &wafer = Row[wafer_number];
              const std::string &site = Row[site_number];
              if (!is_uint(wafer) || !is_uint(site)) {
                continue;
              }
              line.append(id).append(",").append(wafer).append(",").append(site);
            }

            for (const size_t it : idx) {
                const string &value = Row[it];
                if (!is_float(value)) {
                    rowIsValid = false;
                    break;
                }

                line.append(",").append(value);
            }
            if (rowIsValid) {
                merged[id].append(line);
            }
        }

        // final step, remove parts that violates FP
        vector<string> toRemove;
        if (!lastCondition) {
          for (auto &part : merged) {
              if (uniqueID.find(part.first) == uniqueID.end()) {
                toRemove.push_back(part.first);
              }
          }

          for (const auto &partId : toRemove) {
              merged.erase(partId);
          }
        }
    }

  string buf;
  buf = "id,wafer,site";
  for (vector<string>::const_reverse_iterator it = conList.rbegin(); it != conList.rend(); ++it) {
    for (const auto &param : parameters[*it]) {
      buf.append(",").append(param).append("_").append(*it);
    }
  }
  buf.append("\n");

  for (const auto &it : merged) {
    if (!it.second.empty() && it.second[0] != ',') {
      buf.append(it.second).append("\n");
    }
  }

  async_write(HOME_DIR + "Filtered_" + lot + ".csv", buf);
  std::istringstream istr(buf);
  return rapidcsv::Document(istr, rapidcsv::LabelParams(0, 0));
}


typedef float Real;

void PerformUD(const rapidcsv::Document &logFile, const std::string &lot, const std::vector<std::string> &parameters, const std::vector<std::string> &separate) {
  if (logFile.GetRowCount() < 2 || parameters.empty()) {
    return;
  }


  vector<size_t> idx; // parameter indexes
  vector<size_t> idx2; // separator indexes
  for(const auto &it : parameters) {
    idx.push_back(logFile.GetColumnIdx(it)+1);
  }
  for(const auto &it : separate) {
    idx2.push_back(logFile.GetColumnIdx(it)+1);
  }


    map<string, map<string, vector<Real>>> columns;
    map<string, vector<uint64_t>> chipids;

    for (size_t row = 0; row < logFile.GetRowCount(); ++row) {
      const std::vector<std::string> &Row = logFile.GetRowRaw(row);
      auto jt = idx.begin();

      string split_name;
      for (size_t split : idx2) {
        split_name.append("_").append(Row[split]);
      }
      chipids[split_name].push_back(stoi(Row[0])); // the first column is the chipid

      for (const auto &it : parameters) {
        columns[split_name][it].push_back(stod(Row[*(jt++)]));
      }
    }

    for(auto &jt : columns) {
      for(auto &it : jt.second) {
        vector<Real> &values = it.second;
        vector<Real> patValues = calculateUD(values);
        values.swap(patValues);
      }
    }

    string buf;
    auto ins = back_inserter(buf);
    buf = "id";
    for (const auto &param : parameters) {
      buf.append(",").append(param).append("_PAT");
    }
    buf.append("\n");

    for(const auto &it : chipids) {
      const string &split = it.first;
      const vector<uint64_t> &ids = it.second;

      for (size_t i = 0; i < ids.size(); ++i) {
        format_to(ins, "{}", ids[i]);
        for (const auto &jt : parameters) {
          const vector<Real> &patValues = columns[split][jt];
          format_to(ins, ",{:.9f}", patValues[i]);
        }
        buf.append("\n");
      }
    }
    
    async_write(HOME_DIR + "PAT_" + lot + ".csv", buf);
}



void PerformMD(const rapidcsv::Document &logFile, const std::string &lot, const std::vector<std::vector<std::string>> &parameters, const std::vector<std::string> &eval) {
  if (logFile.GetRowCount() < 2 || parameters.empty()) {
    return;
  }

  map<string, vector<Real>> columns;
  vector<string> chipids = logFile.GetRowNames();

  for (const vector<string> &plist : parameters) {
    vector<vector<Real>> values;

    for (const string &param : plist) {
      vector<Real> &&v = logFile.GetColumn<Real>(param);
      values.emplace_back(std::move(v));
    }

    columns[eval[&plist - &parameters[0]]] = calculateMD(std::move(values));
  }

  string buf;
  auto ins= back_inserter(buf);
  buf = "id";
  for (const auto &it : eval) {
    buf.append(",");
    buf.append(it);
  }
  buf.append("\n");

    for (size_t i = 0; i < chipids.size(); ++i) {
      const string &id = chipids[i];
      buf.append(id);

      for (const string &evalName : eval) {
        format_to(ins, ",{:.9f}", columns[evalName][i]);
      }
      buf.append("\n");
    }

  async_write(HOME_DIR+"MD_"+lot+".csv", buf);
}
} // namespace PAT
