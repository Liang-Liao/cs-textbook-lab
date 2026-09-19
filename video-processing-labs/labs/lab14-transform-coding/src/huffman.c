#include "huffman.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    int sym, freq, left, right;
} node;

enum { NS = 4096, SYM_OFF = 2048, MAXN = 8192 };

static int build_tree(const int *freq, const int *used, int nused, node *nodes, int *root) {
    int n = 0;
    int *heap = malloc((size_t)(nused > 0 ? nused : 1) * sizeof(int));
    if (!heap) return -1;
    int hn = 0;
    for (int i = 0; i < nused; i++) {
        int s = used[i];
        nodes[n].sym = s;
        nodes[n].freq = freq[s];
        nodes[n].left = nodes[n].right = -1;
        heap[hn++] = n++;
    }
    if (hn == 0) { free(heap); return -1; }
    if (hn == 1) { *root = heap[0]; free(heap); return n; }
    while (hn > 1) {
        int a = 0, b = 1;
        if (nodes[heap[b]].freq < nodes[heap[a]].freq) { int t = a; a = b; b = t; }
        for (int i = 2; i < hn; i++) {
            if (nodes[heap[i]].freq < nodes[heap[a]].freq) { b = a; a = i; }
            else if (nodes[heap[i]].freq < nodes[heap[b]].freq) b = i;
        }
        nodes[n].sym = -1;
        nodes[n].freq = nodes[heap[a]].freq + nodes[heap[b]].freq;
        nodes[n].left = heap[a];
        nodes[n].right = heap[b];
        heap[a] = n;
        heap[b] = heap[--hn];
        n++;
    }
    *root = heap[0];
    free(heap);
    return n;
}

static void assign(int ni, const node *nodes, uint32_t *code, int *len, uint32_t c, int l) {
    if (ni < 0) return;
    if (nodes[ni].left < 0 && nodes[ni].right < 0) {
        code[nodes[ni].sym] = c;
        len[nodes[ni].sym] = l > 0 ? l : 1;
        return;
    }
    assign(nodes[ni].left, nodes, code, len, (c << 1), l + 1);
    assign(nodes[ni].right, nodes, code, len, (c << 1) | 1u, l + 1);
}

/* Sparse header: n(4B) + nnz(2B) + nnz * (int16 symbol, uint32 freq).
 * Frequencies are serialized as uint32 — a uint16 would silently clamp at
 * 65535 and desynchronize the decoder's tree on large streams. */
int huffman_encode(const int *sym, int n, uint8_t *out, int out_cap, int *out_bits) {
    if (!sym || n <= 0 || !out || out_cap < 16) return -1;
    int *freq = calloc(NS, sizeof(int));
    if (!freq) return -1;
    for (int i = 0; i < n; i++) {
        int s = sym[i] + SYM_OFF;
        if (s < 0 || s >= NS) { free(freq); return -1; }
        freq[s]++;
    }
    int *used = malloc((size_t)NS * sizeof(int));
    if (!used) { free(freq); return -1; }
    int nused = 0;
    for (int i = 0; i < NS; i++) if (freq[i] > 0) used[nused++] = i;

    int hdr = 4 + 2 + nused * 6;
    if (out_cap < hdr + 8) { free(freq); free(used); return -1; }
    memcpy(out, &n, 4);
    uint16_t nnz = (uint16_t)nused;
    memcpy(out + 4, &nnz, 2);
    for (int i = 0; i < nused; i++) {
        int16_t sv = (int16_t)(used[i] - SYM_OFF);
        uint32_t fv = (uint32_t)freq[used[i]];
        memcpy(out + 6 + i * 6, &sv, 2);
        memcpy(out + 8 + i * 6, &fv, 4);
    }

    node *nodes = malloc((size_t)MAXN * sizeof(node));
    uint32_t *code = calloc(NS, sizeof(uint32_t));
    int *len = calloc(NS, sizeof(int));
    if (!nodes || !code || !len) {
        free(freq); free(used); free(nodes); free(code); free(len);
        return -1;
    }
    int root = -1;
    if (build_tree(freq, used, nused, nodes, &root) < 0) {
        free(freq); free(used); free(nodes); free(code); free(len);
        return -1;
    }
    assign(root, nodes, code, len, 0, 0);
    memset(out + hdr, 0, (size_t)(out_cap - hdr));
    int bitpos = 0;
    for (int i = 0; i < n; i++) {
        int s = sym[i] + SYM_OFF;
        for (int b = len[s] - 1; b >= 0; b--) {
            int bit = (code[s] >> b) & 1;
            int bp = bitpos++;
            out[hdr + (bp >> 3)] |= (uint8_t)(bit << (7 - (bp & 7)));
        }
    }
    if (out_bits) *out_bits = bitpos;
    free(freq); free(used); free(code); free(len); free(nodes);
    return hdr + (bitpos + 7) / 8;
}

int huffman_decode(const uint8_t *in, int in_bytes, int *sym, int max_n) {
    if (!in || !sym || in_bytes < 6) return -1;
    int n = 0;
    memcpy(&n, in, 4);
    if (n <= 0 || n > max_n) return -1;
    uint16_t nnz = 0;
    memcpy(&nnz, in + 4, 2);
    int hdr = 6 + (int)nnz * 6;
    if (in_bytes < hdr) return -1;
    int *freq = calloc(NS, sizeof(int));
    int *used = malloc((size_t)(nnz > 0 ? nnz : 1) * sizeof(int));
    if (!freq || !used) { free(freq); free(used); return -1; }
    for (int i = 0; i < (int)nnz; i++) {
        int16_t sv;
        uint32_t fv;
        memcpy(&sv, in + 6 + i * 6, 2);
        memcpy(&fv, in + 8 + i * 6, 4);
        int s = (int)sv + SYM_OFF;
        if (s < 0 || s >= NS) { free(freq); free(used); return -1; }
        freq[s] = (int)fv;
        used[i] = s;
    }
    node *nodes = malloc((size_t)MAXN * sizeof(node));
    if (!nodes) { free(freq); free(used); return -1; }
    int root = -1;
    if (build_tree(freq, used, nnz, nodes, &root) < 0) {
        free(freq); free(used); free(nodes); return -1;
    }
    int bitpos = 0;
    int total_bits = (in_bytes - hdr) * 8;
    for (int i = 0; i < n; i++) {
        int ni = root;
        while (nodes[ni].left >= 0) {
            if (bitpos >= total_bits) { free(freq); free(used); free(nodes); return -1; }
            int b = (in[hdr + (bitpos >> 3)] >> (7 - (bitpos & 7))) & 1;
            bitpos++;
            ni = b ? nodes[ni].right : nodes[ni].left;
            if (ni < 0) { free(freq); free(used); free(nodes); return -1; }
        }
        sym[i] = nodes[ni].sym - SYM_OFF;
    }
    free(freq);
    free(used);
    free(nodes);
    return n;
}
