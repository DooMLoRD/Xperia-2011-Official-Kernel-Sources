# Copyright (C) 2011 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

LOCAL_PATH:= $(call my-dir)

ifeq ($(TARGET_ARCH),arm)
	arch := arm
else ifeq ($(TARGET_ARCH),x86)
	arch := x86
endif
ifdef arch

common_cflags := \
	-Wall -Wmissing-prototypes -Wshadow -Wpointer-arith -Wmissing-declarations \
	-Wno-pointer-sign -Wno-sign-compare -Wno-unused-parameter -Wno-shadow \
	-fno-strict-aliasing -fno-stack-protector \
	-DVGA_$(arch)=1 \
	-DVGO_linux=1 \
	-DVGP_$(arch)_linux=1 \
	-DVG_PLATFORM=\"$(arch)-linux\" \
	-DVG_LIBDIR=\"/system/lib/valgrind\" \
	-DANDROID_SYMBOLS_DIR=\"/data/local/symbols\"

common_includes := \
	external/valgrind/main \
	external/valgrind/main/include \
	external/valgrind/main/VEX/pub \
	external/valgrind/main/coregrind

vex_ldflags := -nodefaultlibs

ifeq ($(TARGET_ARCH),arm)
tool_ldflags := -static -Wl,--build-id=none,-Ttext=0x38000000 -nodefaultlibs -nostartfiles -u _start -e_start
else
tool_ldflags := -static -Wl,-Ttext=0x38000000 -nodefaultlibs -nostartfiles -u _start -e_start
endif

preload_ldflags := -nodefaultlibs -Wl,-z,interpose,-z,initfirst
# Remove this when the all toolchains are GCC 4.4
ifeq ($(TARGET_ARCH),arm)
  preload_ldflags += -Wl,--icf=none
endif

# Build libvex-($arch)-linux.a
include $(CLEAR_VARS)

LOCAL_MODULE := libvex-$(arch)-linux
LOCAL_MODULE_TAGS := optional
LOCAL_ARM_MODE := arm

LOCAL_SRC_FILES := \
	VEX/priv/main_globals.c \
	VEX/priv/main_main.c \
	VEX/priv/main_util.c \
	VEX/priv/ir_defs.c \
	VEX/priv/ir_match.c \
	VEX/priv/ir_opt.c \
	VEX/priv/guest_generic_bb_to_IR.c \
	VEX/priv/guest_generic_x87.c \
	VEX/priv/guest_x86_helpers.c \
	VEX/priv/guest_x86_toIR.c \
	VEX/priv/guest_amd64_helpers.c \
	VEX/priv/guest_amd64_toIR.c \
	VEX/priv/guest_ppc_helpers.c \
	VEX/priv/guest_ppc_toIR.c \
	VEX/priv/guest_arm_helpers.c \
	VEX/priv/guest_arm_toIR.c \
	VEX/priv/host_generic_regs.c \
	VEX/priv/host_generic_simd64.c \
	VEX/priv/host_generic_simd128.c \
	VEX/priv/host_generic_reg_alloc2.c \
	VEX/priv/host_x86_defs.c \
	VEX/priv/host_x86_isel.c \
	VEX/priv/host_amd64_defs.c \
	VEX/priv/host_amd64_isel.c \
	VEX/priv/host_ppc_defs.c \
	VEX/priv/host_ppc_isel.c \
	VEX/priv/host_arm_defs.c \
	VEX/priv/host_arm_isel.c

LOCAL_C_INCLUDES := $(common_includes)

LOCAL_CFLAGS := $(common_cflags) \
	-Wbad-function-cast \
	-Wcast-qual \
	-Wcast-align \
	-fstrict-aliasing

include $(BUILD_STATIC_LIBRARY)

# Build libcoregrind-$(arch)-linux.a
include $(CLEAR_VARS)

LOCAL_MODULE := libcoregrind-$(arch)-linux
LOCAL_MODULE_TAGS := optional
LOCAL_ARM_MODE := arm

