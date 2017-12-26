// Copyright (c) Werner Strydom. All rights reserved.
// Licensed under the MIT license. See LICENSE in the project root for license information.

#include "stdafx.h"

HRESULT CreateDirectoryX(LPCTSTR szPathName)
{
	TCHAR* pszDirectoryName;
	TCHAR *end = NULL; 
	HRESULT hr = S_OK;
	size_t cbToCopy;
	DWORD error = ERROR_SUCCESS;

	if (PathFileExists(szPathName))
	{
		return S_OK;
	}

	pszDirectoryName = LocalAlloc(LPTR, sizeof(TCHAR) * MAX_PATH);
	if (!pszDirectoryName)
	{
		return E_OUTOFMEMORY;
	}


	end = _tcschr(szPathName, _T('\\')); // this would S:
	if (end)
	{
		end = _tcschr(++end, _T('\\'));
	}

	while (SUCCEEDED(hr) && end != NULL)
	{
		if (SUCCEEDED(hr))
		{
			cbToCopy = sizeof(TCHAR) * (end - szPathName);
			hr = StringCbCopyN(pszDirectoryName, _MAX_PATH, szPathName, cbToCopy);
		}

		if (SUCCEEDED(hr))
		{
			if (!CreateDirectory(pszDirectoryName, NULL))
			{
				error = GetLastError();
				if (error != ERROR_ALREADY_EXISTS)
				{
					hr = HRESULT_FROM_WIN32(error);
				}
			}
		}

		end = _tcschr(++end, _T('\\'));
	}

	if (!CreateDirectory(szPathName, NULL))
	{
		error = GetLastError();
		if (error != ERROR_ALREADY_EXISTS)
		{
			hr = HRESULT_FROM_WIN32(error);
		}
	}

	LocalFree(pszDirectoryName);
	return hr;
}

HRESULT SetEnvironmentVariables(LPCTSTR pszConfigurationPath)
{
	//
	// If we pass environments to CreateProcess, then none of the existing variables will be visible to the
	// child process. So instead, we'll read the environment variables and manually set them, thus merging
	// environment variables.
	//
	HRESULT hr = S_OK;
	TCHAR *pszText = NULL;
	TCHAR *pszEnvironmentVariables = NULL;

	pszText = LocalAlloc(LPTR, sizeof(TCHAR) * _MAX_PATH);
	if (!pszText)
	{
		hr = E_OUTOFMEMORY;
	}

	pszEnvironmentVariables = LocalAlloc(LPTR, sizeof(TCHAR) * 32767);
	if (!pszEnvironmentVariables)
	{
		hr = E_OUTOFMEMORY;
	}

	if (SUCCEEDED(hr))
	{
		if (!GetPrivateProfileSection(_T("Environment"), pszEnvironmentVariables, 32767, pszConfigurationPath))
		{
			DWORD error = GetLastError();
			if (error != ERROR_FILE_NOT_FOUND)
			{
				hr = HRESULT_FROM_WIN32(error);
			}
		}
	}
	

	size_t length = 0;
	if (SUCCEEDED(hr))
	{
		hr = StringCbLength(pszEnvironmentVariables, 32767, &length);
	}
	
	if (SUCCEEDED(hr) && 0 < length)
	{
		for (LPTSTR pszVariable = (LPTSTR)pszEnvironmentVariables; *pszVariable; pszVariable++)
		{
			for (size_t i = 0; *pszVariable; i++)
			{
				pszText[i] = *pszVariable;
				pszVariable++;
			}

			wchar_t *pszToken = NULL;
			LPCTSTR pszName = _tcstok_s(pszText, _T("="), &pszToken);
			LPCTSTR pszValue = _tcstok_s(NULL, _T("="), &pszToken);

			if(!SetEnvironmentVariable(pszName, pszValue))
			{
				hr = HRESULT_FROM_WIN32(GetLastError());
				break;
			}
		}
	}

	LocalFree(pszText);
	LocalFree(pszEnvironmentVariables);

	return hr;
}

