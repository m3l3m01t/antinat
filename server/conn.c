/* ANTINAT
 * =======
 * This software is Copyright (c) 2003-05 Malcolm Smith.
 * No warranty is provided, including but not limited to
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * This code is licenced subject to the GNU General
 * Public Licence (GPL).  See the COPYING file for more.
 */

#include "an_serv.h"
#include <time.h>
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_IO_H
#include <io.h>
#endif
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef WITH_MEMCACHED
#include <libmemcached/memcached.h>
#endif

#include <antinat.h>

#ifndef _WIN32_
int writerPipe = -1;
#endif

#ifdef WITH_MYSQL
#include <mysql.h>

const char *memcached_opt = "--SERVER=127.0.0.1:11211";
//const char *memcached_opt = "--SOCKET=/var/run/memcached/memcached.socket";

void
conn_init_mysql(conn_t *conn)
{
	my_bool reconnect = 1;
	mysql_init(&conn->mysql);

    mysql_options(&conn->mysql, MYSQL_OPT_RECONNECT, &reconnect);

#ifdef WITH_MEMCACHED
	if ((conn->memc = memcached(memcached_opt, strlen(memcached_opt)))) {
		memcached_behavior_set(conn->memc, MEMCACHED_BEHAVIOR_BINARY_PROTOCOL, 1);
	}
#endif
}

void
conn_close_mysql(conn_t *conn)
{
	mysql_close(&conn->mysql);
#ifdef WITH_MEMCACHED
	if (conn->memc)
		conn_free_memc(conn);
#endif
}
#endif /* WITH_MYSQL */

void
conn_init_tcp (conn_t * conn, config_t * theconf, SOCKET thiss)
{
	PI_SA sa;
	sl_t sl;
	conn->s = thiss;
	conn->conf = theconf;
	conn->user = NULL;
	conn->pass = NULL;

	conn->buffer = NULL;
	conn->bufflen = 0;

	conn->version = 0;
	conn->authscheme = 0;
	conn->authsrc = 0;

#ifdef WITH_MYSQL
	conn_init_mysql(conn);
#endif

	ai_init (&conn->source, NULL);
	ai_init (&conn->dest, NULL);
	ai_init (&conn->source_on_server, NULL);

	sl = sizeof (sa);
	if (getpeername (conn->s, (SOCKADDR *) & sa, &sl) == 0) {
		ai_setAddress_sa (&conn->source, (SOCKADDR *) & sa);
	}
	if (getsockname (conn->s, (SOCKADDR *) & sa, &sl) == 0) {
		ai_setAddress_sa (&conn->source_on_server, (SOCKADDR *) & sa);
	}

	conn->nExternalSendBytes = 0;
	conn->nExternalRecvBytes = 0;
}

void
conn_init_udp (conn_t * conn, config_t * theconf, SOCKADDR * sa,
			   char *udppacket, int udplen)
{
	conn->s = 0;
	conn->user = NULL;
	conn->pass = NULL;
	conn->conf = theconf;

	conn->version = 0;
	conn->authscheme = 0;
	conn->authsrc = 0;

#ifdef WITH_MYSQL
	conn_init_mysql(conn);
#endif

	ai_init (&conn->source, NULL);
	ai_init (&conn->dest, NULL);
	ai_init (&conn->source_on_server, NULL);

	conn->buffer = udppacket;
	conn->bufflen = udplen;
	conn->buff_upto = 0;

	ai_setAddress_sa (&conn->source, sa);

	conn->nExternalSendBytes = 0;
	conn->nExternalRecvBytes = 0;
}

void
conn_close (conn_t * conn)
{
	ai_close (&conn->source);
	ai_close (&conn->source_on_server);
	ai_close (&conn->dest);

	if (conn->user)
		free (conn->user);
	if (conn->pass)
		free (conn->pass);

	conn->user = conn->pass = NULL;

	conn_close_mysql(conn);

	free (conn);
}

void
conn_setUser (conn_t * conn, char *newuser)
{
	if (conn->user)
		free (conn->user);
	if (newuser == NULL) {
		conn->user = NULL;
	} else {
		conn->user = (char *) malloc (strlen (newuser) + 1);
		strcpy (conn->user, newuser);
	}
}

