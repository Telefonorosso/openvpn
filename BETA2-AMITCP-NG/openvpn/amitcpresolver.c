/*
 * amitcpresolver - runtime DNS configuration for AmiTCP-NG / AmigaOS 68k.
 *
 * Uses the published Roadshow-compatible bsdsocket.library extension ABI.
 * Targets the AmiTCP_NG-main source snapshot provided for this POC.
 *
 * Copyright (C) 2026 AmiOpenVPN project contributors.
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * The operations here use native library vectors, NOT the AmiTCP ARexx
 * parser. In particular, ARexx "REMOVE NAMESERVER" is NOT a valid command.
 */

#include <exec/types.h>
#include <exec/libraries.h>
#include <exec/lists.h>
#include <exec/nodes.h>
#include <proto/exec.h>
#include <stdio.h>
#include <string.h>

#define RC_OK    0
#define RC_WARN  5
#define RC_ERROR 10
#define RC_FAIL  20

/* Roadshow-compatible ABI, as implemented by the supplied AmiTCP-NG tree.
 * The linked list returned by ObtainDomainNameServerList is a snapshot.
 * Every node is owned by the library and MUST be released using its LVO.
 */
struct dns_node {
    struct MinNode dnsn_MinNode;
    LONG dnsn_Size;
    STRPTR dnsn_Address;
    LONG dnsn_UseCount;
};

/* Amiga library base for the explicit LVO calls below. */
static struct Library *SocketBase = 0;

/* LVO -516: AddDomainNameServer(address in A0), returns LONG in D0. */
static LONG api_add(STRPTR address)
{
    register struct Library *a6 __asm("a6") = SocketBase;
    register STRPTR a0 __asm("a0") = address;
    register LONG d0 __asm("d0");
    __asm__ __volatile__("jsr a6@(-516)"
                         : "=r"(d0), "+r"(a0)
                         : "r"(a6)
                         : "d1", "a1", "cc", "memory");
    return d0;
}

/* LVO -522: RemoveDomainNameServer(address in A0); removes one matching entry. */
static LONG api_remove(STRPTR address)
{
    register struct Library *a6 __asm("a6") = SocketBase;
    register STRPTR a0 __asm("a0") = address;
    register LONG d0 __asm("d0");
    __asm__ __volatile__("jsr a6@(-522)"
                         : "=r"(d0), "+r"(a0)
                         : "r"(a6)
                         : "d1", "a1", "cc", "memory");
    return d0;
}

/* LVO -534: ObtainDomainNameServerList(), returns struct List* in D0. */
static struct List *api_obtain(void)
{
    register struct Library *a6 __asm("a6") = SocketBase;
    register struct List *d0 __asm("d0");
    __asm__ __volatile__("jsr a6@(-534)"
                         : "=r"(d0)
                         : "r"(a6)
                         : "d1", "a0", "a1", "cc", "memory");
    return d0;
}

/* LVO -528: ReleaseDomainNameServerList(list in A0). */
static void api_release(struct List *list)
{
    register struct Library *a6 __asm("a6") = SocketBase;
    register struct List *a0 __asm("a0") = list;
    __asm__ __volatile__("jsr a6@(-528)"
                         : "+r"(a0)
                         : "r"(a6)
                         : "d0", "d1", "a1", "cc", "memory");
}

/* LVO -162: Errno(), library's last error in D0. */
static LONG api_errno(void)
{
    register struct Library *a6 __asm("a6") = SocketBase;
    register LONG d0 __asm("d0");
    __asm__ __volatile__("jsr a6@(-162)"
                         : "=r"(d0)
                         : "r"(a6)
                         : "d1", "a0", "a1", "cc", "memory");
    return d0;
}

static void usage(void)
{
    puts("amitcpresolver - live DNS control for AmiTCP-NG");
    puts("Usage: amitcpresolver LIST");
    puts("       amitcpresolver ADD <IPv4-address>");
    puts("       amitcpresolver REMOVE <IPv4-address>");
    puts("Note: REMOVE refuses to remove the last configured DNS server.");
}

/* Strict four-octet decimal IPv4; no DNS lookups, shortcuts, or host names. */
static int valid_ipv4(const char *s)
{
    unsigned part[4];
    int i;
    const char *p = s;
    if (p == NULL || *p == '\0') return 0;
    for (i = 0; i < 4; ++i) {
        unsigned n = 0;
        int digits = 0;
        while (*p >= '0' && *p <= '9') {
            if (++digits > 3) return 0;
            n = n * 10U + (unsigned)(*p - '0');
            ++p;
        }
        if (digits == 0 || n > 255U) return 0;
        part[i] = n;
        if (i < 3) {
            if (*p != '.') return 0;
            ++p;
        } else if (*p != '\0') {
            return 0;
        }
    }
    /* Disallow unusable unspecified, broadcast and multicast destinations. */
    if (part[0] == 0 || part[0] >= 224) return 0;
    return 1;
}

