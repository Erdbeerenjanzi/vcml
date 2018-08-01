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

#ifndef VCML_PROPERTY_H
#define VCML_PROPERTY_H

#include "vcml/common/includes.h"
#include "vcml/common/types.h"
#include "vcml/common/utils.h"
#include "vcml/common/report.h"

#include "vcml/properties/property_base.h"
#include "vcml/properties/property_provider.h"

namespace vcml {

    template <typename T, const unsigned int N = 1>
    class property: public property_base
    {
    private:
        T      m_value[N];
        T      m_defval;
        string m_strval;
        bool   m_inited;

        void update_strval();

        // disabled
        property();
        property(const property&);

    public:
        property(const char* nm, const T& defval = T(), sc_module* mod = NULL);
        virtual ~property();

        virtual const char* str() const;
        virtual void str(const string& s);

        constexpr unsigned int num() const;

        const T& get() const;
        T& get();

        const T& get(unsigned int idx) const;
        T& get(unsigned int idx);

        void set(const T& val);
        void set(const T  val[N]);
        void set(const T& val, unsigned int idx);

        const T& get_default() const;
        void set_default(const T& defval);

        operator T() const;
        T operator ~ () const;

        const T& operator [] (unsigned int idx) const;
        T& operator [] (unsigned int idx);

        property<T, N>& operator = (const property<T, N>& other);

        template <typename T2> property<T, N>& operator   = (const T2& other);
        template <typename T2> property<T, N>& operator  += (const T2& other);
        template <typename T2> property<T, N>& operator  -= (const T2& other);
        template <typename T2> property<T, N>& operator  *= (const T2& other);
        template <typename T2> property<T, N>& operator  /= (const T2& other);
        template <typename T2> property<T, N>& operator  %= (const T2& other);
        template <typename T2> property<T, N>& operator  &= (const T2& other);
        template <typename T2> property<T, N>& operator  |= (const T2& other);
        template <typename T2> property<T, N>& operator  ^= (const T2& other);
        template <typename T2> property<T, N>& operator <<= (const T2& other);
        template <typename T2> property<T, N>& operator >>= (const T2& other);

        template <typename T2> bool operator == (const T2& other);
        template <typename T2> bool operator != (const T2& other);
        template <typename T2> bool operator <= (const T2& other);
        template <typename T2> bool operator >= (const T2& other);
        template <typename T2> bool operator  < (const T2& other);
        template <typename T2> bool operator  > (const T2& other);

    };

    template <typename T, const unsigned int N>
    void property<T, N>::update_strval() {
        m_strval = "";
        for (unsigned int i = 0; i < (N - 1); i++)
            m_strval += to_string<T>(m_value[i]) + ", ";
        m_strval += to_string<T>(m_value[N - 1]);
    }

    template <typename T, const unsigned int N>
    property<T, N>::property(const char* nm, const T& def, sc_module* m):
        property_base(nm, m),
        m_value(),
        m_defval(def),
        m_strval(),
        m_inited(false) {
        for (unsigned int i = 0; i < N; i++)
            m_value[i] = m_defval;
        update_strval();

        string init;
        if (property_provider::init(name(), init))
            str(init);
    }

    template <typename T, const unsigned int N>
    property<T, N>::~property() {
        /* nothing to do */
    }

    template <typename T, const unsigned int N>
    inline const char* property<T, N>::str() const {
        return m_strval.c_str();
    }

    template <typename T, const unsigned int N>
    inline void property<T, N>::str(const string& s) {
        m_inited = true;
        m_strval = s;

        vector<string> args = split(m_strval, ',');
        unsigned int size = args.size();

        if (size < N) {
            log_warn("property %s has not enough initializers", name());
        } else if (size > N) {
            log_warn("property %s has too many initializers", name());
        }

        for (unsigned int i = 0; i < min(N, size); i++)
            m_value[i] = from_string<T>(args[i]);
    }

    template <typename T, const unsigned int N>
    constexpr unsigned int property<T, N>::num() const {
        return N;
    }

    template <typename T, const unsigned int N>
    const T& property<T, N>::get() const {
        return get(0);
    }

    template <typename T, const unsigned int N>
    inline T& property<T, N>::get() {
        return get(0);
    }

    template <typename T, const unsigned int N>
    const T& property<T, N>::get(unsigned int idx) const {
        VCML_ERROR_ON(idx >= N, "index %d out of bounds", idx);
        return m_value[idx];
    }

    template <typename T, const unsigned int N>
    inline T& property<T, N>::get(unsigned int idx) {
        VCML_ERROR_ON(idx >= N, "index %d out of bounds", idx);
        return m_value[idx];
    }

    template <typename T, const unsigned int N>
    inline void property<T, N>::set(const T& val) {
        for (unsigned int i = 0; i < N; i++)
            m_value[i] = val;
        m_inited = true;
        update_strval();
    }

