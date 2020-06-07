/***************************************************************************
 * 
 * Copyright (c) 2019 Haha.com, Inc. All Rights Reserved
 * 
 **************************************************************************/
/**
 * @file append_demo_layout.h
 * @author xiaqichao(com@Haha.com)
 * @date 2019/02/26 18:38:52
 * @brief 
 *  
 **/
#ifndef  __APPEND_DEMO_LAYOUT_H_
#define  __APPEND_DEMO_LAYOUT_H_

#include <string.h>
#include <map>                      // std::map
#include <set>                      // std::set
#include <queue>                    // std::queue and priority_queue
#include <string>                   // std::string
#include <cfloat>                   // std::float
#include <algorithm>
#include <base/endpoint.h>          // base::EndPoint
#include <vector>                   // std::vector
#include <base/status.h>            // base::Status
#include <inttypes.h> 
#include <stdint.h>
#include <cstdint>

#include <boost/atomic.hpp>                     // boost::atomic
#include <base/memory/singleton.h>  // Singleton
#include <bthread/mutex.h>          // CDSMutex
//#include "proto/rbs.pb.h"           // DiskType
#include "common/types.h"
#include "common/lockable.h"        // Lockable
//#include <atomic.h>                 // boost::atomic

//#include <base/memory/singleton.h>  // Singleton
//#include <bthread/mutex.h>          // CDSMutex

#include "io_abstraction.h"

#define NOT_FOUND   0
#define ENTRIES_IN_SUM        512
#define    SUMMARY_SIZE        (7)    /* sizeof(struct summary) */
#define    SUM_FOOTER_SIZE        (5)    /* sizeof(struct summary_footer) */
#define SUM_ENTRY_SIZE        (SUMMARY_SIZE * ENTRIES_IN_SUM)
#define BLOCK_SIZE 0x1000
//#define BLOCK_SIZE 0x1000

/*
 * For SIT entries
 *
 * Each segment is 2MB in size by default so that a bitmap for validity of
 * there-in blocks should occupy 64 bytes, 512 bits.
 * Not allow to change this.
 */                                                                                                                          
#define SIT_VBLOCK_MAP_SIZE 64
#define SIT_ENTRY_PER_BLOCK (BLOCK_SIZE / sizeof(struct f2fs_sit_entry))
/*
 * F2FS uses 4 bytes to represent block address. As a result, supported size of
 * disk is 16 TB and it equals to 16 * 1024 * 1024 / 2 segments.
 */                                                                                                                          
#define F2FS_MAX_SEGMENT       ((16 * 1024 * 1024) / 2)                                                                      
/*                             
 * Note that f2fs_sit_entry->vblocks has the following bit-field information.                
 * [15:10] : allocation type such as CURSEG_XXXX_TYPE                                         * [9:0] : valid block count                                                                  */
#define SIT_VBLOCKS_SHIFT    10
#define SIT_VBLOCKS_MASK    ((1 << SIT_VBLOCKS_SHIFT) - 1)
#define GET_SIT_VBLOCKS(raw_sit)                \
    (le16_to_cpu((raw_sit)->vblocks) & SIT_VBLOCKS_MASK)                                                                     
#define GET_SIT_TYPE(raw_sit)                    \
    ((le16_to_cpu((raw_sit)->vblocks) & ~SIT_VBLOCKS_MASK)    \
     >> SIT_VBLOCKS_SHIFT)
enum BLOCK_STATUS {
    FREE = 0,
    USED = 1,
};
#define OFFSET_BIT_MASK        (0x07)    /* (0x01 << OFFSET_BIT_SHIFT) - 1 */

#define MAX_FILE_NAME_LEN 64

/// SSD domain
#define SSD_SECTOR_SIZE 0x4000ULL
#define SSD_BLK_SIZE 0x1800000ULL
/// demo domain
#define SEGMENT_SIZE 0x200000ULL 

