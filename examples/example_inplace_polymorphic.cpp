#include <dice/template-library/inplace_polymorphic.hpp>

#include <iostream>
#include <string>

namespace dtl = dice::template_library;

struct animal {
	// Required: virtual destructor
	virtual ~animal() = default;

	// Shared interface of all derived classes.
	virtual void say_hello() const = 0;

	// Optional: Add this function to enable copy-construction and copy-assignment for inplace_polymorphic.
	virtual void copy_to(animal *dst) const = 0;

	// Optional: Add this function to enable move-construction and move-assignment for inplace_polymorphic.
	virtual void move_to(animal *dst) noexcept = 0;
};

struct dog : animal {
private:
	std::string name_;

public:
	explicit dog(std::string name) : name_{std::move(name)} {
	}

	void say_hello() const override {
		std::cout << name_ << " says bark\n";
	}

	void copy_to(animal *dst) const override {
		new (dst) dog{*this};
	}

	void move_to(animal *dst) noexcept override {
		new (dst) dog{std::move(*this)};
	}
};

struct cat : animal {
private:
	bool good_mood_;

public:
	explicit cat(bool good_mood = false) : good_mood_{good_mood} {
	}

	void say_hello() const override {
		if (good_mood_) {
			std::cout << "meow\n";
		} else {
			std::cout << "<ignores you>\n";
		}
	}

	void copy_to(animal *dst) const override {
		new (dst) cat{*this};
	}

	void move_to(animal *dst) noexcept override {
		new (dst) cat{std::move(*this)};
	}
};

using any_animal = dtl::inplace_polymorphic<animal, cat, dog>;

int main() {
	any_animal an;
	an->say_hello();

	an.template emplace<dog>("Spark");
	an->say_hello();

	any_animal an2 = an;
	an2->say_hello();
}
