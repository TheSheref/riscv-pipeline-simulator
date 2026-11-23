#ifndef __STAGE_HELPERS_H__
#define __STAGE_HELPERS_H__

#include <stdio.h>
#include "utils.h"
#include "pipeline.h"

/// EXECUTE STAGE HELPERS ///

/**
 * input  : idex_reg_t
 * output : uint32_t alu_control signal
 **/
uint32_t gen_alu_control(idex_reg_t idex_reg)
{
  uint32_t alu_control = 0;
  
  switch(idex_reg.instr.opcode) {
    case 0x33: // R-type
      switch(idex_reg.instr.rtype.funct3) {
        case 0x0: //ADD/SUB/MUL
          if (idex_reg.instr.rtype.funct7 == 0x20) {
            alu_control = 0x1; //SUB
          } else if (idex_reg.instr.rtype.funct7 == 0x1) {
            alu_control = 0xB; //MUL
          } else {
            alu_control = 0x0; //ADD
          }
          break;
        case 0x1: // SLL/MULH
          if (idex_reg.instr.rtype.funct7 == 0x1) {
            alu_control = 0xC; //MULH
          } else {
            alu_control = 0x8; //SLL (Shift Left Logical)
          }
          break;
        case 0x4: //XOR
          alu_control = 0x4;
          break;
        case 0x5: //SRL/SRA
          alu_control = (idex_reg.instr.rtype.funct7 == 0x20) ? 0xA : 0x9; // SRA: SRL
          break;
        case 0x6: //OR
          alu_control = 0x6;
          break;
        case 0x7: //AND
          alu_control = 0x7;
          break;
        case 0x2: //SLT
          alu_control = 0x2;
          break;
        case 0x3: //SLTU
          alu_control = 0x3;
          break;
        default:
          alu_control = 0x0; //defaulting to ADD
          break;
      }
      break;
    case 0x13: //I-type immediate
      switch(idex_reg.instr.itype.funct3) {
        case 0x0://ADDI
          alu_control = 0x0; //ADD
          break;
        case 0x1://SLLI
          alu_control = 0x8; //SLL (Shift Left Logical)
          break;
        case 0x4://XORI
          alu_control = 0x4; //XOR
          break;
        case 0x5: // SRLI/SRAI
          // Check bit 30 of immediate to distinguish SRLI from SRAI
          alu_control = ((idex_reg.instr.itype.imm >> 10) & 0x1) ? 0xA : 0x9; // SRAI : SRLI
          break;
        case 0x6://ORI
          alu_control = 0x6;//OR
          break;
        case 0x7://ANDI
          alu_control = 0x7;//AND
          break;
        case 0x2://SLTI
          alu_control = 0x2;//SLT
          break;
        case 0x3: // SLTIU
          alu_control = 0x3;//SLTU
          break;
        default:
          alu_control = 0x0;//Default to ADD
          break;
      }
      break;
    case 0x03://Load
    case 0x23://Store
    case 0x67://JALR
      alu_control = 0x0;//ADD for address calculation
      break;
    case 0x63: // Branch
      switch(idex_reg.instr.sbtype.funct3) {
        case 0x0://BEQ
        case 0x1://BNE
          alu_control = 0x1;//SUB for comparison
          break;
        case 0x4://BLT
        case 0x5://BGE
          alu_control = 0x2;//SLT for comparison
          break;
        case 0x6://BLTU
        case 0x7://BGEU
          alu_control = 0x3; //SLTU for comparison
          break;
        default:
          alu_control = 0x1;//Default to SUB
          break;
      }
      break;
    case 0x37://LUI.. Load Upper imm
      alu_control = 0x0;//ADD (0 + imm)
      break;
    case 0x17://AUIPC .. Add Upper imm to PC
      alu_control = 0x0; // ADD (PC + imm)
      break;
    case 0x6F://JAL... Jump and Link
      alu_control = 0x0;//ADD for PC + 4 calc
      break;
    default:
      alu_control = 0x0;//Default to ADD
      break;
  }
  
  return alu_control;
}

/**
 * input  : alu_inp1, alu_inp2, alu_control
 * output : uint32_t alu_result
 **/
