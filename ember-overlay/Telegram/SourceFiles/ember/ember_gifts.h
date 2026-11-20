/*
This file is part of Ember, a fork of AyuGram Desktop / Telegram Desktop.

Ember gifts: a purely visual gift that is shown on the profile
to other Ember users only. It costs nothing and is not a real
Telegram gift. The choice is stored as an invisible marker at the
end of the user's bio, so no server of our own is required.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include <optional>

class UserData;

namespace Main {
class Session;
} // namespace Main

namespace Window {
class SessionController;
} // namespace Window

namespace Ember {

// Marker format (all characters are invisible):
//   [magic: 3 digits][payload: 12 digits][check: 1 digit]
// where a digit is one of four zero-width characters (base 4),
// and the payload is the low 24 bits of the Telegram star gift id.
inline constexpr auto kGiftTailBits = 24;

[[nodiscard]] QString EncodeGiftMarker(uint64 giftId);
[[nodiscard]] std::optional<uint32> ParseGiftMarker(const QString &about);
[[nodiscard]] QString StripGiftMarker(const QString &about);

// Emits the current Ember gift tail of the user (if any) and updates.
[[nodiscard]] rpl::producer<std::optional<uint32>> GiftTailValue(
	not_null<UserData*> user);

// Own profile: choose / remove the gift.
void ShowGiftPicker(not_null<Window::SessionController*> controller);

// Any profile: show the gift of that user.
void ShowGiftView(
	not_null<Window::SessionController*> controller,
	not_null<UserData*> user);

// Short text in the current UI language (Russian or English).
[[nodiscard]] QString Text(const QString &en, const QString &ru);

} // namespace Ember
