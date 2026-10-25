/*
 Copyright (C) 2023 Kristian Duske

 This file is part of TrenchBroom.

 TrenchBroom is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 TrenchBroom is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with TrenchBroom. If not, see <http://www.gnu.org/licenses/>.
 */

#include "Observer.h"
#include "base/Result.h"
#include "gl/MockTaskRunner.h"
#include "gl/Resource.h"
#include "gl/ResourceManager.h"
#include "gl/TestGl.h"

#include "kd/ranges/to.h"
#include "kd/reflection_impl.h"

#include <cstdlib>
#include <iostream>
#include <ranges>
#include <thread>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

namespace tb::gl
{
namespace
{

struct MockResource
{
  void upload(Gl& gl) const { mockUpload(gl); }
  void drop(Gl& gl) const { mockDrop(gl); }

  std::function<void(Gl&)> mockUpload = [](auto&) {};
  std::function<void(Gl&)> mockDrop = [](auto&) {};

  kdl_reflect_inline_empty(MockResource);
};

using ResourceT = Resource<MockResource>;
using ResourceWrapperT = ResourceWrapper<MockResource>;

bool operator==(
  const std::vector<const ResourceWrapperBase*>& lhs,
  const std::vector<std::shared_ptr<ResourceT>>& rhs)
{
  if (lhs.size() != rhs.size())
  {
    return false;
  }

  for (size_t i = 0; i < lhs.size(); ++i)
  {
    const auto* lhsCast = dynamic_cast<const ResourceWrapperT*>(lhs[i]);

    if (!lhsCast || *lhsCast != ResourceWrapperT{rhs[i]})
    {
      return false;
    }
  }

  return true;
}

} // namespace

TEST_CASE("ResourceManager")
{
  const auto mockResourceLoader = [&]() { return Result<MockResource>{MockResource{}}; };

  auto mockTaskRunner = MockTaskRunner{};
  auto taskRunner = [&](auto task) { return mockTaskRunner.run(std::move(task)); };

  auto testGl = TestGl{};

  const auto processContext = ProcessContext{testGl, [](auto, auto) {}};

  auto resourceManager = ResourceManager{};

  SECTION("unused deferred catalogs queue nothing and only requested entries load")
  {
    auto decoded = 0;
    auto catalog = std::vector<std::shared_ptr<ResourceT>>{};
    for (auto i = 0; i < 100; ++i)
    {
      auto resource = std::make_shared<ResourceT>([&]() -> Result<MockResource> {
        ++decoded;
        return MockResource{};
      });
      resource->setLoadingPolicy(ResourceLoadingPolicy::OnDemand);
      resourceManager.addResource(resource);
      catalog.push_back(std::move(resource));
    }
    REQUIRE_FALSE(resourceManager.needsProcessing());
    resourceManager.process(taskRunner, processContext);
    CHECK(mockTaskRunner.tasks.empty());
    CHECK(decoded == 0);
    catalog[3]->requestLoading();
    catalog[97]->requestLoading();
    REQUIRE(resourceManager.needsProcessing());
    resourceManager.process(taskRunner, processContext);
    REQUIRE(mockTaskRunner.tasks.size() == 2);
    mockTaskRunner.resolveNextPromise();
    mockTaskRunner.resolveNextPromise();
    resourceManager.process(taskRunner, processContext);
    CHECK(decoded == 2);
    CHECK(catalog[3]->isLoaded());
    CHECK(catalog[97]->isLoaded());
    CHECK_FALSE(catalog[0]->isLoaded());
    CHECK_FALSE(catalog[99]->isLoaded());
  }

  SECTION("budgeted processing resumes beyond a busy prefix")
  {
    using namespace std::chrono_literals;
    auto frontUploads = 0;
    auto tailUploads = 0;
    const auto slowUpload = [&](Gl&) {
      ++frontUploads;
      std::this_thread::sleep_for(5ms);
    };
    auto front = std::make_shared<ResourceT>(MockResource{slowUpload});
    auto tail = std::make_shared<ResourceT>(MockResource{[&](Gl&) { ++tailUploads; }});
    resourceManager.addResource(front);
    resourceManager.addResource(tail);
    resourceManager.process(taskRunner, processContext, 1ms);
    REQUIRE(frontUploads == 1);
    REQUIRE(tailUploads == 0);

    // Keep work at the front of the queue. Restarting each tick at index zero
    // must not prevent later requested textures from ever reaching the GPU.
    *front = ResourceT{MockResource{slowUpload}};
    resourceManager.process(taskRunner, processContext, 1ms);
    CHECK(tailUploads == 1);
    CHECK(frontUploads == 2);
  }

  SECTION("budget cursor survives removal before it and newly appended resources")
  {
    using namespace std::chrono_literals;
    auto uploads = 0;
    auto front = std::make_shared<ResourceT>(
      MockResource{[](Gl&) { std::this_thread::sleep_for(5ms); }});
    auto tail = std::make_shared<ResourceT>(MockResource{[&](Gl&) { ++uploads; }});
    resourceManager.addResource(front);
    resourceManager.addResource(tail);
    resourceManager.process(taskRunner, processContext, 1ms);
    REQUIRE(uploads == 0);
    front.reset();
    resourceManager.process(taskRunner, processContext);
    REQUIRE(uploads == 1);
    CHECK(resourceManager.resources() == std::vector{tail});
    auto appended = std::make_shared<ResourceT>(MockResource{[&](Gl&) { ++uploads; }});
    resourceManager.addResource(appended);
    resourceManager.process(taskRunner, processContext);
    CHECK(uploads == 2);
    CHECK_FALSE(resourceManager.needsProcessing());
  }

  SECTION("unused on-demand catalog retires without decoding")
  {
    const auto* requestedCount = std::getenv("SOURCEBROOM_RESOURCE_GC_COUNT");
    const auto count = requestedCount ? std::atoi(requestedCount) : 4000;
    REQUIRE(count >= 1000);
    REQUIRE(count <= 100000);
    auto decoded = 0;
    for (auto i = 0; i < count; ++i)
    {
      auto resource = std::make_shared<ResourceT>(ResourceLoader<MockResource>{[&]() {
        ++decoded;
        return Result<MockResource>{MockResource{}};
      }});
      resource->setLoadingPolicy(ResourceLoadingPolicy::OnDemand);
      resourceManager.addResource(std::move(resource));
    }
    REQUIRE(resourceManager.resources().size() == size_t(count));
    const auto start = std::chrono::steady_clock::now();
    resourceManager.process(taskRunner, processContext);
    const auto elapsed =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start)
        .count();
    CHECK(resourceManager.resources().empty());
    CHECK(decoded == 0);
    if (requestedCount)
    {
      std::cout << "RESOURCE_GC resources=" << count << " elapsed_ms=" << elapsed
                << " decoded=" << decoded << '\n';
    }
  }