void
conn_setPass (conn_t * conn, char *newpass)
{
	if (conn->pass)
		free (conn->pass);
	if (newpass == NULL) {
		conn->pass = NULL;
	} else {
		conn->pass = (char *) malloc (strlen (newpass) + 1);
		strcpy (conn->pass, newpass);
	}
}

BOOL
conn_setDestHostname (conn_t * conn, char *host)
{
	HOSTENT *phe;
	HOSTENT realhe;
	char buf[1024];
	int perrno = 0;
	an_gethostbyname (host, &realhe, buf, sizeof (buf), &phe, &perrno);
	if (!phe) {
#ifdef WITH_DEBUG
		DEBUG_LOG ("Failed to resolve hostname");
#endif
		return FALSE;
	}

	conn->dest.address_type = phe->h_addrtype;
	ai_setAddress_str (&conn->dest, phe->h_addr_list[0], phe->h_length);
	return TRUE;
}

BOOL
conn_sendData (conn_t * conn, const char *szData, int nSize)
{
	if (conn->buffer) {
		int wanttosend;
		wanttosend = nSize;
		if (conn->buff_upto + wanttosend >= conn->bufflen) {
			wanttosend = conn->bufflen - conn->buff_upto;
		}
		memcpy (&conn->buffer[conn->buff_upto], szData, wanttosend);
		conn->buff_upto += wanttosend;
		if (nSize == wanttosend)
			return TRUE;
		return FALSE;
	} else {
		int ret;
		if (nSize < 0) {
			nSize = strlen (szData);
		}
		ret = send (conn->s, szData, nSize, 0);
		if (ret == nSize)
			return TRUE;
		return FALSE;
	}
}

static BOOLX
conn_waitForDataEx (SOCKET s, int sec, int usec)
{
	int ret;
	fd_set readset;
	struct timeval timeout;

	FD_ZERO (&readset);
	FD_SET (s, &readset);
	if (sec >= 0) {
		timeout.tv_sec = sec;
		timeout.tv_usec = usec;
		ret = select (s + 1, &readset, NULL, NULL, &timeout);
	} else {
		ret = select (s + 1, &readset, NULL, NULL, NULL);
	}
	if (ret > 0)
		return YES;
	if (ret == 0)
		return NOTYET;
#ifndef _WIN32_
	if (errno == EINTR)
		return NOTYET;
#endif
	return NO;
}

BOOL
conn_getChar (conn_t * conn, unsigned char *ch)
{
	if (conn->buffer) {
		/* UDP */
		if (conn->buff_upto < conn->bufflen) {
			*ch = conn->buffer[conn->buff_upto];
			conn->buff_upto++;
			return TRUE;
		}
		return FALSE;
	} else {
		/* TCP */
		int len;
		if (conn_waitForDataEx (conn->s, 60 * 5, 0) != YES)
			return FALSE;
		len = recv (conn->s, (char *) ch, 1, 0);
		if (len > 0)
			return TRUE;
		return FALSE;
	}
}

BOOL
conn_getSlab (conn_t * conn, char *szBuffer, int nSize)
{
	if (conn->buffer) {
		/* UDP */
		int wanttoread;
		wanttoread = nSize;
		if ((conn->buff_upto + wanttoread) >= conn->bufflen) {
			wanttoread = conn->bufflen - conn->buff_upto;
		}
		memcpy (szBuffer, &conn->buffer[conn->buff_upto], wanttoread);
		conn->buff_upto += wanttoread;
		if (nSize == wanttoread)
			return TRUE;
		return FALSE;
	} else {
		int len;
		if (nSize <= 0)
			return FALSE;

		do {
			if (conn_waitForDataEx (conn->s, 60 * 5, 0) != YES)
				return FALSE;
			len = recv (conn->s, szBuffer, nSize, 0);
			szBuffer[len] = '\0';
			szBuffer += len;
			nSize -= len;
		} while ((nSize > 0) && (len > 0));

		if (nSize == 0)
			return TRUE;
		return FALSE;
	}
}


typedef struct st_newConnInfo {
	conn_t *local;
	ANCONN remote;
	struct st_newConnInfo *next;
} newConnInfo;

typedef struct st_forwarderData {
#ifndef _WIN32_
	int readerPipe;
	int writerPipe;
#endif
	newConnInfo *init;
} forwarderData;

#ifndef _WIN32_
void *conn_forwarderThread (void *);

