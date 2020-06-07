/***************************************************************************
 * 
 * Copyright (c) 2019 Haha.com, Inc. All Rights Reserved
 * 
 **************************************************************************/
/**
 * @file io_abstraction.h
 * @author xiaqichao(com@Haha.com)
 * @date 2019/02/26 18:18:56
 * @brief 
 *  
 **/
#ifndef  __APPEND_DEMO_H_
#define __APPEND_DEMO_H_

#include <cstdlib>
#include <stdint.h>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <cstring>
#include <map>                      // std::map
#include <vector>                   // std::vector
#include <set>                      // std::set
#include <string>                   // std::string
#include <algorithm>
#include <errno.h>
#include <climits>

#include <base/memory/singleton.h>
#include <base/containers/doubly_buffered_data.h>
#include <base/memory/ref_counted.h>
//#include "common/libaio.h"
#include <netinet/in.h>
#include <gflags/gflags.h>
#include <base/logging.h>           // CHECK
#include "common/types.h"
#include "common/lockable.h"        // Lockable
#include "layout.h"

namespace bce {
namespace cds {
// demo API laywer

// layout laywer

// IO abstract layer
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

typedef std::vector<scoped_refptr<bce::cds::IORequest>> DemoIovector;

// per disk
class IOWoman  : public bce::cds::Lockable { 
public:
    IOWoman(){};
    virtual ~IOWoman(){};
    // main IO functions
    virtual int myopen(const char * name) = 0; // init device and IO hander
    virtual int single_read(uint64_t offset, uint32_t len, char *buf) = 0;
    virtual int single_write(uint64_t offset, uint32_t len, char *buf) = 0;
    virtual int batch_read(DemoIovector * vec) = 0;
    virtual int batch_write(DemoIovector * vec) = 0;
    virtual int mysync() = 0;
    virtual int myclose() = 0;
    virtual int discard_disk() = 0;
    virtual int init(const char * dev_name) = 0; // similar as mount
    virtual int deinit(const char * dev_name) = 0;  // similar as umount
    //virtual int format(const char * dev_name) = 0; // similar as mkfs.f2fs
    virtual char * mem_alloc(uint32_t) = 0; // allocate buf for IO
    virtual int mem_free(char *) = 0; // free buf for IO
    virtual void merge_io_vectors(std::vector<uint32_t> * blk_index,
            char * buf, DemoIovector *data_requests) = 0;
};

class FuseIOWoman : public IOWoman {
public:
#if 0
    static FuseIOWoman* GetInstance() {
        return Singleton<FuseIOWoman >::get();
    }
#endif
    FuseIOWoman(){};
    virtual ~FuseIOWoman(){};
    // main IO functions
    virtual int myopen(const char * name) {  std::cout << "open " << name <<  "done" << std::endl; return 0;} 
    virtual int single_read(uint64_t offset, uint32_t len, char *buf) {std::cout << "single read done" << std::endl; return 0;}
    virtual int single_write(uint64_t offset, uint32_t len, char *buf) {std::cout << "single write done" << std::endl; return 0;}
    virtual int batch_read(DemoIovector * vec) {std::cout << "batch read done" << std::endl; return 0;}
    virtual int batch_write(DemoIovector * vec) {std::cout << "batch write done" << std::endl; return 0;}
    virtual int mysync() {std::cout << "sync done" << std::endl; return 0;}
    virtual int myclose() {std::cout << "close done" << std::endl;return 0;}
    virtual int discard_disk() {std::cout << "discard disk done" << std::endl;return 0;}
    virtual int init(const char * dev_name) {std::cout << "init done" << std::endl;return 0;}
    virtual int deinit(const char * dev_name) {std::cout << "deinit done" << std::endl;return 0;}
    virtual int format(const char * dev_name) {std::cout << "format done" << std::endl;return 0;}
    char * mem_alloc(uint32_t sz) {return (char *)(malloc(sz));}
    virtual int mem_free(char * buf) {free(buf); return 0;}
    virtual void merge_io_vectors(std::vector<uint32_t> * blk_index, char * buf,
            DemoIovector *data_requests) {
        for (size_t i = 0; i < blk_index->size(); ++i) {
            scoped_refptr<IORequest> io = new IORequest((uint64_t)blk_index->at(i) * BLOCK_SIZE, BLOCK_SIZE,
                    (char *)((uint64_t)(buf) + i * BLOCK_SIZE));
            data_requests->push_back(io);
        }
    }
};

}
}
#endif  //__APPEND_DEMO_H_

/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */
