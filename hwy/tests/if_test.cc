// Copyright 2019 Google LLC
// SPDX-License-Identifier: Apache-2.0
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <stddef.h>

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "tests/if_test.cc"
#include "hwy/foreach_target.h"  // IWYU pragma: keep
#include "hwy/highway.h"
#include "hwy/tests/test_util-inl.h"

HWY_BEFORE_NAMESPACE();
namespace hwy {
namespace HWY_NAMESPACE {

struct TestIfThenElse {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    // TODO(janwas): file compiler bug report
#if HWY_COMPILER_CLANG && (HWY_COMPILER_CLANG < 1800) && HWY_ARCH_ARM
    if (IsSpecialFloat<T>()) return;
#endif

    RandomState rng;

    using TI = MakeSigned<T>;  // For mask > 0 comparison
    const Rebind<TI, D> di;
    const size_t N = Lanes(d);
    auto in1 = AllocateAligned<T>(N);
    auto in2 = AllocateAligned<T>(N);
    auto bool_lanes = AllocateAligned<TI>(N);
    auto expected = AllocateAligned<T>(N);
    HWY_ASSERT(in1 && in2 && bool_lanes && expected);

    // Each lane should have a chance of having mask=true.
    for (size_t rep = 0; rep < AdjustedReps(200); ++rep) {
      for (size_t i = 0; i < N; ++i) {
        in1[i] = ConvertScalarTo<T>(Random32(&rng));
        in2[i] = ConvertScalarTo<T>(Random32(&rng));
        bool_lanes[i] = (Random32(&rng) & 16) ? TI(1) : TI(0);
      }

      const auto v1 = Load(d, in1.get());
      const auto v2 = Load(d, in2.get());
      const auto mask = RebindMask(d, Gt(Load(di, bool_lanes.get()), Zero(di)));

      for (size_t i = 0; i < N; ++i) {
        expected[i] = bool_lanes[i] ? in1[i] : in2[i];
      }
      HWY_ASSERT_VEC_EQ(d, expected.get(), IfThenElse(mask, v1, v2));

      for (size_t i = 0; i < N; ++i) {
        expected[i] = bool_lanes[i] ? in1[i] : ConvertScalarTo<T>(0);
      }
      HWY_ASSERT_VEC_EQ(d, expected.get(), IfThenElseZero(mask, v1));

      for (size_t i = 0; i < N; ++i) {
        expected[i] = bool_lanes[i] ? ConvertScalarTo<T>(0) : in2[i];
      }
      HWY_ASSERT_VEC_EQ(d, expected.get(), IfThenZeroElse(mask, v2));
    }
  }
};

HWY_NOINLINE void TestAllIfThenElse() {
  ForAllTypesAndSpecial(ForPartialVectors<TestIfThenElse>());
}

struct TestIfVecThenElse {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    RandomState rng;

    using TU = MakeUnsigned<T>;  // For all-one mask
    const Rebind<TU, D> du;
    const size_t N = Lanes(d);
    auto in1 = AllocateAligned<T>(N);
    auto in2 = AllocateAligned<T>(N);
    auto vec_lanes = AllocateAligned<TU>(N);
    auto expected = AllocateAligned<T>(N);
    HWY_ASSERT(in1 && in2 && vec_lanes && expected);

    // Each lane should have a chance of having mask=true.
    for (size_t rep = 0; rep < AdjustedReps(200); ++rep) {
      for (size_t i = 0; i < N; ++i) {
        in1[i] = ConvertScalarTo<T>(Random32(&rng));
        in2[i] = ConvertScalarTo<T>(Random32(&rng));
        vec_lanes[i] = (Random32(&rng) & 16) ? static_cast<TU>(~TU(0)) : TU(0);
      }

      const auto v1 = Load(d, in1.get());
      const auto v2 = Load(d, in2.get());
      const auto vec = BitCast(d, Load(du, vec_lanes.get()));

      for (size_t i = 0; i < N; ++i) {
        expected[i] = vec_lanes[i] ? in1[i] : in2[i];
      }
      HWY_ASSERT_VEC_EQ(d, expected.get(), IfVecThenElse(vec, v1, v2));
    }
  }
};

HWY_NOINLINE void TestAllIfVecThenElse() {
  ForAllTypes(ForPartialVectors<TestIfVecThenElse>());
}

struct TestZeroIfNegative {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const auto v0 = Zero(d);
    const auto vp = Iota(d, 1);
    const auto vn = Iota(d, -1E4);  // assumes N < 10^4

