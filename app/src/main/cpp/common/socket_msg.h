#pragma once
#include <stdint.h>

enum DisplayCmd {
    CMD_CREATE_DISPLAY = 1,
    CMD_SET_BUFFER = 2,
    CMD_DESTROY_DISPLAY = 3
};

struct DisplayMsg {
    int32_t cmd;
    int64_t displayId;
    uint32_t width;
    uint32_t height;
    uint32_t stride;
    uint32_t format;
};
