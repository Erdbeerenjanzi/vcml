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

#ifndef VCML_PROPERTY_PROVIDER_H
#define VCML_PROPERTY_PROVIDER_H

#include "vcml/common/includes.h"
#include "vcml/common/types.h"
#include "vcml/common/utils.h"
#include "vcml/common/report.h"

namespace vcml {

    class property_provider
    {
    private:
        struct value {
            string value;
            int uses;
        };

        std::map<string, struct value> m_values;

        virtual bool lookup(const string& name, string& value);

    public:
        property_provider();
        virtual ~property_provider();

        void add(const string& name, const string& value);

    private:
        static list<property_provider*> providers;

        static void register_provider(property_provider* provider);
        static void unregister_provider(property_provider* provider);

    public:
        static bool init(const string& name, string& value);
    };

}

#endif
