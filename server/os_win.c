/* ANTINAT
 * =======
 * This software is Copyright (c) 2002-05 Malcolm Smith.
 * No warranty is provided, including but not limited to
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * This code is licenced subject to the GNU General
 * Public Licence (GPL).  See the COPYING file for more.
 */

#include "an_serv.h"
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_IO_H
#include <io.h>
#endif

void
os_thread_init (os_thread_t * thr)
{
	thr->hThread = NULL;
}

void
os_thread_close (os_thread_t * thr)
{
	if (thr->hThread)
		CloseHandle (thr->hThread);
}


void
os_thread_detach (os_thread_t * thr)
{
}

int
os_thread_exec (os_thread_t * thr, void *(*start) (void *), void *arg)
{
	if (thr->hThread)
		CloseHandle (thr->hThread);
	thr->hThread = CreateThread (NULL, 0, (LPTHREAD_START_ROUTINE) start,
								 arg, 0, &thr->tid);
	if (thr->hThread != INVALID_HANDLE_VALUE)
		return -1;
	return 0;
}

void
os_mutex_init (os_mutex_t * lock)
{
	lock->mutex = CreateMutex (NULL, FALSE, NULL);
}

void
os_mutex_close (os_mutex_t * lock)
{
	CloseHandle (lock->mutex);
}

void
os_mutex_lock (os_mutex_t * lock)
{
	WaitForSingleObject (lock->mutex, INFINITE);
}

void
os_mutex_unlock (os_mutex_t * lock)
{
	ReleaseMutex (lock->mutex);
}

int
os_pipe (int *ends)
{
	return _pipe (ends, 1024, O_BINARY);
}

#ifdef WITH_DEBUG
void
os_debug_log (const char *filename, const char *msg)
{
	char szTemp[2048];
	sprintf (szTemp, "%s: %s", filename, msg);
	MessageBox (NULL, szTemp, "Antinat", 64);
}
#endif
