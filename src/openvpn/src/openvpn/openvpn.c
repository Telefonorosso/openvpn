/*
 *  OpenVPN -- An application to securely tunnel IP networks
 *             over a single TCP/UDP port, with support for SSL/TLS-based
 *             session authentication and key exchange,
 *             packet encryption, packet authentication, and
 *             packet compression.
 *
 *  Copyright (C) 2002-2024 OpenVPN Inc <sales@openvpn.net>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2
 *  as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "syshead.h"

#include "init.h"
#include "forward.h"
#include "multi.h"
#include "win32.h"
#include "platform.h"

#include "memdbg.h"
#ifdef TARGET_AMIGA
/* POC2 FIX22: diagnostic only, remove before a release. */
#define AOVPN22_TRACE(message) do { \
    if (aovpn_diagnostics_enabled()) { \
        fputs("AOVPN22 " message "\n", stderr); \
        fflush(stderr); \
    } \
} while (0)
#endif


#define P2P_CHECK_SIG() EVENT_LOOP_CHECK_SIGNAL(c, process_signal_p2p, c);

static bool
process_signal_p2p(struct context *c)
{
    remap_signal(c);
    return process_signal(c);
}


/**************************************************************************/
/**
 * Main event loop for OpenVPN in client mode, where only one VPN tunnel
 * is active.
 * @ingroup eventloop
 *
 * @param c - The context structure of the single active VPN tunnel.
 */
static void
tunnel_point_to_point(struct context *c)
{
#ifdef TARGET_AMIGA
    AOVPN22_TRACE("P00 entered tunnel_point_to_point");
#endif
    context_clear_2(c);
#ifdef TARGET_AMIGA
    AOVPN22_TRACE("P01 after context_clear_2");
#endif

    /* set point-to-point mode */
    c->mode = CM_P2P;

    /* initialize tunnel instance */
#ifdef TARGET_AMIGA
    AOVPN22_TRACE("P02 before init_instance_handle_signals");
#endif
    init_instance_handle_signals(c, c->es, CC_HARD_USR1_TO_HUP);
#ifdef TARGET_AMIGA
    AOVPN22_TRACE("P03 after init_instance_handle_signals");
#endif
    if (IS_SIG(c))
    {
        return;
    }

    /* main event loop */
#ifdef TARGET_AMIGA
    AOVPN22_TRACE("P04 main event loop entered");
#endif
    while (true)
    {
        perf_push(PERF_EVENT_LOOP);

        /* process timers, TLS, etc. */
        pre_select(c);
        P2P_CHECK_SIG();

        /* set up and do the I/O wait */
        io_wait(c, p2p_iow_flags(c));
        P2P_CHECK_SIG();

        /* timeout? */
        if (c->c2.event_set_status == ES_TIMEOUT)
        {
            perf_pop();
            continue;
        }

        /* process the I/O which triggered select */
        process_io(c);
        P2P_CHECK_SIG();

        perf_pop();
    }

    persist_client_stats(c);

    uninit_management_callback();

    /* tear down tunnel instance (unless --persist-tun) */
    close_instance(c);
}

#undef PROCESS_SIGNAL_P2P

void
init_early(struct context *c)
{
    net_ctx_init(c, &c->net_ctx);

    /* init verbosity and mute levels */
    init_verb_mute(c, IVM_LEVEL_1);

    /* Initialise OpenSSL provider, this needs to be initialised this
     * early since option post-processing and also openssl info
     * printing depends on it */
    for (int j = 1; j < MAX_PARMS && c->options.providers.names[j]; j++)
    {
        c->options.providers.providers[j] =
            crypto_load_provider(c->options.providers.names[j]);
    }
}

static void
uninit_early(struct context *c)
{
    for (int j = 1; j < MAX_PARMS && c->options.providers.providers[j]; j++)
    {
        crypto_unload_provider(c->options.providers.names[j],
                               c->options.providers.providers[j]);
    }
    net_ctx_free(&c->net_ctx);
}


