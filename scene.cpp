//#include <Windows.h>
#include <igl/file_dialog_open.h>
#include <igl/readPLY.h>
#include <utils.h>
#include <scene.h>
#include <api.h>

scene::scene() {}

scene::scene(string fpath_1, string fpath_2) {
    vector<string> fpaths;
    fpaths.push_back(fpath_1);
    fpaths.push_back(fpath_2);
    string fname_1 = fs::path(fpath_1).filename().generic_string();
    string fname_2 = fs::path(fpath_2).filename().generic_string();

    stringstream ss_1(fname_1);
    getline(ss_1, fname_1, '_');
    stringstream ss_2(fname_2);
    getline(ss_2, fname_2, '_');

    if (fname_1 == fname_2) {
        fname = fname_1;
        cout << "The name of jaw file is: " << fname << endl;
    }
    else {
        cout << "please choose the jaws in the same pair." << endl;
        return;
    }

    for (auto fpath : fpaths) {
        string fname = fs::path(fpath).filename().generic_string();
        char jaw_type = fname[fname.find_first_of('.') - 1];

        if (jaw_type == 'L' || jaw_type == 'l') {
            lower_jaw_model = model(fpath);
        }
        else if (jaw_type == 'U' || jaw_type == 'u') {
            upper_jaw_model = model(fpath);
        }
        else {
            cout << "STL file name must end with u for upper jaw or l for lower jaw" << endl;
            return;
        }          
    }
}
scene::~scene() {}

bool scene::segment_jaws() {
    string result_stl, error_msg;
    vector<int> result_label;


    string result_dir = PROJECT_PATH + string("/result");
    auto result_dir_path = fs::path(result_dir);

    if (!fs::is_directory(result_dir_path)) 
        fs::create_directory(result_dir_path);

    if (!upper_jaw_model.segment_jaw(result_stl, result_label, teeth_axis, teeth_comp_ply, error_msg)) {
        cout << error_msg << endl;
        return false;
    }

    ofstream ofs;
    string res_mesh_name = result_dir + string("/seg_") + upper_jaw_model.stl_name() + string(".stl");
    ofs.open(res_mesh_name, ofstream::out | ofstream::binary);
    ofs << result_stl;
    ofs.close();

    string res_label_name = result_dir + string("/seg_") + upper_jaw_model.stl_name() + string(".txt");
    ofs.open(res_label_name, ofstream::out);
    for (const auto& e : result_label) ofs << e << endl;
    ofs.close();

    cout << "upper jaw segment complete..." << endl;

    if (!lower_jaw_model.segment_jaw(result_stl, result_label, teeth_axis, teeth_comp_ply, error_msg)) {
        cout << error_msg << endl;
        return false;
    }

    res_mesh_name = result_dir + string("/seg_") + lower_jaw_model.stl_name() + string(".stl");
    ofs.open(res_mesh_name, ofstream::out | ofstream::binary);
    ofs << result_stl;
    ofs.close();

    res_label_name = result_dir + string("/seg_") + lower_jaw_model.stl_name() + string(".txt");
    ofs.open(res_label_name, ofstream::out);
    for (const auto& e : result_label) ofs << e << endl;

    cout << "lower jaw segment complete..." << endl;

    return true;
}

