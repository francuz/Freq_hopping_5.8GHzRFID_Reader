// Code based on a good set of example from 
// https://github.com/noccy80/mspdev/tree/master/reference/MSP430ware/examples/devices/5xx_6xx/MSP430F530x%2C%20MSP430F5310%20Code%20Examples/C
// Author: Francesco Amato. Sept 2014. Georgia Tech



//MSP430F530x_UCS_8.c: connection to external xtal
//....
//MSP430F530x_UCS_10.c: use 25 MHz clock
//...
//MSP430F530x_uscia0_spi_10.c use SPI
//...



#include <msp430f5310.h>
#include <stdlib.h>
#include <math.h>

// define a struct to manipulate inputs necessary to implement the freq. hopping algorithm
struct define_freq {
  unsigned long Nint;
  unsigned long Nfrac;
};


void SetVcoreUp (unsigned int level)
{
  // Open PMM registers for write
  PMMCTL0_H = PMMPW_H;              
  // Set SVS/SVM high side new level
  SVSMHCTL = SVSHE + SVSHRVL0 * level + SVMHE + SVSMHRRL0 * level;
  // Set SVM low side to new level
  SVSMLCTL = SVSLE + SVMLE + SVSMLRRL0 * level;
  // Wait till SVM is settled
  while ((PMMIFG & SVSMLDLYIFG) == 0);
  // Clear already set flags
  PMMIFG &= ~(SVMLVLRIFG + SVMLIFG);
  // Set VCore to new level
  PMMCTL0_L = PMMCOREV0 * level;
  // Wait till new level reached
  if ((PMMIFG & SVMLIFG))
    while ((PMMIFG & SVMLVLRIFG) == 0);
  // Set SVS/SVM low side to new level
  SVSMLCTL = SVSLE + SVSLRVL0 * level + SVMLE + SVSMLRRL0 * level;
  // Lock PMM registers for write access
  PMMCTL0_H = 0x00;
}


void SetXT2Up(void)
{
  // Increase Vcore setting to level3 to support fsystem=25MHz
  // NOTE: Change core voltage one level at a time..
  SetVcoreUp (0x01);
  SetVcoreUp (0x02); 
  SetVcoreUp (0x03);
 
  
 // UCSCTL4 |= SELA__XT2CLK;                        // Set ACLK = REFO

//  __bis_SR_register(SCG0);                  // Disable the FLL control loop
//  UCSCTL0 = 0x0000;                         // Set lowest possible DCOx, MODx
//  UCSCTL1 = DCORSEL_7;                      // Select DCO range 50MHz operation
//  UCSCTL2 = FLLD_0;// + 762;                   // Set DCO Multiplier for 25MHz
//                                            // (N + 1) * FLLRef = Fdco
//                                            // (762 + 1) * 32768 = 25MHz
//                                            // Set FLL Div = fDCOCLK/2

  UCSCTL3 = SELREF__XT2CLK;                       // Set DCO FLL reference = XT2
  
  // Set clock to external XT2 25 MHz oscillator
  // Set all clock sources to external crystal
  //UCSCTL4 = 0x00;
  UCSCTL4 |= SELA__XT2CLK;                   // Set ACLK source as XT2CLK
  UCSCTL4 |= SELS__XT2CLK;                   // Set SMCLK source as XT2CLK
  UCSCTL4 |= SELM__XT2CLK;                   // Set MCLK source as XT2CLK
  
  //UCSCTL5 = 0x00;
  UCSCTL5 |= DIVS__1;                        // Divide SMCLK source by 1
  UCSCTL5 |= DIVM__1;                        // Divide MCLK source by 1
  UCSCTL5 |= DIVA__1;                        // Divide ACLK source by 1
 
  //UCSCTL6 = 0x00;
  UCSCTL6 |= XT1OFF;                         // Turn off XT1 if not used
  UCSCTL6 |= SMCLKOFF;                       // Turn off SMCLK
  UCSCTL6 &= ~XT2OFF;                        // Turn on XT2 if not in bypass mode
  UCSCTL6 &= ~XT2BYPASS;                     // Source XT2 internally, not in bypass mode
  UCSCTL6 |= XT2DRIVE_3;                     // Set XT2 drive to highest current, f>24MHz
  UCSCTL6 |= XCAP_3;                         // Set capacitance to highest level (12pF?)
 
//  // Loop until XT1, XT2 & DCO stabilizes
//  do
//  {
//    // Clear XT2,XT1,DCO fault flags
//    UCSCTL7 &= ~(XT2OFFG + XT1LFOFFG + DCOFFG);
//    // Clear fault flags
//    SFRIFG1 &= ~OFIFG;
//    __delay_cycles(60000);
//  }
//  while (SFRIFG1&OFIFG);                    // Test oscillator fault flag
} // End SetXT2Up
 
