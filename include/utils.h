#include <map>
#include <vector>
#include <Eigen/Core>
#include <Eigen/Geometry>

Eigen::Matrix4d vectorToMatrix4d(const std::vector<std::vector<float>>& vec);
std::vector<std::vector<float>> matrix4dToVector(const Eigen::Matrix4d& mat);
Eigen::Matrix4d poseToMatrix4d(std::vector<float> pose);

template<typename KeyType, typename ValueType>
void assignToMap(std::map<KeyType, ValueType>& mapObj, const KeyType& key, const ValueType& value) {
    if (mapObj.find(key) == mapObj.end())
        mapObj.insert(std::pair<KeyType, ValueType>(key, value));
    else
        mapObj[key] = value;
}