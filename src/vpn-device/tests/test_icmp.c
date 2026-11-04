#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "../src/icmp_echo_core.h"
static void make_req(unsigned char *p, unsigned int n)
{
    unsigned int i;
    memset(p, 0, n);
    p[0] = 0x45; p[2] = n >> 8; p[3] = (unsigned char)n;
    p[4] = 0x12; p[5] = 0x34; p[8] = 55; p[9] = 1;
    p[12] = 10; p[13] = 77; p[14] = 0; p[15] = 1;
    p[16] = 10; p[17] = 77; p[18] = 0; p[19] = 2;
    p[20] = 8; p[24] = 0xab; p[25] = 0xcd; p[26] = 0; p[27] = 1;
    for(i = 28; i < n; ++i) p[i] = (unsigned char)(i * 13U);
    aovpn_set_checksum(p + 20, n - 20, 2);
    aovpn_set_checksum(p, 20, 10);
}
int main(void)
{
    unsigned char p[1400], original[1400];
    unsigned int sizes[] = {28, 64, 65, 512, 1400};
    unsigned int i, n;
    for(i = 0; i < sizeof(sizes)/sizeof(sizes[0]); ++i) {
        n = sizes[i]; make_req(p, n); memcpy(original, p, n);
        assert(aovpn_make_icmp_reply(p, n) == 1);
        assert(p[20] == 0 && p[12] == 10 && p[15] == 2 && p[19] == 1);
        assert(aovpn_checksum(p,20) == 0 && aovpn_checksum(p+20,n-20) == 0);
        assert(memcmp(p + 24, original + 24, n - 24) == 0);
        assert(aovpn_make_icmp_reply(p, n) == 0); /* no re-replies */
    }
    n=64; make_req(p,n); p[19]=3; assert(aovpn_make_icmp_reply(p,n)==0);
    make_req(p,n); p[10]^=1; assert(aovpn_make_icmp_reply(p,n)==0);
    make_req(p,n); p[25]^=1; assert(aovpn_make_icmp_reply(p,n)==0);
    make_req(p,n); p[6]=0x20; assert(aovpn_make_icmp_reply(p,n)==0);
    make_req(p,n); assert(aovpn_make_icmp_reply(p,n-1)==0);
    puts("PASS: ICMP IPv4 known-good echo packets, checksums, lengths, negative cases");
    return 0;
}
