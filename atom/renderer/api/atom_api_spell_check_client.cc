// Copyright (c) 2014 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#include "atom/renderer/api/atom_api_spell_check_client.h"

#include <vector>

#include "atom/common/native_mate_converters/string16_converter.h"
#include "base/logging.h"
#include "native_mate/converter.h"
#include "native_mate/dictionary.h"
#include "third_party/icu/source/common/unicode/uscript.h"
#include "third_party/WebKit/public/web/WebTextCheckingCompletion.h"
#include "third_party/WebKit/public/web/WebTextCheckingResult.h"

#include "atom/common/node_includes.h"

namespace mate {

template<>
struct Converter<blink::WebTextCheckingResult> {
  static bool FromV8(v8::Isolate* isolate, v8::Handle<v8::Value> val,
                     blink::WebTextCheckingResult* out) {
    mate::Dictionary dict;
    if (!ConvertFromV8(isolate, val, &dict))
      return false;
    return dict.Get("location", &(out->location)) &&
           dict.Get("length", &(out->length));
  }
};

}  // namespace mate

namespace atom {

namespace api {

namespace {

bool HasWordCharacters(const base::string16& text, int index) {
  const base::char16* data = text.data();
  int length = text.length();
  while (index < length) {
    uint32 code = 0;
    U16_NEXT(data, index, length, code);
    UErrorCode error = U_ZERO_ERROR;
    if (uscript_getScript(code, &error) != USCRIPT_COMMON)
      return true;
  }
  return false;
}

}  // namespace

SpellCheckClient::SpellCheckClient(v8::Isolate* isolate,
                                   const std::string& language,
                                   v8::Handle<v8::Object> provider)
    : isolate_(isolate), provider_(isolate, provider) {
  character_attributes_.SetDefaultLanguage(language);

  // Persistent the method.
  mate::Dictionary dict(isolate, provider);
  dict.Get("spellCheck", &spell_check_);
}

SpellCheckClient::~SpellCheckClient() {}

void SpellCheckClient::spellCheck(
    const blink::WebString& text,
    int& misspelling_start,
    int& misspelling_len,
    blink::WebVector<blink::WebString>* optional_suggestions) {
  if (text.length() == 0 || spell_check_.IsEmpty())
    return;

  base::string16 word;
  int word_start;
  int word_length;
  if (!text_iterator_.IsInitialized() &&
      !text_iterator_.Initialize(&character_attributes_, true)) {
      // We failed to initialize text_iterator_, return as spelled correctly.
      VLOG(1) << "Failed to initialize SpellcheckWordIterator";
      return;
  }

  base::string16 in_word(text);
  text_iterator_.SetText(in_word.c_str(), in_word.size());
  while (text_iterator_.GetNextWord(&word, &word_start, &word_length)) {
    // Found a word (or a contraction) that the spellchecker can check the
    // spelling of.
    if (CheckSpelling(word))
      continue;

    // If the given word is a concatenated word of two or more valid words
    // (e.g. "hello:hello"), we should treat it as a valid word.
    if (IsValidContraction(word))
      continue;

    misspelling_start = word_start;
    misspelling_len = word_length;
    return;
  }
}

void SpellCheckClient::checkTextOfParagraph(
    const blink::WebString& text,
    blink::WebTextCheckingTypeMask mask,
    blink::WebVector<blink::WebTextCheckingResult>* results) {
  if (!results)
    return;

  if (!(mask & blink::WebTextCheckingTypeSpelling))
    return;

  NOTREACHED() << "checkTextOfParagraph should never be called";
}

void SpellCheckClient::requestCheckingOfText(
    const blink::WebString& textToCheck,
    const blink::WebVector<uint32_t>& markersInText,
    const blink::WebVector<unsigned>& markerOffsets,
    blink::WebTextCheckingCompletion* completionCallback) {
  base::string16 text(textToCheck);
  if (text.empty() || !HasWordCharacters(text, 0)) {
    completionCallback->didCancelCheckingText();
    return;
  }

  std::vector<blink::WebTextCheckingResult> result;
  if (!CallProviderMethod("requestCheckingOfText", textToCheck, &result)) {
    completionCallback->didCancelCheckingText();
    return;
  }

  completionCallback->didFinishCheckingText(result);
  return;
}

blink::WebString SpellCheckClient::autoCorrectWord(
    const blink::WebString& misspelledWord) {
  base::string16 result;
  if (!CallProviderMethod("autoCorrectWord", misspelledWord, &result))
    return blink::WebString();

  return result;
}

void SpellCheckClient::showSpellingUI(bool show) {
}

bool SpellCheckClient::isShowingSpellingUI() {
  return false;
}

void SpellCheckClient::updateSpellingUIWithMisspelledWord(
    const blink::WebString& word) {
}

template<class T>
bool SpellCheckClient::CallProviderMethod(const char* method,
                                          const blink::WebString& text,
                                          T* result) {
  v8::HandleScope handle_scope(isolate_);

  v8::Handle<v8::Object> provider = provider_.NewHandle();
  if (!provider->Has(mate::StringToV8(isolate_, method)))
    return false;

  v8::Handle<v8::Value> v8_str =
      mate::ConvertToV8(isolate_, base::string16(text));
  v8::Handle<v8::Value> v8_result =
      node::MakeCallback(isolate_, provider, method, 1, &v8_str);

  return mate::ConvertFromV8(isolate_, v8_result, result);;
}

bool SpellCheckClient::CheckSpelling(const base::string16& word_to_check) {
  if (spell_check_.IsEmpty())
    return true;

  v8::HandleScope handle_scope(isolate_);
  v8::Handle<v8::Value> word = mate::ConvertToV8(isolate_, word_to_check);
  v8::Handle<v8::Value> result = spell_check_.NewHandle()->Call(
      provider_.NewHandle(), 1, &word);

  if (result->IsBoolean())
    return result->BooleanValue();
  else
    return true;
}

// Returns whether or not the given string is a valid contraction.
// This function is a fall-back when the SpellcheckWordIterator class
// returns a concatenated word which is not in the selected dictionary
// (e.g. "in'n'out") but each word is valid.
bool SpellCheckClient::IsValidContraction(const base::string16& contraction) {
  if (!contraction_iterator_.IsInitialized() &&
      !contraction_iterator_.Initialize(&character_attributes_, false)) {
    // We failed to initialize the word iterator, return as spelled correctly.
    VLOG(1) << "Failed to initialize contraction_iterator_";
    return true;
  }

  contraction_iterator_.SetText(contraction.c_str(), contraction.length());

  base::string16 word;
  int word_start;
  int word_length;

  while (contraction_iterator_.GetNextWord(&word, &word_start, &word_length)) {
    if (!CheckSpelling(word))
      return false;
  }
  return true;
}

}  // namespace api

}  // namespace atom