    // Zero and positive remain unchanged
    HWY_ASSERT_VEC_EQ(d, v0, ZeroIfNegative(v0));
    HWY_ASSERT_VEC_EQ(d, vp, ZeroIfNegative(vp));

    // Negative are all replaced with zero
    HWY_ASSERT_VEC_EQ(d, v0, ZeroIfNegative(vn));
  }
};

HWY_NOINLINE void TestAllZeroIfNegative() {
  ForFloatTypes(ForPartialVectors<TestZeroIfNegative>());
}

struct TestIfNegative {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const auto v0 = Zero(d);
    const auto vp = Iota(d, 1);
    const auto vsignbit = SignBit(d);
    const auto vn = Or(vp, vsignbit);

    // Zero and positive remain unchanged
    HWY_ASSERT_VEC_EQ(d, v0, IfNegativeThenElse(v0, vn, v0));
    HWY_ASSERT_VEC_EQ(d, vn, IfNegativeThenElse(v0, v0, vn));
    HWY_ASSERT_VEC_EQ(d, vp, IfNegativeThenElse(vp, vn, vp));
    HWY_ASSERT_VEC_EQ(d, vn, IfNegativeThenElse(vp, vp, vn));

    // Negative are replaced with 2nd arg
    HWY_ASSERT_VEC_EQ(d, v0, IfNegativeThenElse(vn, v0, vp));
    HWY_ASSERT_VEC_EQ(d, vn, IfNegativeThenElse(vn, vn, v0));
    HWY_ASSERT_VEC_EQ(d, vp, IfNegativeThenElse(vn, vp, vn));

    const RebindToSigned<decltype(d)> di;
    const RebindToUnsigned<decltype(d)> du;
    using TU = TFromD<decltype(du)>;

    const auto s1 = BitCast(d, ShiftLeft<sizeof(TU) * 8 - 1>(Iota(du, 1)));

    const auto m1 = Xor3(vp, s1, BitCast(d, Set(du, TU{0x71})));
    const auto x1 = Xor(vp, BitCast(d, Set(du, TU{0x2B})));
    const auto x2 = Xor(vp, BitCast(d, Set(du, TU{0xE2})));
    const auto m2 = Xor(m1, vsignbit);

    const auto m1_s = BitCast(d, BroadcastSignBit(BitCast(di, m1)));

    const auto expected_1 = BitwiseIfThenElse(m1_s, x1, x2);
    const auto expected_2 = BitwiseIfThenElse(m1_s, x2, x1);

    HWY_ASSERT_VEC_EQ(d, expected_1, IfNegativeThenElse(m1, x1, x2));
    HWY_ASSERT_VEC_EQ(d, expected_2, IfNegativeThenElse(m2, x1, x2));
  }
};

HWY_NOINLINE void TestAllIfNegative() {
  ForFloatTypes(ForPartialVectors<TestIfNegative>());
  ForSignedTypes(ForPartialVectors<TestIfNegative>());
}

