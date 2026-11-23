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
            alu_control = 0x1; // SUB
          } else if (idex_reg.instr.rtype.funct7 == 0x1) {
            alu_control = 0xB; //MUL
          } else {
            alu_control = 0x0;//ADD
          }
          break;
        case 0x1: // SLL/MULH
          if (idex_reg.instr.rtype.funct7 == 0x1) {
            alu_control = 0xC; //MULH
          } else {
            alu_control = 0x8; //SLL(Shift Left Logical)
          }
          break;
        case 0x2: //SLT
          alu_control = 0x2;
          break;
        case 0x3:// SLTU
          alu_control = 0x3;
          break;
        case 0x4:// XOR
          alu_control = 0x4;
          break;
        case 0x5:// SRA/SRL
          alu_control = (idex_reg.instr.rtype.funct7 == 0x20) ? 0xA : 0x9; // SRA : SRL
          break;
        case 0x6:// OR
          alu_control = 0x6;
          break;
        case 0x7:// AND
          alu_control = 0x7;
          break;
        default:
          alu_control = 0x0;// defaulting to ADD
          break;
      }
      break;


    case 0x13: //I-type immediate
      switch(idex_reg.instr.itype.funct3) {
        case 0x0: //ADDI
          alu_control = 0x0;//ADD
          break;
        case 0x1://SLLI
          alu_control = 0x8;//SLL (Shift Left Logical)
          break;
           case 0x2://SLTI
          alu_control = 0x2;//SLT
          break;
        case 0x3://SLTIU
          alu_control = 0x3;//SLTU
          break;
        case 0x4://XORI
          alu_control = 0x4;//XOR
          break;
        case 0x5: // SRLI/SRAI
          //Check bit 30 of immediate to distinguish SRLI from SRAI
          alu_control = ((idex_reg.instr.itype.imm >> 10) & 0x1) ? 0xA : 0x9;//SRAI: SRLI
          break;
        case 0x6://ORI
          alu_control = 0x6;//OR
          break;
        case 0x7://ANDI
          alu_control = 0x7;//AND
          break;
       
        default:
          alu_control = 0x0;//Default to ADD
          break;
      }
      break;

    case 0x03:// Load (I-type)
   switch(idex_reg.instr.itype.funct3) {
      case 0x0://LB
      case 0x1://LH
      case 0x2://LW
      case 0x4://LBU
      case 0x5://LHU
         alu_control = 0x0;//All use ADD for address calc..
         break;
   }
   break;
case 0x23: // Store (S-type)
   switch(idex_reg.instr.stype.funct3) {
      case 0x0: //SB
      case 0x1://SH
      case 0x2: //SW
         alu_control = 0x0; //All use ADD for address calc.
         break;
   }
   break;
