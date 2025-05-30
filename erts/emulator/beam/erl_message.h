/*
 * %CopyrightBegin%
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright Ericsson AB 1997-2025. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * %CopyrightEnd%
 */

#ifndef __ERL_MESSAGE_H__
#define __ERL_MESSAGE_H__

#include "sys.h"
#include "erl_vm.h"
#define ERTS_PROC_SIG_QUEUE_TYPE_ONLY
#include "erl_proc_sig_queue.h"
#undef ERTS_PROC_SIG_QUEUE_TYPE_ONLY

#ifdef DEBUG
#define ERTS_MSG_COPY_WORDS_PER_REDUCTION 4
#else
#define ERTS_MSG_COPY_WORDS_PER_REDUCTION 64
#endif

/* The number of buffers have to be 64 or less because we currently
   use a single word to implement a bitset with information about
   non-empty buffers */
#ifdef DEBUG
#define ERTS_PROC_SIG_INQ_BUFFERED_NR_OF_BUFFERS 64
#define ERTS_PROC_SIG_INQ_BUFFERED_CONTENTION_INSTALL_LIMIT 250
#define ERTS_PROC_SIG_INQ_BUFFERED_ALWAYS_TURN_ON 1
#define ERTS_PROC_SIG_INQ_BUFFERED_MIN_FLUSH_ALL_OPS_BEFORE_CHANGE 2
#define ERTS_PROC_SIG_INQ_BUFFERED_MIN_NO_ENQUEUES_TO_KEEP \
    (ERTS_PROC_SIG_INQ_BUFFERED_MIN_FLUSH_ALL_OPS_BEFORE_CHANGE +                    \
     ERTS_PROC_SIG_INQ_BUFFERED_MIN_FLUSH_ALL_OPS_BEFORE_CHANGE / 2)
#else
#define ERTS_PROC_SIG_INQ_BUFFERED_NR_OF_BUFFERS 64
#define ERTS_PROC_SIG_INQ_BUFFERED_CONTENTION_INSTALL_LIMIT 50
#define ERTS_PROC_SIG_INQ_BUFFERED_ALWAYS_TURN_ON 0
#define ERTS_PROC_SIG_INQ_BUFFERED_MIN_FLUSH_ALL_OPS_BEFORE_CHANGE 8192
/* At least 1.5 enqueues per flush all op */
#define ERTS_PROC_SIG_INQ_BUFFERED_MIN_NO_ENQUEUES_TO_KEEP \
    (ERTS_PROC_SIG_INQ_BUFFERED_MIN_FLUSH_ALL_OPS_BEFORE_CHANGE +     \
     ERTS_PROC_SIG_INQ_BUFFERED_MIN_FLUSH_ALL_OPS_BEFORE_CHANGE / 2)
#endif

struct proc_bin;
struct external_thing_;

typedef struct erl_mesg ErtsMessage;

/*
 * This struct represents data that must be updated by structure copy,
 * but is stored outside of any heap.
 *
 * Remember to update the static assertions in `erts_init_gc` whenever a new
 * off-heap term type is added.
 */

struct erl_off_heap_header {
    Eterm thing_word;

    /* As an optimization, the first word of user data is stored before the
     * next pointer so that the meaty part of the term (e.g. ErtsDispatchable)
     * can be loaded together with the thing word on architectures that
     * support it. */
    UWord opaque;

    struct erl_off_heap_header* next;
};

#define OH_OVERHEAD(oh, size) do { \
    (oh)->overhead += size;        \
} while(0)

typedef struct erl_off_heap {
    struct erl_off_heap_header* first;
    Uint64 overhead;     /* Administrative overhead (used to force GC). */
} ErlOffHeap;

#define ERTS_INIT_OFF_HEAP(OHP)			\
    do {					\
	(OHP)->first = NULL;			\
	(OHP)->overhead = 0;			\
    } while (0)

