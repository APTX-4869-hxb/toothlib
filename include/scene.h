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
	//map<string, string> teeth_comp_ply;
	map<string, string> teeth_comp_ply_urn;
	vector<int> teeth_id;
	map<int, string> teeth_id_label_map;
	map<int, Eigen::RowVector3d> colors;
	map<string, int> gum_id;

public:
	map<string, vector<float>> poses;
	map<string, vector<float>> poses_origin;
	map<string, vector<float>> poses_arranged;
	map<string, vector<vector<float>>> teeth_axis;
	map<string, vector<vector<float>>> teeth_axis_origin;
	map<string, vector<vector<float>>> teeth_axis_arranged;
	map<string, Eigen::Matrix4f> gizmo_mats;
	map<string, Eigen::Matrix4f> gizmo_mats_origin;
	map<string, Eigen::Matrix4f> gizmo_mats_arranged;
	map<string, vector<Eigen::Matrix4d>> staging_axis_mats;
	model upper_jaw_model;
	model lower_jaw_model;
	string upper_gum_path;
	string lower_gum_path;
	string upper_gum_doc_str;
	string lower_gum_doc_str;
	int last_selected;
	//bool has_gum = false;
	int status = 0;

	scene(); 
	scene(string fpath_1, string fpath_2);
	~scene();

	bool segment_jaws(string result_dir, map<string, string>& teeth_comp_ply);
	bool arrangement(map<string, string>& teeth_comp_ply);
	bool generate_gums(HMODULE hdll, string result_dir);
	bool gum_deform(Eigen::Matrix4d P, string label, HMODULE hdll, string gum, vector<vector<float>>& vertices, vector<vector<int>>& faces);
	bool calc_poses(map<string, vector<float>>& poses, map<string, vector<vector<float>>> teeth_axis);
	bool load_scene(string& result_dir);
	bool save_scene(string result_dir);

	inline string stl_name() { return fname; };
	inline map<string, string> get_teeth_comp_urn() { return teeth_comp_ply_urn; };
	//inline void set_teeth_comp_urn(map<string, string> ply_urn) { teeth_comp_ply_urn = ply_urn; };
	inline void mesh_add_tooth(int id, string label) { teeth_id.push_back(id); teeth_id_label_map.insert(pair<int, string>(id, label)); };
	inline bool mesh_is_tooth(int id) { if (find(teeth_id.begin(), teeth_id.end(), id) != teeth_id.end()) return true; else false; };
	inline int mesh_max_tooth_id() { return *max_element(teeth_id.begin(), teeth_id.end()); };
	inline int mesh_min_tooth_id() { return *min_element(teeth_id.begin(), teeth_id.end()); };
	inline string& get_tooth_label(int id) { return teeth_id_label_map[id]; }
	inline int get_tooth_id(string label) { for (auto pair : teeth_id_label_map) { if (pair.second == label) return pair.first; }; }
	inline void cout_tooth_id() { for (auto pair : teeth_id_label_map) { cout << pair.second << ":" << pair.first << endl; } }

	inline void mesh_add_gum(string type, int id) { gum_id.insert(pair<string, int>(type, id)); };
	inline int get_gum_id(string type) { return gum_id[type]; };
	inline string get_gum_type(int id) { for (auto pair : gum_id) { if (pair.second == id) return pair.first; }; };

	inline void set_colors(int id, Eigen::RowVector3d color) { colors.emplace(id, color); };
	inline Eigen::RowVector3d get_color(int id) { return colors[id]; };
};