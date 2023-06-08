#include <api.h>

string dump_json(Document& doc)
{
    StringBuffer buffer;

    buffer.Clear();

    Writer<StringBuffer> writer(buffer);
    doc.Accept(writer);

    return string(buffer.GetString());
}

void add_string_member(Document& doc, const string& key, const string& val) {
    auto& doc_allocator = doc.GetAllocator();
    Value v;
    v.SetString(val.c_str(), doc_allocator);
    Value k;
    k.SetString(key.c_str(), doc_allocator);
    doc.AddMember(k, v, doc_allocator);
}

string upload_mesh(string& stl_file_path, char& jaw_type, string& stl_, vector<int>& label_, string& error_msg_) {
    ifstream infile(stl_file_path, ifstream::binary);
    if (!infile.is_open()) {
        cerr << "Could not open the file - '"
            << stl_file_path << "'" << endl;
        exit(EXIT_FAILURE);
    }

    string buffer((std::istreambuf_iterator<char>(infile)), std::istreambuf_iterator<char>());
    infile.close();
    string user_id = string(USER_ID);
    string zh_token = string(USER_TOKEN);

    auto start = now();

    cpr::Response r = cpr::Get(cpr::Url{ string(FILE_SERVER_URL) + "/scratch/APIClient/" + user_id + "/upload_url?postfix=stl" },
        cpr::Header{ {"X-ZH-TOKEN", zh_token} },
        cpr::VerifySsl(0)
    );

    if (r.status_code > 300) {
        error_msg_ = "get upload_url request failed with error code: " + to_string(r.status_code);
        return "false";
    }

    string upload_url = string(r.text.c_str());
    upload_url = upload_url.substr(1, upload_url.size() - 2);

    r = cpr::Put(cpr::Url{ upload_url },
        cpr::Body{ buffer },
        cpr::Header{ {"content-type", ""} },
        cpr::VerifySsl(0)
    );

    if (r.status_code > 300) {
        error_msg_ = "file upload request failed with error code: " + to_string(r.status_code);
        return "false";
    }

    cout << "uploading mesh takes " << to_sec(now() - start) << " seconds" << endl;

    auto l_pos = upload_url.find(user_id);
    if (l_pos == upload_url.npos) {
        error_msg_ = "url format is wrong: " + upload_url;
        return "false";
    }
    l_pos += user_id.size() + 1;
    auto r_pos = upload_url.find("?");
    size_t url_cut_len = r_pos - l_pos;
    if (r_pos <= l_pos) {
        error_msg_ = "url format is wrong: " + upload_url;
        return "false";
    }
    else if (r_pos == upload_url.npos) url_cut_len = upload_url.size() - l_pos;
    string urn = "urn:zhfile:o:s:APIClient:" + user_id + ":" + upload_url.substr(l_pos, url_cut_len);

    cout << "Uploaded to urn: " << urn << endl;

    return urn;
}

bool config_request(string urn, char jaw_type, Document& input_data, Document& input_data_mesh_config, Document& output_config, Document& output_config_mesh, Document& request_body) {
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

    add_string_member(request_body, "spec_group", "mesh-processing");
    add_string_member(request_body, "spec_name", "oral-seg");
    add_string_member(request_body, "spec_version", "1.0-snapshot");
    add_string_member(request_body, "user_group", "APIClient");
    add_string_member(request_body, "user_id", USER_ID);
    request_body.AddMember(
        "input_data",
        input_data,
        request_body.GetAllocator());

    request_body.AddMember(
        "output_config",
        output_config,
        request_body.GetAllocator());

}

string submit_job(Document& document, Document& request_body, string& error_msg_) {
    cpr::Response r = cpr::Post(cpr::Url{ string(SERVER_URL) + "/run" },
        cpr::Body{ dump_json(request_body) },
        cpr::Header{ {"Content-Type", "application/json"}, {"X-ZH-TOKEN", string(USER_TOKEN)} },
        cpr::VerifySsl(0) // do not add this line in production for safty reason
    );

    if (r.status_code > 300) {
        error_msg_ = "job creation request failed with error code: " + to_string(r.status_code);
        return "false";
    }

    document.Parse(r.text.c_str());
    string job_id = document["run_id"].GetString();

    cout << "run id is: " << job_id << endl;
    return job_id;
}

bool check_job(string job_id, string& error_msg_) {
    auto start = now();

    bool status = false;

    while (!status) {
        this_thread::sleep_for(chrono::milliseconds(3000)); // sleep 3s
        cpr::Response r_stat = cpr::Get(cpr::Url{ string(SERVER_URL) + "/run/" + job_id },
            cpr::Header{ {"X-ZH-TOKEN", USER_TOKEN} }, cpr::VerifySsl(0));
        if (r_stat.status_code > 300) {
            error_msg_ = "job status request failed with error code: " + to_string(r_stat.status_code);
            return false;
        }

        Document document_stat;
        document_stat.Parse(r_stat.text.c_str());

        status = document_stat["failed"].GetBool();
        if (status) {
            error_msg_ = string("job failed with error: ") + document_stat["reason_public"].GetString();
            return false;
        }

        status = document_stat["completed"].GetBool();
    }

    cout << "job run takes " << to_sec(now() - start) << " seconds" << endl;
    return true;
}

bool get_job_result(string job_id, Document& document_result, string& error_msg_) {
    cpr::Response r = cpr::Get(cpr::Url{ string(SERVER_URL) + "/data/" + job_id },
        cpr::Header{ {"X-ZH-TOKEN", USER_TOKEN} }, cpr::VerifySsl(0));

    if (r.status_code > 300) {
        error_msg_ = "job result request failed with error code: " + to_string(r.status_code);
        return false;
    }
    document_result.Parse(r.text.c_str());
    return true;
}

void download_mesh(Document& document_result, string& stl_, vector<int>& label_) {
    string download_urn = document_result["mesh"]["data"].GetString();

    cpr::Response r = cpr::Get(cpr::Url{ string(FILE_SERVER_URL) + "/file/download?urn=" + download_urn },
        cpr::Header{ {"X-ZH-TOKEN", USER_TOKEN} }, cpr::VerifySsl(0));

    stl_ = string(r.text);

    label_.clear();
    for (auto& v : document_result["seg_labels"].GetArray()) label_.push_back(v.IsInt() ? v.GetInt() : (int)(v.GetDouble() + 0.1));
}