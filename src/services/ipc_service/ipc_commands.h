#pragma once

#include <stdint.h>

// IPC Commands
enum IPCCommands : uint32_t {
    IPC_CMD_NOOP,
    IPC_CMD_MESSAGE_TRAY_OPEN,
    IPC_CMD_VOLUME_UP,
    IPC_CMD_VOLUME_DOWN,
    IPC_CMD_VOLUME_SET,
};

typedef struct _IPCHeader {
    enum IPCCommands type;
} IPCHeader;

typedef struct _IPCMessageTrayOpen {
    IPCHeader header;
} IPCMessageTrayOpen;

typedef struct _IPCVolumeUp {
    IPCHeader header;
} IPCVolumeUp;

typedef struct _IPCVolumeDown {
    IPCHeader header;
} IPCVolumeDown;

typedef struct _IPCVolumeSet {
    IPCHeader header;
    float volume;
} IPCVolumeSet;
