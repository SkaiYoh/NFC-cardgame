#include "card_psd_export.h"
#include <math.h>
#include <raylib.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    unsigned char *data;
    size_t len;
    size_t cap;
    bool ok;
} ByteBuf;

typedef struct {
    const char *name;
    int left;
    int top;
    int width;
    int height;
    Color *pixels;
} PSDLayer;

static const uint16_t PSD_LAYER_CHANNEL_IDS[4] = {0, 1, 2, 0xFFFFu};

static void buf_init(ByteBuf *buf) {
    memset(buf, 0, sizeof(*buf));
    buf->ok = true;
}

static void buf_free(ByteBuf *buf) {
    free(buf->data);
    memset(buf, 0, sizeof(*buf));
}

static bool buf_reserve(ByteBuf *buf, size_t extra) {
    if (!buf->ok) return false;
    if (extra == 0) return true;
    if (buf->len > SIZE_MAX - extra) {
        buf->ok = false;
        return false;
    }
    if (buf->len + extra <= buf->cap) return true;

    size_t need = buf->len + extra;
    size_t cap = buf->cap ? buf->cap : 256;
    while (cap < need) {
        if (cap > SIZE_MAX / 2) {
            cap = need;
            break;
        }
        cap *= 2;
    }

    unsigned char *next = realloc(buf->data, cap);
    if (!next) {
        buf->ok = false;
        return false;
    }

    buf->data = next;
    buf->cap = cap;
    return true;
}

static bool buf_append(ByteBuf *buf, const void *data, size_t len) {
    if (len == 0) return buf->ok;
    if (!buf_reserve(buf, len)) return false;
    memcpy(buf->data + buf->len, data, len);
    buf->len += len;
    return true;
}

static bool buf_u8(ByteBuf *buf, unsigned char v) {
    return buf_append(buf, &v, 1);
}

static bool buf_be16(ByteBuf *buf, uint16_t v) {
    unsigned char b[2] = {
        (unsigned char)((v >> 8) & 0xFF),
        (unsigned char)(v & 0xFF)
    };
    return buf_append(buf, b, sizeof(b));
}

static bool buf_be32(ByteBuf *buf, uint32_t v) {
    unsigned char b[4] = {
        (unsigned char)((v >> 24) & 0xFF),
        (unsigned char)((v >> 16) & 0xFF),
        (unsigned char)((v >> 8) & 0xFF),
        (unsigned char)(v & 0xFF)
    };
    return buf_append(buf, b, sizeof(b));
}

static bool buf_zeroes(ByteBuf *buf, size_t len) {
    static const unsigned char zeros[16] = {0};
    while (len > 0) {
        size_t chunk = (len < sizeof(zeros)) ? len : sizeof(zeros);
        if (!buf_append(buf, zeros, chunk)) return false;
        len -= chunk;
    }
    return true;
}

static size_t pascal4_len(const char *name) {
    size_t len = strlen(name);
    if (len > 255) len = 255;
    size_t total = 1 + len;
    while ((total % 4) != 0) total++;
    return total;
}

static bool buf_pascal4(ByteBuf *buf, const char *name) {
    size_t len = strlen(name);
    if (len > 255) len = 255;
    if (!buf_u8(buf, (unsigned char)len)) return false;
    if (!buf_append(buf, name, len)) return false;
    return buf_zeroes(buf, pascal4_len(name) - (1 + len));
}

static unsigned char color_channel(Color c, int channel) {
    switch (channel) {
        case 0: return c.r;
        case 1: return c.g;
        case 2: return c.b;
        case 3: return c.a;
        default: return 0;
    }
}

static Color *copy_colors(const Color *src, size_t count) {
    if (!src || count == 0) return NULL;
    Color *copy = malloc(count * sizeof(Color));
    if (!copy) return NULL;
    memcpy(copy, src, count * sizeof(Color));
    return copy;
}

static bool trim_layer_bounds(PSDLayer *layer) {
    int minX = layer->width;
    int minY = layer->height;
    int maxX = -1;
    int maxY = -1;

    for (int y = 0; y < layer->height; y++) {
        for (int x = 0; x < layer->width; x++) {
            if (layer->pixels[y * layer->width + x].a == 0) continue;
            if (x < minX) minX = x;
            if (y < minY) minY = y;
            if (x > maxX) maxX = x;
            if (y > maxY) maxY = y;
        }
    }

    if (maxX < minX || maxY < minY) return false;
    if (minX == 0 && minY == 0 && maxX == layer->width - 1 && maxY == layer->height - 1) return true;

    int newWidth = maxX - minX + 1;
    int newHeight = maxY - minY + 1;
    Color *cropped = malloc((size_t)newWidth * (size_t)newHeight * sizeof(Color));
    if (!cropped) return false;

    for (int y = 0; y < newHeight; y++) {
        memcpy(cropped + y * newWidth,
               layer->pixels + (minY + y) * layer->width + minX,
               (size_t)newWidth * sizeof(Color));
    }

    free(layer->pixels);
    layer->pixels = cropped;
    layer->left += minX;
    layer->top += minY;
    layer->width = newWidth;
    layer->height = newHeight;
    return true;
}