struct TestIfNegativeThenNegOrUndefIfZero {
  template <class D, HWY_IF_LANES_LE_D(D, 1)>
  static HWY_INLINE void TestMoreThan1LaneIfNegativeThenNegOrUndefIfZero(
      D /*d*/, Vec<D> /*v1*/, Vec<D> /*v2*/) {}
#if HWY_TARGET != HWY_SCALAR
  template <class D, HWY_IF_LANES_GT_D(D, 1)>
  static HWY_INLINE void TestMoreThan1LaneIfNegativeThenNegOrUndefIfZero(
      D d, Vec<D> v1, Vec<D> v2) {
#if HWY_HAVE_SCALABLE
    if (Lanes(d) < 2) {
      return;
    }
#endif

    const auto v3 = InterleaveLower(d, v1, v1);
    const auto v4 = InterleaveUpper(d, v1, v1);
    const auto v5 = InterleaveLower(d, v1, v2);
    const auto v6 = InterleaveUpper(d, v1, v2);
    const auto v7 = InterleaveLower(d, v2, v1);
    const auto v8 = InterleaveUpper(d, v2, v1);

    HWY_ASSERT_VEC_EQ(d, v3, IfNegativeThenNegOrUndefIfZero(v3, v3));
    HWY_ASSERT_VEC_EQ(d, v4, IfNegativeThenNegOrUndefIfZero(v4, v4));
    HWY_ASSERT_VEC_EQ(d, v3, IfNegativeThenNegOrUndefIfZero(v5, v5));
    HWY_ASSERT_VEC_EQ(d, v4, IfNegativeThenNegOrUndefIfZero(v6, v6));
    HWY_ASSERT_VEC_EQ(d, v3, IfNegativeThenNegOrUndefIfZero(v7, v7));
    HWY_ASSERT_VEC_EQ(d, v4, IfNegativeThenNegOrUndefIfZero(v8, v8));

    HWY_ASSERT_VEC_EQ(d, v5, IfNegativeThenNegOrUndefIfZero(v3, v5));
    HWY_ASSERT_VEC_EQ(d, v6, IfNegativeThenNegOrUndefIfZero(v4, v6));
    HWY_ASSERT_VEC_EQ(d, v7, IfNegativeThenNegOrUndefIfZero(v3, v7));
    HWY_ASSERT_VEC_EQ(d, v8, IfNegativeThenNegOrUndefIfZero(v4, v8));

    const auto zero = Zero(d);
    HWY_ASSERT_VEC_EQ(d, zero, IfNegativeThenNegOrUndefIfZero(v3, zero));
    HWY_ASSERT_VEC_EQ(d, zero, IfNegativeThenNegOrUndefIfZero(v4, zero));
    HWY_ASSERT_VEC_EQ(d, zero, IfNegativeThenNegOrUndefIfZero(v5, zero));
    HWY_ASSERT_VEC_EQ(d, zero, IfNegativeThenNegOrUndefIfZero(v6, zero));
    HWY_ASSERT_VEC_EQ(d, zero, IfNegativeThenNegOrUndefIfZero(v7, zero));
    HWY_ASSERT_VEC_EQ(d, zero, IfNegativeThenNegOrUndefIfZero(v8, zero));
  }
#endif

  template <typename T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const auto v1 = PositiveIota(d);
    const auto v2 = Neg(v1);

    HWY_ASSERT_VEC_EQ(d, v1, IfNegativeThenNegOrUndefIfZero(v1, v1));
    HWY_ASSERT_VEC_EQ(d, v2, IfNegativeThenNegOrUndefIfZero(v1, v2));
    HWY_ASSERT_VEC_EQ(d, v2, IfNegativeThenNegOrUndefIfZero(v2, v1));
    HWY_ASSERT_VEC_EQ(d, v1, IfNegativeThenNegOrUndefIfZero(v2, v2));

    const auto zero = Zero(d);
    HWY_ASSERT_VEC_EQ(d, zero, IfNegativeThenNegOrUndefIfZero(zero, zero));
    HWY_ASSERT_VEC_EQ(d, zero, IfNegativeThenNegOrUndefIfZero(v1, zero));
    HWY_ASSERT_VEC_EQ(d, zero, IfNegativeThenNegOrUndefIfZero(v2, zero));

    const auto vmin = Set(d, LowestValue<T>());
    const auto vmax = Set(d, HighestValue<T>());

    HWY_ASSERT_VEC_EQ(d, v2, IfNegativeThenNegOrUndefIfZero(vmin, v1));
    HWY_ASSERT_VEC_EQ(d, v1, IfNegativeThenNegOrUndefIfZero(vmin, v2));
    HWY_ASSERT_VEC_EQ(d, v1, IfNegativeThenNegOrUndefIfZero(vmax, v1));
    HWY_ASSERT_VEC_EQ(d, v2, IfNegativeThenNegOrUndefIfZero(vmax, v2));

    TestMoreThan1LaneIfNegativeThenNegOrUndefIfZero(d, v1, v2);
  }
};

HWY_NOINLINE void TestAllIfNegativeThenNegOrUndefIfZero() {
  ForSignedTypes(ForPartialVectors<TestIfNegativeThenNegOrUndefIfZero>());
  ForFloatTypes(ForPartialVectors<TestIfNegativeThenNegOrUndefIfZero>());
}

// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace hwy
HWY_AFTER_NAMESPACE();

#if HWY_ONCE

namespace hwy {
HWY_BEFORE_TEST(HwyIfTest);
HWY_EXPORT_AND_TEST_P(HwyIfTest, TestAllIfThenElse);
HWY_EXPORT_AND_TEST_P(HwyIfTest, TestAllIfVecThenElse);
HWY_EXPORT_AND_TEST_P(HwyIfTest, TestAllZeroIfNegative);
HWY_EXPORT_AND_TEST_P(HwyIfTest, TestAllIfNegative);
HWY_EXPORT_AND_TEST_P(HwyIfTest, TestAllIfNegativeThenNegOrUndefIfZero);
}  // namespace hwy

#endif
