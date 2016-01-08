/* ANTINAT
 * =======
 * This software is Copyright (c) 2003-05 Malcolm Smith.
 * No warranty is provided, including but not limited to
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * This code is licenced subject to the GNU General
 * Public Licence (GPL).  See the COPYING file for more.
 */

#include "an_serv.h"
#ifdef HAVE_SIGNAL_H
#include <signal.h>
#endif
#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif
#ifdef HAVE_IO_H
#include <io.h>
#endif
#include <stdlib.h>
#include <stdio.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef _WIN32_
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#include <fcntl.h>

os_mutex_t crypt_lock;
os_mutex_t getpwnam_lock;
os_mutex_t getspnam_lock;
os_mutex_t localtime_lock;
os_mutex_t writerpipe_lock;

SOCKET srv = INVALID_SOCKET;
config_t *conf = NULL;
//char *config_filename = sysconfdir "/antinat.xml";
char *config_filename = NULL;
const char *pid_filename = "/var/run/antinat.pid";
#ifndef _WIN32_
BOOL runAsDaemon = TRUE;
#endif
#ifdef _WIN32_
BOOL runAsApplication = FALSE;
#endif

static BOOL
linger (SOCKET s)
{
	struct linger lyes;
	/*
	   Linger - if the socket is closed, ensure that data is sent/
	   received right up to the last byte.  Don't stop just because
	   the connection is closed.
	 */
	lyes.l_onoff = 1;
	lyes.l_linger = 10;
	setsockopt (s, SOL_SOCKET, SO_LINGER, (char *) &lyes, sizeof (lyes));
	return TRUE;
}

static BOOL
startServer (unsigned short nPort, unsigned long nIP)
{
	SOCKADDR_IN sin;
	int ret;
	unsigned int yes;

	yes = TRUE;

	/* Grab a place to listen for connections. */
	srv = socket (AF_INET, SOCK_STREAM, 0);
	if (srv < 0) {
		return FALSE;
	}

	linger (srv);

	/* If this server has been restarted, don't wait for the old
	 * one to disappear completely */
	setsockopt (srv, SOL_SOCKET, SO_REUSEADDR, (char *) &yes, sizeof (yes));

	memset (&sin, 0, sizeof (sin));
	sin.sin_family = AF_INET;
	sin.sin_port = htons (nPort);

	sin.sin_addr.s_addr = nIP;
	ret = bind (srv, (SOCKADDR *) & sin, sizeof (sin));
	if (ret < 0) {
		return FALSE;
	}

	/* Start listening for incoming connections. */
	ret = listen (srv, 5);
	if (ret == 0)
		return TRUE;
	return FALSE;
}

/*
This big messy function processes each incoming request.
*/
BOOL
HandleRequest ()
{
	SOCKET cli;
	conn_t *conn;
	os_thread_t thr;
	os_thread_init (&thr);

	/* Accept an incoming request, or wait till one arrives. */
	cli = accept (srv, NULL, NULL);
	if (cli == INVALID_SOCKET) {
#ifdef WITH_DEBUG
		DEBUG_LOG ("Accept barfed");
#endif
		return FALSE;
	}
	linger (cli);
	config_reference (conf);
	conn = (conn_t *) malloc (sizeof (conn_t));
	if (conn) {
		conn_init_tcp (conn, conf, cli);

		if (os_thread_exec (&thr, ChildThread, conn)) {
			os_thread_detach (&thr);
			return TRUE;
		}
		conn_close (conn);
	}
	closesocket (cli);
	config_dereference (conf);

#ifdef WITH_DEBUG
	DEBUG_LOG ("Couldn't allocate memory or create thread");
#endif

	return FALSE;
}

#ifndef _WIN32_
void
daemonize ()
{
	int fh;
	pid_t pid;
	if ((pid = fork ()) != 0) {
		if (pid != -1) {
			FILE *fp;

			unlink(pid_filename);
			if ((fp = fopen(pid_filename, "w")) != NULL) {
				fprintf(fp, "%ld", pid);
				fclose(fp);
			}
		}
		exit (EXIT_SUCCESS);
	}
	fh = open ("/dev/null", O_RDWR, 0666);
	close (1);
	close (2);
	dup2 (fh, 1);
	dup2 (fh, 2);
#ifdef HAVE_SETSID
	setsid ();
#endif
}


