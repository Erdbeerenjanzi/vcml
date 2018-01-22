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
using namespace ::testing;

#include "vcml.h"

TEST(exmon, locking) {
    vcml::exmon mon;

    mon.add_lock(0, {100, 200});
    mon.add_lock(1, {300, 400});
    EXPECT_EQ(mon.get_locks().size(), 2);

    mon.break_locks({0, 400});
    EXPECT_TRUE(mon.get_locks().empty());
}

TEST(exmon, update) {
    vcml::exmon mon;

    vcml::ext_exmem ex1(1);
    vcml::ext_exmem ex2(2);
    tlm::tlm_generic_payload tx;

    tx.set_address(100);
    tx.set_data_length(4);
    tx.set_read();
    tx.set_extension(&ex1);

    EXPECT_TRUE(mon.update(tx));
    EXPECT_EQ(mon.get_locks().size(), 1);
    EXPECT_EQ(mon.get_locks()[0].addr, vcml::range(100, 103));
    EXPECT_EQ(mon.get_locks()[0].cpu, ex1.get_id());

    tx.clear_extension(&ex1);
    tx.set_extension(&ex2);

    EXPECT_TRUE(mon.update(tx));
    EXPECT_EQ(mon.get_locks().size(), 2);
    EXPECT_EQ(mon.get_locks()[1].addr, vcml::range(100, 103));
    EXPECT_EQ(mon.get_locks()[1].cpu, ex2.get_id());

    tx.set_write();

    EXPECT_TRUE(mon.update(tx));
    EXPECT_TRUE(mon.get_locks().empty());

    tx.clear_extension(&ex2);
    tx.set_extension(&ex1);

    EXPECT_FALSE(mon.update(tx));

    tx.clear_extension(&ex1);
}

TEST(exmon, dmi) {
    vcml::exmon mon;

    mon.add_lock(0, {100, 199});
    mon.add_lock(1, {300, 399});

    tlm::tlm_dmi dmi;
    dmi.set_dmi_ptr(NULL);
    dmi.set_start_address(0);
    dmi.set_end_address((sc_dt::uint64)-1);

    tlm::tlm_generic_payload tx;

    tx.set_address(100);
    EXPECT_FALSE(mon.override_dmi(tx, dmi));

    tx.set_address(399);
    EXPECT_FALSE(mon.override_dmi(tx, dmi));

    dmi.set_dmi_ptr(NULL);
    dmi.set_start_address(0);
    dmi.set_end_address((sc_dt::uint64)-1);

    tx.set_address(50);
    EXPECT_TRUE(mon.override_dmi(tx, dmi));
    EXPECT_EQ(dmi.get_start_address(), 0);
    EXPECT_EQ(dmi.get_end_address(), 99);
    EXPECT_EQ(dmi.get_dmi_ptr(), (unsigned char*)0);

    dmi.set_dmi_ptr(NULL);
    dmi.set_start_address(0);
    dmi.set_end_address((sc_dt::uint64)-1);

    tx.set_address(200);
    EXPECT_TRUE(mon.override_dmi(tx, dmi));
    EXPECT_EQ(dmi.get_start_address(), 200);
    EXPECT_EQ(dmi.get_end_address(), 299);
    EXPECT_EQ(dmi.get_dmi_ptr(), (unsigned char*)200);

    dmi.set_dmi_ptr(NULL);
    dmi.set_start_address(0);
    dmi.set_end_address((sc_dt::uint64)-1);

    tx.set_address(500);
    EXPECT_TRUE(mon.override_dmi(tx, dmi));
    EXPECT_EQ(dmi.get_start_address(), 400);
    EXPECT_EQ(dmi.get_end_address(), -1);
    EXPECT_EQ(dmi.get_dmi_ptr(), (unsigned char*)400);
}

extern "C" int sc_main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
