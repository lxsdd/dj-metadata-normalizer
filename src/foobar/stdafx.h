#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef STRICT
#define STRICT
#endif
#define _WIN32_WINNT 0x0601

#include <windows.h>
#include <shlobj.h>

#include <SDK/foobar2000.h>

#include "djmeta/normalizer.h"
#include "djmeta/rule_loader.h"

#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
