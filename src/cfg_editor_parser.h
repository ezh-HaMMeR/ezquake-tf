/*
 * Lossless Quake CFG document parser used by the experimental config editor.
 *
 * The parser never executes input and keeps every original byte in exactly one
 * node.  Concatenating node raw buffers therefore reproduces an unchanged file
 * byte-for-byte, including mixed line endings and non-UTF8 Quake characters.
 */

#ifndef EZQUAKE_CFG_EDITOR_PARSER_H
#define EZQUAKE_CFG_EDITOR_PARSER_H

#include <stddef.h>

typedef enum cfg_node_kind_e {
	CFG_NODE_BLANK,
	CFG_NODE_COMMENT,
	CFG_NODE_COMMAND,
	CFG_NODE_SET,
	CFG_NODE_SETINFO,
	CFG_NODE_BIND,
	CFG_NODE_ALIAS,
	CFG_NODE_EXEC,
	CFG_NODE_MALFORMED
} cfg_node_kind_t;

typedef struct cfg_node_s {
	unsigned int id;
	cfg_node_kind_t kind;
	size_t byte_start;
	size_t byte_end;
	unsigned int line_start;
	unsigned int line_end;
	unsigned char *raw;
	size_t raw_length;
	char *command;
	char *name;
	char *value;
	int managed;
	int modified;
} cfg_node_t;

typedef struct cfg_document_s {
	char *source_name;
	cfg_node_t *nodes;
	size_t node_count;
	size_t node_capacity;
} cfg_document_t;

int CFGDoc_Parse(cfg_document_t *document, const char *source_name, const unsigned char *data, size_t length);
void CFGDoc_Free(cfg_document_t *document);

int CFGDoc_Serialize(const cfg_document_t *document, unsigned char **data, size_t *length);
int CFGDoc_ReplaceNode(cfg_document_t *document, unsigned int node_id, const unsigned char *raw, size_t raw_length);
cfg_node_t *CFGDoc_FindNode(cfg_document_t *document, unsigned int node_id);
const cfg_node_t *CFGDoc_FindNodeConst(const cfg_document_t *document, unsigned int node_id);
void CFGDoc_SetManaged(cfg_document_t *document, unsigned int node_id, int managed);

#endif /* EZQUAKE_CFG_EDITOR_PARSER_H */
