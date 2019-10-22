#ifndef INCLUDE_PROTURA_NET_SOCKET_H
#define INCLUDE_PROTURA_NET_SOCKET_H

#include <protura/fs/inode.h>
#include <protura/hlist.h>
#include <protura/net/sockaddr.h>
#include <protura/net/ipv4/ipv4.h>
#include <protura/net/ipv4/udp.h>
#include <protura/net/ipv4/tcp.h>

struct packet;
struct address_family;
struct protocol;

enum socket_flags {
    SOCKET_IS_BOUND,
};

enum socket_state {
    SOCKET_UNCONNECTED,
    SOCKET_CONNECTING,
    SOCKET_CONNECTED,
    SOCKET_DISCONNECTING,
};

struct socket {
    atomic_t refs;

    atomic_t state;
    int last_err;
    struct wait_queue state_changed;

    flags_t flags;

    int address_family;
    int sock_type;
    int protocol;

    list_node_t global_socket_entry;

    list_node_t proto_entry;

    list_node_t socket_entry;

    hlist_node_t socket_hash_entry;

    struct address_family *af;
    struct protocol *proto;

    /* Order of locking:
     *
     * private_lock:
     *     recv_lock
     *     send_lock
     */
    mutex_t private_lock;
    union {
        struct ipv4_socket_private ipv4;
    } af_private;

    union {
        struct udp_socket_private udp;
        struct tcp_socket_private tcp;
    } proto_private;

    mutex_t recv_lock;
    struct wait_queue recv_wait_queue;
    list_head_t recv_queue;
    list_head_t out_of_order_queue;

    mutex_t send_lock;
    struct wait_queue send_wait_queue;
    list_head_t send_queue;
};


#define SOCKET_INIT(sock) \
    { \
        .refs = ATOMIC_INIT(0), \
        .state = ATOMIC_INIT(SOCKET_UNCONNECTED), \
        .state_changed = WAIT_QUEUE_INIT((sock).state_changed, "socket-state-changed"), \
        .global_socket_entry = LIST_NODE_INIT((sock).global_socket_entry), \
        .proto_entry = LIST_NODE_INIT((sock).proto_entry), \
        .socket_entry = LIST_NODE_INIT((sock).socket_entry), \
        .socket_hash_entry = HLIST_NODE_INIT(), \
        .private_lock = MUTEX_INIT((sock).private_lock, "socket-private-lock"), \
        .recv_lock = MUTEX_INIT((sock).recv_lock, "socket-recv-lock"), \
        .recv_wait_queue = WAIT_QUEUE_INIT((sock).recv_wait_queue, "socket-recv-wait-queue"), \
        .recv_queue = LIST_HEAD_INIT((sock).recv_queue), \
        .out_of_order_queue = LIST_HEAD_INIT((sock).out_of_order_queue), \
        .send_lock = MUTEX_INIT((sock).send_lock, "socket-send-lock"), \
        .send_wait_queue = WAIT_QUEUE_INIT((sock).send_wait_queue, "socket-send-wait-queue"), \
        .send_queue = LIST_HEAD_INIT((sock).send_queue), \
    }

static inline void socket_init(struct socket *socket)
{
    *socket = (struct socket)SOCKET_INIT(*socket);
}

struct socket *socket_alloc(void);
void socket_free(struct socket *socket);

#ifdef CONFIG_KERNEL_LOG_SOCKET_REF
# define kp_socket_ref(str, ...) kp(KP_NORMAL, "SOCKET REF: " str, ## __VA_ARGS__)
#else
# define kp_socket_ref(str, ...) do { ; } while (0)
#endif

static inline struct socket *__socket_dup(struct socket *socket)
{
    atomic_inc(&socket->refs);
    return socket;
}

#define socket_dup(s) \
    ({ \
        kp_socket_ref("%s: %d: %p socket dup\n", __func__, __LINE__, (s)); \
        __socket_dup((s)); \
    })

static inline void __socket_put(struct socket *socket)
{
    if (atomic_dec_and_test(&socket->refs))
        socket_free(socket);
}

#define socket_put(s) \
    ({ \
        kp_socket_ref("%s: %d: %p socket put\n", __func__, __LINE__, (s)); \
        __socket_put((s)); \
    })

enum socket_state socket_state_cmpxchg(struct socket *socket, enum socket_state cur, enum socket_state new);;
void socket_state_change(struct socket *, enum socket_state);
int socket_last_error(struct socket *);
void socket_set_last_error(struct socket *socket, int err);

static inline enum socket_state socket_state_get(struct socket *socket)
{
    return atomic_get(&socket->state);
}

#define using_socket_priv(socket) \
    using_mutex(&(socket)->private_lock)

void socket_subsystem_init(void);

extern struct file_ops socket_procfs_file_ops;

int socket_open(int domain, int type, int protocol, struct socket **sock_ret);

int socket_send(struct socket *, const void *buf, size_t len, int flags);
int socket_recv(struct socket *, void *buf, size_t len, int flags);

int socket_sendto(struct socket *, const void *buf, size_t len, int flags, const struct sockaddr *dest, socklen_t addrlen, int nonblock);
int socket_recvfrom(struct socket *, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen, int nonblock);

int socket_bind(struct socket *, const struct sockaddr *addr, socklen_t addrlen);
int socket_getsockname(struct socket *, struct sockaddr *addr, socklen_t *addrlen);

int socket_setsockopt(struct socket *, int level, int optname, const void *optval, socklen_t optlen);
int socket_getsockopt(struct socket *, int level, int optname, void *optval, socklen_t *optlen);

/* FIXME: These should take a 'non-blocking' flag */
int socket_accept(struct socket *socket, struct sockaddr *addr, socklen_t *addrlen, struct socket **new_socket);
int socket_connect(struct socket *, const struct sockaddr *addr, socklen_t addrlen);
int socket_listen(struct socket *, int backlog);

int socket_shutdown(struct socket *, int how);
void socket_release(struct socket *);

#endif