// absolute offset and lenght
// whole layout for demo
#define SB_BASE  0x000000
#define SB_SIZE  (2 * SSD_SECTOR_SIZE)
#define CP_BASE (SSD_BLK_SIZE)
#define CP_SIZE (2 * SSD_BLK_SIZE)
#define NAT_BASE (3 * SSD_BLK_SIZE)
#define NAT_SIZE 0x10800000
#define SIT_BASE (NAT_BASE+NAT_SIZE)
#define SIT_SIZE 0x21000000ULL
#define SSA_BASE (SIT_BASE+SIT_SIZE)
#define SSA_SIZE 0x200000000ULL
#define NODE_BASE (SSA_BASE + SSA_SIZE)
#define NODE_SIZE 0x200000000ULL
#define DATA_BASE (NODE_BASE+NODE_SIZE)
#define DATA_SIZE (0x40000000000ULL - DATA_BASE) 


#define F2FS_SUPER_OFFSET        1024    /* byte-size offset */
#define F2FS_MIN_LOG_SECTOR_SIZE    9    /* 9 bits for 512 bytes */
#define F2FS_MAX_LOG_SECTOR_SIZE    12    /* 12 bits for 4096 bytes */
#define F2FS_LOG_SECTORS_PER_BLOCK    3    /* log number for sector/blk */
#define F2FS_BLKSIZE            4096    /* support only 4KB block */
#define F2FS_BLKSIZE_BITS        12    /* bits for F2FS_BLKSIZE */
#define F2FS_MAX_EXTENSION        64    /* # of extension entries */
#define F2FS_EXTENSION_LEN        8    /* max size of extension */
#define F2FS_BLK_ALIGN(x)    (((x) + F2FS_BLKSIZE - 1) >> F2FS_BLKSIZE_BITS)

#define NULL_ADDR        ((block_t)0)    /* used as block_t addresses */
#define NEW_ADDR        ((block_t)-1)    /* used as block_t addresses */

#define F2FS_BYTES_TO_BLK(bytes)    ((bytes) >> F2FS_BLKSIZE_BITS)
#define F2FS_BLK_TO_BYTES(blk)        ((blk) << F2FS_BLKSIZE_BITS)

/* 0, 1(node nid), 2(meta nid) are reserved node id */
#define F2FS_RESERVED_NODE_NUM        3

#define F2FS_ROOT_INO(sbi)    ((sbi)->root_ino_num)
#define F2FS_NODE_INO(sbi)    ((sbi)->node_ino_num)
#define F2FS_META_INO(sbi)    ((sbi)->meta_ino_num)

#define F2FS_MAX_QUOTAS        3

#define F2FS_IO_SIZE(sbi)    (1 << F2FS_OPTION(sbi).write_io_size_bits) /* Blocks */
#define F2FS_IO_SIZE_KB(sbi)    (1 << (F2FS_OPTION(sbi).write_io_size_bits + 2)) /* KB */
#define F2FS_IO_SIZE_BYTES(sbi)    (1 << (F2FS_OPTION(sbi).write_io_size_bits + 12)) /* B */
#define F2FS_IO_SIZE_BITS(sbi)    (F2FS_OPTION(sbi).write_io_size_bits) /* power of 2 */
#define F2FS_IO_SIZE_MASK(sbi)    (F2FS_IO_SIZE(sbi) - 1)

/* This flag is used by node and meta inodes, and by recovery */
#define GFP_F2FS_ZERO        (GFP_NOFS | __GFP_ZERO)

/*
 * For further optimization on multi-head logs, on-disk layout supports maximum
 * 16 logs by default. The number, 16, is expected to cover all the cases
 * enoughly. The implementaion currently uses no more than 6 logs.
 * Half the logs are used for nodes, and the other half are used for data.
 */
#define MAX_ACTIVE_LOGS    16
#define MAX_ACTIVE_NODE_LOGS    8
#define MAX_ACTIVE_DATA_LOGS    8

