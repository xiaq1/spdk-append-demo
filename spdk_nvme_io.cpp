/***************************************************************************
* 
* Copyright (c) 2019 Haha.com, Inc. All Rights Reserved
* 
**************************************************************************/
/**
 * @file spdk_nvme_io.cpp
 * @author xiaqichao(com@Haha.com)
 * @date 2019/03/11 08:08:15
 * @brief 
 *  
 **/

#include <list>
#include <string>
#include "common/utils.h"           // parse_message_from_iobuf
#include "common/libaio.h"           // parse_message_from_iobuf
#include "common/rpc_call.h"
#include "spdk_nvme_io.h"
#include "spdk/env.h"
#include "spdk/fd.h"
#include "spdk/nvme.h"
#include "spdk/queue.h"
#include "spdk/string.h"
#include "spdk/nvme_intel.h"
#include "spdk/histogram_data.h"
#include "spdk/endian.h"
#include "spdk/dif.h"

//namespace bce {
//namespace cds {

//#define TRACE_ALL_IO 1
struct ctrlr_entry *g_controllers = NULL;
struct ns_entry *g_namespaces = NULL;
#ifdef USE_CMB
BufferSequenceMap g_sequence_buf_map;
#endif
SequenceAndQpairList g_sequenceqpair_free;
SequenceAndQpairList g_sequenceqpair_submitted;
QpairOwnerMap g_qpair_owner_map;

//#define BLOCK_ALIN_SZ(sz) ((sz & (BLOCK_SIZE -1)) == 0 ? sz : sz + BLOCK_SIZE - (sz & (BLOCK_SIZE -1)))
#define BLOCK_ALIN_SZ(sz) (sz < BLOCK_SIZE ? BLOCK_SIZE : sz) 
void dio_complete(void *arg, const struct spdk_nvme_cpl *completion) {
    struct spdk_nvme_sequence *sequence = (struct spdk_nvme_sequence *)arg;
    sequence->is_completed = 1;
}

void aio_complete(void *arg, const struct spdk_nvme_cpl *completion) {
    struct spdk_nvme_sequence *sequence = (struct spdk_nvme_sequence *)arg;
    // call fs callback first
    if (completion->status.sc == 0 && sequence && sequence->io_cb) {
        sequence->io_cb(sequence->cb_args);
    }
    sequence->event.res2 = -completion->status.sc;
    if (completion->status.sc == 0) {
        sequence->event.res = sequence->sz;
        // only move to subitted when it success
    } else {
        sequence->event.res = 0;
    }
    // TODO: should we update list info just when IO really sucess ?
    //if (!sequence->sequenceqpair->submitted_sequences.empty()) {
    //}
    //printf("sequence: %p aio IO completes !\n", sequence);
    sequence->sequenceqpair->processed_sequences.push_back(sequence);
    sequence->sequenceqpair->submitted_sequences.pop_front();
}

bool probe_cb(void *cb_ctx, const struct spdk_nvme_transport_id *trid,
     struct spdk_nvme_ctrlr_opts *opts)
{
    opts->num_io_queues = MAX_QPAIR;
    LOG(INFO) << "Attaching to" << trid->traddr;
    return true;
}

#if 0
static void
setup_qpairs(struct spdk_nvme_ctrlr *ctrlr, uint32_t num_io_queues)
{
	uint32_t i;

	//pthread_mutex_init(&ctrlr->ctrlr_lock, NULL);
	//nvme_ctrlr_construct(ctrlr);

	ctrlr->page_size = 0x1000;
	ctrlr->opts.num_io_queues = num_io_queues;
	ctrlr->free_io_qids = spdk_bit_array_create(num_io_queues + 1);
	//SPDK_CU_ASSERT_FATAL(ctrlr->free_io_qids != NULL);

	//spdk_bit_array_clear(ctrlr->free_io_qids, 0);
	//for (i = 1; i <= num_io_queues; i++) {
    //  spdk_bit_array_set(ctrlr->free_io_qids, i);
	//}
}
#endif