static bool file_write(FILE *f, const void *data, size_t len) {
    return (len == 0) || (fwrite(data, 1, len, f) == len);
}

static bool file_be16(FILE *f, uint16_t v) {
    unsigned char b[2] = {
        (unsigned char)((v >> 8) & 0xFF),
        (unsigned char)(v & 0xFF)
    };
    return file_write(f, b, sizeof(b));
}

static bool file_be32(FILE *f, uint32_t v) {
    unsigned char b[4] = {
        (unsigned char)((v >> 24) & 0xFF),
        (unsigned char)((v >> 16) & 0xFF),
        (unsigned char)((v >> 8) & 0xFF),
        (unsigned char)(v & 0xFF)
    };
    return file_write(f, b, sizeof(b));
}

static int packbits_run_length(const unsigned char *row, int width, int start) {
    int run = 1;
    while (start + run < width && run < 128 && row[start + run] == row[start]) run++;
    return run;
}

static bool buf_packbits_row(ByteBuf *buf, const unsigned char *row, int width) {
    int i = 0;
    while (i < width) {
        int run = packbits_run_length(row, width, i);
        if (run >= 3) {
            if (!buf_u8(buf, (unsigned char)(257 - run))) return false;
            if (!buf_u8(buf, row[i])) return false;
            i += run;
            continue;
        }

        int literalStart = i;
        int literalLen = 0;
        while (i < width) {
            run = packbits_run_length(row, width, i);
            if (run >= 3) break;
            if (literalLen + run > 128) break;
            literalLen += run;
            i += run;
            if (literalLen == 128) break;
        }

        if (!buf_u8(buf, (unsigned char)(literalLen - 1))) return false;
        if (!buf_append(buf, row + literalStart, (size_t)literalLen)) return false;
    }
    return true;
}

static bool encode_rle_plane(ByteBuf *encoded, uint16_t *rowLengths,
                             const Color *pixels, int width, int height, int channel) {
    bool ok = true;
    unsigned char *row = NULL;

    row = malloc((size_t)width);
    if (!row) return false;

    for (int y = 0; ok && y < height; y++) {
        for (int x = 0; x < width; x++) {
            row[x] = color_channel(pixels[y * width + x], channel);
        }

        size_t startLen = encoded->len;
        ok = buf_packbits_row(encoded, row, width);
        if (!ok) break;

        size_t rowLen = encoded->len - startLen;
        if (rowLen > UINT16_MAX) {
            ok = false;
            break;
        }
        rowLengths[y] = (uint16_t)rowLen;
    }

    free(row);
    return ok;
}

static bool buf_rle_channel(ByteBuf *buf, const Color *pixels, int width, int height, int channel) {
    bool ok = true;
    ByteBuf encoded;
    uint16_t *rowLengths = NULL;

    if (!buf_be16(buf, 1)) return false;

    buf_init(&encoded);
    rowLengths = malloc((size_t)height * sizeof(*rowLengths));
    if (!rowLengths) ok = false;

    if (ok) ok = encode_rle_plane(&encoded, rowLengths, pixels, width, height, channel);
    for (int y = 0; ok && y < height; y++) {
        ok = buf_be16(buf, rowLengths[y]);
    }
    if (ok) ok = buf_append(buf, encoded.data, encoded.len);

    free(rowLengths);
    buf_free(&encoded);
    return ok;
}

static bool buf_rle_image(ByteBuf *buf, const Color *pixels, int width, int height, int channelCount) {
    bool ok = true;
    ByteBuf encoded;
    uint16_t *rowLengths = NULL;

    if (!buf_be16(buf, 1)) return false;

    buf_init(&encoded);
    rowLengths = malloc((size_t)channelCount * (size_t)height * sizeof(*rowLengths));
    if (!rowLengths) ok = false;

    for (int channel = 0; ok && channel < channelCount; channel++) {
        ok = encode_rle_plane(&encoded, rowLengths + (channel * height),
                              pixels, width, height, channel);
    }

    for (int channel = 0; ok && channel < channelCount; channel++) {
        for (int y = 0; y < height; y++) {
            ok = buf_be16(buf, rowLengths[channel * height + y]);
            if (!ok) break;
        }
    }
    if (ok) ok = buf_append(buf, encoded.data, encoded.len);

    free(rowLengths);
    buf_free(&encoded);
    return ok;
}

