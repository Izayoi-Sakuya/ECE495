#include <stdarg.h>
#include "stdint.h"
#include "stdbool.h"
#include "time.h"
#include "math.h"
#include "inc/hw_gpio.h"
#include "inc/hw_memmap.h"
#include "inc/hw_nvic.h"
#include "inc/hw_sysctl.h"
#include "inc/hw_types.h"
#include "inc/hw_i2c.h"
#include "inc/hw_ints.h"
#include "driverlib/fpu.h"
#include "driverlib/gpio.h"
#include "driverlib/flash.h"
#include "driverlib/sysctl.h"
#include "driverlib/systick.h"
#include "driverlib/uart.h"
#include "driverlib/udma.h"
#include "driverlib/rom.h"
#include "driverlib/timer.h"
#include "driverlib/interrupt.h"
#include "driverlib/adc.h"
#include "driverlib/pin_map.h"
#include "driverlib/i2c.h"
#include "grlib/grlib.h"
#include "grlib/widget.h"
#include "grlib/canvas.h"
#include "grlib/pushbutton.h"
#include "grlib/slider.h"
#include "utils/ustdlib.h"
#include "Kentec320x240x16_ssd2119_8bit.h"
#include "touch.h"
#define VUrate 3
#define VUfreq 100
#define PI 3.1415926
#define BufferSize VUfreq/VUrate

// The DMA control structure table.
#ifdef ewarm
#pragma data_alignment=1024
tDMAControlTable sDMAControlTable[64];
#elif defined(ccs)
#pragma DATA_ALIGN(sDMAControlTable, 1024)
tDMAControlTable sDMAControlTable[64];
#else
tDMAControlTable sDMAControlTable[64] __attribute__ ((aligned(1024)));
#endif

typedef struct{
	uint8_t bass;
	uint8_t mid;
	uint8_t treble;
	uint8_t Vol;
    int balance;
}Equalizer;

// fcn
void goVU(tWidget *pWidget);
void goEQ(tWidget *pWidget);
void goStart(tWidget *pWidget);
void PnMain(tWidget *pWidget, tContext *pContext);
void SwitchPanel();
void AdjustEQ(tWidget *pWidget, int32_t lValue);
void UpdateEQ();
void UpdateSliders();
void VUisr();
void SampleVU();
void UpdateVU();
void i2cSend(int,int,int);

// var
extern tCanvasWidget PanelList[];
int8_t Balance;
int8_t PanelNum;
int8_t SliderNum;
int8_t EQparam1,EQparam2;
int8_t VUparam1,VUparam2;
uint16_t VUct;
uint16_t FIFOptr = 0;
uint32_t ADCCache[2];
uint32_t ADCFIFO[BufferSize];
uint32_t g_ulButtonState;
tContext sContext;
tRectangle sRect;
Equalizer EQ;
float balfact_l,balfact_r;
double VUlog;
double VU;

// The Title panel, which does exactly what you think it does.
Canvas(g_sMain, PanelList, 0, 0, &g_sKentec320x240x16_SSD2119, 0,
       24, 320, 166, CANVAS_STYLE_APP_DRAWN, 0, 0, 0, 0, 0, 0, PnMain);

// The VU meter panel, which contains a VU meter
Canvas(g_sVU, PanelList+1, 0, 0,
       &g_sKentec320x240x16_SSD2119, 60, 30, 200, 40,
       CANVAS_STYLE_TEXT, 0, 0, ClrBlack,
       &g_sFontCm18, "", 0, 0);

// The Equalizer panel, which contains an equalizer
Canvas(g_sEQ, PanelList+2, 0, 0,
       &g_sKentec320x240x16_SSD2119, 210, 30, 60, 40,
       CANVAS_STYLE_TEXT, 0, 0, ClrBlack,
       &g_sFontCm18, " Balance ", 0, 0);
Canvas(g_sEQ1, PanelList+2, &g_sEQ, 0,
       &g_sKentec320x240x16_SSD2119, 5, 150, 50, 40,
       CANVAS_STYLE_TEXT, 0, 0, ClrBlack,
       &g_sFontCm18, " Bass ", 0, 0);
