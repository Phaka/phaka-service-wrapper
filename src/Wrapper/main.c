// Copyright (c) Werner Strydom. All rights reserved.
// Licensed under the MIT license. See LICENSE in the project root for license information.

#include "stdafx.h"
#include "wrapper.h"
#include "service.h"
#include "service_config.h"
#include "service_control.h"
#include "wrapper-log.h"
#include "wrapper-error.h"

int __cdecl _tmain(int argc, TCHAR* argv[])
{
	int rc = 0;
	TCHAR service_name[_MAX_PATH];
	GetServiceName(service_name, _MAX_PATH);

	if (argc > 1)
	{
		if (lstrcmpi(argv[1], TEXT("install")) == 0)
		{
			SvcInstall(service_name);
		}
		else if (lstrcmpi(argv[1], TEXT("query")) == 0)
		{
			DoQuerySvc(service_name);
		}
		else if (lstrcmpi(argv[1], TEXT("describe")) == 0)
		{
			DoUpdateSvcDesc(service_name);
		}
		else if (lstrcmpi(argv[1], TEXT("disable")) == 0)
		{
			DoDisableSvc(service_name);
		}
		else if (lstrcmpi(argv[1], TEXT("enable")) == 0)
		{
			DoEnableSvc(service_name);
		}
		else if (lstrcmpi(argv[1], TEXT("delete")) == 0)
		{
			DoDeleteSvc(service_name);
		}
		else if (lstrcmpi(argv[1], TEXT("start")) == 0)
		{
			DoStartSvcEx(service_name);
		}
		else if (lstrcmpi(argv[1], TEXT("dacl")) == 0)
		{
			DoUpdateSvcDaclEx(service_name);
		}
		else if (lstrcmpi(argv[1], TEXT("stop")) == 0)
		{
			DoStopSvcEx(service_name);
		}
		else
		{
			_ftprintf(stderr, _T("fatal error: don't know how to '%s'"), argv[1]);
			rc = 1;
		}
	}
	else
	{
		TCHAR module_path[_MAX_PATH];
		GetModuleFileName(NULL, module_path, _MAX_PATH);
		PathCchRenameExtension(module_path, sizeof(module_path) / sizeof(module_path[0]), _T(".log"));
		wrapper_log_set_handler(wrapper_log_file_handler, module_path);

		WRAPPER_INFO(_T("Starting Service"));
		SERVICE_TABLE_ENTRY DispatchTable[] =
		{
			{service_name, (LPSERVICE_MAIN_FUNCTION)SvcMain},
			{NULL, NULL}
		};

		if (!StartServiceCtrlDispatcher(DispatchTable))
		{
			WRAPPER_ERROR(_T("Failed to start service"));
			SvcReportEvent(TEXT("StartServiceCtrlDispatcher"));
		}
		WRAPPER_INFO(_T("Done"));
	}
	return rc;
}
