#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

// Constants
#define MAX_SYMBOL_VALUE 285
#define MAX_CODE_BITS_LENGTH 32
#define BLOCK_SIZE 0x4000 // Define block size constant

// Structure for Huffman Tree
typedef struct
{
    uint16_t symbol_values[MAX_SYMBOL_VALUE];       // Symbol values
    uint32_t compressed_codes[MAX_SYMBOL_VALUE];    // Compressed codes
    uint8_t bits_length[MAX_SYMBOL_VALUE];          // Lengths of codes
    uint16_t symbol_value_offset[MAX_SYMBOL_VALUE]; // Offsets in the symbol value table
} HuffmanTree;

// State structure for managing decompression
typedef struct
{
    const uint32_t *input_dat; // Input data (compressed)
    uint32_t input_size;       // Size of the input data
    uint32_t input_position;   // Current position in the input
    uint32_t head;             // Head for reading bits
    uint32_t bits;             // Bits read from input
    uint32_t buffer;           // Buffer for storing bits
    bool empty;                // Flag to check if input is empty
} State;

// Global variable to track if the Huffman tree is initialized
int huffman_tree_dict_initialized = 0;

// Global variable to hold the Huffman tree
HuffmanTree huffman_tree_dict; // Assume this is a defined structure for your Huffman tree

void pull_byte(State *state_data)
{
    if (state_data->bits >= 32)
    {
        fprintf(stderr, "Tried to pull a value while we still have 32 bits available.\n");
        return; // Changed to return instead of exit
    }

    if ((state_data->input_position + 1) % BLOCK_SIZE == 0)
    {
        ++(state_data->input_position);
    }

    if (state_data->input_position >= state_data->input_size)
    {
        fprintf(stderr, "Reached end of input while trying to fetch a new byte.\n");
        return; // Changed to return instead of exit
    }

    uint32_t temp_value = state_data->input_dat[state_data->input_position];

    if (state_data->bits == 0)
    {
        state_data->head = temp_value;
        state_data->buffer = 0;
    }
    else
    {
        state_data->head = state_data->head | (temp_value >> (state_data->bits));
        state_data->buffer = (temp_value << (32 - state_data->bits));
    }

    state_data->bits += 32;
    ++(state_data->input_position);
}

// Function to ensure we have enough bits
void need_bits(State *state_data, const uint8_t bits)
{
    // Checking that we request at most 32 bits
    if (bits > 32)
    {
        fprintf(stderr, "Tried to need more than 32 bits.\n");
        exit(EXIT_FAILURE);
    }

    if (state_data->bits < bits)
    {
        pull_byte(state_data);
    }
}

// Function to drop bits
void drop_bits(State *state_data, const uint8_t bits)
{
    // Checking that we request at most 32 bits
    if (bits > 32)
    {
        fprintf(stderr, "Tried to drop more than 32 bits.\n");
        exit(EXIT_FAILURE);
    }

    if (bits > state_data->bits)
    {
        fprintf(stderr, "Tried to drop more bits than we have.\n");
        exit(EXIT_FAILURE);
    }

    // Updating the values to drop the bits
    if (bits == 32)
    {
        state_data->head = state_data->buffer;
        state_data->buffer = 0;
    }
    else
    {
        state_data->head = (state_data->head << bits) | (state_data->buffer >> (32 - bits));
        state_data->buffer = state_data->buffer << bits;
    }

    // Update state info
    state_data->bits -= bits;
}

// Function to read bits
uint32_t readBits(const State *iState, const uint8_t bits)
{
    return (iState->head >> (32 - bits));
}

// Function to read a code from the Huffman tree
void read_code(HuffmanTree *huffman_tree, State *state_data, uint16_t *ioCode)
{
    if (huffman_tree->compressed_codes[0] == 0)
    {
        fprintf(stderr, "\nTrying to read code from an empty HuffmanTree. : \n");
        exit(EXIT_FAILURE);
    }

    need_bits(state_data, 32);
    uint16_t temp_index = 0;
    uint32_t bitsRead = readBits(state_data, 32);

    while (bitsRead < huffman_tree->compressed_codes[temp_index])
    {
        ++temp_index;
    }

    uint8_t temp_bits = huffman_tree->bits_length[temp_index];

    *ioCode = huffman_tree->symbol_values[huffman_tree->symbol_value_offset[temp_index] -
                                          ((bitsRead - huffman_tree->compressed_codes[temp_index]) >> (32 - temp_bits))];

    drop_bits(state_data, temp_bits);
}

