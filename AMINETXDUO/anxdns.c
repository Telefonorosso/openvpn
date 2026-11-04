/*
 * anxdns - AmiNetXDuo runtime DNS helper for AmiOpenVPN
 * POC2 FIX1 / 2026-11-04 / AmigaDOS raw-command-line fix, not hardware validated
 *
 * Native AmigaOS m68k, no ixemul. Calls Roadshow-compatible bsdsocket
 * vector slots published and implemented by AmiNetXDuo.
 *
 * Syntax: anxdns ARGS | LIST | CHECK | UP | DOWN | ADD <IP> | REMOVE <IP>
 *
 * MIT License. No private VPN configuration or keys are used.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifndef ANXDNS_HOST_TEST
# include <exec/types.h>
# include <exec/libraries.h>
# include <exec/lists.h>
# include <proto/exec.h>
# include <proto/dos.h>
# define STATE_PATH "RAM:AOVPN-XD-DNS.state"
# define MANUAL_PATH "OPENVPN:AOVPN-DNS.manual"
# define OPT_FORMAT "RAM:AOVPN-XD-DNS-opt%d.tmp"
#else
# define STATE_PATH "ram/AOVPN-XD-DNS.state"
# define MANUAL_PATH "openvpn/AOVPN-DNS.manual"
# define OPT_FORMAT "ram/AOVPN-XD-DNS-opt%d.tmp"
# define SIM_PATH "ram/mock-dns.txt"
#endif

#define MAX_DNS 24
#define MAX_IP 64
#define MAX_LINE 256
#define OPT_COUNT 12

struct Dns {
    char ip[MAX_IP];
    int use;
};
struct DnsSet {
    struct Dns v[MAX_DNS];
    int n;
};
struct Session {
    struct DnsSet original;
    struct DnsSet vpn;
};

#ifndef ANXDNS_HOST_TEST
static struct Library *SocketBase = NULL;

/* Layout from Roadshow DomainNameServerNode: MinNode, Size,
 * Address, UseCount. Kept private to avoid header version dependencies. */
struct DnsNodeLayout {
    struct MinNode node;
    LONG size;
    STRPTR address;
    LONG use_count;
};

static LONG call_dns_add(char *ip)
{
    register struct Library *a6 __asm("a6") = SocketBase;
    register char *a0 __asm("a0") = ip;
    register LONG d0 __asm("d0");
    __asm__ volatile("jsr %%a6@(-0x204:W)" : "=r"(d0), "+r"(a0)
                     : "r"(a6) : "d1", "a1", "cc", "memory");
    return d0;
}
static LONG call_dns_remove(char *ip)
{
    register struct Library *a6 __asm("a6") = SocketBase;
    register char *a0 __asm("a0") = ip;
    register LONG d0 __asm("d0");
    __asm__ volatile("jsr %%a6@(-0x20a:W)" : "=r"(d0), "+r"(a0)
                     : "r"(a6) : "d1", "a1", "cc", "memory");
    return d0;
}
static struct List *call_dns_obtain(void)
{
    register struct Library *a6 __asm("a6") = SocketBase;
    register struct List *d0 __asm("d0");
    __asm__ volatile("jsr %%a6@(-0x216:W)" : "=r"(d0)
                     : "r"(a6) : "d1", "a0", "a1", "cc", "memory");
    return d0;
}
static void call_dns_release(struct List *l)
{
    register struct Library *a6 __asm("a6") = SocketBase;
    register struct List *a0 __asm("a0") = l;
    __asm__ volatile("jsr %%a6@(-0x210:W)" : "+r"(a0)
                     : "r"(a6) : "d0", "d1", "a1", "cc", "memory");
}
#endif

static int valid_ipv4(const char *ip)
{
    const char *p = ip;
    int parts = 0;
    if (!p || !*p) return 0;
    while (*p && parts < 4) {
        int val = 0, ndigits = 0;
        if (!isdigit((unsigned char)*p)) return 0;
        while (isdigit((unsigned char)*p)) {
            val = val * 10 + (*p++ - '0');
            if (++ndigits > 3 || val > 255) return 0;
        }
        if (++parts < 4) {
            if (*p++ != '.') return 0;
        }
    }
    if (*p || parts != 4) return 0;
    {
        unsigned long first = strtoul(ip, NULL, 10);
        if (first == 0 || first == 127 || first >= 224) return 0;
    }
    return 1;
}
static int valid_list_addr(const char *ip)
{
    /* IPv6 entries come from AmiNetXDuo's own formatted list. */
    return valid_ipv4(ip) || (strchr(ip,':') && strlen(ip) < MAX_IP);
}
static int find_ip(const struct DnsSet *s, const char *ip)
{
    int i;
    for (i=0;i<s->n;i++) if (!strcmp(s->v[i].ip,ip)) return i;
    return -1;
}
static int add_item(struct DnsSet *s,const char *ip,int use)
{
    if (!ip || strlen(ip)>=MAX_IP) return 0;
    if (find_ip(s,ip)>=0) return 1;
    if (s->n>=MAX_DNS) return 0;
    strcpy(s->v[s->n].ip,ip);
    s->v[s->n].use=use;
    s->n++;
    return 1;
}

