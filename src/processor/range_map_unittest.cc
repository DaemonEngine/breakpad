// Copyright (C) 2006 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// range_map_unittest.cc: Unit tests for RangeMap
//
// Author: Mark Mentovai


#include <stdio.h>

#include <climits>
#include <memory>

#include "processor/range_map.h"


using std::auto_ptr;
using google_airbag::RangeMap;


// A CountedObject holds an int.  A global (not thread safe!) count of
// allocated CountedObjects is maintained to help test memory management.
class CountedObject {
  public:
    CountedObject(int id) : id_(id) { ++count_; }
    CountedObject(const CountedObject& that) : id_(that.id_) { ++count_; }
    ~CountedObject() { --count_; }

    static int count() { return count_; }
    int id() const { return id_; }

  private:
    static int count_;
    int        id_;
};

int CountedObject::count_;


typedef int AddressType;
typedef RangeMap<AddressType, CountedObject> TestMap;


// RangeTest contains data to use for store and retrieve tests.  See
// RunTests for descriptions of the tests.
struct RangeTest {
  // Base address to use for test
  AddressType address;

  // Size of range to use for test
  AddressType size;

  // Unique ID of range - unstorable ranges must have unique IDs too
  int         id;

  // Whether this range is expected to be stored successfully or not
  bool        expect_storable;
};


// A RangeTestSet encompasses multiple RangeTests, which are run in
// sequence on the same RangeMap.
struct RangeTestSet {
  // An array of RangeTests
  const RangeTest* range_tests;

  // The number of tests in the set
  unsigned int     range_test_count;
};


// StoreTest uses the data in a RangeTest and calls StoreRange on the
// test RangeMap.  It returns true if the expected result occurred, and
// false if something else happened.
bool StoreTest(TestMap* range_map, const RangeTest* range_test) {
  CountedObject object(range_test->id);
  bool stored = range_map->StoreRange(range_test->address,
                                      range_test->size,
                                      object);

  if (stored != range_test->expect_storable) {
    fprintf(stderr, "FAILED: "
            "StoreRange id %d, expected %s, observed %s\n",
            range_test->id,
            range_test->expect_storable ? "storable" : "not storable",
            stored ? "stored" : "not stored");
    return false;
  }

  return true;
}


// RetrieveTest uses the data in RangeTest and calls RetrieveRange on the
// test RangeMap.  If it retrieves the expected value (which can be no
// map entry at the specified range,) it returns true, otherwise, it returns
// false.  RetrieveTest will check the values around the base address and
// the high address of a range to guard against off-by-one errors.
bool RetrieveTest(TestMap* range_map, const RangeTest* range_test) {
  for (unsigned int side = 0; side <= 1; ++side) {
    // When side == 0, check the low side (base address) of each range.
    // When side == 1, check the high side (base + size) of each range.

    // Check one-less and one-greater than the target address in addition
    // to the target address itself.

    // If the size of the range is only 1, don't check one greater than
    // the base or one less than the high - for a successfully stored
    // range, these tests would erroneously fail because the range is too
    // small.
    AddressType low_offset = -1;
    AddressType high_offset = 1;
    if (range_test->size == 1) {
      if (!side)          // when checking the low side,
        high_offset = 0;  // don't check one over the target
      else                // when checking the high side,
        low_offset = 0;   // don't check one under the target
    }

    for (AddressType offset = low_offset; offset <= high_offset; ++offset) {
      AddressType address =
          offset +
          (!side ? range_test->address :
                   range_test->address + range_test->size - 1);

      bool expected_result = false;  // correct for tests not stored
      if (range_test->expect_storable) {
        if (offset == 0)             // when checking target,
          expected_result = true;    // should always succeed
        else if (offset == -1)       // when checking one below target,
          expected_result = side;    // should fail low and succeed high
        else                         // when checking one above target,
          expected_result = !side;   // should succeed low and fail high
      }

      CountedObject object(-1);
      bool retrieved = range_map->RetrieveRange(address, &object);

      bool observed_result = retrieved && object.id() == range_test->id;

       if (observed_result != expected_result) {
        fprintf(stderr, "FAILED: "
                        "RetrieveRange id %d, side %d, offset %d, "
                        "expected %s, observed %s\n",
                        range_test->id,
                        side,
                        offset,
                        expected_result ? "true" : "false",
                        observed_result ? "true" : "false");
        return false;
      }
    }
  }

  return true;
}


