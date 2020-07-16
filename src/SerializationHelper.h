#ifndef PARADIGM4_PICO_PICO_CORE_SERIALIZATION_HELPER_H
#define PARADIGM4_PICO_PICO_CORE_SERIALIZATION_HELPER_H

#include <pico-core/Archive.h>
#include "pico-core/SerializationAvroHelper.h"
namespace paradigm4 {
namespace pico {
namespace core {

#define _AVRO_enc(a, i) paradigm4::pico::core::avro_traits<decltype(v.a)>::encode(e, v.a);

#define _AVRO_dec(a, i) paradigm4::pico::core::avro_traits<decltype(v.a)>::decode(d, v.a);

#define _AVRO_add_field(a, i) builder.add_field<decltype(std::declval<Type>().a)>(#a);

#define _AVRO_resolve_dec(a, i)                     \
    case i:                                         \
        paradigm4::pico::core::avro_traits<decltype(v.a)>::decode(d, v.a); \
        break;

#define PICO_SERIAL(type, args...)                                        \
    struct avro_traits<type> {                                            \
        typedef type Type;                                                \
        static void encode(avro::Encoder& e, const Type& v) {             \
            PICO_FOREACH(_AVRO_enc, args)                                 \
        }                                                                 \
        static void decode(avro::Decoder& d, Type& v) {                   \
            if (avro::ResolvingDecoder* rd =                              \
                    dynamic_cast<avro::ResolvingDecoder*>(&d)) {          \
                const std::vector<size_t> fo = rd->fieldOrder();          \
                for (std::vector<size_t>::const_iterator it = fo.begin(); \
                     it != fo.end(); ++it) {                              \
                    switch (*it) {                                        \
                        PICO_FOREACH(_AVRO_resolve_dec, args)             \
                        default:                                          \
                            break;                                        \
                    }                                                     \
                }                                                         \
            } else {                                                      \
                PICO_FOREACH(_AVRO_dec, args)                             \
            }                                                             \
        }                                                                 \
        static std::string json_schema() {                                \
            using namespace paradigm4::pico::core;                          \
            AvroSchemaBuilder builder(avro_name<Type>());                 \
            PICO_FOREACH(_AVRO_add_field, args)                           \
            return builder.str();                                         \
        }                                                                 \
    }

#define PICO_SERIALIZE(args...)                                           \
    template <typename Type>                                              \
    struct _avro_traits {                                                 \
        static void encode(avro::Encoder& e, const Type& v) {             \
            PICO_FOREACH(_AVRO_enc, args)                                 \
        }                                                                 \
        static void decode(avro::Decoder& d, Type& v) {                   \
            if (avro::ResolvingDecoder* rd =                              \
                    dynamic_cast<avro::ResolvingDecoder*>(&d)) {          \
                const std::vector<size_t> fo = rd->fieldOrder();          \
                for (std::vector<size_t>::const_iterator it = fo.begin(); \
                     it != fo.end(); ++it) {                              \
                    switch (*it) {                                        \
                        PICO_FOREACH(_AVRO_resolve_dec, args)             \
                        default:                                          \
                            break;                                        \
                    }                                                     \
                }                                                         \
            } else {                                                      \
                PICO_FOREACH(_AVRO_dec, args)                             \
            }                                                             \
        }                                                                 \
        static std::string json_schema() {                                \
            using namespace paradigm4::pico::core;                          \
            AvroSchemaBuilder builder(avro_name<Type>());                 \
            PICO_FOREACH(_AVRO_add_field, args)                           \
            return builder.str();                                         \
        }                                                                 \
    };                                                                    \
    friend struct ::paradigm4::pico::core::avro_serial_helper;              \
    PICO_SERIALIZATION(args)

#define PICO_SERIALIZE_EMPTY()                             \
    template <typename Type>                               \
    struct _avro_traits {                                  \
        static void encode(avro::Encoder&, const Type&) {} \
        static void decode(avro::Decoder&, Type&) {}       \
        static std::string json_schema() {                 \
            using namespace paradigm4::pico::core;           \
            AvroSchemaBuilder builder(avro_name<Type>());  \
            return builder.str();                          \
        }                                                  \
    };                                                     \
    friend struct ::paradigm4::pico::core::avro_serial_helper;   \
    PICO_SERIALIZATION()

#define DEF_SERIAL_FRIEND                          \
    template <typename __Ttype, typename __Enable> \
    friend struct pico_avro::avro_traits;

}
} // namespace pico
} // namespace paradigm4

#endif // PARADIGM4_PICO_COMMON_SERIALIZATION_HELPER_H