#ifdef ANXDNS_HOST_TEST
static int sim_fail(const char *envkey, const char *ip)
{
    const char *key = getenv(envkey);
    return key && !strcmp(key, ip);
}
static int sim_load(struct DnsSet *s)
{
    FILE *f = fopen(SIM_PATH,"r");
    char ip[MAX_IP]; int use;
    s->n=0;
    if (!f) return 0;
    while (fscanf(f,"%63s %d",ip,&use)==2)
        if (!add_item(s,ip,use)) { fclose(f); return 0; }
    fclose(f);
    return 1;
}
static int sim_save(const struct DnsSet *s)
{
    FILE *f=fopen(SIM_PATH,"w"); int i;
    if (!f) return 0;
    for(i=0;i<s->n;i++) fprintf(f,"%s %d\n",s->v[i].ip,s->v[i].use);
    if (fclose(f)) return 0;
    return 1;
}
#endif

static int dns_snapshot(struct DnsSet *s)
{
#ifdef ANXDNS_HOST_TEST
    return sim_load(s);
#else
    struct List *list = call_dns_obtain();
    struct DnsNodeLayout *node;
    int success = 1;
    s->n=0;
    if (!list) return 0;
    node = (struct DnsNodeLayout *)list->lh_Head;
    while (node && node->node.mln_Succ) {
        if (!node->address || !valid_list_addr(node->address) ||
            !add_item(s,node->address,(int)node->use_count)) {
            success=0; break;
        }
        node=(struct DnsNodeLayout *)node->node.mln_Succ;
    }
    call_dns_release(list);
    return success;
#endif
}
static int dns_add_raw(const char *ip)
{
#ifdef ANXDNS_HOST_TEST
    struct DnsSet s; int n;
    if (sim_fail("ANXDNS_HOST_FAIL_ADD", ip) || !sim_load(&s)) return 0;
    n=find_ip(&s,ip);
    if(n>=0) { s.v[n].use++; return sim_save(&s); }
    return add_item(&s,ip,1) && sim_save(&s);
#else
    return call_dns_add((char *)ip)==0;
#endif
}
static int dns_remove_raw(const char *ip)
{
#ifdef ANXDNS_HOST_TEST
    struct DnsSet s; int n,i;
    if (sim_fail("ANXDNS_HOST_FAIL_REMOVE", ip) || !sim_load(&s)) return 0;
    n=find_ip(&s,ip);
    if(n<0) return 0;
    if(s.v[n].use>1) { s.v[n].use--; return sim_save(&s); }
    for(i=n;i+1<s.n;i++) s.v[i]=s.v[i+1];
    s.n--;
    return sim_save(&s);
#else
    return call_dns_remove((char *)ip)==0;
#endif
}
static int require_member(const char *ip,int want)
{
    struct DnsSet s;
    if (!dns_snapshot(&s)) return 0;
    return ((find_ip(&s,ip)>=0)?1:0)==want;
}
static int ensure_member(const char *ip,int want)
{
    int present;
    struct DnsSet s;
    if(!dns_snapshot(&s)) return 0;
    present=find_ip(&s,ip)>=0;
    if(present==want) return 1;
    if(want) {
        if(!dns_add_raw(ip)) return 0;
    } else {
        if(!dns_remove_raw(ip)) return 0;
    }
    return require_member(ip,want);
}
static void print_set(const char *name,const struct DnsSet *s)
{
    int i;
    printf("[DNS] %s (%d):\n",name,s->n);
    for(i=0;i<s->n;i++) printf("[DNS]   %s  use=%d\n",s->v[i].ip,s->v[i].use);
}
static int equals_set(const struct DnsSet *a,const struct DnsSet *b)
{
    int i;
    if(a->n!=b->n) return 0;
    for(i=0;i<a->n;i++) if(find_ip(b,a->v[i].ip)<0) return 0;
    return 1;
}

