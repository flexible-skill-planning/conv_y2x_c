#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <yaml.h>

#include "mini_regex.h"
#include "tools_common.h"

/*
 *
 */
static yaml_node_t *get_map_value(
	yaml_document_t *doc,
	yaml_node_t *map_node,
	const char *key_str
) {
	if (!map_node || map_node->type != YAML_MAPPING_NODE) return NULL;
	for (yaml_node_pair_t *pair = map_node->data.mapping.pairs.start;
		 pair < map_node->data.mapping.pairs.top; pair++) {
		yaml_node_t *key = yaml_document_get_node(doc, pair->key);
		if (key && key->type == YAML_SCALAR_NODE) {
			if (strcmp((const char *)key->data.scalar.value, key_str) == 0) {
				return yaml_document_get_node(doc, pair->value);
			}
		}
	}
	return NULL;
}

/*
 *
 */
static bool match_pattern(
	const char *pattern, const char *text
) {
	int ret;
	ret = rx_search( pattern, text );
	return (ret == 0);
}

/*
 *  this is declare for recursive call
 */
bool validate_node(
	yaml_document_t *s_doc, yaml_node_t *s_node,
	yaml_document_t *d_doc, yaml_node_t *d_node,
	const char *path
);

/*
 *
 */
bool validate_string_rules(yaml_document_t *s_doc, yaml_node_t *s_node,
						   const char *val, const char *path) {
	bool is_valid = true;
	unsigned long len = strlen(val);

	// minLength
	yaml_node_t *min_l = get_map_value(s_doc, s_node, "minLength");
	if (min_l && min_l->type == YAML_SCALAR_NODE) {
		unsigned long req_len = strtoul((const char *)min_l->data.scalar.value, NULL , 0);
		if (len < req_len) {
			fprintf(stderr, "[ERROR] %s: string length (%lu) is less minLength (%lu)\n", path, len, req_len);
			is_valid = false;
		}
	}

	// maxLength
	yaml_node_t *max_l = get_map_value(s_doc, s_node, "maxLength");
	if (max_l && max_l->type == YAML_SCALAR_NODE) {
		unsigned long req_len = strtoul((const char *)max_l->data.scalar.value, NULL , 0);
		if (len > req_len) {
			fprintf(stderr, "[ERROR] %s: string length (%lu) is over  maxLength (%lu)\n", path, len, req_len);
			is_valid = false;
		}
	}

	// pattern match
	yaml_node_t *pat = get_map_value(s_doc, s_node, "pattern");
	if (pat && pat->type == YAML_SCALAR_NODE) {
		const char *pattern_str = (const char *)pat->data.scalar.value;
		if (!match_pattern(pattern_str, val)) {
			fprintf(stderr, "[ERROR] %s: value '%s' is not match to pattern '%s'\n", path, val, pattern_str);
			is_valid = false;
		}
	}

	// enum 
	yaml_node_t *enum_node = get_map_value(s_doc, s_node, "enum");
	if (enum_node && enum_node->type == YAML_SEQUENCE_NODE) {
		bool found = false;
		for (yaml_node_item_t *it = enum_node->data.sequence.items.start;
			 it < enum_node->data.sequence.items.top; it++) {
			yaml_node_t *item_node = yaml_document_get_node(s_doc, *it);
			if (item_node && item_node->type == YAML_SCALAR_NODE) {
				if (strcmp((const char *)item_node->data.scalar.value, val) == 0) {
					found = true;
					break;
				}
			}
		}
		if (!found) {
			fprintf(stderr, "[ERROR] %s: value '%s' is not contain enum list.\n", path, val);
			is_valid = false;
		}
	}

	return is_valid;
}

// check minimum, maximum) on integer
bool validate_number_rules(
	yaml_document_t *s_doc,
	yaml_node_t *s_node,
	long val,
	const char *path
) {
	bool is_valid = true;

	// minimum
	yaml_node_t *min_n = get_map_value(s_doc, s_node, "minimum");
	if (min_n && min_n->type == YAML_SCALAR_NODE) {
		long min_val = strtol((const char *)min_n->data.scalar.value, NULL, 0);
		if (val < min_val) {
			fprintf(stderr, "%s %ld %ld \n", path, val, min_val);
			//fprintf(stderr, "[ERROR] %s : value ( %l ) is less than minimum ( %lu ) \n", path, val, min_val);
			is_valid = false;
		}
	}

	// maximum
	yaml_node_t *max_n = get_map_value(s_doc, s_node, "maximum");
	if (max_n && max_n->type == YAML_SCALAR_NODE) {
		long max_val = strtol((const char *)max_n->data.scalar.value, NULL, 0);
		if (val > max_val) {
			fprintf(stderr, "[ERROR] %s : value ( %ld ) is over than maximum ( %ld )  \n", path, val, max_val);
			is_valid = false;
		}
	}

	return is_valid;
}