// Function prototype for building the Huffman tree
void create_huffman_tree(HuffmanTree *ioHuffmanTree, int16_t *ioWorkingBitTab, int16_t *ioWorkingCodeTab)
{
    // Building the HuffmanTree
    uint32_t temp_code = 0;
    uint8_t temp_bits = 0;
    uint16_t comparison_code_index = 0;
    uint16_t symbol_offset = 0;

    while (temp_bits < MAX_CODE_BITS_LENGTH)
    {
        if (ioWorkingBitTab[temp_bits] != -1)
        {
            int16_t temp_symbol = ioWorkingBitTab[temp_bits];
            while (temp_symbol != -1)
            {
                // Registering the code
                ioHuffmanTree->symbol_values[symbol_offset] = temp_symbol;
                ++symbol_offset;
                temp_symbol = ioWorkingCodeTab[temp_symbol];
                --temp_code; // Decrement code value for next symbol
            }

            // Minimum code value for temp_bits bits
            ioHuffmanTree->compressed_codes[comparison_code_index] = ((temp_code + 1) << (32 - temp_bits));

            // Number of bits for l_codeCompIndex index
            ioHuffmanTree->bits_length[comparison_code_index] = temp_bits;

            // Offset in symbol_values table to reach the value
            ioHuffmanTree->symbol_value_offset[comparison_code_index] = symbol_offset - 1;

            ++comparison_code_index;
        }
        temp_code = (temp_code << 1) + 1; // Increment code for next length
        ++temp_bits;
    }
}

void fillTabsHelper(const uint8_t bits, const int16_t symbol, int16_t *ioWorkingBitTab, int16_t *ioWorkingCodeTab)
{
    // Check out of bounds
    if (bits >= MAX_CODE_BITS_LENGTH)
    {
        fprintf(stderr, "Error: Too many bits.\n");
        exit(EXIT_FAILURE); // Exit or handle the error appropriately
    }

    if (symbol >= MAX_SYMBOL_VALUE)
    {
        fprintf(stderr, "Error: Too high symbol.\n");
        exit(EXIT_FAILURE); // Exit or handle the error appropriately
    }

    if (ioWorkingBitTab[bits] == -1)
    {
        ioWorkingBitTab[bits] = symbol;
    }
    else
    {
        ioWorkingCodeTab[symbol] = ioWorkingBitTab[bits];
        ioWorkingBitTab[bits] = symbol;
    }
}

