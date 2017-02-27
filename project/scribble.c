//*****************************************************************************
//
// scribble.c - A simple scribble pad to demonstrate the touch screen driver.
//
//*****************************************************************************
#include <stdint.h>
#include <stdbool.h>

#include "inc/hw_sysctl.h"
#include "inc/hw_types.h"
#include "driverlib/flash.h"
#include "driverlib/sysctl.h"
#include "grlib/grlib.h"
#include "grlib/widget.h"
#include "utils/ringbuf.h"
#include "utils/ustdlib.h"
#include "Kentec320x240x16_ssd2119_8bit.h"
#include "touch.h"
//#include "drivers/set_pinout.h"
#include "driverlib/rom.h"

#include "utils/ringbuf.c"

//*****************************************************************************
//
//! \addtogroup example_list
//! <h1>Scribble Pad (scribble)</h1>
//!
//! The scribble pad provides a drawing area on the screen.  Touching the
//! screen will draw onto the drawing area using a selection of fundamental
//! colors (in other words, the seven colors produced by the three color
//! channels being either fully on or fully off).  Each time the screen is
//! touched to start a new drawing, the drawing area is erased and the next
//! color is selected.
//
//*****************************************************************************

//*****************************************************************************
//
// A structure used to pass touchscreen messages from the interrupt-context
// handler function to the main loop for processing.
//
//*****************************************************************************
typedef struct
{
    unsigned long ulMsg;
    long lX;
    long lY;
}
tScribbleMessage;

//*****************************************************************************
//
// The number of messages we can store in the message queue.
//
//*****************************************************************************
#define MSG_QUEUE_SIZE 16

//*****************************************************************************
//
// The ring buffer memory and control structure we use to implement the
// message queue.
//
//*****************************************************************************
static tScribbleMessage g_psMsgQueueBuffer[MSG_QUEUE_SIZE];
static tRingBufObject g_sMsgQueue;

//*****************************************************************************
//
// The error routine that is called if the driver library encounters an error.
//
//*****************************************************************************
#ifdef DEBUG
void
__error__(char *pcFilename, unsigned long ulLine)
{
}
#endif

//*****************************************************************************
//
// The colors that are used to draw on the screen.
//
//*****************************************************************************
static const unsigned long g_pulColors[] =
{
    ClrWhite,
    ClrYellow,
    ClrMagenta,
    ClrRed,
    ClrCyan,
    ClrLime,
    ClrBlue
};

//*****************************************************************************
//
// The index to the current color in use.
//
//*****************************************************************************
static unsigned long g_ulColorIdx;

//*****************************************************************************
//
// The previous pen position returned from the touch screen driver.
//
//*****************************************************************************
static long g_lX, g_lY;

//*****************************************************************************
//
// The drawing context used to draw to the screen.
//
//*****************************************************************************
static tContext g_sContext;

//*****************************************************************************
//
// The interrupt-context handler for touch screen events from the touch screen
// driver.  This function merely bundles up the event parameters and posts
// them to a message queue.  In the context of the main loop, they will be
// read from the queue and handled using TSMainHandler().
//
//*****************************************************************************
int32_t TSHandler(uint32_t ulMessage, int32_t lX, int32_t lY)
{
    tScribbleMessage sMsg;

    //
    // Build the message that we will write to the queue.
    //
    sMsg.ulMsg = ulMessage;
    sMsg.lX = lX;
    sMsg.lY = lY;

    //
    // Make sure the queue isn't full. If it is, we just ignore this message.
    //
    if(!RingBufFull(&g_sMsgQueue))
    {
        RingBufWrite(&g_sMsgQueue, (unsigned char *)&sMsg,
                     sizeof(tScribbleMessage));
    }

    //
    // Tell the touch handler that everything is fine.
    //
    return(1);
}

