/***************************************************************************
 * 
 * Copyright (c) 2019 Haha.com, Inc. All Rights Reserved
 * 
 **************************************************************************/
/**
 * @file layout.cpp
 * @author xiaqichao(com@Haha.com)
 * @date 2019/02/27 09:36:13
 * @brief 
 *  Manager of kinds of meta data *
 **/
#include <string.h>
#include <map>                      // std::map
#include <set>                      // std::set
#include <queue>                    // std::queue and priority_queue
#include <string>                   // std::string
#include <cfloat>                   // std::float
#include <algorithm>
#include <map>
//#include "io_abstraction.h"
#include "common/utils.h"           // parse_message_from_iobuf
#include "common/rpc_call.h"
#include "spdk_nvme_io.h"
#include "layout.h"
#include "append-demo/spdk_nvme_io.h"
#include "append-demo/layout.h"

namespace bce {
namespace cds {

// malloc memroy for sb
SuperBlockManager::SuperBlockManager() {
    sz = sizeof(f2fs_super_block) < BLOCK_SIZE ?
        BLOCK_SIZE : (sizeof(f2fs_super_block) / BLOCK_SIZE + 1) * BLOCK_SIZE;
    char * buf = mem_alloc(sz);
    sb = (f2fs_super_block *)(buf);
}

SuperBlockManager::~SuperBlockManager() {
    if (sb) {
        mem_free((char *)(sb));
        sb = nullptr;
    }
}

int SuperBlockManager::load() { 
    int ret = single_read(-1, (uint64_t)SB_BASE, sz, (char *)(sb));
    if (ret < (int)sz) {
        LOG(ERROR) << "Fail to load SuperBlock";
        return -1;
    }
    return ret;
}

// save SuperBlock to Disk
// question: whether should alloc another buf to do memory write again? no
int SuperBlockManager::save() { 
    int ret = single_write(-1, (uint64_t)SB_BASE, sz, (char *)(sb));
    if (ret < (int)sz) {
        LOG(ERROR) << "Fail to save SuperBlock";
    }
    return ret;
}

// init in-memory sb (super block)
int SuperBlockManager::init_super_block() {
        //memset((char *)(sb), 0xa5, sizeof(sb)); // use 0xa5 to mark un-initialized field
        memset((char *)(sb), 0x00, sizeof(SuperBlockManager)); // use 0xa5 to mark un-initialized field
        sb->log_blocksize = 12; // 4K defaut
        sb->log_segmentsize = 21; // 2M still
        sb->block_count = DATA_SIZE >> sb->log_blocksize;
        sb->segment_count = DATA_SIZE >> sb->log_segmentsize;
        sb->segment_count_ckpt = CP_SIZE >> sb->log_segmentsize; 
        sb->segment_count_sit = SIT_SIZE >> sb->log_segmentsize;
        sb->segment_count_nat = NAT_SIZE  >> sb->log_segmentsize;
        sb->segment_count_ssa = SSA_SIZE  >> sb->log_segmentsize;
        sb->segment_count_node = NODE_SIZE >> sb->log_segmentsize;
        sb->segment_count_data = DATA_SIZE >> sb->log_segmentsize;
        sb->segment0_blkaddr = NODE_BASE >> sb->log_segmentsize;
        // start address of all partition
        sb->cp_next_blkaddr =   sb->cp_blkaddr =   CP_BASE >>   sb->log_blocksize;
        sb->sit_next_blkaddr =  sb->sit_blkaddr =  SIT_BASE >>  sb->log_blocksize;
        sb->nat_next_blkaddr =  sb->nat_blkaddr =  NAT_BASE >>  sb->log_blocksize;
        sb->ssa_next_blkaddr =  sb->ssa_blkaddr =  SSA_BASE >>  sb->log_blocksize;
        sb->node_next_blkaddr = sb->node_blkaddr = NODE_BASE >> sb->log_blocksize;
        sb->data_next_blkaddr = sb->data_blkaddr = DATA_BASE >> sb->log_blocksize;
        //save(&sb);
        return 0;
}

int SuperBlockManager::format() {
    discard_disk();
    init_super_block(); 
    save();
    return 0;
}

/*
 * node_numbers is got from SuperBlock
 */
NATManager::NATManager(size_t inode_total) :
    inode_numbers(inode_total),
    nat_entries(nullptr)
{
    uint32_t req_sz = inode_total * sizeof(f2fs_nat_entry);
    sz = req_sz < BLOCK_SIZE ? BLOCK_SIZE : (req_sz / BLOCK_SIZE + 1) * BLOCK_SIZE;
}

NATManager::~NATManager() {
    if (nat_entries) {
        mem_free((char *)(nat_entries));
        nat_entries = nullptr;
    }
}

int NATManager::load() { 
    if (inode_numbers == 0) {
        return 0;
    }
    nat_entries = (f2fs_nat_entry*)mem_alloc(sz);
    int ret = single_read(-1, (uint64_t)NAT_BASE, sz, (char *)(nat_entries));
    if (ret < (int)sz) {
        LOG(ERROR) << "Fail to load NATBlock";
        return ret;
    }
    
    for (size_t i = 0; i < inode_numbers; i++) {
        f2fs_nat_entry entry = nat_entries[i];
        uint32_t ino = entry.ino;
        uint32_t blkaddr = entry.block_addr;
        //ino_blk_map.insert(InoBlkPair((unsigned)entry.ino, entry.block_addr));
        ino_blk_map.insert(InoBlkPair(ino, blkaddr));
    }
    return ret;
}

// save SuperBlock to Disk, assume ino_blk_map always update first
int NATManager::save() { 
    if (inode_numbers == 0) {
        return 0;
    }
    inode_numbers = ino_blk_map.size();
    if (!nat_entries) {
        uint32_t req_sz = inode_numbers * sizeof(f2fs_nat_entry);
        sz = req_sz < BLOCK_SIZE ? BLOCK_SIZE : (req_sz / BLOCK_SIZE + 1) * BLOCK_SIZE;
        nat_entries = (f2fs_nat_entry* )mem_alloc(sz);
    }
    int i = 0;
    for (const auto & iter :ino_blk_map)  {
        nat_entries[i].ino = iter.first;
        nat_entries[i].block_addr = iter.second;
        ++i;
    }

    int ret = single_write(-1, (uint64_t)NAT_BASE, sz, (char *)(nat_entries));
    // TODO later: add check md5sum and ret value
    if (ret < (int)sz) {
        LOG(ERROR) << "Fail to save NAT Manager";
    }
    return ret;
}

/*
 * Lookup the NAT by 'ino', return true for successful return, else return false
 */ 
bool NATManager::lookup_block_by_ino(uint32_t ino, uint32_t * blkno) { 
    InoBlkMap::iterator iter = ino_blk_map.find(ino);
    if (iter == ino_blk_map.end()) {
        return false;
    } else {
        *blkno = iter->second;
    }
    return true;
}

/*
 * Lookup the NAT by 'ino', return true for successful return, else return false
 */ 
bool NATManager::add_nat_entry(uint32_t ino, uint32_t blkno) { 
    InoBlkMap::iterator iter = ino_blk_map.find(ino);
    if (iter == ino_blk_map.end()) {
        ino_blk_map.insert(InoBlkPair(ino, blkno));
        return true;
    } else { /* ino already exists */
        CHECK_EQ(blkno, iter->second);
        //*blkno = iter->second;
    }
    return false;
}

SITManager::SITManager(uint32_t total) : sit_total_num(total)
{
#if 0
    uint32_t req_sz = 0;
    if (!total) {
       req_sz = BLOCK_SIZE;
    } else {
       req_sz = sizeof(f2fs_sit_block) * total;
    }
    //sz = req_sz < BLOCK_SIZE ? BLOCK_SIZE : (req_sz / BLOCK_SIZE + 1) * BLOCK_SIZE;
    sz = req_sz;
#else
    sz = SIT_SIZE;
#endif
    sit_entries = (f2fs_sit_entry *)(mem_alloc(sz));
}

SITManager::~SITManager() {
    if (sit_entries) {
        mem_free((char *)(sit_entries));
        sit_entries = nullptr;
    }
}

int SITManager::load() { 
    if (sit_total_num == 0) {
        return 0;
    }
    uint32_t ret = single_read(-1, (uint64_t)SIT_BASE, sz, (char *)(sit_entries));
    if (ret < sz) {
        LOG(ERROR) << "Fail to load SITManager";
    }
    return ret;
}

// save SuperBlock to Disk
int SITManager::save() { 
    if (sit_total_num == 0) {
        return 0;
    }
    uint32_t ret = single_write(-1, (uint64_t)SIT_BASE, sz, (char *)(sit_entries));
    // TODO later: add check md5sum and ret value
    if (ret < sz) {
        LOG(ERROR) << "Fail to save SITManager";
    }
    return ret;
}

// set all blocks valid and free during disk format
int SITManager::format() { 
    for (size_t i = 0; i < sit_total_num; i++) {
        sit_entries[i].vblocks = SIT_VBLOCKS_MASK; // this may need change if block size/segment size changes
        for (int j = 0; j < SIT_VBLOCK_MAP_SIZE; j++) {
            sit_entries[i].valid_map[j] = 0xff;  // 1 means valid
            //sit_entries[i].valid_map[j] = FREE; 
        }
        sit_entries[i].mtime = base::monotonic_time_us();
    }
    return save();
}

/*
 * update SIT valid map and total valid number if one block is used or freed 
 *
 * blkno: physical block number indexed from begingging of raw disk
 *
 * TODO: if the state changes from USED to FREE, we can add the blk to pending trim list
 */
void SITManager::update_sit_map(uint32_t blkno, bool state) {
    // find the expected sit entry
    uint32_t seg_index = (blkno - (SIT_BASE / BLOCK_SIZE)) / (SEGMENT_SIZE / BLOCK_SIZE);
    uint8_t blk_index = (blkno - (SIT_BASE / BLOCK_SIZE)) % (SEGMENT_SIZE / BLOCK_SIZE) / 8;
    uint8_t blk_bit = (blkno - (SIT_BASE / BLOCK_SIZE)) % (SEGMENT_SIZE / BLOCK_SIZE) % 8;
    uint8_t val = sit_entries[seg_index].valid_map[blk_index];

    if (state == USED) {
        --sit_entries[blk_index].vblocks;
        val &= ~(1<<blk_bit);
    } else {
        ++sit_entries[blk_index].vblocks;
        val |= (1<<blk_bit);
    }
    sit_entries[seg_index].valid_map[blk_index] = val;
    sit_entries[seg_index].mtime = base::monotonic_time_us();
}

/*
 * Free all resouce allocated by create_inode
 */
void NodeManager::delete_inode(f2fs_inode * inode) {
    if(inode != NULL) {
        mem_free((char *)inode);
    }
} 

struct f2fs_inode * NodeManager::create_inode(uint32_t blkno, uint32_t ino, const char * name) {
    struct f2fs_inode * inode = (f2fs_inode *)mem_alloc(sizeof(f2fs_inode));
    memset(inode, 0, sizeof(f2fs_inode));
    //strcpy((char *)inode->i_name, name);
    strncpy((char *)inode->i_name, name, std::min(strlen(name) + 1, (size_t)F2FS_NAME_LEN));
    //inode->i_namelen = strlen(name) + 1;
    inode->i_namelen = std::min(strlen(name) + 1, (size_t)F2FS_NAME_LEN);
    inode->i_blkno = blkno;
    return inode;
}

int NodeManager::write_inode(uint32_t blkno, struct f2fs_inode * inode) {
    int ret = single_write(-1, (uint64_t)blkno * BLOCK_SIZE, sizeof(f2fs_inode), (char *)(inode));
    // TODO later: add check md5sum and ret value
    if (ret < 0) {
        LOG(ERROR) << "Fail to write inode";
        return -1;
    }
    return 0;
}

struct f2fs_inode * NodeManager::read_inode(uint32_t blkno) {
    struct f2fs_inode * inode = (f2fs_inode *)mem_alloc(sizeof(f2fs_inode));
    int ret = single_read(-1, (uint64_t)(blkno) * BLOCK_SIZE, sizeof(f2fs_inode), (char *)(inode));
    // TODO later: add check md5sum and ret value
    if (ret < 0) {
        LOG(ERROR) << "Fail to read inode";
        return NULL;
    }
    return inode;
}

/*
 * Currently we only cache the direct/indirect node, but doesn't support evict the older one
 * TODO later: take LRU / LFR  to group them, otherwise it may exhaust memory
 */
direct_node * AddressTranslator::load_direct_node(uint32_t blk_index) {
    DirectNodeBlkMap::iterator it = _dmap.find(blk_index);
    direct_node * dnode = nullptr;
    if (it == _dmap.end()) { // fail to find the inode mapped by block
        dnode = (direct_node *)mem_alloc(sizeof(direct_node));
        single_read(-1, NODE_BASE + (uint64_t)blk_index * BLOCK_SIZE, BLOCK_SIZE, (char *)(dnode));
        _dmap.insert(DirectNodeBlkPair(blk_index, dnode));
    } else {
        dnode = it->second;
    }
    return dnode;
}

/*
 * Add blokc_num and direct_node map
 * TODO later: take LRU / LFR  to group them, otherwise it may exhaust memory
 */
void AddressTranslator::add_direct_node_map(uint32_t blk_index, direct_node * dnode) {
    DirectNodeBlkMap::iterator it = _dmap.find(blk_index);
    if (it == _dmap.end()) { // fail to find the inode mapped by block
        _dmap.insert(DirectNodeBlkPair(blk_index, dnode));
    }
}

/*
 * Currently we only cache the direct/indirect node, but doesn't support evict the older one
 * TODO later: take LRU / LFR  to group them, otherwise it may exhaust memory
 */
//indirect_node * AddressTranslator::load_indirect_node(uint32_t blk_index) {
indirect_node * AddressTranslator::load_indirect_node(unsigned int blk_index) {
    IndirectNodeBlkMap::iterator it = _inmap.find(blk_index);
    indirect_node * indnode = nullptr;
    if (it == _inmap.end()) { // fail to find the inode mapped by block
        indnode = (indirect_node *)mem_alloc(sizeof(indirect_node));
        single_read(-1, NODE_BASE + (uint64_t)blk_index * BLOCK_SIZE, BLOCK_SIZE, (char *)(indnode));
        _inmap.insert(IndirectNodeBlkPair(blk_index, indnode));
    } else {
        indnode = it->second;
    }
    return indnode;
}

/*
 *  Add indirect node map to its block location
 * TODO later: take LRU / LFR  to group them, otherwise it may exhaust memory
 */
//indirect_node * AddressTranslator::load_indirect_node(uint32_t blk_index) {
void AddressTranslator::add_indirect_node_map(unsigned int blk_index, indirect_node * indnode) {
    IndirectNodeBlkMap::iterator it = _inmap.find(blk_index);
    if (it == _inmap.end()) {
        _inmap.insert(IndirectNodeBlkPair(blk_index, indnode));
    }
}

/*
 * The maximum depth is four.
 * Offset[0] will have raw inode offset.
 *
 * get all related block/index to locate the logical 'block' Block 
 */
int AddressTranslator::get_node_path(long block, int offset[4], unsigned int noffset[4]) {
    const long direct_index = DEF_ADDRS_PER_INODE;
    const long direct_blks = ADDRS_PER_BLOCK;
    const long dptrs_per_blk = NIDS_PER_BLOCK;
    const long indirect_blks = ADDRS_PER_BLOCK * NIDS_PER_BLOCK;
    const long dindirect_blks = indirect_blks * NIDS_PER_BLOCK;
    int n = 0;
    int level = 0;

    noffset[0] = 0;
    // offset: block offset in logical file
    // noffset: node offset in the file
    if (block < direct_index) {
        offset[n] = block;
        goto got;
    }
    block -= direct_index;
    if (block < direct_blks) {
        offset[n++] = NODE_DIR1_BLOCK;
        noffset[n] = 1;
        offset[n] = block;
        level = 1;
        goto got;
    }
    block -= direct_blks;
    if (block < direct_blks) {
        offset[n++] = NODE_DIR2_BLOCK;
        noffset[n] = 2;
        offset[n] = block;
        level = 1;
        goto got;
    }
    block -= direct_blks;
    if (block < indirect_blks) {
        offset[n++] = NODE_IND1_BLOCK;
        noffset[n] = 3;
        offset[n++] = block / direct_blks;
        noffset[n] = 4 + offset[n - 1];
        offset[n] = block % direct_blks;
        level = 2;
        goto got;
    }
    block -= indirect_blks;
    if (block < indirect_blks) {
        offset[n++] = NODE_IND2_BLOCK;
        noffset[n] = 4 + dptrs_per_blk;
        offset[n++] = block / direct_blks;
        noffset[n] = 5 + dptrs_per_blk + offset[n - 1];
        offset[n] = block % direct_blks;
        level = 2;
        goto got;
    }
    block -= indirect_blks;
    if (block < dindirect_blks) {
        offset[n++] = NODE_DIND_BLOCK;
        noffset[n] = 5 + (dptrs_per_blk * 2);
        offset[n++] = block / indirect_blks;
        noffset[n] = 6 + (dptrs_per_blk * 2) +
                  offset[n - 1] * (dptrs_per_blk + 1);
        offset[n++] = (block / direct_blks) % dptrs_per_blk;
        noffset[n] = 7 + (dptrs_per_blk * 2) +
                  offset[n - 2] * (dptrs_per_blk + 1) +
                  offset[n - 1];
        offset[n] = block % direct_blks;
        level = 3;
        goto got;
    } else {
        //return -E2BIG;
        return -2;
    }
got:
#if 0
    for (int i = 0; i < 4; ++i) {
        LOG(INFO) << "offset[" << i <<"] = " << offset[i] << "; noffset[" << i <<"] = " << noffset[i];
    }
    LOG(INFO) << "Level: " << level;
#endif
    return level;
}

/*
 * Lookup the phyiscal block address of logical block and update its new physical address to dnode,
 * and still return the original physical block index
 */
int AddressTranslator::update_node_entry(uint32_t phy_blk, long logical_blk) {
    //return _lookup_and_update_entry(phy_blk, logical_blk, true);
    return _lookup_and_update_entry(phy_blk, logical_blk, false);
}

/*
 * only Lookup the phyiscal block address of logical block
 */
int AddressTranslator::lookup_node_entry(long logical_blk) {
    return _lookup_and_update_entry(-1, logical_blk, false);
}

/*
 * Todo: we temp use 0 to means no block already allocated for node
 * Later: we should also check its valid status
 *
 * TODO: we should cache the already read out data,inode/dnode/indnode/d2innode in case read again from disk
 *
 * use block 'phy_blk' to store data in 'logical_blk'
 *
 *  we can get setup cache during lookup and update entry
 *
 *  solution one: re-write load_direct_node /save_direct_node()
 *
 * return the original physical block index, if it is first allocated, return 0; that require
 * block init process to set the 0
 */
int AddressTranslator::_lookup_and_update_entry(uint32_t phy_blk, long logical_blk, bool update)
{
    int offset[4];
    unsigned int noffset[4];
    uint32_t origin_phy_blk = NOT_FOUND; // 0 means not found

    for (int i = 0; i < 4; ++i) {
        offset[0] = noffset[i] = NOT_FOUND;
    }
    int level = get_node_path(logical_blk, offset, noffset);
    direct_node * dnode = NULL;
    indirect_node * in_dir = NULL;
    indirect_node * d2in_dir = NULL;

    switch (level) {
        case 0:
            origin_phy_blk = inode->i_addr[offset[0]]; 
            inode->i_addr[offset[0]] = phy_blk;
            if (update) { // TODO: merge the inode update at last, otherwize the WA is too large
                single_write(-1, (uint64_t)(inode->i_blkno) * BLOCK_SIZE, BLOCK_SIZE, (char *)(inode));
            }
            break;
        case 1:
            if (inode->i_nid[offset[0] - NODE_DIR1_BLOCK] != 0) {
            //if (inode->i_nid[noffset[1] - 1] != 0) {
                dnode = load_direct_node(inode->i_nid[offset[0] - NODE_DIR1_BLOCK]);
                origin_phy_blk = dnode->addr[offset[1]];
            } else {
                inode->i_nid[offset[0] - NODE_DIR1_BLOCK] = g_node_allocator->alloc_free_blk();
                //inode->i_nid[noffset[1] - 1] = g_node_allocator->alloc_free_blk();
                dnode = (direct_node *)mem_alloc(sizeof(direct_node));
                add_direct_node_map(inode->i_nid[offset[0] - NODE_DIR1_BLOCK], dnode);
                // save updated changes to inode
                if (update) { 
                    single_write(-1, (uint64_t)(inode->i_blkno) * BLOCK_SIZE, BLOCK_SIZE, (char *)(inode));
                }
            }
            // update direct node
            dnode->addr[offset[1]] = phy_blk; 
            if (update) {
                // save dnode, update direct pointer of phy_blk
                single_write(-1, (uint64_t)(inode->i_nid[offset[0] - NODE_DIR1_BLOCK]) * BLOCK_SIZE, BLOCK_SIZE, (char *)(dnode));
            }
            break;
        case 2:
            if (inode->i_nid[offset[0] - NODE_DIR1_BLOCK] != 0) { /* already set up entry in indoe */
                in_dir = load_indirect_node(inode->i_nid[offset[0] - NODE_DIR1_BLOCK]); 
                if (in_dir->nid[offset[1]] != 0) { // means direct node exist
                    dnode = load_direct_node(in_dir->nid[offset[1]]);
                } else {
                    in_dir->nid[offset[1]] = g_node_allocator->alloc_free_blk();
                    dnode = (direct_node *)mem_alloc(sizeof(direct_node));
                    add_direct_node_map(in_dir->nid[offset[1]], dnode);
                }
                origin_phy_blk = dnode->addr[offset[2]];
                dnode->addr[offset[2]] = phy_blk;
                if (update) { 
                    single_write(-1, (uint64_t)(in_dir->nid[offset[1]]) * BLOCK_SIZE, BLOCK_SIZE, (char *)(dnode));
                }
            } else { // need setup entry in indoe
                inode->i_nid[offset[0] - NODE_DIR1_BLOCK] = g_node_allocator->alloc_free_blk();
                in_dir = (indirect_node *)mem_alloc(sizeof(indirect_node));
                add_indirect_node_map(inode->i_nid[offset[0] - NODE_DIR1_BLOCK], in_dir);
                in_dir->nid[offset[1]] = g_node_allocator->alloc_free_blk();
                dnode = (direct_node *)mem_alloc(sizeof(direct_node));
                add_direct_node_map(in_dir->nid[offset[1]], dnode);
                dnode->addr[offset[2]] = phy_blk;
                if (update) { 
                    single_write(-1, (uint64_t)(in_dir->nid[offset[1]]) * BLOCK_SIZE, BLOCK_SIZE, (char *)(dnode));
                    single_write(-1, (uint64_t)(inode->i_nid[offset[0] - NODE_DIR1_BLOCK]) * BLOCK_SIZE,
                            BLOCK_SIZE, (char *)(in_dir));
                }
            }
            break;
        case 3:
            if (inode->i_nid[offset[0] - NODE_DIR1_BLOCK] != 0) { // double indirect node exist
                d2in_dir = load_indirect_node(inode->i_nid[offset[0] - NODE_DIR1_BLOCK]); 
                if (d2in_dir->nid[offset[1]] != 0) { // indirect node exist
                    in_dir = load_indirect_node(d2in_dir->nid[offset[1]]);
                    if (in_dir->nid[offset[2]]) { // direct node exist
                        dnode = load_direct_node(in_dir->nid[offset[2]]);
                        origin_phy_blk = dnode->addr[offset[3]];
                        dnode->addr[offset[3]] = phy_blk;
                        if (update) { // only update dnode
                            single_write(-1, (uint64_t)(in_dir->nid[offset[2]]) * BLOCK_SIZE, BLOCK_SIZE, (char *)dnode);
                        }
                    } else { // direct node not exist
                        in_dir->nid[offset[2]] = g_node_allocator->alloc_free_blk();
                        dnode = (direct_node *)mem_alloc(sizeof(direct_node));
                        add_direct_node_map(in_dir->nid[offset[2]], dnode);
                        dnode->addr[offset[3]] = phy_blk;
                        if (update) { // update indirect and dnode
                            single_write(-1, (uint64_t)(d2in_dir->nid[offset[1]]) * BLOCK_SIZE, BLOCK_SIZE, (char *)(in_dir));
                            single_write(-1, (uint64_t)(in_dir->nid[offset[2]]) * BLOCK_SIZE, BLOCK_SIZE, (char *)dnode);
                        }
                    }
                } else { // indirect node not exist
                    d2in_dir->nid[offset[1]] = g_node_allocator->alloc_free_blk(); // alloc block for indirect
                    in_dir = (indirect_node *)mem_alloc(sizeof(indirect_node));
                    add_indirect_node_map(d2in_dir->nid[offset[1]], in_dir);
                    in_dir->nid[offset[2]] = g_node_allocator->alloc_free_blk(); // alloc block for dnode
                    dnode = (direct_node *)mem_alloc(sizeof(direct_node));
                    add_direct_node_map(in_dir->nid[offset[2]], dnode);
                    dnode->addr[offset[3]] = phy_blk;
                    if (update) {
                        // save d2-indirect node
                        single_write(-1, (uint64_t)(inode->i_nid[offset[0] - NODE_DIR1_BLOCK]) * BLOCK_SIZE, BLOCK_SIZE, (char *)d2in_dir);
                        // save indirect node
                        single_write(-1, (uint64_t)(d2in_dir->nid[offset[1]]) * BLOCK_SIZE, BLOCK_SIZE, (char *)in_dir);
                        // save direct node
                        single_write(-1, (uint64_t)(in_dir->nid[offset[2]]) * BLOCK_SIZE, BLOCK_SIZE, (char *)(dnode));
                    }
                }
            } else { // no double indirect allocated
                inode->i_nid[offset[0] - NODE_DIR1_BLOCK] = g_node_allocator->alloc_free_blk(); // alloc d2in blk
                d2in_dir = (indirect_node *)mem_alloc(sizeof(indirect_node));
                add_indirect_node_map(inode->i_nid[offset[0] - NODE_DIR1_BLOCK], d2in_dir);
                d2in_dir->nid[offset[1]] = g_node_allocator->alloc_free_blk(); // alloc indireict block
                in_dir = (indirect_node *)mem_alloc(sizeof(indirect_node));
                add_indirect_node_map(d2in_dir->nid[offset[1]], in_dir);
                in_dir->nid[offset[2]] = g_node_allocator->alloc_free_blk(); // alloc  direct block
                dnode = (direct_node *)mem_alloc(sizeof(direct_node));
                add_direct_node_map(in_dir->nid[offset[2]], dnode);
                dnode->addr[offset[3]] = phy_blk;
                if (update) {
                    // save doube direct node
                    single_write(-1, (uint64_t)(inode->i_nid[offset[0] - NODE_DIR1_BLOCK]) * BLOCK_SIZE, BLOCK_SIZE, (char *)d2in_dir);
                    // save indirect node
                    single_write(-1, (uint64_t)(d2in_dir->nid[offset[1]]) * BLOCK_SIZE,BLOCK_SIZE, (char *)(in_dir));
                    // save direct node
                    single_write(-1, (uint64_t)(in_dir->nid[offset[2]]) * BLOCK_SIZE, BLOCK_SIZE, (char *)(dnode));
                }
            }
            break;
        default:
            LOG(ERROR) << "error!";
            break;
    }
    //LOG(INFO) << "origin_phy_blk: " << origin_phy_blk;
    return origin_phy_blk;
}

/*
 * Get physical blk number according to the logical block number 'block' in current file spcified by
 * inode
int AddressTranslator::get_logical_block_address(uint64_t logical_blknum) {
    int offset[4];
    unsigned int noffset[4];

    int level = get_node_path(logical_blknum, offset, noffset);
}
 */

AddressTranslator::~AddressTranslator() {
    // save direct node 
    // save direct node
    // free all dnode
    for (auto & iter : _dmap) {    
        if (iter.second) { 
            single_write(-1, (uint64_t)iter.first * BLOCK_SIZE, BLOCK_SIZE, (char *)iter.second);
            delete (iter.second);
        }
    }
    // free all indrect node and all double indirect node
    for (auto & iter : _inmap) {    
        if (iter.second) { 
            single_write(-1, (uint64_t)iter.first * BLOCK_SIZE, BLOCK_SIZE, (char *)iter.second);
            delete (iter.second);
        }
    }
}

}
}
/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */
