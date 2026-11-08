// FnFNitro's Mods page text in the game's language (see lang.cpp).
#pragma once

// The language the game had last time (kept in `ini`; before it has ever been seen, Azimuth's locale):
// before tr() is first used.
void lang_init(const wchar_t* ini, const wchar_t* azimuth_dir);
// Notes the game's text language in the ini, for the Mods page next time. Cheap; call it now and then.
void lang_poll(const void* game_base);
// `english` in that language; itself when there's no translation.
const char* tr(const char* english);
// That language's code: "EN", "DE", "CHT"...
const char* lang_code();
