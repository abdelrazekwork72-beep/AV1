#include "importer/async_importer.hpp"

#include <cassert>
#include <iostream>
#include <thread>
#include <vector>

using namespace openmedia::importer;

int main() {
    ImAsyncImporterCreationRecExt rec{};
    assert(AsyncImporterEngine::handleCreateAsyncImporter(&rec) == kImNoErr);
    assert(rec.outAsyncEntry != nullptr);
    assert(rec.outAsyncClose != nullptr);
    assert(rec.inPrivateData != nullptr);

    auto* engine = static_cast<AsyncImporterEngine*>(rec.inPrivateData);
    std::vector<std::thread> workers;
    for (int t = 0; t < 16; ++t) {
        workers.emplace_back([engine, t]() {
            for (int f = 0; f < 25; ++f) {
                openmedia::RawVideoFrame frame;
                frame.width = 1920;
                frame.height = 1080;
                frame.pts = static_cast<int64_t>(t * 100 + f);
                engine->storePrefetchedFrame(frame.pts, frame);
                openmedia::RawVideoFrame out;
                engine->tryGetPrefetchedFrame(frame.pts, out);
                assert(AsyncImporterEngine::asyncImporterEntry(
                    kSelGetSourceVideo, nullptr, engine, &frame.pts) == kImNoErr);
            }
        });
    }
    for (auto& w : workers) {
        w.join();
    }
    assert(engine->cachedCount() <= 8);
    assert(AsyncImporterEngine::asyncClose(engine) == kImNoErr);

    std::cout << "[PASS] TC-2.3.1 AsyncImporterEngine 16-thread reentrancy & aiClose passed.\n";
    return 0;
}
