#ifndef PARADIGM4_PICO_COMMON_INCLUDE_CONFIGURE_CHECKERS_H
#define PARADIGM4_PICO_COMMON_INCLUDE_CONFIGURE_CHECKERS_H

#include <type_traits>
#include <functional>
#include <string>
#include <unordered_set>

#include <boost/regex.hpp>
#include "pico_log.h"
#include "error_code.h"
#include "ConfigureHelper.h"

namespace paradigm4 {
namespace pico {
namespace core {

template<class T>
class DefaultChecker {
public:
    std::function<bool(const T&, const std::string&)> checker() const {
        return [](const T&, const std::string&) -> bool {
            return true;
        };
    }

    std::string tostring() const {
        return "no constraint";
    }
};

template<class U>
class RangeCheckerCC {
public:
    RangeCheckerCC(const U& min, const U& max) : _min(min), _max(max) {}

    std::function<bool(const U&, const std::string&)> checker() const {
        return inner_checker<U>();
    }

    template<class T>
    std::enable_if_t<IsGloggable<T>::value,
        std::function<bool(const T&, const std::string&)>> inner_checker() const {
        static_assert(IsLessEqualComparable<T>::value, "type must be LessEqualComparable");
        return [=](const T& val, const std::string& name) -> bool {
            bool check_ok = _min <= val && val <= _max;
            if (!check_ok) {
                ELOG(WARNING, PICO_CORE_ERRCODE(CFG_CHECKER)) 
                    << "Configure [" << name << "] value [" << val 
                    << "] not in range [" << _min << ", " << _max << "]";
            }   
            return check_ok;
        };
    }

    template<class T>
    std::enable_if_t<!IsGloggable<T>::value,
        std::function<bool(const T&, const std::string&)>> inner_checker() const {
        static_assert(IsLessEqualComparable<T>::value, "type must be LessEqualComparable");
        return [=](const T& val, const std::string& name) -> bool {
            bool check_ok = _min <= val && val <= _max;
            if (!check_ok) {
                ELOG(WARNING, PICO_CORE_ERRCODE(CFG_CHECKER)) 
                    << "Configure [" << name
                    << "] not in range (Sorry, type is not loggable)";
            }   
            return check_ok;
        };  
    }

    std::string tostring() const {
        std::string max_str = "";
        std::string min_str = "";
        bool cast_ok = pico_lexical_cast(_max, max_str);
        cast_ok = cast_ok && pico_lexical_cast(_min, min_str);
        return cast_ok ? (min_str + " <= * <= " + max_str) : std::string("unkown <= * <= unkown");
    }

private:
    U _min;
    U _max;
};

template<class U>
class RangeCheckerCO {
public:
    RangeCheckerCO(const U& min, const U& max) : _min(min), _max(max) {}

    std::function<bool(const U&, const std::string&)> checker() const {
        return inner_checker<U>();
    }

    template<class T>
    std::enable_if_t<IsGloggable<T>::value,
        std::function<bool(const T&, const std::string&)>> inner_checker() const {
        static_assert(IsLessEqualComparable<T>::value && IsLessComparable<T>::value,
                "type must be LessEqualComparable and LessComparable");
        return [=](const T& val, const std::string& name) -> bool {
            bool check_ok = _min <= val && val < _max;
            if (!check_ok) {
                ELOG(WARNING, PICO_CORE_ERRCODE(CFG_CHECKER)) 
                    << "Configure [" << name << "] value [" << val 
                    << "] not in range [" << _min << ", " << _max << ")";
            }   
            return check_ok;
        };  
    }

    template<class T>
    std::enable_if_t<!IsGloggable<T>::value,
        std::function<bool(const T&, const std::string&)>> inner_checker() const {
        static_assert(IsLessEqualComparable<T>::value && IsLessComparable<T>::value,
                "type must be LessEqualComparable and LessComparable");
        return [=](const T& val, const std::string& name) -> bool {
            bool check_ok = _min <= val && val < _max;
            if (!check_ok) {
                ELOG(WARNING, PICO_CORE_ERRCODE(CFG_CHECKER)) 
                    << "Configure [" << name
                    << "] not in range (Sorry, type is not loggable)";
            }   
            return check_ok;
        };  
    }