uint32_t execute_alu(uint32_t alu_inp1, uint32_t alu_inp2, uint32_t alu_control)
{
  uint32_t result;
  switch(alu_control){
    case 0x0://ADD
      result = alu_inp1 + alu_inp2;
      break;
    case 0x1://SUB
      result = alu_inp1 - alu_inp2;
      break;
    case 0x2://SLT (Set Less Than)
      result = ((int32_t)alu_inp1 < (int32_t)alu_inp2) ? 1 : 0;
      break;
    case 0x3://SLTU (Set Less Than Unsigned)
      result = (alu_inp1 < alu_inp2) ? 1 : 0;
      break;
    case 0x4://XOR
      result = alu_inp1 ^ alu_inp2;
      break;
    case 0x6://OR
      result = alu_inp1 | alu_inp2;
      break;
    case 0x7://AND
      result = alu_inp1 & alu_inp2;
      break;
    case 0x8://SLL (Shift Left Logical)
      result = alu_inp1 << (alu_inp2 & 0x1F);//Only usin lower 5 bits for shift amount
      break;
    case 0x9://SRL(Shift Right Logical)
      result = alu_inp1 >> (alu_inp2 & 0x1F); // Only use lower 5 bits for shift amount
      break;
    case 0xA: //SRA (Shift Right Arithmetic)
      result = (uint32_t)((int32_t)alu_inp1 >> (alu_inp2 & 0x1F)); // Arithmetic shift
      break;
    case 0xB://MUL (Multiply)
      result = (uint32_t)((int32_t)alu_inp1 * (int32_t)alu_inp2); // 32-bit multiply
      break;
    case 0xC://MULH (Multiply High)
      {
        int64_t product = (int64_t)((int32_t)alu_inp1) * (int64_t)((int32_t)alu_inp2);
        result = (uint32_t)(product >> 32); //Upper 32 bits
      }
      break;
    default:
      result = 0xBADCAFFE;
      break;
  };
  return result;
}

/// DECODE STAGE HELPERS ///

/**
 * input  : Instruction
 * output : idex_reg_t
 **/
uint32_t gen_imm(Instruction instruction)
{
  int imm_val = 0;
  
  switch(instruction.opcode) {
    case 0x13: //I-type imm (ADDI, XORI, ORI, ANDI, SLTI, SLTIU)
    case 0x03://I-type load (LB, LH, LW, LBU, LHU)
    case 0x67: //I-type JALR
      //I-type: 12-bit imm, sign-extended
      imm_val = sign_extend_number(instruction.itype.imm, 12);
      break;
      
    case 0x23://S-type store (SB, SH, SW)
      //S-type: 12-bit imm split between imm[11:5] and imm[4:0]
      imm_val = (instruction.stype.imm7 << 5) | instruction.stype.imm5;
      imm_val = sign_extend_number(imm_val, 12);
      break;
      
    case 0x63://B-typebranch (BEQ, BNE, BLT, BGE, BLTU, BGEU)
      //B-type: 13-bit immediate for branch offset
      imm_val = get_branch_offset(instruction);
      break;
      
    case 0x37://U-type LUI
    case 0x17://U-type AUIPC
      //U-type: 20-bit immediate in upper 20 bits
      imm_val = instruction.utype.imm << 12;
      break;
      
    case 0x6F:// J-type JAL
      //J-type: 21-bit immediate for jump offset
      imm_val = get_jump_offset(instruction);
      break;
      
    case 0x33://R-type (no immediate)
    default:
      //R-type and undefined opcodes have no immediate
      imm_val = 0;
      break;
  }
  
  return imm_val;
}

/**
 * generates all the control logic that flows around in the pipeline
 * input  : Instruction
 * output : idex_reg_t
 **/
