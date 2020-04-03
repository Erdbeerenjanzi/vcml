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

#ifndef VCML_AIO_H
#define VCML_AIO_H

#include <functional>

namespace vcml {

    enum aio_policy {
        AIO_ONCE,
        AIO_ALWAYS
    };

    typedef std::function<void(int, int)> aio_handler;

    void aio_notify(int fd, aio_handler handler, aio_policy policy);
    void aio_cancel(int fd);

}

#endif
