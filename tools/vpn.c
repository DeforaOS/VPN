/* $Id$ */
/* Copyright (c) 2011-2020 Pierre Pronchery <khorben@defora.org> */
/* This file is part of DeforaOS System VPN */
/* This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>. */



#include <sys/resource.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <dlfcn.h>
#include <sys/socket.h>
#include <System.h>
#include <System/App.h>
#include "../src/common.c"


/* libVPN */
/* private */
/* types */
typedef struct _VPNAppClient
{
	String const * name;
	AppClient * appclient;
} VPNAppClient;

typedef struct _VPNAppClientFD
{
	AppClient * appclient;
	int32_t fd;
} VPNAppClientFD;


/* constants */
#define APPINTERFACE	"VPN"
#define PROGNAME	APPINTERFACE


/* variables */
static VPNAppClient * _vpn_clients = NULL;
static size_t _vpn_clients_cnt = 0;

static VPNAppClientFD * _vpn_clients_fd = NULL;
static size_t _vpn_clients_fd_cnt = 0;

static int _vpn_offset = 1024;

/* local functions */
static int (*old_close)(int fd);
static int (*old_connect)(int fd, const struct sockaddr * name,
		socklen_t namelen);
static ssize_t (*old_read)(int fd, void * buf, size_t count);
static ssize_t (*old_recv)(int fd, void * buf, size_t count, int flags);
static ssize_t (*old_send)(int fd, void const * buf, size_t count, int flags);
static ssize_t (*old_write)(int fd, void const * buf, size_t count);


/* prototypes */
static void _libvpn_init(void);

/* accessors */
static AppClient * _libvpn_get_appclient(const struct sockaddr * name,
		socklen_t namelen);
static AppClient * _libvpn_get_appclient_fd(int32_t * fd);
static String * _libvpn_get_remote_host(
		const struct sockaddr * name, socklen_t namelen);
static int _libvpn_get_remote_name(
		const struct sockaddr * name, socklen_t namelen,
		struct sockaddr ** rname, socklen_t * rnamelen);
static unsigned int _libvpn_is_remote(const struct sockaddr * name,
		socklen_t namelen);

/* useful */
static int _libvpn_deregister_fd(AppClient * appclient, int32_t fd);
static int _libvpn_register_fd(AppClient * appclient, int32_t * fd);


/* functions */
static void _libvpn_init(void)
{
	static void * hdl = NULL;
	/* FIXME some symbols may be in libsocket.so instead */
	static char * libc[] = { "/lib/libc.so", "/lib/libc.so.6" };
	size_t i;
#ifdef RLIMIT_NOFILE
	struct rlimit r;
#endif

	if(hdl != NULL)
		return;
	for(i = 0; i < sizeof(libc) / sizeof(*libc); i++)
		if((hdl = dlopen(libc[i], RTLD_LAZY)) != NULL)
			break;
	if(hdl == NULL)
	{
		fprintf(stderr, "%s: %s\n", PROGNAME, dlerror());
		exit(1);
	}
	if((old_close = dlsym(hdl, "close")) == NULL
			|| (old_connect = dlsym(hdl, "connect")) == NULL
			|| (old_read = dlsym(hdl, "read")) == NULL
			|| (old_recv = dlsym(hdl, "recv")) == NULL
			|| (old_send = dlsym(hdl, "send")) == NULL
			|| (old_write = dlsym(hdl, "write")) == NULL)
	{
		fprintf(stderr, "%s: %s\n", PROGNAME, dlerror());
		dlclose(hdl);
		exit(1);
	}
	dlclose(hdl);
#ifdef RLIMIT_NOFILE
	if(getrlimit(RLIMIT_NOFILE, &r) == 0)
	{
		if(r.rlim_max > INT_MAX)
		{
			fprintf(stderr, "%s: %s\n", PROGNAME, strerror(ERANGE));
			exit(1);
		}
		if(_vpn_offset < r.rlim_max)
			_vpn_offset = (int)r.rlim_max;
	}
# ifdef DEBUG
	fprintf(stderr, "DEBUG: %s() %u\n", __func__, _vpn_offset);
# endif
#endif
}


/* accessors */
/* libvpn_get_appclient */
static AppClient * _libvpn_get_appclient(const struct sockaddr * name,
		socklen_t namelen)
{
	String * host;
	size_t i;
	VPNAppClient * p;

	if((host = _libvpn_get_remote_host(name, namelen)) == NULL)
		return NULL;
	for(i = 0; i < _vpn_clients_cnt; i++)
		if(_vpn_clients[i].name == NULL)
			continue;
		else if(string_compare(_vpn_clients[i].name, host) == 0)
		{
			string_delete(host);
			return _vpn_clients[i].appclient;
		}
	if((p = realloc(_vpn_clients, sizeof(*_vpn_clients) * (i + 1))) == NULL)
	{
		string_delete(host);
		return NULL;
	}
	_vpn_clients = p;
	p = &_vpn_clients[_vpn_clients_cnt++];
	if((p->appclient = appclient_new(NULL, APPINTERFACE, host)) == NULL)
	{
		string_delete(host);
		return NULL;
	}
	p->name = host;
	return p->appclient;
}


/* libvpn_get_appclient_fd */
static AppClient * _libvpn_get_appclient_fd(int32_t * fd)
{
	size_t i;

	if(*fd < _vpn_offset)
		return NULL;
	i = (size_t)*fd - _vpn_offset;
	if(i >= _vpn_clients_fd_cnt)
		return NULL;
	*fd = _vpn_clients_fd[i].fd;
	return _vpn_clients_fd[i].appclient;
}


/* libvpn_get_remote_host */
static String * _libvpn_get_remote_host(
		const struct sockaddr * name, socklen_t namelen)
{
	(void) name;
	(void) namelen;

	/* FIXME really implement through gethostbyname()/getaddrinfo() */
	return getenv("APPSERVER_VPN");
}


/* libvpn_get_remote_name */
static int _libvpn_get_remote_name(
		const struct sockaddr * name, socklen_t namelen,
		struct sockaddr ** rname, socklen_t * rnamelen)
{
	/* FIXME really implement through gethostbyname()/getaddrinfo() */
	if(rname == NULL && rnamelen == NULL)
		return 0;
	return -1;
}


/* libvpn_is_remote */
static unsigned int _libvpn_is_remote(const struct sockaddr * name,
		socklen_t namelen)
{
	return (_libvpn_get_remote_name(name, namelen, NULL, NULL) == 0)
		? 1 : 0;
}


/* useful */
/* libvpn_deregister_fd */
static int _libvpn_deregister_fd(AppClient * appclient, int32_t fd)
{
	if(fd < 0 || (size_t)fd >= _vpn_clients_fd_cnt)
		return -1;
	/* sanity check */
	if(_vpn_clients_fd[fd].appclient != appclient)
		return -1;
	_vpn_clients_fd[fd].appclient = NULL;
	_vpn_clients_fd[fd].fd = -1;
	return 0;
}


/* libvpn_register_fd */
static int _libvpn_register_fd(AppClient * appclient, int32_t * fd)
{
	size_t i;
	VPNAppClientFD * p;

	for(i = 0; i < _vpn_clients_fd_cnt; i++)
		if(_vpn_clients_fd[i].fd < 0)
		{
			_vpn_clients_fd[i].appclient = appclient;
			_vpn_clients_fd[i].fd = *fd;
			*fd = _vpn_offset + i;
			return 0;
		}
	if((p = realloc(_vpn_clients_fd, sizeof(*p) * (i + 1))) == NULL)
		return -1;
	_vpn_clients_fd = p;
	p = &_vpn_clients_fd[_vpn_clients_fd_cnt++];
	p->appclient = appclient;
	p->fd = *fd;
	*fd = _vpn_offset + i;
	return 0;
}


/* public */
/* interface */
/* close */
int close(int fd)
{
	int ret;
	AppClient * appclient;

	_libvpn_init();
	if((appclient = _libvpn_get_appclient_fd(&fd)) == NULL)
		return old_close(fd);
	if(appclient_call(appclient, (void **)&ret, "close", fd) != 0)
		return -1;
#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s(%p:%d) => %d\n", __func__,
			(void *)appclient, fd, ret);
#endif
	if(ret != 0)
		return _vpn_errno(_vpn_error, _vpn_error_cnt, -ret, 1);
	else
		_libvpn_deregister_fd(appclient, fd);
	return ret;
}


/* connect */
int connect(int fd, const struct sockaddr * name, socklen_t namelen)
{
	int ret;
	AppClient * appclient;

	_libvpn_init();
	if(_libvpn_is_remote(name, namelen) == 0)
		return old_connect(fd, name, namelen);
	if((appclient = _libvpn_get_appclient(name, namelen)) == NULL)
		return -1;
	if(appclient_call(appclient, (void **)&ret, "connect", fd) != 0)
		return -1;
#ifdef DEBUG
	fprintf(stderr, "DEBUG: connect(%d) => %d\n", fd, ret);
#endif
	if(ret < 0)
		return _vpn_errno(_vpn_error, _vpn_error_cnt, -ret, 1);
	if(_libvpn_register_fd(appclient, &ret) != 0)
	{
		appclient_call(appclient, NULL, "close", ret);
		return -1;
	}
	return ret;
}


/* read */
ssize_t read(int fd, void * buf, size_t count)
{
	int32_t ret;
	AppClient * appclient;
	Buffer * b;

	_libvpn_init();
	if((appclient = _libvpn_get_appclient_fd(&fd)) == NULL)
		return old_read(fd, buf, count);
	if((b = buffer_new(0, NULL)) == NULL)
		return -1;
	if(appclient_call(appclient, (void **)&ret, "recv", fd, b, count,
				0) != 0)
	{
		buffer_delete(b);
		/* FIXME define errno */
		return -1;
	}
#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s(%p:%d, buf, %lu) => %d\n", __func__,
			(void *)appclient, fd, count, ret);
#endif
	if(ret < 0)
		ret = _vpn_errno(_vpn_error, _vpn_error_cnt, -ret, 1);
	else if(ret > 0)
		memcpy(buf, buffer_get_data(b), ret);
	buffer_delete(b);
	return ret;
}


/* recv */
ssize_t recv(int fd, void * buf, size_t count, int flags)
{
	int32_t ret;
	AppClient * appclient;
	Buffer * b;

	_libvpn_init();
	if((appclient = _libvpn_get_appclient_fd(&fd)) == NULL)
		return old_read(fd, buf, count);
	if((b = buffer_new(0, NULL)) == NULL)
		return -1;
	if(appclient_call(appclient, (void **)&ret, "recv", fd, b, count,
				flags) != 0)
	{
		buffer_delete(b);
		/* FIXME define errno */
		return -1;
	}
#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s(%p:%d, buf, %lu, %#x) => %d\n", __func__,
			(void *)appclient, fd, count, flags, ret);
#endif
	if(ret < 0)
		ret = _vpn_errno(_vpn_error, _vpn_error_cnt, -ret, 1);
	else if(ret > 0)
		memcpy(buf, buffer_get_data(b), ret);
	buffer_delete(b);
	return ret;
}


/* send */
ssize_t send(int fd, void const * buf, size_t count, int flags)
{
	int32_t ret;
	AppClient * appclient;
	Buffer * b;

	_libvpn_init();
	if((appclient = _libvpn_get_appclient_fd(&fd)) == NULL)
		return old_write(fd, buf, count);
	if((b = buffer_new(count, buf)) == NULL)
		return -1;
	if(appclient_call(appclient, (void **)&ret, "send", fd, b, count,
				flags) != 0)
	{
		buffer_delete(b);
		return -1;
	}
#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s(%d, buf, %lu, %#x) => %d\n", __func__,
			fd, count, flags, ret);
#endif
	buffer_delete(b);
	if(ret < 0)
		return _vpn_errno(_vpn_error, _vpn_error_cnt, -ret, 1);
	return ret;
}


/* write */
ssize_t write(int fd, void const * buf, size_t count)
{
	int32_t ret;
	AppClient * appclient;
	Buffer * b;

	_libvpn_init();
	if((appclient = _libvpn_get_appclient_fd(&fd)) == NULL)
		return old_write(fd, buf, count);
	if((b = buffer_new(count, buf)) == NULL)
		return -1;
	if(appclient_call(appclient, (void **)&ret, "send", fd, b, count,
				0) != 0)
	{
		buffer_delete(b);
		return -1;
	}
#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s(%d, buf, %lu) => %d\n", __func__,
			fd, count, ret);
#endif
	buffer_delete(b);
	if(ret < 0)
		return _vpn_errno(_vpn_error, _vpn_error_cnt, -ret, 1);
	return ret;
}
