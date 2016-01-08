
#ifdef WIN32_NO_CONFIG_H
#ifndef _WIN32_
#define _WIN32_
#endif
#define WIN32_LEAN_AND_MEAN
#include "../winconf.h"
#else
#include "../config.h"
#endif
#include <sys/types.h>
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#else
#ifdef HAVE_WINSOCK_H
#include <winsock.h>
#endif
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <string.h>
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#include <antinat.h>

#define R_SUCCESS 3
#define R_PASSED  2
#define R_FAILED  0

#define server "localhost:1080"

#ifdef _WIN32_
#define sleep(x) Sleep(x*1000)
#endif

void
test_msg (char *msg)
{
	int i;
	printf ("%s", msg);
	for (i = strlen (msg); i < 65; i++)
		printf (".");
	fflush (NULL);
}

void
test_res (int result)
{
	switch (result) {
	case R_SUCCESS:
		printf ("Succeeded\n");
		break;
	case R_PASSED:
		printf ("Passed\n");
		break;
	case R_FAILED:
		printf ("FAILED\n");
		exit (EXIT_FAILURE);
		break;
	}
	fflush (NULL);
}

void
socks4_test ()
{
	ANCONN net;
	ANCONN temp;
	int retval;
	struct sockaddr_in sin;

	test_msg ("Testing basic socks4");
	net = an_new_connection ();
	retval = an_set_proxy_url (net, "socks4://" server);
	if (retval != AN_ERROR_SUCCESS)
		test_res (R_FAILED);
	memset (&sin, 0, sizeof (sin));
	sin.sin_family = AF_INET;
	sin.sin_port = htons (1080);
	sin.sin_addr.s_addr = htonl (0x7f000001);
	retval =
		an_socks4_connect_tosockaddr (net, (struct sockaddr *) &sin,
									  sizeof (sin));
	if (retval != AN_ERROR_SUCCESS)
		test_res (R_FAILED);
	retval = an_socks4_close (net);
	if (retval != AN_ERROR_SUCCESS)
		test_res (R_FAILED);
	test_res (R_SUCCESS);

	test_msg ("Testing socks4 with user");
	retval = an_set_credentials (net, "someuser", NULL);
	if (retval != AN_ERROR_SUCCESS)
		test_res (R_FAILED);
	retval =
		an_socks4_connect_tosockaddr (net, (struct sockaddr *) &sin,
									  sizeof (sin));
	if (retval != AN_ERROR_SUCCESS)
		test_res (R_FAILED);
	retval = an_socks4_close (net);
	if (retval != AN_ERROR_SUCCESS)
		test_res (R_FAILED);
	test_res (R_SUCCESS);

	test_msg ("Testing socks4 with bad address");
	retval = an_set_credentials (net, NULL, NULL);
	if (retval != AN_ERROR_SUCCESS)
		test_res (R_FAILED);
	sin.sin_addr.s_addr = htonl (0x01010101);
	retval =
		an_socks4_connect_tosockaddr (net, (struct sockaddr *) &sin,
									  sizeof (sin));
	if (retval == AN_ERROR_SUCCESS)
		test_res (R_FAILED);
	an_socks4_close (net);
	test_res (R_SUCCESS);

	test_msg ("Testing socks4 with bad port");
	sin.sin_addr.s_addr = htonl (0x7f000001);
	sin.sin_port = htons (0xfefe);
	retval =
		an_socks4_connect_tosockaddr (net, (struct sockaddr *) &sin,
									  sizeof (sin));
	if (retval == AN_ERROR_SUCCESS)
		test_res (R_FAILED);
	an_socks4_close (net);
	test_res (R_SUCCESS);

	test_msg ("Testing basic socks4a");
	retval = an_socks4_connect_tohostname (net, "localhost", 1080);
	if (retval != AN_ERROR_SUCCESS)
		test_res (R_FAILED);
	retval = an_socks4_close (net);
	if (retval != AN_ERROR_SUCCESS)
		test_res (R_FAILED);
	test_res (R_SUCCESS);

	test_msg ("Testing basic socks4a with credentials");
	retval = an_set_credentials (net, "someuser", NULL);
	if (retval != AN_ERROR_SUCCESS)
		test_res (R_FAILED);
	retval = an_socks4_connect_tohostname (net, "localhost", 1080);
	if (retval != AN_ERROR_SUCCESS)
		test_res (R_FAILED);
	retval = an_socks4_close (net);
	if (retval != AN_ERROR_SUCCESS)
		test_res (R_FAILED);
	test_res (R_SUCCESS);


	test_msg ("Testing bad socks4a hostname");
	retval = an_set_credentials (net, NULL, NULL);
	if (retval != AN_ERROR_SUCCESS)
		test_res (R_FAILED);
	retval = an_socks4_connect_tohostname (net, "badhostname", 1080);
	if (retval == AN_ERROR_SUCCESS)
		test_res (R_FAILED);
	an_socks4_close (net);
	test_res (R_SUCCESS);

	test_msg ("Testing socks4 binding");
	retval = an_socks4_bind_tohostname (net, "localhost", 1080);
	if (retval != AN_ERROR_SUCCESS)
		test_res (R_FAILED);
	retval = an_socks4_listen (net);
	if (retval != AN_ERROR_SUCCESS)
		test_res (R_FAILED);
	retval =
		an_socks4_getsockname (net, (struct sockaddr *) &sin, sizeof (sin));
	if (retval != AN_ERROR_SUCCESS)
		test_res (R_FAILED);
	temp = an_new_connection ();
	an_set_proxy_url (temp, "socks4://" server);
	sin.sin_addr.s_addr = htonl (0x7f000001);
	sleep (1);
	retval =
		an_socks4_connect_tohostname (temp, "localhost",
									  ntohs (sin.sin_port));
	retval = an_socks4_accept (net, (struct sockaddr *) &sin, sizeof (sin));
	if (retval != AN_ERROR_SUCCESS)
		test_res (R_FAILED);
	an_socks4_close (temp);
	an_destroy (temp);
	an_socks4_close (net);
	test_res (R_SUCCESS);

	test_msg ("Testing socks4 binding with incorrect incoming addy");
	sin.sin_addr.s_addr = htonl (0x01010101);
	sin.sin_port = htons (1080);
	retval =
		an_socks4_bind_tosockaddr (net, (struct sockaddr *) &sin,
								   sizeof (sin));
	if (retval != AN_ERROR_SUCCESS)
		test_res (R_FAILED);
	retval = an_socks4_listen (net);
	if (retval != AN_ERROR_SUCCESS)
		test_res (R_FAILED);
	retval =
		an_socks4_getsockname (net, (struct sockaddr *) &sin, sizeof (sin));
	if (retval != AN_ERROR_SUCCESS)
		test_res (R_FAILED);
	temp = an_new_connection ();
	an_set_proxy_url (temp, "socks4://" server);
	sin.sin_addr.s_addr = htonl (0x7f000001);
	sleep (1);
	an_socks4_connect_tosockaddr (temp, (struct sockaddr *) &sin,
								  sizeof (sin));
	retval = an_socks4_accept (net, (struct sockaddr *) &sin, sizeof (sin));
	if (retval == AN_ERROR_SUCCESS)
		test_res (R_FAILED);
	an_socks4_close (temp);
	an_destroy (temp);
	an_socks4_close (net);
	test_res (R_SUCCESS);

	an_destroy (net);

}

