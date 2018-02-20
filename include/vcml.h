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

#ifndef VCML_H
#define VCML_H

#include "vcml/common/includes.h"
#include "vcml/common/types.h"
#include "vcml/common/utils.h"
#include "vcml/common/report.h"
#include "vcml/common/version.h"
#include "vcml/common/aio.h"
#include "vcml/common/thctl.h"

#include "vcml/logging/logger.h"
#include "vcml/logging/log_file.h"
#include "vcml/logging/log_stream.h"
#include "vcml/logging/log_term.h"

#include "vcml/properties/property_base.h"
#include "vcml/properties/property.h"
#include "vcml/properties/property_provider.h"
#include "vcml/properties/property_provider_arg.h"
#include "vcml/properties/property_provider_env.h"
#include "vcml/properties/property_provider_file.h"

#include "vcml/backends/backend.h"
#include "vcml/backends/backend_null.h"
#include "vcml/backends/backend_file.h"
#include "vcml/backends/backend_term.h"
#include "vcml/backends/backend_stdout.h"
#include "vcml/backends/backend_tcp.h"
#include "vcml/backends/backend_tap.h"

#include "vcml/debugging/rspserver.h"
#include "vcml/debugging/gdbstub.h"
#include "vcml/debugging/gdbserver.h"

#include "vcml/elf.h"
#include "vcml/range.h"
#include "vcml/txext.h"
#include "vcml/exmon.h"
#include "vcml/ports.h"
#include "vcml/stubs.h"
#include "vcml/dmi_cache.h"
#include "vcml/command.h"
#include "vcml/component.h"
#include "vcml/master_socket.h"
#include "vcml/slave_socket.h"
#include "vcml/register.h"
#include "vcml/peripheral.h"
#include "vcml/processor.h"

#include "vcml/models/generic/bus.h"
#include "vcml/models/generic/memory.h"
#include "vcml/models/generic/crossbar.h"
#include "vcml/models/generic/uart8250.h"

#include "vcml/models/opencores/ompic.h"
#include "vcml/models/opencores/ethoc.h"

#endif
