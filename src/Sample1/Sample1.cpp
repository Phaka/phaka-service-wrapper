#include "stdafx.h"
#include <windows.h>
#include <stdio.h>
#include <ctime>

#define BUFSIZE 4096 

int _tmain(int argc, TCHAR* argv[])
{
    time_t rawtime;
    struct tm timeinfo;
    char szTime[MAX_PATH];
    time(&rawtime);
    localtime_s(&timeinfo, &rawtime);
    asctime_s(szTime, _MAX_PATH, &timeinfo);
    printf("\n");
    printf("------------------------------------------------------------------------\n");
    printf(" %s", szTime);
    printf("------------------------------------------------------------------------\n");
    printf("\n");

    LPTCH lpszEnvironmentStrings;
    lpszEnvironmentStrings = GetEnvironmentStrings();
    for (LPTSTR lpszVariable = (LPTSTR)lpszEnvironmentStrings; *lpszVariable; lpszVariable++)
    {
        while (*lpszVariable)
            _puttchar(*lpszVariable++);
        _puttchar('\n');
    }
    FreeEnvironmentStrings(lpszEnvironmentStrings);

    TCHAR szCurrentDirectory[_MAX_PATH];
    GetCurrentDirectory(_MAX_PATH, szCurrentDirectory);
    _tprintf(_T("\n\nCurrent Directory: %s\n\n"), szCurrentDirectory);

    int iterations = 5;
    if (argc == 2)
    {
        iterations = _tstoi(argv[1]);
    }

    for (size_t i = 0; i < iterations; i++)
    {
        printf("Step %d\n", i);
        Sleep(1000);
    }

    return 0;
}
