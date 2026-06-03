/*
 * cmd_id.h
 *
 *  Created on: Jul 16, 2024
 *      Author: User
 */

#ifndef CMD_ID_H_
#define CMD_ID_H_


// General Section (Read Only)
//
#define C2000_Version_spi_addr                      0x400

#define Machine_Status_spi_addr                     0x0401
#define CPU2_Version_spi_addr                       0x0402
#define Vrms_MSB_spi_addr                           0x0403
#define Vrms_LSB_spi_addr                           0x0404
#define Irms_MSB_spi_addr                           0x0405
#define Irms_LSB_spi_addr                           0x0406
#define Ipeak_max_MSB_spi_addr                      0x0407
#define Ipeak_max_LSB_spi_addr                      0x0408
#define W_MSB_spi_addr                              0x0409
#define W_LSB_spi_addr                              0x040A
#define Ipeak_min_MSB_spi_addr                      0x040B
#define Ipeak_min_LSB_spi_addr                      0x040C
#define Vpeak_max_MSB_spi_addr                      0x040D
#define Vpeak_max_LSB_spi_addr                      0x040E
#define Vpeak_min_MSB_spi_addr                      0x040F
#define Vpeak_min_LSB_spi_addr                      0x0410
#define Vdc_avg_MSB_spi_addr                        0x0411
#define Vdc_avg_LSB_spi_addr                        0x0412
#define Idc_avg_MSB_spi_addr                        0x0413
#define Idc_avg_LSB_spi_addr                        0x0414
#define VA_MSB_spi_addr                             0x0415
#define VA_LSB_spi_addr                             0x0416
#define VAR_MSB_spi_addr                            0x0417
#define VAR_LSB_spi_addr                            0x0418
#define PF_spi_addr                                 0x0419
//#define Null_spi_addr                               0x041A
#define CF_spi_addr                                 0x041B
//#define Frequency_spi_addr                          0x041C
#define Ipeak_hold_MSB_spi_addr                     0x041D
#define Ipeak_hold_LSB_spi_addr                     0x041E
#define Calibration_State_spi_addr                  0x041F
//#define Machine_Series_spi_addr                     0x0420
#define Startup_State_spi_addr                      0x0421
//#define C2000_CMD_DATA_CheckSum_spi_addr            0x0422
#define Frequency_MSB_spi_addr                      0x0423
#define Frequency_LSB_spi_addr                      0x0424
#define Sequence_Step_CheckSum_spi_addr             0x0425
#define Sequence_Run_Step_spi_addr                  0x0426
#define Sequence_State_spi_addr                     0x0427
#define Sequence_Set_Step_spi_addr                  0x0428
#define C2000_Alarm_Status_spi_addr                 0x0429
#define C2000_Alarm_Status_2_spi_addr               0x042A
#define Temperature_MSB_spi_addr                    0x042B
#define Temperature_LSB_spi_addr                    0x042C
#define Vpeak_hold_MSB_spi_addr                     0x042D
#define Vpeak_hold_LSB_spi_addr                     0x042E


// ----------------------------------------------------------------------------------------
// @ Begin Calibration Data
#define Calibration_CheckSum_spi_addr               0x0430
#define Calibration_Data_Version_spi_addr           0x0431

#define ADC_to_V_FB_Scale_P_LSB_spi_addr_lr         0x0432
#define ADC_to_V_FB_Scale_P_MSB_spi_addr_lr         0x0433
#define ADC_to_V_FB_Scale_P_LSB_spi_addr_hr         0x0434
#define ADC_to_V_FB_Scale_P_MSB_spi_addr_hr         0x0435

#define ADC_to_V_FB_Scale_N_LSB_spi_addr_lr         0x0436
#define ADC_to_V_FB_Scale_N_MSB_spi_addr_lr         0x0437
#define ADC_to_V_FB_Scale_N_LSB_spi_addr_hr         0x0438
#define ADC_to_V_FB_Scale_N_MSB_spi_addr_hr         0x0439

