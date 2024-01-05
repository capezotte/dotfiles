//! Prints brightness percentage to stdout whenever an ACPI request to change brightness comes to `apcid`.
const std = @import("std");
const os = std.os;
const cwd = std.fs.cwd();
const mem = std.mem;
const fmt = std.fmt;

pub fn slurpInt(comptime T: type, path: []const u8) !T {
    var buf: [16]u8 = undefined;
    const contents = try cwd.readFile(path, &buf);
    return fmt.parseInt(T, mem.trim(u8, contents, "\n\t "), 10);
}

pub fn updateWob(v: u64) void {
    var buf: [os.PATH_MAX]u8 = undefined;
    const fifodir = os.getenv("XDG_RUNTIME_DIR") orelse os.getenv("HOME") orelse @as([]const u8, "");
    const path = fmt.bufPrint(&buf, "{s}/{s}", .{ fifodir, "wob.fifo" }) catch return;
    const wobfd = os.open(path, os.O.NONBLOCK | os.O.WRONLY, 0o000) catch return;
    defer os.close(wobfd);
    const str_print = fmt.bufPrint(&buf, "{d} brightness\n", .{v}) catch return;
    _ = os.write(wobfd, str_print) catch return;
}

pub fn main() !void {
    // ignore wob sigpipe
    try os.sigaction(os.SIG.PIPE, &.{ .handler = .{ .handler = os.SIG.IGN }, .mask = os.empty_sigset, .flags = 0 }, null);
    // general purpose buffer™
    var buf: [os.PATH_MAX]u8 = undefined;

    var arg_it = std.process.ArgIteratorPosix.init();
    _ = arg_it.skip(); // skip program name
    const location = arg_it.next() orelse {
        return error.@"Specify a backlight device";
    };
    if (arg_it.skip()) {
        return error.@"Too many arguments";
    }

    var sck = try std.net.connectUnixSocket("/run/acpid.socket");
    const max_bright = try slurpInt(u64, try fmt.bufPrint(&buf, "/sys/class/backlight/{s}/max_brightness", .{location}));
    while (true) {
        const cur_bright = try slurpInt(u64, try fmt.bufPrint(&buf, "/sys/class/backlight/{s}/brightness", .{location}));
        const bright_perc = (100 * cur_bright) / max_bright;
        var c = try fmt.bufPrint(&buf, "{d}%\n", .{bright_perc});
        try std.io.getStdOut().writeAll(c);
        updateWob(bright_perc);
        while (0 != try sck.read(&buf)) {
            if (std.mem.startsWith(u8, &buf, "video/brightness")) break;
        } else {
            std.log.warn("EOF/error reached, quitting.", .{});
            return;
        }
        std.time.sleep(std.math.pow(u64, 10, 8));
    }
}
