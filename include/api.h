#include <chrono>
#include <iostream>
#include <string>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <map>

#include "rapidjson/document.h"
#include "rapidjson/allocators.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include "cpr/cpr.h"

using namespace rapidjson;
using namespace std;

string dump_json(Document& doc);
void add_string_member(Document& doc, const string& key, const string& val);

template <typename T>
void add_vector_member(Document& doc, const string& key, const vector<T>& vec) {
    auto& doc_allocator = doc.GetAllocator();
    rapidjson::Value vectorValue(rapidjson::kArrayType);
    for (const auto& value : vec)
        vectorValue.PushBack(value, doc_allocator);
    Value k;
    k.SetString(key.c_str(), doc_allocator);
    doc.AddMember(k, vectorValue, doc_allocator);
}

template <typename T>
void add_2dvector_member(Document& doc, const string& key, const vector<vector<T>>& vec) {
    auto& doc_allocator = doc.GetAllocator();
    rapidjson::Value outerArray(rapidjson::kArrayType);

    for (const auto& innerVector : vec) {
        rapidjson::Value innerArray(rapidjson::kArrayType);
        for (const auto& value : innerVector) {
            innerArray.PushBack(value, doc_allocator);
        }
        outerArray.PushBack(innerArray, doc_allocator);
    }
    Value k;
    k.SetString(key.c_str(), doc_allocator);
    doc.AddMember(k, outerArray, doc_allocator);

}

string upload_mesh(string& stl_file_path, char& jaw_type, string& stl_, vector<int>& label_, string& error_msg_);
bool config_request(map<string, string> spec, Document& input_data, Document& output_config, Document& request_body);
string submit_job(Document& document, Document& request_body, string& error_msg_);
bool check_job(string job_id, string& error_msg_);
bool get_job_result(string job_id, Document& document_result, string& error_msg_);
string download_mesh(Document& document_result, string& stl_, const char* object = "mesh");
//void download_t_comp_mesh(Document& document_result, map<string, string>& t_comp_stl_, const char* object = "teeth_comp");
void download_label(Document& document_result, vector<int>& label_);
void download_mesh_from_urn(string download_urn, string& mesh);

inline
chrono::time_point <chrono::high_resolution_clock>
now()
{
    return chrono::high_resolution_clock::now();
}

template <typename T>
inline double to_sec(T t)
{
    unsigned long milli = chrono::duration_cast <chrono::milliseconds> (t).count();
    return (double)milli / 1000.f;
}