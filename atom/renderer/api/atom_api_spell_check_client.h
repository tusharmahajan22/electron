// Copyright (c) 2014 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#ifndef ATOM_RENDERER_API_ATOM_API_SPELL_CHECK_CLIENT_H_
#define ATOM_RENDERER_API_ATOM_API_SPELL_CHECK_CLIENT_H_

#include <string>

#include "base/callback.h"
#include "chrome/renderer/spellchecker/spellcheck_worditerator.h"
#include "native_mate/scoped_persistent.h"
#include "third_party/WebKit/public/web/WebSpellCheckClient.h"

namespace atom {

namespace api {

class SpellCheckClient : public blink::WebSpellCheckClient {
 public:
  SpellCheckClient(v8::Isolate* isolate,
                   const std::string& language,
                   v8::Handle<v8::Object> provider);
  virtual ~SpellCheckClient();

 private:
  // blink::WebSpellCheckClient:
  void spellCheck(
      const blink::WebString& text,
      int& misspelledOffset,
      int& misspelledLength,
      blink::WebVector<blink::WebString>* optionalSuggestions) override;
  void checkTextOfParagraph(
      const blink::WebString&,
      blink::WebTextCheckingTypeMask mask,
      blink::WebVector<blink::WebTextCheckingResult>* results) override;
  void requestCheckingOfText(
      const blink::WebString& textToCheck,
      const blink::WebVector<uint32_t>& markersInText,
      const blink::WebVector<unsigned>& markerOffsets,
      blink::WebTextCheckingCompletion* completionCallback) override;
  blink::WebString autoCorrectWord(
      const blink::WebString& misspelledWord) override;
  void showSpellingUI(bool show) override;
  bool isShowingSpellingUI() override;
  void updateSpellingUIWithMisspelledWord(
      const blink::WebString& word) override;

  template<class T>
  bool CallProviderMethod(const char* method, const blink::WebString& text,
                          T* result);

  // Call JavaScript to check spelling.
  bool CheckSpelling(const base::string16& word_to_check);

  // Returns whether or not the given word is a contraction of valid words
  // (e.g. "word:word").
  bool IsValidContraction(const base::string16& word);

  // Represents character attributes used for filtering out characters which
  // are not supported by this SpellCheck object.
  SpellcheckCharAttribute character_attributes_;

  // Represents word iterators used in this spellchecker. The |text_iterator_|
  // splits text provided by WebKit into words, contractions, or concatenated
  // words. The |contraction_iterator_| splits a concatenated word extracted by
  // |text_iterator_| into word components so we can treat a concatenated word
  // consisting only of correct words as a correct word.
  SpellcheckWordIterator text_iterator_;
  SpellcheckWordIterator contraction_iterator_;

  v8::Isolate* isolate_;
  mate::ScopedPersistent<v8::Object> provider_;
  mate::ScopedPersistent<v8::Function> spell_check_;

  DISALLOW_COPY_AND_ASSIGN(SpellCheckClient);
};

}  // namespace api

}  // namespace atom

#endif  // ATOM_RENDERER_API_ATOM_API_SPELL_CHECK_CLIENT_H_