/**************************************************************************/
/**
 * OpenVPN's main init-run-cleanup loop.
 * @ingroup eventloop
 *
 * This function contains the two outer OpenVPN loops.  Its structure is
 * as follows:
 *  - Once-per-process initialization.
 *  - Outer loop, run at startup and then once per \c SIGHUP:
 *    - Level 1 initialization
 *    - Inner loop, run at startup and then once per \c SIGUSR1:
 *      - Call event loop function depending on client or server mode:
 *        - \c tunnel_point_to_point()
 *        - \c tunnel_server()
 *    - Level 1 cleanup
 *  - Once-per-process cleanup.
 *
 * @param argc - Commandline argument count.
 * @param argv - Commandline argument values.
 */
#ifdef TARGET_AMIGA
/* POC2 FIX26: diagnostic only. Never print arguments from real VPN profiles. */
static void
aovpn26_trace_args(const char *where, int argc, char **argv)
{
    int i;
    if (!aovpn_diagnostics_enabled()) return;
    fprintf(stderr, "AOVPN26 %s argc=%d\n", where, argc);
    if (argc < 0 || (argc > 0 && !argv)) {
        fputs("AOVPN26 invalid argc/argv\n", stderr);
        fflush(stderr);
        return;
    }
    for (i = 0; i < argc && i < 6; ++i) {
        const char *s = argv[i];
        unsigned int n = 0;
        unsigned int b0, b1;
        if (!s) {
            fprintf(stderr, "AOVPN26 %s argv[%d]=NULL\n", where, i);
            continue;
        }
        while (n < 80U && s[n] != '\0') {
            ++n;
        }
        b0 = (unsigned char)s[0];
        b1 = s[0] ? (unsigned char)s[1] : 0U;
        fprintf(stderr,
                "AOVPN26 %s argv[%d] first=%02X,%02X value=[%.*s]%s\n",
                where, i, b0, b1, (int)n, s, n == 80U ? " (truncated)" : "");
    }
    fflush(stderr);
}
#endif /* TARGET_AMIGA */