#define VERSION_LEN    256
#define MAX_VOLUME_NAME        512
#define MAX_PATH_LEN        64
#define MAX_DEVICES        8
#define __packed __attribute__((packed))

#define F2FS_NAME_LEN        255
/* 200 bytes for inline xattrs by default */
#define DEFAULT_INLINE_XATTR_ADDRS    50
#define DEF_ADDRS_PER_INODE    923    /* Address Pointers in an Inode */
#define CUR_ADDRS_PER_INODE(inode)    (DEF_ADDRS_PER_INODE - \
                    get_extra_isize(inode))
#define DEF_NIDS_PER_INODE    5    /* Node IDs in an Inode */
#define ADDRS_PER_INODE(inode)    addrs_per_inode(inode)
#define ADDRS_PER_BLOCK        1018    /* Address Pointers in a Direct Block */
#define NIDS_PER_BLOCK        1018    /* Node IDs in an Indirect Block */

#define ADDRS_PER_PAGE(page, inode)    \
    (IS_INODE(page) ? ADDRS_PER_INODE(inode) : ADDRS_PER_BLOCK)

#define    NODE_DIR1_BLOCK        (DEF_ADDRS_PER_INODE + 1)
#define    NODE_DIR2_BLOCK        (DEF_ADDRS_PER_INODE + 2)
#define    NODE_IND1_BLOCK        (DEF_ADDRS_PER_INODE + 3)
#define    NODE_IND2_BLOCK        (DEF_ADDRS_PER_INODE + 4)
#define    NODE_DIND_BLOCK        (DEF_ADDRS_PER_INODE + 5)

#define F2FS_INLINE_XATTR    0x01    /* file inline xattr flag */
#define F2FS_INLINE_DATA    0x02    /* file inline data flag */
#define F2FS_INLINE_DENTRY    0x04    /* file inline dentry flag */
#define F2FS_DATA_EXIST        0x08    /* file inline data exist flag */
#define F2FS_INLINE_DOTS    0x10    /* file having implicit dot dentries */
#define F2FS_EXTRA_ATTR        0x20    /* file having extra attribute */
#define F2FS_PIN_FILE        0x40    /* file should not be gced */


