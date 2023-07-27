#include <utils.h>

Eigen::Matrix4d vectorToMatrix4d(const std::vector<std::vector<float>>& vec) {
    Eigen::Matrix4d mat;
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            mat(i, j) = vec[i][j];
        }
    }
    return mat;
}

std::vector<std::vector<float>> matrix4dToVector(const Eigen::Matrix4d& mat) {
    std::vector<std::vector<float>> vec;
    for (int i = 0; i < 4; ++i) {
        std::vector<float> line;
        for (int j = 0; j < 4; ++j) {
            line.push_back(float(mat(i, j)));
        }
        vec.push_back(line);
    }
    return vec;
}

Eigen::Matrix4d poseToMatrix4d(std::vector<float> pose) {
    Eigen::Vector3d euler_angles(pose[5], pose[4], pose[3]); // Z, Y, X

    Eigen::Matrix3d R;
    R = Eigen::AngleAxisd(euler_angles(0), Eigen::Vector3d::UnitZ())
        * Eigen::AngleAxisd(euler_angles(1), Eigen::Vector3d::UnitY())
        * Eigen::AngleAxisd(euler_angles(2), Eigen::Vector3d::UnitX());

    Eigen::Vector3d T(pose[0], pose[1], pose[2]);

    Eigen::Matrix4d pose_matrix = Eigen::Matrix4d::Identity();
    pose_matrix.block<3, 3>(0, 0) = R;
    pose_matrix.block<3, 1>(0, 3) = T;

    return pose_matrix;
}