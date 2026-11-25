#include "esp_heap_caps.h"
#include "CJsonReadStream.h"

#include <algorithm>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace
{
using Frames = std::vector<std::string>;
int checks = 0;

void check(bool condition, const std::string &message)
{
    ++checks;
    if (!condition)
        throw std::runtime_error(message);
}

bool feed(CJsonReadStream &stream, std::string bytes)
{
    return stream.add(reinterpret_cast<uint8_t *>(&bytes[0]), static_cast<uint16_t>(bytes.size()));
}

Frames drain(CJsonReadStream &stream)
{
    Frames result;
    std::string frame;
    while (stream.get(frame))
        result.push_back(frame);
    return result;
}

void expect_frames(CJsonReadStream &stream, const Frames &expected, const std::string &label)
{
    const auto actual = drain(stream);
    check(actual == expected, label + ": unexpected frames (got " + std::to_string(actual.size()) + ")");
}

// Every two-part split includes boundaries immediately before/after each quote
// and backslash. Fixed chunk sizes also cross several state changes per add().
void partitions(const std::string &bytes, const Frames &expected, bool resync,
                bool auto_free, uint16_t max_size = 2048, uint8_t abort_byte = 0)
{
    for (std::size_t split = 0; split <= bytes.size(); ++split)
    {
        CJsonReadStream stream(max_size, auto_free, resync, abort_byte);
        feed(stream, bytes.substr(0, split));
        check(!feed(stream, bytes.substr(split)), "two-part input should finish a frame");
        expect_frames(stream, expected, "split " + std::to_string(split));
    }
    for (std::size_t chunk = 1; chunk <= 9; ++chunk)
    {
        CJsonReadStream stream(max_size, auto_free, resync, abort_byte);
        bool pending = false;
        for (std::size_t offset = 0; offset < bytes.size(); offset += chunk)
            pending = feed(stream, bytes.substr(offset, chunk));
        check(!pending, "chunked input should finish a frame");
        expect_frames(stream, expected, "chunk size " + std::to_string(chunk));
    }
}

void common_nested_multiple_frames()
{
    const std::string first = R"({"seq":1,"nested":{"array":[1,{"ok":true},["tail"]]}})";
    const std::string second = R"({"seq":2,"empty":[]})";
    for (bool auto_free : {false, true})
        for (bool resync : {false, true})
            partitions("noise\r\n" + first + "{65535}" + second, {first, "{65535}", second}, resync, auto_free);
}

void common_fragment_pending_and_empty_input()
{
    CJsonReadStream stream(256);
    check(!feed(stream, "noise"), "noise is not a frame");
    check(feed(stream, R"({"outer":)"), "opening frame is pending");
    check(feed(stream, ""), "empty chunk keeps pending state");
    check(feed(stream, R"({"items":[{"n":1})"), "nested object still pending");
    check(!feed(stream, "]}}"), "nested object completed");
    expect_frames(stream, {R"({"outer":{"items":[{"n":1}]}})"}, "fragmented nested object");
    check(!feed(stream, ""), "empty chunk after complete stays complete");
}

void common_resync_outside_strings()
{
    const std::string good = R"({"ok":true})";
    partitions(R"({"lost":{"part":1,{123})" + good, {"{123}", good}, true, false);
    partitions(R"({"lost":{"part":1,{123})" + good, {"{123}", good}, true, true, 12);
    partitions(R"({"lost":{65535})" + good, {"{65535}", good}, true, true);

    // Quotes must cancel a possible marker, rather than joining digits around text.
    const std::string not_marker = R"({"object":{12"key":3}})";
    partitions(not_marker, {not_marker}, true, true);
    const std::string six_digits = R"({"object":{123456}})";
    partitions(six_digits, {six_digits}, true, true);

    CJsonReadStream stream(512, false, false);
    const std::string legacy_nested_marker = R"({"object":{123}})";
    check(!feed(stream, legacy_nested_marker), "resync-disabled nesting completes");
    expect_frames(stream, {legacy_nested_marker}, "resync disabled");
}

void common_abort_resets_partial_frame_in_order()
{
    const std::string can(1, '\x18');
    const std::string first = R"({"seq":1})";
    const std::string incomplete = R"({"lost":"trailing\)";
    const std::string next = R"({"seq":2})";
    for (bool auto_free : {false, true})
        partitions(first + "{12}" + incomplete + can + "}}}}}}}}{34}" + next,
                   {first, "{12}", can, "{34}", next}, true, auto_free, 2048, '\x18');
}

