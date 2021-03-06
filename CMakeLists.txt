# *-* Mode: cmake; *-*

cmake_minimum_required(VERSION 2.8.5)
project(rr C CXX ASM)

# On single configuration generators, make Debug the default configuration
if(NOT CMAKE_CONFIGURATION_TYPES)
  if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Whether to build in `Debug` or `Release` mode." FORCE)
  endif()
endif()

enable_testing()
set(BUILD_SHARED_LIBS ON)

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib/rr)

set(BUILD_TESTS ON CACHE BOOL "Build tests")
set(WILL_RUN_TESTS ${BUILD_TESTS} CACHE BOOL "Run tests")

# CAREFUL!  "-" is an invalid character in RPM package names, while
# debian is happy with it.  However, "_" is illegal in debs, while RPM
# is cool with it.  Sigh.
set(rr_VERSION_MAJOR 4)
set(rr_VERSION_MINOR 4)
set(rr_VERSION_PATCH 0)

add_definitions(-DRR_VERSION="${rr_VERSION_MAJOR}.${rr_VERSION_MINOR}.${rr_VERSION_PATCH}")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pthread -g3 -Wall -Wextra -Wstrict-prototypes -std=gnu11")
set(CMAKE_C_FLAGS_DEBUG "-O0 -Werror -DDEBUG")
set(CMAKE_C_FLAGS_RELEASE "-O2")
# Define __STDC_LIMIT_MACROS so |#include <stdint.h>| works as expected.
# Define __STDC_FORMAT_MACROS so |#include <inttypes.h>| works as expected.
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D__USE_LARGEFILE64 -D__STDC_LIMIT_MACROS -D__STDC_FORMAT_MACROS -std=c++11 -pthread -g3 -Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "-O0 -Werror -DDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "-O2")
set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} -g3")

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-unused-command-line-argument")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-command-line-argument")
endif()

option(force32bit "Force a 32-bit rr build, rather than both 64 and 32-bit. rr will only be able to record and replay 32-bit processes.")
option(disable32bit "On a 64-bit platform, avoid requiring a 32-bit cross-compilation toolchain by not building 32-bit components. rr will be able to record 32-bit processes but not replay them.")

if(force32bit)
  set(rr_32BIT true)
  set(rr_64BIT false)
  set(rr_MBITNESS_OPTION -m32)
else()
  if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    if(disable32bit)
      set(rr_32BIT false)
    else()
      set(rr_32BIT true)
    endif()
    set(rr_64BIT true)
  else()
    set(rr_32BIT true)
    set(rr_64BIT false)
  endif()
  set(rr_MBITNESS_OPTION)
endif()


# Check that compiling 32-bit code on a 64-bit target works, if required.
if(CMAKE_HOST_SYSTEM_PROCESSOR STREQUAL "x86_64" AND rr_32BIT)
  # try_compile won't accept LINK_FLAGS, so do this manually.
  file(WRITE "${CMAKE_BINARY_DIR}/test32.c" "int main() { return 0; }")
  execute_process(COMMAND ${CMAKE_C_COMPILER} -o ${CMAKE_BINARY_DIR}/test32 ${CMAKE_BINARY_DIR}/test32.c -m32
			RESULT_VARIABLE COMPILER_32BIT_RESULT)
  if(NOT (COMPILER_32BIT_RESULT EQUAL 0))
    message(FATAL_ERROR "Your toolchain doesn't support 32-bit cross-compilation. Install the required packages or pass -Ddisable32bit=ON to cmake.")
  endif()
endif()

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${rr_MBITNESS_OPTION}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${rr_MBITNESS_OPTION}")
set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} ${rr_MBITNESS_OPTION}")

find_package(PkgConfig REQUIRED)

# If we're cross-compiling a 32-bit rr build on a 64-bit host we need
# to ensure we're looking for the right libraries.
# This has been tested on Ubuntu and Fedora.
if(CMAKE_HOST_SYSTEM_PROCESSOR STREQUAL "x86_64" AND NOT rr_64BIT)
  set(LIBDIR32_CANDIDATES
    /usr/lib/i386-linux-gnu/pkgconfig/
    /usr/lib/pkgconfig/
  )
  foreach(libdir ${LIBDIR32_CANDIDATES})
    if(IS_DIRECTORY ${libdir})
      set(ENV{PKG_CONFIG_LIBDIR} ${libdir})
      break()
     endif()
   endforeach(libdir)
   if(NOT DEFINED ENV{PKG_CONFIG_LIBDIR})
     message(FATAL_ERROR "Couldn't find a suitable 32-bit pkgconfig lib dir. You probably need to install a 32-bit pkgconfig package (pkgconfig.i686 for Fedora or pkg-config:i386 for Ubuntu")
   endif()
endif()

# Check for required libraries
set(REQUIRED_LIBS
  zlib
)
foreach(required_lib ${REQUIRED_LIBS})
  string(TOUPPER ${required_lib} PKG)
  pkg_check_modules(${PKG} REQUIRED ${required_lib})
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${${PKG}_CFLAGS}")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${${PKG}_CFLAGS}")
endforeach(required_lib)

find_path(SECCOMP NAMES "linux/seccomp.h")
if(NOT SECCOMP)
  message(FATAL_ERROR "Couldn't find linux/seccomp.h. You may need to upgrade your kernel.")
endif()

# Check for Python >=2.7 but not Python 3.
find_package(PythonInterp 2.7 REQUIRED)
if(PYTHON_VERSION_MAJOR GREATER 2)
  message(FATAL_ERROR "Python 3 is not supported, please use Python 2.7.")
endif()

execute_process(COMMAND "${PYTHON_EXECUTABLE}" "-c" "# nothing"
                RESULT_VARIABLE python_status)
if(python_status)
  message(FATAL_ERROR "Couldn't run python interpreter ${PYTHON_EXECUTABLE}.")
endif()

# Check for required Python modules
if(WILL_RUN_TESTS)
  if(NOT BUILD_TESTS)
    message(FATAL_ERROR "Running tests requires building them")
  endif()

  set(REQUIRED_PYTHON_MODULES
    pexpect
  )
else()
  set(REQUIRED_PYTHON_MODULES)
endif()

foreach(py_module ${REQUIRED_PYTHON_MODULES})
  execute_process(COMMAND "${PYTHON_EXECUTABLE}" "-c"
			"import ${py_module}"
			RESULT_VARIABLE module_status)
  if(module_status)
    message(FATAL_ERROR "Couldn't find required Python module ${py_module}.")
  endif()
endforeach(py_module)

if(WILL_RUN_TESTS)
  # Check for gdb
  execute_process(COMMAND "gdb" "--version" RESULT_VARIABLE module_status OUTPUT_QUIET)
  if(module_status)
    message(FATAL_ERROR "Couldn't find gdb.")
  endif()
endif()

set(PRELOAD_COMPILE_FLAGS "-fno-stack-protector")

if (NOT DEBUG_PRELOAD_LIB)
set(PRELOAD_COMPILE_FLAGS "${PRELOAD_COMPILE_FLAGS} ${CMAKE_C_FLAGS_RELEASE}")
endif()

set_source_files_properties(src/preload/preload.c
                            PROPERTIES COMPILE_FLAGS ${PRELOAD_COMPILE_FLAGS})

include_directories("${PROJECT_SOURCE_DIR}/include")
include_directories("${PROJECT_SOURCE_DIR}/third-party/proc-service")
# We need to know where our generated files are.
include_directories("${CMAKE_CURRENT_BINARY_DIR}")

add_library(rrpreload
  src/preload/preload.c
  src/preload/raw_syscall.S
  src/preload/syscall_hook.S
  src/preload/breakpoint_table.S
)

# Ensure that CMake knows about our generated files.
#
# Alphabetical, please.
set(GENERATED_FILES
  AssemblyTemplates.generated
  CheckSyscallNumbers.generated
  SyscallEnumsX64.generated
  SyscallEnumsX86.generated
  SyscallEnumsForTestsX64.generated
  SyscallEnumsForTestsX86.generated
  SyscallHelperFunctions.generated
  SyscallnameArch.generated
  SyscallRecordCase.generated
)

foreach(generated_file ${GENERATED_FILES})
  set_source_files_properties(${generated_file}
                              PROPERTIES GENERATED true HEADER_FILE_ONLY true)
  add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${generated_file}"
                     COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/src/generate_syscalls.py"
		               "${CMAKE_CURRENT_BINARY_DIR}/${generated_file}"
		     DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/generate_syscalls.py"
		       "${CMAKE_CURRENT_SOURCE_DIR}/src/syscalls.py"
		       "${CMAKE_CURRENT_SOURCE_DIR}/src/assembly_templates.py")
endforeach(generated_file)

add_custom_target(Generated DEPENDS ${GENERATED_FILES})

add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/bin/rr_page_64"
                   COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/src/generate_rr_page.py"
                   "${CMAKE_CURRENT_BINARY_DIR}/bin/rr_page_64"
                   DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/generate_rr_page.py")
add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/bin/rr_page_32"
                   COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/src/generate_rr_page.py"
                   "${CMAKE_CURRENT_BINARY_DIR}/bin/rr_page_32"
                   DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/generate_rr_page.py")
add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/bin/rr_page_64_replay"
                   COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/src/generate_rr_page.py"
                   "${CMAKE_CURRENT_BINARY_DIR}/bin/rr_page_64_replay"
                   DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/generate_rr_page.py")
add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/bin/rr_page_32_replay"
                   COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/src/generate_rr_page.py"
                   "${CMAKE_CURRENT_BINARY_DIR}/bin/rr_page_32_replay"
                   DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/generate_rr_page.py")

add_custom_target(Pages DEPENDS
                  "${CMAKE_CURRENT_BINARY_DIR}/bin/rr_page_32"
                  "${CMAKE_CURRENT_BINARY_DIR}/bin/rr_page_64"
                  "${CMAKE_CURRENT_BINARY_DIR}/bin/rr_page_32_replay"
                  "${CMAKE_CURRENT_BINARY_DIR}/bin/rr_page_64_replay")

