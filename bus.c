#include "bus.h"
#include "cpu6507.h"
#include "tia.h"
#include "cartridge.h"

void bus_init(
    Bus *bus,
    TIA *tia,
    RIOT *riot,
    Cartridge *cart,
    TiaStepFn tia_step,
    TiaReadFn tia_read,
    TiaWriteFn tia_write
) {
    bus->tia = tia;
    bus->riot = riot;
    bus->cart = cart;
    bus->tia_step = tia_step;
    bus->tia_read = tia_read;
    bus->tia_write = tia_write;
    bus->data_bus = 0;
    bus->cycles_this_step = 0;

    for (int i = 0; i < 128; i++) {
        bus->ram[i] = 0;
    }
}

void bus_tick(Bus *bus, int cycles) {
    if (!bus || cycles <= 0) {
        return;
    }

    bus->cycles_this_step += cycles;
    if (bus->tia_step && bus->tia) {
        bus->tia_step(bus->tia, cycles * 3);
    }
    if (bus->riot) {
        riot_step(bus->riot, cycles);
    }
    cart_step_cycles(bus->cart, cycles);
}

uint8_t bus_read(Bus *bus, uint16_t address) {
    if (!bus) {
        return 0xFFu;
    }

    uint16_t addr = (uint16_t)(address & 0x1FFFu);
    Cartridge *cart = bus->cart;
    uint8_t value;

    /* FE-cartridges gebruiken dit bereik voor RAM. */
    if (cart != NULL && addr >= 0x0100u && addr <= 0x01FFu &&
        cart->type == CART_TYPE_FE) {
        bus_tick(bus, 1);
        value = bus->ram[addr & 0x007Fu];
    } else if (addr & 0x1000u) {
        bus_tick(bus, 1);
        value = cart ? cart_read(cart, addr, bus->data_bus) : 0xFFu;
    } else if ((addr & 0x0280u) == 0x0280u && (addr & 0x001Fu) == 0x04u) {
        /* INTIM is read before the cycle tick for DPC timing. */
        value = bus->riot ? riot_read(bus->riot, addr) : 0xFFu;
        bus_tick(bus, 1);
    } else {
        bus_tick(bus, 1);
        if ((addr & 0x0280u) == 0x0280u) {
            value = bus->riot ? riot_read(bus->riot, addr) : 0xFFu;
        } else if ((addr & 0x0280u) == 0x0080u) {
            value = bus->ram[addr & 0x007Fu];
        } else {
            value = (bus->tia_read && bus->tia)
                ? bus->tia_read(bus->tia, addr)
                : 0xFFu;
        }
    }
    bus->data_bus = value;
    return value;
}

void bus_write(Bus *bus, uint16_t address, uint8_t value) {
    bus_tick(bus, 1);
    bus->data_bus = value;

    uint16_t addr = (uint16_t)(address & 0x1FFFu);
    uint8_t v = value;
    Cartridge *cart = bus->cart;

    if (cart != NULL && addr >= 0x0100u && addr <= 0x01FFu && cart->type == CART_TYPE_FE) {
        bus->ram[addr & 0x007Fu] = v;
        return;
    }

    if (addr & 0x1000u) {
        if (cart) {
            cart_write(cart, addr, v);
        }
    } else if ((addr & 0x0080u) == 0) {
        if (bus->tia_write && bus->tia) {
            bus->tia_write(bus->tia, (uint16_t)(addr & 0x3Fu), v);
        }
    } else if ((addr & 0x0200u) == 0) {
        bus->ram[addr & 0x7Fu] = v;
    } else {
        if (bus->riot) {
            riot_write(bus->riot, addr, v);
        }
    }
}

int bus_get_cycles_this_step(const Bus *bus) {
    return bus->cycles_this_step;
}

void bus_reset_cycles_this_step(Bus *bus) {
    bus->cycles_this_step = 0;
}

void bus_step_frame(Bus *bus, CPU6507 *cpu) {
    if (!bus || !bus->tia || !cpu) {
        return;
    }

    bus->tia->frame_ready = false;

    while (!bus->tia->frame_ready) {
        if (bus->tia->wsync) {
            int clk = bus->tia->clock;
            int remaining = 228 - clk;
            if (remaining > 0) {
                int stall_cycles = (remaining % 3 == 0) ? remaining / 3 : 1;

                if (bus->tia_step) {
                    bus->tia_step(bus->tia, remaining);
                }
                if (bus->riot) {
                    riot_step(bus->riot, stall_cycles);
                }
                cart_step_cycles(bus->cart, stall_cycles);
                bus->cycles_this_step += stall_cycles;
            }
        } else {
            cpu_step(cpu);
        }
    }
}