HRESULT PathChangeExtension(
	_Out_ LPTSTR  pszDest,
	_In_  size_t  cchDest,
	_In_  LPCTSTR pszSrc,
	_In_  LPCTSTR pszExt,
	_In_  size_t  cchExt)
{
	HRESULT hr = S_OK;
	size_t length = 0;

	hr = StringCchCopy(pszDest, cchDest, pszSrc);
	
	if (SUCCEEDED(hr) && cchExt > 0)
	{
		hr = StringCchLength(pszExt, cchExt + 1, &length);
	}

	if (SUCCEEDED(hr) && length > 0)
	{
		hr = PathCchRenameExtension(pszDest, cchDest, pszExt);
	}

	return hr;
}

// 
// For very long running process that produce a lot of output, we may need to investigate the following:
//
//   (1) Rolling Files based on date and size
//   (2) Creating date based directories, because IO gets slower the more files there are in the directory
//   (3) Whether the log files should be in the AppData folder of the current user; this may actually be more
//       secure that storing the log files in the same location as the executable, which could be readable
//       by any process
//   (4) Expand Variables that are provided in the szPathName, e.g. %APPDATA%\Selenium\Logs
//
HRESULT GetLogPathSetting(
	_Out_ LPTSTR  pszDest,
	_In_  size_t  cchDest,
	_In_  LPCTSTR pszConfigurationPath)
{
	HRESULT hr = S_OK;
	DWORD dwLastError = ERROR_SUCCESS;
	const int size = MAX_PATH;
	TCHAR* pszSetting = NULL;
	TCHAR* pszBasePath = NULL;

	pszSetting = LocalAlloc(LPTR, sizeof(TCHAR)* size);
	if (!pszSetting)
	{
		hr = E_OUTOFMEMORY;
	}

	if (SUCCEEDED(hr))
	{
		pszBasePath = LocalAlloc(LPTR, sizeof(TCHAR)* size);
		if (!pszBasePath)
		{
			hr = E_OUTOFMEMORY;
		}
	}

	if (SUCCEEDED(hr))
	{
		if (!GetPrivateProfileString(_T("Logging"), _T("Path"), _T(""), pszSetting, size, pszConfigurationPath))
		{
			dwLastError = GetLastError();
			if (dwLastError != ERROR_FILE_NOT_FOUND)
			{
				hr = HRESULT_FROM_WIN32(dwLastError);
			}
		}
	}
	

	size_t length = 0;
	if (SUCCEEDED(hr))
	{
		hr = StringCchLength(pszSetting, size, &length);
	}

	// If no log szPathName was specified, we're just going to write it in the location where the
	// executable is at. This is generally a bad idea, since the szPathName of the executable is
	// readable by almost anyone. If logs have any kind of need to know information, we may
	// be causing more trouble that we should. We'll revisit this later. Maybe we should store
	// this in %APPDATA%\%ServiceName%\logs?
	if (SUCCEEDED(hr))
	{
		if (0 >= length)
		{
			hr = PathChangeExtension(pszDest, cchDest, pszConfigurationPath, _T(".log"), 4);
		}
		else if (PathIsRelative(pszSetting))
		{
			if (SUCCEEDED(hr))
			{
				hr = StringCchCopy(pszDest, cchDest, pszConfigurationPath);
			}

			if (SUCCEEDED(hr))
			{
				hr = PathCchRemoveFileSpec(pszDest, cchDest);
			};
			if (SUCCEEDED(hr))
			{
				hr = PathCchAppend(pszDest, cchDest, pszSetting);
			}

			if (SUCCEEDED(hr))
			{
				hr = StringCchCopy(pszBasePath, size, pszDest);
			}

			if (SUCCEEDED(hr))
			{
				hr = PathCchRemoveFileSpec(pszBasePath, size);
			}

			if (SUCCEEDED(hr))
			{
				hr = CreateDirectoryX(pszBasePath);
			}
		}
		else
		{
			hr = StringCchCopy(pszDest, cchDest, pszSetting);
		}
	}

	LocalFree(pszSetting);
	LocalFree(pszBasePath);

	return hr;
}

