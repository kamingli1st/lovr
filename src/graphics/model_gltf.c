#include "graphics/model.h"
#include "graphics/graphics.h"
#include "resources/shaders.h"

gltfModel* lovrModelInitGltf(gltfModel* model, gltfModelData* data) {
  model->data = data;
  lovrRetain(data);

  if (data->bufferViewCount > 0) {
    model->buffers = calloc(data->bufferViewCount, sizeof(Buffer*));
    for (int i = 0; i < data->bufferViewCount; i++) {
      gltfBufferView* view = &data->bufferViews[i];
      gltfBuffer* buffer = &data->buffers[view->buffer];
      model->buffers[i] = lovrBufferCreate(view->length, (uint8_t*) buffer->data + view->offset, BUFFER_GENERIC, USAGE_STATIC, false);
    }
  }

  if (data->primitiveCount > 0) {
    model->meshes = calloc(data->primitiveCount, sizeof(Mesh*));
    for (int i = 0; i < data->primitiveCount; i++) {
      gltfPrimitive* primitive = &data->primitives[i];
      model->meshes[i] = lovrMeshCreateEmpty(primitive->mode);

      bool setDrawRange = false;
      for (int j = 0; j < MAX_DEFAULT_ATTRIBUTES; j++) {
        if (primitive->attributes[j] >= 0) {
          gltfAccessor* accessor = &data->accessors[primitive->attributes[j]];

          lovrMeshAttachAttribute(model->meshes[i], lovrShaderAttributeNames[j], &(MeshAttribute) {
            .buffer = model->buffers[accessor->bufferView],
            .offset = accessor->offset,
            .stride = data->bufferViews[accessor->bufferView].stride,
            .type = accessor->type == F32 ? ATTR_FLOAT : (accessor->type == U8 ? ATTR_BYTE : ATTR_INT),
            .components = accessor->components,
            .enabled = true
          });

          if (!setDrawRange && primitive->indices == -1) {
            lovrMeshSetDrawRange(model->meshes[i], 0, accessor->count);
            setDrawRange = true;
          }
        }
      }

      lovrMeshAttachAttribute(model->meshes[i], "lovrDrawID", &(MeshAttribute) {
        .buffer = lovrGraphicsGetIdentityBuffer(),
        .type = ATTR_BYTE,
        .components = 1,
        .divisor = 1,
        .integer = true,
        .enabled = true
      });

      if (primitive->indices >= 0) {
        gltfAccessor* accessor = &data->accessors[primitive->indices];
        lovrMeshSetIndexBuffer(model->meshes[i], model->buffers[accessor->bufferView], accessor->count, accessor->type == U16 ? 2 : 4);
      }
    }
  }

  return model;
}

void lovrgltfModelDestroy(void* ref) {
  gltfModel* model = ref;
  for (int i = 0; i < model->data->bufferViewCount; i++) {
    lovrRelease(model->buffers[i]);
  }
  for (int i = 0; i < model->data->primitiveCount; i++) {
    lovrRelease(model->meshes[i]);
  }
  lovrRelease(model->data);
  free(ref);
}

void lovrGltfModelDraw(gltfModel* model, mat4 transform, int instances) {
  //
}