void SetSPIup(void)
{
  
  UCA1CTL1 |= UCSWRST;                      // **Put state machine in reset**, necessary to configure the SPI registers
  UCA1CTL0 |= UCCKPH;                       // data is captured on the first clk edge and changed on the next edge
  UCA1CTL0 |= UCSYNC;                       // Enable Synchronous mode
  UCA1CTL0 |= UCMODE_1;                      // 4-pin SPI, 8 bit word, active high 
  UCA1CTL0 |= UCMST;                        // Master mode
  UCA1CTL0 |= UCMSB;                        // MSB first
  // inactive state is low, data is changed on the first clock
  
  UCA1CTL1 |= UCSSEL0;                       // Use ACLK clock
  
  UCA1CTL1 &= ~UCSWRST;			     // Enable UCA1
}



//Prepare data for transfer
unsigned long reorderData(int r_w, unsigned long regAddr, unsigned long txData)
{
  unsigned long vPLL_data;
  
  if (r_w)
  {
    vPLL_data = 0x80000000;
  }
  else
  {
    vPLL_data = 0x00000000;
  }
  
    vPLL_data |= (regAddr << 25);
    vPLL_data |= (txData << 1);
    return vPLL_data;
}

//Transmits data over SPI to PLL
 void vTransmit(unsigned long ulData){
  unsigned char txBuffer[4] = {0};
  
  //char cTmp;
  int i;
  i = 4;
  do{
  
    i--;
    
    txBuffer[i] = (ulData >> (8 * i)) & 0xFF;
    
  }while(i != 0);
  
  P4OUT |= BIT3;  		// Pull STE high
  
    UCA1TXBUF = txBuffer[3]; 
    while(!(UCA1IFG & UCRXIFG));
    UCA1TXBUF = txBuffer[2]; 
    while(!(UCA1IFG & UCRXIFG));
    UCA1TXBUF = txBuffer[1]; 
    while(!(UCA1IFG & UCRXIFG));
    UCA1TXBUF = txBuffer[0]; 
    while(!(UCA1IFG & UCRXIFG));
  
  P4OUT &= ~BIT3;     // Pull STE low
} 
  

//Configure PLL at start up
void set_PLL(void){
 // vTransmit(reorderData(0, 0x05, 0x00AAAA));     //VCO conf register VCO_Sub_Reg05
  vTransmit(reorderData(0, 0x05, 0x001628));     //VCO conf register VCO_Sub_Reg05
  vTransmit(reorderData(0, 0x05, 0x0060A0));     //VCO conf register VCO Sub_Reg04
  vTransmit(reorderData(0, 0x05, 0x002818));     //enable doubler in VCO_Sub_Reg3, to enable fundamental mode use 2898h(double check this)
  //vTransmit(reorderData(0, 0x05, 0x002898));     //enable fundamental mode in VCO_Sub_Reg3
  vTransmit(reorderData(0, 0x05, 0x00E090));     //Output divider = 1, max gain in VCO_Sub_Reg_2
  vTransmit(reorderData(0, 0x05, 0x000000));     //close reg05h programming
  vTransmit(reorderData(0, 0x0A, 0x002046));     //VCO tuning conf, set Vtune resolution to typical val (128). Input Xr reference. Use 2046 for input Xr reference/4
 vTransmit(reorderData(0, 0x0B, 0x07C061));     //CP control register conf.
 // vTransmit(reorderData(0, 0x0F, 0x00001F));    //send lock status on SPI
//vTransmit(reorderData(0, 0x06, 0x2003CA));     //enable integer mode
vTransmit(reorderData(0, 0x06, 0x200B4A));     //enable fractional mode
 }



//dec 2 hex converter



//Frequency hop at frequencies above 3 GHz (doubler ON)
void hop(double freq){ //freq is set in MHz
 
  double multiplier;
  struct define_freq set_freq;
  double support; 
  
  
 multiplier = freq/(2*50);
  //multiplier = freq/(50);
  set_freq.Nint =  floor(multiplier);
  support = (multiplier-set_freq.Nint)*16777216;
  set_freq.Nfrac= floor(support);
  //set_freq.Nfrac = floor(abs(n-set_freq.Nint)*16777216);
  
  vTransmit(reorderData(0, 0x03, set_freq.Nint)); //Set Reg3 to integer 
  vTransmit(reorderData(0, 0x04, set_freq.Nfrac)); //Set Reg4 to frac 
  
}




