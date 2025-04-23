# Logical bugs

# TLS1.3

/**/ means different from other targets but no RFC violations

### OpenSSL

Fuzz Server:

/*wrong version (mutated to downgrade) but alert 40 (should be 70)*/

invalid compression method alert 50 (should be 47)

invalid legacy_version connected

Fuzz Client:

invalid legacy_version connected

### wolfssl

Fuzz Server:

/*delete supported versions value alert 50 (should be 70)

delete supported_versions extension alert 47 (should be 109)*/

delete sig_algs extension connected

Invalid supported_group value connected

/*delete psk exchange mode value connected*/

Fuzz Client:

wrong supported_version value alert 70 (should be 47) 

### gnutls

Fuzz Server:

/*wrong version but alert 40 (should be 70)*/

no cert sent but alert 20 (should be 10)

/*delete supported_versions extension alert 40 (should be 109)*/

Extra DHE support_group extension value alert 47 (should be connected)

/*Delete ec_point_formats extension value DoS*/

delete sig_algs extension alert 40 (should be 109)

/*delete psk exchange mode value connected*/

invalid legacy_version connected

Fuzz Client:

wrong supported_version value alert 70 (should be 47)

duplicated supported_version extension connected

### mbedtls

Fuzz Server:

server force_version is tls13 but client can tls12

/*server force_version is tls12, client tls13 but alert 40 (should be 70)

wrong version (mutated to downgrade) but alert 40 (should be 70)*/

duplicated any extension connected

delete/invalid supported_groups value connected

/*Add a invalid psk exchange mode value alert 47 (should be connected)*/

/*delete psk exchange mode value connected*/

Fuzz Client:

duplicated supported_version extension connected

### matrixssl

Fuzz Server:

no cert sent but alert 20 (should be 10)

Auth request but no cert alert 46 (should be 116)

duplicated any extension connected

delete/invalid supported_versions extension connected

delete sig_algs extension alert 40 (should be 109)

delete sig_algs extension memory leak

invalid legacy_version connected (include 0x0300)

Fuzz Client:

invalid legacy_version connected

Wrong cipher suite cause SEGV

delete supported_versions value memory leak

duplicated supported_version extension connected

### Libressl

Fuzz Server:

/*delete supported_versions type (malformed packet) alert 70 (should be 50)*/

/*Extra ec_point_formats extension value DoS*/

/*delete psk exchange mode value connected*/

Invalid supported_group value connected

duplicated ec_point_format connected

Fuzz Client:

wrong supported_version value alert 70 (should be 47)

# TLS1.2

| Target | gnutl | matirxssl | libressl |
| --- | --- | --- | --- |
| Fuzz server | Invalid ec_poiny_formats value connected | delete sig_alg extensions refused; duplicated any extension connected | no sig_alg value connected; no suppoorted_groups overlap connected |
| Fuzz client | duplicated ec_poiny_format extension connected | serverhello with extension not in clienthello connected |  |

| Target | mbedtls | openssl | wolfssl |
| --- | --- | --- | --- |
| Fuzz server | duplicated any extension connected; delete supported_groups extension refused; wrong compression_method connected | delete supported_groups extension refused | no supported_groups value connected;  |
| Fuzz client | serverhello with extension not in clienthello connected; duplicated ec_poiny_format extension connected; sig_alg extension connected |  |  |

no suppoorted_groups overlap connected alert 40 （should be 47)：openssl、mbedtls、gnutls

compression_method wrong alert 50 (should be 47)：openssl