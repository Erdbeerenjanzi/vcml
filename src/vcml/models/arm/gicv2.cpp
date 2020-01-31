/******************************************************************************
 *                                                                            *
 * Copyright 2020 Jan Henrik Weinstock, Lukas Juenger                         *
 *                                                                            *
 * Licensed under the Apache License, Version 2.0 (the "License");            *
 * you may not use this file except in compliance with the License.           *
 * You may obtain a copy of the License at                                    *
 *                                                                            *
 *     http://www.apache.org/licenses/LICENSE-2.0                             *
 *                                                                            *
 * Unless required by applicable law or agreed to in writing, software        *
 * distributed under the License is distributed on an "AS IS" BASIS,          *
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.   *
 * See the License for the specific language governing permissions and        *
 * limitations under the License.                                             *
 *                                                                            *
 ******************************************************************************/

#include <algorithm>

#include "vcml/models/arm/gicv2.h"

namespace vcml { namespace arm {

    static unsigned int ctz32(u32 value) {
        if (value & 0x00000001)
            return 0; // special case for odd value

        unsigned int count = 1;

        // binary search for the trailing one bit
        if (!(value & 0x0000FFFF)) {
            count += 16;
            value >>= 16;
        }

        if (!(value & 0x000000FF)) {
            count += 8;
            value >>= 8;
        }

        if (!(value & 0x0000000F)) {
            count += 4;
            value >>= 4;
        }

        if (!(value & 0x00000003)) {
            count += 2;
            value >>= 2;
        }

        return count - (value & 0x00000001);
    }

    static inline bool is_software_interrupt(unsigned int irq) {
        return irq < VCML_ARM_GICv2_NSGI;
    }

    gicv2::irq_state::irq_state():
        enabled(0),
        pending(0),
        active(0),
        level(0),
        signaled(0),
        model(N_N),
        trigger(EDGE) {
        // nothing to do
    }

    gicv2::lr::lr():
        pending(false),
        active(false),
        hw(0),
        prio(0),
        virtual_id(0),
        physical_id(0),
        cpu_id(0) {
        // nothing to do
    }

    u32 gicv2::distif::int_pending_mask(int cpu) {
        u32 value = 0;
        unsigned int mask = 1 << cpu;
        for (unsigned int irq = 0; irq < VCML_ARM_GICv2_PRIV; irq++)
            if (m_parent->test_pending(irq, mask))
                    value |= (1 << irq);
        return value;
    }

    u32 gicv2::distif::spi_pending_mask(int cpu) {
        u32 value = 0;
        unsigned int offset = VCML_ARM_GICv2_PRIV + cpu * 32;
        for (unsigned int irq = 0; irq < 32; irq++)
            if (m_parent->test_pending(offset + irq, gicv2::ALL_CPU))
                value |= (1 << irq);
        return value;
    }

    u16 gicv2::distif::ppi_enabled_mask(int cpu) {
        u16 value = 0;
        unsigned int mask = 1 << cpu;
        for (unsigned int irq = 0; irq < VCML_ARM_GICv2_NPPI; irq++)
            if (m_parent->is_irq_enabled(irq + VCML_ARM_GICv2_NSGI, mask))
                value |= (1 << irq);
        return value;
    }

    u32 gicv2::distif::write_CTLR(u32 val) {
        if ((val & CTLR_ENABLE) && !(CTLR & CTLR_ENABLE))
            log_debug("(CTLR) irq forwarding enabled");
        if (!(val & CTLR_ENABLE) && (CTLR & CTLR_ENABLE))
            log_debug("(CTLR) irq forwarding disabled");
        CTLR = val & CTLR_ENABLE;
        m_parent->update();
        return CTLR;
    }

    u32 gicv2::distif::read_ICTR() {
        return 0xFF;
    }

    u32 gicv2::distif::read_ISER() {
        int cpu = current_cpu();
        if (cpu < 0) {
            log_warning("(ISER) invalid cpu %d, assuming 0", cpu);
            cpu = 0;
        }

        u32 mask = ppi_enabled_mask(cpu);
        return (mask << 16) | 0xFFFF; // SGIs are always enabled
    }

    u32 gicv2::distif::write_ISER(u32 val) {
        int cpu = current_cpu();
        if (cpu < 0) {
            log_warning("(ISER) invalid cpu %d, assuming 0", cpu);
            cpu = 0;
        }

        unsigned int irq = VCML_ARM_GICv2_NSGI;
        unsigned int mask = 1 << cpu;

        for (; irq < VCML_ARM_GICv2_PRIV; irq++) {
            if (val & (1 << irq)) {
                m_parent->enable_irq(irq, mask);
                if (m_parent->get_irq_level(irq, mask) &&
                    m_parent->get_irq_trigger(irq) == gicv2::LEVEL) {
                    m_parent->set_irq_pending(irq, true, mask);
                }
            }
        }

        m_parent->update();
        return ISER;
    }

    u32 gicv2::distif::read_SSER(unsigned int idx) {
        u32 value = 0;
        unsigned int irq = VCML_ARM_GICv2_PRIV + idx * 32;
        for (unsigned int i = 0; i < 32; i++) {
            if (m_parent->is_irq_enabled(irq + i, gicv2::ALL_CPU))
                value |= (1 << i);
        }

        return value;
    }

    u32 gicv2::distif::write_SSER(u32 val, unsigned int idx) {
        unsigned int irq = VCML_ARM_GICv2_PRIV + idx * 32;
        for (unsigned int i = 0; i < 32; i++) {
            if (val & (1 << i)) {
                m_parent->enable_irq(irq + i, gicv2::ALL_CPU);
                if ((m_parent->get_irq_level(irq + i, gicv2::ALL_CPU)) &&
                    (m_parent->get_irq_trigger(irq + i) == gicv2::LEVEL)) {
                    m_parent->set_irq_pending(irq + i, true, gicv2::ALL_CPU);
                }
            }
        }

        m_parent->update();
        return SSER;
    }

    u32 gicv2::distif::read_ICER() {
        int cpu = current_cpu();
        if (cpu < 0) {
            log_warning("(ICER) invalid cpu %d, assuming 0", cpu);
            cpu = 0;
        }

        u32 mask = ppi_enabled_mask(cpu);
        return (mask << 16) | 0xFFFF; // SGIs are always enabled
    }

