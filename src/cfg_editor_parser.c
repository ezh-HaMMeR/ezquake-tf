#include "cfg_editor_parser.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static char *CFGDoc_CopyString(const char *text, size_t length)
{
	char *copy = (char *)malloc(length + 1);
	if (!copy) {
		return NULL;
	}
	if (length) {
		memcpy(copy, text, length);
	}
	copy[length] = '\0';
	return copy;
}

static int CFGDoc_StrCaseEqual(const char *left, const char *right)
{
	while (*left && *right) {
		if (tolower((unsigned char)*left) != tolower((unsigned char)*right)) {
			return 0;
		}
		left++;
		right++;
	}
	return *left == *right;
}

static void CFGDoc_FreeNodeSemantic(cfg_node_t *node)
{
	free(node->command);
	free(node->name);
	free(node->value);
	node->command = NULL;
	node->name = NULL;
	node->value = NULL;
}

static size_t CFGDoc_InlineCommentStart(const unsigned char *raw, size_t length)
{
	size_t i;
	int quoted = 0;
	int escaped = 0;

	for (i = 0; i + 1 < length; ++i) {
		unsigned char ch = raw[i];
		if (quoted && ch == '\\' && !escaped) {
			escaped = 1;
			continue;
		}
		if (ch == '"' && !escaped) {
			quoted = !quoted;
		}
		escaped = 0;
		if (!quoted && ch == '/' && raw[i + 1] == '/') {
			return i;
		}
	}
	return length;
}

static size_t CFGDoc_NextToken(const unsigned char *raw, size_t length, size_t *cursor, char **token)
{
	size_t start;
	size_t output_length = 0;
	char *output;
	int quoted = 0;

	*token = NULL;
	while (*cursor < length && isspace((unsigned char)raw[*cursor])) {
		(*cursor)++;
	}
	if (*cursor >= length) {
		return 0;
	}

	start = *cursor;
	if (raw[*cursor] == '"') {
		quoted = 1;
		(*cursor)++;
		start = *cursor;
		while (*cursor < length) {
			if (raw[*cursor] == '\\' && *cursor + 1 < length &&
				(raw[*cursor + 1] == '"' || raw[*cursor + 1] == '\\')) {
				output_length++;
				*cursor += 2;
				continue;
			}
			if (raw[*cursor] == '"') {
				break;
			}
			output_length++;
			(*cursor)++;
		}
	}
	else {
		while (*cursor < length && !isspace((unsigned char)raw[*cursor])) {
			(*cursor)++;
		}
		output_length = *cursor - start;
	}

	output = (char *)malloc(output_length + 1);
	if (!output) {
		return 0;
	}

	if (quoted) {
		size_t input = start;
		size_t out = 0;
		while (input < *cursor) {
			if (raw[input] == '\\' && input + 1 < *cursor &&
				(raw[input + 1] == '"' || raw[input + 1] == '\\')) {
				input++;
			}
			output[out++] = (char)raw[input++];
		}
		output[out] = '\0';
		if (*cursor < length && raw[*cursor] == '"') {
			(*cursor)++;
		}
	}
	else {
		memcpy(output, raw + start, output_length);
		output[output_length] = '\0';
	}

	*token = output;
	return output_length + 1;
}

static char *CFGDoc_CopyValue(const unsigned char *raw, size_t length, size_t cursor)
{
	size_t end = length;
	char *value;

	while (cursor < end && isspace((unsigned char)raw[cursor])) {
		cursor++;
	}
	while (end > cursor && isspace((unsigned char)raw[end - 1])) {
		end--;
	}

	if (end > cursor + 1 && raw[cursor] == '"' && raw[end - 1] == '"') {
		size_t token_cursor = cursor;
		if (CFGDoc_NextToken(raw, end, &token_cursor, &value)) {
			while (token_cursor < end && isspace((unsigned char)raw[token_cursor])) {
				token_cursor++;
			}
			if (token_cursor == end) {
				return value;
			}
			free(value);
		}
	}

	return CFGDoc_CopyString((const char *)raw + cursor, end - cursor);
}