bool scene::arrangement() {
    string error_msg;
    cpr::Response r;
    Document input_data(kObjectType);
    Document input_data_upper_mesh_config(kObjectType);
    Document input_data_lower_mesh_config(kObjectType);
    Document output_config(kObjectType);
    Document output_config_comp_mesh(kObjectType);
    Document request_body(kObjectType);
    Document document;
    Document document_result;

    // Step 1. make input
    // Step 1.1 upload to file server

    add_string_member(input_data_upper_mesh_config, "type", "stl");
    add_string_member(input_data_upper_mesh_config, "data", upper_jaw_model.stl_urn());
    input_data.AddMember(
        "upper_mesh",
        input_data_upper_mesh_config,
        input_data.GetAllocator());

    add_string_member(input_data_lower_mesh_config, "type", "stl");
    add_string_member(input_data_lower_mesh_config, "data", lower_jaw_model.stl_urn());
    input_data.AddMember(
        "lower_mesh",
        input_data_lower_mesh_config,
        input_data.GetAllocator());
    //cout << dump_json(input_data) << endl;

    add_string_member(output_config_comp_mesh, "type", "ply");
    output_config.AddMember(
        "arranged_comp",
        output_config_comp_mesh,
        output_config.GetAllocator());

    map<string, string> spec;
    spec.insert(pair<string, string>("spec_group", "mesh-processing"));
    spec.insert(pair<string, string>("spec_name", "oral-arrangement"));
    spec.insert(pair<string, string>("spec_version", "1.0-snapshot"));

    // Step 1.2 config request
    config_request(spec, input_data, output_config, request_body);

    // Step 2. submit job
    string job_id = submit_job(document, request_body, error_msg);
    if (job_id == "false") {
        cout << error_msg << endl;
        return false;
    }
    // Step 3. check job
    if (!check_job(job_id, error_msg)) {
        cout << error_msg << endl;
        return false;
    }
    // Step 4. get job result
    if (!get_job_result(job_id, document_result, error_msg)) {
        cout << error_msg << endl;
        return false;
    }

    //cout << dump_json(document_result) << endl;
    //return false;

    // Step 5 download mesh

    for (auto& v : document_result["arranged_comp"].GetObjectA()) {
        string download_urn = v.value["data"].GetString();
        assignToMap(teeth_comp_ply_urn, string(v.name.GetString()), download_urn);
        //teeth_comp_ply_urn.insert(pair<string, string>(v.name.GetString(), download_urn));
    }

    download_t_comp_mesh(document_result, teeth_comp_ply, "arranged_comp");

    map<string, string> upper_comp_ply_urn;
    map<string, string> lower_comp_ply_urn;

    for (auto teeth_urn : teeth_comp_ply_urn) {
        if (teeth_urn.first[0] == '3' || teeth_urn.first[0] == '4' || teeth_urn.first[0] == '7' || teeth_urn.first[0] == '8' || (atoi(teeth_urn.first.c_str()) > 94) && (atoi(teeth_urn.first.c_str()) < 99)) {
            lower_comp_ply_urn.insert(teeth_urn);
        }
        else if (teeth_urn.first[0] == '1' || teeth_urn.first[0] == '2' || teeth_urn.first[0] == '5' || teeth_urn.first[0] == '6' || (atoi(teeth_urn.first.c_str()) > 90) && (atoi(teeth_urn.first.c_str()) < 95))
            upper_comp_ply_urn.insert(teeth_urn);
    }
    upper_jaw_model.set_teeth_comp_urn(upper_comp_ply_urn);
    lower_jaw_model.set_teeth_comp_urn(lower_comp_ply_urn);

    for (auto& v : document_result["transformation_dict"].GetObjectA()) {
        Eigen::Matrix4d trans;
        for (int i = 0; i < v.value.Size(); i++)
            for (int j = 0; j < v.value[i].Size(); j++)
                trans(i, j) = v.value[i][j].GetFloat();
        
        Eigen::Matrix4d axis = vectorToMatrixXd(teeth_axis[v.name.GetString()]);
        Eigen::Matrix4d res = trans * axis;
        
        for (int i = 0; i < v.value.Size(); i++)
            for (int j = 0; j < v.value[i].Size(); j++)
                teeth_axis[v.name.GetString()][i][j] = res(i, j);
        //cout << v.name.GetString() << endl;
        //cout << trans << endl;
    }

    return true;
}

bool scene::generate_gums(HMODULE hdll) {

    string result_dir = PROJECT_PATH + string("/result");
    auto result_dir_path = fs::path(result_dir);

    if (!fs::is_directory(result_dir_path))
        fs::create_directory(result_dir_path);

    Document upper_document_result;
    string result_ply, error_msg;
    if (!upper_jaw_model.generate_gum(upper_document_result, result_ply, error_msg)) {
        cout << error_msg << endl;
        return false;
    }

    upper_gum_doc_str = dump_json(upper_document_result);

    upper_jaw_model.create_gum_deformer(upper_document_result, hdll);

    ofstream ofs;
    string res_mesh_name = result_dir + string("/") + upper_jaw_model.stl_name() + string("_gum.ply");
    ofs.open(res_mesh_name, ofstream::out | ofstream::binary);
    ofs << result_ply;
    ofs.close();

    upper_gum_path = res_mesh_name;

    cout << "upper gum generation complete..." << endl;

    Document lower_document_result;
    if (!lower_jaw_model.generate_gum(lower_document_result, result_ply, error_msg)) {
        cout << error_msg << endl;
        return false;
    }
    
    lower_gum_doc_str = dump_json(lower_document_result);

    lower_jaw_model.create_gum_deformer(lower_document_result, hdll);

    res_mesh_name = result_dir + string("/") + lower_jaw_model.stl_name() + string("_gum.ply");
    ofs.open(res_mesh_name, ofstream::out | ofstream::binary);
    ofs << result_ply;
    ofs.close();

    lower_gum_path = res_mesh_name;
    has_gum = true;

    cout << "lower gum generation complete..." << endl;

    return true;
}

