#include "graphics/animator.h"
#include "data/modelData.h"
#include "core/maf.h"
#include "types.h"
#include <stdlib.h>
#include <math.h>

static int trackSortCallback(const void* a, const void* b) {
  return ((Track*) a)->priority < ((Track*) b)->priority;
}

Animator* lovrAnimatorInit(Animator* animator, ModelData* data) {
  lovrRetain(data);
  animator->data = data;
  map_init(&animator->animations);
  vec_init(&animator->tracks);
  vec_reserve(&animator->tracks, data->animationCount);
  animator->speed = 1.f;

  for (uint32_t i = 0; i < data->animationCount; i++) {
    vec_push(&animator->tracks, ((Track) {
      .time = 0.f,
      .speed = 1.f,
      .alpha = 1.f,
      .priority = 0,
      .playing = false,
      .looping = false
    }));

    if (data->animations[i].name) {
      map_set(&animator->animations, data->animations[i].name, i);
    }
  }

  return animator;
}

void lovrAnimatorDestroy(void* ref) {
  Animator* animator = ref;
  lovrRelease(ModelData, animator->data);
  vec_deinit(&animator->tracks);
}

void lovrAnimatorReset(Animator* animator) {
  Track* track; int i;
  vec_foreach_ptr(&animator->tracks, track, i) {
    track->time = 0.f;
    track->speed = 1.f;
    track->playing = false;
    track->looping = false;
  }
  animator->speed = 1.f;
}

void lovrAnimatorUpdate(Animator* animator, float dt) {
  Track* track; int i;
  vec_foreach_ptr(&animator->tracks, track, i) {
    if (track->playing) {
      track->time += dt * track->speed * animator->speed;
      float duration = animator->data->animations[i].duration;

      if (track->looping) {
        track->time = fmodf(track->time, duration);
      } else if (track->time > duration || track->time < 0) {
        track->time = 0;
        track->playing = false;
      }
    }
  }
}

bool lovrAnimatorEvaluate(Animator* animator, uint32_t nodeIndex, mat4 transform) {
  float properties[3][4];
  ModelNode* node = &animator->data->nodes[nodeIndex];
  vec3_init(properties[PROP_TRANSLATION], node->translation);
  quat_init(properties[PROP_ROTATION], node->rotation);
  vec3_init(properties[PROP_SCALE], node->scale);
  bool touched = false;

  for (uint32_t i = 0; i < animator->data->animationCount; i++) {
    ModelAnimation* animation = &animator->data->animations[i];

    for (uint32_t j = 0; j < animation->channelCount; j++) {
      ModelAnimationChannel* channel = &animation->channels[j];
      if (channel->nodeIndex != nodeIndex) {
        continue;
      }

      Track* track = &animator->tracks.data[i];
      if (!track->playing || track->alpha == 0.f) {
        continue;
      }

      float duration = animator->data->animations[i].duration;
      float time = fmodf(track->time, duration);
      uint32_t k = 0;

      while (k < channel->keyframeCount && channel->times[k] < time) {
        k++;
      }

      float value[4];
      bool rotate = channel->property == PROP_ROTATION;
      size_t n = 3 + rotate;
      float* (*lerp)(float* a, float* b, float t) = rotate ? quat_slerp : vec3_lerp;

      if (k < channel->keyframeCount) {
        float t1 = channel->times[k - 1];
        float t2 = channel->times[k];
        float z = (time - t1) / (t2 - t1);
        float next[4];

        memcpy(value, channel->data + (k - 1) * n, n * sizeof(float));
        memcpy(next, channel->data + k * n, n * sizeof(float));

        switch (channel->smoothing) {
          case SMOOTH_STEP:
            if (z >= .5f) {
              memcpy(value, next, n * sizeof(float));
            }
            break;
          case SMOOTH_LINEAR: lerp(value, next, z); break;
          case SMOOTH_CUBIC: lovrThrow("Cubic spline interpolation is not supported yet"); break;
          default: break;
        }
      } else {
        memcpy(value, channel->data + CLAMP(k, 0, channel->keyframeCount - 1) * n, n * sizeof(float));
      }

      if (track->alpha == 1.f) {
        memcpy(properties[channel->property], value, n * sizeof(float));
      } else {
        lerp(properties[channel->property], value, track->alpha);
      }

      touched = true;
    }
  }

  if (touched) {
    vec3 T = properties[PROP_TRANSLATION];
    quat R = properties[PROP_ROTATION];
    vec3 S = properties[PROP_SCALE];
    mat4_translate(transform, T[0], T[1], T[2]);
    mat4_rotateQuat(transform, R);
    mat4_scale(transform, S[0], S[1], S[2]);
  } else {
    mat4_multiply(transform, node->transform);
  }

  return touched;
}

