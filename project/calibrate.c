//*****************************************************************************
//
//	EB-LM4F120-L35 touch screen calibrate
//
//*****************************************************************************

#include "utils/ustdlib.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "driverlib/debug.h"
#include "driverlib/fpu.h"
#include "driverlib/gpio.h"
#include "driverlib/pin_map.h"
#include "driverlib/rom.h"
#include "driverlib/sysctl.h"
#include "driverlib/systick.h"
#include "driverlib/uart.h"
#include "utils/softssi.h"
#include "utils/uartstdio.h"

#include "inc/hw_nvic.h"
#include "inc/hw_sysctl.h"
#include "driverlib/flash.h"
#include "driverlib/rom.h"
#include "grlib/grlib.h"
#include "Kentec320x240x16_ssd2119_8bit.h"
#include "touch.h"

//*****************************************************************************
//
// The graphics context used by the application.
//
//*****************************************************************************
tContext g_sContext;

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
// Performs calibration of the touch screen.
//
//*****************************************************************************
int
main(void)
{
    long lIdx, lX1, lY1, lX2, lY2, lCount;
    long pplPoints[3][4];
    char pcBuffer[32];
    tRectangle sRect;

    //
    // The FPU should be enabled because some compilers will use floating-
    // point registers, even for non-floating-point code.  If the FPU is not
    // enabled this will cause a fault.  This also ensures that floating-
    // point operations could be added to this application and would work
    // correctly and use the hardware floating-point unit.  Finally, lazy
    // stacking is enabled for interrupt handlers.  This allows floating-
    // point instructions to be used within interrupt handlers, but at the
    // expense of extra stack usage.
    //
    FPUEnable();
    FPULazyStackingEnable();

    //
    // Set the clocking to run directly from the crystal.
    //
    SysCtlClockSet(SYSCTL_SYSDIV_4 | SYSCTL_USE_PLL | SYSCTL_XTAL_16MHZ |
                       SYSCTL_OSC_MAIN);

    //
    // Set the device pinout appropriately for this board.
    //
//    PinoutSet();

    //
    // Initialize the display driver.
    //
    Kentec320x240x16_SSD2119Init();

    //
    // Initialize the graphics context.
    //
    GrContextInit(&g_sContext, &g_sKentec320x240x16_SSD2119);

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
    GrStringDrawCentered(&g_sContext, "calibrate", -1,
                         GrContextDpyWidthGet(&g_sContext) / 2, 11, 0);

    //
    // Print the instructions across the middle of the screen in white with a
    // 20 point small-caps font.
    //
    GrContextForegroundSet(&g_sContext, ClrWhite);
    GrContextFontSet(&g_sContext, &g_sFontCmsc20);
    GrStringDraw(&g_sContext, "Touch the box", -1, 0,
                 (GrContextDpyHeightGet(&g_sContext) / 2) - 10, 0);

    //
    // Set the points used for calibration based on the size of the screen.
    //
    pplPoints[0][0] = GrContextDpyWidthGet(&g_sContext) / 10;
    pplPoints[0][1] = (GrContextDpyHeightGet(&g_sContext) * 2)/ 10;
    pplPoints[1][0] = GrContextDpyWidthGet(&g_sContext) / 2;
    pplPoints[1][1] = (GrContextDpyHeightGet(&g_sContext) * 9) / 10;
    pplPoints[2][0] = (GrContextDpyWidthGet(&g_sContext) * 9) / 10;
    pplPoints[2][1] = GrContextDpyHeightGet(&g_sContext) / 2;

    //
    // Initialize the touch screen driver.
    //
    TouchScreenInit();

    //
    // Loop through the calibration points.
    //
    for(lIdx = 0; lIdx < 3; lIdx++)
    {
        //
        // Fill a white box around the calibration point.
        //
        GrContextForegroundSet(&g_sContext, ClrWhite);
        GrFlush(&g_sContext);
    	SysCtlDelay(10000000);



        sRect.i16XMin = pplPoints[lIdx][0] - 5;
        sRect.i16YMin = pplPoints[lIdx][1] - 5;
        sRect.i16XMax = pplPoints[lIdx][0] + 5;
        sRect.i16YMax = pplPoints[lIdx][1] + 5;
        GrRectFill(&g_sContext, &sRect);

        //
        // Flush any cached drawing operations.
        //
        GrFlush(&g_sContext);

        //
        // Initialize the raw sample accumulators and the sample count.
        //
        lX1 = 0;
        lY1 = 0;
        lCount = -5;

        //
        // Loop forever.  This loop is explicitly broken out of when the pen is
        // lifted.
        //
        while(1)
        {
            //
            // Grab the current raw touch screen position.
            //
            lX2 = g_sTouchX;
            lY2 = g_sTouchY;

            //
            // See if the pen is up or down.
            //
            if((lX2 < g_sTouchMin) || (lY2 < g_sTouchMin))
            {
                //
                // The pen is up, so see if any samples have been accumulated.
                //
                if(lCount > 0)
                {
                    //
                    // The pen has just been lifted from the screen, so break
                    // out of the controlling while loop.
                    //
                    break;
                }

                //
                // Reset the accumulators and sample count.
                //
                lX1 = 0;
                lY1 = 0;
                lCount = -5;

                //
                // Grab the next sample.
                //
                continue;
            }

            //
            // Increment the count of samples.
            //
            lCount++;

            //
            // If the sample count is greater than zero, add this sample to the
            // accumulators.
            //
            if(lCount > 0)
            {
                lX1 += lX2;
                lY1 += lY2;
            }
        }

        //
        // Save the averaged raw ADC reading for this calibration point.
        //
        pplPoints[lIdx][2] = lX1 / lCount;
        pplPoints[lIdx][3] = lY1 / lCount;

        //
        // Erase the box around this calibration point.
        //
        GrContextForegroundSet(&g_sContext, ClrBlack);
        GrRectFill(&g_sContext, &sRect);
    }

    //
    // Clear the screen.
    //
    sRect.i16XMin = 0;
    sRect.i16YMin = 24;
    sRect.i16XMax = GrContextDpyWidthGet(&g_sContext) - 1;
    sRect.i16YMax = GrContextDpyHeightGet(&g_sContext) - 1;
    GrRectFill(&g_sContext, &sRect);

    //
    // Indicate that the calibration data is being displayed.
    //
    GrContextForegroundSet(&g_sContext, ClrWhite);
    GrStringDraw(&g_sContext, "Calibration data:", -1, 0, 40, 0);

    //
    // Compute and display the M0 calibration value.
    //
    usprintf(pcBuffer, "M0 = %d",
             (((pplPoints[0][0] - pplPoints[2][0]) *
               (pplPoints[1][3] - pplPoints[2][3])) -
              ((pplPoints[1][0] - pplPoints[2][0]) *
               (pplPoints[0][3] - pplPoints[2][3]))));
    GrStringDraw(&g_sContext, pcBuffer, -1, 0, 80, 0);

    //
    // Compute and display the M1 calibration value.
    //
    usprintf(pcBuffer, "M1 = %d",
             (((pplPoints[0][2] - pplPoints[2][2]) *
               (pplPoints[1][0] - pplPoints[2][0])) -
              ((pplPoints[0][0] - pplPoints[2][0]) *
               (pplPoints[1][2] - pplPoints[2][2]))));
    GrStringDraw(&g_sContext, pcBuffer, -1, 0, 100, 0);

    //
    // Compute and display the M2 calibration value.
    //
    usprintf(pcBuffer, "M2 = %d",
             ((((pplPoints[2][2] * pplPoints[1][0]) -
                (pplPoints[1][2] * pplPoints[2][0])) * pplPoints[0][3]) +
              (((pplPoints[0][2] * pplPoints[2][0]) -
                (pplPoints[2][2] * pplPoints[0][0])) * pplPoints[1][3]) +
              (((pplPoints[1][2] * pplPoints[0][0]) -
                (pplPoints[0][2] * pplPoints[1][0])) * pplPoints[2][3])));
    GrStringDraw(&g_sContext, pcBuffer, -1, 0, 120, 0);

    //
    // Compute and display the M3 calibration value.
    //
    usprintf(pcBuffer, "M3 = %d",
             (((pplPoints[0][1] - pplPoints[2][1]) *
               (pplPoints[1][3] - pplPoints[2][3])) -
              ((pplPoints[1][1] - pplPoints[2][1]) *
               (pplPoints[0][3] - pplPoints[2][3]))));
    GrStringDraw(&g_sContext, pcBuffer, -1, 0, 140, 0);

    //
    // Compute and display the M4 calibration value.
    //
    usprintf(pcBuffer, "M4 = %d",
             (((pplPoints[0][2] - pplPoints[2][2]) *
               (pplPoints[1][1] - pplPoints[2][1])) -
              ((pplPoints[0][1] - pplPoints[2][1]) *
               (pplPoints[1][2] - pplPoints[2][2]))));
    GrStringDraw(&g_sContext, pcBuffer, -1, 0, 160, 0);

    //
    // Compute and display the M5 calibration value.
    //
    usprintf(pcBuffer, "M5 = %d",
             ((((pplPoints[2][2] * pplPoints[1][1]) -
                (pplPoints[1][2] * pplPoints[2][1])) * pplPoints[0][3]) +
              (((pplPoints[0][2] * pplPoints[2][1]) -
                (pplPoints[2][2] * pplPoints[0][1])) * pplPoints[1][3]) +
              (((pplPoints[1][2] * pplPoints[0][1]) -
                (pplPoints[0][2] * pplPoints[1][1])) * pplPoints[2][3])));
    GrStringDraw(&g_sContext, pcBuffer, -1, 0, 180, 0);

    //
    // Compute and display the M6 calibration value.
    //
    usprintf(pcBuffer, "M6 = %d",
             (((pplPoints[0][2] - pplPoints[2][2]) *
               (pplPoints[1][3] - pplPoints[2][3])) -
              ((pplPoints[1][2] - pplPoints[2][2]) *
               (pplPoints[0][3] - pplPoints[2][3]))));
    GrStringDraw(&g_sContext, pcBuffer, -1, 0, 200, 0);

    //
    // Flush any cached drawing operations.
    //
    GrFlush(&g_sContext);

    //
    // The calibration is complete.  Sit around and wait for a reset.
    //
    while(1)
    {
    }
}
