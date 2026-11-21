//! The damage done to a corpus file: one to four small changes at the level
//! of tokens, lines or bytes, a cut, a repetition, or deep nesting.

const std = @import("std");
const tokens = @import("tokens.zig");
const Allocator = std.mem.Allocator;
const Random = std.Random;

// a long name and a long line
const long_name: [300]u8 = @splat('A');
const long_line: [5000]u8 = @splat('a');

/// Values at the edges of the integer types, and text that readers tend to
/// mishandle; added to the words of the corpus.
pub const interesting = [_][]const u8{
    "0",                    "1",                    "-1",                   "127",                  "128",
    "255",                  "256",                  "32767",                "32768",                "65535",
    "65536",                "2147483647",           "2147483648",           "4294967295",           "4294967296",
    "9223372036854775807",  "9223372036854775808",  "18446744073709551615", "18446744073709551616", "99999999999999999999999999999999999999",
    "-9223372036854775808", "-9223372036854775809", "0x",                   "0x10",                 "00",
    "1e9",                  "1.5",                  "\"",                   "'",                    "\\",
    "\"\\",                 "\"\\x",                "\"\\xZZ\"",            "\"\\0\"",              "'\\''",
    "''",                   "/*",                   "*/",                   "//",                   "\x00",
    "\xff",                 "\xef\xbb\xbf",         "\r",                   "\r\n",                 "\t",
    "\x7f",                 "\xc3\xa9",             "(",                    ")",                    "{",
    "}",                    "[",                    "]",                    "#",                    "##",
    "@",                    "$",                    "%",                    ":",                    ";",
    ",",                    ".",                    "=",                    "->",                   &long_name,
    &long_line,
};

const change_counts = [_]usize{ 1, 1, 1, 2, 2, 3, 4 };
const repeats = [_]usize{ 2, 16, 300 };
const depths = [_]usize{ 8, 100, 3000 };
const Nest = struct { opener: []const u8, closer: []const u8 };
const nests = [_]Nest{
    .{ .opener = "(", .closer = ")" },
    .{ .opener = "{", .closer = "}" },
    .{ .opener = "[", .closer = "]" },
    .{ .opener = "#", .closer = "" },
    .{ .opener = "-", .closer = "" },
    .{ .opener = "!", .closer = "" },
};

fn below(r: Random, n: usize) usize {
    return r.uintLessThan(usize, n);
}

fn pick(r: Random, comptime T: type, items: []const T) T {
    return items[below(r, items.len)];
}

/// `data` with one to four small changes; `words` are what a token may
/// become. Everything is allocated with `a`, best an arena.
pub fn mutate(a: Allocator, r: Random, data: []const u8, words: []const []const u8) Allocator.Error![]const u8 {
    var out = data;
    for (0..pick(r, usize, &change_counts)) |_| {
        const how = below(r, 12);
        out = switch (how) {
            0...4 => try tokenLevel(a, r, out, words, how),
            5...7 => try lineLevel(a, r, out, how),
            8 => try byteLevel(a, r, out),
            // cut the file short, usually in the middle of a construct
            9 => out[0..below(r, out.len + 1)],
            10 => try repeat(a, r, out),
            else => try nest(a, r, out),
        };
    }
    return out;
}

/// What a parser sees: replace, delete, insert, swap or double a token.
fn tokenLevel(a: Allocator, r: Random, data: []const u8, words: []const []const u8, how: usize) Allocator.Error![]const u8 {
    var list: std.ArrayList([]const u8) = .fromOwnedSlice(try tokens.split(a, data));
    if (list.items.len == 0) return pick(r, []const u8, words);
    const i = below(r, list.items.len);
    switch (how) {
        0 => list.items[i] = pick(r, []const u8, words),
        1 => _ = list.orderedRemove(i),
        2 => try list.insert(a, i, pick(r, []const u8, words)),
        3 => std.mem.swap([]const u8, &list.items[i], &list.items[below(r, list.items.len)]),
        else => try list.insert(a, i, list.items[i]),
    }
    return std.mem.concat(a, u8, list.items);
}

/// Delete, copy or swap a line.
fn lineLevel(a: Allocator, r: Random, data: []const u8, how: usize) Allocator.Error![]const u8 {
    var list: std.ArrayList([]const u8) = .empty;
    var it = std.mem.splitScalar(u8, data, '\n');
    while (it.next()) |line| try list.append(a, line);
    const i = below(r, list.items.len);
    switch (how) {
        5 => _ = list.orderedRemove(i),
        6 => try list.insert(a, i, list.items[below(r, list.items.len)]),
        else => std.mem.swap([]const u8, &list.items[i], &list.items[below(r, list.items.len)]),
    }
    return std.mem.join(a, "\n", list.items);
}

/// What a lexer sees: one byte replaced, or inserted.
fn byteLevel(a: Allocator, r: Random, data: []const u8) Allocator.Error![]const u8 {
    if (data.len == 0) return data;
    const i = below(r, data.len);
    const byte = [1]u8{@intCast(below(r, 256))};
    return std.mem.concat(a, u8, &.{ data[0..i], &byte, data[i + below(r, 2) ..] });
}

/// The same piece many times over: long lists, long files.
fn repeat(a: Allocator, r: Random, data: []const u8) Allocator.Error![]const u8 {
    if (data.len == 0) return data;
    const i = below(r, data.len);
    const j = @min(data.len, i + r.intRangeLessThan(usize, 1, 40));
    const times = pick(r, usize, &repeats);
    var out: std.ArrayList(u8) = .empty;
    try out.appendSlice(a, data[0..i]);
    for (0..times) |_| try out.appendSlice(a, data[i..j]);
    try out.appendSlice(a, data[j..]);
    return out.items;
}

/// Deep nesting around a short piece.
fn nest(a: Allocator, r: Random, data: []const u8) Allocator.Error![]const u8 {
    const n = pick(r, Nest, &nests);
    const depth = pick(r, usize, &depths);
    const i = below(r, data.len + 1);
    const j = r.intRangeLessThan(usize, i, @min(data.len, i + 30) + 1);
    var out: std.ArrayList(u8) = .empty;
    try out.appendSlice(a, data[0..i]);
    for (0..depth) |_| try out.appendSlice(a, n.opener);
    try out.appendSlice(a, data[i..j]);
    for (0..depth) |_| try out.appendSlice(a, n.closer);
    try out.appendSlice(a, data[j..]);
    return out.items;
}