static int parse_option(const char *line,char *out)
{
    char a[48],b[48],ip[64],extra[16];
    int n;
    a[0]=b[0]=ip[0]=extra[0]=0;
    n=sscanf(line," %47s %47s %63s %15s",a,b,ip,extra);
    if(n==3 && (!strcmp(a,"dhcp-option") || !strcmp(a,"DHCP-OPTION") ||
                !strcmp(a,"DNS")) &&
       (!strcmp(b,"DNS") || !strcmp(b,"dns")) && valid_ipv4(ip)) {
        strcpy(out,ip); return 1;
    }
    /* For manual fallback only, DNS <numeric-address>. */
    if(n==2 && (!strcmp(a,"DNS") || !strcmp(a,"dns")) && valid_ipv4(b)) {
        strcpy(out,b); return 1;
    }
    return 0;
}
static int load_dns_options(struct DnsSet *out)
{
    int i, any_pushed=0;
    char name[160],line[MAX_LINE], ip[MAX_IP];
    FILE *f;
    out->n=0;
    for(i=1;i<=OPT_COUNT;i++) {
        sprintf(name,OPT_FORMAT,i);
        f=fopen(name,"r");
        if(!f) continue;
        while(fgets(line,sizeof(line),f)) {
            if(parse_option(line,ip)) {
                if(!add_item(out,ip,1)) { fclose(f); return 0; }
                any_pushed=1;
            }
        }
        fclose(f);
    }
    if(!any_pushed) {
        f=fopen(MANUAL_PATH,"r");
        if(f) {
            while(fgets(line,sizeof(line),f)) {
                if(line[0]=='#' || line[0]==';' || line[0]=='\n') continue;
                if(!parse_option(line,ip) || !add_item(out,ip,1)) {
                    printf("[DNS] Invalid manual DNS directive: %s",line);
                    fclose(f);return 0;
                }
            }
            fclose(f);
        }
        if(out->n) puts("[DNS] Using manual DNS fallback (no pushed DNS found).");
    }
    return out->n>0;
}
static int save_state(const struct Session *t)
{
    FILE *f=fopen(STATE_PATH,"w");
    int i,ok=1;
    if(!f) return 0;
    if(fprintf(f,"ANXDNS1\n")<0) ok=0;
    for(i=0;i<t->original.n;i++)
        if(fprintf(f,"O %s\n",t->original.v[i].ip)<0) ok=0;
    for(i=0;i<t->vpn.n;i++)
        if(fprintf(f,"V %s\n",t->vpn.v[i].ip)<0) ok=0;
    if(fflush(f)) ok=0;
    if(fclose(f)) ok=0;
    if(!ok) { remove(STATE_PATH); return 0; }
    return 1;
}
static int load_state(struct Session *t)
{
    FILE *f=fopen(STATE_PATH,"r");
    char line[MAX_LINE],ip[MAX_IP],type,extra[4];
    int n;
    t->original.n=t->vpn.n=0;
    if(!f) return 0;
    if(!fgets(line,sizeof(line),f) || strcmp(line,"ANXDNS1\n")) { fclose(f); return 0; }
    while(fgets(line,sizeof(line),f)) {
        extra[0]=0;
        n=sscanf(line," %c %63s %3s",&type,ip,extra);
        if(n!=2 || !valid_list_addr(ip) ||
           (type!='O' && type!='V') ||
           !add_item((type=='O')?&t->original:&t->vpn,ip,1)) {
            fclose(f); return 0;
        }
    }
    fclose(f);
    return t->vpn.n>0;
}
static int state_exists(void)
{
    FILE *f=fopen(STATE_PATH,"r");
    if(f) {fclose(f);return 1;}
    return 0;
}