void common_overflow_limits_and_recovery()
{
    const std::string oversized = R"({"text":"this frame is much too long"})";
    const std::string next = R"({"ok":1})";
    for (bool auto_free : {false, true})
        for (bool resync : {false, true})
        {
            partitions(oversized + next, {next}, resync, auto_free, 12);
            partitions(next, {next}, resync, auto_free, static_cast<uint16_t>(next.size()));
            partitions(next + "{}", {"{}"}, resync, auto_free, static_cast<uint16_t>(next.size() - 1));
        }
    for (uint16_t capacity : {uint16_t(0), uint16_t(1), uint16_t(2)})
    {
        CJsonReadStream stream(capacity);
        check(!feed(stream, "{}"), "small-capacity object completes scanning");
        expect_frames(stream, capacity < 2 ? Frames{} : Frames{"{}"}, "small capacity");
    }
}

void common_free_and_auto_free()
{
    for (bool auto_free : {false, true})
    {
        CJsonReadStream stream(512, auto_free, true);
        check(!feed(stream, "{}"), "first frame complete");
        check(host_heap::live_allocations == (auto_free ? 0 : 1), "auto_free policy");
        // free() retains already queued frames, but drops all partial parse state.
        check(feed(stream, R"({"partial":{12)"), "partial marker is pending");
        stream.free();
        check(host_heap::live_allocations == 0, "explicit free releases buffer");
        check(!feed(stream, "{456}{}"), "fresh frames after free");
        expect_frames(stream, {"{}", "{456}", "{}"}, "free keeps queue, clears partial marker");
        stream.free();
        stream.free();
    }
}

void common_allocation_failure()
{
    for (bool auto_free : {false, true})
    {
        CJsonReadStream stream(128, auto_free, true);
        host_heap::fail_next = true;
        check(feed(stream, R"({"lost":)"), "allocation-failed frame remains structurally pending");
        check(!feed(stream, "0}{123}{}"), "failed frame can finish and recover");
        expect_frames(stream, {"{123}", "{}"}, "allocation failure drops entire frame");
    }
}

#ifdef CONFIG_DATAFORMAT_JSON_STRING_AWARE
void corrected_quoted_braces_and_crc()
{
    const Frames frames = {
        R"({"text":"} tail"})",
        R"({"text":"{ tail"})",
        R"({"text":"{} {123} {65535}","n":1})",
        R"({"key{123}":"value}","nested":{"array":["{",{"text":"} {42}"},[]]}})",
        R"json({"text":"quote: \" } {123} \\ tail","path":"C:\\dir\\","next":{}})json",
        R"json({"text":"literal \\u0022 and unicode \u0022 } {123}"})json"
    };
    for (bool auto_free : {false, true})
        for (bool resync : {false, true})
            for (const auto &frame : frames)
                partitions("{7}" + frame + "{}", {"{7}", frame, "{}"}, resync, auto_free);
}

void corrected_backslash_parity()
{
    for (unsigned slashes = 1; slashes <= 8; ++slashes)
    {
        std::string frame = "{\"text\":\"" + std::string(slashes, '\\');
        if ((slashes % 2) != 0)
            frame += "\" } {123} still string"; // Escaped quote does not end the string.
        frame += "\"}";
        for (bool resync : {false, true})
            partitions(frame + "{}", {frame, "{}"}, resync, true);
    }
}

void corrected_explicit_escape_fragmentation()
{
    CJsonReadStream stream(256, false, true);
    const std::string first = "{\"text\":\"prefix \\";
    const std::string middle = "\"} {123} \\";
    const std::string last = std::string("\\") + "\"}";
    check(feed(stream, first), "chunk ending at escape remains pending");
    expect_frames(stream, {}, "no frame at trailing escape");
    check(feed(stream, ""), "empty chunk preserves pending escape");
    check(feed(stream, middle), "escaped quote and marker remain in string");
    expect_frames(stream, {}, "no CRC marker inside quoted string");
    check(!feed(stream, last), "paired backslash then quote closes string");
    expect_frames(stream, {first + middle + last}, "escape state across three chunks");
}

void corrected_overflow_string_state()
{
    const std::string oversized = R"json({"text":"too long } {123} \" { still quoted \\"})json";
    const std::string next = R"({"ok":1})";
    for (bool resync : {false, true})
        for (bool auto_free : {false, true})
            partitions(oversized + next, {next}, resync, auto_free, 12);

    CJsonReadStream stream(12, false, true);
    host_heap::fail_next = true;
    check(feed(stream, R"({"text":"})"), "allocation failure inside quoted string remains pending");
    check(!feed(stream, R"( {123}"}{9}{})"), "allocation-failed quoted string finishes");
    expect_frames(stream, {"{9}", "{}"}, "allocation failure keeps lexical state until frame ends");
}

