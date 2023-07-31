#include <igl/opengl/glfw/Viewer.h>
#include <igl/unproject_onto_mesh.h>
#include <igl/opengl/glfw/imgui/ImGuiHelpers.h>
#include <igl/opengl/glfw/imgui/ImGuiMenu.h>
#include <igl/opengl/glfw/imgui/ImGuiPlugin.h>
#include <igl/opengl/glfw/imgui/ImGuizmoWidget.h>
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

    HMODULE hdll;
    hdll = LoadLibrary(("libchohotech_gum_deform_x64.dll"));

    DESTROY_FUNC destroy_gum_deformer = (DESTROY_FUNC)GetProcAddress(hdll, "destroy_gum_deformer");

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

                    //set axis centroid
                    Eigen::Vector3d centroid = 0.5 * (viewer.data().V.colwise().maxCoeff() + viewer.data().V.colwise().minCoeff());
                    for (int i = 0; i < 3; i++)
                        fscene.teeth_axis[ply.first][i][3] = centroid[i];
                    viewer.data().add_label(centroid + Eigen::Vector3d(0, -1, 0) * 5, ply.first);

                    //// Add a 3D gizmo plugin
                    //igl::opengl::glfw::imgui::ImGuizmoWidget gizmo;
                    //plugin.widgets.push_back(&gizmo);
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
                            std::cout << "You clicked on tooth #" << fscene.get_tooth_label(data.id) << std::endl;
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
                        ImGui::DragFloat("rotate_x", &fscene.poses[cur_tooth_label][3], 0.1, -igl::PI, igl::PI);
                        ImGui::DragFloat("rotate_y", &fscene.poses[cur_tooth_label][4], 0.1, -igl::PI, igl::PI);
                        ImGui::DragFloat("rotate_z", &fscene.poses[cur_tooth_label][5], 0.1, -igl::PI, igl::PI);
                        ImGui::PopItemWidth();

                        if (last_pose != fscene.poses[cur_tooth_label]) {
                            vector<float> cur_pose = fscene.poses[cur_tooth_label];
                            Eigen::Matrix4d cur_axis_mat = poseToMatrix4d(cur_pose);
                            Eigen::Matrix4d last_axis_mat = poseToMatrix4d(last_pose);

                            Eigen::Matrix4d P = cur_axis_mat * last_axis_mat.inverse();

                            Eigen::MatrixXd new_local_V = (P * viewer.data().V.rowwise().homogeneous().transpose()).transpose();
                            Eigen::MatrixXd new_V = new_local_V.block(0, 0, viewer.data().V.rows(), 3);
                            viewer.data().set_vertices(new_V);


                            fscene.teeth_axis[cur_tooth_label] = matrixXdToVector(cur_axis_mat);
                            //cout << "hhh" << endl;

                            //if (fscene.has_gum) {
                            //    string gum;
                            //    vector<vector<float>> new_gum_v;
                            //    vector<vector<int>> new_gum_f;

                            //    if (cur_tooth_label[0] == '3' || cur_tooth_label[0] == '4' || cur_tooth_label[0] == '7' || cur_tooth_label[0] == '8' || (atoi(cur_tooth_label.c_str()) > 94) && (atoi(cur_tooth_label.c_str()) < 99))
                            //        gum = "lower";
                            //    else if (cur_tooth_label[0] == '1' || cur_tooth_label[0] == '2' || cur_tooth_label[0] == '5' || cur_tooth_label[0] == '6' || (atoi(cur_tooth_label.c_str()) > 90) && (atoi(cur_tooth_label.c_str()) < 95))
                            //        gum = "upper";

                            //    //cout << gum << endl;

                            //    if (!fscene.gum_deform(P, cur_tooth_label, hdll, gum, new_gum_v, new_gum_f)) {
                            //        cout << "gum deform failed." << endl;
                            //        return false;
                            //    }

                            //    Eigen::MatrixXd V = vectorToMatrixXd(new_gum_v); // Vertices
                            //    Eigen::MatrixXi F = vectorToMatrixXi(new_gum_f); // Faces
                            //    
                            //    int index = viewer.mesh_index(fscene.get_gum_id(gum));
                            //    viewer.data_list[index].clear();
                            //    viewer.data_list[index].set_mesh(V, F);
                            //    viewer.data_list[index].set_colors(fscene.get_color(fscene.get_gum_id(gum)));
                            //}
                        }
                    }
                    ImGui::End();
                };
                std::cout << "===============Segment complete.===============" << endl;
            }

            if (ImGui::Button("Arrangement##Functions", ImVec2((w - p), 0)))
            {
                std::cout << "===============Arrangement...===============" << endl;

                if (!fscene.arrangement())
                    return 1;


                for (auto& data : viewer.data_list) {
                    Eigen::MatrixXd V; // Vertices
                    Eigen::MatrixXi F; // Faces

                    string FDI = fscene.get_tooth_label(data.id);

                    ofstream ofs;
                    string comp_name = string("tmp.ply");
                    ofs.open(comp_name, ofstream::out | ofstream::binary);
                    ofs << fscene.get_teeth_comp()[FDI];
                    ofs.close();
                    
                    igl::readPLY("tmp.ply", V, F);
                    Eigen::Vector3d centroid = 0.5 * (V.colwise().maxCoeff() + V.colwise().minCoeff());
                    data.clear();
                    data.set_mesh(V, F);
                    data.set_colors(fscene.get_color(data.id));
                    data.add_label(centroid + Eigen::Vector3d(0, -1, 0) * 5, fscene.get_tooth_label(data.id));
                }
                std::cout << "===============Arrangement complete.===============" << endl;
            }

            if (ImGui::Button("Generate gum##Functions", ImVec2((w - p), 0)))
            {
                std::cout << "===============Gum generating...===============" << endl;

                if (!fscene.generate_gums(hdll))
                    return 1;

                viewer.load_mesh_from_file(fscene.upper_gum_path.c_str(), false);
                fscene.mesh_add_gum("upper", viewer.data().id);
                fscene.set_colors(viewer.data().id, Eigen::RowVector3d(250.0 / 255.0, 203.0 / 255.0, 203.0 / 255.0));
                viewer.load_mesh_from_file(fscene.lower_gum_path.c_str(), false);
                fscene.mesh_add_gum("lower", viewer.data().id);
                fscene.set_colors(viewer.data().id, Eigen::RowVector3d(250.0 / 255.0, 203.0 / 255.0, 203.0 / 255.0));

                std::cout << "===============Gum generation complete.===============" << endl;
            }
        }

    };

    viewer.launch();
}