// check  additionalProperties: false
bool validate_additional_properties(
	yaml_document_t *s_doc, yaml_node_t *s_node,
	yaml_document_t *d_doc, yaml_node_t *d_node,
	const char *path
) {
	yaml_node_t *add_props = get_map_value(s_doc, s_node, "additionalProperties");
	if (!add_props || add_props->type != YAML_SCALAR_NODE) return true;

	// false 
	if (strcmp((const char *)add_props->data.scalar.value, "false") != 0) return true;

	yaml_node_t *props_node = get_map_value(s_doc, s_node, "properties");
	bool is_valid = true;

	// Check whether all keys in the data are defined in `properties`.
	for (yaml_node_pair_t *pair = d_node->data.mapping.pairs.start;
		 pair < d_node->data.mapping.pairs.top; pair++) {
		
		yaml_node_t *d_key = yaml_document_get_node(d_doc, pair->key);
		if (d_key && d_key->type == YAML_SCALAR_NODE) {
			const char *key_str = (const char *)d_key->data.scalar.value;

			// search it in properties 
			if (!props_node || !get_map_value(s_doc, props_node, key_str)) {
				fprintf(stderr, "[ERROR] %s: found undefined key = '%s'  (additionalProperties: false)\n", path, key_str);
				is_valid = false;
			}
		}
	}
	return is_valid;
}

// Check Sequence node (items, maxItems)
bool validate_array(
	yaml_document_t *s_doc, yaml_node_t *s_node,
	yaml_document_t *d_doc, yaml_node_t *d_node,
	const char *path
) {
	bool is_valid = true;
	unsigned long array_len = d_node->data.sequence.items.top - d_node->data.sequence.items.start;

	// maxItems
	yaml_node_t *max_items = get_map_value(s_doc, s_node, "maxItems");
	if (max_items && max_items->type == YAML_SCALAR_NODE) {
		unsigned long max_req = strtoul((const char *)max_items->data.scalar.value, NULL, 0);
		if (array_len > max_req) {
			fprintf(stderr, "[ERROR] %s: the number of items in sequense is (%lu) over maxItems (%lu)\n", path, array_len, max_req);
			is_valid = false;
		}
	}

	// check entities under items 
	yaml_node_t *items_schema = get_map_value(s_doc, s_node, "items");
	if (items_schema) {
		int idx = 0;
		for (yaml_node_item_t *it = d_node->data.sequence.items.start;
			 it < d_node->data.sequence.items.top; it++, idx++) {
			
			yaml_node_t *item_data = yaml_document_get_node(d_doc, *it);
			char item_path[256];
			snprintf(item_path, sizeof(item_path), "%s[%d]", path, idx);

			if (!validate_node(s_doc, items_schema, d_doc, item_data, item_path)) {
				is_valid = false;
			}
		}
	}

	return is_valid;
}