void
socks5_anon_ipv4_test ()
{
	ANCONN net;
	ANCONN temp;
	int retval;
	struct sockaddr_in sin;

	test_msg ("Testing basic socks5");
	net = an_new_connection ();
	retval = an_set_proxy_url (net, "socks5://" server);
	if (retval != AN_ERROR_SUCCESS)
		test_res (R_FAILED);
	memset (&sin, 0, sizeof (sin));
	sin.sin_family = AF_INET;
	sin.sin_port = htons (1080);
	sin.sin_addr.s_addr = htonl (0x7f000001);
	retval =
		an_socks5_connect_tosockaddr (net, (struct sockaddr *) &sin,
									  sizeof (sin));
	if (retval != AN_ERROR_SUCCESS)
		test_res (R_FAILED);
	retval = an_socks5_close (net);
	if (retval != AN_ERROR_SUCCESS)
		test_res (R_FAILED);
	test_res (R_SUCCESS);


	test_msg ("Testing socks5 with bad addy");
	sin.sin_addr.s_addr = htonl (0x01010101);
	retval =
		an_socks5_connect_tosockaddr (net, (struct sockaddr *) &sin,
									  sizeof (sin));
	if (retval == AN_ERROR_SUCCESS)
		test_res (R_FAILED);
	an_socks5_close (net);
	test_res (R_SUCCESS);

	test_msg ("Testing socks5 with bad port");
	sin.sin_addr.s_addr = htonl (0x7f000001);
	sin.sin_port = htons (0xfefe);
	retval =
		an_socks5_connect_tosockaddr (net, (struct sockaddr *) &sin,
									  sizeof (sin));
	if (retval == AN_ERROR_SUCCESS)
		test_res (R_FAILED);
	an_socks5_close (net);
	test_res (R_SUCCESS);

	test_msg ("Testing socks5 with user");
	retval = an_set_credentials (net, "user", "pass");
	if (retval != AN_ERROR_SUCCESS)
		test_res (R_FAILED);
	sin.sin_port = htons (1080);
	retval =
		an_socks5_connect_tosockaddr (net, (struct sockaddr *) &sin,
									  sizeof (sin));
	if (!((retval == AN_ERROR_SUCCESS) || (retval == AN_ERROR_AUTH)))
		test_res (R_FAILED);
	an_socks5_close (net);
	test_res (R_SUCCESS);

	test_msg ("Testing socks5 binding");
	retval = an_socks5_bind_tohostname (net, "localhost", 1080);
	if (retval != AN_ERROR_SUCCESS)
		test_res (R_FAILED);
	retval = an_socks5_listen (net);
	if (retval != AN_ERROR_SUCCESS)
		test_res (R_FAILED);
	retval =
		an_socks5_getsockname (net, (struct sockaddr *) &sin, sizeof (sin));
	if (retval != AN_ERROR_SUCCESS)
		test_res (R_FAILED);
	temp = an_new_connection ();
	an_set_proxy_url (temp, "socks5://" server);
	sin.sin_addr.s_addr = htonl (0x7f000001);
	sleep (1);
	retval =
		an_socks5_connect_tohostname (temp, "localhost",
									  ntohs (sin.sin_port));
	retval = an_socks5_accept (net, (struct sockaddr *) &sin, sizeof (sin));
	if (retval != AN_ERROR_SUCCESS)
		test_res (R_FAILED);
	an_socks5_close (temp);
	an_destroy (temp);
	an_socks5_close (net);
	test_res (R_SUCCESS);

	an_destroy (net);

}