namespace bce {
namespace cds {
/*
 * For superblock
 */
struct f2fs_device {
    uint8_t path[MAX_PATH_LEN];
    uint32_t total_segments;
} __packed;

struct f2fs_super_block {
    uint32_t magic;            /* Magic Number */
    uint16_t major_ver;        /* Major Version */
    uint16_t minor_ver;        /* Minor Version */
    uint32_t log_sectorsize;        /* log2 sector size in bytes */
    uint32_t log_sectors_per_block;    /* log2 # of sectors per block */
    uint32_t log_blocksize;        /* log2 block size in bytes */
    uint32_t log_segmentsize;    /* log2 # of blocks per segment */
    uint32_t segs_per_sec;        /* # of segments per section */
    uint32_t secs_per_zone;        /* # of sections per zone */
    uint32_t checksum_offset;        /* checksum offset inside super block */
    uint64_t block_count;        /* total # of user blocks */
    uint32_t section_count;        /* total # of sections */
    uint32_t segment_count;        /* total # of segments */
    uint32_t segment_count_ckpt;    /* # of segments for checkpoint */
    uint32_t segment_count_sit;    /* # of segments for SIT */
    uint32_t segment_count_nat;    /* # of segments for NAT */
    uint32_t segment_count_ssa;    /* # of segments for SSA */
    uint32_t segment_count_node;    /* # of segments for main node area */
    uint32_t segment_count_data;    /* # of segments for main data area */
    uint32_t segment0_blkaddr;    /* start block address of segment 0 */
    uint32_t cp_blkaddr;        /* start block address of checkpoint */
    uint32_t sit_blkaddr;        /* start block address of SIT */
    uint32_t nat_blkaddr;        /* start block address of NAT */
    uint32_t ssa_blkaddr;        /* start block address of SSA */
    uint32_t node_blkaddr;        /* start block address of main area */
    uint32_t data_blkaddr;        /* start block address of main area */
    uint32_t cp_next_blkaddr;        /* alloc start address of checkpoint */
    uint32_t sit_next_blkaddr;        /* alloc start block address of SIT */
    uint32_t nat_next_blkaddr;        /* alloc start block address of NAT */
    uint32_t ssa_next_blkaddr;        /* alloc start block address of SSA */
    uint32_t node_next_blkaddr;        /* alloc start block address of main area */
    uint32_t data_next_blkaddr;        /* alloc start block address of main area */
    uint32_t root_ino;        /* root inode number */
    uint32_t node_ino;        /* node inode number */
    uint32_t meta_ino;        /* meta inode number */
    uint8_t uuid[16];            /* 128-bit uuid for volume */
    uint16_t volume_name[MAX_VOLUME_NAME];    /* volume name */
    uint32_t extension_count;        /* # of extensions below */
    uint8_t extension_list[F2FS_MAX_EXTENSION][F2FS_EXTENSION_LEN];/* extension array */
    uint32_t cp_payload;
    uint8_t version[VERSION_LEN];    /* the kernel version */
    uint8_t init_version[VERSION_LEN];    /* the initial kernel version */
    uint32_t feature;            /* defined features */
    uint8_t encryption_level;        /* versioning level for encryption */
    uint8_t encrypt_pw_salt[16];    /* Salt used for string2key algorithm */
    struct f2fs_device devs[MAX_DEVICES];    /* device list */
    uint32_t qf_ino[F2FS_MAX_QUOTAS];    /* quota inode numbers */
    uint8_t hot_ext_count;        /* # of hot file extension */
    uint8_t reserved[314];        /* valid reserved region */
} __packed;

/*
 *
 */
class MetaManager : public Lockable {
public:    
    MetaManager() {};
    virtual ~MetaManager() {};
    virtual int load() = 0;
    virtual int save() = 0;
};

class SuperBlockManager : public MetaManager {
public:    
    SuperBlockManager();
    virtual ~SuperBlockManager();
    int load(); // load SuperBlock info from disk to Memroy
    int save(); // save SuperBlock info from memroy to disk
    int init_super_block(); //  format disk 
    int format(); //  format disk 
    uint32_t get_inode_total() const { return sb->node_ino; };
    uint32_t inc_inode_total() { return ++(sb->node_ino); };
    uint32_t dec_inode_total() { return --(sb->node_ino); };
    uint32_t get_nat_total() const  { return sb->nat_next_blkaddr - sb->nat_blkaddr; }; // used init NAT Table
    uint32_t get_sit_total() const  { return sb->sit_next_blkaddr - sb->sit_blkaddr; };
    uint32_t sit_blkaddr()const { return sb->sit_blkaddr;};
    uint32_t sit_next_blkaddr()const { return sb->sit_next_blkaddr;};
    uint32_t nat_blkaddr()const { return sb->nat_blkaddr;};
    uint32_t nat_next_blkaddr()const { return sb->nat_next_blkaddr;};
    uint32_t data_blkaddr()const { return sb->data_blkaddr;};
    uint32_t data_next_blkaddr()const { return sb->data_next_blkaddr;};
    uint32_t node_blkaddr()const { return sb->node_blkaddr;};
    uint32_t node_next_blkaddr()const { return sb->node_next_blkaddr;};
    f2fs_super_block * sb;
    uint32_t sz;
};

/*
 * For NODE structure
 */
struct f2fs_extent {
    uint32_t fofs;        /* start file offset of the extent */
    uint32_t blk;        /* start block address of the extent */
    uint32_t len;        /* lengh of the extent */
} __packed;

struct f2fs_inode {
    uint16_t i_mode;            /* file mode */
    uint8_t i_advise;            /* file hints */
    uint8_t i_inline;            /* file inline flags */
    uint32_t i_blkno;            /* user ID */
    uint32_t i_uid;            /* user ID */
    uint32_t i_gid;            /* group ID */
    uint32_t i_links;            /* links count */
    uint64_t i_size;            /* file size in bytes */
    uint64_t i_blocks;        /* file size in blocks */
    uint64_t i_atime;            /* access time */
    uint64_t i_ctime;            /* change time */
    uint64_t i_mtime;            /* modification time */
    uint32_t i_atime_nsec;        /* access time in nano scale */
    uint32_t i_ctime_nsec;        /* change time in nano scale */
    uint32_t i_mtime_nsec;        /* modification time in nano scale */
    uint32_t i_generation;        /* file version (for NFS) */
    union {
        uint32_t i_current_depth;    /* only for directory depth */
        uint16_t i_gc_failures;    /*
                     * # of gc failures on pinned file.
                     * only for regular files.
                     */
    };
    uint32_t i_xattr_nid;        /* nid to save xattr */
    uint32_t i_flags;            /* file attributes */
    uint32_t i_pino;            /* parent inode number */
    uint32_t i_namelen;        /* file name length */
    uint8_t i_name[F2FS_NAME_LEN];    /* file name for SPOR */
    uint8_t i_dir_level;        /* dentry_level for large dir */

