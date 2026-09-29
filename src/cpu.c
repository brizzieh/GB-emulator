#include "gb_internal.h"

#include <stdio.h>

#define FLAG_Z 0x80
#define FLAG_N 0x40
#define FLAG_H 0x20
#define FLAG_C 0x10

static uint16_t pair_bc(const GameBoy *gb)
{
    return (uint16_t)(((uint16_t)gb->b << 8) | gb->c);
}

static uint16_t pair_de(const GameBoy *gb)
{
    return (uint16_t)(((uint16_t)gb->d << 8) | gb->e);
}

static uint16_t pair_hl(const GameBoy *gb)
{
    return (uint16_t)(((uint16_t)gb->h << 8) | gb->l);
}

static void set_bc(GameBoy *gb, uint16_t value)
{
    gb->b = (uint8_t)(value >> 8);
    gb->c = (uint8_t)value;
}

static void set_de(GameBoy *gb, uint16_t value)
{
    gb->d = (uint8_t)(value >> 8);
    gb->e = (uint8_t)value;
}

static void set_hl(GameBoy *gb, uint16_t value)
{
    gb->h = (uint8_t)(value >> 8);
    gb->l = (uint8_t)value;
}

static void write16(GameBoy *gb, uint16_t address, uint16_t value)
{
    gb_memory_write(gb, address, (uint8_t)value);
    gb_memory_write(gb, (uint16_t)(address + 1), (uint8_t)(value >> 8));
}

static uint8_t fetch8(GameBoy *gb)
{
    uint8_t value = gb_memory_read(gb, gb->pc);
    if (gb->halt_bug) {
        gb->halt_bug = 0;
    } else {
        gb->pc++;
    }
    return value;
}

static uint16_t fetch16(GameBoy *gb)
{
    uint8_t low = fetch8(gb);
    uint8_t high = fetch8(gb);
    return (uint16_t)(low | ((uint16_t)high << 8));
}

static uint8_t read_r(GameBoy *gb, unsigned index)
{
    switch (index & 7) {
        case 0: return gb->b;
        case 1: return gb->c;
        case 2: return gb->d;
        case 3: return gb->e;
        case 4: return gb->h;
        case 5: return gb->l;
        case 6: return gb_memory_read(gb, pair_hl(gb));
        default: return gb->a;
    }
}

static void write_r(GameBoy *gb, unsigned index, uint8_t value)
{
    switch (index & 7) {
        case 0: gb->b = value; break;
        case 1: gb->c = value; break;
        case 2: gb->d = value; break;
        case 3: gb->e = value; break;
        case 4: gb->h = value; break;
        case 5: gb->l = value; break;
        case 6: gb_memory_write(gb, pair_hl(gb), value); break;
        default: gb->a = value; break;
    }
}

static uint16_t read_rr(GameBoy *gb, unsigned index)
{
    switch (index & 3) {
        case 0: return pair_bc(gb);
        case 1: return pair_de(gb);
        case 2: return pair_hl(gb);
        default: return gb->sp;
    }
}

static void write_rr(GameBoy *gb, unsigned index, uint16_t value)
{
    switch (index & 3) {
        case 0: set_bc(gb, value); break;
        case 1: set_de(gb, value); break;
        case 2: set_hl(gb, value); break;
        default: gb->sp = value; break;
    }
}

static uint16_t read_stack_pair(GameBoy *gb, unsigned index)
{
    if ((index & 3) == 3) return (uint16_t)(((uint16_t)gb->a << 8) | gb->f);
    return read_rr(gb, index);
}

static void write_stack_pair(GameBoy *gb, unsigned index, uint16_t value)
{
    if ((index & 3) == 3) {
        gb->a = (uint8_t)(value >> 8);
        gb->f = (uint8_t)value & 0xF0;
    } else {
        write_rr(gb, index, value);
    }
}

static void push16(GameBoy *gb, uint16_t value)
{
    gb->sp--;
    gb_memory_write(gb, gb->sp, (uint8_t)(value >> 8));
    gb->sp--;
    gb_memory_write(gb, gb->sp, (uint8_t)value);
}

static uint16_t pop16(GameBoy *gb)
{
    uint8_t low = gb_memory_read(gb, gb->sp++);
    uint8_t high = gb_memory_read(gb, gb->sp++);
    return (uint16_t)(low | ((uint16_t)high << 8));
}

