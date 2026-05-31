/*******************************************************************************
* Copyright (C) 2021-22 Maxim Integrated Products, Inc., All Rights Reserved.
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the "Software"),
* to deal in the Software without restriction, including without limitation
* the rights to use, copy, modify, merge, publish, distribute, sublicense,
* and/or sell copies of the Software, and to permit persons to whom the
* Software is furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
* OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL MAXIM INTEGRATED BE LIABLE FOR ANY CLAIM, DAMAGES
* OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
* ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
* OTHER DEALINGS IN THE SOFTWARE.
*
* Except as contained in this notice, the name of Maxim Integrated
* Products, Inc. shall not be used except as stated in the Maxim Integrated
* Products, Inc. Branding Policy.
*
* The mere transfer of this software does not imply any licenses
* of trade secrets, proprietary technology, copyrights, patents,
* trademarks, maskwork rights, or any other form of intellectual
* property whatsoever. Maxim Integrated Products, Inc. retains all
* ownership rights.
*******************************************************************************
*/

#include "mbed.h"
#include "DigitalIn.h"
#include "PinNames.h"
#include "platform/mbed_thread.h"
#include "mbed.h"
#include <cmath>
#include <cstdint>
/******************************************************************************
* https://os.mbed.com/users/phonemacro/code/MAX32664C_Example_Host_Code
* Tera Term output is set to 115200 baud rate.
* ver: 220412
******************************************************************************/

/******************************************************************************
* Warning, if using the either
*   MAX32630FTHR+MAXM86161_ADPTER_REVB+MAXM86146EVSYS sensor brd or
*   MAX32630FTHR+MAXM86161_ADPTER_REVB+MAXM86161EVSYS sensor brd,
* The VLED is connected to USB power which is noisy. The VLED should be
* connected to a regulated power supply if you are testing accuracy.
*******************************************************************************
*/

/*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*/
// define one and only one of the following three platforms
#define MAXM86146_CFG 1  // tested on MAXM86146EVSYS_sensorBrd+MAXM86161_ADAPTER_REVB+MAX32630FTHR 33.13.31/12
//#define MAXREFDES103_CFG  // not tested
//#define MAXM86161_CFG 1  // tested on MAXM86161+MAX32630FTHR v32.9.23, 32.13.12
/*****************************************************************************/

/*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*/
// define which adaptor board is being used
#define MAXM86161_ADPTR_EVKIT_B
//#define MAXREFDES1319  // MAXM86161: Remove J7; For PIN1=ACC_INTN, J9 set to 2-3; For PIN13=OPT_INTN, J10 set to 2-3
/******************************************************************************
* MAX32630FTHR GPIOs
******************************************************************************/
#ifdef MAXM86161_ADPTR_EVKIT_B
  #define RST_PIN   P5_6
  #define MFIO_PIN  P5_4
#else
  #ifdef MAXREFDES1319
     #define RST_PIN   P3_0
     #define MFIO_PIN  P5_2
  #endif
#endif

DigitalOut rst(RST_PIN, PullUp);
DigitalOut mfio(MFIO_PIN, PullUp);
DigitalIn Button(P2_3);
I2C sh_i2c(P3_4, P3_5);

/*****************************************************************************/
#if defined(MAXM86161_CFG) //|| defined(MAXM86146_CFG)  // don't need 3.3V for MAXM86146EVSYS_sensorBrd+MAXM86161_ADAPTER_REVB+MAX32630FTHR
#include "max32630fthr.h"
MAX32630FTHR pegasus(MAX32630FTHR::VIO_3V3); // Enable J1-pin2, 3.3V and set GPIO to 3.3v
#endif



/*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*/
//#define RAW  // define this if you want AFE+accel data, no algorithm
//#define ALGO_ONLY 1  // define this if you only want algo data, comment out if you want raw sensor+algo data

// Comment out both of the below for Normal Algorithm Samples Format
#define EXTENDED_ALGO 1  // define this if you want the extended algo samples report format 
//#define PACKED_NORMAL_ALGO 1  // define this if you want the packed normal algo samples report format, 33.13.31
//#define PCK_CFG_MASK  1 // define this and the above if you want to config mask out some of the packed data, 33.13.31
//#define AGC 1 // define this for AGC, otherwise the default is AEC


#define REDUCE_RPT_PERIOD 0x19  // Report samples every 25 frame.
//#define USE_FIFO_BUFFER_CNT 5  // Allow the FIFO to accumulate this many samples

#ifdef MAXREFDES103_CFG
#include "MAX20303.h"
I2C sh_i2c_pmic(P5_7, P6_0);
#endif

#ifdef MAXM86146_CFG
  #define PPG_SZ 36  //maxm86146
#else
  #define PPG_SZ 18  //maxm86161, max86141
#endif
#define ACCEL_SZ 6  // accel
#define SENSOR_SZ (PPG_SZ+ACCEL_SZ)

#if defined(MAXM86161_CFG) || defined(MAXM86146_CFG)  // MAXM86161, MAXM86146, keep algo Fifo size as 52, 20 to keep compatible w/ the GUI
#define old_algo_sz 1 // 3x.12.0 or earlier; 32.9.x
#endif

#if defined(old_algo_sz)
  #if EXTENDED_ALGO
    #define ALGO_SZ 52  // 52 bytes, extended algo size for 3x.12.0
  #elif defined(PACKED_NORMAL_ALGO)
    #define ALGO_SZ 16  // 16 bytes, packed algo normal size for 33.31.31
  #else
    #define ALGO_SZ 20  // 20 bytes, normal algo size for 3x.12.0
  #endif
#else
  #if EXTENDED_ALGO
    #define ALGO_SZ 56  // 56 bytes, extended algo size for 3x.13.x+
  #else
    #define ALGO_SZ 24  // 24 bytes, normal algo size for 3x.13.x+
  #endif
#endif

#if ALGO_ONLY
  #define TTL_SZ (ALGO_SZ)
#else
  #ifdef RAW
    #define TTL_SZ (PPG_SZ+ACCEL_SZ)
  #else
    #define TTL_SZ (PPG_SZ+ACCEL_SZ+ALGO_SZ)
  #endif
#endif


BufferedSerial pc(USBTX, USBRX, 115200);
DigitalOut rLED(LED1);
DigitalOut gLED(LED2);
DigitalOut bLED(LED3);

int32_t heading_printed;  // has the heading been printed?


//#define thread_sleep_for(x) wait_ms(x) // for older versions of mbed


const int SH_ADDR = 0xAA;//0x55;
int32_t Time_to_Read_PPG = 0;

#define BLINKING_RATE_MS 1000ms
void blink_timer(void) {
    gLED = !gLED;  /* blink the green LED */
}

void fifo_timer(void) {
    Time_to_Read_PPG = 1;
}

/*****************************************************************************/
// read_sh_fifo
/*****************************************************************************/
/*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*/
#define OPTIMIZE_FIFO_READ 1  // Assume that the FIFO is filled at the specified rate, so just periodically check then number of samples in the FIFO to save power
#ifdef OPTIMIZE_FIFO_READ     // tested on MAXM86146EVSYS
    int32_t check_fifo_countdown;
    #define MAX_FIFO_CNT 20   // Only send the 0x00 0x00 (step 2.0), and 0x12 0x00 (step 2.1) commands every MAX_FIFO_CNT frames.