// RunTests runs a series of test sets.
bool RunTests() {
  // These tests will be run sequentially.  The first set of tests exercises
  // most functions of RangeTest, and verifies all of the bounds-checking.
  const RangeTest range_tests_0[] = {
    { INT_MIN,     16,      1,  true },   // lowest possible range
    { -2,          5,       2,  true },   // a range through zero
    { INT_MAX - 9, 11,      3,  false },  // tests anti-overflow
    { INT_MAX - 9, 10,      4,  true },   // highest possible range
    { 5,           0,       5,  false },  // tests anti-zero-size
    { 5,           1,       6,  true },   // smallest possible range
    { -20,         15,      7,  true },   // entirely negative

    { 10,          10,      10, true },   // causes the following tests to fail
    { 9,           10,      11, false },  // one-less base, one-less high
    { 9,           11,      12, false },  // one-less base, identical high
    { 9,           12,      13, false },  // completely contains existing
    { 10,          9,       14, false },  // identical base, one-less high
    { 10,          10,      15, false },  // exactly identical to existing range
    { 10,          11,      16, false },  // identical base, one-greater high
    { 11,          8,       17, false },  // contained completely within
    { 11,          9,       18, false },  // one-greater base, identical high
    { 11,          10,      19, false },  // one-greater base, one-greater high
    { 9,           2,       20, false },  // overlaps bottom by one
    { 10,          1,       21, false },  // overlaps bottom by one, contained
    { 19,          1,       22, false },  // overlaps top by one, contained
    { 19,          2,       23, false },  // overlaps top by one

    { 9,           1,       24, true },   // directly below without overlap
    { 20,          1,       25, true },   // directly above without overlap

    { 6,           3,       26, true },   // exactly between two ranges, gapless
    { 7,           3,       27, false },  // tries to span two ranges
    { 7,           5,       28, false },  // tries to span three ranges
    { 4,           20,      29, false },  // tries to contain several ranges

    { 30,          50,      30, true },
    { 90,          25,      31, true },
    { 35,          65,      32, false },  // tries to span two noncontiguous
    { 120,         10000,   33, true },   // > 8-bit
    { 20000,       20000,   34, true },   // > 8-bit
    { 0x10001,     0x10001, 35, true },   // > 16-bit

    { 27,          -1,      36, false }    // tests high < base
  };

  // Attempt to fill the entire space.  The entire space must be filled with
  // three stores because AddressType is signed for these tests, so RangeMap
  // treats the size as signed and rejects sizes that appear to be negative.
  // Even if these tests were run as unsigned, two stores would be needed
  // to fill the space because the entire size of the space could only be
  // described by using one more bit than would be present in AddressType.
  const RangeTest range_tests_1[] = {
    { INT_MIN, INT_MAX, 50, true },   // From INT_MIN to -2, inclusive
    { -1,      2,       51, true },   // From -1 to 0, inclusive
    { 1,       INT_MAX, 52, true },   // From 1 to INT_MAX, inclusive
    { INT_MIN, INT_MAX, 53, false },  // Can't fill the space twice
    { -1,      2,       54, false },
    { 1,       INT_MAX, 55, false },
    { -3,      6,       56, false },  // -3 to 2, inclusive - spans 3 ranges
  };

  // A light round of testing to verify that RetrieveRange does the right
  // the right thing at the extremities of the range when nothing is stored
  // there.  Checks are forced without storing anything at the extremities
  // by setting size = 0.
  const RangeTest range_tests_2[] = {
    { INT_MIN, 0, 100, false },  // makes RetrieveRange check low end
    { -1,      3, 101, true },
    { INT_MAX, 0, 102, false },  // makes RetrieveRange check high end
  };

  // Similar to the previous test set, but with a couple of ranges closer
  // to the extremities.
  const RangeTest range_tests_3[] = {
    { INT_MIN + 1, 1, 110, true },
    { INT_MAX - 1, 1, 111, true },
    { INT_MIN,     0, 112, false },  // makes RetrieveRange check low end
    { INT_MAX,     0, 113, false }   // makes RetrieveRange check high end
  };

  // The range map is cleared between sets of tests listed here.
  const RangeTestSet range_test_sets[] = {
    { range_tests_0, sizeof(range_tests_0) / sizeof(RangeTest) },
    { range_tests_1, sizeof(range_tests_1) / sizeof(RangeTest) },
    { range_tests_2, sizeof(range_tests_2) / sizeof(RangeTest) },
    { range_tests_3, sizeof(range_tests_3) / sizeof(RangeTest) },
    { range_tests_0, sizeof(range_tests_0) / sizeof(RangeTest) }  // Run again
  };

  // Maintain the range map in a pointer so that deletion can be meaningfully
  // tested.
  auto_ptr<TestMap> range_map(new TestMap());

  // Run all of the test sets in sequence.
  unsigned int range_test_set_count = sizeof(range_test_sets) /
                                      sizeof(RangeTestSet);
  for (unsigned int range_test_set_index = 0;
       range_test_set_index < range_test_set_count;
       ++range_test_set_index) {
    const RangeTest* range_tests =
        range_test_sets[range_test_set_index].range_tests;
    unsigned int range_test_count =
        range_test_sets[range_test_set_index].range_test_count;

    // Run the StoreRange test, which validates StoreRange and initializes
    // the RangeMap with data for the RetrieveRange test.
    int stored_count = 0;  // The number of ranges successfully stored
    for (unsigned int range_test_index = 0;
         range_test_index < range_test_count;
         ++range_test_index) {
      const RangeTest* range_test = &range_tests[range_test_index];
      if (!StoreTest(range_map.get(), range_test))
        return false;

      if (range_test->expect_storable)
        ++stored_count;
    }

    // There should be exactly one CountedObject for everything successfully
    // stored in the RangeMap.
    if (CountedObject::count() != stored_count) {
      fprintf(stderr, "FAILED: "
              "stored object counts don't match, expected %d, observed %d\n",
              stored_count,
              CountedObject::count());

      return false;
    }

    // Run the RetrieveRange test
    for (unsigned int range_test_index = 0;
         range_test_index < range_test_count;
         ++range_test_index) {
      const RangeTest* range_test = &range_tests[range_test_index];
      if (!RetrieveTest(range_map.get(), range_test))
        return false;
    }

    // Clear the map between test sets.  If this is the final test set,
    // delete the map instead to test destruction.
    if (range_test_set_index < range_test_set_count - 1)
      range_map->Clear();
    else
      range_map.reset();

    // Test that all stored objects are freed when the RangeMap is cleared
    // or deleted.
    if (CountedObject::count() != 0) {
      fprintf(stderr, "FAILED: "
              "did not free all objects after %s, %d still allocated\n",
              range_test_set_index < range_test_set_count - 1 ? "clear"
                                                              : "delete",
              CountedObject::count());

      return false;
    }
  }

  return true;
}

int main(int argc, char** argv) {
  return RunTests() ? 0 : 1;
}
