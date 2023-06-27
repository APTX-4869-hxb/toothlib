#include <igl/opengl/glfw/Viewer.h>
#include <igl/unproject_onto_mesh.h>
#include <igl/opengl/glfw/imgui/ImGuiHelpers.h>
#include <igl/opengl/glfw/imgui/ImGuiMenu.h>
#include <igl/opengl/glfw/imgui/ImGuiPlugin.h>
#include <igl/readOFF.h>
#include <iostream>

#include <fstream>
#include <sstream>
#include <cstdio>
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <utility>

#include "path.h"
#include "cpr/cpr.h"
#include "api.h"
#include "model.h"

using namespace rapidjson;
using namespace std;


float text_shift_scale_factor;
float render_scale;

int main(int argc, char *argv[]) {
    //Eigen::MatrixXd V;
    //Eigen::MatrixXi F;

    //// Load a mesh in OFF format
    //igl::readOFF("bunny.off", V, F);
    model fmodel;
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
                string fpath = viewer.open_dialog_load_mesh();
                if(fpath != "false")
                    fmodel = model(fpath);
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
            if (ImGui::Button("Segment & completion##Functions", ImVec2((w - p), 0)))
            {
                cout << "Segment and completion..." << endl;

                string result_stl, error_msg;
                map<string, string> result_t_comp_stl;
                vector<int> result_label;

                if (!fmodel.segment_jaw(result_stl, result_t_comp_stl, result_label, error_msg)) {
                    cout << error_msg << endl;
                    return 1;
                }

                fmodel.set_teeth_comp(result_t_comp_stl);

                string result_dir = PROJECT_PATH + string("/result");
                auto result_dir_path = fs::path(result_dir);

                if (!fs::is_directory(result_dir_path)) {
                    fs::create_directory(result_dir_path);
                }

                ofstream ofs;
                string res_mesh_name = result_dir + string("/seg_") + fmodel.stl_name() + string(".stl");
                ofs.open(res_mesh_name, ofstream::out | ofstream::binary);
                ofs << result_stl;
                ofs.close();


                for (auto stl : result_t_comp_stl) {
                    ofstream ofs;
                    string comp_name = result_dir + string("/seg_") + fmodel.stl_name() + string("_comp_") + stl.first + string(".stl");
                    ofs.open(comp_name, ofstream::out | ofstream::binary);
                    ofs << stl.second;
                    ofs.close();

                    viewer.load_mesh_from_file(comp_name.c_str(), false);
                    fmodel.set_colors(viewer.data().id, 0.5 * Eigen::RowVector3d::Random().array() + 0.5);
                    fmodel.mesh_add_tooth(viewer.data().id, stl.first);

                    viewer.data().add_label(viewer.data().V.row(viewer.data().V.size() / 6) + viewer.data().V_normals.row(viewer.data().V.size() / 6).normalized() * 0.5, stl.first);
                }

                viewer.erase_mesh(0);

                viewer.callback_pre_draw =
                    [&](igl::opengl::glfw::Viewer&)
                {
                    //int cur_idx = viewer.selected_data_index;
                    //cout << "cur index:" << viewer.selected_data_index << endl;
                    if (fmodel.last_selected != viewer.selected_data_index)
                    {
                        
                        int cur_id = viewer.data_list[viewer.selected_data_index].id;
                        //cout << "cur_id:" << cur_id << endl;
                        if (!fmodel.mesh_is_tooth(cur_id)) {
                            //cout << "round" << endl;
                            if (cur_id > fmodel.mesh_max_tooth_id())
                                viewer.selected_data_index = viewer.mesh_index(fmodel.mesh_min_tooth_id());
                            else
                                viewer.selected_data_index = viewer.mesh_index(fmodel.mesh_max_tooth_id());
                        }

                        for (auto& data : viewer.data_list) {
                                data.set_colors(fmodel.get_color(data.id));
                        }
                        viewer.data_list[viewer.selected_data_index].set_colors(fmodel.get_color(viewer.data_list[viewer.selected_data_index].id) + Eigen::RowVector3d(0.1, 0.1, 0.1));
                        fmodel.last_selected = viewer.selected_data_index;
                    }
                    return false;
                };

                viewer.callback_mouse_down =
                    [&](igl::opengl::glfw::Viewer& viewer, int, int)->bool
                {

                    int fid;
                    Eigen::Vector3f bc;
                    // Cast a ray in the view direction starting from the mouse position
                    // TODO: depth detection
                    double x = viewer.current_mouse_x;
                    double y = viewer.core().viewport(3) - viewer.current_mouse_y;
                    for (auto& data : viewer.data_list) {
                        if (igl::unproject_onto_mesh(
                            Eigen::Vector2f(x, y),
                            viewer.core().view,
                            viewer.core().proj,
                            viewer.core().viewport,
                            data.V,
                            data.F,
                            fid,
                            bc) 
                            && fmodel.mesh_is_tooth(data.id)
                            ) {
                            std::cout << "You clicked on tooth #" << fmodel.get_tooth_label(viewer.data().id) << std::endl;
                            viewer.selected_data_index = viewer.mesh_index(data.id);
                            return true;
                        }
                    }
                    return false;
                };

                // Draw additional windows
                menu.callback_draw_custom_window = [&]()
                {
                    // Define next window position + size
                    ImGui::SetNextWindowPos(ImVec2(180.f * menu.menu_scaling(), 10), ImGuiCond_FirstUseEver);
                    ImGui::SetNextWindowSize(ImVec2(200, 160), ImGuiCond_FirstUseEver);
                    ImGui::Begin(
                        "Tooth", nullptr,
                        ImGuiWindowFlags_NoSavedSettings
                    );

                    ImGui::Text((string("Tooth Label: ") + fmodel.get_tooth_label(viewer.data().id)).c_str());

                    ImGui::End();
                };

                string res_label_name = result_dir + string("/seg_") + fmodel.stl_name() + string(".txt");
                ofs.open(res_label_name, ofstream::out);
                for (const auto& e : result_label) ofs << e << endl;
                ofs.close();

                cout << "Segment and completion complete." << endl;
            }

            if (ImGui::Button("Generate gum##Functions", ImVec2((w - p), 0)))
            {
                cout << "Gum generating..." << endl;
                
                string result_ply, error_msg;

                if (!fmodel.generate_gum(result_ply, error_msg)) {
                    cout << error_msg << endl;
                    return 1;
                }
                string result_dir = PROJECT_PATH + string("/result");
                auto result_dir_path = fs::path(result_dir);

                if (!fs::is_directory(result_dir_path)) {
                    fs::create_directory(result_dir_path);
                }

                ofstream ofs;
                string res_mesh_name = result_dir + string("/") + fmodel.stl_name() + string("_gum.ply");
                ofs.open(res_mesh_name, ofstream::out | ofstream::binary);
                ofs << result_ply;
                ofs.close();

                viewer.load_mesh_from_file(res_mesh_name.c_str(), false);
                fmodel.set_colors(viewer.data().id, Eigen::RowVector3d(250.0 / 255.0, 203.0 / 255.0, 203.0 / 255.0));
                cout << "Gum generation complete." << endl;
            }
        }

    };

    viewer.launch();
}
