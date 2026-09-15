#include "rapidcsv.hpp"
#include "pat.h"
#include "sparameters.h"
#include <future>

using namespace std;

const static std::string FT1 = "_FT1";
const static std::string FT2 = "_FT2";
const static std::string FT3 = "_FT3";

const static vector<string> CON_LIST = {"FT1", "FT2", "FT3"};

const static vector<string> UV_LIST = {ParamDrift_CorrR_CrackRing+FT1, ParamDrift_Responsivity_ADC_P5_PIX+FT1, ParamR_Blind_PIX+FT1, ParamR_Live_PIX+FT1,
                                        ParamDrift_CorrR_CrackRing+FT2, ParamIddDiff_VddMax_Idle_IC+FT2, ParamIih_LeakageDiff_SDA+FT2, ParamIih_LeakageDiff_SCL+FT2, ParamR_Blind_PIX+FT2, ParamR_Live_PIX+FT2,
                                        ParamDrift_CorrR_CrackRing+FT3, ParamR_Blind_PIX+FT3, ParamR_Live_PIX+FT3,
};

const static vector<string> MD_EVAL = {ParamOPT_SENSOR_MD_PAT, ParamIdd_VddNom_MD_PAT, ParamIdd_VddMax_Idle_MD_PAT};

const static vector<vector<string>> MD_LIST = {{ParamK_KTA_OPT_SENSOR+FT3, ParamK_TA0_OPT_SENSOR+FT3},
                                              {ParamIdd_VddNom_IC+FT1, ParamIdd_VddNom_IC+FT2, ParamIdd_VddNom_IC+FT3},
                                              {ParamIdd_VddMax_Idle_IC+FT1, ParamIdd_VddMax_Idle_IC+FT2, ParamIdd_VddMax_Idle_IC+FT3}
};

const static string device = "90635BASECA200";
const static string sublot = "D40880W";
const static string baselot = "D40880";

// const static vector<string> CON_LIST = {"PR35", "PR125", "PRm40"};

// const static vector<string> UV_LIST = {"Idd_VddNom_IC_PR35", "Idd_VddNom_IC_PR125", "Idd_VddNom_IC_PRm40"};

// const static vector<vector<string>> MD_LIST = {{"Idd_VddNom_IC_PR35", "Idd_VddNom_IC_PR125", "Idd_VddNom_IC_PRm40"}};
// const static vector<string> MD_EVAL = {"Idd_VddNom_IC"};

// const static string device = "90650BAB";
// const static string sublot = "Z43991";
// const static string baselot = "Z43991";


// clang++ -s -static-libstdc++ -O3 -ffast-math -funroll-loops -std=c++23 -ftree-vectorize -march=native -flto -pthread -fopenmp -I include -ltbb pat2.cpp main.cpp -no-pie -o test && time ./test
int main()
{
  std::ios_base::sync_with_stdio(false);

  const auto &log = PAT::FilterLog(device, baselot, sublot, CON_LIST, UV_LIST, MD_LIST, true);

  auto f1 = std::async(std::launch::async, [&]() {
    PAT::PerformUD(log, sublot, UV_LIST, {"wafer"});
  });

  auto f2 = std::async(std::launch::async, [&]() {
    PAT::PerformMD(log, sublot, MD_LIST, MD_EVAL);
  });

  f1.get();
  f2.get();



  return 0;
}
