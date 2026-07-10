// JSON as a data table: an array of objects becomes a constexpr
// std::array of plain structs, so the data is authored as JSON but
// consumed as ordinary C++ - indexable, searchable at compile time,
// with no parsing or allocation at runtime.
//
// Build: make tables

#include <ctjson.hpp>
#include <array>
#include <iostream>
#include <string_view>

constexpr auto materials = ctjson::parse<R"([
	{"name": "aluminium", "density": 2.70,  "melting_point": 933},
	{"name": "iron",      "density": 7.87,  "melting_point": 1811},
	{"name": "copper",    "density": 8.96,  "melting_point": 1358},
	{"name": "tungsten",  "density": 19.25, "melting_point": 3695}
])">();

struct material {
	std::string_view name;
	double density;
	int melting_point;
};

template <size_t... Is> constexpr auto to_table(std::index_sequence<Is...>) {
	return std::array<material, sizeof...(Is)>{{
		material{
			materials.get<Is>().template get<"name">().view(),
			materials.get<Is>().template get<"density">().template to<double>(),
			materials.get<Is>().template get<"melting_point">().template to<int>(),
		}...
	}};
}

constexpr auto table = to_table(std::make_index_sequence<materials.size()>{});

// the table is ordinary constexpr data
static_assert(table.size() == 4);
static_assert(table[1].name == "iron");
static_assert(table[3].density > 19.0);

// compile-time queries over it
constexpr const material & densest = []() -> const material & {
	size_t best = 0;
	for (size_t i = 1; i < table.size(); ++i) {
		if (table[i].density > table[best].density) {
			best = i;
		}
	}
	return table[best];
}();
static_assert(densest.name == "tungsten");

int main() {
	std::cout << "name        density  melts at\n";
	for (const auto & m : table) {
		std::cout << m.name;
		for (size_t pad = m.name.size(); pad < 12; ++pad) std::cout << ' ';
		std::cout << m.density << "     " << m.melting_point << " K\n";
	}
	std::cout << "densest: " << densest.name << " (computed at compile time)\n";
}