    u32 gicv2::distif::write_ICER(u32 val) {
        int cpu = current_cpu();
        if (cpu < 0) {
            log_warning("(ICER) invalid cpu %d, assuming 0", cpu);
            cpu = 0;
        }

        unsigned int irq = VCML_ARM_GICv2_NSGI;
        unsigned int mask = 1 << cpu;

        for (; irq < VCML_ARM_GICv2_PRIV; irq++) {
            if (val & (1 << irq))
                m_parent->disable_irq(irq, mask);
        }

        m_parent->update();
        return ICER;
    }

    u32 gicv2::distif::read_SCER(unsigned int idx) {
        u32 value = 0;
        unsigned int irq = VCML_ARM_GICv2_PRIV + idx * 32;
        for (unsigned int i = 0; i < 32; i++) {
            if (m_parent->is_irq_enabled(irq + i, gicv2::ALL_CPU))
                value |= (1 << i);
        }

        return value;
    }

    u32 gicv2::distif::write_SCER(u32 val, unsigned int idx) {
        unsigned int irq = VCML_ARM_GICv2_PRIV + idx * 32;
        for (unsigned int i = 0; i < 32; i++) {
            if (val & (1 << i))
                m_parent->disable_irq(irq + i, gicv2::ALL_CPU);
        }

        m_parent->update();
        return SCER;
    }

    u32 gicv2::distif::read_ISPR() {
        int cpu = current_cpu();
        if (cpu < 0) {
            log_warning("(ISPR) invalid cpu %d, assuming 0", cpu);
            cpu = 0;
        }

        return int_pending_mask(cpu);
    }

    u32 gicv2::distif::write_ISPR(u32 value) {
        int cpu = current_cpu();
        if (cpu < 0) {
            log_warning("(ISPR) invalid cpu %d, assuming 0", cpu);
            cpu = 0;
        }

        unsigned int irq = VCML_ARM_GICv2_NSGI;
        unsigned int mask = 1 << cpu;

        for (; irq < VCML_ARM_GICv2_PRIV; irq++) {
            if (value & (1 << irq))
                m_parent->set_irq_pending(irq, true, mask);
        }

        m_parent->update();
        return ISPR;
    }

    u32 gicv2::distif::read_SSPR(unsigned int idx) {
        return spi_pending_mask(idx);
    }

    u32 gicv2::distif::write_SSPR(u32 value, unsigned int idx) {
        unsigned int irq = VCML_ARM_GICv2_PRIV + idx * 32;
        for (unsigned int i = 0; i < 32; i++) {
            if (value & (1 << i))
                m_parent->set_irq_pending(irq + i, true, SPIT[i]);
        }

        m_parent->update();
        return SSPR;
    }

    u32 gicv2::distif::read_ICPR() {
        int cpu = current_cpu();
        if (cpu < 0) {
            log_warning("(ICPR) invalid cpu %d, assuming 0", cpu);
            cpu = 0;
        }

        return int_pending_mask(cpu);
    }

    u32 gicv2::distif::write_ICPR(u32 value) {
        int cpu = current_cpu();
        if (cpu < 0) {
            log_warning("(ICPR) invalid cpu %d, assuming 0", cpu);
            cpu = 0;
        }

        unsigned int irq = VCML_ARM_GICv2_NSGI;
        unsigned int mask = 1 << cpu;

        for (; irq < VCML_ARM_GICv2_PRIV; irq++) {
            if (value & (1 << irq))
                m_parent->set_irq_pending(irq, false, mask);
        }

        m_parent->update();
        return SCPR;
    }

    u32 gicv2::distif::read_SCPR(unsigned int idx) {
        return spi_pending_mask(idx);
    }

    u32 gicv2::distif::write_SCPR(u32 val, unsigned int idx) {
        unsigned int irq = VCML_ARM_GICv2_PRIV + idx * 32;
        for (unsigned int i = 0; i < 32; i++) {
            if (val & (1 << i))
                m_parent->set_irq_pending(irq + i, false, gicv2::ALL_CPU);
        }

        m_parent->update();
        return SCPR;
    }

    u32 gicv2::distif::read_IACR() {
        int cpu = current_cpu();
        if (cpu < 0) {
            log_warning("(IACR) invalid cpu %d, assuming 0", cpu);
            cpu = 0;
        }

        u32 value = 0;
        unsigned int mask = 1 << cpu;
        for (unsigned int l = 0; l < VCML_ARM_GICv2_PRIV; l++) {
            if (m_parent->is_irq_active(l, mask))
                value |= (1 << l);
        }

        return value;
    }

    u32 gicv2::distif::read_SACR(unsigned int idx) {
        u32 value = 0;
        unsigned int irq = VCML_ARM_GICv2_PRIV + idx * 32;
        for (unsigned int i = 0; i < 32; i++) {
            if (m_parent->is_irq_active(irq + i, gicv2::ALL_CPU))
                value |= (1 << i);
        }

        return value;
    }

    u32 gicv2::distif::write_ICAR(u32 val) {
        int cpu = current_cpu();
        if (cpu < 0) {
            log_warning("(ICAR) invalid cpu %d, assuming 0", cpu);
            cpu = 0;
        }

        unsigned int mask = 1 << cpu;
        for (unsigned int irq = 0; irq < 32; irq++) {
            if (val & (1 << irq))
                m_parent->set_irq_active(irq, false, mask);
        }

        return 0;
    }

    u32 gicv2::distif::write_SCAR(u32 val, unsigned int idx) {
        unsigned int irq = VCML_ARM_GICv2_PRIV + idx * 32;
        for (unsigned int i = 0; i < 32; i++) {
            if (val & (1 << i))
                m_parent->set_irq_active(irq + i, false, gicv2::ALL_CPU);
        }

        return 0;
    }

    u32 gicv2::distif::read_INTT(unsigned int idx) {
        int cpu = current_cpu();
        if (cpu < 0) {
            log_warning("(INTT) invalid cpu %d, assuming 0", cpu);
            cpu = 0;
        }

        // local cpu is always target for its own SGIs and PPIs
        return 0x01010101 << cpu;
    }

    u32 gicv2::distif::write_CPPI(u32 value) {
        CPPI = value & 0xAAAAAAAA; // odd bits are reserved, zero them out

        unsigned int irq = VCML_ARM_GICv2_NSGI;
        for (unsigned int i = 0; i < VCML_ARM_GICv2_NPPI; i++) {
            if (value & (2 << (i * 2))) {
                m_parent->set_irq_trigger(irq + i, gicv2::EDGE);
                log_debug("irq %u configured to be edge sensitive", irq + i);
            } else {
                m_parent->set_irq_trigger(irq + i, gicv2::LEVEL);
                log_debug("irq %u configured to be level sensitive", irq + i);
            }
        }

        m_parent->update();
        return CPPI;
    }

    u32 gicv2::distif::write_CSPI(u32 value, unsigned int idx) {
        CSPI[idx] = value & 0xAAAAAAAA; // odd bits are reserved, zero them out

        unsigned int irq = VCML_ARM_GICv2_PRIV + idx * 16;
        for (unsigned int i = 0; i < 16; i++) {
            if (value & (2 << (i * 2))) {
                m_parent->set_irq_trigger(irq + i, gicv2::EDGE);
                log_debug("irq %u configured to be edge sensitive", irq + i);
            } else {
                m_parent->set_irq_trigger(irq + i, gicv2::LEVEL);
                log_debug("irq %u configured to be level sensitive", irq + i);
            }
        }

        m_parent->update();
        return CSPI;
    }

    u32 gicv2::distif::write_SCTL(u32 value) {
        int cpu = current_cpu();
        if (cpu < 0) {
            log_warning("(SCTL) invalid cpu %d, assuming 0", cpu);
            cpu = 0;
        }

        unsigned int src_cpu = 1 << cpu;
        unsigned int sgi_num = (value >>  0) & 0x0F;
        unsigned int targets = (value >> 16) & 0xFF;
        unsigned int filters = (value >> 24) & 0x03;

        switch (filters) {
        case 0x0:
            // forward interrupt to CPUs specified in CPU target list
            break;
        case 0x1:
            // forward interrupt to all CPUs except writing CPU
            targets = gicv2::ALL_CPU ^ src_cpu;
            break;
        case 0x2:
            // forward interrupt only to writing CPU
            targets = 1 << cpu;
            break;
        default:
            log_warning("bad SGI target filter");
            break;
        }

        m_parent->set_irq_pending(sgi_num, true, targets);
        for (int target = 0; target < VCML_ARM_GICv2_NCPU; target++) {
            if (targets & (1 << target))
                set_sgi_pending(1 << cpu, sgi_num, target, true);
        }
        m_parent->set_irq_signaled(sgi_num, false, targets);
        m_parent->update();
        return SCTL;
    }


    u8 gicv2::distif::write_SGIS(u8 value, unsigned int idx) {
        int cpu = current_cpu();
        if (cpu < 0) {
            log_warning("(SGIS) invalid cpu %d, assuming 0", cpu);
            cpu = 0;
        }

        unsigned int mask = 1 << cpu;
        unsigned int irq = idx;

        set_sgi_pending(value, irq, cpu, true);
        m_parent->set_irq_pending(irq, true, mask);
        m_parent->set_irq_signaled(irq, false, mask);
        m_parent->update();

        return SGIS;
    }

    u8 gicv2::distif::write_SGIC(u8 value, unsigned int idx) {
        int cpu = current_cpu();
        if (cpu < 0) {
            log_warning("(SGIC) invalid cpu %d, assuming 0", cpu);
            cpu = 0;
        }

        unsigned int mask = 1 << cpu;
        unsigned int irq = idx;

        set_sgi_pending(value, irq, cpu, false);
        if (SGIC.bank(cpu, idx) == 0) // clear SGI if no sources remain
            m_parent->set_irq_pending(irq, false, mask);
        m_parent->update();
        return SGIC;
    }

