#include "msp.h"
#include "math.h"

#define DATAPIN BIT4 //used for hum and temp sensor
#define CE  BIT0    /* P6.0 chip select */
#define RESET BIT6  /* P6.6 reset */
#define DC BIT7     /* P6.7 register select */

/* define the pixel size of display */
#define GLCD_WIDTH  84
#define GLCD_HEIGHT 48

int t2 = 0; //one second timer counter
unsigned char volatile timeout; //bool time out var
unsigned char Packet[5]; //data packet from sensor
unsigned short tempc; //temp data var
unsigned short humid; //hum data var
int pass = 0; //checksum var
float light = 0; //bool light var

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
void displayHT(unsigned short humid, unsigned short tempc, short lighton);
void ADC_init();

//timer A0 interrupt for 1s timer
void TA0_0_IRQHandler(void){
    TIMER_A0->CCTL[0] &= ~BIT0; //resets interrupt
    t2++; // plus 1 timer counter
}

//timer A1 interrupt for time out
void TA1_0_IRQHandler(void){
    TIMER_A1->CCTL[0] &= ~BIT0; //resets interrupt
    timeout = 1; //set timeout
}

//font_table
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
                              {0x02, 0x05, 0x02, 0x00, 0x00, 0x00},  /* ° */
                              {0x00, 0x40, 0xA0, 0x40, 0x00, 0x00},  /* . */
                              {0x88, 0x40, 0x20, 0x10, 0x88, 0x00},  /* % */
                              {0x00, 0x00, 0x00, 0x00, 0x00, 0x00},  /*   */
                              {0x3e, 0x41, 0x41, 0x41, 0x22, 0x00},  /* C */
                              {0x7f, 0x40, 0x40, 0x40, 0x40, 0x00},  /* L */
                              {0x41, 0x41, 0x7f, 0x41, 0x41, 0x00},  /* I */
                              {0x3e, 0x41, 0x49, 0x49, 0x7a, 0x00},  /* G */
                              {0x7f, 0x08, 0x08, 0x08, 0x7f, 0x00},  /* H */
                              {0x01, 0x01, 0x7f, 0x01, 0x01, 0x00},  /* T */
                              {0x26, 0x49, 0x49, 0x49, 0x32, 0x00},  /* S */
                              {0x3e, 0x41, 0x41, 0x41, 0x3e, 0x00},  /* O */
                              {0x7f, 0x04, 0x08, 0x10, 0x7f, 0x00},   /* N */
                              {0x7f, 0x09, 0x09, 0x09, 0x01, 0x00}  /* F */

};
/**
 * main.c
 */
void main(void)
{
	WDT_A->CTL = WDT_A_CTL_PW | WDT_A_CTL_HOLD;		// stop watchdog timer
	initTime1(); //init timer A0
	initTime2(); //init timer A1
	_enable_interrupts(); //enable interrupts
	GLCD_init();    /* initialize the GLCD controller */
	GLCD_clear();   /* clear display and  home the cursor */
	ADC_init();//init ADC conversion
	char bootup = 1; //restart concision
	short lighton = 0; //bool for photo cell
	while (1) {
	    if (bootup == 1)//if fist start
	    {
	        ADC14->CTL0 |= 1; //start a conversion
	        while (!ADC14->IFGR0); //wait until conversion is complete
	        float result = ADC14->MEM[5]; //read conversion result
	        light = result - 20; //threshold value for light on and off
	        bootup = 0;//bool to say init is done
	    }
	    if (t2 >= 12){//wait 1s for temp and hum
	        read_Packet(Packet); //read data from sensor
            if (check_Checksum(Packet)) // check checksum
                pass = 1; //good data
            else
                pass = 0; //bad data
            humid = (Packet[0]<<8) | Packet[1]; //adds most and lest significant bits
            tempc = (Packet[2]<<8) | Packet[3]; //adds most and leas significant bits
            ADC14->CTL0 |= 1; //start a conversion
            while (!ADC14->IFGR0); //wait until conversion is complete
            float result = ADC14->MEM[5]; //read conversion result
            if (result > light) //see if light is above threshold
                lighton = 1; //light is on
            else
                lighton = 0; //light is off
            GLCD_clear(); //clear display
            displayHT(humid, tempc, lighton); //display data
            t2 = 0; //reset timer
            pass = 0; //reset checksum
	    }
	}
}

void ADC_init() {
    ADC14->CTL0 = 0x00000010; //power on ADC and disable during config
    ADC14->CTL0 |= 0x04080310; //ctl0 according to lab
    ADC14->CTL1 = 0x00000010; //12bit resolution
    ADC14->MCTL[5] = 0x08; //A8 input, single ended vref = AVCC
    P4->SEL1 |= BIT5; //config P4.7 for a6
    P4->SEL0 |= BIT5;
    ADC14->CTL1 |= 0x00050000; //configure for memory register 5
    ADC14->CTL0 |= 0x02; //Enable ADC14 after config
}

