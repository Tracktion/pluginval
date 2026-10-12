/*==============================================================================

  Copyright 2018 by Tracktion Corporation.
  For more information visit www.tracktion.com

   You may also use this code under the terms of the GPL v3 (see
   www.gnu.org/licenses).

   pluginval IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL WARRANTIES, WHETHER
   EXPRESSED OR IMPLIED, INCLUDING MERCHANTABILITY AND FITNESS FOR PURPOSE, ARE
   DISCLAIMED.

 ==============================================================================*/

#include <future>
#include "TestUtilities.h"

inline bool logAllocationViolationIfNotAllowed()
{
    auto& ai = getAllocatorInterceptor();

    if (! ai.isAllowedToAllocate())
    {
        ai.logAllocationViolation();
        return false;
    }

    return true;
}

inline void violationError (const std::string& text)
{
    switch (AllocatorInterceptor::getViolationBehaviour())
    {
        case AllocatorInterceptor::ViolationBehaviour::logToCerr:
            std::cerr << text;
            break;
        case AllocatorInterceptor::ViolationBehaviour::throwException:
            throw;
            break;
        case AllocatorInterceptor::ViolationBehaviour::none:
        default:
            break;
    }
}

inline bool throwIfRequiredAndReturnShouldLog()
{
    switch (AllocatorInterceptor::getViolationBehaviour())
    {
        case AllocatorInterceptor::ViolationBehaviour::logToCerr:
            return true;
        case AllocatorInterceptor::ViolationBehaviour::throwException:
            throw std::bad_alloc();
        case AllocatorInterceptor::ViolationBehaviour::none:
        default:
            break;
    }

    return false;
}

//==============================================================================
#if JUCE_CLANG
 #define ATTRIBUTE_USED __attribute__((used))
#else
 #define ATTRIBUTE_USED
#endif

ATTRIBUTE_USED void* operator new (std::size_t sz)
{
    if (! logAllocationViolationIfNotAllowed())
        if (throwIfRequiredAndReturnShouldLog())
            std::cerr << "!!! WARNING: Illegal allocation of " << sz << " bytes\n";

    return std::malloc (sz);
}

ATTRIBUTE_USED void* operator new[] (std::size_t sz)
{
    if (! logAllocationViolationIfNotAllowed())
        if (throwIfRequiredAndReturnShouldLog())
            std::cerr << "!!! WARNING: Illegal array allocation of " << sz << " bytes\n";

    return std::malloc (sz);
}

ATTRIBUTE_USED void operator delete (void* ptr) noexcept
{
    if (! logAllocationViolationIfNotAllowed())
        if (throwIfRequiredAndReturnShouldLog())
            std::cerr << "!!! WARNING: Illegal deletion\n";

    std::free (ptr);
}

ATTRIBUTE_USED void operator delete[] (void* ptr) noexcept
{
    if (! logAllocationViolationIfNotAllowed())
        if (throwIfRequiredAndReturnShouldLog())
            std::cerr << "!!! WARNING: Illegal array deletion\n";

    std::free (ptr);
}

#if JUCE_CXX14_IS_AVAILABLE
void operator delete (void* ptr, size_t) noexcept
{
    if (! logAllocationViolationIfNotAllowed())
        if (throwIfRequiredAndReturnShouldLog())
            std::cerr << "!!! WARNING: Illegal deletion\n";

    std::free (ptr);
}

void operator delete[] (void* ptr, size_t) noexcept
{
    if (! logAllocationViolationIfNotAllowed())
        if (throwIfRequiredAndReturnShouldLog())
            std::cerr << "!!! WARNING: Illegal array deletion\n";

    std::free (ptr);
}
#endif

//==============================================================================
std::atomic<AllocatorInterceptor::ViolationBehaviour> AllocatorInterceptor::violationBehaviour (ViolationBehaviour::logToCerr);

void AllocatorInterceptor::setViolationBehaviour (ViolationBehaviour behaviour) noexcept
{
    violationBehaviour.store (behaviour);
}

AllocatorInterceptor::ViolationBehaviour AllocatorInterceptor::getViolationBehaviour() noexcept
{
    return violationBehaviour.load();
}

