const std = @import("std");
const mem = std.mem;
const cwd = std.fs.cwd();
const stdout = std.io.getStdOut().writer();
const d = "/sys/class/power_supply/";

pub fn main() !void {
    var ret_buf: ["100%\n#RRGGBB\n#RRGGBB\n".len]u8 = undefined;
    var perc_buf: [4]u8 = undefined;
    var online_buf: [1]u8 = .{'0'};
    const online_color = if (mem.eql(u8, cwd.readFile(d ++ "AC/online", &online_buf) catch return, "1")) "#88c0d0" else "#bf616a";
    const perc = cwd.readFile(d ++ "BAT0/capacity", &perc_buf) catch "NaN";
    _ = try stdout.write(try std.fmt.bufPrint(&ret_buf, "{[perc]s}%\n{[color]s}\n{[color]s}\n", .{ .perc = perc, .color = online_color }));
}