case 0x67://JALR (I-type)
   alu_control = 0x0; //ADD for target calculation
   break;
    case 0x63: // Branch
      switch(idex_reg.instr.sbtype.funct3) {
        case 0x0://BEQ
        case 0x1://BNE
          alu_control = 0x1; //SUB for comparison
          break;
        case 0x4://BLT
        case 0x5: //BGE
          alu_control = 0x2; //SLT for comparison
          break;
        case 0x6://BLTU
        case 0x7: //BGEU
          alu_control = 0x3; //SSLTU for comparison
          break;
        default:
          alu_control = 0x0; //default to add
          break;
      }
      break;
    case 0x37://LUI Load Upper Immediate
      alu_control = 0x0; // ADD (0 + immediate)
      break;
    case 0x17://AUIPC Add Upper Immediate to PC
      alu_control = 0x0; // ADD (PC + immediate)
      break;
    case 0x6F:// JAL Jump and Link
      alu_control = 0x0;//ADD for PC + 4 calculation
      break;
    default:
      alu_control = 0x0; //Default to ADD
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
      result = alu_inp1 << (alu_inp2 & 0x1F); // Only usin lower 5 bits for shift amount
      break;
    case 0x9://SRL (Shift Right Logical)
      result = alu_inp1 >> (alu_inp2 & 0x1F); // Only usin lower 5 bits for shift amount
      break;
    case 0xA://SRA (Shift Right Arithmetic)
      result = (uint32_t)((int32_t)alu_inp1 >> (alu_inp2 & 0x1F));
      //Arithmetic shift
      break;
    case 0xB://MUL (Multiply)
      result = (uint32_t)((int32_t)alu_inp1 * (int32_t)alu_inp2); //32-bit multiply
      break;
    case 0xC://MULH (Multiply High)
      {
        int64_t product = (int64_t)((int32_t)alu_inp1) * (int64_t)((int32_t)alu_inp2);
        result = (uint32_t)(product >> 32); // Upper 32 bits
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
    case 0x13://I-type immediate (ADDI, XORI, ORI, ANDI, SLTI, SLTIU)
    case 0x03://I-type load (LB, LH, LW, LBU, LHU)
    case 0x67://I-type JALR
      //I-type: 12-bit immediate, sign-extended
      imm_val = sign_extend_number(instruction.itype.imm, 12);
      break;
      
    case 0x23: //S-type store (SB, SH, SW)
      //S-type: 12-bit immediate split between imm[11:5] and imm[4:0]
      imm_val = (instruction.stype.imm7 << 5) | instruction.stype.imm5;
      imm_val = sign_extend_number(imm_val, 12);
      break;
      
    case 0x63://B-type branch (BEQ, BNE, BLT, BGE, BLTU, BGEU)
      //B-type: 13-bit immediate for branch offset
      imm_val = get_branch_offset(instruction);
      break;
      
    case 0x37://U-type LUI
    case 0x17://U-type AUIPC
      //U-type: 20-bit immediate in upper 20 bitsss
      imm_val = instruction.utype.imm << 12;
      break;
      
    case 0x6F://J-type JAL
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
  
  //Extract destination register index
  uint32_t rd_idx = 0;
  
  switch(instruction.opcode) {
    case 0x33://R-type (ADD, SUB, XOR, OR, AND, SLT, SLTU)
      rd_idx = instruction.rtype.rd;
      idex_reg.reg_write = (rd_idx != 0); // Write to register if not x0
      idex_reg.mem_read = false;
      idex_reg.mem_write = false;
      idex_reg.branch = false;
      idex_reg.alu_src = false; // Use rs2 data, not immediate
      break;
      
    case 0x13://I-type immediate (ADDI, XORI, ORI, ANDI, SLTI, SLTIU)
      rd_idx = instruction.itype.rd;
      idex_reg.reg_write = (rd_idx != 0); // Write to register if not x0
      idex_reg.mem_read = false;
      idex_reg.mem_write = false;
      idex_reg.branch = false;
      idex_reg.alu_src = true; // Use immediate value
      break;
      
    case 0x03://I-type load (LB, LH, LW, LBU, LHU)
      rd_idx = instruction.itype.rd;
      idex_reg.reg_write = (rd_idx != 0); // Write to register if not x0
      idex_reg.mem_read = true;
      idex_reg.mem_write = false;
      idex_reg.branch = false;
      idex_reg.alu_src = true; // Use immediate for address calculation
      break;
      
    case 0x23://S-type store (SB, SH, SW)
      rd_idx = 0; //storee instructions don't write to registers
      idex_reg.reg_write = false;
      idex_reg.mem_read = false;
      idex_reg.mem_write = true;
      idex_reg.branch = false;
      idex_reg.alu_src = true;//Use imm for address calculation
      break;
      
    case 0x63://B-type branch (BEQ, BNE, BLT, BGE, BLTU, BGEU)
      rd_idx = 0;//Branch instructions DO NOt write to registers
      idex_reg.reg_write = false;
      idex_reg.mem_read = false;
      idex_reg.mem_write = false;
      idex_reg.branch = true;
      idex_reg.alu_src = false;//Use rs2 data for comparison
      break;
      
    case 0x37://U-type LUI
      rd_idx = instruction.utype.rd;
      idex_reg.reg_write = (rd_idx != 0);//Write to register if not x0
      idex_reg.mem_read = false;
      idex_reg.mem_write = false;
      idex_reg.branch = false;
      idex_reg.alu_src = true;//Use imm value
      break;
      
    case 0x17: //U-type AUIPC
      rd_idx = instruction.utype.rd;
      idex_reg.reg_write = (rd_idx != 0); //Write to register if not x0
      idex_reg.mem_read = false;
      idex_reg.mem_write = false;
      idex_reg.branch = false;
      idex_reg.alu_src = true;//Use imm value
      break;
      
    case 0x6F: //J-type JAL
      rd_idx = instruction.ujtype.rd;
      idex_reg.reg_write = (rd_idx != 0); //Write to register if not x0
      idex_reg.mem_read = false;
      idex_reg.mem_write = false;
      idex_reg.branch = false;
      idex_reg.alu_src = true; //Use imm for jump calculation
      break;
      
    case 0x67: //I-type JALR
      rd_idx = instruction.itype.rd;
      idex_reg.reg_write = (rd_idx != 0); //write to register if not x0
      idex_reg.mem_read = false;
      idex_reg.mem_write = false;
      idex_reg.branch = false;
      idex_reg.alu_src = true; //Use imm value
      break;
      
    default: //deafulting to Unknown opcode
      rd_idx = 0;
      idex_reg.reg_write = false;
      idex_reg.mem_read = false;
      idex_reg.mem_write = false;
      idex_reg.branch = false;
      idex_reg.alu_src = false;
      break;
  }
  
  // Set the destination register index
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
  if (instruction.opcode != 0x63) { //Not a branch instruction
    return false;
  }
  
  //eval branch condition based on funct3 field
  switch (instruction.sbtype.funct3) {
    case 0x0: //BEQ Branch if equal
      return (rs1_data == rs2_data);
      
    case 0x1: //BNE Branch if not equal
      return (rs1_data != rs2_data);
      
    case 0x4: //BLT Branch if less than (signed)
      return ((int32_t)rs1_data < (int32_t)rs2_data);
      
    case 0x5: //BGE Branch if greater than or equal (signed)
      return ((int32_t)rs1_data >= (int32_t)rs2_data);
      
    case 0x6: //BLTU Branch if less than (unsigned)
      return (rs1_data < rs2_data);
      
    case 0x7://BGEU - Branch if greater than or equal (unsigned)
      return (rs1_data >= rs2_data);
      
    default:
      return false; //Invalid branch instruction
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
  /**
   * YOUR CODE HERE
   */
}

/**
 * Task   : Sets the pipeline wires for the hazard unit's control signals
 *           based on the pipeline register values.
 * input  : pipeline_regs_t*, pipeline_wires_t*
 * output : None
*/
void detect_hazard(pipeline_regs_t* pregs_p, pipeline_wires_t* pwires_p, regfile_t* regfile_p)
{
  /**
   * YOUR CODE HERE //adding a touch of this and that HEEHEHEHEHEHEH tuntungtungtungtung sahurrrrrr
   */
}


///////////////////////////////////////////////////////////////////////////////


/// RESERVED FOR PRINTING REGISTER TRACE AFTER EACH CLOCK CYCLE ///
void print_register_trace(regfile_t* regfile_p)
{
  // print
  for (uint8_t i = 0; i < 8; i++)       // 8 columns
  {
    for (uint8_t j = 0; j < 4; j++)     // of 4 registers each
    {
      printf("r%2d=%08x ", i * 4 + j, regfile_p->R[i * 4 + j]);
    }
    printf("\n");
  }
  printf("\n");
}

#endif // __STAGE_HELPERS_H__
