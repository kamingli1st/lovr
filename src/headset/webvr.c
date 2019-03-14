#include "headset/headset.h"
#include "event/event.h"
#include "graphics/graphics.h"
#include "graphics/canvas.h"
#include "resources/shaders.h"
#include "lib/vec/vec.h"
#include <stdbool.h>

// Provided by resources/lovr.js
extern bool webvrInit(float offset, void (*added)(uint32_t id), void (*removed)(uint32_t id), void (*pressed)(uint32_t, ControllerButton button), void (*released)(uint32_t id, ControllerButton button), void (*mount)(bool mounted));
extern void webvrDestroy(void);
extern HeadsetType webvrGetType(void);
extern HeadsetOrigin webvrGetOriginType(void);
extern bool webvrIsMounted(void);
extern void webvrGetDisplayDimensions(uint32_t* width, uint32_t* height);
extern void webvrGetClipDistance(float* near, float* far);
extern void webvrSetClipDistance(float near, float far);
extern void webvrGetBoundsDimensions(float* width, float* depth);
extern const float* webvrGetBoundsGeometry(int* count);
extern void webvrGetPose(float* x, float* y, float* z, float* angle, float* ax, float* ay, float* az);
extern void webvrGetVelocity(float* vx, float* vy, float* vz);
extern void webvrGetAngularVelocity(float* vx, float* vy, float* vz);
extern bool webvrControllerIsConnected(Controller* controller);
extern ControllerHand webvrControllerGetHand(Controller* controller);
extern void webvrControllerGetPose(Controller* controller, float* x, float* y, float* z, float* angle, float* ax, float* ay, float* az);
extern void webvrControllerGetVelocity(Controller* controller, float* vx, float* vy, float* vz);
extern void webvrControllerGetAngularVelocity(Controller* controller, float* vx, float* vy, float* vz);
extern float webvrControllerGetAxis(Controller* controller, ControllerAxis axis);
extern bool webvrControllerIsDown(Controller* controller, ControllerButton button);
extern bool webvrControllerIsTouched(Controller* controller, ControllerButton button);
extern void webvrControllerVibrate(Controller* controller, float duration, float power);
extern ModelData* webvrControllerNewModelData(Controller* controller);
extern void webvrSetRenderCallback(void (*callback)(float*, float*, float*, float*, void*), void* userdata);
extern void webvrUpdate(float dt);

typedef struct {
  vec_controller_t controllers;
  void (*renderCallback)(void*);
  Canvas* canvas;
  Canvas* blitCanvas;
  Shader* blitShader;
} HeadsetState;

static HeadsetState state;

static void onControllerAdded(uint32_t id) {
  Controller* controller = lovrAlloc(Controller);
  controller->id = id;
  vec_push(&state.controllers, controller);
  lovrRetain(controller);
  lovrEventPush((Event) {
    .type = EVENT_CONTROLLER_ADDED,
    .data.controller = { controller, 0 }
  });
}

static void onControllerRemoved(uint32_t id) {
  for (int i = 0; i < state.controllers.length; i++) {
    if (state.controllers.data[i]->id == id) {
      Controller* controller = state.controllers.data[i];
      lovrRetain(controller);
      lovrEventPush((Event) {
        .type = EVENT_CONTROLLER_REMOVED,
        .data.controller = { controller, 0 }
      });
      vec_splice(&state.controllers, i, 1);
      lovrRelease(Controller, controller);
      break;
    }
  }
}

static void onControllerPressed(uint32_t id, ControllerButton button) {
  lovrEventPush((Event) {
    .type = EVENT_CONTROLLER_PRESSED,
    .data.controller = { state.controllers.data[id], button }
  });
}

static void onControllerReleased(uint32_t id, ControllerButton button) {
  lovrEventPush((Event) {
    .type = EVENT_CONTROLLER_RELEASED,
    .data.controller = { state.controllers.data[id], button }
  });
}

static void onMountChanged(bool mounted) {
  lovrEventPush((Event) {
    .type = EVENT_MOUNT,
    .data.boolean = { mounted }
  });
}

