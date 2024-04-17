#pragma once

#include <stdint.h>

// IPC Commands
enum IPCCommands : uint32_t {
    IPC_CMD_NOOP,
    IPC_CMD_MESSAGE_TRAY_OPEN,
    IPC_CMD_VOLUME_UP,
    IPC_CMD_VOLUME_DOWN,
    IPC_CMD_VOLUME_SET,
    IPC_CMD_VOLUME_MUTE,
    IPC_CMD_BRIGHTNESS_UP,
    IPC_CMD_BRIGHTNESS_DOWN,
	IPC_CMD_THEME_DARK,
	IPC_CMD_THEME_LIGHT,
	IPC_CMD_DUMP_DARK_THEME,
	IPC_CMD_DUMP_LIGHT_THEME,
    IPC_CMD_ACTIVITIES_SHOW,
    IPC_CMD_ACTIVITIES_HIDE,
    IPC_CMD_ACTIVITIES_TOGGLE,
    IPC_CMD_APP_SWITCHER_SHOW,
    IPC_CMD_APP_SWITCHER_HIDE,
    IPC_CMD_APP_SWITCHER_TOGGLE,

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

typedef struct _IPCVolumeMute {
    IPCHeader header;
} IPCVolumeMute;

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

typedef struct _IPCDumpDarkTheme {
	IPCHeader header;
} IPCDumpDarkTheme;

typedef struct _IPCDumpLightTheme {
	IPCHeader header;
} IPCDumpLightTheme;

typedef struct _IPCActivitiesShow {
    IPCHeader header;
} IPCActivitiesShow;

typedef struct _IPCActivitiesHide {
    IPCHeader header;
} IPCActivitiesHide;

typedef struct _IPCActivitiesToggle {
    IPCHeader header;
} IPCActivitiesToggle;

typedef struct _IPCAppSwitcherShow {
    IPCHeader header;
} IPCAppSwitcherShow;

typedef struct _IPCAppSwitcherHide {
    IPCHeader header;
} IPCAppSwitcherHide;

typedef struct _IPCAppSwitcherToggle {
    IPCHeader header;
} IPCAppSwitcherToggle;
