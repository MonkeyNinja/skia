/*
 * Copyright 2011 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef skiatest_Test_DEFINED
#define skiatest_Test_DEFINED

#include "../tools/Registry.h"
#include "GrContextFactory.h"
#include "SkClipOpPriv.h"
#include "SkString.h"
#include "SkTraceEvent.h"
#include "SkTypes.h"

namespace skiatest {

SkString GetTmpDir();

struct Failure {
    Failure(const char* f, int l, const char* c, const SkString& m)
        : fileName(f), lineNo(l), condition(c), message(m) {}
    const char* fileName;
    int lineNo;
    const char* condition;
    SkString message;
    SkString toString() const;
};

class Reporter : SkNoncopyable {
public:
    virtual ~Reporter() {}
    virtual void bumpTestCount();
    virtual void reportFailed(const skiatest::Failure&) = 0;
    virtual bool allowExtendedTest() const;
    virtual bool verbose() const;
    virtual void* stats() const { return nullptr; }

    void reportFailedWithContext(const skiatest::Failure& f) {
        SkString fullMessage = f.message;
        if (!fContextStack.empty()) {
            fullMessage.append(" [");
            for (int i = 0; i < fContextStack.count(); ++i) {
                if (i > 0) {
                    fullMessage.append(", ");
                }
                fullMessage.append(fContextStack[i]);
            }
            fullMessage.append("]");
        }
        this->reportFailed(skiatest::Failure(f.fileName, f.lineNo, f.condition, fullMessage));
    }
    void push(const SkString& message) {
        fContextStack.push_back(message);
    }
    void pop() {
        fContextStack.pop_back();
    }

private:
    SkTArray<SkString> fContextStack;
};

#define REPORT_FAILURE(reporter, cond, message) \
    reporter->reportFailedWithContext(skiatest::Failure(__FILE__, __LINE__, cond, message))

class ReporterContext : SkNoncopyable {
public:
    ReporterContext(Reporter* reporter, const SkString& message) : fReporter(reporter) {
        fReporter->push(message);
    }
    ~ReporterContext() {
        fReporter->pop();
    }

private:
    Reporter* fReporter;
};

typedef void (*TestProc)(skiatest::Reporter*, const GrContextOptions&);
typedef void (*ContextOptionsProc)(GrContextOptions*);

struct Test {
    Test(const char* n, bool g, TestProc p, ContextOptionsProc optionsProc = nullptr)
        : name(n), needsGpu(g), proc(p), fContextOptionsProc(optionsProc) {}
    const char* name;
    bool needsGpu;
    TestProc proc;
    ContextOptionsProc fContextOptionsProc;

    void modifyGrContextOptions(GrContextOptions* options) {
        if (fContextOptionsProc) {
            (*fContextOptionsProc)(options);
        }
    }

    void run(skiatest::Reporter* r, const GrContextOptions& options) const {
        TRACE_EVENT1("test", TRACE_FUNC, "name", this->name/*these are static*/);
        this->proc(r, options);
    }
};

typedef sk_tools::Registry<Test> TestRegistry;

/*
    Use the following macros to make use of the skiatest classes, e.g.

    #include "Test.h"

    DEF_TEST(TestName, reporter) {
        ...
        REPORTER_ASSERT(reporter, x == 15);
        ...
        REPORTER_ASSERT(reporter, x == 15, "x should be 15");
        ...
        if (x != 15) {
            ERRORF(reporter, "x should be 15, but is %d", x);
            return;
        }
        ...
    }
*/

using GrContextFactoryContextType = sk_gpu_test::GrContextFactory::ContextType;

typedef void GrContextTestFn(Reporter*, const sk_gpu_test::ContextInfo&);
typedef bool GrContextTypeFilterFn(GrContextFactoryContextType);

extern bool IsGLContextType(GrContextFactoryContextType);
extern bool IsVulkanContextType(GrContextFactoryContextType);
extern bool IsMetalContextType(GrContextFactoryContextType);
extern bool IsRenderingGLContextType(GrContextFactoryContextType);
extern bool IsNullGLContextType(GrContextFactoryContextType);
void RunWithGPUTestContexts(GrContextTestFn*, GrContextTypeFilterFn*, Reporter*,
                            const GrContextOptions&);

/** Timer provides wall-clock duration since its creation. */
class Timer {
public:
    /** Starts the timer. */
    Timer();

    /** Nanoseconds since creation. */
    double elapsedNs() const;

    /** Milliseconds since creation. */
    double elapsedMs() const;

    /** Milliseconds since creation as an integer.
        Behavior is undefined for durations longer than SK_MSecMax.
    */
    SkMSec elapsedMsInt() const;
private:
    double fStartNanos;
};

}  // namespace skiatest

