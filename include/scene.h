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
	map<string, int> gum_id;

public:
	map<string, vector<float>> poses;
	map<string, vector<vector<float>>> teeth_axis;
	map<string, vector<vector<float>>> teeth_axis_origin;
	model upper_jaw_model;
	model lower_jaw_model;
	string upper_gum_path;
	string lower_gum_path;
	int last_selected;
	bool has_gum = false;

	scene(); 
	scene(string fpath_1, string fpath_2);
	~scene();

	bool segment_jaws();
	bool arrangement();
	bool generate_gums(HMODULE hdll);
	bool gum_deform(Eigen::Matrix4d P, string label, HMODULE hdll, string gum, vector<vector<float>>& vertices, vector<vector<int>>& faces);
	bool calc_poses();
	void load_scene();
	void save_scene();

	inline string stl_name() { return fname; };
	inline map<string, string> get_teeth_comp() { return teeth_comp_ply; };
	inline void mesh_add_tooth(int id, string label) { teeth_id.push_back(id); teeth_id_label_map.insert(pair<int, string>(id, label)); };
	inline bool mesh_is_tooth(int id) { if (find(teeth_id.begin(), teeth_id.end(), id) != teeth_id.end()) return true; else false; };
	inline int mesh_max_tooth_id() { return *max_element(teeth_id.begin(), teeth_id.end()); };
	inline int mesh_min_tooth_id() { return *min_element(teeth_id.begin(), teeth_id.end()); };
	inline string& get_tooth_label(int id) { return teeth_id_label_map[id]; }

	inline void mesh_add_gum(string type, int id) { gum_id.insert(pair<string, int>(type, id)); };
	inline int get_gum_id(string type) { return gum_id[type]; };

	inline void set_colors(int id, Eigen::RowVector3d color) { colors.emplace(id, color); };
	inline Eigen::RowVector3d get_color(int id) { return colors[id]; };
};