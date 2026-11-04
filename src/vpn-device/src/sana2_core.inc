/* EXACT unmodified functions from sana2loop/src/loopback_device.c
   SPDX-License-Identifier: BSD-2-Clause; Copyright (c) 2026 Simon Dick
   Included after Exec/SANA-II headers and SysBase. */

static void safe_add_tail(struct List *list, struct Node *node)
{
    Disable();
    AddTail(list, node);
    Enable();
}

static struct Node *safe_rem_head(struct List *list)
{
    struct Node *n;
    Disable();
    n = RemHead(list);
    Enable();
    return n;
}

static BOOL safe_unlink_if_queued(struct List *list, struct Node *target)
{
    struct Node *n;
    BOOL found = FALSE;
    Disable();
    for (n = list->lh_Head; n->ln_Succ; n = n->ln_Succ) {
        if (n == target) {
            Remove(n);
            found = TRUE;
            break;
        }
    }
    Enable();
    return found;
}

static BOOL default_copy(APTR to asm("a0"), APTR from asm("a1"), ULONG size asm("d0"))
{
    CopyMem(from, to, size);
    return TRUE;
}

static void list_init(struct List *list)
{
    list->lh_Head     = (struct Node *)&list->lh_Tail;
    list->lh_Tail      = NULL;
    list->lh_TailPred  = (struct Node *)&list->lh_Head;
}
