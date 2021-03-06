// Sample1.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

volatile bool stop = false;

BOOL CtrlHandler(DWORD fdwCtrlType)
{
    switch (fdwCtrlType)
    {
        // Handle the CTRL-C signal. 
    case CTRL_C_EVENT:
        printf("Ctrl-C event\n\n");
        Beep(750, 300);
        stop = true;
        return(TRUE);

        // CTRL-CLOSE: confirm that the user wants to exit. 
    case CTRL_CLOSE_EVENT:
        Beep(600, 200);
        printf("Ctrl-Close event\n\n");
        stop = true;
        return(TRUE);

        // Pass other signals to the next handler. 
    case CTRL_BREAK_EVENT:
        Beep(900, 200);
        printf("Ctrl-Break event\n\n");
        stop = true;
        return FALSE;

    case CTRL_LOGOFF_EVENT:
        Beep(1000, 200);
        printf("Ctrl-Logoff event\n\n");
        stop = true;
        return FALSE;

    case CTRL_SHUTDOWN_EVENT:
        Beep(750, 500);
        printf("Ctrl-Shutdown event\n\n");
        stop = true;
        return FALSE;

    default:
        return FALSE;
    }
}

int main()
{
    if (SetConsoleCtrlHandler((PHANDLER_ROUTINE)CtrlHandler, TRUE))
    {
        fprintf(stdout, "Starting\n");
        Sleep(2000);
        fprintf(stdout, "Started\n");
        int i = 1;
        for (;;)
        {
            if (stop)
            {
                break;
            }
            fprintf(stdout, "Iteration (%d)\n", i++);
            Sleep(1000);
        }

        fprintf(stdout, "Stopping\n");
        Sleep(2000);
        fprintf(stdout, "Stopped\n");

        return 0;
    }
    else
    {
        fprintf(stdout, "\nERROR: Could not set control handler");
        return 1;
    }
}