static uint8_t condition(const GameBoy *gb, unsigned index)
{
    switch (index & 3) {
        case 0: return !(gb->f & FLAG_Z);
        case 1: return (gb->f & FLAG_Z) != 0;
        case 2: return !(gb->f & FLAG_C);
        default: return (gb->f & FLAG_C) != 0;
    }
}

static void alu(GameBoy *gb, unsigned operation, uint8_t value)
{
    uint8_t old = gb->a;
    uint8_t carry = (gb->f & FLAG_C) ? 1 : 0;
    uint16_t result;

    switch (operation & 7) {
        case 0:
            result = (uint16_t)old + value;
            gb->a = (uint8_t)result;
            gb->f = (gb->a == 0 ? FLAG_Z : 0) |
                (((old & 0xF) + (value & 0xF) > 0xF) ? FLAG_H : 0) |
                (result > 0xFF ? FLAG_C : 0);
            break;
        case 1:
            result = (uint16_t)old + value + carry;
            gb->a = (uint8_t)result;
            gb->f = (gb->a == 0 ? FLAG_Z : 0) |
                (((old & 0xF) + (value & 0xF) + carry > 0xF) ? FLAG_H : 0) |
                (result > 0xFF ? FLAG_C : 0);
            break;
        case 2:
            gb->a = (uint8_t)(old - value);
            gb->f = FLAG_N | (gb->a == 0 ? FLAG_Z : 0) |
                ((old & 0xF) < (value & 0xF) ? FLAG_H : 0) |
                (old < value ? FLAG_C : 0);
            break;
        case 3: {
            uint16_t subtrahend = (uint16_t)value + carry;
            gb->a = (uint8_t)(old - subtrahend);
            gb->f = FLAG_N | (gb->a == 0 ? FLAG_Z : 0) |
                ((old & 0xF) < ((value & 0xF) + carry) ? FLAG_H : 0) |
                (old < subtrahend ? FLAG_C : 0);
            break;
        }
        case 4:
            gb->a = old & value;
            gb->f = FLAG_H | (gb->a == 0 ? FLAG_Z : 0);
            break;
        case 5:
            gb->a = old ^ value;
            gb->f = gb->a == 0 ? FLAG_Z : 0;
            break;
        case 6:
            gb->a = old | value;
            gb->f = gb->a == 0 ? FLAG_Z : 0;
            break;
        default: {
            uint8_t result8 = (uint8_t)(old - value);
            gb->f = FLAG_N | (result8 == 0 ? FLAG_Z : 0) |
                ((old & 0xF) < (value & 0xF) ? FLAG_H : 0) |
                (old < value ? FLAG_C : 0);
            break;
        }
    }
}

static void decimal_adjust(GameBoy *gb)
{
    uint8_t correction = 0;
    uint8_t carry = gb->f & FLAG_C;
    if (!(gb->f & FLAG_N)) {
        if ((gb->f & FLAG_H) || (gb->a & 0x0F) > 9) correction |= 0x06;
        if (carry || gb->a > 0x99) {
            correction |= 0x60;
            carry = FLAG_C;
        }
        gb->a = (uint8_t)(gb->a + correction);
    } else {
        if (gb->f & FLAG_H) correction |= 0x06;
        if (carry) correction |= 0x60;
        gb->a = (uint8_t)(gb->a - correction);
    }
    gb->f = (uint8_t)((gb->f & FLAG_N) | carry | (gb->a == 0 ? FLAG_Z : 0));
}

static unsigned execute_cb(GameBoy *gb, uint8_t opcode)
{
    unsigned group = opcode >> 6;
    unsigned operation = (opcode >> 3) & 7;
    unsigned reg = opcode & 7;
    uint8_t value = read_r(gb, reg);
    uint8_t carry;

    if (group == 0) {
        switch (operation) {
            case 0:
                carry = value >> 7;
                value = (uint8_t)((value << 1) | carry);
                break;
            case 1:
                carry = value & 1;
                value = (uint8_t)((value >> 1) | (carry << 7));
                break;
            case 2:
                carry = value >> 7;
                value = (uint8_t)((value << 1) | ((gb->f & FLAG_C) ? 1 : 0));
                break;
            case 3:
                carry = value & 1;
                value = (uint8_t)((value >> 1) | ((gb->f & FLAG_C) ? 0x80 : 0));
                break;
            case 4:
                carry = value >> 7;
                value <<= 1;
                break;
            case 5:
                carry = value & 1;
                value = (uint8_t)((value >> 1) | (value & 0x80));
                break;
            case 6:
                carry = 0;
                value = (uint8_t)((value << 4) | (value >> 4));
                break;
            default:
                carry = value & 1;
                value >>= 1;
                break;
        }
        gb->f = (value == 0 ? FLAG_Z : 0) | (carry ? FLAG_C : 0);
        write_r(gb, reg, value);
    } else if (group == 1) {
        gb->f = (uint8_t)((gb->f & FLAG_C) | FLAG_H |
            ((value & (1u << operation)) ? 0 : FLAG_Z));
    } else {
        if (group == 2) value &= (uint8_t)~(1u << operation);
        else value |= (uint8_t)(1u << operation);
        write_r(gb, reg, value);
    }
    return reg == 6 ? (group == 1 ? 12u : 16u) : 8u;
}