typedef struct {
    enum {
        FACTORY_CLOSED = 0,
        FACTORY_HALLOC,
        FACTORY_MESSAGE,
        FACTORY_HEAP_FRAGS,
        FACTORY_STATIC,
        FACTORY_TMP
    } mode;
    Process* p;
    /*
       If the factory is initialized with erts_factory_proc_prealloc_init,
       hp_start points to the top of the main heap if the preallocated data
       fits in the main heap and otherwise it points to somewhere in the
       data area of a heap fragment. If the factory is initialized with any
       of the other init functions that sets the mode to FACTORY_HALLOC,
       hp_start and original_htop always have the same value.

       When erts_factory_proc_prealloc_init is used for initialization the
       preallocated data might be allocated in an existing heap fragment but
       data that is later allocated with erts_produce_heap might fit in the
       main heap, so both hp_start and original_htop are needed to correctly
       restore the heap in the erts_factory_undo function.
    */
    Eterm* hp_start;
    /*
       original_htop stores the top of the main heap at the time
       the factory was initialized and is used to reset the heap
       state if an erts_factory_undo call is made.
    */
    Eterm* original_htop;
    Eterm* hp;
    Eterm* hp_end;
    ErtsMessage *message;
    struct erl_heap_fragment* heap_frags;
    struct erl_heap_fragment* heap_frags_saved;
    Uint heap_frags_saved_used;
    ErlOffHeap* off_heap;
    ErlOffHeap off_heap_saved;
    Uint32 alloc_type;
} ErtsHeapFactory;

void erts_factory_proc_init(ErtsHeapFactory*, Process*);
void erts_factory_proc_prealloc_init(ErtsHeapFactory*, Process*, Sint size);
void erts_factory_heap_frag_init(ErtsHeapFactory*, struct erl_heap_fragment*);
ErtsMessage *erts_factory_message_create(ErtsHeapFactory *, Process *,
					  ErtsProcLocks *, Uint sz);
void erts_factory_selfcontained_message_init(ErtsHeapFactory*, ErtsMessage *, Eterm *);
void erts_factory_static_init(ErtsHeapFactory*, Eterm* hp, Uint size, ErlOffHeap*);
void erts_factory_tmp_init(ErtsHeapFactory*, Eterm* hp, Uint size, Uint32 atype);
void erts_factory_dummy_init(ErtsHeapFactory*);

ERTS_GLB_INLINE Eterm* erts_produce_heap(ErtsHeapFactory*, Uint need, Uint xtra);

Eterm* erts_reserve_heap(ErtsHeapFactory*, Uint need);
void erts_factory_close(ErtsHeapFactory*);
void erts_factory_trim_and_close(ErtsHeapFactory*,Eterm *brefs, Uint brefs_size);
void erts_factory_undo(ErtsHeapFactory*);

void erts_reserve_heap__(ErtsHeapFactory*, Uint need, Uint xtra); /* internal */

#if ERTS_GLB_INLINE_INCL_FUNC_DEF

ERTS_GLB_INLINE Eterm *
erts_produce_heap(ErtsHeapFactory* factory, Uint need, Uint xtra)
{
    Eterm* res;

    ASSERT((unsigned int)factory->mode > (unsigned int)FACTORY_CLOSED);
    if (factory->hp + need > factory->hp_end) {
	erts_reserve_heap__(factory, need, xtra);
    }
    res = factory->hp;
    factory->hp += need;
    INIT_HEAP_MEM(res, need);
    return res;
}

#endif /* ERTS_GLB_INLINE_INCL_FUNC_DEF */

#ifdef CHECK_FOR_HOLES
# define ERTS_FACTORY_HOLE_CHECK(f) do {    \
        /*if ((f)->p) erts_check_for_holes((f)->p);*/ \
    } while (0)
#else
# define ERTS_FACTORY_HOLE_CHECK(p)
#endif

#include "external.h"
#include "erl_process.h"

#define ERTS_INVALID_HFRAG_PTR ((ErlHeapFragment *) ~((UWord) 7))

/*
 * This struct represents a heap fragment, which is used when there
 * isn't sufficient room in the process heap and we can't do a GC.
 */

