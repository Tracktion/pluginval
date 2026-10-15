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

/** The deallocation version: operator delete is noexcept, so a throw from it
    would terminate the process. Violations are still counted, just not thrown.
*/
inline bool shouldLogDeallocationViolation()
{
    return AllocatorInterceptor::getViolationBehaviour() == AllocatorInterceptor::ViolationBehaviour::logToCerr;
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

    // operator new must never return null (malloc (0) may)
    if (auto ptr = std::malloc (sz > 0 ? sz : 1))
        return ptr;

    throw std::bad_alloc();
}

ATTRIBUTE_USED void* operator new[] (std::size_t sz)
{
    if (! logAllocationViolationIfNotAllowed())
        if (throwIfRequiredAndReturnShouldLog())
            std::cerr << "!!! WARNING: Illegal array allocation of " << sz << " bytes\n";

    // operator new must never return null (malloc (0) may)
    if (auto ptr = std::malloc (sz > 0 ? sz : 1))
        return ptr;

    throw std::bad_alloc();
}

ATTRIBUTE_USED void operator delete (void* ptr) noexcept
{
    if (! logAllocationViolationIfNotAllowed())
        if (shouldLogDeallocationViolation())
            std::cerr << "!!! WARNING: Illegal deletion\n";

    std::free (ptr);
}

ATTRIBUTE_USED void operator delete[] (void* ptr) noexcept
{
    if (! logAllocationViolationIfNotAllowed())
        if (shouldLogDeallocationViolation())
            std::cerr << "!!! WARNING: Illegal array deletion\n";

    std::free (ptr);
}

#if JUCE_CXX14_IS_AVAILABLE
void operator delete (void* ptr, size_t) noexcept
{
    if (! logAllocationViolationIfNotAllowed())
        if (shouldLogDeallocationViolation())
            std::cerr << "!!! WARNING: Illegal deletion\n";

    std::free (ptr);
}

void operator delete[] (void* ptr, size_t) noexcept
{
    if (! logAllocationViolationIfNotAllowed())
        if (shouldLogDeallocationViolation())
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


//==============================================================================
struct ChildProcessOutputDecoderTests  : public juce::UnitTest
{
    ChildProcessOutputDecoderTests()
        : juce::UnitTest ("ChildProcessOutputDecoder", "pluginval")
    {
    }

    void runTest() override
    {
        beginTest ("Complete text passes straight through");
        {
            ChildProcessOutputDecoder decoder;
            expectEquals (juce::String (decoder.decode ("abc", 3)), juce::String ("abc"));
        }

        beginTest ("A character split across reads is held back until complete");
        {
            const std::string text = "a\xe2\x82\xac" "b"; // "a€b", the euro sign is 3 bytes

            for (size_t split = 1; split < text.size(); ++split)
            {
                ChildProcessOutputDecoder decoder;
                const auto first = decoder.decode (text.data(), split);
                const auto second = decoder.decode (text.data() + split, text.size() - split);

                expect (first == text.substr (0, split < 4 ? 1 : split));
                expect (first + second == text);
            }
        }

        beginTest ("Invalid bytes aren't held back");
        {
            ChildProcessOutputDecoder decoder;
            expect (decoder.decode ("a\xff", 2) == "a\xff");
        }
    }
};

static ChildProcessOutputDecoderTests childProcessOutputDecoderTests;
