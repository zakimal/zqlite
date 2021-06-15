#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

typedef struct
{
    char *buffer;
    size_t buffer_length;
    ssize_t input_length; // singed size_t
} InputBuffer;

typedef enum
{
    META_COMMAND_SUCCESS,
    META_COMMAND_UNRECOGNIZED_COMMAND,
} MetaCommandResult;

typedef enum
{
    PREPARE_SUCCESS,
    PREPARE_NEGATIVE_ID,
    PREPARE_STRING_TOO_LONG,
    PREPARE_SYNTAX_ERROR,
    PREPARE_UNRECOGNIZED_STATEMENT,
} PrepareResult;

typedef enum
{
    STATEMENT_INSERT,
    STATEMENT_SELECT,
} StatementType;

typedef enum
{
    EXECUTE_SUCCESS,
    EXECUTE_DUPLICATE_KEY,
    EXECUTE_TABLE_FULL,
} ExecuteResult;

#define COLUMN_USERNAME_SIZE 32
#define COLUMN_EMAIL_SIZE 255

typedef struct
{
    uint32_t id;
    char username[COLUMN_USERNAME_SIZE + 1];
    char email[COLUMN_EMAIL_SIZE + 1];
} Row;

typedef struct
{
    StatementType type;
    Row row_to_insert; // only used by insert statement
} Statement;

#define size_of_attribute(Struct, Attribute) sizeof(((Struct *)0)->Attribute)

#define ID_SIZE size_of_attribute(Row, id)
#define USERNAME_SIZE size_of_attribute(Row, username)
#define EMAIL_SIZE size_of_attribute(Row, email)
#define ID_OFFSET 0
#define USERNAME_OFFSET ID_OFFSET + ID_SIZE
#define EMAIL_OFFSET USERNAME_OFFSET + USERNAME_SIZE
#define ROW_SIZE (ID_SIZE + USERNAME_SIZE + EMAIL_SIZE)

#define PAGE_SIZE 4096
#define TABLE_MAX_PAGES 100

typedef struct
{
    int file_descriptor;
    uint32_t file_length;
    uint32_t num_pages;
    void *pages[TABLE_MAX_PAGES];
} Pager;

typedef struct
{
    uint32_t num_rows;
    Pager *pager;
    uint32_t root_page_num;
} Table;

typedef struct
{
    Table *table;
    uint32_t page_num;
    uint32_t cell_num;
    bool end_of_table; // is it a position one past the last row in the table?
} Cursor;

typedef enum
{
    NODE_INTERNAL,
    NODE_LEAF,
} NodeType;

/*
 * Common Node Header Layout
 */
#define NODE_TYPE_SIZE sizeof(uint8_t)
#define NODE_TYPE_OFFSET 0
#define IS_ROOT_SIZE sizeof(uint8_t)
#define IS_ROOT_OFFSET NODE_TYPE_SIZE
#define PARENT_POINTER_SIZE sizeof(uint32_t)
#define PARENT_POINTER_OFFSET (IS_ROOT_OFFSET + IS_ROOT_SIZE)
#define COMMON_NODE_HEADER_SIZE (NODE_TYPE_SIZE + IS_ROOT_SIZE + PARENT_POINTER_SIZE)

/*
 * Internal Node Header Layout
 */
#define INTERNAL_NODE_NUM_KEYS_SIZE sizeof(uint32_t)
#define INTERNAL_NODE_NUM_KEYS_OFFSET COMMON_NODE_HEADER_SIZE
#define INTERNAL_NODE_RIGHT_CHILD_SIZE sizeof(uint32_t)
#define INTERNAL_NODE_RIGHT_CHILD_OFFSET (INTERNAL_NODE_NUM_KEYS_OFFSET + INTERNAL_NODE_NUM_KEYS_SIZE)
#define INTERNAL_NODE_HEADER_SIZE (COMMON_NODE_HEADER_SIZE + INTERNAL_NODE_NUM_KEYS_SIZE + INTERNAL_NODE_RIGHT_CHILD_SIZE)