typedef struct erl_heap_fragment ErlHeapFragment;
struct erl_heap_fragment {
    ErlHeapFragment* next;	/* Next heap fragment */
    ErlOffHeap off_heap;	/* Offset heap data. */
    Uint alloc_size;		/* Size in words of mem */
    Uint used_size;		/* With terms to be moved to heap by GC */
    Eterm mem[1];		/* Data */
};

/* m[0] = message, m[1] = seq trace token */
#define ERL_MESSAGE_REF_ARRAY_SZ 3
#define ERL_MESSAGE_TERM(mp) ((mp)->m[0])
#define ERL_MESSAGE_TOKEN(mp) ((mp)->m[1])
#define ERL_MESSAGE_FROM(mp) ((mp)->m[2])

#ifdef USE_VM_PROBES
/* m[2] = dynamic trace user tag */
#undef ERL_MESSAGE_REF_ARRAY_SZ
#define ERL_MESSAGE_REF_ARRAY_SZ 4
#define ERL_MESSAGE_DT_UTAG(mp) ((mp)->m[3])
#else
#endif

#ifdef USE_VM_PROBES
#define have_no_seqtrace(T) ((T) == NIL || (T) == am_have_dt_utag)
#else
#define have_no_seqtrace(T) ((T) == NIL)
#endif
#define have_seqtrace(T)    (!have_no_seqtrace(T))

#define ERL_MESSAGE_REF_FIELDS__			\
    ErtsMessage *next;	/* Next message */		\
    union {						\
	ErlHeapFragment *heap_frag;			\
	void *attached;					\
    } data;						\
    Eterm m[ERL_MESSAGE_REF_ARRAY_SZ]


typedef struct erl_msg_ref__ {
    ERL_MESSAGE_REF_FIELDS__;
} ErtsMessageRef;

struct erl_mesg {
    ERL_MESSAGE_REF_FIELDS__;

    ErlHeapFragment hfrag;
};

/*
 * The ErtsMessage struct is only one special type
 * of signal. All signal structs have a common
 * beginning and can be differentiated by looking
 * at the ErtsSignal 'common.tag' field.
 *
 * - An ordinary message will have a value
 * - A distribution message that has not been
 *   decoded yet will have the non-value.
 * - Other signals will have an external pid
 *   header tag. In order to differentiate
 *   between those signals one needs to look
 *   at the arity part of the header (see
 *   erts_proc_sig_queue.h).
 */

#define ERTS_SIG_IS_NON_MSG_TAG(Tag) \
    (is_external_pid_header((Tag)))

#define ERTS_SIG_IS_NON_MSG(Sig) \
    ERTS_SIG_IS_NON_MSG_TAG(((ErtsSignal *) (Sig))->common.tag)

#define ERTS_SIG_IS_INTERNAL_MSG_TAG(Tag) \
    (!is_header((Tag)))
#define ERTS_SIG_IS_INTERNAL_MSG(Sig) \
    ERTS_SIG_IS_INTERNAL_MSG_TAG(((ErtsSignal *) (Sig))->common.tag)

#define ERTS_SIG_IS_EXTERNAL_MSG_TAG(Tag) \
    ((Tag) == THE_NON_VALUE)
#define ERTS_SIG_IS_EXTERNAL_MSG(Sig) \
    ERTS_SIG_IS_EXTERNAL_MSG_TAG(((ErtsSignal *) (Sig))->common.tag)

#define ERTS_SIG_IS_MSG_TAG(Tag) \
    (!ERTS_SIG_IS_NON_MSG_TAG(Tag))
#define ERTS_SIG_IS_MSG(Sig) \
    ERTS_SIG_IS_MSG_TAG(((ErtsSignal *) (Sig))->common.tag)

typedef union {
    ErtsSignalCommon common;
    ErtsNonMsgSignal nm_sig;
    ErtsMessageRef msg;
} ErtsSignal;

typedef struct {
    /* pointers to next pointers pointing to... */
    ErtsMessage **next; /* ... next (non-message) signal */
    ErtsMessage **last; /* ... last (non-message) signal */
} ErtsMsgQNMSigs;

/*
 * The ErtsRecvMarker struct is used for two other types of markers
 * namely yield markers and prio queue markers.
 */
