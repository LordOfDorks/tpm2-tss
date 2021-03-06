/* SPDX-License-Identifier: BSD-2-Clause */
/*******************************************************************************
 * Copyright 2017-2018, Fraunhofer SIT sponsored by Infineon Technologies AG
 * All rights reserved.
 *******************************************************************************/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>

#include "tss2_esys.h"
#include "tss2_mu.h"

#include "esys_iutil.h"
#include "test-esapi.h"
#define LOGMODULE test
#include "util/log.h"
#include "util/aux_util.h"

#define FLUSH true
#define NOT_FLUSH false

/*
 * Function to compare policy digest with expected digest.
 * The digest is computed with Esys_PolicyGetDigest.
 */
bool
cmp_policy_digest(ESYS_CONTEXT * esys_context,
                  ESYS_TR * session,
                  TPM2B_DIGEST * expected_digest,
                  char *comment, bool flush_session)
{

    TSS2_RC r;
    TPM2B_DIGEST *policyDigest;

    r = Esys_PolicyGetDigest(esys_context,
                             *session,
                             ESYS_TR_NONE,
                             ESYS_TR_NONE, ESYS_TR_NONE, &policyDigest);
    goto_if_error(r, "Error: PolicyGetDigest", error);

    LOGBLOB_DEBUG(&policyDigest->buffer[0], policyDigest->size,
                  "POLICY DIGEST");

    if (policyDigest->size != 20
        || memcmp(&policyDigest->buffer[0], &expected_digest->buffer[0],
                  policyDigest->size)) {
        free(policyDigest);
        LOG_ERROR("Error: Policy%s digest did not match expected policy.",
                  comment);
        return false;
    }
    free(policyDigest);
    if (flush_session) {
        r = Esys_FlushContext(esys_context, *session);
        goto_if_error(r, "Error: PolicyGetDigest", error);
        *session = ESYS_TR_NONE;
    }

    return true;

 error:
    return false;
}

/** This test is intended to test the ESAPI policy commands, not tested
 *  in other test cases.
 *  When possoble the commands are tested with a
 * trial session and the policy digest is compared with the expected digest.
 *
 * Tested ESAPI commands:
 *  - Esys_FlushContext() (M)
 *  - Esys_PolicyTemplate() (F)
 *
 * @param[in,out] esys_context The ESYS_CONTEXT.
 * @retval EXIT_FAILURE
 * @retval EXIT_SKIP
 * @retval EXIT_SUCCESS
 */
int
test_esys_policy_template_opt(ESYS_CONTEXT * esys_context)
{
    TSS2_RC r;
    int failure_return = EXIT_FAILURE;

    /* Dummy parameters for trial sessoin  */
    ESYS_TR sessionTrial = ESYS_TR_NONE;
    TPMT_SYM_DEF symmetricTrial = {.algorithm = TPM2_ALG_AES,
        .keyBits = {.aes = 128},
        .mode = {.aes = TPM2_ALG_CFB}
    };
    TPM2B_NONCE nonceCallerTrial = {
        .size = 20,
        .buffer = {11, 12, 13, 14, 15, 16, 17, 18, 19, 11,
                   21, 22, 23, 24, 25, 26, 27, 28, 29, 30}
    };

    /*
     * Test PolicyTemplate
     */
    r = Esys_StartAuthSession(esys_context, ESYS_TR_NONE, ESYS_TR_NONE,
                              ESYS_TR_NONE, ESYS_TR_NONE, ESYS_TR_NONE,
                              &nonceCallerTrial,
                              TPM2_SE_TRIAL, &symmetricTrial, TPM2_ALG_SHA1,
                              &sessionTrial);
    goto_if_error(r, "Error: During initialization of policy trial session",
                  error);

    TPM2B_DIGEST templateHash = {
        .size = 20,
        .buffer = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
                   11, 12, 13, 14, 15, 16, 17, 18, 19, 20}
    };

    r = Esys_PolicyTemplate(esys_context,
                            sessionTrial,
                            ESYS_TR_NONE,
                            ESYS_TR_NONE, ESYS_TR_NONE, &templateHash);
    if ((r == TPM2_RC_COMMAND_CODE) ||
        (r == (TPM2_RC_COMMAND_CODE | TSS2_RESMGR_RC_LAYER)) ||
        (r == (TPM2_RC_COMMAND_CODE | TSS2_RESMGR_TPM_RC_LAYER))) {
        LOG_WARNING("Command TPM2_PolicyTemplate  not supported by TPM.");
        failure_return = EXIT_SKIP;
        goto error;
    } else {
        goto_if_error(r, "Error: PolicyTemplate", error);

        TPM2B_DIGEST expectedPolicyTemplate = {
            .size = 20,
            .buffer =
                {0xf6, 0x6d, 0x2a, 0x9c, 0x6e, 0xa8, 0xdf, 0x1a, 0x49, 0x3c,
                 0x42, 0xcc, 0xac, 0x6e, 0x3d, 0x08, 0xc0, 0x84, 0xcf, 0x73}
        };

        if (!cmp_policy_digest
            (esys_context, &sessionTrial, &expectedPolicyTemplate, "Template",
             FLUSH))
            goto error;
    }

    return EXIT_SUCCESS;

 error:

    if (sessionTrial != ESYS_TR_NONE) {
        if (Esys_FlushContext(esys_context, sessionTrial) != TSS2_RC_SUCCESS) {
            LOG_ERROR("Cleanup sessionTrial failed.");
        }
    }
    return failure_return;
}

int
test_invoke_esapi(ESYS_CONTEXT * esys_context) {
    return test_esys_policy_template_opt(esys_context);
}