    std::string tostring() const {
        std::string max_str = "";
        std::string min_str = "";
        bool cast_ok = pico_lexical_cast(_max, max_str);
        cast_ok = cast_ok && pico_lexical_cast(_min, min_str);
        return cast_ok ? (min_str + " <= * < " + max_str) : std::string("unkown <= * < unkown");
    }

private:
    U _min;
    U _max;
};

template<class U>
class RangeCheckerOO {
public:
    RangeCheckerOO(const U& min, const U& max) : _min(min), _max(max) {}

    std::function<bool(const U&, const std::string&)> checker() const {
        return inner_checker<U>();
    }

    template<class T>
    std::enable_if_t<IsGloggable<T>::value,
        std::function<bool(const T&, const std::string&)>> inner_checker() const {
        static_assert(IsLessComparable<T>::value, "type must be LessComparable");
        return [=](const T& val, const std::string& name) -> bool {
            bool check_ok = _min < val && val < _max;
            if (!check_ok) {
                ELOG(WARNING, PICO_CORE_ERRCODE(CFG_CHECKER)) 
                    << "Configure [" << name << "] value [" << val 
                    << "] not in range (" << _min << ", " << _max << ")";
            }   
            return check_ok;
        };  
    }

    template<class T>
    std::enable_if_t<!IsGloggable<T>::value,
        std::function<bool(const T&, const std::string&)>> inner_checker() const {
        static_assert(IsLessComparable<T>::value, "type must be LessComparable");
        return [=](const T& val, const std::string& name) -> bool {
            bool check_ok = _min < val && val < _max;
            if (!check_ok) {
                ELOG(WARNING, PICO_CORE_ERRCODE(CFG_CHECKER)) 
                    << "Configure [" << name
                    << "] not in range (Sorry, type is not loggable)";
            }   
            return check_ok;
        };  
    }

    std::string tostring() const {
        std::string max_str = "";
        std::string min_str = "";
        bool cast_ok = pico_lexical_cast(_max, max_str);
        cast_ok = cast_ok && pico_lexical_cast(_min, min_str);
        return cast_ok ? (min_str + " < * < " + max_str) : std::string("unkown < * < unkown");
    }

private:
    U _min;
    U _max;
};

template<class U>
class RangeCheckerOC {
public:
    RangeCheckerOC(const U& min, const U& max) : _min(min), _max(max) {}

    std::function<bool(const U&, const std::string&)> checker() const {
        return inner_checker<U>();
    }

    template<class T>
    std::enable_if_t<IsGloggable<T>::value,
        std::function<bool(const T&, const std::string&)>> inner_checker() const {
        static_assert(IsLessEqualComparable<T>::value && IsLessComparable<T>::value,
                "type must be LessEqualComparable and LessComparable");
        return [=](const T& val, const std::string& name) -> bool {
            bool check_ok = _min < val && val <= _max;
            if (!check_ok) {
                ELOG(WARNING, PICO_CORE_ERRCODE(CFG_CHECKER)) 
                    << "Configure [" << name << "] value [" << val 
                    << "] not in range (" << _min << ", " << _max << "]";
            }
            return check_ok;
        };  
    }

    template<class T>
    std::enable_if_t<!IsGloggable<T>::value,
        std::function<bool(const T&, const std::string&)>> inner_checker() const {
        static_assert(IsLessEqualComparable<T>::value && IsLessComparable<T>::value,
                "type must be LessEqualComparable and LessComparable");
        return [=](const T& val, const std::string& name) -> bool {
            bool check_ok = _min < val && val <= _max;
            if (!check_ok) {
                ELOG(WARNING, PICO_CORE_ERRCODE(CFG_CHECKER)) 
                    << "Configure [" << name
                    << "] not in range (Sorry, type is not loggable)";
            }   
            return check_ok;
        };  
    }

    std::string tostring() const {
        std::string max_str = "";
        std::string min_str = "";
        bool cast_ok = pico_lexical_cast(_max, max_str);
        cast_ok = cast_ok && pico_lexical_cast(_min, min_str);
        return cast_ok ? (min_str + " < * <= " + max_str) : std::string("unkown < * <= unkown");
    }

private:
    U _min;
    U _max;
};

template<class U>
class GreaterChecker {
public:
    GreaterChecker(const U& min) : _min(min) {}

