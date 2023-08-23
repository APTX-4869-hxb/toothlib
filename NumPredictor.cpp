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
bool ReadAllFile(const std::string& strFileName, std::string& strFileData)
{
    std::ifstream in(strFileName, std::ios::in | std::ios::binary);
    if (!in.is_open())
    {
        return false;
    }
    std::istreambuf_iterator<char> beg(in), end;
    strFileData = std::string(beg, end);
    in.seekg(0, std::ios::end);//移动的文件尾部
    int strFileSize = in.tellg();//获取文件的整体大小
    in.close();
    return true;
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
    this->staging_num = 0;
}
NumPredictor::~NumPredictor() {

}
void NumPredictor::setPoses(map<string, vector<vector<float>>> start, map<string, vector<vector<float>>> end) {
    map<string, vector<vector<float>>>::iterator it;
    for (it = start.begin(); it != start.end(); it++) {
        // pos
        int id = this->ids[atoi(it->first.c_str())];//0-28
        std::cout << endl << "[" << id << "]" << endl;
        for (int j = 0; j < 3; j++) {
            this->start_pose[id][j] = it->second[j][3];
            std::cout << this->start_pose[id][j] << " ";
        }
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
        std::cout << endl << "[" << id << "]" << endl;
        for (int j = 0; j < 3; j++) {
            this->target_pose[id][j] = it->second[j][3];
            //std::cout << this->target_pose[id][j] << " ";
        }
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
    float o1 = _copysign(x, mat9d[2][1] - mat9d[1][2]);
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
        std::cout << "dis:" << temp_dis << " [" << this->start_pose[id][0] - this->target_pose[id][0]
            << "," << this->start_pose[id][1] - this->target_pose[id][1] << "," << this->start_pose[id][2] - this->target_pose[id][2]
            << endl;
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
        float x = axis_x1.dot(axis_x2) / (cal_vector_mode(axis_x1(0), axis_x1(1), axis_x1(2)) * cal_vector_mode(axis_x2(0), axis_x2(1), axis_x2(2)));
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
    std::cout << "refer pos/rot number: " << this->refer_pos_num << " " << this->refer_rot_num << endl;
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
    torch::Tensor data_tensor = torch::cat({ data_tensor_start, data_tensor_end }, 1);
    torch::Tensor input_tensor = torch::cat({ data_tensor.view({-1}), var1_tensor, var2_tensor });

    // 确保输入张量的形状与模型的预期输入形状相匹配
    input_tensor = input_tensor.view({ 1, 394 });
    std::vector<torch::jit::IValue> inputs; // 创建一个输入向量
    inputs.push_back(input_tensor); // 创建一个随机输入
    module.eval();
    at::Tensor output = module.forward(inputs).toTensor(); // 运行模型并获取输出
    std::cout << output[0].item<float>() << std::endl;
    this->staging_num = int(round(output[0].item<float>()));
    if (this->staging_num <= 0 || this->staging_num > 300) {
        this->staging_num = int(round(0.5 * (this->refer_pos_num + this->refer_rot_num)));
    }
    return this->staging_num;
}

int NumPredictor::adjust_result() {
    // 调整结果使其能够均匀插值关键帧
    int add_len = 0;
    int frame_num = this->staging_num;
    if (frame_num > 30) {
        bool flag = false;
        int factor = -1;
        if (notPrime[frame_num - 1] == false) {// frame-1 素数
            add_len = 1;
            for (int i = 5; i > 1; i--) {
                if (frame_num% i == 0 && frame_num / i < 30) {
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
                if ((frame_num - 1) % i == 0 && (frame_num - 1) / i < 30) {
                    flag = true;
                }
                else if ((frame_num - 1) % i) {
                    factor = i;
                }
            }
            if (!flag) {
                if (factor <= 0) {
                    factor = 2;
                }
                add_len = ((frame_num - 1) / factor + 1) * factor - frame_num + 1;
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
int& NumPredictor::getStagingNum() {
    return this->staging_num;
}

StagingGenerator::StagingGenerator() {
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
}

StagingGenerator::~StagingGenerator() {

}

vector<int> StagingGenerator::cal_remove_idx() {
    vector<int> res;
    return res;
}

void StagingGenerator::load_config() {

}
vector<torch::Tensor> StagingGenerator::load_data(map<string, vector<vector<float>>> start, map<string, vector<vector<float>>> end) {
    map<string, vector<vector<float>>>::iterator it;
    std::cout << "staging num:" << this->staging_num << endl;
    torch::Tensor pos = torch::zeros({ 1,this->staging_num,28,3 }, torch::kFloat32);
    torch::Tensor rot9d = torch::zeros({ 1,this->staging_num,28,3,3 }, torch::kFloat32);
    vector<torch::Tensor> res;
    for (it = start.begin(); it != start.end(); it++) {
        // pos
        int id = this->ids[atoi(it->first.c_str())];//0-28
        //std::cout << endl << "[" << id << "]" << endl;
        for (int j = 0; j < 3; j++) {
            pos[0][0][id][j] = it->second[j][3];
        }
        // rot
        for (int j = 0; j < 3; j++) {
            for (int k = 0; k < 3; k++) {
                rot9d[0][0][id][j][k] = it->second[j][k];
            }
        }
    }
    for (it = end.begin(); it != end.end(); it++) {
        // pos
        int id = this->ids[atoi(it->first.c_str())];//0-28
        std::cout << endl << "[" << id << "]" << endl;
        for (int j = 0; j < 3; j++) {
            pos[0][staging_num - 1][id][j] = it->second[j][3];
        }
        // rot
        for (int j = 0; j < 3; j++) {
            for (int k = 0; k < 3; k++) {
                rot9d[0][staging_num - 1][id][j][k] = it->second[j][k];
            }
        }
    }
    res.push_back(pos);
    res.push_back(rot9d);
    std::cout << pos.sizes() << rot9d.sizes() << endl;
    return res;
}

vector<torch::Tensor> StagingGenerator::from_flat_data_joints(torch::Tensor x) {
    /*
    def from_flat_data_joint_data(x):
        # x:(b,seq,c)
        # return: p(b,seq,28,3) r6d
        r=x[...,:168].reshape((x.shape[0],x.shape[1],28,6))
        p=x[...,168:].reshape((x.shape[0],x.shape[1],28,3))
        return p,r
    */
    string func_dir = PROJECT_PATH + string("/model/func/from_flat_data_joint_data.pt");
    torch::jit::script::Module func;
    vector<torch::Tensor> res;
    try {
        func = torch::jit::load(func_dir);
    }
    catch (const c10::Error& e) {
        std::cerr << "Error loading the scripted function: " << e.msg() << std::endl;
        torch::Tensor re_ = torch::tensor({ 0 });
        res.push_back(re_);
    }
    std::vector<torch::jit::IValue> inputs;
    inputs.push_back(x);

    torch::jit::IValue output = func.forward(inputs);

    // 将输出转换为张量
    auto tpl_out = output.toTuple();
    auto p = tpl_out->elements()[0].toTensor();
    auto r = tpl_out->elements()[1].toTensor();
    std::cout << "from flat data to joint:" << r.sizes() << endl;

    res.push_back(p);
    res.push_back(r);
    return res;
}


torch::Tensor StagingGenerator::get_model_input(torch::Tensor p, torch::Tensor r) {
    /*
    def get_model_input(positions_3d, rotations_6d):
        # return x(b,seq,c)
        rot_6d = rotations
        rot = rot_6d.flatten(start_dim=-2)
        pos=positions.flatten(start_dim=-2)

        assert len(rot.shape)==len(pos.shape)
        x = torch.cat([rot, pos], dim=-1) # NOTICE:已经将原来的root pos改为所有pos
        return x
    */
    string func_dir = PROJECT_PATH + string("/model/func/get_model_input.pt");
    torch::jit::script::Module func;
    try {
        func = torch::jit::load(func_dir);
    }
    catch (const c10::Error& e) {
        std::cerr << "Error loading the scripted function: " << e.msg() << std::endl;
        torch::Tensor re_ = torch::tensor({ 0 });
        return re_;
    }
    std::vector<torch::jit::IValue> inputs;
    inputs.push_back(p);
    inputs.push_back(r);

    torch::jit::IValue output = func.forward(inputs);

    // 将输出转换为张量
    torch::Tensor output_tensor = output.toTensor();
    std::cout << "get_model_input:" << output_tensor.sizes() << endl;
    return output_tensor;
}
torch::Tensor StagingGenerator::get_new_pos(torch::Tensor positions, torch::Tensor y, int p_start_idx, int p_end_idx, int seq_slice_start, int seq_slice_stop) {
    /*
    def get_new_positions2(positions, y, p_start_idx: int, p_end_idx: int, seq_slice_start: int, seq_slice_stop: int):
        p_slice = slice(p_start_idx, p_end_idx)
        seq_slice=slice(seq_slice_start, seq_slice_stop)
        ###
        pos_tmp=y[..., seq_slice, p_slice]
        pos_tmp = pos_tmp.reshape(pos_tmp.shape[0], pos_tmp.shape[1], -1, 3)
        # print(pos_tmp.shape) # (batch,seq,joint,3)
        positions_new = positions.clone()
        positions_new[..., seq_slice, :, :] = pos_tmp    # FIXED:注意维度
        ###

        # positions_new = positions.clone()
        # positions_new[..., seq_slice, 0, :] = y[..., seq_slice, p_slice]

        return positions_new
    */
    string func_dir = PROJECT_PATH + string("/model/func/train_get_new_pos.pt");
    torch::jit::script::Module func;
    try {
        func = torch::jit::load(func_dir);
    }
    catch (const c10::Error& e) {
        std::cerr << "Error loading the scripted function: " << e.msg() << std::endl;
        torch::Tensor re_ = torch::tensor({ 0 });
        return re_;
    }
    std::vector<torch::jit::IValue> args;
    args.push_back(positions);
    args.push_back(y);
    args.push_back(p_start_idx);
    args.push_back(p_end_idx);
    args.push_back(seq_slice_start);
    args.push_back(seq_slice_stop);

    torch::jit::IValue output = func.forward(args);

    // 将输出转换为张量
    torch::Tensor output_tensor = output.toTensor();
    std::cout << "train-utils-get new pos:" << output_tensor.sizes() << endl;
    return output_tensor;
}

torch::Tensor StagingGenerator::get_new_rot(torch::Tensor y, int r_start_idx, int r_end_idx, int seq_slice_start, int seq_slice_stop, torch::Tensor rotations) {
    /*
    def get_new_rotations2(y, r_start_idx: int, r_end_idx: int, seq_slice_start: int, seq_slice_stop: int, rotations:Optional[torch.Tensor]):
        r_slice = slice(r_start_idx, r_end_idx)
        seq_slice=slice(seq_slice_start, seq_slice_stop)
        if rotations is None:
            rot = y[..., r_slice]
            rot = rot.reshape(rot.shape[0],rot.shape[1], -1, 6)
            rot = data_utils.matrix6D_to_9D_torch(rot)
        else:
            rot_tmp = y[..., seq_slice, r_slice]
            rot_tmp = rot_tmp.reshape(rot_tmp.shape[0],rot_tmp.shape[1], -1, 6)
            rot_tmp = data_utils.matrix6D_to_9D_torch(rot_tmp)

            rot = rotations.clone()
            rot[..., seq_slice, :, :, :] = rot_tmp

        return rot
    */
    string func_dir = PROJECT_PATH + string("/model/func/train_get_new_rot.pt");
    torch::jit::script::Module func;
    try {
        func = torch::jit::load(func_dir);
    }
    catch (const c10::Error& e) {
        std::cerr << "Error loading the scripted function: " << e.msg() << std::endl;
        torch::Tensor re_ = torch::tensor({ 0 });
        return re_;
    }
    std::vector<torch::jit::IValue> args;
    args.push_back(y);
    args.push_back(r_start_idx);
    args.push_back(r_end_idx);
    args.push_back(seq_slice_start);
    args.push_back(seq_slice_stop);
    args.push_back(rotations);

    torch::jit::IValue output = func.forward(args);

    // 将输出转换为张量
    torch::Tensor output_tensor = output.toTensor();
    std::cout << "train-utils-get new rot:" << output_tensor.sizes() << endl;
    return output_tensor;
}
torch::Tensor StagingGenerator::mat9d_to_6d(torch::Tensor m9d) {
    string func_dir = PROJECT_PATH + string("/model/func/data_utils_matrix9D_to_6D_torch.pt");
    torch::jit::script::Module func;
    try {
        func = torch::jit::load(func_dir);
    }
    catch (const c10::Error& e) {
        std::cerr << "Error loading the scripted function: " << e.msg() << std::endl;
        torch::Tensor re_ = torch::tensor({ 0 });
        return re_;
    }
    std::cout << "m9d to 6d:" << m9d.sizes() << endl;
    std::vector<torch::jit::IValue> inputs;
    inputs.push_back(m9d);

    torch::jit::IValue output = func.forward(inputs);

    // 将输出转换为张量
    torch::Tensor output_tensor = output.toTensor();
    std::cout << "m9d to 6d:" << output_tensor.sizes() << endl;
    return output_tensor;
}
torch::Tensor StagingGenerator::mat6d_to_9d(torch::Tensor m6d) {
    string func_dir = PROJECT_PATH + string("/model/func/data_utils_matrix6D_to_9D_torch.pt");
    torch::jit::script::Module func;
    try {
        func = torch::jit::load(func_dir);
    }
    catch (const c10::Error& e) {
        std::cerr << "Error loading the scripted function: " << e.msg() << std::endl;
        torch::Tensor re_ = torch::tensor({ 0 });
        return re_;
    }
    std::vector<torch::jit::IValue> inputs;
    inputs.push_back(m6d);

    torch::jit::IValue output = func.forward(inputs);

    // 将输出转换为张量
    torch::Tensor output_tensor = output.toTensor();
    std::cout << "m6d to 9d:" << output_tensor.sizes() << endl;
    return output_tensor;
}
torch::Tensor StagingGenerator::get_atten_mask(int windowlen, int contextlen, int targetidx) {
    /*
    def get_attention_mask(window_len:int, context_len:int, target_idx:int):
        atten_mask = torch.ones(window_len, window_len,
                                device="cpu", dtype=torch.bool)
        #atten_mask[:, target_idx] = False
        #atten_mask[:, :context_len] = False
        #atten_mask[:, midway_targets] = False
        atten_mask[:, :target_idx + 1] = False
        # assert context_len==13
        #print("have atten mask!")
        atten_mask = atten_mask.unsqueeze(0)

        # (1, seq, seq)
        return atten_mask
    */
    std::cout << "get atten mask:[window,context,target] " << windowlen << " " << contextlen << " " << targetidx << endl;
    string func_dir = PROJECT_PATH + string("/model/func/get_atten_mask.pt");
    torch::jit::script::Module func;
    try {
        func = torch::jit::load(func_dir);
    }
    catch (const c10::Error& e) {
        std::cerr << "Error loading the scripted function: " << e.msg() << std::endl;
        torch::Tensor re_ = torch::tensor({ 0 });
        return re_;
    }
    std::vector<torch::jit::IValue> inputs;

    inputs.push_back(windowlen);
    inputs.push_back(contextlen);
    inputs.push_back(targetidx);

    torch::jit::IValue output = func.forward(inputs);

    // 将输出转换为张量
    torch::Tensor output_tensor = output.toTensor();
    std::cout << "get_atten_mask:" << output_tensor.sizes() << endl;
    return output_tensor;
}

torch::Tensor StagingGenerator::get_data_mask(int windowlen, int d_mask, vector<vector<int> > constrained_slice,
    int contextlen, int targetidx, std::vector<int> midway_targets) {
    /*
    def get_data_mask(window_len:int, d_mask:int, constrained_slices:List[List[int]],
                      context_len:int, target_idx:int, device:torch.device, dtype:torch.dtype,
                      midway_targets:List[int]=()):
        # 0 for unknown and 1 for known
        data_mask = torch.zeros((window_len, d_mask), device=device, dtype=dtype)
        data_mask[:context_len, :] = 1
        data_mask[target_idx, :] = 1
        # NOTICE: 可以设置config中的d_mask和constrained_slice来固定特定牙齿位置
        for s in constrained_slices:
            # print("get_data_mask:{}".format(s))
            data_mask[midway_targets, s] = 1

        # (seq, d_mask)
        return data_mask
    */
    string func_dir = PROJECT_PATH + string("/model/func/get_data_mask.pt");
    torch::jit::script::Module func;
    try {
        func = torch::jit::load(func_dir);
    }
    catch (const c10::Error& e) {
        std::cerr << "Error loading the scripted function: " << e.msg() << std::endl;
        torch::Tensor re_ = torch::tensor({ 0 });
        return re_;
    }

    torch::Device ddevice(torch::kCPU);  // Set the appropriate device
    torch::Dtype ddtype = torch::kFloat32;
    // get contrained_slice
    int cs_len = constrained_slice.size();
    torch::Tensor _cs = torch::zeros({ cs_len,2 }, torch::kInt);
    for (int i = 0; i < cs_len; i++) {
        _cs[i][0] = constrained_slice[i][0];
        _cs[i][1] = constrained_slice[i][1];
    }
    cs_len = midway_targets.size();
    torch::Tensor _mid = torch::empty({ 0 }, torch::kInt);
    if (cs_len > 0) {
        _mid = torch::from_blob(midway_targets.data(), { cs_len }, torch::kInt);
    }
    std::cout << _cs << _mid << endl;
    // Convert constrained_slices to nested std::vector
    std::vector<torch::jit::IValue> args;
    std::cout << "data mask: windowlen " << windowlen << endl;
    args.push_back(windowlen);
    args.push_back(d_mask);
    args.push_back(_cs);
    args.push_back(contextlen);
    args.push_back(targetidx);
    args.push_back(_mid);

    torch::jit::IValue output = func.forward(args);

    // 将输出转换为张量
    torch::Tensor output_tensor = output.toTensor();
    std::cout << "get_data_mask:" << output_tensor.sizes() << endl;
    return output_tensor;

}
torch::Tensor StagingGenerator::get_keyframe_pos_indices(int windowlen, int seq_slice_start, int seq_slice_stop) {
    /*
    def get_keyframe_pos_indices(window_len:int, seq_slice_start:int,seq_slice_stop:int, dtype:torch.dtype, device:torch.device):
        # position index relative to context and target frame
        // TMP:删除了dtype, device
        ctx_idx = torch.arange(window_len, dtype=dtype, device=device)
        ctx_idx = ctx_idx - (seq_slice_start - 1)
        ctx_idx = ctx_idx[..., None]

        tgt_idx = torch.arange(window_len, dtype=dtype, device=device)
        #print(tgt_idx.device,seq_slice.stop.device)
        tgt_idx = -(tgt_idx - seq_slice_stop)
        tgt_idx = tgt_idx[..., None]
        # ctx_idx: (seq, 1), tgt_idx: (seq, 1)
        keyframe_pos_indices = torch.cat([ctx_idx, tgt_idx], dim=-1)
        # (1, seq, 2)
        return keyframe_pos_indices[None]
        */
    string func_dir = PROJECT_PATH + string("/model/func/get_keyframe_pos_indices.pt");
    torch::jit::script::Module func;
    try {
        func = torch::jit::load(func_dir);
    }
    catch (const c10::Error& e) {
        std::cerr << "Error loading the scripted function: " << e.msg() << std::endl;
        torch::Tensor re_ = torch::tensor({ 0 });
        return re_;
    }

    torch::Device ddevice(torch::kCPU);  // Set the appropriate device
    torch::Dtype ddtype = torch::kFloat32;
    // std::cout << "keyframe pos:" << windowlen << " " << seq_slice_start << " " << seq_slice_stop << endl;
    // Convert constrained_slices to nested std::vector
    std::vector<torch::jit::IValue> args;
    args.push_back(windowlen);
    args.push_back(seq_slice_start);
    args.push_back(seq_slice_stop);
    //args.push_back(ddevice);
    //args.push_back(ddtype);

    torch::jit::IValue output = func.forward(args);

    // 将输出转换为张量
    torch::Tensor output_tensor = output.toTensor();
    std::cout << "get_keyframe_pos_indices:" << output_tensor.sizes() << endl;
    return output_tensor;

}
torch::Tensor StagingGenerator::set_placeholder_root_pos(torch::Tensor x, int seq_slice_start, int seq_slice_stop, vector<int> midway_targets, int p_slice_start, int p_slice_stop) {
    /*
    def set_placeholder_root_pos(x, seq_slice_start:int,seq_slice_stop:int, midway_targets:List[int],
    p_slice_start:int,p_slice_stop:int):
        # set root position of missing part to linear interpolation of
        # root position between constrained frames (i.e. last context frame,
        # midway target frames and target frame).
        constrained_frames = [seq_slice_start - 1, seq_slice_stop]
        //TMP:删除midway constrained_frames.extend(midway_targets)
        constrained_frames.sort()
        p_slice = slice(p_slice_start,p_slice_stop)
        for i in range(len(constrained_frames) - 1):
            start_idx = constrained_frames[i]
            end_idx = constrained_frames[i + 1]
            start_slice = slice(start_idx, start_idx + 1)
            end_slice = slice(end_idx, end_idx + 1)
            inbetween_slice = slice(start_idx + 1, end_idx)

            x[..., inbetween_slice, p_slice] = \
                benchmark.get_linear_interpolation(
                    x[..., start_slice, p_slice],
                    x[..., end_slice, p_slice],
                    end_idx - start_idx - 1
            )
        return x
    */
    string func_dir = PROJECT_PATH + string("/model/func/set_placeholder_root_pos.pt");
    torch::jit::script::Module func;
    try {
        func = torch::jit::load(func_dir);
    }
    catch (const c10::Error& e) {
        std::cerr << "Error loading the scripted function: " << e.msg() << std::endl;
        torch::Tensor re_ = torch::tensor({ 0 });
        return re_;
    }
    int cs_len = midway_targets.size();
    torch::Tensor _mid = torch::empty({ 0 }, torch::kInt);
    if (cs_len > 0) {
        _mid = torch::from_blob(midway_targets.data(), { cs_len }, torch::kInt);
    }
    std::vector<torch::jit::IValue> args;
    args.push_back(x);
    args.push_back(seq_slice_start);
    args.push_back(seq_slice_stop);
    args.push_back(_mid);
    args.push_back(p_slice_start);
    args.push_back(p_slice_stop);

    torch::jit::IValue output = func.forward(args);

    // 将输出转换为张量
    torch::Tensor output_tensor = output.toTensor();
    std::cout << "set_placeholder_root_pos & linear interpolation:" << output_tensor.sizes() << endl;
    return output_tensor;
}

vector<torch::Tensor> StagingGenerator::evaluate(torch::jit::script::Module model, torch::Tensor positions, torch::Tensor rotations,
    vector<int> seq_slice, map<string, int> indices, torch::Tensor _mean, torch::Tensor _std,
    torch::Tensor atten_mask, std::vector<int> midway_targets) {
    int windowlen = positions.size(1);
    int contextlen = seq_slice[0];
    int targetidx = seq_slice[1];

    //rp_slice = slice(indices["r_start_idx"], indices["p_end_idx"])
    model.eval();
    /*if midway_targets :
            midway_targets.sort()
            atten_mask = atten_mask.clone().detach()
            atten_mask[0, :, midway_targets] = False
    */
    torch::Tensor x_orig = this->get_model_input(positions, rotations);
    // zscore
    torch::Tensor x_zscore = (x_orig - _mean) / _std;
    // data mask(seq, 1)
    vector<vector<int> > constrained_slices;
    vector<int> s; s.push_back(0); s.push_back(1);
    constrained_slices.push_back(s);
    std::cout << "eval: windowlen " << windowlen << endl;
    torch::Tensor data_mask = this->get_data_mask(
        windowlen, 1, constrained_slices, contextlen,
        targetidx, midway_targets);

    torch::Tensor keyframe_pos_idx = this->get_keyframe_pos_indices(
        window_len, seq_slice[0], seq_slice[1]);
    // FIXME:torch::cat
    torch::Tensor x = torch::cat({ x_zscore * data_mask,data_mask.expand({x_zscore.size(0),x_zscore.size(1), data_mask.size(1)}) }, -1);

    // p_slice = slice(indices["p_start_idx"], indices["p_end_idx"])
    x = this->set_placeholder_root_pos(x, seq_slice[0], seq_slice[1], midway_targets, indices["p_start_idx"], indices["p_end_idx"]);
    // calculate model output y
    torch::Tensor model_out = this->Net_forward(model, x, keyframe_pos_idx, atten_mask);
    torch::Tensor y = x_zscore.clone().detach();
    // slice , slight copy
    y.slice(1, seq_slice[0], seq_slice[1]) = model_out.slice(1, seq_slice[0], seq_slice[1]).slice(2, indices["r_start_idx"], indices["p_end_idx"]);
    //reverse zscore
    y = y * _std + _mean;
    // notice: 原来的版本中rotations一直是9D的，新版本输入evaluation的是6D的，因此转化一下
    rotations = this->mat6d_to_9d(rotations);//data_utils.matrix6D_to_9D_torch(rotations);
    // new pos and rot
    torch::Tensor pos_new = this->get_new_pos(positions, y, indices["p_start_idx"], indices["p_end_idx"], seq_slice[0], seq_slice[1]);//train_utils.get_new_positions(positions, y, indices, seq_slice);
    torch::Tensor rot_new = this->get_new_rot(y, indices["r_start_idx"], indices["r_end_idx"], seq_slice[0], seq_slice[1], rotations);//train_utils.get_new_rotations(y, indices, rotations, seq_slice);

    vector<torch::Tensor> res;
    res.push_back(pos_new);
    res.push_back(rot_new);
    return res;
}

void StagingGenerator::post_process() {}
void StagingGenerator::Solution(map<string, vector<vector<float>>> start, map<string, vector<vector<float>>> end, int frame_num) {

    string config = "cmpgeo_context_modelVMeanNoMask_nogeo";
    string dconfig = "newgeo_context_modelNOGEOdense";
    // TODO:load config by name
    this->staging_num = frame_num;
    vector<torch::Tensor> origin_data = this->load_data(start, end);
    // TODO:init model and d_model
    torch::jit::script::Module model;
    torch::jit::script::Module dmodel;

    string generator_dir = PROJECT_PATH + string("/model/scripted_model.pt");
    try {
        model = torch::jit::load(generator_dir);
    }
    catch (const c10::Error& e) {
        std::cerr << "Error loading the traced model: " << e.msg() << std::endl;
        return;
    }

    generator_dir = PROJECT_PATH + string("/model/scripted_model_dense.pt");
    try {
        dmodel = torch::jit::load(generator_dir);
    }
    catch (const c10::Error& e) {
        std::cerr << "Error loading the traced model: " << e.msg() << std::endl;
        return;
    }

    map<string, int> indices = { {"r_start_idx", 0},
            {"r_end_idx" , 168},
            {"p_start_idx", 168},
            {"p_end_idx",252 } };

    std::ifstream stats(PROJECT_PATH + string("/model/stats.json"));
    json j_stat;
    stats >> j_stat;
    std::vector<float> vmean = j_stat["mean"];
    std::vector<float> vstd = j_stat["std"];

    std::ifstream statsd(PROJECT_PATH + string("/model/stats_dense.json"));
    json j_statd;
    statsd >> j_statd;
    std::vector<float> vmean_d = j_statd["mean"];
    std::vector<float> vstd_d = j_statd["std"];
    torch::Tensor mean_ = torch::from_blob(vmean.data(), { 252 });
    torch::Tensor std_ = torch::from_blob(vstd.data(), { 252 });
    torch::Tensor mean_d = torch::from_blob(vmean_d.data(), { 252 });
    torch::Tensor std_d = torch::from_blob(vstd_d.data(), { 252 });

    // TODO:fill_value = mean; fill_value_d=mean_d;
    torch::Tensor fill_value = mean_;
    torch::Tensor fill_value_d = mean_d;
    //torch::Tensor fill_value_p, fill_value_r6d;// TODO:fill_value_p, fill_value_r6d;

    // TODO:load data：pos, rot9d, frame, geo,remove_idx->get r6d
    torch::Tensor positions = origin_data[0];
    torch::Tensor rot9d = origin_data[1];
    torch::Tensor rot6d;
    vector<int> remove_idx;
    int frame = this->staging_num;
    int sparse_trans = 30;
    int dense_trans = 5;
    if (frame <= 30) {
        sparse_trans = frame - 2;
        dense_trans = 0;
    }
    else {
        for (int j = 5; j > 1; j--) {
            if ((frame - 1) % j == 0 && (frame - 1) / j < 30) {
                sparse_trans = (frame - 1) / j - 1;
                dense_trans = j - 1;
            }
        }
    }
    cout << "key frames:" << sparse_trans + 1 << " interval:" << dense_trans << endl;
    rot6d = this->mat9d_to_6d(rot9d);
    /*for removed teeth :
    int remove_len = remove_idx.size();
    for (int j = 0; j < remove_len; j++) {
        positions.index({ 0,Slice(),j,Slice() }) = fill_value_p.index({ 0,Slice(),j,Slice() });
        rot6d.index({ 0,Slice(),j,Slice() }) = fill_value_r6d.index({ 0,Slice(),j,Slice() });
    }
    rotations = m6d_to_9d(rot6d);
    */
    vector<int> sparse_seq; sparse_seq.push_back(0); sparse_seq.push_back(frame);
    torch::Tensor sp_pos = positions.slice(1, sparse_seq[0], sparse_seq[1], dense_trans + 1).clone();
    torch::Tensor sp_rot6d = rot6d.slice(1, sparse_seq[0], sparse_seq[1], dense_trans + 1).clone();
    int target_idx = context_len + sparse_trans;
    vector<int> seq_slice;
    seq_slice.push_back(context_len);
    seq_slice.push_back(target_idx);
    window_len = sparse_trans + 2;

    std::vector<int> midway;
    torch::Tensor atten_mask = this->get_atten_mask(window_len, context_len, target_idx);
    if ((frame - 1) % (dense_trans + 1) == 0) {
        int _final_anchor = (frame - 1) / (dense_trans + 1);
        midway.push_back(_final_anchor);
    }
    auto sp_new = this->evaluate(model, sp_pos.clone(), sp_rot6d.clone(), seq_slice, indices, mean_, std_, atten_mask, midway);

    vector<int> res_slice; res_slice.push_back(seq_slice[0] - 1);   res_slice.push_back(seq_slice[1] + 1);
    torch::Tensor pos_new = positions.clone().detach();
    torch::Tensor rot9d_new = rot9d.clone().detach();
    if (dense_trans) {
        pos_new.slice(1, sparse_seq[0], sparse_seq[1], dense_trans + 1) = sp_new[0].slice(1, res_slice[0], res_slice[1]).clone();
        rot9d_new.slice(1, sparse_seq[0], sparse_seq[1], dense_trans + 1) = sp_new[1].slice(1, res_slice[0], res_slice[1]).clone();
        torch::Tensor rot6d_new = this->mat9d_to_6d(rot9d_new);
        vector<int> key_id_list;
        for (int i = 0; i < frame; i += (dense_trans + 1)) {
            key_id_list.push_back(i);
        }
        for (int j = 0; j < key_id_list.size() - 1; j++) {
            int start_idx = key_id_list[j];
            int end_idx = key_id_list[j + 1];
            vector<int> dense_seq; dense_seq.push_back(start_idx); dense_seq.push_back(end_idx + 1);
            torch::Tensor den_pos = pos_new.slice(1, dense_seq[0], dense_seq[1]).clone();
            torch::Tensor  den_rot = rot6d_new.slice(1, dense_seq[0], dense_seq[1]).clone();

            target_idx = 1 + dense_trans;
            seq_slice.clear(); seq_slice.push_back(1); seq_slice.push_back(target_idx);
            window_len = dense_trans + 2;

            torch::Tensor atten_mask_d = this->get_atten_mask(window_len, 1, target_idx);
            vector<int> mid_d;
            auto den_new = this->evaluate(dmodel, den_pos, den_rot, seq_slice, indices, mean_d, std_d, atten_mask_d, mid_d);

            vector<int> res_dense_slice;
            res_dense_slice.push_back(seq_slice[0] - 1);
            res_dense_slice.push_back(seq_slice[1] + 1);
            pos_new.slice(1, dense_seq[0], dense_seq[1]) = den_new[0].slice(1, res_dense_slice[0], res_dense_slice[1]).clone();
            rot9d_new.slice(1, dense_seq[0], dense_seq[1]) = den_new[1].slice(1, res_dense_slice[0], res_dense_slice[1]).clone();
        }
    }
    else {
        pos_new = sp_new[0].slice(1, res_slice[0], res_slice[1]).clone();
        rot9d_new = sp_new[1].slice(1, res_slice[0], res_slice[1]).clone();
    }


}

torch::Tensor StagingGenerator::Net_forward(torch::jit::script::Module model, torch::Tensor x, torch::Tensor keypos, torch::Tensor mask) {

    torch::Tensor input1 = x;//torch::randn({ 1, 30,253 });
    torch::Tensor input2 = keypos;// torch::randn({ 1, 30,2 });
    torch::Tensor input3 = mask;// torch::zeros({ 1, 30,30 }, torch::kBool);

    // 将输入打包成一个元组
    std::vector<torch::jit::IValue> inputs;
    inputs.push_back(input1);
    inputs.push_back(input2);
    inputs.push_back(input3);

    // 设置模型为评估模式
    model.eval();

    // 执行推理
    torch::jit::IValue output = model.forward(inputs);

    // 将输出转换为张量
    torch::Tensor output_tensor = output.toTensor();

    // 打印输出张量的形状大小
    std::cout << "Output Tensor Shape: " << output_tensor.sizes() << std::endl;
    return output_tensor;
}