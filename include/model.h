#include <iostream>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <Eigen/Core>
#include <Eigen/Geometry>

#include "cpr/cpr.h"

using namespace std;
namespace fs = std::filesystem;

class model {
private:
    string stl_file_path;
    string fname;
    char jaw_type;
    vector<int> teeth_id;
    map<int, Eigen::RowVector3d> colors;
    map<string, string> teeth_comp_stl;
    map<string, string> teeth_comp_stl_urn;
    string gum_urn;


public:
    model();
    model(string fpath);
    ~model();
    
    bool segment_jaw(string& stl_, map<string, string>& result_t_comp_stls_, vector<int>& label_, string& error_msg_);
    bool model::generate_gum(string& ply_, string& error_msg_);
    
    inline string stl_path() { return stl_file_path; };
    inline string stl_name() { return fname; };
    inline char stl_jaw_type() { return jaw_type; };
    inline void set_colors(int id, Eigen::RowVector3d color) { colors.emplace(id, color); };
    inline Eigen::RowVector3d get_color(int id) { return colors[id]; };
    inline void set_teeth_comp(map<string, string> stl_) { teeth_comp_stl = stl_; };
    inline void mesh_add_tooth(int id) { teeth_id.push_back(id); };
    inline bool mesh_is_tooth(int id) { if (find(teeth_id.begin(), teeth_id.end(), id) != teeth_id.end()) return true; else false; };
    inline int mesh_max_tooth_id() { return *max_element(teeth_id.begin(), teeth_id.end()); };
    inline int mesh_min_tooth_id() { return *min_element(teeth_id.begin(), teeth_id.end()); };
};