Canvas(g_sEQ2, PanelList+2, &g_sEQ1, 0,
       &g_sKentec320x240x16_SSD2119, 55, 150, 50, 40,
       CANVAS_STYLE_TEXT, 0, 0, ClrBlack,
       &g_sFontCm18, " Mid ", 0, 0);
Canvas(g_sEQ3, PanelList+2, &g_sEQ2, 0,
       &g_sKentec320x240x16_SSD2119, 105, 150, 50, 40,
       CANVAS_STYLE_TEXT, 0, 0, ClrBlack,
       &g_sFontCm18, " Treble ", 0, 0);
Canvas(g_sEQ4, PanelList+2, &g_sEQ3, 0,
       &g_sKentec320x240x16_SSD2119, 210, 140, 60, 40,
       CANVAS_STYLE_TEXT, 0, 0, ClrBlack,
       &g_sFontCm18, " Volume ", 0, 0);

// The names for each of the panels, which is displayed at the bottom of the
// screen.
char* PanelNames[4] =
{
    "1",
    "Hi-Fi Audio Amplifier",
    "VU Meter",
    "Equalizer",
};

// The buttons and text across the bottom of the screen.
Canvas(g_sTitle, 0, 0, 0, &g_sKentec320x240x16_SSD2119, 50, 190, 220, 50,
       CANVAS_STYLE_TEXT | CANVAS_STYLE_TEXT_OPAQUE, 0, 0, ClrSilver,
       &g_sFontCm20, 0, 0, 0);
			 
RectangularButton(bVU, 0, 0, 0, &g_sKentec320x240x16_SSD2119, 60, 190,
                  130, 40, PB_STYLE_FILL | PB_STYLE_TEXT | PB_STYLE_OUTLINE, ClrIndigo, ClrIndigo, ClrWhite,
                  ClrWhite, &g_sFontCm20, "VU Meter", 0, 0, 0, 0, goVU);
									
RectangularButton(bEQ, 0, 0, 0, &g_sKentec320x240x16_SSD2119, 190, 190,
                  130, 40, PB_STYLE_FILL | PB_STYLE_TEXT | PB_STYLE_OUTLINE, ClrIndigo, ClrIndigo, ClrWhite,
                  ClrWhite, &g_sFontCm20, "Equalizer", 0 ,0, 0, 0, goEQ);
									
RectangularButton(bStart, 0, 0, 0, &g_sKentec320x240x16_SSD2119, 0, 190,
                  60, 40, PB_STYLE_FILL | PB_STYLE_TEXT | PB_STYLE_OUTLINE, ClrIndigo, ClrIndigo, ClrWhite,
                  ClrWhite, &g_sFontCm20, "Main", 0, 0, 0, 0, goStart);