static BOOL
conn_createForwarderThread (conn_t * conn, ANCONN two)
{
	newConnInfo *ci;
	forwarderData *fd;
	int piperes[2];
	os_thread_t thr;

	if (os_pipe (piperes) != 0) {
		return FALSE;
	}
	fd = malloc (sizeof (forwarderData));
	if (!fd) {
		close (piperes[0]);
		close (piperes[1]);
		free (fd);
		return FALSE;
	}
	ci = malloc (sizeof (newConnInfo));
	if (!ci) {
		close (piperes[0]);
		close (piperes[1]);
		free (fd);
		free (ci);
		return FALSE;
	}
	ci->local = conn;
	ci->remote = two;
	ci->next = NULL;
	fd->init = ci;
	fd->readerPipe = piperes[0];
	fd->writerPipe = piperes[1];
	/* Should be already locked */
	writerPipe = piperes[1];

#ifdef WITH_DEBUG
	DEBUG_LOG ("Attempting to create new thread");
#endif
	os_thread_init (&thr);
	if (os_thread_exec (&thr, conn_forwarderThread, fd)) {
		os_thread_detach (&thr);
		return TRUE;
	}

	close (piperes[0]);
	close (piperes[1]);
	free (fd);
	free (ci);
	return FALSE;
}
#endif

