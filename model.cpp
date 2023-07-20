#include <model.h>
#include <api.h>
#include <Windows.h>

model::model() {}

model::~model() {}

model::model(string fpath) {
    stl_file_path = fpath;
    fname = fs::path(stl_file_path).filename().generic_string();
    jaw_type = fname[fname.find_first_of('.') - 1];

    stringstream ss(fname);
    getline(ss, fname, '.');

    //cout << "file name is: " << fname << endl;

    //char jaw_type = 'L';
    if (jaw_type == 'L' || jaw_type == 'l') {
        jaw_type = 'L';
    }
    else if (jaw_type == 'U' || jaw_type == 'u') {
        jaw_type = 'U';
    }
    else {
        cout << "STL file name must end with u for upper jaw or l for lower jaw" << endl;
    }

    last_selected = -1;
}

bool model::segment_jaw(string& stl_, vector<int>& label_, string& error_msg_) {
    /* This is the function to segment a jaw using ChohoTech Cloud Service.
        Output:
            stl_: string containing preprocessed mesh data in STL format. This can directly be saved as *.stl file
            label_: segmentation labels corresponding to the output stl_
            error_msg_: error message if job failed
        Returns:
            boolean: true - job successful and results saved to stl_ and label_. false - check error_msg_ for error message

       NOTE: if return value is false, stl_, label_is meaningless, DO NOT USE!!!
    */
    cpr::Response r;
    Document input_data(kObjectType);
    Document input_data_mesh_config(kObjectType);
    Document output_config(kObjectType);
    Document output_config_mesh(kObjectType);
    //Document output_config_comp_mesh(kObjectType);
    //Document output_config_axis(kObjectType);
    Document request_body(kObjectType);
    Document document;
    Document document_result;

    // Step 1. make input
    // Step 1.1 upload to file server
    string urn = upload_mesh(stl_file_path, jaw_type, stl_, label_, error_msg_);
    if (urn == "false")
        return false;

    add_string_member(input_data_mesh_config, "type", "stl");
    add_string_member(input_data_mesh_config, "data", urn);
    input_data.AddMember(
        "mesh",
        input_data_mesh_config,
        input_data.GetAllocator());

    add_string_member(input_data, "jaw_type", (jaw_type == 'L') ? "Lower" : "Upper");

    //cout << dump_json(input_data) << endl;

    add_string_member(output_config_mesh, "type", "stl");
    output_config.AddMember(
        "mesh",
        output_config_mesh,
        output_config.GetAllocator());

    map<string, string> spec;
    spec.insert(pair<string, string>("spec_group", "mesh-processing"));
    spec.insert(pair<string, string>("spec_name", "oral-seg-and-axis"));
    spec.insert(pair<string, string>("spec_version", "1.0-snapshot"));

    // Step 1.2 config request
    config_request(spec, input_data, output_config, request_body);

    // Step 2. submit job
    string job_id = submit_job(document, request_body, error_msg_);
    if (job_id == "false")
        return false;

    // Step 3. check job
    if (!check_job(job_id, error_msg_))
        return false;

    // Step 4. get job result
    if (!get_job_result(job_id, document_result, error_msg_))
        return false;

    // Step 5 download mesh
    urn = download_mesh(document_result, stl_);
    download_label(document_result, label_);

    stl_file_urn = urn;
    
    for (auto& v : document_result["axis"].GetObject()) {
        vector<vector<double>> axis;
        for (int i = 0; i < v.value.Size(); i++) {
            vector<double> axis_line;
            for (int j = 0; j < v.value[i].Size(); j++) 
                axis_line.push_back(v.value[i][j].GetDouble());
            axis.push_back(axis_line);
        }
        teeth_axis.insert(pair<string, vector<vector<double>>>(v.name.GetString(), axis));
    }

    return true;
}

bool model::generate_gum(Document& document_result, string& ply_, string& error_msg_) {
    /* This is the function to generate gum using ChohoTech Cloud Service.
        Output:
            stl_: string containing preprocessed mesh data in STL format. This can directly be saved as *.stl file
            label_: segmentation labels corresponding to the output stl_
            error_msg_: error message if job failed
        Returns:
            boolean: true - job successful and results saved to stl_ and label_. false - check error_msg_ for error message

       NOTE: if return value is false, stl_, label_is meaningless, DO NOT USE!!!
    */
    cpr::Response r;
    Document input_data(kObjectType);
    Document input_data_dict(kObjectType);
    Document output_config(kObjectType);
    Document output_config_mesh(kObjectType);
    Document request_body(kObjectType);
    Document document;
    //Document document_result;
    vector<Document*> input_data_urn;
    // Step 1. make input

    int i = 0;
    for (auto& t : teeth_comp_stl_urn) {
        input_data_urn.push_back(new Document((kObjectType)));
        Document* cur = input_data_urn[i];
        add_string_member(*cur, "type", "stl");
        add_string_member(*cur, "data", t.second);
        Value name;
        name.SetString(t.first.c_str(), (*cur).GetAllocator());
        input_data_dict.AddMember(
            name,
            *input_data_urn[i],
            input_data_dict.GetAllocator());
        i++;
    }
    //cout << dump_json(input_data_dict) << endl;
    input_data.AddMember(
        "teeth_dict",
        input_data_dict,
        input_data.GetAllocator());
    //cout << dump_json(input_data) << endl;

    add_string_member(output_config_mesh, "type", "ply");
    output_config.AddMember(
        "gum",
        output_config_mesh,
        output_config.GetAllocator());

    map<string, string> spec;
    spec.insert(pair<string, string>("spec_group", "mesh-processing"));
    spec.insert(pair<string, string>("spec_name", "gum-generation"));
    spec.insert(pair<string, string>("spec_version", "1.0-snapshot"));

    // Step 1.2 config request
    config_request(spec, input_data, output_config, request_body);

    // Step 2. submit job
    string job_id = submit_job(document, request_body, error_msg_);
    if (job_id == "false")
        return false;

    // Step 3. check job
    if (!check_job(job_id, error_msg_))
        return false;

    // Step 4. get job result
    if (!get_job_result(job_id, document_result, error_msg_))
        return false;

    // Step 5 download mesh
    download_mesh(document_result, ply_, "gum");

    return true;
}

