/* Host-only syntax-check stand-ins for Amiga NDK names. NOT real headers,
 * NOT ABI conformant: NEVER include in an AmigaOS build. */
#ifndef VPN_MOCK_NDK_H
#define VPN_MOCK_NDK_H
#include <stddef.h>
#include <stdint.h>
typedef uint8_t UBYTE;
typedef uint16_t UWORD;
typedef unsigned long ULONG; /* host-pointer-width for init constant syntax */
typedef int32_t LONG;
typedef int32_t BOOL;
typedef uint32_t BPTR;
typedef void *APTR;
typedef char *STRPTR;
#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif
struct Node {struct Node *ln_Succ,*ln_Pred; UBYTE ln_Type,ln_Pri; char *ln_Name;};
struct List {struct Node *lh_Head,*lh_Tail,*lh_TailPred;};
struct Message {struct Node mn_Node;};
struct MsgPort {struct Node mp_Node; UBYTE mp_SigBit;};
struct Unit { struct MsgPort unit_MsgPort; UWORD unit_OpenCnt; };
struct Library {struct Node lib_Node; UBYTE lib_Flags; UWORD lib_NegSize,lib_PosSize,lib_Version,lib_Revision,lib_OpenCnt; APTR lib_IdString;};
struct Device {struct Library dd_Library;};
struct ExecBase {int dummy;};
struct IORequest {struct Message io_Message; struct Device *io_Device; struct Unit *io_Unit; UWORD io_Command; UBYTE io_Flags,io_Error;};
struct IOSana2Req {struct IORequest ios2_Req; ULONG ios2_WireError,ios2_PacketType; UBYTE ios2_SrcAddr[16],ios2_DstAddr[16]; ULONG ios2_DataLength; APTR ios2_Data,ios2_StatData,ios2_BufferManagement;};
struct TagItem {ULONG ti_Tag,ti_Data;};
struct Sana2DeviceStats { ULONG PacketsSent,PacketsReceived,BadData,Overruns,Unused,UnknownTypesReceived,Reconfigurations; struct {ULONG tv_secs,tv_usec;} LastStart;};
struct Sana2PacketTypeStats {ULONG PacketsSent,PacketsReceived,BytesSent,BytesReceived,PacketsDropped;};
struct Sana2DeviceQuery {ULONG SizeAvailable,SizeSupplied,DevQueryFormat,DeviceLevel; UWORD AddrFieldSize; ULONG MTU,BPS,HardwareType;};
struct Sana2SpecialStatHeader {ULONG RecordCountMax,RecordCountSupplied;};
#define TAG_DONE 0
#define TAG_IGNORE 1
#define TAG_MORE 2
#define TAG_SKIP 3
#define S2_CopyToBuff 0x100
#define S2_CopyFromBuff 0x101
#define S2WireType_PPP 3
#define SANA2_MAX_ADDR_BYTES 16
#define SANA2IOF_RAW 8
#define SANA2OPF_MINE 2
#define NT_DEVICE 3
#define NT_MESSAGE 5
#define NT_REPLYMSG 7
#define RTF_AUTOINIT 0x80
#define RTC_MATCHWORD 0x4afc
#define LIBF_SUMUSED 2
#define LIBF_CHANGED 4
#define LIBF_DELEXP 8
#define IOF_QUICK 1
#define IOERR_ABORTED 5
#define IOERR_OPENFAIL 6
#define IOERR_UNITBUSY 7
#define IOERR_NOCMD 8
#define S2ERR_BAD_ARGUMENT 20
#define S2ERR_OUTOFSERVICE 21
#define S2ERR_NO_RESOURCES 22
#define S2ERR_NOT_SUPPORTED 23
#define S2ERR_MTU_EXCEEDED 24
#define S2ERR_BAD_STATE 25
#define S2WERR_BUFF_ERROR 26
#define S2WERR_IS_CONFIGURED 27
#define S2WERR_ALREADY_TRACKED 28
#define S2WERR_NOT_TRACKED 29
#define S2EVENT_ONLINE 1
#define S2EVENT_OFFLINE 2
#define CMD_READ 2
#define CMD_WRITE 3
#define CMD_FLUSH 4
#define CMD_RESET 5
#define CMD_CLEAR 6
#define CMD_START 7
#define CMD_STOP 8
#define S2_READORPHAN 0x200
#define S2_DEVICEQUERY 0x201
#define S2_GETSTATIONADDRESS 0x202
#define S2_CONFIGINTERFACE 0x203
#define S2_ONLINE 0x204
#define S2_OFFLINE 0x205
#define S2_ONEVENT 0x206
#define S2_TRACKTYPE 0x207
#define S2_UNTRACKTYPE 0x208
#define S2_GETGLOBALSTATS 0x209
#define S2_GETTYPESTATS 0x20a
#define S2_GETSPECIALSTATS 0x20b
struct MsgPort *CreateMsgPort(void);
void DeleteMsgPort(struct MsgPort *);
struct IORequest *CreateIORequest(struct MsgPort *, ULONG);
void DeleteIORequest(struct IORequest *);
LONG OpenDevice(STRPTR, ULONG, struct IORequest *, ULONG);
void CloseDevice(struct IORequest *);
LONG DoIO(struct IORequest *);
void SendIO(struct IORequest *);
ULONG Wait(ULONG);
LONG WaitIO(struct IORequest *);
void AbortIO(struct IORequest *);
void CopyMem(const void *, void *, ULONG);
void Disable(void);
void Enable(void);
void AddTail(struct List *, struct Node *);
struct Node *RemHead(struct List *);
void Remove(struct Node *);
void ReplyMsg(struct Message *);
APTR AllocMem(ULONG, ULONG);
void FreeMem(APTR, ULONG);
#define MEMF_CLEAR 0x10000
#endif
