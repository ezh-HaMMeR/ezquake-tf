#ifndef EZQUAKE_FRAME_CAPTURE_H
#define EZQUAKE_FRAME_CAPTURE_H

void FrameCapture_Init(void);
void FrameCapture_Shutdown(void);
void FrameCapture_FrameStart(void);
void FrameCapture_FrameEnd(void);
void FrameCapture_InputBegin(void);
void FrameCapture_InputEnd(int mouse_x, int mouse_y);
void FrameCapture_RenderBegin(void);
void FrameCapture_RenderEnd(void);
void FrameCapture_SwapBegin(void);
void FrameCapture_SwapEnd(void);

#endif
