// Copyright (c) Werner Strydom. All rights reserved.
// Licensed under the MIT license. See LICENSE in the project root for license information.

#include "stdafx.h"
#include "service_config.h"
#include "wrapper-config.h"
#include "wrapper-command.h"
#include "wrapper-memory.h"
#include "wrapper-help.h"

#define EXITCODE_BASE					1
#define EXITCODE_FAILED					(EXITCODE_BASE)
#define EXITCODE_CONFIG_PATH_INVALID	(EXITCODE_BASE + 100)
#define EXITCODE_CONFIG_READ_ERROR		(EXITCODE_BASE + 101)
#define EXITCODE_OUTOFMEMORY			(EXITCODE_BASE * -1)

wrapper_command_t commands[] =
{
	{
		.name = _T("help"),
		.description = _T("Display this usage message."),
		.func = do_help,
	},
	{
		.name = _T("install"),
		.description = _T("Installs the process as a windows service."),
		.func = do_install,
	},
	{
		.name = _T("delete"),
		.description = _T("Deletes the windows service."),
		.func = do_delete,
	},
	{
		.name = _T("enable"),
		.description = _T("Enables the windows service"),
		.func = do_enable,
	},
	{
		.name = _T("disable"),
		.description = _T("Disables the windows service."),
		.func = do_disable,
	},
	{
		.name = _T("start"),
		.description = _T("Starts the windows service."),
		.func = do_start,
	},
	{
		.name = _T("stop"),
		.description = _T("Stops the windows service."),
		.func = do_stop,
	},
	{
		.name = _T("query"),
		.description = _T("Queries the windows service configuration."),
		.func = do_status,
	},
	{
		.name = _T("update"),
		.description = _T("Updates the windows service configuration."),
		.func = do_update,
	},
	{
		.name = NULL,
		.func = NULL
	}
};

int __cdecl _tmain(int argc, TCHAR* argv[])
{
	int exit_code = 0;
	wrapper_error_t* error = NULL;
	wrapper_config_t* config = NULL;

	TCHAR* configuration_path = wrapper_allocate_string(_MAX_PATH);
	if (NULL == configuration_path)
	{
		exit_code = EXITCODE_OUTOFMEMORY;
	}

	if (0 == exit_code)
	{
		if (!wrapper_config_get_path(configuration_path, _MAX_PATH, &error))
		{
			exit_code = EXITCODE_CONFIG_PATH_INVALID;
		}
	}

	if (0 == exit_code)
	{
		config = wrapper_config_alloc();
		if (NULL == config)
		{
			exit_code = EXITCODE_OUTOFMEMORY;
		}
	}

	if (0 == exit_code)
	{
		if (!wrapper_config_read(configuration_path, config, &error))
		{
			exit_code = EXITCODE_CONFIG_READ_ERROR;
		}
	}

	if (0 == exit_code)
	{
		if (argc > 1)
		{
			TCHAR* command_text = argv[1];
			int result = wrapper_command_execute(commands, command_text, config, &error);
			switch (result)
			{
			case 1:
				break;

			case 0:
				_ftprintf(stderr, _T("The command '%s' has failed\n"), command_text);
				exit_code = EXITCODE_FAILED;
				break;

			default:
				_ftprintf(stderr, _T("fatal error: don't know how to '%s'\n"), command_text);
				exit_code = EXITCODE_FAILED;
				break;
			}
		}
		else
		{
			do_run(config, &error);
		}
	}

	if (error)
	{
		_ftprintf(stderr, _T("  Message: %s\n"), error->user_message);
		_ftprintf(stderr, _T("  Code:    %s (0x%08x)\n"), error->message, error->code);
		_ftprintf(stderr, _T("\n"));
	}

	wrapper_free(configuration_path);
	wrapper_error_free(error);
	wrapper_config_free(config);
	return exit_code;
}
