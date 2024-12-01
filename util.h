#pragma once

#define abs_int(value) ((value) < 0 ? -(value) : (value))
#define clip_0(value) ((value) < 0 ? 0 : (value))
#define clip_8(value) ((value) > 255 ? 255 : (value))
#define clip_16(value) ((value) > 65535 ? 65535 : (value))