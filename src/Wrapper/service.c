// Copyright (c) Werner Strydom. All rights reserved.
// Licensed under the MIT license. See LICENSE in the project root for license information.

#include "stdafx.h"
#include "wrapper-error.h"
#include "wrapper-memory.h"
#include "wrapper-log.h"
#include "service_config.h"
#include "wrapper-string.h"

VOID WINAPI wrapper_service_main(DWORD dwArgc, LPTSTR* lpszArgv);
VOID WINAPI wrapper_service_control_handler(DWORD dwCtrl);

int wrapper_service_init(wrapper_config_t* config, wrapper_error_t** error);
int wrapper_service_report_status(DWORD dwCurrentState, DWORD dwWin32ExitCode, DWORD dwWaitHint,
                                  wrapper_config_t* config, wrapper_error_t** error);


SERVICE_STATUS_HANDLE status_handle; // TODO: Move to methods and pass around like variables
TCHAR* stop_event_name = _T("PHAKA_WINDOWS_SERVICE_STOP_EVENT");

const TCHAR* wrapper_service_get_status_text(const unsigned long status)
{
	switch (status)
	{
	case SERVICE_STOPPED:
		return _T("SERVICE_STOPPED");
	case SERVICE_START_PENDING:
		return _T("SERVICE_START_PENDING");
	case SERVICE_STOP_PENDING:
		return _T("SERVICE_STOP_PENDING");
	case SERVICE_RUNNING:
		return _T("SERVICE_RUNNING");
	case SERVICE_CONTINUE_PENDING:
		return _T("SERVICE_CONTINUE_PENDING");
	case SERVICE_PAUSE_PENDING:
		return _T("SERVICE_PAUSE_PENDING");
	case SERVICE_PAUSED:
		return _T("SERVICE_PAUSED");
	default:
		return _T("UNKNOWN");
	}
}

// Inspired from http://stackoverflow.com/a/15281070/1529139
// and http://stackoverflow.com/q/40059902/1529139
BOOL SendConsoleCtrlEvent(DWORD dwProcessId, DWORD dwCtrlEvent)
{
	BOOL success = FALSE;
	DWORD thisConsoleId = GetCurrentProcessId();
	// Leave current console if it exists
	// (otherwise AttachConsole will return ERROR_ACCESS_DENIED)
	BOOL consoleDetached = FreeConsole() != FALSE;

	if (AttachConsole(dwProcessId) != FALSE)
	{
		SetConsoleCtrlHandler(NULL, TRUE);
		success = GenerateConsoleCtrlEvent(dwCtrlEvent, 0) != FALSE;
		FreeConsole();
	}

	if (consoleDetached)
	{
		// Create a new console if previous was deleted by OS
		if (AttachConsole(thisConsoleId) == FALSE)
		{
			int errorCode = GetLastError();
			if (errorCode == 31) // 31=ERROR_GEN_FAILURE
			{
				AllocConsole();
			}
		}
	}
	return success;
}

