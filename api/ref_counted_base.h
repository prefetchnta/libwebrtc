/*
 *  Copyright 2017 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef API_REF_COUNTED_BASE_H_
#define API_REF_COUNTED_BASE_H_

#include <type_traits>

#include "rtc_base/constructor_magic.h"
#include "rtc_base/ref_counter.h"

namespace rtc {

class RefCountedBase {
 public:
  RefCountedBase() = default;

  void AddRef() const { ref_count_.IncRef(); }
  RefCountReleaseStatus Release() const {
    const auto status = ref_count_.DecRef();
    if (status == RefCountReleaseStatus::kDroppedLastRef) {
      delete this;
    }
    return status;
  }

 protected:
  virtual ~RefCountedBase() = default;

 private:
  mutable webrtc::webrtc_impl::RefCounter ref_count_{0};

  RTC_DISALLOW_COPY_AND_ASSIGN(RefCountedBase);
};

// Template based version of `RefCountedBase` for simple implementations that do
// not need (or want) destruction via virtual destructor or the overhead of a
// vtable.
//
// To use:
//   struct MyInt : public rtc::RefCountedNonVirtual<MyInt>  {
//     int foo_ = 0;
//   };
//
//   rtc::scoped_refptr<MyInt> my_int(new MyInt());
//
// sizeof(MyInt) on a 32 bit system would then be 8, int + refcount and no
// vtable generated.
template <typename T>
class RefCountedNonVirtual {
 public:
  RefCountedNonVirtual() = default;

  void AddRef() const { ref_count_.IncRef(); }
  RefCountReleaseStatus Release() const {
    // If you run into this assert, T has virtual methods. There are two
    // options:
    // 1) The class doesn't actually need virtual methods, the type is complete
    //    so the virtual attribute(s) can be removed.
    // 2) The virtual methods are a part of the design of the class. In this
    //    case you can consider using `RefCountedBase` instead or alternatively
    //    use `rtc::RefCountedObject`.
    static_assert(!std::is_polymorphic<T>::value,
                  "T has virtual methods. RefCountedBase is a better fit.");
    const auto status = ref_count_.DecRef();
    if (status == RefCountReleaseStatus::kDroppedLastRef) {
      delete static_cast<const T*>(this);
    }
    return status;
  }

 protected:
  ~RefCountedNonVirtual() = default;

 private:
  mutable webrtc::webrtc_impl::RefCounter ref_count_{0};

  RTC_DISALLOW_COPY_AND_ASSIGN(RefCountedNonVirtual);
};

}  // namespace rtc

#endif  // API_REF_COUNTED_BASE_H_