    struct f2fs_extent i_ext;    /* caching a largest extent */

    union {
        struct { // for what usage?
            uint16_t i_extra_isize;    /* extra inode attribute size */
            uint16_t i_inline_xattr_size;    /* inline xattr size, unit: 4 bytes */
            uint32_t i_projid;    /* project id */
            uint32_t i_inode_checksum;/* inode meta checksum */
            uint64_t i_crtime;    /* creation time */
            uint32_t i_crtime_nsec;    /* creation time in nano scale */
            uint32_t i_extra_end[0];    /* for attribute size calculation */
        } __packed;
        uint32_t i_addr[DEF_ADDRS_PER_INODE];    /* Pointers to data blocks */
    };
    uint32_t i_nid[DEF_NIDS_PER_INODE];    /* direct(2), indirect(2),
                        double_indirect(1) node id */
    uint32_t pad[5]; /* pad to make size of f2fs_inode to be 4096B */
} __packed;

/*
 * f2fs assigns the following node offsets described as (num).
 * N = NIDS_PER_BLOCK
 *
 *  Inode block (0) (0 => DEF_ADDRS_PER_INODE - 1)
 *    |- direct node (1) (DEF_ADDRS_PER_INODE = > DEF_ADDRS_PER_INODE + N - 1)
 *    |- direct node (2) (DEF_ADDRS_PER_INODE + N = > DEF_ADDRS_PER_INODE + 2N - 1)
 *    |- indirect node (3)
 *    |            `- direct node (4)  (DEF_ADDRS_PER_INODE + 2N, DEF_ADDRS_PER_INODE + 3N - 1)
 *    |            `- direct node (5)  (DEF_ADDRS_PER_INODE + 3N, DEF_ADDRS_PER_INODE + 4N - 1)
 *    |             ......
 *    |             - direct node (3+N) (DEF_ADDRS_PER_INODE + 2N + (3+N - 4)*N, +N-1)
 *    |- indirect node (4 + N)
 *    |            `- direct node (5 + N) (DEF_ADDRS_PER_INODE+N**2+2N, DEF_ADDRS_PER_INODE+N**2+2N+N-1 )
 *    |             ......
 *    |            `- direct node (5 + 2N - 1) (DEF_ADDRS_PER_INODE+N**2+2N + (N-1)*N, +N-1)
 *    `- double indirect node (5 + 2N)
 *                 `- indirect node (6 + 2N) (DEF_ADDRS_PER_INODE+2N**2+2N + (N-1)*N, +N**2-1)
 *                       `- direct node
 *                 ......
 *                 `- indirect node ((6 + 2N) + x(N + 1))
 *                       `- direct node
 *                 ......
 *                 `- indirect node ((6 + 2N) + (N - 1)(N + 1)) (DEF_ADDRS_PER_INODE+2N**2+2N + (N-1)*N+N**3-N**2, +N**2-1)
 *                       `- direct node
 */
struct direct_node {
    uint32_t addr[ADDRS_PER_BLOCK];    /* array of data block address */
} __packed;

struct indirect_node {
    uint32_t nid[NIDS_PER_BLOCK];    /* array of data block address */
} __packed;

enum {
    COLD_BIT_SHIFT = 0,
    FSYNC_BIT_SHIFT,
    DENT_BIT_SHIFT,
    OFFSET_BIT_SHIFT
};

struct node_footer {
    uint32_t nid;        /* node id */
    uint32_t ino;        /* inode nunmber */
    uint32_t flag;        /* include cold/mysync/dentry marks and offset */
    uint64_t cp_ver;        /* checkpoint version */
    uint32_t next_blkaddr;    /* next node page block address */
} __packed;

struct f2fs_node {
    /* can be one of three types: inode, direct, and indirect types */
    union {
        struct f2fs_inode i;
        struct direct_node dn;
        struct indirect_node in;
    };
    struct node_footer footer;
} __packed;


class NodeManager : public Lockable {
public:
    static NodeManager* GetInstance() {
        return Singleton<NodeManager>::get();
    }
    struct f2fs_inode * create_inode(uint32_t blkno, uint32_t ino, const char * name);
    int write_inode(uint32_t blokno, struct f2fs_inode * inode); 
    struct f2fs_inode * read_inode(uint32_t blokno);
    void delete_inode(f2fs_inode * inode);
private:
    NodeManager() {}
    ~NodeManager() {}
    DISALLOW_COPY_AND_ASSIGN(NodeManager);
    friend struct DefaultSingletonTraits<NodeManager>;
};

#define g_node_manager NodeManager::GetInstance()
/* Fix me */
/*
 * For NAT entries
 */
//struct f2fs_nat_entry; 
// demo map file name to ino, and search block_addr from nat entry table
struct f2fs_nat_entry {
    //uint8_t version;        /* latest version of cached nat entry */
    uint32_t ino;        /* inode number, used as key */
    uint32_t block_addr;    /* block address */
    //char name[MAX_FILE_NAME_LEN]; // demo doesn't compare name 
} __packed;
#define NAT_ENTRY_PER_BLOCK (BLOCK_SIZE / sizeof(struct f2fs_nat_entry))

// nat block may not need in demo
struct f2fs_nat_block {
    struct f2fs_nat_entry entries[NAT_ENTRY_PER_BLOCK];
} __packed;

// total NAT space: max inode number: 4T/4K 
// 8Byte per inde;
// so that max size: 4T/4K 
typedef std::map<uint32_t, uint32_t> InoBlkMap;
typedef std::pair<uint32_t, uint32_t> InoBlkPair;

class NATManager : public MetaManager {
public:    
    NATManager(size_t);
    virtual ~NATManager();
    int save(); // save NAT info from memroy to disk
    int load(); // load NAT info from disk to Memroy
    //int init_all_NAT(); // 
    //int read_entry();
    //int format() {}; //NAT format() could do nothing in demo
    uint32_t get_inode_total() const { return inode_numbers;};
    bool lookup_block_by_ino(uint32_t ino, uint32_t * blkno);
    //void add_nat_entry(uint32_t ino, uint32_t blkno) {ino_blk_map.insert(InoBlkPair(ino, blkno));};
    bool add_nat_entry(uint32_t ino, uint32_t blkno);
private:
    //f2fs_NAT_block ** nat_blocks;
    size_t inode_numbers;
    f2fs_nat_entry* nat_entries;
    InoBlkMap ino_blk_map;
    uint32_t sz;
};


// used to manage free/used blocks in one segment, a entry map to 2M segments, so we
// can read out all the data */
struct f2fs_sit_entry {
    uint16_t vblocks;                /* reference above */
    uint8_t valid_map[SIT_VBLOCK_MAP_SIZE];    /* bitmap for valid blocks */
    uint64_t mtime;                /* segment age for cleaning */
} __packed;

// a segment dedicated to store f2fs_sit_entry
struct f2fs_sit_block {
    struct f2fs_sit_entry entries[SIT_ENTRY_PER_BLOCK];
} __packed;

// total SIT sapce: (8+2+1)B * 4T/2M = 24MB
/*
 * common interface to identify whether block used or free;
 * and help quick search available Segments according to specified GC police.
 */
class SITManager : public MetaManager {
public:    
    SITManager(uint32_t total);
    virtual ~SITManager();
    int save(); // save SIT info from memroy to disk
    int load(); // load SIT info from disk to Memroy
    int format();
    void update_sit_map(uint32_t blkno, bool state);
private:
    //scoped_refptr<f2fs_sit_entry > sit_entries;
    uint32_t sit_total_num;
    f2fs_sit_entry * sit_entries;
    uint32_t sz;
    //uint32_t last_scaned_end_seg; // gc from here
    //uint32_t latest_touched_seg; // allocate from here, to added during GC
    //SITManager() {};
};

/*
 * For segment summary
 *
 * One summary block contains exactly 512 summary entries, which represents
 * exactly 2MB segment by default. Not allow to change the basic units.
 *
 * NOTE: For initializing fields, you must use set_summary
 *
 * - If data page, nid represents dnode's nid
 * - If node page, nid represents the node page's nid.
 *
 * The ofs_in_node is used by only data page. It represents offset
 * from node's page's beginning to get a data block address.
 * ex) data_blkaddr = (block_t)(nodepage_start_address + ofs_in_node)
 */

/* a summary entry for a 4KB-sized block in a segment, used to record every physical block's owner and its 
 * logical offset */
struct f2fs_summary {
    uint32_t nid;        /* parent node id */ // used nid to index which segment based on NAT, owner node id
    union {
        uint8_t reserved[3];
        struct {
            uint8_t version;        /* node version number */ // used for updated record counter?
            uint16_t ofs_in_node;    /* block index in parent node */
        } __packed;
    };
} __packed;

struct summary_footer {
    unsigned char entry_type;    /* SUM_TYPE_XXX */
    uint32_t check_sum;        /* summary checksum */
} __packed;

/* summary block type, node or data, is stored to the summary_footer */
#define SUM_TYPE_NODE        (1)
#define SUM_TYPE_DATA        (0)
#define SUM_JOURNAL_SIZE    (F2FS_BLKSIZE - SUM_FOOTER_SIZE -\
                SUM_ENTRY_SIZE)
#define NAT_JOURNAL_ENTRIES    ((SUM_JOURNAL_SIZE - 2) /\
                sizeof(struct nat_journal_entry))