#define ADC_to_V_FB_Offset_LSB_spi_addr_lr          0x043A
#define ADC_to_V_FB_Offset_MSB_spi_addr_lr          0x043B
#define ADC_to_V_FB_Offset_LSB_spi_addr_hr          0x043C
#define ADC_to_V_FB_Offset_MSB_spi_addr_hr          0x043D

#define ADC_to_I_FB_Scale_P_LSB_spi_addr_lr         0x043E
#define ADC_to_I_FB_Scale_P_MSB_spi_addr_lr         0x043F
#define ADC_to_I_FB_Scale_P_LSB_spi_addr_hr         0x0440
#define ADC_to_I_FB_Scale_P_MSB_spi_addr_hr         0x0441

#define ADC_to_I_FB_Scale_N_LSB_spi_addr_lr         0x0442
#define ADC_to_I_FB_Scale_N_MSB_spi_addr_lr         0x0443
#define ADC_to_I_FB_Scale_N_LSB_spi_addr_hr         0x0444
#define ADC_to_I_FB_Scale_N_MSB_spi_addr_hr         0x0445

#define ADC_to_I_FB_Offset_LSB_spi_addr_lr          0x0446
#define ADC_to_I_FB_Offset_MSB_spi_addr_lr          0x0447
#define ADC_to_I_FB_Offset_LSB_spi_addr_hr          0x0448
#define ADC_to_I_FB_Offset_MSB_spi_addr_hr          0x0449

#define Cs_mpy_360000_LSB_spi_addr_lr               0x044A
#define Cs_mpy_360000_MSB_spi_addr_lr               0x044B
#define Cs_mpy_360000_LSB_spi_addr_hr               0x044C
#define Cs_mpy_360000_MSB_spi_addr_hr               0x044D

#define inv_Rs_LSB_spi_addr_lr                      0x044E
#define inv_Rs_MSB_spi_addr_lr                      0x044F
#define inv_Rs_LSB_spi_addr_hr                      0x0450
#define inv_Rs_MSB_spi_addr_hr                      0x0451

#define VSET_to_DAC_Scale_LSB_spi_addr_lr           0x0452
#define VSET_to_DAC_Scale_MSB_spi_addr_lr           0x0453
#define VSET_to_DAC_Scale_LSB_spi_addr_hr           0x0454
#define VSET_to_DAC_Scale_MSB_spi_addr_hr           0x0455

#define VSET_to_DAC_Offset_LSB_spi_addr_lr          0x0456
#define VSET_to_DAC_Offset_MSB_spi_addr_lr          0x0457
#define VSET_to_DAC_Offset_LSB_spi_addr_hr          0x0458
#define VSET_to_DAC_Offset_MSB_spi_addr_hr          0x0459

#define Freq_Compensation_coeff_a_LSB_spi_addr_lr   0x045A
#define Freq_Compensation_coeff_a_MSB_spi_addr_lr   0x045B
#define Freq_Compensation_coeff_a_LSB_spi_addr_hr   0x045C
#define Freq_Compensation_coeff_a_MSB_spi_addr_hr   0x045D

#define Freq_Compensation_coeff_b_LSB_spi_addr_lr   0x045E
#define Freq_Compensation_coeff_b_MSB_spi_addr_lr   0x045F
#define Freq_Compensation_coeff_b_LSB_spi_addr_hr   0x0460
#define Freq_Compensation_coeff_b_MSB_spi_addr_hr   0x0461

#define Freq_Compensation_coeff_c_LSB_spi_addr_lr   0x0462
#define Freq_Compensation_coeff_c_MSB_spi_addr_lr   0x0463
#define Freq_Compensation_coeff_c_LSB_spi_addr_hr   0x0464
#define Freq_Compensation_coeff_c_MSB_spi_addr_hr   0x0465

#define V_rms_Offset_LSB_spi_addr_lr                0x0466
#define V_rms_Offset_MSB_spi_addr_lr                0x0467
#define V_rms_Offset_LSB_spi_addr_hr                0x0468
#define V_rms_Offset_MSB_spi_addr_hr                0x0469