void register_ns(struct spdk_nvme_ctrlr *ctrlr, struct spdk_nvme_ns *ns)
{
    struct ns_entry *entry;
    const struct spdk_nvme_ctrlr_data *cdata;
	//struct spdk_nvme_io_qpair_opts opts;

    /*
     * spdk_nvme_ctrlr is the logical abstraction in Spdk for an NVMe
     *  controller.  During initialization, the IDENTIFY data for the
     *  controller is read using an NVMe admin command, and that data
     *  can be retrieved using spdk_nvme_ctrlr_get_data() to get
     *  detailed information on the controller.  Refer to the NVMe
     *  specification for more details on IDENTIFY for NVMe controllers.
     */
    cdata = spdk_nvme_ctrlr_get_data(ctrlr);

    if (!spdk_nvme_ns_is_active(ns)) {
        printf("Controller %-20.20s (%-20.20s): Skipping inactive NS %u\n",
               cdata->mn, cdata->sn,
               spdk_nvme_ns_get_id(ns));
        return;
    }

    entry =(ns_entry *)malloc(sizeof(struct ns_entry));
    if (entry == NULL) {
        perror("ns_entry malloc");
        exit(1);
    }

    entry->ctrlr = ctrlr;
    entry->ns = ns;
    entry->next = g_namespaces;

    /* alloc a io qpaire */
    //setup_qpairs(entry->ctrlr, MAX_QPAIR);
    for (int i = 0; i < MAX_QPAIR; ++i) {
           SequenceAndQpair* iter = new SequenceAndQpair(entry->ctrlr);
            if (iter) {
                //printf("qpair %p allocated\n", qpair);
                g_sequenceqpair_free.push_back(iter);
            }
    }
    printf("Initialization complete.\n");

    g_namespaces = entry;
    printf("  Namespace ID: %d size: %juGB\n", spdk_nvme_ns_get_id(ns),
           spdk_nvme_ns_get_size(ns) / 1000000000);
}

void attach_cb(void *cb_ctx, const struct spdk_nvme_transport_id *trid,
      struct spdk_nvme_ctrlr *ctrlr, const struct spdk_nvme_ctrlr_opts *opts)
{
    int nsid, num_ns;
    struct ctrlr_entry *entry;
    struct spdk_nvme_ns *ns;
    const struct spdk_nvme_ctrlr_data *cdata = spdk_nvme_ctrlr_get_data(ctrlr);

    entry = (struct ctrlr_entry *)malloc(sizeof(struct ctrlr_entry));
    if (entry == NULL) {
        perror("ctrlr_entry malloc");
        exit(1);
    }

    printf("Attached to %s\n", trid->traddr);
    printf("Origin io_queue_pair count: %d, io_queue_size: %d, io_queue_requests:%d\n",
            opts->num_io_queues, opts->io_queue_size,  opts->io_queue_requests);

    snprintf(entry->name, sizeof(entry->name), "%-20.20s (%-20.20s)", cdata->mn, cdata->sn);
    entry->ctrlr = ctrlr;
    entry->next = g_controllers;
    g_controllers = entry;

    /*
     * Each controller has one or more namespaces.  An NVMe namespace is basically
     *  equivalent to a SCSI LUN.  The controller's IDENTIFY data tells us how
     *  many namespaces exist on the controller.  For Intel(R) P3X00 controllers,
     *  it will just be one namespace.
     *
     * Note that in NVMe, namespace IDs start at 1, not 0.
     */
    num_ns = spdk_nvme_ctrlr_get_num_ns(ctrlr);
    printf("Using controller %s with %d namespaces.\n", entry->name, num_ns);
    for (nsid = 1; nsid <= num_ns; nsid++) {
        ns = spdk_nvme_ctrlr_get_ns(ctrlr, nsid);
        if (ns == NULL) {
            continue;
        }
        register_ns(ctrlr, ns);
    }