// sliders for EQ
tSliderWidget SliderList[] =
{
    //Bass
    SliderStruct(PanelList+2, SliderList+1, 0,
              &g_sKentec320x240x16_SSD2119, 10, 40, 35, 110, 0, 255, 127,
              (SL_STYLE_FILL | SL_STYLE_BACKG_FILL | SL_STYLE_OUTLINE |
               SL_STYLE_TEXT | SL_STYLE_BACKG_TEXT | SL_STYLE_VERTICAL),
              ClrGray, ClrBlack, ClrSilver, ClrWhite, ClrWhite,
              &g_sFontCm14, "0", 0, 0, AdjustEQ),
    //Mid
    SliderStruct(PanelList+2, SliderList+2, 0,
            &g_sKentec320x240x16_SSD2119, 60, 40, 35, 110, 0, 255, 127,
            (SL_STYLE_FILL | SL_STYLE_BACKG_FILL | SL_STYLE_OUTLINE |
             SL_STYLE_TEXT | SL_STYLE_BACKG_TEXT | SL_STYLE_VERTICAL),
            ClrGray, ClrBlack, ClrSilver, ClrWhite, ClrWhite,
            &g_sFontCm14, "0", 0, 0, AdjustEQ),
    //Treble
    SliderStruct(PanelList+2, SliderList+3, 0,
            &g_sKentec320x240x16_SSD2119, 110, 40, 35, 110, 0, 255, 127,
            (SL_STYLE_FILL | SL_STYLE_BACKG_FILL | SL_STYLE_OUTLINE |
             SL_STYLE_TEXT | SL_STYLE_BACKG_TEXT | SL_STYLE_VERTICAL),
            ClrGray, ClrBlack, ClrSilver, ClrWhite, ClrWhite,
            &g_sFontCm14, "0", 0, 0, AdjustEQ),
    //Balance
    SliderStruct(PanelList+2, SliderList+4, 0,
            &g_sKentec320x240x16_SSD2119, 160, 70, 150, 25, 0, 255, 127,
            (SL_STYLE_FILL | SL_STYLE_BACKG_FILL | SL_STYLE_OUTLINE |
             SL_STYLE_TEXT | SL_STYLE_BACKG_TEXT),
            ClrGray, ClrBlack, ClrSilver, ClrWhite, ClrWhite,
            &g_sFontCm14, "100%", 0, 0, AdjustEQ),
    //Volume
    SliderStruct(PanelList+2, &g_sEQ4, 0,
            &g_sKentec320x240x16_SSD2119, 160, 110, 150, 25, 0, 255, 127,
            (SL_STYLE_FILL | SL_STYLE_BACKG_FILL | SL_STYLE_OUTLINE |
             SL_STYLE_TEXT | SL_STYLE_BACKG_TEXT),
            ClrGray, ClrBlack, ClrSilver, ClrWhite, ClrWhite,
            &g_sFontCm14, "100%", 0, 0, AdjustEQ),
};

tSliderWidget vum = SliderStruct(PanelList+1, 0, 0,
                         &g_sKentec320x240x16_SSD2119, 35, 70, 250, 75, 0, 255, 0,
                         (SL_STYLE_FILL | SL_STYLE_BACKG_FILL | SL_STYLE_OUTLINE |
                          SL_STYLE_TEXT | SL_STYLE_BACKG_TEXT | SL_STYLE_LOCKED),
                         ClrGray, ClrBlack, ClrSilver, ClrWhite, ClrWhite,
                         &g_sFontCm14, 0, 0, 0, 0);

// Panels
tCanvasWidget PanelList[] = {
    CanvasStruct(0, 0, &g_sMain, &g_sKentec320x240x16_SSD2119, 0, 24,
                          320, 166, CANVAS_STYLE_FILL, ClrGainsboro, 0, 0, 0, 0, 0, 0),
    CanvasStruct(0, 0, &vum, &g_sKentec320x240x16_SSD2119, 0, 24,
                             320, 166, CANVAS_STYLE_FILL, ClrGainsboro, 0, 0, 0, 0, 0, 0),
    CanvasStruct(0, 0, &SliderList, &g_sKentec320x240x16_SSD2119, 0, 24,
                             320, 166, CANVAS_STYLE_FILL, ClrGainsboro, 0, 0, 0, 0, 0, 0)
};


