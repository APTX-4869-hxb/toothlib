#include "NumPredictor.h"
#define AVG_POS_DIS 0.0368885592038585
#define AVG_ROT_DIS 0.008818389680610432
float _sqrt_positive_part(float x) {
    // Returns torch.sqrt(torch.max(0, x)) but with a zero subgradient where x is 0.
    float res = 0;
    if (x > 0) {
        res = sqrt(x);
    }
    return res;
}
float cal_vector_mode(float a, float b, float c) {
    return sqrt(a * a + b * b + c * c);
}
NumPredictor::NumPredictor() {
    int cnt = 0;
    for (int i = 17; i > 10; i--) {
        this->ids[i] = cnt;
        cnt += 1;
    }
    for (int i = 21; i < 28; i++) {
        this->ids[i] = cnt;
        cnt += 1;
    }
    for (int i = 47; i > 40; i--) {
        this->ids[i] = cnt;
        cnt += 1;
    }
    for (int i = 31; i < 38; i++) {
        this->ids[i] = cnt;
        cnt += 1;
    }
    for (int i = 2; i < 500; i++) {
        if (notPrime[i] == false) {
            int j = 2 * i;
            while (j < 500) {
                notPrime[j] = true;
                j += i;
            }
        }
    }
}
NumPredictor::~NumPredictor() {

}
void NumPredictor::setPoses(map<string, vector<vector<float>>> start, map<string, vector<vector<float>>> end) {
    map<string, vector<vector<float>>>::iterator it;
    for (it = start.begin(); it != start.end(); it++) {
        // pos
        int id = this->ids[atoi(it->first.c_str())];//0-28
        for (int j = 0; j < 3; j++)
            this->start_pose[id][j] = it->second[j][3];
        // rot
        vector<float> temp_rot = this->mat9D_to_quat(it->second);
        for (int j = 0; j < 4; j++) {
            this->start_pose[id][j + 3] = temp_rot[j];
        }
        // mat9d
        Eigen::Matrix3d temp;
        temp << it->second[0][0], it->second[0][1], it->second[0][2],
            it->second[1][0], it->second[1][1], it->second[1][2],
            it->second[2][0], it->second[2][1], it->second[2][2];
        this->start_mat9d[it->first] = temp;
    }
    for (it = end.begin(); it != end.end(); it++) {
        // pos
        int id = this->ids[atoi(it->first.c_str())];//0-28
        for (int j = 0; j < 3; j++)
            this->target_pose[id][j] = it->second[j][3];
        // rot
        vector<float> temp_rot = this->mat9D_to_quat(it->second);
        for (int j = 0; j < 4; j++) {
            this->target_pose[id][j + 3] = temp_rot[j];
        }
        // mat9d
        Eigen::Matrix3d temp;
        temp << it->second[0][0], it->second[0][1], it->second[0][2],
            it->second[1][0], it->second[1][1], it->second[1][2],
            it->second[2][0], it->second[2][1], it->second[2][2];
        this->end_mat9d[it->first] = temp;
    }

}

vector<float> NumPredictor::mat9D_to_quat(vector<vector<float>> mat9d) {
	float m00 = mat9d[0][0];
	float m11 = mat9d[1][1];
	float m22 = mat9d[2][2];
    float o0 = 0.5 * _sqrt_positive_part(1 + m00 + m11 + m22);
    float x = 0.5 * _sqrt_positive_part(1 + m00 - m11 - m22);
    float y = 0.5 * _sqrt_positive_part(1 - m00 + m11 - m22);
    float z = 0.5 * _sqrt_positive_part(1 - m00 - m11 + m22);
    float o1 = _copysign(x, mat9d[ 2][1] - mat9d[1][2]);
    float o2 = _copysign(y, mat9d[0][2] - mat9d[2][0]);
    float o3 = _copysign(z, mat9d[1][0] - mat9d[0][1]);
    vector<float> quat;
    quat.push_back(o0);
    quat.push_back(o1);
    quat.push_back(o2);
    quat.push_back(o3);
    return quat;
}

    
void NumPredictor::cal_refer_num() {
    map<string, Eigen::Matrix3d>::iterator it;
    float pos_sum = 0;
    float rot_sum = 0;
    int cnt = 0;
    for (it = this->start_mat9d.begin(); it != this->start_mat9d.end(); it++) {
        string name = it->first;
        int id = this->ids[atoi(name.c_str())];
        //pos
        float temp_dis = cal_vector_mode(this->start_pose[id][0] - this->target_pose[id][0], 
            this->start_pose[id][1] - this->target_pose[id][1], 
            this->start_pose[id][2] - this->target_pose[id][2]);
        pos_sum += temp_dis;
        //rot
        Eigen::Matrix3d mat1 = it->second;
        Eigen::Matrix3d mat2 = this->end_mat9d[name];
        Eigen::Vector3d axis_x1 = mat1 * Eigen::Vector3d(1, 0, 0);//牙轴当前
        Eigen::Vector3d axis_y1 = mat1 * Eigen::Vector3d(0, 1, 0);
        Eigen::Vector3d axis_z1 = mat1 * Eigen::Vector3d(0, 0, 1);

        Eigen::Vector3d axis_x2 = mat2 * Eigen::Vector3d(1, 0, 0);//牙轴目标
        Eigen::Vector3d axis_y2 = mat2 * Eigen::Vector3d(0, 1, 0);
        Eigen::Vector3d axis_z2 = mat2 * Eigen::Vector3d(0, 0, 1);
        float x = axis_x1.dot(axis_x2) / (cal_vector_mode(axis_x1(0),axis_x1(1),axis_x1(2)) * cal_vector_mode(axis_x2(0), axis_x2(1), axis_x2(2)));
        float y = axis_y1.dot(axis_y2) / (cal_vector_mode(axis_y1(0), axis_y1(1), axis_y1(2)) * cal_vector_mode(axis_y2(0), axis_y2(1), axis_y2(2)));
        float z = axis_z1.dot(axis_z2) / (cal_vector_mode(axis_z1(0), axis_z1(1), axis_z1(2)) * cal_vector_mode(axis_z2(0), axis_z2(1), axis_z2(2)));
        x = acos(min(x, float(1.0)));
        y = acos(min(y, float(1.0)));
        z = acos(min(z, float(1.0)));
        rot_sum += (abs(x) + abs(y) + abs(z));

        cnt += 1;
    }
    this->refer_pos_num = pos_sum / cnt / AVG_POS_DIS;
    this->refer_rot_num = rot_sum / cnt / AVG_ROT_DIS;
}