/*
 * Internal Node Body Layout
 */
#define INTERNAL_NODE_KEY_SIZE sizeof(uint32_t)
#define INTERNAL_NODE_CHILD_SIZE sizeof(uint32_t)
#define INTERNAL_NODE_CELL_SIZE (INTERNAL_NODE_CHILD_SIZE + INTERNAL_NODE_KEY_SIZE)

/*
 * Leaf Node Header Layout
 */
#define LEAF_NODE_NUM_CELLS_SIZE sizeof(uint32_t)
#define LEAF_NODE_NUM_CELLS_OFFSET COMMON_NODE_HEADER_SIZE
#define LEAF_NODE_HEADER_SIZE (COMMON_NODE_HEADER_SIZE + LEAF_NODE_NUM_CELLS_SIZE)

/*
 * Leaf Node Body Layout
 */
#define LEAF_NODE_KEY_SIZE sizeof(uint32_t)
#define LEAF_NODE_KEY_OFFSET 0
#define LEAF_NODE_VALUE_SIZE ROW_SIZE
#define LEAF_NODE_VALUE_OFFSET (LEAF_NODE_KEY_OFFSET + LEAF_NODE_KEY_SIZE)
#define LEAF_NODE_CELL_SIZE (LEAF_NODE_KEY_SIZE + LEAF_NODE_VALUE_SIZE)
#define LEAF_NODE_SPACE_FOR_CELLS (PAGE_SIZE - LEAF_NODE_HEADER_SIZE)
#define LEAF_NODE_MAX_CELLS (LEAF_NODE_SPACE_FOR_CELLS / LEAF_NODE_CELL_SIZE)
#define LEAF_NODE_RIGHT_SPLIT_COUNT ((LEAF_NODE_MAX_CELLS + 1) / 2)
#define LEAF_NODE_LEFT_SPLIT_COUNT ((LEAF_NODE_MAX_CELLS + 1) - LEAF_NODE_RIGHT_SPLIT_COUNT)

InputBuffer *new_input_buffer();
MetaCommandResult do_meta_command(InputBuffer *input_buffer, Table *table);
PrepareResult prepare_insert(InputBuffer *input_buffer, Statement *statement);
PrepareResult prepare_statement(InputBuffer *input_buffer, Statement *statement);
ExecuteResult execute_statement(Statement *statement, Table *table);
void print_prompt();
void read_input(InputBuffer *input_buffer);
void close_input_buffer(InputBuffer *input_buffer);
void print_row(Row *row);
void serialize_row(Row *source, void *destination);
void deserialize_row(void *source, Row *destination);
void *get_page(Pager *pager, uint32_t page_num);
uint32_t get_unused_page_num(Pager *pager);
void *cursor_value(Cursor *cursor);
void cursor_advance(Cursor *cursor);
Pager *pager_open(const char *filename);
void pager_flush(Pager *pager, uint32_t page_num);
Table *db_open(const char *filename);
void db_close(Table *table);
ExecuteResult execute_insert(Statement *statement, Table *table);
ExecuteResult execute_select(Statement *statement, Table *table);
Cursor *table_start(Table *table);
Cursor *table_find(Table *table, uint32_t key);
void create_new_root(Table *table, uint32_t right_child_page_num);
uint32_t *internal_node_num_keys(void *node);
uint32_t *internal_node_right_child(void *node);
uint32_t *internal_node_cell(void *node, uint32_t cell_num);
uint32_t *internal_node_child(void *node, uint32_t child_num);
uint32_t *internal_node_key(void *node, uint32_t key_num);
Cursor *internal_node_find(Table *table, uint32_t page_num, uint32_t key);
uint32_t get_node_max_key(void *node);
uint32_t *leaf_node_num_cells(void *node);
void *leaf_node_cell(void *node, uint32_t cell_num);
uint32_t *leaf_node_key(void *node, uint32_t cell_num);
void *leaf_node_value(void *node, uint32_t cell_num);
void initialize_leaf_node(void *node);
void initialize_internal_node(void *node);
void leaf_node_split_and_insert(Cursor *cursor, uint32_t key, Row *value);
void leaf_node_insert(Cursor *cursor, uint32_t key, Row *value);
Cursor *leaf_node_find(Table *table, uint32_t page_num, uint32_t key);
NodeType get_node_type(void *node);
bool is_node_root(void *node);
void set_node_root(void *node, bool is_root);
void set_node_type(void *node, NodeType type);
void print_constants();
void indent(uint32_t level);
void print_tree(Pager *pager, uint32_t page_num, uint32_t indentation_level);