static void blend_over(Color *dst, Color src) {
    if (src.a == 0) return;
    if (src.a == 255 || dst->a == 0) {
        *dst = src;
        return;
    }

    float sa = (float)src.a / 255.0f;
    float da = (float)dst->a / 255.0f;
    float outA = sa + da * (1.0f - sa);
    if (outA <= 0.0f) {
        *dst = (Color){0, 0, 0, 0};
        return;
    }

    float srcScale = sa / outA;
    float dstScale = (da * (1.0f - sa)) / outA;

    dst->r = (unsigned char)lroundf(src.r * srcScale + dst->r * dstScale);
    dst->g = (unsigned char)lroundf(src.g * srcScale + dst->g * dstScale);
    dst->b = (unsigned char)lroundf(src.b * srcScale + dst->b * dstScale);
    dst->a = (unsigned char)lroundf(outA * 255.0f);
}

static void composite_layer(Color *canvas, const PSDLayer *layer) {
    for (int y = 0; y < layer->height; y++) {
        int dstY = layer->top + y;
        if (dstY < 0 || dstY >= CARD_HEIGHT) continue;

        for (int x = 0; x < layer->width; x++) {
            int dstX = layer->left + x;
            if (dstX < 0 || dstX >= CARD_WIDTH) continue;

            blend_over(&canvas[dstY * CARD_WIDTH + dstX], layer->pixels[y * layer->width + x]);
        }
    }
}

static void free_layers(PSDLayer *layers, int layerCount) {
    for (int i = 0; i < layerCount; i++) {
        free(layers[i].pixels);
    }
}