//
// Purpose: 
//   Entry point for the service
//
// Parameters:
//   dwArgc - Number of arguments in the lpszArgv array
//   lpszArgv - Array of strings. The first string is the name of
//     the service and subsequent strings are passed by the process
//     that called the StartService function to start the service.
// 
// Return value:
//   None.
//
VOID WINAPI wrapper_service_main(DWORD dwArgc, LPTSTR* lpszArgv)
{
	HRESULT hr = S_OK;
	wrapper_error_t* error = NULL;
	wrapper_config_t* config = NULL;
	TCHAR* configuration_path = wrapper_allocate_string(_MAX_PATH);
	if (NULL == configuration_path)
	{
		hr = E_OUTOFMEMORY;
	}

	if (SUCCEEDED(hr))
	{
		if (!wrapper_config_get_path(configuration_path, _MAX_PATH, &error))
		{
			wrapper_error_log(error);
			hr = E_FAIL;
		}
	}

	if (SUCCEEDED(hr))
	{
		config = wrapper_config_alloc();
		if (NULL == config)
		{
			hr = E_OUTOFMEMORY;
		}
	}

	TCHAR* service_name = NULL;
	if (SUCCEEDED(hr))
	{
		WRAPPER_INFO(_T("Reading configuration '%s'"), configuration_path);
		if (!wrapper_config_read(configuration_path, config, &error))
		{
			wrapper_error_log(error);
			hr = E_FAIL;
		}
		else
		{
			WRAPPER_INFO(_T("Configuration Settings:"));
			WRAPPER_INFO(_T("  %-20s: %s"), _T("Name"), config->name);
			WRAPPER_INFO(_T("  %-20s: %s"), _T("Title"), config->title);
			WRAPPER_INFO(_T("  %-20s: %s"), _T("Description"), config->description);
			WRAPPER_INFO(_T("  %-20s: %s"), _T("Working Directory"), config->working_directory);
			WRAPPER_INFO(_T("  %-20s: %s"), _T("Command Line"), config->command_line);
			WRAPPER_INFO(_T(""));
			service_name = config->name;
		}
	}

	if (SUCCEEDED(hr))
	{
		WRAPPER_INFO(_T("Register a service control handler for service '%s'"), service_name);
		status_handle = RegisterServiceCtrlHandler(service_name, wrapper_service_control_handler);
		if (!status_handle)
		{
			DWORD last_error = GetLastError();
			error = wrapper_error_from_system(last_error, _T("Failed to register the control handler for service '%s'"),
			                                  service_name);
			wrapper_error_log(error);
			hr = HRESULT_FROM_WIN32(last_error);
		}
		else
		{
			WRAPPER_INFO(_T("Succesfully created a service control handler for service '%s'"), service_name);
		}
	}

	if (SUCCEEDED(hr))
	{

		wrapper_service_init(config, &error);
	}
	else
	{
		wrapper_error_log(error);
		wrapper_service_report_status(SERVICE_STOPPED, NO_ERROR, 0, config, &error);
	}

	wrapper_free(configuration_path);
	wrapper_error_free(error);
	wrapper_config_free(config);
}

HANDLE wrapper_create_child_process(wrapper_config_t* config, wrapper_error_t** error)
{
	HRESULT hr = S_OK;
	STARTUPINFO* startupinfo = NULL;
	PROCESS_INFORMATION* process_information = NULL;

	if (SUCCEEDED(hr))
	{
		process_information = wrapper_allocate(sizeof*process_information);
		if (!process_information)
		{
			hr = E_OUTOFMEMORY;
			if (error)
			{
				*error = wrapper_error_from_hresult(hr, _T("Failed to allocate memory for the process information"));
			}
		}
	}

	if (SUCCEEDED(hr))
	{
		startupinfo = wrapper_allocate(sizeof*startupinfo);
		if (!startupinfo)
		{
			hr = E_OUTOFMEMORY;
			if (error)
			{
				*error = wrapper_error_from_hresult(hr, _T("Failed to allocate memory for the process startup information"));
			}
		}
	}

	TCHAR* command_line = NULL;
	const int command_line_max_size = 32768;
	if (SUCCEEDED(hr))
	{
		command_line = wrapper_allocate_string(command_line_max_size);
		if (!command_line)
		{
			if (error)
			{
				*error = wrapper_error_from_hresult(E_OUTOFMEMORY, _T("Failed to allocate a buffer for the command line"));
			}
			hr = E_OUTOFMEMORY;
		}
	}

	if (SUCCEEDED(hr))
	{
		hr = StringCbCopy(command_line, command_line_max_size, config->command_line);
		if (FAILED(hr))
		{
			if (error)
			{
				*error = wrapper_error_from_hresult(
					hr, _T("Failed to copy the command line from the configuration file to a local buffer"));
			}
		}
	}

	if (SUCCEEDED(hr))
	{
		startupinfo->cb = sizeof startupinfo;
		startupinfo->dwFlags |= STARTF_USESTDHANDLES;

		WRAPPER_INFO(_T("Starting process with command line '%s'"), command_line);

		if (!CreateProcess(NULL,
		                   command_line,
		                   NULL,
		                   NULL,
		                   FALSE,
		                   0,
		                   NULL,
		                   NULL,
		                   startupinfo,
		                   process_information)
		)
		{
			DWORD last_error = GetLastError();
			if (error)
			{
				*error = wrapper_error_from_system(last_error, _T("Failed to start the process with command line '%s'"),
				                                   command_line);
			}
			hr = HRESULT_FROM_WIN32(last_error);
		}
	}

	HANDLE process = NULL;

	wrapper_free(command_line);
	wrapper_free(startupinfo);

	if (process_information)
	{
		process = process_information->hProcess;

		if (process_information->hThread)
		{
			CloseHandle(process_information->hThread);
		}
		wrapper_free(process_information);
	}

	return process;
}