static int CFGDoc_ParseNodeSemantic(cfg_node_t *node)
{
	size_t start = 0;
	size_t end = node->raw_length;
	size_t cursor;
	char *first = NULL;
	char *second = NULL;

	CFGDoc_FreeNodeSemantic(node);
	while (start < end && isspace((unsigned char)node->raw[start])) {
		start++;
	}
	if (start == end) {
		node->kind = CFG_NODE_BLANK;
		return 1;
	}
	if (start + 1 < end && node->raw[start] == '/' && node->raw[start + 1] == '/') {
		node->kind = CFG_NODE_COMMENT;
		return 1;
	}

	end = CFGDoc_InlineCommentStart(node->raw, end);
	while (end > start && isspace((unsigned char)node->raw[end - 1])) {
		end--;
	}
	if (end > start && node->raw[end - 1] == ';') {
		end--;
		while (end > start && isspace((unsigned char)node->raw[end - 1])) {
			end--;
		}
	}
	if (end == start) {
		node->kind = CFG_NODE_COMMENT;
		return 1;
	}

	cursor = start;
	if (!CFGDoc_NextToken(node->raw, end, &cursor, &first)) {
		node->kind = CFG_NODE_MALFORMED;
		return 1;
	}
	node->command = first;

	if (CFGDoc_StrCaseEqual(first, "set") || CFGDoc_StrCaseEqual(first, "seta")) {
		node->kind = CFG_NODE_SET;
	}
	else if (CFGDoc_StrCaseEqual(first, "setinfo")) {
		node->kind = CFG_NODE_SETINFO;
	}
	else if (CFGDoc_StrCaseEqual(first, "bind")) {
		node->kind = CFG_NODE_BIND;
	}
	else if (CFGDoc_StrCaseEqual(first, "alias")) {
		node->kind = CFG_NODE_ALIAS;
	}
	else if (CFGDoc_StrCaseEqual(first, "exec")) {
		node->kind = CFG_NODE_EXEC;
	}
	else {
		node->kind = CFG_NODE_COMMAND;
		node->name = CFGDoc_CopyString(first, strlen(first));
		node->value = CFGDoc_CopyValue(node->raw, end, cursor);
		return node->name && node->value;
	}

	if (!CFGDoc_NextToken(node->raw, end, &cursor, &second)) {
		node->kind = CFG_NODE_MALFORMED;
		return 1;
	}
	node->name = second;
	node->value = CFGDoc_CopyValue(node->raw, end, cursor);
	return node->value != NULL;
}

static unsigned int CFGDoc_CountLineBreaks(const unsigned char *raw, size_t length)
{
	size_t i;
	unsigned int count = 0;
	for (i = 0; i < length; ++i) {
		if (raw[i] == '\n') {
			count++;
		}
		else if (raw[i] == '\r' && (i + 1 == length || raw[i + 1] != '\n')) {
			count++;
		}
	}
	return count;
}

static int CFGDoc_RawEndsInLineBreak(const unsigned char *raw, size_t length)
{
	return length && (raw[length - 1] == '\n' || raw[length - 1] == '\r');
}

static void CFGDoc_Reindex(cfg_document_t *document)
{
	size_t i;
	size_t byte_offset = 0;
	unsigned int line = 1;

	for (i = 0; i < document->node_count; ++i) {
		cfg_node_t *node = &document->nodes[i];
		unsigned int breaks = CFGDoc_CountLineBreaks(node->raw, node->raw_length);
		node->byte_start = byte_offset;
		node->byte_end = byte_offset + node->raw_length;
		node->line_start = line;
		node->line_end = line + breaks;
		if (breaks && CFGDoc_RawEndsInLineBreak(node->raw, node->raw_length)) {
			node->line_end--;
		}
		byte_offset = node->byte_end;
		line += breaks;
	}
}

static int CFGDoc_AppendNode(cfg_document_t *document, const unsigned char *raw, size_t raw_length)
{
	cfg_node_t *node;
	if (document->node_count == document->node_capacity) {
		size_t new_capacity = document->node_capacity ? document->node_capacity * 2 : 32;
		cfg_node_t *new_nodes = (cfg_node_t *)realloc(document->nodes, new_capacity * sizeof(*new_nodes));
		if (!new_nodes) {
			return 0;
		}
		document->nodes = new_nodes;
		document->node_capacity = new_capacity;
	}

	node = &document->nodes[document->node_count];
	memset(node, 0, sizeof(*node));
	node->id = (unsigned int)document->node_count + 1;
	node->raw = (unsigned char *)malloc(raw_length ? raw_length : 1);
	if (!node->raw) {
		return 0;
	}
	if (raw_length) {
		memcpy(node->raw, raw, raw_length);
	}
	node->raw_length = raw_length;
	if (!CFGDoc_ParseNodeSemantic(node)) {
		free(node->raw);
		memset(node, 0, sizeof(*node));
		return 0;
	}
	document->node_count++;
	return 1;
}