    gicv2::distif::distif(const sc_module_name& nm):
        peripheral(nm),
        m_parent(dynamic_cast<gicv2*>(get_parent_object())),
        CTLR("CTLR", 0x000, 0x00000000),
        ICTR("ICTR", 0x004, 0x00000000),
        IIDR("IIDR", 0x008, 0x00000000),
        ISER("ISER", 0x100, 0x0000FFFF),
        SSER("SSER", 0x104, 0x00000000),
        ICER("ICER", 0x180, 0x0000FFFF),
        SCER("SCER", 0x184, 0x00000000),
        ISPR("ISPR", 0x200, 0x00000000),
        SSPR("SSPR", 0x204, 0x00000000),
        ICPR("ICPR", 0x280, 0x00000000),
        SCPR("SCPR", 0x284, 0x00000000),
        IACR("IACR", 0x300),
        SACR("SACR", 0x304),
        ICAR("ICAR", 0x380, 0x00000000),
        SCAR("SCAR", 0x384, 0x00000000),
        SGIP("SGIP", 0x400, 0x00),
        PPIP("PPIP", 0x410, 0x00),
        SPIP("SPIP", 0x420, 0x00),
        INTT("INTT", 0x800),
        SPIT("SPIT", 0x820),
        CSGI("CSGI", 0xC00, 0xAAAAAAAA),
        CPPI("CPPI", 0xC04, 0xAAAAAAAA),
        CSPI("CSPI", 0xC08),
        SCTL("SCTL", 0xF00),
        SGIS("SGIS", 0xF20),
        SGIC("SGIC", 0xF10),
        CIDR("CIDR", 0xFF0),
        IN("IN") {
        VCML_ERROR_ON(!m_parent, "gicv2 parent module not found");

        CTLR.allow_read_write();
        CTLR.write = &distif::write_CTLR;

        ICTR.allow_read();
        ICTR.read = &distif::read_ICTR;

        IIDR.allow_read_write();

        ISER.set_banked();
        ISER.allow_read_write();
        ISER.read = &distif::read_ISER;
        ISER.write = &distif::write_ISER;

        SSER.allow_read_write();
        SSER.tagged_read = &distif::read_SSER;
        SSER.tagged_write = &distif::write_SSER;

        ICER.set_banked();
        ICER.allow_read_write();
        ICER.read = &distif::read_ICER;
        ICER.write = &distif::write_ICER;

        SCER.allow_read_write();
        SCER.tagged_read = &distif::read_SCER;
        SCER.tagged_write = &distif::write_SCER;

        ISPR.set_banked();
        ISPR.allow_read_write();
        ISPR.read = &distif::read_ISPR;
        ISPR.write = &distif::write_ISPR;

        SSPR.allow_read_write();
        SSPR.tagged_read = &distif::read_SSPR;
        SSPR.tagged_write = &distif::write_SSPR;

        ICPR.set_banked();
        ICPR.allow_read_write();
        ICPR.read = &distif::read_ICPR;
        ICPR.write = &distif::write_ICPR;

        SCPR.allow_read_write();
        SCPR.tagged_read = &distif::read_SCPR;
        SCPR.tagged_write = &distif::write_SCPR;

        IACR.set_banked();
        IACR.allow_read();
        IACR.read = &distif::read_IACR;

        SACR.allow_read();
        SACR.tagged_read = &distif::read_SACR;

        ICAR.set_banked();
        ICAR.allow_read_write();
        ICAR.write = &distif::write_ICAR;

        SCAR.allow_read_write();
        SCAR.tagged_write = &distif::write_SCAR;

        SGIP.set_banked();
        SGIP.allow_read_write();

        PPIP.set_banked();
        PPIP.allow_read_write();

        SPIP.allow_read_write();

        INTT.set_banked();
        INTT.allow_read_write();
        INTT.tagged_read = &distif::read_INTT;

        SPIT.allow_read_write();

        CSGI.allow_read();

        CPPI.allow_read_write();
        CPPI.write = &distif::write_CPPI;

        CSPI.allow_read_write();
        CSPI.tagged_write = &distif::write_CSPI;

        SCTL.set_banked();
        SCTL.allow_write();
        SCTL.write = &distif::write_SCTL;

        SGIS.set_banked();
        SGIS.allow_read_write();
        SGIS.tagged_write = &distif::write_SGIS;

        SGIC.set_banked();
        SGIC.allow_read_write();
        SGIC.tagged_write = &distif::write_SGIC;

        CIDR.allow_read();

        reset();
    }

    gicv2::distif::~distif() {
        // nothing to do
    }

    void gicv2::distif::reset() {
        for (unsigned int i = 0; i < CIDR.num(); i++)
            CIDR[i] = (VCML_ARM_GICv2_CID >> (i * 8)) & 0xFF;
    }

    void gicv2::distif::setup(unsigned int num_cpu, unsigned int num_irq) {
        ICTR = ((num_cpu & 0x7) << 5) | (0xF & ((num_irq / 32) - 1));
    }

    void gicv2::distif::set_sgi_pending(u8 value, unsigned int sgi,
                                        unsigned int cpu, bool set) {
        if (set) {
            SGIS.bank(cpu, sgi) |= value;
            SGIC.bank(cpu, sgi) |= value;
        } else {
            SGIS.bank(cpu, sgi) &= ~value;
            SGIC.bank(cpu, sgi) &= ~value;
        }
    }

    void gicv2::distif::end_of_elaboration() {
        // SGIs are enabled per default and cannot be disabled
        for (unsigned int irq = 0; irq < VCML_ARM_GICv2_NSGI; irq++)
            m_parent->enable_irq(irq, gicv2::ALL_CPU);
    }

    void gicv2::cpuif::set_current_irq(unsigned int cpu, unsigned int irq) {
        m_curr_irq[cpu] = irq;
        PRIO.bank(cpu) = (irq == VCML_ARM_GICv2_SPURIOUS_IRQ) ? 0x100 :
                                  (m_parent->get_irq_priority(cpu, irq));
        m_parent->update();
    }

    u32 gicv2::cpuif::write_CTLR(u32 val) {
        if ((val & CTLR_ENABLE) && !(CTLR & CTLR_ENABLE))
            log_debug("(CTLR) enabling cpu %d", current_cpu());
        if (!(val & CTLR_ENABLE) && (CTLR & CTLR_ENABLE))
            log_debug("(CTLR) disabling cpu %d", current_cpu());
        return val & CTLR_ENABLE;
    }

    u32 gicv2::cpuif::write_IPMR(u32 val) {
        return val & 0x000000FF; // read only first 8 bits
    }

    u32 gicv2::cpuif::write_BIPR(u32 val) {
        ABPR = val & 0x7; // read only first 3 bits, store copy in ABPR
        return ABPR;
    }

    u32 gicv2::cpuif::write_EOIR(u32 val) {
        int cpu = current_cpu();
        if (cpu < 0) {
            log_warning("(EOIR) invalid cpu %d, assuming 0", cpu);
            cpu = 0;
        }

        if (m_curr_irq[cpu] == VCML_ARM_GICv2_SPURIOUS_IRQ)
            return 0; // no active IRQ

        unsigned int irq = val & 0x3FF; // interrupt id stored in bits [9..0]
        if (irq >= m_parent->get_irq_num()) {
            log_warning("(EOI) invalid irq %d ignored", irq);
            return 0;
        }

        if (irq == m_curr_irq[cpu]) {
            log_debug("(EOI) cpu %d eois irq %d", cpu, irq);
            set_current_irq(cpu, m_prev_irq[irq][cpu]);
            m_parent->set_irq_active(irq, false, 1 << cpu);
            return 0;
        }

        // handle IRQ that is not currently running
        int iter = m_curr_irq[cpu];
        while (m_prev_irq[iter][cpu] != VCML_ARM_GICv2_SPURIOUS_IRQ) {
            if (m_prev_irq[iter][cpu] == irq) {
                m_prev_irq[iter][cpu] = m_prev_irq[irq][cpu];
                break;
            }

            iter = m_prev_irq[iter][cpu];
        }

        return 0;
    }