#endif

void read_sh_fifo(void) {
    char cmd[8], i, j, k, samples;
    char rsp[3000];
    int32_t ppg[12];
    int16_t accel[3];
    int32_t tst[20];
    int32_t status, opmode, hr, hr_conf, ibi, ibi_conf;
#if EXTENDED_ALGO
    int32_t walk_stp, run_stp, energy, amr, iadj1_rqt, iadj1, iadj2_rqt, iadj2, iadj3_rqt, iadj3;
    int32_t intadj_rqt, intadj, smpladj_rqt, smpladj, rqt_smplave, afestatehr, hr_motion;
#endif
    int32_t act, r, spo2, spo2_conf;
    int32_t spo2_compl, spo2_lo, spo2_mo, spo2_lopi, spo2_unrel, spo2_state, ibi_offset, scd, inappro_ori;
    int32_t ptr = 0;
    int32_t sptr = 0;
    int16_t count;
    mfio = 0; wait_us(300);
    Time_to_Read_PPG = 0;
    samples = 1;



#ifdef USE_FIFO_BUFFER_CNT
    samples = USE_FIFO_BUFFER_CNT;
#endif
#if defined(OPTIMIZE_FIFO_READ)
    check_fifo_countdown--;
    if (check_fifo_countdown == 0) {
#endif

#if defined(MAXREFDES103_CFG) || defined(MAXM86161_CFG)
// 2.1
        cmd[0] = 0x00; cmd[1] = 0x00;
        sh_i2c.write(SH_ADDR, cmd, 2);
        wait_us(100);
        sh_i2c.read(SH_ADDR, rsp, 2);
//        printf("2.1 Status: %x %x\n\r", rsp[0], rsp[1]);
#else
    // tested w/ 33.13.12 doen't need 2.1
#endif


// 2.2
#if defined(PACKED_NORMAL_ALGO)
        cmd[0] = 0x20; 
        sh_i2c.write(SH_ADDR, cmd, 1);
#else
        cmd[0] = 0x12; cmd[1] = 0x00;
        sh_i2c.write(SH_ADDR, cmd, 2);
#endif
        wait_us(100);
        sh_i2c.read(SH_ADDR, rsp, 2);
//      mfio = 1; mfio = 0; wait_us(300);

//      printf("2.2 Status: %x %x\n\r", rsp[0], rsp[1]);
        samples = rsp[1];
//      printf("num samples %d, (num*ttl)+1 %d\n\r",  rsp[1], TTL_SZ*samples+1);
//      printf("num smpls %d \n\r",  samples);
#if defined(OPTIMIZE_FIFO_READ)
    }
    if (check_fifo_countdown <= 0)
        check_fifo_countdown = MAX_FIFO_CNT;
#endif

// 2.3
#if defined(PACKED_NORMAL_ALGO)
        cmd[0] = 0x21;;
        sh_i2c.write(SH_ADDR, cmd, 1);
#else
        cmd[0] = 0x12; cmd[1] = 0x01;
        sh_i2c.write(SH_ADDR, cmd, 2);
#endif
        wait_us(100);
        sh_i2c.read(SH_ADDR, rsp, 1+(TTL_SZ*samples));
        //pc.printf("num smpls %d ttlsiz %d\n\r",  samples, TTL_SZ);
//    mfio = 1; mfio = 0; wait_us(300);
        mfio = 1;

        status = rsp[0];

        sptr = 1;
        for (i = 0; i < samples; i++) {
            ptr = sptr;
#if !(ALGO_ONLY)
            ppg[0] = (rsp[ptr+0] << 16) | (rsp[ptr+1] << 8) | (rsp[ptr+2]);
            ppg[1] = (rsp[ptr+3] << 16) | (rsp[ptr+4] << 8) | (rsp[ptr+5]);
            ppg[2] = (rsp[ptr+6] << 16) | (rsp[ptr+7] << 8) | (rsp[ptr+8]);
            ppg[3] = (rsp[ptr+9] << 16) | (rsp[ptr+10] << 8) | (rsp[ptr+11]);
            ppg[4] = (rsp[ptr+12] << 16) | (rsp[ptr+13] << 8) | (rsp[ptr+14]);
            ppg[5] = (rsp[ptr+15] << 16) | (rsp[ptr+16] << 8) | (rsp[ptr+17]);
            //printf("%d,%d,%d,%d,", ppg[0], ppg[1], ppg[2], ppg[3]);
#ifdef MAXM86146_CFG
#ifdef RAW
            //printf("%d,%d,", ppg[4], ppg[5]);
            ppg[6] = (rsp[ptr+18] << 16) | (rsp[ptr+19] << 8) | (rsp[ptr+20]);
            ppg[7] = (rsp[ptr+21] << 16) | (rsp[ptr+22] << 8) | (rsp[ptr+23]);
            ppg[8] = (rsp[ptr+24] << 16) | (rsp[ptr+25] << 8) | (rsp[ptr+26]);
            ppg[9] = (rsp[ptr+27] << 16) | (rsp[ptr+28] << 8) | (rsp[ptr+29]);
            ppg[10] = (rsp[ptr+30] << 16) | (rsp[ptr+31] << 8) | (rsp[ptr+32]);
            ppg[11] = (rsp[ptr+33] << 16) | (rsp[ptr+34] << 8) | (rsp[ptr+35]);
            //printf("%d,%d,%d,%d,%d,%d,", ppg[6], ppg[7],ppg[8], ppg[9],ppg[10], ppg[11]);
#endif  // raw
#endif  // MAXM86146_CFG
            accel[0] = (rsp[1+PPG_SZ+0] << 8) | (rsp[1+PPG_SZ+1]);
            accel[1] = (rsp[1+PPG_SZ+2] << 8) | (rsp[1+PPG_SZ+3]);
            accel[2] = (rsp[1+PPG_SZ+4] << 8) | (rsp[1+PPG_SZ+5]);
            
            
            //printf("%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n\r", accel[0], accel[1], accel[2], ppg[0], ppg[1], ppg[2], ppg[3], ppg[4], ppg[5], ppg[6]
            //, ppg[7], ppg[8], ppg[9], ppg[10], ppg[11]); 
//**************************** DATI RAW (STAMPATI IN MODALITà RAW E EXTENDED_ALGO)
            //printf("%d,%d,%d,     %d,%d,%d,%d,%d,%d,", accel[0], accel[1], accel[2], ppg[0], ppg[1], ppg[2], ppg[3], ppg[4], ppg[5]); 
            //printf("        %d,%d,%d,%d,%d,%d,       ", ppg[6], ppg[7], ppg[8], ppg[9], ppg[10], ppg[11]); 
           
            ptr = sptr + SENSOR_SZ;

#endif  //!defined(ALGO_ONLY)

#ifndef RAW
#ifdef EXTENDED_ALGO
            opmode = rsp[ptr];
            hr =  (rsp[ptr+1] << 8) + rsp[ptr+2];
            hr_conf = rsp[ptr+3];
            ibi = (rsp[ptr+4] << 8) + rsp[ptr+5];

            ibi_conf = rsp[ptr+6];
            act = rsp[ptr+7];
            walk_stp = (rsp[ptr+8] << 24) + (rsp[ptr+9] << 16) + (rsp[ptr+10] << 8) + rsp[ptr+11];
            run_stp =  (rsp[ptr+12] << 24) + (rsp[ptr+13] << 16) + (rsp[ptr+14] << 8) + rsp[ptr+15];

            energy =   (rsp[ptr+16] << 24) + (rsp[ptr+17] << 16) + (rsp[ptr+18] << 8) + rsp[ptr+19];
            amr =      (rsp[ptr+20] << 24) + (rsp[ptr+21] << 16) + (rsp[ptr+22] << 8) + rsp[ptr+23];
            iadj1_rqt = rsp[ptr+24];
            iadj1 = (rsp[ptr+25] << 8) + rsp[ptr+26];

            iadj2_rqt = rsp[ptr+27];
            iadj2 = (rsp[ptr+28] << 8) + rsp[ptr+29];
            iadj3_rqt = rsp[ptr+30];
            iadj3 = (rsp[ptr+31] << 8) + rsp[ptr+32];

            intadj_rqt = rsp[ptr+33];
            intadj = rsp[ptr+34];
            smpladj_rqt = rsp[ptr+35];
            smpladj = rsp[ptr+36];

            rqt_smplave = rsp[ptr+37];
            afestatehr = rsp[ptr+38];
            hr_motion = rsp[ptr+39];
            scd = rsp[ptr+40];


            r = (rsp[ptr+41] << 8) + rsp[ptr+42];
            spo2_conf = rsp[ptr+43];

            spo2 = (rsp[ptr+44] << 8) + rsp[ptr+45];
            spo2_compl = rsp[ptr+46];
            spo2_lo = rsp[ptr+47];
            spo2_mo = rsp[ptr+48];

            spo2_lopi = rsp[ptr+49];
            spo2_unrel = rsp[ptr+50];
            spo2_state = rsp[ptr+51];

            ibi_offset = rsp[ptr+52];

            sptr += (TTL_SZ);

#if 1
            if (heading_printed == 0) {
                heading_printed = 1;
#if (ALGO_ONLY)
                printf("opmode,hr,hr_conf,ibi,ibi_conf,act,walk_stp,run_stp,energy,amr,");
                printf("iadj1_rqt,iadj1,iadj2_rqt,iadj2,iadj3_rqt,iadj3,intadj_rqt,intadj,");
                printf("smpladj_rqt,smpladj,rqt_smplave,afestatehr,hr_motion,scd,");
                printf("spo2,spo2_compl,spo2_lo,spo2_mo,spo2_lopi,spo2_unrel,spo2_state,ibi_offset,\n\r");
#endif
            }
// *********************** DATI ALGORITMO EXTENDED MODE ********************************************
            //printf("%d,%d,%d,%d,", opmode, hr, hr_conf, ibi);
            //printf("%d,%d,", ibi_conf, act);
            printf("%d,%d,", hr, hr_conf);
            //printf("%d,%d,%d,%d,", spo2, spo2_conf, spo2_lo, spo2_mo);
            //printf("%d,%d,%d,", spo2_lopi,spo2_unrel, spo2_state);
            printf("%d,", act);
            printf("%d,%d,", walk_stp, run_stp);
            //printf("%d,%d,%d,%d,", amr, iadj1_rqt, iadj1, iadj2_rqt);
            //printf("%d,%d,%d,%d,", iadj2, iadj3_rqt, iadj3, intadj_rqt);
            //printf("%d,%d,%d,%d,", intadj, smpladj_rqt, smpladj, rqt_smplave);
            //printf("%d,%d,", afestatehr, hr_motion);
            printf("%d,%d,",  scd, r);
            printf("%d,%d,%d", accel[0], accel[1], accel[2]);
            //printf("%d,%d,%d", spo2, spo2_conf,spo2_lo);
            //printf("%d,", ibi_offset);
#else
            if (heading_printed == 0) {
                heading_printed = 1;
#if (ALGO_ONLY)
                printf("hr,hr_conf,spo2,spo2_conf,spo2_lo,spo2_unrel,scd,\n\r");
#endif
            }
            printf("%d,%d,", hr, hr_conf);
            printf("%d,%d,", spo2, spo2_conf);
            printf("%d,%d,%d,", spo2_lo, spo2_mo, spo2_lopi);
            printf("%d,%d,", spo2_unrel, scd);
#endif  // if 1
#elif defined(PACKED_NORMAL_ALGO)
//            printf("ptr %d ttlsiz %d ", ptr, TTL_SZ);
           opmode = rsp[ptr];
            hr =  (rsp[ptr+1] << 8) + rsp[ptr+2];
            hr_conf = rsp[ptr+3];
            ibi = (rsp[ptr+4] << 8) + rsp[ptr+5];

            ibi_conf = rsp[ptr+6];
//            act = rsp[ptr+7];
            r = (rsp[ptr+7] << 8) + rsp[ptr+8];
            spo2_conf = rsp[ptr+9];

            spo2 = (rsp[ptr+10] << 8) + rsp[ptr+11];
            spo2_compl = rsp[ptr+12];

            spo2_lo = 0; spo2_mo = 0; spo2_lopi = 0; spo2_unrel = 0;

            if (rsp[ptr+13] & 0x01)
                spo2_lo = 1;
            if (rsp[ptr+13] & 0x02)
                spo2_mo = 1;
            if (rsp[ptr+13] & 0x04)
                spo2_lopi = 1;
            if (rsp[ptr+13] & 0x08)
                spo2_unrel = 1;
            spo2_state = (0x30 & rsp[ptr+13]) >> 4;

            scd = (0x03 & rsp[ptr+14]);
            act = (0x1C & rsp[ptr+14]) >> 2;
            inappro_ori = (0x20 & rsp[ptr+14]) >> 5;

            ibi_offset = rsp[ptr+15];

            sptr += (TTL_SZ);
#if defined(PCK_CFG_MASK)  // hex out, for printing config masked out packed data
            // print as hex bytes
            for (j=0; j<ALGO_SZ; j++)
                printf("%02X ", rsp[ptr+j]);
            //printf("\n\r" );
#endif

#if !defined(PCK_CFG_MASK)
  #if 1
    #if 1 // print all
            if (heading_printed == 0) {
                heading_printed = 1;
      #if (ALGO_ONLY)
                printf("opmode,hr,hr_conf,ibi,ibi_conf,act,r,spo2_conf,spo2,spo2_compl,spo2_lo,spo2_mo,spo2_lopi,spo2_unrel,spo2_state,scd,ibioffset,inappro_ori,\n\r");
      #endif
            }
            printf("%d,%d,%d,%d,", opmode, hr, hr_conf, ibi);
            printf("%d,%d,%d,%d,", ibi_conf, act, r, spo2_conf);
            printf("%d,%d,%d,%d,", spo2, spo2_compl, spo2_lo, spo2_mo);
            printf("%d,%d,%d,%d,", spo2_lopi,spo2_unrel, spo2_state, scd);
            printf("%d,", ibi_offset);
            printf("%d,", inappro_ori);
     #endif       
  #else  // print some
            if (heading_printed == 0) {
                heading_printed = 1;
     #if (ALGO_ONLY)
                printf("hr,hr_conf,spo2,spo2_conf,spo2_lo,spo2_unrel,scd,\n\r");
     #endif
            }
            printf("%d,%d,", hr, hr_conf);
            printf("%d,%d,", spo2, spo2_conf);
            printf("%d,", spo2_lo);
            printf("%d,%d,", spo2_unrel, scd);
  #endif
#endif // #if defined(PCK_CFG_MASK)

#else // normal algo size
//            printf("ptr %d ttlsiz %d ", ptr, TTL_SZ);
            opmode = rsp[ptr];
            hr =  (rsp[ptr+1] << 8) + rsp[ptr+2];
            hr_conf = rsp[ptr+3];
            ibi = (rsp[ptr+4] << 8) + rsp[ptr+5];

            ibi_conf = rsp[ptr+6];
            act = rsp[ptr+7];
            r = (rsp[ptr+8] << 8) + rsp[ptr+9];
            spo2_conf = rsp[ptr+10];

            spo2 = (rsp[ptr+11] << 8) + rsp[ptr+12];
            spo2_compl = rsp[ptr+13];
            spo2_lo = rsp[ptr+14];
            spo2_mo = rsp[ptr+15];

            spo2_lopi = rsp[ptr+16];
            spo2_unrel = rsp[ptr+17];
            spo2_state = rsp[ptr+18];
            scd = rsp[ptr+19];

//            ibi_offset = rsp[ptr+20];

            sptr += (TTL_SZ);
  
#if 0
            if (heading_printed == 0) {
                heading_printed = 1;
#if (ALGO_ONLY)
                printf("opmode,hr,hr_conf,ibi,ibi_conf,act,r,spo2_conf,spo2,spo2_compl,spo2_lo,spo2_mo,spo2_lopi,spo2_unrel,spo2_state,scd,ibioffset,\n\r");
#endif
            }
            printf("%d,%d,%d,%d,", opmode, hr, hr_conf, ibi);
            printf("%d,%d,%d,%d,", ibi_conf, act, r, spo2_conf);
            printf("%d,%d,%d,%d,", spo2, spo2_compl, spo2_lo, spo2_mo);
            printf("%d,%d,%d,%d,", spo2_lopi,spo2_unrel, spo2_state, scd);
//            printf("%d,", ibi_offset);
#else
            if (heading_printed == 0) {
                heading_printed = 1;
#if (ALGO_ONLY)
                printf("hr,hr_conf,spo2,spo2_conf,spo2_lo,spo2_unrel,scd,\n\r");
#endif
            }
            printf("%d,%d,", hr/10, hr_conf);
            printf("%d,%d,", spo2/10, spo2_conf);
            printf("%d,", spo2_lo);
            printf("%d,%d,", spo2_unrel, scd);
#endif
#endif  // end normal algo size
#endif // !RAW


            printf("\n\r");

        }
}

