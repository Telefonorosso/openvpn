/*
 *  OpenVPN -- An application to securely tunnel IP networks
 *             over a single TCP/UDP port, with support for SSL/TLS-based
 *             session authentication and key exchange,
 *             packet encryption, packet authentication, and
 *             packet compression.
 *
 *  Copyright (C) 2002-2024 OpenVPN Technologies, Inc. <sales@openvpn.net>
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

#include "buffer.h"
#include "error.h"
#include "platform.h"
#include "win32.h"

#include "memdbg.h"

#include "run_command.h"

#ifdef TARGET_AMIGA
#include <dos/var.h>
#include <proto/dos.h>
#include <stdlib.h>
#include <string.h>
#endif

/* contains an SSEC_x value defined in platform.h */
static int script_security_level = SSEC_BUILT_IN; /* GLOBAL */

int
script_security(void)
{
    return script_security_level;
}

void
script_security_set(int level)
{
    script_security_level = level;
}

/*
 * Generate an error message based on the status code returned by openvpn_execve().
 */
static const char *
system_error_message(int stat, struct gc_arena *gc)
{
    struct buffer out = alloc_buf_gc(256, gc);

    switch (stat)
    {
        case OPENVPN_EXECVE_NOT_ALLOWED:
            buf_printf(&out, "disallowed by script-security setting");
            break;

#if defined(TARGET_AMIGA)
        case OPENVPN_EXECVE_ERROR:
            buf_printf(&out, "AmigaDOS command launch failed");
            break;
        default:
            buf_printf(&out, "AmigaDOS script returned code %d", stat);
            break;
#elif defined(_WIN32)
        case OPENVPN_EXECVE_ERROR:
            buf_printf(&out, "external program did not execute -- ");
        /* fall through */

        default:
            buf_printf(&out, "returned error code %d", stat);
            break;
#else  /* ifdef _WIN32 */

        case OPENVPN_EXECVE_ERROR:
            buf_printf(&out, "external program fork failed");
            break;

        default:
            if (!WIFEXITED(stat))
            {
                buf_printf(&out, "external program did not exit normally");
            }
            else
            {
                const int cmd_ret = WEXITSTATUS(stat);
                if (!cmd_ret)
                {
                    buf_printf(&out, "external program exited normally");
                }
                else if (cmd_ret == OPENVPN_EXECVE_FAILURE)
                {
                    buf_printf(&out, "could not execute external program");
                }
                else
                {
                    buf_printf(&out, "external program exited with error status: %d", cmd_ret);
                }
            }
            break;
#endif /* ifdef _WIN32 */
    }
    return (const char *)out.data;
}

bool
openvpn_execve_allowed(const unsigned int flags)
{
    if (flags & S_SCRIPT)
    {
        return script_security() >= SSEC_SCRIPTS;
    }
    else
    {
        return script_security() >= SSEC_BUILT_IN;
    }
}


#ifdef TARGET_AMIGA
/*
 * POC5-A: minimal native AmigaDOS script executor.
 *
 * This stage intentionally supports only AmigaDOS Execute scripts, not
 * POSIX shell scripts or arbitrary executable commands. It does not
 * modify MiamiDX or invoke anything unless a user-configured OpenVPN
 * hook explicitly requests a script at script-security >= 2.
 *
 * Never feed provider-controlled text into the Shell command stream.
 * Only path/arguments using this conservative character subset are
 * accepted. OpenVPN environment values are passed via private process
 * LocalVars, NOT interpolated into the command string or ENV:.
 */
struct aovpn_local_backup
{
    char *name;
    UBYTE *original;
    ULONG original_len;
    ULONG original_flags;
    bool had_original;
};

