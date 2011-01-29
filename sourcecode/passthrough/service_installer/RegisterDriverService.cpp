// RegisterDriverService.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

void ErrorExit(LPTSTR lpszFunction) 
{ 
    TCHAR szBuf[80]; 
    LPVOID lpMsgBuf;
    DWORD dw = GetLastError(); 

    FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | 
        FORMAT_MESSAGE_FROM_SYSTEM,
        NULL,
        dw,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPTSTR) &lpMsgBuf,
        0, NULL );

    printf("%s failed with error %d: %s", lpszFunction, dw, lpMsgBuf); 
 
    //MessageBox(NULL, szBuf, "Error", MB_OK); 

    LocalFree(lpMsgBuf);
    //ExitProcess(dw); 
}

int _tmain(int argc, _TCHAR* argv[])
{
	// some generic return value variables
	BOOL success;
	DWORD lastError;
	LPTSTR thisProgram;
	
	// security access variable
	DWORD dwDesiredAccess;

	// for OpenSCManager
	LPCTSTR lpMachineName;
	LPCTSTR lpDatabaseName;
	SC_HANDLE scmHandle;

	// for OpenService
	LPSTR lpServiceName;
	SC_HANDLE scmMyService;

	// for CreateService
	LPSTR lpDisplayName;
	DWORD dwServiceType;
	DWORD dwStartType;
	DWORD dwErrorControl;
	LPCTSTR lpBinaryPathName;
	LPCTSTR lpLoadOrderGroup;
	LPDWORD lpdwTagId;
	LPCTSTR lpDependencies;
	LPCTSTR lpServiceStartName;
	LPCTSTR lpPassword;


	if(argc != 3) {
		cout << "Usage: rds <servicename> <.sys file>\n";
		cout << "<.sys file> = System32\\Drivers\\<filename>.sys\n";
		return 1;
	}


	thisProgram = "RegisterDriverService";
	// open the service control manager
	lpMachineName = "\0";							// implicit local machine
	lpDatabaseName = SERVICES_ACTIVE_DATABASE;		// required for the call
	dwDesiredAccess = SC_MANAGER_CREATE_SERVICE;	// need admin priv's eventually
	scmHandle = OpenSCManager(lpMachineName, lpDatabaseName, dwDesiredAccess);
	
	if(!scmHandle) {
		cout << "Failed to open the service control manager.\n";
		lastError = GetLastError();

		switch(lastError) {
			case ERROR_ACCESS_DENIED: 
				cout << "Access to the service control manager was denied.\n";
				break;
			case ERROR_DATABASE_DOES_NOT_EXIST: 
				cout << "Registry database does not exist.\n";
				break;
			case ERROR_INVALID_PARAMETER: 
				cout << "Invalid parameter passed to OpenSCManager.\n";
				break;
			default:
				cout << "Unknown error. Exiting.\n";
		}
		success = CloseServiceHandle(scmHandle);
		return 1;
	} else {
		cout << "Successfully opened the service control manager.\n";
	}

	// look for this service already existing, so we don't step on it
	dwDesiredAccess = SERVICE_INTERROGATE;
	lpServiceName = argv[1];
	scmMyService = OpenService(scmHandle, lpServiceName, dwDesiredAccess);

	if(scmMyService) {
		// success, service exists, bail out
		cout << "The specified service name already exists.\n";
		
		success = CloseServiceHandle(scmMyService);
		success = CloseServiceHandle(scmHandle);
		return 1;
	} else {
		// error, check to make sure it's the right error
		lastError = GetLastError();

		// close the service handle no matter what
		success = CloseServiceHandle(scmMyService);

		// assert the service does not already exist as the error
		if(lastError != ERROR_SERVICE_DOES_NOT_EXIST) {
			success = CloseServiceHandle(scmHandle);
			ErrorExit(thisProgram);
			return 1;
		}
	}

	lpDisplayName = argv[1];
	dwDesiredAccess = SC_MANAGER_CREATE_SERVICE;
	dwServiceType = SERVICE_KERNEL_DRIVER;
	dwStartType = SERVICE_SYSTEM_START;
	dwErrorControl = SERVICE_ERROR_NORMAL;
	lpBinaryPathName = argv[2];
	lpLoadOrderGroup = NULL;
	lpdwTagId = NULL;
	lpDependencies = NULL;
	lpServiceStartName = NULL;
	lpPassword = NULL;

	scmMyService = CreateService(scmHandle, lpServiceName, lpDisplayName, dwDesiredAccess, dwServiceType, dwStartType, dwErrorControl, lpBinaryPathName, lpLoadOrderGroup, lpdwTagId, lpDependencies, lpServiceStartName, lpPassword);
	if(!scmMyService) {
		cout << "Hello.";
	} else {
		cout << "The service was successfully added.\n";
	}
	

	// clean up handles
	success = CloseServiceHandle(scmHandle);
	success = CloseServiceHandle(scmMyService);

	return 0;
}