void initialize_huffman_tree_dict()
{
    int16_t working_bits[MAX_CODE_BITS_LENGTH];
    int16_t working_code[MAX_SYMBOL_VALUE];

    // Initialize working tables
    memset(working_bits, -1, MAX_CODE_BITS_LENGTH * sizeof(int16_t)); // Use -1 to indicate uninitialized
    memset(working_code, -1, MAX_SYMBOL_VALUE * sizeof(int16_t));     // Use -1 to indicate uninitialized

    // clang-format off
    // Define your bits and symbols arrays
    uint8_t bits[] = {
        3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 6, 6, 6, 6, 6, 6, 6, 6, 7, 7, 7, 7, 7, 7, 7, 8, 8, 8, 8,
        8, 8, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
        10, 10, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 12, 12, 12, 12, 12, 12, 12, 13,
        13, 13, 13, 13, 13, 14, 14, 14, 14, 15, 15, 15, 15, 15, 15, 15, 15, 16, 16, 16, 16, 16, 16,
        16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
        16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
        16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
        16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
        16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
        16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
        16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
    }; // Example values, adjust as needed
    int16_t symbols[] = {
        0x0A, 0x09, 0x08, 0x0C, 0x0B, 0x07, 0x00, 0xE0, 0x2A, 0x29, 0x06, 0x4A, 0x40, 0x2C, 0x2B,
        0x28, 0x20, 0x05, 0x04, 0x49, 0x48, 0x27, 0x26, 0x25, 0x0D, 0x03, 0x6A, 0x69, 0x4C, 0x4B,
        0x47, 0x24, 0xE8, 0xA0, 0x89, 0x88, 0x68, 0x67, 0x63, 0x60, 0x46, 0x23, 0xE9, 0xC9, 0xC0,
        0xA9, 0xA8, 0x8A, 0x87, 0x80, 0x66, 0x65, 0x45, 0x44, 0x43, 0x2D, 0x02, 0x01, 0xE5, 0xC8,
        0xAA, 0xA5, 0xA4, 0x8B, 0x85, 0x84, 0x6C, 0x6B, 0x64, 0x4D, 0x0E, 0xE7, 0xCA, 0xC7, 0xA7,
        0xA6, 0x86, 0x83, 0xE6, 0xE4, 0xC4, 0x8C, 0x2E, 0x22, 0xEC, 0xC6, 0x6D, 0x4E, 0xEA, 0xCC,
        0xAC, 0xAB, 0x8D, 0x11, 0x10, 0x0F, 0xFF, 0xFE, 0xFD, 0xFC, 0xFB, 0xFA, 0xF9, 0xF8, 0xF7,
        0xF6, 0xF5, 0xF4, 0xF3, 0xF2, 0xF1, 0xF0, 0xEF, 0xEE, 0xED, 0xEB, 0xE3, 0xE2, 0xE1, 0xDF,
        0xDE, 0xDD, 0xDC, 0xDB, 0xDA, 0xD9, 0xD8, 0xD7, 0xD6, 0xD5, 0xD4, 0xD3, 0xD2, 0xD1, 0xD0,
        0xCF, 0xCE, 0xCD, 0xCB, 0xC5, 0xC3, 0xC2, 0xC1, 0xBF, 0xBE, 0xBD, 0xBC, 0xBB, 0xBA, 0xB9,
        0xB8, 0xB7, 0xB6, 0xB5, 0xB4, 0xB3, 0xB2, 0xB1, 0xB0, 0xAF, 0xAE, 0xAD, 0xA3, 0xA2, 0xA1,
        0x9F, 0x9E, 0x9D, 0x9C, 0x9B, 0x9A, 0x99, 0x98, 0x97, 0x96, 0x95, 0x94, 0x93, 0x92, 0x91,
        0x90, 0x8F, 0x8E, 0x82, 0x81, 0x7F, 0x7E, 0x7D, 0x7C, 0x7B, 0x7A, 0x79, 0x78, 0x77, 0x76,
        0x75, 0x74, 0x73, 0x72, 0x71, 0x70, 0x6F, 0x6E, 0x62, 0x61, 0x5F, 0x5E, 0x5D, 0x5C, 0x5B,
        0x5A, 0x59, 0x58, 0x57, 0x56, 0x55, 0x54, 0x53, 0x52, 0x51, 0x50, 0x4F, 0x42, 0x41, 0x3F,
        0x3E, 0x3D, 0x3C, 0x3B, 0x3A, 0x39, 0x38, 0x37, 0x36, 0x35, 0x34, 0x33, 0x32, 0x31, 0x30,
        0x2F, 0x21, 0x1F, 0x1E, 0x1D, 0x1C, 0x1B, 0x1A, 0x19, 0x18, 0x17, 0x16, 0x15, 0x14, 0x13,
        0x12,
    }; // Example values, adjust as needed

    // clang-format off

    size_t temp_symbol = sizeof(bits) / sizeof(bits[0]); // Calculate number of symbols

    // Populate the working tables
    for (size_t i = 0; i < temp_symbol; i++)
    {
        fillTabsHelper(bits[i], symbols[i], working_bits, working_code);
    }

    // Build the Huffman tree
    create_huffman_tree(&huffman_tree_dict, working_bits, working_code);
}