    template <typename T, const unsigned int N>
    inline void property<T, N>::set(const T val[N]) {
        for (unsigned int i = 0; i < N; i++)
            m_value[i] = val[i];
        m_inited = true;
        update_strval();
    }

    template <typename T, const unsigned int N>
    inline void property<T, N>::set(const T& val, unsigned int idx) {
        VCML_ERROR_ON(idx >= N, "index %d out of bounds", idx);
        m_value[idx] = val;
        m_inited = true;
        update_strval();
    }

    template <typename T, const unsigned int N>
    inline const T& property<T, N>::get_default() const {
        return m_defval;
    }

    template <typename T, const unsigned int N>
    inline void property<T,N>::set_default(const T& defval) {
        m_defval = defval;
        if (!m_inited)
            set(defval);
    }

    template <typename T, const unsigned int N>
    inline property<T,N>::operator T() const {
        return get(0);
    }

    template <typename T, const unsigned int N>
    inline T property<T,N>::operator ~ () const {
        return ~get(0);
    }

    template <typename T, const unsigned int N>
    inline const T& property<T,N>::operator [] (unsigned int idx) const {
        return get(idx);
    }

    template <typename T, const unsigned int N>
    inline T& property<T,N>::operator [] (unsigned int idx) {
        return get(idx);
    }

    template <typename T, const unsigned int N>
    inline property<T,N>& property<T, N>::operator = (const property<T,N>& o) {
        for (unsigned int i = 0; i < N; i++)
            set(o.m_value[i], i);
        return *this;
    }

    template <typename T, const unsigned int N> template<typename T2>
    inline property<T, N>& property<T, N>::operator = (const T2& other) {
        set(other);
        return *this;
    }

    template <typename T, const unsigned int N>template <typename T2>
    inline property<T, N>& property<T, N>::operator += (const T2& other) {
        set(get() + other, 0);
        return *this;
    }

    template <typename T, const unsigned int N> template<typename T2>
    inline property<T, N>& property<T, N>::operator -= (const T2& other) {
        set(get() - other, 0);
        return *this;
    }

    template <typename T, const unsigned int N> template<typename T2>
    inline property<T, N>& property<T, N>::operator *= (const T2& other) {
        set(get() * other);
        return *this;
    }

    template <typename T, const unsigned int N> template<typename T2>
    inline property<T, N>& property<T, N>::operator /= (const T2& other) {
        set(get() / other);
        return *this;
    }

    template <typename T, const unsigned int N> template<typename T2>
    inline property<T, N>& property<T, N>::operator %= (const T2& other) {
        set(get() % other);
        return *this;
    }

    template <typename T, const unsigned int N> template<typename T2>
    inline property<T, N>& property<T, N>::operator &= (const T2& other) {
        set(get() & other);
        return *this;
    }

    template <typename T, const unsigned int N> template<typename T2>
    inline property<T, N>& property<T, N>::operator |= (const T2& other) {
        set(get() | other);
        return *this;
    }

    template <typename T, const unsigned int N> template<typename T2>
    inline property<T, N>& property<T, N>::operator ^= (const T2& other) {
        set(get() ^ other);
        return *this;
    }

    template <typename T, const unsigned int N> template<typename T2>
    inline property<T, N>& property<T, N>::operator <<= (const T2& other) {
        set(get() << other);
        return *this;
    }

    template <typename T, const unsigned int N> template<typename T2>
    inline property<T, N>& property<T, N>::operator >>= (const T2& other) {
        set(get() >> other);
        return *this;
    }

    template <typename T, const unsigned int N> template<typename T2>
    inline bool property<T, N>::operator == (const T2& other) {
        for (unsigned int i = 0; i < N; i++)
            if (m_value[i] != other)
                return false;
        return true;
    }

    template <typename T, const unsigned int N> template <typename T2>
    inline bool property<T, N>::operator < (const T2& other) {
        for (unsigned int i = 0; i < N; i++)
            if (m_value[i] >= other)
                return false;
        return true;
    }

    template <typename T, const unsigned int N> template <typename T2>
    inline bool property<T, N>::operator > (const T2& other) {
        for (unsigned int i = 0; i < N; i++)
            if (m_value[i] <= other)
                return false;
        return true;
    }

    template <typename T, const unsigned int N> template <typename T2>
    inline bool property<T, N>::operator != (const T2& other) {
        return !operator == (other);
    }

    template <typename T, const unsigned int N> template <typename T2>
    inline bool property<T, N>::operator <= (const T2& other) {
        return !operator > (other);
    }

    template <typename T, const unsigned int N>template <typename T2>
    inline bool property<T, N>::operator >= (const T2& other) {
        return !operator < (other);
    }

}

#endif
