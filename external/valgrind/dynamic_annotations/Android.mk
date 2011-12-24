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
include $(CLEAR_VARS)

LOCAL_MODULE := libdynamic_annotations
LOCAL_MODULE_TAGS := optional
LOCAL_ARM_MODE := arm
LOCAL_CFLAGS += -DDYNAMIC_ANNOTATIONS_ENABLED=1
LOCAL_SRC_FILES := dynamic_annotations.c
LOCAL_PRELINK_MODULE := false

# Remove this when the all toolchains are GCC 4.4
ifeq ($(TARGET_ARCH),arm)
  LOCAL_LDFLAGS += -Wl,--icf=none
endif

include $(BUILD_SHARED_LIBRARY)