#ifdef MAXREFDES103_CFG
/*****************************************************************************/
// init_max20303_pmic
/*****************************************************************************/
void init_max20303_pmic(void) {
    /* Wait for pmic to settle down */
    thread_sleep_for(800);

    //set_time(1544787300);  // Set RTC time to Wed, 28 Oct 2009 11:35:37
    MAX20303 max20303(&sh_i2c);
    /*Set LDO1 to 1.8v*/
    max20303.LDO1Config();

    /*Set LDO2 to 3v*/
    max20303.LDO2Config();

    //max20303.BoostEnable();
    max20303.BuckBoostEnable();

    max20303.led0on(0);
    max20303.led1on(0);
    max20303.led2on(0);

    /* Wait for pmic to settle down */
    thread_sleep_for(200);

}
#endif // MAXREFDES103_CFG

/*****************************************************************************/
// init_sh_algo
/*****************************************************************************/
void init_sh_algo(void) {
    char cmd[64];
    char rsp[256];

#ifdef OPTIMIZE_FIFO_READ
    check_fifo_countdown = MAX_FIFO_CNT;
#endif

    // switch to application mode
    rst = 0;
    mfio = 1;
    thread_sleep_for(10);
    rst = 1;
    thread_sleep_for(1500);
#ifdef MAXREFDES103_CFG
    init_max20303_pmic();
#endif
#if 0 // 1.5s is ok for 33.13.12
  thread_sleep_for(250);
#endif

    mfio = 0; wait_us(300);
#if 0
//write SpO2 coefficients 0x00000000 FFD7FBDD 00AB61FE
    cmd[0] = 0x50; cmd[1] = 0x07; cmd[2] = 0x00; 
    cmd[3] = 0x00; cmd[4] = 0x00; cmd[5] = 0x00; cmd[6] = 0x00;
    cmd[7] = 0xFF; cmd[8] = 0xD7; cmd[9] = 0xFB; cmd[10] = 0xDD;
    cmd[11] = 0x00; cmd[12] = 0xAB; cmd[13] = 0x61; cmd[14] = 0xFE;
    sh_i2c.write(SH_ADDR, cmd, 15);
    mfio = 1; thread_sleep_for(2); mfio = 0; wait_us(300);
    sh_i2c.read(SH_ADDR, rsp, 1);
    mfio = 1; mfio = 0; wait_us(300);
    printf("Wr Spo2 Coeff %x\n\r", rsp[0]);
#endif
//1.1 rd SpO2 Coefficients 
    cmd[0] = 0x51; cmd[1] = 0x07; cmd[2] = 0x00;
    sh_i2c.write(SH_ADDR, cmd, 3);
    mfio = 1; thread_sleep_for(2); mfio = 0; wait_us(300);
    sh_i2c.read(SH_ADDR, rsp, 13);
    mfio = 1; mfio = 0; wait_us(300);
    printf("%x SpO2 Coeff %2X%2X%2X%2X  %2X%2X%2X%2X %2X%2X%2X%2X\n\r", rsp[0], rsp[1], rsp[2], rsp[3], rsp[4], rsp[5], rsp[6], rsp[7], rsp[8], rsp[9], rsp[10], rsp[11], rsp[12]);
#ifdef REDUCE_RPT_PERIOD
//Change report period to 25
    cmd[0] = 0x10; cmd[1] = 0x02; cmd[2] = REDUCE_RPT_PERIOD; 
    sh_i2c.write(SH_ADDR, cmd, 3);
    mfio = 1; thread_sleep_for(2); mfio = 0; wait_us(300);
    sh_i2c.read(SH_ADDR, rsp, 1);
    mfio = 1; mfio = 0; wait_us(300);
    printf("Wr report period 25 %x\n\r", rsp[0]);
#endif
//1.9 rd ver
    cmd[0] = 0xFF; cmd[1] = 0x03;
    sh_i2c.write(SH_ADDR, cmd, 2);
    mfio = 1; thread_sleep_for(2); mfio = 0; wait_us(300);
    sh_i2c.read(SH_ADDR, rsp, 4);
    mfio = 1; mfio = 0; wait_us(300);
    printf("Ver: %d %d %d %d\n\r", rsp[0], rsp[1], rsp[2], rsp[3]);

// wr config mask for packed normal data output fifo
#if defined(PCK_CFG_MASK)
    cmd[0] = 0x50; cmd[1] = 0x07; cmd[2] = 0x20; cmd[3] = 0x7F; cmd[4] = 0xFF; cmd[5] = 0x00;
//    cmd[0] = 0x50; cmd[1] = 0x07; cmd[2] = 0x20; cmd[3] = 0xFF; cmd[4] = 0xFF; cmd[5] = 0x00;
    sh_i2c.write(SH_ADDR, cmd, 6);
    mfio = 1; thread_sleep_for(2); mfio = 0; wait_us(300);
    sh_i2c.read(SH_ADDR, rsp, 1);
    mfio = 1; mfio = 0; wait_us(300);
    printf("wr config mask packed: %x \n\r", rsp[0]);
// rd config mask for packed normal data output fifo
    cmd[0] = 0x51; cmd[1] = 0x07; cmd[2] = 0x20;
    sh_i2c.write(SH_ADDR, cmd, 3);
    mfio = 1; thread_sleep_for(2); mfio = 0; wait_us(300);
    sh_i2c.read(SH_ADDR, rsp, 4);
    mfio = 1; mfio = 0; wait_us(300);
    printf("rd config mask for packed normal data output fifo %d %X %X %X \n\r", rsp[0], rsp[1], rsp[2], rsp[3]);
#endif

#if defined(MAXM86161_CFG) || defined(MAXM86146_CFG)
// rd algo sz normal
    cmd[0] = 0x11; cmd[1] = 0x06; cmd[2] = 0x01;
    sh_i2c.write(SH_ADDR, cmd, 3);
    mfio = 1; thread_sleep_for(2); mfio = 0; wait_us(300);
    sh_i2c.read(SH_ADDR, rsp, 2);
    mfio = 1; mfio = 0; wait_us(300);
    printf("rd algo size: %d %d \n\r", rsp[0], rsp[1]);
// rd algo sz extended
    cmd[0] = 0x11; cmd[1] = 0x06; cmd[2] = 0x02;
    sh_i2c.write(SH_ADDR, cmd, 3);
    mfio = 1; thread_sleep_for(2); mfio = 0; wait_us(300);
    sh_i2c.read(SH_ADDR, rsp, 2);
    mfio = 1; mfio = 0; wait_us(300);
    printf("rd algo size: %d %d \n\r", rsp[0], rsp[1]);
// rd algo sz scd
    cmd[0] = 0x11; cmd[1] = 0x06; cmd[2] = 0x03;
    sh_i2c.write(SH_ADDR, cmd, 3);
    mfio = 1; thread_sleep_for(2); mfio = 0; wait_us(300);
    sh_i2c.read(SH_ADDR, rsp, 2);
    mfio = 1; mfio = 0; wait_us(300);
    printf("rd algo size: %d %d \n\r", rsp[0], rsp[1]);
#if defined(MAXM86161_CFG) || defined(MAXM86146_CFG)
// rd algo sz packed normal
    cmd[0] = 0x11; cmd[1] = 0x06; cmd[2] = 0x04;
    sh_i2c.write(SH_ADDR, cmd, 3);
    mfio = 1; thread_sleep_for(2); mfio = 0; wait_us(300);
    sh_i2c.read(SH_ADDR, rsp, 2);
    mfio = 1; mfio = 0; wait_us(300);
    printf("rd algo size: %d %d \n\r", rsp[0], rsp[1]);
#endif
#endif

#if 0
// set Perfusion Index threshold to .05 (3x.13.19+)
    cmd[0] = 0x50; cmd[1] = 0x07; cmd[2] = 0x1D; cmd[3] = 50;
    sh_i2c.write(SH_ADDR, cmd, 4);
    mfio = 1; thread_sleep_for(2); mfio = 0; wait_us(300);
    sh_i2c.read(SH_ADDR, rsp, 1);
    mfio = 1; mfio = 0; wait_us(300);
    printf("pi set to 0x19 : %x \n\r", rsp[0]);
#endif

// 1.2 sensor and algo data
    cmd[0] = 0x10; cmd[1] = 0x00;
#if ALGO_ONLY
    cmd[2] = 0x02;  // algo data
    printf("algo only \n\r");
#else
    cmd[2] = 0x03;  // sensor + algo data
    printf("sens+algo \n\r");
#endif
    sh_i2c.write(SH_ADDR, cmd, 3);
    mfio = 1; thread_sleep_for(2); mfio = 0; wait_us(300);
    sh_i2c.read(SH_ADDR, rsp, 1);
    mfio = 1; mfio = 0; wait_us(300);
    printf("1.2 Status: %x\n\r", rsp[0]);
// 1.5 cont hr, spo2
    cmd[0] = 0x50; cmd[1] = 0x07; cmd[2] = 0x0A; cmd[3] = 0x00;
    sh_i2c.write(SH_ADDR, cmd, 4);
    mfio = 1; thread_sleep_for(2); mfio = 0; wait_us(300);
    sh_i2c.read(SH_ADDR, rsp, 1);
    mfio = 1; mfio = 0; wait_us(300);
    printf("1.5 Status: %x\n\r", rsp[0]);
// 1.6 AEC enable (default)
    cmd[0] = 0x50; cmd[1] = 0x07; cmd[2] = 0x0B; cmd[3] = 0x01;
    sh_i2c.write(SH_ADDR, cmd, 4);
    mfio = 1; thread_sleep_for(2); mfio = 0; wait_us(300);
    sh_i2c.read(SH_ADDR, rsp, 1);
    mfio = 1; mfio = 0; wait_us(300);
    printf("1.6 Status: %x\n\r", rsp[0]);
#if AGC
// AGC1.7 Disable auto PD (default
    cmd[0] = 0x50; cmd[1] = 0x07; cmd[2] = 0x12; cmd[3] = 0x00;
    sh_i2c.write(SH_ADDR, cmd, 4);
    mfio = 1; thread_sleep_for(2); mfio = 0; wait_us(300);
    sh_i2c.read(SH_ADDR, rsp, 1);
    mfio = 1; mfio = 0; wait_us(300);
    printf("AGC1.7 Status: %x\n\r", rsp[0]);
// AGC1.8 Disable SCD (default)
    cmd[0] = 0x50; cmd[1] = 0x07; cmd[2] = 0x0C; cmd[3] = 0x00;
    sh_i2c.write(SH_ADDR, cmd, 4);
    mfio = 1; thread_sleep_for(2); mfio = 0; wait_us(300);
    sh_i2c.read(SH_ADDR, rsp, 1);
    mfio = 1; mfio = 0; wait_us(300);
    printf("AGC1.8 Status: %x\n\r", rsp[0]);
// AGC1.9 Set AGC target PD TO 10uA
    cmd[0] = 0x50; cmd[1] = 0x07; cmd[2] = 0x11; cmd[3] = 0x00; cmd[3] = 0x64;
    sh_i2c.write(SH_ADDR, cmd, 5);
    mfio = 1; thread_sleep_for(2); mfio = 0; wait_us(300);
    sh_i2c.read(SH_ADDR, rsp, 1);
    mfio = 1; mfio = 0; wait_us(300);
    printf("AGC1.9 Status: %x\n\r", rsp[0]);
#else
// 1.7 auto PD (default
    cmd[0] = 0x50; cmd[1] = 0x07; cmd[2] = 0x12; cmd[3] = 0x01;
    sh_i2c.write(SH_ADDR, cmd, 4);
    mfio = 1; thread_sleep_for(2); mfio = 0; wait_us(300);
    sh_i2c.read(SH_ADDR, rsp, 1);
    mfio = 1; mfio = 0; wait_us(300);
    printf("1.7 Status: %x\n\r", rsp[0]);
// 1.8 SCD (default)
    cmd[0] = 0x50; cmd[1] = 0x07; cmd[2] = 0x0C; cmd[3] = 0x01;
    sh_i2c.write(SH_ADDR, cmd, 4);
    mfio = 1; thread_sleep_for(2); mfio = 0; wait_us(300);
    sh_i2c.read(SH_ADDR, rsp, 1);
    mfio = 1; mfio = 0; wait_us(300);
    printf("1.8 Status: %x\n\r", rsp[0]);
#endif

#if defined(MAXM86161_CFG) //only use Red and IR
//1.20   map leds to slots 
    cmd[0] = 0x50; cmd[1] = 0x07; cmd[2] = 0x19; cmd[3] = 0x23; cmd[4] = 0x00; cmd[5] = 0x00;
    sh_i2c.write(SH_ADDR, cmd, 6);
    mfio = 1; thread_sleep_for(2); mfio = 0; wait_us(300);
    sh_i2c.read(SH_ADDR, rsp, 1);
    mfio = 1; mfio = 0; wait_us(300);
    printf("map leds to slots %x\n\r", rsp[0]);
//1.21  map HR inputs to slots, default is 0x0073 for MAXM86161
#if 0 // NA for MAXM86161
    cmd[0] = 0x50; cmd[1] = 0x07; cmd[2] = 0x17; cmd[3] = 0x00; cmd[4] = 0x73;
    sh_i2c.write(SH_ADDR, cmd, 5);
    mfio = 1; thread_sleep_for(2); mfio = 0; wait_us(300);
    sh_i2c.read(SH_ADDR, rsp, 1);
    mfio = 1; mfio = 0; wait_us(300);
    printf("map HR to slots/PDs %x\n\r", rsp[0]);
#endif
//1.22  map SpO2 inputs to slots
    cmd[0] = 0x50; cmd[1] = 0x07; cmd[2] = 0x18; cmd[3] = 0x00; cmd[4] = 0x10;
    sh_i2c.write(SH_ADDR, cmd, 5);
    mfio = 1; thread_sleep_for(2); mfio = 0; wait_us(300);
    sh_i2c.read(SH_ADDR, rsp, 1);
    mfio = 1; mfio = 0; wait_us(300);
    printf("map SpO2 to slots/PDs %x\n\r", rsp[0]);
#endif

#ifdef MAXM86146_CFG
//1.20   map leds to slots for MAXM86146
    cmd[0] = 0x50; cmd[1] = 0x07; cmd[2] = 0x19; cmd[3] = 0x12; cmd[4] = 0x34; cmd[5] = 0x56;
    sh_i2c.write(SH_ADDR, cmd, 6);
    mfio = 1; thread_sleep_for(2); mfio = 0; wait_us(300);
    sh_i2c.read(SH_ADDR, rsp, 1);
    mfio = 1; mfio = 0; wait_us(300);
    printf("map leds to slots %x\n\r", rsp[0]);
//1.21  map HR inputs to slots
    cmd[0] = 0x50; cmd[1] = 0x07; cmd[2] = 0x17; cmd[3] = 0x10; cmd[4] = 0x20;
    sh_i2c.write(SH_ADDR, cmd, 5);
    mfio = 1; thread_sleep_for(2); mfio = 0; wait_us(300);
    sh_i2c.read(SH_ADDR, rsp, 1);
    mfio = 1; mfio = 0; wait_us(300);
    printf("map HR to slots/PDs %x\n\r", rsp[0]);
//1.22  map SpO2 inputs to slots
    cmd[0] = 0x50; cmd[1] = 0x07; cmd[2] = 0x18; cmd[3] = 0x40; cmd[4] = 0x50;
    sh_i2c.write(SH_ADDR, cmd, 5);
    mfio = 1; thread_sleep_for(2); mfio = 0; wait_us(300);
    sh_i2c.read(SH_ADDR, rsp, 1);
    mfio = 1; mfio = 0; wait_us(300);
    printf("map SpO2 to slots/PDs %x\n\r", rsp[0]);

#if 0
//1.20 Sec 4.1  map leds to slots for MAXM86146
    cmd[0] = 0x50; cmd[1] = 0x07; cmd[2] = 0x19; cmd[3] = 0x23; cmd[4] = 0x53; cmd[5] = 0x00;
    sh_i2c.write(SH_ADDR, cmd, 6);
    mfio = 1; thread_sleep_for(2); mfio = 0; wait_us(300);
    sh_i2c.read(SH_ADDR, rsp, 1);
    mfio = 1; mfio = 0; wait_us(300);
    printf("map leds to slots%x\n\r", rsp[0]);
//1.21  map HR inputs to slots
    cmd[0] = 0x50; cmd[1] = 0x07; cmd[2] = 0x17; cmd[3] = 0x00; cmd[4] = 0x10;
    sh_i2c.write(SH_ADDR, cmd, 5);
    mfio = 1; thread_sleep_for(2); mfio = 0; wait_us(300);
    sh_i2c.read(SH_ADDR, rsp, 1);
    mfio = 1; mfio = 0; wait_us(300);
    printf("map HR to slots/PDs %x\n\r", rsp[0]);
//1.22  map SpO2 inputs to slots
    cmd[0] = 0x50; cmd[1] = 0x07; cmd[2] = 0x18; cmd[3] = 0x10; cmd[4] = 0x20;
    sh_i2c.write(SH_ADDR, cmd, 5);
    mfio = 1; thread_sleep_for(2); mfio = 0; wait_us(300);
    sh_i2c.read(SH_ADDR, rsp, 1);
    mfio = 1; mfio = 0; wait_us(300);
    printf("map SpO2 to slots/PDs %x\n\r", rsp[0]);
#endif
#endif // MAXM86146_CFG

#if 0  // MAXM86161 test new commands
// LDO enable
    cmd[0] = 0x10; cmd[1] = 0x12; cmd[2] = 0x01;
    //cmd[0] = 0x10; cmd[1] = 0x12; cmd[2] = 0x00;
    sh_i2c.write(SH_ADDR, cmd, 3);
    thread_sleep_for(2);
    sh_i2c.read(SH_ADDR, rsp, 1);
    printf("\n\r ldo en %x\n\r", rsp[0]);
    mfio = 1; thread_sleep_for(2); mfio = 0; wait_us(300);
// GPIO enable
    cmd[0] = 0x10; cmd[1] = 0x13; cmd[2] = 0x01;
    sh_i2c.write(SH_ADDR, cmd, 3);
    thread_sleep_for(2);
    sh_i2c.read(SH_ADDR, rsp, 1);
    printf("\n\r gpio en %x\n\r", rsp[0]);
    mfio = 1; thread_sleep_for(2); mfio = 0; wait_us(300);
#endif

// 1.30 Enable HR, SpO2 algo
#if EXTENDED_ALGO
    cmd[0] = 0x52; cmd[1] = 0x07; cmd[2] = 0x02;
#elif defined(PACKED_NORMAL_ALGO)
    cmd[0] = 0x52; cmd[1] = 0x07; cmd[2] = 0x04;
#else
    cmd[0] = 0x52; cmd[1] = 0x07; cmd[2] = 0x01;
#endif
    sh_i2c.write(SH_ADDR, cmd, 3);
    mfio = 1;
    thread_sleep_for(465);
    mfio = 0; wait_us(300);
    sh_i2c.read(SH_ADDR, rsp, 1);
    printf("1.14 status: %x\n\r", rsp[0]);

#if 1
//1.31 rd AFE part id
    cmd[0] = 0x41; cmd[1] = 0x00; cmd[2] = 0xFF;
    sh_i2c.write(SH_ADDR, cmd, 3);
    mfio = 1; thread_sleep_for(2); mfio = 0; wait_us(300);
    sh_i2c.read(SH_ADDR, rsp, 2);
    mfio = 1; mfio = 0; wait_us(300);
    printf("1.31 part id afe %x %x\n\r", rsp[0], rsp[1]);
//1.32 rd accel who
    cmd[0] = 0x41; cmd[1] = 0x04; cmd[2] = 0x0F;
    sh_i2c.write(SH_ADDR, cmd, 3);
    mfio = 1; thread_sleep_for(2); mfio = 0; wait_us(300);
    sh_i2c.read(SH_ADDR, rsp, 2);
    mfio = 1; mfio = 0; wait_us(300);
    printf("1.32 who accel %x %x\n\r", rsp[0], rsp[1]);
#endif
    mfio = 1;
}