HRESULT GetDirectoryName(
	_Out_ LPTSTR  pszDest,
	_In_  size_t  cchDest,
	_In_  LPCTSTR pszSrc)
{
	HRESULT hr = S_OK;

	if (SUCCEEDED(hr))
	{
		hr = StringCchCopy(pszDest, cchDest, pszSrc);
	}

	if (SUCCEEDED(hr))
	{
		hr = PathCchRemoveFileSpec(pszDest, cchDest);
	}

	return hr;
}

HRESULT ExecuteChildProcess(TCHAR *pszCommandLine, TCHAR* pszCurrentDirectory, HANDLE hStdOutput)
{
	HRESULT hr = S_OK;
	PROCESS_INFORMATION* lpProcessInformation = NULL;
	STARTUPINFO* lpStartUpInfo = NULL;
	size_t length = 0;

	lpProcessInformation = LocalAlloc(LPTR, sizeof(PROCESS_INFORMATION));
	if (!lpProcessInformation)
	{
		hr = E_OUTOFMEMORY;
	}
	
	lpStartUpInfo = LocalAlloc(LPTR, sizeof(STARTUPINFO));
	if (!lpStartUpInfo)
	{
		hr = E_OUTOFMEMORY;
	}

	if (SUCCEEDED(hr))
	{
		hr = StringCbLength(pszCurrentDirectory, _MAX_PATH, &length);
		if (FAILED(hr))
		{
			return hr;
		}
	}

	if (SUCCEEDED(hr))
	{
		if (length == 0)
		{
			// Create process will fail if the current directory is empty (that is "\0"), so
			// we'll just make it NULL.
			pszCurrentDirectory = NULL;
		}

		lpStartUpInfo->cb = sizeof(STARTUPINFO);
		lpStartUpInfo->hStdError = hStdOutput;
		lpStartUpInfo->hStdOutput = hStdOutput;
		lpStartUpInfo->dwFlags |= STARTF_USESTDHANDLES;

#if UNICODE
		int flags = CREATE_UNICODE_ENVIRONMENT;
#else
		int flags = 0;
#endif

		if (!CreateProcess(NULL,
			pszCommandLine,
			NULL,
			NULL,
			TRUE,
			flags,
			NULL,
			pszCurrentDirectory,
			lpStartUpInfo,
			lpProcessInformation))
		{
			hr = HRESULT_FROM_WIN32(GetLastError());
		}
	}

	if (SUCCEEDED(hr))
	{
		if (WAIT_FAILED == WaitForSingleObject(lpProcessInformation->hProcess, INFINITE))
		{
			hr = HRESULT_FROM_WIN32(GetLastError());
		}
	}

	if (lpProcessInformation)
	{
		if (lpProcessInformation->hProcess)
		{
			CloseHandle(lpProcessInformation->hProcess);
		}
		if (lpProcessInformation->hThread)
		{
			CloseHandle(lpProcessInformation->hThread);
		}
	}

	LocalFree(lpProcessInformation);
	LocalFree(lpStartUpInfo);

	return hr;
}

HRESULT GetWorkingDirectory(
	_Out_ LPTSTR  pszDest,
	_In_  size_t  cchDest,
	_In_  LPCTSTR pszConfigurationPath)
{
	HRESULT hr = S_OK;
	size_t length = 0;
	DWORD dwLastError = ERROR_SUCCESS;

	if(!GetPrivateProfileString(_T("Application"), _T("WorkingDirectory"), _T(""), pszDest, cchDest, pszConfigurationPath))
	{
		dwLastError = GetLastError();
		if (dwLastError != ERROR_FILE_NOT_FOUND)
		{
			hr = HRESULT_FROM_WIN32(dwLastError);
		}
	}

	if (SUCCEEDED(hr))
	{
		hr = StringCchLength(pszDest, cchDest, &length);
	}

	if (SUCCEEDED(hr))
	{
		if (0 >= length)
		{
			hr = GetDirectoryName(pszDest, cchDest, pszConfigurationPath);
		}
	}

	return hr;
}

