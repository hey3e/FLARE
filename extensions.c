/* 
  添加determin_strategy和determine_repeat_count函数，修改tls_construct_extensions函数
*/

/* 
 * 两个纯函数(示例), 基于 (ext_type, envseed) 算出
 *  - strategy: 0=none, 1=remove, 2=repeat
 *  - repeats : 2..150
 */
static int determine_strategy(int ext_type, int envseed) {
    // 一个简易哈希, 结果 ∈ [0..2]
    // (可以改进: 与 handshake 次数或上下文结合)
    unsigned combined = (unsigned)(ext_type * 31) ^ (unsigned)(envseed * 17);
    return combined % 3; // 0,1,2
}


static int determine_repeat_count(int ext_type, int envseed) {
    // 一个简易哈希, 结果 2..150
    unsigned combined = (unsigned)(ext_type * 13) ^ (unsigned)(envseed * 37);
    int raw = (combined % 149); // 0..148
    return raw + 2; // 2..150
}


/*
 * The rest is your original function, except we remove pick_random_in_range
 * and use the above two functions if doMutation=1
 */
int tls_construct_extensions(SSL *s, WPACKET *pkt, unsigned int context,
                             X509 *x, size_t chainidx)
{
    size_t i;
    int min_version, max_version = 0, reason;
    const EXTENSION_DEFINITION *thisexd;


    /********************************************************
     * A) Initialization logic
     ********************************************************/
    static int initialized = 0;
    static int doMutation = 0;          /* 0 = no mutation, 1 = mutate each time */
    static int last_target_type = -1;   /* Which extension type to mutate */
    static int remove_or_repeat = 0;    /* 0=none, 1=remove, 2=repeat */
    static int repeat_count = 1;


    if (!initialized) {
        initialized = 1;


        // 不再需要 srand((unsigned)time(NULL))，我们改为ENVSEED+ext_type的纯函数
        // 只要ENVSEED一致 => 结果一致
        const char *env = getenv("REPEAT_REMOVE");
        if (env != NULL && strcmp(env, "1") == 0) {
            doMutation = 1;
            fprintf(stderr, "[*] REPEAT_REMOVE=1 => Extension mutation enabled (ENVSEED based)\n");
        } else {
            fprintf(stderr, "[*] REPEAT_REMOVE not set or !=1 => No extension mutation\n");
        }
    }


    /********************************************************
     * B) Normal extension start code
     ********************************************************/
    if (!WPACKET_start_sub_packet_u16(pkt)
            || ((context & (SSL_EXT_CLIENT_HELLO | SSL_EXT_TLS1_2_SERVER_HELLO)) != 0
                && !WPACKET_set_flags(pkt, WPACKET_FLAGS_ABANDON_ON_ZERO_LENGTH))) {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, SSL_F_TLS_CONSTRUCT_EXTENSIONS,
                 ERR_R_INTERNAL_ERROR);
        return 0;
    }


    if ((context & SSL_EXT_CLIENT_HELLO) != 0) {
        reason = ssl_get_min_max_version(s, &min_version, &max_version, NULL);
        if (reason != 0) {
            SSLfatal(s, SSL_AD_INTERNAL_ERROR, SSL_F_TLS_CONSTRUCT_EXTENSIONS,
                     reason);
            return 0;
        }
    }


    // custom_ext_add...
    if ((context & SSL_EXT_CLIENT_HELLO) != 0) {
        custom_ext_init(&s->cert->custext);
    }
    if (!custom_ext_add(s, context, pkt, x, chainidx, max_version)) {
        return 0; /* SSLfatal() already called */
    }


    /********************************************************
     * C) If doMutation=1 => pick ext_type + strategy
     *    But now it's deterministic (ENVSEED based), not random
     ********************************************************/
    if (doMutation == 1) {
        // 读取ENVSEED(默认0)
        const char *envseed_str = getenv("ENVSEED");
        int envseed = 0;
        if (envseed_str != NULL) {
            envseed = atoi(envseed_str);
        }


        // 先随机选(或确定)一个扩展
        // 这段依旧是 "table index" 方式, 你可以改成根据 handshake 次数, 这里保持不变
        int chosen_index = (envseed * 31) % (int)OSSL_NELEM(ext_defs);
        last_target_type = ext_defs[chosen_index].type;


        // 取 strategy
        remove_or_repeat = determine_strategy(last_target_type, envseed);
        if (remove_or_repeat == 2) {
            repeat_count = determine_repeat_count(last_target_type, envseed);
        } else {
            repeat_count = 1;
        }


        fprintf(stderr,
            "[*]ENVSEED=%d => ext_type=%d, strategy=%d, repeat=%d\n",
            envseed, last_target_type, remove_or_repeat, repeat_count);
    }


    /********************************************************
     * D) The main extension-dispatch loop
     ********************************************************/
    for (i = 0, thisexd = ext_defs; i < OSSL_NELEM(ext_defs); i++, thisexd++) {
        EXT_RETURN (*construct)(SSL *s, WPACKET *pkt, unsigned int context,
                                X509 *x, size_t chainidx);
        EXT_RETURN ret;


        if (!should_add_extension(s, thisexd->context, context, max_version)) {
            continue;
        }


        construct = s->server ? thisexd->construct_stoc
                              : thisexd->construct_ctos;
        if (construct == NULL)
            continue;


        // If matched the extension to mutate
        if (doMutation == 1 && thisexd->type == last_target_type) {
            if (remove_or_repeat == 1) {
                // remove => skip
                fprintf(stderr, "[*] Removing extension type %d\n", last_target_type);
                continue;
            }
            if (remove_or_repeat == 2) {
                fprintf(stderr, "[*] Repeating extension type %d, %d times\n",
                        last_target_type, repeat_count);
                for (int j=0; j<repeat_count; j++) {
                    ret = construct(s, pkt, context, x, chainidx);
                    if (ret == EXT_RETURN_FAIL) {
                        return 0; // SSLfatal() already called
                    }
                    if (ret == EXT_RETURN_SENT
                        && (context & (SSL_EXT_CLIENT_HELLO
                                       | SSL_EXT_TLS1_3_CERTIFICATE_REQUEST
                                       | SSL_EXT_TLS1_3_NEW_SESSION_TICKET)) != 0)
                        s->ext.extflags[i] |= SSL_EXT_FLAG_SENT;
                }
                continue;
            }
        }


        // default call once
        ret = construct(s, pkt, context, x, chainidx);
        if (ret == EXT_RETURN_FAIL)
            return 0; // SSLfatal() already called
        if (ret == EXT_RETURN_SENT
            && (context & (SSL_EXT_CLIENT_HELLO
                           | SSL_EXT_TLS1_3_CERTIFICATE_REQUEST
                           | SSL_EXT_TLS1_3_NEW_SESSION_TICKET)) != 0)
            s->ext.extflags[i] |= SSL_EXT_FLAG_SENT;
    }


    if (!WPACKET_close(pkt)) {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, SSL_F_TLS_CONSTRUCT_EXTENSIONS,
                 ERR_R_INTERNAL_ERROR);
        return 0;
    }


    return 1;
}
