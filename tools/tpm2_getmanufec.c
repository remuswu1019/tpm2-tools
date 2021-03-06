/* SPDX-License-Identifier: BSD-3-Clause */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <curl/curl.h>
#include <openssl/buffer.h>
#include <openssl/evp.h>
#include <openssl/sha.h>

#include "files.h"
#include "log.h"
#include "tpm2_alg_util.h"
#include "tpm2_auth_util.h"
#include "tpm2_capability.h"
#include "tpm2_tool.h"

typedef struct tpm_getmanufec_ctx tpm_getmanufec_ctx;
struct tpm_getmanufec_ctx {
    char *output_file;
    struct {
        struct {
            char *auth_str;
            tpm2_session *session;
        } endorse;
        struct {
            char *auth_str;
            tpm2_session *session;
        } owner;
    } auth;
    TPM2B_SENSITIVE_CREATE inSensitive;
    char *ec_cert_path;
    ESYS_TR persistent_handle;
    UINT32 algorithm_type;
    FILE *ec_cert_file;
    char *ek_server_addr;
    unsigned int non_persistent_read;
    unsigned int SSL_NO_VERIFY;
    char *ek_path;
    bool verbose;
    TPM2B_PUBLIC *outPublic;
    bool find_persistent_handle;
    char *ek_auth_str;
};

static tpm_getmanufec_ctx ctx = {
    .algorithm_type = TPM2_ALG_RSA,
    .find_persistent_handle = false
};

BYTE authPolicy[] = {0x83, 0x71, 0x97, 0x67, 0x44, 0x84, 0xB3, 0xF8,
                     0x1A, 0x90, 0xCC, 0x8D, 0x46, 0xA5, 0xD7, 0x24,
                     0xFD, 0x52, 0xD7, 0x6E, 0x06, 0x52, 0x0B, 0x64,
                     0xF2, 0xA1, 0xDA, 0x1B, 0x33, 0x14, 0x69, 0xAA};

bool set_key_algorithm(TPM2B_PUBLIC *inPublic) {
    inPublic->publicArea.nameAlg = TPM2_ALG_SHA256;
    // First clear attributes bit field.
    inPublic->publicArea.objectAttributes = 0;
    inPublic->publicArea.objectAttributes |= TPMA_OBJECT_RESTRICTED;
    inPublic->publicArea.objectAttributes &= ~TPMA_OBJECT_USERWITHAUTH;
    inPublic->publicArea.objectAttributes |= TPMA_OBJECT_ADMINWITHPOLICY;
    inPublic->publicArea.objectAttributes &= ~TPMA_OBJECT_SIGN_ENCRYPT;
    inPublic->publicArea.objectAttributes |= TPMA_OBJECT_DECRYPT;
    inPublic->publicArea.objectAttributes |= TPMA_OBJECT_FIXEDTPM;
    inPublic->publicArea.objectAttributes |= TPMA_OBJECT_FIXEDPARENT;
    inPublic->publicArea.objectAttributes |= TPMA_OBJECT_SENSITIVEDATAORIGIN;
    inPublic->publicArea.authPolicy.size = 32;
    memcpy(inPublic->publicArea.authPolicy.buffer, authPolicy, 32);

    inPublic->publicArea.type = ctx.algorithm_type;

    switch (ctx.algorithm_type) {
    case TPM2_ALG_RSA:
        inPublic->publicArea.parameters.rsaDetail.symmetric.algorithm = TPM2_ALG_AES;
        inPublic->publicArea.parameters.rsaDetail.symmetric.keyBits.aes = 128;
        inPublic->publicArea.parameters.rsaDetail.symmetric.mode.aes = TPM2_ALG_CFB;
        inPublic->publicArea.parameters.rsaDetail.scheme.scheme = TPM2_ALG_NULL;
        inPublic->publicArea.parameters.rsaDetail.keyBits = 2048;
        inPublic->publicArea.parameters.rsaDetail.exponent = 0x0;
        inPublic->publicArea.unique.rsa.size = 256;
        break;
    case TPM2_ALG_KEYEDHASH:
        inPublic->publicArea.parameters.keyedHashDetail.scheme.scheme = TPM2_ALG_XOR;
        inPublic->publicArea.parameters.keyedHashDetail.scheme.details.exclusiveOr.hashAlg = TPM2_ALG_SHA256;
        inPublic->publicArea.parameters.keyedHashDetail.scheme.details.exclusiveOr.kdf = TPM2_ALG_KDF1_SP800_108;
        inPublic->publicArea.unique.keyedHash.size = 0;
        break;
    case TPM2_ALG_ECC:
        inPublic->publicArea.parameters.eccDetail.symmetric.algorithm = TPM2_ALG_AES;
        inPublic->publicArea.parameters.eccDetail.symmetric.keyBits.aes = 128;
        inPublic->publicArea.parameters.eccDetail.symmetric.mode.sym = TPM2_ALG_CFB;
        inPublic->publicArea.parameters.eccDetail.scheme.scheme = TPM2_ALG_NULL;
        inPublic->publicArea.parameters.eccDetail.curveID = TPM2_ECC_NIST_P256;
        inPublic->publicArea.parameters.eccDetail.kdf.scheme = TPM2_ALG_NULL;
        inPublic->publicArea.unique.ecc.x.size = 32;
        inPublic->publicArea.unique.ecc.y.size = 32;
        break;
    case TPM2_ALG_SYMCIPHER:
        inPublic->publicArea.parameters.symDetail.sym.algorithm = TPM2_ALG_AES;
        inPublic->publicArea.parameters.symDetail.sym.keyBits.aes = 128;
        inPublic->publicArea.parameters.symDetail.sym.mode.sym = TPM2_ALG_CFB;
        inPublic->publicArea.unique.sym.size = 0;
        break;
    default:
        LOG_ERR("The algorithm type input(%4.4x) is not supported!", ctx.algorithm_type);
        return false;
    }

    return true;
}

