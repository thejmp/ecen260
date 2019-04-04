#include "msp.h"
#include "math.h"

#define DATAPIN BIT4
#define CE  BIT0    /* P6.0 chip select */
#define RESET BIT6  /* P6.6 reset */
#define DC BIT7     /* P6.7 register select */

/* define the pixel size of display */
#define GLCD_WIDTH  84
#define GLCD_HEIGHT 48

int t2 = 0;
unsigned char volatile timeout;
unsigned char Packet[5];
unsigned short tempc;
unsigned short humid;
int pass = 0;

void initTime1(void);
void initTime2(void);
unsigned char check_Response();
void start_signal(void);
unsigned char read_Packet(unsigned char * data);
unsigned char check_Checksum(unsigned char *data);
unsigned concatenate(unsigned x, unsigned y);
void GLCD_setCursor(unsigned char x, unsigned char y);
void GLCD_clear(void);
void GLCD_init(void);
void GLCD_data_write(unsigned char data);
void GLCD_command_write(unsigned char data);
void GLCD_putchar(int c);
void SPI_init(void);
void SPI_write(unsigned char data);
void displayHT(unsigned short humid, unsigned short tempc);
void ADC_init();

void TA0_0_IRQHandler(void){
    TIMER_A0->CCTL[0] &= ~BIT0;
    t2++;
}

void TA1_0_IRQHandler(void){
    TIMER_A1->CCTL[0] &= ~BIT0;
    timeout = 1;
}

const char font_table[][6] = {
                              {0x3C, 0x42, 0x81, 0x42, 0x3C, 0x00},  /* 0 */
                              {0x80, 0x82, 0xff, 0x80, 0x80, 0x00},  /* 1 */
                              {0xE1, 0x91, 0x91, 0x91, 0x8F, 0x00},  /* 2 */
                              {0x81, 0x91, 0x91, 0x91, 0x6E, 0x00},  /* 3 */
                              {0x0F, 0x08, 0x08, 0x08, 0xFF, 0x00},  /* 4 */
                              {0x8F, 0x91, 0x91, 0x91, 0xE1, 0x00},  /* 5 */
                              {0x7E, 0x91, 0x91, 0x91, 0x61, 0x00},  /* 6 */
                              {0x01, 0x01, 0x01, 0x01, 0xFE, 0x00},  /* 7 */
                              {0x76, 0x89, 0x89, 0x89, 0x76, 0x00},  /* 8 */
                              {0x0E, 0x11, 0x11, 0x11, 0xFE, 0x00},  /* 9 */
                              {0x02, 0x05, 0x02, 0x00, 0x00, 0x00},  /*  */
                              {0x00, 0x40, 0xA0, 0x40, 0x00, 0x00},  /* . */
                              {0x88, 0x40, 0x20, 0x10, 0x88, 0x00},  /* % */
                              {0x00, 0x00, 0x00, 0x00, 0x00, 0x00},  /* ° */
                              {0x3e, 0x41, 0x41, 0x41, 0x22, 0x00},  /* C */

};
/**
 * main.c
 */
void main(void)
{
	WDT_A->CTL = WDT_A_CTL_PW | WDT_A_CTL_HOLD;		// stop watchdog timer
	initTime1();
	initTime2();
	_enable_interrupts();
	GLCD_init();    /* initialize the GLCD controller */
	GLCD_clear();   /* clear display and  home the cursor */
	ADC_init();
	while (1) {
	    if (t2 >= 12){
	        read_Packet(Packet);
            if (check_Checksum(Packet))
                pass = 1;
            else
                pass = 0;
            humid = (Packet[0]<<8) | Packet[1];
            tempc = (Packet[2]<<8) | Packet[3];
            ADC14->CTL0 |= 1; //start a conversion
            while (!ADC14->IFGR0); //wait until conversion is complete
            float result = ADC14->MEM[5]; //read conversion result
            float rv = result / 4095 * 5;

            GLCD_clear();
            displayHT(humid, tempc);
            t2 = 0;
            pass = 0;
	    }
	}
}

