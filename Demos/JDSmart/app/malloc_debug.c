#include <stdint.h>
#include <stdlib.h>

//#include <mico_rtos.h>

struct mcp_str
{
    unsigned int ptr;
    int len;
};

#define MALLOC_NUM 256
struct mcp_str mcp[MALLOC_NUM];
int mcp_index = 0;


#define MAGIC_NUMBER 0xefdcba98

int start_mem_debug = 0;

void* MyMalloc(size_t bytes)
{
    uint8_t *p, *p_end;
    bytes = (bytes + 3) & ~3; /* ensure alignment for magic number */
    p = (uint8_t*)__iar_dlmalloc(bytes + 8); /* add 2x 32- b it for size and magic number */

    if (p == NULL)
    {
        abort(); /* out of memory */
    }

    *((uint32_t*) p) = bytes; /* remember size */
    *((uint32_t*)(p + 4 + bytes)) = MAGIC_NUMBER; /* write magic number after user allocation */

    /* crude method of estimating maximum used  size since application start */
    if (mcp_index < MALLOC_NUM)
    {
        mcp[mcp_index].ptr = (unsigned int)(p + 4);
        mcp[mcp_index].len = bytes;
        mcp_index++;
    }

    return p + 4; /* allocated area starts after size */
}

void* MyCalloc(size_t n, size_t size)
{
    uint8_t *p, *p_end;
    size_t bytes;
    bytes = n * size;
    bytes = (bytes + 3) & ~3; /* ensure alignment for magic number */
    p = (uint8_t*)__iar_dlmalloc(bytes + 8); /* add 2x 32- b it for size and magic number */

    if (p == NULL)
    {
        abort(); /* out of memory */
    }

    memset(p, 0, bytes + 8);
    *((uint32_t*) p) = bytes; /* remember size */
    *((uint32_t*)(p + 4 + bytes)) = MAGIC_NUMBER; /* write magic number after user allocation */

    /* crude method of estimating maximum used  size since application start */
    if (mcp_index < MALLOC_NUM)
    {
        mcp[mcp_index].ptr = (unsigned int)(p + 4);
        mcp[mcp_index].len = bytes;
        mcp_index++;
    }

    return p + 4; /* allocated area starts after size */

}

void MyFree(void* vp)
{
    int i;

    if (!vp)
        return;

    uint8_t* p = (uint8_t*)vp  -   4;
    int bytes = *((uint32_t*)p);

    for (i = 0; i < mcp_index; i++)
    {
        if (mcp[i].ptr == (unsigned int)vp)
        {
            mcp_index--;
            mcp[i] = mcp[mcp_index];
            break;
        }
    }

    /* check that magic number is not corrupted */
    if (*((uint32_t*)(p + 4 + bytes)) != MAGIC_NUMBER)
    {
        printf("vp %p, bytes %d\r\n", vp, bytes);
        abort(); /* error: data overflow or  freeing already freed memory */
    }

    *((uint32_t*)(p + 4 + bytes)) = 0; /* remove magic number to be able to detect freeing already freed memory */
    __iar_dlfree(p);
}

void *MyRealloc(void *mem_address, size_t newsize)
{
    if (!mem_address)
        return MyMalloc(newsize);

    uint8_t* p = (uint8_t*)mem_address  -   4;
    int bytes = *((uint32_t*)p);

    if (bytes >= newsize)
        return mem_address;

    p = MyMalloc(newsize);

    if (!p)
        return NULL;

    memcpy(p, mem_address, bytes);
    MyFree(mem_address);

    return p;
}


