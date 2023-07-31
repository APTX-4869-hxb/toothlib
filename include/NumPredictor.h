#pragma once
#include <iostream>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <Eigen/Core>
#include <Eigen/Geometry>
#include <Eigen/Dense>
#include "path.h"
#include "cpr/cpr.h"
#include <torch/script.h>
using namespace std;
namespace fs = std::filesystem;
float _sqrt_positive_part(float x);
float cal_vector_mode(float a, float b, float c);
class NumPredictor
{
private:
	float start_pose[28][7];
	float target_pose[28][7];
	map<string, Eigen::Matrix3d> start_mat9d;
    map<string, Eigen::Matrix3d> end_mat9d;
	float refer_pos_num;
	float refer_rot_num;
    int staging_num;
    bool notPrime[500];
public:
    map<int, int> ids;
public:
	NumPredictor();
	~NumPredictor();
	void setPoses(map<string, vector<vector<float>>> start, map<string, vector<vector<float>>> end);
	vector<float> mat9D_to_quat(vector<vector<float>> mat9d);
	void cal_refer_num();
	int MLP_Net_forward();
    int adjust_result();
    int Solution(map<string, vector<vector<float>>> start, map<string, vector<vector<float>>> end);
};

