/******************************************************************************
 *                                                                            *
 * Copyright 2018 Jan Henrik Weinstock                                        *
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

#include "vcml/models/opencores/ocfbc.h"

#define OCFBC_VBL(val) (1 << (((val) & CTLR_VBL8) >> 7))
#define OCFBC_BPP(val) ((((val) & CTLR_BPP32) >> 9) + 1)

namespace vcml { namespace opencores {

#ifdef HAVE_LIBVNC
    static debugging::vnc_fbmode find_mode(u32 resx, u32 resy, u32 bpp,
                                           bool truecolor, bool host_endian) {
        if (!truecolor)
            return debugging::fbmode_argb32(resx, resy);

        if (bpp == 1)
            return debugging::fbmode_gray8(resx, resy);

        if (host_endian) {
            switch (bpp) {
            case 4: return debugging::fbmode_argb32(resx, resy);
            case 3: return debugging::fbmode_rgb24(resx, resy);
            case 2: return debugging::fbmode_rgb16(resx, resy);
            default: VCML_ERROR("unknown pixel format %dbpp", bpp * 8);
            }
        } else {
            switch (bpp) {
            case 4: return debugging::fbmode_bgra32(resx, resy);
            case 3: return debugging::fbmode_bgr24(resx, resy);
            case 2: return debugging::fbmode_rgb16(resx, resy);
            default: VCML_ERROR("unknown pixel format %dbpp", bpp * 8);
            }
        }
    }
#endif

    SC_HAS_PROCESS(ocfbc);

    u32 ocfbc::read_STAT() {
        log_debug("read STAT register = 0x%08x", STAT.get());
        return STAT;
    }

    u32 ocfbc::write_STAT(u32 val) {
        // only the lower 8 bits are writable
        val = (STAT & 0xFFFFFF00) | (val & 0xFF);

        if ((STAT & STAT_SINT) && !(val & STAT_SINT)) {
            log_debug("clearing system error interrupt");
            IRQ = false;
        }

        if ((STAT & STAT_LUINT) && !(val & STAT_LUINT)) {
            log_debug("clearing FIFO underrun interrupt");
            IRQ = false;
        }

        if ((STAT & STAT_VINT) && !(val & STAT_VINT)) {
            log_debug("clearing vertical interrupt");
            IRQ = false;
        }

        if ((STAT & STAT_HINT) && !(val & STAT_HINT)) {
            log_debug("clearing horizontal interrupt");
            IRQ = false;
        }

        if ((STAT & STAT_VBSINT) && !(val & STAT_VBSINT)) {
            log_debug("clearing video bank switch interrupt");
            IRQ = false;
        }

        if ((STAT & STAT_CBSINT) && !(val & STAT_CBSINT)) {
            log_debug("clearing color bank switch interrupt");
            IRQ = false;
        }

        return val;
    }

    u32 ocfbc::write_CTRL(u32 val) {
        VCML_LOG_REG_BIT_CHANGE(CTLR_VEN,   CTLR, val);
        VCML_LOG_REG_BIT_CHANGE(CTLR_VIE,   CTLR, val);
        VCML_LOG_REG_BIT_CHANGE(CTLR_HIE,   CTLR, val);
        VCML_LOG_REG_BIT_CHANGE(CTLR_VBSIE, CTLR, val);
        VCML_LOG_REG_BIT_CHANGE(CTLR_CBSIE, CTLR, val);
        VCML_LOG_REG_BIT_CHANGE(CTLR_VBSWE, CTLR, val);
        VCML_LOG_REG_BIT_CHANGE(CTLR_CBSWE, CTLR, val);

        int old_vbl = OCFBC_VBL(CTLR);
        int new_vbl = OCFBC_VBL(val);
        if (new_vbl != old_vbl) {
            log_debug("video burst changed to %d (from %d)", new_vbl, old_vbl);
        }

        int old_bpp = OCFBC_BPP(CTLR) * 8;
        int new_bpp = OCFBC_BPP(val) * 8;
        if (new_bpp != old_bpp) {
            log_debug("color depth changed to %d (from %d)", new_bpp, old_bpp);
        }

        VCML_LOG_REG_BIT_CHANGE(CTLR_PC, CTLR, val);

        if ((val & CTLR_VEN) && !(CTLR & CTLR_VEN)) {
            log_debug("device enabled, video ram at 0x%08x", VBARA.get());

            m_resx = (HTIM & 0xffff) + 1;
            m_resy = (VTIM & 0xffff) + 1;
            m_bpp  = OCFBC_BPP(val);

#ifdef HAVE_LIBVNC
            u32 base = (STAT & STAT_AVMP) ? VBARB : VBARA;
            u32 size = m_resx * m_resy * m_bpp;
            u8* vram = NULL;

            tlm_dmi dmi;
            tlm_generic_payload tx;
            tx_setup(tx, TLM_READ_COMMAND, base, NULL, size);
            if (allow_dmi && OUT->get_direct_mem_ptr(tx, dmi)) {
                if (dmi.is_read_allowed() &&
                    dmi.get_start_address() <= base &&
                    dmi.get_end_address() >= base + size) {
                    vram = dmi.get_dmi_ptr() + base - dmi.get_start_address();
                }
            }

            bool truecolor = (val & CTLR_PC) != CTLR_PC;
            debugging::vnc_fbmode mode = find_mode(m_resx, m_resy, m_bpp,
                                                   truecolor, is_host_endian());
            shared_ptr<debugging::vncserver> vnc =
                    debugging::vncserver::lookup(vncport);

            // cannot use DMI with pseudocolor
            if ((vram == NULL) || (!truecolor) || (m_bpp == 2)) {
                log_debug("copying vnc framebuffer from vram");
                m_fb = vnc->setup_framebuffer(mode);
            } else {
                log_debug("mapping vnc framebuffer into vram");
                vnc->setup_framebuffer(mode, vram);
                m_fb = NULL;
            }
#endif
            m_enable.notify(SC_ZERO_TIME);
        }

        return val;
    }

    u32 ocfbc::write_HTIM(u32 val) {
        u32 sync = (val >> 24) & 0xff;
        u32 gdel = (val >> 16) & 0xff;
        u32 gate = (val & 0xffff) + 1;
        log_debug("write HTIM: hsync = %d, hgate delay = %d, hgate = %d",
                  sync, gdel, gate);
        return val;
    }

    u32 ocfbc::write_VTIM(u32 val) {
        u32 sync = (val >> 24) & 0xff;
        u32 gdel = (val >> 16) & 0xff;
        u32 gate = (val & 0xffff) + 1;
        log_debug("write VTIM: vsync = %d, vgate delay = %d, vgate = %d",
                  sync, gdel, gate);
        return val;
    }

    tlm_response_status ocfbc::read(const range& addr, void* ptr, int flags) {
        if (!addr.inside(m_palette_addr))
            return TLM_ADDRESS_ERROR_RESPONSE;

        u8* palette = (u8*)m_palette + addr.start - PALETTE_ADDR;
        memcpy(ptr, palette, addr.length());
        return TLM_OK_RESPONSE;
    }

    tlm_response_status ocfbc::write(const range& addr, const void* p, int f) {
        if (!addr.inside(m_palette_addr))
            return TLM_ADDRESS_ERROR_RESPONSE;

        u8* palette = (u8*)m_palette + addr.start - PALETTE_ADDR;
        memcpy(palette, p, addr.length());
        return TLM_OK_RESPONSE;
    }

    void ocfbc::render() {
        if (m_fb != NULL) { // need to copy data to framebuffer manually
            u8* fb = m_fb;
            for (u32 y = 0; y < m_resy; y++) {
                u32 burstsz = OCFBC_VBL(CTLR);
                u32 linesz = m_resx * m_bpp;
                u8 linebuf[linesz];
                tlm_response_status rs;

                // burst-read one horizontal line of pixels into buffer
                for (u32 x = 0; x < linesz; x += burstsz) {
                    u32 base = (STAT & STAT_AVMP) ? VBARB : VBARA;
                    u32 addr = base + y * linesz + x;
                    if (failed(rs = OUT.read(addr, linebuf + x, burstsz))) {
                        log_debug("failed to read vmem at 0x%08x: %s", addr,
                                  tlm_response_to_str(rs).c_str());
                    }
                }

                // Decode each line pixel by pixel and write to vnc framebuffer
                // Note that fb is host endian, linebuf is target endian. How-
                // ever, the format of fb has been adapted to match the target
                // one as best as possible (e.g. ARGB becomes BGRA on big
                // endian systems, etc.). So most of the time, a simple copy
                // from linebuf to fb suffices.
                for (u32 i = 0; i < linesz; i += m_bpp) {
                    switch (m_bpp) {
                    case 4:
                        *fb++ = linebuf[i + 0]; // target b
                        *fb++ = linebuf[i + 1]; // target g
                        *fb++ = linebuf[i + 2]; // target r
                        *fb++ = linebuf[i + 3]; // target a
                        break;

                    case 3:
                        *fb++ = linebuf[i + 0]; // target b
                        *fb++ = linebuf[i + 1]; // target g
                        *fb++ = linebuf[i + 2]; // target r
                        break;

                    case 2: {
                        // libvnc internally expects rgb16 as bgr15
                        u16 msb = is_big_endian() ? linebuf[i] : linebuf[i+1];
                        u16 lsb = is_big_endian() ? linebuf[i+1] : linebuf[i];
                        u16 rgb = (msb <<  8) | lsb;
                        u16 b5  = (rgb >>  0) & 0x1F;
                        u16 g6  = (rgb >>  5) & 0x3F;
                        u16 r5  = (rgb >> 11) & 0x1F;
                        u16 g5  = (g6  >>  1);
                        u16 bgr = (b5  << 10) | (g5 << 5) | r5;
                        *fb++   = (bgr >>  0) & 0xFF;
                        *fb++   = (bgr >>  8) & 0xFF;
                        break;
                    }

                    case 1:
                        if (CTLR & CTLR_PC) {
                            u32* cur_palette = m_palette;
                            if (STAT & STAT_ACMP)
                                cur_palette += 0x100;
                            u32 color = cur_palette[linebuf[i]];
                            if (!is_host_endian())
                                color = bswap(color); // target -> host
                            *fb++ = (color >>  0) & 0xFF; // b
                            *fb++ = (color >>  8) & 0xFF; // g
                            *fb++ = (color >> 16) & 0xFF; // r
                        } else {
                            *fb++ = linebuf[i];
                            *fb++ = linebuf[i];
                            *fb++ = linebuf[i];
                        }
                        fb++; // skip a
                        break;

                    default:
                        VCML_ERROR("unknown pixel format %u bpp", m_bpp * 8);
                    }
                }

                // Note that the HSYNC interrupt will only be triggered when
                // DMI is not used. Otherwise, this is never executed
                if (CTLR & CTLR_HIE) {
                    STAT |= STAT_HINT;
                    IRQ = true;
                }
            }
        }

        if (CTLR & CTLR_CBSWE) {
            STAT ^= STAT_ACMP;   // toggle ACMP bit
            CTLR &= ~CTLR_CBSWE; // clear CBSWE bit
            if (CTLR & CTLR_CBSIE)
                IRQ = true;
        }

        if (CTLR & CTLR_VBSWE) {
            STAT ^= STAT_AVMP;   // toggle AVMP bit
            CTLR &= ~CTLR_VBSWE; // clear VBSWE bit
            if (CTLR & CTLR_VBSIE)
                IRQ = true;
        }

        if (CTLR & CTLR_VIE) {
            STAT |= STAT_VINT;
            IRQ = true; // VSYNC interrupt
        }

#ifdef HAVE_LIBVNC
        debugging::vncserver::lookup(vncport)->render();
#endif
    }

    void ocfbc::update() {
        while (true) {
            while (!(CTLR & CTLR_VEN))
                wait(m_enable);

            sc_time t = sc_time_stamp();
            render();
            sc_time delta = sc_time_stamp() - t;
            sc_time frame(1.0 / clock, SC_SEC);
            if (delta < frame) {
                wait(frame - delta); // wait until next frame
            } else {
                log_debug("skipped %d frames", (int)(delta / frame));
                wait(frame - delta % frame);
            }
        }
    }

    ocfbc::ocfbc(const sc_module_name& nm):
        peripheral(nm),
        m_palette_addr(PALETTE_ADDR, PALETTE_ADDR + sizeof(m_palette)),
        m_palette(),
        m_fb(NULL),
        m_resx(0),
        m_resy(0),
        m_bpp(0),
        m_enable("enabled"),
        CTLR("CTRLR", 0x00, 0),
        STAT("STATR", 0x04, 0),
        HTIM("HTIMR", 0x08, 0),
        VTIM("VTIMR", 0x0c, 0),
        HVLEN("HVLEN", 0x10, 0),
        VBARA("VBARA", 0x14, 0),
        VBARB("VBARB", 0x18, 0),
        IRQ("IRQ"),
        IN("IN"),
        OUT("OUT"),
        clock("clock", 60), // 60Hz
        vncport("vncport", 0) {

        CTLR.allow_read_write();
        CTLR.write = &ocfbc::write_CTRL;

        STAT.allow_read_write();
        STAT.read = &ocfbc::read_STAT;
        STAT.write = &ocfbc::write_STAT;

        HTIM.allow_read_write();
        HTIM.write = &ocfbc::write_HTIM;

        VTIM.allow_read_write();
        VTIM.write = &ocfbc::write_VTIM;

        SC_THREAD(update);
    }

    ocfbc::~ocfbc() {
        /* nothing to do */
    }

}}
