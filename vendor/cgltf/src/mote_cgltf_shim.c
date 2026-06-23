#include "cgltf.h"

typedef struct mote_cgltf_counts {
    size_t meshes;
    size_t materials;
    size_t accessors;
    size_t nodes;
    size_t scenes;
    size_t animations;
    int has_default_scene;
} mote_cgltf_counts;

typedef struct mote_cgltf_mesh_info {
    const char* name;
    size_t primitives_count;
    size_t weights_count;
    size_t target_names_count;
} mote_cgltf_mesh_info;

typedef struct mote_cgltf_primitive_info {
    int type;
    int has_indices;
    int has_material;
    size_t attributes_count;
    size_t targets_count;
} mote_cgltf_primitive_info;

typedef struct mote_cgltf_material_info {
    const char* name;
    int alpha_mode;
    float alpha_cutoff;
    int double_sided;
    int unlit;
    int has_pbr_metallic_roughness;
    int has_normal_texture;
    int has_occlusion_texture;
    int has_emissive_texture;
    float emissive_factor[3];
} mote_cgltf_material_info;

typedef struct mote_cgltf_node_info {
    const char* name;
    size_t children_count;
    int has_mesh;
    int has_translation;
    int has_rotation;
    int has_scale;
    float translation[3];
    float rotation[4];
    float scale[3];
} mote_cgltf_node_info;

int mote_cgltf_parse_file(const char* path, cgltf_data** out_data)
{
    cgltf_options options = {0};
    if (out_data == NULL) {
        return (int)cgltf_result_invalid_options;
    }

    *out_data = NULL;
    return (int)cgltf_parse_file(&options, path, out_data);
}

int mote_cgltf_load_buffers(const char* path, cgltf_data* data)
{
    cgltf_options options = {0};
    return (int)cgltf_load_buffers(&options, data, path);
}

int mote_cgltf_validate(cgltf_data* data)
{
    return (int)cgltf_validate(data);
}

void mote_cgltf_free(cgltf_data* data)
{
    cgltf_free(data);
}

int mote_cgltf_get_counts(const cgltf_data* data, mote_cgltf_counts* out_counts)
{
    if (data == NULL || out_counts == NULL) {
        return 0;
    }

    out_counts->meshes = data->meshes_count;
    out_counts->materials = data->materials_count;
    out_counts->accessors = data->accessors_count;
    out_counts->nodes = data->nodes_count;
    out_counts->scenes = data->scenes_count;
    out_counts->animations = data->animations_count;
    out_counts->has_default_scene = data->scene != NULL ? 1 : 0;
    return 1;
}

size_t mote_cgltf_num_components(int type)
{
    return cgltf_num_components((cgltf_type)type);
}

int mote_cgltf_accessor_unpack_floats(const cgltf_accessor* accessor, float* out_values, size_t float_count, size_t* out_written)
{
    size_t written;
    if (accessor == NULL) {
        return 0;
    }

    written = cgltf_accessor_unpack_floats(accessor, out_values, float_count);
    if (out_written != NULL) {
        *out_written = written;
    }
    return 1;
}

int mote_cgltf_accessor_read_float(const cgltf_accessor* accessor, size_t index, float* out_values, size_t element_size)
{
    if (accessor == NULL || out_values == NULL) {
        return 0;
    }

    return cgltf_accessor_read_float(accessor, index, out_values, element_size) != 0;
}

const cgltf_accessor* mote_cgltf_data_accessor_at(const cgltf_data* data, size_t index)
{
    if (data == NULL || index >= data->accessors_count) {
        return NULL;
    }

    return &data->accessors[index];
}

size_t mote_cgltf_accessor_count(const cgltf_data* data)
{
    if (data == NULL) {
        return 0;
    }

    return data->accessors_count;
}

const cgltf_mesh* mote_cgltf_data_mesh_at(const cgltf_data* data, size_t index)
{
    if (data == NULL || index >= data->meshes_count) {
        return NULL;
    }

    return &data->meshes[index];
}

size_t mote_cgltf_mesh_count(const cgltf_data* data)
{
    if (data == NULL) {
        return 0;
    }

    return data->meshes_count;
}

int mote_cgltf_mesh_get_info(const cgltf_mesh* mesh, mote_cgltf_mesh_info* out_info)
{
    if (mesh == NULL || out_info == NULL) {
        return 0;
    }

    out_info->name = mesh->name;
    out_info->primitives_count = mesh->primitives_count;
    out_info->weights_count = mesh->weights_count;
    out_info->target_names_count = mesh->target_names_count;
    return 1;
}