static int do_list(void)
{
    struct List *list = api_obtain();
    struct dns_node *node;
    unsigned count = 0;
    if (list == NULL) {
        printf("amitcpresolver: DNS list unavailable (errno %ld)\n", (long)api_errno());
        return RC_ERROR;
    }
    puts("Active DNS servers (in resolver order):");
    for (node = (struct dns_node *)list->lh_Head;
         node->dnsn_MinNode.mln_Succ != NULL;
         node = (struct dns_node *)node->dnsn_MinNode.mln_Succ) {
        printf("  %s  [%s]\n",
               node->dnsn_Address ? (const char *)node->dnsn_Address : "(invalid)",
               node->dnsn_UseCount < 0 ? "static" : "dynamic");
        ++count;
    }
    api_release(list);
    if (count == 0) puts("  (none)");
    printf("Total: %lu\n", (unsigned long)count);
    return RC_OK;
}

/* Inspect a snapshot, then release it BEFORE modifying the live NDB. */
static int dns_lookup(const char *address, unsigned *entries, unsigned *matches)
{
    struct List *list = api_obtain();
    struct dns_node *node;
    if (list == NULL) return 0;
    *entries = 0;
    *matches = 0;
    for (node = (struct dns_node *)list->lh_Head;
         node->dnsn_MinNode.mln_Succ != NULL;
         node = (struct dns_node *)node->dnsn_MinNode.mln_Succ) {
        ++*entries;
        if (node->dnsn_Address != NULL && strcmp(address, (const char *)node->dnsn_Address) == 0)
            ++*matches;
    }
    api_release(list);
    return 1;
}

static int do_change(int add, const char *address)
{
    unsigned count, matches;
    LONG result;
    if (!valid_ipv4(address)) {
        fprintf(stderr, "amitcpresolver: invalid IPv4 address: %s\n", address);
        return RC_ERROR;
    }
    if (!dns_lookup(address, &count, &matches)) {
        fprintf(stderr, "amitcpresolver: cannot read active DNS list (errno %ld)\n",
                (long)api_errno());
        return RC_ERROR;
    }

    if (add) {
        if (matches != 0) {
            printf("amitcpresolver: %s is already configured.\n", address);
            return RC_OK;
        }
        result = api_add((STRPTR)address);
        if (result != 0) {
            fprintf(stderr, "amitcpresolver: ADD failed for %s (errno %ld)\n",
                    address, (long)api_errno());
            return RC_ERROR;
        }
        printf("amitcpresolver: added %s\n", address);
        return RC_OK;
    }

    if (matches == 0) {
        printf("amitcpresolver: %s is already absent.\n", address);
        return RC_OK;
    }
    if (count <= 1) {
        fprintf(stderr, "amitcpresolver: refused to remove the last DNS server (%s).\n",
                address);
        return RC_WARN;
    }
    result = api_remove((STRPTR)address);
    if (result != 0) {
        fprintf(stderr, "amitcpresolver: REMOVE failed for %s (errno %ld)\n",
                address, (long)api_errno());
        return RC_ERROR;
    }
    printf("amitcpresolver: removed one entry for %s\n", address);
    if (matches > 1)
        printf("amitcpresolver: warning: %u duplicate entries existed; repeat REMOVE if needed.\n",
               (unsigned)matches);
    return RC_OK;
}

int main(int argc, char **argv)
{
    int rc;
    if (!((argc == 2 && strcmp(argv[1], "LIST") == 0) ||
          (argc == 3 && (strcmp(argv[1], "ADD") == 0 ||
                         strcmp(argv[1], "REMOVE") == 0)))) {
        usage();
        return RC_ERROR;
    }

    SocketBase = OpenLibrary((STRPTR)"bsdsocket.library", 4);
    if (SocketBase == NULL) {
        fputs("amitcpresolver: bsdsocket.library v4+ unavailable.\n", stderr);
        return RC_FAIL;
    }

    if (strcmp(argv[1], "LIST") == 0)
        rc = do_list();
    else
        rc = do_change(strcmp(argv[1], "ADD") == 0, argv[2]);

    CloseLibrary(SocketBase);
    SocketBase = NULL;
    return rc;
}