static void service_interrupt(GameBoy *gb, uint8_t pending)
{
    unsigned interrupt_bit = 0;
    while (!(pending & (1u << interrupt_bit))) interrupt_bit++;
    gb->ime = 0;
    gb->ime_delay = 0;
    gb->halted = 0;
    gb->stopped = 0;
    gb->memory[0xFF0F] &= (uint8_t)~(1u << interrupt_bit);
    push16(gb, gb->pc);
    gb->pc = (uint16_t)(0x40 + interrupt_bit * 8);
}

static unsigned execute_instruction(GameBoy *gb, uint8_t opcode, uint16_t address)
{
    if (opcode == 0x76) {
        uint8_t pending = gb->memory[0xFF0F] & gb->memory[0xFFFF] & 0x1F;
        if (!gb->ime && pending) gb->halt_bug = 1;
        else gb->halted = 1;
        return 4;
    }

    if (opcode >= 0x40 && opcode <= 0x7F) {
        unsigned destination = (opcode >> 3) & 7;
        unsigned source = opcode & 7;
        write_r(gb, destination, read_r(gb, source));
        return (destination == 6 || source == 6) ? 8 : 4;
    }
    if (opcode >= 0x80 && opcode <= 0xBF) {
        unsigned operation = (opcode >> 3) & 7;
        unsigned reg = opcode & 7;
        alu(gb, operation, read_r(gb, reg));
        return reg == 6 ? 8 : 4;
    }
    if ((opcode & 0xC7) == 0x04) {
        unsigned reg = (opcode >> 3) & 7;
        uint8_t before = read_r(gb, reg);
        uint8_t result = (uint8_t)(before + 1);
        gb->f = (uint8_t)((gb->f & FLAG_C) | (result == 0 ? FLAG_Z : 0) |
            ((before & 0x0F) == 0x0F ? FLAG_H : 0));
        write_r(gb, reg, result);
        return reg == 6 ? 12 : 4;
    }
    if ((opcode & 0xC7) == 0x05) {
        unsigned reg = (opcode >> 3) & 7;
        uint8_t before = read_r(gb, reg);
        uint8_t result = (uint8_t)(before - 1);
        gb->f = (uint8_t)((gb->f & FLAG_C) | FLAG_N | (result == 0 ? FLAG_Z : 0) |
            ((before & 0x0F) == 0 ? FLAG_H : 0));
        write_r(gb, reg, result);
        return reg == 6 ? 12 : 4;
    }
    if ((opcode & 0xC7) == 0x06) {
        unsigned reg = (opcode >> 3) & 7;
        write_r(gb, reg, fetch8(gb));
        return reg == 6 ? 12 : 8;
    }
    if ((opcode & 0xCF) == 0x01) {
        write_rr(gb, (opcode >> 4) & 3, fetch16(gb));
        return 12;
    }
    if ((opcode & 0xCF) == 0x03) {
        unsigned pair = (opcode >> 4) & 3;
        write_rr(gb, pair, (uint16_t)(read_rr(gb, pair) + 1));
        return 8;
    }
    if ((opcode & 0xCF) == 0x0B) {
        unsigned pair = (opcode >> 4) & 3;
        write_rr(gb, pair, (uint16_t)(read_rr(gb, pair) - 1));
        return 8;
    }
    if ((opcode & 0xCF) == 0x09) {
        uint32_t left = pair_hl(gb);
        uint32_t right = read_rr(gb, (opcode >> 4) & 3);
        uint32_t result = left + right;
        gb->f = (uint8_t)((gb->f & FLAG_Z) |
            (((left & 0x0FFF) + (right & 0x0FFF) > 0x0FFF) ? FLAG_H : 0) |
            (result > 0xFFFF ? FLAG_C : 0));
        set_hl(gb, (uint16_t)result);
        return 8;
    }
    if ((opcode & 0xCF) == 0x02) {
        unsigned pair = (opcode >> 4) & 3;
        uint16_t memory_address = pair == 0 ? pair_bc(gb) : pair == 1 ? pair_de(gb) : pair_hl(gb);
        gb_memory_write(gb, memory_address, gb->a);
        if (pair == 2) set_hl(gb, (uint16_t)(memory_address + 1));
        if (pair == 3) set_hl(gb, (uint16_t)(memory_address - 1));
        return 8;
    }
    if ((opcode & 0xCF) == 0x0A) {
        unsigned pair = (opcode >> 4) & 3;
        uint16_t memory_address = pair == 0 ? pair_bc(gb) : pair == 1 ? pair_de(gb) : pair_hl(gb);
        gb->a = gb_memory_read(gb, memory_address);
        if (pair == 2) set_hl(gb, (uint16_t)(memory_address + 1));
        if (pair == 3) set_hl(gb, (uint16_t)(memory_address - 1));
        return 8;
    }

    if ((opcode & 0xE7) == 0x20) {
        int8_t offset = (int8_t)fetch8(gb);
        if (condition(gb, (opcode >> 3) & 3)) {
            gb->pc = (uint16_t)(gb->pc + offset);
            return 12;
        }
        return 8;
    }
    if ((opcode & 0xE7) == 0xC0) {
        if (condition(gb, (opcode >> 3) & 3)) {
            gb->pc = pop16(gb);
            return 20;
        }
        return 8;
    }
    if ((opcode & 0xE7) == 0xC2) {
        uint16_t target = fetch16(gb);
        if (condition(gb, (opcode >> 3) & 3)) {
            gb->pc = target;
            return 16;
        }
        return 12;
    }
    if ((opcode & 0xE7) == 0xC4) {
        uint16_t target = fetch16(gb);
        if (condition(gb, (opcode >> 3) & 3)) {
            push16(gb, gb->pc);
            gb->pc = target;
            return 24;
        }
        return 12;
    }
    if ((opcode & 0xC7) == 0xC7) {
        push16(gb, gb->pc);
        gb->pc = opcode & 0x38;
        return 16;
    }
    if ((opcode & 0xCF) == 0xC1) {
        unsigned pair = (opcode >> 4) & 3;
        write_stack_pair(gb, pair, pop16(gb));
        return 12;
    }
    if ((opcode & 0xCF) == 0xC5) {
        push16(gb, read_stack_pair(gb, (opcode >> 4) & 3));
        return 16;
    }
    if ((opcode & 0xC7) == 0xC6) {
        alu(gb, (opcode >> 3) & 7, fetch8(gb));
        return 8;
    }

    switch (opcode) {
        case 0x00: return 4;
        case 0x07: {
            uint8_t carry = gb->a >> 7;
            gb->a = (uint8_t)((gb->a << 1) | carry);
            gb->f = carry ? FLAG_C : 0;
            return 4;
        }
        case 0x08: {
            uint16_t target = fetch16(gb);
            write16(gb, target, gb->sp);
            return 20;
        }
        case 0x0F: {
            uint8_t carry = gb->a & 1;
            gb->a = (uint8_t)((gb->a >> 1) | (carry << 7));
            gb->f = carry ? FLAG_C : 0;
            return 4;
        }
        case 0x10:
            fetch8(gb);
            gb->stopped = 1;
            return 4;
        case 0x17: {
            uint8_t old_carry = (gb->f & FLAG_C) ? 1 : 0;
            uint8_t carry = gb->a >> 7;
            gb->a = (uint8_t)((gb->a << 1) | old_carry);
            gb->f = carry ? FLAG_C : 0;
            return 4;
        }
        case 0x18: {
            int8_t offset = (int8_t)fetch8(gb);
            gb->pc = (uint16_t)(gb->pc + offset);
            return 12;
        }
        case 0x1F: {
            uint8_t old_carry = (gb->f & FLAG_C) ? 0x80 : 0;
            uint8_t carry = gb->a & 1;
            gb->a = (uint8_t)((gb->a >> 1) | old_carry);
            gb->f = carry ? FLAG_C : 0;
            return 4;
        }
        case 0x27:
            decimal_adjust(gb);
            return 4;
        case 0x2F:
            gb->a = (uint8_t)~gb->a;
            gb->f |= FLAG_N | FLAG_H;
            return 4;
        case 0x37:
            gb->f = (uint8_t)((gb->f & FLAG_Z) | FLAG_C);
            return 4;
        case 0x3F:
            gb->f = (uint8_t)((gb->f & FLAG_Z) | ((gb->f & FLAG_C) ? 0 : FLAG_C));
            return 4;
        case 0xC3:
            gb->pc = fetch16(gb);
            return 16;
        case 0xC9:
            gb->pc = pop16(gb);
            return 16;
        case 0xCB:
            return execute_cb(gb, fetch8(gb));
        case 0xCD: {
            uint16_t target = fetch16(gb);
            push16(gb, gb->pc);
            gb->pc = target;
            return 24;
        }
        case 0xD9:
            gb->pc = pop16(gb);
            gb->ime = 1;
            gb->ime_delay = 0;
            return 16;
        case 0xE0:
            gb_memory_write(gb, (uint16_t)(0xFF00 + fetch8(gb)), gb->a);
            return 12;
        case 0xE2:
            gb_memory_write(gb, (uint16_t)(0xFF00 + gb->c), gb->a);
            return 8;
        case 0xE8: {
            int8_t offset = (int8_t)fetch8(gb);
            uint16_t old = gb->sp;
            uint8_t unsigned_offset = (uint8_t)offset;
            gb->f = (((old & 0x0F) + (unsigned_offset & 0x0F) > 0x0F) ? FLAG_H : 0) |
                (((old & 0xFF) + unsigned_offset > 0xFF) ? FLAG_C : 0);
            gb->sp = (uint16_t)(old + offset);
            return 16;
        }
        case 0xE9:
            gb->pc = pair_hl(gb);
            return 4;
        case 0xEA:
            gb_memory_write(gb, fetch16(gb), gb->a);
            return 16;
        case 0xF0:
            gb->a = gb_memory_read(gb, (uint16_t)(0xFF00 + fetch8(gb)));
            return 12;
        case 0xF2:
            gb->a = gb_memory_read(gb, (uint16_t)(0xFF00 + gb->c));
            return 8;
        case 0xF3:
            gb->ime = 0;
            gb->ime_delay = 0;
            return 4;
        case 0xF8: {
            int8_t offset = (int8_t)fetch8(gb);
            uint16_t old = gb->sp;
            uint8_t unsigned_offset = (uint8_t)offset;
            gb->f = (((old & 0x0F) + (unsigned_offset & 0x0F) > 0x0F) ? FLAG_H : 0) |
                (((old & 0xFF) + unsigned_offset > 0xFF) ? FLAG_C : 0);
            set_hl(gb, (uint16_t)(old + offset));
            return 12;
        }
        case 0xF9:
            gb->sp = pair_hl(gb);
            return 8;
        case 0xFA:
            gb->a = gb_memory_read(gb, fetch16(gb));
            return 16;
        case 0xFB:
            gb->ime_delay = 2;
            return 4;
        default:
            gb->illegal_opcode = opcode;
            gb->halted = 1;
            fprintf(stderr, "Illegal opcode 0x%02X at PC=0x%04X\n", opcode, address);
            return 4;
    }
}

int gb_step(GameBoy *gb)
{
    if (!gb || !gb->rom) return 0;

    uint8_t pending = gb->memory[0xFF0F] & gb->memory[0xFFFF] & 0x1F;
    if (gb->halted || gb->stopped) {
        if (!pending) {
            gb_advance(gb, 4);
            return 4;
        }
        gb->halted = 0;
        gb->stopped = 0;
    }
    if (gb->ime && pending) {
        service_interrupt(gb, pending);
        gb_advance(gb, 20);
        return 20;
    }

    uint16_t address = gb->pc;
    uint8_t opcode = fetch8(gb);
    unsigned cycles = execute_instruction(gb, opcode, address);
    gb->instructions++;
    if (gb->ime_delay) {
        gb->ime_delay--;
        if (!gb->ime_delay) gb->ime = 1;
    }
    gb_advance(gb, cycles);
    return (int)cycles;
}
