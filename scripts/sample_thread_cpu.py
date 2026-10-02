"""Sample Windows process/thread CPU times without suspending game threads.

This measures CPU use, not call stacks, wait reasons or audio underruns.
CSV and metadata default to the workspace's local logs directory.
"""
import argparse
import csv
import ctypes as C
from ctypes import wintypes as W
from datetime import datetime
import json
import os
from pathlib import Path
import time


class ThreadEntry(C.Structure):
    _fields_ = [(name, W.DWORD) for name in (
        'size', 'usage', 'tid', 'pid')] + [('base_priority', W.LONG),
        ('delta_priority', W.LONG), ('flags', W.DWORD)]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--pid', type=int, required=True)
    parser.add_argument('--seconds', type=int, default=60)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    if os.name != 'nt':
        parser.error('Windows only.')
    if args.pid <= 0 or not 1 <= args.seconds <= 300:
        parser.error('PID must be positive; duration must be 1..300 seconds.')
    kernel = C.WinDLL('kernel32', use_last_error=True)
    def api(name, result, arguments):
        func = getattr(kernel, name)
        func.restype, func.argtypes = result, arguments
        return func
    open_process = api('OpenProcess', W.HANDLE, [W.DWORD, W.BOOL, W.DWORD])
    open_thread = api('OpenThread', W.HANDLE, [W.DWORD, W.BOOL, W.DWORD])
    close = api('CloseHandle', W.BOOL, [W.HANDLE])
    snapshot = api('CreateToolhelp32Snapshot', W.HANDLE, [W.DWORD, W.DWORD])
    first = api('Thread32First', W.BOOL, [W.HANDLE, C.POINTER(ThreadEntry)])
    following = api('Thread32Next', W.BOOL, [W.HANDLE, C.POINTER(ThreadEntry)])
    times_args = [W.HANDLE] + [C.POINTER(W.FILETIME)] * 4
    thread_times = api('GetThreadTimes', W.BOOL, times_args)
    process_times = api('GetProcessTimes', W.BOOL, times_args)
    describe = api('GetThreadDescription', C.c_long, [W.HANDLE, C.POINTER(W.LPWSTR)])
    free = api('LocalFree', W.HANDLE, [W.HANDLE])
    image_path = api('QueryFullProcessImageNameW', W.BOOL,
                     [W.HANDLE, W.DWORD, W.LPWSTR, C.POINTER(W.DWORD)])
    wait = api('WaitForSingleObject', W.DWORD, [W.HANDLE, W.DWORD])
    processors = api('GetActiveProcessorCount', W.DWORD, [W.WORD])(0xffff)
    def tick(ft):
        return (ft.dwHighDateTime << 32) | ft.dwLowDateTime
    def read_times(handle, get_times):
        created, exited, system, user = (W.FILETIME() for _ in range(4))
        if not get_times(handle, C.byref(created), C.byref(exited), C.byref(system), C.byref(user)):
            return None
        return tick(created), tick(system), tick(user)
    proc = open_process(0x1000 | 0x100000, False, args.pid)
    if not proc:
        raise C.WinError(C.get_last_error())
    out = args.output or Path(__file__).resolve().parents[2] / 'logs' / (
        'thread-cpu-' + datetime.now().strftime('%Y%m%d-%H%M%S-%f'))
    previous, rows = {}, []
    started = time.perf_counter()
    name = C.create_unicode_buffer(32768)
    size = W.DWORD(len(name))
    try:
        if not image_path(proc, 0, name, C.byref(size)):
            raise C.WinError(C.get_last_error())
        # Include creation times in keys so reused thread IDs are not mixed.
        def record(tid, description, value, now):
            if value is None:
                return
            key = tid, value[0]
            old = previous.get(key)
            previous[key] = now, value
            if old is None:
                return
            elapsed = now - old[0]
            system, user = value[1] - old[1][1], value[2] - old[1][2]
            if elapsed <= 0 or min(system, user) < 0:
                return
            one_core = (system + user) / 10000000 / elapsed * 100
            rows.append([round(now - started, 3), tid, description,
                         round(elapsed, 6), round(one_core / processors, 3),
                         round(one_core, 3), system, user])
        end = started + args.seconds
        while time.perf_counter() < end and wait(proc, 0) == 0x102:
            record(0, 'PROCESS TOTAL', read_times(proc, process_times), time.perf_counter())
            snap = snapshot(4, 0)
            if snap == C.c_void_p(-1).value:
                raise C.WinError(C.get_last_error())
            try:
                entry = ThreadEntry()
                entry.size = C.sizeof(entry)
                more = first(snap, C.byref(entry))
                while more:
                    if entry.pid == args.pid:
                        handle = open_thread(0x800, False, entry.tid)
                        if handle:
                            try:
                                description = W.LPWSTR()
                                text = ''
                                if describe(handle, C.byref(description)) >= 0 and description:
                                    text = description.value
                                    free(C.cast(description, W.HANDLE))
                                record(entry.tid, text, read_times(handle, thread_times),
                                       time.perf_counter())
                            finally:
                                close(handle)
                    more = following(snap, C.byref(entry))
            finally:
                close(snap)
            time.sleep(min(1, max(0, end - time.perf_counter())))
    finally:
        close(proc)
        out.mkdir(parents=True, exist_ok=True)
        with (out / 'thread-cpu.csv').open('w', newline='', encoding='utf-8') as stream:
            writer = csv.writer(stream)
            writer.writerow(['elapsed_s', 'tid', 'name', 'interval_s', 'cpu_total_percent',
                             'cpu_one_core_percent', 'kernel_ticks', 'user_ticks'])
            writer.writerows(rows)
        (out / 'sample.json').write_text(json.dumps({
            'pid': args.pid, 'image': name.value, 'logical_processors': processors,
            'requested_seconds': args.seconds, 'elapsed_seconds': round(time.perf_counter()-started, 3),
            'rows': len(rows), 'interval_seconds': 1,
            'limits': 'CPU-time samples only; no stacks, wait reasons, GPU or audio underrun data.'
        }, indent=2), encoding='utf-8')
    print(out)


if __name__ == '__main__':
    main()