  SECTION("needsProcessing")
  {
    CHECK(!resourceManager.needsProcessing());

    auto resource1 = std::make_shared<ResourceT>(mockResourceLoader);
    resourceManager.addResource(resource1);

    REQUIRE(std::holds_alternative<ResourceUnloaded<MockResource>>(resource1->state()));
    CHECK(resourceManager.needsProcessing());

    resourceManager.process(taskRunner, processContext);
    REQUIRE(std::holds_alternative<ResourceLoading<MockResource>>(resource1->state()));
    CHECK(resourceManager.needsProcessing());

    mockTaskRunner.resolveNextPromise();
    resourceManager.process(taskRunner, processContext);
    REQUIRE(std::holds_alternative<ResourceLoaded<MockResource>>(resource1->state()));
    CHECK(resourceManager.needsProcessing());

    resourceManager.process(taskRunner, processContext);
    REQUIRE(std::holds_alternative<ResourceReady<MockResource>>(resource1->state()));
    CHECK(!resourceManager.needsProcessing());

    auto resource2 = std::make_shared<ResourceT>(mockResourceLoader);
    resourceManager.addResource(resource2);
    REQUIRE(std::holds_alternative<ResourceReady<MockResource>>(resource1->state()));
    REQUIRE(std::holds_alternative<ResourceUnloaded<MockResource>>(resource2->state()));
    CHECK(resourceManager.needsProcessing());

    resourceManager.process(taskRunner, processContext);
    REQUIRE(std::holds_alternative<ResourceReady<MockResource>>(resource1->state()));
    REQUIRE(std::holds_alternative<ResourceLoading<MockResource>>(resource2->state()));
    CHECK(resourceManager.needsProcessing());

    mockTaskRunner.resolveNextPromise();
    resourceManager.process(taskRunner, processContext);
    REQUIRE(std::holds_alternative<ResourceReady<MockResource>>(resource1->state()));
    REQUIRE(std::holds_alternative<ResourceLoaded<MockResource>>(resource2->state()));
    CHECK(resourceManager.needsProcessing());

    resourceManager.process(taskRunner, processContext);
    REQUIRE(std::holds_alternative<ResourceReady<MockResource>>(resource1->state()));
    REQUIRE(std::holds_alternative<ResourceReady<MockResource>>(resource2->state()));
    CHECK(!resourceManager.needsProcessing());

    resource1.reset();
    REQUIRE(std::holds_alternative<ResourceReady<MockResource>>(resource2->state()));
    CHECK(resourceManager.needsProcessing());

    resourceManager.process(taskRunner, processContext);
    REQUIRE(std::holds_alternative<ResourceReady<MockResource>>(resource2->state()));
    CHECK(!resourceManager.needsProcessing());

    resource2.reset();
    CHECK(resourceManager.needsProcessing());

    resourceManager.process(taskRunner, processContext);
    CHECK(!resourceManager.needsProcessing());
  }

