#include <mbgl/storage/asset_file_source.hpp>
#include <mbgl/platform/platform.hpp>
#include <mbgl/util/chrono.hpp>
#include <mbgl/util/run_loop.hpp>
#include <mbgl/util/thread.hpp>

#include <gtest/gtest.h>

namespace {

std::string getFileSourceRoot() {
#ifdef MBGL_ASSET_ZIP
    // Regenerate with `cd test/fixtures/storage/ && zip -r assets.zip assets/`
    return "test/fixtures/storage/assets.zip";
#else
    return "test/fixtures/storage/assets";
#endif
}

} // namespace

using namespace mbgl;

TEST(AssetFileSource, Load) {
    util::RunLoop loop;

    AssetFileSource fs(getFileSourceRoot());

    // iOS seems to run out of file descriptors...
#if TARGET_OS_IPHONE
    unsigned numThreads = 30;
#else
    unsigned numThreads = 50;
#endif

    auto callback = [&] {
        if (!--numThreads) {
            loop.stop();
        }
    };

    class TestWorker {
    public:
        TestWorker(mbgl::AssetFileSource* fs_) : fs(fs_) {}

        void run(std::function<void()> endCallback) {
            const std::string asset("asset://nonempty");

            requestCallback = [this, asset, endCallback](mbgl::Response res) {
                EXPECT_EQ(nullptr, res.error);
                ASSERT_TRUE(res.data.get());
                EXPECT_EQ("content is here\n", *res.data);

                if (!--numRequests) {
                    endCallback();
                    request.reset();
                } else {
                    request = fs->request({ mbgl::Resource::Unknown, asset }, requestCallback);
                }
            };

            request = fs->request({ mbgl::Resource::Unknown, asset }, requestCallback);
        }

    private:
        unsigned numRequests = 1000;

        mbgl::AssetFileSource* fs;
        std::unique_ptr<mbgl::AsyncRequest> request;

        std::function<void(mbgl::Response)> requestCallback;
    };

    std::vector<std::unique_ptr<util::Thread<TestWorker>>> threads;
    std::vector<std::unique_ptr<mbgl::AsyncRequest>> requests;
    util::ThreadContext context = { "Test" };

    for (unsigned i = 0; i < numThreads; ++i) {
        std::unique_ptr<util::Thread<TestWorker>> thread =
            std::make_unique<util::Thread<TestWorker>>(context, &fs);

        requests.push_back(
            thread->invokeWithCallback(&TestWorker::run, callback));

        threads.push_back(std::move(thread));
    }

    loop.run();
}

TEST(AssetFileSource, EmptyFile) {
    util::RunLoop loop;

    AssetFileSource fs(getFileSourceRoot());

    std::unique_ptr<AsyncRequest> req = fs.request({ Resource::Unknown, "asset://empty" }, [&](Response res) {
        req.reset();
        EXPECT_EQ(nullptr, res.error);
        ASSERT_TRUE(res.data.get());
        EXPECT_EQ("", *res.data);
        loop.stop();
    });

    loop.run();
}

TEST(AssetFileSource, NonEmptyFile) {
    util::RunLoop loop;

    AssetFileSource fs(getFileSourceRoot());

    std::unique_ptr<AsyncRequest> req = fs.request({ Resource::Unknown, "asset://nonempty" }, [&](Response res) {
        req.reset();
        EXPECT_EQ(nullptr, res.error);
        ASSERT_TRUE(res.data.get());
        EXPECT_EQ("content is here\n", *res.data);
        loop.stop();
    });

    loop.run();
}

TEST(AssetFileSource, NonExistentFile) {
    util::RunLoop loop;

    AssetFileSource fs(getFileSourceRoot());

    std::unique_ptr<AsyncRequest> req = fs.request({ Resource::Unknown, "asset://does_not_exist" }, [&](Response res) {
        req.reset();
        ASSERT_NE(nullptr, res.error);
        EXPECT_EQ(Response::Error::Reason::NotFound, res.error->reason);
        ASSERT_FALSE(res.data.get());
        // Do not assert on platform-specific error message.
        loop.stop();
    });

    loop.run();
}

TEST(AssetFileSource, ReadDirectory) {
    util::RunLoop loop;

    AssetFileSource fs(getFileSourceRoot());

    std::unique_ptr<AsyncRequest> req = fs.request({ Resource::Unknown, "asset://directory" }, [&](Response res) {
        req.reset();
        ASSERT_NE(nullptr, res.error);
        EXPECT_EQ(Response::Error::Reason::NotFound, res.error->reason);
        ASSERT_FALSE(res.data.get());
        // Do not assert on platform-specific error message.
        loop.stop();
    });

    loop.run();
}

TEST(AssetFileSource, URLEncoding) {
    util::RunLoop loop;

    AssetFileSource fs(getFileSourceRoot());

    std::unique_ptr<AsyncRequest> req = fs.request({ Resource::Unknown, "asset://%6eonempty" }, [&](Response res) {
        req.reset();
        EXPECT_EQ(nullptr, res.error);
        ASSERT_TRUE(res.data.get());
        EXPECT_EQ("content is here\n", *res.data);
        loop.stop();
    });

    loop.run();
}