static int restore_session(const struct Session *t)
{
    int i,ok=1;
    struct DnsSet actual;
    /* Restore original first: never leave a window with zero DNS servers. */
    for(i=0;i<t->original.n;i++) {
        if(!ensure_member(t->original.v[i].ip,1)) {
            printf("[DNS] RESTORE: cannot add %s\n",t->original.v[i].ip);
            ok=0;
        }
    }
    if(!ok) return 0;
    for(i=0;i<t->vpn.n;i++) {
        if(find_ip(&t->original,t->vpn.v[i].ip)>=0) continue;
        if(!ensure_member(t->vpn.v[i].ip,0)) {
            printf("[DNS] RESTORE: cannot remove %s\n",t->vpn.v[i].ip);
            ok=0;
        }
    }
    if(!dns_snapshot(&actual)) return 0;
    if(!equals_set(&t->original,&actual)) {
        puts("[DNS] RESTORE: unexpected DNS entries remain; preserving state.");
        print_set("current",&actual);
        ok=0;
    }
    return ok;
}
static int up_session(void)
{
    struct Session t;
    struct DnsSet current;
    int i;
    if(state_exists()) {
        printf("[DNS] Stale session state: %s. Run DOWN before UP.\n",STATE_PATH);
        return 10;
    }
    if(!load_dns_options(&t.vpn)) {
        puts("[DNS] No valid pushed DNS. Add dhcp-option DNS to OpenVPN server,");
        puts("[DNS] or put 'DNS <ip>' into OPENVPN:AOVPN-DNS.manual.");
        puts("[DNS] Nothing changed.");
        return 10;
    }
    if(!dns_snapshot(&t.original)) { puts("[DNS] Cannot read current resolver.");return 10; }
    if(t.original.n==0) {puts("[DNS] No original DNS entries. Refusing in this test candidate.");return 10;}
    /* Do not destroy resolver entries shared with other network components. */
    for(i=0;i<t.original.n;i++) {
        int u=t.original.v[i].use;
        if(u != 1 && u != -1) {
            printf("[DNS] Shared DNS %s usecount=%d; refusing change.\n",t.original.v[i].ip,u);
            return 10;
        }
    }
    print_set("saved original",&t.original);
    print_set("requested VPN",&t.vpn);
    if(!save_state(&t)) { puts("[DNS] Could not save state: no changes made."); return 10; }

    for(i=0;i<t.vpn.n;i++) {
        if(!ensure_member(t.vpn.v[i].ip,1)) {
            printf("[DNS] Failed adding VPN DNS %s\n",t.vpn.v[i].ip);
            goto rollback;
        }
    }
    for(i=0;i<t.original.n;i++) {
        if(find_ip(&t.vpn,t.original.v[i].ip)>=0) continue;
        if(!ensure_member(t.original.v[i].ip,0)) {
            printf("[DNS] Failed removing original DNS %s\n",t.original.v[i].ip);
            goto rollback;
        }
    }
    if(!dns_snapshot(&current) || !equals_set(&t.vpn,&current)) {
        puts("[DNS] Final DNS set differs from desired VPN set.");
        goto rollback;
    }
    puts("[DNS] Runtime VPN DNS active. Original snapshot preserved for DOWN.");
    print_set("active",&current);
    return 0;
rollback:
    puts("[DNS] UP failed; attempting DNS rollback.");
    if(restore_session(&t)) {
        remove(STATE_PATH);
        puts("[DNS] Original DNS restored; routing caller should revert too.");
    } else {
        puts("[DNS] DNS rollback incomplete: state PRESERVED for manual DOWN!");
    }
    return 10;
}
static int down_session(void)
{
    struct Session t;
    if(!state_exists()) { puts("[DNS] No saved session; DNS unchanged.");return 0; }
    if(!load_state(&t)) { puts("[DNS] Corrupt state. No changes made.");return 10; }
    if(!restore_session(&t)) {
        printf("[DNS] Restore incomplete: keep %s and retry DOWN.\n",STATE_PATH);
        return 10;
    }
    if(remove(STATE_PATH)) { puts("[DNS] Restored, but could not clear state file.");return 10; }
    puts("[DNS] Original DNS restored.");
    return 0;
}
/* CLI parser for AmigaDOS 2.0+.
 * C startup implementations vary: argc/argv is retained only as a fallback.
 * GetArgStr() returns the original NUL-terminated argument string without
 * the command name; unlike ReadArgs(), it does not consume the Shell input.
 * Commands are ASCII and require at most two tokens (command + IPv4).
 */
#define MAX_CMD 24
struct CliArgs {
    char cmd[MAX_CMD];
    char value[MAX_IP];
    int count;
};

static int eq_ci(const char *a, const char *b)
{
    while(*a && *b) {
        if(toupper((unsigned char)*a) != toupper((unsigned char)*b)) return 0;
        a++; b++;
    }
    return !*a && !*b;
}

static int copy_word(char *dst, size_t cap, const char *src)
{
    size_t n = strlen(src);
    if(n >= cap) return 0;
    memcpy(dst,src,n+1);
    return 1;
}

