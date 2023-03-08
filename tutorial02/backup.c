
#include <SDL2/SDL_events.h>
#include <SDL2/SDL_scancode.h>
#include <libavutil/frame.h>
#include <unistd.h>
#include <stdio.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_thread.h>
#include <stdbool.h>

 static bool running = false;
 static int takeScreenShot = 0;
#define MAX_KEYBOARD_KEYS 500
static int keyboard[MAX_KEYBOARD_KEYS];
void doKeyDown(SDL_KeyboardEvent* event)
{
   if (event->repeat == 0 && event->keysym.scancode < MAX_KEYBOARD_KEYS)
	{
		keyboard[event->keysym.scancode] = 1;
	}

};

void doKeyUp(SDL_KeyboardEvent* event)
{
   if (event->repeat == 0 && event->keysym.scancode < MAX_KEYBOARD_KEYS)
	{
		    keyboard[event->keysym.scancode] = 0;
	}

};




 void saveFrame(AVFrame *avFrame, int width, int height, int frameIndex)
{
    FILE * pFile;
    char szFilename[32];
    int  y;

    /**
     * We do a bit of standard file opening, etc., and then write the RGB data.
     * We write the file one line at a time. A PPM file is simply a file that
     * has RGB information laid out in a long string. If you know HTML colors,
     * it would be like laying out the color of each pixel end to end like
     * #ff0000#ff0000.... would be a red screen. (It's stored in binary and
     * without the separator, but you get the idea.) The header indicated how
     * wide and tall the image is, and the max size of the RGB values.
     */

    // Open file
    sprintf(szFilename, "frame%d.ppm", frameIndex);
    pFile = fopen(szFilename, "wb");
    if (pFile == NULL)
    {
        return;
    }

    // Write header
    fprintf(pFile, "P6\n%d %d\n255\n", width, height);

    // Write pixel data
    for (y = 0; y < height; y++)
    {
        fwrite(avFrame->data[0] + y * avFrame->linesize[0], 1, width * 3, pFile);
    }

    // Close file
    fclose(pFile);
}

void handleInput(){

SDL_Event event;
     SDL_PollEvent(&event);
        switch(event.type)
        {
            case SDL_QUIT:
            {
                SDL_Quit();
                exit(0);
            }
            break;
            case SDL_KEYDOWN:
            doKeyDown(&event.key);
            break;
            case SDL_KEYUP:
            doKeyUp(&event.key);
            break;

            default:break;

            }
}
 
 AVFormatContext* screenCtx = NULL;
 AVCodecContext* origCodexCtx = NULL;
 AVCodecContext* screenCodecCtx = NULL;

AVCodec * screenCodec = NULL;
struct SwsContext * screen_sws_ctx = NULL;

      AVFrame * screenFrame = NULL;
AVFrame* pFrameRGB = NULL;
static int screenRet = -1;
static    int screenVideoStream = -1;
 AVPacket * screenPacket   = NULL;