#define ERTS_RECV_MARKER_TYPE_RECV              0
#define ERTS_RECV_MARKER_TYPE_YIELD             1
#define ERTS_RECV_MARKER_TYPE_PRIO_Q_END        2
#define ERTS_RECV_MARKER_TYPE_PRIO_Q_CONT       3

typedef struct {
    ErtsSignal sig;
    ErtsMessage **prev_next;
    signed char mark_type;
    signed char pass;
    signed char set_save;
    signed char in_prioq;
    signed char in_sigq;
    signed char in_msgq;
    signed char prev_ix;
    signed char next_ix;
#ifdef DEBUG
    signed char used;
    Process *proc;
#endif
} ErtsRecvMarker;

#define ERTS_RECV_MARKER_BLOCK_SIZE 8

typedef struct {
    Eterm ref[ERTS_RECV_MARKER_BLOCK_SIZE];
    ErtsRecvMarker marker[ERTS_RECV_MARKER_BLOCK_SIZE];
    signed char free_ix;
    signed char used_ix;
    signed char unused;
    signed char pending_set_save_ix;
    signed char set_save_ix;
} ErtsRecvMarkerBlock;

/* Size of default message buffer (erl_message.c) */
#define ERL_MESSAGE_BUF_SZ 500

typedef struct {
    /*
     * ** The signal queues private to a process. **
     *
     * These are:
     * - an inner queue which only consists of
     *   message signals and possibly receive markers
     * - a middle queue which contains a mixture
     *   of message and non-message signals
     *
     * When the process isn't processing signals in
     * erts_proc_sig_handle_incoming():
     * - the message queue corresponds to the inner
     *   queue. Messages in the middle queue (and
     *   in the outer queue) are in transit and
     *   have NOT been received yet!
     *
     * When the process is processing signals in
     * erts_proc_sig_handle_incoming():
     * - the message queue corresponds to the inner
     *   queue plus the head of the middle queue up
     *   to the signal currently being processed.
     *   Any messages further back in the middle queue
     *   (and in the outer queue) are still in transit
     *   and have NOT been received yet!
     *
     * In the general case the 'len' field of this
     * structure does NOT correspond to the message
     * queue length. When the process is inspected
     * via process info it does however correspond
     * to the message queue length, but this is a
     * special case!
     *
     * When no process-info request is in transit to
     * the process the 'len' field corresponds to
     * the total amount of messages in inner and
     * middle queues (which does NOT correspond to
     * the message queue length). When process-info
     * requests are in transit to the process, the
     * usage of the 'len' field changes and is used
     * as an offset which even might be negative.
     */

    /* inner queue (message queue) */
    ErtsMessage *first;
    ErtsMessage **last;  /* point to the last next pointer */
    ErtsMessage **save;
    Sint mq_len; /* Message queue length */

    /* middle queue */
    ErtsMessage *cont;
    ErtsMessage **cont_last;
    ErtsMsgQNMSigs nmsigs;
    Sint mlenoffs; /* nr of trailing msg sigs after last non-msg sig */

    /* Common for inner and middle queue */
    ErtsRecvMarkerBlock *recv_mrk_blk;
    Uint32 flags;
} ErtsSignalPrivQueues;

typedef struct ErtsSignalInQueue_ {
    ErtsMessage* first;
    ErtsMessage** last;  /* point to the last next pointer */
    Sint mlenoffs; /* nr of trailing msg sigs after last non-msg sig */
    ErtsMsgQNMSigs nmsigs;
#ifdef ERTS_PROC_SIG_HARD_DEBUG
    int may_contain_heap_terms;
#endif
} ErtsSignalInQueue;

typedef union {
    struct ___ErtsSignalInQueueBufferFields {
        erts_mtx_t lock;
        /*
         * Boolean value indicateing if the buffer is alive. An
         * enqueue attempt to a dead buffer has to be canceled
         */
        int alive;
        /*
         * The number of enqueues that has been performed to this
         * buffer. This value is used to decide if we should adapt
         * back to an unbuffered state
         */
        Uint nr_of_enqueues;
        ErtsSignalInQueue queue;
    } b;
    byte align__[ERTS_ALC_CACHE_LINE_ALIGN_SIZE(sizeof(struct ___ErtsSignalInQueueBufferFields))];
} ErtsSignalInQueueBuffer;