/*
 *
*/
bool validate_node(
	yaml_document_t *s_doc, yaml_node_t *s_node,
	yaml_document_t *d_doc, yaml_node_t *d_node,
	const char *path
) {
	if (!s_node || !d_node) return true;
	bool is_valid = true;

	yaml_node_t *type_node = get_map_value(s_doc, s_node, "type");
	if (type_node && type_node->type == YAML_SCALAR_NODE) {
		const char *type_str = (const char *)type_node->data.scalar.value;

		if (strcmp(type_str, "object") == 0) {
			if (d_node->type != YAML_MAPPING_NODE) {
				fprintf(stderr, "[ERROR] %s: Required not match to 'object' type.\n", path);
				return false;
			}
			if (!validate_additional_properties(s_doc, s_node, d_doc, d_node, path)) is_valid = false;

		} else if (strcmp(type_str, "array") == 0) {
			if (d_node->type != YAML_SEQUENCE_NODE) {
				fprintf(stderr, "[ERROR] %s: Required not match to 'array' type.\n", path);
				return false;
			}
			if (!validate_array(s_doc, s_node, d_doc, d_node, path)) is_valid = false;

		} else if (strcmp(type_str, "string") == 0) {
			if (d_node->type != YAML_SCALAR_NODE) {
				fprintf(stderr, "[ERROR] %s: Required not match to 'string' type.\n", path);
				return false;
			}
			const char *val = (const char *)d_node->data.scalar.value;
			if (!validate_string_rules(s_doc, s_node, val, path)) is_valid = false;

		} else if (strcmp(type_str, "integer") == 0) {
			if (d_node->type != YAML_SCALAR_NODE) {
				fprintf(stderr, "[ERROR] %s: Required not match to 'integer' type.\n", path);
				return false;
			}
			const char *val_str = (const char *)d_node->data.scalar.value;
			long val = strtol(val_str, NULL, 00);
			if (!validate_number_rules(s_doc, s_node, val, path)) is_valid = false;
		}
	}

	// check required entities.
	yaml_node_t *req_node = get_map_value(s_doc, s_node, "required");
	if (req_node && req_node->type == YAML_SEQUENCE_NODE && d_node->type == YAML_MAPPING_NODE) {
		for (yaml_node_item_t *it = req_node->data.sequence.items.start;
			 it < req_node->data.sequence.items.top; it++) {
			
			yaml_node_t *req_key = yaml_document_get_node(s_doc, *it);
			if (req_key && req_key->type == YAML_SCALAR_NODE) {
				const char *k_str = (const char *)req_key->data.scalar.value;
				if (!get_map_value(d_doc, d_node, k_str)) {
					fprintf(stderr, "[ERROR] %s: Required key='%s' is not existed\n", path, k_str);
					is_valid = false;
				}
			}
		}
	}

	// check recursive properties
	yaml_node_t *props_node = get_map_value(s_doc, s_node, "properties");
	if (props_node && props_node->type == YAML_MAPPING_NODE && d_node->type == YAML_MAPPING_NODE) {
		for (yaml_node_pair_t *pair = props_node->data.mapping.pairs.start;
			 pair < props_node->data.mapping.pairs.top; pair++) {
			
			yaml_node_t *prop_key = yaml_document_get_node(s_doc, pair->key);
			yaml_node_t *sub_schema = yaml_document_get_node(s_doc, pair->value);

			if (prop_key && prop_key->type == YAML_SCALAR_NODE) {
				const char *key_str = (const char *)prop_key->data.scalar.value;
				yaml_node_t *target_data = get_map_value(d_doc, d_node, key_str);

				if (target_data) {
					char next_path[256];
					snprintf(next_path, sizeof(next_path), "%s.%s", path, key_str);
					if (!validate_node(s_doc, sub_schema, d_doc, target_data, next_path)) {
						is_valid = false;
					}
				}
			}
		}
	}

	return is_valid;
}

/*
 *
*/
bool load_yaml_file(const char *filename, yaml_document_t *doc) {
	FILE *fh = fopen(filename, "rb");
	if (!fh) {
		fprintf(stderr, "can not read %s", filename);
		return false;
	}
	yaml_parser_t parser;
	yaml_parser_initialize(&parser);
	yaml_parser_set_input_file(&parser, fh);

	bool res = yaml_parser_load(&parser, doc);
	if (!res) {
		fprintf(stderr, "YAML parse error (%s): %s\n", filename, parser.problem);
	}
	yaml_parser_delete(&parser);
	fclose(fh);
	return res;
}

int main(int argc, char *argv[]) {
	if (argc < 3) {
		printf("Usage: %s <schema.json|yaml> <user_data.yaml>\n", argv[0]);
		return 1;
	}

	yaml_document_t schema_doc, data_doc;
	if (!load_yaml_file(argv[1], &schema_doc) || !load_yaml_file(argv[2], &data_doc)) {
		return 1;
	}

	yaml_node_t *s_root = yaml_document_get_root_node(&schema_doc);
	yaml_node_t *d_root = yaml_document_get_root_node(&data_doc);

	if (s_root && d_root) {
		printf("--- validation begin ---\n");
		bool ok = validate_node(&schema_doc, s_root, &data_doc, d_root, "root");
		if (ok) {
			printf("\n[SUCCESS] schema validation is pass.\n");
		} else {
			printf("\n[FAILED] schema validation is failed.\n");
		}
	}

	yaml_document_delete(&schema_doc);
	yaml_document_delete(&data_doc);
	return 0;
}

