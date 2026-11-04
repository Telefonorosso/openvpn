/* SPDX-License-Identifier: GPL-2.0-only
 * AmiOpenVPN POC4: SANA-II virtual TUN, vpn.device unit 1.
 * Single instance; dev null remains compatible with validated POC3.
 */
#ifdef TARGET_AMIGA
void
open_tun(const char *dev, const char *dev_type, const char *dev_node,
         struct tuntap *tt, openvpn_net_ctx_t *ctx)
{
    (void)dev; (void)dev_type; (void)ctx;
    if (tt->type == DEV_TYPE_NULL)
    {
        open_null(tt);
        return;
    }
    if (tt->type != DEV_TYPE_TUN || (dev_node && *dev_node))
    {
        msg(M_FATAL, "AmiOpenVPN POC4: use --dev tun without --dev-node");
        return;
    }
    if (!aovpn_bridge_open())
    {
        msg(M_FATAL, "AmiOpenVPN POC4: cannot open vpn.device unit 1 (check device and logs)");
        return;
    }
    tt->fd = 0; /* logical TUN only; event_t is AOVPN_TUN_EVENT (-2) */
    tt->actual_name = string_alloc("vpn.device/1", NULL);
    msg(M_INFO, "AmiOpenVPN POC4: SANA-II vpn.device unit 1 ONLINE (1400-byte IPv4)");
}
void
close_tun(struct tuntap *tt, openvpn_net_ctx_t *ctx)
{
    (void)ctx;
    if (!tt) return;
    if (tt->type == DEV_TYPE_TUN && tt->fd >= 0)
    {
        aovpn_bridge_close();
    }
    free(tt->actual_name);
    clear_tuntap(tt);
    free(tt);
}
int
read_tun(struct tuntap *tt, uint8_t *buf, int len)
{
    int n;
    if (!tt || tt->type != DEV_TYPE_TUN || tt->fd < 0)
    {
        errno = ENODEV;
        return -1;
    }
    n = aovpn_bridge_read(buf, len);
    if (n < 0 && errno != EAGAIN)
    {
        msg(M_FATAL, "AmiOpenVPN POC4: vpn.device CMD_READ failed errno=%d", errno);
    }
    return n;
}
int
write_tun(struct tuntap *tt, uint8_t *buf, int len)
{
    int n;
    if (!tt || tt->type != DEV_TYPE_TUN || tt->fd < 0)
    {
        errno = ENODEV;
        return -1;
    }
    n = aovpn_bridge_write(buf, len);
    if (n < 0)
    {
        msg(M_WARN, "AmiOpenVPN POC4: vpn.device CMD_WRITE failed errno=%d", errno);
    }
    return n;
}
#endif
