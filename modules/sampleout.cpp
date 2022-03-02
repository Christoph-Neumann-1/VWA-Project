// Header:

#include <cstdint>
#include <cstdlib>
namespace Sample
{

    struct Vec2;

    struct __attribute__((packed)) Vec2
    {
        double x;
        double y;
    };

    void *m_malloc(std::uint64_t size);
    void m_free(void *ptr);
}
// Implementation(may be split into multiple files by user):
namespace Sample
{
    void *m_malloc(std::uint64_t size)
    {
    }

    void m_free(void *ptr)
    {
    }
}

// I am not going to handle function pointers here, just dissallow passing them.
// If you REALLY want them use the std struct and tell the vm to do something.

// Example of imported function
namespace otherModule
{
    void (*foo)(); // Later loaded
}

// Wrapper functions are somewhere in a lambda. They need not be visible to anyone.

// Linker stuff. Copy from old file