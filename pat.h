#include <map>
#include <string>
#include <vector>
#include "dataset.hpp"

namespace rapidcsv {
  class Document;
}; // namespace rapidcsv

struct Receipt {
  const std::string device;
  const std::string baselot;
  const std::string lot;
  std::vector<std::string> conList;
  std::vector<std::pair<std::string, std::vector<std::string>>> parameters;
};

namespace PAT {
std::string DownloadLog(const std::string &device, const std::string &lot, const std::string &spec,
  const std::string &condition, const std::vector<std::string> &parameters);


rapidcsv::Document FilterLog(const Receipt &receipt, bool exactLot=true);


rapidcsv::Document PerformUD(const rapidcsv::Document &log, const Receipt &receipt, const std::vector<std::string> &separate = {});


rapidcsv::Document PerformMD(const rapidcsv::Document &log, const Receipt &receipt);

}; // namespace PAT
