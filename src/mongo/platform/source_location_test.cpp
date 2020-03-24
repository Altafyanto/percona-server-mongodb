/**
 *    Copyright (C) 2019-present MongoDB, Inc.
 *
 *    This program is free software: you can redistribute it and/or modify
 *    it under the terms of the Server Side Public License, version 1,
 *    as published by MongoDB, Inc.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    Server Side Public License for more details.
 *
 *    You should have received a copy of the Server Side Public License
 *    along with this program. If not, see
 *    <http://www.mongodb.com/licensing/server-side-public-license>.
 *
 *    As a special exception, the copyright holders give permission to link the
 *    code of portions of this program with the OpenSSL library under certain
 *    conditions as described in each individual source file and distribute
 *    linked combinations including the program with the OpenSSL library. You
 *    must comply with the Server Side Public License in all respects for
 *    all of the code used other than as permitted herein. If you modify file(s)
 *    with this exception, you may extend this exception to your version of the
 *    file(s), but you are not obligated to do so. If you do not wish to do so,
 *    delete this exception statement from your version. If you delete this
 *    exception statement from all source files in the program, then also delete
 *    it in the license file.
 */

#include "mongo/platform/basic.h"

#include "mongo/platform/source_location_test.h"

namespace mongo {
namespace {

constexpr SourceLocation makeLocalFunctionSourceLocationForTest() {
    return MONGO_SOURCE_LOCATION();
}

const SourceLocationHolder kLocation = MONGO_SOURCE_LOCATION_NO_FUNC();

struct StructWithDefaultInitContextMember {
    const SourceLocationHolder location = MONGO_SOURCE_LOCATION_NO_FUNC();
};

#define CALL_MONGO_SOURCE_LOCATION() MONGO_SOURCE_LOCATION()

TEST(SourceLocation, CorrectLineNumber) {
    using LineT = std::invoke_result_t<decltype(&SourceLocation::line), SourceLocation>;
    ASSERT_EQ(MONGO_SOURCE_LOCATION().line(), static_cast<LineT>(__LINE__));
}

TEST(SourceLocation, InlineVariable) {
    SourceLocationHolder inlineLocation1 = MONGO_SOURCE_LOCATION();
    SourceLocationHolder inlineLocation2 = MONGO_SOURCE_LOCATION();
    SourceLocationHolder inlineLocation3 = MONGO_SOURCE_LOCATION();

    // Each location should have the same filename
    ASSERT_EQ(inlineLocation1.file_name(), inlineLocation2.file_name());
    ASSERT_EQ(inlineLocation1.file_name(), inlineLocation3.file_name());

    // The line numbers for each location should increase monotonically when inline
    ASSERT_LT(inlineLocation1.line(), inlineLocation2.line());
    ASSERT_LT(inlineLocation2.line(), inlineLocation3.line());

    unittest::log() << inlineLocation1;
    unittest::log() << inlineLocation2;
    unittest::log() << inlineLocation3;
}

TEST(SourceLocation, LocalFunction) {
    SourceLocationHolder inlineLocation1 = MONGO_SOURCE_LOCATION();
    SourceLocationHolder localFunctionLocation1 = makeLocalFunctionSourceLocationForTest();
    SourceLocationHolder localFunctionLocation2 = makeLocalFunctionSourceLocationForTest();

    // The inline location should have the same file name but a later line
    ASSERT_EQ(inlineLocation1.file_name(), localFunctionLocation1.file_name());
    ASSERT_GT(inlineLocation1.line(), localFunctionLocation1.line());

    // The two local function locations should be identical
    ASSERT_EQ(localFunctionLocation1, localFunctionLocation2);

    unittest::log() << inlineLocation1;
    unittest::log() << localFunctionLocation1;
    unittest::log() << localFunctionLocation2;
}

TEST(SourceLocation, HeaderFunction) {
    SourceLocationHolder inlineLocation1 = MONGO_SOURCE_LOCATION();
    SourceLocationHolder headerLocation1 = makeHeaderSourceLocationForTest();
    SourceLocationHolder headerLocation2 = makeHeaderSourceLocationForTest();

    // The inline location should have a different file name
    ASSERT_NE(inlineLocation1.file_name(), headerLocation1.file_name());

    // The two header locations should be identical
    ASSERT_EQ(headerLocation1, headerLocation2);

    unittest::log() << inlineLocation1;
    unittest::log() << headerLocation1;
    unittest::log() << headerLocation2;
}

TEST(SourceLocation, GlobalVariable) {
    SourceLocationHolder inlineLocation1 = MONGO_SOURCE_LOCATION();

    // The inline location should have the same file name but a later line
    ASSERT_EQ(inlineLocation1.file_name(), kLocation.file_name());
    ASSERT_GT(inlineLocation1.line(), kLocation.line());

    unittest::log() << inlineLocation1;
    unittest::log() << kLocation;
}

TEST(SourceLocation, DefaultStructMember) {
    SourceLocationHolder inlineLocation1 = MONGO_SOURCE_LOCATION();
    StructWithDefaultInitContextMember obj1;
    StructWithDefaultInitContextMember obj2;

    // The inline location should have the same file name but a later line
    ASSERT_EQ(inlineLocation1.file_name(), obj1.location.file_name());
    ASSERT_GT(inlineLocation1.line(), obj1.location.line());

    // The two default ctor'd struct member locations should be identical
    ASSERT_EQ(obj1.location, obj2.location);

    unittest::log() << inlineLocation1;
    unittest::log() << obj1.location;
    unittest::log() << obj2.location;
}

TEST(SourceLocation, Macro) {
    SourceLocationHolder inlineLocation1 = MONGO_SOURCE_LOCATION();
    SourceLocationHolder inlineLocation2 = CALL_MONGO_SOURCE_LOCATION();

    // Each location should have the same filename
    ASSERT_EQ(inlineLocation1.file_name(), inlineLocation2.file_name());

    // The line numbers for each location should increase monotonically when inline
    ASSERT_LT(inlineLocation1.line(), inlineLocation2.line());

    unittest::log() << inlineLocation1;
    unittest::log() << inlineLocation2;
}

TEST(SourceLocation, Constexpr) {
    constexpr SourceLocationHolder inlineLocation1 = MONGO_SOURCE_LOCATION();
    constexpr SourceLocationHolder inlineLocation2 = MONGO_SOURCE_LOCATION();
    static_assert((inlineLocation1.line() + 1) == inlineLocation2.line());
    static_assert(inlineLocation1.column() == inlineLocation2.column());
    static_assert(areEqual(inlineLocation1.file_name(), inlineLocation2.file_name()));
    static_assert(areEqual(inlineLocation1.function_name(), inlineLocation2.function_name()));

    constexpr auto localFunctionLocation = makeLocalFunctionSourceLocationForTest();
    static_assert(inlineLocation1.line() > localFunctionLocation.line());
    static_assert(areEqual(inlineLocation1.file_name(), localFunctionLocation.file_name()));
    static_assert(
        !areEqual(inlineLocation1.function_name(), localFunctionLocation.function_name()));

    constexpr auto headerLocation = makeHeaderSourceLocationForTest();
    static_assert(!areEqual(inlineLocation1.file_name(), headerLocation.file_name()));
    static_assert(!areEqual(inlineLocation1.function_name(), headerLocation.function_name()));
}

}  // namespace
}  // namespace mongo
