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
//#include "append-demo/io_abstraction.h"

namespace bce {
namespace cds {

/*
 * alloc and init sub memeber of DemoFs
 */
DemoFs::DemoFs() {
    g_sb_manager = NULL;
    g_nat_manager = NULL;
    g_sit_manager = NULL;
    g_sit_allocator = NULL;
    g_nat_allocator = NULL;
    g_data_allocator = NULL;
    g_node_allocator = NULL;
}


/*
 * init and mount all meta data on disk
 */
void DemoFs::fs_mount() {
    if (!g_sb_manager) {
        g_sb_manager = new bce::cds::SuperBlockManager();
    }
    g_sb_manager->load();
    if (!g_nat_manager) {
        g_nat_manager = new bce::cds::NATManager(g_sb_manager->get_inode_total());
    }
    g_nat_manager->load();
    if (!g_sit_manager) {
        g_sit_manager = new bce::cds::SITManager(g_sb_manager->get_sit_total());
    }
    g_sit_manager->load();
    if (!g_sit_allocator) {
        g_sit_allocator = new bce::cds::BlockAllocator(g_sb_manager->sit_blkaddr(), g_sb_manager->sit_next_blkaddr());
    }
    if (!g_nat_allocator) {
        g_nat_allocator = new bce::cds::BlockAllocator(g_sb_manager->nat_blkaddr(), g_sb_manager->nat_next_blkaddr());
    }
    if (!g_data_allocator) {
        g_data_allocator = new bce::cds::BlockAllocator(g_sb_manager->data_blkaddr(), g_sb_manager->data_next_blkaddr());
    }
    if (!g_node_allocator) {
        g_node_allocator = new bce::cds::BlockAllocator(g_sb_manager->node_blkaddr(), g_sb_manager->node_next_blkaddr());
    }
}

DemoFs::~DemoFs() {
    delete g_node_allocator;
    delete g_data_allocator;
    delete g_nat_allocator;
    delete g_sit_allocator;
    delete g_sit_manager;
    delete g_nat_manager;
    delete g_sb_manager;
}
/*
 * Called only when format and re-foramt disk content before do any demofs operation on disk
 */
int DemoFs::fs_format() {
    if (!g_sb_manager) {
        g_sb_manager = new bce::cds::SuperBlockManager();
    }
    g_sb_manager->format();
    //g_nat_manager = new bce::cds::NATManager( g_sb_manager->get_inode_total());
    //g_nat_manager->format();
    //g_sit_manager = new bce::cds::SITManager( g_sb_manager->get_inode_total());
    if (!g_sit_allocator) {
        g_sit_allocator = new bce::cds::BlockAllocator(g_sb_manager->sit_blkaddr(), g_sb_manager->sit_next_blkaddr());
    }
    if (!g_sit_manager) {
        g_sit_manager = new bce::cds::SITManager(g_sb_manager->get_sit_total());
    }
    g_sit_manager->format();
    return 0;
}

/*
 * TBD later:  cost-best algorithm first, then greddy algorithm, and it
 * should be placed on a standalone thread
 */
int DemoFs::fs_gc() {
    return 0;
}

/*
 * Open a inode according to the name  TODO: demo desn't check name conflict
 * Notice: this fucntion will alloc a f2fs_inode structure
 */
int DemoFs::fs_myopen(const char * filename) {
    uint32_t ino = detail::SuperFastHash(filename, strlen(filename));
    uint32_t blkno = 0;
    bce::cds::f2fs_inode *inode = nullptr;
    //scoped_refptr<f2fs_inode *> inode = nullptr;
    if(!g_nat_manager->lookup_block_by_ino(ino, &blkno)) {
        // allocate block for inode block
        uint32_t blkno = g_node_allocator->alloc_free_blk();
        LOG(INFO) << "Node allocator allock blkno: " << blkno << " for new opening file";
        g_nat_manager->add_nat_entry(ino, blkno);
        // creatr an inode at the "blkno" block address, notice: still in memroy at this moment
        inode = g_node_manager->create_inode(blkno, ino, filename);
        g_node_manager->write_inode(blkno, inode);
        g_sit_manager->update_sit_map(blkno, USED);
    } else { // succes to lookup the inode and then read out its index node info
        inode = g_node_manager->read_inode(blkno);
    }
    // dont't record myopened inode in demo
    //myopen_files.insert(InoAdressTranslatorPair(ino, (scoped_refptr<AddressTranslator>)new AddressTranslator(inode, g_node_allocator)));
    myopen_files.insert(InoAdressTranslatorPair(ino, (scoped_refptr<AddressTranslator>)new AddressTranslator(inode, g_node_allocator)));
    return ino;
}

struct AsyncWriteCbArgs {
public:
    AsyncWriteCbArgs(scoped_refptr<AddressTranslator> trans,
            scoped_refptr<bce::cds::SITManager> sit_manager,
            uint64_t offset) :
            _trans(trans),
            _sit_manager(sit_manager),
            _offset(offset)
    {
    };

