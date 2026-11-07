#ifndef ___GENERATED_TYPICAL_H___
#define ___GENERATED_TYPICAL_H___

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef char str32_t[33];
typedef struct {
	str32_t      addr;
	int          bit_pos;
	int          bit_width;
	str32_t      name;
	str32_t      data_type;
	int          default_value;
	str32_t      access;
	char*        description;
	char*        generate[];
	char*        tag[];
	char*        notes;
} regmap_t;

typedef struct {
	char*        prefix_name;
	int          prefix_addr_bit;
	char*        prefix_addr_value;
	regmap    _t regmap [];
} YamlNode_t_t;

#endif /* ___GENERATED_TYPICAL_H___ */
