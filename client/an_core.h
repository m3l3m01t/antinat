/* ANTINAT
 * =======
 * This software is Copyright (c) 2003-05 Malcolm Smith.
 * No warranty is provided, including but not limited to
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * This code is licenced subject to the GNU General
 * Public Licence (GPL).  See the COPYING file for more.
 */


int _an_generic_recv (ANCONN conn, void *buf, size_t len, int flags);
int _an_generic_send (ANCONN conn, const void *buf, size_t len, int flags);
int _an_generic_recv_all (ANCONN conn, void *buf, size_t len);
int _an_generic_send_all (ANCONN conn, const void *buf, size_t len);

int _an_generic_close (ANCONN conn);

int _an_blocking_recv (ANCONN conn, void *buf, size_t len, int flags);
int _an_blocking_send (ANCONN conn, const void *buf, size_t len, int flags);

int _an_rawconnect (ANCONN conn);

int _an_generic_getline (ANCONN conn, void *buf, size_t nbuf);
int _an_getsockaddr (struct st_sock_info *info, SOCKADDR * sa, int sl);

int _an_setsockaddr (struct st_sock_info *info, SOCKADDR * sa, int sl);
int _an_setsockaddr_sock (struct st_sock_info *info, SOCKET s, int peer);