set(RR_SOURCES
  src/test/cpuid_loop.S
  src/AddressSpace.cc
  src/AutoRemoteSyscalls.cc
  src/Command.cc
  src/CompressedReader.cc
  src/CompressedWriter.cc
  src/CPUIDBugDetector.cc
  src/DiversionSession.cc
  src/DumpCommand.cc
  src/ElfReader.cc
  src/EmuFs.cc
  src/Event.cc
  src/ExtraRegisters.cc
  src/fast_forward.cc
  src/FdTable.cc
  src/FileMonitor.cc
  src/Flags.cc
  src/ftrace.cc
  src/GdbCommand.cc
  src/GdbCommandHandler.cc
  src/GdbConnection.cc
  src/GdbExpression.cc
  src/GdbInitCommand.cc
  src/GdbServer.cc
  src/HasTaskSet.cc
  src/HelpCommand.cc
  src/kernel_abi.cc
  src/kernel_metadata.cc
  src/log.cc
  src/MagicSaveDataMonitor.cc
  src/MmappedFileMonitor.cc
  src/MonitoredSharedMemory.cc
  src/Monkeypatcher.cc
  src/PerfCounters.cc
  src/ProcFdDirMonitor.cc
  src/ProcMemMonitor.cc
  src/PsCommand.cc
  src/RecordCommand.cc
  src/RecordSession.cc
  src/record_signal.cc
  src/record_syscall.cc
  src/RecordTask.cc
  src/Registers.cc
  src/remote_code_ptr.cc
  src/ReplayCommand.cc
  src/ReplaySession.cc
  src/replay_syscall.cc
  src/ReplayTask.cc
  src/ReplayTimeline.cc
  src/RerunCommand.cc
  src/ReturnAddressList.cc
  src/Scheduler.cc
  src/SeccompFilterRewriter.cc
  src/Session.cc
  src/StdioMonitor.cc
  src/Task.cc
  src/TaskGroup.cc
  src/ThreadDb.cc
  src/TraceFrame.cc
  src/TraceStream.cc
  src/VirtualPerfCounterMonitor.cc
  src/util.cc
  src/WaitStatus.cc)

function(post_build_executable target)
# grsecurity needs these. But if we add them ourselves, they may conflict
# with other flags added in other ways, and they all have to match :-(. So
# don't do this until a better solution presents itself
#  add_custom_command(TARGET ${target}
#                     POST_BUILD
#                     COMMAND setfattr ARGS -n user.pax.flags -v m $<TARGET_FILE:${target}>)
endfunction(post_build_executable)

option(RR_BUILD_SHARED "Build the rr shared library as well as the binary (experimental).")
if(RR_BUILD_SHARED)
  add_library(rr ${RR_SOURCES})
  set_target_properties(rr PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib)
  add_executable(rrbin src/main.cc)
  set(RR_BIN rrbin)
  post_build_executable(rrbin)
  set_target_properties(rrbin PROPERTIES OUTPUT_NAME rr)
  target_link_libraries(rrbin rr)
  install(TARGETS rr
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib)
else()
  add_executable(rr ${RR_SOURCES} src/main.cc)
  post_build_executable(rr)
  set(RR_BIN rr)
endif()
add_dependencies(rr Generated Pages)

target_link_libraries(rr
  ${CMAKE_DL_LIBS}
  -lrt
  ${ZLIB_LDFLAGS}
)

target_link_libraries(rrpreload
  ${CMAKE_DL_LIBS}
)

add_executable(rr_exec_stub src/exec_stub.c)
post_build_executable(rr_exec_stub)
set_target_properties(rr_exec_stub
                      PROPERTIES LINK_FLAGS "-static -nostartfiles -nodefaultlibs")
set_source_files_properties(src/exec_stub.c
                            COMPILE_FLAGS "-fno-stack-protector")

set(RR_GDB_RESOURCES
  32bit-avx.xml
  32bit-core.xml
  32bit-linux.xml
  32bit-sse.xml
  64bit-avx.xml
  64bit-core.xml
  64bit-linux.xml
  64bit-sse.xml
  amd64-avx-linux.xml
  amd64-linux.xml
  i386-avx-linux.xml
  i386-linux.xml
)
foreach(file ${RR_GDB_RESOURCES})
  configure_file("${CMAKE_CURRENT_SOURCE_DIR}/third-party/gdb/${file}"
                 "${CMAKE_CURRENT_BINARY_DIR}/share/rr/${file}"
                 COPYONLY)
  install(FILES third-party/gdb/${file}
          DESTINATION share/rr)
endforeach(file)

install(PROGRAMS scripts/signal-rr-recording.sh
                  ${CMAKE_CURRENT_BINARY_DIR}/bin/rr_page_64
                  ${CMAKE_CURRENT_BINARY_DIR}/bin/rr_page_64_replay
                  ${CMAKE_CURRENT_BINARY_DIR}/bin/rr_page_32
                  ${CMAKE_CURRENT_BINARY_DIR}/bin/rr_page_32_replay
  DESTINATION bin)

install(TARGETS ${RR_BIN} rrpreload rr_exec_stub
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib/rr
  ARCHIVE DESTINATION lib/rr)

