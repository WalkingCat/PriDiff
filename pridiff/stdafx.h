#pragma once

#include "targetver.h"

#include <stdio.h>
#include <tchar.h>
#include <functional>

#include "../witutils/diff_utils.h"
#include "../witutils/diff_commons.h"

#define WIN32_MEAN_AND_LEAN
#include <Windows.h>

#include <Shlwapi.h>
#pragma comment(lib, "shlwapi.lib")