static void onFrame(float* leftView, float* rightView, float* leftProjection, float* rightProjection, void* userdata) {
  Camera camera = { .canvas = NULL, .stereo = true };
  memcpy(camera.projection[0], leftProjection, 16 * sizeof(float));
  memcpy(camera.projection[1], rightProjection, 16 * sizeof(float));
  memcpy(camera.viewMatrix[0], leftView, 16 * sizeof(float));
  memcpy(camera.viewMatrix[1], rightView, 16 * sizeof(float));

  if (lovrGraphicsGetFeatures()->multiview) {
    uint32_t width, height;
    webvrGetDisplayDimensions(&width, &height);

    if (!state.canvas) {
      Texture* texture = lovrTextureCreate(TEXTURE_ARRAY, NULL, 0, false, false, 0);
      lovrTextureAllocate(texture, width, height, 2, FORMAT_RGBA);
      lovrTextureSetFilter(texture, lovrGraphicsGetDefaultFilter());

      CanvasFlags flags = { .multiview = lovrGraphicsGetFeatures()->multiview };
      state.canvas = lovrCanvasCreate(width, height, flags);
      lovrCanvasSetAttachments(state.canvas, &(Attachment) { texture, 0, 0 }, 1);

      state.blitCanvas = lovrCanvasCreate();
      lovrCanvasSetAttachments(state.blitCanvas, (Attachments[]) { { texture, 0, 0 }, { texture, 1, 0 } }, 2);

      lovrRelease(texture);

      const char* vertexSource = lovrFillVertexShader;
      const char* fragmentSource = ""
        "uniform sampler2DArray lovrMultiviewTexture; \n"
        "vec4 color(vec4 graphicsColor, sampler2D image, vec2 uv) { \n"
        "  return texture(lovrMultiviewTexture, vec3(uv.x * 2., uv.y, round(uv.x))); \n"
        "}";

      state.blitShader = lovrShaderCreate(vertexSource, fragmentSource);
      lovrShaderSetTextures(state.blitShader, "lovrMultiviewTexture", &texture, 0, 1);
    }

    camera.canvas = state.canvas;
  }

  lovrGraphicsSetCamera(&camera, true);
  state.renderCallback(userdata);
  lovrGraphicsSetCamera(NULL, false);

  if (lovrGraphicsGetFeatures()->multiview) {
    Shader* shader = lovrGraphicsGetShader();
    lovrGraphicsSetShader(state.blitShader);
    lovrGraphicsFill(NULL, 0.f, 0.f, 1.f, 1.f);
    lovrGraphicsSetShader(shader);
  }
}

static bool webvrDriverInit(float offset, int msaa) {
  vec_init(&state.controllers);

  if (webvrInit(offset, onControllerAdded, onControllerRemoved, onControllerPressed, onControllerReleased, onMountChanged)) {
    state.renderCallback = NULL;
    return true;
  } else {
    return false;
  }
}

static void webvrDriverDestroy() {
  webvrDestroy();
  vec_deinit(&state.controllers);
  lovrRelease(state.canvas);
  lovrRelease(state.blitCanvas);
  lovrRelease(state.blitShader);
  memset(&state, 0, sizeof(HeadsetState));
}

Controller** webvrGetControllers(uint8_t* count) {
  *count = state.controllers.length;
  return state.controllers.data;
}

void webvrRenderTo(void (*callback)(void*), void* userdata) {
  state.renderCallback = callback;
  webvrSetRenderCallback(onFrame, userdata);
}

HeadsetInterface lovrHeadsetWebVRDriver = {
  DRIVER_WEBVR,
  webvrDriverInit,
  webvrDriverDestroy,
  webvrGetType,
  webvrGetOriginType,
  webvrIsMounted,
  webvrGetDisplayDimensions,
  webvrGetClipDistance,
  webvrSetClipDistance,
  webvrGetBoundsDimensions,
  webvrGetBoundsGeometry,
  webvrGetPose,
  webvrGetVelocity,
  webvrGetAngularVelocity,
  webvrGetControllers,
  webvrControllerIsConnected,
  webvrControllerGetHand,
  webvrControllerGetPose,
  webvrControllerGetVelocity,
  webvrControllerGetAngularVelocity,
  webvrControllerGetAxis,
  webvrControllerIsDown,
  webvrControllerIsTouched,
  webvrControllerVibrate,
  webvrControllerNewModelData,
  webvrRenderTo,
  NULL, // No mirror texture
  webvrUpdate
};
