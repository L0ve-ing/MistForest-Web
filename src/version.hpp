#pragma once
#define MIST_VERSION_LABEL "1.1.8"
#define MIST_VERSION_NUMBERS 1, 1, 8, 0
#define MIST_PROTOCOL_VERSION 10108
#define MIST_WIDEN_IMPL(value) L##value
#define MIST_WIDEN(value) MIST_WIDEN_IMPL(value)
#ifndef RC_INVOKED
namespace mist {
inline constexpr wchar_t VersionText[] = MIST_WIDEN(MIST_VERSION_LABEL);
inline constexpr wchar_t WindowClass[] = L"MistForestV" MIST_WIDEN(MIST_VERSION_LABEL);
inline constexpr int ProtocolVersion = MIST_PROTOCOL_VERSION;
}
#endif