int wrapper_wait(HANDLE process, wrapper_config_t* config, wrapper_error_t** error)
{
	DWORD last_error;
	HRESULT hr = S_OK;
	HANDLE events[2];
	HANDLE stop_event = NULL;

	if (SUCCEEDED(hr))
	{
		stop_event = OpenEvent(EVENT_ALL_ACCESS, TRUE, stop_event_name);
		if (!stop_event)
		{
			last_error = GetLastError();
			if (error)
			{
				*error = wrapper_error_from_system(
					last_error, _T("Failed to wait either for the process to terminate or for the stop event to be raised"));
			}
			hr = HRESULT_FROM_WIN32(last_error);
		}
	}

	if (SUCCEEDED(hr))
	{
		events[0] = process;
		events[1] = stop_event;

		const unsigned count = sizeof events / sizeof events[0];
		const int wait_all = FALSE;

		DWORD event = WaitForMultipleObjects(count, events, wait_all, INFINITE);
		switch (event)
		{
		case WAIT_OBJECT_0 + 0:
			// TODO: Display more information about the state of the process, like whether it was killed, crashed or terminated gracefully 
			WRAPPER_INFO(_T("The child process has ended."));
			wrapper_service_report_status(SERVICE_STOP_PENDING, NO_ERROR, 0, config, error);
			break;

		case WAIT_OBJECT_0 + 1:
			WRAPPER_INFO(_T("A request was received to stop the service. Sending a CTRL+C signal to the child process."));
			wrapper_service_report_status(SERVICE_STOP_PENDING, NO_ERROR, 0, config, error);
			DWORD pid = GetProcessId(process);
			SendConsoleCtrlEvent(pid, CTRL_C_EVENT);

			// TODO: Should we forcefully terminate the process if it doesn't respond in say 10 minutes?
			const int timeout = 5000;
			DWORD status;
			do
			{
				wrapper_service_report_status(SERVICE_STOP_PENDING, NO_ERROR, timeout, config, error);
				WRAPPER_INFO(_T("Waiting up to %dms for the child process to termimate."), timeout);
				status = WaitForSingleObject(process, timeout);
			}
			while (status == WAIT_TIMEOUT);
			WRAPPER_INFO(_T("The child process succesfully termimated."));
			break;

		case WAIT_TIMEOUT:
			WRAPPER_WARNING(_T("Wait timed out while waiting for the process to terminate or for the stop event.\n"));
			wrapper_service_report_status(SERVICE_STOP_PENDING, NO_ERROR, 0, config, error);
			hr = HRESULT_FROM_WIN32(ERROR_TIMEOUT);
			if (error)
			{
				*error = wrapper_error_from_hresult(
					hr, _T("Failed to wait either for the process to terminate or for the stop event to be raised"));
			}
			break;

		default:
			wrapper_service_report_status(SERVICE_STOP_PENDING, NO_ERROR, 0, config, error);
			last_error = GetLastError();
			if (error)
			{
				*error = wrapper_error_from_system(
					last_error, _T("Failed to wait either for the process to terminate or for the stop event to be raised"));
			}
			hr = HRESULT_FROM_WIN32(last_error);
			break;
		}
	}

	if (stop_event)
	{
		CloseHandle(stop_event);
	}

	if (FAILED(hr))
	{
		return 0;
	}

	return 1;
}