void corrected_free_clears_quote_and_escape()
{
    for (const auto &partial : {std::string("{\"text\":\""), std::string("{\"text\":\"\\")})
        for (bool auto_free : {false, true})
            for (uint16_t capacity : {uint16_t(8), uint16_t(128)})
            {
                CJsonReadStream stream(capacity, auto_free, true);
                check(feed(stream, partial), "quoted partial frame is pending");
                stream.free();
                check(!feed(stream, "{123}{}"), "free clears quote/escape/overflow state");
                expect_frames(stream, {"{123}", "{}"}, "fresh frames after quoted free");
            }
}

void corrected_unterminated_string_does_not_resync()
{
    CJsonReadStream stream(16, false, true);
    check(feed(stream, R"({"text":"unterminated)"), "open string is pending even when overflowed");
    check(feed(stream, "{123}{}"), "CRC-like content in open string is not structural");
    expect_frames(stream, {}, "no resync from unterminated string");
    stream.free();
    check(!feed(stream, "{123}{}"), "explicit reset recovers unterminated string");
    expect_frames(stream, {"{123}", "{}"}, "reset recovery");
}
#else
void legacy_quoted_braces_are_structural()
{
    CJsonReadStream stream(256, false, false);
    const std::string closing = R"({"text":"} tail"})";
    check(!feed(stream, closing), "legacy closing brace completes early");
    expect_frames(stream, {closing.substr(0, closing.find('}') + 1)}, "legacy premature close");
    check(feed(stream, R"({"text":"{ tail"})"), "legacy opening brace leaves frame pending");
    expect_frames(stream, {}, "legacy quoted opening brace");
    stream.free();
    const std::string balanced = R"({"text":"{} {123}"})";
    check(!feed(stream, balanced), "balanced string braces happen to complete");
    expect_frames(stream, {balanced}, "legacy balanced string braces without resync");
}

void legacy_crc_marker_in_text_resyncs()
{
    const std::string text = R"({"text":"value {123} tail","n":1})";
    partitions(text, {"{123}"}, true, true);
    partitions(text, {text}, false, false);

    const std::string escaped = R"json({"text":"escaped \" {42} tail"})json";
    partitions(escaped, {"{42}"}, true, true);
}

void legacy_escape_does_not_hide_brace()
{
    CJsonReadStream stream(256, true, true);
    const std::string text = R"json({"text":"escaped \" } tail"})json";
    check(!feed(stream, text), "legacy ignores escaped quote context");
    expect_frames(stream, {text.substr(0, text.find('}') + 1)}, "legacy escaped quote brace counting");
}
#endif
} // namespace

int main()
{
    const std::vector<std::pair<const char *, std::function<void()>>> tests = {
        {"nested arrays/objects and multiple frames", common_nested_multiple_frames},
        {"fragment pending and empty chunks", common_fragment_pending_and_empty_input},
        {"resync outside strings and marker limits", common_resync_outside_strings},
        {"out-of-band abort drops partial frame in order", common_abort_resets_partial_frame_in_order},
        {"overflow, exact capacity, and recovery", common_overflow_limits_and_recovery},
        {"free, queued frames, and auto_free", common_free_and_auto_free},
        {"allocation failure recovery", common_allocation_failure},
#ifdef CONFIG_DATAFORMAT_JSON_STRING_AWARE
        {"quoted braces, escaped strings, and CRC text", corrected_quoted_braces_and_crc},
        {"odd/even backslash parity", corrected_backslash_parity},
        {"escape/quote fragmentation", corrected_explicit_escape_fragmentation},
        {"overflow preserves string state", corrected_overflow_string_state},
        {"free resets quote and escape", corrected_free_clears_quote_and_escape},
        {"unterminated string requires close/reset", corrected_unterminated_string_does_not_resync},
#else
        {"legacy quoted braces stay structural", legacy_quoted_braces_are_structural},
        {"legacy CRC text still resyncs", legacy_crc_marker_in_text_resyncs},
        {"legacy escape does not hide brace", legacy_escape_does_not_hide_brace},
#endif
    };
    try
    {
        for (const auto &test : tests)
        {
            test.second();
            check(host_heap::live_allocations == 0, std::string(test.first) + ": leaked buffer");
            check(!host_heap::fail_next, std::string(test.first) + ": unused allocation failure");
            std::cout << "PASS " << test.first << '\n';
        }
#ifdef CONFIG_DATAFORMAT_JSON_STRING_AWARE
        std::cout << "STRING_AWARE=ON: ";
#else
        std::cout << "STRING_AWARE=OFF (legacy): ";
#endif
        std::cout << tests.size() << " tests, " << checks << " checks passed\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << "FAIL " << error.what() << '\n';
        return 1;
    }
}
