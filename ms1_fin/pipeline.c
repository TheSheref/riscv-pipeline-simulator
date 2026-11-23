#include <stdbool.h>
#include "cache.h"
#include "riscv.h"
#include "types.h"
#include "utils.h"
#include "pipeline.h"
#include "stage_helpers.h"

uint64_t total_cycle_counter = 0;
uint64_t miss_count = 0;
uint64_t hit_count = 0;
uint64_t stall_counter = 0;
uint64_t branch_counter = 0;
uint64_t fwd_exex_counter = 0;
uint64_t fwd_exmem_counter = 0;
uint64_t mem_access_counter = 0;

simulator_config_t sim_config = {0};

///////////////////////////////////////////////////////////////////////////////

void bootstrap(pipeline_wires_t* pwires_p, pipeline_regs_t* pregs_p, regfile_t* regfile_p)
{
  //PC src must get the same value as the default PC value
  pwires_p->pc_src0 = regfile_p->PC;
}

///////////////////////////
/// STAGE FUNCTIONALITY ///
///////////////////////////

/** //i hope no one can see that i wrote Cappuccino Assassino hehehehehee
 * STAGE  : stage_fetch
 * output : ifid_reg_t
 **/ 
ifid_reg_t stage_fetch(pipeline_wires_t* pwires_p, regfile_t* regfile_p, Byte* memory_p)
{
  ifid_reg_t ifid_reg = {0};
  
  //Fetch instruction from memory at current PC
  uint32_t instruction_bits = load(memory_p, regfile_p->PC, LENGTH_WORD);
  
  //Parse the instruction bits into Instruction union
  Instruction instruction = parse_instruction(instruction_bits);
  
  //ppopulate the IF/ID register
  ifid_reg.instr = instruction;
  ifid_reg.instr_addr = regfile_p->PC;
  ifid_reg.instr_bits=instruction_bits;
  //determine next PC value
  uint32_t next_pc;
  if (pwires_p->pcsrc) {
    //branch taken: use pc_src1 (branch target)
    next_pc = pwires_p->pc_src1;
  } else {
    //ssequential execution: PC + 4
    next_pc = regfile_p->PC + 4;
  }
  
  //update PC for next cycle
  regfile_p->PC = next_pc;
  
  //store next PC in wires for other stages
  pwires_p->pc_src0 = next_pc;

  #ifdef DEBUG_CYCLE
  printf("[IF ]: Instruction [%08x]@[%08x]: ", instruction_bits, ifid_reg.instr_addr);
  decode_instruction(instruction_bits);
  #endif
  return ifid_reg;
}

/**
 * STAGE  : stage_decode
 * output : idex_reg_t
 **/ 
idex_reg_t stage_decode(ifid_reg_t ifid_reg, pipeline_wires_t* pwires_p, regfile_t* regfile_p)
{
  idex_reg_t idex_reg = {0};
  
  //Copy instruction and address from IF/ID register
  idex_reg.instr = ifid_reg.instr;
  idex_reg.instr_addr = ifid_reg.instr_addr;
  idex_reg.instr_bits= ifid_reg.instr_bits;
  //Read register values from register file
  //rs1 value (source register 1)
  uint32_t rs1_data = 0;
  uint32_t rs2_data = 0;
  
  //extract register indices from instruction
  uint32_t rs1_idx = 0;
  uint32_t rs2_idx = 0;
  
  //generate control signals using helper function
  idex_reg_t control_signals = gen_control(ifid_reg.instr);
  
  //ccopy control signals from gen_control
  idex_reg.reg_write = control_signals.reg_write;
  idex_reg.mem_read = control_signals.mem_read;
  idex_reg.mem_write = control_signals.mem_write;
  idex_reg.branch = control_signals.branch;
  idex_reg.alu_src = control_signals.alu_src;
  idex_reg.rd_idx = control_signals.rd_idx;
  
  //extract register indices from instruction
  switch(ifid_reg.instr.opcode) {
    case 0x33: // R-type
      rs1_idx = ifid_reg.instr.rtype.rs1;
      rs2_idx = ifid_reg.instr.rtype.rs2;
      break;
    case 0x13://I-type (imm)
    case 0x03://I-type (load)
    case 0x67://I-type (jalr)
      rs1_idx = ifid_reg.instr.itype.rs1;
      rs2_idx = 0;//I-type doesn't use rs2...
      break;
    case 0x23://S-type (store)
      rs1_idx = ifid_reg.instr.stype.rs1;
      rs2_idx = ifid_reg.instr.stype.rs2;
      break;
    case 0x63://B-type (branch)
      rs1_idx = ifid_reg.instr.sbtype.rs1;
      rs2_idx = ifid_reg.instr.sbtype.rs2;
      break;
    case 0x37://U-type (lui)
    case 0x17://U-type (auipc)
      rs1_idx = 0;//U-type doesn't use rs1
      rs2_idx = 0;//U-type doesn't use rs2
      break;
    case 0x6F://J-type (jal)
      rs1_idx = 0;//JAL doesn't use rs1
      rs2_idx = 0;//JAL doesn't use rs2
      break;
    default:
      rs1_idx = 0;
      rs2_idx = 0;
      break;
  }
  
  //reeead register values (register 0 is always 0 in RISC-V)
  if (rs1_idx != 0) {
    rs1_data = regfile_p->R[rs1_idx];
  }
  if (rs2_idx != 0) {
    rs2_data = regfile_p->R[rs2_idx];
  }
  
  //generate imm value using helper function
  uint32_t immediate = gen_imm(ifid_reg.instr);
  //store the decoded values in idex_reg
  idex_reg.rs1_data = rs1_data;
  idex_reg.rs2_data = rs2_data;
  idex_reg.rs1_idx = rs1_idx;
  idex_reg.rs2_idx = rs2_idx;
  idex_reg.immediate = immediate;
  
    #ifdef DEBUG_CYCLE
  printf("[ID ]: Instruction [%08x]@[%08x]: ", idex_reg.instr_bits, idex_reg.instr_addr);
  if (idex_reg.instr_bits != 0) {
    decode_instruction(idex_reg.instr_bits);
  } else {
    printf("\n");
  }
  #endif
  
  return idex_reg;
}

