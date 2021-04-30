// Serial Example
// Jason Losh
//
// getsUart0() added by Nicholas Untrecht
//-----------------------------------------------------------------------------
// Hardware Target
//-----------------------------------------------------------------------------

// Target Platform: EK-TM4C123GXL Evaluation Board
// Target uC:       TM4C123GH6PM
// System Clock:    40 MHz

// Hardware configuration:
// Red LED:
//   PF1 drives an NPN transistor that powers the red LED
// Green LED:
//   PF3 drives an NPN transistor that powers the green LED
// UART Interface:
//   U0TX (PA1) and U0RX (PA0) are connected to the 2nd controller
//   The USB on the 2nd controller enumerates to an ICDI interface and a virtual COM port
//   Configured to 115,200 baud, 8N1

//-----------------------------------------------------------------------------
// Device includes, defines, and assembler directives
//-----------------------------------------------------------------------------

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "clock.h"
#include "uart0.h"
#include "tm4c123gh6pm.h"
#include "pwm.h"
#include "wait.h"

// Bitbanding Aliases
#define RED_LED      (*((volatile uint32_t *)(0x42000000 + (0x400253FC-0x40000000)*32 + 1*4))) // PF1
#define GREEN_LED    (*((volatile uint32_t *)(0x42000000 + (0x400253FC-0x40000000)*32 + 3*4))) // PF3
#define BLUE_LED     (*((volatile uint32_t *)(0x42000000 + (0x400253FC-0x40000000)*32 + 2*4))) // PF2
#define PUSH_BUTTON (*((volatile uint32_t *)(0x42000000 + (0x400253FC-0x40000000)*32 + 4*4))) // PF4
#define SLEEP_PIN  (*((volatile uint32_t *)(0x42000000 + (0x400053FC-0x40000000)*32 + 6*4))) // PB6

#define TRIGGER_PIN (*((volatile uint32_t *)(0x42000000 + (0x400243FC-0x40000000)*32 + 1*4))) // PE1
#define ECHO_PIN    (*((volatile uint32_t *)(0x42000000 + (0x400243FC-0x40000000)*32 + 3*4))) // PE3

// Masks
#define RED_LED_MASK 2
#define BLUE_LED_MASK 4
#define GREEN_LED_MASK 8
#define PUSH_BUTTON_MASK 16

#define MAX_CHARS 80
#define MAX_FIELDS 5

// PortB masks
#define RED_BL_LED_MASK 32 // B5
#define NEW_BL_MASK 16 // B4
#define SLEEP_MASK 64

// PortC masks
#define FREQ_IN_MASK_C6 64
#define FREQ_IN_MASK_C4 16

// PortE masks
#define TRIGGER_MASK 2
#define ECHO_MASK 8
#define BLUE_BL_LED_MASK 16
#define GREEN_BL_LED_MASK 32

// PortF masks


typedef struct _USER_DATA
{
char buffer[MAX_CHARS+1];
uint8_t fieldCount;
uint8_t fieldPosition[MAX_FIELDS];
char fieldType[MAX_FIELDS];
} USER_DATA;

#define MAX_INSTRUCTIONS 5

typedef struct _instruction
{
uint8_t command;
uint16_t argument;	
} instruction;

//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

void configTimers()
{
	WTIMER0_CTL_R &= ~TIMER_CTL_TAEN;
	WTIMER1_CTL_R &= ~TIMER_CTL_TAEN;	// turn-off counter before reconfiguring
    WTIMER0_CFG_R = 4;    
	WTIMER1_CFG_R = 4;	// configure as 32-bit counter (A only)
    WTIMER0_TAMR_R = TIMER_TAMR_TAMR_CAP | TIMER_TAMR_TACDIR;  // configure for count up, capture
	WTIMER1_TAMR_R = TIMER_TAMR_TAMR_CAP | TIMER_TAMR_TACDIR; // configure for count up, capture
    WTIMER0_TAMR_R &= ~TIMER_TAMR_TACMR;
	WTIMER1_TAMR_R &= ~TIMER_TAMR_TACMR;	// configure edge-count
    WTIMER0_IMR_R = 0;
	WTIMER1_IMR_R = 0;	// turn-off interrupts
    WTIMER0_TAV_R = 0;
	WTIMER1_TAV_R = 0;	// zero counter for first period
    WTIMER0_CTL_R |= TIMER_CTL_TAEN;
	WTIMER1_CTL_R |= TIMER_CTL_TAEN;
}

