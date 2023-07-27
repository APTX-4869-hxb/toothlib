#include <iostream>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <Eigen/Core>
#include <Eigen/Geometry>

#include "path.h"
#include "cpr/cpr.h"
#include "model.h"

using namespace std;
namespace fs = std::filesystem;

class scene {
private:
	string fname;
	map<string, string> teeth_comp_ply;
	map<string, string> teeth_comp_ply_urn;
	vector<int> teeth_id;
	map<int, string> teeth_id_label_map;
	map<int, Eigen::RowVector3d> colors;
	

public:
	map<string, vector<float>> poses;
	map<string, vector<vector<float>>> teeth_axis;
	model upper_jaw_model;
	model lower_jaw_model;
	string upper_gum_path;
	string lower_gum_path;
	int last_selected;

	scene(); 
	scene(string fpath_1, string fpath_2);
	~scene();

	bool segment_jaws();
	bool arrangement();
	bool generate_gums();
	inline string stl_name() { return fname; };
	bool calc_poses();
	inline map<string, string> get_teeth_comp() { return teeth_comp_ply; };
	inline void mesh_add_tooth(int id, string label) { teeth_id.push_back(id); teeth_id_label_map.insert(pair<int, string>(id, label)); };
	inline bool mesh_is_tooth(int id) { if (find(teeth_id.begin(), teeth_id.end(), id) != teeth_id.end()) return true; else false; };
	inline int mesh_max_tooth_id() { return *max_element(teeth_id.begin(), teeth_id.end()); };
	inline int mesh_min_tooth_id() { return *min_element(teeth_id.begin(), teeth_id.end()); };
	inline string& get_tooth_label(int id) { return teeth_id_label_map[id]; }
	//inline vector<float>& get_pose(string label) { return poses[label]; }
	//inline void set_pose(string label, vector<float> pose) {  poses[label] = pose; }
	inline void set_colors(int id, Eigen::RowVector3d color) { colors.emplace(id, color); };
	inline Eigen::RowVector3d get_color(int id) { return colors[id]; };
};