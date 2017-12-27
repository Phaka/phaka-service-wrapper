// Copyright (c) Werner Strydom. All rights reserved.
// Licensed under the MIT license. See LICENSE in the project root for license information.

#include "stdafx.h"
#include "service_config.h"
#include "wrapper-config.h"

#define EXITCODE_BASE					1
#define EXITCODE_CONFIG_PATH_INVALID	(EXITCODE_BASE)
#define EXITCODE_CONFIG_READ_ERROR		(EXITCODE_BASE + 1)
#define EXITCODE_OUTOFMEMORY			(EXITCODE_BASE + 1000)

int __cdecl _tmain(int argc, TCHAR* argv[])
{
	int rc = 0;
	wrapper_error_t* error = NULL;
	wrapper_config_t* config = NULL;
	TCHAR* configuration_path = LocalAlloc(LPTR, _MAX_PATH*sizeof(TCHAR));
	if (NULL == configuration_path)
	{
		rc = EXITCODE_OUTOFMEMORY;
	}

	if (0 == rc)
	{
		if (!wrapper_config_get_path(configuration_path, _MAX_PATH, &error))
		{
			rc = EXITCODE_CONFIG_PATH_INVALID;
		}
	}

	if (0 == rc)
	{
		config = wrapper_config_alloc();
		if (NULL == config)
		{
			rc = EXITCODE_OUTOFMEMORY;
		}
	}

	if (0 == rc)
	{
		if (!wrapper_config_read(configuration_path, config, &error))
		{
			rc = EXITCODE_CONFIG_READ_ERROR;
		}
	}

	if (0 == rc)
	{
		if (argc > 1)
		{
			if (lstrcmpi(argv[1], TEXT("install")) == 0)
			{
				wrapper_service_install(config, &error);
			}
			else if (lstrcmpi(argv[1], TEXT("query")) == 0)
			{
				wrapper_service_query(config, &error);
			}
			else if (lstrcmpi(argv[1], TEXT("describe")) == 0)
			{
				wrapper_service_update(config, &error);
			}
			else if (lstrcmpi(argv[1], TEXT("disable")) == 0)
			{
				wrapper_service_disable(config, &error);
			}
			else if (lstrcmpi(argv[1], TEXT("enable")) == 0)
			{
				wrapper_service_enable(config, &error);
			}
			else if (lstrcmpi(argv[1], TEXT("delete")) == 0)
			{
				wrapper_service_delete(config, &error);
			}
			else if (lstrcmpi(argv[1], TEXT("start")) == 0)
			{
				wrapper_service_start(config, &error);
			}
			else if (lstrcmpi(argv[1], TEXT("dacl")) == 0)
			{
				wrapper_service_dacl(config, &error);
			}
			else if (lstrcmpi(argv[1], TEXT("stop")) == 0)
			{
				wrapper_service_stop(config, &error);
			}
			else
			{
				_ftprintf(stderr, _T("fatal error: don't know how to '%s'"), argv[1]);
				rc = 1;
			}
		}
		else
		{
			wrapper_service_run(config, &error);
		}
	}

	LocalFree(configuration_path);
	wrapper_error_free(error);
	wrapper_config_free(config);
	return rc;
}
