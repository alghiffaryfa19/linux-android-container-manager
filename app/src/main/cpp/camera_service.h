#pragma once
#include "display_consumer.h"

static inline int camera_service_is_ready(void) { return 0; }
static inline void camera_release_client(void* s) {}
static inline void camera_set_focus(void* s) {}
static inline struct resources camera_allocate_resource(uint32_t* args, void* userdata) { 
    struct resources r = {0}; 
    return r; 
}
static inline void camera_free_resource(struct resources res, void* userdata) {}