//
// Purpose: 
//   The service code
//
// Parameters:
//   dwArgc - Number of arguments in the lpszArgv array
//   lpszArgv - Array of strings. The first string is the name of
//     the service and subsequent strings are passed by the process
//     that called the StartService function to start the service.
// 
// Return value:
//   None
//
int wrapper_service_init(wrapper_config_t* config, wrapper_error_t** error)
{
	HRESULT hr = S_OK;
	DWORD last_error;
	HANDLE process = NULL;

	wrapper_service_report_status(SERVICE_START_PENDING, NO_ERROR, 3000, config, error);

	if (SUCCEEDED(hr))
	{
		HANDLE stop_event = CreateEvent(NULL, TRUE, FALSE, stop_event_name);
		if (stop_event == NULL)
		{
			last_error = GetLastError();
			if (error)
			{
				*error = wrapper_error_from_system(
					last_error, _T("Failed to register the event for service '%s' that would be used to say the process has stopped."),
					config->name);
			}
			hr = HRESULT_FROM_WIN32(last_error);
		}
	}


	if (SUCCEEDED(hr))
	{
		process = wrapper_create_child_process(config, error);
		if (process)
		{
			DWORD pid = GetProcessId(process);
			// TODO: Display more information that could help the user diagnose when there is a failure to execute the process 
			WRAPPER_INFO(_T("Successfully started process with command line '%s'"), config->command_line);
			WRAPPER_INFO(_T("  Process ID: %d (0x%08x)"), pid, pid);

			wrapper_service_report_status(SERVICE_RUNNING, NO_ERROR, 0, config, error);
		}
		else
		{
			if (error)
			{
				wrapper_error_log(*error);
			}
			hr = E_FAIL;
		}
	}

	if (SUCCEEDED(hr))
	{
		if (!wrapper_wait(process, config, error))
		{
			if (error)
			{
				wrapper_error_log(*error);
			}
			hr = E_FAIL;
		}
	}

	if (error && *error)
	{
		const long code = (*error)->code;
		WRAPPER_INFO(_T("The windows service stopped with errors."));
		wrapper_service_report_status(SERVICE_STOPPED, code, 0, config, error);
	}
	else
	{
		WRAPPER_INFO(_T("The windows service stopped succesfully."));
		wrapper_service_report_status(SERVICE_STOPPED, NO_ERROR, 0, config, error);
	}

	if (process)
	{
		CloseHandle(process);
	}
	return 1;
}

//
// Purpose: 
//   Sets the current service status and reports it to the SCM.
//
// Parameters:
//   state - The current state (see SERVICE_STATUS)
//   exit_code - The system error code
//   timeout - Estimated time for pending operation, 
//     in milliseconds
// 
// Return value:
//   None
//
int wrapper_service_report_status(DWORD state,
                                  DWORD exit_code,
                                  DWORD timeout,
                                  wrapper_config_t* config,
                                  wrapper_error_t** error)
{
	static DWORD dwCheckPoint = 1;

	SERVICE_STATUS service_status = {0};

	service_status.dwServiceSpecificExitCode = 0;
	service_status.dwCurrentState = state;
	service_status.dwWin32ExitCode = exit_code;
	service_status.dwWaitHint = timeout;
	service_status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;

	if (state == SERVICE_START_PENDING)
		service_status.dwControlsAccepted = 0;
	else
		service_status.dwControlsAccepted = SERVICE_ACCEPT_STOP;

	if (state == SERVICE_RUNNING ||
		state == SERVICE_STOPPED)
		service_status.dwCheckPoint = 0;
	else
		service_status.dwCheckPoint = dwCheckPoint++;

	const TCHAR* status_text = wrapper_service_get_status_text(state);
	WRAPPER_INFO(_T("Setting the status of the service to '%s'"), status_text);
	if (SetServiceStatus(status_handle, &service_status))
	{
		WRAPPER_INFO(_T("Set the status of the service to '%s'"), status_text);
	}
	else
	{
		DWORD last_error = GetLastError();
		if (error)
		{
			*error = wrapper_error_from_system(
				last_error, _T("Failed to set the status of the service to '%s'"),
				status_text);
		}

		return 0;
	}
	return 1;
}