int setUpAlternatePipeline(char* arguments[])
{


   screenRet = avformat_open_input(&screenCtx, arguments[1], NULL, NULL);
    if (screenRet < 0)
    {
        printf("Could not open file %s\n", arguments[1]);
        return -1;
    }

      screenRet = avformat_find_stream_info(screenCtx, NULL);
    if (screenRet < 0)
    {
        printf("Could not find stream information %s\n", arguments[1]);
        return -1;
    }

     av_dump_format(screenCtx, 0, arguments[1], 0);

int i;
      
    for (i = 0; i < screenCtx->nb_streams; i++)
    {
        if (screenCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            screenVideoStream = i;
            break;
        }
    }

    if (screenVideoStream == -1)
    {
        printf("Could not find video stream.");
        return -1;
    }

      screenCodec = avcodec_find_decoder(screenCtx->streams[screenVideoStream]->codecpar->codec_id);
    if (screenCodec == NULL)
    {
        printf("Unsupported codec!\n");
        return -1;
    }

     origCodexCtx = avcodec_alloc_context3(screenCodec);

   screenRet = avcodec_parameters_to_context(origCodexCtx, screenCtx->streams[screenVideoStream]->codecpar);
    if (screenRet != 0)
    {
        printf("Could not copy codec context.\n");
        return -1;
    }

     screenCodecCtx = avcodec_alloc_context3(screenCodec);

   screenRet = avcodec_parameters_to_context(screenCodecCtx, screenCtx->streams[screenVideoStream]->codecpar);
    if (screenRet != 0)
    {
        printf("Could not copy codec context.\n");
        return -1;
    }

      screenRet = avcodec_open2(screenCodecCtx, screenCodec, NULL);
    if (screenRet < 0)
    {
        printf("Could not open codec.\n");
        return -1;
    }

    screenFrame = av_frame_alloc();
    if (screenFrame == NULL)
    {
        printf("Could not allocate frame.\n");
        return -1;
    }



pFrameRGB = av_frame_alloc();
 if (pFrameRGB == NULL)
    {
        // Could not allocate frame
        printf("Could not allocate frame.\n");

        // exit with error
        return -1;
    }

     uint8_t * screenShotBuffer = NULL;
    int screenNumBytes;

    // Determine required buffer size and allocate buffer
    // numBytes = avpicture_get_size(AV_PIX_FMT_RGB24, pCodecCtx->width, pCodecCtx->height);
    // https://ffmpeg.org/pipermail/ffmpeg-devel/2016-January/187299.html
    // what is 'linesize alignment' meaning?:
    // https://stackoverflow.com/questions/35678041/what-is-linesize-alignment-meaning
    screenNumBytes = av_image_get_buffer_size(AV_PIX_FMT_RGB24, screenCodecCtx->width, screenCodecCtx->height, 32); // [10]
    screenShotBuffer = (uint8_t *) av_malloc(screenNumBytes * sizeof(uint8_t));    // [11]


     av_image_fill_arrays( // [12]
        pFrameRGB->data,
        pFrameRGB->linesize,
        screenShotBuffer,
        AV_PIX_FMT_RGB24,
        screenCodecCtx->width,
        screenCodecCtx->height,
        32
    );

     screenPacket = av_packet_alloc();
    if (screenPacket == NULL)
    {
        printf("Could not alloc packet,\n");
        return -1;
    }

     screen_sws_ctx = sws_getContext(   // [13]
        screenCodecCtx->width,
        screenCodecCtx->height,
        screenCodecCtx->pix_fmt,
        screenCodecCtx->width,
        screenCodecCtx->height,
        AV_PIX_FMT_RGB24,   // sws_scale destination color scheme
        SWS_BILINEAR,
        NULL,
        NULL,
        NULL
    );

}