int main(void)
{
    FPUEnable();
    FPULazyStackingEnable();

    // Set the clock to 40Mhz derived from the PLL and the external oscillator
    SysCtlClockSet(SYSCTL_SYSDIV_6 | SYSCTL_USE_PLL | SYSCTL_XTAL_16MHZ |
                       SYSCTL_OSC_MAIN);

    // Initialize the display driver.
    Kentec320x240x16_SSD2119Init();

    // Initialize the graphics context.
    GrContextInit(&sContext, &g_sKentec320x240x16_SSD2119);

    // Fill the top 24 rows of the screen with blue to create the banner.
    sRect.i16XMin = 0;
    sRect.i16YMin = 0;
    sRect.i16XMax = GrContextDpyWidthGet(&sContext) - 1;
    sRect.i16YMax = 23;
    GrContextForegroundSet(&sContext, ClrDarkSlateBlue);
    GrRectFill(&sContext, &sRect);

    // Put a white box around the banner.
    GrContextForegroundSet(&sContext, ClrWhite);
    GrRectDraw(&sContext, &sRect);

    // Put the application name in the middle of the banner.
    GrContextFontSet(&sContext, &g_sFontCm20);
    GrStringDrawCentered(&sContext, "Hi-Fi Audio Amplifier", -1, GrContextDpyWidthGet(&sContext) / 2, 8, 0);

    // Configure and enable uDMA
    SysCtlPeripheralEnable(SYSCTL_PERIPH_UDMA);
    SysCtlDelay(10);
    uDMAControlBaseSet(&sDMAControlTable[0]);
    uDMAEnable();

    // Initialize the touch screen driver and have it route its messages to the
    // widget tree.
    TouchScreenInit();
    TouchScreenCallbackSet(WidgetPointerMessage);

    // Add the title block and the previous and next buttons to the widget
    // tree.
    WidgetAdd(WIDGET_ROOT, (tWidget*)&g_sTitle);
    WidgetAdd(WIDGET_ROOT, (tWidget*)&bEQ);
    WidgetAdd(WIDGET_ROOT, (tWidget*)&bStart);
	WidgetAdd(WIDGET_ROOT, (tWidget*)&bVU);

	// I2C init
	SysCtlPeripheralEnable(SYSCTL_PERIPH_I2C2);
	SysCtlPeripheralReset(SYSCTL_PERIPH_I2C2);
	SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOE);
	GPIODirModeSet(GPIO_PORTE_BASE, GPIO_PIN_4 | GPIO_PIN_5, GPIO_DIR_MODE_OUT);
	while(!SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOE))
	{
	}
	GPIOPinConfigure(GPIO_PE4_I2C2SCL);
	GPIOPinConfigure(GPIO_PE5_I2C2SDA);
	GPIOPadConfigSet(GPIO_PORTE_BASE, GPIO_PIN_4 | GPIO_PIN_5, GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_OD);
	GPIOPinTypeI2CSCL(GPIO_PORTE_BASE, (uint8_t)GPIO_PIN_4);
	GPIOPinTypeI2C(GPIO_PORTE_BASE, (uint8_t)GPIO_PIN_5);
	while(!SysCtlPeripheralReady(SYSCTL_PERIPH_I2C2))
	{
	};
	I2CMasterEnable(I2C2_BASE);
	I2CMasterInitExpClk(I2C2_BASE, SysCtlClockGet(), false);

	// TIM init
	int period = (SysCtlClockGet() / VUfreq)/ 2;
	SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER2);
	TimerConfigure(TIMER2_BASE, TIMER_CFG_PERIODIC);
	TimerIntRegister(TIMER2_BASE, TIMER_A, VUisr);
	TimerEnable(TIMER2_BASE, TIMER_A);
	IntEnable(INT_TIMER2A);
	TimerIntEnable(TIMER2_BASE, TIMER_TIMA_TIMEOUT);
	TimerLoadSet(TIMER2_BASE, TIMER_A, period -1);
	TimerIntDisable(TIMER2_BASE, TIMER_TIMA_TIMEOUT);

	// ADC init
	// D2 L/5 D3 R/4
	SysCtlPeripheralEnable(SYSCTL_PERIPH_ADC1);
	GPIODirModeSet(GPIO_PORTD_BASE, GPIO_PIN_2 | GPIO_PIN_3, GPIO_DIR_MODE_IN);
    GPIOPinTypeADC(GPIO_PORTD_BASE, GPIO_PIN_2);
    GPIOPinTypeADC(GPIO_PORTD_BASE, GPIO_PIN_3);
	ADCSequenceDisable(ADC1_BASE, 1);
	ADCSequenceConfigure(ADC1_BASE, 1, ADC_TRIGGER_PROCESSOR, 0);
	ADCSequenceStepConfigure(ADC1_BASE, 1, 0, ADC_CTL_CH5);
	ADCSequenceStepConfigure(ADC1_BASE, 1, 1, ADC_CTL_CH4 | ADC_CTL_END);
	ADCSequenceEnable(ADC1_BASE, 1);

	// Parameter Initialization
    EQ.bass = 127;
    EQ.mid = 127;
    EQ.treble = 127;
    EQ.Vol = 127;
    EQ.Vol = 128;
    EQ.balance = 128;
    balfact_l= 1;
    balfact_r= 1;

    // Add the first panel to the widget tree.
    PanelNum = 0;
    WidgetAdd(WIDGET_ROOT, (tWidget*)(PanelList+PanelNum));
    CanvasTextSet(&g_sTitle, PanelNames[PanelNum+1]);

    // Issue the initial paint request to the widgets.
    WidgetPaint(WIDGET_ROOT);

    // Loop forever handling widget messages.
    while(1)
    {
        //
        // Process any messages in the widget message queue.
        //
        WidgetMessageQueueProcess();
    }
}

