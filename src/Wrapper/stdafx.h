// Copyright (c) Werner Strydom. All rights reserved.
// Licensed under the MIT license. See LICENSE in the project root for license information.

#pragma once

#pragma warning( push, 0 )

#include "targetver.h"

#include <stdio.h>
#include <tchar.h>
#include <windows.h>
#include <strsafe.h>
#include <aclapi.h>

#pragma comment(lib, "advapi32.lib")

#pragma warning( pop )

#define SVCNAME TEXT("MySampleService")