int main(int argc, char * argv[])
{
    if ( !(argc > 2) )
    {
        
        return -1;
    }

    int ret = -1;
    int screenRet = -1;

    /**
     * SDL.
     * New API: this implementation does not use deprecated SDL functionalities.
     * Plese refer to tutorial02-deprecated.c for an implementation using the deprecated
     * SDL API.
     */
    ret = SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER);   // [1]
    if (ret != 0)
    {
        // error while initializing SDL
        printf("Could not initialize SDL - %s\n.", SDL_GetError());

        // exit with error
        return -1;
    }

    AVFormatContext * pFormatCtx = NULL;
    ret = avformat_open_input(&pFormatCtx, argv[1], NULL, NULL);
    if (ret < 0)
    {
        printf("Could not open file %s\n", argv[1]);
        return -1;
    }

    ret = avformat_find_stream_info(pFormatCtx, NULL);
    if (ret < 0)
    {
        printf("Could not find stream information %s\n", argv[1]);
        return -1;
    }

    av_dump_format(pFormatCtx, 0, argv[1], 0);
    
    int i;
    
    AVCodecContext * pCodecCtx = NULL;
 
    int videoStream = -1;
    for (i = 0; i < pFormatCtx->nb_streams; i++)
    {
        if (pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            videoStream = i;
            break;
        }
    }

    if (videoStream == -1)
    {
        printf("Could not find video stream.");
        return -1;
    }

    AVCodec * pCodec = NULL;
    pCodec = avcodec_find_decoder(pFormatCtx->streams[videoStream]->codecpar->codec_id);
    if (pCodec == NULL)
    {
        printf("Unsupported codec!\n");
        return -1;
    }

    pCodecCtx = avcodec_alloc_context3(pCodec);

    ret = avcodec_parameters_to_context(pCodecCtx, pFormatCtx->streams[videoStream]->codecpar);
    if (ret != 0)
    {
        printf("Could not copy codec context.\n");
        return -1;
    }

    ret = avcodec_open2(pCodecCtx, pCodec, NULL);
    if (ret < 0)
    {
        printf("Could not open codec.\n");
        return -1;
    }


    AVFrame * pFrame = NULL;
    pFrame = av_frame_alloc();
    if (pFrame == NULL)
    {
        printf("Could not allocate frame.\n");
        return -1;
    }

    // Create a window with the specified position, dimensions, and flags.
    SDL_Window * screen = SDL_CreateWindow( // [2]
                            "SDL Video Player",
                            SDL_WINDOWPOS_UNDEFINED,
                            SDL_WINDOWPOS_UNDEFINED,
                            pCodecCtx->width/2,
                            pCodecCtx->height/2,
                            SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI
    );

    if (!screen)
    {
        // could not set video mode
        printf("SDL: could not set video mode - exiting.\n");

        // exit with Error
        return -1;
    }
	
    //
    SDL_GL_SetSwapInterval(1);

    // A structure that contains a rendering state.
    SDL_Renderer * renderer = NULL;

    // Use this function to create a 2D rendering context for a window.
    renderer = SDL_CreateRenderer(screen, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_TARGETTEXTURE);   // [3]

    // A structure that contains an efficient, driver-specific representation
    // of pixel data.
    SDL_Texture * texture = NULL;

    // Use this function to create a texture for a rendering context.
    texture = SDL_CreateTexture(  // [4]
                renderer,
                SDL_PIXELFORMAT_YV12,
                SDL_TEXTUREACCESS_STREAMING,
                pCodecCtx->width,
                pCodecCtx->height
            );



    struct SwsContext * sws_ctx = NULL;
    AVPacket * pPacket = av_packet_alloc();
    if (pPacket == NULL)
    {
        printf("Could not alloc packet,\n");
        return -1;
    }

     
   

    // set up our SWSContext to convert the image data to YUV420:
    sws_ctx = sws_getContext(
        pCodecCtx->width,
        pCodecCtx->height,
        pCodecCtx->pix_fmt,
        pCodecCtx->width,
        pCodecCtx->height,
        AV_PIX_FMT_YUV420P,
        SWS_BILINEAR,
        NULL,
        NULL,
        NULL
    );

      // initialize SWS context for software scaling
   

    /**
     * As we said before, we are using YV12 to display the image, and getting
     * YUV420 data from ffmpeg.
     */

 

    int numBytes;
    uint8_t * buffer = NULL;

    numBytes = av_image_get_buffer_size(
                AV_PIX_FMT_YUV420P,
                pCodecCtx->width,
                pCodecCtx->height,
                32
            );
    buffer = (uint8_t *) av_malloc(numBytes * sizeof(uint8_t));

    // used later to handle quit event
    SDL_Event event;

    AVFrame * pict = av_frame_alloc();

    av_image_fill_arrays(
        pict->data,
        pict->linesize,
        buffer,
        AV_PIX_FMT_YUV420P,
        pCodecCtx->width,
        pCodecCtx->height,
        32
    );

    
    

    int maxFramesToDecode;
    sscanf(argv[2], "%d", &maxFramesToDecode);
    running = true;

    i = 0;
    
    setUpAlternatePipeline(argv);
  int screenI = 0;