    u32 gicv2::cpuif::read_IACK() {
        int cpu = current_cpu();
        if (cpu < 0) {
            log_warning("(IACK) invalid cpu %d, assuming 0", cpu);
            cpu = 0;
        }

        int irq = PEND.bank(cpu);
        int cpu_mask = (m_parent->get_irq_model(irq) == gicv2::N_1) ?
                       (gicv2::ALL_CPU) : (1 << cpu);

        log_debug("(IACK) cpu %d acknowledges irq %d", cpu, irq);

        // check if CPU is acknowledging a not pending interrupt
        if (irq == VCML_ARM_GICv2_SPURIOUS_IRQ ||
            m_parent->get_irq_priority(cpu, irq) >= PRIO.bank(cpu)) {
            return VCML_ARM_GICv2_SPURIOUS_IRQ;
        }

        if (is_software_interrupt(irq)) {
            u8 pending = m_parent->DISTIF.SGIS.bank(cpu, irq);
            unsigned int src_cpu = ctz32(pending);
            m_parent->DISTIF.set_sgi_pending(1 << src_cpu, irq, cpu, false);

            // check if SGI is not pending from any CPUs
            if (m_parent->DISTIF.SGIS.bank(cpu, irq) == 0)
                m_parent->set_irq_pending(irq, false, cpu_mask);
            IACK = (src_cpu & 0x7) << 10 | irq;
        } else {
            // clear pending state for interrupt 'irq'
            m_parent->set_irq_pending(irq, false, cpu_mask);
            IACK = irq;
        }

        m_prev_irq[irq][cpu] = m_curr_irq[cpu];
        set_current_irq(cpu, irq); // set the acknowledged IRQ to running
        m_parent->set_irq_active(irq, true, cpu_mask);
        m_parent->set_irq_signaled(irq, true, cpu_mask);
        return IACK;
    }

    gicv2::cpuif::cpuif(const sc_module_name& nm):
        peripheral(nm),
        m_parent(dynamic_cast<gicv2*>(get_parent_object())),
        CTLR("CTLR", 0x00, 0x0),
        IPMR("IPMR", 0x04, 0x0),
        BIPR("BIPR", 0x08, 0x0),
        IACK("IACK", 0x0C, 0x0),
        EOIR("EOIR", 0x10, 0x0),
        PRIO("PRIO", 0x14, VCML_ARM_GICv2_IDLE_PRIO),
        PEND("PEND", 0x18, VCML_ARM_GICv2_SPURIOUS_IRQ),
        ABPR("ABPR", 0x1C, 0x0),
        ACPR("ACPR", 0xD0, 0x00000000),
        IIDR("IIDR", 0xFC, VCML_ARM_GICv2_IIDR),
        CIDR("CIDR", 0xFF0),
        DIR("DIR", 0x1000),
        IN("IN") {
        VCML_ERROR_ON(!m_parent, "gicv2 parent module not found");

        CTLR.set_banked();
        CTLR.allow_read_write();
        CTLR.write = &cpuif::write_CTLR;

        IPMR.set_banked();
        IPMR.allow_read_write();
        IPMR.write = &cpuif::write_IPMR;

        BIPR.set_banked();
        BIPR.allow_read_write();
        BIPR.write = &cpuif::write_BIPR;

        IACK.set_banked();
        IACK.allow_read();
        IACK.read = &cpuif::read_IACK;

        EOIR.set_banked();
        EOIR.allow_write();
        EOIR.write = &cpuif::write_EOIR;

        PRIO.set_banked();
        PRIO.allow_read();

        PEND.set_banked();
        PEND.allow_read();

        ABPR.set_banked();
        ABPR.allow_read_write();

        ACPR.allow_read_write();

        IIDR.allow_read();
        CIDR.allow_read();

        DIR.set_banked();
        DIR.allow_read_write();

        reset();
    }

    gicv2::cpuif::~cpuif() {
        // nothing to do
    }

    void gicv2::cpuif::reset() {
        PRIO = PRIO.get_default();
        PEND = PEND.get_default();

        for (unsigned int i = 0; i < CIDR.num(); i++)
            CIDR[i] = (VCML_ARM_GICv2_CID >> (i * 8)) & 0xFF;

        for (unsigned int irq = 0; irq < VCML_ARM_GICv2_NIRQ; irq++)
            for (unsigned int cpu = 0; cpu < VCML_ARM_GICv2_NCPU; cpu++)
                m_prev_irq[irq][cpu] = VCML_ARM_GICv2_SPURIOUS_IRQ;

        for (unsigned int cpu = 0; cpu < VCML_ARM_GICv2_NCPU; cpu++)
            m_curr_irq[cpu] = VCML_ARM_GICv2_SPURIOUS_IRQ;
    }

    u32 gicv2::vifctrl::write_HCR(u32 val) {
        u8 core_id = current_cpu();
        HCR.bank(core_id) = val;
        m_parent->update(true);
        return val;
    };

    u32 gicv2::vifctrl::read_VTR() {
        return 0x90000000 | (VCML_ARM_GICv2_NLR-1);
    };

    u32 gicv2::vifctrl::write_LR(u32 val, unsigned int idx) {
        u8 core_id = current_cpu();
        u8 state = (val >> 28) & 0b11;
        u8 hw = (val >> 31) & 0b1;
        if (hw == 0) {
            u8 eoi = (val >> 19) & 0b1;
            if (eoi == 1)
                log_error("Maintenance IRQ not implemented");
            u8 cpu_id = (val >> 10) & 0b111;
            set_lr_cpuid(idx, core_id, cpu_id);
            set_lr_hw(idx, core_id, false);
            set_lr_physid(idx, core_id, 0);
        } else {
            set_lr_cpuid(idx, core_id, 0);
            set_lr_hw(idx, core_id, true);
            u16 physid = (val >> 10) & 0x1ff;
            set_lr_physid(idx, core_id, physid);
        }

        if (state & 1)
            set_lr_pending(idx, core_id, true);
        if (state & 2)
            set_lr_active(idx, core_id, true);
        if (state == 0) {
            set_lr_pending(idx, core_id, false);
            set_lr_active(idx, core_id, false);
        }
        u32 prio = (val >> 23) & 0x1f;
        set_lr_prio(idx, core_id, prio);
        u32 irq = val & 0x1ff;
        set_lr_vid(idx, core_id, irq);
        LR[idx] = val;
        m_parent->update(true);
        return val;
    };