InputBuffer *new_input_buffer()
{
    InputBuffer *input_buffer = malloc(sizeof(InputBuffer));
    input_buffer->buffer = NULL;
    input_buffer->buffer_length = 0;
    input_buffer->input_length = 0;

    return input_buffer;
}

MetaCommandResult do_meta_command(InputBuffer *input_buffer, Table *table)
{
    if (strcmp(input_buffer->buffer, ".exit") == 0)
    {
        close_input_buffer(input_buffer);
        db_close(table);
        exit(EXIT_SUCCESS);
    }
    else if (strcmp(input_buffer->buffer, ".constants") == 0)
    {
        printf("Constants:\n");
        print_constants();
        return META_COMMAND_SUCCESS;
    }
    else if (strcmp(input_buffer->buffer, ".btree") == 0)
    {
        printf("Tree:\n");
        print_tree(table->pager, 0, 0);
        return META_COMMAND_SUCCESS;
    }
    else
    {
        return META_COMMAND_UNRECOGNIZED_COMMAND;
    }
}

PrepareResult prepare_insert(InputBuffer *input_buffer, Statement *statement)
{
    statement->type = STATEMENT_INSERT;

    char *keyword = strtok(input_buffer->buffer, " ");
    char *id_string = strtok(NULL, " ");
    char *username = strtok(NULL, " ");
    char *email = strtok(NULL, " ");

    if (id_string == NULL || username == NULL || email == NULL)
    {
        return PREPARE_SYNTAX_ERROR;
    }

    int id = atoi(id_string);
    if (id < 0)
    {
        return PREPARE_NEGATIVE_ID;
    }
    if (COLUMN_USERNAME_SIZE < strlen(username))
    {
        return PREPARE_STRING_TOO_LONG;
    }
    if (COLUMN_EMAIL_SIZE < strlen(email))
    {
        return PREPARE_STRING_TOO_LONG;
    }

    statement->row_to_insert.id = id;
    strcpy(statement->row_to_insert.username, username);
    strcpy(statement->row_to_insert.email, email);

    return PREPARE_SUCCESS;
}

PrepareResult prepare_statement(InputBuffer *input_buffer, Statement *statement)
{
    if (strncmp(input_buffer->buffer, "insert", 6) == 0)
    {
        return prepare_insert(input_buffer, statement);
    }

    if (strcmp(input_buffer->buffer, "select") == 0)
    {
        statement->type = STATEMENT_SELECT;
        return PREPARE_SUCCESS;
    }

    return PREPARE_UNRECOGNIZED_STATEMENT;
}

ExecuteResult execute_insert(Statement *statement, Table *table)
{
    void *node = get_page(table->pager, table->root_page_num);
    uint32_t num_cells = (*leaf_node_num_cells(node));

    Row *row_to_insert = &(statement->row_to_insert);
    uint32_t key_to_insert = row_to_insert->id;
    Cursor *cursor = table_find(table, key_to_insert);

    if (cursor->cell_num < num_cells)
    {
        uint32_t key_at_index = *leaf_node_key(node, cursor->cell_num);
        if (key_at_index == key_to_insert)
        {
            return EXECUTE_DUPLICATE_KEY;
        }
    }

    leaf_node_insert(cursor, row_to_insert->id, row_to_insert);

    free(cursor);

    return EXECUTE_SUCCESS;
}

