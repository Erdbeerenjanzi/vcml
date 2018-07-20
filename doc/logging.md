# VCML Logging
----

VCML uses a centralized logging system that allows its individual components
and models to report on everything that happens during the simulation. Log
messages are split into five categories, listed below in priority order:

* `vcml::LOG_ERROR`: highest priority, used for reporting on errors, e.g., when
an exception is caught.
* `vcml::LOG_WARNING`: used for reporting abnormal events, such as missing files
or erroneous configurations.
* `vcml::LOG_INFO`: used for generic info messages, e.g., for printing out the
simulation duriation or bus memory maps at the beginning of the simulation.
* `vcml::LOG_DEBUG`: for printing debug messages
* `vcml::LOG_TRACE`: lowest priority, used for tracing transactions entering or
exiting busses or peripheral components.

Log message output adheres the following format:

`[<LEVEL> <TIME>] <SOURCE>: <MESSAGE>`

In this format, `<LEVEL>` refers to the log level and is shortend to its first
letter, i.e., `E`, `W`, `I`, `D` or `T`. After the log level, the current SystemC
time is printed with nanosecond resolution. One can optionally also print out
the current delta cycle after the time stamp by setting
`vcml::logger::print_delta_cycle = true`. The `<SOURCE>` of the log message is
automatically determined, depending on the log function you called (see below).
A full log message could therefore look like this:

`[D 1.000000200] system.uart0: component disabled`

This tells us that there was a debug message at *1s + 200ns*, informing us that
the component `system.uart0` has been disabled.

Log messages can be generated using the top level logging functions from the
`vcml` namespace. These functions use the current SystemC process name as the
log message source. They use a `printf` style for string formatting:

```
namespace vcml {
    void vcml::log_error(const char* format, ...);
    void vcml::log_warn(const char* format, ...);
    void vcml::log_info(const char* format, ...);
    void vcml::log_debug(const char* format, ...);
}
```

Furthermore, the base class used for modelling hardware in VCML `vcml::component`
also provides logging functionality, that will add the name of the component
as the source of the log message:

```
namespace vcml {
    class component {
    public:
        // ...
        void log_error(const char* format, ...);
        void log_warn(const char* format, ...);
        void log_info(const char* format, ...);
        void log_debug(const char* format, ...);
        // ....
    };
}
```

## Logging Output
---
In order to actually see log messages, you need a logger that will receive the
messages and display them in some form. VCML currently provides three loggers:

* `vcml::log_term`: write the log message to `stderr` using color if available.
* `vcml::log_file`: write log messages to a file specified during construction.
* `vcml::log_stream`: sends the log messages to any `std::ostream` instance.

Each logger can furthermore also specify the log levels it is interested in.
This is done by calling the appropriate `set_level` methods as shown below:

```
vcml::log_file my_logger("log.txt");
my_logger.set_level(vcml::LOG_ERROR); // only receives error messages
my_logger.set_level(vcml::LOG_ERROR, vcml::LOG_INFO); // receives error, warning and info messages
```

Having multiple loggers for different purposes is possible and encourages. You
could have for example a logger that exclusively receives messages from the
`vcml::LOG_DEBUG` level and writes them to a files. Then you could have a second
logger to print all other messages to console:

```
#include <vcml.h>

int sc_main(int argc, char** argv) {
    vcml::log_file debug("debug.txt");
    debug.set_level(vcml::LOG_DEBUG);

    vcml::log_term terminal;
    terminal.set_level(vcml::LOG_ERROR, vcml::LOG_INFO);

    ...
}
```

It is also possible to create your own logger if you need to process VCML log
messages in a customizable way. To define a custom logger, derive from
`vcml::logger` and override `vcml::logger::log_line`:

```
class my_logger: public vcml::logger {
public:
    my_logger() ...
    virtual ~my_logger() ...
    virtual void log_line(vcml::log_level lvl, const char* line);
};
```

## Exceptions and Tracing
---

The logging system is typically also used for TLM transaction tracing as well
as for exception reporting. VCML generally uses the class `vcml::report` for
unrecoverable exceptions, so a good approach to handling would be:

```
int sc_main(int argc, char** argv) {
    vcml::log_term logger;
    try {
        // construct system
        sc_core::sc_start();
        return 0;
    } catch (vcml::report& r) {
        vcml::logger::log(r);
        return EXIT_FAILURE;
    } catch (std::exception& e) {
        vcml::log_error(e.what());
        return EXIT_FAILURE;
    }
}
```

*Note*: when you are using `vcml::logger::log(const vcml::report&)` to log an
exception, VCML will additionally print the call stack from when the exception
was thrown for debug purposes. This behaviour can be disabled by setting
`vcml::logger::print_backtrace = false`.

You can also use `vcml::trace` and `vcml::trace_errors` to log transactions or
just transactions that have an error state set. This feature is generally used
with buses and peripheral components to report on incoming and outgoing
transactions. The transaction type must be `tlm::tlm_generic_payload`. The
transaction will be converted to string using the following format:

`<OP> <ADDRESS> [<VALUE>] (<RESPONSE>)`

Here, `<OP>` referes to the access operation and can be `RD` (read) or `WR`
(write). `<ADDRESS>` shows the destination address in hex and `<VALUE>` contains
the individual byte values of the internal transaction buffer. `<RESPONSE>`
holds the current state of the transaction, see the TLM reference manual for
details. Following is an example of an actual TLM transaction trace:

* reading four bytes from address 0x100: `RD 0x0000100 [00 FF 00 FF] (TLM_OK_RESPONSE)`
* writing single byte to invalid address: `WR 0xFFFFFFFF [EE] (TLM_ADDRESS_ERROR_RESPONSE)`

----
Documentation `vcml-1.0` July 2018
