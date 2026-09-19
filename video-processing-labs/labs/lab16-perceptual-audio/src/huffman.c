/* copied from lab14-transform-coding (adapted for lab16-perceptual-audio) */
#include "huffman.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    int sym, freq, left, right;
} node;

static int build_tree(const int *freq, int nsym, node *nodes, int *root) {
    int n = 0, heap[1024], hn = 0;
    for (int i = 0; i < nsym; i++) {
        if (freq[i] <= 0) continue;
        nodes[n].sym = i;
        nodes[n].freq = freq[i];
        nodes[n].left = nodes[n].right = -1;
        heap[hn++] = n++;
    }
    if (hn == 0) return -1;
    if (hn == 1) { *root = heap[0]; return n; }
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

enum { NS = 512 };

int huffman_encode(const int *sym, int n, uint8_t *out, int out_cap, int *out_bits) {
    if (!sym || n <= 0 || !out || out_cap < NS * 2 + 8) return -1;
    int *freq = calloc(NS, sizeof(int));
    if (!freq) return -1;
    for (int i = 0; i < n; i++) {
        int s = sym[i] + 256;
        if (s < 0 || s >= NS) { free(freq); return -1; }
        freq[s]++;
    }
    memcpy(out, &n, 4);
    for (int i = 0; i < NS; i++) {
        uint16_t f = (uint16_t)(freq[i] > 65535 ? 65535 : freq[i]);
        memcpy(out + 4 + i * 2, &f, 2);
    }
    static node nodes[2048];
    int root = -1;
    if (build_tree(freq, NS, nodes, &root) < 0) { free(freq); return -1; }
    uint32_t *code = calloc(NS, sizeof(uint32_t));
    int *len = calloc(NS, sizeof(int));
    if (!code || !len) { free(freq); free(code); free(len); return -1; }
    assign(root, nodes, code, len, 0, 0);
    int hdr = 4 + NS * 2;
    memset(out + hdr, 0, (size_t)(out_cap - hdr));
    int bitpos = 0;
    for (int i = 0; i < n; i++) {
        int s = sym[i] + 256;
        for (int b = len[s] - 1; b >= 0; b--) {
            int bit = (int)((code[s] >> b) & 1u);
            int bp = bitpos++;
            out[hdr + (bp >> 3)] |= (uint8_t)(bit << (7 - (bp & 7)));
        }
    }
    if (out_bits) *out_bits = bitpos;
    free(freq); free(code); free(len);
    return hdr + (bitpos + 7) / 8;
}

int huffman_decode(const uint8_t *in, int in_bytes, int *sym, int max_n) {
    if (!in || !sym || in_bytes < 4 + NS * 2) return -1;
    int n = 0;
    memcpy(&n, in, 4);
    if (n <= 0 || n > max_n) return -1;
    int *freq = calloc(NS, sizeof(int));
    if (!freq) return -1;
    for (int i = 0; i < NS; i++) {
        uint16_t f;
        memcpy(&f, in + 4 + i * 2, 2);
        freq[i] = f;
    }
    static node nodes[2048];
    int root = -1;
    if (build_tree(freq, NS, nodes, &root) < 0) { free(freq); return -1; }
    int hdr = 4 + NS * 2;
    int bitpos = 0;
    int total_bits = (in_bytes - hdr) * 8;
    for (int i = 0; i < n; i++) {
        int ni = root;
        while (nodes[ni].left >= 0) {
            if (bitpos >= total_bits) { free(freq); return -1; }
            int b = (in[hdr + (bitpos >> 3)] >> (7 - (bitpos & 7))) & 1;
            bitpos++;
            ni = b ? nodes[ni].right : nodes[ni].left;
            if (ni < 0) { free(freq); return -1; }
        }
        sym[i] = nodes[ni].sym - 256;
    }
    free(freq);
    return n;
}