#define REPORTER_ASSERT(r, cond, ...)                              \
    do {                                                           \
        if (!(cond)) {                                             \
            REPORT_FAILURE(r, #cond, SkStringPrintf(__VA_ARGS__)); \
        }                                                          \
    } while (0)

#define ERRORF(r, ...)                                      \
    do {                                                    \
        REPORT_FAILURE(r, "", SkStringPrintf(__VA_ARGS__)); \
    } while (0)

#define INFOF(REPORTER, ...)         \
    do {                             \
        if ((REPORTER)->verbose()) { \
            SkDebugf(__VA_ARGS__);   \
        }                            \
    } while (0)

#ifdef SK_DO_NOT_REGISTER_NONGPU_UNIT_TESTS
namespace skiatest {
struct NonRegister {
    NonRegister(TestProc) {}
};
}
#define DEF_TEST(name, reporter)                                                \
    static void test_##name(skiatest::Reporter*, const GrContextOptions&);      \
    static skiatest::NonRegister name##_NonTestRegistry(test_##name);           \
    void test_##name(skiatest::Reporter* reporter, const GrContextOptions&)
#else
#define DEF_TEST(name, reporter)                                                          \
    static void test_##name(skiatest::Reporter*, const GrContextOptions&);                \
    skiatest::TestRegistry name##TestRegistry(skiatest::Test(#name, false, test_##name)); \
    void test_##name(skiatest::Reporter* reporter, const GrContextOptions&)
#endif

#define DEF_GPUTEST(name, reporter, options)                                             \
    static void test_##name(skiatest::Reporter*, const GrContextOptions&);               \
    skiatest::TestRegistry name##TestRegistry(skiatest::Test(#name, true, test_##name)); \
    void test_##name(skiatest::Reporter* reporter, const GrContextOptions& options)

#define DEF_GPUTEST_FOR_CONTEXTS(name, context_filter, reporter, context_info, options_filter)  \
    static void test_##name(skiatest::Reporter*, const sk_gpu_test::ContextInfo& context_info); \
    static void test_gpu_contexts_##name(skiatest::Reporter* reporter,                          \
                                         const GrContextOptions& options) {                     \
        skiatest::RunWithGPUTestContexts(test_##name, context_filter, reporter, options);       \
    }                                                                                           \
    skiatest::TestRegistry name##TestRegistry(                                                  \
            skiatest::Test(#name, true, test_gpu_contexts_##name, options_filter));             \
    void test_##name(skiatest::Reporter* reporter, const sk_gpu_test::ContextInfo& context_info)

#define DEF_GPUTEST_FOR_ALL_CONTEXTS(name, reporter, context_info)                          \
        DEF_GPUTEST_FOR_CONTEXTS(name, nullptr, reporter, context_info, nullptr)

#define DEF_GPUTEST_FOR_RENDERING_CONTEXTS(name, reporter, context_info)                    \
        DEF_GPUTEST_FOR_CONTEXTS(name, sk_gpu_test::GrContextFactory::IsRenderingContext,   \
                                 reporter, context_info, nullptr)
#define DEF_GPUTEST_FOR_ALL_GL_CONTEXTS(name, reporter, context_info)                       \
        DEF_GPUTEST_FOR_CONTEXTS(name, &skiatest::IsGLContextType,                          \
                                 reporter, context_info, nullptr)
#define DEF_GPUTEST_FOR_GL_RENDERING_CONTEXTS(name, reporter, context_info)                 \
        DEF_GPUTEST_FOR_CONTEXTS(name, &skiatest::IsRenderingGLContextType,                 \
                                 reporter, context_info, nullptr)
#define DEF_GPUTEST_FOR_NULLGL_CONTEXT(name, reporter, context_info)                        \
        DEF_GPUTEST_FOR_CONTEXTS(name, &skiatest::IsNullGLContextType,                      \
                                 reporter, context_info, nullptr)
#define DEF_GPUTEST_FOR_VULKAN_CONTEXT(name, reporter, context_info)                        \
        DEF_GPUTEST_FOR_CONTEXTS(name, &skiatest::IsVulkanContextType,                      \
                                 reporter, context_info, nullptr)
#define DEF_GPUTEST_FOR_METAL_CONTEXT(name, reporter, context_info)                         \
        DEF_GPUTEST_FOR_CONTEXTS(name, &skiatest::IsMetalContextType,                       \
                                 reporter, context_info, nullptr)

#define REQUIRE_PDF_DOCUMENT(TEST_NAME, REPORTER)                          \
    do {                                                                   \
        SkDynamicMemoryWStream testStream;                                 \
        sk_sp<SkDocument> testDoc(SkPDF::MakeDocument(&testStream));       \
        if (!testDoc) {                                                    \
            INFOF(REPORTER, "PDF disabled; %s test skipped.", #TEST_NAME); \
            return;                                                        \
        }                                                                  \
    } while (false)

#endif