void SwitchPanel(){
    WidgetAdd(WIDGET_ROOT,(tWidget*)(PanelList + PanelNum));
    WidgetPaint((tWidget*)(tWidget*)(PanelList + PanelNum));
    GrContextForegroundSet(&sContext, ClrDarkSlateBlue);
    GrRectFill(&sContext, &sRect);
    GrContextForegroundSet(&sContext, ClrWhite);
    GrRectDraw(&sContext, &sRect);
    GrContextFontSet(&sContext, &g_sFontCm20);
    GrStringDrawCentered(&sContext, PanelNames[PanelNum+1], -1, GrContextDpyWidthGet(&sContext) / 2, 8, 0);
    WidgetAdd(WIDGET_ROOT, (tWidget*)&bEQ);
    WidgetAdd(WIDGET_ROOT, (tWidget*)&bStart);
    WidgetAdd(WIDGET_ROOT, (tWidget*)&bVU);
}

// When sliders are changed, update the values of parameters
void AdjustEQ(tWidget *pWidget, int32_t lValue){
    static char pcSliderText[5];
    if(pWidget == (tWidget*)&SliderList[0]) // Bass
    {
        SliderNum = 1;
        EQ.bass = lValue;
        EQparam1 = ((lValue - 127) *600 / 127) / 100;
        EQparam2 = ((lValue - 127) *600 / 127) % 100;
    }
    else if(pWidget == (tWidget*)&SliderList[1]) // Mid
    {
        SliderNum = 2;
        EQ.mid = lValue;
        EQparam1 = ((lValue - 127) *600 / 127) / 100;
        EQparam2 = ((lValue - 127) *600 / 127) % 100;
    }
    else if(pWidget == (tWidget*)&SliderList[2]) // Treble
    {
        SliderNum = 3;
        EQ.treble = lValue;
        EQparam1 = ((lValue - 127) *600 / 127) / 100;
        EQparam2 = ((lValue - 127) *600 / 127) % 100;
    }
    else if(pWidget == (tWidget*)&SliderList[3]) // Balance
    {
        SliderNum = 4;
        EQ.balance = lValue;
        if (lValue < 127)
        {
            balfact_l = (float)lValue / 127;
            balfact_r = 1;
        }
        else if (lValue > 127)
        {
            balfact_l = 1;
            balfact_r = (float)(255 - lValue) / 127;
        }
        else
        {
            balfact_l = 1;
            balfact_r = 1;
        }
    }
    else if(pWidget == (tWidget*)&SliderList[4]){
    	SliderNum = 5;
    	EQ.Vol = lValue;
    }
    if ((EQparam2 >= 0) && (EQparam1 >= 0) && (SliderNum <= 3)){
        usprintf(pcSliderText, "+%d.%d", EQparam1, EQparam2);
    }
    else if (SliderNum <= 3){
        usprintf(pcSliderText, "-%d.%d", -EQparam1, -EQparam2);
    }
    else if (SliderNum == 4){
        if ((balfact_l == 1) && (balfact_r < 1))
        {
            usprintf(pcSliderText, "%d%%R",(int)(balfact_r*100));
        }
        else if ((balfact_r == 1) && (balfact_l < 1))
        {
            usprintf(pcSliderText, "%d%%L",(int)(balfact_l*100));
        }
        else
        {
            usprintf(pcSliderText, "100%%");
        }
    }
    else if (SliderNum == 5){
    	usprintf(pcSliderText, "%d%%",(int)(EQ.Vol*100/255));
    }
    SliderTextSet(&SliderList[SliderNum - 1], pcSliderText);
    WidgetPaint((tWidget*)&SliderList[SliderNum - 1]);
    UpdateEQ(EQ);
}
// Send data to digipots
void i2cSend(int addr, int data1, int data2){
	I2CMasterSlaveAddrSet(I2C2_BASE, (uint8_t)addr, false);

    I2CMasterControl(I2C2_BASE, I2C_MASTER_CMD_BURST_SEND_START);
    I2CMasterDataPut(I2C2_BASE, (uint8_t)0x00);
    I2CMasterControl(I2C2_BASE, I2C_MASTER_CMD_BURST_SEND_CONT);
    while(I2CMasterBusy(I2C2_BASE)){}
    I2CMasterDataPut(I2C2_BASE, (uint8_t)(data1));
    I2CMasterControl(I2C2_BASE, I2C_MASTER_CMD_BURST_SEND_FINISH);
    while(I2CMasterBusy(I2C2_BASE)){}

    I2CMasterSlaveAddrSet(I2C2_BASE, (uint8_t)addr, false);
    I2CMasterControl(I2C2_BASE, I2C_MASTER_CMD_BURST_SEND_START);
    I2CMasterDataPut(I2C2_BASE, (uint8_t)0x80);
    I2CMasterControl(I2C2_BASE, I2C_MASTER_CMD_BURST_SEND_CONT);
    while(I2CMasterBusy(I2C2_BASE)){}
    I2CMasterDataPut(I2C2_BASE, (uint8_t)(data2));
    I2CMasterControl(I2C2_BASE, I2C_MASTER_CMD_BURST_SEND_FINISH);
    while(I2CMasterBusy(I2C2_BASE)){}
}