/*
UNIX trivia - when is a problem a problem?  When you don't ignore it.
If you do nothing, well, you're not being ignorant enough.  You have
to be explicitly ignorant.
*/
void
ignorer (int x)
{
	signal (x, ignorer);
}
#endif

void closeup (int x);

void
reloadconfig (int x)
{
	unsigned long ip;
	unsigned short port;
	if (conf) {
		ip = config_getInterface (conf);
		port = config_getPort (conf);
		config_dereference (conf);
	} else {
		/* Must be invalid */
		ip = 0;
		port = 0;
	}
	conf = (config_t *) malloc (sizeof (config_t));
	if (!loadconfig (conf, config_filename)) {
#ifdef _WIN32_
		MessageBox (NULL, "Could not open configuration file.", "Antinat",
					48);
#else
		printf ("Could not open configuration file.");
#endif
		free (conf);
		exit (EXIT_FAILURE);
	}
	log_log (NULL, LOG_EVT_SERVERRESTART, 0, conf);
	if ((config_getPort (conf) != port) || (config_getInterface (conf) != ip)) {
		if (srv != INVALID_SOCKET)
			closesocket (srv);
		srv = INVALID_SOCKET;
		if (!startServer
			((unsigned short) config_getPort (conf),
			 (unsigned int) config_getInterface (conf))) {
#ifndef _WIN32_
			printf ("Could not listen on interface/port\n");
#else
			MessageBox (NULL, "Could not listen on interface/port", "Antinat",
						16);
#endif
			exit (EXIT_FAILURE);
		}
	}
#ifndef _WIN32_
	signal (x, reloadconfig);
#endif
}

#ifndef _WIN32_
void
kidkiller (int x)
{
	int ret;
#ifdef WITH_DEBUG
	DEBUG_LOG ("It was dead already, honest...");
#endif
	wait (&ret);
	signal (x, kidkiller);
}
#endif

void
closeup (int x)
{
#ifdef WITH_DEBUG
	DEBUG_LOG ("Closing up");
#endif
	log_log (NULL, LOG_EVT_SERVERCLOSE, 0, conf);
	config_dereference (conf);
	/* FIXME: really want to wait for threads to finish properly */
	sleep (2);					/* Give logging threads a chance */
	exit (EXIT_SUCCESS);
}

int
realapp ()
{
	os_mutex_init (&crypt_lock);
	os_mutex_init (&getpwnam_lock);
	os_mutex_init (&getspnam_lock);
	os_mutex_init (&localtime_lock);
	os_mutex_init (&writerpipe_lock);
#ifndef _WIN32_
	reloadconfig (SIGHUP);
	signal (SIGCHLD, kidkiller);
	signal (SIGPIPE, ignorer);
	signal (SIGQUIT, closeup);
	signal (SIGINT, closeup);
#else
	reloadconfig (0);
#endif
#ifndef _WIN32_
	if (runAsDaemon)
		daemonize ();
#endif
	log_log (NULL, LOG_EVT_SERVERSTART, 0, conf);
	while (TRUE) {
		if (!HandleRequest ()) {
#ifdef WITH_DEBUG
			DEBUG_LOG ("Couldn't handle request.");
#endif
		}
	}
	return EXIT_SUCCESS;
}

void
displayHelp ()
{
	char szBuffer[2048];
	sprintf (szBuffer,
			 "Antinat version %s\n\n"
			 "Usage: antinat [-h] [-i|-r|-a] [-cConfigFile] [-x]\n"
			 "-h - This help screen\n"
			 "-c - Specify the location of the configuration file\n"
			 "-p - Specify the location of the pid file\n"
			 "WIN32 ONLY:\n"
			 "-a - Run as application (rather than service)\n"
			 "-i - Install as service\n"
			 "-r - Remove service\n"
			 "UNIX/LINUX ONLY:\n"
			 "-x - Remain on console, do not daemonize\n", AN_VER);
#ifndef _WIN32_
	printf ("%s", szBuffer);
#else
	MessageBox (NULL, szBuffer, "Antinat", 48);
#endif
}

#ifdef _WIN32_
VOID InstallService (LPCTSTR);
VOID RemoveService (LPCTSTR);