HRESULT GetCommandLineSetting(
	_Out_ LPTSTR  pszDest,
	_In_  size_t  cchDest,
	_In_  LPCTSTR pszConfigurationPath)
{
	HRESULT hr = S_OK;
	DWORD dwLastError = ERROR_SUCCESS;
	if(!GetPrivateProfileString(_T("Application"), _T("CommandLine"), _T(""), pszDest, cchDest, pszConfigurationPath))
	{
		dwLastError = GetLastError();
		if (dwLastError != ERROR_FILE_NOT_FOUND)
		{
			hr = HRESULT_FROM_WIN32(dwLastError);
		}
	}

	return hr;
}

HRESULT OpenLogFile(TCHAR *log_path, HANDLE* handle)
{
	HRESULT hr = S_OK;
	SECURITY_ATTRIBUTES security_attributes;
	ZeroMemory(&security_attributes, sizeof(security_attributes));
	security_attributes.nLength = sizeof security_attributes;
	security_attributes.lpSecurityDescriptor = NULL;
	security_attributes.bInheritHandle = TRUE;

	if (NULL == handle)
	{
		return E_INVALIDARG;
	}

	*handle = CreateFile(
		log_path,
		FILE_APPEND_DATA,
		FILE_SHARE_WRITE | FILE_SHARE_READ,
		&security_attributes,
		OPEN_ALWAYS,
		FILE_ATTRIBUTE_NORMAL,
		NULL);

	if (!*handle)
	{
		hr = HRESULT_FROM_WIN32(GetLastError());
	}

	return hr;
}

HRESULT GetErrorMessage(LPTSTR *pszDest, HRESULT dwMessageId)
{
	DWORD rc = ERROR_SUCCESS;        
	HINSTANCE hInstance;  

	if (HRESULT_FACILITY(dwMessageId) == FACILITY_MSMQ)
	{ 
		hInstance = LoadLibrary(TEXT("MQUTIL.DLL"));
		if (hInstance != 0)
		{ 
			rc = FormatMessage(
				FORMAT_MESSAGE_ALLOCATE_BUFFER | 
				FORMAT_MESSAGE_FROM_HMODULE | 
				FORMAT_MESSAGE_IGNORE_INSERTS,
				hInstance, 
				dwMessageId, 
				MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), 
				(LPTSTR)pszDest, 
				4096, 
				NULL 
			);
		} 
	} 
	else if (dwMessageId >= NERR_BASE && dwMessageId <= MAX_NERR)
	{ 
		hInstance = LoadLibrary(TEXT("NETMSG.DLL"));
		if (hInstance != 0)
		{ 
		  rc = FormatMessage(
				FORMAT_MESSAGE_ALLOCATE_BUFFER | 
				FORMAT_MESSAGE_FROM_HMODULE | 
				FORMAT_MESSAGE_IGNORE_INSERTS,  
				hInstance, 
				dwMessageId, 
				MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), 
				(LPTSTR)pszDest, 
				4096, 
				NULL 
			);
		} 
	} 
	else
	{ 
		rc = FormatMessage(
			FORMAT_MESSAGE_ALLOCATE_BUFFER | 
			FORMAT_MESSAGE_FROM_SYSTEM | 
			FORMAT_MESSAGE_IGNORE_INSERTS, 
			NULL, 
			dwMessageId, 
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), 
			(LPTSTR)pszDest, 
			4096, 
			NULL 
		);
	}

	if (!rc)
	{
		return HRESULT_FROM_WIN32(rc);
	}
	return S_OK;
}