    printf("Now io_queue_pair count: %d, io_queue_size: %d, io_queue_requests:%d\n",
            opts->num_io_queues, opts->io_queue_size,  opts->io_queue_requests);
}

void cleanup(void)
{
    struct ns_entry *entry = g_namespaces;
    struct ctrlr_entry *ctrlr_entry = g_controllers;

    while (entry) {
        struct ns_entry *next = entry->next;
        free(entry);
        entry = next;
    }

    while (ctrlr_entry) {
        struct ctrlr_entry *next = ctrlr_entry->next;
        spdk_nvme_detach(ctrlr_entry->ctrlr);
        free(ctrlr_entry);
        ctrlr_entry = next;
    }
}

int spdk_single_dio(int fd, uint64_t offset, uint32_t sz, char *buf, uint8_t op)
{
    struct ns_entry            *entry = g_namespaces;
    int                rc = 0;
    uint32_t length = BLOCK_ALIN_SZ(sz);
#ifdef USE_CMB
    BufferSequenceMap::iterator iter = g_sequence_buf_map.find(buf);
    if (iter == g_sequence_buf_map.end()) {
        LOG(ERROR) << "Fail to find the buf: " <<  (uint64_t)buf;
        return -1;
    }
    struct spdk_nvme_sequence *sequence = iter->second;
#else
#if MULTI
    SequenceAndQpair* iter = NULL;
    auto owned_iter = g_qpair_owner_map.find(fd);
    if (owned_iter == g_qpair_owner_map.end()) {
        if (g_sequenceqpair_free.size() == 0) {
            LOG(ERROR) << "EAGAIN: fail to find free hw queue pair";
            return -EAGAIN;
        } else {
            iter = g_sequenceqpair_free.front();
        }
        if (iter->free_sequences.size() == 0) {
            LOG(ERROR) << "EAGAIN: free sequence is empty for the first free hw queue";
            return -EAGAIN;
        }
        g_sequenceqpair_free.pop_front();
        g_qpair_owner_map.insert(QpairOwnerPair(fd, iter));
        g_sequenceqpair_submitted.push_back(iter);
    } else {
        iter = owned_iter->second;
    }
#else
    SequenceAndQpair* iter = g_sequenceqpair_free.back();
#endif
    struct spdk_nvme_sequence *sequence = iter->free_sequences.front();
#endif
#ifdef TRACE_ALL_IO
    LOG(INFO) << "OP: " << (op == IO_CMD_PWRITE ? "Write, Offset:" : "Read, Offset: ") << offset << ", Size: " <<
        sz << ", Buffer: " << (uint64_t)buf;
#endif
    if (entry != NULL) {
        /*
         * Allocate an I/O qpair that we can use to submit read/write requests
         *  to namespaces on the controller.  NVMe controllers typically support
         *  many qpairs per controller.  Any I/O qpair allocated for a controller
         *  can submit I/O to any namespace on that controller.
         *
         * The Spdk NVMe driver provides no mysynchronization for qpair accesses -
         *  the application must ensure only a single thread submits I/O to a
         *  qpair, and that same thread must also check for completions on that
         *  qpair.  This enables extremely efficient I/O processing by making all
         *  I/O operations completely lockless.
         */
        sequence->is_completed = 0;
        if (op == IO_CMD_PWRITE) {
            rc = spdk_nvme_ns_cmd_write(entry->ns, iter->qpair, buf,
                        offset >> 9, /* LBA start */
                        length >> 9, /* number of LBAs */
                        dio_complete, sequence, 0);
        } else {
            rc = spdk_nvme_ns_cmd_read(entry->ns, iter->qpair, buf,
                        offset >> 9, /* LBA start */
                        length >> 9, /* number of LBAs */
                        dio_complete, sequence, 0);
        }
        if (rc != 0) {
            fprintf(stderr, "starting write I/O failed, %d\n", rc);
            return -4;
        }
        do {
            spdk_nvme_qpair_process_completions(iter->qpair, 1);
        } while (!sequence->is_completed);
    }

    /* free qpair map and commited hardware queue pair */
#ifdef MULTI
    g_sequenceqpair_submitted.pop_back();
    g_qpair_owner_map.erase(fd);
    g_sequenceqpair_free.push_front(iter);
#endif
    return length;
}
static int spdk_single_aio(io_context_t ctx, struct iocb * ioc, IOCb io_cb, void * cb_args)
{
    struct ns_entry            *entry = g_namespaces;
    int                rc = 0;
#ifdef TRACE_ALL_IO
    LOG(INFO) << "OP: " << (op == IO_CMD_PWRITE ? "Write, Offset:" : "Read, Offset: ") << offset << ", Size: " <<
        sz << ", Buffer: " << (uint64_t)buf;
#endif
    SequenceAndQpair* iter = NULL;
#ifdef MULI
    auto owned_iter = g_qpair_owner_map.find(*(int *)ctx);
    if (owned_iter == g_qpair_owner_map.end()) {
        if (g_sequenceqpair_free.size() == 0) {
            //LOG(ERROR) << "EAGAIN: fail to find free hw queue pair";
            //return -EAGAIN;
            iter = g_sequenceqpair_submitted.front();
        } else {
            iter = g_sequenceqpair_free.front();
            if (iter->free_sequences.size() == 0) {
                //LOG(ERROR) << "EAGAIN: free sequence is empty for the first free hw queue";
                iter = g_sequenceqpair_submitted.front();
                //return -EAGAIN;
            } else {
                g_sequenceqpair_free.pop_front();
                g_sequenceqpair_submitted.push_back(iter);
            }
        }
        g_qpair_owner_map.insert(QpairOwnerPair(*(int *)ctx, iter));
    } else {
        iter = owned_iter->second;
    }
#else
    iter = g_sequenceqpair_free.front();
#endif
#if 1
    if (iter->free_sequences.empty()) {
        //g_sequenceqpair_free.pop_front();
        //g_sequenceqpair_submitted.push_back(iter);
        LOG(ERROR) << "EAGAIN: free sequence is empty for the first free hw queue";
        return -EAGAIN;
    }
    //LOG(NOTICE) << "SubmitIO fd:" << *(int *)ctx << "use qpair: " << (uint64_t)iter;
    spdk_nvme_sequence * sequence = iter->free_sequences.front();
    sequence->buf = (char *)ioc->u.c.buf;
    sequence->sz = ioc->u.c.nbytes;
    sequence->ctx = ctx;
    sequence->event.obj = ioc;
    sequence->cb_args = cb_args;
    sequence->io_cb = io_cb;
    iter->free_sequences.pop_front();
    iter->submitted_sequences.push_back(sequence);
#endif
    //printf("push qpair: %p to used queue!\n", qpair);
    if (ioc->aio_lio_opcode == IO_CMD_PWRITE) {
        rc = spdk_nvme_ns_cmd_write(entry->ns, iter->qpair, ioc->u.c.buf,
                    ioc->u.c.offset >> 9, /* LBA start */
                    ioc->u.c.nbytes >> 9, /* number of LBAs */
                    aio_complete, sequence, 0);
    } else {
        rc = spdk_nvme_ns_cmd_read(entry->ns, iter->qpair, ioc->u.c.buf,
                    ioc->u.c.offset >> 9, /* LBA start */
                    ioc->u.c.nbytes >> 9, /* number of LBAs */
                    aio_complete, sequence, 0);
    }
    if (rc != 0) {
        // FIX me: this should re-vert last opeartion
        printf("starting write I/O failed, push back, reback to previous status\n");
        //g_sequenceqpair_submitted.pop_back();
        //g_sequenceqpair_free.push_front(iter);
        return -EAGAIN;
    }
    return ioc->u.c.nbytes;
}

