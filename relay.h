#ifndef RELAY_H
#define RELAY_H

#include <stdint.h>
#include <stddef.h>
#include <pthread.h>

/* Maximum number of concurrent WebSocket clients */
#define MAX_CLIENTS 256

/* Message buffer size */
#define MSG_BUF_SIZE 65536

/* Client state */
typedef struct {
    struct lws *wsi;          /* libwebsocket handle */
    int active;               /* 1 = connected, 0 = free slot */
    uint64_t connected_at;    /* monotonic timestamp */
    unsigned char *frag_buf;  /* fragmentation accumulation buffer */
    size_t frag_len;          /* accumulated fragment length */
    size_t frag_cap;          /* fragmentation buffer capacity */
} client_t;

/* Global relay state */
typedef struct {
    client_t clients[MAX_CLIENTS];
    int client_count;
    pthread_mutex_t lock;
} relay_state_t;

extern relay_state_t relay;

/* Add/remove/query clients (caller must hold relay.lock) */
int  relay_add_client(struct lws *wsi);
void relay_remove_client(struct lws *wsi);

/* Fragment accumulation: returns 0=complete, 1=more fragments, -1=error */
int  relay_accumulate_fragment(struct lws *wsi, const void *data, size_t len,
                               unsigned char **out, size_t *out_len);

void relay_broadcast(const void *data, size_t len, struct lws *sender);

#endif /* RELAY_H */