void *
conn_forwarderThread (void *data)
{
	forwarderData *mydata;
	newConnInfo *head;
	newConnInfo *current;
	newConnInfo *prev;
	int nConns;
	int maxConns;

	SOCKET biggest;
	fd_set fds;
	fd_set wait_q;
	int nActive;
	BOOL bCloseConn;
	BOOL bHaveThrottle;
	char szBuffer[32768];
	int len;
	int readlen;
	int currentrate;
	time_t now;
	struct timeval tv;
#ifdef WITH_DEBUG
	char szDebug[300];
#endif

#ifdef WITH_DEBUG
	DEBUG_LOG ("Created new forwarder thread");
#endif

	mydata = (forwarderData *) data;
	nConns = 1;
#ifndef _WIN32_
	maxConns = config_getMaxConnsPerThread (mydata->init->local->conf);
#else
	maxConns = 1;
#endif

	currentrate = 0;			/* Compiler shutup */
	bHaveThrottle = FALSE;

	head = mydata->init;
	head->next = NULL;
	FD_ZERO (&wait_q);

	while (1) {
		FD_ZERO (&fds);
#ifndef _WIN32_
		biggest = mydata->readerPipe;
		FD_SET (mydata->readerPipe, &fds);
#else
		biggest = 0;
#endif
		for (current = head; current; current = current->next) {
			/* If it's on the wait queue, don't select on it, because
			 * we know there's something to be read already, and didn't
			 * want to read it due to a throttle. */
			if (!FD_ISSET (current->local->s, &wait_q)) {
				if (current->local->s > biggest)
					biggest = current->local->s;
				FD_SET (current->local->s, &fds);
				biggest = AN_FD_SET (current->remote, &fds, biggest);
			}
		}
		biggest++;
		/* Wait for message from either side of any connection plus our
		 * special signalling pipe (non-Win32 only).  Have a timeout to
		 * handle throttled connections; we don't check if there's something
		 * to be read on those, but we do check every second whether we
		 * should start handling them again. */
		if (bHaveThrottle) {
			tv.tv_sec = 1;
			tv.tv_usec = 0;
			nActive = select (biggest, &fds, NULL, NULL, &tv);
		} else {
			/* No throttle, hang indefinitely. */
			nActive = select (biggest, &fds, NULL, NULL, NULL);
		}
		time (&now);
		FD_ZERO (&wait_q);
		bHaveThrottle = 0;

#ifndef _WIN32_
		if (FD_ISSET (mydata->readerPipe, &fds)) {
#ifdef WITH_DEBUG
			DEBUG_LOG ("Activity on new connection monitor");
#endif
			len = read (mydata->readerPipe, szBuffer, sizeof (newConnInfo));
			/* What if it comes in bits?  Need to handle this - FIXME */
#ifdef WITH_DEBUG
			sprintf (szDebug, " Got %i bytes, wanted %i bytes", len,
					 (int) sizeof (newConnInfo));
			DEBUG_LOG (szDebug);
#endif
			if (len == sizeof (newConnInfo)) {
				current = malloc (sizeof (newConnInfo));
				memcpy (current, szBuffer, sizeof (newConnInfo));
				if (nConns >= maxConns) {
					os_mutex_lock (&writerpipe_lock);
					conn_createForwarderThread (current->local,
												current->remote);
					os_mutex_unlock (&writerpipe_lock);
				} else {
					current->next = head;
					head = current;
					nConns++;
#ifdef WITH_DEBUG
					DEBUG_LOG ("Added new connection to forwarder thread");
#endif
				}
			}
		}
#endif

		prev = NULL;
		for (current = head; current;) {
			bCloseConn = FALSE;
			/* Is it throttled? */
			if (current->local->throttle) {
				if (now == current->local->startTime) {
					currentrate = (current->local->nExternalSendBytes +
								   current->local->nExternalRecvBytes);
				} else {
					currentrate = (current->local->nExternalSendBytes +
								   current->local->nExternalRecvBytes) /
						(now - current->local->startTime);
#ifdef WITH_DEBUG
					sprintf(szDebug, "currentrate is now %i",currentrate);
					DEBUG_LOG(szDebug);
#endif
				}
			}
			if (current->local->throttle
				&& currentrate >= current->local->throttle) {
				/* Only need one because all we're checking for above is that
				 * particular connection; we assume both shouldn't be added
				 * to select anyway */
				FD_SET (current->local->s, &wait_q);
				bHaveThrottle = TRUE;
			} else {
				readlen = sizeof (szBuffer) - 1;
				if (current->local->throttle)
					if (current->local->throttle < readlen)
						readlen = current->local->throttle;
				if (FD_ISSET (current->local->s, &fds)) {
					/* A message from the LAN, forward to the server */
					len = recv (current->local->s, szBuffer, readlen, 0);
					if (len <= 0) {
#ifdef WITH_DEBUG
						DEBUG_LOG ("Connection closed by local");
#endif
						/* Connection closed */
						bCloseConn = TRUE;
					} else {
						current->local->nExternalSendBytes += len;
						szBuffer[len] = '\0';
						an_send (current->remote, szBuffer, len, 0);
#if 0
						sprintf (szDebug, "Send %i bytes local->remote", len);
						DEBUG_LOG (szDebug);
#endif
					}
				}
				if (AN_FD_ISSET (current->remote, &fds)) {
					/* A message from the server, forward to the LAN */
					len = an_recv (current->remote, szBuffer, readlen, 0);
					if (len <= 0) {
#ifdef WITH_DEBUG
						DEBUG_LOG ("Connection closed by remote");
#endif
						/* Connection closed */
						bCloseConn = TRUE;
					} else {
						current->local->nExternalRecvBytes += len;
						szBuffer[len] = '\0';
						send (current->local->s, szBuffer, len, 0);
#if 0
						sprintf (szDebug, "Send %i bytes remote->local", len);
						DEBUG_LOG (szDebug);
#endif
					}
				}
				if (bCloseConn) {
					an_close (current->remote);
					an_destroy (current->remote);
					closesocket (current->local->s);
					log_log (current->local, LOG_EVT_LOG,
							 LOG_TYPE_CONNECTIONCLOSE, NULL);
#ifdef WITH_DEBUG
					DEBUG_LOG ("Cleaning up from connection");
#endif
					config_dereference (current->local->conf);
					conn_close (current->local);
					if (prev) {
						prev->next = current->next;
					} else {
						head = current->next;
					}
#ifndef _WIN32_
					free (current);
					current = prev;
					nConns--;
					if (nConns == 0) {
						os_mutex_lock (&writerpipe_lock);
						if (mydata->writerPipe != writerPipe) {
							/* No connections, and we're not the active
							 * thread anymore.  This is where we croak.
							 */
							os_mutex_unlock (&writerpipe_lock);
							close (mydata->writerPipe);
							close (mydata->readerPipe);
							free (mydata);
#endif
#ifdef WITH_DEBUG
							DEBUG_LOG ("Cleaned up old forwarder thread");
#endif
							return NULL;
#ifndef _WIN32_
						}
						os_mutex_unlock (&writerpipe_lock);
					}
#endif
				}
			}
			if (!bCloseConn)
				prev = current;
			if (current)
				current = current->next;

		}
	}

	return NULL;
}