static int iii = 0;
    while (av_read_frame(pFormatCtx, pPacket) >= 0 && running && av_read_frame(screenCtx, screenPacket) >=0)
    {
    //while(av_read_frame(screenCtx, screenPacket) >=0){
        handleInput();

	   
       if (pPacket->stream_index == videoStream && screenPacket->stream_index == screenVideoStream)
        {
            ret = avcodec_send_packet(pCodecCtx, pPacket);
            if (ret < 0)
            {
                printf("Error sending packet for decoding.\n");
                return -1;
            }

             screenRet = avcodec_send_packet(screenCodecCtx, screenPacket);    // [15]
            if (screenRet < 0)
            {
                printf("\n%d\n", screenRet);
                printf("%d", iii);
                // could not send packet for decoding
                printf("Error sending screen packet for decoding.\n");

                // exit with eror
                return -1;
            }
iii++;
            while (ret >= 0 && screenRet >=0)
            {
                ret = avcodec_receive_frame(pCodecCtx, pFrame);

                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                {
                    break;
                }
                else if (ret < 0)
                {
                    printf("Error while decoding.\n");
                    return -1;
                }

                screenRet = avcodec_receive_frame(screenCodecCtx, screenFrame);

                if (screenRet == AVERROR(EAGAIN) || screenRet == AVERROR_EOF)
                {
                    break;
                }
                else if (screenRet < 0)
                {
                    printf("Error while decoding.\n");
                    return -1;
                }


                // Convert the image into YUV format that SDL uses:
                // We change the conversion format to PIX_FMT_YUV420P, and we
                // use sws_scale just like before.
                sws_scale(
                    sws_ctx,
                    (uint8_t const * const *)pFrame->data,
                    pFrame->linesize,
                    0,
                    pCodecCtx->height,
                    pict->data,
                    pict->linesize
                );

                sws_scale(  // [16]
                    screen_sws_ctx,
                    (uint8_t const * const *)screenFrame->data,
                    screenFrame->linesize,
                    0,
                    screenCodecCtx->height,
                    pFrameRGB->data,
                    pFrameRGB->linesize
                );

                if (++i >= 0)
                {
                    // get clip fps
                    double fps = av_q2d(pFormatCtx->streams[videoStream]->r_frame_rate);

                    // get clip sleep time
                    double sleep_time = 1.0/(double)fps;

                    // sleep: usleep won't work when using SDL_CreateWindow
                    // usleep(sleep_time);
                    // Use SDL_Delay in milliseconds to allow for cpu scheduling
                    SDL_Delay((1000 * sleep_time) - 10);    // [5]

                    // The simplest struct in SDL. It contains only four shorts. x, y which
                    // holds the position and w, h which holds width and height.It's important
                    // to note that 0, 0 is the upper-left corner in SDL. So a higher y-value
                    // means lower, and the bottom-right corner will have the coordinate x + w,
                    // y + h.
                    SDL_Rect rect;
                    rect.x = 0;
                    rect.y = 0;
                    rect.w = pCodecCtx->width;
                    rect.h = pCodecCtx->height;
                     sws_ctx = sws_getContext(   // [13]
        pCodecCtx->width,
       pCodecCtx->height,
        pCodecCtx->pix_fmt,
        pCodecCtx->width,
        pCodecCtx->height,
        AV_PIX_FMT_RGB24,   // sws_scale destination color scheme
        SWS_BILINEAR,
        NULL,
        NULL,
        NULL
    );

saveFrame(pFrame, pCodecCtx->width, pCodecCtx->height, i);
                    if(keyboard[SDL_SCANCODE_F2] ){

                      
                
					saveFrame(pFrame, pCodecCtx->width, pCodecCtx->height, i);
                    }
                  

                    // Use this function to update a rectangle within a planar
                    // YV12 or IYUV texture with new pixel data.
                    SDL_UpdateYUVTexture(
                        texture,            // the texture to update
                        &rect,              // a pointer to the rectangle of pixels to update, or NULL to update the entire texture
                        pict->data[0],      // the raw pixel data for the Y plane
                        pict->linesize[0],  // the number of bytes between rows of pixel data for the Y plane
                        pict->data[1],      // the raw pixel data for the U plane
                        pict->linesize[1],  // the number of bytes between rows of pixel data for the U plane
                        pict->data[2],      // the raw pixel data for the V plane
                        pict->linesize[2]   // the number of bytes between rows of pixel data for the V plane
                    );

                    // clear the current rendering target with the drawing color
                    SDL_RenderClear(renderer);

                    // copy a portion of the texture to the current rendering target
                    SDL_RenderCopy(
                        renderer,   // the rendering context
                        texture,    // the source texture
                        NULL,       // the source SDL_Rect structure or NULL for the entire texture
                        NULL        // the destination SDL_Rect structure or NULL for the entire rendering
                                    // target; the texture will be stretched to fill the given rectangle
                    );

                   
                    SDL_RenderPresent(renderer);
                }
                else
                {
                    break;
                }
            }

           
        }

        av_packet_unref(pPacket);
          av_packet_unref(screenPacket);
        
    
}
    av_frame_free(&pFrame);
    av_free(pFrame);
    avcodec_close(pCodecCtx);
    avformat_close_input(&pFormatCtx);

    SDL_DestroyRenderer(renderer);
    SDL_Quit();


}
