#include "simulate.h"
#include "memory.h"
#include "read_elf.h"
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
        int next_pc = pc + 4; // Always set default next PC

        if (log_file)
            fprintf(log_file, "%6d     %05x : %08x  ", stats.insns, pc, instruction);

        // Decode instruction fields
        unsigned int opcode = instruction & 0x7f;
        unsigned int rd = (instruction >> 7) & 0x1f;
        unsigned int funct3 = (instruction >> 12) & 0x7;
        unsigned int rs1 = (instruction >> 15) & 0x1f;
        unsigned int rs2 = (instruction >> 20) & 0x1f;
        unsigned int funct7 = (instruction >> 25) & 0x7f;

        // Handle immediate values carefully to ensure proper sign extension
        int imm_i = ((int)instruction) >> 20;
        int imm_s = (((int)instruction) >> 20) & ~0x1f | ((instruction >> 7) & 0x1f);
        int imm_b = ((instruction & 0x80000000) ? -1 << 12 : 0) |
                    ((instruction & 0x80) << 4) | ((instruction >> 20) & 0x7e0) |
                    ((instruction >> 7) & 0x1e);
        int imm_u = instruction & 0xfffff000;
        int imm_j = ((instruction & 0x80000000) ? -1 << 20 : 0) |
                    (instruction & 0xff000) | ((instruction >> 9) & 0x800) |
                    ((instruction >> 20) & 0x7fe);

        // Get register values before modification
        int rs1_val = registers[rs1];
        int rs2_val = registers[rs2];
        int reg_write = 0;
        int reg_write_value = 0;

        switch (opcode)
        {
        case 0x33: // R-type
            if (rd != 0)
            {
                reg_write = 1;
                switch (funct3)
                {
                case 0x0: // ADD/SUB
                    reg_write_value = (funct7 == 0x20) ? rs1_val - rs2_val : rs1_val + rs2_val;
                    break;
                case 0x1: // SLL
                    reg_write_value = rs1_val << (rs2_val & 0x1f);
                    break;
                case 0x2: // SLT
                    reg_write_value = ((int)rs1_val < (int)rs2_val) ? 1 : 0;
                    break;
                case 0x3: // SLTU
                    reg_write_value = ((unsigned)rs1_val < (unsigned)rs2_val) ? 1 : 0;
                    break;
                case 0x4: // XOR
                    reg_write_value = rs1_val ^ rs2_val;
                    break;
                case 0x5: // SRL/SRA
                    reg_write_value = (funct7 == 0x20) ? ((int)rs1_val >> (rs2_val & 0x1f)) : ((unsigned)rs1_val >> (rs2_val & 0x1f));
                    break;
                case 0x6: // OR
                    reg_write_value = rs1_val | rs2_val;
                    break;
                case 0x7: // AND
                    reg_write_value = rs1_val & rs2_val;
                    break;
                }
            }
            break;

        case 0x13: // I-type
            if (rd != 0)
            {
                reg_write = 1;
                switch (funct3)
                {
                case 0x0: // ADDI
                    reg_write_value = rs1_val + imm_i;
                    break;
                case 0x2: // SLTI
                    reg_write_value = ((int)rs1_val < imm_i) ? 1 : 0;
                    break;
                case 0x3: // SLTIU
                    reg_write_value = ((unsigned)rs1_val < (unsigned)imm_i) ? 1 : 0;
                    break;
                case 0x4: // XORI
                    reg_write_value = rs1_val ^ imm_i;
                    break;
                case 0x6: // ORI
                    reg_write_value = rs1_val | imm_i;
                    break;
                case 0x7: // ANDI
                    reg_write_value = rs1_val & imm_i;
                    break;
                case 0x1: // SLLI
                    reg_write_value = rs1_val << (imm_i & 0x1f);
                    break;
                case 0x5: // SRLI/SRAI
                    reg_write_value = (imm_i & 0x400) ? ((int)rs1_val >> (imm_i & 0x1f)) : ((unsigned)rs1_val >> (imm_i & 0x1f));
                    break;
                }
            }
            break;

        case 0x03: // Load
            if (rd != 0)
            {
                reg_write = 1;
                int addr = rs1_val + imm_i;
                switch (funct3)
                {
                case 0x0: // LB
                    reg_write_value = (int)(char)memory_rd_b(mem, addr);
                    break;
                case 0x1: // LH
                    reg_write_value = (int)(short)memory_rd_h(mem, addr);
                    break;
                case 0x2: // LW
                    reg_write_value = memory_rd_w(mem, addr);
                    break;
                case 0x4: // LBU
                    reg_write_value = (unsigned char)memory_rd_b(mem, addr);
                    break;
                case 0x5: // LHU
                    reg_write_value = (unsigned short)memory_rd_h(mem, addr);
                    break;
                }
            }
            break;

        case 0x23: // Store
        {
            int addr = rs1_val + imm_s;
            switch (funct3)
            {
            case 0x0: // SB
                memory_wr_b(mem, addr, rs2_val);
                break;
            case 0x1: // SH
                memory_wr_h(mem, addr, rs2_val);
                break;
            case 0x2: // SW
                memory_wr_w(mem, addr, rs2_val);
                break;
            }
            if (log_file)
                fprintf(log_file, "    M[%x] <- %x", addr, rs2_val);
        }
        break;

        case 0x63: // Branch
        {
            int take_branch = 0;
            switch (funct3)
            {
            case 0x0: // BEQ
                take_branch = (rs1_val == rs2_val);
                break;
            case 0x1: // BNE
                take_branch = (rs1_val != rs2_val);
                break;
            case 0x4: // BLT
                take_branch = ((int)rs1_val < (int)rs2_val);
                break;
            case 0x5: // BGE
                take_branch = ((int)rs1_val >= (int)rs2_val);
                break;
            case 0x6: // BLTU
                take_branch = ((unsigned)rs1_val < (unsigned)rs2_val);
                break;
            case 0x7: // BGEU
                take_branch = ((unsigned)rs1_val >= (unsigned)rs2_val);
                break;
            }
            if (take_branch)
            {
                next_pc = pc + imm_b;
                if (log_file)
                    fprintf(log_file, "    {T}");
            }
        }
        break;

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

        case 0x73: // ECALL
            switch (registers[17])
            {       // a7 register
            case 1: // GETCHAR
                registers[10] = getchar();
                break;
            case 2:                            // PUTCHAR
                putchar(registers[10] & 0xFF); // Ensure we only output one byte
                fflush(stdout);
                break;
            case 3:
            case 93: // EXIT
                running = 0;
                break;
            }
            break;
        }

        // Update register if write is needed
        if (reg_write && rd != 0)
        {
            registers[rd] = reg_write_value;
            if (log_file)
            {
                fprintf(log_file, "    R[%2d] <- %x", rd, reg_write_value);
            }
        }

        if (log_file)
            fprintf(log_file, "\n");

        // Update PC and instruction count
        pc = next_pc;
        stats.insns++;
    }

    return stats;
}