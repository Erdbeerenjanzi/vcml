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

#include "vcml/common/utils.h"

namespace vcml {

    string dirname(const string& filename) {
#ifdef _WIN32
        const char separator = '\\';
#else
        const char separator = '/';
#endif

        size_t i = filename.rfind(separator, filename.length());
        return (i == string::npos) ? "." : filename.substr(0, i);
    }

    bool file_exists(const string& filename) {
        return access(filename.c_str(), F_OK) != -1;
    }

    size_t fd_peek(int fd, time_t timeoutms) {
        if (fd < 0)
            return 0;

        fd_set in, out, err;
        struct timeval timeout;

        FD_ZERO(&in);
        FD_SET(fd, &in);
        FD_ZERO(&out);
        FD_ZERO(&err);

        timeout.tv_sec  = (timeoutms / 1000ull);
        timeout.tv_usec = (timeoutms % 1000ull) * 1000ull;

        struct timeval* ptimeout = ~timeoutms ? &timeout : nullptr;
        int ret = select(fd + 1, &in, &out, &err, ptimeout);
        return ret > 0 ? 1 : 0;
    }

    size_t fd_read(int fd, void* buffer, size_t buflen) {
        if  (fd < 0 || buffer == nullptr || buflen == 0)
            return 0;

        char* ptr = reinterpret_cast<char*>(buffer);

        size_t numread = 0;
        while (numread < buflen) {
            ssize_t res = ::read(fd, ptr + numread, buflen - numread);
            if (res <= 0)
                return numread;
            numread += res;
        }

        return numread;
    }

    size_t fd_write(int fd, const void* buffer, size_t buflen) {
        if  (fd < 0 || buffer == nullptr || buflen == 0)
            return false;

        const char* ptr = reinterpret_cast<const char*>(buffer);

        size_t written = 0;
        while (written < buflen) {
            ssize_t res = ::write(fd, ptr + written, buflen - written);
            if (res <= 0)
                return written;
            written += res;
        }

        return written;
    }

    string tempdir() {
#ifdef _WIN32
        // ToDo: implement tempdir for windows
#else
        return "/tmp/";
#endif
    }

    string progname() {
        char path[PATH_MAX];
        ssize_t len = readlink("/proc/self/exe", path, sizeof(path) - 1);

        if (len == -1)
            return "unknown";

        path[len] = '\0';
        return path;
    }

    string username() {
        char uname[255];
        if (getlogin_r(uname, sizeof(uname)))
            return "unknown";
        return uname;
    }

    double realtime() {
        struct timespec tp;
        clock_gettime(CLOCK_REALTIME, &tp);
        return tp.tv_sec + tp.tv_nsec * 1e-9;
    }

    string tlm_response_to_str(tlm_response_status status) {
        switch (status) {
        case TLM_OK_RESPONSE:
            return "TLM_OK_RESPONSE";

        case TLM_INCOMPLETE_RESPONSE:
            return "TLM_INCOMPLETE_RESPONSE";

        case TLM_GENERIC_ERROR_RESPONSE:
            return "TLM_GENERIC_ERROR_RESPONSE";

        case TLM_ADDRESS_ERROR_RESPONSE:
            return "TLM_ADDRESS_ERROR_RESPONSE";

        case TLM_COMMAND_ERROR_RESPONSE:
            return "TLM_COMMAND_ERROR_RESPONSE";

        case TLM_BURST_ERROR_RESPONSE:
            return "TLM_BURST_ERROR_RESPONSE";

        case TLM_BYTE_ENABLE_ERROR_RESPONSE:
            return "TLM_BYTE_ENABLE_ERROR_RESPONSE";

        default: {
                stringstream ss;
                ss << "TLM_UNKNOWN_RESPONSE (" << status << ")";
                return ss.str();
            }
        }
    }

    string tlm_transaction_to_str(const tlm_generic_payload& tx) {
        stringstream ss;

        // command
        switch (tx.get_command()) {
        case TLM_READ_COMMAND  : ss << "RD "; break;
        case TLM_WRITE_COMMAND : ss << "WR "; break;
        default: ss << "IG "; break;
        }

        // address
        ss << "0x";
        ss << std::hex;
        ss.width(16);
        ss.fill('0');
        ss << tx.get_address();

        // data array
        unsigned int size = tx.get_data_length();
        unsigned char* c = tx.get_data_ptr();

        ss << " [";
        if (size == 0)
            ss << "<no data>";
        for (unsigned int i = 0; i < size; i++) {
            ss.width(2);
            ss.fill('0');
            ss << static_cast<unsigned int>(*c++);
            if (i != (size - 1))
                ss << " ";
        }
        ss << "]";

        // response status
        ss << " (" << tx.get_response_string() << ")";

        // ToDo: byte enable, streaming, etc.
        return ss.str();
    }

