#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Dedicated streaming JPEG downloader. Unlike the shared text/JSON fetcher
 * (network_http.h), this streams the response body into a fixed pre-allocated
 * PSRAM buffer with an image-specific deadline / stall / watchdog policy and
 * validates the result is a baseline JPEG (not PNG, not truncated).
 */

typedef enum {
    IMG_DL_OK = 0,
    IMG_DL_ERR_ARG,          /* bad argument */
    IMG_DL_ERR_CONNECT,      /* could not open / connect */
    IMG_DL_ERR_STATUS,       /* non-200 HTTP status */
    IMG_DL_ERR_TOO_LARGE,    /* Content-Length >= buffer, or body overflowed buffer */
    IMG_DL_ERR_EMPTY,        /* Content-Length 0 or fewer than 1024 bytes received */
    IMG_DL_ERR_TIMEOUT,      /* total deadline, stall, or single-chunk timeout */
    IMG_DL_ERR_CONN_LOST,    /* connection dropped mid-body */
    IMG_DL_ERR_INCOMPLETE,   /* fewer bytes than declared Content-Length */
    IMG_DL_ERR_NOT_JPEG,     /* PNG magic or missing FF D8 header */
} image_dl_status_t;

typedef struct {
    const char *url;             /* required */
    bool        skip_tls_verify; /* true -> HTTPS without cert validation (per-source toggle) */
    uint8_t    *buf;             /* required: pre-allocated PSRAM download buffer */
    size_t      buf_cap;         /* capacity of buf in bytes */
    /* Called at least every ~50 ms during the read loop so the caller can feed
     * the task watchdog. May be NULL. */
    void      (*watchdog_feed)(void);
} image_dl_req_t;

/**
 * @brief Download a JPEG into req->buf.
 *
 * On IMG_DL_OK, *out_len holds the byte count in req->buf. On any other status
 * the buffer contents are undefined and *out_len is 0.
 */
image_dl_status_t image_download(const image_dl_req_t *req, size_t *out_len);

/** Human-readable name for a status code (for logs). */
const char *image_dl_status_str(image_dl_status_t s);

#ifdef __cplusplus
}
#endif
