#include <igl/opengl/glfw/Viewer.h>
#include <igl/opengl/glfw/imgui/ImGuiHelpers.h>
#include <igl/opengl/glfw/imgui/ImGuiMenu.h>
#include <igl/opengl/glfw/imgui/ImGuiPlugin.h>
#include <igl/readOFF.h>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <cstdio>
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <utility>

#include <cpr/cpr.h>
#include "rapidjson/document.h"
#include "rapidjson/allocators.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"

using namespace rapidjson;
using namespace std;
namespace fs = std::filesystem;

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

bool segment_jaw(const string& stl_file_path, char jaw_type, string& stl_, vector<int>& label_,
    string& error_msg_) {
    /* This is the function to segment a jaw using ChohoTech Cloud Service.

        Input:
            stl_file_path: path to the stl file
            jaw_type: must be either "L" or "U", standing for Lower Jaw and Upper Jaw
        Output:
            stl_: string containing preprocessed mesh data in STL format. This can directly be saved as *.stl file
            label_: segmentation labels corresponding to the output stl_
            error_msg_: error message if job failed
        Returns:
            boolean: true - job successful and results saved to stl_ and label_. false - check error_msg_ for error message

       NOTE: if return value is false, stl_, label_is meaningless, DO NOT USE!!!
    */

    // Step 1. make input
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

    // Step 1.1 upload to file server
    cpr::Response r = cpr::Get(cpr::Url{ string(FILE_SERVER_URL) + "/scratch/APIClient/" + user_id + "/upload_url?postfix=stl" },
        cpr::Header{ {"X-ZH-TOKEN", zh_token} },
        cpr::VerifySsl(0)
    );

    if (r.status_code > 300) {
        error_msg_ = "get upload_url request failed with error code: " + to_string(r.status_code);
        return false;
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
        return false;
    }

    cout << "uploading mesh takes " << to_sec(now() - start) << " seconds" << endl;

    auto l_pos = upload_url.find(user_id);
    if (l_pos == upload_url.npos) {
        error_msg_ = "url format is wrong: " + upload_url;
        return false;
    }
    l_pos += user_id.size() + 1;
    auto r_pos = upload_url.find("?");
    size_t url_cut_len = r_pos - l_pos;
    if (r_pos <= l_pos) {
        error_msg_ = "url format is wrong: " + upload_url;
        return false;
    }
    else if (r_pos == upload_url.npos) url_cut_len = upload_url.size() - l_pos;
    string urn = "urn:zhfile:o:s:APIClient:" + user_id + ":" + upload_url.substr(l_pos, url_cut_len);

    cout << "Uploaded to urn: " << urn << endl;

    Document input_data(kObjectType);
    Document input_data_mesh_config(kObjectType);
    add_string_member(input_data_mesh_config, "type", "stl");
    add_string_member(input_data_mesh_config, "data", urn);
    input_data.AddMember(
        "mesh",
        input_data_mesh_config,
        input_data.GetAllocator());

    add_string_member(input_data, "jaw_type", (jaw_type == 'L') ? "Lower" : "Upper");

    Document output_config(kObjectType);
    Document output_config_mesh(kObjectType);
    add_string_member(output_config_mesh, "type", "stl");
    output_config.AddMember(
        "mesh",
        output_config_mesh,
        output_config.GetAllocator());

    Document request_body(kObjectType);

    auto& request_body_allocator = request_body.GetAllocator();
    add_string_member(request_body, "spec_group", "mesh-processing");
    add_string_member(request_body, "spec_name", "oral-seg");
    add_string_member(request_body, "spec_version", "1.0-snapshot");
    add_string_member(request_body, "user_group", "APIClient");
    add_string_member(request_body, "user_id", USER_ID);
    request_body.AddMember(
        "input_data",
        input_data,
        request_body_allocator);

    request_body.AddMember(
        "output_config",
        output_config,
        request_body_allocator);

    // Step 2. submit job
    r = cpr::Post(cpr::Url{ string(SERVER_URL) + "/run" },
        cpr::Body{ dump_json(request_body) },
        cpr::Header{ {"Content-Type", "application/json"}, {"X-ZH-TOKEN", string(USER_TOKEN)} },
        cpr::VerifySsl(0) // do not add this line in production for safty reason
    );

    if (r.status_code > 300) {
        error_msg_ = "job creation request failed with error code: " + to_string(r.status_code);
        return false;
    }

    Document document;
    document.Parse(r.text.c_str());
    string job_id = document["run_id"].GetString();

    cout << "run id is: " << job_id << endl;

    start = now();

    // Step 3. check job
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

    // Step 4. get job result
    r = cpr::Get(cpr::Url{ string(SERVER_URL) + "/data/" + job_id },
        cpr::Header{ {"X-ZH-TOKEN", USER_TOKEN} }, cpr::VerifySsl(0));


    if (r.status_code > 300) {
        error_msg_ = "job result request failed with error code: " + to_string(r.status_code);
        return false;
    }
    //cout << "get job result" << endl;
    // Step 5. parse result

    Document document_result;
    document_result.Parse(r.text.c_str());

    //cout << "parse result" << endl;

    // Step 5.1 download mesh
    string download_urn = document_result["mesh"]["data"].GetString();

    r = cpr::Get(cpr::Url{ string(FILE_SERVER_URL) + "/file/download?urn=" + download_urn },
        cpr::Header{ {"X-ZH-TOKEN", USER_TOKEN} }, cpr::VerifySsl(0));

    stl_ = string(r.text);

    label_.clear();
    for (auto& v : document_result["seg_labels"].GetArray()) label_.push_back(v.IsInt() ? v.GetInt() : (int)(v.GetDouble() + 0.1));

    return true;
}