BOOL
conn_forwardData (conn_t * conn, ANCONN two)
{
#ifdef _WIN32_
	newConnInfo ci;
	forwarderData fd;
#endif
	time_t now;
#ifdef WITH_DEBUG
	char szDebug[100];

	sprintf (szDebug, "Forwarding connection with throttle of %i",
			 conn->throttle);
	DEBUG_LOG (szDebug);
#endif
	time (&now);
	conn->startTime = now;
#ifdef _WIN32_
	ci.local = conn;
	ci.remote = two;
	ci.next = NULL;
	fd.init = &ci;
	conn_forwarderThread (&fd);
	return TRUE;
#else
#ifdef WITH_DEBUG
	DEBUG_LOG ("Attempting to forward data for connection");
#endif

	os_mutex_lock (&writerpipe_lock);
	if (writerPipe == -1) {
		conn_createForwarderThread (conn, two);
	} else {
		newConnInfo ci;
		ci.local = conn;
		ci.remote = two;
		ci.next = NULL;
		if (write (writerPipe, &ci, sizeof (ci)) < 0) {
			os_mutex_unlock (&writerpipe_lock);
			return FALSE;
		}
	}
	os_mutex_unlock (&writerpipe_lock);
	return TRUE;
#endif
}

BOOL
conn_setupchain (conn_t * conn, ANCONN remote, chain_t * chain)
{
	char *upstreamuser;
	char *upstreampass;
	int i;
#ifdef WITH_DEBUG
	char szDebug[300];
#endif
	if (chain) {
		an_set_proxy_url (remote, chain->uri);
		for (i = 1; i < 32; i++) {
			if (chain->authschemes & (1 << i))
				an_set_authscheme (remote, i);
		}
		if (chain->user)
			upstreamuser = chain->user;
		else
			upstreamuser = conn->user;

		if (chain->pass)
			upstreampass = chain->pass;
		else
			upstreampass = conn->pass;
		an_set_credentials (remote, upstreamuser, upstreampass);
#ifdef WITH_DEBUG
		sprintf (szDebug, "Chaining to %s as %s,%p", chain->uri, upstreamuser,
				 upstreampass);
		DEBUG_LOG (szDebug);
#endif
	} else {
		/* Death to environment variables */
		an_unset_proxy (remote);
	}
	return TRUE;
}

#ifdef WITH_MYSQL

#ifdef WITH_MEMCACHED


#if 0
const char *memcached_opt = "--SERVER=127.0.0.1:11211 --BINARY-PROTOCOL";

memcached_st *
conn_setup_memc(conn_t *conn)
{
	if (conn->memc == NULL) {
		if ((conn->memc = memcached(memcached_opt, strlen(memcached_opt)))) {
			memcached_behavior_set(conn->memc, MEMCACHED_BEHAVIOR_USE_UDP, 1);
			memcached_behavior_set(conn->memc, MEMCACHED_BEHAVIOR_BINARY_PROTOCOL, 1);
		}
	}
	return conn->memc;
}
#endif

#define MYSQL_USER "pi"
#define MYSQL_PASS "raspberry"
#define MYSQL_DB   "dns"

#define TABLE_NAME_DOMAIN  "domain"
#define TABLE_NAME_IP  "ip"

#define TABLE_FIELD_IP "ip"
#define TABLE_FIELD_DNAME "dname"
#define TABLE_FIELD_POLICY "policy"
#define TABLE_FIELD_LVL    "levels"

