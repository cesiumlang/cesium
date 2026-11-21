//! The tokens of a corpus file as the fuzzer sees them: identifiers, decimal
//! numbers, runs of whitespace, and any other single byte.

const std = @import("std");
const Allocator = std.mem.Allocator;

fn isSpace(c: u8) bool {
    return switch (c) {
        ' ', '\t', '\n', '\r', 0x0b, 0x0c => true,
        else => false,
    };
}

fn isIdentifierStart(c: u8) bool {
    return std.ascii.isAlphabetic(c) or c == '_';
}

fn isIdentifier(c: u8) bool {
    return std.ascii.isAlphanumeric(c) or c == '_';
}

/// `data` as tokens, each a slice of `data`.
pub fn split(gpa: Allocator, data: []const u8) Allocator.Error![][]const u8 {
    var list: std.ArrayList([]const u8) = .empty;
    errdefer list.deinit(gpa);
    var i: usize = 0;
    while (i < data.len) {
        const start = i;
        const c = data[i];
        i += 1;
        if (isIdentifierStart(c)) {
            while (i < data.len and isIdentifier(data[i])) i += 1;
        } else if (std.ascii.isDigit(c)) {
            while (i < data.len and std.ascii.isDigit(data[i])) i += 1;
        } else if (isSpace(c)) {
            while (i < data.len and isSpace(data[i])) i += 1;
        }
        try list.append(gpa, data[start..i]);
    }
    return list.toOwnedSlice(gpa);
}

/// Whether a token is a run of whitespace.
pub fn isWhitespace(token: []const u8) bool {
    return token.len > 0 and isSpace(token[0]);
}