//
// Purpose: 
//   Called by SCM whenever a control code is sent to the service
//   using the ControlService function.
//
// Parameters:
//   dwCtrl - control code
// 
// Return value:
//   None
//
VOID WINAPI wrapper_service_control_handler(DWORD dwCtrl)
{
	switch (dwCtrl)
	{
	case SERVICE_CONTROL_STOP:
		{
			wrapper_error_t* error = NULL;
			HANDLE stop_event = OpenEvent(EVENT_ALL_ACCESS, TRUE, stop_event_name);
			if (stop_event)
			{
				WRAPPER_INFO(_T("Received stop request from the service manager."));
				if(SetEvent(stop_event))
				{
					WRAPPER_INFO(_T("Succesfully set the event '%s'."), stop_event_name);
				}
				else
				{
					error = wrapper_error_from_system(GetLastError(), _T("Failed to set the event '%s'. The service may not stop."), stop_event_name);
				}
				CloseHandle(stop_event);
			}
			else
			{
				error = wrapper_error_from_system(GetLastError(), _T("Failed to set the event '%s'."), stop_event_name);
			}

			if (error)
			{
				wrapper_error_log(error);
				wrapper_error_free(error);
			}
		}
		break;

	case SERVICE_CONTROL_INTERROGATE:
		break;

	default:
		break;
	}
}

int wrapper_log_get_path(TCHAR* destination, const size_t size, wrapper_config_t* config, wrapper_error_t** error)
{
	HRESULT hr = S_OK;
	if(!GetModuleFileName(NULL, destination, size))
	{
		hr = HRESULT_FROM_WIN32(GetLastError());
		if (error)
		{
			*error = wrapper_error_from_hresult(hr, _T("Failed to get the path of the current process."));
		}
	}

	if (SUCCEEDED(hr))
	{
		PathCchRenameExtension(destination, size, _T(".log"));
	}

	if (FAILED(hr))
	{
		return 0;
	}

	return 1;
}

int wrapper_service_run(wrapper_config_t* config, wrapper_error_t** error)
{
	HRESULT hr = S_OK;
	TCHAR* log_path = NULL;

	if (SUCCEEDED(hr))
	{
		log_path = wrapper_allocate_string(MAX_PATH);
		if (!log_path)
		{
			hr = E_OUTOFMEMORY;
		}
	}

	if (SUCCEEDED(hr))
	{
		if (!wrapper_log_get_path(log_path, _MAX_PATH, config, error))
		{
			hr = E_FAIL;
		}
	}

	if (SUCCEEDED(hr))
	{
		wrapper_log_set_handler(wrapper_log_file_handler, log_path);
	}

	if (SUCCEEDED(hr))
	{
		WRAPPER_INFO(_T("Starting Service"));
		SERVICE_TABLE_ENTRY DispatchTable[] =
		{
			{config->name, (LPSERVICE_MAIN_FUNCTION)wrapper_service_main},
			{NULL, NULL}
		};

		if (!StartServiceCtrlDispatcher(DispatchTable))
		{
			hr = E_FAIL;
			if (error)
			{
				*error = wrapper_error_from_hresult(hr, _T("Failed to start the service '%s'."), config->name);
			}
		}
		else 
		{
			WRAPPER_INFO(_T("Done"));
		}
	}

	if (FAILED(hr))
	{
		return 0;
	}
	return 1;
}

int wrapper_get_current_process_filename(TCHAR* buffer, size_t size, wrapper_config_t* config, wrapper_error_t** error)
{
	if (!GetModuleFileName(NULL, buffer, size))
	{
		if (error)
		{
			*error = wrapper_error_from_system(GetLastError(), _T("Unable to get the filename of the current process"));
		}
		return 0;
	}
	return 1;
}

int wrapper_service_open_manager(SC_HANDLE* manager, wrapper_error_t** error)
{
	int rc = 1;
	*manager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	if (NULL == *manager)
	{
		rc = 0;
		if (error)
		{
			*error = wrapper_error_from_system(GetLastError(), _T("Unable to get the filename of the current process"));
		}
	}
	return rc;
}

