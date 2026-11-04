# Third-party notices

`sana2loop` by Simon Dick (2026), original source file:
`sana2loop/src/loopback_device.c`, license BSD-2-Clause.
Original license preserved verbatim at `third_party/sana2loop/LICENSE`.

Verbatim function bodies in `src/sana2_core.inc`:
- `safe_add_tail`
- `safe_rem_head`
- `safe_unlink_if_queued`
- `default_copy`
- `list_init`

The `__mulsi3` implementation was also copied verbatim into
`src/vpn_device.c` for a freestanding 68000 link.

The rest of the device's control flow and the IP FIFO are independent
`vpn.device` code, not copies of sana2loop's echo, crossover, replay,
recording or fault-injection engine.
