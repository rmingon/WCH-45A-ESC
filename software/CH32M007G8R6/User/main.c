#include "debug.h"

/* Global Variable to store Throttle */
volatile uint16_t PWM_PulseWidth = 0; // Throttle value (approx 1000 to 2000)
volatile uint16_t PWM_Period = 0;     // Frequency check

void TIM2_PWM_Input_Init(void)
{
    TIM_ICInitTypeDef TIM_ICInitStructure = {0};
    GPIO_InitTypeDef GPIO_InitStructure = {0};
    NVIC_InitTypeDef NVIC_InitStructure = {0};

    // 1. Enable Clocks
    // TIM2 is on APB1, GPIOA and AFIO on APB2
    RCC_PB1PeriphClockCmd(RCC_PB1Periph_TIM2, ENABLE);
    RCC_PB2PeriphClockCmd(RCC_PB2Periph_GPIOA | RCC_PB2Periph_AFIO, ENABLE);

    // 2. Configure PA1 as Input
    // We use Pull-Down to ensure throttle is "0" if wire disconnects
    // (User Note: You have an external 10k Pull-Up R14, consider removing it for safety)
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPD; 
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // 3. Pin Remap (CRITICAL)
    // By default, PA1 is TIM1. We must remap to get TIM2_CH2 on PA1.
    // Check your ch32v00x_gpio.h for the exact definition. 
    // It is often GPIO_PartialRemap1_TIM2 or similar.
    // Uncomment the line below if PA1 is not default TIM2 for your specific package:
    // GPIO_PinRemapConfig(GPIO_FullRemap_TIM2, ENABLE); 

    // 4. Timer Base Config
    // Prescaler = 47 -> 48MHz / (47+1) = 1MHz Timer Clock
    // This gives us 1 microsecond (us) precision.
    TIM2->PSC = 47; 
    TIM2->ATRLR = 0xFFFF; // Max period

    // 5. PWM Input Configuration
    // We use Channel 2 (PA1) as the main input
    TIM_ICInitStructure.TIM_Channel = TIM_Channel_2;
    TIM_ICInitStructure.TIM_ICPolarity = TIM_ICPolarity_Rising; // Reset on Rising Edge
    TIM_ICInitStructure.TIM_ICSelection = TIM_ICSelection_DirectTI;
    TIM_ICInitStructure.TIM_ICPrescaler = TIM_ICPSC_DIV1;
    TIM_ICInitStructure.TIM_ICFilter = 0x0; // Add filter (e.g., 0x4) if you have noise
    
    // This function automatically configures Channel 1 to capture the Falling Edge
    TIM_PWMIConfig(TIM2, &TIM_ICInitStructure);

    // 6. Select Input Trigger: TI2FP2 (Channel 2 Filtered)
    TIM_SelectInputTrigger(TIM2, TIM_TS_TI2FP2);

    // 7. Slave Mode: Reset Counter on Rising Edge
    TIM_SelectSlaveMode(TIM2, TIM_SlaveMode_Reset);

    // 8. Enable Master/Slave Mode
    TIM_SelectMasterSlaveMode(TIM2, TIM_MasterSlaveMode_Enable);

    // 9. Enable Interrupts (Optional, if you want to read data in IRQ)
    TIM_ITConfig(TIM2, TIM_IT_CC2, ENABLE);

    NVIC_InitStructure.NVIC_IRQChannel = TIM2_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    // 10. Enable Timer
    TIM_Cmd(TIM2, ENABLE);
}

// Interrupt Handler to Read Data
void TIM2_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void TIM2_IRQHandler(void)
{
    if (TIM_GetITStatus(TIM2, TIM_IT_CC2) != RESET)
    {
        // CCR1 contains the Pulse Width (Falling edge capture)
        // CCR2 contains the Period (Next Rising edge capture)
        PWM_PulseWidth = TIM_GetCapture1(TIM2); 
        PWM_Period = TIM_GetCapture2(TIM2);
        
        TIM_ClearITPendingBit(TIM2, TIM_IT_CC2);
    }
}

int main(void)
{
    SystemInit();
    USART_Printf_Init(115200); // Initialize debug UART if needed
    
    TIM2_PWM_Input_Init();

    while(1)
    {
        // Safety: If PulseWidth is roughly 1000 to 2000, it's valid.
        // If it is 0, signal is lost.
        
        printf("Throttle: %d us\n", PWM_PulseWidth);
        
        // Delay 100ms
        for(volatile int i=0; i<100000; i++); 
    }
}