/***************************************************************************
 * 
 * Copyright (c) 2019 Haha.com, Inc. All Rights Reserved
 * 
 **************************************************************************/
/**
 * @file FIO_API.h
 * @author xiaqichao(com@Haha.com)
 * @date 2019/02/26 18:34:26
 * @brief 
 *  
 **/
#ifndef  __FIO_API_H_
#define  __FIO_API_H_

#include <time.h>
//#include "common/libaio.h"
#include "append-demo/libaio.h"

#ifdef __cplusplus
extern "C"
{
#endif

int demo_init(void);
int demo_open(const char * filename);
int demo_write(int fd, uint64_t off, uint32_t len, char * buf);
int demo_read(int fd, uint64_t off, uint32_t len, char * buf);
int demo_close(int fd);
int demo_deinit(void);
char * demo_mem_alloc(uint32_t sz);
int demo_mem_free(char * buf);
uint64_t demo_get_file_size(int fd);
int demo_trim(int fd, uint64_t off, uint32_t len);
int demo_sync(int fd, uint64_t off, uint32_t len);
int cds_io_submit(io_context_t ctx, long nr, struct iocb *ios[]);
int cds_io_getevents(io_context_t ctx_id, long min_nr, long nr, 
        struct io_event *events, struct timespec *timeout);
#ifdef __cplusplus
}
#endif

#endif
