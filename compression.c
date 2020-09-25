#include "compression.h"
#include "requests.h"

// This fil contains the functions necessary functions for
// compressing and decompressing the payload.


struct compression_info* create_compression_info()
{
    struct compression_info* c_info = malloc(sizeof(*c_info));

    create_bit_array(c_info);
    set_bit_codes(c_info);
    create_decompression_tree(c_info);

    return c_info;
}

// stores the contents of the compression.dict file into memory
void create_bit_array(struct compression_info* c_info)
{
    char* file_name = "compression.dict";
    FILE* f = fopen(file_name, "rb");

    if (f == NULL)
        perror("couldn't open compression.dict");

    
    size_t array_cap = 50;
    uint8_t* array = malloc(sizeof(*array)*array_cap);

    int i = 0;
    while (true)
    {
        if (feof(f))
        {
            break;
        }

        if (i >= array_cap)
        {
            array_cap += 20;
            array = realloc(array, sizeof(*array)*array_cap);
        }

        array[i] = fgetc(f);

        i += 1;

    }

    fclose(f);

    c_info->array_size = i;
    c_info->bit_array = array;
}

// Creates a dictionary which can be used to access the bitcodes for a 
// specific bytes. The length of the bitcode and its offset in the bit 
// array is stored in the bit_code struct
void set_bit_codes(struct compression_info* c_info)
{
    struct bit_code* codes = malloc(sizeof(*codes)*N_SEGMENTS);
    uint8_t len_buffer;

    size_t i = 0;
    size_t j = 0;
    size_t n_codes = 0;

    
    while (n_codes < N_SEGMENTS)
    {
        len_buffer = 0;
        j = 0;

        while (j < 8)
        {
            if (TEST_BIT(c_info->bit_array, i))
            {
                SET_BIT(len_buffer, j);
            }
            i++;
            j++;
        }

        codes[n_codes].length = len_buffer;
        codes[n_codes].offset = i;
        codes[n_codes].byte = n_codes;

        i = i + len_buffer;
   
        n_codes++;
    }

    c_info->bit_codes = codes;
}


void create_decompression_tree(struct compression_info* c_info)
{
    c_info->root = create_node();

    for (size_t i = 0; i < N_SEGMENTS; i++)
    {
        add_node(c_info, &c_info->root, &c_info->bit_codes[i]);
    }
}


struct node* create_node()
{
    struct node* n = malloc(sizeof(*n));
    n->right = NULL;
    n->left = NULL;
    n->is_leaf = false;

    return n;
}


// adds a new node in the tree based on the bits of an the bit code
// adds nodes along the way to the left if 0 and right if 1
void add_node(struct compression_info* c_info, struct node** root,
             struct bit_code* code)
{
    uint64_t index;
    struct node* cursor = *root;
    
    for (size_t i = 0; i < code->length; i++)
    {
        index = code->offset + i;

        if (TEST_BIT(c_info->bit_array, index))
        {
            if (cursor->right == NULL)
            {                
                cursor->right = create_node();
            }
            
            cursor = cursor->right;
            continue;
            
        }
        else 
        {
            if (cursor->left == NULL)
            {
                cursor->left = create_node();
            }
            
            cursor = cursor->left;
            continue;
            
        }
    }

    cursor->is_leaf = true;
    cursor->byte = code->byte;
    
}

// decompresses the payloae and resets the payload appropiately 
void decompress_payload(struct compression_info* c_info, uint8_t** payload,
                        uint64_t* payload_len)
{
    uint64_t decompressed_cap = (*payload_len)*4;
    uint8_t* decompressed = malloc(sizeof(*decompressed)*decompressed_cap);
    bzero(decompressed, decompressed_cap);

    size_t d_len = 0;
    uint8_t* bit_array = *payload;
    struct node* cursor = c_info->root;

    for (size_t i = 0; i < (*payload_len-1)*8; i++)
    {

        if (cursor->is_leaf)
        {
            decompressed[d_len] = cursor->byte;
            d_len++;

            cursor = c_info->root;
        }

        if (TEST_BIT(bit_array, i))
        {
            cursor = cursor->right;
        }
        else
        {
            cursor = cursor->left;
        }
        
    }

    free(*payload);
    *payload = decompressed;
    *payload_len = d_len;
    
}



void compress_payload(struct compression_info* c_info, uint8_t** payload, 
    uint64_t* payload_len)
{
    uint64_t compressed_cap = (*payload_len)*4;
    uint8_t* compressed = malloc(sizeof(*compressed)*compressed_cap);
    bzero(compressed, compressed_cap);
    

    size_t len_index = 0;
    size_t comp_index = 0;
    size_t code_index = 0;
    size_t ba_index = 0;
    
    struct bit_code* code;
    

    for (size_t i = 0; i < *payload_len; i++)
    {
        len_index = 0;
        code_index = (int) (*payload)[i];
        code = &(c_info->bit_codes[code_index]);

        while (len_index < code->length)
        {
            ba_index = len_index + code->offset;
            if (TEST_BIT(c_info->bit_array, ba_index))
            {
                SET_BIT_BA(compressed, comp_index);
            }

            comp_index++;
            len_index++;
        }

    }

    // case when the compressed payload is already aligned with a byte boundary
    if (comp_index % 8 == 0)
    {
        *payload_len = comp_index/8;
        compressed[*payload_len] = 0;
        *payload_len = *payload_len + 1;  
    }
    // case when the compressed payload is not aligned
    else
    {
        *payload_len = comp_index/8 + 1;
        compressed[*payload_len] = 8 - (comp_index % 8);    
        *payload_len = *payload_len + 1;
    }

    free(*payload);
    *payload = compressed;

}

// frees the decompression tree using a post order traversal
void free_decompression_tree(struct node* n)
{
    if (n->is_leaf)
    {
        free(n);
    }
    else
    {
        if (n->left != NULL)
        {
            free_decompression_tree(n->left);
        }
            
        if (n->right != NULL)
        {
            free_decompression_tree(n->right);
        }
        
        free(n);
    }
    
}

void free_compression_info(struct compression_info* c_info)
{
    free(c_info->bit_codes);
    free(c_info->bit_array);
    free_decompression_tree(c_info->root);
    free(c_info);
}