bool scene::calc_poses() {
    for (auto axis : teeth_axis) {
        //cout << axis.first << endl;
        Eigen::Matrix4d pose_matrix_eigen = vectorToMatrixXd(axis.second);
        Eigen::Matrix3d R = pose_matrix_eigen.block<3, 3>(0, 0);
        Eigen::Vector3d T = pose_matrix_eigen.block<3, 1>(0, 3);
        Eigen::Vector3d euler_angles = R.eulerAngles(2, 1, 0); // 获取欧拉角，这里的顺序为 ZYX
        vector<float> pose = { float(T[0]), float(T[1]), float(T[2]), float(euler_angles[2]), float(euler_angles[1]), float(euler_angles[0]) };
        assignToMap(poses, axis.first, pose);
        //poses.insert(pair<string, vector<float>>(axis.first, pose));
    }

    return true;
}

bool scene::gum_deform(Eigen::Matrix4d P, string label, HMODULE hdll, string gum, vector<vector<float>>& vertices, vector<vector<int>>& faces) {

    DEFORM_FUNC deform = (DEFORM_FUNC)GetProcAddress(hdll, "deform");
    // prepare tooth transformation, relative to the first step
    // transformation matrix
    ToothTransformation tt[40];

    // rotation matrix
    Eigen::Matrix3d R = P.block<3, 3>(0, 0);
    Eigen::Vector3d T = P.block<3, 1>(0, 3);

    //cout << "R: " << R << endl;
    //cout << "T: " << T << endl;
    // assign tid to tt.tid
    tt[0].tid = atoi(label.c_str());
    // assign rotation to tt.rotation
    copy(R.data(), R.data() + 9, tt[0].rotation);
    // assign translation to tt.translation
    tt[0].translation[0] = T[0];
    tt[0].translation[1] = T[1];
    tt[0].translation[2] = T[2];

    // deform
    unsigned int n_teeth = 1;       // number of transformed teeth, could be more than 1
    double* p_deformed_vertices = new double[300000];        // the returned deformed vertices
    int* p_deformed_faces = new int[300000];
    unsigned int deformed_vertices_len, deformed_faces_len;
    int ret = 1;
    if (gum == "lower") {
        ret = deform(lower_jaw_model.gum_deformer_ptr,
            tt,
            n_teeth,
            p_deformed_vertices, &deformed_vertices_len,
            p_deformed_faces, &deformed_faces_len, NULL, NULL);
        //cout << gum << endl;

    }

    else if (gum == "upper") {
        ret = deform(upper_jaw_model.gum_deformer_ptr,
            tt,
            n_teeth,
            p_deformed_vertices, &deformed_vertices_len,
            p_deformed_faces, &deformed_faces_len, NULL, NULL);
        //cout << gum << endl;
    }

    //cout << ret << endl;
    if (ret)
        return false;

    // get result
    std::vector<std::vector<float>> deformed_vertices(deformed_vertices_len / 3);
    for (int i = 0; i < deformed_vertices.size(); i++) {
        deformed_vertices[i].push_back(p_deformed_vertices[i * 3]);
        deformed_vertices[i].push_back(p_deformed_vertices[i * 3 + 1]);
        deformed_vertices[i].push_back(p_deformed_vertices[i * 3 + 2]);
    }
    std::vector<std::vector<int>> deformed_faces(deformed_faces_len / 3);
    for (int i = 0; i < deformed_faces.size(); i++) {
        deformed_faces[i].push_back(p_deformed_faces[i * 3]);
        deformed_faces[i].push_back(p_deformed_faces[i * 3 + 1]);
        deformed_faces[i].push_back(p_deformed_faces[i * 3 + 2]);
    }

    vertices = deformed_vertices;
    faces = deformed_faces;
    ////cout << "deform compelete." << endl;
    return true;

}

