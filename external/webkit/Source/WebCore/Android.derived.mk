##
## Copyright 2009, The Android Open Source Project
##
## Redistribution and use in source and binary forms, with or without
## modification, are permitted provided that the following conditions
## are met:
##  * Redistributions of source code must retain the above copyright
##    notice, this list of conditions and the following disclaimer.
##  * Redistributions in binary form must reproduce the above copyright
##    notice, this list of conditions and the following disclaimer in the
##    documentation and/or other materials provided with the distribution.
##
## THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
## EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
## IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
## PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
## CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
## EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
## PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
## PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
## OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
## (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
## OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
##

# CSS property names and value keywords

GEN := $(intermediates)/css/CSSPropertyNames.h
$(GEN): SCRIPT := $(LOCAL_PATH)/css/makeprop.pl
ifeq ($(ENABLE_SVG),true)
$(GEN): $(intermediates)/%.h : $(LOCAL_PATH)/%.in $(LOCAL_PATH)/css/SVGCSSPropertyNames.in $(LOCAL_PATH)/css/AndroidCSSPropertyNames.in
else
$(GEN): $(intermediates)/%.h : $(LOCAL_PATH)/%.in $(LOCAL_PATH)/css/AndroidCSSPropertyNames.in
endif
	@echo "Generating CSSPropertyNames.h <= CSSPropertyNames.in"
	@mkdir -p $(dir $@)
	@cat $< > $(dir $@)/$(notdir $<)
	@cat $^ > $(@:%.h=%.in)
	@cp -f $(SCRIPT) $(dir $@)
	@cd $(dir $@) ; perl ./$(notdir $(SCRIPT))
LOCAL_GENERATED_SOURCES += $(GEN)  $(GEN:%.h=%.cpp)

# We also need the .cpp files, which are generated as side effects of the
# above rules.  Specifying this explicitly makes -j2 work.
$(patsubst %.h,%.cpp,$(GEN)): $(intermediates)/css/%.cpp : $(intermediates)/css/%.h


GEN := $(intermediates)/css/CSSValueKeywords.h
$(GEN): SCRIPT := $(LOCAL_PATH)/css/makevalues.pl
$(GEN): $(intermediates)/%.h : $(LOCAL_PATH)/%.in $(LOCAL_PATH)/css/SVGCSSValueKeywords.in
	@echo "Generating CSSValueKeywords.h <= CSSValueKeywords.in"
	@mkdir -p $(dir $@)
	@cp -f $(SCRIPT) $(dir $@)
ifeq ($(ENABLE_SVG),true)    
	@perl -ne 'print lc' $^ > $(@:%.h=%.in)
else
	@perl -ne 'print lc' $< > $(@:%.h=%.in)
endif
	@cd $(dir $@); perl makevalues.pl
LOCAL_GENERATED_SOURCES += $(GEN)  $(GEN:%.h=%.cpp)

# We also need the .cpp files, which are generated as side effects of the
# above rules.  Specifying this explicitly makes -j2 work.
$(patsubst %.h,%.cpp,$(GEN)): $(intermediates)/css/%.cpp : $(intermediates)/css/%.h


# DOCTYPE strings

GEN := $(intermediates)/html/DocTypeStrings.cpp
$(GEN): SCRIPT := $(LOCAL_PATH)/make-hash-tools.pl
$(GEN): PRIVATE_CUSTOM_TOOL = perl $(SCRIPT) $(dir $@) $<
$(GEN): $(LOCAL_PATH)/html/DocTypeStrings.gperf
	$(transform-generated-source)
LOCAL_GENERATED_SOURCES += $(GEN)

# XMLViewer CSS
GEN := $(intermediates)/xml/XMLViewerCSS.h
$(GEN): SCRIPT := $(LOCAL_PATH)/inspector/xxd.pl
$(GEN): PRIVATE_CUSTOM_TOOL = perl $(SCRIPT) XMLViewer_css $< $@
$(GEN): $(LOCAL_PATH)/xml/XMLViewer.css
	$(transform-generated-source)
LOCAL_GENERATED_SOURCES += $(GEN)

# XMLViewer JS
GEN := $(intermediates)/xml/XMLViewerJS.h
$(GEN): SCRIPT := $(LOCAL_PATH)/inspector/xxd.pl
$(GEN): PRIVATE_CUSTOM_TOOL = perl $(SCRIPT) XMLViewer_js $< $@
$(GEN): $(LOCAL_PATH)/xml/XMLViewer.js
	$(transform-generated-source)
