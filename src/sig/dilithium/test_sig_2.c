// SPDX-License-Identifier: MIT

#if defined(_WIN32)
#pragma warning(disable : 4244 4293)
#endif


#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <oqs/oqs.h>
#include "params.h"

#include <oqs/oqs.h>

#if OQS_USE_PTHREADS_IN_TESTS
#include <pthread.h>
#endif

#include "pqcrystals-dilithium_dilithium2_ref/sign.h"
#include "ds_benchmark.h"

#ifdef OQS_ENABLE_TEST_CONSTANT_TIME
#include <valgrind/memcheck.h>
#define OQS_TEST_CT_CLASSIFY(addr, len)  VALGRIND_MAKE_MEM_UNDEFINED(addr, len)
#define OQS_TEST_CT_DECLASSIFY(addr, len)  VALGRIND_MAKE_MEM_DEFINED(addr, len)
#else
#define OQS_TEST_CT_CLASSIFY(addr, len)
#define OQS_TEST_CT_DECLASSIFY(addr, len)
#endif



typedef struct magic_s {
	uint8_t val[31];
} magic_t;

static OQS_STATUS sig_test_correctness(const char *method_name) {
    
    PRINT_CURRENT_TIME
    printf("\n");
    
    OQS_SIG *sig = NULL;
    uint8_t *public_key = NULL;
    uint8_t *secret_key = NULL;
    uint8_t *message = NULL;
    size_t message_len = 5;
    uint8_t *signature = NULL;
    size_t signature_len;
    OQS_STATUS rc, ret = OQS_ERROR;
    
    //The magic numbers are random values.
    //The length of the magic number was chosen to be 31 to break alignment
    // magic_tを初期化
    magic_t magic;
    OQS_randombytes(magic.val, sizeof(magic_t));
    
    // sig_lenの実体を作成して初期化
    size_t actual_sig_len;
    size_t *sig_len = &actual_sig_len;
    
    sig = OQS_SIG_new(method_name);
    if (sig == NULL) {
        fprintf(stderr, "ERROR: OQS_SIG_new failed\n");
        goto err;
    }
    
    public_key = malloc(sig->length_public_key + 2 * sizeof(magic_t));
    secret_key = malloc(sig->length_secret_key + 2 * sizeof(magic_t));
    message = malloc(message_len + 2 * sizeof(magic_t));
    signature = malloc(sig->length_signature + 2 * sizeof(magic_t));
    
    
    printf("================================================================================\n");
    printf("Sample computation for signature %s\n", sig->method_name);
    printf("================================================================================\n");
    
    uint8_t *original_secret_key = secret_key;
    uint8_t *original_public_key = public_key;
    

    
    if ((public_key == NULL) || (secret_key == NULL) || (message == NULL) || (signature == NULL)) {
        fprintf(stderr, "ERROR: malloc failed\n");
        goto err;
    }

    printf("malloc has done\n");
    
    //Set the magic numbers before
    memcpy(public_key, magic.val, sizeof(magic_t));
    memcpy(secret_key, magic.val, sizeof(magic_t));
    memcpy(message, magic.val, sizeof(magic_t));
    memcpy(signature, magic.val, sizeof(magic_t));
    
    public_key += sizeof(magic_t);
    secret_key += sizeof(magic_t);
    message += sizeof(magic_t);
    signature += sizeof(magic_t);
    
    // and after
    memcpy(public_key + sig->length_public_key, magic.val, sizeof(magic_t));
    memcpy(secret_key + sig->length_secret_key, magic.val, sizeof(magic_t));
    memcpy(message + message_len, magic.val, sizeof(magic_t));
    memcpy(signature + sig->length_signature, magic.val, sizeof(magic_t));
    
    OQS_randombytes(message, message_len);
    OQS_TEST_CT_DECLASSIFY(message, message_len);
    
    rc = mask_crypto_sign_keypair(public_key, secret_key);
    OQS_TEST_CT_DECLASSIFY(&rc, sizeof rc);
    if (rc != OQS_SUCCESS) {
        fprintf(stderr, "ERROR: OQS_SIG_keypair failed\n");
        goto err;
    }
    printf("keypair has done\n");
    
    rc = mask_crypto_sign(signature, &signature_len, message, message_len, secret_key);
    OQS_TEST_CT_DECLASSIFY(&rc, sizeof rc);
    if (rc != OQS_SUCCESS) {
        fprintf(stderr, "ERROR: OQS_SIG_sign failed\n");
        goto err;
    }
    printf("sign has done\n");
    
    
    printf("sk is ");
    for (size_t i = 0; i < sig->length_secret_key; i++) {
        printf("%02x", original_secret_key[i]);
    }
    printf("\n");

    printf("pk is ");
    for (size_t i = 0; i < sig->length_public_key; i++) {
        printf("%02x", original_public_key[i]);
    }
    printf("\n");
    
    printf("sig is ");
    for (size_t i = 0; i < sig->length_signature; i++) {
        printf("%02x", signature[i]);
    }
    
    printf("\n");
    
    
    OQS_TEST_CT_DECLASSIFY(public_key, sig->length_public_key);
    OQS_TEST_CT_DECLASSIFY(signature, signature_len);
    rc = mask_crypto_sign_verify((uint8_t *)sig, *sig_len,message, message_len,public_key);
    OQS_TEST_CT_DECLASSIFY(&rc, sizeof rc);
    if (rc != OQS_SUCCESS) {
        fprintf(stderr, "ERROR: OQS_SIG_verify failed\n");
        goto err;
    }
    
    printf("verify has done\n");
    
    /* modify the signature to invalidate it */
    OQS_randombytes(signature, signature_len);
    OQS_TEST_CT_DECLASSIFY(signature, signature_len);
    rc = mask_crypto_sign_verify((const uint8_t *)sig, *sig_len,message, message_len,public_key);
    OQS_TEST_CT_DECLASSIFY(&rc, sizeof rc);
    if (rc != OQS_ERROR) {
        fprintf(stderr, "ERROR: OQS_SIG_verify should have failed!\n");
        goto err;
    }
    printf("everything has done\n");
#ifndef OQS_ENABLE_TEST_CONSTANT_TIME
    /* check magic values */
    int rv = memcmp(public_key + sig->length_public_key, magic.val, sizeof(magic_t));
    rv |= memcmp(secret_key + sig->length_secret_key, magic.val, sizeof(magic_t));
    rv |= memcmp(message + message_len, magic.val, sizeof(magic_t));
    rv |= memcmp(signature + sig->length_signature, magic.val, sizeof(magic_t));
    rv |= memcmp(public_key - sizeof(magic_t), magic.val, sizeof(magic_t));
    rv |= memcmp(secret_key - sizeof(magic_t), magic.val, sizeof(magic_t));
    rv |= memcmp(message - sizeof(magic_t), magic.val, sizeof(magic_t));
    rv |= memcmp(signature - sizeof(magic_t), magic.val, sizeof(magic_t));
    if (rv) {
        fprintf(stderr, "ERROR: Magic numbers do not mtach\n");
        goto err;
    }
#endif
    
    printf("verification passes as expected\n");
    ret = OQS_SUCCESS;
    goto cleanup;
    
err:
    ret = OQS_ERROR;
    
cleanup:
    if (secret_key) {
        //OQS_MEM_secure_free(secret_key - sizeof(magic_t), sig->length_secret_key + 2 * sizeof(magic_t));
        OQS_MEM_secure_free(secret_key, sig->length_secret_key + 2 * sizeof(magic_t));
    }
    if (public_key) {
        //OQS_MEM_insecure_free(public_key - sizeof(magic_t));
        OQS_MEM_secure_free(public_key, sig->length_public_key + 2 * sizeof(magic_t));
    }
    if (message) {
         //OQS_MEM_insecure_free(message - sizeof(magic_t));
        OQS_MEM_secure_free(message, message_len + 2 * sizeof(magic_t));
    }
    if (signature) {
        //OQS_MEM_insecure_free(signature - sizeof(magic_t));
        OQS_MEM_secure_free(signature, signature_len + 2 * sizeof(magic_t));
    }
    OQS_SIG_free(sig);
    
    PRINT_CURRENT_TIME
    printf("\n");
    
    return ret;
}