#if ERTS_PROC_SIG_INQ_BUFFERED_NR_OF_BUFFERS > 64
#error The data structure holding information about which slots that are non-empty (the nonempty_slots field in the struct below) needs to be changed (it currently only supports up to 64 slots)
#endif

typedef struct {
    ErtsSignalInQueueBuffer slots[ERTS_PROC_SIG_INQ_BUFFERED_NR_OF_BUFFERS];
    ErtsThrPrgrLaterOp free_item;
    erts_atomic64_t nonempty_slots;
    erts_atomic32_t nonmsgs_in_slots;
    erts_atomic32_t msgs_in_slots;
    /*
     * dirty_refc is incremented by dirty schedulers that access the
     * buffer array to prevent deallocation while they are accessing
     * the buffer array. This is needed since dirty schedulers are not
     * part of the thread progress system.
     */
    erts_refc_t dirty_refc;
    Uint nr_of_rounds_left;
    Uint nr_of_enqueues;
    int alive;
} ErtsSignalInQueueBufferArray;

typedef struct erl_trace_message_queue__ {
    struct erl_trace_message_queue__ *next; /* point to the next receiver */
    Eterm receiver;
    ErtsMessage* first;
    ErtsMessage** last;  /* point to the last next pointer */
    Sint len;            /* queue length */
} ErlTraceMessageQueue;

/* Get "current" message */

#ifdef USE_VM_PROBES
#define LINK_MESSAGE_DTAG(mp, dt) ERL_MESSAGE_DT_UTAG(mp) = dt
#else
#define LINK_MESSAGE_DTAG(mp, dt)
#endif

#ifdef USE_VM_PROBES
#  define ERTS_MSG_RECV_TRACED(P)                                       \
    (ERTS_IS_P_TRACED_FL(P, F_TRACE_RECEIVE)                            \
     || DTRACE_ENABLED(message_queued))
#else
#  define ERTS_MSG_RECV_TRACED(P)                                       \
    (ERTS_IS_P_TRACED_FL(P, F_TRACE_RECEIVE))

#endif

/* Add one message last in message queue */
#define LINK_MESSAGE(p, msg, ps)                                        \
    do {                                                                \
        ASSERT(ERTS_SIG_IS_MSG(msg));                                   \
        ERTS_HDBG_CHECK_SIGNAL_IN_QUEUE__((p), &(p)->sig_inq, "before");\
        ERTS_HDBG_INQ_LEN(&(p)->sig_inq);                               \
        *(p)->sig_inq.last = (msg);                                     \
        (p)->sig_inq.last = &(msg)->next;                               \
        (p)->sig_inq.mlenoffs++;                                        \
        if (!((ps) & ERTS_PSFLG_MSG_SIG_IN_Q))                          \
            (void) erts_atomic32_read_bor_nob(&(p)->state,              \
                                              ERTS_PSFLG_MSG_SIG_IN_Q); \
        ERTS_HDBG_INQ_LEN(&(p)->sig_inq);                               \
        ERTS_HDBG_CHECK_SIGNAL_IN_QUEUE__((p), &(p)->sig_inq, "after"); \
    } while(0)

#define ERTS_HEAP_FRAG_SIZE(DATA_WORDS) \
   (sizeof(ErlHeapFragment) - sizeof(Eterm) + (DATA_WORDS)*sizeof(Eterm))

#define ERTS_INIT_HEAP_FRAG(HEAP_FRAG_P, USED_WORDS, DATA_WORDS)	\
    do {								\
	(HEAP_FRAG_P)->next = NULL;					\
	(HEAP_FRAG_P)->alloc_size = (DATA_WORDS);			\
	(HEAP_FRAG_P)->used_size = (USED_WORDS);			\
	(HEAP_FRAG_P)->off_heap.first = NULL;				\
	(HEAP_FRAG_P)->off_heap.overhead = 0;				\
    } while (0)