tool_rc createEKHandle(ESYS_CONTEXT *ectx)
{
    TPM2_RC rval;

    TPM2B_PUBLIC inPublic = TPM2B_EMPTY_INIT;

    TPM2B_DATA outsideInfo = TPM2B_EMPTY_INIT;
    TPML_PCR_SELECTION creationPCR;

    ESYS_TR handle2048ek;

    bool ret = set_key_algorithm(&inPublic);
    if (!ret) {
        return tool_rc_general_error;
    }

    creationPCR.count = 0;

    ESYS_TR shandle1 = ESYS_TR_NONE;

    tool_rc rc = tpm2_auth_util_get_shandle(ectx, ESYS_TR_RH_ENDORSEMENT,
                            ctx.auth.endorse.session, &shandle1);
    if (rc != tool_rc_success) {
        LOG_ERR("Failed to get shandle");
        return rc;
    }

    rval = Esys_CreatePrimary(ectx, ESYS_TR_RH_ENDORSEMENT,
            shandle1, ESYS_TR_NONE, ESYS_TR_NONE,
            &ctx.inSensitive, &inPublic, &outsideInfo,
            &creationPCR, &handle2048ek, &ctx.outPublic,
            NULL, NULL, NULL);
    if (rval != TPM2_RC_SUCCESS ) {
        LOG_PERR(Esys_CreatePrimary, rval);
        return tool_rc_from_tpm(rval);
    }

    if (!ctx.non_persistent_read) {

        if (!ctx.persistent_handle) {
            LOG_ERR("Persistent handle for EK was not provided");
            return tool_rc_option_error;
        }

        ESYS_TR new_handle;

        ESYS_TR shandle = ESYS_TR_NONE;
        rc = tpm2_auth_util_get_shandle(ectx, ESYS_TR_RH_OWNER,
                                ctx.auth.owner.session, &shandle);
        if (rc != tool_rc_success) {
            LOG_ERR("Couldn't get shandle for owner hierarchy");
            return rc;
        }

        rval = Esys_EvictControl(ectx, ESYS_TR_RH_OWNER, handle2048ek,
                shandle, ESYS_TR_NONE, ESYS_TR_NONE,
                ctx.persistent_handle, &new_handle);
        if (rval != TPM2_RC_SUCCESS ) {
            LOG_PERR(Esys_EvictControl, rval);
            return tool_rc_from_tpm(rval);
        }
        LOG_INFO("EvictControl EK persistent succ.");

        rval = Esys_TR_Close(ectx, &new_handle);
        if (rval != TPM2_RC_SUCCESS) {
            LOG_PERR(Esys_TR_Close, rval);
            return tool_rc_from_tpm(rval);
        }
    }

    rval = Esys_FlushContext(ectx, handle2048ek);
    if (rval != TPM2_RC_SUCCESS ) {
        LOG_PERR(Esys_FlushContext, rval);
        return tool_rc_from_tpm(rval);
    }

    LOG_INFO("Flush transient EK succ.");

    return files_save_public(ctx.outPublic, ctx.output_file)
            ? tool_rc_success : tool_rc_general_error;
}