typedef int (WINAPI* CREATE_FUNC)(const double*, unsigned int,
    const int*, unsigned int, const char*, unsigned int, void**);
typedef int (WINAPI* DEFORM_FUNC)(void*,
                          const ToothTransformation *,
                          unsigned int,
                          double *, unsigned int *,
                          int *, unsigned int *,
                          char *, unsigned int *);
typedef void (WINAPI* DESTROY_FUNC)(void*);

bool model::gum_deform(Document &document_result) {

    HMODULE hdll;
    hdll = LoadLibrary(("libchohotech_gum_deform_x64.dll"));
    
    CREATE_FUNC create_gum_deformer = (CREATE_FUNC)GetProcAddress(hdll, "create_gum_deformer");
    DEFORM_FUNC deform = (DEFORM_FUNC)GetProcAddress(hdll, "deform");
    DESTROY_FUNC destroy_gum_deformer = (DESTROY_FUNC)GetProcAddress(hdll, "destroy_gum_deformer");


    Document ori_gum_info;
    ori_gum_info.Parse(document_result["result"]["ori_gum_info"].GetString());

    auto gum_vertices = ori_gum_info["gum_vertices"].GetArray();
    int num_gum_vertices = ori_gum_info["num_gum_vertices"].GetInt();

    gum_vertices_ptr = new double[num_gum_vertices * 3];
    for (int i = 0; i < num_gum_vertices; i++) {
        gum_vertices_ptr[i * 3] = gum_vertices[i][0].GetDouble();
        gum_vertices_ptr[i * 3 + 1] = gum_vertices[i][1].GetDouble();
        gum_vertices_ptr[i * 3 + 2] = gum_vertices[i][2].GetDouble();
    }

    auto gum_faces = ori_gum_info["gum_faces"].GetArray();

    gum_faces_ptr = new int[gum_faces.Size() * 3];
    for (int i = 0; i < gum_faces.Size(); i++) {
        gum_faces_ptr[i * 3] = gum_faces[i][0].GetInt();
        gum_faces_ptr[i * 3 + 1] = gum_faces[i][1].GetInt();
        gum_faces_ptr[i * 3 + 2] = gum_faces[i][2].GetInt();
    }

    Document a;
    a.CopyFrom(document_result["result"], document_result.GetAllocator());
    string json_str = dump_json(a);
    //string json_str = dump_json(document_result);

    // initialize a gum deformer
    int ret = create_gum_deformer(gum_vertices_ptr, num_gum_vertices * 3, gum_faces_ptr, gum_faces.Size() * 3,
        json_str.c_str(), json_str.size(), &gum_deformer_ptr);
    if (ret < 0) {
        cout << "failed" << endl;
    }

    //// random rotation matrix
    //Eigen::Matrix3d R = Eigen::Matrix3d::Identity();
    //Eigen::AngleAxisd aa(Eigen::AngleAxisd(10, Eigen::Vector3d(0, 0, 1)));
    //// R = aa.toRotationMatrix();
    //cout << "rotation: \n" << R << endl;

    //// assign tid to tt.tid
    //tt[0].tid = 11;
    //// assign rotation to tt.rotation
    //copy(R.data(), R.data() + 9, tt[0].rotation);
    //// assign translation to tt.translation
    //tt[0].translation[0] = 200.;
    //tt[0].translation[1] = 0.;
    //tt[0].translation[2] = 0.;

    //// deform
    //unsigned int n_teeth = 1;       // number of transformed teeth, could be more than 1
    //double* p_deformed_vertices = new double[300000];        // the returned deformed vertices
    //int* p_deformed_faces = new int[300000];
    //unsigned int deformed_vertices_len, deformed_faces_len;
    //deform(gum_deformer_ptr,
    //    tt,
    //    n_teeth,
    //    p_deformed_vertices, &deformed_vertices_len,
    //    p_deformed_faces, &deformed_faces_len, NULL, NULL);

    //cout << "deform" << endl;
    //// get result
    //std::vector<std::vector<double>> deformed_vertices(deformed_vertices_len / 3);
    //for (int i = 0; i < deformed_vertices.size(); i++) {
    //    deformed_vertices[i].push_back(p_deformed_vertices[i * 3]);
    //    deformed_vertices[i].push_back(p_deformed_vertices[i * 3 + 1]);
    //    deformed_vertices[i].push_back(p_deformed_vertices[i * 3 + 2]);
    //}
    //std::vector<std::vector<int>> deformed_faces(deformed_faces_len / 3);
    //for (int i = 0; i < deformed_faces.size(); i++) {
    //    deformed_faces[i].push_back(p_deformed_faces[i * 3]);
    //    deformed_faces[i].push_back(p_deformed_faces[i * 3 + 1]);
    //    deformed_faces[i].push_back(p_deformed_faces[i * 3 + 2]);
    //}
    //cout << "get result" << endl;
    //// destroy
    //destroy_gum_deformer(gum_deformer_ptr);
    //cout << "destroy" << endl;
    return true;
}