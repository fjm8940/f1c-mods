/* Generated guarded hook declarations. Do not edit. */
#pragma once
#include "f1c_hook.h"
#include "f1c_reviewed_api.hpp"

#if defined(_MSC_VER)
#define F1C_GEN_THISCALL __thiscall
#define F1C_GEN_STDCALL __stdcall
#elif defined(__GNUC__) && defined(__i386__)
#define F1C_GEN_THISCALL __attribute__((thiscall))
#define F1C_GEN_STDCALL __attribute__((stdcall))
#else
#define F1C_GEN_THISCALL
#define F1C_GEN_STDCALL
#endif

namespace esc_pause_menu { namespace hooks {

}} // namespace

#undef F1C_GEN_THISCALL
#undef F1C_GEN_STDCALL
