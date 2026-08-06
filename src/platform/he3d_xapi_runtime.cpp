#include "he3d.hpp"

static void *he3d_alloc_or_trap(__SIZE_TYPE__ size)
{
   void *ptr = HE3D::Alloc((HE3D::uint64_t)size);
   if (!ptr) {
      __builtin_trap();
   }
   return ptr;
}

void *operator new(__SIZE_TYPE__ size) { return he3d_alloc_or_trap(size); }
void *operator new[](__SIZE_TYPE__ size) { return he3d_alloc_or_trap(size); }
void operator delete(void *ptr) noexcept { HE3D::Free(ptr); }
void operator delete[](void *ptr) noexcept { HE3D::Free(ptr); }
void operator delete(void *ptr, __SIZE_TYPE__) noexcept { HE3D::Free(ptr); }
void operator delete[](void *ptr, __SIZE_TYPE__) noexcept { HE3D::Free(ptr); }
