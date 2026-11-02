#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <getopt.h>

#include <libwebsockets.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

#include "relay.h"

#define DEFAULT_PORT 8788
#define DEFAULT_IFACE "127.0.0.1"

#ifdef _WIN32
/* On Windows, libwebsockets may modify the iface buffer in-place.
 * Use a mutable stack buffer instead of a string literal. */
static char win_iface_buf[256];
#endif

static volatile int interrupted = 0;

static void sig_handler(int sig) {
    (void)sig;
    interrupted = 1;
}

static void print_usage(const char *prog) {
    fprintf(stderr,
        "Usage: %s [OPTIONS]\n"
        "\n"
        "Options:\n"
        "  -i, --interface ADDR   Listening address (default: %s)\n"
        "  -p, --port PORT        Listening port (default: %d)\n"
        "  -h, --help             Show this help message\n"
        "\n"
        "Example:\n"
        "  %s -i 0.0.0.0 -p 8788\n"
        "  %s -i 127.0.0.1 -p 9090\n",
        prog, DEFAULT_IFACE, DEFAULT_PORT, prog, prog);
}

/* WebSocket callback — handles connect / disconnect / data events */
static int callback_wss(struct lws *wsi, enum lws_callback_reasons reason,
                        void *user, void *in, size_t len) {
    (void)user;
    switch (reason) {

    case LWS_CALLBACK_ESTABLISHED:
        pthread_mutex_lock(&relay.lock);
        if (relay_add_client(wsi) != 0) {
            pthread_mutex_unlock(&relay.lock);
            return -1;
        }
        pthread_mutex_unlock(&relay.lock);
        fprintf(stderr, "[C-RELAY] client connected, total=%d\n",
                relay.client_count);
        break;

    case LWS_CALLBACK_CLOSED:
        pthread_mutex_lock(&relay.lock);
        relay_remove_client(wsi);
        pthread_mutex_unlock(&relay.lock);
        fprintf(stderr, "[C-RELAY] client closed, total=%d\n",
                relay.client_count);
        break;

    case LWS_CALLBACK_RECEIVE:
        if (len == 0 || in == NULL) break;

        pthread_mutex_lock(&relay.lock);

        /* libwebsockets fragmentation helpers:
         * - lws_is_first_fragment: true for first frame (incl. single-frame)
         * - lws_is_final_fragment:   true for last frame */
        int first = lws_is_first_fragment(wsi);
        int final = lws_is_final_fragment(wsi);

        fprintf(stderr, "[C-RELAY] RECEIVE %zu bytes, first=%d final=%d, clients=%d\n",
                len, first, final, relay.client_count);

        if (first && final) {
            /* Single-frame message — broadcast directly */
            relay_broadcast(in, len, wsi);
        } else {
            /* Multi-frame message — accumulate */
            unsigned char *complete = NULL;
            size_t complete_len = 0;
            int rc = relay_accumulate_fragment(wsi, in, len,
                                                &complete, &complete_len);
            if (rc == 0 && complete != NULL) {
                /* Final fragment — broadcast the reassembled message */
                relay_broadcast(complete, complete_len, wsi);
                free(complete);
            } else if (rc < 0) {
                fprintf(stderr, "[C-RELAY] fragment accumulation error\n");
            }
            /* rc == 1: more fragments expected, nothing to do */
        }

        pthread_mutex_unlock(&relay.lock);
        break;

    default:
        break;
    }

    return 0;
}

/* Protocol definition */
static const struct lws_protocols protocols[] = {
    {
        "weron-signaler",          /* name */
        callback_wss,              /* callback */
        0,                         /* per_session_data_size */
        65536,                     /* rx_buffer_size — large enough for encrypted SDP */
        0,                         /* id */
        NULL,                      /* user */
        0,                         /* tx_packet_size */
    },
    { NULL, NULL, 0, 0, 0, NULL, 0 } /* sentinel */
};

int main(int argc, char *argv[]) {
#ifdef _WIN32
    /* Copy default interface into mutable buffer */
    strncpy(win_iface_buf, DEFAULT_IFACE, sizeof(win_iface_buf) - 1);
    win_iface_buf[sizeof(win_iface_buf) - 1] = '\0';
    const char *iface = win_iface_buf;
#else
    const char *iface = DEFAULT_IFACE;
#endif
    int port = DEFAULT_PORT;

    /* Parse command-line arguments */
    struct option long_opts[] = {
        { "interface", required_argument, NULL, 'i' },
        { "port",      required_argument, NULL, 'p' },
        { "help",      no_argument,       NULL, 'h' },
        { NULL, 0, NULL, 0 }
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "i:p:h", long_opts, NULL)) != -1) {
        switch (opt) {
        case 'i':
            iface = optarg;
            break;
        case 'p':
            port = atoi(optarg);
            if (port < 0 || port > 65535) {
                fprintf(stderr, "[C-RELAY] invalid port: %s (must be 0-65535)\n",
                        optarg);
                return 1;
            }
            break;
        case 'h':
            print_usage(argv[0]);
            return 0;
        default:
            print_usage(argv[0]);
            return 1;
        }
    }

    signal(SIGINT, sig_handler);
#ifndef _WIN32
    signal(SIGTERM, sig_handler);
#endif

#ifdef _WIN32
    /* libwebsockets on Windows requires Winsock initialization before
     * lws_create_context.  Without WSAStartup the context creation
     * silently fails with an invalid-parameter crash. */
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        fprintf(stderr, "[C-RELAY] WSAStartup failed\n");
        return 1;
    }
#endif

    /* Initialize relay state */
    memset(&relay, 0, sizeof(relay));
    if (pthread_mutex_init(&relay.lock, NULL) != 0) {
        fprintf(stderr, "[C-RELAY] pthread_mutex_init failed\n");
        return 1;
    }

    /* liblws context creation */
    struct lws_context_creation_info info;
    memset(&info, 0, sizeof(info));

    info.port = port;
    info.iface = iface;
    info.protocols = protocols;
    info.extensions = NULL;
    info.mounts = NULL;
#ifdef LWS_WITH_SSL
    info.ssl_cert_filepath = NULL;
    info.ssl_private_key_filepath = NULL;
    info.ssl_private_key_password = NULL;
    info.ssl_cipher_list = NULL;
#endif
    info.timeout_secs = 10;
    info.server_string = "weron-signaler-c";
    info.error_document_404 = NULL;
    info.gid = -1;
    info.uid = -1;
    info.options = 0;

    struct lws_context *ctx = lws_create_context(&info);
    if (!ctx) {
        fprintf(stderr, "[C-RELAY] lws_create_context failed\n");
        return 1;
    }

    /* When port=0, the kernel assigns a random port; query the actual one */
    int actual_port = port;
    if (port == 0) {
        struct lws_vhost *vh = lws_get_vhost_by_name(ctx, "default");
        if (vh != NULL) {
            actual_port = lws_get_vhost_port(vh);
        }
    }

    fprintf(stderr, "[C-RELAY] listening on %s:%d\n", iface, actual_port);

    /* Event loop */
    while (!interrupted) {
        lws_service(ctx, 100);
    }

    fprintf(stderr, "[C-RELAY] shutting down\n");
    lws_context_destroy(ctx);
    pthread_mutex_destroy(&relay.lock);

#ifdef _WIN32
    WSACleanup();
#endif

    return 0;
}