  SECTION("addResource")
  {
    auto resource1 = std::make_shared<ResourceT>(mockResourceLoader);
    resourceManager.addResource(resource1);

    CHECK(resourceManager.resources() == std::vector{resource1});
    CHECK(resource1.use_count() == 2);
    CHECK(std::holds_alternative<ResourceUnloaded<MockResource>>(resource1->state()));

    auto resource2 = std::make_shared<ResourceT>(mockResourceLoader);
    resourceManager.addResource(resource2);

    CHECK(resourceManager.resources() == std::vector{resource1, resource2});
  }

  SECTION("process")
  {
    auto resourcesWereProcessed =
      Observer<std::vector<ResourceId>>{resourceManager.resourcesWereProcessedNotifier};

    SECTION("resource loading")
    {
      auto resource1 = std::make_shared<ResourceT>(mockResourceLoader);
      auto resource2 = std::make_shared<ResourceT>(mockResourceLoader);
      resourceManager.addResource(resource1);
      resourceManager.addResource(resource2);

      resourceManager.process(taskRunner, processContext);
      CHECK(
        resourcesWereProcessed.notifications
        == std::vector<std::vector<ResourceId>>{{{resource1->id(), resource2->id()}}});
      CHECK(std::holds_alternative<ResourceLoading<MockResource>>(resource1->state()));
      CHECK(std::holds_alternative<ResourceLoading<MockResource>>(resource2->state()));

      SECTION("resource1 finishes loading")
      {
        resourcesWereProcessed.reset();
        mockTaskRunner.resolveNextPromise();
        resourceManager.process(taskRunner, processContext);

        CHECK(
          resourcesWereProcessed.notifications
          == std::vector<std::vector<ResourceId>>{{resource1->id()}});
        CHECK(std::holds_alternative<ResourceLoaded<MockResource>>(resource1->state()));
        CHECK(std::holds_alternative<ResourceLoading<MockResource>>(resource2->state()));

        SECTION("resource2 finishes loading")
        {
          resourcesWereProcessed.reset();
          mockTaskRunner.resolveNextPromise();
          resourceManager.process(taskRunner, processContext);

          CHECK(
            resourcesWereProcessed.notifications
            == std::vector<std::vector<ResourceId>>{{resource1->id(), resource2->id()}});
          CHECK(std::holds_alternative<ResourceReady<MockResource>>(resource1->state()));
          CHECK(std::holds_alternative<ResourceLoaded<MockResource>>(resource2->state()));

          resourcesWereProcessed.reset();
          resourceManager.process(taskRunner, processContext);

          CHECK(
            resourcesWereProcessed.notifications
            == std::vector<std::vector<ResourceId>>{{resource2->id()}});
          CHECK(std::holds_alternative<ResourceReady<MockResource>>(resource1->state()));
          CHECK(std::holds_alternative<ResourceReady<MockResource>>(resource2->state()));
        }
      }

      SECTION("resource2 finishes loading")
      {
        resourcesWereProcessed.reset();
        mockTaskRunner.resolveLastPromise();
        resourceManager.process(taskRunner, processContext);

        CHECK(
          resourcesWereProcessed.notifications
          == std::vector<std::vector<ResourceId>>{{resource2->id()}});
        CHECK(std::holds_alternative<ResourceLoading<MockResource>>(resource1->state()));
        CHECK(std::holds_alternative<ResourceLoaded<MockResource>>(resource2->state()));

        SECTION("resource1 finishes loading")
        {
          resourcesWereProcessed.reset();
          mockTaskRunner.resolveLastPromise();
          resourceManager.process(taskRunner, processContext);

          CHECK(
            resourcesWereProcessed.notifications
            == std::vector<std::vector<ResourceId>>{{resource1->id(), resource2->id()}});
          CHECK(std::holds_alternative<ResourceLoaded<MockResource>>(resource1->state()));
          CHECK(std::holds_alternative<ResourceReady<MockResource>>(resource2->state()));

          resourcesWereProcessed.reset();
          resourceManager.process(taskRunner, processContext);

          CHECK(
            resourcesWereProcessed.notifications
            == std::vector<std::vector<ResourceId>>{{resource1->id()}});
          CHECK(std::holds_alternative<ResourceReady<MockResource>>(resource1->state()));
          CHECK(std::holds_alternative<ResourceReady<MockResource>>(resource2->state()));
        }
      }
    }

    SECTION("dropping resources")
    {
      auto mockDropCalls = std::array{false, false};
      auto sharedResources = std::array{
        std::make_shared<ResourceT>([&]() {
          return Result<MockResource>{MockResource{
            [](const auto&) {},
            [&](const auto&) { mockDropCalls[0] = true; },
          }};
        }),
        std::make_shared<ResourceT>([&]() {
          return Result<MockResource>{MockResource{
            [](const auto&) {},
            [&](const auto&) { mockDropCalls[1] = true; },
          }};
        }),
      };

      const auto resourceIds =
        sharedResources
        | std::views::transform([](const auto& resource) { return resource->id(); })
        | kdl::ranges::to<std::vector>();

      resourceManager.addResource(sharedResources[0]);
      resourceManager.addResource(sharedResources[1]);

      resourceManager.process(taskRunner, processContext);
      mockTaskRunner.resolveNextPromise();
      mockTaskRunner.resolveNextPromise();
      resourceManager.process(taskRunner, processContext);
      resourceManager.process(taskRunner, processContext);
      REQUIRE(
        std::holds_alternative<ResourceReady<MockResource>>(sharedResources[0]->state()));
      REQUIRE(
        std::holds_alternative<ResourceReady<MockResource>>(sharedResources[1]->state()));

      sharedResources[0].reset();
      CHECK(resourceManager.resources().size() == 2);

      resourcesWereProcessed.reset();
      resourceManager.process(taskRunner, processContext);

      CHECK(
        resourcesWereProcessed.notifications
        == std::vector<std::vector<ResourceId>>{{resourceIds[0]}});
      CHECK(resourceManager.resources() == std::vector{sharedResources[1]});
      CHECK(mockDropCalls[0]);

      sharedResources[1].reset();
      CHECK(resourceManager.resources().size() == 1);

      resourcesWereProcessed.reset();
      resourceManager.process(taskRunner, processContext);

      CHECK(
        resourcesWereProcessed.notifications
        == std::vector<std::vector<ResourceId>>{{resourceIds[1]}});
      CHECK(resourceManager.resources().empty());
      CHECK(mockDropCalls[1]);
    }
  }
}

} // namespace tb::gl
