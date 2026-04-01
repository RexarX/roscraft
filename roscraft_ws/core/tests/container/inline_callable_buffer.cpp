#include <doctest/doctest.h>

#include <roscraft/container/inline_callable_buffer.hpp>

#include <memory_resource>
#include <string>
#include <vector>

using namespace roscraft::container;

namespace {

// Track invocation calls
struct InvocationTracker {
  static inline std::vector<int> call_order;
  static inline int total_calls = 0;

  static void Reset() {
    call_order.clear();
    total_calls = 0;
  }
};

// Simple callable for single-signature buffer
struct SimpleCallable {
  int id = 0;

  explicit SimpleCallable(int i) : id(i) {}

  void operator()() { InvocationTracker::call_order.push_back(id); }
};

// Callable with argument
struct CallableWithArg {
  int id = 0;
  int multiplier = 1;

  explicit CallableWithArg(int i, int m = 1) : id(i), multiplier(m) {}

  void operator()(int value) {
    InvocationTracker::call_order.push_back(id * multiplier + value);
  }
};

// Callable with multiple arguments
struct CallableWithMultipleArgs {
  int id = 0;

  explicit CallableWithMultipleArgs(int i) : id(i) {}

  void operator()(int a, int b) {
    InvocationTracker::call_order.push_back(id + a + b);
  }
};

// Multi-signature callable using index dispatch
struct MultiSignatureCallable {
  int id = 0;

  explicit MultiSignatureCallable(int i) : id(i) {}

  void operator()(std::integral_constant<size_t, 0>) {
    InvocationTracker::call_order.push_back(id * 10);
  }

  void operator()(std::integral_constant<size_t, 1>, int value) {
    InvocationTracker::call_order.push_back(id * 100 + value);
  }
};

// Callable with custom method names
struct CustomMethodCallable {
  int id = 0;

  explicit CustomMethodCallable(int i) : id(i) {}

  void Execute() { InvocationTracker::call_order.push_back(id); }

  void Log(int level) {
    InvocationTracker::call_order.push_back(id + level * 1000);
  }
};

// Non-trivial callable with string
struct NonTrivialCallable {
  std::string name;
  int id = 0;

  NonTrivialCallable() = default;
  explicit NonTrivialCallable(std::string n, int i)
      : name(std::move(n)), id(i) {}
  NonTrivialCallable(const NonTrivialCallable&) = default;
  NonTrivialCallable(NonTrivialCallable&&) noexcept = default;
  ~NonTrivialCallable() = default;

  NonTrivialCallable& operator=(const NonTrivialCallable&) = default;
  NonTrivialCallable& operator=(NonTrivialCallable&&) noexcept = default;

  void operator()() { InvocationTracker::call_order.push_back(id); }
};

// Counting type to track constructions/destructions
struct CountingCallable {
  static inline int construct_count = 0;
  static inline int destruct_count = 0;
  static inline int invoke_count = 0;

  int id = 0;

  CountingCallable() { ++construct_count; }
  explicit CountingCallable(int i) : id(i) { ++construct_count; }
  CountingCallable(const CountingCallable& other) : id(other.id) {
    ++construct_count;
  }
  CountingCallable(CountingCallable&& other) noexcept : id(other.id) {
    ++construct_count;
  }
  ~CountingCallable() { ++destruct_count; }

  CountingCallable& operator=(const CountingCallable&) = default;
  CountingCallable& operator=(CountingCallable&&) noexcept = default;

  void operator()() { ++invoke_count; }

  static void Reset() {
    construct_count = 0;
    destruct_count = 0;
    invoke_count = 0;
  }
};

// Lambda-like stateful callable
struct StatefulCallable {
  int* counter = nullptr;

  explicit StatefulCallable(int& cnt) : counter(&cnt) {}

  void operator()() const {
    if (counter != nullptr) {
      ++(*counter);
    }
  }
};

void increment(int& value) {
  ++value;
}

}  // namespace