static policy_type_t
_mysql_query_policy_by_addr(MYSQL *mysql, const char *ipaddr)
{
    policy_type_t policy = POLICY_UNKNOWN;

	MYSQL_RES *results;
	MYSQL_ROW record;
	char sql_statement[512];

#ifdef WITH_DEBUG
	char szDebug[128];
#endif

	if (mysql_real_connect(mysql, NULL, 
				MYSQL_USER, MYSQL_PASS, MYSQL_DB, 0, NULL, 0) == NULL) {
#ifdef WITH_DEBUG
		sprintf (szDebug, "connect to mysql failed(%d):%s\n", mysql_error(mysql));
		DEBUG_LOG (szDebug);
#endif
		return policy;
	}

	snprintf(sql_statement, sizeof(sql_statement) - 1,
			"SELECT " TABLE_FIELD_DNAME "," TABLE_FIELD_POLICY " FROM " TABLE_NAME_IP " WHERE " TABLE_FIELD_IP "='%s'",
			ipaddr);

#ifdef WITH_DEBUG
	sprintf (szDebug, "query ip policy: %s\n", sql_statement);
	DEBUG_LOG (szDebug);
#endif

	if (mysql_query(mysql, sql_statement) != 0) {
		/* query failed */
#ifdef WITH_DEBUG
		sprintf(szDebug, "query ip policy failed:%s\n", mysql_error(mysql));
		DEBUG_LOG (szDebug);
#endif
		mysql_close(mysql);
		return policy;
	}

	if (mysql_field_count(mysql) > 0) {
		results = mysql_store_result(mysql);

		record = mysql_fetch_row(results);

		if (record) {
			policy = strtoul(record[1], NULL, 10);
#ifdef WITH_DEBUG
			sprintf(szDebug, "%s, domain %s, policy %s\n",ipaddr, record[0], record[1]);
			DEBUG_LOG (szDebug);
		} else {
			sprintf(szDebug, "policy not found for %s\n",ipaddr);
			DEBUG_LOG (szDebug);
#endif
		}
		mysql_free_result(results);
	} else {
#ifdef WITH_DEBUG
		sprintf(szDebug, "policy for %s not found\n", ipaddr);
		DEBUG_LOG (szDebug);
#endif
	}

	mysql_close(mysql);
	return policy;	
}

#if 0
static policy_type_t
_mysql_query_domain_policy(MYSQL *mysql, const char *domain)
{
    int lvl = 0;
    policy_type_t policy = POLICY_DEFAULT;

    const char *d;

    /* domain must ends with '.' */
    for (d = domain; *d; d++) {
        if (*d == '.')
            lvl ++;
    }

    d = domain;

    while ((policy == POLICY_DEFAULT) && (lvl > 0)) {
        MYSQL_RES *results;
        MYSQL_ROW record;
        char sql_statement[128];

        snprintf(sql_statement, sizeof(sql_statement) - 1, "SELECT * FROM " TABLE_NAME_DOMAIN " where " TABLE_FIELD_LVL " = %u AND " TABLE_FIELD_DOMAIN " LIKE '%%%s'", lvl--, d);

#ifdef WITH_DEBUG
        fprintf(_priv_data.fp, "query domain policy: %s\n", sql_statement);
#endif

        if (mysql_query(mysql, sql_statement) != 0) {
            /* query failed */
#ifdef WITH_DEBUG
            fprintf(_priv_data.fp, "query domain policy failed:%s\n",
                   mysql_error(mysql));
#endif
            break;
        }

        while (*d++ != '.');

        if (mysql_field_count(mysql) == 0) {
            continue;
        }

        results = mysql_store_result(mysql);

        while((record = mysql_fetch_row(results))) {
#ifdef DEBUG
            unsigned int levels;

            levels = strtoul(record[1], NULL, 10);
            policy = strtoul(record[2], NULL, 10);
            fprintf(_priv_data.fp, "%s,levels ptr %p: %s-%d, policy ptr %p:%s-%d\n", record[0], record[1], record[1],levels,record[2], record[2],policy);
#endif
        }

        mysql_free_result(results);
    }

    return policy;
}
#endif

const char ip_policy_group[10] = "ip_policy";

static char *
memcached_get_policy_by_addr(memcached_st *memc, const char *ipaddr, size_t *value_len, uint32_t *flags)
{
	char *value;
	memcached_return_t error = MEMCACHED_SUCCESS;

	value = memcached_get_by_key(memc, ip_policy_group, sizeof(ip_policy_group), ipaddr, strlen(ipaddr), value_len, flags, &error);
	//value = memcached_get(memc, ipaddr, strlen(ipaddr), value_len, flags, &error);
#ifdef WITH_DEBUG
	if (value == NULL || error != MEMCACHED_SUCCESS) {
		char szDebug[300];

		sprintf (szDebug, "get policy of %s failed:%s", ipaddr,
				memcached_strerror(memc, error));
		DEBUG_LOG (szDebug);
	}
#endif

	return value;
}