idex_reg_t gen_control(Instruction instruction)
{
  idex_reg_t idex_reg = {0};
  
  //extract destination register index
  uint32_t rd_idx = 0;
  
  switch(instruction.opcode) {
    case 0x33: //R-type (ADD, SUB, XOR, OR, AND, SLT, SLTU)
      rd_idx = instruction.rtype.rd;
      idex_reg.reg_write = (rd_idx != 0);//write to register if not x0
      idex_reg.mem_read = false;
      idex_reg.mem_write = false;
      idex_reg.branch = false;
      idex_reg.alu_src = false;//use rs2 data, not imm
      break;
      
    case 0x13://I-type imm (ADDI, XORI, ORI, ANDI, SLTI, SLTIU)
      rd_idx = instruction.itype.rd;
      idex_reg.reg_write = (rd_idx != 0);//write to register if not x0
      idex_reg.mem_read = false;
      idex_reg.mem_write = false;
      idex_reg.branch = false;
      idex_reg.alu_src = true;//Use imm value
      break;
      
    case 0x03://I-type load (LB, LH, LW, LBU, LHU)
      rd_idx = instruction.itype.rd;
      idex_reg.reg_write = (rd_idx != 0);//write to register if not x0
      idex_reg.mem_read = true;
      idex_reg.mem_write = false;
      idex_reg.branch = false;
      idex_reg.alu_src = true;//use imm for address calculation
      break;
      
    case 0x23://S-type store (SB, SH, SW)
      rd_idx = 0;//storee instructions don't write to registers
      idex_reg.reg_write = false;
      idex_reg.mem_read = false;
      idex_reg.mem_write = true;
      idex_reg.branch = false;
      idex_reg.alu_src = true;//Use imm for address calculation
      break;
      
    case 0x63://B-type branch (BEQ, BNE, BLT, BGE, BLTU, BGEU)
      rd_idx = 0;//Branch instructions don't write to registers
      idex_reg.reg_write = false;
      idex_reg.mem_read = false;
      idex_reg.mem_write = false;
      idex_reg.branch = true;
      idex_reg.alu_src = false;//usin rs2 data for comparison
      break;
      
    case 0x37://U-type LUI
      rd_idx = instruction.utype.rd;
      idex_reg.reg_write = (rd_idx != 0);// Write to register if not x0
      idex_reg.mem_read = false;
      idex_reg.mem_write = false;
      idex_reg.branch = false;
      idex_reg.alu_src = true;//usin imm value
      break;
      
    case 0x17://U-type AUIPC
      rd_idx = instruction.utype.rd;
      idex_reg.reg_write = (rd_idx != 0);//write to register if not x0
      idex_reg.mem_read = false;
      idex_reg.mem_write = false;
      idex_reg.branch = false;
      idex_reg.alu_src = true;//use imm value
      break;
      
    case 0x6F://J-type JAL
      rd_idx = instruction.ujtype.rd;
      idex_reg.reg_write = (rd_idx != 0); // Write to register if not x0
      idex_reg.mem_read = false;
      idex_reg.mem_write = false;
      idex_reg.branch = false;
      idex_reg.alu_src = true; //use imm for jump calculation
      break;
      
    case 0x67://I-type JALR
      rd_idx = instruction.itype.rd;
      idex_reg.reg_write = (rd_idx != 0);//write to register if not x0
      idex_reg.mem_read = false;
      idex_reg.mem_write = false;
      idex_reg.branch = false;
      idex_reg.alu_src = true; //Use imm value
      break;
      
    default: //defaultingggg to Unknown opcode
      rd_idx = 0;
      idex_reg.reg_write = false;
      idex_reg.mem_read = false;
      idex_reg.mem_write = false;
      idex_reg.branch = false;
      idex_reg.alu_src = false;
      break;
  }
  
  //set the destination register index
  idex_reg.rd_idx = rd_idx;
  
  return idex_reg;
}

/// MEMORY STAGE HELPERS ///

/**
 * evaluates whether a branch must be taken
 * input  : <open to implementation>
 * output : bool
 **/
bool gen_branch(Instruction instruction, uint32_t rs1_data, uint32_t rs2_data)
{
  if (instruction.opcode != 0x63) { //not a branch instruction
    return false;
  }
  
  // Evaluate branch condition based on funct3 field
  switch (instruction.sbtype.funct3) {
    case 0x0://EQ ..Branch if equal
      return (rs1_data == rs2_data);
      
    case 0x1://BNE ..Branch if not equal
      return (rs1_data != rs2_data);
      
    case 0x4://BLT ..Branch if less than (signed)
      return ((int32_t)rs1_data < (int32_t)rs2_data);
      
    case 0x5://BGE... Branch if greater than or equal (signed)
      return ((int32_t)rs1_data >= (int32_t)rs2_data);
      
    case 0x6://BLTU.. Branch if less than (unsigned)
      return (rs1_data < rs2_data);
      
    case 0x7://BGEU.. Branch if greater than or equal (unsigned)
      return (rs1_data >= rs2_data);
      
    default:
      return false;//Invalid branch instruction tuff luck tbh CAPPUCINOOOOOO 
  }
}


/// PIPELINE FEATURES ///

