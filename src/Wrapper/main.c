// Copyright (c) Werner Strydom. All rights reserved.
// Licensed under the MIT license. See LICENSE in the project root for license information.

#include "stdafx.h"
#include "service_config.h"
#include "wrapper-config.h"
#include "wrapper-command.h"

#define EXITCODE_BASE					1
#define EXITCODE_FAILED					(EXITCODE_BASE)
#define EXITCODE_OUTOFMEMORY			(EXITCODE_BASE + 200)
#define EXITCODE_CONFIG_PATH_INVALID	(EXITCODE_BASE + 100)
#define EXITCODE_CONFIG_READ_ERROR		(EXITCODE_BASE + 101)

int wrapper_help(wrapper_config_t* config, wrapper_error_t** error);


wrapper_command_t commands[] = 
{
	{
		.name = _T("help"),
		.description = _T("Display this usage message."),
		.func = wrapper_help,
	},
	{
		.name = _T("install"),
		.description = _T("Installs the process as a windows service."),
		.func = wrapper_service_install,
	},
	{
		.name = _T("delete"),
		.description = _T("Deletes the windows service."),
		.func = wrapper_service_delete,
	},
	{
		.name = _T("enable"),
		.description = _T("Enables the windows service"),
		.func = wrapper_service_enable,
	},
	{
		.name = _T("disable"),
		.description = _T("Disables the windows service."),
		.func = wrapper_service_disable,
	},
	{
		.name = _T("start"),
		.description = _T("Starts the windows service."),
		.func = wrapper_service_start,
	},
	{
		.name = _T("stop"),
		.description = _T("Stops the windows service."),
		.func = wrapper_service_stop,
	},
	{
		.name = _T("query"),
		.description = _T("Queries the windows service configuration."),
		.func = wrapper_service_query,
	},
	{
		.name = _T("update"),
		.description = _T("Updates the windows service configuration."),
		.func = wrapper_service_update,
	},
	{
		.name = NULL,
		.func = NULL
	}
};

int wrapper_command_get_executable_name(TCHAR* destination, size_t size, wrapper_error_t** error)
{
	if(!GetModuleFileName(NULL, destination, size))
	{
		if (error)
		{
			*error = wrapper_error_from_system(GetLastError(), _T("Unable to get the module path"));
		}
		return 0;
	}

	PathCchRemoveExtension(destination, size);
	PathStripPath(destination);
	CharLower(destination);

	return 1;
}

int wrapper_help(wrapper_config_t* config, wrapper_error_t** error)
{
	int rc = 1;
	TCHAR *name = LocalAlloc(LPTR, _MAX_PATH);
	if (!name)
	{
		if (error)
		{
			*error = wrapper_error_from_hresult(E_OUTOFMEMORY, _T("Failed to allocate memory for the executable name"));
		}
		rc = 0;
	}

	if (rc)
	{
		rc = wrapper_command_get_executable_name(name, _MAX_PATH, error);
	}

	if (rc)
	{
		_ftprintf(stdout, _T("Usage:\n"));
		_ftprintf(stdout, _T("  %s [command]\n"), name);
		_ftprintf(stdout, _T("\n"));
		_ftprintf(stdout, _T("Commands:\n"));
		for (size_t i = 0; commands[i].name != NULL; i++)
		{
			_ftprintf(stdout, _T("  %-15s %s\n"), commands[i].name, commands[i].description);
		}

		_ftprintf(stdout, _T("\n"));
		_ftprintf(stdout, _T("Options:\n"));
		_ftprintf(stdout, _T("  %-15s %s\n"), _T("--nologo"), _T("Suppress the display the startup banner and copyright message."));
		_ftprintf(stdout, _T("\n"));
	}

	LocalFree(name);
	return rc;
}


int wrapper_command_execute(wrapper_command_t* commands, const TCHAR* command_text, wrapper_config_t* config, wrapper_error_t** error)
{
	for (size_t i = 0; commands[i].name != NULL; i++)
	{
		if (lstrcmpi(command_text, commands[i].name) == 0)
		{
			return commands[i].func(config, error);
		}
	}
	return -1;
}

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
			TCHAR* command_text = argv[1];
			int result = wrapper_command_execute(commands, command_text, config, &error);
			switch (result)
			{
			case 1: 
				rc = 0;
				break;

			case 0:
				_ftprintf(stderr, _T("The command '%s' has failed\n"), command_text);
				if (error)
				{
					_ftprintf(stderr, _T("  Message: %s\n"), error->user_message);
					_ftprintf(stderr, _T("  Code:    0x%08x %s\n"), error->code, error->message);
					_ftprintf(stderr, _T(""));
				}
				rc = EXITCODE_FAILED;
				break;

			default:
				_ftprintf(stderr, _T("fatal error: don't know how to '%s'\n"), command_text);
				rc = EXITCODE_FAILED;
				break;
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
