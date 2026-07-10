// Walking a document generically: every node carries its kind, objects
// and arrays are positionally iterable, so a recursive if-constexpr
// visitor can re-serialize (or transform) any document - the whole
// traversal is resolved at compile time, only the printing runs.
//
// Build: make introspection

#include <ctjson5.hpp>
#include <iostream>

template <typename Node> void print(Node node, int indent = 0);

template <typename Object, std::size_t... Is> void print_members(Object, int indent, std::index_sequence<Is...>) {
	((std::cout << std::string(static_cast<size_t>(indent + 2), ' ') << '"' << Object::template key<Is>().view() << "\": ",
	  print(Object::template value<Is>(), indent + 2),
	  std::cout << (Is + 1 < sizeof...(Is) ? ",\n" : "\n")), ...);
}

template <typename Array, std::size_t... Is> void print_elements(Array, int indent, std::index_sequence<Is...>) {
	((std::cout << std::string(static_cast<size_t>(indent + 2), ' '),
	  print(Array::template get<Is>(), indent + 2),
	  std::cout << (Is + 1 < sizeof...(Is) ? ",\n" : "\n")), ...);
}

template <typename Node> void print(Node node, int indent) {
	if constexpr (Node::type == ctjson5::kind::object) {
		if constexpr (Node::empty()) {
			std::cout << "{}";
		} else {
			std::cout << "{\n";
			print_members(node, indent, std::make_index_sequence<Node::size()>{});
			std::cout << std::string(static_cast<size_t>(indent), ' ') << "}";
		}
	} else if constexpr (Node::type == ctjson5::kind::array) {
		if constexpr (Node::empty()) {
			std::cout << "[]";
		} else {
			std::cout << "[\n";
			print_elements(node, indent, std::make_index_sequence<Node::size()>{});
			std::cout << std::string(static_cast<size_t>(indent), ' ') << "]";
		}
	} else if constexpr (Node::type == ctjson5::kind::string) {
		std::cout << '"' << Node::view() << '"';
	} else if constexpr (Node::type == ctjson5::kind::number) {
		std::cout << Node::view();
	} else if constexpr (Node::type == ctjson5::kind::boolean) {
		std::cout << (node ? "true" : "false");
	} else {
		std::cout << "null";
	}
}

constexpr auto doc = ctjson5::parse<R"({
	"library": "ctjson5",
	"constexpr": true,
	"depths": [1, [2, [3]]],
	"empty_things": {"o": {}, "a": []},
	"unicode": "café 😀"
})">();

int main() {
	print(doc);
	std::cout << "\n";
}