void UpdateEQ(){
    // 00: Base L/R; 01: Mid L/R; 10: Treble L/R; 11: Volume L/R

    // Bass 00
	i2cSend(0x2C,EQ.bass,EQ.bass);
    // Mid 01
    i2cSend(0x2D,EQ.mid,EQ.mid);
	// Tre 10
	i2cSend(0x2E,EQ.treble,EQ.treble);
	// Vol 11
	i2cSend(0x2F,EQ.Vol*balfact_l,EQ.Vol*balfact_r);
}

// Functions to switch between panels
void goStart(tWidget *pWidget)
{
    if(PanelNum == 0){
        return;
    }
    WidgetRemove((tWidget*)(PanelList + PanelNum));
    PanelNum = 0;
    TimerIntDisable(TIMER2_BASE, TIMER_TIMA_TIMEOUT);
    SwitchPanel();
}

void goVU(tWidget *pWidget)
{
    static char VUtext[22];
    if(PanelNum == 1){
        return;
    }
    WidgetRemove((tWidget*)(PanelList + PanelNum));
    PanelNum = 1;
    TimerIntEnable(TIMER2_BASE, TIMER_TIMA_TIMEOUT);
    usprintf(VUtext,"Refreshing Rate: %d Hz", VUrate);
    CanvasTextSet(&g_sVU, VUtext);
    SwitchPanel();
    WidgetPaint((tWidget*)&g_sVU);
}

void goEQ(tWidget *pWidget)
{
    if(PanelNum == 2){
        return;
    }
    WidgetRemove((tWidget*)(PanelList + PanelNum));
    PanelNum = 2;
	  TimerIntDisable(TIMER2_BASE, TIMER_TIMA_TIMEOUT);
    SwitchPanel();
    UpdateSliders();
}