/*****************************************************************************/
// init_sh_raw
/*****************************************************************************/
void init_sh_raw(void) {
    char cmd[16];
    char rsp[256];
    // switch to application mode
    rst = 0;
    mfio = 1;
    thread_sleep_for(10);
    rst = 1;
    thread_sleep_for(1500);
#ifdef MAXREFDES103_CFG
    init_max20303_pmic();
#endif

    mfio = 0; wait_us(300);

//1.0 read operating mode
    cmd[0] = 0x02; cmd[1] = 0x00;
    sh_i2c.write(SH_ADDR, cmd, 2);
    mfio = 1; thread_sleep_for(2); mfio = 0; wait_us(300);
    sh_i2c.read(SH_ADDR, rsp, 2);
    mfio = 1; thread_sleep_for(2); mfio = 0; wait_us(300);
    printf("\n\r 0x02 0x00 Status, Read Operating Mode: %x %x\n\r", rsp[0], rsp[1]);
//1.1 rd ver
    cmd[0] = 0xFF; cmd[1] = 0x03;
    sh_i2c.write(SH_ADDR, cmd, 2);
    mfio = 1; thread_sleep_for(2); mfio = 0; wait_us(300);
    sh_i2c.read(SH_ADDR, rsp, 4);
    mfio = 1; mfio = 0; wait_us(300);
    printf("Ver: %d %d %d %d\n\r", rsp[0], rsp[1], rsp[2], rsp[3]);

// raw1.2 sensor data
    cmd[0] = 0x10; cmd[1] = 0x00; cmd[2] = 0x01;
    printf("raw sensor data only \n\r");
    sh_i2c.write(SH_ADDR, cmd, 3);
    mfio = 1; thread_sleep_for(2); mfio = 0; wait_us(300);
    sh_i2c.read(SH_ADDR, rsp, 1);
    mfio = 1; mfio = 0; wait_us(300);
    printf("raw1.2 Status: %x\n\r", rsp[0]);
// raw1.3 interrupt threshold
    cmd[0] = 0x10; cmd[1] = 0x01; cmd[2] = 0x01;
    sh_i2c.write(SH_ADDR, cmd, 3);
    mfio = 1; thread_sleep_for(2); mfio = 0; wait_us(300);
    sh_i2c.read(SH_ADDR, rsp, 1);
    mfio = 1; mfio = 0; wait_us(300);
    printf("raw1.3 Status: %x\n\r", rsp[0]);
// raw1.4 enable sh accel
    cmd[0] = 0x44; cmd[1] = 0x04; cmd[2] = 0x01; cmd[3] = 0x00;
    sh_i2c.write(SH_ADDR, cmd, 4);
    mfio = 1; thread_sleep_for(20); mfio = 0; wait_us(300);
    sh_i2c.read(SH_ADDR, rsp, 1);
    mfio = 1; thread_sleep_for(2); mfio = 0; wait_us(300);
    printf("raw1.4 Status: %x\n\r", rsp[0]);
// raw1.6 enable AFE
//    cmd[0] = 0x44; cmd[1] = 0x00; cmd[2] = 0x01; cmd[3] = 0x00; 
//    sh_i2c.write(SH_ADDR, cmd, 4);
    cmd[0] = 0x44; cmd[1] = 0x00; cmd[2] = 0x01;  // 3 bytes // tested on 33.13.31/12
    sh_i2c.write(SH_ADDR, cmd, 3);
    cmd[0] = 0x44; cmd[1] = 0xFF; cmd[2] = 0x02; cmd[3] = 0x04; cmd[4] = 0x01; cmd[5] = 0x00; cmd[6] = 0x00; cmd[7] = 0x01; cmd[8] = 0x00; 
    sh_i2c.write(SH_ADDR, cmd, 9);
    mfio = 1; thread_sleep_for(465); mfio = 0; wait_us(300);
    sh_i2c.read(SH_ADDR, rsp, 1);
    mfio = 1; thread_sleep_for(2); mfio = 0; wait_us(300);
    printf("raw1.6 Status: %x\n\r", rsp[0]);

// raw1.7 rd accel WHO reg
    cmd[0] = 0x41; cmd[1] = 0x04; cmd[2] = 0x0F;
    sh_i2c.write(SH_ADDR, cmd, 3);
    mfio = 1; thread_sleep_for(2); mfio = 0; wait_us(300);
    sh_i2c.read(SH_ADDR, rsp, 2);
    mfio = 1; thread_sleep_for(2); mfio = 0; wait_us(300);
    printf("1.7 Who Status: %x %x\n\r", rsp[0], rsp[1]);
// rd1.6  AFE part id
    cmd[0] = 0x41; cmd[1] = 0x00; cmd[2] = 0xFF;
    sh_i2c.write(SH_ADDR, cmd, 3);
    mfio = 1; thread_sleep_for(2); mfio = 0; wait_us(300);
    sh_i2c.read(SH_ADDR, rsp, 2);
    mfio = 1; thread_sleep_for(2); mfio = 0; wait_us(300);
    printf("raw1.6 part id Status: %x %x\n\r", rsp[0], rsp[1]);

// raw1.8 sample rate 100 Hz, ave 1
//    cmd[0] = 0x40; cmd[1] = 0x00; cmd[2] = 0x12; cmd[3] = 0x00;  // set AFE reg 0x12 to 25 Hz
    cmd[0] = 0x40; cmd[1] = 0x00; cmd[2] = 0x12; cmd[3] = 0x18;  // set AFE reg 0x12 to 100 Hz
//    cmd[0] = 0x40; cmd[1] = 0x00; cmd[2] = 0x12; cmd[3] = 0x20;  // set AFE reg 0x12 to 200 Hz
    sh_i2c.write(SH_ADDR, cmd, 4);
    mfio = 1; thread_sleep_for(2); mfio = 0; wait_us(300);
    sh_i2c.read(SH_ADDR, rsp, 1);
    mfio = 1; mfio = 0; wait_us(300);
    printf("raw1.8 Status: %x\n\r", rsp[0]);
// raw1.9 LED1 current
    cmd[0] = 0x40; cmd[1] = 0x00; cmd[2] = 0x23; cmd[3] = 0x3F;
    sh_i2c.write(SH_ADDR, cmd, 4);
    mfio = 1; thread_sleep_for(2); mfio = 0; wait_us(300);
    sh_i2c.read(SH_ADDR, rsp, 1);
    mfio = 1; mfio = 0; wait_us(300);
    printf("raw1.9 Status: %x\n\r", rsp[0]);
#ifndef MAXM86146_CFG
// raw1.10 LED2 current
    cmd[0] = 0x40; cmd[1] = 0x00; cmd[2] = 0x24; cmd[3] = 0x3F;
    sh_i2c.write(SH_ADDR, cmd, 4);
    mfio = 1; thread_sleep_for(2); mfio = 0; wait_us(300);
    sh_i2c.read(SH_ADDR, rsp, 1);
    mfio = 1; mfio = 0; wait_us(300);
    printf("raw1.10 Status: %x\n\r", rsp[0]);
#endif
// raw1.11 LED3 current
    cmd[0] = 0x40; cmd[1] = 0x00; cmd[2] = 0x25; cmd[3] = 0x3F;
    sh_i2c.write(SH_ADDR, cmd, 4);
    mfio = 1; thread_sleep_for(2); mfio = 0; wait_us(300);
    sh_i2c.read(SH_ADDR, rsp, 1);
    mfio = 1; mfio = 0; wait_us(300);
    printf("raw1.11 Status: %x\n\r", rsp[0]);
// raw1.11 LED4 current
    cmd[0] = 0x40; cmd[1] = 0x00; cmd[2] = 0x26; cmd[3] = 0x3F;
    sh_i2c.write(SH_ADDR, cmd, 4);
    mfio = 1; thread_sleep_for(2); mfio = 0; wait_us(300);
    sh_i2c.read(SH_ADDR, rsp, 1);
    mfio = 1; mfio = 0; wait_us(300);
    printf("raw1.11 Status: %x\n\r", rsp[0]);
#ifdef MAXM86146_CFG
//       LED5 current
    cmd[0] = 0x40; cmd[1] = 0x00; cmd[2] = 0x27; cmd[3] = 0x7F;
    sh_i2c.write(SH_ADDR, cmd, 4);
    mfio = 1; thread_sleep_for(2); mfio = 0; wait_us(300);
    sh_i2c.read(SH_ADDR, rsp, 1);
    mfio = 1; mfio = 0; wait_us(300);
    printf("led5 Status: %x\n\r", rsp[0]);
//       LED6 current
    cmd[0] = 0x40; cmd[1] = 0x00; cmd[2] = 0x28; cmd[3] = 0x7F;
    sh_i2c.write(SH_ADDR, cmd, 4);
    mfio = 1; thread_sleep_for(2); mfio = 0; wait_us(300);
    sh_i2c.read(SH_ADDR, rsp, 1);
    mfio = 1; mfio = 0; wait_us(300);
    printf("led6 Status: %x\n\r", rsp[0]);

//       Set Seq cntrl1 LED2, LED1
    cmd[0] = 0x40; cmd[1] = 0x00; cmd[2] = 0x20; cmd[3] = 0x21;
    sh_i2c.write(SH_ADDR, cmd, 4);
    mfio = 1; thread_sleep_for(2); mfio = 0; wait_us(300);
    sh_i2c.read(SH_ADDR, rsp, 1);
    mfio = 1; mfio = 0; wait_us(300);
    printf("Seq cntrl1 Status: %x\n\r", rsp[0]);
//       Set Seq cntrl2 LED4, LED3
    cmd[0] = 0x40; cmd[1] = 0x00; cmd[2] = 0x21; cmd[3] = 0xA3;
    sh_i2c.write(SH_ADDR, cmd, 4);
    mfio = 1; thread_sleep_for(2); mfio = 0; wait_us(300);
    sh_i2c.read(SH_ADDR, rsp, 1);
    mfio = 1; mfio = 0; wait_us(300);
    printf("Seq cntrl2 Status: %x\n\r", rsp[0]);
//       Set Seq cntrl3 LED6, LED5
    cmd[0] = 0x40; cmd[1] = 0x00; cmd[2] = 0x22; cmd[3] = 0xCB;
//    cmd[0] = 0x40; cmd[1] = 0x00; cmd[2] = 0x22; cmd[3] = 0x99;  // Ambient
    sh_i2c.write(SH_ADDR, cmd, 4);
    mfio = 1; thread_sleep_for(2); mfio = 0; wait_us(300);
    sh_i2c.read(SH_ADDR, rsp, 1);
    mfio = 1; mfio = 0; wait_us(300);
    printf("Seq cntrl3 Status: %x\n\r", rsp[0]);

#endif
mfio = 1;
}

