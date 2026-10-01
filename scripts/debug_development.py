"""Capture a Windows x64 development-host exception with DbgHelp stack symbols.

Standard library only. Evidence stays outside Git. Stops the debuggee after a
matching first-chance AV or any second-chance AV; this is a diagnostic run.
"""
import ctypes as C
from ctypes import wintypes as W
from datetime import datetime
import argparse, hashlib, json, pathlib, struct, subprocess, sys, time
from run_development import REPO, DEVELOPMENT_SHA256

ROOT = REPO.parent
OUT = None
if sys.platform != 'win32' or C.sizeof(C.c_void_p) != 8:
    raise SystemExit('This diagnostic requires Windows and 64-bit Python.')
KERNEL32 = C.WinDLL('kernel32', use_last_error=True)
DBGHELP = C.WinDLL('dbghelp', use_last_error=True)
VOID_P = C.c_void_p
ULONG64 = C.c_ulonglong

class STARTUPINFO(C.Structure):
    _fields_ = [('cb', W.DWORD), ('reserved', W.LPWSTR), ('desktop', W.LPWSTR), ('title', W.LPWSTR),
                ('x', W.DWORD), ('y', W.DWORD), ('xs', W.DWORD), ('ys', W.DWORD),
                ('xc', W.DWORD), ('yc', W.DWORD), ('fill', W.DWORD), ('flags', W.DWORD),
                ('show', W.WORD), ('cb2', W.WORD), ('res2', VOID_P), ('stdin', W.HANDLE),
                ('stdout', W.HANDLE), ('stderr', W.HANDLE)]
class PROCESS_INFORMATION(C.Structure):
    _fields_ = [('process', W.HANDLE), ('thread', W.HANDLE), ('pid', W.DWORD), ('tid', W.DWORD)]
# Force union to 8-byte alignment, matching DEBUG_EVENT on AMD64.
class DEBUG_EVENT_UNION(C.Union):
    _fields_ = [('bytes', C.c_ubyte * 160), ('align', ULONG64)]
class DEBUG_EVENT(C.Structure):
    _fields_ = [('code', W.DWORD), ('pid', W.DWORD), ('tid', W.DWORD), ('data', DEBUG_EVENT_UNION)]
class ADDRESS64(C.Structure):
    _fields_ = [('offset', ULONG64), ('segment', W.WORD), ('mode', C.c_int)]
class KDHELP64(C.Structure):
    _fields_ = [('thread', ULONG64), ('callbackstack', W.DWORD), ('callbackstore', W.DWORD),
                ('nextcallback', W.DWORD), ('frameptr', W.DWORD), ('callmode', ULONG64),
                ('dispatcher', ULONG64), ('range', ULONG64), ('exceptiondispatcher', ULONG64),
                ('stackbase', ULONG64), ('stacklimit', ULONG64), ('build', W.DWORD), ('tablesize', W.DWORD),
                ('table', ULONG64), ('stuboffset', W.DWORD), ('stubsize', W.DWORD), ('reserved', ULONG64*2)]
class STACKFRAME64(C.Structure):
    _fields_ = [('pc', ADDRESS64), ('ret', ADDRESS64), ('frame', ADDRESS64), ('stack', ADDRESS64), ('store', ADDRESS64),
                ('functiontable', VOID_P), ('params', ULONG64*4), ('far', W.BOOL), ('virtual', W.BOOL),
                ('reserved', ULONG64*3), ('kd', KDHELP64)]
class SYMBOL_INFO(C.Structure):
    _fields_ = [('size', W.ULONG), ('type', W.ULONG), ('reserved', ULONG64*2), ('index', W.ULONG),
                ('symbolsize', W.ULONG), ('modbase', ULONG64), ('flags', W.ULONG), ('value', ULONG64),
                ('address', ULONG64), ('reg', W.ULONG), ('scope', W.ULONG), ('tag', W.ULONG),
                ('namelen', W.ULONG), ('maxname', W.ULONG), ('name', C.c_char*1)]
class IMAGEHLP_LINE64(C.Structure):
    _fields_ = [('size', W.DWORD), ('key', VOID_P), ('number', W.DWORD), ('file', C.c_char_p), ('address', ULONG64)]
def proto(dll, name, result, args):
    fn = getattr(dll, name); fn.restype = result; fn.argtypes = args; return fn