#ifdef OQS_ENABLE_TEST_CONSTANT_TIME
static void TEST_SIG_randombytes(uint8_t *random_array, size_t bytes_to_read) {
	// We can't make direct calls to the system randombytes on some platforms,
	// so we have to swap out the OQS_randombytes provider.
	OQS_randombytes_switch_algorithm("system");
	OQS_randombytes(random_array, bytes_to_read);
	OQS_randombytes_custom_algorithm(&TEST_SIG_randombytes);

	// OQS_TEST_CT_CLASSIFY tells Valgrind's memcheck tool to issue a warning if
	// the program branches on any byte that depends on random_array. This helps us
	// identify timing side-channels, as these bytes often contain secret data.
	OQS_TEST_CT_CLASSIFY(random_array, bytes_to_read);
}
#endif

#if OQS_USE_PTHREADS_IN_TESTS
struct thread_data {
	char *alg_name;
	OQS_STATUS rc;
};

void *test_wrapper(void *arg) {
	struct thread_data *td = arg;
	td->rc = sig_test_correctness(td->alg_name);
	return NULL;
}
#endif

int main(int argc, char **argv) {
	OQS_init();

	printf("Testing signature algorithms using liboqs version %s\n", OQS_version());

	if (argc != 2) {
		fprintf(stderr, "Usage: test_sig algname\n");
		fprintf(stderr, "  algname: ");
		for (size_t i = 0; i < OQS_SIG_algs_length; i++) {
			if (i > 0) {
				fprintf(stderr, ", ");
			}
			fprintf(stderr, "%s", OQS_SIG_alg_identifier(i));
		}
		fprintf(stderr, "\n");
		OQS_destroy();
		return EXIT_FAILURE;
	}


	char *alg_name = argv[1];
	if (!OQS_SIG_alg_is_enabled(alg_name)) {
		printf("Signature algorithm %s not enabled!\n", alg_name);
		OQS_destroy();
		return EXIT_FAILURE;
	}

#ifdef OQS_ENABLE_TEST_CONSTANT_TIME
	OQS_randombytes_custom_algorithm(&TEST_SIG_randombytes);
#else
	OQS_randombytes_switch_algorithm("system");
#endif

	OQS_STATUS rc;
#if OQS_USE_PTHREADS_IN_TESTS
#define MAX_LEN_SIG_NAME_ 64
	pthread_t thread;
	struct thread_data td;
	td.alg_name = alg_name;
	int trc = pthread_create(&thread, NULL, test_wrapper, &td);
	if (trc) {
		fprintf(stderr, "ERROR: Creating pthread\n");
		OQS_destroy();
		return EXIT_FAILURE;
	}
	pthread_join(thread, NULL);
	rc = td.rc;
#else
	rc = sig_test_correctness(alg_name);
#endif
	if (rc != OQS_SUCCESS) {
		OQS_destroy();
		return EXIT_FAILURE;
	}
    
    PRINT_CURRENT_TIME
    
	OQS_destroy();
	return EXIT_SUCCESS;
}