/**
 * STAGE  : stage_execute
 * output : exmem_reg_t
 **/ 
exmem_reg_t stage_execute(idex_reg_t idex_reg, pipeline_wires_t* pwires_p)
{
  exmem_reg_t exmem_reg = {0};
  
  //copy instruction, address, and bits from ID/EX register
  exmem_reg.instr = idex_reg.instr;
  exmem_reg.instr_addr = idex_reg.instr_addr;
  exmem_reg.instr_bits = idex_reg.instr_bits;
  //ccopy control signals from decode stage
  exmem_reg.reg_write = idex_reg.reg_write;
  exmem_reg.mem_read = idex_reg.mem_read;
  exmem_reg.mem_write = idex_reg.mem_write;
  exmem_reg.branch = idex_reg.branch;
  exmem_reg.rd_idx = idex_reg.rd_idx;
  exmem_reg.rs2_data = idex_reg.rs2_data;
  
  //determine ALU inputs
  uint32_t alu_inp1;
  uint32_t alu_inp2;
  
  //special handling for LUI and AUIPC instructions
  if (idex_reg.instr.opcode == 0x37) {//LUI
    alu_inp1 = 0;  //LUI: 0 +imm
    alu_inp2 = idex_reg.immediate;
  } else if (idex_reg.instr.opcode == 0x17) {//AUIPC
    alu_inp1 = idex_reg.instr_addr; //AUIPC: PC+ imm
    alu_inp2 = idex_reg.immediate;
  } else {
    //normal ALU input selection
    alu_inp1 = idex_reg.rs1_data;
    
    //ALU source selection: use imm or rs2 data (MUX)
    if (idex_reg.alu_src) {
      alu_inp2 = idex_reg.immediate; //Use imm value
    } else {
      alu_inp2 = idex_reg.rs2_data;//Use register value
    }
  }
  
  //gnerate ALU control signal
  uint32_t alu_control = gen_alu_control(idex_reg);
  
  //execute ALU operation
  uint32_t alu_result = execute_alu(alu_inp1, alu_inp2, alu_control);
  exmem_reg.alu_result = alu_result;
  
  //handle branch and jump evaluation
  bool branch_taken = false;
  uint32_t branch_target = 0;
  
  //hhandle JAL (jump and link) instruction
  if (idex_reg.instr.opcode == 0x6F) {//JAL opcode
    //calc jump target: PC +sign-extended imm
    branch_target = idex_reg.instr_addr + idex_reg.immediate;
    branch_taken = true;
    
    //For JAL, the ALU result is PC + 4 (return address)
    alu_result = idex_reg.instr_addr + 4;
    exmem_reg.alu_result = alu_result; //Update the ALU result in the output register
  }
  //Handle branch instructions
  else if (idex_reg.branch) {
  //calculate branch target address
    branch_target = idex_reg.instr_addr + idex_reg.immediate;
    
    //evaluate branch condition based on instruction type
    switch(idex_reg.instr.sbtype.funct3) {
      case 0x0://BEQ  branch if equal
        branch_taken = (idex_reg.rs1_data == idex_reg.rs2_data);
        break;
      case 0x1://BNE branch if not equal
        branch_taken = (idex_reg.rs1_data != idex_reg.rs2_data);
        break;
      case 0x4://BLT branch if less than (signed)
        branch_taken = ((int32_t)idex_reg.rs1_data < (int32_t)idex_reg.rs2_data);
        break;
      case 0x5://BGE branch if greater than or equal (signed)
        branch_taken = ((int32_t)idex_reg.rs1_data >= (int32_t)idex_reg.rs2_data);
        break;
      case 0x6://BLTUbranch if less than (unsigned)
        branch_taken = (idex_reg.rs1_data < idex_reg.rs2_data);
        break;
      case 0x7://BGEUbranch if greater than or equal (unsigned)
        branch_taken = (idex_reg.rs1_data >= idex_reg.rs2_data);
        break;
      default:
        branch_taken = false;
        break;
    }
  }
  
  exmem_reg.branch_taken = branch_taken;
  exmem_reg.branch_target = branch_target;
  
  //Update pipeline wires for PC control
  if (branch_taken) {
    pwires_p->pcsrc = true;
    pwires_p->pc_src1 = branch_target;
  } else {
    pwires_p->pcsrc = false;
  }
  
  #ifdef DEBUG_CYCLE
  printf("[EX ]: Instruction [%08x]@[%08x]: ", exmem_reg.instr_bits, exmem_reg.instr_addr);
  if (exmem_reg.instr_bits != 0) {
    decode_instruction(exmem_reg.instr_bits);
  } else {
    printf("\n");
  }
  #endif
  
  return exmem_reg;
}

