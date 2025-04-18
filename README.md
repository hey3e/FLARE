![image](https://github.com/user-attachments/assets/a7adcaac-020e-4c9e-b1e7-3e0f241610a9)# FLARE

## Fault Injection
Use [OpenSSL 1.1.1j](https://github.com/openssl/openssl/tree/OpenSSL_1_1_1j) as an example.

### 1. Compile mttlib.so and rtlib.so
- `mttlib.so`: LLVM instrumentation pass, which is used for injecting faults into programs
```
clang++-12 -shared -fPIC -o mttlib.so mttlib.cpp `llvm-config-12 --cxxflags --ldflags --libs`
```
- `rtlib.so`: External mutation lib
```
clang++-12 -shared -fPIC -o rtlib.so rtlib.cpp `llvm-config-12 --cxxflags --ldflags --libs`
```


### 2. Build OpenSSL
- Add code in  `/openssl/apps/s_server.c` to record coverage (Modify path in code)
```
#include <sanitizer/coverage_interface.h>

static uint64_t total_blocks_executed = 0;
static uint64_t unique_blocks_executed = 0;

void __sanitizer_cov_trace_pc_guard_init(uint32_t *start, uint32_t *stop) {
    static uint64_t counter = 0;
    if (start == stop || *start) return;
    for (uint32_t *x = start; x < stop; x++) *x = ++counter;
    total_blocks_executed = stop - start;
}

void __sanitizer_cov_trace_pc_guard(uint32_t *guard) {
    if (!*guard) {
        return;
    }
    unique_blocks_executed++;
    *guard = 0;
}

void print_coverage_rate() {
    double coverage_rate = (double)unique_blocks_executed / (double)total_blocks_executed * 100.0;
    printf("Coverage rate: %.2f%% (%" PRIu64 " branches executed, %" PRIu64 " total branches)\n", coverage_rate, unique_blocks_executed, total_blocks_executed);
    // Write unique_blocks_executed to a file
    FILE *file = fopen("/path/to/coverage.log", "w");
    if (file == NULL) {
        printf("Error opening file!\n");
        return;
    }
    fprintf(file, "%" PRIu64 "\n", unique_blocks_executed);
    fclose(file);
}
```
- Build (Modify path in `clang-fuzzwithin`)
```
export CC=/path/to/clang-fuzzwithin
./config && make -j8
```

### 3. Fuzz
- Run server
```
./apps/openssl s_server -accept 127.0.0.1:4433 -cert cert.pem -key key.pem -Verify 1
```
- Run  `agent.py` (Modify path in code)
```
python3 agent.py
```

## Logical Flaw Fault
Use [OpenSSL 1.1.1j](https://github.com/openssl/openssl/tree/OpenSSL_1_1_1j) as an example. We leverage OpenSSL s_client and s_server as mutated client and server to connect to peers.

### 1. Client Side Fault
The fault of client side focuses on `WPACKET_XXX` functions in `ssl/statem_clnt.c` and `ssl/extensions_clnt.c`

For example, the `signature_algrithoms` extension is controlled by `tls_construct_ctos_sig_algs` function in `ssl/extensions_clnt.c`
```
EXT_RETURN tls_construct_ctos_sig_algs(SSL *s, WPACKET *pkt,
                                       unsigned int context, X509 *x,
                                       size_t chainidx)
{
    size_t salglen;
    const uint16_t *salg;

    if (!SSL_CLIENT_USE_SIGALGS(s))
        return EXT_RETURN_NOT_SENT;

    salglen = tls12_get_psigalgs(s, 1, &salg);

    /*
    if (!WPACKET_put_bytes_u16(pkt, TLSEXT_TYPE_signature_algorithms)
               /* Sub-packet for sig-algs extension */
            || !WPACKET_start_sub_packet_u16(pkt)
               /* Sub-packet for the actual list */
            || !WPACKET_start_sub_packet_u16(pkt)
            || !tls12_copy_sigalgs(s, pkt, salg, salglen)
            || !WPACKET_close(pkt)
            || !WPACKET_close(pkt)) {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, SSL_F_TLS_CONSTRUCT_CTOS_SIG_ALGS,
                 ERR_R_INTERNAL_ERROR);
        return EXT_RETURN_FAIL;
    }
    */

    return EXT_RETURN_SENT;
}
```
- Remove extension: delete /* */ snippet
- Duplicate extension: cv /* */ snippet
- Modify value: add `!WPACKET_put_bytes_u16(pkt, xxx)` between `!WPACKET_start_sub_packet_u16(pkt)` and `!WPACKET_close(pkt)`

### 2. Server Side Fault
The fault of server side focuses on `WPACKET_XXX` functions in `ssl/statem_srvr.c` and `ssl/extensions_srvr.c`


### 1. Compile call.so, modify clang-fuzzwithin, and rebuild OpenSSL
- Get result as follows, leverage LLM to filter test, debug, error functions, and so on, find the interset
![image](https://github.com/user-attachments/assets/a16e1482-6cdd-4e11-85c5-7bea0abd0e5d)

### 2. Modify Replace.cpp with interset and compile it to Replace.so

### 3. Modify ssl/statem/extensions.c like extensions_instru.c

### 4. Build OpenSSL agian, get the mutated client, you can
- Implement Replace strategy with environment variable REPLACE, whose value equals to one instrument site.
- Implement Remove and Repeat strategies with environment variable REPEAT_REMOVE, where 1 to activate.
- Use environment variable ENVSEED to control the mutation. If ENVSEED is same, the mutation will be identical.

### 5. Differential testing
- Start servers under test
  ```
  stdbuf -oL -eL ./apps/openssl s_server -accept 127.0.0.1:4433 -cert /path/to/cert.pem -key /path/to/key.pem -CAfile /path/to/CA.pem -Verify 1 | grep --line-buffered '\[+\]' > plus4433.log
  ```
  ```
  stdbuf -oL -eL ./examples/server/server -v 4 -p 4434 -i -x | grep --line-buffered '\[+\]' | tee plus4434.log
  ```
  ```
  stdbuf -oL -eL ./apps/openssl/openssl s_server -accept 4435 -cert cert.pem -key key.pem -Verify 1 -tls1_3 | grep --line-buffered '\[+\]' | tee plus4435.log
  ```
- Run diff.py

### 6. Server state instrumentation
- Align state value using LLM
![image](https://github.com/user-attachments/assets/afa01da6-4a10-4097-88bb-a0b5a77b9cb2)
- Instrument aligned value and print standard value. For example, locate code "xxx = TLS_ST_SR_CLNT_HELLO" and instrument "printf("%s\n", "[+]RECVD_CH");"