// Initialize Hardware
void initHw()
{
    // Initialize system clock to 40 MHz
    initSystemClockTo40Mhz();


    // Enable clocks
    SYSCTL_RCGCGPIO_R |= SYSCTL_RCGCGPIO_R1 | SYSCTL_RCGCGPIO_R2 | SYSCTL_RCGCGPIO_R4 | SYSCTL_RCGCGPIO_R5;
    SYSCTL_RCGCPWM_R |= SYSCTL_RCGCPWM_R0;
    SYSCTL_RCGCWTIMER_R |= SYSCTL_RCGCWTIMER_R1 | SYSCTL_RCGCWTIMER_R0;
	SYSCTL_RCGCTIMER_R |= SYSCTL_RCGCTIMER_R1;
	_delay_cycles(3);
	
	TIMER1_CTL_R &= ~TIMER_CTL_TAEN;
	TIMER1_CFG_R = 0;
	TIMER1_TAMR_R = 0x2;
	TIMER1_TAMR_R |= TIMER_TAMR_TACDIR;
	TIMER1_TAV_R = 0;
	
	
    _delay_cycles(3);

    initPWM();

    // PF1-4 for LEDs and Push Button
    GPIO_PORTF_DIR_R |= BLUE_LED_MASK | RED_LED_MASK | GREEN_LED_MASK;
    GPIO_PORTF_DIR_R &= ~PUSH_BUTTON_MASK;
    GPIO_PORTF_PUR_R |= PUSH_BUTTON_MASK;
    GPIO_PORTF_DR2R_R |= BLUE_LED_MASK | RED_LED_MASK | GREEN_LED_MASK;
    GPIO_PORTF_DEN_R |= BLUE_LED_MASK | RED_LED_MASK | GREEN_LED_MASK | PUSH_BUTTON_MASK;


    // PC4 and PC6 for WTIMERS for detecting magnet rotations.
    GPIO_PORTC_AFSEL_R |= FREQ_IN_MASK_C6 | FREQ_IN_MASK_C4;
    GPIO_PORTC_PCTL_R &= ~(GPIO_PCTL_PC6_M | GPIO_PCTL_PC4_M);
    GPIO_PORTC_PCTL_R |= GPIO_PCTL_PC6_WT1CCP0 | GPIO_PCTL_PC4_WT0CCP0;
    GPIO_PORTC_PUR_R |= FREQ_IN_MASK_C6 | FREQ_IN_MASK_C4;
    GPIO_PORTC_DEN_R |= FREQ_IN_MASK_C6 | FREQ_IN_MASK_C4;
	
	configTimers();
	
	GPIO_PORTE_DIR_R |= TRIGGER_MASK;
	GPIO_PORTE_DIR_R &= ~ECHO_MASK;
	GPIO_PORTE_PUR_R |= ECHO_MASK;
	GPIO_PORTE_DR2R_R |= TRIGGER_MASK;
	GPIO_PORTE_DEN_R |= TRIGGER_MASK | ECHO_MASK;

}

// Function that gets string from terminal
// Supports backspaces and enter keys
void getsUart0(USER_DATA* data)
{
    int count = 0;
    char c;

    while(1)
    {
        c = getcUart0();

        // If char c is a backspace (8 or 127), allows overriding of buffer
        if( c == 8 || c == 127 && count > 0)
            count--;
        // If the char c is readable (space, num, alpha), read to buffer
        else if( c >= 32 )
            data->buffer[count++] = c;

        // If return is hit or the MAX_CHARS limit reached, add the null and return
        if( c == 13 || count == MAX_CHARS)
        {
            data->buffer[count] = '\0';
            return;
        }
    }
}

