#ifndef PARADIGM4_PICO_CORE_VIRTUAL_OBJECT_H
#define PARADIGM4_PICO_CORE_VIRTUAL_OBJECT_H

namespace paradigm4 {
namespace pico {
namespace core {
/*!
 * \brief defination of NoncopyableObject
 */

class Object {
public:
    virtual ~Object() = default;
};

class NoncopyableObject : public Object {
public:
    NoncopyableObject() = default;
    NoncopyableObject(const NoncopyableObject&) = delete;
    NoncopyableObject& operator=(const NoncopyableObject&) = delete;
};

class VirtualObject : public NoncopyableObject {
};

} // namespace core
} // namespace pico
} // namespace paradigm4
#endif // PARADIGM4_PICO_CORE_VIRTUAL_OBJECT_H
