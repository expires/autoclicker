#pragma once

#if MNC_LEGACY
inline constexpr bool kNativeDropOverride = false;
#else
inline constexpr bool kNativeDropOverride = true;
#endif
