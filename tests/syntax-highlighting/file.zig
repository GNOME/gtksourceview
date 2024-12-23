/// for documentation
const std = @import("std");
const builtin = @import("builtin");
// comment line
const direction = enum { north, south, east, west };
const vec = struct { x: f32, y: f32, z: f32 };
pub fn main() !void {
    const array = [_]u8{ 1, 2, 3, 4, 5 };
    const multiline_string =
        \\one
        \\multiline
        \\string
    ;
    const my_vector = vec{
        .x = 123 + 0xff,
        .y = 1_000 + 0o755,
        .z = 0b11110000,
    };
    std.debug.print("Hello from " ++ "Zig {}\n", .{builtin.zig_version});
    std.debug.print("{d}\n", .{'\u{10ffff}'}); // maximum valid Unicode scalar value
    std.debug.print("slice:{any}\nstring:{s}\nenum: {s}\nstruct:{?}", .{ array[0..3], multiline_string, @tagName(direction.south), my_vector });
}
