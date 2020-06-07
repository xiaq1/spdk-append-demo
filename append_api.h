/***************************************************************************
 * 
 * Copyright (c) 2019 Haha.com, Inc. All Rights Reserved
 * 
 **************************************************************************/
/**
 * @file append_demo.h
 * @author xiaqichao(com@Haha.com)
 * @date 2019/02/26 18:34:26
 * @brief 
 *  
 **/
#ifndef  __APPEND_DEMO_H_
#define  __APPEND_DEMO_H_

#include <map>                      // std::map
//#include <pair>                      // std::pair
#include <set>                      // std::set
#include <queue>                    // std::queue and priority_queue
#include <string>                   // std::string
#include <cfloat>                   // std::float
#include <algorithm>
//#include <base/endpoint.h>          // base::EndPoint
#include <base/memory/singleton.h>  // Singleton
#include <bthread/mutex.h>          // CDSMutex
#include <time.h>
#include "common/lockable.h"        // Lockable
#include "append-demo/libaio.h"
#include "layout.h"

namespace bce {
namespace cds {

typedef std::map<uint32_t, scoped_refptr<AddressTranslator> > InoAdressTranslatorMap; 
typedef std::pair<uint32_t, scoped_refptr<AddressTranslator> > InoAdressTranslatorPair; 

class DemoFs : public Lockable {
public:
    static DemoFs* GetInstance() {
        return Singleton<DemoFs >::get();
    }
    DemoFs();
    ~DemoFs();

    void fs_mount();
    int fs_format();
    int fs_gc();
    int fs_myopen(const char * filename);
    int fs_write(int fd, uint64_t off, uint32_t len, char * buf);
    int fs_read(int fd, uint64_t off, uint32_t len, char * buf);
    int fs_mysync(int fd);
    int fs_myclose(int fd);
    uint64_t fs_get_file_size(int fd) const;
    int io_submit(io_context_t ctx, long nr, iocb* ios[]);
    int io_getevents(io_context_t ctx_id, long min_nr, long nr, io_event * events, timespec * timeout);
public:
    bce::cds::InoAdressTranslatorMap myopen_files;
    scoped_refptr<bce::cds::SuperBlockManager> g_sb_manager;
    scoped_refptr<bce::cds::NATManager> g_nat_manager;
    scoped_refptr<bce::cds::SITManager> g_sit_manager;
    scoped_refptr<bce::cds::BlockAllocator> g_sit_allocator;
    scoped_refptr<bce::cds::BlockAllocator> g_nat_allocator;
    scoped_refptr<bce::cds::BlockAllocator> g_data_allocator;
    scoped_refptr<bce::cds::BlockAllocator> g_node_allocator;
};

#define g_demofs_manager bce::cds::DemoFs::GetInstance()

}
}
#endif  //__APPEND_DEMO_H_

/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */
