#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <yaml.h>

#define MAX_FIELD_NAME_LEN	64
#define MAX_TYPE_NAME_LEN	32
#define MAX_PROPERTIES		64
#define MAX_NODES			32

#define STR_MAX_LENGTH_INVALID	255

#define MAX_FILEPATH_NAME	256
#define MAX_GEN_FILENAME_LEN	32


typedef struct SchemaProperty SchemaProperty_t;
struct SchemaProperty{
	char name[MAX_FIELD_NAME_LEN];
	char type[MAX_TYPE_NAME_LEN];
	bool is_array;
	SchemaProperty_t *child;
	SchemaProperty_t *next;
} ;


// -----------------------------------------------------------------------------
/**
*
*
*
*/
static bool validate_field_name(const char *name) {
	int len = strlen(name);

	if (len >= MAX_FIELD_NAME_LEN) {
		fprintf(stderr,
			"error: parameter '%s'(%d letters)  touchs a limitation of %3u letters.\n",
			name, len, MAX_FIELD_NAME_LEN - 1
		);
		return false;
	}

	if (len == 0) {
		fprintf(stderr,
			"error: 'name' item is failed. \n"
		);
		return false;
	}

	if (!isalpha((unsigned char)name[0])) {
		fprintf(stderr,
			"error: parameter '%s' should be alphabet at first letter. (detect='%c')\n",
			name, name[0]
		);
		return false;
	}

	for (size_t i = 1; i < len; i++) {
		if (!isalnum((unsigned char)name[i]) && name[i] != '_') {
			fprintf(stderr,
				"error: paramter '%s' should be alphabet, number or underbar at first letter. (detect='%c')\n",
				name, name[i]
			);
			return false;
		}
	}

	return true;
}

/**
*
*
*
*/
static bool validate_type_name(const char *type_name) {
	int len = strlen(type_name);
	if (len >= MAX_TYPE_NAME_LEN) {
		fprintf(stderr, "error: type '%s'(%d letters) touchs a limitation of %3u letters.\n",
				type_name, len, MAX_TYPE_NAME_LEN - 1);
		return false;
	}
	return true;
}

/**
*
*
*
*/
void make_struct_type_name(const char *raw_name, char *out_buf, size_t buf_size) {
	snprintf(out_buf, buf_size, "%sItem", raw_name);
	out_buf[0] = (char)toupper((unsigned char)out_buf[0]);
}

/**
*
*
*
*/
const char* map_json_type_to_c(const char *json_type, const int max_length) {
	if (strcmp(json_type, "object") == 0)  return "struct";
	if (strcmp(json_type, "string") == 0 && max_length<=32)  return "str32_t";
	if (strcmp(json_type, "string") == 0 && max_length>32)  return "char*";
	if (strcmp(json_type, "string") == 0)  return "char*";
	if (strcmp(json_type, "integer") == 0) return "int";
	if (strcmp(json_type, "number") == 0)  return "double";
	if (strcmp(json_type, "boolean") == 0) return "char";
	return "char*"; 
}

/**
*
*
*
*/
void free_schema_properties(SchemaProperty_t *head) {
	while (head) {
		SchemaProperty_t *p_child;
		SchemaProperty_t *p_next;
		p_child = head->child;
		p_next = head->next;
		if (p_child) {
			free_schema_properties( p_child );
		}
		free(head);
		head = p_next;
	}
}

// -----------------------------------------------------------------------------
// analyze schema structure

// declaration for recuisive process
bool parse_properties(yaml_document_t *doc, yaml_node_t *root_node, SchemaProperty_t **list);

/**
*
*
*
*/
bool parse_single_property(yaml_document_t *doc, const char *prop_name, yaml_node_t *prop_def_node, SchemaProperty_t **list) {
	if (!validate_field_name(prop_name)) {
		return false;
	}

	SchemaProperty_t *prop = calloc(1, sizeof(SchemaProperty_t));
	if (!prop) {
		fprintf(stderr, "error: memory allocation failed.\n");
		return false;
	}
	snprintf(prop->name, sizeof(prop->name), "%s", prop_name);

	yaml_node_pair_t *pair;
	const char *json_type = "string"; 
	int max_length = STR_MAX_LENGTH_INVALID;
	bool is_array = false;
	yaml_node_t *items_node = NULL;

	for (pair = prop_def_node->data.mapping.pairs.start; pair < prop_def_node->data.mapping.pairs.top; pair++) {
		yaml_node_t *k = yaml_document_get_node(doc, pair->key);
		yaml_node_t *v = yaml_document_get_node(doc, pair->value);

		if (k->type == YAML_SCALAR_NODE) {
			const char *key_str = (char*)k->data.scalar.value;
			if (strcmp(key_str, "type") == 0 && v->type == YAML_SCALAR_NODE) {
				if( strcmp((char*)v->data.scalar.value, "array") == 0 ) {
					is_array = true;
				}
				else {
					json_type = (char*)v->data.scalar.value;
					if( validate_type_name(json_type) == false ) {
						return false;
					}
				}
			}
			else if (strcmp(key_str, "maxLength") == 0) {
				max_length = strtol( (char*)v->data.scalar.value, NULL, 0 );
			}
			else if (strcmp(key_str, "items") == 0) {
				yaml_node_pair_t *i_pair;
				for (i_pair = v->data.mapping.pairs.start; i_pair < v->data.mapping.pairs.top; i_pair++) {
					yaml_node_t *i_k = yaml_document_get_node(doc, i_pair->key);
					yaml_node_t *i_v = yaml_document_get_node(doc, i_pair->value);
					if (strcmp((char *)i_k->data.scalar.value, "type") == 0
						&& strcmp((char *)i_v->data.scalar.value, "object") == 0
						&& i_v->type == YAML_SCALAR_NODE
					) {
						items_node = v;
						json_type = (char*)i_v->data.scalar.value;
					}
				}
			}

		}
	}

	const char *c_ttype_name;
	c_ttype_name = map_json_type_to_c(json_type, max_length);
	snprintf(prop->type, sizeof(prop->type), "%s", c_ttype_name);
	prop->is_array = is_array;
	*list = prop;
	if ( items_node!=NULL ) {
		parse_properties(doc, items_node, &((*list)->child) ) ;
	}


	return true;
}

/**
*
*
*
*/
bool parse_properties(yaml_document_t *doc, yaml_node_t *root_node, SchemaProperty_t **list) {
	yaml_node_pair_t *pair;
	yaml_node_t *props_node = NULL;

	// search "properties" entity
	for (pair = root_node->data.mapping.pairs.start; pair < root_node->data.mapping.pairs.top; pair++) {
		yaml_node_t *k = yaml_document_get_node(doc, pair->key);
		yaml_node_t *v = yaml_document_get_node(doc, pair->value);

		if (k->type == YAML_SCALAR_NODE && strcmp((char*)k->data.scalar.value, "properties") == 0) {
			if (v->type == YAML_MAPPING_NODE) {
				props_node = v;
				break;
			}
		}
	}
	if (props_node == NULL) {
		return false;
	}
	
	// process each property.
	SchemaProperty_t **prop_ptr;
	prop_ptr = list;
	for (pair = props_node->data.mapping.pairs.start; pair < props_node->data.mapping.pairs.top; pair++) {
		yaml_node_t *k = yaml_document_get_node(doc, pair->key);
		yaml_node_t *v = yaml_document_get_node(doc, pair->value);

		if (k->type == YAML_SCALAR_NODE && v->type == YAML_MAPPING_NODE) {
			const char *prop_name = (char*)k->data.scalar.value;
			if (!parse_single_property(doc, prop_name, v, prop_ptr)) {
				return false; 
			}
			prop_ptr = &((*prop_ptr)->next);
		}
	}
	return true;
}

/**
*
*
*
*/
bool generate_schema_ast(const char *filepath, SchemaProperty_t **node_list) {

	// erro check =================================
	FILE *file = fopen(filepath, "rt");
	if (!file) {
		fprintf(stderr, "error: input file '%s' is not opened.\n", filepath);
		return false;
	}

	yaml_parser_t parser;
	yaml_document_t doc;

	if (!yaml_parser_initialize(&parser)) {
		fprintf(stderr, "error: initialize fault happened at YAML parser. \n");
		fclose(file);
		return false;
	}

	yaml_parser_set_input_file(&parser, file);

	if (!yaml_parser_load(&parser, &doc)) {
		fprintf(stderr, "error: a fault on analysis happened on'%s'.\n", filepath);
		yaml_parser_delete(&parser);
		fclose(file);
		return false;
	}

	yaml_node_t *root;
	root = yaml_document_get_root_node(&doc);
	if (!root || root->type != YAML_MAPPING_NODE) {
		fprintf(stderr, "error: Schema root should be Object(Mapping) style.\n");
		yaml_document_delete(&doc);
		yaml_parser_delete(&parser);
		fclose(file);
		return false;
	}
	
	// =======================================================
	// search root node

	bool ret_val = parse_properties( &doc, doc.nodes.start, node_list );

	if (ret_val) {
		if (*node_list == NULL) {
			fprintf(stderr, "error: perse result is null.\n");
		}
	}
	else if( (*node_list)->child == NULL ){
		fprintf(stderr, "error: not found 'properties' field in properties.\n");
	}
	else {
		fprintf(stderr, "error: parse issue\n");
	}

	yaml_document_delete(&doc);
	yaml_parser_delete(&parser);
	fclose(file);

	return true;
}

