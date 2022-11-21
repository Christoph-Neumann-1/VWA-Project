#define MODULE_IMPL
#include "data.gen.hpp"

using namespace vwa;
// TODO: geometric growth

namespace data
{
    Vector vecInit(vm_int typeSize)
    {
        return Vector{nullptr, nullptr, nullptr, typeSize};
    }
    vm_int vecResize(Vector *vec, vm_int size)
    {
        if (size < 0)
            return -1;
        auto current = vecSize(*vec);
        if (current == size * vec->tSize)
            return 0;
        if (current > size * vec->tSize)
        {
            auto ret = vecReserve(vec, size);
            if (ret)
                return ret;
        }
        vec->used = static_cast<std::byte *>(vec->start) + size * vec->tSize;
        return 0;
    }
    vm_int vecReserve(Vector *vec, vm_int size)
    {
        if (size < 0)
            return -1;
        if (!size)
        {
            core::free(vec->start);
            vec->start = vec->end = vec->used = nullptr;
            return 0;
        }
        if (size <= vecSize(*vec))
            return -1;
        auto newStart = core::realloc(vec->start, size * vec->tSize);
        if (!newStart)
            return -1;
        vec->end = static_cast<std::byte *>(vec->end) - static_cast<std::byte *>(vec->start) + static_cast<std::byte *>(newStart);
        vec->used = static_cast<std::byte *>(vec->used) - static_cast<std::byte *>(vec->start) + static_cast<std::byte *>(newStart);
        vec->start = newStart;
        return 0;
    }
    vm_int vecPush(Vector *vec, void *data)
    {
        auto size = vecSize(*vec);
        auto capacity = vecCapacity(*vec);
        if (size >= capacity)
        {
            auto ret = vecReserve(vec, size + 1);
            if (ret)
                return ret;
        }
        vec->used = static_cast<std::byte *>(memcpy(vec->used, data, vec->tSize)) + vec->tSize;
        return 0;
    }
    vm_int vecSize(Vector vec)
    {
        return static_cast<std::byte *>(vec.used) - static_cast<std::byte *>(vec.start);
    }
    vm_int vecCapacity(Vector vec)
    {
        return static_cast<std::byte *>(vec.end) - static_cast<std::byte *>(vec.start);
    }
    void *vecAt(Vector vec, vm_int pos)
    {
        if (pos >= vecSize(vec))
            return nullptr;
        return static_cast<std::byte *>(vec.start) + vec.tSize * pos;
    }
    void vecDel(Vector *vec)
    {
        core::free(vec->start);
        memset(vec, 0, sizeof(Vector));
    }
    void vecShrink(Vector *vec)
    {
        vecReserve(vec, vecSize(*vec));
    }
    void *arrAt(Array arr, vm_int pos)
    {
        if (pos > arr.size)
            return nullptr;
        return static_cast<std::byte *>(arr.start) + pos * arr.tSize;
    }
    Array arrInit(vm_int typeSize, vm_int n)
    {
        return {core::calloc(n, typeSize), n, typeSize};
    }
    void arrDel(Array* arr)
    {
        core::free(arr->start);
        memset(arr, 0, sizeof(Array));
    }
} // namespace data