//*****************************************************************************
//
// The main loop handler for touch screen events from the touch screen driver.
//
//*****************************************************************************
long
TSMainHandler(unsigned long ulMessage, long lX, long lY)
{
    tRectangle sRect;

    //
    // See which event is being sent from the touch screen driver.
    //
    switch(ulMessage)
    {
        //
        // The pen has just been placed down.
        //
        case WIDGET_MSG_PTR_DOWN:
        {
            //
            // Erase the drawing area.
            //
            GrContextForegroundSet(&g_sContext, ClrBlack);
            sRect.i16XMin = 1;
            sRect.i16YMin = 45;
            sRect.i16XMax = GrContextDpyWidthGet(&g_sContext) - 2;
            sRect.i16YMax = GrContextDpyHeightGet(&g_sContext) - 2;
            GrRectFill(&g_sContext, &sRect);

            //
            // Flush any cached drawing operations.
            //
            GrFlush(&g_sContext);

            //
            // Set the drawing color to the current pen color.
            //
            GrContextForegroundSet(&g_sContext, g_pulColors[g_ulColorIdx]);

            //
            // Save the current position.
            //
            g_lX = lX;
            g_lY = lY;

            //
            // This event has been handled.
            //
            break;
        }

        //
        // Then pen has moved.
        //
        case WIDGET_MSG_PTR_MOVE:
        {
            //
            // Draw a line from the previous position to the current position.
            //
            GrLineDraw(&g_sContext, g_lX, g_lY, lX, lY);

            //
            // Flush any cached drawing operations.
            //
            GrFlush(&g_sContext);

            //
            // Save the current position.
            //
            g_lX = lX;
            g_lY = lY;

            //
            // This event has been handled.
            //
            break;
        }

        //
        // The pen has just been picked up.
        //
        case WIDGET_MSG_PTR_UP:
        {
            //
            // Draw a line from the previous position to the current position.
            //
            GrLineDraw(&g_sContext, g_lX, g_lY, lX, lY);

            //
            // Flush any cached drawing operations.
            //
            GrFlush(&g_sContext);

            //
            // Increment to the next drawing color.
            //
            g_ulColorIdx++;
            if(g_ulColorIdx == (sizeof(g_pulColors) / sizeof(g_pulColors[0])))
            {
                g_ulColorIdx = 0;
            }

            //
            // This event has been handled.
            //
            break;
        }
    }

    //
    // Success.
    //
    return(1);
}

//*****************************************************************************
//
// This function is called in the context of the main loop to process any
// touch screen messages that have been sent.  Messages are posted to a
// queue from the message handler and pulled off here.  This is required
// since it is not safe to have two different execution contexts performing
// graphics operations using the same graphics context.
//
//*****************************************************************************
void
ProcessTouchMessages(void)
{
    tScribbleMessage sMsg;

    while(!RingBufEmpty(&g_sMsgQueue))
    {
        //
        // Get the next message.
        //
        RingBufRead(&g_sMsgQueue, (unsigned char *)&sMsg,
                    sizeof(tScribbleMessage));

        //
        // Dispatch it to the handler.
        //
        TSMainHandler(sMsg.ulMsg, sMsg.lX, sMsg.lY);
    }
}
//*****************************************************************************
//
// This is the handler for this SysTick interrupt.  We use this to provide the
// required timer call to the lwIP stack.
//
//*****************************************************************************
void
SysTickIntHandler(void)
{
}