// -----------------------------------------------------------------------------
/**
*
*
*
*/
void print_header_file_sub(const char *struct_name, SchemaProperty_t *node_list, FILE *fp) {
	SchemaProperty_t *p;
	p = node_list;
	while (p) {
		if (p->child != NULL) {
			print_header_file_sub( p->name, p->child, fp );
		}
		p = p->next;
	}

	p = node_list;
	fprintf(fp, "typedef struct {\n");
	while (p) {
		if (p->child != NULL) {
			fprintf(fp, "	%-10s_t %s ", p->name, p->name);
		}
		else {
			fprintf(fp,"	%-12s %s", p->type, p->name);
		}
		
		if( p->is_array ) {
			fprintf(fp,"[]");
		}
		fprintf(fp,";\n");
		p = p->next;
	}
	fprintf(fp, "} %s_t;\n\n", struct_name);
}


/**
*
*
*
*/
void print_header_file(const char*root_struct_name, SchemaProperty_t *node_list, FILE *fp) {
	fprintf(fp, "#ifndef ___GENERATED_TYPICAL_H___\n");
	fprintf(fp, "#define ___GENERATED_TYPICAL_H___\n\n");
	fprintf(fp, "#include <stdint.h>\n");
	fprintf(fp, "#include <stdbool.h>\n");
	fprintf(fp, "#include <stddef.h>\n\n");
	
	fprintf(fp, "typedef char str32_t[33];\n");
	
	print_header_file_sub( root_struct_name, node_list, fp );

	fprintf(fp, "#endif /* ___GENERATED_TYPICAL_H___ */\n");
}

/**
*
*
*
*/
void print_body_file(const char*root_struct_name, SchemaProperty_t *node_list, FILE *fp) {
	fprintf(fp, "#include \"generated_typical.h\"\n");
	
	fprintf(fp, "typedef char str32_t[33];\n");
	
	print_header_file_sub( root_struct_name, node_list, fp );

	fprintf(fp, "#endif /* ___GENERATED_TYPICAL_H___ */\n");
}

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
/**
*
*
*
*/
int main(int argc, char *argv[]) {
	const char *yaml_filepath = "";
	const char *root_struct_name = "YamlNode_t";
	const char *generated_filepath = "tools";
	
	
	FILE *gen_h_fp, *gen_c_fp;
	char filename[MAX_FILEPATH_NAME];
	const static int max_filepath_len = MAX_FILEPATH_NAME -MAX_GEN_FILENAME_LEN ;
	int len;

	if (argc < 3) {
		printf( "usage:  this program needs 2 options\n" );
		printf( " [1] = schema yaml file \n" );
		printf( " [2] = output directory path(less than %d, without /)\n", max_filepath_len );
		return 1;
	}

	yaml_filepath = argv[1];
	generated_filepath = argv[2];

	len = strlen( generated_filepath );
	if( max_filepath_len <= len  ) {
		printf( "error:  output file path is too long(%d >= %d)\n", len, max_filepath_len );
	}

	sprintf( filename, "%s/generated_typical.h", generated_filepath );
	gen_h_fp = fopen( filename, "w" );
	if( gen_h_fp == NULL ) {
		printf( "error:  output file open error1(%s)\n", filename );
		return 11;
	}

	sprintf( filename, "%s/generated_typical.c", generated_filepath );
	gen_c_fp = fopen( filename, "w" );
	if( gen_c_fp == NULL ) {
		printf( "error:  output file open error2(%s)\n", filename );
		fclose( gen_h_fp);
		return 11;
	}

	// ----------------------------------------
	SchemaProperty_t *root_node;
	if (generate_schema_ast(yaml_filepath, &root_node)) {
		print_header_file(root_struct_name, root_node, gen_h_fp);
	}
	else {
		fprintf(stderr, "\n to generate header file is failed(%s)\n", yaml_filepath);
	}


	free_schema_properties(root_node);
	fclose( gen_h_fp);
	fclose( gen_c_fp);

	return 0;
}