    std::function<bool(const U&, const std::string&)> checker() const {
        return inner_checker<U>();
    }

    template<class T>
    std::enable_if_t<IsGloggable<T>::value,
        std::function<bool(const T&, const std::string&)>> inner_checker() const {
        static_assert(IsLessComparable<T>::value, "type must be LessComparable");
        return [=](const T& val, const std::string& name) -> bool {
            bool check_ok = _min < val;
            if (!check_ok) {
                ELOG(WARNING, PICO_CORE_ERRCODE(CFG_CHECKER)) 
                    << "Configure [" << name << "] value [" << val 
                    << "] not greater than [" << _min << "]";
            }
            return check_ok;
        };  
    }

    template<class T>
    std::enable_if_t<!IsGloggable<T>::value,
        std::function<bool(const T&, const std::string&)>> inner_checker() const {
        static_assert(IsLessComparable<T>::value, "type must be LessComparable");
        return [=](const T& val, const std::string& name) -> bool {
            bool check_ok = _min < val;
            if (!check_ok) {
                ELOG(WARNING, PICO_CORE_ERRCODE(CFG_CHECKER)) 
                    << "Configure [" << name
                    << "] not greater than(Sorry, type is not loggable)";
            }
            return check_ok;
        };
    }

    std::string tostring() const {
        std::string min_str = "";
        bool cast_ok = pico_lexical_cast(_min, min_str);
        return cast_ok ? std::string(" * > " + min_str) : std::string(" * > unkown");
    }

private:
    U _min;
};

template<class U>
class GreaterEqualChecker {
public:
    GreaterEqualChecker(const U& min) : _min(min) {}

    std::function<bool(const U&, const std::string&)> checker() const {
        return inner_checker<U>();
    }

    template<class T>
    std::enable_if_t<IsGloggable<T>::value,
        std::function<bool(const T&, const std::string&)>> inner_checker() const {
        static_assert(IsLessComparable<T>::value, "type must be LessEqualComparable");
        return [=](const T& val, const std::string& name) -> bool {
            bool check_ok = _min <= val;
            if (!check_ok) {
                ELOG(WARNING, PICO_CORE_ERRCODE(CFG_CHECKER)) 
                    << "Configure [" << name << "] value [" << val 
                    << "] not greater equal than [" << _min << "]";
            }
            return check_ok;
        };  
    }

    template<class T>
    std::enable_if_t<!IsGloggable<T>::value,
        std::function<bool(const T&, const std::string&)>> inner_checker() const {
        static_assert(IsLessComparable<T>::value, "type must be LessEqualComparable");
        return [=](const T& val, const std::string& name) -> bool {
            bool check_ok = _min <= val;
            if (!check_ok) {
                ELOG(WARNING, PICO_CORE_ERRCODE(CFG_CHECKER)) 
                    << "Configure [" << name
                    << "] not greater equal than(Sorry, type is not loggable)";
            }
            return check_ok;
        };  
    }

    std::string tostring() const {
        std::string min_str = "";
        bool cast_ok = pico_lexical_cast(_min, min_str);
        return cast_ok ? (" * >= " + min_str) : std::string(" * >= unkown");
    }

private:
    U _min;
};

template<class U>
class LessChecker {
public:
    LessChecker(const U& max) : _max(max) {}

    std::function<bool(const U&, const std::string&)> checker() const {
        return inner_checker<U>();
    }

    template<class T>
    std::enable_if_t<IsGloggable<T>::value,
        std::function<bool(const T&, const std::string&)>> inner_checker() const {
        static_assert(IsLessComparable<T>::value, "type must be LessComparable");
        return [=](const T& val, const std::string& name) -> bool {
            bool check_ok = val < _max;
            if (!check_ok) {
                ELOG(WARNING, PICO_CORE_ERRCODE(CFG_CHECKER)) 
                    << "Configure [" << name << "] value [" << val 
                    << "] not less than [" << _max << "]";
            }
            return check_ok;
        };  
    }