uint32_t lovrAnimatorGetAnimationCount(Animator* animator) {
  return animator->data->animationCount;
}

uint32_t* lovrAnimatorGetAnimationIndex(Animator* animator, const char* name) {
  return map_get(&animator->animations, name);
}

const char* lovrAnimatorGetAnimationName(Animator* animator, uint32_t index) {
  return animator->data->animations[index].name;
}

void lovrAnimatorPlay(Animator* animator, uint32_t animation) {
  Track* track = &animator->tracks.data[animation];
  track->playing = true;
  track->time = 0.f;
}

void lovrAnimatorStop(Animator* animator, uint32_t animation) {
  Track* track = &animator->tracks.data[animation];
  track->playing = false;
  track->time = 0.f;
}

void lovrAnimatorPause(Animator* animator, uint32_t animation) {
  Track* track = &animator->tracks.data[animation];
  track->playing = false;
}

void lovrAnimatorResume(Animator* animator, uint32_t animation) {
  Track* track = &animator->tracks.data[animation];
  track->playing = true;
}

void lovrAnimatorSeek(Animator* animator, uint32_t animation, float time) {
  Track* track = &animator->tracks.data[animation];
  float duration = animator->data->animations[animation].duration;

  while (time > duration) {
    time -= duration;
  }

  while (time < 0.f) {
    time += duration;
  }

  track->time = time;

  if (!track->looping) {
    track->time = MIN(track->time, duration);
    track->time = MAX(track->time, 0);
  }
}

float lovrAnimatorTell(Animator* animator, uint32_t animation) {
  Track* track = &animator->tracks.data[animation];
  return track->time;
}

float lovrAnimatorGetAlpha(Animator* animator, uint32_t animation) {
  Track* track = &animator->tracks.data[animation];
  return track->alpha;
}

void lovrAnimatorSetAlpha(Animator* animator, uint32_t animation, float alpha) {
  Track* track = &animator->tracks.data[animation];
  track->alpha = alpha;
}

float lovrAnimatorGetDuration(Animator* animator, uint32_t animation) {
  return animator->data->animations[animation].duration;
}

bool lovrAnimatorIsPlaying(Animator* animator, uint32_t animation) {
  Track* track = &animator->tracks.data[animation];
  return track->playing;
}

bool lovrAnimatorIsLooping(Animator* animator, uint32_t animation) {
  Track* track = &animator->tracks.data[animation];
  return track->looping;
}

void lovrAnimatorSetLooping(Animator* animator, uint32_t animation, bool loop) {
  Track* track = &animator->tracks.data[animation];
  track->looping = loop;
}

int32_t lovrAnimatorGetPriority(Animator* animator, uint32_t animation) {
  Track* track = &animator->tracks.data[animation];
  return track->priority;
}

void lovrAnimatorSetPriority(Animator* animator, uint32_t animation, int32_t priority) {
  Track* track = &animator->tracks.data[animation];
  track->priority = priority;
  vec_sort(&animator->tracks, trackSortCallback);
}

float lovrAnimatorGetSpeed(Animator* animator, uint32_t animation) {
  if (animation == ~0u) {
    return animator->speed;
  }

  Track* track = &animator->tracks.data[animation];
  return track->speed;
}

void lovrAnimatorSetSpeed(Animator* animator, uint32_t animation, float speed) {
  if (animation == ~0u) {
    animator->speed = speed;
  }

  Track* track = &animator->tracks.data[animation];
  track->speed = speed;
}