create = proto(KERNEL32, 'CreateProcessW', W.BOOL, [W.LPCWSTR, W.LPWSTR, VOID_P, VOID_P, W.BOOL, W.DWORD, VOID_P, W.LPCWSTR, C.POINTER(STARTUPINFO), C.POINTER(PROCESS_INFORMATION)])
wait = proto(KERNEL32, 'WaitForDebugEvent', W.BOOL, [C.POINTER(DEBUG_EVENT), W.DWORD])
cont = proto(KERNEL32, 'ContinueDebugEvent', W.BOOL, [W.DWORD,W.DWORD,W.DWORD])
close = proto(KERNEL32, 'CloseHandle', W.BOOL, [W.HANDLE])
terminate = proto(KERNEL32, 'TerminateProcess', W.BOOL, [W.HANDLE,W.UINT])
getctx = proto(KERNEL32, 'GetThreadContext', W.BOOL, [W.HANDLE,VOID_P])
read = proto(KERNEL32, 'ReadProcessMemory', W.BOOL, [W.HANDLE,VOID_P,VOID_P,C.c_size_t,C.POINTER(C.c_size_t)])
syminit = proto(DBGHELP, 'SymInitializeW', W.BOOL, [W.HANDLE,W.LPCWSTR,W.BOOL])
symclean = proto(DBGHELP, 'SymCleanup', W.BOOL, [W.HANDLE])
symopts = proto(DBGHELP, 'SymSetOptions', W.DWORD, [W.DWORD])
symfrom = proto(DBGHELP, 'SymFromAddr', W.BOOL, [W.HANDLE,ULONG64,C.POINTER(ULONG64),VOID_P])
symline = proto(DBGHELP, 'SymGetLineFromAddr64', W.BOOL, [W.HANDLE,ULONG64,C.POINTER(W.DWORD),C.POINTER(IMAGEHLP_LINE64)])
walk = proto(DBGHELP, 'StackWalk64', W.BOOL, [W.DWORD,W.HANDLE,W.HANDLE,C.POINTER(STACKFRAME64),VOID_P,VOID_P,VOID_P,VOID_P,VOID_P])

def memory(proc, address, size):
    buf=C.create_string_buffer(size); n=C.c_size_t()
    read(proc,VOID_P(address),buf,size,C.byref(n))
    return buf.raw[:n.value]
def symbol(proc, address):
    buf=C.create_string_buffer(C.sizeof(SYMBOL_INFO)+2048)
    info=C.cast(buf,C.POINTER(SYMBOL_INFO)).contents
    info.size=C.sizeof(SYMBOL_INFO); info.maxname=2048
    displacement=ULONG64()
    result={'pc':hex(address)}
    if symfrom(proc,address,C.byref(displacement),buf):
        result.update(name=C.string_at(C.addressof(buf)+SYMBOL_INFO.name.offset,info.namelen).decode('utf-8','replace'), offset=hex(displacement.value))
    line=IMAGEHLP_LINE64(); line.size=C.sizeof(IMAGEHLP_LINE64); off=W.DWORD()
    if symline(proc,address,C.byref(off),C.byref(line)):
        result.update(file=line.file.decode('utf-8','replace'),line=line.number)
    return result