int CFGDoc_Parse(cfg_document_t *document, const char *source_name, const unsigned char *data, size_t length)
{
	size_t start = 0;
	size_t i;
	int quoted = 0;
	int comment = 0;
	int escaped = 0;

	if (!document || (!data && length)) {
		return 0;
	}
	memset(document, 0, sizeof(*document));
	document->source_name = CFGDoc_CopyString(source_name ? source_name : "", source_name ? strlen(source_name) : 0);
	if (!document->source_name) {
		return 0;
	}

	for (i = 0; i < length; ++i) {
		unsigned char ch = data[i];
		if (comment) {
			if (ch == '\r' || ch == '\n') {
				if (ch == '\r' && i + 1 < length && data[i + 1] == '\n') {
					i++;
				}
				if (!CFGDoc_AppendNode(document, data + start, i + 1 - start)) {
					CFGDoc_Free(document);
					return 0;
				}
				start = i + 1;
				comment = 0;
			}
			continue;
		}

		if (quoted && ch == '\\' && !escaped) {
			escaped = 1;
			continue;
		}
		if (ch == '"' && !escaped) {
			quoted = !quoted;
		}
		escaped = 0;

		if (!quoted && ch == '/' && i + 1 < length && data[i + 1] == '/') {
			comment = 1;
			i++;
			continue;
		}
		if (!quoted && ch == ';') {
			if (!CFGDoc_AppendNode(document, data + start, i + 1 - start)) {
				CFGDoc_Free(document);
				return 0;
			}
			start = i + 1;
			continue;
		}
		if (!quoted && (ch == '\r' || ch == '\n')) {
			if (ch == '\r' && i + 1 < length && data[i + 1] == '\n') {
				i++;
			}
			if (!CFGDoc_AppendNode(document, data + start, i + 1 - start)) {
				CFGDoc_Free(document);
				return 0;
			}
			start = i + 1;
		}
	}

	if (start < length && !CFGDoc_AppendNode(document, data + start, length - start)) {
		CFGDoc_Free(document);
		return 0;
	}

	CFGDoc_Reindex(document);
	return 1;
}

void CFGDoc_Free(cfg_document_t *document)
{
	size_t i;
	if (!document) {
		return;
	}
	for (i = 0; i < document->node_count; ++i) {
		CFGDoc_FreeNodeSemantic(&document->nodes[i]);
		free(document->nodes[i].raw);
	}
	free(document->nodes);
	free(document->source_name);
	memset(document, 0, sizeof(*document));
}

int CFGDoc_Serialize(const cfg_document_t *document, unsigned char **data, size_t *length)
{
	size_t total = 0;
	size_t offset = 0;
	size_t i;
	unsigned char *output;

	if (!document || !data || !length) {
		return 0;
	}
	for (i = 0; i < document->node_count; ++i) {
		if (document->nodes[i].raw_length > (size_t)-1 - total) {
			return 0;
		}
		total += document->nodes[i].raw_length;
	}
	if (total == (size_t)-1) {
		return 0;
	}
	output = (unsigned char *)malloc(total + 1);
	if (!output) {
		return 0;
	}
	for (i = 0; i < document->node_count; ++i) {
		if (document->nodes[i].raw_length) {
			memcpy(output + offset, document->nodes[i].raw, document->nodes[i].raw_length);
			offset += document->nodes[i].raw_length;
		}
	}
	output[total] = '\0';
	*data = output;
	*length = total;
	return 1;
}

cfg_node_t *CFGDoc_FindNode(cfg_document_t *document, unsigned int node_id)
{
	size_t i;
	if (!document) {
		return NULL;
	}
	for (i = 0; i < document->node_count; ++i) {
		if (document->nodes[i].id == node_id) {
			return &document->nodes[i];
		}
	}
	return NULL;
}

const cfg_node_t *CFGDoc_FindNodeConst(const cfg_document_t *document, unsigned int node_id)
{
	return CFGDoc_FindNode((cfg_document_t *)document, node_id);
}

int CFGDoc_ReplaceNode(cfg_document_t *document, unsigned int node_id, const unsigned char *raw, size_t raw_length)
{
	cfg_node_t *node = CFGDoc_FindNode(document, node_id);
	unsigned char *copy;
	int managed;

	if (!node || (!raw && raw_length)) {
		return 0;
	}
	copy = (unsigned char *)malloc(raw_length ? raw_length : 1);
	if (!copy) {
		return 0;
	}
	if (raw_length) {
		memcpy(copy, raw, raw_length);
	}
	managed = node->managed;
	CFGDoc_FreeNodeSemantic(node);
	free(node->raw);
	node->raw = copy;
	node->raw_length = raw_length;
	node->managed = managed;
	node->modified = 1;
	if (!CFGDoc_ParseNodeSemantic(node)) {
		return 0;
	}
	CFGDoc_Reindex(document);
	return 1;
}

void CFGDoc_SetManaged(cfg_document_t *document, unsigned int node_id, int managed)
{
	cfg_node_t *node = CFGDoc_FindNode(document, node_id);
	if (node) {
		node->managed = managed != 0;
	}
}