LOCAL_SRC_FILES := \
	coregrind/m_commandline.c \
	coregrind/m_clientstate.c \
	coregrind/m_cpuid.S \
	coregrind/m_debugger.c \
	coregrind/m_debuglog.c \
	coregrind/m_errormgr.c \
	coregrind/m_execontext.c \
	coregrind/m_hashtable.c \
	coregrind/m_libcbase.c \
	coregrind/m_libcassert.c \
	coregrind/m_libcfile.c \
	coregrind/m_libcprint.c \
	coregrind/m_libcproc.c \
	coregrind/m_libcsignal.c \
	coregrind/m_machine.c \
	coregrind/m_main.c \
	coregrind/m_mallocfree.c \
	coregrind/m_options.c \
	coregrind/m_oset.c \
	coregrind/m_redir.c \
	coregrind/m_seqmatch.c \
	coregrind/m_signals.c \
	coregrind/m_sparsewa.c \
	coregrind/m_stacks.c \
	coregrind/m_stacktrace.c \
	coregrind/m_syscall.c \
	coregrind/m_threadstate.c \
	coregrind/m_tooliface.c \
	coregrind/m_trampoline.S \
	coregrind/m_translate.c \
	coregrind/m_transtab.c \
	coregrind/m_vki.c \
	coregrind/m_vkiscnums.c \
	coregrind/m_wordfm.c \
	coregrind/m_xarray.c \
	coregrind/m_aspacehl.c \
	coregrind/m_aspacemgr/aspacemgr-common.c \
	coregrind/m_aspacemgr/aspacemgr-linux.c \
	coregrind/m_aspacemgr/aspacemgr-aix5.c \
	coregrind/m_coredump/coredump-elf.c \
	coregrind/m_coredump/coredump-macho.c \
	coregrind/m_coredump/coredump-xcoff.c \
	coregrind/m_debuginfo/misc.c \
	coregrind/m_debuginfo/d3basics.c \
	coregrind/m_debuginfo/debuginfo.c \
	coregrind/m_debuginfo/readdwarf.c \
	coregrind/m_debuginfo/readdwarf3.c \
	coregrind/m_debuginfo/readelf.c \
	coregrind/m_debuginfo/readmacho.c \
	coregrind/m_debuginfo/readpdb.c \
	coregrind/m_debuginfo/readstabs.c \
	coregrind/m_debuginfo/readxcoff.c \
	coregrind/m_debuginfo/storage.c \
	coregrind/m_debuginfo/tytypes.c \
	coregrind/m_demangle/cp-demangle.c \
	coregrind/m_demangle/cplus-dem.c \
	coregrind/m_demangle/demangle.c \
	coregrind/m_demangle/dyn-string.c \
	coregrind/m_demangle/safe-ctype.c \
	coregrind/m_dispatch/dispatch-x86-linux.S \
	coregrind/m_dispatch/dispatch-amd64-linux.S \
	coregrind/m_dispatch/dispatch-ppc32-linux.S \
	coregrind/m_dispatch/dispatch-ppc64-linux.S \
	coregrind/m_dispatch/dispatch-arm-linux.S \
	coregrind/m_dispatch/dispatch-ppc32-aix5.S \
	coregrind/m_dispatch/dispatch-ppc64-aix5.S \
	coregrind/m_dispatch/dispatch-x86-darwin.S \
	coregrind/m_dispatch/dispatch-amd64-darwin.S \
	coregrind/m_initimg/initimg-linux.c \
	coregrind/m_initimg/initimg-aix5.c \
	coregrind/m_initimg/initimg-darwin.c \
	coregrind/m_initimg/initimg-pathscan.c \
	coregrind/m_mach/mach_basics.c \
	coregrind/m_mach/mach_msg.c \
	coregrind/m_mach/mach_traps-x86-darwin.S \
	coregrind/m_mach/mach_traps-amd64-darwin.S \
	coregrind/m_replacemalloc/replacemalloc_core.c \
	coregrind/m_scheduler/scheduler.c \
	coregrind/m_scheduler/sema.c \
	coregrind/m_sigframe/sigframe-x86-linux.c \
	coregrind/m_sigframe/sigframe-amd64-linux.c \
	coregrind/m_sigframe/sigframe-ppc32-linux.c \
	coregrind/m_sigframe/sigframe-ppc64-linux.c \
	coregrind/m_sigframe/sigframe-arm-linux.c \
	coregrind/m_sigframe/sigframe-ppc32-aix5.c \
	coregrind/m_sigframe/sigframe-ppc64-aix5.c \
	coregrind/m_sigframe/sigframe-x86-darwin.c \
	coregrind/m_sigframe/sigframe-amd64-darwin.c \
	coregrind/m_syswrap/syscall-x86-linux.S \
	coregrind/m_syswrap/syscall-amd64-linux.S \
	coregrind/m_syswrap/syscall-ppc32-linux.S \
	coregrind/m_syswrap/syscall-ppc64-linux.S \
	coregrind/m_syswrap/syscall-arm-linux.S \
	coregrind/m_syswrap/syscall-ppc32-aix5.S \
	coregrind/m_syswrap/syscall-ppc64-aix5.S \
	coregrind/m_syswrap/syscall-x86-darwin.S \
	coregrind/m_syswrap/syscall-amd64-darwin.S \
	coregrind/m_syswrap/syswrap-main.c \
	coregrind/m_syswrap/syswrap-generic.c \
	coregrind/m_syswrap/syswrap-linux.c \
	coregrind/m_syswrap/syswrap-linux-variants.c \
	coregrind/m_syswrap/syswrap-aix5.c \
	coregrind/m_syswrap/syswrap-darwin.c \
	coregrind/m_syswrap/syswrap-x86-linux.c \
	coregrind/m_syswrap/syswrap-amd64-linux.c \
	coregrind/m_syswrap/syswrap-ppc32-linux.c \
	coregrind/m_syswrap/syswrap-ppc64-linux.c \
	coregrind/m_syswrap/syswrap-arm-linux.c \
	coregrind/m_syswrap/syswrap-ppc32-aix5.c \
	coregrind/m_syswrap/syswrap-ppc64-aix5.c \
	coregrind/m_syswrap/syswrap-x86-darwin.c \
	coregrind/m_syswrap/syswrap-amd64-darwin.c \
	coregrind/m_ume/elf.c \
	coregrind/m_ume/macho.c \
	coregrind/m_ume/main.c \
	coregrind/m_ume/script.c

