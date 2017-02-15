#ifndef V_VALUE_HH
#define V_VALUE_HH

enum value_metatype {
	VALUE_GLOBAL,
	VALUE_LOCAL,
	VALUE_CONSTANT,
};

struct value_type;
typedef std::shared_ptr<value_type> value_type_ptr;

struct value_type {
	// TODO
};

struct value;
typedef std::shared_ptr<value> value_ptr;

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

// All builtin types
// TODO: set sizes, etc.
static value_type void_type;
static value_type boolean_type;
static value_type int_type;
static value_type builtin_macro_type;

#endif