    template<class T>
    std::enable_if_t<!IsGloggable<T>::value,
        std::function<bool(const T&, const std::string&)>> inner_checker() const {
        static_assert(IsLessComparable<T>::value, "type must be LessComparable");
        return [=](const T& val, const std::string& name) -> bool {
            bool check_ok = val < _max;
            if (!check_ok) {
                ELOG(WARNING, PICO_CORE_ERRCODE(CFG_CHECKER)) 
                    << "Configure [" << name
                    << "] not less than(Sorry, type is not loggable)";
            }
            return check_ok;
        };  
    }

    std::string tostring() const {
        std::string max_str = "";
        bool cast_ok = pico_lexical_cast(_max, max_str);
        return cast_ok ? (" * < " + max_str) : std::string(" * < unkown");
    }

private:
    U _max;
};

template<class U>
class LessEqualChecker {
public:
    LessEqualChecker(const U& max) : _max(max) {}

    std::function<bool(const U&, const std::string&)> checker() const {
        return inner_checker<U>();
    }

    template<class T>
    std::enable_if_t<IsGloggable<T>::value,
        std::function<bool(const T&, const std::string&)>> inner_checker() const {
        static_assert(IsLessComparable<T>::value, "type must be LessEqualComparable");
        return [=](const T& val, const std::string& name) -> bool {
            bool check_ok = val <= _max;
            if (!check_ok) {
                ELOG(WARNING, PICO_CORE_ERRCODE(CFG_CHECKER)) 
                    << "Configure [" << name << "] value [" << val 
                    << "] not less equal than [" << _max << "]";
            }
            return check_ok;
        };  
    }

    template<class T>
    std::enable_if_t<!IsGloggable<T>::value,
        std::function<bool(const T&, const std::string&)>> inner_checker() const {
        static_assert(IsLessComparable<T>::value, "type must be LessEqualComparable");
        return [=](const T& val, const std::string& name) -> bool {
            bool check_ok = val <= _max;
            if (!check_ok) {
                ELOG(WARNING, PICO_CORE_ERRCODE(CFG_CHECKER)) 
                    << "Configure [" << name
                    << "] not less equal than(Sorry, type is not loggable)";
            }
            return check_ok;
        };  
    }

    std::string tostring() const {
        std::string max_str = "";
        bool cast_ok = pico_lexical_cast(_max, max_str);
        return cast_ok ? (" * <= " + max_str) : std::string(" * <= unkown");
    }

private:
    U _max;
};

template<class U>
class EqualChecker {
public:
    EqualChecker(const U& deftval) : _deftval(deftval) {}

    std::function<bool(const U&, const std::string&)> checker() const {
        return inner_checker<U>();
    }

    template<class T>
    std::enable_if_t<IsGloggable<T>::value,
        std::function<bool(const T&, const std::string&)>> inner_checker() const {
        static_assert(IsEqualComparable<T>::value, "type must be EqualComparable");
        return [=](const T& val, const std::string& name) -> bool {
            bool check_ok = val == _deftval;
            if (!check_ok) {
                ELOG(WARNING, PICO_CORE_ERRCODE(CFG_CHECKER)) 
                    << "Configure [" << name << "] value [" << val 
                    << "] not equal to [" << _deftval << "]";
            }
            return check_ok;
        };
    }

    template<class T>
    std::enable_if_t<!IsGloggable<T>::value,
        std::function<bool(const T&, const std::string&)>> inner_checker() const {
        static_assert(IsEqualComparable<T>::value, "type must be EqualComparable");
        return [=](const T& val, const std::string& name) -> bool {
            bool check_ok = val == _deftval;
            if (!check_ok) {
                ELOG(WARNING, PICO_CORE_ERRCODE(CFG_CHECKER)) 
                    << "Configure [" << name
                    << "] not equal to(Sorry, type is not loggable)";
            }
            return check_ok;
        };
    }

    std::string tostring() const {
        std::string deftval_str = "";
        bool cast_ok = pico_lexical_cast(_deftval, deftval_str);
        return cast_ok ? (" * == " + deftval_str) : std::string(" * == unkown");
    }

private:
    U _deftval;
};

template<class U>
class NotEqualChecker {
public:
    NotEqualChecker(const U& invalid) : _invalid(invalid) {}