static void
memcached_set_policy_by_addr(memcached_st *memc, const char *ipaddr, const void *value, size_t value_len, uint32_t flags)
{
	memcached_return_t retval;
#ifdef WITH_DEBUG
	char szDebug[300];
#endif

#ifdef WITH_DEBUG
	sprintf (szDebug, "update policy of %s to %d", ipaddr, *(int *)value);
	DEBUG_LOG (szDebug);
#endif
	if ((retval = memcached_set_by_key(memc, ip_policy_group, sizeof(ip_policy_group),
				ipaddr, strlen(ipaddr), value, value_len, 300, flags)) != MEMCACHED_SUCCESS) {
#ifdef WITH_DEBUG
		sprintf (szDebug, "update policy of %s failed:%s", ipaddr,
				memcached_strerror(memc, retval));
		DEBUG_LOG (szDebug);
#endif
	};
}

chain_t * 
conn_query_for_chain(conn_t *conn, chain_t *chain)
{
	char *value;
	uint32_t flags = 0;
	const char *ipaddr = ai_getAddressString(&conn->dest);
#ifdef WITH_DEBUG
	char szDebug[300];
#endif

	policy_type_t policy = POLICY_UNKNOWN;

#ifdef WITH_MEMCACHED
	if (conn->memc) {
		size_t value_len = 0;
		uint32_t flags = 0;

		char *value = memcached_get_policy_by_addr(conn->memc, ipaddr, &value_len, &flags);
		if (value && value_len == sizeof(policy)) {
			memcpy(&policy, value, value_len);
		} else{
#ifdef WITH_DEBUG
			sprintf (szDebug, "get policy returns %p value_len: %d", value, value_len);
			DEBUG_LOG (szDebug);
#endif
		}
#ifdef WITH_DEBUG
		sprintf (szDebug, "ip policy of %s from memcached: %d",ipaddr, policy);
		DEBUG_LOG (szDebug);
#endif
	}
#endif

	if (policy == POLICY_UNKNOWN) {
		policy = _mysql_query_policy_by_addr(&conn->mysql, ipaddr);
#ifdef WITH_MEMCACHED
		if (conn->memc) {
			if (policy == POLICY_UNKNOWN)
				policy = POLICY_DEFAULT;
			memcached_set_policy_by_addr(conn->memc, ipaddr, &policy, sizeof(policy), 0);
		}
#endif
	}

	if (policy == POLICY_PROXY) {
		chain->policy = policy;
	} else {
		chain = NULL;
	}

	return chain;
}

void
conn_free_memc(conn_t *conn)
{
	if (conn->memc) {
		memcached_free(conn->memc);
		conn->memc = NULL;
	}
}

#include <sys/select.h>
#include <fcntl.h>

int
direct_connect_tosockaddr (SOCKADDR *sa, int len)
{
	int fd, retval;
	int flags;
	fd_set wfds;
	struct timeval tv;

	fd = socket (sa->sa_family, SOCK_STREAM, 0);

	retval = fcntl (fd, F_GETFL);
	retval |= O_NONBLOCK;
	fcntl (fd, F_SETFL, retval);

	FD_SET(fd, &wfds);

	tv.tv_sec = 6;
	tv.tv_usec = 0;

	connect (fd, sa, len);
	retval = select(fd + 1, NULL, &wfds, NULL, &tv);

	close(fd);
	return (retval == 1) ? 0 : -1;
}

#endif

#endif

void *
ChildThread (void *conn)
{
	conn_t *connection;
	unsigned char ver;
	BOOL ret;
#ifdef WITH_DEBUG
	char szDebug[300];
#endif
	connection = (conn_t *) conn;

	ret = FALSE;

	if (!conn_getChar (connection, &ver))
		goto barfed;
#ifdef WITH_DEBUG
	sprintf (szDebug, "Version: %x", ver);
	DEBUG_LOG (szDebug);
#endif

#ifdef WITH_MYSQL

#endif

	connection->version = ver;
	switch (ver) {
	case 4:
		ret = socks4_handler (connection);
		break;
	case 5:
		ret = socks5_handler (connection);
		break;
	}
  barfed:
	if (ret == FALSE) {
		closesocket (connection->s);
		log_log (connection, LOG_EVT_LOG, LOG_TYPE_CONNECTIONCLOSE, NULL);
#ifdef WITH_DEBUG
		DEBUG_LOG ("Cleaning up from unsuccessful connection");
#endif
		config_dereference (connection->conf);
		conn_close (connection);
	}
	return NULL;
}

/* vim: set ts=4 sw=4 noet: */
