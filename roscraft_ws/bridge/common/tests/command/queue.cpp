#include <doctest/doctest.h>

#include <roscraft/bridge/command/queue.hpp>

#include <array>
#include <concepts>
#include <cstdint>
#include <iterator>
#include <type_traits>
#include <utility>
#include <vector>

using namespace roscraft::bridge;

namespace {

struct TestCmd {
  uint64_t value = 0;
};

struct OtherCmd {
  uint64_t value = 0;
};

static_assert(CommandTrait<TestCmd>);
static_assert(CommandTrait<OtherCmd>);

}  // namespace

TEST_SUITE("bridge::details::CommandStorage") {
  TEST_CASE("bridge::details::CommandStorage virtual interface") {
    static_assert(std::has_virtual_destructor_v<details::CommandStorage>);

    TypedCommandStorage<TestCmd> typed;
    details::CommandStorage& storage = typed;

    CHECK(storage.Empty());
    CHECK_EQ(storage.SizeApprox(), 0U);

    typed.Enqueue(TestCmd{.value = 1});
    CHECK_FALSE(storage.Empty());
    CHECK_EQ(storage.SizeApprox(), 1U);

    storage.Clear();
    CHECK(storage.Empty());
    CHECK_EQ(storage.SizeApprox(), 0U);
  }
}