void main(void)
{
  volatile unsigned int i;
  unsigned long rxBuf;
  
  // assign here the frequencies (in MHz) to hop
  //const unsigned int freq_hop[] =  {5765, 5736, 5766, 5792};//, 5778, 5837, 5774, 5807, 5740, 5756, 5820, 5769, 5835, 5828, 5762, 5788, 5823, 5767, 5805, 5750, 5850, 5836, 5796, 5783, 5772, 5748, 5764, 5834, 5746, 5794, 5818, 5771, 5745, 5729, 5845, 5751, 5814, 5764, 5824, 5749, 5786, 5834, 5827, 5848, 5806, 5739, 5819, 5831, 5773, 5784, 5798, 5811, 5789, 5760, 5780, 5800, 5737, 5849, 5839, 5791, 5825, 5738, 5832, 5792, 5725, 5829, 5843, 5764, 5810, 5840, 5753, 5735, 5790, 5850, 5741};
  

    
  WDTCTL = WDTPW+WDTHOLD;                   // Stop WDT
  
// Xtal clock selection and setting up pins
  P1SEL |= BIT0;                         // ACLK
  P1DIR |= BIT0;                  // Set P1.0 for ACLK;
  
  P4DIR |= BIT3;                        // STE
  P4SEL |= BIT0+BIT4+BIT5;          // P4.0,3,4,5 option select: UCA1 Clock, SIMO, SOMI, STE
  
  P5SEL |= BIT2+BIT3;                    // Port select XT2
 
  SetXT2Up();
  SetSPIup();
  
  
  __delay_cycles(20);
  //soft reset PLL
  vTransmit(reorderData(0, 0x00, 0x000020));
  __delay_cycles(20);

  //read Chip ID
  vTransmit(reorderData(1, 0x00, 0x000000));
  rxBuf = UCA1RXBUF; 
  
  //vTransmit(reorderData(0, 0x05, 0x002018));     //enable doubler in VCO_Sub_Reg3, to enable fundamental mode use 2898h(double check this)
//
//  // Assign enable control to SPI
//  vTransmit(reorderData(0, 0x01, 0x000002));
//  __delay_cycles(20);
//
//  
//  
//  //Check if SPI communication is working
//  vTransmit(reorderData(0, 0x0F, 0x0000C0));     
//  //rxBuf = UCA1RXBUF; //SDO should be low
//  vTransmit(reorderData(0, 0x0F, 0x0000E0));
//  rxBuf = UCA1RXBUF; //SDO should be high
//  
//  
//  
//  // Setup VCO subsystem settings
  set_PLL();
//  
//
//  __delay_cycles(10); 
//  // Change frequency
//  
// //implement freq. hopping in band 5.725 - 5.850 GHz. At least 75 hopping freq.
// //cfr. FCC part 15.247 
////http://www.ecfr.gov/cgi-bin/text-idx?SID=4cc7929e764095d0d36a68a130c1c30e&node=se47.1.15_1247&rgn=div8
  while(1)
  {  
    int j = 0;
    //while (j<3)
    //{
      //set freq
     //hop(freq_hop[j]); //input in MHz
      hop(5800); //input in MHz. use these freq: 5730 -- +10 up to 5840 (then do the same procesure going from 5735 to 5835
      //j++;
      // Lock Detect, use default values      
      vTransmit(reorderData(1, 0x12, 0x000000)); //send lock status on SPI
      //while(!UCA1RXBUF);
      __delay_cycles(7000000);  // wait xx sec
    }
 }
// 
//  
//  //Sample code to test the simplified freq hoping algorithm
//  //while(1)
// //{ 
//
//  //set 5.725 GHz
////  vTransmit(reorderData(0, 0x03, 0x000039)); //Set Reg3 to integer 57d
////  vTransmit(reorderData(0, 0x04, 0x400000)); //Set Reg4 to frac 4194304d
////  vTransmit(reorderData(1, 0x04, 0x000000)); //Read Reg4
////  rxBuf = UCA1RXBUF;	
////  
////  //set 5.740 GHz
////  hop(5740); //input in MHz
////  __delay_cycles(5000000);  // wait xx sec
////  
////  //set 5.760 GHz
////  hop(5760); //input in MHz
////  __delay_cycles(5000000);  // wait xx sec
////  
////  //set 5.830 GHz
////  hop(5830); //input in MHz
////  __delay_cycles(5000000);  // wait xx sec
////  
////  //set 5.80 GHz
////  hop(5800); //input in MHz
////  __delay_cycles(5000000);  // wait xx sec
////  
////  
//  
//  // Lock Detect
////  vTransmit(reorderData(0, 0x07, 0x0002CD)); //digital lock detect
////  vTransmit(reorderData(0, 0x0F, 0x00001F)); //send lock status on SPI
////  __delay_cycles(5000000);  // wait xx sec
////  
////  //set 5.850 GHz
////  vTransmit(reorderData(0, 0x03, 0x00003A)); //Set Reg3 to integer 58d
////  vTransmit(reorderData(0, 0x04, 0x800000)); //Set Reg4 to frac 8388608d
////  vTransmit(reorderData(1, 0x04, 0x000000)); //Read Reg4
////  rxBuf = UCA1RXBUF;			
////  
////  __delay_cycles(5000000);  // wait xx sec
////  
// 
// //mute PLL
//  //vTransmit(reorderData(0, 0x05, 0x00E010));     //reset reg_05h
//   
//// }   