HRESULT Run(LPCTSTR pszConfigurationPath)
{
	const size_t size = _MAX_PATH;
	HANDLE hStdOut = NULL;
	LPTSTR pszBasePath = NULL;
	LPTSTR pszCommandLine = NULL;
	LPTSTR pszLogPath = NULL;
	LPTSTR pszWorkingDirectory = NULL;
	HRESULT hr = S_OK;

	if (!pszConfigurationPath)
	{
		return E_INVALIDARG;
	}
	
	pszBasePath = LocalAlloc(LPTR, sizeof(TCHAR)* size);
	if (!pszBasePath)
	{
		hr = E_OUTOFMEMORY;
	}

	pszCommandLine = LocalAlloc(LPTR, sizeof(TCHAR)* size);
	if (!pszCommandLine)
	{
		hr = E_OUTOFMEMORY;
	}

	pszLogPath = LocalAlloc(LPTR, sizeof(TCHAR)* size);
	if (!pszLogPath)
	{
		hr = E_OUTOFMEMORY;
	}

	pszWorkingDirectory = LocalAlloc(LPTR, sizeof(TCHAR)* size);
	if (!pszWorkingDirectory)
	{
		hr = E_OUTOFMEMORY;
	}

	if (SUCCEEDED(hr))
	{
		hr = GetDirectoryName(pszBasePath, size, pszConfigurationPath);
	}

	if (SUCCEEDED(hr))
	{
		if(!SetEnvironmentVariable(_T("PHAKA_SERVICEW_BASEPATH"), pszBasePath))
		{
			hr = HRESULT_FROM_WIN32(GetLastError());
		}
	}

	if (SUCCEEDED(hr))
	{
		hr = GetLogPathSetting(pszLogPath, size, pszConfigurationPath);
	}

	if (SUCCEEDED(hr))
	{
		hr = GetCommandLineSetting(pszCommandLine, size, pszConfigurationPath);
	}

	if (SUCCEEDED(hr))
	{
		hr = SetEnvironmentVariables(pszConfigurationPath);
	}

	if (SUCCEEDED(hr))
	{
		hr = GetWorkingDirectory(pszBasePath, size, pszConfigurationPath);
	}

	if (SUCCEEDED(hr))
	{
		hr = OpenLogFile(pszLogPath, &hStdOut);
	}

	if (SUCCEEDED(hr))
	{
		hr = ExecuteChildProcess(pszCommandLine, pszWorkingDirectory, hStdOut);
	}

	if (hStdOut)
	{
		CloseHandle(hStdOut);
	}
	
	LocalFree(pszBasePath);
	LocalFree(pszCommandLine);
	LocalFree(pszLogPath);
	LocalFree(pszWorkingDirectory);

	return hr;
}

HRESULT GetApplicationName(
	_Out_ LPTSTR  pszDest,
	_In_  size_t  cchDest,
	_In_  LPCTSTR pszConfigurationPath)
{
	HRESULT hr = S_OK;
	DWORD dwLastError = ERROR_SUCCESS;
	size_t length = 0;

	if (!pszDest)
	{
		return E_INVALIDARG;
	}

	if (cchDest <= 0)
	{
		return E_INVALIDARG;
	}
	
	if (SUCCEEDED(hr))
	{
		if (!GetPrivateProfileString(_T("Service"), _T("DisplayName"), _T(""), pszDest, cchDest, pszConfigurationPath))
		{
			dwLastError = GetLastError();
			if (dwLastError != ERROR_FILE_NOT_FOUND)
			{
				hr = HRESULT_FROM_WIN32(dwLastError);
			}
		}

		if (SUCCEEDED(hr))
		{
			hr = StringCbLength(pszDest, cchDest, &length);
		}
	}

	if (SUCCEEDED(hr))
	{
		if (length <= 0)
		{
			if (!GetPrivateProfileString(_T("Service"), _T("DisplayName"), _T(""), pszDest, cchDest, pszConfigurationPath))
			{
				dwLastError = GetLastError();
				if (dwLastError != ERROR_FILE_NOT_FOUND)
				{
					hr = HRESULT_FROM_WIN32(dwLastError);
				}
			}

			if (SUCCEEDED(hr))
			{
				hr = StringCbLength(pszDest, cchDest, &length);
			}
		}
	}

	if (SUCCEEDED(hr))
	{
		if (length <= 0)
		{
			
			if (!GetModuleFileName(NULL, pszDest, cchDest))
			{
				hr = HRESULT_FROM_WIN32(GetLastError());
			}

			if (SUCCEEDED(hr))
			{
				PathStripPath(pszDest);
				PathRemoveExtension(pszDest);
			}
		}
	}
	return hr;
}