static
int
openvpn_main(int argc, char *argv[])
{
    struct context c;
    #ifdef TARGET_AMIGA
    AOVPN22_TRACE("O00 openvpn_main entered");
    #endif

#if PEDANTIC
    fprintf(stderr, "Sorry, I was built with --enable-pedantic and I am incapable of doing any real work!\n");
    return 1;
#endif

#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif

    #ifdef TARGET_AMIGA
    AOVPN22_TRACE("O01 before CLEAR context");
    #endif
    CLEAR(c);
    #ifdef TARGET_AMIGA
    AOVPN22_TRACE("O02 after CLEAR context");
    #endif

    /* signify first time for components which can
     * only be initialized once per program instantiation. */
    c.first_time = true;

    /* initialize program-wide statics */
    #ifdef TARGET_AMIGA
    AOVPN22_TRACE("O03 before init_static");
    #endif
    if (init_static())
    {
    #ifdef TARGET_AMIGA
    AOVPN22_TRACE("O04 init_static returned true");
    #endif
        /*
         * This loop is initially executed on startup and then
         * once per SIGHUP.
         */
        do
        {
            /* enter pre-initialization mode with regard to signal handling */
            #ifdef TARGET_AMIGA
            AOVPN22_TRACE("O05 before pre_init_signal_catch");
            #endif
            pre_init_signal_catch();
            #ifdef TARGET_AMIGA
            AOVPN22_TRACE("O06 after pre_init_signal_catch");
            #endif

            /* zero context struct but leave first_time member alone */
            context_clear_all_except_first_time(&c);
            #ifdef TARGET_AMIGA
            AOVPN22_TRACE("O07 context reset complete");
            #endif

            /* static signal info object */
            CLEAR(siginfo_static);
            c.sig = &siginfo_static;

            /* initialize garbage collector scoped to context object */
            gc_init(&c.gc);

            /* initialize environmental variable store */
            c.es = env_set_create(NULL);
#ifdef _WIN32
            set_win_sys_path_via_env(c.es);
#endif

#ifdef ENABLE_MANAGEMENT
            /* initialize management subsystem */
            init_management();
#endif

            /* initialize options to default state */
            #ifdef TARGET_AMIGA
            AOVPN22_TRACE("O08 before init_options");
            #endif
            init_options(&c.options, true);
            #ifdef TARGET_AMIGA
            AOVPN22_TRACE("O09 after init_options");
            #endif

            /* parse command line options, and read configuration file */
#ifdef TARGET_AMIGA
            /* Reset BEFORE touching argv: a SIGHUP must not leak raw
             * arguments under the previous configuration's trace setting. */
            aovpn_set_diagnostics(0);
            AOVPN22_TRACE("O10 before parse_argv");
            aovpn26_trace_args("PARSE", argc, argv);
#endif
            parse_argv(&c.options, argc, argv, M_USAGE, OPT_P_DEFAULT, NULL, c.es);
            #ifdef TARGET_AMIGA
            AOVPN22_TRACE("O11 after parse_argv");
            #endif

#ifdef ENABLE_PLUGIN
            /* plugins may contribute options configuration */
            init_verb_mute(&c, IVM_LEVEL_1);
            init_plugins(&c);
            open_plugins(&c, true, OPENVPN_PLUGIN_INIT_PRE_CONFIG_PARSE);
#endif

            /* Early initialisation that need to happen before option
             * post processing and other early startup but after parsing */
            #ifdef TARGET_AMIGA
            AOVPN22_TRACE("O12 before init_early");
            #endif
            init_early(&c);
            #ifdef TARGET_AMIGA
            AOVPN22_TRACE("O13 after init_early");
            #endif

            /* set dev options */
            init_options_dev(&c.options);

            /* openssl print info? */
            #ifdef TARGET_AMIGA
            AOVPN22_TRACE("O14 before print_openssl_info");
            #endif
            if (print_openssl_info(&c.options))
            {
                break;
            }
#ifdef TARGET_AMIGA
            AOVPN22_TRACE("O15 print_openssl_info returned false");
#endif

            /* --genkey mode? */
            if (do_genkey(&c.options))
            {
                break;
            }

            /* tun/tap persist command? */
            if (do_persist_tuntap(&c.options, &c.net_ctx))
            {
                break;
            }
#ifdef TARGET_AMIGA
            AOVPN22_TRACE("O16 genkey/persist checks passed");
#endif

            /* sanity check on options */
            options_postprocess(&c.options, c.es);
#ifdef TARGET_AMIGA
            AOVPN22_TRACE("O17 options_postprocess passed");
#endif

            /* AOVPN V4 diagnostic: independently isolate the startup
             * parameter dump, which runs only at verbosity >= 4.  This
             * changes logging only; connection options remain intact. */
#ifdef TARGET_AMIGA
            if (get_debug_level() >= 4)
            {
                fputs("AOVPN-V4-A: startup parameter dump bypassed\n", stderr);
                fflush(stderr);
            }
#else
            show_settings(&c.options);
#endif
#ifdef TARGET_AMIGA
            AOVPN22_TRACE("O18 show_settings passed");
#endif

            /* print version number */
            msg(M_INFO, "%s", title_string);
#ifdef _WIN32
            show_windows_version(M_INFO);
#endif
            show_library_versions(M_INFO);
#ifdef TARGET_AMIGA
            AOVPN22_TRACE("O19 show_library_versions passed");
#endif

            show_dco_version(M_INFO);
#ifdef TARGET_AMIGA
            AOVPN22_TRACE("O20 show_dco_version passed");
#endif

            /* misc stuff */
#ifdef TARGET_AMIGA
            AOVPN22_TRACE("O21 before pre_setup");
#endif
            pre_setup(&c.options);
#ifdef TARGET_AMIGA
            AOVPN22_TRACE("O22 after pre_setup");
#endif

            /* test crypto? */
            if (do_test_crypto(&c.options))
            {
                break;
            }
#ifdef TARGET_AMIGA
            AOVPN22_TRACE("O23 crypto test check passed");
#endif

            /* Query passwords before becoming a daemon if we don't use the
             * management interface to get them. */
#ifdef ENABLE_MANAGEMENT
            if (!(c.options.management_flags & MF_QUERY_PASSWORDS))
#endif
            init_query_passwords(&c);
#ifdef TARGET_AMIGA
            AOVPN22_TRACE("O24 initial credential query passed");
#endif

            /* become a daemon if --daemon */
            if (c.first_time)
            {
                c.did_we_daemonize = possibly_become_daemon(&c.options);
                write_pid_file(c.options.writepid, c.options.chroot_dir);
            }

#ifdef ENABLE_MANAGEMENT
            /* open management subsystem */
            if (!open_management(&c))
            {
                break;
            }
            /* query for passwords through management interface, if needed */
            if (c.options.management_flags & MF_QUERY_PASSWORDS)
            {
                init_query_passwords(&c);
            }
#endif

            /* set certain options as environmental variables */
            setenv_settings(c.es, &c.options);
#ifdef TARGET_AMIGA
            AOVPN22_TRACE("O25 setenv_settings passed");
#endif

            /* finish context init */
#ifdef TARGET_AMIGA
            AOVPN22_TRACE("O26 before context_init_1");
#endif
            context_init_1(&c);
#ifdef TARGET_AMIGA
            AOVPN22_TRACE("O27 after context_init_1");
            AOVPN22_TRACE("O28 entering tunnel dispatch");
#endif

            do
            {
                /* run tunnel depending on mode */
                switch (c.options.mode)
                {
                    case MODE_POINT_TO_POINT:
                        tunnel_point_to_point(&c);
                        break;

                    case MODE_SERVER:
                        tunnel_server(&c);
                        break;

                    default:
                        ASSERT(0);
                }

                /* indicates first iteration -- has program-wide scope */
                c.first_time = false;

                /* any signals received? */
                if (IS_SIG(&c))
                {
                    print_signal(c.sig, NULL, M_INFO);
                }

                /* pass restart status to management subsystem */
                signal_restart_status(c.sig);
            }
            while (c.sig->signal_received == SIGUSR1);

            env_set_destroy(c.es);
            uninit_options(&c.options);
            gc_reset(&c.gc);
            uninit_early(&c);
        }
        while (c.sig->signal_received == SIGHUP);
    }

    context_gc_free(&c);

#ifdef ENABLE_MANAGEMENT
    /* close management interface */
    close_management();
#endif

    /* uninitialize program-wide statics */
    uninit_static();

    openvpn_exit(OPENVPN_EXIT_STATUS_GOOD); /* exit point */
    return 0;                               /* NOTREACHED */
}