bool scene::load_scene() {
    string json_name = igl::file_dialog_open();
    if (json_name == "")
        return false;
    ifstream t(json_name);
    string str((istreambuf_iterator<char>(t)),
        istreambuf_iterator<char>());

    Document document;
    document.Parse(str.c_str());

    fname = document["fname"].GetString();

    cout << "load completed teeth..." << endl;
    for (auto& v : document["teeth_comp_ply"].GetObjectA()) {
        string ply = v.value.GetString();
        assignToMap(teeth_comp_ply, string(v.name.GetString()), ply);
    }

    //string load_dir = PROJECT_PATH + string("/loadin");
    //for (auto ply : get_teeth_comp()) {
    //    ofstream ofs;
    //    string comp_name = load_dir + string("/seg_") + stl_name() + string("_comp_") + ply.first + string(".ply");
    //    ofs.open(comp_name, ofstream::out | ofstream::binary);
    //    ofs << ply.second;
    //    ofs.close();
    //}

    for (auto& v : document["teeth_comp_ply_urn"].GetObjectA()) {
        string urn = v.value.GetString();
        assignToMap(teeth_comp_ply_urn, string(v.name.GetString()), urn);
    }

    cout << "load axis..." << endl;
    for (auto& v : document["teeth_axis"].GetObjectA()) {
        vector<vector<float>> axis_vec;
        for (int i = 0; i < v.value.Size(); i++) {
            vector<float> axis;
            for (int j = 0; j < v.value[i].Size(); j++)
                axis.push_back(v.value[i][j].GetFloat());
            axis_vec.push_back(axis);
        }
        assignToMap(teeth_axis, string(v.name.GetString()), axis_vec);
        //cout << teeth_axis[string(v.name.GetString())][0][0] << " " << teeth_axis[string(v.name.GetString())][0][1] << " " << teeth_axis[string(v.name.GetString())][0][2] << " " << teeth_axis[string(v.name.GetString())][0][3] << endl;
    }
    for (auto& v : document["teeth_axis_origin"].GetObjectA()) {
        vector<vector<float>> axis_vec;
        for (int i = 0; i < v.value.Size(); i++) {
            vector<float> axis;
            for (int j = 0; j < v.value[i].Size(); j++)
                axis.push_back(v.value[i][j].GetFloat());
            axis_vec.push_back(axis);
        }
        assignToMap(teeth_axis_origin, string(v.name.GetString()), axis_vec);
    }
    for (auto& v : document["teeth_axis_arranged"].GetObjectA()) {
        vector<vector<float>> axis_vec;
        for (int i = 0; i < v.value.Size(); i++) {
            vector<float> axis;
            for (int j = 0; j < v.value[i].Size(); j++)
                axis.push_back(v.value[i][j].GetFloat());
            axis_vec.push_back(axis);
        }
        assignToMap(teeth_axis_arranged, string(v.name.GetString()), axis_vec);
    }
    cout << "load jaw model..." << endl;
    upper_jaw_model = model(document["upper_jaw_model"].GetObjectA());
    lower_jaw_model = model(document["lower_jaw_model"].GetObjectA());

    has_gum = atoi(document["has_gum"].GetString());
    if (has_gum) {
        //cout << "load gum..." << endl;
        //ofstream ofs;
        //string gum_name = load_dir + string("/") + upper_jaw_model.stl_name() + string("_gum.ply");
        //ofs.open(gum_name, ofstream::out | ofstream::binary);
        //ofs << upper_jaw_model.gum_ply;
        //ofs.close();
        upper_gum_path = document["upper_gum_path"].GetString();
        lower_gum_path = document["lower_gum_path"].GetString();

        upper_gum_doc_str = document["upper_gum_doc_str"].GetString();
        lower_gum_doc_str = document["lower_gum_doc_str"].GetString();
        //upper_gum_path = gum_name;

        //gum_name = load_dir + string("/") + lower_jaw_model.stl_name() + string("_gum.ply");
        //ofs.open(gum_name, ofstream::out | ofstream::binary);
        //ofs << lower_jaw_model.gum_ply;
        //ofs.close();

        //lower_gum_path = gum_name;
    }

    last_selected = atoi(document["last_selected"].GetString());

    map<string, string> upper_comp_ply_urn;
    map<string, string> lower_comp_ply_urn;

    for (auto teeth_urn : teeth_comp_ply_urn) {
        if (teeth_urn.first[0] == '3' || teeth_urn.first[0] == '4' || teeth_urn.first[0] == '7' || teeth_urn.first[0] == '8' || (atoi(teeth_urn.first.c_str()) > 94) && (atoi(teeth_urn.first.c_str()) < 99)) {
            lower_comp_ply_urn.insert(teeth_urn);
        }
        else if (teeth_urn.first[0] == '1' || teeth_urn.first[0] == '2' || teeth_urn.first[0] == '5' || teeth_urn.first[0] == '6' || (atoi(teeth_urn.first.c_str()) > 90) && (atoi(teeth_urn.first.c_str()) < 95))
            upper_comp_ply_urn.insert(teeth_urn);
    }
    upper_jaw_model.set_teeth_comp_urn(upper_comp_ply_urn);
    lower_jaw_model.set_teeth_comp_urn(lower_comp_ply_urn);

    cout << "calculate poses..." << endl;
    calc_poses();

    return true;
}