def capture(proc, thread, event, data):
    raw=C.create_string_buffer(1232+16)
    ctx=(C.addressof(raw)+15)&~15
    C.c_uint32.from_address(ctx+48).value=0x10000B
    if not getctx(thread,VOID_P(ctx)): raise C.WinError(C.get_last_error())
    context=C.string_at(ctx,1232)
    regs={name:hex(struct.unpack_from('<Q',context,off)[0]) for name,off in
          [('rax',120),('rcx',128),('rdx',136),('rbx',144),('rsp',152),('rbp',160),('rsi',168),('rdi',176),('r8',184),('r9',192),('r10',200),('r11',208),('r12',216),('r13',224),('r14',232),('r15',240),('rip',248)]}
    symopts(0x2|0x10|0x200|0x80000) # undecorate, source lines, no critical dialogs
    initialized=syminit(proc,str(REPO/'out/build/win-amd64-debug'),True)
    frame=STACKFRAME64()
    frame.pc.offset=int(regs['rip'],16); frame.frame.offset=int(regs['rbp'],16); frame.stack.offset=int(regs['rsp'],16)
    frame.pc.mode=frame.frame.mode=frame.stack.mode=3
    frames=[symbol(proc,frame.pc.offset)]
    if initialized:
        for _ in range(32):
            if not walk(0x8664,proc,thread,C.byref(frame),VOID_P(ctx),None,C.cast(DBGHELP.SymFunctionTableAccess64,VOID_P),C.cast(DBGHELP.SymGetModuleBase64,VOID_P),None): break
            if not frame.pc.offset: break
            if frame.pc.offset != int(frames[-1]['pc'],16):
                frames.append(symbol(proc,frame.pc.offset))
    result={'captured_at': datetime.now().isoformat(), 'process_id': event.pid, 'exception_code':hex(struct.unpack_from('<I',data)[0]),'exception_address':hex(struct.unpack_from('<Q',data,16)[0]),
            'first_chance':struct.unpack_from('<I',data,152)[0], 'fault_address':hex(struct.unpack_from('<Q',data,40)[0]),
            'thread_id':event.tid,'registers':regs,'frames':frames,'symbol_init':bool(initialized)}
    for name in ['rcx','rdx','rbp','rsp']:
        value=int(regs[name],16)
        blob=memory(proc,value,1024)
        (OUT/(name+'.bin')).write_bytes(blob)
    (OUT/'host-context.bin').write_bytes(context)
    (OUT/'stack.bin').write_bytes(memory(proc,int(regs['rsp'],16),65536))
    (OUT/'fault-code.bin').write_bytes(memory(proc,int(regs['rip'],16)-32,128))
    (OUT/'capture.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
    print(json.dumps(result,indent=2),flush=True)
    if initialized:symclean(proc)

def main():
    global OUT
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--seconds',type=int,default=45)
    parser.add_argument('--fault-address',type=lambda value:int(value,0),default=0x10000000C,
                        help='First-chance host fault address to capture (default: guest 0xC at base 0x100000000).')
    parser.add_argument('--config',type=pathlib.Path,default=ROOT/'assets/runtime.local.json')
    parser.add_argument('--game-data-root',type=pathlib.Path)
    args=parser.parse_args()
    if not 1 <= args.seconds <= 600: parser.error('Time limit must be 1..600 seconds.')
    if not 0 <= args.fault_address < 2**64: parser.error('Fault address must fit uint64.')
    root=args.game_data_root
    if root is None:
        root=pathlib.Path(json.loads(args.config.read_text(encoding='utf-8-sig'))['game_data_root'])
    root=root.resolve(strict=True)
    with (root/'Xacalite_ScriptTeam.exe').open('rb') as stream:
        if hashlib.file_digest(stream,'sha256').hexdigest()!=DEVELOPMENT_SHA256:
            parser.exit(1,'Image does not match this development host; refusing to run.\n')
    binary=REPO/'out/build/win-amd64-debug/project_sylpheed.exe'
    if not (root/'config.ini').is_file() or not (root/'dat').is_dir() or not binary.is_file():
        parser.exit(1,'Missing development resources or Debug host.\n')
    OUT=ROOT/'logs'/('crash-probe-'+datetime.now().strftime('%Y%m%d-%H%M%S-%f'))
    OUT.mkdir(parents=True)
    (ROOT/'logs/runtime-user-data/development').mkdir(parents=True,exist_ok=True)
    command=[str(binary),f'--game_data_root={root}',f'--user_data_root={ROOT}/logs/runtime-user-data/development',
             '--gpu_plugin=xenos',f'--log_file={OUT}/runtime.log','--log_level=debug','--log_flush_interval=1','--allow_game_relative_writes=false']
    (OUT/'launch.json').write_text(json.dumps({'command':command,'image_sha256':DEVELOPMENT_SHA256,'seconds':args.seconds,'fault_address':hex(args.fault_address)},indent=2),encoding='utf-8')
    si=STARTUPINFO();si.cb=C.sizeof(si);pi=PROCESS_INFORMATION()
    assert C.sizeof(DEBUG_EVENT)==176 and C.sizeof(STACKFRAME64)==264 and C.sizeof(SYMBOL_INFO)==88
    if not create(str(binary),C.create_unicode_buffer(subprocess.list2cmdline(command)),None,None,False,0x08000002,None,str(binary.parent),C.byref(si),C.byref(pi)):
        raise C.WinError(C.get_last_error())
    print('Evidence:',OUT,flush=True)
    threads={pi.tid:pi.thread}; handles={pi.process,pi.thread}
    end=time.monotonic()+args.seconds; captured=False
    try:
        while time.monotonic()<end:
            event=DEBUG_EVENT()
            if not wait(C.byref(event),1000):continue
            data=bytes(event.data.bytes);status=0x10002
            if event.code==3:
                file,proc,thread=struct.unpack_from('<QQQ',data)
                threads[event.tid]=thread;handles.update([proc,thread])
                if file:close(file)
            elif event.code==2:
                thread=struct.unpack_from('<Q',data)[0];threads[event.tid]=thread;handles.add(thread)
            elif event.code==6:
                file=struct.unpack_from('<Q',data)[0]
                if file:close(file)
            elif event.code==1:
                code=struct.unpack_from('<I',data)[0]
                first=struct.unpack_from('<I',data,152)[0]
                fault=struct.unpack_from('<Q',data,40)[0]
                status=0x80010001
                if code==0x80000003:status=0x10002
                if code==0xC0000005 and (fault==args.fault_address or not first):
                    capture(pi.process,threads[event.tid],event,data)
                    captured=True
                    terminate(pi.process,0xDEAD)
            elif event.code==5:
                print('Host exited:',hex(struct.unpack_from('<I',data)[0]),flush=True)
                cont(event.pid,event.tid,status);break
            cont(event.pid,event.tid,status)
            if captured:break
    finally:
        terminate(pi.process,0xDEAD)
        # Drain process exit to avoid leaving a stopped debuggee.
        for _ in range(20):
            event=DEBUG_EVENT()
            if not wait(C.byref(event),100):break
            cont(event.pid,event.tid,0x10002)
            if event.code==5:break
        for handle in handles:
            if handle:close(handle)
    if not captured:raise RuntimeError('No target exception captured before exit/time limit')

if __name__=='__main__':main()
