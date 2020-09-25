#ifndef COMPRESSION_H
#define COMPRESSION_H

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>



#define N_SEGMENTS (256)


#define SET_BIT_32(BF,N)   ((byte) |= (1<<(N)))
#define IS_BIT_SET_32(BF,N) ((byte) & (1<<(N)))



/* USYD CODE CITATION ACKNOWLEDGEMENT
 * I declare that the macros have been modified from the
 * website
 * 
 * Original URL
 * // http://www.mathcs.emory.edu/~cheung/Courses/255/Syllabus/1-C-intro/bit-array.html
 * Last access May, 2020
 */


#define TEST_BIT(A,k) (A[(k/8)] & ((uint8_t)1 << (7-(k%8)) )) 
#define SET_BIT_BA(A,k)( A[(k/8)] |= ((uint8_t)1 << (7-(k%8)) )) 

struct bit_code {
    uint8_t length;
    size_t offset;
    // uint32_t bits;
    uint8_t byte;
};

struct node {
    bool is_leaf;
    uint8_t byte;
    struct node* right;
    struct node* left;
    
};

struct compression_info
{
    uint8_t* bit_array;
    size_t array_size;
    struct bit_code* bit_codes;
    struct node* root;
};




struct compression_info* create_compression_info();

void create_bit_array(struct compression_info* c_info);

void set_bit_codes(struct compression_info* c_info);

void create_decompression_tree(struct compression_info* c_info);

struct node* create_node();

void add_node(struct compression_info* c_info, struct node** root,
             struct bit_code* code);

void decompress_payload(struct compression_info* c_info, uint8_t** payload,
                        uint64_t* payload_len);

void compress_payload(struct compression_info* c_info, uint8_t** payload, 
    uint64_t* payload_len);

void free_decompression_tree(struct node* n);    

void free_compression_info(struct compression_info* c_info);

#endif