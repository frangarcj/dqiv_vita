// game/stub.hpp -- marker for header-first stubs: a stub body logs ONCE (per call site) and returns a neutral value.
// Work packages replace the body in their own .cpp; the declaration (the public API consumed by other WPs) stays.
#pragma once
#include "utils/logger.h"
#define DQIV_STUB() do { static bool dqiv_once_; if (!dqiv_once_) { dqiv_once_ = true; l_warn("STUB %s", __PRETTY_FUNCTION__); } } while (0)