const cgltf_primitive* mote_cgltf_mesh_primitive_at(const cgltf_mesh* mesh, size_t index)
{
    if (mesh == NULL || index >= mesh->primitives_count) {
        return NULL;
    }

    return &mesh->primitives[index];
}

int mote_cgltf_primitive_get_info(const cgltf_primitive* primitive, mote_cgltf_primitive_info* out_info)
{
    if (primitive == NULL || out_info == NULL) {
        return 0;
    }

    out_info->type = (int)primitive->type;
    out_info->has_indices = primitive->indices != NULL ? 1 : 0;
    out_info->has_material = primitive->material != NULL ? 1 : 0;
    out_info->attributes_count = primitive->attributes_count;
    out_info->targets_count = primitive->targets_count;
    return 1;
}

const cgltf_material* mote_cgltf_data_material_at(const cgltf_data* data, size_t index)
{
    if (data == NULL || index >= data->materials_count) {
        return NULL;
    }

    return &data->materials[index];
}

size_t mote_cgltf_material_count(const cgltf_data* data)
{
    if (data == NULL) {
        return 0;
    }

    return data->materials_count;
}

int mote_cgltf_material_get_info(const cgltf_material* material, mote_cgltf_material_info* out_info)
{
    if (material == NULL || out_info == NULL) {
        return 0;
    }

    out_info->name = material->name;
    out_info->alpha_mode = (int)material->alpha_mode;
    out_info->alpha_cutoff = material->alpha_cutoff;
    out_info->double_sided = material->double_sided != 0 ? 1 : 0;
    out_info->unlit = material->unlit != 0 ? 1 : 0;
    out_info->has_pbr_metallic_roughness = material->has_pbr_metallic_roughness != 0 ? 1 : 0;
    out_info->has_normal_texture = material->normal_texture.texture != NULL ? 1 : 0;
    out_info->has_occlusion_texture = material->occlusion_texture.texture != NULL ? 1 : 0;
    out_info->has_emissive_texture = material->emissive_texture.texture != NULL ? 1 : 0;
    out_info->emissive_factor[0] = material->emissive_factor[0];
    out_info->emissive_factor[1] = material->emissive_factor[1];
    out_info->emissive_factor[2] = material->emissive_factor[2];
    return 1;
}

const cgltf_node* mote_cgltf_data_node_at(const cgltf_data* data, size_t index)
{
    if (data == NULL || index >= data->nodes_count) {
        return NULL;
    }

    return &data->nodes[index];
}

size_t mote_cgltf_node_count(const cgltf_data* data)
{
    if (data == NULL) {
        return 0;
    }

    return data->nodes_count;
}

int mote_cgltf_node_get_info(const cgltf_node* node, mote_cgltf_node_info* out_info)
{
    if (node == NULL || out_info == NULL) {
        return 0;
    }

    out_info->name = node->name;
    out_info->children_count = node->children_count;
    out_info->has_mesh = node->mesh != NULL ? 1 : 0;
    out_info->has_translation = node->has_translation != 0 ? 1 : 0;
    out_info->has_rotation = node->has_rotation != 0 ? 1 : 0;
    out_info->has_scale = node->has_scale != 0 ? 1 : 0;
    out_info->translation[0] = node->translation[0];
    out_info->translation[1] = node->translation[1];
    out_info->translation[2] = node->translation[2];
    out_info->rotation[0] = node->rotation[0];
    out_info->rotation[1] = node->rotation[1];
    out_info->rotation[2] = node->rotation[2];
    out_info->rotation[3] = node->rotation[3];
    out_info->scale[0] = node->scale[0];
    out_info->scale[1] = node->scale[1];
    out_info->scale[2] = node->scale[2];
    return 1;
}

void mote_cgltf_node_transform_local(const cgltf_node* node, float* out_matrix)
{
    if (node == NULL || out_matrix == NULL) {
        return;
    }

    cgltf_node_transform_local(node, out_matrix);
}

void mote_cgltf_node_transform_world(const cgltf_node* node, float* out_matrix)
{
    if (node == NULL || out_matrix == NULL) {
        return;
    }

    cgltf_node_transform_world(node, out_matrix);
}