ExecuteResult execute_select(Statement *statement, Table *table)
{
    Cursor *cursor = table_start(table);

    Row row;
    while (!(cursor->end_of_table))
    {
        deserialize_row(cursor_value(cursor), &row);
        print_row(&row);
        cursor_advance(cursor);
    }

    free(cursor);

    return EXECUTE_SUCCESS;
}

ExecuteResult execute_statement(Statement *statement, Table *table)
{
    switch (statement->type)
    {
    case (STATEMENT_INSERT):
        return execute_insert(statement, table);

    case (STATEMENT_SELECT):
        return execute_select(statement, table);
    }
}

void print_prompt()
{
    printf("db> ");
}

void read_input(InputBuffer *input_buffer)
{
    ssize_t bytes_read = getline(&(input_buffer->buffer), &(input_buffer->buffer_length), stdin);

    if (bytes_read <= 0)
    {
        printf("Error: reading input\n");
        exit(EXIT_FAILURE);
    }

    // ignore tailing newline ('\n')
    input_buffer->input_length = bytes_read - 1;
    input_buffer->buffer[bytes_read - 1] = 0;
}

void close_input_buffer(InputBuffer *input_buffer)
{
    free(input_buffer->buffer);
    free(input_buffer);
}

void print_row(Row *row)
{
    printf("(%d, %s, %s)\n", row->id, row->username, row->email);
}

void serialize_row(Row *source, void *destination)
{
    memcpy(destination + ID_OFFSET, &(source->id), ID_SIZE);
    strncpy(destination + USERNAME_OFFSET, source->username, USERNAME_SIZE);
    strncpy(destination + EMAIL_OFFSET, source->email, EMAIL_SIZE);
}

void deserialize_row(void *source, Row *destination)
{
    memcpy(&(destination->id), source + ID_OFFSET, ID_SIZE);
    memcpy(&(destination->username), source + USERNAME_OFFSET, USERNAME_SIZE);
    memcpy(&(destination->email), source + EMAIL_OFFSET, EMAIL_SIZE);
}

Pager *pager_open(const char *filename)
{
    int fd = open(filename,
                  O_RDWR |      // Read/Write mode
                      O_CREAT,  // Create file if it does not exist
                  S_IWUSR |     // User write permission
                      S_IRUSR); // User read permission
    if (fd == -1)
    {
        printf("Unable to open file.\n");
        exit(EXIT_FAILURE);
    }

    off_t file_length = lseek(fd, 0, SEEK_END);

    Pager *pager = malloc(sizeof(Pager));
    pager->file_descriptor = fd;
    pager->file_length = file_length;
    pager->num_pages = file_length / PAGE_SIZE;

    if (file_length % PAGE_SIZE != 0)
    {
        printf("DB file is not a whole number of pages. Corrupt file.\n");
        exit(EXIT_FAILURE);
    }

    for (uint32_t i = 0; i < TABLE_MAX_PAGES; i++)
    {
        pager->pages[i] = NULL;
    }

    return pager;
}

void pager_flush(Pager *pager, uint32_t page_num)
{
    if (pager->pages[page_num] == NULL)
    {
        printf("Tried to flush null page.\n");
        exit(EXIT_FAILURE);
    }

    off_t offset = lseek(pager->file_descriptor, page_num * PAGE_SIZE, SEEK_SET);

    if (offset == -1)
    {
        printf("Error seeking: %d.\n", errno);
        exit(EXIT_FAILURE);
    }

    ssize_t bytes_written = write(pager->file_descriptor, pager->pages[page_num], PAGE_SIZE);
    if (bytes_written == -1)
    {
        printf("Error writing: %d.\n", errno);
        exit(EXIT_FAILURE);
    }
}