void displayHT(unsigned short humid, unsigned short tempc, short lighton) {
    short back[5] = {0, 0, 0, 0, 0};//for reading in data
    int i = 4; //loop var
    short disp = 0; //to check if data has been sent to display for leading 0s
    for(i = 4; humid != 0; i--){//reads the humidity in backwards
        back[i] = humid % 10;//reads in least significant digit
        humid = (humid - (humid % 10))/10; //removes least significant digit
    }
    for (i = 0; i < 5; i++){ //output 5 digits of humidity
        if (back[i] == 0 && disp != 0){// sees if it is a leading 0
            GLCD_putchar(back[i]); //sends 0 digit to display
            back[i] = 0; // reset number place
        } else if (back[i] != 0) { //sees if it is not a 0
            GLCD_putchar(back[i]);//sends non 0 digit to display
            disp = 1;//sets output to display to true
            back[i] = 0;//reset number place
        }
        if (i == 3)//sees if the next number is the decimal
            GLCD_putchar(11); //outputs a '.'
    }
    GLCD_putchar(12); //out puts a %
    GLCD_putchar(13); //out puts a ' '
    disp = 0;//sets display to false
    for(i = 4; tempc != 0; i--){//reads the temp in backwards
        back[i] = tempc % 10; //reads in least significant digit
        tempc = (tempc - (tempc % 10))/10; //removes least significant digit
    }
    for (i = 0; i < 5; i++){//output 5 digits of temp
            if (back[i] == 0 && disp != 0){//sees if it is a leading 0
                GLCD_putchar(back[i]); //sends 0 digit to display
            } else if (back[i] != 0) { //sees if it is not a 0
                GLCD_putchar(back[i]);//out puts non 0 digit
                disp = 1; // sets displayed to true
            }
            if (i == 3)//sees if the next number is the decimal
                GLCD_putchar(11); //out puts a '.'
        }
    GLCD_putchar(10);//outputs a '°'
    GLCD_putchar(14);//outputs a 'c'
    GLCD_putchar(13);//outputs a ' '
    GLCD_putchar(13);//outputs a ' '
    GLCD_putchar(15);//out puts "light is "
    GLCD_putchar(16);
    GLCD_putchar(17);
    GLCD_putchar(18);
    GLCD_putchar(19);
    GLCD_putchar(13);
    GLCD_putchar(16);
    GLCD_putchar(20);
    GLCD_putchar(13);
    if (lighton == 1) {//sees if light is on
        GLCD_putchar(21);//outputs "on"
        GLCD_putchar(22);
    } else {
        GLCD_putchar(21);//outputs "off"
        GLCD_putchar(23);
        GLCD_putchar(23);
    }
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
    timeout = 0; // set time out to false
    TIMER_A1->CTL |= (BIT2 | BIT4); // reset timer to zero and start condition
    TIMER_A1->CCR[0] = 50; //count 50 clocks 100us
    TIMER_A1->CCTL[0] |= BIT4; //enable interrupt
    while(!(P2->IN & DATAPIN) && !timeout); //wait for sensor to pull low or timeout condition
        if (timeout) //checks to see if timeout
            return 0; //return no response
        else {
            TIMER_A1->CTL |= BIT2; //reset timer
            TIMER_A1->CCTL[0] |= BIT4; //enable interrupt
            timeout = 0; //reset timeout
            while((P2->IN & DATAPIN) && !timeout); //wait for pin to go high or timeout
            if(timeout) //checks to see if timeout
                return 0; //returns no response
            else{
                TIMER_A1->CCTL[0] &= ~BIT4;  // Disable timer interrupt
                return 1; //returns good response
            }
        }
}

unsigned char read_Byte(){
    timeout = 0; // resets timeout
    unsigned char num = 0; //init data returned from sensor
    unsigned char i; //counter
    TIMER_A1->CCTL[0] &= ~BIT4; //turns off interrupt
    for (i=8; i>0; i--){ //runs for the 8 returned bytes
        while(!(P2->IN & DATAPIN)); //Wait for signal to go high
        TIMER_A1->CTL |= BIT2; //Reset timer
        TIMER_A1->CTL |= BIT4; //Reenable timer
        TIMER_A1->CCTL[0] |= BIT4; //Enable timer interrupt
        while(P2->IN & DATAPIN); //Wait for signal to go low
        TIMER_A1->CTL &= ~(BIT3 | BIT4); //Halt Timer
        if (TIMER_A1->R > 4)    //40 @ 1x divider
            num |= 1 << (i-1); //sets a a one if returned from sensor
    }
    return num; //returns data
}

unsigned char check_Checksum(unsigned char *data){
    if (data[4] != (data[0] + data[1] + data[2] + data[3])){//valadates checksum
        return 0; //bad data
    }
    else return 1; //good data
}

unsigned char read_Packet(unsigned char * data)
{
    start_signal(); //send start signal
    if (check_Response()){ //checks sensor response
        //Cannot be done with a for loop!
        data[0] = read_Byte();//reads HS
        data[1] = read_Byte();//reads HL
        data[2] = read_Byte();//reads TS
        data[3] = read_Byte();//reads TL
        data[4] = read_Byte();//reads checksum
        return 1; //returns data received
    }
    else return 0; //returns data not received
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
     TIMER_A0->CCR[0] = 0xffff; //count to max time
     //TIMER_A0->EX0 = 0x6;
     NVIC->ISER[0] = BIT8; //set interrupt
     TIMER_A0->CTL |= (BIT2 | BIT4); //resets and start timer A0
}

void initTime2(void){
    TIMER_A1->CTL = (BIT7 | BIT9); //setup see timer1
    TIMER_A1->CCTL[0] = BIT4;//setup see timer1
    TIMER_A1->EX0 = 0x5;//prsceler of 6 for 2us time counts
    NVIC->ISER[0] = 0x400;//sets interrupt
}

void GLCD_putchar(int c)
{
    int i; //look var
    for(i = 0; i < 6; i++) //sends 6 hex codes
        GLCD_data_write(font_table[c][i]); //sends data to SPI
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