#define NAT_JOURNAL_RESERVED    ((SUM_JOURNAL_SIZE - 2) %\
                sizeof(struct nat_journal_entry))
// 为何 -2 ?
#define SIT_JOURNAL_ENTRIES    ((SUM_JOURNAL_SIZE - 2) /\
                sizeof(struct sit_journal_entry))
#define SIT_JOURNAL_RESERVED    ((SUM_JOURNAL_SIZE - 2) %\
                sizeof(struct sit_journal_entry))

/* Reserved area should make size of f2fs_extra_info equals to
 * that of nat_journal and sit_journal.
 */
#define EXTRA_INFO_RESERVED    (SUM_JOURNAL_SIZE - 2 - 8)

/*
 * frequently updated NAT/SIT entries can be stored in the spare area in
 * summary blocks
 */
enum {
    NAT_JOURNAL = 0,
    SIT_JOURNAL
};

struct nat_journal_entry {
    uint32_t nid;
    struct f2fs_nat_entry ne;
} __packed;

struct nat_journal {
    struct nat_journal_entry entries[NAT_JOURNAL_ENTRIES];
    uint8_t reserved[NAT_JOURNAL_RESERVED];
} __packed;

// used to indicated now which blocks in one segment are valid
struct sit_journal_entry {
    uint32_t segno;
    struct f2fs_sit_entry se;
} __packed;

