/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef NET_DCSCTP_TIMER_TIMER_H_
#define NET_DCSCTP_TIMER_TIMER_H_

#include <stdint.h>

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>

#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "net/dcsctp/public/timeout.h"

namespace dcsctp {

enum class TimerBackoffAlgorithm {
  // The base duration will be used for any restart.
  kFixed,
  // An exponential backoff is used for restarts, with a 2x multiplier, meaning
  // that every restart will use a duration that is twice as long as the
  // previous.
  kExponential,
};

struct TimerOptions {
  explicit TimerOptions(DurationMs duration)
      : TimerOptions(duration, TimerBackoffAlgorithm::kExponential) {}
  TimerOptions(DurationMs duration, TimerBackoffAlgorithm backoff_algorithm)
      : TimerOptions(duration, backoff_algorithm, -1) {}
  TimerOptions(DurationMs duration,
               TimerBackoffAlgorithm backoff_algorithm,
               int max_restarts)
      : duration(duration),
        backoff_algorithm(backoff_algorithm),
        max_restarts(max_restarts) {}

  // The initial timer duration. Can be overridden with `set_duration`.
  const DurationMs duration;
  // If the duration should be increased (using exponential backoff) when it is
  // restarted. If not set, the same duration will be used.
  const TimerBackoffAlgorithm backoff_algorithm;
  // The maximum number of times that the timer will be automatically restarted.
  const int max_restarts;
};

// A high-level timer (in contrast to the low-level `Timeout` class).
//
// Timers are started and can be stopped or restarted. When a timer expires,
// the provided `on_expired` callback will be triggered. A timer is
// automatically restarted, as long as the number of restarts is below the
// configurable `max_restarts` parameter. The `is_running` property can be
// queried to know if it's still running after having expired.
//
// When a timer is restarted, it will use a configurable `backoff_algorithm` to
// possibly adjust the duration of the next expiry. It is also possible to
// return a new base duration (which is the duration before it's adjusted by the
// backoff algorithm).
class Timer {
 public:
  // When expired, the timer handler can optionally return a new duration which
  // will be set as `duration` and used as base duration when the timer is
  // restarted and as input to the backoff algorithm.
  using OnExpired = std::function<absl::optional<DurationMs>()>;

  // TimerManager will have pointers to these instances, so they must not move.
  Timer(const Timer&) = delete;
  Timer& operator=(const Timer&) = delete;

  ~Timer();

  // Starts the timer if it's stopped or restarts the timer if it's already
  // running. The `expiration_count` will be reset.
  void Start();

  // Stops the timer. This can also be called when the timer is already stopped.
  // The `expiration_count` will be reset.
  void Stop();

  // Sets the base duration. The actual timer duration may be larger depending
  // on the backoff algorithm.
  void set_duration(DurationMs duration) { duration_ = duration; }

  // Retrieves the base duration. The actual timer duration may be larger
  // depending on the backoff algorithm.
  DurationMs duration() const { return duration_; }

  // Returns the number of times the timer has expired.
  int expiration_count() const { return expiration_count_; }

  // Returns the timer's options.
  const TimerOptions& options() const { return options_; }

  // Returns the name of the timer.
  absl::string_view name() const { return name_; }

  // Indicates if this timer is currently running.
  bool is_running() const { return is_running_; }

 private:
  friend class TimerManager;
  using UnregisterHandler = std::function<void()>;
  Timer(uint32_t id,
        absl::string_view name,
        OnExpired on_expired,
        UnregisterHandler unregister,
        std::unique_ptr<Timeout> timeout,
        const TimerOptions& options);

  // Called by TimerManager. Will trigger the callback and increment
  // `expiration_count`. The timer will automatically be restarted at the
  // duration as decided by the backoff algorithm, unless the
  // `TimerOptions::max_restarts` has been reached and then it will be stopped
  // and `is_running()` will return false.
  void Trigger(uint32_t generation);

  const uint32_t id_;
  const std::string name_;
  const TimerOptions options_;
  const OnExpired on_expired_;
  const UnregisterHandler unregister_handler_;
  const std::unique_ptr<Timeout> timeout_;

  DurationMs duration_;

  // Increased on each start, and is matched on Trigger, to avoid races.
  uint32_t generation_ = 0;
  bool is_running_ = false;
  // Incremented each time time has expired and reset when stopped or restarted.
  int expiration_count_ = 0;
};

// Creates and manages timers.
class TimerManager {
 public:
  explicit TimerManager(
      std::function<std::unique_ptr<Timeout>()> create_timeout)
      : create_timeout_(std::move(create_timeout)) {}

  // Creates a timer with name `name` that will expire (when started) after
  // `options.duration` and call `on_expired`. There are more `options` that
  // affects the behavior. Note that timers are created initially stopped.
  std::unique_ptr<Timer> CreateTimer(absl::string_view name,
                                     Timer::OnExpired on_expired,
                                     const TimerOptions& options);

  void HandleTimeout(TimeoutID timeout_id);

 private:
  const std::function<std::unique_ptr<Timeout>()> create_timeout_;
  std::unordered_map<int, Timer*> timers_;
  uint32_t next_id_ = 0;
};

}  // namespace dcsctp

#endif  // NET_DCSCTP_TIMER_TIMER_H_