    std::function<bool(const U&, const std::string&)> checker() const {
        return inner_checker<U>();
    }

    template<class T>
    std::enable_if_t<IsGloggable<T>::value,
        std::function<bool(const T&, const std::string&)>> inner_checker() const {
        static_assert(IsEqualComparable<T>::value, "type must be EqualComparable");
        return [=](const T& val, const std::string& name) -> bool {
            bool check_ok = !(val == _invalid);
            if (!check_ok) {
                ELOG(WARNING, PICO_CORE_ERRCODE(CFG_CHECKER)) 
                    << "Configure [" << name << "] value [" << val 
                    << "] equals to invalid [" << _invalid << "]";
            }
            return check_ok;
        };
    }

    template<class T>
    std::enable_if_t<!IsGloggable<T>::value,
        std::function<bool(const T&, const std::string&)>> inner_checker() const {
        static_assert(IsEqualComparable<T>::value, "type must be EqualComparable");
        return [=](const T& val, const std::string& name) -> bool {
            bool check_ok = !(val == _invalid);
            if (!check_ok) {
                ELOG(WARNING, PICO_CORE_ERRCODE(CFG_CHECKER)) 
                    << "Configure [" << name
                    << "] not equal to(Sorry, type is not loggable)";
            }
            return check_ok;
        };
    }

    std::string tostring() const {
        std::string invalid_str = "";
        bool cast_ok = pico_lexical_cast(_invalid, invalid_str);
        return cast_ok ? (" * != " + invalid_str) : std::string(" * != unkown");
    }

private:
    U _invalid;
};

template<class T>
class EnumChecker {
public:
    EnumChecker(const std::unordered_set<T>& valid_values) : _valid_values(valid_values) {}

    std::function<bool(const T&, const std::string&)> checker() const {
        std::string inner_checker_str = tostring();
        return [=](const T& val, const std::string& name) -> bool {
            bool check_ok = _valid_values.find(val) != _valid_values.end();
            if (!check_ok) {
                std::string val_str = "";
                bool cast_flag = pico_lexical_cast(val, val_str);
                ELOG(WARNING, PICO_CORE_ERRCODE(CFG_CHECKER)) 
                    << "Configure [" << name << "] value ["
                    << (cast_flag ? val_str : std::string("(not castable)")) << "] not "
                    << inner_checker_str;
            }
            return check_ok;
        };
    }

    std::string tostring() const {
        std::string valid_str = " in { ";
        std::string tmp_str = "";
        bool cast_flag = true;
        for (const auto& elm : _valid_values) {
            cast_flag = cast_flag && pico_lexical_cast(elm, tmp_str);
            if (cast_flag) {
                valid_str += std::string("[") + tmp_str + "] ";
            }
        }
        valid_str += "}";
        return cast_flag ? valid_str : std::string(" in {not castable}");
    }

private:
    std::unordered_set<T> _valid_values;
};

class RegexChecker {
public:
    RegexChecker(const std::string& regex_str,
            boost::match_flag_type flags = boost::regex_constants::match_default) {
        _regex = boost::regex(regex_str);
        _regex_flags = flags;
        _regex_string = regex_str;
    }

    std::function<bool(const std::string&, const std::string&)> checker() const {
        return [=](const std::string& val, const std::string& name) -> bool {
            bool check_ok = true;
            try {
                check_ok = regex_match(val, _regex, _regex_flags);
                if (!check_ok) {
                    ELOG(WARNING, PICO_CORE_ERRCODE(CFG_CHECKER)) 
                        << "Configure [" << name << "] value [" << val
                        << "] not regex match [\"" << _regex_string << "\"]";
                }
            } catch (boost::regex_error& e) {
                SLOG(WARNING) << "boost regex exception: "<< e.what();
                check_ok = false;
            } catch (std::exception& e) {
                SLOG(WARNING) << "std exception: "<< e.what();
                check_ok = false;
            } catch (...) {
                SLOG(WARNING) << "unkown exception.";
                check_ok = false;
            }

            return check_ok;
        };
    }

