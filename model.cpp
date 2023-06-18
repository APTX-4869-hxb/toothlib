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

    cout << "file name is: " << fname << endl;

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
}

bool model::segment_jaw(string& stl_, map<string, string>& t_comp_stls_, vector<int>& label_, string& error_msg_) {
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

    add_string_member(output_config_comp_mesh, "type", "stl");
    output_config.AddMember(
        "teeth_comp",
        output_config_comp_mesh,
        output_config.GetAllocator());

    map<string, string> spec;
    spec.insert(pair<string, string>("spec_group", "mesh-processing"));
    spec.insert(pair<string, string>("spec_name", "oral-denoise-prod"));
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
    download_mesh(document_result, stl_);
    download_label(document_result, label_);

    for (auto& v : document_result["teeth_comp"].GetObjectA())
        teeth_comp_stl_urn.insert(pair<string, string>(v.name.GetString(), v.value["data"].GetString()));
    

    download_t_comp_mesh(document_result, t_comp_stls_);


    return true;
}


bool model::generate_gum(string& ply_, string& error_msg_) {
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
    Document document_result;
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
    spec.insert(pair<string, string>("spec_name", "wf-gum-only-generation"));
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