    u32 gicv2::vifctrl::read_LR(unsigned int idx) {
        u8 core_id = current_cpu();
        // Update pending and active bit
        if(is_lr_pending(idx, core_id)) {
            LR[idx] = LR[idx] | VCML_ARM_GICv2_LR_PENDING_MASK;
        } else {
            LR[idx] = LR[idx] & ~VCML_ARM_GICv2_LR_PENDING_MASK;
        }
        if(is_lr_active(idx, core_id)) {
            LR[idx] = LR[idx] | VCML_ARM_GICv2_LR_ACTIVE_MASK;
        } else {
            LR[idx] = LR[idx] & ~VCML_ARM_GICv2_LR_ACTIVE_MASK;
        }
        return LR[idx];
    }

    u32 gicv2::vifctrl::write_VMCR(u32 val) {
        int core_id = current_cpu();
        u8 prio_mask = (val >> 27) & 0x1f;
        u8 bpr = (val >> 21) & 0x03;
        u32 ctlr = val & 0x1ff;
        m_parent->VCPUIF.PMR.bank(core_id) = prio_mask << 3;
        m_parent->VCPUIF.BPR.bank(core_id) = bpr;
        m_parent->VCPUIF.CTLR.bank(core_id) = ctlr;
        return val;
    }

    u32 gicv2::vifctrl::read_VMCR() {
        int core_id = current_cpu();
        u8 prio_mask = (m_parent->VCPUIF.PMR.bank(core_id) >> 3) & 0x1f;
        u8 bpr = m_parent->VCPUIF.BPR.bank(core_id) & 0x03;
        u32 ctlr = m_parent->VCPUIF.CTLR.bank(core_id) & 0x1ff;
        return (prio_mask << 27 | bpr << 21 | ctlr);
    }

    u32 gicv2::vifctrl::write_APR(u32 val) {
       int core_id = current_cpu();
       u8 prio = VCML_ARM_GICv2_IDLE_PRIO;
       if (val != 0)
           prio = ((u32) (pow(2,log2(val)))) << (VCML_ARM_GICv2_VIRT_MIN_BPR + 1);
       m_parent->VCPUIF.RPR.bank(core_id) = prio;
       return val;
    }

    u8 gicv2::vifctrl::get_irq_priority(unsigned int core_id, unsigned int irq) {
        for(unsigned int i = 0; i < VCML_ARM_GICv2_NLR; i++) {
            if (m_lr_state[core_id][i].virtual_id == irq &&
                    (m_lr_state[core_id][i].active ||
                    m_lr_state[core_id][i].pending)) {
                return m_lr_state[core_id][i].prio;
            }
        }
        log_error("Failed getting LR prio for irq %d on cpu %d", irq, core_id);
        return 0;
    }

    u8 gicv2::vifctrl::get_lr(unsigned int irq, u8 core_id) {
        for(unsigned int i = 0; i < VCML_ARM_GICv2_NLR; i++) {
            if (m_lr_state[core_id][i].virtual_id == irq  &&
                (m_lr_state[core_id][i].active ||
                m_lr_state[core_id][i].pending)) {
                return i;
            }
        }
        log_error("Failed getting LR for irq %d on cpu%d", irq, core_id);
        return 0;
    }

    gicv2::vifctrl::vifctrl(const sc_module_name& nm):
        peripheral(nm),
        m_parent(dynamic_cast<gicv2*>(get_parent_object())),
        m_lr_state(),
        HCR("HCR", 0x0),
        VTR("VTR", 0x04, 0x0),
        VMCR("VMCR", 0x08),
        APR("APR", 0xF0, 0x0),
        LR("LR", 0x100, 0x0),
        IN("IN") {

        HCR.set_banked();
        HCR.allow_read_write();
        HCR.write = &vifctrl::write_HCR;

        VTR.allow_read();
        VTR.read = &vifctrl::read_VTR;

        LR.set_banked();
        LR.allow_read_write();
        LR.tagged_write = &vifctrl::write_LR;
        LR.tagged_read = &vifctrl::read_LR;

        VMCR.allow_read_write();
        VMCR.read = &vifctrl::read_VMCR;
        VMCR.write = &vifctrl::write_VMCR;

        APR.set_banked();
        APR.allow_read_write();
        APR.write = &vifctrl::write_APR;
    }

    gicv2::vifctrl::~vifctrl() {
        // nothing to do
    }

    u32 gicv2::vcpuif::write_CTLR(u32 val) {
        if (val > 1)
            log_error("Using unimplemented features");
        return val;
    }

    u32 gicv2::vcpuif::write_BPR(u32 val) {
        return std::max(val & 0x07, (u32) VCML_ARM_GICv2_VIRT_MIN_BPR);
    }

    u32 gicv2::vcpuif::read_IAR() {
        u8 core_id = current_cpu();

        u32 irq = HPPIR.bank(core_id);
        if (irq == VCML_ARM_GICv2_SPURIOUS_IRQ ||
                m_vifctrl->get_irq_priority(core_id, irq) >= RPR.bank(core_id))
            return VCML_ARM_GICv2_SPURIOUS_IRQ;

        u32 prio = m_vifctrl->get_irq_priority(core_id, irq) << 3;
        u32 mask = ~0ul << ((BPR.bank(core_id) & 0x07) + 1);
        RPR.bank(core_id) = prio & mask;
        u32 preemption_level = prio >> (VCML_ARM_GICv2_VIRT_MIN_BPR + 1);
        u32 bitno = preemption_level % 32;
        m_parent->VIFCTRL.APR.bank(core_id) |= (1 << bitno);

        u8 lr = m_vifctrl->get_lr(irq, core_id);
        m_vifctrl->set_lr_active(lr, core_id, true);

        m_vifctrl->set_lr_pending(lr, core_id, false);

        log_debug("(vIACK) cpu %d acknowledges virq %d", core_id, irq);
        m_parent->update(true);
        u8 cpu_id = m_vifctrl->get_lr_cpuid(lr, core_id);
        return ( (cpu_id & 0x111) << 10 | irq);
    }