void
process_args (DSParams * param)
{
	char *value;

	if (ds_hash_getNumericValue_n (&param->hsh, 'h')) {
		displayHelp ();
		exit (EXIT_SUCCESS);
	}
	if ((value = ds_hash_getPtrValue_n (&param->hsh, 'p'))) {
		pid_filename = strdup(value);
	}

	if ((value = ds_hash_getPtrValue_n (&param->hsh, 'c'))) {
		config_filename =
			(char *)
			malloc (strlen ((char *) ds_hash_getPtrValue_n (&param->hsh, 'c'))
					+ 1);
		strcpy (config_filename,
				(char *) ds_hash_getPtrValue_n (&param->hsh, 'c'));
	} else {
		config_filename = NULL;
	}
#ifndef WITH_DEBUG
#ifndef _WIN32_
	if (!ds_hash_getNumericValue_n (&param->hsh, 'x'))
		runAsDaemon = TRUE;
#endif
#endif
#ifdef _WIN32_
	if (ds_hash_getNumericValue_n (&param->hsh, 'i')) {
		InstallService ("Antinat");
		exit (EXIT_SUCCESS);
	}
	if (ds_hash_getNumericValue_n (&param->hsh, 'r')) {
		RemoveService ("Antinat");
		exit (EXIT_SUCCESS);
	}
	if (ds_hash_getNumericValue_n (&param->hsh, 'a')) {
		runAsApplication = TRUE;
	}
#endif
}
#else
void	getopt_process_args(int argc, char *argv[])
{
	int opt;
	while ((opt = getopt(argc, argv, "xc:p:")) != -1) {
		switch(opt) {
			case 'x':
				runAsDaemon = FALSE;
				break;
			case 'c':
				config_filename = strdup(optarg);
				break;
			case 'p':
				pid_filename = strdup(optarg);
				break;
			case 'h':
				displayHelp ();
				exit (EXIT_SUCCESS);
				break;
			default:
				displayHelp ();
				exit (EXIT_FAILURE);
		}
	}
}
#endif

/*
main() - you've seen that before.
*/
#ifndef _WIN32_
int
main (int argc, char *argv[])
{
	getopt_process_args(argc, argv);

	return realapp ();
}

#else

HANDLE hServDoneEvent = NULL;
SERVICE_STATUS ssStatus;

SERVICE_STATUS_HANDLE sshStatusHandle;
os_thread_t threadHandle;


BOOL
ReportStatusToSCMgr (DWORD dwCurrentState,
					 DWORD dwWin32ExitCode,
					 DWORD dwCheckPoint, DWORD dwWaitHint)
{
	BOOL fResult;

	if (dwCurrentState == SERVICE_START_PENDING)
		ssStatus.dwControlsAccepted = 0;
	else
		ssStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP |
			SERVICE_ACCEPT_PAUSE_CONTINUE;

	ssStatus.dwCurrentState = dwCurrentState;
	ssStatus.dwWin32ExitCode = dwWin32ExitCode;
	ssStatus.dwCheckPoint = dwCheckPoint;

	ssStatus.dwWaitHint = dwWaitHint;

	if (!(fResult = SetServiceStatus (sshStatusHandle, &ssStatus)))
		SetEvent (hServDoneEvent);

	return fResult;
}


VOID
service_ctrl (DWORD dwCtrlCode)
{
	DWORD dwState = SERVICE_RUNNING;
	switch (dwCtrlCode) {
	case SERVICE_CONTROL_STOP:
		dwState = SERVICE_STOP_PENDING;
		ReportStatusToSCMgr (SERVICE_STOP_PENDING, NO_ERROR, 1, 3000);
		SetEvent (hServDoneEvent);
		return;
	case SERVICE_CONTROL_INTERROGATE:
		break;
	default:
		break;

	}
	ReportStatusToSCMgr (dwState, NO_ERROR, 0, 0);
}

VOID *
worker_thread (VOID * notUsed)
{
	realapp ();
	return NULL;
}

