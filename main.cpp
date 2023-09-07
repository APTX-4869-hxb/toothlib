#include <igl/opengl/glfw/Viewer.h>
#include <igl/unproject_onto_mesh.h>
#include <igl/opengl/glfw/imgui/ImGuiHelpers.h>
#include <igl/opengl/glfw/imgui/ImGuiMenu.h>
#include <igl/opengl/glfw/imgui/ImGuiPlugin.h>
#include <igl/opengl/glfw/imgui/ImGuizmoWidget.h>
#include <igl/readOFF.h>
#include <igl/PI.h>
#include <igl/writePLY.h>
#include <iostream>
#include <GLFW/glfw3.h>

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
#include "NumPredictor.h"
using namespace rapidjson;
using namespace std;


float text_shift_scale_factor;
float render_scale;
int cur_frame = 0;
int frame_num;
int play_continue = false;

void align_to_jaw(igl::opengl::glfw::Viewer& viewer, scene& fscene) {
    Eigen::MatrixXd V; // Vertices
    Eigen::MatrixXi F; // Faces
    igl::read_triangle_mesh(fscene.upper_jaw_model.stl_path(), V, F);
    for (int i = 0; i < viewer.core_list.size(); i++)
        viewer.core_list[i].align_camera_center(V, F);

}

void load_gum(igl::opengl::glfw::Viewer& viewer,  scene& fscene) {
    viewer.load_mesh_from_file(fscene.upper_gum_path.c_str(), false);
    fscene.mesh_add_gum("upper", viewer.data().id);
    fscene.set_colors(viewer.data().id, Eigen::RowVector3d(250.0 / 255.0, 203.0 / 255.0, 203.0 / 255.0));
    viewer.load_mesh_from_file(fscene.lower_gum_path.c_str(), false);
    fscene.mesh_add_gum("lower", viewer.data().id);
    fscene.set_colors(viewer.data().id, Eigen::RowVector3d(250.0 / 255.0, 203.0 / 255.0, 203.0 / 255.0));
}

void load_teeth_comp(igl::opengl::glfw::Viewer& viewer, scene& fscene, string result_dir, bool centroid_change=false) {
    for (auto ply : fscene.get_teeth_comp_urn()) {
        //string result_dir = PROJECT_PATH + string("/result");
        string comp_name = result_dir + string("/seg_") + fscene.stl_name() + string("_comp_") + ply.first + string(".ply");
        //cout << ply.first << endl;
        //cout << comp_name;
        viewer.load_mesh_from_file(comp_name.c_str(), false);
        fscene.set_colors(viewer.data().id, 0.5 * Eigen::RowVector3d::Random().array() + 0.5);
        fscene.mesh_add_tooth(viewer.data().id, ply.first);
        //set axis centroid
        Eigen::Vector3d centroid = 0.5 * (viewer.data().V.colwise().maxCoeff() + viewer.data().V.colwise().minCoeff());
        if (centroid_change) {
            fscene.teeth_axis[ply.first][0][3] = centroid[0];
            fscene.teeth_axis[ply.first][1][3] = centroid[1];
            fscene.teeth_axis[ply.first][2][3] = centroid[2];
        }
        Eigen::Matrix4d axis = vectorToMatrixXd(fscene.teeth_axis[ply.first]);
        viewer.data().add_label(centroid + (axis * Eigen::Vector4d(0, -1, 0, 1)).head<3>() * 0.3, ply.first);
    }
}

