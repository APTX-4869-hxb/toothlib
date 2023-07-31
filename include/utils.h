#include <Windows.h>
#include <map>
#include <vector>
#include <Eigen/Core>
#include <Eigen/Geometry>
#include "chohotech_gum_deform.h"

Eigen::MatrixXi vectorToMatrixXi(const std::vector<std::vector<int>>& vec);
Eigen::MatrixXd vectorToMatrixXd(const std::vector<std::vector<float>>& vec);
std::vector<std::vector<float>> matrixXdToVector(const Eigen::MatrixXd& mat);
Eigen::Matrix4d poseToMatrix4d(std::vector<float> pose);

template<typename KeyType, typename ValueType>
void assignToMap(std::map<KeyType, ValueType>& mapObj, const KeyType& key, const ValueType& value) {
    if (mapObj.find(key) == mapObj.end())
        mapObj.insert(std::pair<KeyType, ValueType>(key, value));
    else
        mapObj[key] = value;
}

typedef int (WINAPI* CREATE_FUNC)(const double*, unsigned int,
    const int*, unsigned int, const char*, unsigned int, void**);
typedef int (WINAPI* DEFORM_FUNC)(void*,
    const ToothTransformation*,
    unsigned int,
    double*, unsigned int*,
    int*, unsigned int*,
    char*, unsigned int*);
typedef void (WINAPI* DESTROY_FUNC)(void*);