# Build 32-bit librrpreload on 64-bit builds.
# We copy the source files into '32' subdirectories in the output
# directory, so we can set different compile options on them.
# This sucks but I can't find a better way to get CMake to build
# the same source file in two different ways.
if(rr_32BIT AND rr_64BIT)
  foreach(file preload_interface.h)
    configure_file("${CMAKE_CURRENT_SOURCE_DIR}/src/preload/${file}"
                   "${CMAKE_CURRENT_BINARY_DIR}/32/${file}"
                   COPYONLY)
  endforeach(file)

  foreach(file preload.c raw_syscall.S syscall_hook.S breakpoint_table.S)
    configure_file("${CMAKE_CURRENT_SOURCE_DIR}/src/preload/${file}"
                   "${CMAKE_CURRENT_BINARY_DIR}/32/${file}"
                   COPYONLY)
    set_source_files_properties("${CMAKE_CURRENT_BINARY_DIR}/32/${file}"
                                PROPERTIES COMPILE_FLAGS "-m32 ${CMAKE_C_FLAGS_RELEASE}")
  endforeach(file)

  add_library(rrpreload_32
    32/preload.c
    32/raw_syscall.S
    32/syscall_hook.S
    32/breakpoint_table.S
  )
  set_target_properties(rrpreload_32 PROPERTIES LINK_FLAGS -m32)
  target_link_libraries(rrpreload_32
    ${CMAKE_DL_LIBS}
  )

  foreach(file exec_stub.c)
    configure_file("${CMAKE_CURRENT_SOURCE_DIR}/src/${file}"
                   "${CMAKE_CURRENT_BINARY_DIR}/32/${file}"
                   COPYONLY)
    set_source_files_properties("${CMAKE_CURRENT_BINARY_DIR}/32/${file}"
                                PROPERTIES COMPILE_FLAGS "-m32 -fno-stack-protector")
  endforeach(file)

  add_executable(rr_exec_stub_32 32/exec_stub.c)
  post_build_executable(rr_exec_stub_32)
  set_target_properties(rr_exec_stub_32
                        PROPERTIES LINK_FLAGS "-static -nostartfiles -nodefaultlibs -m32")

  install(TARGETS rrpreload_32 rr_exec_stub_32
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib/rr
    ARCHIVE DESTINATION lib/rr)
endif()

##--------------------------------------------------
## Testing

# A "basic test" consists of a foo.c source file. All basic tests use the
# same basic_test.run driver script. The test name is passed as an additional
# parameter to the driver script. This script just does
# "compare_test EXIT-SUCCESS", i.e. records and replays the program and verifies
# that the output of both runs is identical and contains EXIT-SUCCESS.
#
# NB: you must update this variable when adding a new test source
# file.  The list is not generated automatically.
#
# Alphabetical, please.
set(BASIC_TESTS
  64bit_child
  _llseek
  accept
  alarm
  alarm2
  alsa_ioctl
  arch_prctl
  async_segv_ignored
  at_threadexit
  bad_ip
  bad_syscall
  barrier
  big_buffers
  block
  block_open
  brk
  brk2
  capget
  chew_cpu
  chmod
  chown
  clock
  clock_nanosleep
  clone
  clone_bad_stack
  clone_bad_tls
  clone_file_range
  clone_immediate_exit
  clone_newflags
  clone_parent
  clone_untraced
  cloned_sigmask
  constructor
  creat_address_not_truncated
  cross_arch
  cwd_inaccessible
  daemon
  desched_blocking_poll
  dup
  doublesegv
  epoll_create
  epoll_create1
  eventfd
  exec_flags
  exec_no_env
  exec_self
  exec_from_main_thread
  exec_from_other_thread
  exit_with_syscallbuf_signal
  fadvise
  fanotify
  fault_in_code_page
  fcntl_owner_ex
  fcntl_dupfd
  fcntl_seals
  fcntl_sig
  fd_tracking_across_threads
  fds_clean
  flock
  flock_ofd
  flock2
  fork_brk
  fork_child_crash
  fork_stress
  futex_pi
  futex_priorities
  fxregs
  gdb_bogus_breakpoint
  getcpu
  getgroups
  getpwnam
  getrandom
  setitimer
  getsid
  gettimeofday
  grandchild_threads
  grandchild_threads_main_running
  grandchild_threads_thread_running
  grandchild_threads_parent_alive
  hle
  inotify
  int3
  intr_futex_wait_restart
  intr_poll
  intr_ppoll
  intr_pselect
  intr_read_no_restart
  intr_read_restart
  intr_sleep
  intr_sleep_no_restart
  invalid_fcntl
  invalid_ioctl
  io
  ioctl
  ioctl_fs
  ioctl_pty
  ioctl_tty
  kcmp
  keyctl
  kill_newborn
  kill_ptracee
  legacy_ugid
  madvise
  madvise_free
  map_fixed
  map_shared_syscall
  membarrier
  memfd_create
  mincore
  mknod
  mlock
  mmap_adjacent_to_rr_usage
  mmap_private
  mmap_ro
  mmap_self_maps_shared
  mmap_shared
  mmap_shared_multiple
  mmap_shared_subpage
  mmap_shared_write
  mmap_short_file
  mmap_tmpfs
  mmap_zero_size_fd
  modify_ldt
  mount_ns_exec
  mprotect
  mprotect_heterogenous
  mprotect_none
  mprotect_stack
  mq
  mremap
  mremap_grow
  mremap_grow_shared
  mremap_non_page_size
  mremap_shrink
  msg
  msync
  mtio
  multiple_pending_signals
  multiple_pending_signals_sequential
  munmap_segv
  munmap_discontinuous
  nanosleep
  netfilter
  no_mask_timeslice
  numa
  old_fork
  orphan_process
  packet_mmap_disable
  pause
  perf_event
  personality
  pthread_rwlocks
  poll_sig_race
  ppoll
  prctl
  prctl_caps
  prctl_deathsig
  prctl_name
  prctl_tsc
  privileged_net_ioctl
  proc_fds
  proc_mem
  protect_rr_fds
  prw
  pthread_condvar_locking
  ptrace
  ptrace_attach_null_status
  ptrace_attach_running
  ptrace_attach_sleeping
  ptrace_attach_stopped
  ptrace_attach_thread_running
  ptrace_breakpoint
  ptrace_change_patched_syscall
  ptrace_debug_regs
  ptrace_exec
  ptrace_exec32
  ptrace_tls
  ptrace_seize
  ptrace_sigchld_blocked
  ptrace_signals
  ptrace_singlestep
  ptrace_syscall
  ptrace_syscall_clone_untraced
  ptrace_sysemu
  ptrace_sysemu_syscall
  ptrace_trace_clone
  ptrace_trace_exit
  ptrace_traceme
  ptracer_death
  ptracer_death_multithread
  ptracer_death_multithread_peer
  pivot_root
  quotactl
  rdtsc
  read_nothing
  readdir
  read_large
  read_oversize
  readlink
  readlinkat
  readv
  record_replay_subject
  recvfrom
  redzone_integrity
  rename
  rlimit
  robust_futex
  rusage
  samask
  save_data_fd
  # sched_attr ... disabled since suitable headers are not widely available yet
  sched_setaffinity
  sched_setparam
  sched_yield
  sched_yield_to_lower_priority
  scm_rights
  seccomp
  seccomp_null
  seccomp_sigsys_sigtrap
  seccomp_tsync
  seccomp_veto_exec
  self_shebang
  self_sigint
  sem
  send_block
  sendfile
  set_ptracer
  set_tid_address
  setgid
  setgroups
  setsid
  setuid
  shm
  sigaction_old
  sigaltstack
  sigchld_interrupt_signal
  sigcont
  sighandler_fork
  sigill
  signal_deferred
  signal_unstoppable
  signalfd
  sigprocmask
  sigprocmask_exec
  sigprocmask_in_syscallbuf_sighandler
  sigprocmask_rr_sigs
  sigprocmask_syscallbuf
  sigqueueinfo
  sigreturn
  sigreturn_reg
  sigreturnmask
  sigrt
  sigstop
  sigstop2
  sigsuspend
  sigtrap
  simple
  sioc
  sock_names_opts
  spinlock_priorities
  splice
  stack_growth_after_syscallbuf
  stack_growth_syscallbuf
  stack_growth_with_guard
  stack_invalid
  stack_overflow
  stack_overflow_altstack
  stack_overflow_with_guard
  statfs
  stdout_child
  stdout_cloexec
  stdout_dup
  stdout_redirect
  strict_priorities
  switch_read
  symlink
  sync
  syscall_bp
  syscallbuf_signal_reset
  syscallbuf_sigstop
  syscallbuf_timeslice
  syscallbuf_timeslice2
  sysconf
  sysctl
  sysemu_singlestep
  sysinfo
  tgkill
  thread_stress
  thread_yield
  timer
  timerfd
  times
  truncate
  uname
  unjoined_thread
  unshare
  utimes
  vfork_flush
  vfork_shared
  video_capture
  vm_readv_writev
  wait
  wait_sigstop
  write_race
  writev
  xattr
  zero_length_read
)

