# FIX4 implementation notes

Reference: TRACE1 hardware log; baseline FIX3C known to remain ONLINE with RX disabled.

1. Restore real `CMD_READ(0x0021)` (already present in TRACE1; unchanged).
2. In `CMD_FLUSH`, abort only typed/orphan read queues; preserve `S2_ONEVENT`, matching loopback.device.
3. Keep `CMD_RESET` semantics unchanged from TRACE1.
4. Preserve `AOVPN-Trace` and `AOVPN-Diag` for hardware falsification.
5. Add host regression of 15 queued RX after flush with pending S2_ONEVENT, and 15 real bidirectional read completions.
6. Bundle the original POC2 ICMP peer source; it is a diagnostic endpoint, not the VPN implementation.

**Hypothesis limit:** TRACE1's very first CMD_FLUSH preceded event cancellation. This change alone is not demonstrated to prevent MiamiDX's initial teardown.