HRESULT PrintLogo()
{
	LPCTSTR pszVersion = _T("1.0.0.0");
	_ftprintf(stdout, _T("Phaka Windows Service Wrapper version %s\n"), pszVersion);
	_ftprintf(stdout, _T("Copyright (C) Werner Strydom. All rights reserved.\n"));
	_ftprintf(stdout, _T("Licensed under the MIT license. See LICENSE for license information.\n"));
	_ftprintf(stdout, _T("\n"));

	return S_OK;
}

int _tmain(int argc, TCHAR* argv[])
{
	LPTSTR pszErrorMessage = NULL;
	LPTSTR pszTitle = NULL;
	LPTSTR pszModulePath = NULL;
	LPTSTR pszConfigurationPath = NULL;
	HRESULT hr = S_OK;
	const int size = MAX_PATH;
	
	if (SUCCEEDED(hr))
	{
		pszModulePath = LocalAlloc(LPTR, sizeof(TCHAR)* size);
		if (!pszModulePath)
		{
			hr = E_OUTOFMEMORY;
		}
	}

	if (SUCCEEDED(hr))
	{
		pszConfigurationPath = LocalAlloc(LPTR, sizeof(TCHAR)* size);
		if (!pszConfigurationPath)
		{
			hr = E_OUTOFMEMORY;
		}
	}
	
	if (SUCCEEDED(hr))
	{
		pszTitle = LocalAlloc(LPTR, sizeof(TCHAR)*_MAX_PATH);
		if (!pszTitle)
		{
			hr = E_OUTOFMEMORY;
		}
	}

	if (SUCCEEDED(hr))
	{
		if (!GetModuleFileName(NULL, pszModulePath, size))
		{
			hr = HRESULT_FROM_WIN32(GetLastError());
		}
	}

	if (SUCCEEDED(hr))
	{
		hr = PathChangeExtension(pszConfigurationPath, size, pszModulePath, _T(".cfg"), 4);
	}

	if (SUCCEEDED(hr))
	{
		if (!PathFileExists(pszConfigurationPath))
		{
			_ftprintf(stderr, _T("The configuration file '%s' does not exist."), pszConfigurationPath);
			hr = HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
		}
	}

	if (SUCCEEDED(hr))
	{
		hr = GetApplicationName(pszTitle, _MAX_PATH, pszConfigurationPath);
	}

	if (SUCCEEDED(hr))
	{
		if (!SetConsoleTitle(pszTitle))
		{
			hr = HRESULT_FROM_WIN32(GetLastError());
		}
	}

	if (SUCCEEDED(hr))
	{
		hr = PrintLogo();
	}

	if (SUCCEEDED(hr))
	{
		hr = Run(pszConfigurationPath);
	}

	if (FAILED(hr))
	{
		if (SUCCEEDED(GetErrorMessage(&pszErrorMessage, hr)))
		{
			_ftprintf(stderr, pszErrorMessage);
		}
		else
		{
			_ftprintf(stderr, _T("Failed to extract error"));
		}
		_ftprintf(stderr, _T("\n"));
	}

	LocalFree(pszModulePath);
	LocalFree(pszConfigurationPath);
	LocalFree(pszErrorMessage);
	LocalFree(pszTitle);
	return hr;
}
