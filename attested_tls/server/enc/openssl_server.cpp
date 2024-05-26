#include <arpa/inet.h>
#include <openenclave/enclave.h>
#include <openssl/evp.h>
#include <openssl/ssl.h>
#include <openssl/rsa.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include "../../common/openssl_utility.h"
#include "gen_shares.h"
#include <openenclave/corelibc/stdlib.h>
#include <stdio.h>
#include <openenclave/seal.h>

static char *key1 = NULL;
static char *key2 = NULL;
static const char *publicHex = NULL;

extern "C"
{
    void generate_keypair(uint8_t** out_sealed_blob, size_t* out_sealed_size);

    int set_up_tls_server(char *server_port, bool keep_server_up);

    void unseal_data(uint8_t* sealed_blob, size_t sealed_size);
};

int verify_callback(int preverify_ok, X509_STORE_CTX *ctx);

int create_listener_socket(int port, int &server_socket)
{
    int ret = -1;
    const int reuse = 1;
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    server_socket = socket(AF_INET, SOCK_STREAM, 0);

    if (server_socket < 0)
    {
        printf(TLS_SERVER "socket creation failed\n");
        goto exit;
    }

    if (setsockopt(
            server_socket,
            SOL_SOCKET,
            SO_REUSEADDR,
            (const void *)&reuse,
            sizeof(reuse)) < 0)
    {
        perror("TLS server: Setsockopt failed");
        goto exit;
    }

    if (bind(server_socket, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("TLS server: Unable to bind socket to the port");
        goto exit;
    }

    if (listen(server_socket, 20) < 0)
    {
        perror("TLS server: Unable to open socket for listening");
        close(server_socket);
        goto exit;
    }
    ret = 0;
exit:
    return ret;
}

int handle_communication_until_done(
    int &server_socket_fd,
    int &client_socket_fd,
    SSL_CTX *&ssl_server_ctx,
    SSL *&ssl_session,
    bool keep_server_up,
    char *share,
    char * share2,
    const char * publicHex)
{
    int ret = -1;
    unsigned char buf[10];
    int read_len = 0;
    int bytes_read = 0;

waiting_for_connection_request:

    // reset ssl_session and client_socket_fd to prepare for the new TLS
    // connection
    close(client_socket_fd);
    SSL_free(ssl_session);
    printf(TLS_SERVER " waiting for client connection\n");

    struct sockaddr_in addr;
    uint len = sizeof(addr);

    client_socket_fd = accept(server_socket_fd, (struct sockaddr *)&addr, &len);
    
    if (client_socket_fd < 0)
    {
        perror("TLS server: Unable to accept the client request");
        goto exit;
    }

    // create a new SSL structure for a connection
    if ((ssl_session = SSL_new(ssl_server_ctx)) == nullptr)
    {
        printf(TLS_SERVER
               "Unable to create a new SSL connection state object\n");
        goto exit;
    }

    SSL_set_fd(ssl_session, client_socket_fd);

    // wait for a TLS/SSL client to initiate a TLS/SSL handshake
    if (SSL_accept(ssl_session) <= 0)
    {
        printf(TLS_SERVER " SSL handshake failed\n");
        goto exit;
    }

    printf(TLS_SERVER "<---- Read from client:\n");
    read_len = sizeof(buf) - 1;
    memset(buf, 0, sizeof(buf));
    bytes_read = SSL_read(ssl_session, buf, (size_t)read_len);
    printf("Received: %s\n", buf);
    if (bytes_read <= 0)
    {
        printf("error\n");
    }

    printf(TLS_SERVER "<---- Write to client:\n");
    if (strcmp((const char*)buf, CLIENT_PAYLOAD)) {      
        if (write_to_session_peer(
                ssl_session, share2, strlen(share2)) != 0)
        {
            printf(TLS_SERVER " Write to client failed\n");
            goto exit;
        }

        if (write_to_session_peer(
                ssl_session, publicHex, strlen(publicHex)) != 0)
        {
            printf(TLS_SERVER " Write to client failed\n");
            goto exit;
        }
    }

    if (strcmp((const char*)buf, CLIENT_PAYLOAD2)) {      
        if (write_to_session_peer(
                ssl_session, share, strlen(share)) != 0)
        {
            printf(TLS_SERVER " Write to client failed\n");
            goto exit;
        }
    }

    if (keep_server_up)
        goto waiting_for_connection_request;

    ret = 0;
exit:
    close(client_socket_fd);
    return ret;
}

int set_up_tls_server(char *server_port, bool keep_server_up)
{
    int ret = 0;
    int server_socket_fd = 0;
    int client_socket_fd = 0;
    int server_port_number = 0;

    X509 *certificate = nullptr;
    EVP_PKEY *pkey = nullptr;
    SSL_CONF_CTX *ssl_confctx = SSL_CONF_CTX_new();

    SSL_CTX *ssl_server_ctx = nullptr;
    SSL *ssl_session = nullptr;

    /* Load host resolver and socket interface modules explicitly */
    if (atoi(server_port) == 12341) {
        if (load_oe_modules() != OE_OK)
        {
            printf(TLS_SERVER "loading required Open Enclave modules failed\n");
            goto exit;
        }

        if ((ssl_server_ctx = SSL_CTX_new(TLS_server_method())) == nullptr)
        {
            printf(TLS_SERVER "unable to create a new SSL context\n");
            goto exit;
        }

        if (initalize_ssl_context(ssl_confctx, ssl_server_ctx) != OE_OK)
        {
            printf(TLS_SERVER "unable to create a initialize SSL context\n ");
            goto exit;
        }

        SSL_CTX_set_verify(ssl_server_ctx, SSL_VERIFY_PEER, &verify_callback);

        if (load_tls_certificates_and_keys(ssl_server_ctx, certificate, pkey) != 0)
        {
            printf(TLS_SERVER
                " unable to load certificate and private key on the server\n ");
            goto exit;
        }

        server_port_number = atoi(server_port);

        printf("\n_uuid_sgx_ecdsa: ");
        for (int i = 0; i < 16; i++) {
            printf("%02x", _uuid_sgx_ecdsa.b[i]);
            if (i == 3 || i == 5 || i == 7 || i == 9) {
                printf("-");
            }
        }
        printf("\n");

        if (create_listener_socket(server_port_number, server_socket_fd) != 0)
        {
            printf(TLS_SERVER " unable to create listener socket on the server\n ");
            goto exit;
        }

        printf("Share 1: %s\n", key1);

        ret = handle_communication_until_done(
            server_socket_fd,
            client_socket_fd,
            ssl_server_ctx,
            ssl_session,
            keep_server_up,
            key1,
            key2,
            publicHex);

        if (ret != 0)
        {
            printf(TLS_SERVER "server communication error %d\n", ret);
            goto exit;
        }
    }

exit:
    if (client_socket_fd > 0)
        close(client_socket_fd);
    if (server_socket_fd > 0)
        close(server_socket_fd);

    if (ssl_session)
    {
        SSL_shutdown(ssl_session);
        SSL_free(ssl_session);
    }
    if (ssl_server_ctx)
        SSL_CTX_free(ssl_server_ctx);
    if (ssl_confctx)
        SSL_CONF_CTX_free(ssl_confctx);
    if (certificate)
        X509_free(certificate);
    if (pkey)
        EVP_PKEY_free(pkey);
    return (ret);
}

void generate_keypair(uint8_t** out_sealed_blob, size_t* out_sealed_size)
{
    const char *privateHex = nullptr;
    BIGNUM *bnKey = BN_new();
    char *bnStr = nullptr;
    BIGNUM *bnRand = nullptr;
    char *points = nullptr;
    char **shares = nullptr;
    KeyPair keypair = {nullptr, 0, nullptr, 0};

    uint8_t *sealed_blob = NULL;
    size_t sealed_size = 0;
    oe_result_t result = OE_FAILURE;

    // der_data = generate_and_return_key_pair(der_length);
    keypair = generate_and_return_key_pair();

    privateHex = generateHexString(keypair.private_key, keypair.private_key_length);
    publicHex = generateHexString(keypair.public_key, keypair.public_key_length);

    // Convert the BIGNUM back to a decimal string for printing
    bnStr = BN_bn2dec(bnKey);
    BN_hex2bn(&bnKey, privateHex);
    // printf("Serialized Key as BIGNUM: %s\n", bnStr);

    // Random Big Number
    bnRand = generate_random_bignum(512);

    // print_bignum(bnRand);

    points = get_polynomial_points(bnRand, bnKey);

    // printf("Polynomial Points: %s\n", points);

    shares = get_shares(points);

    key1 = shares[0];
    key2 = shares[1];

    const char* opt_msg = "sealing";

    oe_seal_setting_t settings[] = {
        OE_SEAL_SET_POLICY(OE_SEAL_POLICY_UNIQUE)
    };

    result = oe_seal(
        NULL,
        settings,
        1,
        (const uint8_t *)key2,
        strlen(key2) + 1,
        (unsigned char*)opt_msg,
        strlen(opt_msg),
        &sealed_blob,
        &sealed_size
    );

    if (result != OE_OK)
    {
        oe_free(sealed_blob);
        sealed_blob = NULL;
        sealed_size = 0;
        fprintf(stderr, "Sealing failed: %s\n", oe_result_str(result));
    }

    *out_sealed_blob = sealed_blob;
    *out_sealed_size = sealed_size;

    // print shares
    // for (int i = 0; i < 2; i++)
    // {
    //     printf("Part %d: %s\n", i + 1, shares[i]);
    // }

    if (privateHex)
        free((void *)privateHex);
    if (bnKey)
        BN_free(bnKey);
    if (bnStr)
        OPENSSL_free(bnStr);
    if (bnRand)
        BN_free(bnRand);
    if (points)
        free(points);
    // if (shares)
    //     for (int i = 0; i < 2; i++)
    //     {
    //         if (shares[i] != nullptr)
    //         {
    //             free(shares[i]);
    //         }
    //     }
    //     free(shares);
}

// int generate_keypair(uint8_t** out_sealed_blob, size_t* out_sealed_size) {
//     int der_length = 0;
//     unsigned char *der_data = nullptr;
//     const char *privateHex = nullptr;
//     BIGNUM *bnKey = BN_new();
//     char *bnStr = nullptr;
//     BIGNUM *bnRand = nullptr;
//     char *points = nullptr;
//     char **shares = nullptr;

//     uint8_t *sealed_blob = NULL;
//     size_t sealed_size = 0;
//     oe_result_t result = OE_FAILURE;

//     der_data = generate_and_return_key_pair(der_length);

//     privateHex = generateHexString(der_data, der_length);

//     // Convert the BIGNUM back to a decimal string for printing
//     bnStr = BN_bn2dec(bnKey);
//     // printf("Serialized Key as BIGNUM: %s\n", bnStr);

//     // Random Big Number
//     bnRand = generate_random_bignum(512);

//     // print_bignum(bnRand);

//     points = get_polynomial_points(bnRand, bnKey);

//     // printf("Polynomial Points: %s\n", points);

//     shares = get_shares(points);

//     key1 = shares[0];
//     key2 = shares[1];

//     size_t key2_len = key2 ? strlen(key2) + 1 : 0;
//     if (key2_len == 0) {
//         fprintf(stderr, "Invalid key length\n");
//         return -1;
//     }

//     oe_seal_setting_t settings[] = {
//         OE_SEAL_SET_POLICY(OE_SEAL_POLICY_UNIQUE) // This initializes the settings array with the unique policy
//     };

//     result = oe_seal(
//         NULL,                  // Target the enclave's identity based on the policy
//         settings,              // Seal settings
//         1,                     // Number of settings
//         (const uint8_t *)key2, // Data to seal (plaintext data)
//         strlen(key2) + 1,      // Size of the data to seal, including null terminator
//         NULL,                  // No additional data
//         0,                     // No additional data size
//         &sealed_blob,          // Output: sealed data blob
//         &sealed_size           // Output: size of sealed data blob
//     );

//     if (result != OE_OK)
//     {
//         oe_free(sealed_blob);
//         sealed_blob = NULL;
//         sealed_size = 0;
//         fprintf(stderr, "Sealing failed: %s\n", oe_result_str(result));
//     }

//     *out_sealed_blob = sealed_blob;
//     *out_sealed_size = sealed_size;

//     // print shares
//     // for (int i = 0; i < 2; i++)
//     // {
//     //     printf("Part %d: %s\n", i + 1, shares[i]);
//     // }

//     if (der_data)
//         free(der_data);
//     if (privateHex)
//         free((void *)privateHex);
//     if (bnKey)
//         BN_free(bnKey);
//     if (bnStr)
//         OPENSSL_free(bnStr);
//     if (bnRand)
//         BN_free(bnRand);
//     if (points)
//         free(points);
//     // if (shares)
//     //     for (int i = 0; i < 2; i++)
//     //     {
//     //         if (shares[i] != nullptr)
//     //         {
//     //             free(shares[i]);
//     //         }
//     //     }
//     //     free(shares);
// }



void unseal_data(uint8_t* sealed_blob, size_t sealed_size) {
    oe_result_t result;
    uint8_t* unsealed_data = NULL;
    size_t unsealed_data_size = 0;
    const char* opt_msg = "sealing";

    // Attempt to unseal the sealed blob
    result = oe_unseal(
        sealed_blob,
        sealed_size,
        (unsigned char*)opt_msg, 
        strlen(opt_msg),
        &unsealed_data,
        &unsealed_data_size);

    if (result != OE_OK) {
        printf("Unsealing failed: %s\n", oe_result_str(result));
        // Optionally, handle specific error codes differently
        switch (result) {
            case OE_UNSUPPORTED:
                printf("The operation is not supported with the current enclave configuration.\n");
                break;
            case OE_INVALID_PARAMETER:
                printf("Invalid parameter passed to oe_unseal(). Check your buffers and lengths.\n");
                break;
            default:
                printf("An unexpected error occurred.\n");
        }
    }

    // Print unsealed data if it's null-terminated string
    if (unsealed_data != NULL) {
        // Optionally, ensure unsealed data is safe to print
        if (unsealed_data[unsealed_data_size - 1] == '\0') {
            printf("Unsealed data: %s\n", unsealed_data);
        } else {
            // For binary data, you would handle it differently
            printf("Unsealed data is binary and not null-terminated.\n");
        }
    }
}