void *get_page(Pager *pager, uint32_t page_num)
{
    if (TABLE_MAX_PAGES < page_num)
    {
        printf("Tried to fetch page number out of bounds: %d (=TABLE_MAX_PAGES) < %d", TABLE_MAX_PAGES, page_num);
        exit(EXIT_FAILURE);
    }

    if (pager->pages[page_num] == NULL)
    {
        // Cache miss. Allocate memory and load from file.
        void *page = malloc(PAGE_SIZE);
        uint32_t num_pages = pager->file_length / PAGE_SIZE;

        if (pager->file_length % PAGE_SIZE)
        {
            num_pages += 1;
        }

        if (page_num <= num_pages)
        {
            lseek(pager->file_descriptor, page_num * PAGE_SIZE, SEEK_SET);
            ssize_t bytes_read = read(pager->file_descriptor, page, PAGE_SIZE);
            if (bytes_read == -1)
            {
                printf("Error reading file: %d\n", errno);
                exit(EXIT_FAILURE);
            }
        }

        pager->pages[page_num] = page;

        if (pager->num_pages <= page_num)
        {
            pager->num_pages = page_num + 1;
        }
    }

    return pager->pages[page_num];
}

uint32_t get_unused_page_num(Pager *pager)
{
    return pager->num_pages;
}

void *cursor_value(Cursor *cursor)
{
    uint32_t page_num = cursor->page_num;
    void *page = get_page(cursor->table->pager, page_num);
    return leaf_node_value(page, cursor->cell_num);
}

void cursor_advance(Cursor *cursor)
{
    uint32_t page_num = cursor->page_num;
    void *node = get_page(cursor->table->pager, page_num);

    cursor->cell_num += 1;
    if ((*leaf_node_num_cells(node)) <= cursor->cell_num)
    {
        cursor->end_of_table = true;
    }
}

Table *db_open(const char *filename)
{
    Pager *pager = pager_open(filename);

    Table *table = malloc(sizeof(Table));
    table->pager = pager;
    table->root_page_num = 0;

    if (pager->num_pages == 0)
    {
        // New database file. Initialize page 0 as leaf node.
        void *root_node = get_page(pager, 0);
        initialize_leaf_node(root_node);
        set_node_root(root_node, true);
    }

    return table;
}

void db_close(Table *table)
{
    Pager *pager = table->pager;

    for (uint32_t i = 0; i < pager->num_pages; i++)
    {
        if (pager->pages[i] == NULL)
        {
            continue;
        }
        pager_flush(pager, i);
        free(pager->pages[i]);
        pager->pages[i] = NULL;
    }

    int result = close(pager->file_descriptor);
    if (result == -1)
    {
        printf("Error closing db file.\n");
        exit(EXIT_FAILURE);
    }

    for (uint32_t i = 0; i < TABLE_MAX_PAGES; i++)
    {
        void *page = pager->pages[i];
        if (page)
        {
            free(page);
            pager->pages[i] = NULL;
        }
    }

    free(pager);
    free(table);
}

Cursor *table_start(Table *table)
{
    Cursor *cursor = malloc(sizeof(Cursor));
    cursor->table = table;
    cursor->page_num = table->root_page_num;
    cursor->cell_num = 0;

    void *root_node = get_page(table->pager, table->root_page_num);
    uint32_t num_cells = *leaf_node_num_cells(root_node);
    cursor->end_of_table = (num_cells == 0);

    return cursor;
}

Cursor *table_find(Table *table, uint32_t key)
{
    uint32_t root_page_num = table->root_page_num;
    void *root_node = get_page(table->pager, root_page_num);

    if (get_node_type(root_node) == NODE_LEAF)
    {
        return leaf_node_find(table, root_page_num, key);
    }
    else
    {
        return internal_node_find(cursor->table, cursor->table->root_page_num, key);
    }
}