set(BASIC_CPP_TESTS
  std_random
)

# A "test with program" consists of a foo.c source file and a foo.run driver
# script.  See src/test/util.sh to learn how the .run files work.
#
# NB: you must update this variable when adding a new test source
# file.  The list is not generated automatically.
#
# Alphabetical, please.
set(TESTS_WITH_PROGRAM
  abort_nonmain
  args
  async_kill_with_threads
  async_kill_with_threads_main_running
  async_kill_with_threads_thread_running
  async_segv
  async_signal_syscalls
  async_signal_syscalls2
  async_signal_syscalls_siginfo
  async_usr1
  block_clone_checkpoint
  block_clone_interrupted
  block_clone_syscallbuf_overflow
  block_intr_sigchld
  blocked_bad_ip
  blocked_sigill
  blocked_sigsegv
  breakpoint
  breakpoint_conditions
  breakpoint_overlap
  call_function
  # Disabled because it's very slow
  # check_session_leaks
  checkpoint_dying_threads
  checkpoint_mixed_mode
  checksum_sanity
  clone_interruption
  clone_vfork
  conditional_breakpoint_calls
  conditional_breakpoint_offload
  condvar_stress
  crash
  crash_in_function
  daemon_read
  dconf_mock
  dev_tty
  diversion_syscall
  dlopen
  exec_many
  execve_loop
  exit_codes
  exit_group
  exit_status
  explicit_checkpoints
  fork_syscalls
  function_calls
  getcwd
  goto_event
  hello
  # Disabled because issue #1806 makes tests fail on Debian 8.5 at least
  # history
  ignored_async_usr1
  ignored_sigsegv
  immediate_restart
  int3_ok
  interrupt
  intr_ptrace_decline
  invalid_jump
  jit_proc_mem
  link
  madvise_dontfork
  main_thread_exit
  mmap_replace_most_mappings
  mmap_shared_prot
  mmap_write
  mprotect_growsdown
  mprotect_syscallbuf_overflow
  mutex_pi_stress
  overflow_branch_counter
  priority
  ptrace_remote_unmap
  # Not called ps, because that interferes with using real 'ps' in tests
  rr_ps
  rr_ps_ns
  read_big_struct
  restart_abnormal_exit
  reverse_continue_breakpoint
  reverse_continue_multiprocess
  reverse_continue_process_signal
  reverse_many_breakpoints
  reverse_step_long
  reverse_step_threads
  reverse_step_threads_break
  search
  segfault
  shared_map
  shared_persistent_file
  signal_numbers
  sigprocmask_race
  stack_growth
  step_thread
  string_instructions
  string_instructions_async_signals
  string_instructions_multiwatch
  string_instructions_replay
  string_instructions_watch
  syscallbuf_fd_disabling
  target_fork
  target_process
  term_nonmain
  term_rr
  term_trace_syscall
  thread_exit_signal
  threaded_syscall_spam
  threads
  tls
  ttyname
  unexpected_stack_growth
  user_ignore_sig
  vfork
  wait_for_all
  watchpoint
  watchpoint_at_sched
  watchpoint_before_signal
  watchpoint_syscall
  watchpoint_unaligned
)