// Function to parse the Huffman tree
void parse_huffman_tree(State *state_data, HuffmanTree *ioHuffmanTree)
{
    // Reading the number of symbols to read
    need_bits(state_data, 16);
    uint16_t number_symbol = (uint16_t)readBits(state_data, 16); // C-style cast
    drop_bits(state_data, 16);

    if (number_symbol > MAX_SYMBOL_VALUE)
    {
        fprintf(stderr, "Too many symbols to decode.\n");
        exit(EXIT_FAILURE);
    }

    int16_t working_bits[MAX_CODE_BITS_LENGTH];
    int16_t working_code[MAX_SYMBOL_VALUE];

    // Initialize our workingTabs
    memset(working_bits, 0xFF, MAX_CODE_BITS_LENGTH * sizeof(int16_t));
    memset(working_code, 0xFF, MAX_SYMBOL_VALUE * sizeof(int16_t));

    int16_t remaining_symbol = number_symbol - 1;

    // Fetching the code repartition
    while (remaining_symbol >= 0)
    {
        uint16_t temp_code = 0;
        read_code(&huffman_tree_dict, state_data, &temp_code);

        uint16_t code_number_bits = temp_code & 0x1F;
        uint16_t code_number_symbol = (temp_code >> 5) + 1;

        if (code_number_bits == 0)
        {
            remaining_symbol -= code_number_symbol;
        }
        else
        {
            while (code_number_symbol > 0)
            {
                if (working_bits[code_number_bits] == -1)
                {
                    working_bits[code_number_bits] = remaining_symbol;
                }
                else
                {
                    working_code[remaining_symbol] = working_bits[code_number_bits];
                    working_bits[code_number_bits] = remaining_symbol;
                }
                --remaining_symbol;
                --code_number_symbol;
            }
        }
    }

    // Effectively build the Huffman tree
    create_huffman_tree(ioHuffmanTree, working_bits, working_code);
}

// Function to inflate data
void inflate_data(State *state_data, uint8_t *output_buffer, const uint32_t output_buffer_size)
{
    uint32_t temp_output_position = 0;

    // Reading the const write size addition value
    need_bits(state_data, 8);
    drop_bits(state_data, 4);
    uint16_t write_size_constant_addition = readBits(state_data, 4) + 1;
    drop_bits(state_data, 4);

    // Declaring our HuffmanTrees
    HuffmanTree huffman_tree_symbol;
    HuffmanTree huffman_tree_copy;

    while (temp_output_position < output_buffer_size)
    {
        // Resetting Huffman trees
        memset(&huffman_tree_symbol, 0, sizeof(HuffmanTree));
        memset(&huffman_tree_copy, 0, sizeof(HuffmanTree));

        // Reading HuffmanTrees
        parse_huffman_tree(state_data, &huffman_tree_symbol);
        parse_huffman_tree(state_data, &huffman_tree_copy);

        // Reading MaxCount
        need_bits(state_data, 4);
        uint32_t max_count = (readBits(state_data, 4) + 1) << 12;
        drop_bits(state_data, 4);

        uint32_t current_code_read_count = 0;

        while ((current_code_read_count < max_count) && (temp_output_position < output_buffer_size))
        {
            ++current_code_read_count;

            // Reading next code
            uint16_t temp_code = 0;
            read_code(&huffman_tree_symbol, state_data, &temp_code);

            if (temp_code < 0x100)
            {
                output_buffer[temp_output_position] = (uint8_t)temp_code; // C-style cast
                ++temp_output_position;
                continue;
            }

            // We are in copy mode!
            // Reading the additional info to know the write size
            temp_code -= 0x100;

            // Write size
            div_t code_division_4 = div(temp_code, 4);

            uint32_t write_size = 0;
            if (code_division_4.quot == 0)
            {
                write_size = temp_code;
            }
            else if (code_division_4.quot < 7)
            {
                write_size = ((1 << (code_division_4.quot - 1)) * (4 + code_division_4.rem));
            }
            else if (temp_code == 28)
            {
                write_size = 0xFF;
            }
            else
            {
                fprintf(stderr, "Invalid value for writeSize code.\n");
                exit(EXIT_FAILURE);
            }

            // Additional bits
            if (code_division_4.quot > 1 && temp_code != 28)
            {
                uint8_t write_size_addition = code_division_4.quot - 1;
                need_bits(state_data, write_size_addition);
                write_size |= readBits(state_data, write_size_addition);
                drop_bits(state_data, write_size_addition);
            }
            write_size += write_size_constant_addition;

            // Write offset
            // Reading the write offset
            read_code(&huffman_tree_copy, state_data, &temp_code);

            div_t code_division_2 = div(temp_code, 2);

            uint32_t write_offset = 0;
            if (code_division_2.quot == 0)
            {
                write_offset = temp_code;
            }
            else if (code_division_2.quot < 17)
            {
                write_offset = ((1 << (code_division_2.quot - 1)) * (2 + code_division_2.rem));
            }
            else
            {
                fprintf(stderr, "Invalid value for writeOffset code.\n");
                exit(EXIT_FAILURE);
            }

            // Additional bits
            if (code_division_2.quot > 1)
            {
                uint8_t write_offset_addition_bits = code_division_2.quot - 1;
                need_bits(state_data, write_offset_addition_bits);
                write_offset |= readBits(state_data, write_offset_addition_bits);
                drop_bits(state_data, write_offset_addition_bits);
            }
            write_offset += 1;

            uint32_t already_written = 0;
            while ((already_written < write_size) && (temp_output_position < output_buffer_size))
            {
                output_buffer[temp_output_position] = output_buffer[temp_output_position - write_offset];
                ++temp_output_position;
                ++already_written;
            }
        }
    }
}

