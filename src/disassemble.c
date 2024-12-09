#include "disassemble.h"
#include <stdio.h>

void disassemble(uint32_t addr, uint32_t instruction, char* result, size_t buf_size, struct symbols* symbols) {
    unsigned int opcode = instruction & 0x7f;
    unsigned int rd = (instruction >> 7) & 0x1f;
    unsigned int funct3 = (instruction >> 12) & 0x7;
    unsigned int rs1 = (instruction >> 15) & 0x1f;
    unsigned int rs2 = (instruction >> 20) & 0x1f;
    unsigned int funct7 = (instruction >> 25) & 0x7f;

    int imm_i = (int)(instruction & 0xfff00000) >> 20;
    int imm_s = ((int)(instruction & 0xfe000000) >> 25) | ((instruction >> 7) & 0x1f);
    int imm_b = ((int)(instruction & 0x80000000) >> 19) | ((instruction & 0x80) << 4) | ((instruction >> 20) & 0x7e0) | ((instruction >> 7) & 0x1e);
    int imm_u = instruction & 0xfffff000;
    int imm_j = ((int)(instruction & 0x80000000) >> 11) | (instruction & 0xff000) | ((instruction >> 9) & 0x800) | ((instruction >> 20) & 0x7fe);

    switch (opcode) {
        case 0x33: // R-type
            switch (funct3) {
                case 0x0:
                    if (funct7 == 0x00) {
                        snprintf(result, buf_size, "add x%d, x%d, x%d", rd, rs1, rs2);
                    } else if (funct7 == 0x20) {
                        snprintf(result, buf_size, "sub x%d, x%d, x%d", rd, rs1, rs2);
                    }
                    break;
                case 0x1:
                    snprintf(result, buf_size, "sll x%d, x%d, x%d", rd, rs1, rs2);
                    break;
                case 0x2:
                    snprintf(result, buf_size, "slt x%d, x%d, x%d", rd, rs1, rs2);
                    break;
                case 0x3:
                    snprintf(result, buf_size, "sltu x%d, x%d, x%d", rd, rs1, rs2);
                    break;
                case 0x4:
                    snprintf(result, buf_size, "xor x%d, x%d, x%d", rd, rs1, rs2);
                    break;
                case 0x5:
                    if (funct7 == 0x00) {
                        snprintf(result, buf_size, "srl x%d, x%d, x%d", rd, rs1, rs2);
                    } else if (funct7 == 0x20) {
                        snprintf(result, buf_size, "sra x%d, x%d, x%d", rd, rs1, rs2);
                    }
                    break;
                case 0x6:
                    snprintf(result, buf_size, "or x%d, x%d, x%d", rd, rs1, rs2);
                    break;
                case 0x7:
                    snprintf(result, buf_size, "and x%d, x%d, x%d", rd, rs1, rs2);
                    break;
            }
            break;
        case 0x13: // I-type
            switch (funct3) {
                case 0x0:
                    snprintf(result, buf_size, "addi x%d, x%d, %d", rd, rs1, imm_i);
                    break;
                case 0x2:
                    snprintf(result, buf_size, "slti x%d, x%d, %d", rd, rs1, imm_i);
                    break;
                case 0x3:
                    snprintf(result, buf_size, "sltiu x%d, x%d, %d", rd, rs1, imm_i);
                    break;
                case 0x4:
                    snprintf(result, buf_size, "xori x%d, x%d, %d", rd, rs1, imm_i);
                    break;
                case 0x6:
                    snprintf(result, buf_size, "ori x%d, x%d, %d", rd, rs1, imm_i);
                    break;
                case 0x7:
                    snprintf(result, buf_size, "andi x%d, x%d, %d", rd, rs1, imm_i);
                    break;
                case 0x1:
                    snprintf(result, buf_size, "slli x%d, x%d, %d", rd, rs1, imm_i & 0x1f);
                    break;
                case 0x5:
                    if (funct7 == 0x00) {
                        snprintf(result, buf_size, "srli x%d, x%d, %d", rd, rs1, imm_i & 0x1f);
                    } else if (funct7 == 0x20) {
                        snprintf(result, buf_size, "srai x%d, x%d, %d", rd, rs1, imm_i & 0x1f);
                    }
                    break;
            }
            break;
        case 0x03: // Load
            switch (funct3) {
                case 0x0:
                    snprintf(result, buf_size, "lb x%d, %d(x%d)", rd, imm_i, rs1);
                    break;
                case 0x1:
                    snprintf(result, buf_size, "lh x%d, %d(x%d)", rd, imm_i, rs1);
                    break;
                case 0x2:
                    snprintf(result, buf_size, "lw x%d, %d(x%d)", rd, imm_i, rs1);
                    break;
                case 0x4:
                    snprintf(result, buf_size, "lbu x%d, %d(x%d)", rd, imm_i, rs1);
                    break;
                case 0x5:
                    snprintf(result, buf_size, "lhu x%d, %d(x%d)", rd, imm_i, rs1);
                    break;
            }
            break;
        case 0x23: // S-type
            switch (funct3) {
                case 0x0:
                    snprintf(result, buf_size, "sb x%d, %d(x%d)", rs2, imm_s, rs1);
                    break;
                case 0x1:
                    snprintf(result, buf_size, "sh x%d, %d(x%d)", rs2, imm_s, rs1);
                    break;
                case 0x2:
                    snprintf(result, buf_size, "sw x%d, %d(x%d)", rs2, imm_s, rs1);
                    break;
            }
            break;
        case 0x63: // B-type
            switch (funct3) {
                case 0x0:
                    snprintf(result, buf_size, "beq x%d, x%d, %d", rs1, rs2, imm_b);
                    break;
                case 0x1:
                    snprintf(result, buf_size, "bne x%d, x%d, %d", rs1, rs2, imm_b);
                    break;
                case 0x4:
                    snprintf(result, buf_size, "blt x%d, x%d, %d", rs1, rs2, imm_b);
                    break;
                case 0x5:
                    snprintf(result, buf_size, "bge x%d, x%d, %d", rs1, rs2, imm_b);
                    break;
                case 0x6:
                    snprintf(result, buf_size, "bltu x%d, x%d, %d", rs1, rs2, imm_b);
                    break;
                case 0x7:
                    snprintf(result, buf_size, "bgeu x%d, x%d, %d", rs1, rs2, imm_b);
                    break;
            }
            break;
        case 0x37: // LUI
            snprintf(result, buf_size, "lui x%d, %d", rd, imm_u);
            break;
        case 0x17: // AUIPC
            snprintf(result, buf_size, "auipc x%d, %d", rd, imm_u);
            break;
        case 0x6F: // JAL
            snprintf(result, buf_size, "jal x%d, %d", rd, imm_j);
            break;
        case 0x67: // JALR
            snprintf(result, buf_size, "jalr x%d, %d(x%d)", rd, imm_i, rs1);
            break;
        case 0x73: // ECALL
            snprintf(result, buf_size, "ecall");
            break;
        default:
            snprintf(result, buf_size, "unknown");
            break;
    }
}