void
socks5_anon_name_test ()
{
	ANCONN net;
	int retval;

	test_msg ("Testing basic named socks5");
	net = an_new_connection ();
	retval = an_set_proxy_url (net, "socks5://" server);
	if (retval != AN_ERROR_SUCCESS)
		test_res (R_FAILED);
	retval = an_socks5_connect_tohostname (net, "localhost", 1080);
	if (retval != AN_ERROR_SUCCESS)
		test_res (R_FAILED);
	retval = an_socks5_close (net);
	if (retval != AN_ERROR_SUCCESS)
		test_res (R_FAILED);
	test_res (R_SUCCESS);


	test_msg ("Testing with bad hostname");
	retval = an_socks5_connect_tohostname (net, "badhostname", 1080);
	if (retval == AN_ERROR_SUCCESS)
		test_res (R_FAILED);
	retval = an_socks5_close (net);
	if (retval != AN_ERROR_SUCCESS)
		test_res (R_FAILED);
	test_res (R_SUCCESS);

	an_destroy (net);
}

/*

typedef struct socks5_udp_req {
	unsigned char rsv1;
	unsigned char rsv2;
	unsigned char frag;
	unsigned char atyp;
	unsigned long ipaddy;
	unsigned short port;
	char data[128];
} socks5_udp_req;

void socks5_udp_test()
{
	char szBuf1[512];
	SOCKET s,c;
	SOCKADDR sa;
	SOCKADDR_IN sin;
	SOCKADDR_IN srvsin;
	unsigned short endport;
	sl_t sl;
	socks5_ver ver;
	socks5_ver_resp vr;
	socks5_ip_req req;
	socks5_resp resp;
	socks5_udp_req udpreq;
	DSNetwork net;

	sl=sizeof(sa);
	s=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	c=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	memset(&sa,0,sizeof(sa));
	sa.sa_family=AF_INET;
	sl=sizeof(sa);
	bind(s, &sa, sl);
	memset(&sa,0,sizeof(sa));
	sa.sa_family=AF_INET;
	sl=sizeof(sa);
	bind(c, &sa, sl);
	sl=sizeof(sa);
	getsockname(s, &sa, &sl);
	endport=((SOCKADDR_IN *)&sa)->sin_port;

	test_msg("Testing socks5 UDP");
	test_fill_socks5_ver(&ver);
	test_fill_socks5_req(&req);
	if (!net.connectToHost("localhost",1080)) test_res(R_FAILED);
	if (!net.sendData((char *)&ver,sizeof(ver))) test_res(R_FAILED);
	if (!net.getSlab((char *)&vr,sizeof(vr)-1)) test_res(R_FAILED);
	if ((vr.meth!=ver.meth)||(vr.ver!=ver.ver)) test_res(R_FAILED);
	req.dstport=((SOCKADDR_IN*)&sa)->sin_port;
	req.dstip=htonl(0x7f000001);
	req.cmd=0x03;
	if (!net.sendData((char *)&req,sizeof(req))) test_res(R_FAILED);
	if (!net.getSlab((char *)&resp,sizeof(resp)-1)) test_res(R_FAILED);
	if (resp.cmd!=0) test_res(R_FAILED);
	memset(&udpreq,0,sizeof(udpreq));
	udpreq.atyp=0x01;
	udpreq.ipaddy=htonl(0x7f000001);
	udpreq.port=endport;
	strcpy(udpreq.data,"Hello");
	memset(&sin,0,sizeof(sin));
	sin.sin_family=AF_INET;
	sin.sin_port=resp.dstport;
	memcpy(&sin.sin_addr.s_addr,&resp.dstip,4);
	sl=sizeof(sin);
	sl=sendto(c, &udpreq, sizeof(udpreq), 0, (SOCKADDR *)&sin, sl);
	sl=sizeof(sin);
	recvfrom(s, &szBuf1, sizeof(szBuf1), 0, (SOCKADDR *)&srvsin, &sl);
	if (strcmp(szBuf1,"Hello")) test_res(R_FAILED);
	strcpy(szBuf1,"Greetings Earthlings");
	sl=sizeof(sin);
	sendto(s, szBuf1, strlen(szBuf1)+1, 0, (SOCKADDR *)&srvsin, sl);
	sl=sizeof(sin);
	recvfrom(c, &udpreq, sizeof(udpreq), 0, (SOCKADDR *)&sin, &sl);
	if (!strcmp(udpreq.data,"Greetings Earthlings")) {
		test_res(R_SUCCESS);
	} else {
		test_res(R_FAILED);
	}
	
	net.terminate();

	test_msg("Testing socks5 UDP wo TCP addy");
	test_fill_socks5_ver(&ver);
	test_fill_socks5_req(&req);
	if (!net.connectToHost("localhost",1080)) test_res(R_FAILED);
	if (!net.sendData((char *)&ver,sizeof(ver))) test_res(R_FAILED);
	if (!net.getSlab((char *)&vr,sizeof(vr)-1)) test_res(R_FAILED);
	if ((vr.meth!=ver.meth)||(vr.ver!=ver.ver)) test_res(R_FAILED);
	req.dstport=0;
	req.dstip=0;
	req.cmd=0x03;
	if (!net.sendData((char *)&req,sizeof(req))) test_res(R_FAILED);
	if (!net.getSlab((char *)&resp,sizeof(resp)-1)) test_res(R_FAILED);
	if (resp.cmd!=0) test_res(R_FAILED);
	memset(&udpreq,0,sizeof(udpreq));
	udpreq.atyp=0x01;
	udpreq.ipaddy=htonl(0x7f000001);
	udpreq.port=endport;
	strcpy(udpreq.data,"Hello");
	memset(&sin,0,sizeof(sin));
	sin.sin_family=AF_INET;
	sin.sin_port=resp.dstport;
	memcpy(&sin.sin_addr.s_addr,&resp.dstip,4);
	sl=sizeof(sin);
	sl=sendto(c, &udpreq, sizeof(udpreq), 0, (SOCKADDR *)&sin, sl);
	sl=sizeof(sin);
	recvfrom(s, &szBuf1, sizeof(szBuf1), 0, (SOCKADDR *)&srvsin, &sl);
	if (strcmp(szBuf1,"Hello")) test_res(R_FAILED);
	strcpy(szBuf1,"Greetings Earthlings");
	sl=sizeof(sin);
	sendto(s, szBuf1, strlen(szBuf1)+1, 0, (SOCKADDR *)&srvsin, sl);
	sl=sizeof(sin);
	recvfrom(c, &udpreq, sizeof(udpreq), 0, (SOCKADDR *)&sin, &sl);
	if (!strcmp(udpreq.data,"Greetings Earthlings")) {
		test_res(R_SUCCESS);
	} else {
		test_res(R_FAILED);
	}
	
	net.terminate();

}


*/

