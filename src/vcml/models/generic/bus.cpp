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

#include "vcml/models/generic/bus.h"

namespace vcml { namespace generic {

    bool bus::cmd_show(const vector<string>& args, ostream& os) {
        os << "Memory map of " << name();
        #define HEX(x, w) std::setfill('0') << std::setw(w) << \
                          std::hex << (x) << std::dec

        vector<bus_mapping> mappings(m_mappings);
        std::sort(mappings.begin(), mappings.end(),
                [](const bus_mapping& a, const bus_mapping& b) -> bool {
            return a.addr.start < b.addr.start;
        });

        int i = 0;
        for (auto bm : mappings) {
            os << std::endl << i++ << ": " << HEX(bm.addr.start, 8) << ".."
               << HEX(bm.addr.end, 8) << " -> ";

            if (bm.offset > 0) {
                os << HEX(bm.offset, 8) << " .. "
                   << HEX(bm.offset + bm.addr.length() - 1, 8) << " ";
            }

            os << (bm.peer.empty() ? OUT[bm.port].name() : bm.peer);
        }

        if (m_default.port != -1) {
            os << std::endl << "default route -> ";
            if (m_default.peer.empty())
                os << OUT[m_default.port].name();
            else
                os << m_default.peer;
        }

        #undef HEX
        return true;
    }

    typedef tlm_utils::simple_initiator_socket_tagged<bus, 64> isock;
    typedef tlm_utils::simple_target_socket_tagged<bus, 64> tsock;

    tlm_target_socket<64>* bus::create_target_socket(unsigned int idx) {
        sc_simcontext* simc = sc_get_curr_simcontext();
        simc->hierarchy_push(this);

        stringstream ss;
        ss << "IN" << idx;

        tsock* sock = new tsock(ss.str().c_str());

        sock->register_b_transport(this,
                &bus::cb_b_transport, idx);
        sock->register_transport_dbg(this,
                &bus::cb_transport_dbg, idx);
        sock->register_get_direct_mem_ptr(this,
                &bus::cb_get_direct_mem_ptr, idx);

        simc->hierarchy_pop();
        return sock;
    }

    tlm_initiator_socket<64>*
    bus::create_initiator_socket(unsigned int idx) {
        sc_simcontext* simc = sc_get_curr_simcontext();
        simc->hierarchy_push(this);

        stringstream ss;
        ss << "OUT" << idx;

        isock* sock = new isock(ss.str().c_str());

        sock->register_invalidate_direct_mem_ptr(this,
                &bus::cb_invalidate_direct_mem_ptr, idx);

        simc->hierarchy_pop();
        return sock;
    }

    void bus::cb_b_transport(int port, tlm_generic_payload& tx, sc_time& dt) {
        if (!trace_errors)
            trace_in(port, tx);

        b_transport(port, tx, dt);

        if (!trace_errors || failed(tx.get_response_status()))
            trace_out(port, tx);
    }

    unsigned int bus::cb_transport_dbg(int port, tlm_generic_payload& tx) {
        return transport_dbg(port, tx);
    }

    bool bus::cb_get_direct_mem_ptr(int port, tlm_generic_payload& tx,
                                           tlm_dmi& dmi) {
        return get_direct_mem_ptr(port, tx, dmi);
    }

    void bus::cb_invalidate_direct_mem_ptr(int port, sc_dt::uint64 s,
                                           sc_dt::uint64 e) {
        invalidate_direct_mem_ptr(port, s, e);
    }

    void bus::b_transport(int port, tlm_generic_payload& tx, sc_time& dt) {
        const bus_mapping& dest = lookup(tx);
        if (dest.port == -1) {
            tx.set_response_status(TLM_ADDRESS_ERROR_RESPONSE);
            return;
        }

        u64 addr = tx.get_address();
        tx.set_address(addr - dest.addr.start + dest.offset);
        OUT[dest.port]->b_transport(tx, dt);
        tx.set_address(addr);
    }

    unsigned int bus::transport_dbg(int port, tlm_generic_payload& tx) {
        const bus_mapping& dest = lookup(tx);
        if (dest.port == -1) {
            tx.set_response_status(TLM_ADDRESS_ERROR_RESPONSE);
            return 0;
        }

        u64 addr = tx.get_address();
        tx.set_address(addr - dest.addr.start + dest.offset);
        unsigned int response = OUT[dest.port]->transport_dbg(tx);
        tx.set_address(addr);
        return response;
    }

