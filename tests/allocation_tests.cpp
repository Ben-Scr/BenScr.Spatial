#include <benscr/spatial/point_index.hpp>

#include <cstdlib>
#include <iostream>
#include <new>
#include <vector>

namespace
{
// This executable is single-threaded. Only an operation under test enables failures.
int failAfter = -1;
} // namespace

void *operator new(std::size_t size)
{
    if (failAfter == 0)
    {
        throw std::bad_alloc();
    }
    if (failAfter > 0)
    {
        --failAfter;
    }
    if (void *memory = std::malloc(size == 0 ? 1 : size))
    {
        return memory;
    }
    throw std::bad_alloc();
}
void operator delete(void *memory) noexcept
{
    std::free(memory);
}
void operator delete(void *memory, std::size_t) noexcept
{
    std::free(memory);
}

int main()
{
    namespace Spatial = BenScr::Spatial;
    for (int operation = 0; operation < 3; ++operation)
    {
        bool sawFailure = false;
        bool sawSuccess = false;
        for (int allocation = 0; allocation < 40; ++allocation)
        {
            Spatial::PointIndex index, source(2);
            if (!index.Insert(1, {0, 0}) || !source.Insert(9, {2, 2}))
            {
                return 1;
            }
            bool failed = false;
            failAfter = allocation;
            try
            {
                if (operation == 0)
                {
                    (void)index.Insert(2, {100, 100});
                }
                if (operation == 1)
                {
                    (void)index.Update(1, {100, 100});
                }
                if (operation == 2)
                {
                    index = source;
                }
            }
            catch (const std::bad_alloc &)
            {
                failed = true;
            }
            failAfter = -1;
            sawFailure |= failed;
            sawSuccess |= !failed;
            const auto values = index.QueryRectangle({-1000, -1000}, {1000, 1000});
            if (failed)
            {
                if (values != std::vector<Spatial::Entry>{{1, {0, 0}}} || index.Count() != 1 ||
                    index.CellSize() != 1)
                {
                    return 2;
                }
                if (!index.Update(1, {3, 3}) || !index.Remove(1) || index.Count() != 0)
                {
                    return 3;
                }
            }
            else
            {
                if (operation == 0 &&
                    values != std::vector<Spatial::Entry>{{1, {0, 0}}, {2, {100, 100}}})
                {
                    return 4;
                }
                if (operation == 1 && values != std::vector<Spatial::Entry>{{1, {100, 100}}})
                {
                    return 5;
                }
                if (operation == 2 && values != std::vector<Spatial::Entry>{{9, {2, 2}}})
                {
                    return 6;
                }
            }
        }
        if (!sawFailure || !sawSuccess)
        {
            return 7;
        }
    }
    std::cout << "Insert, update and copy-assignment allocation rollback passed.\n";
}