LOCAL_GENERATED_SOURCES += $(GEN)

# HTML entity table

GEN := $(intermediates)/html/HTMLEntityTable.cpp
$(GEN): SCRIPT := $(LOCAL_PATH)/html/parser/create-html-entity-table
$(GEN): WEBKITPY_RESOURCES_DIRECTORY := $(LOCAL_PATH)/../../Tools/Scripts/webkitpy
$(GEN): PRIVATE_CUSTOM_TOOL = $(dir $@)$(notdir $(SCRIPT)) -o $@ $<
$(GEN): $(LOCAL_PATH)/html/parser/HTMLEntityNames.in
	@echo "Generating HTMLEntityTable.cpp"
	@mkdir -p $(dir $@)
	@cp -f $(SCRIPT) $(dir $@)
	@cp -rf $(WEBKITPY_RESOURCES_DIRECTORY) $(dir $@)
	$(transform-generated-source)
LOCAL_GENERATED_SOURCES += $(GEN)


# color names

GEN := $(intermediates)/platform/ColorData.cpp
$(GEN): SCRIPT := $(LOCAL_PATH)/make-hash-tools.pl
$(GEN): PRIVATE_CUSTOM_TOOL = perl $(SCRIPT) $(dir $@) $<
$(GEN): $(LOCAL_PATH)/platform/ColorData.gperf
	$(transform-generated-source)
LOCAL_GENERATED_SOURCES += $(GEN)


# CSS tokenizer

GEN := $(intermediates)/css/tokenizer.cpp
$(GEN): PRIVATE_CUSTOM_TOOL = $(OLD_FLEX) -t $< | perl $(dir $<)/maketokenizer > $@
$(GEN): $(LOCAL_PATH)/css/tokenizer.flex $(LOCAL_PATH)/css/maketokenizer
	$(transform-generated-source)
# we have to do this dep by hand:
$(intermediates)/css/CSSParser.o : $(GEN)

# CSS grammar

GEN := $(intermediates)/CSSGrammar.cpp
$(GEN) : PRIVATE_YACCFLAGS := -p cssyy
$(GEN): $(LOCAL_PATH)/css/CSSGrammar.y
	$(call local-transform-y-to-cpp,.cpp)
$(GEN): $(LOCAL_BISON)

LOCAL_GENERATED_SOURCES += $(GEN)

# XPath grammar

GEN := $(intermediates)/XPathGrammar.cpp
$(GEN) : PRIVATE_YACCFLAGS := -p xpathyy
$(GEN): $(LOCAL_PATH)/xml/XPathGrammar.y
	$(call local-transform-y-to-cpp,.cpp)
$(GEN): $(LOCAL_BISON)

LOCAL_GENERATED_SOURCES += $(GEN)
# user agent style sheets

style_sheets := $(LOCAL_PATH)/css/html.css $(LOCAL_PATH)/css/quirks.css $(LOCAL_PATH)/css/fullscreen.css $(LOCAL_PATH)/css/view-source.css $(LOCAL_PATH)/css/mediaControls.css $(LOCAL_PATH)/css/mediaControlsAndroid.css
ifeq ($(ENABLE_SVG), true)
style_sheets := $(style_sheets) $(LOCAL_PATH)/css/svg.css
endif
GEN := $(intermediates)/css/UserAgentStyleSheets.h
make_css_file_arrays := $(LOCAL_PATH)/css/make-css-file-arrays.pl
$(GEN): PRIVATE_CUSTOM_TOOL = $< $@ $(basename $@).cpp $(filter %.css,$^)
$(GEN): $(make_css_file_arrays) $(style_sheets)
	$(transform-generated-source)
LOCAL_GENERATED_SOURCES += $(GEN) $(GEN:%.h=%.cpp)

# character set name table

#gen_inputs := \
		$(LOCAL_PATH)/platform/make-charset-table.pl \
		$(LOCAL_PATH)/platform/character-sets.txt \
		$(LOCAL_PATH)/platform/android/android-encodings.txt
#GEN := $(intermediates)/platform/CharsetData.cpp
#$(GEN): PRIVATE_CUSTOM_TOOL = $^ "android::Encoding::ENCODING_" > $@
#$(GEN): $(gen_inputs)
#	$(transform-generated-source)
#LOCAL_GENERATED_SOURCES += $(GEN)