TEST_SUITE("bridge::TypedCommandStorage") {
  TEST_CASE("bridge::TypedCommandStorage::ctor") {
    static_assert(std::default_initializable<TypedCommandStorage<TestCmd>>);
    static_assert(!std::copy_constructible<TypedCommandStorage<TestCmd>>);
    static_assert(std::move_constructible<TypedCommandStorage<TestCmd>>);

    TypedCommandStorage<TestCmd> storage;
    CHECK(storage.Empty());
    CHECK_EQ(storage.SizeApprox(), 0U);
  }

  TEST_CASE("bridge::TypedCommandStorage::operator=") {
    static_assert(!std::is_copy_assignable_v<TypedCommandStorage<TestCmd>>);
    static_assert(std::movable<TypedCommandStorage<TestCmd>>);

    TypedCommandStorage<TestCmd> source;
    source.Enqueue(TestCmd{.value = 7});

    TypedCommandStorage<TestCmd> target;
    target = std::move(source);

    CHECK_FALSE(target.Empty());
    CHECK_EQ(target.Dequeue().value, 7U);
  }

  TEST_CASE("bridge::TypedCommandStorage::Clear") {
    TypedCommandStorage<TestCmd> storage;
    storage.Enqueue(TestCmd{.value = 1});
    storage.Enqueue(TestCmd{.value = 2});

    storage.Clear();

    CHECK(storage.Empty());
    CHECK_EQ(storage.SizeApprox(), 0U);
  }

  TEST_CASE("bridge::TypedCommandStorage::Enqueue(const T&)") {
    TypedCommandStorage<TestCmd> storage;
    const TestCmd cmd{.value = 11};

    storage.Enqueue(cmd);

    CHECK_FALSE(storage.Empty());
    CHECK_EQ(storage.Dequeue().value, 11U);
  }

  TEST_CASE("bridge::TypedCommandStorage::Enqueue(T&&)") {
    TypedCommandStorage<TestCmd> storage;

    storage.Enqueue(TestCmd{.value = 12});

    CHECK_FALSE(storage.Empty());
    CHECK_EQ(storage.Dequeue().value, 12U);
  }

  TEST_CASE(
      "bridge::TypedCommandStorage::Enqueue(CommandQueueProducerToken&, "
      "const T&)") {
    TypedCommandStorage<TestCmd> storage;
    CommandQueueProducerToken token = storage.MakeProducerToken();
    const TestCmd cmd{.value = 13};

    storage.Enqueue(token, cmd);

    CHECK_FALSE(storage.Empty());
    CHECK_EQ(storage.Dequeue().value, 13U);
  }

  TEST_CASE(
      "bridge::TypedCommandStorage::Enqueue(CommandQueueProducerToken&, "
      "T&&)") {
    TypedCommandStorage<TestCmd> storage;
    CommandQueueProducerToken token = storage.MakeProducerToken();

    storage.Enqueue(token, TestCmd{.value = 14});

    CHECK_FALSE(storage.Empty());
    CHECK_EQ(storage.Dequeue().value, 14U);
  }

  TEST_CASE("bridge::TypedCommandStorage::EnqueueBulk(R&&)") {
    TypedCommandStorage<TestCmd> storage;
    std::vector<TestCmd> values{{.value = 1}, {.value = 2}, {.value = 3}};

    storage.EnqueueBulk(values);

    CHECK_EQ(storage.SizeApprox(), 3U);
    CHECK_EQ(storage.Dequeue().value, 1U);
    CHECK_EQ(storage.Dequeue().value, 2U);
    CHECK_EQ(storage.Dequeue().value, 3U);
  }

  TEST_CASE(
      "bridge::TypedCommandStorage::EnqueueBulk(CommandQueueProducerToken&, "
      "R&&)") {
    TypedCommandStorage<TestCmd> storage;
    CommandQueueProducerToken token = storage.MakeProducerToken();
    std::array<TestCmd, 3> values{{{.value = 4}, {.value = 5}, {.value = 6}}};

    storage.EnqueueBulk(token, values);

    CHECK_EQ(storage.SizeApprox(), 3U);
    CHECK_EQ(storage.Dequeue().value, 4U);
    CHECK_EQ(storage.Dequeue().value, 5U);
    CHECK_EQ(storage.Dequeue().value, 6U);
  }

  TEST_CASE("bridge::TypedCommandStorage::Dequeue()") {
    SUBCASE("Returns default-constructed value when empty") {
      TypedCommandStorage<TestCmd> storage;
      CHECK_EQ(storage.Dequeue().value, 0U);
    }

    SUBCASE("Returns queued value when not empty") {
      TypedCommandStorage<TestCmd> storage;
      storage.Enqueue(TestCmd{.value = 21});
      CHECK_EQ(storage.Dequeue().value, 21U);
    }
  }

  TEST_CASE(
      "bridge::TypedCommandStorage::Dequeue(CommandQueueConsumerToken&)") {
    SUBCASE("Returns default-constructed value when empty") {
      TypedCommandStorage<TestCmd> storage;
      CommandQueueConsumerToken token = storage.MakeConsumerToken();
      CHECK_EQ(storage.Dequeue(token).value, 0U);
    }

    SUBCASE("Returns queued value when not empty") {
      TypedCommandStorage<TestCmd> storage;
      CommandQueueConsumerToken token = storage.MakeConsumerToken();
      storage.Enqueue(TestCmd{.value = 22});
      CHECK_EQ(storage.Dequeue(token).value, 22U);
    }
  }

  TEST_CASE("bridge::TypedCommandStorage::Dequeue(T&)") {
    TypedCommandStorage<TestCmd> storage;

    TestCmd output{.value = 99};
    CHECK_FALSE(storage.Dequeue(output));
    CHECK_EQ(output.value, 99U);

    storage.Enqueue(TestCmd{.value = 23});
    CHECK(storage.Dequeue(output));
    CHECK_EQ(output.value, 23U);
  }

  TEST_CASE(
      "bridge::TypedCommandStorage::Dequeue(CommandQueueConsumerToken&, "
      "T&)") {
    TypedCommandStorage<TestCmd> storage;
    CommandQueueConsumerToken token = storage.MakeConsumerToken();

    TestCmd output{.value = 88};
    CHECK_FALSE(storage.Dequeue(token, output));
    CHECK_EQ(output.value, 88U);

    storage.Enqueue(TestCmd{.value = 24});
    CHECK(storage.Dequeue(token, output));
    CHECK_EQ(output.value, 24U);
  }

  TEST_CASE("bridge::TypedCommandStorage::Into(It, size_t)") {
    TypedCommandStorage<TestCmd> storage;
    storage.Enqueue(TestCmd{.value = 30});
    storage.Enqueue(TestCmd{.value = 31});
    storage.Enqueue(TestCmd{.value = 32});

    std::vector<TestCmd> out;
    const size_t moved = storage.Into(std::back_inserter(out), 2);

    CHECK_EQ(moved, 2U);
    CHECK_EQ(out.size(), 2U);
    CHECK_EQ(out[0].value, 30U);
    CHECK_EQ(out[1].value, 31U);
    CHECK_EQ(storage.Dequeue().value, 32U);
  }

  TEST_CASE(
      "bridge::TypedCommandStorage::Into(CommandQueueConsumerToken&, It, "
      "size_t)") {
    TypedCommandStorage<TestCmd> storage;
    CommandQueueConsumerToken token = storage.MakeConsumerToken();
    storage.Enqueue(TestCmd{.value = 40});
    storage.Enqueue(TestCmd{.value = 41});

    std::vector<TestCmd> out;
    const size_t moved = storage.Into(token, std::back_inserter(out), 10);

    CHECK_EQ(moved, 2U);
    CHECK_EQ(out.size(), 2U);
    CHECK_EQ(out[0].value, 40U);
    CHECK_EQ(out[1].value, 41U);
    CHECK(storage.Empty());
  }

  TEST_CASE("bridge::TypedCommandStorage::MakeProducerToken") {
    TypedCommandStorage<TestCmd> storage;
    CommandQueueProducerToken token = storage.MakeProducerToken();

    storage.Enqueue(token, TestCmd{.value = 50});

    CHECK_EQ(storage.Dequeue().value, 50U);
  }

  TEST_CASE("bridge::TypedCommandStorage::MakeConsumerToken") {
    TypedCommandStorage<TestCmd> storage;
    CommandQueueConsumerToken token = storage.MakeConsumerToken();
    storage.Enqueue(TestCmd{.value = 60});

    CHECK_EQ(storage.Dequeue(token).value, 60U);
  }

  TEST_CASE("bridge::TypedCommandStorage::Empty") {
    TypedCommandStorage<TestCmd> storage;
    CHECK(storage.Empty());

    storage.Enqueue(TestCmd{.value = 70});
    CHECK_FALSE(storage.Empty());
  }

  TEST_CASE("bridge::TypedCommandStorage::SizeApprox") {
    TypedCommandStorage<TestCmd> storage;
    CHECK_EQ(storage.SizeApprox(), 0U);

    storage.Enqueue(TestCmd{.value = 71});
    storage.Enqueue(TestCmd{.value = 72});
    CHECK_EQ(storage.SizeApprox(), 2U);
  }
}