int wrapper_service_create(SC_HANDLE* service, const SC_HANDLE manager, const wrapper_config_t* config,
                           wrapper_error_t** error)
{
	int rc = 1;
	TCHAR *path = NULL;
	TCHAR* service_name = NULL;
	TCHAR* display_name = NULL;

	if (rc)
	{
		rc = wrapper_string_duplicate(&service_name, config->name, error);
	}

	if (rc)
	{
		if (0 == _tcslen(config->title))
		{
			rc = wrapper_string_duplicate(&display_name, config->name, error);
		}
		else
		{
			rc = wrapper_string_duplicate(&display_name, config->title, error);
		}
	}


	if (rc)
	{
		path = wrapper_allocate_string(_MAX_PATH);
		if (!path)
		{
			rc = 0;
			if (error)
			{
				*error = wrapper_error_from_system(ERROR_OUTOFMEMORY, _T("Unable to get the filename of the current process"));
			}
		}
	}

	if (rc)
	{
		rc = wrapper_get_current_process_filename(path, _MAX_PATH, config, error);
	}

	if (rc)
	{
		void* load_order_group = NULL;
		void* tag_id = NULL;
		void* dependencies = NULL;
		void* username = NULL;
		void* password = NULL;
		const long desired_access = SERVICE_ALL_ACCESS;
		const int service_type = SERVICE_WIN32_OWN_PROCESS;
		const int start_type = SERVICE_DEMAND_START;
		const int error_control = SERVICE_ERROR_NORMAL;

		*service = CreateService(
			manager,  
			service_name,  
			display_name,
			desired_access,
			service_type,
			start_type,
			error_control,  
			path,
			load_order_group,
			tag_id,
			dependencies,
			username,
			password);  

		if (*service == NULL)
		{
			if (error)
			{
				*error = wrapper_error_from_system(GetLastError(), _T("Failed to create service '%s' (%s)"), service_name, display_name);
			}
			rc = 0;
		}
	}

	wrapper_free(service_name);
	wrapper_free(display_name);
	wrapper_free(path);

	return rc;
}

//
// Purpose: 
//   Installs a service in the SCM database
//
// Parameters:
//   None
// 
// Return value:
//   None
//
int wrapper_service_install(wrapper_config_t* config, wrapper_error_t** error)
{
	int rc = 1;
	SC_HANDLE service = NULL;
	SC_HANDLE manager = NULL;

	if (rc)
	{
		rc = wrapper_service_open_manager(&manager, error);
	}

	if (rc)
	{
		rc = wrapper_service_create(&service, manager, config, error);
		if(rc) 
		{
			WRAPPER_INFO(_T("The service '%s' was successfully installed"), config->name);
		}
	}

	if (service)
	{
		CloseServiceHandle(service);
	}

	if (manager)
	{
		CloseServiceHandle(manager);
	}

	
	return rc;
}