# A "test without program" is a foo.run driver script only, which does
# something with one of the test executables above (or has special rules
# to build its own executable).
#
# NB: you must update this variable when adding a new test source
# file.  The list is not generated automatically.
#
# Alphabetical, please.
set(TESTS_WITHOUT_PROGRAM
  async_signal_syscalls_100
  async_signal_syscalls_1000
  bad_breakpoint
  break_block
  break_clock
  break_clone
  break_exec
  break_int3
  break_mmap_private
  break_msg
  break_rdtsc
  break_sigreturn
  break_sync_signal
  break_thread
  break_time_slice
  breakpoint_consistent
  call_exit
  check_patched_pthread
  checkpoint_async_signal_syscalls_1000
  checkpoint_mmap_shared
  checkpoint_prctl_name
  checkpoint_simple
  cont_signal
  cpuid
  dead_thread_target
  desched_ticks
  deliver_async_signal_during_syscalls
  env_newline
  exec_deleted
  exec_stop
  execp
  explicit_checkpoint_clone
  final_sigkill
  first_instruction
  fork_exec_info_thr
  gcrypt_rdrand
  get_thread_list
  hardlink_mmapped_files
  mprotect_step
  parent_no_break_child_bkpt
  parent_no_stop_child_crash
  post_exec_fpu_regs
  read_bad_mem
  record_replay
  remove_watchpoint
  restart_invalid_checkpoint
  restart_unstable
  restart_diversion
  reverse_alarm
  reverse_continue_exec_subprocess
  reverse_continue_fork_subprocess
  reverse_continue_start
  reverse_finish
  reverse_step_breakpoint
  reverse_step_signal
  reverse_step_threads2
  reverse_watchpoint
  reverse_watchpoint_syscall
  run_end
  run_in_function
  sanity
  shm_checkpoint
  siginfo
  signal_stop
  signal_checkpoint
  simple_script
  simple_script_debug
  simple_winch
  stack_overflow_debug
  step1
  step_rdtsc
  step_signal
  string_instructions_break
  string_instructions_replay_quirk
  subprocess_exit_ends_session
  switch_processes
  syscallbuf_timeslice_250
  trace_version
  term_trace_cpu
  unwind_on_signal
  vfork_exec
  watchpoint_cond
  when
)