#define I_rms_Offset_LSB_spi_addr_lr                0x046A
#define I_rms_Offset_MSB_spi_addr_lr                0x046B
#define I_rms_Offset_LSB_spi_addr_hr                0x046C
#define I_rms_Offset_MSB_spi_addr_hr                0x046D

#define ADC_to_EXTIN_L1_Scale_LSB_spi_addr_lr       0x046E
#define ADC_to_EXTIN_L1_Scale_MSB_spi_addr_lr       0x046F
#define ADC_to_EXTIN_L1_Scale_LSB_spi_addr_hr       0x0470
#define ADC_to_EXTIN_L1_Scale_MSB_spi_addr_hr       0x0471

#define ADC_to_EXTIN_L1_offset_LSB_spi_addr_lr      0x0472
#define ADC_to_EXTIN_L1_offset_MSB_spi_addr_lr      0x0473
#define ADC_to_EXTIN_L1_offset_LSB_spi_addr_hr      0x0474
#define ADC_to_EXTIN_L1_offset_MSB_spi_addr_hr      0x0475

#define p_V_peak_Offset_LSB_spi_addr_lr             0x0476
#define p_V_peak_Offset_MSB_spi_addr_lr             0x0477
#define p_V_peak_Offset_LSB_spi_addr_hr             0x0478
#define p_V_peak_Offset_MSB_spi_addr_hr             0x0479

#define n_V_peak_Offset_LSB_spi_addr_lr             0x047A
#define n_V_peak_Offset_MSB_spi_addr_lr             0x047B
#define n_V_peak_Offset_LSB_spi_addr_hr             0x047C
#define n_V_peak_Offset_MSB_spi_addr_hr             0x047D

#define p_I_peak_Offset_LSB_spi_addr_lr             0x047E
#define p_I_peak_Offset_MSB_spi_addr_lr             0x047F
#define p_I_peak_Offset_LSB_spi_addr_hr             0x0480
#define p_I_peak_Offset_MSB_spi_addr_hr             0x0481

#define n_I_peak_Offset_LSB_spi_addr_lr             0x0482
#define n_I_peak_Offset_MSB_spi_addr_lr             0x0483
#define n_I_peak_Offset_LSB_spi_addr_hr             0x0484
#define n_I_peak_Offset_MSB_spi_addr_hr             0x0485

// @ End of Calibration Data
// ----------------------------------------------------------------------------------------


// General Section (Read / Write)
// @ group: 0x0700
#define V_and_I_Raw_Data_Counter_spi_addr           0x0700
#define External_Input_spi_addr                     0x0701
#define External_Output_spi_addr                    0x0702
#define External_AD_spi_addr                        0x0703
#define Wave_Data_Address_Page_spi_addr             0x0704
#define External_Frequency_H_spi_addr               0x0705
#define External_Frequency_L_spi_addr               0x0706
#define Spi_Block_Status_spi_addr                   0x0707
#define Spi_Block_Write_Index_spi_addr              0x0708
#define Spi_Block_CheckSum_spi_addr                 0x0709
#define Spi_Block_Progress_spi_addr                 0x070A


// @ group: 0x0800
#define Update_C2000_Setting_spi_addr               0x0800
#define V_and_I_Raw_Data_Operate_spi_addr           0x0801
#define RD_Mode_IO_Control_spi_addr                 0x0802
#define Print_All_Cali_Variable_spi_addr            0x0803


