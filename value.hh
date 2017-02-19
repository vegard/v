#ifndef V_VALUE_HH
#define V_VALUE_HH

enum value_metatype {
	VALUE_GLOBAL,
	VALUE_LOCAL,
	VALUE_CONSTANT,
};

struct value_type;
typedef std::shared_ptr<value_type> value_type_ptr;

struct value;
typedef std::shared_ptr<value> value_ptr;

struct value_type {
	// TODO
	unsigned int alignment;
	unsigned int size;

	// TODO
	value_ptr (*constructor)();
};

struct value {
	value_metatype metatype;

	union {
		struct {
			void *host_address;
		} global;

		struct {
			// offset in the local stack frame
			unsigned int offset;
		} local;

		struct {
			//ast_node_ptr node;
			uint64_t u64;
		} constant;
	};

	// TODO: use value_type_ptr?
	value_type *type;

	value()
	{
	}

	value(value_metatype metatype, value_type *type):
		metatype(metatype),
		type(type)
	{
	}

	~value()
	{
	}
};

// Some builtin types

static value_type builtin_type_void = {0, 0};
static value_type builtin_type_type = {alignof(value_type), sizeof(value_type)};
static value_type builtin_type_boolean= {1, 1};
static value_type builtin_type_int = {alignof(mpz_class), sizeof(mpz_class)};
static value_type builtin_type_uint64 = {8, 8};
static value_type builtin_type_macro = {alignof(void *), sizeof(void *)};

#endif