// a segment dedicated to store SIT entries
struct sit_journal {
    struct sit_journal_entry entries[SIT_JOURNAL_ENTRIES];
    uint8_t reserved[SIT_JOURNAL_RESERVED];
} __packed;

struct f2fs_extra_info {
    uint64_t kbytes_written;
    uint8_t reserved[EXTRA_INFO_RESERVED];
} __packed;

struct f2fs_journal {
    union {
        uint16_t n_nats;
        uint16_t n_sits;
    };
    /* spare area is used by NAT or SIT journals or extra info */
    union {
        struct nat_journal nat_j;
        struct sit_journal sit_j;
        struct f2fs_extra_info info;
    };
} __packed;

/* 4KB-sized summary block structure */
struct f2fs_summary_block {
    struct f2fs_summary entries[ENTRIES_IN_SUM]; // 512 entry * 7 bytes per entry ,used to recor block owner and logical offset
    struct f2fs_journal journal;
    struct summary_footer footer;
} __packed;

class BlockAllocator : public Lockable {
public:
        BlockAllocator(uint32_t start, uint32_t next) : start_blk(start) {
            next_blk.store(next, boost::memory_order_relaxed);
        }
        // we assume the next blknum block are all free in demo
        uint32_t alloc_free_blk(uint32_t blknum = 1) {
            return next_blk.fetch_add(blknum, boost::memory_order_relaxed);
        }
        uint32_t get_free_blk() {
            return next_blk.load(boost::memory_order_relaxed);
        }
private:
        uint32_t start_blk;
        boost::atomic<uint32_t> next_blk;
};