void callback_draw(igl::opengl::glfw::Viewer& viewer, igl::opengl::glfw::imgui::ImGuiMenu& menu, scene& fscene, HMODULE& hdll) {
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
        if (!(fscene.status & 0b1000)) {
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
                ImGui::DragFloat("coordinate_x", &fscene.poses[cur_tooth_label][0], 0.01, -50.0, 50.0);
                ImGui::DragFloat("coordinate_y", &fscene.poses[cur_tooth_label][1], 0.01, -50.0, 50.0);
                ImGui::DragFloat("coordinate_z", &fscene.poses[cur_tooth_label][2], 0.01, -50.0, 50.0);
                ImGui::DragFloat("rotate_x", &fscene.poses[cur_tooth_label][3], 0.01, -igl::PI, igl::PI);
                ImGui::DragFloat("rotate_y", &fscene.poses[cur_tooth_label][4], 0.01, -igl::PI, igl::PI);
                ImGui::DragFloat("rotate_z", &fscene.poses[cur_tooth_label][5], 0.01, -igl::PI, igl::PI);
                ImGui::PopItemWidth();

                float w = ImGui::GetContentRegionAvail().x;
                float p = ImGui::GetStyle().FramePadding.x;
                if (ImGui::Button("Reset", ImVec2((w - p), 0))) {
                    fscene.poses = fscene.poses_arranged;
                }

                if (last_pose != fscene.poses[cur_tooth_label]) {
                    vector<float> cur_pose = fscene.poses[cur_tooth_label];
                    Eigen::Matrix4d cur_axis_mat = poseToMatrix4d(cur_pose);
                    Eigen::Matrix4d last_axis_mat = poseToMatrix4d(last_pose);

                    Eigen::Matrix4d P = cur_axis_mat * last_axis_mat.inverse();
                    //cout << "-------------------------------" << endl;
                    //cout << "cur_axis_mat: " << cur_axis_mat << endl;
                    //cout << "last_axis_mat: " << last_axis_mat << endl;

                    //cout <<"P: " << P << endl;
                    //Eigen::MatrixXd new_local_V = (viewer.data().V.rowwise().homogeneous() * P);

                    Eigen::MatrixXd new_local_V = (P * viewer.data().V.rowwise().homogeneous().transpose()).transpose();
                    Eigen::MatrixXd new_V = new_local_V.block(0, 0, viewer.data().V.rows(), 3);
                    viewer.data().set_vertices(new_V);

                    fscene.teeth_axis[cur_tooth_label] = matrixXdToVector(cur_axis_mat);
                    //cout << "hhh" << endl;

                    if (fscene.status & 0b100) {
                        string gum;
                        vector<vector<float>> new_gum_v;
                        vector<vector<int>> new_gum_f;

                        if (cur_tooth_label[0] == '3' || cur_tooth_label[0] == '4' || cur_tooth_label[0] == '7' || cur_tooth_label[0] == '8' || (atoi(cur_tooth_label.c_str()) > 94) && (atoi(cur_tooth_label.c_str()) < 99))
                            gum = "lower";
                        else if (cur_tooth_label[0] == '1' || cur_tooth_label[0] == '2' || cur_tooth_label[0] == '5' || cur_tooth_label[0] == '6' || (atoi(cur_tooth_label.c_str()) > 90) && (atoi(cur_tooth_label.c_str()) < 95))
                            gum = "upper";

                        //cout << gum << endl;
                        Eigen::Matrix4d ori_axis_mat = vectorToMatrixXd(fscene.teeth_axis_arranged[cur_tooth_label]);
                        //Eigen::Matrix4d P_ori = cur_axis_mat * ori_axis_mat.inverse();

                        Eigen::Matrix4d P_ori = (cur_axis_mat * ori_axis_mat.inverse()).inverse();
                        Eigen::Vector3d T = P_ori.block<3, 1>(0, 3);
                        P_ori.block<3, 1>(0, 3) = -T;

                        if (!fscene.gum_deform(P_ori, cur_tooth_label, hdll, gum, new_gum_v, new_gum_f)) {
                            std::cout << "gum deform failed." << endl;
                            return false;
                        }

                        Eigen::MatrixXd V = vectorToMatrixXd(new_gum_v); // Vertices
                        Eigen::MatrixXi F = vectorToMatrixXi(new_gum_f); // Faces

                        int index = viewer.mesh_index(fscene.get_gum_id(gum));
                        viewer.data_list[index].clear();
                        viewer.data_list[index].set_mesh(V, F);
                        viewer.data_list[index].compute_normals();
                        viewer.data_list[index].set_colors(fscene.get_color(fscene.get_gum_id(gum)));
                    }
                    //cout << "-------------------------------" << endl;
                }
            }
            ImGui::End();

        }
    };
}

