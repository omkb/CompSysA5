#include "simulate.h"
#include "memory.h"
#include "read_elf.h"
#include "disassemble.h"
#include <stdio.h>
#include <stdlib.h>

struct Stat simulate(struct memory *mem, int start_addr, FILE *log_file, struct symbols *symbols)
{
    struct Stat stats = {0};
    int pc = start_addr;
    int registers[32] = {0};
    int running = 1;

    while (running)
    {
        unsigned int instruction = memory_rd_w(mem, pc);
        int next_pc = pc + 4;

        char disassembled[64];
        disassemble(pc, instruction, disassembled, sizeof(disassembled), symbols);

        if (log_file)
            fprintf(log_file, "%6ld     %05x : %08x  %-20s", stats.insns, pc, instruction, disassembled);

        unsigned int opcode = instruction & 0x7f;
        unsigned int rd = (instruction >> 7) & 0x1f;
        unsigned int funct3 = (instruction >> 12) & 0x7;
        unsigned int rs1 = (instruction >> 15) & 0x1f;
        unsigned int rs2 = (instruction >> 20) & 0x1f;
        unsigned int funct7 = (instruction >> 25) & 0x7f;

        int imm_i = ((int)instruction) >> 20;
        int imm_s = (((int)instruction) >> 20 & ~0x1f) | ((instruction >> 7) & 0x1f);
        int imm_b = ((int)instruction >> 31 << 12) | ((instruction >> 7) & 0x1e) |
                    ((instruction >> 20) & 0x7e0) | ((instruction << 4) & 0x800);
        int imm_u = instruction & 0xfffff000;
        int imm_j = ((int)instruction >> 31 << 20) | (instruction & 0xff000) |
                    ((instruction >> 9) & 0x800) | ((instruction >> 20) & 0x7fe);

        int rs1_val = registers[rs1], rs2_val = registers[rs2];
        int reg_write = 0, reg_write_value = 0;

        switch (opcode)
        {
        case 0x33: // R-type ALU operations
            if (rd != 0)
            {
                reg_write = 1;
                if (funct7 == 0x01)
                { // M-extension instructions
                    switch (funct3)
                    {
                    case 0x0:
                        reg_write_value = rs1_val * rs2_val;
                        break; // MUL
                    case 0x1:
                        reg_write_value = ((long long)rs1_val * (long long)rs2_val) >> 32;
                        break; // MULH
                    case 0x4:
                        reg_write_value = rs2_val ? (int)rs1_val / (int)rs2_val : -1;
                        break; // DIV
                    case 0x5:
                        reg_write_value = rs2_val ? (unsigned)rs1_val / (unsigned)rs2_val : (unsigned)-1;
                        break; // DIVU
                    case 0x6:
                        reg_write_value = rs2_val ? (int)rs1_val % (int)rs2_val : rs1_val;
                        break; // REM
                    case 0x7:
                        reg_write_value = rs2_val ? (unsigned)rs1_val % (unsigned)rs2_val : (unsigned) rs1_val;
                        break; // REMU
                    }
                }
                else
                {
                    switch (funct3)
                    {
                    case 0x0:
                        reg_write_value = (funct7 == 0x20) ? rs1_val - rs2_val : rs1_val + rs2_val;
                        break; // ADD/SUB
                    case 0x1:
                        reg_write_value = rs1_val << (rs2_val & 0x1f);
                        break; // SLL
                    case 0x2:
                        reg_write_value = (int)rs1_val < (int)rs2_val;
                        break; // SLT
                    case 0x3:
                        reg_write_value = (unsigned)rs1_val < (unsigned)rs2_val;
                        break; // SLTU
                    case 0x4:
                        reg_write_value = rs1_val ^ rs2_val;
                        break; // XOR
                    case 0x5:
                        reg_write_value = (funct7 == 0x20) ? (int)rs1_val >> (rs2_val & 0x1f) : (int)(unsigned)rs1_val >> (rs2_val & 0x1f);
                        break; // SRL/SRA
                    case 0x6:
                        reg_write_value = rs1_val | rs2_val;
                        break; // OR
                    case 0x7:
                        reg_write_value = rs1_val & rs2_val;
                        break; // AND
                    }
                }
            }
            break;

        case 0x13: // I-type immediate operations
            if (rd != 0)
            {
                reg_write = 1;
                switch (funct3)
                {
                case 0x0:
                    reg_write_value = rs1_val + imm_i;
                    break; // ADDI
                case 0x1:
                    reg_write_value = rs1_val << (imm_i & 0x1f);
                    break; // SLLI
                case 0x2:
                    reg_write_value = (int)rs1_val < imm_i;
                    break; // SLTI
                case 0x3:
                    reg_write_value = (unsigned)rs1_val < (unsigned)imm_i;
                    break; // SLTIU
                case 0x4:
                    reg_write_value = rs1_val ^ imm_i;
                    break; // XORI
                case 0x5:
                    reg_write_value = (imm_i & 0x400) ? (int)rs1_val >> (imm_i & 0x1f) : (int)(unsigned)rs1_val >> (imm_i & 0x1f);
                    break; // SRLI/SRAI
                case 0x6:
                    reg_write_value = rs1_val | imm_i;
                    break; // ORI
                case 0x7:
                    reg_write_value = rs1_val & imm_i;
                    break; // ANDI
                }
            }
            break;

        case 0x03: // Load instructions
            if (rd != 0)
            {
                reg_write = 1;
                int addr = rs1_val + imm_i;
                switch (funct3)
                {
                case 0x0:
                    reg_write_value = (int)(signed char)memory_rd_b(mem, addr);
                    break; // LB
                case 0x1:
                    reg_write_value = (int)(signed short)memory_rd_h(mem, addr);
                    break; // LH
                case 0x2:
                    reg_write_value = memory_rd_w(mem, addr);
                    break; // LW
                case 0x4:
                    reg_write_value = (unsigned char)memory_rd_b(mem, addr);
                    break; // LBU
                case 0x5:
                    reg_write_value = (unsigned short)memory_rd_h(mem, addr);
                    break; // LHU
                }
            }
            break;

        case 0x23: // Store instructions
        {
            int addr = rs1_val + imm_s;
            switch (funct3)
            {
            case 0x0:
                memory_wr_b(mem, addr, rs2_val);
                break; // SB
            case 0x1:
                memory_wr_h(mem, addr, rs2_val);
                break; // SH
            case 0x2:
                memory_wr_w(mem, addr, rs2_val);
                break; // SW
            }
            if (log_file)
                fprintf(log_file, "    M[%x] <- %x", addr, rs2_val);
            break;
        }

        case 0x63: // Branch instructions
        {
            int take_branch = 0;
            switch (funct3)
            {
            case 0x0:
                take_branch = (rs1_val == rs2_val);
                break; // BEQ
            case 0x1:
                take_branch = (rs1_val != rs2_val);
                break; // BNE
            case 0x4:
                take_branch = ((int)rs1_val < (int)rs2_val);
                break; // BLT
            case 0x5:
                take_branch = ((int)rs1_val >= (int)rs2_val);
                break; // BGE
            case 0x6:
                take_branch = ((unsigned)rs1_val < (unsigned)rs2_val);
                break; // BLTU
            case 0x7:
                take_branch = ((unsigned)rs1_val >= (unsigned)rs2_val);
                break; // BGEU
            }
            if (take_branch)
            {
                next_pc = pc + imm_b;
                if (log_file)
                    fprintf(log_file, "    {T}");
            }
            break;
        }

        case 0x37: // LUI
            if (rd != 0)
            {
                reg_write = 1;
                reg_write_value = imm_u;
            }
            break;
        case 0x17: // AUIPC
            if (rd != 0)
            {
                reg_write = 1;
                reg_write_value = pc + imm_u;
            }
            break;

        case 0x6F: // JAL
            if (rd != 0)
            {
                reg_write = 1;
                reg_write_value = pc + 4;
            }
            next_pc = pc + imm_j;
            break;
        case 0x67: // JALR
            if (rd != 0)
            {
                reg_write = 1;
                reg_write_value = pc + 4;
            }
            next_pc = (rs1_val + imm_i) & ~1;
            break;

        case 0x73: // System calls (ECALL)
            switch (registers[17])
            {
            case 1:
                registers[10] = getchar();
                break; // SYSCALL_GETCHAR
            case 2:
                putchar(registers[10]);
                break; // SYSCALL_PUTCHAR
            case 3:
            case 93:
                running = 0;
                break; // SYSCALL_EXIT
            }
            break;

        default:
            break;
        }

        if (reg_write)
        {
            registers[rd] = reg_write_value;
            if (log_file)
                fprintf(log_file, "R[%2d] <- %x", rd, reg_write_value);
        }

        if (log_file)
            fprintf(log_file, "\n");

        pc = next_pc;
        stats.insns++;
    }

    return stats;
}