    std::vector<uint32_t> _blk_addrs;
    scoped_refptr<AddressTranslator> _trans;
    scoped_refptr<bce::cds::SITManager> _sit_manager;
    uint64_t _offset;
};

void async_write_cb(void * params)
{
    AsyncWriteCbArgs* args = (AsyncWriteCbArgs *)params;
    for (size_t i = 0; i < args->_blk_addrs.size(); i++) {
        // the update_node_addr should handle both append and over-ride wirte
        uint32_t original_blk_addrs = args->_trans->update_node_entry(args->_blk_addrs.at(i),
                args->_offset / BLOCK_SIZE  + i);
        // update SIT entry
        if (NOT_FOUND != original_blk_addrs) {
            args->_sit_manager->update_sit_map(original_blk_addrs , FREE);
        }
        args->_sit_manager->update_sit_map(args->_blk_addrs.at(i), USED);
    }
    delete args;
}

/*
 * As a demo, write data to some location of the data area, and update node/metadata area
 * accordingly
 * FIXME: in demo, assume offset and len are 4K/BLOCK_SIZE alined
 * TODO: now we in-place update direct node's adress info 
 */
int DemoFs::fs_write(int fd, uint64_t offset, uint32_t len, char * buf)
{
    InoAdressTranslatorMap::iterator it = myopen_files.find(fd);
    if (it == myopen_files.end()) {
        LOG(ERROR) << "write: fail to find fd:" << fd;
        return  -1;
    }
    scoped_refptr<AddressTranslator> trans = it->second;

    std::vector<uint32_t> blk_addrs;
    // TODO: the allocatd blocks may not be continuous
    uint32_t start_blk = g_data_allocator->alloc_free_blk(len / BLOCK_SIZE);
#ifdef TRACE_ALL_IO
    LOG(INFO) << "Data Allocator alloc start_blk: " << start_blk << " blkcnt: " << len / BLOCK_SIZE;
#endif
    for (uint32_t i = 0; i < len / BLOCK_SIZE; ++i) {
        blk_addrs.push_back(start_blk + i);
    }
    // generate write request to data
    // TODO: write maybe merged here to turn out to be large write
    DemoIovector data_requests;
    //std::vector<uint32_t> request_blks;
    // merge continuous block number as a larger regions
    // in demo, we can assume they always be continuous address
    merge_io_vectors(&blk_addrs, buf, &data_requests);
    int length = batch_write(fd, &data_requests);
    // TODO: should set the old block for data to DIRTY state if it includs covered writting
    // but bit can't support bit
    for (size_t i = 0; i < blk_addrs.size(); i++) {
        // the update_node_addr should handle both append and over-ride wirte
        uint32_t original_blk_addrs = trans->update_node_entry(blk_addrs.at(i), offset / BLOCK_SIZE  + i);
        // update SIT entry
        if (NOT_FOUND != original_blk_addrs) {
            g_sit_manager->update_sit_map(original_blk_addrs , FREE);
        }
        g_sit_manager->update_sit_map(blk_addrs.at(i), USED);
    }
#if 0
    /* in-place update doesn't need update node/indirect node  */
#endif
    uint64_t current_size = trans->i_size.load(boost::memory_order_consume);
    if (offset + length  > current_size) {
        trans->i_size.fetch_add(offset + length - current_size, boost::memory_order_release);
        trans->inode->i_size = trans->i_size.load(boost::memory_order_consume); 
    }
    return length;
}

/*
 * As a demo, write data to some location of the data area, and update node/metadata area
 * accordingly
 */
int DemoFs::fs_read(int fd, uint64_t offset, uint32_t len, char * buf) {
    InoAdressTranslatorMap::iterator it = myopen_files.find(fd);
    if (it == myopen_files.end()) {
        LOG(ERROR) << "read: fail to find fd:" << fd;
        return  -1;
    }
    // check file size first
    bce::cds::f2fs_inode * inode = it->second->inode;
    if (offset + len > inode->i_size) {
        LOG(ERROR) << "read: read range exceed file size:" << fd;
        return  -2;
    }
    scoped_refptr<AddressTranslator> trans = it->second;
    // demo will simplify the check whether offset and length beyond the valid limit and len and
    // assume offset and len are both BLOCK_SIZE align
    std::vector<uint32_t> blk_addrs;
    for (size_t i = 0; i < len / BLOCK_SIZE; ++i) {
        uint32_t start_blk = trans->lookup_node_entry(offset / BLOCK_SIZE + i);
        //CHECK(start_blk != NOT_FOUND);
        if (start_blk != NOT_FOUND) {
            blk_addrs.push_back(start_blk);
        } else {
            LOG(ERROR) << "Read fail to find the " << (offset / BLOCK_SIZE + i) << " logical block"; 
            return -3;
        }
    }
    //g_data_allocator->get_direct_blks(inode, offset, len, &blk_addrs);
    DemoIovector read_data_requests;
    merge_io_vectors(&blk_addrs, buf, &read_data_requests);
    return  batch_read(fd, &read_data_requests);
}

/*
 * Close myopened file and free all memory allocated for its inode/dnode/indirect node and so on
 */
int DemoFs::fs_myclose(int fd) {
    InoAdressTranslatorMap::iterator it = myopen_files.find(fd);
    if (it == myopen_files.end()) {
        LOG(ERROR) << "read: fail to find fd:" << fd;
        return  -1;
    }
    //bce::cds::f2fs_inode * inode = it->second->inode;
    scoped_refptr<AddressTranslator> trans = it->second;
    // demo will simplify the check whether offset and length beyond the valid limit
    //std::vector<uint32_t> blk_addrs;
    // FIX me: the update of indoe should move ahead
    single_write(fd, (uint64_t)(trans->inode->i_blkno) * BLOCK_SIZE, BLOCK_SIZE, (char *)(trans->inode));
    trans->inode->i_size = trans->i_size;
    g_node_manager->delete_inode(trans->inode);
    // auto ptr will free mem space for AddressTranslator by itself
    return 0;
}

uint64_t DemoFs::fs_get_file_size(int fd) const {
    InoAdressTranslatorMap::const_iterator it = myopen_files.find(fd);
    if (it == myopen_files.end()) {
        LOG(ERROR) << "read: fail to find fd:" << fd;
        return  0;
        //return  -1;
    }
    scoped_refptr<AddressTranslator> trans = it->second;
    return trans->i_size.load(boost::memory_order_relaxed);
}

int DemoFs::io_submit(io_context_t ctx, long nr, iocb* ios[])
{
    int length = 0;
    int done_cnt = 0;
    //bce::cds::iocb ** ios = ios_args;
    int fd = *(int*)ctx;
    for (long req_index = 0; req_index < nr; ++req_index) {
        //int fd = ios[req_index]->aio_fildes;
        InoAdressTranslatorMap::iterator it = myopen_files.find(fd);
        if (it == myopen_files.end()) {
            LOG(ERROR) << "write: fail to find fd:" << fd;
            return  -1;
        }
        uint32_t len = ios[req_index]->u.c.nbytes;
        uint32_t offset = ios[req_index]->u.c.offset;
        //char * buf = (char *)ios[req_index]->u.c.buf;
        scoped_refptr<AddressTranslator> trans = it->second;

        // TODO: the allocatd blocks may not be continuous
        AsyncWriteCbArgs* args = new AsyncWriteCbArgs(trans, g_sit_manager, offset);
        uint32_t start_blk = g_data_allocator->alloc_free_blk(len / BLOCK_SIZE);
        for (uint32_t i = 0; i < len / BLOCK_SIZE; ++i) {
            args->_blk_addrs.push_back(start_blk + i);
        }
        // generate write request to data
        // TODO: write maybe merged here to turn out to be large write
        //DemoIovector data_requests;
        //std::vector<uint32_t> request_blks;
        // merge continuous block number as a larger regions
        // in demo, we can assume they always be continuous address
        //merge_io_vectors(args->blk_addrs, buf, &data_requests);
        // differs from direct_IO, we just submit one IO request to simplify the implementation
        int ret = nvme_aio_submit((io_context_t )&fd, ios[req_index], async_write_cb, (void *)args);
        if (ret > 0) {
            length = ret; 
            ++done_cnt;
            if (ios[req_index]->aio_lio_opcode == IO_CMD_PWRITE) {
                uint64_t current_size = trans->i_size.load(boost::memory_order_consume);
                if (offset + length  > current_size) {
                    trans->i_size.fetch_add(offset + length - current_size, boost::memory_order_release);
                    trans->inode->i_size = trans->i_size.load(boost::memory_order_consume);
                }
            }
        }
    }
    return done_cnt;
}

int DemoFs::io_getevents(io_context_t ctx_id, long min_nr, long nr, io_event * events,
        timespec * timeout)
{
    return nvme_aio_getevents(ctx_id, min_nr, nr, events, timeout);
}

}
}
/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */
