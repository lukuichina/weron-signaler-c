#include "relay.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <libwebsockets.h>

relay_state_t relay;

int relay_add_client(struct lws *wsi) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (!relay.clients[i].active) {
            relay.clients[i].wsi = wsi;
            relay.clients[i].active = 1;
            relay.clients[i].connected_at = time(NULL);
            relay.clients[i].frag_buf = NULL;
            relay.clients[i].frag_len = 0;
            relay.clients[i].frag_cap = 0;
            relay.client_count++;
            return 0;
        }
    }
    return -1;
}

void relay_remove_client(struct lws *wsi) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (relay.clients[i].active && relay.clients[i].wsi == wsi) {
            relay.clients[i].active = 0;
            relay.clients[i].wsi = NULL;
            free(relay.clients[i].frag_buf);
            relay.clients[i].frag_buf = NULL;
            relay.clients[i].frag_len = 0;
            relay.clients[i].frag_cap = 0;
            relay.client_count--;
            return;
        }
    }
}

/*
 * Accumulate a fragment.  When the final fragment arrives, *out_len is set
 * to the complete message length and the caller owns the returned buffer
 * (or NULL on error).  Returns 0 on success, -1 on error.
 */
int relay_accumulate_fragment(struct lws *wsi, const void *data, size_t len,
                              unsigned char **out, size_t *out_len) {
    int idx = -1;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (relay.clients[i].active && relay.clients[i].wsi == wsi) {
            idx = i;
            break;
        }
    }
    if (idx < 0) return -1;

    client_t *c = &relay.clients[idx];

    /* Grow buffer if needed */
    if (c->frag_len + len > c->frag_cap) {
        size_t new_cap = c->frag_cap ? c->frag_cap * 2 : 4096;
        while (new_cap < c->frag_len + len) new_cap *= 2;

        unsigned char *nb = realloc(c->frag_buf, new_cap);
        if (!nb) return -1;
        c->frag_buf = nb;
        c->frag_cap = new_cap;
    }

    memcpy(c->frag_buf + c->frag_len, data, len);
    c->frag_len += len;

    /* libwebsockets signals final fragment via lws_is_final_fragment */
    if (lws_is_final_fragment(wsi)) {
        *out = c->frag_buf;
        *out_len = c->frag_len;

        /* Reset for next message */
        c->frag_buf = NULL;
        c->frag_len = 0;
        c->frag_cap = 0;
        return 0;
    }

    return 1; /* more fragments expected */
}

void relay_broadcast(const void *data, size_t len, struct lws *sender) {
    fprintf(stderr, "[C-RELAY] broadcasting %zu bytes to %d clients\n",
            len, relay.client_count);

    unsigned char *buf = malloc(LWS_PRE + len);
    if (!buf) {
        fprintf(stderr, "[C-RELAY] malloc failed for %zu bytes\n", LWS_PRE + len);
        return;
    }
    memcpy(buf + LWS_PRE, data, len);

    int sent = 0;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (relay.clients[i].active && relay.clients[i].wsi != sender) {
            int n = lws_write(relay.clients[i].wsi, buf + LWS_PRE, len,
                              LWS_WRITE_BINARY);
            if (n < 0) {
                fprintf(stderr, "[C-RELAY] lws_write error to client %d: %d\n", i, n);
            } else {
                sent++;
            }
        }
    }
    fprintf(stderr, "[C-RELAY] broadcast done: sent to %d clients\n", sent);
    free(buf);
}