if(BUILD_TESTS)
  foreach(test ${BASIC_TESTS} ${TESTS_WITH_PROGRAM})
    add_executable(${test} src/test/${test}.c)
    post_build_executable(${test})
    set_source_files_properties(src/test/${test}.c
                                PROPERTIES COMPILE_FLAGS ${CMAKE_C_FLAGS_DEBUG})
    add_dependencies(${test} Generated)
    target_link_libraries(${test} -lrt -ldl)
  endforeach(test)

  # Test disabled because it requires libuvc to be built and installed, and a
  # working USB camera
  # add_executable(usb src/test/usb.c)
  # post_build_executable(usb)
  # add_dependencies(usb Generated)
  # target_link_libraries(usb -lrt -L/usr/local/lib -luvc -lusb-1.0)

  foreach(test ${BASIC_CPP_TESTS})
    add_executable(${test} src/test/${test}.cc)
    post_build_executable(${test})
    set_source_files_properties(src/test/${test}.cc
                                PROPERTIES COMPILE_FLAGS ${CMAKE_CXX_FLAGS_DEBUG})
    add_dependencies(${test} Generated)
    target_link_libraries(${test} -lrt)
  endforeach(test)

  add_library(test_lib
    src/test/test_lib.c
    )
  add_dependencies(test_lib Generated)
  target_link_libraries(constructor -lrt test_lib)

  # cpuid test needs to link with cpuid_loop.S
  add_executable(cpuid src/test/cpuid.c src/test/cpuid_loop.S)
  post_build_executable(cpuid)
  add_dependencies(cpuid Generated)
  target_link_libraries(cpuid -lrt)

  # Check if we're running on KNL. If so, we allot more time to tests, due to
  # reduced single-core performance.
  exec_program(cat ARGS "/proc/cpuinfo" OUTPUT_VARIABLE CPUINFO)
  string(REGEX MATCH "^.*(Xeon Phi).*$" CPU_MODEL_PHI ${CPUINFO})

  function(configure_test test)
    if("${test}" MATCHES "^checkpoint_")
      set(TIMEOUT 600)
    elseif("${test}" MATCHES "stress")
      set(TIMEOUT 600)
    elseif("${test}" MATCHES "string_instructions")
      set(TIMEOUT 600)
    elseif(NOT "${CPU_MODEL_PHI}" STREQUAL "")
      set(TIMEOUT 480)
    else()
      set(TIMEOUT 120)
    endif()
    set_tests_properties(${test}
      PROPERTIES FAIL_REGULAR_EXPRESSION "FAILED" TIMEOUT ${TIMEOUT})
  endfunction(configure_test)

  foreach(test ${BASIC_TESTS} ${BASIC_CPP_TESTS} ${OTHER_TESTS})
    add_test(${test}
      bash ${CMAKE_SOURCE_DIR}/src/test/basic_test.run -b ${CMAKE_SOURCE_DIR} ${PROJECT_BINARY_DIR} ${test})
    configure_test(${test})
    add_test(${test}-no-syscallbuf
      bash ${CMAKE_SOURCE_DIR}/src/test/basic_test.run -n ${CMAKE_SOURCE_DIR} ${PROJECT_BINARY_DIR} ${test})
    configure_test(${test}-no-syscallbuf)
  endforeach(test)

  foreach(test ${TESTS_WITH_PROGRAM} ${TESTS_WITHOUT_PROGRAM})
    add_test(${test}
      bash ${CMAKE_SOURCE_DIR}/src/test/${test}.run -b ${CMAKE_SOURCE_DIR} ${PROJECT_BINARY_DIR} ${test})
    configure_test(${test})
    add_test(${test}-no-syscallbuf
      bash ${CMAKE_SOURCE_DIR}/src/test/${test}.run -n ${CMAKE_SOURCE_DIR} ${PROJECT_BINARY_DIR} ${test})
    configure_test(${test}-no-syscallbuf)
  endforeach(test)

  # Run 32-bit tests on 64-bit builds.
  # We copy the test files into '32' subdirectories in the output
  # directory, so we can set different compile options on them.
  # This sucks but I can't find a better way to get CMake to build
  # the same source file in two different ways.
  if(rr_32BIT AND rr_64BIT)
    foreach(header rrutil.h nsutils.h)
      configure_file("${CMAKE_CURRENT_SOURCE_DIR}/src/test/${header}"
                    "${CMAKE_CURRENT_BINARY_DIR}/32/${header}"
                    COPYONLY)
    endforeach(header)

    foreach(test ${BASIC_TESTS} ${TESTS_WITH_PROGRAM} cpuid test_lib)
      configure_file("${CMAKE_CURRENT_SOURCE_DIR}/src/test/${test}.c"
                     "${CMAKE_CURRENT_BINARY_DIR}/32/${test}.c"
                     COPYONLY)
      set_source_files_properties("${CMAKE_CURRENT_BINARY_DIR}/32/${test}.c"
                                  PROPERTIES COMPILE_FLAGS "-m32 ${CMAKE_C_FLAGS_DEBUG}")
    endforeach(test)

    foreach(test ${BASIC_CPP_TESTS})
      configure_file("${CMAKE_CURRENT_SOURCE_DIR}/src/test/${test}.cc"
                     "${CMAKE_CURRENT_BINARY_DIR}/32/${test}.cc"
                     COPYONLY)
      set_source_files_properties("${CMAKE_CURRENT_BINARY_DIR}/32/${test}.cc"
                                  PROPERTIES COMPILE_FLAGS "-m32 ${CMAKE_CXX_FLAGS_DEBUG}")
    endforeach(test)

    foreach(file cpuid_loop.S)
      configure_file("${CMAKE_CURRENT_SOURCE_DIR}/src/test/${file}"
                     "${CMAKE_CURRENT_BINARY_DIR}/32/${file}"
                     COPYONLY)
      set_source_files_properties("${CMAKE_CURRENT_BINARY_DIR}/32/${file}"
                                  PROPERTIES COMPILE_FLAGS -m32)
    endforeach(file)

    foreach(test ${BASIC_TESTS} ${TESTS_WITH_PROGRAM})
      add_executable(${test}_32 "${CMAKE_CURRENT_BINARY_DIR}/32/${test}.c")
      post_build_executable(${test}_32)
      add_dependencies(${test}_32 Generated)
      set_target_properties(${test}_32 PROPERTIES LINK_FLAGS "-m32 ${CMAKE_C_FLAGS_DEBUG}")
      target_link_libraries(${test}_32 -lrt -ldl)
    endforeach(test)

    foreach(test ${BASIC_CPP_TESTS})
      add_executable(${test}_32 "${CMAKE_CURRENT_BINARY_DIR}/32/${test}.cc")
      post_build_executable(${test}_32)
      add_dependencies(${test}_32 Generated)
      set_target_properties(${test}_32 PROPERTIES LINK_FLAGS "-m32 ${CMAKE_CXX_FLAGS_DEBUG}")
      target_link_libraries(${test}_32 -lrt)
    endforeach(test)

    add_library(test_lib_32
      "${CMAKE_CURRENT_BINARY_DIR}/32/test_lib.c"
    )
    add_dependencies(test_lib_32 Generated)
    set_target_properties(test_lib_32 PROPERTIES LINK_FLAGS -m32)

    target_link_libraries(constructor_32 -lrt test_lib_32)

    # cpuid test needs to link with cpuid_loop.S
    add_executable(cpuid_32 32/cpuid.c 32/cpuid_loop.S)
    post_build_executable(cpuid_32)
    add_dependencies(cpuid_32 Generated)
    set_target_properties(cpuid_32 PROPERTIES LINK_FLAGS -m32)
    target_link_libraries(cpuid_32 -lrt)

    foreach(test ${BASIC_TESTS} ${BASIC_CPP_TESTS} ${OTHER_TESTS})
      add_test(${test}-32
        bash ${CMAKE_SOURCE_DIR}/src/test/basic_test.run -b ${CMAKE_SOURCE_DIR} ${PROJECT_BINARY_DIR} ${test}_32)
      configure_test(${test}-32)
      add_test(${test}-32-no-syscallbuf
        bash ${CMAKE_SOURCE_DIR}/src/test/basic_test.run -n ${CMAKE_SOURCE_DIR} ${PROJECT_BINARY_DIR} ${test}_32)
      configure_test(${test}-32-no-syscallbuf)
    endforeach(test)

    foreach(test ${TESTS_WITH_PROGRAM} ${TESTS_WITHOUT_PROGRAM})
      add_test(${test}-32
        bash ${CMAKE_SOURCE_DIR}/src/test/${test}.run -b ${CMAKE_SOURCE_DIR} ${PROJECT_BINARY_DIR} ${test}_32)
      configure_test(${test}-32)
      add_test(${test}-32-no-syscallbuf
	bash ${CMAKE_SOURCE_DIR}/src/test/${test}.run -n ${CMAKE_SOURCE_DIR} ${PROJECT_BINARY_DIR} ${test}_32)
      configure_test(${test}-32-no-syscallbuf)
    endforeach(test)
  endif()

  set(CHAOS_TESTS
    core_count
    mmap_adjacent
    mmap_bits
    starvation_multithreaded
    starvation_singlethreaded
  )

  foreach(test ${CHAOS_TESTS})
    add_executable(${test} src/chaos-test/${test}.c)
    post_build_executable(${test})
    target_link_libraries(${test} -lrt)
  endforeach(test)

  add_executable(ftrace_helper src/ftrace/ftrace_helper.c)