// Control Section
// @ group: 0x0900 & 0x0A00
#define Output_ON_OFF_spi_addr                      0x0900
#define Fan_Speed_spi_addr                          0x0901
#define Output_Waveform_Set_spi_addr                0x0902
#define Output_Mode_Set_spi_addr                    0x0903
#define Output_Range_Set_spi_addr                   0x0904
#define Ipeak_hold_Clear_spi_addr                   0x0905
#define T_Ipeak_hold_spi_addr                       0x0906
#define I_RMS_Limiter_ON_OFF_spi_addr               0x0907
#define I_Peak_Limiter_ON_OFF_spi_addr               0x0908
#define Frequency_Set_MSB_spi_addr                  0x0909
#define Frequency_Set_LSB_spi_addr                  0x090A
#define Vpeak_hold_Clear_spi_addr                   0x090B
#define T_Vpeak_hold_spi_addr                       0x090C
#define External_IO_Control_ON_OFF_spi_addr         0x090D
#define Zero_Change_Phase_ON_OFF_spi_addr           0x090E
#define Start_Phase_Set_spi_addr                    0x090F
#define Start_Phase_ON_OFF_spi_addr                 0x0910
#define Stop_Phase_Set_spi_addr                     0x0911
#define Stop_Phase_ON_OFF_spi_addr                  0x0912
#define Output_relay_Enable_Disable_spi_addr        0x0913
#define V_Measure_Compensation_ON_OFF_spi_addr      0x0914
#define V_Slew_Rate_spi_addr                        0x0915
#define Frequency_Compensation_ON_OFF_spi_addr      0x0916
#define V_Slew_Rate_mode_spi_addr                   0x0917
#define SYNC_Frequency_Source_spi_addr              0x0918
#define Remote_Sense_ON_OFF_spi_addr                0x0919
#define CPU_State_spi_addr                          0x091A

#define I_RMS_Limiter_Set_MSB_spi_addr              0x0920
#define I_RMS_Limiter_Set_LSB_spi_addr              0x0921
#define p_I_Peak_Limiter_Set_MSB_spi_addr           0x0922
#define p_I_Peak_Limiter_Set_LSB_spi_addr           0x0923
#define n_I_Peak_Limiter_Set_MSB_spi_addr           0x0924
#define n_I_Peak_Limiter_Set_LSB_spi_addr           0x0925
#define Ext_Gain_Set_MSB_spi_addr                   0x0926
#define Ext_Gain_Set_LSB_spi_addr                   0x0927
#define DC_Voltage_Set_MSB_spi_addr                 0x0928
#define DC_Voltage_Set_LSB_spi_addr                 0x0929
#define AC_Voltage_Set_MSB_spi_addr                 0x092A
#define AC_Voltage_Set_LSB_spi_addr                 0x092B
#define VCA_Gain_Set_MSB_spi_addr                   0x092C
#define VCA_Gain_Set_LSB_spi_addr                   0x092D
#define I_RMS_Trip_Set_MSB_spi_addr                 0x092E
#define I_RMS_Trip_Set_LSB_spi_addr                 0x092F
#define p_I_Peak_Trip_Set_MSB_spi_addr              0x0930
#define p_I_Peak_Trip_Set_LSB_spi_addr              0x0931
#define n_I_Peak_Trip_Set_MSB_spi_addr              0x0932
#define n_I_Peak_Trip_Set_LSB_spi_addr              0x0933
#define V_RMS_High_Trip_Set_MSB_spi_addr            0x0934
#define V_RMS_High_Trip_Set_LSB_spi_addr            0x0935
#define V_RMS_Low_Trip_Set_MSB_spi_addr             0x0936
#define V_RMS_Low_Trip_Set_LSB_spi_addr             0x0937
#define Freq_High_Trip_Set_MSB_spi_addr             0x0938
#define Freq_High_Trip_Set_LSB_spi_addr             0x0939
#define Freq_Low_Trip_Set_MSB_spi_addr              0x093A
#define Freq_Low_Trip_Set_LSB_spi_addr              0x093B
#define Phase_Set_spi_addr                          0x093C
#define p_V_Peak_Trip_Set_MSB_spi_addr              0x093D
#define p_V_Peak_Trip_Set_LSB_spi_addr              0x093E
#define n_V_Peak_Trip_Set_MSB_spi_addr              0x093F
#define n_V_Peak_Trip_Set_LSB_spi_addr              0x0940

// Block RAM streaming window
// 0x3000..0x3FFE carries up to 4095 points. 0x3FFF marks the RAM page ready
// for a later background extflash commit.
#define Spi_Block_Data_Base_spi_addr                0x3000
#define Spi_Block_Data_Last_spi_addr                0x3FFE
#define Spi_Block_End_spi_addr                      0x3FFF


#endif /* CMD_ID_H_ */