// Function to convert uint8_t buffer to uint32_t buffer
uint32_t* convert_u8_to_u32(const uint8_t* input, size_t input_size, size_t* output_size) {
    if (input_size % 4 != 0) {
        return NULL; // Return NULL if input size is not a multiple of 4
    }

    *output_size = input_size / 4;
    uint32_t* output = (uint32_t*)malloc(*output_size * sizeof(uint32_t));
    if (output == NULL) {
        return NULL; // Return NULL if memory allocation fails
    }

    for (size_t i = 0; i < *output_size; i++) {
        output[i] = (uint32_t)input[i * 4] |
                     (uint32_t)input[i * 4 + 1] << 8 |
                     (uint32_t)input[i * 4 + 2] << 16 |
                     (uint32_t)input[i * 4 + 3] << 24; // Little-endian conversion
    }

    return output;
}

extern uint8_t *inflate_buffer(uint32_t input_buffer_size, const uint8_t *input_buffer, uint32_t *output_buffer_size,uint32_t custom_output_buffer_size)
{
    if (input_buffer == NULL)
    {
        fprintf(stderr, "Error: Input buffer is null.\n");
        return NULL;
    }

    if (!huffman_tree_dict_initialized)
    {
        initialize_huffman_tree_dict();
        huffman_tree_dict_initialized = 1;
    }

    if (huffman_tree_dict.compressed_codes[0] == 0)
    {
        fprintf(stderr, "Huffman Tree Empty.\n");
        exit(EXIT_FAILURE);
    }

    // Convert uint8_t buffer to uint32_t buffer
    size_t u32_output_size;
    uint32_t* u32_input_buffer = convert_u8_to_u32(input_buffer, input_buffer_size, &u32_output_size);
    if (u32_input_buffer == NULL) {
        fprintf(stderr, "Error: Failed to convert input buffer.\n");
        return NULL;
    }

    // Initialize state
    State state_data;
    state_data.input_dat = u32_input_buffer; // Use the converted uint32_t buffer
    state_data.input_size = u32_output_size; // Update input size to uint32_t size
    state_data.input_position = 0;
    state_data.head = 0;
    state_data.bits = 0;
    state_data.buffer = 0;
    state_data.empty = 0;

    // Skipping header & getting size of the uncompressed data
    need_bits(&state_data, 32);
    drop_bits(&state_data, 32);

    // Getting size of the uncompressed data
    need_bits(&state_data, 32);
    uint32_t temp_output_buffer_size = readBits(&state_data, 32);
    drop_bits(&state_data, 32);

    if (*output_buffer_size != 0)
    {
        // We do not take max here as we won't be able to have more than the output available
        if (temp_output_buffer_size > *output_buffer_size)
        {
            temp_output_buffer_size = *output_buffer_size;
        }
    }

    *output_buffer_size = temp_output_buffer_size;

    if (custom_output_buffer_size>0)
    {
        temp_output_buffer_size=custom_output_buffer_size;
    }
    

    // Allocate memory for output buffer
    uint8_t *output_buffer = (uint8_t *)malloc(sizeof(uint8_t) * temp_output_buffer_size);
    if (output_buffer == NULL)
    {
        fprintf(stderr, "Error: Memory allocation failed.\n");
        *output_buffer_size = 0;
        return NULL;
    }

    // Inflate data
    inflate_data(&state_data, output_buffer, temp_output_buffer_size);

    return output_buffer;
}