static unsigned char *HashEKPublicKey(void) {

    unsigned char *hash = (unsigned char*)malloc(SHA256_DIGEST_LENGTH);
    if (!hash) {
        LOG_ERR ("OOM");
        return NULL;
    }


    SHA256_CTX sha256;
    int is_success = SHA256_Init(&sha256);
    if (!is_success) {
        LOG_ERR ("SHA256_Init failed");
        goto err;
    }

    is_success = SHA256_Update(&sha256, ctx.outPublic->publicArea.unique.rsa.buffer,
            ctx.outPublic->publicArea.unique.rsa.size);
    if (!is_success) {
        LOG_ERR ("SHA256_Update failed");
        goto err;
    }

    /* TODO what do these magic bytes line up to? */
    BYTE buf[3] = {
        0x1,
        0x00,
        0x01 //Exponent
    };

    is_success = SHA256_Update(&sha256, buf, sizeof(buf));
    if (!is_success) {
        LOG_ERR ("SHA256_Update failed");
        goto err;
    }

    is_success = SHA256_Final(hash, &sha256);
    if (!is_success) {
        LOG_ERR ("SHA256_Final failed");
        goto err;
    }

    if (ctx.verbose) {
        tpm2_tool_output("public-key-hash:\n");
        tpm2_tool_output("  sha256: ");
        unsigned i;
        for (i = 0; i < SHA256_DIGEST_LENGTH; i++) {
            tpm2_tool_output("%02X", hash[i]);
        }
        tpm2_tool_output("\n");
    }

    return hash;
err:
    free(hash);
    return NULL;
}

char *Base64Encode(const unsigned char* buffer)
{
    BIO *bio, *b64;
    BUF_MEM *bufferPtr;

    LOG_INFO("Calculating the Base64Encode of the hash of the Endorsement Public Key:");

    if (buffer == NULL) {
        LOG_ERR("HashEKPublicKey returned null");
        return NULL;
    }

    b64 = BIO_new(BIO_f_base64());
    bio = BIO_new(BIO_s_mem());
    bio = BIO_push(b64, bio);
    BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);
    BIO_write(bio, buffer, SHA256_DIGEST_LENGTH);
    UNUSED(BIO_flush(bio));
    BIO_get_mem_ptr(bio, &bufferPtr);

    /* these are not NULL terminated */
    char *b64text = bufferPtr->data;
    size_t len = bufferPtr->length;

    size_t i;
    for (i = 0; i < len; i++) {
        if (b64text[i] == '+') {
            b64text[i] = '-';
        }
        if (b64text[i] == '/') {
            b64text[i] = '_';
        }
    }

    char *final_string = NULL;

    CURL *curl = curl_easy_init();
    if (curl) {
        char *output = curl_easy_escape(curl, b64text, len);
        if (output) {
            final_string = strdup(output);
            curl_free(output);
        }
    }
    curl_easy_cleanup(curl);
    curl_global_cleanup();
    BIO_free_all(bio);

    /* format to a proper NULL terminated string */
    return final_string;
}