void create_new_root(Table *table, uint32_t right_child_page_num)
{
    // Handle splitting the root.
    // Old root copied to new page, becomes left child.
    // Address of right child passed in.
    // Reinitialize root page to contain the new root node.
    // New root node splits to two children.

    void *root = get_page(table->pager, table->root_page_num);
    void *right_child = get_page(table->pager, right_child_page_num);
    uint32_t left_child_page_num = get_unused_page_num(table->pager);
    void *left_child = get_page(table->pager, left_child_page_num);

    // Left child has data copied from old root.
    memcpy(left_child, root, PAGE_SIZE);
    set_node_root(left_child, false);

    // Root node is a new internal node with one key and two children.
    initialize_internal_node(root);
    set_node_root(root, true);
    *internal_node_num_keys(root) = 1;
    *internal_node_child(root, 0) = left_child_page_num;
    uint32_t left_child_max_key = get_node_max_key(left_child);
    *internal_node_key(root, 0) = left_child_max_key;
    *internal_node_right_child(root) = right_child_page_num;
}

uint32_t *internal_node_num_keys(void *node)
{
    return node + INTERNAL_NODE_NUM_KEYS_OFFSET;
}

uint32_t *internal_node_right_child(void *node)
{
    return node + INTERNAL_NODE_RIGHT_CHILD_OFFSET;
}

uint32_t *internal_node_cell(void *node, uint32_t cell_num)
{
    return node + INTERNAL_NODE_HEADER_SIZE + cell_num * INTERNAL_NODE_CELL_SIZE;
}

uint32_t *internal_node_child(void *node, uint32_t child_num)
{
    uint32_t num_keys = *internal_node_num_keys(node);
    if (num_keys < child_num)
    {
        printf("Tried to access child_num %d > num_keys %d\n", child_num, num_keys);
        exit(EXIT_FAILURE);
    }
    else if (num_keys == child_num)
    {
        return internal_node_right_child(node);
    }
    else
    {
        return internal_node_cell(node, child_num);
    }
}

uint32_t *internal_node_key(void *node, uint32_t key_num)
{
    return internal_node_cell(node, key_num) + INTERNAL_NODE_CHILD_SIZE;
}

Cursor *internal_node_find(Table *table, uint32_t page_num, uint32_t key)
{
    void *node = get_page(table->pager, page_num);
    uint32_t num_keys = *internal_node_num_keys(node);

    uint32_t min_index = 0;
    uint32_t max_index = num_keys;

    while (min_index != max_index)
    {
        uint32_t index = (min_index + max_index) / 2;
        uint32_t key_to_right = *internal_node_key(node, index);
        if (key <= key_to_right)
        {
            max_index = index;
        }
        else
        {
            min_index = index + 1;
        }
    }

    uint32_t child_num = *internal_node_child(node, min_index);
    void *child = get_page(table->pager, child_num);
    switch (get_node_type(child))
    {
    case NODE_LEAF:
        return leaf_node_find(table, child_num, key);
    case NODE_INTERNAL:
        return internal_node_find(table, child_num, key);
    }
}

uint32_t get_node_max_key(void *node)
{
    switch (get_node_type(node))
    {
    case NODE_INTERNAL:
        return *internal_node_key(node, *internal_node_num_keys(node) - 1);
    case NODE_LEAF:
        return *leaf_node_key(node, *leaf_node_num_cells(node) - 1);
    }
}

uint32_t *leaf_node_num_cells(void *node)
{
    return node + LEAF_NODE_NUM_CELLS_OFFSET;
}

void *leaf_node_cell(void *node, uint32_t cell_num)
{
    return node + LEAF_NODE_HEADER_SIZE + cell_num * LEAF_NODE_CELL_SIZE;
}

uint32_t *leaf_node_key(void *node, uint32_t cell_num)
{
    return leaf_node_cell(node, cell_num);
}

void *leaf_node_value(void *node, uint32_t cell_num)
{
    return leaf_node_cell(node, cell_num) + LEAF_NODE_KEY_SIZE;
}

void initialize_leaf_node(void *node)
{
    set_node_type(node, NODE_LEAF);
    set_node_root(node, false);
    *leaf_node_num_cells(node) = 0;
}

void initialize_internal_node(void *node)
{
    set_node_type(node, NODE_INTERNAL);
    set_node_root(node, false);
    *internal_node_num_keys(node) = 0;
}

