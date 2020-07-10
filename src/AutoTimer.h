#ifndef PARADIGM4_PICO_CORE_AUTO_TIMER_H
#define PARADIGM4_PICO_CORE_AUTO_TIMER_H

#include <chrono>

#include "Accumulator.h"
#include "TimerAggregator.h"

#define VTIMER_LEVEL 1
namespace paradigm4 {
namespace pico {
namespace core {

inline bool& pico_is_evaluate_performance() {
    static bool is_evaluate_performance = false;
    return is_evaluate_performance;
}

template<class CLOCK = std::chrono::steady_clock,
    class DUR = std::chrono::duration<double, std::ratio<1, 1000>>>
class BasicAutoTimer : public NoncopyableObject {
public:
    BasicAutoTimer(const std::string& name, const size_t flush_freq = 1024)
        : BasicAutoTimer(true, name, flush_freq) {
    }

    BasicAutoTimer(bool final_need_flush, const std::string& name,  const size_t flush_freq = 128) {
        if (!pico_is_evaluate_performance()) {
            return;
        }
        _acc.reset(new Accumulator<TimerAggregator<typename DUR::rep>>(name, flush_freq, final_need_flush));
        _stoped = false;
        _start = CLOCK::now();
    }

    BasicAutoTimer(BasicAutoTimer<CLOCK, DUR>&& bat) {
        if (!pico_is_evaluate_performance()) {
            return;
        }
        _stoped = bat._stoped;
        _acc = std::move(bat._acc);
        _start = std::move(bat._start);
        bat._stoped = true;
    }
    
    ~BasicAutoTimer() {
        stop();
    }

    void start() {
        if (!pico_is_evaluate_performance()) {
            return;
        }
        _stoped = false;
        _start = CLOCK::now();
    }

    void stop() {
        if (!pico_is_evaluate_performance()) {
            return;
        }
        if (!_stoped) {
            _acc->write(std::chrono::duration_cast<DUR>(
                        CLOCK::now() - _start).count());
            _stoped = true;
        }
    }

    static const std::string& unit2string() {
        static std::string unit = "unknown";
        return unit;
    };

private:
    std::unique_ptr<Accumulator<TimerAggregator<typename DUR::rep>>> _acc;
    typename CLOCK::time_point _start;
    bool _stoped = false;
};

enum PicoTime {ms, us, s, m, h};

template<PicoTime PT>
class AutoTimer {
};

template<>
class AutoTimer<PicoTime::ms> : public BasicAutoTimer<> {
public:
    using BasicAutoTimer<>::BasicAutoTimer;