/**
 * STAGE  : stage_mem
 * output : memwb_reg_t
 **/ 
memwb_reg_t stage_mem(exmem_reg_t exmem_reg, pipeline_wires_t* pwires_p, Byte* memory_p, Cache* cache_p)
{
  memwb_reg_t memwb_reg = {0};
  
  //Copy instruction, address, and bits from EX/MEM register
  memwb_reg.instr = exmem_reg.instr;
  memwb_reg.instr_addr = exmem_reg.instr_addr;
  memwb_reg.instr_bits = exmem_reg.instr_bits;
  //Copy control signals
  memwb_reg.reg_write = exmem_reg.reg_write;
  memwb_reg.rd_idx = exmem_reg.rd_idx;
  
  //cooppy branch information from execute stage
  memwb_reg.branch_taken = exmem_reg.branch_taken;
  memwb_reg.branch_target = exmem_reg.branch_target;
  
  //default write data is ALU result
  uint32_t write_data = exmem_reg.alu_result;
  bool mem_to_reg = false;
  
  //handlin memory ops
  if (exmem_reg.mem_read) {
    //Load operation ... read from memory
    uint32_t mem_address = exmem_reg.alu_result;
    
    //Determinein load type based on funct3
    switch(exmem_reg.instr.itype.funct3) {
      case 0x0: // LB ... load byte (sign-extended)
        {
          int8_t loaded_byte = (int8_t)load(memory_p, mem_address, LENGTH_BYTE);
          write_data = (uint32_t)loaded_byte;//Sign extension
        }
        break;
      case 0x1: // LH ... load halfword (sign-extended)
        {
          int16_t loaded_half = (int16_t)load(memory_p, mem_address, LENGTH_HALF_WORD);
          write_data = (uint32_t)loaded_half; // sign extend
        }
        break;
      case 0x2: //LW load word
        write_data = load(memory_p, mem_address, LENGTH_WORD);
        break;
      case 0x4: //LBU load byte unsigned
        write_data = (uint32_t)load(memory_p, mem_address, LENGTH_BYTE);
        break;
      case 0x5: //LHU load halfword unsigned
        write_data = (uint32_t)load(memory_p, mem_address, LENGTH_HALF_WORD);
        break;
      default:
        write_data = load(memory_p, mem_address, LENGTH_WORD);
        break;
    }
    mem_to_reg = true;
    
    #ifdef DEBUG_CYCLE
    printf("[MEM]: Instruction [%08x]@[%08x]: ", memwb_reg.instr_bits, memwb_reg.instr_addr);
    if (memwb_reg.instr_bits != 0) {
      decode_instruction(memwb_reg.instr_bits);
    } else {
      printf("\n");
    }
    #endif
    
  } else if (exmem_reg.mem_write) {
    //store operation write to memory
    uint32_t mem_address = exmem_reg.alu_result;
    uint32_t store_data = exmem_reg.rs2_data;
    
    //deetermine store type based on funct3
    switch(exmem_reg.instr.stype.funct3) {
      case 0x0://SB store byte
        store(memory_p, mem_address, LENGTH_BYTE, (Byte)(store_data & 0xFF));
        break;
      case 0x1: //SH store halfword
        store(memory_p, mem_address, LENGTH_HALF_WORD, (Half)(store_data & 0xFFFF));
        break;
      case 0x2:// SW store word
        store(memory_p, mem_address, LENGTH_WORD, store_data);
        break;
      default:
        store(memory_p, mem_address, LENGTH_WORD, store_data);
        break;
    }
    
    #ifdef DEBUG_CYCLE
    printf("[MEM]: Instruction [%08x]@[%08x]: ", exmem_reg.instr_bits, exmem_reg.instr_addr);
    if (exmem_reg.instr_bits != 0) {
      decode_instruction(exmem_reg.instr_bits);
    } else {
      printf("\n");
    }
    #endif
    
  } else {
    //no memory operation ... use ALU result
    #ifdef DEBUG_CYCLE
    printf("[MEM]: Instruction [%08x]@[%08x]: ", exmem_reg.instr_bits, exmem_reg.instr_addr);
    if (exmem_reg.instr_bits != 0) {
      decode_instruction(exmem_reg.instr_bits);
    } else {
      printf("\n");
    }
    #endif
  }
  
  //setthe write data and control signals
  memwb_reg.write_data = write_data;
  memwb_reg.mem_to_reg = mem_to_reg;
  
  return memwb_reg;
}

