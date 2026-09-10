#include <map>
#include <string>
#include <vector>
#include "dataframe.hpp"

namespace rapidcsv {
  class Document;
}; // namespace rapidcsv


namespace PAT {
// typedef float Real;
// const std::string HOME_DIR="/var/tmp/pat/";



std::string DownloadLog(const std::string &device, const std::string &lot, const std::string &spec,
  const std::string &condition, const std::vector<std::string> &parameters);


  
rapidcsv::Document FilterLog(const std::string &device, const std::string &baselot, const std::string &lot, const std::vector<std::string> &conList,
   const std::vector<std::string> &patlist, const std::vector<std::vector<std::string>> &mdlist, bool exactLot=false);


void PerformUD(const rapidcsv::Document &logFile, const std::string &lot, const std::vector<std::string> &parameters, const std::vector<std::string> &separate = {});


void PerformMD(const rapidcsv::Document &log, const std::string &lot,const std::vector<std::vector<std::string>> &parameters, const std::vector<std::string> &eval);

}; // namespace PAT
