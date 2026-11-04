/* Private read-only lifecycle trace ABI. Not a SANA-II command. */
#ifndef AOVPN_TRACE_H
#define AOVPN_TRACE_H
#define AOVPN_TRACE_CMD 0x7F21U
#define AOVPN_TRACE_MAGIC 0x41565431UL /* AVT1 */
#define AOVPN_TRACE_VERSION 1UL
#define AOVPN_TRACE_CAP 128U
#define AOVPN_T_OPEN 1UL
#define AOVPN_T_CLOSE_IN 2UL
#define AOVPN_T_CLOSE_OUT 3UL
#define AOVPN_T_BEGIN 4UL
#define AOVPN_T_DONE 5UL
#define AOVPN_T_READ_QUEUED 6UL
#define AOVPN_T_EVENT_QUEUED 7UL
#define AOVPN_T_ABORT_IN 8UL
#define AOVPN_T_ABORT_OK 9UL
#define AOVPN_T_ABORT_MISS 10UL
#define AOVPN_T_ASYNC_READ_REPLY 11UL
#define AOVPN_T_ASYNC_ABORT_REPLY 12UL
#define AOVPN_T_ASYNC_EVENT_REPLY 13UL
struct AovpnTraceEntry {
    ULONG seq, event, request, command, type, flags, node_type;
    ULONG error, wire_error, online, opens, reads, orphans, events, fifo;
};
struct AovpnTraceDump {
    ULONG magic, version, size, unit, count, next_seq;
    struct AovpnTraceEntry entries[AOVPN_TRACE_CAP]; /* chronological */
};
struct AovpnTraceState {
    ULONG next_seq, count;
    struct AovpnTraceEntry entries[AOVPN_TRACE_CAP];
};
#endif