    static const std::string& unit2string() {
        static std::string unit = "ms";
        return unit;
    }
};

template<>
class AutoTimer<PicoTime::us> : public BasicAutoTimer<std::chrono::steady_clock,
      std::chrono::duration<double, std::ratio<1, 1000000>>> {
public:
    typedef BasicAutoTimer<std::chrono::steady_clock,
            std::chrono::duration<double, std::ratio<1, 1000000>>> inner_timer_t;
    using inner_timer_t::BasicAutoTimer;

    static const std::string& unit2string() {
        static std::string unit = "us";
        return unit;
    }
};

template<>
class AutoTimer<PicoTime::s> : public BasicAutoTimer<std::chrono::steady_clock,
      std::chrono::duration<double, std::ratio<1, 1>>> {
public:
    typedef BasicAutoTimer<std::chrono::steady_clock,
            std::chrono::duration<double, std::ratio<1, 1>>> inner_timer_t;
    using inner_timer_t::BasicAutoTimer;
    
    static const std::string& unit2string() {
        static std::string unit = "sec";
        return unit;
    }
};

template<>
class AutoTimer<PicoTime::m> : public BasicAutoTimer<std::chrono::steady_clock,
      std::chrono::duration<double, std::ratio<60, 1>>> {
public:
    typedef BasicAutoTimer<std::chrono::steady_clock,
            std::chrono::duration<double, std::ratio<60, 1>>> inner_timer_t;
    using inner_timer_t::BasicAutoTimer;
    
    static const std::string& unit2string() {
        static std::string unit = "min";
        return unit;
    }
};

template<>
class AutoTimer<PicoTime::h> : public BasicAutoTimer<std::chrono::steady_clock,
      std::chrono::duration<double, std::ratio<3600, 1>>> {
public:
    typedef BasicAutoTimer<std::chrono::steady_clock,
            std::chrono::duration<double, std::ratio<3600, 1>>> inner_timer_t;
    using inner_timer_t::BasicAutoTimer;
    
    static const std::string& unit2string() {
        static std::string unit = "hr";
        return unit;
    }
};

inline std::string pico_generate_auto_timer_name(const std::string& component,
        const std::string& name, const std::string& unit) {
    return "timer::" + component + "::" + name + "(" + unit + ")";
}

template<PicoTime PT>
AutoTimer<PT> inner_create_auto_timer(
        const std::string& component, const std::string& name, const size_t flush_freq) {
    static std::string dummy_auto_timer_name = "dummy";
    if (pico_is_evaluate_performance()) {
        return AutoTimer<PT>(
                pico_generate_auto_timer_name(component, name, AutoTimer<PT>::unit2string()), 
                flush_freq);
    } else {
        return AutoTimer<PT>(dummy_auto_timer_name, flush_freq);
    }
}

inline AutoTimer<PicoTime::ms> pico_framework_auto_timer_ms(
        const std::string& name, const size_t flush_freq = 1024) {
    return inner_create_auto_timer<PicoTime::ms>("framework", name, flush_freq);
}

inline AutoTimer<PicoTime::ms> pico_application_auto_timer_ms(
        const std::string& name, const size_t flush_freq = 1024) {
    return inner_create_auto_timer<PicoTime::ms>("application", name, flush_freq);
}

inline AutoTimer<PicoTime::us> pico_framework_auto_timer_us(
        const std::string& name, const size_t flush_freq = 1024) {
    static std::string component = "framework";
    return inner_create_auto_timer<PicoTime::us>(component, name, flush_freq);
}

inline AutoTimer<PicoTime::us> pico_application_auto_timer_us(
        const std::string& name, const size_t flush_freq = 1024) {
    static std::string component = "application";
    return inner_create_auto_timer<PicoTime::us>(component, name, flush_freq);
}
template<PicoTime PT >
struct AutoTimerWrapper{
    AutoTimerWrapper(AutoTimer<PT>& ti):timer(ti){timer.start();}
    ~AutoTimerWrapper(){
        timer.stop();
    }
    AutoTimer<PT>& timer;
};
#ifndef PICO_AUTO_TIMER_FAST_V
#define PICO_AUTO_TIMER_FAST_V(timer, component, name, type, flush_freq) \
    static std::string _pico_auto_timer_name_##timer = \
            paradigm4::pico::core::pico_generate_auto_timer_name(component, name, \
                    paradigm4::pico::core::AutoTimer<type>::unit2string()); \
    paradigm4::pico::core::AutoTimer<type> timer = \
            paradigm4::pico::core::AutoTimer<type>(_pico_auto_timer_name_##timer, flush_freq);
#endif

#ifndef PICO_AUTO_TIMER_FAST
#define PICO_AUTO_TIMER_FAST(timer, component, name, type) \
    PICO_AUTO_TIMER_FAST_V(timer, component, name, type, 1024)
#endif

#define _VTIMER(component, name, unit, arg...)  \
static thread_local paradigm4::pico::core::AutoTimer<PicoTime::unit> PICO_CAT(_timer_, __LINE__)(gettid()!=getpid(), "timer::"#component"::"#name"("#unit")", ##arg); \
paradigm4::pico::core::AutoTimerWrapper<PicoTime::unit> PICO_CAT(_wrapper_, __LINE__)(PICO_CAT(_timer_, __LINE__));

#if VTIMER_LEVEL>=0
#define VTIMER_0 _VTIMER
#else
#define VTIMER_0(...)
#endif
#if VTIMER_LEVEL>0
#define VTIMER_1 _VTIMER
#else
#define VTIMER_1(...)
#endif
#if VTIMER_LEVEL>1
#define VTIMER_2 _VTIMER
#else
#define VTIMER_2(...)
#endif
#if VTIMER_LEVEL>2
#define VTIMER_3 _VTIMER
#else
#define VTIMER_3(...)
#endif
#if VTIMER_LEVEL>3
#define VTIMER_4 _VTIMER
#else
#define VTIMER_4(...)
#endif
#define VTIMER(level, args...) VTIMER_##level(args)

}
} // namespace pico
} // namespace paradigm4

#endif // PARADIGM4_PICO_COMMON_AUTO_TIMER_H
