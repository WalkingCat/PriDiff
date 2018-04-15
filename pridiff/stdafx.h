#pragma once

#include "targetver.h"

#include <stdio.h>
#include <tchar.h>
#include <functional>

#include "..\witutils\find_files.h"
#include "..\witutils\find_files_wcs.h"

#define WIN32_MEAN_AND_LEAN
#include <Windows.h>

#include <Shlwapi.h>
#pragma comment(lib, "shlwapi.lib")