void leaf_node_split_and_insert(Cursor *cursor, uint32_t key, Row *value)
{
    // Create a new node and move half the cells over.
    // Insert the new value in one of the two nodes.
    // Update parent or create a new parent.

    void *old_node = get_page(cursor->table->pager, cursor->page_num);
    uint32_t new_page_num = get_unused_page_num(cursor->table->pager);
    void *new_node = get_page(cursor->table->pager, new_page_num);
    initialize_leaf_node(new_node);

    // All existing keys plus new key should be divided
    // evenly between old (left) and new (right) nodes.
    // Starting from the right, move each key to correct position.

    for (int32_t i = LEAF_NODE_MAX_CELLS; 0 <= i; i--)
    {
        void *destination_node;
        if (LEAF_NODE_LEFT_SPLIT_COUNT <= i)
        {
            destination_node = new_node;
        }
        else
        {
            destination_node = old_node;
        }

        uint32_t index_within_node = i % LEAF_NODE_LEFT_SPLIT_COUNT;
        void *destination = leaf_node_cell(destination_node, index_within_node);

        if (i == cursor->cell_num)
        {
            serialize_row(value, destination);
        }
        else if (cursor->cell_num < i)
        {
            memcpy(destination, leaf_node_cell(old_node, i - 1), LEAF_NODE_CELL_SIZE);
        }
        else
        {
            memcpy(destination, leaf_node_cell(old_node, i), LEAF_NODE_CELL_SIZE);
        }
    }

    // Update cell count on both leaf nodes.
    *(leaf_node_num_cells(old_node)) = LEAF_NODE_LEFT_SPLIT_COUNT;
    *(leaf_node_num_cells(new_node)) = LEAF_NODE_LEFT_SPLIT_COUNT;

    if (is_node_root(old_node))
    {
        return create_new_root(cursor->table, new_page_num);
    }
    else
    {
        printf("Need to implement updating parent after split\n");
        exit(EXIT_FAILURE);
    }
}

void leaf_node_insert(Cursor *cursor, uint32_t key, Row *value)
{
    void *node = get_page(cursor->table->pager, cursor->page_num);

    uint32_t num_cells = *leaf_node_num_cells(node);
    if (LEAF_NODE_MAX_CELLS <= num_cells)
    {
        leaf_node_split_and_insert(cursor, key, value);
        return;
    }

    if (cursor->cell_num < num_cells)
    {
        // Make room for the new cell
        for (uint32_t i = num_cells; cursor->cell_num < i; i--)
        {
            memcpy(leaf_node_cell(node, i), leaf_node_cell(node, i - 1), LEAF_NODE_CELL_SIZE);
        }
    }

    *(leaf_node_num_cells(node)) += 1;
    *(leaf_node_key(node, cursor->cell_num)) = key;
    serialize_row(value, leaf_node_value(node, cursor->cell_num));
}

Cursor *leaf_node_find(Table *table, uint32_t page_num, uint32_t key)
{
    void *node = get_page(table->pager, page_num);
    uint32_t num_cells = *leaf_node_num_cells(node);

    Cursor *cursor = malloc(sizeof(Cursor));
    cursor->table = table;
    cursor->page_num = page_num;

    // Binary search
    uint32_t min_index = 0;
    uint32_t one_past_max_index = num_cells;
    while (one_past_max_index != min_index)
    {
        uint32_t index = (min_index + one_past_max_index) / 2;
        uint32_t key_at_index = *leaf_node_key(node, index);
        if (key == key_at_index)
        {
            cursor->cell_num = index;
            return cursor;
        }
        if (key < key_at_index)
        {
            one_past_max_index = index;
        }
        else
        {
            min_index = index + 1;
        }
    }

    cursor->cell_num = min_index;
    return cursor;
}

NodeType get_node_type(void *node)
{
    uint8_t value = *((uint8_t *)(node + NODE_TYPE_OFFSET));
    return (NodeType)value;
}

bool is_node_root(void *node)
{
    uint8_t value = *((uint8_t *)(node + IS_ROOT_OFFSET));
    return (bool)value;
}