//
// Purpose: 
//   Retrieves and displays the current service configuration.
//
// Parameters:
//   None
// 
// Return value:
//   None
//
int wrapper_service_query(wrapper_config_t* config, wrapper_error_t** error)
{
	SC_HANDLE manager = NULL;
	SC_HANDLE service = NULL;
	LPQUERY_SERVICE_CONFIG service_config = NULL;
	LPSERVICE_DESCRIPTION service_description = NULL;
	DWORD dwBytesNeeded = 0;
	DWORD cbBufSize = 0;
	DWORD dwError = 0;

	manager = OpenSCManager(
		NULL, 
		NULL,  
		SC_MANAGER_ALL_ACCESS);  

	if (NULL == manager)
	{
		printf("OpenSCManager failed (%d)\n", GetLastError());
		return;
	}

	// Get a handle to the service.

	service = OpenService(
		manager, // SCM database 
		config->name, // name of service 
		SERVICE_QUERY_CONFIG); // need query config access 

	if (service == NULL)
	{
		printf("OpenService failed (%d)\n", GetLastError());
		CloseServiceHandle(manager);
		return;
	}

	// Get the configuration information.

	if (!QueryServiceConfig(
		service,
		NULL,
		0,
		&dwBytesNeeded))
	{
		dwError = GetLastError();
		if (ERROR_INSUFFICIENT_BUFFER == dwError)
		{
			cbBufSize = dwBytesNeeded;
			service_config = (LPQUERY_SERVICE_CONFIG)LocalAlloc(LMEM_FIXED, cbBufSize);
		}
		else
		{
			printf("QueryServiceConfig failed (%d)", dwError);
			goto cleanup;
		}
	}

	if (!QueryServiceConfig(
		service,
		service_config,
		cbBufSize,
		&dwBytesNeeded))
	{
		printf("QueryServiceConfig failed (%d)", GetLastError());
		goto cleanup;
	}

	if (!QueryServiceConfig2(
		service,
		SERVICE_CONFIG_DESCRIPTION,
		NULL,
		0,
		&dwBytesNeeded))
	{
		dwError = GetLastError();
		if (ERROR_INSUFFICIENT_BUFFER == dwError)
		{
			cbBufSize = dwBytesNeeded;
			service_description = (LPSERVICE_DESCRIPTION)LocalAlloc(LMEM_FIXED, cbBufSize);
		}
		else
		{
			printf("QueryServiceConfig2 failed (%d)", dwError);
			goto cleanup;
		}
	}

	if (!QueryServiceConfig2(
		service,
		SERVICE_CONFIG_DESCRIPTION,
		(LPBYTE)service_description,
		cbBufSize,
		&dwBytesNeeded))
	{
		printf("QueryServiceConfig2 failed (%d)", GetLastError());
		goto cleanup;
	}

	// Print the configuration information.

	_tprintf(TEXT("%s configuration: \n"), config->name);
	_tprintf(TEXT("  Type: 0x%x\n"), service_config->dwServiceType);
	_tprintf(TEXT("  Start Type: 0x%x\n"), service_config->dwStartType);
	_tprintf(TEXT("  Error Control: 0x%x\n"), service_config->dwErrorControl);
	_tprintf(TEXT("  Binary path: %s\n"), service_config->lpBinaryPathName);
	_tprintf(TEXT("  Account: %s\n"), service_config->lpServiceStartName);

	if (service_description->lpDescription != NULL && lstrcmp(service_description->lpDescription, TEXT("")) != 0)
		_tprintf(TEXT("  Description: %s\n"), service_description->lpDescription);
	if (service_config->lpLoadOrderGroup != NULL && lstrcmp(service_config->lpLoadOrderGroup, TEXT("")) != 0)
		_tprintf(TEXT("  Load order group: %s\n"), service_config->lpLoadOrderGroup);
	if (service_config->dwTagId != 0)
		_tprintf(TEXT("  Tag ID: %d\n"), service_config->dwTagId);
	if (service_config->lpDependencies != NULL && lstrcmp(service_config->lpDependencies, TEXT("")) != 0)
		_tprintf(TEXT("  Dependencies: %s\n"), service_config->lpDependencies);

	LocalFree(service_config);
	LocalFree(service_description);

cleanup:
	CloseServiceHandle(service);
	CloseServiceHandle(manager);

	return 1;
}

//
// Purpose: 
//   Disables the service.
//
// Parameters:
//   None
// 
// Return value:
//   None
//
int wrapper_service_disable(wrapper_config_t* config, wrapper_error_t** error)
{
	SC_HANDLE schSCManager;
	SC_HANDLE schService;

	// Get a handle to the SCM database. 

	schSCManager = OpenSCManager(
		NULL, // local computer
		NULL, // ServicesActive database 
		SC_MANAGER_ALL_ACCESS); // full access rights 

	if (NULL == schSCManager)
	{
		printf("OpenSCManager failed (%d)\n", GetLastError());
		return;
	}

	// Get a handle to the service.

	schService = OpenService(
		schSCManager, // SCM database 
		config->name, // name of service 
		SERVICE_CHANGE_CONFIG); // need change config access 

	if (schService == NULL)
	{
		printf("OpenService failed (%d)\n", GetLastError());
		CloseServiceHandle(schSCManager);
		return;
	}

	// Change the service start type.

	if (!ChangeServiceConfig(
		schService, // handle of service 
		SERVICE_NO_CHANGE, // service type: no change 
		SERVICE_DISABLED, // service start type 
		SERVICE_NO_CHANGE, // error control: no change 
		NULL, // binary path: no change 
		NULL, // load order group: no change 
		NULL, // tag ID: no change 
		NULL, // dependencies: no change 
		NULL, // account name: no change 
		NULL, // password: no change 
		NULL)) // display name: no change
	{
		printf("ChangeServiceConfig failed (%d)\n", GetLastError());
	}
	else
		printf("Service disabled successfully.\n");

	CloseServiceHandle(schService);
	CloseServiceHandle(schSCManager);

	return 1;
}