//*****************************************************************************
//
// Provides a scribble pad using the display on the Intelligent Display Module.
//
//*****************************************************************************
int
main(void)
{
    tRectangle sRect;
	unsigned int X_s, C_g;
    //
    // Set the clocking to run from the PLL.
    //
    SysCtlClockSet(SYSCTL_SYSDIV_2_5 | SYSCTL_USE_PLL | SYSCTL_OSC_MAIN |
                   SYSCTL_XTAL_16MHZ);

    //
    // Set the device pinout appropriately for this board.
    //
 //   PinoutSet();

    //
    // Initialize the display driver.
    //
    Kentec320x240x16_SSD2119Init();

    //
    // Initialize the graphics context.
    //
    GrContextInit(&g_sContext, &g_sKentec320x240x16_SSD2119);

	C_g=0x0000;
	for(X_s=32;X_s<288;)
	{	//for(L_s=0;L_s<5;L_s++)
		{
			GrContextForegroundSet(&g_sContext, C_g);
			GrLineDraw(&g_sContext, X_s, 60, X_s, 90);
			X_s++;
		}
		C_g+=0x010000;
	}
	
	C_g=0x0000;
	for(X_s=32;X_s<288;)
	{	//for(L_s=0;L_s<5;L_s++)
		{
			GrContextForegroundSet(&g_sContext, C_g);
			GrLineDraw(&g_sContext, X_s, 100, X_s, 130);
			X_s++;
		}
		C_g+=0x000100;
	}

	C_g=0x0000;
	for(X_s=32;X_s<288;)
	{	//for(L_s=0;L_s<5;L_s++)
		{
			GrContextForegroundSet(&g_sContext, C_g);
			GrLineDraw(&g_sContext, X_s, 140, X_s, 170);
			X_s++;
		}
		C_g+=0x000001;
	}

	C_g=0x0000;
	for(X_s=32;X_s<288;)
	{	//for(L_s=0;L_s<5;L_s++)
		{
			GrContextForegroundSet(&g_sContext, C_g);
			GrLineDraw(&g_sContext, X_s, 180, X_s, 210);
			X_s++;
		}
		C_g+=0x010101;
	}

    //
    // Fill the top 24 rows of the screen with blue to create the banner.
    //
    sRect.i16XMin = 0;
    sRect.i16YMin = 0;
    sRect.i16XMax = GrContextDpyWidthGet(&g_sContext) - 1;
    sRect.i16YMax = 23;
    GrContextForegroundSet(&g_sContext, ClrDarkBlue);
    GrRectFill(&g_sContext, &sRect);

    //
    // Put a white box around the banner.
    //
    GrContextForegroundSet(&g_sContext, ClrWhite);
    GrRectDraw(&g_sContext, &sRect);

    //
    // Put the application name in the middle of the banner.
    //
    GrContextFontSet(&g_sContext, &g_sFontCm20);
    GrStringDrawCentered(&g_sContext, "scribble", -1,
                         GrContextDpyWidthGet(&g_sContext) / 2, 11, 0);

    //
    // Print the instructions across the top of the screen in white with a 20
    // point san-serif font.
    //
    GrContextForegroundSet(&g_sContext, ClrWhite);
    GrContextFontSet(&g_sContext, &g_sFontCmss20);
    GrStringDrawCentered(&g_sContext, "Touch the screen to draw", -1,
                         GrContextDpyWidthGet(&g_sContext) / 2, 34, 0);

    //
    // Draw a green box around the scribble area.
    //
    sRect.i16XMin = 0;
    sRect.i16YMin = 44;
    sRect.i16XMax = GrContextDpyWidthGet(&g_sContext) - 1;
    sRect.i16YMax = GrContextDpyHeightGet(&g_sContext) - 1;
    GrContextForegroundSet(&g_sContext, ClrGreen);
    GrRectDraw(&g_sContext, &sRect);

    //
    // Flush any cached drawing operations.
    //
    GrFlush(&g_sContext);

    //
    // Set the clipping region so that drawing can not occur outside the green
    // box.
    //
    sRect.i16XMin++;
    sRect.i16YMin++;
    sRect.i16XMax--;
    sRect.i16YMax--;
    GrContextClipRegionSet(&g_sContext, &sRect);

    //
    // Set the color index to zero.
    //
    g_ulColorIdx = 0;

    //
    // Initialize the message queue we use to pass messages from the touch
    // interrupt handler context to the main loop for processing.
    //
    RingBufInit(&g_sMsgQueue, (unsigned char *)g_psMsgQueueBuffer,
                (MSG_QUEUE_SIZE * sizeof(tScribbleMessage)));

    //
    // Initialize the touch screen driver.
    //
    TouchScreenInit();

    //
    // Set the touch screen event handler.
    //
    TouchScreenCallbackSet(TSHandler);

    //
    // Loop forever.  All the drawing is done in the touch screen event
    // handler.
    //
    while(1)
    {
        //
        // Process any new touchscreen messages.
        //
        ProcessTouchMessages();
    }
}