/*****************************************************************************/
// main
/*****************************************************************************/
int main()
{
    sh_i2c.frequency(400000);
    int32_t ledcnt = 0;
    rLED = LED_OFF;  gLED = LED_ON;  bLED = LED_OFF;
    Ticker ticker;   // calls a callback repeatedly with a timeout

#if 0 // test disable algo
    char cmd[8];
    char rsp[256];
    // 3.1 Disable algo
    mfio = 1; mfio = 0; wait_us(300);
    cmd[0] = 0x52; cmd[1] = 0x07; cmd[2] = 0x00;
    sh_i2c.write(SH_ADDR, cmd, 3);
    thread_sleep_for(465);
    sh_i2c.read(SH_ADDR, rsp, 1);
    printf("3.1 disable algo: %x\n\r", rsp[0]);
    mfio = 1;
#endif

    //ticker.attach(callback(&blink_timer), BLINKING_RATE_MS);  /* set timer for one second */
#ifdef RAW
    init_sh_raw();
#else
    init_sh_algo();
#endif
    mfio = 1;
    heading_printed = 0;
#if defined(MAXREFDES103_CFG) || defined(MAXM86161_CFG) || defined(MAXM86146_CFG)
    Timer tmr1;
    while (1) {
        tmr1.start();
#ifdef RAW 
        if (tmr1.read_ms() >= 10) {
#else
  #ifdef REDUCE_RPT_PERIOD
    #ifdef USE_FIFO_BUFFER_CNT
        if (tmr1.read_ms() >= 40*REDUCE_RPT_PERIOD*USE_FIFO_BUFFER_CNT) {
    #else
        if (tmr1.read_ms() >= 40*REDUCE_RPT_PERIOD) {
    #endif
  #else
    #ifdef USE_FIFO_BUFFER_CNT
        if (tmr1.read_ms() >= 40*USE_FIFO_BUFFER_CNT) {
    #else
        if (tmr1.read_ms() >= 40) {
    #endif
  #endif
#endif
            tmr1.reset();
            read_sh_fifo();
            if ((ledcnt++ % 50) == 0)
                gLED = !gLED;
        }
    }
#else
    ticker.attach(callback(&fifo_timer), 0.040f);
    while (1) {
        if (Time_to_Read_PPG) {
            read_sh_fifo();
            if ((ledcnt++ % 50) == 0)
                gLED = !gLED;
        }
    }
#endif
}