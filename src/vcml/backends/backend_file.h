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

#ifndef VCML_BACKEND_FILE_H
#define VCML_BACKEND_FILE_H

#include "vcml/backends/backend.h"

namespace vcml {

    class backend_file: public backend
    {
    private:
        ifstream m_rx;
        ofstream m_tx;

    public:
        property<string> rx;
        property<string> tx;

        backend_file(const sc_module_name& name = "backend",
                     const char* rx = nullptr, const char* tx = nullptr);
        virtual ~backend_file();
        VCML_KIND(backend_file);

        virtual size_t peek();
        virtual size_t read(void* buf, size_t len);
        virtual size_t write(const void* buf, size_t len);

        static backend* create(const string& name);
    };

}

#endif
