/*
 * Copyright 2007, The Android Open Source Project
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef EditorClientAndroid_h
#define EditorClientAndroid_h

#include "config.h"


#include "EditorClient.h"
#include "Page.h"
#include "TextCheckerClient.h"
#include "autofill/WebAutofill.h"

#include <wtf/OwnPtr.h>

using namespace WebCore;

namespace android {

class EditorClientAndroid : public EditorClient, public TextCheckerClient {
public:
    EditorClientAndroid() {
        m_shouldChangeSelectedRange = true;
        m_uiGeneratedSelectionChange = false;
    }
    virtual void pageDestroyed();
    
    virtual bool shouldDeleteRange(Range*);
    virtual bool shouldShowDeleteInterface(HTMLElement*);
    virtual bool smartInsertDeleteEnabled(); 
    virtual bool isSelectTrailingWhitespaceEnabled();
    virtual bool isContinuousSpellCheckingEnabled();
    virtual void toggleContinuousSpellChecking();
    virtual bool isGrammarCheckingEnabled();
    virtual void toggleGrammarChecking();
    virtual int spellCheckerDocumentTag();
    
    virtual bool isEditable();
    
    virtual bool shouldBeginEditing(Range*);
    virtual bool shouldEndEditing(Range*);
    virtual bool shouldInsertNode(Node*, Range*, EditorInsertAction);
    virtual bool shouldInsertText(const String&, Range*, EditorInsertAction);
    virtual bool shouldChangeSelectedRange(Range* fromRange, Range* toRange, EAffinity, bool stillSelecting);

    virtual bool shouldApplyStyle(CSSStyleDeclaration*, Range*);
//  virtual bool shouldChangeTypingStyle(CSSStyleDeclaration* fromStyle, CSSStyleDeclaration* toStyle);
//  virtual bool doCommandBySelector(SEL selector);
    virtual bool shouldMoveRangeAfterDelete(Range*, Range*);

    virtual void didBeginEditing();
    virtual void respondToChangedContents();
    virtual void respondToChangedSelection();
    virtual void didEndEditing();
    virtual void didWriteSelectionToPasteboard();
    virtual void didSetSelectionTypesForPasteboard();
//  virtual void didChangeTypingStyle:(NSNotification *)notification;
//  virtual void didChangeSelection:(NSNotification *)notification;
//  virtual NSUndoManager* undoManager:(WebView *)webView;

    virtual void registerCommandForUndo(PassRefPtr<EditCommand>);
    virtual void registerCommandForRedo(PassRefPtr<EditCommand>);
    virtual void clearUndoRedoOperations();
    
    virtual bool canCopyCut(bool defaultValue) const;
    virtual bool canPaste(bool defaultValue) const;
    virtual bool canUndo() const;
    virtual bool canRedo() const;
    
    virtual void undo();
    virtual void redo();

    virtual void handleKeyboardEvent(KeyboardEvent*);
    virtual void handleInputMethodKeydown(KeyboardEvent*);

    virtual void textFieldDidBeginEditing(Element*);
    virtual void textFieldDidEndEditing(Element*);
    virtual void textDidChangeInTextField(Element*);
    virtual bool doTextFieldCommandFromEvent(Element*, KeyboardEvent*);
    virtual void textWillBeDeletedInTextField(Element*);
    virtual void textDidChangeInTextArea(Element*);

    virtual void ignoreWordInSpellDocument(const String&);
    virtual void learnWord(const String&);
    virtual void checkSpellingOfString(const UChar*, int length, int* misspellingLocation, int* misspellingLength);
    virtual String getAutoCorrectSuggestionForMisspelledWord(const String& misspelledWorld);
    virtual void checkGrammarOfString(const UChar*, int length, WTF::Vector<GrammarDetail>&, int* badGrammarLocation, int* badGrammarLength);
    virtual void updateSpellingUIWithGrammarString(const String&, const GrammarDetail& detail);
    virtual void updateSpellingUIWithMisspelledWord(const String&);
    virtual void showSpellingUI(bool show);
    virtual bool spellingUIIsShowing();
    virtual void getGuessesForWord(const String&, const String& context, WTF::Vector<String>& guesses);
    virtual void willSetInputMethodState();
    virtual void setInputMethodState(bool);
    virtual void requestCheckingOfString(SpellChecker*, int, TextCheckingTypeMask, const String&);

    virtual TextCheckerClient* textChecker() { return this; }

    // Android specific:
    void setPage(Page* page) { m_page = page; }
    void setShouldChangeSelectedRange(bool shouldChangeSelectedRange) { m_shouldChangeSelectedRange = shouldChangeSelectedRange; }
    void setUiGeneratedSelectionChange(bool uiGenerated) { m_uiGeneratedSelectionChange = uiGenerated; }
#if ENABLE(WEB_AUTOFILL)
    WebAutofill* getAutofill();
#endif
private:
    Page* m_page;
    bool  m_shouldChangeSelectedRange;
    bool  m_uiGeneratedSelectionChange;
#if ENABLE(WEB_AUTOFILL)
    OwnPtr<WebAutofill> m_autoFill;
#endif
};

}

#endif