LOCAL_C_INCLUDES := $(common_includes)

LOCAL_LDFLAGS := $(vex_ldflags)

LOCAL_CFLAGS := $(common_cflags)

include $(BUILD_STATIC_LIBRARY)

# Build libreplacemalloc_toolpreload-$(arch)-linux.a
include $(CLEAR_VARS)

LOCAL_MODULE := libreplacemalloc_toolpreload-$(arch)-linux
LOCAL_MODULE_TAGS := optional
LOCAL_ARM_MODE := arm

LOCAL_SRC_FILES := \
	coregrind/m_replacemalloc/vg_replace_malloc.c

LOCAL_C_INCLUDES := $(common_includes)

LOCAL_LDFLAGS := $(preload_ldflags)

LOCAL_CFLAGS := $(common_cflags)

include $(BUILD_STATIC_LIBRARY)

# Build vgpreload_core-$(arch)-linux.so
include $(CLEAR_VARS)

LOCAL_MODULE := vgpreload_core-$(arch)-linux
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/valgrind
LOCAL_ARM_MODE := arm
LOCAL_STRIP_MODULE := false
LOCAL_NO_CRT := true
LOCAL_PRELINK_MODULE := false

LOCAL_SRC_FILES := \
	coregrind/vg_preloaded.c

LOCAL_C_INCLUDES := $(common_includes)

LOCAL_LDFLAGS := $(preload_ldflags)

LOCAL_CFLAGS := $(common_cflags)

include $(BUILD_SHARED_LIBRARY)

# Build memcheck-$(arch)-linux
include $(CLEAR_VARS)

LOCAL_MODULE := memcheck-$(arch)-linux
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/valgrind
LOCAL_ARM_MODE := arm
LOCAL_FORCE_STATIC_EXECUTABLE := true
LOCAL_NO_CRT := true
LOCAL_SYSTEM_SHARED_LIBRARIES :=

LOCAL_SRC_FILES := \
	memcheck/mc_leakcheck.c \
	memcheck/mc_malloc_wrappers.c \
	memcheck/mc_main.c \
	memcheck/mc_translate.c \
	memcheck/mc_machine.c \
	memcheck/mc_errors.c

LOCAL_C_INCLUDES := $(common_includes)

LOCAL_LDFLAGS := $(tool_ldflags)

LOCAL_CFLAGS := $(common_cflags)

LOCAL_STATIC_LIBRARIES := libcoregrind-$(arch)-linux libvex-$(arch)-linux

include $(BUILD_EXECUTABLE)

# Build vgpreload_memcheck-$(arch)-linux.so
include $(CLEAR_VARS)

LOCAL_MODULE := vgpreload_memcheck-$(arch)-linux
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/valgrind
LOCAL_ARM_MODE := arm
LOCAL_STRIP_MODULE := false
LOCAL_NO_CRT := true
LOCAL_PRELINK_MODULE := false

LOCAL_SRC_FILES := \
	memcheck/mc_replace_strmem.c

LOCAL_C_INCLUDES := $(common_includes)

LOCAL_LDFLAGS := $(preload_ldflags)

LOCAL_CFLAGS := $(common_cflags)

LOCAL_WHOLE_STATIC_LIBRARIES := libreplacemalloc_toolpreload-$(arch)-linux

include $(BUILD_SHARED_LIBRARY)

# Build cachegrind-$(arch)-linux
include $(CLEAR_VARS)

