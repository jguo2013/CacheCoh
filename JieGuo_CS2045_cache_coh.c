#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "type.h"
#include <omp.h>

/* global variables */
int proc_num;           //processor number
unsigned int line_num;  //cache line number
unsigned int inst_line; //instruction lines
FILE *input_fp;         //input file pointer
FILE *output_fp;        //output file pointer

int rr_cnt;             //round robin counter
int pending_cmd;        //pending command
int depth_of_queue;     //depth_of_queue is equal to process number + 1 + 2
int bus_cyc;
int cmd_cpl;
inst_buf_t *inst_buf_array;

cache_line_t *cache_array;

req_queue_t *req_queue_array;

req_queue_t *rx_req_queue_array;

p_stat_t *p_stat_array;

void init_cache()
{
 char buffer[128];
 int i,j;
 
  pending_cmd = 0;
  rr_cnt = 0;
  
  fgets(buffer, sizeof(buffer), input_fp);
  sscanf(buffer, "%d%d%d\n", &proc_num,&line_num,&inst_line);
 
  line_num = (unsigned int)pow(2,(double)line_num);
 
  inst_buf_array = (inst_buf_t *)malloc(proc_num*(sizeof(inst_buf_t)));
  
  depth_of_queue = proc_num+1;
  
  for(i=0;i<=proc_num;i++)
    {
     inst_buf_array[i].cache_array = (cache_line_t *)malloc(line_num*(sizeof(cache_line_t)));
     inst_buf_array[i].state       = CMD_CPL; //initial state
     inst_buf_array[i].cyc         = 0; 
     for(j=0;j<line_num;j++)
     {
 	    inst_buf_array[i].cache_array[j].flag = 0;
 	    inst_buf_array[i].cache_array[j].tag = -1; 	    
     } 
    }
    
  req_queue_array = (req_queue_t *)malloc((proc_num+1)*(sizeof(req_queue_t)));
  for(i=0;i<=proc_num;i++)//include memory module
  {
 	 req_queue_array[i].num = 0;
 	 req_queue_array[i].wrp = 0; 
 	 req_queue_array[i].rdp = 0; 
 	 req_queue_array[i].req_array = (req_info_t *)malloc(depth_of_queue*(sizeof(req_info_t)));	
     for(j=0;j<=proc_num;j++)//include memory module
     {
 	    req_queue_array[i].req_array[j].priority = 0;//initial state: normal
     } 	 
  } 

  rx_req_queue_array = (req_queue_t *)malloc((proc_num+1)*(sizeof(req_queue_t)));
  for(i=0;i<=proc_num;i++)//include memory module
  {
 	 rx_req_queue_array[i].num = 0;
 	 rx_req_queue_array[i].wrp = 0; 
 	 rx_req_queue_array[i].rdp = 0; 
 	 rx_req_queue_array[i].req_array = (req_info_t *)malloc((2*depth_of_queue)*(sizeof(req_info_t)));	
     for(j=0;j<=proc_num;j++)//include memory module
     {
 	    rx_req_queue_array[i].req_array[j].priority = 0;//initial state: normal
     } 	 
  }
   
 p_stat_array = malloc(proc_num*(sizeof(p_stat_t)));
  for(i=0;i<proc_num;i++)
  {
 	 p_stat_array[i].total = 0;
 	 p_stat_array[i].miss = 0; 	 
  } 

}
 
 void parse_input()
 {
 	int i,j;
 	char buffer[128];
 	char *temp;
 	char temp_cmd_type;
 	int  loc;
 	temp = buffer;
 	
 	if(pending_cmd != 0)
 		{
  	 printf("the processor commands are not completed before fetching new commands\n");
  	 exit(-1); 			
 		}
 	
  if(!fgets(buffer, sizeof(buffer), input_fp))
   {
  	 printf("instruction is less than pre-set value\n");
  	 exit(-1);
   }
  else
   {
  	 for(i=0;i<proc_num;i++)
  	 {
  	 	loc = 0;
      sscanf(temp, "%d %c ", &(inst_buf_array[i].blk_adr), &temp_cmd_type);
      
      if(temp_cmd_type == '0')
      	{inst_buf_array[i].cmd_type = 0;}
      else if(temp_cmd_type == '1')
      	{inst_buf_array[i].cmd_type = 1;}
      	
      if(temp_cmd_type != '0' && temp_cmd_type != '1')
      	{
      	 printf("command type is not correct when fetching command\n");
      	 exit(-1);      		
      	}
      while(loc < 2 && i < (proc_num - 1))
      {
       if (temp[0] == ' ' || temp[0] == '\240')
       	{loc++;}
       temp++;	 	
      }
      
      pending_cmd++;
      if(inst_buf_array[i].state != CMD_CPL)//the command is not completed
      	{
      	 printf("the new comand is fetched before the previous command is finished\n");
      	 exit(-1);
      	}
      else
      	{
      	 if(LS_DEBUG_ON == 1){
      	 printf("the new comand of Processor %d is fetched \n", i);}      		
         inst_buf_array[i].state = CMD_READY;
        }         
	   }
   }
 }
 
 int generate_mem_instruction(int proc_id)//want data or not in wriite miss
 {
 	int          temp_cmd;
 	int          temp_adr;
 	int          wb_adr;
 	int          temp_tag;
 	int          temp_index;
 	int          temp_wpt;
 	 	
 	if(inst_buf_array[proc_id].state != CMD_READY)
 		{
     printf("generate new mem operation when inst_buf state is not CMD_READY\n");
     exit(-1); 			
 		}
 		
 	if(req_queue_array[proc_id].num != 0)
 		{
     printf("generate new mem operation when req queue is not empty\n");
     exit(-1); 			
 		}
 		 		
 	temp_cmd   = inst_buf_array[proc_id].cmd_type;
 	temp_adr   = inst_buf_array[proc_id].blk_adr;
 	temp_tag   = temp_adr/line_num;
 	temp_index = (temp_adr)%(line_num); 
 	inst_buf_array[proc_id].state = CMD_CPL;
 	p_stat_array[proc_id].total = p_stat_array[proc_id].total+1;
  temp_wpt   = req_queue_array[proc_id].wrp;
  
 	switch(temp_cmd)
 	 {
 	 	case 0: //read
 	  {
 	   if(inst_buf_array[proc_id].cache_array[temp_index].tag != temp_tag &&
 		    inst_buf_array[proc_id].cache_array[temp_index].flag == CACHE_EXC)//read miss & write back
 		   {
 		   	wb_adr = (inst_buf_array[proc_id].cache_array[temp_index].tag)*line_num +
 		   	          temp_index; 
 		   	inst_buf_array[proc_id].cache_array[temp_index].flag = CACHE_INVLD; 
 		   	if(LS_DEBUG_ON == 1){
 		    printf("Processor %d: block: %d -> INVALID\n", proc_id,wb_adr);}	  		   			   	 		   	          
 		   	req_queue_array[proc_id].req_array[temp_wpt].src_id  = proc_id;
 		   	req_queue_array[proc_id].req_array[temp_wpt].blk_adr = wb_adr;
 		   	req_queue_array[proc_id].req_array[temp_wpt].opt_type = WB;
 		   	req_queue_array[proc_id].req_array[temp_wpt].priority = 0;
 		   	if(req_queue_array[proc_id].wrp == depth_of_queue-1)
 		   		{req_queue_array[proc_id].wrp = 0;temp_wpt=0;}
 		    else
 		   		{req_queue_array[proc_id].wrp++;temp_wpt++;} 		    	
 		   	req_queue_array[proc_id].num++;
 		   	if(LS_DEBUG_ON == 1){
 		    printf("Processor %d: block: %d write back for read miss\n", proc_id,wb_adr);}	   		
 		   } 	
 	  	
 	   if(inst_buf_array[proc_id].cache_array[temp_index].flag == CACHE_INVLD ||
 		   (inst_buf_array[proc_id].cache_array[temp_index].tag != temp_tag &&
 		    inst_buf_array[proc_id].cache_array[temp_index].flag != CACHE_INVLD))//read miss
 		   {
 		   	req_queue_array[proc_id].req_array[temp_wpt].src_id  = proc_id;
 		   	req_queue_array[proc_id].req_array[temp_wpt].blk_adr = temp_adr;
 		   	req_queue_array[proc_id].req_array[temp_wpt].opt_type = RRS;
 		   	req_queue_array[proc_id].req_array[temp_wpt].priority = 0;
 		   	if(req_queue_array[proc_id].wrp == depth_of_queue-1)
 		   		{req_queue_array[proc_id].wrp = 0;}
 		    else
 		   		{req_queue_array[proc_id].wrp++;}
 		   	req_queue_array[proc_id].num++;			   		
      	p_stat_array[proc_id].miss = p_stat_array[proc_id].miss+1;
      	if(LS_DEBUG_ON == 1){
 		    printf("Processor %d: block: %d read miss\n", proc_id,inst_buf_array[proc_id].blk_adr);}	   		 		   	
 		   }
 		 if(req_queue_array[proc_id].num >= depth_of_queue)
 		  {
       printf("queue is full when generating of new mem opt\n");
       exit(-1); 			
 		  } 
 		  
 	   if(inst_buf_array[proc_id].cache_array[temp_index].tag == temp_tag &&
 		    inst_buf_array[proc_id].cache_array[temp_index].flag != CACHE_INVLD)//read hit
 		   {
 		   	if(LS_DEBUG_ON == 1){
 		   	printf("Processor %d: block: %d read hit\n", proc_id,inst_buf_array[proc_id].blk_adr);
 		    }	   		 		   	 		   	
 		   	inst_buf_array[proc_id].state = CMD_CPL;
 		   	bus_cyc++;
        fprintf(output_fp,"C:%d P:action     THD:-1 CMD:NULL      ADDR:-1 PRI:normal\n",bus_cyc);
        fprintf(output_fp,"C:%d P:reaction     THD:-1 CMD:NULL   ADDR:-1 PRI:normal\n",bus_cyc); 		   	 
 		   	inst_buf_array[proc_id].cyc = bus_cyc;		   	
 		   	return(1);
 		   }
 		 else
 		 	 {
 		   	inst_buf_array[proc_id].state = CMD_IN_Q; 		 	 	
 		 	 	return(0);
 		 	 } 		 	 
 		 break;
 		}
 	 	case 1: 
 	  {
 	   if(inst_buf_array[proc_id].cache_array[temp_index].tag != temp_tag &&
 		    inst_buf_array[proc_id].cache_array[temp_index].flag != CACHE_INVLD)//write miss
 		   {
 		   	wb_adr = (inst_buf_array[proc_id].cache_array[temp_index].tag)*line_num +
 		   	          temp_index;
 		   	inst_buf_array[proc_id].cache_array[temp_index].flag = CACHE_INVLD; 
 		   	if(LS_DEBUG_ON == 1){
 		    printf("Processor %d: block: %d -> INVALID\n", proc_id,wb_adr);}	 		   	           
 		   	req_queue_array[proc_id].req_array[temp_wpt].src_id  = proc_id;
 		   	req_queue_array[proc_id].req_array[temp_wpt].blk_adr = wb_adr;
 		   	req_queue_array[proc_id].req_array[temp_wpt].opt_type = WB;
 		   	req_queue_array[proc_id].req_array[temp_wpt].priority = 0;
 		   	if(req_queue_array[proc_id].wrp == depth_of_queue-1)
 		   		{req_queue_array[proc_id].wrp = 0;temp_wpt=0;}
 		    else
 		   		{req_queue_array[proc_id].wrp++;temp_wpt++;}
 		   	req_queue_array[proc_id].num++;
 		   	if(LS_DEBUG_ON == 1){ 		   	
 		    printf("Processor %d: block %d write back\n", proc_id,wb_adr);
 		    }	   		 		   		
 		   } 	
 	   if(inst_buf_array[proc_id].cache_array[temp_index].tag != temp_tag &&
 		    inst_buf_array[proc_id].cache_array[temp_index].flag == CACHE_SHD)//write miss
       { 
 		   	wb_adr = (inst_buf_array[proc_id].cache_array[temp_index].tag)*line_num +
 		   	          temp_index; 
 		   	inst_buf_array[proc_id].cache_array[wb_adr].flag == CACHE_INVLD;                 			   	
       	if(LS_DEBUG_ON == 1){ 		   	
 		    printf("Processor %d: block %d -> INVALID\n", proc_id,wb_adr);}
 		   } 	  	
 	   if(inst_buf_array[proc_id].cache_array[temp_index].flag == CACHE_INVLD ||
 		   (inst_buf_array[proc_id].cache_array[temp_index].tag != temp_tag &&
 		    inst_buf_array[proc_id].cache_array[temp_index].flag != CACHE_INVLD))//write miss
 		   {
 		   	req_queue_array[proc_id].req_array[temp_wpt].src_id  = proc_id;
 		   	req_queue_array[proc_id].req_array[temp_wpt].blk_adr = temp_adr;
 		   	req_queue_array[proc_id].req_array[temp_wpt].opt_type = RRM_D;
 		   	req_queue_array[proc_id].req_array[temp_wpt].priority = 0;
 		   	if(req_queue_array[proc_id].wrp == depth_of_queue-1)
 		   		{req_queue_array[proc_id].wrp = 0;temp_wpt = 0;}
 		    else
 		   		{req_queue_array[proc_id].wrp++;temp_wpt++;}
 		   	req_queue_array[proc_id].num++;	 		  
 		   	p_stat_array[proc_id].miss = p_stat_array[proc_id].miss+1;
 		   }
 		   
 		 if(LS_DEBUG_ON == 1 && 
 		 	 (inst_buf_array[proc_id].cache_array[temp_index].tag != temp_tag ||
 		    inst_buf_array[proc_id].cache_array[temp_index].flag == CACHE_INVLD)){
 		  printf("Processor %d: block: %d write miss\n", proc_id,inst_buf_array[proc_id].blk_adr);}	   		
     	
 	   if(inst_buf_array[proc_id].cache_array[temp_index].tag == temp_tag &&
 		    inst_buf_array[proc_id].cache_array[temp_index].flag == CACHE_SHD)//write hit
 		   {
        inst_buf_array[proc_id].cache_array[temp_index].flag = CACHE_EXC; 		   	
 		   	req_queue_array[proc_id].req_array[temp_wpt].src_id  = proc_id;
 		   	req_queue_array[proc_id].req_array[temp_wpt].blk_adr = temp_adr;
 		   	req_queue_array[proc_id].req_array[temp_wpt].opt_type = RRM;
 		   	req_queue_array[proc_id].req_array[temp_wpt].priority = 0;
 		   	if(req_queue_array[proc_id].wrp == depth_of_queue-1)
 		   		{req_queue_array[proc_id].wrp = 0;}
 		    else
 		   		{req_queue_array[proc_id].wrp++;}
 		   	req_queue_array[proc_id].num++;	
 		   	if(LS_DEBUG_ON == 1){
 		    printf("Processor %d: block: %d write hit\n", proc_id,inst_buf_array[proc_id].blk_adr);}	 
 		   } 	
 		   
 		 if(req_queue_array[proc_id].num > depth_of_queue)
 		  {
       printf("queue is full when generating of new mem opt\n");
       exit(-1); 			
 		  } 	
     
 	   if(inst_buf_array[proc_id].cache_array[temp_index].tag == temp_tag &&
 		    inst_buf_array[proc_id].cache_array[temp_index].flag != CACHE_INVLD)//write hit
 		   {
 		   	inst_buf_array[proc_id].state = CMD_CPL; 
 		   	bus_cyc++;
        fprintf(output_fp,"C:%d P:action     THD:-1 CMD:NULL      ADDR:-1 PRI:normal\n",bus_cyc);
        fprintf(output_fp,"C:%d P:reaction     THD:-1 CMD:NULL   ADDR:-1 PRI:normal\n",bus_cyc);  		   		
 		   	inst_buf_array[proc_id].cyc = bus_cyc;	   	
 		   	return(1);
 		   }
 		 else
 		 	 {
 		   	inst_buf_array[proc_id].state = CMD_IN_Q; 		 	 	
 		 	 	return(0);
 		 	 }
 		 	  		  	   	   
 		 break;
 		}
 	 } 		
 }

 int arbitrate_bus(int proc_id)
 {
 	int          rr_sel = rr_cnt;
 	int          sel_id = rr_cnt;
 	int          sel_pri = 0;
 	int          sel_vld = 0;
 	int          i,cnt;
 	int          des_id;
  
  int          temp_id;
	int          temp_opt;
 	int          temp_adr;
 	int          temp_tag;
 	int          temp_index;
 	int          find_des = 0;//1:data in caches of process
 	int          temp_rdp;
 	int          temp_wdp;
 	int          temp_cyc;
 	 	
 	for(i=0;i<=proc_num;i++)
 	{
   if(req_queue_array[rr_sel].num !=0)
   	{
   	 temp_rdp = req_queue_array[rr_sel].rdp;
     if(sel_vld == 0)
     	{
   	   sel_id = rr_sel;
   	   sel_vld = 1;
   	   if(req_queue_array[rr_sel].req_array[temp_rdp].priority == 1)
   	   	{
   	   	 sel_pri = 1;
   	   	}
   	  }
   	 else if(sel_pri == 0 &&
   	 	       req_queue_array[rr_sel].req_array[temp_rdp].priority == 1)
   	 	{
   	   sel_id = rr_sel;  
   	   sel_pri = 1; 	 	 
   	 	}
   	} 
   if(rr_sel == proc_num) {rr_sel = 0;}
   else {rr_sel = rr_sel + 1;}   			
 	}
 	
 	if(rr_cnt == proc_num) {rr_cnt = 0;}
 	else {rr_cnt++;}	
 		
 	if(sel_id != proc_id || sel_vld == 0)
 		{
     return(0); 			
 		}
 	if(LS_DEBUG_ON == 1){ 		
  printf("Processor %d is selected, RR register is incremented to %d\n", sel_id,rr_cnt);}
  
  temp_rdp   = req_queue_array[sel_id].rdp;	
 	temp_id    = req_queue_array[sel_id].req_array[temp_rdp].des_id;	 		
 	temp_opt   = req_queue_array[sel_id].req_array[temp_rdp].opt_type ;
 	temp_adr   = req_queue_array[sel_id].req_array[temp_rdp].blk_adr;
 	temp_cyc   = req_queue_array[sel_id].req_array[temp_rdp].cyc;
 	temp_tag   = temp_adr/line_num;
 	temp_index = (temp_adr)%(line_num); 
 	
 	if(req_queue_array[proc_id].num == 0)
 	 {
    printf("queue is empty when reading new mem opt\n");
    exit(-1); 			
 	 }
 	
 	if(req_queue_array[sel_id].rdp == depth_of_queue-1)
  	{req_queue_array[sel_id].rdp = 0;}
  else
  	{req_queue_array[sel_id].rdp++;}
  	
 	(req_queue_array[sel_id].num)--; 	
 	
 	des_id = sel_id;
 	if(sel_id ==proc_num){cnt = proc_num;}
 	else {cnt = proc_num-1;}
 	for(i=0;i<cnt;i++)
 	 {
 	  if(des_id == 0){des_id = proc_num-1;}
 	  else {des_id = des_id - 1;}
 	 	
 	 	   switch(temp_opt)
 	 	   {
 	 	   case RRS:
 	 	    {
         if(inst_buf_array[des_id].cache_array[temp_index].tag  == temp_tag &&
 	 	   		  inst_buf_array[des_id].cache_array[temp_index].flag == CACHE_EXC) 
 	 	       {
 	 	       	inst_buf_array[des_id].cache_array[temp_index].flag = CACHE_SHD;
 	         if(LS_DEBUG_ON == 1){   	
           printf("Processor %d receive request RRS from Processor %d(E)\n", des_id, sel_id);}
           fprintf(output_fp,"C:%d P:action     THD:%d CMD:read      ADDR:%d PRI:normal\n",bus_cyc,sel_id,temp_adr); 
           fprintf(output_fp,"C:%d P:reaction     THD:%d CMD:read      ADDR:%d PRI:normal\n",bus_cyc,des_id,temp_adr); 
           bus_cyc++;          	 
 	 	       }		 	       
 	       break; 	      
 	 	    }
 	 	   case RRM: case RRM_D://modify check src state
 	 	    {
 	 	   	 if(inst_buf_array[des_id].cache_array[temp_index].tag  == temp_tag &&
 	 	   		  inst_buf_array[des_id].cache_array[temp_index].flag == CACHE_EXC) //hit & exclusive or shared
 	 	       {
 	 	       	inst_buf_array[des_id].cache_array[temp_index].flag = CACHE_INVLD;       			
            fprintf(output_fp,"C:%d P:action     THD:%d CMD:read      ADDR:%d PRI:normal\n",bus_cyc,sel_id,temp_adr);
            fprintf(output_fp,"C:%d P:reaction     THD:%d CMD:reply   ADDR:%d PRI:normal\n",bus_cyc,des_id,temp_adr);
 	          if(LS_DEBUG_ON == 1){
 	          	if(temp_opt == RRM_D)printf("Processor %d receive request RRM_D from Processor %d\n", des_id, sel_id);
 	          	else {printf("Processor %d receive request RRM_D from Processor %d\n", des_id, sel_id);}}
 	          bus_cyc++; 	         	 	          	      	     	
 	 	       }
 	 	   	else if(inst_buf_array[des_id].cache_array[temp_index].tag  == temp_tag &&
 	 	   		      inst_buf_array[des_id].cache_array[temp_index].flag == CACHE_SHD) //hit & exclusive or shared
 	 	       {
 	 	       	inst_buf_array[des_id].cache_array[temp_index].flag = CACHE_INVLD;	 	       	 	 	      			
            fprintf(output_fp,"C:%d P:action     THD:%d CMD:read      ADDR:%d PRI:normal\n",bus_cyc,sel_id,temp_adr);
            fprintf(output_fp,"C:%d P:reaction     THD:%d CMD:reply   ADDR:%d PRI:normal\n",bus_cyc,des_id,temp_adr);             		 	      	
 	          if(LS_DEBUG_ON == 1){
 	          	if(temp_opt == RRM_D)printf("Processor %d receive request RRM_D from Processor %d\n", des_id, sel_id);
 	          	else {printf("Processor %d receive request RRM_D from Processor %d\n", des_id, sel_id);}}
 	          bus_cyc++; 	 	         	 	          	      	     	
 	 	       } 	 	       
 	 	     break;
 	 	    }
 	 	   case WB: 
 	 	    {
 	 	     if(find_des == 0)
 	 	     	{
          fprintf(output_fp,"C:%d P:action     THD:%d CMD:write      ADDR:%d PRI:normal\n",bus_cyc,sel_id,temp_adr);
          fprintf(output_fp,"C:%d P:reaction     THD:%d CMD:reply      ADDR:%d PRI:normal\n",bus_cyc,proc_num,temp_adr);              		        
 	        find_des = 1;  	 
 	        if(LS_DEBUG_ON == 1){	    
           printf("Processor %d receive request WB from Processor %d\n", proc_num, sel_id);}
          bus_cyc++; 	
          } 	  	       
 	       break;
 	 	    } 	 	   
 	 	   } 
 	 	   if(rx_req_queue_array[des_id].num > depth_of_queue)
 		   {
        printf("queue is full when generating of new mem opt\n");
        exit(-1);        
 		   } 	  	 	    
 	 }
 	if(temp_opt == RRS || temp_opt == RRM_D)//RRS or RRM 
 		{
     fprintf(output_fp,"C:%d P:action     THD:%d CMD:read      ADDR:%d PRI:normal\n",bus_cyc,sel_id,temp_adr);
     fprintf(output_fp,"C:%d P:reaction     THD:%d CMD:reply      ADDR:%d PRI:high\n",bus_cyc,proc_num,temp_adr);
     inst_buf_array[sel_id].cache_array[temp_index].tag  = temp_tag;
     bus_cyc++; 	
 	   if(temp_opt == RRM_D)     
 	 	 {inst_buf_array[sel_id].cache_array[temp_index].flag = CACHE_EXC;}
 	 	 else
 	 	 {inst_buf_array[sel_id].cache_array[temp_index].flag = CACHE_SHD;}
 	 	  	 	 	
 	   if(LS_DEBUG_ON == 1)
 	   	{
 	     printf("Processor %d receive request RRM_D/RRS from Processor %d\n", proc_num, sel_id);
 	   	} 	  	       
 	  }  
 	if(temp_opt == RRS || temp_opt == RRM_D){cmd_cpl = 1;} 
 	else {cmd_cpl = 0;}	
 	return(1);  		
 } 

 
 void print_cache_stat()
 {
 	int i,j ;
 	int temp_flag;
 	int temp_tag;
 	int temp_adr;
 	int temp_index;
  
  if(LS_DEBUG_ON == 1){
 	for(i=0;i<proc_num;i++)
 	{
 	 printf("cache state of processor %d is:\n",i);
 	 if(inst_buf_array[i].cmd_type == 0)
 	 	{
 	   printf("read block %d  \n",inst_buf_array[i].blk_adr);
 	   fprintf(output_fp,"read block %d  \n",inst_buf_array[i].blk_adr);	   
 	  }
 	 else
 	 	{
 	   printf("write block %d  \n",inst_buf_array[i].blk_adr);
 	   fprintf(output_fp,"write block %d  \n",inst_buf_array[i].blk_adr);	   
 	  } 	
 	   	
	temp_adr   = inst_buf_array[i].blk_adr;
 	temp_tag   = temp_adr/line_num;
 	temp_index = (temp_adr)%(line_num); 
 	
 	if(temp_tag != inst_buf_array[i].cache_array[temp_index].tag)
 		{
 		 printf("tag is detected not correct when print cache state\n");
 		 exit(-1);
 		}
 
 	 switch(inst_buf_array[i].cache_array[temp_index].flag)
 	  {
 	   case CACHE_INVLD :
 	   	{
 	     printf("state: invalid\n\n");
 	     fprintf(output_fp,"state: invalid\n\n");
 	     break;
 	    }
 	   case CACHE_EXC :
 	   	{
 	     printf("state: exclusive\n\n");
 	     fprintf(output_fp,"state: exclusive\n\n");
 	     break;
 	    } 
 	   case CACHE_SHD :
 	   	{
 	     printf("state: shared\n\n");
 	     fprintf(output_fp,"state: shared\n\n");
 	     break;
 	    }
 	  } 	     	    	    
 	}}
	
 	 if(proc_num <= 4 && inst_line <= 3)
 	 	{
 	    fprintf(output_fp,"cache state:\n "); 	 		
 	 		for(j=0;j<line_num;j++)
 	 		{
       for(i=0;i<proc_num;i++)
 	     {
 	     	if(inst_buf_array[i].cache_array[j].tag != -1){temp_tag = (inst_buf_array[i].cache_array[j].tag*line_num)+j;}
 		    else {temp_tag = inst_buf_array[i].cache_array[j].tag;}
 		    temp_flag = inst_buf_array[i].cache_array[j].flag;
 		    if(j==0 && i==0 && temp_flag == 0){temp_tag = -1;} 		    
 	      fprintf(output_fp,"%d ",temp_tag);	 
 	      switch(temp_flag)
 	       {
 	        case CACHE_INVLD:
 	         {  
 	          fprintf(output_fp,"I, ");	
 	          break;
 	         }
 	        case CACHE_SHD:
 	         {  
 	          fprintf(output_fp,"S, ");
 	          break;
 	         }
 	        case CACHE_EXC:
 	         {  
 	          fprintf(output_fp,"E, ");
 	          break;
 	         } 
 	        default:
 	         {  
 	          printf("state of cache line %d is wrong type\n",j);
 	          exit(-1);	
 	         } 	         	          	           
 	       }
 	     }
 	    fprintf(output_fp,"\n");	 
 	    } 	 		
 	 	}
 	 	
   fprintf(output_fp,"\n");	 	
 	 	 	
 }
 
 void cache_sim()
 {
 	int i;
 	int j;
 	
 	bus_cyc = 0;
  for(i=0;i<inst_line;i++)
   {
    parse_input();
    
    while(pending_cmd) //command is not completed
    {
     if(LS_DEBUG_ON == 1){
       printf("line %d is pending\n",i);
       if(i == 2012)
       {bus_cyc = 0;}} 

   //# pragma omp parallel for num_threads(proc_num+1) shared(bus_cyc,pending_cmd,inst_buf_array,req_queue_array) private(cmd_cpl,j)             	
     for(j=0;j<=proc_num;j++)//include mem
     {
     	cmd_cpl = 0;
     	if(inst_buf_array[j].state == CMD_READY && j != proc_num)
     	 {
        cmd_cpl = generate_mem_instruction(j);
       }   
         
      if(inst_buf_array[j].state == CMD_IN_Q)
      	{
      	 inst_buf_array[j].state = CMD_SENT;      	 
       	}   
     # pragma omp critical
     {  	
      pending_cmd=pending_cmd-cmd_cpl;
     }
            	   
      if(LS_DEBUG_ON == 1){
      printf("pending_cmd is equal to %d on round %d\n",pending_cmd,j);} 
           		
    # pragma omp critical
     {
       while(arbitrate_bus(j)){pending_cmd=pending_cmd-cmd_cpl;
       if(cmd_cpl == 1){
       	if(i == inst_line -1)
       		{fprintf(output_fp,"THD:%d completed\n",j);}
         inst_buf_array[j].state = CMD_CPL;
         inst_buf_array[j].cyc = bus_cyc;
       }
      }
     }
          # pragma omp barrier    
     }
    }
    print_cache_stat();
   }
 }
  
 void print_p_stat()
 {
 	int i;
 	double miss_rate;
  
  fprintf(output_fp,"completed cycle:");
 	for(i=0;i<proc_num;i++)
 	 {
    fprintf(output_fp,"%d,",inst_buf_array[i].cyc);
   }	
  fprintf(output_fp,"\n");
  
  fprintf(output_fp,"cache hits:");
 	for(i=0;i<proc_num;i++)
 	 {
    fprintf(output_fp,"%d,",p_stat_array[i].total-p_stat_array[i].miss);
   }	
  fprintf(output_fp,"\n");

  fprintf(output_fp,"cache misses:");
 	for(i=0;i<proc_num;i++)
 	 {
    fprintf(output_fp,"%d,",p_stat_array[i].miss);
   }	
  fprintf(output_fp,"\n");
       
  fprintf(output_fp,"cache miss rate:");
 	for(i=0;i<proc_num;i++)
 	 {
 	 	miss_rate = (double)(p_stat_array[i].miss)/(double)(p_stat_array[i].total);
    fprintf(output_fp,"%lf,",miss_rate);
   }	
  fprintf(output_fp,"\n\n");
 }
 
/*argv[1]: input; [2]: output*/
int main (int argc, char **argv)
{ 
  input_fp  = fopen(argv[1],"r");
  output_fp = fopen(argv[2],"w");  
   
  init_cache();//initialize cache
  
  cache_sim();//memory access
  
  print_p_stat();
  printf("simulation is successful!\n");
  
  fclose(input_fp);      
  fclose(output_fp);
  
  exit(0);
}