int RetrieveEndorsementCredentials(char *b64h)
{
    int ret = -1;

    size_t len = 1 + strlen(b64h) + strlen(ctx.ek_server_addr);
    char *weblink = (char *)malloc(len);
    if (!weblink) {
        LOG_ERR("oom");
        return ret;
    }

    snprintf(weblink, len, "%s%s", ctx.ek_server_addr, b64h);

    CURLcode rc = curl_global_init(CURL_GLOBAL_DEFAULT);
    if (rc != CURLE_OK) {
        LOG_ERR("curl_global_init failed: %s", curl_easy_strerror(rc));
        goto out_memory;
    }

    CURL *curl = curl_easy_init();
    if (!curl) {
        LOG_ERR("curl_easy_init failed");
        goto out_global_cleanup;
    }

    /*
     * should not be used - Used only on platforms with older CA certificates.
     */
    if (ctx.SSL_NO_VERIFY) {
        rc = curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
        if (rc != CURLE_OK) {
            LOG_ERR("curl_easy_setopt for CURLOPT_SSL_VERIFYPEER failed: %s", curl_easy_strerror(rc));
            goto out_easy_cleanup;
        }
    }

    rc = curl_easy_setopt(curl, CURLOPT_URL, weblink);
    if (rc != CURLE_OK) {
        LOG_ERR("curl_easy_setopt for CURLOPT_URL failed: %s", curl_easy_strerror(rc));
        goto out_easy_cleanup;
    }

    /*
     * If verbose is set, add in diagnostic information for debugging connections.
     * https://curl.haxx.se/libcurl/c/CURLOPT_VERBOSE.html
     */
    rc = curl_easy_setopt(curl, CURLOPT_VERBOSE, (long)ctx.verbose);
    if (rc != CURLE_OK) {
        LOG_ERR("curl_easy_setopt for CURLOPT_VERBOSE failed: %s", curl_easy_strerror(rc));
        goto out_easy_cleanup;
    }

    /*
     * If an output file is specified, write to the file, else curl will use stdout:
     * https://curl.haxx.se/libcurl/c/CURLOPT_WRITEDATA.html
     */
    if (ctx.ec_cert_file) {
        rc = curl_easy_setopt(curl, CURLOPT_WRITEDATA, ctx.ec_cert_file);
        if (rc != CURLE_OK) {
            LOG_ERR("curl_easy_setopt for CURLOPT_WRITEDATA failed: %s", curl_easy_strerror(rc));
            goto out_easy_cleanup;
        }
    }

    rc = curl_easy_perform(curl);
    if (rc != CURLE_OK) {
        LOG_ERR("curl_easy_perform() failed: %s", curl_easy_strerror(rc));
        goto out_easy_cleanup;
    }

    ret = 0;

out_easy_cleanup:
    curl_easy_cleanup(curl);
out_global_cleanup:
    curl_global_cleanup();
out_memory:
    free(weblink);

    return ret;
}


int TPMinitialProvisioning(void)
{
    int rc = 1;
    unsigned char *hash = HashEKPublicKey();
    char *b64 = Base64Encode(hash);
    if (!b64) {
        LOG_ERR("Base64Encode returned null");
        goto out;
    }

    LOG_INFO("%s", b64);

    rc = RetrieveEndorsementCredentials(b64);

    free(b64);
out:
    free(hash);
    return rc;
}

static bool on_option(char key, char *value) {

    switch (key) {
    case 'H':
        if (!strcmp(value, "-")) {
            ctx.find_persistent_handle = true;
        } else if (!tpm2_util_string_to_uint32(value, &ctx.persistent_handle)) {
            LOG_ERR("Please input the handle used to make EK persistent(hex) in correct format.");
            return false;
        }
        break;
    case 'P':
        ctx.auth.endorse.auth_str = value;
        break;
    case 'w':
        ctx.auth.owner.auth_str = value;
        break;
    case 'p': {
        ctx.ek_auth_str = value;
    }   break;
    case 'G':
        ctx.algorithm_type = tpm2_alg_util_from_optarg(value, tpm2_alg_util_flags_base);
        if (ctx.algorithm_type == TPM2_ALG_ERROR) {
             LOG_ERR("Please input the algorithm type in correct format.");
            return false;
        }
        break;
    case 'o':
        if (value == NULL ) {
            LOG_ERR("Please input the file used to save the pub ek.");
            return false;
        }
        ctx.output_file = value;
        break;
    case 'E':
        ctx.ec_cert_path = value;
        break;
    case 'N':
        ctx.non_persistent_read = 1;
        break;
    case 'O':
        ctx.ek_path = value;
        break;
    case 'U':
        ctx.SSL_NO_VERIFY = 1;
        LOG_WARN("TLS communication with the said TPM manufacturer server setup with SSL_NO_VERIFY!");
        break;
    }
    return true;
}

static bool on_args(int argc, char **argv) {

    if (argc > 1) {
        LOG_ERR("Only supports one remote server url, got: %d", argc);
        return false;
    }

    ctx.ek_server_addr = argv[0];

    return true;
}

