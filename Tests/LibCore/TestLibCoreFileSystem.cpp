/*
 * Copyright (c) 2026, the Ladybird Browser Contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/ByteBuffer.h>
#include <AK/Format.h>
#include <AK/ScopeGuard.h>
#include <AK/StringView.h>
#include <LibCore/File.h>
#include <LibCore/StandardPaths.h>
#include <LibCore/System.h>
#include <LibFileSystem/FileSystem.h>
#include <LibTest/TestCase.h>
#include <fcntl.h>
#include <unistd.h>

TEST_CASE(copy_directory_does_not_leave_partial_destination_on_self_copy)
{
    auto const root = ByteString::formatted("{}/ladybird-copy-directory-{}"sv, Core::StandardPaths::tempfile_directory(), Core::System::getpid());
    auto const source = ByteString::formatted("{}/src"sv, root);
    auto const nested = ByteString::formatted("{}/nested"sv, source);
    auto const payload = ByteString::formatted("{}/payload.txt"sv, nested);
    auto const self_copy_destination = ByteString::formatted("{}/src/dst"sv, root);
    auto const sibling_copy_destination = ByteString::formatted("{}/copy"sv, root);
    auto const sibling_payload = ByteString::formatted("{}/nested/payload.txt"sv, sibling_copy_destination);

    [[maybe_unused]] auto cleanup = ScopeGuard([&] { (void)FileSystem::remove(root, FileSystem::RecursionMode::Allowed); });
    (void)FileSystem::remove(root, FileSystem::RecursionMode::Allowed);

    TRY_OR_FAIL(Core::System::mkdir(root, 0700));
    TRY_OR_FAIL(Core::System::mkdir(source, 0700));
    TRY_OR_FAIL(Core::System::mkdir(nested, 0700));

    {
        auto file = TRY_OR_FAIL(Core::File::open(payload, Core::File::OpenMode::Write));
        TRY_OR_FAIL(file->write_until_depleted("payload"sv.bytes()));
    }

    auto source_stat = TRY_OR_FAIL(Core::System::stat(source));

    auto self_copy_result = FileSystem::copy_directory(self_copy_destination, source, source_stat, FileSystem::LinkMode::Disallowed, FileSystem::PreserveMode::Nothing);
    EXPECT(self_copy_result.is_error());
    EXPECT_EQ(self_copy_result.error().code(), EINVAL);
    EXPECT(!FileSystem::exists(self_copy_destination));
    EXPECT(FileSystem::exists(source));
    EXPECT(FileSystem::exists(payload));

    {
        auto file = TRY_OR_FAIL(Core::File::open(payload, Core::File::OpenMode::Read));
        auto buffer = TRY_OR_FAIL(ByteBuffer::create_uninitialized(7));
        TRY_OR_FAIL(file->read_until_filled(buffer));
        EXPECT_EQ(StringView { buffer.bytes() }, "payload"sv);
    }

    auto sibling_copy_result = FileSystem::copy_directory(sibling_copy_destination, source, source_stat, FileSystem::LinkMode::Disallowed, FileSystem::PreserveMode::Nothing);
    EXPECT(!sibling_copy_result.is_error());
    EXPECT(FileSystem::exists(sibling_copy_destination));
    EXPECT(FileSystem::exists(sibling_payload));

    {
        auto file = TRY_OR_FAIL(Core::File::open(sibling_payload, Core::File::OpenMode::Read));
        auto buffer = TRY_OR_FAIL(ByteBuffer::create_uninitialized(7));
        TRY_OR_FAIL(file->read_until_filled(buffer));
        EXPECT_EQ(StringView { buffer.bytes() }, "payload"sv);
    }
}
