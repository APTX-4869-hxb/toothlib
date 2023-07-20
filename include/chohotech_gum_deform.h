// chohotech_gum_deform.h

/*
 * @brief The transformation info of a tooth relative to the tooth pose at the 1st step.
 *        The transformation is defined as: new_x = x * rotation.T + translation, where x is
 *        gum vertex matrix with shape nx3.
 * @param tid: The tooth id.
 * @param rotation: The rotation matrix relative to the tooth pose at the 1st step.
 * @param translation: The translation vector relative to the tooth pose at the 1st step.
 */
struct ToothTransformation {
    int tid;    // tooth id
    double rotation[9]; // rotation matrix
    double translation[3]; // translation vector
};

/*
 * @brief Construct a gum deformer.
 * @param gum_vertices: Gum vertices with length nx3.
 * @param gum_vertices_len: Length of gum vertices, nx3.
 * @param gum_faces: Gum faces with length mx3.
 * @param gum_faces_len: Length of gum faces, mx3.
 * @param api_result_str: API result string. The string can be got from the API-returned result
 *        with a key "result". The string will be converted to a json dictionary which SHOULD include
 *        the following keys: "handle_dict", "surf_point_dict", "tooth_part_gum_dict", "tooth_boundary_dict'.
 *        You don't need to concern about the meaning of the keys.
 * @param api_result_str_len: Length of API result string.
 * @param gum_deformer_ptr: Pointer to gum deformer.
 */
//extern "C" int create_gum_deformer(
//        const double *gum_vertices, unsigned int gum_vertices_len,
//        const int *gum_faces, unsigned int gum_faces_len,
//        const char *api_result_str, unsigned int api_result_str_len,
//        void **gum_deformer_ptr);
//
///*
// * @brief Destroy a gum deformer.
// * @param gum_deformer_ptr: Pointer to gum deformer.
// */
//extern "C" void destroy_gum_deformer(void *gum_deformer);
//
///*
// * @brief Do gum deformation.
// * @param gum_deformer: The pointer to the gum deformer.
// * @param tooth_transform_list: A list of tooth transform.
// * @param n_teeth: Number of transformed teeth.
// * @param deformed_vertices: The gum vertices after deformation.
// * @param deformed_vertices_len: The return length of deformed vertices.
// * @param deformed_faces: The gum faces after deformation.
// * @param deform_faces_len: The return length of deformed faces.
// * @param gum_info_str: The result string including information about the deformed gum.
// * @param gum_info_str_len: The return length of gum_info_str.
// */
//extern "C" int deform(void *gum_deformer,
//                      const ToothTransformation *tooth_transform_list,
//                      unsigned int n_teeth,
//                      double *deformed_vertices, unsigned int *deformed_vertices_len,
//                      int *deformed_faces, unsigned int *deformed_faces_len,
//                      char *gum_info_str = NULL, unsigned int *gum_info_str_len = NULL);