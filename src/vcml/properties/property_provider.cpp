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

#include "vcml/properties/property_provider.h"
#include "vcml/logging/logger.h"

namespace vcml {

    bool property_provider::lookup(const string& name, string& value) {
        if (!stl_contains(m_values, value))
            return false;

        struct value val = m_values[name];
        value = val.value;
        val.uses++;
        return true;
    }

    property_provider::property_provider():
        m_values() {
        register_provider(this);
    }

    property_provider::~property_provider() {
        unregister_provider(this);

        std::map<string, struct value>::const_iterator it;
        for (auto val: m_values) {
            if (val.second.uses == 0)
                log_debug("unused property '%s'", it->first.c_str());
        }
    }

    void property_provider::add(const string& name, const string& value) {
        struct value val = {
            .value = value,
            .uses = 0
        };

        m_values[name] = val;
    }

    vector<property_provider*> property_provider::providers;

    void property_provider::register_provider(property_provider* p) {
        stl_add_unique(providers, p);
    }

    void property_provider::unregister_provider(property_provider* p) {
        stl_remove_erase(providers, p);
    }

    bool property_provider::init(const string& name, string& value) {
        for (unsigned int i = 0; i < providers.size(); i++)
            if (providers[i]->lookup(name, value))
                return true;
        return false;
    }

}
