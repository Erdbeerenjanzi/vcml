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

#include <gtest/gtest.h>
#include <gmock/gmock.h>

using namespace ::testing;

#include "vcml.h"

class mock_component: public vcml::component
{
public:
    vcml::master_socket OUT;

    mock_component(const sc_core::sc_module_name& nm):
        vcml::component(nm),
        OUT("OUT") {
        /* nothing to do */
    }
};

TEST(generic_memory, access) {
    mock_component mock("MOCK");
    vcml::generic::memory mem("MEM", 0x1000);
    mock.OUT.bind(mem.IN);

    sc_core::sc_start(sc_core::SC_ZERO_TIME);

    mock.OUT.write(0x0, 0x11223344);
    mock.OUT.write(0x4, 0x55667788);

    vcml::u64 data;
    EXPECT_EQ(mock.OUT.read(0x0, data), tlm::TLM_OK_RESPONSE);
    EXPECT_EQ(data, 0x5566778811223344ull);
    EXPECT_EQ(mock.get_dmi().get_entries().size(), mem.get_dmi().get_entries().size());

    mem.readonly = true;

    EXPECT_EQ(mock.OUT.write(0x0, 0xfefefefe, vcml::VCML_FLAG_NODMI), tlm::TLM_COMMAND_ERROR_RESPONSE);
    EXPECT_EQ(mock.OUT.write(0x0, 0xfefefefe, vcml::VCML_FLAG_DEBUG), tlm::TLM_OK_RESPONSE);

    EXPECT_EQ(mock.OUT.write(0x0, 0xfefefefe), tlm::TLM_OK_RESPONSE);
    mock.get_dmi().invalidate(0, -1);
    EXPECT_EQ(mock.OUT.write(0x0, 0xfefefefe), tlm::TLM_COMMAND_ERROR_RESPONSE);
}

extern "C" int sc_main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    ::testing::InitGoogleMock(&argc, argv);
    return RUN_ALL_TESTS();
}