int main(int argc, char* argv[]) {
    //Eigen::MatrixXd V;
    //Eigen::MatrixXi F;

    std::chrono::system_clock::time_point now = std::chrono::system_clock::now();

    std::time_t now_time = std::chrono::system_clock::to_time_t(now);

    std::tm* now_tm = std::localtime(&now_time);
    ostringstream oss;
    oss << put_time(now_tm, "%Y_%m_%d_%H");
    string time = oss.str();

    string result_dir;
    map<string, string> teeth_comp_ply;

    HMODULE hdll;
    hdll = LoadLibrary(("libchohotech_gum_deform_x64.dll"));

    DESTROY_FUNC destroy_gum_deformer = (DESTROY_FUNC)GetProcAddress(hdll, "destroy_gum_deformer");

    //// Load a mesh in OFF format
    //igl::readOFF("bunny.off", V, F);
    scene fscene;
    NumPredictor mlp_predictor;
    StagingGenerator stage_g;
    //model fmodel;
    // Init the viewer
    igl::opengl::glfw::Viewer viewer;

    // Attach a menu plugin
    igl::opengl::glfw::imgui::ImGuiPlugin plugin;
    viewer.plugins.push_back(&plugin);
    igl::opengl::glfw::imgui::ImGuiMenu menu;
    plugin.widgets.push_back(&menu);

    //igl::opengl::glfw::imgui::ImGuizmoWidget gizmo;
    //plugin.widgets.push_back(&gizmo);
    //gizmo.visible = false;


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
                std::cout << "please select scene json..." << endl;
                if (fscene.load_scene(result_dir)) {

                    align_to_jaw(viewer, fscene);
                    load_teeth_comp(viewer, fscene, result_dir, false);

                    for (auto& data : viewer.data_list) {
                        Eigen::MatrixXd V; // Vertices
                        Eigen::MatrixXi F; // Faces

                        string FDI = fscene.get_tooth_label(data.id);
                        string comp_name = result_dir + string("/seg_") + fscene.stl_name() + string("_comp_") + FDI + string(".ply");

                        igl::readPLY(comp_name, V, F);

                        Eigen::MatrixXd teeth_axis = vectorToMatrixXd(fscene.teeth_axis[FDI]);
                        Eigen::MatrixXd new_local_V = (teeth_axis * V.rowwise().homogeneous().transpose()).transpose();
                        // Eigen::MatrixXd new_local_V = (V.rowwise().homogeneous() * axis.transpose());
                        Eigen::MatrixXd new_V = new_local_V.block(0, 0, V.rows(), 3);

                        Eigen::Vector3d centroid = 0.5 * (new_V.colwise().maxCoeff() + new_V.colwise().minCoeff());
                        data.clear();
                        data.set_mesh(new_V, F);
                        data.compute_normals();
                        data.set_colors(fscene.get_color(data.id));
                        Eigen::Matrix4d axis = vectorToMatrixXd(fscene.teeth_axis[fscene.get_tooth_label(data.id)]);
                        data.add_label(centroid + (axis * Eigen::Vector4d(0, -1, 0, 1)).head<3>() * 0.3, fscene.get_tooth_label(data.id));
                    }

                    if (fscene.status & 0b100) {
                        load_gum(viewer, fscene);
                        Document upper_gum_doc;
                        Document lower_gum_doc;
                        upper_gum_doc.Parse(fscene.upper_gum_doc_str.c_str());
                        lower_gum_doc.Parse(fscene.lower_gum_doc_str.c_str());
                        fscene.upper_jaw_model.create_gum_deformer(upper_gum_doc, hdll);
                        fscene.lower_jaw_model.create_gum_deformer(lower_gum_doc, hdll);
                    }

                    callback_draw(viewer, menu, fscene, hdll);
                    std::cout << "load success." << endl;
                    std::cout << "The name of jaw file is: " << fscene.stl_name() << endl;
                }
                else
                    std::cout << "no scene loaded." << endl;
                //viewer.load_scene(); 
            }
            ImGui::SameLine(0, p);
            if (ImGui::Button("Save##Scene", ImVec2((w - p) / 2.f, 0)))
            {
                std::cout << "saving scene..." << endl;
                //string dir = igl::file_dialog_open();
                //if (dir.length() == 0) {
                //    cout << "no directory selected." << endl;
                //    return 1;
                //}
                //result_dir = dir;
                fscene.save_scene(result_dir);
                for (auto& data : viewer.data_list) {
                    if (!fscene.mesh_is_tooth(data.id)) {
                        string type = fscene.get_gum_type(data.id);
                        string gum_name;
                        if (type == "upper")
                            gum_name = result_dir + string("/") + fscene.upper_jaw_model.stl_name() + string("_gum.ply");
                        else if (type == "lower")
                            gum_name = result_dir + string("/") + fscene.lower_jaw_model.stl_name() + string("_gum.ply");
                        igl::writePLY(gum_name, data.V, data.F);
                    }
                }
                std::cout << "scene saved." << endl;
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
                std::cout << "please select one jaw..." << endl;
                string fpath_1 = igl::file_dialog_open();

                if (fpath_1 == "false") {
                    cout << "no models loaded." << endl;
                    return 1;
                }
                std::cout << "please select the other jaw in the same pair..." << endl;
                string fpath_2 = igl::file_dialog_open();
                if (fpath_2 == "false") {
                    cout << "no models loaded." << endl;
                    return 1;
                }

                viewer.load_mesh_from_file(fpath_1.c_str());
                viewer.load_mesh_from_file(fpath_2.c_str());

                fscene = scene(fpath_1, fpath_2);
                result_dir = PROJECT_PATH + string("/result/") + fscene.stl_name() + string("_") + time;
                cout << "result will be saved to " << result_dir << endl;
                //string fpath = viewer.open_dialog_load_mesh();
                //if(fpath != "false")
                //    fmodel = model(fpath);
            }
            ImGui::SameLine(0, p);
            if (ImGui::Button("Save selected##Model", ImVec2((w - p) / 2.f, 0)))
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
                if (fscene.status & 0b1) {
                    cout << "This scene has been segmented already." << endl;
                    return 1;
                }
                std::cout << "===============Segment...===============" << endl;
                //cout << result_dir << endl;
                if (!fscene.segment_jaws(result_dir, teeth_comp_ply))
                    return 1;
                //string result_dir = PROJECT_PATH + string("/result");
                for (auto ply : teeth_comp_ply) {
                    ofstream ofs;
                    string comp_name = result_dir + string("/seg_") + fscene.stl_name() + string("_comp_") + ply.first + string(".ply");
                    ofs.open(comp_name, ofstream::out | ofstream::binary);
                    ofs << ply.second;
                    ofs.close();
                    //cout << ply.first << endl;
                    //cout << ply.second << endl;

                }
                load_teeth_comp(viewer, fscene, result_dir, true);

                for (auto ply : fscene.get_teeth_comp_urn()) {
                    string comp_name = result_dir + string("/seg_") + fscene.stl_name() + string("_comp_") + ply.first + string(".ply");
                    Eigen::MatrixXd V; // Vertices
                    Eigen::MatrixXi F; // Faces
                    igl::readPLY(comp_name, V, F);
                    Eigen::MatrixXd axis = vectorToMatrixXd(fscene.teeth_axis[ply.first]);
                    Eigen::MatrixXd new_local_V = (axis.inverse() * V.rowwise().homogeneous().transpose()).transpose();
                    // Eigen::MatrixXd new_local_V = (V.rowwise().homogeneous() * axis.transpose());
                    Eigen::MatrixXd new_V = new_local_V.block(0, 0, V.rows(), 3);
                    igl::writePLY(comp_name, new_V, F);
                }

                fscene.calc_poses(fscene.poses, fscene.teeth_axis);

                viewer.erase_mesh(0);
                viewer.erase_mesh(0);
                callback_draw(viewer, menu, fscene, hdll);

                fscene.teeth_axis_origin = fscene.teeth_axis;
                fscene.poses_origin = fscene.poses;
                fscene.status |= 0b1;
                std::cout << "===============Segment complete.===============" << endl;
            }

            if (ImGui::Button("Arrangement##Functions", ImVec2((w - p), 0)))
            {
                if (fscene.status & 0b10) {
                    cout << "This scene has been arranged already." << endl;
                    return 1;
                }
                std::cout << "===============Arrangement...===============" << endl;

                if (!fscene.arrangement(teeth_comp_ply))
                    return 1;

                //cout << "before arrangement: " << fscene.poses["31"] << endl;
                fscene.calc_poses(fscene.poses, fscene.teeth_axis);
                //cout << "after arrangement: " << fscene.poses["31"] << endl;

                for (auto& data : viewer.data_list) {
                    Eigen::MatrixXd V; // Vertices
                    Eigen::MatrixXi F; // Faces

                    string FDI = fscene.get_tooth_label(data.id);

                    //string result_dir = PROJECT_PATH + string("/result");
                    //ofstream ofs;
                    string comp_name = result_dir + string("/seg_") + fscene.stl_name() + string("_comp_") + FDI + string(".ply");
                    //ofs.open(comp_name, ofstream::out | ofstream::binary);
                    //ofs << teeth_comp_ply[FDI];
                    //ofs.close();
                    igl::readPLY(comp_name, V, F);

                    Eigen::MatrixXd teeth_axis = vectorToMatrixXd(fscene.teeth_axis[FDI]);
                    Eigen::MatrixXd new_local_V = (teeth_axis * V.rowwise().homogeneous().transpose()).transpose();
                    // Eigen::MatrixXd new_local_V = (V.rowwise().homogeneous() * axis.transpose());
                    Eigen::MatrixXd new_V = new_local_V.block(0, 0, V.rows(), 3);

                    Eigen::Vector3d centroid = 0.5 * (new_V.colwise().maxCoeff() + new_V.colwise().minCoeff());
                    data.clear();
                    data.set_mesh(new_V, F);
                    data.compute_normals();
                    data.set_colors(fscene.get_color(data.id));
                    Eigen::Matrix4d axis = vectorToMatrixXd(fscene.teeth_axis[fscene.get_tooth_label(data.id)]);
                    data.add_label(centroid + (axis * Eigen::Vector4d(0, -1, 0, 1)).head<3>() * 0.3, fscene.get_tooth_label(data.id));
                }
                fscene.teeth_axis_arranged = fscene.teeth_axis;
                fscene.poses_arranged = fscene.poses;
                fscene.status |= 0b10;
                std::cout << "===============Arrangement complete.===============" << endl;
            }

            if (ImGui::Button("Generate gum##Functions", ImVec2((w - p), 0)))
            {
                if (fscene.status & 0b100) {
                    cout << "This scene has Generated gums already." << endl;
                    return 1;
                }
                std::cout << "===============Gum generating...===============" << endl;

                if (!fscene.generate_gums(hdll, result_dir))
                    return 1;

                load_gum(viewer, fscene);

                fscene.status |= 0b100;
                std::cout << "===============Gum generation complete.===============" << endl;
            }
            if (ImGui::Button("Predict Number##Functions", ImVec2((w - p), 0)))
            {
                std::cout << "===============Predict Staging Number...===============" << endl;
                mlp_predictor.Solution(fscene.teeth_axis_origin, fscene.teeth_axis);
                std::cout << "===============predict complete.===============" << endl;
                std::cout << "predict result: " << to_string(mlp_predictor.getStagingNum()) << endl;
            }
            if (ImGui::Button("Staging Generation##Functions", ImVec2((w - p), 0)))
            {
                std::cout << "===============Staging...===============" << endl;
                std::cout << "before stging:" << mlp_predictor.staging_num << endl;
                auto res = stage_g.Solution(fscene.teeth_axis_origin, fscene.teeth_axis, mlp_predictor.staging_num);

                fscene.status |= 0b1000;
                auto poses = fscene.poses;
                
                //vector<string> labels;
                
                for (auto tooth : res) {
                    string cur_tooth_label = tooth.first;
                    auto axis_vec = tooth.second;
                    frame_num = axis_vec.size();

                    vector<Eigen::Matrix4d> tooth_axis_mats;
                    for (int i = 0; i < frame_num; i++) {
                        
                        Eigen::Matrix4d cur_mat = Eigen::Matrix4d::Zero();
                        for (int j = 0; j < 3; j++)
                            cur_mat(j, 3) = axis_vec[i][j];
                        for (int j = 3; j < 12; j++)
                            cur_mat((j - 3) / 3, j % 3) = axis_vec[i][j];
                        cur_mat(3, 3) = 1;

                        tooth_axis_mats.push_back(cur_mat);
                    }
                    fscene.staging_axis_mats.insert(pair<string, vector<Eigen::Matrix4d>>(cur_tooth_label, tooth_axis_mats));
                    //labels.push_back(cur_tooth_label);
                }

                //fscene.poses = fscene.poses_origin;

                for (auto& data : viewer.data_list) {
                    if (fscene.mesh_is_tooth(data.id)) {
                        string label = fscene.get_tooth_label(data.id);
                        Eigen::Matrix4d cur_axis_mat = fscene.staging_axis_mats[label][0];
                        vector<float> last_pose = fscene.poses[label];
                        Eigen::Matrix4d last_axis_mat = poseToMatrix4d(last_pose);
                        Eigen::Matrix4d P = cur_axis_mat * last_axis_mat.inverse();
                        Eigen::MatrixXd new_local_V = (P * data.V.rowwise().homogeneous().transpose()).transpose();
                        Eigen::MatrixXd new_V = new_local_V.block(0, 0, data.V.rows(), 3);

                        data.set_vertices(new_V);
                    }
                    else {
                        data.set_visible(false);
                    }
                }
                
                menu.callback_draw_custom_window = [&]() {
                    ImGui::SetNextWindowPos(ImVec2(180.f * menu.menu_scaling(), 10), ImGuiCond_FirstUseEver);
                    ImGui::SetNextWindowSize(ImVec2(200, 240), ImGuiCond_FirstUseEver);
                    ImGui::Begin(
                        "Staging Frame", nullptr,
                        ImGuiWindowFlags_NoSavedSettings
                    );
                    int last_frame = cur_frame;
                    char cur_frame_char[5];
                    itoa(cur_frame, cur_frame_char, 10);
                    ImGui::LabelText("Frame", cur_frame_char);
                    ImGui::LabelText("Tooth Label", (fscene.get_tooth_label(viewer.data().id)).c_str());
                    if (ImGui::Button("Last frame") && cur_frame)
                        cur_frame -= 1;
                    if (ImGui::Button("Next frame") && cur_frame < frame_num - 1)
                        cur_frame += 1;
                    if (ImGui::Button("Play to end")) {
                        play_continue = true;
                    }
                    if (ImGui::Button("Stop")) {
                        play_continue = false;
                    }

                    if (cur_frame < frame_num && play_continue) {
                        cur_frame += 1;
                        std::this_thread::sleep_for(std::chrono::milliseconds(150));
                    }
                    if(cur_frame == frame_num)
                        play_continue = false;

                    
                    if (last_frame != cur_frame) {
                        cout << "last: " << last_frame << endl;
                        cout << "cur: " << cur_frame << endl;

                        for (auto& data : viewer.data_list) {
                            if (fscene.mesh_is_tooth(data.id)) {
                                string label = fscene.get_tooth_label(data.id);

                                cout << "label: " << label << endl;
                                Eigen::Matrix4d cur_axis_mat = fscene.staging_axis_mats[label][cur_frame];
                                Eigen::Matrix4d last_axis_mat = fscene.staging_axis_mats[label][last_frame];
                                cout << "last_axis_mat: " << last_axis_mat << endl; 
                                cout << "cur_axis_mat: " << cur_axis_mat << endl;
                                
                                Eigen::Matrix4d P = cur_axis_mat * last_axis_mat.inverse();


                                //cout <<"P: " << P << endl;
                                Eigen::MatrixXd new_local_V = (P * data.V.rowwise().homogeneous().transpose()).transpose();
                                Eigen::MatrixXd new_V = new_local_V.block(0, 0, data.V.rows(), 3);

                                data.set_vertices(new_V);

                            }
                        }
                    }
                    ImGui::End();
                };
                //fscene.status &= 0b0111;
                fscene.poses = poses;
                
                std::cout << "===============Staging complete.===============" << endl;
            }
        }
    };

    viewer.launch();

    //glfwTerminate(); // This terminates GLFW after both viewers are closed
}
