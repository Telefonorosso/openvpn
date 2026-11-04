/* Private POC2 FIX2 diagnostic ABI; not part of SANA-II. */
#ifndef AOVPN_DIAG_H
#define AOVPN_DIAG_H
#define AOVPN_DIAG_CMD 0x7F20U
#define AOVPN_DIAG_MAGIC 0x41564432UL /* AVD2 */
#define AOVPN_DIAG_VERSION 1UL
struct AovpnDiag {
    ULONG magic, version, size, unit, online, open_count, fifo_count;
    ULONG write_received, write_accepted, write_rejected;
    ULONG last_write_type, last_write_flags, last_write_error, last_write_wire_error;
    ULONG reject_offline, reject_type_or_raw, reject_length, reject_ring, reject_copy;
    ULONG read_submitted, read_queued, read_completed, read_errors;
    ULONG fifo_enqueued, fifo_delivered;
};
#endif