/*
 * Init global context and bdev env 
 */
int single_write(int fd, uint64_t offset, uint32_t len, char *buf)  {
    return spdk_single_dio(fd, offset, len, buf, IO_CMD_PWRITE);
}

int single_read(int fd, uint64_t offset, uint32_t len, char *buf)  {
    return spdk_single_dio(fd, offset, len, buf, IO_CMD_PREAD);
}

int myopen(const char * name) {
    return 0;
}

int batch_read(int fd, DemoIovector * vec) {
    int sz = 0;
    for (const auto & iter : *vec) {
        sz += spdk_single_dio(fd, iter->offset, iter->len, iter->buf, IO_CMD_PREAD);
    }
    return sz;
}

int batch_write(int fd, DemoIovector * vec) {
    int sz = 0;
    for (const auto & iter : *vec) {
        sz += spdk_single_dio(fd, iter->offset, iter->len, iter->buf, IO_CMD_PWRITE);
    }
    return sz;
}

int nvme_aio_submit(io_context_t ctx, iocb * ioc, IOCb io_cb, void * cb_args) {
    int ret = spdk_single_aio(ctx, ioc, io_cb, cb_args);
    //int ret = spdk_single_aio(ctx, ioc->u.c.offset, ioc->u.c.nbytes, (char *)(ioc->u.c.buf), ioc->aio_lio_opcode, io_cb, cb_args);
    if (ret < 0) {
        return ret;
    } else {
        return ret;
    }
}

