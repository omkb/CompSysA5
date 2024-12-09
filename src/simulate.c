#include "simulate.h"
#include "memory.h"
#include "read_elf.h"
#include <stdio.h>
#include <stdlib.h>

// Define the system call codes
#define SYSCALL_GETCHAR 1
#define SYSCALL_PUTCHAR 2
#define SYSCALL_EXIT 3
#define SYSCALL_EXIT_ALT 93

struct Stat simulate(struct memory *mem, int start_addr, FILE *log_file, struct symbols* symbols) {
    struct Stat stats = {0};
    int pc = start_addr;
    int registers[32] = {0};
    int running = 1;

    while (running) {
        unsigned int instruction = memory_rd_w(mem, pc);
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
                        if (funct7 == 0x00) { // ADD
                            registers[rd] = registers[rs1] + registers[rs2];
                        } else if (funct7 == 0x20) { // SUB
                            registers[rd] = registers[rs1] - registers[rs2];
                        }
                        break;
                    case 0x1: // SLL
                        registers[rd] = registers[rs1] << (registers[rs2] & 0x1f);
                        break;
                    case 0x2: // SLT
                        registers[rd] = (int)registers[rs1] < (int)registers[rs2];
                        break;
                    case 0x3: // SLTU
                        registers[rd] = (unsigned int)registers[rs1] < (unsigned int)registers[rs2];
                        break;
                    case 0x4: // XOR
                        registers[rd] = registers[rs1] ^ registers[rs2];
                        break;
                    case 0x5:
                        if (funct7 == 0x00) { // SRL
                            registers[rd] = (unsigned int)registers[rs1] >> (registers[rs2] & 0x1f);
                        } else if (funct7 == 0x20) { // SRA
                            registers[rd] = (int)registers[rs1] >> (registers[rs2] & 0x1f);
                        }
                        break;
                    case 0x6: // OR
                        registers[rd] = registers[rs1] | registers[rs2];
                        break;
                    case 0x7: // AND
                        registers[rd] = registers[rs1] & registers[rs2];
                        break;
                }
                break;
            case 0x13: // I-type
                switch (funct3) {
                    case 0x0: // ADDI
                        registers[rd] = registers[rs1] + imm_i;
                        break;
                    case 0x2: // SLTI
                        registers[rd] = (int)registers[rs1] < imm_i;
                        break;
                    case 0x3: // SLTIU
                        registers[rd] = (unsigned int)registers[rs1] < (unsigned int)imm_i;
                        break;
                    case 0x4: // XORI
                        registers[rd] = registers[rs1] ^ imm_i;
                        break;
                    case 0x6: // ORI
                        registers[rd] = registers[rs1] | imm_i;
                        break;
                    case 0x7: // ANDI
                        registers[rd] = registers[rs1] & imm_i;
                        break;
                    case 0x1: // SLLI
                        registers[rd] = registers[rs1] << (imm_i & 0x1f);
                        break;
                    case 0x5:
                        if (funct7 == 0x00) { // SRLI
                            registers[rd] = (unsigned int)registers[rs1] >> (imm_i & 0x1f);
                        } else if (funct7 == 0x20) { // SRAI
                            registers[rd] = (int)registers[rs1] >> (imm_i & 0x1f);
                        }
                        break;
                }
                break;
            case 0x03: // Load
                switch (funct3) {
                    case 0x0: // LB
                        registers[rd] = (char)memory_rd_b(mem, registers[rs1] + imm_i);
                        break;
                    case 0x1: // LH
                        registers[rd] = (short)memory_rd_h(mem, registers[rs1] + imm_i);
                        break;
                    case 0x2: // LW
                        registers[rd] = memory_rd_w(mem, registers[rs1] + imm_i);
                        break;
                    case 0x4: // LBU
                        registers[rd] = (unsigned char)memory_rd_b(mem, registers[rs1] + imm_i);
                        break;
                    case 0x5: // LHU
                        registers[rd] = (unsigned short)memory_rd_h(mem, registers[rs1] + imm_i);
                        break;
                }
                break;
            case 0x23: // S-type
                switch (funct3) {
                    case 0x0: // SB
                        memory_wr_b(mem, registers[rs1] + imm_s, registers[rs2] & 0xff);
                        break;
                    case 0x1: // SH
                        memory_wr_h(mem, registers[rs1] + imm_s, registers[rs2] & 0xffff);
                        break;
                    case 0x2: // SW
                        memory_wr_w(mem, registers[rs1] + imm_s, registers[rs2]);
                        break;
                }
                break;
            case 0x63: // B-type
                switch (funct3) {
                    case 0x0: // BEQ
                        if (registers[rs1] == registers[rs2]) {
                            pc += imm_b;
                            continue;
                        }
                        break;
                    case 0x1: // BNE
                        if (registers[rs1] != registers[rs2]) {
                            pc += imm_b;
                            continue;
                        }
                        break;
                    case 0x4: // BLT
                        if ((int)registers[rs1] < (int)registers[rs2]) {
                            pc += imm_b;
                            continue;
                        }
                        break;
                    case 0x5: // BGE
                        if ((int)registers[rs1] >= (int)registers[rs2]) {
                            pc += imm_b;
                            continue;
                        }
                        break;
                    case 0x6: // BLTU
                        if ((unsigned int)registers[rs1] < (unsigned int)registers[rs2]) {
                            pc += imm_b;
                            continue;
                        }
                        break;
                    case 0x7: // BGEU
                        if ((unsigned int)registers[rs1] >= (unsigned int)registers[rs2]) {
                            pc += imm_b;
                            continue;
                        }
                        break;
                }
                break;
            case 0x37: // LUI
                registers[rd] = imm_u;
                break;
            case 0x17: // AUIPC
                registers[rd] = pc + imm_u;
                break;
            case 0x6F: // JAL
                registers[rd] = pc + 4;
                pc += imm_j;
                continue;
            case 0x67: // JALR
                registers[rd] = pc + 4;
                pc = (registers[rs1] + imm_i) & ~1;
                continue;
            case 0x73: // ECALL
                switch (registers[17]) { // A7
                    case SYSCALL_GETCHAR:
                        registers[10] = getchar(); // A0
                        break;
                    case SYSCALL_PUTCHAR:
                        putchar(registers[10]); // A0
                        break;
                    case SYSCALL_EXIT:
                    case SYSCALL_EXIT_ALT:
                        running = 0;
                        break;
                }
                break;
        }

        pc += 4;
        stats.insns++;
    }

    return stats;
}