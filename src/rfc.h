#ifndef DP_RFC_H
#define DP_RFC_H

#define UINT8_ALL_BIT_SET   0xFF
#define UINT16_ALL_BIT_SET  0xFFFF
#define UINT32_ALL_BIT_SET  0xFFFFFFFF
#define UINT64_ALL_BIT_SET  0xFFFFFFFFFFFFFFFF

#define MAXFILTERS	    2048	// maximum amount of filters
#define LENGTH		    32	    //length of unsigned int by bit
#define SIZE		    (MAXFILTERS/LENGTH)	   // SIZE = ceiling ( rules / LENGTH )
//#define EQS_SIZE	    5000
#define EQS_SIZE	    50000
#define P0_CELL_ENTRY_SIZE  65536
#define P1_CELL_ENTRY_SIZE  600000
//#define P1_CELL_ENTRY_SIZE  2000000
//#define P1_CELL_ENTRY_SIZE  10000000
#define P2_CELL_ENTRY_SIZE  P1_CELL_ENTRY_SIZE

#define RFC_INVALID_RULE_ID 65535
#define RFC_INVALID_IP_MAP  65535
#define RFC_DIM_VALUE       65535
#define BYTE_MAX_VALUE      255
#define IPV6_MAX_BITS       128
#define IPV6_HALF_MAX_BITS  64
#define IPV4_MAX_BITS       32
#define RFC_UINT32_ALL_BIT_SET  UINT32_ALL_BIT_SET
#define RFC_INVALID_IP_MASK_LEN 0xFF
/***************************************************************************/
typedef enum dim_e
{
    DIM_HI_SIP = 0,
    DIM_LO_SIP = 1,
    DIM_HI_DIP = 2,
    DIM_LO_DIP = 3,
    DIM_SPORT  = 4,
    DIM_DPORT  = 5,
    DIM_PROTO  = 6,
    DIM_VID    = 7,
    DIM_MAX    = 8,              // dim p0 max
}dim_t;   

typedef enum dim_v6_ip_e
{
    DIM_V6_SIP_MAP = 0,
    DIM_V6_DIP_MAP = 1,
    DIM_V6_IP_MAX  = 2,
}dim_v6_ip_t;   

typedef enum dim_v6_nip_e
{
    DIM_V6_SPORT   = 0,
    DIM_V6_DPORT   = 1,
    DIM_V6_PROTO   = 2,
    DIM_V6_VID     = 3,
    DIM_V6_NIP_MAX = 4,
}dim_v6_nip_t;

#define DIM_P1_MAX              2	
#define DIM_P0_MAX              DIM_MAX 
#define DIM_V6_P1_MAX           2	
#define DIM_V6_P0_IP_MAX        DIM_V6_IP_MAX
#define DIM_V6_P0_NIP_MAX       DIM_V6_NIP_MAX
#define IP_DIM_P0_MAX           2
/* rfc rule end point type */
typedef enum end_point_e
{		
    EP_START = 0,			/* low limit */
    EP_END   = 1,			/* high limit */
    EP_MAX   = 2,    
}end_point_t;

/** IP Addr Definition, applied for both v4 and v6 */
typedef union{
    struct{
        uint32_t reserved;
        uint32_t addr_v4;
    };
    struct{
        uint64_t addr_v6_upper;
        uint64_t addr_v6_lower;
    };
    uint64_t value[2];
}rfc_ip_addr_t;

//structures for filters...
typedef struct filter_s						
{
    uint32_t acl_rule_id;	/* acl rule_id*/
    uint32_t dim[DIM_MAX][EP_MAX];	
}filter_t;

typedef struct filtset_s
{
    uint32_t n_filts;
    filter_t filt_arr[MAXFILTERS];	
}filtset_t;

//structures for filters...
typedef struct ipv6_filter_s						
{
    uint32_t      acl_rule_id; /* acl rule_id*/
    uint32_t      nip_dim[DIM_V6_NIP_MAX][EP_MAX];	
    rfc_ip_addr_t ip_dim[DIM_V6_IP_MAX][EP_MAX];  /* dim[0] for sip  dim[1] for dip */
}ipv6_filter_t;

