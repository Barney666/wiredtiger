#!/usr/bin/env python3
#
# Public Domain 2014-present MongoDB, Inc.
# Public Domain 2008-2014 WiredTiger, Inc.
#
# This is free and unencumbered software released into the public domain.
#
# Anyone is free to copy, modify, publish, use, compile, sell, or
# distribute this software, either in source code form or as a compiled
# binary, for any purpose, commercial or non-commercial, and by any
# means.
#
# In jurisdictions that recognize copyright laws, the author or authors
# of this software dedicate any and all copyright interest in the
# software to the public domain. We make this dedication for the benefit
# of the public at large and to the detriment of our heirs and
# successors. We intend this dedication to be an overt act of
# relinquishment in perpetuity of all present and future rights to this
# software under copyright law.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
# EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
# IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
# OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
# ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
# OTHER DEALINGS IN THE SOFTWARE.

# Compare WT data files in two different home directories.

import fnmatch, glob, os, sys, time

def usage_exit():
    print('Usage: backup_analysis.py dir1 dir2 [granularity]')
    print('  dir1 and dir2 are POSIX pathnames to WiredTiger backup directories,')
    print('Options:')
    print('  granularity - an (optional) positive integer indicating the granularity')
    print('  of the incremental backup blocks. The default is 16MB')
    sys.exit(1)

def die(reason):
    print('backup_analysis.py: error: ' + reason, file=sys.stderr)
    sys.exit(1)

# Check that the directory given is a backup. Look for the WiredTiger.backup file.
def check_backup(mydir):
    if not os.path.isdir(mydir):
        return False
    backup_file=mydir + "/WiredTiger.backup"
    if not os.path.exists(backup_file):
        return False
    return True

def compare_file(dir1, dir2, filename, file_size, cmp_size, granularity):
    # Initialize all of our counters per file.
    bytes_gran = 0
    gran_blocks = 0
    num_blocks = file_size // cmp_size
    partial = file_size % cmp_size
    pct20 = granularity // 5
    pct80 = pct20 * 4
    pct20_count = 0
    pct80_count = 0
    start_off = 0
    total_bytes_diff = 0
    fp1 = open(os.path.join(dir1, filename), "rb")
    fp2 = open(os.path.join(dir2, filename), "rb")
    # Time how long it takes to compare each file.
    start = time.asctime()
    # Compare the bytes in cmp_size blocks between both files.
    for b in range(0, num_blocks + 1):
        offset = cmp_size * b
        # Gather and report information when we cross a granularity boundary.
        if offset % granularity == 0:
            if offset != 0 and bytes_gran != 0:
                print("{}: Offset {}: {} bytes differ in {} bytes".format(filename, start_off, bytes_gran, granularity))
            # Account for small or large block changes.
            if bytes_gran != 0:
                if bytes_gran <= pct20:
                    pct20_count += 1
                elif bytes_gran >= pct80:
                    pct80_count += 1
            # Reset for the next granularity block.
            start_off = offset
            bytes_gran = 0

        # Compare the two blocks. We know both files are at least file_size so all reads should work.
        buf1 = fp1.read(cmp_size)
        buf2 = fp2.read(cmp_size)
        # If they're different, gather information.
        if buf1 != buf2:
            total_bytes_diff += cmp_size
            # Count how many granularity level blocks changed.
            if bytes_gran == 0:
                gran_blocks += 1
            bytes_gran += cmp_size
    # Account for anything from the final iteration of the loop and partial blocks.
    if partial != 0:
        buf1 = fp1.read(partial)
        buf2 = fp2.read(partial)
        # If they're different, gather information.
        if buf1 != buf2:
            total_bytes_diff += partial
            bytes_gran += partial
    fp1.close()
    fp2.close()
    end = time.asctime()
    if bytes_gran != 0:
        if offset != 0:
            print("{}: Offset {}: {} bytes differ in {} bytes".format(filename, start_off, bytes_gran, granularity))
        if bytes_gran <= pct20:
            pct20_count += 1
        elif bytes_gran >= pct80:
            pct80_count += 1

    # Report for each file.
    print("{}: started {} completed {}".format(filename, start, end))
    print("{} ({}) differs by {} bytes in {} granularity blocks".format(filename, file_size, total_bytes_diff, gran_blocks ))
    if gran_blocks != 0:
        pct20_blocks = round(abs(pct20_count / gran_blocks * 100))
        pct80_blocks = round(abs(pct80_count / gran_blocks * 100))
        print("{}: {} of {} blocks ({}%) differ by {} bytes or less of {} (20%)".format(filename, pct20_count, gran_blocks, pct20_blocks, pct20, granularity))
        print("{}: {} of {} blocks ({}%) differ by {} bytes or more of {} (80%)".format(filename, pct80_count, gran_blocks, pct80_blocks, pct80, granularity))

def compare_backups(dir1, dir2, granularity):
    common=set()
    files1=set(fnmatch.filter(os.listdir(dir1), "*.wt"))
    files2=set(fnmatch.filter(os.listdir(dir2), "*.wt"))

    common = files1.intersection(files2)
    for file in files1.difference(files2):
        print(file + " dropped between backups")
    for file in files2.difference(files1):
        print(file + " created between backups")
    print(common)
    for f in common:
        f1_size = os.stat(os.path.join(dir1, f)).st_size
        f2_size = os.stat(os.path.join(dir2, f)).st_size
        min_size = min(f1_size, f2_size)
        # For now we're only concerned with changed blocks between backups.
        # So only compare the minimum size both files have in common.
        # FIXME: More could be done here to report extra blocks added/removed.
        compare_file(dir1, dir2, f, min_size, 4096, granularity)

def backup_analysis(args):
    if len(args) < 2:
        usage_exit()
    dir1 = args[0]
    dir2 = args[1]
    if len(args) > 2:
        granularity = int(args[2])
    else:
        granularity = 16*1024*1024

    if dir1 == dir2:
        print("Same directory specified. " + dir1)
        usage_exit()

    # Verify both directories are backups.
    if check_backup(dir1) == False:
        print(dir1 + " is not a backup directory")
        usage_exit()
    if check_backup(dir2) == False:
        print(dir1 + " is not a backup directory")
        usage_exit()
    # Find the files that are in common or dropped or created between the backups
    # and compare them.
    compare_backups(dir1, dir2, granularity)

if __name__ == "__main__":
    backup_analysis(sys.argv[1:])