static int parse_raw(const char *raw, struct CliArgs *a)
{
    const char *p = raw;
    char *dst;
    size_t cap;
    a->count=0;
    a->cmd[0]=a->value[0]=0;
    if(!p) return 0;
    while(*p) {
        size_t n=0;
        int quoted=0;
        while(*p && isspace((unsigned char)*p)) p++;
        if(!*p) break;
        if(a->count==2) return 0; /* Unexpected third argument. */
        dst=(a->count==0)?a->cmd:a->value;
        cap=(a->count==0)?sizeof(a->cmd):sizeof(a->value);
        if(*p=='"') { quoted=1; p++; }
        while(*p) {
            if(quoted && *p=='"') { p++; quoted=0; break; }
            if(!quoted && isspace((unsigned char)*p)) break;
            if(n+1>=cap) return 0;
            dst[n++]=*p++;
        }
        if(quoted || !n) return 0;
        dst[n]=0;
        if(*p && !isspace((unsigned char)*p)) return 0;
        a->count++;
    }
    return a->count>0;
}

static int parse_cli(int argc,char **argv, struct CliArgs *a, const char **origin)
{
    const char *raw;
#ifdef ANXDNS_HOST_TEST
    raw=getenv("ANXDNS_HOST_RAWARGS"); /* Host-only simulation. */
#else
    raw=GetArgStr(); /* AmigaDOS V36+: does not depend on crt argv. */
#endif
    if(raw && *raw) {
        *origin="AmigaDOS/GetArgStr";
        return parse_raw(raw,a);
    }
    *origin="C-runtime/argv fallback";
    a->count=0;
    a->cmd[0]=a->value[0]=0;
    if(argc<2 || argc>3 || !argv || !argv[1]) return 0;
    if(!copy_word(a->cmd,sizeof(a->cmd),argv[1])) return 0;
    a->count=1;
    if(argc==3) {
        if(!argv[2] || !copy_word(a->value,sizeof(a->value),argv[2])) return 0;
        a->count=2;
    }
    return 1;
}

int main(int argc,char **argv)
{
    int code=10;
    struct DnsSet s;
    struct CliArgs a;
    const char *origin="unknown";
    int parsed=parse_cli(argc,argv,&a,&origin);

    /* Always available, even if bsdsocket.library is not running. */
    if(parsed && a.count==1 && eq_ci(a.cmd,"ARGS")) {
        printf("[DNS] Command parser: %s\n",origin);
        printf("[DNS] parsed count=%d command='%s'\n",a.count,a.cmd);
        printf("[DNS] C runtime argc=%d\n",argc);
        return 0;
    }
    if(parsed && a.count==1 && (eq_ci(a.cmd,"HELP") || eq_ci(a.cmd,"?"))) {
        puts("Usage: anxdns ARGS | LIST | CHECK | UP | DOWN | ADD <IPv4> | REMOVE <IPv4>");
        return 0;
    }
    if(!parsed || !((a.count==1 && (eq_ci(a.cmd,"LIST") || eq_ci(a.cmd,"CHECK") ||
                                     eq_ci(a.cmd,"UP") || eq_ci(a.cmd,"DOWN"))) ||
                     (a.count==2 && (eq_ci(a.cmd,"ADD") || eq_ci(a.cmd,"REMOVE"))))) {
        printf("[DNS] Invalid command line (source: %s, argc=%d).\n",origin,argc);
        puts("Usage: anxdns ARGS | LIST | CHECK | UP | DOWN | ADD <IPv4> | REMOVE <IPv4>");
        return 10;
    }
#ifndef ANXDNS_HOST_TEST
    SocketBase=OpenLibrary("bsdsocket.library",1);
    if(!SocketBase) {puts("[DNS] Cannot open bsdsocket.library.");return 10;}
#endif
    if(a.count==1 && (eq_ci(a.cmd,"LIST") || eq_ci(a.cmd,"CHECK"))) {
        if(dns_snapshot(&s)) {print_set("current",&s);code=0;}
        else puts("[DNS] Could not get resolver list.");
    } else if(a.count==1 && eq_ci(a.cmd,"UP")) code=up_session();
    else if(a.count==1 && eq_ci(a.cmd,"DOWN")) code=down_session();
    else if(a.count==2 && (eq_ci(a.cmd,"ADD") || eq_ci(a.cmd,"REMOVE"))) {
        if(!valid_ipv4(a.value)) puts("[DNS] Expected dotted-quad IPv4 address.");
        else if(eq_ci(a.cmd,"ADD") ? dns_add_raw(a.value) : dns_remove_raw(a.value)) {
            code=0;
            if(dns_snapshot(&s)) print_set("current",&s);
        } else puts("[DNS] API operation failed; check AmiNetXDuo full stack / NETADMIN.");
    }
#ifndef ANXDNS_HOST_TEST
    CloseLibrary(SocketBase);
#endif
    return code;
}