# the above rule will make this build too
$(intermediates)/css/UserAgentStyleSheets.cpp : $(GEN)

# XML attribute names

GEN:= $(intermediates)/XMLNSNames.cpp
$(GEN): PRIVATE_PATH := $(LOCAL_PATH)
$(GEN): PRIVATE_CUSTOM_TOOL = perl -I $(PRIVATE_PATH)/bindings/scripts $< --attrs $(xmlns_attrs) --output $(dir $@) 
$(GEN): xmlns_attrs := $(LOCAL_PATH)/xml/xmlnsattrs.in
$(GEN): $(LOCAL_PATH)/dom/make_names.pl $(xmlns_attrs)
	$(transform-generated-source)
LOCAL_GENERATED_SOURCES += $(GEN)

GEN:= $(intermediates)/XMLNames.cpp
$(GEN): PRIVATE_PATH := $(LOCAL_PATH)
$(GEN): PRIVATE_CUSTOM_TOOL = perl -I $(PRIVATE_PATH)/bindings/scripts $< --attrs $(xml_attrs) --output $(dir $@) 
$(GEN): xml_attrs := $(LOCAL_PATH)/xml/xmlattrs.in
$(GEN): $(LOCAL_PATH)/dom/make_names.pl $(xml_attrs)
	$(transform-generated-source)
LOCAL_GENERATED_SOURCES += $(GEN)

# XLink attribute names

GEN:= $(intermediates)/XLinkNames.cpp
$(GEN): PRIVATE_PATH := $(LOCAL_PATH)
$(GEN): PRIVATE_CUSTOM_TOOL = perl -I $(PRIVATE_PATH)/bindings/scripts $< --attrs $(xlink_attrs) --output $(dir $@) 
$(GEN): xlink_attrs := $(LOCAL_PATH)/svg/xlinkattrs.in
$(GEN): $(LOCAL_PATH)/dom/make_names.pl $(xlink_attrs)
	$(transform-generated-source)
LOCAL_GENERATED_SOURCES += $(GEN)

ifeq ($(ENABLE_AUTOFILL),true)
# This is really ugly, but necessary. The following rules are taken from
# external/chromium/Android.mk.
# Chromium uses several third party libraries and headers that are already
# present on Android, but in different include paths. Generate a set of
# forwarding headers in the location that Chromium expects. We need to do
# this in both the Chromium and WebCore projects because of the
# WebKit <-> Chromium bindings which include headers from the Chromuim project.

THIRD_PARTY = $(intermediates)/third_party
CHROMIUM_SRC_DIR := $(LOCAL_PATH)/../../../chromium
SCRIPT := $(CHROMIUM_SRC_DIR)/android/generateAndroidForwardingHeader.pl

GEN := $(THIRD_PARTY)/expat/files/lib/expat.h
$(GEN): $(SCRIPT)
$(GEN):
	perl $(SCRIPT) $@ "lib/expat.h"
LOCAL_GENERATED_SOURCES += $(GEN)

GEN := $(THIRD_PARTY)/skia/include/core/SkBitmap.h
$(GEN): $(SCRIPT)
$(GEN):
	perl $(SCRIPT) $@ "include/core/SkBitmap.h"
LOCAL_GENERATED_SOURCES += $(GEN)

GEN := $(THIRD_PARTY)/WebKit/WebKit/chromium/public/WebFormControlElement.h
$(GEN): $(SCRIPT)
$(GEN):
	perl $(SCRIPT) $@ "public/WebFormControlElement.h"
LOCAL_GENERATED_SOURCES += $(GEN)

GEN := $(THIRD_PARTY)/WebKit/WebKit/chromium/public/WebRegularExpression.h
$(GEN): $(SCRIPT)
$(GEN):
	perl $(SCRIPT) $@ "public/WebRegularExpression.h"
LOCAL_GENERATED_SOURCES += $(GEN)

GEN := $(THIRD_PARTY)/WebKit/WebKit/chromium/public/WebString.h
$(GEN): $(SCRIPT)
$(GEN):
	perl $(SCRIPT) $@ "public/WebString.h"
LOCAL_GENERATED_SOURCES += $(GEN)

endif

LOCAL_SRC_FILES += $(LOCAL_GENERATED_SOURCES)