string fname;

int main(int argc, char *argv[]) {
    Eigen::MatrixXd V;
    Eigen::MatrixXi F;

    //// Load a mesh in OFF format
    //igl::readOFF("bunny.off", V, F);

    // Init the viewer
    igl::opengl::glfw::Viewer viewer;

    // Attach a menu plugin
    igl::opengl::glfw::imgui::ImGuiPlugin plugin;
    viewer.plugins.push_back(&plugin);
    igl::opengl::glfw::imgui::ImGuiMenu menu;
    plugin.widgets.push_back(&menu);

    menu.callback_draw_viewer_menu = [&]()
    {
        // Workspace
        if (ImGui::CollapsingHeader("Scene", ImGuiTreeNodeFlags_DefaultOpen))
        {
            float w = ImGui::GetContentRegionAvail().x;
            float p = ImGui::GetStyle().FramePadding.x;
            if (ImGui::Button("Load##Scene", ImVec2((w - p) / 2.f, 0)))
            {
                viewer.load_scene();
            }
            ImGui::SameLine(0, p);
            if (ImGui::Button("Save##Scene", ImVec2((w - p) / 2.f, 0)))
            {
                viewer.save_scene();
            }
        }
        // Model
        if (ImGui::CollapsingHeader("Model", ImGuiTreeNodeFlags_DefaultOpen))
        {
            float w = ImGui::GetContentRegionAvail().x;
            float p = ImGui::GetStyle().FramePadding.x;
            if (ImGui::Button("Load##Model", ImVec2((w - p) / 2.f, 0)))
            {
                fname = viewer.open_dialog_load_mesh();
            }
            ImGui::SameLine(0, p);
            if (ImGui::Button("Save##Model", ImVec2((w - p) / 2.f, 0)))
            {
                viewer.open_dialog_save_mesh();
            }
        }

        // Viewing options
        if (ImGui::CollapsingHeader("Viewing Options", ImGuiTreeNodeFlags_DefaultOpen))
        {
            // Zoom
            ImGui::PushItemWidth(80 * menu.menu_scaling());
            ImGui::DragFloat("Zoom", &(viewer.core().camera_zoom), 0.05f, 0.1f, 20.0f);

            // Select rotation type
            int rotation_type = static_cast<int>(viewer.core().rotation_type);
            static Eigen::Quaternionf trackball_angle = Eigen::Quaternionf::Identity();
            static bool orthographic = true;
            if (ImGui::Combo("Camera Type", &rotation_type, "Trackball\0Two Axes\0002D Mode\0\0"))
            {
                using RT = igl::opengl::ViewerCore::RotationType;
                auto new_type = static_cast<RT>(rotation_type);
                if (new_type != viewer.core().rotation_type)
                {
                    if (new_type == RT::ROTATION_TYPE_NO_ROTATION)
                    {
                        trackball_angle = viewer.core().trackball_angle;
                        orthographic = viewer.core().orthographic;
                        viewer.core().trackball_angle = Eigen::Quaternionf::Identity();
                        viewer.core().orthographic = true;
                    }
                    else if (viewer.core().rotation_type == RT::ROTATION_TYPE_NO_ROTATION)
                    {
                        viewer.core().trackball_angle = trackball_angle;
                        viewer.core().orthographic = orthographic;
                    }
                    viewer.core().set_rotation_type(new_type);
                }
            }

            // Orthographic view
            ImGui::Checkbox("Orthographic view", &(viewer.core().orthographic));
            ImGui::PopItemWidth();
        }

        // Helper for setting viewport specific mesh options
        auto make_checkbox = [&](const char* label, unsigned int& option)
        {
            return ImGui::Checkbox(label,
                [&]() { return viewer.core().is_set(option); },
                [&](bool value) { return viewer.core().set(option, value); }
            );
        };

        // Draw options
        if (ImGui::CollapsingHeader("Draw Options", ImGuiTreeNodeFlags_DefaultOpen))
        {

            make_checkbox("Show texture", viewer.data().show_texture);
            ImGui::ColorEdit4("Background", viewer.core().background_color.data(),
                ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_PickerHueWheel);
            ImGui::ColorEdit4("Line color", viewer.data().line_color.data(),
                ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_PickerHueWheel);
            ImGui::PushItemWidth(ImGui::GetWindowWidth() * 0.3f);
            ImGui::DragFloat("Shininess", &(viewer.data().shininess), 0.05f, 0.0f, 100.0f);
            ImGui::PopItemWidth();
        }

        // Overlays
        if (ImGui::CollapsingHeader("Overlays", ImGuiTreeNodeFlags_DefaultOpen))
        {
            make_checkbox("Wireframe", viewer.data().show_lines);
            make_checkbox("Fill", viewer.data().show_faces);
            make_checkbox("Show vertex labels", viewer.data().show_vertex_labels);
            make_checkbox("Show faces labels", viewer.data().show_face_labels);
            make_checkbox("Show extra labels", viewer.data().show_custom_labels);
        }

        if (ImGui::CollapsingHeader("Functions", ImGuiTreeNodeFlags_DefaultOpen))
        {
            float w = ImGui::GetContentRegionAvail().x;
            float p = ImGui::GetStyle().FramePadding.x;
            if (ImGui::Button("Segment##Functions", ImVec2((w - p), 0)))
            {
                cout << "Segment..." << endl;

                char jaw_type = fs::path(fname).filename().generic_string()[fs::path(fname).filename().generic_string().find_first_of('.') - 1];

                //char jaw_type = 'L';
                if (jaw_type == 'L' || jaw_type == 'l') {
                    jaw_type = 'L';
                }
                else if (jaw_type == 'U' || jaw_type == 'u') {
                    jaw_type = 'U';
                }
                else {
                    cout << "STL file name must be either u.stl for upper jaw or l.stl for lower jaw" << endl;
                    return 1;
                }

                string result_stl, error_msg;
                vector<int> result_label;
                if (!segment_jaw(fname, jaw_type, result_stl, result_label, error_msg)) {
                    cout << error_msg << endl;
                    return 1;
                }

                string result_dir = string(".\\result");
                auto result_dir_path = fs::path(result_dir);

                if (!fs::is_directory(result_dir_path)) {
                    fs::create_directory(result_dir_path);
                }

                ofstream ofs;
                string resname = result_dir + string("/result_mesh.stl");
                ofs.open(result_dir_path / "result_mesh.stl", ofstream::out | ofstream::binary);
                ofs << result_stl;
                ofs.close();

                ofs.open(result_dir_path / "result_label.txt", ofstream::out);
                for (const auto& e : result_label) ofs << e << endl;
                ofs.close();

                viewer.load_mesh_from_file(fname.c_str());
            }
        }

    };

    //// Plot the mesh
    //viewer.data().set_mesh(V, F);

    viewer.launch();
}