typedef struct ipv6_filtset_s
{
    uint32_t      n_filts;
    ipv6_filter_t filt_arr[MAXFILTERS];	
}ipv6_filtset_t;

typedef struct filt2rule_s
{
    uint32_t n_filt;
    uint32_t filt[MAXFILTERS];
}filt2rule_t;

typedef struct filt_end_point_s
{
    uint32_t      value;
    uint32_t      filt_id;
    int8_t        end_flag;	    /* 1:start, -1:end */
    rfc_ip_addr_t ip;
}filt_end_point_t;


typedef struct ce_s
{
    uint16_t eqid;	  /* 2 byte, eqid */
    uint32_t cbm[SIZE];   /* LENGTH*SIZE bits, CBM */
}ce_t;

typedef struct list_eq_s
{
    uint16_t n_ces;	    /* number of ces*/
    ce_t     ces[EQS_SIZE];
}list_eq_t;

typedef struct ip_map_s
{
    rfc_ip_addr_t start_ip;
    uint16_t      range_id;
}ip_map_t;

typedef struct ip_map_table_s
{
    uint16_t n_map;
    ip_map_t map_arr[2*MAXFILTERS+1];
}ip_map_table_t;

/* structure for Phase0 node */
typedef struct pnode_s
{
    uint16_t cell[P0_CELL_ENTRY_SIZE]; /* each cell stores an eqid */
    uint16_t n_ces;
}pnode_t;

/* structure for Phase1 & Phase2 node */
typedef struct pnoder_s
{
    uint32_t ncells;	    /* number of valid cells */
    uint16_t cell[P1_CELL_ENTRY_SIZE];   /* ???? need to protect  */
    uint16_t n_ces;
}pnoder_t;

typedef struct rfc_5tuple_s
{
    uint8_t       sip_mask; /* mask len range  24 :  255.255.255.0*/
    uint8_t       dip_mask;
    uint8_t       start_proto;
    uint8_t       end_proto;
    uint32_t      start_sport;
    uint32_t      end_sport;
    uint32_t      start_dport;
    uint32_t      end_dport;
    uint32_t      start_vid;
    uint32_t      end_vid;
    rfc_ip_addr_t start_sip;
    rfc_ip_addr_t end_sip;
    rfc_ip_addr_t start_dip;
    rfc_ip_addr_t end_dip;
    rfc_ip_addr_t sip;
    rfc_ip_addr_t dip;
}rfc_5tuple_t;

typedef struct rfc_node_s
{
    filt2rule_t  f2r;
    pnode_t      p0_node[DIM_P0_MAX];
    pnoder_t     p1_node[DIM_P1_MAX];
    pnoder_t     p2_node;
}rfc_node_t;

typedef struct rfc_single_ip_node_s
{
    filt2rule_t  f2r;
    pnode_t      p0_node[IP_DIM_P0_MAX];
    pnoder_t     p1_node;
}rfc_single_ip_node_t;

/* structure for Phase0 node */
typedef struct pnode_map_s
{
    uint16_t       cell[P0_CELL_ENTRY_SIZE]; /* each cell stores an eqid */
    ip_map_table_t imt;
    uint16_t       n_ces;
}pnode_map_t;

typedef struct rfc_map_node_s
{
    filt2rule_t f2r;
    pnode_map_t p0_map_node[DIM_V6_P0_IP_MAX];
    pnode_t     p0_node[DIM_V6_P0_NIP_MAX];
    pnoder_t    p1_node[DIM_V6_P1_MAX];
    pnoder_t    p2_node;
}rfc_map_node_t;


typedef struct _vlib_rfc_main_t {
#define RFC_IPSET_MAX_ID	1024
	/** RFC node for quick search. */
	rfc_node_t search;
	list_eq_t  pnode_eqs[DIM_P0_MAX];
	list_eq_t  pnoder_eqs[DIM_P1_MAX];
	rfc_5tuple_t ipset[RFC_IPSET_MAX_ID];
}vlib_rfc_main_t;

extern vlib_rfc_main_t vlib_rfc_main;

#endif