LOCAL_MODULE := cachegrind-$(arch)-linux
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/valgrind
LOCAL_ARM_MODE := arm
LOCAL_FORCE_STATIC_EXECUTABLE := true
LOCAL_NO_CRT := true
LOCAL_SYSTEM_SHARED_LIBRARIES :=

LOCAL_SRC_FILES := \
	cachegrind/cg_main.c \
	cachegrind/cg-x86-amd64.c \
	cachegrind/cg-ppc32.c \
	cachegrind/cg-ppc64.c \
	cachegrind/cg-arm.c

LOCAL_C_INCLUDES := $(common_includes)

LOCAL_LDFLAGS := $(tool_ldflags)

LOCAL_CFLAGS := $(common_cflags)

LOCAL_STATIC_LIBRARIES := libcoregrind-$(arch)-linux libvex-$(arch)-linux

include $(BUILD_EXECUTABLE)

# Build callgrind-$(arch)-linux
include $(CLEAR_VARS)

LOCAL_MODULE := callgrind-$(arch)-linux
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/valgrind
LOCAL_ARM_MODE := arm
LOCAL_FORCE_STATIC_EXECUTABLE := true
LOCAL_NO_CRT := true
LOCAL_SYSTEM_SHARED_LIBRARIES :=

LOCAL_SRC_FILES := \
	callgrind/bb.c \
	callgrind/bbcc.c \
	callgrind/callstack.c \
	callgrind/clo.c \
	callgrind/command.c \
	callgrind/context.c \
	callgrind/costs.c \
	callgrind/debug.c \
	callgrind/dump.c \
	callgrind/events.c \
	callgrind/fn.c \
	callgrind/jumps.c \
	callgrind/main.c \
	callgrind/sim.c \
	callgrind/threads.c \
	cachegrind/cg-x86-amd64.c \
	cachegrind/cg-ppc32.c \
	cachegrind/cg-ppc64.c \
	cachegrind/cg-arm.c

LOCAL_C_INCLUDES := $(common_includes) \
	external/valgrind/main/cachegrind

LOCAL_LDFLAGS := $(tool_ldflags)

LOCAL_CFLAGS := $(common_cflags)

LOCAL_STATIC_LIBRARIES := libcoregrind-$(arch)-linux libvex-$(arch)-linux

include $(BUILD_EXECUTABLE)

# Build helgrind-$(arch)-linux
include $(CLEAR_VARS)

LOCAL_MODULE := helgrind-$(arch)-linux
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/valgrind
LOCAL_ARM_MODE := arm
LOCAL_FORCE_STATIC_EXECUTABLE := true
LOCAL_NO_CRT := true
LOCAL_SYSTEM_SHARED_LIBRARIES :=

LOCAL_SRC_FILES := \
	helgrind/hg_basics.c \
	helgrind/hg_errors.c \
	helgrind/hg_lock_n_thread.c \
	helgrind/hg_main.c \
	helgrind/hg_wordset.c \
	helgrind/libhb_core.c

LOCAL_C_INCLUDES := $(common_includes)

LOCAL_LDFLAGS := $(tool_ldflags)

LOCAL_CFLAGS := $(common_cflags)

LOCAL_STATIC_LIBRARIES := libcoregrind-$(arch)-linux libvex-$(arch)-linux

include $(BUILD_EXECUTABLE)

# Build vgpreload_helgrind-$(arch)-linux.so
include $(CLEAR_VARS)

LOCAL_MODULE := vgpreload_helgrind-$(arch)-linux
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/valgrind
LOCAL_ARM_MODE := arm
LOCAL_STRIP_MODULE := false
LOCAL_NO_CRT := true
LOCAL_PRELINK_MODULE := false

LOCAL_SRC_FILES := \
	helgrind/hg_intercepts.c

LOCAL_C_INCLUDES := $(common_includes)

LOCAL_LDFLAGS := $(preload_ldflags)

LOCAL_CFLAGS := $(common_cflags)

LOCAL_WHOLE_STATIC_LIBRARIES := libreplacemalloc_toolpreload-$(arch)-linux

include $(BUILD_SHARED_LIBRARY)

# Build drd-$(arch)-linux
include $(CLEAR_VARS)

LOCAL_MODULE := drd-$(arch)-linux
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/valgrind
LOCAL_ARM_MODE := arm
LOCAL_FORCE_STATIC_EXECUTABLE := true
LOCAL_NO_CRT := true
LOCAL_SYSTEM_SHARED_LIBRARIES :=