#ifdef USE_VM_PROBES
#define ERL_MESSAGE_DT_UTAG_INIT(MP) ERL_MESSAGE_DT_UTAG(MP) = NIL
#else
#define ERL_MESSAGE_DT_UTAG_INIT(MP) do{ } while (0)
#endif

#define ERTS_INIT_MESSAGE(MP)                           \
    do {                                                \
        (MP)->next = NULL;                              \
        ERL_MESSAGE_TERM(MP) = THE_NON_VALUE;           \
        ERL_MESSAGE_TOKEN(MP) = THE_NON_VALUE;          \
        ERL_MESSAGE_FROM(MP) = NIL;                     \
        ERL_MESSAGE_DT_UTAG_INIT(MP);                   \
        MP->data.attached = NULL;                       \
    } while (0)

void init_message(void);
ErlHeapFragment* new_message_buffer(Uint);
ErlHeapFragment* erts_resize_message_buffer(ErlHeapFragment *, Uint,
					    Eterm *, Uint);
void free_message_buffer(ErlHeapFragment *);
void erts_queue_dist_message(Process*, ErtsProcLocks, ErtsDistExternal *,
                             ErlHeapFragment *, Eterm, Eterm);
void erts_queue_message(Process*, ErtsProcLocks,ErtsMessage*, Eterm, Eterm);
void erts_queue_message_token(Process*, ErtsProcLocks,ErtsMessage*, Eterm, Eterm, Eterm);
void erts_queue_proc_message(Process* from,Process* to, ErtsProcLocks,ErtsMessage*, Eterm);
void erts_queue_proc_messages(Process* from, Process* to, ErtsProcLocks,
                              ErtsMessage*, ErtsMessage**, Uint);
void erts_deliver_exit_message(Eterm, Process*, ErtsProcLocks *, Eterm, Eterm);
void erts_send_message(Process*, Process*, ErtsProcLocks*, Eterm);
void erts_link_mbuf_to_proc(Process *proc, ErlHeapFragment *bp);

Uint erts_msg_attached_data_size_aux(ErtsMessage *msg);

void erts_cleanup_offheap_list(struct erl_off_heap_header* first);
void erts_cleanup_offheap(ErlOffHeap *offheap);
void erts_save_message_in_proc(Process *p, ErtsMessage *msg);
Sint erts_move_messages_off_heap(Process *c_p);
Sint erts_complete_off_heap_message_queue_change(Process *c_p);
Eterm erts_change_message_queue_management(Process *c_p, Eterm new_state);

void erts_cleanup_messages(ErtsMessage *mp);

void erts_free_message_ref(void *);
void *erts_alloc_message_ref(void) ERTS_ATTR_MALLOC_D(erts_free_message_ref,1);

ErtsMessage *erts_try_alloc_message_on_heap(Process *pp,
					    erts_aint32_t *psp,
					    ErtsProcLocks *plp,
					    Uint sz,
					    Eterm **hpp,
					    ErlOffHeap **ohpp,
					    int *on_heap_p);
ErtsMessage *erts_realloc_shrink_message(ErtsMessage *mp, Uint sz,
					 Eterm *brefs, Uint brefs_size);

ERTS_GLB_FORCE_INLINE ErtsMessage *erts_alloc_message(Uint sz, Eterm **hpp);
ERTS_GLB_FORCE_INLINE ErtsMessage *erts_shrink_message(ErtsMessage *mp, Uint sz,
						       Eterm *brefs, Uint brefs_size);
ERTS_GLB_FORCE_INLINE void erts_free_message(ErtsMessage *mp);
ERTS_GLB_INLINE Uint erts_used_frag_sz(const ErlHeapFragment*);
ERTS_GLB_INLINE Uint erts_msg_attached_data_size(ErtsMessage *msg);

#define ERTS_MSG_COMBINED_HFRAG ((void *) 0x1)

#define erts_message_to_heap_frag(MP)                   \
    (((MP)->data.attached == ERTS_MSG_COMBINED_HFRAG) ? \
        &(MP)->hfrag : (MP)->data.heap_frag)

#if ERTS_GLB_INLINE_INCL_FUNC_DEF