static void
aovpn_restore_local_vars(struct aovpn_local_backup *b, size_t used)
{
    while (used > 0)
    {
        struct aovpn_local_backup *entry = &b[--used];
        if (entry->name)
        {
            if (entry->had_original)
            {
                if (!SetVar((STRPTR)entry->name,
                            (STRPTR)entry->original,
                            (LONG)entry->original_len,
                            GVF_LOCAL_ONLY | (entry->original_flags & GVF_BINARY_VAR)))
                {
                    msg(M_WARN, "AOVPN-SCRIPT: failed restoring local variable %s", entry->name);
                }
            }
            else if (!DeleteVar((STRPTR)entry->name, GVF_LOCAL_ONLY))
            {
                msg(M_WARN, "AOVPN-SCRIPT: failed deleting temporary local variable %s", entry->name);
            }
        }
        free(entry->original);
        free(entry->name);
    }
}

static bool
aovpn_ascii_alpha(unsigned char c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

static bool
aovpn_ascii_digit(unsigned char c)
{
    return c >= '0' && c <= '9';
}

static bool
aovpn_env_name_valid(const char *s, size_t n)
{
    size_t i;
    if (!n || n > 127 || !(aovpn_ascii_alpha((unsigned char)s[0]) || s[0] == '_'))
    {
        return false;
    }
    for (i = 1; i < n; ++i)
    {
        if (!(aovpn_ascii_alpha((unsigned char)s[i]) || aovpn_ascii_digit((unsigned char)s[i]) || s[i] == '_'))
        {
            return false;
        }
    }
    return true;
}

static bool
aovpn_script_arg_valid(const char *s)
{
    const unsigned char *p = (const unsigned char *)s;
    if (!s)
    {
        return false;
    }
    for (; *p; ++p)
    {
        const unsigned char c = *p;
        if (!((aovpn_ascii_alpha(c) || aovpn_ascii_digit(c)) || c == '_' || c == '-' || c == '.' || c == ':'
              || c == '/' || c == '+' || c == '@' || c == '=' || c == ','
              || c == ' '))
        {
            return false;
        }
    }
    return true;
}

/* C:Execute + arguments wrapped in double quotes; quotes, escapes, control
 * characters and command separators are rejected by the validator above.
 * Do not convert this to a naive argv_str() or strcat() implementation. */
static char *
aovpn_script_command(const struct argv *a)
{
    static const char prefix[] = "C:Execute ";
    size_t total = sizeof(prefix);
    size_t i;
    char *cmd, *p;

    if (!a || !a->argc || !a->argv || !a->argv[0]
        || !a->argv[0][0])
    {
        return NULL;
    }
    for (i = 0; i < a->argc; ++i)
    {
        size_t n;
        if (!aovpn_script_arg_valid(a->argv[i]))
        {
            return NULL;
        }
        n = strlen(a->argv[i]);
        if (n > 1024 || total > 4096 - n - 4)
        {
            return NULL;
        }
        total += n + 3;
    }
    cmd = malloc(total);
    if (!cmd)
    {
        return NULL;
    }
    p = cmd;
    memcpy(p, prefix, sizeof(prefix) - 1);
    p += sizeof(prefix) - 1;
    for (i = 0; i < a->argc; ++i)
    {
        size_t n = strlen(a->argv[i]);
        *p++ = '"';
        memcpy(p, a->argv[i], n);
        p += n;
        *p++ = '"';
        if (i + 1 < a->argc)
        {
            *p++ = ' ';
        }
    }
    *p = '\0';
    return cmd;
}

static int
aovpn_amiga_script_execve(const struct argv *a, const struct env_set *es)
{
    struct gc_arena gc = gc_new();
    const char **env = make_env_array(es, true, &gc);
    struct aovpn_local_backup *saved = NULL;
    size_t n = 0, used = 0, i;
    char *cmd = aovpn_script_command(a);
    int result = OPENVPN_EXECVE_ERROR;

    if (!cmd)
    {
        msg(M_WARN, "AOVPN-SCRIPT: rejected script path/argument (restricted character set)");
        goto done;
    }
    while (env && env[n])
    {
        ++n;
    }
    if (n > 1024)
    {
        msg(M_WARN, "AOVPN-SCRIPT: too many environment variables");
        goto done;
    }
    saved = calloc(n ? n : 1, sizeof(*saved));
    if (!saved)
    {
        goto done;
    }

    /* Save the caller's local variables and restore them even on failure.
     * Copy previous LocalVar data before SetVar invalidates the node. */
    for (i = 0; i < n; ++i)
    {
        const char *eq = strchr(env[i], '=');
        const size_t len = eq ? (size_t)(eq - env[i]) : 0;
        struct LocalVar *old;
        struct aovpn_local_backup *b = &saved[used];
        if (!eq || !aovpn_env_name_valid(env[i], len))
        {
            msg(M_WARN, "AOVPN-SCRIPT: invalid environment variable name");
            goto restore;
        }
        b->name = malloc(len + 1);
        if (!b->name)
        {
            goto restore;
        }
        memcpy(b->name, env[i], len);
        b->name[len] = '\0';
        ++used;
        old = FindVar((STRPTR)b->name, LV_VAR);
        if (old)
        {
            if (old->lv_Len > 65535)
            {
                goto restore;
            }
            b->had_original = true;
            b->original_len = old->lv_Len;
            b->original_flags = old->lv_Flags;
            b->original = malloc(b->original_len ? b->original_len : 1);
            if (!b->original)
            {
                /* Nothing has been changed for this entry yet. */
                b->had_original = false;
                --used;
                free(b->name);
                b->name = NULL;
                goto restore;
            }
            if (b->original_len)
            {
                memcpy(b->original, old->lv_Value, b->original_len);
            }
        }
        if (!SetVar((STRPTR)b->name, (STRPTR)(eq + 1), -1, GVF_LOCAL_ONLY))
        {
            msg(M_WARN, "AOVPN-SCRIPT: cannot export local variable %s", b->name);
            goto restore;
        }
    }
    /* SystemTagList creates a synchronous Shell; NP_CopyVars defaults TRUE
     * on AmigaOS V36+. The child inherits our exported local variables. */
    {
        LONG rc = SystemTagList((STRPTR)cmd, NULL);
        if (rc >= 0 && rc <= 255)
        {
            result = (int)rc; /* Native Amiga return code, not waitpid status. */
        }
        else
        {
            msg(M_WARN, "AOVPN-SCRIPT: AmigaDOS SystemTagList failed (%ld)", (long)rc);
        }
    }
restore:
    aovpn_restore_local_vars(saved, used);
done:
    free(saved);
    free(cmd);
    gc_free(&gc);
    return result;
}
#endif /* TARGET_AMIGA */

#ifndef _WIN32
/*
 * Run execve() inside a fork().  Designed to replicate the semantics of system() but
 * in a safer way that doesn't require the invocation of a shell or the risks
 * associated with formatting and parsing a command line.
 * Returns the exit status of child, OPENVPN_EXECVE_NOT_ALLOWED if openvpn_execve_allowed()
 * returns false, or OPENVPN_EXECVE_ERROR on other errors.
 */
int
openvpn_execve(const struct argv *a, const struct env_set *es, const unsigned int flags)
{
    struct gc_arena gc = gc_new();
    int ret = OPENVPN_EXECVE_ERROR;
    static bool warn_shown = false;

    if (a && a->argv[0])
    {
#ifdef TARGET_AMIGA
        if (openvpn_execve_allowed(flags))
        {
            ret = aovpn_amiga_script_execve(a, es);
        }
        else
        {
            ret = OPENVPN_EXECVE_NOT_ALLOWED;
            if (!warn_shown && (script_security() < SSEC_SCRIPTS))
            {
                msg(M_WARN, SCRIPT_SECURITY_WARNING);
                warn_shown = true;
            }
        }
#elif defined(ENABLE_FEATURE_EXECVE)
        if (openvpn_execve_allowed(flags))
        {
            const char *cmd = a->argv[0];
            char *const *argv = a->argv;
            char *const *envp = (char *const *)make_env_array(es, true, &gc);
            pid_t pid;

            pid = fork();
            if (pid == (pid_t)0) /* child side */
            {
                execve(cmd, argv, envp);
                exit(OPENVPN_EXECVE_FAILURE);
            }
            else if (pid < (pid_t)0) /* fork failed */
            {
                msg(M_ERR, "openvpn_execve: unable to fork");
            }
            else /* parent side */
            {
                if (waitpid(pid, &ret, 0) != pid)
                {
                    ret = OPENVPN_EXECVE_ERROR;
                }
            }
        }
        else
        {
            ret = OPENVPN_EXECVE_NOT_ALLOWED;
            if (!warn_shown && (script_security() < SSEC_SCRIPTS))
            {
                msg(M_WARN, SCRIPT_SECURITY_WARNING);
                warn_shown = true;
            }
        }
#else  /* if defined(ENABLE_FEATURE_EXECVE) */
        msg(M_WARN, "openvpn_execve: execve function not available");
#endif /* TARGET_AMIGA / ENABLE_FEATURE_EXECVE */
    }
    else
    {
        msg(M_FATAL, "openvpn_execve: called with empty argv");
    }

    gc_free(&gc);
    return ret;
}
#endif /* ifndef _WIN32 */

/*
 * Wrapper around openvpn_execve
 */
int
openvpn_execve_check(const struct argv *a, const struct env_set *es, const unsigned int flags, const char *error_message)
{
    struct gc_arena gc = gc_new();
    const int stat = openvpn_execve(a, es, flags);
    int ret = false;

    if (flags & S_EXITCODE)
    {
        ret = platform_ret_code(stat);
        if (ret != -1)
        {
            goto done;
        }
    }
    else if (platform_system_ok(stat))
    {
        ret = true;
        goto done;
    }
    if (error_message)
    {
        msg(((flags & S_FATAL) ? M_FATAL : M_WARN), "%s: %s",
            error_message,
            system_error_message(stat, &gc));
    }
done:
    gc_free(&gc);

    return ret;
}

/*
 * Run execve() inside a fork(), duping stdout.  Designed to replicate the semantics of popen() but
 * in a safer way that doesn't require the invocation of a shell or the risks
 * associated with formatting and parsing a command line.
 */
int
openvpn_popen(const struct argv *a,  const struct env_set *es)
{
    struct gc_arena gc = gc_new();
    int ret = -1;

    if (a && a->argv[0])
    {
#if defined(ENABLE_FEATURE_EXECVE) && !defined(TARGET_AMIGA)
        static bool warn_shown = false;
        if (script_security() >= SSEC_BUILT_IN)
        {
            const char *cmd = a->argv[0];
            char *const *argv = a->argv;
            char *const *envp = (char *const *)make_env_array(es, true, &gc);
            pid_t pid;
            int pipe_stdout[2];

            if (pipe(pipe_stdout) == 0)
            {
                pid = fork();
                if (pid == (pid_t)0)       /* child side */
                {
                    close(pipe_stdout[0]);         /* Close read end */
                    dup2(pipe_stdout[1], 1);
                    execve(cmd, argv, envp);
                    exit(OPENVPN_EXECVE_FAILURE);
                }
                else if (pid > (pid_t)0)       /* parent side */
                {
                    int status = 0;

                    close(pipe_stdout[1]);        /* Close write end */
                    waitpid(pid, &status, 0);
                    ret = pipe_stdout[0];
                }
                else       /* fork failed */
                {
                    close(pipe_stdout[0]);
                    close(pipe_stdout[1]);
                    msg(M_ERR, "openvpn_popen: unable to fork %s", cmd);
                }
            }
            else
            {
                msg(M_WARN, "openvpn_popen: unable to create stdout pipe for %s", cmd);
                ret = -1;
            }
        }
        else if (!warn_shown && (script_security() < SSEC_SCRIPTS))
        {
            msg(M_WARN, SCRIPT_SECURITY_WARNING);
            warn_shown = true;
        }
#else  /* if defined(ENABLE_FEATURE_EXECVE) */
        msg(M_WARN, "openvpn_popen: execve function not available");
#endif /* if defined(ENABLE_FEATURE_EXECVE) */
    }
    else
    {
        msg(M_FATAL, "openvpn_popen: called with empty argv");
    }

    gc_free(&gc);
    return ret;
}
