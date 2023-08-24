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
#include <nlohmann/json.hpp>
using json = nlohmann::json;
using namespace std;
using namespace torch::indexing;
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

	bool notPrime[500];
public:
	map<int, int> ids;
	int staging_num;
public:
	NumPredictor();
	~NumPredictor();
	void setPoses(map<string, vector<vector<float>>> start, map<string, vector<vector<float>>> end);
	vector<float> mat9D_to_quat(vector<vector<float>> mat9d);
	void cal_refer_num();
	int MLP_Net_forward();
	int adjust_result();
	int Solution(map<string, vector<vector<float>>> start, map<string, vector<vector<float>>> end);
	int& getStagingNum();
};

class StagingGenerator {
public:
	int staging_num=0;
	vector<float> start_frame;
	vector<float> target_frame;
	vector< vector<float> > all_frames;
	int sparse_trans;
	int dense_trans;

private:
	map<int, int> ids;
	int context_len = 1;
	int window_len = 30;


public:
	StagingGenerator();
	~StagingGenerator();
	void load_config();
	vector<torch::Tensor> load_data(map<string, vector<vector<float>>> start, map<string, vector<vector<float>>> end);
	vector<int> cal_remove_idx();
	vector<torch::Tensor> from_flat_data_joints(torch::Tensor x);
	torch::Tensor get_model_input(torch::Tensor p, torch::Tensor r);
	torch::Tensor get_atten_mask(int windowlen, int contextlen, int targetidx);
	torch::Tensor get_data_mask(int windowlen, int d_mask, vector<vector<int> > constrained_slice, int contextlen, int targetidx, std::vector<int> midway_targets);
	torch::Tensor get_keyframe_pos_indices(int windowlen, int seq_slice_start, int seq_slice_stop);
	torch::Tensor set_placeholder_root_pos(torch::Tensor x, int seq_slice_start, int seq_slice_stop, vector<int> midway_targets, int p_slice_start, int p_slice_stop);
	
	torch::Tensor mat9d_to_6d(torch::Tensor m9d);
	torch::Tensor mat6d_to_9d(torch::Tensor m6d);
	torch::Tensor get_new_pos(torch::Tensor positions, torch::Tensor y, int p_start_idx, int p_end_idx, int seq_slice_start,int seq_slice_stop);
	torch::Tensor get_new_rot(torch::Tensor y, int r_start_idx, int r_end_idx, int seq_slice_start, int seq_slice_stop, torch::Tensor rotations);

	vector<torch::Tensor> evaluate(torch::jit::script::Module model, torch::Tensor positions, torch::Tensor rotations, vector<int> seq_slice, map<string, int> indices, torch::Tensor _mean, torch::Tensor _std, torch::Tensor atten_mask,  vector<int> midway_targets);

	void post_process();
	map<string, vector<vector<float>>> Solution(map<string, vector<vector<float>>> start, map<string, vector<vector<float>>> end, int frame_num);
private:
	torch::Tensor Net_forward(torch::jit::script::Module model,torch::Tensor x, torch::Tensor keypos, torch::Tensor mask);
};