bool card_export_psd(const CardAtlas *atlas, const CardVisual *visual, const char *path) {
    if (!atlas || !visual || !path || path[0] == '\0') return false;

    bool ok = true;
    PSDLayer layers[CARD_LAYER_COUNT] = {0};
    int layerCount = 0;
    Color *composite = calloc((size_t)CARD_WIDTH * (size_t)CARD_HEIGHT, sizeof(Color));
    ByteBuf layerRecords;
    ByteBuf layerPixels;
    ByteBuf compositePixels;
    Image sheetImage = {0};
    FILE *f = NULL;

    buf_init(&layerRecords);
    buf_init(&layerPixels);
    buf_init(&compositePixels);

    if (!composite) ok = false;

    if (ok) {
        sheetImage = LoadImage(CARD_SHEET_PATH);
        if (!sheetImage.data) ok = false;
    }

    if (ok) ImageFormat(&sheetImage, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);

    for (int i = 0; ok && i < CARD_LAYER_COUNT; i++) {
        CardLayerExportInfo info;
        if (!card_layer_export_info(atlas, visual, i, &info)) {
            ok = false;
            break;
        }
        if (!info.visible || info.source.width <= 0.0f || info.source.height <= 0.0f) continue;

        Image layerImage = ImageFromImage(sheetImage, info.source);
        ImageFormat(&layerImage, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);

        Color *layerPixelsRaw = LoadImageColors(layerImage);
        Color *ownedPixels = NULL;
        if (!layerPixelsRaw) {
            ok = false;
        } else {
            ownedPixels = copy_colors(layerPixelsRaw, (size_t)layerImage.width * (size_t)layerImage.height);
            UnloadImageColors(layerPixelsRaw);
            if (!ownedPixels) ok = false;
        }

        int layerWidth = layerImage.width;
        int layerHeight = layerImage.height;
        UnloadImage(layerImage);
        if (!ok) break;

        layers[layerCount].name = info.name;
        layers[layerCount].left = (int)lroundf(info.bounds.x);
        layers[layerCount].top = (int)lroundf(info.bounds.y);
        layers[layerCount].width = layerWidth;
        layers[layerCount].height = layerHeight;
        layers[layerCount].pixels = ownedPixels;

        if (!trim_layer_bounds(&layers[layerCount])) {
            free(layers[layerCount].pixels);
            layers[layerCount].pixels = NULL;
            continue;
        }

        composite_layer(composite, &layers[layerCount]);
        layerCount++;
    }

    for (int i = layerCount - 1; ok && i >= 0; i--) {
        const PSDLayer *layer = &layers[i];
        ByteBuf channelData[4];
        uint32_t channelSizes[4] = {0};

        for (int channel = 0; channel < 4; channel++) {
            buf_init(&channelData[channel]);
            ok = buf_rle_channel(&channelData[channel], layer->pixels, layer->width, layer->height, channel);
            if (!ok) break;
            if (channelData[channel].len > UINT32_MAX) {
                ok = false;
                break;
            }
            channelSizes[channel] = (uint32_t)channelData[channel].len;
        }

        size_t extraLen = 8 + pascal4_len(layer->name);
        if (extraLen > UINT32_MAX) ok = false;

        if (ok) {
            ok = buf_be32(&layerRecords, (uint32_t)layer->top);
            ok = ok && buf_be32(&layerRecords, (uint32_t)layer->left);
            ok = ok && buf_be32(&layerRecords, (uint32_t)(layer->top + layer->height));
            ok = ok && buf_be32(&layerRecords, (uint32_t)(layer->left + layer->width));
            ok = ok && buf_be16(&layerRecords, 4);

            for (int channel = 0; ok && channel < 4; channel++) {
                ok = buf_be16(&layerRecords, PSD_LAYER_CHANNEL_IDS[channel]);
                ok = ok && buf_be32(&layerRecords, channelSizes[channel]);
            }

            ok = ok && buf_append(&layerRecords, "8BIM", 4);
            ok = ok && buf_append(&layerRecords, "norm", 4);
            ok = ok && buf_u8(&layerRecords, 255);
            ok = ok && buf_u8(&layerRecords, 0);
            ok = ok && buf_u8(&layerRecords, 0);
            ok = ok && buf_u8(&layerRecords, 0);
            ok = ok && buf_be32(&layerRecords, (uint32_t)extraLen);
            ok = ok && buf_be32(&layerRecords, 0);
            ok = ok && buf_be32(&layerRecords, 0);
            ok = ok && buf_pascal4(&layerRecords, layer->name);
        }

        for (int channel = 0; channel < 4; channel++) {
            if (ok) ok = buf_append(&layerPixels, channelData[channel].data, channelData[channel].len);
            buf_free(&channelData[channel]);
        }
    }

    if (ok) ok = buf_rle_image(&compositePixels, composite, CARD_WIDTH, CARD_HEIGHT, 4);

    if (ok) {
        f = fopen(path, "wb");
        if (!f) ok = false;
    }

    if (ok) {
        uint32_t layerInfoLen = 0;
        uint32_t layerAndMaskLen = 0;

        if (layerCount > 0) {
            size_t totalLayerInfoLen = 2 + layerRecords.len + layerPixels.len;
            if ((totalLayerInfoLen & 1u) != 0u) totalLayerInfoLen++;
            if (totalLayerInfoLen > UINT32_MAX - 8) {
                ok = false;
            } else {
                layerInfoLen = (uint32_t)totalLayerInfoLen;
                layerAndMaskLen = 4 + layerInfoLen + 4;
            }
        }

        ok = ok && file_write(f, "8BPS", 4);
        ok = ok && file_be16(f, 1);
        ok = ok && file_write(f, "\0\0\0\0\0\0", 6);
        ok = ok && file_be16(f, 4);
        ok = ok && file_be32(f, CARD_HEIGHT);
        ok = ok && file_be32(f, CARD_WIDTH);
        ok = ok && file_be16(f, 8);
        ok = ok && file_be16(f, 3);

        ok = ok && file_be32(f, 0);
        ok = ok && file_be32(f, 0);

        ok = ok && file_be32(f, layerAndMaskLen);
        if (ok && layerCount > 0) {
            ok = file_be32(f, layerInfoLen);
            ok = ok && file_be16(f, (uint16_t)(int16_t)(-layerCount));
            ok = ok && file_write(f, layerRecords.data, layerRecords.len);
            ok = ok && file_write(f, layerPixels.data, layerPixels.len);
            if (ok && ((2 + layerRecords.len + layerPixels.len) & 1u) != 0u) {
                ok = file_write(f, "\0", 1);
            }
            ok = ok && file_be32(f, 0);
        }

        ok = ok && file_write(f, compositePixels.data, compositePixels.len);
    }

    if (f) fclose(f);
    if (sheetImage.data) UnloadImage(sheetImage);
    buf_free(&layerRecords);
    buf_free(&layerPixels);
    buf_free(&compositePixels);
    free_layers(layers, layerCount);
    free(composite);

    return ok;
}