ERTS_GLB_FORCE_INLINE ErtsMessage *erts_alloc_message(Uint sz, Eterm **hpp)
{
    ErtsMessage *mp;

    if (sz == 0) {
	mp = (ErtsMessage *)erts_alloc_message_ref();
        ERTS_INIT_MESSAGE(mp);
	if (hpp)
	    *hpp = NULL;
	return mp;
    }

    mp = (ErtsMessage *)erts_alloc(
        ERTS_ALC_T_MSG, sizeof(ErtsMessage) + (sz - 1)*sizeof(Eterm));

    ERTS_INIT_MESSAGE(mp);
    mp->data.attached = ERTS_MSG_COMBINED_HFRAG;
    ERTS_INIT_HEAP_FRAG(&mp->hfrag, sz, sz);

    if (hpp)
	*hpp = &mp->hfrag.mem[0];

    return mp;
}

ERTS_GLB_FORCE_INLINE ErtsMessage *
erts_shrink_message(ErtsMessage *mp, Uint sz, Eterm *brefs, Uint brefs_size)
{
    if (sz == 0) {
	ErtsMessage *nmp;
	if (!mp->data.attached)
	    return mp;
	ASSERT(mp->data.attached == ERTS_MSG_COMBINED_HFRAG);
	nmp = (ErtsMessage *)erts_alloc_message_ref();
#ifdef DEBUG
	if (brefs && brefs_size) {
	    int i;
	    for (i = 0; i < brefs_size; i++)
		ASSERT(is_non_value(brefs[i]) || is_immed(brefs[i]));
	}
#endif
	erts_free(ERTS_ALC_T_MSG, mp);
	return nmp;
    }

    ASSERT(mp->data.attached == ERTS_MSG_COMBINED_HFRAG);
    ASSERT(mp->hfrag.used_size >= sz);

    if (sz >= (mp->hfrag.alloc_size - mp->hfrag.alloc_size / 16)) {
	mp->hfrag.used_size = sz;
	return mp;
    }

    return erts_realloc_shrink_message(mp, sz, brefs, brefs_size);
}

ERTS_GLB_FORCE_INLINE void erts_free_message(ErtsMessage *mp)
{
    if (mp->data.attached != ERTS_MSG_COMBINED_HFRAG)
	erts_free_message_ref(mp);
    else
	erts_free(ERTS_ALC_T_MSG, mp);
}

ERTS_GLB_INLINE Uint erts_used_frag_sz(const ErlHeapFragment* bp)
{
    Uint sz = 0;
    for ( ; bp!=NULL; bp=bp->next) {
	sz += bp->used_size;
    }
    return sz;
}

ERTS_GLB_INLINE Uint erts_msg_attached_data_size(ErtsMessage *msg)
{
    ASSERT(msg->data.attached);

    if (ERTS_SIG_IS_INTERNAL_MSG(msg))
	return erts_used_frag_sz(erts_message_to_heap_frag(msg));

    return erts_msg_attached_data_size_aux(msg);
}

#endif

Uint erts_mbuf_size(Process *p);
#if defined(DEBUG) || 0
#  define ERTS_CHK_MBUF_SZ(P)				\
    do {						\
	Uint actual_mbuf_sz__ = erts_mbuf_size((P));	\
	ERTS_ASSERT((P)->mbuf_sz >= actual_mbuf_sz__);	\
    } while (0)
#else
#  define ERTS_CHK_MBUF_SZ(P) ((void) 1)
#endif

#define ERTS_FOREACH_SIG_PRIVQS(PROC, MVAR, CODE)                       \
    do {                                                                \
        int i__;                                                        \
        ErtsMessage *msgs__[2] = {(PROC)->sig_qs.first,                 \
                                  (PROC)->sig_qs.cont};                 \
        for (i__ = 0; i__ < sizeof(msgs__)/sizeof(msgs__[0]); i__++) {  \
            ErtsMessage *MVAR;                                          \
            for (MVAR = msgs__[i__]; MVAR; MVAR = MVAR->next) {         \
                CODE;                                                   \
            }                                                           \
        }                                                               \
    } while (0)

#endif
