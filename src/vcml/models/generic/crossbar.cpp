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

#include "vcml/models/generic/crossbar.h"

namespace vcml { namespace generic {

    void crossbar::forward(unsigned int from) {
        bool val = IN[from].read();
        bool bcast = is_broadcast(from);

        out_port_list::iterator it;
        for (auto port : OUT)
            if (bcast || is_forward(from, port.first))
                port.second->write(val);
    }

    crossbar::crossbar(const sc_module_name& nm):
        peripheral(nm),
        m_forward(),
        IN("IN"),
        OUT("OUT") {
        /* nothing to do */
    }

    crossbar::~crossbar() {
        /* nothing to do */
    }

    void crossbar::end_of_elaboration() {
        in_port_list::iterator it;
        for (auto port : IN) {
            stringstream ss;
            ss << "forward_" << port.first;

            sc_spawn_options opts;
            opts.spawn_method();
            opts.set_sensitivity(port.second);
            opts.dont_initialize();

            sc_spawn(sc_bind(&crossbar::forward, this, it->first),
                     sc_gen_unique_name(ss.str().c_str()), &opts);
        }
    }
}}
