#include <igl/opengl/glfw/Viewer.h>
#include <igl/unproject_onto_mesh.h>
#include <igl/opengl/glfw/imgui/ImGuiHelpers.h>
#include <igl/opengl/glfw/imgui/ImGuiMenu.h>
#include <igl/opengl/glfw/imgui/ImGuiPlugin.h>
#include <igl/readOFF.h>
#include <igl/PI.h>

#include <iostream>

#include <fstream>
#include <sstream>
#include <cstdio>
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <utility>

#include "cpr/cpr.h"
#include "utils.h"
#include "api.h"
#include "scene.h"

using namespace rapidjson;
using namespace std;


float text_shift_scale_factor;
float render_scale;

int main(int argc, char *argv[]) {
    //Eigen::MatrixXd V;
    //Eigen::MatrixXi F;

    //// Load a mesh in OFF format
    //igl::readOFF("bunny.off", V, F);
    scene fscene;
    model fmodel;
    // Init the viewer
    igl::opengl::glfw::Viewer viewer;

    // Attach a menu plugin
    igl::opengl::glfw::imgui::ImGuiPlugin plugin;
    viewer.plugins.push_back(&plugin);
    igl::opengl::glfw::imgui::ImGuiMenu menu;
    plugin.widgets.push_back(&menu);

    int pose_dirty = 0;

    menu.callback_draw_viewer_menu = [&]()
    {
        // Workspace
        if (ImGui::CollapsingHeader("Scene", ImGuiTreeNodeFlags_DefaultOpen))
        {
            float w = ImGui::GetContentRegionAvail().x;
            float p = ImGui::GetStyle().FramePadding.x;
            if (ImGui::Button("Load##Scene", ImVec2((w - p) / 2.f, 0)))
            {
                std::cout << "please select one jaw..." << endl;
                string fpath_1 = viewer.open_dialog_load_mesh();
                std::cout << "please select the other jaw in the same pair..." << endl;
                string fpath_2 = viewer.open_dialog_load_mesh();
                if (fpath_1 != "false" && fpath_2 != "false")
                    fscene = scene(fpath_1, fpath_2);
                //viewer.load_scene();
            }
            ImGui::SameLine(0, p);
            if (ImGui::Button("Save##Scene", ImVec2((w - p) / 2.f, 0)))
            {
                //viewer.save_scene();
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
            if (ImGui::Button("Segment##Functions", ImVec2((w - p), 0)))
            {
                std::cout << "===============Segment...===============" << endl;
                //fscene.segment_jaws();
                if (!fscene.segment_jaws())
                    return 1;

                std::cout << "===============Segment complete.===============" << endl;
            }

            if (ImGui::Button("Arrangement##Functions", ImVec2((w - p), 0)))
            {
                std::cout << "===============Arrangement...===============" << endl;

                if (!fscene.arrangement())
                    return 1;

                string result_dir = PROJECT_PATH + string("/result");

                for (auto ply : fscene.get_teeth_comp()) {
                    ofstream ofs;
                    string comp_name = result_dir + string("/seg_") + fscene.stl_name() + string("_comp_") + ply.first + string(".ply");
                    ofs.open(comp_name, ofstream::out | ofstream::binary);
                    ofs << ply.second;
                    ofs.close();

                    viewer.load_mesh_from_file(comp_name.c_str(), false);
                    fscene.set_colors(viewer.data().id, 0.5 * Eigen::RowVector3d::Random().array() + 0.5);
                    fscene.mesh_add_tooth(viewer.data().id, ply.first);

                    //viewer.data().add_label(viewer.data().V.row(viewer.data().V.size() / 6) + viewer.data().V_normals.row(viewer.data().V.size() / 6).normalized() * 0.5, ply.first);
                }

                fscene.calc_poses();

                viewer.erase_mesh(0);
                viewer.erase_mesh(0);
                viewer.callback_pre_draw =
                    [&](igl::opengl::glfw::Viewer&)
                {
                    //int cur_idx = viewer.selected_data_index;
                    //cout << "cur index:" << viewer.selected_data_index << endl;
                    if (fscene.last_selected != viewer.selected_data_index)
                    {

                        int cur_id = viewer.data_list[viewer.selected_data_index].id;
                        //cout << "cur_id:" << cur_id << endl;
                        if (!fscene.mesh_is_tooth(cur_id)) {
                            //cout << "round" << endl;
                            if (cur_id > fscene.mesh_max_tooth_id())
                                viewer.selected_data_index = viewer.mesh_index(fscene.mesh_min_tooth_id());
                            else
                                viewer.selected_data_index = viewer.mesh_index(fscene.mesh_max_tooth_id());
                        }

                        for (auto& data : viewer.data_list) {
                            data.set_colors(fscene.get_color(data.id));
                        }
                        viewer.data_list[viewer.selected_data_index].set_colors(fscene.get_color(viewer.data_list[viewer.selected_data_index].id) + Eigen::RowVector3d(0.1, 0.1, 0.1));
                        fscene.last_selected = viewer.selected_data_index;
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
                            && fscene.mesh_is_tooth(data.id)
                            ) {
                            std::cout << "You clicked on tooth #" << fscene.get_tooth_label(viewer.data().id) << std::endl;
                            viewer.selected_data_index = viewer.mesh_index(data.id);
                            return true;
                        }
                    }
                    return false;
                };

                //viewer.callback_post_draw = [&](igl::opengl::glfw::Viewer&) {
                //    return true;
                //};

                // Draw additional windows
                menu.callback_draw_custom_window = [&]()
                {
                    // Define next window position + size
                    ImGui::SetNextWindowPos(ImVec2(180.f * menu.menu_scaling(), 10), ImGuiCond_FirstUseEver);
                    ImGui::SetNextWindowSize(ImVec2(200, 240), ImGuiCond_FirstUseEver);
                    ImGui::Begin(
                        "Tooth", nullptr,
                        ImGuiWindowFlags_NoSavedSettings
                    );

                    if (fscene.mesh_is_tooth(viewer.data().id)) {
                        string cur_tooth_label = fscene.get_tooth_label(viewer.data().id);
                        vector<float> last_pose = fscene.poses[cur_tooth_label];
                        ImGui::PushItemWidth(-80);
                        ImGui::InputText("Tooth Label", fscene.get_tooth_label(viewer.data().id));
                        ImGui::DragFloat("coordinate_x", &fscene.poses[cur_tooth_label][0], 0.1, -50.0, 50.0);
                        ImGui::DragFloat("coordinate_y", &fscene.poses[cur_tooth_label][1], 0.1, -50.0, 50.0);
                        ImGui::DragFloat("coordinate_z", &fscene.poses[cur_tooth_label][2], 0.1, -50.0, 50.0);
                        ImGui::DragFloat("rotate_x", &fscene.poses[cur_tooth_label][3], 0.1, -2 * igl::PI, 2 * igl::PI);
                        ImGui::DragFloat("rotate_y", &fscene.poses[cur_tooth_label][4], 0.1, -2 * igl::PI, 2 * igl::PI);
                        ImGui::DragFloat("rotate_z", &fscene.poses[cur_tooth_label][5], 0.1, -2 * igl::PI, 2 * igl::PI);
                        ImGui::PopItemWidth();

                        if (last_pose != fscene.poses[cur_tooth_label]) {
                            vector<float> cur_pose = fscene.poses[cur_tooth_label];
                            Eigen::Matrix4d cur_axis_inv_mat = poseToMatrix4d(cur_pose);
                            Eigen::Matrix4d last_axis_inv_mat = poseToMatrix4d(last_pose);

                            Eigen::Matrix4d P = cur_axis_inv_mat * last_axis_inv_mat.inverse();

                            Eigen::Matrix3d R = P.block<3, 3>(0, 0);
                            Eigen::Vector3d T = P.block<3, 1>(0, 3);

                            Eigen::MatrixXd T_vertices(viewer.data().V.rows(), 3);

                            for (int i = 0; i < T_vertices.rows(); i++)
                                for (int j = 0; j < 3; j++)
                                    T_vertices(i, j) = T[j];

                            fscene.teeth_axis[cur_tooth_label] = matrix4dToVector(cur_axis_inv_mat.inverse());
                            
                            viewer.data().set_vertices((R * viewer.data().V.transpose() + T_vertices.transpose()).transpose());

                        }

                    }
                    ImGui::End();
                };
                std::cout << "===============Arrangement complete.===============" << endl;
            }

            if (ImGui::Button("Generate gum##Functions", ImVec2((w - p), 0)))
            {
                std::cout << "===============Gum generating...===============" << endl;

                if (!fscene.generate_gums())
                    return 1;

                viewer.load_mesh_from_file(fscene.upper_gum_path.c_str(), false);
                fscene.set_colors(viewer.data().id, Eigen::RowVector3d(250.0 / 255.0, 203.0 / 255.0, 203.0 / 255.0));
                viewer.load_mesh_from_file(fscene.lower_gum_path.c_str(), false);
                fscene.set_colors(viewer.data().id, Eigen::RowVector3d(250.0 / 255.0, 203.0 / 255.0, 203.0 / 255.0));

                std::cout << "===============Gum generation complete.===============" << endl;
            }
        }

    };

    viewer.launch();
}
