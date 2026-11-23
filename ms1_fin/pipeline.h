#ifndef __PIPELINE_H__
#define __PIPELINE_H__

#include "config.h"
#include "types.h"
#include "cache.h"
#include <stdbool.h>

///////////////////////////////////////////////////////////////////////////////
/// Functionality
///////////////////////////////////////////////////////////////////////////////

extern simulator_config_t sim_config;
extern uint64_t miss_count;
extern uint64_t hit_count;
extern uint64_t total_cycle_counter;
extern uint64_t stall_counter;
extern uint64_t branch_counter;
extern uint64_t fwd_exex_counter;
extern uint64_t fwd_exmem_counter;
extern uint64_t mem_access_counter;

///////////////////////////////////////////////////////////////////////////////
/// RISC-V Pipeline Register Types
///////////////////////////////////////////////////////////////////////////////

typedef struct
{
  Instruction instr;
  uint32_t    instr_addr;
  uint32_t    instr_bits; //Raw instruction bits for debugging
  /**
   * Add other fields here
   */
}ifid_reg_t;

typedef struct
{
  Instruction instr;
  uint32_t    instr_addr;
  uint32_t    instr_bits;  //Raw instruction bits for debugging
  //Register values read from register file
  uint32_t    rs1_data;
  uint32_t    rs2_data;
  // Register indices
  uint32_t    rs1_idx;
  uint32_t    rs2_idx;
  uint32_t    rd_idx;
  //Immediate value
  uint32_t    immediate;
  //control signals (to be expanded as needed)
  bool        reg_write;
  bool        mem_read;
  bool        mem_write;
  bool        branch;
  bool        alu_src;
  uint32_t    alu_op;
  /**
   * Add other control fields as needed
   */
}idex_reg_t;

typedef struct
{
  Instruction instr;
  uint32_t    instr_addr;
  uint32_t    instr_bits; //Raw instruction bits for debugging
  // ALU result
  uint32_t    alu_result;
  //Register data for memory operations
  uint32_t    rs2_data;//For store operations
  uint32_t    rd_idx;//Destination register index
  //Control signals passed from decode
  bool        reg_write;
  bool        mem_read;
  bool        mem_write;
  bool        branch;
  //Branch evaluation result
  bool        branch_taken;
  uint32_t    branch_target;
  /**
   * Add other fields as needed
   */
}exmem_reg_t;

typedef struct
{
  Instruction instr;
  uint32_t    instr_addr;
  uint32_t    instr_bits;
  //Data to be written back to register file
  uint32_t    write_data; //Data to write to destination register
  uint32_t    rd_idx; //Destination register index
  //Control signals
  bool        reg_write; //Whether to write to register file
  //Source of write data (for multiplexing)
  bool        mem_to_reg; //0: ALU result, 1: Memory data
  //Branch information (passed through from execute stage)
  bool        branch_taken;//Whether a branch was taken
  uint32_t    branch_target;//Target address of the branch
}memwb_reg_t;


///////////////////////////////////////////////////////////////////////////////
/// Register types with input and output variants for simulator
///////////////////////////////////////////////////////////////////////////////

typedef struct
{
  ifid_reg_t inp;
  ifid_reg_t out;
}ifid_reg_pair_t;

typedef struct
{
  idex_reg_t inp;
  idex_reg_t out;
}idex_reg_pair_t;

typedef struct
{
  exmem_reg_t inp;
  exmem_reg_t out;
}exmem_reg_pair_t;

typedef struct
{
  memwb_reg_t inp;
  memwb_reg_t out;
}memwb_reg_pair_t;

///////////////////////////////////////////////////////////////////////////////
/// Functional pipeline requirements
///////////////////////////////////////////////////////////////////////////////

typedef struct
{
  ifid_reg_pair_t  ifid_preg;
  idex_reg_pair_t  idex_preg;
  exmem_reg_pair_t exmem_preg;
  memwb_reg_pair_t memwb_preg;
}pipeline_regs_t;

typedef struct
{
  bool      pcsrc;
  uint32_t  pc_src0;
  uint32_t  pc_src1;
  /**
   * Add other fields here
   */
}pipeline_wires_t;


///////////////////////////////////////////////////////////////////////////////
/// Function definitions for different stages
///////////////////////////////////////////////////////////////////////////////
//hehehee out here hittin the gwiddy no fanum Tralalero Tralala with that bombardiro crocodilo
/**
 * output : ifid_reg_t
 **/ 
ifid_reg_t stage_fetch(pipeline_wires_t* pwires_p, regfile_t* regfile_p, Byte* memory_p);

/**
 * output : idex_reg_t
 **/ 
idex_reg_t stage_decode(ifid_reg_t ifid_reg, pipeline_wires_t* pwires_p, regfile_t* regfile_p);

/**
 * output : exmem_reg_t
 **/ 
exmem_reg_t stage_execute(idex_reg_t idex_reg, pipeline_wires_t* pwires_p);

/**
 * output : memwb_reg_t
 **/ 
memwb_reg_t stage_mem(exmem_reg_t exmem_reg, pipeline_wires_t* pwires_p, Byte* memory, Cache* cache_p);

/**
 * output : write_data
 **/ 
void stage_writeback(memwb_reg_t memwb_reg, pipeline_wires_t* pwires_p, regfile_t* regfile_p);

void cycle_pipeline(regfile_t* regfile_p, Byte* memory_p, Cache* cache_p, pipeline_regs_t* pregs_p, pipeline_wires_t* pwires_p, bool* ecall_exit);

void bootstrap(pipeline_wires_t* pwires_p, pipeline_regs_t* pregs_p, regfile_t* regfile_p);

#endif  // __PIPELINE_H__
