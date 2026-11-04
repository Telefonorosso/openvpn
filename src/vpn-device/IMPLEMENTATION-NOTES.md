# Engineering decisions / POC1

- This is **vpn.device**, not a fork of sana2loop or a throwaway paired
  loopback. Source identical helpers are explicitly documented and checked.
- The code intentionally uses a **single real SANA-II device** with two
  units, rather than a private IPC protocol. Both sides use `IOSana2Req`.
- TX writes complete synchronously; RX reads may pend, respond asynchronously
  when the peer submits a write, or be canceled with `AbortIO`.
- The ring holds raw IPv4 datagrams **without Ethernet or PPP byte framing**.
  `S2WireType_PPP` advertises point-to-point semantics, not PPP negotiation.
- Supports one type (`0x0800`) deliberately; no ARP is appropriate for PPP.
- `ios2_BufferManagement` is a per-open cookie; consumers must copy it into
  every IORequest. The driver validates membership before dispatch.
- No use of DOS or C RTL inside driver hooks. `__mulsi3` and Resident
  technique reused to support freestanding linking.
- **Open issue**: payload copy under `Disable()` bounds all races simply,
  but must be measured/optimized before RC under heavy AmiOpenVPN traffic.
- **Open issue**: cross-task in-flight read vs simultaneous close has not
  been hardware stress-tested. The test probe does not exercise that race.
- SANA-II capabilities are an implementation target, not a claim of full
  SANA-II Rev 2/3 conformance until tested against the actual MiamiDX stack.