    u64 time_to_ns(const sc_time& t) {
        return t.value() / sc_time(1.0, SC_NS).value();
    }

    u64 time_to_us(const sc_time& t) {
        return t.value() / sc_time(1.0, SC_US).value();
    }

    u64 time_to_ms(const sc_time& t) {
        return t.value() / sc_time(1.0, SC_MS).value();
    }

    bool is_thread(sc_process_b* proc) {
        if (proc == nullptr)
            proc = sc_get_current_process_b();
        if (proc == nullptr)
            return false;
        return proc->proc_kind() == sc_core::SC_THREAD_PROC_;
    }

    bool is_method(sc_process_b* proc) {
        if (proc == nullptr)
            proc = sc_get_current_process_b();
        if (proc == nullptr)
            return false;
        return proc->proc_kind() == sc_core::SC_METHOD_PROC_;
    }

    sc_process_b* current_thread() {
        sc_process_b* proc = sc_get_current_process_b();
        if (proc == nullptr || proc->proc_kind() != sc_core::SC_THREAD_PROC_)
            return nullptr;
        return proc;
    }

    sc_process_b* current_method() {
        sc_process_b* proc = sc_get_current_process_b();
        if (proc == nullptr || proc->proc_kind() != sc_core::SC_METHOD_PROC_)
            return nullptr;
        return proc;
    }

    sc_object* find_object(const string& name) {
        return sc_core::sc_find_object(name.c_str());
    }

    sc_attr_base* find_attribute(const string& name) {
        size_t pos = name.rfind(SC_HIERARCHY_CHAR);
        if (pos == string::npos)
            return NULL;

        sc_object* parent = find_object(name.substr(0, pos));
        if (parent == nullptr)
            return nullptr;

        return parent->get_attribute(name);
    }

    string call_origin() {
        pthread_t this_thread = pthread_self();
        if (this_thread != thctl_sysc_thread()) {
            char buffer[16] = { 0 };
            pthread_getname_np(this_thread, buffer, sizeof(buffer));
            return mkstr("pthread '%s'", buffer);
        }

        sc_core::sc_simcontext* simc = sc_core::sc_get_curr_simcontext();
        if (simc) {
            sc_process_b* proc = sc_get_current_process_b();
            if (proc)
                return proc->name();

            sc_module* module = simc->hierarchy_curr();
            if (module)
                return module->name();
        }

        return "";
    }

    vector<string> backtrace(unsigned int frames, unsigned int skip) {
        vector<string> sv;

        void* symbols[frames + skip];
        unsigned int size = (unsigned int)::backtrace(symbols, frames + skip);
        if (size <= skip)
            return sv;

        sv.resize(size - skip);

        size_t dmbufsz = 256;
        char* dmbuf = (char*)malloc(dmbufsz);
        char** names = ::backtrace_symbols(symbols, size);
        for (unsigned int i = skip; i < size; i++) {
            char* func = NULL, *offset = NULL, *end = NULL;
            for (char* ptr = names[i]; *ptr != '\0'; ptr++) {
                if (*ptr == '(')
                    func = ptr++;
                else if (*ptr == '+')
                    offset = ptr++;
                else if (*ptr == ')') {
                    end = ptr++;
                    break;
                }
            }

            if (!func || !offset || !end) {
                sv[i-skip] = mkstr("<unknown> [%p]", symbols[i]);
                continue;
            }

            *func++ = '\0';
            *offset++ = '\0';
            *end = '\0';

            sv[i-skip] = string(func) + "+" + string(offset);

            int status = 0;
            char* res = abi::__cxa_demangle(func, dmbuf, &dmbufsz, &status);
            if (status == 0) {
                sv[i-skip] = string(dmbuf) + "+" + string(offset);
                dmbuf = res; // dmbuf might get reallocated
            }
        }

        free(names);
        free(dmbuf);

        return sv;
    }

    bool is_debug_build() {
#ifdef VCML_DEBUG
        return true;
#else
        return false;
#endif
    }

}