LOCAL_SRC_FILES := \
	drd/drd_barrier.c \
	drd/drd_bitmap2_node.c \
	drd/drd_clientobj.c \
	drd/drd_clientreq.c \
	drd/drd_cond.c \
	drd/drd_cond_initializer.c \
	drd/drd_error.c \
	drd/drd_hb.c \
	drd/drd_load_store.c \
	drd/drd_main.c \
	drd/drd_malloc_wrappers.c \
	drd/drd_mutex.c \
	drd/drd_rwlock.c \
	drd/drd_semaphore.c \
	drd/drd_suppression.c

LOCAL_C_INCLUDES := $(common_includes)

LOCAL_LDFLAGS := $(tool_ldflags)

LOCAL_CFLAGS := $(common_cflags)

LOCAL_STATIC_LIBRARIES := libcoregrind-$(arch)-linux libvex-$(arch)-linux

include $(BUILD_EXECUTABLE)

# Build vgpreload_drd-$(arch)-linux.so
include $(CLEAR_VARS)

LOCAL_MODULE := vgpreload_drd-$(arch)-linux
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/valgrind
LOCAL_ARM_MODE := arm
LOCAL_STRIP_MODULE := false
LOCAL_NO_CRT := true
LOCAL_PRELINK_MODULE := false

LOCAL_SRC_FILES := \
	drd/drd_pthread_intercepts.c \
	drd/drd_qtcore_intercepts.c \
	drd/drd_strmem_intercepts.c

LOCAL_C_INCLUDES := $(common_includes)

LOCAL_LDFLAGS := $(preload_ldflags)

LOCAL_CFLAGS := $(common_cflags)

LOCAL_WHOLE_STATIC_LIBRARIES := libreplacemalloc_toolpreload-$(arch)-linux

include $(BUILD_SHARED_LIBRARY)

# Build massif-$(arch)-linux
include $(CLEAR_VARS)

LOCAL_MODULE := massif-$(arch)-linux
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/valgrind
LOCAL_ARM_MODE := arm
LOCAL_FORCE_STATIC_EXECUTABLE := true
LOCAL_NO_CRT := true
LOCAL_SYSTEM_SHARED_LIBRARIES :=

LOCAL_SRC_FILES := \
	massif/ms_main.c

LOCAL_C_INCLUDES := $(common_includes)

LOCAL_LDFLAGS := $(tool_ldflags)

LOCAL_CFLAGS := $(common_cflags)

LOCAL_STATIC_LIBRARIES := libcoregrind-$(arch)-linux libvex-$(arch)-linux

include $(BUILD_EXECUTABLE)

# Build vgpreload_massif-$(arch)-linux.so
include $(CLEAR_VARS)

LOCAL_MODULE := vgpreload_massif-$(arch)-linux
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/valgrind
LOCAL_ARM_MODE := arm
LOCAL_STRIP_MODULE := false
LOCAL_NO_CRT := true
LOCAL_PRELINK_MODULE := false

LOCAL_SRC_FILES :=

LOCAL_C_INCLUDES := $(common_includes)

LOCAL_LDFLAGS := $(preload_ldflags)

LOCAL_CFLAGS := $(common_cflags)

LOCAL_WHOLE_STATIC_LIBRARIES := libreplacemalloc_toolpreload-$(arch)-linux

include $(BUILD_SHARED_LIBRARY)

# Build none-$(arch)-linux
include $(CLEAR_VARS)

LOCAL_MODULE := none-$(arch)-linux
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/valgrind
LOCAL_ARM_MODE := arm
LOCAL_FORCE_STATIC_EXECUTABLE := true
LOCAL_NO_CRT := true
LOCAL_SYSTEM_SHARED_LIBRARIES :=

LOCAL_SRC_FILES := \
	none/nl_main.c

LOCAL_C_INCLUDES := $(common_includes)

LOCAL_LDFLAGS := $(tool_ldflags)

LOCAL_CFLAGS := $(common_cflags)

LOCAL_STATIC_LIBRARIES := libcoregrind-$(arch)-linux libvex-$(arch)-linux

include $(BUILD_EXECUTABLE)

# Build valgrind
include $(CLEAR_VARS)

LOCAL_MODULE := valgrind
LOCAL_MODULE_TAGS := optional
LOCAL_ARM_MODE := arm

LOCAL_SRC_FILES := \
	coregrind/launcher-linux.c \
	coregrind/m_debuglog.c

LOCAL_C_INCLUDES := $(common_includes)

LOCAL_CFLAGS := $(common_cflags)

include $(BUILD_EXECUTABLE)

# Copy prebuilt suppressions
include $(CLEAR_VARS)

LOCAL_MODULE := default.supp
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/valgrind
LOCAL_SRC_FILES := bionic.supp

include $(BUILD_PREBUILT)

endif
