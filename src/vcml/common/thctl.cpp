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

#include "vcml/common/thctl.h"
#include "vcml/common/report.h"

namespace vcml {

    // we just need this class to have something that is called every cycle...
    class thctl_helper: public sc_core::sc_trace_file
    {
    public:
    #define DECL_TRACE_METHOD_A(tp) \
        virtual void trace(const tp& object, const string& nm) {}

    #define DECL_TRACE_METHOD_B(tp) \
        virtual void trace(const tp& object,const string& nm, int w) {}

        DECL_TRACE_METHOD_A(sc_event)
        DECL_TRACE_METHOD_A(sc_time)

        DECL_TRACE_METHOD_A(bool)
        DECL_TRACE_METHOD_A(sc_dt::sc_bit)
        DECL_TRACE_METHOD_A(sc_dt::sc_logic)

        DECL_TRACE_METHOD_B(unsigned char)
        DECL_TRACE_METHOD_B(unsigned short)
        DECL_TRACE_METHOD_B(unsigned int)
        DECL_TRACE_METHOD_B(unsigned long)
        DECL_TRACE_METHOD_B(char)
        DECL_TRACE_METHOD_B(short)
        DECL_TRACE_METHOD_B(int)
        DECL_TRACE_METHOD_B(long)
        DECL_TRACE_METHOD_B(sc_dt::int64)
        DECL_TRACE_METHOD_B(sc_dt::uint64)

        DECL_TRACE_METHOD_A(float)
        DECL_TRACE_METHOD_A(double)
        DECL_TRACE_METHOD_A(sc_dt::sc_int_base)
        DECL_TRACE_METHOD_A(sc_dt::sc_uint_base)
        DECL_TRACE_METHOD_A(sc_dt::sc_signed)
        DECL_TRACE_METHOD_A(sc_dt::sc_unsigned)

        DECL_TRACE_METHOD_A(sc_dt::sc_fxval)
        DECL_TRACE_METHOD_A(sc_dt::sc_fxval_fast)
        DECL_TRACE_METHOD_A(sc_dt::sc_fxnum)
        DECL_TRACE_METHOD_A(sc_dt::sc_fxnum_fast)

        DECL_TRACE_METHOD_A(sc_dt::sc_bv_base)
        DECL_TRACE_METHOD_A(sc_dt::sc_lv_base)

    #undef DECL_TRACE_METHOD_A
    #undef DECL_TRACE_METHOD_B

        virtual void trace(const unsigned int& object,
                           const std::string& name,
                           const char** enum_literals ) {};
        virtual void write_comment(const std::string& comment) {};
        virtual void set_time_unit(double v, sc_core::sc_time_unit tu) {}

        thctl_helper() {
            sc_get_curr_simcontext()->add_trace_file(this);
        }

        virtual ~thctl_helper() {
    #if SYSTEMC_VERSION >= 20140417
            sc_get_curr_simcontext()->remove_trace_file(this);
    #endif
        }

    protected:
        virtual void cycle(bool delta_cycle) override;
    };

    void thctl_helper::cycle(bool delta_cycle) {
        thctl_exit_critical();
        thctl_enter_critical();
    }

    static pthread_t thctl_init();

    pthread_t g_thctl_sysc_thread = pthread_self();
    pthread_t g_thctl_mutex_owner = thctl_init();
    pthread_mutex_t g_thctl_mutex = PTHREAD_MUTEX_INITIALIZER;

    pthread_t thctl_sysc_thread() {
        return g_thctl_sysc_thread;
    }

    bool thctl_is_sysc_thread() {
        return g_thctl_sysc_thread == pthread_self();
    }

    void thctl_enter_critical() {
        VCML_ERROR_ON(thctl_in_critical(),
                      "thread already in critical section");
        pthread_mutex_lock(&g_thctl_mutex);
        g_thctl_mutex_owner = pthread_self();
    }

    void thctl_exit_critical() {
        VCML_ERROR_ON(!thctl_in_critical(), "thread not in critical section");
        g_thctl_mutex_owner = 0;
        pthread_mutex_unlock(&g_thctl_mutex);
    }

    bool thctl_in_critical() {
        return g_thctl_mutex_owner == pthread_self();
    }

    static pthread_t thctl_init() {
        static thctl_helper instance;
        pthread_mutex_lock(&g_thctl_mutex);
        return pthread_self();
    }

}