// Set the value of Sliders when switching into panel EQ                        NEED CHANGE
void UpdateSliders()
{
    static char pcSliderText1[6];
    static char pcSliderText2[6];
    static char pcSliderText3[6];
    static char pcSliderText4[5];
    static char pcSliderText5[5];
    //Bass
    EQparam1 = ((EQ.bass - 127) *600 / 127) / 100;
    EQparam2 = ((EQ.bass - 127) *600 / 127) % 100;
    if ((EQparam2 >= 0) && (EQparam1 >= 0)){
        usprintf(pcSliderText1, "+%d.%d", EQparam1, EQparam2);
    }
    else{
        usprintf(pcSliderText1, "-%d.%d", -EQparam1, -EQparam2);
    }
    SliderTextSet(&SliderList[0], pcSliderText1);
    WidgetPaint((tWidget*)&SliderList[0]);

    // Mid
    EQparam1 = ((EQ.mid - 127) *600 / 127) / 100;
    EQparam2 = ((EQ.mid - 127) *600 / 127) % 100;
    if ((EQparam2 >= 0) && (EQparam1 >= 0)){
        usprintf(pcSliderText2, "+%d.%d", EQparam1, EQparam2);
    }
    else{
        usprintf(pcSliderText2, "-%d.%d", -EQparam1, -EQparam2);
    }
    SliderTextSet(&SliderList[1], pcSliderText2);
    WidgetPaint((tWidget*)&SliderList[1]);

    // Tre
    EQparam1 = ((EQ.treble - 127) *600 / 127) / 100;
    EQparam2 = ((EQ.treble - 127) *600 / 127) % 100;
    if ((EQparam2 >= 0) && (EQparam1 >= 0)){
        usprintf(pcSliderText3, "+%d.%d", EQparam1, EQparam2);
    }
    else{
        usprintf(pcSliderText3, "-%d.%d", -EQparam1, -EQparam2);
    }
    SliderTextSet(&SliderList[2], pcSliderText3);
    WidgetPaint((tWidget*)&SliderList[2]);

    // Bal
    if ((balfact_l == 1) && (balfact_r < 1))
    {
        usprintf(pcSliderText4, "%d%%R",(int)(balfact_r*100));
    }
    else if ((balfact_r == 1) && (balfact_l < 1))
    {
        usprintf(pcSliderText4, "%d%%L",(int)(balfact_l*100));
    }
    else
    {
        usprintf(pcSliderText4, "100%%");
    }
    SliderTextSet(&SliderList[3], pcSliderText4);
    WidgetPaint((tWidget*)&SliderList[3]);

    // Vol
    usprintf(pcSliderText5, "%d%%",(int)(EQ.Vol*100/255));

    SliderTextSet(&SliderList[4], pcSliderText5);
    WidgetPaint((tWidget*)&SliderList[4]);
}

// Functions for each panel
void PnMain(tWidget* pWidget, tContext* pContext)
{
    GrContextFontSet(pContext, &g_sFontCm18);
    GrContextForegroundSet(pContext, ClrBlack);

    GrStringDraw(pContext, "ECE 49595 ", -1, 80, 80, 0);
    GrStringDraw(pContext, "Sec.14, Team 3", -1, 74, 98, 0);
    GrStringDraw(pContext, "Team Original", -1, 76, 116, 0);
}


// does exactly same as name suggests
void VUisr(){
    TimerIntClear(TIMER2_BASE, TIMER_TIMA_TIMEOUT);
    if (VUct == BufferSize){
        if (PanelNum == 1){
            UpdateVU();
        }
        VUct = 0;
    }
    SampleVU();
    VUct++;
}

// does exactly same as name suggests
void SampleVU(){
    ADCProcessorTrigger(ADC1_BASE, 1);
    while (ADCBusy(ADC1_BASE)){};
    ADCSequenceDataGet(ADC1_BASE, 1, ADCCache);

    if (FIFOptr > (BufferSize - 1)){
        FIFOptr = 0;
    }

    uint8_t i;
    for (i = 0;i < 2;i++){
        ADCFIFO[FIFOptr] = ADCCache[i];
        FIFOptr++;
    }
}

// does exactly same as name suggests
void UpdateVU(){
    double sum;
    uint16_t i;
    static char text[8];
    for (i = 0;i < (BufferSize - 1);i++){
        sum += (double)ADCFIFO[i] / 4095 * 3.3;
    }
    VU = sum / 5 / BufferSize;
    VUlog = 20 * log10(VU / PI * 2.19089023);
    VUparam1 = (int)VUlog;
    if (VUlog < 0){
        VUparam2 = -(VUlog - VUparam1) * 100;
    }
    else {
        VUparam2 = (VUlog - VUparam1) * 100;
    }
    usprintf(text, "%d.%d dB VU", VUparam1, VUparam2);
    SliderTextSet(&vum, text);
	SliderValueSet(&vum, VU * 255 * 2.5);
    WidgetPaint((tWidget*)&vum);
}