/*
 * TBD later
 */
int mysync() {
    return 0;
}

/*
 * TBD later
 */
int myclose() {
    return 0;
}

/*
 * TBD later
 */
int discard_disk() {
    LOG(INFO) << "spdk discard disk done" << std::endl;
    return 0;
}

/*
 * Init bdev env per disk
 */
int init(const char * dev_name) {
    int rc;
    struct spdk_env_opts opts;

    /*
     * Spdk relies on an abstraction around the local environment
     * named env that handles memory allocation and PCI device operations.
     * This library must be initialized first.
     *
     */
    spdk_env_opts_init(&opts);
    opts.name = "append_demo";
    opts.shm_id = 0;
    //opts.core_mask = "0xffffffff";
    if (spdk_env_init(&opts) < 0) {
        fprintf(stderr, "Unable to initialize Spdk env\n");
        return -1;
    }

    printf("Initializing NVMe Controllers\n");

    /*
     * Start the Spdk NVMe enumeration process.  probe_cb will be called
     *  for each NVMe controller found, giving our application a choice on
     *  whether to attach to each controller.  attach_cb will then be
     *  called for each controller after the Spdk NVMe driver has completed
     *  initializing the controller we chose to attach.
     */
    rc = spdk_nvme_probe(NULL, NULL, probe_cb, attach_cb, NULL);
    if (rc != 0) {
        fprintf(stderr, "spdk_nvme_probe() failed\n");
        cleanup();
        return -2;
    }

    if (g_controllers == NULL) {
        fprintf(stderr, "no NVMe controllers found\n");
        cleanup();
        return -3;
    }

    return 0;
}

int deinit(const char * dev_name)  {
    // free all qpairs
    /*
    while (!g_sequenceqpair_free.empty()) {
        auto iter = g_sequenceqpair_free.front();
        delete iter;
        g_sequenceqpair_free.pop_front();
    }
    while (!g_sequenceqpair_submitted.empty()) {
        auto iter = g_sequenceqpair_submitted.front();
        delete iter;
        g_sequenceqpair_submitted.pop_front();
    } */
    cleanup();
    return 0;
}

int format(const char * dev_name)  {
    return 0;
}

/*
 * make sz BLOCK_SIZE aligned
 */