/**
 * Task   : Sets the pipeline wires for the forwarding unit's control signals
 *           based on the pipeline register values.
 * input  : pipeline_regs_t*, pipeline_wires_t*
 * output : None
*/
void gen_forward(pipeline_regs_t* pregs_p, pipeline_wires_t* pwires_p)
{
  idex_reg_t idex = pregs_p->idex_preg.out;
  exmem_reg_t exmem = pregs_p->exmem_preg.out;
  memwb_reg_t memwb = pregs_p->memwb_preg.out;
  
  //ceck for EX hazard (forward from EX/MEM to EX stage)
  if (exmem.reg_write && exmem.rd_idx != 0) {
    if (idex.rs1_idx == exmem.rd_idx) {
      pwires_p->forward_rs1 = 0x1;//forward from EX/MEM
      fwd_exex_counter++;
    }
    if (idex.rs2_idx == exmem.rd_idx) {
      pwires_p->forward_rs2 = 0x1;//forward from EX/MEM
      fwd_exex_counter++;
    }
    
    // Special case for store instructions - check if we need to forward to rs2 for store data
    if (idex.mem_write && idex.rs2_idx == exmem.rd_idx) {
      pwires_p->forward_rs2 = 0x1;//forward from EX/MEM for store data
      fwd_exex_counter++;
    }
  }
  
  //cgeck for MEM hazard (forward from MEM/WB to EX stage)
  if (memwb.reg_write && memwb.rd_idx != 0) {
    //only forward if not already forwarded from EX/MEM (higher priority)
    if (idex.rs1_idx == memwb.rd_idx && (idex.rs1_idx != exmem.rd_idx || !exmem.reg_write)) {
      pwires_p->forward_rs1 = 0x2;// Forward from MEM/WB
      fwd_exmem_counter++;
    }
    if (idex.rs2_idx == memwb.rd_idx && (idex.rs2_idx != exmem.rd_idx || !exmem.reg_write)) {
      pwires_p->forward_rs2 = 0x2;// Forward from MEM/WB
      fwd_exmem_counter++;
    }
  }
}

/**
 * Task   : Sets the pipeline wires for the hazard unit's control signals
 *           based on the pipeline register values.
 * input  : pipeline_regs_t*, pipeline_wires_t*, regfile_t*
 * output : None
*/
void detect_hazard(pipeline_regs_t* pregs_p, pipeline_wires_t* pwires_p, regfile_t* regfile_p)
{
  // Initialize hazard detection signals
  bool load_use_hazard = false;
  bool control_hazard = false;
  // Get pipeline registers
  ifid_reg_t ifid = pregs_p->ifid_preg.out;
  idex_reg_t idex = pregs_p->idex_preg.out;
  exmem_reg_t exmem = pregs_p->exmem_preg.out;
  // Check for load-use hazard (stall needed)
  if (idex.mem_read) {
    // Check if the destination register of the load is used in the next instruction
    uint32_t rd = idex.rd_idx;
    uint32_t rs1 = 0;
    uint32_t rs2 = 0;
    // Get rs1 and rs2 from the instruction in IF/ID
    if (ifid.instr_bits != 0) {  // Not a zeroed bubble
      switch (ifid.instr.opcode) {
        case 0x33: // R-type
          rs1 = ifid.instr.rtype.rs1;
          rs2 = ifid.instr.rtype.rs2;
          break;
        case 0x03: // Load (I-type)
        case 0x13: // I-type immediate
        case 0x67: // JALR (I-type)
          rs1 = ifid.instr.itype.rs1;
          rs2 = 0;
          break;
        case 0x23: // Store (S-type)
          rs1 = ifid.instr.stype.rs1;
          rs2 = ifid.instr.stype.rs2;
          break;
        case 0x63: // Branch (B-type)
          rs1 = ifid.instr.sbtype.rs1;
          rs2 = ifid.instr.sbtype.rs2;
          break;
        default:
          rs1 = 0;
          rs2 = 0;
          break;
      }
      // Check if either source register matches the load destination
      if ((rs1 != 0 && rs1 == rd) || (rs2 != 0 && rs2 == rd)) {
        load_use_hazard = true;
        pwires_p->hzrd_detected = true;
        stall_counter++;
      } else {
        pwires_p->hzrd_detected = false;
      }
    }
  }
  // Check for control hazard (branch/jump). Only set flush; clearing is handled centrally.
  if (exmem.branch_taken) {
    control_hazard = true;
    branch_counter++;
    pwires_p->flush = true;
    pwires_p->new_pc = exmem.branch_target;
  } else {
    pwires_p->flush = false;
  }
  // Set hazard detection signals: stall only IF. Do NOT bubble ID/EX.
  pwires_p->ifid_stall = load_use_hazard;
  pwires_p->idex_bubble = false;
  // If we're stalling, we need to re-fetch the current instruction (hold PC)
  // PC will be held in fetch; we defer any hazard logging to the main cycle between IF and ID
}


///////////////////////////////////////////////////////////////////////////////


/// RESERVED FOR PRINTING REGISTER TRACE AFTER EACH CLOCK CYCLE ///
void print_register_trace(regfile_t* regfile_p)
{
  // print
  for (uint8_t i = 0; i < 8; i++)//8 clms
  {
    for (uint8_t j = 0; j < 4; j++)//of 4 registers each
    {
      printf("r%2d=%08x ", i * 4 + j, regfile_p->R[i * 4 + j]);
    }
    printf("\n");
  }
  printf("\n");
}

#endif // __STAGE_HELPERS_H__