void parseFields(USER_DATA* data)
{
    char c = '!';
    data->fieldCount = 0;
    uint8_t isPrevDelim = 1;
    uint8_t i;
    // Loop until we reach the end of the buffer
    for(i = 0; i < MAX_CHARS; i++)
    {
        c = data->buffer[i]; // Gets the char from the buffer
        if( c == NULL)
            return;

        if(isPrevDelim)
        {
            // if char c is alpha a-z LOWERCASE
            if( c >= 'a' && c <= 'z' )
            {
                data->fieldType[data->fieldCount] = 'a';
                data->fieldPosition[data->fieldCount] = i;
                data->fieldCount++;
                isPrevDelim = 0;
            }

            // if char c is alpha A-Z UPPERCASE
            if ( c >= 'A' && c <= 'Z' )
            {
                data->fieldType[data->fieldCount] = 'A';
                data->fieldPosition[data->fieldCount] = i;
                data->fieldCount++;
                isPrevDelim = 0;
            }

            // if char c is numeric
            if( c >= '0' && c <= '9')
            {
                data->fieldType[data->fieldCount] = 'n';
                data->fieldPosition[data->fieldCount] = i;
                data->fieldCount++;
                isPrevDelim = 0;
            }
        }
        else if( !( (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ) )
        {
            isPrevDelim = 1;
            data->buffer[i] = '\0';
        }

    }

}

char* getFieldString(USER_DATA* data, uint8_t fieldNumber)
{
    if(fieldNumber <= data->fieldCount)
        return &data->buffer[ data->fieldPosition[fieldNumber] ];
    else
        return '\0';
}


int32_t getFieldInteger(USER_DATA* data, uint8_t fieldNumber)
{
    char* strValue;
    int32_t returnVal = 0;

    if(fieldNumber <= data->fieldCount && data->fieldType[fieldNumber] == 'n')
    {
        strValue = &data->buffer[ data->fieldPosition[fieldNumber] ];

        int8_t i;
        for(i = 0; strValue[i] != '\0'; ++i)
        {
            returnVal = returnVal * 10 + (strValue[i] - '0');
        }

        return returnVal;
    }

    else
        return -1;
}


bool strcomp(char * a, char * b)
{
    int8_t i = 0;
    char c1 = a[i];
    char c2 = b[i];

    do
    {
        if(c1 != c2)
            return false;
        else
        {
            i++;
            c1 = a[i];
            c2 = b[i];
        }
    } while(c1 != NULL || c2 != NULL);
    return true;
}


bool isCommand(USER_DATA* data, char strCommand[], uint8_t minArguments)
{
    char* strCompare = &data->buffer[ data->fieldPosition[0] ];

    if( minArguments >= data->fieldCount - 1 && strcomp(strCompare, strCommand) )
        return true;
    else
        return false;
}

void rb_forward( int16_t dist )
{
    uint16_t ticks = 40 * dist / 30;

	// Calculate distance in centimeters.
    PWM0_1_CMPA_R = 0;
    PWM0_1_CMPB_R = 1001;
    PWM0_2_CMPA_R = 996;
    PWM0_2_CMPB_R = 0;
    GREEN_LED = 1;

    if(dist == -1)
    {
        return;
    }
    else
    {
        WTIMER0_TAV_R = 0;
        WTIMER1_TAV_R = 0;
        while(WTIMER0_TAV_R < ticks || WTIMER1_TAV_R < ticks);
        GREEN_LED = 0;
        PWM0_1_CMPB_R = 0;
        PWM0_2_CMPA_R = 0;
    }


    //waitMicrosecond(1000000);


	return;
}

void rb_reverse( int16_t dist )
{
    uint16_t ticks = 40 * dist / 30;

	// Calculate distance in centimeters.
    PWM0_1_CMPA_R = 1001;
    PWM0_1_CMPB_R = 0;
    PWM0_2_CMPA_R = 0;
    PWM0_2_CMPB_R = 996;
    GREEN_LED = 1;

    //waitMicrosecond(1000000);

    if(dist == -1)
    {
        return;
    }
    else
    {
        WTIMER0_TAV_R = 0;
        WTIMER1_TAV_R = 0;
        while(WTIMER0_TAV_R < ticks || WTIMER1_TAV_R < ticks);
        GREEN_LED = 0;
        PWM0_1_CMPA_R = 0;
        PWM0_2_CMPB_R = 0;
    }
    return;
}

void rb_cwRotate( int16_t angle )
{
    uint16_t ticks = 20 * angle / 90;

    PWM0_1_CMPA_R = 1001;
    PWM0_1_CMPB_R = 0;
    PWM0_2_CMPA_R = 996;
    PWM0_2_CMPB_R = 0;

    //waitMicrosecond(1000000);
    WTIMER0_TAV_R = 0;
    WTIMER1_TAV_R = 0;
    while(WTIMER0_TAV_R < ticks || WTIMER1_TAV_R < ticks);

    PWM0_1_CMPA_R = 0;
    PWM0_2_CMPA_R = 0;
	return;
}

void rb_ccwRotate( int16_t angle )
{
    uint16_t ticks = 45 * angle / 180;

    PWM0_1_CMPA_R = 0;
    PWM0_1_CMPB_R = 1001;
    PWM0_2_CMPA_R = 0;
    PWM0_2_CMPB_R = 996;

    //waitMicrosecond(1000000);
    WTIMER0_TAV_R = 0;
    WTIMER1_TAV_R = 0;
    while(WTIMER0_TAV_R < ticks || WTIMER1_TAV_R < ticks);

    PWM0_1_CMPB_R = 0;
    PWM0_2_CMPB_R = 0;
	return;
}	

void rb_wait( char * device )
{
    RED_LED = 1;
    SLEEP_PIN = 0;
    if( strcomp(device, "pb") )
    {
        while(PUSH_BUTTON);
        SLEEP_PIN = 1;
    }
    RED_LED = 0;
	return;
}

void rb_pause( uint8_t time )
{
    waitMicrosecond(time *  1000);
	return;
}

void rb_stop()
{
    SLEEP_PIN = 0;
    RED_LED = 1;
	return;
}

void comm2str(instruction instruct, int index)
{
    char output[20];
    int16_t argument = instruct.argument;
    if(argument == 0xFFFF)
        argument = -1;
    switch(instruct.command)
    {
    case 0:
        sprintf(output, "%d. forward %d", index+1, instruct.argument);
        putsUart0(output);
        break;
        //putsUart0()
    case 1:
        sprintf(output, "%d. reverse %d", index+1, instruct.argument);
        putsUart0(output);
        break;
    case 2:
        sprintf(output, "%d. cw %d", index+1, instruct.argument);
        putsUart0(output);
        break;
    case 3:
        sprintf(output, "%d. ccw %d", index+1, instruct.argument);
        putsUart0(output);
        break;
    case 4:
        sprintf(output, "%d. wait %d", index+1, instruct.argument);
        putsUart0(output);
        break;
    case 5:
        sprintf(output, "%d. pause %d", index+1, instruct.argument);
        putsUart0(output);
        break;
    case 6:
        sprintf(output, "%d. stop %d", index+1, instruct.argument);
        putsUart0(output);
        break;
    }
    putcUart0('\n');
    return;
}

instruction comm2instruct(USER_DATA comm)
{
    instruction returnStruct;
    if( isCommand(&comm, "forward", 2) )
    {
        returnStruct.command = 0;
        if( getFieldInteger(&comm, 1) == -1 )
            returnStruct.argument = 0xFFFF;
        else
            returnStruct.argument = getFieldInteger(&comm, 1);
    }

    if( isCommand(&comm, "reverse", 2) )
    {
        returnStruct.command = 1;
        if( getFieldInteger(&comm, 1) == -1 )
            returnStruct.argument = 0xFFFF;
        else
            returnStruct.argument = getFieldInteger(&comm, 1);
    }

    if( isCommand(&comm, "cw", 2) )
    {
        returnStruct.command = 2;
        if( getFieldInteger(&comm, 1) == -1 )
            returnStruct.argument = 0xFFFF;
        else
            returnStruct.argument = getFieldInteger(&comm, 1);
    }

    if( isCommand(&comm, "ccw", 2) )
    {
        returnStruct.command = 3;
        if( getFieldInteger(&comm, 1) == -1 )
            returnStruct.argument = 0xFFFF;
        else
            returnStruct.argument = getFieldInteger(&comm, 1);
    }

    if( isCommand(&comm, "wait", 2) )
    {
        returnStruct.command = 4;
        if( strcomp(getFieldString(&comm, 1), "pb") )
            returnStruct.argument = 0x1111;
        else
            returnStruct.argument = 0xFFFF;
    }

    if( isCommand(&comm, "pause", 2) )
    {
        returnStruct.command = 5;
        returnStruct.argument = getFieldInteger(&comm, 1);
    }

    if( isCommand(&comm, "stop", 1) )
    {
        returnStruct.command = 6;
        returnStruct.argument = getFieldInteger(&comm, 1);
    }

    return returnStruct;
}

void test()
{
    PWM0_1_CMPA_R = 1000;
    waitMicrosecond(1000000);
    PWM0_1_CMPA_R = 0;

    PWM0_1_CMPB_R = 1000;
    waitMicrosecond(1000000);
    PWM0_1_CMPB_R = 0;

    PWM0_2_CMPA_R = 1000;
    waitMicrosecond(1000000);
    PWM0_2_CMPA_R = 0;

    PWM0_2_CMPB_R = 1000;
    waitMicrosecond(1000000);
    PWM0_2_CMPB_R = 0;
}

void data_flush(USER_DATA * clear)
{
/*
 * char buffer[MAX_CHARS+1];
 * uint8_t fieldCount;
 * uint8_t fieldPosition[MAX_FIELDS];
 * char fieldType[MAX_FIELDS];
 */
    int8_t i;
    for(i = 0; i < MAX_CHARS; i++)
        clear->buffer[i] = '\0';
    clear->fieldCount = 0;
    for(i = 0; i < MAX_FIELDS; i++)
    {
        clear->fieldPosition[i] = 0;
        clear->fieldType[i] = '\0';
    }

}

void instruct_insert(instruction * arr, instruction adding, uint8_t insert, int8_t index, bool max)
{
    uint8_t i;
    insert--;

    if(max)
    {
        for(i = MAX_INSTRUCTIONS-2; i >= insert; i--)
            arr[i+1] = arr[i];
        arr[insert] = adding;
    }
    else
    {
        if(insert >= index)
            return;

        for(i = index-1; i >= insert; i--)
            arr[i+1] = arr[i];
        arr[insert] = adding;
    }
    return;
}

void instruct_delete(instruction * arr, uint8_t remove, int8_t index, bool max)
{
    uint8_t i;
    remove--;

    if(max)
    {
        for(i = remove; i < MAX_INSTRUCTIONS-1; i++)
            arr[i] = arr[i+1];
    }
    else
    {
        if(remove >= index)
            return;

        for(i = remove; i < index-1; i++)
            arr[i] = arr[i+1];
    }
    return;
}

void wait_distance( uint32_t input )
{
	uint32_t dist;
	
	RED_LED = 1;
	ECHO_PIN = 0;

	do
	{
	    TIMER1_TAV_R = 0;

		TRIGGER_PIN = 1;
		waitMicrosecond(20);
		TRIGGER_PIN = 0;

		while( ECHO_PIN == 0 );
		TIMER1_CTL_R |= TIMER_CTL_TAEN;

		while( ECHO_PIN )
		TIMER1_CTL_R &= ~TIMER_CTL_TAEN;
		dist = TIMER1_TAV_R / 1000000 / 58;
	} while(input < dist);
	
	char s[5];
	sprintf(s, "%d\n", dist);
	putsUart0(s);
	BLUE_LED = 1;
	GREEN_LED = 1;
	waitMicrosecond(1000000);
	RED_LED = 0;
	GREEN_LED = 0;
	BLUE_LED = 0;
	return;
}

//-----------------------------------------------------------------------------
// Main
//-----------------------------------------------------------------------------

int main(void)
{
    USER_DATA data;
    instruction inst_arr[MAX_INSTRUCTIONS];
    int8_t inst_index = 0;
    bool inst_max = false;

    initHw();
    initUart0();
    setUart0BaudRate(115200, 40e6);
    SLEEP_PIN = 1;
	
	wait_distance(15);
	
	uint8_t i;

    while(true)
    {
        putcUart0('>');
        BLUE_LED = 1;
        getsUart0(&data);
        BLUE_LED = 0;
        putcUart0('\n');

        parseFields(&data);


#ifdef DEBUG
        putcUart0('\n');
        for (i = 0; i < data.fieldCount; i++)
        {
            putcUart0(data.fieldType[i]);
            putcUart0('\t');
            putsUart0(&data.buffer[ data.fieldPosition[i] ]);
            putcUart0('\n');
        }
#endif


        if( isCommand(&data, "forward", 2) )
		{
            inst_arr[inst_index].command = 0;
            if( getFieldInteger(&data, 1) == -1 )
                inst_arr[inst_index++].argument = 0xFFFF;
            else
                inst_arr[inst_index++].argument = getFieldInteger(&data, 1);
			rb_forward( getFieldInteger(&data, 1) );
		}
		
		if( isCommand(&data, "reverse", 2) )
		{
		    inst_arr[inst_index].command = 1;
		    if( getFieldInteger(&data, 1) == -1 )
		        inst_arr[inst_index++].argument = 0xFFFF;
		    else
		        inst_arr[inst_index++].argument = getFieldInteger(&data, 1);
			rb_reverse( getFieldInteger(&data, 1) );
		}
		
		if( isCommand(&data, "cw", 2) )
		{
		    inst_arr[inst_index].command = 2;
		    if( getFieldInteger(&data, 1) == -1 )
		        inst_arr[inst_index++].argument = 0xFFFF;
		    else
		        inst_arr[inst_index++].argument = getFieldInteger(&data, 1);
			rb_cwRotate( getFieldInteger(&data, 1) );
		}
		
		if( isCommand(&data, "ccw", 2) )
		{
		    inst_arr[inst_index].command = 3;
		    if( getFieldInteger(&data, 1) == -1 )
		        inst_arr[inst_index++].argument = 0xFFFF;
		    else
		        inst_arr[inst_index++].argument = getFieldInteger(&data, 1);
			rb_ccwRotate( getFieldInteger(&data, 1) );
		}
		
		if( isCommand(&data, "wait", 2) )
		{
		    inst_arr[inst_index].command = 4;
		    if( strcomp(getFieldString(&data, 1), "pb") )
		        inst_arr[inst_index++].argument = 0x1111;
		    else
		        inst_arr[inst_index++].argument = 0xFFFF;
			rb_wait( getFieldString(&data, 1) );
		}
		
		if( isCommand(&data, "pause", 2) )
		{
		    inst_arr[inst_index].command = 5;
		    inst_arr[inst_index++].argument = getFieldInteger(&data, 1);
			rb_pause( getFieldInteger(&data, 1) );
		}
		
		if( isCommand(&data, "stop", 1) )
		{
		    inst_arr[inst_index].command = 6;
		    inst_arr[inst_index++].argument = getFieldInteger(&data, 1);
			rb_stop();
		}

		if( isCommand(&data, "list", 1) )
		{
		    if(inst_max)
		        for(i = 0; i < MAX_INSTRUCTIONS; i++)
		            comm2str(inst_arr[i], i);
		    else
		        for(i = 0; i < inst_index; i++)
		            comm2str(inst_arr[i], i);
		}

		if( isCommand(&data, "insert", 2) )
		{
		    uint8_t spot = getFieldInteger(&data, 1);

		    putsUart0("insert command: ");
		    getsUart0(&data);
		    putcUart0('\n');
		    parseFields(&data);
		    instruction inserting = comm2instruct(data);

		    instruct_insert(inst_arr, inserting, spot, inst_index++, inst_max);
		}

		if( isCommand(&data, "delete", 2) )
		{
		    instruct_delete(inst_arr, getFieldInteger(&data, 1), inst_index--, inst_max);
		    if(inst_max)
		    {
		        inst_index = MAX_INSTRUCTIONS - 1;
		        inst_max = false;
		    }
		}

		if(inst_index % MAX_INSTRUCTIONS == 0)
		{
		    inst_index = inst_index % MAX_INSTRUCTIONS;
		    inst_max = true;
		}
		char s[5];
		sprintf(s, "%d %d\n", WTIMER0_TAV_R, WTIMER1_TAV_R);
	    putsUart0(s);
		data_flush(&data);
    }
}