char * mem_alloc(uint32_t sz) {
    //LOG(INFO) << "Begin alloc mememory ...." << std::endl;
    char *buf = (char *)spdk_zmalloc(BLOCK_ALIN_SZ(sz), BLOCK_SIZE, NULL,
            SPDK_ENV_SOCKET_ID_ANY, SPDK_MALLOC_DMA);
#ifdef TRACE_ALL_IO
    LOG(INFO) << "buf: " << (uint64_t)buf << " size: " << BLOCK_ALIN_SZ(sz) << " allocated";
#endif
#ifdef USE_CMB
    sequence->using_cmb_io = 0;
    sequence->sz = BLOCK_ALIN_SZ(sz);
    if (buf == NULL) {
        LOG(ERROR) << "spdk_zmalloc: buffer allocation failed\n" << std::endl;
        sequence->using_cmb_io = 1;
        //buf = spdk_zmalloc(sz, sz > 0x1000 ? 0x1000 : sz, NULL, SPDK_ENV_SOCKET_ID_ANY, SPDK_MALLOC_DMA);
        //buf = (char *)spdk_zmalloc(sequence->sz, BLOCK_SIZE, NULL, SPDK_ENV_SOCKET_ID_ANY, SPDK_MALLOC_DMA);
        buf = (char *)spdk_nvme_ctrlr_alloc_cmb_io_buffer(g_namespaces->ctrlr, sequence->sz);
    }
    spdk_nvme_sequence * sequence = (spdk_nvme_sequence *)malloc(sizeof(struct spdk_nvme_sequence));
    if (sequence == NULL) {
        if (buf) {
            if (sequence->using_cmb_io == 0) {
                spdk_free(buf);
            } else {
                spdk_nvme_ctrlr_free_cmb_io_buffer(g_namespaces->ctrlr, buf, sequence->sz);
            }
        }
        return NULL;
    }
    if (buf == NULL) {
        LOG(ERROR) << "spdk_nvme_ctrlr_alloc_cmb_io_buffer: buffer allocation failed\n";
        free(sequence);
        return NULL;
    }
#ifdef TRACE_ALL_IO
    LOG(INFO) << "buf: " << (uint64_t)buf << " sequence: " << (uint64_t)sequence <<
        " using_cmb_io: " << sequence->using_cmb_io << " allocated" << std::endl;
#endif
    g_sequence_buf_map.insert(BufferSequencePair(buf, sequence));
#endif
    return buf;
}
   
// free buf for IO
int mem_free(char * buf)
{
    if (buf == NULL) {
        LOG(ERROR) << buf << " : is already NULL\n";
        return 0;
    }
#ifndef USE_CMB
    spdk_free(buf);
#else
    BufferSequenceMap::iterator iter = g_sequence_buf_map.find(buf);
    if (iter == g_sequence_buf_map.end()) {
        LOG(ERROR) << "Fail to find the buf: " <<  (uint64_t)buf;
        return -1;
    }
    struct spdk_nvme_sequence *sequence = iter->second;

#ifdef TRACE_ALL_IO
    LOG(INFO) << "buf: " << (uint64_t)buf << " sequence: " << (uint64_t)sequence <<
        " using_cmb_io:" << sequence->using_cmb_io << " to free" << std::endl;
#endif
    if (sequence->using_cmb_io != 0) {
        spdk_nvme_ctrlr_free_cmb_io_buffer(g_namespaces->ctrlr, sequence->buf, sequence->sz);
    } else {
        spdk_free(buf);
    }
    free(sequence);
    g_sequence_buf_map.erase(*iter);
#endif
    return 0;
}

void merge_io_vectors(std::vector<uint32_t> * blk_index, char * buf, DemoIovector *data_requests)
{
    size_t j = 0;
    for (size_t i = 0; i < blk_index->size(); i = j + 1) {
        for (j = i; j < blk_index->size() - 1; ++j) {
            if (blk_index->at(j) + 1 !=  blk_index->at(j + 1)) {
                break;
            }
        }
        scoped_refptr<IORequest> io = new IORequest((uint64_t)blk_index->at(i) * BLOCK_SIZE,
                (uint64_t)(j - i + 1) * BLOCK_SIZE, (char *)((uint64_t)(buf) + (uint64_t)(i) * BLOCK_SIZE));
        data_requests->push_back(io);
    }
}

