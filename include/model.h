#include <iostream>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include "cpr/cpr.h"

using namespace std;
namespace fs = std::filesystem;

class model {
  private:
    string stl_file_path;
    string fname;
    char jaw_type;

public:
    model();
    model(string fpath);
    ~model();
    bool segment_jaw(string& stl_, vector<int>& label_, string& error_msg_);
    inline string stl_path() { return stl_file_path; };
    inline string stl_name() { return fname; };
    inline char stl_jaw_type() { return jaw_type; };
};


