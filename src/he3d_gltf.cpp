#include "he3d.hpp"

static long long he3d_cgltf_atoll(const char *text)
{
   long long value = 0;
   int sign = 1;
   if (!text) return 0;
   while (*text == ' ' || *text == '\t') ++text;
   if (*text == '-') {
      sign = -1;
      ++text;
   } else if (*text == '+') {
      ++text;
   }
   while (*text >= '0' && *text <= '9') value = value * 10 + (*text++ - '0');
   return sign < 0 ? -value : value;
}

static int he3d_cgltf_atoi(const char *text) { return (int)he3d_cgltf_atoll(text); }

static float he3d_cgltf_atof(const char *text)
{
   if (!text) return 0.0f;
   float value = 0.0f;
   float sign = 1.0f;
   while (*text == ' ' || *text == '\t') ++text;
   if (*text == '-') {
      sign = -1.0f;
      ++text;
   } else if (*text == '+') {
      ++text;
   }
   while (*text >= '0' && *text <= '9') value = value * 10.0f + (float)(*text++ - '0');
   if (*text == '.') {
      float place = 0.1f;
      ++text;
      while (*text >= '0' && *text <= '9') {
         value += (float)(*text++ - '0') * place;
         place *= 0.1f;
      }
   }
   int exponent = 0;
   if (*text == 'e' || *text == 'E') {
      ++text;
      exponent = he3d_cgltf_atoi(text);
   }
   while (exponent > 0) {
      value *= 10.0f;
      --exponent;
   }
   while (exponent < 0) {
      value *= 0.1f;
      ++exponent;
   }
   return sign * value;
}

#define CGLTF_IMPLEMENTATION
#define CGLTF_MALLOC(size) HE3D::Alloc((HE3D::uint64_t)(size))
#define CGLTF_FREE(pointer) HE3D::Free(pointer)
#define CGLTF_ATOI(text) he3d_cgltf_atoi(text)
#define CGLTF_ATOF(text) he3d_cgltf_atof(text)
#define CGLTF_ATOLL(text) he3d_cgltf_atoll(text)
#include "cgltf.h"