#ifdef _WIN32
int
wmain(int argc, wchar_t *wargv[])
{
    char **argv;
    int ret;
    int i;

    if ((argv = calloc(argc+1, sizeof(char *))) == NULL)
    {
        return 1;
    }

    for (i = 0; i < argc; i++)
    {
        int n = WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1, NULL, 0, NULL, NULL);
        argv[i] = malloc(n);
        WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1, argv[i], n, NULL, NULL);
    }

    ret = openvpn_main(argc, argv);

    for (i = 0; i < argc; i++)
    {
        free(argv[i]);
    }
    free(argv);

    return ret;
}
#else  /* ifdef _WIN32 */
int
main(int argc, char *argv[])
{
#ifdef TARGET_AMIGA /* AmiOpenVPN-POC2 */
    if (aovpn_diagnostics_enabled()) {
        fputs("AOVPN21 M1 main entered\n", stderr);
        fflush(stderr);
    }
    /* AOVPN27: original argv has already proved invalid; do not dereference. */
    if (aovpn_diagnostics_enabled()) {
        fprintf(stderr, "AOVPN27 ORIGINAL argc=%d argv=%p (not dereferenced)\n",
                argc, (void *)argv);
        fflush(stderr);
    }
    if (!aovpn_amiga_rebuild_argv(&argc, &argv)) return 27;
    aovpn26_trace_args("REBUILT", argc, argv);
    if (!aovpn_socket_init()) return 20;
    if (aovpn_diagnostics_enabled()) {
        fputs("AOVPN21 M2 socket init passed\n", stderr);
        fflush(stderr);
    }
    /* AOVPN25: explicit TLS init after socket init */
    if (!aovpn_amissl_init()) return 25;
    if (aovpn_diagnostics_enabled()) {
        fputs("AOVPN21 M3 entering openvpn_main\n", stderr);
        fflush(stderr);
    }
#endif
    return openvpn_main(argc, argv);
}
#endif /* ifdef _WIN32 */
