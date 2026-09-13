#include "crypto_common.h"
#include "protocols.h"

int main(void)
{
    test_stats_t s;
    auth_shared_t alice = { .secret = 0xC0FFEE1234ULL };
    uint64_t challenge, response;

    test_begin(&s, "Chapter 3: Basic Protocols");

    test_check(&s, "challenge available", auth_challenge(&challenge) == 0);
    response = auth_response(alice.secret, challenge);
    test_check(&s, "challenge-response verifies", auth_verify(&alice, challenge, response) == 1);
    test_check(&s, "wrong response rejected", auth_verify(&alice, challenge, response ^ 1) == 0);
    test_check(&s, "wrong secret rejected",
               auth_verify(&(auth_shared_t){ .secret = alice.secret + 1 }, challenge, response) == 0);

    /* replay: same challenge/response still verifies (by design of this simple form) */
    test_check(&s, "same challenge same response",
               auth_response(alice.secret, challenge) == response);

    return test_end(&s);
}
