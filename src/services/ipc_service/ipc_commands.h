#pragma once

#include <stdint.h>

// IPC Commands
enum IPCCommands : uint32_t {
    IPC_CMD_NOOP,
    IPC_CMD_MESSAGE_TRAY_OPEN,
    IPC_CMD_VOLUME_UP,
    IPC_CMD_VOLUME_DOWN,
    IPC_CMD_VOLUME_SET,
    IPC_CMD_BRIGHTNESS_UP,
    IPC_CMD_BRIGHTNESS_DOWN,
	IPC_CMD_THEME_DARK,
	IPC_CMD_THEME_LIGHT
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

typedef struct _IPCBrightnessUp {
    IPCHeader header;
} IPCBrightnessUp;

typedef struct _IPCBrightnessDown {
    IPCHeader header;
} IPCBrightnessDown;

typedef struct _IPCThemeDark {
	IPCHeader header;
} IPCThemeDark;

typedef struct _IPCThemeLight {
	IPCHeader header;
} IPCThemeLight;
