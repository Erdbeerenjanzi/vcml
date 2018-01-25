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

#ifndef VCML_DMI_H
#define VCML_DMI_H

#include "vcml/common/includes.h"
#include "vcml/common/types.h"
#include "vcml/common/utils.h"
#include "vcml/common/report.h"

#include "vcml/range.h"

namespace vcml {

    class dmi_cache
    {
    private:
        vector<tlm_dmi> m_entries;

        void cleanup();

    public:
        inline const vector<tlm_dmi> get_entries() const {
            return m_entries;
        }

        dmi_cache();
        virtual ~dmi_cache();

        void insert(const tlm_dmi& dmi);

        void invalidate(u64 start, u64 end);
        void invalidate(const range& r);

        bool lookup(const range& r, tlm_command c, tlm_dmi& dmi);
        bool lookup(u64 addr, u64 size, tlm_command c, tlm_dmi& dmi);
        bool lookup(const tlm_generic_payload& tx, tlm_dmi& dmi);
    };

    static inline void dmi_set_access(tlm_dmi& dmi, vcml_access a) {
        switch (a) {
        case VCML_ACCESS_READ: dmi.allow_read(); break;
        case VCML_ACCESS_WRITE: dmi.allow_write(); break;
        case VCML_ACCESS_READ_WRITE: dmi.allow_read_write(); break;
        default: dmi.allow_none();
        }
    }

    static inline bool dmi_check_access(const tlm_dmi& dmi, tlm_command cmd) {
        switch (cmd) {
        case TLM_READ_COMMAND: return dmi.is_read_allowed();
        case TLM_WRITE_COMMAND: return dmi.is_write_allowed();
        case TLM_IGNORE_COMMAND:
        default:
            break;
        }

        return false;
    }

    static inline unsigned char* dmi_get_ptr(const tlm_dmi& dmi, u64 addr) {
        return dmi.get_dmi_ptr() + addr - dmi.get_start_address();
    }

    static inline void dmi_set_start_address(tlm_dmi& dmi, u64 addr) {
        dmi.set_dmi_ptr(dmi_get_ptr(dmi, addr));
        dmi.set_start_address(addr);
    }

}

#endif