endif()

include(ProcessorCount)
ProcessorCount(N)
if(NOT N EQUAL 0)
  set(JFLAG -j${N})
endif()

add_custom_target(check COMMAND ${CMAKE_CTEST_COMMAND} --verbose ${JFLAG})
# Run only syscallbuf-enabled and native-bitness tests
add_custom_target(fastcheck COMMAND ${CMAKE_CTEST_COMMAND} --verbose --exclude-regex '[-]' ${JFLAG})

##--------------------------------------------------
## Package configuration

include (InstallRequiredSystemLibraries)

set(CPACK_PACKAGE_NAME "rr")
set(CPACK_PACKAGE_VERSION_MAJOR "${rr_VERSION_MAJOR}")
set(CPACK_PACKAGE_VERSION_MINOR "${rr_VERSION_MINOR}")
set(CPACK_PACKAGE_VERSION_PATCH "${rr_VERSION_PATCH}")
set(CPACK_SYSTEM_NAME "${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")

set(CPACK_OUTPUT_FILE_PREFIX dist)
set(CPACK_GENERATOR "TGZ;RPM;DEB")
set(CPACK_SOURCE_GENERATOR "TGZ")
set(CPACK_BINARY_DIR "${PROJECT_BINARY_DIR}")
set(CPACK_STRIP_FILES TRUE)

set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
  "Lightweight tool for recording and replaying execution of applications (trees of processes and threads)")
set(CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_SOURCE_DIR}/README.md")
set(CPACK_PACKAGE_VENDOR "Mozilla Foundation")

set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Mozilla Foundation")
set(CPACK_DEBIAN_PACKAGE_SECTION "devel")
if(${CMAKE_SYSTEM_PROCESSOR} STREQUAL "x86_64")
  set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64")
elseif(${CMAKE_SYSTEM_PROCESSOR} MATCHES "i.86")
  set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "i386")
elseif(${CMAKE_SYSTEM_PROCESSOR} MATCHES "arm.*")
  set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "arm")
endif()

# XXX Cmake 2.8.7 doesn't know how to avoid specifying /usr,
# /usr/bin, etc, as files to be installed, but distros are finicky
# about their specification.  We want to manually filter those paths
# out of our install list but 2.8.7 also isn't capable of that.
set(CPACK_RPM_USER_BINARY_SPECFILE "${CMAKE_SOURCE_DIR}/rr.spec")
set(CPACK_RPM_PACKAGE_RELEASE 1)
set(CPACK_RPM_PACKAGE_GROUP "Development/Debuggers")
set(CPACK_RPM_PACKAGE_LICENSE "MIT and BSD")

include (CPack)

##--------------------------------------------------
## Misc

add_custom_target(setup-travis COMMAND src/script/setup_travis.sh)
