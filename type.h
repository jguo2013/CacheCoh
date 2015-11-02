#define LS_DEBUG_ON 0

#define BYTES_PER_INT 2
#define NO_OP 0
#define RRS   1
#define RRM   2
#define WB    3
#define D     4 
#define RRM_D 5
#define B_REQ 6

#define NO_CMD      0
#define CMD_READY   1
#define CMD_IN_Q    2
#define CMD_SENT    3
#define CMD_CPL     4 

#define CACHE_INVLD 0
#define CACHE_SHD   1
#define CACHE_EXC   2

typedef struct cache_line {
   int             tag;       //tag = block address / cache number
   int             flag;      //0: invalid 1: shared 2: exclusive
} cache_line_t;

typedef struct inst_buf {
   int             cmd_type;  //operation type: 1: write 0:read
   int             blk_adr;   //block address
   int             cyc;       //excution cycle
   int             state;     //0: no command; 1: command ready 2:command in queue; 3: wait for request block 4: complete
   cache_line_t   *cache_array;
} inst_buf_t;

typedef struct req_info {
   int             src_id;    
   int             des_id;    
   int             blk_adr;   
   int             opt_type;  //0: non opt; 1: RRS; 2: RRM 3:WB; 4: D; 5: RRM_D 6: B_REQ
   int             priority;  //0: normal 1: priority
   int             cyc;   
} req_info_t;

typedef struct req_queue {
   int             wrp;      //write pointer       
   int             rdp;      //read  pointer
   int             num;
   req_info_t     *req_array;
   req_info_t     *rx_dat_array;
} req_queue_t;

typedef struct p_stat {
   unsigned int    total ;   //total cache access times per processor
   unsigned int    miss;     //total cache miss times per processor
} p_stat_t;