/**
 * STAGE  : stage_writeback
 * output : nothing - The state of the register file may be changed
 **/ 
 void stage_writeback(memwb_reg_t memwb_reg, pipeline_wires_t* pwires_p, regfile_t* regfile_p)
 {
   // Only write to register file if:
   // 1. There's a valid instruction (not NOP/bubble)
   // 2. reg_write control signal is asserted
   // 3. destination register is not x0 (which is always 0 in RISC-V)
   if (memwb_reg.instr_bits != 0 && memwb_reg.reg_write && memwb_reg.rd_idx != 0) {
     // Write the data to the destination register
     regfile_p->R[memwb_reg.rd_idx] = memwb_reg.write_data;
     
   }  
   #ifdef DEBUG_CYCLE
       printf("[WB ]: Instruction [%08x]@[%08x]: ", memwb_reg.instr_bits, memwb_reg.instr_addr);
       if (memwb_reg.instr_bits != 0) {
           decode_instruction(memwb_reg.instr_bits);
       } else {
           printf("\n");
       }
     #endif
 }

///////////////////////////////////////////////////////////////////////////////

/** 
 * excite the pipeline with one clock cycle
 **/
void cycle_pipeline(regfile_t* regfile_p, Byte* memory_p, Cache* cache_p, pipeline_regs_t* pregs_p, pipeline_wires_t* pwires_p, bool* ecall_exit)
{
  #ifdef DEBUG_CYCLE
  printf("v==============");
  printf("Cycle Counter = %5ld", total_cycle_counter);
  printf("==============v\n\n");
  #endif

  // process each stage

  /* Output               |    Stage      |       Inputs  */
  pregs_p->ifid_preg.inp  = stage_fetch     (pwires_p, regfile_p, memory_p);
  
  pregs_p->idex_preg.inp  = stage_decode    (pregs_p->ifid_preg.out, pwires_p, regfile_p);

  pregs_p->exmem_preg.inp = stage_execute   (pregs_p->idex_preg.out, pwires_p);

  pregs_p->memwb_preg.inp = stage_mem       (pregs_p->exmem_preg.out, pwires_p, memory_p, cache_p);

                            stage_writeback (pregs_p->memwb_preg.out, pwires_p, regfile_p);

  // update all the output registers for the next cycle from the input registers in the current cycle
  pregs_p->ifid_preg.out  = pregs_p->ifid_preg.inp;
  pregs_p->idex_preg.out  = pregs_p->idex_preg.inp;
  pregs_p->exmem_preg.out = pregs_p->exmem_preg.inp;
  pregs_p->memwb_preg.out = pregs_p->memwb_preg.inp;

  /////////////////// NO CHANGES BELOW THIS ARE REQUIRED //////////////////////

  // increment the cycle
  total_cycle_counter++;

  #ifdef DEBUG_REG_TRACE
  print_register_trace(regfile_p);
  #endif

  /**
   * check ecall condition
   * To do this, the value stored in R[10] (a0 or x10) should be 10.
   * Hence, the ecall condition is checked by the existence of following
   * two instructions in sequence:
   * 1. <instr>  x10, <val1>, <val2> 
   * 2. ecall
   * 
   * The first instruction must write the value 10 to x10.
   * The second instruction is the ecall (opcode: 0x73)
   * 
   * The condition checks whether the R[10] value is 10 when the
   * `memwb_reg.instr.opcode` == 0x73 (to propagate the ecall)
   * 
   * If more functionality on ecall needs to be added, it can be done
   * by adding more conditions on the value of R[10]
   */
  if( (pregs_p->memwb_preg.out.instr.bits == 0x00000073) &&
      (regfile_p->R[10] == 10) )
  {
    *(ecall_exit) = true;
  }
}

