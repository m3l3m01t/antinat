/* ANTINAT
 * =======
 * This software is Copyright (c) 2002-04 Malcolm Smith.
 * No warranty is provided, including but not limited to
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * This code is licenced subject to the GNU General
 * Public Licence (GPL).  See the COPYING file for more.
 */

#include "an_serv.h"
#ifdef HAVE_PTHREAD_H
#include <pthread.h>
#endif

void
os_thread_init (os_thread_t * thr)
{
}

void
os_thread_close (os_thread_t * thr)
{
}


void
os_thread_detach (os_thread_t * thr)
{
	pthread_detach (thr->tid);
}

int
os_thread_exec (os_thread_t * thr, void *(*start) (void *), void *arg)
{
	pthread_attr_t atts;
	pthread_attr_init (&atts);
#ifdef HAVE_PTHREAD_ATTR_SETSCOPE
	pthread_attr_setscope (&atts, PTHREAD_SCOPE_SYSTEM);
#endif
#ifdef HAVE_PTHREAD_ATTR_SETSTACKSIZE
	pthread_attr_setstacksize (&atts, 256 * 1024);
#endif
	return !pthread_create (&thr->tid, &atts, start, arg);
}

void
os_mutex_init (os_mutex_t * lock)
{
	pthread_mutex_init (&lock->mutex, NULL);
}

void
os_mutex_close (os_mutex_t * lock)
{
	pthread_mutex_destroy (&lock->mutex);
}

void
os_mutex_lock (os_mutex_t * lock)
{
	pthread_mutex_lock (&lock->mutex);
}

void
os_mutex_unlock (os_mutex_t * lock)
{
	pthread_mutex_unlock (&lock->mutex);
}

int
os_pipe (int *ends)
{
	return pipe (ends);
}

#ifdef WITH_DEBUG
void
os_debug_log (const char *filename, int line, const char *msg)
{
	fprintf (stderr, "%s - %d: %s\n", filename, line, msg);
}
#endif
