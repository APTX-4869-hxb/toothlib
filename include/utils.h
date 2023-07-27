#include <vector>
#include <Eigen/Core>
#include <Eigen/Geometry>

Eigen::Matrix4d vectorToMatrix4d(const std::vector<std::vector<float>>& vec);
std::vector<std::vector<float>> matrix4dToVector(const Eigen::Matrix4d& mat);
Eigen::Matrix4d poseToMatrix4d(std::vector<float> pose);