#include <iostream>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <Eigen/Core>
#include <Eigen/Geometry>

#include "rapidjson/document.h"
#include "rapidjson/allocators.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include "cpr/cpr.h"

//#undef GetObject

using namespace rapidjson;
using namespace std;
namespace fs = std::filesystem;

class model {
private:
    string stl_file_path;
    string stl_file_urn;

    string fname;
    char jaw_type;
    //vector<int> teeth_id;
    //map<int, string> teeth_id_label_map;
    //map<int, Eigen::RowVector3d> colors;
    //map<string, string> teeth_comp_stl;
    map<string, string> teeth_comp_ply_urn;
    string gum_urn;
    string gum_ply;
    //std::vector<std::vector<double>> gum_vertices;
    //std::vector<std::vector<int>> gum_faces;
    double* gum_vertices_ptr;
    int* gum_faces_ptr;
    //ToothTransformation tt[40];
public:

    //int last_selected;
    void* gum_deformer_ptr;
    //map<string, vector<vector<float>>> teeth_axis;

    model();
    model(string fpath);
    model(Value val);
    ~model();
    
    bool segment_jaw(string& stl_, vector<int>& label_, map<string, vector<vector<float>>>& teeth_axis, map<string, string>& teeth_comp, string& error_msg_);
    bool generate_gum(Document& document_result, string& ply_, string& error_msg_);
    bool create_gum_deformer(Document& document_result, HMODULE hdll);
    Document save_model();

    inline string stl_path() { return stl_file_path; };
    inline string stl_name() { return fname; };
    inline string stl_urn() { return stl_file_urn; };

    inline char stl_jaw_type() { return jaw_type; };
    //inline void set_colors(int id, Eigen::RowVector3d color) { colors.emplace(id, color); };
    //inline Eigen::RowVector3d get_color(int id) { return colors[id]; };
    inline void set_teeth_comp_urn(map<string, string> ply_urn) { teeth_comp_ply_urn = ply_urn; };
    //inline map<string, string> get_teeth_comp() { return teeth_comp_stl; };
    //inline void mesh_add_tooth(int id, string label) { teeth_id.push_back(id); teeth_id_label_map.insert(pair<int, string>(id, label)); };
    //inline bool mesh_is_tooth(int id) { if (find(teeth_id.begin(), teeth_id.end(), id) != teeth_id.end()) return true; else false; };
    //inline int mesh_max_tooth_id() { return *max_element(teeth_id.begin(), teeth_id.end()); };
    //inline int mesh_min_tooth_id() { return *min_element(teeth_id.begin(), teeth_id.end()); };
    //inline string get_tooth_label(int id) { return teeth_id_label_map[id]; }

};
