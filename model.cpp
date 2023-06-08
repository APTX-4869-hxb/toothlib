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
    Document request_body(kObjectType);
    Document document;
    Document document_result;

    // Step 1. make input
    // Step 1.1 upload to file server
    string urn = upload_mesh(stl_file_path, jaw_type, stl_, label_, error_msg_);
    if (urn == "false")
        return false;
    // Step 1.2 config request
    config_request(urn, this->stl_jaw_type(), input_data, input_data_mesh_config, output_config, output_config_mesh, request_body);

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
    download_mesh(document_result, stl_, label_);

    return true;
}