void set_node_root(void *node, bool is_root)
{
    uint8_t value = is_root;
    *((uint8_t *)(node + IS_ROOT_OFFSET)) = value;
}

void set_node_type(void *node, NodeType type)
{
    uint8_t value = type;
    *((uint8_t *)(node + NODE_TYPE_OFFSET)) = value;
}

void print_constants()
{
    printf("ROW_SIZE:                  %ld\n", ROW_SIZE);
    printf("COMMON_NODE_HEADER_SIZE:   %ld\n", COMMON_NODE_HEADER_SIZE);
    printf("LEAF_NODE_HEADER_SIZE:     %ld\n", LEAF_NODE_HEADER_SIZE);
    printf("LEAF_NODE_CELL_SIZE:       %ld\n", LEAF_NODE_CELL_SIZE);
    printf("LEAF_NODE_SPACE_FOR_CELLS: %ld\n", LEAF_NODE_SPACE_FOR_CELLS);
    printf("LEAF_NODE_MAX_CELLS:       %ld\n", LEAF_NODE_MAX_CELLS);
}

// for debug
void indent(uint32_t level)
{
    for (uint32_t i = 0; i < level; i++)
    {
        printf("    ");
    }
}

void print_tree(Pager *pager, uint32_t page_num, uint32_t indentation_level)
{
    void *node = get_page(pager, page_num);
    uint32_t num_keys, child;

    switch (get_node_type(node))
    {
    case NODE_LEAF:
        num_keys = *leaf_node_num_cells(node);
        indent(indentation_level);
        printf("- leaf (size %d)\n", num_keys);
        for (uint32_t i = 0; i < num_keys; i++)
        {
            indent(indentation_level + 1);
            printf("- %d\n", *leaf_node_key(node, i));
        }
        break;
    case NODE_INTERNAL:
        num_keys = *internal_node_num_keys(node);
        indent(indentation_level);
        printf("- internal (size %d)\n", num_keys);
        for (uint32_t i = 0; i < num_keys; i++)
        {
            child = *internal_node_child(node, i);
            print_tree(pager, child, indentation_level + 1);

            indent(indentation_level + 1);
            printf("- key %d\n", *internal_node_key(node, i));
        }
        child = *internal_node_right_child(node);
        print_tree(pager, child, indentation_level + 1);
        break;
    }
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        printf("Must supply a database filename.\n");
        exit(EXIT_FAILURE);
    }

    char *filename = argv[1];
    Table *table = db_open(filename);

    InputBuffer *input_buffer = new_input_buffer();
    while (true)
    {
        print_prompt();

        read_input(input_buffer);

        if (input_buffer->buffer[0] == '.')
        {
            switch (do_meta_command(input_buffer, table))
            {
            case (META_COMMAND_SUCCESS):
                continue;
            case (META_COMMAND_UNRECOGNIZED_COMMAND):
                printf("Unrecognized command '%s'.\n", input_buffer->buffer);
                continue;
            }
        }

        Statement statement;
        switch (prepare_statement(input_buffer, &statement))
        {
        case (PREPARE_SUCCESS):
            break;
        case (PREPARE_NEGATIVE_ID):
            printf("ID must be positive.\n");
            continue;
        case (PREPARE_STRING_TOO_LONG):
            printf("String is too long.\n");
            continue;
        case (PREPARE_SYNTAX_ERROR):
            printf("Syntax error. Could not parse statement.\n");
            continue;
        case (PREPARE_UNRECOGNIZED_STATEMENT):
            printf("Unrecognized keyword at start of '%s'.\n", input_buffer->buffer);
            continue;
        }

        switch (execute_statement(&statement, table))
        {
        case (EXECUTE_SUCCESS):
            printf("Executed.\n");
            break;
        case (EXECUTE_DUPLICATE_KEY):
            printf("Error: Duplicate key.\n");
            break;
        case (EXECUTE_TABLE_FULL):
            printf("Error: Table full.\n");
            break;
        }
    }
}