#define STRESS_SIZE 512

void
socks4_stress ()
{
	ANCONN net[STRESS_SIZE];
	int retval;
	struct sockaddr_in sin;
	int i;

	test_msg ("Stressing basic socks4");
	for (i = 0; i < STRESS_SIZE; i++) {
		net[i] = an_new_connection ();
		retval = an_set_proxy_url (net[i], "socks4://" server);
		if (retval != AN_ERROR_SUCCESS)
			test_res (R_FAILED);
		memset (&sin, 0, sizeof (sin));
		sin.sin_family = AF_INET;
		sin.sin_port = htons (1080);
		sin.sin_addr.s_addr = htonl (0x7f000001);
		retval =
			an_socks4_connect_tosockaddr (net[i], (struct sockaddr *) &sin,
										  sizeof (sin));
		if (retval != AN_ERROR_SUCCESS) {
			printf ("Connection error %i errno %i on connection #%i\n",
					retval, errno, i);
			test_res (R_FAILED);
		}
	}
	sleep (2);
	for (i = 0; i < STRESS_SIZE; i++) {
		retval = an_socks4_close (net[i]);
		if (retval != AN_ERROR_SUCCESS)
			test_res (R_FAILED);
	}
	test_res (R_SUCCESS);
}

int
main ()
{
	socks4_test ();
	printf ("\n");
	socks5_anon_ipv4_test ();
	socks5_anon_name_test ();
	socks4_stress ();
/*
	socks5_udp_test();
*/
	return EXIT_SUCCESS;
}
