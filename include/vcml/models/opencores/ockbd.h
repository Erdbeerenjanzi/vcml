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

#ifndef VCML_OPENCORES_OCKBD_H
#define VCML_OPENCORES_OCKBD_H

#include "vcml/common/includes.h"
#include "vcml/common/types.h"
#include "vcml/common/utils.h"
#include "vcml/common/report.h"

#include "vcml/ports.h"
#include "vcml/register.h"
#include "vcml/peripheral.h"
#include "vcml/slave_socket.h"

#include "vcml/properties/property.h"
#include "vcml/debugging/vncserver.h"

namespace vcml { namespace opencores {

    class ockbd: public peripheral {
    private:
        size_t    m_key_size;
        queue<u8> m_key_fifo;

        shared_ptr<debugging::vncserver> m_vnc;

        void update();
        void poll();

        u8 read_KHR();

    public:
        reg<ockbd, u8> KHR;

        out_port IRQ;
        slave_socket IN;

        property<clock_t> clock;
        property<u16> vncport;

        ockbd(const sc_module_name& name);
        virtual ~ockbd();
        VCML_KIND(ockbd);
    };

}}

#endif
