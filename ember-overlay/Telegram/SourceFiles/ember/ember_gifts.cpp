/*
This file is part of Ember, a fork of AyuGram Desktop / Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "ember/ember_gifts.h"

#include "api/api_premium.h"
#include "apiwrap.h"
#include "data/data_changes.h"
#include "data/data_peer.h"
#include "data/data_premium_limits.h"
#include "data/data_session.h"
#include "data/data_star_gift.h"
#include "data/data_user.h"
#include "info/peer_gifts/info_peer_gifts_common.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "ui/layers/generic_box.h"
#include "ui/widgets/labels.h"
#include "window/window_session_controller.h"
#include "styles/style_boxes.h"
#include "styles/style_credits.h"
#include "styles/style_layers.h"

#include <QtCore/QLocale>

#include <array>

namespace Ember {
namespace {

using namespace Info::PeerGifts;

constexpr auto kDigits = std::array<char16_t, 4>{
	char16_t(0x200B), // zero width space
	char16_t(0x200C), // zero width non-joiner
	char16_t(0x200D), // zero width joiner
	char16_t(0x2060), // word joiner
};
constexpr auto kMagic = std::array<int, 3>{ 2, 3, 2 };
constexpr auto kPayloadDigits = kGiftTailBits / 2;
constexpr auto kMarkerLength = int(kMagic.size()) + kPayloadDigits + 1;
constexpr auto kTailMask = (uint64(1) << kGiftTailBits) - 1;

[[nodiscard]] int DigitOf(QChar ch) {
	for (auto i = 0; i != int(kDigits.size()); ++i) {
		if (ch.unicode() == kDigits[i]) {
			return i;
		}
	}
	return -1;
}

// Returns the position of the marker start or -1.
[[nodiscard]] int FindMarker(const QString &text, uint32 *tail) {
	const auto size = int(text.size());
	for (auto from = 0; from + kMarkerLength <= size; ++from) {
		auto ok = true;
		for (auto i = 0; i != int(kMagic.size()); ++i) {
			if (DigitOf(text[from + i]) != kMagic[i]) {
				ok = false;
				break;
			}
		}
		if (!ok) {
			continue;
		}
		auto value = uint32(0);
		auto sum = 0;
		for (auto i = 0; i != kPayloadDigits; ++i) {
			const auto digit = DigitOf(text[from + int(kMagic.size()) + i]);
			if (digit < 0) {
				ok = false;
				break;
			}
			value = (value << 2) | uint32(digit);
			sum += digit;
		}
		if (!ok) {
			continue;
		}
		const auto check = DigitOf(text[from + kMarkerLength - 1]);
		if (check != (sum % 4)) {
			continue;
		}
		if (tail) {
			*tail = value;
		}
		return from;
	}
	return -1;
}

[[nodiscard]] bool MatchesTail(const Data::StarGift &gift, uint32 tail) {
	return !gift.unique && ((gift.id & kTailMask) == uint64(tail));
}

struct GiftsState {
	explicit GiftsState(not_null<Main::Session*> session)
	: options(session->user())
	, delegate(session, GiftButtonMode::Minimal) {
	}

	Api::PremiumGiftCodeOptions options;
	Delegate delegate;
	std::vector<not_null<GiftButton*>> buttons;
	std::vector<uint64> ids;
	rpl::variable<int> selected = -1;
};

} // namespace

QString Text(const QString &en, const QString &ru) {
	return (QLocale().language() == QLocale::Russian) ? ru : en;
}

QString EncodeGiftMarker(uint64 giftId) {
	auto result = QString();
	result.reserve(kMarkerLength);
	for (const auto digit : kMagic) {
		result.append(QChar(kDigits[digit]));
	}
	const auto tail = uint32(giftId & kTailMask);
	auto sum = 0;
	for (auto i = kPayloadDigits - 1; i >= 0; --i) {
		const auto digit = int((tail >> (i * 2)) & 3);
		sum += digit;
		result.append(QChar(kDigits[digit]));
	}
	result.append(QChar(kDigits[sum % 4]));
	return result;
}

std::optional<uint32> ParseGiftMarker(const QString &about) {
	auto tail = uint32(0);
	return (FindMarker(about, &tail) >= 0)
		? std::make_optional(tail)
		: std::nullopt;
}

QString StripGiftMarker(const QString &about) {
	auto result = about;
	while (true) {
		const auto at = FindMarker(result, nullptr);
		if (at < 0) {
			return result;
		}
		result.remove(at, kMarkerLength);
	}
}

rpl::producer<std::optional<uint32>> GiftTailValue(
		not_null<UserData*> user) {
	return user->session().changes().peerFlagsValue(
		user,
		Data::PeerUpdate::Flag::About
	) | rpl::map([=] {
		return ParseGiftMarker(user->about());
	}) | rpl::distinct_until_changed();
}

void ShowGiftPicker(not_null<Window::SessionController*> controller) {
	const auto session = &controller->session();
	controller->show(Box([=](not_null<Ui::GenericBox*> box) {
		const auto state = box->lifetime().make_state<GiftsState>(session);

		box->setTitle(rpl::single(Text(
			u"Ember gift"_q,
			u"Ember-подарок"_q)));
		box->setWidth(st::boxWideWidth);
		box->addRow(
			object_ptr<Ui::FlatLabel>(
				box,
				rpl::single(Text(
					u"Pick a gift. It is free and visible only to Ember users who open your profile."_q,
					u"Выберите подарок. Он бесплатный и виден только пользователям Ember в вашем профиле."_q)),
				st::boxLabel),
			st::giftBoxAboutMargin,
			style::al_top);

		const auto gifts = box->addRow(
			object_ptr<Ui::RpWidget>(box),
			QMargins(
				st::giftBoxPadding.left(),
				st::giftBoxPadding.top(),
				st::giftBoxPadding.right(),
				0));

		const auto layout = [=](int width) {
			const auto singleMin = state->delegate.buttonSize();
			if (width < singleMin.width() || state->buttons.empty()) {
				return;
			}
			const auto count = int(state->buttons.size());
			const auto skipw = st::giftBoxGiftSkip.x();
			const auto skiph = st::giftBoxGiftSkip.y();
			const auto perRow = std::min(
				(width + skipw) / (singleMin.width() + skipw),
				std::max(count, 1));
			if (perRow <= 0) {
				return;
			}
			const auto single = (width - (perRow - 1) * skipw) / perRow;
			const auto height = singleMin.height();
			const auto rows = (count + perRow - 1) / perRow;
			for (auto index = 0; index != count; ++index) {
				const auto row = index / perRow;
				const auto column = index % perRow;
				state->buttons[index]->setGeometry(
					QRect(
						column * (single + skipw),
						row * (height + skiph),
						single,
						height),
					state->delegate.buttonExtend());
			}
			gifts->resize(width, rows * (height + skiph) - skiph);
		};
		gifts->widthValue() | rpl::on_next(layout, gifts->lifetime());

		state->options.requestStarGifts(
		) | rpl::on_error_done([=](QString error) {
		}, [=] {
			for (const auto &gift : state->options.starGifts()) {
				if (gift.unique) {
					continue;
				}
				const auto index = int(state->buttons.size());
				state->ids.push_back(gift.id);
				state->buttons.push_back(
					Ui::CreateChild<GiftButton>(gifts, &state->delegate));
				const auto button = state->buttons.back();
				button->setDescriptor(
					GiftTypeStars{ .info = gift },
					GiftButton::Mode::Minimal);
				button->setClickedCallback([=] {
					const auto now = state->selected.current();
					state->selected = (now == index) ? -1 : index;
				});
				button->show();
			}
			layout(gifts->width());
		}, box->lifetime());

		state->selected.value(
		) | rpl::combine_previous(
		) | rpl::on_next([=](int old, int now) {
			if (old >= 0 && old < int(state->buttons.size())) {
				state->buttons[old]->toggleSelected(false);
			}
			if (now >= 0 && now < int(state->buttons.size())) {
				state->buttons[now]->toggleSelected(true);
			}
		}, box->lifetime());

		const auto save = [=](std::optional<uint64> id) {
			const auto self = session->user();
			auto text = StripGiftMarker(self->about());
			if (id) {
				text += EncodeGiftMarker(*id);
			}
			const auto limit
				= Data::PremiumLimits(session).aboutLengthCurrent();
			if (text.size() > limit) {
				controller->showToast(Text(
					u"Your bio is too long to store the gift. Shorten it a little."_q,
					u"Био слишком длинное, чтобы сохранить подарок. Сократите его."_q));
				return;
			}
			session->api().saveSelfBio(text);
			box->closeBox();
		};
		box->addButton(rpl::single(Text(u"Apply"_q, u"Применить"_q)), [=] {
			const auto index = state->selected.current();
			if (index < 0 || index >= int(state->ids.size())) {
				return;
			}
			save(state->ids[index]);
		});
		box->addLeftButton(
			rpl::single(Text(u"Remove"_q, u"Убрать"_q)),
			[=] { save(std::nullopt); });
		box->addButton(tr::lng_close(), [=] { box->closeBox(); });
	}));
}

void ShowGiftView(
		not_null<Window::SessionController*> controller,
		not_null<UserData*> user) {
	const auto session = &controller->session();
	controller->show(Box([=](not_null<Ui::GenericBox*> box) {
		const auto state = box->lifetime().make_state<GiftsState>(session);
		const auto tail = ParseGiftMarker(user->about());

		box->setTitle(rpl::single(Text(
			u"Ember gift"_q,
			u"Ember-подарок"_q)));
		box->setWidth(st::boxWideWidth);
		const auto label = box->addRow(
			object_ptr<Ui::FlatLabel>(
				box,
				rpl::single(Text(
					u"This gift is visible only to Ember users."_q,
					u"Этот подарок видят только пользователи Ember."_q)),
				st::boxLabel),
			st::giftBoxAboutMargin,
			style::al_top);
		Q_UNUSED(label);

		const auto holder = box->addRow(
			object_ptr<Ui::RpWidget>(box),
			QMargins(
				st::giftBoxPadding.left(),
				st::giftBoxPadding.top(),
				st::giftBoxPadding.right(),
				st::giftBoxPadding.bottom()),
			style::al_top);
		const auto size = state->delegate.buttonSize();
		holder->resize(size.width(), size.height());

		if (tail) {
			const auto wanted = *tail;
			state->options.requestStarGifts(
			) | rpl::on_error_done([=](QString error) {
			}, [=] {
				for (const auto &gift : state->options.starGifts()) {
					if (!MatchesTail(gift, wanted)) {
						continue;
					}
					const auto button = Ui::CreateChild<GiftButton>(
						holder,
						&state->delegate);
					button->setDescriptor(
						GiftTypeStars{ .info = gift },
						GiftButton::Mode::Minimal);
					button->setGeometry(
						QRect(QPoint(), state->delegate.buttonSize()),
						state->delegate.buttonExtend());
					button->show();
					break;
				}
			}, box->lifetime());
		}
		box->addButton(tr::lng_close(), [=] { box->closeBox(); });
	}));
}

} // namespace Ember