VOID
service_main (DWORD dwArgc, LPTSTR * lpszArgv)
{
	DWORD dwWait;

	sshStatusHandle = RegisterServiceCtrlHandler (TEXT ("Antinat"),
												  (LPHANDLER_FUNCTION)
												  service_ctrl);

	if (!sshStatusHandle)
		goto cleanup;

	ssStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
	ssStatus.dwServiceSpecificExitCode = 0;


	if (!ReportStatusToSCMgr (SERVICE_START_PENDING, NO_ERROR, 1, 1000))
		goto cleanup;

	hServDoneEvent = CreateEvent (NULL, TRUE, FALSE, NULL);

	if (hServDoneEvent == (HANDLE) NULL)
		goto cleanup;

	if (!ReportStatusToSCMgr (SERVICE_START_PENDING, NO_ERROR, 2, 1000))
		goto cleanup;

	os_thread_init (&threadHandle);
	if (!os_thread_exec (&threadHandle, worker_thread, NULL))
		goto cleanup;


	if (!ReportStatusToSCMgr (SERVICE_RUNNING, NO_ERROR, 0, 0))
		goto cleanup;

	dwWait = WaitForSingleObject (hServDoneEvent, INFINITE);

  cleanup:

	if (hServDoneEvent != NULL)
		CloseHandle (hServDoneEvent);

	if (sshStatusHandle != 0)
		(VOID) ReportStatusToSCMgr (SERVICE_STOPPED, 0, 0, 0);

	return;
}

VOID
InstallService (LPCTSTR serviceName)
{
	CHAR lpszBinaryPathName[MAX_PATH];
	SC_HANDLE schSCManager;
	SC_HANDLE schService;

	if (GetModuleFileName
		(NULL, lpszBinaryPathName, sizeof (lpszBinaryPathName)) == 0)
		return;

	schSCManager = OpenSCManager (NULL, NULL, SC_MANAGER_ALL_ACCESS);


	schService = CreateService (schSCManager,
								serviceName,
								serviceName,
								SERVICE_ALL_ACCESS,
								SERVICE_WIN32_OWN_PROCESS,
								SERVICE_DEMAND_START,
								SERVICE_ERROR_NORMAL,
								lpszBinaryPathName,
								NULL, NULL, NULL, NULL, NULL);

	CloseServiceHandle (schService);
}

VOID
RemoveService (LPCTSTR serviceName)
{
	BOOL ret;
	SC_HANDLE schService;
	SC_HANDLE schSCManager;

	schSCManager = OpenSCManager (NULL, NULL, SC_MANAGER_ALL_ACCESS);


	schService = OpenService (schSCManager, serviceName, SERVICE_ALL_ACCESS);

	ret = DeleteService (schService);
	CloseServiceHandle (schSCManager);
}



int WINAPI
WinMain (HINSTANCE hInst, HINSTANCE hPrev, LPSTR lpszCmdLine, int nCmdShow)
{
	DSParams param;
	WSADATA wd;
	ds_param_init (&param);
	ds_param_setFlagSwitch (&param, 'i');
	ds_param_setFlagSwitch (&param, 'r');
	ds_param_setFlagSwitch (&param, 'a');
	ds_param_setFlagSwitch (&param, 'h');
	ds_param_setStringSwitch (&param, 'c');
	ds_param_process_str (&param, lpszCmdLine);
	process_args (&param);
	ds_param_close (&param);

	if (config_filename == NULL) {
		HKEY regKey;
		if ((RegOpenKeyEx
			 (HKEY_CURRENT_USER, "SOFTWARE\\Antinat", 0, KEY_READ,
			  &regKey) == ERROR_SUCCESS)
			||
			(RegOpenKeyEx
			 (HKEY_LOCAL_MACHINE, "SOFTWARE\\Antinat", 0, KEY_READ,
			  &regKey) == ERROR_SUCCESS)) {
			DWORD regType;
			DWORD regSize;
			config_filename = (LPSTR) malloc (MAX_PATH);
			regSize = MAX_PATH;
			if ((RegQueryValueEx
				 (regKey, "antinat.xml", 0, &regType,
				  (LPBYTE) config_filename, &regSize) != ERROR_SUCCESS)
				|| (regType != REG_SZ)) {
				free (config_filename);
				config_filename = NULL;
			}
			RegCloseKey (regKey);

		}
	}

	WSAStartup (MAKEWORD (1, 1), &wd);

	if (runAsApplication) {
		return realapp ();
	} else {
		SERVICE_TABLE_ENTRY dispatchTable[] = {
			{TEXT ("Antinat"), (LPSERVICE_MAIN_FUNCTION) service_main}
			,
			{NULL, NULL}
		};

		if (!StartServiceCtrlDispatcher (dispatchTable)) {
			SetEvent (hServDoneEvent);
		}
		return 0;
	}
}

#endif