bool tpm2_tool_onstart(tpm2_options **opts) {

    const struct option topts[] =
    {
        { "eh-auth",              required_argument, NULL, 'P' },
        { "owner-auth",           required_argument, NULL, 'w' },
        { "ek-auth",              required_argument, NULL, 'p' },
        { "persistent-handle",    required_argument, NULL, 'H' },
        { "key-algorithm",        required_argument, NULL, 'G' },
        { "output",               required_argument, NULL, 'o' },
        { "non-persistent",       no_argument,       NULL, 'N' },
        { "offline",              required_argument, NULL, 'O' },
        { "ec-cert",              required_argument, NULL, 'E' },
        { "untrusted",            no_argument,       NULL, 'U' },
    };

    *opts = tpm2_options_new("P:w:H:p:G:o:NO:E:i:U", ARRAY_LEN(topts), topts,
                             on_option, on_args, 0);

    return *opts != NULL;
}

tool_rc tpm2_tool_onrun(ESYS_CONTEXT *ectx, tpm2_option_flags flags) {

    if (!ctx.ek_server_addr) {
        LOG_ERR("Must specify a remote server url!");
        return tool_rc_option_error;
    }

    ctx.verbose = flags.verbose;

    tool_rc rc = tpm2_auth_util_from_optarg(ectx, ctx.auth.endorse.auth_str,
            &ctx.auth.endorse.session, false);
    if (rc != tool_rc_success) {
        LOG_ERR("Invalid endorsement authorization");
        return rc;
    }

    rc = tpm2_auth_util_from_optarg(ectx, ctx.auth.owner.auth_str,
            &ctx.auth.owner.session, false);
    if (rc != tool_rc_success) {
        LOG_ERR("Invalid owner authorization");
        return rc;
    }

    tpm2_session *tmp;
    rc = tpm2_auth_util_from_optarg(NULL, ctx.ek_auth_str,
            &tmp, true);
    if (rc != tool_rc_success) {
        LOG_ERR("Invalid EK auth");
        return rc;
    }

    const TPM2B_AUTH *auth = tpm2_session_get_auth_value(tmp);
    ctx.inSensitive.sensitive.userAuth = *auth;

    tpm2_session_close(&tmp);

    if (ctx.find_persistent_handle) {
        rc = tpm2_capability_find_vacant_persistent_handle(ectx,
                        &ctx.persistent_handle);
        if (rc != tool_rc_success) {
            LOG_ERR("handle/H passed with a value of '-' but unable to find a"
                    " vacant persistent handle!");
            return rc;
        }
        tpm2_tool_output("persistent-handle: 0x%x\n", ctx.persistent_handle);
    }

    if (ctx.ec_cert_path) {
        ctx.ec_cert_file = fopen(ctx.ec_cert_path, "wb");
        if (!ctx.ec_cert_file) {
            LOG_ERR("Could not open file for writing: \"%s\"", ctx.ec_cert_path);
            return tool_rc_general_error;
        }
    }

    if (!ctx.ek_path) {
        tool_rc rc = createEKHandle(ectx);
        if (rc != tool_rc_success) {
            return tool_rc_general_error;
        }
    } else {
        ctx.outPublic = malloc(sizeof(*ctx.outPublic));
        ctx.outPublic->size = 0;

        bool res = files_load_public(ctx.ek_path, ctx.outPublic);
        if (!res) {
            LOG_ERR("Could not load existing EK public from file");
            return tool_rc_general_error;
        }
    }

    int ret = TPMinitialProvisioning();
    if (ret) {
        return tool_rc_general_error;
    }

    return tool_rc_success;
}

tool_rc tpm2_tool_onstop(ESYS_CONTEXT *ectx) {
    UNUSED(ectx);

    tool_rc rc = tool_rc_success;

    tool_rc tmp_rc = tpm2_session_close(&ctx.auth.owner.session);
    if (tmp_rc != tool_rc_success) {
        rc = tmp_rc;
    }

    tmp_rc = tpm2_session_close(&ctx.auth.endorse.session);
    if (tmp_rc != tool_rc_success) {
        rc = tmp_rc;
    }

    if (ctx.ec_cert_file) {
        fclose(ctx.ec_cert_file);
    }

    return rc;
}
