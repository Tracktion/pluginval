/*==============================================================================

  Copyright 2018 by Tracktion Corporation.
  For more information visit www.tracktion.com

   You may also use this code under the terms of the GPL v3 (see
   www.gnu.org/licenses).

   pluginval IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL WARRANTIES, WHETHER
   EXPRESSED OR IMPLIED, INCLUDING MERCHANTABILITY AND FITNESS FOR PURPOSE, ARE
   DISCLAIMED.

 ==============================================================================*/

#pragma once

#if PLUGINVAL_ENABLE_RTCHECK
    #include <rtcheck.h>
    #define RTC_REALTIME_CONTEXT rtc::realtime_context rc##__LINE__; rtc::disable_checks_for_thread (static_cast<uint64_t>(rtc::check_flags::pthread_mutex_lock) | static_cast<uint64_t>(rtc::check_flags::pthread_mutex_unlock));

    #define RTC_REALTIME_CONTEXT_IF_LEVEL_10(level) \
      std::optional<rtc::realtime_context> rc;      \
                                                    \
      if (level >= 10)                              \
      {                                             \
          rc.emplace();                             \
          rtc::disable_checks_for_thread (static_cast<uint64_t>(rtc::check_flags::pthread_mutex_lock) \
                                          | static_cast<uint64_t>(rtc::check_flags::pthread_mutex_unlock)); \
      }

    #define RTC_REALTIME_CONTEXT_IF_ENABLED(realtimeCheckMode, blockNum)                                        \
      std::optional<rtc::realtime_context> rc;                                                                  \
                                                                                                                \
      if (realtimeCheckMode != RealtimeCheck::disabled)                                                         \
      {                                                                                                         \
          if (realtimeCheckMode != RealtimeCheck::relaxed || blockNum > 0)                                      \
          {                                                                                                     \
              rc.emplace();                                                                                     \
              rtc::disable_checks_for_thread (static_cast<uint64_t>(rtc::check_flags::pthread_mutex_lock)       \
                                              | static_cast<uint64_t>(rtc::check_flags::pthread_mutex_unlock)); \
          }                                                                                                     \
      }
#else
    #define RTC_REALTIME_CONTEXT_IF_ENABLED(realtimeCheckMode, blockNum)    \
        (void) realtimeCheckMode;                                           \
        (void) blockNum;
#endif