namespace HE3D
{
namespace
{
static void *GltfAlloc(void *, cgltf_size size) { return Alloc((uint64_t)size); }

static void GltfFree(void *, void *pointer) { Free(pointer); }

static bool CopyBytes(void *destination, const void *source, uint64_t size)
{
   if (!destination || !source) return false;
   uint8_t       *out = (uint8_t *)destination;
   const uint8_t *in  = (const uint8_t *)source;
   for (uint64_t index = 0; index < size; ++index) out[index] = in[index];
   return true;
}

static bool IsSafeBufferUri(const char *uri)
{
   if (!uri || !uri[0] || uri[0] == '/' || uri[0] == '\\') return false;
   for (const char *cursor = uri; *cursor; ++cursor) {
      if (*cursor == ':' || *cursor == '/' || *cursor == '\\') return false;
      if (cursor[0] == '.' && cursor[1] == '.') return false;
   }
   return true;
}

static cgltf_result GltfReadFile(const cgltf_memory_options *, const cgltf_file_options *,
                                 const char *path, cgltf_size *size, void **data)
{
   if (!path || !size || !data) return cgltf_result_invalid_options;
   *data = nullptr;
   *size = 0;

   FileData file = {};
   if (!LoadFile(path, &file) || !file.data || file.length == 0) {
      if (file.data) CloseFile(&file);
      return cgltf_result_file_not_found;
   }
   uint64_t length = file.length;
   void *copy = Alloc(length);
   if (!copy || !CopyBytes(copy, file.data, length)) {
      if (copy) Free(copy);
      CloseFile(&file);
      return cgltf_result_out_of_memory;
   }
   CloseFile(&file);
   *data = copy;
   *size = (cgltf_size)length;
   return cgltf_result_success;
}

static void GltfReleaseFile(const cgltf_memory_options *, const cgltf_file_options *, void *data)
{
   Free(data);
}

static const cgltf_attribute *FindAttribute(const cgltf_primitive &primitive,
                                            cgltf_attribute_type type, int index)
{
   for (cgltf_size attributeIndex = 0; attributeIndex < primitive.attributes_count;
        ++attributeIndex) {
      const cgltf_attribute &attribute = primitive.attributes[attributeIndex];
      if (attribute.type == type && attribute.index == index) return &attribute;
   }
   return nullptr;
}

static bool IsPositionAccessor(const cgltf_accessor *accessor)
{
   return accessor && accessor->buffer_view && !accessor->is_sparse &&
          accessor->component_type == cgltf_component_type_r_32f && accessor->type == cgltf_type_vec3 &&
          accessor->count > 0;
}

static bool IsUvAccessor(const cgltf_accessor *accessor)
{
   return accessor && accessor->buffer_view && !accessor->is_sparse &&
          accessor->component_type == cgltf_component_type_r_32f && accessor->type == cgltf_type_vec2;
}

static bool IsIndexAccessor(const cgltf_accessor *accessor)
{
   return accessor && accessor->buffer_view && !accessor->is_sparse &&
          accessor->type == cgltf_type_scalar &&
          (accessor->component_type == cgltf_component_type_r_8u ||
           accessor->component_type == cgltf_component_type_r_16u ||
           accessor->component_type == cgltf_component_type_r_32u);
}

static bool IsSupportedDocument(const cgltf_data *data)
{
   if (!data || data->buffers_count != 1 || data->meshes_count != 1 ||
       data->meshes[0].primitives_count != 1) {
      return false;
   }
   const cgltf_primitive &primitive = data->meshes[0].primitives[0];
   if (primitive.type != cgltf_primitive_type_triangles) return false;
   const cgltf_attribute *position = FindAttribute(primitive, cgltf_attribute_type_position, 0);
   const cgltf_attribute *uv       = FindAttribute(primitive, cgltf_attribute_type_texcoord, 0);
   if (!position || !IsPositionAccessor(position->data)) return false;
   if (uv && !IsUvAccessor(uv->data)) return false;
   if (primitive.indices && !IsIndexAccessor(primitive.indices)) return false;
   cgltf_size outputCount = primitive.indices ? primitive.indices->count : position->data->count;
   return outputCount > 0 && outputCount % 3 == 0 && outputCount <= 16777215U;
}

static bool ReadVertex(const cgltf_accessor *positions, const cgltf_accessor *uvs, cgltf_size index,
                       float3 *position, float2 *uv)
{
   float positionValues[3] = {};
   if (!cgltf_accessor_read_float(positions, index, positionValues, 3)) return false;
   *position = {positionValues[0], positionValues[1], positionValues[2]};
   *uv       = {0.0f, 0.0f};
   if (!uvs) return true;
   float uvValues[2] = {};
   if (!cgltf_accessor_read_float(uvs, index, uvValues, 2)) return false;
   *uv = {uvValues[0], 1.0f - uvValues[1]};
   return true;
}
} // namespace

Mesh Mesh::LoadGLTF(const char *filename)
{
   if (!filename || !filename[0]) return Mesh();

   FileData file = {};
   if (!LoadFile(filename, &file) || !file.data || file.length < 4) {
      if (file.data) CloseFile(&file);
      return Mesh();
   }

   cgltf_options options = {};
   options.memory.alloc_func = GltfAlloc;
   options.memory.free_func  = GltfFree;
   options.file.read         = GltfReadFile;
   options.file.release      = GltfReleaseFile;

   cgltf_data *document = nullptr;
   cgltf_result result = cgltf_parse(&options, file.data, (cgltf_size)file.length, &document);
   if (result != cgltf_result_success || !document || !IsSupportedDocument(document)) {
      if (document) cgltf_free(document);
      CloseFile(&file);
      return Mesh();
   }

   if (document->buffers[0].uri && !IsSafeBufferUri(document->buffers[0].uri)) {
      cgltf_free(document);
      CloseFile(&file);
      return Mesh();
   }
   result = cgltf_load_buffers(&options, document, filename);
   if (result != cgltf_result_success || cgltf_validate(document) != cgltf_result_success) {
      cgltf_free(document);
      CloseFile(&file);
      return Mesh();
   }

   const cgltf_primitive &primitive = document->meshes[0].primitives[0];
   const cgltf_accessor *positions = FindAttribute(primitive, cgltf_attribute_type_position, 0)->data;
   const cgltf_attribute *uv        = FindAttribute(primitive, cgltf_attribute_type_texcoord, 0);
   const cgltf_accessor *uvs        = uv ? uv->data : nullptr;
   cgltf_size outputCount = primitive.indices ? primitive.indices->count : positions->count;
   Mesh mesh = Mesh::Create((int32_t)outputCount);
   if (!mesh.IsValid()) {
      cgltf_free(document);
      CloseFile(&file);
      return Mesh();
   }

   bool valid = true;
   for (cgltf_size outputIndex = 0; outputIndex < outputCount; ++outputIndex) {
      cgltf_size vertexIndex = primitive.indices
                                    ? cgltf_accessor_read_index(primitive.indices, outputIndex)
                                    : outputIndex;
      if (vertexIndex >= positions->count || !ReadVertex(positions, uvs, vertexIndex,
                                                          &mesh.m_vertices[outputIndex],
                                                          &mesh.m_uvs[outputIndex])) {
         valid = false;
         break;
      }
   }
   if (valid) mesh.RecalculateTriangleNormals();
   cgltf_free(document);
   CloseFile(&file);
   if (!valid) return Mesh();
   return mesh;
}
} // namespace HE3D