bool scene::save_scene() {
    string result_dir = PROJECT_PATH + string("/result");
    string res_scene_name = result_dir + string("/") + stl_name() + string("_scene.json");

    Document scene_data(kObjectType);
    Document teeth_comp_ply_data(kObjectType);
    Document teeth_comp_ply_urn_data(kObjectType);
    Document teeth_id_data(kObjectType);
    Document teeth_id_label_map_data(kObjectType);
    Document colors_data(kObjectType);
    Document gum_id_data(kObjectType);
    Document poses_data(kObjectType);
    Document teeth_axis_data(kObjectType);
    Document teeth_axis_origin_data(kObjectType);
    Document teeth_axis_arranged_data(kObjectType);
    Document upper_jaw_model_document(kObjectType);
    Document lower_jaw_model_document(kObjectType);

    add_string_member(scene_data, "fname", fname);
    for (auto ply : teeth_comp_ply) 
        add_string_member(teeth_comp_ply_data, ply.first, ply.second);
    scene_data.AddMember(
        "teeth_comp_ply",
        teeth_comp_ply_data,
        scene_data.GetAllocator());

    for (auto urn : teeth_comp_ply_urn)
        add_string_member(teeth_comp_ply_urn_data, urn.first, urn.second);
    scene_data.AddMember(
        "teeth_comp_ply_urn",
        teeth_comp_ply_urn_data,
        scene_data.GetAllocator());

    for (auto axis : teeth_axis) 
        add_2dvector_member(teeth_axis_data, axis.first, axis.second);
    scene_data.AddMember(
        "teeth_axis",
        teeth_axis_data,
        scene_data.GetAllocator());

    for (auto axis : teeth_axis_origin)
        add_2dvector_member(teeth_axis_origin_data, axis.first, axis.second);
    scene_data.AddMember(
        "teeth_axis_origin",
        teeth_axis_origin_data,
        scene_data.GetAllocator());

    for (auto axis : teeth_axis_arranged)
        add_2dvector_member(teeth_axis_arranged_data, axis.first, axis.second);
    scene_data.AddMember(
        "teeth_axis_arranged",
        teeth_axis_arranged_data,
        scene_data.GetAllocator());

    //cout << "save model..." << endl;
    upper_jaw_model.save_model(upper_jaw_model_document);
    //cout << "save model..." << endl;
    scene_data.AddMember(
        "upper_jaw_model",
        upper_jaw_model_document,
        scene_data.GetAllocator());
    lower_jaw_model.save_model(lower_jaw_model_document);
    scene_data.AddMember(
        "lower_jaw_model",
        lower_jaw_model_document,
        scene_data.GetAllocator());

    add_string_member(scene_data, "upper_gum_doc_str", upper_gum_doc_str);
    add_string_member(scene_data, "lower_gum_doc_str", lower_gum_doc_str);

    add_string_member(scene_data, "upper_gum_path", upper_gum_path);
    add_string_member(scene_data, "lower_gum_path", lower_gum_path);

    char last_selected_char[10];
    itoa(last_selected, last_selected_char, 10);
    add_string_member(scene_data, "last_selected", string(last_selected_char));
    char has_gum_char[10];
    itoa(has_gum, has_gum_char, 10);
    add_string_member(scene_data, "has_gum", has_gum_char);

    ofstream ofs;
    ofs.open(res_scene_name, ofstream::out | ofstream::binary);
    ofs << dump_json(scene_data);

    ofs.close();

    return true;
}