TEST_SUITE("container::InlineCallableBufferStorable") {
  TEST_CASE("container::InlineCallableBufferStorable: concept validation") {
    SUBCASE("Simple types satisfy concept") {
      CHECK(InlineCallableBufferStorable<SimpleCallable>);
      CHECK(InlineCallableBufferStorable<CallableWithArg>);
      CHECK(InlineCallableBufferStorable<CustomMethodCallable>);
    }

    SUBCASE("Non-trivial types satisfy concept") {
      CHECK(InlineCallableBufferStorable<NonTrivialCallable>);
    }
  }
}

TEST_SUITE("container::InlineCallableBuffer") {
  TEST_CASE("container::InlineCallableBuffer::ctor: default construction") {
    InlineCallableBuffer<void()> buffer;

    CHECK(buffer.Empty());
    CHECK_EQ(buffer.Size(), 0);
  }

  TEST_CASE("container::InlineCallableBuffer::ctor: allocator construction") {
    std::allocator<std::byte> alloc;
    InlineCallableBuffer<std::allocator<std::byte>, void()> buffer(alloc);

    CHECK(buffer.Empty());
    CHECK_EQ(buffer.Size(), 0);
  }

  TEST_CASE("container::InlineCallableBuffer::ctor: move construction") {
    InlineCallableBuffer<std::allocator<std::byte>, void()> original;
    original.Push(SimpleCallable{1});
    original.Push(SimpleCallable{2});

    CHECK_EQ(original.Size(), 2);

    InlineCallableBuffer<std::allocator<std::byte>, void()> moved(
        std::move(original));

    CHECK_EQ(moved.Size(), 2);
    CHECK(original.Empty());

    InvocationTracker::Reset();
    moved.Invoke();

    CHECK_EQ(InvocationTracker::call_order.size(), 2);
    CHECK_EQ(InvocationTracker::call_order[0], 1);
    CHECK_EQ(InvocationTracker::call_order[1], 2);
  }

  TEST_CASE("container::InlineCallableBuffer::operator=: move assignment") {
    InlineCallableBuffer<std::allocator<std::byte>, void()> original;
    original.Push(SimpleCallable{10});

    InlineCallableBuffer<std::allocator<std::byte>, void()> target;
    target.Push(SimpleCallable{99});

    target = std::move(original);

    CHECK_EQ(target.Size(), 1);
    CHECK(original.Empty());

    InvocationTracker::Reset();
    target.Invoke();

    CHECK_EQ(InvocationTracker::call_order.size(), 1);
    CHECK_EQ(InvocationTracker::call_order[0], 10);
  }

  TEST_CASE("container::InlineCallableBuffer::Push: adding callables") {
    InvocationTracker::Reset();

    InlineCallableBuffer<std::allocator<std::byte>, void()> buffer;

    SUBCASE("Push single callable") {
      buffer.Push(SimpleCallable{42});

      CHECK_FALSE(buffer.Empty());
      CHECK_EQ(buffer.Size(), 1);
    }

    SUBCASE("Push multiple callables") {
      buffer.Push(SimpleCallable{1});
      buffer.Push(SimpleCallable{2});
      buffer.Push(SimpleCallable{3});

      CHECK_EQ(buffer.Size(), 3);
    }

    SUBCASE("Push by rvalue") {
      SimpleCallable callable{100};
      buffer.Push(std::move(callable));

      CHECK_EQ(buffer.Size(), 1);
    }
  }

  TEST_CASE("container::InlineCallableBuffer::Push: with custom methods") {
    InvocationTracker::Reset();

    InlineCallableBuffer<std::allocator<std::byte>, void(), void(int)> buffer;

    buffer.Push<&CustomMethodCallable::Execute, &CustomMethodCallable::Log>(
        CustomMethodCallable{5});

    CHECK_EQ(buffer.Size(), 1);

    buffer.Invoke<0>();
    CHECK_EQ(InvocationTracker::call_order.size(), 1);
    CHECK_EQ(InvocationTracker::call_order[0], 5);

    buffer.Invoke<1>(3);
    CHECK_EQ(InvocationTracker::call_order.size(), 2);
    CHECK_EQ(InvocationTracker::call_order[1], 5 + 3 * 1000);
  }

  TEST_CASE("container::InlineCallableBuffer::Invoke: single signature") {
    InvocationTracker::Reset();

    InlineCallableBuffer<std::allocator<std::byte>, void()> buffer;

    buffer.Push(SimpleCallable{1});
    buffer.Push(SimpleCallable{2});
    buffer.Push(SimpleCallable{3});

    buffer.Invoke();

    CHECK_EQ(InvocationTracker::call_order.size(), 3);
    CHECK_EQ(InvocationTracker::call_order[0], 1);
    CHECK_EQ(InvocationTracker::call_order[1], 2);
    CHECK_EQ(InvocationTracker::call_order[2], 3);
  }

  TEST_CASE("container::InlineCallableBuffer::Invoke: with arguments") {
    InvocationTracker::Reset();

    InlineCallableBuffer<std::allocator<std::byte>, void(int)> buffer;

    buffer.Push(CallableWithArg{10, 1});
    buffer.Push(CallableWithArg{20, 1});

    buffer.Invoke(5);

    CHECK_EQ(InvocationTracker::call_order.size(), 2);
    CHECK_EQ(InvocationTracker::call_order[0], 10 + 5);
    CHECK_EQ(InvocationTracker::call_order[1], 20 + 5);
  }

  TEST_CASE("container::InlineCallableBuffer::Invoke: multiple arguments") {
    InvocationTracker::Reset();

    InlineCallableBuffer<std::allocator<std::byte>, void(int, int)> buffer;

    buffer.Push(CallableWithMultipleArgs{100});
    buffer.Push(CallableWithMultipleArgs{200});

    buffer.Invoke(10, 20);

    CHECK_EQ(InvocationTracker::call_order.size(), 2);
    CHECK_EQ(InvocationTracker::call_order[0], 100 + 10 + 20);
    CHECK_EQ(InvocationTracker::call_order[1], 200 + 10 + 20);
  }

  TEST_CASE(
      "container::InlineCallableBuffer::Invoke: multi-signature with index") {
    InvocationTracker::Reset();

    InlineCallableBuffer<std::allocator<std::byte>, void(), void(int)> buffer;

    buffer.Push(MultiSignatureCallable{1});
    buffer.Push(MultiSignatureCallable{2});

    // Invoke first operation (index 0)
    buffer.Invoke<0>();

    CHECK_EQ(InvocationTracker::call_order.size(), 2);
    CHECK_EQ(InvocationTracker::call_order[0], 10);  // 1 * 10
    CHECK_EQ(InvocationTracker::call_order[1], 20);  // 2 * 10

    InvocationTracker::Reset();

    // Invoke second operation (index 1) with argument
    buffer.Invoke<1>(5);

    CHECK_EQ(InvocationTracker::call_order.size(), 2);
    CHECK_EQ(InvocationTracker::call_order[0], 105);  // 1 * 100 + 5
    CHECK_EQ(InvocationTracker::call_order[1], 205);  // 2 * 100 + 5
  }

  TEST_CASE("container::InlineCallableBuffer::Invoke: empty buffer") {
    InvocationTracker::Reset();

    InlineCallableBuffer<std::allocator<std::byte>, void()> buffer;

    // Should not crash
    CHECK_NOTHROW(buffer.Invoke());
    CHECK(InvocationTracker::call_order.empty());
  }

  TEST_CASE("container::InlineCallableBuffer::Clear: clearing all callables") {
    CountingCallable::Reset();

    InlineCallableBuffer<std::allocator<std::byte>, void()> buffer;

    buffer.Push(CountingCallable{1});
    buffer.Push(CountingCallable{2});
    buffer.Push(CountingCallable{3});

    CHECK_EQ(buffer.Size(), 3);
    int constructs_before_clear = CountingCallable::construct_count;
    int destructs_before_clear = CountingCallable::destruct_count;

    buffer.Clear();

    CHECK(buffer.Empty());
    CHECK_EQ(buffer.Size(), 0);
    // Destructors should have been called for the 3 stored callables
    CHECK_EQ(CountingCallable::destruct_count - destructs_before_clear, 3);
  }

  TEST_CASE("container::InlineCallableBuffer::Reserve: reserving space") {
    InlineCallableBuffer<std::allocator<std::byte>, void()> buffer;

    buffer.Reserve(10);

    // Can't directly check capacity, but should be able to add elements
    for (int i = 0; i < 10; ++i) {
      buffer.Push(SimpleCallable{i});
    }

    CHECK_EQ(buffer.Size(), 10);
  }

  TEST_CASE("container::InlineCallableBuffer::ReserveBytes: reserving bytes") {
    InlineCallableBuffer<std::allocator<std::byte>, void()> buffer;

    buffer.ReserveBytes(1024);

    CHECK_GE(buffer.CapacityBytes(), 1024);
  }

  TEST_CASE("container::InlineCallableBuffer::Swap: swapping buffers") {
    InvocationTracker::Reset();

    InlineCallableBuffer<std::allocator<std::byte>, void()> buffer1;
    buffer1.Push(SimpleCallable{1});
    buffer1.Push(SimpleCallable{2});

    InlineCallableBuffer<std::allocator<std::byte>, void()> buffer2;
    buffer2.Push(SimpleCallable{10});

    buffer1.Swap(buffer2);

    CHECK_EQ(buffer1.Size(), 1);
    CHECK_EQ(buffer2.Size(), 2);

    buffer1.Invoke();
    CHECK_EQ(InvocationTracker::call_order.size(), 1);
    CHECK_EQ(InvocationTracker::call_order[0], 10);

    InvocationTracker::Reset();

    buffer2.Invoke();
    CHECK_EQ(InvocationTracker::call_order.size(), 2);
    CHECK_EQ(InvocationTracker::call_order[0], 1);
    CHECK_EQ(InvocationTracker::call_order[1], 2);
  }

  TEST_CASE("container::InlineCallableBuffer::Empty: empty check") {
    InlineCallableBuffer<std::allocator<std::byte>, void()> buffer;

    CHECK(buffer.Empty());

    buffer.Push(SimpleCallable{1});
    CHECK_FALSE(buffer.Empty());

    buffer.Clear();
    CHECK(buffer.Empty());
  }

  TEST_CASE("container::InlineCallableBuffer::Size: size tracking") {
    InlineCallableBuffer<std::allocator<std::byte>, void()> buffer;

    CHECK_EQ(buffer.Size(), 0);

    buffer.Push(SimpleCallable{1});
    CHECK_EQ(buffer.Size(), 1);

    buffer.Push(SimpleCallable{2});
    CHECK_EQ(buffer.Size(), 2);

    buffer.Push(SimpleCallable{3});
    CHECK_EQ(buffer.Size(), 3);

    buffer.Clear();
    CHECK_EQ(buffer.Size(), 0);
  }

  TEST_CASE("container::InlineCallableBuffer::GetAllocator: allocator access") {
    std::allocator<std::byte> alloc;
    InlineCallableBuffer<std::allocator<std::byte>, void()> buffer(alloc);

    auto retrieved_alloc = buffer.GetAllocator();
    CHECK(retrieved_alloc == alloc);
  }

  TEST_CASE("container::PmrInlineCallableBuffer: works with memory_resource") {
    InvocationTracker::Reset();

    std::byte storage[2048];
    std::pmr::monotonic_buffer_resource resource(storage, sizeof(storage));

    PmrInlineCallableBuffer<void()> buffer{&resource};
    buffer.Push(SimpleCallable{7});
    buffer.Invoke();

    REQUIRE_EQ(InvocationTracker::call_order.size(), 1);
    CHECK_EQ(InvocationTracker::call_order[0], 7);
  }

  TEST_CASE("container::InlineCallableBuffer::non-trivial callable handling") {
    InvocationTracker::Reset();

    InlineCallableBuffer<std::allocator<std::byte>, void()> buffer;

    buffer.Push(NonTrivialCallable{"first", 1});
    buffer.Push(NonTrivialCallable{"second", 2});

    CHECK_EQ(buffer.Size(), 2);

    buffer.Invoke();

    CHECK_EQ(InvocationTracker::call_order.size(), 2);
    CHECK_EQ(InvocationTracker::call_order[0], 1);
    CHECK_EQ(InvocationTracker::call_order[1], 2);
  }

  TEST_CASE("container::InlineCallableBuffer::destructor is called") {
    CountingCallable::Reset();

    {
      InlineCallableBuffer<std::allocator<std::byte>, void()> buffer;
      buffer.Push(CountingCallable{1});
      buffer.Push(CountingCallable{2});

      CHECK_GE(CountingCallable::construct_count, 2);
    }

    // After buffer goes out of scope, destructors should be called
    CHECK_EQ(CountingCallable::construct_count,
             CountingCallable::destruct_count);
  }

  TEST_CASE("container::InlineCallableBuffer::stateful callable") {
    int counter = 0;

    {
      InlineCallableBuffer<std::allocator<std::byte>, void()> buffer;
      buffer.Push(StatefulCallable{counter});
      buffer.Push(StatefulCallable{counter});
      buffer.Push(StatefulCallable{counter});

      buffer.Invoke();
    }

    CHECK_EQ(counter, 3);
  }

  TEST_CASE("container::InlineCallableBuffer::large number of callables") {
    InvocationTracker::Reset();

    InlineCallableBuffer<std::allocator<std::byte>, void()> buffer;

    constexpr int kCount = 100;
    for (int i = 0; i < kCount; ++i) {
      buffer.Push(SimpleCallable{i});
    }

    CHECK_EQ(buffer.Size(), kCount);

    buffer.Invoke();

    CHECK_EQ(InvocationTracker::call_order.size(), kCount);
    for (int i = 0; i < kCount; ++i) {
      CHECK_EQ(InvocationTracker::call_order[i], i);
    }
  }

  TEST_CASE("container::InlineCallableBuffer::swap friend function") {
    InvocationTracker::Reset();

    InlineCallableBuffer<std::allocator<std::byte>, void()> buffer1;
    buffer1.Push(SimpleCallable{1});

    InlineCallableBuffer<std::allocator<std::byte>, void()> buffer2;
    buffer2.Push(SimpleCallable{2});

    swap(buffer1, buffer2);

    buffer1.Invoke();
    CHECK_EQ(InvocationTracker::call_order.size(), 1);
    CHECK_EQ(InvocationTracker::call_order[0], 2);
  }

  TEST_CASE("container::InlineCallableBuffer::invocation order is preserved") {
    InvocationTracker::Reset();

    InlineCallableBuffer<std::allocator<std::byte>, void()> buffer;

    // Push in specific order
    buffer.Push(SimpleCallable{100});
    buffer.Push(SimpleCallable{200});
    buffer.Push(SimpleCallable{300});
    buffer.Push(SimpleCallable{400});
    buffer.Push(SimpleCallable{500});

    buffer.Invoke();

    CHECK_EQ(InvocationTracker::call_order.size(), 5);
    CHECK_EQ(InvocationTracker::call_order[0], 100);
    CHECK_EQ(InvocationTracker::call_order[1], 200);
    CHECK_EQ(InvocationTracker::call_order[2], 300);
    CHECK_EQ(InvocationTracker::call_order[3], 400);
    CHECK_EQ(InvocationTracker::call_order[4], 500);
  }

  TEST_CASE("container::InlineCallableBuffer::multiple invocations") {
    InvocationTracker::Reset();

    InlineCallableBuffer<std::allocator<std::byte>, void()> buffer;
    buffer.Push(SimpleCallable{1});
    buffer.Push(SimpleCallable{2});

    buffer.Invoke();
    CHECK_EQ(InvocationTracker::call_order.size(), 2);

    buffer.Invoke();
    CHECK_EQ(InvocationTracker::call_order.size(), 4);

    buffer.Invoke();
    CHECK_EQ(InvocationTracker::call_order.size(), 6);
  }

  TEST_CASE("container::InlineCallableBuffer::mixed callable types") {
    InvocationTracker::Reset();

    InlineCallableBuffer<std::allocator<std::byte>, void()> buffer;

    // Different callable types that all satisfy the same signature
    buffer.Push(SimpleCallable{1});
    buffer.Push(NonTrivialCallable{"test", 2});

    CHECK_EQ(buffer.Size(), 2);

    buffer.Invoke();

    CHECK_EQ(InvocationTracker::call_order.size(), 2);
    CHECK_EQ(InvocationTracker::call_order[0], 1);
    CHECK_EQ(InvocationTracker::call_order[1], 2);
  }

  TEST_CASE("container::InlineCallableBuffer::free function callables") {
    InvocationTracker::Reset();

    InlineCallableBuffer<std::allocator<std::byte>, void(int&)> buffer;

    buffer.Push(&increment);

    CHECK_EQ(buffer.Size(), 1);

    int value = 0;
    buffer.Invoke(value);

    CHECK_EQ(value, 1);
    CHECK_EQ(InvocationTracker::call_order.size(),
             0);  // No tracked calls for free functions
  }

  TEST_CASE(
      "container::InlineCallableBuffer::alias deduction with signatures only") {
    InvocationTracker::Reset();

    // Default allocator deduction: all args are void(Args...) signatures
    InlineCallableBuffer<void()> buffer;

    buffer.Push(SimpleCallable{1});
    buffer.Push(SimpleCallable{2});

    CHECK_EQ(buffer.Size(), 2);
    CHECK_FALSE(buffer.Empty());

    buffer.Invoke();

    CHECK_EQ(InvocationTracker::call_order.size(), 2);
    CHECK_EQ(InvocationTracker::call_order[0], 1);
    CHECK_EQ(InvocationTracker::call_order[1], 2);
  }

  TEST_CASE(
      "container::InlineCallableBuffer::alias deduction with multiple "
      "signatures") {
    InvocationTracker::Reset();

    // Multiple signatures with default allocator
    InlineCallableBuffer<void(), void(int)> buffer;

    buffer.Push(MultiSignatureCallable{1});

    CHECK_EQ(buffer.Size(), 1);

    buffer.template Invoke<0>();
    CHECK_EQ(InvocationTracker::call_order.size(), 1);
    CHECK_EQ(InvocationTracker::call_order[0], 10);

    InvocationTracker::Reset();

    buffer.template Invoke<1>(42);
    CHECK_EQ(InvocationTracker::call_order.size(), 1);
    CHECK_EQ(InvocationTracker::call_order[0], 142);
  }

  TEST_CASE(
      "container::InlineCallableBuffer::alias deduction with custom allocator "
      "and multiple signatures") {
    InvocationTracker::Reset();

    // Custom allocator with multiple signatures
    InlineCallableBuffer<std::allocator<std::byte>, void(), void(int)> buffer;

    buffer.Push(MultiSignatureCallable{5});

    CHECK_EQ(buffer.Size(), 1);

    buffer.template Invoke<0>();
    CHECK_EQ(InvocationTracker::call_order.size(), 1);
    CHECK_EQ(InvocationTracker::call_order[0], 50);
  }
}  // TEST_SUITE