int NumPredictor::MLP_Net_forward() {
    // data,refer
    torch::jit::script::Module module; // 创建一个Module对象
    string mlp_dir = PROJECT_PATH + string("/model/mlp_cpu.pt");
    try {
        // 反序列化保存的模型
        module = torch::jit::load(mlp_dir);
    }
    catch (const torch::Error& e) {
        std::cerr << "加载模型失败\n";
        return -1;
    }
    torch::Tensor data_tensor_start = torch::from_blob(this->start_pose, { 28, 7 });
    torch::Tensor data_tensor_end = torch::from_blob(this->target_pose, { 28, 7 });

    // 将两个float变量转换为torch::Tensor
    torch::Tensor var1_tensor = torch::tensor({ this->refer_pos_num });
    torch::Tensor var2_tensor = torch::tensor({ this->refer_rot_num });

    // 将这三个tensor连接在一起
    torch::Tensor data_tensor = torch::cat({ data_tensor_start, data_tensor_end },1);
    torch::Tensor input_tensor = torch::cat({ data_tensor.view({-1}), var1_tensor, var2_tensor });

    // 确保输入张量的形状与模型的预期输入形状相匹配
    input_tensor = input_tensor.view({ 1, 394 });
    std::vector<torch::jit::IValue> inputs; // 创建一个输入向量
    inputs.push_back(torch::randn({ 1, 394 })); // 创建一个随机输入

    at::Tensor output = module.forward(inputs).toTensor(); // 运行模型并获取输出
    std::cout << output[0].item<float>() << std::endl;
    this->staging_num = int(round(output[0].item<float>()));
    return int(round(output[0].item<float>()));
}

int NumPredictor::adjust_result() {
    // 调整结果使其能够均匀插值关键帧
    int add_len = 0;
    int frame_num = this->staging_num;
    if (frame_num> 30) {
        bool flag = false;
        int factor = -1;
        if (notPrime[frame_num-1] == false) {// frame-1 素数
            add_len = 1;
            for (int i = 5; i > 1; i--) {
                if (frame_num % i == 0 && frame_num / i < 30) {
                    flag = true;
                }
                else if (frame_num % i) {
                    factor = i;
                }
            }
            if (!flag) {
                if (factor <= 0) {
                    factor = 2;
                }
                add_len = (frame_num / factor + 1) * factor - frame_num + 1;
            }
        }
        else {//frame-1合数
            for (int i = 5; i > 1; i--) {
                if ((frame_num-1) % i == 0 && (frame_num-1) / i < 30) {
                    flag = true;
                }
                else if ((frame_num-1) % i) {
                    factor = i;
                }
            }
            if (!flag) {
                if (factor <= 0) {
                    factor = 2;
                }
                add_len = ((frame_num-1) / factor + 1) * factor - frame_num + 1;
            }
        }
    }
    std::cout << "real frames: " << frame_num << endl;
    std::cout << "extra frames: " << add_len << endl;
    this->staging_num = frame_num + add_len;
    return add_len;
}

int NumPredictor::Solution(map<string, vector<vector<float>>> start, map<string, vector<vector<float>>> end) {
    this->setPoses(start, end);
    this->cal_refer_num();
    this->MLP_Net_forward();
    int add = adjust_result();
    return add;
}