void ADC_init() {
    P2->SEL0 &= 0x0; //set sel0 and sel1 to 0 for digital I/O
        P2->SEL1 &= 0x0;
        P2->DIR |= (BIT0|BIT1|BIT2); //set bits as output

        ADC14->CTL0 = 0x00000010; //power on ADC and disable during config
        ADC14->CTL0 |= 0x04080310; //ctl0 according to lab
        ADC14->CTL1 = 0x00000020; //12bit resolution
        ADC14->MCTL[5] = 0x06; //A6 input, single ended vref = AVCC
        P4->SEL1 |= 0x80; //config P4.7 for a6
        P4->SEL0 |= 0x80;
        ADC14->CTL1 |= 0x00050000; //configure for memory register 5
        ADC14->CTL0 |= 0x02; //Enable ADC14 after config
}

void displayHT(unsigned short humid, unsigned short tempc) {
    short back[5] = {0, 0, 0, 0, 0};
    int i = 4;
    short disp = 0;
    for(i = 4; humid != 0; i--){
        back[i] = humid % 10;
        humid = (humid - (humid % 10))/10;
    }
    for (i = 0; i < 5; i++){
        if (back[i] == 0 && disp != 0){
            GLCD_putchar(back[i]);
            back[i] = 0;
        } else if (back[i] != 0) {
            GLCD_putchar(back[i]);
            disp = 1;
            back[i] = 0;
        }
        if (i == 3)
            GLCD_putchar(11);
    }
    GLCD_putchar(12);
    GLCD_putchar(13);
    disp = 0;
    for(i = 4; tempc != 0; i--){
        back[i] = tempc % 10;
        tempc = (tempc - (tempc % 10))/10;
    }
    for (i = 0; i < 5; i++){
            if (back[i] == 0 && disp != 0){
                GLCD_putchar(back[i]);
            } else if (back[i] != 0) {
                GLCD_putchar(back[i]);
                disp = 1;
            }
            if (i == 3)
                GLCD_putchar(11);
        }
    GLCD_putchar(10);
    GLCD_putchar(14);
}

void start_signal(void)
{
    P2->DIR |= DATAPIN; //sets P2.0 as output
    P2->OUT &= ~DATAPIN; //sets P2.0 to low
    __delay_cycles(25000); // Low for at least 18ms
    P2->OUT |= DATAPIN; //sets P2.0 to high
    __delay_cycles(30);     // High for at 20us-40us
    P2->DIR &= ~DATAPIN; //sets P2.0 as input
}

unsigned char check_Response(){
    timeout = 0;
    TIMER_A1->CTL |= (BIT2 | BIT4);
    TIMER_A1->CCR[0] = 50;
    TIMER_A1->CCTL[0] |= BIT4;
    while(!(P2->IN & DATAPIN) && !timeout);
        if (timeout)
            return 0;
        else {
            TIMER_A1->CTL |= BIT2;
            TIMER_A1->CCTL[0] |= BIT4;
            timeout = 0;
            while((P2->IN & DATAPIN) && !timeout);
            if(timeout)
                return 0;
            else{
                TIMER_A1->CCTL[0] &= ~BIT4;  // Disable timer interrupt
                return 1;
            }
        }
}

unsigned char read_Byte(){
    timeout = 0;
    unsigned char num = 0;
    unsigned char i;
    TIMER_A1->CCTL[0] &= ~BIT4;
    for (i=8; i>0; i--){
        while(!(P2->IN & DATAPIN)); //Wait for signal to go high
        TIMER_A1->CTL |= BIT2; //Reset timer
        TIMER_A1->CTL |= BIT4; //Reenable timer
        TIMER_A1->CCTL[0] |= BIT4; //Enable timer interrupt
        while(P2->IN & DATAPIN); //Wait for signal to go low
        TIMER_A1->CTL &= ~(BIT3 | BIT4); //Halt Timer
        if (TIMER_A1->R > 4)    //40 @ 1x divider
            num |= 1 << (i-1);
    }
    return num;
}

unsigned char check_Checksum(unsigned char *data){
    if (data[4] != (data[0] + data[1] + data[2] + data[3])){
        return 0;
    }
    else return 1;
}

