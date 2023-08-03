#include <utils.h>
#include <model.h>
#include <api.h>


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

    //last_selected = -1;
}
model::model(Value val) {
    stl_file_path = string(val["stl_file_path"].GetString());
    stl_file_urn = string(val["stl_file_urn"].GetString());
    fname = string(val["fname"].GetString());
    jaw_type = fname[fname.length() - 1];

    //
    //for (auto& v : val["teeth_comp_ply_urn"].GetObjectA()) {
    //    string urn = v.value.GetString();
    //    assignToMap(teeth_comp_ply_urn, string(v.name.GetString()), urn);
    //}
    //cout << val["gum_urn"].GetString() << endl;
    //if(val["gum_urn"].GetStringLength())
    gum_urn = string(val["gum_urn"].GetString());
    //if (val["gum_ply"].GetStringLength())
    gum_ply = string(val["gum_ply"].GetString());
}
bool model::segment_jaw(string& stl_, vector<int>& label_, map<string, vector<vector<float>>>& teeth_axis, map<string, string>& teeth_comp, string& error_msg_) {
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
    Document output_config_comp_mesh(kObjectType);
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

    add_string_member(output_config_comp_mesh, "type", "ply");
    output_config.AddMember(
        "teeth_comp",
        output_config_comp_mesh,
        output_config.GetAllocator());

    map<string, string> spec;
    spec.insert(pair<string, string>("spec_group", "mesh-processing"));
    spec.insert(pair<string, string>("spec_name", "oral-comp-and-axis"));
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
    
    for (auto& v : document_result["teeth_comp"].GetObjectA()) {
        string download_urn = v.value["data"].GetString();
        assignToMap(teeth_comp_ply_urn, string(v.name.GetString()), download_urn);
        //teeth_comp_ply_urn.insert(pair<string, string>(v.name.GetString(), download_urn));
    }

    download_t_comp_mesh(document_result, teeth_comp);

    for (auto& v : document_result["axis"].GetObjectA()) {
        vector<vector<float>> axis;
        for (int i = 0; i < v.value.Size(); i++) {
            vector<float> axis_line;
            for (int j = 0; j < v.value[i].Size(); j++) 
                axis_line.push_back(v.value[i][j].GetDouble());
            axis.push_back(axis_line);
        }
        teeth_axis.insert(pair<string, vector<vector<float>>>(v.name.GetString(), axis));
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
    for (auto& t : teeth_comp_ply_urn) {
        //cout << t.first << endl;
        //cout << t.second << endl;
        input_data_urn.push_back(new Document((kObjectType)));
        Document* cur = input_data_urn[i];
        add_string_member(*cur, "type", "ply");
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
    gum_urn = download_mesh(document_result, ply_, "gum");

    gum_ply = ply_;

    return true;
}



bool model::create_gum_deformer(Document &document_result, HMODULE hdll) {
    
    CREATE_FUNC create_gum_deformer = (CREATE_FUNC)GetProcAddress(hdll, "create_gum_deformer");

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
    return true;
}

bool model::save_model(Document& doc) {

    //Document teeth_comp_ply_urn_data(kObjectType);

    add_string_member(doc, "stl_file_path", stl_file_path);
    add_string_member(doc, "stl_file_urn", stl_file_urn);
    add_string_member(doc, "fname", fname);

    //for (auto urn : teeth_comp_ply_urn)
    //    add_string_member(teeth_comp_ply_urn_data, urn.first, urn.second);
    //doc.AddMember(
    //    "teeth_comp_ply_urn",
    //    teeth_comp_ply_urn_data,
    //    doc.GetAllocator());

    add_string_member(doc, "gum_urn", gum_urn);
    add_string_member(doc, "gum_ply", gum_ply);

    return true;
}