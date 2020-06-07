/***************************************************************************
 * 
 * Copyright (c) 2019 Haha.com, Inc. All Rights Reserved
 * 
 **************************************************************************/
/**
 * @file spdk_nvme_io.h
 * @author xiaqichao(com@Haha.com)
 * @date 2019/03/14 18:52:24
 * @brief 
 *  
 **/
#ifndef __SPDK_NVME_IO_H_  //__SPDK_NVME_IO_H_
#define __SPDK_NVME_IO_H_

#ifdef USE_CMB
#undef USE_CMB
#endif

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <vector>
#include <list>
#include <algorithm>
#include <map>

//#include "common/libaio.h"
#include "append-demo/libaio.h"
#include "append-demo/io_abstraction.h"
#include "io_abstraction.h"
//#include "spdk/stdinc.h"
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



//#include <base/memory/singleton.h>
//#include <base/containers/doubly_buffered_data.h>
//#include <base/memory/ref_counted.h>
//#include "common/libaio.h"
//#include <netinet/in.h>
//#include <gflags/gflags.h>
//#include <base/logging.h>           // CHECK
//#include "common/types.h"
#include "common/lockable.h"        // Lockable
//#include "layout.h"

//namespace bce {
//namespace cds {
// IO abstract layer

#define MAX_QPAIR 128
#define DEFAULT_QUEUE_DEPTH 256 // best performance
typedef void (*IOCb)(void * cb_args);

class IORequest : public bce::cds::Lockable {
public:
    IORequest(uint64_t off, uint32_t l, char * data) :
        offset(off),
        len(l),
        buf(data){};
    ~IORequest() {};
    uint64_t offset; // offset from begining of disk or file on byte
    uint32_t len;
    char * buf; // buf data to read or write
};

typedef std::vector<scoped_refptr<IORequest>> DemoIovector;

struct ctrlr_entry {
    struct spdk_nvme_ctrlr  *ctrlr;
    struct ctrlr_entry      *next;
    char                    name[1024];
};

struct ns_entry {
    struct spdk_nvme_ctrlr  *ctrlr;
    struct spdk_nvme_ns     *ns;
    struct ns_entry         *next;
};

class SequenceAndQpair;
struct spdk_nvme_sequence {
    struct ns_entry *ns_entry;
    char            *buf;
    size_t          sz; 
    int            is_completed;
#ifdef USE_CMB
    unsigned        using_cmb_io;
#endif
    IOCb            io_cb;
    void *          cb_args;
    struct  io_event event;
    io_context_t ctx;
    SequenceAndQpair*   sequenceqpair;
};

class SequenceAndQpair {
public:
    SequenceAndQpair(struct spdk_nvme_ctrlr  *ctrlr, int depth) {
        qpair = spdk_nvme_ctrlr_alloc_io_qpair(ctrlr, NULL, 0);
        for (int i = 0; i < depth; ++i) {
            spdk_nvme_sequence * sequence = (spdk_nvme_sequence *)malloc(sizeof(struct spdk_nvme_sequence));
            if (sequence) {
                //sequence->ns_entry = ctrlr;
                sequence->event.res = 0;
                sequence->event.res2 = 0;
                sequence->sequenceqpair = this;
                free_sequences.push_back(sequence);
            }
        }
    }
    SequenceAndQpair(struct spdk_nvme_ctrlr  *ctrlr) : SequenceAndQpair(ctrlr, DEFAULT_QUEUE_DEPTH) {}
    ~SequenceAndQpair() { 
        if (qpair) {
            spdk_nvme_ctrlr_free_io_qpair(qpair);
            qpair = NULL;
        }

        while (!free_sequences.empty()) {
            spdk_nvme_sequence * sequence = free_sequences.front();
            if (sequence) {
                free (sequence);
                sequence = NULL;
            }
            free_sequences.pop_front();
        }
        while (!submitted_sequences.empty()) {
            spdk_nvme_sequence * sequence = submitted_sequences.front();
            if (sequence) {
                free (sequence);
                sequence = NULL;
            }
            submitted_sequences.pop_front();
        }
        while (!processed_sequences.empty()) {
            spdk_nvme_sequence * sequence = processed_sequences.front();
            if (sequence) {
                free(sequence);
                sequence = NULL;
            }
            //free (sequence);
            processed_sequences.pop_front();
        }
    }
    struct spdk_nvme_qpair* qpair;
    std::list<spdk_nvme_sequence* > submitted_sequences;
    std::list<spdk_nvme_sequence* > processed_sequences;
    std::list<spdk_nvme_sequence* > free_sequences;
};

typedef std::map<char *, struct spdk_nvme_sequence* > BufferSequenceMap;
typedef std::pair<char *, struct spdk_nvme_sequence* > BufferSequencePair;
typedef std::list<SequenceAndQpair* > SequenceAndQpairList;
typedef std::list<SequenceAndQpair* > SequenceAndQpairList;
typedef std::pair<int, SequenceAndQpair*> QpairOwnerPair;
typedef std::map<int, SequenceAndQpair* > QpairOwnerMap;

    // main IO functions
    int myopen(const char * name);
    int single_read(int fd, uint64_t offset, uint32_t len, char *buf);
    int single_write(int fd, uint64_t offset, uint32_t len, char *buf);
    int batch_read(int fd, DemoIovector *);
    int batch_write(int fd, DemoIovector *);
    int nume_aio_submit(DemoIovector *, IOCb, void *);
    int nume_io_getevents(DemoIovector *, IOCb, void *);
    int mysync();
    int myclose();
    int discard_disk();
    int init(const char * dev_name);
    int deinit(const char * dev_name);
    int format(const char * dev_name);
    char * mem_alloc(uint32_t sz);
    int mem_free(char * buf);
    void register_ns(struct spdk_nvme_ctrlr *ctrlr, struct spdk_nvme_ns *ns);
    //void io_complete(void *arg, const struct spdk_nvme_cpl *completion);
    //bool probe_cb(void *cb_ctx, const struct spdk_nvme_transport_id *trid,
    //struct spdk_nvme_ctrlr_opts *opts);
    //void attach_cb(void *cb_ctx, const struct spdk_nvme_transport_id *trid,
    //struct spdk_nvme_ctrlr *ctrlr, const struct spdk_nvme_ctrlr_opts *opts);
    void cleanup(void);
    int spdk_single_dio(int fd, uint64_t offset, uint32_t length, char *buf, uint8_t op);
    void merge_io_vectors(std::vector<uint32_t> * blk_index, char * buf, DemoIovector *data_requests);
    int nvme_aio_submit(io_context_t ctx, iocb * ioc, IOCb io_cb, void * cb_args);
    int nvme_aio_getevents(io_context_t ctx_id, long min_nr, long nr, io_event *events, struct timespec *timeout); 
//private:
    //SpdkNvmeIOWoman(){};
    //virtual ~SpdkNvmeIOWoman(){};
    extern struct ctrlr_entry *g_controllers;
    extern struct ns_entry *g_namespaces;
#ifdef USE_CMB
    extern BufferSequenceMap g_sequence_buf_map;
#endif
    //friend struct DefaultSingletonTraits<SpdkNvmeIOWoman>;
//};

//#define g_io_worker  SpdkNvmeIOWoman::GetInstance()

//}
//}
#endif  //__SPDK_NVME_IO_H_

/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */
