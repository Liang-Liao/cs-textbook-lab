#include "crypto_common.h"
#include "secret_share.h"
#include "ticket.h"

int main(void)
{
    test_stats_t s;
    test_begin(&s, "Chapter 4: Intermediate Protocols");

    /* --- Shamir (3,5) over a small prime --- */
    {
        const uint64_t p = 65537;
        const uint64_t secret = 0xC0FFEE % p;
        ss_share_t shares[SS_MAX_SHARES];
        uint64_t rec = 0;
        int i;

        test_check(&s, "ss_split 3-of-5",
                   ss_split(secret, 5, 3, p, shares) == 0);

        /* any 3 shares recover */
        test_check(&s, "recover shares 0,1,2",
                   ss_recover(shares, 3, 3, p, &rec) == 0 && rec == secret);
        test_check(&s, "recover shares 1,3,4",
                   ss_recover(&shares[1], 3, 3, p, &rec) == 0 && rec == secret);
        {
            ss_share_t pick[3] = { shares[0], shares[2], shares[4] };
            test_check(&s, "recover shares 0,2,4",
                       ss_recover(pick, 3, 3, p, &rec) == 0 && rec == secret);
        }

        /* two shares must NOT give the real secret (usually) */
        {
            uint64_t wrong = 0;
            ss_recover(shares, 2, 3, p, &wrong); /* count < k fails */
            test_check(&s, "reject fewer than k shares",
                       ss_recover(shares, 2, 3, p, &wrong) != 0);
        }

        /* different splits give different shares */
        {
            ss_share_t b[SS_MAX_SHARES];
            ss_split(secret, 5, 3, p, b);
            test_check(&s, "shares are randomized",
                       memcmp(shares, b, sizeof(shares)) != 0);
        }
        (void)i;
    }

    /* --- Ticket issue/open --- */
    {
        ticket_plain_t pt, rt;
        ticket_t t;
        const uint64_t key = 0x1122334455667788ULL;
        memset(&pt, 0, sizeof(pt));
        memcpy(pt.client_id, "alice\0\0\0", 8);
        memcpy(pt.server_id, "filesv\0\0", 8);
        pt.session_key = 0xDEADBEEFCAFEBABEULL;
        pt.timestamp = 1000;
        pt.lifetime = 3600;
        ticket_issue(&pt, key, &t);
        test_check(&s, "ticket_open recovers fields",
                   ticket_open(&t, key, &rt) == 0 &&
                   memcmp(&pt, &rt, sizeof(pt)) == 0);
        test_check(&s, "wrong key garbles ticket",
                   ticket_open(&t, key ^ 1, &rt) != 0 ||
                   memcmp(&pt, &rt, sizeof(pt)) != 0);
    }

    /* --- Timestamp chain --- */
    {
        ts_chain_t c;
        uint64_t h0, h1;
        uint8_t d1[] = {1, 2, 3};
        uint8_t d2[] = {9, 8};
        ts_chain_init(&c);
        h0 = c.h;
        ts_chain_append(&c, 100, d1, 3);
        h1 = c.h;
        test_check(&s, "chain advances",
                   ts_chain_check_link(h0, 0, 100, d1, 3, h1));
        test_check(&s, "chain rejects wrong data",
                   !ts_chain_check_link(h0, 0, 100, d2, 2, h1));
        ts_chain_append(&c, 101, d2, 2);
        test_check(&s, "second link ok",
                   ts_chain_check_link(h1, 1, 101, d2, 2, c.h));
    }

    return test_end(&s);
}