unsigned char read_Packet(unsigned char * data)
{
    start_signal();
    if (check_Response()){
        //Cannot be done with a for loop!
        data[0] = read_Byte();
        data[1] = read_Byte();
        data[2] = read_Byte();
        data[3] = read_Byte();
        data[4] = read_Byte();
        return 1;
    }
    else return 0;
}

void initTime1(void){
    /*
     *  bits9,8=10 set clock source to SMCLK
     * bits7,6=10 input clock divider /4 presceler
     * bits5,4=00 stops the clock
     * bit2=0     clear the timer
     * bit1=0     no interrupt
     */
    TIMER_A0->CTL = (BIT7 | BIT9);
     /*
      * bits15,14=00 no capture mode
      * bit8=0       capare mode (peraotic interupter)
      * bit4=1       enable capture/compare interrupt on CCIFG
      * bit0=0       clear capture/compare interrupt flag
      */
     TIMER_A0->CCTL[0] = BIT4;
     TIMER_A0->CCR[0] = 0xffff;
     //TIMER_A0->EX0 = 0x6;
     NVIC->ISER[0] = BIT8;
     TIMER_A0->CTL |= (BIT2 | BIT4);
}

void initTime2(void){
    TIMER_A1->CTL = (BIT7 | BIT9);
    TIMER_A1->CCTL[0] = BIT4;
    TIMER_A1->EX0 = 0x5;
    NVIC->ISER[0] = 0x400;
}

void GLCD_putchar(int c)
{
    int i;
    for(i = 0; i < 6; i++)
        GLCD_data_write(font_table[c][i]);
}

void GLCD_setCursor(unsigned char x, unsigned char y)
{
    GLCD_command_write(0x80 | x); /* column */
    GLCD_command_write(0x40 | y); /* bank (8 rows per bank) */
}

/* clears the GLCD by writing zeros to the entire screen */
void GLCD_clear(void)
{
    int32_t index;
    for(index = 0; index < (GLCD_WIDTH * GLCD_HEIGHT / 8); index++)
        GLCD_data_write(0x00);
    GLCD_setCursor(0, 0); /* return to the home position */
}

/* send the initialization commands to PCD8544 GLCD controller */
void GLCD_init(void)
{
    SPI_init();
    /* hardware reset of GLCD controller */
    P6->OUT |= RESET;   /* deasssert reset */

    GLCD_command_write(0x21);   /* set extended command mode */
    GLCD_command_write(0xB8);   /* set LCD Vop for contrast */
    GLCD_command_write(0x04);   /* set temp coefficient */
    GLCD_command_write(0x14);   /* set LCD bias mode 1:48 */
    GLCD_command_write(0x20);   /* set normal command mode */
    GLCD_command_write(0x0C);   /* set display normal mode */
}

/* write to GLCD controller data register */
void GLCD_data_write(unsigned char data)
{
    P6->OUT |= DC;              /* select data register */
    SPI_write(data);            /* send data via SPI */
}

/* write to GLCD controller command register */
void GLCD_command_write(unsigned char data)
{
    P6->OUT &= ~DC;             /* select command register */
    SPI_write(data);            /* send data via SPI */
}

void SPI_init(void)
{
    EUSCI_B0->CTLW0 = 0x0001;   /* put UCB0 in reset mode */
    EUSCI_B0->CTLW0 = 0x69C1;   /* PH=0, PL=1, MSB first, Master, SPI, SMCLK */
    EUSCI_B0->BRW = 3;          /* 3 MHz / 3 = 1MHz */
    EUSCI_B0->CTLW0 &= ~0x001;   /* enable UCB0 after config */

    P1->SEL0 |= 0x60;           /* P1.5, P1.6 for UCB0 */
    P1->SEL1 &= ~0x60;

    P6->DIR |= (CE | RESET | DC); /* P6.7, P6.6, P6.0 set as output */
    P6->OUT |= CE;              /* CE idle high */
    P6->OUT &= ~RESET;          /* assert reset */
}

void SPI_write(unsigned char data)
{
    P6->OUT &= ~CE;             /* assert /CE */
    EUSCI_B0->TXBUF = data;     /* write data */
    while(EUSCI_B0->STATW & 0x01);/* wait for transmit done */
    P6->OUT |= CE;              /* deassert /CE */
}