//
// Purpose: 
//   Enables the service.
//
// Parameters:
//   None
// 
// Return value:
//   None
//
int wrapper_service_enable(wrapper_config_t* config, wrapper_error_t** error)
{
	SC_HANDLE schSCManager;
	SC_HANDLE schService;

	// Get a handle to the SCM database. 

	schSCManager = OpenSCManager(
		NULL, // local computer
		NULL, // ServicesActive database 
		SC_MANAGER_ALL_ACCESS); // full access rights 

	if (NULL == schSCManager)
	{
		printf("OpenSCManager failed (%d)\n", GetLastError());
		return;
	}

	// Get a handle to the service.

	schService = OpenService(
		schSCManager, // SCM database 
		config->name, // name of service 
		SERVICE_CHANGE_CONFIG); // need change config access 

	if (schService == NULL)
	{
		printf("OpenService failed (%d)\n", GetLastError());
		CloseServiceHandle(schSCManager);
		return;
	}

	// Change the service start type.

	if (!ChangeServiceConfig(
		schService, // handle of service 
		SERVICE_NO_CHANGE, // service type: no change 
		SERVICE_DEMAND_START, // service start type 
		SERVICE_NO_CHANGE, // error control: no change 
		NULL, // binary path: no change 
		NULL, // load order group: no change 
		NULL, // tag ID: no change 
		NULL, // dependencies: no change 
		NULL, // account name: no change 
		NULL, // password: no change 
		NULL)) // display name: no change
	{
		printf("ChangeServiceConfig failed (%d)\n", GetLastError());
	}
	else
		printf("Service enabled successfully.\n");

	CloseServiceHandle(schService);
	CloseServiceHandle(schSCManager);

	return 1;
}

//
// Purpose: 
//   Updates the service description to "This is a test description".
//
// Parameters:
//   None
// 
// Return value:
//   None
//
int wrapper_service_update(wrapper_config_t* config, wrapper_error_t** error)
{
	SC_HANDLE schSCManager;
	SC_HANDLE schService;
	SERVICE_DESCRIPTION sd;
	LPTSTR szDesc = config->description;

	// Get a handle to the SCM database. 

	schSCManager = OpenSCManager(
		NULL, // local computer
		NULL, // ServicesActive database 
		SC_MANAGER_ALL_ACCESS); // full access rights 

	if (NULL == schSCManager)
	{
		printf("OpenSCManager failed (%d)\n", GetLastError());
		return;
	}

	// Get a handle to the service.

	schService = OpenService(
		schSCManager, // SCM database 
		config->name, // name of service 
		SERVICE_CHANGE_CONFIG); // need change config access 

	if (schService == NULL)
	{
		printf("OpenService failed (%d)\n", GetLastError());
		CloseServiceHandle(schSCManager);
		return;
	}

	// Change the service description.

	sd.lpDescription = szDesc;

	if (!ChangeServiceConfig2(
		schService, // handle to service
		SERVICE_CONFIG_DESCRIPTION, // change: description
		&sd)) // new description
	{
		printf("ChangeServiceConfig2 failed\n");
	}
	else
		printf("Service description updated successfully.\n");

	CloseServiceHandle(schService);
	CloseServiceHandle(schSCManager);

	return 1;
}

//
// Purpose: 
//   Deletes a service from the SCM database
//
// Parameters:
//   None
// 
// Return value:
//   None
//
int wrapper_service_delete(wrapper_config_t* config, wrapper_error_t** error)
{
	SC_HANDLE schSCManager;
	SC_HANDLE schService;
	SERVICE_STATUS ssStatus;

	// Get a handle to the SCM database. 

	schSCManager = OpenSCManager(
		NULL, // local computer
		NULL, // ServicesActive database 
		SC_MANAGER_ALL_ACCESS); // full access rights 

	if (NULL == schSCManager)
	{
		printf("OpenSCManager failed (%d)\n", GetLastError());
		return;
	}

	// Get a handle to the service.

	schService = OpenService(
		schSCManager, // SCM database 
		config->name, // name of service 
		DELETE); // need delete access 

	if (schService == NULL)
	{
		printf("OpenService failed (%d)\n", GetLastError());
		CloseServiceHandle(schSCManager);
		return;
	}

	// Delete the service.

	if (!DeleteService(schService))
	{
		printf("DeleteService failed (%d)\n", GetLastError());
	}
	else
		printf("Service deleted successfully\n");

	CloseServiceHandle(schService);
	CloseServiceHandle(schSCManager);

	return 1;
}
