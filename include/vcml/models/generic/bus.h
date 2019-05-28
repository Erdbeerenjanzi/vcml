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

#ifndef VCML_GENERIC_BUS
#define VCML_GENERIC_BUS

#include "vcml/common/includes.h"
#include "vcml/common/types.h"
#include "vcml/common/utils.h"
#include "vcml/common/report.h"

#include "vcml/range.h"
#include "vcml/command.h"
#include "vcml/component.h"

namespace vcml { namespace generic {

    class bus;

    struct bus_mapping {
        int port;
        range addr;
        u64 offset;
        string peer;
    };

    template <typename T>
    class bus_ports
    {
    private:
        unsigned int m_next;
        bus*  m_parent;
        std::map<unsigned int, T*> m_sockets;

        // disabled
        bus_ports();
        bus_ports(const bus_ports&);
        bus_ports& operator= (const bus_ports&);

    public:
        bus_ports(bus* parent);
        virtual ~bus_ports();

        bool exists(unsigned int idx) const;

        unsigned int next_idx() const;
        T& next();

        inline T& operator[] (unsigned int idx);
        inline const T& operator[] (unsigned int idx) const;

        typedef typename std::map<unsigned int, T*>::iterator iterator;

        iterator begin() { return m_sockets.begin(); }
        iterator end()   { return m_sockets.end(); }
    };

    class bus: public component
    {
    private:
        bool cmd_show(const vector<string>& args, ostream& os);

        vector<bus_mapping> m_mappings;
        bus_mapping         m_default;

        tlm_target_socket<64>* create_target_socket(unsigned int idx);
        tlm_initiator_socket<64>* create_initiator_socket(unsigned int idx);

        void cb_b_transport(int port, tlm_generic_payload& tx, sc_time& dt);
        unsigned int cb_transport_dbg(int port, tlm_generic_payload& tx);
        bool cb_get_direct_mem_ptr(int port, tlm_generic_payload& tx,
                                   tlm_dmi& dmi);
        void cb_invalidate_direct_mem_ptr(int port, sc_dt::uint64 s,
                                          sc_dt::uint64 e);

        using component::b_transport;
        using component::transport_dbg;
        using component::get_direct_mem_ptr;
        using component::invalidate_direct_mem_ptr;

    protected:
        void b_transport(int port, tlm_generic_payload& tx, sc_time& dt);
        unsigned int transport_dbg(int port, tlm_generic_payload& tx);
        bool get_direct_mem_ptr(int port, tlm_generic_payload& tx,
                                tlm_dmi& dmi);
        void invalidate_direct_mem_ptr(int port, sc_dt::uint64 start,
                                       sc_dt::uint64 end);

    public:
        bus_ports<tlm_target_socket<64> > IN;
        bus_ports<tlm_initiator_socket<64> > OUT;

        const bus_mapping& lookup(const range& addr) const;

        void map(unsigned int port, const range& addr, u64 offset = 0,
                 const string& peer = "");
        void map(unsigned int port, u64 start, u64 end, u64 offset = 0,
                 const string& peer = "");

        unsigned int bind(tlm_initiator_socket<64>& socket);
        unsigned int bind(tlm_target_socket<64>& socket, const range& addr,
                          u64 offset = 0);
        unsigned int bind(tlm_target_socket<64>& socket, u64 start, u64 end,
                          u64 offset = 0);

        void map_default(unsigned int port, const string& peer = "");
        unsigned int bind_default(tlm_target_socket<64>& socket);

        bus(const sc_core::sc_module_name& nm);
        virtual ~bus();

        VCML_KIND(bus);

        template <typename T>
        T* create_socket(unsigned int idx);

        void trace_in(int port, const tlm_generic_payload& tx) const;
        void trace_out(int port, const tlm_generic_payload& tx) const;
    };

    template <typename T>
    bus_ports<T>::bus_ports(bus* parent):
        m_next(0),
        m_parent(parent),
        m_sockets() {
        VCML_ERROR_ON(parent == NULL, "bus_ports parent must not be NULL");
    }

    template <typename T>
    bus_ports<T>::~bus_ports() {
        typename std::map<unsigned int, T*>::iterator it;
        for (it = m_sockets.begin(); it != m_sockets.end(); it++)
            delete it->second;
    }

    template <typename T>
    bool bus_ports<T>::exists(unsigned int idx) const {
        return m_sockets.find(idx) != m_sockets.end();
    }

    template <typename T>
    unsigned int bus_ports<T>::next_idx() const {
        return m_next;
    }

    template <typename T>
    T& bus_ports<T>::next() {
        return operator [] (next_idx());
    }

    template <typename T>
    inline T& bus_ports<T>::operator[] (unsigned int idx) {
        if (!exists(idx)) {
            m_sockets[idx] = m_parent->create_socket<T>(idx);
            m_next = idx + 1;
        }

        return *m_sockets[idx];
    }

    template <typename T>
    inline const T& bus_ports<T>::operator[] (unsigned int idx) const {
        VCML_ERROR_ON(!exists(idx), "bus port %d does not exist", idx);
        return *m_sockets.at(idx);
    }

    inline void bus::trace_in(int port, const tlm_generic_payload& tx) const {
        if (!logger::would_log(LOG_TRACE) || loglvl < LOG_TRACE)
            return;
        const tlm_target_socket<64>& tgt = IN[port];
        logger::log(LOG_TRACE, tgt.name(), ">> " + tlm_transaction_to_str(tx));
    }

    inline void bus::trace_out(int port, const tlm_generic_payload& tx) const {
        if (!logger::would_log(LOG_TRACE) || loglvl < LOG_TRACE)
            return;
        const tlm_target_socket<64>& tgt = IN[port];
        logger::log(LOG_TRACE, tgt.name(), "<< " + tlm_transaction_to_str(tx));
    }

}}

#endif