/* recorded all direct node address to Block Map in one i-node domain */
typedef std::map<uint32_t, direct_node *> DirectNodeBlkMap;
typedef std::pair<uint32_t, direct_node *> DirectNodeBlkPair;
/* recorded all indirect node address to Block Map in one i-node domain */
typedef std::map<uint32_t, indirect_node *> IndirectNodeBlkMap;
typedef std::pair<uint32_t, indirect_node *> IndirectNodeBlkPair;

// TODO: as a demo, we don't use direct node/indirect node cache to save frequent read dnode/indnode
// at same location , it mannager all sub node (dnode/indirect node / double indirect node)
// Every i_node/file has an  AddressTranslator
class AddressTranslator : public Lockable {
public:
    AddressTranslator(struct f2fs_inode * i, BlockAllocator * alloc) : inode(i), g_node_allocator(alloc) {
        i_size = inode->i_size;
    };
    ~AddressTranslator();
    int get_node_path(long block, int offset[4], unsigned int noffset[4]);
    //int update_node_entry(uint32_t phy_blk, int offset[4], unsigned int noffset[4]);
    int _lookup_and_update_entry(uint32_t phy_blk, long logical_blk, bool update);
    int update_node_entry(uint32_t phy_blk, long logical_blk);
    int lookup_node_entry(long logical_blk);
    //load_inode(uint32_t blk_index);
    void add_direct_node_map(uint32_t blk_index, direct_node * dnode);
    direct_node * load_direct_node(uint32_t blk_index);
    //indirect_node * load_indirect_node(uint32_t blk_index);
    indirect_node * load_indirect_node(unsigned int blk_index);
    void add_indirect_node_map(unsigned int blk_index, indirect_node * indnode);

    f2fs_inode * inode;
    boost::atomic<uint64_t> i_size;
private:
    DirectNodeBlkMap _dmap;
    IndirectNodeBlkMap _inmap;
    BlockAllocator * g_node_allocator;
};

}
}

#endif  //__APPEND_DEMO_LAYOUT_H_

/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */
