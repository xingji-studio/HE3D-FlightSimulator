#pragma once

#include "he3d_platform.hpp"

namespace HE3D {

struct PlatformKeyTracker {
    int32_t keys[128];
    int32_t count;
};

static inline void PlatformKeyTrackerInit(PlatformKeyTracker *tracker) {
    if (!tracker) {
        return;
    }
    tracker->count = 0;
}

static inline int32_t PlatformNormalizeAsciiKey(int32_t key) {
    if (key >= 'A' && key <= 'Z') {
        return key + ('a' - 'A');
    }
    return key;
}

static inline void PlatformRememberPressedKey(PlatformKeyTracker *tracker, int32_t key) {
    if (!tracker) {
        return;
    }
    for (int32_t i = 0; i < tracker->count; i++) {
        if (tracker->keys[i] == key) {
            return;
        }
    }
    if (tracker->count < (int32_t)(sizeof(tracker->keys) / sizeof(tracker->keys[0]))) {
        tracker->keys[tracker->count++] = key;
    }
}

static inline void PlatformForgetPressedKey(PlatformKeyTracker *tracker, int32_t key) {
    if (!tracker) {
        return;
    }
    for (int32_t i = 0; i < tracker->count;) {
        if (tracker->keys[i] == key) {
            tracker->keys[i] = tracker->keys[--tracker->count];
            continue;
        }
        i++;
    }
}

static inline void PlatformReleasePressedKeys(PlatformKeyTracker *tracker,
                                              KeyCallback callback,
                                              void *user) {
    if (!tracker) {
        return;
    }
    if (callback) {
        for (int32_t i = 0; i < tracker->count; i++) {
            callback(tracker->keys[i], false, user);
        }
    }
    tracker->count = 0;
}

} // namespace HE3D
