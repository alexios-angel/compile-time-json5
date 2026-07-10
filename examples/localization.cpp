// A message catalog with compile-time lookup and fallback: contains<>
// makes optional keys an if-constexpr decision, so a missing
// translation falls back to the default language - resolved entirely
// at compile time, with a static_assert guaranteeing the fallback
// language is complete.
//
// Build: make localization

#include <ctjson5.hpp>
#include <iostream>
#include <string_view>

constexpr auto catalog = ctjson5::parse<R"({
	"en": {
		"greeting":  "Hello",
		"farewell":  "Goodbye",
		"thanks":    "Thank you"
	},
	"cs": {
		"greeting":  "Ahoj",
		"thanks":    "Děkuji"
	}
})">();

// every key used below must exist in the fallback language
template <ctll::fixed_string Key> constexpr std::string_view message_in_english() {
	static_assert(decltype(catalog.get<"en">())::template contains<Key>(),
		"the fallback language must define every message key");
	return catalog.get<"en">().get<Key>().view();
}

// czech if translated, english otherwise - decided while compiling
template <ctll::fixed_string Key> constexpr std::string_view message_in_czech() {
	if constexpr (decltype(catalog.get<"cs">())::template contains<Key>()) {
		return catalog.get<"cs">().get<Key>().view();
	} else {
		return message_in_english<Key>();
	}
}

static_assert(message_in_czech<"greeting">() == "Ahoj");
static_assert(message_in_czech<"farewell">() == "Goodbye"); // fell back
static_assert(message_in_czech<"thanks">() == "Děkuji");

int main() {
	std::cout << "greeting: " << message_in_czech<"greeting">() << "\n";
	std::cout << "farewell: " << message_in_czech<"farewell">() << " (fallback)\n";
	std::cout << "thanks:   " << message_in_czech<"thanks">() << "\n";

	// iterate a whole language at runtime
	std::cout << "english catalog:\n";
	ctjson5::for_each(catalog.get<"en">(), [](auto key, auto value) {
		std::cout << "  " << key.view() << " = " << value.view() << "\n";
	});
}
