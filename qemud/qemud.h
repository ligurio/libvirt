/*
 * qemud.h: daemon data structure definitions
 *
 * Copyright (C) 2006-2008 Red Hat, Inc.
 * Copyright (C) 2006 Daniel P. Berrange
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307  USA
 *
 * Author: Daniel P. Berrange <berrange@redhat.com>
 */


#ifndef QEMUD_INTERNAL_H__
#define QEMUD_INTERNAL_H__

#include <config.h>

#include <gnutls/gnutls.h>
#include <gnutls/x509.h>
#include "gnutls_1_0_compat.h"
#if HAVE_SASL
#include <sasl/sasl.h>
#endif

#ifdef HAVE_POLKIT
#include <dbus/dbus.h>
#endif

#ifdef HAVE_SYS_SYSLIMITS_H
#include <sys/syslimits.h>
#endif

#include <rpc/types.h>
#include <rpc/xdr.h>
#include "remote_protocol.h"
#include "logging.h"

#ifdef __GNUC__
#ifdef HAVE_ANSIDECL_H
#include <ansidecl.h>
#endif
#ifndef ATTRIBUTE_UNUSED
#define ATTRIBUTE_UNUSED __attribute__((__unused__))
#endif
#ifndef ATTRIBUTE_FORMAT
#define ATTRIBUTE_FORMAT(args...) __attribute__((__format__ (args)))
#endif
#else
#define ATTRIBUTE_UNUSED
#define ATTRIBUTE_FORMAT(...)
#endif

#define qemudDebug DEBUG

enum qemud_mode {
    QEMUD_MODE_RX_HEADER,       /* Receiving the fixed length RPC header data */
    QEMUD_MODE_RX_PAYLOAD,      /* Receiving the variable length RPC payload data */
    QEMUD_MODE_WAIT_DISPATCH,   /* Message received, waiting for worker to process */
    QEMUD_MODE_IN_DISPATCH,     /* RPC call being processed */
    QEMUD_MODE_TX_PACKET,       /* Transmitting reply to RPC call */
    QEMUD_MODE_TLS_HANDSHAKE,   /* Performing TLS handshake */
};

/* Whether we're passing reads & writes through a sasl SSF */
enum qemud_sasl_ssf {
    QEMUD_SASL_SSF_NONE = 0,
    QEMUD_SASL_SSF_READ = 1,
    QEMUD_SASL_SSF_WRITE = 2,
};

enum qemud_sock_type {
    QEMUD_SOCK_TYPE_UNIX = 0,
    QEMUD_SOCK_TYPE_TCP = 1,
    QEMUD_SOCK_TYPE_TLS = 2,
};

/* Stores the per-client connection state */
struct qemud_client {
    PTHREAD_MUTEX_T(lock);

    int magic;

    int fd;
    int watch;
    int readonly:1;
    int closing:1;
    enum qemud_mode mode;

    struct sockaddr_storage addr;
    socklen_t addrlen;

    int type; /* qemud_sock_type */
    gnutls_session_t tlssession;
    int auth;
#if HAVE_SASL
    sasl_conn_t *saslconn;
    int saslSSF;
    const char *saslDecoded;
    unsigned int saslDecodedLength;
    unsigned int saslDecodedOffset;
    const char *saslEncoded;
    unsigned int saslEncodedLength;
    unsigned int saslEncodedOffset;
    char *saslUsername;
#endif

    unsigned int incomingSerial;
    unsigned int outgoingSerial;

    char buffer [REMOTE_MESSAGE_MAX];
    unsigned int bufferLength;
    unsigned int bufferOffset;

    /* This is only valid if a remote open call has been made on this
     * connection, otherwise it will be NULL.  Also if remote close is
     * called, it will be set back to NULL if that succeeds.
     */
    virConnectPtr conn;
    int refs;

    /* back-pointer to our server */
    struct qemud_server *server;
};

#define QEMUD_CLIENT_MAGIC 0x7788aaee


struct qemud_socket {
    int fd;
    int watch;
    int readonly;
    int type; /* qemud_sock_type */
    int auth;
    int port;
    struct qemud_socket *next;
};

/* Main server state */
struct qemud_server {
    pthread_mutex_t lock;
    pthread_cond_t job;

    int nworkers;
    pthread_t *workers;
    int nsockets;
    struct qemud_socket *sockets;
    int nclients;
    struct qemud_client **clients;

    int sigread;
    char logDir[PATH_MAX];
    unsigned int shutdown : 1;
#ifdef HAVE_AVAHI
    struct libvirtd_mdns *mdns;
#endif
#if HAVE_SASL
    char **saslUsernameWhitelist;
#endif
#if HAVE_POLKIT
    DBusConnection *sysbus;
#endif
};

void qemudLog(int priority, const char *fmt, ...)
    ATTRIBUTE_FORMAT(printf,2,3);


int qemudSetCloseExec(int fd);
int qemudSetNonBlock(int fd);

unsigned int
remoteDispatchClientRequest (struct qemud_server *server,
                             struct qemud_client *client);

void qemudDispatchClientWrite(struct qemud_server *server,
                             struct qemud_client *client);

#if HAVE_POLKIT
int qemudGetSocketIdentity(int fd, uid_t *uid, pid_t *pid);
#endif

int remoteRelayDomainEvent (virConnectPtr conn ATTRIBUTE_UNUSED,
                            virDomainPtr dom,
                            int event,
                            int detail,
                            void *opaque);

#endif