AllocatorInterceptor& getAllocatorInterceptor()
{
    thread_local AllocatorInterceptor ai;
    return ai;
}

//==============================================================================
ScopedAllocationDisabler::ScopedAllocationDisabler()    { getAllocatorInterceptor().disableAllocations(); }
ScopedAllocationDisabler::~ScopedAllocationDisabler()   { getAllocatorInterceptor().enableAllocations(); }

//==============================================================================
struct AllocatorInterceptorTests    : public juce::UnitTest,
                                      private juce::AsyncUpdater
{
    AllocatorInterceptorTests()
        : juce::UnitTest ("AllocatorInterceptorTests", "pluginval")
    {
    }

    void runTest() override
    {
        AllocatorInterceptor::setViolationBehaviour (AllocatorInterceptor::ViolationBehaviour::none);
        auto& allocatorInterceptor = getAllocatorInterceptor();

        beginTest ("Ensure separate allocators are used for threads");
        {
            AllocatorInterceptor* ai1 = &getAllocatorInterceptor();
            AllocatorInterceptor *ai2 = nullptr;

            auto task = std::async (std::launch::async, [&ai2] { ai2 = &getAllocatorInterceptor(); });

            task.get();

            expect (ai1 != ai2);
        }

        beginTest ("Ensure all allocations pass");
        {
            performAllocations();
            expect (! allocatorInterceptor.getAndClearAllocationViolation());
            expectEquals (allocatorInterceptor.getAndClearNumAllocationViolations(), 0);
        }

        beginTest ("Ensure all allocations fail on MessageThread");
        {
            {
                ScopedAllocationDisabler sad;
                performAllocations();
            }

            expect (allocatorInterceptor.getAndClearAllocationViolation());
            expectGreaterThan (allocatorInterceptor.getAndClearNumAllocationViolations(), 0);
        }

        beginTest ("Ensure allocation violation status is reset");
        {
            expect (! allocatorInterceptor.getAndClearAllocationViolation());
            expectEquals (allocatorInterceptor.getAndClearNumAllocationViolations(), 0);
        }

        beginTest ("Ensure all allocations pass on MessageThread, fail on background thread");
        {
            auto task = std::async (std::launch::async, [this]
                                    {
                                        ScopedAllocationDisabler sad;
                                        performAllocations();
                                        expect (getAllocatorInterceptor().getAndClearAllocationViolation());
                                        expectGreaterThan (getAllocatorInterceptor().getAndClearNumAllocationViolations(), 0);
                                    });
            task.get();

            expect (! allocatorInterceptor.getAndClearAllocationViolation());
            expectEquals (allocatorInterceptor.getAndClearNumAllocationViolations(), 0);

            performAllocations();
            expect (! allocatorInterceptor.getAndClearAllocationViolation());
            expectEquals (allocatorInterceptor.getAndClearNumAllocationViolations(), 0);
        }

        beginTest ("Ensure array allocations are caught");
        {
            expect (! allocatorInterceptor.getAndClearAllocationViolation());
            expectEquals (getAllocatorInterceptor().getAndClearNumAllocationViolations(), 0);

            {
                ScopedAllocationDisabler sad;
                std::vector<int> ints (42);
                ints.resize (100);
            }

            expect (allocatorInterceptor.getAndClearAllocationViolation());
            expectGreaterThan (getAllocatorInterceptor().getAndClearNumAllocationViolations(), 0);
        }

        beginTest ("Ensure allocations are thrown");
        {
            AllocatorInterceptor::setViolationBehaviour (AllocatorInterceptor::ViolationBehaviour::throwException);
            ScopedAllocationDisabler sad;
            expectThrows (std::vector<int> (42));
            AllocatorInterceptor::setViolationBehaviour (AllocatorInterceptor::ViolationBehaviour::logToCerr);
        }
    }

private:
    void performAllocations()
    {
        juce::String s ("Hello world");
        std::vector<float> floats (42);
        triggerAsyncUpdate();

        ignoreUnused (s, floats);
    }

    void handleAsyncUpdate() override {}
};

static AllocatorInterceptorTests allocatorInterceptorTests;