    bool bus::get_direct_mem_ptr(int port, tlm_generic_payload& tx,
                            tlm_dmi& dmi) {
        const bus_mapping& dest = lookup(tx);
        if (dest.port == -1) {
            tx.set_response_status(TLM_ADDRESS_ERROR_RESPONSE);
            return false;
        }

        u64 addr = tx.get_address();
        tx.set_address(addr - dest.addr.start + dest.offset);
        bool use_dmi = OUT[dest.port]->get_direct_mem_ptr(tx, dmi);
        tx.set_address(addr);

        if (use_dmi) {
            u64 s = dmi.get_start_address() + dest.addr.start - dest.offset;
            u64 e = dmi.get_end_address() + dest.addr.start - dest.offset;

            // check if target gave more DMI space than it has address space
            if (s < dest.addr.start) {
                log_warning("truncating dmi start from 0x%016x to 0x%016x", s,
                            dest.addr.start);
                s = dest.addr.start;
            }

            if (e > dest.addr.end) {
                log_warning("truncating dmi end from 0x%016x to 0x%016x", e,
                            dest.addr.end);
                e = dest.addr.end;
            }

            dmi.set_start_address(s);
            dmi.set_end_address(e);
        }

        return use_dmi;
    }

    void bus::invalidate_direct_mem_ptr(int port, sc_dt::uint64 start,
                                               sc_dt::uint64 end) {
        for (unsigned int i = 0; i < m_mappings.size(); i++) {
            const bus_mapping& m = m_mappings[i];
            if (m.port == port) {
                u64 s = m.addr.start + start - m.offset;
                u64 e = m.addr.start + end - m.offset;

                bus_ports<tlm_target_socket<64> >::iterator it;
                for (it = IN.begin(); it != IN.end(); it++)
                    (*it->second)->invalidate_direct_mem_ptr(s, e);
            }
        }
    }

    const bus_mapping& bus::lookup(const range& addr) const {
        for (unsigned int i = 0; i < m_mappings.size(); i++) {
            const bus_mapping& m = m_mappings[i];
            if (m.addr.includes(addr))
                return m;
        }

        return m_default;
    }

    void bus::map(unsigned int port, const range& addr, u64 offset,
                  const string& peer) {
        const bus_mapping& other = lookup(addr);
        if (other.port != -1) {
            VCML_ERROR("Cannot map %d:0x%016lx..0x%016lx to '%s', because it "\
                       "overlaps with %d:0x%016lx..0x%016lx mapped to '%s'",
                       port, addr.start, addr.end, peer.c_str(), other.port,
                       other.addr.start, other.addr.end, other.peer.c_str());
        }

        bus_mapping mapping = {
            .port = (int)port,
            .addr = addr,
            .offset = offset,
            .peer = peer,
        };

        m_mappings.push_back(mapping);
    }

    void bus::map(unsigned int port, u64 start, u64 end, u64 offset,
                  const string& peer) {
        map(port, range(start, end), offset, peer);
    }

    unsigned int bus::bind(tlm_initiator_socket<64>& socket) {
        unsigned int port = IN.next_idx();
        socket.bind(IN[port]);
        return port;
    }

    unsigned int bus::bind(tlm_target_socket<64>& socket, const range& addr,
                           u64 offset) {
        unsigned int port = OUT.next_idx();
        map(port, addr, offset, socket.name());
        OUT[port].bind(socket);
        return port;
    }

    unsigned int bus::bind(tlm_target_socket<64>& socket, u64 start,
                                  u64 end, u64 offset) {
        return bind(socket, range(start, end), offset);
    }

    void bus::map_default(unsigned int port, const string& peer) {
        if (m_default.port != -1)
            VCML_ERROR("default bus route already mapped");

        m_default.port = port;
        m_default.addr = range(0ull, ~0ull);
        m_default.offset = 0;
        m_default.peer = peer;
    }

    unsigned int bus::bind_default(tlm_target_socket<64>& socket) {
        unsigned int port = OUT.next_idx();
        map_default(port, socket.name());
        OUT[port].bind(socket);
        return port;
    }

    bus::bus(const sc_module_name& nm):
        component(nm),
        m_mappings(),
        m_default(),
        IN(this),
        OUT(this) {

        m_default.port = -1;
        m_default.addr = range(0ull, ~0ull);
        m_default.offset = 0;
        m_default.peer = "";

        register_command("show", 0, this, &bus::cmd_show,
                         "shows the memory map of this bus");
    }

    bus::~bus() {
        /* nothing to do */
    }

    template <>
    tlm_target_socket<64>*
    bus::create_socket<tlm_target_socket<64> >(unsigned int idx) {
        return create_target_socket(idx);
    }

    template <>
    tlm_initiator_socket<64>*
    bus::create_socket<tlm_initiator_socket<64> >(unsigned int idx) {
        return create_initiator_socket(idx);
    }

}}