    std::string tostring() const {
        return std::string("regex [\"") + _regex_string + "\"]";
    }

private:
    boost::regex _regex;
    std::string _regex_string;
    boost::match_flag_type _regex_flags;
};

template<class CHK, class T>
class ListNodeChecker {
public:
    ListNodeChecker(const CHK& checker): _checker(checker){}

    template <typename...ARGS>
    ListNodeChecker(ARGS&&... args): _checker(std::forward<ARGS>(args)...){}

    std::function<bool(const ListNode<T>&, const std::string&)> checker() const {
        return [=](const ListNode<T>& val, const std::string& name) -> bool {
            auto checker = _checker.checker();
            for (size_t i = 0; i < val.size(); ++i) {
                std::string namei = name + "[" + std::to_string(i) + "]";
                if (!checker(val[i], namei)) {
                    return false;
                }
            }
            return true;
        };
    }

    std::string tostring() const {
        return "for each: " + _checker.tostring();
    }

private:
    CHK _checker;
};

template<class CHK, class T>
class ListSizeChecker {
public:
    ListSizeChecker(const CHK& checker): _checker(checker){}

    template <typename... ARGS>
    ListSizeChecker(ARGS&&... args): _checker(std::forward<ARGS>(args)...){}

    std::function<bool(const ListNode<T>&, const std::string&)> checker() const {
        return [=](const ListNode<T>& val, const std::string& name) -> bool {
            auto checker = _checker.checker();
            std::string namei = name + ".size";
                if (!checker(val.size(), namei)) {
                    return false;
                }
            return true;
        };
    }

    std::string tostring() const {
        return "size: " + _checker.tostring();
    }

private:
    CHK _checker;
};


template<class CHK, class T>
class NonEmptyListNodeChecker {
public:
    NonEmptyListNodeChecker(const CHK& checker): _checker(checker) {}

    template <typename...ARGS>
    NonEmptyListNodeChecker(ARGS&&... args): _checker(std::forward<ARGS>(args)...){}

    std::function<bool(const ListNode<T>&, const std::string&)> checker() const {
        return [=](const ListNode<T>& val, const std::string& name) -> bool {
            if (val.size() == 0) {
                ELOG(WARNING, PICO_CORE_ERRCODE(CFG_CHECKER)) 
                    << "list node [" << name << "] must be non-empty.";
                return false;
            }
            auto checker = _checker.checker();
            for (size_t i = 0; i < val.size(); ++i) {
                std::string namei = name + "[" + std::to_string(i) + "]";
                if (!checker(val[i], namei)) {
                    return false;
                }
            }
            return true;
        };
    }

    std::string tostring() const {
        return "for each: " + _checker.tostring();
    }

private:
    CHK _checker;
};

typedef ListNodeChecker<RegexChecker, std::string> ListRegexChecker;

typedef NonEmptyListNodeChecker<RegexChecker, std::string> NonEmptyListRegexChecker;

template <typename T>
using NonEmptyListChecker=NonEmptyListNodeChecker<DefaultChecker<T>, T>;

template <typename T>
using DefaultListChecker=ListNodeChecker<DefaultChecker<T>, T>;

template <typename T>
using ListRangeCheckerCC=ListNodeChecker<RangeCheckerCC<T>,T>;

template <typename T>
using ListSizeEqualChecker=ListSizeChecker<EqualChecker<size_t>,T>;

template <typename T>
using ListSizeNotEqualChecker=ListSizeChecker<NotEqualChecker<size_t>,T>;

template <typename T>
using ListSizeGreaterChecker=ListSizeChecker<GreaterChecker<size_t>,T>;

template <typename T>
using ListSizeGreaterEqualChecker=ListSizeChecker<GreaterEqualChecker<size_t>,T>;

template <typename T>
using ListSizeLessChecker=ListSizeChecker<LessChecker<size_t>,T>;

template <typename T>
using ListSizeLessEqualChecker=ListSizeChecker<LessEqualChecker<size_t>,T>;

template <typename T>
using ListEnumChecker=ListNodeChecker<EnumChecker<T>,T>;

}
} // namespace pico
} // namespace paradigm4

#endif // PARADIGM4_PICO_COMMON_INCLUDE_CONFIGURE_CHECKERS_H