    u32 gicv2::vcpuif::write_EOIR(u32 val) {
        u8 core_id = current_cpu();
        u32 irq = val & 0x1FF;

        if (irq >= m_parent->get_irq_num()) {
            log_warning("(EOI) invalid irq %d ignored", irq);
            return 0;
        }
        log_debug("(vEOIR) cpu%d eois virq %d", core_id, irq);

        u8 lr = m_vifctrl->get_lr(irq, core_id);
        // drop priority and update APR
        m_vifctrl->APR.bank(core_id) &= m_parent->VIFCTRL.APR.bank(core_id) - 1;
        u32 apr = m_parent->VIFCTRL.APR.bank(core_id);
        if (apr)
            RPR.bank(core_id) = ((u32) (pow(2,log2(apr)))) << (VCML_ARM_GICv2_VIRT_MIN_BPR + 1);
        else
            RPR.bank(core_id) = VCML_ARM_GICv2_IDLE_PRIO;
        // deactivate interrupt
        m_vifctrl->set_lr_active(lr, core_id, false);
        if (m_vifctrl->is_lr_hw(lr, core_id)) {
            u16 physid = m_vifctrl->get_lr_physid(lr, core_id);
            if (!(physid < VCML_ARM_GICv2_NSGI || physid > VCML_ARM_GICv2_NIRQ)) {
                m_parent->set_irq_active(physid, false, 1 << core_id);
            } else {
                log_error("Unexpected physical id %d for cpu %d in LR %d", physid, core_id, lr);
            }
        }
        m_parent->update(true);
        return val;
    }

    gicv2::vcpuif::vcpuif(const sc_module_name& nm, vifctrl *vifctrl):
        peripheral(nm),
        m_parent(dynamic_cast<gicv2*>(get_parent_object())),
        m_vifctrl(vifctrl),
        CTLR("CTLR", 0x00, 0x0),
        PMR("PMR", 0x04, 0x0),
        BPR("BPR", 0x08, 2),
        IAR("IAR", 0x0C, 0x0),
        EOIR("EOIR", 0x10, 0x0),
        RPR("RPR", 0x14, VCML_ARM_GICv2_IDLE_PRIO),
        HPPIR("HPPIR", 0x18, VCML_ARM_GICv2_SPURIOUS_IRQ),
        APR("APR", 0xD0, 0x00000000),
        IIDR("IIDR", 0xFC, VCML_ARM_GICv2_IIDR),
        IN("IN") {

        CTLR.set_banked();
        CTLR.allow_read_write();
        CTLR.write = &vcpuif::write_CTLR;

        PMR.set_banked();
        PMR.allow_read_write();

        BPR.set_banked();
        BPR.allow_read_write();
        BPR.write = &vcpuif::write_BPR;

        IAR.set_banked();
        IAR.allow_read();
        IAR.read = &vcpuif::read_IAR;

        EOIR.set_banked();
        EOIR.allow_write();
        EOIR.write = &vcpuif::write_EOIR;

        RPR.set_banked();

        HPPIR.set_banked();
        HPPIR.allow_read_write();

        APR.set_banked();
        APR.allow_read_write();

        IIDR.allow_read();

        reset();
    }

    gicv2::vcpuif::~vcpuif() {
        // nothing to do
    }

    void gicv2::vcpuif::reset() {
        RPR = RPR.get_default();
        HPPIR = HPPIR.get_default();
    }

    gicv2::gicv2(const sc_module_name& nm):
        peripheral(nm),
        DISTIF("distif"),
        CPUIF("cpuif"),
        VIFCTRL("vifctrl"),
        VCPUIF("vcpuif", &VIFCTRL),
        PPI_IN("PPI_IN"),
        SPI_IN("SPI_IN"),
        FIQ_OUT("FIQ_OUT"),
        IRQ_OUT("IRQ_OUT"),
        VFIQ_OUT("VFIQ_OUT"),
        VIRQ_OUT("VIRQ_OUT"),
        m_irq_num(VCML_ARM_GICv2_PRIV),
        m_cpu_num(0),
        m_irq_state() {
        // nothing to do
    }

    gicv2::~gicv2() {
        // nothing to do
    }

