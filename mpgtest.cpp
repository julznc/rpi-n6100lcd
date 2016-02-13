
#include <stdio.h>
#include <unistd.h>

extern "C" {
  //#include <ffmpeg.h>
  #include <libavformat/avformat.h>
  #include <libavcodec/avcodec.h>
  #include <libswscale/swscale.h>
}

#include "n6100lcd.h"

#define LCD_PIXFORMAT   AV_PIX_FMT_RGB444LE

// https://github.com/phamquy/FFmpeg-tutorial-samples/
int play_video(const char *filename, N6100LCD *plcd)
{
    AVFormatContext *pFormatCtx;
    AVCodecContext  *pCodecCtx;
    AVCodec         *pCodec;

    // Register all formats and codecs
    av_register_all();

    pFormatCtx = avformat_alloc_context();

    // Open video file
    if(avformat_open_input(&pFormatCtx, filename, NULL, NULL) != 0)
    {
        printf("Couldn't open file\n");
        return -1;
    }

    // Retrieve stream information
    if(avformat_find_stream_info(pFormatCtx, NULL) < 0)
    {
        printf("Couldn't find stream information!\n");
        return -1;
    }

    // Dump information about file onto standard error
    av_dump_format(pFormatCtx, 0, filename, 0);

    // Find the first video stream
    int videoStreamIdx = -1;
    {
        int i = 0;
        for(i=0; i<pFormatCtx->nb_streams; i++)
        {
            if(pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) { //CODEC_TYPE_VIDEO
                videoStreamIdx=i;
                break;
            }
        }
    }

    // Check if video stream is found
    if(videoStreamIdx==-1) {
        printf("Didn't find a video stream!\n");
        return -1;
    }

    // Get a pointer to the codec context for the video stream
    pCodecCtx = pFormatCtx->streams[videoStreamIdx]->codec;


    // Find the decoder for the video stream
    pCodec = avcodec_find_decoder( pCodecCtx->codec_id);
    if(pCodec==NULL) {
        printf("Codec not found!\n");
        return -1;
    }

    // Open codec
    if( avcodec_open2(pCodecCtx, pCodec, NULL) < 0 ) {
        printf("Could not open codec!\n");
        return -1;
    }

    // Allocate video frame
    AVFrame        *pFrame = av_frame_alloc();
    AVFrame        *pFrameRGB = av_frame_alloc();;
    AVPacket        packet;
    int             frameFinished;

    if(NULL==pFrame || NULL==pFrameRGB) {
        printf("Could not allocate frame structure!\n");
        return -1;
    }

    int w = pCodecCtx->width;
    int h = pCodecCtx->height;
    int numBytes=avpicture_get_size(LCD_PIXFORMAT, w, h);

    uint8_t *buffer=(uint8_t *)av_malloc(numBytes*sizeof(uint8_t));
    avpicture_fill((AVPicture *)pFrameRGB, buffer, LCD_PIXFORMAT, w, h);

    // initialize SWS context for software scaling
    struct SwsContext *sws_ctx = sws_getContext(w, h, pCodecCtx->pix_fmt,
                                       w, h, LCD_PIXFORMAT,
                                       SWS_BICUBIC, NULL, NULL, NULL);

    while (av_read_frame(pFormatCtx, &packet) >= 0) {
        // Is this a packet from the video stream?
        if(packet.stream_index==videoStreamIdx) {

            // Decode video frame
            avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished, &packet);

            // Did we get a video frame?
            if(frameFinished) {
                // Convert the image from its native format to RGB
                sws_scale(sws_ctx, (uint8_t const * const *)pFrame->data,
                            pFrame->linesize, 0, h,
                            pFrameRGB->data, pFrameRGB->linesize);
                for (int y=0; y < h; y++) {
                    uint16_t *line = (uint16_t*)(pFrameRGB->data[0] + y*pFrameRGB->linesize[0]);
                    //plcd->draw_hline(line, 0, y, w);
                    plcd->draw_hline(line, 0, y+20, w);
                }
                usleep(100); // todo: use pCodecCtx->framerate?
            }
        }
        // Free the packet that was allocated by av_read_frame
        av_free_packet(&packet);
    }

    // Free the RGB image
    av_free(buffer);
    av_frame_free(&pFrameRGB);

    // Free the YUV frame
    av_frame_free(&pFrame);

    // Close the codec
    avcodec_close(pCodecCtx);

    // Close the video file
    avformat_close_input(&pFormatCtx);

    return 0;
}

int main(int argc, char *argv[])
{
    N6100LCD lcd;
    lcd.init();
    lcd.clear(BLUE);
    sleep(1);

    while (0==play_video(argc>1? argv[1]:"/home/pi/demo.mpg", &lcd));

    return 0;
}