static int handle_one_submitted_queuepair(io_context_t ctx_id, SequenceAndQpair* iter,
        long nr, struct io_event *events, bool is_submitted_queue)
{
    int done_cnt = 0;
    //while (!iter->submitted_sequences.empty() || done_cnt < nr) {
    //while ((!iter->submitted_sequences.empty()) && done_cnt < nr) {
    //while ((!iter->submitted_sequences.empty()) && done_cnt < nr) {
    while ((!iter->submitted_sequences.empty())) {
       //int ret = spdk_nvme_qpair_process_completions(iter->qpair, nr - done_cnt);
       int ret = spdk_nvme_qpair_process_completions(iter->qpair, 0);
       //int ret = spdk_nvme_qpair_process_completions(iter->qpair, nr);
       if (ret < 0) { /* error occures ?, redo the operation at the end */
           //LOG(ERROR) << "spdk_nvme_qpair_process_completions error: " << ret;
           break;
           //continue;
       } else if (ret == 0) { /* error occures ?, redo the operation at the end */
           continue;
       } else {
           for (int i = 0; i < ret; ++i) {
               spdk_nvme_sequence * sequence = iter->processed_sequences.front();
               if (events) {
                   events[done_cnt + i].obj = sequence->event.obj;
                   events[done_cnt + i].res = sequence->event.res;
                   events[done_cnt + i].res2 = sequence->event.res2;
                   io_callback_t cb = (io_callback_t)events[done_cnt + i].data;
                   /* then call libaio's callback */
                   if (cb) {
                       struct iocb *iocb = sequence->event.obj;
                       cb(ctx_id, iocb, sequence->event.res, sequence->event.res2);
                   }
               }
               iter->processed_sequences.pop_front();
               iter->free_sequences.push_back(sequence);
           }
           done_cnt += ret;
           if (iter->processed_sequences.empty() && iter->submitted_sequences.empty()) {
                //g_sequenceqpair_submitted.pop_back();
                //g_qpair_owner_map.erase(*(int*)ctx_id);
                //g_sequenceqpair_free.push_front(iter);
               //g_sequenceqpair_submitted.pop_front();
               //g_sequenceqpair_free.push_back(iter);
               break;
           }
       }
    }
    return done_cnt;
}
/* only get events for ctx_id (fd) for one file */
int nvme_aio_getevents(io_context_t ctx_id, long min_nr, long nr, struct io_event *events,
        struct timespec *timeout)
{
    int ret, done_cnt = 0;
    struct timespec cur_time, due_time;
    cur_time.tv_nsec = due_time.tv_nsec = 0;
    if (timeout) {
        //timespec cur_time, due_time;
        clock_gettime(CLOCK_REALTIME, &cur_time);
        due_time.tv_nsec = cur_time.tv_nsec + timeout->tv_nsec;
    }
    SequenceAndQpair* iter = NULL;
#ifdef MULI
    auto owned_iter = g_qpair_owner_map.find(*(int *)ctx_id);
    if (owned_iter == g_qpair_owner_map.end()) {
        //LOG(ERROR) << "Fail to find the fd:" << *(int *)ctx_id;
        iter = g_sequenceqpair_submitted.front();
        //return -5;
    }
#else
    iter = g_sequenceqpair_free.front();
#endif
    ret = handle_one_submitted_queuepair(ctx_id, iter, nr, events + done_cnt, false);
    if (ret > 0) {
        struct io_event *temp_events = events + done_cnt;
        if (temp_events->obj->aio_fildes != *(int *)ctx_id) {
            LOG(ERROR) << "fd doesn't match, handled: "  << *(int *)ctx_id << "expected: " << temp_events->obj->aio_fildes;
        }
        done_cnt += ret;
        return done_cnt;
    }
    if (timeout) {
        clock_gettime(CLOCK_REALTIME, &cur_time);
        if (cur_time.tv_nsec >= due_time.tv_nsec) {
            return done_cnt;
        }
    }
    return done_cnt;
}
