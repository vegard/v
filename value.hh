#ifndef V_VALUE_HH
#define V_VALUE_HH

enum value_storage_type {
	VALUE_GLOBAL,
	VALUE_LOCAL,
	VALUE_CONSTANT,
};

struct value_type;
typedef std::shared_ptr<value_type> value_type_ptr;

struct value;
typedef std::shared_ptr<value> value_ptr;

struct function;
struct scope;
typedef std::shared_ptr<scope> scope_ptr;

struct ast_node;
typedef std::shared_ptr<ast_node> ast_node_ptr;

struct value_type {
	// TODO
	unsigned int alignment;
	unsigned int size;

	// TODO
	value_ptr (*constructor)();

	value_ptr (*call)(function &f, scope_ptr s, value_ptr lhs, ast_node_ptr rhs);
};

struct value {
	value_storage_type storage_type;

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

	value_type_ptr type;

	value()
	{
	}

	value(value_storage_type storage_type, value_type_ptr type):
		storage_type(storage_type),
		type(type)
	{
	}

	~value()
	{
	}
};

// Some builtin types

static auto builtin_type_void = std::make_shared<value_type>(value_type{0, 0});
static auto builtin_type_type = std::make_shared<value_type>(value_type{alignof(value_type), sizeof(value_type)});
static auto builtin_type_boolean= std::make_shared<value_type>(value_type{1, 1});

// TODO: "int" is 64-bit for the time being, see copmile(AST_LITERAL_INTEGER) in compile.hh
//static value_type builtin_type_int = {alignof(mpz_class), sizeof(mpz_class)};
static auto builtin_type_int = std::make_shared<value_type>(value_type{8, 8});

static auto builtin_type_macro = std::make_shared<value_type>(value_type{alignof(void *), sizeof(void *)});

#endif
