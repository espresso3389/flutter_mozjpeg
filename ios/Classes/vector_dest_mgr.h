#include <vector>

struct vector_dest_mgr
{
public:
    // WARNING: lifetime of the buffer should be equal to or longer than cinfo.
    static void init(j_compress_ptr cinfo, std::vector<unsigned char> &buffer)
    {
        if (cinfo->dest == NULL)
        { /* first time for this JPEG object? */
            cinfo->dest = (jpeg_destination_mgr *)(*cinfo->mem->alloc_small)((j_common_ptr)cinfo, JPOOL_PERMANENT, sizeof(vector_dest_mgr));
        }
        else if (cinfo->dest->init_destination != init_vector_destination)
        {
            throw JERR_BUFFER_SIZE;
        }

        const size_t INITIAL_BUFFER_SIZE = 32 * 1024;
        vector_dest_mgr *dest = (vector_dest_mgr *)cinfo->dest;
        dest->pub.init_destination = init_vector_destination;
        dest->pub.empty_output_buffer = empty_vector_output_buffer;
        dest->pub.term_destination = term_vector_destination;
        dest->buffer = &buffer;
        if (buffer.empty())
            buffer.resize(INITIAL_BUFFER_SIZE);
        dest->pub.next_output_byte = dest->buffer->data();
        dest->pub.free_in_buffer = dest->buffer->size();
    }

private:
    struct jpeg_destination_mgr pub;
    std::vector<unsigned char> *buffer;

    static void init_vector_destination(j_compress_ptr cinfo)
    {
        /* no work necessary here */
    }

    static boolean empty_vector_output_buffer(j_compress_ptr cinfo)
    {
        vector_dest_mgr *dest = (vector_dest_mgr *)cinfo->dest;
        if (dest->pub.free_in_buffer > 0)
            return TRUE;
        size_t curSize = dest->buffer->size();
        dest->buffer->resize(curSize * 2);
        dest->pub.next_output_byte = dest->buffer->data() + curSize;
        dest->pub.free_in_buffer = curSize;
        return TRUE;
    }

    static void term_vector_destination(j_compress_ptr cinfo)
    {
        vector_dest_mgr *dest = (vector_dest_mgr *)cinfo->dest;
        dest->buffer->resize(dest->buffer->size() - dest->pub.free_in_buffer);
    }
};