    void gicv2::update(bool virt) {
        for (unsigned int cpu = 0; cpu < m_cpu_num; cpu++) {
            unsigned int irq;
            unsigned int mask = 1 << cpu;
            unsigned int best_irq = VCML_ARM_GICv2_SPURIOUS_IRQ;
            unsigned int best_prio = VCML_ARM_GICv2_IDLE_PRIO;
            if (!virt)
                CPUIF.PEND.bank(cpu) = VCML_ARM_GICv2_SPURIOUS_IRQ;
            else
                VCPUIF.HPPIR.bank(cpu) = VCML_ARM_GICv2_SPURIOUS_IRQ;

            if (!virt && (DISTIF.CTLR == 0u || CPUIF.CTLR.bank(cpu) == 0u)) {
                log_debug("Disabling IRQ[%d]", cpu);
                IRQ_OUT[cpu].write(false);
                continue; // TODO check if continue or return
            }

            if (virt && (VIFCTRL.HCR.bank(cpu) == 0u)) {
                log_debug("Disabling VIRQ[%d]", cpu);
                VIRQ_OUT[cpu].write(false);
                continue;
            }

            if(!virt) {
                // check SGIs
                for (irq = 0; irq < VCML_ARM_GICv2_NSGI; irq++) {
                    if (is_irq_enabled(irq, mask) && test_pending(irq, mask) && !is_irq_active(irq, mask)) {
                        if (DISTIF.SGIP.bank(cpu, irq) < best_prio) {
                            best_prio = DISTIF.SGIP.bank(cpu, irq);
                            best_irq = irq;
                        }
                    }
                }

                // check PPIs
                for (irq = VCML_ARM_GICv2_NSGI; irq < VCML_ARM_GICv2_PRIV; irq++) {
                    if (is_irq_enabled(irq, mask) && test_pending(irq, mask) && !is_irq_active(irq, mask)) {
                        int idx = irq - VCML_ARM_GICv2_NSGI;
                        if (DISTIF.PPIP.bank(cpu, idx) < best_prio) {
                            best_prio = DISTIF.PPIP.bank(cpu, idx);
                            best_irq = irq;
                        }
                    }
                }

                // check SPIs
                for (irq = VCML_ARM_GICv2_PRIV; irq < m_irq_num; irq++) {
                    int idx = irq - VCML_ARM_GICv2_PRIV;
                    if (is_irq_enabled(irq, mask) && test_pending(irq, mask) &&
                        (DISTIF.SPIT[idx] & mask) && !is_irq_active(irq, mask)) {
                        if (DISTIF.SPIP[idx] < best_prio) {
                            best_prio = DISTIF.SPIP[idx];
                            best_irq = irq;
                        }
                    }
                }
            }
            else {
                for (int lr_idx = 0; lr_idx < VCML_ARM_GICv2_NLR; lr_idx++) {
                    if (VIFCTRL.is_lr_pending(lr_idx, cpu)) {
                        u8 prio = (VIFCTRL.LR.bank(cpu, lr_idx) & (0x1F<<23)) >> 23;
                        if (prio < best_prio) {
                            best_prio = prio;
                            best_irq = (VIFCTRL.LR.bank(cpu, lr_idx) & 0x1FF);
                        }
                    }
                }
            }
            // signal IRQ to processor if priority is higher
            bool level = false;
            if (!virt) {
                if (best_prio < CPUIF.IPMR.bank(cpu)) {
                    log_debug("Setting irq %u pending on cpuif %u", best_irq, cpu);
                    CPUIF.PEND.bank(cpu) = best_irq;
                    if (best_prio < CPUIF.PRIO.bank(cpu))
                        level = true;
                }
            }
            else {
                if (best_prio < VCPUIF.PMR.bank(cpu)) {
                    VCPUIF.HPPIR.bank(cpu) = best_irq;
                    if (best_prio < VCPUIF.RPR.bank(cpu))
                        level = true;
                 }
            }

            if(!virt) {
                if (IRQ_OUT[cpu].read() != level)
                    log_debug("%sing %s[%u] for irq %u", level ? "sett" : "clear", "IRQ", cpu, best_irq);
                IRQ_OUT[cpu].write(level); // FIRQ or NIRQ?
            }
            else {
                if(VIRQ_OUT[cpu].read() != level)
                    log_debug("%sing %s[%u] for irq %u", level ? "sett" : "clear", "VIRQ", cpu, best_irq);
                VIRQ_OUT[cpu].write(level);
            }
        }
    }

    u8 gicv2::get_irq_priority(unsigned int cpu, unsigned int irq)
    {
        if (irq < VCML_ARM_GICv2_NSGI)
            return DISTIF.SGIP.bank(cpu, irq);
        else if (irq < VCML_ARM_GICv2_PRIV)
            return DISTIF.PPIP.bank(cpu, irq - VCML_ARM_GICv2_NSGI);
        else if (irq < VCML_ARM_GICv2_NIRQ)
            return DISTIF.SPIP[irq - VCML_ARM_GICv2_PRIV];

        log_error("tried to get IRQ priority of invalid IRQ ID (%d)", irq);
        return 0;
    }

    void gicv2::ppi_handler(unsigned int cpu, unsigned int irq) {
        unsigned int idx = irq - VCML_ARM_GICv2_NSGI + cpu * VCML_ARM_GICv2_NPPI;
        unsigned int mask = 1 << cpu;

        bool irq_level = PPI_IN[idx].read();
        set_irq_level(irq, irq_level, mask);
        set_irq_signaled(irq, false, gicv2::ALL_CPU);
        if (get_irq_trigger(irq) == EDGE && irq_level)
            set_irq_pending(irq, true, mask);

        update();
    }

    void gicv2::spi_handler(unsigned int irq) {
        unsigned int idx = irq - VCML_ARM_GICv2_PRIV;
        unsigned int target_cpu = DISTIF.SPIT[idx];

        bool irq_level = SPI_IN[idx].read();
        set_irq_level(irq, irq_level, gicv2::ALL_CPU);
        set_irq_signaled(irq, false, gicv2::ALL_CPU);
        if (get_irq_trigger(irq) == EDGE && irq_level)
            set_irq_pending(irq, true, target_cpu);

        update();
    }

    void gicv2::end_of_elaboration() {
        m_cpu_num = 0;
        m_irq_num = VCML_ARM_GICv2_PRIV;

        // determine the number of processors from the connected IRQs
        for (auto cpu : IRQ_OUT)
            m_cpu_num = max(m_cpu_num, cpu.first + 1);

        // register handlers for each private peripheral interrupt
        for (auto ppi : PPI_IN) {
            unsigned int cpu = ppi.first / VCML_ARM_GICv2_NPPI;
            unsigned int irq = ppi.first % VCML_ARM_GICv2_NPPI
                               + VCML_ARM_GICv2_NSGI;
            stringstream ss;
            ss << "cpu_" << cpu << "_ppi_" << irq << "_handler";

            sc_spawn_options opts;
            opts.spawn_method();
            opts.set_sensitivity(ppi.second);
            opts.dont_initialize();

            sc_spawn(sc_bind(&gicv2::ppi_handler, this, cpu, irq),
                     ss.str().c_str(), &opts);
        }

        // register handlers for each SPI
        for (auto spi : SPI_IN) {
            unsigned int irq = spi.first + VCML_ARM_GICv2_PRIV;

            if (irq >= VCML_ARM_GICv2_NIRQ)
                VCML_ERROR("too many interrupts (%u)", irq);

            if (irq >= m_irq_num)
                m_irq_num = irq + 1;

            stringstream ss;
            ss << "spi_" << irq << "_handler";


            sc_spawn_options opts;
            opts.spawn_method();
            opts.set_sensitivity(spi.second);
            opts.dont_initialize();

            sc_spawn(sc_bind(&gicv2::spi_handler, this, irq),
                     ss.str().c_str(), &opts);
        }

        log_debug("found %u cpus with %u irqs in total", m_cpu_num, m_irq_num);
        DISTIF.setup(m_cpu_num, m_irq_num);
    }

}}
