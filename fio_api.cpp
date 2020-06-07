/***************************************************************************
 * 
 * Copyright (c) 2019 Haha.com, Inc. All Rights Reserved * 
 **************************************************************************/
/**
 * @file append_demo.cpp
 * @author xiaqichao(com@Haha.com)
 * @date 2019/02/26 18:19:39
 * @brief 
 *  
 **/
#include <cstring>
#include <map>
#include "common/super_fast_hash.h"
#include "common/utils.h"           // parse_message_from_iobuf
#include "common/rpc_call.h"
//#include "append-demo/layout.h"
#include "append_api.h"
#include "layout.h"
#include "append-demo/append_api.h"
#include "append-demo/spdk_nvme_io.h"

#ifdef __cplusplus
extern "C"
{
#endif

// API impletion layer
void demo_init() {
    /* init spdk device */
    init(NULL);
    g_demofs_manager->fs_format();
    g_demofs_manager->fs_mount();
}

int demo_open(const char * filename) {
    return g_demofs_manager->fs_myopen(filename); 
}

int demo_write(int fd, uint64_t off, uint32_t len, char * buf) {
    return g_demofs_manager->fs_write(fd, off, len ,buf); 
}

int demo_read(int fd, uint64_t off, uint32_t len, char * buf) {
    return g_demofs_manager->fs_read(fd, off, len ,buf); 
}

#if 0
int demo_sync(int fd) {
    return g_demofs_manager->fs_mysync(fd); 
}
#endif

int demo_close(int fd) {
    return g_demofs_manager->fs_myclose(fd); 
}

int demo_deinit(int fd) {
    return deinit(NULL);
}

char * demo_mem_alloc(uint32_t sz)
{
    return mem_alloc(sz);
}

int demo_mem_free(char * buf)
{
    return mem_free(buf);
}

uint64_t demo_get_file_size(int fd)
{
    return g_demofs_manager->fs_get_file_size(fd);
}

int demo_trim(int fd, uint64_t off, uint32_t len)
{
    // TBD later
    return len;
}

int demo_sync(int fd, uint64_t off, uint32_t len)
{
    // not required
    return len;
}

int cds_io_submit(io_context_t ctx, long nr, struct iocb *ios[])
{
    //iocb *ios[] = (iocb *[])ios_arg;
    return g_demofs_manager->io_submit(ctx, nr, ios);
}

int cds_io_getevents(io_context_t ctx_id, long min_nr, long nr, 
        struct io_event *events, struct timespec *timeout)
{
    return g_demofs_manager->io_getevents(ctx_id, min_nr, nr, events, timeout);
}

#ifdef __cplusplus
}
#endif
