/*
 * Copyright 2001 Niels Provos <provos@citi.umich.edu>
 * All rights reserved.
 *
 * Copyright (c) 2002 Matthieu Herrb
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *    - Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    - Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following
 *      disclaimer in the documentation and/or other materials provided
 *      with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "lldpd.h"

#include <sys/param.h>
#include <sys/uio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void
send_fd(int sock, int fd)
{
	struct msghdr msg;
	union {
		struct cmsghdr hdr;
		char buf[CMSG_SPACE(sizeof(int))];
	} cmsgbuf;
	struct cmsghdr *cmsg;
	struct iovec vec;
	int result = 0;
	ssize_t n;

	memset(&msg, 0, sizeof(msg));

	if (fd >= 0) {
		msg.msg_control = (caddr_t)&cmsgbuf.buf;
		msg.msg_controllen = sizeof(cmsgbuf.buf);
		cmsg = CMSG_FIRSTHDR(&msg);
		cmsg->cmsg_len = CMSG_LEN(sizeof(int));
		cmsg->cmsg_level = SOL_SOCKET;
		cmsg->cmsg_type = SCM_RIGHTS;
		*(int *)CMSG_DATA(cmsg) = fd;
	} else {
		result = errno;
	}

	vec.iov_base = &result;
	vec.iov_len = sizeof(int);
	msg.msg_iov = &vec;
	msg.msg_iovlen = 1;

	if ((n = sendmsg(sock, &msg, 0)) == -1)
		LLOG_WARN("sendmsg(%d)", sock);
	if (n != sizeof(int))
		LLOG_WARNX("sendmsg: expected sent 1 got %ld",
		    (long)n);
}

int
receive_fd(int sock)
{
	struct msghdr msg;
	union {
		struct cmsghdr hdr;
		char buf[CMSG_SPACE(sizeof(int))];
	} cmsgbuf;
	struct cmsghdr *cmsg;
	struct iovec vec;
	ssize_t n;
	int result;
	int fd;

	memset(&msg, 0, sizeof(msg));
	vec.iov_base = &result;
	vec.iov_len = sizeof(int);
	msg.msg_iov = &vec;
	msg.msg_iovlen = 1;
	msg.msg_control = &cmsgbuf.buf;
	msg.msg_controllen = sizeof(cmsgbuf.buf);

	if ((n = recvmsg(sock, &msg, 0)) == -1)
		LLOG_WARN("recvmsg");
	if (n != sizeof(int))
		LLOG_WARNX("recvmsg: expected received 1 got %ld",
		    (long)n);
	if (result == 0) {
		cmsg = CMSG_FIRSTHDR(&msg);
		if (cmsg == NULL) {
			LLOG_WARNX("no message header");
			return -1;
		}
		if (cmsg->cmsg_type != SCM_RIGHTS)
			LLOG_WARNX("expected type %d got %d",
			    SCM_RIGHTS, cmsg->cmsg_type);
		fd = (*(int *)CMSG_DATA(cmsg));
		return fd;
	} else {
		errno = result;
		return -1;
	}
}