TEST_SUITE("bridge::CommandQueue") {
  TEST_CASE("bridge::CommandQueue::ctor") {
    static_assert(std::default_initializable<CommandQueue>);
    static_assert(!std::copy_constructible<CommandQueue>);
    static_assert(std::move_constructible<CommandQueue>);

    CommandQueue queue;
    CHECK_EQ(queue.TypeCount(), 0U);
    CHECK_FALSE(queue.HasCommands());
  }

  TEST_CASE("bridge::CommandQueue::operator=") {
    static_assert(!std::is_copy_assignable_v<CommandQueue>);
    static_assert(std::movable<CommandQueue>);

    CommandQueue source;
    source.Register<TestCmd>();
    source.Enqueue(TestCmd{.value = 81});

    CommandQueue target;
    target = std::move(source);

    CHECK(target.IsRegistered<TestCmd>());
    CHECK(target.HasCommands<TestCmd>());
    CHECK_EQ(target.TypedStorage<TestCmd>().Dequeue().value, 81U);
  }

  TEST_CASE("bridge::CommandQueue::Register") {
    CommandQueue queue;

    queue.Register<TestCmd>();
    CHECK(queue.IsRegistered<TestCmd>());
    CHECK_EQ(queue.TypeCount(), 1U);

    queue.Register<TestCmd>();
    CHECK_EQ(queue.TypeCount(), 1U);
  }

  TEST_CASE("bridge::CommandQueue::Clear") {
    CommandQueue queue;
    queue.Register<TestCmd>();
    queue.Register<OtherCmd>();
    queue.Enqueue(TestCmd{.value = 1});
    queue.Enqueue(OtherCmd{.value = 2});

    queue.Clear();

    CHECK(queue.IsRegistered<TestCmd>());
    CHECK(queue.IsRegistered<OtherCmd>());
    CHECK_FALSE(queue.HasCommands());
    CHECK_EQ(queue.CommandCount(), 0U);
  }

  TEST_CASE("bridge::CommandQueue::Clear<T>") {
    CommandQueue queue;
    queue.Register<TestCmd>();
    queue.Register<OtherCmd>();
    queue.Enqueue(TestCmd{.value = 11});
    queue.Enqueue(OtherCmd{.value = 12});

    queue.Clear<TestCmd>();

    CHECK_FALSE(queue.HasCommands<TestCmd>());
    CHECK(queue.HasCommands<OtherCmd>());

    queue.Clear<OtherCmd>();
    CHECK_FALSE(queue.HasCommands<OtherCmd>());

    queue.Clear<OtherCmd>();
    CHECK_FALSE(queue.HasCommands<OtherCmd>());
  }

  TEST_CASE("bridge::CommandQueue::Reset") {
    CommandQueue queue;
    queue.Register<TestCmd>();
    queue.Enqueue(TestCmd{.value = 21});

    queue.Reset();

    CHECK_EQ(queue.TypeCount(), 0U);
    CHECK_FALSE(queue.IsRegistered<TestCmd>());
  }

  TEST_CASE("bridge::CommandQueue::Reset<T>") {
    CommandQueue queue;
    queue.Register<TestCmd>();
    queue.Register<OtherCmd>();
    queue.Enqueue(TestCmd{.value = 31});
    queue.Enqueue(OtherCmd{.value = 32});

    queue.Reset<TestCmd>();

    CHECK_FALSE(queue.IsRegistered<TestCmd>());
    CHECK(queue.IsRegistered<OtherCmd>());
    CHECK_EQ(queue.TypeCount(), 1U);

    queue.Reset<TestCmd>();
    CHECK_EQ(queue.TypeCount(), 1U);
  }

  TEST_CASE("bridge::CommandQueue::Enqueue(T&&)") {
    CommandQueue queue;
    queue.Register<TestCmd>();

    queue.Enqueue(TestCmd{.value = 41});

    CHECK(queue.HasCommands<TestCmd>());
    CHECK_EQ(queue.TypedStorage<TestCmd>().Dequeue().value, 41U);
  }

  TEST_CASE("bridge::CommandQueue::Enqueue(CommandQueueProducerToken&, T&&)") {
    CommandQueue queue;
    queue.Register<TestCmd>();
    CommandQueueProducerToken token = queue.MakeProducerToken<TestCmd>();

    queue.Enqueue(token, TestCmd{.value = 42});

    CHECK(queue.HasCommands<TestCmd>());
    CHECK_EQ(queue.TypedStorage<TestCmd>().Dequeue().value, 42U);
  }

  TEST_CASE("bridge::CommandQueue::EnqueueBulk(R&&)") {
    CommandQueue queue;
    queue.Register<TestCmd>();
    std::vector<TestCmd> values{{.value = 1}, {.value = 2}, {.value = 3}};

    queue.EnqueueBulk(values);

    CHECK_EQ(queue.CommandCount<TestCmd>(), 3U);
    CHECK_EQ(queue.TypedStorage<TestCmd>().Dequeue().value, 1U);
    CHECK_EQ(queue.TypedStorage<TestCmd>().Dequeue().value, 2U);
    CHECK_EQ(queue.TypedStorage<TestCmd>().Dequeue().value, 3U);
  }

  TEST_CASE(
      "bridge::CommandQueue::EnqueueBulk(CommandQueueProducerToken&, R&&)") {
    CommandQueue queue;
    queue.Register<TestCmd>();
    CommandQueueProducerToken token = queue.MakeProducerToken<TestCmd>();
    std::array<TestCmd, 2> values{{{.value = 4}, {.value = 5}}};

    queue.EnqueueBulk(token, values);

    CHECK_EQ(queue.CommandCount<TestCmd>(), 2U);
    CHECK_EQ(queue.TypedStorage<TestCmd>().Dequeue().value, 4U);
    CHECK_EQ(queue.TypedStorage<TestCmd>().Dequeue().value, 5U);
  }

  TEST_CASE("bridge::CommandQueue::MakeProducerToken<T>") {
    CommandQueue queue;
    queue.Register<TestCmd>();

    CommandQueueProducerToken token = queue.MakeProducerToken<TestCmd>();
    queue.Enqueue(token, TestCmd{.value = 51});

    CHECK(queue.HasCommands<TestCmd>());
  }

  TEST_CASE("bridge::CommandQueue::MakeConsumerToken<T>") {
    CommandQueue queue;
    queue.Register<TestCmd>();
    queue.Enqueue(TestCmd{.value = 52});

    CommandQueueConsumerToken token = queue.MakeConsumerToken<TestCmd>();
    CHECK_EQ(queue.TypedStorage<TestCmd>().Dequeue(token).value, 52U);
  }

  TEST_CASE("bridge::CommandQueue::Swap") {
    CommandQueue lhs;
    lhs.Register<TestCmd>();
    lhs.Enqueue(TestCmd{.value = 61});

    CommandQueue rhs;
    rhs.Register<OtherCmd>();
    rhs.Enqueue(OtherCmd{.value = 62});

    lhs.Swap(rhs);

    CHECK(lhs.IsRegistered<OtherCmd>());
    CHECK_FALSE(lhs.IsRegistered<TestCmd>());
    CHECK_EQ(lhs.TypedStorage<OtherCmd>().Dequeue().value, 62U);

    CHECK(rhs.IsRegistered<TestCmd>());
    CHECK_FALSE(rhs.IsRegistered<OtherCmd>());
    CHECK_EQ(rhs.TypedStorage<TestCmd>().Dequeue().value, 61U);
  }

  TEST_CASE("bridge::CommandQueue::swap") {
    CommandQueue lhs;
    lhs.Register<TestCmd>();
    lhs.Enqueue(TestCmd{.value = 71});

    CommandQueue rhs;
    rhs.Register<OtherCmd>();
    rhs.Enqueue(OtherCmd{.value = 72});

    swap(lhs, rhs);

    CHECK(lhs.IsRegistered<OtherCmd>());
    CHECK_EQ(lhs.TypedStorage<OtherCmd>().Dequeue().value, 72U);
    CHECK(rhs.IsRegistered<TestCmd>());
    CHECK_EQ(rhs.TypedStorage<TestCmd>().Dequeue().value, 71U);
  }

  TEST_CASE("bridge::CommandQueue::IsRegistered<T>") {
    CommandQueue queue;
    CHECK_FALSE(queue.IsRegistered<TestCmd>());

    queue.Register<TestCmd>();
    CHECK(queue.IsRegistered<TestCmd>());
    CHECK_FALSE(queue.IsRegistered<OtherCmd>());
  }

  TEST_CASE("bridge::CommandQueue::HasCommands") {
    CommandQueue queue;
    queue.Register<TestCmd>();
    queue.Register<OtherCmd>();

    CHECK_FALSE(queue.HasCommands());

    queue.Enqueue(OtherCmd{.value = 91});
    CHECK(queue.HasCommands());

    queue.Clear();
    CHECK_FALSE(queue.HasCommands());
  }

  TEST_CASE("bridge::CommandQueue::HasCommands<T>") {
    CommandQueue queue;

    CHECK_FALSE(queue.HasCommands<TestCmd>());

    queue.Register<TestCmd>();
    CHECK_FALSE(queue.HasCommands<TestCmd>());

    queue.Enqueue(TestCmd{.value = 92});
    CHECK(queue.HasCommands<TestCmd>());

    queue.Clear<TestCmd>();
    CHECK_FALSE(queue.HasCommands<TestCmd>());
  }

  TEST_CASE("bridge::CommandQueue::TypeCount") {
    CommandQueue queue;
    CHECK_EQ(queue.TypeCount(), 0U);

    queue.Register<TestCmd>();
    CHECK_EQ(queue.TypeCount(), 1U);

    queue.Register<OtherCmd>();
    CHECK_EQ(queue.TypeCount(), 2U);

    queue.Reset<TestCmd>();
    CHECK_EQ(queue.TypeCount(), 1U);
  }

  TEST_CASE("bridge::CommandQueue::CommandCount") {
    CommandQueue queue;
    queue.Register<TestCmd>();
    queue.Register<OtherCmd>();

    CHECK_EQ(queue.CommandCount(), 0U);

    queue.Enqueue(TestCmd{.value = 101});
    queue.Enqueue(TestCmd{.value = 102});
    queue.Enqueue(OtherCmd{.value = 103});

    CHECK_EQ(queue.CommandCount(), 3U);
  }

  TEST_CASE("bridge::CommandQueue::CommandCount<T>") {
    CommandQueue queue;

    CHECK_EQ(queue.CommandCount<TestCmd>(), 0U);

    queue.Register<TestCmd>();
    queue.Enqueue(TestCmd{.value = 111});
    queue.Enqueue(TestCmd{.value = 112});

    CHECK_EQ(queue.CommandCount<TestCmd>(), 2U);
    CHECK_EQ(queue.CommandCount<OtherCmd>(), 0U);
  }

  TEST_CASE("bridge::CommandQueue::TypedStorage") {
    CommandQueue queue;
    queue.Register<TestCmd>();
    queue.Enqueue(TestCmd{.value = 121});

    auto& storage = queue.TypedStorage<TestCmd>();
    CHECK_EQ(storage.Dequeue().value, 121U);

    const auto& const_queue = queue;
    const auto& const_storage = const_queue.TypedStorage<TestCmd>();
    CHECK(const_storage.Empty());
  }
}
