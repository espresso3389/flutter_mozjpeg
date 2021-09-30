#include "cdjpeg.h"
#include "cdjapi.h"
#include "jconfigint.h"

#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

#include <string>
#include <vector>
#include <memory>

#include "vector_dest_mgr.h"

static int comps[] = {
    -1,
    1, // Grayscale
    3, // RGB
    3, // YCbCr
    4, // CMYK
    4, // YCCK
    3, // extRGB
    4, // extRGBX
    3, // extBGR
    4, // extBGRX
    4, // extXBGR
    4, // extXRGB
    4, // extRGBA
    4, // extBGRA
    4, // extABGR
    4, // extARGB
    1, // RGB565???
};

extern "C" __attribute__((visibility("default"))) __attribute__((used)) void jpeg_compress(const unsigned char *p0, int width, int height, int stride, int input_cs, int quality, int dpi, void *context)
{
    jpeg_compress_struct cinfo;
    jpeg_error_mgr jsrcerr;
    cinfo.err = debug_foward_error(&jsrcerr);

    jpeg_create_compress(&cinfo);

    cdjpeg_progress_mgr progress;
    start_progress_monitor((j_common_ptr)&cinfo, &progress, context);

    std::vector<unsigned char> outbuffer;
    try
    {
        vector_dest_mgr::init(&cinfo, outbuffer);

        cinfo.image_width = (JDIMENSION)width;
        cinfo.image_height = (JDIMENSION)height;
        cinfo.input_components = comps[input_cs];

        jpeg_c_set_int_param(&cinfo, JINT_COMPRESS_PROFILE, JCP_MAX_COMPRESSION);

        cinfo.in_color_space = (J_COLOR_SPACE)input_cs;
        jpeg_set_defaults(&cinfo);
        cinfo.err->trace_level = 0;

        jpeg_set_quality(&cinfo, quality, FALSE);

        cinfo.density_unit = 1; // dpi
        cinfo.X_density = (UINT16)dpi;
        cinfo.Y_density = (UINT16)dpi;

        cinfo.write_JFIF_header = TRUE;
        cinfo.write_Adobe_marker = FALSE;

        jpeg_start_compress(&cinfo, TRUE);

        for (int y = 0; y < height; y++)
        {
            const unsigned char *pLine = p0 + stride * y;
            jpeg_write_scanlines(&cinfo, (JSAMPARRAY)&pLine, 1);
        }
    }
    catch (int code)
    {
        debug_printf("Woops, exit_code=%d\n", code);
        jpeg_destroy_compress(&cinfo);
        post_progress_monitor((j_common_ptr)&cinfo, PROGRESS_PASS_EXITCODE, 0, code);
        return;
    }
    catch (std::exception &e)
    {
        debug_printf("exception: %s\n", e.what());
        jpeg_destroy_compress(&cinfo);
        post_progress_monitor((j_common_ptr)&cinfo, PROGRESS_PASS_EXITCODE, 0, -1);
        return;
    }

    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);
    debug_printf("compression succeeded.\n");

    std::vector<unsigned char> *pVector = new std::vector<unsigned char>(std::move(outbuffer));
    notify_progress_v(progress.context, PROGRESS_PASS_VECTOR_PTR, 0, pVector);
    post_progress_monitor((j_common_ptr)&cinfo, PROGRESS_PASS_EXITCODE, 0, 0);
}

extern "C" __attribute__((visibility("default"))) __attribute__((used)) void *jpeg_compress_get_ptr(void *p)
{
    if (!p)
        return NULL;
    std::vector<unsigned char> &v = *(std::vector<unsigned char> *)p;
    return v.data();
}

extern "C" __attribute__((visibility("default"))) __attribute__((used)) size_t jpeg_compress_get_size(void *p)
{
    if (!p)
        return -1;
    std::vector<unsigned char> &v = *(std::vector<unsigned char> *)p;
    return v.size();
}

extern "C" __attribute__((visibility("default"))) __attribute__((used)) void jpeg_compress_release(void *p)
{
    if (p)
        delete (std::vector<unsigned char> *)p;
}

#include <pthread.h>

class JpegCompressParams
{
public:
    JpegCompressParams(const unsigned char *p0, int width, int height, int stride, int input_cs, int quality, int dpi, void *context) : p0(p0), width(width), height(height), stride(stride), input_cs(input_cs), quality(quality), dpi(dpi), context(context)
    {
    }

    void fireAndForget()
    {
        pthread_t t;
        if (pthread_create(&t, NULL, s_start, (void *)this) != 0)
        {
            notify_progress(context, PROGRESS_PASS_EXITCODE, -1, -1); // error
        }
    }

private:
    const unsigned char *p0;
    int width;
    int height;
    int stride;
    int input_cs;
    int quality;
    int dpi;
    void *context;

    static void *s_start(void *param)
    {
        JpegCompressParams &jcp = *(JpegCompressParams *)param;
        jpeg_compress(jcp.p0, jcp.width, jcp.height, jcp.stride, jcp.input_cs, jcp.quality, jcp.dpi, jcp.context);
        delete &jcp;
        return 0;
    }
};

extern "C" __attribute__((visibility("default"))) __attribute__((used)) void jpeg_compress_threaded(const unsigned char *p0, int width, int height, int stride, int input_cs, int quality, int dpi, void *context)
{
    JpegCompressParams *jcp = new JpegCompressParams(p0, width, height, stride, input_cs, quality, dpi, context);
    jcp->fireAndForget();
}
